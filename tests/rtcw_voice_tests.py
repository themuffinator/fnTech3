"""RTCW's exact MP/sound-disabled voice query and explicit active-SP guard."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <climits>
#include <cstdint>
#include <initializer_list>
static comGameId_t profile;
int Com_GameIsRTCW(){return profile==COM_GAME_RTCW_SP||profile==COM_GAME_RTCW_MP;}
int Com_GameIsRTCWMP(){return profile==COM_GAME_RTCW_MP;}
void Com_Error(errorParm_t,const char *,...){throw 7;}
static void Shutdown(){}
static struct{void(*Shutdown)();}si;
'''
CHECKS = r'''
template<class F>void Reject(F f){bool threw=false;try{f();}catch(int){threw=true;}assert(threw);}
int main(){
 for(auto game:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  profile=game;
  for(bool active:{false,true}){
   si.Shutdown=active?Shutdown:nullptr;
   for(int entity:{INT_MIN,-1,0,1,127,128,1023,INT_MAX}){
    if(!Com_GameIsRTCW()||(profile==COM_GAME_RTCW_SP&&(entity<0||(active&&entity<128))))
     Reject([&]{S_RTCWVoiceAmplitude(entity);});
    else assert(S_RTCWVoiceAmplitude(entity)==0);
   }
   if(sizeof(intptr_t)>4){
    Reject([]{S_RTCWVoiceAmplitude(static_cast<intptr_t>(INT64_C(2147483648)));});
    Reject([]{S_RTCWVoiceAmplitude(static_cast<intptr_t>(-INT64_C(2147483649)));});
   }
  }
 }
 // Shutdown/device-init failure restores the valid silent case.
 profile=COM_GAME_RTCW_SP;si.Shutdown=Shutdown;Reject([]{S_RTCWVoiceAmplitude(3);});
 si={};assert(S_RTCWVoiceAmplitude(3)==0);
}
'''


class RTCWVoiceTests(unittest.TestCase):
    def test_profile_audio_lifecycle_and_entity_bounds(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            flags = ['-m32']
        sound = (ROOT / 'code/client/audio/snd_main.cpp').read_text()
        dispatch = (ROOT / 'code/client/cl_cgame.cpp').read_text()
        self.assertIn('case Call::CG_S_GETVOICEAMPLITUDE:\n\t\t\treturn S_RTCWVoiceAmplitude( args[1] );', dispatch)
        body = function(sound, 'extern "C" int S_RTCWVoiceAmplitude(')
        with tempfile.TemporaryDirectory(prefix='rtcw-voice-', dir=ROOT / '.tmp') as tmp:
            source = Path(tmp) / 'probe.cpp'; exe = Path(tmp) / 'probe.exe'
            source.write_text(PRELUDE + body + CHECKS)
            built = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT), str(source), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
