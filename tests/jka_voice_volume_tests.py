"""Academy voice query follows the real facade's audio-absent state."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/compat/jka_game_abi.h"
#include <cassert>
#include <cstdint>
#include <initializer_list>
static comGameId_t selected;
int Com_GameIsJKAMP(){return selected==COM_GAME_JKA_MP;}
void Com_Error(errorParm_t,const char*,...){throw 7;}
static int backendCalls=0;
static void Shutdown(){++backendCalls;}
static int voiceCalls=0,level=0;
static int Voice(int entity){assert(entity==17);++voiceCalls;return level;}
static struct { void(*Shutdown)(); int(*JKAVoiceVolume)(int); } si;
'''
CHECKS = r'''
template<class F>void Refuses(F call){bool caught=false;try{call();}catch(int){caught=true;}assert(caught);}
int main(){
 for(int id=COM_GAME_QUAKE3;id<COM_GAME_COUNT;++id){
  selected=static_cast<comGameId_t>(id);
  for(bool active:{false,true}){
   si.Shutdown=active?Shutdown:nullptr;
   for(intptr_t entity:{INTPTR_MIN,intptr_t(-1),intptr_t(0),intptr_t(1),intptr_t(1023),intptr_t(1024),INTPTR_MAX}){
    if(selected==COM_GAME_JKA_MP&&!active&&entity>=0&&entity<1024)
     assert(S_JKAVoiceVolume(entity)==0);
    else Refuses([&]{S_JKAVoiceVolume(entity);});
    assert(backendCalls==0&&si.Shutdown==(active?Shutdown:nullptr));
   }
  }
 }
 selected=COM_GAME_JKA_MP;
 si.Shutdown=Shutdown;Refuses([]{S_JKAVoiceVolume(17);});
 si.JKAVoiceVolume=Voice;
 for(int expected:{-1,0,1,2,3,4}){level=expected;assert(S_JKAVoiceVolume(17)==expected);}
 assert(voiceCalls==6);
 si={};assert(S_JKAVoiceVolume(17)==0);
 assert(backendCalls==0);
}
'''


class JKAVoiceVolumeTests(unittest.TestCase):
    def test_actual_facade_profile_bounds_and_absent_backend(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        body = function((ROOT / 'code/client/audio/snd_main.cpp').read_text(),
                        'extern "C" int S_JKAVoiceVolume(')
        scratch = ROOT / '.tmp/tests/jka-voice-volume'
        at = scratch
        while at != ROOT:
            if at.exists():
                assert not at.is_symlink() and not getattr(at.lstat(), 'st_file_attributes', 0) & 0x400
            at = at.parent
        scratch.mkdir(parents=True, exist_ok=True)
        assert scratch.resolve().is_relative_to((ROOT / '.tmp').resolve())
        with tempfile.TemporaryDirectory(prefix='facade-', dir=scratch) as temporary:
            source = Path(temporary) / 'voice.cpp'
            source.write_text(HARNESS + body + CHECKS)
            for extra in ([], ['-O2', '-ffast-math']):
                # One executable per build: Windows can hold the last one run.
                executable = Path(temporary) / ('voice-fast.exe' if extra else 'voice.exe')
                built = subprocess.run([compiler, *flags, '-std=c++17', *extra,
                    '-I', str(ROOT), str(source), '-o', str(executable)], cwd=ROOT,
                    capture_output=True, text=True, timeout=60)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                ran = subprocess.run([str(executable)], cwd=ROOT, capture_output=True,
                    text=True, timeout=30, **options)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            assert Path(temporary).resolve().is_relative_to(scratch.resolve())
            assert not any(item.is_symlink() or getattr(item.lstat(), 'st_file_attributes', 0) & 0x400
                           for item in Path(temporary).rglob('*'))


if __name__ == '__main__':
    unittest.main()
