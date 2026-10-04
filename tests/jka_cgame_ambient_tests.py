"""Run Academy ambient imports against bounded definitions and real cgame cleanup."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
TASK=ROOT/'.tmp/tests/jka-cgame-ambient'
from compiler_support import compiler_target_args
from rtcw_music_fixture import function

def fixture():
    vm=(ROOT/'code/qcommon/vm.c').read_text()
    host=(ROOT/'code/client/client_jka_cgame_host.hpp').read_text()
    clear=function(host,'static void CL_JKACgameClear(')
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/client/client_jka_command_values.hpp"
#include "code/compat/jka_lightstyle.hpp"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/jka_module_abi.h"
#include <array>
#include <map>
#include <limits>
#include <vector>
#include <string>
#include <cassert>
static void CL_JKACgameClear();
struct Drop{};static bool recover;
void QDECL Com_Error(errorParm_t,const char *,...){if(recover)CL_JKACgameClear();throw Drop{};}
static vm_t *cgvm;
''' + function(vm,'void *VM_ExplicitArgPtr(')+'\n'+function(vm,'void VM_CheckBounds(')+'\n'+ \
        function(host,'static void *CL_JKACgameSpan(') + r'''
static const std::string source=
 "type ambientSet\n"
 "generalSet shared\nloopedWave wind/hum\nsubWaves weather rain\n"
 "localSet local\nradius 500\nsubWaves zones one two\n"
 "bmodelSet shared\nsubWaves doors open close\n";
static std::string data=source;
static int opens,reads,closes,nextFile=1,overrideLength=-1,parsedMessages;
void QDECL Com_Printf(const char *format,...){assert(std::strstr(format,"ambient definitions parsed"));++parsedMessages;}
static bool missing,noHandle,shortRead,openDrop,readDrop,closeDrop,silent,soundDrop;
static std::map<int,int> files;
static std::vector<std::string> registered;
int FS_FOpenFileRead(const char *name,fileHandle_t *handle,qboolean unique){
 assert(!std::strcmp(name,"sound/sound.txt")&&!unique);++opens;*handle=0;
 if(missing)return -1;if(!noHandle){*handle=nextFile++;files[*handle]=1;}
 if(openDrop)Com_Error(ERR_DROP,"open failure");
 return overrideLength<0?static_cast<int>(data.size()):overrideLength;
}
int FS_Read(void *output,int length,fileHandle_t file){
 ++reads;assert(files.count(file)&&length==static_cast<int>(data.size()));
 if(readDrop)Com_Error(ERR_DROP,"read failure");
 if(shortRead)--length;std::memcpy(output,data.data(),length);return length;
}
void FS_FCloseFile(fileHandle_t file){
 assert(files.erase(file)==1);++closes;if(closeDrop)Com_Error(ERR_DROP,"close failure");
}
sfxHandle_t S_RegisterSound(const char *name,qboolean compression){
 assert(!compression);registered.emplace_back(name);
 if(soundDrop)Com_Error(ERR_DROP,"sound failure");
 return silent?0:static_cast<sfxHandle_t>(100+registered.size());
}
#include "code/client/client_jka_cgame_ambient.hpp"
static fntech3::jka::lightstyle::Store jkaCgameLightStyles;
static void *jkaCgameSharedBuffer;static std::vector<int> jkaFonts;
static int modelResets,stringResets;
static void CL_JKACgameGhoul2Clear(){++modelResets;}
static void CL_JKACgameEffectsClear(){}
static void CL_JKAStringReset(){++stringResets;}
void S_ClearJKAMusicPreload(){}
void S_JKASetSoundRegistrationQuiet(qboolean){}
''' + clear + r'''
static intptr_t Invoke(intptr_t *args){
 intptr_t result=57;
 const auto call=fntech3::jka::module::Lookup(COM_GAME_JKA_MP,
  fntech3::jka::module::Module::CGAME,fntech3::jka::module::Direction::IMPORT,args[0]);
 assert(CL_JKACgameAmbientCall(args,call,result));return result;
}
template<class F>static void Reject(F action){bool rejected=false;try{action();}catch(Drop){rejected=true;}assert(rejected);}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 intptr_t add[]{50,reinterpret_cast<intptr_t>("shared")},parse[]{49};
 intptr_t get[]{52,reinterpret_cast<intptr_t>("shared"),0};
 assert(Invoke(get)==-1&&!jkaCgameAmbient&&opens==0);
 assert(!Invoke(add)&&!Invoke(add)&&jkaCgameAmbient->catalog.SelectedCount()==1);
 add[1]=reinterpret_cast<intptr_t>("local");assert(!Invoke(add));
 assert(!Invoke(parse)&&opens==1&&reads==1&&closes==1&&files.empty()&&parsedMessages==1);
 assert(jkaCgameAmbient->catalog.Definitions().size()==3&&registered.size()==6);
 // Missing/null/empty names return before touching any listener vector.
 // A present declaration remains a named active-playback refusal, even with
 // sound initialization disabled; absence cannot stand in for active audio.
 const int registeredBeforeUpdate=static_cast<int>(registered.size());
 intptr_t update[]{48,reinterpret_cast<intptr_t>("missing"),-1};
 assert(Invoke(update)==0);update[1]=0;assert(Invoke(update)==0);
 update[1]=reinterpret_cast<intptr_t>("");assert(Invoke(update)==0);
 update[1]=reinterpret_cast<intptr_t>("SHARED");assert(Invoke(update)==0);
 update[1]=reinterpret_cast<intptr_t>("shared");Reject([&]{Invoke(update);});
 assert(static_cast<int>(registered.size())==registeredBeforeUpdate&&files.empty());

 assert(registered[0]=="sound/wind/hum.wav"&&registered[5]=="sound/doors/close.wav");
 assert(Invoke(get)==105);get[2]=1;assert(Invoke(get)==106);
 for(int index:{-1,2,8,INT_MAX}){get[2]=index;assert(Invoke(get)==-1);}get[2]=0;
 get[1]=0;assert(Invoke(get)==-1);get[1]=reinterpret_cast<intptr_t>("missing");assert(Invoke(get)==-1);
 get[1]=reinterpret_cast<intptr_t>("local");assert(Invoke(get)==103);get[2]=1;assert(Invoke(get)==104);
 get[1]=reinterpret_cast<intptr_t>("LOCAL");assert(Invoke(get)==-1);
 add[1]=reinterpret_cast<intptr_t>("#CLEAR");assert(!Invoke(add));
 get[1]=reinterpret_cast<intptr_t>("shared");get[2]=0;assert(Invoke(get)==105);
 assert(!Invoke(parse)&&registered.size()==6&&jkaCgameAmbient->sounds.empty());
 assert(Invoke(get)==-1);silent=true;add[1]=reinterpret_cast<intptr_t>("shared");assert(!Invoke(add));
 assert(!Invoke(parse)&&registered.size()==10&&Invoke(get)==0);silent=false;
 // All mutable state resets through the real cgame owner lifecycle.
 jkaCgameSharedBuffer=&vm;jkaFonts.push_back(1);int beforeModel=modelResets,beforeString=stringResets;
 fntech3::jka::command::selections.Set(255,257);
 CL_JKACgameClear();
 { usercmd_t cleared{};cleared.raven.generic_cmd=77;
   fntech3::jka::command::selections.Publish(cleared);
   assert(!cleared.raven.forcesel&&!cleared.raven.invensel&&cleared.raven.generic_cmd==77); }
assert(!jkaCgameAmbient&&!jkaCgameSharedBuffer&&jkaFonts.empty());
 assert(modelResets==beforeModel+1&&stringResets==beforeString+1);
 assert(Invoke(get)==-1&&!Invoke(add));
 std::array<char,64> longName;longName.fill('x');
 add[1]=reinterpret_cast<intptr_t>(longName.data());Reject([&]{Invoke(add);});
 add[1]=reinterpret_cast<intptr_t>("has space");Reject([&]{Invoke(add);});
 add[1]=0;Reject([&]{Invoke(add);});add[1]=-1;Reject([&]{Invoke(add);});
 vm.entryPoint=nullptr;std::array<byte,256> qvm{};vm.dataBase=qvm.data();vm.dataMask=255;
 add[1]=250;std::memset(qvm.data()+250,'x',6);Reject([&]{Invoke(add);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);add[1]=reinterpret_cast<intptr_t>("shared");
 recover=true;
 const auto reset=[&]{missing=noHandle=shortRead=openDrop=readDrop=closeDrop=soundDrop=false;overrideLength=-1;data=source;assert(files.empty());};
 for(int failure=0;failure<9;++failure){
  reset();assert(!Invoke(add));
  if(failure==0)missing=true;
  if(failure==1)noHandle=true;
  if(failure==2)overrideLength=0;
  if(failure==3)overrideLength=INT_MAX;
  if(failure==4)shortRead=true;
  if(failure==5)openDrop=true;
  if(failure==6)readDrop=true;
  if(failure==7)closeDrop=true;
  if(failure==8)soundDrop=true;
  Reject([&]{Invoke(parse);});assert(!jkaCgameAmbient&&files.empty());
 }
 reset();assert(!Invoke(add));data="type ambientset\ngeneralSet other\n";
 Reject([&]{Invoke(parse);});assert(!jkaCgameAmbient&&files.empty());reset();
 const int registeredBefore=static_cast<int>(registered.size());assert(!Invoke(add));
 data="type ambientset\nbmodelSet shared\nsubWaves ../outside bad\n";
 Reject([&]{Invoke(parse);});assert(!jkaCgameAmbient&&files.empty()&&static_cast<int>(registered.size())==registeredBefore);reset();
 intptr_t absent[]{48,reinterpret_cast<intptr_t>("missing"),-1};
 Reject([&]{Invoke(absent);}); // A nonempty name needs a loaded catalog.
 absent[1]=0;assert(Invoke(absent)==0&&!jkaCgameAmbient);
 std::array<char,64> invalidName;invalidName.fill('x');
 absent[1]=reinterpret_cast<intptr_t>(invalidName.data());Reject([&]{Invoke(absent);});
 intptr_t unserved[]{51};intptr_t result=29;
 assert(!CL_JKACgameAmbientCall(unserved,fntech3::jka::module::Call::CG_S_ADDLOCALSET,result)&&result==29);
 assert(parsedMessages==3);recover=false;CL_JKACgameClear();assert(files.empty());
}
'''

class JKACgameAmbientTests(unittest.TestCase):
    def test_native_selection_registration_and_recovery(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        self.assertTrue(TASK.resolve().is_relative_to(ROOT.resolve()))
        TASK.mkdir(parents=True,exist_ok=True)
        output=Path(tempfile.mkdtemp(dir=TASK,prefix='ambient-host-')).absolute()
        try:
            src=output/'ambient-host.cpp';exe=output/('ambient-host.exe' if os.name=='nt' else 'ambient-host')
            src.write_text(fixture())
            target=compiler_target_args(compiler)
            if os.name=='nt' and not target:target=['-DFNTECH3_Q3_ONLY']
            for fast in (False,True):
                with self.subTest(fast_math=fast):
                    command=[compiler,*target,'-std=c++17','-O2','-Werror','-D_CRT_SECURE_NO_WARNINGS',
                             *(['-ffast-math'] if fast else []),'-I',str(ROOT),str(src),'-o',str(exe)]
                    built=subprocess.run(command,cwd=ROOT,text=True,capture_output=True,timeout=90)
                    self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                    ran=subprocess.run([str(exe)],cwd=ROOT,text=True,capture_output=True,timeout=20)
                    self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)
        finally:
            self.assertEqual(output.resolve(),output)
            self.assertTrue(output.is_relative_to(TASK.absolute()))
            for directory,directories,names in os.walk(output,followlinks=False):
                for path in [Path(directory),*(Path(directory)/name for name in directories+names)]:
                    self.assertFalse(path.is_symlink() or getattr(path.stat(),'st_file_attributes',0)&0x400)
            shutil.rmtree(output)

if __name__=='__main__':unittest.main()
