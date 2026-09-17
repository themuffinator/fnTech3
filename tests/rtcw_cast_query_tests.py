"""Execute the native cast callback boundary through the actual VM dispatcher."""
from pathlib import Path
import unittest
from rtcw_server_world_tests import function
import rtcw_botlib_tests as bootstrap

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#define USE_DEDICATED
#include "code/server/server.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/rtcw_module_abi.h"
#include <cassert>
#include <array>
#include <cstdarg>
#include <cstring>
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile(){return &profile;}
int Com_GameIsRTCW(){return profile.id==COM_GAME_RTCW_SP||profile.id==COM_GAME_RTCW_MP;}
int Com_GameIsRTCWSP(){return profile.id==COM_GAME_RTCW_SP;}
struct Drop{};
void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}
static vm_t module;
vm_t *gvm=&module;
static intptr_t VM_CallNativeExports(vm_t*,int,intptr_t*){assert(false);return 0;}
int32_t VM_CallCompiled(vm_t*,int,int32_t*){assert(false);return 0;}
int32_t VM_CallInterpreted2(vm_t*,int,int32_t*){assert(false);return 0;}
#include "code/server/server_rtcw_cast.hpp"
'''

CHECKS = r'''
static int calls,resultValue,sourceEntity,destinationEntity,updateFlag,duckFlag,hitFlag;
static bool nested,doNest;
static vec3_t sourcePoint{3,5,7},destinationPoint{-11,13,17};
static const float *sourceArgument,*destinationArgument;
static intptr_t QDECL Native(int call,...){
 ++calls;assert(call==11||call==12);assert(module.callLevel==(nested?2:1));++module.syscallCount;
 int32_t args[15];va_list ap;va_start(ap,call);for(int &v:args)v=va_arg(ap,int32_t);va_end(ap);
 for(int i=5;i<15;++i)assert(args[i]==0);
 auto pointer=[](int32_t value){return reinterpret_cast<float*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));};
 if(call==11){
  auto *from=pointer(args[0]),*to=pointer(args[2]);
  assert(from!=sourceArgument&&to!=destinationArgument&&from!=to);
  assert(reinterpret_cast<uintptr_t>(from)%4==0&&reinterpret_cast<uintptr_t>(to)%4==0);
  assert(!std::memcmp(from,sourcePoint,12)&&!std::memcmp(to,destinationPoint,12));
  assert(args[1]==sourceEntity&&args[3]==destinationEntity&&args[4]==updateFlag);
  from[0]=100;to[0]=200;
  if(doNest){nested=true;assert(SV_RTCWCastAttack(sourceEntity,destinationEntity,sourcePoint,duckFlag,hitFlag)==resultValue);
   nested=false;assert(module.callLevel==1&&from[0]==100&&to[0]==200);}
 }else{
  auto *point=pointer(args[2]);assert(point!=sourceArgument&&!std::memcmp(point,sourcePoint,12));
  assert(args[0]==sourceEntity&&args[1]==destinationEntity&&args[3]==duckFlag&&args[4]==hitFlag);point[0]=300;
 }
 return resultValue;
}
template<class F> static void rejected(F action,int invoked=0){int before=calls;
 try{action();assert(false);}catch(Drop&){}assert(calls==before+invoked&&module.callLevel==0);}
int main(){
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  profile={};profile.id=id;profile.key="fixture";profile.bots.maxEntities=id==COM_GAME_RTCW_SP?2048:1024;
  module={};module.index=VM_GAME;module.name="qagame";module.entryPoint=Native;gvm=&module;
  AAS_SetRTCWCastCallbacks({SV_RTCWCastAvailable,SV_RTCWCastVisible,SV_RTCWCastAttack});
  const auto &callbacks=AAS_RTCWCastCallbacks();
  if(!Com_GameIsRTCW()){
   assert(!callbacks.available&&!callbacks.visible&&!callbacks.attack);
   assert(!SV_RTCWCastAvailable()&&!SV_RTCWCastVisible(nullptr,-1,nullptr,-1,-1));
   assert(!SV_RTCWCastAttack(-1,-1,nullptr,-1,-1));
   rejected([&]{VM_Call(&module,5,AICAST_VISIBLEFROMPOS);});
   rejected([&]{VM_Call(&module,5,AICAST_CHECKATTACKATPOS);});continue;
  }
  assert(callbacks.available&&callbacks.visible&&callbacks.attack);
  gvm=nullptr;assert(!callbacks.available());assert(!callbacks.visible(nullptr,-1,nullptr,-1,-1));
  assert(!callbacks.attack(-1,-1,nullptr,-1,-1));gvm=&module;
  module.entryPoint=nullptr;rejected([&]{callbacks.available();});module.entryPoint=Native;
  module.index=VM_BAD;rejected([&]{callbacks.available();});module.index=VM_GAME;
  module.dllExports=reinterpret_cast<intptr_t*>(1);rejected([&]{callbacks.available();});module.dllExports=nullptr;
  if(sizeof(void*)!=4){rejected([&]{callbacks.available();});
   rejected([&]{callbacks.visible(nullptr,-1,nullptr,-1,-1);});
   rejected([&]{callbacks.attack(-1,-1,nullptr,-1,-1);});continue;}
  assert(callbacks.available());
  std::array<byte,64> memory{};std::memcpy(memory.data()+1,sourcePoint,12);std::memcpy(memory.data()+29,destinationPoint,12);
  sourceArgument=reinterpret_cast<const float*>(memory.data()+1);destinationArgument=reinterpret_cast<const float*>(memory.data()+29);
  const auto original=memory;
  for(int entity:{0,1,127,1023})for(int value:{0,1})for(int flag:{0,1}){
   sourceEntity=entity;destinationEntity=1023-entity;resultValue=value;
   updateFlag=duckFlag=hitFlag=flag;module.syscallCount=79;doNest=true;
   assert(callbacks.visible(sourceArgument,sourceEntity,destinationArgument,destinationEntity,updateFlag)==value);
   assert(memory==original&&module.callLevel==0&&module.syscallCount==2);doNest=false;
   assert(callbacks.attack(sourceEntity,destinationEntity,sourceArgument,duckFlag,hitFlag)==value);
   assert(memory==original&&module.callLevel==0&&module.syscallCount==1);
  }
  sourceEntity=1;destinationEntity=2;updateFlag=duckFlag=hitFlag=0;resultValue=1;
  for(int bad:{-1,1024,2047,profile.bots.maxEntities,INT_MIN,INT_MAX}){
   rejected([&]{callbacks.visible(sourceArgument,bad,destinationArgument,2,0);});
   rejected([&]{callbacks.visible(sourceArgument,1,destinationArgument,bad,0);});
   rejected([&]{callbacks.attack(bad,2,sourceArgument,0,0);});
   rejected([&]{callbacks.attack(1,bad,sourceArgument,0,0);});
   rejected([&]{callbacks.visible(sourceArgument,1,destinationArgument,2,bad);});
   rejected([&]{callbacks.attack(1,2,sourceArgument,bad,0);});
   rejected([&]{callbacks.attack(1,2,sourceArgument,0,bad);});
  }
  rejected([&]{callbacks.visible(nullptr,1,destinationArgument,2,0);});
  rejected([&]{callbacks.visible(sourceArgument,1,nullptr,2,0);});
  rejected([&]{callbacks.attack(1,2,nullptr,0,0);});
  for(uint32_t bits:{0x7fc00001u,0x7f800000u,0xff800000u,0x48000001u,0xc8000001u})for(int axis=0;axis<3;++axis){
   vec3_t invalid{3,5,7};std::memcpy(invalid+axis,&bits,4);
   rejected([&]{callbacks.visible(invalid,1,destinationArgument,2,0);});
   rejected([&]{callbacks.visible(sourceArgument,1,invalid,2,0);});
   rejected([&]{callbacks.attack(1,2,invalid,0,0);});
  }
  for(int bad:{-1,2,INT_MIN,INT_MAX}){resultValue=bad;
   rejected([&]{callbacks.visible(sourceArgument,1,destinationArgument,2,0);},1);
   rejected([&]{callbacks.attack(1,2,sourceArgument,0,0);},1);}
  assert(memory==original);
  AAS_SetRTCWCastCallbacks({});assert(!callbacks.available&&!callbacks.visible&&!callbacks.attack);
 }
}
'''


class RTCWCastQueryTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_callbacks_dispatch_ownership_reentrancy_and_profile_guards(self):
        vm = (ROOT / 'code/qcommon/vm.c').read_text()
        main = (ROOT / 'code/botlib/be_aas_main.cpp').read_text()
        source = PRELUDE
        for signature in ('static qboolean VM_PrepareRTCWExport(', 'intptr_t QDECL VM_Call('):
            source += '\n' + function(vm, signature)
        source += '\nstatic aas_rtcw_cast_callbacks_t rtcwCastCallbacks;\n'
        for signature in ('void AAS_SetRTCWCastCallbacks(', 'const aas_rtcw_cast_callbacks_t &AAS_RTCWCastCallbacks('):
            source += '\n' + function(main, signature)
        self.compile_and_run(source + CHECKS, flags=['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
