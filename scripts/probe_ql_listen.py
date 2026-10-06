#!/usr/bin/env python3
"""Quake Live servers started from the client, by server type, live over loopback.

For each fnTech3 runtime named (``--runtime label=dir``, holding
``fntech3.exe`` and ``fntech3.ded.exe``), this starts the ``quakelive``
profile's servers and asks them what a joining player and a browser ask.
Retail ``quakelive_steam.exe`` keeps a server to itself by ``sv_serverType``,
never by its game type: 0 (offline, the default) drops every packet from
anywhere but its own client, 1 (LAN) refuses a challenge from outside the LAN,
2 (Internet) takes anyone (record ``2026-10-06-ql-listen-server-type.md``).

- ``listen-0``, ``listen-1``, ``listen-2``: a client hosts Race
  (``+set sv_serverType N +map campgrounds race``) on 27975 with a player
  named ProbeHost. From 127.0.0.1, an address on the LAN: ``getinfo``,
  ``getstatus``, ``getchallenge`` and an rcon ``echo``; then a second client,
  ProbeGuest (port 27976), connects and must enter or not.
- ``dedicated-0``: ``fntech3.ded`` with ``sv_serverType 0`` on Race; a
  dedicated build is unchanged, so it answers all four.

Every address here is on the LAN, so the LAN server's refusal of an outsider
is a unit test's (``tests/ql_server_type_tests.py``), not this probe's. Windows
only: every process runs on a private desktop with its own home, windowed,
silent, without Steam, IPv4 alone on 127.0.0.1, with no input. The
installation is read only and its inventory is compared before and after. No
retail program runs.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import secrets
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

INSTALL = Path(r'E:\SteamLibrary\steamapps\common\Quake Live')
GAME = 'baseq3'
OOB = b'\xff\xff\xff\xff'
PORT = 27975
# getinfo, getstatus, getchallenge and rcon share one per-address bucket on
# the server: ten at once, then one a second
PACE = 1.1

# What each case must show, per runtime label: the running game type, which
# of getinfo, getstatus, getchallenge and rcon reply, and whether ProbeGuest
# enters. "before" is HEAD 5603d244: a client-hosted Quake Live server answered
# no getinfo, getstatus or getchallenge whatever its server type -- the flag
# its local match sets, and Race taken for single player -- and took rcon.
EXPECTED = {
    ('listen-0', 'before'): ([False, False, False, True], False),
    ('listen-1', 'before'): ([False, False, False, True], False),
    ('listen-2', 'before'): ([False, False, False, True], False),
    ('listen-0', 'after'): ([False, False, False, False], False),
    ('listen-1', 'after'): ([False, False, True, True], True),
    ('listen-2', 'after'): ([False, False, True, True], True),
    ('dedicated-0', 'before'): ([True, True, True, True], None),
    ('dedicated-0', 'after'): ([True, True, True, True], None),
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(root):
    return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in root.rglob('*') if p.is_file()}


def wait_for(predicate, timeout, step=0.5):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        value = predicate()
        if value:
            return value
        time.sleep(step)
    return predicate()


def ask(port, text, marker, timeout=1.5):
    """The first out-of-band reply starting with marker, or None."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        sock.sendto(OOB + text.encode('latin-1'), ('127.0.0.1', port))
        end = time.monotonic() + timeout
        while True:
            left = end - time.monotonic()
            if left <= 0:
                return None
            sock.settimeout(left)
            try:
                data, _ = sock.recvfrom(65535)
            except socket.timeout:
                return None
            except ConnectionResetError:
                continue
            body = data[4:].decode('latin-1', 'replace')
            if body.startswith(marker):
                return body


def queries(port, password):
    replies = []
    for text, marker in (('getinfo probe', 'infoResponse'), ('getstatus probe', 'statusResponse'),
                         ('getchallenge', 'challengeResponse'), (f'rcon {password} echo probe-echo', 'print')):
        replies.append(ask(port, text, marker))
        time.sleep(PACE)
    return replies


def free(port):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        try:
            sock.bind(('127.0.0.1', port))
            return True
        except OSError:
            return False


def log_text(path):
    return path.read_text('latin-1', 'replace') if path.exists() else ''


class Session:
    def __init__(self, directory):
        self.dir = directory
        self.dir.mkdir(parents=True, exist_ok=False)
        self.result = dict(checks=[])
        self.processes = []

    def check(self, name, ok, **detail):
        self.result['checks'].append(dict(name=name, ok=bool(ok), **detail))
        print(('  ok   ' if ok else '  FAIL ') + name + ('' if ok else f' {detail}'))

    def start(self, label, executable, settings, tail, cwd):
        home = self.dir / f'{label}-home'
        (home / GAME).mkdir(parents=True)
        # the installation's own configuration would leak in otherwise
        for config in ('qzconfig.cfg', 'repconfig.cfg', 'autoexec.cfg'):
            (home / GAME / config).write_text('', encoding='latin-1')
        common = dict(com_gameProfile='quakelive', fs_basepath=str(INSTALL), fs_homepath=str(home), fs_game='',
                      developer='1', logfile='2', net_ip='127.0.0.1', net_enabled='1', com_steamIntegration='0',
                      fs_skipWorkshop='1', cl_webuiEnable='0', r_fullscreen='0', r_mode='3', in_mouse='0',
                      in_joystick='0', in_nograb='1', s_initsound='0', com_introplayed='1', cl_allowDownload='0',
                      com_maxfps='60')
        command = [str(executable)]
        for key, value in {**common, **settings}.items():
            command += ['+set', key, str(value)]
        command += tail
        process = WindowsPrivateProcess(command, cwd, self.dir / f'{label}-stdout.log')
        self.processes.append(process)
        self.result.setdefault('processes', {})[label] = dict(
            command=[c if c != settings.get('rconPassword') else '<password>' for c in command],
            sha256=sha(executable))
        return process, home / GAME / 'qconsole.log'

    def stop_all(self):
        """Stop every process and wait until each has exited: a client still
        tearing down its renderer stalled the next case's host in an earlier
        run, before and after this change alike."""
        for process in self.processes:
            try:
                process.terminate()
            except OSError as error:
                # Windows refuses a process whose exit has begun
                self.result.setdefault('stopErrors', []).append(str(error))
            if not wait_for(lambda: process.poll() is not None, 90, 0.5):
                self.result.setdefault('stopErrors', []).append('a process did not exit in 90 s')
            try:
                process.close()
            except (OSError, subprocess.TimeoutExpired) as error:
                self.result.setdefault('stopErrors', []).append(str(error))
        self.processes = []


def game_type(log):
    """g_gametype as the startup command printed it once the map loaded."""
    found = re.findall(r'"g_gametype" is:"(-?\d+)', log)
    return int(found[-1]) if found else None


def listen_case(session, runtime, label, password, server_type):
    exe = runtime / 'fntech3.exe'
    host, log = session.start('host', exe, dict(net_port=PORT, rconPassword=password, name='ProbeHost',
                                                sv_serverType=server_type, sv_maxclients=8, bot_enable=0,
                                                sv_hostname='QL listen probe'),
                              ['+map', 'campgrounds', 'race', '+g_gametype'], runtime)
    entered = wait_for(lambda: 'CS_ACTIVE for ProbeHost' in log_text(log), 150)
    session.check('the client hosts and its own player enters', entered)
    time.sleep(3.0)
    session.result['gametype'] = game_type(log_text(log))
    session.check('the game runs Race, game type 2', session.result['gametype'] == 2,
                  gametype=session.result['gametype'])
    return host, log


def run_case(session, runtime, label, password, case):
    want, joins = EXPECTED[(case, label)]
    if case == 'dedicated-0':
        exe = runtime / 'fntech3.ded.exe'
        host, log = session.start('server', exe, dict(dedicated=1, net_port=PORT, rconPassword=password,
                                                      sv_serverType=0, sv_maxclients=8, bot_enable=0,
                                                      sv_hostname='QL dedicated probe'),
                                  ['+map', 'campgrounds', 'race', '+g_gametype'], runtime)
        started = wait_for(lambda: game_type(log_text(log)) is not None, 120)
        session.check('the dedicated server starts', started)
        time.sleep(3.0)
        session.result['gametype'] = game_type(log_text(log))
        session.check('the game runs Race, game type 2', session.result['gametype'] == 2,
                      gametype=session.result['gametype'])
    else:
        host, log = listen_case(session, runtime, label, password, int(case.split('-')[1]))
    replies = queries(PORT, password)
    answered = [reply is not None for reply in replies]
    session.result['replies'] = [r[:200] if r else None for r in replies]
    session.result['answered'] = answered
    session.check('getinfo, getstatus, getchallenge, rcon: ' + ', '.join(
        'answered' if w else 'silent' for w in want), answered == want, answered=answered)
    if joins is not None:
        guest, guest_log = session.start('guest', runtime / 'fntech3.exe', dict(net_port=PORT + 1, name='ProbeGuest'),
                                         ['+connect', f'127.0.0.1:{PORT}'], runtime)
        joined = wait_for(lambda: 'CS_ACTIVE for ProbeGuest' in log_text(log), 90 if joins else 45)
        session.result['guestLogTail'] = log_text(guest_log).splitlines()[-6:]
        session.check('ProbeGuest connects and enters' if joins else 'ProbeGuest cannot connect',
                      bool(joined) == joins, guest=session.result['guestLogTail'])
        guest.terminate()
        session.result['guestLog'] = str(guest_log)
    rejected = [line for line in log_text(log).splitlines() if 'rate limit' in line]
    session.check('no query hit the rate limit', not rejected, lines=rejected[:3])
    session.result['log'] = str(log)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', action='append', required=True, help='label=dir (before or after)')
    parser.add_argument('--output', type=Path, default=ROOT / '.tmp' / 'ql-listen' / 'live')
    parser.add_argument('--cases', default='listen-0,listen-1,listen-2,dedicated-0')
    args = parser.parse_args()
    for port in (PORT, PORT + 1):
        if not free(port):
            sys.exit(f'port {port} is in use')
    before = inventory(INSTALL / GAME)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    results = dict(installation=str(INSTALL), started=stamp, runs={})
    for item in args.runtime:
        label, directory = item.split('=', 1)
        runtime = Path(directory).resolve()
        for case in args.cases.split(','):
            name = f'{label}-{case}'
            print(name)
            session = Session(args.output / stamp / name)
            password = secrets.token_hex(6)
            try:
                run_case(session, runtime, label, password, case)
            except Exception as error:  # noqa: BLE001 -- recorded, then the next run
                session.check('the run completes', False, error=repr(error))
            finally:
                session.stop_all()
            session.result['passed'] = all(c['ok'] for c in session.result['checks'])
            results['runs'][name] = session.result
            (session.dir / 'result.json').write_text(json.dumps(session.result, indent=1), encoding='utf-8')
            time.sleep(2.0)
    results['installationUnchanged'] = inventory(INSTALL / GAME) == before
    print('installation unchanged:', results['installationUnchanged'])
    out = args.output / stamp / 'results.json'
    out.write_text(json.dumps(results, indent=1), encoding='utf-8')
    print(out)
    return 0 if results['installationUnchanged'] and all(r['passed'] for r in results['runs'].values()) else 1


if __name__ == '__main__':
    sys.exit(main())
