#!/usr/bin/env python3
"""Compare jump run-up and horizontal velocity against disposable RTCW sources.

Reference GPLv3 bodies/notices stay in temporary programs. Run-up comparisons
share the engine's checked prediction, geometry and callbacks. Failed shared
predictions and nonfinite reference velocity results are excluded explicitly.
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
from rtcw_jump_runstart_tests import jump_source
from rtcw_travel_tests import SCENE, active_function
from rtcw_server_world_tests import function


COMPARE=r'''
int main(){profile=&profiles[MODE_SP?2:3];unsigned queries=0,runQueries=0,velocityQueries=0;
 unsigned runDifferences=0,velocityDifferences=0,decisions=0,failedPredictions=0,undefinedVelocity=0;
 unsigned slimeStops=0,waterStops=0,lavaStops=0,gapStops=0;
 const float directions[][2]={{3,4},{-3,4},{3,-4},{-3,-4},{0,8},{8,0},{1,1},{0,0}};
 for(int geometry:{0,1}){travelSetup();if(geometry)gapGeometry();
  for(float x:{-40.f,-1.f,0.f,1.f,5.f,19.f,20.f,21.f,31.f,60.f})
  for(float z:{-12.f,0.f,1.f,17.f,80.f})for(float y:{-10.f,0.f,9.f})
  for(auto &direction:directions)for(int liquid:{0,CONTENTS_WATER,CONTENTS_SLIME,CONTENTS_LAVA}){
   contents=liquid;auto reach=link(x,y,z,x+direction[0],y+direction[1],z+50);
   vec3_t actual{-71,-73,-79},expected{-71,-73,-79},before{-71,-73,-79};
   predictionCalls=0;int actualStatus=AAS_RTCWJumpReachRunStart(reach.start,reach.end,actual);
   assert(predictionCalls==1);const auto actualPrediction=lastPrediction;int actualPredicted=lastPredicted;
   predictionCalls=0;reference::AAS_JumpReachRunStart(&reach,expected);assert(predictionCalls==1);
   ++queries;++runQueries;
   if(!actualPredicted||!lastPredicted){++failedPredictions;
    if(!actualStatus)assert(!memcmp(actual,before,12));continue;}
   const bool decision=actualPrediction.stopevent!=lastPrediction.stopevent||!actualStatus;
   const bool different=decision||memcmp(actual,expected,12);runDifferences+=different;decisions+=decision;
   slimeStops+=bool(actualPrediction.stopevent&SE_ENTERSLIME);waterStops+=bool(actualPrediction.stopevent&SE_ENTERWATER);
   lavaStops+=bool(actualPrediction.stopevent&SE_ENTERLAVA);gapStops+=bool(actualPrediction.stopevent&SE_GAP);
   if(different&&runDifferences<=5)fprintf(stderr,"run geometry=%d origin=%g,%g,%g direction=%g,%g liquid=%d event=%d/%d\n",
    geometry,x,y,z,direction[0],direction[1],liquid,actualPrediction.stopevent,lastPrediction.stopevent);
  }travelFinish();
 }
 for(int settings:{0,1}){
  aassettings.phys_gravity=reference::aassettings.sv_gravity=settings?400:800;
  aassettings.phys_maxvelocity=reference::aassettings.sv_maxvelocity=settings?200:320;
  for(float vertical:{-300.f,-1.f,0.f,100.f,270.f,1000.f})for(float height:{-1000.f,-10.f,0.f,1.f,45.f,46.f,1000.f})
  for(float distance:{0.f,1.f,5.f,100.f,1000.f})for(float side:{0.f,17.f}){
   vec3_t from{10,20,30},to{10+distance,20+side,30+height};float actual=-71,expected=-73;
   int a=AAS_HorizontalVelocityForJump(vertical,from,to,&actual);
   int b=reference::AAS_HorizontalVelocityForJump(vertical,from,to,&expected);++queries;++velocityQueries;
   if(!fntech3::rtcw::routing::Finite(expected)){++undefinedVelocity;continue;}
   const bool different=a!=b||memcmp(&actual,&expected,4);velocityDifferences+=different;decisions+=a!=b;
   if(different&&velocityDifferences<=5)fprintf(stderr,"velocity gravity=%g z=%g height=%g distance=%g,%g result=%d/%d speed=%g/%g\n",
    aassettings.phys_gravity,vertical,height,distance,side,a,b,actual,expected);
  }
 }
 printf("{\"queries\":%u,\"runQueries\":%u,\"velocityQueries\":%u,\"recordDifferences\":%u,\"decisionDifferences\":%u,"
  "\"runDifferences\":%u,\"velocityDifferences\":%u,\"failedPredictionsExcluded\":%u,\"nonfiniteReferenceVelocitiesExcluded\":%u,"
  "\"slimeStops\":%u,\"waterStops\":%u,\"lavaStops\":%u,\"gapStops\":%u}\n",
  queries,runQueries,velocityQueries,runDifferences+velocityDifferences,decisions,runDifferences,velocityDifferences,
  failedPredictions,undefinedVelocity,slimeStops,waterStops,lavaStops,gapStops);
}
'''


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base):
    source=jump_source();actual=function(source,'int AAS_PredictClientMovement(')
    source=source.replace(actual,actual.replace('AAS_PredictClientMovement(','Real_PredictClientMovement(',1)+r'''
static int predictionCalls,lastPredicted;static aas_clientmove_t lastPrediction;
int AAS_PredictClientMovement(aas_clientmove_t *out,int entity,const vec3_t origin,int presence,int ground,
 const vec3_t velocity,const vec3_t command,int commandFrames,int frames,float duration,int events,int area,int visualize){
 ++predictionCalls;lastPredicted=Real_PredictClientMovement(out,entity,origin,presence,ground,velocity,command,
  commandFrames,frames,duration,events,area,visualize);lastPrediction=*out;return lastPredicted;
}
''')
    source+=SCENE+'\nnamespace reference {\n'
    math=base/('qcommon/q_math.c' if (base/'qcommon/q_math.c').is_file() else 'game/q_math.c')
    move=base/'botlib/be_aas_move.c'
    text=math.read_text();source+=text.split('*/',1)[0]+'*/\n'
    signature=re.search(r'^(?:vec_t|float)\s+VectorNormalize\s*\(',text,re.M)[0]
    source+=function(text,signature)+'\n'
    source+='static struct {float sv_gravity,sv_maxvelocity;} aassettings;\n'
    text=move.read_text();source+=text.split('*/',1)[0]+'*/\n'
    source+=active_function(text,'AAS_JumpReachRunStart')+'\n'
    source+=active_function(text,'AAS_HorizontalVelocityForJump')+'\n}\n'+COMPARE
    return source,[dict(path=str(p),sha256=sha(p)) for p in [math,move]]


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
    files=['code/botlib/be_ai_move.cpp','code/botlib/be_aas_move.cpp','code/botlib/rtcw_navigation.h',
        'tests/rtcw_jump_runstart_tests.py','tests/rtcw_travel_tests.py','scripts/probe_rtcw_jump_runstart.py']
    report=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),architecture=args.arch,sanitized=args.sanitize,
        fastMath=args.fast_math,scope='Run-up policy with shared checked engine prediction; horizontal velocity defined-output cases; failed prediction/nonfinite reference outputs excluded; no live AI claim',
        implementationSha256={n:sha(ROOT/n) for n in files},results=[])
    env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                         ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
        source,provenance=fixture(base);row=dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode()).hexdigest())
        report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='jump-runstart-reference-',dir=output.parent) as temp:
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
            row['passed']=not built.returncode and row.get('exitCode')==0 and row.get('queries')==10440
            row['exactAgreement']=row['passed'] and row.get('recordDifferences')==0
        output.write_text(json.dumps(report,indent=2)+'\n')
        print(variant,'PASS' if row['passed'] else 'FAIL',row.get('queries'),row.get('recordDifferences'),flush=True)
    return 0 if all(r['passed'] and (r['exactAgreement'] or args.fast_math or r['reference'].startswith('iortcw')) for r in report['results']) else 1


if __name__=='__main__':raise SystemExit(main())
