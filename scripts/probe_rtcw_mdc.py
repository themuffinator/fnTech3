#!/usr/bin/env python3
"""Compare the MDC reader/animation decoder with original RTCW and iortcw.

Reads legitimate installed models directly from their pk3 archives into memory.
Reference code is compiled only in disposable fixtures with its notices intact;
reports contain identities/counts/errors, never model geometry or game modules.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
HEADERS = ('code/renderercommon/tr_mdc_format.h', 'code/renderercommon/tr_mdc_decode.h')

PRELUDE = r'''
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
#include "code/renderercommon/tr_mdc_decode.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK 1023
static float r_anormals[256][3] = {
#include "anorms256.h"
};
static struct { float sinTable[1024]; } tr;
typedef struct { struct { int frame, oldframe; } e; } probeEntity_t;
static probeEntity_t entity;
static struct { int numVertexes; float xyz[4096][4], normal[4096][4]; } tess;
static struct { probeEntity_t *currentEntity; } backEnd = {&entity};
'''

CHECKS = r'''
int main(void) {
    fnMdcNormals_t normals;
    unsigned char lengthBytes[4];
    unsigned count = 0;
    int i;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    FnMdc_InitNormals(&normals);
    for (i = 0; i < 1024; ++i) tr.sinTable[i] = sin(DEG2RAD(i * 360.0f / (float)(FUNCTABLE_SIZE - 1)));
    assert(!memcmp(normals.directions, r_anormals, sizeof(r_anormals)));
    assert(!memcmp(normals.sine, tr.sinTable, sizeof(tr.sinTable)));
    while (fread(lengthBytes, 1, 4, stdin) == 4) {
        uint32_t bytes = FnMdc_U32(lengthBytes), offset, frame, tag, surfaceIndex;
        unsigned char *data, *unaligned;
        fnMdcView_t view;
        uint64_t comparisons = 0, tagComparisons = 0;
        double maxPosition = 0, maxNormal = 0;
        if (!bytes) break;
        assert(bytes < 32 * 1024 * 1024);
        data = (unsigned char *)malloc(bytes); unaligned = (unsigned char *)malloc((size_t)bytes + 1);
        assert(data && unaligned && fread(data, 1, bytes, stdin) == bytes);
        memcpy(unaligned + 1, data, bytes);
        if (!FnMdc_Open(unaligned + 1, bytes, &view)) {
            fprintf(stderr, "MDC validation rejected input %u\n", count); return 2;
        }
        for (frame = 0; frame < view.numFrames; ++frame) {
            const md3Frame_t *reference = (const md3Frame_t *)(data + view.frames) + frame;
            fnMdcFrame_t decoded;
            assert(FnMdc_Frame(&view, frame, &decoded));
            assert(!memcmp(decoded.bounds, reference->bounds, sizeof(decoded.bounds)));
            assert(!memcmp(decoded.origin, reference->localOrigin, sizeof(decoded.origin)));
            assert(decoded.radius == reference->radius);
            for (tag = 0; tag < view.numTags; ++tag) {
                const mdcTag_t *referenceTag = (const mdcTag_t *)(data + view.tags) + frame * view.numTags + tag;
                float xyz[3], angles[3];
                assert(FnMdc_Tag(&view, frame, tag, xyz, angles));
                for (i = 0; i < 3; ++i) {
                    assert(xyz[i] == (float)(referenceTag->xyz[i] * MD3_XYZ_SCALE));
                    assert(angles[i] == (float)(referenceTag->angles[i] * MDC_TAG_ANGLE_SCALE));
                }
                ++tagComparisons;
            }
        }
        offset = view.surfaces;
        for (surfaceIndex = 0; surfaceIndex < view.numSurfaces; ++surfaceIndex) {
            fnMdcSurfaceView_t surface;
            mdcSurface_t *reference = (mdcSurface_t *)(data + offset);
            assert(FnMdc_Surface(&view, offset, &surface));
            assert(surface.numVerts + 3 < 4096); // Capacity of this comparison fixture only.
            for (frame = 0; frame < view.numFrames; ++frame) {
                int sample;
                entity.e.frame = frame; entity.e.oldframe = frame ? frame - 1 : view.numFrames - 1;
                for (sample = 0; sample < 3; ++sample) {
                    const float backlerp = sample * 0.375f;
                    uint32_t vertex;
                    tess.numVertexes = 3;
                    LerpCMeshVertexes(reference, backlerp);
                    for (vertex = 0; vertex < surface.numVerts; ++vertex) {
                        float xyz[3], normal[3];
                        assert(FnMdc_AnimatedVertex(&surface, &normals, entity.e.oldframe, frame,
                            backlerp, vertex, xyz, normal));
                        for (i = 0; i < 3; ++i) {
                            const double positionError = fabs(xyz[i] - tess.xyz[vertex + 3][i]);
                            const double normalError = fabs(normal[i] - tess.normal[vertex + 3][i]);
                            if (positionError > maxPosition) maxPosition = positionError;
                            if (normalError > maxNormal) maxNormal = normalError;
                            if (positionError > 0.0001 || normalError > 0.000002) {
                                fprintf(stderr, "Mismatch: model=%u surface=%u frame=%u vertex=%u sample=%d xyz=%g normal=%g\n",
                                    count, surfaceIndex, frame, vertex, sample, positionError, normalError);
                                return 3;
                            }
                        }
                        ++comparisons;
                    }
                }
            }
            offset += (uint32_t)surface.bytes;
        }
        printf("{\"index\":%u,\"frames\":%u,\"surfaces\":%u,\"tags\":%u,\"vertexComparisons\":%llu,"
            "\"tagComparisons\":%llu,\"maxPositionError\":%.9g,\"maxNormalError\":%.9g}\n", count,
            view.numFrames, view.numSurfaces, view.numTags, (unsigned long long)comparisons,
            (unsigned long long)tagComparisons, maxPosition, maxNormal);
        free(data); free(unaligned); ++count;
    }
    return ferror(stdin) ? 4 : 0;
}
'''


def function(text: str, signature: str) -> str:
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw-source', type=Path)
    parser.add_argument('--compiler', default=shutil.which('clang'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve()
    report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    triple = subprocess.run([args.compiler, *target, '-dumpmachine'], cwd=ROOT,
                            capture_output=True, text=True, check=True).stdout.strip()
    instrumentation = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = {'kind': 'CPU MDC reference/installed-asset probe; not live rendering',
              'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
              'installation': str(args.installation.resolve(strict=True)),
              'implementationSha256': {name: digest(ROOT / name) for name in HEADERS},
              'assets': [], 'results': []}
    payload = bytearray()
    for archive in sorted((args.installation / 'main').glob('*.pk3')):
        with zipfile.ZipFile(archive) as pak:
            for entry in pak.infolist():
                if not entry.filename.lower().endswith('.mdc'):
                    continue
                data = pak.read(entry)
                report['assets'].append({'archive': archive.name, 'name': entry.filename, 'bytes': len(data),
                                         'sha256': hashlib.sha256(data).hexdigest()})
                payload += struct.pack('<I', len(data)) + data
    if not report['assets']:
        parser.error('No MDC assets found in the supplied installation/main/*.pk3')
    payload += b'\0\0\0\0'
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, variant, reference in references:
        common = reference / ('qcommon' if variant == 'iortcw' else 'game')
        files = {name: reference / 'renderer' / name for name in ('tr_local.h', 'tr_surface.c', 'anorms256.h')}
        files['q_math.c'] = common / 'q_math.c'
        files['q_shared.h'] = common / 'q_shared.h'
        files['qfiles.h'] = reference / 'qcommon/qfiles.h'
        texts = {name: path.read_text(encoding='utf-8') for name, path in files.items()}
        notices = '\n'.join(text[:text.index('*/') + 2] for text in texts.values())
        source = notices + PRELUDE
        for name in ('MDC_DIST_SCALE', 'MDC_MAX_OFS'):
            source += re.search(r'^#define ' + name + r'[^\n]*', texts['tr_local.h'], re.M)[0] + '\n'
        macro_start = texts['tr_local.h'].index('#define R_MDC_DecodeXyzCompressed')
        for line in texts['tr_local.h'][macro_start:].splitlines():
            source += line + '\n'
            if not line.rstrip().endswith('\\'):
                break
        source += function(texts['q_math.c'], 'vec_t VectorNormalize(')
        source += function(texts['tr_surface.c'], 'static void LerpCMeshVertexes(')
        with tempfile.TemporaryDirectory(prefix='mdc-reference-', dir=report_path.parent) as temp:
            folder = Path(temp)
            fixture = folder / 'probe.c'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            fixture.write_text(source + CHECKS, encoding='utf-8')
            command = [args.compiler, *target, *instrumentation, '-std=c11', '-O1', '-fno-strict-aliasing',
                       '-I', str(ROOT), '-I', str(common), '-I', str(reference / 'qcommon'),
                       '-I', str(reference / 'renderer'), str(fixture), '-o', str(executable)]
            if os.name != 'nt':
                if variant == 'iortcw':
                    command.append('-DARCH_STRING="' + triple.split('-')[0] + '"')
                command.append('-lm')
            build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            result = {'mode': mode, 'reference': variant, 'source': str(reference),
                      'sourceSha256': {name: digest(path) for name, path in files.items()},
                      'buildExitCode': build.returncode, 'buildOutput': build.stdout + build.stderr}
            if build.returncode == 0:
                environment = os.environ.copy()
                if args.sanitize:
                    environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], input=payload, cwd=ROOT, capture_output=True,
                                     timeout=240, env=environment, **options)
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
