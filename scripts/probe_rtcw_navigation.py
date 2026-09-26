#!/usr/bin/env python3
"""Compare original/iortcw blocking decisions with the shared engine.

GPLv3 functions are used only in disposable fixtures with their notices. The
fixture models the raw signed module argument as int, avoiding C++ enum UB for
SP's negative sentinel. This is source-decision evidence, not retail AI parity.
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
from rtcw_navigation_tests import navigation_source, CHECKS
from rtcw_server_world_tests import function


REFERENCE = r'''
#undef aasworld
namespace reference {
using qboolean=int; // signed trap argument, including SP's -1 sentinel
constexpr int qtrue=1;
constexpr int MAX_AAS_WORLDS=2;
aas_t *aasworld;
void AAS_SetCurrentWorld(int i){assert(::AAS_SetCurrentWorld(i));aasworld=aas_currentWorld;}
int AAS_EnableRoutingArea(int a,int e){return ::AAS_EnableRoutingArea(a,e);}
int AAS_BBoxAreas(float *lo,float *hi,int *out,int n){return ::AAS_BBoxAreas(lo,hi,out,n);}
int VectorCompare(const float *a,const float *b){return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2];}
'''

COMPARE = r'''
int main(){
 botimport.Print=print;profile=&profiles[MODE_SP?2:3];
 const float pairs[][6]={{0,0,0,0,0,0},{3,3,3,3,3,3},{1,-2,-3,2,2,3},{-20,-20,-20,20,20,20}};
 int cases=0,matches=0,differences=0;
 for(int mask=0;mask<4;++mask)for(int initial=0;initial<2;++initial)for(int pattern=0;pattern<3;++pattern)
 for(const auto &pair:pairs)for(int flag:{-2,-1,0,1,2}){
  loadWorld(0,0);loadWorld(1,10);
  auto prepare=[&]{for(int w=0;w<2;++w){AAS_SetCurrentWorld(w);aasworld.loaded=(mask>>w)&1;
   for(int a=0;a<3;++a)aasworld.areasettings[a].areaflags=128|((a==pattern)?AREA_DISABLED:0);
  } AAS_SetCurrentWorld(initial);};
  vec3_t lo,hi;std::copy(pair,pair+3,lo);std::copy(pair+3,pair+6,hi);
  prepare();AAS_SetBlockingEntity(lo,hi,flag);int expected[2][3];
  assert(aas_currentWorld==&aasWorlds[1]);
  for(int w=0;w<2;++w)for(int a=0;a<3;++a)expected[w][a]=aasWorlds[w].areasettings[a].areaflags;
  prepare();reference::AAS_SetAASBlockingEntity(lo,hi,flag);bool equal=true;
  assert(aas_currentWorld==&aasWorlds[1]);
  for(int w=0;w<2;++w)for(int a=0;a<3;++a)equal &= expected[w][a]==aasWorlds[w].areasettings[a].areaflags;
  ++cases;if(equal)++matches;else{
   // iortcw SP changed the reset predicate to match MP. Original SP is the
   // current profile contract; retain the source difference explicitly.
   assert(MODE_SP&&IORTCW&&reference::VectorCompare(lo,hi)&&flag<=0);++differences;
  }
  AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
 }
 assert(!MODE_SP||!IORTCW||differences>0);
 std::printf("{\"cases\":%d,\"matching\":%d,\"knownResetDifferences\":%d}\n",cases,matches,differences);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw-source', type=Path)
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else ([] if os.name == 'nt' else ['-m32'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    if os.name == 'nt' and not args.target:
        flags += ['-DFNTECH3_Q3_ONLY']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        scope='Source blocking decisions with signed trap arguments and actual engine area queries; not retail AI parity',
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        sanitized=args.sanitize, implementationSha256={str(p): sha(ROOT / p) for p in
            ('code/botlib/be_aas_main.cpp', 'code/botlib/be_aas_sample.cpp', 'code/botlib/be_aas_route.cpp')}, references=[])
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, variant, root in references:
        path = root / 'botlib/be_aas_entity.c'; text = path.read_text()
        source = text[:text.index('*/') + 2] + '\n#include <cstdio>\n' + navigation_source()
        source += CHECKS[:CHECKS.index('int main()')] + REFERENCE
        source += function(text, 'void AAS_SetAASBlockingEntity(') + '\n}\n#define aasworld (*aas_currentWorld)\n' + COMPARE
        row = dict(mode=mode, variant=variant, source=str(path), sha256=sha(path)); report['references'].append(row)
        with tempfile.TemporaryDirectory(prefix='navigation-reference-', dir=report_path.parent) as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-D_CRT_SECURE_NO_WARNINGS',
                f'-DMODE_SP={int(mode == "sp")}', f'-DIORTCW={int(variant == "iortcw")}', '-I', str(ROOT), str(cpp),
                str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++', str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            row.update(compileCommand=command, buildExitCode=built.returncode)
            if built.returncode:
                row['buildOutput'] = built.stdout + built.stderr
            else:
                run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                row.update(exitCode=run.returncode, stderr=run.stderr)
                if not run.returncode: row.update(json.loads(run.stdout))
                else: row['stdout'] = run.stdout
            row['passed'] = not built.returncode and row.get('exitCode') == 0
        report_path.write_text(json.dumps(report, indent=2) + '\n')
        print(mode, variant, 'PASS' if row['passed'] else 'FAIL', row.get('cases', 0), 'cases', flush=True)
    return 0 if all(row['passed'] for row in report['references']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
