#!/usr/bin/env python3
"""Transfer an authored ET fixture pak, then enter a pure session.

The pak travels over UDP blocks, or with --www through the server's web
redirect: a loopback HTTP server serves the server's home, the server sets
sv_wwwDownload, and the client fetches the file there. --www-missing serves
nothing, so the client must decline the redirect and get the file in blocks.
--www-disconnected adds sv_wwwDlDisconnected, which lets a client leave while
it fetches: retail et.exe then leaves and comes back, and fnTech3's client
stays connected, as the server allows either.

Windows only. The fixed engine or retail executable runs on an unswitched
private desktop, windowed, with input disabled. Writable state and the generated
pak stay beneath the output directory; the legitimate installation is read only.
No input is injected and no operating-system screenshot is taken.
"""
from pathlib import Path
import argparse
import functools
import hashlib
import http.server
import json
import os
import random
import re
import socket
import subprocess
import sys
import threading
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

MOD = 'fntech3_download_probe'
PAK = 'fntech3-fixture.pk3'
NAME = 'fntech3-download-probe'
SEED = 20261002
# A fallback URL names a web page, not the file: the client stops with it.
REDIRECT_REFUSAL = 'This server asks for its files to be fetched from '
ERRORS = re.compile(r'^(?:ERROR:.*|.*Illegible server message.*|'
                    r'.*read past end of server message.*|.*recursive error after.*|'
                    r'.*Server fatal crashed.*|.*com_errorMessage VM_Call with NULL vm.*)$', re.M)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return path.read_text(encoding='utf-8', errors='replace') if path.exists() else ''


def inventory(path):
    return {str(p.relative_to(path)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in path.rglob('*') if p.is_file()}


def write_fixture(path):
    """A deterministic ZIP containing only independently authored data."""
    generator = random.Random(SEED)
    payload = bytes(generator.randrange(256) for _ in range(32768))
    path.parent.mkdir(parents=True, exist_ok=True)
    info = zipfile.ZipInfo('fntech3_probe_payload.dat', (2026, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_STORED
    with zipfile.ZipFile(path, 'w') as archive:
        archive.writestr(info, payload)
    return dict(seed=SEED, payloadBytes=len(payload), zipTimestamp='2026-01-01T00:00:00',
                payloadSha256=hashlib.sha256(payload).hexdigest(),
                bytes=path.stat().st_size, sha256=sha(path))


def command(executable, settings, tail=()):
    result = [str(executable)]
    for key, value in settings.items():
        result += ['+set', key, str(value)]
    return result + list(tail)


def transfer_log_status(text):
    name = re.escape(MOD + '/' + PAK)
    begun = bool(re.search(r'clientDownload: \d+ : beginning "' + name + '"', text))
    completed = bool(re.search(r'clientDownload: \d+ : (?:file "' + name +
                               r'" completed|download complete)', text))
    return begun, completed


def settings(installation, home, retail):
    result = dict(fs_basepath=str(installation), fs_homepath=str(home), fs_cdpath='',
                  fs_game=MOD, r_fullscreen='0', developer='1', logfile='2',
                  net_ip='127.0.0.1', sv_punkbuster='0', cl_punkbuster='0')
    if not retail:
        result['com_gameProfile'] = 'wolfet'
    return result


class RecordingHandler(http.server.SimpleHTTPRequestHandler):
    """Serves one folder over loopback and records every request."""
    requests = None

    def log_message(self, format, *args):
        pass

    def log_request(self, code='-', size='-'):
        self.requests.append(dict(path=self.path, status=int(code) if str(code).isdigit() else code))


def start_web(folder):
    requests = []
    handler = type('Handler', (RecordingHandler,), dict(requests=requests))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0),
                                             functools.partial(handler, directory=str(folder)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, requests


def web_fetched(result):
    """The pak came through the web redirect: a 200 for it and no block transfer."""
    return any(r['path'].endswith('/' + MOD + '/' + PAK) and r['status'] == 200
               for r in result.get('httpRequests', []))


def web_declined(result):
    """The client asked the web server, was refused, and took the blocks instead."""
    return any(r['path'].endswith('/' + MOD + '/' + PAK) and r['status'] == 404
               for r in result.get('httpRequests', [])) and result.get('downloadCompleted')


def passed(result):
    """A finalized file alone does not establish download/pure admission."""
    if result.get('mode') == 'www' and not web_fetched(result):
        return False
    if result.get('mode') == 'www-missing' and not web_declined(result):
        return False
    udp = result.get('mode', 'udp-transfer') in ('udp-transfer', 'www-missing')
    return bool(result.get('clientExit') == 0 and not result.get('clientForced')
                and result.get('serverExit') == 0 and not result.get('serverForced')
                and result.get('installationUnchanged') and result.get('fixtureInitiallyAbsent')
                and result.get('fixtureMatches') and not result.get('temporaryPakRemains')
                and (not udp or (result.get('downloadBegan') and result.get('downloadCompleted')))
                and result.get('serverBegin') and not result.get('unpureDrop')
                and not result.get('clientErrors') and not result.get('serverErrors'))


def redirect_refused(result):
    return bool(result.get('redirectRefused') and result.get('clientLiveAfterRefusal')
                and result.get('serverExit') == 0 and not result.get('serverForced')
                and result.get('installationUnchanged') and result.get('fixtureInitiallyAbsent')
                and result.get('downloadedBytes') == 0 and not result.get('temporaryPakRemains')
                and not result.get('downloadCompleted') and not result.get('serverBegin')
                and result.get('clientErrors') and all(REDIRECT_REFUSAL in error
                    for error in result['clientErrors']) and not result.get('serverErrors'))


def probe(args, output):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    server_home, client_home = output / 'server-home', output / 'client-home'
    server_home.mkdir(); client_home.mkdir()
    fixture = server_home / MOD / PAK
    recipe = write_fixture(fixture)
    downloaded = client_home / MOD / PAK
    before = inventory(args.installation)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    password = 'fntech3-local-download-probe'
    server_values = settings(args.installation, server_home, args.retail_server)
    server_values.update(dedicated='1', net_port=port, sv_pure='1', sv_allowDownload='1',
                         sv_wwwDownload='0', sv_wwwBaseURL='', sv_wwwFallbackURL='', sv_dlURL='',
                         g_gametype='2', bot_enable='0', rconPassword=password,
                         **{f'sv_master{i}': '' for i in range(1, 6)})
    if args.expect_redirect_refusal:
        server_values.update(sv_wwwDownload='1',
                             sv_wwwFallbackURL='https://example.invalid/fntech3-download-probe')
    web = requests = None
    if args.www or args.www_missing:
        # Only loopback: the server's own home, or an empty folder that 404s.
        served = server_home if args.www else output / 'web-empty'
        served.mkdir(exist_ok=True)
        web, requests = start_web(served)
        server_values.update(sv_wwwDownload='1', sv_wwwFallbackURL='',
                             sv_wwwBaseURL=f'http://127.0.0.1:{web.server_address[1]}')
        if args.www_disconnected:
            server_values['sv_wwwDlDisconnected'] = '1'
    client_values = settings(args.installation, client_home, args.retail_client)
    client_values.update(r_mode='-1', r_customWidth='800', r_customHeight='600', in_mouse='0',
                         in_joystick='0', in_nograb='1', s_initsound='0', cl_allowDownload='1',
                         cl_wwwDownload='1' if args.www or args.www_missing else '0',
                         cl_motd='0', cl_autoupdate='0', com_introplayed='1',
                         com_maxfps='60', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
                         name=NAME, rate='25000', net_port='0')
    if not args.retail_client:
        client_values.update(com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                             cl_renderer='glx', net_ip6='::1', net_port6='0')
    bootstrap = ('com_gameProfile', 'fs_basepath', 'fs_homepath', 'fs_game', 'fs_cdpath',
                 'r_fullscreen', 'logfile', 'developer', 'net_ip', 'net_port')
    client_startup = {key: value for key, value in client_values.items() if key in bootstrap}
    client_config = {key: value for key, value in client_values.items() if key not in bootstrap}
    # activeAction starts only after the download and cgame initialization.
    # Repeat it in both cfg search directories: fs_game may be restored from
    # the server during a filesystem restart. Neither cfg lives in the install.
    for game in (MOD, 'etmain'):
        folder = client_home / game
        folder.mkdir(exist_ok=True)
        (folder / 'autoexec.cfg').write_text(
            ''.join(f'set {key} "{value}"\n' for key, value in client_config.items()) +
            'set activeAction "wait 240; echo FNTECH3_DOWNLOAD_ACTIVE; quit"\n', encoding='utf-8')
    host_command = command(args.server, server_values,
                           ['+map', args.map, '+echo', 'FNTECH3_DOWNLOAD_SERVER_READY'])
    connect_tail = ['+connect', f'127.0.0.1:{port}']
    if args.connect_delay_frames:
        connect_tail = ['+wait', str(args.connect_delay_frames)] + connect_tail
    peer_command = command(args.client, client_startup, connect_tail)
    level = ('Reference comparison' if args.retail_client and args.retail_server else
             'Interop' if args.retail_client or args.retail_server else 'Live')
    result = dict(profile='wolfet', evidence=level,
                  mode=('redirect-refusal' if args.expect_redirect_refusal else 'www' if args.www
                        else 'www-missing' if args.www_missing else 'udp-transfer'),
                  map=args.map, pure=True, transport='UDP', retailClient=args.retail_client,
                  wwwDisconnected=bool(args.www_disconnected),
                  connectDelayFrames=args.connect_delay_frames,
                  retailServer=args.retail_server, fixtureRecipe=recipe,
                  fixtureInitiallyAbsent=not downloaded.exists(),
                  clientCommand=peer_command, serverCommand=host_command,
                  clientConfig=client_config,
                  clientSha256=sha(args.client), serverSha256=sha(args.server), passed=False)
    host = peer = None
    server_log = server_home / MOD / ('etconsole.log' if args.retail_server else 'qconsole.log')
    client_log = client_home / MOD / ('etconsole.log' if args.retail_client else 'qconsole.log')
    begin = time.monotonic()
    try:
        host = WindowsPrivateProcess(host_command, args.server.parent, output / 'server-stdout.log')
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline and host.poll() is None:
            text = read(server_log)
            if ('FNTECH3_DOWNLOAD_SERVER_READY' in text or
                    args.retail_server and 'sv_serverRestarting 0' in text):
                break
            time.sleep(0.2)
        else:
            raise RuntimeError('server did not finish map startup')
        result['serverReadySeconds'] = round(time.monotonic() - begin, 3)
        time.sleep(2)
        peer = WindowsPrivateProcess(peer_command, args.client.parent, output / 'client-stdout.log')
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline and peer.poll() is None:
            if args.expect_redirect_refusal and REDIRECT_REFUSAL in read(client_log):
                time.sleep(2)
                result['clientLiveAfterRefusal'] = peer.poll() is None
                break
            time.sleep(0.2)
        result['clientExit'] = peer.poll()
        result['clientForced'] = peer.poll() is None
    except (OSError, RuntimeError) as error:
        result['error'] = str(error)
    finally:
        if peer:
            peer.close()
        if host:
            if host.poll() is None:
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as stop:
                    stop.sendto(b'\xff\xff\xff\xff' + f'rcon {password} quit'.encode('ascii'),
                                ('127.0.0.1', port))
                try:
                    host.wait(10)
                except subprocess.TimeoutExpired:
                    pass
            result['serverExit'] = host.poll()
            result['serverForced'] = host.poll() is None
            host.close()
    server_text, client_text = read(server_log), read(client_log)
    (output / 'server-console.log').write_text(server_text, encoding='utf-8')
    (output / 'client-console.log').write_text(client_text, encoding='utf-8')
    begun, completed = transfer_log_status(server_text)
    result.update(installationUnchanged=before == inventory(args.installation),
                  elapsedSeconds=round(time.monotonic() - begin, 3),
                  fixtureMatches=downloaded.is_file() and sha(downloaded) == recipe['sha256'],
                  downloadedBytes=downloaded.stat().st_size if downloaded.exists() else 0,
                  downloadedSha256=sha(downloaded) if downloaded.exists() else None,
                  temporaryPakRemains=downloaded.with_suffix('.pk3.tmp').exists(),
                  downloadBegan=begun, downloadCompleted=completed,
                  serverBegin=bool(re.search(r'ClientBegin: \d+', server_text)),
                  clientActiveMarker='FNTECH3_DOWNLOAD_ACTIVE' in client_text,
                  redirectRefused=REDIRECT_REFUSAL in client_text,
                  unpureDrop='Unpure client detected' in server_text,
                  clientErrors=ERRORS.findall(client_text), serverErrors=ERRORS.findall(server_text),
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT,
                                              capture_output=True, text=True).stdout.strip())
    if web:
        web.shutdown()
        web.server_close()
        result['httpRequests'] = list(requests)
        result['serverRedirected'] = 'Redirecting client' in server_text
        # The client's own word that it leaves to fetch; servers log its commands.
        result['clientLeftToFetch'] = 'wwwdl bbl8r' in server_text
    result['passed'] = redirect_refused(result) if args.expect_redirect_refusal else passed(result)
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if not k.endswith('Command')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--retail-client', action='store_true')
    parser.add_argument('--retail-server', action='store_true')
    parser.add_argument('--expect-redirect-refusal', action='store_true',
                        help='send ET fallback URL to a non-www client; require actionable refusal')
    parser.add_argument('--www', action='store_true',
                        help='serve the server home over loopback HTTP and redirect the download there')
    parser.add_argument('--www-missing', action='store_true',
                        help='redirect to a loopback HTTP server that has no such file')
    parser.add_argument('--www-disconnected', action='store_true',
                        help='with --www, let clients leave while they fetch (sv_wwwDlDisconnected 1)')
    parser.add_argument('--map', default='railgun')
    parser.add_argument('--timeout', type=float, default=120)
    parser.add_argument('--connect-delay-frames', type=int, default=0,
                        help='allow the menu/UI to initialize before the queued connect')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('runtime probes require Windows')
    for name in ('client', 'server', 'installation', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    if not args.output.is_relative_to(ROOT / '.tmp'):
        parser.error('output must remain beneath the project .tmp directory')
    if args.output.is_relative_to(args.installation) or args.installation.is_relative_to(args.output):
        parser.error('output and installation must be separate')
    if args.expect_redirect_refusal and args.retail_client:
        parser.error('redirect refusal tests require an fnTech3 client')
    if sum(map(bool, (args.www, args.www_missing, args.expect_redirect_refusal))) > 1:
        parser.error('choose one of --www, --www-missing and --expect-redirect-refusal')
    if args.www_disconnected and not args.www:
        parser.error('--www-disconnected needs --www')
    if args.connect_delay_frames < 0:
        parser.error('connect delay must be nonnegative')
    return 0 if probe(args, args.output) else 1


if __name__ == '__main__':
    raise SystemExit(main())
