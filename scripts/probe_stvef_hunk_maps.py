#!/usr/bin/env python3
"""Check every installed Elite Force map at the profile's minimum client hunk.

Windows only. Uses the stable fnTech3 client on private, unswitched desktops,
windowed with mouse and joystick input disabled. Sound defaults to disabled;
enabled backends are explicitly muted. Configurations, logs,
native caches and engine screenshots stay in project-local isolated homes.
Retail installations are read-only. No input is injected or captured.
This proves bounded map startup, not campaign traversal or gameplay parity.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess
from probe_retail_map_loads import cases

MARKER = 'STVEF_HUNK_MAP_READY'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): [p.stat().st_size, p.stat().st_mtime_ns]
            for p in installation.rglob('*') if p.is_file()}


def log_fields(text):
    totals = re.findall(r'^\s*(\d+) bytes total hunk', text, re.M)
    size = int(totals[-1]) if totals else None
    zones = re.findall(r'^\s*(\d+) bytes total main zone', text, re.M)
    return dict(firstSnapshot='CL_FirstSnapshot:' in text, completed=MARKER in text,
                errors=re.findall(r'^(?:ERROR:|recursive error after:)(.*)$', text, re.M)
                + re.findall(r'^----- Client Shutdown \(Server fatal crashed: (.*?)\) -----', text, re.M),
                hunkBytes=size, hunkMiB=size // (1024 * 1024) if size is not None else None,
                zoneBytes=int(zones[-1]) if zones else None)


def passes(result):
    minimum = result['expectedHunkMiB']
    return bool(result['exitCode'] == 0 and not result['forced'] and result['completed']
                and result['firstSnapshot'] and result['hunkBytes'] == minimum * 1024 * 1024
                and (not result.get('minimumZoneMiB') or
                     (result['zoneBytes'] is not None and result['zoneBytes'] >= result['minimumZoneMiB'] * 1024 * 1024))
                and not result['errors'] and len(result['screenshots']) == 1)


def recheck(output):
    output = output.resolve(strict=True)
    if not output.is_relative_to(ROOT) or output == ROOT:
        raise ValueError('recheck output must be inside the active project')
    path = output / 'result.json'
    report = json.loads(path.read_text(encoding='utf-8'))
    original = output / 'first-validation.json'
    if not original.exists():
        original.write_bytes(path.read_bytes())
    for result in report['maps'].values():
        expected = report['expectedHunkMiB']
        result['expectedHunkMiB'] = expected[result['profile']] if isinstance(expected, dict) else expected
        log = Path(result['log']).resolve(strict=True)
        if not log.is_relative_to(output) or sha(log) != result['logSha256']:
            raise ValueError('stored log is outside the output or has changed')
        for shot in result['screenshots']:
            image = Path(shot['path']).resolve(strict=True)
            if not image.is_relative_to(output) or sha(image) != shot['sha256']:
                raise ValueError('stored engine screenshot is outside the output or has changed')
        result.update(log_fields(log.read_text(errors='replace')))
        result['passed'] = passes(result)
        (log.parents[2] / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    report['passed'] = sum(r['passed'] for r in report['maps'].values())
    report['failed'] = sum(not r['passed'] for r in report['maps'].values())
    report['validationNote'] = 'Re-evaluated unchanged logs/screenshots with bytes-based meminfo and fatal-shutdown parsing; no runtime rerun.'
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"{report['passed']} passed, {report['failed']} failed")
    return 0 if not report['failed'] and report['installationUnchanged'] and report['binariesUnchanged'] else 1


def probe(case, args):
    label = re.sub(r'[^A-Za-z0-9_.-]', '_', case['label'])
    output = args.output / label
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    game = home / 'baseEF'
    game.mkdir(parents=True)
    for name in ('efconfig.cfg', 'hmconfig.cfg'):
        (game / name).write_text('', encoding='ascii')
    steps = f'wait 100; meminfo; screenshotPNG hunk-map; echo {MARKER}; wait 4; quit'
    cfg = f'set activeAction "{steps}"\nmap {case["name"]}\n'
    (game / 'hunk-probe.cfg').write_text(cfg, encoding='ascii')
    values = dict(com_gameProfile=case['profile'], fs_basepath=str(args.installation), fs_homepath=str(home),
                  com_hunkMegs=str(args.hunk_mib), r_fullscreen='0', r_mode='-1',
                  r_customWidth=str(args.width), r_customHeight=str(args.height),
                  in_mouse='0', in_joystick='0', in_nograb='1',
                  s_initsound='0' if args.sound_backend == 'off' else '1',
                  s_backend='legacy' if args.sound_backend == 'legacy' else 'openal',
                  s_volume='0', s_musicvolume='0', s_alDriver='OpenAL32.dll', cl_renderer=args.renderer,
                  net_enabled='0', net_ip='127.0.0.1', net_port='0', com_steamIntegration='0',
                  fs_skipWorkshop='1', cl_webuiEnable='0', cl_allowDownload='0', com_introplayed='1',
                  logfile='2', developer='1', sv_pure='0', bot_enable='1')
    command = [str(args.client)]
    for key, value in values.items():
        command += ['+set', key, value]
    command += ['+exec', 'hunk-probe.cfg']
    log = game / 'qconsole.log'
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, args.client.parent, output / 'stdout.log')
    try:
        while process.poll() is None and time.monotonic() - begin < args.timeout:
            if log.exists():
                current = log.read_text(errors='replace')
                if 'recursive error after:' in current or 'Server fatal crashed:' in current:
                    break
            time.sleep(.2)
        forced = process.poll() is None
        if forced:
            process.terminate()
        code = process.wait(5)
    finally:
        process.close()
    text = log.read_text(errors='replace') if log.exists() else ''
    shots = sorted(game.glob('screenshots/*.png'))
    result = dict(profile=case['profile'], map=case['name'], command=command, config=cfg,
                  expectedHunkMiB=max(args.hunk_mib, 160 if case['profile'] == 'stvef-sp' else 128),
                  minimumZoneMiB=96 if case['profile'] == 'stvef-sp' else 1,
                  exitCode=code, forced=forced, elapsedSeconds=round(time.monotonic() - begin, 3),
                  **log_fields(text),
                  log=str(log), logSha256=sha(log) if log.exists() else None,
                  screenshots=[dict(path=str(p), bytes=p.stat().st_size, sha256=sha(p)) for p in shots])
    result['passed'] = passes(result)
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path)
    parser.add_argument('--installation', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--recheck-output', type=Path, help='re-evaluate retained logs without launching a game')
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), default='vk')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--timeout', type=float, default=75)
    parser.add_argument('--case', action='append', help='select a profile:map label; repeat to select several')
    parser.add_argument('--hunk-mib', type=int, default=36, help='requested hunk; the profile minimum still applies')
    parser.add_argument('--width', type=int, default=800)
    parser.add_argument('--height', type=int, default=600)
    parser.add_argument('--sound-backend', choices=('off', 'openal', 'legacy'), default='off',
                        help='sound backend; enabled backends initialize with effects/music muted')
    args = parser.parse_args()
    if args.recheck_output:
        return recheck(args.recheck_output)
    if os.name != 'nt':
        parser.error('requires the Windows private-desktop harness')
    if not args.client or not args.installation or not args.output:
        parser.error('--client, --installation and --output are required')
    args.client = args.client.resolve(strict=True)
    args.installation = args.installation.resolve(strict=True)
    args.output = args.output.resolve()
    if not args.output.is_relative_to(ROOT) or args.output == ROOT:
        parser.error('output must be a fresh directory inside the active project')
    args.output.mkdir(parents=True, exist_ok=False)
    todo = cases({'stvef': args.installation})
    if args.case:
        selected = set(args.case)
        available = {case['label'] for case in todo}
        if selected - available:
            parser.error('unknown cases: ' + ', '.join(sorted(selected - available)))
        todo = [case for case in todo if case['label'] in selected]
    before = inventory(args.installation)
    watched = [args.client, args.client.parent / f'fnql_{args.renderer}_x86.dll']
    binaries = {str(p): sha(p) for p in watched}
    results, lock = {}, threading.Lock()
    started = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())

    def one(case):
        result = probe(case, args)
        with lock:
            results[case['label']] = result
            with (args.output / 'rows.jsonl').open('a', encoding='utf-8') as journal:
                journal.write(json.dumps(dict(label=case['label'], result=result)) + '\n')
            print(case['label'], 'PASS' if result['passed'] else 'FAIL', result['hunkMiB'],
                  result['exitCode'], result['elapsedSeconds'], result['errors'][:1], flush=True)

    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        list(pool.map(one, todo))
    report = dict(sourceCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  renderer=args.renderer, requestedHunkMiB=args.hunk_mib,
                  width=args.width, height=args.height, soundBackend=args.sound_backend,
                  expectedHunkMiB={profile: max(args.hunk_mib, floor) for profile, floor in
                                   [('stvef-sp', 160), ('stvef-mp', 128)]},
                  startedUtc=started, endedUtc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                  binaries=binaries, binariesUnchanged=binaries == {str(p): sha(p) for p in watched},
                  installation=str(args.installation), installationUnchanged=before == inventory(args.installation),
                  passed=sum(r['passed'] for r in results.values()), failed=sum(not r['passed'] for r in results.values()),
                  maps=dict(sorted(results.items())))
    (args.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"{report['passed']} passed, {report['failed']} failed", flush=True)
    return 0 if (not report['failed'] and report['installationUnchanged'] and report['binariesUnchanged']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
