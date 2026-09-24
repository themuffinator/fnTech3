"""Original IHV trace decisions through actual server code and native records.

The optional reference host supplies numeric digests for synthetic contacts and
CM results. These tests establish server aggregation, not BSP collision parity.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/server/server.h"
#include "code/server/server_game_data.h"
#include "code/server/server_world_bounds.hpp"
#include "code/server/sv_collision_model.hpp"
#include "code/qcommon/cm_model_handles.h"
#include "code/compat/q3_ihv_collision.hpp"
#include "code/compat/q3_ihv_state_adapter.hpp"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/stvef_game_abi.h"
#include "tests/q3_ihv_trace_cases.hpp"
#include <cstdio>
#include <limits>
#include <stdexcept>
server_t sv{};
static comGameProfile_t profile{};
const comGameProfile_t *Com_GameProfile(){return &profile;}
int Com_GameIsWolfET(){return profile.id==COM_GAME_WOLFET;}
struct Drop{};
void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
const vec3_t vec3_origin{};
namespace tc=ihv_trace_cases;
static const tc::Case *currentCase;
static tc::Digest digest;
static int worldCalls,contactCalls,entityCalls,pointCalls,worldPointCalls;
static std::vector<int> tracedEntities;
static std::array<float,3> lastTraceOrigin,lastTraceAngles,lastPointOrigin,lastPointAngles;
static_assert(sizeof(trace_t)==sizeof(tc::Trace));
static_assert(offsetof(trace_t,plane)==offsetof(tc::Trace,planeNormal));
static_assert(offsetof(trace_t,entityNum)==offsetof(tc::Trace,entityNum));
static void Check(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
int SV_AreaEntities(const vec3_t mins,const vec3_t maxs,int *out,int maximum){
 Check(maximum==1024,"Contact limit");++contactCalls;digest.Word(2);digest.Vector(mins);digest.Vector(maxs);
 for(int i=0;i<currentCase->count;++i)out[i]=currentCase->order[i];return currentCase->count;
}
clipHandle_t SV_ClipHandleForEntity(const sharedEntity_t *e,qboolean capsule){
 Check(!capsule,"Box model");return 64+SV_NumForGentity(const_cast<sharedEntity_t*>(e));
}
void CM_BoxTrace(trace_t *out,const vec3_t start,const vec3_t end,const vec3_t mins,const vec3_t maxs,int model,int mask,qboolean capsule){
 Check(!model&&!capsule&&mask==currentCase->mask,"World model/mask");++worldCalls;digest.Word(1);
 digest.Vector(start);digest.Vector(end);digest.Vector(mins);digest.Vector(maxs);std::memcpy(out,&currentCase->world,sizeof(*out));
}
void CM_SetTempBoxModelContents(int){}
void CM_TransformedBoxTrace(trace_t *out,const vec3_t start,const vec3_t end,const vec3_t mins,const vec3_t maxs,int model,int mask,const vec3_t origin,const vec3_t angles,qboolean capsule){
 Check(model>=64&&model<72&&!capsule&&mask==currentCase->mask,"Entity model/mask");++entityCalls;digest.Word(3);digest.Word(model-64);
 tracedEntities.push_back(model-64);std::memcpy(lastTraceOrigin.data(),origin,12);std::memcpy(lastTraceAngles.data(),angles,12);
 digest.Vector(start);digest.Vector(end);digest.Vector(mins);digest.Vector(maxs);digest.Vector(origin);digest.Vector(angles);
 std::memcpy(out,&currentCase->traces[model-64],sizeof(*out));
}
int CM_PointContents(const vec3_t point,int model){Check(!model,"World point model");++worldPointCalls;digest.Word(4);digest.Vector(point);return 0x800;}
int CM_TransformedPointContents(const vec3_t point,int model,const vec3_t origin,const vec3_t angles){
 Check(model>=64&&model<72,"Entity point model");++pointCalls;digest.Word(5);digest.Word(model-64);
 std::memcpy(lastPointOrigin.data(),origin,12);std::memcpy(lastPointAngles.data(),angles,12);
 digest.Vector(point);digest.Vector(origin);digest.Vector(angles);return 1<<(model-64);
}
'''

OWNED = r'''
static std::array<sharedEntity_t,8> owned;
int SV_NumForGentity(sharedEntity_t *e){for(int i=0;i<8;++i)if(e==&owned[i])return i;throw Drop{};}
sharedEntity_t *SV_GentityNum(int i){if(i<0||i>=8)throw Drop{};return &owned[i];}
qboolean SV_ReadGameEntity(const void *e,sharedEntity_t *out){
 for(const auto &candidate:owned)if(e==&candidate&&out){*out=candidate;return qtrue;}return qfalse;
}
qboolean SV_ReadGameEntityByNum(int i,sharedEntity_t *out){return i>=0&&i<8?SV_ReadGameEntity(&owned[i],out):qfalse;}
'''

CHECKS = r'''
static tc::Trace Sweep(const tc::Case &c){
 trace_t trace{};SV_Trace(&trace,c.start.data(),c.nullMins?nullptr:c.mins.data(),c.nullMaxs?nullptr:c.maxs.data(),c.end.data(),c.pass,c.mask,qfalse);
 tc::Trace result;std::memcpy(&result,&trace,sizeof(result));return result;
}
int main(){
#ifdef FNTECH3_IHV_NATIVE_DATA
 if(sizeof(void*)!=4)return 77;
 constexpr std::size_t stride=556;
 std::array<std::uint32_t,(8*stride+32)/4> arena{};
 auto *base=reinterpret_cast<unsigned char*>(arena.data())+16;
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=base;binding.entitySpanBytes=8*stride;binding.entityCount=8;binding.entityStride=stride;binding.clientCount=1;
 binding.validateClientRecord=[](void*,const void*,std::int32_t,std::size_t){return false;};
 Check(serverGameData.BindNative(COM_GAME_QUAKE3_IHV,binding)==fntech3::server::GameDataBindError::None,"Native binding");
#endif
 profile.id=COM_GAME_QUAKE3_IHV;
 std::array<tc::Entity,8> records;
 const auto cases=tc::Make();
 const auto setup=[&](const tc::Case &c){
  currentCase=&c;tc::Initialize(c,records);tracedEntities.clear();
#ifdef FNTECH3_IHV_NATIVE_DATA
  std::memset(arena.data(),0xa5,sizeof(arena));
  for(unsigned i=0;i<8;++i){
   if(c.owner[i]>=0)records[i].owner=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base+c.owner[i]*stride));
   std::memcpy(base+i*stride,&records[i],sizeof(records[i]));
  }
#else
  owned={};
  for(unsigned i=0;i<8;++i){
   auto &e=owned[i];const auto &r=records[i];fntech3_q3_ihv_entityState_t state;
   std::memcpy(&state,r.state,sizeof(state));fntech3::q3ihv::state::ToInternal(state,e.s);
   e.r.svFlags=r.svFlags;e.r.bmodel=r.bmodel?qtrue:qfalse;e.r.contents=r.contents;
   e.r.ownerNum=c.owner[i]>=0?c.owner[i]:profile.id==COM_GAME_QUAKE3_IHV?-1:ENTITYNUM_NONE;
   std::memcpy(e.r.mins,r.mins,12);std::memcpy(e.r.maxs,r.maxs,12);
   std::memcpy(e.r.currentOrigin,r.currentOrigin,12);std::memcpy(e.r.currentAngles,r.currentAngles,12);
  }
#endif
 };
 std::size_t index=0;
 try{
  for(;index<cases.size();++index){
   const auto &c=cases[index];setup(c);
#ifdef FNTECH3_IHV_NATIVE_DATA
   const auto before=arena;
#else
   const auto before=owned;
#endif
   digest.Word(static_cast<std::uint32_t>(index));digest.Add(Sweep(c));
#ifdef FNTECH3_IHV_NATIVE_DATA
   Check(before==arena,"Trace changed native records, private tails or guards");
#else
   Check(!std::memcmp(&before,&owned,sizeof(owned)),"Trace changed shared records");
#endif
  }
  const auto traceDigest=digest.value;digest={};
  for(index=4096;index<cases.size();++index){
   const auto &c=cases[index];setup(c);
#ifdef FNTECH3_IHV_NATIVE_DATA
   const auto before=arena;
#else
   const auto before=owned;
#endif
   digest.Word(static_cast<std::uint32_t>(index));digest.Word(SV_PointContents(c.start.data(),c.pass));
#ifdef FNTECH3_IHV_NATIVE_DATA
   Check(before==arena,"Point query changed native records, private tails or guards");
#else
   Check(!std::memcmp(&before,&owned,sizeof(owned)),"Point query changed shared records");
#endif
  }
  std::printf("IHV traces=%zu points=%zu trace=%016llx point=%016llx\n",cases.size(),cases.size()-4096,static_cast<unsigned long long>(traceDigest),static_cast<unsigned long long>(digest.value));
  Check(traceDigest==0x5ad43e76c7086a58ull,"Original trace digest");Check(digest.value==0x640534dde0569e69ull,"Original point digest");
  Check(worldCalls==4737&&contactCalls==4350&&entityCalls==4231&&pointCalls==2605,"Original query counts");

  // Invalid input must fail before a world query or output publication.
  tc::Case c;c.world=tc::Sample(12,1);c.traces[1]=tc::Sample(8,2);c.count=1;
  const std::uint32_t nan=0x7fc00001u,infinity=0x7f800000u;
  for(int fault=0;fault<7;++fault){
   auto bad=c;if(fault==0)bad.mins[0]=bad.maxs[0]+1;
   if(fault==1)std::memcpy(&bad.start[0],&nan,4);
   if(fault==2)std::memcpy(&bad.maxs[2],&infinity,4);
   if(fault==3){bad.start[0]=bad.end[0]=bad.maxs[0]=(std::numeric_limits<float>::max)();}
   setup(bad);trace_t result;std::memset(&result,0xa5,sizeof(result));const auto before=result;
   const int calls=worldCalls;bool failed=false;
   try{SV_Trace(fault==6?nullptr:&result,fault==4?nullptr:bad.start.data(),bad.mins.data(),bad.maxs.data(),fault==5?nullptr:bad.end.data(),0,bad.mask,qfalse);}catch(Drop&){failed=true;}
   Check(failed&&worldCalls==calls&&!std::memcmp(&before,&result,sizeof(result)),"Invalid sweep changed output or queried world");
  }
  setup(c);const int pointBefore=worldPointCalls;
  for(int fault=0;fault<2;++fault){auto bad=c.start;std::memcpy(&bad[1],&nan,4);bool failed=false;
   try{SV_PointContents(fault?nullptr:bad.data(),0);}catch(Drop&){failed=true;}Check(failed,"Invalid point accepted");}
  Check(worldPointCalls==pointBefore,"Invalid point queried world");

#ifdef FNTECH3_IHV_NATIVE_DATA
  for(int fault=0;fault<4;++fault){
   setup(c);auto *entity=reinterpret_cast<tc::Entity*>(base+stride);
   if(fault==0)entity->owner=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base+1));
   if(fault==1)std::memcpy(entity->state+88,&nan,4);
   if(fault==2)entity->mins[0]=entity->maxs[0]+1;
   const auto before=arena;trace_t result;std::memset(&result,0xa5,sizeof(result));const auto previous=result;
   bool failed=false;try{SV_Trace(&result,c.start.data(),c.mins.data(),c.maxs.data(),c.end.data(),fault==3?8:0,c.mask,qfalse);}catch(Drop&){failed=true;}
   Check(failed&&arena==before&&!std::memcmp(&previous,&result,sizeof(result)),"Invalid native geometry/owner/pass changed output");
  }
  // Start/end may live inside the output record: only publish after reading.
  setup(c);const auto expected=Sweep(c);trace_t alias{};std::memcpy(alias.endpos,c.start.data(),12);
  SV_Trace(&alias,alias.endpos,c.mins.data(),c.maxs.data(),c.end.data(),c.pass,c.mask,qfalse);
  Check(!std::memcmp(&alias,&expected,sizeof(alias)),"Aliased start/output");
  serverGameData.Invalidate();bool failed=false;try{Sweep(c);}catch(Drop&){failed=true;}Check(failed,"Invalidated binding accepted");
#else
  // Later profiles retain their existing world sentinel, sticky startsolid,
  // sibling filtering and current-transform policies in the same server code.
  for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
   profile.id=id;c=tc::Case{};c.pass=ENTITYNUM_NONE;c.count=0;c.world=tc::Sample(12,1);setup(c);
   Check(Sweep(c).entityNum==ENTITYNUM_NONE,"Later no-hit sentinel");
   c.world=tc::Sample(8,1);setup(c);Check(Sweep(c).entityNum==ENTITYNUM_WORLD,"Later world sentinel");
   c.count=1;c.world=tc::Sample(14,1);c.traces[1]=tc::Sample(8,2);setup(c);
   auto result=Sweep(c);Check(result.entityNum==1&&result.startsolid&&result.fraction==.5f,"Later sticky startsolid");
   c.world=tc::Sample(4,1);c.traces[1]=tc::Sample(10,2);setup(c);result=Sweep(c);
   Check(result.entityNum==ENTITYNUM_WORLD&&result.startsolid&&result.fraction==.25f&&result.endpos[0]==c.world.endpos[0],"Later startsolid merge");
   c.world=tc::Sample(12,1);c.pass=1;c.count=2;c.order={2,3,1,4,5,6,7,0};c.owner[1]=3;c.owner[2]=3;
   c.traces[2]=tc::Sample(4,2);c.traces[3]=tc::Sample(8,3);setup(c);result=Sweep(c);
   Check(result.entityNum==3&&tracedEntities==std::vector<int>{3},"Later owner/sibling filtering");
   c=tc::Case{};c.pass=ENTITYNUM_NONE;c.count=1;c.brushBits=2;c.world=tc::Sample(12,1);c.traces[1]=tc::Sample(8,2);setup(c);Sweep(c);
   Check(!std::memcmp(lastTraceOrigin.data(),owned[1].r.currentOrigin,12)&&!std::memcmp(lastTraceAngles.data(),owned[1].r.currentAngles,12),"Later trace transform");
   SV_PointContents(c.start.data(),c.pass);
   Check(!std::memcmp(lastPointOrigin.data(),owned[1].r.currentOrigin,12)&&!std::memcmp(lastPointAngles.data(),owned[1].r.currentAngles,12),"Later point transform");
   c.brushBits=0;setup(c);Sweep(c);SV_PointContents(c.start.data(),c.pass);
   Check(lastTraceAngles==std::array<float,3>{}&&lastPointAngles==std::array<float,3>{},"Later box rotation");
   if(id==COM_GAME_WOLFET){c.pass=-2;setup(c);const int calls=contactCalls;result=Sweep(c);Check(result.entityNum==ENTITYNUM_NONE&&contactCalls==calls,"ET world-only pass");}
  }
#endif
 }catch(const Drop&){std::fprintf(stderr,"Unexpected engine rejection at case %zu\n",index);return 1;}
 catch(const std::exception &e){std::fprintf(stderr,"%s at case %zu\n",e.what(),index);return 1;}
 std::puts("IHV trace/point decisions, bounds and profile isolation passed");return 0;
}
'''


def build_source(native: bool) -> str:
    world = (ROOT/'code/server/sv_world.cpp').read_text(encoding='utf-8')
    source = PRELUDE
    if native:
        game = (ROOT/'code/server/sv_game.cpp').read_text(encoding='utf-8')
        source += '\nstatic fntech3::server::ServerGameDataView serverGameData;\n'
        source += game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
        for signature in ['int\tSV_NumForGentity(', 'sharedEntity_t *SV_GentityNum(',
                          'qboolean SV_ReadGameEntity(', 'qboolean SV_ReadGameEntityByNum(']:
            source += '\n'+function(game, signature)
    else:
        source += OWNED
    start = world.index('struct moveclip_t {')
    source += '\n'+world[start:world.index('\n};', start)+3]
    for signature in ['static void SV_ClipMoveToEntities(', 'void SV_Trace(', 'int SV_PointContents(']:
        source += '\n'+function(world, signature)
    return source+CHECKS


class IhvTraceTests(unittest.TestCase):
    def run_fixture(self, native: bool):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        source = build_source(native)
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        scratch = ROOT/'.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ihv-trace-', dir=scratch) as folder:
            folder = Path(folder)
            cpp = folder/'probe.cpp'
            exe = folder/'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            command = [compiler, *target, '-std=c++17', '-O2', '-ffast-math', '-I', str(ROOT), str(cpp)]
            if native:
                command += ['-DFNTECH3_IHV_NATIVE_DATA', str(ROOT/'code/server/server_game_data.cpp'),
                            '-x', 'c++', str(ROOT/'code/compat/game_abi_adapter.c')]
            result = subprocess.run(command+['-o', str(exe)], cwd=ROOT,
                                    capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            if result.returncode == 77:
                self.skipTest('Native IHV module records require a 32-bit host')
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)

    def test_native_trace_and_point_contracts(self):
        self.run_fixture(True)

    def test_shared_trace_and_later_profiles(self):
        self.run_fixture(False)


if __name__ == '__main__':
    unittest.main()
