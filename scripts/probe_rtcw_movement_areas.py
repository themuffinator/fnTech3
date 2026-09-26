#!/usr/bin/env python3
"""Compare movement-area queries against original and iortcw source fixtures.

GPLv3 reference bodies execute only in disposable programs with their notices.
Both implementations use fnTech3's actual AAS geometry traces and a controlled
BSP ground trace. This is source-policy evidence, not original BSP or live AI
parity. No game module or proprietary input is used.
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
sys.path.insert(0, str(ROOT/'tests'))
from rtcw_movement_area_tests import movement_area_source, GEOMETRY
from rtcw_server_world_tests import function

COMPARE = r'''
int main(){
 profile=&profiles[MODE_SP?2:3];setup();geometry();geometry(1);
 unsigned queries=0,areaQueries=0,modelQueries=0;
 for(int world:{0,1}){AAS_SetCurrentWorld(world);
  for(int a:{INT_MIN,-1,0,1,2,3,INT_MAX})for(int disabled:{0,AREA_DISABLED}){
   aasworld.areasettings[1].areaflags=aasworld.areasettings[2].areaflags=disabled;
   assert(AAS_AreaReachability(a)==reference::AAS_AreaReachability(a));++areaQueries;
  }
  for(int type:{TRAVEL_ELEVATOR,TRAVEL_FUNCBOB,TRAVEL_ELEVATOR|TRAVELFLAG_NOTTEAM1,
                TRAVEL_FUNCBOB|TRAVELFLAG_NOTTEAM1,TRAVEL_WALK,0,-1}){
   aasworld.reachability[1].traveltype=type;
   for(int start:{INT_MIN,-1,0,1,2,3,INT_MAX})for(int model:{-1,0,3,4,65535,INT_MAX}){
    assert(AAS_NextModelReachability(start,model)==reference::AAS_NextModelReachability(start,model));++modelQueries;
   }
  }
  aasworld.reachability[1].traveltype=TRAVEL_ELEVATOR;
  aasworld.reachability[1].areanum=world?2:1;
  aasworld.reachability[2].areanum=world?1:2;
  AAS_EntityWorld().entities[2].i.modelindex=4;
  for(float z:{-15.0f,-.125f,0.0f,.125f,4.0f,50.0f,99.75f,100.0f,100.25f,101.0f,125.0f,300.0f,500.0f,1024.0f})
  for(int hit=0;hit<6;++hit)for(int modelType=0;modelType<4;++modelType)
  for(int links=0;links<4;++links)for(int disabled=0;disabled<4;++disabled)
  for(int liquid:{0,CONTENTS_WATER,CONTENTS_SLIME}){
   vec3_t origin{5,7,z};const auto before=std::array<float,3>{origin[0],origin[1],origin[2]};
   modeltypes[3]=modelType;modeltypes[4]=3-modelType;
   ground={};ground.fraction=hit==4?1:.5f;ground.startsolid=hit==5?qtrue:qfalse;
   ground.ent=hit==0?ENTITYNUM_WORLD:hit==1?ENTITYNUM_NONE:hit==2?2:1;
   aasworld.areasettings[1].numreachableareas=links&1?2:0;
   aasworld.areasettings[2].numreachableareas=links&2?1:0;
   aasworld.areasettings[1].areaflags=disabled&1?AREA_DISABLED:0;
   aasworld.areasettings[2].areaflags=disabled&2?AREA_DISABLED:0;
   contents=liquid;expectedPass=queries%3==0?-1:int(queries%1024);
   bspCalls=contentsCalls=entityTraces=0;
   int actual=BotReachabilityArea(origin,expectedPass);
   const auto actualCalls=std::array<int,3>{bspCalls,contentsCalls,entityTraces};
   assert(!memcmp(origin,before.data(),12));
   bspCalls=contentsCalls=entityTraces=0;
   int expected=reference::BotReachabilityArea(origin,expectedPass);
   const auto expectedCalls=std::array<int,3>{bspCalls,contentsCalls,entityTraces};
   if(actual!=expected||actualCalls!=expectedCalls){
    fprintf(stderr,"world=%d z=%g hit=%d model=%d links=%d disabled=%d liquid=%d actual=%d expected=%d\n",
      world,z,hit,modelType,links,disabled,liquid,actual,expected);return 1;
   }
   assert(!memcmp(origin,before.data(),12));++queries;
  }
 }
 finish();printf("{\"queries\":%u,\"areaQueries\":%u,\"modelQueries\":%u}\n",queries,areaQueries,modelQueries);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base):
    source = movement_area_source()+GEOMETRY
    source += '\n#pragma push_macro("aasworld")\n#undef aasworld\n#define aasworld aas_currentWorld\nnamespace reference {\n'
    paths = []
    for filename, signatures in {
        'be_aas_reach.c': ('int AAS_AreaReachability(',),
        'be_aas_route.c': ('int AAS_NextModelReachability(',),
        'be_ai_move.c': ('int BotFuzzyPointReachabilityArea(', 'int BotReachabilityArea('),
    }.items():
        path = base/'botlib'/filename
        text = path.read_text()
        source += text.split('*/',1)[0]+'*/\n'
        for signature in signatures:
            source += function(text,signature)+'\n'
        paths.append(dict(path=str(path),sha256=sha(path)))
    source += '\n}\n#pragma pop_macro("aasworld")\n'+COMPARE
    return source,paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['sp-source','mp-source','iortcw-source','report']:
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--compiler',default='clang++')
    parser.add_argument('--target')
    parser.add_argument('--sanitize',action='store_true')
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:parser.error('C++ compiler unavailable')
    output = args.report.resolve();output.relative_to(ROOT/'.tmp');output.parent.mkdir(parents=True,exist_ok=True)
    target = ['--target='+args.target] if args.target else ([] if os.name=='nt' else ['-m32'])
    triple = subprocess.check_output([compiler,*target,'-dumpmachine'],cwd=ROOT,text=True).strip()
    flags = ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    if triple.startswith('x86_64'):flags += ['-DFNTECH3_Q3_ONLY']
    files = ['code/botlib/be_ai_move.cpp','code/botlib/be_aas_reach.cpp','code/botlib/be_aas_route.cpp',
        'code/botlib/be_aas_sample.cpp','code/botlib/be_aas_bspq3.cpp','code/botlib/be_aas_entity.cpp',
        'code/server/server_rtcw_botlib.hpp','tests/rtcw_movement_area_tests.py','scripts/probe_rtcw_movement_areas.py']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope=__doc__,target=triple,
        sanitized=args.sanitize,implementationSha256={name:sha(ROOT/name) for name in files},results=[])
    env = os.environ.copy()
    if args.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                         ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
        source,provenance = fixture(base)
        row = dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode()).hexdigest())
        report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='movement-areas-',dir=output.parent) as temp:
            folder=Path(temp);cpp=folder/'probe.cpp';exe=folder/'probe.exe';cpp.write_text(source,encoding='utf-8')
            command=[compiler,*target,*flags,'-std=c++17','-O2','-D_CRT_SECURE_NO_WARNINGS','-I',str(ROOT),
                f'-DMODE_SP={int(variant.endswith("SP"))}',str(cpp),str(ROOT/'code/server/server_game_data.cpp'),
                '-x','c++',str(ROOT/'code/compat/game_abi_adapter.c'),'-o',str(exe)]
            built=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
            row.update(compileCommand=command,buildExitCode=built.returncode,buildOutput=built.stdout+built.stderr)
            if not built.returncode:
                options={'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
                run=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=120,env=env,**options)
                row.update(exitCode=run.returncode,stderr=run.stderr)
                if not run.returncode:row.update(json.loads(run.stdout))
                else:row['stdout']=run.stdout
            row['passed']=not built.returncode and row.get('exitCode')==0 and row.get('queries')==32256
        output.write_text(json.dumps(report,indent=2)+'\n')
        print(variant,'PASS' if row['passed'] else 'FAIL',row.get('queries'),flush=True)
    return 0 if all(row['passed'] for row in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
