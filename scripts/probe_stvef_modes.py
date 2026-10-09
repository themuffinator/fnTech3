#!/usr/bin/env python3
"""Play Holomatch's game types and Expansion Pack modes with bots on retail
``stvoyHM.exe``'s dedicated server and on fnTech3's.

Each case starts a map with `devmap` in one game type, with any Expansion Pack
mode (`g_pMod*`) set before the map, adds bots, and plays at `timescale 3`
(or ``--timescale``)
until the game logs its `Exit:` line or the case times out. The server's game
log (`games.log`) and console log are read for kills, the exit reason, the
modes the engine left on after its conflict rules, and errors. No client
takes part, and no input is involved.

Windows only. Every server is bound to 127.0.0.1 with an isolated writable
folder under ``--output``; retail reads a copy of the four retail paks
(``.tmp/runtime/stvef-retail``), as every Elite Force probe does, because it
ignores ``fs_homepath``.
"""
from pathlib import Path
import argparse
import json
import re
import secrets
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'scripts'))
from probe_legacy_discovery import INSTALLS, stage_retail  # noqa: E402
from probe_legacy_sessions import PROFILES, Peer, free_port, query, split_settings, wait_for, inventory  # noqa: E402

PMODS = ('g_pModAssimilation', 'g_pModDisintegration', 'g_pModActionHero', 'g_pModSpecialties',
         'g_pModElimination')
# (label, map, game type, Expansion Pack modes asked for, bots)
CASES = [
    ('ffa', 'hm_borg1', 0, (), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    ('ffa-disintegration', 'hm_borg1', 0, ('g_pModDisintegration',), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    ('ffa-actionhero', 'hm_voy1', 0, ('g_pModActionHero',), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    ('ffa-specialties', 'hm_dn1', 0, ('g_pModSpecialties',), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    ('ffa-elimination', 'hm_kln1', 0, ('g_pModElimination',), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    ('tournament', 'hm_voy2', 1, (), ('Chell', 'Chakotay')),
    ('team', 'hm_for1', 3, (), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    ('team-assimilation', 'hm_borg2', 3, ('g_pModAssimilation',), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    ('team-elimination', 'hm_scav1', 3, ('g_pModElimination',), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    ('ctf', 'ctf_voy1', 4, (), ('Chell', 'Chakotay', 'Foster', 'Gamma', 'Munro', 'Biessman')),
    ('ctf-specialties', 'ctf_kln1', 4, ('g_pModSpecialties',), ('Chell', 'Chakotay', 'Foster', 'Gamma')),
    # Conflicts the engine resolves before the map: Assimilation needs team
    # play, and Action Hero is refused by team game types.
    ('ffa-asks-assimilation', 'hm_cam', 0, ('g_pModAssimilation',), ('Chell', 'Chakotay')),
    ('team-asks-actionhero', 'hm_noon', 3, ('g_pModActionHero',), ('Chell', 'Chakotay')),
]
# The game speed and time limit every case plays at (--timescale, --timelimit).
PLAY = dict(timescale='3', timelimit='3')


def server(kind, runtime, folder, layout, case, late_bots=False):
    label, level, gametype, pmods, bots = case
    port, password = free_port(), secrets.token_hex(6)
    settings = dict(dedicated='1', logfile='2', net_ip='127.0.0.1', net_port=str(port), net_enabled='1',
                    sv_master1='', sv_master2='', sv_master3='', rconpassword=password, sv_pure='0',
                    g_gametype=str(gametype), bot_enable='1', sv_maxclients='12', g_doWarmup='0', g_warmup='0',
                    fraglimit='10', capturelimit='3', timelimit=PLAY['timelimit'], g_inactivity='0', g_log='games.log',
                    g_logSync='1', sv_hostname='fntech3-modes')
    for name in PMODS:
        settings[name] = '1' if name in pmods else '0'
    if kind == 'retail':
        base = folder / 'sb'
        base.mkdir(parents=True)
        exe, cwd = layout['root'] / 'stvoyHM.exe', layout['root']
        settings = dict(fs_basepath=str(base), fs_cdpath=str(layout['cdpath']), **settings)
        log_dir, cfg_dir = base, base / 'BaseEF'
    else:
        home = folder / 'home'
        home.mkdir(parents=True)
        exe, cwd = runtime / 'fntech3.ded.exe', runtime
        settings = dict(com_gameProfile='stvef-mp', fs_basepath=str(INSTALLS['stvef-mp']), fs_homepath=str(home),
                        com_steamIntegration='0', **settings)
        log_dir, cfg_dir = home, home / 'BaseEF'
    boot, rest = split_settings(settings)
    cfg_dir.mkdir(parents=True, exist_ok=True)
    start = rest + [f'devmap {level}', f"timescale {PLAY['timescale']}"]
    if not late_bots:
        start += [f'addbot {bot} 2' for bot in bots]
    (cfg_dir / 'probeserver.cfg').write_text('\n'.join(start) + '\n', encoding='latin1')
    peer = Peer('server', exe, cwd, boot, ['+exec', 'probeserver.cfg'], folder, log_dir, 'qconsole.log', folder)
    peer.port, peer.password, peer.cfg_dir = port, password, cfg_dir
    return peer


def summarize(peer, case):
    game = peer.game_log()
    console = peer.log()
    init = re.findall(r'InitGame: ([^\n]*)', game)
    modes = {}
    if init:
        for name in PMODS + ('g_gametype',):
            found = re.search(r'\\%s\\([^\\]*)' % name, init[-1])
            modes[name] = found.group(1) if found else None
    exits = re.findall(r'Exit: ([^\n]*)', game)
    kills = re.findall(r'Kill: [^\n]*', game)
    return dict(initGame=bool(init), modes=modes, exit=exits[-1] if exits else None, kills=len(kills),
                means=sorted({k.split(' by ')[-1].strip() for k in kills if ' by ' in k}),
                scores=re.findall(r'(?m)^ *\d+:\d+ *score: [^\n]*', game)[-12:],
                errors=re.findall(r'(?m)^[^\n]*(?:ERROR|rror:|crash|bypass data segment)[^\n]*$', console)[:6])


def run_case(kind, runtime, output, layout, case, timeout, trial=0, late_bots=False):
    folder = output / (f'{kind}-{case[0]}' if not trial else f'{kind}-{case[0]}-{trial}')
    peer = server(kind, runtime, folder, layout, case, late_bots)
    began = time.monotonic()
    try:
        if late_bots:
            # The bots join a level already running, as an admin adds them,
            # rather than from the cfg that starts it.
            wait_for(lambda: case[1] in (query(peer.port, 'getinfo probe', timeout=1.0) or ''), 60, 0.5)
            time.sleep(3.0)
            (peer.cfg_dir / 'probebots.cfg').write_text(
                ''.join(f'addbot {bot} 2\n' for bot in case[4]), encoding='latin1')
            query(peer.port, f'rcon {peer.password} exec probebots.cfg', timeout=1.5)
            began = time.monotonic()
        wait_for(lambda: 'Exit:' in peer.game_log() or not peer.alive(), timeout, 1.0)
    finally:
        seconds = round(time.monotonic() - began, 1)
        if peer.alive():
            query(peer.port, f'rcon {peer.password} quit', timeout=1.0)
        peer.stop(15)
        (folder / 'server-console.log').write_text(peer.log(), encoding='latin1')
        (folder / 'server-games.log').write_text(peer.game_log(), encoding='latin1')
    return dict(kind=kind, case=case[0], trial=trial, map=case[1], gametype=case[2], asked=list(case[3]),
                bots=list(case[4]),
                seconds=seconds, sha256=peer.sha256, serverExit=peer.exit, forced=peer.forced,
                **summarize(peer, case))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True, help='fnTech3 runtime folder')
    parser.add_argument('--kinds', nargs='+', default=['retail', 'fntech3'], choices=['retail', 'fntech3'])
    parser.add_argument('--cases', nargs='+', default=[c[0] for c in CASES], choices=[c[0] for c in CASES])
    parser.add_argument('--timeout', type=float, default=150)
    parser.add_argument('--repeat', type=int, default=1, help='trials of each case on each engine')
    parser.add_argument('--late-bots', action='store_true', help='add the bots by rcon once the level runs')
    parser.add_argument('--timescale', default=PLAY['timescale'], help='game speed every case plays at')
    parser.add_argument('--timelimit', default=PLAY['timelimit'], help='minutes of game time a case may last')
    args = parser.parse_args()
    PLAY.update(timescale=args.timescale, timelimit=args.timelimit)
    if sys.platform != 'win32':
        parser.error('Windows only')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runtime = args.runtime.resolve(strict=True)
    installation = INSTALLS['stvef-mp']
    layout = stage_retail('stvef-mp', PROFILES['stvef-mp'], installation)
    before = inventory(installation)
    results = []
    for case in [c for c in CASES if c[0] in args.cases]:
        for kind, trial in [(k, n) for n in range(args.repeat) for k in args.kinds]:
            result = run_case(kind, runtime, output, layout, case, args.timeout, trial, args.late_bots)
            results.append(result)
            print(f"{kind:8s} {case[0]:22s} exit={result['exit']!r} kills={result['kills']} "
                  f"modes={ {k[6:]: v for k, v in result['modes'].items() if v == '1'} } "
                  f"gt={result['modes'].get('g_gametype')} errors={result['errors'][:2]}", flush=True)
            (output / 'summary.json').write_text(json.dumps(dict(results=results), indent=2) + '\n',
                                                 encoding='utf-8')
    summary = dict(results=results, installationUnchanged=before == inventory(installation))
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print('installationUnchanged', summary['installationUnchanged'])


if __name__ == '__main__':
    main()
