"""Academy tails, particle physics, impact and death effects, spawn shapes and
the curves of every moving or segment family."""
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
static bool Near(float a,float b,float tolerance=0.0001f){return std::fabs(a-b)<=tolerance;}
static float Dot(const play::Vector &a,const play::Vector &b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static const char *Spark=
 "Tail\n{\nname spark\nflags useAlpha usePhysics expensivePhysics impactFx deathFx\n"
 "spawnFlags orgOnSphere axisFromSphere absoluteAccel\ncount 3\nlife 600 900\nradius 4\nheight 8\n"
 "velocity 150 0 0 250 0 0\ngravity -400\nbounce 0.4 0.6\nwake 12\nwobble\n{\nstart 1\n}\n"
 "width\n{\nstart 1.5\nend 0.5\nflags linear\n}\nheight\n{\nstart -6\nend -18\nparms 40\nflags linear clamp\n}\n"
 "alpha\n{\nstart 1\nend 0\nflags random linear\n}\nshaders\n[\ngfx/own/spark\n]\n"
 "impactfx\n[\nown/hit\n]\ndeathfx\n[\nown/die\nown/die2\n]\n}\n";
int main(){
 play::Spec spec;const char *reason=nullptr;
 // A Tail compiles with retail's aliases: "width" is size, a "height" group
 // is length, "parms" is parm; unknown keys and groups are read past.
 assert(Compile(Spark,spec,&reason));
 assert(spec.kind==Kind::Tail&&spec.shaders==(std::vector<int>{8})&&spec.impacts==(std::vector<int>{9}));
 assert(spec.deaths==(std::vector<int>{10,11}));
 assert(spec.flags==(0x8000000u|0x2000000u|0x800000u|0x80000000u|0x20000000u)&&spec.spawnFlags==(1u|2u|0x800u));
 assert(spec.curves==(3u|0x100u|0xd000u));
 assert(spec.elasticity.first==0.4f&&spec.elasticity.last==0.6f&&spec.radius.first==4&&spec.height.first==8);
 assert(spec.length.first==-6&&spec.lengthEnd.first==-18&&spec.lengthParm.first==40&&spec.size.first==1.5f&&spec.sizeEnd.first==0.5f);
 assert(spec.ignored==(std::vector<std::string>{"wake","wobble"}));
 // "bounce" and "intensity" share one parser: elasticity and usePhysics,
 // or an Electricity's intensity.
 assert(Compile("Particle\n{\nshader gfx/own/p\nintensity 0.3\n}\n",spec)&&spec.elasticity.first==0.3f&&spec.flags==0x2000000u);
 assert(Compile("Electricity\n{\nshader gfx/own/b\nbounce 2.5\n}\n",spec)&&spec.chaos.first==2.5f&&spec.flags==0x2000000u);
 // Inert in retail multiplayer: wind, affectedByWind, an untraced
 // org2isOffset, and physics bits on a Line.
 assert(Compile("Particle\n{\nshader gfx/own/p\nwind 3\nspawnFlags affectedByWind\n}\n",spec));
 assert(Compile("Line\n{\nshader gfx/own/l\nflags usePhysics impactFx useAlpha\nspawnFlags org2isOffset\n}\n",spec)&&!spec.offsetEnd);
 assert(Compile("Particle\n{\nshader gfx/own/p\ncount 500\nflags depthHack setShaderTime\n}\n",spec));
 // Named refusals.
 struct Case{const char *text,*reason;};
 for(const Case &item:std::initializer_list<Case>{
  {"Particle\n{\nshader gfx/own/p\nflags ghoul2Collision\n}\n","effects flags are not served yet"},
  {"Particle\n{\nshader gfx/own/p\nflags relative\n}\n","effects flags are not served yet"},
  {"OrientedParticle\n{\nshader gfx/own/p\nflags setShaderTime\n}\n","effects flags are not served yet"},
  {"OrientedParticle\n{\nshader gfx/own/p\nflags depthHack\n}\n","effects flags are not served yet"},
  {"Tail\n{\nshader gfx/own/t\nflags depthHack\n}\n","effects flags are not served yet"},
  {"Line\n{\nshader gfx/own/l\nflags depthHack\n}\n","effects flags are not served yet"},
  {"Particle\n{\nshader gfx/own/p\nspawnflags randRotAroundFwd\n}\n","effects spawn controls are not served yet"},
  {"Particle\n{\nshader gfx/own/p\nmin -1 -1 -1\n}\n","effects field is outside the served playback subset"},
  {"Tail\n{\nshader gfx/own/t\nsize2\n{\nflags linear\n}\n}\n","effects curve controls are not served yet"},
  {"Particle\n{\nshader gfx/own/p\nbounce 101\n}\n","effects elasticity exceeds the Fn bound"},
  {"Particle\n{\nshader gfx/own/p\nradius 20000\n}\n","effects spawn shape exceeds the Fn bound"},
  {"Particle\n{\nshader gfx/own/p\ncount 501\n}\n","effects spawn count exceeds the Fn bound"},
  {"Particle\n{\nshader gfx/own/p\nmodel models/own/m.md3\n}\n","effects playback requires registered Fn shaders"}}){
  play::Spec old;old.shader=199;assert(!Compile(item.text,old,&reason)&&old.shader==199&&!std::strcmp(reason,item.reason));
 }
 // MakeNormalVectors (0x443840), and a zero forward gives zero rows.
 const auto normal=play::NormalAxis({0,0,1});
 assert((normal[0]==play::Vector{0,0,1})&&(normal[1]==play::Vector{1,0,0})&&(normal[2]==play::Vector{0,-1,0}));
 const auto zero=play::NormalAxis({0,0,0});for(const auto &row:zero)assert((row==play::Vector{0,0,0}));
 // orgOnSphere: a point on the ellipsoid of the radius and height, in world
 // axes; axisFromSphere points the axis, and so the velocity, out through it.
 assert(Compile(Spark,spec));play::Axis axis;assert(play::Basis({1,0,0},axis));
 std::uint32_t random=11;std::set<int> deaths;
 for(int n=0;n<500;++n){
  play::Spawn s;assert(play::Instantiate(spec,{100,0,0},axis,random,s));
  const play::Vector offset{s.origin[0]-100,s.origin[1],s.origin[2]};
  assert(Near((offset[0]*offset[0]+offset[1]*offset[1])/16+offset[2]*offset[2]/64,1,0.001f));
  const auto out=play::Normalized(offset);for(int c=0;c<3;++c)assert(Near(s.axis[0][c],out[c]));
  // Each component draws its own speed (0x408739..0x4089eb).
  for(int c=0;c<3;++c)if(std::fabs(s.axis[0][c])>0.01f){
   const float speed=s.velocity[c]/s.axis[0][c];assert(speed>=149.99f&&speed<=250.01f);}
  assert((s.acceleration==play::Vector{0,0,-400})&&s.elasticity>=0.4f&&s.elasticity<=0.6f);
  assert(s.impact==9&&s.previous==s.origin&&s.length==-6&&s.lengthEnd==-18);deaths.insert(s.death);
 }
 assert(deaths==(std::set<int>{10,11}));
 // orgOnCylinder: radius out from the play axis, height along it; the
 // cylinder's axisFromSphere crosses world up with the new forward.
 assert(Compile("Particle\n{\nshader gfx/own/p\nspawnFlags orgOnCylinder axisFromSphere\nradius 5\nheight 30\nvelocity 10 0 0\n}\n",spec));
 assert(play::Basis({0,0,1},axis));
 bool above=false,below=false;
 for(int n=0;n<500;++n){
  play::Spawn s;assert(play::Instantiate(spec,{0,0,0},axis,random,s));
  const float along=s.origin[2],around=std::sqrt(s.origin[0]*s.origin[0]+s.origin[1]*s.origin[1]);
  assert(along>=-15.001f&&along<=15.001f&&Near(around,5,0.001f));above|=along>5;below|=along<-5;
  const auto forward=play::Normalized(s.origin);for(int c=0;c<3;++c)assert(Near(s.axis[0][c],forward[c]));
  const auto right=play::Cross({0,0,1},forward);for(int c=0;c<3;++c)assert(Near(s.axis[1][c],right[c]));
  for(int c=0;c<3;++c)assert(Near(s.velocity[c],10*forward[c],0.001f));
 }
 assert(above&&below);
 // cheapOrgCalc and absoluteVel use the world's axes.
 assert(play::Basis({0,1,0},axis));
 assert(Compile("Particle\n{\nshader gfx/own/p\nspawnFlags cheapOrgCalc absoluteVel\norigin 1 2 3\nvelocity 0 0 7\n}\n",spec));
 play::Spawn cheap;assert(play::Instantiate(spec,{10,20,30},axis,random,cheap));
 assert((cheap.origin==play::Vector{11,22,33})&&(cheap.velocity==play::Vector{0,0,7}));
 assert(Compile("Particle\n{\nshader gfx/own/p\norigin 1 0 0\nvelocity 0 0 7\n}\n",spec));
 assert(play::Instantiate(spec,{10,20,30},axis,random,cheap)&&(cheap.origin==play::Vector{10,21,30}));
 assert(Near(Dot(cheap.velocity,axis[2]),7)&&Near(cheap.velocity[1],0));
 // A Line's untraced cheapOrg2Calc endpoint: the placement plus world offsets.
 assert(Compile("Line\n{\nshader gfx/own/l\norigin 0 0 5\norigin2 1 2 3\nspawnFlags cheapOrg2Calc\n}\n",spec));
 play::Spawn line;assert(play::Instantiate(spec,{10,20,30},axis,random,line)&&(line.endpoint==play::Vector{11,22,33}));
 // UpdateOrigin's step (0x401a50) and its traced outcomes.
 play::Spawn p;p.kind=Kind::Tail;p.flags=0x2000000u|0x800000u|0x80000000u;p.velocity={0,0,-400};p.elasticity=0.5f;p.origin={0,0,10};
 play::Vector next;assert(play::ParticlePredict(p,0.05f,next)&&(next==play::Vector{0,0,-10}));
 play::Contact floor;floor.fraction=0.5f;floor.endpos={0,0,0};floor.normal={0,0,1};bool impact=false;
 assert(play::ParticleCollide(p,0.05f,next,floor,impact)==play::Collision::Bounced&&impact);
 assert((p.velocity==play::Vector{0,0,200})&&p.elasticity==0.25f&&(p.origin==play::Vector{0,0,1})&&(p.flags&0x2000000u));
 // The step takes the fraction's acceleration before it reflects.
 p.velocity={0,0,-400};p.acceleration={0,0,-800};p.elasticity=0.5f;p.origin={0,0,10};
 assert(play::ParticlePredict(p,0.05f,next)&&Near(next[2],-12));
 assert(play::ParticleCollide(p,0.05f,next,floor,impact)==play::Collision::Bounced&&Near(p.velocity[2],230));
 // Below a speed of ten it rests and its physics ends.
 p.velocity={0,0,-10};p.acceleration={};p.elasticity=0.5f;
 assert(play::ParticleCollide(p,0.05f,next,floor,impact)==play::Collision::Bounced);
 assert((p.velocity==play::Vector{0,0,0})&&!(p.flags&(0x2000000u|0x80000000u))&&(p.origin==play::Vector{0,0,1}));
 // A missed step moves; starting in solid stops it where it is.
 p.flags=0x2000000u|0x800000u;p.origin={0,0,10};play::Contact clear;
 assert(play::ParticleCollide(p,0.05f,{0,0,8},clear,impact)==play::Collision::Moved&&p.origin[2]==8&&!impact);
 play::Contact solid;solid.startSolid=true;p.velocity={1,2,3};
 assert(play::ParticleCollide(p,0.05f,{0,0,4},solid,impact)==play::Collision::Stuck&&p.origin[2]==8);
 assert((p.velocity==play::Vector{0,0,0})&&!(p.flags&0x2000000u));
 // impactKills dies on the hit, after an impact unless the surface forbids it.
 p.flags=0x2000000u|0x800000u|0x40000000u|0x80000000u;
 assert(play::ParticleCollide(p,0.05f,next,floor,impact)==play::Collision::Killed&&impact);
 floor.surfaceFlags=0x80000u;assert(play::ParticleCollide(p,0.05f,next,floor,impact)==play::Collision::Killed&&!impact);
 // Only expensivePhysics always traces; plain usePhysics needs RMG (0x401afc).
 p.flags=0x2000000u;assert(!play::ParticleTraces(p,false)&&play::ParticleTraces(p,true));
 p.flags=0x2000000u|0x800000u;assert(play::ParticleTraces(p,false));p.flags=0x800000u;assert(!play::ParticleTraces(p,true));
 // CTail's length curve (0x4031c0) and endpoint (0x4033b0); a negative
 // length points along the motion.
 play::Spawn t;t.kind=Kind::Tail;t.life=1000;t.size=2;t.sizeEnd=2;t.length=-10;t.lengthEnd=-30;t.curves=1u<<12;
 t.rgbStart={1,1,1};t.alphaStart=1;t.alphaEnd=1;
 play::Look look;assert(play::ParticleLook(t,1500,1000,look)&&Near(look.length,-20)&&look.radius==2);
 t.curves=0xdu<<12;t.lengthParm=40;assert(play::ParticleLook(t,1200,1000,look)&&Near(look.length,-17));
 play::Vector end;assert(!play::TailEnd(t,-20,end));
 t.previous={0,0,4};assert(play::TailEnd(t,-20,end)&&(end==play::Vector{0,0,-20}));
 // The random word scales by a fresh draw and needs a generator.
 t.curves=2u;t.flags=0x8000000u;t.alphaEnd=0;assert(!play::ParticleLook(t,1500,1000,look));
 std::set<int> alphas;for(int n=0;n<200;++n){assert(play::ParticleLook(t,1500,1000,look,&random));alphas.insert(look.rgba[3]);}
 assert(alphas.size()>50);
 // A Line runs the particle curves: alpha linear, a width that holds until
 // its parm then shrinks.
 assert(Compile("Line\n{\nshader gfx/own/l\nlife 1000\norigin2 0 0 10\nalpha\n{\nstart 1\nend 0\nflags linear\n}\n"
  "width\n{\nstart 4\nend 2\nflags nonlinear\nparms 50\n}\n}\n",spec));
 assert(spec.curves==(1u|(4u<<8))&&spec.sizeParm.first==50);
 play::Spawn segment;assert(play::Instantiate(spec,{0,0,0},axis,random,segment));
 assert(play::ParticleLook(segment,1500,1000,look)&&look.radius==4&&look.rgba[0]==126&&look.rgba[3]==255);
 assert(play::ParticleLook(segment,1750,1000,look)&&Near(look.radius,3));
}
'''

HOST = r'''
static int traces,probes;
intptr_t QDECL VM_Call(vm_t *module,int nargs,int operation,...){
 assert(module==cgvm&&nargs==0&&operation==CG_RAVEN_TRACE);
 auto *shared=static_cast<byte*>(jkaCgameSharedBuffer);
 fntech3_jka_cgame_trace_t request;std::memcpy(&request,shared,sizeof(request));
 assert(request.skipEntity==-1&&request.mask==0x1001);
 auto response=request;response.result={};response.result.fraction=1;
 for(int n=0;n<3;++n)response.result.endpos[n]=request.end[n];
 const bool point=!std::memcmp(request.start,request.end,sizeof(request.start));
 // A floor at z=0, flagged no-impact beyond x=500.
 if(point){++probes;if(request.start[2]<0)response.result.startsolid=response.result.allsolid=1;}
 else{
  ++traces;
  if(request.end[2]<0){
   const float fraction=request.start[2]/(request.start[2]-request.end[2]);response.result.fraction=fraction;
   for(int n=0;n<3;++n)response.result.endpos[n]=request.start[n]+fraction*(request.end[n]-request.start[n]);
   response.result.endpos[2]=0;response.result.plane.normal[2]=1;
   if(request.start[0]>500)response.result.surfaceFlags=0x80000;
  }
 }
 std::memcpy(shared,&response,sizeof(response));return 0;
}
'''

MAIN = r'''
using Kind=fntech3::jka::effects::Kind;
static bool Near(float a,float b,float tolerance=0.001f){return std::fabs(a-b)<=tolerance;}
static int Handle(const char *shader){
 for(std::size_t n=0;n<shaders.size();++n)if(shaders[n]==shader)return static_cast<int>(101+n);
 return -1;
}
static std::vector<const JKACgameEffectInstance *> With(const char *shader){
 std::vector<const JKACgameEffectInstance *> found;const int handle=Handle(shader);
 for(const auto &instance:jkaCgameEffects->instances)if(instance.active&&instance.spawn.shader==handle)found.push_back(&instance);
 return found;
}
static void Play(intptr_t id,float x,float z,float direction=1){
 const float origin[]{x,0,z},forward[]{0,0,direction};
 intptr_t args[]{266,id,reinterpret_cast<intptr_t>(origin),reinterpret_cast<intptr_t>(forward),-1,-1};
 assert(InvokePlayback(args)==0);
}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 view.vieworg[0]=300;view.vieworg[2]=30;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);
 fxVariables[2].value=16;fxVariables[3].integer=0;Frame(984);Frame(1000);
 std::array<byte,FNTECH3_JKA_CGAME_SHARED_BUFFER_BYTES> scratch{};jkaCgameSharedBuffer=scratch.data();
 assets["effects/own/spark.efx"]="Tail\n{\nflags usePhysics expensivePhysics impactFx deathFx\nspawnFlags absoluteVel\n"
  "life 1000\nvelocity 0 0 -400\nbounce 0.5\nsize\n{\nstart 2\n}\nlength\n{\nstart 10\nend 20\nflags linear\n}\n"
  "shaders\n[\ngfx/own/tail\n]\nimpactfx\n[\nown/hit\n]\ndeathfx\n[\nown/die\n]\n}\n";
 assets["effects/own/hit.efx"]="Particle\n{\nlife 100\nsize\n{\nstart 3\n}\nshader gfx/own/hit\n}\n";
 assets["effects/own/die.efx"]="OrientedParticle\n{\nlife 100\nsize\n{\nstart 4\n}\nshader gfx/own/die\n}\n";
 assets["effects/own/dart.efx"]="Particle\n{\nflags usePhysics expensivePhysics impactKills impactFx deathFx\nspawnFlags absoluteVel\n"
  "life 1000\nvelocity 0 0 -1000\nshader gfx/own/dart\nimpactfx\n[\nown/hit\n]\ndeathfx\n[\nown/die\n]\n}\n";
 assets["effects/own/drop.efx"]="Particle\n{\nflags usePhysics\nspawnFlags absoluteVel\nlife 1000\nvelocity 0 0 -1000\n"
  "bounce 0.5\nshader gfx/own/drop\n}\n";
 assets["effects/own/beam.efx"]="Line\n{\nspawnFlags org2fromTrace traceImpactFx\nlife 100\nsize\n{\nstart 2\n}\n"
  "shader gfx/own/beam\nimpactfx\n[\nown/hit\n]\n}\n";
 const auto spark=Register("own/spark"),dart=Register("own/dart"),drop=Register("own/drop"),beam=Register("own/beam");
 assert(spark>0&&dart>0&&drop>0&&beam>0);
 // A tail that has not moved has no direction and draws nothing.
 Play(spark,0,50);assert(Active()==1);Draw();assert(!polygons&&!traces);
 // The next frame it falls 40 units, traced (expensivePhysics), and draws
 // back to where it was by its linear length (10 to 20; 11 at 100 ms).
 Frame(1100);Draw();assert(traces==1&&polygons==1&&vertices.size()==4);
 const auto *tail=With("gfx/own/tail").at(0);assert(tail->spawn.origin[2]==10&&tail->spawn.previous[2]==50);
 for(int n=0;n<4;++n)assert(Near(vertices[n].xyz[2],n<2?10.0f:21.0f));
 // Then it hits the floor: the impact effect plays where it hit, and it
 // bounces with half its elasticity, one unit off the plane.
 Frame(1200);Draw();assert(traces==2);
 tail=With("gfx/own/tail").at(0);assert(tail->spawn.origin[2]==1&&tail->spawn.velocity[2]==200&&tail->spawn.elasticity==0.25f);
 const auto hits=With("gfx/own/hit");assert(hits.size()==1&&Near(hits[0]->spawn.origin[2],0)&&hits[0]->startTime==1200);
 // At its end its death effect plays where it is, facing a random normal.
 assert(With("gfx/own/die").empty());const auto last=tail->spawn.origin;
 Frame(2001);Draw();assert(With("gfx/own/tail").empty());
 const auto deaths=With("gfx/own/die");assert(deaths.size()==1&&deaths[0]->spawn.origin==last);
 assert(Near(std::sqrt(deaths[0]->spawn.axis[0][0]*deaths[0]->spawn.axis[0][0]+deaths[0]->spawn.axis[0][1]*deaths[0]->spawn.axis[0][1]+
  deaths[0]->spawn.axis[0][2]*deaths[0]->spawn.axis[0][2]),1));
 // impactKills: the impact plays, the dart dies, and no death effect.
 Frame(3000);Play(dart,0,50);assert(With("gfx/own/dart").size()==1);
 Frame(3100);Draw();assert(With("gfx/own/dart").empty()&&With("gfx/own/hit").size()==1&&With("gfx/own/die").empty());
 // A no-impact surface kills without the impact.
 Frame(3500);Play(dart,600,50);Frame(3600);Draw();assert(With("gfx/own/dart").empty()&&With("gfx/own/hit").empty());
 // Plain usePhysics passes through the world while RMG is 0 (0x401afc)...
 const int traced=traces;
 Frame(4000);Play(drop,0,50);Frame(4100);Draw();
 assert(traces==traced&&!probes&&With("gfx/own/drop").at(0)->spawn.origin[2]==-50);
 // ...and with RMG set, probes the point, then traces and bounces.
 fxVariables[3].integer=1;Frame(5200);Draw();assert(With("gfx/own/drop").empty());Frame(6000);
 Play(drop,0,50);Frame(6100);Draw();
 assert(probes==1&&traces==traced+1);
 const auto drops=With("gfx/own/drop");assert(drops.size()==1&&drops[0]->spawn.origin[2]==1&&drops[0]->spawn.velocity[2]==500);
 fxVariables[3].integer=0;
 // traceImpactFx: the Line's endpoint trace ends on the floor and plays the
 // impact effect there.
 Frame(7000);Play(beam,0,50,-1);
 const auto beams=With("gfx/own/beam");assert(beams.size()==1&&Near(beams[0]->spawn.endpoint[2],0));
 const auto impacts=With("gfx/own/hit");assert(impacts.size()==1&&Near(impacts[0]->spawn.origin[2],0));
}
'''


def host_fixture():
    prefix = playback_fixture().split('int main(){')[0]
    old_trace = 'intptr_t QDECL VM_Call(vm_t*,int,int,...){assert(false);return 0;}'
    assert prefix.count(old_trace) == 1
    return prefix.replace(old_trace, HOST) + MAIN


class JKATailsPhysicsTests(unittest.TestCase):
    def compile_and_run(self, body, name):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-tails-physics'
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
                    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Werror',
                        '-D_CRT_SECURE_NO_WARNINGS', *(['-ffast-math'] if fast else []), '-I', str(ROOT),
                        str(source), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_tails_shapes_curves_and_physics(self):
        self.compile_and_run(KERNEL, 'kernel')

    def test_actual_imports_trace_bounce_impact_and_death(self):
        self.compile_and_run(host_fixture(), 'host')


if __name__ == '__main__':
    unittest.main()
