"""Exercise real server entity traces and AAS entity selection across profiles."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap
import rtcw_server_world_tests as world
from rtcw_prediction_tests import prediction_source, SCENE
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def server_source():
    source = world.PRELUDE[:world.PRELUDE.index('const vec3_t vec3_origin')]
    source += r'''
#include "code/botlib/botlib.h"
const vec3_t vec3_origin{};
static int hullCalls,traceCalls,hullCapsule,traceCapsule,traceModel;
static vec3_t observedMins,observedMaxs,observedOrigin,observedAngles;
static trace_t collision;
int CM_NumInlineModels(){return 4;}
clipHandle_t CM_InlineModel(int model){if(model<0||model>=4)throw Drop{};return model;}
clipHandle_t CM_TempBoxModel(const vec3_t lo,const vec3_t hi,int capsule){
 ++hullCalls;hullCapsule=capsule;VectorCopy(lo,observedMins);VectorCopy(hi,observedMaxs);return capsule?510:511;
}
void CM_TransformedBoxTrace(trace_t *out,const vec3_t start,const vec3_t end,const vec3_t lo,
 const vec3_t hi,clipHandle_t model,int mask,const vec3_t origin,const vec3_t angles,qboolean capsule){
 assert(start[0]==1&&end[0]==81&&lo[2]==-24&&hi[2]==32&&mask==CONTENTS_SOLID);
 ++traceCalls;traceCapsule=capsule;traceModel=model;
 VectorCopy(origin,observedOrigin);VectorCopy(angles,observedAngles);*out=collision;
}
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
'''
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
    source += catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
    game = (ROOT / 'code/server/sv_game.cpp').read_text()
    source += game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
    for signature in ('sharedEntity_t *SV_GentityNum(', 'qboolean SV_ReadGameEntity('):
        source += '\n' + function(game, signature)
    server = (ROOT / 'code/server/sv_world.cpp').read_text()
    for signature in ('clipHandle_t SV_ClipHandleForEntity(', 'void SV_ClipToEntity('):
        source += '\n' + function(server, signature)
    source += '\n' + function((ROOT / 'code/server/sv_bot.cpp').read_text(), 'static void BotImport_EntityTrace(')
    return source


def aas_source():
    source = prediction_source()
    collision = function((ROOT / 'code/botlib/be_aas_bspq3.cpp').read_text(), 'qboolean AAS_EntityCollision(')
    collision += '\n' + function((ROOT / 'code/botlib/be_aas_sample.cpp').read_text(), 'static qboolean AAS_AreaEntityCollision(')
    return source.replace('static int AAS_AreaEntityCollision(int,vec3_t,vec3_t,int,int,aas_trace_t*){++entityTraces;return 0;}', collision)


class RTCWEntityCollisionTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def run_source(self, source):
        self.compile_and_run(source,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++', str(ROOT / 'code/compat/game_abi_adapter.c')],
            ['-O2', '-ffast-math'])

    def test_server_hulls_sweeps_and_bot_trace_with_private_game_records(self):
        self.run_source(server_source() + r'''
template<class Entity,class Player> void run(comGameId_t id){
 for(const auto &p:profiles)if(p.id==id)profile=p;
 const bool rtcw=Com_GameIsRTCW()!=0;
 constexpr size_t count=3,base=16,stride=sizeof(Entity)+64;
 std::vector<byte> entities(base+count*stride+16,0xa5),clients(sizeof(Player));
 for(size_t n=0;n<count;++n){Entity initial{};initial.s.number=int(n);initial.r.s.number=791;
  initial.r.mins[0]=-12;initial.r.mins[1]=-13;initial.r.mins[2]=-24;
  initial.r.maxs[0]=12;initial.r.maxs[1]=13;initial.r.maxs[2]=32;
  initial.r.currentOrigin[0]=100;initial.r.currentOrigin[1]=200;initial.r.currentOrigin[2]=300;
  initial.r.currentAngles[1]=90;initial.r.contents=CONTENTS_SOLID;
  memcpy(entities.data()+base+n*stride,&initial,sizeof(initial));}
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=entities.data()+base;binding.entitySpanBytes=count*stride;binding.entityCount=count;binding.entityStride=stride;
 binding.clientBase=clients.data();binding.clientSpanBytes=clients.size();binding.clientCount=1;binding.clientStride=sizeof(Player);
 assert(serverGameData.BindNative(id,binding)==fntech3::server::GameDataBindError::None);
 auto read=[&](){Entity e;memcpy(&e,entities.data()+base+stride,sizeof(e));return e;};
 auto write=[&](const Entity &e){memcpy(entities.data()+base+stride,&e,sizeof(e));};
 vec3_t start{1,2,3},end{81,82,83},lo{-15,-15,-24},hi{15,15,32};
 collision={};collision.fraction=.375f;collision.startsolid=qtrue;collision.entityNum=73;
 collision.endpos[0]=31;collision.endpos[1]=32;collision.endpos[2]=33;
 collision.plane.normal[0]=1;collision.plane.dist=17;collision.plane.type=1;collision.plane.signbits=4;
 collision.surfaceFlags=0x40000040;collision.contents=CONTENTS_SOLID;
 for(bool brush:{false,true})for(bool flag:{false,true})for(bool sweep:{false,true}){
  auto e=read();e.r.bmodel=brush;e.s.modelindex=2;
  e.r.svFlags=flag?(rtcw?FNTECH3_RTCW_SVF_CAPSULE:SVF_CAPSULE):0;write(e);
  const auto unchanged=entities;hullCalls=traceCalls=0;trace_t result{};
  SV_ClipToEntity(&result,start,lo,hi,end,1,CONTENTS_SOLID,sweep?qtrue:qfalse);
  assert(entities==unchanged&&traceCalls==1&&traceCapsule==sweep);
  const bool capsule=flag&&(rtcw||sweep);
  assert(traceModel==(brush?2:capsule?510:511)&&hullCalls==!brush);
  if(!brush){assert(hullCapsule==capsule&&!memcmp(observedMins,e.r.mins,12)&&!memcmp(observedMaxs,e.r.maxs,12));}
  assert(!memcmp(observedOrigin,e.r.currentOrigin,12)&&observedAngles[1]==(brush?90:0));
  auto expected=collision;expected.entityNum=1;assert(!memcmp(&result,&expected,sizeof(result)));
  hullCalls=traceCalls=0;bsp_trace_t bot{};
  BotImport_EntityTrace(&bot,start,lo,hi,end,1,CONTENTS_SOLID);
  assert(!traceCapsule&&traceModel==(brush?2:flag&&rtcw?510:511)&&entities==unchanged);
  assert(bot.fraction==collision.fraction&&bot.startsolid&&bot.ent==1&&!memcmp(bot.endpos,collision.endpos,12));
  assert(bot.plane.dist==17&&bot.plane.normal[0]==1&&bot.plane.type==1&&bot.plane.signbits==4);
  assert(bot.surface.flags==collision.surfaceFlags&&!bot.surface.value&&!bot.contents&&!bot.exp_dist&&!bot.sidenum);
 }
 auto e=read();e.r.contents=0;write(e);const auto unchanged=entities;
 hullCalls=traceCalls=0;bsp_trace_t bot;memset(&bot,0xa5,sizeof(bot));
 BotImport_EntityTrace(&bot,start,lo,hi,end,1,CONTENTS_SOLID);
 assert(bot.fraction==1&&!traceCalls&&!hullCalls&&!bot.startsolid&&!bot.ent&&entities==unchanged);
 for(int invalid:{-1,3,1024,INT_MAX})rejected([&]{BotImport_EntityTrace(&bot,start,lo,hi,end,invalid,CONTENTS_SOLID);});
 serverGameData.Invalidate();rejected([&]{BotImport_EntityTrace(&bot,start,lo,hi,end,1,CONTENTS_SOLID);});
 assert(!traceCalls&&entities==unchanged);
}
int main(){
 run<fntech3_q3_sharedEntity_t,fntech3_q3_playerState_t>(COM_GAME_QUAKE3);
 run<fntech3_ql_sharedEntity_t,fntech3_ql_playerState_t>(COM_GAME_QUAKELIVE);
 run<fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_sp_playerState_t>(COM_GAME_RTCW_SP);
 run<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP);
}
''')

    def test_aas_nearest_entity_pass_filter_ties_and_selected_world(self):
        self.run_source(aas_source() + SCENE + r'''
static std::array<float,4> fractions;
static std::vector<int> visited;
static int solidEntity;
static int expectedPresence=PRESENCE_NORMAL;
static void entityTrace(bsp_trace_t *out,vec3_t start,vec3_t lo,vec3_t hi,vec3_t end,int entity,int mask){
 assert(entity>0&&entity<4&&mask==(CONTENTS_SOLID|CONTENTS_PLAYERCLIP));visited.push_back(entity);
 assert(lo[0]==-15&&lo[1]==-15&&lo[2]==-24&&hi[0]==15&&hi[1]==15&&hi[2]==(expectedPresence==PRESENCE_NORMAL?32:8));
 *out={};out->fraction=fractions[entity];out->ent=entity;out->startsolid=entity==solidEntity?qtrue:qfalse;
 for(int axis=0;axis<3;++axis)out->endpos[axis]=start[axis]+(end[axis]-start[axis])*out->fraction;
}
static void links(){
 AAS_InitAASLinkHeap();AAS_InitAASLinkedEntities();
 vec3_t lo{1,1,32},hi{2,2,48};
 for(int entity:{1,2,3})assert(AAS_AASLinkEntity(lo,hi,entity));
}
int main(){
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;setup();links();botimport.EntityTrace=entityTrace;
  vec3_t start{0,0,64},end{100,0,64};fractions={1,.75f,.25f,.25f};solidEntity=0;
  for(int presence:{PRESENCE_NORMAL,PRESENCE_CROUCH}){expectedPresence=presence;
   for(int pass:{0,1,2,3}){aas_trace_t out{};out.fraction=.875f;out.lastarea=77;visited.clear();
    assert(AAS_AreaEntityCollision(1,start,end,presence,pass,&out));
    assert(out.ent==(pass==3?2:3)&&out.endpos[0]==25&&out.fraction==.875f&&out.lastarea==77);
    assert(!out.area&&!out.planenum&&!out.startsolid);
    std::vector<int> expected;for(int e:{3,2,1})if(e!=pass)expected.push_back(e);assert(visited==expected);
   }
  }
  expectedPresence=PRESENCE_NORMAL;visited.clear();
  auto hit=AAS_TraceClientBBox(start,end,PRESENCE_NORMAL,3);
  assert(hit.ent==2&&hit.fraction==.25f&&hit.endpos[0]==25);
  visited.clear();auto ignored=AAS_TraceClientBBox(start,end,PRESENCE_NORMAL,-1);
  assert(ignored.fraction==1&&visited.empty());
  fractions[2]=0;solidEntity=2;hit=AAS_TraceClientBBox(start,end,PRESENCE_NORMAL,3);
  assert(hit.startsolid&&hit.fraction==0&&hit.ent==2&&!memcmp(hit.endpos,start,12));
  fractions={1,1,1,1};solidEntity=0;aas_trace_t out;memset(&out,0xa5,sizeof(out));auto unchanged=out;
  assert(!AAS_AreaEntityCollision(1,start,end,PRESENCE_NORMAL,0,&out)&&!memcmp(&out,&unchanged,sizeof(out)));
  if(Com_GameIsRTCW()){
   scene(1);AAS_InitAASLinkedEntities();visited.clear();fractions[2]=.25f;
   assert(AAS_TraceClientBBox(start,end,PRESENCE_NORMAL,3).fraction==1&&visited.empty());
   AAS_SetCurrentWorld(0);assert(AAS_TraceClientBBox(start,end,PRESENCE_NORMAL,3).ent==2);
  }
  stop();
 }
}
''')


if __name__ == '__main__':
    unittest.main()
