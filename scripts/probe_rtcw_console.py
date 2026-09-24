#!/usr/bin/env python3
"""Capture a Wolfenstein profile's console, and optionally RTCW MP's HUD, with
the engine's own screenshot command.

Runs fnTech3 (`rtcw-mp`, `rtcw-sp` or `wolfet`) or that profile's retail
executable (WolfMP.exe, WolfSP.exe or et.exe) windowed on an unswitched private
desktop with an isolated home, mouse, joystick and sound off, and the
installation read-only and inventoried before and after. The console is opened
with the engine's toggleconsole command; no input is sent. fnTech3 captures
with screenshotPNG, retail with screenshot. Only RTCW MP has a map scene: RTCW
SP's map start shows a briefing that only a click dismisses. Retail runs only
at the main menu, 640x480 (r_mode 3), with its MOTD and auto-update requests
off; the report lists any address the log says was resolved. After an fnTech3
capture the report records the search path, the console shaders the renderer
loaded, and where the engine finds each --which file. Reports hold hashes,
never copied assets.
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
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

# Each profile's game directory, retail executable and retail log.
PROFILES = {
    'rtcw-mp': ('main', 'WolfMP.exe', 'rtcwconsole.log'),
    'rtcw-sp': ('main', 'WolfSP.exe', 'rtcwconsole.log'),
    'wolfet': ('etmain', 'et.exe', 'etconsole.log'),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): [path.stat().st_size, path.stat().st_mtime_ns]
            for path in installation.rglob('*') if path.is_file()}


def inspection(text, paths):
    """What the fnTech3 run's closing path, shaderlist, imagelist and which
    commands printed."""
    listing = text.rsplit('Current search path:', 1)[1] if 'Current search path:' in text else ''
    stems = [path.rsplit('.', 1)[0].lower() for path in paths]
    return dict(
        searchPaks=re.findall(r'^[^\n]*?([^\\/\n]+\.pk3) \(\d+ files\)$', listing.split('\n\n', 1)[0], re.M),
        consoleShaders=[line.strip() for line in text.splitlines() if re.search(r': console\S*( \(DEFAULTED\))?$', line)],
        images=[line.strip() for line in text.splitlines()
                if re.match(r'\s*\d+\s+\d+\s+\d+ ', line) and any(stem in line.lower() for stem in stems)],
        which=[line.strip() for line in text.splitlines() if re.match(r'File ("|not found: ")', line)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=PROFILES, default='rtcw-mp')
    parser.add_argument('--engine', choices=('fntech3', 'retail'), default='fntech3')
    parser.add_argument('--executable', type=Path,
                        help="fnTech3 client built with the profile's development option")
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scene', choices=('ui', 'map'), default='ui')
    parser.add_argument('--width', type=int, default=640)
    parser.add_argument('--height', type=int, default=480)
    parser.add_argument('--cvar', action='append', default=[], metavar='NAME=VALUE',
                        help='Set in the probe config, e.g. con_backgroundStyle=0')
    parser.add_argument('--hud', action='store_true',
                        help='On mp_beach, close the limbo menu, capture the HUD and write cl_hudDump first')
    parser.add_argument('--which', action='append', default=[], metavar='PATH',
                        help="After an fnTech3 capture, find this file with the engine's which command")
    parser.add_argument('--timeout', type=int, default=150)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    if args.scene == 'map' and args.profile != 'rtcw-mp':
        parser.error('Only rtcw-mp has a map scene')
    if args.engine == 'retail' and (args.scene != 'ui' or args.hud or args.cvar or args.executable or args.which):
        parser.error('Retail runs only the main-menu scene, with no --hud, --cvar, --executable or --which')
    if args.engine == 'fntech3' and not args.executable:
        parser.error('--executable is required for fnTech3')
    if args.hud and args.scene != 'map':
        parser.error('--hud requires --scene map')
    if not (320 <= args.width <= 7680 and 240 <= args.height <= 4320):
        parser.error('--width/--height out of range')
    for assignment in args.cvar:
        if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*=[^";]*', assignment):
            parser.error(f'Invalid --cvar {assignment!r}')
    for path in args.which:
        if not re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_./-]*', path) or '..' in path.split('/'):
            parser.error(f'Invalid --which {path!r}')
    game, retail, retail_log = PROFILES[args.profile]
    installation = args.installation.resolve(strict=True)
    if args.engine == 'retail':
        executable = (installation / retail).resolve(strict=True)
    else:
        executable = args.executable.resolve(strict=True)
        executable.relative_to(ROOT)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    (home / game).mkdir(parents=True)
    before = inventory(installation)

    shot = 'rtcw-console'
    hud = f'screenshotPNG {shot}-hud; wait 5; set cl_hudDump 0; ' if args.hud else ''
    pre = 'wait 210; CloseLimboMenu; wait 60; ' if args.hud else ''
    if args.engine == 'retail':
        # Retail's command line splits at every '+', so the action is a
        # sequence of startup commands rather than one vstr. cl_motd and
        # cl_autoupdate keep retail from contacting its master and update
        # servers; et.exe asks etmaster.idsoftware.com for its MOTD otherwise.
        settings = dict(fs_basepath=str(installation), fs_homepath=str(home), r_fullscreen='0', r_mode='3',
                        in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0', logfile='2',
                        cl_punkbuster='0', cl_allowDownload='0', cl_motd='0', cl_autoupdate='0',
                        r_ignorehwgamma='1', com_introplayed='1')
        tail = ['+wait', '200', '+toggleconsole', '+wait', '90', '+screenshot', shot, '+wait', '10', '+quit']
        log = home / game / retail_log
        capture = shot + '.tga'
    else:
        settings = dict(com_gameProfile=args.profile, fs_basepath=str(installation), fs_homepath=str(home),
                        fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth=str(args.width),
                        r_customHeight=str(args.height), in_mouse='0', in_joystick='0', in_nograb='1',
                        s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                        cl_renderer='glx', logfile='4', net_ip='127.0.0.1', net_port='0', net_ip6='::1',
                        net_port6='0')
        values = dict(developer='1', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                      cl_motd='0', com_introplayed='1', com_recommendedSet='1', com_maxfps='60',
                      com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
                      **{f'sv_master{number}': '' for number in range(1, 6)})
        if args.hud:
            values['cl_hudDump'] = '1'
        values.update(assignment.split('=', 1) for assignment in args.cvar)
        # The inspection runs from its own file after the capture, which keeps
        # the action within one cvar value however many files are named.
        commands = ['path', 'shaderlist', 'imagelist'] + [f'which {path}' for path in args.which]
        (home / game / 'console-inspect.cfg').write_text(''.join(f'{line}\n' for line in commands),
                                                          encoding='utf-8')
        action = (f'wait 90; {pre}{hud}toggleconsole; wait 90; screenshotPNG {shot}; wait 5; '
                  'exec console-inspect.cfg; quit')
        values['activeAction' if args.scene == 'map' else 'probeAction'] = action
        config = ''.join(f'set {name} "{value}"\n' for name, value in values.items())
        (home / game / 'console-probe.cfg').write_text(config, encoding='utf-8')
        (output / 'console-probe.cfg').write_text(config, encoding='utf-8')
        tail = ['+exec', 'console-probe.cfg'] + (['+devmap', 'mp_beach'] if args.scene == 'map'
                                                 else ['+vstr', 'probeAction'])
        log = home / game / 'qconsole.log'
        capture = shot + '*.png'
    command = [str(executable)]
    for name, value in settings.items():
        command += ['+set', name, value]
    command += tail
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  profile=args.profile, engine=args.engine, executable=str(executable),
                  executableSha256=sha(executable), installation=str(installation), scene=args.scene,
                  cvars=args.cvar, hud=args.hud, command=command, screenshots=[])
    renderer = executable.parent / 'fnql_glx_x86.dll'
    if args.engine == 'fntech3' and renderer.is_file():
        report['rendererSha256'] = sha(renderer)
    process = None
    try:
        process = WindowsPrivateProcess(command, executable.parent, output / 'stdout.log')
        report['desktop'] = process.name
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline and process.poll() is None:
            time.sleep(0.2)
        report['exitCode'] = process.poll()
    except OSError as error:
        report['error'] = str(error)
    finally:
        if process:
            report['forcedStop'] = process.poll() is None
            process.close()
    text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
    if log.is_file():
        shutil.copy2(log, output / log.name)
    for image in sorted(home.rglob(capture)):
        destination = output / image.name
        shutil.copy2(image, destination)
        report['screenshots'].append(dict(name=image.name, sha256=sha(destination)))
    dump = next(home.rglob('fntech3-hud-dump.json'), None)
    if dump:
        shutil.copy2(dump, output / dump.name)
        groups = json.loads(dump.read_text(encoding='utf-8'))['rules']
        report['hudDumpShaders'] = sorted({group['match'].get('shader', '') for group in groups})
    report['engineErrors'] = [line for line in text.splitlines()
                              if re.search(r'error:|sys_error|fatal crashed:|recursive', line, re.I)]
    report['consoleArtWarnings'] = [line for line in text.splitlines()
                                    if 'R_FindImageFile' in line and "shader 'console" in line
                                    or re.search(r'Shader image unavailable for: console', line)]
    report['networkLookups'] = [line for line in text.splitlines() if re.match(r'Resolving ', line)]
    if args.engine == 'fntech3':
        report['inspection'] = inspection(text, args.which)
    shutil.rmtree(home)
    report['installationUnchanged'] = before == inventory(installation)
    report['passed'] = (report.get('exitCode') == 0 and not report.get('forcedStop')
                        and len(report['screenshots']) == (2 if args.hud else 1)
                        and not report['engineErrors'] and report['installationUnchanged'])
    if args.engine == 'fntech3':
        report['windowedObserved'] = f'{args.width} x {args.height} windowed' in text
        report['passed'] &= report['windowedObserved']
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(args.profile, args.engine, args.scene, 'PASS' if report['passed'] else 'FAIL', report.get('exitCode'),
          flush=True)
    if not report['installationUnchanged']:
        raise RuntimeError('Installation file metadata changed during the probe')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
