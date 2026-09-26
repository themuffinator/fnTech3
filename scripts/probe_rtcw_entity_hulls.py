#!/usr/bin/env python3
"""Compare RTCW server hull selection with original/iortcw source fixtures.

Original GPLv3 bodies compile only in disposable programs with their notices.
The engine path reads frozen external game records through its real adapter;
the reference reads those same external records directly. Collision backends
record the chosen model and bounds; this is not collision geometry parity.
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
sys.path.insert(0, str(ROOT / 'tests'))
from rtcw_entity_collision_tests import server_source
from rtcw_server_world_tests import function

COMPARE = r'''
int main(){
 profile=profiles[MODE_SP?2:3];
 constexpr size_t base=16,stride=sizeof(NativeEntity)+64;
 std::vector<byte> memory(base+stride+16,0xa5),clients(sizeof(NativePlayer));
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=memory.data()+base;binding.entitySpanBytes=stride;binding.entityCount=1;binding.entityStride=stride;
 binding.clientBase=clients.data();binding.clientSpanBytes=clients.size();binding.clientCount=1;binding.clientStride=sizeof(NativePlayer);
 assert(serverGameData.BindNative(profile.id,binding)==fntech3::server::GameDataBindError::None);
 unsigned comparisons=0;
 for(int brush:{-1,0,1,2})for(unsigned flags:{0u,unsigned(FNTECH3_RTCW_SVF_CAPSULE),0x80000000u,0x80000000u|FNTECH3_RTCW_SVF_CAPSULE})
 for(bool sweep:{false,true})for(int model=0;model<4;++model)for(int size=0;size<8;++size){
  NativeEntity input{};input.r.bmodel=brush;input.s.modelindex=model;input.r.svFlags=int(flags);
  input.s.number=0;input.r.s.number=971;
  for(int axis=0;axis<3;++axis){input.r.mins[axis]=-float(size*(axis+1));input.r.maxs[axis]=float(size*(axis+2));}
  memcpy(binding.entityBase,&input,sizeof(input));const auto unchanged=memory;
  auto reset=[](){hullCalls=0;hullCapsule=-1;VectorClear(observedMins);VectorClear(observedMaxs);};
  reset();int actual=SV_ClipHandleForEntity(static_cast<sharedEntity_t*>(binding.entityBase),sweep?qtrue:qfalse);
  int actualCalls=hullCalls,actualCapsule=hullCapsule;vec3_t actualMin,actualMax;
  VectorCopy(observedMins,actualMin);VectorCopy(observedMaxs,actualMax);
  reset();int expected=reference::SV_ClipHandleForEntity(&input);
  assert(actual==expected&&actualCalls==hullCalls&&actualCapsule==hullCapsule);
  assert(!memcmp(actualMin,observedMins,12)&&!memcmp(actualMax,observedMaxs,12)&&memory==unchanged);
  ++comparisons;
 }
 serverGameData.Invalidate();printf("{\"comparisons\":%u}\n",comparisons);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base):
    world = base/'server/sv_world.c'; header = base/'game/g_public.h'
    text = world.read_text()
    flag = int(re.search(r'^#define\s+SVF_CAPSULE\s+(0[xX][0-9a-fA-F]+|[0-9]+)', header.read_text(), re.M)[1],0)
    source = server_source() + r'''
#include <type_traits>
using NativeEntity=std::conditional_t<MODE_SP,fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_mp_sharedEntity_t>;
using NativePlayer=std::conditional_t<MODE_SP,fntech3_rtcw_sp_playerState_t,fntech3_rtcw_mp_playerState_t>;
'''
    source += f'\nstatic_assert(FNTECH3_RTCW_SVF_CAPSULE=={flag});\n'
    source += '#pragma push_macro("SVF_CAPSULE")\n#undef SVF_CAPSULE\n'+f'#define SVF_CAPSULE {flag}\n'
    source += text.split('*/',1)[0]+'*/\nnamespace reference {\nusing sharedEntity_t=NativeEntity;\n'
    source += function(text,'clipHandle_t SV_ClipHandleForEntity(')
    source += '\n}\n#pragma pop_macro("SVF_CAPSULE")\n'+COMPARE
    return source,[dict(path=str(path),sha256=sha(path)) for path in (world,header)]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('sp-source','mp-source','iortcw-source','report'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--compiler',default='clang++')
    parser.add_argument('--target')
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();compiler=shutil.which(args.compiler)
    if not compiler:parser.error('C++ compiler unavailable')
    output=args.report.resolve();output.relative_to(ROOT/'.tmp');output.parent.mkdir(parents=True,exist_ok=True)
    target=['--target='+args.target] if args.target else ([] if os.name=='nt' else ['-m32'])
    triple=subprocess.check_output([compiler,*target,'-dumpmachine'],cwd=ROOT,text=True).strip()
    flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    if triple.startswith('x86_64'):flags+=['-DFNTECH3_Q3_ONLY']
    files=['code/qcommon/game_profile.h','code/qcommon/game_profile.cpp','code/server/sv_collision_model.hpp',
        'code/server/sv_world.cpp','code/server/sv_game.cpp','code/server/server_game_data.cpp',
        'tests/rtcw_entity_collision_tests.py','scripts/probe_rtcw_entity_hulls.py']
    report=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope=__doc__,target=triple,sanitized=args.sanitize,
        implementationSha256={name:sha(ROOT/name) for name in files},results=[])
    env=os.environ.copy()
    if args.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                        ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
        source,provenance=fixture(base)
        row=dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode()).hexdigest())
        report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='entity-hulls-',dir=output.parent) as temp:
            folder=Path(temp);cpp=folder/'probe.cpp';exe=folder/'probe.exe';cpp.write_text(source,encoding='utf-8')
            command=[compiler,*target,*flags,'-std=c++17','-O2','-D_CRT_SECURE_NO_WARNINGS','-I',str(ROOT),
                f'-DMODE_SP={int(variant.endswith("SP"))}',str(cpp),str(ROOT/'code/server/server_game_data.cpp'),
                '-x','c++',str(ROOT/'code/compat/game_abi_adapter.c'),'-o',str(exe)]
            built=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
            row.update(compileCommand=command,buildExitCode=built.returncode,buildOutput=built.stdout+built.stderr)
            if not built.returncode:
                options={'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
                run=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=30,env=env,**options)
                row.update(exitCode=run.returncode,stderr=run.stderr)
                if not run.returncode:row.update(json.loads(run.stdout))
                else:row['stdout']=run.stdout
            row['passed']=not built.returncode and row.get('exitCode')==0 and row.get('comparisons')==1024
        output.write_text(json.dumps(report,indent=2)+'\n')
        print(variant,'PASS' if row['passed'] else 'FAIL',row.get('comparisons'),flush=True)
    return 0 if all(row['passed'] for row in report['results']) else 1


if __name__=='__main__':
    raise SystemExit(main())
