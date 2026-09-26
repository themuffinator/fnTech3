#!/usr/bin/env python3
"""RTCW MP and ET dedicated servers echo broadcast prints without "[lon]"/"[lof]".

Windows only. Runs one dedicated server on an unswitched private desktop with
an isolated home, loopback networking and no clients: fnTech3's (--server) or
the installation's own retail server (--retail-server: WolfMP.exe for rtcw-mp,
etded.exe for wolfet). A config it execs loads a map and changes timelimit
twice. Both games broadcast each change as
`print "Server:[lof] timelimit [lon]changed to[lof] N\n"`, and both retail
engines echo it to the console without the tokens. The run passes when each
change was echoed exactly once in that form, no echo kept a token, the server
quit cleanly within the timeout and the installation is unchanged. No input is
injected and nothing is rendered.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

# retailDedicated: the retail server is the client executable run with
# dedicated 1; etded.exe is a dedicated build and sets dedicated itself.
PROFILES = {
    'rtcw-mp': dict(game='main', retail='WolfMP.exe', retailDedicated=True, retailLog='rtcwconsole.log',
                    module='Main/qagame_mp_x86.dll', map='mp_beach'),
    'wolfet': dict(game='etmain', retail='etded.exe', retailDedicated=False, retailLog='etconsole.log',
                   module='etmain/qagame_mp_x86.dll', map='goldrush'),
}
LIMITS = ('17', '23')
MARKER = 'fntech3-broadcast-echo-'
SCRIPT = 'fntech3-broadcast-echo.cfg'
SETTLE_FRAMES = '60'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def probe(args, output):
    settings = PROFILES[args.profile]
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    (home / settings['game']).mkdir(parents=True)
    retail = args.retail_server
    server = args.installation / settings['retail'] if retail else args.server
    mapname = args.map or settings['map']
    before = inventory(args.installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    values = dict(fs_basepath=str(args.installation), fs_homepath=str(home),
                  logfile='2', net_ip='127.0.0.1', net_port=str(port), sv_pure='0',
                  sv_punkbuster='0', bot_enable='0', **{f'sv_master{i}': '' for i in range(1, 6)})
    if not retail:
        values = dict(com_gameProfile=args.profile, dedicated='1', **values)
    elif settings['retailDedicated']:
        values = dict(dedicated='1', **values)
    lines = [f'map {mapname}', f'wait {SETTLE_FRAMES}', f'echo {MARKER}loaded']
    for limit in LIMITS:
        lines += [f'timelimit {limit}', f'wait {SETTLE_FRAMES}']
    lines += [f'echo {MARKER}done', 'quit']
    (home / settings['game'] / SCRIPT).write_text(''.join(line + '\n' for line in lines), encoding='ascii')
    command = [str(server)]
    for name, value in values.items():
        command += ['+set', name, value]
    command += ['+exec', SCRIPT]
    log = home / settings['game'] / (settings['retailLog'] if retail else 'qconsole.log')
    result = dict(evidence=f'Live: retail {server.name} dedicated server' if retail
                  else 'Live: fnTech3 dedicated server', profile=args.profile, map=mapname,
                  retailServer=retail, command=command, script=lines, serverSha256=sha(server),
                  moduleSha256=sha(args.installation / settings['module']), passed=False)
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
    result['log'] = str(log)
    result['markers'] = [line.strip() for line in text.splitlines() if line.startswith(MARKER)]
    result['broadcasts'] = [line for line in text.splitlines() if line.startswith('broadcast: ')]
    expected = [f'broadcast: print "Server: timelimit changed to {limit}\\n"' for limit in LIMITS]
    result['expected'] = expected
    result['expectedCounts'] = [result['broadcasts'].count(line) for line in expected]
    result['tokenEchoes'] = [line for line in result['broadcasts'] if '[lon]' in line or '[lof]' in line]
    result['errors'] = [line for line in text.splitlines() if line.startswith('ERROR') or 'Error:' in line]
    result['installationUnchanged'] = before == inventory(args.installation)
    # Retail ET's echo queues a cpm command instead of printing, so its log has
    # no markers; the second change's echo and a clean quit show the run ended.
    result['passed'] = bool(result['expectedCounts'] == [1] * len(LIMITS) and not result['tokenEchoes']
                            and result.get('exitCode') == 0 and not result.get('timedOut')
                            and result['installationUnchanged'] and not result.get('error')
                            and not result['errors'])
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('evidence', 'profile', 'serverSha256', 'broadcasts',
                                              'expectedCounts', 'tokenEchoes', 'exitCode',
                                              'installationUnchanged', 'passed')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    servers = parser.add_mutually_exclusive_group(required=True)
    servers.add_argument('--server', type=Path, help='fnTech3 dedicated executable')
    servers.add_argument('--retail-server', action='store_true',
                         help="run the installation's retail dedicated server")
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail root holding the executable and its game directory')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--map', help='stock map to load (default: mp_beach or goldrush)')
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
