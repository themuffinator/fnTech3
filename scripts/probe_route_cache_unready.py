#!/usr/bin/env python3
"""Ask a dedicated server for a route cache dump before its routing exists.

The Quake III botlib writes its route caches to maps/NAME.rcd whenever the
game sets saveroutingcache (Quake III's and Quake Live's games do so for
`bot_saveroutingcache 1`, a cheat, so the map is loaded with devmap), from
AAS_StartFrame, before it has checked that routing is initialized. With
`bot_forcereachability 1` the botlib computes a map's reachability again over
many frames, and until that is done the areas are counted but the cache
tables the writer walks have not been made
(docs/fntech3/2026-09-29-route-cache-before-routing.md).

For each map below and each server given (--server LABEL=EXE, built alike
but for the writer), the dedicated server loads the map with bots on and
reachability forced, asks for a dump while reachability is still computed,
waits, lists the clients and quits. Each run reports whether the server
faulted, whether it printed that no dump was written, whether any dump
landed in the home, and whether the level went on to finish reachability's
first steps. Windows only: private desktops, networking off, console
commands only. Installations are only read.

  --installation KEY=PATH  q3 or ql, as for probe_retail_map_loads.py
"""
from pathlib import Path
import argparse
import json
import os
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
from probe_retail_map_loads import sha256  # noqa: E402
from probe_route_cache_loads import COLOR, DONE, MAPS, play  # noqa: E402

CASES = ('q3:q3dm7', 'ql:campgrounds')
NOT_WRITTEN = 'Warning: no route cache written: routing is not initialized'
CALCULATING = 'calculating reachability...'
PERCENT = re.compile(r'(\d+\.\d)%')
# The exception filter's report. Reachability prints its progress without a
# newline, so the error banner can start mid-line, where the shared harness's
# banner pattern does not look.
FAULTED = re.compile(r'ERROR: Unhandled exception caught\s*Exception Code: (\S+)\s*Exception Address: (\S+)')


def script(case, waits):
    _, _, _, _, load, _ = case
    # The game reads bot_forcereachability once, when it sets the botlib up at
    # the map's load; the dump is asked for while reachability is computed.
    return ['set bot_forcereachability 1', load, f'wait {waits}', 'set bot_saveroutingcache 1',
            f'wait {waits}', 'status', f'echo {DONE}', 'quit']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--server', action='append', required=True, help='LABEL=dedicated executable')
    parser.add_argument('--installation', action='append', required=True, help='KEY=PATH, q3 and ql')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--waits', type=int, default=40, help='server frames before and after the request')
    parser.add_argument('--timeout', type=float, default=30)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    servers = dict(item.split('=', 1) for item in args.server)
    servers = {label: Path(exe).resolve(strict=True) for label, exe in servers.items()}
    installations = dict(item.split('=', 1) for item in args.installation)
    installations = {key: Path(path).resolve(strict=True) for key, path in installations.items()}
    output = args.output.resolve()
    (output / 'logs').mkdir(parents=True, exist_ok=False)
    report = dict(servers={label: dict(path=str(exe), sha256=sha256(exe)) for label, exe in servers.items()},
                  installations={key: str(path) for key, path in installations.items()},
                  waits=args.waits, runs={})
    for label in CASES:
        case = MAPS[label]
        installation = installations[case[0]]
        lines = script(case, args.waits)
        for server, exe in servers.items():
            slug = f'{server}-{label.replace(":", "-").replace("/", "-")}'
            home = output / 'homes' / slug
            result, dump = play(exe, case, installation, home, output, slug, lines, args.timeout)
            text = COLOR.sub('', (output / 'logs' / f'{slug}-console.log').read_text(encoding='latin-1'))
            percents = [float(p) for p in PERCENT.findall(text)]
            fault = FAULTED.search(text)
            if fault:
                # The level ended there, and with it the script's quit: the
                # server idles until the harness stops it.
                result['outcome'] = 'faulted'
                result['errors'] = ['Unhandled exception caught: %s at %s' % fault.groups()]
            result.update(
                script=lines,
                calculating=CALCULATING in text,
                reachabilityPercentReached=max(percents) if percents else None,
                notWritten=text.count(NOT_WRITTEN),
                dumpInHome=None if dump is None else len(dump),
                exeUnchanged=sha256(exe) == report['servers'][server]['sha256'])
            report['runs'][slug] = result
            print(slug, result['outcome'], result['errors'][:1], 'notWritten', result['notWritten'],
                  'dump', result['dumpInHome'], 'reach%', result['reachabilityPercentReached'], flush=True)
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
