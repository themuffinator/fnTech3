"""Academy Emitter, Light, FxRunner and Electricity playback (the spawn effect chain)."""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from jka_effect_play_tests import fixture as playback_fixture

ROOT = Path(__file__).resolve().parents[1]

# Independently authored declarations with the shape of the retail spawn chain
# (a beam that climbs, sheds lights and throws bolts); no retail text is used.
CHAIN = {
    'own/spawn': 'Emitter\n{\nflags usePhysics emitFx expensivePhysics impactKills\nlife 8000\n'
                 'velocity 3000 0 0\ndensity 100\nemitfx\n[\nown/lights\n]\n}\n',
    'own/lights': 'Light\n{\nlife 40\nsize\n{\nstart 300\n}\nrgb\n{\nstart 0.1 0.5 0.4\n}\n}\n'
                  'FxRunner\n{\ncount 1\nplayFx\n[\nown/bolts\n]\n}\n',
    'own/bolts': 'Electricity\n{\ncount 1\nlife 125\nspawnflags org2fromTrace org2isOffset\n'
                 'origin2 0 -12000 12000\nflags useModel useBBox\nintensity 2\n'
                 'rgb\n{\nstart 0.2 1 1\n}\nsize\n{\nstart 4\nend 1\nflags linear\n}\n'
                 'shaders\n[\ngfx/own/bolt\n]\n}\n',
}


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def reference_emissions(frames):
    """Restate the measured walk (jamp.exe 0x4038f0) for a 3000u/s climb."""
    base = origin = 0.0
    previous, velocity, emitted = 1000, 3000.0, []
    for frame in range(frames):
        now = 1000 + 16 * frame
        base = origin
        if now > 1000:
            origin = f32(origin + f32(0.016) * velocity)
        time, elapsed = previous, 0
        while time <= now:
            elapsed += 12
            seconds = f32(f32(elapsed) * f32(0.001))
            half = f32(f32(seconds * seconds) * 0.5)
            position = f32(half * velocity + seconds * velocity + base)
            if (base - position) ** 2 >= 10000:
                emitted.append((now, position))
                base, elapsed, previous = position, 0, time
            time += 12
    return emitted


KERNEL = r'''
#include "code/compat/jka_effect_play.hpp"
#include <cassert>
#include <cstring>
#include <vector>
namespace effect=fntech3::jka::effects;
namespace play=fntech3::jka::effectplay;
static bool Compile(const std::string &text,play::Spec &spec,const char **reason=nullptr){
 effect::Definition definition;effect::Failure syntax;play::Failure failure;
 assert(effect::Parse(text,definition,syntax));
 for(auto &media:definition.primitives[0].media)media.handle=8;
 const bool served=play::Compile(definition.primitives[0],spec,failure);
 if(reason)*reason=failure.reason;return served;
}
static bool Near(float a,float b,float tolerance=0.0001f){return std::fabs(a-b)<=tolerance;}
int main(){
 play::Spec spec;const char *reason=nullptr;
 // Served declarations, flag words without case, and their compiled data.
 assert(Compile("Emitter\n{\nflags usePhysics EMITFX expensivePhysics impactkills\nlife 8000\nvelocity 3000 0 0\n"
  "density 100\nvariance 5\nemitfx\n[\nown/a\nown/b\n]\n}\n",spec));
 assert(spec.kind==effect::Kind::Emitter&&spec.flags==(0x2000000u|0x10000000u|0x800000u|0x40000000u));
 assert(spec.children.size()==2&&spec.density.first==100&&spec.variance.first==5&&spec.velocity[0].first==3000);
 assert(Compile("Light\n{\nlife 40\nsize\n{\nstart 300\nend 10\nflags linear\n}\nrgb\n{\nstart 0.1 0.5 0.4\n}\nalpha\n{\nstart 0.5\n}\n}\n",spec));
 assert(spec.sizeLinear&&spec.size.first==300&&spec.sizeEnd.first==10);
 assert(Compile("FxRunner\n{\ncount 0 0.6\nplayFx\n[\nown/a\n]\n}\n",spec));
 assert(spec.kind==effect::Kind::Runner&&spec.count.last==0.6f&&spec.children.size()==1);
 assert(Compile("Electricity\n{\ncount 0 0.6\nlife 125 225\nspawnflags org2fromTrace org2isOffset\n"
  "origin2 0 -12000 12000 0 -18000 18000\nflags useModel useBBox\nintensity 2.0 3.5\n"
  "rgb\n{\nstart 0.2 1.0 1.0 0.2 1.0 0.7\n}\nsize\n{\nstart 4.0 7.0\nend 1.0\nflags linear\n}\n"
  "shaders\n[\ngfx/own/bolt\n]\n}\n",spec));
 // "intensity" also sets usePhysics, which Electricity takes as forks (0x40cdca).
 assert(spec.traceEnd&&spec.offsetEnd&&spec.sizeLinear&&spec.flags==(0x1000000u|0x4000000u|0x2000000u));
 assert(spec.endpoint[1].first==-12000&&spec.endpoint[1].last==-18000&&spec.chaos.last==3.5f&&spec.life.last==225);
 // A Line now serves traced offsets through the same endpoint path.
 assert(Compile("Line\n{\nshader gfx/own/line\nspawnflags org2fromTrace org2isOffset\norigin2 0 10 0\n}\n",spec));
 // A flag word retail does not know sets nothing and is read past (0x40d79a).
 assert(Compile("Emitter\n{\nflags emitFx madeUp\n}\n",spec)&&spec.flags==0x10000000u);
 assert(spec.unknownWords==(std::vector<std::string>{"madeup"}));
 // Named refusals for controls this subset does not serve.
 for(const char *text:{
  "Emitter\n{\nflags usePhysics impactKills\n}\n",            // retail's cheap point-contents probe
  "Emitter\n{\nflags usePhysics expensivePhysics\n}\n",       // bounce
  "Emitter\n{\nflags emitFx useBBox\n}\n",
  "Emitter\n{\nspawnflags randRotAroundFwd\n}\n",
  "Emitter\n{\nacceleration 0 0 10\n}\n",
  "Emitter\n{\nangles 0 90 0\n}\n",
  "Emitter\n{\nmodel models/own/chunk.md3\n}\n",
  "Emitter\n{\nsize\n{\nstart 3\n}\n}\n",
  "Light\n{\nshader gfx/own/light\n}\n",
  "Light\n{\nsize\n{\nstart 3\nflags wave\n}\n}\n",
  "Light\n{\nlength\n{\nstart 3\n}\n}\n",
  "FxRunner\n{\nshader gfx/own/runner\n}\n",
  "FxRunner\n{\nemitfx\n[\nown/a\n]\n}\n",
  "FxRunner\n{\ndelay 60001\n}\n",                            // beyond the Fn delay bound
  "Electricity\n{\nshader gfx/own/bolt\nflags depthHack\n}\n",
  "Electricity\n{\nshader gfx/own/bolt\nspawnflags orgOnSphere randRotAroundFwd\n}\n",
  "Electricity\n{\nshader gfx/own/bolt\nintensity 65\n}\n",
  "Electricity\n{\nshader gfx/own/bolt\nlength\n{\nstart 3\n}\n}\n",
  "Particle\n{\nshader gfx/own/p\ncount 501\n}\n",
  "Flash\n{\nshader gfx/own/t\n}\n"}){                            // a family still unserved
  play::Spec old;old.shader=199;assert(!Compile(text,old,&reason)&&old.shader==199&&reason);
 }
 // Retail count rule (0x4072b1..0x4073ba).
 std::uint32_t random=7;play::Spec counted;
 counted.count={0,0.6f};int ones=0;
 for(int n=0;n<6000;++n){const int count=play::Count(counted,1,random);assert(count==0||count==1);ones+=count;}
 assert(ones>800&&ones<1200); // floor(v+0.5) is one for v>=0.5: a sixth.
 counted.count={2,2};assert(play::Count(counted,0,random)==2);       // no spread: unscaled
 counted.count={10,10.5f};assert(play::Count(counted,0,random)>=10); // spread of a half: unscaled
 counted.count={10,20};for(int n=0;n<50;++n)assert(play::Count(counted,0,random)==1); // scaled to 0, minimum one
 counted.count={0,20};for(int n=0;n<50;++n)assert(play::Count(counted,0,random)==0);
 counted.count={20,20.0f};assert(play::Count(counted,5,random)==20);
 counted.count={0,40};for(int n=0;n<200;++n)assert(play::Count(counted,5,random)<=40); // scale clamps to one
 counted.count={0.4f,0.4f};assert(play::Count(counted,1,random)==0);
 counted.count={0.5f,0.5f};assert(play::Count(counted,1,random)==1);
 // An emitter's axis comes from the play axis's angles, roll zero.
 play::Axis axis;assert(play::Basis({0,0,1},axis));
 const auto up=play::AnglesAxis(play::DirectionAngles(axis[0]));
 const float expected[3][3]{{0,0,1},{0,1,0},{-1,0,0}};
 for(int r=0;r<3;++r)for(int n=0;n<3;++n)assert(Near(up[r][n],expected[r][n]));
 const auto east=play::AnglesAxis(play::DirectionAngles({0,1,0}));
 assert(Near(east[0][1],1)&&Near(east[1][0],-1)&&Near(east[2][2],1));
 // Emitter spawn: velocity along the play axis, children picked once.
 play::Spec emitter;assert(Compile("Emitter\n{\nflags usePhysics emitFx expensivePhysics impactKills\nlife 8000\n"
  "velocity 3000 0 0\ndensity 100\nemitfx\n[\nown/a\n]\n}\n",emitter));
 play::Spawn spawn;random=11;assert(play::Instantiate(emitter,{0,0,0},axis,random,spawn));
 assert(spawn.velocity[2]==3000&&spawn.velocity[0]==0&&spawn.child==8&&spawn.density==100&&spawn.life==8000);
 // Walk: the measured prediction from this frame's start, 12ms ticks.
 spawn.previousTime=1000;std::vector<float> emitted;
 for(int frame=0;frame<12;++frame){
  const int now=1000+16*frame;spawn.base=spawn.origin;spawn.baseVelocity=spawn.velocity;
  if(now>1000){play::Vector next;assert(play::EmitterNext(spawn,0.016f,next));spawn.origin=next;}
  assert(play::EmitterWalk(spawn,now,random,[&](const play::Vector &position){
   assert(position[0]==0&&position[1]==0);emitted.push_back(float(now));emitted.push_back(position[2]);return true;}));
 }
 static const float reference[]{@REFERENCE@};
 assert(emitted.size()==sizeof(reference)/sizeof(reference[0]));
 for(std::size_t n=0;n<emitted.size();++n)assert(Near(emitted[n],reference[n],0.001f));
 // A refusing callback stops the walk; a later start time emits nothing.
 spawn.previousTime=5000;assert(play::EmitterWalk(spawn,4000,random,[](const play::Vector &){assert(false);return false;}));
 // Light and Electricity sizes: linear from start to end over the lifetime.
 play::Spawn sized;sized.size=4;sized.sizeEnd=1;sized.life=125;sized.sizeLinear=true;
 assert(play::CurveRadius(sized,0)==4&&Near(play::CurveRadius(sized,125),1)&&Near(play::CurveRadius(sized,25),3.4f));
 sized.sizeLinear=false;assert(play::CurveRadius(sized,100)==4);
 // Electricity endpoints: traced along forward, offsets once per component.
 play::Spec bolt;assert(Compile("Electricity\n{\nshader gfx/own/bolt\nlife 100\nspawnflags org2fromTrace org2isOffset\n"
  "origin2 0 -12000 12000\nflags useModel useBBox\nintensity 2\nsize\n{\nstart 4\nend 1\nflags linear\n}\n}\n",bolt));
 play::Spawn arc;assert(play::Instantiate(bolt,{10,20,30},up,random,arc));
 assert(arc.traceEnd&&Near(arc.endpoint[0],10-12000)&&Near(arc.endpoint[1],20-12000)&&Near(arc.endpoint[2],30+16384,0.01f));
 assert(arc.chaos==2&&arc.sizeLinear&&arc.life==100);
 // Fn's bolt: seeded, ends pinned, grows over half its life, tapers.
 arc.endpoint={10,20,1030};play::Bolt points,again;
 assert(play::BoltPoints(arc,0,points)==0);
 assert(play::BoltPoints(arc,50,points)==play::BoltSegments&&play::BoltPoints(arc,50,again)==play::BoltSegments);
 assert(points==again&&points[0]==arc.origin&&points[play::BoltSegments]==arc.endpoint);
 bool jagged=false;for(const auto &point:points)jagged|=point[0]!=10||point[1]!=20;assert(jagged);
 assert(play::BoltPoints(arc,10,points)==4); // 3.2 segments: three whole and a part
 assert(Near(points[4][2],points[3][2]+(again[4][2]-again[3][2])*0.2f,0.01f));
 assert(play::BoltWidth(arc,4,0)==4&&play::BoltWidth(arc,4,play::BoltSegments)==0);
 // Forks: two branches leave at segments 5 and 10 once the bolt has passed them.
 play::Bolt branch;int from=0;
 assert(play::BranchPoints(arc,10,0,branch,from)==0&&from==5); // reach 3.2
 assert(play::BranchPoints(arc,20,0,branch,from)==2&&from==5); // reach 6.4: 1.4 of 11 segments, scaled to 8
 assert(play::BranchPoints(arc,50,1,branch,from)==play::BranchSegments&&from==10);
 assert(branch[0]==again[10]&&branch[play::BranchSegments]!=again[play::BoltSegments]);
 assert(play::BranchWidth(arc,4,5,0)==4*(1-5.0f/16)*0.7f&&play::BranchWidth(arc,4,5,play::BranchSegments)==0);
 assert(play::BranchPoints(arc,50,2,branch,from)==0);
 arc.flags&=~0x2000000u;assert(play::BranchPoints(arc,50,0,branch,from)==0);
 arc.flags=0;assert(play::BoltPoints(arc,0,points)==play::BoltSegments&&play::BoltWidth(arc,4,play::BoltSegments)==4);
 arc.endpoint=arc.origin;assert(play::BoltPoints(arc,50,points)==0);
 // Light spawn carries its RGB alone, not alpha-weighted bytes.
 play::Spec glow;assert(Compile("Light\n{\nlife 40\nsize\n{\nstart 300\n}\nrgb\n{\nstart 0.1 0.5 0.4\n}\nalpha\n{\nstart 0.5\n}\n}\n",glow));
 play::Spawn light;assert(play::Instantiate(glow,{1,2,3},up,random,light));
 assert(light.light[0]==0.1f&&light.light[1]==0.5f&&light.light[2]==0.4f&&light.size==300&&light.life==40);
}
'''

LIGHTS = r'''
struct LightCall{float origin[3],radius,r,g,b;};static std::vector<LightCall> lights;
static void AddEffectLight(const vec3_t origin,float radius,float r,float g,float b){
 lights.push_back({{origin[0],origin[1],origin[2]},radius,r,g,b});
}
'''

TRACE = r'''
static int endpointTraces,physicsTraces;static float ceiling=1000;static bool solidStart;
intptr_t QDECL VM_Call(vm_t *module,int nargs,int operation,...){
 assert(module==cgvm&&nargs==0&&operation==CG_RAVEN_TRACE);
 auto *shared=static_cast<byte*>(jkaCgameSharedBuffer);
 fntech3_jka_cgame_trace_t request;std::memcpy(&request,shared,sizeof(request));
 assert(request.skipEntity==-1&&request.mask==0x1001);
 for(int n=0;n<3;++n)assert(request.mins[n]==0&&request.maxs[n]==0);
 double span=0;for(int n=0;n<3;++n)span+=double(request.end[n]-request.start[n])*(request.end[n]-request.start[n]);
 auto response=request;response.result={};response.result.fraction=1;
 if(span>1000.0*1000.0){++endpointTraces;response.result.fraction=0.25f;}
 else{++physicsTraces;if(request.end[2]>ceiling)response.result.fraction=0.5f;
  if(solidStart){response.result.startsolid=1;response.result.fraction=0;}}
 for(int n=0;n<3;++n)response.result.endpos[n]=request.start[n]+response.result.fraction*(request.end[n]-request.start[n]);
 std::memcpy(shared,&response,sizeof(response));return 0;
}
'''

HOST = r'''
static int Count(fntech3::jka::effects::Kind kind){
 int count=0;for(const auto &instance:jkaCgameEffects->instances)count+=instance.active&&instance.spawn.kind==kind;return count;
}
static const fntech3::jka::effectplay::Spawn *First(fntech3::jka::effects::Kind kind){
 for(const auto &instance:jkaCgameEffects->instances)if(instance.active&&instance.spawn.kind==kind)return &instance.spawn;
 return nullptr;
}
using Kind=fntech3::jka::effects::Kind;
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 view.vieworg[0]=400;view.vieworg[1]=0;view.vieworg[2]=100;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);Frame(1000);
 std::array<byte,FNTECH3_JKA_CGAME_SHARED_BUFFER_BYTES> scratch{};jkaCgameSharedBuffer=scratch.data();
 @ASSETS@
 const auto id=Register("own/spawn");assert(id>0&&shaders.size()==1&&shaders[0]=="gfx/own/bolt");
 const float origin[]{0,0,0},forward[]{0,0,1};
 intptr_t args[]{266,id,reinterpret_cast<intptr_t>(origin),reinterpret_cast<intptr_t>(forward),-1,-1};
 assert(InvokePlayback(args)==0&&Active()==1&&Count(Kind::Emitter)==1);
 for(int n=1;n<=3;++n)assert(jkaCgameEffects->effects[n-1]->playable); // whole graph compiled
 assert(!endpointTraces&&!physicsTraces);
 // Creation frame: no motion, no emission, nothing drawn.
 Draw();assert(!physicsTraces&&Active()==1&&lights.empty()&&!polygons&&!sprites);
 static const float reference[]{@REFERENCE@};std::size_t next=0;
 int bolts=0;
 for(int frame=1;frame<12;++frame){
  const int now=1000+16*frame;Frame(now);
  const auto lightsBefore=lights.size();const auto emittedBefore=Count(Kind::Light);
  Draw();assert(physicsTraces==frame);
  const auto *emitter=First(Kind::Emitter);assert(emitter&&std::fabs(emitter->origin[2]-float(16*frame)*3)<0.001f);
  while(next<sizeof(reference)/sizeof(reference[0])&&reference[next]==float(now)){
   // One Light at the emission point and one traced bolt from it.
   bool found=false;
   for(const auto &call:lights)found|=std::fabs(call.origin[2]-reference[next+1])<0.001f&&call.radius==300&&
    call.r==0.1f&&call.g==0.5f&&call.b==0.4f&&call.origin[0]==0&&call.origin[1]==0;
   assert(found);next+=2;++bolts;
  }
  assert(endpointTraces==bolts&&lights.size()>=lightsBefore);(void)emittedBefore;
 }
 assert(next==sizeof(reference)/sizeof(reference[0])&&bolts>=6);
 // Bolts: endpoints traced from the emission point, drawn as tapered strips.
 const auto *arc=First(Kind::Electricity);assert(arc);
 assert(std::fabs(arc->endpoint[0]+3000)<0.01f&&std::fabs(arc->endpoint[1]+3000)<0.01f);
 assert(polygons>0&&vertices.size()==std::size_t(polygons)*4);
 // Lights live 40ms and bolts their own lifetimes, inclusive.
 for(const auto &instance:jkaCgameEffects->instances)if(instance.active)
  assert(jkaCgameEffects->time-instance.startTime<=instance.spawn.life&&(instance.spawn.kind!=Kind::Light||instance.spawn.life==40));
 // The climb hits the ceiling: the emitter dies with no emission that frame.
 int now=1000+16*12;
 for(;Count(Kind::Emitter)&&now<9000;now+=16){Frame(now);Draw();}
 assert(!Count(Kind::Emitter)&&now<1600);
 for(const auto &instance:jkaCgameEffects->instances)assert(!instance.active||instance.startTime<now-16);
 // A solid start stops the emitter without killing it.
 CL_JKACgameEffectsFree();System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view));Frame(20000);
 args[1]=Register("own/spawn");assert(InvokePlayback(args)==0);Draw();
 solidStart=true;Frame(20016);Draw();solidStart=false;
 const auto *stopped=First(Kind::Emitter);assert(stopped&&stopped->origin[2]==0&&stopped->velocity[2]==0);
 assert(!(stopped->flags&0x2000000u));const auto traces=physicsTraces;
 Frame(20032);Draw();assert(physicsTraces==traces&&First(Kind::Emitter)->origin[2]==0);
 // Nesting is bounded: an FxRunner cycle refuses instead of recursing.
 assets["effects/own/loop.efx"]="FxRunner\n{\nplayfx\n[\nown/loop\n]\n}\n";
 args[1]=Register("own/loop");const auto before=Active();
 Reject([&]{InvokePlayback(args);});assert(Active()==before);
 // An unserved descendant refuses the whole graph before any callback.
 assets["effects/own/badroot.efx"]="Line\n{\nshader gfx/own/line\nspawnflags org2fromTrace\n}\n"
  "FxRunner\n{\nplayfx\n[\nown/badchild\n]\n}\n";
 assets["effects/own/badchild.efx"]="Flash\n{\nshader gfx/own/flash\n}\n";
 const auto oldTraces=endpointTraces;args[1]=Register("own/badroot");
 Reject([&]{InvokePlayback(args);});assert(endpointTraces==oldTraces&&Active()==before);
 // A missing light service refuses before submitting anything.
 CL_JKACgameEffectsFree();System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view));Frame(30000);
 args[1]=Register("own/lights");assert(InvokePlayback(args)==0&&Count(Kind::Light)==1&&Count(Kind::Electricity)==1);
 const auto service=re.AddLightToScene;re.AddLightToScene=nullptr;const auto polys=polygons;
 Reject([&]{Draw();});assert(polygons==polys);re.AddLightToScene=service;
 Draw();assert(polygons==polys&&lights.back().radius==300); // the bolt has not grown yet
 Frame(30016);Draw();assert(polygons>polys);
}
'''


def host_fixture():
    prefix = playback_fixture().split('int main(){')[0]
    old_trace = 'intptr_t QDECL VM_Call(vm_t*,int,int,...){assert(false);return 0;}'
    old_light = 'static void AddEffectLight(const vec3_t,float,float,float,float){assert(false);}'
    assert prefix.count(old_trace) == 1 and prefix.count(old_light) == 1
    prefix = prefix.replace(old_light, LIGHTS).replace(old_trace, TRACE)
    reference = ','.join(f'{value!r}f' for item in reference_emissions(12) for value in (float(item[0]), item[1]))
    assets = ''.join(f'assets["effects/{name}.efx"]="{text.encode("unicode_escape").decode()}";'
                     for name, text in CHAIN.items())
    return prefix + HOST.replace('@REFERENCE@', reference).replace('@ASSETS@', assets)


def kernel():
    reference = ','.join(f'{value!r}f' for item in reference_emissions(12) for value in (float(item[0]), item[1]))
    return KERNEL.replace('@REFERENCE@', reference)


class JKASpawnEffectTests(unittest.TestCase):
    def compile_and_run(self, body, name):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-spawn-effects'
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

    def test_reference_walk_restates_the_measured_spacing(self):
        emitted = reference_emissions(12)
        self.assertEqual([time for time, _ in emitted], [1032, 1048, 1080, 1096, 1128, 1144, 1176])
        self.assertAlmostEqual(emitted[0][1], 157.944, places=3)

    def test_declarations_count_rule_walk_and_bolt_geometry(self):
        self.compile_and_run(kernel(), 'kernel')

    def test_actual_imports_emit_lights_and_traced_bolts(self):
        self.compile_and_run(host_fixture(), 'host')


if __name__ == '__main__':
    unittest.main()
