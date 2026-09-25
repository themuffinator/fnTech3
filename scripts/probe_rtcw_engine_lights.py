#!/usr/bin/env python3
"""Show RTCW scenes taking the engine's own lights, in each raster renderer.

RTCW SP and MP take the engine's static map lights (`maps/<map>.lights.json`),
surface-light proxies and `r_dlightTest` lights as every other profile does,
within the rules of `code/renderercommon/tr_rtcw_engine_lights.h`: only in the
per-pixel dynamic-light modes, only where an unforced RTCW light is drawn, and
never in a frame slot the RTCW modules may still claim.

The probe records one short demo on a listen server (RTCW MP `mp_ice`, or
Quake III `q3dm17` for the other profiles' non-regression) and reads the
player's view with `viewpos`. It writes a fixture light file beside that view
into every replay's isolated home -- three lights, one of them a spot -- then
replays the demo with `timedemo 1`, one snapshot a rendered frame, under each
renderer and setting. Captures at the same frame therefore show the same
scene. Every image is the engine's own `screenshotPNG`, and each run's console
carries the renderers' own promotion counters (`r_staticLightDebug`,
`r_surfaceLightProxyDebug`). `--reuse` replays another run's demo and fixture,
and `--against` compares every capture with that run's, so two runtimes -- a
build without a change and one with it -- can be set side by side.

Every action is a console command from a config the probe writes into the
isolated home: a private desktop, windowed, no keyboard or mouse input. The
installation is read-only; its file inventory is compared before and after.
A light file the engine package ships for the probe's map outranks the
fixture, and a run that did not load the fixture fails.
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
from windows_private_process import WindowsPrivateProcess

WIDTH, HEIGHT = 800, 600
DEMO = 'enginelights'
FRAMES = (40, 80)
# fnTech3 refuses more startup commands than this with a dialog, which on a
# private desktop waits for ever; settings go in the exec'd config instead.
MAX_STARTUP_COMMANDS = 32
ERRORS = re.compile(r'^.*(?:ERROR:|Sys_Error|fatal crashed|recursive error|Server crashed).*$', re.M | re.I)
VIEWPOS = re.compile(r'^\((-?\d+) (-?\d+) (-?\d+)\) : (-?\d+)\s*$', re.M)
STATIC = re.compile(r'^static lights file:(\S+) loaded:(\d+) parsefail:(\d+) count:(\d+) point:(\d+) spot:(\d+) '
                    r'spatial:\d+/\d+ promoted:(\d+) shadow:(\d+) skip disabled:(\d+) pvs:(\d+) budget:(\d+)', re.M)
PROXIES = re.compile(r'^surfacelight proxies sources:(\d+) built:(\d+) .*? promoted:(\d+) shadow:(\d+) .*?'
                     r'disabled:(\d+) pvs:(\d+) budget:(\d+)', re.M)

# How each profile reaches a player standing in a lit room. RTCW MP opens its
# limbo menu on joining; the cgame's CloseLimboMenu shuts it.
PROFILES = {
    'rtcw-mp': dict(game='main', map='mp_ice', join='wait 60; team b 0 14 0 17 0; wait 90; CloseLimboMenu; wait 60; ',
                    startup=dict(cl_autoupdate='0', cl_motd='0'), renderers=('glx', 'vk', 'rtx')),
    'q3': dict(game='baseq3', map='q3dm17', join='wait 90; ', startup=dict(cl_motd='0'),
               renderers=('glx', 'vk', 'rtx')),
    # RTCW SP records a demo only with g_synchronousClients 1, so its client is
    # captured live instead, with the lights on, off and on again in one run.
    # devmap loads escape1 without its briefing.
    'rtcw-sp': dict(game='main', map='escape1', join='wait 200; ', startup=dict(cl_motd='0'),
                    renderers=('glx', 'vk', 'rtx'), live=True),
}

# name: (settings, commands before the first capture). r_dlightTest is not
# registered by the RTX renderer.
CASES = {
    'off': (dict(r_staticLights='0', r_surfaceLightProxies='0'), ''),
    'static': (dict(r_staticLights='1', r_surfaceLightProxies='0'), ''),
    'proxies': (dict(r_staticLights='0', r_surfaceLightProxies='1'), ''),
    'test': (dict(r_staticLights='0', r_surfaceLightProxies='0'), 'r_dlightTest 4 320 160 24 0; '),
    'mode0-off': (dict(r_dlightMode='0', r_staticLights='0', r_surfaceLightProxies='0'), ''),
    'mode0-static': (dict(r_dlightMode='0', r_staticLights='1', r_surfaceLightProxies='1'), ''),
    'nodynamic-off': (dict(r_dynamiclight='0', r_staticLights='0', r_surfaceLightProxies='0'), ''),
    'nodynamic-static': (dict(r_dynamiclight='0', r_staticLights='1', r_surfaceLightProxies='1'), ''),
}
# Pairs whose frames must match in RTCW: the engine's lights are not added there.
IDENTICAL = (('mode0-off', 'mode0-static'), ('nodynamic-off', 'nodynamic-static'))


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


def launch(executable, installation, profile, home, renderer, cfg_values, tail, timeout, offline=False):
    spec = PROFILES[profile]
    game = home / spec['game']
    game.mkdir(parents=True, exist_ok=True)
    (game / 'probe.cfg').write_text(''.join(f'set {key} "{value}"\n' for key, value in cfg_values.items()),
                                    encoding='utf-8')
    startup = dict(com_gameProfile=profile, fs_basepath=str(installation), fs_homepath=str(home),
                   fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth=str(WIDTH),
                   r_customHeight=str(HEIGHT), in_mouse='0', in_joystick='0', in_nograb='1',
                   s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                   cl_renderer=renderer, logfile='2', net_ip='127.0.0.1', net_port='0', net_ip6='::1',
                   net_port6='0', rtx_rt_mode='0', rtx_rt_require='0', com_maxfps='60',
                   com_recommendedSet='1', **spec['startup'])
    if offline:
        # a replay needs no socket, and a new executable path then raises no firewall prompt
        startup['net_enabled'] = '0'
    command = [str(executable)]
    for key, value in startup.items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg'] + tail
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


def record(executable, installation, profile, output):
    spec = PROFILES[profile]
    row = output / 'record'
    home = row / 'home'
    action = spec['join'] + 'viewpos; wait 30; record ' + DEMO + '; wait 300; viewpos; stoprecord; wait 30; quit'
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  com_introplayed='1', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
                  activeAction=action)
    result, text = launch(executable, installation, profile, home, 'glx', values, ['+devmap', spec['map']], 180)
    views = VIEWPOS.findall(text)
    result['views'] = [list(map(int, view)) for view in views]
    demos = sorted((home / spec['game'] / 'demos').glob(DEMO + '.*'))
    result['demo'] = None
    if demos:
        kept = output / demos[0].name
        shutil.copy2(demos[0], kept)
        result['demo'] = dict(path=str(kept), sha256=sha(kept), bytes=kept.stat().st_size)
    shutil.rmtree(home)
    # The fixture lights sit in front of the last view; a player still moving
    # when the demo starts would be recorded here.
    result['stationary'] = len(views) >= 2 and views[-1] == views[-2]
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and bool(demos)
                        and not result['errors'] and bool(views))
    return result


def light_file(view, name):
    """Three fixture lights in front of a view: red and blue points, a green spot."""
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
    return dict(version=1, generator='fnTech3 scripts/probe_rtcw_engine_lights.py (test fixture)',
                map=name, lights=lights)


def counters(text):
    static = [dict(zip(('file', 'loaded', 'parseFailed', 'count', 'point', 'spot', 'promoted', 'shadow',
                        'skipDisabled', 'skipPVS', 'skipBudget'), (m[0], *map(int, m[1:]))))
              for m in STATIC.findall(text)]
    proxies = [dict(zip(('sources', 'built', 'promoted', 'shadow', 'skipDisabled', 'skipPVS', 'skipBudget'),
                        map(int, m))) for m in PROXIES.findall(text)]

    # One line a frame, for the frame's last scene.
    def histogram(rows, keys):
        counts = {}
        for row in rows:
            key = ' '.join(f'{name}:{row[name]}' for name in keys)
            counts[key] = counts.get(key, 0) + 1
        return counts
    return dict(staticLines=len(static), staticCounts=sorted({row['count'] for row in static}),
                staticMaxPromoted=max((row['promoted'] for row in static), default=None),
                staticFrames=histogram(static, ('count', 'promoted', 'shadow', 'skipDisabled', 'skipPVS', 'skipBudget')),
                proxyLines=len(proxies),
                proxyMaxPromoted=max((row['promoted'] for row in proxies), default=None),
                proxyFrames=histogram(proxies, ('built', 'promoted', 'shadow', 'skipDisabled', 'skipPVS', 'skipBudget')))


def fixture_loaded(result, lights):
    """Whether every counter line reports the fixture's own light count.

    The fixture sits in the isolated home, and a light file the engine package
    ships for the same map outranks it; a run that loaded that file instead
    proves nothing about the fixture, so it fails.
    """
    result['fixtureLoaded'] = result['counters']['staticCounts'] == [len(lights['lights'])]
    return result['fixtureLoaded']


def replay(executable, installation, profile, output, demo, lights, case, renderer, offline):
    spec = PROFILES[profile]
    settings, commands = CASES[case]
    row = output / f'{renderer}-{case}'
    home = row / 'home'
    game = home / spec['game']
    (game / 'demos').mkdir(parents=True)
    (game / 'maps').mkdir(parents=True)
    shutil.copy2(demo, game / 'demos' / demo.name)
    (game / 'maps' / f"{spec['map']}.lights.json").write_text(json.dumps(lights, indent=1) + '\n', encoding='utf-8')
    steps, previous = [], 0
    for index, frame in enumerate(FRAMES):
        steps.append(f'wait {frame - previous}; screenshotPNG shot{index}')
        previous = frame
    # The debug counters are cheat cvars; the demo's devmap server allows them.
    # The lagometer graphs wall-clock frame times, the one thing a timedemo
    # replay does not repeat. A notify line printed in the frame's own
    # millisecond still draws at con_notifytime 0, and the counters print every
    # frame, so -1 hides them all.
    action = ('r_staticLightDebug 1; r_surfaceLightProxyDebug 1; viewpos; ' + commands +
              '; '.join(steps) + '; wait 10; quit')
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  com_introplayed='1', timedemo='1', con_notifytime='-1', cg_lagometer='0',
                  **settings, activeAction=action)
    result, text = launch(executable, installation, profile, home, renderer, values, ['+demo', demo.name], 180,
                          offline)
    result.update(case=case, renderer=renderer, settings=settings, commands=commands,
                  views=[list(map(int, view)) for view in VIEWPOS.findall(text)],
                  counters=counters(text), screenshots=[])
    for index, _ in enumerate(FRAMES):
        capture = next(home.rglob(f'shot{index}.png'), None)
        if capture is not None:
            kept = row / f'shot{index}.png'
            shutil.copy2(capture, kept)
            result['screenshots'].append(dict(frame=FRAMES[index], path=str(kept), sha256=sha(kept)))
    shutil.rmtree(home)
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and not result['errors']
                        and len(result['screenshots']) == len(FRAMES) and result['binariesUnchanged']
                        and fixture_loaded(result, lights))
    return result


def live(executable, installation, profile, output, renderers):
    """One running client per renderer: the fixture lights on, off, then on again."""
    spec = PROFILES[profile]
    view_home = output / 'view' / 'home'
    values = dict(developer='0', com_introplayed='1', activeAction=spec['join'] + 'viewpos; wait 5; quit')
    view, text = launch(executable, installation, profile, view_home, 'glx', values, ['+devmap', spec['map']], 180)
    view['views'] = [list(map(int, found)) for found in VIEWPOS.findall(text)]
    shutil.rmtree(view_home)
    view['passed'] = view['exitCode'] == 0 and not view['forcedStop'] and not view['errors'] and bool(view['views'])
    runs = []
    if not view['passed']:
        return view, None, runs
    lights = light_file(view['views'][-1], spec['map'])
    for renderer in renderers:
        row = output / f'{renderer}-live'
        home = row / 'home'
        maps = home / spec['game'] / 'maps'
        maps.mkdir(parents=True)
        (maps / f"{spec['map']}.lights.json").write_text(json.dumps(lights, indent=1) + '\n', encoding='utf-8')
        # A screenshot is written at the end of the next frame, so the setting
        # changes only after the capture's frame has run.
        action = (spec['join'] + 'r_staticLightDebug 1; viewpos; wait 20; screenshotPNG on; wait 2; '
                  'r_staticLights 0; wait 20; screenshotPNG off; wait 2; r_staticLights 1; wait 20; '
                  'screenshotPNG again; wait 10; quit')
        values = dict(developer='0', com_introplayed='1', con_notifytime='-1', r_staticLights='1',
                      activeAction=action)
        result, text = launch(executable, installation, profile, home, renderer, values, ['+devmap', spec['map']], 180)
        result.update(renderer=renderer, views=[list(map(int, found)) for found in VIEWPOS.findall(text)],
                      counters=counters(text), screenshots={})
        for name in ('on', 'off', 'again'):
            capture = next(home.rglob(f'{name}.png'), None)
            if capture is not None:
                kept = row / f'{name}.png'
                shutil.copy2(capture, kept)
                result['screenshots'][name] = dict(path=str(kept), sha256=sha(kept))
        shutil.rmtree(home)
        if len(result['screenshots']) == 3:
            shots = result['screenshots']
            result['onVersusOff'] = compare(shots['on']['path'], shots['off']['path'])
            result['onVersusAgain'] = compare(shots['on']['path'], shots['again']['path'])
        result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and not result['errors']
                            and len(result['screenshots']) == 3 and result['binariesUnchanged']
                            and fixture_loaded(result, lights))
        runs.append(result)
        print(renderer, 'live', 'PASS' if result['passed'] else 'FAIL', json.dumps(result['counters']['staticFrames']),
              json.dumps({key: result.get(key, {}).get('changedPixels') for key in ('onVersusOff', 'onVersusAgain')}),
              flush=True)
    return view, lights, runs


def compare(a, b):
    import numpy
    from PIL import Image
    x = numpy.asarray(Image.open(a).convert('RGB'), dtype=numpy.float64)
    y = numpy.asarray(Image.open(b).convert('RGB'), dtype=numpy.float64)
    diff = numpy.abs(x - y)
    changed = diff.sum(axis=2) >= 6
    out = dict(identical=bool(numpy.array_equal(x, y)), meanAbsDiff=round(float(diff.mean()), 4),
               changedPixels=int(changed.sum()),
               meanRGB=[round(float(v), 2) for v in y.reshape(-1, 3).mean(axis=0)])
    if changed.any():
        out['meanDeltaInChanged'] = [round(float(v), 2) for v in (y - x)[changed].mean(axis=0)]
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=sorted(PROFILES), default='rtcw-mp')
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--executable', type=Path, default=ROOT / 'meson/build/win32-dev/fntech3.exe')
    parser.add_argument('--case', action='append', choices=sorted(CASES))
    parser.add_argument('--renderer', action='append', choices=('glx', 'vk', 'rtx'))
    parser.add_argument('--reuse', type=Path, help="another run's report.json: replay its demo and fixture")
    parser.add_argument('--against', type=Path, help="another run's report.json: compare every capture with it")
    parser.add_argument('--offline', action='store_true', help='replay with net_enabled 0')
    args = parser.parse_args()
    spec = PROFILES[args.profile]
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    installation = args.installation.resolve()
    executable = args.executable.resolve()
    before = inventory(installation)
    report = dict(scope=__doc__, dateUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  profile=args.profile, installation=str(installation), map=spec['map'], frames=FRAMES,
                  width=WIDTH, height=HEIGHT, executable=str(executable))
    if spec.get('live'):
        report['view'], report['lights'], report['runs'] = live(executable, installation, args.profile, output,
                                                                args.renderer or spec['renderers'])
        report['installationUnchanged'] = inventory(installation) == before
        report['passed'] = (report['view']['passed'] and bool(report['runs']) and
                            all(run['passed'] for run in report['runs']) and report['installationUnchanged'])
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('PASS' if report['passed'] else 'FAIL')
        return 0 if report['passed'] else 1
    if args.reuse:
        reused = json.loads(args.reuse.read_text(encoding='utf-8'))
        assert reused['profile'] == args.profile and reused['record']['passed']
        report['record'] = recorded = dict(reused['record'], reusedFrom=str(args.reuse.resolve()))
        assert sha(recorded['demo']['path']) == recorded['demo']['sha256']
    else:
        report['record'] = recorded = record(executable, installation, args.profile, output)
    runs = []
    if recorded['passed']:
        lights = light_file(recorded['views'][-1], spec['map'])
        (output / f"{spec['map']}.lights.json").write_text(json.dumps(lights, indent=1) + '\n', encoding='utf-8')
        report['lights'] = lights
        demo = Path(recorded['demo']['path'])
        for case in args.case or CASES:
            for renderer in spec['renderers']:
                if (args.renderer and renderer not in args.renderer) or (case == 'test' and renderer == 'rtx'):
                    continue
                result = replay(executable, installation, args.profile, output, demo, lights, case, renderer,
                                args.offline)
                runs.append(result)
                print(renderer, case, 'PASS' if result['passed'] else 'FAIL',
                      json.dumps(result['counters']['staticFrames']), json.dumps(result['counters']['proxyFrames']),
                      flush=True)
    report['runs'] = runs
    comparisons = []
    shots = {(run['renderer'], run['case']): run['screenshots'] for run in runs if run['passed']}
    for (renderer, case), captures in sorted(shots.items()):
        reference = 'mode0-off' if case.startswith('mode0') else 'nodynamic-off' if case.startswith('nodynamic') else 'off'
        if case == reference or (renderer, reference) not in shots:
            continue
        for mine, theirs in zip(captures, shots[(renderer, reference)]):
            comparisons.append(dict(renderer=renderer, case=case, against=reference, frame=mine['frame'],
                                    **compare(theirs['path'], mine['path'])))
    report['comparisons'] = comparisons
    report['identicalWhereExcluded'] = (args.profile != 'rtcw-mp' or
                                        all(row['identical'] for row in comparisons
                                            if (row['against'], row['case']) in IDENTICAL))
    if args.against:
        other = json.loads(args.against.read_text(encoding='utf-8'))
        theirs = {(run['renderer'], run['case']): run['screenshots'] for run in other['runs'] if run['passed']}
        report['againstReport'] = str(args.against.resolve())
        report['againstComparisons'] = [
            dict(renderer=renderer, case=case, frame=mine['frame'], **compare(their['path'], mine['path']))
            for (renderer, case), captures in sorted(shots.items()) if (renderer, case) in theirs
            for mine, their in zip(captures, theirs[(renderer, case)])]
    report['installationUnchanged'] = inventory(installation) == before
    report['passed'] = (recorded['passed'] and bool(runs) and all(run['passed'] for run in runs)
                        and report['identicalWhereExcluded'] and report['installationUnchanged'])
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for row in comparisons:
        print(row['renderer'], row['case'], 'vs', row['against'], row['frame'], 'identical' if row['identical'] else
              f"changed:{row['changedPixels']} mean:{row['meanAbsDiff']} delta:{row.get('meanDeltaInChanged')}")
    for row in report.get('againstComparisons', ()):
        print(row['renderer'], row['case'], 'vs other runtime', row['frame'], 'identical' if row['identical'] else
              f"changed:{row['changedPixels']} mean:{row['meanAbsDiff']}")
    print('PASS' if report['passed'] else 'FAIL')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
