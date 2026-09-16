"""Exercise the actual selected-world AAS queries through native/QVM imports."""
from pathlib import Path
import unittest
import rtcw_navigation_tests as navigation
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def query_source():
    source = navigation.navigation_source()
    source += r'''
static int errors, contents, contentsCalls;
static vec3_t contentsPoint;
void QDECL AAS_Error(const char*,...){++errors;}
static int pointContents(vec3_t p){++contentsCalls;VectorCopy(p,contentsPoint);return contents;}
'''
    for name, signatures in {
        'be_aas_sample.cpp': ('void AAS_PresenceTypeBoundingBox(', 'int AAS_PointAreaNum('),
        'be_aas_bspq3.cpp': ('int AAS_PointContents(',),
        'be_aas_reach.cpp': ('int AAS_AreaReachability(',),
        'be_aas_move.cpp': ('int AAS_Swimming(',),
    }.items():
        text = (ROOT / 'code/botlib' / name).read_text()
        for signature in signatures:
            source += '\n' + function(text, signature)
    source += navigation.CHECKS[:navigation.CHECKS.index('static bool enabled(')]
    return source


CHECKS = r'''
static int backendCalls;
static int area(vec3_t p){++backendCalls;return AAS_PointAreaNum(p);}
static int reach(int a){++backendCalls;return AAS_AreaReachability(a);}
static int point(vec3_t p){++backendCalls;return AAS_PointContents(p);}
static int swimming(vec3_t p){++backendCalls;return AAS_Swimming(p);}
static void bounds(int p,vec3_t lo,vec3_t hi){++backendCalls;AAS_PresenceTypeBoundingBox(p,lo,hi);}
static void checkImports(){
 botlib_export_t backend{};botlib_export=&backend;
 backend.aas.AAS_PointAreaNum=area;backend.aas.AAS_PointContents=point;
 backend.aas.AAS_AreaReachability=reach;backend.aas.AAS_Swimming=swimming;
 backend.aas.AAS_PresenceTypeBoundingBox=bounds;botimport.PointContents=pointContents;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()){
   for(Call c:{Call::BOTLIB_AAS_POINT_AREA_NUM,Call::BOTLIB_AAS_POINT_CONTENTS,
       Call::BOTLIB_AAS_AREA_REACHABILITY,Call::BOTLIB_AAS_SWIMMING,Call::BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX}){
    intptr_t result=71;int calls=backendCalls;
    assert(!SV_RTCWBotLibCall(nullptr,c,nullptr,result)&&result==71&&backendCalls==calls);
   }continue;
  }
  for(bool qvm:{false,true}){
   std::array<byte,128> memory{};vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=memory.size();vm.dataMask=127;
   if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](size_t n){return qvm?(intptr_t)n:reinterpret_cast<intptr_t>(memory.data()+n);};
   auto invoke=[&](int ordinal,std::initializer_list<intptr_t> values){
    intptr_t args[6]{};std::copy(values.begin(),values.end(),args+1);args[0]=ordinal;intptr_t result=99;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,ordinal),args,result));return result;
   };
   auto failure=[&](int ordinal,std::initializer_list<intptr_t> values){
    auto saved=memory;int calls=backendCalls;rejected([&]{invoke(ordinal,values);});
    assert(memory==saved&&backendCalls==calls);
   };
   const vec3_t v={5,7,9};std::memcpy(memory.data()+1,v,sizeof(v));
   assert(AAS_SetCurrentWorld(0));assert(invoke(308,{address(1)})==0);
   assert(invoke(316,{0})==0); // no graph yet
   loadWorld(0,0);aasworld.areasettings[1].numreachableareas=7;
   loadWorld(1,10);aasworld.areasettings[2].numreachableareas=19;
   auto *selected=aas_currentWorld;
   assert(invoke(308,{address(1)})==2&&invoke(316,{2})==19&&aas_currentWorld==selected);
   AAS_SetCurrentWorld(0);selected=aas_currentWorld;
   assert(invoke(308,{address(1)})==1&&invoke(316,{1})==7&&aas_currentWorld==selected);
   // Coplanar points use the back child; negative-facing planes use their normal.
   float zero=0;std::memcpy(memory.data()+1,&zero,4);assert(invoke(308,{address(1)})==2);
   std::memcpy(memory.data()+1,v,sizeof(v));aasworld.planes[0].normal[0]=-1;
   assert(invoke(308,{address(1)})==2);aasworld.nodes[1].children[1]=0;
   assert(invoke(308,{address(1)})==0);
   for(int a:{-1,3,INT_MAX,INT_MIN})assert(invoke(316,{a})==0);
   for(unsigned bit=0;bit<32;++bit){
    contents=static_cast<int>(1u<<bit);auto before=memory;contentsCalls=0;
    assert(invoke(310,{address(1)})==contents&&contentsCalls==1&&memory==before);
    assert(!std::memcmp(contentsPoint,v,sizeof(v)));
    assert(invoke(318,{address(1)})==((contents&(CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA))!=0));
    assert(contentsPoint[0]==5&&contentsPoint[1]==7&&contentsPoint[2]==7&&memory==before);
   }
   for(int presence:{2,4,0,3,-1,INT_MIN,INT_MAX}){
    memory.fill(0xa5);assert(invoke(305,{presence,address(1),address(17)})==0);
    vec3_t lo,hi;std::memcpy(lo,memory.data()+1,12);std::memcpy(hi,memory.data()+17,12);
    assert(lo[0]==-15&&lo[1]==-15&&lo[2]==-24);
    assert(hi[0]==15&&hi[1]==15&&hi[2]==(presence==2?32:8));
    for(size_t i=0;i<memory.size();++i)if(!(i>=1&&i<13)&&!(i>=17&&i<29))assert(memory[i]==0xa5);
   }
   // Both outputs are validated before publishing either; overlap retains write order.
   assert(invoke(305,{2,address(1),address(1)})==0);
   vec3_t hi;std::memcpy(hi,memory.data()+1,12);assert(hi[2]==32);
   failure(305,{2,0,address(17)});failure(305,{2,address(1),0});
   for(int call:{308,310,318}){
    std::memcpy(memory.data()+1,v,12);failure(call,{0});
    for(unsigned bits:{0x7f800000u,0xff800000u,0x7fc00001u})for(int axis=0;axis<3;++axis){
     std::memcpy(memory.data()+1,v,12);std::memcpy(memory.data()+1+4*axis,&bits,4);failure(call,{address(1)});
    }
    std::memcpy(memory.data()+116,v,12);invoke(call,{address(116)});
    if(qvm){failure(call,{address(117)});failure(call,{129});failure(call,{-1});}
   }
   if(qvm){failure(305,{2,address(1),address(117)});failure(305,{2,address(117),address(17)});}
   if(sizeof(intptr_t)>4){const intptr_t wide=static_cast<intptr_t>(INT64_C(0x100000000));
    failure(305,{wide,address(1),address(17)});failure(316,{wide});}
   botlib_export=nullptr;
   for(int call:{305,308,310,316,318})failure(call,{1,address(1),address(17)});
   botlib_export=&backend;AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
   std::memcpy(memory.data()+1,v,12);assert(invoke(308,{address(1)})==0&&invoke(316,{0})==0);
  }
 }
}
int main(){botimport.Print=print;checkImports();}
'''


class RTCWAASQueryTests(unittest.TestCase):
    compile_and_run = navigation.RTCWNavigationTests.compile_and_run

    def test_scalar_queries_and_bounds_native_qvm_selected_world(self):
        self.compile_and_run(query_source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
