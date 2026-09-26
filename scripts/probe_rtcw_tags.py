#!/usr/bin/env python3
"""Compare indexed mesh attachments with original RTCW/iortcw and installed assets.

Each reference is compiled in a separate disposable translation unit with its
notices intact. The engine implementation is compiled against fnTech3's actual
records and math functions. Reports contain identities/counts/errors, not assets.
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
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from mesh_tag_tests import fixture, function

HEADERS = ('code/renderercommon/tr_mesh_tags.h', 'code/renderercommon/tr_mdc_format.h',
           'code/renderercommon/tr_mdc_model_types.h', 'code/renderercommon/tr_types.h',
           'code/renderer/tr_local.h', 'code/qcommon/q_math.c')

OWN = r'''
static mdcModel_t compressedModel;
static unsigned char *unalignedMdc;
void ProbeOwnOpen(void *data, uint32_t bytes, int compressed) {
    memset(models, 0, sizeof(models));
    free(unalignedMdc); unalignedMdc = NULL;
    if (compressed) {
        unalignedMdc = (unsigned char *)malloc((size_t)bytes + 1); assert(unalignedMdc);
        memcpy(unalignedMdc + 1, data, bytes);
        assert(FnMdc_Open(unalignedMdc + 1, bytes, &compressedModel.view));
        models[1].type = MOD_MDC; models[1].mdc[0] = &compressedModel;
    } else {
        models[1].type = MOD_MESH; models[1].md3[0] = (md3Header_t *)data;
    }
}
void ProbeOwnClose(void) { free(unalignedMdc); unalignedMdc = NULL; }
int ProbeOwnTag(int old, int current, float back, const char *name, int first, float *output) {
    refEntity_t entity;
    orientation_t tag;
    int index;
    memset(&entity, 0, sizeof(entity)); entity.hModel = 1;
    entity.oldframe = old; entity.frame = current; entity.backlerp = back;
    index = R_LerpTagEntity(&tag, &entity, name, first);
    _Static_assert(sizeof(tag) == 12 * sizeof(float), "bridge output");
    memcpy(output, &tag, sizeof(tag)); return index;
}
'''

REFERENCE = r'''
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
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
enum { MOD_BAD, MOD_MESH, MOD_MDC, MOD_MDS, MOD_MDR, MOD_IQM };
typedef struct { int type; void *md3[3], *mdc[3], *mds, *modelData; } model_t;
static model_t model;
static model_t *R_GetModelByHandle(qhandle_t handle) { assert(handle == 1); return &model; }
static int R_GetBoneTag(orientation_t *tag, void *data, int first, const refEntity_t *entity, const char *name) {
    assert(0); return -1;
}
void Q_strncpyz(char *destination, const char *source, int bytes) {
    assert(bytes > 0); strncpy(destination, source, bytes - 1); destination[bytes - 1] = 0;
}
void ProbeOwnOpen(void *, uint32_t, int);
void ProbeOwnClose(void);
int ProbeOwnTag(int, int, float, const char *, int, float *);
'''

CHECKS = r'''
static uint64_t comparisons, indexDifferences;
static double maxOrigin, maxAxis;
static void Compare(int old, int current, float back, const char *name, int first) {
    refEntity_t entity;
    orientation_t reference;
    float own[12], values[12];
    int i, actual, expected;
    memset(&entity, 0, sizeof(entity)); entity.hModel = 1;
    entity.oldframe = old; entity.frame = current; entity.backlerp = back;
    expected = R_LerpTag(&reference, &entity, name, first);
    actual = ProbeOwnTag(old, current, back, name, first, own);
    if (actual != expected) {
        float ignored[12];
        assert(IORTCW && actual >= 0 && expected >= 0);
        assert(actual == ProbeOwnTag(current, current, 0, name, first, ignored));
        assert(expected == ProbeOwnTag(old, old, 0, name, first, ignored));
        ++indexDifferences; // Original RTCW's current-frame index is authoritative.
    }
    memcpy(values, &reference, sizeof(values));
    for (i = 0; i < 12; ++i) {
        const double error = fabs((double)own[i] - values[i]);
        assert(isfinite(own[i]) && isfinite(values[i]));
        if (i < 3) { if (error > maxOrigin) maxOrigin = error; }
        else if (error > maxAxis) maxAxis = error;
        if (error > (i < 3 ? 0.0001 : 0.000002)) {
            fprintf(stderr, "Tag comparison %llu component %d: %.9g vs %.9g\n",
                (unsigned long long)comparisons, i, own[i], values[i]); abort();
        }
    }
    ++comparisons;
}
int main(void) {
    unsigned char length[4];
    unsigned count = 0;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    while (fread(length, 1, 4, stdin) == 4) {
        const uint32_t bytes = (uint32_t)length[0] | (uint32_t)length[1] << 8 |
            (uint32_t)length[2] << 16 | (uint32_t)length[3] << 24;
        unsigned char *data;
        int compressed, frames, tags, frame, tag, blend, start;
        const float backlerps[] = {-0.5f, 0, 0.125f, 0.5f, 0.75f, 1, 1.5f, 0x1p-26f};
        if (!bytes) break;
        assert(bytes >= sizeof(md3Header_t) && bytes < 32 * 1024 * 1024);
        data = (unsigned char *)malloc(bytes); assert(data && fread(data, 1, bytes, stdin) == bytes);
        compressed = *(const uint32_t *)data == MDC_IDENT;
        memset(&model, 0, sizeof(model));
        if (compressed) {
            const mdcHeader_t *header = (const mdcHeader_t *)data;
            frames = header->numFrames; tags = header->numTags; model.type = MOD_MDC; model.mdc[0] = data;
        } else {
            const md3Header_t *header = (const md3Header_t *)data;
            assert(header->ident == MD3_IDENT);
            frames = header->numFrames; tags = header->numTags; model.type = MOD_MESH; model.md3[0] = data;
        }
        assert(frames > 0 && tags >= 0);
        ProbeOwnOpen(data, bytes, compressed);
        comparisons = indexDifferences = 0; maxOrigin = maxAxis = 0;
        for (frame = 0; frame < frames; ++frame) for (tag = 0; tag < tags; ++tag) {
            const char *name = compressed ? ((const mdcTagName_t *)(data + ((mdcHeader_t *)data)->ofsTagNames))[tag].name :
                ((const md3Tag_t *)(data + ((md3Header_t *)data)->ofsTags))[frame * tags + tag].name;
            const int firsts[] = {-1, tag, tag + 1};
            for (blend = 0; blend < 8; ++blend) for (start = 0; start < 3; ++start)
                Compare(frame ? frame - 1 : frames - 1, frame, backlerps[blend], name, firsts[start]);
            Compare(INT_MAX, frame, 0.375f, name, 0);
            Compare(frame, INT_MAX, 0.375f, name, INT_MAX);
        }
        Compare(0, INT_MAX, 0.5f, "fntech3_missing_tag", 0);
        printf("{\"index\":%u,\"compressed\":%s,\"comparisons\":%llu,\"oldIndexDifferences\":%llu,"
               "\"maxOriginError\":%.9g,\"maxAxisError\":%.9g}\n", count++, compressed ? "true" : "false",
               (unsigned long long)comparisons, (unsigned long long)indexDifferences, maxOrigin, maxAxis);
        ProbeOwnClose(); free(data);
    }
    // Changing tag order across frames distinguishes the two reference families.
    {
        struct { md3Header_t header; md3Tag_t tags[2][3]; } sample;
        int frame, tag;
        memset(&sample, 0, sizeof(sample)); sample.header.ident = MD3_IDENT;
        sample.header.numFrames = 2; sample.header.numTags = 3;
        sample.header.ofsTags = (int)((byte *)sample.tags - (byte *)&sample);
        for (frame = 0; frame < 2; ++frame) for (tag = 0; tag < 3; ++tag) {
            strcpy(sample.tags[frame][tag].name, tag == 2 || (!frame && !tag) ? "repeat" : "other");
            sample.tags[frame][tag].origin[0] = frame * 10.0f + tag;
            AxisClear(sample.tags[frame][tag].axis);
        }
        memset(&model, 0, sizeof(model)); model.type = MOD_MESH; model.md3[0] = &sample;
        ProbeOwnOpen(&sample, sizeof(sample), 0); indexDifferences = 0;
        Compare(0, 1, 0.5f, "repeat", 0); assert(indexDifferences == (IORTCW ? 1 : 0));
        ProbeOwnClose();
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
    report = {'kind': 'CPU MD3/MDC tag reference probe; not MDS or live rendering',
              'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
              'implementationSha256': {name: digest(ROOT / name) for name in HEADERS},
              'installation': str(args.installation.resolve(strict=True)), 'assets': [], 'results': []}
    payload = bytearray()
    for archive in sorted((args.installation / 'main').glob('*.pk3')):
        with zipfile.ZipFile(archive) as pak:
            for entry in pak.infolist():
                if Path(entry.filename).suffix.lower() not in ('.md3', '.mdc'):
                    continue
                data = pak.read(entry)
                report['assets'].append({'archive': archive.name, 'name': entry.filename, 'bytes': len(data),
                                         'sha256': hashlib.sha256(data).hexdigest()})
                payload += struct.pack('<I', len(data)) + data
    if not report['assets']:
        parser.error('No MD3/MDC assets in the supplied installation/main/*.pk3')
    payload += b'\0\0\0\0'
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    renames = ('vec3_origin', 'VectorNormalize', 'AngleVectors', 'AnglesToAxis', 'AxisClear', 'Q_strncpyz', 'R_LerpTag')
    own = ''.join(f'#define {name} FnProbe_{name}\n' for name in renames) + fixture('renderer') + OWN
    for mode, variant, reference in references:
        common = reference / ('qcommon' if variant == 'iortcw' else 'game')
        paths = {'tr_model.c': reference / 'renderer/tr_model.c', 'q_math.c': common / 'q_math.c',
                 'q_shared.h': common / 'q_shared.h', 'qfiles.h': reference / 'qcommon/qfiles.h',
                 'tr_types.h': reference / ('renderer' if variant == 'iortcw' else 'cgame') / 'tr_types.h'}
        texts = {name: path.read_text(encoding='utf-8') for name, path in paths.items()}
        source = '\n'.join(text[:text.index('*/') + 2] for text in texts.values()) + REFERENCE
        source += f'#define IORTCW {int(variant == "iortcw")}\n'
        source += 'vec3_t vec3_origin = {0,0,0};\n'
        for signature in ('void AngleVectors(', 'void AnglesToAxis(', 'void AxisClear(', 'vec_t VectorNormalize('):
            source += function(texts['q_math.c'], signature)
        for signature in ('static int R_GetTag(', 'static int R_GetMDCTag('):
            source += function(texts['tr_model.c'], signature)
        if variant == 'iortcw':
            source += '''
static int R_GetAnimTag(mdrHeader_t *data, int frame, const char *name, int first, md3Tag_t **output) { assert(0); return -1; }
static int R_IQMLerpTag(orientation_t *tag, void *data, int old, int frame, float blend, const char *name, int first) { assert(0); return -1; }
'''
        source += function(texts['tr_model.c'], 'int R_LerpTag(') + CHECKS
        with tempfile.TemporaryDirectory(prefix='tag-reference-', dir=report_path.parent) as temp:
            folder = Path(temp)
            own_path, reference_path = folder / 'own.c', folder / 'reference.c'
            own_path.write_text(own, encoding='utf-8'); reference_path.write_text(source, encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c11', '-O1', '-fno-strict-aliasing',
                       '-I', str(ROOT), '-I', str(common), '-I', str(reference / 'qcommon'),
                       '-I', str(paths['tr_types.h'].parent), str(own_path), str(reference_path), '-o', str(executable)]
            if args.sanitize:
                command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
            if os.name == 'nt' and 'x86_64' in triple:
                command.append('-DFNTECH3_Q3_ONLY')
            if os.name != 'nt':
                if variant == 'iortcw':
                    command.append('-DARCH_STRING="' + triple.split('-')[0] + '"')
                command.append('-lm')
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
