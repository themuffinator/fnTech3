#!/usr/bin/env python3
"""Compare RTCW 2D adapters with local original/iortcw command and CPU backends.

Reference functions are compiled only in disposable local fixtures. No game
module, asset, graphics context, input device or proprietary content is loaded.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/compat/rtcw_draw_adapter.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
struct shader_t { int index; } shader{17};
'''

HARNESS = r'''
static stretchPicCommand_t command;
static struct { qboolean registered; } tr{qtrue};
static struct {
    int numVertexes, numIndexes; shader_t *shader;
    float xyz[4][4], texCoords[4][2][2];
    alignas(4) byte vertexColors[4][4];
    int indexes[6];
} tess;
static struct {
    qboolean projection2D; int entity2D; int *currentEntity;
    alignas(4) byte color2D[4];
} backEnd;
enum { RC_STRETCH_PIC_GRADIENT = 3, RC_ROTATED_PIC = 4 };
static stretchPicCommand_t *R_GetCommandBuffer(int bytes) {
    assert(bytes == sizeof(command)); command = {}; return &command;
}
static shader_t *R_GetShaderByHandle(int handle) { assert(handle == 17); return &shader; }
static void RB_SetGL2D() { backEnd.projection2D = qtrue; }
static void RB_EndSurface() { tess.numVertexes = tess.numIndexes = 0; }
static void RB_BeginSurface(shader_t *value, int fog) { assert(fog == 0); tess.shader = value; }
#define RB_CHECKOVERFLOW(v,i) assert((v) == 4 && (i) == 6 && tess.numVertexes == 0 && tess.numIndexes == 0)
static void Reset() {
    tess = {}; backEnd = {};
    const byte color[] = {7, 31, 97, 239}; std::memcpy(backEnd.color2D, color, 4);
}
static void Compare(const refDrawQuad_t &quad) {
    assert(tess.numVertexes == 4 && tess.numIndexes == 6);
    const int indices[] = {3, 0, 2, 2, 0, 1}; assert(!std::memcmp(tess.indexes, indices, sizeof(indices)));
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 2; ++j) {
            assert(std::fabs(quad.xy[i][j] - tess.xyz[i][j]) <= 0.0001f);
            assert(quad.st[i][j] == tess.texCoords[i][0][j]);
        }
        assert(tess.xyz[i][2] == 0);
        const byte *color = quad.colorMask & (1u << i) ? quad.colors[i].rgba : backEnd.color2D;
        assert(!std::memcmp(color, tess.vertexColors[i], 4));
    }
}
'''

CHECKS = r'''
int main() {
    int cases = 0;
    for (int i = 0; i < 128; ++i) {
        const float widths[] = {0, 1, 16, -32, 80}, heights[] = {0, 1, 20, -100};
        const float turns[] = {-2, -0.75f, 0, 0.125f, 0.25f, 1.75f};
        const int types[] = {INT_MIN, -7, 0, 1, INT_MAX};
        const float x = (i % 17) * -2.25f, y = (i % 11) * 3.5f;
        const float w = widths[i % 5], h = heights[i % 4];
        const float s1 = (i % 3) * 0.125f, t1 = (i % 5) * 0.0625f, s2 = 1.25f, t2 = -0.5f;
        vec4_t color;
        for (int j = 0; j < 4; ++j) color[j] = ((i + j) * 13 % 256) / 255.0f;
        const float *gradient = i % 9 ? color : nullptr;
        refDrawQuad_t actual;
        Reset(); RE_StretchPicGradient(x, y, w, h, s1, t1, s2, t2, 17, gradient, types[i % 5]);
        RB_StretchPicGradient(&command);
        assert(fntech3::rtcw::render::GradientQuad(x, y, w, h, s1, t1, s2, t2, 17, gradient, types[i % 5], actual));
        Compare(actual); ++cases;
#if PROBE_MP
        Reset(); RE_RotatedPic(x, y, w, h, s1, t1, s2, t2, 17, turns[i % 6]);
        RB_RotatedPic(&command);
        assert(fntech3::rtcw::render::RotatedQuad(x, y, w, h, s1, t1, s2, t2, 17, turns[i % 6], actual));
        Compare(actual); ++cases;
#endif
    }
    std::printf("%d reference comparisons passed\n", cases);
}
'''


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source', required=True, type=Path)
    parser.add_argument('--mp-source', required=True, type=Path)
    parser.add_argument('--iortcw-source', type=Path)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target', help='Optional Clang target triple')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    if not args.compiler:
        parser.error('clang++ is required')
    report_path = args.report.resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    triple = subprocess.run([args.compiler, *target, '-dumpmachine'], cwd=ROOT, capture_output=True,
                            text=True, timeout=15, check=True).stdout.strip()
    diagnostic = ['-DFNTECH3_Q3_ONLY'] if triple.startswith('x86_64') and 'windows' in triple else []
    instrumentation = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = {
        'kind': 'CPU reference comparison; not live RTCW rendering',
        'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
        'adapterSha256': digest(ROOT / 'code/compat/rtcw_draw_adapter.hpp'), 'results': [],
    }
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, origin, reference in references:
        reference = reference.resolve(strict=True) / 'renderer'
        files = {name: reference / name for name in ('tr_local.h', 'tr_cmds.c', 'tr_backend.c')}
        texts = {name: path.read_text(encoding='utf-8') for name, path in files.items()}
        declaration = re.search(r'typedef struct\s*\{[^{}]*\}\s*stretchPicCommand_t;', texts['tr_local.h'])
        if not declaration:
            raise ValueError(f'{reference}: missing command record')
        # Preserve the source notices in each disposable reference translation unit.
        notices = '\n'.join(text[:text.index('*/') + 2] for text in texts.values())
        source = notices + f'\n#define PROBE_MP {int(mode == "mp")}\n' + PRELUDE + declaration[0] + HARNESS
        source += function(texts['tr_cmds.c'], 'void RE_StretchPicGradient(')
        source += function(texts['tr_backend.c'], 'const void *RB_StretchPicGradient(')
        if mode == 'mp':
            source += function(texts['tr_cmds.c'], 'void RE_RotatedPic(')
            source += function(texts['tr_backend.c'], 'const void *RB_RotatedPic(')
        result = {'mode': mode, 'reference': origin, 'source': str(reference),
                  'sourceHashes': {name: digest(path) for name, path in files.items()}}
        with tempfile.TemporaryDirectory(prefix=f'draw-{mode}-', dir=report_path.parent) as temp:
            probe = Path(temp) / 'probe.cpp'
            executable = Path(temp) / ('probe.exe' if os.name == 'nt' else 'probe')
            probe.write_text(source + CHECKS, encoding='utf-8')
            command = [args.compiler, *target, *instrumentation, *diagnostic, '-std=c++17',
                       '-fno-strict-aliasing', '-I', str(ROOT), str(probe), '-o', str(executable)]
            build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            result['buildExitCode'] = build.returncode
            result['buildOutput'] = build.stdout + build.stderr
            if build.returncode == 0:
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                environment = os.environ.copy()
                if args.sanitize:
                    environment['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
                    environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
                run = subprocess.run([str(executable)], cwd=ROOT, env=environment, capture_output=True,
                                     text=True, timeout=30, **options)
                result['exitCode'] = run.returncode
                result['output'] = run.stdout + run.stderr
                result['comparisons'] = 256 if mode == 'mp' else 128
                result['passed'] = run.returncode == 0
            else:
                result['passed'] = False
        report['results'].append(result)
        print(f'{origin}/{mode}: {"PASS" if result["passed"] else "FAIL"}')
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if all(result['passed'] for result in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
