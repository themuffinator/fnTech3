"""Academy effect CameraShake primitives: the compiled life, radius and
intensity, the per-spawn draw, and the record the host hands the client
game's export 31."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from jka_effect_play_tests import fixture as playback_fixture

ROOT = Path(__file__).resolve().parents[1]

# Independently authored declarations; no retail effect text is used.
KERNEL = r'''
#include "code/compat/jka_effect_play.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <set>
namespace effect=fntech3::jka::effects;
namespace play=fntech3::jka::effectplay;
using Kind=effect::Kind;
static bool Compile(const std::string &text,play::Spec &spec,const char **reason=nullptr){
 effect::Definition definition;effect::Failure syntax;play::Failure failure;
 assert(effect::Parse(text,definition,syntax));
 int handle=8;for(auto &media:definition.primitives[0].media)media.handle=handle++;
 const bool served=play::Compile(definition.primitives[0],spec,failure);
 if(reason)*reason=failure.reason;return served;
}
int main(){
 play::Spec spec;const char *reason=nullptr;
 // Life, radius and intensity; "bounce" and "intensity" share one parser
 // (0x40cd90), so the later key wins. Flag words are inert.
 assert(Compile("CameraShake\n{\nname quake\nflags usePhysics\nlife 400 900\nintensity 7 8\nbounce 1.5 3\n"
  "radius 250 500\ncullrange 2000\ndelay 0 50\n}\n",spec,&reason));
 assert(spec.kind==Kind::CameraShake&&spec.life.first==400&&spec.life.last==900);
 assert(spec.elasticity.first==1.5f&&spec.elasticity.last==3&&spec.radius.first==250&&spec.radius.last==500);
 assert(spec.delay.last==50&&spec.ignored.empty()&&spec.unknownWords.empty());
 assert(Compile("CameraShake\n{\nbounce 1 2\nintensity 0.25\n}\n",spec)&&spec.elasticity.first==0.25f&&spec.elasticity.last==0.25f);
 // The constructor's defaults (0x40c650): a 50 ms life, no radius, no intensity.
 assert(Compile("CameraShake\n{\n}\n",spec)&&spec.life.first==50&&spec.radius.last==0&&spec.elasticity.last==0);
 // Every field and list the case never reads is inert, whatever its value.
 assert(Compile("CameraShake\n{\nshaders\n[\ngfx/own/a\n]\nsounds\n[\nsound/own/a.wav\n]\nvelocity 1 2 3\n"
  "gravity 99999\nrotation 5\nwind 3\nflags depthHack relative setShaderTime\nspawnFlags evenDistribution orgOnSphere\n"
  "rgb\n{\nstart 1 0 0\nflags wave\n}\nsize\n{\nstart 99999\n}\n}\n",spec));
 assert(spec.spawnFlags==(0x2000u|1u)&&spec.shaders.empty()&&spec.sounds.empty()&&spec.children.empty());
 // Named refusals for values beyond Fn's bounds and unserved spawn controls.
 struct Case{const char *text,*reason;};
 for(const Case &item:std::initializer_list<Case>{
  {"CameraShake\n{\nintensity 0 101\n}\n","effects camera shake intensity exceeds the Fn bound"},
  {"CameraShake\n{\nbounce -150\n}\n","effects camera shake intensity exceeds the Fn bound"},
  {"CameraShake\n{\nradius 20000\n}\n","effects spawn shape exceeds the Fn bound"},
  {"CameraShake\n{\nlife 0\n}\n","effects lifetime is outside the Fn bound"},
  {"CameraShake\n{\nlife 70000\n}\n","effects lifetime is outside the Fn bound"},
  {"CameraShake\n{\nspawnFlags randRotAroundFwd\n}\n","effects spawn controls are not served yet"},
  {"CameraShake\n{\ncount 501\n}\n","effects spawn count exceeds the Fn bound"},
  {"CameraShake\n{\norigin 0 0 20000\n}\n","effects vector range exceeds the Fn bound"}}){
  play::Spec old;old.shader=199;assert(!Compile(item.text,old,&reason)&&old.shader==199&&!std::strcmp(reason,item.reason));
 }
 // Each spawn draws an intensity and radius within their ranges; the radius
 // and life are truncated toward zero, as _ftol (0x539584) does.
 assert(Compile("CameraShake\n{\norigin 4 0 0\nlife 70.9\nintensity 2 3\nradius 300 400\n}\n",spec));
 play::Axis axis;assert(play::Basis({0,1,0},axis));
 std::uint32_t random=5;std::set<int> radii;float least=10,most=0;
 for(int n=0;n<300;++n){
  play::Spawn s;assert(play::Instantiate(spec,{10,20,30},axis,random,s));
  assert(s.kind==Kind::CameraShake&&(s.origin==play::Vector{10,24,30})&&s.life==70);
  assert(s.shakeRadius>=300&&s.shakeRadius<=400&&s.shakeIntensity>=2&&s.shakeIntensity<=3);
  radii.insert(s.shakeRadius);least=std::min(least,s.shakeIntensity);most=std::max(most,s.shakeIntensity);
 }
 assert(radii.size()>50&&least<2.1f&&most>2.9f);
 assert(Compile("CameraShake\n{\nradius 10.9\nintensity -0.5\n}\n",spec));
 play::Spawn fixed;assert(play::Instantiate(spec,{0,0,0},axis,random,fixed));
 assert(fixed.shakeRadius==10&&fixed.shakeIntensity==-0.5f&&fixed.life==50);
 assert(Compile("CameraShake\n{\nradius -3.7\n}\n",spec)&&play::Instantiate(spec,{0,0,0},axis,random,fixed)&&fixed.shakeRadius==-3);
 // The shared spawn shapes run before the case: orgOnSphere moves the origin.
 assert(Compile("CameraShake\n{\nspawnFlags orgOnSphere\nradius 64\nheight 64\n}\n",spec));
 for(int n=0;n<50;++n){
  play::Spawn s;assert(play::Instantiate(spec,{0,0,0},axis,random,s));
  const double length=std::sqrt(double(s.origin[0])*s.origin[0]+double(s.origin[1])*s.origin[1]+double(s.origin[2])*s.origin[2]);
  assert(length>63.9&&length<64.1&&s.shakeRadius==64);
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
static intptr_t playArgs[6];static float playOrigin[3],playForward[3]{0,0,1};
static void Play(intptr_t id,float x,float y,float z){
 playOrigin[0]=x;playOrigin[1]=y;playOrigin[2]=z;
 const intptr_t args[]{266,id,reinterpret_cast<intptr_t>(playOrigin),reinterpret_cast<intptr_t>(playForward),-1,-1};
 std::memcpy(playArgs,args,sizeof(args));assert(InvokePlayback(playArgs)==0);
}
static bool Same(const fntech3_jka_cgame_camera_shake_t &record,float x,float y,float z,float intensity,int radius,int time){
 return record.origin[0]==x&&record.origin[1]==y&&record.origin[2]==z&&record.intensity==intensity&&
  record.radius==radius&&record.time==time;
}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);
 fxVariables[2].value=16;Frame(1000);
 std::array<byte,FNTECH3_JKA_CGAME_SHARED_BUFFER_BYTES+1> scratch{};jkaCgameSharedBuffer=scratch.data()+1;
 assets["effects/own/blast.efx"]="CameraShake\n{\nflags usePhysics\norigin 6 0 0\nlife 300.75\nbounce 2.5\nradius 200.9\n}\n"
  "Particle\n{\nlife 100\nsize\n{\nstart 3\n}\nshader gfx/own/flash\n}\n";
 assets["effects/own/late.efx"]="CameraShake\n{\ndelay 100\nlife 80\nintensity 0.5\nradius 64\n}\n";
 assets["effects/own/none.efx"]="CameraShake\n{\ncount 0\nradius 64\n}\n";
 assets["effects/own/near.efx"]="CameraShake\n{\nlife 120\nintensity 1\nradius 100\n}\n";
 assets["effects/own/runner.efx"]="FxRunner\n{\norigin 5 0 0\nplayfx\n[\nown/near\n]\n}\n";
 const auto blast=Register("own/blast"),late=Register("own/late"),none=Register("own/none"),runner=Register("own/runner");
 assert(blast>0&&late>0&&none>0&&runner>0);
 // The case publishes the spawn origin, the intensity and the truncated
 // radius and life, and calls export 31 at once; it creates no instance,
 // while the particle beside it does.
 Play(blast,10,20,30);
 assert(shakes.size()==1&&Active()==1&&Same(shakes[0],10,20,36,2.5f,200,300));
 // A delayed shake waits in the schedule and is handed over when it falls due.
 shakes.clear();Frame(2000);Play(late,1,2,3);assert(shakes.empty()&&jkaCgameEffects->scheduled.size()==1);
 Frame(2050);Draw();assert(shakes.empty());
 Frame(2100);Draw();assert(shakes.size()==1&&Same(shakes[0],1,2,3,0.5f,64,80)&&jkaCgameEffects->scheduled.empty());
 // A count of zero hands nothing over; a paused frame creates nothing.
 shakes.clear();Frame(3000);Play(none,0,0,0);assert(shakes.empty());
 Frame(0);Play(blast,0,0,0);assert(shakes.empty());Frame(3050);
 // A nested shake comes from where its runner places it.
 Frame(3100);Play(runner,7,8,9);assert(shakes.size()==1&&Same(shakes[0],7,8,14,1,100,120));
 // The module may not play or free effects from inside the export, and it
 // may not leave the effect context or shared buffer changed behind it.
 intptr_t current=runner;
 for(int mutation=0;mutation<4;++mutation){
  if(mutation==0)shakeMutation=[]{InvokePlayback(playArgs);};
  if(mutation==1)shakeMutation=[]{CL_JKACgameEffectsFree();};
  if(mutation==2)shakeMutation=[]{CL_JKACgameEffectsAdjustTime(5000);};
  if(mutation==3)shakeMutation=[]{jkaCgameSharedBuffer=nullptr;};
  shakes.clear();recover=true;Reject([&]{Play(current,0,0,0);});recover=false;shakeMutation=nullptr;
  assert(shakes.size()==1&&!jkaCgameEffects&&!jkaCgameEffectsTracing&&!jkaCgameSharedBuffer);
  System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view));Frame(4000+mutation);
  jkaCgameSharedBuffer=scratch.data()+1;assets["effects/own/near.efx"]="CameraShake\n{\nlife 120\nintensity 1\nradius 100\n}\n";
  assets["effects/own/runner.efx"]="FxRunner\n{\norigin 5 0 0\nplayfx\n[\nown/near\n]\n}\n";
  current=Register("own/runner");assert(current>0);
 }
 // Without a registered shared buffer the record has nowhere to go.
 jkaCgameSharedBuffer=nullptr;shakes.clear();Reject([&]{Play(current,0,0,0);});assert(shakes.empty());
}
'''


def host_fixture():
    prefix = playback_fixture().split('int main(){')[0]
    old_call = 'intptr_t QDECL VM_Call(vm_t*,int,int,...){assert(false);return 0;}'
    assert prefix.count(old_call) == 1
    return prefix.replace(old_call, HOST) + MAIN


class JKACameraShakeTests(unittest.TestCase):
    def compile_and_run(self, body, name):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-camera-shake'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / f'{name}.cpp'
            source.write_text(body)
            for fast in (False, True):
                with self.subTest(fast_math=fast):
                    executable = directory / (('fast' if fast else 'plain') + ('.exe' if os.name == 'nt' else ''))
                    target = compiler_target_args(compiler)
                    if os.name == 'nt' and not target and name == 'kernel':
                        target = ['-DFNTECH3_Q3_ONLY']
                    # Clang keeps the 540 KB pool reset of the effects
                    # context's init and free (instances = {}) in the import
                    # dispatcher's frame, and the reentry cases nest two such
                    # frames: a property of this fixture, not of the engine,
                    # whose MSVC frames there are under 5 KB.
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

    def test_life_radius_intensity_and_inert_fields(self):
        self.compile_and_run(KERNEL, 'kernel')

    def test_actual_imports_hand_the_record_to_export_31(self):
        self.compile_and_run(host_fixture(), 'host')


if __name__ == '__main__':
    unittest.main()
