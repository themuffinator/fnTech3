"""Execute ET area caches against numeric observations of original et.exe.

The fixture holds generated inputs and results from the two hash-pinned retail
functions; it contains no executable or navigation bytes. The original probe
runs those functions in an x86 emulator without starting ET. These regressions
compile the production route code, including cache ownership and cost rows.
"""
from pathlib import Path
import json
import unittest

import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/wolfet-area-routing.json'

WORLD = r'''
#include <cstdio>
#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned int __stdcall SetErrorMode(unsigned int);
#endif
struct Case {
 const char *name;
 int areaFlags,travelFlags,contents,inside,link;
 bool missingSourceRow;
 int sourceCluster,sourceIndex,goalIndex;
 bool branch;
 int intermediateFlags;
 bool missingIntermediateRow;
};
static void world(const Case &c){
 initialize();const int count=c.branch?4:2;
 struct Link{int source,destination,type,time;};
 std::vector<Link> links{{1,2,2,c.link},{2,1,5,c.link}};
 if(c.branch)links={{1,2,2,5},{1,3,2,10},{2,1,5,10},{3,2,2,10},{4,2,2,15}};
 aasworld.numareas=aasworld.numareasettings=count+1;
 aasworld.numclusters=2;aasworld.numportals=1;
 aasworld.areas=botlib::ClearedAlloc<aas_area_t>(count+1);
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(count+1);
 aasworld.clusters=botlib::ClearedAlloc<aas_cluster_t>(2);
 aasworld.portals=botlib::ClearedAlloc<aas_portal_t>(1);
 aasworld.clusters[1].numareas=aasworld.clusters[1].numreachabilityareas=count;
 aasworld.reachabilitysize=(int)links.size();
 aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(links.size());
 int next=0;
 for(int area=1;area<=count;++area){auto &s=aasworld.areasettings[area];
  s.cluster=1;s.clusterareanum=area-1;s.firstreachablearea=next;s.presencetype=PRESENCE_NORMAL;
  for(const auto &link:links)if(link.source==area){auto &r=aasworld.reachability[next++];
   ++s.numreachableareas;r.areanum=link.destination;r.traveltype=link.type;r.traveltime=link.time;}
 }
 aasworld.areasettings[2].contents=c.contents;aasworld.areasettings[2].areaflags=c.areaFlags;
 if(c.branch)aasworld.areasettings[3].areaflags=c.intermediateFlags;
 routingTables();
 // Supply generated matrices so the comparison isolates the cache update
 // from ET's separate geometry-to-cost calculation, which remains open.
 for(int area=1;area<=count;++area)for(int row=0;row<aasworld.areasettings[area].numreachableareas;++row)
  for(int i=0;i<aasworld.reversedreachability[area].numlinks;++i)
   aasworld.areatraveltimes[area][row][i]=area==2?c.inside:1;
 if(c.branch){const unsigned short goal[]={50,1,99};
  std::copy(std::begin(goal),std::end(goal),aasworld.areatraveltimes[2][0]);
  aasworld.areatraveltimes[3][0][0]=7;}
 if(c.missingSourceRow)aasworld.areatraveltimes[1]=nullptr;
 if(c.missingIntermediateRow)aasworld.areatraveltimes[3]=nullptr;
 aasworld.areasettings[1].cluster=c.sourceCluster;
 aasworld.areasettings[1].clusterareanum=c.sourceIndex;
 aasworld.areasettings[2].clusterareanum=c.goalIndex;
}
static aas_routingcache_t *update(const Case &c){
 const int count=c.branch?4:2;auto *cache=AAS_AllocRoutingCache(count);
 cache->cluster=1;cache->areanum=2;cache->starttraveltime=1;cache->travelflags=c.travelFlags;
 AAS_UpdateAreaRoutingCache(cache);return cache;
}
static void processSetup(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
#ifdef _WIN32
 SetErrorMode(0x0001|0x0002);
#endif
 for(const auto &p:profiles)if(p.id==COM_GAME_WOLFET)profile=&p;
 assert(profile&&Com_GameIsWolfET());
}
'''


def production():
    return routes.route_source() + routes.GRAPH + WORLD


def cpp_case(case):
    fields = ('areaFlags', 'travelFlags', 'contents', 'inside', 'link',
              'missingSourceRow', 'sourceCluster', 'sourceIndex', 'goalIndex',
              'branch', 'intermediateFlags', 'missingIntermediateRow')
    return '{' + json.dumps(case['name']) + ',' + ','.join(
        str(int(case[field])) for field in fields) + '}'


class WolfETAreaRoutingTests(unittest.TestCase):
    compile_and_run = routing.RTCWRoutingTests.compile_and_run

    def test_original_generated_cache_costs_choices_and_contents(self):
        fixture = json.loads(FIXTURE.read_text(encoding='utf-8'))
        self.assertEqual(fixture['retailSha256'],
                         '4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e')
        self.assertEqual(len(fixture['contentsFlags']), 128)
        checks = 'int main(){processSetup();\n'
        for values in fixture['cases']:
            case = dict(zip(fixture['columns'], values))
            checks += '{const Case c=' + cpp_case(case) + ';world(c);auto *cache=update(c);\n'
            checks += 'const int times[]={' + ','.join(map(str, case['times'])) + '};\n'
            checks += 'const int choices[]={' + ','.join(map(str, case['choices'])) + '};\n'
            checks += r'''
for(size_t i=0;i<std::size(times);++i){
 if(cache->traveltimes[i]!=times[i]||cache->reachabilities[i]!=choices[i]){
  fprintf(stderr,"%s entry %zu: %u/%u expected %d/%d\n",c.name,i,
   cache->traveltimes[i],cache->reachabilities[i],times[i],choices[i]);return 1;}}
'''
            checks += f'assert(aasworld.frameroutingupdates=={case["updates"]});\n'
            checks += 'AAS_FreeRoutingCache(cache);shutdown();}\n'
        checks += 'const int expected[]={' + ','.join(map(str, fixture['contentsFlags'])) + '};\n'
        checks += r'''
 initialize();aasworld.numareas=aasworld.numareasettings=2;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(2);
 const int bits[]={1,2,4,256,1024,2048,4096};
 for(int mask=0;mask<128;++mask){int contents=0;
  for(int bit=0;bit<7;++bit)if(mask&(1<<bit))contents|=bits[bit];
  for(int areaFlags:{0,8,16,2048,4096,6160}){
   aasworld.areasettings[1].contents=contents;aasworld.areasettings[1].areaflags=areaFlags;
   AAS_InitAreaContentsTravelFlags();
   assert(AAS_GetAreaContentsTravelFlags(1)==expected[mask]);
   assert(AAS_AreaContentsTravelFlags(1)==expected[mask]);
  }
 }shutdown();
 printf("42 original area caches and 768 contents combinations match\n");
}
'''
        self.compile_and_run(production() + checks, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_missing_goal_rows_partial_cleanup_and_saturated_counter(self):
        checks = r'''
int main(){processSetup();const Case c={"safety",0,0x80002,0,17,10,false,1,0,1,false,0,false};
 for(int failure=0;failure<4;++failure){world(c);
  if(failure==0)aasworld.areasettings[2].numreachableareas=0;
  if(failure==1){FreeMemory(aasworld.areatraveltimes);aasworld.areatraveltimes=nullptr;}
  if(failure==2)aasworld.areatraveltimes[2]=nullptr;
  if(failure==3)aasworld.areatraveltimes[2][0]=nullptr;
  auto *cache=update(c);assert(!cache->traveltimes[0]&&!cache->traveltimes[1]);
  assert(!aasworld.frameroutingupdates);AAS_FreeRoutingCache(cache);shutdown();
 }
 world(c);aasworld.areatraveltimes[1][0]=nullptr;
 auto *cache=update(c);assert(!cache->traveltimes[0]&&cache->traveltimes[1]==1);
 AAS_FreeRoutingCache(cache);shutdown();
 for(int failure=0;failure<4;++failure){world(c);
  if(failure==0)aasworld.areasettings[2].clusterareanum=-1;
  if(failure==1)aasworld.areasettings[1].clusterareanum=-1;
  if(failure==2)aasworld.areasettings[1].firstreachablearea=1;
  if(failure==3)aasworld.areasettings[1].numreachableareas=0;
  cache=update(c);assert(!cache->traveltimes[0]);
  AAS_FreeRoutingCache(cache);shutdown();
 }
 world(c);aasworld.frameroutingupdates=INT_MAX;
 cache=update(c);assert(cache->traveltimes[0]==28&&aasworld.frameroutingupdates==INT_MAX);
 AAS_FreeRoutingCache(cache);shutdown();
 assert(AAS_AreaTravelTimeToGoalArea(1,nullptr,2,0x80002)==0);
}
'''
        self.compile_and_run(production() + checks, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
