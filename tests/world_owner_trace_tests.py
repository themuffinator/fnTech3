"""Trace with the world as the pass entity, as RTCW MP and ET dynamite does.

When a charge stops moving, the retail game makes the world, ENTITYNUM_WORLD,
its owner and passes that owner to its next trace. The server then reads the
world's own record, which lies past the entity count the module publishes, to
find the pass entity's owner. These tests run the production binding, entity
reads and trace aggregation over retail-sized entity arenas.
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
#include "code/compat/game_abi_adapter.h"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/stvef_game_abi.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
server_t sv{};
static comGameProfile_t profile{};
const comGameProfile_t *Com_GameProfile(){return &profile;}
int Com_GameIsRTCW(){return profile.id==COM_GAME_RTCW_SP||profile.id==COM_GAME_RTCW_MP;}
int Com_GameIsWolfET(){return profile.id==COM_GAME_WOLFET;}
struct Drop{};
void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
const vec3_t vec3_origin{};
static std::vector<int> touching,traced;
static void Check(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
int SV_AreaEntities(const vec3_t,const vec3_t,int *out,int maximum){
 Check(maximum==MAX_GENTITIES,"Contact limit");
 for(std::size_t i=0;i<touching.size();++i)out[i]=touching[i];return static_cast<int>(touching.size());
}
// Entity zero is a client, not collision model zero (the world).
clipHandle_t SV_ClipHandleForEntity(const sharedEntity_t *e,qboolean){return 64+SV_NumForGentity(const_cast<sharedEntity_t*>(e));}
void CM_BoxTrace(trace_t *out,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int,int,qboolean){*out={};out->fraction=1;}
void CM_SetTempBoxModelContents(int){}
void CM_TransformedBoxTrace(trace_t *out,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int model,int,const vec3_t,const vec3_t,qboolean){
 traced.push_back(model-64);*out={};out->fraction=1;
}
static fntech3::server::ServerGameDataView serverGameData;
'''

CHECKS = r'''
template<class Entity,class Player> void run(comGameId_t id,std::size_t stride,int expectedArena){
 profile.id=id;
 const int arena=SV_GameEntityArena();
 Check(arena==expectedArena,"Profile entity arena");
 const int records=arena?arena:68;
 // The module clears its whole arena before spawning, as G_InitGame does.
 std::vector<std::uint32_t> words((records*stride+32)/4,0);
 auto *base=reinterpret_cast<unsigned char*>(words.data())+16;
 std::memset(words.data(),0xa5,16);std::memset(base+records*stride,0xa5,16);
 auto put=[&](int number,int owner){
  Entity e{};e.s.number=number;e.r.ownerNum=owner;e.r.linked=1;e.r.contents=CONTENTS_SOLID;
  for(int axis=0;axis<3;++axis){e.r.mins[axis]=-4;e.r.maxs[axis]=4;}
  e.r.currentOrigin[0]=static_cast<float>(number);
  std::memcpy(base+number*stride,&e,sizeof(e));
 };
 put(0,ENTITYNUM_NONE);   // client 0
 put(1,ENTITYNUM_NONE);   // client 1
 put(64,ENTITYNUM_WORLD); // the charge that traces
 put(65,ENTITYNUM_WORLD); // another settled charge
 put(66,0);               // client 0's grenade
 put(67,1);               // client 1's grenade
 if(arena){
  // Retail SP_worldspawn numbers the world's record but never sets its owner.
  Entity world{};world.s.number=ENTITYNUM_WORLD;
  std::memcpy(base+ENTITYNUM_WORLD*stride,&world,sizeof(world));
 }
 std::vector<unsigned char> clients(sizeof(Player));
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=base;binding.entitySpanBytes=records*stride;binding.entityCount=68;
 binding.entityCapacity=arena;binding.entityStride=stride;
 binding.clientBase=clients.data();binding.clientSpanBytes=clients.size();binding.clientCount=1;binding.clientStride=sizeof(Player);
 Check(serverGameData.BindNative(id,binding)==fntech3::server::GameDataBindError::None,"Module binding");
 const auto before=words;
 touching={0,1,64,65,66,67};traced.clear();
 vec3_t start{0,0,0},end{0,0,-8},mins{-4,-4,0},maxs{4,4,8};trace_t result{};
 bool refused=false;
 try{SV_Trace(&result,start,mins,maxs,end,ENTITYNUM_WORLD,CONTENTS_SOLID,qfalse);}catch(Drop&){refused=true;}
 if(arena){
  // The world's owner is 0, so client 0's grenade is passed with the world's
  // own charges, exactly as the original engine's unchecked read gives.
  Check(!refused,"World pass entity refused");
  Check(traced==std::vector<int>{0,1,67},"World pass entity owner filtering");
  Check(result.fraction==1&&result.entityNum==ENTITYNUM_NONE,"World pass entity result");
 }else{
  // This change does not widen a profile whose module gave no such evidence.
  Check(refused&&traced.empty(),"A profile without an arena read past its published count");
 }
 Check(words==before,"Trace changed module records or guards");
 refused=false;
 try{SV_Trace(&result,start,mins,maxs,end,arena?arena:68,CONTENTS_SOLID,qfalse);}catch(Drop&){refused=true;}
 Check(refused,"Pass entity past the bound accepted");
 serverGameData.Invalidate();
}
int main(){
 try{
  run<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP,1352,1024);
  run<fntech3_wolfet_sharedEntity_t,fntech3_wolfet_playerState_t>(COM_GAME_WOLFET,1404,1024);
  run<fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_sp_playerState_t>(COM_GAME_RTCW_SP,
   (sizeof(fntech3_rtcw_sp_sharedEntity_t)+15)&~std::size_t(15),1024);
  run<fntech3_q3_sharedEntity_t,fntech3_q3_playerState_t>(COM_GAME_QUAKE3,
   (sizeof(fntech3_q3_sharedEntity_t)+15)&~std::size_t(15),0);
  run<fntech3_ql_sharedEntity_t,fntech3_ql_playerState_t>(COM_GAME_QUAKELIVE,
   (sizeof(fntech3_ql_sharedEntity_t)+15)&~std::size_t(15),0);
  for(auto id:{COM_GAME_STVEF_MP,COM_GAME_JK2_MP,COM_GAME_JKA_MP}){
   profile.id=id;Check(SV_GameEntityArena()==0,"Other profiles keep their published bound");
  }
 }catch(const Drop&){std::fprintf(stderr,"Unexpected engine rejection\n");return 1;}
 catch(const std::exception &e){std::fprintf(stderr,"%s (profile %d)\n",e.what(),static_cast<int>(profile.id));return 1;}
 std::puts("World pass-entity traces passed");return 0;
}
'''


def build_source() -> str:
    game = (ROOT/'code/server/sv_game.cpp').read_text(encoding='utf-8')
    world = (ROOT/'code/server/sv_world.cpp').read_text(encoding='utf-8')
    source = PRELUDE
    source += game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
    for signature in ['int\tSV_NumForGentity(', 'sharedEntity_t *SV_GentityNum(',
                      'qboolean SV_ReadGameEntity(', 'qboolean SV_ReadGameEntityByNum(',
                      'static int SV_GameEntityArena(']:
        source += '\n'+function(game, signature)
    start = world.index('struct moveclip_t {')
    source += '\n'+world[start:world.index('\n};', start)+3]
    for signature in ['static void SV_ClipMoveToEntities(', 'void SV_Trace(']:
        source += '\n'+function(world, signature)
    return source+CHECKS


class WorldOwnerTraceTests(unittest.TestCase):
    def test_world_pass_entity_reads_the_module_arena(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        game = (ROOT/'code/server/sv_game.cpp').read_text(encoding='utf-8')
        locate = function(game, 'static void SV_LocateGameData(')
        self.assertIn('binding.entityCapacity = SV_GameEntityArena();', locate)
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        scratch = ROOT/'.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='world-owner-trace-', dir=scratch) as folder:
            folder = Path(folder)
            cpp = folder/'probe.cpp'
            exe = folder/'probe.exe'
            cpp.write_text(build_source(), encoding='utf-8')
            command = [compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp),
                       str(ROOT/'code/server/server_game_data.cpp'),
                       '-x', 'c++', str(ROOT/'code/compat/game_abi_adapter.c'), '-o', str(exe)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(built.returncode, 0, built.stdout+built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout+run.stderr)


if __name__ == '__main__':
    unittest.main()
