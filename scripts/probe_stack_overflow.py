#!/usr/bin/env python3
"""Overflow the main thread's stack of a running fnTech3 client and dedicated server on purpose.

The developer-only `crash stack` command recurses until the stack runs out;
`crash` writes through a null pointer, the path every other fault takes. Each
client scenario is a fresh client, windowed on an unswitched private desktop
with sound and mouse/joystick input off, driven by a startup config:

  client-menu-overflow         `crash stack` at the main menu
  client-map-overflow          `crash stack` from the console with a map loaded
  client-map-access-violation  `crash` there

A command buffer does not survive an error, so a client meets one fault. A
dedicated server still takes rcon with no map running, so one server meets a
sequence of them, on loopback (`server-sequence`):

  1. `crash stack` by rcon while a map runs: the command runs inside the
     server's frame, under the rcon redirect whose cleanup the unwind runs
  2. `crash stack` again with no map: the guard page the first overflow used
     up must be back, or this one ends the process
  3. `crash` with no map
  4. the map again, then `crash stack` inside its frame
  5. `quit`, which must end the process with status 0

A fault ends as `reported` when the engine prints its ERR_DROP naming the
expected exception code (`STACK_OVERFLOW` or `ACCESS_VIOLATION`) and the
process is still running two seconds later (and still answering rcon, for the
server), `crashed` when the process dies, `wrong-report` when the ERR_DROP
names something else, `unresponsive` when a reporting server stops answering,
or `silent` when nothing happens within the time limit. Every fault must end
`reported`, and the server must quit with status 0.
Windows only; console commands and rcon only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

GAMES = {'q3': 'baseq3', 'ql': 'baseq3', 'rtcw-sp': 'main', 'rtcw-mp': 'main'}
# An empty configuration in the home shadows the installation's own.
CONFIGS = ('q3config', 'qzconfig', 'wolfconfig', 'wolfconfig_mp')
# Com_Error prints an ERR_DROP between two rows of asterisks.
DROP = re.compile(r'^\*{20}\r?\nERROR: (.*?)\s*^\*{20}', re.M | re.S)
CODE = re.compile(r'Exception Code: (\S+)')
ADDRESS = re.compile(r'Exception Address: (\S+)')
PASSWORD = 'overflowprobe'
OUTCOMES = ('reported', 'crashed', 'wrong-report', 'unresponsive', 'silent')

CLIENT_SCENARIOS = {
    'client-menu-overflow': ('wait 30\ncrash stack\n', 'STACK_OVERFLOW'),
    'client-map-overflow': ('{map}\nwait 120\ncrash stack\n', 'STACK_OVERFLOW'),
    'client-map-access-violation': ('{map}\nwait 120\ncrash\n', 'ACCESS_VIOLATION'),
}


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def prepare_home(home, profile):
    if home.exists():
        shutil.rmtree(home)
    game = home / GAMES[profile]
    game.mkdir(parents=True)
    for name in CONFIGS:
        for suffix in ('.cfg', '_server.cfg'):
            (game / (name + suffix)).write_bytes(b'')
    return game


def common_variables(profile, installation, home):
    return dict(com_gameProfile=profile, fs_basepath=str(installation), fs_homepath=str(home), fs_game='',
                logfile='2', developer='1', sv_pure='0', bot_enable='0', com_steamIntegration='0',
                fs_skipWorkshop='1', sv_errorExit='0', sv_master1='', sv_master2='', sv_master3='')


def command_line(exe, variables, config):
    command = [str(exe)]
    for key, value in variables.items():
        command += ['+set', key, value]
    return command + ['+exec', config]


def reader(log):
    return lambda: log.read_text(encoding='latin-1') if log.exists() else ''


def drops(text):
    return [match.group(1).replace('\r\n', '\n') for match in DROP.finditer(text)]


def await_drop(process, read, count, timeout):
    """Wait for the count-th ERR_DROP; return (message, alive two seconds later)."""
    begin = time.monotonic()
    while time.monotonic() - begin < timeout:
        found = drops(read())
        if len(found) >= count:
            time.sleep(2)
            return found[count - 1], process.poll() is None
        if process.poll() is not None:
            time.sleep(0.5)
            found = drops(read())
            return (found[count - 1] if len(found) >= count else None), False
        time.sleep(0.1)
    return None, process.poll() is None


def classify(message, alive, expected):
    if message is None:
        return 'silent' if alive else 'crashed'
    if not alive:
        return 'crashed'
    code = CODE.search(message)
    return 'reported' if message.startswith('Unhandled exception caught') and code and code.group(1) == expected \
        else 'wrong-report'


def fault_row(message, alive, expected, exit_code=None):
    code = CODE.search(message or '')
    address = ADDRESS.search(message or '')
    return dict(outcome=classify(message, alive, expected), expectedCode=expected,
                reportedCode=code.group(1) if code else None, address=address.group(1) if address else None,
                message=message, aliveAfterTwoSeconds=alive, exitCode=exit_code)


def stop(process):
    try:
        process.close()
    except subprocess.TimeoutExpired:
        process.wait(60)
        process.close()


def run_client(name, client, installation, profile, load, output):
    steps, expected = CLIENT_SCENARIOS[name]
    home = output / f'{name}-home'
    game = prepare_home(home, profile)
    (game / 'probe.cfg').write_text(steps.format(map=load), encoding='utf-8')
    variables = common_variables(profile, installation, home)
    variables.update(r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0',
                     cl_webuiEnable='0', cl_renderer='glx', cl_allowDownload='0', cl_motd='0', net_enabled='0',
                     com_introplayed='1')
    read = reader(game / 'qconsole.log')
    begin = time.monotonic()
    process = WindowsPrivateProcess(command_line(client, variables, 'probe.cfg'), client.parent,
                                    output / f'{name}-stdout.log')
    try:
        message, alive = await_drop(process, read, 1, 90)
        exit_code = process.poll()
    finally:
        stop(process)
    (output / f'{name}-console.log').write_text(read(), encoding='utf-8')
    row = fault_row(message, alive, expected, exit_code)
    row['elapsedSeconds'] = round(time.monotonic() - begin, 2)
    return row


def rcon(port, command, timeout=2.0):
    packet = b'\xff\xff\xff\xffrcon ' + PASSWORD.encode() + b' ' + command.encode() + b'\n'
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.settimeout(timeout)
        s.sendto(packet, ('127.0.0.1', port))
        chunks = []
        try:
            while True:
                data, _ = s.recvfrom(65536)
                chunks.append(data[4:].decode('latin-1', 'replace'))
                s.settimeout(0.3)
        except (socket.timeout, ConnectionResetError):
            # Windows reports an ICMP port-unreachable as a reset on the next receive.
            pass
        return ''.join(chunks)


def await_echo(process, port, token, timeout):
    begin = time.monotonic()
    while time.monotonic() - begin < timeout and process.poll() is None:
        if token in rcon(port, f'echo {token}', 1.0):
            return True
        time.sleep(0.5)
    return False


def await_map(process, port, read, timeout):
    """rcon answers with or without a map; a map runs once `status` lists its name."""
    begin = time.monotonic()
    while time.monotonic() - begin < timeout and process.poll() is None:
        if 'map:' in rcon(port, 'status', 1.0):
            return True
        time.sleep(0.5)
    return False


def run_server(server, installation, profile, load, port, output):
    name = 'server-sequence'
    home = output / f'{name}-home'
    game = prepare_home(home, profile)
    (game / 'probe.cfg').write_text(load + '\n', encoding='utf-8')
    variables = common_variables(profile, installation, home)
    variables.update(dedicated='1', net_enabled='1', net_ip='127.0.0.1', net_port=str(port),
                     rconPassword=PASSWORD, sv_maxclientsPerIP='8')
    read = reader(game / 'qconsole.log')
    begin = time.monotonic()
    process = WindowsPrivateProcess(command_line(server, variables, 'probe.cfg'), server.parent,
                                    output / f'{name}-stdout.log')
    steps, replies = [], []
    quit_code = None
    try:
        ready = await_map(process, port, read, 60)
        steps.append(dict(step='map running', ok=ready))
        faults = 0
        plan = [('crash stack', 'STACK_OVERFLOW', 'map running'), ('crash stack', 'STACK_OVERFLOW', 'no map'),
                ('crash', 'ACCESS_VIOLATION', 'no map'), ('reload', None, None),
                ('crash stack', 'STACK_OVERFLOW', 'map running')]
        for command, expected, state in plan:
            if not ready or process.poll() is not None:
                break
            if command == 'reload':
                (game / 'probe_map.cfg').write_text(load + '\n', encoding='utf-8')
                reply = rcon(port, 'exec probe_map.cfg', 5.0)
                ready = await_map(process, port, read, 60)
                steps.append(dict(step='map again', ok=ready))
                replies.append(f'==== exec probe_map.cfg\n{reply}')
                continue
            # An rcon command's prints, its ERR_DROP's included, go to the rcon
            # client rather than the log while the redirect lasts.
            logged = len(drops(read()))
            reply = rcon(port, command, 3.0)
            time.sleep(2)
            alive = process.poll() is None
            found = drops(reply) or drops(read())[logged:]
            message = found[0] if found else None
            row = fault_row(message, alive, expected, process.poll())
            faults += 1
            row['answersRconAfter'] = alive and await_echo(process, port, f'alive{faults}', 10)
            if not row['answersRconAfter'] and row['outcome'] == 'reported':
                row['outcome'] = 'unresponsive'
            row.update(step=f'{command} ({state})', reportIn='rcon reply' if drops(reply) else 'log')
            replies.append(f'==== {command} ({state})\n{reply}')
            steps.append(row)
        if process.poll() is None:
            rcon(port, 'quit', 1.0)
            try:
                quit_code = process.wait(30)
                steps.append(dict(step='quit', ok=quit_code == 0, exitCode=quit_code))
            except subprocess.TimeoutExpired:
                steps.append(dict(step='quit', ok=False))
    finally:
        stop(process)
    (output / f'{name}-console.log').write_text(read(), encoding='utf-8')
    (output / f'{name}-rcon.log').write_text('\n'.join(replies) + '\n', encoding='utf-8')
    return dict(steps=steps, quitExitCode=quit_code, elapsedSeconds=round(time.monotonic() - begin, 2))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(GAMES), default='q3')
    parser.add_argument('--map', default='q3dm1', help='map name inside the paks, e.g. q3dm17')
    parser.add_argument('--port', type=int, default=29170, help='loopback UDP port for the dedicated server')
    parser.add_argument('--only', help='comma-separated scenario names to run')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client, server = args.client.resolve(strict=True), args.server.resolve(strict=True)
    installation = args.installation.resolve(strict=True)
    load = f'map {args.map} ffa' if args.profile == 'ql' else f'map {args.map}'
    wanted = set(args.only.split(',')) if args.only else set(CLIENT_SCENARIOS) | {'server-sequence'}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    hashes = dict(clientSha256=sha256(client), serverSha256=sha256(server))
    report = dict(profile=args.profile, map=args.map, client=str(client), server=str(server),
                  installation=str(installation), **hashes, scenarios={})
    for name in CLIENT_SCENARIOS:
        if name in wanted:
            row = run_client(name, client, installation, args.profile, load, output)
            report['scenarios'][name] = row
            print(name, row['outcome'], row['reportedCode'], row['address'], flush=True)
    if 'server-sequence' in wanted:
        sequence = run_server(server, installation, args.profile, load, args.port, output)
        report['scenarios']['server-sequence'] = sequence
        for step in sequence['steps']:
            print('server-sequence', step['step'], step.get('outcome', step.get('ok')), step.get('reportedCode', ''),
                  step.get('address', ''), flush=True)
    report['binariesUnchanged'] = hashes == dict(clientSha256=sha256(client), serverSha256=sha256(server))
    report['sourceCommit'] = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                            capture_output=True).stdout.strip()
    faults = [row for row in report['scenarios'].values() if 'outcome' in row]
    faults += [step for step in report['scenarios'].get('server-sequence', {}).get('steps', []) if 'outcome' in step]
    report['outcomes'] = {o: sum(row['outcome'] == o for row in faults) for o in OUTCOMES}
    sequence_ok = all(step.get('ok', True) for step in report['scenarios'].get('server-sequence', {}).get('steps', []))
    report['passed'] = bool(faults) and all(row['outcome'] == 'reported' for row in faults) and sequence_ok \
        and report['binariesUnchanged']
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report['outcomes']), 'passed' if report['passed'] else 'FAILED')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
