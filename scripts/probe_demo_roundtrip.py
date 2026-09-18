#!/usr/bin/env python3
"""Record and play demos through fnTech3, and exchange RTCW MP demos with retail.

Windows only. Every process runs windowed on an unswitched private desktop with
an isolated writable home and mouse/joystick input disabled. Console commands
from startup configs drive recording and playback; no mouse or keyboard input
is injected and no operating-system capture is taken. Screenshots come from the
engine's own screenshot commands.

Scenarios:
  self               fnTech3 records a local --profile session (q3, ql or
                     rtcw-mp), then plays it back normally and as a timedemo in
                     fresh processes.
  rtcw-mp-retail-in  retail WolfMP.exe records while connected to the fnTech3
                     dedicated server; fnTech3 then plays the retail-written demo.
  rtcw-mp-retail-out retail WolfMP.exe plays a demo fnTech3 recorded (--demo).
  rtcw-sp            fnTech3 loads an SP save (--save) and records once
                     synchronous clients are set; replay must then be refused
                     cleanly, because original SP cannot replay SP demos.

A pass means the file was written with the owning profile's extension, has a
well-formed envelope, and the player reached its completion marker, captured
its screenshots and exited normally without an engine error. It is not a
visual-parity or sustained-gameplay claim.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

COMPLETE = 'RTCW_DEMO_COMPLETE'
ERRORS = re.compile(r'^(?:ERROR:|.*Illegible server message|.*read past end of server message|'
                    r'.*Demo file was truncated|.*Server fatal crashed|.*recursive error after)', re.M)


# Local session contract for the self scenario: game directory, the command
# that starts a local server, extra settings, the demo extension the profile
# owns, and the console commands that clear the view before recording.
SESSIONS = {
    'q3': dict(game='baseq3', start=['+devmap', 'q3dm17'], settings=[], extension='dm_68', clear=''),
    'ql': dict(game='baseq3', start=['+map', 'campgrounds', 'ffa'], settings=[], extension='dm_91',
               clear=''),
    'rtcw-mp': dict(game='main', start=['+devmap', 'mp_beach'], settings=['set g_gametype 5'],
                    extension='dm_60', clear='CloseLimboMenu; -scores; '),
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in installation.rglob('*') if p.is_file()}


def envelope(path):
    """Walk the [sequence][length][bytes] records to the (-1, -1) terminator."""
    data = Path(path).read_bytes()
    cursor = messages = 0
    sequences = []
    while cursor + 8 <= len(data):
        sequence, size = struct.unpack_from('<ii', data, cursor)
        if (sequence, size) == (-1, -1):
            return dict(bytes=len(data), messages=messages, terminated=cursor + 8 == len(data),
                        firstSequence=sequences[0] if sequences else None,
                        lastSequence=sequences[-1] if sequences else None,
                        sequencesIncrease=all(b > a for a, b in zip(sequences, sequences[1:])),
                        sha256=hashlib.sha256(data).hexdigest())
        if size < 0 or size > 32768 or cursor + 8 + size > len(data):
            break
        sequences.append(sequence)
        messages += 1
        cursor += 8 + size
    return dict(bytes=len(data), messages=messages, terminated=False,
                sha256=hashlib.sha256(data).hexdigest())


def fntech3_command(client, installation, home, profile, extra):
    values = dict(com_gameProfile=profile, fs_basepath=str(installation), fs_homepath=str(home),
                  fs_game='', fs_cdpath='', r_fullscreen='0', r_windowedMode='-1',
                  r_windowedWidth='800', r_windowedHeight='600', in_mouse='0', in_joystick='0',
                  in_nograb='1', s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1',
                  cl_webuiEnable='0', cl_renderer='glx', logfile='2', net_ip='127.0.0.1',
                  net_port='0', net_ip6='::1', net_port6='0')
    command = [str(client)]
    for name, value in values.items():
        command += ['+set', name, value]
    return command + extra


def retail_command(client, installation, home, settings, commands):
    # Retail appends activeAction to the command buffer without a newline, so
    # the cgame's next command would join its last word; callers end with ';'.
    values = dict(fs_basepath=str(installation), fs_homepath=str(home), r_fullscreen='0',
                  r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1', logfile='2',
                  com_viewlog='0', s_initsound='0', cl_punkbuster='0', cl_allowDownload='0',
                  r_ignorehwgamma='1', **settings)
    command = [str(client)]
    for name, value in values.items():
        command += ['+set', name, value]
    return command + commands


def run(command, cwd, output, log, timeout):
    """Run one private-desktop process; return (exit, forced, elapsed, console)."""
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, cwd, output)
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            time.sleep(0.1)
        forced = process.poll() is None
        code = process.poll()
    finally:
        process.close()
    text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
    return code, forced, round(time.monotonic() - begin, 3), text


def write_config(game, name, lines):
    game.mkdir(parents=True, exist_ok=True)
    (game / name).write_text(''.join(line + '\n' for line in lines), encoding='utf-8')


def collect(home, destination, pattern):
    shots = []
    for capture in sorted(home.rglob(pattern)):
        target = destination / capture.name
        shutil.copy2(capture, target)
        shots.append(dict(name=target.name, sha256=sha(target)))
    return shots


def fntech3_playback(client, installation, output, profile, demo_file, timedemo, gamedir='main'):
    """Play demo_file in a fresh process and home; require the completion marker."""
    label = 'timedemo' if timedemo else 'playback'
    home = output / f'{label}-home'
    game = home / gamedir
    (game / 'demos').mkdir(parents=True)
    shutil.copy2(demo_file, game / 'demos' / demo_file.name)
    stem = demo_file.stem
    write_config(game, 'probe.cfg', [
        'set com_introplayed 1', 'set com_maxfps 60', 'set com_maxfpsUnfocused 60',
        'set com_maxfpsMinimized 60', 'set cl_allowDownload 0', 'set cl_motd 0',
        f'set timedemo {int(timedemo)}', 'set cl_quitOnDemoCompleted 1',
        f'set nextdemo "echo {COMPLETE}"',
        f'set activeAction "wait 30; screenshotPNG {label}-a; wait 90; screenshotPNG {label}-b"',
        f'demo {stem}'])
    command = fntech3_command(client, installation, home, profile, ['+exec', 'probe.cfg'])
    code, forced, elapsed, text = run(command, client.parent, output / f'{label}-stdout.log',
                                      game / 'qconsole.log', 120)
    (output / f'{label}-console.log').write_text(text, encoding='utf-8')
    shots = collect(home, output, f'{label}-*.png')
    timing = re.search(r'^(\d+) frames, ([\d.]+) seconds: ([\d.]+) fps', text, re.M)
    row = dict(command=command, exitCode=code, forcedTermination=forced, elapsedSeconds=elapsed,
               completionMarker=COMPLETE in text, screenshots=shots,
               engineErrors=ERRORS.findall(text)[:10],
               demoFile=f'demos/{demo_file.name}' in text or f'Demo file: demos/{demo_file.name}' in text)
    if timedemo:
        row['timedemo'] = timing.group(0) if timing else None
    row['passed'] = (code == 0 and not forced and row['completionMarker'] and len(shots) == 2
                     and not row['engineErrors'] and (not timedemo or bool(timing)))
    return row


def self_session(args, output):
    session = SESSIONS[args.profile]
    home = output / 'record-home'
    game = home / session['game']
    stem = f'fntech3-{args.profile}-probe'
    settings = ['set com_introplayed 1', 'set com_maxfps 60', 'set com_maxfpsUnfocused 60',
                'set com_maxfpsMinimized 60', 'set cl_allowDownload 0', 'set cl_motd 0',
                'set developer 0'] + session['settings'] + [f'set sv_master{i} ""' for i in range(1, 6)]
    settings.append(f'set activeAction "wait 180; {session["clear"]}wait 60; '
                    f'record {stem}; wait 360; screenshotPNG record-a; wait 120; stoprecord; wait 10; quit"')
    write_config(game, 'autoexec.cfg', settings)
    command = fntech3_command(args.client, args.installation, home, args.profile, session['start'])
    code, forced, elapsed, text = run(command, args.client.parent, output / 'record-stdout.log',
                                      game / 'qconsole.log', 150)
    (output / 'record-console.log').write_text(text, encoding='utf-8')
    demo = game / 'demos' / f'{stem}.{session["extension"]}'
    written = sorted(p.name for p in (game / 'demos').glob('*')) if (game / 'demos').exists() else []
    record = dict(command=command, exitCode=code, forcedTermination=forced, elapsedSeconds=elapsed,
                  screenshots=collect(home, output, 'record-*.png'),
                  engineErrors=ERRORS.findall(text)[:10], writtenFiles=written)
    # Exactly one file, named with the extension the profile owns.
    record['passed'] = (code == 0 and not forced and written == [demo.name]
                        and not record['engineErrors'])
    result = dict(scenario='self', profile=args.profile, start=session['start'], record=record)
    if demo.exists():
        kept = output / demo.name
        shutil.copy2(demo, kept)
        result['demo'] = dict(file=kept.name, **envelope(kept))
        for key, timedemo in (('playback', False), ('timedemoRun', True)):
            result[key] = fntech3_playback(args.client, args.installation, output, args.profile,
                                           kept, timedemo, session['game'])
    result['passed'] = bool(record['passed'] and result.get('demo', {}).get('terminated')
                            and result.get('playback', {}).get('passed')
                            and result.get('timedemoRun', {}).get('passed'))
    return result


def mp_retail_in(args, output):
    retail = args.installation / 'WolfMP.exe'
    server_home, client_home = output / 'server-home', output / 'retail-home'
    server_home.mkdir(); client_home.mkdir()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    server_command = [str(args.server)]
    for name, value in dict(com_gameProfile='rtcw-mp', fs_basepath=str(args.installation),
                            fs_homepath=str(server_home), dedicated='1', developer='1', net_ip='127.0.0.1',
                            net_port=str(port), sv_pure='1' if args.pure else '0',
                            sv_punkbuster='0', bot_enable='0', g_gametype='5', logfile='2',
                            sv_master1='', sv_master2='', sv_master3='', sv_master4='',
                            sv_master5='').items():
        server_command += ['+set', name, value]
    server_command += ['+map', args.map, '+wait', '100', '+echo', 'RTCW_SERVER_READY',
                       '+wait', '2400', '+quit']
    stem = 'retail-mp-probe'
    client_command = retail_command(retail, args.installation, client_home, dict(
        com_maxfps='30', name='fntech3-demo-probe',
        activeAction=f'wait 150; record {stem}; wait 450; stoprecord; wait 10; quit;'),
        ['+connect', f'127.0.0.1:{port}'])
    server_log = server_home / 'main' / 'qconsole.log'
    host = WindowsPrivateProcess(server_command, args.server.parent, output / 'server-stdout.log')
    begin = time.monotonic()
    result = dict(scenario='rtcw-mp-retail-in', profile='rtcw-mp', map=args.map, pure=args.pure,
                  serverCommand=server_command, clientCommand=client_command,
                  retailSha256=sha(retail))
    try:
        while time.monotonic() - begin < 30 and host.poll() is None:
            if server_log.exists() and 'RTCW_SERVER_READY' in server_log.read_text(errors='replace'):
                break
            time.sleep(0.1)
        code, forced, elapsed, text = run(client_command, retail.parent, output / 'retail-stdout.log',
                                          client_home / 'main' / 'rtcwconsole.log', 90)
        result.update(retailExit=code, retailForced=forced, retailElapsedSeconds=elapsed)
        (output / 'retail-console.log').write_text(text, encoding='utf-8')
    finally:
        host.close()
    server_text = server_log.read_text(errors='replace') if server_log.exists() else ''
    (output / 'server-console.log').write_text(server_text, encoding='utf-8')
    demo = client_home / 'main' / 'demos' / f'{stem}.dm_60'
    result['serverActive'] = ('Going from CS_PRIMED to CS_ACTIVE for fntech3-demo-probe' in server_text
                              and 'ClientBegin: 0' in server_text)
    if args.pure:
        result['pureAdmission'] = result['serverActive'] and 'Unpure client detected' not in server_text
    if demo.exists():
        kept = output / demo.name
        shutil.copy2(demo, kept)
        result['demo'] = dict(file=kept.name, **envelope(kept))
        result['playback'] = fntech3_playback(args.client, args.installation, output, 'rtcw-mp', kept, False)
        result['timedemoRun'] = fntech3_playback(args.client, args.installation, output, 'rtcw-mp', kept, True)
    result['passed'] = bool(result.get('retailExit') == 0 and not result.get('retailForced')
                            and result['serverActive'] and result.get('pureAdmission', True) and result.get('demo', {}).get('terminated')
                            and result.get('playback', {}).get('passed')
                            and result.get('timedemoRun', {}).get('passed'))
    return result


def mp_retail_out(args, output):
    retail = args.installation / 'WolfMP.exe'
    demo = args.demo.resolve(strict=True)
    if demo.suffix.lower() != '.dm_60':
        raise SystemExit('--demo must be an RTCW MP .dm_60 file')
    home = output / 'retail-home'
    (home / 'main' / 'demos').mkdir(parents=True)
    shutil.copy2(demo, home / 'main' / 'demos' / demo.name)
    command = retail_command(retail, args.installation, home, dict(
        com_maxfps='30', nextdemo='quit',
        activeAction='wait 30; screenshot; wait 90; screenshot;'), ['+demo', demo.stem])
    code, forced, elapsed, text = run(command, retail.parent, output / 'retail-stdout.log',
                                      home / 'main' / 'rtcwconsole.log', 120)
    (output / 'retail-console.log').write_text(text, encoding='utf-8')
    shots = collect(home, output, 'shot*.*')
    result = dict(scenario='rtcw-mp-retail-out', profile='rtcw-mp', command=command,
                  retailSha256=sha(retail), demo=dict(file=demo.name, **envelope(demo)),
                  exitCode=code, forcedTermination=forced, elapsedSeconds=elapsed,
                  screenshots=shots, engineErrors=ERRORS.findall(text)[:10])
    result['passed'] = code == 0 and not forced and len(shots) == 2 and not result['engineErrors']
    return result


def sp_refusal(client, installation, output, demo_file):
    """Original SP cannot replay SP demos; fnTech3 must refuse and stay usable."""
    home = output / 'playback-home'
    game = home / 'main'
    (game / 'demos').mkdir(parents=True)
    shutil.copy2(demo_file, game / 'demos' / demo_file.name)
    marker = 'RTCW_SP_REFUSAL_DONE'
    write_config(game, 'probe.cfg', [
        'set com_introplayed 1', 'set com_maxfps 60', 'set cl_allowDownload 0', 'set cl_motd 0',
        f'demo {demo_file.stem}', 'wait 30', f'echo {marker}', 'quit'])
    command = fntech3_command(client, installation, home, 'rtcw-sp', ['+exec', 'probe.cfg'])
    code, forced, elapsed, text = run(command, client.parent, output / 'playback-stdout.log',
                                      game / 'qconsole.log', 60)
    (output / 'playback-console.log').write_text(text, encoding='utf-8')
    row = dict(command=command, exitCode=code, forcedTermination=forced, elapsedSeconds=elapsed,
               refused='RTCW single-player demos cannot be replayed' in text,
               foundFile=f'Demo file: demos/{demo_file.name}' in text or
                         f'demos/{demo_file.name}' in text,
               mapLoaded='LOADING... maps/' in text, marker=marker in text,
               engineErrors=ERRORS.findall(text)[:10])
    row['passed'] = (code == 0 and not forced and row['refused'] and not row['mapLoaded']
                     and row['marker'] and not row['engineErrors'])
    return row


def sp_self(args, output):
    save = args.save.resolve(strict=True)
    home = output / 'record-home'
    game = home / 'main'
    (game / 'save').mkdir(parents=True)
    shutil.copy2(save, game / 'save' / 'probe.svg')
    stem = 'fntech3-sp-probe'
    # Original SP refuses to record without synchronous clients: try once
    # without it, then set it and record for real in the same session.
    write_config(game, 'autoexec.cfg', [
        'set com_introplayed 1', 'set com_maxfps 60', 'set com_maxfpsUnfocused 60',
        'set com_maxfpsMinimized 60', 'set cl_allowDownload 0', 'set cl_motd 0',
        'set g_synchronousClients 0',
        f'set activeAction "wait 600; record {stem}-refused; wait 5; set g_synchronousClients 1; '
        f'record {stem}; wait 360; screenshotPNG record-a; wait 120; stoprecord; wait 10; quit"'])
    command = fntech3_command(args.client, args.installation, home, 'rtcw-sp', ['+loadgame', 'probe'])
    code, forced, elapsed, text = run(command, args.client.parent, output / 'record-stdout.log',
                                      game / 'qconsole.log', 150)
    (output / 'record-console.log').write_text(text, encoding='utf-8')
    demo = game / 'demos' / f'{stem}.dm_49'
    written = sorted(p.name for p in (game / 'demos').glob('*')) if (game / 'demos').exists() else []
    record = dict(command=command, save=dict(file=save.name, sha256=sha(save)), exitCode=code,
                  forcedTermination=forced, elapsedSeconds=elapsed,
                  screenshots=collect(home, output, 'record-*.png'),
                  engineErrors=ERRORS.findall(text)[:10], writtenFiles=written,
                  unsynchronizedRefused="The server must have 'g_synchronousClients 1' set for demos" in text
                                        and f'{stem}-refused.dm_49' not in written)
    record['passed'] = (code == 0 and not forced and demo.exists() and not record['engineErrors']
                        and record['unsynchronizedRefused'])
    result = dict(scenario='rtcw-sp', profile='rtcw-sp', record=record)
    if demo.exists():
        kept = output / demo.name
        shutil.copy2(demo, kept)
        result['demo'] = dict(file=kept.name, **envelope(kept))
        result['playbackRefusal'] = sp_refusal(args.client, args.installation, output, kept)
    result['passed'] = bool(record['passed'] and result.get('demo', {}).get('terminated')
                            and result.get('playbackRefusal', {}).get('passed'))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--scenario', required=True,
                        choices=('self', 'rtcw-mp-retail-in', 'rtcw-mp-retail-out', 'rtcw-sp'))
    parser.add_argument('--profile', choices=tuple(SESSIONS), default='rtcw-mp',
                        help='self: the profile whose local session is recorded')
    parser.add_argument('--client', type=Path, help='fnTech3 client executable')
    parser.add_argument('--server', type=Path, help='fnTech3 dedicated executable (mp-retail-in)')
    parser.add_argument('--installation', type=Path, required=True,
                        help='legitimate installation root for the selected game')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--map', default='mp_beach', help='rtcw-mp-retail-in: server map')
    parser.add_argument('--pure', action='store_true', help='rtcw-mp-retail-in: run the server with sv_pure 1')
    parser.add_argument('--demo', type=Path, help='rtcw-mp-retail-out: fnTech3-written .dm_60 to play')
    parser.add_argument('--save', type=Path, help='rtcw-sp: SP .svg to load before recording')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    if args.scenario != 'rtcw-mp-retail-out' and not args.client:
        parser.error('--client is required for this scenario')
    if args.scenario == 'rtcw-mp-retail-in' and not args.server:
        parser.error('--server is required for rtcw-mp-retail-in')
    if args.scenario == 'rtcw-mp-retail-out' and not args.demo:
        parser.error('--demo is required for rtcw-mp-retail-out')
    if args.scenario == 'rtcw-sp' and not args.save:
        parser.error('--save is required for rtcw-sp')
    args.installation = args.installation.resolve(strict=True)
    for name in ('client', 'server'):
        if getattr(args, name):
            setattr(args, name, getattr(args, name).resolve(strict=True))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    before = inventory(args.installation)
    scenario = dict(zip(('self', 'rtcw-mp-retail-in', 'rtcw-mp-retail-out', 'rtcw-sp'),
                        (self_session, mp_retail_in, mp_retail_out, sp_self)))[args.scenario]
    result = scenario(args, output)
    result['installationUnchanged'] = before == inventory(args.installation)
    result['passed'] = bool(result['passed'] and result['installationUnchanged'])
    for name in ('client', 'server'):
        if getattr(args, name):
            result[f'{name}Sha256'] = sha(getattr(args, name))
    result['sourceCommit'] = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                            capture_output=True).stdout.strip()
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if 'ommand' not in k}, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
