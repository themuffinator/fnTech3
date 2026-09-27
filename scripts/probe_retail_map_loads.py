#!/usr/bin/env python3
"""Load every installed map of every map-loading profile and classify each load.

Each map in an installation's own paks (and loose `maps/` files) is loaded once,
in a fresh process with an isolated home, under the profile that owns it:

  q3        Quake III's baseq3 and missionpack maps (missionpack under fs_game)
  ql        Quake Live (`map NAME ffa`)
  rtcw-sp   RTCW maps not named mp_*; rtcw-mp the mp_* ones
  wolfet    Enemy Territory
  stvef-sp  Elite Force maps not named hm_* or ctf_*; stvef-mp those
  q3-ihv    the IHV test's maps, on the dedicated server
  jka-sp    Jedi Academy maps outside maps/mp/; jka-mp those, dedicated server
  jk2-sp    Jedi Outcast maps not named ctf_*, duel_* or ffa_*; jk2-mp those,
            dedicated server
  alice, fakk2, mohaa
  q3test    each Q3Test release's maps, from that release's own folder; 1.02's
            and 1.03's on the dedicated server, since they shipped no Win32
            client game

A client loads the map with `map`, which loads its collision and spawns the
level, draws it for a while and quits; a dedicated server loads it and quits.
Each home carries empty configurations under every profile's names, so an
installation's own configuration (the Steam Quake III one sets a 56 MB hunk
under missionpack) cannot reach the load.
Each run ends in one of: `loaded` (the process quit normally after the map was
up and printed no engine error), `rejected` (the engine printed an ERR_DROP and
was still running two seconds later), `faulted` (the same, but the error is the
engine's own report of a hardware exception it caught), `fatal` (an ERR_FATAL,
whose dialog a private desktop cannot dismiss, so the run is stopped at once),
`crashed` (the process died with a failure status) or `hung` (none of these
within the time limit). The Jedi single-player modes refuse their module
boundary by name once the map's collision has loaded, so that refusal is how
their maps end.

Run it before and after a loader change with the same arguments, then
`--compare BEFORE AFTER` lists every map whose outcome or first error changed.
Each finished map is also appended to `rows.jsonl`, and `--resume` continues
an interrupted run in the same output folder.
Windows only: windowed on unswitched private desktops, sound, mouse and
joystick input off, networking off, console commands only. Installations are
only read.

  --installation KEY=PATH  one per title: q3, ql, rtcw, wolfet, stvef, q3-ihv,
                           jka, jk2, alice, fakk2, mohaa, and q3test (a folder
                           holding one folder per release)
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import hashlib
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
sys.path.insert(0, str(ROOT / 'tests'))

# title: (game directories searched for maps, the profile's own game directory)
TITLES = {
    'q3': (('baseq3', 'missionpack'), 'baseq3'),
    'ql': (('baseq3',), 'baseq3'),
    'rtcw': (('main',), 'main'),
    'wolfet': (('etmain',), 'etmain'),
    'stvef': (('baseef',), 'baseEF'),
    'q3-ihv': (('baseq3',), 'baseq3'),
    'jka': (('base',), 'base'),
    'jk2': (('base',), 'base'),
    'alice': (('base',), 'base'),
    'fakk2': (('fakk',), 'fakk'),
    'mohaa': (('main',), 'main'),
    'q3test': (('demoq3',), 'demoq3'),
}
DEDICATED = {'q3-ihv', 'jka-sp', 'jka-mp', 'jk2-sp', 'jk2-mp'}
# Q3Test 1.02 and 1.03 shipped no Win32 client game, so only a server loads their maps.
DEDICATED_Q3TEST = {'1.02', '1.03'}
# Their `wait N` counts milliseconds, as their retail engines' do.
MILLISECOND_WAITS = {'alice', 'fakk2', 'mohaa'}
# Every profile's configuration names. An empty one in the home shadows the
# installation's own, which the search path would otherwise find and run.
CONFIGS = ('q3config', 'qzconfig', 'wolfconfig', 'wolfconfig_mp', 'etconfig', 'efconfig', 'hmconfig', 'jaconfig',
           'jampconfig', 'jk2config', 'jk2mpconfig', 'config', 'configs/unnamedsoldier')
LOADED = 'MAP_LOAD_PROBE_DONE'
# Com_Error prints an ERR_DROP between two rows of asterisks; a module's own
# line that merely starts with "ERROR:" is not one.
DROP = re.compile(r'^\*{20}\r?\nERROR: (.*?)\s*^\*{20}', re.M | re.S)
# An ERR_FATAL shuts the server down with this reason before its dialog.
FATAL = re.compile(r'fatal crashed: (.*?)\s*\) -----', re.S)
FAULT = 'Unhandled exception caught'
SURFACES = re.compile(r'\.\.\.loaded (\d+) faces, (\d+) meshes, (\d+) trisurfs, (\d+) flares')
OUTCOMES = ('loaded', 'rejected', 'faulted', 'fatal', 'crashed', 'hung')


def profile_for(title, name):
    base = name.rsplit('/', 1)[-1]
    if title == 'rtcw':
        return 'rtcw-mp' if base.startswith('mp_') else 'rtcw-sp'
    if title == 'stvef':
        return 'stvef-mp' if base.startswith(('hm_', 'ctf_')) else 'stvef-sp'
    if title == 'jka':
        return 'jka-mp' if name.startswith('mp/') else 'jka-sp'
    if title == 'jk2':
        return 'jk2-mp' if base.startswith(('ctf_', 'duel_', 'ffa_')) else 'jk2-sp'
    return title


def discover(title, installation):
    """Every map the installation carries: {(game dir, map name): source}, later paks winning."""
    found = {}
    wanted, _ = TITLES[title]
    for folder in sorted(p for p in installation.iterdir() if p.is_dir() and p.name.lower() in wanted):
        paks = sorted((p for p in folder.iterdir() if p.is_file() and p.suffix.lower() == '.pk3'),
                      key=lambda p: p.name.lower())
        for pak in paks:
            with zipfile.ZipFile(pak) as archive:
                for entry in archive.namelist():
                    key = entry.lower()
                    if key.startswith('maps/') and key.endswith('.bsp'):
                        found[(folder.name, key[5:-4])] = f'{pak.name}:{entry}'
        maps = folder / 'maps'
        if maps.is_dir():
            for path in sorted(maps.rglob('*')):
                if path.is_file() and path.suffix.lower() == '.bsp':
                    name = path.relative_to(maps).as_posix().lower()[:-4]
                    found[(folder.name, name)] = f'loose:maps/{path.relative_to(maps).as_posix()}'
    return found


def cases(installations):
    out = []
    for title, path in sorted(installations.items()):
        roots = sorted(p for p in path.iterdir() if p.is_dir()) if title == 'q3test' else [path]
        for root in roots:
            if title == 'q3test' and not any(p.name.lower() == 'demoq3' for p in root.iterdir()):
                continue
            for (folder, name), source in sorted(discover(title, root).items()):
                profile = profile_for(title, name)
                label = f'{profile}:{name}' if title != 'q3test' else f'q3test-{root.name}:{name}'
                if title == 'q3' and folder.lower() == 'missionpack':
                    label = f'q3:missionpack/{name}'
                out.append(dict(label=label, title=title, profile=profile, basepath=root, folder=folder,
                                name=name, source=source))
    return out


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stop(process):
    # close() races a child that exits between its poll and its terminate
    # (PermissionError); by the next attempt the child has gone.
    for attempt in range(3):
        try:
            process.close()
            return
        except subprocess.TimeoutExpired:
            process.wait(60)
        except PermissionError:
            time.sleep(0.5)
    process.close()


def play(case, client, server, renderer, output, timeout):
    from windows_private_process import WindowsPrivateProcess

    slug = re.sub(r'[^A-Za-z0-9_.-]+', '_', case['label'])
    home = output / 'homes' / slug
    if home.exists():
        shutil.rmtree(home)
    game = home / TITLES[case['title']][1]
    profile = case['profile']
    mod = 'missionpack' if case['label'].startswith('q3:missionpack/') else ''
    for folder in (game, home / mod) if mod else (game,):
        (folder / 'configs').mkdir(parents=True, exist_ok=True)
        for name in CONFIGS:
            for suffix in ('.cfg', '_server.cfg'):
                (folder / (name + suffix)).write_bytes(b'')
    load = f"map {case['name']} ffa" if profile == 'ql' else f"map {case['name']}"
    settle = 'wait 6000' if profile in MILLISECOND_WAITS else 'wait 120'
    (game / 'maploads.cfg').write_text(f'{load}\n{settle}\necho {LOADED}\nquit\n', encoding='utf-8')
    dedicated = profile in DEDICATED or (profile == 'q3test' and case['basepath'].name in DEDICATED_Q3TEST)
    exe = server if dedicated else client
    variables = dict(com_gameProfile=profile, fs_basepath=str(case['basepath']), fs_homepath=str(home),
                     fs_game=mod, logfile='2', developer='0', sv_pure='0', bot_enable='0', net_enabled='0',
                     com_steamIntegration='0', fs_skipWorkshop='1', sv_errorExit='0')
    if dedicated:
        variables['dedicated'] = '1'
    else:
        variables.update(r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                         s_initsound='0', cl_webuiEnable='0', cl_renderer=renderer, cl_allowDownload='0',
                         com_introplayed='1', cl_motd='0')
    command = [str(exe)]
    for key, value in variables.items():
        command += ['+set', key, value]
    command += ['+exec', 'maploads.cfg']
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
    errors = DROP.findall(text)
    fatal = FATAL.findall(text)
    if code == 0 and LOADED in text and not errors and not fatal:
        outcome = 'loaded'
    elif fatal:
        outcome, errors = 'fatal', fatal
    elif not forced and code != 0:
        outcome = 'crashed'
    elif errors and alive_after:
        outcome = 'faulted' if FAULT in errors[0] else 'rejected'
    else:
        outcome = 'hung'
    surfaces = SURFACES.search(text)
    return dict(profile=profile, map=case['name'], source=case['source'], outcome=outcome, exitCode=code,
                forcedTermination=forced, elapsedSeconds=round(time.monotonic() - begin, 2),
                errors=errors[:3], worldSurfaces=[int(x) for x in surfaces.groups()] if surfaces else None)


def compare(before_path, after_path):
    before = json.loads(Path(before_path).read_text(encoding='utf-8'))['maps']
    after = json.loads(Path(after_path).read_text(encoding='utf-8'))['maps']
    changed = 0
    for label in sorted(set(before) | set(after)):
        b, a = before.get(label), after.get(label)
        key = lambda r: None if r is None else (r['outcome'], r['errors'][:1], r['worldSurfaces'])
        if key(b) != key(a):
            changed += 1
            print(label, 'before', key(b), 'after', key(a))
    print(f'{changed} of {len(set(before) | set(after))} maps changed')
    return 1 if changed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path)
    parser.add_argument('--server', type=Path)
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--renderer', default='glx', choices=('glx', 'vk', 'rtx'))
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
        if key not in TITLES or not path:
            parser.error(f'unknown installation {item!r}')
        installations[key] = Path(path).resolve(strict=True)
    client, server = args.client.resolve(strict=True), args.server.resolve(strict=True)
    todo = cases(installations)
    if args.profiles:
        wanted = set(args.profiles.split(','))
        todo = [c for c in todo if c['profile'] in wanted or c['label'].split(':')[0] in wanted]
    if args.only:
        wanted = set(args.only.split(','))
        todo = [c for c in todo if c['label'] in wanted]
    output = args.output.resolve()
    (output / 'logs').mkdir(parents=True, exist_ok=args.resume)
    binaries = {p.name: sha256(p) for p in (client, server, client.parent / f'fnql_{args.renderer}_x86.dll')
                if p.exists()}
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
        row = play(case, client, server, args.renderer, output, args.timeout)
        with lock:
            rows[case['label']] = row
            with journal.open('a', encoding='utf-8') as out:
                out.write(json.dumps(dict(label=case['label'], binaries=binaries, row=row)) + '\n')
            print(case['label'], row['outcome'], row['errors'][:1], row['elapsedSeconds'], flush=True)

    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        list(pool.map(one, todo))
    after = {p.name: sha256(p) for p in (client, server, client.parent / f'fnql_{args.renderer}_x86.dll')
             if p.exists()}
    by_profile = {}
    for row in rows.values():
        counts = by_profile.setdefault(row['profile'], {o: 0 for o in OUTCOMES})
        counts[row['outcome']] += 1
    report = dict(renderer=args.renderer, binaries=binaries, binariesUnchanged=binaries == after,
                  startedUtc=started, endedUtc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
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
