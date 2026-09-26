#!/usr/bin/env python3
"""Compare movement-to-goal orchestration with disposable RTCW source programs.

Original GPLv3 bodies and their notices stay in temporary programs. The sides
share engine AAS collision/prediction/actions and controlled route/BSP services.
Reference-local undefined result fields are seeded and excluded; caller-owned
optional fields remain part of the comparison. This is not live AI evidence.
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
from rtcw_move_goal_tests import goal_source, SCENE
from rtcw_travel_tests import active_function, TRAVEL_NAMES
from rtcw_server_world_tests import function
import probe_rtcw_travel as travel


COMPARE=r'''
static unsigned bits(float value){unsigned n;memcpy(&n,&value,4);return n;}
template<class S>static auto stateRecord(const S &s){
 return std::array<unsigned,16>{unsigned(s.areanum),unsigned(s.lastareanum),unsigned(s.lastgoalareanum),
  unsigned(s.lastreachnum),unsigned(s.reachareanum),unsigned(s.moveflags),unsigned(s.jumpreach),
  bits(s.lastorigin[0]),bits(s.lastorigin[1]),bits(s.lastorigin[2]),bits(s.lasttime),
  bits(s.grapplevisible_time),bits(s.lastgrappledist),bits(s.reachability_time),
  unsigned(s.avoidreach[0]),unsigned(s.avoidreachtries[0])};
}
template<class S>static void copyState(const bot_movestate_t &in,S &out){out={};
 VectorCopy(in.origin,out.origin);VectorCopy(in.velocity,out.velocity);VectorCopy(in.viewoffset,out.viewoffset);
 VectorCopy(in.viewangles,out.viewangles);VectorCopy(in.lastorigin,out.lastorigin);
 out.entitynum=in.entitynum;out.client=in.client;out.thinktime=in.thinktime;out.presencetype=in.presencetype;
 out.areanum=in.areanum;out.lastareanum=in.lastareanum;out.lastgoalareanum=in.lastgoalareanum;
 out.lastreachnum=in.lastreachnum;out.reachareanum=in.reachareanum;out.moveflags=in.moveflags;out.jumpreach=in.jumpreach;
 out.lasttime=in.lasttime;out.grapplevisible_time=in.grapplevisible_time;out.lastgrappledist=in.lastgrappledist;
 out.reachability_time=in.reachability_time;
}
int main(){
 profile=&profiles[MODE_SP?2:3];auto *ms=goalSetup();
 FreeMemory(aasworld.areas);FreeMemory(aasworld.areasettings);FreeMemory(aasworld.reachability);
 aasworld.numareas=aasworld.numareasettings=6;aasworld.reachabilitysize=10;
 aasworld.areas=botlib::ClearedAlloc<aas_area_t>(6);
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(6);
 aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(10);AAS_InitAASLinkedEntities();
 for(int i=1;i<6;++i){aasworld.areasettings[i].presencetype=PRESENCE_NORMAL|PRESENCE_CROUCH;
  aasworld.areasettings[i].numreachableareas=3;aasworld.areasettings[i].firstreachablearea=1;}
 unsigned queries=0,differences=0,decisions=0,definedWords=0,undefinedWords=0;
 const int types[]={TRAVEL_WALK,TRAVEL_CROUCH,TRAVEL_BARRIERJUMP,TRAVEL_LADDER,TRAVEL_WALKOFFLEDGE,
  TRAVEL_JUMP,TRAVEL_SWIM,TRAVEL_WATERJUMP,TRAVEL_TELEPORT,TRAVEL_ELEVATOR,TRAVEL_GRAPPLEHOOK,
  TRAVEL_ROCKETJUMP,TRAVEL_JUMPPAD,TRAVEL_FUNCBOB,TRAVEL_BFGJUMP,TRAVEL_WALK|TRAVELFLAG_NOTTEAM1};
 std::array<unsigned,16> perTravel{};
 const float positions[][3]={{0,0,1},{4,3,1},{40,16,100},{-20,9,60},{2,3,-10},{0,0,2.9f}};
 for(int ti=0;ti<16;++ti)for(int pi=0;pi<6;++pi)for(int si=0;si<8;++si)
 for(int gi=0;gi<4;++gi)for(int obstacle=0;obstacle<3;++obstacle)for(int variant=0;variant<2;++variant){
  for(int i=0;i<10;++i){auto &r=aasworld.reachability[i];r=link(5,7,1,80,40,variant?100:0);
   r.traveltype=types[ti];r.areanum=2;r.edgenum=100;}
  for(int i=7;i<=9;++i){auto &r=aasworld.reachability[i];r.areanum=i-4;VectorSet(r.end,100*i,20*i,30);}
  routeCosts[2]=300;routeCosts[3]=variant?180:300;routeCosts[4]=variant?179:100;
  routeLinks[2]=7;routeLinks[3]=8;routeLinks[4]=9;
  bot_movestate_t initial{};VectorCopy(positions[pi],initial.origin);VectorCopy(initial.origin,initial.lastorigin);
  initial.presencetype=PRESENCE_NORMAL;initial.client=0;initial.entitynum=1000;initial.thinktime=.1f;
  initial.velocity[0]=variant?200:0;initial.velocity[2]=variant?49:0;initial.viewoffset[2]=24;
  initial.areanum=initial.lastareanum=initial.reachareanum=1;initial.lastgoalareanum=gi==0?1:gi==1?2:5;
  initial.lastreachnum=si==3?0:1;initial.jumpreach=variant;initial.reachability_time=si==2?4:9;
  initial.lasttime=si==1?4.9f:si==4?5.1f:4;initial.moveflags=si%2?0:MFL_ONGROUND;
  if(si==5)initial.moveflags|=MFL_ACTIVEGRAPPLE;
  initial.lastgrappledist=1000;initial.grapplevisible_time=5;
  auto goal=destination(initial.lastgoalareanum);if(gi==0)VectorSet(goal.origin,5,0,1);
  goal.flags=variant?GFL_NOSLOWAPPROACH:0;
  reference::bot_goal_t otherGoal{};VectorCopy(goal.origin,otherGoal.origin);otherGoal.areanum=goal.areanum;
  otherGoal.flags=variant?reference::GFL_NOSLOWAPPROACH:0;
  if(gi==3)initial.lastgoalareanum=0;
  selectedReach=si==5?0:2;obstruction=obstacle==0?ENTITYNUM_WORLD:obstacle==1?1:7;traceSolid=variant&&pi==4;
  contents=variant?(pi==3?CONTENTS_WATER:pi==4?CONTENTS_SLIME:0):0;
  aasworld.areasettings[1].contents=ti==12?AREACONTENTS_JUMPPAD:0;
  modeltypes[3]=ti==9?MODELTYPE_FUNC_PLAT:ti==13?MODELTYPE_FUNC_BOB:0;
  AAS_EntityWorld().entities[1].i.origin[2]=variant?50:0;
  *ms=initial;copyState(initial,reference::state);
  const int handle=si==7?0:1,flags=-1;
  bot_moveresult_t actual;reference::bot_moveresult_t expected;
  unsigned seeds[13];for(int i=0;i<13;++i)seeds[i]=0xc28e0000u+unsigned(i);
  memcpy(&actual,seeds,52);memcpy(&expected,seeds,52);reference::callerResult=&expected;
  selectionCalls=routeCalls=0;resetActions();srand(713+pi);
  Engine_BotMoveToGoal(&actual,handle,si==6?nullptr:&goal,flags);
  const auto actualInput=botinputs[0];
  const auto actualMoves=moveActions;const auto actualCommands=commands;const auto actualState=stateRecord(*ms);
  const int actualTraces=bspCalls,actualRoutes=routeCalls,actualSelections=selectionCalls;
  const int actualRouteFlags=observedRouteFlags,actualMoveFlags=observedMoveFlags;
  selectionCalls=routeCalls=0;resetActions();srand(713+pi);
  reference::BotMoveToGoal(&expected,handle,si==6?nullptr:&otherGoal,flags);
  const auto expectedState=stateRecord(reference::state);
  actual.flags=(actual.flags&255)|((actual.flags&MOVERESULT_FUTUREVIEW)?256:0);
  if(actual.type==RESULTTYPE_INVISIBLEGRAPPLE)actual.type=2;
  bool decision=memcmp(&actual,&expected,24)||actualInput.actionflags!=botinputs[0].actionflags
   ||actualInput.weapon!=botinputs[0].weapon||actualCommands!=commands;
  for(int i=0;i<7;++i)decision|=actualState[i]!=expectedState[i];
  decision|=actualSelections!=selectionCalls||(selectionCalls&&(actualRouteFlags!=observedRouteFlags||actualMoveFlags!=observedMoveFlags));
  bool different=decision||actualState!=expectedState||memcmp(&actualInput,&botinputs[0],sizeof(actualInput))
   ||actualTraces!=bspCalls||actualRoutes!=routeCalls||actualMoves.size()!=moveActions.size();
  if(actualMoves.size()==moveActions.size()&&!actualMoves.empty())
   different|=memcmp(actualMoves.data(),moveActions.data(),actualMoves.size()*sizeof(MoveAction))!=0;
  for(unsigned wi=6;wi<13;++wi){unsigned a,b;memcpy(&a,reinterpret_cast<char*>(&actual)+4*wi,4);
   memcpy(&b,reinterpret_cast<char*>(&expected)+4*wi,4);if(b==0x7fc00123u){++undefinedWords;continue;}
   ++definedWords;different|=a!=b;}
  ++queries;differences+=different;decisions+=decision;perTravel[ti]+=different;
  if(different&&differences<=8)fprintf(stderr,"travel=%d pos=%d state=%d goal=%d obstacle=%d variant=%d decision=%d routes=%d/%d selections=%d/%d flags=%d/%d travel=%d/%d speed=%g/%g\n",
   ti,pi,si,gi,obstacle,variant,int(decision),actualRoutes,routeCalls,actualSelections,selectionCalls,
   actual.flags,expected.flags,actual.traveltype,expected.traveltype,actualInput.speed,botinputs[0].speed);
 }
 goalFinish();
 printf("{\"queries\":%u,\"recordDifferences\":%u,\"decisionDifferences\":%u,\"definedResultWords\":%u,\"undefinedResultWordsExcluded\":%u,\"perTravelDifferences\":[",
  queries,differences,decisions,definedWords,undefinedWords);
 for(size_t i=0;i<perTravel.size();++i)printf("%s%u",i?",":"",perTravel[i]);puts("]}");
}
'''


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base):
    source=goal_source()
    stub=function(source,'bool AAS_RTCWMovementFuturePosition(')
    route=(ROOT/'code/botlib/be_aas_route.cpp').read_text()
    source=source.replace(stub,r'''
static int routeCosts[6],routeLinks[6],routeCalls;
int AAS_AreaRouteToGoalArea(int area,vec3_t,int,int,int *cost,int *reach){
 assert(area>0&&area<6);++routeCalls;*cost=routeCosts[area];*reach=routeLinks[area];return *cost!=0;
}
'''+function(route,'bool AAS_RTCWMovementFuturePosition('))
    source+=SCENE
    for name in [*travel.DEPENDENCIES,*TRAVEL_NAMES,'BotReachabilityTime','BotMoveInGoalArea','BotOnTopOfEntity','BotMoveToGoal']:
        source=re.sub(r'\b'+name+r'\b','Engine_'+name,source)
    original,provenance=travel.fixture(base)
    reference=original[original.index('\nnamespace reference {'):original.index('\n}\n#undef bot_moveresult_t_cleared')]
    move=base/'botlib/be_ai_move.c';text=move.read_text()
    goal=base/('botlib/be_ai_goal.h' if (base/'botlib/be_ai_goal.h').is_file() else 'game/be_ai_goal.h')
    header=base/('botlib/be_ai_move.h' if (base/'botlib/be_ai_move.h').is_file() else 'game/be_ai_move.h')
    goaltext=goal.read_text();start=goaltext.index('typedef struct bot_goal_s');end='} bot_goal_t;'
    extra=goaltext.split('*/',1)[0]+'*/\n'+goaltext[start:goaltext.index(end,start)+len(end)]+'\n'
    for path,name in [(goal,'GFL_NOSLOWAPPROACH'),(header,'MOVERESULT_FUTUREVIEW')]:
        value=re.search(r'^#define\s+'+name+r'\s+(\w+)',path.read_text(),re.M)[1]
        extra+=f'static constexpr int {name}={int(value,0)};\n'
    reference=reference.replace('namespace reference {','namespace reference {\n'+extra,1)
    reference=reference.replace('static float sv_maxstep,sv_maxbarrier,sv_gravity;',
        'static bot_moveresult_t *callerResult;\nstatic float sv_maxstep=18,sv_maxbarrier=32,sv_gravity=800;')
    reference=reference.replace('memcpy(result,seed,52);SourceClearMoveResult(result);',
        'if(result!=callerResult)memcpy(result,seed,52);SourceClearMoveResult(result);')
    math=base/('qcommon/q_math.c' if (base/'qcommon/q_math.c').is_file() else 'game/q_math.c')
    reference+=active_function(math.read_text(),'VectorDistance')+'\n'
    reference+='static int bot_developer;\nstatic bot_movestate_t state;\nbot_movestate_t *BotMoveStateFromHandle(int n){return n==1?&state:nullptr;}\n'
    split='movetravelflags' in function(text,'int BotGetReachabilityToGoal(')
    reference+='int BotGetReachabilityToGoal(vec3_t p,int a,int,int lg,int la,int*,float*,int*,bot_goal_t *g,int rf'
    reference+=',int mf' if split else ''
    reference+='){return ::AAS_RTCWMovementReachability(p,a,lg,la,g->areanum,rf,'+('mf' if split else 'rf')+');}\n'
    # The original ladder body uses the source pointer spelling for the world.
    movement=base/'botlib/be_aas_move.c';movementtext=movement.read_text()
    reference+=movementtext.split('*/',1)[0]+'*/\n#pragma push_macro("aasworld")\n#undef aasworld\n#define aasworld aas_currentWorld\n'
    reference+=active_function(movementtext,'AAS_AgainstLadder')+'\n#pragma pop_macro("aasworld")\n'
    for name in ['BotOnTopOfEntity','BotReachabilityTime','BotMoveInGoalArea','BotMoveToGoal']:
        reference+=active_function(text,name)+'\n'
    source+='\n#pragma push_macro("AVOIDREACH")\n#undef AVOIDREACH\n'+reference
    source+='\n}\n#undef bot_moveresult_t_cleared\n#pragma pop_macro("AVOIDREACH")\n'+COMPARE
    for path in [goal,movement]:provenance.append(dict(path=str(path),sha256=sha(path)))
    return source,provenance


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source',type=Path,default=Path('E:/_SOURCE/_CODE/RTCW-SP-master'))
    parser.add_argument('--mp-source',type=Path,default=Path('E:/_SOURCE/_CODE/RTCW-MP-master'))
    parser.add_argument('--iortcw-source',type=Path,default=Path('E:/_SOURCE/_CODE/iortcw-master'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--compiler',default=shutil.which('clang++'))
    parser.add_argument('--arch',choices=['x86','x64'],default='x86')
    parser.add_argument('--sanitize',action='store_true');parser.add_argument('--fast-math',action='store_true')
    args=parser.parse_args();output=args.output.resolve();output.relative_to(ROOT/'.tmp');output.parent.mkdir(parents=True,exist_ok=True)
    target=['-m32'] if args.arch=='x86' else []
    flags=['-ffast-math'] if args.fast_math else ['-fno-fast-math','-ffp-contract=off']
    if os.name=='nt':target=['--target=i686-pc-windows-msvc'] if args.arch=='x86' else ['-DFNTECH3_Q3_ONLY']
    elif args.arch=='x86':flags+=['-msse2','-mfpmath=sse']
    if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    files=['code/botlib/be_ai_move.cpp','code/botlib/be_aas_move.cpp','code/botlib/be_aas_route.cpp',
        'code/server/server_rtcw_move_goal.hpp','code/compat/rtcw_game_abi.h',
        'tests/rtcw_move_goal_tests.py','scripts/probe_rtcw_move_goal.py','scripts/probe_rtcw_travel.py']
    report=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),architecture=args.arch,
        sanitized=args.sanitize,fastMath=args.fast_math,
        scope='Shared engine geometry/prediction/actions, controlled route/BSP services; undefined reference-local result words excluded; caller-owned bytes compared; no live AI claim',
        implementationSha256={n:sha(ROOT/n) for n in files},results=[])
    env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                         ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
        source,provenance=fixture(base);row=dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode()).hexdigest())
        report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='move-goal-reference-',dir=output.parent) as temp:
            cpp=Path(temp)/'probe.cpp';exe=Path(temp)/'probe.exe';cpp.write_text(source,encoding='utf-8')
            command=[args.compiler,*target,*flags,'-std=c++17','-O2','-D_CRT_SECURE_NO_WARNINGS','-I',str(ROOT),
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
            row['passed']=not built.returncode and row.get('exitCode')==0 and row.get('queries')==18432
            row['exactAgreement']=row['passed'] and row.get('recordDifferences')==0
        output.write_text(json.dumps(report,indent=2)+'\n')
        print(variant,'PASS' if row['passed'] else 'FAIL',row.get('queries'),row.get('recordDifferences'),flush=True)
    return 0 if all(r['passed'] and (r['exactAgreement'] or args.fast_math or r['reference'].startswith('iortcw')) for r in report['results']) else 1


if __name__=='__main__':raise SystemExit(main())
