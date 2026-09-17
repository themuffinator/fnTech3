#!/usr/bin/env python3
"""Compare movement-link selection and view queries against RTCW source fixtures.

GPLv3 reference bodies and notices stay in disposable executables. Independent
navigation worlds share the engine's routing implementation and synthetic graph
geometry; visibility uses a controlled BSP trace callback. These are policy
comparisons, not original routing/collision or live campaign evidence.
For iortcw SP, which removed the separate movement-flags argument, that input
is omitted from its reference call and differences remain diagnostic.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from rtcw_movement_view_tests import view_source
from rtcw_route_query_tests import GRAPH
from rtcw_server_world_tests import function

COMPARE=r'''
int main(){
 profile=&profiles[MODE_SP?2:3];unsigned routeQueries=0,viewQueries=0,visibleQueries=0,differences=0,decisions=0;
 unsigned routeDifferences=0,viewDifferences=0,visibleDifferences=0,viewDecisions=0,visibleDecisions=0;
 const int masks[]={TFL_WALK|TFL_AIR,TFL_DEFAULT,-1,TFL_JUMP|TFL_AIR};
 auto difference=[&](bool value,bool decision,const char *kind,int area,int goal){
  differences+=value;decisions+=decision;
  if(!strcmp(kind,"route"))routeDifferences+=value;
  else if(!strcmp(kind,"view")){viewDifferences+=value;viewDecisions+=decision;}
  else{visibleDifferences+=value;visibleDecisions+=decision;}
  if(value&&differences<=3)fprintf(stderr,"%s area=%d goal=%d decisionDifference=%d\n",kind,area,goal,decision);
 };
 for(unsigned seed:{0u,17u,29u}){
  initialize();for(int world:{0,1}){AAS_SetCurrentWorld(world);graph(seed);}
  AAS_SetCurrentWorld(0);int handle=BotAllocMoveState();assert(handle==1);
  auto *ms=botmovestates[handle];reference::state={};
  const int areas=aasworld.numareas,links=aasworld.reachabilitysize;
  for(int area=1;area<areas;++area)for(int goalArea=1;goalArea<areas;++goalArea)
  for(int lastGoal:{0,goalArea})for(int previous:{0,(area+goalArea)%(areas-1)+1})
  for(int routeFlags:masks)for(int movementFlags:{masks[0],masks[1],masks[2]}){
   AAS_SetCurrentWorld(0);vec3_t origin;VectorCopy(aasworld.areas[area].center,origin);
   aasworld.frameroutingupdates=0;
   int actual=AAS_RTCWMovementReachability(origin,area,lastGoal,previous,goalArea,routeFlags,movementFlags);
   AAS_SetCurrentWorld(1);aasworld.frameroutingupdates=0;reference::bot_goal_t goal{};goal.areanum=goalArea;
   int expected=reference::BotGetReachabilityToGoal(origin,area,-1,lastGoal,previous,reference::state.avoidreach,
    reference::state.avoidreachtimes,reference::state.avoidreachtries,&goal,routeFlags
#if REFERENCE_SPLIT_FLAGS
    ,movementFlags
#endif
   );
   difference(actual!=expected,actual!=expected,"route",area,goalArea);++routeQueries;
  }
  for(int link=1;link<links;++link)for(int goalArea=1;goalArea<areas;++goalArea){
   AAS_SetCurrentWorld(0);int owner=0;
   for(int a=1;a<areas;++a){auto &s=aasworld.areasettings[a];
    if(link>=s.firstreachablearea&&link<s.firstreachablearea+s.numreachableareas){owner=a;break;}}
   assert(owner);*ms={};ms->lastreachnum=link;ms->lastareanum=owner;ms->lastgoalareanum=goalArea;
   VectorCopy(aasworld.areas[owner].center,ms->origin);ms->origin[0]+=.25f;
   reference::state={};reference::state.lastreachnum=link;reference::state.lastareanum=owner;
   reference::state.lastgoalareanum=goalArea;VectorCopy(ms->origin,reference::state.origin);
   bot_goal_t goal{};goal.areanum=goalArea;goal.entitynum=1000;VectorCopy(aasworld.areas[goalArea].center,goal.origin);
   reference::bot_goal_t refGoal{};refGoal.areanum=goalArea;refGoal.entitynum=goal.entitynum;VectorCopy(goal.origin,refGoal.origin);
   for(float look:{0.0f,1.0f,25.0f,200.0f})for(int flags:masks){
    vec3_t actualPoint{-71,-73,-79},expectedPoint{-71,-73,-79};
    AAS_SetCurrentWorld(0);aasworld.frameroutingupdates=0;
    int actual=BotMovementViewTarget(handle,&goal,flags,look,actualPoint);
    AAS_SetCurrentWorld(1);aasworld.frameroutingupdates=0;
    int expected=reference::BotMovementViewTarget(handle,&refGoal,flags,look,expectedPoint);
    difference(actual!=expected||memcmp(actualPoint,expectedPoint,12),actual!=expected,"view",owner,goalArea);++viewQueries;
   }
   VectorCopy(goal.origin,expectedEye);expectedPass=goal.entitynum;
   for(int visibility:{0,1,2,5})for(int flags:{masks[0],masks[2]}){
    vec3_t actualPoint{-71,-73,-79},expectedPoint{-71,-73,-79};visibleTrace=visibility;traceCalls=0;
    AAS_SetCurrentWorld(0);aasworld.frameroutingupdates=0;botimport.Trace=sightTrace;
    int actual=BotPredictVisiblePosition(ms->origin,owner,&goal,flags,actualPoint);int actualTraces=traceCalls;
    AAS_SetCurrentWorld(1);aasworld.frameroutingupdates=0;traceCalls=0;
    int expected=reference::BotPredictVisiblePosition(ms->origin,owner,&refGoal,flags,expectedPoint);
    difference(actual!=expected||memcmp(actualPoint,expectedPoint,12)||actualTraces!=traceCalls,
      actual!=expected,"visible",owner,goalArea);++visibleQueries;
   }
  }
  BotShutdownMoveAI();shutdown();
 }
 printf("{\"routeQueries\":%u,\"viewQueries\":%u,\"visibleQueries\":%u,\"recordDifferences\":%u,\"decisionDifferences\":%u,"
  "\"routeDifferences\":%u,\"viewDifferences\":%u,\"visibleDifferences\":%u,\"viewDecisionDifferences\":%u,\"visibleDecisionDifferences\":%u}\n",
  routeQueries,viewQueries,visibleQueries,differences,decisions,routeDifferences,viewDifferences,visibleDifferences,viewDecisions,visibleDecisions);
}
'''


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base,sp):
    # Rename only the engine-side helper to avoid C++ ADL finding it while
    # unchanged reference code passes the shared engine reachability record.
    source=view_source().replace('BotValidTravel(', 'Engine_BotValidTravel(')+GRAPH
    source+='\n#pragma push_macro("AVOIDREACH")\n#undef AVOIDREACH\n'
    source+='#pragma push_macro("aasworld")\n#undef aasworld\n#define aasworld aas_currentWorld\nnamespace reference {\n'
    math=base/('qcommon/q_math.c' if (base/'qcommon/q_math.c').is_file() else 'game/q_math.c')
    move=base/'botlib/be_ai_move.c'
    goal=base/('botlib/be_ai_goal.h' if (base/'botlib/be_ai_goal.h').is_file() else 'game/be_ai_goal.h')
    aas=base/('botlib/be_aas.h' if (base/'botlib/be_aas.h').is_file() else 'game/be_aas.h')
    aasfile=base/'botlib/aasfile.h'
    paths=[math,move,goal,aas,aasfile]
    for path,name in [(aas,'TFL_DONOTENTER_LARGE'),(aasfile,'AREACONTENTS_DONOTENTER_LARGE')]:
        value=re.search(r'^#define\s+'+name+r'\s+(\w+)',path.read_text(),re.M)[1]
        source+=f'static constexpr int {name}={int(value,0)};\n'
    for path in [math,goal,move]:
        text=path.read_text();source+=text.split('*/',1)[0]+'*/\n'
        if path==math:
            for name in ['VectorNormalize','VectorNormalize2']:
                signature=re.search(r'^(?:vec_t|float)\s+'+name+r'\s*\(',text,re.M)[0]
                source+=function(text,signature)+'\n'
        else:
            name='bot_goal' if path==goal else 'bot_movestate'
            start=text.index('typedef struct '+name+'_s')
            end='} '+name+'_t;'
            source+=text[start:text.index(end,start)+len(end)]+'\n'
    source+='static bot_movestate_t state;\nbot_movestate_t *BotMoveStateFromHandle(int n){assert(n==1);return &state;}\n'
    source+='int AAS_AreaContentsTravelFlag(int n){return ::AAS_AreaContentsTravelFlags(n);}\n'
    for filename,signatures in {
        'be_aas_reach.c':['int AAS_AreaDoNotEnterLarge('],
        'be_aas_route.c':['int AAS_AreaTravelTimeToGoalAreaCheckLoop('] if sp else [],
        'be_ai_move.c':['int BotValidTravel(', 'int BotGetReachabilityToGoal(', 'int BotAddToTarget(',
            'int BotMovementViewTarget(', 'int BotVisible(', 'int BotPredictVisiblePosition('],
    }.items():
        if not signatures:continue
        path=base/'botlib'/filename;text=path.read_text();source+=text.split('*/',1)[0]+'*/\n'
        for signature in signatures:source+=function(text,signature)+'\n'
        if path not in paths:paths.append(path)
    split='movetravelflags' in function(move.read_text(),'int BotGetReachabilityToGoal(')
    source+='\n}\n#pragma pop_macro("aasworld")\n#pragma pop_macro("AVOIDREACH")\n'
    source+=f'#define REFERENCE_SPLIT_FLAGS {int(split)}\n'+COMPARE
    return source,[dict(path=str(p),sha256=sha(p)) for p in paths]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['sp-source','mp-source','iortcw-source','report']:parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--compiler',default='clang++');parser.add_argument('--target')
    parser.add_argument('--sanitize',action='store_true');parser.add_argument('--fast-math',action='store_true')
    args=parser.parse_args();compiler=shutil.which(args.compiler)
    if not compiler:parser.error('C++ compiler unavailable')
    output=args.report.resolve();output.relative_to(ROOT/'.tmp');output.parent.mkdir(parents=True,exist_ok=True)
    target=['--target='+args.target] if args.target else ([] if os.name=='nt' else ['-m32','-msse2','-mfpmath=sse'])
    triple=subprocess.check_output([compiler,*target,'-dumpmachine'],cwd=ROOT,text=True).strip()
    flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    flags+=['-ffast-math'] if args.fast_math else ['-ffp-contract=off']
    if triple.startswith('x86_64'):flags+=['-DFNTECH3_Q3_ONLY']
    files=['code/botlib/be_ai_move.cpp','code/botlib/be_aas_route.cpp','code/botlib/be_aas_reach.cpp',
        'code/botlib/rtcw_navigation.h','code/botlib/rtcw_movement.hpp','code/server/server_rtcw_movement_view.hpp',
        'code/compat/rtcw_game_abi.h','tests/rtcw_goal_route_tests.py','tests/rtcw_movement_view_tests.py',
        'tests/rtcw_route_query_tests.py','scripts/probe_rtcw_movement_views.py']
    report=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope=__doc__,target=triple,
        sanitized=args.sanitize,fastMath=args.fast_math,implementationSha256={name:sha(ROOT/name) for name in files},results=[])
    env=os.environ.copy()
    if args.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                         ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
        source,provenance=fixture(base,variant.endswith('SP'))
        row=dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode()).hexdigest(),
                 referenceHasSeparateMovementFlags='#define REFERENCE_SPLIT_FLAGS 1' in source);report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='movement-view-',dir=output.parent) as temp:
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
            row['passed']=not built.returncode and row.get('exitCode')==0 and row.get('routeQueries')==20736 \
                and row.get('viewQueries')==33984 and row.get('visibleQueries')==16992
            row['exactAgreement']=row['passed'] and row.get('recordDifferences')==0
        output.write_text(json.dumps(report,indent=2)+'\n')
        print(variant,'PASS' if row['passed'] else 'FAIL',row.get('recordDifferences'),row.get('decisionDifferences'),flush=True)
    return 0 if all(row['passed'] and (row['exactAgreement'] or args.fast_math or row['reference'].startswith('iortcw'))
                    for row in report['results']) else 1


if __name__=='__main__':raise SystemExit(main())
