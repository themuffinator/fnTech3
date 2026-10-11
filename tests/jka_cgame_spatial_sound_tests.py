"""Academy sound imports keep their native arguments and Fn audio paths.

Retail jamp.exe 1.01 e8062d39...: import 37 at 0x412910 enters 0x46ac60;
0x46ae6f selects the listener's level from the last audio listener, not the
current player snapshot. Imports 40 and 42 both enter 0x46b180, at 0x412975
and 0x4129a1 respectively. No retail implementation text is incorporated.
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

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/cgame/cg_public.h"
#include "code/client/client_jka_cgame.hpp"
#include "code/client/client_jka_sound.hpp"
#include "code/compat/jka_render_adapter.hpp"
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <limits>
struct Drop {};
void QDECL Com_Error(errorParm_t, const char *, ...) { throw Drop{}; }
static comGameId_t profile = COM_GAME_JKA_MP;
int Com_GameIsJKAMP() { return profile == COM_GAME_JKA_MP; }
static int jkaSoundListener = 0;
static int starts, lastEntity, lastChannel, lastHandle, respatializations;
static float lastVolume;
static const float *lastOriginPointer;
static vec3_t lastOrigin;
void S_StartSoundVolume(vec3_t origin, int entity, int channel, sfxHandle_t handle, float volume) {
 ++starts;lastEntity=entity;lastChannel=channel;lastHandle=handle;lastVolume=volume;
 lastOriginPointer=origin;
 if(origin) std::memcpy(lastOrigin,origin,sizeof(lastOrigin));
}
static void Respatialize(int entity,const vec3_t origin,vec3_t axis[3],int water) {
 assert(entity==jkaSoundListener||!Com_GameIsJKAMP());++respatializations;
 assert(origin&&axis&&water==1);
}
static struct {void (*Respatialize)(int,const vec3_t,vec3_t[3],int);} si;
static const void *spanData;
static std::size_t spanBytes;
static void *CL_JKACgameSpan(intptr_t address,std::size_t bytes) {
 if(address!=reinterpret_cast<intptr_t>(spanData)||bytes>spanBytes)throw Drop{};
 return const_cast<void *>(spanData);
}
'''

DISPATCH = r'''
static bool Dispatch(intptr_t *args,intptr_t &result) {
 using namespace fntech3::jka;
 using module::Call;
 result=0;
 const auto call=module::Lookup(COM_GAME_JKA_MP,module::Module::CGAME,module::Direction::IMPORT,args[0]);
 switch(call) {
'''

CHECKS = r'''
 default: assert(false);
 }
 return false;
}
template<class F>void Refuses(F callback) {
 const int before=starts;
 bool caught=false;try{callback();}catch(Drop){caught=true;}
 assert(caught&&before==starts);
}
static void Start(intptr_t origin,intptr_t entity,intptr_t channel,intptr_t handle) {
 intptr_t args[]{37,origin,entity,channel,handle},result=99;
 assert(Dispatch(args,result)&&result==0);
}
int main() {
 using namespace fntech3::jka;
 using module::Call;
 // None of the other profiles goes through the Academy facade.
 for(int id=COM_GAME_QUAKE3;id<COM_GAME_COUNT;++id) {
  profile=static_cast<comGameId_t>(id);
  if(profile!=COM_GAME_JKA_MP)Refuses([]{S_StartJKASound(nullptr,0,0,1);});
 }
 profile=COM_GAME_JKA_MP;
 assert(cgame::SharedImport(Call::CG_S_STARTSOUND)==-1);
 assert(cgame::SharedImport(Call::CG_S_ADDLOOPINGSOUND)==CG_S_ADDLOOPINGSOUND);
 assert(cgame::SharedImport(Call::CG_S_ADDREALLOOPINGSOUND)==CG_S_ADDLOOPINGSOUND);
 assert(cgame::SharedImport(40)==CG_S_ADDLOOPINGSOUND&&cgame::SharedImport(42)==CG_S_ADDLOOPINGSOUND);
 // At startup the audio listener is zero. Both attached and fixed-origin
 // sounds follow the same channel/level rule; the vector is native-unaligned.
 std::array<unsigned char, sizeof(vec3_t)+2> native{};
 vec3_t vector{17.25f,-8.5f,0.0f},axes[3]{{1,0,0},{0,1,0},{0,0,1}};
 std::memcpy(native.data()+1,vector,sizeof(vector));
 spanData=native.data()+1;spanBytes=sizeof(vec3_t);
 const auto address=reinterpret_cast<intptr_t>(spanData);
 const int expected[]{CHAN_AUTO,CHAN_LOCAL,CHAN_WEAPON,CHAN_VOICE,12,
  CHAN_ITEM,CHAN_BODY,15,CHAN_LOCAL_SOUND,CHAN_ANNOUNCER,18,19,20,21};
 for(int channel=0;channel<14;++channel) for(int entity:{0,1,1023}) for(bool fixed:{false,true}) {
  const int before=starts;
  Start(fixed?address:0,entity,channel,72);
  assert(starts==before+1&&lastEntity==entity&&lastHandle==72&&lastChannel==expected[channel]);
  assert(lastVolume==((entity==0&&channel<7)?0.75f:1.0f));
  if(fixed) {
   assert(lastOriginPointer!=spanData&&std::memcmp(lastOrigin,vector,sizeof(vector))==0);
  } else assert(!lastOriginPointer);
 }
 // Explicit origins do not require a linked entity (retail effect calls use
 // -1); implicit origins must fit the selected profile's entity arena.
 for(int entity:{INT_MIN,-1,1024,INT_MAX}) {
  Start(address,entity,0,72);assert(lastEntity==entity&&lastVolume==1.0f);
  Refuses([&]{Start(0,entity,0,72);});
 }
 // Follow the sound listener, including an absent backend and a listener
 // change. Calls from another profile must not change Academy-owned state.
 for(bool backend:{false,true}) {
  si.Respatialize=backend?Respatialize:nullptr;
  S_Respatialize(17,vector,axes,1);assert(jkaSoundListener==17);
  Start(0,17,2,73);assert(lastVolume==0.75f);
  Start(0,0,2,73);assert(lastVolume==1.0f);
  profile=COM_GAME_QUAKE3;S_Respatialize(18,vector,axes,1);assert(jkaSoundListener==17);
  profile=COM_GAME_JKA_MP;
 }
 assert(respatializations==2);
 // Never truncate a native scalar, or publish malformed vector input.
 if(sizeof(intptr_t)>sizeof(int)) {
  for(int index:{2,3,4}) for(long long wide:{(long long)INT_MIN-1,(long long)INT_MAX+1}) {
   intptr_t args[]{37,address,0,0,1};args[index]=static_cast<intptr_t>(wide);intptr_t result;
   Refuses([&]{Dispatch(args,result);});
  }
 }
 spanBytes=sizeof(vec3_t)-1;Refuses([&]{Start(address,0,0,1);});spanBytes=sizeof(vec3_t);
 Refuses([&]{Start(address+1,0,0,1);});
 for(std::uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00001u}) for(int axis=0;axis<3;++axis) {
  std::memcpy(native.data()+1,vector,sizeof(vector));
  std::memcpy(native.data()+1+axis*sizeof(float),&bits,sizeof(bits));
  Refuses([&]{Start(address,0,0,1);});
 }
}
'''

LOOP_PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/cgame/cg_public.h"
#include "code/client/client_jka_cgame.hpp"
#include <array>
#include <cassert>
void QDECL Com_Error(errorParm_t,const char *,...) { assert(false); }
void QDECL Com_Printf(const char *,...) { assert(false); }
struct sfx_t {qboolean inMemory=qtrue;int soundLength=1024;const char *soundName="fixture";};
struct Loop {vec3_t origin{},velocity{};qboolean active=qfalse,kill=qfalse,doppler=qfalse;
 float oldDopplerScale=0,dopplerScale=0;sfx_t *sfx=nullptr;int framenum=0;};
static std::array<Loop,1024> loopSounds;
static int s_soundStarted=1,s_soundMuted=0,s_numSfx=1,listener_number=0;
static sfx_t sample;
static sfx_t *S_KnownSfxAt(int handle) {assert(handle==0);return &sample;}
static void S_memoryLoad(sfx_t *) {assert(false);}
static struct {int integer=1;} dopplerSetting,*s_doppler=&dopplerSetting;
static struct {int framecount=1;} cls;
static constexpr float MAX_DOPPLER_SCALE=50.0f;
'''

LOOP_CHECKS = r'''
int main() {
 const vec3_t origin{10,0,0},velocity{100,0,0};
 // Actual backend bodies make this distinction observable: a real loop
 // suppresses Doppler, whereas both Academy imports select an ordinary loop.
 for(int ordinal:{40,42}) {
  loopSounds[1]={};
  const auto operation=fntech3::jka::cgame::SharedImport(ordinal);
  if(operation==CG_S_ADDLOOPINGSOUND)S_Base_AddLoopingSound(1,origin,velocity,0);
  else if(operation==CG_S_ADDREALLOOPINGSOUND)S_Base_AddRealLoopingSound(1,origin,velocity,0);
  else assert(false);
  const auto &loop=loopSounds[1];
  assert(loop.active&&loop.kill&&loop.doppler&&loop.sfx==&sample);
  assert(loop.dopplerScale>1.2f&&loop.dopplerScale<1.22f&&loop.framenum==1);
  assert(loop.origin[0]==10&&loop.velocity[0]==100);
 }
 S_Base_AddRealLoopingSound(2,origin,velocity,0);
 assert(loopSounds[2].active&&!loopSounds[2].kill&&!loopSounds[2].doppler);
}
'''


class JKACgameSpatialSoundTests(unittest.TestCase):
    def compile_and_run(self, body):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        scratch = ROOT / '.tmp/tests/jka-cgame-spatial-sound'
        at = scratch
        while at != ROOT:
            if at.exists():
                assert not at.is_symlink() and not getattr(at.lstat(), 'st_file_attributes', 0) & 0x400
            at = at.parent
        scratch.mkdir(parents=True, exist_ok=True)
        self.assertTrue(scratch.resolve().is_relative_to((ROOT / '.tmp').resolve()))
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder = Path(temporary).resolve()
            source = folder / 'probe.cpp'
            source.write_text(body)
            flags = compiler_target_args(compiler)
            if os.name == 'nt' and not flags:
                flags = ['-DFNTECH3_Q3_ONLY']
            for extra, name in (([], 'normal'), (['-O2', '-ffast-math'], 'fast')):
                executable = folder / (name + '.exe')
                built = subprocess.run([compiler, *flags, '-std=c++17', *extra, '-I', str(ROOT),
                                        str(source), '-o', str(executable)], cwd=folder,
                                       text=True, capture_output=True, timeout=90)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                ran = subprocess.run([str(executable)], cwd=folder, text=True,
                                     capture_output=True, timeout=30, **options)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertFalse(any(item.is_symlink() or getattr(item.lstat(), 'st_file_attributes', 0) & 0x400
                                 for item in folder.rglob('*')))

    def test_production_host_listener_and_backend_contracts(self):
        main = (ROOT / 'code/client/audio/snd_main.cpp').read_text()
        host = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text()
        start = host.index('    case Call::CG_S_STARTSOUND: {')
        end = host.index('    case Call::CG_S_STARTLOCALSOUND:', start)
        body = PRELUDE + function(main, 'extern "C" void S_StartJKASound(')
        body += function(main, 'extern "C" void S_Respatialize(')
        self.compile_and_run(body + DISPATCH + host[start:end] + CHECKS)

    def test_both_loop_imports_reach_fn_doppler_path(self):
        backend = (ROOT / 'code/client/audio/legacy/snd_dma.cpp').read_text()
        # The file has a forward declaration: select the actual definition.
        start = backend.index('static void S_Base_AddLoopingSound(',
                              backend.index('static void S_Base_AddLoopingSound(') + 1)
        body = LOOP_PRELUDE + function(backend[start:], 'static void S_Base_AddLoopingSound(')
        body += function(backend, 'static void S_Base_AddRealLoopingSound(')
        self.compile_and_run(body + LOOP_CHECKS)

    def test_listener_lifetime_is_the_sound_system(self):
        main = (ROOT / 'code/client/audio/snd_main.cpp').read_text()
        init = function(main, 'extern "C" void S_Init( void )')
        self.assertIn('jkaSoundListener = 0;', init)
        self.assertLess(init.index('if ( si.Shutdown != nullptr )'), init.index('jkaSoundListener = 0;'))
        host = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text()
        self.assertNotIn('jkaSoundListener', function(host, 'static void CL_JKACgameClear()'))


if __name__ == '__main__':
    unittest.main()
