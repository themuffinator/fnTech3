#!/usr/bin/env python3
"""Open each of Elite Force single player's console-opened menus, with no
device input, and keep an engine screenshot of each.

efuix86.dll answers console commands of its own for the menus the game
opens when the player uses a panel aboard the Virtual Voyager -- the
holodeck, turbolift and transporter panels, the ship's library, logs and
consoles -- for the closing credits, and for the generic and holodeck menus
(`genericmenu`, `genericholomenu`, `endholomenu`). The probe starts a level,
then for each command opens its menu over loopback rcon, waits, and writes an
engine screenshot. It records what the client logged for each: an unknown
command, an error, or nothing. Runs are windowed on an unswitched private
desktop, with an isolated home, no sound and no input; the installation is
read only.
"""
from pathlib import Path
import argparse
import json
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from probe_stvef_campaign import Client, INSTALL, inventory, sha, settle, started, level_start, wait_for  # noqa: E402

# The menu commands efuix86.dll's UI_ConsoleCommand answers (their names in
# its strings), in the order tried. The credits come last: they hold the
# menus until they end.
COMMANDS = ['ui_holodeck', 'ui_turbolift', 'ui_transporter', 'ui_library', 'ui_log', 'ui_navigation',
            'ui_ops', 'ui_padd', 'ui_tactical', 'ui_astrometrics', 'ui_cargo', 'ui_diseases',
            'ui_engineeringdata', 'ui_engineeringstatus', 'ui_weaponlibrary', 'ui_shootingrange',
            'ui_hazardgame', 'ui_initialsetup', 'genericmenu', 'genericholomenu', 'endholomenu',
            'ui_closingcredits']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--level', default='tour/deck02')
    parser.add_argument('--renderer', default='glx', choices=['glx', 'vk', 'rtx'])
    parser.add_argument('--commands', nargs='+', default=COMMANDS)
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Windows only')
    runtime = args.runtime.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    before = inventory(INSTALL)
    client = Client(runtime, output / 'home', args.renderer, args.level)
    rows = []
    error = None
    try:
        if not wait_for(lambda: started(client, args.level, 0), 120):
            raise RuntimeError('level did not start')
        settle(client)
        client.rcon('screenshot sp-menu-before')
        for command in args.commands:
            mark = len(client.log())
            client.rcon(command)
            time.sleep(2.5)
            name = f'sp-menu-{command}'
            client.rcon(f'screenshot {name}')
            time.sleep(1.2)
            text = client.log()[mark:]
            shots = sorted(client.home.rglob(f'{name}.*'))
            rows.append(dict(command=command, unknown=bool(re.search(r'(?i)unknown command', text)),
                             errors=re.findall(r'(?m)^[^\n]*(?:ERROR|rror:)[^\n]*$', text)[:3],
                             logged=[line for line in text.splitlines() if line.strip()][:8],
                             screenshot=[str(p.relative_to(output)) for p in shots],
                             screenshotSha256=[sha(p) for p in shots], alive=client.alive()))
            print(f"{command:22s} unknown={rows[-1]['unknown']} errors={rows[-1]['errors']} "
                  f"shots={len(shots)}", flush=True)
            if not client.alive():
                break
    except Exception as failure:  # keep what was observed
        error = repr(failure)
    finally:
        exit_code, forced = client.stop()
    distinct = len({tuple(r['screenshotSha256']) for r in rows if r['screenshotSha256']})
    result = dict(runtime=str(runtime), sha256=client.sha256, level=args.level, renderer=args.renderer,
                  command=client.command, rows=rows, distinctScreenshots=distinct, exit=exit_code, forced=forced,
                  probeError=error, installationUnchanged=before == inventory(INSTALL))
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('rows', len(rows), 'distinct screenshots', distinct, 'error', error)


if __name__ == '__main__':
    main()
