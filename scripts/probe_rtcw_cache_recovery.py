#!/usr/bin/env python3
"""Recover damaged SP navigation caches and exchange them with retail RTCW.

Windows only. Dedicated, windowed processes use unswitched private desktops and
isolated homes. No input automation or screen capture. Installed assets remain
read-only; generated caches and native module copies are removed after the run.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tests'))
from windows_private_process import WindowsPrivateProcess


def sha(data):
    return hashlib.sha256(data).hexdigest()


def inventory(folder):
    return {str(p.relative_to(folder)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in folder.rglob('*') if p.is_file()}


def remove_home(home, output):
    home.resolve(strict=True).relative_to(output.resolve(strict=True))
    for path in [home, *home.rglob('*')]:
        if path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
            raise RuntimeError(f'Refusing cleanup through a link: {path}')
    shutil.rmtree(home)


def run(engine, installation, output, map_name, label, cache_input=None):
    folder = output/map_name/label
    game = folder/'home/main'
    (game/'maps').mkdir(parents=True, exist_ok=False)
    names = [f'{map_name}_b{world}.rcd' for world in range(2)]
    for name in names:
        (game/'maps'/name).write_bytes(cache_input[name] if cache_input else bytes(36))
    retail = label.startswith('retail')
    settings = dict(fs_basepath=str(installation), fs_homepath=str(game.parent), fs_game='',
                    dedicated='1', developer='1', logfile='4', fs_debug='1',
                    r_fullscreen='0', in_mouse='0', in_joystick='0', in_nograb='1',
                    net_ip='127.0.0.1', net_port='0', sv_pure='0', com_viewlog='0', s_initsound='0',
                    **{f'sv_master{i}': '' for i in range(1, 6)})
    if not retail:
        settings['com_gameProfile'] = 'rtcw-sp'
    command = [str(engine)]
    for key, value in settings.items():
        command += ['+set', key, value]
    command += ['+spdevmap', map_name, '+wait', '120', '+quit']
    start = time.monotonic()
    process = None
    result = dict(label=label, command=command, executableSha256=sha(engine.read_bytes()), passed=False)
    try:
        process = WindowsPrivateProcess(command, engine.parent, folder/'stdout.log')
        while process.poll() is None and time.monotonic()-start < 180:
            time.sleep(.2)
        result['exitCode'] = process.poll()
    finally:
        if process:
            process.close()
    result['seconds'] = round(time.monotonic()-start, 3)
    logfile = game/('rtcwconsole.log' if retail else 'qconsole.log')
    text = logfile.read_text(encoding='utf-8', errors='replace') if logfile.exists() else ''
    (folder/'console.log').write_text(text, encoding='utf-8')
    caches = {name: (game/'maps'/name).read_bytes() for name in names}
    result['caches'] = [dict(name=name, bytes=len(data), sha256=sha(data),
                            header=list(struct.unpack_from('<9I', data))) for name, data in caches.items()]
    result['initializedWorlds'] = text.count('AAS initialized.')
    result['generatedWorlds'] = text.count('AAS_CreateAllRoutingCache' if retail else 'Generating RTCW routing cache:')
    result['cacheInputsUnchanged'] = cache_input is not None and caches == cache_input
    result['cacheReads'] = [line for line in text.splitlines() if '.rcd' in line]
    result['errors'] = re.findall(r'^.*(?:ERROR:|Server crashed|recursive error).*$', text, re.M)
    result['passed'] = (result.get('exitCode') == 0 and result['initializedWorlds'] == 2
                        and not result['errors'] and all(len(data) > 36 for data in caches.values())
                        and (result['generatedWorlds'] == 0 and result['cacheInputsUnchanged']
                             if cache_input else result['generatedWorlds'] == 2))
    (folder/'result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    remove_home(game.parent, output)
    print(f'{map_name} {label}: {"PASS" if result["passed"] else "FAIL"} ({result["seconds"]} s)', flush=True)
    return result, caches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--maps', nargs='+', default=['escape1', 'baseout', 'boss1'])
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows is required')
    if any(not re.fullmatch(r'[a-zA-Z0-9_-]+', name) for name in args.maps):
        parser.error('Map names must be simple installed SP map names')
    output = args.output.resolve()
    output.relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=False)
    installation = args.installation.resolve(strict=True)
    server = args.server.resolve(strict=True)
    retail = (installation/'WolfSP.exe').resolve(strict=True)
    before = inventory(installation)
    runs = []
    for name in args.maps:
        result, generated = run(server, installation, output, name, 'fntech3-cold')
        runs.append(result)
        if not result['passed']:
            break
        for engine, label in [(server, 'fntech3-reload'), (retail, 'retail-read')]:
            result, _ = run(engine, installation, output, name, label, generated)
            runs.append(result)
        result, generated = run(retail, installation, output, name, 'retail-cold')
        runs.append(result)
        if result['passed']:
            result, _ = run(server, installation, output, name, 'fntech3-read-retail', generated)
            runs.append(result)
    unchanged = before == inventory(installation)
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  evidence='SP dedicated map initialization and bidirectional RCD15 interchange; no gameplay claim',
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  worktreeDirty=bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True)),
                  installation=str(installation), installationUnchanged=unchanged,
                  moduleSha256=sha((installation/'qagamex86.dll').read_bytes()),
                  runs=runs, passed=unchanged and len(runs)==len(args.maps)*5 and all(r['passed'] for r in runs))
    (output/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
