#!/usr/bin/env python3
"""Capture engine screenshots from the RTX renderer on a private desktop.

The probe plays a demo or loads a map from a legitimate Quake III Arena
installation, freezes the view, and writes one engine screenshot per capture
step with the ``screenshotPNG`` command. Each step can change renderer
variables first, so one launch compares several transport settings on the
same frame. It reports the renderer's capability line, native-dispatch
marker, Vulkan validation messages and the hash of every image.

Windows only. The client runs windowed on an unswitched private desktop with
an isolated ``fs_homepath`` whose empty ``q3config.cfg`` shadows the
installation's; sound, mouse, joystick, Steam, Workshop and WebUI are
disabled. No input is injected and no operating-system capture is used. The
installation is read-only input.

Example::

    python scripts/rtx_probe.py --client .tmp/meson-q3-rtx-x64/fntech3-q3.exe \
        --installation "C:/Program Files (x86)/Steam/steamapps/common/Quake 3 Arena" \
        --output .tmp/rtx-probe/example --demo four --advance 400 \
        --startup rtx_rt_mode=2 --startup rtx_rt_require=1 \
        --capture native --capture "debug:rtx_rt_debug_visualizer=4"
"""
from __future__ import annotations

from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))

# Variables the renderer or client reads only at startup (latched or read
# during renderer initialisation). They must be on the command line.
STARTUP_KEYS = {
    'com_gameProfile', 'fs_basepath', 'fs_homepath', 'fs_game', 'r_fullscreen', 'r_mode',
    'r_customwidth', 'r_customheight', 'r_windowedMode', 'r_windowedWidth',
    'r_windowedHeight', 'in_mouse', 'in_nograb', 'in_joystick', 's_initsound',
    'cl_renderer', 'cl_webuiEnable', 'logfile', 'developer', 'com_steamIntegration',
    'fs_skipWorkshop', 'net_ip', 'com_introplayed', 'rtx_debug_vk_validation',
    'com_hunkMegs', 'com_zoneMegs',
}
MAX_STARTUP_BYTES = 1023
MAX_PLUS_COMMANDS = 31
VALIDATION_RE = re.compile(r'Vulkan validation \((?P<layer>[^)]*)\): (?P<message>.*)')
VUID_RE = re.compile(r'VUID-[A-Za-z0-9_.-]+')
MODE_RE = re.compile(r'RTX capability gate:\s*requested=\w+\s+\((\d+)\),\s*active=\w+\s+\((\d+)\),\s*require=(\d+)', re.I)
DISPATCH_MARKER = 'RTX RT primary dispatch validation'
FATAL_RE = re.compile(r'\bERR_FATAL\b|\bfatal error\b|\brecursive error\b|VK_ERROR_[A-Z_]+|device lost', re.I)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_assignment(text: str) -> tuple[str, str]:
    if '=' not in text:
        raise argparse.ArgumentTypeError(f'expected name=value, got {text!r}')
    name, value = text.split('=', 1)
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', name):
        raise argparse.ArgumentTypeError(f'bad variable name {name!r}')
    if '"' in value or ';' in value or '\n' in value:
        raise argparse.ArgumentTypeError(f'value for {name} may not contain quotes, semicolons or newlines')
    return name, value


def parse_capture(text: str) -> dict:
    """``name[:var=value,!command,...][@frames]`` -> a capture step.

    Items starting with ``!`` are console commands (``!vid_restart``,
    ``!devmap q3dm1``) run, in order with the assignments, before the wait.
    """
    frames = None
    if '@' in text:
        text, count = text.rsplit('@', 1)
        frames = int(count)
    name, _, assignments = text.partition(':')
    if not re.fullmatch(r'[A-Za-z0-9_-]+', name):
        raise argparse.ArgumentTypeError(f'bad capture name {name!r}')
    actions = []
    for item in assignments.split(','):
        if not item:
            continue
        if item.startswith('!'):
            command = item[1:]
            if not re.fullmatch(r'[A-Za-z0-9_ .-]+', command):
                raise argparse.ArgumentTypeError(f'bad command {command!r}')
            actions.append(('command', command))
        else:
            actions.append(('set',) + parse_assignment(item))
    values = [(a[1], a[2]) for a in actions if a[0] == 'set']
    return dict(name=name, set=values, actions=actions, frames=frames)


def build_plan(args: argparse.Namespace, home: Path) -> tuple[list[str], str]:
    width, height = args.width, args.height
    startup = {
        'com_gameProfile': 'q3', 'fs_basepath': str(args.installation), 'fs_homepath': str(home),
        'r_fullscreen': '0', 'r_mode': '-1', 'r_customwidth': str(width), 'r_customheight': str(height),
        'r_windowedMode': '-1', 'r_windowedWidth': str(width), 'r_windowedHeight': str(height),
        'in_mouse': '0', 'in_nograb': '1', 'in_joystick': '0', 's_initsound': '0',
        'cl_renderer': 'rtx', 'cl_webuiEnable': '0', 'logfile': '2', 'developer': '1',
        'com_steamIntegration': '0', 'fs_skipWorkshop': '1', 'net_ip': '127.0.0.1',
        'com_introplayed': '1',
    }
    runtime = {
        'cl_allowDownload': '0', 'cl_motd': '0', 'sv_autoRecordDemos': '0', 'com_maxfps': '60',
        'com_maxfpsUnfocused': '60', 'cg_lagometer': '0', 'con_notifytime': '0', 'cg_drawFPS': '0',
    }
    for name, value in args.startup:
        startup[name] = value
    for name, value in args.set:
        (startup if name in STARTUP_KEYS else runtime)[name] = value

    lines = [f'set {name} "{value}"' for name, value in runtime.items()]
    if args.demo:
        lines += ['set timedemo "1"', 'set cl_freezeDemo "0"', f'demo "{args.demo}"']
        # timedemo advances one snapshot per rendered frame, so the frozen
        # frame is the same on every run of the same demo. It also sets the
        # client's time from its frame count even while cl_freezeDemo is set
        # (CL_SetCGameTime), so it must end when the view freezes.
        lines += [f'wait {args.advance}', 'set cl_freezeDemo "1"', 'set timedemo "0"']
    else:
        lines += [f'devmap {args.map}', f'wait {args.advance}']
        lines += [f'{command}' for command in args.command]
    if args.demo:
        lines += [f'{command}' for command in args.command]
    lines += ['rtx_caps_report 2']
    for step in args.capture:
        for action in step['actions']:
            if action[0] == 'set':
                lines.append(f'set {action[1]} "{action[2]}"')
            else:
                lines.append(action[1])
        lines.append(f'wait {step["frames"] if step["frames"] is not None else args.settle}')
        lines.append(f'screenshotPNG {step["name"]}')
        lines.append('wait 3')
    lines.append('quit')
    script = '\n'.join(lines) + '\n'

    command = [str(args.client)]
    for name, value in startup.items():
        command += ['+set', name, value]
    command += ['+exec', 'rtx-probe.cfg']
    plus = sum(1 for arg in command if arg.startswith('+'))
    if plus > MAX_PLUS_COMMANDS:
        raise SystemExit(f'{plus} startup commands exceed the {MAX_PLUS_COMMANDS} the engine keeps')
    length = len(subprocess.list2cmdline(command[1:]).encode('utf-8'))
    if length > MAX_STARTUP_BYTES:
        raise SystemExit(f'startup command line is {length} bytes; the engine refuses more than {MAX_STARTUP_BYTES}')
    return command, script


def summarize_console(text: str) -> dict:
    validation = [m.group(0) for m in VALIDATION_RE.finditer(text)]
    vuids = sorted({v for line in validation for v in VUID_RE.findall(line)})
    mode = MODE_RE.search(text)
    fatal = [line for line in text.splitlines() if FATAL_RE.search(line)]
    return dict(
        mode=dict(requested=int(mode.group(1)), active=int(mode.group(2)), require=int(mode.group(3))) if mode else None,
        nativeDispatch=DISPATCH_MARKER in text,
        validationMessages=len(validation),
        validationSample=validation[:20],
        vuids=vuids,
        fatal=fatal[:20],
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True, help='fnTech3 client executable')
    parser.add_argument('--installation', type=Path, required=True, help='legitimate Quake III Arena root')
    parser.add_argument('--output', type=Path, required=True, help='new result directory')
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--demo', help='demo name resolved by the engine (e.g. four)')
    source.add_argument('--map', help='map to load with devmap')
    parser.add_argument('--advance', type=int, default=300, help='frames to play (or wait after devmap) before freezing')
    parser.add_argument('--settle', type=int, default=90, help='frames to wait before each capture without @frames')
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    parser.add_argument('--startup', type=parse_assignment, action='append', default=[],
                        help='name=value set on the command line (latched renderer variables)')
    parser.add_argument('--set', type=parse_assignment, action='append', default=[],
                        help='name=value set before the demo starts (startup names go to the command line)')
    parser.add_argument('--command', action='append', default=[], help='console command run after freezing')
    parser.add_argument('--capture', type=parse_capture, action='append', default=[],
                        help='name[:var=value,...][@frames]; captures run in order')
    parser.add_argument('--pak', type=Path, action='append', default=[],
                        help='pk3 copied into the isolated home (a synthetic or user-supplied texture pack)')
    parser.add_argument('--timeout', type=float, default=150.0)
    parser.add_argument('--plan', action='store_true', help='print the command and script without running')
    args = parser.parse_args()
    if not args.capture:
        args.capture = [parse_capture('capture')]
    args.client = args.client.resolve()
    args.installation = args.installation.resolve()
    output = args.output.resolve()
    home = output / 'home'
    command, script = build_plan(args, home)
    if args.plan:
        print(json.dumps(dict(command=command, script=script), indent=2))
        return 0
    if os.name != 'nt':
        parser.error('this probe requires the Windows private-desktop harness')
    from windows_private_process import WindowsPrivateProcess

    output.mkdir(parents=True, exist_ok=False)
    (home / 'baseq3').mkdir(parents=True)
    (home / 'baseq3' / 'rtx-probe.cfg').write_text(script, encoding='ascii')
    # The home path is searched first: an empty q3config.cfg keeps the
    # installation's own (r_vertexLight, r_picmip, ...) out of the probe.
    (home / 'baseq3' / 'q3config.cfg').write_text('// rtx_probe: defaults only\n', encoding='ascii')
    for pak in args.pak:
        shutil.copyfile(pak, home / 'baseq3' / pak.name)
    renderer_dlls = sorted(args.client.parent.glob('fnql_rtx_*.dll'))
    manifest = dict(
        command=command, script=script,
        utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        commit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True).stdout.strip(),
        dirty=bool(subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT, capture_output=True, text=True).stdout.strip()),
        client=dict(path=str(args.client), sha256=sha(args.client)),
        renderers=[dict(path=str(p), sha256=sha(p)) for p in renderer_dlls],
        paks=[dict(name=p.name, sha256=sha(p)) for p in args.pak],
        privateDesktop=True,
    )
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, args.client.parent, output / 'stdout.log')
    try:
        while process.poll() is None and time.monotonic() - begin < args.timeout:
            time.sleep(0.2)
        manifest['exitCode'] = process.poll()
        manifest['forcedTermination'] = manifest['exitCode'] is None
    finally:
        process.close()
    manifest['elapsedSeconds'] = round(time.monotonic() - begin, 2)
    logs = sorted(home.rglob('qconsole.log'))
    text = logs[0].read_text(encoding='utf-8', errors='replace') if logs else ''
    (output / 'console.log').write_text(text, encoding='utf-8')
    manifest['console'] = summarize_console(text)
    images = []
    for step in args.capture:
        found = sorted(home.rglob(f'{step["name"]}.png'))
        if found:
            target = output / found[0].name
            target.write_bytes(found[0].read_bytes())
            images.append(dict(name=step['name'], set=step['set'], path=str(target), sha256=sha(target)))
        else:
            images.append(dict(name=step['name'], set=step['set'], missing=True))
    manifest['images'] = images
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(dict(output=str(output), console=manifest['console'],
                          images=[(i['name'], 'missing' if i.get('missing') else 'ok') for i in images],
                          exitCode=manifest['exitCode'], forced=manifest['forcedTermination'],
                          elapsed=manifest['elapsedSeconds']), indent=2))
    return 0 if all(not i.get('missing') for i in images) and not manifest['console']['fatal'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
