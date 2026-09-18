#!/usr/bin/env python3
"""Pose, skin, reduce and place tags on every retail Enemy Territory skeletal model.

Pairs every installed MDM mesh with every installed MDX skeleton, runs the
renderer's own pose, skinning, detail-reduction, collapse and tag arithmetic
over them, and reports what came out. Models are extracted to a scratch
directory; the installation is never written to.

This is asset arithmetic, not rendering: nothing is drawn and no image is
produced. It answers whether the numbers the draw path would submit are finite,
inside the bounds the asset itself declares, and indexable.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from probe_wolfet_models import extract  # noqa: E402

HARNESS = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
const vec3_t vec3_origin = {0,0,0};
/*MATH*/
#include "code/renderercommon/tr_wolfet_pose.h"
#include "code/renderercommon/tr_wolfet_lod.h"

static std::vector<unsigned char> Read(const char *path) {
 std::vector<unsigned char> bytes;
 std::FILE *file = std::fopen(path, "rb");
 if (!file) return bytes;
 std::fseek(file, 0, SEEK_END);
 const long length = std::ftell(file);
 std::fseek(file, 0, SEEK_SET);
 if (length > 0) {
  bytes.resize((size_t)length);
  if (std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) bytes.clear();
 }
 std::fclose(file);
 return bytes;
}

struct Report {
 long long poses = 0, vertices = 0, tags = 0, triangles = 0;
 /* At rest -- a frame against itself with no torso rotation asked for -- every
  * value the pose produces must sit inside the bounds the frame itself
  * declares, and a tag's axis must still be a rotation. */
 double restOutside = 0, restTagAxis = 0;
 /* Turned -- a quarter turn of the torso, blended halfway between two frames.
  * ET scales its torso rotation towards the identity by each bone's weight,
  * which is a straight blend and not a rotation, and the authored bounds
  * describe the unturned pose, so neither figure is expected to be zero here.
  * They are reported, not asserted. */
 double turnedOutside = 0, turnedTagAxis = 0;
 const char *failure = nullptr;
};

static void Note(Report &report, const char *why) { if (!report.failure) report.failure = why; }

/* The shipped skeletons write the greater corner of a frame's box first, so
 * the pair is ordered here before it is used as a range. */
static void Contain(const fnWolfETFrame_t &frame, const float point[3], double &worstOutside) {
 for (int i = 0; i < 3; ++i) {
  const double a = frame.bounds[0][i], b = frame.bounds[1][i];
  const double low = a < b ? a : b, high = a < b ? b : a;
  const double under = low - point[i], over = point[i] - high;
  const double worst = under > over ? under : over;
  if (worst > worstOutside) worstOutside = worst;
 }
}

static void CountBoundsOrder(const fnWolfETSkeletonView_t &skeleton,
 long long &greaterFirst, long long &lesserFirst) {
 for (uint32_t f = 0; f < skeleton.numFrames; ++f) {
  fnWolfETFrame_t frame;
  int greater = 0, lesser = 0;
  if (!FnWolfET_Frame(&skeleton, f, &frame)) return;
  for (int i = 0; i < 3; ++i) {
   if (frame.bounds[0][i] > frame.bounds[1][i]) ++greater;
   else if (frame.bounds[0][i] < frame.bounds[1][i]) ++lesser;
  }
  if (greater && !lesser) ++greaterFirst;
  else if (lesser && !greater) ++lesserFirst;
 }
}

static void AxisError(const float axis[3][3], double &worst) {
 for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
  double dot = 0;
  for (int k = 0; k < 3; ++k) dot += (double)axis[r][k] * axis[c][k];
  const double error = fabs(dot - (r == c ? 1.0 : 0.0));
  if (error > worst) worst = error;
 }
}

static void PoseAllFrames(const fnWolfETSkeletonView_t &skeleton, Report &report) {
 fnWolfETPoseParams_t params;
 memset(&params, 0, sizeof(params));
 params.frameView = params.oldFrameView = &skeleton;
 params.torsoView = params.oldTorsoView = &skeleton;
 for (int i = 0; i < 3; ++i) params.torsoAxis[i][i] = 1;
 for (uint32_t f = 0; f < skeleton.numFrames; ++f) {
  fnWolfETPose_t pose;
  fnWolfETFrame_t frame;
  params.frame = params.oldFrame = params.torsoFrame = params.oldTorsoFrame = f;
  if (!FnWolfET_Frame(&skeleton, f, &frame)) { Note(report, "frame refused"); return; }
  if (!FnWolfET_DecodePose(&params, &pose)) { Note(report, "pose refused"); return; }
  ++report.poses;
  for (uint32_t b = 0; b < pose.numBones; ++b) Contain(frame, pose.bones[b].origin, report.restOutside);
 }
}

static void DrawPair(const fnWolfETMeshView_t &mesh, const fnWolfETSkeletonView_t &skeleton,
 int turned, Report &report) {
 const uint32_t step = skeleton.numFrames > 16 ? skeleton.numFrames / 16 : 1;
 double &outside = turned ? report.turnedOutside : report.restOutside;
 double &tagAxis = turned ? report.turnedTagAxis : report.restTagAxis;
 for (uint32_t f = 0; f < skeleton.numFrames; f += step) {
  const uint32_t next = f + 1 < skeleton.numFrames ? f + 1 : f;
  fnWolfETPoseParams_t params;
  fnWolfETPose_t pose;
  fnWolfETFrame_t frame;
  memset(&params, 0, sizeof(params));
  params.frameView = params.oldFrameView = &skeleton;
  params.torsoView = params.oldTorsoView = &skeleton;
  if (turned) {
   /* A quarter turn of the torso, blended halfway between two frames: the two
    * things ET's own entity asks for that a rest pose never exercises. */
   params.torsoAxis[0][1] = 1; params.torsoAxis[1][0] = -1; params.torsoAxis[2][2] = 1;
   params.frame = next; params.oldFrame = f;
   params.torsoFrame = next; params.oldTorsoFrame = f;
   params.backlerp = params.torsoBacklerp = 0.5f;
  } else {
   for (int i = 0; i < 3; ++i) params.torsoAxis[i][i] = 1;
   params.frame = params.oldFrame = params.torsoFrame = params.oldTorsoFrame = f;
  }
  if (!FnWolfET_Frame(&skeleton, params.frame, &frame)) { Note(report, "frame refused"); return; }
  if (!FnWolfET_DecodePose(&params, &pose)) { Note(report, "pose refused"); return; }
  ++report.poses;
  for (uint32_t s = 0; s < mesh.numSurfaces; ++s) {
   fnWolfETMeshSurfaceView_t surface;
   std::vector<fnWolfETVertexView_t> vertices;
   std::vector<uint32_t> collapse;
   uint32_t offset, i;
   if (!FnWolfET_MeshSurface(&mesh, s, &surface)) { Note(report, "surface refused"); return; }
   vertices.resize(surface.numVerts);
   offset = surface.vertices;
   for (i = 0; i < surface.numVerts; ++i) {
    if (!FnWolfET_MeshVertexAt(&surface, offset, &vertices[i])) {
     Note(report, "vertex refused"); return;
    }
    offset = vertices[i].next;
   }
   for (i = 0; i < surface.numVerts; ++i) {
    float xyz[3], normal[3];
    if (!FnWolfET_SkinVertex(&vertices[i], &pose, xyz, normal)) {
     Note(report, "skin refused"); return;
    }
    Contain(frame, xyz, outside);
    ++report.vertices;
   }
   /* Every detail level the viewer's setting can ask for must still index. */
   collapse.resize(surface.numVerts ? surface.numVerts : 1);
   for (int tenth = 0; tenth <= 10; ++tenth) {
    const float fraction = (float)tenth / 10.0f;
    for (int dead = 0; dead < 2; ++dead) {
     const uint32_t retained = FnWolfET_LodVertexCount(&surface, fraction, dead);
     if (!retained) continue;
     if (!FnWolfET_CollapseVertices(&surface, retained, collapse.data(), collapse.size())) {
      Note(report, "collapse refused"); return;
     }
     for (i = 0; i < surface.numTriangles; ++i) {
      const unsigned char *t = surface.data + surface.triangles + (size_t)i * 12;
      for (int c = 0; c < 3; ++c) {
       const uint32_t index = FnWolfET_U32(t + c * 4);
       if (index >= surface.numVerts || collapse[index] >= retained) {
        Note(report, "collapsed index out of range"); return;
       }
      }
      ++report.triangles;
     }
    }
   }
  }
  for (uint32_t t = 0; t < mesh.numTags; ++t) {
   float origin[3], axis[3][3];
   if (!FnWolfET_TagOrientation(&mesh, t, &pose, origin, axis)) {
    Note(report, "tag refused"); return;
   }
   /* A tag's own axis and an unturned bone are both rotations, so what comes
    * out at rest must be one too. */
   AxisError(axis, tagAxis);
   Contain(frame, origin, outside);
   ++report.tags;
  }
 }
}

int main(int argc, char **argv) {
 if (argc < 3) return 2;
 const std::vector<unsigned char> meshBytes = Read(argv[1]);
 const std::vector<unsigned char> skeletonBytes = Read(argv[2]);
 fnWolfETMeshView_t mesh;
 fnWolfETSkeletonView_t skeleton;
 Report report;
 if (meshBytes.empty() || skeletonBytes.empty()) { std::printf("unreadable\n"); return 1; }
 if (!FnWolfET_Mesh(meshBytes.data(), meshBytes.size(), &mesh)) {
  std::printf("mesh refused\n"); return 1;
 }
 if (!FnWolfET_Skeleton(skeletonBytes.data(), skeletonBytes.size(), &skeleton)) {
  std::printf("skeleton refused\n"); return 1;
 }
 long long greaterFirst = 0, lesserFirst = 0;
 CountBoundsOrder(skeleton, greaterFirst, lesserFirst);
 PoseAllFrames(skeleton, report);
 if (!report.failure) DrawPair(mesh, skeleton, 0, report);
 if (!report.failure) DrawPair(mesh, skeleton, 1, report);
 if (report.failure) { std::printf("%s\n", report.failure); return 1; }
 std::printf("ok poses=%lld vertices=%lld triangles=%lld tags=%lld"
  " rest=%.4f/%.7f turned=%.4f/%.6f boundsorder=%lld/%lld\n",
  report.poses, report.vertices, report.triangles, report.tags,
  report.restOutside, report.restTagAxis, report.turnedOutside, report.turnedTagAxis,
  greaterFirst, lesserFirst);
 return 0;
}
'''


def function(source, signature):
    start = source.index(signature)
    depth, i = 0, source.index('{', start)
    while True:
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                return source[start:i + 1] + '\n'
        i += 1


def harness():
    math = (ROOT/'code/qcommon/q_math.c').read_text(encoding='utf-8')
    extracted = ''
    for signature in ('void AngleVectors(', 'void AnglesToAxis('):
        extracted += function(math, signature)
    return HARNESS.replace('/*MATH*/', extracted)


def build(folder):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('a C++17 compiler is required')
    source = folder/'draw.cpp'
    executable = folder/('draw.exe' if os.name == 'nt' else 'draw')
    source.write_text(harness(), encoding='utf-8')
    target = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32']
    built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT),
        str(source), '-o', str(executable)], cwd=ROOT, capture_output=True,
        text=True, timeout=300)
    if built.returncode:
        raise SystemExit(built.stdout + built.stderr)
    return executable


def probe(installation, output):
    before = {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
              for p in installation.rglob('*') if p.is_file()}
    output.mkdir(parents=True, exist_ok=True)
    results = []
    with tempfile.TemporaryDirectory(dir=output) as directory:
        folder = Path(directory)
        executable = build(folder)
        found = extract(installation, folder)
        meshes = [(entry, path) for _, entry, path, suffix in found if suffix == '.mdm']
        skeletons = [(entry, path) for _, entry, path, suffix in found if suffix == '.mdx']
        for meshName, meshPath in sorted(meshes):
            for skeletonName, skeletonPath in sorted(skeletons):
                run = subprocess.run([str(executable), str(meshPath), str(skeletonPath)],
                    capture_output=True, text=True, timeout=900)
                results.append({
                    'mesh': meshName, 'skeleton': skeletonName,
                    'posed': run.returncode == 0,
                    'detail': run.stdout.strip() or run.stderr.strip(),
                })
    after = {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
             for p in installation.rglob('*') if p.is_file()}
    rest = [r['detail'].split('rest=')[1].split()[0]
            for r in results if r['posed'] and 'rest=' in r['detail']]
    outside = [float(v.split('/')[0]) for v in rest]
    tagAxis = [float(v.split('/')[1]) for v in rest]
    order = [r['detail'].split('boundsorder=')[1].split()[0]
             for r in results if r['posed'] and 'boundsorder=' in r['detail']]
    greaterFirst = sum(int(v.split('/')[0]) for v in order)
    lesserFirst = sum(int(v.split('/')[1]) for v in order)
    summary = {
        'profile': 'wolfet',
        'evidence': 'Assets: installed MDM/MDX pose, skin, detail and tags',
        'installation': str(installation),
        'installationUnchanged': before == after,
        'pairs': len(results),
        'posed': sum(1 for r in results if r['posed']),
        'worstRestOutsideBounds': max(outside) if outside else None,
        'worstRestTagAxisError': max(tagAxis) if tagAxis else None,
        'frameBoundsGreaterCornerFirst': greaterFirst,
        'frameBoundsLesserCornerFirst': lesserFirst,
        'results': results,
    }
    summary['passed'] = bool(results) and summary['posed'] == summary['pairs'] \
        and summary['installationUnchanged']
    (output/'result.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in summary.items() if k != 'results'}, indent=2))
    for record in results:
        print('  %-46s %-40s %s' % (record['mesh'], record['skeleton'], record['detail']))
    return summary['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    arguments = parser.parse_args()
    return 0 if probe(arguments.installation, arguments.output) else 1


if __name__ == '__main__':
    raise SystemExit(main())
