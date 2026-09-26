#!/usr/bin/env python3
"""Fire into mp_beach's surf and capture the water ripples, in fnTech3 and retail.

RTCW MP's cgame makes a ripple entity, reference entity type 3, when a bullet
lands under water: CG_Bullet traces back to the water's surface and
CG_MissileHitWall spawns CG_WaterRipple there. mp_beach's one water brush
(textures/liquids/water_beach_nerve) has its surface at z 64. The Allies
spawn aboard landing craft, whose decks take the shots, so the probe stands
its soldier in the open surf between the two groups of craft, where the feet
are a few units under the surface, and fires at the sand ahead.

Every action is a console command from a config the probe writes into the
isolated home: join the Allies as a soldier with a Thompson, close the limbo
menu, setviewpos (devmap enables cheats), +lookdown, +attack. No keyboard or
mouse input is injected, the window is on a private desktop, and each engine
captures with its own screenshot command. The installation is read-only; its
file inventory is compared before and after. Retail splits its command line at
every '+', even inside quotes, so the actions cannot travel on the command
line.
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

WIDTH, HEIGHT = 640, 480
# Frames at com_maxfps 60. A soldier placed at (-1700, 2600) facing the sea
# stands on the last of the sand with the eye at z 141. Seven frames of
# +lookdown at the default 140 degrees a second tilt the view about 16 degrees,
# so rounds strike the seabed some 280 units out, past the waterline even with
# the Thompson's spread, and the ripples lie flat in view instead of against
# the lens. A bullet's ripple lives for a second. Retail WolfMP.exe paces its
# frames faster than 60 a second on a private desktop despite com_maxfps, so
# the same frame counts aim it higher and fire fewer rounds; the burst is long
# enough that it still fires several. fixedtime would equalize the two, but
# retail restarts its game soon after fixedtime is set and then stalls.
SCRIPT = ('wait 60; team b 0 14 0 17 0; wait 90; CloseLimboMenu; wait 30; '
          'setviewpos -1700 2600 100 180; wait 90; +lookdown; wait 7; -lookdown; wait 30; viewpos; '
          '+attack; wait 30; {shot1}; wait 20; {shot2}; -attack; wait 20; {shot3}; wait 5; quit;')
ERRORS = re.compile(r'^.*(?:ERROR:|Bad reType|Server crashed|Sys_Error|fatal crashed|recursive error).*$',
                    re.M | re.I)


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


def run(command, cwd, stdout, timeout=120):
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
            # A hung retail client can outlast close()'s five-second grace
            # after it is terminated; wait for it before closing again.
            process.wait(60)
            process.close()
    return code, forced, round(time.monotonic() - begin, 3)


def fntech3_row(executable, installation, output, renderer, label):
    row_dir = output / f'fntech3-{label}-{renderer}'
    home = row_dir / 'home'
    game = home / 'main'
    game.mkdir(parents=True)
    shots = [f'fntech3-{label}-{renderer}-{index}' for index in (1, 2, 3)]
    action = SCRIPT.format(**{f'shot{index}': f'screenshotPNG {name}' for index, name in enumerate(shots, 1)})
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  cl_motd='0', com_introplayed='1', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
                  activeAction=action)
    (game / 'ripple.cfg').write_text(''.join(f'set {key} "{value}"\n' for key, value in values.items()),
                                     encoding='utf-8')
    startup = dict(com_gameProfile='rtcw-mp', fs_basepath=str(installation), fs_homepath=str(home),
                   fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth=str(WIDTH),
                   r_customHeight=str(HEIGHT), in_mouse='0', in_joystick='0', in_nograb='1',
                   s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                   cl_renderer=renderer, logfile='2', net_ip='127.0.0.1', net_port='0', net_ip6='::1',
                   net_port6='0', rtx_rt_mode='0', rtx_rt_require='0', com_maxfps='60',
                   com_recommendedSet='1')
    command = [str(executable)]
    for key, value in startup.items():
        command += ['+set', key, value]
    command += ['+exec', 'ripple.cfg', '+devmap', 'mp_beach']
    code, forced, elapsed = run(command, executable.parent, row_dir / 'stdout.log')
    log = game / 'qconsole.log'
    captures = [next(home.rglob(name + '.png'), None) for name in shots]
    return finish(row_dir, home, output, dict(engine='fntech3', label=label, renderer=renderer,
        executable=str(executable), executableSha256=sha(executable),
        rendererSha256=sha(executable.parent / f'fnql_{renderer}_x86.dll'), command=command,
        exitCode=code, forcedStop=forced, elapsedSeconds=elapsed), log, captures)


def retail_row(installation, output):
    retail = installation / 'WolfMP.exe'
    row_dir = output / 'retail'
    home = row_dir / 'home'
    game = home / 'main'
    game.mkdir(parents=True)
    action = SCRIPT.format(shot1='screenshot', shot2='screenshot', shot3='screenshot')
    values = dict(cl_allowDownload='0', sv_punkbuster='0', activeAction=action)
    (game / 'ripple.cfg').write_text(''.join(f'set {key} "{value}"\n' for key, value in values.items()),
                                     encoding='utf-8')
    # Retail latches com_maxfps, and without com_recommendedSet its first run
    # executes a hardware-detected config after ours; both go on the command
    # line so the frame counts in SCRIPT take the same time in both engines.
    # cl_autoupdate and cl_motd stop the main menu checking an external server
    # for an update and asking one for the MOTD.
    startup = dict(fs_basepath=str(installation), fs_homepath=str(home), r_fullscreen='0', r_mode='3',
                   in_mouse='0', in_joystick='0', in_nograb='1', logfile='2', com_viewlog='0',
                   s_initsound='0', cl_punkbuster='0', r_ignorehwgamma='1', com_maxfps='60',
                   com_recommendedSet='1', cl_autoupdate='0', cl_motd='0')
    command = [str(retail)]
    for key, value in startup.items():
        command += ['+set', key, value]
    command += ['+exec', 'ripple.cfg', '+devmap', 'mp_beach']
    code, forced, elapsed = run(command, retail.parent, row_dir / 'stdout.log')
    log = game / 'rtcwconsole.log'
    captures = sorted(home.rglob('shot*.*'))
    return finish(row_dir, home, output, dict(engine='retail', executable=str(retail),
        executableSha256=sha(retail), command=command, exitCode=code, forcedStop=forced,
        elapsedSeconds=elapsed), log, captures)


def finish(row_dir, home, output, row, log, captures):
    text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
    (row_dir / 'console.log').write_text(text, encoding='utf-8')
    row['errors'] = ERRORS.findall(text)
    row['viewpos'] = re.findall(r'^\(\s*-?\d+\s+-?\d+\s+-?\d+\s*\)\s*:\s*-?\d+.*$', text, re.M)
    row['joinedAllies'] = bool(re.search(r'joined the (?:Allies|Allied)', text, re.I))
    row['screenshots'] = []
    for capture in captures:
        if capture is None:
            continue
        destination = row_dir / capture.name
        shutil.copy2(capture, destination)
        entry = dict(path=str(destination), sha256=sha(destination))
        if destination.suffix.lower() == '.tga':
            try:
                from PIL import Image
                converted = destination.with_suffix('.png')
                Image.open(destination).save(converted)
                entry['png'] = str(converted)
            except Exception as error:  # conversion is a viewing aid only
                entry['pngError'] = str(error)
        row['screenshots'].append(entry)
    remove_home(home, output)
    row['homeRemoved'] = True
    row['passed'] = (row['exitCode'] == 0 and not row['forcedStop'] and not row['errors']
                     and len(row['screenshots']) == 3)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--executable', type=Path, help='fnTech3 client built with -Drtcw-development=true')
    parser.add_argument('--label', default='build', help='name for this fnTech3 build in the report')
    parser.add_argument('--renderer', action='append', choices=('glx', 'vk', 'rtx'))
    parser.add_argument('--retail', action='store_true', help="also run the installation's WolfMP.exe")
    args = parser.parse_args()
    installation = args.installation.resolve(strict=True)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    if not args.executable and not args.retail:
        parser.error('pass --executable, --retail or both')
    before = inventory(installation)
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  worktreeDirty=bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
                  map='mp_beach', script=SCRIPT, runs=[])
    if args.executable:
        executable = args.executable.resolve(strict=True)
        executable.relative_to(ROOT)
        for renderer in args.renderer or ['glx']:
            report['runs'].append(fntech3_row(executable, installation, output, renderer, args.label))
    if args.retail:
        report['runs'].append(retail_row(installation, output))
    report['installationUnchanged'] = before == inventory(installation)
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for row in report['runs']:
        print(row['engine'], row.get('label', ''), row.get('renderer', ''), 'exit', row['exitCode'],
              'forced', row['forcedStop'], 'shots', len(row['screenshots']), 'errors', row['errors'],
              'viewpos', row['viewpos'][-1:], flush=True)
    print('installation unchanged', report['installationUnchanged'], 'output', output)
    return 0 if report['installationUnchanged'] and all(row['passed'] for row in report['runs']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
