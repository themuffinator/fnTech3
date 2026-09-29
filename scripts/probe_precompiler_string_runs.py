#!/usr/bin/env python3
"""Load a bot whose character file names it through a long run of strings a define expands to.

The botlib's script precompiler joins adjacent strings. The script reader
joins the quoted strings it sees together; PC_ReadToken joins the rest, such
as strings defines expand to, and read each by calling itself, a frame per
string (docs/fntech3/2026-09-29-precompiler-string-runs.md). This lays out a
Quake III dedicated server's isolated home with a copy of Sarge's character
file from the installation, in which the skill 1 name is written as a run of
RUN defines that expand to "" before "Sarge", loads q3dm1 with bots on and
adds Sarge at skill 1. Each server given (--server LABEL=EXE) reports how the
level ended, the error it printed if any, and the name the server listed for
the bot. Nothing but the character file is changed, and only in the probe's
home.

Windows only: private desktops, networking off, console commands only. The
installation is only read.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
from probe_retail_map_loads import CONFIGS, sha256, stop  # noqa: E402

RUN = 20000
DONE = 'PRECOMPILER_PROBE_DONE'
NAME_LINE = re.compile(r'(\tCHARACTERISTIC_NAME\s+)"Sarge"')
STATUS = re.compile(r'^\s*\d+\s+-?\d+\s+(?:\d+|PRM|CON|ZMB)\s+(.*?)\s+bot\s+\d+\s+\d+\s*$', re.M)
ERROR = re.compile(r'ERROR: (.*)')
COLOR = re.compile(r'\^[0-9A-Za-z]')


def character(installation):
    """Sarge's character file as the game mounts it, and the pak it came from."""
    found = None
    folder = installation / 'baseq3'
    for pak in sorted((p for p in folder.iterdir() if p.suffix.lower() == '.pk3'), key=lambda p: p.name.lower()):
        with zipfile.ZipFile(pak) as archive:
            for name in archive.namelist():
                if name.lower() == 'botfiles/bots/sarge_c.c':
                    found = (pak.name, archive.read(name))
    if not found:
        raise SystemExit('no botfiles/bots/sarge_c.c in the installation')
    return found


def with_run(text, count):
    """The file with the first CHARACTERISTIC_NAME written as `count` empty strings and "Sarge"."""
    if not NAME_LINE.search(text):
        raise SystemExit('the character file has no CHARACTERISTIC_NAME "Sarge"')
    text = text.replace('#include "chars.h"', '#include "chars.h"\n#define FNTECH3_EMPTY ""', 1)
    return NAME_LINE.sub(lambda m: m.group(1) + 'FNTECH3_EMPTY ' * count + '"Sarge"', text, count=1)


def play(exe, installation, home, output, slug, source, timeout):
    from windows_private_process import WindowsPrivateProcess

    if home.exists():
        shutil.rmtree(home)
    game = home / 'baseq3'
    (game / 'botfiles' / 'bots').mkdir(parents=True)
    (game / 'configs').mkdir()
    for config in CONFIGS:
        for suffix in ('.cfg', '_server.cfg'):
            (game / (config + suffix)).write_bytes(b'')
    (game / 'botfiles' / 'bots' / 'sarge_c.c').write_bytes(source)
    lines = ['map q3dm1', 'wait 50', 'addbot sarge 1', 'wait 100', 'status', f'echo {DONE}', 'quit']
    (game / 'precompiler.cfg').write_text(''.join(line + '\n' for line in lines), encoding='utf-8')
    command = [str(exe)]
    for key, value in dict(com_gameProfile='q3', fs_basepath=str(installation), fs_homepath=str(home), fs_game='',
                           logfile='2', developer='0', sv_pure='0', bot_enable='1', net_enabled='0',
                           dedicated='1', com_steamIntegration='0', bot_developer='0').items():
        command += ['+set', key, value]
    command += ['+exec', 'precompiler.cfg']
    log = game / 'qconsole.log'
    read = lambda: log.read_text(encoding='latin-1') if log.exists() else ''
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, exe.parent, output / f'{slug}-stdout.log')
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            text = read()
            # an error drop ends the level and with it the script's quit
            if 'ERROR:' in text:
                time.sleep(2)
                break
            time.sleep(0.2)
        forced = process.poll() is None
        code = process.poll()
    finally:
        stop(process)
    text = COLOR.sub('', read())
    (output / f'{slug}-console.log').write_text(text, encoding='latin-1')
    errors = ERROR.findall(text)
    listed = STATUS.findall(text)
    if code == 0 and DONE in text and not errors:
        outcome = 'loaded'
    elif errors:
        outcome = 'faulted' if 'Unhandled exception' in errors[0] or 'STACK_OVERFLOW' in text else 'rejected'
    elif not forced and code:
        outcome = 'crashed'
    else:
        outcome = 'hung'
    return dict(outcome=outcome, exitCode=code, forcedTermination=forced, errors=errors[:2],
                exception=re.findall(r'Exception (?:Code|Address): (\S+)', text)[:2], botsListed=listed,
                elapsedSeconds=round(time.monotonic() - begin, 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--server', action='append', required=True, help='LABEL=dedicated executable')
    parser.add_argument('--installation', type=Path, required=True, help='Quake III installation')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=60)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    installation = args.installation.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    pak, original = character(installation)
    changed = with_run(original.decode('latin-1'), RUN).encode('latin-1')
    report = dict(installation=str(installation), characterPak=pak,
                  characterSha256=hashlib.sha256(original).hexdigest(), run=RUN,
                  changedBytes=len(changed), servers={}, runs={})
    for item in args.server:
        label, _, path = item.partition('=')
        exe = Path(path).resolve(strict=True)
        report['servers'][label] = dict(path=str(exe), sha256=sha256(exe))
        for variant, source in (('retail', original), ('run', changed)):
            slug = f'{label}-{variant}'
            result = play(exe, installation, output / 'homes' / slug, output, slug, source, args.timeout)
            result['exeUnchanged'] = sha256(exe) == report['servers'][label]['sha256']
            report['runs'][slug] = result
            print(slug, result['outcome'], result['errors'][:1], result['botsListed'], flush=True)
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
