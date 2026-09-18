#!/usr/bin/env python3
"""Compare fnTech3's RTCW MP only-visible-clients test with the original function.

The original isEntVisible is read from a local RTCW MP source tree (and, when
given, iortcw's) and compiled unchanged only inside a disposable fixture, next to
fnTech3's CL_RTCWPlayerVisible extracted from code/client/cl_parse.cpp. Both use
the same q_shared.h macros and inline math and the same Q_rsqrt and AngleVectors
taken from code/qcommon/q_math.c. A recording CM_BoxTrace answers each trace from
a per-case script, so every case compares the exact trace arguments (bit
patterns of both end points, bounds, model, contents mask, capsule flag) and the
decision. No implementation text from the references is incorporated.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#ifdef _MSC_SSE2
#include <intrin.h>
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
struct TraceCall { uint32_t start[3], end[3]; bool nullBounds; int model, mask, capsule; };
static std::vector<TraceCall> calls;
static int clearAt;
static void CM_BoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins,
    const vec3_t maxs, clipHandle_t model, int mask, int capsule ) {
    TraceCall call{};
    std::memcpy( call.start, start, sizeof( call.start ) );
    std::memcpy( call.end, end, sizeof( call.end ) );
    call.nullBounds = !mins && !maxs; call.model = model; call.mask = mask; call.capsule = capsule;
    std::memset( tr, 0, sizeof( *tr ) );
    tr->fraction = static_cast<int>( calls.size() ) == clearAt ? 1.0f : 0.5f;
    calls.push_back( call );
}
'''

REFERENCE_TYPES = r'''
namespace reference {
struct entityState_t { struct { vec3_t trBase; } pos; int animMovetype; };
static struct { vec3_t cgameClientLerpOrigin;
    struct { struct { int viewheight; float leanf; vec3_t viewangles; } ps; } snap; } cl;
'''

OURS_TYPES = r'''
namespace ours {
struct entityState_t { struct { vec3_t trBase; } pos; struct { int animMovetype; } rtcw; };
static struct { vec3_t cgameRTCWLerpOrigin;
    struct { struct { int viewheight; vec3_t viewangles; struct { float leanf; } rtcw; } ps; } snap; } cl;
'''

MAIN = r'''
struct Result { std::vector<TraceCall> calls; bool visible; };
static bool Same( const std::vector<TraceCall> &a, const std::vector<TraceCall> &b ) {
    if ( a.size() != b.size() ) return false;
    for ( std::size_t i = 0; i < a.size(); ++i )
        if ( std::memcmp( &a[i], &b[i], sizeof( TraceCall ) ) ) return false;
    return true;
}
int main( int argc, char **argv ) {
    const unsigned cases = argc > 1 ? static_cast<unsigned>( std::strtoul( argv[1], nullptr, 10 ) ) : 100000u;
    std::mt19937 rng( 20260918u );
    std::uniform_real_distribution<float> coordinate( -4096.0f, 4096.0f ), angle( -180.0f, 180.0f );
    std::uniform_real_distribution<float> lean( -64.0f, 64.0f );
    std::uniform_int_distribution<int> height( -8, 64 ), trace( 0, 7 ), coin( 0, 3 );
    unsigned long long traces = 0, visible = 0, leaning = 0, low = 0, unseen = 0;
    for ( unsigned n = 0; n < cases; ++n ) {
        vec3_t eye, base, view; float leanf = coin( rng ) ? 0.0f : lean( rng );
        for ( int k = 0; k < 3; ++k ) { eye[k] = coordinate( rng ); base[k] = coordinate( rng ); view[k] = angle( rng ); }
        if ( n % 97 == 0 ) { VectorCopy( eye, base ); }            /* degenerate direction */
        if ( n % 89 == 0 ) { base[0] = eye[0]; base[1] = eye[1]; }  /* straight up or down */
        const int viewheight = height( rng ), movetype = coin( rng ) == 0;
        clearAt = trace( rng );
        reference::cl = {}; ours::cl = {};
        VectorCopy( eye, reference::cl.cgameClientLerpOrigin ); VectorCopy( eye, ours::cl.cgameRTCWLerpOrigin );
        reference::cl.snap.ps.viewheight = ours::cl.snap.ps.viewheight = viewheight;
        reference::cl.snap.ps.leanf = ours::cl.snap.ps.rtcw.leanf = leanf;
        VectorCopy( view, reference::cl.snap.ps.viewangles ); VectorCopy( view, ours::cl.snap.ps.viewangles );
        reference::entityState_t theirEntity{}; ours::entityState_t ourEntity{};
        VectorCopy( base, theirEntity.pos.trBase ); VectorCopy( base, ourEntity.pos.trBase );
        theirEntity.animMovetype = ourEntity.rtcw.animMovetype = movetype;
        calls.clear(); const bool theirs = reference::isEntVisible( &theirEntity ) != 0; const auto theirCalls = calls;
        calls.clear(); const bool mine = ours::CL_RTCWPlayerVisible( &ourEntity ); const auto myCalls = calls;
        if ( theirs != mine || !Same( theirCalls, myCalls ) ) {
            std::fprintf( stderr, "case %u differs: visible %d/%d, traces %zu/%zu\n", n, theirs, mine,
                theirCalls.size(), myCalls.size() );
            return 1;
        }
        for ( const auto &call : myCalls ) assert( call.nullBounds && call.model == 0 && call.capsule == 0 );
        traces += myCalls.size(); visible += mine; leaning += leanf != 0; low += movetype; unseen += clearAt == 7;
    }
    std::printf( "{\"cases\":%u,\"traces\":%llu,\"visible\":%llu,\"leaning\":%llu,\"low\":%llu,\"unseen\":%llu}\n",
        cases, traces, visible, leaning, low, unseen );
}
'''


def build_fixture(reference_text, label):
    q_math = (ROOT / 'code/qcommon/q_math.c').read_text(encoding='utf-8')
    parse = (ROOT / 'code/client/cl_parse.cpp').read_text(encoding='utf-8')
    source = PRELUDE
    source += function(q_math, 'float Q_rsqrt( float number )\n{')
    source += function(q_math, 'void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {')
    source += REFERENCE_TYPES + '#define qboolean int\n'
    source += function(reference_text, 'qboolean isEntVisible( entityState_t *ent ) {')
    source += '#undef qboolean\n}\n'
    source += OURS_TYPES
    source += function(parse, 'static bool CL_RTCWPlayerVisible( const entityState_t *ent ) {')
    source += '}\n' + MAIN
    return source


def run(reference, label, cases, work):
    text = reference.read_text(encoding='utf-8', errors='replace')
    fixture = work / f'{label}.cpp'
    binary = work / (f'{label}.exe' if os.name == 'nt' else label)
    fixture.write_text(build_fixture(text, label), encoding='utf-8')
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('A C++17 compiler is required')
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target:
        target = ['-DFNTECH3_Q3_ONLY']  # the shared x86 guard; this fixture is pure math
    built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-w',
                            '-I', str(ROOT), str(fixture), '-o', str(binary)],
                           capture_output=True, text=True, timeout=120)
    if built.returncode:
        raise SystemExit(built.stdout + built.stderr)
    ran = subprocess.run([str(binary), str(cases)], capture_output=True, text=True, timeout=600)
    row = dict(reference=str(reference), referenceSha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
               compiler=compiler, target=target or ['default'],
               exitCode=ran.returncode, stderr=ran.stderr.strip())
    if ran.returncode == 0:
        row.update(json.loads(ran.stdout))
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True,
                        help='original RTCW MP src/client/cl_parse.c')
    parser.add_argument('--iortcw', type=Path, help='iortcw MP/code/client/cl_parse.c')
    parser.add_argument('--cases', type=int, default=100000)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    (ROOT / '.tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rtcw-only-visible-', dir=ROOT / '.tmp') as folder:
        work = Path(folder)
        results = [run(args.reference.resolve(strict=True), 'original', args.cases, work)]
        if args.iortcw:
            results.append(run(args.iortcw.resolve(strict=True), 'iortcw', args.cases, work))
    report = dict(implementation=dict(path='code/client/cl_parse.cpp', sha256=hashlib.sha256(
        (ROOT / 'code/client/cl_parse.cpp').read_bytes()).hexdigest()), results=results,
        passed=all(r['exitCode'] == 0 for r in results))
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
