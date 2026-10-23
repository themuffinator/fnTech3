#!/usr/bin/env python3
"""Compare static single-player Elite Force views with retail ``stvoy.exe``.

Each scene is a single-player map and a `setviewpos`. Both engines run it the
same way: `fixedtime 50`, so every frame carries 50 ms of game time; `devmap`
the map; wait until its opening scene is over; freeze its NPCs and effects
(`d_npcfreeze 1`, `fx_freeze 1`), hide the HUD and the view weapon; move to
the view; wait three seconds of game time for the player to land; take the
engine's own `screenshot`; and then print `viewpos`, so the log shows that
every engine stood at the same point without the text reaching the image.
Retail counts two `wait`s a frame where fnTech3 counts one, so retail's
waits are doubled and both screenshots fall at the same game time.
``--matched`` also runs fnTech3 with the settings retail's first start
leaves in force (``MATCHED``), so renderers are compared like for like.
The game seeds its randomness from the wall clock in both engines, so views
are chosen without NPCs in them. Retail is run twice per scene, to show how
much it differs from itself; fnTech3 once per renderer.

The run reports, for every image against retail's first, the mean absolute
difference of each colour channel (0-255) and the share of pixels whose
largest channel difference exceeds 16.

Windows only. Every process runs windowed (640x480) on a private desktop that
is never switched to, without sound or mouse, with isolated writable folders
under ``--output``. Retail runs from a copy of ``stvoy.exe`` and its libraries
beside the staged Holomatch runtime (``.tmp/runtime/stvef-retail``) and reads
a copy of the four retail paks, as every Elite Force probe does; the
installation is compared before and after. fnTech3 reads the same four paks
and the game libraries from ``.tmp/stvef-stock``, without the installation's
third-party texture pack. No input is injected.
"""
from pathlib import Path
import argparse
import json
import re
import shutil
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'scripts'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402
from probe_legacy_discovery import INSTALLS, stage_retail  # noqa: E402
from probe_legacy_sessions import PROFILES, inventory, sha  # noqa: E402

INSTALLATION = INSTALLS['stvef-mp']
# fnTech3's game folder: the four retail paks and the game libraries alone, as
# retail reads them, without the installation's third-party texture pack
# (docs/fntech3/2026-10-09-stvef-sp-campaign.md).
STOCK = ROOT / '.tmp' / 'stvef-stock'
# (label, map, setviewpos)
SCENES = [
    ('voy2', 'voy2', '-2048 3846 -181 0'),
    ('stasis1', 'stasis1', '-340 2296 159 0'),
    ('scav2', 'scav2', '-680 1576 151 0'),
    ('dn1', 'dn1', '-950 2 1027 0'),
    ('borg2', 'borg2', '1728 -1676 151 0'),
    ('scav3', 'scav3', '-392 -80 167 0'),
    ('forge2', 'forge2', '1162 -214 7 0'),
    ('voy14', 'voy14', '908 224 23 0'),
]
# Game seconds a level gets for its opening scene, and the player to come to
# rest after moving, before the view is taken.
SETTLE_SECONDS = 30
REST_SECONDS = 3
FRAME_MSEC = 50
COMMON = dict(r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', s_initsound='0', com_maxfps='60',
              logfile='2')
# The settings retail's own first start leaves in force: stvoy.exe 1.20 runs
# the game's perfect.cfg when com_FirstTime is unset on a CPU it classes as
# 36 (MMX, SSE and SSE2, no 3DNow!; Com_Init 0x412ceb..0x412d33, class
# 0x42ef00), which turns on flares and projection shadows, while its
# latched picmip stays at 1 for the session, with retail's texture filter.
# --matched gives fnTech3 these, to compare renderers like for like.
MATCHED = dict(r_flares='1', cg_shadows='3', r_picmip='1', r_textureMode='GL_LINEAR_MIPMAP_NEAREST',
               r_ext_max_anisotropy='16', r_subdivisions='2', r_lodbias='0')
VIEWPOS = re.compile(r'\((-?\d+) (-?\d+) (-?\d+)\) : (-?\d+)')


def stage():
    layout = stage_retail('stvef-mp', PROFILES['stvef-mp'], INSTALLATION)
    source, target = INSTALLATION / 'stvoy.exe', layout['root'] / 'stvoy.exe'
    if not target.exists() or sha(source) != sha(target):
        shutil.copy2(source, target)
    return layout


def script(level, view, waits_per_frame):
    frames = lambda seconds: int(seconds * 1000 / FRAME_MSEC) * waits_per_frame
    return [f'fixedtime {FRAME_MSEC}', f'devmap {level}', f'wait {frames(SETTLE_SECONDS)}',
            'd_npcfreeze 1', 'fx_freeze 1', 'cg_draw2D 0', 'cg_drawGun 0', f'setviewpos {view}',
            f'wait {frames(REST_SECONDS)}', 'screenshot', f'wait {2 * waits_per_frame}', 'viewpos',
            f'wait {frames(1)}', 'quit']


def run(folder, engine, level, view, layout, runtime, renderer, extra=None):
    folder.mkdir(parents=True)
    settings = dict(COMMON, **(extra or {}))
    if engine == 'retail':
        base = folder / 'sb'
        (base / 'BaseEF').mkdir(parents=True)
        cfg, shots = base / 'BaseEF' / 'probeview.cfg', base / 'BaseEF' / 'screenshots'
        exe, cwd = layout['root'] / 'stvoy.exe', layout['root']
        settings = dict(fs_basepath=str(base), fs_cdpath=str(layout['cdpath']), **settings)
        lines = script(level, view, 2)
    else:
        home = folder / 'home'
        (home / 'BaseEF').mkdir(parents=True)
        (home / 'BaseEF' / 'efconfig.cfg').write_text('// empty\n', encoding='latin1')
        cfg, shots = home / 'BaseEF' / 'probeview.cfg', home / 'BaseEF' / 'screenshots'
        exe, cwd = runtime / 'fntech3.exe', runtime
        settings = dict(com_gameProfile='stvef-sp', fs_basepath=str(STOCK), fs_homepath=str(home),
                        com_steamIntegration='0', cl_renderer=renderer, in_nograb='1', **settings)
        if renderer == 'rtx':
            settings.update(rtx_rt_mode='0', r_hdr='0')
        lines = script(level, view, 1)
    cfg.write_text('\n'.join(lines) + '\n', encoding='latin1')
    command = [str(exe)]
    for key, value in settings.items():
        command += ['+set', key, value]
    command += ['+exec', 'probeview.cfg']
    process = WindowsPrivateProcess(command, cwd, folder / 'stdout.log')
    began = time.monotonic()
    while process.poll() is None and time.monotonic() - began < 180:
        time.sleep(0.5)
    forced = process.poll() is None
    if forced:
        process.terminate()
    exit_code = process.poll()
    process.close()
    images = sorted(shots.glob('*')) if shots.exists() else []
    log = cfg.parent / 'qconsole.log'
    found = VIEWPOS.findall(log.read_text(encoding='latin1', errors='replace')) if log.exists() else []
    return dict(engine=engine, renderer=renderer, command=command, sha256=sha(exe), exit=exit_code,
                forced=forced, seconds=round(time.monotonic() - began, 1),
                image=str(images[0].relative_to(folder.parent)) if len(images) == 1 else None,
                images=len(images), viewpos=[int(v) for v in found[-1]] if found else None)


def compare(reference, other):
    from PIL import Image, ImageChops, ImageStat
    a = Image.open(reference).convert('RGB')
    b = Image.open(other).convert('RGB')
    if a.size != b.size:
        return dict(sizes=[a.size, b.size])
    difference = ImageChops.difference(a, b)
    mean = [round(value, 3) for value in ImageStat.Stat(difference).mean]
    largest = difference.split()
    peak = ImageChops.lighter(ImageChops.lighter(largest[0], largest[1]), largest[2])
    over = sum(peak.histogram()[17:])
    return dict(meanAbsolute=mean, over16=round(over / (a.size[0] * a.size[1]), 4))


def result_problems(results, installation_unchanged):
    """Do not report a failed capture or a different camera as a comparison."""
    problems = [] if installation_unchanged else ['installation changed']
    for scene in results:
        reference_view = scene['runs'][0]['viewpos']
        for run in scene['runs']:
            label = f"{scene['scene']} {run['engine']} {run['renderer'] or 'retail'}"
            if run['exit'] != 0 or run['forced']:
                problems.append(f'{label}: did not exit normally')
            if run['images'] != 1 or not run['image']:
                problems.append(f'{label}: expected exactly one engine screenshot')
            if run['viewpos'] is None or run['viewpos'] != reference_view:
                problems.append(f'{label}: missing or mismatched view position')
            if 'sizes' in run.get('againstRetail', {}):
                problems.append(f'{label}: screenshot dimensions differ')
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True, help='fnTech3 runtime folder')
    parser.add_argument('--scenes', nargs='+', default=[s[0] for s in SCENES], choices=[s[0] for s in SCENES])
    parser.add_argument('--renderers', nargs='+', default=['glx', 'vk', 'rtx'], choices=['glx', 'vk', 'rtx'])
    parser.add_argument('--retail-set', nargs=2, action='append', default=[], metavar=('CVAR', 'VALUE'),
                        help='a cvar for the second retail run only, to see what an image difference follows')
    parser.add_argument('--retail-all-set', nargs=2, action='append', default=[], metavar=('CVAR', 'VALUE'),
                        help='a cvar for both retail runs, e.g. r_ext_compress_textures 0 with the same for fnTech3')
    parser.add_argument('--matched', action='store_true',
                        help="run fnTech3 with the settings retail's first start leaves (MATCHED)")
    parser.add_argument('--fntech3-set', nargs=2, action='append', default=[], metavar=('CVAR', 'VALUE'),
                        help="a cvar for every fnTech3 run, e.g. r_hdr 0")
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Windows only')
    if not all((STOCK / 'BaseEF' / f'pak{n}.pk3').is_file() for n in range(4)):
        parser.error(f'{STOCK} must hold BaseEF/pak0-3.pk3 and the game libraries')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runtime = args.runtime.resolve(strict=True)
    layout = stage()
    before = inventory(INSTALLATION)
    results = []
    for label, level, view in [s for s in SCENES if s[0] in args.scenes]:
        retail = dict(args.retail_all_set)
        runs = [run(output / f'{label}-retail-1', 'retail', level, view, layout, runtime, None, retail),
                run(output / f'{label}-retail-2', 'retail', level, view, layout, runtime, None,
                    dict(retail, **dict(args.retail_set)))]
        extra = dict(MATCHED if args.matched else {}, **dict(args.fntech3_set))
        runs += [run(output / f'{label}-fntech3-{r}', 'fntech3', level, view, layout, runtime, r, extra)
                 for r in args.renderers]
        reference = runs[0]['image']
        for entry in runs[1:]:
            if reference and entry['image']:
                entry['againstRetail'] = compare(output / reference, output / entry['image'])
        result = dict(scene=label, map=level, view=view, runs=runs)
        results.append(result)
        print(json.dumps(dict(scene=label, viewpos=[r['viewpos'] for r in runs],
                              diffs=[r.get('againstRetail') for r in runs[1:]])), flush=True)
        (output / 'summary.json').write_text(json.dumps(dict(results=results), indent=1) + '\n', encoding='utf-8')
    summary = dict(results=results, installationUnchanged=before == inventory(INSTALLATION))
    summary['problems'] = result_problems(results, summary['installationUnchanged'])
    (output / 'summary.json').write_text(json.dumps(summary, indent=1) + '\n', encoding='utf-8')
    print('installationUnchanged', summary['installationUnchanged'])
    for problem in summary['problems']:
        print(problem, file=sys.stderr)
    return 1 if summary['problems'] else 0


if __name__ == '__main__':
    sys.exit(main())
