#!/usr/bin/env python3
"""What a connected client sees when map_restart follows a variable change.

Windows only. Runs a dedicated server, fnTech3's (--server) or the retail one
(--retail-server: WolfMP.exe for rtcw-mp, etded.exe for wolfet), and one retail
client (WolfMP.exe or et.exe) that connects to it over UDP loopback. Each runs
windowed on its own unswitched private desktop with an isolated home, and no
mouse or keyboard input is injected. Once the client is in the game, the probe
drives the server over rcon: each step is a config in the server's home that
rcon execs, so the step's own output reaches the server log. After a step
settles, the probe has the server `say` a marker until the client's log shows
it, so every step ends with the client back in the game. An RTCW MP run first
waits for the game's own warmup restart, which starts play once a player has
joined.

Two plans. `restarts` starts pure, so the client has proved its paks before
anything changes, then restarts after a new game type, after sv_pure 0 and
after a new sv_maxclients. `pure-live` starts non-pure and turns sv_pure on
with no restart, then restarts. For each step the probe cuts both logs at the
step's boundaries and counts, on the server, level spawns, game
initializations, gamestates sent to the client and the exact restart messages;
and on the client, the server commands it executed (cl_showServerCommands),
the game type its serverinfo updates carried, the cgame's own map_restart
handling (cg_showmiss) and the gamestates it loaded (CL_InitCGame). The run
passes when every step completed, the client stayed connected and the
installation is unchanged. It makes no claim about what was drawn.
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

# retailDedicated: the retail server is the client executable run with
# dedicated 1, rather than a separate dedicated executable.
PROFILES = {
    'rtcw-mp': dict(game='main', client='WolfMP.exe', retail='WolfMP.exe', retailDedicated=True,
                    retailLog='rtcwconsole.log', clientLog='rtcwconsole.log',
                    module='Main/qagame_mp_x86.dll', map='mp_beach', gameTypes=('5', '6'),
                    name='fntech3-restart-probe', warmupRestart=True),
    # One player keeps ET's game in its countdown, so it never restarts itself.
    'wolfet': dict(game='etmain', client='et.exe', retail='etded.exe', retailDedicated=False,
                   retailLog='etconsole.log', clientLog='etconsole.log',
                   module='etmain/qagame_mp_x86.dll', map='goldrush', gameTypes=('2', '3'),
                   name='fntech3-restart-probe', warmupRestart=False),
}
MAX_CLIENTS = '12'
# The server's starting sv_pure, then each step's console commands.
PLANS = {
    'restarts': ('1', [
        ('game-type', ['g_gametype {next}', 'map_restart']),
        ('pure-off', ['sv_pure 0', 'map_restart']),
        ('max-clients', ['sv_maxclients %s' % MAX_CLIENTS, 'map_restart']),
    ]),
    # A client that joined a non-pure server has never sent a pure proof.
    'pure-live': ('0', [
        ('pure-on', ['sv_pure 1']),
        ('pure-restart', ['map_restart']),
    ]),
}
PASSWORD = 'fntech3-restart-probe'
OOB = bytes([255, 255, 255, 255])


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def settings_command(executable, values, actions):
    result = [str(executable)]
    for name, value in values.items():
        result += ['+set', name, value]
    return result + actions


class Rcon:
    """One rcon command at a time; waits for the server's reply to each."""

    def __init__(self, port):
        self.address = ('127.0.0.1', port)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('127.0.0.1', 0))
        self.socket.settimeout(0.2)
        self.last = 0.0

    def close(self):
        self.socket.close()

    def post(self, command):
        """Send once without waiting: fnTech3 replies only when the
        command printed something, and say prints nothing there."""
        time.sleep(max(0.0, 0.6 - (time.monotonic() - self.last)))
        self.socket.sendto(OOB + ('rcon %s %s' % (PASSWORD, command)).encode('ascii'), self.address)
        self.last = time.monotonic()

    def send(self, command, timeout=20.0, resend=3.0):
        """Send until a reply arrives. A command that must not run twice
        passes a resend interval longer than its timeout."""
        packet = OOB + ('rcon %s %s' % (PASSWORD, command)).encode('ascii')
        # Drop a late reply to an earlier command.
        while True:
            try:
                self.socket.recvfrom(65536)
            except ConnectionResetError:
                continue
            except socket.timeout:
                break
        begin = time.monotonic()
        sent = None
        while time.monotonic() - begin < timeout:
            now = time.monotonic()
            # Retail rcon answers one command per half second, server-wide.
            if sent is None or now - sent > resend:
                time.sleep(max(0.0, 0.6 - (now - self.last)))
                self.socket.sendto(packet, self.address)
                sent = self.last = time.monotonic()
            try:
                data, _ = self.socket.recvfrom(65536)
            except (socket.timeout, ConnectionResetError):
                # Windows reports a packet that found no listener yet as a
                # reset on the next read.
                continue
            if data.startswith(OOB + b'print'):
                reply = data[len(OOB) + 6:]
                # A long reply arrives in several packets.
                while True:
                    try:
                        more, _ = self.socket.recvfrom(65536)
                    except socket.timeout:
                        break
                    if more.startswith(OOB + b'print'):
                        reply += more[len(OOB) + 6:]
                return reply.decode('ascii', 'replace')
        raise TimeoutError('no rcon reply to %r' % command)


def read(path):
    return path.read_bytes() if path.exists() else b''


def count(text, needle):
    return text.count(needle)


def printed(text, name):
    matches = re.findall(r'^"%s" is:"(.*?)(?:\^7)?"' % re.escape(name), text, re.M)
    return matches[-1] if matches else None


def summarize_server(text, name):
    return dict(
        serverInitializations=count(text, '------ Server Initialization ------'),
        gameInitializations=count(text, '------- Game Initialization -------'),
        restartMessages=[line.strip() for line in text.splitlines() if 'variable change -- restarting' in line],
        gamestatesSent=count(text, 'SV_SendClientGameState() for %s' % name),
        pureGamestateResends=count(text, "%s: didn't get cp command, resending gamestate" % name),
        unpureDrops=count(text, 'Cannot validate pure client') + count(text, 'Unpure client detected'),
        serverCrashed='Server crashed' in text or 'Unhandled exception' in text,
        g_gametype=printed(text, 'g_gametype'), sv_pure=printed(text, 'sv_pure'),
        sv_maxclients=printed(text, 'sv_maxclients'))


def summarize_client(text, marker):
    commands = re.findall(r'^serverCommand: \d+ : (.*)$', text, re.M)
    names = [command.split(' ', 1)[0] for command in commands]
    # CS_SYSTEMINFO is configstring 1 in both games; the key shows the pure
    # mode the server published to this client.
    pure = re.findall(r'^serverCommand: \d+ : cs 1 ".*?\\sv_pure\\(\d+)', text, re.M)
    # CS_SERVERINFO, configstring 0, carries the game type in place.
    game_types = re.findall(r'^serverCommand: \d+ : cs 0 ".*?\\g_gametype\\(\d+)', text, re.M)
    return dict(
        serverCommands=sorted(set(names)),
        serverInfoGameTypes=game_types,
        mapRestartCommands=names.count('map_restart'),
        cgameMapRestarts=count(text, 'CG_MapRestart'),
        gamestatesLoaded=count(text, 'CL_InitCGame:'),
        systemInfoPure=pure,
        disconnected=any(s in text for s in ('Server disconnected', 'was kicked', 'Cannot validate pure client',
                                             'Unpure client detected', 'ERROR:')),
        markerSeen=marker in text)


def probe(args, output):
    profile = PROFILES[args.profile]
    output.mkdir(parents=True, exist_ok=False)
    server_home, client_home = output / 'server', output / 'client'
    (server_home / profile['game']).mkdir(parents=True)
    client_home.mkdir()
    installation = args.installation
    before = inventory(installation)
    client = installation / profile['client']
    retail = args.retail_server
    server = installation / profile['retail'] if retail else args.server
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    start_type, next_type = profile['gameTypes']
    start_pure, plan = PLANS[args.plan]
    steps = [(name, [command.format(next=next_type) for command in commands]) for name, commands in plan]
    for index, (name, commands) in enumerate(steps):
        lines = ['echo fntech3-step-%s' % name] + commands
        lines += ['wait 20', 'g_gametype', 'sv_pure', 'sv_maxclients']
        (server_home / profile['game'] / ('fntech3-step%d.cfg' % index)).write_text(
            ''.join(line + '\n' for line in lines), encoding='ascii')
    values = dict(fs_basepath=str(installation), fs_homepath=str(server_home), developer='1',
                  logfile='2', net_ip='127.0.0.1', net_port=str(port), sv_pure=start_pure,
                  sv_punkbuster='0', bot_enable='0', g_gametype=start_type,
                  rconpassword=PASSWORD, **{'sv_master%d' % i: '' for i in range(1, 6)})
    if not retail:
        values = dict(com_gameProfile=args.profile, dedicated='1', **values)
    elif profile['retailDedicated']:
        values = dict(dedicated='1', **values)
    server_command = settings_command(server, values, ['+map', profile['map']])
    client_command = settings_command(client, dict(
        fs_basepath=str(installation), fs_homepath=str(client_home), r_fullscreen='0',
        in_mouse='0', in_joystick='0', in_nograb='1', logfile='2', com_viewlog='0',
        com_maxfps='30', s_initsound='0', r_mode='3', name=profile['name'],
        cl_punkbuster='0', cl_allowDownload='0', cl_wwwDownload='0', r_ignorehwgamma='1',
        # Retail menus ask Activision's update hosts and the MOTD master.
        cl_autoupdate='0', cl_motd='0', developer='1', cl_showServerCommands='1',
        cg_showmiss='1'), ['+connect', '127.0.0.1:%d' % port, '+wait', '9000', '+quit'])
    server_log = server_home / profile['game'] / (profile['retailLog'] if retail else 'qconsole.log')
    client_log = client_home / profile['game'] / profile['clientLog']
    result = dict(evidence='Interop: retail client connected to a %s dedicated server'
                  % ('retail' if retail else 'fnTech3'), profile=args.profile, plan=args.plan,
                  retailServer=retail,
                  map=profile['map'], serverCommand=server_command, clientCommand=client_command,
                  serverSha256=sha(server), clientSha256=sha(client),
                  moduleSha256=sha(installation / profile['module']), steps=[], passed=False)
    processes = []
    rcon = Rcon(port)
    begin = time.monotonic()
    try:
        host = WindowsPrivateProcess(server_command, server.parent, output / 'server-stdout.log')
        processes.append(host)
        rcon.send('echo fntech3-ready', timeout=60)
        peer = WindowsPrivateProcess(client_command, client.parent, output / 'client-stdout.log')
        processes.append(peer)
        entered = 'Going from CS_PRIMED to CS_ACTIVE for %s' % profile['name']
        while time.monotonic() - begin < 120 and entered not in read(server_log).decode('utf-8', 'replace'):
            if peer.poll() is not None or host.poll() is not None:
                raise RuntimeError('a process exited before the client entered the game')
            time.sleep(0.2)
        if entered not in read(server_log).decode('utf-8', 'replace'):
            raise RuntimeError('the client never entered the game')
        # RTCW MP's game counts a joined player down from warmup, then
        # restarts the level itself into play (gamestate 0). Let that restart
        # happen before the first step, so no step's logs hold it.
        if profile['warmupRestart']:
            settle_begin = time.monotonic()
            while time.monotonic() - settle_begin < 45:
                result['gamestateBeforeSteps'] = printed(rcon.send('gamestate'), 'gamestate')
                if result['gamestateBeforeSteps'] == '0':
                    break
                time.sleep(1)
            if result['gamestateBeforeSteps'] != '0':
                raise RuntimeError('the game never left its warmup')
        time.sleep(3)
        for index, (name, commands) in enumerate(steps):
            server_start, client_start = len(read(server_log)), len(read(client_log))
            step_begin = time.monotonic()
            rcon.send('exec fntech3-step%d.cfg' % index, resend=60)
            time.sleep(args.settle)
            marker = 'fntech3-marker-%s-unsent' % name
            for attempt in range(40):
                # A crashed server never passes a marker on.
                if b'Server crashed' in read(server_log)[server_start:]:
                    break
                marker = 'fntech3-marker-%s-%d' % (name, attempt)
                rcon.post('say %s' % marker)
                deadline = time.monotonic() + 2.0
                while time.monotonic() < deadline and marker.encode() not in read(client_log)[client_start:]:
                    time.sleep(0.1)
                if marker.encode() in read(client_log)[client_start:]:
                    break
            time.sleep(1)
            server_text = read(server_log)[server_start:].decode('utf-8', 'replace')
            client_text = read(client_log)[client_start:].decode('utf-8', 'replace')
            step = dict(step=name, commands=commands, seconds=round(time.monotonic() - step_begin, 3))
            step.update(summarize_server(server_text, profile['name']))
            step.update(summarize_client(client_text, marker))
            result['steps'].append(step)
            (output / ('step-%d-%s-server.log' % (index, name))).write_text(server_text, encoding='utf-8')
            (output / ('step-%d-%s-client.log' % (index, name))).write_text(client_text, encoding='utf-8')
            if not step['markerSeen']:
                raise RuntimeError('the client did not come back after step %s' % name)
        try:
            rcon.send('quit', timeout=3)
        except TimeoutError:
            pass
    except (OSError, RuntimeError, TimeoutError) as error:
        result['error'] = str(error)
    finally:
        rcon.close()
        for process in reversed(processes):
            result.setdefault('exits', []).append(process.poll())
            process.close()
    result['elapsedSeconds'] = round(time.monotonic() - begin, 3)
    result['serverShaAfter'] = sha(server)
    result['installationUnchanged'] = before == inventory(installation)
    result['passed'] = bool(len(result['steps']) == len(steps) and not result.get('error')
                            and all(s['markerSeen'] and not s['disconnected'] for s in result['steps'])
                            and result['installationUnchanged']
                            and result['serverShaAfter'] == result['serverSha256'])
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if not k.endswith('Command')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    parser.add_argument('--plan', choices=sorted(PLANS), required=True)
    servers = parser.add_mutually_exclusive_group(required=True)
    servers.add_argument('--server', type=Path, help='fnTech3 dedicated executable')
    servers.add_argument('--retail-server', action='store_true',
                         help='run the retail dedicated server from the installation')
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail root holding the client executable and the game directory')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--settle', type=float, default=5.0,
                        help='seconds to wait after each step before looking for the client')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This retail Windows client probe requires Windows')
    args.installation = args.installation.resolve(strict=True)
    if args.server:
        args.server = args.server.resolve(strict=True)
    return 0 if probe(args, args.output.resolve()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
