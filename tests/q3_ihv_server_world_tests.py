"""Exercise the actual server link path with synthetic engine-owned records.

Native module storage and BSP queries are separate gates; these stubs do not
establish either one. Geometry vectors are pinned to original IHV observations.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import PRELUDE, function

ROOT = Path(__file__).resolve().parents[1]

BRIDGE = r'''
static std::array<sharedEntity_t,2> entities;
qboolean SV_ReadGameEntity(const void *entity,sharedEntity_t *out) {
 for(const auto &candidate:entities) if(entity==&candidate && out){*out=candidate;return qtrue;}
 return qfalse;
}
qboolean SV_WriteGameEntity(void *entity,const sharedEntity_t *in) {
 for(auto &candidate:entities) if(entity==&candidate && in){candidate=*in;return qtrue;}
 return qfalse;
}
qboolean SV_ReadGameEntityByNum(int index,sharedEntity_t *out) {
 return index>=0 && index<2 ? SV_ReadGameEntity(&entities[index],out) : qfalse;
}
svEntity_t *SV_SvEntityForGentity(sharedEntity_t *entity) {
 for(size_t i=0;i<entities.size();++i) if(entity==&entities[i]) return &sv.svEntities[i];
 throw Drop{};
}
'''

CHECKS = r'''
#include "tests/q3_ihv_collision_cases.hpp"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
int main() {
 profile.id=COM_GAME_QUAKE3_IHV; leaves=0;
 ihv_collision_cases::Digest digest;
 const auto cases=ihv_collision_cases::Make();
 for(size_t i=0;i<cases.size();++i) {
  const auto &c=cases[i]; SV_ClearWorld(); entities={}; auto &entity=entities[1];
  entity.s.number=1; entity.r.bmodel=c.brush?qtrue:qfalse;
  std::memcpy(&entity.r.contents,&c.contents,4);
  std::memcpy(entity.r.mins,c.mins.data(),12);std::memcpy(entity.r.maxs,c.maxs.data(),12);
  const float unused[3]={1234,5678,-4321};
  std::memcpy(entity.s.origin,unused,12);std::memcpy(entity.s.angles,unused,12);
  std::memcpy(entity.r.currentOrigin,unused,12);std::memcpy(entity.r.currentAngles,unused,12);
  entity.r.svFlags=c.current?SVF_USE_CURRENT_ORIGIN:0;
  std::memcpy(c.current?entity.r.currentOrigin:entity.s.origin,c.origin.data(),12);
  std::memcpy(c.current?entity.r.currentAngles:entity.s.angles,c.angles.data(),12);
  SV_LinkEntity(&entity);
  fntech3::q3ihv::LinkBounds result;
  CHECK(entity.s.solid>=0 && entity.s.solid<=65535);
  result.solid=static_cast<std::uint16_t>(entity.s.solid);
  std::memcpy(result.mins,entity.r.absmin,12);std::memcpy(result.maxs,entity.r.absmax,12);
  digest.Word(static_cast<std::uint32_t>(i));digest.Add(result);
  CHECK(!entity.r.linked && !sv.svEntities[1].worldSector);
 }
 CHECK(digest.value==0x7c40185466a69dbdull);
 // No later game inherits IHV packing or the state-origin fallback.
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}) {
  profile.id=id; SV_ClearWorld(); entities={};auto &e=entities[1];e.s.number=1;
  e.r.mins[0]=e.r.mins[1]=-16;e.r.mins[2]=-24;e.r.maxs[0]=e.r.maxs[1]=16;e.r.maxs[2]=32;
  e.s.origin[0]=-999;e.r.currentOrigin[0]=100;e.r.contents=CONTENTS_SOLID;
  SV_LinkEntity(&e); CHECK(e.s.solid==0x401810 && e.r.absmin[0]==83 && e.r.absmax[0]==117);
  e.r.bmodel=qtrue;SV_LinkEntity(&e);CHECK(e.s.solid==SOLID_BMODEL);
 }
 profile.id=COM_GAME_QUAKE3_IHV; SV_ClearWorld(); entities={};auto &e=entities[1];e.s.number=1;
 e.r.mins[0]=e.r.mins[1]=e.r.mins[2]=-16;e.r.maxs[0]=e.r.maxs[1]=e.r.maxs[2]=16;e.r.contents=1;
 const std::uint32_t nan=0x7fc00001u;std::memcpy(e.r.currentOrigin,&nan,4);
 // IHV's unused transform must not affect the link.
 leaves=2;SV_LinkEntity(&e);CHECK(e.r.linked && sv.svEntities[1].worldSector);
 e.r.svFlags=SVF_USE_CURRENT_ORIGIN;const auto before=e;auto *oldSector=sv.svEntities[1].worldSector;
 bool rejected=false;try{SV_LinkEntity(&e);}catch(Drop&){rejected=true;}
 CHECK(rejected && !std::memcmp(&before,&e,sizeof(e)) && sv.svEntities[1].worldSector==oldSector);
 // Overflow also rejects before breaking the previous world membership.
 e.r.currentOrigin[0]=e.r.maxs[0]=(std::numeric_limits<float>::max)();
 rejected=false;try{SV_LinkEntity(&e);}catch(Drop&){rejected=true;}
 CHECK(rejected && e.r.linked && sv.svEntities[1].worldSector==oldSector);
 // IHV keeps Quake II's cluster record: deduplicated numbers, or the first
 // straddled node once the box reaches 128 leafs or more than 16 clusters.
 profile.id=COM_GAME_QUAKE3_IHV; SV_ClearWorld(); entities={};auto &c=entities[1];c.s.number=1;
 c.r.mins[0]=c.r.mins[1]=c.r.mins[2]=-16;c.r.maxs[0]=c.r.maxs[1]=c.r.maxs[2]=16;c.r.contents=1;
 leaves=10;clusterModulus=3;SV_LinkEntity(&c);
 CHECK(sv.svEntities[1].numClusters==3);
 for(int i=0;i<3;++i)CHECK(sv.svEntities[1].clusternums[i]==i);
 clusterModulus=0;leaves=20;SV_LinkEntity(&c);
 CHECK(sv.svEntities[1].numClusters==-1 && sv.svEntities[1].lastCluster==topNode);
 leaves=128;SV_LinkEntity(&c);
 CHECK(sv.svEntities[1].numClusters==-1 && sv.svEntities[1].lastCluster==topNode);
 leaves=16;SV_LinkEntity(&c);
 CHECK(sv.svEntities[1].numClusters==16 && sv.svEntities[1].clusternums[15]==15);
 // The other profiles keep Quake III's list and its overflow cluster.
 profile.id=COM_GAME_QUAKE3;leaves=128;SV_LinkEntity(&c);
 CHECK(sv.svEntities[1].numClusters==16 && sv.svEntities[1].lastCluster==31);
 profile.id=COM_GAME_QUAKE3_IHV;leaves=2;
 std::puts("IHV server link vectors, cluster records, profile isolation and failure preservation passed");
}
'''


class IhvServerWorldTests(unittest.TestCase):
    def test_native_record_link_geometry(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++17 compiler unavailable')
        game = (ROOT/'code/server/sv_game.cpp').read_text()
        world = (ROOT/'code/server/sv_world.cpp').read_text()
        source = PRELUDE+game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
        for signature in ['int\tSV_NumForGentity(', 'sharedEntity_t *SV_GentityNum(',
                          'qboolean SV_ReadGameEntity(', 'qboolean SV_WriteGameEntity(',
                          'qboolean SV_ReadGameEntityByNum(', 'svEntity_t\t*SV_SvEntityForGentity(']:
            source += '\n'+function(game,signature)
        source += world[world.index('struct worldSector_s {'):world.index('\nstruct moveclip_t {')]
        source += r'''
#include "tests/q3_ihv_collision_cases.hpp"
#include <cstdio>
int main(){
 if(sizeof(void*)!=4)return 77;
 profile.id=COM_GAME_QUAKE3_IHV;leaves=0;
 constexpr int stride=556;
 std::vector<std::uint32_t> storage((2*stride+32)/4,0xa5a5a5a5u);
 auto *base=reinterpret_cast<unsigned char*>(storage.data())+16;
 auto *native=reinterpret_cast<fntech3_q3_ihv_entity_t*>(base+stride);
 auto *opaque=static_cast<sharedEntity_t*>(static_cast<void*>(native));
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=base;binding.entitySpanBytes=2*stride;binding.entityCount=2;
 binding.entityStride=stride;binding.clientCount=1;
 binding.validateClientRecord=[](void*,const void*,std::int32_t,std::size_t){return false;};
 assert(serverGameData.BindNative(COM_GAME_QUAKE3_IHV,binding)==fntech3::server::GameDataBindError::None);
 sv.gentities=base;sv.gentitySize=stride;sv.num_entities=2;
 const auto cases=ihv_collision_cases::Make();ihv_collision_cases::Digest digest;
 for(std::size_t index=0;index<cases.size();++index){
  SV_ClearWorld();const auto &c=cases[index];*native={};
  std::memset(native->state,0x7d,sizeof(native->state));native->state[0]=1;native->state[1]=0;
  native->inuse=1;native->bmodel=c.brush;std::memcpy(&native->contents,&c.contents,4);
  std::memcpy(native->mins,c.mins.data(),12);std::memcpy(native->maxs,c.maxs.data(),12);
  const float unused[3]={1234,5678,-4321};
  std::memcpy(native->state+88,unused,12);std::memcpy(native->state+112,unused,12);
  std::memcpy(native->currentOrigin,unused,12);std::memcpy(native->currentAngles,unused,12);
  native->svFlags=c.current?0x80:0;
  std::memcpy(c.current?native->currentOrigin:static_cast<void*>(native->state+88),c.origin.data(),12);
  std::memcpy(c.current?native->currentAngles:static_cast<void*>(native->state+112),c.angles.data(),12);
  auto expected=storage;fntech3::q3ihv::LinkBounds bounds;
  assert(ihv_collision_cases::Calculate(c,bounds));
  auto *expectedNative=reinterpret_cast<fntech3_q3_ihv_entity_t*>(reinterpret_cast<unsigned char*>(expected.data())+16+stride);
  std::memcpy(expectedNative->state+154,&bounds.solid,2);
  std::memcpy(expectedNative->absmin,bounds.mins,12);std::memcpy(expectedNative->absmax,bounds.maxs,12);
  SV_LinkEntity(opaque);assert(storage==expected); // Includes padding, private tail, world and guards.
  fntech3::q3ihv::LinkBounds actual;
  std::memcpy(&actual.solid,native->state+154,2);std::memcpy(actual.mins,native->absmin,12);std::memcpy(actual.maxs,native->absmax,12);
  digest.Word(static_cast<std::uint32_t>(index));digest.Add(actual);
 }
 assert(digest.value==0x7c40185466a69dbdull);
 SV_ClearWorld();*native={};native->state[0]=1;native->inuse=1;native->contents=1;
 for(int i=0;i<3;++i){native->mins[i]=-16;native->maxs[i]=16;}
 leaves=2;SV_LinkEntity(opaque);assert(native->linked&&sv.svEntities[1].worldSector);
 auto *sector=sv.svEntities[1].worldSector;
 native->owner=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base+1));auto before=storage;
 bool failed=false;try{SV_LinkEntity(opaque);}catch(Drop&){failed=true;}
 assert(failed&&storage==before&&sv.svEntities[1].worldSector==sector);
 native->owner=0;SV_UnlinkEntity(opaque);assert(!native->linked&&!sv.svEntities[1].worldSector);
 serverGameData.Invalidate();std::puts("IHV original link vectors pass through the real native record adapter without private-byte corruption");
}
'''
        (ROOT/'.tmp').mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ihv-native-world-',dir=ROOT/'.tmp') as folder:
            folder=Path(folder);cpp=folder/'probe.cpp';exe=folder/'probe.exe';cpp.write_text(source,encoding='utf-8')
            target=compiler_target_args(compiler)
            if os.name=='nt' and not target: target=['-DFNTECH3_Q3_ONLY']
            result=subprocess.run([compiler,*target,'-std=c++17','-O2','-ffast-math','-I',str(ROOT),str(cpp),
                                   str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',
                                   str(ROOT/'code/compat/game_abi_adapter.c'),'-o',str(exe)],
                                  cwd=ROOT,capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=20)
            if result.returncode==77:self.skipTest('Native IHV module records require a 32-bit host')
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_link_geometry_and_profile_isolation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++17 compiler unavailable')
        world = (ROOT/'code/server/sv_world.cpp').read_text()
        source = PRELUDE+BRIDGE+world[world.index('struct worldSector_s {'):world.index('\nstruct moveclip_t {')]+CHECKS
        scratch = ROOT/'.tmp'; scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ihv-world-', dir=scratch) as folder:
            folder = Path(folder); cpp = folder/'probe.cpp'; exe = folder/'probe.exe'; cpp.write_text(source)
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target: target = ['-DFNTECH3_Q3_ONLY']
            result = subprocess.run([compiler,*target,'-std=c++17','-O2','-ffast-math','-I',str(ROOT),str(cpp),'-o',str(exe)],
                                    cwd=ROOT,capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result = subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=20)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__ == '__main__': unittest.main()
