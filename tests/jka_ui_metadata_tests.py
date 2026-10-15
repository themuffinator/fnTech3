"""Retail-shaped UI diagnostic/key catcher calls, with no platform input I/O."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
from compiler_support import engine_fixture_args
from rtcw_server_world_tests import function
ROOT=Path(__file__).resolve().parents[1]


def fixture():
    host=(ROOT/'code/client/client_jka_ui_host.hpp').read_text()
    parser=(ROOT/'code/botlib/l_precomp.cpp').read_text()
    keys=(ROOT/'code/client/cl_keys.cpp').read_text()
    vm=(ROOT/'code/qcommon/vm.c').read_text()
    strings=(ROOT/'code/qcommon/q_shared.c').read_text()
    start=host.index('\tcase Call::UI_PC_SOURCE_FILE_AND_LINE:')
    end=host.index('\tcase Call::UI_LANGUAGE_ISASIAN:',start)
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/botlib/botlib.h"
#include "code/botlib/l_script.h"
#include "code/botlib/l_precomp.h"
#include "code/client/input_compat.hpp"
#include "code/compat/jka_module_abi.h"
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <limits>
struct Drop{};void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
static vm_t *uivm;static botlib_export_t *botlib_export;
static source_t *sourceFiles[64];
static constexpr int MAX_SOURCEFILES=64;
static int keyCatchers,clears;
void Key_ClearStates(){++clears;}
''' + function(parser,'int PC_SourceFileAndLine(')+'\n'+function(keys,'int Key_GetCatcher(')+'\n'+function(keys,'void Key_SetCatcher(')+'\n'+function(strings,'void Q_strncpyz(')+'\n'+function(vm,'void *VM_ExplicitArgPtr(')+'\n'+function(vm,'void VM_CheckBounds(')+'\n'+function(host,'static void *CL_JKAUINativeSpan(')+r'''
using Call=fntech3::jka::module::Call;
static bool Invoke(intptr_t *args,intptr_t &result){result=0;switch(fntech3::jka::module::Lookup(COM_GAME_JKA_MP,fntech3::jka::module::Module::UI,fntech3::jka::module::Direction::IMPORT,args[0])){
''' + host[start:end] + r'''
default:assert(false);return false;}}
template<class F>static void Reject(F fn){bool rejected=false;try{fn();}catch(Drop){rejected=true;}assert(rejected);}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);uivm=&vm;
 botlib_export_t api{};api.PC_SourceFileAndLine=PC_SourceFileAndLine;botlib_export=&api;
 source_t source{};script_t script{};source.scriptstack=&script;script.line=186;
 std::strcpy(source.filename,"ui/jamp/main.menu");sourceFiles[1]=&source;
 std::array<byte,140> name;name.fill(0xa5);std::array<byte,9> line;line.fill(0xa5);
 intptr_t args[]{70,1,reinterpret_cast<intptr_t>(name.data()+3),reinterpret_cast<intptr_t>(line.data()+2)},result=-1;
 assert(Invoke(args,result)&&result==1);assert(!std::strcmp(reinterpret_cast<char *>(name.data()+3),source.filename));
 std::int32_t externalLine=-1;std::memcpy(&externalLine,line.data()+2,4);assert(externalLine==186);
 assert(name[2]==0xa5&&name[131]==0xa5&&line[1]==0xa5&&line[6]==0xa5);
 const auto oldName=name;const auto oldLine=line;args[1]=64;assert(Invoke(args,result)&&result==0&&name==oldName&&line==oldLine);
 args[1]=-1;Reject([&]{Invoke(args,result);});assert(name==oldName&&line==oldLine);args[1]=1;
 args[2]=0;Reject([&]{Invoke(args,result);});args[2]=reinterpret_cast<intptr_t>(name.data()+3);args[3]=-1;Reject([&]{Invoke(args,result);});
 args[3]=reinterpret_cast<intptr_t>(line.data()+2);script.line=-1;Reject([&]{Invoke(args,result);});assert(name==oldName&&line==oldLine);script.line=186;
 std::memset(source.filename,'x',128);source.filename[128]=0;Reject([&]{Invoke(args,result);});assert(name==oldName&&line==oldLine);
 vm.entryPoint=nullptr;Reject([&]{Invoke(args,result);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 botlib_export=nullptr;Reject([&]{Invoke(args,result);});botlib_export=&api;
 // Internal browser/mouse-pass flags are not exposed to Raven. Setting
 // supported flags preserves the engine console/browser and normal anti-stuck cleanup.
 keyCatchers=KEYCATCH_CONSOLE|KEYCATCH_BROWSER|KEYCATCH_RETAIL_MOUSEPASS;
 intptr_t get[]{41};assert(Invoke(get,result)&&result==1);
 intptr_t set[]{42,KEYCATCH_UI};assert(Invoke(set,result)&&result==0);
 assert(keyCatchers==(KEYCATCH_UI|KEYCATCH_CONSOLE|KEYCATCH_BROWSER)&&clears==1);
 assert(Invoke(get,result)&&result==3);assert(Invoke(set,result)&&clears==1);
 for(int value:{-1,16,INT_MAX}){set[1]=value;Reject([&]{Invoke(set,result);});assert(clears==1);}
 set[1]=0;vm.entryPoint=nullptr;Reject([&]{Invoke(get,result);});Reject([&]{Invoke(set,result);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.dllExports=reinterpret_cast<void *>(1);
 Reject([&]{Invoke(get,result);});Reject([&]{Invoke(set,result);});assert(clears==1);
}
'''


class UIEngineMetadataTests(unittest.TestCase):
    def test_real_diagnostics_and_key_catcher_metadata(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-ui-metadata';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory=Path(folder);source=directory/'host.cpp';executable=directory/('host.exe' if os.name=='nt' else 'host')
            source.write_text(fixture())
            built=subprocess.run([compiler,*engine_fixture_args(compiler),'-std=c++17','-O2','-Werror','-D_CRT_SECURE_NO_WARNINGS','-I',str(ROOT),str(source),'-o',str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=120)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

if __name__=='__main__':unittest.main()
