"""Execute retail UI cvar cases with the production VM and canonical cvar calls."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import engine_fixture_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    host = (ROOT / 'code/client/client_jka_ui_host.hpp').read_text()
    vm = (ROOT / 'code/qcommon/vm.c').read_text()
    cvar = (ROOT / 'code/qcommon/cvar.c').read_text()
    start = host.index('\tcase Call::UI_CVAR_REGISTER:')
    end = host.index('\tcase Call::UI_CVAR_CREATE:', start)
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/client/client_jka_ui.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
struct Drop {};
void QDECL Com_Error(errorParm_t, const char *, ...) { throw Drop{}; }
void QDECL Com_Printf(const char *, ...) {}
void QDECL Com_DPrintf(const char *, ...) {}
void Q_strncpyz(char *to, const char *from, int size) {
 assert(to && from && size > 0);
 int n=0;for(;n+1<size&&from[n];++n)to[n]=from[n];to[n]=0;
}
static vm_t *uivm;
static cvar_t cvar_indexes[3];
static unsigned cvar_numIndexes=3;
static int calls, observedFlags;
static cvar_t *Cvar_FindVar(const char *name) {
 for(auto &value:cvar_indexes)if(value.name&&!std::strcmp(value.name,name))return &value;
 return nullptr;
}
cvar_t *Cvar_Get(const char *name, const char *text, int flags) {
 auto *value=Cvar_FindVar(name);assert(value && text);
 ++calls;observedFlags=flags;value->flags|=flags;return value;
}
#define INVALID_FLAGS (CVAR_USER_CREATED | CVAR_SERVER_CREATED | CVAR_PRIVATE | CVAR_MODIFIED | CVAR_NONEXISTENT)
''' + function(cvar, 'void Cvar_Register(') + '\n' + function(cvar, 'void Cvar_Update(') + '\n' + \
        function(vm, 'void *VM_ExplicitArgPtr(') + '\n' + function(vm, 'void VM_CheckBounds(') + '\n' + \
        function(host, 'static void *CL_JKAUINativeSpan(') + r'''
#undef VMA
#define VMA(index) static_cast<const char *>(VM_ExplicitArgPtr(uivm,args[index]))
using Call=fntech3::jka::module::Call;
static bool dispatch(Call call, intptr_t *args) {
 namespace ui=fntech3::jka::ui;
 switch(call) {
''' + host[start:end] + r'''
 default:assert(false);return false;
 }
}
static void invoke(intptr_t *args) {
 auto call=fntech3::jka::module::Lookup(COM_GAME_JKA_MP,
  fntech3::jka::module::Module::UI,fntech3::jka::module::Direction::IMPORT,args[0]);
 assert(dispatch(call,args));
}
template<class F>static void reject(F action){bool dropped=false;try{action();}catch(Drop){dropped=true;}assert(dropped);}
int main(){
 static_assert(sizeof(fntech3_jka_vmCvar_t)==272);
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.privateFlag=CVAR_PRIVATE;uivm=&vm;
 cvar_indexes[0].name=const_cast<char *>("private");cvar_indexes[0].string=const_cast<char *>("secret");
 cvar_indexes[0].flags=CVAR_PRIVATE;cvar_indexes[0].modificationCount=5;
 cvar_indexes[1].name=const_cast<char *>("safe");cvar_indexes[1].string=const_cast<char *>("12.5");
 cvar_indexes[1].modificationCount=3;cvar_indexes[1].value=12.5f;cvar_indexes[1].integer=12;
 cvar_indexes[2].name=const_cast<char *>("debug_protocol");cvar_indexes[2].string=const_cast<char *>("");
 // Both retail UI_INIT forms create cvar metadata without touching an output.
 intptr_t registration[]{51,0,reinterpret_cast<intptr_t>("safe"),reinterpret_cast<intptr_t>("17"),0x800};
 invoke(registration);assert(calls==1&&observedFlags==CVAR_VM_CREATED);
 registration[2]=reinterpret_cast<intptr_t>("debug_protocol");registration[4]=0;
 invoke(registration);assert(calls==2&&observedFlags==CVAR_VM_CREATED);
 registration[2]=reinterpret_cast<intptr_t>("private");invoke(registration);assert(calls==2);
 // Unaligned records have exact external bounds; no canonical flags escape.
 std::array<byte,280> guarded;guarded.fill(0xa5);auto address=reinterpret_cast<intptr_t>(guarded.data()+3);
 registration[1]=address;registration[2]=reinterpret_cast<intptr_t>("safe");registration[4]=0xffffffffu;
 invoke(registration);assert(calls==3&&observedFlags==(CVAR_VM_CREATED|(0x7ff&~INVALID_FLAGS)));
 fntech3_jka_vmCvar_t output;std::memcpy(&output,guarded.data()+3,sizeof(output));
 assert(output.handle==1&&output.modificationCount==3&&output.value==12.5f&&output.integer==12);
 assert(!std::strcmp(output.string,"12.5")&&guarded[2]==0xa5&&guarded[275]==0xa5);
 intptr_t update[]{52,address};
 // An unchanged canonical update must preserve even nonterminated external bytes.
 std::memset(output.string,0xfe,sizeof(output.string));std::memcpy(guarded.data()+3,&output,sizeof(output));
 auto saved=guarded;invoke(update);assert(guarded==saved);
 registration[2]=reinterpret_cast<intptr_t>("private");invoke(registration);assert(calls==3&&guarded==saved);
 output.handle=0;std::memcpy(guarded.data()+3,&output,sizeof(output));saved=guarded;
 invoke(update);assert(guarded==saved);
 output.handle=1;std::memcpy(guarded.data()+3,&output,sizeof(output));
 char large[600];std::memset(large,'x',599);large[599]=0;
 cvar_indexes[1].string=large;++cvar_indexes[1].modificationCount;
 invoke(update);std::memcpy(&output,guarded.data()+3,sizeof(output));
 assert(output.modificationCount==4&&output.string[254]=='x'&&output.string[255]==0);
 assert(guarded[2]==0xa5&&guarded[275]==0xa5);
 for(int invalid:{-1,3,INT_MAX}){
  output.handle=invalid;std::memcpy(guarded.data()+3,&output,sizeof(output));saved=guarded;
  reject([&]{invoke(update);});assert(guarded==saved);
 }
 update[1]=0;reject([&]{invoke(update);});update[1]=-1;reject([&]{invoke(update);});
 registration[1]=-1;reject([&]{invoke(registration);});
 registration[1]=0;registration[2]=reinterpret_cast<intptr_t>("safe");
 vm.entryPoint=nullptr;reject([&]{invoke(registration);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.dllExports=reinterpret_cast<void *>(1);
 reject([&]{invoke(registration);});update[1]=address;reject([&]{invoke(update);});
 vm.dllExports=nullptr;uivm=nullptr;reject([&]{invoke(registration);});reject([&]{invoke(update);});
}
'''


class JKAUICvarTests(unittest.TestCase):
    def test_nullable_registration_and_frozen_record_updates(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-ui-cvar'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / 'host.cpp'
            executable = directory / ('host.exe' if os.name == 'nt' else 'host')
            source.write_text(fixture())
            built = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17',
                '-O2', '-ffast-math', '-Werror', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT),
                str(source), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
