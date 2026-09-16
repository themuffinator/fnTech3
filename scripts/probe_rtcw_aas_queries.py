#!/usr/bin/env python3
"""Compare basic AAS queries with unchanged original/iortcw functions.

GPLv3 reference functions and their notices enter disposable fixtures only.
Synthetic graphs prove query decisions, not retail navigation or gameplay.
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
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from rtcw_aas_query_tests import query_source
from rtcw_aas_trace_tests import trace_source
from rtcw_server_world_tests import function

FUNCTIONS = {
    'be_aas_sample': ('void AAS_PresenceTypeBoundingBox(', 'int AAS_PointAreaNum('),
    'be_aas_bspq3': ('int AAS_PointContents(',),
    'be_aas_reach': ('int AAS_AreaReachability(',),
    'be_aas_move': ('int AAS_Swimming(',),
}

COMPARE = r'''
static unsigned randomState=0x9437581u;
static unsigned randomValue(){randomState=randomState*1664525u+1013904223u;return randomState;}
static void graph(int world){
 assert(AAS_SetCurrentWorld(world));
 aasworld.loaded=1;aasworld.numareas=aasworld.numareasettings=33;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(33);
 aasworld.numnodes=32;aasworld.nodes=botlib::ClearedAlloc<aas_node_t>(32);
 aasworld.numplanes=32;aasworld.planes=botlib::ClearedAlloc<aas_plane_t>(32);
 for(int a=0;a<33;++a)aasworld.areasettings[a].numreachableareas=randomValue()%1000;
 for(int n=1;n<32;++n){auto &node=aasworld.nodes[n];node.planenum=n;
  for(int s=0;s<2;++s){int child=2*n+s;node.children[s]=child<32?child:((child%9)==0?0:-(child-31));}
  auto &plane=aasworld.planes[n];
  for(int i=0;i<3;++i)plane.normal[i]=float(int(randomValue()%5)-2)*.25f;
  plane.dist=float(int(randomValue()%201)-100)*.25f;
 }
}
int main(){
 profile=&profiles[MODE_SP?2:3];botimport.Print=print;botimport.PointContents=pointContents;
 unsigned comparisons=0;
 for(int seed=0;seed<32;++seed){
  for(int w=0;w<2;++w)graph(w);
  for(int w:{0,1,0}){
   assert(AAS_SetCurrentWorld(w));reference::selectWorld();
   for(int n=0;n<4096;++n){vec3_t v;
    for(float &f:v)f=float(int(randomValue()%1601)-800)*.125f;
    if(n<3)v[n]=0;
    assert(AAS_PointAreaNum(v)==reference::AAS_PointAreaNum(v));++comparisons;
   }
   for(int a=-1;a<35;++a){assert(AAS_AreaReachability(a)==reference::AAS_AreaReachability(a));++comparisons;}
   vec3_t v={};aasworld.loaded=0;
   assert(AAS_PointAreaNum(v)==reference::AAS_PointAreaNum(v));++comparisons;aasworld.loaded=1;
  }
  AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
 }
 for(int presence:{INT_MIN,-1,0,1,2,3,4,5,INT_MAX}){
  vec3_t lo,hi,rlo,rhi;AAS_PresenceTypeBoundingBox(presence,lo,hi);
  reference::AAS_PresenceTypeBoundingBox(presence,rlo,rhi);
  assert(!std::memcmp(lo,rlo,12)&&!std::memcmp(hi,rhi,12));++comparisons;
 }
 for(int i=0;i<65536;++i){vec3_t v={float(i),float(i%17),float(i%11)};
  unsigned bits=randomValue();std::memcpy(&contents,&bits,4);
  int own=AAS_PointContents(v),original=reference::AAS_PointContents(v);assert(own==original);++comparisons;
  own=AAS_Swimming(v);vec3_t observed;VectorCopy(contentsPoint,observed);
  original=reference::AAS_Swimming(v);
  assert(own==original&&!std::memcmp(observed,contentsPoint,12));++comparisons;
 }
 std::printf("{\"comparisons\":%u}\n",comparisons);
}
'''


COMPARE_TRACE = COMPARE[:COMPARE.index('int main()')] + r'''
static unsigned comparisons;
static void compare(vec3_t start,vec3_t end,int capacity,bool points){
 int ownAreas[66],refAreas[66];vec3_t ownPoints[66],refPoints[66];
 std::memset(ownAreas,0xa5,sizeof(ownAreas));std::memcpy(refAreas,ownAreas,sizeof(ownAreas));
 std::memset(ownPoints,0xa5,sizeof(ownPoints));std::memcpy(refPoints,ownPoints,sizeof(ownPoints));
 int own=AAS_TraceAreas(start,end,ownAreas,points?ownPoints:nullptr,capacity);
 int ref=reference::AAS_TraceAreas(start,end,refAreas,points?refPoints:nullptr,capacity);
 assert(own==ref&&!std::memcmp(ownAreas,refAreas,sizeof(ownAreas))&&!std::memcmp(ownPoints,refPoints,sizeof(ownPoints)));
 ++comparisons;
}
static void deepGraph(){
 assert(AAS_SetCurrentWorld(0));aasworld.loaded=1;aasworld.numareas=aasworld.numareasettings=202;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(202);
 aasworld.numnodes=201;aasworld.nodes=botlib::ClearedAlloc<aas_node_t>(201);
 aasworld.numplanes=201;aasworld.planes=botlib::ClearedAlloc<aas_plane_t>(201);
 for(int n=1;n<201;++n){auto &node=aasworld.nodes[n];node.planenum=n;
  node.children[0]=-(n+1);node.children[1]=n==200?-1:n+1;
  aasworld.planes[n].normal[0]=1;aasworld.planes[n].dist=-float(n);
 }
}
int main(){
 profile=&profiles[MODE_SP?2:3];botimport.Print=print;
 for(int seed=0;seed<32;++seed){
  for(int w=0;w<2;++w)graph(w);
  for(int w:{0,1,0}){
   assert(AAS_SetCurrentWorld(w));reference::selectWorld();
   for(int n=0;n<4096;++n){vec3_t start,end;
    for(int axis=0;axis<3;++axis){start[axis]=float(int(randomValue()%1601)-800)*.125f;end[axis]=float(int(randomValue()%1601)-800)*.125f;}
    if(n%17==0)VectorCopy(start,end);
    for(bool points:{false,true})compare(start,end,n%64+1,points);
   }
   vec3_t start{},end{};aasworld.loaded=0;compare(start,end,1,true);aasworld.loaded=1;
  }
  AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
 }
 // Deep traversal differs from stack occupancy: single-sided paths can
 // exceed 127 nodes; too many pending split segments retain original limits.
 deepGraph();reference::selectWorld();
 for(float a:{-300.0f,-250.0f,0.0f,20.0f})for(float b:{-300.0f,-250.0f,0.0f,20.0f})
 for(bool points:{false,true}){vec3_t start{a,3,7},end{b,11,15};compare(start,end,64,points);}
 AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
 std::printf("{\"comparisons\":%u}\n",comparisons);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw-source', type=Path, required=True)
    parser.add_argument('--queries', choices=('basic', 'trace'), default='basic')
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default='clang++')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('C++ compiler unavailable')
    output = args.report.resolve(); output.relative_to(ROOT / '.tmp')
    output.parent.mkdir(parents=True, exist_ok=True)
    target = [f'--target={args.target}'] if args.target else ([] if os.name == 'nt' else ['-m32'])
    triple = subprocess.check_output([compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip()
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    if os.name == 'nt' and triple.startswith('x86_64'):
        flags += ['-DFNTECH3_Q3_ONLY']
    functions = FUNCTIONS if args.queries == 'basic' else {'be_aas_sample': ('int AAS_TraceAreas(',)}
    factory = query_source if args.queries == 'basic' else trace_source
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        target=triple, sanitized=args.sanitize, querySet=args.queries,
        scope=f'{args.queries} queries on selected synthetic graphs; no gameplay evidence',
        implementationSha256={name: sha(ROOT / f'code/botlib/{name}.cpp') for name in functions},
        boundarySha256=sha(ROOT / 'code/server/server_rtcw_aas_query.hpp'), results=[])
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, variant, base in references:
        paths = [base / f'botlib/{name}.c' for name in functions]
        headers = ([base / 'botlib/aasfile.h', base / ('qcommon' if variant == 'iortcw' else 'game') / 'surfaceflags.h']
                   if args.queries == 'basic' else [])
        constants = []
        for header, names in zip(headers, [('PRESENCE_NORMAL', 'PRESENCE_CROUCH'), ('CONTENTS_LAVA', 'CONTENTS_SLIME', 'CONTENTS_WATER')]):
            text = header.read_text()
            for name in names:
                value = re.search(r'^#define\s+' + name + r'\s+(\d+)\s*$', text, re.M)[1]
                constants.append((name, value))
        source = paths[0].read_text().split('*/', 1)[0] + '*/\n#include <cstdio>\n' + factory()
        for name, value in constants:
            source += f'\nstatic_assert({name}=={value});\n#pragma push_macro("{name}")\n#undef {name}\n'
        source += '\n#undef aasworld\nnamespace reference {\naas_t *aasworld;\nvoid selectWorld(){aasworld=aas_currentWorld;}\n'
        for name, value in constants:
            source += f'constexpr int {name}={value};\n'
        for path, signatures in zip(paths, functions.values()):
            text = path.read_text()
            for signature in signatures:
                source += '\n' + function(text, signature)
        source += '\n}\n#define aasworld (*aas_currentWorld)\n'
        for name, _ in constants:
            source += f'#pragma pop_macro("{name}")\n'
        source += COMPARE if args.queries == 'basic' else COMPARE_TRACE
        row = dict(mode=mode, referenceVariant=variant,
            references=[dict(path=str(path), sha256=sha(path)) for path in [*paths, *headers]])
        report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='aas-query-reference-', dir=output.parent) as temp:
            cpp = Path(temp) / 'probe.cpp'; exe = Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            command = [compiler, *target, *flags, '-std=c++17', '-O2', '-I', str(ROOT),
                f'-DMODE_SP={int(mode == "sp")}', str(cpp), str(ROOT / 'code/server/server_game_data.cpp'),
                '-x', 'c++', str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            row.update(compileCommand=command, buildExitCode=built.returncode)
            if built.returncode:
                row['buildOutput'] = built.stdout + built.stderr
            else:
                env = os.environ.copy()
                if args.sanitize:
                    env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, env=env)
                row.update(exitCode=run.returncode, stderr=run.stderr)
                if run.returncode:
                    row['stdout'] = run.stdout
                else:
                    row.update(json.loads(run.stdout))
            row['passed'] = not built.returncode and row.get('exitCode') == 0
        output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(mode, variant, 'PASS' if row['passed'] else 'FAIL', row.get('comparisons', 0), 'comparisons', flush=True)
    return 0 if all(row['passed'] for row in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
