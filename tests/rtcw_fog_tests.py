"""Execute the shared RTCW fog controller in C and C++, including invalid input."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
#include "code/renderercommon/tr_rtcw_fog.h"
#include <assert.h>
#include <math.h>
/* Construct the foreign IEEE payloads explicitly, including ignored arguments. */
static float foreignFloat(uint32_t bits){float value;memcpy(&value,&bits,4);return value;}
#define BAD_NAN foreignFloat(UINT32_C(0x7fc00000))
#define BAD_INFINITY foreignFloat(UINT32_C(0x7f800000))
static fnRTCWFogState_t state, before;
static void same(void){assert(!memcmp(&state,&before,sizeof(state)));}
static void define(int slot,float density){
 assert(FnRTCWFogSet(&state,qtrue,slot,10,1000,0.2f,0.3f,0.4f,density,100));
}
static void switchTo(int slot,int duration,int now){
 assert(FnRTCWFogSet(&state,qtrue,FN_RTCW_FOG_SWITCH,slot,duration,BAD_NAN,BAD_NAN,BAD_NAN,BAD_NAN,now));
}
static void closeEnough(float a,float b){assert(fabsf(a-b)<0.0001f);}
static void invalidCommands(void){
 int slot,channel;float values[4];
 FnRTCWFogReset(&state);define(FN_RTCW_FOG_MAP,0.1f);before=state;
 assert(!FnRTCWFogSet(NULL,qtrue,4,0,1,0,0,0,0,0));
 for(slot=-2;slot<=12;++slot)if(slot<0||slot>FN_RTCW_FOG_SWITCH){
  assert(!FnRTCWFogSet(&state,qtrue,slot,0,1,0,0,0,0,0));same();
 }
 for(slot=-2;slot<=12;++slot)if(slot<0||slot>=FN_RTCW_FOG_SLOTS){
  assert(!FnRTCWFogSet(&state,qtrue,FN_RTCW_FOG_SWITCH,slot,1,0,0,0,0,0));same();
 }
 switchTo(FN_RTCW_FOG_SERVER,10,100);same(); /* absent target */
 for(channel=0;channel<4;++channel){
  int kind;for(kind=0;kind<3;++kind){
   values[0]=values[1]=values[2]=values[3]=0.1f;
   values[channel]=kind==0?BAD_NAN:(kind==1?BAD_INFINITY:-BAD_INFINITY);
   assert(!FnRTCWFogSet(&state,qtrue,4,0,1,values[0],values[1],values[2],values[3],0));same();
  }
 }
 assert(!FnRTCWFogSet(&state,qtrue,4,0,1,0,0,0,-1,0));same();
 assert(!FnRTCWFogSet(&state,qtrue,10,4,1,0,0,0,0,INT_MAX));same();
 assert(!FnRTCWFogSet(&state,qtrue,10,4,-1,0,0,0,0,INT_MIN));same();
 /* Unregister reads neither ignored colors nor the previous transition. */
 assert(FnRTCWFogSet(&state,qtrue,4,0,0,BAD_NAN,BAD_INFINITY,-BAD_INFINITY,-1,0));
 before.slots[4].registered=qfalse;same();
 FnRTCWFogReset(NULL);assert(FnRTCWFogAdvance(NULL,0,qtrue,qfalse,8192)==8192);
}
static void transitions(void){
 rtcwRenderFog_t saved;
 FnRTCWFogReset(&state);define(4,0.1f);switchTo(4,0,100);
 assert(FnRTCWFogAdvance(&state,100,qtrue,qfalse,8192)==8192);
 assert(state.slots[7].registered&&state.slots[7].color[3]==0); /* source timestamp */
 FnRTCWFogAdvance(&state,101,qtrue,qfalse,8192);
 assert(state.slots[7].color[3]==1&&state.slots[7].drawsky);
 define(5,1);assert(state.slots[5].mode==FN_RTCW_FOG_EXP);
 assert(FnRTCWFogSet(&state,qtrue,5,20,2000,0.8f,0.7f,0.6f,1,100));
 switchTo(5,100,101);FnRTCWFogAdvance(&state,151,qtrue,qfalse,8192);
 closeEnough(state.slots[7].start,15);closeEnough(state.slots[7].end,1500);
 closeEnough(state.slots[7].color[0],0.5f);assert(state.slots[7].density==1);
 saved=state.slots[7];switchTo(4,200,151);
 assert(!memcmp(&state.slots[8],&saved,sizeof(saved)));
 FnRTCWFogAdvance(&state,351,qtrue,qfalse,8192);closeEnough(state.slots[7].end,1000);
 define(6,1.01f);assert(!state.slots[6].drawsky&&state.slots[6].clearscreen);
 switchTo(6,200,351);
 assert(FnRTCWFogAdvance(&state,351,qtrue,qfalse,8192)==1000);
 assert(state.slots[9].finishTime==0&&state.slots[7].mode==FN_RTCW_FOG_LINEAR);
 assert(FnRTCWFogAdvance(&state,352,qtrue,qfalse,500)==500);
 assert(FnRTCWFogAdvance(&state,352,qfalse,qtrue,500)==1500);
 assert(FnRTCWFogAdvance(&state,352,qtrue,qtrue,500)==500);
 saved=state.slots[7];assert(FnRTCWFogSet(&state,qtrue,9,0,0,BAD_NAN,BAD_NAN,BAD_NAN,BAD_NAN,0));
 assert(FnRTCWFogAdvance(&state,353,qtrue,qfalse,8192)==8192);
 assert(!memcmp(&state.slots[7],&saved,sizeof(saved)));
 assert(FnRTCWFogAdvance(&state,353,qfalse,qfalse,8192)==1000);
 assert(!state.slots[7].registered); /* SP copies an unregistered target */
 FnRTCWFogReset(&state);memset(&before,0,sizeof(before));same();
}
static void timeAndColorBounds(void){
 FnRTCWFogReset(&state);define(4,0.1f);define(5,0.2f);
 switchTo(4,-1,100);FnRTCWFogAdvance(&state,100,qtrue,qfalse,8192);
 assert(state.slots[7].color[3]==1); /* negative duration is immediate */
 assert(FnRTCWFogSet(&state,qtrue,5,20,2000,0.8f,0.7f,0.6f,0.2f,100));
 switchTo(5,100,100);FnRTCWFogAdvance(&state,50,qtrue,qfalse,8192);
 closeEnough(state.slots[7].end,1000); /* clock rewind does not extrapolate */
 switchTo(5,INT_MAX,INT_MIN);FnRTCWFogAdvance(&state,INT_MAX,qtrue,qfalse,8192);
 closeEnough(state.slots[7].end,2000); /* elapsed arithmetic cannot overflow */
 switchTo(4,0,INT_MAX);FnRTCWFogAdvance(&state,INT_MIN,qtrue,qfalse,8192);
 assert(FnRTCWFogFinite(state.slots[7].end));
 closeEnough(FnRTCWFogMix(-FLT_MAX,FLT_MAX,0.5f),0);
 assert(FnRTCWFogMix(-FLT_MAX,FLT_MAX,0)==-FLT_MAX);
 assert(FnRTCWFogMix(-FLT_MAX,FLT_MAX,1)==FLT_MAX);
 assert(FnRTCWFogSet(&state,qtrue,4,0,100,FLT_MAX,-FLT_MAX,FLT_MAX,1.1f,0));
 assert(FnRTCWFogSet(&state,qtrue,5,0,200,-FLT_MAX,FLT_MAX,-FLT_MAX,1.1f,0));
 switchTo(4,-1,100);FnRTCWFogAdvance(&state,100,qtrue,qfalse,8192);
 switchTo(5,100,100);FnRTCWFogAdvance(&state,150,qtrue,qfalse,8192);
 closeEnough(state.slots[7].color[0],0);closeEnough(state.slots[7].color[1],0);
}
static void singlePlayerRules(void){
 rtcwRenderFog_t saved;
 FnRTCWFogReset(&state);
 assert(FnRTCWFogSet(&state,qfalse,6,0,1000,0.2f,0.3f,0.4f,1,0));
 assert(state.slots[6].mode==FN_RTCW_FOG_LINEAR);
 assert(FnRTCWFogSet(&state,qfalse,10,6,100,0,0,0,0,100));
 assert(state.slots[8].end==1000); /* SP starts from the selected fog if no current exists */
 assert(FnRTCWFogAdvance(&state,150,qfalse,qfalse,8192)==1000);
 assert(state.slots[7].dirty==1&&state.slots[7].registered);
 assert(FnRTCWFogAdvance(&state,200,qfalse,qtrue,8192)==2000);
 assert(state.slots[7].dirty==1);
 assert(FnRTCWFogAdvance(&state,201,qfalse,qfalse,8192)==1000);
 assert(state.slots[7].dirty==0);
 assert(FnRTCWFogSet(&state,qfalse,5,0,2000,0.8f,0.7f,0.6f,1,201));
 assert(FnRTCWFogSet(&state,qfalse,10,5,0,BAD_NAN,BAD_NAN,BAD_NAN,BAD_NAN,201));
 assert(!state.slots[9].startTime&&!state.slots[9].finishTime);
 assert(state.slots[9].dirty&&state.slots[7].dirty);
 assert(FnRTCWFogAdvance(&state,201,qfalse,qfalse,8192)==2000);
 assert(state.slots[7].color[3]==1&&!state.slots[7].dirty);
 saved=state.slots[7];
 /* SP map selection disables fog even when that slot is unregistered. Its
  * duration is unused, so an otherwise overflowing finish time is irrelevant. */
 assert(FnRTCWFogSet(&state,qfalse,10,4,INT_MAX,BAD_NAN,BAD_NAN,BAD_NAN,BAD_NAN,INT_MAX));
 assert(state.selected==0&&!state.slots[4].registered&&!state.slots[9].registered);
 assert(!memcmp(&saved,&state.slots[7],sizeof(saved)));
 assert(FnRTCWFogAdvance(&state,201,qfalse,qfalse,8192)==8192);
 assert(!state.slots[7].registered&&state.slots[7].mode==0);
}
int main(void){invalidCommands();transitions();timeAndColorBounds();singlePlayerRules();return 0;}
'''


class RTCWFogTests(unittest.TestCase):
    def test_renderer_boundary_keeps_ieee_guards(self):
        # Passing a float under finite-only assumptions can discard even its
        # integer-bit validation. Keep the actual legacy renderer policy pinned.
        makefile = (ROOT / 'Makefile').read_text()
        self.assertIn('RENDERER_FP_CFLAGS = $(if $(filter tr_scene.c tr_shade.c,$(notdir $<)),-fno-fast-math)', makefile)

    def test_commands_transitions_and_bounds(self):
        for language in ('c', 'c++'):
            compiler = shutil.which('clang' if language == 'c' else 'clang++')
            if not compiler:
                self.skipTest('Clang C/C++ compiler unavailable')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
            with self.subTest(language=language), tempfile.TemporaryDirectory(prefix='rtcw-fog-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp)
                source = folder / ('probe.c' if language == 'c' else 'probe.cpp')
                executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(CHECKS, encoding='utf-8')
                command = [compiler, *target, '-std=' + ('c11' if language == 'c' else 'c++17'),
                           '-O2', '-ffast-math', '-fno-fast-math', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT), str(source), '-o', str(executable)]
                if os.name != 'nt':
                    command.append('-lm')
                build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
