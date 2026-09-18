#!/usr/bin/env python3
"""Compare what changes on screen while a Q3 demo is paused, retail versus fnTech3.

Both retail quake3.exe and the fnTech3 client (q3 profile) play the same demo
from the installation's own paks (default `four`, shipped as baseq3/pak6.pk3
demos/four.dm_66 and the identical pak8.pk3 demos/four.dm_68),
freeze it with cl_freezeDemo, take two engine screenshots 90 frames apart, and
resume. The probe reports, for each engine, how many pixels differ between the
two paused frames and the bounding box of those pixels, so a difference confined
to the same HUD element in both engines can be recognised as game-module
behaviour rather than an engine defect.

Windows only: windowed on unswitched private desktops with isolated homes, sound
and mouse/joystick input disabled, console commands only; no injected input and
no operating-system capture. The installation is read-only input.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys
import time

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def run(command, cwd, stdout, timeout):
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, cwd, stdout)
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            time.sleep(0.1)
        forced = process.poll() is None
        return process.poll(), forced, round(time.monotonic() - begin, 3)
    finally:
        process.close()


def difference(first, second):
    a, b = Image.open(first).convert('RGB'), Image.open(second).convert('RGB')
    if a.size != b.size:
        return dict(sizeMismatch=[a.size, b.size])
    delta = ImageChops.difference(a, b)
    box = delta.getbbox()
    changed = sum(1 for pixel in delta.getdata() if pixel != (0, 0, 0))
    return dict(size=list(a.size), changedPixels=changed, box=list(box) if box else None,
                boxFraction=[round(v / (a.size[i % 2]), 4) for i, v in enumerate(box)] if box else None)


def engine(label, command, cwd, home, game, output, pattern, timeout=90):
    code, forced, elapsed = run(command, cwd, output / f'{label}-stdout.log', timeout)
    logs = sorted(home.rglob('*console.log')) + sorted(home.rglob('qconsole.log'))
    text = logs[0].read_text(encoding='utf-8', errors='replace') if logs else ''
    (output / f'{label}-console.log').write_text(text, encoding='utf-8')
    shots = []
    for capture in sorted(home.rglob(pattern)):
        target = output / f'{label}-{capture.name}'
        target.write_bytes(capture.read_bytes())
        shots.append(target)
    row = dict(command=command, exitCode=code, forcedTermination=forced, elapsedSeconds=elapsed,
               screenshots=[dict(name=s.name, sha256=sha(s)) for s in shots])
    if len(shots) >= 2:
        row['pausedDifference'] = difference(shots[0], shots[1])
    if len(shots) >= 3:
        row['resumedDifference'] = difference(shots[1], shots[2])
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True, help='fnTech3 client executable')
    parser.add_argument('--installation', type=Path, required=True, help='Quake III Arena root with quake3.exe')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--demo', default='four', help='demo name inside the installation paks')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client, installation = args.client.resolve(strict=True), args.installation.resolve(strict=True)
    retail = installation / 'quake3.exe'
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    before = inventory(installation)
    # Retail appends activeAction without a newline; end the action with ';'.
    # con_notifytime 0 keeps each capture's own console notice off the next one.
    pause = 'wait 30; set cl_freezeDemo 1; wait 5; {a}; wait 90; {b}; wait 3; set cl_freezeDemo 0; wait 60; {c};'
    results = {}
    home = output / 'retail-home'
    (home / 'baseq3').mkdir(parents=True)
    command = [str(retail)]
    for name, value in dict(fs_basepath=str(installation), fs_homepath=str(home), fs_game='',
                            r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                            s_initsound='0', logfile='2', com_maxfps='60', cg_lagometer='1', con_notifytime='0',
                            nextdemo='quit', activeAction=pause.format(a='screenshot', b='screenshot', c='screenshot')).items():
        command += ['+set', name, value]
    command += ['+demo', args.demo]
    results['retail'] = engine('retail', command, installation, home, 'baseq3', output, 'shot*.tga')
    home = output / 'fntech3-home'
    (home / 'baseq3').mkdir(parents=True)
    command = [str(client)]
    for name, value in dict(com_gameProfile='q3', fs_basepath=str(installation), fs_homepath=str(home),
                            fs_game='', r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0',
                            in_nograb='1', s_initsound='0', logfile='2', com_maxfps='60',
                            com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                            cl_renderer='glx', cg_lagometer='1', con_notifytime='0', cl_quitOnDemoCompleted='1',
                            activeAction=pause.format(a='screenshotPNG paused-a', b='screenshotPNG paused-b',
                                                              c='screenshotPNG paused-c')).items():
        command += ['+set', name, value]
    command += ['+demo', args.demo]
    results['fntech3'] = engine('fntech3', command, client.parent, home, 'baseq3', output, 'paused*.png')
    report = dict(demo=args.demo, retailSha256=sha(retail), clientSha256=sha(client),
                  installationUnchanged=before == inventory(installation), results=results,
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip())
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: {a: b for a, b in v.items() if a != 'command'} if isinstance(v, dict) and 'exitCode' in v else v
                      for k, v in report.items() if k != 'results'}, indent=2))
    for label, row in results.items():
        print(label, row['exitCode'], row['forcedTermination'], len(row['screenshots']),
              row.get('pausedDifference'), row.get('resumedDifference', {}).get('changedPixels'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
