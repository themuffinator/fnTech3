#!/usr/bin/env python3
"""Capture each profile's notify lines and chat line with the engine's own
screenshot command.

Runs fnTech3, or a retail engine for comparison, windowed on an unswitched
private desktop with an isolated home, mouse, joystick and sound off, and the
installation read-only and inventoried before and after. Every action is a
console command from a config the probe writes into the isolated home: once
the map is active, echo three coloured lines, capture them, open the chat line
with messagemode, capture it, switch to the team chat line with messagemode2,
capture that, and quit. No input is sent. fnTech3 captures with screenshotPNG,
retail with screenshot. Retail command lines split at every '+', so the
actions stay in the config. Reports hold hashes, never copied assets.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

# profile: game directory, map arguments, what to do before the capture, and
# the say and team chat commands
PROFILES = {
    'q3': ('baseq3', ['+devmap', 'q3dm17'], '', ('messagemode', 'messagemode2')),
    'ql': ('baseq3', ['+devmap', 'campgrounds', 'ffa'], '', ('messagemode', 'messagemode2')),
    # devmap, not spdevmap: an FFA load skips the mission briefing, which
    # only a click dismisses and which hides the overlay as a menu does.
    'rtcw-sp': ('main', ['+devmap', 'escape1'], '', ('messagemode', 'messagemode2')),
    # The limbo menu hides everything in WolfMP.exe, so it is captured once
    # with its lines, then closed.
    'rtcw-mp': ('main', ['+devmap', 'mp_beach'],
                'echo ^3limbo line; wait 20; {shot} {label}-limbo; wait 10; CloseLimboMenu; wait 60; ',
                ('messagemode', 'messagemode2')),
    # et.exe draws the engine's overlay only with the cheat con_drawnotify,
    # settable once devmap has turned cheats on. openlimbomenu closes the
    # limbo panel. The engine's chat commands are ET's own; the third is the
    # fireteam's.
    'wolfet': ('etmain', ['+devmap', 'oasis'], 'openlimbomenu; wait 30; set con_drawnotify 1; ',
               ('clMessageMode', 'clMessageMode3')),
    'stvef-sp': ('baseef', ['+devmap', 'borg1'], '', ('messagemode', 'messagemode2')),
    'stvef-mp': ('baseef', ['+devmap', 'hm_voy1'], '', ('messagemode', 'messagemode2')),
}
# Retail engines that write only under fs_homepath. Elite Force's ignore it and
# write their config and logs into the installation, so they are not run here;
# docs/fntech3/2026-09-18-stvef-holomatch-sessions.md runs them from a copy.
RETAIL = {'q3': 'quake3.exe', 'rtcw-sp': 'WolfSP.exe', 'rtcw-mp': 'WolfMP.exe'}
# A capture is taken at the end of the next frame, so each one waits a few
# frames before the command that follows can change the screen. The field's
# cursor blinks every 256 ms, so the say line is captured twice, 16 frames
# apart, to catch it in one of them.
ACTION = ('wait 90; {pre}echo ^3notify one; echo ^1notify two; echo notify three; wait 30; '
          '{shot} {label}-lines; wait 5; {say}; wait 30; {shot} {label}-say; wait 16; '
          '{shot} {label}-say2; wait 5; {say}; {team}; wait 30; {shot} {label}-team; wait 5; '
          '{team}; wait 5; quit')
ERRORS = re.compile(r'^.*(?:ERROR:|Sys_Error|fatal crashed|recursive error|Server crashed).*$', re.M | re.I)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): (path.stat().st_size, path.stat().st_mtime_ns)
            for path in installation.rglob('*') if path.is_file()}


def run(command, cwd, stdout, timeout):
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, cwd, stdout)
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            time.sleep(0.1)
        forced = process.poll() is None
        code = process.poll()
    finally:
        try:
            process.close()
        except subprocess.TimeoutExpired:
            process.wait(60)
            process.close()
    return code, forced, round(time.monotonic() - begin, 3)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--executable', type=Path, help='fnTech3 client; omit to run the retail engine')
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=960)
    parser.add_argument('--timeout', type=int, default=180)
    args = parser.parse_args()
    game, map_args, pre, (say, team) = PROFILES[args.profile]
    installation = args.installation.resolve(strict=True)
    retail = args.executable is None
    if retail:
        if args.profile not in RETAIL or (args.width, args.height) != (640, 480):
            parser.error('retail runs need a retail profile and 640x480')
        executable = (installation / RETAIL[args.profile]).resolve(strict=True)
    else:
        executable = args.executable.resolve(strict=True)
        executable.relative_to(ROOT)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    (home / game).mkdir(parents=True)
    before = inventory(installation)

    label = f'notify-{args.profile}'
    shot = 'screenshot' if retail else 'screenshotPNG'
    action = ACTION.format(pre=pre.format(shot=shot, label=label), shot=shot, label=label, say=say, team=team)
    values = dict(cl_allowDownload='0', com_introplayed='1', activeAction=action)
    if not retail:
        values.update(sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_motd='0', com_maxfps='60',
                      com_maxfpsUnfocused='60', com_maxfpsMinimized='60')
    config = ''.join(f'set {name} "{value}"\n' for name, value in values.items())
    (home / game / 'notify-probe.cfg').write_text(config, encoding='utf-8')
    (output / 'notify-probe.cfg').write_text(config, encoding='utf-8')
    if retail:
        startup = dict(fs_basepath=str(installation), fs_homepath=str(home), r_fullscreen='0', r_mode='3',
                       in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0', logfile='2',
                       com_maxfps='60', cl_punkbuster='0', sv_punkbuster='0', r_ignorehwgamma='1',
                       com_recommendedSet='1')
    else:
        startup = dict(com_gameProfile=args.profile, fs_basepath=str(installation), fs_homepath=str(home),
                       fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth=str(args.width),
                       r_customHeight=str(args.height), in_mouse='0', in_joystick='0', in_nograb='1',
                       s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                       cl_renderer='glx', logfile='2', net_ip='127.0.0.1', net_port='0', net_ip6='::1',
                       net_port6='0', com_recommendedSet='1')
        if args.profile == 'wolfet':
            # ET's maps need a larger hunk; see scripts/probe_wolfet_client.py.
            startup.update(com_hunkMegs='256', sv_pure='0')
    command = [str(executable)]
    for name, value in startup.items():
        command += ['+set', name, value]
    command += ['+exec', 'notify-probe.cfg'] + map_args
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  profile=args.profile, engine='retail' if retail else 'fntech3',
                  executable=str(executable), executableSha256=sha(executable),
                  installation=str(installation), width=args.width, height=args.height,
                  command=command, action=action, screenshots=[])
    renderer = executable.parent / 'fnql_glx_x86.dll'
    if not retail and renderer.is_file():
        report['rendererSha256'] = sha(renderer)
    code, forced, elapsed = run(command, executable.parent, output / 'stdout.log', args.timeout)
    report.update(exitCode=code, forcedStop=forced, elapsedSeconds=elapsed)
    logs = [path for path in home.rglob('*.log') if path.is_file()]
    text = '\n'.join(path.read_text(encoding='utf-8', errors='replace') for path in logs)
    (output / 'console.log').write_text(text, encoding='utf-8')
    captures = sorted(path for path in home.rglob('*') if path.suffix.lower() in ('.png', '.tga', '.jpg')
                      and 'screenshots' in path.parts)
    for capture in captures:
        destination = output / capture.name
        shutil.copy2(capture, destination)
        entry = dict(name=capture.name, sha256=sha(destination))
        if destination.suffix.lower() == '.tga':
            try:
                from PIL import Image
                converted = destination.with_suffix('.png')
                Image.open(destination).save(converted)
                entry['png'] = converted.name
            except Exception as error:  # conversion is a viewing aid only
                entry['pngError'] = str(error)
        report['screenshots'].append(entry)
    report['errors'] = ERRORS.findall(text)
    report['notifyTime'] = re.findall(r'con_notifytime[^\n]*', text)[:3]
    shutil.rmtree(home)
    report['installationUnchanged'] = before == inventory(installation)
    report['passed'] = (code == 0 and not forced and not report['errors'] and report['installationUnchanged']
                        and len(report['screenshots']) >= 3)
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(args.profile, report['engine'], 'PASS' if report['passed'] else 'FAIL', 'exit', code, 'forced', forced,
          'shots', len(report['screenshots']), 'errors', report['errors'][:3], flush=True)
    if not report['installationUnchanged']:
        raise RuntimeError('Installation file metadata changed during the probe')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
