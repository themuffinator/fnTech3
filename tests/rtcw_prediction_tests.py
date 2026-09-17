"""Execute prediction, real AAS world traces and the frozen RTCW result adapter."""
from pathlib import Path
import re
import unittest
import rtcw_aas_query_tests as queries
import rtcw_navigation_tests as navigation
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def prediction_source():
    source = queries.query_source()
    source += r'''
#include "code/botlib/be_aas_sample.h"
#include "code/botlib/be_aas_move.h"
#include "code/botlib/rtcw_movement.hpp"
aas_settings_t aassettings;
static int debugLines, entityTraces;
void AAS_DebugLine(vec3_t,vec3_t,int){++debugLines;}
// This fixture exercises AAS world geometry. Dynamic BSP/entity collision
// responses are outside this corpus, and are never synthesized as a hit.
static int AAS_AreaEntityCollision(int,vec3_t,vec3_t,int,int,aas_trace_t*){++entityTraces;return 0;}
static constexpr float ON_EPSILON=0, TRACEPLANE_EPSILON=.125f;
'''
    sample = (ROOT / 'code/botlib/be_aas_sample.cpp').read_text()
    start = sample.index('struct aas_tracestack_t')
    source += '\n' + sample[start:sample.index('};', start) + 2]
    math = (ROOT / 'code/qcommon/q_math.c').read_text()
    for name in ('VectorNormalize',):
        signature = re.search(r'^(?:vec_t|float)\s+' + name + r'\(', math, re.M)[0]
        source += '\n' + function(math, signature)
    for filename, signatures in {
        'be_aas_sample.cpp': ('int AAS_PointPresenceType(', 'aas_plane_t *AAS_PlaneFromNum(',
            'int AAS_TraceAreas(', 'aas_trace_t AAS_TraceClientBBox('),
        'be_aas_move.cpp': ('void AAS_InitSettings(', 'int AAS_OnGround(',
            'static void AAS_Accelerate(', 'static void AAS_ApplyFriction(',
            'static qboolean AAS_ClipToBBox(', 'static int AAS_ClientMovementPrediction(',
            'int AAS_PredictClientMovement('),
    }.items():
        text = (ROOT / 'code/botlib' / filename).read_text()
        for signature in signatures:
            source += '\n' + function(text, signature)
    return source


SCENE = r'''
static void scene(int world=0){
 assert(AAS_SetCurrentWorld(world));
 aasworld.loaded=aasworld.initialized=1;
 aasworld.numareas=aasworld.numareasettings=3;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(3);
 for(int i=0;i<3;++i)aasworld.areasettings[i].presencetype=PRESENCE_NORMAL|PRESENCE_CROUCH;
 aasworld.numnodes=2;aasworld.nodes=botlib::ClearedAlloc<aas_node_t>(2);
 aasworld.numplanes=2;aasworld.planes=botlib::ClearedAlloc<aas_plane_t>(2);
 aasworld.planes[0].normal[2]=1;aasworld.planes[1].normal[2]=-1;
 aasworld.nodes[1].planenum=0;aasworld.nodes[1].children[0]=-1;aasworld.nodes[1].children[1]=0;
 contents=debugLines=entityTraces=0;botimport.PointContents=pointContents;
 AAS_InitSettings();
}
static void setup(){
 botimport.Print=print;
 LibVarSet("maxentities",std::to_string(profile->bots.maxEntities).c_str());
 assert(!AAS_Setup());scene();
}
static void stop(){AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());}
static aas_clientmove_t predict(int flags=0,int frames=1,float z=1,int cmdframes=0,const float *command=nullptr){
 vec3_t origin{0,0,z},velocity{200,0,0},zero{};aas_clientmove_t output;
 assert(AAS_PredictClientMovement(&output,-1,origin,PRESENCE_NORMAL,1,velocity,command?command:zero,
  cmdframes,frames,.1f,flags,1,0));
 return output;
}
'''


class RTCWPredictionTests(unittest.TestCase):
    compile_and_run = navigation.RTCWNavigationTests.compile_and_run

    def run_source(self, checks):
        self.compile_and_run(prediction_source() + SCENE + checks,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])

    def test_profile_physics_events_geometry_and_work_limits(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;setup();bool rtcw=Com_GameIsRTCW()!=0,sp=Com_GameIsRTCWSP()!=0;
  assert(aassettings.phys_maxwalkvelocity==(rtcw?300:320));
  assert(aassettings.phys_maxstep==(rtcw?18:19));
  assert(aassettings.phys_maxwaterjump==(rtcw?17:18));
  assert(fabs(aassettings.phys_maxbarrier-(sp?49:rtcw?44.7625:33))<.001);
  LibVarSet("phys_maxwalkvelocity","123");AAS_InitSettings();
  assert(aassettings.phys_maxwalkvelocity==(rtcw?300:123));
  LibVarSet("phys_maxwalkvelocity","320");AAS_InitSettings();
  auto dry=predict();assert(fabs(dry.velocity[0]-(rtcw?180:80))<.001&&dry.frames==1);
  contents=CONTENTS_WATER;auto wet=predict();assert(fabs(wet.velocity[0]-(rtcw?80:180))<.001);
  assert(predict(SE_ENTERWATER).endcontents==CONTENTS_WATER);contents=0;
  vec3_t inside{0,0,-1};assert(AAS_OnGround(inside,PRESENCE_NORMAL,-1)==sp);
  aasworld.areasettings[1].contents=AREACONTENTS_JUMPPAD;
  auto pad=predict(SE_TOUCHJUMPPAD);assert(pad.stopevent==(rtcw?SE_TOUCHJUMPPAD:0));
  assert(pad.frames==(rtcw?0:1));
  aasworld.areasettings[1].contents=AREACONTENTS_TELEPORTER;
  auto ordered=predict(SE_HITGROUND|SE_TOUCHTELEPORTER);
  assert(ordered.stopevent==(rtcw?SE_HITGROUND:SE_TOUCHTELEPORTER));
  aasworld.areasettings[1].contents=0;contents=CONTENTS_SLIME;
  assert(predict(SE_GAP,1,100).stopevent==(rtcw?0:SE_GAP));contents=0;
  vec3_t crouch{200,0,-400},jump{200,0,2};
  assert(predict(0,1,1,1,crouch).presencetype==PRESENCE_CROUCH);
  assert(predict(0,1,1,1,jump).endpos[2]>1);
  auto enter=predict(SE_ENTERAREA);assert(enter.stopevent==SE_ENTERAREA&&enter.frames==0);
  if(rtcw){
   assert(!predict(SE_HITBOUNDINGBOX|SE_TOUCHCLUSTERPORTAL).stopevent);
   vec3_t origin{0,0,1},zero{};aas_clientmove_t out{};
   assert(!AAS_PredictClientMovement(&out,-1,origin,2,0,zero,zero,0,4097,.1f,0,0,0));
   assert(!out.frames&&!out.endpos[2]);
   assert(AAS_PredictClientMovement(&out,-1,origin,2,0,zero,zero,0,-1,-1,0,0,0));
   assert(out.frames==0&&out.time==0&&out.endpos[2]==1.25f);
   assert(AAS_PredictClientMovement(&out,-1,origin,2,0,zero,zero,0,4096,.1f,0,0,0)&&out.frames==4096);
   auto *nodes=aasworld.nodes;aasworld.nodes=nullptr;
   assert(!AAS_PredictClientMovement(&out,-1,origin,2,0,zero,zero,0,1,.1f,0,0,0));aasworld.nodes=nodes;
   auto *planes=aasworld.planes;aasworld.planes=nullptr;
   assert(!AAS_PredictClientMovement(&out,-1,origin,2,0,zero,zero,0,1,.1f,0,0,0));aasworld.planes=planes;
   int settingsCount=aasworld.numareasettings;--aasworld.numareasettings;
   assert(!AAS_PredictClientMovement(&out,-1,origin,2,0,zero,zero,0,1,.1f,0,0,0));aasworld.numareasettings=settingsCount;
   for(uint32_t bits:{1u,0x7f7fffffu,0x7f800000u,0x7fc00001u}){
    volatile uint32_t raw=bits;uint32_t copy=raw;float duration;memcpy(&duration,&copy,4);
    vec3_t speed{131072,131072,131072};
    assert(!AAS_PredictClientMovement(&out,-1,origin,2,0,speed,zero,0,2,duration,0,0,0));
    aas_clientmove_t cleared{};assert(!memcmp(&out,&cleared,sizeof(out)));
   }
   origin[0]=131072;vec3_t excessive{131072,0,0};
   assert(!AAS_PredictClientMovement(&out,-1,origin,2,0,excessive,zero,0,1,.1f,0,0,0));origin[0]=0;
   int planeCount=aasworld.numplanes;aasworld.numplanes=0;
   assert(!AAS_PlaneFromNum(0)&&!AAS_OnGround(origin,2,-1));aasworld.numplanes=planeCount;
   assert(!AAS_PlaneFromNum(-1)&&!AAS_PlaneFromNum(planeCount));
   scene(1);aasworld.planes[0].dist=50;aasworld.planes[1].dist=-50;
   assert(predict(SE_HITGROUND,1,51).endpos[2]>50);AAS_SetCurrentWorld(0);
   assert(predict(SE_HITGROUND).endpos[2]<2);
   origin[2]=-1;memset(&out,0xa5,sizeof(out));
   assert(!AAS_PredictClientMovement(&out,-1,origin,2,0,zero,zero,0,1,.1f,0,0,0));
   aas_clientmove_t cleared{};assert(!memcmp(&out,&cleared,sizeof(out)));
   aasworld.loaded=0;origin[2]=1;
   assert(!AAS_PredictClientMovement(&out,-1,origin,2,0,zero,zero,0,1,.1f,0,0,0));
  }
  stop();
 }
}
''')

    def test_native_qvm_record_scalars_aliases_failures_and_release_showroute(self):
        self.run_source(r'''
static int calls,corrupt;
static int backend(aas_clientmove_t *out,int ent,const vec3_t pos,int presence,int ground,
 const vec3_t velocity,const vec3_t command,int commandFrames,int maximum,float dt,int events,int area,int visual){
 ++calls;int result=AAS_PredictClientMovement(out,ent,pos,presence,ground,velocity,command,
  commandFrames,maximum,dt,events,area,visual);
 if(corrupt==1)out->trace.fraction=2;
 if(corrupt==2)out->time=-1;
 if(corrupt==3)out->stopevent=2048;
 if(corrupt==5){uint32_t infinity=0x7f800000u;memcpy(out->velocity,&infinity,4);}
 if(corrupt==6){uint32_t nan=0x7fc00001u;memcpy(out->endpos,&nan,4);}
 return corrupt==4?2:result;
}
static intptr_t floatBits(float value){int bits;memcpy(&bits,&value,4);return bits;}
int main(){
 botlib_export_t table{};table.aas.AAS_PredictClientMovement=backend;botlib_export=&table;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()){
   for(auto op:{Call::BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT,Call::BOTLIB_AAS_RT_SHOWROUTE}){
    intptr_t result=71;assert(!SV_RTCWAASQueryCall(nullptr,op,nullptr,result)&&result==71);}
   continue;
  }
  intptr_t noOp=71;botlib_export=nullptr;
  assert(SV_RTCWAASQueryCall(nullptr,Call::BOTLIB_AAS_RT_SHOWROUTE,nullptr,noOp)&&noOp==0);
  botlib_export=&table;
  for(bool qvm:{false,true}){setup();std::array<byte,256> memory{};vm_t vm{};
   vm.dataBase=memory.data();vm.exactDataLength=memory.size();
   if(!qvm)vm.dllExports=reinterpret_cast<void*>(1);
   auto address=[&](int i)->intptr_t{return qvm?i:reinterpret_cast<intptr_t>(memory.data()+i);};
   intptr_t args[14]{};
   auto reset=[&](){memory.fill(0xa5);vec3_t pos{0,0,1},vel{200,0,0},cmd{};
    memcpy(memory.data()+1,pos,12);memcpy(memory.data()+17,vel,12);memcpy(memory.data()+33,cmd,12);
    intptr_t values[]={319,address(65),-1,address(1),2,1,address(17),address(33),0,1,floatBits(.1f),0,1,0};
    std::copy(std::begin(values),std::end(values),args);corrupt=0;};
   auto invoke=[&](){intptr_t result=71;assert(SV_RTCWAASQueryCall(&vm,Call::BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT,args,result));return result;};
   auto bad=[&](){auto saved=memory;int before=calls;rejected([&]{invoke();});assert(memory==saved&&calls==before);};
   reset();auto before=memory;assert(invoke()==1);
   fntech3_rtcw_aas_clientmove_t value;memcpy(&value,memory.data()+65,80);
   assert(value.frames==1&&fabs(value.velocity[0]-180)<.001&&value.presencetype==2);
   for(size_t i=0;i<memory.size();++i)if(i<65||i>=145)assert(memory[i]==before[i]);
   reset();contents=CONTENTS_WATER;args[11]=SE_ENTERWATER;assert(invoke()==1);
   memcpy(&value,memory.data()+65,80);assert(value.endcontents==float(CONTENTS_WATER));contents=0;
   reset();args[1]=address(1);assert(invoke()==1);memcpy(&value,memory.data()+1,80);assert(value.frames==1);
   reset();args[1]=address(176);assert(invoke()==1);
   for(int slot:{1,3,6,7}){reset();args[slot]=0;bad();}
   for(int slot:{2,4,5,8,9,11,12,13})if(sizeof(intptr_t)>4){reset();args[slot]=intptr_t(INT64_C(0x100000000));bad();}
   for(int value:{-2,1024,2047,p.bots.maxEntities,INT_MAX}){reset();args[2]=value;bad();}
   reset();args[2]=1023;assert(invoke()==1);
   for(int slot:{5,13}){reset();args[slot]=2;bad();}
   reset();args[9]=4097;bad();
   for(unsigned bits:{0x7f800000u,0xff800000u,0x7fc00001u}){
    reset();args[10]=static_cast<int32_t>(bits);bad();
    for(int offset:{1,17,33}){reset();memcpy(memory.data()+offset,&bits,4);bad();}
   }
   if(qvm){for(int slot:{3,6,7}){reset();args[slot]=245;bad();}reset();args[1]=177;bad();}
   for(int fault:{1,2,3,4,5,6}){reset();corrupt=fault;auto saved=memory;rejected([&]{invoke();});assert(saved==memory);}
   reset();float below=-1;memcpy(memory.data()+9,&below,4);assert(invoke()==0);
   fntech3_rtcw_aas_clientmove_t zero{};assert(!memcmp(memory.data()+65,&zero,80));
   reset();botlib_export=nullptr;bad();botlib_export=&table;
   stop();
  }
 }
}
''')


if __name__ == '__main__':
    unittest.main()
