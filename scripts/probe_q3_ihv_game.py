#!/usr/bin/env python3
"""Exercise IHV API 4 dedicated lifecycle with a user-supplied installation.

Requires a Windows x86 server built with -Dq3-ihv-development=true. All runs
use fresh writable homes and a private desktop, with networking, rendering,
audio and input disabled. This does not establish client, bot or wire support.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

from probe_demo_roundtrip import fntech3_command, run

ROOT = Path(__file__).resolve().parents[1]
MAPS = ('ihv_test1', 'km_portal', 'test_fog2', 'tim_dm1')
SPAWN = re.compile(r'IHV spawn: (\w+) at (\d+), entities=(\d+)/(\d+) stride=(\d+) base=(\w+), (\d+) allocations, (\d+) bytes')
SHUTDOWN = re.compile(r'IHV shutdown: (\d+) allocations, (\d+) bytes before host cleanup')


def identity(path):
    return dict(path=str(path), bytes=path.stat().st_size,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=30)
    args = parser.parse_args()
    for name in ('server', 'installation', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    if not (ROOT/'.tmp').resolve() in args.output.parents or args.output.exists():
        parser.error('Use a fresh output directory inside the project .tmp directory')
    if args.timeout <= 0 or args.timeout > 60:
        parser.error('Timeout must be between zero and 60 seconds')
    for source in (args.installation, args.server.parent):
        if source == args.output or source in args.output.parents or args.output in source.parents:
            parser.error('Inputs and output must be disjoint')
    inputs = [args.installation/name for name in ('quake3.exe', 'qagamex86.dll', 'cgamex86.dll',
        'baseq3/default.cfg', 'baseq3/scripts/bots.cfg', 'baseq3/maps/tim_dm1.nav')]
    inputs += [args.installation/f'baseq3/maps/{name}.bsp' for name in MAPS]
    before = [identity(p) for p in inputs]
    # Scope these expected observations to the supplied original game module.
    if before[1]['sha256'] != '9855d375751272d0b22e9078e827cf7ff3c2e42867f525b7fd84b93967c0eeb6':
        parser.error('This probe expects the documented February 1999 IHV game DLL')
    args.output.mkdir(parents=True)
    report = dict(scope='Dedicated original-DLL map lifecycle and error cleanup; no clients, bots or network',
        commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        server=identity(args.server), inputs=before, runs=[])
    lifecycle = ['set sv_pure 0', 'set bot_enable 0', 'set sv_maxclients 8', 'set maxclients 8']
    for n, name in enumerate(MAPS):
        lifecycle += [f'map {name}', 'wait 30', f'echo IHV_MAP_{n+1}']
    lifecycle += ['map_restart 0', 'wait 30', 'echo IHV_MAP_RESTART', 'killserver',
                  'map ihv_test1', 'wait 30', 'echo IHV_MAP_RELOAD', 'status', 'quit']
    scenarios = [
        ('lifecycle', lifecycle, None),
        ('init-rejection', ['set maxentities 1000000', 'map ihv_test1'], 'IHV game allocation rejected'),
        ('capacity-rejection', ['set sv_maxclients 8', 'set maxclients 4', 'map ihv_test1'], 'exceeds native maxclients'),
    ]
    for name, lines, expected_error in scenarios:
        folder = args.output/name; game = folder/'home/baseq3'; game.mkdir(parents=True)
        for config in ('autoexec.cfg', 'q3config_server.cfg'):
            (game/config).write_text('// Isolated probe configuration.\n', encoding='utf-8')
        (game/'probe.cfg').write_text('\n'.join(lines)+'\n', encoding='utf-8')
        command = fntech3_command(args.server, args.installation, folder/'home', 'q3-ihv',
            ['+set', 'dedicated', '1', '+set', 'net_enabled', '0', '+set', 'com_viewlog', '0',
             '+set', 'viewlog', '0', '+set', 'developer', '1', '+set', 'com_buildScript', '1',
             '+exec', 'probe.cfg'])
        code, forced, seconds, console = run(command, args.server.parent, folder/'stdout.log', game/'qconsole.log', args.timeout)
        (folder/'console.log').write_text(console, encoding='utf-8')
        stdout = (folder/'stdout.log').read_text(encoding='utf-8', errors='replace')
        spawned = [dict(zip(('map', 'time', 'entities', 'capacity', 'stride', 'base', 'allocations', 'bytes'), values))
                   for values in SPAWN.findall(console)]
        shutdown = [tuple(map(int, values)) for values in SHUTDOWN.findall(console)]
        row = dict(name=name, command=command, exit_code=code, forced=forced, seconds=seconds,
            initializations=console.count('------- Game Initialization -------'),
            shutdown_callbacks=console.count('==== ShutdownGame ===='),
            spawns=spawned, shutdown_allocations=shutdown,
            warnings=dict((w, console.count(w)) for w in sorted(set(re.findall(r'WARNING:[^\r\n]*', console)))))
        if expected_error:
            row['expected_error'] = expected_error
            row['passed'] = (code == 1 and not forced and expected_error in stdout+console
                and row['initializations'] == 1 and row['shutdown_callbacks'] == (0 if name == 'init-rejection' else 1)
                and shutdown == [(0, 0)] and not spawned and 'recursive error' not in stdout+console)
        else:
            markers = [f'IHV_MAP_{n}' for n in range(1, 5)] + ['IHV_MAP_RESTART', 'IHV_MAP_RELOAD']
            row['passed'] = (code == 0 and not forced and row['initializations'] == 2 and row['shutdown_callbacks'] == 2
                and shutdown == [(0, 0), (0, 0)] and [s['map'] for s in spawned] == [*MAPS, 'tim_dm1', 'ihv_test1']
                and all(s['time'] == '1000' and s['stride'] == '556' and s['capacity'] == '1024' for s in spawned)
                and len({s['base'] for s in spawned[:5]}) == 1 and all(re.search(r'^'+m+r'\s*$', console, re.M) for m in markers)
                and console.count('...loaded 86 surfaces and 319 neighbors') == 2
                and not re.search(r'ERROR:|Sys_Error:|recursive error', stdout+console))
        report['runs'].append(row)
        print(name, 'PASS' if row['passed'] else 'FAIL', flush=True)
        (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    report['inputs_unchanged'] = before == [identity(p) for p in inputs]
    report['passed'] = report['inputs_unchanged'] and all(r['passed'] for r in report['runs'])
    report['artifacts'] = [identity(p) for p in sorted(args.output.rglob('*')) if p.is_file() and p.name != 'report.json']
    (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['passed'] else 1


if __name__ == '__main__': raise SystemExit(main())
