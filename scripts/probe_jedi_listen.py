#!/usr/bin/env python3
"""Jedi listen servers and their game types, live over loopback UDP.

For each fnTech3 runtime named (``--runtime label=dir``, a client build with
``-Djka-development=true`` and ``-Djk2-development=true``), this starts the
``jka-mp`` and ``jk2-mp`` servers from the client executable, whose server
compiles the listen-server test (``SV_GameTypeHidesListenServer``), and asks
them what a LAN browser and a joining player ask: ``getinfo``, ``getstatus``
and ``getchallenge``.

- ``jka-listen``: a client hosting with ``+set g_gametype 2 +map mp/ffa1``
  on 29070 with a player named ProbeHost, recorded to where it stops:
  Academy's retail game refuses Jedi Master, and the host's own client game
  stops at its first bolt query, whose clock fnTech3 serves for a remote
  client only (record ``2026-10-02-jka-compatibility.md``), taking the
  listen server down with it.
- ``jka-host``, ``jka-host-sp``: so the client executable hosts with
  ``dedicated 1`` -- the same compiled server, the listen test included,
  without its own client game -- by ``+set g_gametype 2 +map mp/ffa1`` and
  by ``+spmap mp/ffa1``: getinfo, getstatus and getchallenge, then a second
  client (ProbeGuest, port 29071) connects and enters. It stays on that
  first map: fnTech3's Academy client game stops at its first Ghoul2
  attachment after a map change (record ``2026-10-06-jka-live-join.md``).
- ``jka-types``, ``jk2-types``: the same dedicated-mode server through the
  commands that set a game type: Jedi Master by ``map``, Jedi Master by
  ``g_gametype 2`` and ``map_restart``, single player by ``spmap``, and
  ``map`` again from single player. After each, rcon reads the running
  ``g_gametype`` and the three queries are asked.

Windows only: every process runs on a private desktop with its own home,
windowed, silent, IPv4 alone on 127.0.0.1, with no input. The installations
are read only and their inventories are compared before and after. No retail
program runs: retail ``jamp.exe`` needs disc 1 to connect, and Jedi
Outcast's fnTech3 client stops at UI import 200 before it can ask a server
anything (record ``2026-10-05-legacy-server-hosting.md``).
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

OOB = b'\xff\xff\xff\xff'
GAMES = {
    'jka': dict(profile='jka-mp', install=Path(r'E:\Games\Jedi Academy'), port=29070, map='mp/ffa1',
                config='jampconfig.cfg', single=5),
    'jk2': dict(profile='jk2-mp', install=Path(r'E:\Games\Jedi Outcast'), port=28070, map='ffa_bespin',
                config='jk2mpconfig.cfg', single=4),
}

# What each step must show, per runtime label: the running game type,
# whether the three queries are answered, and how often the game refused the
# game type it was handed. "before" is HEAD 192c321f, whose map command
# turned Jedi Master, 2, into a free-for-all, whose spmap set Quake III's 2,
# and whose listen test hid game type 2 in both games. Academy's retail game
# (1.01 jampgamex86.dll, G_RegisterCvars 0x2008ea00) refuses Holocron, Jedi
# Master and Capture the Ysalamiri and sets g_gametype back to 0, so no step
# of its keeps 2; Outcast's game plays Jedi Master on ffa_bespin.
STEPS = ('map at g_gametype 2', 'g_gametype 2 and map_restart', 'spmap', 'map from single player')
EXPECTED = {
    ('jka', 'after'): ((0, True, 1), (0, True, 1), (5, True, 0), (5, True, 0)),
    ('jka', 'before'): ((0, True, 0), (0, True, 1), (0, True, 1), (0, True, 0)),
    ('jk2', 'after'): ((2, True, 0), (2, True, 0), (4, False, 0), (0, True, 0)),
    ('jk2', 'before'): ((0, True, 0), (2, False, 0), (2, False, 0), (0, True, 0)),
}
REFUSED = 'This gametype is not supported.'


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


def info_pairs(text):
    words = text.split('\\')[1:]
    return dict(zip(words[0::2], words[1::2]))


# Every query and rcon from one address shares the server's per-address
# bucket (SVC_RateLimitAddress( from, 10, 1000 )): a burst of ten, then one
# a second. A pause after each packet keeps the probe inside it; a browser
# asks a server once.
PACE = 1.1


def queries(port):
    info = ask(port, 'getinfo probe', 'infoResponse')
    time.sleep(PACE)
    status = ask(port, 'getstatus probe', 'statusResponse')
    time.sleep(PACE)
    challenge = ask(port, 'getchallenge', 'challengeResponse')
    time.sleep(PACE)
    return dict(
        info=info_pairs(info.split('\n', 1)[1]) if info and '\n' in info else None,
        status=info_pairs(status.split('\n')[1]) if status and status.count('\n') >= 1 else None,
        challenge=challenge.strip() if challenge else None)


def rcon(port, password, text, timeout=3.0):
    reply = ask(port, f'rcon {password} {text}\n', 'print', timeout)
    time.sleep(PACE)
    return reply[len('print'):].strip() if reply else None


def settled(port, password, timeout=90.0):
    """Wait out a map load: the server reads the next packet only after the
    command that loads it, so an echo comes back once it is done."""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if 'probe-ready' in (rcon(port, password, 'echo probe-ready', 2.0) or ''):
            return True
        time.sleep(1.0)
    return False


def rcon_load(port, password, text, timeout=120.0):
    """An rcon command that may load a map, with its whole reply: a command's
    prints go to its rcon reply, not the log, flushed in several packets as
    the load runs, and read here once an echo shows the load done."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
        sock.bind(('127.0.0.1', 0))
        sock.sendto(OOB + f'rcon {password} {text}\n'.encode('latin-1'), ('127.0.0.1', port))
        time.sleep(PACE)
        loaded = settled(port, password, timeout)
        chunks = []
        sock.settimeout(0.5)
        while True:
            try:
                data, _ = sock.recvfrom(65535)
            except socket.timeout:
                break
            except ConnectionResetError:
                continue
            body = data[4:].decode('latin-1', 'replace')
            if body.startswith('print'):
                chunks.append(body[len('print'):].lstrip('\n'))
        return ''.join(chunks), loaded


def running_type(port, password, replies=None):
    reply = rcon(port, password, 'g_gametype') or ''
    if replies is not None:
        replies.append(reply)
    match = re.search(r'"g_gametype" is:"(-?\d+)', reply)
    return int(match.group(1)) if match else None


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

    def start(self, label, executable, game, settings, tail, cwd):
        home = self.dir / f'{label}-home'
        (home / 'base').mkdir(parents=True)
        # the installation has no multiplayer configuration, and none leaks in
        for config in (game['config'], game['config'].replace('.cfg', '_server.cfg')):
            (home / 'base' / config).write_text('', encoding='latin-1')
        common = dict(com_gameProfile=game['profile'], fs_basepath=str(game['install']), fs_homepath=str(home),
                      developer='1', logfile='2', net_ip='127.0.0.1', net_enabled='1', r_fullscreen='0',
                      r_mode='-1', r_customwidth='640', r_customheight='480', s_volume='0', s_musicvolume='0',
                      # Academy's client game stops at voice volume with a
                      # sound backend running (snd_main.cpp's named refusal)
                      s_initsound='0', com_maxfps='60', in_mouse='0', in_nograb='1')
        command = [str(executable)]
        for key, value in {**common, **settings}.items():
            command += ['+set', key, str(value)]
        command += tail
        process = WindowsPrivateProcess(command, cwd, self.dir / f'{label}-stdout.log')
        self.processes.append(process)
        self.result.setdefault('processes', {})[label] = dict(
            command=[c if c != settings.get('rconPassword') else '<password>' for c in command],
            sha256=sha(executable))
        return process, home / 'base' / 'qconsole.log'

    def stop_all(self):
        for process in self.processes:
            try:
                process.close()
            except OSError as error:
                self.result.setdefault('stopErrors', []).append(str(error))
        self.processes = []


CLOCK_STOP = ('Academy cgame bolt queries require the measured remote-client frame clock; '
              'local-server clock sharing is not served yet')


def refusal_check(session, log, label, single):
    """HEAD's map turns 2 into a free-for-all before the game sees it, and its
    spmap sets 2, which the retail game refuses; this change's map leaves 2
    to the game, which refuses it, and its spmap sets Academy's 5."""
    refusals = log_text(log).count(REFUSED)
    session.result['gameRefusals'] = refusals
    refused = (single and label != 'after') or (not single and label == 'after')
    session.check('the retail game is handed Jedi Master and refuses it' if refused
                  else 'the retail game is handed no Jedi Master', refusals == (1 if refused else 0),
                  refusals=refusals)


def listen_case(session, runtime, label, password):
    """A client hosting by `map` at g_gametype 2, to where it stops."""
    game = GAMES['jka']
    exe = runtime / 'fntech3.exe'
    port = game['port']
    host, log = session.start('host', exe, game, dict(net_port=port, rconPassword=password, name='ProbeHost',
                                                      sv_maxclients=8, g_gametype=2,
                                                      sv_hostname='Jedi listen probe'),
                              ['+map', game['map']], runtime)
    entered = wait_for(lambda: 'CS_ACTIVE for ProbeHost' in log_text(log), 120, 0.5)
    session.check('the listen server starts and its own client enters', entered)
    refusal_check(session, log, label, single=False)
    stopped = wait_for(lambda: f'Server crashed: {CLOCK_STOP}' in log_text(log), 60, 0.5)
    session.check("the host's client game stops at its first bolt query, the open local-server clock, "
                  'and the listen server with it', stopped)
    session.result['logTail'] = [line for line in log_text(log).splitlines() if 'ERROR' in line or 'crashed' in line]
    session.result['log'] = str(log)


def host_case(session, runtime, label, password, single):
    """The client executable hosting with dedicated 1, by `map` at
    g_gametype 2 or by `spmap`; a second client joins."""
    game = GAMES['jka']
    exe = runtime / 'fntech3.exe'
    port = game['port']
    settings = dict(dedicated=1, net_port=port, rconPassword=password, sv_maxclients=8,
                    sv_hostname='Jedi listen probe')
    if not single:
        settings['g_gametype'] = 2
    host, log = session.start('host', exe, game, settings, ['+spmap' if single else '+map', game['map']], runtime)
    started = wait_for(lambda: 'CM_LoadMap' in log_text(log), 120, 0.5) and settled(port, password, 120.0)
    session.check('the client executable starts a server', started)
    refusal_check(session, log, label, single)
    answers = queries(port)
    session.result['queries'] = answers
    info = answers['info'] or {}
    want_type = 5 if single and label == 'after' else 0
    session.check(f'getinfo answers with game type {want_type}',
                  answers['info'] is not None and info.get('gametype') == str(want_type), info=info)
    session.check('getinfo names the server on the profile protocol',
                  info.get('hostname') == 'Jedi listen probe' and info.get('protocol') == '26'
                  and info.get('mapname') == 'mp/ffa1' and info.get('clients') == '0', info=info)
    session.check('getstatus answers', answers['status'] is not None
                  and answers['status'].get('g_gametype') == str(want_type), status=answers['status'])
    session.check('getchallenge answers', bool(answers['challenge']), challenge=answers['challenge'])
    guest, guest_log = session.start('guest', exe, game, dict(net_port=port + 1, name='ProbeGuest'),
                                     ['+connect', f'127.0.0.1:{port}'], runtime)
    joined = wait_for(lambda: 'CS_ACTIVE for ProbeGuest' in log_text(log), 90, 0.5)
    session.check('a second client connects and enters', joined, guest=log_text(guest_log).splitlines()[-8:])
    after = ask(port, 'getinfo probe', 'infoResponse')
    time.sleep(PACE)
    session.result['infoWithGuest'] = info_pairs(after.split('\n', 1)[1]) if after else None
    session.check('getinfo then counts it', (session.result['infoWithGuest'] or {}).get('clients') == '1',
                  info=session.result['infoWithGuest'])
    session.result['gametypeRcon'] = running_type(port, password)
    session.check('rcon reads the running game type', session.result['gametypeRcon'] == want_type,
                  gametype=session.result['gametypeRcon'])
    time.sleep(3.0)
    session.result['guestLogTail'] = log_text(guest_log).splitlines()[-8:]
    session.result['guestErrors'] = [line for line in log_text(guest_log).splitlines() if 'ERROR' in line]
    guest.terminate()
    rcon(port, password, 'quit')
    exited = wait_for(lambda: host.poll() is not None, 30, 0.5)
    session.check('the server quits', exited, exit=host.poll())
    session.result['log'] = str(log)
    session.result['guestLog'] = str(guest_log)


def types_case(session, runtime, label, password, name):
    game = GAMES[name]
    exe = runtime / 'fntech3.exe'
    port = game['port']
    server, log = session.start('server', exe, game, dict(dedicated=1, net_port=port, rconPassword=password,
                                                          g_gametype=2, sv_maxclients=8,
                                                          sv_hostname='Jedi game type probe'),
                                ['+map', game['map']], runtime)
    started = wait_for(lambda: 'CM_LoadMap' in log_text(log), 120, 0.5) and settled(port, password, 120.0)
    session.check('the server starts and answers rcon', started)
    commands = (None, ('g_gametype 2', 'map_restart 0'), (f'spmap {game["map"]}',), (f'map {game["map"]}',))
    want = EXPECTED.get((name, label))
    session.result['steps'] = []
    for index, (step, sends) in enumerate(zip(STEPS, commands)):
        replies, loaded = [], True
        # the first load's prints are in the log, every later one's in its reply
        refusals = log_text(log).count(REFUSED) if not sends else 0
        for text in sends or ():
            reply, done = rcon_load(port, password, text)
            refusals += reply.count(REFUSED)
            loaded = loaded and done
            replies.append(reply if len(reply) < 200 else reply[:120] + ' ... ' + reply[-60:])
        running = running_type(port, password, replies)
        answers = queries(port)
        info = answers['info'] or {}
        answered = [answers['info'] is not None, answers['status'] is not None, answers['challenge'] is not None]
        record = dict(step=step, rcon=sends, replies=replies, loaded=loaded, gametype=running, answered=answered,
                      gameRefusals=refusals,
                      info=answers['info'], challenge=answers['challenge'])
        session.result['steps'].append(record)
        print(f'    {step}: g_gametype {running}, getinfo/getstatus/getchallenge {answered}, '
              f'game refusals {record["gameRefusals"]}')
        if want:
            game_type, answers_all, refused = want[index]
            session.check(f'{step}: game type {game_type}, ' + ('answered' if answers_all else 'silent')
                          + (f', the game refusing {refused}' if refused else ''),
                          loaded and running == game_type and answered == [answers_all] * 3
                          and (not answers_all or info.get('gametype') == str(game_type))
                          and refusals == refused, **record)
    rcon(port, password, 'quit')
    exited = wait_for(lambda: server.poll() is not None, 30, 0.5)
    session.check('the server quits', exited, exit=server.poll())
    session.result['log'] = str(log)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', action='append', required=True, help='label=dir (label before or after)')
    parser.add_argument('--output', type=Path, default=ROOT / '.tmp' / 'jedi-listen' / 'live')
    parser.add_argument('--cases', default='jka-listen,jka-host,jka-host-sp,jka-types,jk2-types')
    args = parser.parse_args()
    for port in (28070, 29070, 29071):
        if not free(port):
            sys.exit(f'port {port} is in use; run when no other Jedi server is up')
    before = {name: inventory(game['install']) for name, game in GAMES.items()}
    stamp = time.strftime('%Y%m%d-%H%M%S')
    results = dict(installations={name: str(game['install']) for name, game in GAMES.items()}, started=stamp,
                   runs={})
    for item in args.runtime:
        label, directory = item.split('=', 1)
        runtime = Path(directory).resolve()
        for case in args.cases.split(','):
            name = f'{label}-{case}'
            print(name)
            session = Session(args.output / stamp / name)
            password = secrets.token_hex(6)
            try:
                if case == 'jka-listen':
                    listen_case(session, runtime, label, password)
                elif case in ('jka-host', 'jka-host-sp'):
                    host_case(session, runtime, label, password, single=case.endswith('-sp'))
                else:
                    types_case(session, runtime, label, password, case.split('-')[0])
            except Exception as error:  # noqa: BLE001 -- recorded, then the next run
                session.check('the run completes', False, error=repr(error))
            finally:
                session.stop_all()
            session.result['passed'] = all(c['ok'] for c in session.result['checks'])
            results['runs'][name] = session.result
            (session.dir / 'result.json').write_text(json.dumps(session.result, indent=1), encoding='utf-8')
            time.sleep(2.0)
    results['installationsUnchanged'] = all(inventory(game['install']) == before[name]
                                            for name, game in GAMES.items())
    print('installations unchanged:', results['installationsUnchanged'])
    out = args.output / stamp / 'results.json'
    out.write_text(json.dumps(results, indent=1), encoding='utf-8')
    print(out)
    return 0 if results['installationsUnchanged'] and all(r['passed'] for r in results['runs'].values()) else 1


if __name__ == '__main__':
    sys.exit(main())
