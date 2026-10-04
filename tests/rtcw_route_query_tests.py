"""Run the actual route caches, queries and RTCW native/QVM import boundary."""
from pathlib import Path
import unittest
import rtcw_routing_tests
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def route_source():
    source = rtcw_routing_tests.routing_source()
    route = (ROOT / 'code/botlib/be_aas_route.cpp').read_text()
    reach = (ROOT / 'code/botlib/be_aas_reach.cpp').read_text()
    source += function(reach, 'int AAS_AreaDoNotEnter(') + '\n'
    # The earlier occurrence is a disabled, commented-out implementation.
    source += function(route[route.rindex('static int AAS_FreeOldestCache('):],
                       'static int AAS_FreeOldestCache(') + '\n'
    for signature in ('static ID_INLINE float AAS_RoutingTime(',
                      'static void AAS_CreateReversedReachability(',
                      'static void AAS_CalculateAreaTravelTimes(',
                      'static void AAS_UpdateAreaRoutingCache(',
                      'static aas_routingcache_t *AAS_GetAreaRoutingCache(',
                      'static void AAS_UpdatePortalRoutingCache(',
                      'static aas_routingcache_t *AAS_GetPortalRoutingCache(',
                      'static int AAS_AreaRouteToGoalArea(',
                      'int AAS_AreaTravelTimeToGoalArea('):
        source += function(route, signature) + '\n'
    return source


GRAPH = r'''
static void routingTables(){
 AAS_FreeRoutingCaches();AAS_InitTravelFlagFromType();AAS_InitAreaContentsTravelFlags();
 aasworld.areaupdate=botlib::ClearedAlloc<aas_routingupdate_t>(aasworld.numareas);
 aasworld.portalupdate=botlib::ClearedAlloc<aas_routingupdate_t>(aasworld.numportals+1);
 if(Com_GameIsRTCW()){
  size_t links=0;assert(fntech3::rtcw::navigation::RoutingGeometry(links));
  fntech3::rtcw::navigation::ReversedRouting(links);
  assert(fntech3::rtcw::navigation::RoutingCosts(links));
 }else{AAS_CreateReversedReachability();AAS_CalculateAreaTravelTimes();}
 AAS_InitPortalMaxTravelTimes();AAS_InitClusterAreaCache();AAS_InitPortalCache();
 AAS_InitRoutingLimits();aasworld.max_routingcachesize=4096*1024;aasworld.frameroutingupdates=0;
 aasworld.loaded=aasworld.initialized=qtrue;
}
// A ring of clusters. Each has ordinary areas and two shared portal areas.
// Different seeds vary link costs/types, slopes, liquids and entry restrictions.
static void graph(unsigned seed=0,int ordinary=3,int clusters=3,bool fan=false){
 assert(clusters==1||clusters>=3);const int portals=clusters==1?0:clusters;
 const int regular=ordinary*clusters,total=regular+portals;
 aasworld.numareas=aasworld.numareasettings=total+1;aasworld.numclusters=clusters+1;
 aasworld.numportals=portals+1;aasworld.portalindexsize=portals*2;
 aasworld.areas=botlib::ClearedAlloc<aas_area_t>(total+1);
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(total+1);
 aasworld.rtcwGroundSteepness=botlib::ClearedAlloc<float>(total+1);
 aasworld.clusters=botlib::ClearedAlloc<aas_cluster_t>(clusters+1);
 aasworld.portals=botlib::ClearedAlloc<aas_portal_t>(portals+1);
 aasworld.portalindex=botlib::ClearedAlloc<aas_portalindex_t>(portals*2+1);
 std::vector<std::vector<int>> members(clusters+1),destinations(total+1);
 for(int c=1;c<=clusters;++c){auto &cl=aasworld.clusters[c];
  cl.numareas=cl.numreachabilityareas=ordinary+(portals?2:0);cl.numportals=portals?2:0;
  cl.firstportal=portals?(c-1)*2:0;
  for(int i=0;i<ordinary;++i){int a=(c-1)*ordinary+i+1;members[c].push_back(a);
   aasworld.areasettings[a].cluster=c;aasworld.areasettings[a].clusterareanum=i;}
  if(portals){int previous=c==1?clusters:c-1;members[c].push_back(regular+c);
   members[c].push_back(regular+previous);aasworld.portalindex[cl.firstportal]=c;
   aasworld.portalindex[cl.firstportal+1]=previous;}
 }
 for(int p=1;p<=portals;++p){auto &portal=aasworld.portals[p];
  portal.areanum=regular+p;portal.frontcluster=p;portal.backcluster=p%clusters+1;
  portal.clusterareanum[0]=ordinary;portal.clusterareanum[1]=ordinary+1;
  aasworld.areasettings[portal.areanum].cluster=-p;}
 for(int c=1;c<=clusters;++c)for(int a:members[c])for(int b:members[c])
  if(a!=b&&(!fan||a==1&&b==2||a!=1&&b==1))destinations[a].push_back(b);
 auto nextValue=[&](){seed=seed*1664525u+1013904223u;return seed;};const bool varied=seed!=0;
 int links=0;
 for(int a=1;a<=total;++a){auto &s=aasworld.areasettings[a];
  auto &d=destinations[a];std::sort(d.begin(),d.end());d.erase(std::unique(d.begin(),d.end()),d.end());
  s.firstreachablearea=links;s.numreachableareas=(int)d.size();links+=s.numreachableareas;
  s.presencetype=PRESENCE_NORMAL;s.areaflags=AREA_GROUNDED;aasworld.areas[a].areanum=a;
  aasworld.areas[a].center[0]=float(a*8);aasworld.areas[a].center[1]=float(a%3);
  if(varied){unsigned value=nextValue();s.presencetype=(value&1)?PRESENCE_NORMAL:PRESENCE_CROUCH;
   s.contents=(value%11==0)?1024:(value%7==0)?256:(value%5==0)?1:0;
   if(s.contents&1)s.areaflags|=AREA_LIQUID;
   aasworld.rtcwGroundSteepness[a]=float(value%17)/16.0f;}
 }
 aasworld.reachabilitysize=links;aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(links);
 for(int a=1;a<=total;++a){int offset=aasworld.areasettings[a].firstreachablearea;
  for(int b:destinations[a]){auto &r=aasworld.reachability[offset++];r.areanum=b;r.traveltype=TRAVEL_WALK;
   r.traveltime=10;VectorCopy(aasworld.areas[a].center,r.start);VectorCopy(aasworld.areas[b].center,r.end);
   if(varied){static const int types[]={2,3,4,7,18,19};r.traveltype=types[nextValue()%6];
    r.traveltime=5+nextValue()%80;r.start[1]+=float(nextValue()%9);r.end[2]+=float(nextValue()%9);}
  }
 }
 routingTables();
}
static void cacheLinks(){
 int bytes=0,count=0;aas_routingcache_t *previous=nullptr;
 for(auto *cache=aasworld.oldestcache;cache;cache=cache->time_next){
  assert(cache->time_prev==previous);bytes+=cache->size;previous=cache;++count;
 }
 assert(previous==aasworld.newestcache&&bytes==aasworld.routingcachesize);
 int linked=0;
 auto chain=[&](aas_routingcache_t *head){aas_routingcache_t *prev=nullptr;
  for(auto *cache=head;cache;cache=cache->next){assert(cache->prev==prev);prev=cache;++linked;}};
 for(int c=0;c<aasworld.numclusters;++c)for(int a=0;a<aasworld.clusters[c].numareas;++a)
  chain(aasworld.clusterareacache[c][a]);
 for(int a=0;a<aasworld.numareas;++a)chain(aasworld.portalcache[a]);
 assert(linked==count);
}
'''


class RTCWRouteQueryTests(unittest.TestCase):
    compile_and_run = rtcw_routing_tests.RTCWRoutingTests.compile_and_run

    def test_profile_costs_budget_first_reachability_eviction_and_cleanup(self):
        source = route_source() + GRAPH + r'''
int main(){
 const int flags=TFL_WALK|TFL_AIR;
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;initialize();graph(0,2,1);
  // RTCW, ET and Elite Force Holomatch start from the goal's own row, which
  // costs the time inside it; Quake III 1.32 starts from zeros.
  assert(AAS_AreaTravelTimeToGoalArea(1,nullptr,2,flags)==(Com_GameIsRTCW()||Com_GameIsWolfET()||Com_GameIsSTVEFMP()?12:11));
  assert(aasworld.frameroutingupdates==(Com_GameIsRTCW()||Com_GameIsWolfET()?2:1));cacheLinks();
  aasworld.frameroutingupdates=101;auto *hit=AAS_GetAreaRoutingCache(1,2,flags);
  assert(hit&&aasworld.frameroutingupdates==101);
  auto *miss=AAS_GetAreaRoutingCache(1,1,flags);assert((miss==nullptr)==bool(Com_GameIsRTCW()));
  if(Com_GameIsRTCW()){
   assert(AAS_AreaTravelTimeToGoalArea(2,nullptr,1,flags)==0);
   aasworld.frameroutingupdates=100;assert(AAS_GetAreaRoutingCache(1,1,flags));
   assert(aasworld.frameroutingupdates==102);
   aasworld.frameroutingupdates=INT_MAX;assert(AAS_GetAreaRoutingCache(1,1,flags|TFL_JUMP,true));
   assert(aasworld.frameroutingupdates==INT_MAX);
  }
  cacheLinks();shutdown();
  initialize();graph();
  for(int a=1;a<aasworld.numareas;++a)for(int g=1;g<aasworld.numareas;++g){
   aasworld.frameroutingupdates=0;int time=71,reach=73;
   assert(AAS_AreaRouteToGoalArea(a,aasworld.areas[a].center,g,flags,&time,&reach)&&time>0);
   if(a!=g){assert(reach>=aasworld.areasettings[a].firstreachablearea);
    assert(reach<aasworld.areasettings[a].firstreachablearea+aasworld.areasettings[a].numreachableareas);}
   if(Com_GameIsRTCW()){
    int noOrigin=0,first=-99;assert(AAS_AreaRouteToGoalArea(a,nullptr,g,flags,&noOrigin,&first));
    assert(first==reach&&noOrigin>0&&noOrigin<=time);
   }
   cacheLinks();
  }
  // Blocking a portal invalidates routes, not the selected other world.
  // Elite Force Holomatch's engine never tests the flag (stvoyHM.exe
  // 0x004992d0), so its route stays as it was.
  aasworld.frameroutingupdates=0;int open=0,openFirst=0;
  assert(AAS_AreaRouteToGoalArea(1,aasworld.areas[1].center,4,flags,&open,&openFirst));
  AAS_EnableRoutingArea(10,0);assert(aasworld.areasettings[10].areaflags&AREA_DISABLED);
  aasworld.frameroutingupdates=0;int time=0,first=0;
  assert(AAS_AreaRouteToGoalArea(1,aasworld.areas[1].center,4,flags,&time,&first));
  if(Com_GameIsSTVEFMP())assert(time==open&&first==openFirst&&aasworld.reachability[first].areanum==10);
  else assert(aasworld.reachability[first].areanum!=10);
  cacheLinks();
  // RTCW and ET expire one ordinary cache plus one portal cache per pass.
  routingTables();auto *ordinary=AAS_GetAreaRoutingCache(1,1,flags,true);
  auto *portalArea=AAS_GetAreaRoutingCache(1,10,flags,true);
  auto *portal=AAS_GetPortalRoutingCache(2,4,flags);int before=aasworld.routingcachesize;
  const int expected=ordinary->size+(Com_GameIsRTCW()||Com_GameIsWolfET()?portal->size:0);
  assert(AAS_FreeOldestCache());assert(aasworld.routingcachesize==before-expected);
  assert(aasworld.clusterareacache[1][3]==portalArea);cacheLinks();
  while(AAS_FreeOldestCache())cacheLinks();
  assert(aasworld.routingcachesize>0);shutdown();
 }
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;
  initialize();graph(0,131,1,true);
  assert(aasworld.reversedreachability[1].numlinks==130);
  for(int a=2;a<132;++a){aasworld.frameroutingupdates=0;
   assert(AAS_AreaTravelTimeToGoalArea(a,nullptr,1,flags)>0);}
  cacheLinks();shutdown();
  // Safe failures for geometry excluded from routing and missing cost rows.
  initialize();graph();aasworld.areasettings[1].cluster=0;
  assert(AAS_AreaTravelTimeToGoalArea(1,nullptr,2,flags)==0);
  assert(AAS_AreaTravelTimeToGoalArea(2,nullptr,1,flags)==0);
  aasworld.areasettings[1].cluster=1;aasworld.areasettings[2].numreachableareas=0;
  assert(AAS_AreaTravelTimeToGoalArea(1,nullptr,2,flags)==0);cacheLinks();shutdown();
  for(int fail=0;fail<12;++fail){initialize();graph();failAllocation=fail;bool failed=false;
   try{AAS_AreaTravelTimeToGoalArea(1,nullptr,4,flags);}catch(Drop&){failed=true;}
   failAllocation=-1;shutdown();if(!failed){assert(fail>=4);break;}
  }
 }
}
'''
        self.compile_and_run(source, rtcw_routing_tests.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_import_317_native_qvm_selected_world_bounds_and_shutdown(self):
        source = route_source() + GRAPH + r'''
static int calls;static bool hasOrigin;static vec3_t copiedOrigin;
static int query(int area,vec3_t origin,int goal,int flags){++calls;hasOrigin=origin!=nullptr;
 if(origin)VectorCopy(origin,copiedOrigin);return AAS_AreaTravelTimeToGoalArea(area,origin,goal,flags);}
int main(){
 using namespace fntech3::rtcw::module;botlib_export_t backend{};backend.aas.AAS_AreaTravelTimeToGoalArea=query;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()){intptr_t result=17;
   assert(!SV_RTCWBotLibCall(nullptr,Call::BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA,nullptr,result)&&result==17);continue;}
  for(bool qvm:{false,true}){initialize();botlib_export=&backend;
   std::array<byte,128> memory{};vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=128;vm.dataMask=127;
   if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](size_t n){return qvm?(intptr_t)n:reinterpret_cast<intptr_t>(memory.data()+n);};
   auto invoke=[&](intptr_t a,intptr_t origin,intptr_t g,intptr_t flags){
    intptr_t args[]={317,a,origin,g,flags},result=99;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,317),args,result));return result;};
   auto reject=[&](intptr_t a,intptr_t origin,intptr_t g,intptr_t flags){
    auto saved=memory;int count=calls;try{invoke(a,origin,g,flags);assert(false);}catch(Drop&){}
    assert(memory==saved&&calls==count);};
   assert(invoke(1,0,1,-1)==0);AAS_SetCurrentWorld(0);graph();AAS_SetCurrentWorld(1);graph(17);
   vec3_t point={3,4,5};std::memcpy(memory.data()+1,point,12);std::memcpy(memory.data()+116,point,12);
   for(int world:{0,1,0}){AAS_SetCurrentWorld(world);auto *selected=aas_currentWorld;
    for(int a:{-1,0,1,4,10,12,13,INT_MIN,INT_MAX})for(int goal:{0,1,4,12,13,INT_MAX})
    for(int flags:{TFL_WALK|TFL_AIR,-1,INT_MIN})for(int at:{0,1,116}){
     aasworld.frameroutingupdates=0;int expected=AAS_AreaTravelTimeToGoalArea(a,at?point:nullptr,goal,flags);
     aasworld.frameroutingupdates=0;auto saved=memory;
     assert(invoke(a,at?address(at):0,goal,flags)==expected&&memory==saved&&aas_currentWorld==selected);
     assert(hasOrigin==bool(at));if(at)assert(!std::memcmp(point,copiedOrigin,12));
    }cacheLinks();
   }
   for(uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00001u,0x48000001u,0xc8000001u})for(int axis=0;axis<3;++axis){
    std::memcpy(memory.data()+1,point,12);std::memcpy(memory.data()+1+axis*4,&bits,4);reject(1,address(1),4,-1);}
   std::memcpy(memory.data()+1,point,12);
   if(qvm){reject(1,address(117),4,-1);reject(1,-1,4,-1);reject(1,129,4,-1);}
   if(sizeof(intptr_t)>4){const intptr_t wide=static_cast<intptr_t>(INT64_C(0x100000000));
    reject(wide,0,4,-1);reject(1,0,wide,-1);reject(1,0,4,wide);}
   botlib_export=nullptr;reject(1,0,4,-1);botlib_export=&backend;
   shutdown();assert(invoke(1,0,1,-1)==0&&invoke(1,address(1),4,-1)==0);
  }
 }
}
'''
        self.compile_and_run(source, rtcw_routing_tests.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
