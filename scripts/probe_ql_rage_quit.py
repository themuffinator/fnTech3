#!/usr/bin/env python3
"""A rage quit on a Quake Live server, announced the way the retail engine does.

Windows only. Runs fnTech3's dedicated server (--server) as `quakelive` on
campgrounds ffa with the installation's retail game module, and two fnTech3
clients (--client) that join it over UDP loopback, each on its own unswitched
private desktop with an isolated home. No mouse or keyboard input is injected.
An observer joins first. The rage quitter joins after it with
`activeAction ragequit`, so the retail cgame's own `ragequit` command runs on
its first frame of play: it forwards `ragequit` to the server, whose retail
game calls game import 192 for that client, and a few frames later it quits,
which drops the client from the server as `disconnected`.

quakelive_steam.exe announces a client its game marked that way as
`print "%s ^1rage^7quits\\n"` (0x4df716, the format at 0x5452ac) instead of
`print "%s^7 %s\\n"`. The observer logs every server command it executes
(developer 1), and the dedicated server echoes each broadcast print to its log.
noticeMatchesRetail holds when the observer received exactly one notice about
the rage quitter, in the rage-quit form, and the server echoed that broadcast.
The run passes when it holds, the rage quitter quit by itself, the server
logged the drop, neither client failed with an error, and the installation is
unchanged. It makes no claim about what was drawn.
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

GAME = 'baseq3'
QUITTER = 'fntech3-ragequitter'
OBSERVER = 'fntech3-observer'
PASSWORD = 'fntech3-ragequit-probe'
OOB = bytes([255, 255, 255, 255])
# Each form a notice about the rage quitter could take.
FORMS = {'Quake Live rage quit': 'print "{name} ^1rage^7quits\n"',
         'Quake III print': 'print "{name}^7 {reason}\n"'}
RETAIL = 'Quake Live rage quit'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def read(path):
    if not path.exists():
        return ''
    return path.read_bytes().decode('utf-8', 'replace').replace('\r\n', '\n')


def settings_command(executable, values, actions):
    result = [str(executable)]
    for name, value in values.items():
        result += ['+set', name, value]
    return result + actions


def server_commands(text):
    """Every command a client logged, whole. `serverCommand: N : %s\\n` holds
    the command as it arrived: a quoted argument can carry newlines, and a
    newline straight after the line's own belongs to the command. fnTech3
    colours a developer line with a leading ^5."""
    commands = []
    for match in re.finditer(r'^(?:\^5)?serverCommand: (\d+) : ', text, re.M):
        i, quoted = match.end(), False
        while i < len(text) and (quoted or text[i] != '\n'):
            quoted ^= text[i] == '"'
            i += 1
        trailing = text[i:i + 2] == '\n\n'
        commands.append(text[match.end():i] + ('\n' if trailing else ''))
    return commands


def notices(commands):
    """The engine's notices about the rage quitter: every print naming it but
    the retail game's own announcement that it connected."""
    joined = 'print "%s^7 connected\n"' % QUITTER
    return [c for c in commands if QUITTER in c and c.startswith('print "') and c != joined]


def form(notice):
    for label, template in FORMS.items():
        pattern = re.escape(template.format(name=QUITTER, reason='\0')).replace('\0', '(.*)')
        if re.fullmatch(pattern, notice, re.S):
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


class Rcon:
    """Loopback rcon; fnTech3 answers only a command that printed something."""

    def __init__(self, port):
        self.address = ('127.0.0.1', port)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('127.0.0.1', 0))
        self.socket.settimeout(0.2)

    def close(self):
        self.socket.close()

    def post(self, command):
        self.socket.sendto(OOB + ('rcon %s %s' % (PASSWORD, command)).encode('ascii'), self.address)

    def ask(self, command, timeout):
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


def client_command(executable, installation, home, name, port, action):
    values = dict(com_gameProfile='quakelive', fs_basepath=str(installation), fs_homepath=str(home),
                  fs_game='', r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                  s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                  cl_renderer='glx', logfile='2', net_port='0', com_introplayed='1', cl_allowDownload='0',
                  developer='1', name=name)
    if action:
        values['activeAction'] = action
    return settings_command(executable, values, ['+connect', '127.0.0.1:%d' % port])


def close_all(processes, result):
    for process in reversed(processes):
        result.setdefault('exits', []).append(process.poll())
        try:
            process.close()
        except (OSError, subprocess.TimeoutExpired) as error:
            result.setdefault('closeErrors', []).append(str(error)[:200])


def probe(args, output):
    output.mkdir(parents=True, exist_ok=False)
    server_home = output / 'server'
    (server_home / GAME).mkdir(parents=True)
    homes = {name: output / name for name in (OBSERVER, QUITTER)}
    for home in homes.values():
        (home / GAME).mkdir(parents=True)
    installation = args.installation
    before = inventory(installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    server_command = settings_command(args.server, dict(
        com_gameProfile='quakelive', fs_basepath=str(installation), fs_homepath=str(server_home),
        dedicated='1', developer='1', logfile='2', net_ip='127.0.0.1', net_port=str(port),
        sv_pure=args.pure, bot_enable='0', com_steamIntegration='0', rconpassword=PASSWORD,
        **{'sv_master%d' % i: '' for i in range(1, 6)}), ['+map', 'campgrounds', 'ffa'])
    commands = {OBSERVER: client_command(args.client, installation, homes[OBSERVER], OBSERVER, port, ''),
                QUITTER: client_command(args.client, installation, homes[QUITTER], QUITTER, port, 'ragequit')}
    server_log = server_home / GAME / 'qconsole.log'
    logs = {name: home / GAME / 'qconsole.log' for name, home in homes.items()}
    module = installation / GAME / 'bin.pk3'
    result = dict(evidence='Interop: two fnTech3 clients of a fnTech3 Quake Live dedicated server, '
                  'one rage quitting through the retail cgame and game', profile='ql', map='campgrounds',
                  factory='ffa', pure=args.pure, serverCommand=server_command, clientCommands=commands,
                  serverSha256=sha(args.server), clientSha256=sha(args.client), binPk3Sha256=sha(module),
                  passed=False)
    processes = []
    rcon = Rcon(port)
    begin = time.monotonic()
    quitter = None
    try:
        host = WindowsPrivateProcess(server_command, args.server.parent, output / 'server-stdout.log')
        processes.append(host)
        rcon.ask('sv_pure', timeout=90)
        for name in (OBSERVER, QUITTER):
            process = WindowsPrivateProcess(commands[name], args.client.parent, output / ('%s-stdout.log' % name))
            processes.append(process)
            if name == QUITTER:
                quitter = process
                break
            entered = 'Going from CS_PRIMED to CS_ACTIVE for %s' % name
            if not wait_for(lambda: entered in read(server_log), 120, processes):
                raise RuntimeError('%s never entered the game' % name)
        # The rage quitter enters, runs ragequit on its first frame of play
        # and quits by itself.
        entered = 'Going from CS_PRIMED to CS_ACTIVE for %s' % QUITTER
        if not wait_for(lambda: entered in read(server_log), 120, processes[:2]):
            raise RuntimeError('the rage quitter never entered the game')
        try:
            result['quitterExit'] = quitter.wait(60)
        except subprocess.TimeoutExpired:
            result['quitterExit'] = None
        heard = lambda: bool(notices(server_commands(read(logs[OBSERVER]))))
        if not wait_for(heard, 20, processes[:2]):
            raise RuntimeError('the observer never heard of the drop')
        time.sleep(2)
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
    heard_notices = notices(server_commands(texts[OBSERVER]))
    # The dedicated server echoes each broadcast print with its newlines escaped.
    broadcasts = notices([line[len('broadcast: '):].replace('\\n', '\n')
                          for line in server_text.splitlines() if line.startswith('broadcast: ')])
    expected = FORMS[RETAIL].format(name=QUITTER)
    result.update(
        observerNotices=heard_notices,
        observerNoticeForm=form(heard_notices[0]) if heard_notices else None,
        serverBroadcasts=broadcasts,
        serverEchoMatches=broadcasts == [expected],
        # What the server received from the rage quitter, in order (developer 1).
        quitterCommands=re.findall(r'^(?:\^5)?clientCommand: %s : \d+ : (.*)$' % re.escape(QUITTER),
                                   server_text, re.M),
        serverDropped=[line for line in server_text.splitlines() if 'CS_ZOMBIE for %s' % QUITTER in line],
        errors={name: [line for line in text.splitlines() if line.startswith('ERROR:')]
                for name, text in list(texts.items()) + [('server', server_text)]},
        serverCrashed='Server crashed' in server_text or 'Unhandled exception' in server_text,
        installationUnchanged=before == inventory(installation),
        serverShaAfter=sha(args.server), clientShaAfter=sha(args.client))
    result['noticeMatchesRetail'] = bool(heard_notices == [expected] and result['serverEchoMatches'])
    result['passed'] = bool(not result.get('error') and result['noticeMatchesRetail']
                            and result.get('quitterExit') == 0
                            and result['quitterCommands'][-2:] == ['ragequit', 'disconnect']
                            and result['serverDropped'] and not result['serverCrashed']
                            and not any(result['errors'].values())
                            and result['installationUnchanged']
                            and result['serverShaAfter'] == result['serverSha256']
                            and result['clientShaAfter'] == result['clientSha256'])
    (output / 'server.log').write_text(server_text, encoding='utf-8')
    for name, text in texts.items():
        (output / ('%s.log' % name)).write_text(text, encoding='utf-8')
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if not k.endswith(('Command', 'Commands'))}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--server', type=Path, required=True, help='fnTech3 dedicated executable')
    parser.add_argument('--client', type=Path, required=True, help='fnTech3 client executable')
    parser.add_argument('--installation', type=Path, required=True,
                        help='Quake Live root holding baseq3, read only')
    parser.add_argument('--pure', choices=('0', '1'), default='1')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This private-desktop probe requires Windows')
    args.installation = args.installation.resolve(strict=True)
    args.server = args.server.resolve(strict=True)
    args.client = args.client.resolve(strict=True)
    return 0 if probe(args, args.output.resolve()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
