#!/usr/bin/env python3
"""Bounded retail JKA client admission to an fnTech3 multiplayer server.

Windows only, unswitched private desktops, windowed rendering, no device input.
Retail executables run as independent copies at one stable project-local path;
paks are read from the installation and all writable data uses isolated homes.
The screenshot is produced by the retail engine's registered screenshot command.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import socket
import stat
import subprocess
import sys
import time
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess
from server_query_evidence import server_info

NAME = 'fntech3-jka-peer'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(root):
    return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in root.rglob('*') if p.is_file()}


def project_directory(path):
    # Normalize traversal without dereferencing links before checking them.
    path = Path(os.path.abspath(path))
    if not path.is_relative_to(ROOT / '.tmp'):
        raise ValueError('JKA runtime/output must stay inside project .tmp')
    current = ROOT
    for name in path.relative_to(ROOT).parts:
        current = current / name
        if current.exists() or current.is_symlink():
            meta = current.lstat()
            if stat.S_ISLNK(meta.st_mode) or getattr(meta, 'st_file_attributes', 0) & 0x400:
                raise ValueError(f'JKA runtime/output traverses a link: {current}')
    path.mkdir(parents=True, exist_ok=True)
    return path.resolve(strict=True)


def read(path):
    return path.read_text(encoding='latin1', errors='replace') if path.exists() else ''


def command(executable, settings, tail=()):
    result = [str(executable)]
    for key, value in settings.items():
        result.extend(['+set', key, str(value)])
    return result + list(tail)


def run(args):
    output = project_directory(args.output)
    if (output / 'result.json').exists():
        raise ValueError('Select a fresh output directory; evidence is never overwritten')
    before = inventory(args.installation)
    retail_root = project_directory(ROOT / '.tmp/runtime/jka-retail')
    for source in args.installation.iterdir():
        if source.is_file() and source.suffix.lower() in ('.exe', '.dll'):
            destination = retail_root / source.name
            if not destination.exists() or sha(source) != sha(destination):
                shutil.copy2(source, destination)
    retail = retail_root / 'jamp.exe'
    retail_base = project_directory(retail_root / 'base')
    homes = [project_directory(output / name) for name in ('server-home', 'client-home')]
    for home in homes:
        (home / 'base').mkdir(exist_ok=True)
        for cfg in ('jampconfig.cfg', 'jampconfig_server.cfg'):
            (home / 'base' / cfg).write_text('', encoding='ascii')
    # Retail's native loader needs loose DLLs. Paks remain installed read-only;
    # extract only the installed module members into this probe's writable home.
    for pak in sorted((args.installation / 'base').glob('*.pk3')):
        with zipfile.ZipFile(pak) as archive:
            for member in archive.namelist():
                if member.lower() in ('uix86.dll', 'cgamex86.dll', 'jampgamex86.dll'):
                    payload = archive.read(member)
                    (homes[1] / 'base' / Path(member).name).write_bytes(payload)
                    (retail_base / Path(member).name).write_bytes(payload)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    host_command = command(args.server, dict(com_gameProfile='jka-mp',
        fs_basepath=args.installation, fs_homepath=homes[0], dedicated=1,
        developer=1, logfile=2, net_enabled=1, net_ip='127.0.0.1', net_ip6='::1',
        net_port=port, sv_pure=int(args.pure), bot_enable=0, g_gametype=0,
        r_fullscreen=0, sv_master1='', sv_master2='', sv_master3='', sv_master4='',
        sv_master5='', com_steamIntegration=0), ['+map', args.map])
    (homes[1] / 'base/autoexec.cfg').write_text(
        'set name "' + NAME + '"\nset cl_allowDownload 0\nset cl_motd 0\n'
        'set com_maxfps 60\nset com_introplayed 1\nset activeAction "wait 300; screenshot; wait 30; quit"\n',
        encoding='ascii')
    peer_command = command(retail, dict(fs_basepath=retail_root, fs_homepath=homes[1],
        fs_cdpath=args.installation, r_fullscreen=0, r_mode=-1, r_customwidth=800,
        r_customheight=600, in_mouse=0, in_joystick=0, in_nograb=1, s_initsound=0,
        developer=1, logfile=2, com_introplayed=1, net_ip='127.0.0.1', net_port=0),
        [])
    # Keep the non-set startup action first, before retail command-line limits.
    peer_command[1:1] = ['+connect', f'127.0.0.1:{port}']
    result = dict(profile='jka-mp', map=args.map, pure=args.pure,
        evidence='Interop subset: retail client to fnTech3 server',
        serverCommand=host_command, clientCommand=peer_command,
        serverSha256=sha(args.server), retailClientSha256=sha(retail),
        installation=str(args.installation), passed=False)
    host = peer = None
    begin = time.monotonic()
    try:
        host = WindowsPrivateProcess(host_command, args.server.parent, output / 'server-stdout.log')
        nonce = uuid.uuid4().hex
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(.25)
            while host.poll() is None and time.monotonic() - begin < 30:
                sock.sendto(b'\xff\xff\xff\xffgetinfo ' + nonce.encode() + b'\n', ('127.0.0.1', port))
                try:
                    payload, address = sock.recvfrom(65535)
                except (socket.timeout, ConnectionResetError):
                    continue
                info = server_info(payload, 'infoResponse', args.map, nonce, 26)
                if address == ('127.0.0.1', port) and info:
                    result['serverIdentity'] = info
                    break
        if not result.get('serverIdentity'):
            raise RuntimeError('No challenged protocol-26 identity from the selected server')
        peer = WindowsPrivateProcess(peer_command, retail_root, output / 'client-stdout.log')
        deadline = time.monotonic() + args.timeout
        while peer.poll() is None and time.monotonic() < deadline:
            time.sleep(.2)
        result['clientExit'] = peer.poll()
        result['clientForced'] = peer.poll() is None
    except (OSError, RuntimeError) as error:
        result['error'] = str(error)
    finally:
        for process in (peer, host):
            if process:
                process.close()
    server_text = read(homes[0] / 'base/qconsole.log')
    client_text = read(homes[1] / 'base/qconsole.log')
    (output / 'server-console.log').write_text(server_text, encoding='utf-8')
    (output / 'client-console.log').write_text(client_text, encoding='utf-8')
    captures = sorted(homes[1].rglob('screenshots/*'))
    captures = [p for p in captures if p.is_file() and p.suffix.lower() in ('.tga', '.jpg', '.png')]
    result.update(installationUnchanged=before == inventory(args.installation),
        serverExecutableUnchanged=result['serverSha256'] == sha(args.server),
        elapsedSeconds=round(time.monotonic() - begin, 3),
        serverActive=f'Going from CS_PRIMED to CS_ACTIVE for {NAME}' in server_text,
        serverBegin=bool(re.search(r'ClientBegin: \d+', server_text)),
        unpureDrop='Unpure client detected' in server_text,
        serverErrors=re.findall(r'^(?:ERROR:.*|.*not served yet.*|.*Illegible.*)', server_text, re.M),
        clientErrors=re.findall(r'^(?:ERROR:.*|.*Illegible.*|.*read past end.*)', client_text, re.M),
        screenshots=[dict(path=str(p.relative_to(ROOT)), sha256=sha(p)) for p in captures])
    if 'Disk 1 not in drive' in client_text or re.search(r'^NEED CD\s*$', client_text, re.M):
        result['retailMediaRequired'] = True
        result['clientErrors'].append('Retail Disc 1 media check refused connection before UDP negotiation')
    result['passed'] = bool(result.get('clientExit') == 0 and not result.get('clientForced')
        and result['serverActive'] and result['serverBegin'] and captures
        and not result['unpureDrop'] and not result['serverErrors'] and not result['clientErrors']
        and result['installationUnchanged'] and result['serverExecutableUnchanged'])
    result['sourceCommit'] = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT,
        text=True, capture_output=True, check=True).stdout.strip()
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if not k.endswith('Command')}, indent=2))
    return result['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--map', default='mp/ffa1')
    parser.add_argument('--timeout', type=float, default=45)
    parser.add_argument('--pure', action='store_true')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows private-desktop harness required')
    args.server = args.server.resolve(strict=True)
    args.installation = args.installation.resolve(strict=True)
    return 0 if run(args) else 1


if __name__ == '__main__':
    raise SystemExit(main())
