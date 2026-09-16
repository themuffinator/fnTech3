#!/usr/bin/env python3
"""Compare brush shader selection/mip/lookup requests with four RTCW sources.

Source functions are compiled only in disposable fixtures, retaining notices.
Image/model/shader services are synthetic. This does not compare GPU shader
compilation, reference shader-cache mutation, or retail rendering.
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
from model_shader_tests import brush_fixture, function


CHECKS = r'''
int main() {
    shader_t materials[3]{{"textures/brick", 0, 101}, {"textures/metal", 0, 102}, {"textures/wood", 0, 103}};
    msurface_t surfaces[3]{{materials}, {materials+1}, {materials+2}};
    bmodel_t brush{}; brush.firstSurface = surfaces; brush.numSurfaces = 3;
    models[1].type = MOD_BRUSH; models[1].bmodel = &brush;
    shaderStage_t resultStage{}; resultShader.stages[0] = &resultStage;
    unsigned cases = 0, diffuseRequests = 0;
    for (int lightmap : {INT_MIN, -6, -4, -3, -2, -1, 0, 1, 7, 8, INT_MAX})
    for (int imageState = 0; imageState < 5; ++imageState)
    for (int surface : {INT_MIN, -1, 0, 1, 2, 3, INT_MAX})
    for (int withLightmap : {INT_MIN, -1, 0, 1, INT_MAX}) {
        const int selected = surface < 0 || surface > 2 ? 0 : surface;
        for (auto &material : materials) material.lightmapIndex = lightmap;
        const auto flags = imageState == 2 ? IMGFLAG_MIPMAP : IMGFLAG_NONE;
        image_t match{imageState == 4 ? "case-mismatch" : materials[selected].name, nullptr, flags, flags ? qtrue : qfalse};
        image_t collision{"other-image", &match, IMGFLAG_MIPMAP, qtrue};
        hashTable[0] = imageState == 0 ? nullptr : imageState >= 3 ? &collision : &match;
        shaderCalls = 0; shaderName.clear(); shaderMip = qfalse;
        const int actual = RE_GetShaderFromModel(1,surface,withLightmap);
        const auto actualCalls = shaderCalls; const auto actualName = shaderName; const auto actualMip = shaderMip;
        shaderCalls = 0; shaderName.clear(); shaderMip = qfalse; resultStage.rgbGen = -1;
        const int expected = reference::RE_GetShaderFromModel(1,surface,withLightmap);
        assert(actual == expected && actualCalls == shaderCalls && actualName == shaderName && actualMip == shaderMip);
        if (lightmap > LIGHTMAP_NONE) {
            assert(actual == 731 && shaderCalls == 1 && resultStage.rgbGen == CGEN_LIGHTING_DIFFUSE);
            ++diffuseRequests;
        } else assert(actual == materials[selected].index && !shaderCalls);
        ++cases;
    }
    std::printf("{\"cases\":%u,\"diffuseRequests\":%u}\n", cases, diffuseRequests);
}
'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_fixture(reference):
    source = brush_fixture('renderer')
    source = source.replace('struct shader_t {', 'enum { CGEN_LIGHTING_DIFFUSE = 91 };\nstruct shaderStage_t { int rgbGen; };\nstruct shader_t {')
    source = source.replace('int lightmapIndex, index; };', 'int lightmapIndex, index; shaderStage_t *stages[1]; };')
    source = source.replace('imgFlags_t flags; };', 'imgFlags_t flags; qboolean mipmap; };')
    source += r'''
namespace reference {
// Preserve the C reference's integer-to-qboolean assignment in this C++ fixture.
static qboolean operator&(imgFlags_t a, imgFlags_t b) { return static_cast<qboolean>((int)a & (int)b); }
static shader_t *R_FindShader(const char *name, int lightmap, qboolean mip) {
    assert(lightmap == LIGHTMAP_NONE); return ::R_FindModelShader(name,mip);
}
'''
    source += function(reference, 'qhandle_t RE_GetShaderFromModel(') + '\n}\n' + CHECKS
    return reference[:reference.index('*/') + 2] + '\n' + source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sp-source', 'mp-source', 'iortcw-source', 'report'):
        parser.add_argument('--' + name, type=Path, required=name != 'iortcw-source')
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    triple = subprocess.run([args.compiler, *target, '-dumpmachine'], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    own = ['code/renderercommon/tr_model_shader.h', 'tests/model_shader_tests.py']
    own += [f'code/{r}/{f}' for r in ('renderer', 'renderervk', 'rendererrtx') for f in ('tr_shader.c', 'tr_image.c', 'tr_local.h')]
    report = {'kind': __doc__, 'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
              'implementationSha256': {name: digest(ROOT / name) for name in own}, 'results': []}
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, variant, reference in references:
        path = reference / 'renderer/tr_image.c'
        with tempfile.TemporaryDirectory(prefix='model-shader-reference-', dir=report_path.parent) as temp:
            folder = Path(temp); fixture_path = folder / 'probe.cpp'
            fixture_path.write_text(make_fixture(path.read_text(encoding='utf-8')), encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c++17', '-O1', '-I', str(ROOT), str(fixture_path), '-o', str(executable)]
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
                result['passed'] = run.returncode == 0 and result['observations'] == {'cases': 1925, 'diffuseRequests': 875}
            else:
                result['passed'] = False
            report['results'].append(result)
            report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
            print(f'{variant} {mode}: {"PASS" if result["passed"] else "FAIL"}; {result.get("observations")}', flush=True)
    return 0 if all(result['passed'] for result in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
