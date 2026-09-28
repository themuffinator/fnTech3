#!/usr/bin/env python3
"""Run an isolated, windowed Ritual-engine client probe on a private desktop.

The profiles are Heavy Metal: F.A.K.K.2 (`fakk2`, game folder `fakk`) and
American McGee's Alice (`alice`, game folder `base`). Use an executable
configured with -Dfakk2-development=true or -Dalice-development=true. No
platform input is injected and the user's desktop is never switched. The
probe's console steps are written into a cfg in the isolated home and executed
with +exec, so the startup command line stays short; captures use the engine's
own screenshotPNG command. The installation is only read, and the manifest
records whether it is unchanged. A client still running at --timeout is
stopped. The manifest is written however the client ends; if it could not be
stopped, `stopError` says why and the probe exits with status 1.

Under the Ritual profiles `wait N` counts milliseconds, as retail's does, so
steps wait in time rather than in frames. A screenshot is written at the end
of its frame: follow the last one with a wait, or `quit` runs first.

`--actions FILE` drives the menus and play with the engine's own synthetic
input instead (`in_synthesize`, enabled by `+set in_synthetic 1`, which this
option adds). One action a line:

  point X Y        put the menu cursor at video pixel X, Y
  click X Y [KEY]  point, then press and let go of KEY (default mouse1)
  press KEY [MS]   press KEY and let it go MS later (default 60)
  down KEY         press KEY and hold it
  up KEY           let KEY go
  type TEXT        type the characters of TEXT
  motion DX DY     relative mouse motion: the menu cursor, or the view in play
  shot NAME        screenshotPNG NAME
  anything else    a console line, `wait N` included

A key or button runs its binding, and a menu its command, by appending to
the command buffer, behind every step still waiting there. So each action
that sends a key or a button ends a phase: 100 ms later the next phase is
queued as a console line, the way typed console text arrives, and it runs
from its own cfg behind whatever that key or button appended.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

GAME_DIRS = {'fakk2': 'fakk', 'alice': 'base'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): [path.stat().st_size, path.stat().st_mtime_ns]
            for path in installation.rglob('*') if path.is_file()}


def compile_actions(script):
    """The phases of an --actions script, each a list of console lines."""
    phases = [[]]

    def chain():
        phases[-1] += ['wait 100', f'in_synthesize console exec ritual-input-{len(phases)}.cfg']
        phases.append([])

    for number, raw in enumerate(script.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(('#', '//')):
            continue
        word, _, rest = line.partition(' ')
        args = rest.split()
        phase = phases[-1]
        try:
            if word in ('point', 'click'):
                x, y = int(args[0]), int(args[1])
                # To the corner, then out: the event queue merges motion
                # queued back to back, so the second waits for the next frame.
                # A Ritual wait counts the frame it starts in, so a slow frame
                # used up a 20 ms wait and both went to the corner.
                phase += ['in_synthesize motion -100000 -100000', 'wait 200', f'in_synthesize motion {x} {y}']
                if word == 'click':
                    key = args[2] if len(args) > 2 else 'mouse1'
                    phase += ['wait 150', f'in_synthesize key {key} 1', 'wait 80', f'in_synthesize key {key} 0']
                    chain()
            elif word == 'press':
                hold = int(args[1]) if len(args) > 1 else 60
                phase += [f'in_synthesize key {args[0]} 1', f'wait {hold}', f'in_synthesize key {args[0]} 0']
                chain()
            elif word in ('down', 'up'):
                phase.append(f'in_synthesize key {args[0]} {1 if word == "down" else 0}')
                chain()
            elif word == 'type':
                phase.append(f'in_synthesize char {rest}')
                chain()
            elif word == 'motion':
                phase.append(f'in_synthesize motion {int(args[0])} {int(args[1])}')
            elif word == 'shot':
                phase.append(f'screenshotPNG {args[0]}')
            else:
                phase.append(line)
        except (IndexError, ValueError):
            raise SystemExit(f'actions line {number}: cannot read "{line}"')
    return phases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', choices=sorted(GAME_DIRS), default='fakk2')
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), default='glx')
    parser.add_argument('--map', help='devmap this level before the steps run')
    parser.add_argument('--steps', default='wait 8000\nscreenshotPNG menu\nwait 1500\n',
                        help='console steps, newline-separated; quit is appended')
    parser.add_argument('--steps-file', type=Path)
    parser.add_argument('--actions', type=Path, help='an input script run in place of the steps (see above)')
    parser.add_argument('--no-quit', action='store_true',
                        help='do not append quit: the steps end the run themselves, for example from '
                             'activeAction after a level change, since a game module appends its own '
                             'commands behind every step still waiting in the buffer')
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--width', type=int, default=800)
    parser.add_argument('--height', type=int, default=600)
    parser.add_argument('--sound', action='store_true', help='initialise sound (it stays muted unfocused)')
    parser.add_argument('--extra', action='append', default=[], help='extra +set name value pairs as name=value')
    parser.add_argument('--overlay', type=Path, help='a folder copied into the fresh home, such as test scripts')
    args = parser.parse_args()

    game = GAME_DIRS[args.profile]
    output = args.output.resolve()
    home = output / 'home'
    if home.exists():
        for path in sorted(home.rglob('*'), reverse=True):
            if path.is_file():
                path.unlink()
    (home / game).mkdir(parents=True, exist_ok=True)
    if args.overlay:
        shutil.copytree(args.overlay, home, dirs_exist_ok=True)

    if args.actions:
        phases = compile_actions(args.actions.read_text())
    else:
        text = args.steps_file.read_text() if args.steps_file else args.steps.replace('\\n', '\n')
        phases = [text.splitlines()]
    # The map starts first: +exec inserts the steps ahead of any later
    # command-line command, so their waits would hold a +devmap back.
    if args.map:
        phases[0].insert(0, f'devmap {args.map}')
    if not args.no_quit:
        phases[-1].append('quit')
    for index, phase in enumerate(phases):
        name = 'ritual-probe.cfg' if index == 0 else f'ritual-input-{index}.cfg'
        (home / game / name).write_text(''.join(line + '\n' for line in phase))
    steps = ''.join(line + '\n' for line in phases[0])

    before = inventory(args.installation)
    command = [str(args.executable.resolve()),
               '+set', 'com_gameProfile', args.profile,
               '+set', 'fs_basepath', str(args.installation.resolve()),
               '+set', 'fs_homepath', str(home),
               '+set', 'fs_game', '', '+set', 'fs_cdpath', '',
               '+set', 'r_fullscreen', '0', '+set', 'r_mode', '-1',
               '+set', 'r_customWidth', str(args.width), '+set', 'r_customHeight', str(args.height),
               '+set', 'in_mouse', '0', '+set', 'in_joystick', '0', '+set', 'in_nograb', '1',
               '+set', 's_initsound', '1' if args.sound else '0',
               '+set', 'com_steamIntegration', '0', '+set', 'fs_skipWorkshop', '1',
               '+set', 'cl_webuiEnable', '0', '+set', 'cl_renderer', args.renderer,
               '+set', 'logfile', '4', '+set', 'net_ip', '127.0.0.1', '+set', 'net_port', '0',
               # IPv4 on loopback alone: the default also listens for IPv6 on
               # every address, which draws a firewall prompt per exe path
               '+set', 'net_enabled', '1',
               '+set', 'developer', '1']
    if args.actions:
        command += ['+set', 'in_synthetic', '1']
    for pair in args.extra:
        name, _, value = pair.partition('=')
        command += ['+set', name, value]
    command += ['+exec', 'ritual-probe.cfg']

    stdout = output / 'stdout.log'
    started = time.monotonic()
    process = WindowsPrivateProcess(command, args.executable.resolve().parent, stdout)
    forced, code, stop_error = False, None, None
    try:
        while process.poll() is None and time.monotonic() - started < args.timeout:
            time.sleep(0.25)
        if process.poll() is None:
            # False when the client began to exit on its own at the deadline.
            forced = process.terminate()
        # A stopped client's teardown can take seconds on a loaded machine.
        code = process.wait(60)
    except (OSError, subprocess.TimeoutExpired) as error:
        stop_error = f'{type(error).__name__}: {error}'
    finally:
        try:
            process.close()
        except (OSError, subprocess.TimeoutExpired) as error:
            stop_error = stop_error or f'{type(error).__name__}: {error}'

    shots_dir = home / game / 'screenshots'
    shots = sorted(p.name for p in shots_dir.glob('*')) if shots_dir.is_dir() else []
    renderer = args.executable.resolve().parent / f'fnql_{args.renderer}_x86.dll'
    manifest = {
        'profile': args.profile,
        'command': command,
        'steps': steps,
        'phases': len(phases),
        'exitCode': code,
        'forcedStop': forced,
        'stopError': stop_error,
        'seconds': round(time.monotonic() - started, 1),
        'screenshots': shots,
        'exeSha256': sha(args.executable.resolve()),
        'rendererSha256': sha(renderer) if renderer.is_file() else None,
        'installationUnchanged': inventory(args.installation) == before,
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    print(json.dumps({k: manifest[k] for k in ('exitCode', 'forcedStop', 'stopError', 'seconds', 'screenshots',
                                                'installationUnchanged')}, indent=2))
    if stop_error:
        print(f'Stopping the client failed; if it still runs, find it by its home {home} and stop that PID.',
              file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
