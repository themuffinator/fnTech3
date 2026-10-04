"""Exercise RTCW's server-to-cgame tag callback with the real VM export path."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as compiler
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/vm_local.h"
#include "code/game/g_public.h"
#include "code/ui/ui_public.h"
#include "code/cgame/cg_public.h"
#include "code/renderercommon/tr_types.h"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/wolfet_module_abi.h"
#include "code/compat/rtcw_game_abi.h"
#include "code/compat/rtcw_key_adapter.h"
#include "code/compat/wolfet_key_adapter.h"
#include "code/compat/wolfet_menu_adapter.h"
#include "code/compat/jka_cgame_exports.h"
#include "code/compat/jka_ui_exports.h"
#include "code/server/server_rtcw_args.hpp"
#include <cassert>
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile(){return &profile;}
int Com_GameIsWolfET(void) { return profile.id == COM_GAME_WOLFET; }
int Com_GameIsJKAMP(void) { return profile.id == COM_GAME_JKA_MP; }
int Com_GameIsJKASP(void) { return profile.id == COM_GAME_JKA_SP; }
int Com_GameIsRTCW(){return profile.id==COM_GAME_RTCW_SP||profile.id==COM_GAME_RTCW_MP;}
void QDECL Com_Error(errorParm_t,const char*,...){throw 7;}
static cvar_t dedicated;
cvar_t *com_dedicated=&dedicated;
static vm_t game,client;
static vm_t *cgvm=&client;
static intptr_t SV_RTCWGetTag(const vm_t*,intptr_t,intptr_t,intptr_t);
static int calls,expectedClient,returnValue=1;
static bool nested,preserve,poison;
static int poisonField;
static unsigned poisonBits;
static const char *expectedName;
static const void *sourceName,*sourceOutput;
static fntech3_rtcw_orientation_t expectedInput,answer;
static intptr_t QDECL Native(int call,...){
 assert(call==9&&client.callLevel==(nested?2:1));
 va_list ap;va_start(ap,call);int number=va_arg(ap,int32_t);
 auto *name=reinterpret_cast<char*>(static_cast<uintptr_t>(static_cast<uint32_t>(va_arg(ap,int32_t))));
 auto *out=reinterpret_cast<fntech3_rtcw_orientation_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(va_arg(ap,int32_t))));
 for(int n=3;n<15;++n)assert(va_arg(ap,int32_t)==0);va_end(ap);
 assert(number==expectedClient&&!std::strcmp(name,expectedName)&&name!=sourceName&&out!=sourceOutput);
 assert(!std::memcmp(out,&expectedInput,sizeof(*out))&&reinterpret_cast<uintptr_t>(out)%4==0);
 ++calls;++client.syscallCount;
 if(!nested){
  auto input=expectedInput;auto *savedName=expectedName;
  fntech3_rtcw_orientation_t inner=answer;expectedInput=inner;expectedName="nested";nested=true;
  vm_t nativeGame=game;nativeGame.entryPoint=Native;
  assert(SV_RTCWGetTag(&nativeGame,number,reinterpret_cast<intptr_t>(expectedName),reinterpret_cast<intptr_t>(&inner))==returnValue);
  nested=false;expectedInput=input;expectedName=savedName;
  assert(client.callLevel==1&&client.syscallCount==2&&!std::strcmp(name,expectedName));
  assert(!std::memcmp(out,&expectedInput,sizeof(*out)));
 }
 name[0]='!';if(!preserve)*out=answer;
 if(poison)std::memcpy(reinterpret_cast<byte*>(out)+poisonField*4,&poisonBits,4);
 return returnValue;
}
static intptr_t VM_CallNativeExports(vm_t*,int,intptr_t*){assert(false);return 0;}
int32_t VM_CallCompiled(vm_t*,int,int32_t*){assert(false);return 0;}
int32_t VM_CallInterpreted2(vm_t*,int,int32_t*){assert(false);return 0;}
'''

CHECKS = r'''
template<class F> void rejected(F f,int expectedCalls=0){
 const int before=calls;bool failed=false;try{f();}catch(int n){assert(n==7);failed=true;}
 assert(failed&&calls==before+expectedCalls&&client.callLevel==0);
}
int main(){
 for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  profile.id=id;profile.key="fixture";profile.maxClients=id==COM_GAME_RTCW_SP?128:64;
  client={};client.name="cgame";client.index=VM_CGAME;client.entryPoint=Native;
  game={};game.index=VM_GAME;game.entryPoint=Native;
  dedicated.integer=0;cgvm=&client;
  std::array<byte,192> memory{};game.dataBase=memory.data();game.exactDataLength=memory.size();game.dataMask=255;
  auto reset=[&]{
   memory.fill(0xa5);std::memcpy(memory.data()+1,"tag_head",9);
   fntech3_rtcw_orientation_t input={{-3,5,7},{{1,2,3},{4,5,6},{7,8,9}}};
   std::memcpy(memory.data()+81,&input,sizeof(input));expectedInput=input;
  };
  answer={{11,13,17},{{.25f,.5f,.75f},{1,1.25f,1.5f},{-7,-8,-9}}};
  for(bool qvm:{false,true}){
   game.entryPoint=qvm?nullptr:Native;
   auto address=[&](size_t n){return qvm?(intptr_t)n:reinterpret_cast<intptr_t>(memory.data()+n);};
   auto lookup=[&](intptr_t number,intptr_t name,intptr_t output){return SV_RTCWGetTag(&game,number,name,output);};
   reset();expectedName=reinterpret_cast<char*>(memory.data()+1);sourceName=memory.data()+1;sourceOutput=memory.data()+81;
   if(sizeof(void*)==4){
    for(int number:{0,1,profile.maxClients-1})for(int result:{0,1,-1,INT_MAX})for(bool keep:{false,true}){
     expectedClient=number;returnValue=result;preserve=keep;reset();auto saved=memory;int before=calls;
     assert(lookup(number,address(1),address(81))==result&&calls==before+2);
     assert(!std::memcmp(memory.data()+81,keep?&expectedInput:&answer,sizeof(answer)));
     for(size_t i=0;i<memory.size();++i)if(i<81||i>=129)assert(memory[i]==saved[i]);
    }
    returnValue=1;preserve=false;expectedClient=0;reset();
    for(int bad:{-1,profile.maxClients,INT_MIN,INT_MAX})rejected([&]{lookup(bad,address(1),address(81));});
    for(intptr_t bad:{intptr_t(0),intptr_t(-1)}){
     rejected([&]{lookup(0,bad,address(81));});rejected([&]{lookup(0,address(1),bad);});
    }
    std::memset(memory.data()+1,'x',64);auto saved=memory;
    rejected([&]{lookup(0,address(1),address(81));});assert(memory==saved);reset();
    // Exact-end records work; a one-byte-short QVM destination is rejected.
    std::memcpy(memory.data()+144,&expectedInput,sizeof(expectedInput));sourceOutput=memory.data()+144;
    assert(lookup(0,address(1),address(144))==1);sourceOutput=memory.data()+81;
    if(qvm){rejected([&]{lookup(0,address(1),145);});rejected([&]{lookup(0,192,address(81));});}
    // The same output/name range is valid because the name is copied first.
    reset();expectedName=reinterpret_cast<char*>(memory.data()+81);
    std::memcpy(memory.data()+81,"tag_head",9);std::memcpy(&expectedInput,memory.data()+81,sizeof(expectedInput));
    sourceName=sourceOutput=memory.data()+81;assert(lookup(0,address(81),address(81))==1);
    expectedName=reinterpret_cast<char*>(memory.data()+1);sourceName=memory.data()+1;reset();
    // Nonfinite successful output is rejected atomically after the callback.
    nested=true;poison=true;
    for(unsigned bits:{0x7f800000u,0xff800000u,0x7fc00001u})for(int field=0;field<12;++field){
     poisonBits=bits;poisonField=field;auto before=memory;
     // Native fixture expects nested call depth; simulate reentry without another callback.
     client.callLevel=1;bool failed=false;
     try{lookup(0,address(1),address(81));}catch(int n){assert(n==7);failed=true;}
     assert(failed&&memory==before&&client.callLevel==1);client.callLevel=0;
    }
    poison=false;nested=false;
   }else{
    rejected([&]{lookup(0,address(1),address(81));});
    rejected([&]{lookup(static_cast<intptr_t>(INT64_C(0x100000000)),address(1),address(81));});
   }
   reset();auto saved=memory;
   // Missing cgame preserves the complete output. Dedicated mode reads no arguments.
   cgvm=nullptr;assert(lookup(0,address(1),address(81))==0&&memory==saved);cgvm=&client;
   dedicated.integer=1;assert(SV_RTCWGetTag(nullptr,INTPTR_MAX,0,0)==0);dedicated.integer=0;
   game.index=VM_UI;rejected([&]{lookup(0,address(1),address(81));});game.index=VM_GAME;
   client.entryPoint=nullptr;rejected([&]{lookup(0,address(1),address(81));});client.entryPoint=Native;
   client.dllExports=&client;rejected([&]{lookup(0,address(1),address(81));});client.dllExports=nullptr;
   client.index=VM_GAME;rejected([&]{lookup(0,address(1),address(81));});client.index=VM_CGAME;
   for(auto other:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
    profile.id=other;rejected([&]{lookup(0,address(1),address(81));});
    rejected([&]{CL_RTCWGetTag(0,nullptr,nullptr);});
   }profile.id=id;
  }
 }
}
'''


class RTCWTagHandoffTests(unittest.TestCase):
    compile_and_run = compiler.RTCWBotLibTests.compile_and_run

    def test_native_export_nested_callback_and_native_qvm_source_bounds(self):
        vm = (ROOT / 'code/qcommon/vm.c').read_text()
        start = vm.index('#ifndef USE_DEDICATED\nstatic int VM_RTCWMenu(')
        end = vm.index('\n}\n', vm.index('intptr_t QDECL VM_Call(', start)) + 3
        game = function((ROOT / 'code/server/sv_game.cpp').read_text(), 'static intptr_t SV_RTCWGetTag(')
        client = function((ROOT / 'code/client/cl_cgame.cpp').read_text(), 'intptr_t CL_RTCWGetTag(')
        self.compile_and_run(PRELUDE + vm[start:end] + game + client + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++', str(ROOT / 'code/compat/game_abi_adapter.c')],
            ['-O2', '-ffast-math', '-I', str(ROOT / 'code/qcommon')])

    def test_dedicated_has_no_client_dependency_and_does_not_touch_arguments(self):
        game = function((ROOT / 'code/server/sv_game.cpp').read_text(), 'static intptr_t SV_RTCWGetTag(')
        source = PRELUDE[:PRELUDE.index('static cvar_t dedicated;')] + game + r'''
int main(){
 for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  profile.id=id;assert(!SV_RTCWGetTag(nullptr,INTPTR_MAX,INTPTR_MAX,INTPTR_MAX));
 }
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  profile.id=id;bool failed=false;
  try{SV_RTCWGetTag(nullptr,0,0,0);}catch(int n){assert(n==7);failed=true;}
  assert(failed);
 }
}
'''
        for definition in ('DEDICATED', 'USE_DEDICATED'):
            with self.subTest(definition=definition):
                self.compile_and_run(source, flags=['-D' + definition, '-O2'])


if __name__ == '__main__':
    unittest.main()
