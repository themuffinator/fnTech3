#!/usr/bin/env python3
"""Corrupt live netchannel traffic between an fnTech3 client and server.

One fnTech3 dedicated server runs for the whole probe. Each session starts a
fresh fnTech3 client that connects through a loopback relay. Connectionless
packets (the handshake) pass unchanged. Once the client is in the game, the relay
corrupts one seeded sequenced datagram in --every for --seconds, in both
directions or only the one given by --direction. Each corrupted datagram gets 1-8
random bytes, after the sequence number in half of them and anywhere in the
others. Each session ends in one of:

  connected  the client stayed in the game: its console tick loop kept running
  dropped    the client printed an engine error and was still running two
             seconds later (an error clears the tick loop)
  crashed    the client process died with a failure status
  hung       none of these

After every session the server must answer a direct getstatus query, and at the
end it must still be running with no crash. Windows only: windowed clients on
unswitched private desktops with sound and mouse/joystick input disabled,
loopback UDP only, console commands only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import random
import re
import select
import socket
import struct
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

GAMES = {'q3': 'baseq3', 'ql': 'baseq3', 'rtcw-mp': 'main'}
TICK = 'NET_FAULT_TICK'
ERROR = re.compile(r'^ERROR: .*$', re.M)


class CorruptingRelay:
    """Loopback relay; corrupts sequenced datagrams once `armed` is set."""

    def __init__(self, server_port, seed, every, directions):
        self.server = ('127.0.0.1', server_port)
        self.rng = random.Random(seed)
        self.every = every
        self.directions = directions
        self.armed = threading.Event()
        self.stop = threading.Event()
        self.front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.back = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.front.bind(('127.0.0.1', 0))
        self.back.bind(('127.0.0.1', 0))
        self.port = self.front.getsockname()[1]
        self.client = None
        self.error = None
        self.stats = {d: dict(forwarded=0, sequenced=0, corrupted=0) for d in ('to_server', 'to_client')}
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def _mangle(self, direction, data):
        stat = self.stats[direction]
        if len(data) < 4 or data[:4] == b'\xff' * 4:
            return data
        stat['sequenced'] += 1
        if not self.armed.is_set() or direction not in self.directions or self.rng.randrange(self.every):
            return data
        stat['corrupted'] += 1
        buffer = bytearray(data)
        start = 4 if self.rng.randrange(2) and len(buffer) > 4 else 0
        for _ in range(self.rng.randint(1, 8)):
            buffer[self.rng.randrange(start, len(buffer))] = self.rng.randrange(256)
        return bytes(buffer)

    def _run(self):
        try:
            while not self.stop.is_set():
                ready, _, _ = select.select([self.front, self.back], [], [], 0.01)
                for source in ready:
                    try:
                        data, address = source.recvfrom(65535)
                    except ConnectionResetError:
                        continue
                    if source is self.front:
                        if self.client is None:
                            self.client = address
                        if address == self.client:
                            self.back.sendto(self._mangle('to_server', data), self.server)
                            self.stats['to_server']['forwarded'] += 1
                    elif address == self.server and self.client:
                        self.front.sendto(self._mangle('to_client', data), self.client)
                        self.stats['to_client']['forwarded'] += 1
        except Exception as exc:
            self.error = repr(exc)

    def close(self):
        self.stop.set()
        self.thread.join(2)
        self.front.close()
        self.back.close()
        return dict(stats=self.stats, error=self.error)


def getstatus(port):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.settimeout(1.0)
        for _ in range(3):
            probe.sendto(b'\xff\xff\xff\xffgetstatus', ('127.0.0.1', port))
            try:
                data, _ = probe.recvfrom(65535)
                if data.startswith(b'\xff\xff\xff\xffstatusResponse'):
                    return True
            except (socket.timeout, ConnectionResetError):  # Windows reports an unreachable port as a reset
                continue
    return False


def read(path):
    return path.read_text(encoding='utf-8', errors='replace') if path.exists() else ''


def stop(process):
    try:
        process.close()
    except subprocess.TimeoutExpired:
        process.wait(60)
        process.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(GAMES), required=True)
    parser.add_argument('--map', required=True)
    parser.add_argument('--sessions', type=int, default=20)
    parser.add_argument('--every', type=int, default=8, help='corrupt one sequenced datagram in this many')
    parser.add_argument('--seconds', type=float, default=15)
    parser.add_argument('--seed', type=int, default=20260918)
    parser.add_argument('--direction', choices=('both', 'to_server', 'to_client'), default='both',
                        help='which direction to corrupt')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client, server = args.client.resolve(strict=True), args.server.resolve(strict=True)
    installation = args.installation.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    game = GAMES[args.profile]
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    server_home = output / 'server-home'
    (server_home / game).mkdir(parents=True)
    command = [str(server)]
    for key, value in dict(com_gameProfile=args.profile, fs_basepath=str(installation), fs_homepath=str(server_home),
                           dedicated='1', net_ip='127.0.0.1', net_port=str(port), sv_pure='0', bot_enable='0',
                           sv_punkbuster='0', logfile='2', sv_maxclients='4', developer='1',
                           **{f'sv_master{i}': '' for i in range(1, 6)}).items():
        command += ['+set', key, value]
    command += ['+map', args.map] + (['ffa'] if args.profile == 'ql' else [])
    report = dict(profile=args.profile, map=args.map, every=args.every, seconds=args.seconds, seed=args.seed,
                  direction=args.direction,
                  serverCommand=' '.join(command[1:]), serverSha256=hashlib.sha256(server.read_bytes()).hexdigest(),
                  clientSha256=hashlib.sha256(client.read_bytes()).hexdigest(), sessions={},
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip())
    host = WindowsPrivateProcess(command, server.parent, output / 'server-stdout.log')
    try:
        begin = time.monotonic()
        while time.monotonic() - begin < 90 and not getstatus(port):
            time.sleep(1)
        report['serverReady'] = getstatus(port)
        for session in range(args.sessions if report['serverReady'] else 0):
            relay = CorruptingRelay(port, args.seed * 1000 + session, args.every,
                                    ('to_server', 'to_client') if args.direction == 'both' else (args.direction,))
            home = output / f'client-{session:02d}'
            (home / game).mkdir(parents=True)
            # Connect before the tick loop starts: the loop re-queues itself ahead of later commands.
            (home / game / 'probe.cfg').write_text(f'connect 127.0.0.1:{relay.port}\n'
                                                   f'set tick "echo {TICK}; wait 100; vstr tick"\nvstr tick\n',
                                                   encoding='utf-8')
            ccommand = [str(client)]
            for key, value in dict(com_gameProfile=args.profile, fs_basepath=str(installation), fs_homepath=str(home),
                                   fs_game='', r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0',
                                   in_nograb='1', s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1',
                                   cl_webuiEnable='0', cl_renderer='glx', logfile='2', net_port='0',
                                   com_introplayed='1', cl_allowDownload='0', name=f'netfault{session}').items():
                ccommand += ['+set', key, value]
            ccommand += ['+exec', 'probe.cfg']
            log = home / game / 'qconsole.log'
            process = WindowsPrivateProcess(ccommand, client.parent, output / f'client-{session:02d}-stdout.log')
            started = time.monotonic()
            armed_at = error_at = None
            alive_after = False
            ticks = []
            try:
                while process.poll() is None and time.monotonic() - started < 60 + args.seconds:
                    text = read(log)
                    now = time.monotonic()
                    ticks.append((now, text.count(TICK)))
                    if armed_at is None and re.search(r'^CL_FirstSnapshot|^Com_TouchMemory', text, re.M):
                        relay.armed.set()
                        armed_at = now
                    if error_at is None and ERROR.search(text):
                        error_at = now
                    if error_at is not None and now - error_at >= 2:
                        alive_after = process.poll() is None
                        break
                    if armed_at is not None and now - armed_at >= args.seconds:
                        break
                    time.sleep(0.1)
                forced = process.poll() is None
                code = process.poll()
            finally:
                relay.armed.clear()
                stop(process)
            traffic = relay.close()
            text = read(log)
            (output / f'client-{session:02d}-console.log').write_text(text, encoding='utf-8')
            errors = ERROR.findall(text)
            final = text.count(TICK)
            last = ticks[-1][0] if ticks else 0
            earlier = max((count for at, count in ticks if at <= last - 3), default=final)
            if not forced and code != 0:
                outcome = 'crashed'
            elif errors and alive_after:
                outcome = 'dropped'
            elif armed_at is not None and not errors and forced and final > earlier:
                outcome = 'connected'
            else:
                outcome = 'hung' if armed_at is not None else 'never-active'
            report['sessions'][session] = dict(
                outcome=outcome, exitCode=code, forcedTermination=forced, errors=errors[:3],
                corruptedSeconds=round(time.monotonic() - armed_at, 1) if armed_at else None,
                traffic=traffic, serverAnswersAfter=getstatus(port), serverRunning=host.poll() is None)
            row = report['sessions'][session]
            print(session, outcome, row['errors'][:1], traffic['stats'], row['serverAnswersAfter'], flush=True)
            if host.poll() is not None:
                break
    finally:
        report['serverExitCode'] = host.poll()
        stop(host)
    server_log = read(server_home / game / 'qconsole.log')
    (output / 'server-console.log').write_text(server_log, encoding='utf-8')
    rows = report['sessions'].values()
    report['outcomes'] = {o: sum(r['outcome'] == o for r in rows)
                          for o in ('connected', 'dropped', 'crashed', 'hung', 'never-active')}
    report['serverDrops'] = re.findall(r'^[^\r\n]*(?:Illegible|dropped|timed out)[^\r\n]*$', server_log, re.M)[:20]
    report['passed'] = (report.get('serverReady') and report['serverExitCode'] is None
                        and all(r['serverAnswersAfter'] for r in rows)
                        and not report['outcomes']['crashed'] and not report['outcomes']['hung']
                        and not report['outcomes']['never-active'] and len(report['sessions']) == args.sessions)
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report['outcomes']), 'passed' if report['passed'] else 'FAILED')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
