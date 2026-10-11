"""Execute both Jedi cgames' partial-init and error-unload handling in real shutdown."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT=Path(__file__).resolve().parents[1]


class JKACgameLifecycleTests(unittest.TestCase):
    def test_partial_init_error_and_normal_shutdown(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        production=(ROOT/'code/client/cl_cgame.cpp').read_text()
        source=r'''
#include <cassert>
#include <csetjmp>
#include <cstring>
#include <initializer_list>
enum {qfalse=0,CG_SHUTDOWN=1,H_CGAME=2,KEYCATCH_CGAME=8};
struct VM {};static VM module;static auto *cgvm=&module;
static bool jka,jk2,cl_jkaCgameInitialized,cl_jk2CgameInitialized,com_errorEntered;
static int shutdowns,frees,clears,closes,catcher,stringClears;
static bool poisonShutdown;
static char errorText[128];
static std::jmp_buf recovery;
static bool Com_GameIsJKAMP(){return jka;}
static bool Com_GameIsJK2MP(){return jk2;}
static void CL_JK2StringReset(unsigned owner){assert(owner==1);++stringClears;}
static bool Com_GameIsQ3Test(){return false;}
static int Key_GetCatcher(){return catcher;}
static void Key_SetCatcher(int value){catcher=value;}
static struct {int cgameStarted;} cls;
static void CL_ClearEnemyHighlightState(){}
static void CL_ClearLiquidInteractionState(){}
static void CL_ClearRTCWCameras(){}
static void CL_ClearWolfETCameras(){}
static void CL_HudResetCGame(){}
static void CL_EntityOverrideCGameShutdown(){}
static void CL_Q3TestShutdownCGame(){assert(false);}
static bool Com_GameIsQuake3IHV(){return false;}
static void CL_Q3IHVShutdownCGame(){assert(false);}
static void CL_JKACgameClear(){++clears;}
static void FS_VM_CloseFiles(int value){assert(value==H_CGAME);++closes;}
static void VertexLighting(int value){assert(!value);}
static struct {decltype(&VertexLighting) VertexLighting;} re{VertexLighting};
static void VM_Call(VM *value,int count,int ordinal){
 assert(value==&module&&!count&&ordinal==CG_SHUTDOWN);
 assert(!cl_jkaCgameInitialized&&!cl_jk2CgameInitialized&&!clears&&!stringClears);++shutdowns;
 if(poisonShutdown)std::memcpy(errorText,"Unsupported jk2-mp cgame import264",sizeof("Unsupported jk2-mp cgame import264"));
}
static void VM_Free(VM *value){assert(value==&module);++frees;}
''' + function(production,'void CL_ShutdownCGame(') + r'''
static void Reset(bool game,bool initialized,bool error){
 jka=game;jk2=false;cl_jkaCgameInitialized=initialized;cl_jk2CgameInitialized=false;com_errorEntered=error;
 cgvm=&module;shutdowns=frees=clears=closes=stringClears=0;catcher=15;cls.cgameStarted=1;
 poisonShutdown=false;errorText[0]=0;
}
static void ResetOutcast(bool initialized,bool error){
 Reset(false,false,error);jk2=true;cl_jk2CgameInitialized=initialized;
}
static void AbortExport(){
 std::memcpy(errorText,"original cgame initialization failure",sizeof("original cgame initialization failure"));
 com_errorEntered=true;CL_ShutdownCGame();std::longjmp(recovery,1);
}
int main(){
 Reset(true,false,false);CL_ShutdownCGame();
 assert(!shutdowns&&frees==1&&clears==1&&closes==1&&!cgvm&&!cls.cgameStarted&&catcher==7);
 Reset(true,true,true);CL_ShutdownCGame();assert(!shutdowns&&frees==1&&clears==1&&closes==1);
 Reset(true,true,false);CL_ShutdownCGame();assert(shutdowns==1&&frees==1&&clears==1&&closes==1);
 CL_ShutdownCGame();assert(shutdowns==1&&frees==1&&clears==2&&closes==2);
 Reset(false,false,true);CL_ShutdownCGame();assert(shutdowns==1&&frees==1&&!clears&&closes==1);
 Reset(true,true,false);cgvm=nullptr;CL_ShutdownCGame();assert(!shutdowns&&!frees&&clears==1&&closes==1);
 // Outcast's QVM retains its image/files/strings after a partial init, but
 // CG_SHUTDOWN may call an unrelated unserved service and mask the cause.
 ResetOutcast(false,false);CL_ShutdownCGame();
 assert(!shutdowns&&frees==1&&!clears&&closes==1&&stringClears==1&&!cgvm&&!cls.cgameStarted&&catcher==7);
 ResetOutcast(true,true);CL_ShutdownCGame();assert(!shutdowns&&frees==1&&closes==1&&stringClears==1);
 ResetOutcast(true,false);CL_ShutdownCGame();assert(shutdowns==1&&frees==1&&closes==1&&stringClears==1);
 CL_ShutdownCGame();assert(shutdowns==1&&frees==1&&closes==2&&stringClears==2);
 ResetOutcast(false,false);cgvm=nullptr;CL_ShutdownCGame();assert(!shutdowns&&!frees&&closes==1&&stringClears==1);
 // Emulate the engine's error cleanup followed by longjmp. A shutdown
 // reentry would replace the original diagnostic with import264 here.
 for(bool initialized:{false,true}){
  ResetOutcast(initialized,false);poisonShutdown=true;
  if(!setjmp(recovery)){AbortExport();assert(false);}
  assert(!std::strcmp(errorText,"original cgame initialization failure"));
  assert(!shutdowns&&frees==1&&closes==1&&stringClears==1&&!cgvm&&!cl_jk2CgameInitialized);
 }
}
'''
        scratch=ROOT/'.tmp/tests/jka-cgame-lifecycle'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            path=Path(folder)/'lifecycle.cpp';path.write_text(source)
            executable=Path(folder)/('lifecycle.exe' if os.name=='nt' else 'lifecycle')
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-Wall',
                '-Werror',str(path),'-o',str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=120)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        init=function(production,'void CL_InitCGame(')
        self.assertLess(init.index('cl_jkaCgameInitialized = false;'),init.index('CL_LoadCGameVM('))
        self.assertGreater(init.index('cl_jkaCgameInitialized = Com_GameIsJKAMP();'),
            init.index('VM_Call( cgvm, 3, CG_INIT'))
        self.assertLess(init.index('cl_jk2CgameInitialized = false;'),init.index('CL_LoadCGameVM('))
        self.assertGreater(init.index('cl_jk2CgameInitialized = Com_GameIsJK2MP();'),
            init.index('VM_Call( cgvm, 3, CG_INIT'))


if __name__=='__main__':unittest.main()
