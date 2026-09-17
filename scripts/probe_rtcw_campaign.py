#!/usr/bin/env python3
"""Launch every installed official RTCW map separately, with engine PNGs and saves.

SP startup and --check-mp-match require a private client from prepare_rtcw_campaign_probe.py.
Basic MP captures and fresh-process save loading use an ordinary client. Installations
remain read-only, runs are windowed on an unswitched desktop. No device input.
The private hook may click only the SP briefing Ready button (AGENTS.md).
"""
from pathlib import Path
import argparse
import datetime
import json
import re
import shutil
import struct
import subprocess
import time
import zipfile
import probe_rtcw_boot as boot

ROOT = Path(__file__).resolve().parents[1]

def maps(installation):
    result = {}
    arenas = {}
    for pak in sorted((installation / 'Main').glob('*.pk3')):
        if not re.fullmatch(r'(?:pak0|sp_pak\d+|mp_pak\d+|mp_pakmaps\d+)\.pk3', pak.name, re.I):
            continue
        with zipfile.ZipFile(pak) as z:
            for item in z.infolist():
                if re.fullmatch(r'maps/[\w-]+\.bsp', item.filename, re.I):
                    name = Path(item.filename).stem
                    result[name] = dict(map=name, mode='mp' if name.startswith('mp_') else 'sp',
                        pak=pak.name, crc32=f'{item.CRC:08x}', bytes=item.file_size)
                elif item.filename.lower().endswith('.arena') and pak.name.lower().startswith('mp_'):
                    arenas[item.filename.lower()] = z.read(item).decode('utf-8', errors='replace')
    for contents in arenas.values():
        for block in re.findall(r'\{([^{}]*)\}', contents):
            name = re.search(r'\bmap\s+"?([\w-]+)', block)
            kinds = re.search(r'\btype\s+"([^"]+)"', block)
            if name and kinds and name[1] in result:
                types = kinds[1].split()
                row = result[name[1]]
                row['arenaTypes'] = types
                # Original MP bg_public.h: objective=5, stopwatch=6, checkpoint=7.
                # Destruction and Trench Toast advertise checkpoint only.
                row['gameType'] = next((number for kind, number in
                    [('wolfmp', 5), ('wolfcp', 7), ('wolfsw', 6)] if kind in types), 5)
    return [result[name] for name in sorted(result)]

def run(args, item, output):
    folder = output / item['map']; folder.mkdir()
    home = folder / 'home'; game = home / 'main'; game.mkdir(parents=True)
    sp = item['mode'] == 'sp'
    cold = args.load_saves_from is not None
    dwell = args.dwell or (90 if item['map'] == 'escape1' else 20)
    saving = not cold and sp and not item['map'].startswith('cutscene') and item['map'] != 'end'
    if cold:
        if not sp: raise ValueError('Save loading is an SP operation')
        source = args.load_saves_from / item['map'] / 'saves/campaign.svg'
        (game / 'save').mkdir()
        shutil.copyfile(source, game / 'save/campaign.svg')
    values = dict(developer='1', com_introplayed='1', com_recommendedSet='1',
        com_maxfps='60', com_maxfpsUnfocused='60', com_maxfpsMinimized='60',
        sv_autoRecordDemos='0', cl_autoRecordDemos='0', cl_allowDownload='0', cl_motd='0',
        s_backend='openal', s_muteWhenUnfocused='1', s_muteWhenMinimized='1',
        r_csmShadows='0', r_picmip='0', r_picmip2='0', con_notifytime='0',
        rtcw_probe_dwell=str(dwell), rtcw_probe_saveload=str(int(saving)),
        rtcw_probe_guard=str(int(args.check_cinematic_guard)),
        activeAction='fntech3_campaign_start')
    if cold:
        values['activeAction'] = ('wait 600; echo CAMPAIGN cold-load-complete; savegame_loading; '
            'mapname; savegame campaign_cold; screenshotPNG campaign-cold; wait 5; quit')
    elif not sp:
        values['g_gametype'] = str(item.get('gameType', 5))
        if args.check_mp_match:
            values['g_warmup'] = '5'
        else:
            values['activeAction'] = ('wait 180; CloseLimboMenu; -scores; wait 120; '
                'echo CAMPAIGN multiplayer-world; status; screenshotPNG campaign-before; wait 5; quit')
    for i in range(1, 6): values[f'sv_master{i}'] = ''
    config = ''.join(f'set {key} "{value}"\n' for key, value in values.items())
    (game / 'autoexec.cfg').write_text(config)
    (folder / 'autoexec.cfg').write_text(config)
    startup = dict(com_gameProfile='rtcw-' + item['mode'], fs_basepath=str(args.installation),
        fs_homepath=str(home), fs_game='', fs_cdpath='', r_fullscreen='0', r_windowedMode='-1',
        r_windowedWidth='800', r_windowedHeight='600', in_mouse='0', in_joystick='0', in_nograb='1',
        s_initsound='1', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
        cl_renderer='glx', logfile='4', net_ip='127.0.0.1', net_port='0', net_ip6='::1', net_port6='0')
    command = [str(args.executable)]
    for key, value in startup.items(): command += ['+set', key, value]
    command += ['+loadgame', 'campaign'] if cold else ['+spdevmap' if sp else '+devmap', item['map']]
    assert len(subprocess.list2cmdline(command[1:]).encode()) < 1024
    row = dict(item, command=command, savingExpected=saving, dwellSeconds=dwell, screenshots=[])
    row['coldLoad'] = cold
    process = None
    try:
        process = boot.WindowsPrivateProcess(command, args.executable.parent, folder / 'stdout.log')
        row['exitCode'] = boot.wait_for_boot(process, game / 'qconsole.log', command, dwell + (240 if saving else 60))
    except Exception as error:
        row.update(error=str(error), errorType=type(error).__name__)
    finally:
        if process:
            row['forcedStop'] = process.poll() is None; process.close()
        log = (game / 'qconsole.log').read_text(errors='replace') if (game / 'qconsole.log').exists() else ''
        (folder / 'console.log').write_text(log, encoding='utf-8')
        for capture in home.rglob('campaign-*.png'):
            dest = folder / capture.name; shutil.copy2(capture, dest)
            row['screenshots'].append(dict(path=str(dest), sha256=boot.sha(dest)))
        row['saves'] = []
        for save in home.rglob('*.svg'):
            data = save.read_bytes()
            header = dict(version=struct.unpack_from('<i', data)[0],
                map=data[4:68].split(b'\0')[0].decode(errors='replace'),
                time=struct.unpack_from('<i', data, 68)[0],
                episode=struct.unpack_from('<i', data, 76)[0]) if len(data) >= 80 else None
            row['saves'].append(dict(path=str(save.relative_to(home)), bytes=len(data),
                sha256=boot.sha(save), header=header))
        row['observations'] = [line for line in log.splitlines() if line.startswith('CAMPAIGN')]
        row['engineErrors'] = [line for line in log.splitlines() if re.search(
            r'error:|sys_error|fatal crashed:|not implemented|unsupported rtcw|rtcw[^\n]*unsupported|server command overflow|lost reliable commands|recursive cl_shutdown', line, re.I)]
        row['warnings'] = [line for line in log.splitlines() if re.search(r'warning:|couldn.t|can.t find|failed|invalid|not found', line, re.I)]
        row['aas'] = [line for line in log.splitlines() if re.search(r'AAS|routing cache', line)]
        if sp:
            row['aasInitialized'] = all(f'RTCW routing cache loaded: maps/{item["map"]}_b{i}.rcd' in log for i in range(2)) and log.count('AAS initialized.') >= 2
        row['windowedObserved'] = '800 x 600 windowed' in log
        row['passed'] = row.get('exitCode') == 0 and not row.get('forcedStop') and row['windowedObserved'] and not row['engineErrors'] and bool(row['screenshots'])
        if sp: row['passed'] &= row['aasInitialized']
        if args.check_mp_match and not sp:
            samples = re.findall(r'CAMPAIGN mp-match phase=match-(?:running|complete) gamestate=(\d+) time=(\d+) commandTime=(\d+)', log)
            row['matchAdvanced'] = (len(samples) == 2 and all(int(s[0]) == 0 for s in samples) and
                int(samples[1][1]) > int(samples[0][1]) + 2000 and
                int(samples[1][2]) > int(samples[0][2]) + 2000 and
                ' : map_restart' in log)
            row['passed'] &= row['matchAdvanced']
        if saving:
            row['saveLoadPassed'] = ("G_LoadGame '(null)'" in log and any(s['path'].replace('\\', '/') == 'main/save/campaign.svg' for s in row['saves']) and
                any(s['path'].replace('\\', '/') == 'main/save/campaign_after.svg' for s in row['saves']) and
                any('after-load' in s and 'state=8 valid=1' in s and 'loading=0' in s for s in row['observations']))
            row['passed'] &= row['saveLoadPassed']
        if cold:
            source_episode = struct.unpack_from('<i', source.read_bytes(), 76)[0]
            row['coldLoadPassed'] = ("G_LoadGame '(null)'" in log and
                any(s['path'].replace('\\', '/') == 'main/save/campaign_cold.svg' and
                    s['header'] and s['header']['map'] == item['map'] and
                    s['header']['episode'] == source_episode for s in row['saves']) and
                bool(re.search(r'"savegame_loading" is:"0', log)))
            row['passed'] &= row['coldLoadPassed']
        if args.check_cinematic_guard:
            row['cinematicSaveRefused'] = ('CAMPAIGN checking cinematic save refusal' in log and
                'Cannot save RTCW during a cinematic' in log and
                not any(s['path'].replace('\\', '/').endswith('/campaign_cinematic.svg') for s in row['saves']))
            row['passed'] &= row['cinematicSaveRefused']
        # Keep saves only on explicit request for a subsequent independent load.
        if args.keep_saves:
            saved = folder / 'saves'; saved.mkdir()
            for save in home.rglob('*.svg'):
                dest = saved / save.relative_to(game / 'save'); dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(save, dest)
        boot.remove_home(home, output)
        (folder / 'manifest.json').write_text(json.dumps(row, indent=2) + '\n')
    print(item['map'], 'PASS' if row['passed'] else 'FAIL', row.get('error', ''), flush=True)
    return row

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--maps', nargs='+')
    parser.add_argument('--dwell', type=int)
    parser.add_argument('--keep-saves', action='store_true')
    parser.add_argument('--check-mp-match', action='store_true',
                        help='Use the private probe to verify warmup restart and advancing match/usercmd time')
    parser.add_argument('--check-cinematic-guard', action='store_true',
                        help='Require a save attempt during an opening cinematic to be refused')
    parser.add_argument('--load-saves-from', type=Path,
                        help='Load each saved campaign.svg in a fresh ordinary client process')
    args = parser.parse_args()
    args.executable = args.executable.resolve(strict=True); args.executable.relative_to(ROOT)
    args.installation = args.installation.resolve(strict=True)
    output = args.output.resolve(); output.relative_to(ROOT / '.tmp'); output.mkdir(parents=True, exist_ok=False)
    if args.dwell is not None and not 3 <= args.dwell <= 180: parser.error('dwell must be 3–180 seconds')
    inventory = boot.inventory(args.installation)
    items = maps(args.installation)
    if args.maps:
        unknown = set(args.maps) - {r['map'] for r in items}
        if unknown: parser.error('Unknown official maps: ' + ', '.join(sorted(unknown)))
        items = [r for r in items if r['map'] in args.maps]
    if args.load_saves_from:
        args.load_saves_from = args.load_saves_from.resolve(strict=True)
        args.load_saves_from.relative_to(ROOT / '.tmp')
        items = [r for r in items if (args.load_saves_from / r['map'] / 'saves/campaign.svg').is_file()]
    if not items:
        parser.error('No official maps with the required inputs were selected')
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        executable=str(args.executable), executableSha256=boot.sha(args.executable),
        installation=str(args.installation), mapInventory=items, runs=[])
    for item in items:
        if (output / 'stop').exists(): break
        report['runs'].append(run(args, item, output))
        report['installationUnchanged'] = inventory == boot.inventory(args.installation)
        (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        if not report['installationUnchanged']: raise RuntimeError('Installation metadata changed')
    return 0 if all(row['passed'] for row in report['runs']) else 1

if __name__ == '__main__':
    raise SystemExit(main())
