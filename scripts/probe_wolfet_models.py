#!/usr/bin/env python3
"""Parse every MDM and MDX in an installed Enemy Territory with both fnTech3 readers.

Extracts the models from the installation's paks into a scratch directory, never
writing to the installation, runs both the compatibility reader and the
renderer's own byte reader over each file, and reports whether they agree.
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
#include "code/renderercommon/tr_wolfet_model_format.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

// The renderer reads these files with its own byte reader, separate from the
// compatibility one. Both must accept the same files and agree on their counts,
// or the renderer would be drawing something the decoder never verified.
static int RendererMesh(const std::vector<unsigned char> &bytes, int &surfaces,
 int &tags, int &verts, int &tris) {
 fnWolfETMeshView_t view;
 if (!FnWolfET_Mesh(bytes.data(), bytes.size(), &view)) return 0;
 surfaces = (int)view.numSurfaces;
 tags = (int)view.numTags;
 verts = tris = 0;
 for (uint32_t i = 0; i < view.numSurfaces; ++i) {
  fnWolfETMeshSurfaceView_t surface;
  if (!FnWolfET_MeshSurface(&view, i, &surface)) return 0;
  for (uint32_t v = 0; v < surface.numVerts; ++v) {
   fnWolfETVertexView_t vertex;
   if (!FnWolfET_MeshVertex(&surface, v, &vertex)) return 0;
   for (uint32_t w = 0; w < vertex.numWeights; ++w) {
    fnWolfETWeight_t weight;
    if (!FnWolfET_MeshWeight(&vertex, w, &weight)) return 0;
    // ET's own header calls this an index into the surface's bone references,
    // but its renderer and its shipped assets both use the global bone list.
    if (weight.bone >= FN_WOLFET_MAX_BONES) return 0;
   }
  }
  verts += (int)surface.numVerts;
  tris += (int)surface.numTriangles;
 }
 for (uint32_t i = 0; i < view.numTags; ++i) {
  fnWolfETTag_t tag;
  if (!FnWolfET_MeshTag(&view, i, &tag, nullptr)) return 0;
 }
 return 1;
}

static int RendererSkeleton(const std::vector<unsigned char> &bytes, int &frames,
 int &bones, int &torso) {
 fnWolfETSkeletonView_t view;
 if (!FnWolfET_Skeleton(bytes.data(), bytes.size(), &view)) return 0;
 frames = (int)view.numFrames;
 bones = (int)view.numBones;
 torso = view.torsoParent;
 for (uint32_t f = 0; f < view.numFrames; ++f) {
  fnWolfETFrame_t frame;
  if (!FnWolfET_Frame(&view, f, &frame)) return 0;
  for (uint32_t b = 0; b < view.numBones; ++b) {
   fnWolfETBonePose_t pose;
   if (!FnWolfET_BonePose(&view, f, b, &pose)) return 0;
  }
 }
 for (uint32_t b = 0; b < view.numBones; ++b) {
  fnWolfETBone_t bone;
  if (!FnWolfET_Bone(&view, b, &bone, nullptr)) return 0;
 }
 return 1;
}

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
  int frames = 0, bones = 0, torso = 0;
  if (!RendererSkeleton(bytes, frames, bones, torso)) {
   std::printf("renderer reader refused\n"); return 1;
  }
  if (frames != mdx.numFrames || bones != mdx.numBones || torso != mdx.torsoParent) {
   std::printf("readers disagree\n"); return 1;
  }
  std::printf("ok version=%d frames=%d bones=%d torso=%d renderer=agrees\n",
   mdx.version, mdx.numFrames, mdx.numBones, mdx.torsoParent);
  return 0;
 }
 Mdm mdm;
 const Error error = ParseMdm(bytes.data(), bytes.size(), 0, mdm);
 if (error != Error::None) { std::printf("%s\n", ErrorName(error)); return 1; }
 int surfaces = 0, tags = 0, verts = 0, tris = 0;
 if (!RendererMesh(bytes, surfaces, tags, verts, tris)) {
  std::printf("renderer reader refused\n"); return 1;
 }
 if (surfaces != mdm.numSurfaces || tags != mdm.numTags ||
  verts != mdm.totalVerts || tris != mdm.totalTriangles) {
  std::printf("readers disagree\n"); return 1;
 }
 std::printf("ok version=%d surfaces=%d tags=%d verts=%d tris=%d maxbone=%d renderer=agrees\n",
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
