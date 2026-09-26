#!/usr/bin/env python3
"""A bot holds its profile's retail reliable-command window, whatever its slot.

Windows only. Runs one fnTech3 dedicated server on an unswitched private
desktop with an isolated home, loopback networking and no clients. A config it
execs loads a map, makes one bot with the game's own command -- Quake III's
`addbot`, or ET's `spawnbot` with bot_enable set -- and lists the clients. It
then sends that bot a burst of `tell` lines in one frame. Each is a `chat`
command addressed to the bot alone, and nothing acknowledges a bot's commands
within a frame, so the burst fills the bot's reliable window until the server
drops it with "Server command overflow". The pending commands it prints then
are the window: every command the bot held, and the one that overflowed.

The retail windows: quake3.exe 1.32 rings 64 commands (SV_AddServerCommand
0x4395b0), and et.exe and etded.exe ring 256 (0x447820, 0x42e010). No retail
engine has `tell`, so only fnTech3 servers run here; retail etded.exe's own
overflow, on a bot's 257th unacknowledged command, is recorded in
docs/fntech3/2026-09-24-wolfet-bot-broadcasts.md.

The run passes when the bot was listed before the burst, the server dropped
it holding exactly the profile's retail window, the bot was gone afterwards,
the server quit cleanly within the timeout and the installation is unchanged.
No input is injected and nothing is rendered.
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

PROFILES = {
    'quake3': dict(game='baseq3', module='baseq3/pak0.pk3', map='q3dm1', bot='addbot sarge 1',
                   slot=0, retailWindow=64),
    'wolfet': dict(game='etmain', module='etmain/qagame_mp_x86.dll', map='goldrush', bot='spawnbot',
                   slot=1, retailWindow=256),
}
BURST = 300
MARKER = 'fntech3_bot_window'
SCRIPT = 'fntech3-bot-window.cfg'
SETTLE_FRAMES = '60'
PENDING = re.compile(r'cmd\s+-?\d+: (.*)$')
TELL = 'chat "^6console: flood'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def bot_rows(lines, slot):
    """The bot's row in each `status` listing: its slot, then `bot` as its address."""
    listings = []
    for index, line in enumerate(lines):
        if line.startswith('cl score ping name'):
            rows = []
            for row in lines[index + 2:]:
                if not row.strip():
                    break
                rows.append(row)
            listings.append([row for row in rows if re.match(rf'\s*{slot}\s', row) and ' bot ' in row])
    return listings


def probe(args, output):
    settings = PROFILES[args.profile]
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    (home / settings['game']).mkdir(parents=True)
    before = inventory(args.installation)
    serverBefore = sha(args.server)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    values = dict(com_gameProfile=args.profile, dedicated='1', fs_basepath=str(args.installation),
                  fs_homepath=str(home), logfile='2', net_ip='127.0.0.1', net_port=str(port),
                  sv_pure='0', bot_enable='1', **{f'sv_master{i}': '' for i in range(1, 6)})
    slot = settings['slot']
    lines = [f'map {settings["map"]}', f'wait {SETTLE_FRAMES}', settings['bot'], f'wait {SETTLE_FRAMES}',
             f'set {MARKER} burst', MARKER, 'status']
    lines += [f'tell {slot} flood{n}' for n in range(BURST)]
    lines += ['wait 10', 'status', f'set {MARKER} done', MARKER, 'quit']
    (home / settings['game'] / SCRIPT).write_text(''.join(line + '\n' for line in lines), encoding='ascii')
    command = [str(args.server)]
    for name, value in values.items():
        command += ['+set', name, value]
    command += ['+exec', SCRIPT]
    log = home / settings['game'] / 'qconsole.log'
    result = dict(evidence='Live: fnTech3 dedicated server', profile=args.profile, map=settings['map'],
                  bot=settings['bot'], slot=slot, burst=BURST, command=command,
                  serverSha256=serverBefore, moduleSha256=sha(args.installation / settings['module']),
                  retailWindow=settings['retailWindow'], passed=False)
    begin = time.monotonic()
    host = None
    try:
        host = WindowsPrivateProcess(command, args.server.parent, output / 'server-stdout.log')
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
    result['serverUnchanged'] = sha(args.server) == serverBefore
    text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
    log_lines = text.splitlines()
    result['log'] = str(log)
    result['markers'] = [line.strip() for line in log_lines if MARKER in line and 'is:' in line]
    listings = bot_rows(log_lines, slot)
    result['botBefore'] = listings[0] if listings else []
    result['botAfter'] = listings[-1] if len(listings) > 1 else None
    dumps = [index for index, line in enumerate(log_lines) if line.startswith('===== pending server commands')]
    result['overflows'] = len(dumps)
    pending = []
    if dumps:
        for line in log_lines[dumps[0] + 1:]:
            matched = PENDING.match(line)
            if not matched:
                break
            pending.append(matched.group(1))
    held, overflowed = pending[:-1], pending[-1:]
    result['window'] = len(held)
    result['heldTells'] = sum(1 for command in held if command.startswith(TELL))
    # The game's own commands addressed to the bot before the burst, by verb.
    game = {}
    for command in held:
        if not command.startswith(TELL):
            verb = command.split(' ', 1)[0]
            game[verb] = game.get(verb, 0) + 1
    result['heldGameCommands'] = game
    result['overflowingCommand'] = overflowed[0] if overflowed else None
    result['tellsDelivered'] = sum(1 for line in log_lines if 'console: flood' in line and not PENDING.match(line))
    result['tellsRefused'] = sum(1 for line in log_lines if line.strip() == f'Client {slot} is not active')
    result['errors'] = [line for line in log_lines if line.startswith('ERROR') or 'Error:' in line]
    result['installationUnchanged'] = before == inventory(args.installation)
    result['matchesRetail'] = result['window'] == settings['retailWindow']
    result['passed'] = bool(result['botBefore'] and result['overflows'] == 1 and result['matchesRetail']
                            and result['heldTells'] + sum(game.values()) == result['window']
                            and result['overflowingCommand'] and result['overflowingCommand'].startswith(TELL)
                            and result['botAfter'] == [] and result.get('exitCode') == 0
                            and not result.get('timedOut') and not result.get('error')
                            and result['serverUnchanged'] and result['installationUnchanged']
                            and not result['errors'])
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('evidence', 'profile', 'serverSha256', 'botBefore', 'window',
                                              'heldTells', 'heldGameCommands', 'overflowingCommand',
                                              'retailWindow', 'exitCode', 'installationUnchanged',
                                              'passed')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--server', type=Path, required=True, help='fnTech3 dedicated executable')
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    parser.add_argument('--installation', type=Path, required=True,
                        help='retail root holding the game directory')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=150)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This installed native-module probe requires Windows')
    args.installation = args.installation.resolve(strict=True)
    args.server = args.server.resolve(strict=True)
    return 0 if probe(args, args.output.resolve()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
