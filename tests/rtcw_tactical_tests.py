"""Execute tactical searches with real geometry, route caches and ABI adapters."""
from pathlib import Path
import unittest
import rtcw_visibility_tests as visibility
import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing

ROOT = Path(__file__).resolve().parents[1]


def tactical_source():
    source = visibility.visibility_source()
    source = source.replace('static void initialize(){',
                            'static void initialize(){\nLibVarSet("maxentities",std::to_string(profile->bots.maxEntities).c_str());')
    source = source.replace('bool AAS_RTCWHidePosition(const float*,int,int,const float*,int,int,float*){assert(false);return false;}', '')
    source = source.replace('int AAS_RTCWAttackPosition(int,int,int,float,int,float*){assert(false);return 0;}', '')
    return source + '\n#include "code/botlib/rtcw_tactical.hpp"\n'


SETUP = r'''
namespace tactical=fntech3::rtcw::tactical;
static int visibilityCalls,attackCalls,visibleResult,attackResult=1,sideEffect;
static bool available=true;
static float sequence=1;
static void tick(){aasworld.time=sequence++;aasworld.frameroutingupdates=0;}
static void effect(){
 if(sideEffect==1){
  // A game callback may enter route-cache code or select another hull.
  assert(AAS_AreaTravelTimeToGoalArea(1,aasworld.areas[1].center,2,-1)>0);
  assert(AAS_SetCurrentWorld(1));
 }else if(sideEffect==2){
  vec3_t output{71,72,73};auto before=aas_currentWorld;
  assert(!AAS_RTCWHidePosition(aasworld.areas[1].center,1,1,aasworld.areas[3].center,3,3,output));
  assert(!AAS_RTCWAttackPosition(1,2,3,100,-1,output));
  assert(output[0]==71&&aas_currentWorld==before);
 }else if(sideEffect==3)AAS_FreeRoutingCaches();
 else if(sideEffect==4)throw Drop{};
}
static int see(const float*,int enemy,const float*,int entity,int update){
 assert(enemy==3&&entity==1&&update==0);++visibilityCalls;effect();return visibleResult;
}
static int attack(int entity,int enemy,const float*,int duck,int hit){
 assert(entity==1&&enemy==3&&!duck&&!hit);++attackCalls;effect();return attackResult;
}
static void callbacks(){AAS_SetRTCWCastCallbacks({[](){return available;},see,attack});}
static void bind(int entity,int area){
 auto &ent=AAS_EntityWorld().entities[entity];ent.i.valid=qtrue;ent.i.solid=SOLID_BBOX;
 VectorCopy(aasworld.areas[area].center,ent.i.origin);
 auto *link=AAS_AllocAASLink();assert(link);*link={};link->areanum=area;link->entnum=entity;
 link->next_ent=aasworld.arealinkedentities[area];
 if(link->next_ent)link->next_ent->prev_ent=link;
 aasworld.arealinkedentities[area]=link;ent.areas=link;ent.areaWorld=aas_currentWorld;
}
static void scene(int count=3){
 graph(0,count,1);slabs();visibility();
 aasworld.rtcwWaypoints=botlib::ClearedAlloc<vec3_t>(aasworld.numareas);
 for(int i=0;i<aasworld.numareas;++i)VectorCopy(aasworld.areas[i].center,aasworld.rtcwWaypoints[i]);
 AAS_InitAASLinkHeap();AAS_InitAASLinkedEntities();bind(1,1);bind(2,1);bind(3,3);
 callbacks();visibilityCalls=attackCalls=sideEffect=0;visibleResult=0;attackResult=1;available=true;tick();
}
static bool hide(vec3_t output,int entity=1){return AAS_RTCWHidePosition(aasworld.areas[1].center,entity,1,
 aasworld.areas[3].center,3,3,output);}
static int strike(vec3_t output,float distance=100){return AAS_RTCWAttackPosition(1,2,3,distance,-1,output);}
'''


class RTCWTacticalTests(unittest.TestCase):
    compile_and_run = routing.RTCWRoutingTests.compile_and_run

    def run_source(self, checks, setup=SETUP):
        self.compile_and_run(tactical_source() + routes.GRAPH + visibility.GEOMETRY + setup + checks,
                             routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_search_policies_callbacks_nested_routing_and_world_ownership(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;initialize();
  if(!Com_GameIsRTCW()){
   assert(!AAS_RTCWHidePosition(nullptr,-1,-1,nullptr,-1,-1,nullptr));
   assert(!AAS_RTCWAttackPosition(-1,-1,-1,0,-1,nullptr));shutdown();continue;
  }
  scene();vec3_t output{71,72,73};
  assert(hide(output)&&!memcmp(output,aasworld.rtcwWaypoints[2],12));
  assert(visibilityCalls==2&&!aasworld.rtcwTacticalToken);
  const int calls=visibilityCalls;assert(hide(output)==bool(Com_GameIsRTCWSP()));
  assert(visibilityCalls==calls+(Com_GameIsRTCWSP()?2:0));
  tick();visibleResult=1;output[0]=71;assert(!hide(output)&&output[0]==71);visibleResult=0;
  tick();aasworld.areasettings[2].areaflags|=AREA_LADDER;
  assert(hide(output)==!Com_GameIsRTCWSP());aasworld.areasettings[2].areaflags&=~AREA_LADDER;
  tick();const int link=aasworld.areasettings[1].firstreachablearea;
  assert(aasworld.reachability[link].areanum==2);aasworld.reachability[link].traveltype=TRAVEL_JUMPPAD;
  assert(hide(output)==bool(Com_GameIsRTCWSP()));aasworld.reachability[link].traveltype=TRAVEL_WALK;
  tick();aasworld.areasettings[2].contents=4;aasworld.areacontentstravelflags[2]=AAS_GetAreaContentsTravelFlags(2);
  assert(hide(output));aasworld.areasettings[2].contents=0;
  aasworld.areacontentstravelflags[2]=AAS_GetAreaContentsTravelFlags(2);
  tick();if(Com_GameIsRTCWSP()){tactical::hideTime=aasworld.time;tactical::hideWork=3000;
   assert(!hide(output));assert(hide(output,-2));}
  else assert(!hide(output,-2));
  tick();visible(3,2,true);assert(strike(output)==2&&attackCalls==1);
  assert(!strike(output)&&attackCalls==1);tick();output[0]=71;assert(!strike(output,0)&&output[0]==71);
  tick();attackResult=0;assert(!strike(output));attackResult=1;
  tick();aasworld.areasettings[2].areaflags|=AREA_DISABLED;assert(!strike(output));
  aasworld.areasettings[2].areaflags&=~AREA_DISABLED;
  tick();aasworld.areasettings[2].areaflags|=AREA_LADDER;assert(strike(output)==(Com_GameIsRTCWSP()?0:2));
  aasworld.areasettings[2].areaflags&=~AREA_LADDER;
  tick();auto *owner=aas_currentWorld;sideEffect=1;assert(strike(output)==2&&aas_currentWorld==owner);
  tick();sideEffect=2;assert(strike(output)==2&&!aasworld.rtcwTacticalToken);sideEffect=0;
  tick();auto *linked=AAS_EntityWorld().entities[1].areaWorld;AAS_SetCurrentWorld(1);
  AAS_EntityWorld().entities[1].areaWorld=aas_currentWorld;aas_currentWorld=owner;
  assert(strike(output)==(Com_GameIsRTCWSP()?2:0));AAS_EntityWorld().entities[1].areaWorld=linked;
  tick();available=false;int before=visibilityCalls+attackCalls;assert(!hide(output)&&!strike(output));
  assert(visibilityCalls+attackCalls==before);available=true;
  tick();sideEffect=3;output[0]=71;assert(!strike(output)&&output[0]==71);
  assert(!aasworld.rtcwTacticalNodes&&!aasworld.rtcwTacticalToken);sideEffect=0;shutdown();
 }
}
''')

    def test_partial_setup_allocation_failure_bounds_and_callback_errors(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;initialize();
  vec3_t output{71,72,73};assert(!AAS_RTCWAttackPosition(1,2,3,100,-1,output));scene();
  visible(3,2,true);failAllocation=0;try{strike(output);assert(false);}catch(Drop&){}
  failAllocation=-1;assert(!aasworld.rtcwTacticalNodes&&!aasworld.rtcwTacticalToken);
  tick();assert(strike(output)==2);auto *nodes=aasworld.rtcwTacticalNodes;
  for(int bad:{-1,1024,2047,p.bots.maxEntities,INT_MIN,INT_MAX}){
   tick();assert(!AAS_RTCWAttackPosition(bad,2,3,100,-1,output));
   assert(!AAS_RTCWAttackPosition(1,bad,3,100,-1,output));
   assert(!AAS_RTCWAttackPosition(1,2,bad,100,-1,output));
  }
  tick();assert(!hide(output,INT_MIN));assert(!strike(output,-1));
  for(uint32_t bits:{0x7fc00000u,0x7f800000u,0xff800000u}){volatile uint32_t input=bits;bits=input;float bad;memcpy(&bad,&bits,4);
   const int found=strike(output,bad);if(found)fprintf(stderr,"invalid range %08x bits=%08x finite=%d result=%d\n",bits,fntech3::rtcw::routing::Bits(bad),int(fntech3::rtcw::routing::Finite(bad)),found);
   assert(!found);}
  --aasworld.rtcwTacticalCapacity;assert(!strike(output));++aasworld.rtcwTacticalCapacity;
  for(int bad:{-1,2}){tick();attackResult=bad;output[0]=71;assert(!strike(output)&&output[0]==71);}
  attackResult=1;tick();sideEffect=4;try{strike(output);assert(false);}catch(Drop&){}
  assert(aasworld.rtcwTacticalToken&&aasworld.rtcwTacticalNodes==nodes);sideEffect=0;
  shutdown();initialize();scene();visible(3,2,true);assert(strike(output)==2);shutdown();
 }
}
''')

    def test_occupancy_both_worlds_and_nonlocal_shutdown(self):
        setup = '#include <csetjmp>\nstatic std::jmp_buf errorExit;\n' + SETUP.replace(
            'else if(sideEffect==4)throw Drop{};', 'else if(sideEffect==4)std::longjmp(errorExit,1);')
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;initialize();
  for(int world=0;world<2;++world){AAS_SetCurrentWorld(world);scene(4);bind(4,2);
   vec3_t output{71,72,73};assert(hide(output));
   assert(!memcmp(output,aasworld.rtcwWaypoints[Com_GameIsRTCWSP()?2:4],12));
   AAS_EntityWorld().entities[4].i.valid=qfalse;tick();assert(hide(output));
   assert(!memcmp(output,aasworld.rtcwWaypoints[2],12));
   visible(3,2,true);tick();assert(strike(output)==2);
   if(!Com_GameIsRTCWSP()){
    auto *links=aasworld.arealinkedentities[2];links->next_ent=links;
    assert(tactical::Occupied(2,1));links->next_ent=nullptr;
    const int capacity=aasworld.linkheapsize;aasworld.linkheapsize=INT_MIN;
    assert(tactical::Occupied(2,1));assert(!tactical::EntityArea(1));aasworld.linkheapsize=capacity;
   }
  }
  assert(aasWorlds[0].rtcwTacticalNodes&&aasWorlds[1].rtcwTacticalNodes
   &&aasWorlds[0].rtcwTacticalNodes!=aasWorlds[1].rtcwTacticalNodes);
  sideEffect=4;tick();if(!setjmp(errorExit)){vec3_t output;strike(output);assert(false);}
  assert(aasworld.rtcwTacticalToken);shutdown();
  assert(!aasWorlds[0].rtcwTacticalNodes&&!aasWorlds[1].rtcwTacticalNodes);sideEffect=0;
  initialize();scene();vec3_t output;assert(hide(output));shutdown();
 }
}
''', setup)

    def test_zero_cost_cycles_exhaust_budgets_and_repeat_frame_gates(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;initialize();scene();
  // A valid ushort wrap makes these cyclic links cost zero. Visited=0 is
  // deliberately not a visited marker in RTCW, so only the work budget ends it.
  for(int i=0;i<aasworld.reachabilitysize;++i){auto &reach=aasworld.reachability[i];
   reach.traveltime=65535;VectorClear(reach.start);VectorClear(reach.end);}
  for(int e=1;e<=3;++e){auto &ent=AAS_EntityWorld().entities[e];ent.i.solid=SOLID_NOT;VectorClear(ent.i.origin);}
  vec3_t point{},output{71,72,73};visibleResult=1;tick();
  assert(!AAS_RTCWHidePosition(point,1,1,point,3,3,output)&&output[0]==71);
  assert(tactical::hideWork>(Com_GameIsRTCWSP()?3000:4000));
  const int before=visibilityCalls;
  assert(!AAS_RTCWHidePosition(point,1,1,point,3,3,output)&&visibilityCalls==before);
  assert(!aasworld.rtcwTacticalToken);for(int a=1;a<aasworld.numareas;++a)assert(!aasworld.rtcwTacticalNodes[a].queued);
  if(Com_GameIsRTCWSP()){
   assert(!AAS_RTCWHidePosition(point,-2,1,point,3,3,output));assert(visibilityCalls>before);
  }
  shutdown();initialize();scene(40);
  for(int a=1;a<aasworld.numareas;++a)visible(3,a,true);
  attackResult=0;tick();assert(!strike(output,10000));
  // Each attack callback consumes ten work units, plus edge visits.
  assert(attackCalls>0&&attackCalls<=19&&!aasworld.rtcwTacticalToken);
  int pending=0;for(int a=1;a<aasworld.numareas;++a)pending+=aasworld.rtcwTacticalNodes[a].queued;
  assert(pending>0);const int attacks=attackCalls;assert(!strike(output)&&attackCalls==attacks);
  tick();attackResult=1;assert(strike(output,10000)>0);shutdown();
 }
}
''')

    def test_native_qvm_spans_aliases_scalars_and_failure_publication(self):
        self.run_source(r'''
using namespace fntech3::rtcw::module;
int main(){
 botlib_export_t exports{};botlib_export=&exports;
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW()){
   intptr_t result=73;for(auto call:{Call::BOTLIB_AAS_RT_GETHIDEPOS,Call::BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE})
    assert(!SV_RTCWAASQueryCall(nullptr,call,nullptr,result)&&result==73);continue;}
  assert(Ordinal(p.id,Call::BOTLIB_AAS_RT_GETHIDEPOS)==321);
  assert(Ordinal(p.id,Call::BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE)==322);
  for(bool native:{false,true}){initialize();scene();
   std::array<byte,128> memory;vm_t vm{};vm.dataBase=memory.data();vm.dataMask=127;vm.exactDataLength=128;
   if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](int at){return native?reinterpret_cast<intptr_t>(memory.data()+at):intptr_t(at);};
   auto reset=[&](){memory.fill(0xa5);memcpy(memory.data()+1,aasworld.areas[1].center,12);
    memcpy(memory.data()+33,aasworld.areas[3].center,12);tick();};
   auto invoke=[&](Call call,intptr_t *args){intptr_t result=73;
    assert(SV_RTCWBotLibCall(&vm,call,args,result));return result;};
   intptr_t hideArgs[]={321,address(1),1,1,address(33),3,3,address(116)};
   intptr_t attackArgs[]={322,1,2,3,0x42c80000,-1,address(116)};
   auto reject=[&](Call call,intptr_t *args){auto before=memory;
    try{invoke(call,args);fprintf(stderr,"expected drop: profile=%s native=%d call=%d args=%lld,%lld,%lld,%lld,%lld,%lld\n",p.key,int(native),int(args[0]),
     (long long)args[1],(long long)args[2],(long long)args[3],(long long)args[4],(long long)args[5],(long long)args[6]);assert(false);}catch(Drop&){}assert(memory==before);};
   for(int out:{1,5,33,37,116}){reset();hideArgs[7]=address(out);
    assert(invoke(Call::BOTLIB_AAS_RT_GETHIDEPOS,hideArgs)==1);
    assert(!memcmp(memory.data()+out,aasworld.rtcwWaypoints[2],12));}
   reset();hideArgs[7]=address(116);visibleResult=1;auto before=memory;
   assert(!invoke(Call::BOTLIB_AAS_RT_GETHIDEPOS,hideArgs)&&memory==before);visibleResult=0;
   visible(3,2,true);reset();assert(invoke(Call::BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE,attackArgs)==2);
   assert(!memcmp(memory.data()+116,aasworld.rtcwWaypoints[2],12));
   for(int index:{1,4,7}){reset();auto saved=hideArgs[index];hideArgs[index]=0;
    reject(Call::BOTLIB_AAS_RT_GETHIDEPOS,hideArgs);hideArgs[index]=saved;}
   if(!native){for(int index:{1,4,7}){auto saved=hideArgs[index];hideArgs[index]=117;
     reject(Call::BOTLIB_AAS_RT_GETHIDEPOS,hideArgs);hideArgs[index]=saved;}
    attackArgs[6]=117;reject(Call::BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE,attackArgs);attackArgs[6]=116;}
   for(int index:{1,2,3}){auto saved=attackArgs[index];for(int value:{-1,1024,2047,p.bots.maxEntities,INT_MIN,INT_MAX}){
    attackArgs[index]=value;reject(Call::BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE,attackArgs);}attackArgs[index]=saved;}
   for(uint32_t bits:{0xbf800000u,0x7fc00000u,0x7f800000u,0xff800000u}){
    attackArgs[4]=static_cast<int32_t>(bits);reject(Call::BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE,attackArgs);}
   attackArgs[4]=0x42c80000;
   for(uint32_t bits:{0x7fc00000u,0x48000001u}){reset();volatile uint32_t input=bits;bits=input;memcpy(memory.data()+1,&bits,4);
    reject(Call::BOTLIB_AAS_RT_GETHIDEPOS,hideArgs);}
   if(sizeof(intptr_t)>4){reset();for(int index:{2,3,5,6}){auto saved=hideArgs[index];
    hideArgs[index]=static_cast<intptr_t>(UINT64_C(0x80000000));reject(Call::BOTLIB_AAS_RT_GETHIDEPOS,hideArgs);hideArgs[index]=saved;}}
   shutdown();
  }
 }
}
''')


if __name__ == '__main__':
    unittest.main()
