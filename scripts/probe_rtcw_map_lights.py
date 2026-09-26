#!/usr/bin/env python3
"""Show RTCW's generated static map lights, served from the engine package.

Every official RTCW map now has a `maps/<map>.lights.json` in the engine's
package archive (`fntech3-pkg.fnz`, beside the executable), converted from the
map's own `light` entities by `scripts/generate_stock_rtcw_lights.py`. The
installation ships none and the probe writes none, so a light the renderers
load can only have come from the package.

RTCW MP: the probe records one short demo on a listen server, then replays it
with `timedemo 1` (one snapshot a rendered frame) under each renderer and
setting, so captures at the same frame show the same scene. RTCW SP records a
demo only with `g_synchronousClients 1`, so each renderer's client is captured
live instead, with the lights on, off and on again. `r_staticLightReload`
prints how many lights the renderer loaded, and `r_staticLightDebug` prints
each frame's promotion counters. Every image is the engine's own
`screenshotPNG`.

Every action is a console command from a config the probe writes into the
isolated home: a private desktop, windowed, no keyboard or mouse input, and
`net_enabled 0` so a runtime copied to a new path opens no socket. The
installation is read-only; its file inventory is compared before and after.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import re
import shutil
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

WIDTH, HEIGHT = 800, 600
DEMO = 'maplights'
FRAMES = (40, 80)
# fnTech3 refuses more startup commands than this with a dialog, which on a
# private desktop waits for ever; settings go in the exec'd config instead.
MAX_STARTUP_COMMANDS = 32
ERRORS = re.compile(r'^.*(?:ERROR:|Sys_Error|fatal crashed|recursive error|Server crashed).*$', re.M | re.I)
VIEWPOS = re.compile(r'^\((-?\d+) (-?\d+) (-?\d+)\) : (-?\d+)\s*$', re.M)
LOADED = re.compile(r'^static lights file:(\S+) version:(\d+) loaded:(\d+) point:(\d+) spot:(\d+) '
                    r'spatial:(\d+)/(\d+) skipped unsupported:(\d+) invalid:(\d+) overflow:(\d+)', re.M)
RELOADED = re.compile(r'^Reloaded (\d+) static map lights from (\S+)', re.M)
STATIC = re.compile(r'^static lights file:(\S+) loaded:(\d+) parsefail:(\d+) count:(\d+) point:(\d+) spot:(\d+) '
                    r'spatial:\d+/\d+ promoted:(\d+) shadow:(\d+) skip disabled:(\d+) pvs:(\d+) budget:(\d+)', re.M)

PROFILES = {
    # RTCW MP opens its limbo menu on joining; the cgame's CloseLimboMenu shuts it.
    'rtcw-mp': dict(game='main', map='mp_castle',
                    join='wait 60; team b 0 14 0 17 0; wait 90; CloseLimboMenu; wait 60; ',
                    startup=dict(cl_autoupdate='0', cl_motd='0')),
    # devmap loads an SP map without its briefing.
    'rtcw-sp': dict(game='main', map='escape1', join='wait 200; ', startup=dict(cl_motd='0'), live=True),
}
RENDERERS = ('glx', 'vk', 'rtx')

# name: settings. Mode 0 is RTCW's original projected-light option, which
# takes no engine light: its pair must match.
CASES = {
    'off': dict(r_staticLights='0'),
    'static': dict(r_staticLights='1'),
    'static-noshadow': dict(r_staticLights='1', r_staticLightShadows='0'),
    'mode0-off': dict(r_dlightMode='0', r_staticLights='0'),
    'mode0-static': dict(r_dlightMode='0', r_staticLights='1'),
}
IDENTICAL = (('mode0-off', 'mode0-static'),)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): (path.stat().st_size, path.stat().st_mtime_ns)
            for path in installation.rglob('*') if path.is_file()}


def package_lights(runtime, profile, name):
    """The light file the runtime's package serves for a map, if any."""
    member = f'games/{profile}/main/maps/{name}.lights.json'
    with zipfile.ZipFile(runtime / 'fntech3-pkg.fnz') as archive:
        if member not in archive.namelist():
            return None
        payload = archive.read(member)
    document = json.loads(payload)
    return dict(member=member, sha256=hashlib.sha256(payload).hexdigest(), lights=len(document['lights']),
                spots=sum(1 for light in document['lights'] if light['type'] == 'spot'))


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


def launch(runtime, installation, profile, home, renderer, cfg_values, tail, timeout):
    spec = PROFILES[profile]
    executable = runtime / 'fntech3.exe'
    game = home / spec['game']
    game.mkdir(parents=True, exist_ok=True)
    (game / 'probe.cfg').write_text(''.join(f'set {key} "{value}"\n' for key, value in cfg_values.items()),
                                    encoding='utf-8')
    startup = dict(com_gameProfile=profile, fs_basepath=str(installation), fs_homepath=str(home),
                   fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth=str(WIDTH),
                   r_customHeight=str(HEIGHT), in_mouse='0', in_joystick='0', in_nograb='1',
                   s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                   cl_renderer=renderer, logfile='2', net_enabled='0', rtx_rt_mode='0', rtx_rt_require='0',
                   com_maxfps='60', com_recommendedSet='1', **spec['startup'])
    command = [str(executable)]
    for key, value in startup.items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg'] + tail
    assert sum(argument.startswith('+') for argument in command) <= MAX_STARTUP_COMMANDS, command
    binaries = [executable, runtime / f'fnql_{renderer}_x86.dll', runtime / 'fntech3-pkg.fnz']
    before = {path.name: sha(path) for path in binaries}
    code, forced, elapsed = run(command, runtime, home.parent / 'stdout.log', timeout)
    log = game / 'qconsole.log'
    text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
    (home.parent / 'console.log').write_text(text, encoding='utf-8')
    after = {path.name: sha(path) for path in binaries}
    return dict(command=command, exitCode=code, forcedStop=forced, elapsedSeconds=elapsed,
                errors=ERRORS.findall(text), runtimeSha256=before, binariesUnchanged=before == after), text


def loaded(text):
    """What the renderer loaded, from r_staticLightReload with r_staticLightDebug on."""
    rows = [dict(zip(('file', 'version', 'loaded', 'point', 'spot', 'spatialized', 'spatialFallback',
                      'skippedUnsupported', 'skippedInvalid', 'skippedOverflow'), (m[0], *map(int, m[1:]))))
            for m in LOADED.findall(text)]
    return dict(files=rows, reloaded=[dict(count=int(m[0]), file=m[1]) for m in RELOADED.findall(text)])


def counters(text):
    static = [dict(zip(('file', 'loaded', 'parseFailed', 'count', 'point', 'spot', 'promoted', 'shadow',
                        'skipDisabled', 'skipPVS', 'skipBudget'), (m[0], *map(int, m[1:]))))
              for m in STATIC.findall(text)]
    frames = {}
    for row in static:
        key = ' '.join(f'{name}:{row[name]}' for name in ('count', 'promoted', 'shadow', 'skipDisabled',
                                                           'skipPVS', 'skipBudget'))
        frames[key] = frames.get(key, 0) + 1
    return dict(staticLines=len(static), staticMaxPromoted=max((row['promoted'] for row in static), default=None),
                staticMaxShadow=max((row['shadow'] for row in static), default=None), staticFrames=frames)


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


def record(runtime, installation, profile, output):
    spec = PROFILES[profile]
    home = output / 'record' / 'home'
    action = spec['join'] + 'viewpos; wait 30; record ' + DEMO + '; wait 300; viewpos; stoprecord; wait 30; quit'
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  com_introplayed='1', com_maxfpsUnfocused='60', com_maxfpsMinimized='60', activeAction=action)
    result, text = launch(runtime, installation, profile, home, 'glx', values, ['+devmap', spec['map']], 180)
    result['views'] = [list(map(int, view)) for view in VIEWPOS.findall(text)]
    demos = sorted((home / spec['game'] / 'demos').glob(DEMO + '.*'))
    result['demo'] = None
    if demos:
        kept = output / demos[0].name
        shutil.copy2(demos[0], kept)
        result['demo'] = dict(path=str(kept), sha256=sha(kept), bytes=kept.stat().st_size)
    shutil.rmtree(home)
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and bool(demos)
                        and not result['errors'] and bool(result['views']))
    return result


def replay(runtime, installation, profile, output, demo, case, renderer):
    spec = PROFILES[profile]
    row = output / f'{renderer}-{case}'
    home = row / 'home'
    game = home / spec['game']
    (game / 'demos').mkdir(parents=True)
    shutil.copy2(demo, game / 'demos' / demo.name)
    steps, previous = [], 0
    for index, frame in enumerate(FRAMES):
        steps.append(f'wait {frame - previous}; screenshotPNG shot{index}')
        previous = frame
    # The debug cvars are cheats; the demo's devmap server allows them. The
    # lagometer graphs wall-clock frame times, the one thing a timedemo replay
    # does not repeat; con_notifytime -1 hides the counters printed each frame.
    action = ('r_staticLightDebug 1; r_staticLightReload; viewpos; ' + '; '.join(steps) + '; wait 10; quit')
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  com_introplayed='1', timedemo='1', con_notifytime='-1', cg_lagometer='0',
                  **CASES[case], activeAction=action)
    result, text = launch(runtime, installation, profile, home, renderer, values, ['+demo', demo.name], 180)
    result.update(case=case, renderer=renderer, settings=CASES[case], loaded=loaded(text),
                  views=[list(map(int, view)) for view in VIEWPOS.findall(text)],
                  counters=counters(text), screenshots=[])
    for index, _ in enumerate(FRAMES):
        capture = next(home.rglob(f'shot{index}.png'), None)
        if capture is not None:
            kept = row / f'shot{index}.png'
            shutil.copy2(capture, kept)
            result['screenshots'].append(dict(frame=FRAMES[index], path=str(kept), sha256=sha(kept)))
    stray = sorted(str(path.relative_to(home)) for path in home.rglob('*.lights.json'))
    shutil.rmtree(home)
    result['homeLightFiles'] = stray
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and not result['errors']
                        and len(result['screenshots']) == len(FRAMES) and result['binariesUnchanged']
                        and not stray)
    return result


def live(runtime, installation, profile, output, renderers, extra):
    """One running client per renderer: the package's lights on, off, then on again."""
    spec = PROFILES[profile]
    runs = []
    for renderer in renderers:
        row = output / f'{renderer}-live'
        home = row / 'home'
        # A screenshot is written at the end of the next frame, so a setting
        # changes only after the capture's frame has run.
        action = (spec['join'] + extra + 'r_staticLightDebug 1; r_staticLightReload; viewpos; wait 20; '
                  'screenshotPNG on; wait 2; r_staticLights 0; wait 20; screenshotPNG off; wait 2; '
                  'r_staticLights 1; wait 20; screenshotPNG again; wait 2; r_staticLightShadows 0; wait 20; '
                  'screenshotPNG noshadow; wait 10; quit')
        values = dict(developer='0', com_introplayed='1', con_notifytime='-1', r_staticLights='1',
                      activeAction=action)
        result, text = launch(runtime, installation, profile, home, renderer, values, ['+devmap', spec['map']], 240)
        result.update(renderer=renderer, loaded=loaded(text),
                      views=[list(map(int, found)) for found in VIEWPOS.findall(text)],
                      counters=counters(text), screenshots={})
        for name in ('on', 'off', 'again', 'noshadow'):
            capture = next(home.rglob(f'{name}.png'), None)
            if capture is not None:
                kept = row / f'{name}.png'
                shutil.copy2(capture, kept)
                result['screenshots'][name] = dict(path=str(kept), sha256=sha(kept))
        stray = sorted(str(path.relative_to(home)) for path in home.rglob('*.lights.json'))
        shutil.rmtree(home)
        result['homeLightFiles'] = stray
        shots = result['screenshots']
        if len(shots) == 4:
            result['offVersusOn'] = compare(shots['off']['path'], shots['on']['path'])
            result['onVersusAgain'] = compare(shots['on']['path'], shots['again']['path'])
            result['noshadowVersusAgain'] = compare(shots['noshadow']['path'], shots['again']['path'])
        result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and not result['errors']
                            and len(shots) == 4 and result['binariesUnchanged'] and not stray)
        runs.append(result)
        print(renderer, 'live', 'PASS' if result['passed'] else 'FAIL', json.dumps(result['loaded']['reloaded']),
              json.dumps(result['counters']['staticFrames']),
              json.dumps({key: result.get(key, {}).get('changedPixels')
                          for key in ('offVersusOn', 'onVersusAgain', 'noshadowVersusAgain')}), flush=True)
    return runs


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--profile', choices=sorted(PROFILES), default='rtcw-mp')
    parser.add_argument('--map', help='override the profile map')
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'meson/build/win32-dev',
                        help='directory holding fntech3.exe, the renderer DLLs and fntech3-pkg.fnz')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--case', action='append', choices=sorted(CASES))
    parser.add_argument('--renderer', action='append', choices=RENDERERS)
    parser.add_argument('--reuse', type=Path, help="another run's report.json: replay its demo")
    parser.add_argument('--against', type=Path, help="another run's report.json: compare every capture with it")
    parser.add_argument('--extra', default='', help='console commands run after joining (live runs)')
    args = parser.parse_args()
    spec = PROFILES[args.profile]
    if args.map:
        PROFILES[args.profile] = spec = dict(spec, map=args.map)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    installation = args.installation.resolve()
    runtime = args.runtime.resolve()
    renderers = args.renderer or RENDERERS
    before = inventory(installation)
    shipped = list(installation.rglob('*.lights.json'))
    report = dict(scope=__doc__, dateUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  profile=args.profile, installation=str(installation), map=spec['map'], frames=FRAMES,
                  width=WIDTH, height=HEIGHT, runtime=str(runtime),
                  package=package_lights(runtime, args.profile, spec['map']),
                  installationLightFiles=[str(path) for path in shipped])
    if spec.get('live'):
        report['runs'] = live(runtime, installation, args.profile, output, renderers, args.extra)
        expected = report['package']['lights'] if report['package'] else 0
        report['loadedFromPackage'] = all(
            [row['count'] for row in run['loaded']['reloaded']] == [expected] for run in report['runs'])
        report['installationUnchanged'] = inventory(installation) == before
        report['passed'] = (bool(report['runs']) and all(run['passed'] for run in report['runs'])
                            and report['installationUnchanged'] and not shipped)
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('loaded from package:', report['loadedFromPackage'])
        print('PASS' if report['passed'] else 'FAIL')
        return 0 if report['passed'] else 1
    if args.reuse:
        reused = json.loads(args.reuse.read_text(encoding='utf-8'))
        assert reused['profile'] == args.profile and reused['record']['passed']
        report['record'] = recorded = dict(reused['record'], reusedFrom=str(args.reuse.resolve()))
        assert sha(recorded['demo']['path']) == recorded['demo']['sha256']
    else:
        report['record'] = recorded = record(runtime, installation, args.profile, output)
    runs = []
    if recorded['passed']:
        demo = Path(recorded['demo']['path'])
        for case in args.case or CASES:
            for renderer in renderers:
                result = replay(runtime, installation, args.profile, output, demo, case, renderer)
                runs.append(result)
                print(renderer, case, 'PASS' if result['passed'] else 'FAIL', json.dumps(result['loaded']['reloaded']),
                      json.dumps(result['counters']['staticFrames']), flush=True)
    report['runs'] = runs
    expected = report['package']['lights'] if report['package'] else 0
    report['loadedFromPackage'] = bool(runs) and all(
        [row['count'] for row in run['loaded']['reloaded']] == [expected] for run in runs)
    comparisons = []
    shots = {(run['renderer'], run['case']): run['screenshots'] for run in runs if run['passed']}
    for (renderer, case), captures in sorted(shots.items()):
        reference = 'mode0-off' if case.startswith('mode0') else 'static' if case == 'static-noshadow' else 'off'
        if case == reference or (renderer, reference) not in shots:
            continue
        for mine, theirs in zip(captures, shots[(renderer, reference)]):
            comparisons.append(dict(renderer=renderer, case=case, against=reference, frame=mine['frame'],
                                    **compare(theirs['path'], mine['path'])))
    report['comparisons'] = comparisons
    report['identicalWhereExcluded'] = all(row['identical'] for row in comparisons
                                           if (row['against'], row['case']) in IDENTICAL)
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
                        and report['identicalWhereExcluded'] and report['installationUnchanged'] and not shipped)
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for row in comparisons:
        print(row['renderer'], row['case'], 'vs', row['against'], row['frame'], 'identical' if row['identical'] else
              f"changed:{row['changedPixels']} mean:{row['meanAbsDiff']} delta:{row.get('meanDeltaInChanged')}")
    for row in report.get('againstComparisons', ()):
        print(row['renderer'], row['case'], 'vs other runtime', row['frame'], 'identical' if row['identical'] else
              f"changed:{row['changedPixels']} mean:{row['meanAbsDiff']}")
    print('loaded from package:', report['loadedFromPackage'])
    print('PASS' if report['passed'] else 'FAIL')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
