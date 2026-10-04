"""Execute RTCW frame initialization, cost tables and profile boundaries."""
from pathlib import Path
import unittest
import rtcw_aas_tests
import rtcw_aas_load_tests
import rtcw_routecache_load_tests
import rtcw_routecache_tests
import rtcw_botlib_tests
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def routing_source():
    source = rtcw_routecache_load_tests.loader_source()
    source += '\n#include "code/compat/rtcw_routing.hpp"\n'
    source += '\n#include "code/compat/wolfet_navigation.hpp"\n'
    reach = (ROOT / 'code/botlib/be_aas_reach.cpp').read_text()
    for signature in ('int AAS_AreaCrouch(', 'int AAS_AreaSwim('):
        source += function(reach, signature) + '\n'
    route = (ROOT / 'code/botlib/be_aas_route.cpp').read_text()
    source += route[route.index('static constexpr float DISTANCEFACTOR_CROUCH'):route.index('//cache refresh time')]
    for signature in ('static void AAS_InitTravelFlagFromType(', 'static ID_INLINE int AAS_TravelFlagForType_inline(',
                      'int AAS_TravelFlagForType(', 'static int AAS_GetAreaContentsTravelFlags(',
                      'static ID_INLINE int AAS_AreaContentsTravelFlags_inline(', 'int AAS_AreaContentsTravelFlags(',
                      'static void AAS_InitAreaContentsTravelFlags(', 'unsigned short int AAS_AreaTravelTime(',
                      'static int AAS_PortalMaxTravelTime(', 'static void AAS_InitPortalMaxTravelTimes(',
                      'static void AAS_InitRoutingLimits('):
        source += function(route, signature) + '\n'
    source += r'''
#include "code/botlib/rtcw_routing_init.hpp"
// Cache regeneration has its own fixture with real tracing and serialization.
static bool AAS_BuildRTCWRouteCache(){return false;}
static int legacyContinues,legacyClusters,legacyOptimizes,legacyWrites,legacyRouting,legacyUnlinks,legacyCaches;
int AAS_ContinueInitReachability(float){++legacyContinues;return 0;}
void AAS_InitClustering(){++legacyClusters;}void AAS_Optimize(){++legacyOptimizes;}
qboolean AAS_WriteAASFile(const char*){++legacyWrites;return qtrue;}
void AAS_InitRouting(){assert(!Com_GameIsRTCW());++legacyRouting;}
void AAS_UnlinkInvalidEntities(){++legacyUnlinks;}
void AAS_RoutingInfo(){}void PrintUsedMemorySize(){}void PrintMemoryLabels(){}
void AAS_WriteRouteCache(){++legacyCaches;}
static int exits;
static void QDECL checkedPrint(int type,const char*,...){if(type==PRT_EXIT){++exits;throw Drop{};}}
'''
    main = (ROOT / 'code/botlib/be_aas_main.cpp').read_text()
    for signature in ('static void AAS_SetInitialized(', 'void AAS_ContinueInit(', 'int AAS_StartFrame(',
                      'int AAS_Initialized(', 'float AAS_Time('):
        source += function(main, signature) + '\n'
    return source


class RTCWRoutingTests(unittest.TestCase):
    compile_and_run = rtcw_botlib_tests.RTCWBotLibTests.compile_and_run

    def test_initialized_and_time_imports_follow_world_and_float_bits(self):
        source = routing_source() + r'''
int main(){
 using namespace fntech3::rtcw::module;
 botlib_export_t exports{};exports.aas.AAS_Initialized=AAS_Initialized;exports.aas.AAS_Time=AAS_Time;
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;
  for(bool native:{false,true}){initialize();botlib_export=&exports;vm_t vm{};
   if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto invoke=[&](Call call){intptr_t result=73;
    // Neither no-argument call may read the syscall argument array.
    assert(SV_RTCWBotLibCall(&vm,call,nullptr,result));return result;};
   for(int world=0;world<2;++world){assert(AAS_SetCurrentWorld(world));
    assert(invoke(Call::BOTLIB_AAS_INITIALIZED)==0&&invoke(Call::BOTLIB_AAS_TIME)==0);
    aasworld.initialized=qtrue;assert(invoke(Call::BOTLIB_AAS_INITIALIZED)==1);
    for(uint32_t bits:{0u,0x80000000u,0x3fa00000u,0xbfa00000u,0x7f7fffffu}){
     std::memcpy(&aasworld.time,&bits,4);int expected;std::memcpy(&expected,&bits,4);
     assert(invoke(Call::BOTLIB_AAS_TIME)==expected);
    }
   }
   AAS_SetCurrentWorld(0);aasworld.initialized=qfalse;aasworld.time=0;
   assert(invoke(Call::BOTLIB_AAS_INITIALIZED)==0&&invoke(Call::BOTLIB_AAS_TIME)==0);
   AAS_SetCurrentWorld(1);assert(invoke(Call::BOTLIB_AAS_INITIALIZED)==1&&invoke(Call::BOTLIB_AAS_TIME)==0x7f7fffff);
   shutdown();assert(invoke(Call::BOTLIB_AAS_INITIALIZED)==0&&invoke(Call::BOTLIB_AAS_TIME)==0);
   botlib_export=nullptr;for(auto call:{Call::BOTLIB_AAS_INITIALIZED,Call::BOTLIB_AAS_TIME}){
    try{invoke(call);assert(false);}catch(Drop&){}
   }
  }
 }
}
'''
        self.compile_and_run(source, self.extra(), ['-O2', '-ffast-math'])

    def test_cost_arithmetic_flags_bounds_and_legacy_profile_values(self):
        source = routing_source() + r'''
namespace routing=fntech3::rtcw::routing;
static float fromBits(uint32_t bits){volatile uint32_t input=bits;bits=input;float value;std::memcpy(&value,&bits,4);return value;}
int main(){
 for(float value:{0.0f,-1.0f,fromBits(0x7fc00000),fromBits(0x7f800000),1048577.0f})
  assert(routing::CacheBudget(value)==4096*1024);
 assert(routing::CacheBudget(1.999f)==1024&&routing::CacheBudget(4096.5f)==4096*1024);
 assert(routing::CacheBudget(1048576)==1073741824);
 float start[3]={},end[3]={3,4,0};uint16_t cost=99;
 assert(routing::AreaTravelTime(start,end,0,false,false,true,cost)&&cost==2);
 assert(routing::AreaTravelTime(start,end,0,false,false,false,cost)&&cost==1);
 assert(routing::AreaTravelTime(start,end,.5f,false,false,true,cost)&&cost==18);
 assert(routing::AreaTravelTime(start,end,.5f,false,false,false,cost)&&cost==17);
 assert(routing::AreaTravelTime(start,end,0,true,false,true,cost)&&cost==7);
 assert(routing::AreaTravelTime(start,end,0,false,true,true,cost)&&cost==5);
 assert(routing::AreaTravelTime(start,start,1,true,true,true,cost)&&cost==1);
 end[0]=65536;end[1]=0;assert(routing::AreaTravelTime(start,end,0,false,true,true,cost)&&cost==0);
 end[0]=131072;assert(routing::AreaTravelTime(start,end,1,true,true,true,cost));
 cost=73;end[0]=131073;assert(!routing::AreaTravelTime(start,end,0,false,false,true,cost)&&cost==73);
 end[0]=3;for(float invalid:{-1.0f,2.0f,fromBits(0x7fc00000),fromBits(0x7f800000)}){
  assert(!routing::AreaTravelTime(start,end,invalid,false,false,true,cost)&&cost==73);
 }
 for(uint32_t bits:{0x7fc00000u,0x7f800000u,0xff800000u}){float invalid=fromBits(bits);
  end[0]=invalid;assert(!routing::AreaTravelTime(start,end,0,false,false,true,cost)&&cost==73);
 }
 assert(!routing::AreaTravelTime(nullptr,end,0,false,false,true,cost)&&cost==73);
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;initialize();aasworld.numareas=aasworld.numareasettings=3;
  aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(3);
  aasworld.rtcwGroundSteepness=botlib::ClearedAlloc<float>(3);AAS_InitTravelFlagFromType();
  auto &settings=aasworld.areasettings[1];settings.presencetype=PRESENCE_NORMAL;
  float a[3]={},b[3]={3,4,0};assert(AAS_AreaTravelTime(1,a,b)==(Com_GameIsRTCWSP()||Com_GameIsWolfET()?2:1));
  // Holomatch's engine has neither RTCW's bit for 1024 nor Quake III 1.32's
  // team and bridge flags, and a liquid takes its own flag alone
  // (stvoyHM.exe 0x00499290).
  settings.contents=1024|2048|4096|256;settings.areaflags=AREA_BRIDGE;
  assert(AAS_GetAreaContentsTravelFlags(1)==((Com_GameIsRTCW()||Com_GameIsWolfET())?
   (0x00080000|0x00800000|0x02000000):Com_GameIsSTVEFMP()?(TFL_AIR|TFL_DONOTENTER):
   (TFL_AIR|TFL_DONOTENTER|TFL_NOTTEAM1|TFL_NOTTEAM2|TFL_BRIDGE)));
  settings.contents|=1;assert(AAS_GetAreaContentsTravelFlags(1)==(Com_GameIsRTCW()||Com_GameIsWolfET()||Com_GameIsSTVEFMP()?TFL_WATER:
   (TFL_WATER|TFL_DONOTENTER|TFL_NOTTEAM1|TFL_NOTTEAM2|TFL_BRIDGE)));
  assert(AAS_TravelFlagForType(TRAVEL_WALK|TRAVELFLAG_NOTTEAM1)==(Com_GameIsRTCW()||Com_GameIsWolfET()?TFL_INVALID:TFL_WALK|TFL_NOTTEAM1));
  assert(AAS_TravelFlagForType(-1)==TFL_INVALID&&AAS_TravelFlagForType(32)==TFL_INVALID);
  shutdown();
 }
}
'''
        self.compile_and_run(source, self.extra(), ['-O2', '-ffast-math'])

    @staticmethod
    def extra():
        return [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++', str(ROOT / 'code/compat/game_abi_adapter.c')]

    def test_two_world_frames_complete_tables_failure_cleanup_and_reload(self):
        aas = rtcw_aas_tests.synthetic_aas()
        cache = rtcw_routecache_tests.synthetic_cache(aas)
        source = routing_source()
        source += '\nstatic const byte initialMap[]={' + ','.join(map(str, aas)) + '};\n'
        source += '\nstatic const byte initialCache[]={' + ','.join(map(str, cache)) + '};\n'
        source += '\nstatic const byte quakeMap[]={' + ','.join(map(str, rtcw_aas_load_tests.quake_aas(aas))) + '};\n'
        source += r'''
static void loadMap(int world){
 AAS_SetCurrentWorld(world);files["maps/probe.aas"]={std::begin(initialMap),std::end(initialMap)};
 assert(AAS_LoadAASFile("maps/probe.aas")==0);Q_strncpyz(aasworld.mapname,"probe",sizeof(aasworld.mapname));
 files["maps/probe.rcd"]={std::begin(initialCache),std::end(initialCache)};
 if(!Com_GameIsRTCWSP())files["maps/probe.rcd"][4]=12;
}
static void checkTables(){
 assert(aasworld.initialized&&aasworld.rtcwRouteCacheLoaded&&aasworld.areaupdate&&aasworld.portalupdate);
 assert(aasworld.areaupdate[2].areanum==0);aasworld.areaupdate[2].areanum=77;aasworld.areaupdate[2].areanum=0;
 assert(aasworld.reversedreachability[1].numlinks==1&&aasworld.reversedreachability[2].numlinks==1);
 assert(aasworld.reversedreachability[1].first->linknum==1&&aasworld.reversedreachability[1].first->areanum==2);
 assert(aasworld.reversedreachability[2].first->linknum==0&&aasworld.reversedreachability[2].first->areanum==1);
 assert(aasworld.areatraveltimes[1][0][0]==1&&aasworld.areatraveltimes[2][0][0]==(Com_GameIsRTCWSP()?2:1));
 assert(aasworld.portalmaxtraveltimes[0]==0&&aasworld.max_routingcachesize==4096*1024);
 assert(!aasworld.reachabilityareas&&!aasworld.reachabilityareaindex);
}
int main(){
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;initialize();botimport.Print=checkedPrint;
  if(!Com_GameIsRTCW()){
   // ET owns AAS8; the other single-world profiles retain their Q3 AAS5 control.
   const char *map=Com_GameIsWolfET()?"maps/et.aas":"maps/q3.aas";
   if(Com_GameIsWolfET())files[map]={std::begin(initialMap),std::end(initialMap)};
   else files[map]={std::begin(quakeMap),std::end(quakeMap)};
   assert(AAS_LoadAASFile(map)==0);
   assert((aasworld.rtcwGroundSteepness!=nullptr)==Com_GameIsWolfET());
   const int before=legacyRouting;assert(AAS_StartFrame(3)==0&&aasworld.initialized&&aasworld.numframes==1);
   assert(legacyRouting==before+1&&legacyUnlinks>0&&!aasworld.rtcwRouteCacheLoaded);shutdown();continue;
  }
  const int unlinks=legacyUnlinks,continues=legacyContinues;
  assert(AAS_StartFrame(0)==0&&aas_currentWorld==&aasWorlds[1]&&aasworld.numframes==1&&!aasworld.initialized);
  loadMap(0);loadMap(1);aasWorlds[0].entities[1].i.valid=qtrue;
  assert(AAS_StartFrame(1.25f)==0&&aas_currentWorld==&aasWorlds[1]&&aasworld.numframes==2);
  assert(!aasWorlds[0].entities[1].i.valid&&aasWorlds[0].entities[1].i.number==1);
  assert(legacyUnlinks==unlinks&&legacyContinues==continues);
  for(int world=0;world<2;++world){AAS_SetCurrentWorld(world);checkTables();assert(AAS_Time()==1.25f);}
  auto *first=aasWorlds[0].areatraveltimes;auto *second=aasWorlds[1].areatraveltimes;assert(first!=second);
  const auto openedBefore=opened.size();aasworld.numframes=INT_MAX;aasworld.frameroutingupdates=123;
  assert(AAS_StartFrame(-0.0f)==0&&aasworld.numframes==INT_MAX&&aasworld.frameroutingupdates==0);
  assert(fntech3::rtcw::routing::Bits(aasWorlds[0].time)==0x80000000u&&aasWorlds[0].areatraveltimes==first
   &&aasWorlds[1].areatraveltimes==second&&opened.size()==openedBefore&&aasWorlds[0].numframes==0);
  files["maps/reload_b0.aas"]={std::begin(initialMap),std::end(initialMap)};
  files["maps/reload_b0.rcd"]=files.at("maps/probe.rcd");
  assert(AAS_LoadMap("reload")==0);assert(AAS_StartFrame(3)==0&&aasWorlds[0].initialized&&!aasWorlds[1].initialized);
  shutdown();
 }
 for(int count:{129,256}){profile=&profiles[2];initialize();botimport.Print=checkedPrint;loadMap(0);
  FreeMemory(aasworld.reachability);aasworld.reachabilitysize=count+1;
  aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(count+1);
  aasworld.areasettings[0].numreachableareas=1; // reserved retail area may have a span
  aasworld.areasettings[1].numreachableareas=count;aasworld.areasettings[2].firstreachablearea=count;
  for(int i=0;i<count;++i)aasworld.reachability[i].areanum=2;
  aasworld.reachability[count].areanum=1;
  assert(AAS_StartFrame(0)==0);AAS_SetCurrentWorld(0);
  assert(aasworld.reversedreachability[2].numlinks==count&&aasworld.reversedreachability[2].first->linknum==count-1);
  assert(aasworld.areatraveltimes[1][count-1][0]==1&&aasworld.areatraveltimes[2][0][count-1]==1);shutdown();
 }
 bool completed=false;
 for(int fail=0;fail<30;++fail){profile=&profiles[2];initialize();botimport.Print=checkedPrint;loadMap(0);
  failAllocation=fail;bool failed=false;
  try{assert(AAS_StartFrame(1)==0);AAS_SetCurrentWorld(0);checkTables();}catch(Drop&){failed=true;}
  failAllocation=-1;shutdown();if(!failed){assert(fail>=15);completed=true;break;}
 }
 assert(completed);
 for(int bad=0;bad<6;++bad){profile=&profiles[2];initialize();botimport.Print=checkedPrint;loadMap(0);
  if(bad==0)files.erase("maps/probe.rcd");
  if(bad==1)files["maps/probe.rcd"][16]^=1;
  if(bad==2)aasworld.areasettings[1].numreachableareas=257;
  if(bad==3)aasworld.areasettings[0].firstreachablearea=aasworld.reachabilitysize+1;
  if(bad==4)aasworld.reachability[0].areanum=3;
  if(bad==5){volatile uint32_t input=0x7fc00000;uint32_t bits=input;std::memcpy(&aasworld.rtcwGroundSteepness[1],&bits,4);}
  const int before=exits;try{AAS_StartFrame(1);assert(false);}catch(Drop&){}
  assert(exits==before+1&&!aasWorlds[0].initialized);shutdown();
 }
}
'''
        self.compile_and_run(source, self.extra(), ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
