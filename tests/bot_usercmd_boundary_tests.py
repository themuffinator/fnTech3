"""Execute the server bot-command dispatch with bounded Q3/QL argument spans."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class BotUsercmdBoundaryTests(unittest.TestCase):
    def test_layout_selection_and_rejection_before_client_think(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler required')
        source = (ROOT / 'code/server/sv_game.cpp').read_text()
        start = source.index('\tcase BOTLIB_USER_COMMAND:')
        branch = source[start:source.index('\tcase BOTLIB_AAS_BBOX_AREAS:', start)]
        harness = r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
enum {BOTLIB_USER_COMMAND, ERR_DROP};
struct fntech3_q3_usercmd_t {unsigned char bytes[24];};
struct fntech3_ql_usercmd_t {unsigned char bytes[28];};
struct usercmd_t {int converted;};
struct VM {bool dllExports;} vm, *gvm=&vm;
int calls=0, conversions=0, client=0;
std::size_t available=0, checked=0;
bool SV_IsClientIndex(int n){return n==1;}
int &SV_ClientForIndex(int n){assert(n==1);return client;}
void SV_ClientThink(int*,usercmd_t *cmd){++calls;client=cmd->converted;}
void FnTech3_InternalUsercmdFromQL(usercmd_t *d,void*){++conversions;d->converted=28;}
void FnTech3_InternalUsercmdFromQ3(usercmd_t *d,void*){++conversions;d->converted=24;}
void bounds(std::size_t size){checked=size;if(size>available)throw 1;}
void Com_Error(int,const char*){throw 2;}
#define VM_CHECKBOUNDS(vm,ptr,size) bounds(size)
#define VMA(n) reinterpret_cast<void*>(args[n])
int dispatch(intptr_t *args){switch(args[0]){
''' + branch + r'''
default:return -1;}}
int main(){
 unsigned char payload[28]{};
 intptr_t args[]={BOTLIB_USER_COMMAND,1,reinterpret_cast<intptr_t>(payload)};
 for(bool structured:{false,true}) {
  vm.dllExports=structured;available=structured?28:24;checked=0;
  int oldCalls=calls, oldConversions=conversions;
  assert(dispatch(args)==0 && calls==oldCalls+1 && conversions==oldConversions+1);
  assert(checked==available && client==static_cast<int>(available));
  --available;
  try{dispatch(args);assert(false);}catch(int error){assert(error==1);}
  assert(calls==oldCalls+1 && conversions==oldConversions+1);
  ++available;args[2]=0;
  try{dispatch(args);assert(false);}catch(int error){assert(error==2);}
  assert(calls==oldCalls+1 && conversions==oldConversions+1);
  args[1]=-1;available=0;checked=0;
  assert(dispatch(args)==0 && calls==oldCalls+1 && checked==0);
  args[1]=1;args[2]=reinterpret_cast<intptr_t>(payload);
 }
}
'''
        harness = '#include <initializer_list>\n' + harness
        with tempfile.TemporaryDirectory(prefix='fntech3-bot-command-') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / 'probe.exe'
            cpp.write_text(harness)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
