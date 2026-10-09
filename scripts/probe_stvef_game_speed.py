#!/usr/bin/env python3
"""How fast a dedicated Holomatch server's game runs under a timescale.

For each server (retail ``stvoyHM.exe`` or fnTech3's) and each timescale,
the server starts ``hm_borg1`` with `devmap` at that timescale and no
players; then a bot is added over rcon every ``--interval`` seconds of wall
time, and the game time of each bot's ``ClientConnect`` line is read from the
game log (to the second). The run reports game seconds per wall second
between the first bot and the last.

Windows only. Every server is bound to 127.0.0.1 with an isolated writable
folder under ``--output``; retail reads a copy of the retail paks
(``.tmp/runtime/stvef-retail``), as every Elite Force probe does, and the
installation is compared before and after. No client takes part, and no
input is involved.
"""
from pathlib import Path
import argparse
import json
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'scripts'))
import probe_stvef_modes as modes  # noqa: E402
from probe_legacy_discovery import INSTALLS, stage_retail  # noqa: E402
from probe_legacy_sessions import PROFILES, inventory, query, wait_for  # noqa: E402

BOTS = ('Chell', 'Chakotay', 'Foster', 'Gamma', 'Munro', 'Biessman')


def connects(game_log):
    """Each client slot's first ClientConnect, in seconds of game time."""
    found = {}
    for minutes, seconds, slot in re.findall(r'(?m)^\s*(\d+):(\d\d) ClientConnect: (\d+)', game_log):
        found.setdefault(int(slot), int(minutes) * 60 + int(seconds))
    return found


def run(kind, runtime, output, layout, timescale, count, interval):
    modes.PLAY.update(timescale=timescale, timelimit='0')
    folder = output / f'{kind}-ts{timescale}'
    peer = modes.server(kind, runtime, folder, layout, ('speed', 'hm_borg1', 0, (), ()), late_bots=True)
    walls = []
    try:
        ready = wait_for(lambda: 'hm_borg1' in (query(peer.port, 'getinfo probe', timeout=1.0) or ''), 60, 0.5)
        if not ready:
            raise RuntimeError('server not ready')
        time.sleep(2.0)
        began = time.monotonic()
        for n in range(count):
            target = began + n * interval
            while time.monotonic() < target:
                time.sleep(0.05)
            (peer.cfg_dir / f'probebot{n}.cfg').write_text(f'addbot {BOTS[n]} 1\n', encoding='latin1')
            sent = time.monotonic()
            query(peer.port, f'rcon {peer.password} exec probebot{n}.cfg', timeout=1.5)
            walls.append(round(sent - began, 3))
            wait_for(lambda: len(connects(peer.game_log())) > n, 20, 0.2)
    finally:
        if peer.alive():
            query(peer.port, f'rcon {peer.password} quit', timeout=1.0)
        peer.stop(15)
        (folder / 'server-console.log').write_text(peer.log(), encoding='latin1')
        (folder / 'server-games.log').write_text(peer.game_log(), encoding='latin1')
    game = [connects(peer.game_log()).get(n) for n in range(count)]
    rate = None
    if len(game) >= 2 and game[0] is not None and game[-1] is not None:
        rate = round((game[-1] - game[0]) / (walls[-1] - walls[0]), 4)
    return dict(kind=kind, timescale=timescale, sha256=peer.sha256, wallSeconds=walls, gameSeconds=game,
                gamePerWall=rate, hitches=len(re.findall(r'Hitch warning', peer.log())))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True, help='fnTech3 runtime folder')
    parser.add_argument('--kinds', nargs='+', default=['retail', 'fntech3'], choices=['retail', 'fntech3'])
    parser.add_argument('--timescales', nargs='+', default=['0.1', '0.5', '3'])
    parser.add_argument('--stamps', type=int, default=3, help='bots added, at most six')
    parser.add_argument('--interval', type=float, default=60.0, help='wall seconds between bots')
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Windows only')
    if not 2 <= args.stamps <= len(BOTS):
        parser.error(f'--stamps must be 2 to {len(BOTS)}')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runtime = args.runtime.resolve(strict=True)
    installation = INSTALLS['stvef-mp']
    layout = stage_retail('stvef-mp', PROFILES['stvef-mp'], installation)
    before = inventory(installation)
    results = []
    for timescale in args.timescales:
        for kind in args.kinds:
            result = run(kind, runtime, output, layout, timescale, args.stamps, args.interval)
            results.append(result)
            print(json.dumps(result), flush=True)
            (output / 'summary.json').write_text(json.dumps(results, indent=1), encoding='utf-8')
    unchanged = before == inventory(installation)
    (output / 'summary.json').write_text(json.dumps(dict(results=results, installationUnchanged=unchanged),
                                                    indent=1), encoding='utf-8')
    print('installationUnchanged', unchanged)


if __name__ == '__main__':
    main()
