#!/usr/bin/env python3
"""What retail clients are told when a server drops one of them: RTCW MP and ET.

Windows only. Runs a dedicated server, fnTech3's (--server) or the
installation's retail one (--retail-server: WolfMP.exe for rtcw-mp, etded.exe
for wolfet), and two retail clients (WolfMP.exe or et.exe) that join it over
UDP loopback, each on its own unswitched private desktop with an isolated
home. No mouse or keyboard input is injected. The client to be dropped joins
first, so it holds slot 0; an observer joins after it. Once both are in the
game, the server kicks slot 0 from a config that rcon execs, so what the
server prints for the kick reaches its log. Both clients log every server
command they execute (developer 1 with cl_showServerCommands 1), byte for
byte: the observer's log holds the notice broadcast about the dropped client,
and the dropped client's log holds whatever reached it before it left.

The result answers two questions. noticeMatchesRetail: the observer received
exactly one notice about the dropped client, in the profile's retail form.
clientsIntact: the dropped client executed its disconnect command and neither
client failed with an error. The run passes when both hold, the server
logged the drop and the installation is unchanged. It makes no claim about
what was drawn.
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

# retailDedicated: the retail server is the client executable run with
# dedicated 1; etded.exe is a dedicated build and sets dedicated itself.
PROFILES = {
    'rtcw-mp': dict(game='main', client='WolfMP.exe', retail='WolfMP.exe', retailDedicated=True,
                    retailLog='rtcwconsole.log', clientLog='rtcwconsole.log',
                    module='Main/qagame_mp_x86.dll', map='mp_beach', gameType='5',
                    # WolfMP.exe's SV_DropClient pushes this format, 0x4eeea4, at 0x434f3d.
                    notice='print "[lof]{name}^7 [lon]{reason}\n"'),
    'wolfet': dict(game='etmain', client='et.exe', retail='etded.exe', retailDedicated=False,
                   retailLog='etconsole.log', clientLog='etconsole.log',
                   module='etmain/qagame_mp_x86.dll', map='goldrush', gameType='2',
                   # et.exe pushes this one at 0x441937, etded.exe at 0x427f57.
                   notice='cpm "{name}^7 {reason}\n"'),
}
# Each engine's notice format, to name the one that arrived.
FORMS = {'RTCW MP print with [lof] and [lon]': 'print "[lof]{name}^7 [lon]{reason}\n"',
         'ET cpm': 'cpm "{name}^7 {reason}\n"',
         'Quake III print': 'print "{name}^7 {reason}\n"'}
DROPPED = 'fntech3-drop-probe'
OBSERVER = 'fntech3-drop-observer'
PASSWORD = 'fntech3-drop-probe'
KICK = 'fntech3-drop-kick.cfg'
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


def read(path):
    if not path.exists():
        return ''
    return path.read_bytes().decode('utf-8', 'replace').replace('\r\n', '\n')


class Rcon:
    """Loopback rcon. Retail drops a command handled within half a second of
    the previous one, from anyone, so commands go out a second apart."""

    def __init__(self, port):
        self.address = ('127.0.0.1', port)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('127.0.0.1', 0))
        self.socket.settimeout(0.2)
        self.last = 0.0

    def close(self):
        self.socket.close()

    def post(self, command):
        """Send once. fnTech3 answers only a command that printed something."""
        time.sleep(max(0.0, 1.0 - (time.monotonic() - self.last)))
        self.socket.sendto(OOB + ('rcon %s %s' % (PASSWORD, command)).encode('ascii'), self.address)
        self.last = time.monotonic()

    def ask(self, command, timeout):
        """Send until a reply arrives; only for a command that prints and may run twice."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.post(command)
            wait = time.monotonic() + 1.0
            while time.monotonic() < wait:
                try:
                    data, _ = self.socket.recvfrom(65536)
                except (socket.timeout, ConnectionResetError):
                    # Windows reports a packet that found no listener yet as
                    # a reset on the next read.
                    continue
                if data.startswith(OOB + b'print'):
                    return data[len(OOB) + 6:].decode('ascii', 'replace')
        raise TimeoutError('no rcon reply to %r' % command)


def server_commands(text):
    """Every command a client logged, whole. `serverCommand: N : %s\\n` holds
    the command as it arrived: a quoted argument can carry newlines, and a
    newline straight after the line's own belongs to the command."""
    commands = []
    for match in re.finditer(r'^serverCommand: (\d+) : ', text, re.M):
        i, quoted = match.end(), False
        while i < len(text) and (quoted or text[i] != '\n'):
            quoted ^= text[i] == '"'
            i += 1
        trailing = text[i:i + 2] == '\n\n'
        commands.append(text[match.end():i] + ('\n' if trailing else ''))
    return commands


def notices(commands):
    return [c for c in commands if DROPPED in c and re.match(r'(print|cpm) "', c)]


def reason_in(template, notice):
    """The reason a notice gives, if it has this template's form."""
    pattern = re.escape(template.format(name=DROPPED, reason='\0')).replace('\\\0', '\0')
    match = re.fullmatch(pattern.replace('\0', '(.*)'), notice, re.S)
    return match.group(1) if match else None


def form(notice):
    for label, template in FORMS.items():
        if reason_in(template, notice) is not None:
            return label
    return None


def wait_for(condition, seconds, processes):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if condition():
            return True
        if any(process.poll() is not None for process in processes):
            return condition()
        time.sleep(0.2)
    return condition()


def client_command(executable, installation, home, name, port):
    return settings_command(executable, dict(
        fs_basepath=str(installation), fs_homepath=str(home), r_fullscreen='0',
        in_mouse='0', in_joystick='0', in_nograb='1', logfile='2', com_viewlog='0',
        com_maxfps='30', s_initsound='0', r_mode='3', name=name,
        cl_punkbuster='0', cl_allowDownload='0', cl_wwwDownload='0', r_ignorehwgamma='1',
        # Retail menus ask Activision's update hosts and the MOTD master.
        cl_autoupdate='0', cl_motd='0', developer='1', cl_showServerCommands='1',
        # A fresh home otherwise picks recommended video settings on its
        # first run and restarts the renderer, possibly mid-connection.
        com_recommendedSet='1'),
        ['+connect', '127.0.0.1:%d' % port])


def close_all(processes, result):
    # Every process is closed even when one is slow to end: a retail client
    # has taken over five seconds to die after TerminateProcess.
    for process in reversed(processes):
        result.setdefault('exits', []).append(process.poll())
        for _ in range(3):
            try:
                process.close()
                break
            except (OSError, subprocess.TimeoutExpired) as error:
                result.setdefault('closeErrors', []).append(str(error)[:200])
                time.sleep(5)


def probe(args, output):
    profile = PROFILES[args.profile]
    output.mkdir(parents=True, exist_ok=False)
    server_home = output / 'server'
    (server_home / profile['game']).mkdir(parents=True)
    (server_home / profile['game'] / KICK).write_text('clientkick 0\n', encoding='ascii')
    homes = {name: output / name for name in (DROPPED, OBSERVER)}
    for home in homes.values():
        home.mkdir()
    installation = args.installation
    before = inventory(installation)
    client = installation / profile['client']
    retail = args.retail_server
    server = installation / profile['retail'] if retail else args.server
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    values = dict(fs_basepath=str(installation), fs_homepath=str(server_home), developer='1',
                  logfile='2', net_ip='127.0.0.1', net_port=str(port), sv_pure='1',
                  sv_punkbuster='0', bot_enable='0', g_gametype=profile['gameType'],
                  rconpassword=PASSWORD, **{'sv_master%d' % i: '' for i in range(1, 6)})
    if not retail:
        values = dict(com_gameProfile=args.profile, dedicated='1', **values)
    elif profile['retailDedicated']:
        values = dict(dedicated='1', **values)
    server_command = settings_command(server, values, ['+map', profile['map']])
    commands = {name: client_command(client, installation, home, name, port) for name, home in homes.items()}
    server_log = server_home / profile['game'] / (profile['retailLog'] if retail else 'qconsole.log')
    logs = {name: home / profile['game'] / profile['clientLog'] for name, home in homes.items()}
    result = dict(evidence='Interop: two retail clients of a %s dedicated server, one dropped'
                  % ('retail' if retail else 'fnTech3'), profile=args.profile, retailServer=retail,
                  map=profile['map'], serverCommand=server_command, clientCommands=commands,
                  serverSha256=sha(server), clientSha256=sha(client),
                  moduleSha256=sha(installation / profile['module']), kicks=0, passed=False)
    processes = []
    rcon = Rcon(port)
    begin = time.monotonic()
    seen = 0
    heard = lambda: bool(notices(server_commands(read(logs[OBSERVER]))[seen:]))
    try:
        host = WindowsPrivateProcess(server_command, server.parent, output / 'server-stdout.log')
        processes.append(host)
        rcon.ask('g_gametype', timeout=60)
        # The client to be dropped joins first, so it holds slot 0.
        for name in (DROPPED, OBSERVER):
            processes.append(WindowsPrivateProcess(commands[name], client.parent,
                                                   output / ('%s-stdout.log' % name)))
            entered = 'Going from CS_PRIMED to CS_ACTIVE for %s' % name
            if not wait_for(lambda: entered in read(server_log), 120, processes):
                raise RuntimeError('%s never entered the game' % name)
        time.sleep(args.settle)
        seen = len(server_commands(read(logs[OBSERVER])))
        # An exec'd config runs after the rcon command has returned, so the
        # kick's own output reaches the server log, not the rcon reply. A kick
        # retail never handled is sent again; one it handled left nobody in
        # slot 0 to kick twice.
        for _ in range(3):
            rcon.post('exec %s' % KICK)
            result['kicks'] += 1
            if wait_for(heard, 15, processes[:1]):
                break
        if not heard():
            raise RuntimeError('the observer never heard of the drop')
        time.sleep(3)
        rcon.post('quit')
        wait_for(lambda: host.poll() is not None, 15, [])
    except (OSError, RuntimeError, TimeoutError) as error:
        result['error'] = str(error)
    finally:
        rcon.close()
        close_all(processes, result)
    result['elapsedSeconds'] = round(time.monotonic() - begin, 3)
    server_text = read(server_log)
    texts = {name: read(log) for name, log in logs.items()}
    heard_notices = notices(server_commands(texts[OBSERVER])[seen:])
    dropped_commands = server_commands(texts[DROPPED])
    reasons = [reason_in(profile['notice'], notice) for notice in heard_notices]
    result.update(
        observerNotices=heard_notices,
        observerNoticeForm=form(heard_notices[0]) if heard_notices else None,
        reason=reasons[0] if reasons else None,
        # What the observer's game printed once it had read the notice.
        observerPrinted=[line for line in texts[OBSERVER].splitlines()
                         if DROPPED in line and not line.startswith(('serverCommand:', 'Going ', 'Cvar_Set2'))],
        droppedNotices=notices(dropped_commands),
        droppedDisconnects=[c for c in dropped_commands if c.startswith('disconnect')],
        droppedErrors=[line for line in texts[DROPPED].splitlines()
                       if line.startswith('ERROR:') or 'Server Disconnected' in line],
        observerErrors=[line for line in texts[OBSERVER].splitlines() if line.startswith('ERROR:')],
        serverBroadcasts=[line for line in server_text.splitlines() if line.startswith('broadcast: ')],
        serverDropped=[line for line in server_text.splitlines() if 'CS_ZOMBIE for %s' % DROPPED in line],
        serverCrashed='Server crashed' in server_text or 'Unhandled exception' in server_text,
        installationUnchanged=before == inventory(installation),
        serverShaAfter=sha(server))
    # Two separate questions: whether the notice everyone else was sent has
    # the retail form, and whether both clients came through the drop as
    # retail's do, the dropped one told why it left and the observer still in.
    result['noticeMatchesRetail'] = bool(len(heard_notices) == 1 and reasons[0] is not None)
    result['clientsIntact'] = bool(len(result['droppedDisconnects']) == 1 and not result['observerErrors']
                                   and not any(line.startswith('ERROR:') for line in result['droppedErrors']))
    result['passed'] = bool(not result.get('error') and result['noticeMatchesRetail']
                            and result['clientsIntact']
                            and result['serverDropped'] and not result['serverCrashed']
                            and result['installationUnchanged']
                            and result['serverShaAfter'] == result['serverSha256'])
    (output / 'server.log').write_text(server_text, encoding='utf-8')
    for name, text in texts.items():
        (output / ('%s.log' % name)).write_text(text, encoding='utf-8')
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if not k.endswith(('Command', 'Commands'))}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    servers = parser.add_mutually_exclusive_group(required=True)
    servers.add_argument('--server', type=Path, help='fnTech3 dedicated executable')
    servers.add_argument('--retail-server', action='store_true',
                         help='run the retail dedicated server from the installation')
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail root holding the client executable and the game directory')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--settle', type=float, default=3.0,
                        help='seconds with both clients in the game before the kick')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This retail Windows client probe requires Windows')
    args.installation = args.installation.resolve(strict=True)
    if args.server:
        args.server = args.server.resolve(strict=True)
    return 0 if probe(args, args.output.resolve()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
