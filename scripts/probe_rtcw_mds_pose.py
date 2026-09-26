#!/usr/bin/env python3
"""Compare independent MDS poses with original/iortcw CPU bone evaluation.

Reference implementations are compiled only in disposable translation units,
with their notices intact. Installed files stay in memory; reports keep hashes,
counts and maximum errors, not geometry. No graphics or game module is loaded.
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
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from mds_pose_tests import fixture, function

OWN = r'''
static unsigned char *ownData;
static fnMdsView_t ownView;
static fnMdsPose_t ownPose;
static fnMdsPoseParams_t ownParams;
void MdsProbeOpen(const void *data, uint32_t bytes) {
    free(ownData); ownData = (unsigned char *)malloc((size_t)bytes + 1); assert(ownData);
    memcpy(ownData + 1, data, bytes); assert(FnMds_Open(ownData + 1, bytes, &ownView));
}
void MdsProbeClose(void) { free(ownData); ownData = NULL; }
int MdsProbePose(int frame, int old, int torso, int oldTorso, float back, float torsoBack, const float *axis, float *output) {
    fnMdsPoseParams_t params;
    memset(&params, 0, sizeof(params)); params.frame = frame; params.oldFrame = old;
    params.torsoFrame = torso; params.oldTorsoFrame = oldTorso; params.backlerp = back; params.torsoBacklerp = torsoBack;
    memcpy(params.torsoAxis, axis, sizeof(params.torsoAxis));
    ownParams=params;
    if (!FnMds_DecodePose(&ownView, &params, &ownPose)) return 0;
    _Static_assert(sizeof(fnMdsTransform_t) == 48, "pose bridge");
    memcpy(output, ownPose.bones, ownPose.numBones * sizeof(ownPose.bones[0])); return 1;
}
int MdsProbeBone(uint32_t bone, float *output) {
    fnMdsTransform_t transform;
    if(!FnMds_DecodeBone(&ownView,&ownParams,bone,&transform))return 0;
    memcpy(output,&transform,sizeof(transform));return 1;
}
int MdsProbeFoot(int frame,uint32_t bone,float *output) {
    fnMdsPoseParams_t params;fnMdsTransform_t transform;int row,column;
    memset(&params,0,sizeof(params));params.frame=params.oldFrame=frame;
    params.torsoFrame=UINT32_MAX;params.oldTorsoFrame=INT32_MAX;
    params.backlerp=params.torsoBacklerp=NAN;
    for(row=0;row<3;++row)for(column=0;column<3;++column)params.torsoAxis[row][column]=NAN;
    if(!FnMds_DecodeBone(&ownView,&params,bone,&transform))return 0;
    memcpy(output,&transform,sizeof(transform));return 1;
}
int MdsProbeVertex(uint32_t surfaceOffset, uint32_t vertexOffset, float *xyz, float *normal) {
    fnMdsSurfaceView_t surface;
    fnMdsVertexView_t vertex;
    return FnMds_Surface(&ownView, surfaceOffset, &surface) && FnMds_Vertex(&surface, vertexOffset, &vertex) &&
        FnMds_SkinVertex(&vertex, &ownPose, xyz, normal);
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
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
typedef unsigned glIndex_t;
static cvar_t debug;
static cvar_t *r_bonesDebug = &debug;
static void QDECL ProbePrint(int level, const char *message, ...) { assert(0); }
static struct { void (QDECL *Printf)(int, const char *, ...); } ri = {ProbePrint};
vec3_t vec3_origin = {0,0,0};
static float LAVangle, sp, sy, cp, cy;
void MdsProbeOpen(const void *, uint32_t);
void MdsProbeClose(void);
int MdsProbePose(int, int, int, int, float, float, const float *, float *);
int MdsProbeBone(uint32_t,float *);
int MdsProbeFoot(int,uint32_t,float *);
int MdsProbeVertex(uint32_t, uint32_t, float *, float *);
'''

CHECKS = r'''
static double maxPosition, maxDirection;
static void Check(float actual, float expected, int direction, unsigned model, int frameIndex, int boneIndex, int component) {
    const double error = fabs((double)actual - expected);
    if (direction) { if (error > maxDirection) maxDirection = error; }
    else if (error > maxPosition) maxPosition = error;
    if (!isfinite(actual) || !isfinite(expected) || error > (direction ? 0.00002 : 0.001)) {
        fprintf(stderr, "MDS model %u frame %d bone/vertex %d component %d (%s): %.9g vs %.9g; error %.9g\n",
                model, frameIndex, boneIndex, component, direction ? "axis/normal" : "origin/xyz", actual, expected, error);
        abort();
    }
}
int main(void) {
    unsigned char length[4];
    unsigned modelIndex = 0;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    while (fread(length, 1, 4, stdin) == 4) {
        const uint32_t bytes = (uint32_t)length[0] | (uint32_t)length[1] << 8 |
            (uint32_t)length[2] << 16 | (uint32_t)length[3] << 24;
        unsigned char *data;
        mdsHeader_t *header;
        int frameIndex, boneIndex;
        uint64_t poseCount = 0, vertexCount = 0, tagCount = 0, partialTagCount = 0;
        if (!bytes) break;
        assert(bytes >= sizeof(mdsHeader_t) && bytes < 32 * 1024 * 1024);
        data = (unsigned char *)malloc(bytes); assert(data && fread(data, 1, bytes, stdin) == bytes);
        header = (mdsHeader_t *)data; assert(header->numBones > 0 && header->numBones <= MDS_MAX_BONES);
        MdsProbeOpen(data, bytes); maxPosition = maxDirection = 0;
        for (frameIndex = 0; frameIndex < header->numFrames; ++frameIndex) {
            const float fractions[] = {0, 0.25f, 0.5f, 0.75f, 1, -0.25f, 1.25f};
            refEntity_t entity;
            vec3_t torsoAngles;
            float own[MDS_MAX_BONES * 12];
            memset(&entity, 0, sizeof(entity)); entity.hModel = 1;
            entity.frame = frameIndex; entity.oldframe = frameIndex ? frameIndex - 1 : header->numFrames - 1;
            entity.torsoFrame = (frameIndex * 37 + 11) % header->numFrames;
            entity.oldTorsoFrame = entity.torsoFrame ? entity.torsoFrame - 1 : header->numFrames - 1;
            entity.backlerp = fractions[frameIndex % 7]; entity.torsoBacklerp = fractions[(frameIndex / 7) % 7];
            torsoAngles[0] = ((frameIndex / 9) % 5 - 2) * 10.0f;
            torsoAngles[1] = (frameIndex % 9 - 4) * 15.0f;
            torsoAngles[2] = ((frameIndex / 45) % 5 - 2) * 5.0f;
            AnglesToAxis(torsoAngles, entity.torsoAxis);
            memset(validBones, 0, sizeof(validBones)); memset(&lastBoneEntity, 0xff, sizeof(lastBoneEntity)); fullTorso = qfalse;
            assert(MdsProbePose(entity.frame, entity.oldframe, entity.torsoFrame, entity.oldTorsoFrame,
                               entity.backlerp, entity.torsoBacklerp, &entity.torsoAxis[0][0], own));
            {
                uint32_t surfaceOffset = header->ofsSurfaces;
                int surfaceIndex;
                for (surfaceIndex = 0; surfaceIndex < header->numSurfaces; ++surfaceIndex) {
                    mdsSurface_t *surface = (mdsSurface_t *)(data + surfaceOffset);
                    mdsVertex_t *vertex = (mdsVertex_t *)((byte *)surface + surface->ofsVerts);
                    int vertexIndex, referenceIndex;
                    int *references = (int *)((byte *)surface + surface->ofsBoneReferences);
                    // Preserve the file's actual surface order and the reference
                    // cache across surfaces of this entity.
                    R_CalcBones(header, &entity, references, surface->numBoneReferences);
                    for (referenceIndex = 0; referenceIndex < surface->numBoneReferences; ++referenceIndex) {
                        int row, column; boneIndex = references[referenceIndex];
                        for (row = 0; row < 3; ++row) {
                            for (column = 0; column < 3; ++column)
                                Check(own[boneIndex * 12 + row * 3 + column], bones[boneIndex].matrix[row][column], 1, modelIndex, frameIndex, boneIndex, row * 3 + column);
                            Check(own[boneIndex * 12 + 9 + row], bones[boneIndex].translation[row], 0, modelIndex, frameIndex, boneIndex, row);
                        }
                        ++poseCount;
                    }
                    for (vertexIndex = 0; (!(frameIndex % 64) || frameIndex + 1 == header->numFrames) && vertexIndex < surface->numVerts; ++vertexIndex) {
                        vec3_t referenceXyz = {0,0,0}, referenceNormal, ownXyz, ownNormal;
                        int weightIndex, component;
                        for (weightIndex = 0; weightIndex < vertex->numWeights; ++weightIndex) {
                            mdsWeight_t *weight = &vertex->weights[weightIndex];
                            mdsBoneFrame_t *bone = &bones[weight->boneIndex];
                            LocalAddScaledMatrixTransformVectorTranslate(weight->offset, weight->boneWeight, bone->matrix, bone->translation, referenceXyz);
                        }
                        LocalMatrixTransformVector(vertex->normal, bones[vertex->weights[0].boneIndex].matrix, referenceNormal);
                        assert(MdsProbeVertex(surfaceOffset, (uint32_t)((byte *)vertex - (byte *)surface), ownXyz, ownNormal));
                        for (component = 0; component < 3; ++component) {
                            Check(ownXyz[component], referenceXyz[component], 0, modelIndex, frameIndex, vertexIndex, component);
                            Check(ownNormal[component], referenceNormal[component], 1, modelIndex, frameIndex, vertexIndex, component);
                        }
                        vertex = (mdsVertex_t *)(vertex->weights + vertex->numWeights); ++vertexCount;
                    }
                    surfaceOffset += surface->ofsEnd;
                }
            }
            if (!(frameIndex % 64) || frameIndex + 1 == header->numFrames) {
                int tagIndex;
                mdsTag_t *tags = (mdsTag_t *)(data + header->ofsTags);
                for (tagIndex = 0; tagIndex < header->numTags; ++tagIndex) {
                    orientation_t tag; int row, column;float selected[12];
                    // A fresh attachment query evaluates only its ancestry.
                    memset(validBones, 0, sizeof(validBones)); memset(&lastBoneEntity, 0xff, sizeof(lastBoneEntity)); fullTorso = qfalse;
                    assert(R_GetBoneTag(&tag, header, tagIndex, &entity, tags[tagIndex].name) == tagIndex);
                    boneIndex = tags[tagIndex].boneIndex;
                    assert(MdsProbeBone(boneIndex,selected));
                    for (row = 0; row < 3; ++row) {
                        for (column = 0; column < 3; ++column)
                            Check(own[boneIndex * 12 + row * 3 + column], tag.axis[row][column], 1, modelIndex, frameIndex, boneIndex, row * 3 + column);
                        Check(own[boneIndex * 12 + 9 + row], tag.origin[row], 0, modelIndex, frameIndex, boneIndex, row);
                        for(column=0;column<3;++column)
                            Check(selected[row*3+column],tag.axis[row][column],1,modelIndex,frameIndex,boneIndex,row*3+column);
                        Check(selected[9+row],tag.origin[row],0,modelIndex,frameIndex,boneIndex,row);
                    }
                    ++tagCount;
                    {
                        mdsBoneInfo_t *info=(mdsBoneInfo_t *)(data+header->ofsBones);
                        int bone=boneIndex,lowerOnly=1;
                        while(bone>=0){if(info[bone].torsoWeight!=0)lowerOnly=0;bone=info[bone].parent;}
                        if(lowerOnly){
                            refEntity_t partial;
                            memset(&partial,0xa5,sizeof(partial));partial.frame=partial.oldframe=frameIndex;
                            partial.backlerp=NAN;partial.torsoFrame=partial.oldTorsoFrame=0;partial.torsoBacklerp=0;
                            for(row=0;row<3;++row)for(column=0;column<3;++column)partial.torsoAxis[row][column]=NAN;
                            memset(validBones,0,sizeof(validBones));memset(&lastBoneEntity,0xff,sizeof(lastBoneEntity));fullTorso=qfalse;
                            assert(R_GetBoneTag(&tag,header,tagIndex,&partial,tags[tagIndex].name)==tagIndex);
                            assert(MdsProbeFoot(frameIndex,boneIndex,selected));
                            for(row=0;row<3;++row){
                                for(column=0;column<3;++column)
                                    Check(selected[row*3+column],tag.axis[row][column],1,modelIndex,frameIndex,boneIndex,row*3+column);
                                Check(selected[9+row],tag.origin[row],0,modelIndex,frameIndex,boneIndex,row);
                            }
                            ++partialTagCount;
                        }
                    }
                }
            }
        }
        printf("{\"index\":%u,\"frames\":%d,\"bonePoses\":%llu,\"vertices\":%llu,\"tags\":%llu,\"partialTags\":%llu,\"maxPositionError\":%.9g,\"maxDirectionError\":%.9g}\n",
            modelIndex++, header->numFrames, (unsigned long long)poseCount, (unsigned long long)vertexCount, (unsigned long long)tagCount,
            (unsigned long long)partialTagCount, maxPosition, maxDirection);
        fflush(stdout); MdsProbeClose(); free(data);
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
    triple = subprocess.run([args.compiler, *target, '-dumpmachine'], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    report = {'kind': 'CPU MDS pose reference probe using authored surface lists and fresh tag ancestry; no live rendering',
              'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
              'implementationSha256': {name: digest(ROOT / name) for name in
                  ('code/renderercommon/tr_mds_format.h', 'code/renderercommon/tr_mds_pose.h', 'code/qcommon/q_math.c')},
              'installation': str(args.installation.resolve(strict=True)), 'assets': [], 'results': []}
    payload = bytearray()
    for archive in sorted((args.installation / 'main').glob('*.pk3')):
        with zipfile.ZipFile(archive) as pak:
            for entry in pak.infolist():
                if entry.filename.lower().endswith('.mds'):
                    data = pak.read(entry)
                    report['assets'].append({'archive': archive.name, 'name': entry.filename, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
                    payload += struct.pack('<I', len(data)) + data
    if not report['assets']:
        parser.error('No installed MDS files')
    payload += b'\0\0\0\0'
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    own = ''.join(f'#define {name} FnProbe_{name}\n' for name in ('vec3_origin', 'AngleVectors', 'AnglesToAxis')) + fixture() + OWN
    for mode, variant, reference in references:
        common = reference / ('qcommon' if variant == 'iortcw' else 'game')
        paths = {'tr_animation.c': reference / 'renderer/tr_animation.c', 'q_math.c': common / 'q_math.c',
                 'q_shared.h': common / 'q_shared.h', 'qfiles.h': reference / 'qcommon/qfiles.h',
                 'tr_types.h': reference / ('renderer' if variant == 'iortcw' else 'cgame') / 'tr_types.h'}
        texts = {name: path.read_text(encoding='utf-8') for name, path in paths.items()}
        source = '\n'.join(text[:text.index('*/') + 2] for text in texts.values()) + REFERENCE
        animation = texts['tr_animation.c']
        start, end = animation.index('static float frontlerp'), animation.index('static int totalrv')
        source += animation[start:animation.index(';', end) + 1] + '\n'
        source += re.search(r'^#define ANGLES_SHORT_TO_FLOAT[^\n]*', animation, re.M)[0] + '\n'
        for signature in ('void AngleVectors(', 'void AnglesToAxis(', 'float AngleNormalize360', 'float AngleNormalize180', 'vec_t VectorNormalize('):
            source += function(texts['q_math.c'], signature)
        for name in ('LocalMatrixTransformVector', 'LocalScaledMatrixTransformVector', 'LocalAddScaledMatrixTransformVectorTranslate',
                     'LocalAngleVector', 'LocalVectorMA', 'SLerp_Normal', 'Matrix4MultiplyInto3x3AndTranslation',
                     'Matrix4FromAxisPlusTranslation', 'Matrix4FromScaledAxisPlusTranslation', 'Matrix3Transpose',
                     'R_CalcBone', 'R_CalcBoneLerp', 'R_CalcBones', 'R_RecursiveBoneListAdd'):
            source += function(animation, 'void ' + name + '(')
        source += function(animation, 'int R_GetBoneTag(')
        source += CHECKS
        with tempfile.TemporaryDirectory(prefix='mds-pose-reference-', dir=report_path.parent) as temp:
            folder = Path(temp)
            own_path, reference_path = folder / 'own.c', folder / 'reference.c'
            own_path.write_text(own, encoding='utf-8'); reference_path.write_text(source, encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c11', '-O1', '-fno-strict-aliasing',
                       '-I', str(ROOT), '-I', str(common), '-I', str(reference / 'qcommon'), '-I', str(paths['tr_types.h'].parent),
                       str(own_path), str(reference_path), '-o', str(executable)]
            if os.name == 'nt' and 'x86_64' in triple:
                command.append('-DFNTECH3_Q3_ONLY')
            if args.sanitize:
                command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
            if os.name != 'nt':
                command.append('-lm')
                if variant == 'iortcw':
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
                run = subprocess.run([str(executable)], input=payload, cwd=ROOT, capture_output=True, timeout=240, env=environment, **options)
                result.update(exitCode=run.returncode, diagnostics=run.stderr.decode('utf-8', errors='replace'),
                              models=[json.loads(line) for line in run.stdout.splitlines()])
                result['passed'] = run.returncode == 0 and len(result['models']) == len(report['assets'])
            else:
                result['passed'] = False
            report['results'].append(result)
            report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
            print(f'{variant} {mode}: {"PASS" if result["passed"] else "FAIL"}; {len(result.get("models", []))} models', flush=True)
    return 0 if all(result['passed'] for result in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
