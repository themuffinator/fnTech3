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

This compiles fnTech3's production allocation and floods
(code/botlib/be_aas_route.cpp, code/botlib/wolfet_routing.hpp) with the
production visibility accessors over a generated world. No retail file is
read. Each mutation restores one earlier behaviour and must make the probe
fail. See docs/fntech3/2026-10-15-census-small-gaps.md.
"""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing
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
// The alternative-route and hide searches are compiled but not run here.
int AAS_PointAreaNum(vec3_t){assert(false);return 0;}
int AAS_AreaReachability(int){assert(false);return 0;}
extern "C" vec_t VectorNormalize(vec3_t){assert(false);return 0;}
void AAS_ProjectPointOntoVector(vec3_t,vec3_t,vec3_t,vec3_t){assert(false);}
static size_t records(const void *array){return allocationSizes.at(const_cast<void*>(array))/sizeof(aas_routingupdate_t);}
static const int flags=TFL_WALK|TFL_AIR;
// A corridor 1-2-3-4-5-6 with a branch 2-7-8-9, every link both ways. The
// corridor runs along x at 100 units an area; the branch stays within 120
// units of area 1, so no danger or retreat search settles there.
static void corridor(){
 initialize();const int count=9;
 static const float position[count+1][2]={{0,0},{100,0},{200,0},{300,0},{400,0},{500,0},{600,0},
  {140,40},{150,70},{160,100}};
 const int pairs[][2]={{1,2},{2,3},{3,4},{4,5},{5,6},{2,7},{7,8},{8,9}};
 std::vector<std::vector<int>> destinations(count+1);
 for(const auto &pair:pairs){destinations[pair[0]].push_back(pair[1]);destinations[pair[1]].push_back(pair[0]);}
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
'''

CHECKS = r'''
int main(){processSetup();
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
 std::printf("ET update records and tactical queues match etded.exe 2.60b\n");
 return 0;
}
'''


def production():
    route = (ROOT / 'code/botlib/be_aas_route.cpp').read_text(encoding='utf-8')
    visibility = (ROOT / 'code/botlib/wolfet_visibility.hpp').read_text(encoding='utf-8')
    floods = (ROOT / 'code/botlib/wolfet_routing.hpp').read_text(encoding='utf-8')
    parts = {
        'allocation': function(route, 'static void AAS_InitRoutingUpdate('),
        'visibility': '\n'.join(function(visibility, signature) for signature in (
            'void AAS_WolfETFreeVisibility( void )', 'int AAS_WolfETAreaVisible( int srcarea, int destarea )',
            'const float *AAS_WolfETWaypoint( int area )')),
        'floods': floods,
    }
    return parts


def source(parts):
    base = routes.route_source() + routes.GRAPH
    for anchor in (VISIBILITY_STUBS, ALLOCATIONS, ALLOCATOR):
        assert base.count(anchor) == 1, anchor
    base = (base.replace(VISIBILITY_STUBS, VISIBILITY_DECLARATIONS).replace(ALLOCATIONS, SIZED_ALLOCATIONS)
            .replace(ALLOCATOR, SIZED_ALLOCATOR))
    base = '#include <map>\n#include <cstdio>\n' + base
    return (base + '\n' + parts['allocation'] + '\n' + parts['visibility'] + '\n'
            + parts['floods'] + '\n' + WORLD + CHECKS)


MUTATIONS = {
    "ET's update records sized as Quake III's": (
        'allocation', '\tif (Com_GameIsWolfET()) maxreachabilityareas = std::max(maxreachabilityareas, aasworld.numareas);\n', ''),
    'the danger search refuses a marked record': (
        'floods', 'keeps the same areas reachable within the time limits. */\n\t\t\twork.Append( update );',
        'keeps the same areas reachable within the time limits. */\n\t\t\twork.Push( update );'),
    'the retreat refuses a marked record': (
        'floods', 'bestDistance = static_cast<float>( danger );\n\t\t\t}\n\t\t\tauto *update = &aasworld.areaupdate[next];\n'
        '\t\t\tupdate->areanum = next;\n\t\t\tVectorCopy( reach.end, update->start );\n\t\t\twork.Append( update );',
        'bestDistance = static_cast<float>( danger );\n\t\t\t}\n\t\t\tauto *update = &aasworld.areaupdate[next];\n'
        '\t\t\tupdate->areanum = next;\n\t\t\tVectorCopy( reach.end, update->start );\n\t\t\twork.Push( update );'),
    'the range search leaves its queue marked': ('floods', '\twork.Drain();\n\treturn count;', '\treturn count;'),
    'the range search queues a marked record': (
        'floods', '\tvoid Push( aas_routingupdate_t *update ) {\n\t\tif ( update->inlist ) return;\n',
        '\tvoid Push( aas_routingupdate_t *update ) {\n'),
}


class WolfETTacticalQueueTests(unittest.TestCase):
    def compile_and_run(self, text):
        scratch = ROOT / '.tmp/wolfet-tactical-queue-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with patch.object(tempfile, 'tempdir', str(scratch)):
            routing.RTCWRoutingTests.compile_and_run(self, text, routing.RTCWRoutingTests.extra(),
                                                     ['-O2', '-ffast-math'])

    def test_update_records_and_queue_marks_follow_etded(self):
        self.compile_and_run(source(production()))

    def test_each_earlier_behaviour_fails(self):
        parts = production()
        for description, (part, old, new) in MUTATIONS.items():
            with self.subTest(mutation=description):
                text = parts[part].replace('\r\n', '\n')
                self.assertEqual(text.count(old), 1, old)
                mutated = dict(parts, **{part: text.replace(old, new)})
                with self.assertRaises(AssertionError):
                    self.compile_and_run(source(mutated))


if __name__ == '__main__':
    unittest.main()
