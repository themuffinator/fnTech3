"""Execute world ownership, real AAS box queries, blocking and module bounds."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def navigation_source():
    source = bootstrap.PRELUDE[:bootstrap.PRELUDE.index('static int Export_BotLibSetup();')]
    source = source.replace('void *GetMemory(unsigned long size){',
        'static int failAllocation=-1;\nvoid *GetMemory(unsigned long size){'
        'if(failAllocation==0)throw Drop{};if(failAllocation>0)--failAllocation;')
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
    source += catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
    source += r'''
#include "code/botlib/aasfile.h"
#include "code/botlib/be_aas.h"
#include "code/botlib/be_aas_def.h"
#include "code/botlib/l_memory.h"
#include "code/botlib/rtcw_navigation.h"
int Com_GameIsRTCWSP(){return profile->id==COM_GAME_RTCW_SP;}
int botDeveloper;
botlib_import_t botimport;
botlib_export_t *botlib_export;
void *GetHunkMemory(unsigned long n){return GetMemory(n);}
void *GetClearedMemory(unsigned long n){return GetClearedHunkMemory(n);}
int SV_BotLibSetup(){assert(false);return 0;}
void BotInitAvoidReach(int){assert(false);}
server_t sv{};serverStatic_t svs{};
void SV_ClientThink(client_t*,usercmd_t*){assert(false);}
bool EA_ClientInputAvailable(int){assert(false);return false;}
unsigned EA_RTCWExtraInputFlags(int){assert(false);return 0;}
void EA_RestoreRTCWInput(int,const bot_input_t*,unsigned){assert(false);}
int SV_BotLibShutdown(){assert(false);return 0;}
static int bspDumps;
void AAS_DumpBSPData(){++bspDumps;}
void AAS_InvalidateEntities(){}
void AAS_DumpAASData(){
 if(aasworld.areasettings)FreeMemory(aasworld.areasettings);
 if(aasworld.nodes)FreeMemory(aasworld.nodes);
 if(aasworld.planes)FreeMemory(aasworld.planes);
}
static void QDECL print(int,const char*,...){}
struct aas_linkstack_t{int nodenum;};
#define ENABLE_ALTROUTING
'''
    main = (ROOT / 'code/botlib/be_aas_main.cpp').read_text()
    source += main[main.index('static aas_t aasWorlds['):main.index('//===========================================================================')]
    files = {
        'be_aas_sample.cpp': ('void AAS_InitAASLinkHeap(', 'void AAS_FreeAASLinkHeap(',
            'aas_link_t *AAS_AllocAASLink(', 'static void AAS_DeAllocAASLink(',
            'void AAS_InitAASLinkedEntities(', 'void AAS_FreeAASLinkedEntities(',
            'static int AAS_BoxOnPlaneSide2(', 'void AAS_UnlinkFromAreas(',
            'aas_link_t *AAS_AASLinkEntity(', 'int AAS_BBoxAreas('),
        'be_aas_route.cpp': ('static void AAS_UnlinkCache(', 'static void AAS_LinkCache(',
            'void AAS_FreeRoutingCache(', 'static void AAS_RemoveRoutingCacheInCluster(',
            'static void AAS_RemoveRoutingCacheUsingArea(', 'int AAS_EnableRoutingArea(',
            'void AAS_FreeAllClusterAreaCache(', 'void AAS_FreeAllPortalCache(',
            'void AAS_FreeRoutingCaches('),
        'be_aas_routealt.cpp': ('void AAS_InitAlternativeRouting(', 'void AAS_ShutdownAlternativeRouting('),
        'be_aas_main.cpp': ('int AAS_Setup(', 'static void AAS_ShutdownWorld(', 'void AAS_Shutdown('),
    }
    for filename, signatures in files.items():
        text = (ROOT / 'code/botlib' / filename).read_text()
        for signature in signatures:
            source += '\n' + function(text, signature)
    source += '\n#include "code/server/server_rtcw_botlib.hpp"\n'
    return source


CHECKS = r'''
using namespace fntech3::rtcw::module;
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
static void loadWorld(int index,float split){
 assert(AAS_SetCurrentWorld(index));
 aasworld.loaded=1;aasworld.numareas=aasworld.numareasettings=3;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(3);
 aasworld.numnodes=2;aasworld.nodes=botlib::ClearedAlloc<aas_node_t>(2);
 aasworld.numplanes=1;aasworld.planes=botlib::ClearedAlloc<aas_plane_t>(1);
 aasworld.nodes[1].children[0]=-1;aasworld.nodes[1].children[1]=-2;
 aasworld.planes[0].normal[0]=1;aasworld.planes[0].dist=split;
 AAS_InitAASLinkHeap();AAS_InitAASLinkedEntities();AAS_InitAlternativeRouting();
 aasworld.hidetraveltimes=botlib::ClearedAlloc<unsigned short>(3);
}
static bool enabled(int world,int area){
 assert(AAS_SetCurrentWorld(world));return AAS_EnableRoutingArea(area,-1)!=0;
}
static void checkWorlds(){
 for(const auto &p:profiles){profile=&p;const int count=p.bots.navigationWorlds;
  assert(count==(Com_GameIsRTCW()?2:1));assert(AAS_SetCurrentWorld(0));auto *primary=aas_currentWorld;
  assert(!AAS_SetCurrentWorld(-1)&&!AAS_SetCurrentWorld(count)&&aas_currentWorld==primary);
  vec3_t lo={1,-2,-3},hi={2,2,3},zero={};
  // Retail MP calls blocking with the library disabled: no allocation needed.
  AAS_SetBlockingEntity(lo,hi,1);assert(aas_currentWorld==&aasWorlds[count-1]&&allocations.empty());
  assert(AAS_Setup()==0&&aas_currentWorld==primary&&aasworld.entities);
  loadWorld(0,0);auto *firstLinks=aasworld.linkheap;auto *firstScratch=aasworld.midrangeareas;
  if(count==2){loadWorld(1,10);assert(aasworld.linkheap!=firstLinks&&aasworld.midrangeareas!=firstScratch);}
  AAS_SetBlockingEntity(lo,hi,1);assert(aas_currentWorld==&aasWorlds[count-1]);
  assert(!enabled(0,1)&&enabled(0,2));
  if(count==2)assert(enabled(1,1)&&!enabled(1,2));
  // Transient query links must not leak or remain attached to the areas.
  for(int i=0;i<count;++i){AAS_SetCurrentWorld(i);assert(aasworld.numaaslinks==aasworld.linkheapsize);
   assert(!aasworld.arealinkedentities[1]&&!aasworld.arealinkedentities[2]);}
  AAS_SetBlockingEntity(lo,hi,0);assert(enabled(0,1));
  AAS_SetBlockingEntity(lo,hi,2);assert(!enabled(0,1));
  // Original SP and MP intentionally have different reset sentinels.
  AAS_SetBlockingEntity(zero,zero,Com_GameIsRTCWSP()?-1:0);
  for(int i=0;i<count;++i)assert(enabled(i,1)&&enabled(i,2));
  assert(AAS_EnableRoutingArea(0,0)==0&&AAS_EnableRoutingArea(3,0)==0);
  // A loaded world may have no caches, one cache table, or both. Changing
  // an area invalidates only that world's actual cache entries.
  AAS_SetCurrentWorld(0);
  aasworld.numclusters=2;aas_cluster_t clusters[2]{};clusters[1].numareas=3;aasworld.clusters=clusters;
  aasworld.areasettings[1].cluster=1;
  auto *cache=botlib::ClearedAlloc<aas_routingcache_t>();cache->size=sizeof(*cache);
  aasworld.routingcachesize=cache->size;AAS_LinkCache(cache);
  auto *table=botlib::ClearedAlloc<aas_routingcache_t*>(3);table[1]=cache;aasworld.portalcache=table;
  assert(AAS_EnableRoutingArea(1,0)==1&&!table[1]&&aasworld.routingcachesize==0&&!aasworld.newestcache);
  AAS_FreeAllPortalCache();
  auto *clusterTable=botlib::ClearedAlloc<aas_routingcache_t**>(2);
  auto *entries=botlib::ClearedAlloc<aas_routingcache_t*>(3);clusterTable[1]=entries;
  aasworld.clusterareacache=clusterTable;cache=botlib::ClearedAlloc<aas_routingcache_t>();
  cache->size=sizeof(*cache);entries[1]=cache;aasworld.routingcachesize=cache->size;AAS_LinkCache(cache);
  assert(AAS_EnableRoutingArea(1,1)==0&&!entries[1]&&!aasworld.routingcachesize);
  // Real cluster-cache allocation is contiguous; dispose the synthetic split table here.
  FreeMemory(entries);FreeMemory(clusterTable);aasworld.clusterareacache=nullptr;aasworld.clusters=nullptr;
  int oldDumps=bspDumps;AAS_Shutdown();assert(bspDumps==oldDumps+1&&aas_currentWorld==primary);
  for(int i=0;i<count;++i){AAS_SetCurrentWorld(i);assert(!aasworld.loaded&&!aasworld.entities&&!aasworld.hidetraveltimes);}
  AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
  // A larger next map needs fresh scratch storage; no stale three-area buffer.
  assert(AAS_Setup()==0);loadWorld(0,0);AAS_FreeRoutingCaches();
  assert(!aasworld.hidetraveltimes);AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
  // Failed replacement allocations cannot leave freed pointers in a world.
  for(int fail=0;fail<3;++fail){
   assert(AAS_Setup()==0);loadWorld(0,0);failAllocation=fail;
   rejected([&]{AAS_InitAlternativeRouting();AAS_InitAASLinkedEntities();});
   failAllocation=-1;AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
  }
 }
}
static void checkArguments(){
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;
  for(bool qvm:{false,true}){
   std::array<byte,256> memory{};vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=memory.size();vm.dataMask=255;
   if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](size_t n){return qvm?(intptr_t)n:reinterpret_cast<intptr_t>(memory.data()+n);};
   auto invoke=[&](int ordinal,std::initializer_list<intptr_t> values){
    intptr_t args[5]{};std::copy(values.begin(),values.end(),args+1);args[0]=ordinal;intptr_t result=99;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,ordinal),args,result));assert(result==0);
   };
   assert(AAS_SetCurrentWorld(0));auto *primary=aas_currentWorld;
   invoke(307,{1});assert(aas_currentWorld!=primary);
   rejected([&]{invoke(307,{-1});});rejected([&]{invoke(307,{2});});
   const int ordinal=Com_GameIsRTCWSP()?324:323;
   const vec3_t lo={-1,-2,-3},hi={1,2,3};std::memcpy(memory.data()+1,lo,12);std::memcpy(memory.data()+17,hi,12);
   auto before=memory;invoke(ordinal,{address(1),address(17),1});assert(memory==before);
   invoke(ordinal,{address(1),address(1),-1});assert(memory==before);
   rejected([&]{invoke(ordinal,{address(17),address(1),1});});
   rejected([&]{invoke(ordinal,{0,address(17),1});});
   for(unsigned bits:{0x7f800000u,0xff800000u,0x7fc00001u}){
    std::memcpy(memory.data()+1,&bits,4);rejected([&]{invoke(ordinal,{address(1),address(17),1});});
   }
   memory=before;
   if(qvm){rejected([&]{invoke(ordinal,{address(250),address(17),1});});
    rejected([&]{invoke(ordinal,{257,address(17),1});});}
   if(sizeof(intptr_t)>4)rejected([&]{invoke(ordinal,{address(1),address(17),static_cast<intptr_t>(INT64_C(0x100000000))});});
   AAS_Shutdown();assert(allocations.empty());
  }
 }
}
int main(){botimport.Print=print;checkWorlds();checkArguments();}
'''


class RTCWNavigationTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_world_selection_blocking_cleanup_and_native_qvm_arguments(self):
        self.compile_and_run(navigation_source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
