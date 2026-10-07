"""Outcast bots must see linked BODY entities and be able to trace into them.

The retail QVM rejects an enemy with s.solid == 0 (instruction 12746), before
PVS or sight traces. jk2mp.exe 1.02a links contents 0x101 (0x450315) and builds
the temporary body hull with 0x100 (0x420e92). Exercise production linking,
the frozen module records and the real hull/trace code, without retail inputs.
"""
from pathlib import Path
import unittest

import entity_box_contents_tests as box
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
int main(){try{
 for(const auto &p:profiles){
  profile=p;
  Check(CM_BodyContents()==(p.id==COM_GAME_JK2_MP?0x100:0x2000000),"Profile body bit");
 }
 for(const auto &p:profiles)if(p.id==COM_GAME_JK2_MP)profile=p;
 Fixture_LoadEmptyWorld();
 constexpr int body=0x100;
 Check(Fixture_TempBoxContents()==body,"Retail box hull bit");
 struct Shared {
  int linked,linkcount,svFlags,singleClient,bmodel;
  float mins[3],maxs[3];int contents;
  float absmin[3],absmax[3],currentOrigin[3],currentAngles[3];int unnamed_96,ownerNum;
 };
 struct Entity {fntech3_jk2_entityState_t s;Shared r;};
 static_assert(sizeof(Entity)==400&&offsetof(Entity,r.contents)==FNTECH3_JK2_R_CONTENTS);
 static_assert(offsetof(Entity,r.ownerNum)==396); // retail 0x45093d and 0x4509cc
 constexpr std::size_t stride=sizeof(Entity)+48,base=16,count=2;
 std::vector<byte> memory(base+stride*count+16,0xa5),clients(sizeof(fntech3_jk2_playerState_t));
 for(int i=0;i<2;++i){
  Entity e{};e.s.number=i;e.r.contents=body;e.r.ownerNum=ENTITYNUM_NONE;
  e.r.mins[0]=e.r.mins[1]=-16;e.r.mins[2]=-24;
  e.r.maxs[0]=e.r.maxs[1]=16;e.r.maxs[2]=32;e.r.currentOrigin[0]=64.0f*i;
  std::memcpy(memory.data()+base+i*stride,&e,sizeof(e));
 }
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=memory.data()+base;binding.entitySpanBytes=count*stride;
 binding.entityCount=count;binding.entityStride=stride;
 binding.clientBase=clients.data();binding.clientSpanBytes=clients.size();binding.clientCount=1;binding.clientStride=clients.size();
 Check(serverGameData.BindNative(COM_GAME_JK2_MP,binding)==fntech3::server::GameDataBindError::None,"Binding");
 sv.gentities=binding.entityBase;sv.gentitySize=stride;sv.num_entities=count;
 auto read=[&](int i){Entity e;std::memcpy(&e,memory.data()+base+i*stride,sizeof(e));return e;};
 auto put=[&](int i,const Entity &e){std::memcpy(memory.data()+base+i*stride,&e,sizeof(e));};
 auto ptr=[&](int i){return reinterpret_cast<sharedEntity_t*>(memory.data()+base+i*stride);};
 const auto original=memory;
 SV_ClearWorld();SV_LinkEntity(ptr(0));SV_LinkEntity(ptr(1));
 for(int i=0;i<2;++i){
  const auto e=read(i);
  Check(e.s.solid==0x401810,"Bot enemy predicate sees a solid player");
  Check(e.r.contents==body&&e.r.linked&&e.r.linkcount==1,"Native contents and linkage");
  Check(e.r.unnamed_96==0&&e.r.ownerNum==ENTITYNUM_NONE,"Owner at 396; unnamed 392 untouched");
  Check(std::memcmp(memory.data()+base+i*stride+sizeof(Entity),original.data()+base+i*stride+sizeof(Entity),48)==0,"Private tail guard");
 }
 auto candidate=read(1);candidate.r.contents=0x2000000;put(1,candidate);SV_LinkEntity(ptr(1));
 Check(read(1).s.solid==0,"Q3 BODY is not a JK2 body");
 candidate.r.contents=0;put(1,candidate);SV_LinkEntity(ptr(1));Check(read(1).s.solid==0,"Non-solid entity");
 candidate.r.contents=1;put(1,candidate);SV_LinkEntity(ptr(1));Check(read(1).s.solid==0x401810,"Solid brush bit unchanged");
 candidate.r.contents=body;put(1,candidate);SV_LinkEntity(ptr(1));
 const auto beforeTrace=memory;
 vec3_t start{0,0,0},end{128,0,0};trace_t trace;
 SV_Trace(&trace,start,vec3_origin,vec3_origin,end,0,body|1,qfalse);
 Check(trace.entityNum==1&&trace.fraction>0&&trace.fraction<1&&trace.contents==body,"Shot hits linked enemy with native contents");
 SV_Trace(&trace,start,vec3_origin,vec3_origin,end,0,1,qfalse);
 Check(trace.fraction==1&&trace.entityNum==ENTITYNUM_NONE,"Sight ignores player bodies");
 SV_ClipToEntity(&trace,start,vec3_origin,vec3_origin,end,1,body,qfalse);
 Check(trace.fraction<1&&trace.entityNum==1&&trace.contents==body,"Single entity clip");
 vec3_t inside{64,0,0};
 SV_Trace(&trace,inside,vec3_origin,vec3_origin,inside,0,body,qfalse);
 Check(trace.allsolid&&trace.startsolid&&trace.fraction==0&&trace.contents==body,"Inside body");
 Check(SV_PointContents(inside,0)==body,"Point contents");
 Check(Fixture_TempBoxContents()==body&&memory==beforeTrace,"Trace preserves hull and module records");
 // The same hull is used for a client-side entity trace.
 const vec3_t lo{-16,-16,-24},hi{16,16,32};
 auto hull=CM_TempBoxModel(lo,hi,qfalse);
 CM_TransformedBoxTrace(&trace,start,end,vec3_origin,vec3_origin,hull,body,inside,vec3_origin,qfalse);
 Check(trace.fraction<1&&trace.contents==body,"Client box hull");
 // Retail's sphere/capsule hit writers also report 0x100.
 hull=CM_TempBoxModel(lo,hi,qtrue);
 const vec3_t smallLo{-1,-1,-1},smallHi{1,1,1};
 CM_TransformedBoxTrace(&trace,start,end,smallLo,smallHi,hull,body,inside,vec3_origin,qtrue);
 Check(trace.fraction<1&&trace.contents==body,"Capsule hit contents");
 SV_UnlinkEntity(ptr(1));SV_Trace(&trace,start,vec3_origin,vec3_origin,end,0,body,qfalse);
 Check(trace.fraction==1,"Unlinked body excluded");
 serverGameData.Invalidate();
 std::puts("JK2 bot collision passed");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
 catch(const Drop&){std::fprintf(stderr,"Unexpected drop\n");return 1;}}
'''


def source():
    game = (ROOT/'code/server/sv_game.cpp').read_text()
    world = (ROOT/'code/server/sv_world.cpp').read_text()
    prelude = box.REAL_PRELUDE
    start = prelude.index('static std::vector<int> touching;')
    end = prelude.index('static fntech3::server::ServerGameDataView', start)
    prelude = prelude[:start]+prelude[end:]
    prelude += '\n#include "code/compat/jk2_game_abi.h"\n#include <algorithm>\n#include <limits>\n'
    prelude += '\nint Com_GameIsQuake3IHV(){return profile.id==COM_GAME_QUAKE3_IHV;}\n'
    prelude += 'int Com_GameIsSTVEFMP(){return profile.id==COM_GAME_STVEF_MP;}\n'
    prelude += game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
    for signature in ['int\tSV_NumForGentity(', 'sharedEntity_t *SV_GentityNum(',
                      'qboolean SV_ReadGameEntity(', 'qboolean SV_WriteGameEntity(',
                      'qboolean SV_ReadGameEntityByNum(', 'svEntity_t\t*SV_SvEntityForGentity(']:
        prelude += '\n'+function(game, signature)
    prelude += '\n'+box.catalog()
    prelude += box.server_world('clipHandle_t SV_ClipHandleForEntity(')
    prelude += '\n'+world[world.index('struct worldSector_s {'):world.index('\nstruct moveclip_t {')]
    prelude += box.server_world('struct moveclip_t {', 'void SV_ClipToEntity(',
                               'static void SV_ClipMoveToEntities(', 'void SV_Trace(', 'int SV_PointContents(')
    return prelude+CHECKS


def collision_source():
    load = (ROOT/'code/qcommon/cm_load.c').read_text()
    return box.collision_source()+'\n'+function(load, 'int CM_LeafCluster(')+'\n'+function(load, 'int CM_LeafArea(')


class JK2BotCollisionTests(unittest.TestCase):
    def test_native_body_linking_and_real_collision(self):
        harness = box.EntityBoxContentsTests()
        result = harness.build_and_run(
            'jk2-bot-collision-', source(),
            extra_cpp=[str(ROOT/'code/server/server_game_data.cpp'), '-x', 'c++',
                       str(ROOT/'code/compat/game_abi_adapter.c')],
            c_sources=[('collision.c', collision_source()), ('code/qcommon/cm_trace.c', None),
                       ('code/qcommon/cm_test.c', None), ('code/qcommon/q_math.c', None)])
        self.assertIn('JK2 bot collision passed', result)


if __name__ == '__main__':
    unittest.main()
