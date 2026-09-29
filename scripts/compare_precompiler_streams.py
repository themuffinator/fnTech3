#!/usr/bin/env python3
"""Run two builds of the botlib script precompiler over every installed menu and bot file and compare them.

The engine reads menus (for the UI modules, through PC_LoadSourceHandle, and
Enemy Territory's through PC_LoadWolfETSourceHandle) and bot files (bot
characters, chat, weights, items and weapons) with the precompiler in
code/botlib/l_precomp.cpp. This compiles tests/fixtures/precompiler_host.cpp
twice, once with the precompiler as a git revision has it (--base, HEAD by
default) and once as the working tree has it. Each installation's game folder
is laid out from its paks into a scratch folder, later paks over earlier ones
as the game mounts them, and each host reads every ui/ and ui_mp/ *.menu, *.txt
and *.h file and every botfiles/**/*.c and *.h file in it as an entry file,
with #include resolved from the game folder, or for bot files from botfiles/
as the botlib reads them (Enemy Territory's name the folder themselves). The run passes when both hosts give the same token stream, the same
error and warning counts and the same exit status for every file.

Installations are only read.

  --installation KEY=PATH   q3, ql, rtcw, wolfet, stvef, jka or jk2
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args  # noqa: E402

HOST = ROOT / 'tests' / 'fixtures' / 'precompiler_host.cpp'
SOURCES = ('code/botlib/l_precomp.cpp', 'code/botlib/l_script.cpp', 'code/botlib/l_memory.cpp')
# Game folders each installation mounts, in mount order, and whether its menus
# are read by Enemy Territory's rules.
GAMES = {
    'q3': (('baseq3', 'missionpack'), False),
    'ql': (('baseq3',), False),
    'rtcw': (('main',), False),
    'wolfet': (('etmain',), True),
    'stvef': (('baseef',), False),
    'jka': (('base',), False),
    'jk2': (('base',), False),
}


def wanted(name):
    lower = name.lower()
    # RTCW's multiplayer menus live in ui_mp/; menudef.h and the like beside them
    if lower.startswith(('ui/', 'ui_mp/')) and lower.endswith(('.menu', '.txt', '.h')):
        return True
    return lower.startswith('botfiles/') and lower.endswith(('.c', '.h'))


def lay_out(installation, folder, target):
    """The game folder's menu and bot files, later paks over earlier ones, loose files last."""
    folder_path = next((p for p in installation.iterdir() if p.is_dir() and p.name.lower() == folder), None)
    if folder_path is None:
        return 0
    count = 0
    for pak in sorted((p for p in folder_path.iterdir() if p.suffix.lower() == '.pk3'), key=lambda p: p.name.lower()):
        try:
            archive = zipfile.ZipFile(pak)
        except zipfile.BadZipFile:
            continue
        with archive:
            for info in archive.infolist():
                if info.is_dir() or not wanted(info.filename):
                    continue
                out = target / info.filename.lower()
                out.parent.mkdir(parents=True, exist_ok=True)
                out.write_bytes(archive.read(info))
                count += 1
    return count


def build(source_text, directory, name):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('a C++17 compiler is required')
    precomp = directory / f'{name}_l_precomp.cpp'
    precomp.write_text(source_text, encoding='utf-8')
    exe = directory / f'{name}.exe'
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target:
        target = ['-DFNTECH3_Q3_ONLY']
    # The copy's own quoted includes are found beside the original.
    command = [compiler, *target, '-O2', '-std=c++17', '-DBOTLIB', '-w', '-I', str(ROOT),
               '-I', str(ROOT / 'code' / 'botlib'), str(HOST), str(precomp),
               *[str(ROOT / s) for s in SOURCES[1:]], '-o', str(exe)]
    built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=300)
    if built.returncode:
        raise SystemExit(built.stdout + built.stderr)
    return exe


def run(exe, root, wolfet, entry):
    ran = subprocess.run([str(exe), str(root), '1' if wolfet else '0', '1', entry], capture_output=True,
                         timeout=120)
    counts = ran.stderr.decode('latin-1').strip().splitlines()[-1:] if ran.stderr else []
    return dict(exit=ran.returncode, tokens=hashlib.sha256(ran.stdout).hexdigest(),
                count=ran.stdout.count(b'\n'), summary=counts[0] if counts else '')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--base', default='HEAD', help='revision whose precompiler is compared')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    installations = {}
    for item in args.installation:
        key, _, path = item.partition('=')
        if key not in GAMES or not path:
            parser.error(f'unknown installation {item!r}')
        installations[key] = Path(path).resolve(strict=True)
    base = subprocess.run(['git', 'show', f'{args.base}:code/botlib/l_precomp.cpp'], cwd=ROOT, capture_output=True,
                          check=True).stdout.decode('utf-8')
    work = (ROOT / 'code/botlib/l_precomp.cpp').read_text(encoding='utf-8')
    report = dict(base=subprocess.run(['git', 'rev-parse', args.base], cwd=ROOT, capture_output=True,
                                      text=True).stdout.strip(),
                  baseSha256=hashlib.sha256(base.encode('utf-8')).hexdigest(),
                  workSha256=hashlib.sha256(work.encode('utf-8')).hexdigest(),
                  installations={k: str(v) for k, v in installations.items()}, games={}, differences=[])
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='precompiler-streams-', dir=ROOT / '.tmp') as scratch:
        scratch = Path(scratch)
        before, after = build(base, scratch, 'before'), build(work, scratch, 'after')
        for key, installation in sorted(installations.items()):
            folders, wolfet = GAMES[key]
            root = scratch / key
            laid = sum(lay_out(installation, folder, root) for folder in folders)
            files = sorted(p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()) if laid else []
            same = tokens = errors = 0
            for entry in files:
                # The botlib reads bot files from its "botfiles" base folder,
                # where their includes are found; menus from the game folder.
                # Enemy Territory's bot files name their includes from the game
                # folder ("botfiles/chars.h").
                where, name = root, entry
                if entry.startswith('botfiles/') and key != 'wolfet':
                    where, name = root / 'botfiles', entry[len('botfiles/'):]
                one, two = run(before, where, wolfet, name), run(after, where, wolfet, name)
                tokens += two['count']
                errors += two['exit'] != 0
                if one == two:
                    same += 1
                else:
                    report['differences'].append(dict(game=key, file=entry, before=one, after=two))
            report['games'][key] = dict(files=len(files), identical=same, tokens=tokens, filesWithErrors=errors)
            print(key, report['games'][key], flush=True)
    report['passed'] = not report['differences']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('passed' if report['passed'] else f'{len(report["differences"])} files differ')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
