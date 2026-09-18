#!/usr/bin/env python3
"""Run an isolated, windowed Enemy Territory client on a private desktop.

Use an executable configured explicitly with -Dwolfet-development=true. The
retail cgame and UI modules are loaded from the installation, which is only read
from and is checked unchanged afterwards; no module, asset or capture is copied
into the repository. No input is injected. Captures use the engine's own
screenshotPNG command, never an operating-system screen capture.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

MAPS = ('oasis', 'battery', 'goldrush', 'fueldump', 'radar', 'railgun')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): [path.stat().st_size, path.stat().st_mtime_ns]
            for path in installation.rglob('*') if path.is_file()}


def remove_home(home, output):
    target = home.resolve(strict=True)
    target.relative_to(output.resolve(strict=True))
    for path in [target, *target.rglob('*')]:
        info = path.lstat()
        if path.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise RuntimeError(f'Refusing cleanup through a link: {path}')
    shutil.rmtree(target)


def wait_for_exit(process, log, command, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        code = process.poll()
        if code is not None:
            return code
        if log.is_file():
            text = log.read_text(encoding='utf-8', errors='replace')
            if 'recursive error after:' in text or 'Server fatal crashed:' in text:
                raise RuntimeError('Engine reported a fatal error; see console.log')
        time.sleep(0.1)
    raise subprocess.TimeoutExpired(command, timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), default='glx')
    parser.add_argument('--scene', choices=('ui', 'map'), default='map')
    parser.add_argument('--map', choices=MAPS, default='oasis')
    parser.add_argument('--frames', type=int, default=180,
                        help='Frames to wait after becoming active before capture (1-2400)')
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--hunk', type=int, default=0,
                        help='Override com_hunkMegs for this run (0 keeps the engine and profile choice)')
    parser.add_argument('--audio', choices=('off', 'openal-muted'), default='off',
                        help='Focus-muted OpenAL loads sounds and audio zones without playing them')
    parser.add_argument('--extra', default='',
                        help='Console commands run just before the capture, for diagnostics')
    args = parser.parse_args()
    if not 1 <= args.frames <= 2400:
        parser.error('--frames must be between 1 and 2400')
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    executable = args.executable.resolve(strict=True)
    executable.relative_to(ROOT)
    installation = args.installation.resolve(strict=True)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    before = inventory(installation)
    home = Path(tempfile.mkdtemp(prefix='home-', dir=output))
    game = home / 'etmain'
    game.mkdir()
    filename = f'fntech3-wolfet-{args.scene}-{args.map}-{args.renderer}'
    startup = dict(com_gameProfile='wolfet', fs_basepath=str(installation), fs_homepath=str(home),
        fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth='640',
        r_customHeight='480', in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0',
        com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0', cl_renderer=args.renderer,
        logfile='4', net_ip='127.0.0.1', net_port='0', net_ip6='::1', net_port6='0',
        rtx_rt_mode='0', rtx_rt_require='0')
    if args.hunk:
        startup['com_hunkMegs'] = str(args.hunk)
    if args.audio == 'openal-muted':
        # The private desktop never has focus, so a focus-muted device is silent.
        startup.update(s_initsound='1', s_backend='openal')
    values = dict(developer='1', sv_autoRecordDemos='0', cl_autoRecordDemos='0',
        cl_allowDownload='0', cl_motd='0', com_introplayed='1', com_recommendedSet='1',
        com_maxfps='60', com_maxfpsUnfocused='60', com_maxfpsMinimized='60', sv_pure='0')
    for number in range(1, 6):
        values[f'sv_master{number}'] = ''
    if args.audio == 'openal-muted':
        values.update(s_muteWhenUnfocused='1', s_muteWhenMinimized='1')
    extra = (args.extra.strip().rstrip(';') + '; ') if args.extra.strip() else ''
    tail = f'wait {args.frames}; {extra}screenshotPNG {filename}; wait 5; quit'
    if args.scene == 'map':
        values['activeAction'] = tail
    else:
        values['wolfet_probeAction'] = tail
    config = ''.join(f'set {name} "{value}"\n' for name, value in values.items())
    (game / 'wolfet-probe.cfg').write_text(config, encoding='utf-8')
    (output / 'wolfet-probe.cfg').write_text(config, encoding='utf-8')
    command = [str(executable)]
    for name, value in startup.items():
        command += ['+set', name, value]
    command += ['+exec', 'wolfet-probe.cfg']
    if args.scene == 'map':
        command += ['+devmap', args.map]
    else:
        command += ['+vstr', 'wolfet_probeAction']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        worktreeDirty=bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
        executable=str(executable), executableSha256=sha(executable),
        renderer=args.renderer, rendererSha256=sha(executable.parent / f'fnql_{args.renderer}_x86.dll'),
        installation=str(installation), scene=args.scene, map=args.map if args.scene == 'map' else None,
        audio=args.audio,
        captureDelayFrames=args.frames, command=command,
        scope='Experimental ET client through retail cgame and UI modules; no input, peer '
              'interop or gameplay claim', screenshots=[])
    process = None
    try:
        assert sum(word.startswith('+') for word in command) <= 32, 'Startup command count exceeds engine capacity'
        assert len(subprocess.list2cmdline(command[1:]).encode('utf-8')) < 1024, 'Startup command line exceeds engine capacity'
        process = WindowsPrivateProcess(command, executable.parent, output / 'stdout.log')
        report['desktop'] = process.name
        report['exitCode'] = wait_for_exit(process, game / 'qconsole.log', command, args.timeout)
    except Exception as error:
        report['error'] = str(error)
        report['errorType'] = type(error).__name__
    finally:
        if process:
            report['forcedStop'] = process.poll() is None
            process.close()
        log = game / 'qconsole.log'
        text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
        if log.is_file():
            shutil.copy2(log, output / 'console.log')
        for capture in home.rglob(filename + '.png'):
            destination = output / capture.name
            shutil.copy2(capture, destination)
            report['screenshots'].append(dict(path=str(destination), sha256=sha(destination)))
        report['modules'] = [line for line in text.splitlines() if re.search(
            r'_mp_x86|vm_loaddll|loading dll|Sys_LoadDll', line, re.I)]
        report['engineErrors'] = [line for line in text.splitlines() if re.search(
            r'error:|sys_error|fatal crashed:|not implemented|unsupported .*(cgame|ui) import|'
            r'recursive cl_shutdown|server command overflow|lost reliable commands', line, re.I)]
        report['windowedObserved'] = '640 x 480 windowed' in text
        report['activeObserved'] = bool(re.search(r'CA_ACTIVE|entered the game', text))
        report['passed'] = (report.get('exitCode') == 0 and not report.get('forcedStop')
                            and len(report['screenshots']) == 1 and report['windowedObserved']
                            and not report['engineErrors'])
        report['visualPassed'] = None  # An engine capture needs a separate visual inspection.
        remove_home(home, output)
        report['homeRemoved'] = True
        report['installationUnchanged'] = before == inventory(installation)
        (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(args.scene, args.map, args.renderer, 'CLIENT PASS' if report['passed'] else 'CLIENT FAIL',
          report.get('errorType', ''), flush=True)
    if not report['installationUnchanged']:
        raise RuntimeError('Installation file metadata changed during the probe')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
