#!/usr/bin/env python3
"""How often a Holomatch server sends a client its snapshots, in game time.

For each server (retail ``stvoyHM.exe`` or an fnTech3 dedicated server) and
each timescale, the server starts ``hm_borg1`` with `devmap` at that
timescale with two bots and is set `g_synchronousClients 1` (an Elite Force
client records only from such a server), the fnTech3 client joins it with the
given `snaps`, records a demo for ``--seconds`` of wall time and leaves. The demo's snapshots
are read with ``fntech3_stvef_demo_probe --times`` (server time and flags of
each), and the run reports how far apart their server times are.

Windows only. Every process runs windowed on a private desktop with an
isolated writable folder under ``--output``, bound to 127.0.0.1, without
sound. No mouse or keyboard input is injected. Retail reads a copy of the
retail paks (``.tmp/runtime/stvef-retail``), as every Elite Force probe does,
and the installation is compared before and after.
"""
from collections import Counter
from pathlib import Path
import argparse
import json
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'scripts'))
import probe_stvef_modes as modes  # noqa: E402
from probe_legacy_discovery import INSTALLS, stage_retail  # noqa: E402
from probe_legacy_sessions import (NAME, PROFILES, Run, begins, inventory, query, slot_of,  # noqa: E402
                                   start_client, wait_for)

KEY = 'stvef-mp'
CASE = ('cadence', 'hm_borg1', 0, (), ('Chell', 'Chakotay'))
SNAPFLAG_RATE_DELAYED = 1


def decode(probe, demo):
    result = subprocess.run([str(probe), '--times'], input=str(demo) + '\n', capture_output=True, text=True,
                            check=True)
    row = json.loads(result.stdout.splitlines()[0])
    times = row['snapshotTimes']
    gaps = Counter(b[1] - a[1] for a, b in zip(times, times[1:]))
    return dict(error=row['error'], snapshots=len(times),
                serverSeconds=round((times[-1][1] - times[0][1]) / 1000, 2) if times else 0,
                gaps={str(k): v for k, v in sorted(gaps.items())},
                rateDelayed=sum(1 for t in times if t[2] & SNAPFLAG_RATE_DELAYED))


def run_case(output, kind, runtime, client_runtime, layout, timescale, snaps, seconds, probe):
    modes.PLAY.update(timescale=timescale, timelimit='0')
    label = f'{kind}-ts{timescale}-snaps{snaps}'
    run = Run(output, label)
    server = modes.server('retail' if kind == 'retail' else 'fntech3', runtime, run.dir / 'server', layout, CASE)
    client = None
    record = dict(kind=kind, timescale=timescale, snaps=snaps, serverCommand=server.command,
                  serverSha256=server.sha256)
    try:
        ready = wait_for(lambda: CASE[1] in (query(server.port, 'getinfo probe', timeout=1.0) or ''), 60, 0.5)
        if not run.step('server-ready', ready):
            return record
        # An Elite Force client records a demo only from a server that sets
        # g_synchronousClients, as retail's does; the server's snapshot
        # schedule does not read it.
        (server.cfg_dir / 'probesync.cfg').write_text('set g_synchronousClients 1\n', encoding='latin1')
        time.sleep(1.1)
        query(server.port, f'rcon {server.password} exec probesync.cfg', timeout=1.5)
        time.sleep(1.1)
        synchronous = query(server.port, f'rcon {server.password} g_synchronousClients', timeout=1.5) or ''
        if not run.step('synchronous-clients', '"g_synchronousClients" is:"1' in synchronous,
                        reply=synchronous.strip()[-80:]):
            return record
        client = start_client(run, 'fntech3', KEY, PROFILES[KEY], INSTALLS[KEY], client_runtime, layout,
                              server.port, extra_lines=[f'set snaps {snaps}'])
        record.update(clientCommand=client.command, clientSha256=client.sha256)
        game = lambda: server.game_log()
        slot = wait_for(lambda: slot_of(game(), NAME), PROFILES[KEY].get('connectSeconds', 40))
        joined = slot is not None and wait_for(lambda: begins(game(), slot) >= 1, 40)
        if not run.step('join', joined, slot=slot):
            return record
        time.sleep(2.0)
        client.driver.send('record cadence')
        time.sleep(seconds)
        client.driver.send('stoprecord')
        demo = client.home / 'BaseEF' / 'demos' / 'cadence.efdemo'
        written = wait_for(lambda: demo.exists() and demo.stat().st_size > 0, 15)
        time.sleep(1.0)
        if run.step('demo', written, path=str(demo)):
            record.update(decode(probe, demo))
    finally:
        if client:
            if client.alive():
                client.driver.send('quit')
            client.stop(15)
            (run.dir / 'client-console.log').write_text(client.log(), encoding='latin1')
        if server.alive():
            query(server.port, f'rcon {server.password} quit', timeout=1.0)
        server.stop(15)
        (run.dir / 'server-console.log').write_text(server.log(), encoding='latin1')
        (run.dir / 'server-games.log').write_text(server.game_log(), encoding='latin1')
        record['steps'] = run.steps
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--servers', nargs='+', required=True, metavar='KIND=RUNTIME',
                        help="'retail', or a label and an fnTech3 runtime folder, e.g. candidate=.tmp/runtime/x")
    parser.add_argument('--client-runtime', type=Path, required=True, help="the fnTech3 client's runtime folder")
    parser.add_argument('--probe', type=Path, required=True, help='built fntech3_stvef_demo_probe executable')
    parser.add_argument('--timescales', nargs='+', default=['1', '3'])
    parser.add_argument('--snaps', nargs='+', default=['20'])
    parser.add_argument('--seconds', type=float, default=20.0, help='wall seconds each demo records')
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Windows only')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    installation = INSTALLS[KEY]
    layout = stage_retail(KEY, PROFILES[KEY], installation)
    before = inventory(installation)
    results = []
    for server in args.servers:
        kind, _, folder = server.partition('=')
        runtime = Path(folder).resolve(strict=True) if folder else None
        for timescale in args.timescales:
            for snaps in args.snaps:
                result = run_case(output, kind, runtime, args.client_runtime.resolve(strict=True), layout,
                                  timescale, snaps, args.seconds, args.probe.resolve(strict=True))
                results.append(result)
                print(f"{kind:10s} timescale {timescale:4s} snaps {snaps:3s} snapshots={result.get('snapshots')} "
                      f"gaps={result.get('gaps')} rateDelayed={result.get('rateDelayed')}", flush=True)
                (output / 'summary.json').write_text(json.dumps(dict(results=results), indent=1) + '\n',
                                                     encoding='utf-8')
    summary = dict(results=results, installationUnchanged=before == inventory(installation))
    (output / 'summary.json').write_text(json.dumps(summary, indent=1) + '\n', encoding='utf-8')
    print('installationUnchanged', summary['installationUnchanged'])


if __name__ == '__main__':
    main()
