#!/usr/bin/env python3
"""Probe supplied IHV maps using retail Q3 modules in fnTech3 (Windows).

This is an asset probe, not IHV native-module, gameplay or protocol support.
The supplied installation stays read-only. Each run has an independent writable
home on an unswitched private desktop, windowed, with input disabled. Captures
use the engine's screenshot command. No proprietary inputs enter source control.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys

from probe_demo_roundtrip import fntech3_command, run

ROOT = Path(__file__).resolve().parents[1]
MAPS = ('ihv_test1', 'km_portal', 'test_fog2', 'tim_dm1')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inside(path, parent):
    return path == parent or parent in path.parents


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True, help='IHV ref/install directory')
    parser.add_argument('--retail', type=Path, required=True, help='Legitimate complete retail Q3 installation')
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), default='glx')
    parser.add_argument('--map', choices=MAPS, action='append', dest='maps')
    parser.add_argument('--capture-frame', type=int, action='append', dest='frames',
                        help='Capture after this many client frames; repeat for an animation sequence')
    parser.add_argument('--timeout', type=float, default=45)
    args = parser.parse_args()
    frames = sorted(set(args.frames or [30]))
    if frames[0] < 1 or frames[-1] > 36000:
        parser.error('Capture frames must be between 1 and 36000')
    for key in ('installation', 'retail', 'client', 'output'):
        setattr(args, key, getattr(args, key).resolve())
    if not inside(args.output, ROOT / '.tmp'):
        parser.error('Output must be a new directory under the project .tmp directory')
    for source in (args.installation, args.retail, args.client.parent):
        if inside(args.output, source) or inside(source, args.output):
            parser.error('Output and source paths must be disjoint')
    if args.output.exists():
        parser.error('Use a fresh output directory; existing evidence is never overwritten')
    if not (args.retail / 'baseq3/pak0.pk3').is_file():
        parser.error('Retail installation must contain baseq3/pak0.pk3')
    args.output.mkdir(parents=True)
    source = args.installation / 'baseq3'
    report = dict(scope='IHV asset load with retail Q3 QVMs; not IHV gameplay',
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  client_sha256=sha(args.client), renderer=args.renderer,
                  retail_paks={p.name: sha(p) for p in sorted((args.retail / 'baseq3').glob('pak*.pk3'))},
                  installation=str(args.installation), runs=[])
    # Rendering changes need the loaded module's identity as well as the client.
    report['renderer_modules'] = {
        p.name: sha(p) for p in sorted(args.client.parent.glob(f'*_{args.renderer}_*.dll'))}
    for name in args.maps or MAPS:
        output = args.output / name
        game = output / 'home/ihv'
        game.mkdir(parents=True)
        # No startup configs, native modules, demos or screenshots from the
        # historical installation can be selected accidentally by this probe.
        for directory in ('maps', 'models', 'textures', 'scripts', 'sound', 'gfx', 'icons', 'env', 'sprites'):
            if (source / directory).is_dir():
                shutil.copytree(source / directory, game / directory)
        actions, previous = [], 0
        for frame in frames:
            filename = 'ihv-map' if len(frames) == 1 else f'ihv-map-{frame:05d}'
            actions.append(f'wait {frame-previous}; screenshotPNG {filename};')
            previous = frame
        actions.append('wait 10; echo IHV_MAP_COMPLETE; quit;')
        (game / 'probe.cfg').write_text(
            'set com_introplayed 1\nset cl_motd 0\nset sv_pure 0\nset bot_enable 0\n'
            'set com_maxfps 60\nset com_maxfpsUnfocused 60\nset com_maxfpsMinimized 60\n'
            'set cl_allowDownload 0\nset cg_draw2D 0\n'
            'set activeAction "' + ' '.join(actions) + '"\n'
            f'devmap {name}\n', encoding='utf-8')
        cmd = fntech3_command(args.client, args.retail, output / 'home', 'q3', ['+exec', 'probe.cfg'])
        cmd[cmd.index('fs_game') + 1] = 'ihv'
        cmd[cmd.index('cl_renderer') + 1] = args.renderer
        code, forced, elapsed, log = run(cmd, args.client.parent, output / 'stdout.log',
                                         game / 'qconsole.log', args.timeout)
        (output / 'console.log').write_text(log, encoding='utf-8')
        captures = [dict(path=str(p.relative_to(output)), sha256=sha(p))
                    for p in sorted(game.glob('screenshots/ihv-map*.png'))]
        errors = re.findall(r'^.*(?:ERROR:|Error:|could not translate IBSP|recursive error).*$', log, re.M)
        row = dict(map=name, map_sha256=sha(source / f'maps/{name}.bsp'), command=cmd,
                   capture_frames=frames,
                   exit_code=code, forced=forced, elapsed=elapsed, screenshots=captures, errors=errors,
                   warnings=re.findall(r'^.*WARNING:.*$', log, re.M))
        row['passed'] = (code == 0 and not forced and len(captures) == len(frames)
                         and bool(re.search(r'^IHV_MAP_COMPLETE\s*$', log, re.M)) and not errors)
        report['runs'].append(row)
        report['passed'] = all(r['passed'] for r in report['runs'])
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(name, 'PASS' if row['passed'] else 'FAIL', flush=True)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
