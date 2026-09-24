"""Execute RTCW movement-area lookup, real AAS geometry and native/QVM arguments."""
from pathlib import Path
import unittest
from rtcw_prediction_tests import prediction_source, SCENE
from rtcw_server_world_tests import function
import rtcw_botlib_tests as bootstrap

ROOT = Path(__file__).resolve().parents[1]


def movement_area_source():
    source = prediction_source() + r'''
#include "code/botlib/be_ai_goal.h"
#include "code/botlib/be_ai_move.h"
static constexpr int MODELTYPE_FUNC_PLAT=1,MODELTYPE_FUNC_BOB=2;
static int modeltypes[MAX_MODELS];
'''
    for filename, signatures in {
        'be_aas_bspq3.cpp': ('bsp_trace_t AAS_Trace(',),
        'be_aas_entity.cpp': ('int AAS_EntityModelindex(',),
        'be_aas_route.cpp': ('void AAS_ReachabilityFromNum(', 'int AAS_NextModelReachability('),
        'be_ai_move.cpp': ('int BotFuzzyPointReachabilityArea(', 'int BotReachabilityArea('),
    }.items():
        text = (ROOT/'code/botlib'/filename).read_text()
        for signature in signatures:
            source += '\n'+function(text, signature)
    return source


GEOMETRY = SCENE + r'''
static int bspCalls,expectedPass;
static bsp_trace_t ground;
static void groundTrace(bsp_trace_t *out,vec3_t start,vec3_t lo,vec3_t hi,vec3_t end,int pass,int mask){
 assert(lo[0]==-15&&lo[1]==-15&&lo[2]==-24&&hi[0]==15&&hi[1]==15&&hi[2]==8);
 assert(start[0]==end[0]&&start[1]==end[1]&&end[2]==start[2]-3);
 assert(pass==expectedPass&&mask==(CONTENTS_SOLID|CONTENTS_PLAYERCLIP));++bspCalls;*out=ground;
}
static void geometry(int world=0){
 if(world)scene(world);
 FreeMemory(aasworld.nodes);FreeMemory(aasworld.planes);FreeMemory(aasworld.areasettings);
 aasworld.numareas=aasworld.numareasettings=3;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(3);
 for(int a:{1,2})aasworld.areasettings[a].presencetype=PRESENCE_NORMAL|PRESENCE_CROUCH;
 aasworld.areasettings[1].numreachableareas=1;
 aasworld.numnodes=3;aasworld.nodes=botlib::ClearedAlloc<aas_node_t>(3);
 aasworld.numplanes=4;aasworld.planes=botlib::ClearedAlloc<aas_plane_t>(4);
 for(int p:{0,2}){aasworld.planes[p].normal[2]=1;aasworld.planes[p+1].normal[2]=-1;}
 aasworld.planes[0].dist=100;aasworld.planes[1].dist=-100;
 aasworld.nodes[1].planenum=0;aasworld.nodes[1].children[0]=-2;aasworld.nodes[1].children[1]=2;
 aasworld.nodes[2].planenum=2;aasworld.nodes[2].children[0]=-1;aasworld.nodes[2].children[1]=0;
 aasworld.reachabilitysize=3;aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(3);
 aasworld.reachability[1].traveltype=TRAVEL_ELEVATOR;aasworld.reachability[1].facenum=3;aasworld.reachability[1].areanum=1;
 aasworld.reachability[2].traveltype=TRAVEL_FUNCBOB;aasworld.reachability[2].facenum=0x20000003;aasworld.reachability[2].areanum=2;
 botimport.Trace=groundTrace;std::fill_n(modeltypes,MAX_MODELS,0);
 ground={};ground.fraction=.5f;ground.ent=1;contents=bspCalls=0;expectedPass=-1;
 AAS_EntityWorld().entities[1].i.modelindex=3;
}
static void finish(){
 for(int world=0;world<profile->bots.navigationWorlds;++world){AAS_SetCurrentWorld(world);
  if(aasworld.reachability){FreeMemory(aasworld.reachability);aasworld.reachability=nullptr;}}
 stop();
}
'''


class RTCWMovementAreaTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def run_source(self, checks):
        self.compile_and_run(movement_area_source()+GEOMETRY+checks,
            [str(ROOT/'code/server/server_game_data.cpp'), '-x', 'c++', str(ROOT/'code/compat/game_abi_adapter.c')],
            ['-O2', '-ffast-math'])

    def test_ground_movers_fuzzy_fall_disabled_areas_and_world_lifetime(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;setup();geometry();vec3_t origin{5,7,500};
  ground.ent=ENTITYNUM_WORLD;assert(BotReachabilityArea(origin,-1)==2);
  ground.ent=ENTITYNUM_NONE;assert(BotReachabilityArea(origin,-1)==2);
  ground.ent=1;ground.fraction=1;assert(BotReachabilityArea(origin,-1)==2);
  ground.fraction=.5f;ground.startsolid=qtrue;assert(BotReachabilityArea(origin,-1)==2);
  ground.startsolid=qfalse;assert(BotReachabilityArea(origin,-1)==1);
  contents=CONTENTS_SLIME;assert(BotReachabilityArea(origin,-1)==2);contents=0;
  modeltypes[3]=MODELTYPE_FUNC_PLAT;assert(BotReachabilityArea(origin,-1)==1);
  aasworld.reachability[1].traveltype=TRAVEL_ELEVATOR|TRAVELFLAG_NOTTEAM1;
  assert(AAS_NextModelReachability(0,3)==(Com_GameIsRTCW()?2:1));
  assert(BotReachabilityArea(origin,-1)==(Com_GameIsRTCW()?2:1));
  aasworld.reachability[1].traveltype=TRAVEL_ELEVATOR;
  assert(AAS_NextModelReachability(1,3)==2&&AAS_NextModelReachability(2,3)==0);
  modeltypes[3]=MODELTYPE_FUNC_BOB;assert(BotReachabilityArea(origin,-1)==1);
  modeltypes[3]=0;aasworld.areasettings[2].numreachableareas=1;
  assert(BotReachabilityArea(origin,-1)==2);
  aasworld.areasettings[2].areaflags=AREA_DISABLED;
  assert(AAS_AreaReachability(2)==(Com_GameIsRTCWSP()?0:1));
  assert(BotReachabilityArea(origin,-1)==(Com_GameIsRTCWSP()?1:2));
  aasworld.areasettings[2].areaflags=0;
  // Return the selected world's links; entity state is shared by RTCW worlds.
  if(Com_GameIsRTCW()){
   geometry(1);modeltypes[3]=MODELTYPE_FUNC_PLAT;aasworld.reachability[1].areanum=2;
   auto *selected=aas_currentWorld;assert(BotReachabilityArea(origin,-1)==2&&aas_currentWorld==selected);
   AAS_SetCurrentWorld(0);assert(BotReachabilityArea(origin,-1)==1);
   for(int pass:{-1,0,127,1023}){expectedPass=pass;assert(BotReachabilityArea(origin,pass)==1);}
   const int before=bspCalls;
   for(int pass:{INT_MIN,-2,1024,INT_MAX})assert(!BotReachabilityArea(origin,pass));
   assert(bspCalls==before);expectedPass=-1;
   for(int entity:{-1,1024,2047,INT_MAX}){ground.ent=entity;assert(!BotReachabilityArea(origin,-1));}
   ground.ent=1;
   for(int model:{INT_MIN,-1,256,511,INT_MAX}){
    AAS_EntityWorld().entities[1].i.modelindex=model;assert(!BotReachabilityArea(origin,-1));}
   AAS_EntityWorld().entities[1].i.modelindex=3;
   auto *nodes=aasworld.nodes;aasworld.nodes=nullptr;int calls=bspCalls;
   assert(!BotReachabilityArea(origin,-1)&&bspCalls==calls);aasworld.nodes=nodes;
   auto *settings=aasworld.areasettings;aasworld.areasettings=nullptr;
   assert(!BotReachabilityArea(origin,-1)&&!AAS_AreaReachability(1));aasworld.areasettings=settings;
   --aasworld.numareasettings;assert(!BotReachabilityArea(origin,-1)&&!AAS_AreaReachability(1));++aasworld.numareasettings;
   aasworld.initialized=qfalse;assert(!BotReachabilityArea(origin,-1)&&!AAS_NextModelReachability(0,3));aasworld.initialized=qtrue;
   for(unsigned bits:{0x48000001u,0x7f7fffffu,0x7f800000u,0x7fc00001u}){
    memcpy(&origin[0],&bits,4);assert(!BotReachabilityArea(origin,-1));}origin[0]=5;
   assert(!BotReachabilityArea(nullptr,-1));
   auto *entities=AAS_EntityWorld().entities;AAS_EntityWorld().entities=nullptr;
   assert(!BotReachabilityArea(origin,-1));AAS_EntityWorld().entities=entities;
   botimport.Trace=nullptr;assert(!BotReachabilityArea(origin,-1));botimport.Trace=groundTrace;
  }
  finish();if(Com_GameIsRTCW())assert(!BotReachabilityArea(origin,-1)&&!AAS_AreaReachability(0));
 }
}
''')

    def test_import_553_native_qvm_bounds_owned_vectors_and_results(self):
        self.run_source(r'''
static int calls,resultValue;static vec3_t inputPoint;static int inputEntity;
static int backend(vec3_t origin,int entity){++calls;VectorCopy(origin,inputPoint);inputEntity=entity;origin[0]=97;return resultValue;}
int main(){
 botlib_export_t api{};api.ai.BotReachabilityArea=backend;
 for(const auto &p:profiles){profile=&p;intptr_t result=19;
  if(!Com_GameIsRTCW()){assert(!SV_RTCWBotLibCall(nullptr,Call::BOTLIB_AI_REACHABILITY_AREA,nullptr,result)&&result==19);continue;}
  for(bool native:{false,true}){
   std::array<byte,128> memory;memory.fill(0xa5);vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=128;vm.dataMask=127;
   if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](int offset){return native?reinterpret_cast<intptr_t>(memory.data()+offset):intptr_t(offset);};
   auto invoke=[&](intptr_t pointer,intptr_t entity){intptr_t args[]{553,pointer,entity},out=17;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,553),args,out));return out;};
   vec3_t point{1,2,3};auto write=[&](int offset=1){memcpy(memory.data()+offset,point,12);};write();
   auto failure=[&](intptr_t pointer,intptr_t entity){const auto before=memory;int count=calls;
    rejected([&]{invoke(pointer,entity);});assert(memory==before&&calls==count);};
   botlib_export=nullptr;failure(address(1),0);botlib_export=&api;
   for(int expected:{0,1,65534})for(int entity:{-1,0,127,1023}){
    resultValue=expected;auto before=memory;assert(invoke(address(1),entity)==expected&&memory==before);
    assert(!memcmp(inputPoint,point,12)&&inputEntity==entity);
   }
   failure(0,0);for(int bad:{INT_MIN,-2,1024,2047,INT_MAX})failure(address(1),bad);
   for(unsigned bits:{0x48000001u,0x7f7fffffu,0x7f800000u,0xff800000u,0x7fc00001u}){
    write();memcpy(memory.data()+5,&bits,4);failure(address(1),0);}write();
   if(!native)for(int bad:{-1,117,127,129,INT_MAX})failure(bad,0);
   if(sizeof(intptr_t)>4)failure(address(1),static_cast<intptr_t>(INT64_C(0x100000000)));
   write(116);resultValue=3;auto before=memory;assert(invoke(address(116),0)==3&&memory==before);
   for(int invalid:{-1,65535,INT_MAX}){resultValue=invalid;before=memory;int count=calls;
    rejected([&]{invoke(address(1),0);});assert(memory==before&&calls==count+1);}
   assert(allocations.empty());
  }
 }
}
''')


if __name__ == '__main__':
    unittest.main()
