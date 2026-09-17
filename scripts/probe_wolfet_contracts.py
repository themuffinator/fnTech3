#!/usr/bin/env python3
"""Check every frozen ET field against user-supplied original headers.

Only interoperability facts are emitted. Reference implementations and game
assets are never incorporated. This is source evidence, not runtime parity.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BOUNDARY = ROOT / 'code/compat/wolfet_game_abi.h'


def records():
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', BOUNDARY.read_text(), flags=re.S)
    result = {}
    for body, name in re.findall(r'typedef\s+struct\s*\{([^{}]*)\}\s*(\w+)\s*;', text):
        fields = []
        for declaration in body.split(';'):
            if not declaration.strip():
                continue
            scalar, rest = declaration.strip().split(maxsplit=1)
            for item in rest.split(','):
                match = re.fullmatch(r'\s*(\w+)\s*((?:\[[^]]+\])*)\s*', item)
                if not match:
                    raise ValueError(declaration)
                fields.append((scalar, match[1], bool(match[2])))
        result[name] = fields
    return result


def fields_for(types, name, prefix=''):
    for scalar, field, array in types[name]:
        if scalar in types and not array:
            yield from fields_for(types, scalar, prefix + field + '.')
        else:
            yield prefix + field


def translation_unit():
    lines = ['#include "q_shared.h"', '#include "tr_types.h"',
             '#include "cg_public.h"', '#include "g_public.h"',
             '#include "ui_public.h"', '#include "bg_public.h"',
             '#include <stddef.h>', f'#include "{BOUNDARY.as_posix()}"']
    types = records()
    checks = 0
    for own in types:
        original = own.removeprefix('fntech3_wolfet_')
        if original == 'plane_t':
            original = 'cplane_t'
        lines += [f'_Static_assert(sizeof({own}) == sizeof({original}), "{original} size");',
                  f'_Static_assert(_Alignof({own}) == _Alignof({original}), "{original} alignment");']
        checks += 2
        for field in fields_for(types, own):
            lines += [f'_Static_assert(offsetof({own},{field}) == offsetof({original},{field}), "{original}.{field} offset");',
                      f'_Static_assert(sizeof((({own}*)0)->{field}) == sizeof((({original}*)0)->{field}), "{original}.{field} size");']
            checks += 2
    constants = {'MAX_CLIENTS': 'MAX_CLIENTS', 'MAX_CONFIGSTRINGS': 'MAX_CONFIGSTRINGS',
                 'MAX_GAMESTATE_CHARS': 'MAX_GAMESTATE_CHARS', 'CS_PLAYERS': 'CS_PLAYERS',
                 'CS_WOLFINFO': 'CS_WOLFINFO', 'MAX_ENTITIES_IN_SNAPSHOT': 'MAX_ENTITIES_IN_SNAPSHOT',
                 'UI_API_VERSION': 'UI_API_VERSION', 'CGAME_API_VERSION': 'CGAME_IMPORT_API_VERSION',
                 'GAME_API_VERSION': 'GAME_API_VERSION'}
    for own, ref in constants.items():
        lines += [f'_Static_assert(FNTECH3_WOLFET_{own} == {ref}, "{ref}");']
        checks += 1
    module_file = ROOT / 'code/compat/wolfet_module_calls.def'
    if module_file.exists():
        for name, value in re.findall(r'^FNTECH3_WOLFET_MODULE_CALL\(\w+, \w+, (\w+), (\d+)\)', module_file.read_text(), re.M):
            lines += [f'_Static_assert({name} == {value}, "{name} ordinal");']
            checks += 1
    return '\n'.join(lines) + '\n', checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--compiler', default='clang')
    parser.add_argument('--target')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    reference = args.source.resolve() / 'src'
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('compiler unavailable')
    unit, checks = translation_unit()
    scratch = ROOT / '.tmp' / 'wolfet'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as folder:
        path = Path(folder) / 'contracts.c'
        path.write_text(unit, encoding='utf-8')
        command = [compiler, '-std=c11', '-fsyntax-only', '-Wno-microsoft', '-Wno-comment']
        if args.target:
            command += ['--target=' + args.target]
        for name in ('game', 'cgame', 'ui', 'qcommon', 'renderer'):
            command += ['-I', str(reference / name)]
        run = subprocess.run([*command, str(path)], cwd=ROOT, capture_output=True, text=True, timeout=60)
    headers = ('game/q_shared.h', 'game/g_public.h', 'game/bg_public.h',
               'cgame/cg_public.h', 'cgame/tr_types.h', 'ui/ui_public.h')
    report = {'evidence': 'Source/Unit', 'target': args.target or 'compiler default',
              'staticAssertions': checks, 'passed': run.returncode == 0,
              'sourceSha256': {h: hashlib.sha256((reference / h).read_bytes()).hexdigest() for h in headers},
              'diagnostics': run.stdout + run.stderr}
    args.report.resolve().parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"WolfET: {'PASS' if report['passed'] else 'FAIL'} ({checks} assertions)")
    print(report['diagnostics'], end='')
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
