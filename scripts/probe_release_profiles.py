#!/usr/bin/env python3
"""Run q3, ql, rtcw-sp and rtcw-mp from one pair of release binaries.

The same client executable loads a stock map under each profile, against that
profile's own installation and a fresh isolated home, and takes the engine's own
screenshot once the level is running (RTCW SP stops at its mission briefing,
since no input is given). The same dedicated-server executable then starts each
multiplayer profile on a stock map and must answer direct, challenged getinfo
and getstatus queries with the profile's protocol and expected map
before quitting. Windowed on unswitched private desktops with sound and
mouse/joystick input disabled; console commands only; loopback only.
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
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess
from server_query_evidence import observed_port, server_info

RUNS = {
    # profile: (installation argument, game dir, client map command, server map command, extra client cvars)
    'q3': ('q3', 'baseq3', ['+devmap', 'q3dm1'], ['+map', 'q3dm1'], {}),
    'ql': ('ql', 'baseq3', ['+devmap', 'campgrounds', 'ffa'], ['+map', 'campgrounds', 'ffa'], {}),
    'rtcw-sp': ('rtcw', 'main', ['+spdevmap', 'escape1'], None, {}),
    'rtcw-mp': ('rtcw', 'main', ['+devmap', 'mp_beach'], ['+map', 'mp_beach'], {'g_gametype': '5'}),
}
ACTION = {'rtcw-mp': 'wait 180; CloseLimboMenu; wait 120; screenshotPNG release-{p}; wait 5; quit',
          'rtcw-sp': 'wait 300; screenshotPNG release-{p}; wait 5; quit'}
DEFAULT_ACTION = 'wait 200; screenshotPNG release-{p}; wait 5; quit'
PROTOCOLS = {'q3': 68, 'ql': 91, 'rtcw-sp': 49, 'rtcw-mp': 60}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def getstatus(port, protocol, mapname):
    challenge = 'fntech3-' + uuid.uuid4().hex
    observed = {}
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.settimeout(1.0)
        # Connected UDP rejects datagrams from another source address/port.
        # The echoed nonce and identity reject stale and foreign-map replies.
        probe.connect(('127.0.0.1', port))
        try:
            # SVC_Info publishes protocol explicitly. SVC_Status publishes
            # serverinfo, whose protocol key is not required by Q3/QL/RTCW.
            for command, response_name, expected_protocol in (
                    ('getinfo', 'infoResponse', protocol), ('getstatus', 'statusResponse', None)):
                probe.send(b'\xff\xff\xff\xff' + command.encode('ascii') + b' ' + challenge.encode('ascii') + b'\n')
                reply = server_info(probe.recv(65535), response_name, mapname, challenge, expected_protocol)
                if reply is None:
                    return None
                observed[command] = reply
            return observed
        except (socket.timeout, ConnectionResetError):
            return None


def run(command, cwd, stdout, seconds, poll=None):
    process = WindowsPrivateProcess(command, cwd, stdout)
    begin, answered = time.monotonic(), False
    try:
        while process.poll() is None and time.monotonic() - begin < seconds:
            if poll and not answered:
                answered = poll()
            time.sleep(0.5)
        forced = process.poll() is None
        code = process.poll()
    finally:
        try:
            process.close()
        except subprocess.TimeoutExpired:
            process.wait(60)
            process.close()
    return dict(exitCode=code, forcedTermination=forced, elapsedSeconds=round(time.monotonic() - begin, 1),
                answeredGetstatus=bool(answered) if poll else None,
                serverInfo=answered.get('getinfo') if isinstance(answered, dict) else None,
                statusInfo=answered.get('getstatus') if isinstance(answered, dict) else None)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--q3', type=Path, required=True)
    parser.add_argument('--ql', type=Path, required=True)
    parser.add_argument('--rtcw', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client, server = args.client.resolve(strict=True), args.server.resolve(strict=True)
    roots = {'q3': args.q3.resolve(strict=True), 'ql': args.ql.resolve(strict=True),
             'rtcw': args.rtcw.resolve(strict=True)}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = dict(clientSha256=sha(client), serverSha256=sha(server), clients={}, servers={}, failures=[],
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip())
    for profile, (root, game, client_map, server_map, extra) in RUNS.items():
        home = output / f'{profile}-client'
        (home / game).mkdir(parents=True)
        values = dict(com_gameProfile=profile, fs_basepath=str(roots[root]), fs_homepath=str(home), fs_game='',
                      r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0',
                      com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0', cl_renderer='glx',
                      logfile='2', net_port='0', net_ip='127.0.0.1', net_ip6='::1',
                      com_introplayed='1', cl_allowDownload='0', bot_enable='0',
                      sv_pure='0', con_notifytime='0',
                      activeAction=ACTION.get(profile, DEFAULT_ACTION).format(p=profile), **extra,
                      **{f'sv_master{i}': '' for i in range(1, 6)})
        command = [str(client)]
        for key, value in values.items():
            command += ['+set', key, value]
        command += client_map
        row = run(command, client.parent, output / f'{profile}-client-stdout.log', 150)
        log = (home / game / 'qconsole.log')
        text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
        (output / f'{profile}-client-console.log').write_text(text, encoding='utf-8')
        shots = sorted(home.rglob('release-*.png'))
        for shot in shots:
            (output / shot.name).write_bytes(shot.read_bytes())
        row.update(command=' '.join(command[1:]), screenshots=[dict(name=s.name, sha256=sha(s)) for s in shots],
                   errors=re.findall(r'^ERROR: [^\r\n]*', text, re.M)[:3],
                   modules=sorted(set(re.findall(r"^VM_LoadDLL '([^']+)' ok|^Loading vm file ([^\r\n.]+)", text, re.M))))
        report['clients'][profile] = row
        if row['exitCode'] != 0 or len(shots) != 1 or row['errors']:
            report['failures'].append(f'{profile} client')
        print(profile, 'client', row['exitCode'], [s['name'] for s in row['screenshots']], row['errors'][:1], flush=True)
        if not server_map:
            continue
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        home = output / f'{profile}-server'
        (home / game).mkdir(parents=True)
        (home / game / 'release.cfg').write_text('wait 400\nquit\n', encoding='utf-8')
        command = [str(server)]
        for key, value in dict(com_gameProfile=profile, fs_basepath=str(roots[root]), fs_homepath=str(home),
                               dedicated='1', net_enabled='1', net_ip='127.0.0.1', net_ip6='::1',
                               net_port=str(port), sv_pure='0', bot_enable='0',
                               logfile='2', sv_punkbuster='0', **extra,
                               **{f'sv_master{i}': '' for i in range(1, 6)}).items():
            command += ['+set', key, value]
        command += server_map + ['+net_port', '+exec', 'release.cfg']
        row = run(command, server.parent, output / f'{profile}-server-stdout.log', 120,
                  lambda: getstatus(port, PROTOCOLS[profile], server_map[1]))
        log = home / game / 'qconsole.log'
        text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
        (output / f'{profile}-server-console.log').write_text(text, encoding='utf-8')
        row.update(command=' '.join(command[1:]), expectedPort=port, observedPort=observed_port(text),
                   errors=re.findall(r'^ERROR: [^\r\n]*', text, re.M)[:3])
        report['servers'][profile] = row
        if (row['exitCode'] != 0 or not row['answeredGetstatus'] or row['errors']
                or row['observedPort'] != port):
            report['failures'].append(f'{profile} server')
        print(profile, 'server', row['exitCode'], row['answeredGetstatus'], row['errors'][:1], flush=True)
    report['passed'] = not report['failures']
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('passed' if report['passed'] else f"failed: {report['failures']}")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
