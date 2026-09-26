#!/usr/bin/env python3
"""Exercise RTCW saves in isolated, windowed ordinary clients without input.

Inputs must be local saves produced by a legitimate installation. The game
module owns serialization. This probe checks repeat, cross-map, malformed-header,
autosave and original-engine rewrite paths; it does not inspect private payloads.
"""
from pathlib import Path
import argparse
import datetime
import json
import re
import shutil
import struct
import subprocess
import probe_rtcw_boot as boot

ROOT = Path(__file__).resolve().parents[1]


def header(path):
    data = path.read_bytes()
    return dict(bytes=len(data), sha256=boot.sha(path), version=struct.unpack_from('<i', data)[0],
                map=data[4:68].split(b'\0')[0].decode('ascii'), time=struct.unpack_from('<i', data, 68)[0],
                episode=struct.unpack_from('<i', data, 76)[0])


def run(args, case, source, action, expected_loads, target=None):
    folder = args.output / case
    game = folder / 'home/main'
    game.mkdir(parents=True)
    home = game.parent
    saved = game / 'save'
    saved.mkdir()
    shutil.copyfile(source, saved / 'campaign.svg')
    source_header = header(source)
    expected_map = header(target)['map'] if target else source_header['map']
    expected_episode = header(target)['episode'] if target else source_header['episode']
    if target:
        shutil.copyfile(target, saved / 'destination.svg')
    if case == 'invalid-headers':
        (saved / 'truncated.svg').write_bytes(b'\x12\0\0\0escape2')
        invalid = bytearray((saved / 'campaign.svg').read_bytes())
        invalid[:4] = struct.pack('<i', 999)
        (saved / 'badversion.svg').write_bytes(invalid)
    values = dict(developer='1', com_introplayed='1', com_recommendedSet='1', com_maxfps='60',
                  com_maxfpsUnfocused='60', com_maxfpsMinimized='60', cl_motd='0',
                  cl_allowDownload='0', r_picmip='0', r_picmip2='0', r_csmShadows='0',
                  con_notifytime='0', sv_autoRecordDemos='0', cl_autoRecordDemos='0',
                  s_backend='openal', s_muteWhenUnfocused='1', s_muteWhenMinimized='1')
    values.update({f'sv_master{i}': '' for i in range(1, 6)})
    (game / 'autoexec.cfg').write_text(''.join(f'set {k} "{v}"\n' for k, v in values.items()))
    (game / 'run.cfg').write_text('set activeAction "exec cycle-start.cfg"\n')
    final = ('wait 300; echo SAVE_CYCLE complete; savegame_loading; mapname; viewpos; '
             'savegame final; screenshotPNG save-cycle; wait 5; quit\n')
    (game / 'cycle-start.cfg').write_text(action + ('\n' if target else final))
    if target:
        (game / 'cycle-finish.cfg').write_text(final)
    for name in ('autoexec.cfg', 'run.cfg', 'cycle-start.cfg', 'cycle-finish.cfg'):
        if (game / name).exists():
            shutil.copyfile(game / name, folder / name)
    startup = dict(com_gameProfile='rtcw-sp', fs_basepath=str(args.installation), fs_homepath=str(home),
                   fs_game='', fs_cdpath='', r_fullscreen='0', r_mode='-1',
                   r_customWidth='800', r_customHeight='600', in_mouse='0', in_joystick='0',
                   in_nograb='1', s_initsound='1', com_steamIntegration='0', fs_skipWorkshop='1',
                   cl_webuiEnable='0', cl_renderer='glx', logfile='4', net_ip='127.0.0.1',
                   net_port='0', net_ip6='::1', net_port6='0')
    command = [str(args.executable)]
    for key, value in startup.items():
        command += ['+set', key, value]
    command += ['+exec', 'run.cfg', '+loadgame', 'campaign']
    assert len(subprocess.list2cmdline(command[1:]).encode()) < 1024
    row = dict(case=case, command=command, source=str(source), sourceHeader=source_header,
               expectedMap=expected_map, expectedLoads=expected_loads, inputEvents=0)
    process = None
    try:
        process = boot.WindowsPrivateProcess(command, args.executable.parent, folder / 'stdout.log')
        row['exitCode'] = boot.wait_for_boot(process, game / 'qconsole.log', command, 150)
    except Exception as error:
        row['error'] = str(error)
    finally:
        if process:
            row['forcedStop'] = process.poll() is None
            process.close()
        log = (game / 'qconsole.log').read_text(errors='replace') if (game / 'qconsole.log').exists() else ''
        (folder / 'console.log').write_text(log, encoding='utf-8')
        row['loads'] = log.count("G_LoadGame '(null)'")
        row['engineErrors'] = [line for line in log.splitlines() if re.search(r'ERROR:|Server fatal crashed:|recursive error', line, re.I)]
        row['windowedObserved'] = '800 x 600 windowed' in log
        row['saves'] = []
        for path in saved.glob('*.svg'):
            if path.name in ('truncated.svg', 'badversion.svg'):
                continue
            row['saves'].append(dict(name=path.name, **header(path)))
            if path.name.startswith(('final', 'round')):
                shutil.copy2(path, folder / path.name)
        captures = list(home.rglob('save-cycle.png'))
        if len(captures) == 1:
            shutil.copy2(captures[0], folder / 'save-cycle.png')
            row['screenshotSha256'] = boot.sha(captures[0])
        row['passed'] = (row.get('exitCode') == 0 and not row.get('forcedStop') and not row['engineErrors']
                         and row['windowedObserved'] and row['loads'] == expected_loads and len(captures) == 1
                         and 'SAVE_CYCLE complete' in log and bool(re.search(r'"savegame_loading" is:"0', log))
                         and any(s['name'] == 'final.svg' and s['map'] == expected_map and
                                 s['episode'] == expected_episode for s in row['saves']))
        if case == 'invalid-headers':
            row['invalidRequestsRejected'] = log.count('Cannot load RTCW save') == 3 and 'loadgame <save name>: use a name within the save directory.' in log
            row['previousCurrentPreserved'] = boot.sha(saved / 'current.svg') == boot.sha(source)
            row['passed'] &= row['invalidRequestsRejected'] and row['previousCurrentPreserved']
        boot.remove_home(home, args.output)
        (folder / 'manifest.json').write_text(json.dumps(row, indent=2) + '\n')
    print(case, 'PASS' if row['passed'] else 'FAIL', row.get('error', ''), flush=True)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--cross-source', type=Path, required=True)
    parser.add_argument('--autosave', type=Path, required=True)
    parser.add_argument('--retail', type=Path, nargs='*', default=[])
    parser.add_argument('--cases', nargs='+', help='Run only these case names')
    args = parser.parse_args()
    args.executable = args.executable.resolve(strict=True)
    args.executable.relative_to(ROOT)
    args.installation = args.installation.resolve(strict=True)
    args.output = args.output.resolve()
    args.output.relative_to(ROOT / '.tmp')
    args.output.mkdir(parents=True, exist_ok=False)
    before = boot.inventory(args.installation)
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  executable=str(args.executable), executableSha256=boot.sha(args.executable), runs=[])
    cases = [
        ('repeat', args.source, 'wait 300; viewpos; ' + ''.join(
            f'loadgame campaign; wait 180; savegame round{i}; viewpos; ' for i in range(1, 4)), 4, None),
        ('cross-map', args.source, 'wait 300; set activeAction "exec cycle-finish.cfg"; loadgame destination', 2, args.cross_source),
        ('invalid-headers', args.source, 'wait 300; loadgame absent; loadgame truncated; loadgame badversion; loadgame ../outside; ', 1, None),
        ('autosave', args.autosave, 'wait 600; ', 1, None),
    ]
    cases += [(f'retail-{i}', path, 'wait 600; ', 1, None) for i, path in enumerate(args.retail)]
    if args.cases:
        unknown = set(args.cases) - {case[0] for case in cases}
        if unknown:
            parser.error('Unknown cases: ' + ', '.join(sorted(unknown)))
        cases = [case for case in cases if case[0] in args.cases]
    for case, source, action, expected, target in cases:
        report['runs'].append(run(args, case, source.resolve(strict=True), action, expected, target))
        report['installationUnchanged'] = before == boot.inventory(args.installation)
        (args.output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        if not report['installationUnchanged']:
            raise RuntimeError('Installation changed')
    return 0 if all(row['passed'] for row in report['runs']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
