#!/usr/bin/env python3
"""Run isolated, windowed RTCW SP/MP developer boot probes on a private desktop.

Use an executable configured explicitly with -Drtcw-development=true. No input
is injected. Captures use the engine screenshotPNG command. Installed files
are read-only inputs; reports never contain copied game modules or assets.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import wave
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(installation):
    return {str(path.relative_to(installation)): [path.stat().st_size, path.stat().st_mtime_ns]
            for path in installation.rglob('*') if path.is_file()}


def remove_home(home, output):
    target = home.resolve(strict=True); target.relative_to(output.resolve(strict=True))
    for path in [target, *target.rglob('*')]:
        info = path.lstat()
        if path.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise RuntimeError(f'Refusing cleanup through a link: {path}')
    shutil.rmtree(target)


def wait_for_boot(process, log, command, timeout=45):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        code = process.poll()
        if code is not None:
            return code
        if log.is_file():
            text = log.read_text(encoding='utf-8', errors='replace')
            if '\nERROR:' in text or 'recursive error after:' in text or 'Server fatal crashed:' in text:
                raise RuntimeError('Engine reported an initialization error; see console.log')
        time.sleep(0.1)
    raise subprocess.TimeoutExpired(command, timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), default='glx')
    parser.add_argument('--scene', choices=('ui', 'map'), default='ui')
    parser.add_argument('--mode', choices=('both', 'sp', 'mp'), default='both')
    parser.add_argument('--frames', type=int, default=120,
                        help='Frames to wait after becoming active before capture (1-1200)')
    parser.add_argument('--server-state', action='store_true',
                        help='Print server status and the game entity list before a map capture')
    parser.add_argument('--entity-streams', action='store_true',
                        help='Exercise synthetic silent entity streams, replacement and stop on the selected audio backend')
    parser.add_argument('--language', type=int, choices=range(5), default=0)
    parser.add_argument('--pure', type=int, choices=(0, 1),
                        help='Override the selected profile default for an explicit pure-admission probe')
    parser.add_argument('--audio', choices=('off', 'openal-muted', 'legacy-muted'), default='off',
                        help='Focus-muted OpenAL exercises decoding; zero-volume legacy checks device initialization only')
    args = parser.parse_args()
    if args.entity_streams and (args.scene != 'map' or args.audio == 'off'):
        parser.error('--entity-streams requires --scene map and an enabled muted audio backend')
    if not 1 <= args.frames <= 1200:
        parser.error('--frames must be between 1 and 1200')
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    executable = args.executable.resolve(strict=True); executable.relative_to(ROOT)
    installation = args.installation.resolve(strict=True)
    output = args.output.resolve(); output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    before = inventory(installation)
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        worktreeDirty=bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
        executable=str(executable), executableSha256=sha(executable), installation=str(installation),
        rendererApi=int(re.search(r'REF_API_VERSION\s+(\d+)', (ROOT / 'code/renderercommon/tr_public.h').read_text())[1]),
        scope='Experimental RTCW SP/MP boot through one executable; no input, peer interop or gameplay-completion claim', runs=[])
    for mode, map_name in [('sp', 'escape1'), ('mp', 'mp_beach')]:
        if args.mode != 'both' and args.mode != mode:
            continue
        row_dir = output / mode; row_dir.mkdir()
        home = Path(tempfile.mkdtemp(prefix='home-', dir=row_dir)); game = home / 'main'; game.mkdir()
        if args.entity_streams:
            payload = io.BytesIO()
            with wave.open(payload, 'wb') as sound:
                sound.setnchannels(2); sound.setsampwidth(2); sound.setframerate(22050)
                sound.writeframes(bytes(22050 * 4 * 30))
            # This fixture contains only generated silence. A pak also tests
            # MP pure admission without changing the legitimate installation.
            with zipfile.ZipFile(game / 'zz-fntech3-stream-probe.pk3', 'w', zipfile.ZIP_DEFLATED) as pak:
                info = zipfile.ZipInfo('sound/fntech3-stream-probe.wav', (1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED; pak.writestr(info, payload.getvalue())
        filename = f'fntech3-rtcw-{mode}-{args.scene}-{args.renderer}'
        startup = dict(com_gameProfile=f'rtcw-{mode}', fs_basepath=str(installation), fs_homepath=str(home),
            fs_game='', fs_cdpath='', r_fullscreen='0', r_windowedMode='-1', r_windowedWidth='640',
            r_windowedHeight='480', in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0',
            com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0', cl_renderer=args.renderer,
            logfile='4', net_ip='127.0.0.1', net_port='0', net_ip6='::1', net_port6='0',
            rtx_rt_mode='0', rtx_rt_require='0')
        if mode == 'mp':
            startup['cl_language'] = str(args.language)
        if args.audio != 'off':
            startup.update(s_initsound='1', s_backend='openal' if args.audio == 'openal-muted' else 'legacy')
        if args.audio == 'legacy-muted':
            startup.update(s_volume='0', s_musicVolume='0')
        values = dict(developer='1', sv_autoRecordDemos='0', cl_autoRecordDemos='0',
            cl_allowDownload='0', cl_motd='0', com_introplayed='1', com_recommendedSet='1',
            com_maxfps='60', com_maxfpsUnfocused='60', com_maxfpsMinimized='60')
        if args.pure is not None:
            values['sv_pure'] = str(args.pure)
        if args.audio == 'openal-muted':
            values.update(s_muteWhenUnfocused='1', s_muteWhenMinimized='1')
        if args.audio == 'legacy-muted':
            # Retail UI initialization can reset the startup volume cvars.
            # Reapply in the explicit config and verify the observed values.
            values.update(s_volume='0', s_musicVolume='0', s_muteWhenUnfocused='1', s_muteWhenMinimized='1')
        for number in range(1, 6):
            values[f'sv_master{number}'] = ''
        diagnostics = ('s_info; s_backendActive; s_musicVolume; ' + ('s_currentMusic; ' if mode == 'sp' else '')) if args.audio != 'off' else ''
        if mode == 'sp' and args.scene == 'map':
            diagnostics += 'g_missionStats; cl_waitForFire; cl_paused; sv_paused; cg_norender; com_expectedhunkusage; '
        if args.server_state and args.scene == 'map':
            diagnostics += 'status debug; entitylist; sv_maxclients; sv_fps; timescale; g_reloading; cvarlist savegame_loading; '
        tail = f'wait {args.frames}; sv_pure; {diagnostics}screenshotPNG {filename}; wait 5; quit'
        if args.entity_streams:
            sound = 'sound/fntech3-stream-probe.wav'
            stop = 'stopstreamingsound 1000' if mode == 'sp' else 's_stop'
            restore = 'set s_volume 0; ' if args.audio == 'legacy-muted' else ''
            exercise = (f's_stop; set s_volume 1; streamingsound {sound} 1000 3 0; '
                        f'streamingsound {sound} 1000 0 0; wait 12; echo stream_probe_started; s_info; '
                        f'streamingsound {sound} 1000 3 0; wait 12; echo stream_probe_replaced; s_info; '
                        f'{stop}; wait 12; echo stream_probe_stopped; {restore}s_info; ')
            tail = f'wait {args.frames}; {exercise}sv_pure; {diagnostics}screenshotPNG {filename}; wait 5; quit'
        if args.server_state and args.scene == 'map':
            tail = 'status debug; ' + tail
        if args.scene == 'map':
            values['activeAction'] = tail
        else:
            values['rtcw_probeAction'] = tail
        config = ''.join(f'set {name} "{value}"\n' for name, value in values.items())
        config += '\nsv_pure\n'
        (game / 'rtcw-boot.cfg').write_text(config, encoding='utf-8')
        (row_dir / 'rtcw-boot.cfg').write_text(config, encoding='utf-8')
        command = [str(executable)]
        for name, value in startup.items():
            command += ['+set', name, value]
        command += ['+exec', 'rtcw-boot.cfg']
        if args.scene == 'map':
            command += ['+spdevmap' if mode == 'sp' else '+devmap', map_name]
        else:
            command += ['+vstr', 'rtcw_probeAction']
        row = dict(mode=mode, language=args.language if mode == 'mp' else None,
            scene=args.scene, map=map_name if args.scene == 'map' else None,
            renderer=args.renderer, rendererSha256=sha(executable.parent / f'fnql_{args.renderer}_x86.dll'),
            captureDelayFrames=args.frames,
            serverStateDiagnostics=args.server_state,
            entityStreamProbe=args.entity_streams,
            command=command, desktopSwitched=False, audio=args.audio, pureOverride=args.pure, screenshots=[])
        process = None
        try:
            assert sum(word.startswith('+') for word in command) <= 32, 'Startup command count exceeds engine capacity'
            assert len(subprocess.list2cmdline(command[1:]).encode('utf-8')) < 1024, 'Startup command line exceeds engine capacity'
            process = WindowsPrivateProcess(command, executable.parent, row_dir / 'stdout.log')
            row['desktop'] = process.name
            row['exitCode'] = wait_for_boot(process, game / 'qconsole.log', command)
        except Exception as error:
            row['error'] = str(error); row['errorType'] = type(error).__name__
        finally:
            if process:
                row['forcedStop'] = process.poll() is None; process.close()
            log = game / 'qconsole.log'
            text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
            if log.is_file():
                shutil.copy2(log, row_dir / 'console.log')
            for capture in home.rglob(filename + '.png'):
                destination = row_dir / capture.name; shutil.copy2(capture, destination)
                row['screenshots'].append(dict(path=str(destination), sha256=sha(destination)))
            row['moduleDiagnostics'] = [line for line in text.splitlines() if any(token in line.lower() for token in
                ('vm_loaddll', 'rtcw', 'sys_error', 'error:', 'unsupported', 'not implemented'))]
            row['passed'] = row.get('exitCode') == 0 and not row.get('forcedStop') and len(row['screenshots']) == 1
            row['windowedObserved'] = '640 x 480 windowed' in text
            row['pureValues'] = [int(n) for n in re.findall(r'"sv_pure" is:"([01])', text)]
            row['engineErrors'] = [line for line in text.splitlines() if re.search(
                r'error:|sys_error|fatal crashed:|not implemented|unsupported rtcw|rtcw[^\n]*unsupported|recursive cl_shutdown|server command overflow|lost reliable commands|reliable command.*cycled out', line, re.I)]
            row['passed'] &= row['windowedObserved'] and not row['engineErrors']
            if args.entity_streams:
                observations = {}
                for event in ('started', 'replaced', 'stopped'):
                    marker = 'stream_probe_' + event
                    section = text.rsplit(marker, 1)[-1] if marker in text else ''
                    match = re.search(r'RTCW entity streams: (\d+) active, (\d+) buffered', section)
                    observations[event] = list(map(int, match.groups())) if match else None
                row['entityStreamObservations'] = observations
                row['entityStreamsPassed'] = observations == dict(started=[2, 2], replaced=[2, 2], stopped=[0, 0])
                row['passed'] &= row['entityStreamsPassed']
            if args.audio != 'off':
                row['audioInitialized'] = ('Using OpenAL backend' if args.audio == 'openal-muted' else 'Backend: legacy') in text
                row['musicQueuedBuffers'] = [int(n) for n in re.findall(r'Music stream buffers: (\d+) queued', text)]
                row['masterVolumes'] = [float(n) for n in re.findall(r'Master volume: ([0-9.+-]+)', text)]
                row['musicVolumes'] = [float(n) for n in re.findall(r'"s_musicVolume" is:"([0-9.+-]+)', text)]
                row['focusMuteObserved'] = 'Mute state: focus-muted' in text
                row['passed'] &= row['audioInitialized']
                if args.audio == 'openal-muted':
                    # Main menus have background music. A map/briefing movie
                    # can legitimately stop it and use cinematic audio instead.
                    row['musicQueueRequired'] = args.scene == 'ui'
                    row['passed'] &= row['focusMuteObserved'] and (
                        not row['musicQueueRequired'] or any(n > 0 for n in row['musicQueuedBuffers']))
                else:
                    row['volumeMuteObserved'] = bool(row['masterVolumes'] and row['masterVolumes'][-1] == 0
                        and row['musicVolumes'] and row['musicVolumes'][-1] == 0)
                    row['passed'] &= row['volumeMuteObserved']
            row['nativeBootPassed'] = row['passed']
            row['visualPassed'] = None # Engine captures need a separate visual inspection.
            remove_home(home, output); row['homeRemoved'] = True
            report['runs'].append(row)
            report['installationUnchanged'] = before == inventory(installation)
            (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(mode, args.scene, 'BOOT PASS' if row['passed'] else 'BOOT FAIL', row.get('errorType', ''), flush=True)
        if not report['installationUnchanged']:
            raise RuntimeError('Installation file metadata changed during the probe')
    return 0 if all(row['passed'] for row in report['runs']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
