#!/usr/bin/env python3
"""Legacy server discovery: master heartbeats, query replies and browser acceptance.

For each multiplayer profile this stands up a fake master on the loopback
address -- on the profile's own master port when that port is free -- and runs,
one server at a time:

- the server: an fnTech3 dedicated runtime, or the profile's retail server
  where one runs on this machine, as a public server (``dedicated 2``) that
  publishes to that master named without a port, so the server chooses the
  port itself. ``net_port`` is left unset, so it also binds its own default;
- the master's ``getinfo`` and ``getstatus`` queries of whatever heartbeat
  arrives, and the replies' keys;
- the profile's retail client, where one runs here, and the fnTech3 client
  asking that server for its info with ``ping``. Each prints ``ping time``
  with ``developer 1`` only once its CL_ServerInfoPacket accepts the reply,
  and ``Different game info packet`` or ``Different protocol info packet``
  when it drops it;
- the fnTech3 client asking the master for servers with ``globalservers``,
  again with the master named without a port;
- for RTCW and ET, an rcon ``gameCompleteStatus``, the command their game
  modules run when a match ends, after which a public server sends every
  master its status;
- an rcon ``quit``, after which the master records the shutdown heartbeat.

Everything binds 127.0.0.1 only, so no real master, authorize or update
service is reached. Windows only: every process runs windowed on a private
desktop with an isolated home, without sound or input. Installations are read
only and their inventories are compared before and after. Retail Elite Force
writes beside its base path, so it runs from a scratch layout under
``.tmp/runtime/stvef-retail``; retail Jedi titles run from copies under
``.tmp/runtime/jka-retail`` and ``.tmp/runtime/jk2-retail`` with the
installation as their CD path. Retail Jedi Knight II cannot host here: its
server checks for the game disc. Its client and retail Jedi Academy's do not
check until they connect, so ``ping`` still runs.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import secrets
import shutil
import socket
import struct
import sys
import threading
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

OOB = b'\xff\xff\xff\xff'
RUNTIME = ROOT / '.tmp' / 'runtime'

INSTALLS = {
    'q3': Path(r'C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena'),
    'rtcw-mp': Path(r'E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein'),
    'wolfet': Path(r'E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory'),
    'stvef-mp': Path(r'E:\Games\Star Trek Voyager Elite Force'),
    'jka-mp': Path(r'E:\Games\Jedi Academy'),
    'jk2-mp': Path(r'E:\Games\Jedi Outcast'),
}

# Retail facts the probe checks against: each engine's protocol, default
# server port, the port a master address without one is sent to, the
# heartbeat game name and its shutdown name (see the record for addresses).
PROFILES = {
    'q3': dict(game='baseq3', map='q3dm17', protocol=68, serverPort=27960, masterPort=27950,
               heartbeat='QuakeArena-1', flatline='QuakeArena-1', flatlines=2,
               retailServer='quake3.exe', retailClient='quake3.exe', retailLog='qconsole.log',
               clientSettings=dict(cl_motd='0')),
    'rtcw-mp': dict(game='main', map='mp_beach', protocol=60, serverPort=27960, masterPort=27950,
                    heartbeat='Wolfenstein-1', flatline='WolfFlatline-1', flatlines=1, completeStatus=True,
                    retailServer='WolfMP.exe', retailClient='WolfMP.exe', retailLog='rtcwconsole.log',
                    serverSettings=dict(sv_punkbuster='0', g_gametype='5'),
                    clientSettings=dict(cl_motd='0', cl_autoupdate='0', cl_punkbuster='0',
                                        com_recommendedSet='1')),
    'wolfet': dict(game='etmain', map='railgun', protocol=84, serverPort=27960, masterPort=27950,
                   heartbeat='EnemyTerritory-1', flatline='ETFlatline-1', flatlines=1, completeStatus=True,
                   retailServer='etded.exe', retailClient='et.exe', retailLog='etconsole.log',
                   serverSettings=dict(sv_punkbuster='0', g_gametype='2'),
                   clientSettings=dict(cl_motd='0', cl_autoupdate='0', cl_punkbuster='0',
                                       com_recommendedSet='1')),
    'stvef-mp': dict(game='BaseEF', map='hm_borg1', protocol=24, serverPort=27960, masterPort=27953,
                     heartbeat='STEF1', flatline='STEF1', flatlines=2, keyValueHeartbeat=True,
                     retailServer='stvoyHM.exe', retailClient='stvoyHM.exe', retailLog='qconsole.log',
                     layout='stvef', clientSettings=dict(cl_motd='0'),
                     # Started with both developer 1 and logfile 2, retail
                     # Elite Force overflows its stack before its first line
                     # (either alone starts), so developer comes on later.
                     developerLater=True),
    'jka-mp': dict(game='base', map='mp/ffa1', protocol=26, serverPort=29070, masterPort=29060,
                   heartbeat='QuakeArena-1', flatline='QuakeArena-1', flatlines=2,
                   retailServer='jamp.exe', retailClient='jamp.exe', retailLog='qconsole.log',
                   layout='jka', serverSettings=dict(g_gametype='0'),
                   clientSettings=dict(cl_motd='0', com_introplayed='1')),
    'jk2-mp': dict(game='base', map='ffa_bespin', protocol=15, serverPort=28070, masterPort=28060,
                   heartbeat='QuakeArena-1', flatline='QuakeArena-1', flatlines=2,
                   retailServer=None, retailClient='jk2mp.exe', retailLog='qconsole.log',
                   layout='jk2', serverSettings=dict(g_gametype='0'),
                   clientSettings=dict(cl_motd='0', com_introplayed='1')),
}

LEGACY_MASTER_PORT = 27950


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def read(path):
    return path.read_text(encoding='latin1', errors='replace') if path.exists() else ''


def parse_info(text):
    """A backslash info string as an ordered list of (key, value) pairs."""
    text = text.strip().strip('"')
    if not text.startswith('\\'):
        return []
    fields = text.split('\\')[1:]
    return list(zip(fields[::2], fields[1::2]))


class FakeMaster(threading.Thread):
    """Records what servers and clients send a master, and answers like one."""

    def __init__(self, port, label):
        super().__init__(daemon=True)
        self.label = label
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('127.0.0.1', port))
        self.sock.settimeout(0.2)
        self.port = self.sock.getsockname()[1]
        self.events = []
        self.listing = []
        self.lock = threading.Lock()
        self.stopping = threading.Event()
        self.started = time.monotonic()

    def record(self, kind, source, **fields):
        with self.lock:
            self.events.append(dict(kind=kind, source=f'{source[0]}:{source[1]}',
                                    seconds=round(time.monotonic() - self.started, 3), **fields))

    def query(self, source):
        nonce = secrets.token_hex(6)
        for request in (b'getinfo ', b'getstatus '):
            self.sock.sendto(OOB + request + nonce.encode(), source)
        return nonce

    def run(self):
        while not self.stopping.is_set():
            try:
                payload, source = self.sock.recvfrom(65535)
            except (socket.timeout, ConnectionResetError):
                continue
            except OSError:
                break
            if not payload.startswith(OOB):
                self.record('nonOob', source, size=len(payload))
                continue
            body = payload[4:]
            text = body.decode('latin1', errors='replace')
            if text.startswith('heartbeat '):
                self.record('heartbeat', source, text=text, nonce=self.query(source))
            elif text.startswith('\\heartbeat\\'):
                self.record('heartbeat', source, text=text, nonce=self.query(source))
            elif text.startswith('infoResponse'):
                rest = text[len('infoResponse'):].lstrip('\n ').rstrip('\x00')
                self.record('infoResponse', source, header=text[:13], info=parse_info(rest.split('\n')[0]))
            elif text.startswith('statusResponse'):
                lines = text[len('statusResponse'):].lstrip('\n').rstrip('\x00').split('\n')
                self.record('statusResponse', source, info=parse_info(lines[0]),
                            players=[line for line in lines[1:] if line])
            elif text.startswith('gameCompleteStatus'):
                lines = text[len('gameCompleteStatus'):].lstrip('\n').rstrip('\x00').split('\n')
                self.record('gameCompleteStatus', source, info=parse_info(lines[0]),
                            players=[line for line in lines[1:] if line])
            elif text.startswith('getservers'):
                self.record('getservers', source, text=text.rstrip('\x00'))
                reply = OOB + b'getserversResponse'
                for host, port in self.listing:
                    reply += b'\\' + socket.inet_aton(host) + struct.pack('>H', port)
                reply += b'\\EOT\x00\x00\x00'
                self.sock.sendto(reply, source)
            else:
                self.record('other', source, text=text[:120])

    def stop(self):
        self.stopping.set()
        self.join(2)
        self.sock.close()

    def snapshot(self):
        with self.lock:
            return list(self.events)


def open_master(port, label):
    try:
        return FakeMaster(port, label), True
    except OSError:
        return FakeMaster(0, label), False


def command(executable, settings, tail=()):
    result = [str(executable)]
    for key, value in settings.items():
        result.extend(['+set', key, str(value)])
    return result + list(tail)


def stage_retail(profile_key, profile, installation):
    """Lay down the read-only-safe retail runtime this profile needs."""
    layout = profile.get('layout')
    if layout == 'jka':
        root = RUNTIME / 'jka-retail'
        base = root / 'base'
        base.mkdir(parents=True, exist_ok=True)
        for source in installation.iterdir():
            if source.is_file() and source.suffix.lower() in ('.exe', '.dll'):
                target = root / source.name
                if not target.exists() or sha(source) != sha(target):
                    shutil.copy2(source, target)
        for pak in sorted((installation / 'base').glob('*.pk3')):
            with zipfile.ZipFile(pak) as archive:
                for member in archive.namelist():
                    if member.lower() in ('uix86.dll', 'cgamex86.dll', 'jampgamex86.dll'):
                        target = base / Path(member).name
                        payload = archive.read(member)
                        if not target.exists() or target.read_bytes() != payload:
                            target.write_bytes(payload)
        return dict(root=root, cdpath=installation)
    if layout == 'jk2':
        root = RUNTIME / 'jk2-retail'
        root.mkdir(parents=True, exist_ok=True)
        for source in installation.iterdir():
            if source.is_file() and source.suffix.lower() in ('.exe', '.dll'):
                target = root / source.name
                if not target.exists() or sha(source) != sha(target):
                    shutil.copy2(source, target)
        (root / 'base').mkdir(exist_ok=True)
        return dict(root=root, cdpath=installation)
    if layout == 'stvef':
        # Retail Elite Force formats search paths into 128-byte buffers, has
        # no home path, and overflows its stack on the installation's
        # third-party texture pack, so it reads a copy of the four retail paks.
        root = RUNTIME / 'stvef-retail'
        cd = root / 'cd' / 'BaseEF'
        cd.mkdir(parents=True, exist_ok=True)
        for name in ('pak0.pk3', 'pak1.pk3', 'pak2.pk3', 'pak3.pk3'):
            source, target = installation / 'BaseEF' / name, cd / name
            if not target.exists() or target.stat().st_size != source.stat().st_size:
                shutil.copy2(source, target)
        for source in installation.iterdir():
            if source.is_file() and source.name.lower() in ('stvoyhm.exe', 'ffc10.dll', 'getinfo.dll',
                                                           'binkw32.dll', 'efgamex86.dll', 'efuix86.dll'):
                target = root / source.name
                if not target.exists() or sha(source) != sha(target):
                    shutil.copy2(source, target)
        return dict(root=root, cdpath=root / 'cd')
    return dict(root=installation, cdpath=None)


class Run:
    def __init__(self, output, name):
        self.dir = output / name
        self.dir.mkdir(parents=True, exist_ok=False)
        self.result = dict(name=name)


def wait_for(predicate, timeout, step=0.1):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        value = predicate()
        if value:
            return value
        time.sleep(step)
    return predicate()


def newest_log(home, name):
    candidates = [p for p in home.rglob(name) if p.is_file()]
    return max(candidates, key=lambda p: p.stat().st_mtime) if candidates else None


def bound_port(log):
    found = re.findall(r'Opening IP socket: (?:127\.0\.0\.1|localhost):(\d+)', log)
    ports = [int(p) for p in found]
    return ports


def rcon(port, password, text, timeout=1.0):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.sendto(OOB + f'rcon {password} {text}'.encode(), ('127.0.0.1', port))
        try:
            return sock.recvfrom(65535)[0].decode('latin1', errors='replace')
        except (socket.timeout, ConnectionResetError):
            return None


def start_server(run, kind, profile_key, profile, installation, runtime, retail_layout, master_port_text):
    home = run.dir / 'server-home'
    home.mkdir()
    password = secrets.token_hex(6)
    settings = dict(dedicated='2', developer='1', logfile='2', net_ip='127.0.0.1', net_enabled='1',
                    sv_master1='127.0.0.1' + master_port_text, sv_master2='', sv_master3='', sv_master4='',
                    sv_master5='', rconpassword=password, sv_hostname='legacy-discovery-' + kind,
                    bot_enable='0', **profile.get('serverSettings', {}))
    if kind == 'retail' and profile.get('developerLater'):
        del settings['developer']
    if kind == 'retail':
        executable = retail_layout['root'] / profile['retailServer']
        cwd = retail_layout['root']
        if profile.get('layout') == 'stvef':
            # Retail Elite Force writes beside its base path: give it this run's.
            base = run.dir / 'b'
            base.mkdir()
            settings = dict(fs_basepath=str(base), fs_cdpath=str(retail_layout['cdpath']), **settings)
            home = base
        else:
            settings = dict(fs_basepath=str(retail_layout['root']), fs_homepath=str(home), **settings)
            if retail_layout['cdpath']:
                settings['fs_cdpath'] = str(retail_layout['cdpath'])
    else:
        executable = runtime / 'fntech3.ded.exe'
        cwd = runtime
        settings = dict(com_gameProfile=profile_key, fs_basepath=str(installation), fs_homepath=str(home),
                        **settings)
    cmd = command(executable, settings, ['+map', profile['map']])
    run.result.update(serverKind=kind, serverExecutable=str(executable), serverSha256=sha(executable),
                      serverCommand=[c if c != password else '<rcon>' for c in cmd])
    process = WindowsPrivateProcess(cmd, cwd, run.dir / 'server-stdout.log')
    return process, home, password


def client_cfg(home, game, lines):
    folder = home / game
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'legacyprobe.cfg').write_text('\n'.join(lines) + '\n', encoding='ascii')


def run_client(run, label, executable, cwd, settings, cfg_home, game, lines, timeout, log_name, log_home):
    client_cfg(cfg_home, game, lines)
    cmd = command(executable, settings, ['+exec', 'legacyprobe.cfg'])
    run.result[label + 'Command'] = cmd
    run.result[label + 'Sha256'] = sha(executable)
    process = WindowsPrivateProcess(cmd, cwd, run.dir / f'{label}-stdout.log')
    try:
        wait_for(lambda: process.poll() is not None, timeout)
        exit_code = process.poll()
        if exit_code is None:
            run.result[label + 'ForcedStop'] = bool(process.terminate())
    finally:
        process.close()
    log = newest_log(log_home, log_name)
    text = read(log) if log else ''
    run.result[label + 'Exit'] = exit_code
    run.result[label + 'Log'] = str(log) if log else None
    return text


def ping_verdict(text, port):
    # Retail prints the address with its port, this engine without one.
    answers = re.findall(r'ping time (\d+)ms from 127\.0\.0\.1(?::(\d+))?', text)
    return dict(accepted=any(not answer_port or int(answer_port) == port for _, answer_port in answers),
                differentGame='Different game info packet' in text,
                differentProtocol='Different protocol info packet' in text)


def run_case(output, profile_key, server_kind, runtime, args):
    profile = PROFILES[profile_key]
    installation = INSTALLS[profile_key]
    run = Run(output, f'{profile_key}-{server_kind}')
    result = run.result
    result.update(profile=profile_key, map=profile['map'], expected=dict(
        protocol=profile['protocol'], serverPort=profile['serverPort'], masterPort=profile['masterPort'],
        heartbeat=profile['heartbeat'], flatline=profile['flatline'], flatlines=profile['flatlines']))
    before = inventory(installation)
    retail_layout = stage_retail(profile_key, profile, installation)
    master, own_port = open_master(profile['masterPort'], 'profile')
    stray = None
    if profile['masterPort'] != LEGACY_MASTER_PORT:
        stray, _ = open_master(LEGACY_MASTER_PORT, 'quake')
        stray.start()
    master.start()
    result['masterOnProfilePort'] = own_port
    result['masterPort'] = master.port
    port_text = '' if own_port else f':{master.port}'
    server = None
    try:
        server, server_home, password = start_server(run, server_kind if server_kind == 'retail' else 'fntech3',
                                                     profile_key, profile, installation, runtime,
                                                     retail_layout, port_text)

        def server_log():
            log = newest_log(server_home, profile['retailLog'] if server_kind == 'retail' else 'qconsole.log')
            return read(log) if log else ''

        heartbeat = wait_for(lambda: [e for e in master.snapshot() if e['kind'] == 'heartbeat'], args.timeout)
        wait_for(lambda: [e for e in master.snapshot() if e['kind'] == 'statusResponse'], 5)
        ports = bound_port(server_log())
        result['serverPortsOpened'] = ports
        port = ports[-1] if ports else None
        if heartbeat and not port:
            port = int(heartbeat[0]['source'].split(':')[1])
        result['serverPort'] = port
        if port and not args.no_clients:
            master.listing = [('127.0.0.1', port)]
            # The retail client, where one runs here.
            if profile.get('retailClient') and not args.skip_retail_client:
                client_home = run.dir / 'retail-client-home'
                client_home.mkdir()
                exe = retail_layout['root'] / profile['retailClient']
                settings = dict(r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                                s_initsound='0', developer='1', logfile='2', net_ip='127.0.0.1', net_port='0',
                                cl_allowDownload='0', **profile.get('clientSettings', {}))
                if profile.get('layout') == 'stvef':
                    base = run.dir / 'c'
                    base.mkdir()
                    settings = dict(fs_basepath=str(base), fs_cdpath=str(retail_layout['cdpath']), **settings)
                    cfg_home, log_home = base, base
                else:
                    settings = dict(fs_basepath=str(retail_layout['root']), fs_homepath=str(client_home),
                                    **settings)
                    if retail_layout['cdpath']:
                        settings['fs_cdpath'] = str(retail_layout['cdpath'])
                    cfg_home, log_home = client_home, client_home
                lines = ['wait 60', f'ping 127.0.0.1:{port}', 'wait 120', f'serverstatus 127.0.0.1:{port}',
                         'wait 120', 'quit']
                if profile.get('developerLater'):
                    del settings['developer']
                    lines = ['wait 30', 'developer 1'] + lines
                text = run_client(run, 'retailClient', exe, retail_layout['root'], settings, cfg_home,
                                  profile['game'], lines, 90, profile['retailLog'], log_home)
                result['retailClientPing'] = ping_verdict(text, port)
                result['retailClientStatusPrinted'] = bool(re.search(r'Server settings:', text))
            # The fnTech3 client: asks the server, then the master.
            if not args.skip_fntech3_client:
                client_home = run.dir / 'fntech3-client-home'
                client_home.mkdir()
                exe = args.client_runtime / 'fntech3.exe'
                settings = dict(com_gameProfile=profile_key, fs_basepath=str(installation),
                                fs_homepath=str(client_home), r_fullscreen='0', r_mode='3', in_mouse='0',
                                in_joystick='0', in_nograb='1', s_initsound='0', developer='1', logfile='2',
                                net_ip='127.0.0.1', net_port='0', net_enabled='1', cl_allowDownload='0',
                                sv_master1='127.0.0.1' + port_text, sv_master2='', sv_master3='',
                                sv_master4='', sv_master5='', com_steamIntegration='0', cl_motd='0')
                lines = ['wait 30', f'ping 127.0.0.1:{port}', 'wait 90',
                         f'globalservers 1 {profile["protocol"]} full empty', 'wait 120', 'quit']
                text = run_client(run, 'fntech3Client', exe, args.client_runtime, settings, client_home,
                                  profile['game'], lines, 90, 'qconsole.log', client_home)
                result['fntech3ClientPing'] = ping_verdict(text, port)
                parsed = re.findall(r'getserversResponse:\s*(\d+) servers parsed', text)
                result['fntech3ClientListParsed'] = [int(n) for n in parsed]
                result['fntech3ClientMasterRequest'] = re.findall(r'Requesting servers from [^\n]*', text)
        # A match's end, as the Wolfenstein game modules report it.
        if port and profile.get('completeStatus'):
            result['rconCompleteReply'] = rcon(port, password, 'gameCompleteStatus')
            complete = wait_for(lambda: [e for e in master.snapshot() if e['kind'] == 'gameCompleteStatus'], 5)
            result['gameCompleteStatus'] = complete[0] if complete else None
        # Shut down by rcon and keep listening for the flatline.
        if port:
            result['rconQuitReply'] = rcon(port, password, 'quit')
        wait_for(lambda: server.poll() is not None, 15)
        time.sleep(1.0)
        result['serverExit'] = server.poll()
        if result['serverExit'] is None:
            result['serverForcedStop'] = bool(server.terminate())
    except (OSError, RuntimeError) as error:
        result['error'] = repr(error)
    finally:
        if server:
            server.close()
        master.stop()
        if stray:
            stray.stop()
    events = master.snapshot()
    result['masterEvents'] = events
    if stray:
        result['legacyPortEvents'] = stray.snapshot()
    heartbeats = [e['text'] for e in events if e['kind'] == 'heartbeat']
    result['heartbeats'] = heartbeats
    infos = [e['info'] for e in events if e['kind'] == 'infoResponse']
    statuses = [e['info'] for e in events if e['kind'] == 'statusResponse']
    result['infoKeys'] = [k for k, _ in infos[0]] if infos else None
    result['info'] = dict(infos[0]) if infos else None
    result['statusKeys'] = [k for k, _ in statuses[0]] if statuses else None
    result['installationUnchanged'] = before == inventory(installation)
    return result


def summarize(results):
    lines = []
    for r in results:
        exp = r['expected']
        hb = r.get('heartbeats') or []
        lines.append(dict(
            case=r['name'],
            heartbeatsOnProfilePort=len(hb) if r.get('masterOnProfilePort') else None,
            heartbeatsOnQuakePort=len([e for e in r.get('legacyPortEvents', []) if e['kind'] == 'heartbeat']),
            firstHeartbeat=hb[0] if hb else None,
            lastHeartbeat=hb[-1] if hb else None,
            serverPort=r.get('serverPort'), expectedServerPort=exp['serverPort'],
            infoGamename=(r.get('info') or {}).get('gamename'),
            infoProtocol=(r.get('info') or {}).get('protocol'),
            statusHasProtocol=bool(r.get('statusKeys') and 'protocol' in r['statusKeys']),
            retailClientPing=r.get('retailClientPing'), fntech3ClientPing=r.get('fntech3ClientPing'),
            fntech3ClientListParsed=r.get('fntech3ClientListParsed'),
            installationUnchanged=r.get('installationUnchanged'), error=r.get('error')))
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', default=list(PROFILES), choices=list(PROFILES))
    parser.add_argument('--runtime', action='append', default=[], metavar='LABEL=DIR',
                        help='fnTech3 runtime folder holding fntech3.ded.exe; repeatable')
    parser.add_argument('--client-runtime', type=Path, required=True,
                        help='fnTech3 runtime folder whose fntech3.exe browses and pings')
    parser.add_argument('--retail-server', action='store_true', help='also run each profile\'s retail server')
    parser.add_argument('--skip-retail-client', action='store_true')
    parser.add_argument('--skip-fntech3-client', action='store_true')
    parser.add_argument('--no-clients', action='store_true')
    parser.add_argument('--timeout', type=float, default=45.0, help='seconds to wait for the first heartbeat')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows only')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runtimes = []
    for item in args.runtime:
        label, _, folder = item.partition('=')
        runtimes.append((label, Path(folder).resolve(strict=True)))
    args.client_runtime = args.client_runtime.resolve(strict=True)
    results = []
    for profile_key in args.profiles:
        kinds = [(label, folder) for label, folder in runtimes]
        if args.retail_server and PROFILES[profile_key].get('retailServer'):
            kinds.append(('retail', None))
        for label, folder in kinds:
            print(f'== {profile_key} {label}', flush=True)
            result = run_case(output, profile_key, label, folder, args)
            results.append(result)
            (output / f'{profile_key}-{label}' / 'result.json').write_text(json.dumps(result, indent=2) + '\n',
                                                                           encoding='utf-8')
            print(json.dumps(summarize([result])[0], indent=1), flush=True)
    (output / 'summary.json').write_text(json.dumps(summarize(results), indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
