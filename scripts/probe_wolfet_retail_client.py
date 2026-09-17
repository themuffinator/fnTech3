#!/usr/bin/env python3
"""Bounded inbound ET retail-client interoperability, without input automation.

Windows only. Runs windowed on unswitched private desktops with isolated homes.
This verifies a non-pure connection and optional console-driven team changes,
not gameplay or rendering parity.
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


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def command(executable, installation, home, settings, commands):
    values = dict(fs_basepath=str(installation), fs_homepath=str(home),
                  r_fullscreen='0', in_mouse='0', in_joystick='0', in_nograb='1',
                  logfile='2', com_viewlog='0', **settings)
    result = [str(executable)]
    for name, value in values.items():
        result.extend(['+set', name, value])
    return result + commands


def probe(server, installation, output, team_check=False, camera_check=False):
    output.mkdir(parents=True, exist_ok=False)
    server_home, client_home = output/'server', output/'client'
    server_home.mkdir(); client_home.mkdir()
    client = installation/'et.exe'
    before = inventory(installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    server_command = command(server, installation, server_home, dict(
        com_gameProfile='wolfet', dedicated='1', developer='1', net_ip='127.0.0.1',
        net_port=str(port), sv_pure='0', sv_punkbuster='0', bot_enable='0',
        sv_master1='', sv_master2='', sv_master3='', sv_master4='', sv_master5=''),
        ['+map', 'goldrush', '+wait', '1100', '+quit'])
    client_actions = ['+connect', f'127.0.0.1:{port}']
    if camera_check:
        client_actions += ['+wait', '300', '+openlimbomenu', '+wait', '30',
                           '+obj', '0', '+wait', '90', '+obj', '1', '+wait', '90',
                           '+openlimbomenu', '+wait', '240', '+quit']
    elif team_check:
        client_actions += ['+wait', '300', '+team', 'axis', '0', '3', '2',
                           '+wait', '300', '+team', 'allies', '0', '8', '7',
                           '+wait', '150', '+quit']
    else:
        client_actions += ['+wait', '750', '+quit']
    client_command = command(client, installation, client_home, dict(
        com_maxfps='30', s_initsound='0', r_mode='3', name='fntech3-et-probe',
        cl_punkbuster='0', cl_allowDownload='0', cl_wwwDownload='0',
        r_ignorehwgamma='1', developer='1', cl_showServerCommands='1' if camera_check else '0'),
        client_actions)
    result = dict(evidence='Interop: inbound, non-pure team admission' if team_check else 'Interop: inbound, non-pure spectator only',
                  profile='wolfet', serverCommand=server_command, clientCommand=client_command,
                  serverSha256=hashlib.sha256(server.read_bytes()).hexdigest(),
                  clientSha256=hashlib.sha256(client.read_bytes()).hexdigest(),
                  moduleSha256=hashlib.sha256((installation/'etmain/qagame_mp_x86.dll').read_bytes()).hexdigest(),
                  passed=False)
    if camera_check:
        result['evidence'] = 'Interop: inbound, non-pure objective-camera requests and replies'
    server_log = server_home/'etmain/qconsole.log'
    read_log = lambda: server_log.read_text(encoding='utf-8', errors='replace') if server_log.exists() else ''
    processes = []
    begin = time.monotonic()
    try:
        host = WindowsPrivateProcess(server_command, server.parent, output/'server-stdout.log')
        processes.append(host)
        while time.monotonic()-begin < 12 and host.poll() is None:
            if 'Setting Allied autospawn' in read_log():
                break
            time.sleep(0.1)
        if 'Setting Allied autospawn' not in read_log():
            raise RuntimeError('Dedicated map initialization failed')
        peer = WindowsPrivateProcess(client_command, client.parent, output/'client-stdout.log')
        processes.append(peer)
        while time.monotonic()-begin < 55 and peer.poll() is None:
            time.sleep(0.1)
        result['clientExit'] = peer.poll()
        while time.monotonic()-begin < 60 and host.poll() is None:
            time.sleep(0.1)
        result['serverExit'] = host.poll()
    except (OSError, RuntimeError) as error:
        result['error'] = str(error)
    finally:
        for process in reversed(processes):
            process.close()
    log = read_log()
    result.update(installationUnchanged=before == inventory(installation),
                  elapsedSeconds=round(time.monotonic()-begin, 3),
                  active='Going from CS_PRIMED to CS_ACTIVE for fntech3-et-probe' in log,
                  clientBegin='ClientBegin: 0' in log,
                  disconnected='Going from CS_ACTIVE to CS_ZOMBIE for fntech3-et-probe' in log)
    if team_check:
        result['teamAdmission'] = {name: f'\\t\\{number}\\c\\0' in log
                                   for name, number in (('axis', 1), ('allies', 2))}
    if camera_check:
        client_log = client_home/'etmain/etconsole.log'
        client_text = client_log.read_text(encoding='utf-8', errors='replace') if client_log.exists() else ''
        replies = set(re.findall(r'\bserverCommand:\s+\d+\s*:\s+portalcampos\s+([01])\b', client_text))
        result['cameraReplies'] = {objective: objective in replies for objective in ('0', '1')}
    result['passed'] = bool(result.get('clientExit') == 0 and result.get('serverExit') == 0
                            and result['active'] and result['clientBegin'] and result['disconnected']
                            and result['installationUnchanged'] and 'ERROR:' not in log
                            and all(result.get('teamAdmission', {}).values())
                            and all(result.get('cameraReplies', {}).values()))
    (output/'result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if not k.endswith('Command')}, indent=2))
    return result['passed']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    checks = parser.add_mutually_exclusive_group()
    checks.add_argument('--team-check', action='store_true',
                        help='issue console commands to join Axis and Allies; no mouse/keyboard input')
    checks.add_argument('--camera-check', action='store_true',
                        help='open the limbo panel and verify replies for two objective cameras; no visual parity claim')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This retail Windows client probe requires Windows')
    raise SystemExit(0 if probe(args.server.resolve(strict=True), args.installation.resolve(strict=True),
                               args.output.resolve(), args.team_check, args.camera_check) else 1)
