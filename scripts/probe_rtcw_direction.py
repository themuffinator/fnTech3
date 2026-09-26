#!/usr/bin/env python3
"""Compare RTCW directional movement policies against disposable source fixtures.

Reference GPLv3 movement and normalization bodies retain their notices and run
only in temporary executables. Both implementations use fnTech3's AAS traces,
prediction and elementary actions. This isolates direction policies; it does
not establish original collision/prediction or live cast-AI parity. Failed
predictions are reported separately because the engine refuses to act on them.
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

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tests'))
from rtcw_direction_tests import direction_source, GEOMETRY
from rtcw_server_world_tests import function

PREDICTION = r'''
static bool failedPrediction;
static std::vector<unsigned> predictionArguments;
int AAS_PredictClientMovement(aas_clientmove_t *out,int entity,const vec3_t origin,int presence,int ground,
 const vec3_t velocity,const vec3_t command,int commandFrames,int maximumFrames,float duration,int events,int area,int visualize){
 for(int value:{entity,presence,ground,commandFrames,maximumFrames,events,area,visualize})
  predictionArguments.push_back(unsigned(value));
 for(float value:{duration,origin[0],origin[1],origin[2],velocity[0],velocity[1],velocity[2],command[0],command[1],command[2]}){
  unsigned bits;memcpy(&bits,&value,4);predictionArguments.push_back(bits);
 }
 int result=Real_AAS_PredictClientMovement(out,entity,origin,presence,ground,velocity,command,
   commandFrames,maximumFrames,duration,events,area,visualize);
 failedPrediction|=!result;return result;
}
'''

COMPARE = r'''
int main(){
 profile=&profiles[MODE_SP?2:3];
 unsigned queries=0,compared=0,failedPredictions=0,differences=0,decisions=0,failedDifferences=0,barriers=0;
 for(int settings:{0,1})for(float level:{-64.0f,0.0f,16.0f,24.0f,40.0f}){
  const int state=prepare();gapGeometry();aasworld.planes[4].dist=level;aasworld.planes[5].dist=-level;
  reference::sv_maxstep=settings?19:18;reference::sv_maxbarrier=settings?48:32;
  LibVarSet("sv_step",settings?"19":"18");LibVarSet("sv_maxbarrier",settings?"48":"32");BotSetupMoveAI();
  // Changing live libvars after setup must not change the RTCW snapshots.
  LibVarSet("sv_step","123");LibVarSet("sv_maxbarrier","456");
  for(int liquid:{0,CONTENTS_WATER,CONTENTS_SLIME,CONTENTS_LAVA})for(int flags:{0,1,2,3})
  for(float vz:{0.0f,50.0f})for(float z:{1.0f,100.0f})
  for(const auto &d:std::array<std::array<float,3>,5>{{{0,0,0},{1,0,0},{-1,0,0},{3,4,12},{100,-200,75}}})
  for(float speed:{-200.0f,0.0f,200.0f,600.0f})for(int type:{0,MOVE_JUMP,MOVE_CROUCH,MOVE_JUMP|MOVE_CROUCH}){
   ++queries;contents=liquid;bot_movestate_t initial{};initial.origin[2]=z;initial.velocity[2]=vz;
   initial.thinktime=.1f;initial.presencetype=PRESENCE_NORMAL;initial.moveflags=flags;
   *botmovestates[state]=initial;reference::state={};
   VectorCopy(initial.origin,reference::state.origin);VectorCopy(initial.velocity,reference::state.velocity);
   reference::state.thinktime=initial.thinktime;reference::state.presencetype=initial.presencetype;
   reference::state.moveflags=flags;
   vec3_t direction{d[0],d[1],d[2]};
   bot_input_t seed{};seed.thinktime=.05f;seed.actionflags=ACTION_ATTACK;seed.weapon=7;
   botinputs[0]=seed;failedPrediction=false;predictionArguments.clear();
   const int actual=BotMoveInDirection(state,direction,speed,type);
   const auto actualState=*botmovestates[state];const auto action=botinputs[0];
   const bool actualFailed=failedPrediction;const auto arguments=predictionArguments;
   assert(!memcmp(direction,d.data(),12));
   botinputs[0]=seed;failedPrediction=false;predictionArguments.clear();
   const int expected=reference::BotMoveInDirection(state,direction,speed,type);
   assert(!memcmp(direction,d.data(),12));
   const bool different=actual!=expected||actualState.moveflags!=reference::state.moveflags
     ||memcmp(&action,&botinputs[0],sizeof(action))||arguments!=predictionArguments;
   if(actualFailed||failedPrediction){++failedPredictions;failedDifferences+=different;continue;}
   ++compared;differences+=different;decisions+=actual!=expected;
   barriers+=bool((actualState.moveflags&MFL_BARRIERJUMP)&&(action.actionflags&ACTION_JUMP));
   if(different&&differences<=3)fprintf(stderr,
    "settings=%d level=%g liquid=%d flags=%d vz=%g z=%g dir=(%g,%g,%g) speed=%g type=%d actual=%d expected=%d\n",
    settings,level,liquid,flags,vz,z,d[0],d[1],d[2],speed,type,actual,expected);
   initial.moveflags=actualState.moveflags;assert(!memcmp(&initial,&actualState,sizeof(initial)));
  }
  finish();
 }
 printf("{\"queries\":%u,\"compared\":%u,\"failedPredictions\":%u,\"recordDifferences\":%u,"
  "\"decisionDifferences\":%u,\"failedPredictionDifferences\":%u,\"barrierActions\":%u}\n",
  queries,compared,failedPredictions,differences,decisions,failedDifferences,barriers);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base):
    source = direction_source().replace('int AAS_PredictClientMovement(', 'int Real_AAS_PredictClientMovement(', 1)
    source += GEOMETRY+PREDICTION+'\nnamespace reference {\n'
    math = base/('qcommon/q_math.c' if (base/'qcommon/q_math.c').is_file() else 'game/q_math.c')
    move = base/'botlib/be_ai_move.c'
    text = math.read_text()
    source += text.split('*/', 1)[0]+'*/\n'
    for name in ['VectorNormalize', 'VectorNormalize2']:
        signature = re.search(r'^(?:vec_t|float)\s+'+name+r'\s*\(', text, re.M)[0]
        source += function(text, signature)+'\n'
    text = move.read_text()
    source += text.split('*/', 1)[0]+'*/\n'
    start = text.index('typedef struct bot_movestate_s')
    source += text[start:text.index('} bot_movestate_t;', start)+len('} bot_movestate_t;')]
    source += '\nstatic bot_movestate_t state;\nstatic float sv_maxstep,sv_maxbarrier;\n'
    source += 'bot_movestate_t *BotMoveStateFromHandle(int handle){assert(handle==1);return &state;}\n'
    for name in ['BotGapDistance','BotCheckBarrierJump','BotSwimInDirection','BotWalkInDirection','BotMoveInDirection']:
        signature = re.search(r'^(?:int|float)\s+'+name+r'\s*\(', text, re.M)[0]
        source += function(text, signature)+'\n'
    source += '\n}\n'+COMPARE
    return source, [dict(path=str(p), sha256=sha(p)) for p in [math,move]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['sp-source','mp-source','iortcw-source','report']:
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--compiler',default='clang++')
    parser.add_argument('--target')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--fast-math',action='store_true')
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:parser.error('C++ compiler unavailable')
    output = args.report.resolve();output.relative_to(ROOT/'.tmp');output.parent.mkdir(parents=True,exist_ok=True)
    target = ['--target='+args.target] if args.target else ([] if os.name=='nt' else ['-m32','-msse2','-mfpmath=sse'])
    triple = subprocess.check_output([compiler,*target,'-dumpmachine'],cwd=ROOT,text=True).strip()
    flags = ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    flags += ['-ffast-math'] if args.fast_math else ['-ffp-contract=off']
    if triple.startswith('x86_64'):flags += ['-DFNTECH3_Q3_ONLY']
    files = ['code/botlib/be_ai_move.cpp','code/botlib/be_ea.cpp','code/botlib/rtcw_movement.hpp',
        'code/botlib/be_aas_move.cpp','code/botlib/be_aas_sample.cpp','code/server/server_rtcw_botlib.hpp',
        'tests/rtcw_direction_tests.py','tests/rtcw_prediction_tests.py','scripts/probe_rtcw_direction.py']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope=__doc__,target=triple,
        sanitized=args.sanitize,fastMath=args.fast_math,implementationSha256={name:sha(ROOT/name) for name in files},results=[])
    env = os.environ.copy()
    if args.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                         ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
        source,provenance = fixture(base)
        row = dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode()).hexdigest())
        report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='direction-',dir=output.parent) as temp:
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
            row['passed']=not built.returncode and row.get('exitCode')==0 and row.get('queries')==51200
            row['exactAgreement']=row['passed'] and row.get('recordDifferences')==0
        output.write_text(json.dumps(report,indent=2)+'\n')
        print(variant,'PASS' if row['passed'] else 'FAIL',row.get('compared'),row.get('recordDifferences'),flush=True)
    return 0 if all(row['passed'] and (row['exactAgreement'] or args.fast_math or row['reference'].startswith('iortcw'))
                    for row in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
