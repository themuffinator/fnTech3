#!/usr/bin/env python3
"""Compare travel execution against disposable original/iortcw source fixtures.

GPLv3 reference bodies/notices remain confined to temporary programs. Both
sides use engine AAS geometry, prediction, entities and elementary actions,
with controlled BSP trace/model callbacks. Reference result clearing is wrapped
to seed otherwise undefined fields; only fields actually written are compared.
This is travel-policy evidence, not original collision or live actor evidence.
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
from rtcw_travel_tests import travel_source, SCENE, TRAVEL_NAMES, active_function
from rtcw_server_world_tests import function

DEPENDENCIES=['AngleDiff','BotOnMover','MoverDown','MoverBottomCenter','BotCheckBlocked','BotAirControl',
    'BotFuncBobStartEnd','GrappleState','BotResetGrapple','BotGapDistance','BotCheckBarrierJump']

COMPARE=r'''
static unsigned bits(float value){unsigned n;memcpy(&n,&value,4);return n;}
template<class S>static auto stateRecord(const S &s){
 return std::array<unsigned,5>{unsigned(s.moveflags),unsigned(s.jumpreach),bits(s.grapplevisible_time),
  bits(s.lastgrappledist),bits(s.reachability_time)};
}
static auto integerResult(const bot_moveresult_t &r){
 return std::array<int,6>{r.failure,r.type,r.blocked,r.blockentity,r.traveltype,r.flags};
}
int main(){
 static_assert(sizeof(bot_moveresult_t)==52&&sizeof(reference::bot_moveresult_t)==52,"result word comparison");
 profile=&profiles[MODE_SP?2:3];auto *ms=travelSetup();
 unsigned queries=0,differences=0,decisions=0,undefinedWords=0,definedWords=0;
 std::array<unsigned,22> perTravel{};
 const std::array<std::array<float,3>,12> positions{{{0,0,1},{2,3,1},{20,10,24},{80,-60,60},
  {-80,10,65.75f},{0,0,500},{5,0,100.5f},{0,0,-10},{1,1,1},{0,0,249},{0,-3,0},{0,0,32.75f}}};
 for(int settings:{0,1}){
  LibVarSet("sv_step",settings?"20":"18");LibVarSet("sv_maxbarrier",settings?"32.5":"32");
  LibVarSet("sv_gravity",settings?"400":"800");BotSetupMoveAI();
  reference::sv_maxstep=settings?20:18;reference::sv_maxbarrier=settings?32.5f:32;reference::sv_gravity=settings?400:800;
  LibVarSet("sv_step","123");LibVarSet("sv_maxbarrier","456");LibVarSet("sv_gravity","0");
  for(size_t pi=0;pi<positions.size();++pi)for(int ri=0;ri<8;++ri)for(int fi=0;fi<10;++fi)for(int obstacle=0;obstacle<3;++obstacle){
   auto r=link(ri&1?5:0,ri&2?7:0,1,ri&1?10:100,ri&2?40:0,ri&4?100:0);
   r.edgenum=100;r.traveltype=TRAVEL_GRAPPLEHOOK;r.areanum=1;
   aasworld.reachability[1]=r;obstruction=obstacle==0?ENTITYNUM_WORLD:obstacle==1?7:ENTITYNUM_NONE;
   traceSolid=(ri==7);contents=(ri==6?CONTENTS_WATER:ri==7?CONTENTS_SLIME:0);
   aasworld.areasettings[1].presencetype=ri==3?PRESENCE_CROUCH:ri==4?0:PRESENCE_NORMAL|PRESENCE_CROUCH;
   aasworld.areasettings[1].numreachableareas=ri&1;
   AAS_EntityWorld().entities[1].i.origin[2]=ri&1?50:0;
   auto &hook=AAS_EntityWorld().entities[2].i;hook={};hook.valid=qtrue;hook.modelindex=0;
   VectorCopy(r.end,hook.origin);VectorCopy(r.end,hook.lastvisorigin);
   if(ri&1)hook.origin[0]+=1;if(ri&2)hook.modelindex=9;
   LibVarSet("laserhook",ri==5?"1":"0");
   for(unsigned ti=0;ti<22;++ti){
    bot_movestate_t initial{};VectorCopy(positions[pi].data(),initial.origin);
    initial.presencetype=PRESENCE_NORMAL;initial.thinktime=.1f;initial.areanum=initial.reachareanum=1;
    initial.lastreachnum=1;initial.jumpreach=fi&1;initial.velocity[2]=fi&1?249:250;
    initial.velocity[0]=settings?200:0;initial.viewoffset[2]=24;initial.viewangles[0]=fi&1?-90:90;
    const int flags[]={0,MFL_ONGROUND,MFL_SWIMMING,MFL_AGAINSTLADDER,MFL_WALK,MFL_WATERJUMP,
      MFL_GRAPPLEPULL,MFL_ACTIVEGRAPPLE,MFL_ACTIVEGRAPPLE|MFL_GRAPPLERESET,MFL_ACTIVEGRAPPLE|MFL_SWIMMING};
    initial.moveflags=flags[fi];initial.lastgrappledist=fi&1?1000:0;
    initial.grapplevisible_time=fi&1?5:0;initial.reachability_time=7;
    *ms=initial;reference::bot_movestate_t other{};
    VectorCopy(initial.origin,other.origin);VectorCopy(initial.velocity,other.velocity);
    VectorCopy(initial.viewoffset,other.viewoffset);VectorCopy(initial.viewangles,other.viewangles);
    other.presencetype=initial.presencetype;other.thinktime=initial.thinktime;other.areanum=initial.areanum;
    other.reachareanum=initial.reachareanum;other.lastreachnum=initial.lastreachnum;other.jumpreach=initial.jumpreach;
    other.moveflags=initial.moveflags;other.lastgrappledist=initial.lastgrappledist;
    other.grapplevisible_time=initial.grapplevisible_time;other.reachability_time=initial.reachability_time;
    resetActions();srand(0x1900+unsigned(pi));const auto actual=engineTravel[ti](ms,&r);
    const auto actualInput=botinputs[0];const auto actualMoves=moveActions;const auto actualCommands=commands;
    const auto actualState=stateRecord(*ms);const int actualTraces=bspCalls;
    resetActions();srand(0x1900+unsigned(pi));const auto expected=referenceTravel[ti](&other,&r);
    const auto expectedState=stateRecord(other);const int *expectedInts=&expected.failure;
    const auto ints=integerResult(actual);bool decision=memcmp(ints.data(),expectedInts,24)!=0
     ||actualInput.actionflags!=botinputs[0].actionflags||actualInput.weapon!=botinputs[0].weapon
     ||actualState[0]!=expectedState[0]||actualState[1]!=expectedState[1]||actualCommands!=commands;
    bool different=decision||memcmp(&actualInput,&botinputs[0],sizeof(actualInput))
     ||actualState!=expectedState||actualTraces!=bspCalls||actualMoves.size()!=moveActions.size();
    if(actualMoves.size()==moveActions.size()&&!actualMoves.empty())
     different|=memcmp(actualMoves.data(),moveActions.data(),actualMoves.size()*sizeof(MoveAction))!=0;
    for(unsigned wi=6;wi<13;++wi){
     unsigned a,b;memcpy(&a,reinterpret_cast<const char*>(&actual)+4*wi,4);
     memcpy(&b,reinterpret_cast<const char*>(&expected)+4*wi,4);
     if(b==0x7fc00123u){++undefinedWords;continue;}
     ++definedWords;different|=a!=b;
    }
    ++queries;differences+=different;decisions+=decision;perTravel[ti]+=different;
    if(different&&differences<=5)fprintf(stderr,"travel=%u settings=%d pos=%zu reach=%d flags=%d obstacle=%d decision=%d traces=%d/%d speed=%g/%g\n",
     ti,settings,pi,ri,fi,obstacle,int(decision),actualTraces,bspCalls,actualInput.speed,botinputs[0].speed);
   }
  }
 }
 travelFinish();
 printf("{\"queries\":%u,\"recordDifferences\":%u,\"decisionDifferences\":%u,\"definedResultWords\":%u,\"undefinedResultWordsExcluded\":%u,\"perTravelDifferences\":[",
  queries,differences,decisions,definedWords,undefinedWords);
 for(unsigned i=0;i<perTravel.size();++i)printf("%s%u",i?",":"",perTravel[i]);puts("]}");
}
'''


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base):
    source=travel_source()+SCENE
    for name in [*DEPENDENCIES,*TRAVEL_NAMES]:
        source=re.sub(r'\b'+name+r'\b','Engine_'+name,source)
    source+='\nnamespace reference {\n'
    math=base/('qcommon/q_math.c' if (base/'qcommon/q_math.c').is_file() else 'game/q_math.c')
    text=math.read_text();source+=text.split('*/',1)[0]+'*/\n'
    for name in ['VectorNormalize','VectorNormalize2','vectoangles','AngleVectors','ProjectPointOntoVector']:
        signature=re.search(r'^(?:vec_t|float|void)\s+'+name+r'\s*\(',text,re.M)
        if signature:source+=function(text,signature[0])+'\n'
    source+='\nvoid Vector2Angles(const vec3_t v,vec3_t a){vectoangles(v,a);}\n'
    move=base/'botlib/be_ai_move.c';text=move.read_text();source+=text.split('*/',1)[0]+'*/\n'
    start=text.index('typedef struct bot_movestate_s')
    source+=text[start:text.index('} bot_movestate_t;',start)+len('} bot_movestate_t;')]+'\n'
    header=base/('botlib/be_ai_move.h' if (base/'botlib/be_ai_move.h').is_file() else 'game/be_ai_move.h')
    result=header.read_text();source+=result.split('*/',1)[0]+'*/\n'
    start=result.index('typedef struct bot_moveresult_s')
    source+=result[start:result.index('} bot_moveresult_t;',start)+len('} bot_moveresult_t;')]+'\n'
    source+='static float sv_maxstep,sv_maxbarrier,sv_gravity;\nstatic int abs(int n){return n<0?-n:n;}\n'
    source+='static constexpr const char *CMD_HOOKOFF="hookoff",*CMD_HOOKON="hookon";\n'
    source+='int AAS_OriginOfEntityWithModelNum(int model,vec3_t origin){return AAS_OriginOfMoverWithModelNum(model,origin);}\n'
    main=base/'botlib/be_aas_main.c';maintext=main.read_text();source+=maintext.split('*/',1)[0]+'*/\n'
    source+=active_function(maintext,'AAS_IndexFromModel')+'\n'
    if 'bot_moveresult_t_cleared' in text:
        source+=re.search(r'^#define bot_moveresult_t_cleared.*$',result,re.M)[0]+'\n'
    else:
        source+=active_function(text,'BotClearMoveResult').replace('BotClearMoveResult','SourceClearMoveResult',1)+'\n'
        source+='''
void BotClearMoveResult(bot_moveresult_t *result){
 unsigned seed[13];std::fill_n(seed,13,0x7fc00123u);memcpy(result,seed,52);SourceClearMoveResult(result);
}
'''
    # Dependents are ordered independently of their order in the source files.
    for name in DEPENDENCIES:
        if name=='BotCheckBlocked':continue
        if name=='BotResetGrapple':continue
        source+=active_function(text,name)+'\n'
    source+=active_function(text,'BotCheckBlocked')+'\n'
    source+=active_function(text,'BotResetGrapple')+'\n'
    for name in TRAVEL_NAMES:source+=active_function(text,name)+'\n'
    source+='\n}\n#undef bot_moveresult_t_cleared\nusing EngineTravel=bot_moveresult_t(*)(bot_movestate_t*,aas_reachability_t*);\n'
    source+='static EngineTravel engineTravel[]={'+','.join('Engine_'+n for n in TRAVEL_NAMES)+'};\n'
    source+='using ReferenceTravel=reference::bot_moveresult_t(*)(reference::bot_movestate_t*,aas_reachability_t*);\n'
    source+='static ReferenceTravel referenceTravel[]={'+','.join('reference::'+n for n in TRAVEL_NAMES)+'};\n'
    source+=COMPARE
    return source,[dict(path=str(p),sha256=sha(p)) for p in [math,move,header,main]]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source',type=Path,default=Path('E:/_SOURCE/_CODE/RTCW-SP-master'))
    parser.add_argument('--mp-source',type=Path,default=Path('E:/_SOURCE/_CODE/RTCW-MP-master'))
    parser.add_argument('--iortcw-source',type=Path,default=Path('E:/_SOURCE/_CODE/iortcw-master'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--compiler',default=shutil.which('clang++'))
    parser.add_argument('--arch',choices=['x86','x64'],default='x86')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--fast-math',action='store_true')
    args=parser.parse_args();output=args.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
    target=['-m32'] if args.arch=='x86' else []
    flags=['-ffast-math'] if args.fast_math else ['-fno-fast-math','-ffp-contract=off']
    if os.name=='nt':target=['--target=i686-pc-windows-msvc'] if args.arch=='x86' else ['-DFNTECH3_Q3_ONLY']
    elif args.arch=='x86':flags+=['-msse2','-mfpmath=sse']
    if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    files=['code/botlib/be_ai_move.cpp','code/botlib/be_aas_move.cpp','code/botlib/be_ea.cpp',
        'code/botlib/be_aas_entity.cpp','code/botlib/rtcw_movement.hpp','tests/rtcw_travel_tests.py',
        'tests/rtcw_direction_tests.py','scripts/probe_rtcw_travel.py']
    report=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),architecture=args.arch,
        sanitized=args.sanitize,fastMath=args.fast_math,travelNames=TRAVEL_NAMES,
        scope='Shared engine AAS/prediction/entity/actions and controlled BSP; seeded undefined reference result fields excluded; no original collision or live movement claim',
        implementationSha256={n:sha(ROOT/n) for n in files},results=[])
    env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                         ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
        source,provenance=fixture(base);row=dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode()).hexdigest())
        report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='travel-reference-',dir=output.parent) as temp:
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
            row['passed']=not built.returncode and row.get('exitCode')==0 and row.get('queries')==126720
            row['exactAgreement']=row['passed'] and row.get('recordDifferences')==0
        output.write_text(json.dumps(report,indent=2)+'\n')
        print(variant,'PASS' if row['passed'] else 'FAIL',row.get('queries'),row.get('recordDifferences'),flush=True)
    return 0 if all(r['passed'] and (r['exactAgreement'] or args.fast_math or r['reference'].startswith('iortcw')) for r in report['results']) else 1


if __name__=='__main__':raise SystemExit(main())
