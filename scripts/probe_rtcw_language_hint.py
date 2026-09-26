#!/usr/bin/env python3
"""Capture RTCW MP's spectator hint in a chosen cl_language, retail and fnTech3.

The MP cgame prints "Press ... to follow next player" with the names of the
keys bound to that action, which it asks the engine for through its key-name
import. Original RTCW names them in the player's cl_language. This probe starts
one fnTech3 dedicated rtcw-mp server and joins it as a spectator with retail
WolfMP.exe and with the fnTech3 client, each in the given language, and keeps
each engine's own screenshot of the hint.

Windows only: windowed on unswitched private desktops with isolated homes,
loopback only, sound and mouse/joystick input disabled, console commands only;
no injected input and no operating-system capture.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return path.read_text(encoding='utf-8', errors='replace') if path.exists() else ''


def wait_process(process, seconds):
    begin = time.monotonic()
    while process.poll() is None and time.monotonic() - begin < seconds:
        time.sleep(0.2)
    return process.poll()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--language', type=int, default=1, help='cl_language: 1 French, 2 German, 3 Italian, 4 Spanish')
    parser.add_argument('--skip-retail', action='store_true')
    parser.add_argument('--live-switch', action='store_true',
                        help='start in English, capture, switch to --language in the session, capture again')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    installation = args.installation.resolve(strict=True)
    client, server = args.client.resolve(strict=True), args.server.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    server_home = output / 'server-home'
    server_home.mkdir()
    server_command = [str(server)]
    for name, value in dict(com_gameProfile='rtcw-mp', fs_basepath=str(installation), fs_homepath=str(server_home),
                            dedicated='1', net_ip='127.0.0.1', net_port=str(port), sv_pure='0', g_gametype='5',
                            bot_enable='0', sv_punkbuster='0', logfile='2',
                            **{f'sv_master{i}': '' for i in range(1, 6)}).items():
        server_command += ['+set', name, value]
    server_command += ['+map', 'mp_beach']
    report = dict(language=args.language, liveSwitch=args.live_switch, serverSha256=sha(server), clientSha256=sha(client), runs={},
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip())
    host = WindowsPrivateProcess(server_command, server.parent, output / 'server-stdout.log')
    try:
        begin = time.monotonic()
        while time.monotonic() - begin < 60 and not re.search(r'^\d+ items registered',
                                                              read(server_home / 'main/qconsole.log'), re.M):
            time.sleep(0.2)
        time.sleep(2)
        runs = [('fntech3', client)] + ([] if args.skip_retail else [('retail', installation / 'WolfMP.exe')])
        for label, executable in runs:
            home = output / f'{label}-home'
            (home / 'main').mkdir(parents=True)
            if label == 'retail':
                values = dict(fs_basepath=str(installation), fs_homepath=str(home), r_fullscreen='0', r_mode='3',
                              in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0', logfile='2',
                              cl_punkbuster='0', cl_allowDownload='0', r_ignorehwgamma='1', com_introplayed='1',
                              # No update check from the main menu, and no MOTD request there or on connect.
                              cl_autoupdate='0', cl_motd='0',
                              cl_language='0' if args.live_switch else str(args.language), name='language-probe',
                              activeAction=(f'wait 300; CloseLimboMenu; wait 60; screenshot; set cl_language {args.language}; '
                                            'wait 120; screenshot; wait 20; quit;') if args.live_switch else
                              'wait 300; CloseLimboMenu; wait 60; screenshot; wait 20; quit;')
            else:
                values = dict(com_gameProfile='rtcw-mp', fs_basepath=str(installation), fs_homepath=str(home),
                              fs_game='', r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                              s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                              cl_renderer='glx', logfile='2', com_introplayed='1',
                              cl_language='0' if args.live_switch else str(args.language),
                              name='language-probe', net_port='0',
                              # fnTech3 captures the next frame it renders, so let that frame be drawn
                              # before the language changes.
                              activeAction=(f'wait 300; CloseLimboMenu; wait 60; screenshotPNG hint-a; wait 5; set cl_language {args.language}; '
                                            'wait 120; screenshotPNG hint-b; wait 20; quit') if args.live_switch else
                              'wait 300; CloseLimboMenu; wait 60; screenshotPNG hint; wait 20; quit')
            command = [str(executable)]
            for name, value in values.items():
                command += ['+set', name, value]
            command += ['+connect', f'127.0.0.1:{port}']
            process = WindowsPrivateProcess(command, executable.parent, output / f'{label}-stdout.log')
            try:
                code = wait_process(process, 90)
                forced = process.poll() is None
            finally:
                process.close()
            shots = []
            for capture in sorted(home.rglob('*')):
                if capture.suffix.lower() in ('.png', '.tga', '.jpg') and 'screenshots' in capture.parts:
                    target = output / f'{label}-{capture.name}'
                    target.write_bytes(capture.read_bytes())
                    shots.append(dict(name=target.name, sha256=sha(target)))
            report['runs'][label] = dict(command=' '.join(command[1:]), exitCode=code, forcedTermination=forced,
                                         screenshots=shots, executableSha256=sha(executable))
    finally:
        host.close()
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'runs'}, indent=2))
    for label, run in report['runs'].items():
        print(label, run['exitCode'], run['forcedTermination'], [s['name'] for s in run['screenshots']])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
