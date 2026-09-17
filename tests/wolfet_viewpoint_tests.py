"""Execute ET viewpoint/PVS contracts without running a game or taking input."""
from pathlib import Path
import unittest
import wolfet_snapshot_tests as snapshots

ROOT = Path(__file__).resolve().parents[1]

SUPPORT = r'''
#include <cmath>
#include <limits>
static playerState_t player{};
static client_t peers[2]{};
static int cameraReads;
static std::array<float,3> audioOrigin;
qboolean SV_ReadGameEntity(const void *pointer,sharedEntity_t *out){
 ++cameraReads;
 for(int i=0;i<sv.num_entities;++i)if(pointer==&entities[i]){*out=entities[i];return qtrue;}
 return qfalse;
}
qboolean SV_ReadGameClient(int client,playerState_t *out){assert(client>=0&&client<2);*out=player;return qtrue;}
static void SV_AddAudioReachableEntities(const vec3_t origin,int,clientSnapshot_t*,SnapshotEntityNumbers&){
 audioOrigin={origin[0],origin[1],origin[2]};
}
template<class F> void rejected(F f){bool dropped=false;try{f();}catch(Drop&){dropped=true;}assert(dropped);}
static bool close(float a,float b){return std::abs(a-b)<0.0001f;}
static std::vector<int> collect(int viewer=0){
 SnapshotEntityNumbers numbers;clientSnapshot_t frame{};frame.ps.clientNum=viewer;numbers.clientNum=viewer;
 sv.svEntities[viewer].snapshotCounter=sv.snapshotCounter;
 vec3_t origin{};SV_AddEntitiesVisibleFromPoint(origin,&frame,numbers,false);
 if(numbers.unordered)SV_SortEntityNumbers(numbers);
 return {numbers.indices.begin(),numbers.indices.begin()+numbers.count};
}
'''

CHECKS = r'''
int main(){
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  setup(id,2);entities[1].r.svFlags=SVF_WOLFET_ORIGIN_ONLY;
  sv.svEntities[1].clusternums[0]=0;sv.svEntities[1].wolfetOriginCluster=1;
  assert(collect()==(id==COM_GAME_WOLFET?std::vector<int>{}:std::vector<int>{1}));
  ++sv.snapshotCounter;sv.svEntities[1].wolfetOriginCluster=0;areasConnected=false;
  assert(collect()==(id==COM_GAME_WOLFET?std::vector<int>{1}:std::vector<int>{}));
 }
 for(int cluster:{-1,0,1,7,8,INT_MIN,INT_MAX}){
  setup(COM_GAME_WOLFET,2);entities[1].r.svFlags=SVF_WOLFET_ORIGIN_ONLY;
  sv.svEntities[1].wolfetOriginCluster=cluster;visibility[0]=0x81;
  const bool expected=cluster==0||cluster==7;
  assert(collect().size()==(expected?1:0)&&calls==(expected?1:0));
 }
 setup(COM_GAME_WOLFET,2);entities[1].r.svFlags=SVF_WOLFET_ORIGIN_ONLY;
 sv.svEntities[1].wolfetOriginCluster=0;missingPvs=true;assert(collect().empty()&&calls==0);
 missingPvs=false;allow=false;++sv.snapshotCounter;assert(collect().empty()&&calls==1);
 // Broadcast has precedence; NOCLIENT has precedence over broadcast.
 setup(COM_GAME_WOLFET,2);entities[1].r.svFlags=SVF_WOLFET_ORIGIN_ONLY|SVF_BROADCAST;
 sv.svEntities[1].wolfetOriginCluster=-1;assert(collect()==std::vector<int>{1});
 ++sv.snapshotCounter;entities[1].r.svFlags|=SVF_NOCLIENT;assert(collect().empty());

 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  setup(id,3);entities[0].r.svFlags=SVF_WOLFET_SELF_PORTAL;
  entities[0].s.origin2[0]=100;entities[0].r.s.origin2[0]=-100;
  sv.svEntities[2].clusternums[0]=0;
  assert(collect()==(id==COM_GAME_WOLFET?std::vector<int>{1,2}:std::vector<int>{2}));
  assert(viewpoints.size()==(id==COM_GAME_WOLFET?2:1));
 }
 // A followed player supplies the self portal; the connected player remains
 // the owner of the exclusive camera, matching their separate contracts.
 setup(COM_GAME_WOLFET,3);entities[1].r.svFlags=SVF_WOLFET_SELF_PORTAL;
 entities[1].s.origin2[0]=100;sv.svEntities[0].clusternums[0]=0;
 assert(collect(1)==(std::vector<int>{0,2}));
 // Cyclic ordinary portals and a self portal terminate and include each entity once.
 setup(COM_GAME_WOLFET,4);entities[0].r.svFlags=SVF_WOLFET_SELF_PORTAL;entities[0].s.origin2[0]=100;
 entities[1].r.svFlags=SVF_PORTAL;entities[1].s.origin2[0]=0;
 entities[2].r.svFlags=SVF_PORTAL;entities[2].s.origin2[0]=100;sv.svEntities[2].clusternums[0]=0;
 assert(collect()==(std::vector<int>{1,2,3}));assert(viewpoints.size()==4&&calls==3);

 setup(COM_GAME_WOLFET,3);player={};std::memset(peers,0,sizeof(peers));svs.clients=peers;
 player.clientNum=0;peers[0].state=CS_ACTIVE;peers[0].gentity=&entities[0];
 entities[0].r.svFlags=SVF_WOLFET_SELF_PORTAL_EXCLUSIVE;entities[0].s.origin2[0]=100;
 sv.svEntities[1].clusternums[0]=0;SV_BuildClientSnapshot(&peers[0]);
 const auto &frame=peers[0].frames[0];
 assert(frame.num_entities==1&&frame.ents[0]->number==2&&frame.ps.origin[0]==0);
 assert(frame.areabytes==1&&frame.areabits[0]==byte(~2));
 assert(audioOrigin[0]==100&&audioOrigin[1]==0&&audioOrigin[2]==0);

 setup(COM_GAME_WOLFET,3);player={};std::memset(peers,0,sizeof(peers));svs.clients=peers;
 peers[0].state=CS_ACTIVE;peers[0].gentity=&entities[0];
 entities[0].r.svFlags=SVF_WOLFET_SELF_PORTAL;entities[0].s.origin2[0]=100;
 sv.svEntities[1].clusternums[0]=0;SV_BuildClientSnapshot(&peers[0]);
 assert(frame.num_entities==2&&frame.ents[0]->number==1&&frame.ents[1]->number==2);
 assert(frame.areabytes==1&&frame.areabits[0]==byte(~3));

 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  setup(id,2);player={};player.origin[0]=10;player.origin[1]=20;player.origin[2]=30;player.viewheight=64;
  player.rtcw.leanf=16;peers[0].gentity=&entities[0];cameraReads=0;
  entities[0].s.origin2[0]=100;entities[0].s.origin2[1]=200;entities[0].s.origin2[2]=300;
  vec3_t origin{};SV_SnapshotViewOrigin(&peers[0],player,origin);
  assert(close(origin[0],10));
  assert(close(origin[1],id==COM_GAME_WOLFET?4.155710f:20));
  assert(close(origin[2],id==COM_GAME_WOLFET?91.773230f:94));
  assert(cameraReads==(id==COM_GAME_WOLFET?1:0));
  entities[0].r.svFlags=SVF_WOLFET_SELF_PORTAL_EXCLUSIVE;
  player.viewangles[YAW]=90;SV_SnapshotViewOrigin(&peers[0],player,origin);
  assert(close(origin[0],id==COM_GAME_WOLFET?115.844290f:10));
  assert(close(origin[1],id==COM_GAME_WOLFET?200:20));
  assert(close(origin[2],id==COM_GAME_WOLFET?361.773230f:94));
  player.rtcw.leanf=-16;player.viewangles[YAW]=0;SV_SnapshotViewOrigin(&peers[0],player,origin);
  assert(close(origin[1],id==COM_GAME_WOLFET?215.844290f:20));
  assert(close(origin[2],id==COM_GAME_WOLFET?361.773230f:94));
 }
 setup(COM_GAME_WOLFET,2);player={};peers[0].gentity=&entities[0];vec3_t origin{};
 const float nan=std::numeric_limits<float>::quiet_NaN();
 const float inf=std::numeric_limits<float>::infinity();
 for(float bad:{nan,inf,-inf}){
  player.rtcw.leanf=bad;rejected([&]{SV_SnapshotViewOrigin(&peers[0],player,origin);});
  player.rtcw.leanf=1;player.viewangles[YAW]=bad;rejected([&]{SV_SnapshotViewOrigin(&peers[0],player,origin);});
  player={};player.origin[0]=bad;rejected([&]{SV_SnapshotViewOrigin(&peers[0],player,origin);});
  player={};entities[0].r.svFlags=SVF_WOLFET_SELF_PORTAL_EXCLUSIVE;entities[0].s.origin2[0]=bad;
  rejected([&]{SV_SnapshotViewOrigin(&peers[0],player,origin);});
  entities[0].r.svFlags=SVF_WOLFET_SELF_PORTAL;
  rejected([&]{collect();});entities[0].r.svFlags=0;
 }
 peers[0].gentity=nullptr;rejected([&]{SV_SnapshotViewOrigin(&peers[0],player,origin);});
 setup(COM_GAME_WOLFET,2);SnapshotEntityNumbers numbers;clientSnapshot_t empty{};
 vec3_t badPoint{nan,0,0};rejected([&]{SV_AddEntitiesVisibleFromPoint(badPoint,&empty,numbers,false);});
 assert(viewpoints.empty());
}
'''

DENSE = r'''
int main(){
 static_assert(FNTECH3_WOLFET_MAX_ENTITIES_IN_SNAPSHOT==512); // Frozen cgame ABI.
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  setup(id,ENTITYNUM_NONE);player={};std::memset(peers,0,sizeof(peers));svs.clients=peers;
  peers[0].gentity=&entities[0];peers[0].state=CS_ACTIVE;
  for(auto &entity:entities)entity.r.svFlags=SVF_BROADCAST;
  auto &frame=peers[0].frames[0];frame.messageSent=12345;
  SV_BuildClientSnapshot(&peers[0]);
  const int expected=id==COM_GAME_WOLFET?ENTITYNUM_NONE-1:MAX_SNAPSHOT_ENTITIES;
  assert(frame.num_entities==expected);
  for(int i=0;i<expected;++i)assert(frame.ents[i]->number==i+1&&frame.ents[i]->number<ENTITYNUM_NONE);
  assert(frame.messageSent==12345&&calls==(id==COM_GAME_WOLFET?expected:0));
 }
}
'''


def function(text, signature):
    begin = text.index(signature)
    return text[begin:text.index('\n}', begin)+2]


class WolfETViewpointTests(unittest.TestCase):
    def source(self):
        harness = snapshots.WolfETSnapshotTests()
        text = (ROOT/'code/server/sv_snapshot.cpp').read_text(encoding='utf-8')
        source = harness.snapshot_source(True)
        source += '\n'+function(text, 'static void SV_SortEntityNumbers(')+SUPPORT
        math = (ROOT/'code/qcommon/q_math.c').read_text(encoding='utf-8')
        source += '\n'+function(math, 'void AngleVectors(')
        for signature in ('static void SV_SnapshotViewOrigin(', 'static void SV_BuildClientSnapshot('):
            source += '\n'+function(text, signature)
        return source

    def test_origin_visibility_cameras_lean_and_portal_cycles(self):
        snapshots.WolfETSnapshotTests().compile_source(self.source()+CHECKS)

    def test_dense_snapshot_preserves_the_separate_cgame_capacity(self):
        snapshots.WolfETSnapshotTests().compile_source(self.source()+DENSE)


if __name__ == '__main__':
    unittest.main()
