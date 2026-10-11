#!/usr/bin/env python3
"""Exercise MOHAA 1.00's effects using its retail modules and m1l1's map.

Windows only. Run the stable win32-dev client windowed on a private desktop,
with no input injection. A small original map script in the isolated home
positions the observer, destroys m1l1's two gas barrels, and exercises other
hidden emitters and the grenade/bazooka network events. Captures come only
from the engine's screenshot command. This replaces the mission script for
the probe; it does not validate campaign progression or retail-engine parity.
RTX uses raster mode, so ray-query driver support is not required.

Example: python scripts/probe_mohaa_effects.py --renderer glx
         --output .tmp/mohaa-effects/glx
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

CASES = [
    ('empty_barrel', 'models/fx/barrel_empty_destroyed.tik', 'idle'),
    ('oil_barrel', 'models/fx/barrel_oil_destroyed.tik', 'idle'),
    ('water_barrel', 'models/fx/barrel_water_destroyed.tik', 'idle'),
    ('grenade', 'models/fx/m2fgrenadeexplosion.tik', 'idle'),
    ('bazooka', 'models/fx/bazookaexplosion.tik', 'idle'),
    ('electric', 'models/emitters/electric_arc.tik', 'start'),
    ('steam', 'models/emitters/pipe_steam.tik', 'start'),
    ('waterfall', 'models/emitters/waterfall.tik', 'start'),
    ('fire', 'models/emitters/firegood.tik', 'start'),
    ('mortar', 'models/emitters/mortar_dirt.tik', 'start'),
    ('tank', 'models/emitters/explosion_tank.tik', 'start'),
]
CONTINUOUS = {'electric', 'steam', 'waterfall', 'fire'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(root):
    return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in root.rglob('*') if p.is_file()}


def map_script():
    lines = ['main:'] + [f'cache "{model}"' for _, model, _ in CASES]
    lines += ['level waittill prespawn', 'level waittill spawn',
             '$player.origin = ( -3770 -950 400 )', '$player.viewangles = ( 0 90 0 )',
             '$player nodamage', 'wait 4']

    def capture(name):
        # Give snapshots/interpolation time to deliver the event, then take
        # early and late frames to include both the flash and the smoke.
        lines.extend(['wait 0.1', f'$player stufftext "screenshot {name}_early"',
                      'wait 0.4', f'$player stufftext "screenshot {name}_late"', 'wait 0.8',
                      f'$player stufftext "screenshot {name}_settled"', 'wait 0.2'])

    lines += ['$player stufftext "echo FXPROBE_BEGIN gas_barrels"',
              '$player stufftext "screenshot barrels_before"', 'wait 0.5']
    # Destroy index 2 first: removing an entity can renumber a target array.
    for number in (2, 1):
        lines += [f'$barrel[{number}] damage $world 15000 $world (0 0 0) (0 0 0) (0 0 0) 0 9 0 0']
        capture(f'gas_barrel_{number}')
        lines += ['wait 5']

    for name, model, anim in CASES:
        lines += [f'$player stufftext "echo FXPROBE_BEGIN {name}"', 'wait 0.5']
        if name in ('grenade', 'bazooka'):
            # Use the TIKI's Explosion class, whose server animation sends
            # the retail CGM event. Others test client animation emitters.
            lines += [f'local.fx = spawn "{model}" origin "-3770 -700 425"']
        else:
            lines += [f'local.fx = spawn script_model model "{model}" origin "-3770 -700 425"',
                      'local.fx rendereffects "+dontdraw"', 'local.fx.angles = ( -90 0 0 )']
        lines += [f'local.fx anim {anim}']
        capture(name)
        lines += ['if (local.fx)', '    local.fx remove']
        # Let particles expire before the next case. The long-lived mortar
        # and tank clouds run last; the report does not compare pixels.
        lines += ['wait 8' if name in CONTINUOUS else 'wait 5']
    lines += ['$player stufftext "echo FXPROBE_DONE"', 'end']
    # These BSP triggers normally enter the mission script. They do nothing
    # in this fixture, which only exercises effects at the barrels' location.
    for label in ('captain_talk', 'intro_exploder', 'hint_use', 'triggerend', 'dooropen',
                  'blowthedoor', 'ending_door', 'ending_kill', 'standup', 'hint_reach',
                  'door_locked', 'barrel', 'door_end_open'):
        lines += [f'{label}:', 'end']
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--install', type=Path, default=Path(r'E:\Games\Medal of Honor Allied Assault'))
    parser.add_argument('--runtime', type=Path, default=ROOT / 'meson/build/win32-dev')
    parser.add_argument('--port', type=int, default=12439)
    options = parser.parse_args()
    out, install, runtime = options.output.resolve(), options.install.resolve(), options.runtime.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('use a new, empty output directory')
    if install == out or install in out.parents or out in install.parents:
        parser.error('output and installation must be separate')
    exe = runtime / 'fntech3.exe'
    before = inventory(install)
    out.mkdir(parents=True, exist_ok=True)
    home = out / 'home'
    (home / 'main/maps').mkdir(parents=True)
    (home / 'main/maps/m1l1.scr').write_text(map_script(), encoding='ascii')
    log = home / 'main/qconsole.log'
    cvars = dict(com_gameProfile='mohaa', fs_basepath=str(install), fs_homepath=str(home),
                 logfile='2', developer='1', r_fullscreen='0', r_mode='3', cl_renderer=options.renderer,
                 in_mouse='0', in_nograb='1', in_joystick='0', s_volume='0', s_musicvolume='0',
                 net_ip='127.0.0.1', net_port=str(options.port), rconPassword='fxprobe', sv_pure='0',
                 com_maxfps='60', com_maxfpsUnfocused='60', thereisnomonkey='1', cg_effectdetail='1',
                 g_ai='0', cl_shownet='2', rtx_rt_mode='0')
    args = [str(exe)]
    for key, value in cvars.items():
        args += ['+set', key, value]
    args += ['+devmap', 'm1l1']
    report = dict(renderer=options.renderer, command=args, executable_sha256=sha(exe),
                  git_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  modules={name: sha(install / name) for name in ('main/cgamex86.dll', 'main/gamex86.dll')},
                  cases=['gas_barrel_2', 'gas_barrel_1'] + [c[0] for c in CASES],
                  scope='retail-module effects fixture; replacement mission script; no input; engine screenshots')
    def text():
        return log.read_text('latin-1') if log.exists() else ''

    def send(command):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(b'\xff\xff\xff\xff\x02rcon fxprobe ' + command.encode('ascii') + b'\n',
                        ('127.0.0.1', options.port))

    # Refuse an occupied game port, so commands cannot reach another probe.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', options.port))
    process = WindowsPrivateProcess(args, ROOT, out / 'stdout.txt')
    try:
        end = time.monotonic() + 240
        released = False
        while time.monotonic() < end:
            content = text()
            if process.poll() is not None:
                raise RuntimeError(f'client exited early: {process.poll()}')
            if "Script 'maps/m1l1.scr' was not properly loaded" in content:
                raise RuntimeError('effects fixture script failed to load')
            if not released and 'CL_FirstSnapshot' in content:
                send('finishloadingscreen')
                released = True
            if re.search(r'^FXPROBE_DONE\s*$', content, re.M):
                break
            time.sleep(.15)
        else:
            raise RuntimeError('effects fixture did not complete within 240 seconds')
        time.sleep(.3)
        send('quit')
        process.wait(15)
        report['complete'] = True
    finally:
        process.close()
        report['installation_unchanged'] = inventory(install) == before
        report['screenshots'] = sorted(p.name for p in (home / 'main/screenshots').glob('*.png'))
        content = text()
        report['network_effect_messages'] = content.count('svc_cgameMessage')
        report['network_events_by_case'] = {}
        for name in ('grenade', 'bazooka'):
            segment = re.search(rf'^FXPROBE_BEGIN {name}\s*$(.*?)(?=^FXPROBE_BEGIN |^FXPROBE_DONE)',
                                content, re.M | re.S)
            report['network_events_by_case'][name] = segment[1].count('svc_cgameMessage') if segment else 0
        report['diagnostics'] = sorted(set(line for line in content.splitlines()
                                         if re.search(r'ERROR|does not serve|Bad CGM|bad reType|Unknown command', line)))
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    expected = {'barrels_before.png'} | {f'{case}_{phase}.png' for case in report['cases']
                                         for phase in ('early', 'late', 'settled')}
    missing = expected - set(report['screenshots'])
    if missing or not report['installation_unchanged']:
        raise RuntimeError(f'missing captures {sorted(missing)}; installation unchanged {report["installation_unchanged"]}')
    if any(count != 1 for count in report['network_events_by_case'].values()):
        raise RuntimeError(f'expected one network effect per case: {report["network_events_by_case"]}')
    print(json.dumps(dict(output=str(out), screenshots=len(report['screenshots']),
                          network_effect_messages=report['network_effect_messages'],
                          diagnostics=report['diagnostics']), indent=2))


if __name__ == '__main__':
    main()
