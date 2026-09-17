"""Execute fog view ownership and profile boundaries for all backends."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

SOURCE = r'''
#include "code/renderercommon/tr_rtcw_fog_view.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <initializer_list>
static struct {comGameId_t gameId; void (*Error)(errorParm_t,const char*,...);} ri;
static cvar_t fastsky{};
static cvar_t *r_fastsky=&fastsky;
static cvar_t *r_wolffog=nullptr;
static struct {
 fnRTCWFogState_t rtcwFog;
 qboolean rtcwSkyPortal;
 struct {int time; fnRTCWFogFrame_t rtcwFog;} refdef;
} tr;
static void Error(errorParm_t,const char*,...){throw 7;}
#include "code/renderercommon/tr_rtcw_fog_scene.h"
template<class F>static void reject(F action){bool failed=false;try{action();}catch(int){failed=true;}assert(failed);}
int main(){
 ri.Error=Error;refdef_t view{};view.time=100;
 for(auto mode:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  ri.gameId=mode;auto old=tr;
  reject([]{RE_SetRTCWFog(6,10,2000,0,0,0,1.1f);});
  assert(!memcmp(&tr,&old,sizeof(tr)));
  R_CopyRTCWFogToScene(&view);assert(!tr.refdef.rtcwFog.fogEnabled);
 }
 for(auto mode:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  memset(&tr,0,sizeof(tr));ri.gameId=mode;tr.refdef.time=100;
  view={};view.time=100;
  R_CopyRTCWFogToScene(&view);assert(!tr.refdef.rtcwFog.fogEnabled);
  RE_SetRTCWFog(10,6,20,NAN,NAN,NAN,NAN); // absent target is harmless
  RE_SetRTCWFog(6,10,2000,0.2f,0.3f,0.4f,1.1f);
  RE_SetRTCWFog(10,6,0,NAN,NAN,NAN,NAN);view.time=101;
  R_CopyRTCWFogToScene(&view);
  assert(tr.refdef.rtcwFog.fogEnabled&&tr.refdef.rtcwFog.farClip==2000);
  auto snapshot=tr.refdef.rtcwFog;
  RE_SetRTCWFog(6,20,3000,0.8f,0.7f,0.6f,1.1f);
  assert(!memcmp(&snapshot,&tr.refdef.rtcwFog,sizeof(snapshot)));
  view.rdflags=RDF_NOWORLDMODEL;
  auto controller=tr.rtcwFog;R_CopyRTCWFogToScene(&view);
  assert(!tr.refdef.rtcwFog.fogEnabled&&!tr.refdef.rtcwFog.skyEnabled);
  assert(!memcmp(&controller,&tr.rtcwFog,sizeof(controller)));
  view.rdflags=0;view.rtcw.skyboxPortal=qtrue;
  R_CopyRTCWFogToScene(&view);assert(tr.rtcwSkyPortal&&!tr.refdef.rtcwFog.fogEnabled);
  RE_SetRTCWFog(2,0,500,0.5f,0.5f,0.5f,0.1f);
  R_CopyRTCWFogToScene(&view);
  assert(tr.refdef.rtcwFog.fog.end==500);
  auto saved=tr.rtcwFog;
  reject([]{RE_SetRTCWFog(-1,0,0,0,0,0,0);});
  reject([]{RE_SetRTCWFog(6,0,2000,NAN,0,0,0);});
  assert(!memcmp(&saved,&tr.rtcwFog,sizeof(saved)));
  FnRTCWFogReset(&tr.rtcwFog);tr.rtcwSkyPortal=qfalse;view={};
  R_CopyRTCWFogToScene(&view);assert(!tr.refdef.rtcwFog.fogEnabled);
 }
 // Snapshot values stay separate from both native input and future commands.
 fnRTCWFogState_t controller{};
 assert(FnRTCWFogSet(&controller,qtrue,6,0,1000,0.2f,0.3f,0.4f,0.1f,0));
 assert(FnRTCWFogSet(&controller,qtrue,10,6,0,0,0,0,0,0));
 view={};view.time=1;view.rtcw.fog.registered=qtrue;view.rtcw.fog.end=17;
 auto frame=FnRTCWFogCapture(&controller,qtrue,qfalse,&view);
 assert(frame.fog.end==1000&&view.rtcw.fog.end==17&&frame.drawSky);
 auto held=frame;FnRTCWFogReset(&controller);assert(!memcmp(&held,&frame,sizeof(frame)));
 view.rtcw.drawSkybox=qfalse;
 frame=FnRTCWFogCapture(&controller,qtrue,qtrue,&view);assert(!frame.drawSky);
 view.rtcw.drawSkybox=qtrue;
 frame=FnRTCWFogCapture(&controller,qfalse,qtrue,&view);assert(frame.drawSky);
 return 0;
}
'''


class RTCWFogSceneTests(unittest.TestCase):
    def test_view_ownership_and_profile_boundaries(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            flags = ['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-fog-scene-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); source = folder / 'probe.cpp'; executable = folder / 'probe.exe'
            source.write_text(SOURCE, encoding='utf-8')
            command = [compiler, *flags, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-variable', '-I', str(ROOT), str(source), '-o', str(executable)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
