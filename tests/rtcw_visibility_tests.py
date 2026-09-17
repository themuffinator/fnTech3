"""Execute RTCW visibility, fuzzy area selection and the SP route import."""
from pathlib import Path
import unittest
import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
STAND_IN = 'bool AAS_RTCWRouteFirstVisiblePosition(const float*,const float*,int,float*){assert(false);return false;}'


def visibility_source():
    source = routes.route_source().replace(STAND_IN, '')
    sample = (ROOT / 'code/botlib/be_aas_sample.cpp').read_text()
    start = sample.index('struct aas_tracestack_t')
    source += '\n' + sample[start:sample.index('};', start) + 2]
    for name, signatures in {
        'be_aas_sample.cpp': ('int AAS_PointAreaNum(', 'int AAS_TraceAreas('),
        'be_aas_reach.cpp': ('int AAS_AreaReachability(',),
        'be_ai_move.cpp': ('int BotFuzzyPointReachabilityArea(',),
        'be_aas_route.cpp': ('static int AAS_AreaVisible(', 'bool AAS_RTCWRouteFirstVisiblePosition('),
    }.items():
        text = (ROOT / 'code/botlib' / name).read_text()
        for signature in signatures:
            source += '\n' + function(text, signature)
    return source


GEOMETRY = r'''
// A sequence of planar slabs; the default places each graph center in its area.
static void slabs(float spacing=8){
 const int count=aasworld.numareas-1;assert(count>1);
 aasworld.numnodes=count;aasworld.numplanes=count-1;
 aasworld.nodes=botlib::ClearedAlloc<aas_node_t>(count);
 aasworld.planes=botlib::ClearedAlloc<aas_plane_t>(count-1);
 for(int a=1;a<count;++a){auto &node=aasworld.nodes[a];node.planenum=a-1;
  node.children[0]=a+1<count?a+1:-count;node.children[1]=-a;
  auto &plane=aasworld.planes[a-1];plane.normal[0]=1;plane.dist=spacing*(float(a)+.5f);}
}
static void visibility(unsigned seed=0){
 if(aasworld.rtcwVisibility)FreeMemory(aasworld.rtcwVisibility);
 if(aasworld.rtcwVisibilityData)FreeMemory(aasworld.rtcwVisibilityData);
 if(aasworld.rtcwDecodedVisibility)FreeMemory(aasworld.rtcwDecodedVisibility);
 int n=aasworld.numareas;aasworld.rtcwVisibility=botlib::ClearedAlloc<aas_rtcw_visibility_t>(n);
 aasworld.rtcwVisibilityData=botlib::ClearedAlloc<byte>(n*n*2);
 aasworld.rtcwDecodedVisibility=botlib::ClearedAlloc<byte>(n);
 for(int a=0;a<n;++a){byte *row=aasworld.rtcwVisibilityData+a*n*2;
  for(int b=0;b<n;++b){row[b*2]=seed?byte((a*17+b*11+seed)%7<2?255:0):0;row[b*2+1]=1;}
  if(!seed||a%5)aasworld.rtcwVisibility[a]={row,n*2};}
 aasworld.rtcwVisibilityArea=-1;aasworld.rtcwRouteCacheLoaded=qtrue;
}
static void visible(int from,int to,bool value){
 assert(aasworld.rtcwVisibility[from].data);
 aasworld.rtcwVisibilityData[from*aasworld.numareas*2+to*2]=value?255:0;
 aasworld.rtcwVisibilityArea=-1;
}
'''


class RTCWVisibilityTests(unittest.TestCase):
    compile_and_run = routing.RTCWRoutingTests.compile_and_run

    def run_source(self, checks):
        self.compile_and_run(visibility_source() + routes.GRAPH + GEOMETRY + checks,
                             routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_cached_rows_bounds_world_ownership_and_fuzzy_search(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;initialize();
  assert(!AAS_AreaVisible(0,0)&&!AAS_AreaVisible(-1,INT_MAX));
  for(int w=0;w<p.bots.navigationWorlds;++w){AAS_SetCurrentWorld(w);graph();slabs();visibility(w+1);
   for(int a=0;a<aasworld.numareas;++a)for(int b=0;b<aasworld.numareas;++b){
    int expected=Com_GameIsRTCW()&&a%5&&((a*17+b*11+w+1)%7<2);
    assert(AAS_AreaVisible(a,b)==expected);
   }
   if(Com_GameIsRTCW()){
    AAS_AreaVisible(1,2);int cached=aasworld.rtcwVisibilityArea;assert(cached==1);
    std::vector<byte> before(aasworld.rtcwDecodedVisibility,aasworld.rtcwDecodedVisibility+aasworld.numareas);
    assert(!AAS_AreaVisible(0,1)&&!AAS_AreaVisible(-1,1)&&!AAS_AreaVisible(1,aasworld.numareas));
    assert(aasworld.rtcwVisibilityArea==cached&&!std::memcmp(before.data(),aasworld.rtcwDecodedVisibility,before.size()));
    auto row=aasworld.rtcwVisibility[2];byte malformed[]={1,0,0,13};
    for(int bytes:{-1,0,1,2,3,4}){aasworld.rtcwVisibility[2]={malformed,bytes};
     assert(!AAS_AreaVisible(2,1)&&aasworld.rtcwVisibilityArea==1);
     assert(!std::memcmp(before.data(),aasworld.rtcwDecodedVisibility,before.size()));}
    aasworld.rtcwVisibility[2]=row;
    // A hit reuses the decoded row; an intervening missing row cannot discard it.
    int bit=before[2];aasworld.rtcwVisibilityData[aasworld.numareas*2+4]=bit?0:255;
    assert(AAS_AreaVisible(1,2)==bit);AAS_AreaVisible(2,1);assert(AAS_AreaVisible(1,2)==!bit);
    auto *decoded=aasworld.rtcwDecodedVisibility;aasworld.rtcwDecodedVisibility=nullptr;
    assert(!AAS_AreaVisible(1,2));aasworld.rtcwDecodedVisibility=decoded;
    visibility();visible(0,1,true);assert(AAS_AreaVisible(0,1)==1);
    aasworld.rtcwRouteCacheLoaded=qfalse;assert(!AAS_AreaVisible(0,1));aasworld.rtcwRouteCacheLoaded=qtrue;
   }
  }
  for(int w=0;w<p.bots.navigationWorlds;++w){AAS_SetCurrentWorld(w);
   assert(aasworld.rtcwVisibilityArea==(Com_GameIsRTCW()?0:-1));}
  shutdown();assert(!AAS_AreaVisible(1,2));
  initialize();graph(0,2,1);slabs();vec3_t point{};
  for(float distance:{12.0f,32.0f}){aasworld.planes[0].dist=distance;
   bool reaches=Com_GameIsRTCWSP()||(Com_GameIsRTCW()&&distance==12);
   aasworld.nodes[1].children[0]=-2;aasworld.nodes[1].children[1]=-1;
   aasworld.areasettings[1].numreachableareas=0;aasworld.areasettings[2].numreachableareas=1;
   assert(BotFuzzyPointReachabilityArea(point)==(reaches?2:1));
   aasworld.areasettings[2].numreachableareas=0;assert(BotFuzzyPointReachabilityArea(point)==1);
   aasworld.nodes[1].children[1]=0;assert(BotFuzzyPointReachabilityArea(point)==(reaches?2:0));
   aasworld.nodes[1].children[0]=0;assert(BotFuzzyPointReachabilityArea(point)==0);
  }
  shutdown();assert(BotFuzzyPointReachabilityArea(point)==0);
 }
}
''')

    def test_sp_native_qvm_buffers_real_routes_and_failure_publication(self):
        self.run_source(r'''
using namespace fntech3::rtcw::module;
int main(){
 botlib_export_t backend{};botlib_export=&backend;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCWSP()){
   intptr_t result=71;assert(!SV_RTCWAASQueryCall(nullptr,Call::BOTLIB_AAS_GETROUTEFIRSTVISPOS,nullptr,result)&&result==71);
   assert(!AAS_RTCWRouteFirstVisiblePosition(nullptr,nullptr,-1,nullptr));continue;}
  assert(Ordinal(p.id,Call::BOTLIB_AAS_GETROUTEFIRSTVISPOS)==323);
  assert(Ordinal(COM_GAME_RTCW_MP,Call::BOTLIB_AAS_GETROUTEFIRSTVISPOS)==-1);
  for(bool qvm:{false,true}){initialize();
   std::array<byte,128> memory;vm_t vm{};vm.dataBase=memory.data();vm.dataMask=127;vm.exactDataLength=128;
   if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](int at){return qvm?intptr_t(at):reinterpret_cast<intptr_t>(memory.data()+at);};
   auto invoke=[&](intptr_t a,intptr_t b,intptr_t flags,intptr_t out){intptr_t args[]={323,a,b,flags,out},result=73;
    assert(SV_RTCWBotLibCall(&vm,Call::BOTLIB_AAS_GETROUTEFIRSTVISPOS,args,result));return result;};
   auto prepare=[&](int a,int b){memory.fill(0xa5);vec3_t source{float(a*8),0,0},dest{float(b*8),0,0};
    std::memcpy(memory.data()+1,source,12);std::memcpy(memory.data()+33,dest,12);};
   auto reject=[&](intptr_t a,intptr_t b,intptr_t flags,intptr_t out){auto before=memory;
    try{invoke(a,b,flags,out);assert(false);}catch(Drop&){}assert(memory==before);};
   prepare(1,4);auto saved=memory;assert(!invoke(address(1),address(33),-1,address(116))&&memory==saved);
   for(int w:{0,1}){AAS_SetCurrentWorld(w);graph();slabs();visibility();}
   for(int w:{0,1,0}){AAS_SetCurrentWorld(w);auto *selected=aas_currentWorld;
    auto check=[&](int a,int b,int flags,bool found,const float *expected){prepare(a,b);auto before=memory;
     aasworld.frameroutingupdates=0;assert(invoke(address(1),address(33),flags,address(116))==found);
     if(found)std::memcpy(before.data()+116,expected,12);assert(memory==before&&aas_currentWorld==selected);};
    vec3_t start{8,0,0};check(1,1,-1,true,start);check(1,2,-1,true,start);
    // The 1 -> 4 route passes through portal area 10.
    visible(10,4,true);check(1,4,-1,true,aasworld.areas[10].center);
    visible(1,4,true);check(1,4,0,true,start);visible(1,4,false);
    check(1,4,0,false,nullptr);visible(10,4,false);
    check(1,4,-1,true,aasworld.areas[10].center);
    auto row=aasworld.rtcwVisibility[4];aasworld.rtcwVisibility[4]={};
    check(1,4,-1,false,nullptr);aasworld.rtcwVisibility[4]=row;
    // Caller outputs can overlap either input, at unaligned and exact-end addresses.
    visible(1,4,true);
    for(int out:{1,5,33,37,116}){prepare(1,4);auto before=memory;std::memcpy(before.data()+out,start,12);
     assert(invoke(address(1),address(33),-1,address(out))==1&&memory==before);}
    visible(1,4,false);
   }
   // Corrupt backend coordinates cannot escape through the module output.
   AAS_SetCurrentWorld(0);visible(10,4,true);int portalLink=-1;
   for(int i=0;i<aasworld.areasettings[1].numreachableareas;++i){
    int link=aasworld.areasettings[1].firstreachablearea+i;
    if(aasworld.reachability[link].areanum==10)portalLink=link;}
   assert(portalLink>=0);vec3_t entry;VectorCopy(aasworld.reachability[portalLink].end,entry);
   for(uint32_t bits:{0x7fc00001u,0x7f800000u,0x48000001u}){prepare(1,4);
    std::memcpy(aasworld.reachability[portalLink].end+1,&bits,4);
    reject(address(1),address(33),-1,address(116));}
   VectorCopy(entry,aasworld.reachability[portalLink].end);
   for(uint32_t bits:{0x7fc00001u,0x7f800000u,0xff800000u,0x48000001u,0xc8000001u})
   for(int at:{1,33})for(int axis=0;axis<3;++axis){prepare(1,4);std::memcpy(memory.data()+at+axis*4,&bits,4);
    reject(address(1),address(33),-1,address(116));}
   prepare(1,4);reject(0,address(33),-1,address(116));reject(address(1),0,-1,address(116));reject(address(1),address(33),-1,0);
   if(qvm)for(int bad:{-1,117,128}){reject(bad,address(33),-1,116);reject(1,bad,-1,116);reject(1,33,-1,bad);}
   if(sizeof(intptr_t)>4)reject(address(1),address(33),static_cast<intptr_t>(INT64_C(0x100000000)),address(116));
   botlib_export=nullptr;reject(address(1),address(33),-1,address(116));botlib_export=&backend;
   shutdown();prepare(1,4);saved=memory;assert(!invoke(address(1),address(33),-1,address(116))&&memory==saved);
  }
 }
}
''')

    def test_route_progress_limit_bad_links_and_partial_initialization(self):
        source = visibility_source() + routes.GRAPH + GEOMETRY + r'''
static int calls,stopAfter=200,mode;
static int ScriptFuzzy(vec3_t point){return int(point[0]);}
static int ScriptRoute(int area,vec3_t point,int goal,int flags,int *cost,int *link){
 assert(area==1||area==2);assert(goal==4&&flags==-1);++calls;
 if(mode==1)return 0;
 *cost=mode==2?100:1000-calls;*link=mode==3?INT_MAX:mode==4?-1:0;
 aasworld.reachability[0].areanum=calls==stopAfter?4:mode==5?aasworld.numareas:2;
 vec3_t next{2,float(calls),3};VectorCopy(next,aasworld.reachability[0].end);
 if(calls>1)assert(point[1]==calls-1);
 return 1;
}
#define AAS_RTCWRouteFirstVisiblePosition BoundedQuery
#define BotFuzzyPointReachabilityArea ScriptFuzzy
#define AAS_AreaRouteToGoalArea ScriptRoute
'''
        route = (ROOT / 'code/botlib/be_aas_route.cpp').read_text()
        source += function(route, 'bool AAS_RTCWRouteFirstVisiblePosition(') + r'''
#undef AAS_RTCWRouteFirstVisiblePosition
#undef BotFuzzyPointReachabilityArea
#undef AAS_AreaRouteToGoalArea
int main(){
 profile=&profiles[2];initialize();graph();visibility();
 vec3_t start{1,0,0},dest{4,0,0},out{71,72,73},unchanged{71,72,73};
 for(int end:{1,2,199,200,201}){stopAfter=end;calls=0;VectorCopy(unchanged,out);
  assert(BoundedQuery(start,dest,-1,out)==(end<=200));assert(calls==std::min(end,200));
  if(end==1)assert(!std::memcmp(out,start,12));
  else if(end<=200)assert(out[0]==2&&out[1]==end-1&&out[2]==3);
  else assert(!std::memcmp(out,unchanged,12));
 }
 stopAfter=200;
 for(mode=1;mode<=5;++mode){calls=0;VectorCopy(unchanged,out);
  assert(!BoundedQuery(start,dest,-1,out)&&!std::memcmp(out,unchanged,12));
  assert(calls==(mode==2?2:1));}
 mode=0;calls=0;aasworld.initialized=qfalse;
 assert(!BoundedQuery(start,start,-1,out)&&!calls);aasworld.initialized=qtrue;
 assert(!BoundedQuery(nullptr,dest,-1,out)&&!BoundedQuery(start,nullptr,-1,out));
 assert(!BoundedQuery(start,dest,-1,nullptr)&&!calls);
 for(uint32_t bits:{0x7fc00001u,0x7f800000u,0x48000001u}){
  vec3_t bad{1,0,0};std::memcpy(bad+1,&bits,4);assert(!BoundedQuery(bad,dest,-1,out)&&!calls);}
 visible(2,4,true);calls=0;stopAfter=200;
 assert(BoundedQuery(start,dest,-1,out)&&calls==1&&out[0]==2&&out[1]==1&&out[2]==3);
 shutdown();assert(!BoundedQuery(start,dest,-1,out));
}
'''
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
