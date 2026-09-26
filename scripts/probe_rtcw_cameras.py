#!/usr/bin/env python3
"""Compare owned camera evaluation with unmodified local SP/MP source references.

Reads user-supplied camera assets from pk3 archives into pipes; persists only
identities, result hashes and comparison diagnostics. No retail input files,
game modules, renderer, network or device input are created or invoked.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args


def identity(path):
    return dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True, help='RTCW main asset directory')
    parser.add_argument('--sp-reference', type=Path, required=True, help='Original SP src directory')
    parser.add_argument('--mp-reference', type=Path, required=True, help='Original MP src directory')
    parser.add_argument('--iortcw-reference', type=Path, help='Optional iortcw tree, containing SP and MP')
    parser.add_argument('--output', type=Path, required=True, help='New evidence directory under this project .tmp')
    parser.add_argument('--compiler', default='clang++')
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to((ROOT / '.tmp').resolve())
    output.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('C++ compiler unavailable')
    target = compiler_target_args(compiler)
    references = [('original-sp', args.sp_reference / 'splines', False),
                  ('original-mp', args.mp_reference / 'splines', False)]
    if args.iortcw_reference:
        references += [(f'iortcw-{mode.lower()}', args.iortcw_reference / mode / 'code/splines', True)
                       for mode in ['SP', 'MP']]
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  worktreeDirty=True, compiler=compiler, target=target,
                  scope='Numerical source comparison with local assets; no gameplay/rendering claim',
                  cameraSources=[identity(ROOT / p) for p in ['code/compat/rtcw_camera.hpp',
                      'code/compat/rtcw_camera_parser.hpp', 'tests/rtcw_camera_probe_host.cpp']], references=[], rows=[])
    host = ROOT / 'tests/rtcw_camera_probe_host.cpp'
    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    with tempfile.TemporaryDirectory(prefix='compile-', dir=output) as temporary:
        folder = Path(temporary)
        binaries = {}
        builds = [('candidate-sp', None, False), ('candidate-mp', None, False), *references]
        for name, reference, iortcw in builds:
            binary = folder / (name + ('.exe' if os.name == 'nt' else ''))
            command = [compiler, *target, '-std=c++17', '-O2', '-ffp-contract=off', '-fno-strict-aliasing', '-Wno-everything', '-I', str(ROOT)]
            if name == 'candidate-mp':
                command += ['-DCANDIDATE_CAMERA_MP']
            if os.name == 'nt':
                command += ['-DWIN32', '-D_CRT_SECURE_NO_WARNINGS']
            if reference:
                files = [reference / p for p in ['splines.cpp', 'q_parse.cpp', 'q_shared.cpp', 'util_str.cpp', 'math_vector.cpp']]
                if iortcw:
                    # iortcw's Makefile links qcommon/q_shared.c; the obsolete
                    # splines/q_shared.cpp is not part of its engine build.
                    files.remove(reference / 'q_shared.cpp')
                    shared = reference.parent / 'qcommon/q_shared.c'
                    files.append(shared)
                # Record source notices without importing any implementation
                # into the engine or committing reference copies.
                for source in files:
                    if 'either version 3' not in source.read_text(errors='replace'):
                        raise RuntimeError(f'Expected GPLv3-or-later reference notice: {source}')
                headers = [reference / p for p in ['splines.h', 'q_splineshared.h', 'util_list.h', 'util_str.h', 'math_vector.h']]
                report['references'].append(dict(name=name, license='GPL-3.0-or-later; observational executable only',
                                                 files=[identity(p) for p in files + headers]))
                command += ['-DORIGINAL_CAMERA_REFERENCE', '-I', str(reference)]
                if iortcw:
                    command += ['-DIORTCW_CAMERA_REFERENCE']
                    shim = folder / 'reference-types.h'
                    shim.write_text('#include <stdint.h>\n#if defined(_MSC_VER)\n#define int8_t fntech3_probe_reference_int8_t\n#endif\n#if defined(_WIN64) && !defined(__WIN64__)\n#define __WIN64__ 1\n#endif\n')
                    cpp_shim = folder / 'reference-cpp-types.h'
                    cpp_shim.write_text('#include <cstdio>\n#include <cstdlib>\n#include <cstring>\n#include <string>\n#include <iostream>\n#include <iterator>\n' + shim.read_text() + '#include "q_splineshared.h"\n#undef int8_t\n#ifdef _MSC_VER\n#undef __attribute__\n#endif\n')
                    command += ['-include', str(cpp_shim)]
                    shared_object = folder / (name + '-shared.o')
                    ccommand = [compiler, *target, '-x', 'c', '-std=c11', '-O2', '-Wno-everything', '-include', str(shim)]
                    if os.name == 'nt':
                        ccommand += ['-DWIN32', '-D_CRT_SECURE_NO_WARNINGS']
                    ccommand += ['-c', str(shared), '-o', str(shared_object)]
                    cbuilt = subprocess.run(ccommand, cwd=ROOT, capture_output=True, text=True, timeout=60, **options)
                    (output / (name + '-shared-build.log')).write_text(cbuilt.stdout + cbuilt.stderr)
                    if cbuilt.returncode:
                        raise RuntimeError(f'{name} C helper build failed; inspect its shared-build.log')
                    command += [str(p) for p in files if p != shared] + [str(shared_object)]
                else:
                    command += [str(p) for p in files]
            command += [str(host), '-o', str(binary)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90, **options)
            (output / (name + '-build.log')).write_text(built.stdout + built.stderr)
            if built.returncode:
                raise RuntimeError(f'{name} build failed; see {output / (name + "-build.log")}')
            binaries[name] = binary
        report['executables'] = {name: identity(binary) for name, binary in binaries.items()}
        # Exercise dense and irregular updates, exact interval edges, expiry,
        # long gaps and a second start with the same parsed definition.
        times = sorted(set([*range(0, 4001, 37), *range(5000, 60001, 937),
                            0, 9, 10, 11, 499, 500, 501, 999, 1000, 1001, 1999, 2000, 2001,
                            2999, 3000, 3001, 9999, 10000, 10001, 29999, 30000, 30001, 30999, 31000, 60000]))
        report['queryOffsets'] = times
        report['startsPerFile'] = 2
        for pak in sorted(args.installation.glob('*.pk3')):
            with zipfile.ZipFile(pak) as archive:
                for member in archive.infolist():
                    if not member.filename.lower().endswith('.camera'):
                        continue
                    if member.file_size > 1024 * 1024:
                        raise RuntimeError(f'Oversize camera fixture {member.filename}')
                    data = archive.read(member)
                    runs = {}
                    for name, binary in binaries.items():
                        run = subprocess.run([str(binary), *map(str, times)], input=data, cwd=ROOT,
                                             capture_output=True, timeout=15, **options)
                        runs[name] = run
                    comparisons = []
                    for name, _, _ in references:
                        # iortcw MP adopts the corrected SP timing. Original
                        # MP remains authoritative for the fnTech3 MP profile.
                        candidate = 'candidate-mp' if name == 'original-mp' else 'candidate-sp'
                        actual = runs[candidate]
                        expected = runs[name]
                        a_lines, b_lines = actual.stdout.splitlines(), expected.stdout.splitlines()
                        differences = []; delta = 0.0; compared = 0; source_nonfinite = 0
                        for a, b in zip(a_lines, b_lines):
                            if a.startswith(b'sample ') and b.startswith(b'sample '):
                                av, bv = a.split(), b.split()
                                def number(token):
                                    return float('nan') if b'nan' in token.lower() else float(token)
                                actual_values = [number(x) for x in av[4:]]
                                source_values = [number(x) for x in bv[4:]]
                                valid = all(math.isfinite(v) for v in actual_values)
                                source_valid = all(math.isfinite(v) for v in source_values)
                                same = len(av) == len(bv) and av[:4] == bv[:4] and valid
                                if source_valid:
                                    values = [abs(x-y) for x,y in zip(actual_values,source_values)]
                                    if valid:
                                        delta = max(delta, *values)
                                    same = same and max(values) <= .015
                                    compared += 1
                                else:
                                    # Degenerate original spline spans can
                                    # produce NaNs. Record these separately;
                                    # require a finite candidate, not NaN parity.
                                    source_nonfinite += 1
                            else:
                                same = a == b
                            if not same and len(differences) < 3:
                                differences.append(dict(candidate=a.decode(errors='replace'), reference=b.decode(errors='replace')))
                        passed = (actual.returncode == expected.returncode == 0 and len(a_lines) == len(b_lines)
                                  and not differences and actual.stderr == expected.stderr)
                        comparisons.append(dict(reference=name, candidate=candidate, passed=passed, maxNumericDelta=delta,
                                                finiteSourceSamplesCompared=compared, sourceNonFiniteSamples=source_nonfinite,
                                                differences=differences, exitCodes=[actual.returncode, expected.returncode],
                                                resultSha256=[hashlib.sha256(r.stdout).hexdigest() for r in [actual, expected]],
                                                eventSha256=[hashlib.sha256(r.stderr).hexdigest() for r in [actual, expected]],
                                                diagnostics=[] if passed else [r.stderr.decode(errors='replace')[:2048] for r in [actual, expected]]))
                    row = dict(pak=pak.name, camera=member.filename, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                               samplesPerImplementation=2 * len(times), comparisons=comparisons,
                               passed=all(c['passed'] for c in comparisons))
                    report['rows'].append(row)
                    if not row['passed']:
                        print('FAIL', member.filename, flush=True)
        if not report['rows']:
            raise RuntimeError('No camera assets found')
        report['passed'] = all(row['passed'] for row in report['rows'])
        report['cameraCount'] = len(report['rows'])
        report['comparisonCount'] = len(report['rows']) * len(references)
        report['samplesCompared'] = sum(row['samplesPerImplementation'] * len(references) for row in report['rows'])
    report['temporaryBuildRemoved'] = not folder.exists()
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ['passed', 'cameraCount', 'comparisonCount', 'samplesCompared', 'temporaryBuildRemoved']}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
