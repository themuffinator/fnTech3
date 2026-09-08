"""Explicit Windows dedicated retail probe; never controls user input.

Run with --exe, --profile q3|ql, --assets and a new --output directory.
Uses private loopback RCON to restart/change maps and query bot status. Retail
inputs stay read-only; only sanitized manifests may be added to the repository.
"""
import argparse
import datetime
import hashlib
import json
import re
import secrets
import socket
import subprocess
import tempfile
import time
from pathlib import Path

from windows_private_process import WindowsPrivateProcess


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def transition_checks(record, maps):
    restarts = [a for a in record['actions'] if a['command'] == 'map_restart 0']
    marker = 'VM_Restart()' if record['profile'] == 'q3' else '(api 10)'
    samples = record['statusSamples']
    # QL may choose an initial rotation map. Later explicit map commands must
    # reach their requested targets, and each restart must retain that map.
    maps_ok = (len(samples) == 6 and len(samples[0]['map']) == 1
               and samples[0]['map'] == samples[1]['map']
               and [s['map'] for s in samples[2:]] == [[maps[1]], [maps[1]], [maps[0]], [maps[0]]])
    combined = '\n'.join(''.join(a['reply']) for a in record['actions'])
    return {'requestedMapsObserved': maps_ok,
            'threeModuleRestartsObserved': len(restarts) == 3 and all(
                'ShutdownGame' in ''.join(a['reply']) and marker in ''.join(a['reply']) for a in restarts),
            'noWorldBindingErrorInReplies': 'invalid world entity' not in combined}


def run(args):
    exe = args.exe.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=False)
    names = ('Sarge', 'Grunt', 'Visor', 'Major')
    maps = ('q3dm6', 'q3dm17') if args.profile == 'q3' else ('campgrounds', 'cursed')
    map_command = lambda name: 'map ' + name + (' ffa' if args.profile == 'ql' else '')
    schedule = [(8, 'status'), (12, 'map_restart 0'), (20, 'status')]
    for start, name in ((28, maps[1]), (60, maps[0])):
        schedule += [(start, map_command(name)), (start + 2, 'kick allbots')]
        schedule += [(start + 3 + i, 'addbot ' + bot + ' 3') for i, bot in enumerate(names)]
        if args.profile == 'ql':
            schedule += [(start + 7, 'allready')]
        schedule += [(start + 12, 'status'), (start + 16, 'map_restart 0'), (start + 24, 'status')]
    if args.profile == 'ql':
        schedule += [(at + 1, 'g_gameState') for at, cmd in schedule if cmd == 'status']
    schedule += [(90, 'quit')]
    schedule.sort()
    record = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'profile': args.profile, 'architecture': 'Windows x86',
              'engineSha256': digest(exe), 'probeSha256': digest(Path(__file__)),
              'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'worktreeDirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], text=True)),
              'assets': {p.name: digest(p) for p in sorted((assets / 'baseq3').glob('*.pk3'))},
              'actions': [], 'desktopSwitched': False, 'error': None}
    with tempfile.TemporaryDirectory(prefix='fntech3-retail-transitions-') as temporary:
        home = Path(temporary)
        game = home / 'baseq3'
        game.mkdir()
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        password = secrets.token_hex(24)
        config = ''.join(f'set sv_master{i} ""\n' for i in range(1, 6))
        config += ('set sv_pure 1\nset sv_maxclients 8\nset bot_enable 1\n'
                   'set bot_minplayers 0\nset g_gametype 0\nset fraglimit 0\n'
                   'set timelimit 0\nset sv_autoRecordDemos 0\nset g_doWarmup 0\n'
                   'set g_warmup 0\nset g_warmupDelay 0\n')
        config += map_command(maps[0]) + '\nwait 10\n'
        config += ''.join('addbot ' + name + ' 3\n' for name in names)
        if args.profile == 'ql':
            config += 'allready\n'
        (game / 'probe.cfg').write_text(config)
        command = [str(exe)]
        for key, value in {'com_gameProfile': args.profile, 'fs_basepath': str(assets),
                           'fs_homepath': str(home), 'fs_game': '', 'fs_cdpath': '',
                           'net_ip': '127.0.0.1', 'net_port': str(port), 'dedicated': '1',
                           'com_steamIntegration': '0', 'fs_skipWorkshop': '1',
                           'logfile': '2', 'developer': '1', 'r_fullscreen': '0',
                           'in_mouse': '0', 'in_joystick': '0', 'in_nograb': '1',
                           'rconPassword': password}.items():
            command += ['+set', key, value]
        command += ['+exec', 'probe.cfg']
        record['command'] = ['<redacted temporary RCON password>' if item == password else item for item in command]
        record['config'] = config
        started = time.monotonic()
        proc = WindowsPrivateProcess(command, exe.parent, home / 'stdout.log')
        record['privateDesktop'] = proc.name
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as control:
                control.settimeout(.3)
                for delay, cmd in schedule:
                    while time.monotonic() - started < delay and proc.poll() is None:
                        time.sleep(.1)
                    if proc.poll() is not None:
                        break
                    control.sendto(b'\xff' * 4 + f'rcon {password} {cmd}\n'.encode(), ('127.0.0.1', port))
                    replies = []
                    try:
                        while True:
                            data, peer = control.recvfrom(65535)
                            if peer == ('127.0.0.1', port):
                                replies.append(data.decode('utf-8', errors='replace').replace(password, '<redacted>'))
                    except (socket.timeout, ConnectionResetError):
                        pass
                    record['actions'].append({'atSeconds': round(time.monotonic() - started, 3),
                                              'command': cmd, 'reply': replies})
            record['exitCode'] = proc.wait(5)
        except subprocess.TimeoutExpired:
            record['error'] = 'process timeout'
            record['exitCode'] = None
        finally:
            record['forcedStop'] = proc.poll() is None
            proc.close()
        record['elapsedSeconds'] = round(time.monotonic() - started, 3)
        log = (game / 'qconsole.log').read_text(errors='replace') if (game / 'qconsole.log').exists() else ''
        log = log.replace(password, '<redacted>')
        (args.output / 'console.log').write_text(log)
        record['consoleSha256'] = digest(args.output / 'console.log')
        record['lifecycleLines'] = [line for line in log.splitlines() if any(
            word in line for word in ('InitGame', 'ShutdownGame', 'Server Shutdown', 'ERROR', 'invalid world entity'))]
        statuses = [a for a in record['actions'] if a['command'] == 'status']
        record['statusSamples'] = []
        for action in statuses:
            reply = ''.join(action['reply'])
            rows = [line for line in reply.splitlines() if re.match(r'\s*\d+\s+-?\d+\s+\w+', line)]
            bots = [name for name in names if any(re.search(r'\b' + name + r'\b', row) and 'bot' in row.lower() for row in rows)]
            record['statusSamples'].append({'atSeconds': action['atSeconds'], 'bots': bots,
                                             'map': re.findall(r'map:\s*(\S+)', reply)})
        record['passed'] = (record['exitCode'] == 0 and not record['forcedStop']
                            and len(statuses) == 6 and all(len(s['bots']) == 4 for s in record['statusSamples'])
                            and 'invalid world entity' not in log)
        if args.profile == 'ql':
            states = [a for a in record['actions'] if a['command'] == 'g_gameState']
            record['passed'] &= len(states) == 6 and all('IN_PROGRESS' in ''.join(a['reply']) for a in states)
        record['transitionChecks'] = transition_checks(record, maps)
        record['passed'] &= all(record['transitionChecks'].values())
    (args.output / 'manifest.json').write_text(json.dumps(record, indent=2, ensure_ascii=True) + '\n')
    print(json.dumps({k: record[k] for k in ('profile', 'passed', 'exitCode', 'forcedStop', 'statusSamples')}), flush=True)
    return 0 if record['passed'] else 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--profile', choices=('q3', 'ql'), required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    raise SystemExit(run(parser.parse_args()))
