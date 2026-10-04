"""Run Academy's real bot actions and the game dispatcher input boundary."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
import rtcw_actions_tests as actions
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def source():
    # Reuse the real botlib setup fixture. Only the dispatcher cases are
    # extracted: calls still execute be_ea.cpp and the actual Academy adapter.
    prelude = actions.action_source().replace(
        'using Call=fntech3::rtcw::module::Call;', '').replace(
        'using namespace fntech3::rtcw::module;', '')
    game = (ROOT / 'code/server/server_jka_game.hpp').read_text()
    start = game.index('\tcase Call::BOTLIB_EA_SAY:')
    end = game.index('\t/* --- the state a bot keeps', start)
    vm = (ROOT / 'code/qcommon/vm.c').read_text()
    prelude += '\n' + function(vm, 'void *VM_ExplicitArgPtr(')
    prelude += '\n' + function(vm, 'void VM_CheckBounds(')
    return prelude + r'''
#include "code/compat/jka_module_abi.h"
#include "code/server/server_jka_actions.hpp"
using Call=fntech3::jka::module::Call;
vm_t *gvm;
static float JKAFloat(intptr_t value){float result;int bits=int(value);std::memcpy(&result,&bits,4);return result;}
#undef VMF
#define VMF(index) JKAFloat(args[index])
static bool callAction(Call call,const intptr_t *args){
 auto pointer=[&](int index){return VM_ExplicitArgPtr(gvm,args[index]);};
 auto text=[&](int index){return static_cast<const char*>(pointer(index));};
 switch(call){
''' + game[start:end] + r'''
 default:return false;
 }
}
static void check(bool native){
 for(const auto &p:profiles)if(p.id==COM_GAME_JKA_MP)profile=&p;
 botlibglobals.maxclients=profile->maxClients;assert(EA_Setup()==0);
 std::array<byte,96> memory;memory.fill(0xa5);
 vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=int(memory.size());
 if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 gvm=&vm;int client=profile->maxClients-1;
 auto address=[&](int at)->intptr_t{return native?reinterpret_cast<intptr_t>(memory.data()+at):at;};
 auto invoke=[&](Call call,intptr_t selected,std::initializer_list<intptr_t> extra={}){
  intptr_t args[5]={0,selected};int n=2;for(auto value:extra)args[n++]=value;
  assert(callAction(call,args));
 };
 auto get=[&](int at=3){
  auto before=memory;invoke(Call::BOTLIB_EA_GET_INPUT,client,{floatBits(.25f),address(at)});
  bot_input_t input;std::memcpy(&input,memory.data()+at,40);
  std::memcpy(before.data()+at,&input,40);assert(before==memory);return input;
 };
 invoke(Call::BOTLIB_EA_ALT_ATTACK,client);invoke(Call::BOTLIB_EA_FORCEPOWER,client);
 invoke(Call::BOTLIB_EA_ATTACK,client);invoke(Call::BOTLIB_EA_SELECT_WEAPON,client,{17});
 assert(get().actionflags==0x300001&&get().weapon==17&&get().thinktime==.25f);
 // The profile output translates only the marker and leaves other fields.
 for(unsigned flags=0;flags<4096;++flags){
  bot_input_t input{.25f,{1,-2,3},4,{5,6,-7},int(flags<<12),8};
  input.actionflags|=ACTION_JUMPEDLASTFRAME;
  auto output=SV_JKABotActionOutput(input);
  auto expected=input;expected.actionflags=(input.actionflags&~ACTION_JUMPEDLASTFRAME)|0x800000;
  assert(!std::memcmp(&output,&expected,40));
 }
 invoke(Call::BOTLIB_EA_RESET_INPUT,client);assert(get().actionflags==0&&get().weapon==17);
 invoke(Call::BOTLIB_EA_JUMP,client);assert(get().actionflags==0x10);
 invoke(Call::BOTLIB_EA_RESET_INPUT,client);assert(get().actionflags==0x800000);
 assert(botinputs[client].actionflags==ACTION_JUMPEDLASTFRAME);
 invoke(Call::BOTLIB_EA_JUMP,client);assert(get().actionflags==0x800000);
 invoke(Call::BOTLIB_EA_RESET_INPUT,client);assert(get().actionflags==0);
 invoke(Call::BOTLIB_EA_JUMP,client);invoke(Call::BOTLIB_EA_ALT_ATTACK,client);
 assert(get().actionflags==0x200010);invoke(Call::BOTLIB_EA_RESET_INPUT,client);
 invoke(Call::BOTLIB_EA_FORCEPOWER,client);assert(get().actionflags==0x900000);
 auto unchanged=[&](auto attempt){auto before=memory;auto state=botinputs[client];
  rejected(attempt);assert(before==memory&&!std::memcmp(&state,&botinputs[client],40));};
 for(int bad:{-1,profile->maxClients,INT_MIN,INT_MAX}){
  unchanged([&]{invoke(Call::BOTLIB_EA_ALT_ATTACK,bad);});
  unchanged([&]{invoke(Call::BOTLIB_EA_FORCEPOWER,bad);});
  unchanged([&]{invoke(Call::BOTLIB_EA_GET_INPUT,bad,{0,address(3)});});
 }
 if(sizeof(intptr_t)>4)unchanged([&]{invoke(Call::BOTLIB_EA_ALT_ATTACK,INT64_C(0x100000000));});
 unchanged([&]{invoke(Call::BOTLIB_EA_GET_INPUT,client,{0,0});});
 get(int(memory.size()-40));
 if(!native)for(int bad:{-1,int(memory.size()-39),int(memory.size()),INT_MAX})
  unchanged([&]{invoke(Call::BOTLIB_EA_GET_INPUT,client,{0,bad});});
 botlib_export=nullptr;unchanged([&]{invoke(Call::BOTLIB_EA_FORCEPOWER,client);});
 botlib_export=&exports;EA_Shutdown();rejected([&]{invoke(Call::BOTLIB_EA_ALT_ATTACK,client);});
 assert(allocations.empty());
}
int main(){setup();check(false);check(true);}
'''


class JKAActionsTests(unittest.TestCase):
    def test_retail_action_bits_reset_lifecycle_and_checked_outputs(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jka-actions'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary).resolve()
            cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source(), encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, '-O2', '-std=c++17',
                                    '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(cpp),
                                    str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
                                    str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)],
                                   cwd=folder, capture_output=True, text=True, timeout=90)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertTrue(folder.is_relative_to(parent.resolve()))
            self.assertFalse(any(p.is_symlink() or p.stat().st_file_attributes & 0x400
                                 for p in folder.rglob('*')) if os.name == 'nt'
                             else any(p.is_symlink() for p in folder.rglob('*')))


if __name__ == '__main__':
    unittest.main()
