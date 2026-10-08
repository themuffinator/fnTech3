#!/usr/bin/env python3
"""Holomatch's bots across map commands, and its drop notices, on a retail and
an fnTech3 dedicated server.

The same rcon-driven sequence runs on retail ``stvoyHM.exe`` 1.20 and on
fnTech3's ``stvef-mp`` dedicated server, with no client: a bot is added, then

1. ``devmap`` to another map;
2. ``spmap``, a bot added in its single-player game, then ``map``;
3. ``g_gametype 2`` set (latched) and ``map``;
4. ``map`` in a free-for-all;
5. ``kick`` of the bot by name.

After each step the probe reads which bots the server lists (``getstatus``)
and every drop notice the server echoed to its own console (``broadcast:
print "..."``, which both engines write on a dedicated server). Retail's rules
are in ``stvoyHM.exe``'s SV_Map_f (0x42e440) and SV_SpawnServer
(0x433b42..0x433b7b); the kick reason is 0x4cb1bc.

Windows only. Each server runs on a private desktop, bound to 127.0.0.1, with
an isolated writable folder under ``--output``; retail reads a copy of the four
retail paks (``.tmp/runtime/stvef-retail``), as every Elite Force probe does,
because it ignores ``fs_homepath``. No client, input or capture is involved.
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
from probe_legacy_sessions import (PROFILES, Peer, free_port, query, sha, split_settings,  # noqa: E402
                                   wait_for, inventory)

BOT = 'Munro'


def server(kind, runtime, folder, layout):
    port, password = free_port(), secrets.token_hex(6)
    settings = dict(dedicated='1', developer='1', logfile='2', net_ip='127.0.0.1', net_port=str(port),
                    net_enabled='1', sv_master1='', sv_master2='', sv_master3='', rconpassword=password,
                    sv_pure='0', g_gametype='0', bot_enable='1', sv_maxclients='8', g_doWarmup='0',
                    timelimit='0', fraglimit='0', g_inactivity='0', sv_hostname='fntech3-bot-rules')
    if kind == 'retail':
        base = folder / 'sb'
        base.mkdir(parents=True)
        exe, cwd = layout['root'] / 'stvoyHM.exe', layout['root']
        del settings['developer']
        settings = dict(fs_basepath=str(base), fs_cdpath=str(layout['cdpath']), **settings)
        log_dir, cfg_dir = base, base / 'BaseEF'
    else:
        home = folder / 'home'
        home.mkdir(parents=True)
        exe, cwd = runtime / 'fntech3.ded.exe', runtime
        settings = dict(com_gameProfile='stvef-mp', fs_basepath=str(INSTALLS['stvef-mp']),
                        fs_homepath=str(home), com_steamIntegration='0', **settings)
        log_dir, cfg_dir = home, home / 'BaseEF'
    boot, rest = split_settings(settings)
    cfg_dir.mkdir(parents=True, exist_ok=True)
    (cfg_dir / 'probeserver.cfg').write_text('\n'.join(rest) + '\n', encoding='latin1')
    peer = Peer('server', exe, cwd, boot, ['+exec', 'probeserver.cfg', '+map', 'hm_borg1'],
                folder, log_dir, 'qconsole.log', folder)
    peer.port, peer.password, peer.cfg_dir, peer.kind = port, password, cfg_dir, kind
    return peer


def rcon(peer, step, lines):
    """Runs lines from a cfg, so the server's console logs them."""
    name = f'probebot_{step}.cfg'
    (peer.cfg_dir / name).write_text('\n'.join(lines) + '\n', encoding='latin1')
    reply = None
    for _ in range(3):
        reply = query(peer.port, f'rcon {peer.password} exec {name}', timeout=1.5)
        if reply and 'exec' in reply:
            break
        time.sleep(1.1)
    time.sleep(1.1)
    return reply


def players(peer):
    reply = query(peer.port, 'getstatus probe', timeout=1.5)
    if not reply:
        return None
    lines = reply.split('\n')[2:]
    return [re.sub(r'\^.', '', found.group(1)) for line in lines
            if (found := re.search(r'"(.*)"', line))]


def mapname(peer):
    reply = query(peer.port, 'getinfo probe', timeout=1.5)
    found = re.search(r'\\mapname\\([^\\]*)', reply or '')
    return found.group(1) if found else None


def settle(peer, wanted, before=None):
    """Waits for the map to be up again, then for the bot list to stop changing."""
    wait_for(lambda: mapname(peer) == wanted, 60, 0.5)
    time.sleep(2.2)
    listed = players(peer)
    time.sleep(1.1)
    return players(peer) or listed


def notices(log):
    return re.findall(r'broadcast: print "([^\n]*)', log)


def run(kind, runtime, output, layout):
    folder = output / kind
    peer = server(kind, runtime, folder, layout)
    steps = []
    try:
        if not wait_for(lambda: mapname(peer) == 'hm_borg1', 60, 0.5):
            return dict(kind=kind, error='server never listed hm_borg1', sha256=peer.sha256)
        time.sleep(2.2)

        def step(name, lines, wanted):
            mark = len(notices(peer.log()))
            rcon(peer, name, lines)
            listed = settle(peer, wanted)
            steps.append(dict(step=name, commands=lines, map=mapname(peer), players=listed,
                              notices=notices(peer.log())[mark:]))
            time.sleep(1.1)

        step('addbot', [f'addbot {BOT} 1'], 'hm_borg1')
        step('devmap', ['devmap hm_voy1'], 'hm_voy1')
        step('addbot-again', [f'addbot {BOT} 1'], 'hm_voy1')
        step('spmap', ['spmap hm_borg1'], 'hm_borg1')
        step('addbot-single-player', [f'addbot {BOT} 1'], 'hm_borg1')
        step('map-from-single-player', ['map hm_voy1'], 'hm_voy1')
        step('ensure-bot', [] if any(BOT in p for p in (steps[-1]['players'] or [])) else [f'addbot {BOT} 1'],
             'hm_voy1')
        step('map-latched-single-player', ['g_gametype 2', 'map hm_borg1'], 'hm_borg1')
        step('free-for-all', ['g_gametype 0', 'map hm_voy1'], 'hm_voy1')
        step('addbot-free-for-all', [f'addbot {BOT} 1'], 'hm_voy1')
        step('map', ['map hm_borg1'], 'hm_borg1')
        step('kick', [f'kick {BOT}'], 'hm_borg1')
    finally:
        if peer.alive():
            query(peer.port, f'rcon {peer.password} quit', timeout=1.0)
        peer.stop(15)
        (folder / 'server-console.log').write_text(peer.log(), encoding='latin1')
    return dict(kind=kind, sha256=peer.sha256, command=peer.command, exit=peer.exit, forced=peer.forced,
                steps=steps)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True, help='fnTech3 runtime folder')
    parser.add_argument('--kinds', nargs='+', default=['retail', 'fntech3'], choices=['retail', 'fntech3'])
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runtime = args.runtime.resolve(strict=True)
    installation = INSTALLS['stvef-mp']
    layout = stage_retail('stvef-mp', PROFILES['stvef-mp'], installation)
    before = inventory(installation)
    results = [run(kind, runtime, output, layout) for kind in args.kinds]
    summary = dict(results=results, installationUnchanged=before == inventory(installation))
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    for result in results:
        print(f"== {result['kind']}")
        for item in result.get('steps', []):
            print(f"   {item['step']}: map={item['map']} players={item['players']} notices={item['notices']}")
        if result.get('error'):
            print('   error:', result['error'])
    print('installationUnchanged', summary['installationUnchanged'])


if __name__ == '__main__':
    main()
