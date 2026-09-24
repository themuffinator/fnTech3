#!/usr/bin/env python3
"""RTCW's map load and map_restart: the level times their settling frames run at.

Windows only. Runs the retail engine of the chosen RTCW mode, WolfMP.exe or
WolfSP.exe, as a dedicated server, and then each fnTech3 dedicated server given
with --server, each on an unswitched private desktop with an isolated home,
loopback networking and no clients. No input is injected and nothing is
rendered.

Every server loads a map with g_scriptDebug set. Both RTCW games register that
cvar without the cheat flag, and their map scripts then print the level time,
in milliseconds, of each command they reach. A spawn event's script starts while
GAME_INIT spawns the level, so its first command prints at the time GAME_INIT
was given. Every later line prints from a game frame, at that frame's time,
and a frame prints a script's current command again when that command began
at the frame's own time. So the lines after each "Game Initialization" banner
give the frames that level ran, relative to its start. The summary says, for
every fnTech3 server, whether its load and restarts match the retail engine's
line for line. The run passes when every server ran cleanly, the installation
is unchanged, and each server named by --require-match matched in every level.

Each step, and a marker after it, is a config in the isolated home run through
a loopback rcon "exec", paced a second apart, as the Enemy Territory settle
probe does. A level's marker waits until its timeline has passed the compared
span, and the probe sends an info query every 50 ms meanwhile, so an idle
server that blocks between frames still runs them. Retail WolfSP.exe opens no
socket, so its steps and markers all run from its startup config instead. The
lines quote the retail map script, so result files keep them but summaries
keep only their digests.
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
    ('first-restart', [['map_restart', '0']], 3.0),
    ('second-restart', [['map_restart', '0']], 3.0),
)
LEVELS = ('loaded', 'first-restart', 'second-restart')
MARK = 'fntech3_settle_mark_'
BANNER = 'Game Initialization'
PREFIX = 'fntech3-settle-order'
# Lines from the first 2.5 s of each level are compared.
HORIZON = 2500
DEBUG_LINE = re.compile(r'^(-?\d+) : \(([^)\r\n]*)\) ([^\r\n]*?)\r?$', re.M)
RCON_GAP = 1.0
RCON_TRIES = 3
# Each mode's retail server, game module, default map and own settings. A map
# whose spawn scripts wait shows the first frame's time: mp_rocket's and
# rocket's wait in many. rocket's scripts fall silent after about 550 ms, so
# its levels are given a fixed time with the server kept awake, in seconds,
# rather than a line past the compared span; a server that fell behind would
# then show a shorter timeline, not a matching one.
#
# Retail WolfSP.exe's dedicated server opens no network socket, so it cannot be
# paced by rcon. Its steps run from the startup config instead, each followed
# by console waits, which it counts once per command-buffer pass. With a
# config to run it makes a pass about every 11 ms, and each pass runs the game
# frames due since the last, so 300 waits leave each level past the compared
# span; the result records how far each level ran.
MODES = {
    'rtcw-mp': dict(retail='WolfMP.exe', module='Main/qagame_mp_x86.dll', map='mp_rocket',
                    values=dict(sv_punkbuster='0', bot_enable='0', g_gametype='5'), quiet=None,
                    retailWaits=None),
    'rtcw-sp': dict(retail='WolfSP.exe', module='qagamex86.dll', map='rocket',
                    values=dict(com_recommendedSet='1'), quiet=5.0, retailWaits=300),
}


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
    return dict(levelStart=start, lines=kept, lineCount=len(kept),
                digest=hashlib.sha256(json.dumps(kept).encode('utf-8')).hexdigest(),
                frameOffsets=sorted({t for t, _, _ in kept}),
                linesAtStart=sum(1 for t, _, _ in kept if t == 0))


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
        """Wake an idle server that would otherwise block between frames."""
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


def run(installation, mode, server, label, output, level, timeout):
    retail = label == 'retail'
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    (home / 'main').mkdir(parents=True)
    before = inventory(installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    password = secrets.token_hex(8)
    values = dict(fs_basepath=str(installation), fs_homepath=str(home), dedicated='1', logfile='2',
                  net_ip='127.0.0.1', net_port=str(port), sv_pure='0', sv_fps='20', g_scriptDebug='1',
                  rconpassword=password, **MODES[mode]['values'], **{f'sv_master{i}': '' for i in range(1, 6)})
    if not retail:
        values = dict(com_gameProfile=mode, **values)
    configs = {f'{PREFIX}.cfg': [f'set {MARK}{m.replace("-", "_")} 1' for m, *_ in STEPS] + [f'map {level}']}
    waits = MODES[mode]['retailWaits'] if retail else None
    for index, (marker, actions, _) in enumerate(STEPS):
        mark = [f'{MARK}{marker.replace("-", "_")}', 'gamestate', 'g_gametype']
        if waits:
            configs[f'{PREFIX}.cfg'] += [' '.join(action) for action in actions] + [f'wait {waits}'] + mark
            continue
        if actions:
            configs[f'{PREFIX}-step{index}.cfg'] = [' '.join(action) for action in actions]
        configs[f'{PREFIX}-mark{index}.cfg'] = mark
    if waits:
        configs[f'{PREFIX}.cfg'].append('quit')
    for name, lines in configs.items():
        (home / 'main' / name).write_text(''.join(line + '\n' for line in lines), encoding='ascii')
    command = [str(server)]
    for name, value in values.items():
        command += ['+set', name, value]
    command += ['+exec', f'{PREFIX}.cfg']
    log = home / 'main' / ('rtcwconsole.log' if retail else 'qconsole.log')
    result = dict(evidence=f'Live: retail {MODES[mode]["retail"]} dedicated server' if retail
                  else 'Live: fnTech3 dedicated server', label=label, profile=mode, map=level,
                  command=[part if part != password else '<random>' for part in command], configs=configs,
                  serverSha256=sha(server), moduleSha256=sha(installation / MODES[mode]['module']))
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

    def paced():
        """Run each step and marker by rcon, then quit."""
        nonlocal rcon
        if not wait_for(lambda text: BANNER in text, timeout):
            raise TimeoutError('the level never started')
        rcon = Rcon(port, password)
        if not rcon.ready(timeout):
            raise TimeoutError('the server never answered')
        quiet = MODES[mode]['quiet']
        for index, (marker, actions, seconds) in enumerate(STEPS):
            before_step = len(read(log)) if actions else 0
            if actions:
                rcon.send(f'exec {PREFIX}-step{index}.cfg', f'execing {PREFIX}-step{index}.cfg')
                wait_for(lambda text: False, seconds)
            # A level's marker waits until the level has run past the compared
            # span, whatever the frames' pace in real time, or for the mode's
            # fixed time where its scripts fall silent sooner.
            if quiet:
                wait_for(lambda text: False, quiet)
            elif not wait_for(lambda text: beyond_horizon(text[before_step:]), 30):
                raise TimeoutError(f'{marker} never ran {HORIZON} ms of its level')
            rcon.send(f'exec {PREFIX}-mark{index}.cfg', f'execing {PREFIX}-mark{index}.cfg')
            if not wait_for(lambda text: marker_line(marker) in text, 5):
                raise TimeoutError(f'marker {marker} never reached the log')
        rcon.send('quit')
        return 15

    try:
        host = WindowsPrivateProcess(command, server.parent, output / 'server-stdout.log')
        # A server run from its startup config alone quits at the end of it.
        allowed = timeout + len(STEPS) * waits * 3 if waits else paced()
        deadline = time.monotonic() + allowed
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
                             gametype=printed(text[end:], 'g_gametype') if end >= 0 else None,
                             timeline=timeline(chunk))
        start = end + 1 if end >= 0 else len(text)
    result['steps'] = steps
    result['variableChangeRestarts'] = len(re.findall(r'variable change -- restarting', text))
    result['networkLookups'] = [line for line in text.splitlines() if line.startswith('Resolving ')]
    result['errors'] = [line for line in text.splitlines() if line.startswith('ERROR') or 'Error:' in line]
    result['rconAccepted'] = len(re.findall(r'^Rcon from ', text, re.M))
    result['rconRan'] = sum(1 for r in result.get('rcon', []) if 'execing ' in r['reply'])
    result['installationUnchanged'] = before == inventory(installation)
    result['clean'] = bool(not result.get('error') and not result.get('timedOut') and not result['errors']
                           and result['installationUnchanged']
                           and all(steps[m]['levelStarts'] == 1 and steps[m]['timeline'] for m in LEVELS))
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    return result


def lines_of(result, step):
    found = result['steps'][step]['timeline']
    return found['lines'] if found else None


def divergence(retail, engine):
    """The first line at which two timelines differ, as offsets only."""
    if retail is None or engine is None or retail == engine:
        return None
    for index, pair in enumerate(zip(retail, engine)):
        if pair[0] != pair[1]:
            return dict(index=index, retailOffset=pair[0][0], fntech3Offset=pair[1][0])
    index = min(len(retail), len(engine))
    return dict(index=index, retailOffset=retail[index][0] if index < len(retail) else None,
                fntech3Offset=engine[index][0] if index < len(engine) else None)


def server_argument(text):
    label, _, path = text.partition('=')
    if not label or not path or label == 'retail':
        raise argparse.ArgumentTypeError('expected label=path, with a label other than "retail"')
    return label, Path(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--server', type=server_argument, action='append', required=True,
                        help='label=path of an fnTech3 dedicated executable; repeatable')
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail RTCW root holding WolfMP.exe, WolfSP.exe and Main/')
    parser.add_argument('--profile', choices=sorted(MODES), default='rtcw-mp')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--map', help="the level; defaults to the mode's")
    parser.add_argument('--timeout', type=float, default=60, help='seconds allowed for the level to load')
    parser.add_argument('--require-match', action='append', default=[], metavar='LABEL',
                        help='fail unless this server matches retail in every level; repeatable')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This installed native-module probe requires Windows')
    unknown = set(args.require_match) - {label for label, _ in args.server}
    if unknown:
        parser.error(f'--require-match names no --server: {", ".join(sorted(unknown))}')
    installation = args.installation.resolve(strict=True)
    output = args.output.resolve()
    mode = args.profile
    level = args.map or MODES[mode]['map']
    runs = [('retail', run(installation, mode, installation / MODES[mode]['retail'], 'retail',
                           output / 'retail', level, args.timeout))]
    for label, server in args.server:
        server = server.resolve(strict=True)
        runs.append((label, run(installation, mode, server, label, output / label, level, args.timeout)))
    retail = runs[0][1]

    def each(field, step):
        return {label: (r['steps'][step]['timeline'] or {}).get(field) for label, r in runs}

    def spans(result):
        """How long each level but the last ran: the next level starts where it
        stopped, since a restart begins at the level time it was given."""
        starts = [(result['steps'][step]['timeline'] or {}).get('levelStart') for step in LEVELS]
        return [b - a if a is not None and b is not None else None for a, b in zip(starts, starts[1:])]

    summary = dict(
        profile=mode, map=level,
        retailSha256=retail['serverSha256'], moduleSha256=retail['moduleSha256'],
        servers={label: r['serverSha256'] for label, r in runs[1:]},
        clean={label: r['clean'] for label, r in runs},
        matchesRetail={label: {step: lines_of(r, step) == lines_of(retail, step) for step in LEVELS}
                       for label, r in runs[1:]},
        levelSpans={label: spans(r) for label, r in runs},
        lineCounts={step: each('lineCount', step) for step in LEVELS},
        linesAtStart={step: each('linesAtStart', step) for step in LEVELS},
        digests={step: each('digest', step) for step in LEVELS},
        frameOffsets={step: {label: (offsets or [])[:14] for label, offsets in each('frameOffsets', step).items()}
                      for step in LEVELS},
        firstDivergence={label: {step: divergence(lines_of(retail, step), lines_of(r, step)) for step in LEVELS}
                         for label, r in runs[1:]},
        gamestate={label: [s['gamestate'] for s in r['steps'].values()] for label, r in runs},
        gametype={label: [s['gametype'] for s in r['steps'].values()] for label, r in runs})
    summary['passed'] = bool(all(summary['clean'].values())
                             and all(all(summary['matchesRetail'][label].values()) for label in args.require_match))
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))
    return 0 if summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
