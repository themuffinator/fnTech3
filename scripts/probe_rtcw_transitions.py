#!/usr/bin/env python3
"""Play RTCW SP level exits through to the next level, with no device input.

Each run starts one official map with `spdevmap` in a private client built
with tests/rtcw_transition_probe.hpp. The hook clicks only each level's
briefing Ready button (AGENTS.md's standing intro-screen permission). On
every level that has a step file, console commands from that file (god,
notarget, give, setviewpos) carry the player into the level's own objective
and exit triggers; the retail game script then saves the persistent player
and changes level itself. Installations stay read-only, runs are windowed
on an unswitched private desktop, and captures use the engine screenshot.

Build the private client away from the shared tree (an export or worktree),
as scripts/prepare_rtcw_campaign_probe.py does for its own hook: include the
header just before CL_Frame, call CL_RTCWTransitionFrame() first in it, and
register `fntech3_transition_start` beside `modelist`.
"""
from pathlib import Path
import argparse
import datetime
import json
import re
import shutil
import subprocess
import time
import probe_rtcw_boot as boot

ROOT = Path(__file__).resolve().parents[1]

# Points inside each trigger, read from each map's own entities, models and
# brushes: the objective triggers its player script marks, then the exit
# trigger whose script runs `changelevel <next> persistent`. Each is the
# centre of one of the trigger's brushes -- village1's objective 2 is a chain
# of thin walls whose bounding box centre touches none -- and z is that
# brush's floor plus the player's 24-unit mins, so the player's box overlaps it.
STEPS = {
    'escape1': [('action3', 512, 680, -536), ('action14', -3408, 776, 152), ('endmap', -3696, 960, 152)],
    'village1': [('action3', -2644, -1216, -376), ('got_objective2', -394, 4168, -84),
                 ('endmapb', -384, 4544, -110)],
    'dig': [('getobjective1', 1238, 1377, 48), ('endmap', 2447, 1416, -222)],
    'castle': [('getobjective1', 1092, 2131, 24), ('endmap', 1088, 2360, 24)],
}

CHAINS = {
    'escape': ['cutscene1', 'escape1', 'escape2'],
    'village': ['village1', 'crypt1'],
    'castle': ['dig', 'castle', 'end'],
    'forest': ['cutscene6', 'forest'],
    'sfm': ['cutscene9', 'sfm'],
    'norway': ['cutscene11', 'norway'],
    'dam': ['cutscene14', 'dam'],
    'control-escape2': ['escape2'],
    'control-crypt1': ['crypt1'],
    'control-castle': ['castle'],
    'control-end': ['end'],
    # A map command typed while the briefing holds the game: WolfSP.exe
    # ignores it, so the level stays escape2.
    'guard-briefing': ['escape2'],
    # The player dies after taking a weapon and armour: the game reloads the
    # save it made when play began, through map_restart, without them.
    'death-castle': ['castle'],
}

CHAIN_SETTINGS = {
    'guard-briefing': dict(rtcw_probe_briefing_command='spdevmap crypt1'),
    'death-castle': dict(rtcw_probe_restarts='1'),
}

# Step files that replace a level's teleports in one chain.
STEP_OVERRIDES = {
    'death-castle': {'castle': ['give Venom', 'give armor', 'wait 120',
                                'echo TRANSITION step castle kill', 'kill']},
}

OBSERVE = re.compile(r'TRANSITION (\S+) level=(\d+) map=(\S*) state=(\d+) valid=(\d+) time=(-?\d+) camera=(\d+) '
                     r'paused=(\d+) norender=(\d+) reloading=(\d+) persid=(\S*) health=(-?\d+) armor=(-?\d+) '
                     r'weapon=(\d+) weapons=([0-9a-f]+),([0-9a-f]+) ammo=(\S*) origin=(\S+) '
                     r'restarts=(\d+) loading=(\d+)')
FIELDS = ('phase', 'level', 'map', 'state', 'valid', 'time', 'camera', 'paused', 'norender', 'reloading',
          'persid', 'health', 'armor', 'weapon', 'weapons0', 'weapons1', 'ammo', 'origin', 'restarts', 'loading')


def step_config(name):
    lines = ['god', 'notarget', 'give Venom', 'give armor', 'wait 60']
    for trigger, x, y, z in STEPS[name]:
        lines += [f'echo TRANSITION step {name} {trigger}', f'setviewpos {x} {y} {z} 0', 'wait 240']
    return ''.join(line + '\n' for line in lines)


def run(args, chain, output):
    levels = CHAINS[chain]
    folder = output / chain; folder.mkdir()
    home = folder / 'home'; game = home / 'main'; game.mkdir(parents=True)
    values = dict(developer='1', com_introplayed='1', com_recommendedSet='1',
        com_maxfps='60', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
        sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0', cl_motd='0',
        s_backend='openal', s_muteWhenUnfocused='1', s_muteWhenMinimized='1',
        r_csmShadows='0', con_notifytime='0', aicast_debug='2', aicast_debugname='player',
        rtcw_probe_levels=str(len(levels)), rtcw_probe_restarts='0', rtcw_probe_dwell=str(args.dwell),
        rtcw_probe_timeout=str(args.timeout), activeAction='fntech3_transition_start')
    values.update(CHAIN_SETTINGS.get(chain, {}))
    for i in range(1, 6): values[f'sv_master{i}'] = ''
    config = ''.join(f'set {key} "{value}"\n' for key, value in values.items())
    (game / 'autoexec.cfg').write_text(config)
    (game / 'wolfconfig.cfg').write_text('')
    steps = {name: step_config(name) for name in levels[:-1] if name in STEPS}
    steps.update({name: ''.join(line + '\n' for line in lines)
                  for name, lines in STEP_OVERRIDES.get(chain, {}).items()})
    for name, text in steps.items():
        (game / f'transition-{name}.cfg').write_text(text)
        (folder / f'transition-{name}.cfg').write_text(text)
    startup = dict(com_gameProfile='rtcw-sp', fs_basepath=str(args.installation),
        fs_homepath=str(home), fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1',
        r_customWidth='800', r_customHeight='600', in_mouse='0', in_joystick='0', in_nograb='1',
        s_initsound='1', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
        cl_renderer='glx', logfile='4', net_enabled='0')
    command = [str(args.executable)]
    for key, value in startup.items(): command += ['+set', key, value]
    command += ['+spdevmap', levels[0]]
    assert len(subprocess.list2cmdline(command[1:]).encode()) < 1024
    row = dict(chain=chain, levels=levels, settings=CHAIN_SETTINGS.get(chain, {}), command=command, screenshots=[])
    process = None
    started = time.monotonic()
    try:
        process = boot.WindowsPrivateProcess(command, args.executable.parent, folder / 'stdout.log')
        row['exitCode'] = boot.wait_for_boot(process, game / 'qconsole.log', command, args.timeout + 60)
    except Exception as error:
        row.update(error=str(error), errorType=type(error).__name__)
    finally:
        if process:
            row['forcedStop'] = process.poll() is None; process.close()
        row['seconds'] = round(time.monotonic() - started, 1)
        log = (game / 'qconsole.log').read_text(errors='replace') if (game / 'qconsole.log').exists() else ''
        (folder / 'console.log').write_text(log, encoding='utf-8')
        for capture in sorted(home.rglob('transition-*.png')):
            dest = folder / capture.name; shutil.copy2(capture, dest)
            row['screenshots'].append(dict(path=str(dest.relative_to(ROOT)), sha256=boot.sha(dest)))
        row['observations'] = [dict(zip(FIELDS, m.groups())) for m in OBSERVE.finditer(log)]
        row['steps'] = re.findall(r'^TRANSITION step (\S+ \S+)', log, re.M)
        row['clicks'] = log.count('TRANSITION briefing Ready click')
        row['playerScript'] = [line for line in log.splitlines()
            if re.match(r'\(player\) AIScript (?:event: trigger|command: (?:objectivemet|missionsuccess|changelevel))', line)]
        row['levelLoads'] = re.findall(r'^Server: (\S+)', log, re.M) or re.findall(r'^------ Server Initialization ------\n.*?Server: (\S+)', log, re.S)
        row['persistentFiles'] = sorted(str(p.relative_to(home)) for p in home.rglob('*.psw'))
        row['saves'] = sorted(str(p.relative_to(home)) for p in home.rglob('*.svg'))
        row['engineErrors'] = [line for line in log.splitlines() if re.search(
            r'error:|sys_error|fatal crashed:|not implemented|unsupported rtcw|rtcw[^\n]*unsupported|server command overflow|lost reliable commands|recursive cl_shutdown', line, re.I)]
        row['objectivesNotComplete'] = log.count('objectivesnotcomplete')
        reached = [o['map'] for o in row['observations'] if o['phase'] == 'level-active']
        row['levelsReached'] = reached
        row['restarts'] = sum(o['phase'] == 'level-restarted' for o in row['observations'])
        row['passed'] = (reached == levels and row.get('exitCode') == 0 and not row.get('forcedStop') and
                         not row['engineErrors'] and any(o['phase'] == 'final' for o in row['observations']) and
                         row['restarts'] == int(values['rtcw_probe_restarts']))
        if not args.keep_home:
            boot.remove_home(home, output)
        (folder / 'manifest.json').write_text(json.dumps(row, indent=2) + '\n')
    print(chain, 'PASS' if row['passed'] else 'FAIL', ' -> '.join(row['levelsReached']), row.get('error', ''), flush=True)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--chains', nargs='+', choices=sorted(CHAINS), default=sorted(CHAINS))
    parser.add_argument('--dwell', type=int, default=8)
    parser.add_argument('--timeout', type=int, default=420)
    parser.add_argument('--keep-home', action='store_true')
    args = parser.parse_args()
    args.executable = args.executable.resolve(strict=True); args.executable.relative_to(ROOT)
    args.installation = args.installation.resolve(strict=True)
    output = args.output.resolve(); output.relative_to(ROOT / '.tmp'); output.mkdir(parents=True, exist_ok=False)
    inventory = boot.inventory(args.installation)
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        executable=str(args.executable), executableSha256=boot.sha(args.executable),
        hookSha256=boot.sha(ROOT / 'tests/rtcw_transition_probe.hpp'),
        installation=str(args.installation), runs=[])
    for chain in args.chains:
        if (output / 'stop').exists(): break
        report['runs'].append(run(args, chain, output))
        report['installationUnchanged'] = inventory == boot.inventory(args.installation)
        (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        if not report['installationUnchanged']: raise RuntimeError('Installation metadata changed')
    return 0 if all(row['passed'] for row in report['runs']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
