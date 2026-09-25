"""Execute menu restoration across a real client video-restart function.

The synthetic endpoints model module-owned geometry and invalidated handles;
no game, input device, or operating-system window is used.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include "code/client/window_resize.hpp"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
using qboolean=int;
constexpr int qfalse=0,qtrue=1;
enum { UIMENU_NONE,UIMENU_MAIN,UIMENU_INGAME,UIMENU_BRIEFING,
       UI_GET_ACTIVE_MENU,UI_SET_ACTIVE_MENU,KEYCATCH_UI=128,
       CA_DISCONNECTED=0,CA_CONNECTED=2,CA_ACTIVE=4,CA_CINEMATIC=5,CA_LOADING=6,CA_PRIMED=7,
       FS_UI_REF=1,FS_CGAME_REF=2 };
using refShutdownCode_t=int;
int vmStorage, *uivm=&vmStorage;
struct { int state; bool startCgame,cgameStarted,soundRegistered; } cls;
struct { bool demorecording,demoplaying; int checksumFeed; } clc;
int catcher,paused,menu,profile,queryCalls,registeredWidth,windowWidth=640;
int generation,menuGeneration,uiWidth,renderWidth;
std::vector<std::string> events;
bool failRenderer=false;
bool Com_GameIsRTCW(){return profile>=2;}
bool Com_GameIsAlice(){return false;}
bool Com_GameIsRitual(){return false;}
void S_AliceMusicFreeAll(){}
void S_AliceMusicSoundtrackAfterRestart(){}
int Key_GetCatcher(){return catcher;}
int VM_Call(int*,int count,int call,int arg=0) {
 if(call==UI_GET_ACTIVE_MENU) {assert(profile>=2&&count==0);++queryCalls;return menu;}
 assert(call==UI_SET_ACTIVE_MENU&&count==1);
 assert(uivm&&uiWidth==renderWidth&&menuGeneration==generation);
 menu=arg;catcher|=KEYCATCH_UI;events.push_back("reopen");return 0;
}
int Cvar_VariableIntegerValue(const char *name){assert(!strcmp(name,"cl_paused"));return paused;}
void Cvar_Set(const char *name,const char *value){assert(!strcmp(name,"cl_paused"));paused=std::stoi(value);}
bool CL_VideoRecording(){return false;}
void CL_CloseAVI(int){}
void CL_StopRecord_f(){}
void S_DisableSounds(){}
void CL_WebHost_NotifySoundsStopped(){}
void CL_ShutdownVMs(){uivm=nullptr;catcher&=~KEYCATCH_UI;events.push_back("shutdown");}
void CL_ShutdownRef(int){++generation;events.push_back("renderer shutdown");}
void CL_ResetPureClientAtServer(){}
void FS_ClearPakReferences(int){}
void FS_ConditionalRestart(int,int){}
void CL_ClearMemory(){}
void CL_StartHunkUsers(){
 if(failRenderer)throw 1;
 renderWidth=windowWidth;uiWidth=renderWidth;menuGeneration=generation;
 uivm=&vmStorage;menu=UIMENU_NONE;events.push_back("init ui");
}
void CL_InitCGame(){cls.state=CA_PRIMED;paused=1;events.push_back("init cgame");}
void CL_SendPureChecksums(){}
void CL_WebHost_InvalidateConfigSnapshot(){events.push_back("web refresh");}
static fnql::client::WindowResizeScheduler cl_windowResize;
static int cl_windowResizeRestart,cl_windowModeChange,cl_restartMenu,cl_restartMenuPaused;
'''

CHECKS = r'''
int main(){
 for(profile=0;profile<4;++profile) for(bool active:{false,true})
 for(bool visible:{false,true}) for(int width:{640,960,1280}) {
  cls={};cls.state=active?CA_ACTIVE:CA_DISCONNECTED;cls.cgameStarted=active;
  uivm=&vmStorage;catcher=visible?KEYCATCH_UI:0;
  menu=profile>=2?UIMENU_BRIEFING:(active?UIMENU_INGAME:UIMENU_MAIN);
  paused=active&&visible;int oldPaused=paused,oldMenu=visible?menu:UIMENU_NONE;
  windowWidth=width;queryCalls=0;events.clear();
  CL_Vid_Restart(1);
  if(active&&visible){assert(paused==0&&cl_restartMenu==oldMenu);cls.state=CA_ACTIVE;CL_FinishUIMenuRestart();}
  assert(menu==oldMenu&&uiWidth==width);
  if(visible)assert(paused==oldPaused);
  assert(queryCalls==(profile>=2&&visible?1:0));
  assert(bool(catcher&KEYCATCH_UI)==visible);
  assert(events.front()=="shutdown");
  if(active&&visible)assert(events.back()=="reopen");
 }
 // Simulate ERR_DROP interrupting a resize. Its disconnect hook must clear
 // both guards, allowing a later resize to be scheduled after recovery.
 cl_windowResize.Notify(1,800,600,true);cl_windowResizeRestart=cl_windowModeChange=1;
 failRenderer=true;
 try{CL_Vid_Restart(1);assert(false);}catch(int){CL_ResetWindowResizeState();}
 assert(!cl_windowResize.Pending()&&!cl_windowResizeRestart&&!cl_windowModeChange);
 assert(cl_windowResize.Notify(500,1000,700,true));
}
'''


class MenuResizeTests(unittest.TestCase):
    def test_module_geometry_menu_ownership_and_error_recovery(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        flags = compiler_target_args(compiler)
        ui = (ROOT / 'code/client/cl_ui.cpp').read_text()
        client = (ROOT / 'code/client/cl_main.cpp').read_text()
        body = FIXTURE + function(ui, 'int CL_UIMenuForRestart(')
        body += function(ui, 'void CL_RestoreUIMenu(')
        body += function(client, 'static void CL_ResetWindowResizeState( void ) {')
        body += function(client, 'static void CL_FinishUIMenuRestart( void ) {')
        body += function(client, 'static void CL_Vid_Restart( refShutdownCode_t shutdownCode ) {') + CHECKS
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='menu-resize-', dir=ROOT / '.tmp') as tmp:
            source = Path(tmp) / 'probe.cpp'
            exe = Path(tmp) / 'probe.exe'
            source.write_text(body)
            built = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT),
                                    str(source), '-o', str(exe)], capture_output=True, text=True, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], capture_output=True, text=True, cwd=ROOT, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        disconnect = function(client, 'qboolean CL_Disconnect(')
        self.assertIn('CL_ResetWindowResizeState();', disconnect)


if __name__ == '__main__':
    unittest.main()
