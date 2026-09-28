#!/usr/bin/env python3
"""Load every installed map that carries bot navigation, with bots on, and record how the navigation loads.

Each map whose navigation files the installations carry -- maps/NAME.aas, or
RTCW's two hulls maps/NAME_b0.aas and maps/NAME_b1.aas -- is loaded once, in a
fresh process with an isolated home, under the profile that owns it and with
the bot library enabled:

  q3        Quake III's baseq3 and missionpack maps, dedicated server
  ql        Quake Live (`map NAME ffa`, or the first game type its map pool
            offers a map in when that is not free-for-all), dedicated server
  stvef-mp  Elite Force's Holomatch maps, dedicated server
  rtcw-mp   RTCW's mp_* maps, dedicated server
  rtcw-sp   RTCW's other maps, client (its own listen server), since the
            single-player cast needs a player to start the level

Where the game has a command to add a bot (Quake III's, Quake Live's and
Elite Force's `addbot`), one bot joins once the level runs, the level runs on,
and `status` lists the clients. Each run records what the bot library printed
about the navigation -- each file it loaded, with RTCW's area and
reachability counts, and every load error -- whether routing finished
initialising ("AAS initialized"), whether the bot was listed, and how the
process ended, classified as scripts/probe_retail_map_loads.py classifies map
loads. Run it before and after a botlib change with the same arguments, then
`--compare BEFORE AFTER` lists every map whose record changed.

Windows only: private desktops, sound, input and networking off, console
commands only. Installations are only read.

  --installation KEY=PATH  q3, ql, rtcw or stvef, as for probe_retail_map_loads.py
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
from probe_retail_map_loads import (CONFIGS, DROP, FATAL, FAULT, OUTCOMES, TITLES, discover, profile_for, sha256,  # noqa: E402
                                    stop)

NAVIGATION = ('q3', 'ql', 'rtcw', 'stvef')
# The dedicated server runs every profile but RTCW's single player.
DEDICATED = {'q3', 'ql', 'stvef-mp', 'rtcw-mp'}
# A bot each game's own bots.txt names, by profile and game folder.
BOTS = {('q3', 'baseq3'): 'sarge', ('q3', 'missionpack'): 'xaero', ('ql', 'baseq3'): 'anarki',
        ('stvef-mp', 'baseef'): 'alexandria'}
DONE = 'BOT_LOAD_PROBE_DONE'
COLOR = re.compile(r'\^[0-9A-Za-z]')
# What the bot library prints as it loads navigation, and its load errors.
LOADED = re.compile(r'^loaded (maps/\S+\.aas)(?: \(AAS8, (\d+) areas, (\d+) reachabilities\))?\s*$', re.M)
BOTLIB_ERROR = re.compile(r'^(?:Fatal|Error): (.*?)\s*$', re.M)
# A `status` row: slot, score, ping or state, name, address, rate, Steam id.
STATUS = re.compile(r'^\s*\d+\s+-?\d+\s+(?:\d+|PRM|CON|ZMB)\s+(.*?)\s+bot\s+\d+\s+\d+\s*$', re.M)


def navigation_maps(title, installation):
    """{(game dir, map name): [navigation files]} for maps whose BSP the installation carries too."""
    wanted, _ = TITLES[title]
    maps = discover(title, installation)
    found = {}
    for folder in sorted(p for p in installation.iterdir() if p.is_dir() and p.name.lower() in wanted):
        names = set()
        for pak in sorted(p for p in folder.iterdir() if p.is_file() and p.suffix.lower() == '.pk3'):
            with zipfile.ZipFile(pak) as archive:
                names.update(e.lower() for e in archive.namelist() if e.lower().startswith('maps/')
                             and e.lower().endswith('.aas'))
        loose = folder / 'maps'
        if loose.is_dir():
            names.update('maps/' + p.relative_to(loose).as_posix().lower() for p in loose.rglob('*.aas'))
        for name in names:
            base = name[5:-4]
            if title == 'rtcw':
                base = re.sub(r'_b[01]$', '', base)
            if (folder.name, base) in maps:
                found.setdefault((folder.name, base), []).append(name)
    return {key: sorted(value) for key, value in sorted(found.items())}


def quake_live_types(installation):
    """{map: game types} from Quake Live's scripts/arenas.txt: the types its map pool offers each map in."""
    types = {}
    for pak in sorted((installation / 'baseq3').glob('*.pk3')):
        with zipfile.ZipFile(pak) as archive:
            for entry in archive.namelist():
                if entry.lower() == 'scripts/arenas.txt':
                    text = archive.read(entry).decode('latin-1')
                    for block in re.findall(r'\{(.*?)\}', text, re.S):
                        name = re.search(r'map\s+"([^"]+)"', block)
                        kind = re.search(r'type\s+"([^"]+)"', block)
                        if name and kind:
                            types[name.group(1).lower()] = kind.group(1).split()
    return types


def cases(installations):
    out = []
    for title, path in sorted(installations.items()):
        # Quake Live loads a map only for a game type its map pool offers it in
        types = quake_live_types(path) if title == 'ql' else {}
        for (folder, name), files in navigation_maps(title, path).items():
            profile = profile_for(title, name)
            label = f'q3:missionpack/{name}' if folder.lower() == 'missionpack' else f'{profile}:{name}'
            offered = types.get(name, ['ffa'])
            out.append(dict(label=label, title=title, profile=profile, basepath=path, folder=folder, name=name,
                            navigation=files, bot=BOTS.get((profile, folder.lower())),
                            factory='ffa' if 'ffa' in offered else offered[0]))
    return out


def clean(text):
    return COLOR.sub('', text)


def play(case, client, server, output, timeout, extra=None):
    """One load; `extra` maps paths under the home's game folder to bytes placed there first."""
    from windows_private_process import WindowsPrivateProcess

    slug = re.sub(r'[^A-Za-z0-9_.-]+', '_', case['label'])
    home = output / 'homes' / slug
    if home.exists():
        shutil.rmtree(home)
    game = home / TITLES[case['title']][1]
    profile = case['profile']
    mod = 'missionpack' if case['folder'].lower() == 'missionpack' else ''
    for folder in (game, home / mod) if mod else (game,):
        (folder / 'configs').mkdir(parents=True, exist_ok=True)
        for name in CONFIGS:
            for suffix in ('.cfg', '_server.cfg'):
                (folder / (name + suffix)).write_bytes(b'')
    lines = [f"map {case['name']} {case['factory']}" if profile == 'ql' else f"map {case['name']}", 'wait 100']
    if case['bot']:
        lines += [f"addbot {case['bot']} 1", 'wait 300']
    else:
        lines += ['wait 300']
    lines += ['status', f'echo {DONE}', 'quit']
    (game / 'botloads.cfg').write_text(''.join(line + '\n' for line in lines), encoding='utf-8')
    for relative, data in (extra or {}).items():
        path = (home / mod if mod else game) / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    dedicated = profile in DEDICATED
    exe = server if dedicated else client
    variables = dict(com_gameProfile=profile, fs_basepath=str(case['basepath']), fs_homepath=str(home),
                     fs_game=mod, logfile='2', developer='0', sv_pure='0', bot_enable='1', net_enabled='0',
                     com_steamIntegration='0', fs_skipWorkshop='1', sv_errorExit='0', bot_developer='0')
    if dedicated:
        variables['dedicated'] = '1'
    else:
        variables.update(r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                         s_initsound='0', cl_webuiEnable='0', cl_renderer='glx', cl_allowDownload='0',
                         com_introplayed='1', cl_motd='0')
    command = [str(exe)]
    for key, value in variables.items():
        command += ['+set', key, value]
    command += ['+exec', 'botloads.cfg']
    log = (home / mod if mod else game) / 'qconsole.log'
    read = lambda: log.read_text(encoding='latin-1') if log.exists() else ''
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, exe.parent, output / 'logs' / f'{slug}-stdout.log')
    dropped_at, alive_after = None, False
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            text = read()
            if FATAL.search(text):
                break
            if dropped_at is None and DROP.search(text):
                dropped_at = time.monotonic()
            if dropped_at is not None and time.monotonic() - dropped_at >= 2:
                alive_after = process.poll() is None
                break
            time.sleep(0.1)
        forced = process.poll() is None
        code = process.poll()
    finally:
        stop(process)
    text = read()
    (output / 'logs' / f'{slug}-console.log').write_text(text, encoding='latin-1')
    shutil.rmtree(home, ignore_errors=True)
    plain = clean(text)
    errors = DROP.findall(text)
    fatal = FATAL.findall(text)
    if code == 0 and DONE in text and not errors and not fatal:
        outcome = 'loaded'
    elif fatal:
        outcome, errors = 'fatal', fatal
    elif not forced and code != 0:
        outcome = 'crashed'
    elif errors and alive_after:
        outcome = 'faulted' if FAULT in errors[0] else 'rejected'
    else:
        outcome = 'hung'
    navigation = [[m.group(1)] + [int(v) for v in m.groups()[1:] if v] for m in LOADED.finditer(plain)]
    listed = STATUS.findall(plain)
    return dict(profile=profile, map=case['name'], gameType=case['factory'] if profile == 'ql' else None,
                files=case['navigation'], outcome=outcome, exitCode=code,
                forcedTermination=forced, elapsedSeconds=round(time.monotonic() - begin, 2), errors=errors[:3],
                navigation=navigation, botlibErrors=BOTLIB_ERROR.findall(plain)[:6],
                routingInitialized=plain.count('AAS initialized'), bot=case['bot'],
                botListed=bool(case['bot']) and any(case['bot'] in name.lower() for name in listed))


def record(row):
    """What must not change between two builds, for one map."""
    return (row['outcome'], row['errors'][:1], row['navigation'], row['botlibErrors'], row['routingInitialized'] > 0,
            row['botListed'])


def compare(before_path, after_path):
    before = json.loads(Path(before_path).read_text(encoding='utf-8'))['maps']
    after = json.loads(Path(after_path).read_text(encoding='utf-8'))['maps']
    changed = 0
    for label in sorted(set(before) | set(after)):
        b, a = before.get(label), after.get(label)
        if (b and record(b)) != (a and record(a)):
            changed += 1
            print(label, 'before', b and record(b), 'after', a and record(a))
    print(f'{changed} of {len(set(before) | set(after))} maps changed')
    return 1 if changed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path)
    parser.add_argument('--server', type=Path)
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--profiles', help='comma-separated profiles to run (default: all found)')
    parser.add_argument('--only', help='comma-separated labels to run, e.g. q3:q3dm1')
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--timeout', type=float, default=150)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--resume', action='store_true', help='keep the maps an earlier run in --output finished')
    parser.add_argument('--compare', nargs=2, metavar=('BEFORE', 'AFTER'))
    args = parser.parse_args()
    if args.compare:
        return compare(*args.compare)
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    if not args.client or not args.server or not args.output:
        parser.error('--client, --server and --output are required')
    installations = {}
    for item in args.installation:
        key, _, path = item.partition('=')
        if key not in NAVIGATION or not path:
            parser.error(f'unknown installation {item!r}')
        installations[key] = Path(path).resolve(strict=True)
    client, server = args.client.resolve(strict=True), args.server.resolve(strict=True)
    todo = cases(installations)
    if args.profiles:
        wanted = set(args.profiles.split(','))
        todo = [c for c in todo if c['profile'] in wanted]
    if args.only:
        wanted = set(args.only.split(','))
        todo = [c for c in todo if c['label'] in wanted]
    output = args.output.resolve()
    (output / 'logs').mkdir(parents=True, exist_ok=args.resume)
    watched = (client, server, client.parent / 'fnql_glx_x86.dll')
    binaries = {p.name: sha256(p) for p in watched if p.exists()}
    started = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
    rows, lock = {}, threading.Lock()
    journal = output / 'rows.jsonl'
    if args.resume and journal.exists():
        for line in journal.read_text(encoding='utf-8').splitlines():
            entry = json.loads(line)
            if entry['binaries'] != binaries:
                parser.error(f"{entry['label']} ran on other binaries; start a new run")
            rows[entry['label']] = entry['row']
        todo = [c for c in todo if c['label'] not in rows]

    def one(case):
        row = play(case, client, server, output, args.timeout)
        with lock:
            rows[case['label']] = row
            with journal.open('a', encoding='utf-8') as out:
                out.write(json.dumps(dict(label=case['label'], binaries=binaries, row=row)) + '\n')
            print(case['label'], row['outcome'], row['navigation'], row['botlibErrors'][:1],
                  'routing' if row['routingInitialized'] else 'NO ROUTING', 'bot' if row['botListed'] else '-',
                  row['elapsedSeconds'], flush=True)

    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        list(pool.map(one, todo))
    after = {p.name: sha256(p) for p in watched if p.exists()}
    by_profile = {}
    for row in rows.values():
        counts = by_profile.setdefault(row['profile'], dict({o: 0 for o in OUTCOMES}, maps=0, navigationLoaded=0,
                                                            routingInitialized=0, botsListed=0, botlibErrors=0))
        counts[row['outcome']] += 1
        counts['maps'] += 1
        counts['navigationLoaded'] += len(row['navigation']) == len(row['files'])
        counts['routingInitialized'] += row['routingInitialized'] > 0
        counts['botsListed'] += row['botListed']
        counts['botlibErrors'] += bool(row['botlibErrors'])
    report = dict(binaries=binaries, binariesUnchanged=binaries == after, startedUtc=started,
                  endedUtc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                  installations={k: str(v) for k, v in installations.items()},
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip(),
                  outcomes={o: sum(r['outcome'] == o for r in rows.values()) for o in OUTCOMES},
                  profiles=by_profile, maps=dict(sorted(rows.items())))
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report['outcomes']), 'binaries unchanged' if report['binariesUnchanged'] else 'BINARIES CHANGED')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
