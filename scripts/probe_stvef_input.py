#!/usr/bin/env python3
"""Drive Elite Force single player and Holomatch with the engine's own
synthetic input, and check what the game did with it.

The client starts with `+set in_synthetic 1`, which lets `in_synthesize` queue
key, character and relative mouse-motion events in the engine's event queue,
where the platform layer puts real input; nothing reaches the operating
system's input and the window, on an unswitched private desktop, never has
focus. Each check is driven over loopback rcon:

- movement: the key bound to `+forward` is held, and the cgame's `viewpos`
  must report the player moved;
- look: relative motion turns the view, and `viewpos` reports the new yaw;
- fire: the key bound to `+attack` is held. Single player's carried ammunition
  (`playerammo*`, which `maptransition` writes from the live player) must
  drop; Holomatch's HUD is captured before and after;
- a key bound through the console takes effect when pressed;
- skip (`--skip`, single player): the use-item button skips the level's opening
  scene, and the time until the game clears `skippingCinematic` is measured.

Menus are driven with `menu` actions in a scenario file: `point X Y`, `click X
Y [KEY]`, `press KEY [SECONDS]`, `type TEXT`, `shot NAME`, `wait SECONDS` and
console lines. Engine screenshots record each screen.

Runs are windowed, with an isolated home and no sound; the installation is
read only. The user approved synthetic input for this task's Elite Force
probe windows on private desktops (2026-10-08).
"""
from pathlib import Path
import argparse
import json
import re
import secrets
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'scripts'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402
from probe_stvef_campaign import INSTALL, OOB, free_port, inventory, sha, wait_for  # noqa: E402

CONFIG = {'stvef-sp': 'efconfig.cfg', 'stvef-mp': 'hmconfig.cfg'}


class Client:
    """A windowed client with rcon on loopback and synthetic input enabled."""

    def __init__(self, runtime, home, profile, first, renderer='glx', extra=()):
        self.home, self.port, self.password = home, free_port(), secrets.token_hex(6)
        self.game = home / 'BaseEF'
        self.game.mkdir(parents=True)
        for name in CONFIG.values():
            (self.game / name).write_text('// empty\n', encoding='latin1')
        lines = [f'set rconpassword "{self.password}"', 'developer 1'] + list(extra)
        if first:
            lines.append(f'devmap {first}')
        (self.game / 'probe.cfg').write_text('\n'.join(lines) + '\n', encoding='latin1')
        self.command = [str(runtime / 'fntech3.exe'), '+set', 'com_gameProfile', profile,
                        '+set', 'fs_basepath', str(INSTALL), '+set', 'fs_homepath', str(home),
                        '+set', 'cl_renderer', renderer, '+set', 'r_fullscreen', '0', '+set', 'r_mode', '3',
                        '+set', 'in_mouse', '0', '+set', 'in_joystick', '0', '+set', 'in_nograb', '1',
                        '+set', 'in_synthetic', '1',
                        '+set', 's_initsound', '0', '+set', 'net_enabled', '1', '+set', 'net_ip', '127.0.0.1',
                        '+set', 'net_port', str(self.port), '+set', 'logfile', '2',
                        '+set', 'com_hunkMegs', '256', '+set', 'com_zoneMegs', '128',
                        '+set', 'com_introplayed', '1', '+set', 'com_maxfps', '60',
                        '+set', 'com_maxfpsUnfocused', '60', '+set', 'com_maxfpsMinimized', '60',
                        '+set', 'use_ff', '0', '+set', 'cl_motd', '0', '+exec', 'probe.cfg']
        self.sha256 = sha(runtime / 'fntech3.exe')
        self.process = WindowsPrivateProcess(self.command, runtime, home / 'stdout.log')
        self.last = 0.0
        self.text, self.read, self.files = '', 0, 0

    def log(self):
        try:
            with open(self.game / 'qconsole.log', 'rb') as handle:
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

    def run(self, lines):
        """Runs console lines from a cfg, in order, with frame waits."""
        self.files += 1
        name = f'probestep{self.files}.cfg'
        (self.game / name).write_text('\n'.join(lines) + '\n', encoding='latin1')
        return self.rcon(f'exec {name}')

    def cvar(self, name):
        found = re.search(r'"%s" is:"([^"]*?)(?:\^7)?"' % re.escape(name), self.rcon(name))
        return found.group(1) if found else None

    def shot(self, name):
        self.rcon(f'screenshot {name}')
        time.sleep(1.2)
        return [str(p.relative_to(self.home)) for p in sorted(self.home.rglob(f'{name}.*'))]

    def viewpos(self):
        """The cgame's `viewpos`: the view origin and yaw. Its print comes
        back in the rcon reply, which redirects the console while it runs."""
        reply = self.rcon('viewpos')
        found = re.search(r'\((-?[\d.]+) (-?[\d.]+) (-?[\d.]+)\) : (-?[\d.]+)', reply)
        if not found:
            self.viewposReply = reply[:200]
        return [float(found.group(i)) for i in range(1, 5)] if found else None

    def key(self, name, seconds):
        self.run([f'in_synthesize key {name} 1'])
        time.sleep(seconds)
        self.run([f'in_synthesize key {name} 0'])

    def motion(self, dx, dy):
        self.run([f'in_synthesize motion {dx} {dy}'])

    def stop(self):
        if self.alive():
            self.rcon('quit')
            wait_for(lambda: not self.alive(), 30, 0.5)
        exit_code = self.process.poll()
        forced = exit_code is None
        if forced:
            self.process.terminate()
            wait_for(lambda: not self.alive(), 30, 0.5)
        self.process.close()
        return exit_code, forced


def binding(client, key):
    found = re.search(r'"%s" = "([^"]*)"' % re.escape(key), client.rcon(f'bind {key}'))
    return found.group(1) if found else None


def in_game(client, level):
    return wait_for(lambda: re.search(r'(?m)^Server: %s\r?$' % re.escape(level), client.log())
                    and 'CL_InitCGame' in client.log(), 120)


def settle(client, seconds=6.0):
    """Until play runs unpaused for three queries, at least `seconds` in."""
    begin, calm = time.monotonic(), 0
    while time.monotonic() - begin < 240:
        calm = calm + 1 if client.cvar('cl_paused') == '0' else 0
        if calm >= 3 and time.monotonic() - begin >= seconds:
            return True
    return False


def play_checks(client, profile, level, forward, attack):
    """Movement, look, fire and a console binding, through synthetic keys."""
    result = dict(bindings={k: binding(client, k) for k in (forward, attack, 'mouse1', 'w', 'uparrow')})
    start = client.viewpos()
    client.key(forward, 1.5)
    time.sleep(0.5)
    moved = client.viewpos()
    result['movement'] = dict(key=forward, before=start, after=moved,
                              distance=None if not (start and moved) else round(
                                  sum((a - b) ** 2 for a, b in zip(start[:3], moved[:3])) ** 0.5, 1))
    client.motion(400, 0)
    time.sleep(0.8)
    turned = client.viewpos()
    result['look'] = dict(motion=[400, 0], before=moved, after=turned,
                          yawChange=None if not (moved and turned) else round(
                              ((turned[3] - moved[3] + 180) % 360) - 180, 1))
    # A key bound from the console runs its binding when synthesized.
    client.run(['bind k "echo probe-bound-key-ran"'])
    mark = len(client.log())
    client.key('k', 0.3)
    result['consoleBinding'] = dict(key='k', ran=bool(wait_for(lambda: 'probe-bound-key-ran' in
                                                               client.log()[mark:], 5, 0.2)))
    result['fireShotsBefore'] = client.shot(f'{profile}-fire-before')
    if profile == 'stvef-sp':
        result['fire'] = sp_fire(client, level, attack)
    else:
        client.key(attack, 1.5)
        time.sleep(0.5)
        result['fire'] = dict(key=attack, held=1.5)
    result['fireShotsAfter'] = client.shot(f'{profile}-fire-after')
    return result


def carried(client):
    return {name: client.cvar(name) for name in ('playersave', 'playerammo0', 'playerammo1', 'playerammo2',
                                                  'playerammo3')}


def sp_fire(client, level, attack, weapon=2):
    """Single player: the ammunition maptransition carries drops after firing.
    The engine writes `playersave` and `playerammo*` from the live player as
    it leaves a level, so the level is left for itself before and after the
    key is held. borg1 and borg2 start the player with the compression rifle,
    weapon 2, whose ammunition is finite (the phaser's recharges)."""
    def transition():
        mark = len(client.log())
        client.run([f'maptransition {level}'])
        wait_for(lambda: re.search(r'(?m)^Server: %s\r?$' % re.escape(level), client.log()[mark:])
                 and 'CL_InitCGame' in client.log()[mark:], 120)
        settle(client)
        return carried(client)

    client.run([f'weapon {weapon}'])
    time.sleep(1.5)
    before = transition()
    client.run([f'weapon {weapon}'])
    time.sleep(1.5)
    client.key(attack, 2.0)
    time.sleep(0.5)
    after = transition()
    spent = {}
    for name in ('playerammo0', 'playerammo1', 'playerammo2', 'playerammo3'):
        try:
            spent[name] = int(before[name].split()[0]) - int(after[name].split()[0])
        except (AttributeError, ValueError, IndexError, TypeError):
            spent[name] = None
    return dict(key=attack, held=2.0, weapon=weapon, before=before, after=after, spent=spent)


def skip_scene(client, key='u', delay=4.0, limit=180):
    """Single player: the use-item button (`+button2`) skips a scripted
    scene. The game sets `skippingCinematic` 1 and `timescale` 100 when it is
    newly pressed in a scene, and clears both when the scene ends
    (efgamex86.dll 0x2005627a, button bit 4 at 0x20056ae4), so the engine's
    cap on a frame's game time sets how long the skip takes."""
    client.run([f'bind {key} +button2'])
    # A level may open with a movie, which any key ends; the scene follows it.
    result = dict(movie=bool(wait_for(lambda: 'finished cinematic' in client.log(), 30, 0.5)))
    time.sleep(delay)
    result.update(key=key, before=dict(skipping=client.cvar('skippingCinematic'), timescale=client.cvar('timescale')))
    client.key(key, 0.5)
    pressed = time.monotonic()
    # Nothing but the cvar is asked while the skip runs: each rcon query
    # waits out the server's query limit, which sets the resolution.
    samples = []
    while time.monotonic() - pressed < limit:
        value = client.cvar('skippingCinematic')
        samples.append([round(time.monotonic() - pressed, 1), value])
        if value == '0' and any(seen == '1' for _, seen in samples):
            break
    skipping = [at for at, value in samples if value == '1']
    result['samples'] = samples
    result['skipping'] = dict(started=bool(skipping), firstSeen=skipping[0] if skipping else None,
                              lastSeen=skipping[-1] if skipping else None,
                              clearedBy=samples[-1][0] if skipping and samples[-1][1] == '0' else None)
    result['timescaleAfter'] = client.cvar('timescale')
    result['shotAfter'] = client.shot('skip-after')
    return result


def menu(client, scenario):
    """Runs a menu scenario; returns what each step left behind."""
    steps = []
    for raw in scenario:
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        word, _, rest = line.partition(' ')
        args = rest.split()
        entry = dict(action=line)
        if word in ('point', 'click'):
            client.run(['in_synthesize motion -100000 -100000'])
            time.sleep(0.4)
            client.run([f'in_synthesize motion {int(args[0])} {int(args[1])}'])
            time.sleep(0.4)
            if word == 'click':
                key = args[2] if len(args) > 2 else 'mouse1'
                client.run([f'in_synthesize key {key} 1', 'wait 3', f'in_synthesize key {key} 0'])
                time.sleep(0.8)
        elif word == 'press':
            client.key(args[0], float(args[1]) if len(args) > 1 else 0.15)
            time.sleep(0.5)
        elif word == 'type':
            client.run([f'in_synthesize char {rest}'])
            time.sleep(0.5)
        elif word == 'shot':
            entry['screenshot'] = client.shot(args[0])
        elif word == 'wait':
            time.sleep(float(args[0]))
        else:
            mark = len(client.log())
            entry['reply'] = client.rcon(line)[:400]
            time.sleep(0.5)
            entry['logged'] = client.log()[mark:][-400:]
        steps.append(entry)
    return steps


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--profile', choices=['stvef-sp', 'stvef-mp'], required=True)
    parser.add_argument('--level', default=None)
    parser.add_argument('--play', action='store_true', help='movement, look, fire and binding checks')
    parser.add_argument('--menu', type=Path, help='a menu scenario file')
    parser.add_argument('--skip', action='store_true',
                        help="single player: skip the level's opening scene with the use-item button, at 5 frames a "
                             "second")
    parser.add_argument('--forward', default='w')
    parser.add_argument('--attack', default='mouse1')
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Windows only')
    level = args.level or ('tutorial' if args.profile == 'stvef-sp' else 'hm_borg1')
    runtime = args.runtime.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    before = inventory(INSTALL)
    # A skip is timed at a fixed 5 frames a second, so each frame's game
    # time is the engine's cap rather than the machine's frame rate.
    extra = ['com_maxfps 5', 'com_maxfpsUnfocused 5', 'com_maxfpsMinimized 5'] if args.skip else []
    client = Client(runtime, output / 'home', args.profile, level, extra=extra)
    result = dict(profile=args.profile, level=level, runtime=str(runtime), sha256=client.sha256,
                  command=client.command)
    try:
        if not in_game(client, level):
            raise RuntimeError('level did not start')
        if args.skip:
            result['skip'] = skip_scene(client)
        settle(client)
        if args.play:
            result['play'] = play_checks(client, args.profile, level, args.forward, args.attack)
        if args.menu:
            result['menu'] = menu(client, args.menu.read_text(encoding='utf-8').splitlines())
    except Exception as failure:  # keep what was observed
        result['probeError'] = repr(failure)
    finally:
        result['exit'], result['forced'] = client.stop()
    result['installationUnchanged'] = before == inventory(INSTALL)
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('command',)}, indent=1)[:6000])


if __name__ == '__main__':
    main()
