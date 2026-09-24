#!/usr/bin/env python3
"""Throw one charge and let it settle in RTCW MP or ET, on a listen or dedicated server.

When dynamite stops moving, the RTCW MP and ET game modules make the world,
ENTITYNUM_WORLD (1022), its owner, and the charge's next trace passes that
owner as the pass entity. The server then reads the world's own record, past
the entity count the module publishes. fnTech3 refused that read and dropped
the server with "SV_ClipMoveToEntities: invalid pass entity 1022".

Every action is a console command from a config the probe writes into an
isolated home: join the Allies as an engineer, close the limbo menu, select
the dynamite bank, look down, press and release +attack once, then wait long
enough for the charge to land and settle. In MP gametypes an unarmed charge
never explodes. No keyboard or mouse input is injected, every window is on a
private desktop, and the client captures with its own screenshotPNG command,
half a second after the throw and again once the charge has settled. The RTCW
and ET entity readers do not print cl_shownet deltas, so the captures, and a
paired run that is refused at the settled charge, are what show the throw.
A dedicated run starts the server on 127.0.0.1 with the same devmap and
connects the client to it. The installation is read-only; its file inventory
is compared before and after.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import re
import shutil
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

# The engineer is class 2 in both titles. RTCW MP's ClientSpawn takes the class
# from the usercmd's mpSetup bits, which its cgame builds from mp_playerType,
# so that cvar says engineer as well as the team command. ET's team command
# sets the class itself: class 2 with a Thompson (8) and a Colt (7). Both keep
# dynamite first in weapon bank 6 (weapBanksMultiPlayer). The RTCW placement is
# the ripple probe's: the last of mp_beach's sand, facing the sea. setviewpos
# needs devmap's cheats.
PROFILES = {
    'rtcw-mp': dict(game='main', map='mp_beach', hunk=0, cvars=dict(mp_playerType='2'),
                    join='team b 2 14 0 17 0', close='CloseLimboMenu',
                    place='setviewpos -1700 2600 100 180; wait 90; '),
    'wolfet': dict(game='etmain', map='oasis', hunk=256, cvars={},
                   join='team b 2 8 7', close='openlimbomenu', place=''),
}
SCRIPT = ('wait 60; {join}; wait 120; {close}; wait 30; {place}weaponbank 6; wait 60; '
          '+lookdown; wait 20; -lookdown; wait 30; +attack; wait 30; -attack; wait 30; '
          'screenshotPNG {shot}-thrown; wait 270; screenshotPNG {shot}-settled; wait 5; quit;')
ERRORS = re.compile(r'^.*(?:ERROR:|invalid pass entity|Server crashed|Sys_Error|fatal crashed|'
                    r'recursive error).*$', re.M | re.I)
WORLD_PASS = re.compile(r'invalid pass entity 1022')
READY = re.compile(r'^Server: \S+\s*$.*?^-{35}\s*$', re.M | re.S)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): (path.stat().st_size, path.stat().st_mtime_ns)
            for path in installation.rglob('*') if path.is_file()}


def remove_home(home, output):
    target = home.resolve(strict=True)
    target.relative_to(output.resolve(strict=True))
    for path in [target, *target.rglob('*')]:
        info = path.lstat()
        if path.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise RuntimeError(f'Refusing cleanup through a link: {path}')
    shutil.rmtree(target)


def read(path):
    return path.read_text(encoding='utf-8', errors='replace') if path.is_file() else ''


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


def close(process):
    try:
        process.close()
    except subprocess.TimeoutExpired:
        process.wait(60)
        process.close()


def command_line(executable, startup, tail):
    command = [str(executable)]
    for key, value in startup.items():
        command += ['+set', key, value]
    command += tail
    if sum(word.startswith('+') for word in command) > 32:
        raise RuntimeError('Startup command count exceeds engine capacity')
    return command


def run_probe(args, installation, output):
    profile = PROFILES[args.profile]
    client_home = output / 'client-home'
    client_game = client_home / profile['game']
    client_game.mkdir(parents=True)
    shot = f'explosive-{args.profile}-{args.mode}'
    action = SCRIPT.format(shot=shot, **profile)
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  cl_motd='0', com_introplayed='1', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
                  sv_pure='0', activeAction=action, **profile['cvars'])
    for number in range(1, 6):
        values[f'sv_master{number}'] = ''
    config = ''.join(f'set {key} "{value}"\n' for key, value in values.items())
    (client_game / 'explosive.cfg').write_text(config, encoding='utf-8')
    (output / 'client.cfg').write_text(config, encoding='utf-8')
    startup = dict(com_gameProfile=args.profile, fs_basepath=str(installation), fs_homepath=str(client_home),
                   fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth='640',
                   r_customHeight='480', in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0',
                   com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0', cl_renderer='glx',
                   logfile='2', net_ip='127.0.0.1', net_port='0', net_ip6='::1', net_port6='0',
                   com_maxfps='60', com_recommendedSet='1')
    if profile['hunk']:
        startup['com_hunkMegs'] = str(profile['hunk'])
    report = dict(profile=args.profile, mode=args.mode, map=profile['map'], script=action,
                  client=str(args.client), clientSha256=sha(args.client))
    server = None
    server_home = output / 'server-home'
    try:
        if args.mode == 'dedicated':
            server_game = server_home / profile['game']
            server_game.mkdir(parents=True)
            port = free_port()
            server_values = dict(sv_pure='0', sv_maxclients='4', sv_allowDownload='0', dedicated='1',
                                 sv_autoRecordDemos='0', developer='0')
            for number in range(1, 6):
                server_values[f'sv_master{number}'] = ''
            server_config = ''.join(f'set {key} "{value}"\n' for key, value in server_values.items())
            (server_game / 'server.cfg').write_text(server_config, encoding='utf-8')
            (output / 'server.cfg').write_text(server_config, encoding='utf-8')
            server_startup = dict(com_gameProfile=args.profile, fs_basepath=str(installation),
                                  fs_homepath=str(server_home), fs_game='', fs_cdpath='', dedicated='1',
                                  net_ip='127.0.0.1', net_port=str(port), net_ip6='::1', net_port6='0',
                                  com_steamIntegration='0', fs_skipWorkshop='1', logfile='2')
            if profile['hunk']:
                server_startup['com_hunkMegs'] = str(profile['hunk'])
            server_command = command_line(args.server, server_startup,
                                          ['+exec', 'server.cfg', '+devmap', profile['map']])
            report.update(server=str(args.server), serverSha256=sha(args.server), serverCommand=server_command,
                          port=port)
            server = WindowsPrivateProcess(server_command, args.server.parent, output / 'server-stdout.log')
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline and server.poll() is None:
                if READY.search(read(server_game / 'qconsole.log')):
                    break
                time.sleep(0.2)
            else:
                raise RuntimeError('Dedicated server did not finish loading the map')
            client_command = command_line(args.client, startup,
                                          ['+exec', 'explosive.cfg', '+connect', f'127.0.0.1:{port}'])
        else:
            client_command = command_line(args.client, startup,
                                          ['+exec', 'explosive.cfg', '+devmap', profile['map']])
        report['clientCommand'] = client_command
        begin = time.monotonic()
        client = WindowsPrivateProcess(client_command, args.client.parent, output / 'client-stdout.log')
        try:
            while client.poll() is None and time.monotonic() - begin < args.timeout:
                time.sleep(0.1)
            report['forcedStop'] = client.poll() is None
            report['exitCode'] = client.poll()
        finally:
            close(client)
        report['elapsedSeconds'] = round(time.monotonic() - begin, 3)
        if server:
            report['serverRunningAfterClient'] = server.poll() is None
    finally:
        if server:
            close(server)
    client_log = read(client_game / 'qconsole.log')
    server_log = read(server_home / profile['game'] / 'qconsole.log') if args.mode == 'dedicated' else ''
    (output / 'client-console.log').write_text(client_log, encoding='utf-8')
    if args.mode == 'dedicated':
        (output / 'server-console.log').write_text(server_log, encoding='utf-8')
    report['clientErrors'] = ERRORS.findall(client_log)
    report['serverErrors'] = ERRORS.findall(server_log)
    report['worldPassRefused'] = bool(WORLD_PASS.search(client_log + server_log))
    report['joinedAllies'] = bool(re.search(r'joined the (?:Allies|Allied)', client_log + server_log, re.I))
    report['screenshots'] = []
    for capture in sorted(client_home.rglob(shot + '-*.png')):
        destination = output / capture.name
        shutil.copy2(capture, destination)
        report['screenshots'].append(dict(path=str(destination), sha256=sha(destination)))
    for home in (client_home, server_home):
        if home.exists():
            remove_home(home, output)
    report['homesRemoved'] = True
    if args.expect == 'refusal':
        report['passed'] = report['worldPassRefused']
    else:
        report['passed'] = (not report['worldPassRefused'] and not report['clientErrors']
                            and not report['serverErrors'] and report.get('exitCode') == 0
                            and not report.get('forcedStop') and len(report['screenshots']) == 2
                            and report.get('serverRunningAfterClient', True))
    report['visualPassed'] = None  # The captures need a separate look for the charge.
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    parser.add_argument('--mode', choices=('listen', 'dedicated'), required=True)
    parser.add_argument('--expect', choices=('refusal', 'survival'), required=True,
                        help='refusal: the server drops at the settled charge; survival: play continues')
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--client', type=Path, required=True, help='fnTech3 client (fntech3.exe)')
    parser.add_argument('--server', type=Path, help='fnTech3 dedicated server, for --mode dedicated')
    parser.add_argument('--timeout', type=int, default=150)
    args = parser.parse_args()
    if args.mode == 'dedicated' and not args.server:
        parser.error('--mode dedicated needs --server')
    installation = args.installation.resolve(strict=True)
    args.client = args.client.resolve(strict=True)
    args.client.relative_to(ROOT)
    if args.server:
        args.server = args.server.resolve(strict=True)
        args.server.relative_to(ROOT)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    before = inventory(installation)
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  worktreeDirty=bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
                  expect=args.expect)
    try:
        report.update(run_probe(args, installation, output))
    except Exception as error:
        report['error'] = str(error)
        report['errorType'] = type(error).__name__
        report['passed'] = False
    report['installationUnchanged'] = before == inventory(installation)
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(args.profile, args.mode, 'expect', args.expect, 'PASS' if report['passed'] else 'FAIL',
          'shots', len(report.get('screenshots', [])), 'refused', report.get('worldPassRefused'),
          'errors', report.get('clientErrors', []) + report.get('serverErrors', []),
          report.get('errorType', ''), flush=True)
    print('installation unchanged', report['installationUnchanged'], 'output', output)
    return 0 if report['installationUnchanged'] and report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
