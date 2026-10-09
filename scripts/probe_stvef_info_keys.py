#!/usr/bin/env python3
"""List what retail ``stvoyHM.exe``'s dedicated Holomatch server and fnTech3's
publish in ``serverinfo`` and ``systeminfo``.

Both servers start on one map with the same settings (those of
``probe_stvef_modes.py``), and each is asked for its ``serverinfo`` and
``systeminfo`` over rcon. The keys each lists, and those only one of them
lists, are printed and written to ``result.json`` with both raw replies.
Retail prints a key in a 20-character column and runs a longer key into its
value, so keys are taken from the column.

Windows only. Both servers are bound to 127.0.0.1 with isolated writable
folders under ``--output``; retail reads a copy of the retail paks, as every
Elite Force probe does. No client takes part, and no input is involved.
"""
from pathlib import Path
import argparse
import json
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
import probe_stvef_modes as modes  # noqa: E402
from probe_legacy_discovery import INSTALLS, stage_retail  # noqa: E402
from probe_legacy_sessions import PROFILES, query, wait_for, inventory  # noqa: E402

KNOWN_LONG = ('g_pModDisintegration', 'g_synchronousClients', 'sv_referencedPakNames')


def keys(reply):
    """The keys of a `serverinfo` or `systeminfo` print, in order."""
    found = []
    for line in reply.replace('\xff', '').splitlines():
        if not line.strip() or line.startswith(('print', 'Server info', 'System info')):
            continue
        long_key = next((key for key in KNOWN_LONG if line.startswith(key)), None)
        found.append(long_key or line[:20].split()[0])
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True, help='fnTech3 runtime folder')
    parser.add_argument('--map', default='hm_borg1')
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Windows only')
    runtime = args.runtime.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    installation = INSTALLS['stvef-mp']
    layout = stage_retail('stvef-mp', PROFILES['stvef-mp'], installation)
    before = inventory(installation)
    case = ('info', args.map, 0, (), ())
    results = {}
    for kind in ('retail', 'fntech3'):
        peer = modes.server(kind, runtime, output / kind, layout, case)
        try:
            wait_for(lambda: args.map in (query(peer.port, 'getinfo probe', timeout=1.0) or ''), 90, 0.5)
            time.sleep(2.0)
            entry = dict(sha256=peer.sha256)
            for command in ('serverinfo', 'systeminfo'):
                reply = query(peer.port, f'rcon {peer.password} {command}', timeout=2.0) or ''
                entry[command] = dict(raw=reply.replace('\xff', ''), keys=keys(reply))
                time.sleep(1.2)
            results[kind] = entry
        finally:
            if peer.alive():
                query(peer.port, f'rcon {peer.password} quit', timeout=1.0)
            peer.stop(15)
    for command in ('serverinfo', 'systeminfo'):
        retail, ours = results['retail'][command]['keys'], results['fntech3'][command]['keys']
        results[command] = dict(onlyRetail=sorted(set(retail) - set(ours)), onlyFnTech3=sorted(set(ours) - set(retail)),
                                counts=[len(retail), len(ours)])
        print(f"{command}: retail {len(retail)} keys, fnTech3 {len(ours)}; only retail "
              f"{results[command]['onlyRetail']}, only fnTech3 {results[command]['onlyFnTech3']}")
    results['installationUnchanged'] = before == inventory(installation)
    (output / 'result.json').write_text(json.dumps(results, indent=1) + '\n', encoding='utf-8')
    print('installationUnchanged', results['installationUnchanged'])


if __name__ == '__main__':
    main()
