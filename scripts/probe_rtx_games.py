#!/usr/bin/env python3
"""Path-trace each game's content with the RTX renderer, then draw the same
frame with its raster renderer.

For each profile it is given, the probe loads one map with `devmap` in an
isolated home on a private desktop (windowed, no keyboard or mouse input).
Once the player is in view it freezes the game clock with `timescale 0`,
restarts the renderer, lets the path tracer (`rtx_rt_mode 2`) accumulate, and
writes the engine's own `screenshotPNG`; then it restarts the renderer in
raster mode (`rtx_rt_mode 0; vid_restart`) and captures the same frozen frame.
Both captures follow a restart because a restart reloads the client game,
which rebuilds its scene from the frozen clock: RTCW's restarts its flickering
lights dark until the clock moves, so a frame taken before a restart and one
after it show different lights. It reports
the renderer's capability gate and tracing backend, the native-dispatch
marker, Vulkan validation messages, errors, the path tracer's GPU timings and
the traced/raster brightness ratio of the view (the status bar excluded).

Every action is a console command from a config the probe writes into the
isolated home. Installations are read-only inputs; each one's file inventory
is compared before and after.

Example::

    python scripts/probe_rtx_games.py --client meson/build/win32-dev/fntech3.exe \\
        --output .tmp/rtx-games/example \\
        --game "q3=C:/Program Files (x86)/Steam/steamapps/common/Quake 3 Arena" \\
        --game "wolfet=E:/SteamLibrary/steamapps/common/Wolfenstein Enemy Territory"
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

ERRORS = re.compile(r'^.*(?:ERROR:|Sys_Error|fatal crashed|recursive error|Server crashed).*$', re.M | re.I)
VALIDATION = re.compile(r'^.*Vulkan validation.*$', re.M)
GATE = re.compile(r'RTX capability gate: requested=\w+ \((\d+)\), active=\w+ \((\d+)\), require=(\d+), backend=(\w+)')
TRACING = re.compile(r'RTX RT primary dispatch validation: .*tracing=(\w+)')
TIMING = re.compile(r'^RTX RT timing: .*$', re.M)

# How each profile reaches a view of the world, and gets it back once the
# raster restart has reloaded the cgame. RTCW MP opens its limbo menu on
# joining (the cgame's CloseLimboMenu shuts it); ET's limbo panel covers the
# screen until the cgame's openlimbomenu toggles it; Quake Live's devmap needs
# a factory; RTCW SP's devmap skips the mission briefing.
PROFILES = {
    'q3': dict(game='baseq3', map=['q3dm17'], join='wait 60; ', rejoin='', startup=dict(cl_motd='0')),
    'ql': dict(game='baseq3', map=['campgrounds', 'ffa'], join='wait 120; ', rejoin='', startup=dict()),
    'rtcw-mp': dict(game='main', map=['mp_ice'],
                    join='wait 60; team b 0 14 0 17 0; wait 90; CloseLimboMenu; wait 60; ',
                    rejoin='CloseLimboMenu; wait 30; ',
                    startup=dict(cl_autoupdate='0', cl_motd='0')),
    'rtcw-sp': dict(game='main', map=['escape1'], join='wait 120; ', rejoin='', startup=dict(cl_motd='0')),
    # ET's openlimbomenu toggles the limbo panel. After a restart it covers the
    # view only on maps that show a campaign briefing, which closing turns
    # into the panel (cg_newDraw.c CG_EventHandling): these retail ones.
    'wolfet': dict(game='etmain', map=['oasis'], join='wait 120; ', rejoin='',
                   limbo=('oasis', 'radar'),
                   startup=dict(cl_autoupdate='0', cl_motd='0', com_hunkMegs='256')),
}


CONFIGS = {'q3': 'q3config.cfg', 'ql': 'qzconfig.cfg', 'rtcw-sp': 'wolfconfig.cfg', 'rtcw-mp': 'wolfconfig_mp.cfg',
           'wolfet': 'etconfig.cfg'}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): (path.stat().st_size, path.stat().st_mtime_ns)
            for path in installation.rglob('*') if path.is_file()}


def brightness(path, rows):
    import numpy
    from PIL import Image
    image = numpy.asarray(Image.open(path).convert('RGB'), dtype=numpy.float64)
    return float(image[: int(image.shape[0] * rows)].mean())


def probe(client, profile, installation, output, args):
    spec = PROFILES[profile]
    row = output / profile
    home = row / 'home'
    game = home / spec['game']
    game.mkdir(parents=True)
    demo = args.demo.get(profile)
    # console commands run once the view is frozen, before the traced capture
    extra = ''.join(f'{command}; ' for command in args.command)
    if demo is not None:
        # a demo plays for a while, then freezes where the traced and raster
        # captures are taken; it needs no team or limbo menu
        (game / 'demos').mkdir()
        shutil.copy2(demo, game / 'demos' / demo.name)
        # cl_freezeDemo stops the demo's clock in every profile; Quake Live
        # keeps timescale at 0.1 or more, as retail does
        # (no restart before the traced capture: a restart while a Quake Live
        # demo is frozen leaves it on its loading screen)
        action = (f'wait {args.demo_advance}; cl_freezeDemo 1; timescale 0; {extra}wait {args.accumulate}; '
                  'screenshotPNG rt; '
                  f'wait 3; rtx_rt_mode 0; vid_restart; wait {args.settle}; screenshotPNG raster; wait 5; quit')
    else:
        rejoin = spec['rejoin']
        if (args.map.get(profile) or spec['map'])[0] in spec.get('limbo', ()):
            rejoin += 'openlimbomenu; wait 30; '
        action = (spec['join'] + 'timescale 0; vid_restart; wait 30; ' + rejoin +
                  f'{extra}wait {args.accumulate}; screenshotPNG rt; wait 3; '
                  'rtx_rt_mode 0; vid_restart; wait 30; ' + rejoin +
                  f'wait {args.settle}; screenshotPNG raster; wait 5; quit')
    values = dict(developer='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0',
                  com_introplayed='1', com_maxfps='60', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
                  con_notifytime='-1', cg_lagometer='0', cg_drawFPS='0', sv_pure='0',
                  rtx_rt_perf_timing='1', rtx_rt_perf_interval='100', activeAction=action)
    for number in range(1, 6):
        values[f'sv_master{number}'] = ''
    for item in args.set:
        name, _, value = item.partition('=')
        values[name] = value
    (game / 'rtxprobe.cfg').write_text(''.join(f'set {k} "{v}"\n' for k, v in values.items()), encoding='utf-8')
    # The home path is searched first: an empty configuration keeps the
    # installation's own (Steam Quake III's sets r_vertexLight 2 and
    # r_picmip 1) out of both captures.
    (game / CONFIGS[profile]).write_text('// probe_rtx_games: defaults only\n', encoding='ascii')
    startup = dict(com_gameProfile=profile, fs_basepath=str(installation), fs_homepath=str(home),
                   fs_game='', r_fullscreen='0', r_mode='-1', r_customWidth=str(args.width),
                   r_customHeight=str(args.height), in_mouse='0', in_joystick='0', in_nograb='1',
                   s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                   cl_renderer='rtx', logfile='2', net_ip='127.0.0.1', net_port='0', net_ip6='::1',
                   net_port6='0', rtx_rt_mode='2', rtx_rt_require='1', rtx_debug_vk_validation='1',
                   com_recommendedSet='1', **spec['startup'])
    for item in args.startup:
        name, _, value = item.partition('=')
        startup[name] = value
    command = [str(client)]
    for name, value in startup.items():
        command += ['+set', name, value]
    if demo is not None:
        command += ['+exec', 'rtxprobe.cfg', '+demo', demo.stem]
    else:
        command += ['+exec', 'rtxprobe.cfg', '+devmap'] + (args.map.get(profile) or spec['map'])
    assert sum(word.startswith('+') for word in command) <= 32, 'too many startup commands'
    assert len(subprocess.list2cmdline(command[1:]).encode('utf-8')) < 1024, 'startup command line too long'

    begin = time.monotonic()
    process = WindowsPrivateProcess(command, client.parent, row / 'stdout.log')
    try:
        while process.poll() is None and time.monotonic() - begin < args.timeout:
            time.sleep(0.2)
        forced = process.poll() is None
        code = process.poll()
    finally:
        try:
            process.close()
        except (subprocess.TimeoutExpired, PermissionError):
            pass
    elapsed = round(time.monotonic() - begin, 2)

    logs = sorted(home.rglob('*console.log'))
    text = logs[0].read_text(encoding='utf-8', errors='replace') if logs else ''
    (row / 'console.log').write_text(text, encoding='utf-8')
    result = dict(profile=profile, installation=str(installation), command=command, exitCode=code,
                  demo=dict(file=demo.name, sha256=sha(demo)) if demo is not None else None,
                  forcedStop=forced, elapsedSeconds=elapsed, errors=ERRORS.findall(text)[:8],
                  validationMessages=len(VALIDATION.findall(text)),
                  validationSample=VALIDATION.findall(text)[:3],
                  validationLayers='requested validation layers are not available' not in text,
                  gates=[dict(requested=int(g[0]), active=int(g[1]), require=int(g[2]), backend=g[3])
                         for g in GATE.findall(text)],
                  tracing=TRACING.findall(text), timing=TIMING.findall(text)[-3:],
                  rtLines=[line for line in text.splitlines() if line.startswith('RTX RT: ')][:8],
                  screenshots={})
    for name in ('rt', 'raster'):
        capture = next(home.rglob(f'{name}.png'), None)
        if capture is not None:
            kept = row / f'{name}.png'
            shutil.copy2(capture, kept)
            result['screenshots'][name] = dict(path=str(kept), sha256=sha(kept),
                                               brightness=round(brightness(kept, args.view_rows), 2))
    shots = result['screenshots']
    if 'rt' in shots and 'raster' in shots and shots['raster']['brightness'] > 0:
        result['tracedOverRaster'] = round(shots['rt']['brightness'] / shots['raster']['brightness'], 3)
    shutil.rmtree(home, ignore_errors=True)
    result['passed'] = (code == 0 and not forced and not result['errors'] and len(shots) == 2 and
                        result['tracing'] and not result['validationMessages'])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True, help='fnTech3 client executable')
    parser.add_argument('--output', type=Path, required=True, help='new result directory under .tmp')
    parser.add_argument('--game', action='append', required=True, metavar='PROFILE=INSTALLATION',
                        help=f'a legitimate installation for one of: {", ".join(PROFILES)}')
    parser.add_argument('--map', action='append', default=[], metavar='PROFILE=MAP[,FACTORY]',
                        help='load this map instead of the profile default')
    parser.add_argument('--demo', action='append', default=[], metavar='PROFILE=DEMO',
                        help='play this demo file (copied into the isolated home) instead of loading a map')
    parser.add_argument('--demo-advance', type=int, default=120, help='frames a demo plays before it freezes')
    parser.add_argument('--command', action='append', default=[],
                        help='console command run once the view freezes (rtx_rt_pick, rtx_rt_lightinfo ...)')
    parser.add_argument('--startup', action='append', default=[], metavar='NAME=VALUE',
                        help='set on the command line (latched renderer variables)')
    parser.add_argument('--set', action='append', default=[], metavar='NAME=VALUE',
                        help='set from the probe config before the map loads')
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=720)
    parser.add_argument('--accumulate', type=int, default=200, help='frames traced before the capture')
    parser.add_argument('--settle', type=int, default=120, help='frames after the raster restart')
    parser.add_argument('--view-rows', type=float, default=0.83,
                        help='fraction of the image, from the top, that the brightness covers')
    parser.add_argument('--timeout', type=float, default=300)
    args = parser.parse_args()
    client = args.client.resolve(strict=True)
    client.relative_to(ROOT)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    maps = {}
    for item in args.map:
        profile, _, value = item.partition('=')
        maps[profile] = value.split(',')
    args.map = maps
    demos = {}
    for item in args.demo:
        profile, _, path = item.partition('=')
        demos[profile] = Path(path).resolve(strict=True)
    args.demo = demos
    games = []
    for item in args.game:
        profile, _, path = item.partition('=')
        if profile not in PROFILES:
            parser.error(f'unknown profile {profile}')
        games.append((profile, Path(path).resolve(strict=True)))

    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  worktreeDirty=bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
                  client=str(client), clientSha256=sha(client),
                  renderer=str(client.parent / 'fnql_rtx_x86.dll'), results=[])
    renderer = client.parent / 'fnql_rtx_x86.dll'
    if not renderer.exists():
        renderer = client.parent / 'fnql_rtx_x86_64.dll'
    report['renderer'] = str(renderer)
    report['rendererSha256'] = sha(renderer) if renderer.exists() else None
    for profile, installation in games:
        before = inventory(installation)
        result = probe(client, profile, installation, output, args)
        result['installationUnchanged'] = inventory(installation) == before
        result['passed'] = bool(result['passed'] and result['installationUnchanged'])
        report['results'].append(result)
        print(json.dumps({k: result.get(k) for k in ('profile', 'passed', 'exitCode', 'forcedStop', 'gates',
                                                        'tracing', 'validationMessages', 'validationLayers',
                                                        'tracedOverRaster', 'errors', 'timing')}), flush=True)
    (output / 'manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return 0 if all(r['passed'] for r in report['results']) else 1


if __name__ == '__main__':
    sys.exit(main())
