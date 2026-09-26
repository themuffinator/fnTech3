#!/usr/bin/env python3
"""Probe the Q3Test releases: the retail engines and fnTech3's q3test profile.

Windows x86 only. Every process runs windowed on an unswitched private desktop
with mouse and joystick input disabled; console commands from startup configs
drive it. No keyboard or mouse input is injected and no operating-system
capture is taken: screenshots come from each engine's own `screenshot`
command. Installations stay read-only. Retail engines run from fixed runtime
copies at the fixed paths the 2026-09-20 point-release audit first used,
.tmp/q3-point-releases/r/<n>/, so a firewall decision made once stays valid;
fnTech3 runs from its build with a fresh, isolated home per run.

The retail 1.02, 1.03 and 1.05 engines fault while listing the host's IPv4
addresses: their NET_GetLocalAddress steps through the list two entries at a
time and reads past its terminator on a host with an odd number of addresses
(docs/fntech3/Q3TEST.md). They run under cdb with a breakpoint that ends that
loop before its first entry; nothing on disk is patched.

Scenarios:
  retail-demo     a retail client plays a recording shipped in its pak,
                  optionally as a timedemo, taking engine screenshots
  retail-server   a retail dedicated server starts a map and answers the
                  connectionless getinfo and getstatus queries
  fntech3-server  fntech3.ded.exe starts maps under the q3test profile
  retail-client   fntech3.ded.exe serves a map on 127.0.0.1 and a retail
                  client of the same release joins it and takes screenshots
  protocol-client the headless protocol client (fntech3_q3test_client_probe)
                  joins the release's retail server (--target retail) or
                  fntech3.ded.exe (--target fntech3) on 127.0.0.1, walks and
                  leaves; for 1.02 and 1.03, which have no retail client
  fntech3-client  fntech3.exe joins a server on 127.0.0.1 -- fntech3.ded.exe
                  (--target fntech3), the release's retail server (--target
                  retail) or its own listen server (--target local) -- and
                  takes engine screenshots once it is in the game; with
                  --target menu it stays disconnected and captures its menus,
                  with --target demo it plays --demo (optionally as a
                  --timedemo) and captures it; --video also records the
                  replay with the engine's `video` command through the
                  legacy mixer and checks the sound it mixed

Nobody hears these probes: a client on a private desktop mixes into a silent
buffer, which is what `video` writes beside the frames. The check reports the
track's level and looks for every effect the replay started (`s_show 1`) in
it by normalised cross-correlation with the effect's own WAV from the
installation's paks (scripts/probe_ritual_audio.py).

Each run writes a JSON report and the logs into a fresh --output directory
under .tmp/.
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
from windows_private_process import WindowsPrivateProcess  # noqa: E402

CDB = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / \
    'Windows Kits/10/Debuggers/x86/cdb.exe'
RELEASES = ('1.02', '1.03', '1.05', '1.06', '1.07', '1.08')
# `test esi, esi` just after NET_GetLocalAddress loads the first address;
# clearing esi there ends the loop before it reads any entry.
NET_LOOP_BYPASS = {'1.02': 0x00428265, '1.03': 0x00428a35, '1.05': 0x004291e5}
MODULES = ('quake3.exe', 'qagamex86.dll', 'cgamex86.dll', 'uix86.dll')
# The audit's runtime folder for each release (docs/fntech3/2026-09-20-q3-point-releases.json).
RUNTIME_SLOTS = {'1.03': '19', '1.05': '20', '1.06': '21', '1.02': '22', '1.07': '23', '1.08': '24'}
# Files a retail engine writes into its own demoq3/; cleared before each run
# so no run inherits another's configuration or captures.
GENERATED = ('q3config.cfg', 'qconsole.log', 'probe.cfg', 'games.log')
PROTOCOLS = {'1.02': 35, '1.03': 35, '1.05': 36, '1.06': 38, '1.07': 39, '1.08': 40}
RETAIL_CLIENT = ['+set', 'logfile', '2', '+set', 'r_fullscreen', '0', '+set', 'r_mode', '3',
                 '+set', 'in_mouse', '0', '+set', 'in_joystick', '0', '+set', 's_initsound', '0']


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inside(path, parent):
    return path == parent or parent in path.parents


def git_head():
    try:
        return subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def retail_runtime(installation, release):
    """Refresh the fixed runtime copy of a release and return its path."""
    runtime = ROOT / '.tmp' / 'q3-point-releases' / 'r' / RUNTIME_SLOTS[release]
    (runtime / 'demoq3').mkdir(parents=True, exist_ok=True)
    for name in MODULES:
        source = installation / name
        target = runtime / name
        if source.is_file():
            if not target.is_file() or sha(target) != sha(source):
                shutil.copy2(source, target)
        elif target.exists():
            target.unlink()
    for pak in sorted((installation / 'demoq3').glob('pak*.pk3')):
        target = runtime / 'demoq3' / pak.name
        if not target.is_file() or target.stat().st_size != pak.stat().st_size or sha(target) != sha(pak):
            shutil.copy2(pak, target)
    for name in GENERATED:
        stale = runtime / 'demoq3' / name
        if stale.exists():
            stale.unlink()
    for stale in (runtime / 'demoq3').glob('probe*.cfg'):
        stale.unlink()
    shots = runtime / 'demoq3' / 'screenshots'
    if shots.exists():
        shutil.rmtree(shots)
    return runtime


def retail_command(runtime, release, arguments):
    exe = runtime / 'quake3.exe'
    if release in NET_LOOP_BYPASS:
        if not CDB.is_file():
            raise SystemExit(f'cdb.exe is required to start retail {release}: {CDB}')
        script = f'bp 0x{NET_LOOP_BYPASS[release]:08x} "r esi=0; gc"; g'
        return [str(CDB), '-o', '-G', '-c', script, str(exe)] + arguments
    return [str(exe)] + arguments


def run(command, cwd, stdout, timeout, until=None, log=None):
    """Run one private-desktop process. `until(text)` may end it early once
    the log shows what the probe waited for."""
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, cwd, stdout)
    reached = False
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            time.sleep(0.25)
            if until and log and log.exists():
                if until(log.read_text(encoding='latin-1', errors='replace')):
                    reached = True
                    break
        code = process.poll()
        forced = code is None
    finally:
        try:
            process.close()
        except OSError:
            pass
    text = log.read_text(encoding='latin-1', errors='replace') if log and log.exists() else ''
    return dict(exitCode=code, forced=forced, reached=reached,
                elapsedSeconds=round(time.monotonic() - begin, 3)), text


def write_cfg(path, lines):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(''.join(line + '\n' for line in lines), encoding='latin-1')


def query(port, message, timeout=2.0):
    """Send one connectionless packet to 127.0.0.1 and return the reply text."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.sendto(b'\xff\xff\xff\xff' + message.encode('latin-1'), ('127.0.0.1', port))
        try:
            data, _ = sock.recvfrom(65536)
        except socket.timeout:
            return None
    return data[4:].decode('latin-1', errors='replace') if data[:4] == b'\xff' * 4 else None


def scenario_retail_demo(args, report):
    runtime = retail_runtime(args.installation, args.release)
    demoq3 = runtime / 'demoq3'
    demos = demoq3 / 'demos'
    if demos.exists():
        shutil.rmtree(demos)
    if args.demo_file:
        # A recording made elsewhere, e.g. by fntech3.exe, played from the
        # runtime copy's own demos folder (the installation stays untouched).
        demos.mkdir(parents=True)
        shutil.copy2(args.demo_file, demos / args.demo_file.name)
        args.demo = args.demo_file.stem
        report['demoFile'] = dict(name=args.demo_file.name, sha256=sha(args.demo_file))
    # Everything goes on the command line. Window, input and sound settings
    # must precede the renderer, and the 1.02 to 1.05 engines, restricted to
    # their pak, cannot exec a loose config at all.
    actions = []
    for frame in args.frames or []:
        actions += ['+wait', str(frame), '+screenshot']
    command = retail_command(runtime, args.release, RETAIL_CLIENT + [
        '+set', 'net_ip', '127.0.0.1', '+set', 'net_port', str(args.port),
        '+set', 'timedemo', str(int(args.timedemo)), '+demo', args.demo] + actions)
    log = demoq3 / 'qconsole.log'
    timing = re.compile(r'^(\d+) frames, ([\d.]+) seconds: ([\d.]+) fps', re.M)
    status, text = run(command, runtime, args.output / 'stdout.log', args.timeout,
                       until=lambda t: bool(timing.search(t)), log=log)
    (args.output / 'qconsole.log').write_text(text, encoding='latin-1')
    shots = []
    for shot in sorted((demoq3 / 'screenshots').glob('*')) if (demoq3 / 'screenshots').is_dir() else []:
        target = args.output / shot.name
        shutil.copy2(shot, target)
        shots.append(dict(name=shot.name, sha256=sha(target)))
    found = timing.search(text)
    report.update(command=command, status=status, screenshots=shots,
                  timedemo=dict(frames=int(found[1]), seconds=float(found[2])) if found else None)
    report['passed'] = bool(found) if args.timedemo else not status['forced'] or status['reached']
    return report['passed']


def scenario_retail_server(args, report):
    runtime = retail_runtime(args.installation, args.release)
    demoq3 = runtime / 'demoq3'
    command = retail_command(runtime, args.release, [
        '+set', 'dedicated', '1', '+set', 'logfile', '2', '+set', 'net_ip', '127.0.0.1',
        '+set', 'net_port', str(args.port), '+set', 'sv_maxclients', '2', '+map', args.map])
    log = demoq3 / 'qconsole.log'
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, runtime, args.output / 'stdout.log')
    replies = {}
    try:
        while process.poll() is None and time.monotonic() - begin < args.timeout:
            time.sleep(0.5)
            text = log.read_text(encoding='latin-1', errors='replace') if log.exists() else ''
            if 'Server: ' + args.map in text or 'Server: ' + args.map.upper() in text:
                time.sleep(1.0)
                for message in ('getinfo xxx', 'getstatus'):
                    replies[message] = query(args.port, message)
                break
        code = process.poll()
    finally:
        try:
            process.close()
        except OSError:
            pass
    text = log.read_text(encoding='latin-1', errors='replace') if log.exists() else ''
    (args.output / 'qconsole.log').write_text(text, encoding='latin-1')
    report.update(command=command, exitCode=code, replies=replies)
    report['passed'] = bool(replies.get('getinfo xxx'))
    return report['passed']


def fntech3_server_command(args, home, extra):
    return [str(args.server), '+set', 'com_gameProfile', 'q3test', '+set', 'fs_basepath', str(args.installation),
            '+set', 'fs_homepath', str(home), '+set', 'net_ip', '127.0.0.1', '+set', 'net_port', str(args.port),
            '+set', 'logfile', '2', '+set', 'developer', '1', '+set', 'sv_maxclients', '4',
            '+set', 'rconPassword', 'probe', '+set', 'rcon_password', 'probe'] + \
        [item for cvar, value in zip((args.server_set or [])[0::2], (args.server_set or [])[1::2])
         for item in ('+set', cvar, value)] + extra


def rcon(port, password, command, timeout=2.0):
    """Send one rcon command to 127.0.0.1 and return the printed reply."""
    return query(port, f'rcon {password} {command}', timeout)


def scenario_retail_client(args, report):
    """fntech3.ded.exe serves a map on 127.0.0.1 and a retail client of the
    same release connects to it, takes engine screenshots and quits."""
    home = args.output / 'server-home'
    server_log = home / 'demoq3' / 'qconsole.log'
    server = WindowsPrivateProcess(fntech3_server_command(args, home, ['+map', args.map]),
                                   args.server.parent, args.output / 'server-stdout.log')
    runtime = retail_runtime(args.installation, args.release)
    log = runtime / 'demoq3' / 'qconsole.log'
    client = None
    try:
        begin = time.monotonic()
        while server.poll() is None and time.monotonic() - begin < 60:
            time.sleep(0.5)
            text = server_log.read_text(encoding='latin-1', errors='replace') if server_log.exists() else ''
            if 'Server: ' + args.map in text and text.rstrip().endswith('-----------------------------------'):
                break
        time.sleep(1.0)
        # Retail's wait takes no count, each is one frame, and frames outside
        # a level are not held to com_maxfps (790 took 1.2 s): waits cannot
        # time anything. So the client takes a series of widely spaced
        # screenshots and never quits by itself; the harness ends it a fixed
        # time after the server reports it in the game. Steps are chained
        # config files of plain waits, which 1.06 and later exec from disk;
        # 1.02 to 1.05 read no loose file but q3config.cfg.
        demoq3 = runtime / 'demoq3'
        actions = ['+set', 'cl_motd', '0', '+set', 'net_ip', '127.0.0.1', '+set', 'net_port', str(args.port + 10),
                   '+set', 'name', 'RetailProbe']
        pairs = args.client_set or []
        for cvar, value in zip(pairs[0::2], pairs[1::2]):
            actions += ['+set', cvar, value]
        if args.release in NET_LOOP_BYPASS:
            # q3config.cfg, the one loose file these engines read, defines a
            # tree of vstr'd waits and a loop that screenshots after every
            # 20 x 8 x 8 x 8 x 5 of them; the command line starts it once every
            # setting above has been applied. Each level is short, since a
            # longer insertion overflows 1.05's command buffer.
            write_cfg(demoq3 / 'q3config.cfg', [
                'seta r_fullscreen "0"', 'seta r_mode "3"', 'seta in_mouse "0"', 'seta in_joystick "0"',
                'seta s_initsound "0"', 'seta cl_motd "0"',
                'set pa "' + ';'.join(['wait'] * 20) + '"', 'set pb "' + ';'.join(['vstr pa'] * 8) + '"',
                'set pc "' + ';'.join(['vstr pb'] * 8) + '"', 'set pd "' + ';'.join(['vstr pc'] * 8) + '"',
                'set pe "' + ';'.join(['vstr pd'] * 5) + '"',
                'set probe_loop "vstr pe;screenshot;vstr probe_loop"',
                f'set probe_start "connect 127.0.0.1:{args.port};vstr probe_loop"'])
            actions += ['+vstr', 'probe_start']
        else:
            steps = [f'connect 127.0.0.1:{args.port}']
            for frame in args.frames or [4000] * 8:
                steps += ['wait'] * frame + ['screenshot']
            chunks = [steps[i:i + 400] for i in range(0, len(steps), 400)]
            for index, chunk in enumerate(chunks):
                lines = list(chunk)
                if index + 1 < len(chunks):
                    lines.append(f'exec probe{index + 1}.cfg')
                write_cfg(demoq3 / (f'probe{index}.cfg' if index else 'probe.cfg'), lines)
            actions += ['+exec', 'probe.cfg']
        command = retail_command(runtime, args.release, RETAIL_CLIENT + actions)
        client = WindowsPrivateProcess(command, runtime, args.output / 'client-stdout.log')
        begin = time.monotonic()
        entered = None
        # --rcon SECONDS COMMAND: operator commands, timed from entering.
        pending = sorted((float(when), command) for when, command in zip((args.rcon or [])[0::2], (args.rcon or [])[1::2]))
        sent = []
        while client.poll() is None and time.monotonic() - begin < args.timeout:
            time.sleep(0.5)
            if entered is None:
                text = server_log.read_text(encoding='latin-1', errors='replace') if server_log.exists() else ''
                if 'entered the game' in text:
                    entered = time.monotonic()
                continue
            while pending and time.monotonic() - entered >= pending[0][0]:
                when, command = pending.pop(0)
                sent.append(dict(atSeconds=when, command=command, reply=rcon(args.port, 'probe', command)))
            if time.monotonic() - entered >= args.hold:
                break
        report['rcon'] = sent
        report.update(clientCommand=command, clientExit=client.poll(),
                      enteredAfterSeconds=round(entered - begin, 3) if entered else None)
    finally:
        for process in (client, server):
            if process is None:
                continue
            try:
                process.close()
            except OSError:
                pass
    client_text = log.read_text(encoding='latin-1', errors='replace') if log.exists() else ''
    server_text = server_log.read_text(encoding='latin-1', errors='replace') if server_log.exists() else ''
    (args.output / 'client-qconsole.log').write_text(client_text, encoding='latin-1')
    (args.output / 'server-qconsole.log').write_text(server_text, encoding='latin-1')
    shots = []
    demoq3 = runtime / 'demoq3'
    for shot in sorted((demoq3 / 'screenshots').glob('*')) if (demoq3 / 'screenshots').is_dir() else []:
        target = args.output / shot.name
        shutil.copy2(shot, target)
        shots.append(dict(name=shot.name, sha256=sha(target)))
    report.update(screenshots=shots, server_sha256=sha(args.server))
    report['passed'] = bool(shots) and 'ERROR' not in client_text and report.get('enteredAfterSeconds') is not None
    return report['passed']


def fntech3_client_command(args, home, extra):
    """fntech3.exe under the q3test profile: windowed, no mouse or joystick,
    loopback only, and an isolated home."""
    command = [str(args.client), '+set', 'com_gameProfile', 'q3test', '+set', 'fs_basepath', str(args.installation),
               '+set', 'fs_homepath', str(home), '+set', 'r_fullscreen', '0', '+set', 'r_mode', '-1',
               '+set', 'r_customwidth', '800', '+set', 'r_customheight', '600', '+set', 'in_mouse', '0',
               '+set', 'in_joystick', '0', '+set', 'net_ip', '127.0.0.1', '+set', 'net_port', str(args.port + 20),
               '+set', 'cl_motd', '0', '+set', 'logfile', '2', '+set', 'developer', '1', '+set', 'name', 'fnTech3Probe']
    pairs = args.client_set or []
    for cvar, value in zip(pairs[0::2], pairs[1::2]):
        command += ['+set', cvar, value]
    return command + extra


def video_sound(avi, log_text, game_dir, output):
    """What a `video` capture mixed: the track's level, the share of it above
    -60 dBFS, and each effect the replay started (`s_show 1` lines after the
    capture began, at the painted sample time) looked for in it. The track is
    kept as a WAV; the AVI's frames are not needed and are deleted."""
    import wave
    import numpy as np
    import probe_ritual_audio as audio
    rate, pcm = audio.avi_audio(avi)
    mono = pcm.mean(axis=1)
    block = rate // 10
    blocks = len(mono) // block
    rms = np.sqrt(np.mean(mono[:blocks * block].reshape(blocks, block) ** 2, axis=1)) if blocks else np.zeros(1)
    begun = log_text.find('Q3TEST_CLIENT_ACTIVE')
    starts = [(int(m[1]) * 1000.0 / rate, m[2]) for m in
              re.finditer(r'^(\d+) : (sound/\S+?)\s*$', log_text[max(begun, 0):], re.M)]
    matched = audio.match_effects(pcm, rate, starts, audio.Effects(game_dir))
    track = output / (avi.stem + '.wav')
    with wave.open(str(track), 'wb') as wav:
        wav.setnchannels(pcm.shape[1])
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes((np.clip(pcm, -1.0, 32767 / 32768.0) * 32768.0).astype('<i2').tobytes())
    result = dict(avi=dict(name=avi.name, bytes=avi.stat().st_size, sha256=sha(avi)),
                  track=dict(name=track.name, sha256=sha(track)), level=audio.level(pcm, rate),
                  audibleShare=round(float(np.mean(rms > 10 ** (-60 / 20))), 3), starts=len(starts), **matched)
    avi.unlink()
    return result


def scenario_fntech3_client(args, report):
    """fntech3.exe joins a server on 127.0.0.1 and, from its first snapshot,
    takes engine screenshots between waits, then quits."""
    home = args.output / 'client-home'
    game = home / 'demoq3'
    client_log = game / 'qconsole.log'
    server = None
    server_log = None
    runtime = None
    if args.target == 'fntech3':
        server_home = args.output / 'server-home'
        server_log = server_home / 'demoq3' / 'qconsole.log'
        server = WindowsPrivateProcess(fntech3_server_command(args, server_home, ['+map', args.map]),
                                       args.server.parent, args.output / 'server-stdout.log')
    elif args.target == 'retail':
        runtime = retail_runtime(args.installation, args.release)
        server_log = runtime / 'demoq3' / 'qconsole.log'
        server = WindowsPrivateProcess(retail_command(runtime, args.release, [
            '+set', 'dedicated', '1', '+set', 'logfile', '2', '+set', 'net_ip', '127.0.0.1',
            '+set', 'net_port', str(args.port), '+set', 'sv_maxclients', '4', '+map', args.map]),
            runtime, args.output / 'server-stdout.log')
    # The steps run from the first snapshot (activeAction); waits count frames.
    shots = []
    for frame in args.frames or [60, 200, 200, 200]:
        shots += [f'wait {frame}', 'screenshot']
    write_cfg(game / 'q3config.cfg', [])
    if args.target == 'demo' and args.demo_file:
        (game / 'demos').mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.demo_file, game / 'demos' / args.demo_file.name)
        args.demo = args.demo_file.stem
        report['demoFile'] = dict(name=args.demo_file.name, sha256=sha(args.demo_file))
    record = [f'record {args.record}'] if args.record else []
    stop = ['stoprecord'] if args.record else []
    # `video` runs only while a demo plays; quitting closes the file.
    video = [f'video {args.video}'] if args.video else []
    write_cfg(game / 'probe_active.cfg', ['echo Q3TEST_CLIENT_ACTIVE'] + video + record + shots + stop +
              ['wait 30', 'echo Q3TEST_CLIENT_DONE', 'quit'])
    if args.target == 'menu':
        # Disconnected, the engine opens the main menu by itself.
        write_cfg(game / 'probe.cfg', shots + ['wait 30', 'echo Q3TEST_CLIENT_DONE', 'quit'])
    elif args.target == 'demo' and args.timedemo:
        # A timedemo runs to its end, prints its rate and disconnects.
        write_cfg(game / 'probe.cfg', ['set timedemo 1', 'set nextdemo "echo Q3TEST_CLIENT_DONE; quit"',
                                       f'demo {args.demo}'])
    elif args.target == 'demo':
        write_cfg(game / 'probe.cfg', ['set activeAction "exec probe_active.cfg"', f'demo {args.demo}'])
    else:
        start = f'map {args.map}' if args.target == 'local' else f'connect 127.0.0.1:{args.port}'
        write_cfg(game / 'probe.cfg', ['set activeAction "exec probe_active.cfg"', start])
    client = None
    try:
        if server is not None:
            begin = time.monotonic()
            while server.poll() is None and time.monotonic() - begin < 60:
                time.sleep(0.5)
                text = server_log.read_text(encoding='latin-1', errors='replace') if server_log.exists() else ''
                if re.search(r'Server: ' + re.escape(args.map), text, re.I):
                    break
            time.sleep(1.5)
        # Only the legacy mixer writes a capture's sound; `s_show 1` prints
        # each effect it starts, with the painted sample time.
        sound = ['+set', 's_backend', 'legacy', '+set', 's_show', '1'] if args.video else []
        command = fntech3_client_command(args, home, sound + ['+exec', 'probe.cfg'])
        client = WindowsPrivateProcess(command, args.client.parent, args.output / 'client-stdout.log')
        begin = time.monotonic()
        while client.poll() is None and time.monotonic() - begin < args.timeout:
            time.sleep(0.5)
            text = client_log.read_text(encoding='latin-1', errors='replace') if client_log.exists() else ''
            if 'Q3TEST_CLIENT_DONE' in text:
                break
        # quit writes the configuration and disconnects
        for _ in range(20):
            if client.poll() is not None:
                break
            time.sleep(0.5)
        report.update(clientCommand=command, clientExit=client.poll(),
                      elapsedSeconds=round(time.monotonic() - begin, 3))
    finally:
        for process in (client, server):
            if process is None:
                continue
            try:
                process.close()
            except OSError:
                pass
    client_text = client_log.read_text(encoding='latin-1', errors='replace') if client_log.exists() else ''
    (args.output / 'client-qconsole.log').write_text(client_text, encoding='latin-1')
    if server_log is not None:
        server_text = server_log.read_text(encoding='latin-1', errors='replace') if server_log.exists() else ''
        (args.output / 'server-qconsole.log').write_text(server_text, encoding='latin-1')
    if args.record:
        demo = game / 'demos' / f'{args.record}.dm3'
        if demo.is_file():
            shutil.copy2(demo, args.output / demo.name)
            report['recorded'] = dict(name=demo.name, bytes=demo.stat().st_size, sha256=sha(demo))
        else:
            report['recorded'] = None
    captures = []
    shots_dir = game / 'screenshots'
    for shot in sorted(shots_dir.glob('*')) if shots_dir.is_dir() else []:
        target = args.output / shot.name
        shutil.copy2(shot, target)
        captures.append(dict(name=shot.name, sha256=sha(target)))
    errors = re.findall(r'^.*(?:ERROR:|Error:|crashed|recursive error|WARNING: Q3Test).*$', client_text, re.M)
    timing = re.search(r'^(\d+) frames, ([\d.]+) seconds: ([\d.]+) fps', client_text, re.M)
    report.update(target=args.target, screenshots=captures, errors=errors, client_sha256=sha(args.client),
                  active='Q3TEST_CLIENT_ACTIVE' in client_text or args.target == 'menu',
                  done='Q3TEST_CLIENT_DONE' in client_text,
                  timedemo=dict(frames=int(timing[1]), seconds=float(timing[2])) if timing else None)
    if args.target == 'demo' and args.timedemo:
        report['passed'] = report['done'] and timing is not None and not errors
        return report['passed']
    if args.video:
        avi = game / 'videos' / f'{args.video}.avi'
        report['video'] = video_sound(avi, client_text, args.installation / 'demoq3', args.output) \
            if avi.is_file() else None
        report['passed'] = report['done'] and bool(captures) and not errors and bool(report['video']) \
            and report['video']['audibleShare'] > 0.5
        return report['passed']
    if args.target == 'fntech3':
        report['server_sha256'] = sha(args.server)
    report['passed'] = report['done'] and bool(captures) and not errors and (
        not args.record or bool(report.get('recorded')))
    return report['passed']


def scenario_protocol_client(args, report):
    """The headless protocol client joins a server on 127.0.0.1, runs forward
    for --hold seconds and disconnects; its JSON summary and both logs are
    kept."""
    if args.target == 'fntech3':
        server_home = args.output / 'server-home'
        server_log = server_home / 'demoq3' / 'qconsole.log'
        server = WindowsPrivateProcess(fntech3_server_command(args, server_home, ['+map', args.map]),
                                       args.server.parent, args.output / 'server-stdout.log')
    else:
        runtime = retail_runtime(args.installation, args.release)
        server_log = runtime / 'demoq3' / 'qconsole.log'
        server = WindowsPrivateProcess(retail_command(runtime, args.release, [
            '+set', 'dedicated', '1', '+set', 'logfile', '2', '+set', 'net_ip', '127.0.0.1',
            '+set', 'net_port', str(args.port), '+set', 'sv_maxclients', '4', '+map', args.map]),
            runtime, args.output / 'server-stdout.log')
    summary = None
    try:
        begin = time.monotonic()
        while server.poll() is None and time.monotonic() - begin < 60:
            time.sleep(0.5)
            text = server_log.read_text(encoding='latin-1', errors='replace') if server_log.exists() else ''
            if re.search(r'Server: ' + re.escape(args.map), text, re.I):
                break
        time.sleep(1.5)
        command = [str(args.probe), '--protocol', str(PROTOCOLS[args.release]), '--server', f'127.0.0.1:{args.port}',
                   '--port', str(args.port + 30), '--seconds', str(args.hold)]
        run_probe = subprocess.run(command, capture_output=True, text=True, timeout=args.hold + 30)
        (args.output / 'probe-stdout.json').write_text(run_probe.stdout, encoding='utf-8')
        (args.output / 'probe-stderr.log').write_text(run_probe.stderr, encoding='utf-8')
        try:
            summary = json.loads(run_probe.stdout)
        except ValueError:
            summary = None
        report.update(probeCommand=command, probeExit=run_probe.returncode)
        time.sleep(2.0)
    finally:
        try:
            server.close()
        except OSError:
            pass
    server_text = server_log.read_text(encoding='latin-1', errors='replace') if server_log.exists() else ''
    (args.output / 'server-qconsole.log').write_text(server_text, encoding='latin-1')
    report.update(target=args.target, probe=summary, probe_sha256=sha(args.probe),
                  serverSawEnter='entered the game' in server_text,
                  serverSawDisconnect='disconnected' in server_text)
    if args.target == 'fntech3':
        report['server_sha256'] = sha(args.server)
    report['passed'] = bool(summary) and summary.get('connected') and summary.get('snapshots', 0) > 0 and \
        not summary.get('error') and summary.get('moved', 0) > 16 and report['serverSawEnter']
    return report['passed']


def scenario_fntech3_server(args, report):
    home = args.output / 'home'
    rows = []
    for name in args.maps:
        game = home / name / 'demoq3'
        write_cfg(game / 'probe.cfg', ['set sv_maxclients 2', f'map {name}',
                                       'wait 60', 'echo Q3TEST_SERVER_READY', 'quit'])
        command = [str(args.server), '+set', 'com_gameProfile', 'q3test',
                   '+set', 'fs_basepath', str(args.installation), '+set', 'fs_homepath', str(home / name),
                   '+set', 'net_enabled', '0', '+set', 'logfile', '2', '+exec', 'probe.cfg']
        status, text = run(command, args.server.parent, args.output / f'{name}-stdout.log', args.timeout,
                           log=game / 'qconsole.log')
        (args.output / f'{name}-qconsole.log').write_text(text, encoding='latin-1')
        errors = re.findall(r'^.*(?:ERROR:|Error:|crashed|recursive error).*$', text, re.M)
        row = dict(map=name, command=command, status=status, errors=errors,
                   ready='Q3TEST_SERVER_READY' in text)
        row['passed'] = row['ready'] and not errors and status['exitCode'] == 0
        rows.append(row)
        print(name, 'PASS' if row['passed'] else 'FAIL', flush=True)
    report.update(runs=rows, server_sha256=sha(args.server))
    report['passed'] = all(row['passed'] for row in rows)
    return report['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('scenario', choices=('retail-demo', 'retail-server', 'fntech3-server', 'retail-client',
                                             'fntech3-client', 'protocol-client'))
    parser.add_argument('--installation', type=Path, required=True,
                        help='Q3Test installation root (quake3.exe, modules, demoq3/)')
    parser.add_argument('--release', choices=RELEASES, help='required for retail scenarios')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--server', type=Path, help='fntech3.ded.exe')
    parser.add_argument('--client', type=Path, help='fntech3.exe')
    parser.add_argument('--probe', type=Path, help='fntech3_q3test_client_probe.exe')
    parser.add_argument('--demo', default='q3demo1')
    parser.add_argument('--demo-file', type=Path, help='retail-demo, fntech3-client --target demo: a .dm3 to copy in and play')
    parser.add_argument('--record', help='fntech3-client: record this demo name while in the game')
    parser.add_argument('--video', help="fntech3-client --target demo: capture the replay under this name with "
                                        "the engine's `video` command and check the sound it mixed")
    parser.add_argument('--timedemo', action='store_true')
    parser.add_argument('--frame', type=int, action='append', dest='frames',
                        help='retail-demo, fntech3-client: take a screenshot this many frames after the previous one')
    parser.add_argument('--target', choices=('fntech3', 'retail', 'local', 'menu', 'demo'), default='fntech3',
                        help='fntech3-client: the server it joins')
    parser.add_argument('--map', default='q3test1')
    parser.add_argument('--maps', nargs='+', default=['q3test1', 'q3test2'])
    parser.add_argument('--port', type=int, default=27960)
    parser.add_argument('--timeout', type=float, default=90)
    parser.add_argument('--hold', type=float, default=20,
                        help='retail-client: seconds to keep the client after it enters the game; '
                             'protocol-client: seconds the probe stays')
    parser.add_argument('--rcon', nargs=2, action='extend', metavar=('SECONDS', 'COMMAND'),
                        help='retail-client: an rcon command this long after the client enters the game')
    parser.add_argument('--server-set', nargs=2, action='extend', metavar=('CVAR', 'VALUE'),
                        help='another +set for fntech3.ded.exe')
    parser.add_argument('--client-set', nargs=2, action='extend', metavar=('CVAR', 'VALUE'),
                        help='retail-client, fntech3-client: another +set for the client')
    args = parser.parse_args()
    args.installation = args.installation.resolve()
    if args.demo_file:
        args.demo_file = args.demo_file.resolve()
    args.output = args.output.resolve()
    if not inside(args.output, ROOT / '.tmp'):
        parser.error('--output must be a new directory under the project .tmp directory')
    if args.output.exists():
        parser.error('use a fresh --output directory; evidence is never overwritten')
    if inside(args.output, args.installation) or inside(args.installation, args.output):
        parser.error('--output and --installation must be disjoint')
    if args.scenario.startswith('retail') and not args.release:
        parser.error('retail scenarios need --release')
    if args.scenario == 'fntech3-client':
        if not args.client:
            parser.error('fntech3-client needs --client')
        args.client = args.client.resolve()
        if args.target == 'retail' and not args.release:
            parser.error('--target retail needs --release')
        if args.video and (args.target != 'demo' or args.timedemo):
            parser.error('--video needs --target demo without --timedemo')
    if args.scenario == 'protocol-client':
        if not args.probe or not args.release:
            parser.error('protocol-client needs --probe and --release')
        args.probe = args.probe.resolve()
    if args.scenario in ('fntech3-server', 'retail-client') or (
            args.scenario in ('fntech3-client', 'protocol-client') and args.target == 'fntech3'):
        if not args.server:
            parser.error(f'{args.scenario} needs --server')
        args.server = args.server.resolve()
    args.output.mkdir(parents=True)
    before = {p.relative_to(args.installation).as_posix(): sha(p)
              for p in sorted(args.installation.rglob('*')) if p.is_file()}
    report = dict(scenario=args.scenario, release=args.release, commit=git_head(),
                  installation=str(args.installation), installationFiles=before)
    passed = {'retail-demo': scenario_retail_demo, 'retail-server': scenario_retail_server,
              'fntech3-server': scenario_fntech3_server,
              'retail-client': scenario_retail_client,
              'fntech3-client': scenario_fntech3_client,
              'protocol-client': scenario_protocol_client}[args.scenario](args, report)
    after = {p.relative_to(args.installation).as_posix(): sha(p)
             for p in sorted(args.installation.rglob('*')) if p.is_file()}
    report['installationUnchanged'] = before == after
    report['passed'] = bool(passed) and report['installationUnchanged']
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
