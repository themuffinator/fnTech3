#!/usr/bin/env python3
"""Compare RTCW movement initialization/reset with defined published behavior.

Original GPLv3 functions are compiled only in disposable fixtures retaining
their notice. Original reset-last-avoidance reads beyond an array and is excluded;
iortcw's corrected implementation is compared unchanged. Navigation and actual
movement prediction are outside this probe.
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
from rtcw_ai_state_tests import state_source
from rtcw_server_world_tests import function

COMPARE = r'''
}
template<class T> bool zero(const T &v){
 const auto *bytes=reinterpret_cast<const byte*>(&v);for(size_t i=0;i<sizeof(v);++i)if(bytes[i])return false;return true;
}
static int canonical(int flags){return (flags&63)|((flags&448)<<1);}
static unsigned initialized,resets,lastResets;
int main(){
 profile=&profiles[kSP?2:3];botimport.Print=print;
 const int h=BotAllocMoveState();auto *actual=BotMoveStateFromHandle(h);auto &expected=reference::state;
 for(unsigned previous=0;previous<512;++previous)for(unsigned inputFlags=0;inputFlags<1024;++inputFlags){
  actual->moveflags=canonical(previous)|64;expected.moveflags=previous;expected.lasttime=17;
  actual->lastareanum=expected.lastareanum=19;actual->avoidreach[0]=expected.avoidreach[0]=27;
  fntech3_rtcw_bot_initmove_t in{{1,2,3},{4,5,6},{7,8,9},1023,profile->maxClients-1,.125f,2,{10,11,12},int(inputFlags)};
  bot_initmove_t translated{};assert(fntech3::rtcw::bot::MoveState(in,profile->maxClients,translated));
  reference::bot_initmove_t original{};static_assert(sizeof(in)==sizeof(original));std::memcpy(&original,&in,sizeof(in));
  BotInitMoveState(h,&translated);reference::BotInitMoveState(1,&original);
  assert(actual->moveflags==(canonical(expected.moveflags)|64));
  assert(actual->entitynum==expected.entitynum&&actual->client==expected.client&&actual->thinktime==expected.thinktime&&actual->presencetype==expected.presencetype);
  assert(!memcmp(actual->origin,expected.origin,12)&&!memcmp(actual->velocity,expected.velocity,12));
  assert(!memcmp(actual->viewoffset,expected.viewoffset,12)&&!memcmp(actual->viewangles,expected.viewangles,12));
  assert(actual->lastareanum==expected.lastareanum&&actual->avoidreach[0]==expected.avoidreach[0]&&expected.lasttime==17);++initialized;
 }
 for(int n=0;n<4096;++n){
  actual->avoidreach[0]=expected.avoidreach[0]=n;
  actual->avoidreachtimes[0]=expected.avoidreachtimes[0]=float(n%7)-3;
  actual->avoidreachtries[0]=expected.avoidreachtries[0]=n%4;
#if REFERENCE_LAST_RESET_DEFINED
  BotResetLastAvoidReach(h);reference::BotResetLastAvoidReach(1);
  assert(!memcmp(actual->avoidreach,expected.avoidreach,sizeof(expected.avoidreach)));
  assert(!memcmp(actual->avoidreachtimes,expected.avoidreachtimes,sizeof(expected.avoidreachtimes)));
  assert(!memcmp(actual->avoidreachtries,expected.avoidreachtries,sizeof(expected.avoidreachtries)));++lastResets;
#endif
  for(bool reset:{false,true}){
   actual->avoidreach[0]=expected.avoidreach[0]=9;actual->avoidreachtimes[0]=expected.avoidreachtimes[0]=12;
   actual->avoidreachtries[0]=expected.avoidreachtries[0]=3;
   actual->lastareanum=expected.lastareanum=19;actual->lastgoalareanum=expected.lastgoalareanum=23;
   actual->lastreachnum=expected.lastreachnum=29;actual->origin[0]=expected.origin[0]=17;
   if(reset){BotResetAvoidReach(h);reference::BotResetAvoidReach(1);}
   else{BotInitAvoidReach(h);reference::BotInitAvoidReach(1);}
   assert(zero(actual->avoidreach)&&zero(expected.avoidreach)&&zero(actual->avoidreachtimes)&&zero(expected.avoidreachtimes));
   assert(zero(actual->avoidreachtries)&&zero(expected.avoidreachtries));
   assert(actual->lastareanum==expected.lastareanum&&actual->lastgoalareanum==expected.lastgoalareanum);
   assert(actual->lastreachnum==expected.lastreachnum&&actual->origin[0]==expected.origin[0]);++resets;
  }
  BotResetMoveState(h);reference::BotResetMoveState(1);assert(zero(*actual)&&zero(expected));++resets;
 }
 BotFreeMoveState(h);assert(allocations.empty());
 std::printf("{\"initializations\":%u,\"resets\":%u,\"lastAvoidanceResets\":%u}\n",initialized,resets,lastResets);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--mode', choices=('sp', 'mp'), required=True)
    parser.add_argument('--iortcw', action='store_true')
    parser.add_argument('--compiler', default='clang++')
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args(); compiler = shutil.which(args.compiler)
    if not compiler: parser.error('C++ compiler not found')
    base = args.source.resolve() / ('code' if args.iortcw else 'src')
    folder = base / ('botlib' if args.iortcw else 'game')
    path = base/'botlib/be_ai_move.c'; original = path.read_text()
    last_reset = function(original, 'void BotResetLastAvoidReach(')
    undefined_last_reset = 'ms->avoidreachtries[i]' in last_reset
    source = original[:original.index('*/')+2] + '\n' + state_source()
    source += f'\n#define REFERENCE_LAST_RESET_DEFINED {int(not undefined_last_reset)}\n'
    source += f'\nconstexpr bool kSP={str(args.mode == "sp").lower()};\nnamespace reference {{\n'
    source += f'#include "{(folder/"be_ai_goal.h").as_posix()}"\n#include "{(folder/"be_ai_move.h").as_posix()}"\n'
    start = original.index('typedef struct bot_movestate_s')
    end = original.index('} bot_movestate_t;', start) + len('} bot_movestate_t;')
    source += original[start:end]
    source += '\nstatic bot_movestate_t state;\nstatic bot_movestate_t *BotMoveStateFromHandle(int h){assert(h==1);return &state;}\n'
    for name in ('BotInitMoveState', 'BotInitAvoidReach', 'BotResetAvoidReach', 'BotResetLastAvoidReach', 'BotResetMoveState'):
        if name == 'BotResetLastAvoidReach' and undefined_last_reset:
            continue  # Do not compile, execute or silently repair undefined reference code.
        source += '\n' + function(original, f'void {name}(')
    source += COMPARE
    target = [f'--target={args.target}'] if args.target else ([] if os.name == 'nt' else ['-m32'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),mode=args.mode,
        referenceVariant='iortcw' if args.iortcw else 'original',
        target=subprocess.check_output([compiler,*target,'-dumpmachine'],cwd=ROOT,text=True).strip(),
        sanitized=args.sanitize,scope='Movement initialization and reset; actual navigation/movement excluded',
        referenceExclusions=['BotResetLastAvoidReach reads avoidreachtries[MAX_AVOIDREACH]; compare the corrected iortcw implementation separately'] if undefined_last_reset else [],
        implementationSha256=sha(ROOT/'code/botlib/be_ai_move.cpp'),adapterSha256=sha(ROOT/'code/compat/rtcw_bot_ai.hpp'),
        references=[dict(path=str(p),sha256=sha(p)) for p in (path,folder/'be_ai_move.h',folder/'be_ai_goal.h')])
    output=args.report.resolve();output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ai-state-reference-',dir=output.parent) as temp:
        cpp=Path(temp)/'probe.cpp';exe=Path(temp)/'probe.exe';cpp.write_text(source,encoding='utf-8')
        diagnostic=['-DFNTECH3_Q3_ONLY'] if os.name=='nt' and report['target'].startswith('x86_64') else []
        command=[compiler,*target,*flags,*diagnostic,'-std=c++17','-O2','-I',str(ROOT),str(cpp),
            str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',str(ROOT/'code/compat/game_abi_adapter.c'),'-o',str(exe)]
        built=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
        report.update(compileCommand=command,buildExitCode=built.returncode)
        if built.returncode:report['buildOutput']=built.stdout+built.stderr
        else:
            env=os.environ.copy()
            if args.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            options={'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
            run=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=60,env=env,**options)
            report.update(exitCode=run.returncode,stderr=run.stderr)
            if not run.returncode:report.update(json.loads(run.stdout))
            else:report['stdout']=run.stdout
        report['passed']=not built.returncode and report.get('exitCode')==0
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL',args.mode,report.get('initializations',0),'initializations,',report.get('resets',0),'resets')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
