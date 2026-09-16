"""Execute shared BSP getters and RTCW native/QVM import boundaries."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
SIGNATURES = ('int AAS_NextBSPEntity(', 'static int AAS_BSPEntityInRange(',
              'int AAS_ValueForBSPEpairKey(', 'int AAS_VectorForBSPEpairKey(',
              'int AAS_FloatForBSPEpairKey(', 'int AAS_IntForBSPEpairKey(')


def bsp_source():
    source = bootstrap.PRELUDE[:bootstrap.PRELUDE.index('static int Export_BotLibSetup();')]
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
    source += catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
    source += r'''
#include "code/compat/rtcw_bsp_query.hpp"
static constexpr int MAX_EPAIRKEY=128;
botlib_import_t botimport;
static int errors;
static void QDECL print(int kind,const char*,...){if(kind==PRT_ERROR)++errors;}
'''
    text = (ROOT / 'code/botlib/be_aas_bspq3.cpp').read_text()
    source += text[text.index('static constexpr int MAX_BSPENTITIES'):text.index('#ifdef BSP_DEBUG')]
    math = (ROOT / 'code/qcommon/q_math.c').read_text()
    source += function(math, 'static int Q_isfinite(') + function(math, 'float Q_atof(')
    source += function((ROOT / 'code/qcommon/q_shared.c').read_text(), 'int Com_Split(')
    for signature in SIGNATURES:
        source += function(text, signature)
    return source


CHECKS = r'''
static botlib_export_t exports;
botlib_export_t *botlib_export=&exports;
#include "code/server/server_rtcw_bsp_query.hpp"
using namespace fntech3::rtcw::module;
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
static void setValue(const char *value){
 static char key[]="property";static bsp_epair_t pair;
 bspworld={};bspworld.numentities=3;pair={key,const_cast<char*>(value),nullptr};bspworld.entities[1].epairs=&pair;
}
static void checkGetters(){
 for(const auto &p:profiles){profile=&p;setValue("alpha");
  for(int ent:{INT_MIN,-2,-1,2,3,INT_MAX})assert(AAS_NextBSPEntity(ent)==0);
  assert(AAS_NextBSPEntity(0)==1&&AAS_NextBSPEntity(1)==2);
  for(int capacity:{1,2,5,6,7,128,2048}){
   std::vector<char> actual(capacity+2,'Z');assert(AAS_ValueForBSPEpairKey(1,"property",actual.data()+1,capacity));
   assert(actual.front()=='Z'&&actual.back()=='Z');const int text=std::min(5,capacity-1);
   assert(!std::memcmp(actual.data()+1,"alpha",text)&&actual[text+1]==0);
   for(int i=text+2;i<=capacity;++i)assert(actual[i]==(Com_GameIsRTCW()?0:'Z'));
  }
  for(int ent:{INT_MIN,0,1,2,3,INT_MAX}){
   char out[8]={'Z','Z','Z','Z','Z','Z','Z','Z'};
   assert(!AAS_ValueForBSPEpairKey(ent,"missing",out+1,6)&&!out[1]&&out[0]=='Z'&&out[2]=='Z'&&out[7]=='Z');
  }
  char guard='Z';for(int capacity:{-1,0})assert(!AAS_ValueForBSPEpairKey(1,"property",&guard,capacity)&&guard=='Z');
  assert(!AAS_ValueForBSPEpairKey(1,"property",nullptr,5));
  assert(!AAS_ValueForBSPEpairKey(1,"Property",&guard,1));
  vec3_t value;setValue("1\t2\n3");assert(AAS_VectorForBSPEpairKey(1,"property",value));
  assert(value[0]==1&&value[1]==(Com_GameIsRTCW()?2:0)&&value[2]==(Com_GameIsRTCW()?3:0));
  setValue("1 wrong 3");assert(AAS_VectorForBSPEpairKey(1,"property",value));
  assert(value[0]==1&&value[1]==0&&value[2]==(Com_GameIsRTCW()?0:3));
  setValue("");assert(AAS_VectorForBSPEpairKey(1,"property",value)&&value[0]==0&&value[1]==0&&value[2]==0);
  setValue("1 2 3");assert(!AAS_VectorForBSPEpairKey(1,"missing",value)&&value[0]==0&&value[1]==0&&value[2]==0);
  std::string longText(127,' ');longText+="1 2 3";setValue(longText.c_str());
  assert(AAS_VectorForBSPEpairKey(1,"property",value)&&value[0]==0&&value[1]==0&&value[2]==0);
  float scalar;int integer;setValue("-12.5suffix");
  assert(AAS_FloatForBSPEpairKey(1,"property",&scalar)&&scalar==-12.5f);
  assert(AAS_IntForBSPEpairKey(1,"property",&integer)&&integer==-12);
  for(const char *text:{"", "junk", "+ 9"}){setValue(text);
   assert(AAS_FloatForBSPEpairKey(1,"property",&scalar)&&scalar==0);
   assert(AAS_IntForBSPEpairKey(1,"property",&integer)&&integer==0);
  }
  if(Com_GameIsRTCW()){
   for(const char *text:{"nan","inf","-inf","1e309","-1e309","1e39"}){setValue(text);
    scalar=99;assert(!AAS_FloatForBSPEpairKey(1,"property",&scalar)&&scalar==0);
    assert(!AAS_VectorForBSPEpairKey(1,"property",value)&&value[0]==0&&value[1]==0&&value[2]==0);
   }
   for(const char *text:{"2147483648","-2147483649","9999999999999999999999999999"}){setValue(text);
    integer=99;assert(!AAS_IntForBSPEpairKey(1,"property",&integer)&&integer==0);
   }
   setValue("-0");assert(AAS_FloatForBSPEpairKey(1,"property",&scalar));
   uint32_t bits;std::memcpy(&bits,&scalar,4);assert(bits==0x80000000u);
   assert(AAS_VectorForBSPEpairKey(1,"property",value));std::memcpy(&bits,value,4);assert(bits==0x80000000u);
   setValue("2147483647tail");assert(AAS_IntForBSPEpairKey(1,"property",&integer)&&integer==INT_MAX);
   setValue("-2147483648");assert(AAS_IntForBSPEpairKey(1,"property",&integer)&&integer==INT_MIN);
  }
  bspworld={};assert(!AAS_NextBSPEntity(0));
 }
}
static void checkBoundary(){
 exports.aas.AAS_NextBSPEntity=AAS_NextBSPEntity;exports.aas.AAS_ValueForBSPEpairKey=AAS_ValueForBSPEpairKey;
 exports.aas.AAS_VectorForBSPEpairKey=AAS_VectorForBSPEpairKey;
 exports.aas.AAS_FloatForBSPEpairKey=AAS_FloatForBSPEpairKey;exports.aas.AAS_IntForBSPEpairKey=AAS_IntForBSPEpairKey;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()){intptr_t result=99;assert(!SV_RTCWBSPQueryCall(nullptr,Call::BOTLIB_AAS_NEXT_BSP_ENTITY,nullptr,result)&&result==99);continue;}
  for(bool qvm:{false,true}){
   std::array<byte,4096> memory;memory.fill(0x55);vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=memory.size();vm.dataMask=4095;
   if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](size_t n){return qvm?(intptr_t)n:reinterpret_cast<intptr_t>(memory.data()+n);};
   auto put=[&](const char *key){std::strcpy(reinterpret_cast<char*>(memory.data()+1),key);};
   auto invoke=[&](int ordinal,std::initializer_list<intptr_t> values){
    // Exact argument allocation catches accidental reads beyond the contract.
    std::vector<intptr_t> args(1+values.size());args[0]=ordinal;std::copy(values.begin(),values.end(),args.begin()+1);
    intptr_t result=-99;assert(SV_RTCWBSPQueryCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,ordinal),args.data(),result));return result;
   };
   setValue("1\t-2\n3.5");put("property");assert(invoke(311,{0})==1&&invoke(311,{INT_MAX})==0);
   assert(invoke(312,{1,address(1),address(129),2048})==1&&memory[128]==0x55&&memory[2177]==0x55);
   for(size_t i=139;i<2177;++i)assert(!memory[i]);
   assert(invoke(312,{1,address(1),0,0})==0);
   auto before=memory;rejected([&]{invoke(312,{1,address(1),address(129),-1});});assert(memory==before);
   assert(invoke(313,{1,address(1),address(129)})==1);float values[3];std::memcpy(values,memory.data()+129,12);
   assert(values[0]==1&&values[1]==-2&&values[2]==3.5f);
   assert(invoke(314,{1,address(1),address(129)})==1);float scalar;std::memcpy(&scalar,memory.data()+129,4);assert(scalar==1);
   assert(invoke(315,{1,address(1),address(129)})==1);int integer;std::memcpy(&integer,memory.data()+129,4);assert(integer==1);
   put("missing");memory.fill(0x55);put("missing");
   assert(!invoke(312,{1,address(1),address(129),20})&&!memory[129]&&memory[130]==0x55);
   assert(!invoke(313,{1,address(1),address(129)}));for(int i=129;i<141;++i)assert(!memory[i]);
   assert(memory[128]==0x55&&memory[141]==0x55);
   put("property");assert(invoke(312,{1,address(1),address(1),32})==1);
   assert(!std::strcmp(reinterpret_cast<char*>(memory.data()+1),"1\t-2\n3.5"));
   put("property");assert(invoke(313,{1,address(1),address(1)})==1);
   std::memcpy(values,memory.data()+1,12);assert(values[0]==1&&values[1]==-2&&values[2]==3.5f);
   put("property");before=memory;
   for(int ordinal:{312,313,314,315}){
    if(ordinal==312)rejected([&]{invoke(ordinal,{1,address(1),0,12});});
    else rejected([&]{invoke(ordinal,{1,address(1),0});});
   }
   assert(memory==before);
   rejected([&]{invoke(313,{1,0,address(129)});});
   std::fill_n(memory.data()+1024,1024,'a');rejected([&]{invoke(313,{1,address(1024),address(129)});});
   if(qvm){
    for(int ordinal:{313,314,315})rejected([&]{invoke(ordinal,{1,address(1),4093});});
    rejected([&]{invoke(312,{1,address(1),4093,4});});
    rejected([&]{invoke(313,{1,4097,address(129)});});
   }
   if(sizeof(intptr_t)>4){rejected([&]{invoke(311,{static_cast<intptr_t>(INT64_C(0x100000000))});});}
   botlib_export=nullptr;rejected([&]{invoke(311,{0});});botlib_export=&exports;
  }
 }
}
int main(){botimport.Print=print;checkGetters();checkBoundary();assert(errors>0);}
'''


class RTCWBSPQueryTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_property_semantics_profile_isolation_and_exact_module_extents(self):
        self.compile_and_run(bsp_source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
