#!/usr/bin/env python3
"""Show a dedicated server's game module spawning a map's entity override.

Fresh runs of the dedicated server on one map, each in its own home with
networking off: the first writes the level's entities with
`sv_saveentfile`; a marker entity whose classname no game knows is appended
to that file; the second must load the override and the game module must
print that the marker has no spawn function; the third, with
`sv_entityOverrides 0`, must do neither. The installation is inventoried
before and after. For profiles whose client cannot yet run a level here
(q3-ihv, jk2-mp, jka-sp) as well as any other.

--from-bsp builds the override from the map's own BSP entity lump instead
(loose or in a pak), for a profile whose level stops before sv_saveentfile
could run; --expect-drop accepts a level that stops with a drop after its
game module spawned it (jka-sp's first frame), stopping the server then.

usage: python scripts/probe_entity_override_server.py --profile jka-mp
    --installation "E:/Games/Jedi Academy" --map mp/ffa1 --output .tmp/<task>/jka-mp
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import time
import zipfile
from pathlib import Path

from probe_entity_editor import WindowsPrivateProcess, inventory

ROOT = Path(__file__).resolve().parents[1]
GAME = {'q3': 'baseq3', 'q3-ihv': 'baseq3', 'q3test': 'baseq3', 'ql': 'baseq3', 'rtcw-mp': 'main',
        'wolfet': 'etmain', 'stvef-mp': 'baseEF', 'jka-mp': 'base', 'jka-sp': 'base', 'jk2-mp': 'base', 'jk2-sp': 'base'}
MARKER = 'fntech3_override_marker'
DROP = re.compile(r'(?:\^\d)?(?:ERROR|Sys_Error|recursive error)[^\n]*')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def bsp_entities(installation, game, name):
    """The map's entity lump (IBSP or RBSP lump 0), as the engine would find
    the BSP: loose first, then the paks, the last in name order winning."""
    target = f'maps/{name}.bsp'
    data = None
    loose = installation / game / target
    if loose.is_file():
        data = loose.read_bytes()
    else:
        for pak in sorted((installation / game).glob('*.pk3'), key=lambda path: path.name.lower()):
            with zipfile.ZipFile(pak) as archive:
                for entry in archive.namelist():
                    if entry.lower() == target.lower():
                        data = archive.read(entry)
    if data is None or data[:4] not in (b'IBSP', b'RBSP'):
        raise SystemExit(f'--from-bsp: no IBSP or RBSP {target} under {installation / game}')
    offset, length = struct.unpack_from('<ii', data, 8)
    return data[offset:offset + length].rstrip(b'\0')


def run(command, cwd, stdout, log, timeout, expect_drop):
    """Run the server until it exits; with expect_drop, stop it two seconds
    after the console shows a drop. (exit, forced, stoppedAfterDrop, s)."""
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, cwd, stdout)
    dropped = None
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            if expect_drop and dropped is None and log.is_file() and DROP.search(
                    log.read_text(encoding='utf-8', errors='replace').replace(MARKER, '')):
                dropped = time.monotonic()
            if dropped is not None and time.monotonic() - dropped >= 2:
                break
            time.sleep(0.1)
        stopped = dropped is not None and process.poll() is None
        forced = process.poll() is None and not stopped
        code = process.poll()
    finally:
        process.close()
    return code, forced, stopped, round(time.monotonic() - begin, 3)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--profile', required=True, choices=sorted(GAME))
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--map', required=True)
    parser.add_argument('--server', type=Path, default=ROOT / 'meson/build/win32-dev/fntech3.ded.exe')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=90)
    parser.add_argument('--from-bsp', action='store_true', help="build the override from the BSP's lump")
    parser.add_argument('--expect-drop', action='store_true', help='the level stops with a drop after spawning')
    args = parser.parse_args()
    installation, server, output = (path.resolve() for path in (args.installation, args.server, args.output))
    if (ROOT / '.tmp').resolve() not in output.parents or output.exists():
        parser.error('use a fresh output directory inside the project .tmp directory')
    if not re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_/-]*', args.map) or '..' in args.map:
        parser.error('a map name of letters, digits, _, - and / only')
    if args.expect_drop and not args.from_bsp:
        parser.error('a level that stops cannot run sv_saveentfile: use --from-bsp with --expect-drop')
    game = GAME[args.profile]
    before = inventory(installation)
    output.mkdir(parents=True)
    report = dict(profile=args.profile, map=args.map, installation=str(installation), fromBsp=args.from_bsp,
                  expectDrop=args.expect_drop,
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  server=str(server), serverSha256=sha(server), runs={})
    override = None

    def start(name, lines):
        home = output / name
        (home / game).mkdir(parents=True)
        for config in ('autoexec.cfg', 'q3config_server.cfg', 'jampserver.cfg', 'jk2mpserver.cfg'):
            (home / game / config).write_text('// Isolated probe configuration.\n', encoding='utf-8')
        (home / game / 'probe.cfg').write_text('\n'.join(lines) + '\n', encoding='utf-8')
        if override is not None:
            target = home / game / 'maps' / f'{args.map}.ent'
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(override)
        command = [str(server)]
        for key, value in dict(com_gameProfile=args.profile, fs_basepath=str(installation), fs_homepath=str(home),
                               fs_game='', fs_cdpath='', dedicated='1', net_enabled='0', developer='1',
                               logfile='2', com_steamIntegration='0', com_viewlog='0').items():
            command += ['+set', key, value]
        command += ['+exec', 'probe.cfg']
        code, forced, stopped, elapsed = run(command, server.parent, home / 'stdout.log',
                                             home / game / 'qconsole.log', args.timeout, args.expect_drop)
        console = '\n'.join(path.read_text(encoding='utf-8', errors='replace')
                            for path in sorted(home.rglob('*.log')) if path.is_file())
        (output / f'{name}.log').write_text(console, encoding='utf-8')
        row = dict(command=command, steps=lines, exitCode=code, forcedStop=forced, stoppedAfterDrop=stopped,
                   elapsedSeconds=elapsed,
                   wrote=re.findall(r'Wrote maps/[^\n]*\.ent \(\d+ bytes\)', console),
                   loaded=re.findall(r'Loaded entity override [^\n]*', console),
                   ignored=re.findall(r'Ignored [^\n]*\.ent[^\n]*', console),
                   marker=re.findall(MARKER + r"[^\n]*spawn function[^\n]*", console),
                   errors=[line for line in DROP.findall(console) if MARKER not in line])
        report['runs'][name] = row
        return home, row

    def ended(row):
        """Exited cleanly, or -- expected -- stopped after a drop."""
        if args.expect_drop:
            return row['stoppedAfterDrop'] and not row['forcedStop'] and bool(row['errors'])
        return row['exitCode'] == 0 and not row['forcedStop'] and not row['errors']

    common = ['set sv_pure 0', 'set bot_enable 0', 'set g_gametype 0']
    marker = ('{\n"classname" "%s"\n"origin" "0 0 0"\n}\n' % MARKER).encode('ascii')
    saved = None
    if args.from_bsp:
        lump = bsp_entities(installation, game, args.map)
        report['bsp'] = dict(bytes=len(lump), sha256=hashlib.sha256(lump).hexdigest())
        override = lump + (b'' if lump.endswith(b'\n') else b'\n') + marker
    else:
        home, saved = start('save', common + [f'map {args.map}', 'sv_saveentfile', 'quit'])
        written = home / game / 'maps' / f'{args.map}.ent'
        if written.is_file():
            text = written.read_bytes()
            report['saved'] = dict(bytes=len(text), sha256=hashlib.sha256(text).hexdigest())
            override = text + marker
    if override is not None:
        (output / 'override.ent').write_bytes(override)
        report['override'] = dict(bytes=len(override), sha256=hashlib.sha256(override).hexdigest())
    tail = [] if args.expect_drop else ['quit']
    _, loaded = start('override', common + [f'map {args.map}'] + tail)
    _, control = start('control', common + ['set sv_entityOverrides 0', f'map {args.map}'] + tail)
    report['installationUnchanged'] = before == inventory(installation)
    report['passed'] = bool(
        report['installationUnchanged'] and override is not None
        and (saved is None or (ended(saved) and saved['wrote'] and not saved['marker'] and not saved['ignored']))
        and ended(loaded) and ended(control) and not loaded['ignored'] and not control['ignored']
        and len(loaded['loaded']) == 1 and loaded['marker']
        and not control['loaded'] and not control['marker'])
    (output / 'report.json').write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
    print(args.profile, args.map, 'PASS' if report['passed'] else 'FAIL',
          {name: dict(exit=row['exitCode'], forced=row['forcedStop'], stopped=row['stoppedAfterDrop'],
                      loaded=row['loaded'], marker=row['marker'], errors=row['errors'][:2])
           for name, row in report['runs'].items()})
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
