"""Execute selected-world area traces with checked native and QVM buffers."""
from pathlib import Path
import unittest
import rtcw_aas_query_tests as queries
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def trace_source():
    source = queries.query_source()
    sample = (ROOT / 'code/botlib/be_aas_sample.cpp').read_text()
    start = sample.index('struct aas_tracestack_t')
    source += '\n' + sample[start:sample.index('};', start) + 2]
    return source + '\n' + function(sample, 'int AAS_TraceAreas(')


CHECKS = r'''
static int calls,fault;
static int trace(vec3_t start,vec3_t end,int *areas,vec3_t *points,int capacity){
 ++calls;int result=AAS_TraceAreas(start,end,areas,points,capacity);
 if(fault==1)return -1;if(fault==2)return capacity+1;
 if(fault==3)areas[0]=0;if(fault==4)areas[0]=65535;
 if(fault>=5&&points){unsigned bits=fault==5?0x7fc00001u:0x7f800000u;std::memcpy(&points[0][1],&bits,4);}
 return result;
}
int main(){
 botimport.Print=print;botlib_export_t backend{};backend.aas.AAS_TraceAreas=trace;botlib_export=&backend;
 for(const auto &p:profiles){profile=&p;
  for(int capacity:{0,-1,INT_MIN})assert(AAS_TraceAreas(nullptr,nullptr,nullptr,nullptr,capacity)==0);
  if(!Com_GameIsRTCW()){
   intptr_t result=79;assert(!SV_RTCWAASQueryCall(nullptr,Call::BOTLIB_AAS_TRACE_AREAS,nullptr,result)&&result==79);
   if(!p.bots.navigationWorlds)continue; // IHV has no retail AAS world.
   loadWorld(0,0);vec3_t start{-20,3,7},end{20,9,11},points[3]{};int areas[3]{-1,-1,-1};
   assert(AAS_TraceAreas(start,end,areas,points,3)==2&&areas[0]==2&&areas[1]==1&&areas[2]==-1);
   assert(points[0][0]==-20&&points[1][0]==0&&points[1][1]==6&&points[1][2]==9);
   AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());continue;
  }
  for(bool qvm:{false,true}){
   std::array<byte,1024> memory;vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=memory.size();vm.dataMask=1023;
   if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](size_t at){return qvm?intptr_t(at):reinterpret_cast<intptr_t>(memory.data()+at);};
   auto invoke=[&](std::initializer_list<intptr_t> values){
    intptr_t args[6]{309};std::copy(values.begin(),values.end(),args+1);intptr_t result=77;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,309),args,result));return result;
   };
   auto prepare=[&]{memory.fill(0xa5);vec3_t start{-20,3,7},end{20,9,11};
    std::memcpy(memory.data()+1,start,12);std::memcpy(memory.data()+33,end,12);fault=0;};
   auto failure=[&](std::initializer_list<intptr_t> values,bool backendCalled=false){
    auto before=memory;int previous=calls;rejected([&]{invoke(values);});
    assert(memory==before&&calls==previous+backendCalled);
   };
   prepare();auto before=memory;int previous=calls;assert(invoke({0,0,0,0,0})==0&&calls==previous&&memory==before);
   // No loaded graph still initializes areas[0], leaving points untouched.
   assert(AAS_SetCurrentWorld(0));assert(invoke({address(1),address(33),address(129),address(257),3})==0);
   int zero=0;std::memcpy(before.data()+129,&zero,4);assert(memory==before);
   loadWorld(0,0);loadWorld(1,10);
   for(int world:{0,1,0})for(int capacity:{1,2,3})for(bool withPoints:{false,true}){
    AAS_SetCurrentWorld(world);prepare();auto expected=memory;auto *selected=aas_currentWorld;
    assert(invoke({address(1),address(33),address(129),withPoints?address(257):0,capacity})==std::min(capacity,2));
    int area2=2,area1=1;vec3_t start{-20,3,7},split{float(world*10),world?7.5f:6.0f,world?10.0f:9.0f};
    std::memcpy(expected.data()+129,&area2,4);if(withPoints)std::memcpy(expected.data()+257,start,12);
    if(capacity>1){std::memcpy(expected.data()+133,&area1,4);if(withPoints)std::memcpy(expected.data()+269,split,12);}
    assert(memory==expected&&aas_currentWorld==selected);
   }
   AAS_SetCurrentWorld(0);
   // Opposite-facing planes, exact plane endpoints and solid leaves.
   for(int direction:{-1,1})for(float from:{-20.0f,0.0f,20.0f})for(float to:{-20.0f,0.0f,20.0f}){
    prepare();aasworld.planes[0].normal[0]=float(direction);
    vec3_t start{from,3,7},end{to,9,11};std::memcpy(memory.data()+1,start,12);std::memcpy(memory.data()+33,end,12);
    int expectedAreas[4]={-5,-5,-5,-5};vec3_t expectedPoints[4]{};
    int n=AAS_TraceAreas(start,end,expectedAreas,expectedPoints,4);
    assert(invoke({address(1),address(33),address(129),address(257),4})==n);
    assert(!std::memcmp(memory.data()+129,expectedAreas,std::max(1,n)*4));
    assert(!std::memcmp(memory.data()+257,expectedPoints,n*12));
   }
   aasworld.planes[0].normal[0]=1;aasworld.nodes[1].children[0]=aasworld.nodes[1].children[1]=0;
   prepare();before=memory;assert(invoke({address(1),address(33),address(129),address(257),3})==0);
   std::memcpy(before.data()+129,&zero,4);assert(memory==before);
   aasworld.nodes[1].children[0]=-1;aasworld.nodes[1].children[1]=-2;
   // Owned inputs and per-hit publishing support unaligned overlapping arrays.
   for(size_t areas:{size_t(1),size_t(129),size_t(257)})for(size_t points:{areas,areas+1,areas+4}){
    prepare();auto expected=memory;int area2=2,area1=1;vec3_t start{-20,3,7},split{0,6,9};
    std::memcpy(expected.data()+areas,&area2,4);std::memcpy(expected.data()+points,start,12);
    std::memcpy(expected.data()+areas+4,&area1,4);std::memcpy(expected.data()+points+12,split,12);
    assert(invoke({address(1),address(33),address(areas),address(points),3})==2&&memory==expected);
   }
   prepare();failure({address(1),address(33),0,address(257),3});
   failure({0,address(33),address(129),address(257),3});failure({address(1),0,address(129),address(257),3});
   failure({0,0,0,0,-1});failure({0,0,0,0,65536});failure({0,0,0,0,INT_MAX});
   for(unsigned bits:{0x7f800000u,0xff800000u,0x7fc00001u})for(size_t at:{size_t(1),size_t(33)})for(int axis=0;axis<3;++axis){
    prepare();std::memcpy(memory.data()+at+4*axis,&bits,4);failure({address(1),address(33),address(129),address(257),3});
   }
   prepare();assert(invoke({address(1),address(33),address(1016),address(976),2})==2);
   prepare();assert(invoke({address(1),address(33),address(129),address(1000),2})==2);
   if(qvm){prepare();failure({address(1),address(33),address(1017),address(257),2});
    failure({address(1),address(33),address(129),address(1001),2});
    failure({address(1013),address(33),address(129),address(257),2});
    failure({address(1),-1,address(129),address(257),2});
    failure({address(1),address(33),1025,address(257),2});}
   else {
    failure({intptr_t(-4),address(33),address(129),address(257),3});
   }
   if(sizeof(intptr_t)>4)failure({0,0,0,0,intptr_t(INT64_C(0x100000000))});
   // All backend outputs are validated before the caller sees any writes.
   for(int kind=1;kind<=6;++kind){prepare();fault=kind;failure({address(1),address(33),address(129),address(257),3},true);}
   prepare();botlib_export=nullptr;failure({address(1),address(33),address(129),address(257),3});botlib_export=&backend;
   AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
   assert(invoke({address(1),address(33),address(129),0,3})==0);
  }
 }
}
'''


class RTCWAASTraceTests(unittest.TestCase):
    compile_and_run = queries.RTCWAASQueryTests.compile_and_run

    def test_trace_native_qvm_worlds_aliasing_limits_and_shutdown(self):
        self.compile_and_run(trace_source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])

    def test_nonlocal_engine_errors_release_trace_scratch_before_drop(self):
        source = '#include <csetjmp>\nstatic std::jmp_buf errorJump;\n' + trace_source()
        source = source.replace('void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}',
            'void QDECL Com_Error(errorParm_t,const char*,...){std::longjmp(errorJump,1);}')
        source = source.replace('template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}',
            'template<class F> void rejected(F f){if(!setjmp(errorJump)){f();assert(false);}}')
        self.assertIn('std::longjmp(errorJump,1)', source)
        self.assertIn('if(!setjmp(errorJump))', source)
        self.compile_and_run(source + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
