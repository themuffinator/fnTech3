#!/usr/bin/env python3
"""Compare the MDS byte reader with reference records and legitimate installed files.

This is format evidence, not skeletal animation or live rendering. Assets are
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

FIXTURE = r'''
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
#include "code/renderercommon/tr_mds_format.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
int main(void) {
    unsigned char length[4];
    unsigned count = 0;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    while (fread(length, 1, 4, stdin) == 4) {
        const uint32_t bytes = FnMds_U32(length);
        unsigned char *data, *unaligned;
        const mdsHeader_t *header;
        fnMdsView_t view;
        uint32_t frameIndex, boneIndex, tagIndex, surfaceIndex, surfaceOffset;
        uint64_t compressedBones = 0, vertices = 0, weights = 0, triangles = 0;
        if (!bytes) break;
        assert(bytes >= sizeof(mdsHeader_t) && bytes < 32 * 1024 * 1024);
        data = (unsigned char *)malloc(bytes); unaligned = (unsigned char *)malloc((size_t)bytes + 1);
        assert(data && unaligned && fread(data, 1, bytes, stdin) == bytes);
        memcpy(unaligned + 1, data, bytes); header = (const mdsHeader_t *)data;
        if (!FnMds_Open(unaligned + 1, bytes, &view)) {
            fprintf(stderr, "MDS validation rejected model %u\n", count); return 2;
        }
        assert(view.numFrames == header->numFrames && view.numBones == header->numBones);
        assert(view.numSurfaces == header->numSurfaces && view.numTags == header->numTags);
        assert(view.torsoParent == header->torsoParent && view.bytes == header->ofsEnd);
        assert(view.lodScale == header->lodScale && view.lodBias == header->lodBias);
        assert(view.frameStride == sizeof(mdsFrame_t) + (header->numBones - 1) * sizeof(mdsBoneFrameCompressed_t));
        for (frameIndex = 0; frameIndex < view.numFrames; ++frameIndex) {
            const mdsFrame_t *reference = (const mdsFrame_t *)(data + header->ofsFrames + frameIndex * view.frameStride);
            fnMdsFrame_t frame;
            assert(FnMds_Frame(&view, frameIndex, &frame));
            assert(!memcmp(frame.bounds, reference->bounds, sizeof(frame.bounds)));
            assert(!memcmp(frame.origin, reference->localOrigin, sizeof(frame.origin)));
            assert(!memcmp(frame.parentOffset, reference->parentOffset, sizeof(frame.parentOffset)));
            assert(frame.radius == reference->radius);
            for (boneIndex = 0; boneIndex < view.numBones; ++boneIndex) {
                fnMdsCompressedBone_t bone;
                int j;
                assert(FnMds_CompressedBone(&view, frameIndex, boneIndex, &bone));
                for (j = 0; j < 3; ++j) assert(bone.angles[j] == reference->bones[boneIndex].angles[j]);
                for (j = 0; j < 2; ++j) assert(bone.offsetAngles[j] == reference->bones[boneIndex].ofsAngles[j]);
                ++compressedBones;
            }
        }
        for (boneIndex = 0; boneIndex < view.numBones; ++boneIndex) {
            const mdsBoneInfo_t *reference = (const mdsBoneInfo_t *)(data + header->ofsBones) + boneIndex;
            fnMdsBone_t bone;
            assert(FnMds_Bone(&view, boneIndex, &bone));
            assert(bone.parent == reference->parent && bone.torsoWeight == reference->torsoWeight);
            assert(bone.parentDist == reference->parentDist && bone.flags == reference->flags);
        }
        for (tagIndex = 0; tagIndex < view.numTags; ++tagIndex) {
            const mdsTag_t *reference = (const mdsTag_t *)(data + header->ofsTags) + tagIndex;
            fnMdsTag_t tag;
            assert(FnMds_Tag(&view, tagIndex, &tag));
            assert(tag.bone == reference->boneIndex && tag.torsoWeight == reference->torsoWeight);
        }
        surfaceOffset = header->ofsSurfaces;
        for (surfaceIndex = 0; surfaceIndex < view.numSurfaces; ++surfaceIndex) {
            const mdsSurface_t *reference = (const mdsSurface_t *)(data + surfaceOffset);
            const mdsVertex_t *referenceVertex = (const mdsVertex_t *)((const byte *)reference + reference->ofsVerts);
            fnMdsSurfaceView_t surface;
            uint32_t vertexIndex, triangleIndex, referenceIndex, vertexOffset;
            assert(FnMds_Surface(&view, surfaceOffset, &surface));
            assert(surface.numVerts == reference->numVerts && surface.numTriangles == reference->numTriangles);
            assert(surface.minLod == reference->minLod && surface.bytes == reference->ofsEnd);
            assert(surface.numBoneReferences == reference->numBoneReferences);
            assert(!strcmp((const char *)surface.data + 4, reference->name));
            assert(!strcmp((const char *)surface.data + 68, reference->shader));
            vertexOffset = surface.vertices;
            for (vertexIndex = 0; vertexIndex < surface.numVerts; ++vertexIndex) {
                fnMdsVertexView_t vertex;
                uint32_t weightIndex;
                assert(FnMds_Vertex(&surface, vertexOffset, &vertex));
                assert(vertex.numWeights == referenceVertex->numWeights);
                assert(!memcmp(vertex.normal, referenceVertex->normal, sizeof(vertex.normal)));
                assert(!memcmp(vertex.st, referenceVertex->texCoords, sizeof(vertex.st)));
                for (weightIndex = 0; weightIndex < vertex.numWeights; ++weightIndex) {
                    const mdsWeight_t *referenceWeight = referenceVertex->weights + weightIndex;
                    fnMdsWeight_t weight;
                    assert(FnMds_Weight(&vertex, weightIndex, &weight));
                    assert(weight.bone == referenceWeight->boneIndex && weight.weight == referenceWeight->boneWeight);
                    assert(!memcmp(weight.offset, referenceWeight->offset, sizeof(weight.offset))); ++weights;
                }
                assert(FnMds_U32(surface.data + surface.collapseMap + vertexIndex * 4) ==
                    ((const uint32_t *)((const byte *)reference + reference->ofsCollapseMap))[vertexIndex]);
                vertexOffset = vertex.next;
                referenceVertex = (const mdsVertex_t *)(referenceVertex->weights + referenceVertex->numWeights); ++vertices;
            }
            for (triangleIndex = 0; triangleIndex < surface.numTriangles; ++triangleIndex) {
                const mdsTriangle_t *triangle = (const mdsTriangle_t *)((const byte *)reference + reference->ofsTriangles) + triangleIndex;
                int j;
                for (j = 0; j < 3; ++j) assert(FnMds_U32(surface.data + surface.triangles + triangleIndex * 12 + j * 4) == triangle->indexes[j]);
                ++triangles;
            }
            for (referenceIndex = 0; referenceIndex < surface.numBoneReferences; ++referenceIndex)
                assert(FnMds_U32(surface.data + surface.boneReferences + referenceIndex * 4) ==
                    ((const uint32_t *)((const byte *)reference + reference->ofsBoneReferences))[referenceIndex]);
            surfaceOffset += reference->ofsEnd;
        }
        printf("{\"index\":%u,\"frames\":%u,\"bones\":%u,\"surfaces\":%u,\"tags\":%u,"
               "\"compressedBones\":%llu,\"vertices\":%llu,\"weights\":%llu,\"triangles\":%llu}\n",
               count++, view.numFrames, view.numBones, view.numSurfaces, view.numTags,
               (unsigned long long)compressedBones, (unsigned long long)vertices,
               (unsigned long long)weights, (unsigned long long)triangles);
        free(data); free(unaligned);
    }
    return 0;
}
'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
    report = {'kind': 'CPU MDS format/reference probe; not animation or rendering',
              'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
              'implementationSha256': {name: digest(ROOT / name) for name in
                  ('code/renderercommon/tr_mds_format.h', 'code/compat/rtcw_mds_layout.def')},
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
        paths = {'q_shared.h': common / 'q_shared.h', 'qfiles.h': reference / 'qcommon/qfiles.h'}
        texts = {name: path.read_text(encoding='utf-8') for name, path in paths.items()}
        source = '\n'.join(text[:text.index('*/') + 2] for text in texts.values()) + FIXTURE
        with tempfile.TemporaryDirectory(prefix='mds-reference-', dir=report_path.parent) as temp:
            folder = Path(temp)
            fixture_path = folder / 'probe.c'
            fixture_path.write_text(source, encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c11', '-O1', '-fno-strict-aliasing',
                       '-I', str(ROOT), '-I', str(common), '-I', str(reference / 'qcommon'),
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
