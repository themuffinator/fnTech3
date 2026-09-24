#!/usr/bin/env python3
"""RTCW MP map_restart keeps g_gametype to a type the map allows.

Windows only. Runs one dedicated server, fnTech3's (--server) or retail
WolfMP.exe (--retail-server), on an unswitched private desktop with an isolated
home, loopback networking and no clients, and drives it only through its
command line and a config it execs. No input is injected and nothing is
rendered.

mp_destruction's worldspawn forbids objective and stopwatch (spawnflags 3) and
mp_beach's forbids checkpoint (4). Each step loads a map or restarts, sometimes
after latching a game type from the console, then prints g_gametype and
gamestate behind a marker. The run passes when every printed type is the one
retail WolfMP.exe's map_restart keeps and the installation is unchanged.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

# (marker, console commands, g_gametype afterwards)
STEPS = (
    ('loaded-destruction', [['map', 'mp_destruction']], '5'),
    ('restart-objective-on-destruction', [['map_restart']], '7'),
    ('restart-latched-stopwatch-on-destruction', [['g_gametype', '6'], ['map_restart']], '7'),
    ('loaded-beach', [['map', 'mp_beach']], '7'),
    ('restart-checkpoint-on-beach', [['map_restart']], '5'),
    ('restart-latched-stopwatch-on-beach', [['g_gametype', '6'], ['map_restart']], '6'),
)
MARKER = 'fntech3-restart-step-'
SETTLE_FRAMES = '60'
SCRIPT = 'fntech3-restart-game-type.cfg'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def printed(text, name):
    match = re.search(r'^"%s" is:"(.*?)(?:\^7)?"' % re.escape(name), text, re.M)
    return match.group(1) if match else None


def probe(args, output):
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    home.mkdir()
    retail = args.retail_server
    server = args.installation / 'WolfMP.exe' if retail else args.server
    before = inventory(args.installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    values = dict(fs_basepath=str(args.installation), fs_homepath=str(home), dedicated='1',
                  logfile='2', net_ip='127.0.0.1', net_port=str(port), sv_pure='0',
                  sv_punkbuster='0', bot_enable='0', g_gametype='5',
                  **{f'sv_master{i}': '' for i in range(1, 6)})
    if not retail:
        values = dict(com_gameProfile='rtcw-mp', **values)
    # fnTech3 limits its startup command to 1,023 bytes, so the steps run from
    # a config in the isolated home, which both engines search first.
    lines = []
    for marker, actions, _ in STEPS:
        lines += [' '.join(action) for action in actions]
        lines += [f'wait {SETTLE_FRAMES}', f'echo {MARKER}{marker}', 'g_gametype', 'gamestate']
    lines += [f'wait {SETTLE_FRAMES}', 'quit']
    (home / 'main').mkdir()
    (home / 'main' / SCRIPT).write_text(''.join(line + '\n' for line in lines), encoding='ascii')
    command = [str(server)]
    for name, value in values.items():
        command += ['+set', name, value]
    command += ['+exec', SCRIPT]
    log = home / 'main' / ('rtcwconsole.log' if retail else 'qconsole.log')
    result = dict(evidence='Live: retail WolfMP.exe dedicated server' if retail
                  else 'Live: fnTech3 dedicated server', profile='rtcw-mp', retailServer=retail,
                  command=command, script=lines, serverSha256=sha(server),
                  moduleSha256=sha(args.installation / 'Main/qagame_mp_x86.dll'), passed=False)
    begin = time.monotonic()
    host = None
    try:
        host = WindowsPrivateProcess(command, server.parent, output / 'server-stdout.log')
        while time.monotonic() - begin < args.timeout and host.poll() is None:
            time.sleep(0.2)
        result['exitCode'] = host.poll()
        result['timedOut'] = result['exitCode'] is None
    except OSError as error:
        result['error'] = str(error)
    finally:
        if host:
            host.close()
    result['elapsedSeconds'] = round(time.monotonic() - begin, 3)
    text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
    steps = []
    for marker, actions, expected in STEPS:
        start = text.find(MARKER + marker)
        tail = text[start:] if start >= 0 else ''
        steps.append(dict(step=marker, commands=[' '.join(a) for a in actions], expected=expected,
                          g_gametype=printed(tail, 'g_gametype'), gamestate=printed(tail, 'gamestate')))
    result['steps'] = steps
    result['variableChangeRestarts'] = len(re.findall(r'variable change -- restarting', text))
    # A level spawn prints both banners; an in-place restart re-runs only the
    # game's. Broadcasts are echoed as the server console prints them.
    result['serverInitializations'] = text.count('------ Server Initialization ------')
    result['gameInitializations'] = text.count('------- Game Initialization -------')
    result['broadcasts'] = [line for line in text.splitlines() if line.startswith('broadcast: ')]
    result['errors'] = [line for line in text.splitlines() if line.startswith('ERROR') or 'Error:' in line]
    result['installationUnchanged'] = before == inventory(args.installation)
    result['passed'] = bool(all(s['g_gametype'] == s['expected'] for s in steps)
                            and result['installationUnchanged'] and not result.get('timedOut')
                            and not result.get('error') and not result['errors'])
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('evidence', 'serverSha256', 'steps', 'variableChangeRestarts',
                                              'serverInitializations', 'gameInitializations', 'broadcasts',
                                              'installationUnchanged', 'passed')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    servers = parser.add_mutually_exclusive_group(required=True)
    servers.add_argument('--server', type=Path, help='fnTech3 dedicated executable')
    servers.add_argument('--retail-server', action='store_true', help='run retail WolfMP.exe as the server')
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail RTCW root holding WolfMP.exe and Main/')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=120)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This installed native-module probe requires Windows')
    args.installation = args.installation.resolve(strict=True)
    if args.server:
        args.server = args.server.resolve(strict=True)
    return 0 if probe(args, args.output.resolve()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
