"""Read every Jedi Academy map, mesh and skeleton with fnTech3's own readers.

The probe extracts each asset from a legitimate installation's paks into a
temporary directory, runs it through the engine's RBSP translator and Ghoul2
readers, and reports what each one holds. It reads the installation, writes
only below --output and its own temporary directory, and never stores a retail
payload in the repository.

    python scripts/probe_jka_assets.py --installation "E:/Games/Jedi Academy" \
        --output .tmp/jka/assets
"""
import argparse
import collections
import json
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args  # noqa: E402

KINDS = {'.bsp': 'map', '.glm': 'mesh', '.gla': 'skeleton',
         '.rof': 'roff', '.roff': 'roff', '.ibi': 'script'}


def build_probe(folder):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('a C++17 compiler is required')
    exe = folder / ('probe.exe' if sys.platform == 'win32' else 'probe')
    subprocess.run(
        [compiler, *compiler_target_args(compiler), '-std=c++17', '-O2',
         str(ROOT / 'tests/fixtures/jka_asset_probe.cpp'), '-o', str(exe)],
        check=True, capture_output=True, text=True)
    return exe


def extract(installation, folder):
    """Extract every map, mesh and skeleton, latest pak winning, as the
    filesystem would mount them."""
    paks = sorted((Path(installation) / 'base').glob('*.pk3'))
    if not paks:
        raise SystemExit(f'no paks under {installation}/base')
    chosen = {}
    for pak in paks:
        with zipfile.ZipFile(pak) as archive:
            for info in archive.infolist():
                suffix = Path(info.filename).suffix.lower()
                if suffix in KINDS and info.file_size:
                    chosen[info.filename.lower()] = (pak, info.filename)
    files = []
    for name, (pak, member) in sorted(chosen.items()):
        target = folder / name.replace('/', '_')
        with zipfile.ZipFile(pak) as archive, open(target, 'wb') as out:
            shutil.copyfileobj(archive.open(member), out)
        files.append((name, target))
    return files


def parse(line):
    fields = line.split()
    record = {'path': fields[0], 'kind': fields[1], 'status': fields[2]}
    for field in fields[3:]:
        key, _, value = field.partition('=')
        record[key] = value if not value.lstrip('-').isdigit() else int(value)
    if record['status'] != 'ok':
        record['reason'] = ' '.join(fields[3:])
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', required=True)
    parser.add_argument('--output', default='.tmp/jka/assets')
    arguments = parser.parse_args()

    output = Path(arguments.output)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='fntech3-jka-assets-') as temporary:
        folder = Path(temporary)
        probe = build_probe(folder)
        files = extract(arguments.installation, folder)
        names = {str(path): name for name, path in files}

        def run(lines):
            finished = subprocess.run([str(probe)], input='\n'.join(lines) + '\n',
                                      capture_output=True, text=True)
            if finished.returncode:
                raise SystemExit(f'probe failed: {finished.stderr}')
            found = []
            for line in finished.stdout.splitlines():
                record = parse(line)
                record['path'] = names.get(record['path'], record['path'])
                found.append(record)
            return found

        # Skeletons and maps first, so every mesh can then be read against the
        # bone count of the skeleton it names, as the renderer would pair them.
        meshes = [path for name, path in files if name.endswith('.glm')]
        others = [path for name, path in files if not name.endswith('.glm')]
        records = run([str(path) for path in others])
        bones = {record['path'].rsplit('.', 1)[0].lower(): record['bones']
                 for record in records if record['kind'] == 'skeleton' and record['status'] == 'ok'}
        pending = run([str(path) for path in meshes])
        lines = []
        for record in pending:
            named = str(record.get('skeleton', '')).lower().lstrip('*')
            lines.append(f"{folder / record['path'].replace('/', '_')}\t{bones.get(named, 0)}")
            record['skeletonBones'] = bones.get(named, 0)
        paired = {record['path']: record for record in run(lines)}
        for record in pending:
            record.update(paired.get(record['path'], {}))
        records += pending

    counts = collections.Counter((record['kind'], record['status']) for record in records)
    summary = {
        'installation': arguments.installation,
        'files': len(records),
        'counts': {f'{kind}/{status}': count for (kind, status), count in sorted(counts.items())},
        'refused': [record for record in records if record['status'] != 'ok'],
    }
    for kind, key in (('map', 'surfaces'), ('mesh', 'verts'), ('skeleton', 'frames'),
                      ('roff', 'entries')):
        values = [record[key] for record in records
                  if record['kind'] == kind and record['status'] == 'ok' and key in record]
        if values:
            summary[f'{kind}_{key}_total'] = sum(values)
            summary[f'{kind}_{key}_max'] = max(values)
    skeletons = {record['path'].rsplit('.', 1)[0].lower()
                 for record in records if record['kind'] == 'skeleton'}
    named = [str(record.get('skeleton', '')).lower() for record in records
             if record['kind'] == 'mesh' and record['status'] == 'ok']
    # "*default" is Raven's marker for a mesh that animates with no skeleton.
    summary['meshes_without_a_skeleton'] = sum(1 for name in named if name.startswith('*'))
    summary['meshes_paired_with_a_skeleton'] = sum(1 for name in named
                                                   if not name.startswith('*') and name in skeletons)
    summary['meshes_naming_an_absent_skeleton'] = sorted(
        {name for name in named if not name.startswith('*') and name not in skeletons})
    summary['distinct_pairs'] = len({(record['path'], record.get('skeleton'))
                                     for record in records if record['kind'] == 'mesh'})

    (output / 'assets.json').write_text(json.dumps(
        {'summary': summary, 'records': records}, indent=1), encoding='utf-8')
    print(json.dumps(summary, indent=1))
    return 0 if not summary['refused'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
