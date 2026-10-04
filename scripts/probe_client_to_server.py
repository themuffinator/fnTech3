#!/usr/bin/env python3
"""Bounded client-to-server admission: an fnTech3 client joins a dedicated server.

The server is retail RTCW WolfMP.exe or ET etded.exe (--retail-server), or an
fnTech3 dedicated executable (--server), for rtcw-mp, wolfet or q3. Windows only:
both processes run windowed on unswitched private desktops with isolated homes,
loopback only, with sound and mouse/joystick input disabled. The client acts
through console commands in its activeAction; no mouse or keyboard input is
injected and no operating-system capture is taken. The server's own console log
is the admission evidence and the client's engine screenshots show what it drew.

The connection uses 127.0.0.1 over UDP, so the full getchallenge /
challengeResponse / connect / connectResponse exchange runs; a loopback
listen-server session would skip it. --pure runs the server with sv_pure 1, so
the client enters the world only after its checksum proof is accepted.
--team-check (rtcw-mp or wolfet) joins Axis then Allies with the game's team
command, and --only-visible sets sv_onlyVisibleClients 1 on the server. This is
not a sustained-gameplay or rendering-parity claim.
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

NAME = 'fntech3-handshake-probe'
ENGINE_ERRORS = re.compile(r'^(?:ERROR:|.*Illegible server message|.*read past end of server message|'
                           r'.*Server fatal crashed|.*recursive error after)', re.M)
PROFILES = {
    'rtcw-mp': dict(game='main', map='mp_beach', module='Main/qagame_mp_x86.dll',
                    retailExecutable='WolfMP.exe', retailLog='rtcwconsole.log',
                    settings=dict(g_gametype='5', bot_enable='0', sv_punkbuster='0')),
    'wolfet': dict(game='etmain', map='railgun', module='etmain/qagame_mp_x86.dll',
                   retailExecutable='etded.exe', retailLog='etconsole.log',
                   settings=dict(g_gametype='2', bot_enable='0', sv_punkbuster='0')),
    'q3': dict(game='baseq3', map='q3dm17', module='baseq3/pak0.pk3',
               settings=dict(g_gametype='0', bot_enable='0')),
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def read(path):
    return path.read_text(encoding='utf-8', errors='replace') if path.exists() else ''


def probe(args, output):
    profile = PROFILES[args.profile]
    output.mkdir(parents=True, exist_ok=False)
    server_home, client_home = output / 'server-home', output / 'client-home'
    server_home.mkdir(); client_home.mkdir()
    retail = args.retail_server
    server = args.installation / profile['retailExecutable'] if retail else args.server
    before = inventory(args.installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    values = dict(fs_basepath=str(args.installation), fs_homepath=str(server_home), dedicated='1',
                  developer='1', logfile='2', net_ip='127.0.0.1', net_port=str(port), r_fullscreen='0',
                  sv_pure='1' if args.pure else '0',
                  **{f'sv_master{i}': '' for i in range(1, 6)}, **profile['settings'])
    if not retail:
        values = dict(com_gameProfile=args.profile, **values)
    if args.only_visible:
        values['sv_onlyVisibleClients'] = '1'
    if args.gametype is not None:
        values['g_gametype'] = str(args.gametype)
    server_command = [str(server)]
    for name, value in values.items():
        server_command += ['+set', name, value]
    server_command += ['+map', args.map or profile['map']]
    shots = 'wait 60; screenshotPNG handshake-{0}; '
    if args.team_check:
        action = ('wait 200; team r; wait 240; CloseLimboMenu; -scores; ' + shots.format('axis') +
                  'team b; wait 240; CloseLimboMenu; -scores; ' + shots.format('allies') + 'wait 30; quit')
    elif args.profile in ('rtcw-mp', 'wolfet'):
        action = 'wait 200; CloseLimboMenu; -scores; ' + shots.format('world') + 'wait 30; quit'
    else:
        action = 'wait 200; ' + shots.format('world') + 'wait 30; quit'
    game = client_home / profile['game']
    game.mkdir()
    (game / 'autoexec.cfg').write_text(''.join(f'set {k} "{v}"\n' for k, v in dict(
        com_introplayed='1', com_maxfps='60', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
        cl_allowDownload='0', cl_motd='0', name=NAME, activeAction=action).items()), encoding='utf-8')
    client_command = [str(args.client)]
    for name, value in dict(com_gameProfile=args.profile, fs_basepath=str(args.installation),
                            fs_homepath=str(client_home), fs_game='', fs_cdpath='', r_fullscreen='0',
                            r_mode='-1', r_customWidth='800', r_customHeight='600',
                            in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0',
                            com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                            cl_renderer='glx', logfile='2', developer='1', net_ip='127.0.0.1',
                            net_port='0', net_ip6='::1', net_port6='0').items():
        client_command += ['+set', name, value]
    client_command += ['+connect', f'127.0.0.1:{port}']
    result = dict(evidence=('Interop: outbound to retail' if retail else 'Live: fnTech3 client and server')
                  + (', pure' if args.pure else ', non-pure')
                  + (', team admission' if args.team_check else '')
                  + (', sv_onlyVisibleClients 1' if args.only_visible else ''),
                  profile=args.profile, map=args.map or profile['map'], pure=args.pure,
                  retailServer=retail, serverCommand=server_command, clientCommand=client_command,
                  clientAction=action, clientSha256=sha(args.client), serverSha256=sha(server),
                  moduleSha256=sha(args.installation / profile['module']), passed=False)
    server_log = server_home / profile['game'] / (profile['retailLog'] if retail else 'qconsole.log')
    host = peer = None
    begin = time.monotonic()
    try:
        host = WindowsPrivateProcess(server_command, server.parent, output / 'server-stdout.log')
        # Retail applies its settings and restarts once before its items settle.
        while time.monotonic() - begin < 60 and host.poll() is None:
            text = read(server_log)
            ready = ('sv_serverRestarting 0' in text if args.profile == 'wolfet' and retail
                     else bool(re.search(r'^\d+ items registered', text, re.M)))
            if ready and (not retail or 'sv_serverRestarting 0' in text):
                break
            time.sleep(0.2)
        time.sleep(2)
        result['serverReadySeconds'] = round(time.monotonic() - begin, 3)
        peer = WindowsPrivateProcess(client_command, args.client.parent, output / 'client-stdout.log')
        deadline = time.monotonic() + (90 if args.team_check else 60)
        while time.monotonic() < deadline and peer.poll() is None:
            time.sleep(0.2)
        result['clientExit'] = peer.poll()
        result['clientForced'] = peer.poll() is None
    except (OSError, RuntimeError) as error:
        result['error'] = str(error)
    finally:
        for process in (peer, host):
            if process:
                process.close()
    server_text = read(server_log)
    client_text = read(game / 'qconsole.log')
    (output / 'server-console.log').write_text(server_text, encoding='utf-8')
    (output / 'client-console.log').write_text(client_text, encoding='utf-8')
    captures = []
    for capture in sorted(client_home.rglob('handshake-*.png')):
        target = output / capture.name
        target.write_bytes(capture.read_bytes())
        captures.append(dict(name=target.name, sha256=sha(target)))
    replies = re.findall(r'^CL packet [^:]+:\d+: ((?:challengeResponse|connectResponse)[^\n]*)', client_text, re.M)
    result.update(
        installationUnchanged=before == inventory(args.installation),
        elapsedSeconds=round(time.monotonic() - begin, 3),
        handshakeReplies=replies,
        serverActive=f'Going from CS_PRIMED to CS_ACTIVE for {NAME}' in server_text,
        serverBegin=bool(re.search(r'ClientBegin: \d+', server_text)),
        unpureDrop='Unpure client detected' in server_text,
        rejectedConnect=re.findall(r'rejected connect[^\n]*', server_text)[:3],
        clientErrors=ENGINE_ERRORS.findall(client_text)[:10],
        clientSawPureServer='Connected to a pure server' in client_text,
        clientHidesUnseenPlayers='Server hides players this client cannot see' in client_text,
        screenshots=captures)
    if args.team_check:
        result['teamAdmission'] = {name: bool(re.search(r'ClientUserinfoChanged: \d+ n\\%s\\t\\%d\\' % (
            re.escape(NAME), number), server_text)) for name, number in (('axis', 1), ('allies', 2))}
    expected_shots = 2 if args.team_check else 1
    result['passed'] = bool(result.get('clientExit') == 0 and not result.get('clientForced')
                            and result['serverActive'] and result['serverBegin']
                            and not result['unpureDrop'] and not result['rejectedConnect']
                            and not result['clientErrors'] and result['installationUnchanged']
                            and len(captures) == expected_shots
                            and (not args.pure or result['clientSawPureServer'])
                            and (not args.only_visible or result['clientHidesUnseenPlayers'])
                            and all(result.get('teamAdmission', {}).values()))
    result['sourceCommit'] = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                            capture_output=True).stdout.strip()
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if not k.endswith('Command')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True, help='fnTech3 client executable')
    servers = parser.add_mutually_exclusive_group(required=True)
    servers.add_argument('--server', type=Path, help='fnTech3 dedicated executable')
    servers.add_argument('--retail-server', action='store_true', help='run retail RTCW WolfMP.exe or ET etded.exe as the server')
    parser.add_argument('--profile', choices=tuple(PROFILES), default='rtcw-mp')
    parser.add_argument('--installation', type=Path, required=True,
                        help='legitimate installation root for the profile')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--map', help='server map; defaults to mp_beach, railgun or q3dm17 by profile')
    parser.add_argument('--pure', action='store_true', help='run the server with sv_pure 1')
    parser.add_argument('--gametype', type=int, help='server g_gametype; defaults to the profile probe mode')
    parser.add_argument('--team-check', action='store_true', help='rtcw-mp or wolfet: join Axis then Allies')
    parser.add_argument('--only-visible', action='store_true', help='rtcw-mp or wolfet: server sets sv_onlyVisibleClients 1')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    if args.profile not in ('rtcw-mp', 'wolfet') and (args.retail_server or args.team_check or args.only_visible):
        parser.error('--retail-server, --team-check and --only-visible require rtcw-mp or wolfet')
    args.client = args.client.resolve(strict=True)
    args.installation = args.installation.resolve(strict=True)
    if args.server:
        args.server = args.server.resolve(strict=True)
    return 0 if probe(args, args.output.resolve()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
