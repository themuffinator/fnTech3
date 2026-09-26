#!/usr/bin/env python3
"""Bounded inbound RTCW MP retail-client interoperability, without input automation.

Windows only. Runs windowed on unswitched private desktops with isolated homes.
This verifies a connection, optionally pure, and records the pure systeminfo the
server published for that run. Console commands only: no mouse or keyboard input
is injected, and no rendering parity is claimed.

The pure run exists to answer one question: retail ET faults when a pure server
publishes `sv_paks` with an empty `sv_pakNames`, because ET's client dereferences
the stored names in FS_VerifyOfficialPaks. RTCW MP has no such consumer, so this
probe records whether a retail RTCW MP client admits under the same systeminfo.
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


# An access violation leaves this exit status; retail ET produced it when a pure
# server published checksums with no names.
STATUS_ACCESS_VIOLATION = 0xC0000005


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


def published(text, key):
    """Recover a cvar the dedicated console printed for this run."""
    match = re.search(r'^"%s" is:"(.*?)(?:\^7)?"' % re.escape(key), text, re.MULTILINE)
    return match.group(1) if match else None


def probe(server, installation, output, pure=False, team_check=False, map_name='mp_beach', gametype=None):
    output.mkdir(parents=True, exist_ok=False)
    server_home, client_home = output/'server', output/'client'
    server_home.mkdir(); client_home.mkdir()
    client = installation/'WolfMP.exe'
    before = inventory(installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    # Printing the two pure keys after the map settles records the exact
    # systeminfo condition the retail client was then asked to accept.
    server_command = command(server, installation, server_home, dict(
        com_gameProfile='rtcw-mp', dedicated='1', developer='1', net_ip='127.0.0.1',
        net_port=str(port), sv_pure='1' if pure else '0', sv_punkbuster='0', bot_enable='0',
        sv_master1='', sv_master2='', sv_master3='', sv_master4='', sv_master5='',
        **({'g_gametype': str(gametype)} if gametype is not None else {})),
        ['+map', map_name, '+wait', '100', '+sv_paks', '+sv_pakNames', '+sv_pure',
         '+wait', '1000', '+quit'])
    client_actions = ['+connect', f'127.0.0.1:{port}']
    if team_check:
        client_actions += ['+wait', '300', '+team', 'r', '+wait', '300',
                           '+team', 'b', '+wait', '150', '+quit']
    else:
        client_actions += ['+wait', '750', '+quit']
    client_command = command(client, installation, client_home, dict(
        com_maxfps='30', s_initsound='0', r_mode='3', name='fntech3-rtcw-probe',
        cl_punkbuster='0', cl_allowDownload='0', r_ignorehwgamma='1', developer='1',
        # WolfMP.exe's main menu checks au2rtcw*.activision.com for an update
        # and asks wolfmotd.idsoftware.com for its MOTD, as connect does.
        cl_autoupdate='0', cl_motd='0'),
        client_actions)
    kind = 'pure' if pure else 'non-pure'
    result = dict(evidence=f'Interop: inbound, {kind} '
                           + ('team admission' if team_check else 'spectator only'),
                  profile='rtcw-mp', map=map_name, serverCommand=server_command, clientCommand=client_command,
                  serverSha256=hashlib.sha256(server.read_bytes()).hexdigest(),
                  clientSha256=hashlib.sha256(client.read_bytes()).hexdigest(),
                  moduleSha256=hashlib.sha256((installation/'Main/qagame_mp_x86.dll').read_bytes()).hexdigest(),
                  passed=False)
    server_log = server_home/'main/qconsole.log'
    read_log = lambda: server_log.read_text(encoding='utf-8', errors='replace') if server_log.exists() else ''
    processes = []
    begin = time.monotonic()
    try:
        host = WindowsPrivateProcess(server_command, server.parent, output/'server-stdout.log')
        processes.append(host)
        while time.monotonic()-begin < 30 and host.poll() is None:
            if '"sv_pure" is:' in read_log():
                break
            time.sleep(0.1)
        if '"sv_pure" is:' not in read_log():
            raise RuntimeError('Dedicated map initialization failed')
        peer = WindowsPrivateProcess(client_command, client.parent, output/'client-stdout.log')
        processes.append(peer)
        deadline = 75
        while time.monotonic()-begin < deadline and peer.poll() is None:
            time.sleep(0.1)
        result['clientExit'] = peer.poll()
        while time.monotonic()-begin < deadline+10 and host.poll() is None:
            time.sleep(0.1)
        result['serverExit'] = host.poll()
    except (OSError, RuntimeError) as error:
        result['error'] = str(error)
    finally:
        for process in reversed(processes):
            process.close()
    log = read_log()
    # The retail MP client names its own log after the game, not qconsole.log.
    client_log = client_home/'main/rtcwconsole.log'
    client_text = client_log.read_text(encoding='utf-8', errors='replace') if client_log.exists() else ''
    exit_status = result.get('clientExit')
    result.update(installationUnchanged=before == inventory(installation),
                  elapsedSeconds=round(time.monotonic()-begin, 3),
                  active='Going from CS_PRIMED to CS_ACTIVE for fntech3-rtcw-probe' in log,
                  clientBegin='ClientBegin: 0' in log,
                  disconnected='Going from CS_ACTIVE to CS_ZOMBIE for fntech3-rtcw-probe' in log,
                  # The exact systeminfo the client was asked to accept.
                  publishedPaks=published(log, 'sv_paks'),
                  publishedPakNames=published(log, 'sv_pakNames'),
                  publishedPure=published(log, 'sv_pure'),
                  clientAccessViolation=(exit_status is not None
                                         and (exit_status & 0xFFFFFFFF) == STATUS_ACCESS_VIOLATION))
    if pure:
        # Under sv_pure the client cannot enter the world until the server has
        # accepted its cp proof, so reaching CS_ACTIVE without an unpure drop is
        # the admission evidence. The client's own line records that it took the
        # pure path at all rather than silently skipping it.
        result['pureAdmission'] = (result['active'] and 'Unpure client detected' not in log)
        # Without these the client never took the pure path and the run would
        # prove nothing about its handling of the missing names.
        result['clientSawPureServer'] = 'Connected to a pure server' in client_text
        result['clientPureListEntries'] = client_text.count('on the pure list')
        # Retail RTCW MP has no FS_VerifyOfficialPaks; ET's failure printed this.
        result['officialPakError'] = 'official pak file' in client_text
    if team_check:
        result['teamAdmission'] = {name: f'\\t\\{number}\\' in log
                                   for name, number in (('axis', 1), ('allies', 2))}
    result['passed'] = bool(result.get('clientExit') == 0 and result.get('serverExit') == 0
                            and result['active'] and result['clientBegin'] and result['disconnected']
                            and result['installationUnchanged'] and not result['clientAccessViolation']
                            and 'ERROR:' not in log
                            and not result.get('officialPakError', False)
                            and all(result.get('teamAdmission', {}).values())
                            and result.get('pureAdmission', True)
                            and result.get('clientSawPureServer', True)
                            and result.get('clientPureListEntries', 1) > 0)
    (output/'result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if not k.endswith('Command')}, indent=2))
    return result['passed']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail RTCW root holding WolfMP.exe and Main/')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pure', action='store_true',
                        help='run the server with sv_pure 1 and require the client to pass pure admission')
    parser.add_argument('--team-check', action='store_true',
                        help='issue console commands to join both teams; no mouse/keyboard input')
    parser.add_argument('--map', default='mp_beach', help='server map; an installed RTCW MP map')
    parser.add_argument('--gametype', type=int, help='server g_gametype (5 objective, 6 stopwatch, 7 checkpoint)')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This retail Windows client probe requires Windows')
    raise SystemExit(0 if probe(args.server.resolve(strict=True), args.installation.resolve(strict=True),
                               args.output.resolve(), args.pure, args.team_check, args.map, args.gametype) else 1)
