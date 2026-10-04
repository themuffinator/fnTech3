#!/usr/bin/env python3
"""Probe fnTech3's Academy MP UI or an explicit loopback server on Windows.

Use a stable Windows x86 fntech3.exe built with -Djka-development=true. The
installation supplies read-only paks; all writable data is isolated below the
project .tmp directory. The client runs on an unswitched private desktop,
windowed at 800x600, with mouse and joystick disabled. No input is injected.
Captures use the engine's screenshotPNG command. A bounded failure retains its
named module frontier and logs; a successful capture still needs visual review.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import math
import os
import re
import shutil
import stat
import struct
import subprocess
import time
import zipfile

from probe_jka_peer import ROOT, WindowsPrivateProcess, command, inventory, read, sha

CONFIG = 'jka-client-probe.cfg'
MARKER = 'FNTECH3_JKA_CLIENT_'
ERRORS = re.compile(r'error:|sys_error|fatal crashed:|unsupported .*(?:cgame|ui|jka)|'
    r'jka[^\n]*(?:unsupported|not implemented)|not served yet|recursive .*shutdown|'
    r'illegible|read past end|lost reliable commands|server command overflow', re.I)


def validate(args):
    if args.mode == 'connect' and not args.port:
        raise ValueError('--mode connect requires an explicit --port')
    if args.mode == 'startup-ui' and args.port is not None:
        raise ValueError('--port only applies to --mode connect')
    if args.port is not None and not 1 <= args.port <= 65535:
        raise ValueError('--port must be between 1 and 65535')
    if not 1 <= args.frames <= 2400:
        raise ValueError('--frames must be between 1 and 2400')
    if not math.isfinite(args.timeout) or not 1 <= args.timeout <= 600:
        raise ValueError('--timeout must be finite and between 1 and 600 seconds')
    executable = args.executable.resolve(strict=True)
    if executable.name.casefold() != 'fntech3.exe' or not executable.is_relative_to(ROOT):
        raise ValueError('Use a stable project-local fntech3.exe; do not copy or rename it for a probe')
    raw = executable.read_bytes()
    if len(raw) < 64 or raw[:2] != b'MZ':
        raise ValueError('The JKA native client requires a Windows x86 PE executable')
    offset = struct.unpack_from('<I', raw, 60)[0]
    if offset > len(raw)-6 or raw[offset:offset+4] != b'PE\0\0' or struct.unpack_from('<H', raw, offset+4)[0] != 0x14c:
        raise ValueError('The JKA retail native modules require a Windows x86 executable')
    installation = args.installation.resolve(strict=True)
    if not (installation/'jamp.exe').is_file() or not (installation/'base/assets0.pk3').is_file():
        raise ValueError('Select the legitimate Jedi Academy installation containing jamp.exe and base/assets0.pk3')
    output = Path(os.path.abspath(args.output))
    if not output.is_relative_to(ROOT/'.tmp') or output == ROOT/'.tmp':
        raise ValueError('Probe output needs its own directory below project .tmp')
    current = ROOT
    for name in output.relative_to(ROOT).parts:
        current /= name
        if current.exists() or current.is_symlink():
            meta = current.lstat()
            if stat.S_ISLNK(meta.st_mode) or getattr(meta, 'st_file_attributes', 0) & 0x400 or not current.is_dir():
                raise ValueError(f'Probe output traverses a link or non-directory: {current}')
    if output.exists():
        raise ValueError('Select a fresh output directory; probe evidence is never overwritten')
    for protected in (installation, executable.parent):
        if output.is_relative_to(protected) or protected.is_relative_to(output):
            raise ValueError(f'Probe output overlaps a protected installation/runtime: {protected}')
    renderer = executable.parent/f'fnql_{args.renderer}_x86.dll'
    if not renderer.is_file():
        raise ValueError(f'The matching renderer is missing: {renderer}')
    return executable, installation, output, renderer


def modules(installation):
    """Hash installed module candidates in memory without extracting a DLL."""
    result = []
    for pak in sorted((installation/'base').glob('*.pk3'), key=lambda path: path.name.casefold()):
        with zipfile.ZipFile(pak) as archive:
            for member in archive.infolist():
                if member.filename.casefold() in ('uix86.dll', 'cgamex86.dll', 'jampgamex86.dll'):
                    result.append(dict(pak=pak.name, member=member.filename, bytes=member.file_size,
                        crc32=f'{member.CRC:08x}', sha256=hashlib.sha256(archive.read(member)).hexdigest()))
    return result


def configuration(mode, frames):
    shot = 'fntech3-jka-'+mode
    action = f'echo {MARKER}{mode.upper().replace("-", "_")}; wait {frames}; screenshotPNG {shot}; wait 5; quit;'
    values = dict(developer=1, com_introplayed=1, com_recommendedSet=1, com_maxfps=60,
        com_maxfpsUnfocused=60, com_maxfpsMinimized=60, cl_allowDownload=0, cl_motd=0,
        cl_autoRecordDemos=0, sv_autoRecordDemos=0, ui_singlePlayerActive=0,
        s_volume=0, s_musicVolume=0)
    for number in range(1, 6):
        values[f'sv_master{number}'] = ''
    values['activeAction' if mode == 'connect' else 'jka_probeAction'] = action
    return ''.join(f'set {name} "{value}"\n' for name,value in values.items()), shot


def launch_command(executable, installation, home, args):
    settings = dict(com_gameProfile='jka-mp', fs_basepath=installation, fs_homepath=home,
        fs_game='', fs_cdpath='', r_fullscreen=0, r_mode=-1, r_customWidth=800,
        r_customHeight=600, in_mouse=0, in_joystick=0, in_nograb=1, in_synthetic=0,
        s_initsound=0, com_steamIntegration=0, fs_skipWorkshop=1, cl_webuiEnable=0,
        cl_renderer=args.renderer, logfile=4, net_enabled=1, net_ip='127.0.0.1', net_port=0,
        com_introplayed=1)
    tail = ['+exec', CONFIG]
    tail += ['+connect', f'127.0.0.1:{args.port}'] if args.mode == 'connect' else ['+vstr', 'jka_probeAction']
    result = command(executable, settings, tail)
    if sum(word.startswith('+') for word in result) > 32 or len(subprocess.list2cmdline(result[1:]).encode('utf-8')) >= 1024:
        raise ValueError('Startup arguments exceed the engine limit; shorten the project-local output path')
    return result


def remove_home(home, output):
    target = home.resolve(strict=True)
    if not target.is_relative_to(output.resolve(strict=True)) or target == output:
        raise RuntimeError('Refusing cleanup outside the probe output')
    for path in (target, *target.rglob('*')):
        meta = path.lstat()
        if path.is_symlink() or getattr(meta, 'st_file_attributes', 0) & 0x400:
            raise RuntimeError(f'Refusing probe cleanup through a link: {path}')
    shutil.rmtree(target)


def run(args):
    executable, installation, output, renderer = validate(args)
    home = output/'home'
    launch = launch_command(executable, installation, home, args)
    before = inventory(installation)
    candidates = modules(installation)
    config, shot = configuration(args.mode, args.frames)
    output.mkdir(parents=True)
    game = home/'base'; game.mkdir(parents=True)
    for name in ('jampconfig.cfg','jampconfig_server.cfg'):
        (game/name).write_text('', encoding='ascii')
    (game/CONFIG).write_text(config, encoding='utf-8')
    (output/CONFIG).write_text(config, encoding='utf-8')
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), profile='jka-mp',
        mode=args.mode, port=args.port, renderer=args.renderer, captureDelayFrames=args.frames,
        command=launch, executable=str(executable), executableSha256=sha(executable),
        rendererSha256=sha(renderer), installation=str(installation), moduleCandidates=candidates,
        commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        worktreeDirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT)),
        scope='Client UI/bootstrap or loopback admission subset; no input or complete gameplay claim',
        desktopSwitched=False, screenshots=[], passed=False, visualPassed=None)
    process = None; stopped = True; begin = time.monotonic()
    try:
        # A constructor can fail after creating a child. Preserve the home
        # until a returned process handle confirms teardown completed.
        stopped = False
        process = WindowsPrivateProcess(launch, executable.parent, output/'stdout.log')
        report['desktop'] = process.name
        deadline = begin+args.timeout
        while process.poll() is None and time.monotonic() < deadline:
            time.sleep(.2)
        report['exitCode'] = process.poll()
        report['forcedStop'] = report['exitCode'] is None
        if report['forcedStop']:
            process.terminate()
            report['exitCode'] = process.wait(process.EXIT_GRACE)
    except (OSError, subprocess.TimeoutExpired) as error:
        report['error'] = f'{type(error).__name__}: {error}'
    finally:
        if process:
            try:
                process.close()
                stopped = True
            except (OSError, subprocess.TimeoutExpired) as error:
                stopped = False
                report['stopError'] = f'{type(error).__name__}: {error}'
        console = read(game/'qconsole.log')
        text = console+'\n'+read(output/'stdout.log')
        (output/'console.log').write_text(console,encoding='utf-8')
        report['moduleDiagnostics'] = [line for line in text.splitlines() if re.search(r'VM_LoadDll|native module|loading.*(?:ui|cgame)',line,re.I)]
        report['loadedModuleCaches'] = [dict(path=str(path.relative_to(home)),sha256=sha(path))
            for path in home.rglob('*.dll') if path.is_file()]
        report['frontiers'] = list(dict.fromkeys(line for line in text.splitlines() if ERRORS.search(line)))
        report['windowedObserved'] = bool(re.search(r'800\s*x\s*600\s+windowed',text,re.I))
        report['commonInitializationObserved'] = '--- Common Initialization Complete ---' in text
        report['scheduledActionObserved'] = bool(re.search('^'+re.escape(MARKER+args.mode.upper().replace('-','_'))+r'\s*$',text,re.M))
        report['activeObserved'] = 'CL_FirstSnapshot:' in text
        report['uiInitReturnedInference'] = report['commonInitializationObserved'] and any('uix86' in line.casefold() for line in report['moduleDiagnostics'])
        report['uiInitInferenceBasis'] = 'CL_StartHunkUsers initializes UI before Common Initialization Complete; this is control-flow inference, not an export trace'
        for capture in sorted(home.rglob(shot+'.png')):
            destination=output/capture.name;shutil.copy2(capture,destination)
            report['screenshots'].append(dict(path=str(destination.relative_to(ROOT)),sha256=sha(destination)))
        report['installationUnchanged'] = before == inventory(installation)
        report['executableUnchanged'] = report['executableSha256'] == sha(executable)
        report['rendererUnchanged'] = report['rendererSha256'] == sha(renderer)
        report['elapsedSeconds'] = round(time.monotonic()-begin,3)
        report['passed'] = bool(report.get('exitCode')==0 and not report.get('forcedStop') and not report.get('error')
            and not report.get('stopError') and report['windowedObserved'] and report['scheduledActionObserved']
            and report['uiInitReturnedInference'] and len(report['screenshots'])==1 and not report['frontiers']
            and report['installationUnchanged'] and report['executableUnchanged'] and report['rendererUnchanged']
            and (args.mode=='startup-ui' or report['activeObserved']))
        if stopped:
            try:
                remove_home(home,output)
                report['homeRemoved'] = True
            except (ValueError,OSError,RuntimeError) as error:
                report['homeRemoved'] = False
                report['cleanupError'] = f'{type(error).__name__}: {error}'
                report['passed'] = False
        else:
            report['homeRemoved'] = False
        (output/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({key:report.get(key) for key in ('mode','passed','exitCode','forcedStop','frontiers','screenshots',
        'installationUnchanged','executableUnchanged','uiInitReturnedInference','activeObserved','error','stopError')},indent=2))
    return report['passed']


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,default=ROOT/'meson/build/win32-dev/fntech3.exe')
    parser.add_argument('--installation',type=Path,default=Path('E:/Games/Jedi Academy'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--mode',choices=('startup-ui','connect'),default='startup-ui')
    parser.add_argument('--port',type=int,help='Explicit existing loopback server port; connect mode only')
    parser.add_argument('--renderer',choices=('glx','vk','rtx'),default='glx')
    parser.add_argument('--frames',type=int,default=180)
    parser.add_argument('--timeout',type=float,default=60)
    args=parser.parse_args()
    if os.name!='nt':
        parser.error('This probe requires the Windows private-desktop harness')
    try:
        return 0 if run(args) else 1
    except (ValueError,OSError,zipfile.BadZipFile) as error:
        parser.error(str(error))


if __name__=='__main__':
    raise SystemExit(main())
