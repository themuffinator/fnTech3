#!/usr/bin/env python3
"""An Enemy Territory dedicated server leaves its bots out of broadcasts.

Windows only. Runs one ET dedicated server on an unswitched private desktop
with an isolated home, loopback networking and no clients: fnTech3's
(--server) or the installation's etded.exe (--retail-server). A config it
execs loads a map with bots enabled, spawns one bot with the game's
`spawnbot`, lists the clients, then says a burst of lines from the console in
one frame -- each a broadcast `chat` command -- and lists the clients again.

Retail ET passes over a client whose entity carries SVF_BOT when it
broadcasts. A server that queues the burst for the bot instead fills the
bot's reliable window, which nothing acknowledges within one frame, and drops
the bot with "Server command overflow". The burst is longer than retail's
256-command window, so any server that queued it would drop the bot.

A second stage then changes sv_hostname and says a line in each of FLOOD
spaced frames. Each change is a configstring update addressed to every primed
client, the bot included; each line is a broadcast. The game's bot AI is the
only thing that acknowledges a bot's commands, and it does not think here, so
the updates fill the bot's window until the server drops it: the pending
commands it prints then show what reached the bot.

The run passes when the bot is listed before and after the burst, nothing
overflowed during it, the flood then overflowed the bot's window with
configstring updates and no broadcast among its pending commands, the server
quit cleanly within the timeout and the installation is unchanged. No input
is injected and nothing is rendered.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

GAME = 'etmain'
MODULE = 'etmain/qagame_mp_x86.dll'
MARKER = 'fntech3_bot_probe'
SCRIPT = 'fntech3-bot-broadcast.cfg'
SETTLE_FRAMES = '60'
BURST = 300
FLOOD = 300
# Enough console frames apart that each flood step lands in a server frame of
# its own, so each hostname change is its own configstring update.
FLOOD_SPACING = '10'
PENDING = re.compile(r'cmd\s+-?\d+: ')
# Lines the game or engine prints about bots, connections and drops.
NOTABLE = re.compile(r'bot|AAS|ClientConnect|ClientBegin|ClientDisconnect|active client|overflow|'
                     r'timed out|pending server commands|exception|ERROR|Error:', re.IGNORECASE)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def marker(stage):
    return [f'set {MARKER} {stage}', MARKER]


def status_blocks(lines):
    """Each `status` listing, from its `map:` line to the next blank line."""
    blocks = []
    for index, line in enumerate(lines):
        if line.startswith('map: '):
            block = []
            for row in lines[index + 1:]:
                if not row.strip():
                    break
                block.append(row)
            blocks.append(block)
    return blocks


def bots(block):
    """Rows whose address column is a bot's."""
    return [row for row in block if re.search(r'(^|\s)bot(\s|$)', row)]


def probe(args, output):
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    (home / GAME).mkdir(parents=True)
    retail = args.retail_server
    server = args.installation / 'etded.exe' if retail else args.server
    before = inventory(args.installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    values = dict(fs_basepath=str(args.installation), fs_homepath=str(home),
                  logfile='2', net_ip='127.0.0.1', net_port=str(port), sv_pure='0',
                  sv_punkbuster='0', bot_enable='1', **{f'sv_master{i}': '' for i in range(1, 6)})
    if not retail:
        # IPv4 on loopback only: retail ET has no IPv6 socket to open.
        values = dict(com_gameProfile='wolfet', dedicated='1', net_enabled='1', **values)
    lines = [f'map {args.map}', f'wait {SETTLE_FRAMES}', *marker('spawn'), 'spawnbot',
             f'wait {SETTLE_FRAMES}', *marker('before'), 'status']
    lines += [f'say burst{n}' for n in range(BURST)]
    lines += [*marker('after'), f'wait {SETTLE_FRAMES}', 'status', *marker('flood')]
    for n in range(FLOOD):
        lines += [f'set sv_hostname fntech3-flood-{n}', f'say flood{n}', f'wait {FLOOD_SPACING}']
    lines += [*marker('flooded'), f'wait {SETTLE_FRAMES}', 'status', *marker('done'), 'quit']
    (home / GAME / SCRIPT).write_text(''.join(line + '\n' for line in lines), encoding='ascii')
    command = [str(server)]
    for name, value in values.items():
        command += ['+set', name, value]
    command += ['+exec', SCRIPT]
    log = home / GAME / ('etconsole.log' if retail else 'qconsole.log')
    result = dict(evidence=f'Live: retail {server.name} dedicated server' if retail
                  else 'Live: fnTech3 dedicated server', profile='wolfet', map=args.map,
                  retailServer=retail, burst=BURST, flood=FLOOD, command=command,
                  script=[line for line in lines if not line.startswith(('say ', 'set sv_hostname'))
                          and line != f'wait {FLOOD_SPACING}']
                  + [f'say burst0 .. say burst{BURST - 1}, in one frame after the first status',
                     f'after the flood marker, {FLOOD} times: set sv_hostname fntech3-flood-N; '
                     f'say floodN; wait {FLOOD_SPACING}'],
                  serverSha256=sha(server), moduleSha256=sha(args.installation / MODULE), passed=False)
    begin = time.monotonic()
    host = None
    try:
        host = WindowsPrivateProcess(command, server.parent, output / 'server-stdout.log')
        while time.monotonic() - begin < args.timeout and host.poll() is None:
            time.sleep(0.2)
        result['exitCode'] = host.poll()
        result['timedOut'] = result['exitCode'] is None
    except OSError as error:
        result['error'] = str(error)
    finally:
        if host:
            host.close()
    result['elapsedSeconds'] = round(time.monotonic() - begin, 3)
    text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
    rows = text.splitlines()
    result['log'] = str(log)
    result['markers'] = [row.strip() for row in rows if MARKER in row and 'set ' not in row]
    # The flood marker's line splits the log into the burst and flood stages.
    split = next((i for i, row in enumerate(rows) if MARKER in row and '"flood' in row), len(rows))
    stages = dict(burst=rows[:split], flood=rows[split:])
    blocks = status_blocks(rows)
    result['statusListings'] = blocks
    for name, index in (('BeforeBurst', 0), ('AfterBurst', 1), ('AfterFlood', 2)):
        result[f'botRows{name}'] = bots(blocks[index]) if len(blocks) > index else []
    for name, stage in stages.items():
        pending = [row for row in stage if PENDING.match(row)]
        numbers = [int(PENDING.match(row).group(0).split()[1].rstrip(':')) for row in pending]
        result[name] = dict(
            overflows=[row for row in stage if 'Server command overflow' in row],
            pendingDumps=sum('===== pending server commands =====' in row for row in stage),
            pendingCommands=len(pending),
            # The dump numbers from the last acknowledged command plus one.
            firstPendingNumber=numbers[0] if numbers else None,
            pendingConfigstrings=sum(bool(re.search(r': b?cs\d? ', row)) for row in pending),
            pendingBroadcasts=[row for row in pending if 'console: ' in row],
            pendingOther=[row for row in pending if not re.search(r': b?cs\d? ', row)
                          and 'console: ' not in row],
            pendingFirstAndLast=pending[:2] + pending[-2:])
    result['notable'] = [row for row in rows if NOTABLE.search(row) and 'burst' not in row
                         and 'flood' not in row and not PENDING.match(row)]
    result['installationUnchanged'] = before == inventory(args.installation)
    burst, flood = result['burst'], result['flood']
    # Retail prints the pending commands but not the drop reason for a bot.
    result['passed'] = bool(len(blocks) >= 3 and len(result['botRowsBeforeBurst']) == 1
                            and len(result['botRowsAfterBurst']) == 1 and not burst['overflows']
                            and not burst['pendingDumps'] and flood['pendingDumps'] == 1
                            and flood['pendingConfigstrings'] > 0
                            and not flood['pendingBroadcasts'] and not result['botRowsAfterFlood']
                            and result.get('exitCode') == 0 and not result.get('timedOut')
                            and result['installationUnchanged'] and not result.get('error'))
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('evidence', 'serverSha256', 'botRowsBeforeBurst',
                                              'botRowsAfterBurst', 'botRowsAfterFlood', 'burst', 'flood',
                                              'exitCode', 'installationUnchanged', 'passed')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    servers = parser.add_mutually_exclusive_group(required=True)
    servers.add_argument('--server', type=Path, help='fnTech3 dedicated executable')
    servers.add_argument('--retail-server', action='store_true', help="run the installation's etded.exe")
    parser.add_argument('--installation', type=Path, required=True,
                        help='ET root holding etded.exe and etmain')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--map', default='goldrush', help='stock map to load (default: goldrush)')
    parser.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This installed native-module probe requires Windows')
    args.installation = args.installation.resolve(strict=True)
    if args.server:
        args.server = args.server.resolve(strict=True)
    return 0 if probe(args, args.output.resolve()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
