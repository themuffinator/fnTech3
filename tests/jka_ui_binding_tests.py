"""Execute the retail-shaped UI binding query against real host metadata."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT=Path(__file__).resolve().parents[1]


def fixture():
    host=(ROOT/'code/client/client_jka_ui_host.hpp').read_text()
    vm=(ROOT/'code/qcommon/vm.c').read_text()
    keys=(ROOT/'code/qcommon/keys.c').read_text()
    strings=(ROOT/'code/qcommon/q_shared.c').read_text()
    start=host.index('\tcase Call::UI_KEY_GETBINDINGBUF:')
    end=host.index('\tcase Call::UI_PC_LOAD_SOURCE:',start)
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/client/keys.h"
#include "code/compat/jka_module_abi.h"
#include "code/compat/jka_key_adapter.h"
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <limits>
struct Drop{};
void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
qkey_t keys[MAX_KEYS]{};
static vm_t *uivm;
''' + function(vm,'void *VM_ExplicitArgPtr(') + '\n' + function(vm,'void VM_CheckBounds(') + '\n' + \
        function(keys,'const char *Key_GetBinding(') + '\n' + function(strings,'void Q_strncpyz(') + '\n' + \
        function(host,'static void *CL_JKAUINativeSpan(') + r'''
using Call=fntech3::jka::module::Call;
static bool invoke(intptr_t *args,intptr_t &result){result=0;switch(Call::UI_KEY_GETBINDINGBUF){
''' + host[start:end] + r'''
default:throw 9;}}
template<class F> static void reject(F call){bool failed=false;try{call();}catch(Drop){failed=true;}assert(failed);}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);uivm=&vm;
 char wrong[]="wrong raw ordinal", tab[]="toggle", enter[]="say enter", shift[]="+speed", attack[]="+attack", letter[]="+left";
 keys[1].binding=wrong;keys[10].binding=wrong;keys[K_TAB].binding=tab;keys[K_ENTER].binding=enter;
 keys[K_SHIFT].binding=shift;keys[K_MOUSE1].binding=attack;keys['a'].binding=letter;
 keys[K_F1].binding=attack;keys[K_JOY32].binding=enter;keys[K_AUX16].binding=letter;
 auto initial=std::array<qkey_t,MAX_KEYS>{};std::memcpy(initial.data(),keys,sizeof(keys));
 std::array<byte,40> buffer;buffer.fill(0xa5);auto output=reinterpret_cast<intptr_t>(buffer.data()+3);
 auto query=[&](intptr_t key,intptr_t size){intptr_t args[]{35,key,output,size},result=-1;assert(invoke(args,result)&&result==0);};
 auto check=[&](intptr_t key,const char *value){buffer.fill(0xa5);query(key,32);assert(!std::strcmp(reinterpret_cast<char *>(buffer.data()+3),value));assert(buffer[0]==0xa5&&buffer[2]==0xa5&&buffer[35]==0xa5&&buffer[39]==0xa5);};
 check(1,shift);check(9,tab);check(10,enter);check(141,attack);check(65,letter);check(97,"");
 check(28,attack);check(287,enter);check(303,letter);check(-1,"");check(0,"");check(320,"");
 for(int key : {129,130,131,136,155,160,164,174,255,304,319})check(key,"");
 buffer.fill(0xa5);query(10,1);assert(buffer[3]==0&&buffer[4]==0xa5);
 buffer.fill(0xa5);query(10,4);assert(!std::memcmp(buffer.data()+3,"say\0",4)&&buffer[7]==0xa5);
 for(intptr_t size : {intptr_t(0),intptr_t(-1)}){auto before=buffer;reject([&]{query(10,size);});assert(buffer==before);}
 intptr_t args[]{35,10,0,32},result=0;auto before=buffer;reject([&]{invoke(args,result);});assert(buffer==before);
 args[2]=-1;reject([&]{invoke(args,result);});assert(buffer==before);
 args[2]=output;vm.entryPoint=nullptr;reject([&]{invoke(args,result);});assert(buffer==before);
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.dllExports=reinterpret_cast<void *>(1);reject([&]{invoke(args,result);});assert(buffer==before);
 vm.dllExports=nullptr;uivm=nullptr;reject([&]{invoke(args,result);});assert(buffer==before);uivm=&vm;
 assert(!std::memcmp(initial.data(),keys,sizeof(keys)));
 assert(FNTech3_JKAUIBindingKey(8)==K_BACKSPACE&&FNTech3_JKAUIBindingKey(127)==K_DEL);
 assert(FNTech3_JKAUIBindingKey(137)==K_MWHEELUP&&FNTech3_JKAUIBindingKey(139)==K_MWHEELDOWN);
 assert(FNTech3_JKAUIBindingKey(215)==K_KP_STAR&&FNTech3_JKAUIBindingKey(247)==K_KP_SLASH);
 assert(FNTech3_JKAUIBindingKey(256)==K_JOY1&&FNTech3_JKAUIBindingKey(288)==K_AUX1);
}
'''


class JKAUIBindingTests(unittest.TestCase):
    def test_production_three_argument_query_key_slots_and_bounded_buffers(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++17 compiler required')
        parent=ROOT/'.tmp/tests/jka-ui-binding';parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder=Path(temporary).resolve();cpp=folder/'probe.cpp';exe=folder/'probe.exe';cpp.write_text(fixture(),encoding='utf-8')
            target=compiler_target_args(compiler)
            if os.name=='nt' and not target:target=['-DFNTECH3_Q3_ONLY']
            build=subprocess.run([compiler,*target,'-std=c++17','-O2','-ffast-math','-Werror','-I',str(ROOT),str(cpp),'-o',str(exe)],
                cwd=folder,text=True,capture_output=True,timeout=90)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],cwd=folder,text=True,capture_output=True,timeout=20)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertTrue(folder.is_relative_to(parent.resolve()))
            self.assertFalse(any(p.is_symlink() or getattr(p.lstat(),'st_file_attributes',0)&0x400 for p in folder.rglob('*')))


if __name__=='__main__':unittest.main()
