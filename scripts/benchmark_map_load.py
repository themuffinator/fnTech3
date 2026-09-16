#!/usr/bin/env python3
r"""Time devmap -> active snapshot in an isolated, windowed client.

Example:
  python scripts/benchmark_map_load.py --exe build/fntech3.exe --profile ql \
      --basepath "C:/Games/Quake Live" --map mcsarges --factory ffa --renderer vk

Each process measures its initial map load and subsequent devmap reloads.
No keyboard/mouse automation or installation writes are used. Engine screenshots,
console logs, commands, binary hashes and observed timings remain in --output.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--basepath', type=Path, required=True)
    parser.add_argument('--profile', choices=('q3', 'ql'), required=True)
    parser.add_argument('--map', required=True)
    parser.add_argument('--factory', default='ffa')
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), default='vk')
    parser.add_argument('--loads', type=int, default=3)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--timeout', type=float, default=180)
    parser.add_argument('--settings-from', type=Path, help='Copy only rendering/audio/cgame cvars from this profile home')
    parser.add_argument('--with-services', action='store_true')
    parser.add_argument('--steam-provider', type=Path)
    args = parser.parse_args()
    if not 1 <= args.loads <= 50 or args.timeout <= 0:
        parser.error('--loads must be 1..50 and --timeout must be positive')
    if not re.fullmatch(r'[A-Za-z0-9_/-]{1,48}', args.map) or not re.fullmatch(r'[A-Za-z0-9_-]+', args.factory):
        parser.error('map and factory must be engine names without console-command syntax')
    exe, base = args.exe.resolve(strict=True), args.basepath.resolve(strict=True)
    if not exe.is_file() or not base.is_dir():
        parser.error('--exe must be a file and --basepath an installation directory')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    output = (args.output or ROOT / '.tmp' / ('map-load-' + stamp)).resolve()
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    game_home = home / 'baseq3'
    game_home.mkdir(parents=True)
    settings = {
        'com_gameProfile': args.profile, 'fs_basepath': str(base), 'fs_homepath': str(home),
        'r_fullscreen': '0', 'r_mode': '-1', 'r_customwidth': '1280', 'r_customheight': '720',
        'cl_renderer': args.renderer, 'in_mouse': '0', 'in_nograb': '1', 'in_joystick': '0',
        's_volume': '0', 's_musicvolume': '0', 's_initsound': '1',
        'com_steamIntegration': str(int(args.with_services)), 'cl_webuiEnable': str(int(args.with_services)),
        'logfile': '2', 'developer': '1', 'net_ip': '127.0.0.1', 'net_port': '27983',
        'net_enabled': '1', 'sv_pure': '1', 'com_introplayed': '1',
        'activeAction': 'exec load-step-1.cfg',
    }
    if args.steam_provider:
        settings['com_steamProvider'] = str(args.steam_provider.resolve(strict=True))
    if args.settings_from:
        names = ('qzconfig.cfg', 'repconfig.cfg') if args.profile == 'ql' else ('q3config.cfg',)
        for name in names:
            source = args.settings_from.resolve(strict=True) / name
            if not source.exists():
                continue
            pairs = re.findall(r'^seta ((?:r_|s_|cg_)[^ ]+) "([^"\n]*)"', source.read_text(), re.M)
            lines = [f'seta {key} "{value}"' for key, value in pairs if key not in settings]
            config_home = home if args.profile == 'ql' else game_home
            (config_home / name).write_text('\n'.join(lines) + '\n')
    map_command = f'devmap {args.map}' + (f' {args.factory}' if args.profile == 'ql' else '')
    for index in range(1, args.loads + 1):
        lines = [f'echo LOAD_PROBE_ACTIVE {index}', 'wait 8', f'screenshotPNG load-{index}', 'wait 8']
        if index < args.loads:
            lines += [f'set activeAction "exec load-step-{index + 1}.cfg"',
                      f'echo LOAD_PROBE_MAP_REQUEST {index + 1}', map_command]
        else:
            lines += ['quit']
        (game_home / f'load-step-{index}.cfg').write_text('\n'.join(lines) + '\n')
    command = [str(exe)]
    for key, value in settings.items():
        command += ['+set', key, value]
    command += ['+echo', 'LOAD_PROBE_MAP_REQUEST 1', '+devmap', args.map]
    if args.profile == 'ql':
        command += [args.factory]
    result = {'command': command, 'map_command': map_command, 'profile': args.profile,
              'renderer': args.renderer, 'started_utc': stamp, 'loads': [], 'events': []}
    result['binary_sha256'] = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (exe, exe.parent / f'fnql_{args.renderer}_x86.dll', exe.parent / 'FnQL-pkg.fnz')
        if path.is_file()
    }
    (output / 'command.json').write_text(json.dumps(command, indent=2))
    start = time.perf_counter()
    process = subprocess.Popen(command, cwd=exe.parent, stdin=subprocess.DEVNULL,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    offset, pending, current = 0, '', None
    try:
        while True:
            elapsed = time.perf_counter() - start
            log = game_home / 'qconsole.log'
            if log.exists():
                with log.open('rb') as stream:
                    stream.seek(offset)
                    data = stream.read()
                    offset += len(data)
                pending += data.decode('utf-8', errors='replace')
                lines = pending.split('\n')
                pending = lines.pop()
                for line in lines:
                    line = line.rstrip()
                    result['events'].append({'seconds': round(elapsed, 4), 'line': line})
                    if line.startswith('LOAD_PROBE_MAP_REQUEST'):
                        current = {'request_seconds': elapsed}
                    if current is not None:
                        match = re.search(r'CL_InitCGame:\s+([0-9.]+) seconds', line)
                        if match:
                            current['cgame_ms'] = round(float(match[1]) * 1000)
                        match = re.search(r'snapshot wait (\d+) ms', line)
                        if match:
                            current['snapshot_wait_ms'] = int(match[1])
                        if line.startswith('LOAD_PROBE_ACTIVE'):
                            current['map_to_active_ms'] = round((elapsed - current['request_seconds']) * 1000)
                            result['loads'].append(current)
                            print(f'Load {len(result["loads"])}: {current["map_to_active_ms"]} ms', flush=True)
                            current = None
            if process.poll() is not None:
                break
            if elapsed >= args.timeout:
                result['timed_out'] = True
                process.terminate()
                break
            time.sleep(0.01)
    finally:
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)
        result['exit_code'] = process.returncode
        result['elapsed_seconds'] = time.perf_counter() - start
        (output / 'observed.json').write_text(json.dumps(result, indent=2))
    print(output)
    return int(process.returncode != 0 or len(result['loads']) != args.loads)


if __name__ == '__main__':
    raise SystemExit(main())
