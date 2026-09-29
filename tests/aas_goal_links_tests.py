"""Routing to an area entered by more than 128 links costs each of them nothing inside the goal.

Quake III's routing starts every area cache in its goal area, where the links
into the area cost no travel time, and walks the area's reversed links from
there. Retail quake3.exe 1.32 and quakelive_steam.exe keep those zeros in a
stack array of 128 in AAS_UpdateAreaRoutingCache, but an area takes a
reversed link from each of the first 128 reachabilities of every other area,
so it can have more: routing to it read the rest of the stack as travel
times. Installed files have such areas (Team Arena's mpq3tourney6 134 links,
Quake Live's blackcathedral 198 and reflux 138, Elite Force's hm_cam 355).
The row is now allocated with the reversed links, as long as the most links
into any area. RTCW's routing starts from the goal area's own travel times
and is unchanged (docs/fntech3/2026-09-28-aas-goal-links.md), and so, since
docs/fntech3/2026-09-29-stvef-route-seed.md, does Elite Force Holomatch's,
whose links into a goal pay the time inside it; its routes must still match
the same world without the fillers.

The fixtures compile the routing from its source over worlds whose goal, or
the portal between two clusters, is entered by up to 4,013 links: filler
areas numbered after the rest lead into it alone, so its reversed links put
theirs first and push every other link into it past the 128th. Every route
must match the same world without the fillers, where fewer than 128 links
enter. Under a guard, the stack above the routing holds a word no travel
time here comes to, so a row read past its end cannot pass for zeros.
"""
from pathlib import Path
import unittest

import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing

ROOT = Path(__file__).resolve().parents[1]

WORLD = r'''
#include <cstdio>
#include <stdlib.h>
static unsigned mix(unsigned seed,unsigned a,unsigned b,unsigned salt){
 unsigned h=seed*0x9e3779b9u^(a+1)*0x85ebca6bu^(b+1)*0xc2b2ae35u^(salt+1)*0x27d4eb2fu;
 h^=h>>15;h*=0x2c1b3c6du;h^=h>>12;h*=0x297a2d39u;h^=h>>15;return h;
}
struct GoalWorld{int goal,portal,into,firstFiller,fillers;std::vector<int> named;};
// Area 1 is the goal, and the `core` areas after it share its cluster: they
// lead to each other and to it, and it leads to each of them. With a portal,
// area core+2 joins that cluster to a second one, whose `core` areas follow
// it and lead to each other and to the portal, which leads to all of both.
// The fillers come last and lead only into the goal, or into the portal from
// the second cluster; nothing leads to them. The named areas' costs, travel
// types, postures and contents vary with the seed alone, so each world
// routes them as it does without its fillers.
static GoalWorld goalWorld(unsigned seed,int core,int fillers,bool portal){
 GoalWorld w{1,portal?core+2:0,0,0,fillers,{}};w.into=portal?w.portal:w.goal;
 const int named=portal?2*core+2:core+1,total=named+fillers;w.firstFiller=named+1;
 std::vector<int> one,two,cluster(total+1,1);std::vector<std::vector<int>> leads(total+1);
 for(int a=1;a<=core+1;++a)one.push_back(a);
 if(portal)for(int a=core+3;a<=named;++a){two.push_back(a);cluster[a]=2;}
 auto join=[&](std::vector<int> group){if(portal)group.push_back(w.portal);
  for(int a:group)for(int b:group)if(a!=b)leads[a].push_back(b);};
 join(one);if(portal){join(two);cluster[w.portal]=-1;}
 for(int a=w.firstFiller;a<=total;++a){leads[a]={w.into};cluster[a]=portal?2:1;}
 for(auto &l:leads){std::sort(l.begin(),l.end());l.erase(std::unique(l.begin(),l.end()),l.end());}
 for(int a=1;a<=named;++a)w.named.push_back(a);
 aasworld.numareas=aasworld.numareasettings=total+1;aasworld.numclusters=portal?3:2;
 aasworld.numportals=portal?2:1;aasworld.portalindexsize=portal?2:0;
 aasworld.areas=botlib::ClearedAlloc<aas_area_t>(total+1);
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(total+1);
 aasworld.rtcwGroundSteepness=botlib::ClearedAlloc<float>(total+1);
 aasworld.clusters=botlib::ClearedAlloc<aas_cluster_t>(aasworld.numclusters);
 aasworld.portals=botlib::ClearedAlloc<aas_portal_t>(aasworld.numportals);
 aasworld.portalindex=botlib::ClearedAlloc<aas_portalindex_t>(2);
 // each cluster numbers its named areas, then the portal, then its fillers
 int count[3]={};
 for(int a=1;a<=named;++a)if(cluster[a]>0)aasworld.areasettings[a].clusterareanum=count[cluster[a]]++;
 if(portal){auto &p=aasworld.portals[1];p.areanum=w.portal;p.frontcluster=1;p.backcluster=2;
  p.clusterareanum[0]=count[1]++;p.clusterareanum[1]=count[2]++;
  aasworld.portalindex[0]=aasworld.portalindex[1]=1;aasworld.clusters[2].firstportal=1;
  aasworld.clusters[1].numportals=aasworld.clusters[2].numportals=1;}
 for(int a=w.firstFiller;a<=total;++a)aasworld.areasettings[a].clusterareanum=count[cluster[a]]++;
 for(int c=1;c<aasworld.numclusters;++c)aasworld.clusters[c].numareas=aasworld.clusters[c].numreachabilityareas=count[c];
 int links=0;
 for(int a=1;a<=total;++a){auto &s=aasworld.areasettings[a];auto &area=aasworld.areas[a];
  s.cluster=cluster[a];s.firstreachablearea=links;s.numreachableareas=(int)leads[a].size();links+=s.numreachableareas;
  s.presencetype=PRESENCE_NORMAL;s.areaflags=AREA_GROUNDED;area.areanum=a;
  area.center[0]=float((a<w.firstFiller?a:w.into)*8);area.center[1]=float(a%3);
  if(a<w.firstFiller&&a!=w.goal&&a!=w.portal){const unsigned v=mix(seed,a,0,0);
   s.presencetype=(v&1)?PRESENCE_NORMAL:PRESENCE_CROUCH;
   s.contents=v%7==0?AREACONTENTS_DONOTENTER:v%5==0?AREACONTENTS_WATER:0;
   if(s.contents&AREACONTENTS_WATER)s.areaflags|=AREA_LIQUID;}
 }
 aasworld.reachabilitysize=links;aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(links);
 static const int types[]={TRAVEL_WALK,TRAVEL_CROUCH,TRAVEL_BARRIERJUMP,TRAVEL_JUMP,TRAVEL_WALKOFFLEDGE,
  TRAVEL_JUMPPAD,TRAVEL_ELEVATOR};
 for(int a=1;a<=total;++a){int r=aasworld.areasettings[a].firstreachablearea;
  for(int b:leads[a]){auto &reach=aasworld.reachability[r++];reach.areanum=b;
   VectorCopy(aasworld.areas[a].center,reach.start);VectorCopy(aasworld.areas[b].center,reach.end);
   if(a<w.firstFiller){const unsigned v=mix(seed,a,b,1);reach.traveltype=types[v%7];reach.traveltime=5+(v>>8)%300;
    reach.start[1]+=float((v>>16)%9);reach.end[2]+=float((v>>24)%9);}
   // a filler enters at the centre, no farther from any way on than a named
   // area's link, so the most a portal costs to cross stays the same
   else{reach.traveltype=TRAVEL_WALK;reach.traveltime=5+mix(seed,a,b,2)%300;}
  }
 }
 routingTables();
 return w;
}
static int plainRoute(int area,vec3_t origin,int goal,int flags,int *time,int *reach){
 return AAS_AreaRouteToGoalArea(area,origin,goal,flags,time,reach);
}
static int (*query)(int,vec3_t,int,int,int*,int*)=plainRoute;
static const int FLAGS[]={TFL_DEFAULT,TFL_WALK|TFL_AIR,TFL_WALK|TFL_CROUCH|TFL_JUMP|TFL_AIR|TFL_WATER};
struct Route{int routed,time,reach;bool operator==(const Route &o)const{return routed==o.routed&&time==o.time&&reach==o.reach;}};
// every named area to the goal and to the portal, with and without an origin;
// without one, a route through a portal keeps the link it is handed, which
// AAS_AreaTravelTimeToGoalArea sets to 0
static std::vector<Route> namedRoutes(const GoalWorld &w){
 std::vector<Route> out;std::vector<int> goals{w.goal};if(w.portal)goals.push_back(w.portal);
 for(int goal:goals)for(int flags:FLAGS)for(int a:w.named)for(bool origin:{false,true}){
  vec3_t at;VectorCopy(aasworld.areas[a].center,at);at[2]+=4;int time=-1,reach=0;
  aasworld.frameroutingupdates=0;const int routed=query(a,origin?at:nullptr,goal,flags,&time,&reach);
  out.push_back({routed,routed?time:0,routed?reach:0});
 }
 return out;
}
// A filler's one link into the goal, or into the portal, costs what the
// cache starts at plus its own time: none inside the area it enters, where
// Quake III starts from zeros. Elite Force Holomatch's engine starts from the
// goal's own first row (stvoyHM.exe 0x004992d0), so there the link also pays
// the time inside the goal from its end to the start of the goal's first
// reachability.
static void fillerRoutes(const GoalWorld &w){
 const int firstOut=aasworld.areasettings[w.into].firstreachablearea;
 for(int flags:FLAGS){int across=-1;
  for(int a=w.firstFiller;a<w.firstFiller+w.fillers;++a){const auto &s=aasworld.areasettings[a];
   const int link=aasworld.reachability[s.firstreachablearea].traveltime;int time=-1,reach=-1;
   vec3_t end,out;VectorCopy(aasworld.reachability[s.firstreachablearea].end,end);
   VectorCopy(aasworld.reachability[firstOut].start,out);
   const int inside=Com_GameIsSTVEFMP()?AAS_AreaTravelTime(w.into,end,out):0;
   aasworld.frameroutingupdates=0;assert(query(a,nullptr,w.into,flags,&time,&reach));
   assert(reach==s.firstreachablearea&&time==1+inside+link);
   // on to the goal beyond the portal, from the filler's centre, every
   // filler pays the same besides its link
   if(w.portal){vec3_t at;VectorCopy(aasworld.areas[a].center,at);reach=-1;
    const int routed=query(a,at,w.goal,flags,&time,&reach);
    const int rest=routed?time-link:0;if(across<0)across=rest;assert(rest==across);
    if(routed)assert(reach==s.firstreachablearea);}
  }
 }
}
// Every profile routing as Quake III does, which takes in those whose
// installed files have such areas: Quake III and Team Arena, Quake Live and
// Elite Force's Holomatch.
static std::vector<int> quakeRoutingProfiles(){
 std::vector<int> ids;
 for(const auto &p:profiles){profile=&p;if(p.bots.navigationWorlds&&!Com_GameIsRTCW())ids.push_back(p.id);}
 for(int id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_STVEF_MP})
  assert(std::find(ids.begin(),ids.end(),id)!=ids.end());
 return ids;
}
static void useProfile(int id){for(const auto &p:profiles)if(p.id==id)profile=&p;}
static int compareWorlds(unsigned seed,int core,bool portal,std::initializer_list<int> extra){
 initialize();GoalWorld w=goalWorld(seed,core,0,portal);
 const int base=aasworld.reversedreachability[w.into].numlinks;assert(base>0&&base<128);
 const auto expected=namedRoutes(w);shutdown();
 std::vector<int> counts(extra);if(counts.empty())counts={128-base,129-base,200,1000};
 for(int fillers:counts){initialize();w=goalWorld(seed,core,fillers,portal);
  assert(aasworld.reversedreachability[w.into].numlinks==base+fillers);
  if(namedRoutes(w)!=expected){fprintf(stderr,"seed %u core %d portal %d fillers %d: routes differ\n",seed,core,portal,fillers);abort();}
  fillerRoutes(w);shutdown();
 }
 return (int)expected.size();
}
'''

EQUAL_ROUTES = r'''
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 int compared=0;
 for(int id:quakeRoutingProfiles()){useProfile(id);
  for(bool portal:{false,true})for(int core:{3,6})for(unsigned seed=1;seed<=4;++seed)
   compared+=compareWorlds(seed,core,portal,{});
 }
 printf("%d routes compared\n",compared);
}
'''

GUARDED_ROUTES = r'''
#if defined(__GNUC__)||defined(__clang__)
#define GOAL_NOINLINE __attribute__((noinline))
#else
#define GOAL_NOINLINE __declspec(noinline)
#endif
// The routing runs below a frame full of this word; nothing here costs as
// much, so a travel time read from the stack past a row cannot pass as zero.
static constexpr unsigned short GUARD=0x5a5a;
static int (*volatile unguarded)(int,vec3_t,int,int,int*,int*)=AAS_AreaRouteToGoalArea;
static GOAL_NOINLINE int guardedRoute(int area,vec3_t origin,int goal,int flags,int *time,int *reach){
 volatile unsigned short guard[8192];
 for(int i=0;i<8192;++i)guard[i]=GUARD;
 const int routed=unguarded(area,origin,goal,flags,time,reach);
 for(int i=0;i<8192;i+=1024)if(guard[i]!=GUARD)abort();
 return routed;
}
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 query=guardedRoute;
 for(int id:quakeRoutingProfiles()){useProfile(id);
  for(bool portal:{false,true})compareWorlds(7,6,portal,{300,1000,4000});
 }
}
'''


class AasGoalLinkTests(unittest.TestCase):
    def run_checks(self, checks):
        routing.RTCWRoutingTests.compile_and_run(self, routes.route_source() + routes.GRAPH + WORLD + checks,
                                                 routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_a_goal_entered_by_more_than_128_links_routes_as_one_entered_by_fewer(self):
        self.run_checks(EQUAL_ROUTES)

    def test_links_past_the_128th_read_no_stack_under_a_guard(self):
        self.run_checks(GUARDED_ROUTES)


if __name__ == '__main__':
    unittest.main()
