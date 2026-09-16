#!/usr/bin/env python3
"""Compare MDS detail selection and triangle reduction against RTCW SP/MP references.

This is CPU detail-selection evidence, not live rendering. Assets are
read into memory from installation archives. Only hashes and counts are saved.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'




def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_fixture(mode, animation):
    # Compile the reference functions/blocks unchanged only in the disposable
    # fixture, with the original file notices supplied by the caller.
    prelude = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdint.h>
#if defined(_MSC_VER)
#define int8_t fn_probe_int8_t
#endif
#if defined(_WIN64) && !defined(__WIN64__)
#define __WIN64__ 1
#endif
#include "q_shared.h"
#undef int8_t
#include "qfiles.h"
#include "tr_types.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_mds_format.h"
#include "code/renderercommon/tr_mds_lod.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static cvar_t scale, bias;
static cvar_t *r_lodscale = &scale, *r_lodbias = &bias;
static float ProjectRadius(float radius, vec3_t origin) { (void)origin; return radius; }
'''
    lod_function = 'RB_CalcMDSLod' if 'float RB_CalcMDSLod(' in animation else 'R_CalcMDSLod'
    source = prelude + '#define RB_ProjectRadius ProjectRadius\n' + function(animation, 'float ' + lod_function + '(')
    if lod_function != 'R_CalcMDSLod':
        source += '#define R_CalcMDSLod ' + lod_function + '\n'
    source += '\n#define GAME COM_GAME_RTCW_' + mode.upper() + '\n'
    # The original backend's vertex selection (including its double 0.35
    # comparison) and collapse/index block are observed independently.
    start = animation.index('if ( refent->reFlags & REFLAG_DEAD_LOD )', animation.index('void RB_SurfaceAnim('))
    end = animation.index('\n\t}', animation.index('if ( render_count > surface->numVerts )', start)) + 4
    selection = animation[start:end]
    start = animation.index('if ( render_count == surface->numVerts )', end)
    end = animation.index('\n\n//DBG_SHOWTIME', start)
    collapse = animation[start:end]
    source += r'''
static int ReferenceVertexCount(mdsSurface_t *surface, float lodScale, refEntity_t *refent) {
    int render_count;
''' + selection + r'''
    return render_count;
}
static int ReferenceTriangles(mdsSurface_t *surface, int render_count, int *output) {
    int collapse[MDS_MAX_VERTS], *pCollapse, *pCollapseMap;
    int *collapse_map = (int *)((byte *)surface + surface->ofsCollapseMap);
    int *triangles = (int *)((byte *)surface + surface->ofsTriangles);
    int indexes = surface->numTriangles * 3, j, p0, p1, p2;
    int baseVertex = 7, baseIndex = 0, *pIndexes = output;
    struct { int numIndexes; } tess = {0};
''' + collapse + r'''
    return tess.numIndexes;
}
int main(void) {
    unsigned char length[4]; unsigned count = 0;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    while (fread(length, 1, 4, stdin) == 4) {
        const uint32_t bytes = FnMds_U32(length);
        unsigned char *data; fnMdsView_t view;
        unsigned surfaceIndex, offset, fractions = 0, counts = 0, meshes = 0;
        unsigned long long indices = 0;
        if (!bytes) break;
        assert(bytes < 32 * 1024 * 1024);
        data = (unsigned char *)malloc(bytes); assert(data && fread(data, 1, bytes, stdin) == bytes);
        assert(FnMds_Open(data, bytes, &view));
        offset = view.surfaces;
        for (surfaceIndex = 0; surfaceIndex < view.numSurfaces; ++surfaceIndex) {
            fnMdsSurfaceView_t surface;
            mdsSurface_t *reference = (mdsSurface_t *)(data + offset);
            refEntity_t entity; const float projected[] = {-1, 0, 0.000001f, 0.001f, 0.125f, 0.5f, 1};
            int flag, p, s, b, retained;
            assert(FnMds_Surface(&view, offset, &surface)); memset(&entity, 0, sizeof(entity));
            for (flag = 0; flag < 8; ++flag) for (p = 0; p < 7; ++p) for (s = -1; s <= 5; s += 2) for (b = -2; b <= 4; b += 2) {
                float expected, actual;
                entity.reFlags = (flag & 1 ? 8 : 0) | (flag & 2 ? 32 : 0) | (flag & 4 ? 16 : 0);
                scale.value = (float)s; bias.value = (float)b;
                expected = R_CalcMDSLod(&entity, entity.origin, projected[p], view.lodBias, view.lodScale);
                actual = FnMds_LodFraction(GAME, entity.reFlags, projected[p], scale.value, bias.value, view.lodScale, view.lodBias);
                assert(expected == actual); ++fractions;
                assert(ReferenceVertexCount(reference, expected, &entity) == FnMds_LodVertexCount(&surface, actual, entity.reFlags & 32));
                ++counts;
            }
            for (retained = 1; retained <= (int)surface.numVerts; ++retained) {
                uint32_t map[MDS_MAX_VERTS];
                int *triangles = (int *)malloc((size_t)surface.numTriangles * 12 + 4);
                int expectedCount, actualCount = 0; uint32_t triangle;
                assert(triangles && FnMds_CollapseVertices(&surface, retained, map, MDS_MAX_VERTS));
                expectedCount = ReferenceTriangles(reference, retained, triangles);
                for (triangle = 0; triangle < surface.numTriangles; ++triangle) {
                    const unsigned char *p = surface.data + surface.triangles + (size_t)triangle * 12;
                    const uint32_t a = map[FnMds_U32(p)], b = map[FnMds_U32(p + 4)], c = map[FnMds_U32(p + 8)];
                    if (retained != surface.numVerts && (a == b || b == c || c == a)) continue;
                    assert(actualCount + 3 <= expectedCount);
                    assert(triangles[actualCount++] == a + 7 && triangles[actualCount++] == b + 7 && triangles[actualCount++] == c + 7);
                }
                assert(actualCount == expectedCount); indices += actualCount; ++meshes; free(triangles);
            }
            offset += (uint32_t)surface.bytes;
        }
        printf("{\"index\":%u,\"surfaces\":%u,\"fractions\":%u,\"vertexCounts\":%u,\"reducedMeshes\":%u,\"indices\":%llu}\n",
               count++, view.numSurfaces, fractions, counts, meshes, indices);
        free(data);
    }
    return 0;
}
'''
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('installation', 'sp-source', 'mp-source', 'iortcw-source', 'report'):
        parser.add_argument('--' + name, type=Path, required=name != 'iortcw-source')
    parser.add_argument('--compiler', default=shutil.which('clang'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve()
    report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    triple = subprocess.run([args.compiler, *target, '-dumpmachine'], cwd=ROOT,
                            capture_output=True, text=True, check=True).stdout.strip()
    report = {'kind': 'CPU MDS LOD fraction and all retained-vertex triangle lists; no live rendering',
              'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
              'implementationSha256': {name: digest(ROOT / name) for name in
                  ('code/renderercommon/tr_mds_format.h', 'code/renderercommon/tr_mds_lod.h')},
              'installation': str(args.installation.resolve(strict=True)), 'assets': [], 'results': []}
    payload = bytearray()
    for archive in sorted((args.installation / 'main').glob('*.pk3')):
        with zipfile.ZipFile(archive) as pak:
            for entry in pak.infolist():
                if not entry.filename.lower().endswith('.mds'):
                    continue
                data = pak.read(entry)
                report['assets'].append({'archive': archive.name, 'name': entry.filename, 'bytes': len(data),
                                         'sha256': hashlib.sha256(data).hexdigest()})
                payload += struct.pack('<I', len(data)) + data
    if not report['assets']:
        parser.error('No MDS assets in the supplied installation/main/*.pk3')
    payload += b'\0\0\0\0'
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, variant, reference in references:
        common = reference / ('qcommon' if variant == 'iortcw' else 'game')
        paths = {'q_shared.h': common / 'q_shared.h', 'qfiles.h': reference / 'qcommon/qfiles.h', 'tr_animation.c': reference / 'renderer/tr_animation.c',
                 'tr_types.h': reference / ('renderer' if variant == 'iortcw' else 'cgame') / 'tr_types.h'}
        texts = {name: path.read_text(encoding='utf-8') for name, path in paths.items()}
        source = '\n'.join(text[:text.index('*/') + 2] for text in texts.values()) + make_fixture(mode, texts['tr_animation.c'])
        with tempfile.TemporaryDirectory(prefix='mds-reference-', dir=report_path.parent) as temp:
            folder = Path(temp)
            fixture_path = folder / 'probe.c'
            fixture_path.write_text(source, encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c11', '-O1', '-fno-strict-aliasing',
                       '-I', str(ROOT), '-I', str(common), '-I', str(reference / 'qcommon'), '-I', str(paths['tr_types.h'].parent),
                       str(fixture_path), '-o', str(executable)]
            if args.sanitize:
                command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
            if os.name != 'nt' and variant == 'iortcw':
                command.append('-DARCH_STRING="' + triple.split('-')[0] + '"')
            build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            result = {'mode': mode, 'reference': variant, 'source': str(reference),
                      'sourceSha256': {name: digest(path) for name, path in paths.items()},
                      'buildExitCode': build.returncode, 'buildOutput': build.stdout + build.stderr}
            if build.returncode == 0:
                environment = os.environ.copy()
                if args.sanitize:
                    environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], input=payload, cwd=ROOT, capture_output=True,
                                     timeout=120, env=environment, **options)
                result.update(exitCode=run.returncode, diagnostics=run.stderr.decode('utf-8', errors='replace'),
                              models=[json.loads(line) for line in run.stdout.splitlines()])
                result['passed'] = run.returncode == 0 and len(result['models']) == len(report['assets'])
            else:
                result['passed'] = False
            report['results'].append(result)
            report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
            print(f'{variant} {mode}: {"PASS" if result["passed"] else "FAIL"}; '
                  f'{len(result.get("models", []))} models', flush=True)
    return 0 if all(result['passed'] for result in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
