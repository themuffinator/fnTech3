"""Exercise RTCW movement-link selection with controlled and real route caches."""
from pathlib import Path
import unittest
import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing
from rtcw_server_world_tests import function

ROOT=Path(__file__).resolve().parents[1]


def goal_route_source(controlled=False):
    source=routes.route_source()
    if controlled:
        source=source.replace('AAS_AreaRouteToGoalArea(', 'Real_AAS_AreaRouteToGoalArea(')
        source+=r'''
static int costs[32],nextLinks[32],calls;
static std::vector<int> seenFlags,seenAreas;
static int AAS_AreaRouteToGoalArea(int area,vec3_t,int goal,int flags,int *cost,int *reach){
 assert(area>0&&area<32&&goal>0&&goal<32);++calls;seenFlags.push_back(flags);seenAreas.push_back(area);
 *cost=costs[area];*reach=nextLinks[area];return *cost!=0;
}
'''
    reach=(ROOT/'code/botlib/be_aas_reach.cpp').read_text()
    source+='\n'+function(reach,'int AAS_AreaReachability(')
    route=(ROOT/'code/botlib/be_aas_route.cpp').read_text()
    for signature in ['bool AAS_RTCWMovementRoutingReady(', 'int AAS_RTCWMovementReachability(']:
        source+='\n'+function(route,signature)
    return source


class RTCWGoalRouteTests(unittest.TestCase):
    compile_and_run=routing.RTCWRoutingTests.compile_and_run

    def run_source(self,checks,controlled=False):
        self.compile_and_run(goal_route_source(controlled)+routes.GRAPH+checks,
            routing.RTCWRoutingTests.extra(),['-O2','-ffast-math'])

    def test_sp_mp_route_costs_loop_rejection_entry_flags_and_terminal_areas(self):
        self.run_source(r'''
static constexpr int flags=TFL_WALK|TFL_AIR;
int main(){
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;initialize();graph(0,4,1);
  if(!Com_GameIsRTCW()){
   assert(!AAS_RTCWMovementRoutingReady());vec3_t origin{};
   assert(!AAS_RTCWMovementReachability(origin,2,4,0,4,flags,flags));shutdown();continue;
  }
  const bool sp=Com_GameIsRTCWSP();auto &s=aasworld.areasettings[2];
  assert(s.firstreachablearea==3&&s.numreachableareas==3);
  auto &a=aasworld.reachability[3],&b=aasworld.reachability[4],&c=aasworld.reachability[5];
  assert(a.areanum==1&&b.areanum==3&&c.areanum==4);
  costs[1]=costs[3]=100;costs[4]=0;nextLinks[1]=nextLinks[3]=1;
  assert(aasworld.reachability[1].areanum==3); // No loop to current area 2.
  vec3_t origin{16,2,0};VectorCopy(origin,a.start);a.start[0]+=100;
  VectorCopy(origin,b.start);a.traveltime=b.traveltime=10;
  auto query=[&](int goal=4,int previous=0,int lastGoal=4,int routeFlags=flags,int moveFlags=flags){
   calls=0;seenFlags.clear();seenAreas.clear();aasworld.frameroutingupdates=0;
   return AAS_RTCWMovementReachability(origin,2,lastGoal,previous,goal,routeFlags,moveFlags);
  };
  assert(query()==(sp?3:4)); // SP omits travel to link start; first equal-cost link wins.
  VectorCopy(origin,a.start);assert(query()==3);
  assert(query(4,1)==4&&query(4,1,0)==3);
  nextLinks[1]=0;assert(aasworld.reachability[0].areanum==2);
  assert(query()==(sp?4:3));nextLinks[1]=1;
  aasworld.areasettings[1].areaflags|=AREA_DISABLED;
  assert(query()==(sp?4:3));aasworld.areasettings[1].areaflags&=~AREA_DISABLED;
  int count=aasworld.areasettings[1].numreachableareas;aasworld.areasettings[1].numreachableareas=0;
  assert(query()==(sp?4:3));aasworld.areasettings[1].numreachableareas=count;
  // Routing flags do not authorize an otherwise forbidden immediate movement.
  a.traveltype=TRAVEL_JUMP;assert(query(4,0,4,flags|TFL_JUMP,flags)==4);
  assert(query(4,0,4,flags,flags|TFL_JUMP)==3);a.traveltype=TRAVEL_WALK;
  a.traveltype=TRAVEL_WALK|TRAVELFLAG_NOTTEAM1;
  assert(query()==4);a.traveltype=TRAVEL_WALK;
  // Endpoint restrictions are added to both masks, including the large variant.
  for(int content:{AREACONTENTS_DONOTENTER,1024,AREACONTENTS_DONOTENTER|1024}){
   const int allowed=(content&AREACONTENTS_DONOTENTER?TFL_DONOTENTER:0)
     |(content&1024?fntech3::rtcw::routing::DoNotEnterLarge:0);
   aasworld.areasettings[1].contents=content;AAS_InitAreaContentsTravelFlags();
   assert(query()==4);
   for(int endpoint:{2,4}){aasworld.areasettings[endpoint].contents=content;
    AAS_InitAreaContentsTravelFlags();assert(query()==3);
    for(int observed:seenFlags)assert(observed==(flags|allowed));
    aasworld.areasettings[endpoint].contents=0;}
   aasworld.areasettings[1].contents=0;AAS_InitAreaContentsTravelFlags();
  }
  // Empty spans must not expose the next area's links; zero remains a sentinel.
  int first=s.firstreachablearea;count=s.numreachableareas;s.numreachableareas=0;
  assert(!query()&&!calls);s.numreachableareas=count;s.firstreachablearea=0;
  assert(!query()&&!calls);s.firstreachablearea=first;
  for(int invalid:{-1,INT_MAX}){s.numreachableareas=invalid;assert(!query()&&!calls);}s.numreachableareas=count;
  assert(query()==3);shutdown();
 }
}
''',True)

    def test_real_cached_routes_world_ownership_invalid_inputs_and_partial_shutdown(self):
        self.run_source(r'''
int main(){
 const int flags=TFL_WALK|TFL_AIR;
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;initialize();
  vec3_t origin{24,0,0};assert(!AAS_RTCWMovementRoutingReady());
  assert(!AAS_RTCWMovementReachability(origin,3,2,0,2,flags,flags));
  for(int world:{0,1}){AAS_SetCurrentWorld(world);graph(0,3,1);
   assert(AAS_RTCWMovementRoutingReady());
   aasworld.frameroutingupdates=0;assert(AAS_RTCWMovementReachability(origin,3,2,0,2,flags,flags)==5);
   assert(AAS_RTCWMovementReachability(origin,3,2,2,2,flags,flags)==4);cacheLinks();
   for(int invalid:{INT_MIN,-1,0,aasworld.numareas,INT_MAX}){
    assert(!AAS_RTCWMovementReachability(origin,invalid,2,0,2,flags,flags));
    assert(!AAS_RTCWMovementReachability(origin,3,2,0,invalid,flags,flags));
   }
   assert(!AAS_RTCWMovementReachability(nullptr,3,2,0,2,flags,flags));
   for(unsigned bits:{0x48000001u,0x7f800000u,0x7fc00001u}){
    vec3_t invalid{24,0,0};memcpy(invalid+1,&bits,4);
    assert(!AAS_RTCWMovementReachability(invalid,3,2,0,2,flags,flags));
   }
   auto saved=aasworld;
   aasworld.initialized=0;assert(!AAS_RTCWMovementRoutingReady());aasworld=saved;
   aasworld.numareasettings--;assert(!AAS_RTCWMovementRoutingReady());aasworld=saved;
   aasworld.clusterareacache=nullptr;assert(!AAS_RTCWMovementRoutingReady());aasworld=saved;
   aasworld.reversedreachability=nullptr;assert(!AAS_RTCWMovementRoutingReady());aasworld=saved;
   aasworld.areasettings[2].areaflags|=AREA_DISABLED;
  }
  AAS_SetCurrentWorld(0);assert(aasworld.areasettings[2].areaflags&AREA_DISABLED);
  AAS_SetCurrentWorld(1);assert(aasworld.areasettings[2].areaflags&AREA_DISABLED);
  shutdown();assert(!AAS_RTCWMovementRoutingReady());
  assert(!AAS_RTCWMovementReachability(origin,3,2,0,2,flags,flags));
 }
}
''')


if __name__=='__main__':unittest.main()
