#!/usr/bin/env python3
"""Parse every MDM and MDX in an installed Enemy Territory with fnTech3's reader.

Extracts the models from the installation's paks into a scratch directory, never
writing to the installation, and reports the parser's verdict for each file.
This is asset-decode evidence; it makes no rendering claim.
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

HARNESS = r'''
#include "code/compat/wolfet_model_format.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char **argv) {
 if (argc != 3) return 2;
 std::FILE *file = std::fopen(argv[2], "rb");
 if (!file) { std::printf("unreadable\n"); return 1; }
 std::fseek(file, 0, SEEK_END);
 const long length = std::ftell(file);
 std::fseek(file, 0, SEEK_SET);
 std::vector<unsigned char> bytes(length > 0 ? static_cast<size_t>(length) : 0);
 if (!bytes.empty() && std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) {
  std::fclose(file); std::printf("short read\n"); return 1;
 }
 std::fclose(file);
 using namespace fntech3::wolfet::model;
 if (argv[1][2] == 'x') {  // "mdm" or "mdx"
  Mdx mdx;
  const Error error = ParseMdx(bytes.data(), bytes.size(), mdx);
  if (error != Error::None) { std::printf("%s\n", ErrorName(error)); return 1; }
  std::printf("ok version=%d frames=%d bones=%d torso=%d\n",
   mdx.version, mdx.numFrames, mdx.numBones, mdx.torsoParent);
  return 0;
 }
 Mdm mdm;
 const Error error = ParseMdm(bytes.data(), bytes.size(), 0, mdm);
 if (error != Error::None) { std::printf("%s\n", ErrorName(error)); return 1; }
 std::printf("ok version=%d surfaces=%d tags=%d verts=%d tris=%d maxbone=%d\n",
  mdm.version, mdm.numSurfaces, mdm.numTags, mdm.totalVerts, mdm.totalTriangles,
  mdm.maxBoneReference);
 return 0;
}
'''


def build(folder):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('a C++17 compiler is required')
    source = folder/'models.cpp'
    executable = folder/('models.exe' if os.name == 'nt' else 'models')
    source.write_text(HARNESS, encoding='utf-8')
    built = subprocess.run([compiler, '-std=c++17', '-I', str(ROOT), str(source),
        '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=180)
    if built.returncode:
        raise SystemExit(built.stdout + built.stderr)
    return executable


def extract(installation, folder):
    found = []
    for pak in sorted(installation.rglob('*.pk3')):
        try:
            archive = zipfile.ZipFile(pak)
        except (OSError, zipfile.BadZipFile):
            continue
        with archive:
            for entry in archive.namelist():
                suffix = Path(entry).suffix.lower()
                if suffix not in ('.mdm', '.mdx'):
                    continue
                target = folder/'assets'/pak.stem/entry.replace('/', os.sep)
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(entry) as src, open(target, 'wb') as dst:
                    shutil.copyfileobj(src, dst)
                found.append((pak.name, entry, target, suffix))
    return found


def probe(installation, output):
    before = {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
              for p in installation.rglob('*') if p.is_file()}
    output.mkdir(parents=True, exist_ok=True)
    results = []
    with tempfile.TemporaryDirectory(dir=output) as directory:
        folder = Path(directory)
        executable = build(folder)
        for pak, entry, path, suffix in extract(installation, folder):
            run = subprocess.run([str(executable), suffix[1:], str(path)],
                capture_output=True, text=True, timeout=60)
            results.append({
                'pak': pak, 'file': entry, 'kind': suffix[1:],
                'bytes': path.stat().st_size,
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                'parsed': run.returncode == 0,
                'detail': run.stdout.strip() or run.stderr.strip(),
            })
    after = {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
             for p in installation.rglob('*') if p.is_file()}
    summary = {
        'profile': 'wolfet',
        'evidence': 'Assets: installed MDM/MDX decode',
        'installation': str(installation),
        'installationUnchanged': before == after,
        'total': len(results),
        'parsed': sum(1 for r in results if r['parsed']),
        'results': sorted(results, key=lambda r: (r['kind'], r['file'])),
    }
    summary['passed'] = bool(results) and summary['parsed'] == summary['total'] \
        and summary['installationUnchanged']
    (output/'result.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in summary.items() if k != 'results'}, indent=2))
    for record in summary['results']:
        print('  %-4s %-52s %s' % (record['kind'], record['file'], record['detail']))
    return summary['passed']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sys.exit(0 if probe(args.installation.resolve(strict=True),
                        args.output.resolve()) else 1)
