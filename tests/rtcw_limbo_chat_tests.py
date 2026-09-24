"""Exercise MP chat history and the exact native/QVM cgame-to-UI boundary."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/compat/rtcw_limbo_chat.hpp"
#include <cassert>
#include <climits>
#include <limits>
#include <string>
static bool multiplayer=true;
static int Com_GameIsRTCWMP(){return multiplayer;}
void QDECL Com_Error(errorParm_t,const char *,...){throw 1;}
struct vm_t{bool native;unsigned char bytes[1024];};
static void Bounds(vm_t *vm,intptr_t address,std::size_t size){
 if(!vm||(!vm->native&&(address<0||static_cast<uintptr_t>(address)>sizeof(vm->bytes)||size>sizeof(vm->bytes)-address)))throw 2;
}
#define VM_CHECKBOUNDS Bounds
static void *VM_ExplicitArgPtr(vm_t *vm,intptr_t value){return vm->native?reinterpret_cast<void *>(value):vm->bytes+value;}
static struct{rtcwLimboChat_t rtcwLimboChat;}cl;
template<class F>static void Reject(F action){
 const auto saved=cl.rtcwLimboChat;bool failed=false;try{action();}catch(int){failed=true;}
 assert(failed&&!memcmp(&saved,&cl.rtcwLimboChat,sizeof(saved)));
}
'''

CHECKS = r'''
int main(){
 for(bool native:{false,true}){
  cl.rtcwLimboChat={};vm_t vm{native,{}};
  auto ptr=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(vm.bytes+offset):offset;};
  auto submit=[&](const std::string &text){assert(text.size()<700);memcpy(vm.bytes+13,text.c_str(),text.size()+1);CL_RTCWLimboChat(&vm,ptr(13));};
  auto get=[&](intptr_t index){memset(vm.bytes+799,0x55,142);bool ok=CL_RTCWGetLimboString(&vm,index,ptr(800))!=0;
   assert(vm.bytes[799]==0x55&&vm.bytes[940]==0x55);return ok;};
  for(int i=0;i<7;++i){assert(get(i));for(int c=800;c<940;++c)assert(vm.bytes[c]==0);}
  submit("hello ^1red");assert(get(0)&&!strcmp(reinterpret_cast<char *>(vm.bytes+800),"hello ^1red"));
  submit("");assert(get(0)&&vm.bytes[800]==0);assert(get(1)&&vm.bytes[800]=='h');
  for(int i=0;i<9;++i)submit(std::to_string(i));
  for(int i=0;i<7;++i)assert(get(i)&&vm.bytes[800]=='8'-i&&vm.bytes[801]==0);
  submit(std::string(200,'x'));assert(get(0));for(int c=800;c<940;++c)assert(vm.bytes[c]=='x');
  submit("^1"+std::string(140,'x')+"ignored");assert(strlen(cl.rtcwLimboChat.lines[0])==142);
  submit("^^1a^");assert(get(0)&&!strcmp(reinterpret_cast<char *>(vm.bytes+800),"^^1a^"));
  std::string colors;for(int i=0;i<300;++i)colors+="^1";submit(colors);
  assert(strlen(cl.rtcwLimboChat.lines[0])==420&&get(0));
  const auto saved=cl.rtcwLimboChat;CL_RTCWLimboChat(&vm,0);assert(!memcmp(&saved,&cl.rtcwLimboChat,sizeof(saved)));
  for(intptr_t index:{intptr_t(-1),intptr_t(7),INTPTR_MIN,INTPTR_MAX}){
   assert(CL_RTCWGetLimboString(nullptr,index,0)==qfalse);assert(!get(index));for(int c=800;c<940;++c)assert(vm.bytes[c]==0x55);
  }
  Reject([&]{CL_RTCWGetLimboString(&vm,0,0);});
  Reject([&]{CL_RTCWGetLimboString(&vm,0,static_cast<intptr_t>(UINTPTR_MAX-3));});
  Reject([&]{CL_RTCWLimboChat(&vm,static_cast<intptr_t>(UINTPTR_MAX));});
  Reject([&]{CL_RTCWLimboChat(nullptr,ptr(13));});
  if(!native){
   Reject([&]{CL_RTCWGetLimboString(&vm,0,885);});
   vm.bytes[1023]='^';Reject([&]{CL_RTCWLimboChat(&vm,1023);});
  }
  multiplayer=false;Reject([&]{CL_RTCWGetLimboString(&vm,0,ptr(800));});Reject([&]{CL_RTCWLimboChat(&vm,0);});multiplayer=true;
 }
 cl.rtcwLimboChat={}; // Same POD reset used when the active map state is cleared.
 for(const auto &line:cl.rtcwLimboChat.lines)for(char ch:line)assert(ch==0);
}
'''


class RTCWLimboChatTests(unittest.TestCase):
    def test_history_colors_and_native_qvm_bounds(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        source = (ROOT / 'code/client/cl_main.cpp').read_text()
        body = function(source, 'void CL_RTCWLimboChat(') + '\n' + function(source, 'qboolean CL_RTCWGetLimboString(')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-limbo-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(PRELUDE + body + CHECKS, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
