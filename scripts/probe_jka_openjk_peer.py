#!/usr/bin/env python3
"""Bounded Windows x86 Academy MP admission between fnTech3 and pinned OpenJK.

This supplies maintained-peer evidence, separately from retail-engine interop.
Stable executables run on unswitched private desktops, windowed, without input
injection. Retail paks are read-only; module caches and writable data stay in
isolated project homes. Captures use each engine's registered PNG command.
"""
from pathlib import Path
import argparse
import binascii
import contextlib
import datetime
import json
import math
import os
import re
import socket
import stat
import struct
import subprocess
import time
import uuid
import zipfile
import zlib

from probe_jka_client import ROOT, ERRORS, modules, remove_home
from probe_jka_peer import WindowsPrivateProcess, command, inventory, read, sha
from server_query_evidence import server_info
from jka_demo_evidence import generated_demo

CONFIG = 'jka-openjk-peer.cfg'
MARKER = 'FNTECH3_JKA_MAINTAINED_ACTIVE'
CLIENT_NAME = 'fntech3-jka-maintained-peer'
OPENJK_COMMIT = '1a6a643427aa347553e9073dac5570b33337c4d9'


def checked_output(path, protected):
    path = Path(os.path.abspath(path))
    if not path.is_relative_to(ROOT / '.tmp') or path == ROOT / '.tmp':
        raise ValueError('Evidence requires its own directory below project .tmp')
    for item in protected:
        if path.is_relative_to(item) or item.is_relative_to(path):
            raise ValueError(f'Evidence overlaps an installation or stable runtime: {item}')
    current = ROOT
    for name in path.relative_to(ROOT).parts:
        current /= name
        if current.exists() or current.is_symlink():
            meta = current.lstat()
            if (stat.S_ISLNK(meta.st_mode) or getattr(meta, 'st_file_attributes', 0) & 0x400
                    or not current.is_dir()):
                raise ValueError(f'Evidence traverses a link or non-directory: {current}')
    if path.exists():
        raise ValueError('Evidence output already exists; select a fresh directory')
    return path


def x86_executable(path, expected_name):
    path = path.resolve(strict=True)
    if path.name.casefold() != expected_name.casefold() or not path.is_relative_to(ROOT):
        raise ValueError(f'Use the stable project-local {expected_name}; do not copy or rename it')
    raw = path.read_bytes()
    if len(raw) < 64 or raw[:2] != b'MZ':
        raise ValueError(f'Not an x86 PE executable: {path}')
    offset = struct.unpack_from('<I', raw, 60)[0]
    if (offset > len(raw) - 6 or raw[offset:offset + 4] != b'PE\0\0'
            or struct.unpack_from('<H', raw, offset + 4)[0] != 0x14c):
        raise ValueError(f'Retail native modules require an x86 executable: {path}')
    return path


def validate(args):
    if not math.isfinite(args.timeout) or not 1 <= args.timeout <= 600:
        raise ValueError('Timeout must be finite and between 1 and 600 seconds')
    if not 1 <= args.frames <= 2400:
        raise ValueError('Capture delay must be between 1 and 2400 frames')
    if not re.fullmatch(r'[a-zA-Z0-9_/-]+', args.map) or '..' in args.map.split('/'):
        raise ValueError('Select an explicit contained map name')
    if args.server_only and args.direction != 'fntech3-to-openjk':
        raise ValueError('Readiness-only mode may launch only the independent OpenJK server')
    if getattr(args, 'record_demos', False) and (args.server_only or args.direction != 'openjk-to-fntech3'):
        raise ValueError('Demo capture requires the known OpenJK-client/fnTech3-server pair')
    installation = args.installation.resolve(strict=True)
    if not (installation / 'jamp.exe').is_file() or not (installation / 'base/assets0.pk3').is_file():
        raise ValueError('Select a legitimate Academy installation with jamp.exe and base/assets0.pk3')
    openjk = args.openjk_directory.resolve(strict=True)
    if not openjk.is_relative_to(ROOT / '.tmp/runtime'):
        raise ValueError('Use a fixed project-local OpenJK comparison runtime')
    if args.direction == 'fntech3-to-openjk':
        server = x86_executable(openjk / 'openjkded.x86.exe', 'openjkded.x86.exe')
        client = None if args.server_only else x86_executable(args.client, 'fntech3.exe')
        renderer = None if args.server_only else client.parent / f'fnql_{args.renderer}_x86.dll'
        server_engine, client_engine = 'openjk', 'fntech3'
    else:
        server = x86_executable(args.server, 'fntech3.ded.exe')
        client = x86_executable(openjk / 'openjk.x86.exe', 'openjk.x86.exe')
        renderer = openjk / 'rd-vanilla_x86.dll'
        if not (openjk / 'SDL2.dll').is_file():
            raise ValueError('The independent OpenJK SDL2 runtime is missing')
        server_engine, client_engine = 'fntech3', 'openjk'
    if renderer and not renderer.is_file():
        raise ValueError(f'The matching renderer is missing: {renderer}')
    output = checked_output(args.output, (installation, openjk, server.parent,
        *((client.parent,) if client else ())))
    return installation, output, server, client, renderer, server_engine, client_engine


def configuration(engine, frames, hostname=None, record=False):
    values = dict(developer=1, com_bootlogo=0, com_introplayed=1,
        cl_allowDownload=0, cl_motd=0, com_maxfps=60, com_maxfpsUnfocused=60,
        com_maxfpsMinimized=60, s_volume=0, s_musicVolume=0, cl_shownet=3,
        name=CLIENT_NAME, cl_autoRecordDemos=0, sv_autoRecordDemos=0,
        g_log='games.log', g_logSync=1)
    for number in range(1, 6):
        values[f'sv_master{number}'] = ''
    if hostname:
        values['sv_hostname'] = hostname
        if record:
            values['sv_autoRecordDemos'] = 1
    else:
        capture = 'screenshotPNG' if engine == 'fntech3' else 'screenshot_png'
        begin = 'record jka-maintained-client; ' if record else ''
        end = 'stoprecord; ' if record else ''
        values['activeAction'] = (f'echo {MARKER}; {begin}wait {frames}; {end}'
            f'{capture} jka-maintained-peer; wait 5; quit;')
    return ''.join(f'set {name} "{value}"\n' for name, value in values.items())


def launch_command(executable, engine, installation, home, args, port, hostname=None):
    settings = dict(fs_basepath=installation, fs_homepath=home, fs_game='', fs_cdpath='',
        r_fullscreen=0, in_mouse=0, in_keyboard=0, in_joystick=0, in_nograb=1,
        s_initsound=0, developer=1, logfile=2 if engine == 'openjk' else 4,
        net_enabled=1, net_ip='127.0.0.1', net_port=port if hostname else 0)
    if engine == 'fntech3':
        settings.update(com_gameProfile='jka-mp', com_steamIntegration=0, in_synthetic=0)
    else:
        # Sys_UnpackDLL uses GetTempPath. launch_private scopes TEMP/TMP to
        # this home, keeping native module extraction inside the project.
        settings['com_unpackLibraries'] = 1
    if hostname:
        settings.update(dedicated=1, sv_pure=int(args.pure), bot_enable=0,
            g_gametype=0, sv_maxclients=1, sv_hostname=hostname)
        tail = ['+exec', CONFIG, '+map', args.map]
    else:
        settings.update(r_mode=-1, r_customWidth=800, r_customHeight=600,
            cl_renderer=args.renderer if engine == 'fntech3' else 'rd-vanilla',
            com_introplayed=1, com_bootlogo=0)
        tail = ['+exec', CONFIG, '+connect', f'127.0.0.1:{port}']
    result = command(executable, settings, tail)
    if sum(word.startswith('+') for word in result) > 32:
        raise ValueError('Startup arguments exceed 32 commands')
    if len(subprocess.list2cmdline(result[1:]).encode('utf-8')) >= 1024:
        raise ValueError('Startup arguments exceed 1023 bytes; shorten the output path')
    return result


@contextlib.contextmanager
def child_temporary_environment(home):
    temporary = home / 'os-temp'
    temporary.mkdir(exist_ok=True)
    previous = {key: os.environ.get(key) for key in ('TEMP', 'TMP')}
    try:
        os.environ.update(TEMP=str(temporary), TMP=str(temporary))
        yield
    finally:
        for key, value in previous.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


def launch_private(launch, executable, home, stdout):
    with child_temporary_environment(home):
        return WindowsPrivateProcess(launch, executable.parent, stdout)


def ready_identity(host, port, mapname, hostname, timeout=30):
    nonce = uuid.uuid4().hex
    deadline = time.monotonic() + timeout
    result = {}
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(.25)
        while host.poll() is None and time.monotonic() < deadline:
            for request, response in (('getinfo', 'infoResponse'), ('getstatus', 'statusResponse')):
                sock.sendto(b'\xff\xff\xff\xff' + request.encode() + b' ' + nonce.encode() + b'\n',
                    ('127.0.0.1', port))
                try:
                    payload, address = sock.recvfrom(65535)
                except (socket.timeout, ConnectionResetError):
                    continue
                if address != ('127.0.0.1', port):
                    continue
                info = server_info(payload, response, mapname, nonce, 26 if request == 'getinfo' else None)
                key = 'hostname' if request == 'getinfo' else 'sv_hostname'
                if not info or info.get(key) != hostname:
                    continue
                if request == 'getstatus' and payload.split(b'\n', 2)[-1].strip(b'\r\n\0'):
                    continue  # Readiness must precede all client admission.
                result[request] = dict(fields=info, packetHex=payload.hex())
            if len(result) == 2:
                return result
    raise RuntimeError('No empty, challenged map/protocol-26 identity from this selected server')


def drop_completed(server_text):
    # The transition is printed after the game disconnect callback returns.
    # OpenJK clears userinfo/name before printing it. An anonymous transition
    # is evidence only after this private probe's named disconnect command
    # and slot0 callback log, in that order; a callback log alone is insufficient.
    if any(marker in server_text for marker in (
            f'Going from CS_ACTIVE to CS_ZOMBIE for {CLIENT_NAME}',
            f'Going to CS_ZOMBIE for {CLIENT_NAME}')):
        return True
    trace = re.sub(r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} ', '', server_text, flags=re.M)
    command = r'^clientCommand: ' + re.escape(CLIENT_NAME) + r' : \d+ : disconnect\s*$'
    for request in re.finditer(command, trace, re.M):
        tail = trace[request.end():]
        callback = re.search(r'^ClientDisconnect:\s+0\b[^\n]*$', tail, re.M)
        if callback and re.search(r'^Going to CS_ZOMBIE for[ \t]*$', tail[callback.end():], re.M):
            return True
    return False


def drain_disconnect(host, home, stdout, timeout=2):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        current = read(home / 'base/qconsole.log') + '\n' + read(stdout)
        if drop_completed(current):
            return True
        if host.poll() is not None or any(ERRORS.search(line) for line in current.splitlines()):
            return False
        time.sleep(.05)
    return False


def connection_evidence(server_text, game_text, client_text):
    # Pinned OpenJK prefixes each console print with local wall-clock time.
    # Keep original logs as evidence; normalize only for scalar trace parsing.
    trace = re.sub(r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} ', '', client_text, flags=re.M)
    snapshots = [dict(sequence=int(sequence), delta=int(delta), ping=int(ping))
        for sequence, delta, ping in re.findall(r'^\s*snapshot:(-?\d+)\s+delta:(-?\d+)\s+ping:(-?\d+)\s*$', trace, re.M)]
    frontiers = lambda text: list(dict.fromkeys(line for line in text.splitlines() if ERRORS.search(line)))
    return dict(serverActive=f'Going from CS_PRIMED to CS_ACTIVE for {CLIENT_NAME}' in server_text,
        serverBegin=bool(re.search(r'\bClientBegin:\s+0\b', game_text)),
        serverDisconnect=bool(re.search(r'\bClientDisconnect:\s+0\b', game_text)),
        serverDropCompleted=drop_completed(server_text),
        activeActionObserved=bool(re.search(r'^' + MARKER + r'\s*$', trace, re.M)),
        windowedObserved=bool(re.search(r'800\s*x\s*600\s+windowed', client_text, re.I)),
        snapshots=snapshots, validSnapshotCount=len({row['sequence'] for row in snapshots}),
        deltaSnapshotObserved=any(row['delta'] > 0 for row in snapshots),
        serverFrontiers=frontiers(server_text), clientFrontiers=frontiers(client_text),
        unpureDrop='Unpure client detected' in server_text)


def valid_capture(raw):
    if len(raw) > 16 * 1024 * 1024 or raw[:8] != b'\x89PNG\r\n\x1a\n':
        return False
    cursor, channels, compressed, ended = 8, None, bytearray(), False
    while cursor + 12 <= len(raw):
        length = struct.unpack_from('>I', raw, cursor)[0]
        if length > len(raw) - cursor - 12:
            return False
        kind = raw[cursor + 4:cursor + 8]
        body = raw[cursor + 8:cursor + 8 + length]
        checksum = struct.unpack_from('>I', raw, cursor + 8 + length)[0]
        if binascii.crc32(kind + body) & 0xffffffff != checksum:
            return False
        if kind == b'IHDR':
            if cursor != 8 or length != 13:
                return False
            width, height, bits, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', body)
            if (width, height, bits, compression, filtering, interlace) != (800, 600, 8, 0, 0, 0) or color not in (2, 6):
                return False
            channels = 3 if color == 2 else 4
        elif kind == b'IDAT':
            if channels is None:
                return False
            compressed.extend(body)
        elif kind == b'IEND':
            if length:
                return False
            ended = True
        elif not kind[0] & 0x20:
            return False  # Unknown critical PNG record.
        cursor += length + 12
        if ended:
            break
    if not ended or cursor != len(raw) or channels is None:
        return False
    stride = 800 * channels + 1
    expected = stride * 600
    try:
        decoder = zlib.decompressobj()
        pixels = decoder.decompress(compressed, expected + 1)
    except zlib.error:
        return False
    return (len(pixels) == expected and decoder.eof and not decoder.unused_data
        and not decoder.unconsumed_tail and all(pixels[row * stride] <= 4 for row in range(600)))


def capture_identity(path):
    raw = path.read_bytes()
    return dict(path=str(path.relative_to(ROOT)), sha256=sha(path), png800x600=valid_capture(raw))


def run(args):
    installation, output, server, client, renderer, server_engine, client_engine = validate(args)
    before = inventory(installation)
    module_candidates = modules(installation)
    homes = [output / name for name in ('server-home', 'client-home')]
    hostname = 'jka-maintained-' + uuid.uuid4().hex[:12]
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    host_command = launch_command(server, server_engine, installation, homes[0], args, port, hostname)
    peer_command = None if args.server_only else launch_command(client, client_engine, installation, homes[1], args, port)
    output.mkdir(parents=True)
    for home, engine, label in zip(homes, (server_engine, client_engine), (hostname, None)):
        game = home / 'base'
        game.mkdir(parents=True)
        for name in ('jampconfig.cfg', 'jampconfig_server.cfg'):
            (game / name).write_text('', encoding='ascii')
        config = configuration(engine, args.frames, label, getattr(args, 'record_demos', False))
        (game / CONFIG).write_text(config, encoding='ascii')
        (output / ('server-' if label else 'client-') / CONFIG).parent.mkdir()
        (output / ('server-' if label else 'client-') / CONFIG).write_text(config, encoding='ascii')
    binaries = [server, *((client, renderer) if client else ())]
    identities = {str(path): sha(path) for path in binaries}
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        profile='jka-mp', direction=args.direction, map=args.map, pure=args.pure,
        readinessOnly=args.server_only, recordingRequested=getattr(args, 'record_demos', False), port=port, timeout=args.timeout,
        serverCommand=host_command, clientCommand=peer_command, binarySha256=identities,
        openjkReferenceCommit=OPENJK_COMMIT, installation=str(installation), moduleCandidates=module_candidates,
        sourceCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        worktreeDirty=bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
        scope='Maintained-peer admission and snapshot subset; retail-engine interoperability remains open',
        desktopSwitched=False, inputInjected=False, visualPassed=None, passed=False)
    processes = []
    stopped = True
    constructing = False
    begin = time.monotonic()
    try:
        stopped = False
        constructing = True
        host = launch_private(host_command, server, homes[0], output / 'server-stdout.log')
        processes.append(host)
        constructing = False
        report['serverDesktop'] = host.name
        report['readyIdentity'] = ready_identity(host, port, args.map, hostname)
        if not args.server_only:
            constructing = True
            peer = launch_private(peer_command, client, homes[1], output / 'client-stdout.log')
            processes.append(peer)
            constructing = False
            report['clientDesktop'] = peer.name
            deadline = time.monotonic() + args.timeout
            while peer.poll() is None and time.monotonic() < deadline and host.poll() is None:
                current = read(homes[1] / 'base/qconsole.log') + '\n' + read(output / 'client-stdout.log')
                failure = next((line for line in current.splitlines() if ERRORS.search(line)), None)
                if failure:
                    # Error dialogs on a private desktop may stay open. Stop
                    # the owned child after recording its explicit frontier.
                    report['earlyStopReason'] = failure
                    report['earlyStopSource'] = 'client'
                    break
                server_current = read(homes[0] / 'base/qconsole.log') + '\n' + read(output / 'server-stdout.log')
                failure = next((line for line in server_current.splitlines()
                    if ERRORS.search(line) or 'Unpure client detected' in line), None)
                if failure:
                    report['earlyStopReason'] = failure
                    report['earlyStopSource'] = 'server'
                    break
                time.sleep(.2)
            report['clientExit'] = peer.poll()
            report['clientForced'] = report['clientExit'] is None
            report['serverExitedDuringClient'] = host.poll() is not None
            if report['clientExit'] == 0 and not report['serverExitedDuringClient']:
                report['disconnectDrainComplete'] = drain_disconnect(host, homes[0], output / 'server-stdout.log')
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        report['error'] = f'{type(error).__name__}: {error}'
    finally:
        stopped = True
        if processes:
            # Preserve the selected server's natural exit before close() can
            # terminate it. Readiness failures may occur before a peer exists.
            report['serverExitBeforeCleanup'] = processes[0].poll()
            report['serverForced'] = report['serverExitBeforeCleanup'] is None
        for process in reversed(processes):
            try:
                process.close()
            except (OSError, subprocess.TimeoutExpired) as error:
                stopped = False
                report.setdefault('stopErrors', []).append(str(error))
        # A constructor may have failed after partially creating a process.
        if constructing:
            stopped = False
        server_text = read(homes[0] / 'base/qconsole.log') + '\n' + read(output / 'server-stdout.log')
        game_text = read(homes[0] / 'base/games.log')
        client_text = read(homes[1] / 'base/qconsole.log') + '\n' + read(output / 'client-stdout.log')
        for name, text in (('server-console.log', server_text), ('server-game.log', game_text), ('client-console.log', client_text)):
            (output / name).write_text(text, encoding='utf-8')
        report.update(connection_evidence(server_text, game_text, client_text))
        report['demos'] = []
        if report['recordingRequested']:
            try:
                if not stopped:
                    raise ValueError('Demo retention requires confirmed process teardown')
                sources = [('server', homes[0], list((homes[0] / 'base/demos/server').glob('*.dm_26'))),
                    ('client', homes[1], list((homes[1] / 'base/demos').glob('jka-maintained-client.dm_26')))]
                if 'Server demo recording to ' not in server_text or 'Stopped server demo recording: ' not in server_text:
                    raise ValueError('Native server demo did not start and finalize')
                if 'recording to demos/jka-maintained-client.dm_26.' not in client_text or 'Stopped demo.' not in client_text:
                    raise ValueError('Maintained client demo did not start and stop')
                validated = []
                for origin, home, selected in sources:
                    if len(selected) != 1:
                        raise ValueError(f'Expected exactly one generated {origin} .dm_26')
                    raw, details = generated_demo(selected[0], home)
                    validated.append((origin, raw, details))
                for origin, raw, details in validated:
                    target = output / 'demos' / f'{origin}.dm_26'
                    target.parent.mkdir(exist_ok=True)
                    target.write_bytes(raw)
                    report['demos'].append(dict(origin=origin, path=str(target.relative_to(ROOT)), **details))
            except (OSError, ValueError) as error:
                report['demoError'] = str(error)
        report['screenshots'] = []
        for source in homes[1].rglob('jka-maintained-peer.png'):
            target = output / 'jka-maintained-peer.png'
            if target.exists():
                report['error'] = 'Multiple engine captures at the expected name'
                break
            target.write_bytes(source.read_bytes())
            report['screenshots'].append(capture_identity(target))
        report['nativeCaches'] = [dict(home=home.name, path=str(path.relative_to(home)), sha256=sha(path))
            for home in homes for path in home.rglob('*') if path.is_file() and path.suffix.casefold() in ('.dll', '.tmp')]
        report['installationUnchanged'] = before == inventory(installation)
        report['binariesUnchanged'] = all(sha(Path(path)) == digest for path, digest in identities.items())
        report['elapsedSeconds'] = round(time.monotonic() - begin, 3)
        common = bool(report.get('readyIdentity') and not report.get('error') and not report.get('stopErrors')
            and report['installationUnchanged'] and report['binariesUnchanged'] and not report['serverFrontiers'])
        admission = bool(report.get('clientExit') == 0 and not report.get('clientForced')
            and not report.get('serverExitedDuringClient') and report['serverActive'] and report['serverBegin']
            and report['serverDisconnect'] and report['serverDropCompleted']
            and report['activeActionObserved'] and report['windowedObserved'] and report['validSnapshotCount'] >= 2
            and report['deltaSnapshotObserved'] and not report['clientFrontiers'] and not report['unpureDrop']
            and len(report['screenshots']) == 1 and report['screenshots'][0]['png800x600'])
        recording = not report['recordingRequested'] or (len(report['demos']) == 2 and not report.get('demoError'))
        report['passed'] = common and recording and (args.server_only or admission)
        report['homesRemoved'] = False
        if stopped:
            try:
                for home in homes:
                    remove_home(home, output)
                report['homesRemoved'] = True
            except (OSError, RuntimeError, ValueError) as error:
                report['cleanupError'] = str(error)
                report['passed'] = False
        else:
            report['passed'] = False
        (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: report.get(key) for key in ('direction', 'readinessOnly', 'passed',
        'clientExit', 'clientForced', 'serverExitBeforeCleanup', 'serverForced', 'serverActive', 'serverBegin', 'serverDisconnect', 'serverDropCompleted', 'validSnapshotCount',
        'deltaSnapshotObserved', 'unpureDrop', 'screenshots', 'serverFrontiers', 'clientFrontiers', 'earlyStopReason', 'error', 'stopErrors')}, indent=2))
    return report['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--direction', choices=('fntech3-to-openjk', 'openjk-to-fntech3'), required=True)
    parser.add_argument('--server-only', action='store_true', help='OpenJK readiness only; no fnTech3 process')
    parser.add_argument('--record-demos', action='store_true', help='Capture bounded native-server and maintained-client demos on the reverse pair')
    parser.add_argument('--client', type=Path, default=ROOT / 'meson/build/win32-dev/fntech3.exe')
    parser.add_argument('--server', type=Path, default=ROOT / 'meson/build/win32-dev/fntech3.ded.exe')
    parser.add_argument('--openjk-directory', type=Path, default=ROOT / '.tmp/runtime/jka-openjk-build')
    parser.add_argument('--installation', type=Path, default=Path('E:/Games/Jedi Academy'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), default='glx')
    parser.add_argument('--map', default='mp/ffa1')
    parser.add_argument('--pure', action='store_true')
    parser.add_argument('--frames', type=int, default=180)
    parser.add_argument('--timeout', type=float, default=60)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows private-desktop harness required')
    try:
        return 0 if run(args) else 1
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.error(str(error))


if __name__ == '__main__':
    raise SystemExit(main())
