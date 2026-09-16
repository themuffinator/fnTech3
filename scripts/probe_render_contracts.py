#!/usr/bin/env python3
"""Verify frozen Q3/QL render records against the pinned, read-only references.

Only source/ABI facts are checked. This does not load retail modules or claim
runtime rendering compatibility. Disposable reference headers stay under the
project's selected evidence directory and retain their original license text.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = ROOT / 'code/compat/legacy_render_layout.def'
REFERENCES = {
    'q3': ('7d8c9629cdc7979f09697266470359101ffd9eda', 'fnq3'),
    'ql': ('fce44fe714564621e6ceb79e3da7c53089646af2', 'fnql'),
}
HEADERS = ['code/qcommon/q_shared.h', 'code/qcommon/q_platform.h',
           'code/qcommon/surfaceflags.h', 'code/renderercommon/tr_types.h']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--q3-source', type=Path, required=True)
    parser.add_argument('--ql-source', type=Path, required=True)
    parser.add_argument('--compiler', default='clang')
    parser.add_argument('--target')
    parser.add_argument('--profile', choices=['all', 'q3', 'ql'], default='all')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve()
    report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('C11 compiler unavailable')
    target = ['--target=' + args.target] if args.target else []
    actual_target = subprocess.run([compiler, *target, '-dumpmachine'], cwd=ROOT,
        capture_output=True, text=True, check=True, timeout=30).stdout.strip()
    rows = []
    with tempfile.TemporaryDirectory(prefix='legacy-render-headers-', dir=report_path.parent) as temp:
        for profile, (revision, version_name) in REFERENCES.items():
            if args.profile not in ('all', profile):
                continue
            source = getattr(args, profile + '_source').resolve(strict=True)
            folder = Path(temp) / profile
            hashes = {}
            for name in HEADERS + [f'version/{version_name}_version.h']:
                run = subprocess.run(['git', '-C', str(source), 'show', f'{revision}:{name}'],
                    cwd=ROOT, capture_output=True, check=True, timeout=30)
                destination = folder / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(run.stdout)
                hashes[name] = hashlib.sha256(run.stdout).hexdigest()
            unit = folder / 'probe.c'
            unit.write_text('\n'.join([
                '#include "code/qcommon/q_shared.h"', '#include "code/renderercommon/tr_types.h"',
                '#include <stddef.h>',
                '#define FNTECH3_RENDER_RECORD(type, bytes, alignment) _Static_assert(sizeof(type) == bytes && _Alignof(type) == alignment, #type " layout");',
                '#define FNTECH3_RENDER_FIELD(type, own, original, offset, bytes) _Static_assert(offsetof(type, original) == offset && sizeof(((type *)0)->original) == bytes, #type "." #original " layout");',
                '#define FNTECH3_RENDER_TYPE(name, value) _Static_assert(name == value, #name " ordinal");',
                f'#include "{LAYOUT.as_posix()}"', '']), encoding='utf-8')
            command = [compiler, *target, '-std=c11', '-fsyntax-only', '-I', str(folder), str(unit)]
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
            row = dict(profile=profile, source=str(source), revision=revision, sourceSha256=hashes,
                       assertions=len(re.findall(r'^FNTECH3_RENDER_', LAYOUT.read_text(), re.M)),
                       command=command, passed=run.returncode == 0, diagnostics=run.stdout + run.stderr)
            rows.append(row)
            print(f'{profile}: {"PASS" if row["passed"] else "FAIL"}; {row["assertions"]} source assertions')
            if run.returncode:
                print(row['diagnostics'])
    report = dict(schemaVersion=1, evidence='Pinned source layout fixture',
                  utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), platform=os.name,
                  compiler=compiler, target=actual_target,
                  layoutSha256=hashlib.sha256(LAYOUT.read_bytes()).hexdigest(), runs=rows)
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if all(row['passed'] for row in rows) else 1


if __name__ == '__main__':
    raise SystemExit(main())
