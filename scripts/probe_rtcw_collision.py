#!/usr/bin/env python3
"""Load legitimate RTCW SP/MP maps through the shipping collision sources.

This is an offline subsystem fixture, not an engine/gameplay compatibility
claim. Assets remain in their installation; only reports and a synthetic
probe executable are written beneath the selected project output directory.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ['code/qcommon/' + name + '.c' for name in
           ['cm_load', 'cm_patch', 'cm_polylib', 'cm_test', 'cm_trace', 'q_math', 'q_shared', 'md4']]
SOURCES += ['code/qcommon/game_profile.cpp', 'tests/fixtures/rtcw_collision_probe.cpp']
HEADERS = ['code/qcommon/' + name + '.h' for name in
           ['q_shared', 'q_platform', 'cm_local', 'cm_public', 'cm_model_handles',
            'cm_trace_contract', 'cm_game_contract', 'cm_patch', 'cm_polylib',
            'qfiles', 'ql_bsp', 'bsp_v43', 'game_manager', 'game_profile', 'surfaceflags']]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', default='clang')
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to(ROOT)  # Scratch and evidence stay inside the project.
    output.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which(args.compiler)
    cxx = str(Path(compiler).with_name(Path(compiler).name.replace('clang', 'clang++', 1))) if compiler else None
    if not compiler or not cxx or not Path(cxx).is_file():
        parser.error('clang and clang++ are required')
    target = ['--target=' + args.target] if args.target else []
    abi = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' and not args.target else []
    if os.name != 'nt' and not args.target:
        abi += ['-m32']
    actual_target = subprocess.run([compiler, *target, *abi, '-dumpmachine'], cwd=ROOT,
                                  capture_output=True, text=True, check=True, timeout=30).stdout.strip()
    sanitizer = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    objects = []
    build_log = []
    for source in SOURCES:
        path = ROOT / source
        obj = output / (path.stem + '.o')
        flags = [] if path.name == 'game_profile.cpp' else [
            '-DCom_GameProfile=FnFixture_GameProfile', '-DCom_GameBspFormat=FnFixture_GameBspFormat']
        command = [cxx if path.suffix == '.cpp' else compiler, *target, *abi, *sanitizer,
                   '-std=c++17' if path.suffix == '.cpp' else '-std=c11',
                   '-I', str(ROOT), '-I', str(ROOT / 'code/qcommon'), *flags, '-c', str(path), '-o', str(obj)]
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
        build_log += [json.dumps(command), run.stdout, run.stderr]
        if run.returncode:
            (output / 'build.log').write_text('\n'.join(build_log), encoding='utf-8')
            print(run.stdout + run.stderr)
            return 1
        objects.append(str(obj))
    executable = output / ('collision.exe' if os.name == 'nt' else 'collision')
    command = [cxx, *target, *abi, *sanitizer, *objects, '-o', str(executable)]
    run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
    build_log += [json.dumps(command), run.stdout, run.stderr]
    (output / 'build.log').write_text('\n'.join(build_log), encoding='utf-8')
    if run.returncode:
        print(run.stdout + run.stderr)
        return 1
    rows = []
    for profile, archive_name, map_name in [('rtcw-sp', 'pak0.pk3', 'escape1'),
                                          ('rtcw-mp', 'mp_pak0.pk3', 'mp_beach')]:
        archive_path = args.installation.resolve(strict=True) / 'main' / archive_name
        qpath = f'maps/{map_name}.bsp'
        with zipfile.ZipFile(archive_path) as archive:
            if archive.getinfo(qpath).file_size > 128 * 1024 * 1024:
                parser.error(f'{archive_path}: {qpath} exceeds the 128 MiB probe limit')
            data = archive.read(qpath)
        command = [str(executable), profile, qpath]
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        try:
            run = subprocess.run(command, cwd=ROOT, input=data, capture_output=True, timeout=60, **options)
        except subprocess.TimeoutExpired as error:
            run = subprocess.CompletedProcess(command, -1, error.stdout or b'',
                (error.stderr or b'') + b'\nCollision probe exceeded 60 seconds')
        diagnostics = run.stderr.decode('utf-8', errors='replace')
        result = None
        if run.returncode == 0:
            try:
                result = json.loads(run.stdout)
            except (ValueError, UnicodeDecodeError):
                diagnostics += '\nInvalid collision probe JSON output'
                run.returncode = -1
        row = dict(profile=profile, archive=str(archive_path), map=qpath,
                   mapSha256=hashlib.sha256(data).hexdigest(), mapBytes=len(data),
                   command=command, exitCode=run.returncode, passed=run.returncode == 0,
                   result=result, diagnostics=diagnostics)
        rows.append(row)
        print(f'{profile}: {"PASS" if row["passed"] else "FAIL"}; {result or diagnostics}')
    revision = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    dirty = bool(subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT, capture_output=True, text=True, check=True).stdout)
    report = dict(schemaVersion=1, evidence='Offline collision subsystem fixture',
                  utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  commit=revision, worktreeDirty=dirty, platform=os.name, compiler=compiler,
                  executable=str(executable), executableSha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                  target=actual_target, sanitizer=args.sanitize,
                  sourceSha256={name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in SOURCES + HEADERS},
                  runs=rows)
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if all(row['passed'] for row in rows) else 1


if __name__ == '__main__':
    raise SystemExit(main())
