#!/usr/bin/env python3
"""Enemy Territory's map_restart: the level times its game runs at.

Windows only. Runs retail etded.exe and then one fnTech3 dedicated server
(--server), each on an unswitched private desktop with an isolated home,
loopback networking and no clients. No input is injected and nothing is
rendered.

Both load goldrush with devmap and set g_scriptDebug, a cheat cvar, so the
retail game module prints the level time, in milliseconds, of every map
script event and command. The spawn events print while GAME_INIT spawns the
level, at the time it was given; every later line prints from a game frame at
that frame's time. A map_restart therefore leaves a timeline of the frames it
ran, relative to the level's start, which the probe takes from the log after
the restart's "Game Initialization" banner. The run passes when each restart's
timeline matches retail etded.exe's line for line and the installation is
unchanged.

Each step, and a marker after it, is a config in the isolated home run through
a loopback rcon "exec", paced a second apart: retail ET silently drops an rcon
command handled within 500 ms of the last one, and a dedicated server's echo
logs nothing, so a marker prints a cvar instead. A level's marker waits until
its timeline has passed the compared span. Meanwhile the probe sends an info
query every 50 ms, because retail etded.exe with no clients blocks for
seconds between frames and runs the missed ones only after the next packet,
behind whatever command that packet brought.
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

# (marker, console commands, seconds waited afterwards). The level loads from
# the startup config; the other steps run by rcon.
STEPS = (
    ('loaded', [], 0.0),
    ('script-debug', [['set', 'g_scriptDebug', '1']], 1.0),
    ('first-restart', [['map_restart', '0']], 3.0),
    ('second-restart', [['map_restart', '0']], 3.0),
)
RESTARTS = ('first-restart', 'second-restart')
MARK = 'fntech3_settle_mark_'
BANNER = 'Game Initialization'
PREFIX = 'fntech3-restart-settle'
# Lines from the first 2.5 s of each restarted level are compared.
HORIZON = 2500
DEBUG_LINE = re.compile(r'^(-?\d+) : \(([^)\r\n]*)\) ([^\r\n]*?)\r?$', re.M)
RCON_GAP = 1.0
RCON_TRIES = 3


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def marker_line(marker):
    return '"%s%s" is:' % (MARK, marker.replace('-', '_'))


def printed(text, name):
    match = re.search(r'^"%s" is:"(.*?)(?:\^7)?"' % re.escape(name), text, re.M)
    return match.group(1) if match else None


def read(log):
    try:
        return log.read_text(encoding='utf-8', errors='replace')
    except OSError:
        return ''


def debug_lines(text):
    """Script-debug lines after the last level start in text."""
    banner = text.rfind(BANNER)
    if banner < 0:
        return []
    return [(int(m.group(1)), m.group(2), m.group(3).rstrip()) for m in DEBUG_LINE.finditer(text, banner)]


def beyond_horizon(text):
    lines = debug_lines(text)
    return bool(lines) and lines[-1][0] - lines[0][0] > HORIZON


def timeline(text):
    """Script-debug lines after the last level start in text, as offsets from it."""
    lines = debug_lines(text)
    if not lines:
        return None
    start = lines[0][0]
    kept = [[t - start, name, what] for t, name, what in lines if t - start <= HORIZON]
    # The lines quote the retail map script, so evidence keeps their digest.
    return dict(levelStart=start, lines=kept, lineCount=len(kept),
                digest=hashlib.sha256(json.dumps(kept).encode('utf-8')).hexdigest(),
                frameOffsets=sorted({t for t, _, _ in kept}))


class Rcon:
    def __init__(self, port, password):
        self.address = ('127.0.0.1', port)
        self.password = password
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.settimeout(0.1)
        self.last = 0.0
        self.replies = []

    def receive(self, prefix, until, seconds):
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
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.socket.sendto(b'\xff\xff\xff\xffgetinfo fntech3', self.address)
            if 'fntech3' in self.receive(b'infoResponse\n', 'fntech3', 0.3):
                return True
        return False

    def tick(self):
        """Wake the server. Retail etded.exe with no clients otherwise blocks
        for seconds between frames, then runs the missed ones behind whatever
        command the next packet brought."""
        self.socket.sendto(b'\xff\xff\xff\xffgetinfo tick', self.address)
        self.receive(b'\0', '\0', 0.05)

    def send(self, command, expect=None):
        for _ in range(RCON_TRIES if expect else 1):
            pause = self.last + RCON_GAP - time.monotonic()
            if pause > 0:
                time.sleep(pause)
            self.socket.sendto(b'\xff\xff\xff\xffrcon %s %s' % (self.password.encode(), command.encode()),
                               self.address)
            self.last = time.monotonic()
            reply = self.receive(b'print\n', expect or '\0', 1.5 if expect else 0.3)
            self.replies.append(dict(command=command, reply=reply[-200:]))
            if not expect or expect in reply:
                return
        raise TimeoutError(f'rcon {command!r} never ran')

    def close(self):
        self.socket.close()


def run(installation, server, retail, output, level, timeout):
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    (home / 'etmain').mkdir(parents=True)
    before = inventory(installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    password = secrets.token_hex(8)
    values = dict(fs_basepath=str(installation), fs_homepath=str(home), dedicated='1', logfile='2',
                  net_ip='127.0.0.1', net_port=str(port), sv_pure='0', sv_punkbuster='0', sv_fps='20',
                  g_gametype='2', rconpassword=password, **{f'sv_master{i}': '' for i in range(1, 6)})
    if not retail:
        values = dict(com_gameProfile='wolfet', **values)
    configs = {f'{PREFIX}.cfg': [f'set {MARK}{m.replace("-", "_")} 1' for m, *_ in STEPS]
               + ['set g_scriptDebug 1', f'devmap {level}']}
    for index, (marker, actions, _) in enumerate(STEPS):
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
                  else 'Live: fnTech3 dedicated server', profile='wolfet', retailServer=retail, map=level,
                  command=[part if part != password else '<random>' for part in command], configs=configs,
                  serverSha256=sha(server), moduleSha256=sha(installation / 'etmain/qagame_mp_x86.dll'),
                  pak0Sha256=sha(installation / 'etmain/pak0.pk3'))
    begin = time.monotonic()
    host = rcon = None

    def wait_for(predicate, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline and host.poll() is None:
            if predicate(read(log)):
                return True
            if rcon:
                rcon.tick()
            else:
                time.sleep(0.05)
        return predicate(read(log))

    try:
        host = WindowsPrivateProcess(command, server.parent, output / 'server-stdout.log')
        if not wait_for(lambda text: BANNER in text, timeout):
            raise TimeoutError('the level never started')
        rcon = Rcon(port, password)
        if not rcon.ready(timeout):
            raise TimeoutError('the server never answered')
        for index, (marker, actions, seconds) in enumerate(STEPS):
            before_step = len(read(log)) if actions else 0
            if actions:
                rcon.send(f'exec {PREFIX}-step{index}.cfg', f'execing {PREFIX}-step{index}.cfg')
                wait_for(lambda text: False, seconds)
            # A level's marker waits until the level has run past the compared
            # span, whatever the frames' pace in real time.
            if marker in RESTARTS + ('loaded',) and not wait_for(
                    lambda text: beyond_horizon(text[before_step:]), 30):
                raise TimeoutError(f'{marker} never ran {HORIZON} ms of its level')
            rcon.send(f'exec {PREFIX}-mark{index}.cfg', f'execing {PREFIX}-mark{index}.cfg')
            if not wait_for(lambda text: marker_line(marker) in text, 5):
                raise TimeoutError(f'marker {marker} never reached the log')
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
    steps = {}
    start = 0
    for marker, actions, _ in STEPS:
        end = text.find(marker_line(marker), start)
        chunk = text[start:end] if end >= 0 else ''
        steps[marker] = dict(commands=[' '.join(a) for a in actions],
                             levelStarts=chunk.count(BANNER),
                             gamestate=printed(text[end:], 'gamestate') if end >= 0 else None,
                             timeline=timeline(chunk))
        start = end + 1 if end >= 0 else len(text)
    result['steps'] = steps
    result['loadTimeline'] = timeline(text[:text.find(marker_line('loaded'))]) if marker_line('loaded') in text else None
    result['variableChangeRestarts'] = len(re.findall(r'variable change -- restarting', text))
    result['networkLookups'] = [line for line in text.splitlines() if line.startswith('Resolving ')]
    result['errors'] = [line for line in text.splitlines() if line.startswith('ERROR') or 'Error:' in line]
    result['cheatProtected'] = 'cheat protected' in text
    result['rconAccepted'] = len(re.findall(r'^Rcon from ', text, re.M))
    result['rconRan'] = sum(1 for r in result.get('rcon', []) if 'execing ' in r['reply'])
    result['installationUnchanged'] = before == inventory(installation)
    result['clean'] = bool(not result.get('error') and not result.get('timedOut') and not result['errors']
                           and result['installationUnchanged'] and not result['cheatProtected']
                           and result['rconAccepted'] == result['rconRan'] + 1
                           and all(steps[r]['levelStarts'] == 1 and steps[r]['timeline'] for r in RESTARTS))
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    return result


def lines_of(result, step):
    found = result['steps'][step]['timeline']
    return found['lines'] if found else None


def lines_of_load(result):
    found = result['loadTimeline']
    return found['lines'] if found else None


def divergence(retail, engine):
    """The first line at which two timelines differ, as [retail, fnTech3]."""
    if retail is None or engine is None or retail == engine:
        return None
    for index, pair in enumerate(zip(retail, engine)):
        if pair[0] != pair[1]:
            return dict(index=index, retail=pair[0], fntech3=pair[1])
    index = min(len(retail), len(engine))
    return dict(index=index, retail=retail[index] if index < len(retail) else None,
                fntech3=engine[index] if index < len(engine) else None)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--server', type=Path, required=True, help='fnTech3 dedicated executable')
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail ET 2.60b root holding etded.exe and etmain/')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--map', default='goldrush')
    parser.add_argument('--timeout', type=float, default=60, help='seconds allowed for the level to load')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This installed native-module probe requires Windows')
    installation = args.installation.resolve(strict=True)
    server = args.server.resolve(strict=True)
    output = args.output.resolve()
    retail = run(installation, installation / 'etded.exe', True, output / 'retail', args.map, args.timeout)
    engine = run(installation, server, False, output / 'fntech3', args.map, args.timeout)
    comparison = {step: lines_of(engine, step) == lines_of(retail, step) for step in RESTARTS}
    servers = (('retail', retail), ('fntech3', engine))

    def each(field, step):
        return {name: (r['steps'][step]['timeline'] or {}).get(field) for name, r in servers}

    # The initial load is reported, not judged: SV_SpawnServer is not changed
    # here.
    load = [lines_of_load(r) for _, r in servers]
    summary = dict(
        retailSha256=retail['serverSha256'], serverSha256=engine['serverSha256'],
        moduleSha256=retail['moduleSha256'], pak0Sha256=retail['pak0Sha256'],
        retailClean=retail['clean'], serverClean=engine['clean'],
        restartMatchesRetail=comparison,
        restartLineCounts={step: each('lineCount', step) for step in RESTARTS},
        restartDigests={step: each('digest', step) for step in RESTARTS},
        restartFrameOffsets={step: each('frameOffsets', step) for step in RESTARTS},
        firstDivergence={step: divergence(lines_of(retail, step), lines_of(engine, step)) for step in RESTARTS},
        loadFrameOffsets={name: ((r['loadTimeline'] or {}).get('frameOffsets') or [])[:12] for name, r in servers},
        loadMatchesRetail=load[0] == load[1] if all(load) else None,
        loadFirstDivergence=divergence(*load),
        gamestate={name: [s['gamestate'] for s in r['steps'].values()] for name, r in servers},
        passed=retail['clean'] and engine['clean'] and all(comparison.values()))
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))
    return 0 if summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
