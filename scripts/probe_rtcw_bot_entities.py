#!/usr/bin/env python3
"""Compare RTCW entity updates with published functions in disposable GPLv3 fixtures.

Entity records and link/model callback arguments are compared. Actual link-heap
ownership, malformed input and shutdown are covered by rtcw_bot_entity_tests.py.
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
from rtcw_server_world_tests import function

HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/botlib/botlib.h"
#include "code/botlib/be_aas.h"
#include "code/botlib/aasfile.h"
#include "code/compat/rtcw_bot_entity.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
int Com_GameIsRTCW(){return 1;}
void QDECL Com_Error(errorParm_t,const char *,...){std::abort();}
static void QDECL print(int,const char *,...){}
static botlib_import_t botimport;
#define aasworld (*aas_currentWorld)
'''

CONTEXT = r'''
struct aas_s;
struct aas_link_t{};
struct bsp_link_t{};
struct aas_entity_t{aas_entityinfo_t i;aas_link_t *areas;aas_s *areaWorld;bsp_link_t *leaves;};
struct aas_s{int loaded,initialized,numframes,maxentities;float time;aas_entity_t *entities;};
using aas_t=aas_s;
static aas_entity_t records[1024];
static aas_t worlds[2];
static aas_t *aas_currentWorld=worlds,*defaultaasworld=worlds;
static aas_t &AAS_EntityWorld(){return worlds[0];}
static float AAS_Time(){return aasworld.time;}
struct CallData{
 int modelCalls,unlinks,links,bspUnlinks,bspLinks,model,entity,presence,world;
 float angles[3],mins[3],maxs[3];
};
static CallData calls{};
static void AAS_BSPModelMinsMaxsOrigin(int model,vec3_t angles,vec3_t mins,vec3_t maxs,vec3_t origin){
 assert(!origin);++calls.modelCalls;calls.model=model;VectorCopy(angles,calls.angles);
 for(int i=0;i<3;++i){mins[i]=-float(model+1+i);maxs[i]=float(model+2+i);}
}
static void AAS_UnlinkFromAreas(aas_link_t*){++calls.unlinks;}
static aas_link_t *AAS_LinkEntityClientBBox(vec3_t mins,vec3_t maxs,int entity,int presence){
 ++calls.links;calls.entity=entity;calls.presence=presence;calls.world=int(aas_currentWorld-worlds);
 VectorCopy(mins,calls.mins);VectorCopy(maxs,calls.maxs);return nullptr;
}
static void AAS_UnlinkFromBSPLeaves(bsp_link_t*){++calls.bspUnlinks;}
static bsp_link_t *AAS_BSPLinkEntity(vec3_t mins,vec3_t maxs,int entity,int model){
 assert(!model&&entity==calls.entity);assert(VectorCompare(mins,calls.mins)&&VectorCompare(maxs,calls.maxs));
 ++calls.bspLinks;return nullptr;
}
'''

COMPARE = r'''
static uint32_t seed=0x421bc982;
static unsigned randomWord(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static unsigned comparisons;
static void compare(int entity,bot_entitystate_t &state){
 engine::calls={};reference::calls={};auto before=state;
 const auto expected=reference::AAS_UpdateEntity(entity,&state);
 const auto actual=engine::AAS_UpdateEntity(entity,&state);
 static_assert(sizeof(engine::calls)==sizeof(reference::calls));
 if(expected!=actual||std::memcmp(&engine::records[entity].i,&reference::records[entity].i,sizeof(aas_entityinfo_t))||
    std::memcmp(&engine::calls,&reference::calls,sizeof(engine::calls))||std::memcmp(&before,&state,sizeof(state))){
  std::fprintf(stderr,"entity update mismatch at comparison %u entity %d solid %d\n",comparisons,entity,state.solid);
  std::abort();
 }++comparisons;
}
int main(){
 botimport.Print=print;
 engine::worlds[0].entities=engine::records;reference::worlds[0].entities=reference::records;
 engine::worlds[0].maxentities=reference::worlds[0].maxentities=1024;
 for(int n=0;n<1024;++n)engine::records[n].i.event=reference::records[n].i.event=n;
 for(unsigned step=0;step<100000;++step){
  const int selected=randomWord()%2;engine::aas_currentWorld=&engine::worlds[selected];
  reference::aas_currentWorld=&reference::worlds[selected];
  for(int w=0;w<2;++w){
   engine::worlds[w].time=reference::worlds[w].time=float(step)*.25f+w*.125f;
   engine::worlds[w].numframes=reference::worlds[w].numframes=step%4;
   engine::worlds[w].loaded=reference::worlds[w].loaded=step%983!=0;
  }
  bot_entitystate_t state{};const int entity=step%64==0?ENTITYNUM_WORLD:int(randomWord()%1024);
  state.type=randomWord()%20;state.flags=randomWord();state.solid=randomWord()%4;
  for(int i=0;i<3;++i){state.origin[i]=float(int(randomWord()%8192)-4096)*.125f;
   state.old_origin[i]=state.origin[i]-.25f;state.angles[i]=float(int(randomWord()%720)-360);
   state.mins[i]=-float(randomWord()%80);state.maxs[i]=float(randomWord()%80);}
  state.groundent=randomWord()%1024;state.modelindex=randomWord()%64;state.modelindex2=randomWord()%64;
  state.frame=randomWord()%512;state.event=randomWord()%512;state.eventParm=randomWord()%1024;
  state.powerups=randomWord();state.weapon=randomWord()%64;state.legsAnim=randomWord()%256;state.torsoAnim=randomWord()%256;
  compare(entity,state);
  // Identical updates, solid changes and zero-time repeats exercise relink rules.
  if(step%3==0)compare(entity,state);
  if(step%5==0){state.solid=SOLID_BBOX;compare(entity,state);}
 }
 std::printf("{\"updates\":%u,\"recordComparisons\":%u,\"callbackComparisons\":%u}\n",comparisons,comparisons,comparisons);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path, help='Original RTCW tree or iortcw SP/MP subtree')
    parser.add_argument('--mode', choices=('sp', 'mp'), required=True)
    parser.add_argument('--iortcw', action='store_true')
    parser.add_argument('--compiler', default='clang++')
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('C++ compiler not found')
    report_path = args.report.resolve(); report_path.parent.mkdir(parents=True, exist_ok=True)
    reference = args.source.resolve() / ('code' if args.iortcw else 'src') / 'botlib/be_aas_entity.c'
    original = reference.read_text()
    own = ROOT / 'code/botlib/be_aas_entity.cpp'; implementation = own.read_text()
    source = original[:original.index('*/')+2] + '\n' + HOST
    source += '\nnamespace engine {\n' + CONTEXT
    source += function(implementation, 'static void AAS_UnlinkEntityAreas(') + '\n'
    source += function(implementation, 'int AAS_UpdateEntity(') + '\n}\nnamespace reference {\n' + CONTEXT
    source += function(original, 'int AAS_UpdateEntity(') + '\n}\n' + COMPARE
    target = [f'--target={args.target}'] if args.target else ([] if os.name == 'nt' else ['-m32'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), mode=args.mode,
        referenceVariant='iortcw' if args.iortcw else 'original',
        scope='Entity records and link/model callback arguments; shared heap ownership tested separately',
        target=subprocess.check_output([compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        sanitized=args.sanitize, implementationSha256=sha(own),
        adapterSha256=sha(ROOT / 'code/compat/rtcw_bot_entity.hpp'),
        references=[dict(path=str(reference), sha256=sha(reference))])
    with tempfile.TemporaryDirectory(prefix='entity-reference-', dir=report_path.parent) as temporary:
        folder = Path(temporary); cpp = folder/'probe.cpp'; exe = folder/'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        diagnostic = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' and report['target'].startswith('x86_64') else []
        command = [compiler, *target, *flags, *diagnostic, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
        report.update(compileCommand=command, buildExitCode=built.returncode)
        if built.returncode:
            report['buildOutput'] = built.stdout + built.stderr
        else:
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)
            report.update(exitCode=run.returncode, stderr=run.stderr)
            if not run.returncode: report.update(json.loads(run.stdout))
            else: report['stdout'] = run.stdout
        report['passed'] = not built.returncode and report.get('exitCode') == 0
    report_path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', args.mode, report.get('updates', 0), 'updates')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
