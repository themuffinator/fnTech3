#!/usr/bin/env python3
"""Pure admission on every installed RTCW MP map, in both directions.

For each official MP map found in the installation, in the game type its own
.arena file advertises (objective, else checkpoint), this runs:

- inbound: a retail WolfMP.exe client joins the fnTech3 dedicated server running
  sv_pure 1 (scripts/probe_rtcw_retail_client.py --pure);
- outbound: the fnTech3 client joins a retail WolfMP.exe dedicated server running
  sv_pure 1 (scripts/probe_client_to_server.py --retail-server --pure).

Both probes are windowed on private desktops with isolated homes and inject no
input. A map passes when both directions are admitted after the pure proof with
no unpure drop. This is admission evidence, not gameplay or objective evidence.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import probe_rtcw_campaign as campaign


def run(command, log):
    with log.open('w', encoding='utf-8') as handle:
        return subprocess.run(command, cwd=ROOT, stdout=handle, stderr=subprocess.STDOUT).returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--maps', nargs='*', help='limit to these maps')
    args = parser.parse_args()
    installation = args.installation.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    maps = [m for m in campaign.maps(installation) if m['mode'] == 'mp'
            and (not args.maps or m['map'] in args.maps)]
    rows = []
    for item in maps:
        name, gametype = item['map'], item.get('gameType', 5)
        inbound, outbound = output / name / 'inbound', output / name / 'outbound'
        (output / name).mkdir()
        inbound_exit = run([sys.executable, 'scripts/probe_rtcw_retail_client.py', '--server', str(args.server),
                            '--installation', str(installation), '--output', str(inbound), '--pure',
                            '--map', name, '--gametype', str(gametype)], output / name / 'inbound.out')
        outbound_exit = run([sys.executable, 'scripts/probe_client_to_server.py', '--client', str(args.client),
                             '--retail-server', '--pure', '--installation', str(installation),
                             '--map', name, '--gametype', str(gametype), '--output', str(outbound)],
                            output / name / 'outbound.out')
        row = dict(map=name, gametype=gametype, pak=item['pak'], inboundExit=inbound_exit,
                   outboundExit=outbound_exit)
        for key, folder in (('inbound', inbound), ('outbound', outbound)):
            result = folder / 'result.json'
            data = json.loads(result.read_text(encoding='utf-8')) if result.exists() else {}
            row[key] = {k: data.get(k) for k in (
                'passed', 'active', 'serverActive', 'pureAdmission', 'unpureDrop', 'clientSawPureServer',
                'clientPureListEntries', 'installationUnchanged', 'elapsedSeconds', 'error')
                if k in data}
        row['passed'] = bool(row['inbound'].get('passed') and row['outbound'].get('passed'))
        rows.append(row)
        print(name, gametype, 'inbound', row['inbound'].get('passed'), 'outbound', row['outbound'].get('passed'),
              flush=True)
    summary = dict(maps=len(rows), passed=sum(r['passed'] for r in rows), rows=rows)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    return 0 if rows and all(r['passed'] for r in rows) else 1


if __name__ == '__main__':
    raise SystemExit(main())
