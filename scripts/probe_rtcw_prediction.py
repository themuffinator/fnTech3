#!/usr/bin/env python3
"""Compare prediction against disposable original RTCW source fixtures.

Original SP/MP with strict floating point are agreement gates; fast-math stress
runs and iortcw are diagnostic. iortcw changes friction and vector normalization.
Original start-solid traces read an uninitialized direction; their observations
are counted separately and excluded from agreement. GPLv3 bodies stay in temporary
programs with their notices. Synthetic AAS geometry and point contents exercise
world collision; this does not establish dynamic-entity or installed-map parity.
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
from rtcw_prediction_tests import prediction_source, SCENE
from rtcw_server_world_tests import function

PRELUDE = r'''
#undef aasworld
namespace reference {
static aas_t *aasworld;
static void selectWorld(aas_t *world){aasworld=world;}
static bool indeterminatePlane;
static aas_settings_t aassettings;
struct aas_clientmove_s {
 vec3_t endpos,velocity;aas_trace_t trace;
 int presencetype,stopevent;float endcontents,time;int frames;
};
using aas_clientmove_t=aas_clientmove_s;
static_assert(sizeof(aas_clientmove_t)==sizeof(fntech3_rtcw_aas_clientmove_t));
'''

COMPARE = r'''
#define aasworld (*aas_currentWorld)
static void shape(int geometry){
 FreeMemory(aasworld.nodes);FreeMemory(aasworld.planes);
 aasworld.numnodes=4;aasworld.numplanes=6;
 aasworld.nodes=botlib::ClearedAlloc<aas_node_t>(4);
 aasworld.planes=botlib::ClearedAlloc<aas_plane_t>(6);
 auto plane=[](int i,float x,float z,float d,int type){
  aasworld.planes[i].normal[0]=x;aasworld.planes[i].normal[2]=z;aasworld.planes[i].dist=d;aasworld.planes[i].type=type;
  aasworld.planes[i+1]=aasworld.planes[i];VectorNegate(aasworld.planes[i].normal,aasworld.planes[i+1].normal);aasworld.planes[i+1].dist=-d;
 };
 plane(0,geometry==3?.6f:0,geometry==3?.8f:1,0,geometry==3?3:2);
 plane(2,1,0,64,0);plane(4,0,1,8,2);
 aasworld.nodes[1].planenum=0;aasworld.nodes[1].children[0]=-1;
 if(geometry==1||geometry==2){aasworld.nodes[1].children[0]=2;
  aasworld.nodes[2].planenum=2;aasworld.nodes[2].children[1]=-1;
  aasworld.nodes[2].children[0]=geometry==1?-2:3;
  aasworld.nodes[3].planenum=4;aasworld.nodes[3].children[0]=-2;
 }
 aasworld.areasettings[2].presencetype=geometry==1?PRESENCE_CROUCH:PRESENCE_NORMAL|PRESENCE_CROUCH;
}
int main(){
 profile=&profiles[MODE_SP?2:3];setup();uint64_t queries=0,differences=0,falseResults=0,decisionDifferences=0,excluded=0,rawDifferences=0;
 double maxPositionError=0,maxVelocityError=0,maxFractionError=0;
 unsigned random=0x37142u;auto next=[&](){random=random*1664525u+1013904223u;return random;};
 std::array<uint32_t,20> firstOurs{},firstTheirs{};int firstArgs[9]{};
 for(int world=0;world<2;++world){if(world)scene(world);reference::selectWorld(aas_currentWorld);
  reference::AAS_InitSettings();
  assert(aassettings.phys_maxbarrier==reference::aassettings.phys_maxbarrier);
  assert(aassettings.phys_maxwalkvelocity==reference::aassettings.phys_maxwalkvelocity);
  assert(aassettings.phys_maxstep==reference::aassettings.phys_maxstep);
  reference::AAS_RT_ShowRoute(nullptr,INT_MIN,INT_MAX);
  for(int geometry=0;geometry<4;++geometry){shape(geometry);
   for(int i=0;i<1024;++i){
    static const int liquids[]={0,CONTENTS_WATER,CONTENTS_SLIME,CONTENTS_LAVA};
    contents=liquids[i%4];
    static const int events[]={0,1,2,4,8,16,32,64,128,256,512,1024,2048,4096,-1};
    static const int lengths[]={-1,0,1,2,8,32};static const float durations[]={-.1f,0,.05f,.1f,.2f};
    const int event=events[i%15],frames=lengths[(i/4)%6],commands=i%5;
    const float dt=durations[(i/9)%5];const int presence=(i%7)?PRESENCE_NORMAL:PRESENCE_CROUCH,ground=i%2;
    aasworld.areasettings[1].contents=(i%5==0?AREACONTENTS_JUMPPAD:0)|(i%9==0?AREACONTENTS_TELEPORTER:0);
    aasworld.areasettings[2].contents=(i%6==0?AREACONTENTS_WATER:0)|(i%11==0?AREACONTENTS_SLIME:0);
    vec3_t origin{float(int(next()%140)-10),float(int(next()%16)),float(int(next()%80)-4)};
    vec3_t velocity{float(int(next()%600)-300),float(int(next()%60)-30),float(int(next()%1000)-500)};
    vec3_t command{float(int(next()%640)-320),float(int(next()%60)-30),i%3==0?-400.0f:i%3==1?0.0f:400.0f};
    aas_clientmove_t ours;reference::aas_clientmove_t theirs;
    int a=AAS_PredictClientMovement(&ours,-1,origin,presence,ground,velocity,command,commands,frames,dt,event,1,0);
    reference::indeterminatePlane=false;
    int b=reference::AAS_PredictClientMovement(&theirs,-1,origin,presence,ground,velocity,command,commands,frames,dt,event,1,0);
    fntech3_rtcw_aas_clientmove_t external{};assert(fntech3::rtcw::movement::Result(ours,external));
    ++queries;if(!a)++falseResults;
    const bool same=a==b&&!std::memcmp(&external,&theirs,sizeof(external));
    if(!same)++rawDifferences;
    // Original traces read an uninitialized direction when starting in solid.
    // Those raw observations are diagnostic, never an agreement requirement.
    if(ORIGINAL_REFERENCE&&reference::indeterminatePlane){++excluded;continue;}
    if(a!=b||external.stopevent!=theirs.stopevent||external.frames!=theirs.frames
      ||external.presencetype!=theirs.presencetype||external.endcontents!=theirs.endcontents
      ||external.trace.startsolid!=theirs.trace.startsolid||external.trace.ent!=theirs.trace.ent
      ||external.trace.area!=theirs.trace.area||external.trace.lastarea!=theirs.trace.lastarea
      ||external.trace.planenum!=theirs.trace.planenum)++decisionDifferences;
    for(int axis=0;axis<3;++axis){
     maxPositionError=std::max(maxPositionError,std::fabs(double(external.endpos[axis])-theirs.endpos[axis]));
     maxPositionError=std::max(maxPositionError,std::fabs(double(external.trace.endpos[axis])-theirs.trace.endpos[axis]));
     maxVelocityError=std::max(maxVelocityError,std::fabs(double(external.velocity[axis])-theirs.velocity[axis]));}
    maxFractionError=std::max(maxFractionError,std::fabs(double(external.trace.fraction)-theirs.trace.fraction));
    if(!same){
     if(!differences){std::memcpy(firstOurs.data(),&external,sizeof(external));std::memcpy(firstTheirs.data(),&theirs,sizeof(theirs));
      int args[]={world,geometry,i,event,frames,commands,presence,a,b};std::memcpy(firstArgs,args,sizeof(args));}
     ++differences;
    }
   }
  }
 }
 stop();
 std::printf("{\"queries\":%llu,\"differences\":%llu,\"falseResults\":%llu,\"decisionDifferences\":%llu,\"excludedIndeterminatePlane\":%llu,\"rawDifferences\":%llu,\"maxPositionError\":%.9g,\"maxVelocityError\":%.9g,\"maxFractionError\":%.9g,\"firstArgs\":[",
  (unsigned long long)queries,(unsigned long long)differences,(unsigned long long)falseResults,
  (unsigned long long)decisionDifferences,(unsigned long long)excluded,(unsigned long long)rawDifferences,
  maxPositionError,maxVelocityError,maxFractionError);
 for(int i=0;i<9;++i)std::printf("%s%d",i?",":"",firstArgs[i]);
 std::printf("],\"firstEngineWords\":[");for(int i=0;i<20;++i)std::printf("%s%u",i?",":"",firstOurs[i]);
 std::printf("],\"firstReferenceWords\":[");for(int i=0;i<20;++i)std::printf("%s%u",i?",":"",firstTheirs[i]);std::printf("]}\n");
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base, variant):
    source = prediction_source() + SCENE + PRELUDE
    move_path = base / 'botlib/be_aas_move.c'
    sample_path = base / 'botlib/be_aas_sample.c'
    math_path = base / ('qcommon/q_math.c' if variant.startswith('iortcw') else 'game/q_math.c')
    table_path = base / 'botlib/be_aas_routetable.c'
    # Isolate reference field spellings without changing the reference bodies.
    fields = sorted(set(re.findall(r'aassettings\.(sv_\w+)', move_path.read_text())))
    source += '\n'.join('#define '+name+' phys_'+name.removeprefix('sv_') for name in fields)+'\n'
    groups = [(math_path, ('VectorNormalize',)),
              (sample_path, ('AAS_PointAreaNum', 'AAS_PointPresenceType', 'AAS_TraceAreas',
                             'AAS_TraceClientBBox', 'AAS_PlaneFromNum')),
              (move_path, ('AAS_InitSettings', 'AAS_OnGround', 'AAS_Swimming',
                           'AAS_Accelerate', 'AAS_ApplyFriction', 'AAS_PredictClientMovement')),
              (table_path, ('AAS_RT_ShowRoute',))]
    for path, names in groups:
        body = path.read_text()
        source += body.split('*/', 1)[0] + '*/\n'
        for name in names:
            signature = re.search(r'^(?:static\s+)?(?:int|void|vec_t|aas_trace_t|aas_plane_t\s*\*)\s*'+name+r'\(',body,re.M)[0]
            if name == 'AAS_TraceClientBBox':
                source += '#define AAS_TraceClientBBox ReferenceTraceClientBBox\n'
            source += function(body,signature)+'\n'
            if name == 'AAS_TraceClientBBox':
                source += '''#undef AAS_TraceClientBBox
aas_trace_t AAS_TraceClientBBox(vec3_t start,vec3_t end,int presence,int entity){
 auto trace=ReferenceTraceClientBBox(start,end,presence,entity);
 if(trace.startsolid)indeterminatePlane=true;
 return trace;
}
'''
    source += '\n'.join('#undef '+name for name in fields)+'\n}\n'+COMPARE
    return source,[dict(path=str(path),sha256=sha(path)) for path,_ in groups]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('sp-source','mp-source','iortcw-source','report'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--target')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--float-mode',choices=('fast','strict'),default='strict')
    parser.add_argument('--float-unit',choices=('default','sse2'),default='default',
                        help='Optionally isolate x87 extended precision using SSE2 in the comparison only')
    parser.add_argument('--compiler',default='clang++')
    args=parser.parse_args();compiler=shutil.which(args.compiler)
    if not compiler:parser.error('C++ compiler unavailable')
    output=args.report.resolve();output.relative_to(ROOT/'.tmp');output.parent.mkdir(parents=True,exist_ok=True)
    target=[f'--target={args.target}'] if args.target else ([] if os.name=='nt' else ['-m32'])
    triple=subprocess.check_output([compiler,*target,'-dumpmachine'],cwd=ROOT,text=True).strip()
    flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    if triple.startswith('x86_64'):
        flags += ['-DFNTECH3_Q3_ONLY']
    if args.float_unit == 'sse2':
        flags += ['-msse2', '-mfpmath=sse']
    files=['code/botlib/be_aas_move.cpp','code/botlib/be_aas_sample.cpp','code/botlib/rtcw_movement.hpp',
           'code/compat/rtcw_prediction.hpp','code/compat/rtcw_game_abi.h','code/server/server_rtcw_aas_query.hpp',
           'tests/rtcw_prediction_tests.py','scripts/probe_rtcw_prediction.py']
    report=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),target=triple,sanitized=args.sanitize,
                floatMode=args.float_mode,floatUnit=args.float_unit,
                scope=__doc__,implementationSha256={name:sha(ROOT/name) for name in files},results=[])
    env=os.environ.copy()
    if args.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                         ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
        source,provenance=fixture(base,variant)
        row=dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode('utf-8')).hexdigest(),
                 agreementRequired=variant.startswith('original') and args.float_mode=='strict');report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='prediction-reference-',dir=output.parent) as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(source,encoding='utf-8')
            arithmetic=['-ffast-math'] if args.float_mode=='fast' else ['-fno-fast-math','-ffp-contract=off']
            command=[compiler,*target,*flags,'-std=c++17','-O2',*arithmetic,'-D_CRT_SECURE_NO_WARNINGS',
                     '-I',str(ROOT),f'-DMODE_SP={int(variant.endswith("SP"))}',
                     f'-DORIGINAL_REFERENCE={int(variant.startswith("original"))}',str(cpp),
                     str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',str(ROOT/'code/compat/game_abi_adapter.c'),'-o',str(exe)]
            built=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
            row.update(compileCommand=command,buildExitCode=built.returncode)
            if built.returncode:row['buildOutput']=built.stdout+built.stderr
            else:
                run=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=120,env=env)
                row.update(exitCode=run.returncode,stderr=run.stderr)
                if run.returncode:row['stdout']=run.stdout
                else:row['synthetic']=json.loads(run.stdout)
            row['executed']=not built.returncode and row.get('exitCode')==0
            row['agrees']=row['executed'] and row['synthetic']['differences']==0
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(variant,'AGREES' if row['agrees'] else 'DIFFERS' if row['executed'] else 'ERROR',row.get('synthetic'),flush=True)
    return 0 if all(row['executed'] and (row['agrees'] or not row['agreementRequired']) for row in report['results']) else 1


if __name__=='__main__':
    raise SystemExit(main())
