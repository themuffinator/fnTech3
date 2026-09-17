"""Check the reviewed RTCW command inventory; optionally verify original sources.

This is a registration audit, not a preprocessor, gameplay test, or ABI proof.
Conditional commands and all renderer/platform alternatives are included.
Only command names, handlers, locations and hashes are collected from references.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / 'docs/fntech3/rtcw-engine-commands.json'
SCOPES = ('client', 'server', 'qcommon', 'renderer', 'rendererglx',
          'renderervk', 'rendererrtx', 'win32', 'unix', 'sdl', 'macosx')
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*', re.S)
DIRECT = re.compile(r'\bCmd_AddCommand\s*\(\s*"([^"\n]+)"\s*,\s*([A-Za-z_]\w*)')
TABLE = re.compile(r'\{\s*"([^"\n]+)"\s*,\s*([A-Za-z_]\w*)')


def registrations(root: Path, *, current: bool = False) -> dict:
    """Extract literal registrations, preserving source line numbers."""
    found = {}
    for scope in SCOPES:
        for path in sorted((root / scope).rglob('*')):
            if path.suffix not in ('.c', '.cpp', '.h', '.hpp'):
                continue
            text = path.read_text(encoding='utf-8', errors='replace')
            text = TOKENS.sub(lambda m: re.sub(r'[^\n]', ' ', m[0])
                              if m[0].startswith(('//', '/*')) else m[0], text)
            matches = list(DIRECT.finditer(text))
            # These two engine-owned registries use tables. Other string tables
            # (key names, shader directives, cvars) are not console commands.
            if current and path.name in ('cl_input.cpp', 'sv_ccmds.cpp'):
                matches += list(TABLE.finditer(text))
            for match in matches:
                if match[2] in ('NULL', 'nullptr'):
                    continue  # module-forwarded completion entries
                found.setdefault(match[1].lower(), []).append({
                    'file': path.relative_to(root).as_posix(),
                    'line': text.count('\n', 0, match.start()) + 1,
                    'handler': match[2],
                })
    return found


def validate(inventory: dict, current: dict, references: dict | None = None) -> list[str]:
    errors = []
    names = set()
    for row in inventory['commands']:
        name = row['name']
        if name in names:
            errors.append(f'duplicate command: {name}')
        names.add(name)
        if not row['note'] or not row['original']:
            errors.append(f'missing review/provenance: {name}')
        status = row['disposition']
        if status in ('engine', 'unavailable-service'):
            locations = {entry['file'] for entry in current.get(name, [])}
            for owner in row['owners']:
                if owner not in locations:
                    errors.append(f'missing registration: {name} in code/{owner}')
            if not row['owners']:
                errors.append(f'missing engine owner: {name}')
        elif status in ('optional-tooling', 'retired-integration'):
            if name in current:
                errors.append(f'excluded command is now registered; review disposition: {name}')
        else:
            errors.append(f'unclassified command: {name}')
    for mode, root in (references or {}).items():
        actual = registrations(root)
        expected = {row['name'] for row in inventory['commands'] if mode in row['original']}
        for name in sorted(set(actual) ^ expected):
            errors.append(f'{mode} reference inventory drift: {name}')
        for relative, digest in inventory['reference_files'][mode].items():
            path = root / relative
            if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                errors.append(f'{mode} reference source changed: {relative}')
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-sp', type=Path, help='Original RTCW-SP src directory')
    parser.add_argument('--reference-mp', type=Path, help='Original RTCW-MP src directory')
    args = parser.parse_args()
    inventory = json.loads(INVENTORY.read_text(encoding='utf-8'))
    references = {mode: path for mode, path in
                  (('rtcw-sp', args.reference_sp), ('rtcw-mp', args.reference_mp)) if path}
    errors = validate(inventory, registrations(ROOT / 'code', current=True), references)
    for error in errors:
        print(error)
    counts = {mode: sum(mode in row['original'] for row in inventory['commands'])
              for mode in ('rtcw-sp', 'rtcw-mp')}
    print(f"{len(inventory['commands'])} reviewed command names; {counts}; {len(errors)} errors")
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
