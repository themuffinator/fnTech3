"""Execute profile-specific menu routing without accessing any input devices."""
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
#include "code/client/input_compat.hpp"
#include "code/qcommon/game_profile.h"
#include <cassert>
using fnql::input::PointerOwner;
enum { KEYCATCH_CONSOLE=1, KEYCATCH_UI=2, KEYCATCH_CGAME=8,
       KEYCATCH_RETAIL_MOUSEPASS=16, KEYCATCH_BROWSER=32 };
enum { UI_MOUSE_EVENT=4, CG_MOUSE_EVENT=6 };
constexpr int kPointerMenuMask = KEYCATCH_UI | KEYCATCH_CGAME | KEYCATCH_BROWSER;
struct vm_t { bool dllExports; } ui, cg;
vm_t *uivm=&ui, *cgvm=&cg;
struct { struct { int vidWidth=2560, vidHeight=1440; } glconfig;
         int captureWidth=1280, captureHeight=720; } cls;
struct { int mouseDx[2]{}, mouseDy[2]{}, mouseIndex=0; } cl;
struct { bool isFullscreen=false, cdsFullscreen=false; } glw_state;
comGameId_t profile;
int catcher, calls, lane, outX, outY;
bool delayElapsed=true, ignoreMouse=false;
int Com_GameIsQuakeLive() { return profile==COM_GAME_QUAKELIVE; }
int Com_GameIsAlice() { return profile==COM_GAME_ALICE; }
bool aliceCursorMode;
int cursorCalls;
void CL_AliceMoveCursor(int,int) { ++cursorCalls; }
int Key_GetCatcher() { return catcher; }
bool IN_ConsoleUsesAbsolutePointer() { return !glw_state.cdsFullscreen; }
bool CL_AdvertisementBridge_IsDelayElapsed() { return delayElapsed; }
int Cvar_VariableIntegerValue(const char*) { return ignoreMouse; }
void Record(int target, int x, int y) { ++calls; lane=target; outX=x; outY=y; }
void Con_MouseEvent(int x,int y) { Record(1,x,y); }
void Con_SetMousePos(int x,int y) { Record(2,x,y); }
void CL_WebView_OnMouseMove(int x,int y) { Record(3,x,y); }
void VM_Call(vm_t *vm,int count,int event,int x,int y) {
 assert(count==2);
 assert((vm==uivm && event==UI_MOUSE_EVENT) || (vm==cgvm && event==CG_MOUSE_EVENT));
 Record(event,x,y);
}
'''

CHECKS = r'''
int main() {
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  profile=id;
  const bool absolute=id==COM_GAME_QUAKELIVE;
  for(int mask:{int(KEYCATCH_UI),int(KEYCATCH_CGAME),KEYCATCH_UI|KEYCATCH_CGAME}) {
   catcher=mask;
   for(bool native:{false,true}) {
    ui.dllExports=cg.dllExports=native;
    const int event=(mask&KEYCATCH_UI)?UI_MOUSE_EVENT:CG_MOUSE_EVENT;
    calls=0; CL_MouseEvent(-17,23);
    assert(calls==(absolute?0:1));
    if(calls) assert(lane==event && outX==-17 && outY==23);
    calls=0; CL_MouseAbsoluteEvent(1920,1080);
    assert(calls==(absolute?1:0));
    if(calls) assert(lane==event && outX==(native?960:1920) && outY==(native?540:1080));
    assert(cl.mouseDx[0]==0 && cl.mouseDy[0]==0);
   }
   // All backends choose matching presentation and release on focus loss.
   for(auto resolve:{SDL_Owner,WIN_Owner,X11_Owner}) {
    assert(resolve()==(absolute?PointerOwner::Menu:PointerOwner::Gameplay));
    fnql::input::PointerModeInputs inputs;
    inputs.owner=resolve();
    auto mode=fnql::input::ResolvePointerMode(inputs);
    assert(mode.driveInput && mode.reportAbsolute==absolute);
    assert(mode.relativeMotion==!absolute && mode.showSystemCursor==absolute);
    inputs.focused=false;
    mode=fnql::input::ResolvePointerMode(inputs);
    assert(!mode.driveInput && !mode.confineToWindow && mode.showSystemCursor);
   }
   // The browser and console override a module catcher in every profile.
   catcher=mask|KEYCATCH_BROWSER;
   for(auto resolve:{SDL_Owner,WIN_Owner,X11_Owner}) assert(resolve()==PointerOwner::Menu);
   calls=0; CL_MouseEvent(9,10); assert(!calls);
   CL_MouseAbsoluteEvent(11,12); assert(calls==1 && lane==3 && outX==11 && outY==12);
   catcher|=KEYCATCH_CONSOLE;
   for(auto resolve:{SDL_Owner,WIN_Owner,X11_Owner}) assert(resolve()==PointerOwner::Console);
   calls=0; CL_MouseEvent(-9,10); assert(calls==1 && lane==1 && outX==-9);
   CL_MouseAbsoluteEvent(11,12); assert(calls==2 && lane==2 && outX==11);
  }
  catcher=KEYCATCH_UI;
  calls=0; delayElapsed=false;
  CL_MouseEvent(1,2); CL_MouseAbsoluteEvent(3,4); assert(!calls);
  delayElapsed=true; ignoreMouse=true;
  CL_MouseEvent(1,2); CL_MouseAbsoluteEvent(3,4); assert(!calls);
  ignoreMouse=false;
  uivm=nullptr; cgvm=nullptr;
  for(int mask:{KEYCATCH_UI,KEYCATCH_CGAME}) {
   catcher=mask; CL_MouseEvent(1,2); CL_MouseAbsoluteEvent(3,4); assert(!calls);
  }
  uivm=&ui; cgvm=&cg;
  // Closing a menu restores gameplay accumulation; stale absolute events do nothing.
  catcher=0; cl.mouseDx[0]=cl.mouseDy[0]=0;
  CL_MouseAbsoluteEvent(100,200); assert(!calls);
  CL_MouseEvent(-4,7); assert(cl.mouseDx[0]==-4 && cl.mouseDy[0]==7);
  cl.mouseDx[0]=cl.mouseDy[0]=0;
 }
 // Alice's cursor mode takes relative motion before gameplay does, and only
 // under Alice; the console keeps its own mouse even then.
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_ALICE}) {
  profile=id; catcher=0; aliceCursorMode=true; cursorCalls=0; calls=0;
  CL_MouseEvent(5,6);
  const bool alice=id==COM_GAME_ALICE;
  assert(cursorCalls==(alice?1:0));
  assert(cl.mouseDx[0]==(alice?0:5) && cl.mouseDy[0]==(alice?0:6));
  cl.mouseDx[0]=cl.mouseDy[0]=0;
  catcher=KEYCATCH_CONSOLE; cursorCalls=0;
  CL_MouseEvent(1,1); assert(!cursorCalls && calls==1 && lane==1);
  aliceCursorMode=false;
 }
}
'''


class MenuMouseContractTests(unittest.TestCase):
    def test_profile_dispatch_and_backend_presentation(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        source = FIXTURE
        client = (ROOT / 'code/client/cl_input.cpp').read_text()
        for signature in ('void CL_MouseEvent(',
                          'static void CL_ProjectDrawableToRetailModule(',
                          'void CL_MouseAbsoluteEvent('):
            source += function(client, signature)
        for path, signature, old, new in (
            ('code/sdl/sdl_input.cpp', 'static PointerOwner IN_ResolvePointerOwner(',
             'IN_ResolvePointerOwner', 'SDL_Owner'),
            ('code/win32/win_input.cpp', 'fnql::input::PointerOwner WIN_ResolvePointerOwner(',
             'WIN_ResolvePointerOwner', 'WIN_Owner'),
            ('code/unix/linux_glimp.cpp', 'static PointerOwner IN_AbsolutePointerOwnerKind(',
             'IN_AbsolutePointerOwnerKind', 'X11_Owner'),
        ):
            source += function((ROOT / path).read_text(), signature).replace(old, new)
        source += CHECKS
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='menu-mouse-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            build = subprocess.run(
                [compiler, *compiler_target_args(compiler), '-std=c++17', '-O2',
                 '-I', str(ROOT), str(cpp), '-o', str(exe)],
                cwd=folder, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=folder, capture_output=True,
                                 text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
