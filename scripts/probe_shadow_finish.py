#!/usr/bin/env python3
"""Measure how dark each renderer draws stencil shadows, on identical frames.

With `cg_shadows 2` a renderer marks every shadowed pixel in the stencil
buffer and then multiplies the marked pixels by a constant in one full-screen
pass, RB_ShadowFinish. GL multiplies by 0.6 with fog off and no texture. The
Vulkan and RTX renderers drew that pass with whatever uniform the view's last
stage had pushed, so RTCW's global fog (and Alice's far-plane fog) reached it.

The probe records one short demo on a listen server, then replays it with
`timedemo 1` -- one snapshot a rendered frame -- once with `cg_shadows 2` and
once with `cg_shadows 0`, for each renderer and each runtime it is given (for
example a build without a change and one with it). Captures at the same frame
count therefore show the same scene, and a shadowed capture divided by the
unshadowed one over the pixels the shadows changed is the pass's multiplier.

Every action is a console command from a config the probe writes into the
isolated home (a private desktop, windowed, no keyboard or mouse input); every
image is the engine's own `screenshotPNG`. The installation is read-only; its
file inventory is compared before and after.
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

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

WIDTH, HEIGHT = 800, 600
DEMO = 'shadowfinish'
ERRORS = re.compile(r'^.*(?:ERROR:|Sys_Error|fatal crashed|recursive error|Server crashed).*$', re.M | re.I)

# How each profile reaches a player who stands in view of their own shadow.
# RTCW MP opens its limbo menu on joining; the cgame's CloseLimboMenu shuts
# it. devmap enables cheats, so cg_thirdPerson holds in the demo too.
PROFILES = {
    'rtcw-mp': dict(game='main', map=['mp_ice'], log='qconsole.log',
                    join='wait 60; team b 0 14 0 17 0; wait 90; CloseLimboMenu; wait 60; ',
                    startup=dict(cl_autoupdate='0', cl_motd='0')),
    'q3': dict(game='baseq3', map=['q3dm17'], log='qconsole.log', join='wait 60; ',
               startup=dict(cl_motd='0')),
    'ql': dict(game='baseq3', map=['campgrounds', 'ffa'], log='qconsole.log', join='wait 120; ',
               startup=dict()),
}


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


def launch(executable, installation, profile, home, renderer, shadows, cfg_values, tail, timeout):
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
                   com_recommendedSet='1', cg_shadows=str(shadows), **spec['startup'])
    command = [str(executable)]
    for key, value in startup.items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg'] + tail
    code, forced, elapsed = run(command, executable.parent, home.parent / 'stdout.log', timeout)
    log = game / spec['log']
    text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
    (home.parent / 'console.log').write_text(text, encoding='utf-8')
    return dict(command=command, exitCode=code, forcedStop=forced, elapsedSeconds=elapsed,
                errors=ERRORS.findall(text), executable=str(executable), executableSha256=sha(executable),
                rendererSha256=sha(executable.parent / f'fnql_{renderer}_x86.dll')), text


def record(executable, installation, profile, output):
    spec = PROFILES[profile]
    row = output / 'record'
    home = row / 'home'
    action = (spec['join'] + 'cg_thirdPerson 1; wait 60; record ' + DEMO +
              '; wait 360; stoprecord; wait 30; quit')
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  com_introplayed='1', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
                  activeAction=action)
    result, _ = launch(executable, installation, profile, home, 'glx', 2, values,
                       ['+devmap'] + spec['map'], 180)
    demos = sorted((home / spec['game'] / 'demos').glob(DEMO + '.*'))
    result['demo'] = None
    if demos:
        kept = output / demos[0].name
        shutil.copy2(demos[0], kept)
        result['demo'] = dict(path=str(kept), sha256=sha(kept), bytes=kept.stat().st_size)
    shutil.rmtree(home)
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and bool(demos)
                        and not result['errors'])
    return result


def replay(executable, label, installation, profile, output, demo, renderer, shadows, frames):
    spec = PROFILES[profile]
    row = output / f'{label}-{renderer}-shadows{shadows}'
    home = row / 'home'
    (home / spec['game'] / 'demos').mkdir(parents=True)
    shutil.copy2(demo, home / spec['game'] / 'demos' / demo.name)
    steps, previous = [], 0
    for index, frame in enumerate(frames):
        steps.append(f'wait {frame - previous}; screenshotPNG shot{index}')
        previous = frame
    # The lagometer graphs wall-clock frame times, the one thing a timedemo
    # replay does not repeat; the notify lines report load times.
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  com_introplayed='1', timedemo='1', cg_thirdPerson='1', con_notifytime='0',
                  cg_lagometer='0', activeAction='; '.join(steps) + '; wait 10; quit')
    result, _ = launch(executable, installation, profile, home, renderer, shadows, values,
                       ['+demo', demo.name], 180)
    result.update(label=label, renderer=renderer, shadows=shadows, screenshots=[])
    for index, _ in enumerate(frames):
        capture = next(home.rglob(f'shot{index}.png'), None)
        if capture is not None:
            kept = row / f'shot{index}.png'
            shutil.copy2(capture, kept)
            result['screenshots'].append(dict(frame=frames[index], path=str(kept), sha256=sha(kept)))
    shutil.rmtree(home)
    result['passed'] = (result['exitCode'] == 0 and not result['forcedStop'] and not result['errors']
                        and len(result['screenshots']) == len(frames))
    return result


def load(path):
    import numpy
    from PIL import Image
    return numpy.asarray(Image.open(path).convert('RGB'), dtype=numpy.float64)


def measure(shadowed, unshadowed):
    """The pass's multiplier over the pixels the shadows darkened."""
    import numpy
    lit, dark = load(unshadowed), load(shadowed)
    mask = ( lit - dark ).sum( axis=2 ) >= 6
    # dim pixels quantize too coarsely to say anything about a multiplier
    bright = mask & ( lit.min( axis=2 ) >= 48 )
    # a shadow only darkens: anything the pass brightened is a fault
    out = dict(changedPixels=int(mask.sum()), measuredPixels=int(bright.sum()),
               brightenedPixels=int(( ( dark - lit ).sum( axis=2 ) >= 6 ).sum()))
    if bright.any():
        ratios = dark[bright] / lit[bright]
        out['meanRatio'] = [round(float(v), 4) for v in ratios.mean(axis=0)]
        out['medianRatio'] = [round(float(v), 4) for v in numpy.median(ratios, axis=0)]
    return out, mask


def compare(a, b, mask=None):
    import numpy
    x, y = load(a), load(b)
    diff = numpy.abs(x - y)
    out = dict(identical=bool(numpy.array_equal(x, y)), meanAbsDiff=round(float(diff.mean()), 4))
    if mask is not None and mask.any():
        out['meanAbsDiffInShadow'] = round(float(diff[mask].mean()), 4)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', action='append', required=True, metavar='LABEL=EXE',
                        help='a client to replay with, e.g. before=.tmp/runtime/x/fntech3.exe; the first records')
    parser.add_argument('--renderer', action='append', choices=('glx', 'vk', 'rtx'))
    parser.add_argument('--frames', default='40,80', help='replay frame counts to capture at')
    parser.add_argument('--demo', type=Path, help='replay this demo instead of recording one')
    args = parser.parse_args()
    installation = args.installation.resolve(strict=True)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    runtimes = []
    for item in args.runtime:
        label, _, path = item.partition('=')
        executable = Path(path).resolve(strict=True)
        executable.relative_to(ROOT)
        runtimes.append((label, executable))
    renderers = args.renderer or ['glx', 'vk', 'rtx']
    frames = [int(value) for value in args.frames.split(',')]
    before = inventory(installation)
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), profile=args.profile,
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  worktreeDirty=bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
                  installation=str(installation), map=PROFILES[args.profile]['map'], frames=frames,
                  runtimes={label: dict(executable=str(exe), sha256=sha(exe)) for label, exe in runtimes})
    if args.demo:
        demo = args.demo.resolve(strict=True)
        report['record'] = dict(demo=dict(path=str(demo), sha256=sha(demo)), passed=True)
    else:
        report['record'] = record(runtimes[0][1], installation, args.profile, output)
        demo = Path(report['record']['demo']['path']) if report['record']['demo'] else None
    print('record', report['record'].get('passed'), report['record'].get('demo'), flush=True)
    report['replays'] = []
    report['measurements'] = []
    glx_masks = {}
    if demo:
        for label, executable in runtimes:
            for renderer in renderers:
                pair = {}
                for shadows in (2, 0):
                    result = replay(executable, label, installation, args.profile, output, demo,
                                    renderer, shadows, frames)
                    report['replays'].append(result)
                    pair[shadows] = result
                    print(label, renderer, 'shadows', shadows, 'passed', result['passed'],
                          'errors', result['errors'], flush=True)
                if not (pair[2]['passed'] and pair[0]['passed']):
                    continue
                for index, frame in enumerate(frames):
                    shot, mask = measure(pair[2]['screenshots'][index]['path'], pair[0]['screenshots'][index]['path'])
                    shot.update(label=label, renderer=renderer, frame=frame)
                    report['measurements'].append(shot)
                    if label == runtimes[0][0] and renderer == 'glx':
                        glx_masks[frame] = mask
                    print(' ', label, renderer, 'frame', frame, shot, flush=True)
        # Every capture against GL's from the first runtime, frame by frame,
        # and over the pixels GL's shadows darkened in that frame.
        reference = {(r['shadows'], s['frame']): s['path'] for r in report['replays']
                     if r['label'] == runtimes[0][0] and r['renderer'] == 'glx' for s in r['screenshots']}
        report['againstGlx'] = []
        for r in report['replays']:
            for s in r['screenshots']:
                key = (r['shadows'], s['frame'])
                if key in reference and s['path'] != reference[key]:
                    entry = compare(s['path'], reference[key], glx_masks.get(s['frame']))
                    entry.update(label=r['label'], renderer=r['renderer'], shadows=r['shadows'], frame=s['frame'])
                    report['againstGlx'].append(entry)
        # The same renderer across runtimes: what the change alone moved.
        report['acrossRuntimes'] = []
        for r in report['replays']:
            if r['label'] == runtimes[0][0]:
                continue
            base = next((b for b in report['replays'] if b['label'] == runtimes[0][0]
                         and b['renderer'] == r['renderer'] and b['shadows'] == r['shadows']), None)
            if not base:
                continue
            for s, t in zip(r['screenshots'], base['screenshots']):
                entry = compare(s['path'], t['path'])
                entry.update(label=r['label'], against=runtimes[0][0], renderer=r['renderer'],
                             shadows=r['shadows'], frame=s['frame'])
                report['acrossRuntimes'].append(entry)
    report['installationUnchanged'] = before == inventory(installation)
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('installation unchanged', report['installationUnchanged'], 'output', output)
    passed = (report['installationUnchanged'] and report['record'].get('passed')
              and all(r['passed'] for r in report['replays']))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
