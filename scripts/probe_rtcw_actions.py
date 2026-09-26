#!/usr/bin/env python3
"""Compare RTCW action state with unchanged published SP/MP functions.

Reference implementation text and its GPLv3 notice are compiled only in the
disposable fixture. No game modules, assets or player input are used.
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
from compiler_support import compiler_target_args
from rtcw_actions_tests import action_source
from rtcw_server_world_tests import function

COMPARE = r'''
}
static unsigned comparisons;
static void compare(){
 bot_input_t input;EA_GetInput(0,.125f,&input);
 auto actual=fntech3::rtcw::bot::ActionOutput(input,EA_ExtraInputFlags(0));
 reference::bot_input_t expected;reference::EA_GetInput(0,.125f,&expected);
 static_assert(sizeof(actual)==sizeof(expected));assert(!memcmp(&actual,&expected,sizeof(actual)));++comparisons;
}
static void restore(const fntech3_rtcw_bot_input_t &input){
 bot_input_t state;assert(fntech3::rtcw::bot::ActionInput(input,state));
 EA_RestoreInput(0,&state,unsigned(input.actionflags)&~0x3fffu);
 reference::bot_input_t expected;std::memcpy(&expected,&input,sizeof(input));reference::EA_ResetInput(0,&expected);
}
int main(){
 setup();profile=&profiles[kSP?2:3];botlibglobals.maxclients=profile->maxClients;EA_Setup();
 reference::botimport.BotClientCommand=commandCallback;
 struct Pair{void(*own)(int);void(*original)(int);};
 const Pair actions[]={
  {EA_Gesture,reference::EA_Gesture},{EA_Attack,reference::EA_Attack},{reload,reference::EA_Reload},
  {EA_Talk,reference::EA_Talk},{EA_Use,reference::EA_Use},{EA_Respawn,reference::EA_Respawn},
  {EA_Jump,reference::EA_Jump},{EA_DelayedJump,reference::EA_DelayedJump},
  {EA_Crouch,reference::EA_Crouch},{EA_Walk,reference::EA_Walk},
  {EA_MoveUp,reference::EA_MoveUp},{EA_MoveDown,reference::EA_MoveDown},
  {EA_MoveForward,reference::EA_MoveForward},{EA_MoveBack,reference::EA_MoveBack},
  {EA_MoveLeft,reference::EA_MoveLeft},{EA_MoveRight,reference::EA_MoveRight}};
 for(unsigned flags=0;flags<16384;++flags){
  fntech3_rtcw_bot_input_t initial{.25f,{1,-2,3},431,{4,5,-6},0,INT_MIN};
  unsigned raw=flags|((flags*0x9e3779b9u)&~0x3fffu);std::memcpy(&initial.actionflags,&raw,4);
  restore(initial);compare();
  for(auto action:actions){restore(initial);action.own(0);action.original(0);compare();}
  restore(initial);EA_ResetInput(0);reference::EA_ResetInput(0,nullptr);compare();
  EA_Jump(0);reference::EA_Jump(0);compare();
  EA_DelayedJump(0);reference::EA_DelayedJump(0);compare();
  EA_ResetInput(0);reference::EA_ResetInput(0,nullptr);compare();
 }
 float vector[3]={17,-23,31};
 for(float speed:{-999.f,-400.f,-.0f,0.f,3.25f,400.f,999.f}){
  EA_Move(0,vector,speed);reference::EA_Move(0,vector,speed);compare();
 }
 EA_View(0,vector);reference::EA_View(0,vector);compare();
 for(int weapon:{INT_MIN,-1,0,63,INT_MAX}){EA_SelectWeapon(0,weapon);reference::EA_SelectWeapon(0,weapon);compare();}
 EA_EndRegular(0,10);reference::EA_EndRegular(0,10);compare();
 struct TextPair{void(*own)(int,const char*);void(*original)(int,char*);};
 const TextPair textActions[]={{EA_Say,reference::EA_Say},{EA_SayTeam,reference::EA_SayTeam},
  {EA_Command,reference::EA_Command},{EA_UseItem,reference::EA_UseItem},{EA_DropItem,reference::EA_DropItem},
  {EA_UseInv,reference::EA_UseInv},{EA_DropInv,reference::EA_DropInv}};
 for(auto action:textActions)for(std::string text:{std::string(),std::string("item name; test"),std::string(1023,'x')}){
  action.own(0,text.c_str());auto expected=command;action.original(0,text.data());assert(command==expected&&commandClient==0);++comparisons;
 }
 EA_Shutdown();assert(allocations.empty());std::printf("{\"comparisons\":%u}\n",comparisons);
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
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('C++ compiler not found')
    base = args.source.resolve(strict=True) / ('code' if args.iortcw else 'src')
    path = base / 'botlib/be_ea.c'
    header = base / ('botlib' if args.iortcw else 'game') / 'botlib.h'
    original = path.read_text(); types = header.read_text()
    source = original[:original.index('*/') + 2] + '\n' + action_source()
    source += '\nstatic void reload(int client){EA_Action(client,ACTION_RTCW_RELOAD);}\n'
    source += f'constexpr bool kSP={str(args.mode == "sp").lower()};\n'
    # Only the original record/flag declarations and unchanged EA functions
    # enter the reference namespace. The engine ABI remains independently typed.
    for name in re.findall(r'^#define\s+(ACTION_\w+)', types, re.M):
        source += f'#undef {name}\n'
    source += '#undef ACTION_JUMPEDLASTFRAME\nnamespace reference {\n'
    source += '\n'.join(re.findall(r'^#define\s+ACTION_.*$', types, re.M)) + '\n'
    source += re.search(r'^#define\s+ACTION_JUMPEDLASTFRAME.*$', original, re.M)[0] + '\n'
    source += 'constexpr int MAX_USERMOVE=400;\n'
    start = types.index('typedef struct bot_input_s')
    source += types[start:types.index('} bot_input_t;', start) + len('} bot_input_t;')]
    source += '\nstatic bot_input_t inputs[1],*botinputs=inputs;\nstruct {void(*BotClientCommand)(int,const char*);}botimport;\n'
    for match in re.finditer(r'^void (EA_\w+)\s*\(', original, re.M):
        if match[1] != 'EA_Shutdown':
            source += '\n' + function(original, match[0])
    source += COMPARE
    # Without --target, the 32-bit target the tests use (compiler_support.py).
    target = [f'--target={args.target}'] if args.target else compiler_target_args(compiler)
    triple = subprocess.check_output([compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip()
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    if os.name == 'nt' and triple.startswith('x86_64'):
        flags += ['-DFNTECH3_Q3_ONLY']
    output = args.report.resolve(); output.relative_to(ROOT / '.tmp')
    output.parent.mkdir(parents=True, exist_ok=True)
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), mode=args.mode,
        referenceVariant='iortcw' if args.iortcw else 'original', target=triple, sanitized=args.sanitize,
        scope='Elementary action state, flag aliases, reset/get and command text; no user input or gameplay',
        implementationSha256=sha(ROOT / 'code/botlib/be_ea.cpp'),
        adapterSha256=sha(ROOT / 'code/compat/rtcw_bot_actions.hpp'),
        references=[dict(path=str(p), sha256=sha(p)) for p in (path, header)])
    with tempfile.TemporaryDirectory(prefix='actions-reference-', dir=output.parent) as temp:
        cpp = Path(temp) / 'probe.cpp'; exe = Path(temp) / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        command = [compiler, *target, *flags, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp),
            str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
            str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
        report.update(compileCommand=command, buildExitCode=built.returncode)
        if built.returncode:
            report['buildOutput'] = built.stdout + built.stderr
        else:
            env = os.environ.copy()
            if args.sanitize:
                env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60, env=env, **options)
            report.update(exitCode=run.returncode, stderr=run.stderr)
            if run.returncode:
                report['stdout'] = run.stdout
            else:
                report.update(json.loads(run.stdout))
        report['passed'] = not built.returncode and report.get('exitCode') == 0
    output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', args.mode, report.get('comparisons', 0), 'comparisons')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
