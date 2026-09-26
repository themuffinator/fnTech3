#!/usr/bin/env python3
"""Enemy Territory map_restart runs while a delayed restart is pending.

Windows only. Runs one dedicated server, fnTech3's (--server) or retail
etded.exe (--retail-server), on an unswitched private desktop with an isolated
home, loopback networking and no clients. No input is injected and nothing is
rendered.

A delayed restart fires on the server's clock and appends its "map_restart 0"
to the end of the command buffer, so a script exec'd in one piece would hold
it back until the script ended. The probe therefore paces the steps in real
time from outside: each step, and each marker, is a config in the isolated
home that the probe runs with a loopback rcon "exec", leaving the buffer
empty in between. Commands run from the buffer print to the log, where the
game module's initialization banner, printed at every level start, counts the
restarts each step made. The run passes when every step makes as many
restarts as retail etded.exe does and the installation is unchanged.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import secrets
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

# (marker, console commands, seconds waited afterwards, restarts retail makes).
# The map loads from the startup config; the other steps run by rcon.
STEPS = (
    ('loaded', [], 0.0, 1),
    ('delayed-an-hour', [['map_restart', '3600']], 1.5, 0),
    ('immediate-while-pending', [['map_restart', '0']], 1.5, 1),
    ('delayed-two-seconds-while-pending', [['map_restart', '2']], 4.5, 1),
    ('immediate-then-pending-fires', [['map_restart', '2'], ['wait', '5'], ['map_restart', '0']], 4.5, 2),
)
MARK = 'fntech3_restart_mark_'
BANNER = 'Game Initialization'
PREFIX = 'fntech3-pending-restart'
# Retail ET silently drops an rcon command handled within 500 ms of the last
# one, from anyone, and a command queued behind a long frame is handled late.
# Every exec is therefore spaced out, and resent until its reply shows it ran.
RCON_GAP = 1.0
RCON_TRIES = 3


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def printed(text, name):
    match = re.search(r'^"%s" is:"(.*?)(?:\^7)?"' % re.escape(name), text, re.M)
    return match.group(1) if match else None


def marker_line(marker):
    return '"%s%s" is:' % (MARK, marker.replace('-', '_'))


def read(log):
    try:
        return log.read_text(encoding='utf-8', errors='replace')
    except OSError:
        return ''


class Rcon:
    def __init__(self, port, password):
        self.address = ('127.0.0.1', port)
        self.password = password
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.settimeout(0.1)
        self.last = 0.0
        self.replies = []

    def receive(self, prefix, until, seconds):
        """Join the replies that start with prefix until one holds until."""
        text = ''
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                data, peer = self.socket.recvfrom(65535)
            except (socket.timeout, ConnectionResetError):
                continue
            if peer == self.address and data.startswith(b'\xff\xff\xff\xff' + prefix):
                text += data[4 + len(prefix):].decode('latin-1')
                if until in text:
                    break
        return text

    def ready(self, seconds):
        """Wait until the server answers queries, which it does between frames."""
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.socket.sendto(b'\xff\xff\xff\xffgetinfo fntech3', self.address)
            if 'fntech3' in self.receive(b'infoResponse\n', 'fntech3', 0.3):
                return True
        return False

    def send(self, command, expect=None):
        for _ in range(RCON_TRIES if expect else 1):
            pause = self.last + RCON_GAP - time.monotonic()
            if pause > 0:
                time.sleep(pause)
            self.socket.sendto(b'\xff\xff\xff\xffrcon %s %s' % (self.password.encode(), command.encode()),
                               self.address)
            self.last = time.monotonic()
            reply = self.receive(b'print\n', expect or '\0', 1.5 if expect else 0.3)
            self.replies.append(dict(command=command, reply=reply))
            if not expect or expect in reply:
                return
        raise TimeoutError(f'rcon {command!r} never ran')

    def close(self):
        self.socket.close()


def probe(args, output):
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    (home / 'etmain').mkdir(parents=True)
    retail = args.retail_server
    server = args.installation / 'etded.exe' if retail else args.server
    before = inventory(args.installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    password = secrets.token_hex(8)
    values = dict(fs_basepath=str(args.installation), fs_homepath=str(home), dedicated='1',
                  logfile='2', net_ip='127.0.0.1', net_port=str(port), sv_pure='0',
                  sv_punkbuster='0', g_gametype='2', r_fullscreen='0', rconpassword=password,
                  **{f'sv_master{i}': '' for i in range(1, 6)})
    if not retail:
        values = dict(com_gameProfile='wolfet', **values)
    configs = {}
    configs[f'{PREFIX}.cfg'] = [f'set {MARK}{m.replace("-", "_")} 1' for m, *_ in STEPS] + [f'map {args.map}']
    for index, (marker, actions, _, _) in enumerate(STEPS):
        if actions:
            configs[f'{PREFIX}-step{index}.cfg'] = [' '.join(action) for action in actions]
        configs[f'{PREFIX}-mark{index}.cfg'] = [f'{MARK}{marker.replace("-", "_")}', 'gamestate']
    for name, lines in configs.items():
        (home / 'etmain' / name).write_text(''.join(line + '\n' for line in lines), encoding='ascii')
    command = [str(server)]
    for name, value in values.items():
        command += ['+set', name, value]
    command += ['+exec', f'{PREFIX}.cfg']
    log = home / 'etmain' / ('etconsole.log' if retail else 'qconsole.log')
    result = dict(evidence='Live: retail etded.exe dedicated server' if retail
                  else 'Live: fnTech3 dedicated server', profile='wolfet', retailServer=retail,
                  map=args.map, command=[part if part != password else '<random>' for part in command],
                  configs=configs, serverSha256=sha(server),
                  moduleSha256=sha(args.installation / 'etmain/qagame_mp_x86.dll'),
                  pak0Sha256=sha(args.installation / 'etmain/pak0.pk3'), passed=False)
    begin = time.monotonic()
    seen = {}
    host = rcon = None

    def wait_for(predicate, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline and host.poll() is None:
            if predicate(read(log)):
                return True
            time.sleep(0.05)
        return predicate(read(log))

    try:
        host = WindowsPrivateProcess(command, server.parent, output / 'server-stdout.log')
        rcon = Rcon(port, password)
        if not wait_for(lambda text: BANNER in text, args.timeout) or not rcon.ready(args.timeout):
            raise TimeoutError('the map never started')
        for index, (marker, actions, seconds, _) in enumerate(STEPS):
            if actions:
                rcon.send(f'exec {PREFIX}-step{index}.cfg', f'execing {PREFIX}-step{index}.cfg')
                time.sleep(seconds)
            rcon.send(f'exec {PREFIX}-mark{index}.cfg', f'execing {PREFIX}-mark{index}.cfg')
            if not wait_for(lambda text: marker_line(marker) in text, 5):
                raise TimeoutError(f'marker {marker} never reached the log')
            seen[marker] = round(time.monotonic() - begin, 3)
        rcon.send('quit')
        deadline = time.monotonic() + 15
        while host.poll() is None and time.monotonic() < deadline:
            time.sleep(0.1)
        result['exitCode'] = host.poll()
        result['timedOut'] = result['exitCode'] is None
    except (OSError, TimeoutError) as error:
        result['error'] = str(error)
    finally:
        if rcon:
            result['rcon'] = rcon.replies
            rcon.close()
        if host:
            host.close()
    result['elapsedSeconds'] = round(time.monotonic() - begin, 3)
    text = read(log)
    steps = []
    start = 0
    for marker, actions, seconds, expected in STEPS:
        end = text.find(marker_line(marker), start)
        steps.append(dict(step=marker, commands=[' '.join(a) for a in actions], waitSeconds=seconds,
                          restarts=text[start:end].count(BANNER) if end >= 0 else None,
                          expected=expected, markedAtSeconds=seen.get(marker),
                          gamestate=printed(text[end:], 'gamestate') if end >= 0 else None))
        start = end + 1 if end >= 0 else len(text)
    result['steps'] = steps
    result['variableChangeRestarts'] = len(re.findall(r'variable change -- restarting', text))
    result['networkLookups'] = [line for line in text.splitlines() if line.startswith('Resolving ')]
    result['errors'] = [line for line in text.splitlines() if line.startswith('ERROR') or 'Error:' in line]
    result['badRcon'] = [r for r in result.get('rcon', []) if 'rconpassword' in r['reply'].lower()]
    # Each engine logs the commands it accepts, before running them.
    result['rconAccepted'] = len(re.findall(r'^Rcon from ', text, re.M))
    result['rconRan'] = sum(1 for r in result.get('rcon', []) if 'execing ' in r['reply'])
    result['installationUnchanged'] = before == inventory(args.installation)
    result['passed'] = bool(all(s['restarts'] == s['expected'] for s in steps)
                            and result['installationUnchanged'] and not result.get('timedOut')
                            and not result.get('error') and not result['errors'] and not result['badRcon']
                            and result['rconAccepted'] == result['rconRan'] + 1)
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: result.get(k) for k in ('evidence', 'serverSha256', 'steps', 'variableChangeRestarts',
                                                  'networkLookups', 'rconAccepted', 'rconRan',
                                                  'installationUnchanged', 'error', 'passed')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    servers = parser.add_mutually_exclusive_group(required=True)
    servers.add_argument('--server', type=Path, help='fnTech3 dedicated executable')
    servers.add_argument('--retail-server', action='store_true', help='run retail etded.exe as the server')
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail ET 2.60b root holding etded.exe and etmain/')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--map', default='goldrush')
    parser.add_argument('--timeout', type=float, default=60, help='seconds allowed for the map to load')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This installed native-module probe requires Windows')
    args.installation = args.installation.resolve(strict=True)
    if args.server:
        args.server = args.server.resolve(strict=True)
    return 0 if probe(args, args.output.resolve()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
