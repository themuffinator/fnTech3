#!/usr/bin/env python3
"""Compare normal-based vertex alpha against original and iortcw SP/MP.

Reference switch bodies and notices are retained in disposable fixtures only.
This checks CPU alpha bytes with finite synthetic inputs, not GPU blending,
installed shader selection, or the original renderer's entity mutation.
"""
from __future__ import annotations
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from model_shader_tests import PRELUDE

HOST = r'''
#include <cmath>
#include "code/renderercommon/tr_rtcw_normal_fade.h"
namespace reference {
enum { AGEN_NORMALZFADE = 1 };
static const vec3_t vec3_origin = {0,0,0};
static void VectorRotate(const vec3_t value, const vec3_t axis[3], vec3_t out) {
    for(int i=0;i<3;++i)out[i]=DotProduct(value,axis[i]);
}
struct Entity {
    vec3_t fireRiseDir,axis[3];
    qhandle_t hModel;
    float shaderTime;
    byte shaderRGBA[4];
};
static struct {Entity e;} entity;
static struct {decltype(entity) *currentEntity;} backEnd{&entity};
static struct {int numVertexes;vec4_t normal[1];struct {byte colors[1][4];}svars;} tess;
struct Stage {float zFadeBounds[2];byte constantColor[4];};
static byte Evaluate(const refEntity_t &input, const float *normal, const float bounds[2], byte peak) {
    VectorCopy(input.rtcw.fireRiseDir,entity.e.fireRiseDir);
    std::memcpy(entity.e.axis,input.axis,sizeof(input.axis));
    entity.e.hModel=input.hModel;entity.e.shaderTime=.375f;
    std::memcpy(entity.e.shaderRGBA,input.shader.rgba,sizeof(entity.e.shaderRGBA));
    tess.numVertexes=1;VectorCopy(normal,tess.normal[0]);
    std::memset(tess.svars.colors,43,sizeof(tess.svars.colors));
    Stage stage{};stage.zFadeBounds[0]=bounds[0];stage.zFadeBounds[1]=bounds[1];stage.constantColor[3]=peak;
    const Stage *pStage=&stage;int i;
    switch(AGEN_NORMALZFADE) {
'''

CHECKS = r'''
    }
    for(int channel=0;channel<3;++channel)assert(tess.svars.colors[0][channel]==43);
    return tess.svars.colors[0][3];
}
}
int main() {
    const float bounds[][2]={{-1,1},{-.3f,.7f},{0,1},{-1,0},{.1f,.9f},
        {-1000,1},{-1,-1000},{-1000,-1000},{1,-1},{0,0}};
    const float directions[][3]={{0,0,0},{0,0,1},{.6f,0,.8f},{0,-1,0},{1,2,3}};
    const float axes[][3][3]={{{1,0,0},{0,1,0},{0,0,1}},
        {{0,0,1},{0,1,0},{-1,0,0}},{{.8f,.6f,0},{-.6f,.8f,0},{0,0,1}}};
    unsigned cases=0,mismatches=0,maximumDelta=0;
    for(int model:{0,1})for(const auto &axis:axes)for(const auto &direction:directions)
    for(const auto &range:bounds)for(int peak:{0,1,63,127,254,255})
    for(int opacity:{0,1,63,127,128,254,255})for(int sample=0;sample<=64;++sample) {
        refEntity_t input{};input.hModel=model;input.shader.rgba[3]=(byte)opacity;
        std::memcpy(input.axis,axis,sizeof(axis));VectorCopy(direction,input.rtcw.fireRiseDir);
        const float z=(sample-32)/32.0f;
        const float normal[]={std::sqrt(1-z*z)*.6f,std::sqrt(1-z*z)*.8f,z};
        const auto unchanged=input;
        const byte actual=R_RTCWNormalZAlpha(&input,normal,range,(byte)peak);
        const byte expected=reference::Evaluate(input,normal,range,(byte)peak);
        assert(!std::memcmp(&input,&unchanged,sizeof(input)));
        const auto delta=(unsigned)std::abs((int)actual-(int)expected);
        if(delta){
            if(mismatches<8)std::fprintf(stderr,"alpha %u/%u model %d peak %d opacity %d sample %d bounds %.9g %.9g\n",
                actual,expected,model,peak,opacity,sample,range[0],range[1]);
            ++mismatches;if(delta>maximumDelta)maximumDelta=delta;
        }
        ++cases;
    }
    std::printf("{\"cases\":%u,\"mismatches\":%u,\"maximumAlphaByteDelta\":%u}\n",cases,mismatches,maximumDelta);
}
'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(reference):
    start = reference.index('case AGEN_NORMALZFADE:')
    body = reference[start:reference.index('case AGEN_VERTEX:', start)]
    return reference[:reference.index('*/') + 2] + '\n' + PRELUDE + HOST + body + CHECKS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sp-source', 'mp-source', 'iortcw-source', 'report'):
        parser.add_argument('--' + name, type=Path, required=name != 'iortcw-source')
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--float-unit', choices=('default', 'sse2'), default='default')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    triple = subprocess.run([args.compiler, *target, '-dumpmachine'], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    own = ['code/renderercommon/tr_rtcw_normal_fade.h', 'code/renderercommon/tr_rtcw_normal_fade_parser.h',
           'tests/rtcw_normal_fade_tests.py', 'scripts/probe_rtcw_normal_fade.py']
    report = {'kind': __doc__, 'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize, 'floatUnit': args.float_unit,
              'implementationSha256': {name: digest(ROOT / name) for name in own}, 'results': []}
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, variant, reference in references:
        path = reference / 'renderer/tr_shade.c'
        with tempfile.TemporaryDirectory(prefix='normal-fade-reference-', dir=report_path.parent) as temp:
            folder = Path(temp); fixture_path = folder / 'probe.cpp'
            fixture_path.write_text(fixture(path.read_text(encoding='utf-8')), encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c++17', '-O1', '-I', str(ROOT), str(fixture_path), '-o', str(executable)]
            if args.float_unit == 'sse2':
                command += ['-msse2', '-mfpmath=sse']
            if os.name == 'nt' and triple.startswith('x86_64'):
                command.append('-DFNTECH3_Q3_ONLY')
            if args.sanitize:
                command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
            build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            result = {'mode': mode, 'reference': variant, 'source': str(path), 'sourceSha256': digest(path),
                      'buildExitCode': build.returncode, 'buildOutput': build.stdout + build.stderr}
            if build.returncode == 0:
                environment = os.environ.copy()
                if args.sanitize:
                    environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, env=environment, **options)
                result.update(exitCode=run.returncode, diagnostics=run.stderr,
                              observations=json.loads(run.stdout) if run.returncode == 0 else None)
                result['passed'] = run.returncode == 0 and result['observations'] == {'cases': 819000, 'mismatches': 0, 'maximumAlphaByteDelta': 0}
            else:
                result['passed'] = False
            report['results'].append(result)
            report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
            print(f'{variant} {mode}: {"PASS" if result["passed"] else "FAIL"}; {result.get("observations")}', flush=True)
    return 0 if all(result['passed'] for result in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
