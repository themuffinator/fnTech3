#!/usr/bin/env python3
"""Drive the in-game entity editor through a whole edit, and show that the
game spawns what it saved.

Runs fnTech3 windowed on an unswitched private desktop with an isolated home,
mouse, joystick and sound off, and the installation read-only and inventoried
before and after. Every action is a console command from configs the probe
writes into the isolated home, chained once the map is active: open the
editor, place its camera, select an entity, make a new one, move it, undo and
redo, save the map's override, apply it (the level restarts and the server
reads maps/<map>.ent), close the editor and stand the player where the new
entity was made, capturing each step with the engine's own screenshotPNG. No
input is sent.

A second run (--malformed) writes a broken override into the home first and
shows that the server sets it aside for the BSP's entities, with the reason.

A third (--dense) stands the editor's camera above all of a map's entities,
looking down with every label on, and records what the editor's frame drew
(`entityeditor stats`): however crowded the view, the overlay keeps to its
share of the renderer's command buffer and nothing is left out. The camera
comes from the entity origins of the map's BSP in the installation's pk3s,
so it needs a Quake III-line BSP (IBSP).

The manifest records the console lines that prove each step, the saved file's
entities as the editor's reader parses them, and hashes of the captures.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

# profile: game directory, map arguments, what runs before the editor opens,
# the editor camera (x y z pitch yaw), a filter that finds an entity to
# select, the classname made, and where the player is stood afterwards
# (x y z yaw), or None where the game has no setviewpos.
PROFILES = {
    # q3dm17 has no BFG, so one in the game's view after the restart was
    # spawned from the override.
    'q3': dict(game='baseq3', map=['+devmap', 'q3dm17'], pre='', camera='100 400 420 13 55',
               find='rocketlauncher', make='weapon_bfg', place='224 504 344', stand='100 400 400 40'),
    'ql': dict(game='baseq3', map=['+devmap', 'campgrounds', 'ffa'], pre='', camera=None,
               find='rocketlauncher', make='weapon_bfg', stand=None),
    # Elsewhere the new entity is info_notnull, a point every one of these
    # games spawns and nothing else reads.
    'wolfet': dict(game='etmain', map=['+devmap', 'oasis'], pre='openlimbomenu; wait 30; ', camera=None,
                   find='spawn', make='info_notnull', stand=None),
    'rtcw-mp': dict(game='main', map=['+devmap', 'mp_beach'], pre='CloseLimboMenu; wait 30; ', camera=None,
                    find='spawn', make='info_notnull', stand=None),
    'stvef-mp': dict(game='baseef', map=['+devmap', 'hm_voy1'], pre='', camera=None,
                     find='info_player_deathmatch', make='info_notnull', stand=None),
    # forest starts in play with no briefing; RTCW's single player restarts
    # with map_restart as its multiplayer does.
    'rtcw-sp': dict(game='main', map=['+devmap', 'forest'], pre='', camera=None,
                    find='info_player_start', make='info_notnull', stand=None),
    # Elite Force's single player has no map_restart, so Apply loads the map
    # again; the firing range starts in play.
    'stvef-sp': dict(game='baseef', map=['+devmap', '_holodeck_firingrange'], pre='', camera=None,
                     find='info_player_start', make='info_notnull', stand=None),
    'jka-mp': dict(game='base', map=['+devmap', 'mp/ffa1'], pre='', camera=None,
                   find='info_player_deathmatch', make='info_notnull', stand=None),
    # a Q3Test release's own folder (.tmp/q3test/install/<release>)
    'q3test': dict(game='demoq3', map=['+devmap', 'q3test1'], pre='', camera=None,
                   find='info_player_deathmatch', make='info_notnull', stand=None),
    # Medal of Honor's multiplayer maps sit in maps/DM, so their override is
    # maps/DM/<map>.ent; script_origin is a point its game spawns.
    'mohaa': dict(game='main', map=['+devmap', 'dm/mohdm1'], pre='finishloadingscreen; wait 3000; ', camera=None,
                  find='info_player_deathmatch', make='script_origin', stand=None, milliseconds=True),
    # Ritual's engines count `wait` in milliseconds and restart a level by
    # loading it again; their configs start the level themselves.
    'alice': dict(game='base', map=['devmap', 'garden1'], pre='skipcinematic; wait 4000; ', camera=None,
                  find='info_player_start',
                  make='info_notnull', stand=None, ritual=True),
    'fakk2': dict(game='fakk', map=['devmap', 'homes1'], pre='', camera=None, find='info_player_start',
                  make='info_notnull', stand=None, ritual=True),
}
ERRORS = re.compile(r'^.*(?:ERROR:|Sys_Error|fatal crashed|recursive error|Server crashed).*$', re.M | re.I)
PROOF = {
    'opened': re.compile(r'Entity editor: \d+ entities from [^\n]*'),
    'made': re.compile(r'Entity editor: made a [^\n]*'),
    'saved': re.compile(r'Entity editor: saved [^\n]*'),
    'loaded': re.compile(r'Loaded entity override [^\n]*'),
    'ignored': re.compile(r'Ignored [^\n]*\.ent[^\n]*'),
    'restarted': re.compile(r'Entity editor: the level restarted with [^\n]*'),
    'undid': re.compile(r'Entity editor: undid [^\n]*'),
    'redid': re.compile(r'Entity editor: redid [^\n]*'),
    'listed': re.compile(r'\d+ of \d+ entities[^\n]*'),
    'wrote': re.compile(r'Wrote maps/[^\n]*\.ent \(\d+ bytes\)'),
    'stats': re.compile(r'Entity editor, last frame: [^\n]*'),
    'applying': re.compile(r'Entity editor: (?:restarting the level|the entities to spawn changed[^:]*): [^\n]*'),
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): (path.stat().st_size, path.stat().st_mtime_ns)
            for path in installation.rglob('*') if path.is_file()}


def run(command, cwd, stdout, timeout):
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
            process.wait(60)
            process.close()
    return code, forced, round(time.monotonic() - begin, 3)


def tokens(text):
    """COM_ParseExt over entity text, as code/qcommon/entity_text.hpp reads it."""
    position, out = 0, []
    while True:
        while position < len(text) and (text[position] <= ' ' or ord(text[position]) >= 0x80):
            position += 1
        if position >= len(text):
            return out
        if text.startswith('//', position):
            end = text.find('\n', position)
            position = len(text) if end < 0 else end
            continue
        if text.startswith('/*', position):
            end = text.find('*/', position + 2)
            position = len(text) if end < 0 else end + 2
            continue
        if text[position] == '"':
            end = text.find('"', position + 1)
            out.append(text[position + 1:] if end < 0 else text[position + 1:end])
            position = len(text) if end < 0 else end + 1
            continue
        start = position
        while position < len(text) and text[position] > ' ' and ord(text[position]) < 0x80:
            position += 1
        out.append(text[start:position])


def entities(text):
    stream, result = iter(tokens(text)), []
    for brace in stream:
        if not brace.startswith('{'):
            raise ValueError('found %r when expecting {' % brace)
        pairs = []
        for key in stream:
            if key.startswith('}'):
                break
            pairs.append((key, next(stream)))
        else:
            raise ValueError('end of text without a closing brace')
        result.append(pairs)
    return result


def dense_camera(installation, game, mapname):
    """The editor's camera above the centre of the map's entity origins,
    looking down, high enough for its view to hold them all; and how many
    origins there are. The BSP is the one the highest pk3 holding it has."""
    name = f'maps/{mapname}.bsp'.lower()
    data = None
    for pak in sorted((installation / game).glob('*.pk3'), reverse=True):
        with zipfile.ZipFile(pak) as archive:
            info = next((item for item in archive.infolist() if item.filename.lower() == name), None)
            if info is not None:
                data = archive.read(info)
                break
    if data is None:
        raise SystemExit(f'--dense: no {name} in the pk3s of {installation / game}')
    if data[:4] != b'IBSP':
        raise SystemExit(f'--dense reads Quake III-line BSPs (IBSP); {name} is not one')
    offset, length = struct.unpack_from('<ii', data, 8)
    text = data[offset:offset + length].split(b'\0')[0].decode('latin-1')
    origins = []
    for pairs in entities(text):
        for key, value in pairs:
            parts = value.split()
            if key == 'origin' and len(parts) >= 3:
                try:
                    origins.append(tuple(float(part) for part in parts[:3]))
                except ValueError:
                    pass
    xs, ys, zs = zip(*origins)
    span = max(max(xs) - min(xs), max(ys) - min(ys))
    return (f'{(min(xs) + max(xs)) / 2:.0f} {(min(ys) + max(ys)) / 2:.0f} {max(zs) + span * 0.55:.0f} 89 90',
            len(origins))


def steps(profile, label, shot, malformed, dense=None):
    """The configs, chained: each ends by waiting and exec'ing the next."""
    p = PROFILES[profile]
    ritual = p.get('ritual', False) or p.get('milliseconds', False)

    def wait(frames, milliseconds=None):
        # frames at 60 fps, or the milliseconds of Ritual's engines and 2015's
        return f'wait {milliseconds or frames * 17}' if ritual else f'wait {frames}'

    if dense:
        # every entity in view, every label on; then what the frame drew
        return [f'{p["pre"]}entityeditor open; {wait(30)}; set cl_entityEditorLabels 2; entityeditor camera {dense}; '
                f'{wait(60)}; {shot} {label}; {wait(20)}; entityeditor stats; {wait(5)}; quit']
    if malformed:
        # the server set the broken file aside; sv_saveentfile writes the
        # entities it runs (the BSP's) over it, and a restart loads that
        return [f'{wait(30)}; {shot} {label}-malformed; {wait(10, 400)}; sv_saveentfile; entityeditor apply; '
                f'{wait(240, 20000)}',
                f'entityeditor list; {wait(10, 400)}; quit']
    return [
        # open, frame, select
        f'{p["pre"]}entityeditor open; {wait(30)}; '
        + (f'entityeditor camera {p["camera"]}; ' if p['camera'] else '') + f'{wait(40)}; '
        f'{shot} {label}-open; {wait(10, 400)}; entityeditor find {p["find"]}; entityeditor next; {wait(30)}; '
        f'{shot} {label}-selected; {wait(10, 400)}',
        # make, set, move, undo and redo, then save
        f'entityeditor find ""; entityeditor new {p["make"]}; entityeditor set fntech3_probe 1; '
        + (f'entityeditor move {p["place"]}; ' if p.get('place') else '')
        + f'entityeditor nudge 0 0 32; {wait(20)}; entityeditor undo; entityeditor redo; entityeditor undo; '
        f'{wait(20)}; {shot} {label}-made; {wait(10, 400)}; entityeditor save; entityeditor list {p["make"]}',
        # apply: a new entity changes what spawns, so the Quake III line
        # loads the map again; the others restart as they always do
        f'entityeditor apply; {wait(240, 20000)}',
        f'{shot} {label}-applied; {wait(10, 400)}; entityeditor list fntech3_probe',
        # a value alone: the Quake III line's map_restart respawns it
        f'entityeditor find fntech3_probe; entityeditor next; entityeditor nudge 0 0 8; entityeditor apply; '
        f'{wait(240, 20000)}',
        f'entityeditor find ""; entityeditor list fntech3_probe; {wait(10, 400)}',
        # the game's own view of what it spawned
        ('entityeditor close; ' + (f'setviewpos {p["stand"]}; ' if p['stand'] else '')
         + f'{wait(90)}; {shot} {label}-game; {wait(10, 400)}; quit'),
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--renderer', default='glx')
    parser.add_argument('--width', type=int, default=1600)
    parser.add_argument('--height', type=int, default=900)
    parser.add_argument('--timeout', type=int, default=240)
    parser.add_argument('--set', action='append', default=[], metavar='NAME=VALUE',
                        help='an extra startup cvar, set after the probe ones')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--malformed', action='store_true',
                      help='write a broken override first and show the server sets it aside')
    mode.add_argument('--dense', action='store_true',
                      help="view every entity from above with every label on and report the editor's frame")
    args = parser.parse_args()
    p = PROFILES[args.profile]
    installation = args.installation.resolve(strict=True)
    executable = args.executable.resolve(strict=True)
    executable.relative_to(ROOT)
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    game = p['game']
    (home / game / 'maps').mkdir(parents=True)
    before = inventory(installation)
    mapname = p['map'][1].lstrip('+')
    override = home / game / 'maps' / f'{mapname}.ent'
    override.parent.mkdir(parents=True, exist_ok=True)
    if args.malformed:
        override.write_text('{\n"classname" "worldspawn"\n"message" "never closed"\n', encoding='utf-8')

    camera, origins = dense_camera(installation, game, mapname) if args.dense else (None, 0)
    label = f'ee-{args.profile}' + ('-malformed' if args.malformed else '-dense' if args.dense else '')
    configs = steps(args.profile, label, 'screenshotPNG', args.malformed, camera)
    for index, body in enumerate(configs):
        chained = body if index + 1 == len(configs) else f'{body}; wait 5; exec ee-step{index + 2}.cfg'
        (home / game / f'ee-step{index + 1}.cfg').write_text(chained + '\n', encoding='utf-8')
    values = dict(cl_allowDownload='0', com_introplayed='1', activeAction='wait 60; exec ee-step1.cfg',
                  sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_motd='0', com_maxfps='60',
                  com_maxfpsUnfocused='60', com_maxfpsMinimized='60')
    if p.get('ritual'):
        # no activeAction there: the config loads the level, then waits for it
        del values['activeAction']
    config = ''.join(f'set {name} "{value}"\n' for name, value in values.items())
    if p.get('ritual'):
        config += ' '.join(p['map']) + '\nwait 12000\nexec ee-step1.cfg\n'
    (home / game / 'ee-probe.cfg').write_text(config, encoding='utf-8')
    startup = dict(com_gameProfile=args.profile, fs_basepath=str(installation), fs_homepath=str(home),
                   fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1', r_customWidth=str(args.width),
                   r_customHeight=str(args.height), in_mouse='0', in_joystick='0', in_nograb='1',
                   s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                   cl_renderer=args.renderer, logfile='2', net_enabled='0', com_recommendedSet='1')
    if args.profile == 'wolfet':
        startup.update(com_hunkMegs='256', sv_pure='0')
    if args.profile == 'stvef-sp':
        # its screenshots need a larger hunk than its default
        startup.update(com_hunkMegs='128')
    for pair in args.set:
        name, _, value = pair.partition('=')
        startup[name] = value
    command = [str(executable)]
    for name, value in startup.items():
        command += ['+set', name, value]
    command += ['+exec', 'ee-probe.cfg'] + ([] if p.get('ritual') else p['map'])
    for index, body in enumerate(configs):
        (output / f'ee-step{index + 1}.cfg').write_text(body + '\n', encoding='utf-8')
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  profile=args.profile, executable=str(executable), executableSha256=sha(executable),
                  renderer=args.renderer, installation=str(installation), width=args.width, height=args.height,
                  command=command, steps=configs, malformed=args.malformed, screenshots=[])
    if args.dense:
        report['dense'] = dict(camera=camera, origins=origins)
    renderer = executable.parent / f'fnql_{args.renderer}_x86.dll'
    if renderer.is_file():
        report['rendererSha256'] = sha(renderer)
    code, forced, elapsed = run(command, executable.parent, output / 'stdout.log', args.timeout)
    report.update(exitCode=code, forcedStop=forced, elapsedSeconds=elapsed)
    logs = [path for path in home.rglob('*.log') if path.is_file()]
    text = '\n'.join(path.read_text(encoding='utf-8', errors='replace') for path in logs)
    (output / 'console.log').write_text(text, encoding='utf-8')
    for capture in sorted(path for path in home.rglob('*.png') if 'screenshots' in path.parts):
        destination = output / capture.name
        shutil.copy2(capture, destination)
        report['screenshots'].append(dict(name=capture.name, sha256=sha(destination)))
    report['proof'] = {name: pattern.findall(text)[:6] for name, pattern in PROOF.items()}
    report['errors'] = ERRORS.findall(text)
    if override.is_file():
        saved = override.read_text(encoding='utf-8', errors='replace')
        shutil.copy2(override, output / override.name)
        report['override'] = dict(path=str(override.relative_to(home)), bytes=len(saved.encode('utf-8')),
                                  sha256=sha(override))
        try:
            parsed = entities(saved)
            classnames = [dict(pairs).get('classname', '') for pairs in parsed]
            report['override'].update(entities=len(parsed), first=classnames[0] if classnames else None,
                                      made=classnames.count(p['make']),
                                      probeKeys=sum(1 for pairs in parsed if ('fntech3_probe', '1') in pairs))
        except (ValueError, StopIteration) as error:
            report['override']['parseError'] = str(error)
    shutil.rmtree(home)
    report['installationUnchanged'] = before == inventory(installation)
    proof = report['proof']
    if args.dense:
        # the frame drew everything it was given, and the capture was written
        report['passed'] = (code == 0 and not forced and not report['errors'] and report['installationUnchanged']
                            and bool(proof['stats']) and ', 0 KiB left out' in proof['stats'][0]
                            and len(report['screenshots']) >= 1)
    elif args.malformed:
        # set aside first, then written whole and loaded on the restart
        report['passed'] = (code == 0 and not forced and not report['errors'] and report['installationUnchanged']
                            and bool(proof['ignored']) and bool(proof['wrote']) and len(proof['loaded']) == 1
                            and text.index(proof['ignored'][0]) < text.index(proof['loaded'][0])
                            and report.get('override', {}).get('first') == 'worldspawn')
    else:
        # the Quake III line loads the map again for the new entity and
        # restarts it with map_restart for the nudge alone
        applying = [line.rstrip() for line in proof['applying']]
        quake3_line = not (p.get('ritual') or args.profile in ('mohaa', 'stvef-sp'))
        restarts = len(applying) >= 2 and (not quake3_line or (
            applying[0].startswith('Entity editor: the entities to spawn changed')
            and applying[1].endswith(': map_restart 0')))
        report['passed'] = (code == 0 and not forced and not report['errors'] and report['installationUnchanged']
                            and all(proof[name] for name in ('opened', 'made', 'saved', 'loaded', 'restarted',
                                                             'undid', 'redid'))
                            and len(proof['loaded']) >= 2 and len(proof['restarted']) >= 2 and restarts
                            and report.get('override', {}).get('probeKeys') == 1
                            and len(report['screenshots']) >= 5)
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(args.profile, 'PASS' if report['passed'] else 'FAIL', 'exit', code, 'forced', forced,
          'shots', len(report['screenshots']), 'errors', report['errors'][:3], flush=True)
    if not report['installationUnchanged']:
        raise RuntimeError('Installation file metadata changed during the probe')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
