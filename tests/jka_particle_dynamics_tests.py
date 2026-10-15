"""Academy particle dynamics: shader lists, motion, curves and delayed spawns."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from jka_effect_play_tests import fixture as playback_fixture

ROOT = Path(__file__).resolve().parents[1]

# Independently authored declarations with the shape of a torch's fire
# particle (a shader list, drift, curves and staggered spawns); no retail
# text is used.
KERNEL = r'''
#include "code/compat/jka_effect_play.hpp"
#include <cassert>
#include <cstring>
#include <initializer_list>
namespace effect=fntech3::jka::effects;
namespace play=fntech3::jka::effectplay;
static bool Compile(const std::string &text,play::Spec &spec,const char **reason=nullptr){
 effect::Definition definition;effect::Failure syntax;play::Failure failure;
 assert(effect::Parse(text,definition,syntax));
 int handle=8;for(auto &media:definition.primitives[0].media)media.handle=handle++;
 const bool served=play::Compile(definition.primitives[0],spec,failure);
 if(reason)*reason=failure.reason;return served;
}
static bool Near(float a,float b,float tolerance=0.0001f){return std::fabs(a-b)<=tolerance;}
static const char *Fire=
 "Particle\n{\nflags useAlpha\nspawnFlags evenDistribution rgbComponentInterpolation\ncount 4\nlife 400 800\n"
 "delay 0 200\ncullrange 250\norigin 2 -1 -1 4 1 1\nvelocity 10 0 0 20 0 0\nacceleration 0 -5 -5 0 5 5\ngravity 20 30\n"
 "rotation 80 100\nrotationDelta -4 4\nrgb\n{\nstart 1 0.5 0 1 0.9 0.2\nend 0 0 0\nparm 60\nflags linear nonlinear\n}\n"
 "alpha\n{\nend 0\nparm 2 5\nflags wave\n}\nsize\n{\nstart 4 5\nend 12\nparm 50\nflags linear clamp\n}\n"
 "shaders\n[\ngfx/own/fire1\ngfx/own/fire2\ngfx/own/fire3\n]\n}\n";
int main(){
 play::Spec spec;const char *reason=nullptr;
 // The whole declaration compiles: three shaders, curve nibbles, flags.
 assert(Compile(Fire,spec,&reason));
 assert(spec.kind==effect::Kind::Particle&&spec.shaders==(std::vector<int>{8,9,10})&&spec.shader==8);
 assert(spec.flags==0x8000000u&&spec.spawnFlags==(0x2000u|0x4000u));
 assert(spec.curves==((5u<<4)|8u|(0xdu<<8)));
 assert(spec.delay.first==0&&spec.delay.last==200&&spec.gravity.last==30&&spec.rotationDelta.first==-4);
 assert(spec.acceleration[1].first==-5&&spec.acceleration[1].last==5&&spec.velocity[0].last==20);
 assert(spec.colorEnd[0].first==0&&spec.rgbParm.first==60&&spec.alphaParm.last==5&&spec.sizeParm.first==50);
 assert(spec.sizeEnd.first==12&&spec.alphaEnd.first==0&&spec.color[3].first==1);
 // Curve words OR together (0x40cb10); a fifth word is never read.
 std::uint32_t bits=0;
 assert(play::CurveWords("nonlinear WAVE",bits)&&bits==0xcu);
 assert(play::CurveWords("linear clamp",bits)&&bits==0xdu);
 assert(play::CurveWords("linear linear linear linear madeUp",bits)&&bits==1u);
 assert(!play::CurveWords("linear madeUp",bits));
 // Delays, cull ranges and even spacing serve every family.
 assert(Compile("Line\n{\nshader gfx/own/l\ncullrange 100\ndelay 0 50\nspawnflags evenDistribution\n}\n",spec));
 assert(Compile("FxRunner\n{\ndelay 5\nplayFx\n[\nown/a\n]\n}\n",spec)&&spec.delay.first==5);
 // Named refusals.
 struct Case{const char *text,*reason;};
 for(const Case &item:std::initializer_list<Case>{
  {"Particle\n{\nshader gfx/own/p\nflags ghoul2Collision\n}\n","effects flags are not served yet"},
  {"Particle\n{\nshader gfx/own/p\nsize\n{\nflags linear sideways\n}\n}\n","unknown effects curve flag word"},
  {"Particle\n{\nshader gfx/own/p\nflags relative\n}\n","effects flags are not served yet"},
  {"OrientedParticle\n{\nshader gfx/own/p\nflags depthHack\n}\n","effects flags are not served yet"},
  {"Particle\n{\nshader gfx/own/p\ndelay 60001\n}\n","effects spawn delay exceeds the Fn bound"},
  {"Particle\n{\nshader gfx/own/p\ngravity 20000\n}\n","effects gravity exceeds the Fn bound"},
  {"Particle\n{\nshader gfx/own/p\nsound sound/own/s\n}\n","effects playback requires registered Fn shaders"},
  {"Particle\n{\ncount 1\n}\n","effects playback requires registered Fn shaders"},
  {"Particle\n{\nshader gfx/own/p\nspawnflags randRotAroundFwd\n}\n","effects spawn controls are not served yet"},
  {"Line\n{\nshader gfx/own/l\nlength\n{\nflags linear\n}\n}\n","effects curve controls are not served yet"},
  {"Line\n{\nshader gfx/own/l\nvelocity 1 0 0\n}\n","effects field is outside the served playback subset"},
  {"Emitter\n{\nacceleration 0 0 10\n}\n","effects field is outside the served playback subset"}}){
  play::Spec old;old.shader=199;assert(!Compile(item.text,old,&reason)&&old.shader==199&&!std::strcmp(reason,item.reason));
 }
 // One shader per spawn from the list; rgbComponentInterpolation keeps the
 // colours on one line (0x406de0); ranges land where the template puts them.
 play::Axis axis;assert(play::Basis({0,0,1},axis));
 assert(Compile(Fire,spec));
 std::uint32_t random=5;int picks[3]{};
 for(int n=0;n<3000;++n){
  play::Spawn spawn;assert(play::Instantiate(spec,{0,0,0},axis,random,spawn));
  assert(spawn.shader>=8&&spawn.shader<=10);++picks[spawn.shader-8];
  const float fraction=(spawn.rgbStart[1]-0.5f)/0.4f;
  assert(fraction>=-0.0001f&&fraction<=1.0001f&&spawn.rgbStart[0]==1&&Near(spawn.rgbStart[2],0.2f*fraction,0.001f));
  assert(spawn.rgbEnd[0]==0&&spawn.alphaStart==1&&spawn.alphaEnd==0&&spawn.sizeEnd==12&&spawn.curves==spec.curves);
  assert(spawn.life>=400&&spawn.life<=800&&spawn.velocity[2]>=10&&spawn.velocity[2]<=20);
  assert(spawn.acceleration[2]>=20&&spawn.acceleration[2]<=30&&spawn.rotationDelta>=-4&&spawn.rotationDelta<=4);
  assert(spawn.sizeParm==50&&spawn.alphaParm>=2&&spawn.alphaParm<=5&&spawn.rgbParm==60);
 }
 for(int count:picks)assert(count>800&&count<1200);
 assert(Compile("Particle\n{\nshader gfx/own/p\nrgb\n{\nstart 0 0 0 1 1 1\n}\n}\n",spec));
 bool independent=false;
 for(int n=0;n<50;++n){play::Spawn spawn;assert(play::Instantiate(spec,{0,0,0},axis,random,spawn)&&spawn.shader==8);
  independent|=!Near(spawn.rgbStart[0],spawn.rgbStart[1],0.01f);}
 assert(independent);
 assert(Compile("Particle\n{\nshader gfx/own/p\nspawnflags rgbComponentInterpolation\nrgb\n{\nstart 0 0 0 1 1 1\n}\n}\n",spec));
 for(int n=0;n<50;++n){play::Spawn spawn;assert(play::Instantiate(spec,{0,0,0},axis,random,spawn));
  assert(spawn.rgbStart[0]==spawn.rgbStart[1]&&spawn.rgbStart[1]==spawn.rgbStart[2]);}
 // The curve law (0x401e50, 0x402040, 0x402390) and its parm conversion.
 using play::CurvePercent;using play::CurveParm;
 assert(CurvePercent(1,0,1200,1000,400)==0.5f&&CurvePercent(0,123,1500,1000,1000)==1);
 const float nonlinear=CurveParm(5,80,1000,1000);assert(nonlinear==1800);
 assert(Near(CurvePercent(5,nonlinear,1500,1000,1000),0.75f)&&Near(CurvePercent(5,nonlinear,1900,1000,1000),0.3f));
 assert(CurvePercent(4,nonlinear,1500,1000,1000)==1&&Near(CurvePercent(4,nonlinear,1900,1000,1000),0.5f));
 const float clamp=CurveParm(0xd,50,1000,1000);assert(clamp==1500);
 assert(Near(CurvePercent(0xd,clamp,1200,1000,1000),0.7f)&&Near(CurvePercent(0xd,clamp,1600,1000,1000),0.2f));
 assert(Near(CurvePercent(0xc,clamp,1200,1000,1000),0.6f)&&CurvePercent(0xc,clamp,1600,1000,1000)==0);
 const float wave=CurveParm(8,2,1000,1000);assert(wave==2*0.0031415901612490416f);
 assert(Near(CurvePercent(8,wave,1250,1000,1000),0,0.001f)&&Near(CurvePercent(8,wave,1500,1000,1000),-1,0.001f));
 assert(Near(CurvePercent(9,wave,1500,1000,1000),-0.5f,0.001f));
 // One frame's look: nearest RGB bytes, truncated alpha, RGB scaled by
 // alpha unless useAlpha writes the alpha byte.
 play::Spawn look;look.life=1000;look.size=10;look.sizeEnd=2;look.rgbStart={1,0.5f,0};look.alphaStart=0.5f;
 play::Look out;assert(play::ParticleLook(look,1000,1000,out));
 assert(out.radius==10&&out.rgba==(std::array<unsigned char,4>{126,63,0,255}));
 look.flags=0x8000000u;assert(play::ParticleLook(look,1000,1000,out)&&out.rgba==(std::array<unsigned char,4>{255,128,0,127}));
 look.curves=(1u<<8)|(1u<<4)|1u;assert(play::ParticleLook(look,1500,1000,out));
 assert(out.radius==6&&out.rgba==(std::array<unsigned char,4>{128,64,0,63}));
 look.alphaStart=1;look.alphaEnd=0;look.curves=8u;look.alphaParm=2;
 assert(play::ParticleLook(look,1500,1000,out)&&out.rgba[3]==0); // a negative wave clamps to zero
 // Rotation steps and decays by the frame time (0x402640).
 play::Spawn spin;spin.rotationDelta=100;assert(play::ParticleRotate(spin,50));
 assert(Near(spin.rotation,50)&&Near(spin.rotationDelta,96.5f));
 assert(play::ParticleRotate(spin,0)&&Near(spin.rotation,50)&&Near(spin.rotationDelta,96.5f));
 // Motion: velocity takes the acceleration first (0x401a50).
 play::Spawn moving;moving.velocity={10,0,0};moving.acceleration={0,0,-20};assert(play::ParticleNext(moving,0.5f));
 assert((moving.velocity==play::Vector{10,0,-10})&&(moving.origin==play::Vector{5,0,-5}));
 // A late spawn is advanced by its lag (0x408e84).
 assert(Compile("Particle\n{\nshader gfx/own/p\nvelocity 10 0 0\ngravity 20\n}\n",spec));
 play::Spawn on,late;std::uint32_t first=3,second=3;
 assert(play::Instantiate(spec,{0,0,0},axis,first,on)&&play::Instantiate(spec,{0,0,0},axis,second,late,100));
 assert(on.origin[2]==0&&on.velocity[2]==10&&on.acceleration[2]==20);
 assert(Near(late.velocity[2],12)&&Near(late.origin[2],0.105f*12));
 // Delays (0x4073c6): even spacing over the truncated span, else drawn.
 assert(Compile("Particle\n{\nshader gfx/own/p\ncount 4\ndelay 0 200\nspawnflags evenDistribution\n}\n",spec));
 for(int n=0;n<4;++n)assert(play::Delay(spec,n,4,random)==50*n);
 assert(Compile("Particle\n{\nshader gfx/own/p\ncount 3\ndelay 0 100\nspawnflags evenDistribution\n}\n",spec));
 assert(play::Delay(spec,1,3,random)==33&&play::Delay(spec,2,3,random)==66);
 assert(Compile("Particle\n{\nshader gfx/own/p\ndelay 7\n}\n",spec));
 const auto before=random;assert(play::Delay(spec,0,1,random)==7&&random==before);
 assert(Compile("Particle\n{\nshader gfx/own/p\ndelay -30 -10\n}\n",spec));
 for(int n=0;n<50;++n){const int delay=play::Delay(spec,0,1,random);assert(delay<=-10&&delay>=-30);}
}
'''

TORCH = ('Particle\\n{\\nflags useAlpha\\nspawnFlags evenDistribution\\ncount 4\\nlife 400\\ndelay 0 200\\n'
         'cullrange 250\\nvelocity 10 0 0\\ngravity 20\\nrotation 90\\nrotationDelta 4\\n'
         'rgb\\n{\\nstart 1 0.5 0\\nend 0 0 0\\nflags linear\\n}\\n'
         'alpha\\n{\\nstart 1\\nend 0\\nparm 50\\nflags linear nonlinear\\n}\\n'
         'size\\n{\\nstart 8\\nend 2\\nflags linear\\n}\\n'
         'shaders\\n[\\ngfx/own/f1\\ngfx/own/f2\\ngfx/own/f3\\n]\\n}\\n')

HOST = r'''
static bool Near(float a,float b,float tolerance=0.0005f){return std::fabs(a-b)<=tolerance;}
static const JKACgameEffectInstance *Started(int time){
 for(const auto &instance:jkaCgameEffects->instances)if(instance.active&&instance.startTime==time)return &instance;
 return nullptr;
}
static const refEntity_t *Sprite(const JKACgameEffectInstance &instance){
 for(const auto &entity:entities)if(entity.origin[2]==instance.spawn.origin[2])return &entity;
 return nullptr;
}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);
 fxVariables[2].value=16;Frame(984);Frame(1000);
 assets["effects/own/torch.efx"]="@TORCH@";
 const auto id=Register("own/torch");assert(id>0&&shaders.size()==3);
 const float origin[]{100,200,300},forward[]{0,0,1};
 intptr_t args[]{266,id,reinterpret_cast<intptr_t>(origin),reinterpret_cast<intptr_t>(forward),-1,-1};
 // One spawn now, three waiting 50, 100 and 150 ms (evenDistribution).
 assert(InvokePlayback(args)==0&&Active()==1&&jkaCgameEffects->scheduled.size()==3);
 for(int n=0;n<3;++n)assert(jkaCgameEffects->scheduled[n].due==1050+50*n);
 Draw();assert(sprites==1);
 const auto &drawn=entities.back();
 assert(drawn.radius==8&&drawn.customShader>=101&&drawn.customShader<=103&&Near(drawn.rotation,90.64f));
 assert(drawn.shader.rgba[0]==255&&drawn.shader.rgba[1]==128&&drawn.shader.rgba[2]==0&&drawn.shader.rgba[3]==255);
 // The next frame moves it: velocity takes gravity first.
 Frame(1016);entities.clear();Draw();
 const auto *first=Started(1000);assert(first&&Near(first->spawn.origin[2],300+0.016f*10.32f)&&Near(first->spawn.velocity[2],10.32f));
 Frame(1050);Draw();assert(Active()==2&&jkaCgameEffects->scheduled.size()==2&&Started(1050)->spawn.origin[2]==300);
 // A paused frame keeps the schedule; a late creation is advanced by its lag.
 Frame(0);Draw();assert(Active()==2&&jkaCgameEffects->scheduled.size()==2);
 Frame(1120);Draw();assert(Active()==3&&jkaCgameEffects->scheduled.size()==1);
 const auto *late=Started(1120);assert(late&&Near(late->spawn.origin[2],300+0.0202f*10.4f)&&Near(late->spawn.velocity[2],10.4f));
 // Curves over the lifetime: linear RGB; alpha linear with nonlinear from 50%.
 Frame(1300);entities.clear();Draw();
 first=Started(1000);assert(first);const auto *sprite=Sprite(*first);assert(sprite);
 assert(sprite->radius==3.5f&&sprite->shader.rgba[0]==64&&sprite->shader.rgba[1]==32&&sprite->shader.rgba[2]==0);
 assert(sprite->shader.rgba[3]==95);
 Frame(1401);Draw();assert(!Started(1000)&&!jkaCgameEffects->scheduled.size());
 // Every listed shader is drawn over repeated plays.
 bool seen[3]{};
 for(int frame=0;frame<60;++frame){Frame(2000+16*frame);entities.clear();assert(InvokePlayback(args)==0);Draw();
  for(const auto &entity:entities)seen[entity.customShader-101]=true;}
 assert(seen[0]&&seen[1]&&seen[2]);
 // Free drops waiting spawns; the Fn schedule bound drops and counts.
 assert(!jkaCgameEffects->scheduled.empty());CL_JKACgameEffectsFree();assert(jkaCgameEffects->scheduled.empty());
 System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view));Frame(5000);args[1]=Register("own/torch");
 jkaCgameEffects->scheduled.resize(jkaCgameEffectScheduleBound);
 assert(InvokePlayback(args)==0&&Active()==1&&jkaCgameEffects->scheduleDropped==3);
}
'''


def host_fixture():
    prefix = playback_fixture().split('int main(){')[0]
    return prefix + HOST.replace('@TORCH@', TORCH)


class JKAParticleDynamicsTests(unittest.TestCase):
    def compile_and_run(self, body, name):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-particle-dynamics'
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

    def test_shader_lists_curves_motion_and_delays(self):
        self.compile_and_run(KERNEL, 'kernel')

    def test_actual_imports_schedule_move_and_draw_particles(self):
        self.compile_and_run(host_fixture(), 'host')

    def test_every_renderer_reads_academy_glow_stages(self):
        # jamp.exe 0x4b3fd4 and jasp.exe 0x47a188 accept "glow" as a stage
        # word; without it every glow shader (small_fire's flames) defaulted.
        branch = ('else if ( !Q_stricmp( token, "glow" ) && ( ri.gameId == COM_GAME_JKA_SP || '
                  'ri.gameId == COM_GAME_JKA_MP ) )')
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                text = (ROOT / 'code' / renderer / 'tr_shader.c').read_text(encoding='utf-8')
                self.assertEqual(text.count(branch), 1)
                start = text.index(branch)
                body = text[start:text.index('}', start)]
                self.assertIn('continue;', body)
                self.assertLess(start, text.index("WARNING: unknown parameter '%s' in shader", start))


if __name__ == '__main__':
    unittest.main()
