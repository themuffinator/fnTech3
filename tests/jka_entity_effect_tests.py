"""Academy effects played on an entity (client-game import 268): the
caller's own axis, the unbolted word every retail call site passes, and the
named refusal of a bolted one."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from jka_effect_play_tests import fixture as playback_fixture

ROOT = Path(__file__).resolve().parents[1]

KERNEL = r'''
#include "code/compat/jka_effect_play.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
namespace play=fntech3::jka::effectplay;
int main(){
 static_assert(play::EntityUnbolted==-1,"retail's unbolted word");
 // The nine words are taken as given, row by row, unnormalized.
 const float scaled[9]{0,2,0, -2,0,0, 0,0,2};
 play::Axis axis;assert(play::EntityAxis(scaled,axis));
 assert((axis[0]==play::Vector{0,2,0})&&(axis[1]==play::Vector{-2,0,0})&&(axis[2]==play::Vector{0,0,2}));
 const float skew[9]{0.5f,0.25f,0, 0,1,0, 0.125f,0,-3};
 assert(play::EntityAxis(skew,axis)&&(axis[0]==play::Vector{0.5f,0.25f,0})&&(axis[2]==play::Vector{0.125f,0,-3}));
 const float bound[9]{1024,-1024,0, 0,0,0, 0,0,0};assert(play::EntityAxis(bound,axis)&&axis[0][1]==-1024);
 // A word that is not finite, or beyond the Fn bound, refuses and leaves
 // the output as it was.
 const float kept[9]{1,0,0, 0,1,0, 0,0,1};
 // Bit patterns, so fast-math builds hold them: +inf, -inf, NaN, then
 // 1024.5 and -2048.
 for(int word=0;word<9;++word)for(const std::uint32_t bad:{0x7f800000u,0xff800000u,0x7fc00001u,0x44801000u,0xc5000000u}){
  float words[9];std::memcpy(words,kept,sizeof(words));std::memcpy(&words[word],&bad,4);
  play::Axis old;assert(play::EntityAxis(kept,old));
  assert(!play::EntityAxis(words,old)&&(old[0]==play::Vector{1,0,0})&&(old[2]==play::Vector{0,0,1}));
 }
}
'''

HOST = r'''
static std::vector<fntech3_jka_cgame_camera_shake_t> shakes;
static void (*shakeMutation)();
intptr_t QDECL VM_Call(vm_t *module,int nargs,int operation,...){
 assert(module==cgvm&&nargs==0&&operation==CG_RAVEN_CAMERA_SHAKE);
 fntech3_jka_cgame_camera_shake_t record;std::memcpy(&record,jkaCgameSharedBuffer,sizeof(record));
 shakes.push_back(record);
 if(shakeMutation)shakeMutation();
 return 0;
}
'''

MAIN = r'''
using Kind=fntech3::jka::effects::Kind;
// Import 268's seven words: id, origin, axis, bolt word, entity, volume, radius.
static std::array<byte,14> originBytes;static std::array<byte,38> axisBytes;
static intptr_t entityArgs[8];
static void Place(float x,float y,float z,const float (&axis)[9]){
 const float origin[]{x,y,z};std::memcpy(originBytes.data()+1,origin,12);std::memcpy(axisBytes.data()+1,axis,36);
}
static intptr_t PlayEntity(intptr_t id,intptr_t bolt=-1,intptr_t entity=-1,intptr_t volume=-1,intptr_t radius=-1){
 const intptr_t args[]{268,id,reinterpret_cast<intptr_t>(originBytes.data()+1),reinterpret_cast<intptr_t>(axisBytes.data()+1),
  bolt,entity,volume,radius};
 std::memcpy(entityArgs,args,sizeof(args));return InvokePlayback(entityArgs);
}
static const playback::Spawn *Live(Kind kind,int nth=0){
 for(const auto &instance:jkaCgameEffects->instances)if(instance.active&&instance.spawn.kind==kind&&!nth--)return &instance.spawn;
 return nullptr;
}
static bool Near(const playback::Vector &value,float x,float y,float z){
 return std::fabs(value[0]-x)<1e-4f&&std::fabs(value[1]-y)<1e-4f&&std::fabs(value[2]-z)<1e-4f;
}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);
 fxVariables[2].value=16;Frame(1000);
 std::array<byte,FNTECH3_JKA_CGAME_SHARED_BUFFER_BYTES+1> scratch{};jkaCgameSharedBuffer=scratch.data()+1;
 originBytes.fill(0xa5);axisBytes.fill(0xa5);
 // An independently authored muzzle flash: a particle offset along all three
 // axes, a sound at the same offset and a shake.
 assets["effects/own/flash.efx"]="Particle\n{\norigin 6 3 2\nlife 100\nsize\n{\nstart 3\n}\nshader gfx/own/flash\n}\n"
  "Sound\n{\norigin 6 3 2\nsound sound/own/shot.wav\n}\n"
  "CameraShake\n{\norigin 6 3 2\nlife 50\nintensity 0.25\nradius 60\n}\n";
 assets["effects/own/late.efx"]="Particle\n{\ndelay 100\norigin 4 0 0\nlife 100\nsize\n{\nstart 3\n}\nshader gfx/own/late\n}\n";
 const auto flash=Register("own/flash"),late=Register("own/late");assert(flash>0&&late>0);
 // The caller's axis is used as given, so a forward along +y and a right
 // along -x place the offsets there; the stored axis is the caller's.
 const float turned[9]{0,1,0, -1,0,0, 0,0,1};
 Place(100,200,300,turned);const auto placedOrigin=originBytes;const auto placedAxis=axisBytes;
 assert(PlayEntity(flash)==0&&Active()==1&&originBytes==placedOrigin&&axisBytes==placedAxis);
 const auto *particle=Live(Kind::Particle);assert(particle&&Near(particle->origin,97,206,302));
 assert((particle->axis[0]==playback::Vector{0,1,0})&&(particle->axis[1]==playback::Vector{-1,0,0}));
 assert(effectSounds.size()==1&&effectSounds[0].origin==(std::array<float,3>{97,206,302}));
 assert(shakes.size()==1&&shakes[0].origin[0]==97&&shakes[0].origin[1]==206&&shakes[0].origin[2]==302&&
  shakes[0].intensity==0.25f&&shakes[0].radius==60&&shakes[0].time==50);
 // Unnormalized: a scaled axis scales the offsets, where import 266 builds
 // a unit axis from its forward direction.
 const float scaled[9]{0,2,0, -2,0,0, 0,0,2};
 Place(0,0,0,scaled);assert(PlayEntity(flash)==0&&Live(Kind::Particle,1)&&Near(Live(Kind::Particle,1)->origin,-6,12,4));
 const float forward[]{0,2,0};std::array<byte,14> forwardBytes;std::memcpy(forwardBytes.data()+1,forward,12);
 intptr_t idArgs[]{266,flash,reinterpret_cast<intptr_t>(originBytes.data()+1),reinterpret_cast<intptr_t>(forwardBytes.data()+1),-1,-1};
 assert(InvokePlayback(idArgs)==0&&Active()==3);
 const auto *unit=Live(Kind::Particle,2);assert(unit&&std::fabs(unit->axis[0][1]-1)<1e-6f&&std::fabs(unit->origin[1]-6)<1e-5f);
 // The entity number, volume and radius are not read: any values play the
 // same spawn from the same draws.
 Frame(1100);Place(10,20,30,turned);jkaCgameEffects->instances={};
 const auto seed=jkaCgameEffects->random;assert(PlayEntity(flash)==0);
 const auto first=*Live(Kind::Particle);const auto after=jkaCgameEffects->random;
 jkaCgameEffects->random=seed;jkaCgameEffects->instances={};assert(PlayEntity(flash,-1,77,255,4096)==0);
 const auto second=*Live(Kind::Particle,0);assert(first.origin==second.origin&&first.life==second.life&&jkaCgameEffects->random==after);
 // A delayed spawn waits in the schedule with the caller's origin and axis.
 jkaCgameEffects->instances={};effectSounds.clear();shakes.clear();
 Frame(2000);Place(1,2,3,turned);assert(PlayEntity(late)==0&&Active()==0&&jkaCgameEffects->scheduled.size()==1);
 assert((jkaCgameEffects->scheduled[0].axis[1]==playback::Vector{-1,0,0}));
 Frame(2050);Draw();assert(Active()==0);Frame(2100);Draw();
 assert(Active()==1&&Near(Live(Kind::Particle)->origin,1,6,3)&&jkaCgameEffects->scheduled.empty());
 // A paused frame creates nothing; a missing declaration logs and returns
 // before the bolt word is read.
 jkaCgameEffects->instances={};Frame(0);assert(PlayEntity(flash)==0&&Active()==0&&shakes.empty());Frame(2200);
 assert(PlayEntity(0,5)==0&&PlayEntity(256,0x100401)==0&&Active()==0);
 // Any bolt word other than -1 refuses by name and creates nothing: retail
 // would bolt the spawns to a model through Ghoul2.
 for(const intptr_t bolt:{intptr_t(0),intptr_t(1),intptr_t(0x100401),intptr_t(-2),intptr_t(INT32_MIN)}){
  Reject([&]{PlayEntity(flash,bolt);});assert(Active()==0&&shakes.empty()&&effectSounds.empty()&&jkaCgameEffects);
 }
 // An axis that is not finite or exceeds the Fn bound, and unreadable
 // pointers, refuse.
 for(int word=0;word<9;++word)for(const std::uint32_t bad:{0x7fc00001u,0x45000000u}){
  float axis[9];std::memcpy(axis,turned,sizeof(axis));std::memcpy(&axis[word],&bad,4);Place(0,0,0,axis);
  Reject([&]{PlayEntity(flash);});assert(Active()==0);
 }
 Place(0,0,0,turned);
 const std::uint32_t inf=0x7f800000u;std::memcpy(originBytes.data()+5,&inf,4);
 Reject([&]{PlayEntity(flash);});assert(Active()==0);Place(0,0,0,turned);
 for(int position:{2,3}){
  intptr_t args[]{268,flash,reinterpret_cast<intptr_t>(originBytes.data()+1),reinterpret_cast<intptr_t>(axisBytes.data()+1),-1,-1,-1,-1};
  args[position]=0;Reject([&]{InvokePlayback(args);});
  args[position]=static_cast<intptr_t>(UINTPTR_MAX-8);Reject([&]{InvokePlayback(args);});
 }
 vm.index=VM_UI;Reject([&]{PlayEntity(flash);});vm.index=VM_CGAME;
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{PlayEntity(flash);});vm.dllExports=nullptr;
 assert(Active()==0);
 // Reentry from inside the shake export refuses, and the context is cleared.
 shakeMutation=[]{InvokePlayback(entityArgs);};recover=true;Reject([&]{PlayEntity(flash);});recover=false;shakeMutation=nullptr;
 assert(!jkaCgameEffects&&!jkaCgameEffectsTracing);
}
'''


def host_fixture():
    prefix = playback_fixture().split('int main(){')[0]
    old_call = 'intptr_t QDECL VM_Call(vm_t*,int,int,...){assert(false);return 0;}'
    assert prefix.count(old_call) == 1
    return prefix.replace(old_call, HOST) + MAIN


class JKAEntityEffectTests(unittest.TestCase):
    def compile_and_run(self, body, name):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-entity-effect'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / f'{name}.cpp'
            source.write_text(body)
            for fast in (False, True):
                with self.subTest(fast_math=fast):
                    executable = directory / (('fast' if fast else 'plain') + ('.exe' if os.name == 'nt' else ''))
                    target = engine_fixture_args(compiler)
                    # As in jka_camera_shake_tests: clang keeps the effects
                    # pool's 540 KB reset in the import dispatcher's frame,
                    # and the reentry case nests two such frames.
                    stack = []
                    if os.name == 'nt' and name == 'host':
                        stack = (['-Wl,--stack,8388608'] if Path(compiler).stem.lower() == 'g++'
                                 else ['-Wl,/stack:8388608'])
                    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Werror',
                        '-D_CRT_SECURE_NO_WARNINGS', *(['-ffast-math'] if fast else []), '-I', str(ROOT),
                        str(source), '-o', str(executable), *stack], cwd=ROOT, capture_output=True, text=True,
                        timeout=120)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_caller_axis_kernel(self):
        self.compile_and_run(KERNEL, 'kernel')

    def test_import_268_plays_at_the_callers_axis(self):
        self.compile_and_run(host_fixture(), 'host')


if __name__ == '__main__':
    unittest.main()
