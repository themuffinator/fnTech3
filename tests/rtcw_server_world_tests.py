"""Execute server world queries through all five frozen game-data layouts."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/server/server.h"
#include "code/server/server_game_data.h"
#include "code/server/server_world_bounds.hpp"
#include "code/server/sv_collision_model.hpp"
#include "code/compat/game_abi_adapter.h"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/stvef_game_abi.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>
server_t sv{};
static comGameProfile_t profile{};
const comGameProfile_t *Com_GameProfile() { return &profile; }
int Com_GameIsRTCW() { return profile.id == COM_GAME_RTCW_SP || profile.id == COM_GAME_RTCW_MP; }
int Com_GameIsWolfET() { return profile.id == COM_GAME_WOLFET; }
int Com_GameIsSTVEFMP() { return profile.id == COM_GAME_STVEF_MP; }
static fntech3::server::ServerGameDataView serverGameData;
struct Drop {};
void QDECL Com_Error(errorParm_t, const char *, ...) { throw Drop{}; }
void QDECL Com_Printf(const char *, ...) {}
void QDECL Com_DPrintf(const char *, ...) {}
const vec3_t vec3_origin{};
float RadiusFromBounds(const vec3_t mins, const vec3_t maxs) {
 float sum=0;for(int i=0;i<3;++i){float m=std::max(std::abs(mins[i]),std::abs(maxs[i]));sum+=m*m;}return std::sqrt(sum);
}
static int leaves=2, boxCalls, portalCalls, contactCalls, lastCapsule;
static bool connected=true, noMask=false, touch=true;
static int destinationCluster=1;
static byte pvs[4]={2,0,0,0};
int CM_NumInlineModels(){return 4;}
int CM_NumClusters(){return 32;}
clipHandle_t CM_InlineModel(int model){if(model<0||model>=4)throw Drop{};return model;}
void CM_ModelBounds(clipHandle_t model,vec3_t mins,vec3_t maxs){
 for(int i=0;i<3;++i){mins[i]=model?-4.0f:-1024.0f;maxs[i]=model?4.0f:1024.0f;}
}
clipHandle_t CM_TempBoxModel(const vec3_t mins,const vec3_t maxs,int capsule){
 assert(SV_ValidWorldBounds(mins,maxs));lastCapsule=capsule;return 3;
}
int CM_BoxLeafnums(const vec3_t mins,const vec3_t maxs,int *out,int capacity,int *last){
 assert(SV_ValidWorldBounds(mins,maxs));++boxCalls;assert(leaves<=capacity);
 for(int i=0;i<leaves;++i)out[i]=i;*last=31;return leaves;
}
int CM_LeafCluster(int leaf){return leaf==100?destinationCluster:leaf;}
int CM_LeafArea(int leaf){return leaf==100?1:leaf%2;}
int CM_PointLeafnum(const vec3_t point){return point[0]>0?100:0;}
byte *CM_ClusterPVS(int){return noMask?nullptr:pvs;}
qboolean CM_AreasConnected(int a,int b){if(a<0||b<0||a>=32||b>=32)throw Drop{};return connected?qtrue:qfalse;}
void CM_AdjustAreaPortalState(int a,int b,qboolean open){assert(a==0&&b==1);connected=open!=0;++portalCalls;}
void CM_TransformedBoxTrace(trace_t *out,const vec3_t start,const vec3_t end,const vec3_t mins,
 const vec3_t maxs,clipHandle_t model,int contents,const vec3_t origin,const vec3_t angles,qboolean capsule){
 assert(start==vec3_origin&&end==vec3_origin&&model>=0&&contents==-1);
 assert(SV_ValidWorldBounds(mins,maxs)&&SV_FiniteWorldVector(origin)&&SV_FiniteWorldVector(angles));
 *out={};out->startsolid=touch?qtrue:qfalse;lastCapsule=capsule;++contactCalls;
}
'''


CHECKS = r'''
#include "code/server/server_rtcw_world.hpp"
using Call=fntech3::rtcw::module::Call;
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
template<class Entity, class Player> void run(comGameId_t id,bool qvm) {
 profile.id=id;sv={};leaves=2;connected=true;noMask=false;destinationCluster=1;
 constexpr size_t stride=sizeof(Entity)+48, base=128, count=4;
 std::vector<byte> memory(base+count*stride+128,0xa5),clients(sizeof(Player));
 for(size_t i=0;i<count;++i){Entity initial{};initial.s.number=(int)i;
  if constexpr (!std::is_same_v<Entity,fntech3_wolfet_sharedEntity_t>) initial.r.s.number=991;
  initial.r.svFlags=static_cast<int>(0x80000002u);
  initial.r.ownerNum=1023;
  initial.r.mins[0]=initial.r.mins[1]=-16;initial.r.mins[2]=-24;
  initial.r.maxs[0]=initial.r.maxs[1]=16;initial.r.maxs[2]=32;
  initial.r.contents=CONTENTS_SOLID;initial.r.currentOrigin[0]=(float)i*100;
  std::memcpy(memory.data()+base+i*stride,&initial,sizeof(initial));
 }
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=memory.data()+base;binding.entitySpanBytes=count*stride;
 binding.entityCount=count;binding.entityStride=stride;
 binding.clientBase=clients.data();binding.clientSpanBytes=clients.size();binding.clientCount=1;binding.clientStride=sizeof(Player);
 assert(serverGameData.BindNative(id,binding)==fntech3::server::GameDataBindError::None);
 sv.gentities=binding.entityBase;sv.gentitySize=stride;sv.num_entities=count;
 vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=(int)memory.size();
 if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 auto address=[&](size_t at)->intptr_t{return qvm?(intptr_t)at:reinterpret_cast<intptr_t>(memory.data()+at);};
 auto read=[&](int i){Entity e;std::memcpy(&e,memory.data()+base+i*stride,sizeof(e));return e;};
 auto write=[&](int i,const Entity&e){std::memcpy(memory.data()+base+i*stride,&e,sizeof(e));};
 auto pointer=[&](int i){return static_cast<sharedEntity_t*>(static_cast<void*>(memory.data()+base+i*stride));};
 auto invoke=[&](Call call,std::initializer_list<intptr_t> values){
  std::array<intptr_t,6> args{};std::copy(values.begin(),values.end(),args.begin()+1);intptr_t result=777;
  assert(SV_RTCWWorldCall(&vm,call,args.data(),result));return result;
 };
 auto link=[&](int i){if(Com_GameIsRTCW())invoke(Call::G_LINKENTITY,{address(base+i*stride)});else SV_LinkEntity(pointer(i));};
 auto unlink=[&](int i){if(Com_GameIsRTCW())invoke(Call::G_UNLINKENTITY,{address(base+i*stride)});else SV_UnlinkEntity(pointer(i));};
 SV_ClearWorld();assert(sv_numworldSectors==31);
 const auto original=memory;auto expected=read(0);
 link(0);expected.r.linked=1;expected.r.linkcount=1;expected.s.solid=(64<<16)|(24<<8)|16;
 for(int i=0;i<3;++i){expected.r.absmin[i]=expected.r.mins[i]-1;expected.r.absmax[i]=expected.r.maxs[i]+1;}
 assert(std::memcmp(&expected,memory.data()+base,sizeof(expected))==0);
 // Complete raw allocation comparison proves no second state/private tail,
 // neighboring record or guard corruption, for Q3, QL, SP and MP.
 auto fullExpected=original;std::memcpy(fullExpected.data()+base,&expected,sizeof(expected));assert(memory==fullExpected);
 assert(sv.svEntities[0].areanum==0&&sv.svEntities[0].areanum2==1&&sv.svEntities[0].numClusters==2);
 auto*sector=sv.svEntities[0].worldSector;assert(sector);
 link(0);assert(read(0).r.linkcount==2&&sv.svEntities[0].nextEntityInWorldSector!=&sv.svEntities[0]);
 link(1);link(2);link(3);vec3_t mins{-200,-200,-200},maxs{400,200,200};int found[6]={-7,-7,-7,-7,-7,-7};
 assert(SV_AreaEntities(mins,maxs,found+1,4)==4);std::sort(found+1,found+5);
 assert(found[0]==-7&&found[5]==-7);for(int i=0;i<4;++i)assert(found[i+1]==i);
 assert(SV_AreaEntities(mins,maxs,found,1)==1);
 unlink(2);unlink(2);assert(!read(2).r.linked&&!sv.svEntities[2].worldSector);
 assert(SV_AreaEntities(mins,maxs,found,4)==3);
 // Outside-world relink clears both sides without incrementing linkcount.
 leaves=0;int previous=read(1).r.linkcount;link(1);assert(!read(1).r.linked&&!sv.svEntities[1].worldSector&&read(1).r.linkcount==previous);
 leaves=128;link(1);assert(sv.svEntities[1].numClusters==16&&sv.svEntities[1].lastCluster==31);leaves=2;
 auto e=read(0);e.r.linkcount=INT_MAX;write(0,e);link(0);assert(read(0).r.linkcount==INT_MIN);
 // Huge finite bounds clamp before conversion. Invalid/overflowed bounds do
 // not disturb the old linkage or rewrite module memory.
 e=read(0);e.r.maxs[0]=1e30f;write(0,e);link(0);assert((read(0).s.solid&255)==255);
 e=read(0);e.r.maxs[0]=16;write(0,e);link(0);
 for(int failure=0;failure<4;++failure){auto bad=read(0);
  if(failure==0)bad.r.currentOrigin[0]=std::numeric_limits<float>::quiet_NaN();
  if(failure==1)bad.r.mins[0]=bad.r.maxs[0]+1;
  if(failure==2){bad.r.maxs[0]=std::numeric_limits<float>::max();bad.r.currentOrigin[0]=bad.r.maxs[0];}
  if(failure==3){bad.r.bmodel=1;bad.r.currentAngles[1]=1;bad.r.maxs[0]=1e30f;}
  auto good=read(0);write(0,bad);auto before=memory;auto *old=sv.svEntities[0].worldSector;int calls=boxCalls;
  rejected([&]{link(0);});assert(memory==before&&sv.svEntities[0].worldSector==old&&calls==boxCalls);write(0,good);
 }
 if(Com_GameIsWolfET()){
  auto brush=read(0);brush.r.bmodel=1;brush.r.currentOrigin[0]=100;
  brush.r.svFlags|=FNTECH3_WOLFET_SVF_IGNOREBMODELEXTENTS;write(0,brush);
  for(int cluster:{-1,0,7,31}){
   destinationCluster=cluster;link(0);
   assert(sv.svEntities[0].wolfetOriginCluster==cluster);
   assert(read(0).r.svFlags==brush.r.svFlags);
  }
  brush.r.bmodel=0;write(0,brush);link(0);assert(sv.svEntities[0].wolfetOriginCluster==-1);
  brush.r.bmodel=1;write(0,brush);link(0);assert(sv.svEntities[0].wolfetOriginCluster==31);
  SV_ClearWorld();assert(sv.svEntities[0].wolfetOriginCluster==-1);
 }
 if(Com_GameIsRTCW()){
  rejected([&]{invoke(Call::G_LINKENTITY,{address(base+1)});});
  rejected([&]{invoke(Call::G_UNLINKENTITY,{address(base+sizeof(Entity))});});
  rejected([&]{invoke(Call::G_LINKENTITY,{0});});
  if(qvm){rejected([&]{invoke(Call::G_LINKENTITY,{address(memory.size()+base)});});
   rejected([&]{invoke(Call::G_LINKENTITY,{-1});});}
  std::memcpy(memory.data()+8,mins,sizeof(mins));std::memcpy(memory.data()+24,maxs,sizeof(maxs));
  const int capacity=3;const size_t output=49;std::fill(memory.begin()+48,memory.begin()+64,0xa5);
  assert(invoke(Call::G_ENTITIES_IN_BOX,{address(8),address(24),address(output),capacity})==3);
  assert(memory[48]==0xa5&&memory[output+12]==0xa5);
  assert(invoke(Call::G_ENTITIES_IN_BOX,{address(8),address(24),0,0})==0);
  auto before=memory;
  for(int n:{-1,MAX_GENTITIES+1,INT_MAX})rejected([&]{invoke(Call::G_ENTITIES_IN_BOX,{address(8),address(24),address(output),n});});
  if(qvm)rejected([&]{invoke(Call::G_ENTITIES_IN_BOX,{address(8),address(24),address(memory.size()-2),1});});
  assert(memory==before);
  // Legal zero-radius and output/input aliases are copied before querying.
  assert(invoke(Call::G_ENTITIES_IN_BOX,{address(8),address(24),address(8),1})==1);
  std::memcpy(memory.data()+8,mins,sizeof(mins));
  for(Call c:{Call::G_ENTITY_CONTACT,Call::G_ENTITY_CONTACTCAPSULE}){
   touch=true;assert(invoke(c,{address(8),address(24),address(base)})==1);
   assert(lastCapsule==(c==Call::G_ENTITY_CONTACTCAPSULE));touch=false;assert(invoke(c,{address(8),address(24),address(base)})==0);
  }
  std::strcpy(reinterpret_cast<char*>(memory.data()+64),"*2");
  invoke(Call::G_SET_BRUSH_MODEL,{address(base),address(64)});
  e=read(0);assert(e.r.bmodel&&e.s.modelindex==2&&e.r.contents==-1&&e.s.solid==SOLID_BMODEL&&e.r.mins[0]==-4&&e.r.maxs[2]==4);
  e.r.currentAngles[1]=90;write(0,e);link(0);assert(read(0).r.absmax[0]>7.92f&&read(0).r.absmax[0]<7.93f);
  for(const char *name:{"*","*4","*-1","*2147483648","*1junk","bad"}){
   std::strcpy(reinterpret_cast<char*>(memory.data()+64),name);before=memory;
   rejected([&]{invoke(Call::G_SET_BRUSH_MODEL,{address(base),address(64)});});assert(memory==before);
  }
  invoke(Call::G_ADJUST_AREA_PORTAL_STATE,{address(base),0});assert(!connected);
  assert(invoke(Call::G_AREAS_CONNECTED,{0,1})==0);invoke(Call::G_ADJUST_AREA_PORTAL_STATE,{address(base),1});assert(connected);
  if(qvm)rejected([&]{SV_RTCWVector(&vm,address(memory.size()-2));});
  if(sizeof(intptr_t)>4)rejected([&]{SV_RTCWInteger(static_cast<intptr_t>(INT_MAX)+1);});
  serverGameData.Invalidate();rejected([&]{invoke(Call::G_UNLINKENTITY,{address(base)});});
 }else{
  intptr_t result=99;assert(!SV_RTCWWorldCall(&vm,Call::G_LINKENTITY,nullptr,result)&&result==99);
 }
 serverGameData.Invalidate();sv={};
}
int main(){
 run<fntech3_q3_sharedEntity_t,fntech3_q3_playerState_t>(COM_GAME_QUAKE3,false);
 run<fntech3_ql_sharedEntity_t,fntech3_ql_playerState_t>(COM_GAME_QUAKELIVE,false);
 run<fntech3_wolfet_sharedEntity_t,fntech3_wolfet_playerState_t>(COM_GAME_WOLFET,false);
 for(bool qvm:{false,true}){
  run<fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_sp_playerState_t>(COM_GAME_RTCW_SP,qvm);
  run<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP,qvm);
 }
 vec3_t from{-1,0,0},to{1,0,0};connected=true;noMask=false;pvs[0]=2;
 for(int cluster:{-1,0,1,7,8,31,32,INT_MAX}){
  destinationCluster=cluster;bool visible=cluster==1;
  assert(bool(SV_inPVS(from,to))==visible&&SV_inPVSIgnorePortals(from,to)==visible);
 }
 destinationCluster=1;connected=false;assert(!SV_inPVS(from,to)&&SV_inPVSIgnorePortals(from,to));
 noMask=true;destinationCluster=-1;connected=true;assert(SV_inPVS(from,to)&&SV_inPVSIgnorePortals(from,to));
}
'''


class RTCWServerWorldTests(unittest.TestCase):
    def test_world_membership_bounds_queries_and_private_entity_tails(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        game = (ROOT / 'code/server/sv_game.cpp').read_text()
        world = (ROOT / 'code/server/sv_world.cpp').read_text()
        source = PRELUDE + game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
        for signature in ['int\tSV_NumForGentity(', 'sharedEntity_t *SV_GentityNum(',
                          'qboolean SV_ReadGameEntity(', 'qboolean SV_WriteGameEntity(',
                          'qboolean SV_ReadGameEntityByNum(', 'svEntity_t\t*SV_SvEntityForGentity(']:
            source += '\n' + function(game, signature)
        source += '\n' + function(world, 'clipHandle_t SV_ClipHandleForEntity(')
        source += '\n' + world[world.index('struct worldSector_s {'):world.index('\nstruct moveclip_t {')]
        for signature in ['static void SV_SetBrushModel(', 'qboolean SV_inPVS(',
                          'static bool SV_inPVSIgnorePortals(', 'static void SV_AdjustAreaPortalState(',
                          'static bool SV_EntityContact(']:
            source += '\n' + function(game, signature)
        source += CHECKS
        # The tested adapter must run before any generic ordinal remapping.
        dispatch = function(game, 'static intptr_t SV_GameSystemCalls(')
        self.assertLess(dispatch.index('SV_RTCWWorldCall('), dispatch.index('switch ( call )'))
        with tempfile.TemporaryDirectory(prefix='rtcw-world-') as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            command = [compiler, *target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT),
                       str(cpp), str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
                       str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
