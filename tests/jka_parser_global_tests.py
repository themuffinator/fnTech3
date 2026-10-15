"""Exercise JKA definition loading with the real botlib lexer/precompiler."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import engine_fixture_args
from rtcw_server_world_tests import function

ROOT=Path(__file__).resolve().parents[1]
SOURCES=('code/botlib/l_precomp.cpp','code/botlib/l_script.cpp','code/botlib/l_memory.cpp')


def parser_fixture():
    base=(ROOT/'tests/fixtures/precompiler_host.cpp').read_text()
    base=base[:base.index('int main( int argc, char **argv )')]
    base=base.replace('#include <vector>','#include <vector>\n#include <cassert>\n#include <array>\n#include <iostream>')
    base=base.replace('static int errors, warnings;', 'static int errors, warnings, allocations;\nstatic bool checkReentry;\nstatic std::string imported;\nstatic std::size_t importedPosition;\nstatic fileHandle_t shortHandle;')
    base=base.replace('char text[4096];','if(checkReentry) assert(!PC_LoadGlobalDefines("valid.h"));\n\tchar text[4096];')
    base=base.replace('return std::calloc( 1, (size_t)size );','++allocations; return std::calloc( 1, (size_t)size );')
    base=base.replace('std::free( ptr );','--allocations; std::free( ptr );')
    base=base.replace('std::string path = root + "/" + qpath;', 'if(!std::strcmp(qpath,"retail.h")){*handle=12345;importedPosition=0;return (int)imported.size();}\n\tstd::string path = root + "/" + qpath;')
    base=base.replace('return (int)length;', 'if(!std::strcmp(qpath,"negative.h"))return -2;\n\tif(!std::strcmp(qpath,"short.h"))shortHandle=*handle;\n\treturn (int)length;')
    base=base.replace('return (int)std::fread( buffer, 1, (size_t)len, files[handle] );', 'if(handle==shortHandle){shortHandle=0;--len;}\n\treturn (int)std::fread( buffer, 1, (size_t)len, files[handle] );')
    base=base.replace('return (int)std::fread( buffer, 1, (size_t)len, files[handle] );', 'if(handle==12345){const auto available=imported.size()-importedPosition;const auto count=available<(size_t)len?available:(size_t)len;std::memcpy(buffer,imported.data()+importedPosition,count);importedPosition+=count;return (int)count;}\n\treturn (int)std::fread( buffer, 1, (size_t)len, files[handle] );')
    base=base.replace('if ( handle > 0 &&', 'if(handle==12345)return;\n\tif ( handle > 0 &&')
    return base+r'''
static std::vector<std::string> Read(int handle){
 assert(handle);pc_token_t token{};std::vector<std::string> result;
 while(PC_ReadTokenHandle(handle,&token))result.emplace_back(token.string);
 assert(PC_FreeSourceHandle(handle));return result;
}
static void Check(const char *file,std::initializer_list<const char *> expected){
 auto actual=Read(PC_LoadSourceHandle(file));std::vector<std::string> wanted;
 for(auto *token:expected)wanted.emplace_back(token);assert(actual==wanted);
}
int main(int argc,char **argv){
 assert(argc>=2);root=argv[1];
 botimport.Print=HostPrint;botimport.GetMemory=HostGetMemory;botimport.FreeMemory=HostFreeMemory;
 botimport.AvailableMemory=HostAvailableMemory;botimport.HunkAlloc=HostHunkAlloc;
 botimport.FS_FOpenFile=HostOpen;botimport.FS_Read=HostRead;botimport.FS_FCloseFile=HostClose;
 botimport.Sys_Milliseconds=HostMilliseconds;
 if(argc>2){
   std::getline(std::cin,imported,'\0');assert(PC_LoadGlobalDefines("retail.h"));
   Check("retail.menu",{"1","2","3"});PC_RemoveAllGlobalDefines();
   assert(allocations==0);std::cout<<"retail global definitions passed\n";return 0;
 }
 assert(PC_AddGlobalDefine("EXISTING 7"));int before=PC_LoadSourceHandle("before.menu");assert(before);
 auto jka=Read(PC_LoadJKASourceHandle("jka.menu"));assert(jka==std::vector<std::string>({"@MENUS_MULTIPLAYER","@MENUS_INCLUDED","after"}));
 assert(Read(PC_LoadSourceHandle("jka.menu")).empty());assert(Read(PC_LoadWolfETSourceHandle("jka.menu")).empty());
 assert(PC_LoadGlobalDefines("valid.h"));assert(Read(before)==std::vector<std::string>({"VALUE","7"}));
 Check("future.menu",{"42","7","9","[","1","+","2","]","REMOVED"});
 int copied=PC_LoadSourceHandle("future.menu");assert(copied);
 // Loading does not consume a source-handle slot, even with all slots occupied.
 std::array<int,62> handles{};for(auto &handle:handles){handle=PC_LoadSourceHandle("future.menu");assert(handle);}
 assert(PC_LoadGlobalDefines("replacement.h"));for(int handle:handles)assert(PC_FreeSourceHandle(handle));
 Check("before.menu",{"43","7"});
 for(auto *file:{"missing.h","bad.h","unterminated.h","endif.h","parameter.h","merge.h","pragma.h","line.h","include.h","recursive.h","large.h","cycle.h","self.h","truncated.h","negative.h","short.h","depth0.h","../valid.h","/valid.h","C:/valid.h","path.h"}){
   checkReentry=true;assert(!PC_LoadGlobalDefines(file));checkReentry=false;Check("rollback.menu",{"LEAK","7","43"});
 }
 assert(!PC_LoadGlobalDefines(nullptr)&&!PC_LoadGlobalDefines(""));
 std::array<char,MAX_QPATH+1> longName;longName.fill('a');longName.back()=0;assert(!PC_LoadGlobalDefines(longName.data()));
 PC_RemoveAllGlobalDefines();Check("before.menu",{"VALUE","EXISTING"});
 assert(Read(copied)==std::vector<std::string>({"42","7","9","[","1","+","2","]","REMOVED"}));
 PC_RemoveAllGlobalDefines();assert(allocations==0);for(auto *file:files)assert(!file);
 std::cout<<"global definitions passed\n";return 0;
}
'''


def host_fixture():
    host=(ROOT/'code/client/client_jka_ui_host.hpp').read_text()
    vm=(ROOT/'code/qcommon/vm.c').read_text()
    start=host.index('\tcase Call::UI_PC_LOAD_SOURCE:')
    end=host.index('\tcase Call::UI_PC_SOURCE_FILE_AND_LINE:',start)
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/botlib/botlib.h"
#include "code/compat/jka_module_abi.h"
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
struct Drop{};void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
static vm_t *uivm;static botlib_export_t *botlib_export;
static int loads,clears;static const void *external;
static int Load(const char *name){assert(name!=external);assert(!std::strcmp(name,"ui/jamp/menudef.h"));++loads;return 7;}
static void Clear(){++clears;}
''' + function(vm,'void *VM_ExplicitArgPtr(')+'\n'+function(vm,'void VM_CheckBounds(')+'\n'+function(host,'static void *CL_JKAUINativeSpan(')+r'''
using Call=fntech3::jka::module::Call;
static bool Invoke(intptr_t *args,intptr_t &result){result=0;const auto call=fntech3::jka::module::Lookup(COM_GAME_JKA_MP,fntech3::jka::module::Module::UI,fntech3::jka::module::Direction::IMPORT,args[0]);switch(call){
'''+host[start:end]+r'''
default:assert(false);return false;}}
template<class F>static void Reject(F fn){bool rejected=false;try{fn();}catch(Drop){rejected=true;}assert(rejected);}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);uivm=&vm;
 botlib_export_t api{};api.PC_LoadGlobalDefines=Load;api.PC_LoadJKASourceHandle=Load;api.PC_RemoveAllGlobalDefines=Clear;botlib_export=&api;
 std::array<char,MAX_QPATH> path{};std::strcpy(path.data(),"ui/jamp/menudef.h");external=path.data();auto before=path;
 intptr_t args[]{71,reinterpret_cast<intptr_t>(path.data())},result=-1;
 assert(Invoke(args,result)&&result==7&&loads==1&&clears==0&&path==before);
 args[0]=67;assert(Invoke(args,result)&&result==7&&loads==2&&clears==0&&path==before);
 api.PC_LoadJKASourceHandle=nullptr;Reject([&]{Invoke(args,result);});api.PC_LoadJKASourceHandle=Load;args[0]=71;
 intptr_t remove[]{72};assert(Invoke(remove,result)&&result==0&&clears==1&&loads==2);
 path.fill('x');Reject([&]{Invoke(args,result);});assert(loads==2&&clears==1);
 args[1]=0;Reject([&]{Invoke(args,result);});args[1]=-1;Reject([&]{Invoke(args,result);});
 args[1]=reinterpret_cast<intptr_t>(external);vm.entryPoint=nullptr;Reject([&]{Invoke(args,result);});Reject([&]{Invoke(remove,result);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.dllExports=reinterpret_cast<void *>(1);Reject([&]{Invoke(remove,result);});
 vm.dllExports=nullptr;path=before;botlib_export=nullptr;Reject([&]{Invoke(args,result);});Reject([&]{Invoke(remove,result);});
 botlib_export=&api;api.PC_LoadGlobalDefines=nullptr;Reject([&]{Invoke(args,result);});api.PC_RemoveAllGlobalDefines=nullptr;Reject([&]{Invoke(remove,result);});
 assert(loads==2&&clears==1&&path==before);
}
'''


class GlobalParserTests(unittest.TestCase):
    def test_actual_host_signatures_bounds_and_callbacks(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-parser-global';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory=Path(folder);source=directory/'host.cpp';executable=directory/('host.exe' if os.name=='nt' else 'host')
            source.write_text(host_fixture())
            built=subprocess.run([compiler,*engine_fixture_args(compiler),'-std=c++17','-O2','-ffast-math','-Werror','-D_CRT_SECURE_NO_WARNINGS','-I',str(ROOT),str(source),'-o',str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=120)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_actual_parser_state_failure_and_cleanup(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-parser-global';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory=Path(folder);source=directory/'probe.cpp';executable=directory/('probe.exe' if os.name=='nt' else 'probe')
            source.write_text(parser_fixture())
            files={
                'valid.h':'#define VALUE 42\n#define F(a,b) [ a + b ]\n#ifdef EXISTING\n#include "extra.h"\n#endif\n#define REMOVED 1\n#undef REMOVED\n#if 0\n#define SKIPPED 2\n#endif\n',
                'jka.menu':'@MENUS_MULTIPLAYER\n#include "included.menu"\nafter\n','included.menu':'@MENUS_INCLUDED\n',
                'extra.h':'#define EXTRA 9\n','replacement.h':'#define VALUE 43\n',
                'before.menu':'VALUE EXISTING\n','future.menu':'VALUE EXISTING EXTRA F(1,2) REMOVED\n',
                'rollback.menu':'LEAK EXISTING VALUE\n','bad.h':'#define LEAK 1\n#unsupported x\n',
                'unterminated.h':'#define LEAK 1\n"unterminated','endif.h':'#define LEAK 1\n#ifdef VALUE\n',
                'parameter.h':'#define F(1) 2\n','merge.h':'#define F ## 2\n',
                'pragma.h':'#pragma once\n','line.h':'#line 9\n','include.h':'#include <extra.h\n',
                'recursive.h':'#include "recursive.h"\n','large.h':' '*( (1<<20)+1),
                'cycle.h':'#define A B\n#define B A\nA\n','truncated.h':'#define LEAK 1\n"'+'a'*1050+'"\n',
                'self.h':'#define SELF SELF\n',
                'negative.h':'#define LEAK 1\n','short.h':'#define LEAK 1\n',
                'path.h':'#include "../valid.h"\n',
            }
            files.update({f'depth{n}.h':f'#include "depth{n+1}.h"\n' for n in range(33)})
            files['depth33.h']='#define LEAK 1\n'
            for name,value in files.items():(directory/name).write_text(value)
            built=subprocess.run([compiler,*engine_fixture_args(compiler),'-std=c++17','-O2','-ffast-math','-Werror','-D_CRT_SECURE_NO_WARNINGS','-DBOTLIB','-I',str(ROOT),str(source),*[str(ROOT/path) for path in SOURCES],'-o',str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=120)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(executable),str(directory)],cwd=ROOT,capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('global definitions passed',run.stdout)

if __name__=='__main__':unittest.main()
