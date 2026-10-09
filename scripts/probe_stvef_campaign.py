#!/usr/bin/env python3
"""Play Elite Force's single-player campaign, level exit by level exit, with
no device input.

New Game starts `map borg1` (efuix86.dll 0x40015dfb), and 44 level exits
follow to `voy20`, whose epilogue plays the closing movie and the credits.
Every exit is a `target_level_change`: its use function sends the engine
`maptransition <map> <target>`, or `loadtransition` for a hub, through the
game's SendConsoleCommand (efgamex86.dll 0x2007dba0 and G_ChangeMap
0x20083740). The probe fires each one with the game's own `use <targetname>`
console command (0x200833b0), which the retail menus also send, once the
level is in play and any opening movie has finished. Where `use` cannot
reach an exit, a level's own trigger is touched by `setviewpos` instead.
The exits and their triggers were read from each map's entity lump and its
compiled ICARUS scripts in the installation's paks (2026-10-08).

The probe drives the client over loopback rcon, not a console script: the
game appends its transition command to the command buffer, behind anything
a script still holds there, so a script's own waits would hold it back. It
records, for each level, the load, the `playersave` and `playerammo*` cvars
the engine carries to the next level, the autosave's map, warnings and
errors, and an engine screenshot. Runs are windowed on an unswitched private
desktop, with an isolated home, no sound and no input; the installation is
read only and its inventory compared before and after.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import secrets
import socket
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

INSTALL = Path(r'E:\Games\Star Trek Voyager Elite Force')
OOB = b'\xff\xff\xff\xff'

# (level, exit, next level, fallbacks). The exit is the targetname of the
# level's target_level_change or of what fires it; a fallback touches the
# exit's own trigger or runs the level's ending script.
CHAIN = [
    ('borg1', 'use t96', 'borg2', ['setviewpos 872 2188 -121 0']),
    ('borg2', 'use t121', 'holodeck', ['setviewpos 1728 -1676 151 0']),
    ('holodeck', 'use loadVoy1', 'voy1', ['use startcin']),
    ('voy1', 'use goto_voy2', 'voy2', ['use t24']),
    ('voy2', 'use t85', 'voy3', ['setviewpos -2048 3846 -181 0']),
    ('voy3', 'use changelevel', 'voy4', ['use nextlevel', 'use t73']),
    ('voy4', 'use changelevel', 'voy5', ['use t1368']),
    ('voy5', 'use loadmap', 'stasis1', ['use t198']),
    ('stasis1', 'use t673', 'stasis2', ['setviewpos -340 2296 159 0']),
    ('stasis2', 'use loadStasis3', 'stasis3', ['use shutdown_display']),
    ('stasis3', 'use loadVoy6', 'voy6', ['setviewpos 2104 -2560 1063 0']),
    ('voy6', 'use goto_voy7', 'voy7', ['use changelevel']),
    ('voy7', 'use t81', 'voy8', ['use godown']),
    ('voy8', 'use goto_scav1', 'scav1', ['use t8']),
    ('scav1', 'use startScav2', 'scav2', []),
    ('scav2', 'use t28', 'scav3', ['setviewpos -680 1576 151 0']),
    ('scav3', 'use t115', 'scav3b', ['setviewpos -392 -80 167 0']),
    ('scav3b', 'use levelchange', 'scav4', ['use endscript']),
    ('scav4', 'use t247', 'scav5', ['setviewpos 768 2164 -171 0']),
    ('scav5', 'use t306', 'scavboss', ['setviewpos 588 1108 -1157 0']),
    ('scavboss', 'use goto_voy9', 'voy9', ['setviewpos -776 -1986 -105 0']),
    ('voy9', 'use goto_borg3', 'borg3', ['use outro']),
    ('borg3', 'use t265', 'borg4', ['setviewpos 404 1952 215 0']),
    ('borg4', 'use endlevel', 'borg5', []),
    ('borg5', 'use t447', 'borg6', ['setviewpos -192 1364 -9 0']),
    ('borg6', 'use goto_voy13', 'voy13', ['use t33']),
    ('voy13', 'use levelchange', 'voy14', ['use t190']),
    ('voy14', 'use loadvoy15', 'voy15', ['setviewpos 908 224 23 0']),
    ('voy15', 'use goto_dn1', 'dn1', ['use t201']),
    ('dn1', 'use t2', 'dn2', ['setviewpos -950 2 1027 0']),
    ('dn2', 'use goto_dn3', 'dn3', ['use newgoals']),
    ('dn3', 'use goto4', 'dn4', ['setviewpos 667 772 455 0']),
    ('dn4', 'use t109', 'dn5', ['setviewpos 692 -1528 -173 0']),
    ('dn5', 'use goto_dn6', 'dn6', ['setviewpos -1388 -348 1067 0']),
    ('dn6', 'use level_change', 'dn8', ['use t79']),
    ('dn8', 'use changelevel', 'voy16', ['setviewpos 1328 -984 -265 0']),
    ('voy16', 'use goto_voy17', 'voy17', ['use t48']),
    ('voy17', 'use goto_forge1', 'forge1', ['setviewpos 1324 -448 63 0']),
    ('forge1', 'use t40', 'forge2', ['setviewpos -2208 808 -1577 0']),
    ('forge2', 'use t31', 'forge3', ['setviewpos 1162 -214 7 0']),
    ('forge3', 'use t331', 'forge4', ['setviewpos -104 -300 -377 0']),
    ('forge4', 'use t1', 'forge5', ['setviewpos 438 -440 247 0']),
    ('forge5', 'use exit', 'forgeboss', ['setviewpos -244 -244 -1553 0']),
    ('forgeboss', 'use changemap', 'voy20', ['use bridge_cin']),
]
FINAL = 'voy20'

# The campaign's optional and required holodeck trips: the level's
# `hololevel` exit saves `exitholodeck` and changes to the holodeck, whose
# `endsim` script runner loads that save again (each holodeck's
# endsim script sets SET_LOADGAME exitholodeck). voy13's turbolift waits for
# its trip. Its brig is a fail state, reached with `gotobrig`; the brig ends
# with the player dead, and `load *respawn` returns to the autosave.
SIDE_TRIPS = {
    'holodeck-voy5': [('voy5', 'use hololevel', '_holodeck_highnoon', []),
                      ('_holodeck_highnoon', 'use endsim', 'voy5', [])],
    'holodeck-voy7': [('voy7', 'use hololevel', '_holodeck_camelot', []),
                      ('_holodeck_camelot', 'use endsim', 'voy7', [])],
    'holodeck-voy13': [('voy13', 'use hololevel', '_holodeck_temple', []),
                       ('_holodeck_temple', 'use endsim', 'voy13', [])],
    'brig-voy1': [('voy1', 'use gotobrig', '_brig', []), ('_brig', 'load *respawn', 'voy1', [])],
    # voy4's gotobrig also fires an exit to `brig`, a map that does not
    # exist, just before the one to `_brig` (entities 413 and 469).
    'brig-voy4': [('voy4', 'use gotobrig', '_brig', []), ('_brig', 'load *respawn', 'voy4', [])],
    # The Virtual Voyager tour (efuix86.dll 0x40017000 sets cg_virtualVoyager
    # 1 and starts tour/deck02): every turbolift exit, `tour_turbo_NN`, is a
    # hub exit, so `loadtransition` keeps the deck left as hub/<deck> and
    # restores a deck visited before. deck02 and deck04 are each come back to.
    'tour': [('tour/deck02', 'use tour_turbo_03', 'tour/deck03', []),
             ('tour/deck03', 'use tour_turbo_02', 'tour/deck02', []),
             ('tour/deck02', 'use tour_turbo_04', 'tour/deck04', []),
             ('tour/deck04', 'use tour_turbo_05', 'tour/deck05', []),
             ('tour/deck05', 'use tour_turbo_08', 'tour/deck08', []),
             ('tour/deck08', 'use tour_turbo_09', 'tour/deck09', []),
             ('tour/deck09', 'use tour_turbo_10', 'tour/deck10', []),
             ('tour/deck10', 'use tour_turbo_11', 'tour/deck11', []),
             ('tour/deck11', 'use tour_turbo_01', 'tour/deck01', []),
             ('tour/deck01', 'use tour_turbo_04', 'tour/deck04', [])],
}
# Console lines a chain needs before its first level.
PRELUDES = {'tour': ['set cg_virtualvoyager 1']}


def chain_steps(name):
    """(level, command, next level, fallbacks) for each level, the last with
    no command: the campaign ends on voy20's epilogue, a side trip back on the
    level it left."""
    if name == 'campaign':
        return [list(step) for step in CHAIN] + [[FINAL, None, None, []]]
    steps = [list(step) for step in SIDE_TRIPS[name]]
    return steps + [[steps[-1][2], None, None, []]]

ERROR = re.compile(r'^(?:\^\d)?(?:ERROR|Server crashed|\*{8,}|.*Exception)', re.M)
WARNING = re.compile(r'^(?:\^\d)?WARNING', re.M)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def inventory(folder):
    return {str(p.relative_to(folder)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in folder.rglob('*') if p.is_file()}


def save_map(path):
    """The map an Elite Force SP save holds: its MPCM chunk. Each chunk is an
    id, its length and a checksum, the data and the 0x1234ABCD magic, every
    word little-endian, the id a four-character constant (so MPCM reads MCPM)."""
    try:
        data = Path(path).read_bytes()
    except OSError:
        return None
    at = 0
    while at + 12 <= len(data):
        ident, length, _ = struct.unpack_from('<4sII', data, at)
        if length > len(data) - at - 16:
            return None
        if ident[::-1] == b'MPCM':
            return data[at + 12:at + 12 + length].split(b'\0')[0].decode('latin1')
        at += 12 + length + 4
    return None


class Client:
    def __init__(self, runtime, home, renderer, first, basepath=INSTALL, prelude=()):
        self.home, self.port, self.password = home, free_port(), secrets.token_hex(6)
        (home / 'BaseEF').mkdir(parents=True)
        # The installation's own config would run otherwise.
        (home / 'BaseEF' / 'efconfig.cfg').write_text('// empty\n', encoding='latin1')
        lines = [f'set rconpassword "{self.password}"', 'developer 1', *prelude, f'devmap {first}']
        (home / 'BaseEF' / 'probe.cfg').write_text('\n'.join(lines) + '\n', encoding='latin1')
        self.command = [str(runtime / 'fntech3.exe'), '+set', 'com_gameProfile', 'stvef-sp',
                        '+set', 'fs_basepath', str(basepath), '+set', 'fs_homepath', str(home),
                        '+set', 'cl_renderer', renderer, '+set', 'r_fullscreen', '0', '+set', 'r_mode', '3',
                        '+set', 'in_mouse', '0', '+set', 'in_joystick', '0', '+set', 'in_nograb', '1',
                        '+set', 's_initsound', '0', '+set', 'net_enabled', '1', '+set', 'net_ip', '127.0.0.1',
                        '+set', 'net_port', str(self.port), '+set', 'logfile', '2',
                        '+set', 'com_hunkMegs', '256', '+set', 'com_zoneMegs', '128',
                        '+set', 'com_introplayed', '1', '+set', 'com_maxfps', '60',
                        '+set', 'com_maxfpsUnfocused', '60', '+set', 'com_maxfpsMinimized', '60',
                        '+set', 'use_ff', '0', '+exec', 'probe.cfg']
        self.sha256 = sha(runtime / 'fntech3.exe')
        self.process = WindowsPrivateProcess(self.command, runtime, home / 'stdout.log')
        self.last = 0.0
        self.text, self.read = '', 0

    def log(self):
        """The client's log so far, read from where the last read ended."""
        path = self.home / 'BaseEF' / 'qconsole.log'
        try:
            with open(path, 'rb') as handle:
                handle.seek(self.read)
                data = handle.read()
        except (OSError, MemoryError):
            return self.text
        self.read += len(data)
        self.text += data.decode('latin1', errors='replace')
        return self.text

    def alive(self):
        return self.process.poll() is None

    def rcon(self, line, timeout=2.0):
        # One rcon a second: the server's per-address bucket refuses more.
        time.sleep(max(0.0, self.last + 1.1 - time.monotonic()))
        self.last = time.monotonic()
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(timeout)
            sock.sendto(OOB + f'rcon {self.password} {line}'.encode('latin1'), ('127.0.0.1', self.port))
            parts = []
            try:
                while True:
                    parts.append(sock.recvfrom(65535)[0][4:].decode('latin1', errors='replace'))
                    sock.settimeout(0.3)
            except (socket.timeout, ConnectionResetError, OSError):
                pass
        return ''.join(parts)

    def cvar(self, name):
        found = re.search(r'"%s" is:"([^"]*?)(?:\^7)?"' % re.escape(name), self.rcon(name))
        return found.group(1) if found else None

    def run(self, line):
        """Runs a console line from a cfg: rcon redirects what a command it
        runs itself prints into its reply, a level's start among it, while a
        cfg's lines run from the command buffer and print to the log."""
        self.files = getattr(self, 'files', 0) + 1
        name = f'probestep{self.files}.cfg'
        (self.home / 'BaseEF' / name).write_text(line + '\n', encoding='latin1')
        return self.rcon(f'exec {name}')

    def stop(self):
        if self.alive():
            self.rcon('quit')
            end = time.monotonic() + 30
            while self.alive() and time.monotonic() < end:
                time.sleep(0.5)
        exit_code = self.process.poll()
        forced = False
        if exit_code is None:
            forced = True
            self.process.terminate()
            end = time.monotonic() + 30
            while self.alive() and time.monotonic() < end:
                time.sleep(0.5)
        self.process.close()
        return exit_code, forced


def wait_for(predicate, timeout, step=0.5):
    end = time.monotonic() + timeout
    while True:
        value = predicate()
        if value or time.monotonic() >= end:
            return value
        time.sleep(step)


def server_line(level):
    return re.compile(r'(?m)^Server: %s\r?$' % re.escape(level))


def level_start(text, level, before=0):
    found = server_line(level).search(text, before)
    return found.start() if found else -1


def started(client, level, before):
    """The level's server is up and the client has started its cgame on it."""
    text = client.log()
    at = level_start(text, level, before)
    return at >= 0 and 'CL_InitCGame' in text[at:]


def settle(client, least=6.0, timeout=240.0):
    """Waits until play runs: not paused (a movie pauses the level) for three
    queries in a row, and at least `least` seconds after the level began."""
    begin = time.monotonic()
    calm = 0
    paused_seen = False
    while time.monotonic() - begin < timeout:
        value = client.cvar('cl_paused')
        if value == '0':
            calm += 1
        else:
            paused_seen = paused_seen or value == '1'
            calm = 0
        if calm >= 3 and time.monotonic() - begin >= least:
            return True, paused_seen, round(time.monotonic() - begin, 1)
    return False, paused_seen, round(time.monotonic() - begin, 1)


def wait_save_allowed(client, timeout=180.0):
    """Waits until the game allows a save here, as a player walking into a
    holodeck always finds it: its exit saves `exitholodeck` first, and a save
    is refused during the game's camera scenes (SG_WriteSavegame asks
    SG_GameAllowedToSaveHere, stvoy.exe 0x427360). Asked with the game's own
    save command, whose test save is then wiped."""
    begin = time.monotonic()
    while time.monotonic() - begin < timeout:
        # The save's own messages come back in the rcon reply.
        if re.search(r'(?m)^(?:\^\d)?Done\.', client.rcon('save probe-allowed')):
            client.rcon('wipe probe-allowed')
            return round(time.monotonic() - begin, 1)
        time.sleep(2.0)
    return None


def record_level(client, level, step):
    shot = f'campaign-{step:02d}-{level.replace("/", "_")}'
    client.rcon(f'screenshot {shot}')
    carried = {name: client.cvar(name) for name in ('playersave', 'playerammo0', 'playerammo1',
                                                    'playerammo2', 'playerammo3', 'mapname')}
    shots = sorted(client.home.rglob(f'{shot}.*'))
    return dict(screenshot=[str(p.relative_to(client.home)) for p in shots],
                screenshotSha256=[sha(p) for p in shots], carried=carried,
                autosave=save_map(client.home / 'BaseEF' / 'saves' / 'auto.sav'))


def play(args, output):
    home = output / 'home'
    steps = chain_steps(args.chain)[args.start:]
    client = Client(args.runtime, home, args.renderer, steps[0][0], args.basepath, PRELUDES.get(args.chain, ()))
    levels = []
    error = None
    try:
        mark = 0
        for index, (level, exit_command, following, fallbacks) in enumerate(steps, start=args.start):
            entry = dict(step=index + 1, level=level)
            levels.append(entry)
            began = time.monotonic()
            if not wait_for(lambda: started(client, level, mark), args.load_seconds):
                entry.update(passed=False, failure='level did not start', alive=client.alive())
                break
            entry['loadSeconds'] = round(time.monotonic() - began, 1)
            level_mark = level_start(client.log(), level, mark)
            if exit_command is None and args.chain == 'campaign':
                # Its epilogue starts as the player spawns.
                entry.update(final(client, level_mark))
                break
            segment = client.log()[mark:level_mark + 1]
            restored = re.findall(r'(?m)^[^\n]*(?:Restoring game|Failed to open savegame|Loading game|Done\.)[^\n]*$', segment)
            entry['restored'] = [line.strip() for line in restored][-3:]
            ok, paused, seconds = settle(client)
            entry.update(settled=ok, pausedForMovie=paused, settleSeconds=seconds)
            if not ok:
                entry.update(passed=False, failure='level never ran unpaused')
                break
            entry.update(record_level(client, level, index + 1))
            if args.chain != 'campaign' and exit_command and not level.startswith('_'):
                entry['saveAllowedAfter'] = wait_save_allowed(client)
            if exit_command is None:
                entry.update(passed=True, errors=ERROR.findall(client.log()[level_mark:])[:5])
                print(f'== {entry["step"]:2d} {level}: back, autosave={entry["autosave"]}', flush=True)
                break
            used = []
            for command in [exit_command] + fallbacks:
                used.append(command)
                print(f'   {level}: {command}', flush=True)
                client.run(command)
                if wait_for(lambda: level_start(client.log(), following, level_mark + 1) >= 0, args.exit_seconds):
                    break
            text = client.log()
            # The game's transition command runs from the command buffer with
            # no echo, so the next level's start is the evidence it ran.
            entry.update(commands=used, passed=level_start(text, following, level_mark + 1) >= 0)
            segment = text[level_mark:]
            entry.update(errors=ERROR.findall(segment)[:5], warnings=len(WARNING.findall(segment)))
            print(f'== {entry["step"]:2d} {level}: passed={entry["passed"]} via={used[-1]!r} '
                  f'load={entry["loadSeconds"]}s settle={seconds}s autosave={entry["autosave"]}', flush=True)
            if not entry['passed']:
                entry['failure'] = 'no transition'
                break
            mark = level_mark + 1
    except Exception as failure:  # keep what was observed, then report it
        error = repr(failure)
    finally:
        exit_code, forced = client.stop()
    return dict(runtime=str(args.runtime), sha256=client.sha256, renderer=args.renderer, basepath=str(args.basepath),
                chain=args.chain, command=client.command, levels=levels, exit=exit_code, forced=forced,
                probeError=error)


def final(client, level_mark):
    """voy20's epilogue starts by itself: the closing movie, st_21, then the
    credits, which the game starts with their music (efgamex86.dll sends
    `music music/endcredits.mp3`). Once the credits are up, three captures
    20 seconds apart show them rolling; ten minutes is the limit."""
    begin = time.monotonic()
    shots, credits_at = [], None
    seen_pause = False
    while time.monotonic() - begin < 600 and client.alive():
        paused = client.cvar('cl_paused')
        seen_pause = seen_pause or paused == '1'
        text = client.log()[level_mark:]
        if credits_at is None and 'endcredits' in text and 'Bink finished: video/st_21' in text:
            credits_at = time.monotonic()
        if credits_at is not None:
            due = credits_at + 3 + 20 * len(shots)
            if time.monotonic() >= due:
                name = f'campaign-credits-{len(shots):02d}'
                client.rcon(f'screenshot {name}')
                shots.append(name)
                if len(shots) == 3:
                    time.sleep(1.5)
                    break
        time.sleep(1.0)
    text = client.log()[level_mark:]
    movies = re.findall(r'(?im)^[^\n]*(?:CIN_PlayCinematic|Bink[: ])[^\n]*$', text)
    return dict(finalSeconds=round(time.monotonic() - begin, 1), finalPaused=seen_pause,
                movieLines=[line.strip() for line in movies[:12]],
                movieFinished='Bink finished: video/st_21' in text,
                creditsStarted=credits_at is not None,
                creditsLines=[line.strip() for line in re.findall(r'(?im)^[^\n]*credits[^\n]*$', text)[:6]],
                finalScreenshots=[str(p.relative_to(client.home)) for n in shots
                                  for p in client.home.rglob(f'{n}.*')],
                passed=client.alive() and credits_at is not None and len(shots) == 3)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True, help='fnTech3 runtime folder')
    parser.add_argument('--renderer', default='glx', choices=['glx', 'vk', 'rtx'])
    parser.add_argument('--chain', default='campaign', choices=['campaign', *SIDE_TRIPS])
    parser.add_argument('--start', type=int, default=0, help='index of the first level in the chain')
    parser.add_argument('--basepath', type=Path, default=INSTALL,
                        help='the game folder: the installation, or a stock layout of its retail paks')
    parser.add_argument('--load-seconds', type=float, default=120)
    parser.add_argument('--exit-seconds', type=float, default=45)
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Windows only')
    args.runtime = args.runtime.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    args.basepath = args.basepath.resolve(strict=True)
    before = inventory(INSTALL)
    result = play(args, output)
    result['installationUnchanged'] = before == inventory(INSTALL)
    result['passed'] = (all(level.get('passed') for level in result['levels']) and bool(result['levels'])
                        and result['installationUnchanged'] and not result['probeError'])
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('passed', result['passed'], 'levels', len(result['levels']), 'exit', result['exit'])


if __name__ == '__main__':
    main()
