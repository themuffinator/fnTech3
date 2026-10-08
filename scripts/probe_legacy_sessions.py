#!/usr/bin/env python3
"""Legacy multiplayer sessions: join retail servers and host retail clients.

For each multiplayer profile with a retail peer that runs on this machine, one
client plays a scripted session on one server over UDP on 127.0.0.1:

- ``join``: the fnTech3 client on the profile's retail dedicated server;
- ``host``: the profile's retail client on an fnTech3 dedicated server;
- ``listen``: the retail client on an fnTech3 client hosting the game;
- ``control``: the retail client on the retail server, to tell an engine's
  behaviour from the scenario's.

The main session is pure by default and runs: join, a chat line each way, a
change to a second map, an in-place ``map_restart``, a client ``reconnect``,
and the server quitting under the client. A second, short session joins and is
kicked. Each step waits for the server's own game log (``ClientBegin`` for the
probe client's slot, its chat line) or the client's console log (the server's
chat line, the disconnect message), and the run records how far it got.

The server is driven by rcon ``exec`` of a cfg written into its home, so its
console logs every command. The client is driven by its own console: its
startup cfg loops ``exec probecmd<N>.cfg`` and each command file the probe
writes ends by pointing the loop at the next number, so every step runs once.
No mouse or keyboard input is injected and no operating-system capture is
taken. Windows only: every process runs windowed on a private desktop with an
isolated home, bound to 127.0.0.1, without sound. Installations stay read
only; their inventories are compared before and after. Retail Jedi Academy's
client needs its disc to connect, so that profile runs ``join`` only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import random
import re
import secrets
import socket
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'scripts'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402
from probe_legacy_discovery import INSTALLS, stage_retail  # noqa: E402

OOB = b'\xff\xff\xff\xff'
NAME = 'fnprobe'
# The second client of a mixed session.
NAME2 = 'fnprobe2'

PROFILES = {
    'q3': dict(game='baseq3', maps=('q3dm17', 'q3dm6'), protocol=68, gametype='0',
               retailServer='quake3.exe', retailClient='quake3.exe', retailLog='qconsole.log',
               clientSettings=dict(cl_motd='0')),
    'rtcw-mp': dict(game='main', maps=('mp_beach', 'mp_ice'), protocol=60, gametype='5',
                    retailServer='WolfMP.exe', retailClient='WolfMP.exe', retailLog='rtcwconsole.log',
                    serverSettings=dict(sv_punkbuster='0'),
                    clientSettings=dict(cl_motd='0', cl_autoupdate='0', cl_punkbuster='0',
                                        com_recommendedSet='1')),
    'wolfet': dict(game='etmain', maps=('railgun', 'battery'), protocol=84, gametype='2',
                   retailServer='etded.exe', retailClient='et.exe', retailLog='etconsole.log',
                   serverSettings=dict(sv_punkbuster='0'),
                   clientSettings=dict(cl_motd='0', cl_autoupdate='0', cl_punkbuster='0',
                                       com_recommendedSet='1'),
                   # Retail ET's kick is its game's, and bans the address for a while.
                   kickBans=True,
                   # ET's game counts a warm-up down after each map load, then
                   # restarts the map into play (gamestate 2, 1, then 0). Retail
                   # etded ignores every message a client sends with the old
                   # server id unless it is already downloading, so a client
                   # that asks for a file just before that restart is never
                   # answered: clients start once the server plays.
                   playingCvar='gamestate'),
    'stvef-mp': dict(game='BaseEF', maps=('hm_borg1', 'hm_voy1'), protocol=24, gametype='0',
                     retailServer='stvoyHM.exe', retailClient='stvoyHM.exe', retailLog='qconsole.log',
                     layout='stvef', developerLater=True, clientSettings=dict(cl_motd='0'),
                     # Raven's engines name their console chat svsay and have no say;
                     # Elite Force 1.20 kicks by name and has no clientkick.
                     consoleSay='svsay', kickCommand='kick {name}',
                     # Retail asks Raven's dead authorize server about a
                     # non-LAN client and waits it out (about 20 s).
                     connectSeconds=60),
    # Q3Test 1.08 (layout q3test): no home path, so its writable base is the
    # run's own folder and its paks come from the release's runtime slot.
    'q3test': dict(game='demoq3', maps=('q3test1', 'q3test2'), protocol=40, gametype='0',
                   retailServer='quake3.exe', retailClient='quake3.exe', retailLog='qconsole.log',
                   layout='q3test', release='1.08', clientSettings=dict(cl_motd='0')),
    'jka-mp': dict(game='base', maps=('mp/ffa1', 'mp/ffa3'), protocol=26, gametype='0',
                   retailServer='jamp.exe', retailClient=None, retailLog='qconsole.log', layout='jka',
                   consoleSay='svsay',
                   clientSettings=dict(cl_motd='0', com_introplayed='1')),
}
DIRECTIONS = ('join', 'host', 'listen', 'control')
# From a client's slot to its entering the game: a retail ET client loading
# a map on a busy machine took 22 s.
BEGIN_SECONDS = 40


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def read(path):
    return path.read_text(encoding='latin1', errors='replace') if path and path.exists() else ''


def command(executable, settings, tail=()):
    result = [str(executable)]
    for key, value in settings.items():
        result.extend(['+set', key, str(value)])
    return result + list(tail)


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def query(port, request, timeout=0.5):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        try:
            sock.sendto(OOB + request.encode('latin1'), ('127.0.0.1', port))
            return sock.recvfrom(65535)[0].decode('latin1', errors='replace')
        except (socket.timeout, ConnectionResetError, OSError):
            return None


def wait_for(predicate, timeout, step=0.2):
    end = time.monotonic() + timeout
    while True:
        value = predicate()
        if value or time.monotonic() >= end:
            return value
        time.sleep(step)


class Peer:
    """A started process with its writable home and console log."""

    def __init__(self, role, executable, cwd, settings, tail, home, log_dir, log_name, run_dir):
        self.role, self.home, self.log_dir, self.log_name = role, home, log_dir, log_name
        self.command = command(executable, settings, tail)
        self.sha256 = sha(executable)
        self.process = WindowsPrivateProcess(self.command, cwd, run_dir / f'{role}-stdout.log')
        self.exit = None
        self.forced = False

    def log(self):
        found = [p for p in self.log_dir.rglob(self.log_name) if p.is_file()] if self.log_dir.exists() else []
        return read(max(found, key=lambda p: p.stat().st_mtime)) if found else ''

    def game_log(self):
        found = [p for p in self.log_dir.rglob('games.log') if p.is_file()] if self.log_dir.exists() else []
        return '\n'.join(read(p) for p in found)

    def alive(self):
        return self.process.poll() is None

    def stop(self, grace=10):
        wait_for(lambda: not self.alive(), grace)
        self.exit = self.process.poll()
        self.stop_error = None
        try:
            if self.exit is None:
                self.forced = bool(self.process.terminate())
                # A client tearing down its renderer can take a while to go.
                wait_for(lambda: not self.alive(), 15)
                self.exit = self.process.poll()
            self.process.close()
        except OSError as error:
            # Windows refuses to stop a child whose own exit is under way; a
            # retail Steam executable can sit in that teardown for a while.
            self.stop_error = repr(error)
            wait_for(lambda: not self.alive(), 30, 0.5)
            self.exit = self.process.poll()
            try:
                self.process.close()
            except OSError as again:
                self.stop_error += ' / ' + repr(again)


class ClientDriver:
    """Feeds a running client one command file at a time."""

    def __init__(self, folder):
        self.folder = folder
        self.number = 1

    def startup(self, lines):
        poll = 'set probe_poll "exec probecmd1.cfg; wait 15; vstr probe_poll"'
        (self.folder / 'probestart.cfg').write_text('\n'.join(lines + [poll, 'vstr probe_poll']) + '\n',
                                                    encoding='latin1')

    def send(self, *lines):
        following = self.number + 1
        body = list(lines) + [f'set probe_poll "exec probecmd{following}.cfg; wait 15; vstr probe_poll"']
        temporary = self.folder / f'probecmd{self.number}.tmp'
        temporary.write_text('\n'.join(body) + '\n', encoding='latin1')
        os.replace(temporary, self.folder / f'probecmd{self.number}.cfg')
        self.number = following


def slot_of(game_log, name):
    found = re.findall(r'ClientUserinfoChanged: (\d+) n\\' + re.escape(name) + r'\\', game_log)
    if found:
        return int(found[-1])
    # Retail Jedi Academy's game logs no userinfo; its engine's developer
    # output names the client entering the slot the game connected.
    found = re.findall(r'ClientConnect: (\d+)\s*\n(?:[^\n]*\n){0,2}?Going from CS_FREE to CS_CONNECTED for '
                       + re.escape(name) + r'\b', game_log)
    return int(found[-1]) if found else None


def begins(game_log, slot):
    return len(re.findall(r'ClientBegin: %d\b' % slot, game_log)) if slot is not None else 0


BOOT_KEYS = {
    # Read before any startup cfg runs: the filesystem and network, the
    # window and input, and the retail clients' startup update and MOTD checks.
    'com_gameProfile', 'fs_basepath', 'fs_homepath', 'fs_cdpath', 'fs_game', 'dedicated', 'developer', 'logfile',
    'net_ip', 'net_port', 'net_enabled', 'r_fullscreen', 'r_mode', 'in_mouse', 'in_joystick', 'in_nograb',
    's_initsound', 'com_maxfps', 'com_maxfpsUnfocused', 'com_maxfpsMinimized', 'com_steamIntegration',
    'cl_motd', 'cl_autoupdate', 'com_introplayed', 'com_recommendedSet', 'cl_punkbuster', 'sv_punkbuster',
}


def split_settings(settings):
    """Bootstrap cvars stay on the command line; fnTech3 refuses more than 32
    startup commands and retail keeps only 31, so the rest go in a cfg."""
    boot = {k: v for k, v in settings.items() if k in BOOT_KEYS}
    rest = [f'set {k} "{v}"' for k, v in settings.items() if k not in BOOT_KEYS]
    return boot, rest


def server_settings(profile, port, password, pure):
    return dict(dedicated='1', developer='1', logfile='2', net_ip='127.0.0.1', net_port=str(port),
                net_enabled='1', sv_master1='', sv_master2='', sv_master3='', sv_master4='', sv_master5='',
                rconpassword=password, sv_pure='1' if pure else '0', g_gametype=profile['gametype'],
                g_log='games.log', g_logSync='1', bot_enable='0', sv_maxclients='8', g_doWarmup='0',
                g_warmup='0', timelimit='0', fraglimit='0', g_inactivity='0', sv_timeout='60',
                sv_hostname='fntech3-legacy-session', **profile.get('serverSettings', {}))


def start_server(run, kind, key, profile, installation, runtime, layout, pure, mod=None):
    port, password = free_port(), secrets.token_hex(6)
    home = run.dir / 'server-home'
    home.mkdir()
    settings = server_settings(profile, port, password, pure)
    if mod:
        settings.update(fs_game=mod, sv_allowDownload='1')
    if kind == 'retail':
        exe, cwd = layout['root'] / profile['retailServer'], layout['root']
        if profile.get('developerLater'):
            del settings['developer']
        if profile.get('layout') in ('stvef', 'q3test'):
            base = run.dir / 'sb'
            base.mkdir()
            settings = dict(fs_basepath=str(base), fs_cdpath=str(layout['cdpath']), **settings)
            log_dir, cfg_dir = base, base / profile['game']
        else:
            settings = dict(fs_basepath=str(layout['root']), fs_homepath=str(home), **settings)
            if layout['cdpath']:
                settings['fs_cdpath'] = str(layout['cdpath'])
            log_dir, cfg_dir = home, home / profile['game']
        log_name = profile['retailLog']
    else:
        listen = kind == 'listen'
        exe = runtime / ('fntech3.exe' if listen else 'fntech3.ded.exe')
        cwd = runtime
        settings = dict(com_gameProfile=key, fs_basepath=str(installation), fs_homepath=str(home),
                        com_steamIntegration='0', **settings)
        settings.update(FNTECH3_SETTINGS)
        if listen:
            settings.update(dedicated='0', r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0',
                            in_nograb='1', s_initsound='0', name='listenhost', com_maxfps='60',
                            com_maxfpsUnfocused='60', com_maxfpsMinimized='60', cl_motd='0')
        log_dir, cfg_dir, log_name = home, home / profile['game'], 'qconsole.log'
    boot, rest = split_settings(settings)
    cfg_dir.mkdir(parents=True, exist_ok=True)
    (cfg_dir / 'probeserver.cfg').write_text('\n'.join(rest) + '\n', encoding='latin1')
    # The mod folder sits beside the base game wherever this server writes.
    fixture = write_fixture(cfg_dir.parent / mod / PAK) if mod else None
    peer = Peer('server', exe, cwd, boot, ['+exec', 'probeserver.cfg', '+map', profile['maps'][0]],
                home, log_dir, log_name, run.dir)
    peer.port, peer.password, peer.kind, peer.cfg_dir = port, password, kind, cfg_dir
    peer.settings, peer.fixture = settings, fixture
    return peer


def start_client(run, kind, key, profile, installation, runtime, layout, port, extra_lines=(), download=False,
                 name=NAME, role='client'):
    home = run.dir / f'{role}-home'
    home.mkdir()
    settings = dict(r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                    s_initsound='0', developer='1', logfile='2', net_ip='127.0.0.1', net_port='0',
                    name=name, cl_allowDownload='1' if download else '0', com_maxfps='60', rate='25000',
                    **profile.get('clientSettings', {}))
    lines = []
    if kind == 'retail':
        exe, cwd = layout['root'] / profile['retailClient'], layout['root']
        if profile.get('layout') in ('stvef', 'q3test'):
            base = run.dir / ('cb' if role == 'client' else 'cb2')
            base.mkdir()
            settings = dict(fs_basepath=str(base), fs_cdpath=str(layout['cdpath']), **settings)
            log_dir, cfg_dir = base, base / profile['game']
        else:
            settings = dict(fs_basepath=str(layout['root']), fs_homepath=str(home), **settings)
            if layout['cdpath']:
                settings['fs_cdpath'] = str(layout['cdpath'])
            log_dir, cfg_dir = home, home / profile['game']
        log_name = profile['retailLog']
        if profile.get('developerLater'):
            del settings['developer']
            lines = ['wait 30', 'developer 1']
    else:
        exe, cwd = runtime / 'fntech3.exe', runtime
        settings = dict(com_gameProfile=key, fs_basepath=str(installation), fs_homepath=str(home),
                        com_steamIntegration='0', net_enabled='1', com_maxfpsUnfocused='60',
                        com_maxfpsMinimized='60', **settings)
        settings.update(FNTECH3_SETTINGS)
        log_dir, cfg_dir, log_name = home, home / profile['game'], 'qconsole.log'
    boot, rest = split_settings(settings)
    cfg_dir.mkdir(parents=True, exist_ok=True)
    driver = ClientDriver(cfg_dir)
    driver.startup(rest + lines + list(extra_lines) + [f'connect 127.0.0.1:{port}'])
    peer = Peer(role, exe, cwd, boot, ['+exec', 'probestart.cfg'], home, log_dir, log_name, run.dir)
    peer.driver, peer.kind, peer.settings = driver, kind, settings
    return peer


def server_exec(server, step, lines):
    """Run lines on the server from a cfg, so its console logs them."""
    name = f'probesrv_{step}.cfg'
    (server.cfg_dir / name).write_text('\n'.join(lines) + '\n', encoding='latin1')
    reply = None
    for _ in range(3):
        reply = query(server.port, f'rcon {server.password} exec {name}', timeout=1.5)
        if reply and 'exec' in reply:
            break
        time.sleep(1.1)
    time.sleep(1.1)  # retail ET drops an rcon within 500 ms of the last
    return reply


class Run:
    def __init__(self, output, name):
        self.dir = output / name
        self.dir.mkdir(parents=True, exist_ok=False)
        self.steps = []
        self.started = time.monotonic()

    def step(self, name, passed, **detail):
        self.steps.append(dict(step=name, passed=bool(passed),
                               seconds=round(time.monotonic() - self.started, 2), **detail))
        print(f'   {name}: {"ok" if passed else "FAILED"} {detail if not passed else ""}', flush=True)
        return passed


def server_ready(server, map_name):
    def ready():
        reply = query(server.port, 'getinfo probe')
        return reply if reply and ('\\mapname\\' + map_name) in reply else None
    return wait_for(ready, 60, 0.5)


def server_playing(server, cvar):
    """Waits until the server's game reports itself playing (0) by rcon."""
    def playing():
        reply = query(server.port, f'rcon {server.password} {cvar}', timeout=1.0)
        found = re.search(r'"%s" is:"(-?\d+)' % re.escape(cvar), reply or '')
        return found.group(1) == '0' if found else None
    return wait_for(playing, 90, 1.2)


def disconnect_evidence(client, marker, step):
    """What the client recorded about a server-side disconnect.

    A retail client keeps running its console script after the server drops
    it, so it is asked to print com_errorMessage. This engine clears the
    command buffer in every Com_Error, a server disconnect included
    (common.c, as Quake3e), so its script ends there: its own log of the
    disconnect command is the evidence, and the process must survive it."""
    if client.kind == 'retail':
        client.driver.send(f'set probe_mark {step}', 'probe_mark', 'com_errorMessage')
        wait_for(lambda: step in client.log()[marker:], 20)
        tail = client.log()[marker:]
        message = re.findall(r'"com_errorMessage" is:"([^"]*)"', tail)
        # A developer client also logs each command the server sent it;
        # Elite Force's has no com_errorMessage, so these are its evidence.
        received = re.findall(r'serverCommand: \d+ : (disconnect[^\n]*)', tail)
        printed = re.findall(r'serverCommand: \d+ : print "([^"\n]*)', tail)
        return dict(answered=step in tail, clientError=message[-1] if message else None,
                    received=received[-1] if received else None, printed=printed[-1] if printed else None)
    # The disconnect command, or what the game's client module prints for it
    # (ET's cgame prints "[skipnotify]Server quit" and logs no command line).
    told = re.compile(r'disconnect|server quit|kicked|server disconnected|server shutdown', re.I)
    wait_for(lambda: told.search(client.log()[marker:]), 15)
    time.sleep(3)
    tail = client.log()[marker:]
    reason = re.findall(r'serverCommand: \d+ : (disconnect[^\n]*)', tail)
    printed = re.findall(r'(?:serverCommand: \d+ : print "|\[skipnotify\])([^"\n]*)', tail)
    return dict(answered=bool(told.search(tail)) and client.alive(), clientError=reason[-1] if reason else None,
                printed=printed[-1] if printed else None, survived=client.alive())


def main_session(run, server, client, profile):
    """The pure session's steps; returns the probe client's slot."""
    connect_seconds = profile.get('connectSeconds', 40)
    game = lambda: server.game_log() + server.log()
    slot = wait_for(lambda: slot_of(game(), NAME), connect_seconds)
    joined = slot is not None and wait_for(lambda: begins(game(), slot) >= 1, BEGIN_SECONDS)
    if not run.step('join', joined, slot=slot):
        return slot
    nonce = secrets.token_hex(4)
    client.driver.send(f'say probe-client-{nonce}')
    run.step('client-chat', wait_for(lambda: f'probe-client-{nonce}' in game(), 15))
    # A dedicated server's console chat, or the hosting player's own on a listen server.
    say = 'say' if server.kind == 'listen' else profile.get('consoleSay', 'say')
    server_exec(server, 'say', [f'{say} probe-server-{nonce}'])
    run.step('server-chat', wait_for(lambda: f'probe-server-{nonce}' in client.log(), 15))
    base = begins(game(), slot)
    server_exec(server, 'map', [f'map {profile["maps"][1]}'])
    changed = wait_for(lambda: begins(game(), slot) > base, 60)
    run.step('map-change', changed, map=profile['maps'][1])
    if not changed:
        return slot
    base = begins(game(), slot)
    server_exec(server, 'restart', ['map_restart 0'])
    run.step('map-restart', wait_for(lambda: begins(game(), slot) > base, 40))
    # The client connects again while connected. `reconnect` would append the
    # connect behind the command loop, which never yields to it.
    base = len(re.findall(r'ClientBegin: \d+', game()))
    client.driver.send(f'connect 127.0.0.1:{server.port}')
    rejoined = wait_for(lambda: len(re.findall(r'ClientBegin: \d+', game())) > base
                        and slot_of(game(), NAME) is not None, connect_seconds + 20)
    slot = slot_of(game(), NAME)
    run.step('reconnect', rejoined, slot=slot)
    marker = len(client.log())
    query(server.port, f'rcon {server.password} quit', timeout=1.0)
    server_quit = wait_for(lambda: not server.alive(), 20)
    evidence = disconnect_evidence(client, marker, 'afterquit')
    run.step('server-quit', server_quit and evidence['answered'], **evidence)
    return slot


def mixed_session(run, server, clients, profile):
    """A retail and an fnTech3 client on one server: each must enter, see the
    other's chat line, follow a map change and see the other again."""
    connect_seconds = profile.get('connectSeconds', 40)
    game = lambda: server.game_log() + server.log()
    names = [NAME, NAME2]
    slots = [wait_for(lambda n=n: slot_of(game(), n), connect_seconds) for n in names]
    joined = all(s is not None for s in slots) and \
        wait_for(lambda: all(begins(game(), s) >= 1 for s in slots), 30)
    if not run.step('mixed-join', joined, slots=slots):
        return
    nonces = [secrets.token_hex(4) for _ in clients]
    for client, nonce in zip(clients, nonces):
        client.driver.send(f'say probe-mixed-{nonce}')
    heard = wait_for(lambda: f'probe-mixed-{nonces[1]}' in clients[0].log()
                     and f'probe-mixed-{nonces[0]}' in clients[1].log(), 20)
    run.step('mixed-chat', heard)
    base = [begins(game(), s) for s in slots]
    server_exec(server, 'map', [f'map {profile["maps"][1]}'])
    changed = wait_for(lambda: all(begins(game(), s) > b for s, b in zip(slots, base)), 90)
    run.step('mixed-map-change', changed)
    if not changed:
        return
    # A server's flood protection (sv_floodProtect, on in every retail game)
    # drops a second reliable command within a second, and an entering client
    # has just sent its own: wait that out, and say it again once if lost.
    heard = False
    for _ in range(2):
        time.sleep(2.5)
        nonces = [secrets.token_hex(4) for _ in clients]
        for client, nonce in zip(clients, nonces):
            client.driver.send(f'say probe-mixed-{nonce}')
        heard = wait_for(lambda: f'probe-mixed-{nonces[1]}' in clients[0].log()
                         and f'probe-mixed-{nonces[0]}' in clients[1].log(), 10)
        if heard:
            break
    run.step('mixed-chat-after-change', heard)


# --hold: how long a sustained session keeps its two clients on the server.
HOLD_SECONDS = 300


def sustained_session(run, server, clients, profile):
    """A retail and an fnTech3 client share a server for HOLD_SECONDS: every
    20 s each says a line the other must hear, a lost round being said once
    more, and the map changes halfway."""
    connect_seconds = profile.get('connectSeconds', 40)
    game = lambda: server.game_log() + server.log()
    names = [NAME, NAME2]
    slots = [wait_for(lambda n=n: slot_of(game(), n), connect_seconds) for n in names]
    joined = all(s is not None for s in slots) and \
        wait_for(lambda: all(begins(game(), s) >= 1 for s in slots), 30)
    if not run.step('sustained-join', joined, slots=slots):
        return
    rounds = max(2, HOLD_SECONDS // 20)
    begin = time.monotonic()
    heard_rounds, retried, missed, changed = 0, 0, [], None
    # Rounds in which the second client heard the first, and the reverse.
    heard_by_second, heard_by_first = 0, 0
    for number in range(rounds):
        if number == rounds // 2:
            base = [begins(game(), s) for s in slots]
            server_exec(server, 'map', [f'map {profile["maps"][1]}'])
            changed = wait_for(lambda: all(begins(game(), s) > b for s, b in zip(slots, base)), 90)
            if not changed:
                break
        # Pace the rounds 20 s apart; flood protection drops a second
        # reliable command within a second of another.
        time.sleep(max(2.5, begin + 20 * (number + 1) - time.monotonic()))
        heard = False
        second, first = False, False
        for attempt in range(2):
            nonces = [secrets.token_hex(4) for _ in clients]
            for client, nonce in zip(clients, nonces):
                client.driver.send(f'say probe-hold-{nonce}')
            heard = wait_for(lambda: f'probe-hold-{nonces[1]}' in clients[0].log()
                             and f'probe-hold-{nonces[0]}' in clients[1].log(), 10)
            second = second or f'probe-hold-{nonces[0]}' in clients[1].log()
            first = first or f'probe-hold-{nonces[1]}' in clients[0].log()
            if heard:
                retried += attempt
                break
            time.sleep(2.5)
        heard_by_second += second
        heard_by_first += first
        if heard:
            heard_rounds += 1
        else:
            missed.append(number)
    run.step('sustained-map-change', bool(changed))
    run.step('sustained-chat', heard_rounds == rounds, rounds=rounds, heard=heard_rounds,
             heardBySecond=heard_by_second, heardByFirst=heard_by_first,
             retried=retried, missed=missed, held=round(time.monotonic() - begin, 1))


def kick_session(run, server, client, profile):
    connect_seconds = profile.get('connectSeconds', 40)
    game = lambda: server.game_log() + server.log()
    slot = wait_for(lambda: slot_of(game(), NAME), connect_seconds)
    joined = slot is not None and wait_for(lambda: begins(game(), slot) >= 1, BEGIN_SECONDS)
    if not run.step('kick-join', joined, slot=slot):
        return
    marker = len(client.log())
    server_exec(server, 'kick', [profile.get('kickCommand', 'clientkick {slot}').format(slot=slot, name=NAME)])
    gone = wait_for(lambda: NAME not in (query(server.port, 'getstatus probe') or '').split('\n', 2)[-1], 15)
    evidence = disconnect_evidence(client, marker, 'afterkick')
    run.step('kick', gone and evidence['answered'], **evidence)


MOD = 'fntech3probe'
# --fntech3-set: cvars for fnTech3's server and clients only, such as
# net_dropsim, which drops that share of the packets an fnTech3 process
# receives; retail peers have no such setting.
FNTECH3_SETTINGS = {}
# --payload-bytes: the size of the generated pak's one file.
PAYLOAD_BYTES = 32768
# Retail RTCW MP loads a zz_ pak under an mp_ name it does not have
# (FS_AddGameDirectory's sort), so the fixture avoids mp_, sp_ and zz_.
PAK = 'fntech3probe.pk3'


def write_fixture(path, seed=20261005):
    """A deterministic pak holding only generated bytes, no game content."""
    generator = random.Random(seed)
    payload = bytes(generator.randrange(256) for _ in range(PAYLOAD_BYTES))
    path.parent.mkdir(parents=True, exist_ok=True)
    info = zipfile.ZipInfo('fntech3_probe_payload.dat', (2026, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_STORED
    with zipfile.ZipFile(path, 'w') as archive:
        archive.writestr(info, payload)
    return dict(seed=seed, payloadBytes=len(payload), bytes=path.stat().st_size, sha256=sha(path))


def download_session(run, server, client, profile, recipe):
    """A pak in the server's mod folder counts as referenced, so the client
    must fetch it before it may enter; it then plays pure inside the mod."""
    connect_seconds = profile.get('connectSeconds', 40) + 60
    game = lambda: server.game_log() + server.log()
    slot = wait_for(lambda: slot_of(game(), NAME), connect_seconds)
    joined = slot is not None and wait_for(lambda: begins(game(), slot) >= 1, 40)
    downloaded = [p for p in client.log_dir.rglob(PAK) if p.is_file()]
    exact = any(sha(p) == recipe['sha256'] for p in downloaded)
    leftovers = [str(p) for p in client.log_dir.rglob(PAK + '.tmp')] + \
        [str(p) for p in client.log_dir.rglob('*.tmp') if PAK[:-4] in p.name]
    text = server.log() + client.log()
    run.step('download', joined and exact and not leftovers, slot=slot,
             files=[str(p.relative_to(client.log_dir)) for p in downloaded], leftovers=leftovers,
             serverLoggedTransfer=bool(re.search(re.escape(PAK), server.log())),
             unpure='Unpure client detected' in text)


def run_direction(output, key, direction, runtime, args):
    profile = PROFILES[key]
    installation = INSTALLS[key]
    layout = stage_retail(key, profile, installation)
    server_kind = {'join': 'retail', 'control': 'retail', 'host': 'fntech3', 'listen': 'listen'}[direction]
    client_kind = {'join': 'fntech3', 'control': 'retail', 'host': 'retail', 'listen': 'retail'}[direction]
    before = inventory(installation)
    result = dict(profile=key, direction=direction, serverKind=server_kind, clientKind=client_kind,
                  maps=list(profile['maps']), pure=not args.no_pure, sessions=[])
    for session in args.sessions:
        # Mixed and sustained sessions put a retail and an fnTech3 client on one server.
        if session in ('mixed', 'sustained') and (direction not in ('join', 'host') or not profile.get('retailClient')):
            continue
        run = Run(output, f'{key}-{direction}-{session}')
        mod = MOD if session == 'download' else None
        server = start_server(run, server_kind, key, profile, installation, runtime, layout,
                              pure=not args.no_pure and session != 'kick', mod=mod)
        client = None
        others = []
        record = dict(session=session, serverCommand=server.command, serverSha256=server.sha256)
        if mod:
            record['fixture'] = server.fixture
        try:
            if not run.step('server-ready', server_ready(server, profile['maps'][0])):
                continue
            if profile.get('playingCvar') and not run.step('server-playing',
                                                           server_playing(server, profile['playingCvar'])):
                continue
            client = start_client(run, client_kind, key, profile, installation, runtime, layout, server.port,
                                  download=bool(mod))
            record.update(clientCommand=client.command, clientSha256=client.sha256)
            if session == 'main':
                main_session(run, server, client, profile)
            elif session == 'kick':
                kick_session(run, server, client, profile)
            elif session in ('mixed', 'sustained'):
                other = 'retail' if client_kind == 'fntech3' else 'fntech3'
                others.append(start_client(run, other, key, profile, installation, runtime, layout,
                                           server.port, name=NAME2, role='client2'))
                record.update(secondClientCommand=others[0].command, secondClientSha256=others[0].sha256)
                (mixed_session if session == 'mixed' else sustained_session)(run, server, [client] + others, profile)
            else:
                download_session(run, server, client, profile, server.fixture)
        except (OSError, RuntimeError) as error:
            record['error'] = repr(error)
        finally:
            for second in others:
                if second.alive():
                    second.driver.send('quit')
                second.stop(15)
                record.update(secondClientExit=second.exit, secondClientForced=second.forced)
                (run.dir / 'client2-console.log').write_text(second.log(), encoding='latin1')
            if client:
                if client.alive():
                    client.driver.send('quit')
                client.stop(15)
                record.update(clientExit=client.exit, clientForced=client.forced, clientStopError=client.stop_error)
                (run.dir / 'client-console.log').write_text(client.log(), encoding='latin1')
            if server.alive():
                query(server.port, f'rcon {server.password} quit', timeout=1.0)
            server.stop(15)
            record.update(serverExit=server.exit, serverForced=server.forced, serverStopError=server.stop_error)
            (run.dir / 'server-console.log').write_text(server.log(), encoding='latin1')
            (run.dir / 'server-games.log').write_text(server.game_log(), encoding='latin1')
            record['steps'] = run.steps
            result['sessions'].append(record)
    result['installationUnchanged'] = before == inventory(installation)
    # A retail client quits from its script. This engine's script ends with the
    # server's disconnect (see disconnect_evidence), so the probe stops it.
    def clean(session):
        if client_kind == 'retail':
            return session.get('clientExit') == 0 and not session.get('clientForced')
        return session.get('clientExit') is not None
    result['passed'] = all(step['passed'] for s in result['sessions'] for step in s['steps']) and \
        result['installationUnchanged'] and all(clean(s) for s in result['sessions'])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', default=list(PROFILES), choices=list(PROFILES))
    parser.add_argument('--directions', nargs='+', default=list(DIRECTIONS), choices=list(DIRECTIONS))
    parser.add_argument('--runtime', type=Path, required=True, help='fnTech3 runtime folder')
    parser.add_argument('--no-pure', action='store_true')
    parser.add_argument('--sessions', nargs='+', default=['main', 'kick', 'download', 'mixed'],
                        choices=['main', 'kick', 'download', 'mixed', 'sustained'])
    parser.add_argument('--hold', type=int, default=HOLD_SECONDS,
                        help='seconds a sustained session keeps its two clients on the server')
    parser.add_argument('--fntech3-set', nargs=2, action='append', default=[], metavar=('CVAR', 'VALUE'),
                        help="a cvar for fnTech3's server and clients only, e.g. net_dropsim 20")
    parser.add_argument('--payload-bytes', type=int, default=PAYLOAD_BYTES,
                        help="size of the download session pak's generated file")
    args = parser.parse_args()
    FNTECH3_SETTINGS.update(dict(args.fntech3_set))
    if not 1 <= args.payload_bytes <= 64 << 20:
        parser.error('--payload-bytes must be 1 to 64 MiB')
    globals()['PAYLOAD_BYTES'] = args.payload_bytes
    if not 40 <= args.hold <= 6 * 3600:
        parser.error('--hold must be 40 s to 6 h')
    globals()['HOLD_SECONDS'] = args.hold
    if os.name != 'nt':
        parser.error('Windows only')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runtime = args.runtime.resolve(strict=True)
    results = []
    for key in args.profiles:
        for direction in args.directions:
            if direction in ('host', 'listen', 'control') and not PROFILES[key].get('retailClient'):
                continue
            print(f'== {key} {direction}', flush=True)
            result = run_direction(output, key, direction, runtime, args)
            results.append(result)
            (output / f'{key}-{direction}.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
            print(f'   passed={result["passed"]}', flush=True)
    summary = [dict(profile=r['profile'], direction=r['direction'], passed=r['passed'],
                    steps={f'{s["session"]}:{step["step"]}': step['passed']
                           for s in r['sessions'] for step in s['steps']},
                    exits={s['session']: (s.get('serverExit'), s.get('clientExit'), s.get('clientForced'))
                           for s in r['sessions']}) for r in results]
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
