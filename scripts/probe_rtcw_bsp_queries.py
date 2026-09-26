#!/usr/bin/env python3
"""Compare live BSP property getters with original RTCW and iortcw functions.

GPLv3 reference functions remain unchanged in disposable compilation fixtures
with their notices. Invalid-capacity and overflowing-integer reference calls
are excluded; production's stricter malformed-input behavior has unit tests.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from rtcw_bsp_query_tests import bsp_source, SIGNATURES
from rtcw_server_world_tests import function

COMPARE = r'''
struct GetterSet{
 const char *name;bool sp;bsp_t *world;
 int(*next)(int);int(*value)(int,char*,char*,int);int(*vector)(int,char*,float*);
 int(*scalar)(int,char*,float*);int(*integer)(int,char*,int*);
};
#define REF(name,isSP) {#name,isSP,&name::bspworld,name::AAS_NextBSPEntity,name::AAS_ValueForBSPEpairKey,\
 name::AAS_VectorForBSPEpairKey,name::AAS_FloatForBSPEpairKey,name::AAS_IntForBSPEpairKey}
static GetterSet references[]={REF(originalSP,true),REF(originalMP,false),REF(iortcwSP,true),REF(iortcwMP,false)};
#undef REF
static uint32_t state=0x81734629;
static uint32_t randomWord(){state=state*1664525u+1013904223u;return state;}
int main(){
 botimport.Print=print;
 std::vector<std::string> corpus={"","text","0","-0","+0","+ 2","-12.5 trailing",".5 2.75 -3.125",
  "1 2 3","1\t2\n3","1 2","1 bad 3","1e2 2e-3 0x1p-1","1e+ 2 3","0x1p+ 2 3",
  "  \t\r\n1 \t -2 3.25 fourth", "1,2,3","1.0.2 .3", "0x1.8p2 -0x1p-1 0x.8p1",
  "2147483647", "-2147483648", "1e-300 -1e-300 0", "3.40282346638528859811704183484516925440e38"};
 for(int size:{1,2,126,127,128,129,1023}){corpus.push_back(std::string(size,'x'));
  corpus.push_back(std::string(size,' ') + "1 2 3");}
 for(int i=0;i<2048;++i){
  const int a=int(randomWord()%1000000)-500000,b=int(randomWord()%1000000)-500000,c=int(randomWord()%1000000)-500000;
  const char *gap=i%3==0?"\t":i%3==1?" ":"\n";
  corpus.push_back(std::to_string(a)+".125"+gap+std::to_string(b)+".5"+gap+std::to_string(c)+".75suffix");
 }
 printf("{\"references\":[");bool first=true;
 for(const auto &ref:references){
  profile=&profiles[ref.sp?2:3];uint64_t strings=0,numbers=0,iterations=0;
  for(int count:{0,1,2,2048}){bspworld={};ref.world->numentities=bspworld.numentities=count;
   for(int ent:{INT_MIN,-1,0,1,2,3,2047,2048,INT_MAX-1}){assert(AAS_NextBSPEntity(ent)==ref.next(ent));++iterations;}}
  bspworld={};*ref.world={};bspworld.numentities=ref.world->numentities=3;
  char key[]="property",upper[]="Property",missing[]="missing",old[]="shadowed";
  bsp_epair_t second{key,old,nullptr},pair{key,nullptr,&second};
  bspworld.entities[1].epairs=ref.world->entities[1].epairs=&pair;
  for(auto &text:corpus){pair.value=text.data();
   for(int ent:{-1,0,1,2,3})for(char *query:{key,upper,missing}){
    for(int capacity:{1,2,7,127,128,129,1024,2048}){
     std::vector<char> a(capacity+2,'Z'),b(capacity+2,'Z');
     assert(AAS_ValueForBSPEpairKey(ent,query,a.data()+1,capacity)==ref.value(ent,query,b.data()+1,capacity));
     assert(a==b);++strings;
    }
    float a[3]={17,18,19},b[3]={17,18,19};
    assert(AAS_VectorForBSPEpairKey(ent,query,a)==ref.vector(ent,query,b)&&!std::memcmp(a,b,sizeof(a)));++numbers;
    float x=17,y=17;assert(AAS_FloatForBSPEpairKey(ent,query,&x)==ref.scalar(ent,query,&y)&&!std::memcmp(&x,&y,4));++numbers;
    int m=17,n=17;assert(AAS_IntForBSPEpairKey(ent,query,&m)==ref.integer(ent,query,&n)&&m==n);++numbers;
   }
  }
  if(!first)printf(",");first=false;
  printf("{\"name\":\"%s\",\"stringComparisons\":%llu,\"numericComparisons\":%llu,\"iteratorComparisons\":%llu}",
   ref.name,(unsigned long long)strings,(unsigned long long)numbers,(unsigned long long)iterations);
 }
 printf("]}");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    source = bsp_source(); paths = []
    for name, root in [('originalSP', args.sp_source / 'src'), ('originalMP', args.mp_source / 'src'),
                       ('iortcwSP', args.iortcw / 'SP/code'), ('iortcwMP', args.iortcw / 'MP/code')]:
        path = root / 'botlib/be_aas_bspq3.c'; text = path.read_text(encoding='utf-8', errors='replace'); paths.append(path)
        source += text[:text.index('*/') + 2] + f'\nnamespace {name} {{ static bsp_t bspworld;\n'
        for signature in SIGNATURES:
            source += function(text, signature.replace('static ', '')) + '\n'
        source += '}\n'
    source += COMPARE
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else ['-DFNTECH3_Q3_ONLY'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), sanitized=args.sanitize,
        scope='BSP property bytes/numeric values and entity iteration; not campaign or routing parity',
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        implementationSha256={str(p): hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in
            ['code/botlib/be_aas_bspq3.cpp', 'code/compat/rtcw_bsp_query.hpp']},
        referenceSources=[dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths])
    with tempfile.TemporaryDirectory(prefix='bsp-query-reference-', dir=report_path.parent) as directory:
        folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-D_CRT_SECURE_NO_WARNINGS',
                   '-I', str(ROOT), str(cpp), '-o', str(exe)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
        report.update(compileCommand=command, buildExitCode=built.returncode)
        if built.returncode:
            report['buildOutput'] = built.stdout + built.stderr
        else:
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            report.update(exitCode=run.returncode, stderr=run.stderr)
            if run.returncode:
                report['stdout'] = run.stdout
            else:
                report.update(json.loads(run.stdout))
        report['passed'] = not built.returncode and report.get('exitCode') == 0
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', len(report.get('references', [])), 'references')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
