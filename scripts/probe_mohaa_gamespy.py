#!/usr/bin/env python3
"""Medal of Honor's GameSpy LAN queries, live over loopback UDP.

For each fnTech3 runtime named (``--runtime label=dir``), this starts the
mohaa profile's servers and asks them what GameSpy's LAN search and query
loop asked a server (OpenMoHAA's gserverlist.c and cl_uiserverlist.cpp, read
as reference **[O]**): ``\\status\\`` broadcast to ports 12300 to 12316, every
server that answers with ``\\final\\`` then asked ``\\status\\`` again at the
port it answered from, its packets read until ``\\final\\``.

- ``dedicated``: two dedicated servers on ``dm/mohdm1``, the second left to
  find its own ports; the LAN search, each query kind, what must go
  unanswered, the per-address rate limit, ``getinfo`` on the game port,
  ``sv_gamespy 0`` and 1, a moved ``net_gamespy_port``, a single-player game
  type and ``quit``, each by rcon.
- ``listen``: a client hosting a team match (``g_gametype 2``) on
  ``dm/mohdm2`` with a player named ProbeHost: ``getinfo`` on its game port,
  the LAN search and its ``\\status\\``, a second client joining it over UDP,
  then the host joins the Allies, takes a rifle and dies by the game's
  ``kill``, and the answer is read again.

Everything binds 127.0.0.1 with IPv4 alone; this machine delivers a
limited broadcast between loopback sockets, so the search is a real
broadcast. Windows only: every process runs on a private desktop with its
own home, windowed, silent, with no input. The installation is read only
and its inventory is compared before and after. No retail client runs here,
so no retail browser takes part.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import secrets
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

INSTALL = Path(r'E:\Games\Medal of Honor Allied Assault')
OOB = b'\xff\xff\xff\xff\x02'
LAN_PORTS = range(12300, 12317)
BASIC = {'gamename', 'gamever', 'location'}
INFO = {'hostname', 'hostport', 'mapname', 'gametype', 'numplayers', 'maxplayers', 'gamemode', 'gametype_i',
        'dedicated', 'pure'}
RULES = {'timelimit', 'fraglimit', 'rankedserver'}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(root):
    return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in root.rglob('*') if p.is_file()}


def wait_for(predicate, timeout, step=0.1):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        value = predicate()
        if value:
            return value
        time.sleep(step)
    return predicate()


def udp():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 0))
    return sock


def receive(sock, until, stop=None):
    """Datagrams until the deadline, or until stop(datagrams) holds."""
    got = []
    while True:
        left = until - time.monotonic()
        if left <= 0:
            return got
        sock.settimeout(left)
        try:
            data, source = sock.recvfrom(65535)
        except socket.timeout:
            return got
        except ConnectionResetError:
            # Windows: an earlier datagram's port was closed
            continue
        got.append((data, source))
        if stop and stop(got):
            return got


def ask(port, payload, timeout=1.5):
    """Packets answering one query, read until `\\final\\` or the timeout."""
    with udp() as sock:
        start = time.monotonic()
        sock.sendto(payload, ('127.0.0.1', port))
        got = receive(sock, start + timeout, lambda g: b'\\final\\' in g[-1][0])
        return [data for data, source in got if source[1] == port], round((time.monotonic() - start) * 1000, 1)


def decode(packets):
    """Each packet's checks, and the answer's pairs in order (queryid and
    final left out, as a browser keeps them)."""
    problems, pairs, query_ids = [], [], set()
    for index, packet in enumerate(packets):
        text = packet.decode('latin-1')
        match = re.search(r'\\queryid\\(\d+)\.(\d+)$', text)
        if len(packet) > 1400:
            problems.append(f'packet {index + 1} is {len(packet)} bytes')
        if not match:
            problems.append(f'packet {index + 1} has no trailing queryid')
            continue
        query_ids.add(match.group(1))
        if int(match.group(2)) != index + 1:
            problems.append(f'packet {index + 1} numbered {match.group(2)}')
        if ('\\final\\' in text) != (index == len(packets) - 1):
            problems.append(f'packet {index + 1} final misplaced')
        body = text[:match.start()]
        words = body.split('\\')[1:]
        if len(words) % 2:
            problems.append(f'packet {index + 1} splits a pair')
        pairs += [(words[i], words[i + 1]) for i in range(0, len(words) - 1, 2) if words[i] != 'final']
    if len(query_ids) > 1:
        problems.append(f'queryids differ: {sorted(query_ids)}')
    return problems, pairs


def lan_search(seconds=3.0):
    """GameSpy's LAN search: `\\status\\` broadcast to 12300-12316, every
    answer with `\\final\\` a server at the port it came from."""
    with udp() as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        start = time.monotonic()
        for port in LAN_PORTS:
            sock.sendto(b'\\status\\', ('255.255.255.255', port))
        got = receive(sock, start + seconds)
    return sorted({source for data, source in got if b'\\final\\' in data})


def getinfo(port, timeout=1.5):
    with udp() as sock:
        sock.sendto(OOB + b'getinfo probe', ('127.0.0.1', port))
        got = receive(sock, time.monotonic() + timeout, lambda g: b'infoResponse' in g[-1][0])
    for data, source in got:
        if b'infoResponse' in data:
            text = data.decode('latin-1')
            words = text[text.index('infoResponse') + len('infoResponse'):].strip().split('\\')[1:]
            return dict(zip(words[0::2], words[1::2]))
    return None


def rcon(port, password, text, timeout=2.0):
    with udp() as sock:
        sock.sendto(OOB + f'rcon {password} {text}\n'.encode('latin-1'), ('127.0.0.1', port))
        got = receive(sock, time.monotonic() + timeout, lambda g: True)
        if got:
            more = receive(sock, time.monotonic() + 0.3)
            got += more
    return ''.join(data[5:].decode('latin-1', 'replace') for data, _ in got) or None


def free(port):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        try:
            sock.bind(('127.0.0.1', port))
            return True
        except OSError:
            return False


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
        (home / 'main').mkdir(parents=True)
        command = [str(executable)]
        for key, value in settings.items():
            command += ['+set', key, str(value).replace('<home>', str(home))]
        command += tail
        process = WindowsPrivateProcess(command, cwd, self.dir / f'{label}-stdout.log')
        self.processes.append(process)
        self.result.setdefault('processes', {})[label] = dict(
            command=[c if 'rconPassword' not in c else c for c in command], sha256=sha(executable))
        return process, home / 'main' / 'qconsole.log'

    def stop_all(self):
        for process in self.processes:
            try:
                process.close()
            except OSError as error:
                self.result.setdefault('stopErrors', []).append(str(error))
        self.processes = []


def log_text(path):
    return path.read_text('latin-1', 'replace') if path.exists() else ''


def answered_port(log):
    found = re.findall(r'GameSpy LAN queries answered on UDP port (\d+)', log)
    return [int(p) for p in found]


def dedicated_case(session, runtime, password, expect_answers):
    exe = runtime / 'fntech3.ded.exe'
    common = dict(com_gameProfile='mohaa', fs_basepath=str(INSTALL), fs_homepath='<home>', dedicated='1',
                  developer='1', logfile='2', net_ip='127.0.0.1', net_enabled='1', rconPassword=password,
                  g_gametype='1', sv_maxclients='8', timelimit='20', fraglimit='0')
    first, first_log = session.start('dedicated-a', exe, dict(common, sv_hostname='GameSpy probe A'),
                                     ['+map', 'dm/mohdm1'], runtime)
    ready = wait_for(lambda: getinfo(12203), 90, 0.5)
    session.check('first server answers getinfo on 12203', ready, info=ready)
    if not expect_answers:
        time.sleep(1.0)
        found = lan_search()
        session.result['lanSearch'] = [list(s) for s in found]
        session.check('HEAD: the LAN search finds nothing', not found, found=found)
        packets, _ = ask(12300, b'\\status\\', 1.0)
        session.check('HEAD: nothing answers \\status\\ on 12300', not packets, packets=len(packets))
        rcon(12203, password, 'quit')
        session.check('the server quits', wait_for(lambda: first.poll() is not None, 20, 0.5), exit=first.poll())
        session.result['logs'] = {'a': str(first_log)}
        return
    second, second_log = session.start('dedicated-b', exe, dict(common, sv_hostname='GameSpy probe B'),
                                       ['+map', 'dm/mohdm1'], runtime)
    ready_b = wait_for(lambda: getinfo(12204), 90, 0.5)
    session.check('second server finds 12204 and answers getinfo there', ready_b, info=ready_b)
    time.sleep(1.0)
    logs = {name: log_text(path) for name, path in (('a', first_log), ('b', second_log))}
    session.result['answeredPorts'] = {name: answered_port(text) for name, text in logs.items()}

    found = lan_search()
    session.result['lanSearch'] = [list(s) for s in found]
    session.check('the LAN search finds both, at 12300 and 12301',
                  found == [('127.0.0.1', 12300), ('127.0.0.1', 12301)], found=found)
    replies = {}
    for port, host_port, name in ((12300, 12203, 'GameSpy probe A'), (12301, 12204, 'GameSpy probe B')):
        packets, ms = ask(port, b'\\status\\')
        problems, pairs = decode(packets)
        values = dict(pairs)
        replies[port] = dict(packets=[p.decode('latin-1') for p in packets], ms=ms, problems=problems)
        keys = [k for k, _ in pairs]
        session.check(f'{port} \\status\\: one well-formed answer', packets and not problems, problems=problems)
        session.check(f'{port} \\status\\: the basic, info and rules keys and no others',
                      set(keys) == BASIC | INFO | RULES and len(keys) == len(set(keys)), keys=keys)
        session.check(f'{port} \\status\\: basic, info, rules in order',
                      keys[:3] == ['gamename', 'gamever', 'location'] and keys[-3:] == ['timelimit', 'fraglimit',
                                                                                         'rankedserver'], keys=keys)
        expected = dict(gamename='mohaa', gamever='1.00', location='1', hostname=name, hostport=str(host_port),
                        mapname='mohdm1', gametype='Free-For-All', numplayers='0', maxplayers='8',
                        gamemode='openplaying', gametype_i='1', dedicated='1', pure='0', timelimit='20',
                        fraglimit='0', rankedserver='0')
        session.check(f'{port} \\status\\: the values', values == expected, values=values)
    session.result['statusReplies'] = replies

    kinds = {}
    for label, payload, wanted in (('basic', b'\\basic\\', BASIC), ('info', b'\\info\\', INFO),
                                   ('rules', b'\\rules\\', RULES), ('players', b'\\players\\', set()),
                                   ('info,basic', b'\\info\\\\basic\\', BASIC | INFO)):
        time.sleep(0.15)
        packets, ms = ask(12300, payload)
        problems, pairs = decode(packets)
        keys = [k for k, _ in pairs]
        kinds[label] = dict(packets=[p.decode('latin-1') for p in packets], ms=ms)
        ok = packets and not problems and set(keys) == wanted
        if label == 'info,basic':
            ok = ok and keys[:3] == ['gamename', 'gamever', 'location']
        session.check(f'\\{label}\\ answers its keys', ok, keys=keys, problems=problems)
    packets, ms = ask(12300, b'\\echo\\fnTech3probe')
    problems, pairs = decode(packets)
    kinds['echo'] = dict(packets=[p.decode('latin-1') for p in packets], ms=ms)
    session.check('\\echo\\ comes back', pairs == [('echo', 'fnTech3probe')] and not problems, pairs=pairs)
    session.result['queryKinds'] = kinds

    silent = {}
    for label, payload in (('cdkey', b';\\status\\'), ('secure', b'\\secure\\ABCDEF'), ('garbage', b'hello'),
                           ('oversize', b'\\status\\' + b'x' * 300), ('demonstration', b'\\packets\\')):
        time.sleep(0.15)
        packets, ms = ask(12300, payload, 1.0)
        silent[label] = len(packets)
        session.check(f'{label} query goes unanswered', not packets, packets=len(packets))
    session.result['unanswered'] = silent

    time.sleep(1.5)
    with udp() as sock:
        for _ in range(15):
            sock.sendto(b'\\basic\\', ('127.0.0.1', 12300))
        got = receive(sock, time.monotonic() + 1.5)
    answers = sum(1 for data, _ in got if b'\\final\\' in data)
    session.result['rateLimit'] = dict(sent=15, answered=answers)
    session.check('fifteen queries at once from one address: ten answered', answers == 10, answered=answers)
    time.sleep(1.5)

    info = getinfo(12203)
    session.check('getinfo on the game port still answered', info and info.get('hostname') == 'GameSpy probe A',
                  info=info)

    reply = rcon(12203, password, 'sv_gamespy 0')
    time.sleep(1.0)
    packets, _ = ask(12300, b'\\status\\', 1.0)
    session.check('sv_gamespy 0 closes the port', not packets and free(12300), rcon=reply)
    rcon(12203, password, 'sv_gamespy 1')
    time.sleep(1.0)
    packets, _ = ask(12300, b'\\basic\\')
    session.check('sv_gamespy 1 opens it again', bool(packets) and answered_port(log_text(first_log)).count(12300) >= 2,
                  ports=answered_port(log_text(first_log)))

    rcon(12203, password, 'net_gamespy_port 12350')
    time.sleep(1.0)
    moved, _ = ask(12350, b'\\basic\\')
    old, _ = ask(12300, b'\\basic\\', 1.0)
    session.check('net_gamespy_port 12350 moves it', bool(moved) and not old, ports=answered_port(log_text(first_log)))
    rcon(12203, password, 'net_gamespy_port 12300')
    time.sleep(1.0)

    # a single-player game type closes it; the cfg keeps the spawn's prints in the log
    (first_log.parent / 'gamespy_sp.cfg').write_text('g_gametype 0\nmap dm/mohdm1\n', encoding='latin-1')
    rcon(12203, password, 'exec gamespy_sp.cfg')
    wait_for(lambda: log_text(first_log).count('------ Server Initialization ------') >= 2, 60, 0.5)
    wait_for(lambda: getinfo(12203), 60, 0.5)
    time.sleep(1.5)
    packets, _ = ask(12300, b'\\status\\', 1.0)
    session.check('single player (g_gametype 0) answers nothing', not packets,
                  gametype=re.findall(r'g_gametype[^\n]*', rcon(12203, password, 'g_gametype') or '')[:1])

    rcon(12203, password, 'quit')
    rcon(12204, password, 'quit')
    exits = [wait_for(lambda p=p: p.poll() is not None, 20, 0.5) for p in (first, second)]
    packets, _ = ask(12301, b'\\status\\', 1.0)
    session.check('both servers quit and their ports close', all(exits) and not packets and free(12300)
                  and free(12301), exits=[p.poll() for p in (first, second)])
    session.result['logs'] = {'a': str(first_log), 'b': str(second_log)}
    for name, path in (('a', first_log), ('b', second_log)):
        text = log_text(path)
        session.result.setdefault('errors', {})[name] = [line for line in text.splitlines()
                                                          if re.search(r'ERROR|WARNING: (?!.*sound)', line)][:20]


def listen_case(session, runtime, password, expect_answers):
    exe = runtime / 'fntech3.exe'
    settings = dict(com_gameProfile='mohaa', fs_basepath=str(INSTALL), fs_homepath='<home>', developer='1',
                    logfile='2', net_ip='127.0.0.1', net_enabled='1', net_port='12223', rconPassword=password,
                    r_fullscreen='0', r_mode='-1', r_customwidth='800', r_customheight='600', s_volume='0',
                    s_musicvolume='0', in_mouse='0', in_nograb='1', name='ProbeHost', g_gametype='2',
                    sv_hostname='GameSpy listen probe', sv_maxclients='8', sv_gamespy='1')
    client, log = session.start('listen', exe, settings, ['+map', 'dm/mohdm2'], runtime)
    started = wait_for(lambda: 'CL_FirstSnapshot' in log_text(log), 120, 0.5)
    session.check('the listen server starts and its client plays', started)
    time.sleep(2.0)
    info = getinfo(12223)
    session.result['getinfo'] = info
    session.check('a team match listen server answers getinfo' if expect_answers
                  else 'HEAD: a team match listen server ignores getinfo',
                  bool(info) == expect_answers, info=info)
    found = lan_search()
    session.result['lanSearch'] = [list(s) for s in found]
    session.check('the LAN search finds it at 12300' if expect_answers else 'HEAD: the LAN search finds nothing',
                  found == ([('127.0.0.1', 12300)] if expect_answers else []), found=found)
    if expect_answers:
        packets, ms = ask(12300, b'\\status\\')
        problems, pairs = decode(packets)
        values = dict(pairs)
        session.result['status'] = dict(packets=[p.decode('latin-1') for p in packets], ms=ms)
        session.check('its \\status\\: a listen team match with its player',
                      not problems and values.get('dedicated') == '0' and values.get('gametype_i') == '2'
                      and values.get('gametype') == 'Team-Match' and values.get('numplayers') == '1'
                      and values.get('hostport') == '12223' and values.get('player_0') == 'ProbeHost'
                      and values.get('mapname') == 'mohdm2', values=values, problems=problems)
    # a second client joins over UDP
    guest_settings = dict(com_gameProfile='mohaa', fs_basepath=str(INSTALL), fs_homepath='<home>', developer='1',
                          logfile='2', net_ip='127.0.0.1', net_enabled='1', net_port='12233', r_fullscreen='0',
                          r_mode='-1', r_customwidth='640', r_customheight='480', s_volume='0',
                          s_musicvolume='0', in_mouse='0', in_nograb='1', name='ProbeGuest')
    guest, guest_log = session.start('guest', exe, guest_settings, ['+connect', '127.0.0.1:12223'], runtime)
    joined = wait_for(lambda: 'CS_ACTIVE for ProbeGuest' in log_text(log), 60 if expect_answers else 30, 0.5)
    session.result['guestLogTail'] = log_text(guest_log).splitlines()[-6:]
    session.check('a remote client joins it' if expect_answers else 'HEAD: a remote client cannot join it',
                  bool(joined) == expect_answers, guest=session.result['guestLogTail'])
    if expect_answers:
        packets, ms = ask(12300, b'\\players\\')
        problems, pairs = decode(packets)
        values = dict(pairs)
        session.result['playersWithGuest'] = dict(packets=[p.decode('latin-1') for p in packets], ms=ms)
        session.check('its player list names both', not problems and values.get('player_0') == 'ProbeHost'
                      and values.get('player_1') == 'ProbeGuest', values=values)
    guest.terminate()
    if expect_answers:
        # into the Allies with a rifle, which spawns the player, then the
        # game's own suicide
        session.result['rcon'] = [rcon(12223, password, 'cmd join_team allies')]
        time.sleep(1.5)
        session.result['rcon'].append(rcon(12223, password, 'cmd primarydmweapon rifle'))
        # the game refuses `kill` for five seconds after a spawn (OpenMoHAA
        # Player::Kill reads it so)
        time.sleep(7.0)
        session.result['rcon'].append(rcon(12223, password, 'cmd kill'))
        time.sleep(3.0)
        packets, ms = ask(12300, b'\\players\\')
        problems, pairs = decode(packets)
        values = dict(pairs)
        session.result['playersAfterKill'] = dict(packets=[p.decode('latin-1') for p in packets], ms=ms)
        session.check('after its kill the player list counts a death', not problems
                      and values.get('player_0') == 'ProbeHost' and values.get('deaths_0') == '1', values=values)
    rcon(12223, password, 'quit')
    exited = wait_for(lambda: client.poll() is not None, 30, 0.5)
    session.check('the client quits', exited, exit=client.poll())
    session.result['log'] = str(log)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', action='append', required=True, help='label=dir')
    parser.add_argument('--output', type=Path, default=ROOT / '.tmp' / 'mohaa-gamespy' / 'live')
    parser.add_argument('--cases', default='dedicated,listen')
    args = parser.parse_args()
    for port in (12203, 12204, 12223, *LAN_PORTS, 12350):
        if not free(port):
            sys.exit(f'port {port} is in use; run when no other Medal of Honor server is up')
    before = inventory(INSTALL)
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
                if case == 'dedicated':
                    dedicated_case(session, runtime, password, expect_answers=label != 'before')
                else:
                    listen_case(session, runtime, password, expect_answers=label != 'before')
            except Exception as error:  # noqa: BLE001 -- recorded, then the next run
                session.check('the run completes', False, error=repr(error))
            finally:
                session.stop_all()
            session.result['passed'] = all(c['ok'] for c in session.result['checks'])
            results['runs'][name] = session.result
            (session.dir / 'result.json').write_text(json.dumps(session.result, indent=1), encoding='utf-8')
    results['installationUnchanged'] = inventory(INSTALL) == before
    print('installation unchanged:', results['installationUnchanged'])
    out = args.output / stamp / 'results.json'
    out.write_text(json.dumps(results, indent=1), encoding='utf-8')
    print(out)
    return 0 if results['installationUnchanged'] and all(r['passed'] for r in results['runs'].values()) else 1


if __name__ == '__main__':
    sys.exit(main())
