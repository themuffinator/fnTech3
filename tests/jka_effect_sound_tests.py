"""Academy effect Sound primitives: the compiled list, the per-spawn pick, and
the spatial sound the host starts for each spawn."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from jka_effect_play_tests import fixture as playback_fixture

ROOT = Path(__file__).resolve().parents[1]

# Independently authored declarations; no retail effect text is used.
KERNEL = r'''
#include "code/compat/jka_effect_play.hpp"
#include <cassert>
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
 // The list, count, delay and spawn flags; "lessAttenuation" is no word of
 // retail's parser (0x40d4e0), so it sets nothing and is kept for diagnostics.
 assert(Compile("Sound\n{\nname boom\ncullrange 900\ncount 0 2\ndelay 0 300\nspawnFlags lessAttenuation evenDistribution\n"
  "sounds\n[\nsound/own/a.wav\nsound/own/b.mp3\n]\n}\n",spec,&reason));
 assert(spec.kind==Kind::Sound&&spec.sounds==(std::vector<int>{8,9})&&spec.spawnFlags==0x2000u);
 assert(spec.count.first==0&&spec.count.last==2&&spec.delay.last==300);
 assert(spec.unknownWords==(std::vector<std::string>{"lessattenuation"})&&spec.ignored.empty());
 // Fields the Sound case never reads are inert, whatever their values.
 assert(Compile("Sound\n{\nsound sound/own/a.wav\nlife 70000\nvelocity 1 2 3\nbounce 200\nrotation 5\n"
  "flags usePhysics depthHack relative\nrgb\n{\nstart 1 0 0\nflags wave\n}\nsize\n{\nstart 99999\n}\n}\n",spec));
 assert(spec.sounds==(std::vector<int>{8}));
 // Unknown flag words are read past on every family; known ones still count.
 assert(Compile("Particle\n{\nshader gfx/own/p\nflags useAlpha bogus depthHack\n}\n",spec));
 assert(spec.flags==(0x8000000u|0x100000u)&&spec.unknownWords==(std::vector<std::string>{"bogus"}));
 // An empty list is retail's handle zero: silent.
 assert(Compile("Sound\n{\nname quiet\n}\n",spec)&&spec.sounds.empty());
 // Named refusals.
 struct Case{const char *text,*reason;};
 for(const Case &item:std::initializer_list<Case>{
  {"Sound\n{\nsounds\n[\nsound/own/a.wav\n]\nshader gfx/own/p\n}\n","effects media is not served for this primitive family"},
  {"Sound\n{\nsound sound/own/a.wav\nspawnFlags randRotAroundFwd\n}\n","effects spawn controls are not served yet"},
  {"Sound\n{\nsound sound/own/a.wav\ncount 501\n}\n","effects spawn count exceeds the Fn bound"},
  {"Sound\n{\nsound sound/own/a.wav\norigin 0 0 20000\n}\n","effects vector range exceeds the Fn bound"}}){
  play::Spec old;old.shader=199;assert(!Compile(item.text,old,&reason)&&old.shader==199&&!std::strcmp(reason,item.reason));
 }
 // Each spawn picks one handle (0x405d50) at the spawn origin, with the
 // origin offsets along the play axis.
 assert(Compile("Sound\n{\norigin 4 0 0\nsounds\n[\nsound/own/a.wav\nsound/own/b.wav\nsound/own/c.wav\n]\n}\n",spec));
 play::Axis axis;assert(play::Basis({0,1,0},axis));
 std::uint32_t random=5;std::set<int> picked;
 for(int n=0;n<300;++n){
  play::Spawn s;assert(play::Instantiate(spec,{10,20,30},axis,random,s));
  assert(s.kind==Kind::Sound&&(s.origin==play::Vector{10,24,30}));picked.insert(s.sound);
 }
 assert(picked==(std::set<int>{8,9,10}));
 assert(Compile("Sound\n{\nname quiet\n}\n",spec));
 play::Spawn quiet;assert(play::Instantiate(spec,{0,0,0},axis,random,quiet)&&quiet.sound==0);
 assert(play::SoundEntity==1023&&play::SoundChannel==0);
}
'''

MAIN = r'''
using Kind=fntech3::jka::effects::Kind;
static void Play(intptr_t id,float x,float y,float z){
 const float origin[]{x,y,z},forward[]{0,0,1};
 intptr_t args[]{266,id,reinterpret_cast<intptr_t>(origin),reinterpret_cast<intptr_t>(forward),-1,-1};
 assert(InvokePlayback(args)==0);
}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);
 fxVariables[2].value=16;Frame(1000);
 assets["effects/own/shot.efx"]="Sound\n{\norigin 2 0 0\nsounds\n[\nsound/own/a.wav\nsound/own/b.wav\n]\n}\n"
  "Particle\n{\nlife 100\nsize\n{\nstart 3\n}\nshader gfx/own/flash\n}\n";
 assets["effects/own/late.efx"]="Sound\n{\ndelay 100\nsound sound/own/late.wav\n}\n";
 assets["effects/own/none.efx"]="Sound\n{\ncount 0\nsound sound/own/none.wav\n}\n";
 assets["effects/own/mixed.efx"]="Sound\n{\nsound sound/own/a.wav\nshader gfx/own/flash\n}\n";
 const auto shot=Register("own/shot"),late=Register("own/late"),none=Register("own/none");
 assert(shot>0&&late>0&&none>0);
 const std::set<int> shotSounds{301,302};
 assert(sounds==(std::vector<std::string>{"sound/own/a.wav","sound/own/b.wav","sound/own/late.wav","sound/own/none.wav"}));
 // The Sound case starts its sound at once, at the spawn origin, on
 // ENTITYNUM_NONE and CHAN_AUTO; it creates no instance, while the
 // particle beside it does.
 Play(shot,10,20,30);
 assert(effectSounds.size()==1&&Active()==1);
 const auto first=effectSounds[0];
 assert((first.origin==std::array<float,3>{10,20,32})&&first.entity==1023&&first.channel==0&&shotSounds.count(first.handle));
 // Many plays pick both handles.
 std::set<int> picked;for(int n=0;n<200;++n){Frame(1001+n);Play(shot,0,0,0);picked.insert(effectSounds.back().handle);}
 assert(picked==shotSounds&&effectSounds.size()==201);
 // A delayed Sound waits in the schedule and starts when it falls due.
 effectSounds.clear();Frame(2000);Play(late,1,2,3);assert(effectSounds.empty()&&jkaCgameEffects->scheduled.size()==1);
 Frame(2050);Draw();assert(effectSounds.empty());
 Frame(2100);Draw();assert(effectSounds.size()==1&&effectSounds[0].handle==303&&jkaCgameEffects->scheduled.empty());
 assert((effectSounds[0].origin==std::array<float,3>{1,2,3}));
 // A count of zero plays nothing.
 effectSounds.clear();Frame(3000);Play(none,0,0,0);assert(effectSounds.empty());
 // A nested Sound plays where its runner places it.
 assets["effects/own/near.efx"]="Sound\n{\nsound sound/own/near.wav\n}\n";
 assets["effects/own/runner.efx"]="FxRunner\n{\norigin 5 0 0\nplayfx\n[\nown/near\n]\n}\n";
 const auto runner=Register("own/runner");assert(runner>0);
 Frame(3100);Play(runner,7,8,9);
 assert(effectSounds.size()==1&&(effectSounds[0].origin==std::array<float,3>{7,8,14})&&effectSounds[0].handle==305);
 // A failed registration is handle zero: silent, as retail's null sound.
 silent=true;assets["effects/own/missing.efx"]="Sound\n{\nsound sound/own/missing.wav\n}\n";
 const auto missing=Register("own/missing");silent=false;assert(missing>0);
 effectSounds.clear();Frame(3200);Play(missing,0,0,0);assert(effectSounds.empty());
 // Other media in a Sound would share its list: a named refusal.
 const auto mixed=Register("own/mixed");assert(mixed>0);
 Frame(3300);Reject([&]{Play(mixed,0,0,0);});assert(effectSounds.empty());
}
'''


def host_fixture():
    prefix = playback_fixture().split('int main(){')[0]
    assert prefix.count('#include <set>') == 0
    return prefix.replace('#include <vector>', '#include <vector>\n#include <set>', 1) + MAIN


class JKAEffectSoundTests(unittest.TestCase):
    def compile_and_run(self, body, name):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-effect-sound'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / f'{name}.cpp'
            source.write_text(body)
            for fast in (False, True):
                with self.subTest(fast_math=fast):
                    executable = directory / (('fast' if fast else 'plain') + ('.exe' if os.name == 'nt' else ''))
                    target = engine_fixture_args(compiler)
                    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Werror',
                        '-D_CRT_SECURE_NO_WARNINGS', *(['-ffast-math'] if fast else []), '-I', str(ROOT),
                        str(source), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_sound_list_pick_and_inert_fields(self):
        self.compile_and_run(KERNEL, 'kernel')

    def test_actual_imports_start_spatial_sounds(self):
        self.compile_and_run(host_fixture(), 'host')


if __name__ == '__main__':
    unittest.main()
