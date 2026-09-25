#!/usr/bin/env python3
"""Show Enemy Territory scenes taking the engine's own lights beside the modules' 32.

Retail ET accepts 32 module lights a frame, counted across every scene the
frame renders, and keeps a forced module light (REF_FORCE_DLIGHT) whatever
r_dynamiclight says. fnTech3 adds its own lights to a scene as it is rendered:
static map lights (`maps/<map>.lights.json`), surface-light proxies and
`r_dlightTest`. Under `code/renderercommon/tr_rtcw_engine_lights.h` they take
only frame slots the modules can no longer claim, keep a scene within the 32
lights its surface masks can name, and are off at r_dynamiclight 0.

fnTech3 does not replay ET demos yet, so every run is a live listen server on
oasis, where an Allied engineer spawns on dry ground. Each step is a console
command from a config the probe writes into an isolated home, and each capture
is the engine's own `screenshotPNG`. Captures of a live scene differ a little
between frames, so each comparison has a companion that measures that motion
alone.

- `lights`: one client per renderer. An engineer spawns; three fixture
  lights in front of the player are switched on, off and on again, then the
  same at r_dynamiclight 0. GL and Vulkan also fill the scene with 32
  `r_dlightTest` lights before the fixture. The renderers' promotion counters
  (`r_staticLightDebug`) are printed between markers for every step.
- `dry`: the throw run below without `+attack`, to check the spawn and view.
- `throw`: fires once (`+attack`), which needs the user's permission for that
  run: pass `--allow-throw` only then. The charge's glow is a forced module
  light; it is captured at r_dynamiclight 1, 0 and 1, then again once it has
  faded (it lasts 15.5 s from the charge's first snapshot).

No keyboard or mouse input is sent: every window is on a private desktop,
windowed. The installation is only read; its file inventory is compared before
and after.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

WIDTH, HEIGHT = 800, 600
GAME, MAP = 'etmain', 'oasis'
MAX_STARTUP_COMMANDS = 32
ERRORS = re.compile(r'^.*(?:ERROR:|Sys_Error|fatal crashed|recursive error|Server crashed).*$', re.M | re.I)
VIEWPOS = re.compile(r'^\((-?\d+) (-?\d+) (-?\d+)\) : (-?\d+)\s*$', re.M)
# A cvar typed alone prints its value; ET's cgame takes over echo.
MARKER = re.compile(r'^"probe_step" is:"([^"^]+)\^7"', re.M)
STATIC = re.compile(r'^static lights file:(\S+) loaded:(\d+) parsefail:(\d+) count:(\d+) point:(\d+) spot:(\d+) '
                    r'spatial:\d+/\d+ promoted:(\d+) shadow:(\d+) skip disabled:(\d+) pvs:(\d+) budget:(\d+)', re.M)

# The engineer is class 2 with a Thompson (8) and a Colt (7); ET's team command
# sets the class itself, and dynamite leads weapon bank 6. A running match
# spawns a new player only with the next reinforcement wave, and the map script
# sets the wave time unless g_user*RespawnTime overrides it, so the waves are a
# second apart. openlimbomenu toggles the limbo panel off.
JOIN = 'wait 60; team b 2 8 7; wait 150; openlimbomenu; wait 30; '
LOOK = 'weaponbank 6; wait 60; +lookdown; wait 20; -lookdown; wait 30; '


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): (path.stat().st_size, path.stat().st_mtime_ns)
            for path in installation.rglob('*') if path.is_file()}


def run(command, cwd, stdout, timeout):
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, cwd, stdout)
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            time.sleep(0.1)
        forced = process.poll() is None
        code = process.poll()
    finally:
        try:
            process.close()
        except subprocess.TimeoutExpired:
            process.wait(60)
            process.close()
    return code, forced, round(time.monotonic() - begin, 3)


OFFLINE = False


def launch(executable, installation, home, renderer, cfg_values, timeout):
    game = home / GAME
    game.mkdir(parents=True, exist_ok=True)
    (game / 'probe.cfg').write_text(''.join(f'set {key} "{value}"\n' for key, value in cfg_values.items()),
                                    encoding='utf-8')
    startup = dict(com_gameProfile='wolfet', fs_basepath=str(installation), fs_homepath=str(home),
                   fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth=str(WIDTH),
                   r_customHeight=str(HEIGHT), in_mouse='0', in_joystick='0', in_nograb='1',
                   s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                   cl_renderer=renderer, logfile='2', net_ip='127.0.0.1', net_port='0', net_ip6='::1',
                   net_port6='0', rtx_rt_mode='0', rtx_rt_require='0', com_maxfps='60',
                   com_recommendedSet='1', com_hunkMegs='256', cl_motd='0', cl_autoupdate='0')
    if OFFLINE:
        # a listen server needs no socket, and a new executable path then raises no firewall prompt
        startup['net_enabled'] = '0'
    command = [str(executable)]
    for key, value in startup.items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg', '+devmap', MAP]
    assert sum(argument.startswith('+') for argument in command) <= MAX_STARTUP_COMMANDS, command
    renderer_dll = executable.parent / f'fnql_{renderer}_x86.dll'
    before = dict(executableSha256=sha(executable), rendererSha256=sha(renderer_dll))
    code, forced, elapsed = run(command, executable.parent, home.parent / 'stdout.log', timeout)
    log = game / 'qconsole.log'
    text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
    (home.parent / 'console.log').write_text(text, encoding='utf-8')
    after = dict(executableSha256=sha(executable), rendererSha256=sha(renderer_dll))
    return dict(command=command, exitCode=code, forcedStop=forced, elapsedSeconds=elapsed,
                errors=ERRORS.findall(text), executable=str(executable), **before,
                binariesUnchanged=before == after), text


def fixture(view):
    """Three lights in front of a view: red and blue points, a green spot."""
    x, y, z, yaw = view
    angle = math.radians(yaw)
    forward = (math.cos(angle), math.sin(angle), 0.0)
    right = (math.sin(angle), -math.cos(angle), 0.0)

    def at(ahead, side, up):
        return [round(x + forward[0] * ahead + right[0] * side, 1),
                round(y + forward[1] * ahead + right[1] * side, 1), round(z + up, 1)]

    down = (forward[0] * 0.6, forward[1] * 0.6, -0.8)
    lights = [
        dict(name='probe-red', type='point', origin=at(140, -70, -24), color=[1, 0.15, 0.1],
             intensity=260, radius=260, castsShadows=False),
        dict(name='probe-blue', type='point', origin=at(200, 90, -8), color=[0.15, 0.3, 1],
             intensity=240, radius=240, castsShadows=True),
        dict(name='probe-green-spot', type='spot', origin=at(90, 0, 60), direction=[round(v, 4) for v in down],
             color=[0.2, 1, 0.25], intensity=420, radius=420, innerAngle=20, outerAngle=40, castsShadows=False),
    ]
    return dict(version=1, generator='fnTech3 scripts/probe_wolfet_engine_lights.py (test fixture)',
                map=MAP, lights=lights)


def steps_counters(text):
    """The static-light counter lines printed after each step's marker."""
    marks = [(match.start(), match[1]) for match in MARKER.finditer(text)]
    out = {}
    for index, (start, name) in enumerate(marks):
        end = marks[index + 1][0] if index + 1 < len(marks) else len(text)
        rows = STATIC.findall(text, start, end)
        histogram = {}
        for row in rows:
            key = f'promoted:{row[6]} skipDisabled:{row[8]} skipPVS:{row[9]} skipBudget:{row[10]}'
            histogram[key] = histogram.get(key, 0) + 1
        out[name] = histogram
    return out


def capture(name, settle=20):
    # A capture is written at the end of the next frame, so a setting changed
    # after it waits two frames.
    return f'set probe_step {name}; probe_step; wait {settle}; screenshotPNG {name}; wait 2; '


def keep_captures(home, row, names):
    kept = {}
    for name in names:
        found = next(home.rglob(f'{name}.png'), None)
        if found is not None:
            target = row / f'{name}.png'
            shutil.copy2(found, target)
            kept[name] = dict(path=str(target), sha256=sha(target))
    return kept


def view_run(executable, installation, output):
    """Where the engineer stands, for the fixture lights."""
    row = output / 'view'
    home = row / 'home'
    values = base_values(JOIN + 'viewpos; wait 5; quit')
    result, text = launch(executable, installation, home, 'glx', values, 180)
    result['views'] = [list(map(int, found)) for found in VIEWPOS.findall(text)]
    shutil.rmtree(home)
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and not result['errors']
                        and bool(result['views']))
    return result


def base_values(action, **extra):
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  com_introplayed='1', com_maxfpsUnfocused='60', com_maxfpsMinimized='60', sv_pure='0',
                  g_userAlliedRespawnTime='1', g_userAxisRespawnTime='1', con_notifytime='-1', cg_lagometer='0',
                  **{f'sv_master{number}': '' for number in range(1, 6)})
    values.update(extra)
    values['activeAction'] = action
    return values


LIGHT_STEPS = ('on', 'off', 'again', 'nodyn-on', 'nodyn-off')
TEST_STEPS = ('fill-on', 'fill-off')


def lights_run(executable, installation, output, renderer, lights):
    row = output / f'{renderer}-lights'
    home = row / 'home'
    maps = home / GAME / 'maps'
    maps.mkdir(parents=True)
    (maps / f'{MAP}.lights.json').write_text(json.dumps(lights, indent=1) + '\n', encoding='utf-8')
    action = (JOIN + 'cg_draw2D 0; cg_drawGun 0; r_staticLightDebug 1; viewpos; ' +
              capture('on') + 'r_staticLights 0; ' + capture('off') + 'r_staticLights 1; ' + capture('again') +
              'r_dynamiclight 0; ' + capture('nodyn-on') + 'r_staticLights 0; ' + capture('nodyn-off') +
              'r_dynamiclight 1; r_staticLights 1; ')
    names = list(LIGHT_STEPS)
    if renderer != 'rtx':
        # 32 test lights come first in every world scene; the fixture follows.
        action += 'r_staticLightMaxLights 32; r_dlightTest 32 320 160 24 0; ' + capture('fill-on')
        action += 'r_staticLights 0; ' + capture('fill-off')
        names += TEST_STEPS
    action += 'wait 10; quit'
    result, text = launch(executable, installation, home, renderer,
                          base_values(action, r_staticLights='1'), 240)
    result.update(renderer=renderer, views=[list(map(int, found)) for found in VIEWPOS.findall(text)],
                  counters=steps_counters(text), screenshots=keep_captures(home, row, names))
    shutil.rmtree(home)
    shots = result['screenshots']
    result['comparisons'] = {}
    for mine, reference in (('off', 'on'), ('again', 'on'), ('nodyn-off', 'nodyn-on'), ('nodyn-on', 'again'),
                            ('fill-off', 'fill-on')):
        if mine in shots and reference in shots:
            result['comparisons'][f'{mine}-vs-{reference}'] = compare(shots[reference]['path'], shots[mine]['path'])
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and not result['errors']
                        and len(shots) == len(names) and result['binariesUnchanged'])
    return result


GLOW_STEPS = ('glow', 'glow-nodyn', 'glow-again', 'faded', 'faded-nodyn')


def throw_run(executable, installation, output, renderer, fire):
    row = output / f'{renderer}-{"throw" if fire else "dry"}'
    home = row / 'home'
    press = '+attack; wait 30; -attack; ' if fire else 'wait 30; '
    # The glow fades out 15.5 s after the charge's first snapshot; at 60
    # frames a second the faded captures come about 20 s after the throw.
    action = (JOIN + 'cg_drawGun 0; ' + capture('ready') + 'cg_draw2D 0; ' + LOOK + press + 'viewpos; ' +
              capture('glow', 40) + 'r_dynamiclight 0; ' + capture('glow-nodyn') + 'r_dynamiclight 1; ' +
              capture('glow-again') + 'wait 1200; ' + capture('faded') + 'r_dynamiclight 0; ' +
              capture('faded-nodyn') + 'r_dynamiclight 1; wait 10; quit')
    names = ('ready',) + GLOW_STEPS
    result, text = launch(executable, installation, home, renderer, base_values(action), 240)
    result.update(renderer=renderer, fired=fire, script=action,
                  views=[list(map(int, found)) for found in VIEWPOS.findall(text)],
                  screenshots=keep_captures(home, row, names))
    shutil.rmtree(home)
    shots = result['screenshots']
    result['comparisons'] = {}
    for mine, reference in (('glow', 'faded'), ('glow-nodyn', 'faded-nodyn'), ('glow-nodyn', 'glow'),
                            ('glow-again', 'glow'), ('faded-nodyn', 'faded')):
        if mine in shots and reference in shots:
            result['comparisons'][f'{mine}-vs-{reference}'] = compare(shots[reference]['path'], shots[mine]['path'])
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and not result['errors']
                        and len(shots) == len(names) and result['binariesUnchanged'])
    return result


def compare(a, b):
    import numpy
    from PIL import Image
    x = numpy.asarray(Image.open(a).convert('RGB'), dtype=numpy.float64)
    y = numpy.asarray(Image.open(b).convert('RGB'), dtype=numpy.float64)
    diff = numpy.abs(x - y)
    changed = diff.sum(axis=2) >= 6
    out = dict(identical=bool(numpy.array_equal(x, y)), meanAbsDiff=round(float(diff.mean()), 4),
               changedPixels=int(changed.sum()))
    if changed.any():
        out['meanDeltaInChanged'] = [round(float(v), 2) for v in (y - x)[changed].mean(axis=0)]
        rows, columns = numpy.nonzero(changed)
        out['changedBox'] = [int(columns.min()), int(rows.min()), int(columns.max()), int(rows.max())]
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('lights', 'dry', 'throw'), required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--executable', type=Path, default=ROOT / 'meson/build/win32-dev/fntech3.exe')
    parser.add_argument('--renderer', action='append', choices=('glx', 'vk', 'rtx'))
    parser.add_argument('--offline', action='store_true', help='run with net_enabled 0')
    parser.add_argument('--allow-throw', action='store_true',
                        help="fire once in --mode throw; only with the user's permission for that run")
    args = parser.parse_args()
    if args.mode == 'throw' and not args.allow_throw:
        parser.error('--mode throw fires once and needs --allow-throw')
    global OFFLINE
    OFFLINE = args.offline
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    installation = args.installation.resolve()
    executable = args.executable.resolve()
    before = inventory(installation)
    report = dict(scope=__doc__, dateUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  profile='wolfet', mode=args.mode, installation=str(installation), map=MAP, width=WIDTH,
                  height=HEIGHT, executable=str(executable),
                  runtime={name: sha(executable.parent / name) for name in
                           ('fntech3.exe', 'fnql_glx_x86.dll', 'fnql_vk_x86.dll', 'fnql_rtx_x86.dll',
                            'fntech3-pkg.fnz', 'fnql-web.pak') if (executable.parent / name).is_file()})
    runs = []
    if args.mode == 'lights':
        report['view'] = view = view_run(executable, installation, output)
        if view['passed']:
            report['lights'] = lights = fixture(view['views'][-1])
            (output / f'{MAP}.lights.json').write_text(json.dumps(lights, indent=1) + '\n', encoding='utf-8')
            for renderer in args.renderer or ('glx', 'vk', 'rtx'):
                result = lights_run(executable, installation, output, renderer, lights)
                runs.append(result)
                print(renderer, 'lights', 'PASS' if result['passed'] else 'FAIL', flush=True)
                for name, row in result['comparisons'].items():
                    print(' ', name, 'identical' if row['identical'] else
                          f"changed:{row['changedPixels']} mean:{row['meanAbsDiff']} "
                          f"delta:{row.get('meanDeltaInChanged')}", flush=True)
                for name, histogram in result['counters'].items():
                    print(' ', name, json.dumps(histogram), flush=True)
    else:
        for renderer in args.renderer or ('glx',):
            result = throw_run(executable, installation, output, renderer, args.mode == 'throw')
            runs.append(result)
            print(renderer, args.mode, 'PASS' if result['passed'] else 'FAIL', result['views'], flush=True)
            for name, row in result['comparisons'].items():
                print(' ', name, 'identical' if row['identical'] else
                      f"changed:{row['changedPixels']} mean:{row['meanAbsDiff']} "
                      f"delta:{row.get('meanDeltaInChanged')} box:{row.get('changedBox')}", flush=True)
    report['runs'] = runs
    report['installationUnchanged'] = inventory(installation) == before
    report['passed'] = (bool(runs) and all(run['passed'] for run in runs) and report['installationUnchanged'])
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
