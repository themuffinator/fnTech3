"""Academy Line declarations, trace callback ownership and Fn polygon packets."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from jka_effect_play_tests import fixture as playback_fixture

ROOT = Path(__file__).resolve().parents[1]

KERNEL = r'''
#include "code/compat/jka_effect_play.hpp"
#include <cassert>
#include <iostream>
#include <iterator>
#include <limits>
#include <cstring>
namespace effect=fntech3::jka::effects;
namespace play=fntech3::jka::effectplay;
static bool Compile(const std::string &text,play::Spec &spec){
 effect::Definition definition;effect::Failure syntax;play::Failure failure;
 assert(effect::Parse(text,definition,syntax));
 definition.primitives[0].media[0].handle=8;
 return play::Compile(definition.primitives[0],spec,failure);
}
static const std::string head="Line\n{\nshader gfx/own/line\nlife 250\norigin 3 4 5\norigin2 30 40 50\n";
int main(int argc,char**){
 if(argc>1){
  const std::string text{std::istreambuf_iterator<char>(std::cin),{}};
  effect::Definition definition;effect::Failure syntax;assert(effect::Parse(text,definition,syntax));
  for(auto &primitive:definition.primitives){
   for(auto &media:primitive.media)media.handle=8;
   play::Spec spec;play::Failure failure;
   const bool served=play::Compile(primitive,spec,failure);
   std::cout<<primitive.declaration.name<<" "<<(served?"served":failure.reason)<<"\n";
  }
  return 0;
 }
 play::Spec spec;assert(Compile(head+"size\n{\nstart 9\nend 5\nparm 8\nflags wave\n}\n}\n",spec));
 play::Axis axis;assert(!play::Basis({0,0,0},axis));
 float invalidComponent;std::uint32_t invalidBits=0x7f800000u;std::memcpy(&invalidComponent,&invalidBits,4);
 assert(!play::Basis({invalidComponent,0,0},axis));
 invalidBits=0x7fc00001u;std::memcpy(&invalidComponent,&invalidBits,4);
 assert(!play::Basis({invalidComponent,0,0},axis));
 const float largest=std::numeric_limits<float>::max();
 assert(play::Basis({largest,largest,largest},axis));
 assert(play::Basis({1,0,0},axis));std::uint32_t random=5;play::Spawn spawn;
 assert(play::Instantiate(spec,{10,20,30},axis,random,spawn));
 assert((spawn.origin==play::Vector{13,24,35}));
 // Explicit origin2 is relative to the placement, not the offset start.
 assert((spawn.endpoint==play::Vector{40,60,80}));
 assert(spawn.sizeWave&&!spawn.traceEnd&&spawn.life==250);
 assert(std::fabs(spawn.sizeRate-0.02513272129f)<1e-9f);
 assert(play::LineRadius(spawn,0)==9);
 assert(std::fabs(play::LineRadius(spawn,125)-1)<0.00001f);
 assert(std::fabs(play::LineRadius(spawn,250)-9)<0.00001f);
 // Independently executed retail0x401e50 wave vectors, including overshoot.
 const float retail[]{8,7.61803436279f,6.6180357933f,5.38196897507f,4.38196849823f,
  4,4.38196277618f,5.38195943832f,6.61802625656f};
 spawn.size=8;spawn.sizeEnd=6;
 for(int n=0;n<9;++n)assert(std::fabs(play::LineRadius(spawn,n*25)-retail[n])<0.00001f);
 assert(Compile(head+"spawnflags org2fromTrace\n}\n",spec));
 assert(play::Instantiate(spec,{10,20,30},axis,random,spawn));
 assert(spawn.traceEnd&&(spawn.endpoint==play::Vector{16397,24,35}));
 play::Vector side{};
 assert(play::LineSide({0,0,0},{100,0,0},{0,1,0},side));
 assert(side[0]==0&&side[1]==0&&side[2]==-1);
 assert(play::LineSide({0,0,0},{100,0,0},{0,0,0},side));
 assert(side[0]==0&&side[1]==0&&side[2]==-1);
 assert(!play::LineSide({0,0,0},{0,0,0},{0,1,0},side));
 unsigned nan=0x7fc00001u;play::Vector invalid{};std::memcpy(invalid.data(),&nan,4);
 assert(!play::LineSide(invalid,{1,0,0},{0,1,0},side));
 for(const char *extra:{"spawnflags cheapOrg2Calc","spawnflags org2fromTrace traceImpactFx",
  "size\n{\nflags wave linear\n}","size\n{\nparm 10001\nflags wave\n}",
  "origin2 20000 0 0","count 101","life 0","delay 1","flags depthHack"}){
  play::Spec old;old.shader=199;assert(!Compile(head+extra+"\n}\n",old));assert(old.shader==199);
 }
}
'''

TRACE = r'''
static int traceCalls,traceFailure;static fntech3_jka_cgame_trace_t observedTrace;
static void (*traceMutation)();
intptr_t QDECL VM_Call(vm_t *module,int nargs,int operation,...){
 assert(module==cgvm&&nargs==0&&operation==CG_RAVEN_TRACE);++traceCalls;
 auto *shared=static_cast<byte*>(jkaCgameSharedBuffer);
 std::memcpy(&observedTrace,shared,sizeof(observedTrace));
 assert(observedTrace.skipEntity==-1&&observedTrace.mask==0x1001);
 for(int n=0;n<3;++n)assert(observedTrace.mins[n]==0&&observedTrace.maxs[n]==0);
 auto response=observedTrace;response.result.fraction=0.5f;
 for(int n=0;n<3;++n)response.result.endpos[n]=observedTrace.start[n]+0.5f*(observedTrace.end[n]-observedTrace.start[n]);
 if(traceFailure==1)response.result.fraction=2;
 if(traceFailure==2){const unsigned nan=0x7fc00001u;std::memcpy(response.result.endpos,&nan,4);}
 if(traceFailure==3)Com_Error(ERR_DROP,"trace callback failure");
 if(traceFailure==4)jkaCgameSharedBuffer=nullptr;
 if(traceMutation)traceMutation();
 std::memcpy(shared,&response,sizeof(response));return 0;
}
'''

HOST_CHECKS = r'''
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);Frame(500);
 // Deliberately unaligned shared record: all publication must use memcpy.
 std::array<byte,FNTECH3_JKA_CGAME_SHARED_BUFFER_BYTES+2> scratch;scratch.fill(0xa5);
 jkaCgameSharedBuffer=scratch.data()+1;
 assets["effects/own/line.efx"]="Line\n{\nshader gfx/own/line\nlife 250\norigin 3 0 0\nspawnflags org2fromTrace\nsize\n{\nstart 9\nend 5\nparm 8\nflags wave\n}\n}\n";
 const auto id=Register("own/line");const float origin[]{100,200,300},forward[]{1,0,0};
 intptr_t args[]{266,id,reinterpret_cast<intptr_t>(origin),reinterpret_cast<intptr_t>(forward),-1,-1};
 assert(InvokePlayback(args)==0&&Active()==1&&traceCalls==1);
 assert(observedTrace.start[0]==103&&observedTrace.end[0]==16487);
 assert(scratch.front()==0xa5&&scratch.back()==0xa5);
 for(unsigned n=105;n<scratch.size();++n)assert(scratch[n]==0xa5);
 const auto &spawn=jkaCgameEffects->instances[0].spawn;assert(spawn.endpoint[0]==8295);
 // A Line starting near the eye must still span toward its distant end.
 std::memcpy(view.vieworg,spawn.origin.data(),sizeof(view.vieworg));view.vieworg[1]+=1;fxVariables[2].value=16;
 Draw();assert(sprites==0&&polygons==1&&vertices.size()==4);
 assert(vertices[0].xyz[0]==103&&vertices[1].xyz[0]==103&&vertices[2].xyz[0]==8295&&vertices[3].xyz[0]==8295);
 assert(std::fabs(vertices[1].xyz[2]-vertices[0].xyz[2]-18)<0.0001f); // Winding faces the eye.
 Frame(625);Draw();assert(polygons==2&&vertices.size()==8);
 assert(std::fabs(vertices[5].xyz[2]-vertices[4].xyz[2]-2)<0.0001f);
 Frame(750);Draw();assert(Active()==1&&polygons==3);
 Frame(751);Draw();assert(!Active()&&polygons==3);
 // Trace failures publish neither a partial primitive nor a random-state step.
 Frame(800);const auto seed=jkaCgameEffects->random;
 for(int failure:{1,2,4}){
  traceFailure=failure;jkaCgameSharedBuffer=scratch.data()+1;
  Reject([&]{InvokePlayback(args);});assert(!Active()&&jkaCgameEffects->random==seed);
 }
 traceFailure=0;jkaCgameSharedBuffer=nullptr;Reject([&]{InvokePlayback(args);});assert(!Active());
 jkaCgameSharedBuffer=scratch.data()+1;
 // Compile the whole graph before callbacks: unsupported siblings are honest.
 assets["effects/own/mixed.efx"]=assets["effects/own/line.efx"]+"Emitter\n{\nemitfx own/child\n}\n";
 const auto oldCalls=traceCalls;args[1]=Register("own/mixed");Reject([&]{InvokePlayback(args);});
 assert(!Active()&&traceCalls==oldCalls);
 // Cleanup may retain/reuse the state address while invalidating the spec.
 // Every mutation must be caught before the enclosing spawn loop resumes.
 for(int mutation=0;mutation<5;++mutation){
  CL_JKACgameEffectsFree();System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view));Frame(900);
  args[1]=Register("own/line");jkaCgameSharedBuffer=scratch.data()+1;
  if(mutation==0)traceMutation=[] {CL_JKACgameEffectsFree();};
  if(mutation==1)traceMutation=[] {CL_JKACgameEffectsInit(0);};
  if(mutation==2)traceMutation=[] {CL_JKACgameEffectsClear();};
  if(mutation==3)traceMutation=[] {CL_JKACgameEffectsAdjustTime(1000);};
  if(mutation==4)traceMutation=[] {intptr_t call[]{0},result=0;CL_JKACgameEffectsCall(call,Call::CG_FX_FREE_SYSTEM,result);};
  recover=true;Reject([&]{InvokePlayback(args);});recover=false;traceMutation=nullptr;
  assert(!Active()&&!jkaCgameEffects&&!jkaCgameEffectsTracing);
 }
 System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view));Frame(1000);
 args[1]=Register("own/line");jkaCgameSharedBuffer=scratch.data()+1;
 traceFailure=3;recover=jumpRecovery=true;
 if(!setjmp(recoveryPoint)){InvokePlayback(args);assert(false);}
 recover=jumpRecovery=false;
 assert(!jkaCgameEffects&&!jkaCgameSharedBuffer&&files.empty()&&opens==closes);
}
'''


def host_fixture():
    prefix = playback_fixture().split('int main(){')[0]
    old = 'intptr_t QDECL VM_Call(vm_t*,int,int,...){assert(false);return 0;}'
    assert prefix.count(old) == 1
    return prefix.replace(old, TRACE) + HOST_CHECKS


class JKALineEffectTests(unittest.TestCase):
    def compile_and_run(self, body):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-line-effects'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / 'line.cpp'
            source.write_text(body)
            for fast in (False, True):
                with self.subTest(fast_math=fast):
                    executable = directory / (('fast' if fast else 'plain') + ('.exe' if os.name == 'nt' else ''))
                    target = compiler_target_args(compiler)
                    if os.name == 'nt' and not target:
                        target = ['-DFNTECH3_Q3_ONLY']
                    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Werror',
                        '-D_CRT_SECURE_NO_WARNINGS', *(['-ffast-math'] if fast else []), '-I', str(ROOT),
                        str(source), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_line_contract_and_degenerate_geometry(self):
        self.compile_and_run(KERNEL)

    def test_actual_trace_callback_polygons_and_error_cleanup(self):
        self.compile_and_run(host_fixture())


if __name__ == '__main__':
    unittest.main()
