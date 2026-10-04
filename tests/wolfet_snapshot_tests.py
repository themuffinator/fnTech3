"""Exercise ET snapshot capacity, callback filtering and visibility proxies."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
HOST = r'''
#include "code/server/server.h"
#include "code/server/server_world_bounds.hpp"
#include <array>
#include <cassert>
#include <iterator>
#include <vector>
static comGameId_t profile;
int Com_GameIsWolfET(){return profile==COM_GAME_WOLFET;}
// Elite Force SP's camera scenes are not driven here.
int Com_GameIsSTVEFSP(){return profile==COM_GAME_STVEF_SP;}
int Com_GameIsJKAMP(){return profile==COM_GAME_JKA_MP;}
qboolean SV_JKASnapshotBroadcast(int,int,qboolean*){assert(!"JKA uses its own snapshot fixture");return qfalse;}
qboolean SV_ReadJKAVehicleState(int,playerState_t*){assert(!"JKA uses its own vehicle fixture");return qfalse;}
qboolean SV_STVEFSPCameraOrigin(vec3_t){return qfalse;}
server_t sv{};
serverStatic_t svs{};
struct Drop {};
void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}
static int calls;
static bool allow=true;
bool SV_WolfETSnapshotCallback(int,int){++calls;return allow;}
static snapshotFrame_t storage{};
static std::array<sharedEntity_t,MAX_GENTITIES> entities{};
qboolean SV_ReadGameEntityByNum(int n,sharedEntity_t *out){
 if(n<0||n>=sv.num_entities)return qfalse;*out=entities[n];return qtrue;
}
qboolean SV_WriteGameEntityByNum(int n,const sharedEntity_t *value){
 if(n<0||n>=sv.num_entities)return qfalse;entities[n]=*value;return qtrue;
}
void QDECL Com_DPrintf(const char*,...){}
static bool SV_GameForcesSnapshotEntity(int,int){return false;}
static bool areasConnected=true,missingPvs=false;
static std::vector<std::array<float,3>> viewpoints;
static byte visibility[2]={1,2};
static bool SV_AreasVisibleForSnapshot(int,const svEntity_t*){return areasConnected;}
int CM_PointLeafnum(const float *point){
 assert(viewpoints.size()<100);viewpoints.push_back({point[0],point[1],point[2]});return point[0]>=100?1:0;
}
int CM_LeafArea(int leaf){return leaf;}
int CM_LeafCluster(int leaf){return leaf;}
int CM_NumClusters(){return 8;}
int CM_WriteAreaBits(byte *out,int area){out[0]|=byte(1<<area);return 1;}
byte *CM_ClusterPVS(int cluster){return missingPvs?nullptr:visibility+(cluster==1?1:0);}
using SnapshotEntityIndex = int;
static void setup(comGameId_t id,int count){
 profile=id;std::memset(&sv,0,sizeof(sv));std::memset(&svs,0,sizeof(svs));
 std::memset(&storage,0,sizeof(storage));std::memset(entities.data(),0,sizeof(entities));calls=0;allow=true;
 areasConnected=true;missingPvs=false;viewpoints.clear();visibility[0]=1;visibility[1]=2;
 sv.state=SS_GAME;sv.snapshotCounter=19;sv.num_entities=count;
 svs.currFrame=&storage;storage.count=count;
 for(int i=0;i<count;++i){entities[i].s.number=i;entities[i].s.otherEntityNum=ENTITYNUM_NONE;
  entities[i].r.linked=qtrue;storage.ents[i]=&entities[i].s;
  sv.svEntities[i].numClusters=1;sv.svEntities[i].clusternums[0]=1;}
}
'''
CHECKS = r'''
int main(){
 static_assert(MAX_SERVER_SNAPSHOT_ENTITIES >= MAX_SNAPSHOT_ENTITIES);
 static_assert(MAX_SERVER_SNAPSHOT_ENTITIES == MAX_GENTITIES);
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  setup(id,MAX_GENTITIES);
  struct { SnapshotEntityNumbers value; unsigned sentinel=0x5a5aa5a5; } guarded;
  const int expected=id==COM_GAME_WOLFET?MAX_GENTITIES:256;
  svEntity_t entity{};
  for(int index=0;index<MAX_GENTITIES+32;++index){
   entity.snapshotCounter=0;
   guarded.value.Add(entity,index);
   assert(guarded.value.count==(index<expected?index+1:expected));
   assert(entity.snapshotCounter==19);
  }
  assert(guarded.sentinel==0x5a5aa5a5);
  clientSnapshot_t frame{};
  static std::array<entityState_t,MAX_GENTITIES> states{};
  assert(guarded.value.count<=std::size(frame.ents));
  for(int index=0;index<guarded.value.count;++index){
   assert(guarded.value.indices[index]==index);
   frame.ents[index]=&states[guarded.value.indices[index]];
  }
  assert(frame.ents[expected-1]==&states[expected-1]);
  assert(calls==(id==COM_GAME_WOLFET?MAX_GENTITIES:0));
 }
 setup(COM_GAME_WOLFET,2);
 for(int invalid:{-1,2,INT_MAX}){
  SnapshotEntityNumbers out;bool dropped=false;++sv.snapshotCounter;
  try{out.Add(sv.svEntities[0],invalid);}catch(Drop&){dropped=true;}
  assert(dropped&&out.count==0&&calls==0);
 }
 for(bool missingFrame:{false,true}){
  SnapshotEntityNumbers out;bool dropped=false;++sv.snapshotCounter;
  storage.ents[0]=nullptr;if(missingFrame)svs.currFrame=nullptr;
  try{out.Add(sv.svEntities[0],0);}catch(Drop&){dropped=true;}
  assert(dropped&&out.count==0&&calls==0);
 }
}
'''

VISIBILITY = r'''
static std::vector<int> visible(int client=0){
 SnapshotEntityNumbers out;out.clientNum=client;clientSnapshot_t frame{};frame.ps.clientNum=client;
 sv.svEntities[client].snapshotCounter=sv.snapshotCounter;
 vec3_t origin{};SV_AddEntitiesVisibleFromPoint(origin,&frame,out,false);
 return {out.indices.begin(),out.indices.begin()+out.count};
}
static void proxy(int number,int flags,int target){
 entities[number].r.svFlags=flags;entities[number].s.otherEntityNum=target;
 sv.svEntities[number].clusternums[0]=0;
}
int main(){
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  setup(id,8);proxy(2,SVF_RTCW_VISDUMMY,3);
  assert(visible()==std::vector<int>{id==COM_GAME_WOLFET?3:2});
  assert(calls==(id==COM_GAME_WOLFET?1:0));
 }
 setup(COM_GAME_WOLFET,8);proxy(2,SVF_RTCW_VISDUMMY,3);
 entities[3].r.svFlags=SVF_NOCLIENT|SVF_SINGLECLIENT;entities[3].r.singleClient=6;
 assert(visible()==std::vector<int>{3}); // Original single-proxy exception.
 setup(COM_GAME_WOLFET,8);proxy(2,SVF_RTCW_VISDUMMY_MULTIPLE,0);
 for(int i=3;i<7;++i)entities[i].s.otherEntityNum=2;
 entities[5].r.svFlags=SVF_NOCLIENT;entities[6].r.linked=qfalse;
 assert(visible()==(std::vector<int>{3,4}));
 setup(COM_GAME_WOLFET,8);proxy(2,SVF_RTCW_VISDUMMY,3);proxy(7,SVF_RTCW_VISDUMMY,3);
 allow=false;assert(visible().empty()&&calls==1);
 ++sv.snapshotCounter;allow=true;calls=0;assert(visible(1)==std::vector<int>{3});assert(calls==1);
 setup(COM_GAME_WOLFET,8);entities[3].r.svFlags=SVF_BROADCAST;allow=false;
 assert(visible().empty()&&calls==1); // Broadcasts still consult the callback.
 setup(COM_GAME_WOLFET,8);entities[3].r.svFlags=SVF_BROADCAST|SVF_NOCLIENT;
 assert(visible().empty()&&calls==0); // Common storage must not leak NOCLIENT.
 for(int target:{-1,8,INT_MAX}){
  setup(COM_GAME_WOLFET,8);proxy(2,SVF_RTCW_VISDUMMY,target);
  bool dropped=false;try{visible();}catch(Drop&){dropped=true;}assert(dropped);
 }
 setup(COM_GAME_WOLFET,8);proxy(2,SVF_RTCW_VISDUMMY,3);entities[3].r.linked=qfalse;
 assert(visible().empty()&&calls==0);
 // Build actual shared storage: only ET retains a hidden proxy target, and
 // ordinary NOCLIENT entities must still stay out of the client's snapshot.
 static std::array<entityState_t,MAX_GENTITIES> backing{};
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  setup(id,8);proxy(2,SVF_RTCW_VISDUMMY,3);
  entities[3].r.svFlags=SVF_NOCLIENT;entities[5].r.linked=qfalse;
  entities[6].r.svFlags=SVF_BROADCAST|SVF_NOCLIENT;
  svs.snapshotEntities=backing.data();svs.numSnapshotEntities=backing.size();
  svs.freeStorageEntities=backing.size();SV_BuildCommonSnapshot();++sv.snapshotCounter;
  assert(svs.currFrame->count==(id==COM_GAME_WOLFET?7:5));
  const auto indices=visible();assert(indices.size()==1);
  assert(svs.currFrame->ents[indices[0]]->number==(id==COM_GAME_WOLFET?3:2));
  assert(calls==(id==COM_GAME_WOLFET?1:0));
 }
}
'''


class WolfETSnapshotTests(unittest.TestCase):
    def compile_source(self,source_text):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'snapshot.cpp'
            executable = folder/('snapshot.exe' if os.name == 'nt' else 'snapshot')
            source.write_text(source_text,encoding='utf-8')
            target = compiler_target_args(compiler)
            if not target:
                target = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler,*target,'-std=c++17',
                '-I',str(ROOT),str(source),'-o',str(executable)],
                cwd=ROOT,capture_output=True,text=True,timeout=90)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run = subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=20)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def snapshot_source(self,visibility=False):
        snapshot = (ROOT/'code/server/sv_snapshot.cpp').read_text(encoding='utf-8')
        begin = snapshot.index('struct SnapshotEntityNumbers {')
        body = snapshot[begin:snapshot.index('\n};',begin)+3]
        if visibility:
            for signature in ('static bool SV_AddWolfETVisibilityTargets(',
                              'static bool SV_EntityPassesSnapshotClientFlags(',
                              'static void SV_AddEntitiesVisibleFromPoint(',
                              'static void SV_BuildCommonSnapshot('):
                begin = snapshot.index(signature)
                body += '\n'+snapshot[begin:snapshot.index('\n}',begin)+2]
        return HOST+body

    def test_et_window_and_existing_profile_caps(self):
        self.compile_source(self.snapshot_source()+CHECKS)

    def test_visibility_proxies_callbacks_and_other_profiles(self):
        self.compile_source(self.snapshot_source(True)+VISIBILITY)


if __name__ == '__main__':
    unittest.main()
