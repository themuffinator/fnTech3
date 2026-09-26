#!/usr/bin/env python3
"""Compare ET camera/lean coordinates with an external original-source build.

Reference implementation text is read only into disposable local translation
units. This compares finite viewpoint calculations, not PVS selection, rendering,
retail floating-point execution or the original self-portal recursion path.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

COMMON = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static sharedEntity_t camera{};
static playerState_t player{};
static client_t peer{};
static vec3_t observed{};
'''

REFERENCE = r'''
server_t sv{};
serverStatic_t svs{};
// Original sv_snapshot.c's private candidate capacity; this probe selects none.
struct snapshotEntityNumbers_t {int numSnapshotEntities;int snapshotEntities[2048];};
void QDECL Com_Error(int,const char*,...){std::abort();}
sharedEntity_t *SV_GentityNum(int){return &camera;}
playerState_t *SV_GameClientNum(int){return &player;}
static int QDECL SV_QsortEntityNumbers(const void*,const void*){std::abort();}
static void SV_AddEntitiesVisibleFromPoint(vec3_t origin,clientSnapshot_t*,snapshotEntityNumbers_t*){
 std::memcpy(observed,origin,sizeof(observed));
}
'''

ENGINE = r'''
int Com_GameIsWolfET(){return 1;}
void QDECL Com_Error(errorParm_t,const char*,...){std::abort();}
qboolean SV_ReadGameEntity(const void *pointer,sharedEntity_t *out){
 assert(pointer==&camera);*out=camera;return qtrue;
}
'''

MAIN = r'''
int main(){
 int flags,followed;
 float lean;
 peer.gentity=&camera;
#ifdef ORIGINAL
 svs.clients=&peer;
#endif
 while(std::scanf("%i %i %f %f %f %i %f %f %f %f %f %f %f",
  &flags,&followed,&player.origin[0],&player.origin[1],&player.origin[2],&player.viewheight,
  &player.viewangles[0],&player.viewangles[1],&player.viewangles[2],&lean,
  &camera.s.origin2[0],&camera.s.origin2[1],&camera.s.origin2[2])==13){
  player.clientNum=followed;
#ifdef ORIGINAL
  player.leanf=lean;camera.r.svFlags=flags;
  SV_BuildClientSnapshot(&peer);
#else
  player.rtcw.leanf=lean;
  camera.r.svFlags=(flags&FNTECH3_WOLFET_SVF_SELF_PORTAL_EXCLUSIVE)?SVF_WOLFET_SELF_PORTAL_EXCLUSIVE:0;
  SV_SnapshotViewOrigin(&peer,player,observed);
#endif
  std::printf("%.9g %.9g %.9g\n",double(observed[0]),double(observed[1]),double(observed[2]));
 }
}
'''


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start)+2]


def corpus():
    rng = random.Random(84017)
    cases = []
    for flags in (0, 0x8000, 0x10000, 0x18000):
        for followed in (0, 1):
            for lean in (0, -16, -8, -1, 1, 8, 16):
                for angles in ((0,0,0), (0,90,0), (45,180,0), (-80,-170,45), (25,5,-120)):
                    for origin in ((0,0,0), (-1024,4096,-64), (64000,-64000,200)):
                        cases.append((flags, followed, *origin, 64, *angles, lean, 100, -200, 900))
    for _ in range(4096):
        cases.append((rng.choice((0,0x10000)), rng.randrange(2),
                      *(rng.uniform(-64000,64000) for _ in range(3)), rng.choice((-48,0,40,64,512)),
                      *(rng.uniform(-360,360) for _ in range(3)), rng.uniform(-16,16),
                      *(rng.uniform(-64000,64000) for _ in range(3))))
    return cases


def run(command, **kwargs):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90, **kwargs)
    if result.returncode:
        raise RuntimeError(result.stdout+result.stderr)
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--compiler', default='clang++')
    parser.add_argument('--target', default='i686-pc-windows-msvc' if os.name == 'nt' else None)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    reference = args.source.resolve()/'src'
    reference_snapshot = reference/'server/sv_snapshot.c'
    reference_math = reference/'game/q_math.c'
    own_snapshot = ROOT/'code/server/sv_snapshot.cpp'
    own_math = ROOT/'code/qcommon/q_math.c'
    report = dict(evidence='Source/Unit', target=args.target, passed=False)
    scratch = ROOT/'.tmp/wolfet-visibility'
    scratch.mkdir(parents=True, exist_ok=True)
    try:
        reference_text = f'#include "{(reference/"server/server.h").as_posix()}"\n#undef vsnprintf\n'+COMMON+REFERENCE
        reference_text += function(reference_math.read_text(), 'void AngleVectors(')
        reference_text += function(reference_snapshot.read_text(), 'static void SV_BuildClientSnapshot(')+MAIN
        own_text = '#include "code/server/server.h"\n#include "code/server/server_world_bounds.hpp"\n'+COMMON+ENGINE
        own_text += function(own_math.read_text(), 'void AngleVectors(')
        own_text += function(own_snapshot.read_text(), 'static void SV_SnapshotViewOrigin(')+MAIN
        inputs = corpus()
        stdin = '\n'.join(' '.join(map(str, case)) for case in inputs)+'\n'
        outputs = []
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder = Path(temporary)
            for original, text in ((True, reference_text), (False, own_text)):
                name = 'reference' if original else 'engine'
                source = folder/(name+'.cpp')
                source.write_text(text, encoding='utf-8')
                executable = folder/(name+('.exe' if os.name == 'nt' else ''))
                command = [args.compiler, '-std=c++17', '-I', str(ROOT), '-Wno-comment', '-D_CRT_SECURE_NO_WARNINGS']
                if args.target:
                    command += ['--target='+args.target]
                if original:
                    command += ['-DORIGINAL', '-fno-operator-names']
                run([*command, str(source), '-o', str(executable)])
                outputs.append(run([str(executable)], input=stdin).splitlines())
        if len(outputs[0]) != len(inputs) or outputs[0] != outputs[1]:
            mismatch = next((i for i, pair in enumerate(zip(*outputs)) if pair[0] != pair[1]), None)
            raise AssertionError(f'Viewpoint comparison failed at case {mismatch}; rows {list(map(len,outputs))}')
        report.update(passed=True, cases=len(inputs), comparison='exact nine-significant-digit float output',
                      sourceSha256={str(path.relative_to(reference)): hashlib.sha256(path.read_bytes()).hexdigest()
                                    for path in (reference_snapshot, reference_math)})
    except (OSError, RuntimeError, AssertionError, ValueError) as error:
        report['error'] = str(error)
        print(error)
    args.report.resolve().parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f"WolfET viewpoints: {'PASS' if report['passed'] else 'FAIL'} ({report.get('cases',0)} comparisons)")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
