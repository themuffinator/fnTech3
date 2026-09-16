"""Execute the real RTCW cache loader, world ownership and partial cleanup."""
from pathlib import Path
import unittest
import rtcw_aas_tests
import rtcw_aas_load_tests
import rtcw_botlib_tests
import rtcw_routecache_tests
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def loader_source():
    source = rtcw_aas_load_tests.loader_source()
    route = (ROOT / 'code/botlib/be_aas_route.cpp').read_text()
    for signature in ('static ID_INLINE int AAS_ClusterAreaNum(', 'static aas_routingcache_t *AAS_AllocRoutingCache(',
                      'static void AAS_InitClusterAreaCache(', 'static void AAS_InitPortalCache('):
        source += function(route, signature) + '\n'
    source += '\n#include "code/botlib/rtcw_routecache_loader.hpp"\n'
    return source


COMPARE = r'''
namespace rc=fntech3::rtcw::routecache;
static uint64_t recordComparisons,entryComparisons,visibilityComparisons,waypointComparisons;
static void compare(const std::vector<byte> &data){
 rc::Signature signature{(uint32_t)aasworld.numareas,(uint32_t)aasworld.numclusters,{}};
 for(unsigned i=0;i<3;++i)signature.crc[i]=aasworld.rtcwNavigationCrc[i];
 rc::View view;assert(rc::Parse(data.data(),data.size(),Com_GameIsRTCWSP()?rc::Version::SP:rc::Version::MP,signature,view)==rc::Error::None);
 assert(rc::ValidateTopology(view,fntech3::rtcw::navigation::RouteTopology(aasworld))==rc::Error::None);
 assert(aasworld.rtcwRouteCacheLoaded&&!aasworld.rtcwLoadBytes&&!aasworld.rtcwLoadFile&&!fileOpen);
 auto *cache=aasworld.oldestcache;aas_routingcache_t *previous=nullptr;int size=0;
 for(const auto record:view.Caches()){
  assert(cache&&cache->time_prev==previous&&cache->type==(record.IsPortal()?CACHETYPE_PORTAL:CACHETYPE_AREA));
  assert(cache->size==int(sizeof(*cache)+record.Count()*3));size+=cache->size;
  assert(cache->time==record.Time()&&cache->starttraveltime==record.StartTravelTime());
  assert(cache->cluster==record.Cluster()&&cache->areanum==record.Area()&&cache->travelflags==record.TravelFlags());
  for(unsigned axis=0;axis<3;++axis)assert(cache->origin[axis]==record.Origin(axis));
  const auto *times=reinterpret_cast<const unsigned short *>(reinterpret_cast<const byte *>(cache)+offsetof(aas_routingcache_t,traveltimes));
  for(size_t i=0;i<record.Count();++i){assert(times[i]==record.TravelTime(i)&&cache->reachabilities[i]==record.Reachability(i));++entryComparisons;}
  auto *head=record.IsPortal()?aasworld.portalcache[cache->areanum]
   :aasworld.clusterareacache[cache->cluster][AAS_ClusterAreaNum(cache->cluster,cache->areanum)];
  assert(head&&!head->prev);bool found=false;
  for(auto *node=head;node;node=node->next){if(node==cache)found=true;if(node->next)assert(node->next->prev==node);}
  assert(found);previous=cache;cache=cache->time_next;++recordComparisons;
 }assert(!cache&&previous==aasworld.newestcache&&size==aasworld.routingcachesize);
 size_t area=0;for(const auto row:view.Visibility()){
  assert(aasworld.rtcwVisibility[area].bytes==(int)row.bytes);
  if(row.bytes)assert(!std::memcmp(aasworld.rtcwVisibility[area].data,row.data,row.bytes));
  else assert(!aasworld.rtcwVisibility[area].data);
  assert(aasworld.rtcwDecodedVisibility[area]==0);
  for(unsigned axis=0;axis<3;++axis)assert(aasworld.rtcwWaypoints[area][axis]==view.Waypoint(area,axis));
  ++area;++visibilityComparisons;++waypointComparisons;
 }assert(area==view.AreaCount()&&aasworld.rtcwVisibilityArea==-1);
}
'''


class RTCWRouteCacheLoadTests(unittest.TestCase):
    compile_and_run = rtcw_botlib_tests.RTCWBotLibTests.compile_and_run

    def test_native_conversion_reload_blocking_and_failure_cleanup(self):
        aas = rtcw_aas_tests.synthetic_aas()
        cache = rtcw_routecache_tests.synthetic_cache(aas)
        source = loader_source() + COMPARE
        source += '\nstatic const byte initialMap[]={' + ','.join(map(str, aas)) + '};\n'
        source += '\nstatic const byte initialCache[]={' + ','.join(map(str, cache)) + '};\n'
        source += r'''
static void loadMap(){
 files["maps/probe.aas"]={std::begin(initialMap),std::end(initialMap)};
 assert(AAS_LoadAASFile("maps/probe.aas")==0);Q_strncpyz(aasworld.mapname,"probe",sizeof(aasworld.mapname));
 files["maps/probe.rcd"]={std::begin(initialCache),std::end(initialCache)};
 if(!Com_GameIsRTCWSP())files["maps/probe.rcd"][4]=12;
}
int main(){
 for(const auto &p:profiles){profile=&p;initialize();
  assert(!AAS_ReadRTCWRouteCache()&&opened.empty());
  if(!Com_GameIsRTCW()){shutdown();continue;}
  for(int world=0;world<2;++world){assert(AAS_SetCurrentWorld(world));loadMap();
   assert(AAS_ReadRTCWRouteCache());compare(files.at("maps/probe.rcd"));
  }
  assert(aasWorlds[0].rtcwVisibilityData!=aasWorlds[1].rtcwVisibilityData);
  assert(aasWorlds[0].oldestcache!=aasWorlds[1].oldestcache);
  for(int world=0;world<2;++world){AAS_SetCurrentWorld(world);
   assert(AAS_ReadRTCWRouteCache());compare(files.at("maps/probe.rcd"));
   auto *old=aasworld.oldestcache;auto *vis=aasworld.rtcwVisibilityData;
   const auto good=files.at("maps/probe.rcd");
   auto unchanged=[&]{assert(!AAS_ReadRTCWRouteCache()&&aasworld.oldestcache==old
    &&aasworld.rtcwVisibilityData==vis&&aasworld.rtcwRouteCacheLoaded&&!fileOpen&&!aasworld.rtcwLoadBytes);};
   files.erase("maps/probe.rcd");unchanged();
   for(size_t n=0;n<good.size();++n){files["maps/probe.rcd"]={good.begin(),good.begin()+n};unchanged();}
   files["maps/probe.rcd"]=good;
   for(int size:{0,35,INT_MAX}){fileSizeOverride=size;unchanged();}fileSizeOverride=-1;
   for(int n:{0,35,120,(int)good.size()-1}){shortRead=n;unchanged();}shortRead=-1;
   for(unsigned byte:{0u,4u,8u,16u,20u,24u,36u,48u}){files["maps/probe.rcd"]=good;files["maps/probe.rcd"][byte]^=0xff;unchanged();}
   files["maps/probe.rcd"]=good;files["maps/probe.rcd"][36+55+52+4+1]=2;unchanged();
   files["maps/probe.rcd"]=good;
   // Blocking must invalidate loaded native cache links with correct accounting.
   auto *other=aasWorlds[1-world].oldestcache;assert(AAS_EnableRoutingArea(1,0)==1);
   assert(!aasworld.oldestcache&&!aasworld.newestcache&&aasworld.routingcachesize==0&&aasWorlds[1-world].oldestcache==other);
   assert(AAS_ReadRTCWRouteCache());compare(good);
  }
  // A real map replacement frees routing before replacing AAS geometry.
  files["maps/reload_b0.aas"]={std::begin(initialMap),std::end(initialMap)};
  assert(AAS_LoadMap("reload")==0);
  for(int world=0;world<2;++world){AAS_SetCurrentWorld(world);assert(!aasworld.rtcwVisibility&&!aasworld.rtcwRouteCacheLoaded&&!aasworld.oldestcache);}
  shutdown();
 }
 for(bool replacing:{false,true})for(int fail=0;fail<20;++fail){
  profile=&profiles[2];initialize();loadMap();if(replacing){assert(AAS_ReadRTCWRouteCache());}
  failAllocation=fail;bool failed=false;
  try{assert(AAS_ReadRTCWRouteCache());compare(files.at("maps/probe.rcd"));}catch(Drop&){failed=true;}
  failAllocation=-1;shutdown();if(!failed){assert(fail>=9);break;}
 }
}
'''
        self.compile_and_run(source,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
