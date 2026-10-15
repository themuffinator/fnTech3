"""Run ET's tactical floods over the update records ET allocates.

Retail etded.exe 2.60b (SHA-256 7eb60691...cdf) sizes the routing update
records by the world's areas (AAS_InitRoutingUpdate 0x0044a1d0, numareas *
0x28 bytes at 0x0044a1ed), because its tactical searches address them by area
number. Two of those searches queue a record without looking at its queued
mark (AAS_AvoidDangerArea 0x00447f30, queue 0x00448166..0x00448182;
AAS_Retreat 0x0044b790, queue 0x0044ba85..0x0044ba9e), and two refuse a
marked record (AAS_ListAreasInRange 0x0044a450 at 0x0044a663,
AAS_NearestHideArea 0x0044a6d0 at 0x0044ad4d). A search that returns early
leaves its queue marked. The range search keeps flooding once its list is full
(0x0044a63a), past the caller's array, so it leaves no marks of its own.

The hide search runs over the same corridor with the game's answers given by
the probe, which records every question. Retail AAS_NearestHideArea skips
ladder and disabled areas (0x0044a93a), measures its distance limit to where a
step ends (0x0044a94e), refuses or charges a step near or towards an enemy
more than 32 units away (0x0044a9ff..0x0044ac27), charges 1,000 for an
occupied area (0x0044ac30), asks the game about the start before it refuses
area 0 (0x0044a832, 0x0044a86a) and then once about each area
(0x0044acb0..0x0044ad7b), and counts a step for every candidate and every
queue test (0x0044a998, 0x0044ad0c): past 3,000 it queues nothing more but
empties its queue. The searches one frame answers measure those steps.

This compiles fnTech3's production allocation and floods
(code/botlib/be_aas_route.cpp, code/botlib/wolfet_routing.hpp) with the
production visibility accessors and vector helpers over a generated world. No
retail file is read. Each mutation restores one earlier behaviour or drops one
retail rule and must make the probe fail; a hide mutation must fail in its own
scenario. See docs/fntech3/2026-10-15-census-small-gaps.md and
docs/fntech3/2026-10-15-wolfet-hide-search.md.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import time
import unittest
from unittest.mock import patch

import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

# The shared fixtures stand in for ET's visibility; these floods need the
# production accessors over tables the probe fills.
VISIBILITY_STUBS = '''void AAS_WolfETBuildVisibility(){}
void AAS_WolfETFreeVisibility(){}
int AAS_WolfETAreaVisible(int,int){return 0;}
const float *AAS_WolfETWaypoint(int){return nullptr;}
'''
VISIBILITY_DECLARATIONS = '''void AAS_WolfETBuildVisibility(){}
void AAS_WolfETFreeVisibility();
int AAS_WolfETAreaVisible(int,int);
const float *AAS_WolfETWaypoint(int);
'''
ALLOCATIONS = 'static std::set<void*> allocations;\n'
SIZED_ALLOCATIONS = 'static std::set<void*> allocations;\nstatic std::map<void*,unsigned long> allocationSizes;\n'
ALLOCATOR = 'auto *p=std::malloc(size);assert(p);allocations.insert(p);return p;}'
SIZED_ALLOCATOR = 'auto *p=std::malloc(size);assert(p);allocations.insert(p);allocationSizes[p]=size;return p;}'

WORLD = r'''
// The alternative-route search is compiled but not run here.
int AAS_PointAreaNum(vec3_t){assert(false);return 0;}
int AAS_AreaReachability(int){assert(false);return 0;}
static size_t records(const void *array){return allocationSizes.at(const_cast<void*>(array))/sizeof(aas_routingupdate_t);}
static const int flags=TFL_WALK|TFL_AIR;
// A corridor 1-2-3-4-5-6 with a branch 2-7-8-9, every link both ways, and on
// request a link 3-8 closing a ring. The corridor runs along x at 100 units
// an area; the branch stays within 120 units of area 1, so no danger or
// retreat search settles there.
static void corridor(bool ring=false){
 initialize();const int count=9;
 static const float position[count+1][2]={{0,0},{100,0},{200,0},{300,0},{400,0},{500,0},{600,0},
  {140,40},{150,70},{160,100}};
 const int pairs[][2]={{1,2},{2,3},{3,4},{4,5},{5,6},{2,7},{7,8},{8,9}};
 std::vector<std::vector<int>> destinations(count+1);
 for(const auto &pair:pairs){destinations[pair[0]].push_back(pair[1]);destinations[pair[1]].push_back(pair[0]);}
 if(ring){destinations[3].push_back(8);destinations[8].push_back(3);}
 aasworld.numareas=aasworld.numareasettings=count+1;aasworld.numclusters=2;aasworld.numportals=1;
 aasworld.areas=botlib::ClearedAlloc<aas_area_t>(count+1);
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(count+1);
 aasworld.rtcwGroundSteepness=botlib::ClearedAlloc<float>(count+1);
 aasworld.clusters=botlib::ClearedAlloc<aas_cluster_t>(2);
 aasworld.portals=botlib::ClearedAlloc<aas_portal_t>(1);
 aasworld.clusters[1].numareas=aasworld.clusters[1].numreachabilityareas=count;
 int links=0;
 for(int a=1;a<=count;++a){auto &s=aasworld.areasettings[a];auto &d=destinations[a];std::sort(d.begin(),d.end());
  s.cluster=1;s.clusterareanum=a-1;s.presencetype=PRESENCE_NORMAL;s.areaflags=AREA_GROUNDED;
  s.firstreachablearea=links;s.numreachableareas=(int)d.size();links+=s.numreachableareas;
  aasworld.areas[a].areanum=a;aasworld.areas[a].center[0]=position[a][0];aasworld.areas[a].center[1]=position[a][1];
  aasworld.areas[a].center[2]=24;}
 aasworld.reachabilitysize=links;aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(links);
 for(int a=1;a<=count;++a){int offset=aasworld.areasettings[a].firstreachablearea;
  for(int b:destinations[a]){auto &r=aasworld.reachability[offset++];r.areanum=b;r.traveltype=TRAVEL_WALK;
   r.traveltime=10;VectorCopy(aasworld.areas[a].center,r.start);VectorCopy(aasworld.areas[b].center,r.end);}}
 routingTables();
 // The production allocation replaces the fixture's own update records.
 AAS_InitRoutingUpdate();
 assert(records(aasworld.areaupdate)==(size_t)aasworld.numareas);
 // Waypoints at the centres; every area sees every other.
 aasworld.wolfetWaypoints=static_cast<vec3_t*>(GetClearedMemory((count+1)*sizeof(vec3_t)));
 for(int a=1;a<=count;++a)VectorCopy(aasworld.areas[a].center,aasworld.wolfetWaypoints[a]);
 const int stride=(count+1+7)/8;aasworld.wolfetVisibilityStride=stride;
 aasworld.wolfetVisibility=static_cast<byte*>(GetClearedMemory((count+1)*stride));
 for(int a=1;a<=count;++a)for(int b=1;b<=count;++b)aasworld.wolfetVisibility[a*stride+(b>>3)]|=byte(1<<(b&7));
 aasworld.wolfetVisibilityBuilt=qtrue;
}
static float *at(int area){return aasworld.wolfetWaypoints[area];}
static int danger(){return AAS_WolfETAvoidDangerArea(at(1),1,at(1),1,350,flags);}
static int retreat(){int spots[]={1};return AAS_WolfETRetreat(spots,1,at(1),1,at(1),1,250,250,flags);}
static int listed(int maxareas,std::vector<int> *areas=nullptr){
 vec3_t out[16];const int count=AAS_WolfETListAreasInRange(at(1),1,10000,flags,out,maxareas);
 if(areas){areas->clear();for(int i=0;i<count;++i)for(int a=1;a<aasworld.numareas;++a)
  if(VectorCompare(out[i],at(a)))areas->push_back(a);}
 return count;
}
static int marked(){int count=0;for(int a=0;a<aasworld.numareas;++a)count+=aasworld.areaupdate[a].inlist?1:0;return count;}
static void processSetup(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
}

// The game's side of the hide search: whether the enemy sees the source at
// the start, which areas' waypoints it sees, and every question in order
// (the area asked about, or 0 for the start).
static bool gameReady,startSeen;
static std::set<int> seenAreas;
static std::vector<int> asked;
static const float *expectedFrom;
static int expectedEnemy,expectedSource;
static bool gameAvailable(){return gameReady;}
static int gameVisible(const float *from,int enemy,const float *to,int source,int update){
 assert(from==expectedFrom&&enemy==expectedEnemy&&source==expectedSource&&update==0);
 const float *table=aasworld.wolfetWaypoints[0];
 if(to>table&&to<table+3*aasworld.numareas){
  const int area=int((to-table)/3);assert(to==aasworld.wolfetWaypoints[area]);
  asked.push_back(area);return seenAreas.count(area)?1:0;
 }
 asked.push_back(0);return startSeen?1:0;
}
static float frame;
static void nextFrame(){aasworld.time=++frame;}
static void hideWorld(bool ring=false){
 corridor(ring);AAS_SetWolfETCallbacks({gameAvailable,gameVisible});
 gameReady=startSeen=true;seenAreas.clear();asked.clear();nextFrame();
}
// What the game says the enemy sees: every area but those listed.
static std::set<int> allBut(std::initializer_list<int> hidden){
 std::set<int> areas;for(int a=1;a<aasworld.numareas;++a)areas.insert(a);
 for(int a:hidden)areas.erase(a);
 return areas;
}
// Takes some areas out of one area's sight.
static void unseen(int from,std::initializer_list<int> areas){
 const int stride=aasworld.wolfetVisibilityStride;
 for(int a:areas)aasworld.wolfetVisibility[from*stride+(a>>3)]&=byte(~(1<<(a&7)));
}
// Links one entity into an area through the world's own link heap.
static void occupy(int area,int entity,bool solid=true){
 if(!aasworld.linkheap)AAS_InitAASLinkHeap();
 if(!aasworld.arealinkedentities)AAS_InitAASLinkedEntities();
 aas_link_t *link=AAS_AllocAASLink();assert(link);
 link->entnum=entity;link->areanum=area;link->prev_ent=nullptr;link->next_ent=aasworld.arealinkedentities[area];
 if(link->next_ent)link->next_ent->prev_ent=link;
 aasworld.arealinkedentities[area]=link;
 auto &info=AAS_EntityWorld().entities[entity].i;info.valid=qtrue;info.solid=solid?2:0;
}
struct Hide{
 int source=3,area=1,enemy=7,enemyArea=0;
 vec3_t origin{},enemyAt{},distpos{};
 float maxdist=0;bool limited=false;
};
// The source stands at an area's waypoint, and the enemy with it.
static Hide standing(int area){Hide h;h.area=area;VectorCopy(at(area),h.origin);VectorCopy(at(area),h.enemyAt);return h;}
static int lastResult=-1,lastRuns=-1;
static int hide(Hide h){
 expectedFrom=h.enemyAt;expectedEnemy=h.enemy;expectedSource=h.source<0?~h.source:h.source;
 return lastResult=AAS_WolfETNearestHideArea(h.source,h.origin,h.area,h.enemy,h.enemyAt,h.enemyArea,flags,
  h.maxdist,h.limited?h.distpos:nullptr);
}
// The searches one new frame answers before it refuses one, which asks the
// game nothing.
static int runs(const Hide &h){
 nextFrame();int count=0;
 while(count<10000){asked.clear();hide(h);if(asked.empty())break;++count;}
 return lastRuns=count;
}
static bool askedOnly(std::initializer_list<int> areas){return asked==std::vector<int>(areas);}
static const char *scenario="";
[[noreturn]] static void hideFailed(const char *what,int line){
 std::fprintf(stderr,"hide check failed in %s: %s (line %d; result %d, runs %d, asked",scenario,what,line,
  lastResult,lastRuns);
 for(int area:asked)std::fprintf(stderr," %d",area);
 std::fprintf(stderr,")\n");std::exit(3);
}
#define HIDE_CHECK(x) ((x)?(void)0:hideFailed(#x,__LINE__))
'''

CHECKS = r'''
static void queueChecks(){
 // Update records: one for every area under ET, the largest cluster's
 // reachability areas elsewhere, and the portals' own as before.
 int etRuns=0,otherRuns=0;
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds||Com_GameIsRTCW())continue;
  initialize();graph(0,3,3);assert(aasworld.numareas==13&&aasworld.clusters[1].numreachabilityareas==5);
  AAS_InitRoutingUpdate();
  if(Com_GameIsWolfET()){assert(records(aasworld.areaupdate)==13);++etRuns;}
  else{assert(records(aasworld.areaupdate)==5);++otherRuns;}
  assert(records(aasworld.portalupdate)==(size_t)aasworld.numportals+1);
  shutdown();
 }
 assert(etRuns==1&&otherRuns>=3);
 for(const auto &p:profiles)if(p.id==COM_GAME_WOLFET)profile=&p;

 // Unmarked records: the danger search settles on area 5, the first more
 // than 350 units from the danger; the retreat on area 4, the first beyond
 // 250 from the start whose walk from the danger spot runs past 250. The
 // range search marks no source area, so it lists area 1 too, reached back
 // from area 2, as retail does.
 corridor();assert(danger()==5);shutdown();
 corridor();assert(retreat()==4);shutdown();
 std::vector<int> areas;
 corridor();assert(listed(16,&areas)==9&&(areas==std::vector<int>{2,1,3,7,4,8,5,9,6})&&marked()==0);shutdown();

 // A mark left on area 3, the corridor's only way on. The danger and retreat
 // searches queue it anyway.
 corridor();aasworld.areaupdate[3].inlist=qtrue;assert(danger()==5);shutdown();
 corridor();aasworld.areaupdate[3].inlist=qtrue;assert(retreat()==4);shutdown();
 // The range search refuses it: area 3 is listed, nothing beyond it is.
 corridor();aasworld.areaupdate[3].inlist=qtrue;
 assert(listed(16,&areas)==6&&(areas==std::vector<int>{2,1,3,7,8,9}));shutdown();

 // The danger search returns from area 4's links with area 8 still queued,
 // and leaves it marked; the range search then lists area 8 but cannot go on
 // to area 9 through it.
 corridor();assert(danger()==5&&aasworld.areaupdate[8].inlist&&marked()==1);
 assert(listed(16,&areas)==8&&(areas==std::vector<int>{2,1,3,7,4,8,5,6}));shutdown();

 // A range search that stops at its limit with area 3 still queued leaves
 // nothing marked, as retail's continued flood leaves nothing, so the next
 // range search reaches every area.
 corridor();assert(listed(4,&areas)==4&&(areas==std::vector<int>{2,1,3,7}));
 assert(marked()==0);assert(listed(16)==9);shutdown();
}

static void hideChecks(){
 // Every step costs 11 here: 1 to cross the area and 10 for the link.
 const std::vector<int> wholeFlood{0,2,1,3,7,4,8,5,9};

 // With the enemy where the source stands no enemy rule applies, and an
 // enemy outside every area leaves the area visibility silent: the game
 // decides. It is asked about the start, then once about each area reached
 // under the best time so far, the source area included, reached back
 // through area 2. Area 6 (55) is never asked about once area 9 (44) is
 // found.
 {scenario="enemy beside the source";
  hideWorld();seenAreas=allBut({9});Hide a=standing(1);
  HIDE_CHECK(hide(a)==9&&asked==wholeFlood);
  shutdown();}

 // That search takes 24 steps: one for each of its 16 candidates and one
 // for each of the 8 that reach the queue. A frame answers 125 of them (the
 // last starting at 2,976 steps) and refuses the next; a negative source
 // number restarts the count.
 {scenario="frame budget";
  hideWorld();seenAreas=allBut({9});Hide a=standing(1);
  HIDE_CHECK(runs(a)==125);
  Hide again=a;again.source=~a.source;asked.clear();
  HIDE_CHECK(hide(again)==9&&asked==wholeFlood);
  shutdown();}

 // Past 3,000 steps a search queues nothing more but empties its queue. 124
 // such searches and two of 4 steps leave 2,984; the next passes 3,000 at
 // area 5, with area 8 already queued, and still finds area 9. The search
 // after it is refused.
 {scenario="past the limit";
  hideWorld();Hide a=standing(1),last=standing(9);
  seenAreas=allBut({9});for(int i=0;i<124;++i)HIDE_CHECK(hide(a)==9);
  seenAreas=allBut({8});for(int i=0;i<2;++i){asked.clear();HIDE_CHECK(hide(last)==8&&askedOnly({0,8}));}
  seenAreas=allBut({9});asked.clear();HIDE_CHECK(hide(a)==9&&asked==wholeFlood);
  asked.clear();HIDE_CHECK(hide(a)==0&&asked.empty());
  shutdown();}

 // A ladder or disabled area is never entered, nor asked about, and its link
 // takes no step: 17 steps a search behind a ladder at area 4, so 177
 // searches a frame.
 {scenario="ladder and disabled";
  for(int flag:{AREA_LADDER,AREA_DISABLED}){
   hideWorld();aasworld.areasettings[4].areaflags|=flag;Hide a=standing(1);
   seenAreas=allBut({5});HIDE_CHECK(hide(a)==0&&askedOnly({0,2,1,3,7,8,9}));
   seenAreas=allBut({4});asked.clear();HIDE_CHECK(hide(a)==0&&askedOnly({0,2,1,3,7,8,9}));
   seenAreas=allBut({5});HIDE_CHECK(runs(a)==177);
   shutdown();
  }}

 // The distance limit is measured to where each step ends, not to the
 // area's waypoint: area 5's waypoint lies 340 units from the source but its
 // step ends 400 away, and area 4's waypoint 380 away but its step 300. A
 // limit of 0 refuses every step that does not end on the point itself.
 {scenario="distance limit";
  hideWorld();Hide a=standing(1);a.limited=true;VectorCopy(at(1),a.distpos);a.maxdist=350;
  at(5)[0]=440;seenAreas=allBut({5});HIDE_CHECK(hide(a)==0&&askedOnly({0,2,1,3,7,4,8,9}));
  at(4)[0]=480;seenAreas=allBut({4});asked.clear();HIDE_CHECK(hide(a)==4);
  a.maxdist=0;seenAreas=allBut({9});asked.clear();HIDE_CHECK(hide(a)==0&&askedOnly({0}));
  shutdown();}

 // An area is hidden only when the enemy's area cannot see it and the game
 // says the enemy cannot see its waypoint; areas the enemy's area sees are
 // not asked about. Area 6 sees none of the branch and the game sees area
 // 7, so area 8 is the nearest hidden.
 {scenario="area and game visibility";
  hideWorld();unseen(6,{7,8,9});Hide a=standing(1);a.enemyArea=6;seenAreas={7};
  HIDE_CHECK(hide(a)==8&&askedOnly({0,7,8}));
  shutdown();}

 // A source the enemy cannot see at the start never steps into the enemy
 // area's view. Area 6 sees every area but 5, which is hidden and lies
 // beyond areas in view.
 {scenario="unseen source";
  hideWorld();unseen(6,{5});Hide a=standing(1);a.enemyArea=6;seenAreas={};
  HIDE_CHECK(hide(a)==5&&askedOnly({0,5}));
  startSeen=false;asked.clear();HIDE_CHECK(hide(a)==0&&askedOnly({0}));
  shutdown();}

 // An occupied area costs 1,000 more: with entity 5 in area 7 (22), the
 // hidden area 4 (33) is nearer. The searcher itself, or an entity that is
 // not solid, costs nothing.
 {scenario="occupied area";
  hideWorld();occupy(7,5);Hide a=standing(1);seenAreas=allBut({4,7});
  HIDE_CHECK(hide(a)==4&&askedOnly({0,2,1,3,7,4}));
  shutdown();
  hideWorld();occupy(7,a.source);seenAreas=allBut({4,7});
  HIDE_CHECK(hide(a)==7&&askedOnly({0,2,1,3,7}));
  shutdown();
  hideWorld();occupy(7,5,false);seenAreas=allBut({4,7});
  HIDE_CHECK(hide(a)==7);
  shutdown();}

 // With the enemy more than 32 units away no step may pass within 256 units
 // of it and nearer than it stands. From (100,300) the step from area 7 to
 // area 8 ends 235 units from it, and area 8 has no other way in.
 {scenario="past the enemy";
  hideWorld();Hide a=standing(1);a.enemyAt[0]=100;a.enemyAt[1]=300;seenAreas=allBut({8});
  HIDE_CHECK(hide(a)==0);
  shutdown();}

 // Nor may a step end more than halfway towards it. From (1000,0) area 6
 // lies 500 units its way from the source, past half its 900, though the
 // step there ends 400 units from it.
 {scenario="halfway to the enemy";
  hideWorld();Hide a=standing(1);a.enemyAt[0]=1000;seenAreas=allBut({6});
  HIDE_CHECK(hide(a)==0);
  shutdown();}

 // A step that comes nearer costs ten for each unit gained: each step east
 // gains 100 units and costs 1,000 more, so area 9 up the branch (under
 // 1,200) beats area 5 (4,044), found first.
 {scenario="approach cost";
  hideWorld();Hide a=standing(1);a.enemyAt[0]=1000;seenAreas=allBut({5,9});
  HIDE_CHECK(hide(a)==9);
  shutdown();}

 // Round the ring area 8 is reached twice as cheaply, through areas 3 and 7.
 // The game is asked about it once. Its second visit still takes steps, and
 // when it is the best area it is set best again: 22 steps a search, 137
 // searches a frame.
 {scenario="ring";
  hideWorld(true);Hide a=standing(1);seenAreas=allBut({});
  HIDE_CHECK(hide(a)==0&&askedOnly({0,2,1,3,7,4,8,5,9,6}));
  seenAreas=allBut({8});asked.clear();HIDE_CHECK(hide(a)==8&&askedOnly({0,2,1,3,7,4,8}));
  HIDE_CHECK(runs(a)==137);
  shutdown();}

 // The start is asked about before area 0, or one outside the world, is
 // refused.
 {scenario="area 0";
  hideWorld();Hide a=standing(1);seenAreas=allBut({9});
  for(int area:{0,-1,10,100000}){a.area=area;asked.clear();HIDE_CHECK(hide(a)==0&&askedOnly({0}));}
  shutdown();}

 // Retail asks for the enemy's travel time and drops the answer; the query
 // still builds the routing cache towards the enemy's area.
 {scenario="routing cache";
  hideWorld();HIDE_CHECK(!aasworld.newestcache);Hide a=standing(1);a.enemyArea=6;
  HIDE_CHECK(hide(a)==0&&aasworld.newestcache&&aasworld.newestcache->areanum==6);
  shutdown();}

 // Without the game no area can be proved hidden, and nothing is asked.
 {scenario="no game";
  hideWorld();Hide a=standing(1);seenAreas=allBut({9});gameReady=false;
  HIDE_CHECK(hide(a)==0&&asked.empty());
  AAS_SetWolfETCallbacks({});HIDE_CHECK(hide(a)==0&&asked.empty());
  shutdown();}
}

int main(){processSetup();
 queueChecks();
 hideChecks();
 std::printf("ET update records, tactical queues and hide search match etded.exe 2.60b\n");
 return 0;
}
'''


def production():
    route = (ROOT / 'code/botlib/be_aas_route.cpp').read_text(encoding='utf-8')
    visibility = (ROOT / 'code/botlib/wolfet_visibility.hpp').read_text(encoding='utf-8')
    floods = (ROOT / 'code/botlib/wolfet_routing.hpp').read_text(encoding='utf-8')
    main = (ROOT / 'code/botlib/be_aas_main.cpp').read_text(encoding='utf-8')
    math = (ROOT / 'code/qcommon/q_math.c').read_text(encoding='utf-8')
    parts = {
        'allocation': function(route, 'static void AAS_InitRoutingUpdate('),
        'visibility': '\n'.join(function(visibility, signature) for signature in (
            'void AAS_WolfETFreeVisibility( void )', 'int AAS_WolfETAreaVisible( int srcarea, int destarea )',
            'const float *AAS_WolfETWaypoint( int area )')),
        # The hide search projects and normalizes with the engine's own helpers.
        'vectors': ('extern "C" ' + function(math, 'vec_t VectorNormalize( vec3_t v ) {') + '\n'
                    + function(main, 'void AAS_ProjectPointOntoVector(')),
        'floods': floods,
    }
    return parts


def source(parts):
    base = routes.route_source() + routes.GRAPH
    for anchor in (VISIBILITY_STUBS, ALLOCATIONS, ALLOCATOR):
        assert base.count(anchor) == 1, anchor
    base = (base.replace(VISIBILITY_STUBS, VISIBILITY_DECLARATIONS).replace(ALLOCATIONS, SIZED_ALLOCATIONS)
            .replace(ALLOCATOR, SIZED_ALLOCATOR))
    base = '#include <map>\n#include <cstdio>\n#include <cstdlib>\n' + base
    return (base + '\n' + parts['allocation'] + '\n' + parts['visibility'] + '\n' + parts['vectors'] + '\n'
            + parts['floods'] + '\n' + WORLD + CHECKS)


# The hide search's first link test, where the earlier search counted a step.
HIDE_LINK = ('\t\t\tconst auto &reach = aasworld.reachability[first + index];\n'
             '\t\t\tif ( !Travelable( reach, badflags ) ) continue;\n'
             '\t\t\tconst int next = reach.areanum;\n'
             '\t\t\tif ( Blocked( next ) || next == enemyareanum ) continue;\n')
HIDE_STEP = '\t\t\t++loopCount;\n'
HIDE_QUEUE_STEP = '\t\t\tif ( loopCount++ > maxLoops ) continue;\n'
HIDE_GAME_TEST = ('if ( seen[next] == 2 || ( !seen[next] && !callbacks.visible( enemyorigin, enemynum,\n'
                  '\t\t\t\t\taasworld.wolfetWaypoints[next], srcnum, 0 ) ) ) {')
HIDE_START = '\n\tWork work;\n\tif ( !work.Begin( areanum, origin ) ) return 0;\n'
HIDE_START_TEST = '\t// Asked before the start area is refused'

# Each entry: (part, ((old, new), ...), the hide scenario that must fail or
# None for the queue checks' own assertions).
MUTATIONS = {
    "ET's update records sized as Quake III's": (
        'allocation', (('\tif (Com_GameIsWolfET()) maxreachabilityareas = std::max(maxreachabilityareas, aasworld.numareas);\n', ''),),
        None),
    'the danger search refuses a marked record': (
        'floods', (('keeps the same areas reachable within the time limits. */\n\t\t\twork.Append( update );',
                    'keeps the same areas reachable within the time limits. */\n\t\t\twork.Push( update );'),),
        None),
    'the retreat refuses a marked record': (
        'floods', (('bestDistance = static_cast<float>( danger );\n\t\t\t}\n\t\t\tauto *update = &aasworld.areaupdate[next];\n'
                    '\t\t\tupdate->areanum = next;\n\t\t\tVectorCopy( reach.end, update->start );\n\t\t\twork.Append( update );',
                    'bestDistance = static_cast<float>( danger );\n\t\t\t}\n\t\t\tauto *update = &aasworld.areaupdate[next];\n'
                    '\t\t\tupdate->areanum = next;\n\t\t\tVectorCopy( reach.end, update->start );\n\t\t\twork.Push( update );'),),
        None),
    'the range search leaves its queue marked': ('floods', (('\twork.Drain();\n\treturn count;', '\treturn count;'),), None),
    'the range search queues a marked record': (
        'floods', (('\tvoid Push( aas_routingupdate_t *update ) {\n\t\tif ( update->inlist ) return;\n',
                    '\tvoid Push( aas_routingupdate_t *update ) {\n'),),
        None),
    'the hide search trusts the area visibility alone': (
        'floods', ((HIDE_GAME_TEST, 'if ( true ) {'),), 'enemy beside the source'),
    'the hide search refuses a step ending within 40 units of the enemy': (
        'floods', (('\t\t\tconst bool distant = enemyDistance > 32;\n',
                    '\t\t\tif ( passDistance < 40 ) continue;\n\t\t\tconst bool distant = enemyDistance > 32;\n'),),
        'enemy beside the source'),
    'the hide search charges an approach to an enemy beside it': (
        'floods', (('\t\t\tif ( distant && passDistance < startDistance ) {', '\t\t\tif ( passDistance < startDistance ) {'),),
        'enemy beside the source'),
    'the hide search counts each link before its tests and returns once over': (
        'floods', ((HIDE_STEP, ''), (HIDE_QUEUE_STEP, ''),
                   (HIDE_LINK, '\t\t\tif ( ++loopCount > maxLoops ) return best;\n' + HIDE_LINK)),
        'frame budget'),
    'the hide search returns as soon as its steps run out': (
        'floods', ((HIDE_STEP, '\t\t\tif ( ++loopCount > maxLoops ) return best;\n'),), 'past the limit'),
    'the hide search counts a step before its tests': (
        'floods', ((HIDE_STEP, ''), (HIDE_LINK, HIDE_STEP + HIDE_LINK)), 'ladder and disabled'),
    'the hide search passes ladder and disabled areas': (
        'floods', (('\t\t\tif ( Blocked( next ) || next == enemyareanum ) continue;\n',
                    '\t\t\tif ( next == enemyareanum ) continue;\n'),),
        'ladder and disabled'),
    "the hide search limits its distance at the area's waypoint": (
        'floods', (('\t\t\tif ( distpos && Distance( reachEnd, distpos ) > maxdist ) continue;\n',
                    '\t\t\tif ( maxdist > 0 && distpos && AAS_WolfETWaypoint( next )\n'
                    '\t\t\t\t&& Distance( distpos, AAS_WolfETWaypoint( next ) ) > maxdist ) continue;\n'),),
        'distance limit'),
    "the hide search lets an unseen source step into the enemy area's view": (
        'floods', (('\t\t\tif ( enemyareanum && !startVisible && AAS_WolfETAreaVisible( enemyareanum, next ) ) continue;\n',
                    ''),),
        'unseen source'),
    'the hide search charges nothing for an occupied area': (
        'floods', (('\t\t\tif ( Occupied( next, srcnum ) ) travel = static_cast<unsigned short>( travel + 1000 );\n', ''),),
        'occupied area'),
    'the hide search counts the searcher as an occupant': (
        'floods', (('\t\t\tif ( Occupied( next, srcnum ) )', '\t\t\tif ( Occupied( next, -1 ) )'),), 'occupied area'),
    'the hide search lets a step pass beside the enemy': (
        'floods', (('\t\t\tif ( distant && passDistance < enemyDistance && passDistance < 256 ) continue;\n', ''),),
        'past the enemy'),
    'the hide search lets a step lead more than halfway to the enemy': (
        'floods', (('\t\t\tif ( distant && DotProduct( step, enemyVec ) > enemyDistance * 0.5f ) continue;\n', ''),),
        'halfway to the enemy'),
    'the hide search charges nothing for coming nearer the enemy': (
        'floods', (('\t\t\t\ttravel = TruncatedTime( static_cast<float>( travel ) + ( startDistance - passDistance ) * 10 );\n',
                    ''),),
        'approach cost'),
    'the hide search asks the game again about an area': (
        'floods', ((HIDE_GAME_TEST, 'if ( !callbacks.visible( enemyorigin, enemynum, aasworld.wolfetWaypoints[next], srcnum, 0 ) ) {'),),
        'ring'),
    'the hide search skips an area reached again as cheaply': (
        'floods', (('aasworld.hidetraveltimes[next] < travel ) continue;', 'aasworld.hidetraveltimes[next] <= travel ) continue;'),),
        'ring'),
    'the hide search keeps its best area when reaching it again': (
        'floods', (('\t\t\tif ( best == next ) {\n\t\t\t\tbest = 0;\n\t\t\t\tbestTravel = 0;\n\t\t\t}\n', ''),), 'ring'),
    'the hide search refuses area 0 before asking about its start': (
        'floods', ((HIDE_START, '\n'), (HIDE_START_TEST, HIDE_START.lstrip('\n') + HIDE_START_TEST)), 'area 0'),
    "the hide search leaves the enemy's routing cache unbuilt": (
        'floods', (('\tif ( enemyareanum ) AAS_AreaTravelTimeToGoalArea( areanum, origin, enemyareanum, travelflags );\n', ''),),
        'routing cache'),
}


def mutate(parts, part, edits):
    text = parts[part].replace('\r\n', '\n')
    for old, new in edits:
        if text.count(old) != 1:
            raise AssertionError(f'{old!r} occurs {text.count(old)} times')
        text = text.replace(old, new)
    return dict(parts, **{part: text})


def remove_folder(folder):
    # A finished probe can hold its executable for a moment after it exits.
    for _ in range(50):
        try:
            shutil.rmtree(folder)
            return
        except PermissionError:
            time.sleep(0.1)
    shutil.rmtree(folder)


class WolfETTacticalQueueTests(unittest.TestCase):
    def compile_and_run(self, text):
        scratch = ROOT / '.tmp/wolfet-tactical-queue-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with patch.object(tempfile, 'tempdir', str(scratch)):
            routing.RTCWRoutingTests.compile_and_run(self, text, routing.RTCWRoutingTests.extra(),
                                                     ['-O2', '-ffast-math'])

    def test_update_records_queue_marks_and_hide_search_follow_etded(self):
        self.compile_and_run(source(production()))

    def test_each_earlier_behaviour_fails(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        folder = Path(tempfile.mkdtemp(prefix='wolfet-tactical-mutations-', dir=ROOT / '.tmp'))
        self.addCleanup(remove_folder, folder)
        parts = production()
        variants = [(f'mutation{index}', source(mutate(parts, part, edits)))
                    for index, (part, edits, _) in enumerate(MUTATIONS.values())]

        def build_and_run(name, text):
            cpp, exe = folder / f'{name}.cpp', folder / f'{name}.exe'
            cpp.write_text(text, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-O2', '-ffast-math', '-std=c++17',
                                    '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(cpp),
                                    *routing.RTCWRoutingTests.extra(), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=600)
            if built.returncode:
                return built, None
            return built, subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)

        # Builds are independent, so run several at once; each clang holds a
        # few hundred megabytes.
        with ThreadPoolExecutor(max_workers=max(1, min(len(variants), os.cpu_count() or 1, 8))) as pool:
            results = list(pool.map(lambda variant: build_and_run(*variant), variants))
        for (description, (_, _, expected)), (built, ran) in zip(MUTATIONS.items(), results):
            with self.subTest(mutation=description):
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                self.assertNotEqual(ran.returncode, 0, description)
                if expected:
                    self.assertIn(f'hide check failed in {expected}:', ran.stderr, ran.stdout + ran.stderr)
                else:
                    self.assertNotIn('hide check failed', ran.stderr, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
