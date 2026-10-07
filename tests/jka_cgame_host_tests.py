"""Run production Academy cgame record cases against bounded host fixtures."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    host = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text()
    # Empty-lane update is valid only while no client cache/play path can
    # publish a trajectory. Require that boundary explicitly in this fixture.
    for name in ('CG_ROFF_CACHE', 'CG_ROFF_PLAY'):
        assert 'case Call::' + name + ':' not in host
    shared = (ROOT / 'code/client/cl_cgame.cpp').read_text()
    vm = (ROOT / 'code/qcommon/vm.c').read_text()
    prefix_start = host.index('    using namespace fntech3::jka;', host.index('static bool CL_JKACgameCall('))
    prefix_end = host.index('    const auto call =', prefix_start)
    records_start = host.index('    case Call::CG_OPENUIMENU:')
    records_end = host.index('    case Call::CG_PC_READ_TOKEN:', records_start)
    light_start = host.index('    case Call::CG_R_ADDLIGHTTOSCENE:')
    light_end = host.index('    case Call::CG_R_LERPTAG:', light_start)
    sound_start = host.index('    case Call::CG_S_GETVOICEVOLUME:')
    sound_end = host.index('    case Call::CG_GETGLCONFIG:', sound_start)
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/compat/jka_lightstyle.hpp"
#include "code/compat/jka_ui_exports.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/cgame/cg_public.h"
#include "code/qcommon/netchan_safety.hpp"
#include "code/client/client_jka_cgame.hpp"
#include "code/client/client_jka_command_values.hpp"
#include "code/compat/jka_render_adapter.hpp"
#include <vector>
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <limits>
struct Drop {};
static const char *lastDrop;
void QDECL Com_Error(errorParm_t, const char *format, ...) { lastDrop=format;throw Drop{}; }
constexpr int MAX_PARSE_ENTITIES=2048;
struct clSnapshot_t {
 qboolean valid; int snapFlags,ping,serverTime,messageNum,parseEntitiesNum,numEntities,serverCommandNum;
 byte areamask[32]; playerState_t ps,jkaVehiclePs;
};
static struct {
 int cmdNumber,parseEntitiesNum;
 int cgameUserCmdValue,cgameUserCmdPrimary,cgameUserCmdFov;float cgameSensitivity;
 usercmd_t cmds[CMD_BACKUP];
 clSnapshot_t snap,snapshots[PACKET_BACKUP];
 entityState_t parseEntities[MAX_PARSE_ENTITIES],entityBaselines[1024];
 gameState_t gameState;
} cl;
vm_t *cgvm;
vm_t *uivm;
static qboolean menuReady=qtrue;
static qboolean CL_UIModuleReady(){return menuReady;}
static int menuCalls,lastEngineMenu,lastNativeMenu;
intptr_t QDECL VM_Call(vm_t *vm,int count,int ordinal,...) {
 assert(vm==uivm&&count==1&&ordinal==UI_SET_ACTIVE_MENU);
 va_list arguments;va_start(arguments,ordinal);lastEngineMenu=va_arg(arguments,int);va_end(arguments);
 lastNativeMenu=FNTech3_JKAUIMenuToExternal(lastEngineMenu);assert(lastNativeMenu>=0);
 ++menuCalls;return 99; // The cgame import itself is void and returns zero.
}
static fntech3::jka::lightstyle::Store jkaCgameLightStyles;
static float rendererDistance=6000.0f;
static bool rendererWorld=true;
static int distanceQueries;
static qboolean GetDistance(float *out){
 ++distanceQueries;if(!rendererWorld)return qfalse;*out=rendererDistance;return qtrue;
}
static int entityTokenReads,entityTokenResets;
static qboolean GetToken(char *out,int size){
 if(size==-1){assert(!out);++entityTokenResets;return qtrue;}
 assert(out&&size>0);++entityTokenReads;out[0]=0;return qfalse;
}
void QDECL Com_DPrintf(const char *,...){}
static int polygonCalls,polygonShader;
static std::vector<polyVert_t> polygonVertices;
static void AddPoly(qhandle_t shader,int count,const polyVert_t *vertices,int polys){
 assert(polys==1&&count>0&&vertices);++polygonCalls;polygonShader=shader;
 polygonVertices.assign(vertices,vertices+count);
}
static int sceneLightCalls,lastLightAdditive;
static std::array<float,7> sceneLight;
static const float *lastLightOriginAddress;
static void RecordLight(const vec3_t origin,float radius,float r,float g,float b,int additive){
 ++sceneLightCalls;lastLightAdditive=additive;lastLightOriginAddress=origin;
 std::memcpy(sceneLight.data(),origin,sizeof(vec3_t));
 const float values[]{radius,r,g,b};std::memcpy(sceneLight.data()+3,values,sizeof(values));
}
static void AddLight(const vec3_t origin,float radius,float r,float g,float b){RecordLight(origin,radius,r,g,b,0);}
static void AddAdditiveLight(const vec3_t origin,float radius,float r,float g,float b){RecordLight(origin,radius,r,g,b,1);}
static struct {decltype(&GetDistance) GetJKADistanceCull=GetDistance;decltype(&GetToken) GetEntityToken=GetToken;
 decltype(&AddPoly) AddPolyToScene=AddPoly;
 decltype(&AddLight) AddLightToScene=AddLight;
 decltype(&AddAdditiveLight) AddAdditiveLightToScene=AddAdditiveLight;} re;
// No local .ent override: the client game's entity tokens are the renderer's.
static qboolean CL_GetEntityToken(char *out,int size){return re.GetEntityToken(out,size);}
static int sounds,clears,music;
static qboolean musicPreload;
static qboolean registrationQuiet;
static cvar_t automapDisable;
static bool automapRegistered;
cvar_t *Cvar_Get(const char *name,const char *value,int flags){
 assert(!std::strcmp(name,"r_autoMapDisable")&&!std::strcmp(value,"1")&&flags==0);
 if(!automapRegistered){automapDisable.integer=1;automapRegistered=true;}
 return &automapDisable;
}
static intptr_t voiceEntity=-1;
static int S_JKAVoiceVolume(intptr_t entity){voiceEntity=entity;return 3;}
void S_JKASetSoundRegistrationQuiet(qboolean value){registrationQuiet=value;}
sfxHandle_t S_RegisterSound(const char *name,qboolean compressed) {
 assert(!std::strcmp(name,"sound/test.wav")&&compressed==qfalse);++sounds;return 137;
}
void S_ClearLoopingSoundsFrame(){++clears;}
void S_JKAStartBackgroundTrack(const char *intro,const char *loop,qboolean preload){
 musicPreload=preload;
 assert(!std::strcmp(intro,"intro")&&!std::strcmp(loop,"loop"));++music;
}
''' + function(vm, 'void *VM_ExplicitArgPtr(') + '\n' + function(vm, 'void VM_CheckBounds(') + '\n' + \
        function(shared, 'static void CL_SetUserCmdValue(') + '\n' + \
        function(shared, 'static qboolean CL_GetUserCmd(') + '\n' + \
        function(shared, 'static clSnapshot_t *CL_FindSnapshot(') + '\n' + \
        function(host, 'static void *CL_JKACgameSpan(') + '\n' + function(host, 'static std::array<char,MAX_QPATH> CL_JKACgameMusicName(') + r'''
#undef VMA
#define VMA(index) static_cast<const char *>(VM_ExplicitArgPtr(cgvm,args[index]))
using Call=fntech3::jka::module::Call;
static bool dispatch(Call call,intptr_t *args,intptr_t &result) {
''' + host[prefix_start:prefix_end] + ' switch(call){\n' + \
        host[light_start:light_end] + host[records_start:records_end] + host[sound_start:sound_end] + r'''
 default:assert(false);
 }
 return false;
}
static intptr_t invoke(Call call,intptr_t *args){intptr_t result=-1;assert(dispatch(call,args,result));return result;}
template<class F> void rejected(F callback){bool dropped=false;try{callback();}catch(Drop){dropped=true;}assert(dropped);}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 vm_t uiVm{};uiVm.index=VM_UI;uiVm.entryPoint=reinterpret_cast<decltype(uiVm.entryPoint)>(1);uivm=&uiVm;

    { intptr_t voiceArgs[]{35,17};vm.index=VM_CGAME;
      if(sizeof(void*)==4){assert(invoke(Call::CG_S_GETVOICEVOLUME,voiceArgs)==3);
        assert(voiceEntity==17);vm.index=VM_UI;voiceEntity=-1;
        rejected([&]{invoke(Call::CG_S_GETVOICEVOLUME,voiceArgs);});assert(voiceEntity==-1);
        vm.index=VM_CGAME;vm.entryPoint=nullptr;
        rejected([&]{invoke(Call::CG_S_GETVOICEVOLUME,voiceArgs);});assert(voiceEntity==-1);
        vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
      }else{rejected([&]{invoke(Call::CG_S_GETVOICEVOLUME,voiceArgs);});assert(voiceEntity==-1);}
      assert(fntech3::jka::cgame::SharedImport(Call::CG_S_GETVOICEVOLUME)==-1);
    }
 intptr_t menuArgs[]{238,3};
 if(sizeof(void *)==4){
  for(int native=0;native<6;++native){menuArgs[1]=native;
   assert(invoke(Call::CG_OPENUIMENU,menuArgs)==0&&lastNativeMenu==native);}
  assert(menuCalls==6&&lastEngineMenu==UIMENU_POSTGAME);
  for(int native:{-1,6,11,12,INT_MIN,INT_MAX}){menuArgs[1]=native;rejected([&]{invoke(Call::CG_OPENUIMENU,menuArgs);});}
  assert(menuCalls==6);menuArgs[1]=3;
  menuReady=qfalse;rejected([&]{invoke(Call::CG_OPENUIMENU,menuArgs);});menuReady=qtrue;
  uiVm.dllExports=reinterpret_cast<decltype(uiVm.dllExports)>(1);rejected([&]{invoke(Call::CG_OPENUIMENU,menuArgs);});uiVm.dllExports=nullptr;
  uiVm.entryPoint=nullptr;rejected([&]{invoke(Call::CG_OPENUIMENU,menuArgs);});uiVm.entryPoint=reinterpret_cast<decltype(uiVm.entryPoint)>(1);
  uiVm.index=VM_CGAME;rejected([&]{invoke(Call::CG_OPENUIMENU,menuArgs);});uiVm.index=VM_UI;
  uivm=nullptr;rejected([&]{invoke(Call::CG_OPENUIMENU,menuArgs);});uivm=&uiVm;
  vm.entryPoint=nullptr;rejected([&]{invoke(Call::CG_OPENUIMENU,menuArgs);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
  assert(menuCalls==6&&fntech3::jka::cgame::SharedImport(Call::CG_OPENUIMENU)==-1);
 }else{rejected([&]{invoke(Call::CG_OPENUIMENU,menuArgs);});assert(!menuCalls);}
 auto floatBits=[](float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return static_cast<intptr_t>(bits);};
 intptr_t commandArgs[]{235,257,floatBits(0.5f),0,0,0,-1,258,0};
 if(sizeof(void *)==4){
  vm.index=VM_CGAME;cl.cgameUserCmdPrimary=77;cl.cgameUserCmdFov=88;
  assert(invoke(Call::CG_SETUSERCMDVALUE,commandArgs)==0);
  assert(cl.cgameUserCmdValue==257&&cl.cgameSensitivity==0.5f&&!cl.cgameUserCmdPrimary&&!cl.cgameUserCmdFov);
  usercmd_t selections{};selections.raven.generic_cmd=99;fntech3::jka::command::selections.Publish(selections);
  assert(selections.raven.forcesel==255&&selections.raven.invensel==2&&selections.raven.generic_cmd==99);
  auto unchanged=[&]{usercmd_t check{};fntech3::jka::command::selections.Publish(check);
   assert(check.raven.forcesel==255&&check.raven.invensel==2&&cl.cgameUserCmdValue==257&&cl.cgameSensitivity==0.5f);};
  for(int index:{2,3,4,5}){
   for(std::uint32_t encoding:{0x7f800000u,0xff800000u,0x7fc00001u}){
    auto previous=commandArgs[index];commandArgs[index]=static_cast<intptr_t>(encoding);
    rejected([&]{invoke(Call::CG_SETUSERCMDVALUE,commandArgs);});unchanged();commandArgs[index]=previous;
   }
  }
  commandArgs[2]=floatBits(-1.0f);rejected([&]{invoke(Call::CG_SETUSERCMDVALUE,commandArgs);});unchanged();commandArgs[2]=floatBits(0.5f);
  for(int index:{3,4,5}){commandArgs[index]=floatBits(1.0f);rejected([&]{invoke(Call::CG_SETUSERCMDVALUE,commandArgs);});unchanged();commandArgs[index]=0;}
  for(int value:{INT_MIN,-1,1,INT_MAX}){commandArgs[8]=value;rejected([&]{invoke(Call::CG_SETUSERCMDVALUE,commandArgs);});unchanged();}commandArgs[8]=0;
  vm.index=VM_UI;rejected([&]{invoke(Call::CG_SETUSERCMDVALUE,commandArgs);});unchanged();vm.index=VM_CGAME;
  vm.entryPoint=nullptr;rejected([&]{invoke(Call::CG_SETUSERCMDVALUE,commandArgs);});unchanged();vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
  commandArgs[2]=floatBits(-0.0f);assert(invoke(Call::CG_SETUSERCMDVALUE,commandArgs)==0);
  commandArgs[2]=floatBits(0.5f);assert(invoke(Call::CG_SETUSERCMDVALUE,commandArgs)==0);
  assert(fntech3::jka::cgame::SharedImport(Call::CG_SETUSERCMDVALUE)==-1);
 }else rejected([&]{invoke(Call::CG_SETUSERCMDVALUE,commandArgs);});
 std::array<byte,sizeof(vec3_t)+8> lightBytes;lightBytes.fill(0xa5);
 const vec3_t lightOrigin{-10.25f,0.0f,100.5f};
 std::memcpy(lightBytes.data()+3,lightOrigin,sizeof(lightOrigin));
 const auto originalLight=lightBytes;
 intptr_t lightArgs[]{207,reinterpret_cast<intptr_t>(lightBytes.data()+3),
  floatBits(256.5f),floatBits(1.5f),floatBits(-0.5f),floatBits(-0.0f)};
 if(sizeof(void *)==4){
  vm.index=VM_CGAME;
  for(Call call:{Call::CG_R_ADDLIGHTTOSCENE,Call::CG_R_ADDADDITIVELIGHTTOSCENE}){
   const int expectedAdditive=call==Call::CG_R_ADDADDITIVELIGHTTOSCENE;
   lightArgs[0]=expectedAdditive?208:207;
   const int beforeCalls=sceneLightCalls;
   assert(invoke(call,lightArgs)==0&&sceneLightCalls==beforeCalls+1);
   assert(lastLightAdditive==expectedAdditive&&lastLightOriginAddress!=reinterpret_cast<const float *>(lightBytes.data()+3));
   assert(!std::memcmp(sceneLight.data(),lightOrigin,sizeof(lightOrigin)));
   const float expected[]{256.5f,1.5f,-0.5f,-0.0f};
   assert(!std::memcmp(sceneLight.data()+3,expected,sizeof(expected))&&lightBytes==originalLight);
   const auto published=sceneLight;
   for(int field=0;field<7;++field)for(std::uint32_t encoding:{0x7f800000u,0xff800000u,0x7fc00001u,0x7f800001u}){
    auto saved=field>=3?lightArgs[field-1]:intptr_t(0);
    if(field<3)std::memcpy(lightBytes.data()+3+field*4,&encoding,4);
    else lightArgs[field-1]=static_cast<intptr_t>(encoding);
    rejected([&]{invoke(call,lightArgs);});
    assert(sceneLightCalls==beforeCalls+1&&!std::memcmp(sceneLight.data(),published.data(),sizeof(sceneLight)));
    lightBytes=originalLight;if(field>=3)lightArgs[field-1]=saved;
   }
   for(auto address:{std::uintptr_t(0),std::numeric_limits<std::uintptr_t>::max(),
                    std::numeric_limits<std::uintptr_t>::max()-7}){
    lightArgs[1]=static_cast<intptr_t>(address);rejected([&]{invoke(call,lightArgs);});
   }
   lightArgs[1]=reinterpret_cast<intptr_t>(lightBytes.data()+3);
   if(expectedAdditive)re.AddAdditiveLightToScene=nullptr;else re.AddLightToScene=nullptr;
   rejected([&]{invoke(call,lightArgs);});
   re.AddLightToScene=AddLight;re.AddAdditiveLightToScene=AddAdditiveLight;
   vm.index=VM_UI;rejected([&]{invoke(call,lightArgs);});vm.index=VM_CGAME;
   vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);rejected([&]{invoke(call,lightArgs);});vm.dllExports=nullptr;
   vm.entryPoint=nullptr;rejected([&]{invoke(call,lightArgs);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   assert(sceneLightCalls==beforeCalls+1&&lightBytes==originalLight);
   assert(fntech3::jka::cgame::SharedImport(call)==-1);
  }
  // Finite nonpositive radii and signed/HDR colors keep their exact words.
  // The existing Fn light service owns its nonpositive-radius skip behavior.
  re.AddAdditiveLightToScene=nullptr;
  for(float radius:{0.0f,-0.0f,-5.0f}){
   lightArgs[2]=floatBits(radius);const int beforeCalls=sceneLightCalls;
   assert(invoke(Call::CG_R_ADDLIGHTTOSCENE,lightArgs)==0&&sceneLightCalls==beforeCalls+1);
   assert(!std::memcmp(sceneLight.data()+3,&radius,4));
  }
  re.AddAdditiveLightToScene=AddAdditiveLight;
 }else{
  rejected([&]{invoke(Call::CG_R_ADDLIGHTTOSCENE,lightArgs);});
  rejected([&]{invoke(Call::CG_R_ADDADDITIVELIGHTTOSCENE,lightArgs);});assert(!sceneLightCalls);
 }
 std::array<byte,4*24+8> polygonBytes;polygonBytes.fill(0xa5);
 for(int vertex=0;vertex<4;++vertex){
  for(int field=0;field<5;++field){float value=vertex+field*0.25f;std::memcpy(polygonBytes.data()+3+vertex*24+field*4,&value,4);}
  for(int color=0;color<4;++color)polygonBytes[3+vertex*24+20+color]=byte(40+vertex+color);
 }
 const auto originalPolygon=polygonBytes;
 intptr_t polyArgs[]{203,137,4,reinterpret_cast<intptr_t>(polygonBytes.data()+3)};
 if(sizeof(void *)==4){
  vm.index=VM_CGAME;assert(invoke(Call::CG_R_ADDPOLYTOSCENE,polyArgs)==0);
  assert(polygonCalls==1&&polygonShader==137&&polygonVertices.size()==4&&polygonBytes==originalPolygon);
  for(int vertex=0;vertex<4;++vertex){
   assert(polygonVertices[vertex].xyz[2]==vertex+0.5f&&polygonVertices[vertex].st[1]==vertex+1.0f);
   assert(polygonVertices[vertex].modulate.rgba[0]==40+vertex&&polygonVertices[vertex].modulate.rgba[3]==43+vertex);
  }
  for(int field=0;field<5;++field)for(std::uint32_t encoding:{0x7f800000u,0xff800000u,0x7fc00001u}){
   polygonBytes=originalPolygon;std::memcpy(polygonBytes.data()+3+3*24+field*4,&encoding,4);
   rejected([&]{invoke(Call::CG_R_ADDPOLYTOSCENE,polyArgs);});assert(polygonCalls==1);
  }
  polygonBytes=originalPolygon;
  for(int count:{INT_MIN,-1,0,32769,INT_MAX}){polyArgs[2]=count;rejected([&]{invoke(Call::CG_R_ADDPOLYTOSCENE,polyArgs);});assert(polygonCalls==1);}
  polyArgs[2]=4;polyArgs[3]=0;rejected([&]{invoke(Call::CG_R_ADDPOLYTOSCENE,polyArgs);});
  polyArgs[3]=intptr_t(std::numeric_limits<std::uintptr_t>::max()-20);rejected([&]{invoke(Call::CG_R_ADDPOLYTOSCENE,polyArgs);});
  polyArgs[3]=reinterpret_cast<intptr_t>(polygonBytes.data()+3);
  re.AddPolyToScene=nullptr;rejected([&]{invoke(Call::CG_R_ADDPOLYTOSCENE,polyArgs);});re.AddPolyToScene=AddPoly;
  vm.index=VM_UI;rejected([&]{invoke(Call::CG_R_ADDPOLYTOSCENE,polyArgs);});vm.index=VM_CGAME;
  assert(polygonCalls==1&&polygonBytes==originalPolygon);
  assert(fntech3::jka::cgame::SharedImport(Call::CG_R_ADDPOLYTOSCENE)==-1);
 }else{rejected([&]{invoke(Call::CG_R_ADDPOLYTOSCENE,polyArgs);});assert(!polygonCalls);}
 std::array<byte,sizeof(fntech3_jka_mp_snapshot_t)+16> memory;memory.fill(0xa5);
 auto address=reinterpret_cast<intptr_t>(memory.data()+3);
 auto invokeOutput=[&](Call call,intptr_t index){intptr_t args[]{0,index,address};return invoke(call,args);};
 auto checked=[&](Call call,intptr_t index){auto before=memory;assert(invokeOutput(call,index)==0);assert(before==memory);};
 auto rejectedOutput=[&](Call call,intptr_t index){auto before=memory;rejected([&]{invokeOutput(call,index);});assert(before==memory);};
 cl.snap.messageNum=100;cl.parseEntitiesNum=10;
 auto &snap=cl.snapshots[100&PACKET_MASK];snap.valid=qtrue;snap.messageNum=100;
 snap.numEntities=4;snap.parseEntitiesNum=6;snap.serverCommandNum=11;snap.ps.clientNum=7;snap.jkaVehiclePs.clientNum=9;
 for(int n=6;n<10;++n)cl.parseEntities[n].number=n;
 assert(invokeOutput(Call::CG_GETSNAPSHOT,100)==1);
 fntech3_jka_mp_snapshot_t out;std::memcpy(&out,memory.data()+3,sizeof(out));
 assert(out.numEntities==4&&out.ps.clientNum==7&&out.vps.clientNum==9);
 assert(out.entities[0].number==6&&out.entities[3].number==9&&out.serverCommandSequence==11);
 assert(memory[0]==0xa5&&memory[2]==0xa5&&memory[3+sizeof(out)]==0xa5);
 checked(Call::CG_GETSNAPSHOT,-1);checked(Call::CG_GETSNAPSHOT,68);
 rejectedOutput(Call::CG_GETSNAPSHOT,101);
 snap.valid=qfalse;checked(Call::CG_GETSNAPSHOT,100);snap.valid=qtrue;
 snap.numEntities=5;checked(Call::CG_GETSNAPSHOT,100);snap.numEntities=4;
 cl.parseEntities[6].pos.gravity=1;rejectedOutput(Call::CG_GETSNAPSHOT,100);cl.parseEntities[6].pos.gravity=0;
 checked(Call::CG_GETDEFAULTSTATE,-1);checked(Call::CG_GETDEFAULTSTATE,1024);checked(Call::CG_GETDEFAULTSTATE,7);
 cl.entityBaselines[7].eFlags=0x80;cl.entityBaselines[7].number=7;
 assert(invokeOutput(Call::CG_GETDEFAULTSTATE,7)==1);
 fntech3_jka_entityState_t entity;std::memcpy(&entity,memory.data()+3,sizeof(entity));assert(entity.number==7);
 cl.cmdNumber=10;cl.cmds[10].serverTime=55;cl.cmds[10].raven.forcesel=8;
 assert(invokeOutput(Call::CG_GETUSERCMD,10)==1);
 fntech3_jka_usercmd_t command;std::memcpy(&command,memory.data()+3,sizeof(command));assert(command.serverTime==55&&command.forcesel==8);
 checked(Call::CG_GETUSERCMD,-1);rejectedOutput(Call::CG_GETUSERCMD,11);
 if(sizeof(intptr_t)>4){checked(Call::CG_GETSNAPSHOT,INT64_C(0x100000000));checked(Call::CG_GETUSERCMD,INT64_C(0x100000000));}
 cl.gameState.dataCount=1;intptr_t gsArgs[]{0,address};assert(invoke(Call::CG_GETGAMESTATE,gsArgs)==0);
 // Result is unused for void imports; only published data matters.
 fntech3_jka_mp_gameState_t gs;std::memcpy(&gs,memory.data()+3,sizeof(gs));assert(gs.dataCount==1);
 cl.gameState.dataCount=16001;auto before=memory;rejected([&]{invoke(Call::CG_GETGAMESTATE,gsArgs);});assert(before==memory);
 intptr_t nullArgs[]{0,100,0};rejected([&]{invoke(Call::CG_GETSNAPSHOT,nullArgs);});
 intptr_t overflowArgs[]{0,100,std::numeric_limits<intptr_t>::max()};
 overflowArgs[2]=intptr_t(std::numeric_limits<std::uintptr_t>::max()-20);
 rejected([&]{invoke(Call::CG_GETSNAPSHOT,overflowArgs);});
 vm.entryPoint=nullptr;rejectedOutput(Call::CG_GETSNAPSHOT,100);
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.dllExports=reinterpret_cast<void *>(1);
 rejectedOutput(Call::CG_GETSNAPSHOT,100);vm.dllExports=nullptr;
 intptr_t soundArgs[]{0,reinterpret_cast<intptr_t>("sound/test.wav")};assert(invoke(Call::CG_S_REGISTERSOUND,soundArgs)==137);
 assert(sounds==1);
 intptr_t quietArgs[]{0,INT_MIN};assert(invoke(Call::CG_S_SHUTUP,quietArgs)==0&&registrationQuiet==qtrue);
 quietArgs[1]=0;assert(invoke(Call::CG_S_SHUTUP,quietArgs)==0&&registrationQuiet==qfalse&&sounds==1&&clears==0&&music==0);
 intptr_t clearArgs[]{0};assert(invoke(Call::CG_S_CLEARLOOPINGSOUNDS,clearArgs)==0);assert(clears==1);
 intptr_t musicArgs[]{0,reinterpret_cast<intptr_t>("intro"),reinterpret_cast<intptr_t>("loop"),0};
 assert(invoke(Call::CG_S_STARTBACKGROUNDTRACK,musicArgs)==0&&music==1);
 musicArgs[3]=INT_MIN;assert(invoke(Call::CG_S_STARTBACKGROUNDTRACK,musicArgs)==0&&music==2&&musicPreload==qtrue);
 std::array<char,MAX_QPATH> badMusic;badMusic.fill('x');musicArgs[1]=reinterpret_cast<intptr_t>(badMusic.data());rejected([&]{invoke(Call::CG_S_STARTBACKGROUNDTRACK,musicArgs);});assert(music==2);
 // The optional automap query consumes only its ordinal. No cache success
 // is invented when the default disabled mode returns false.
 intptr_t automapArgs[]{225};assert(invoke(Call::CG_R_INITWIREFRAMEAUTO,automapArgs)==0&&automapRegistered);
 automapDisable.integer=0;rejected([&]{invoke(Call::CG_R_INITWIREFRAMEAUTO,automapArgs);});
 assert(std::strstr(lastDrop,"wireframe automap generation is not implemented"));
 automapDisable.integer=-7;assert(invoke(Call::CG_R_INITWIREFRAMEAUTO,automapArgs)==0);

 intptr_t tokenArgs[]{261,0,-1};assert(invoke(Call::CG_GET_ENTITY_TOKEN,tokenArgs)==1&&entityTokenResets==1);
 tokenArgs[1]=std::numeric_limits<intptr_t>::max();assert(invoke(Call::CG_GET_ENTITY_TOKEN,tokenArgs)==1&&entityTokenResets==2);
 std::array<byte,8> tokenBytes;tokenBytes.fill(0xa5);
 tokenArgs[1]=reinterpret_cast<intptr_t>(tokenBytes.data()+3);tokenArgs[2]=1;
 assert(invoke(Call::CG_GET_ENTITY_TOKEN,tokenArgs)==0&&entityTokenReads==1);
 assert(!tokenBytes[3]&&tokenBytes[2]==0xa5&&tokenBytes[4]==0xa5);
 for(int size:{INT_MIN,-2,0}){tokenArgs[2]=size;rejected([&]{invoke(Call::CG_GET_ENTITY_TOKEN,tokenArgs);});}
 tokenArgs[2]=1;tokenArgs[1]=0;rejected([&]{invoke(Call::CG_GET_ENTITY_TOKEN,tokenArgs);});
 tokenArgs[1]=intptr_t(std::numeric_limits<std::uintptr_t>::max()-2);tokenArgs[2]=8;
 rejected([&]{invoke(Call::CG_GET_ENTITY_TOKEN,tokenArgs);});assert(entityTokenReads==1);
 if(sizeof(intptr_t)>4){tokenArgs[2]=INT64_C(0x100000000);rejected([&]{invoke(Call::CG_GET_ENTITY_TOKEN,tokenArgs);});}
 assert(fntech3::jka::cgame::SharedImport(Call::CG_GET_ENTITY_TOKEN)==-1);
 vm.entryPoint=nullptr;tokenArgs[2]=-1;rejected([&]{invoke(Call::CG_GET_ENTITY_TOKEN,tokenArgs);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);

 std::array<byte,12> distanceBytes;distanceBytes.fill(0xa5);
 intptr_t distanceArgs[]{222,reinterpret_cast<intptr_t>(distanceBytes.data()+3)};
 assert(invoke(Call::CG_R_GETDISTANCECULL,distanceArgs)==0&&distanceQueries==1);
 float distance;std::memcpy(&distance,distanceBytes.data()+3,sizeof(distance));assert(distance==6000.0f);
 assert(distanceBytes[2]==0xa5&&distanceBytes[7]==0xa5);
 rendererDistance=1850.0f;assert(invoke(Call::CG_R_GETDISTANCECULL,distanceArgs)==0);
 std::memcpy(&distance,distanceBytes.data()+3,sizeof(distance));assert(distance==1850.0f);
 auto beforeDistance=distanceBytes;rendererWorld=false;
 rejected([&]{invoke(Call::CG_R_GETDISTANCECULL,distanceArgs);});assert(distanceBytes==beforeDistance);
 re.GetJKADistanceCull=nullptr;rejected([&]{invoke(Call::CG_R_GETDISTANCECULL,distanceArgs);});
 assert(distanceBytes==beforeDistance);re.GetJKADistanceCull=GetDistance;rendererWorld=true;
 distanceArgs[1]=0;int beforeQueries=distanceQueries;
 rejected([&]{invoke(Call::CG_R_GETDISTANCECULL,distanceArgs);});assert(distanceQueries==beforeQueries);
 distanceArgs[1]=intptr_t(std::numeric_limits<std::uintptr_t>::max()-2);
 rejected([&]{invoke(Call::CG_R_GETDISTANCECULL,distanceArgs);});assert(distanceQueries==beforeQueries);
 distanceArgs[1]=reinterpret_cast<intptr_t>(distanceBytes.data()+3);
 vm.entryPoint=nullptr;rejected([&]{invoke(Call::CG_R_GETDISTANCECULL,distanceArgs);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);

 std::array<byte,12> styleBytes;styleBytes.fill(0xa5);
 intptr_t getStyle[]{219,63,reinterpret_cast<intptr_t>(styleBytes.data()+3)};
 assert(invoke(Call::CG_R_GET_LIGHT_STYLE,getStyle)==0);
 for(int i=3;i<7;++i)assert(styleBytes[i]==255);
 assert(styleBytes[2]==0xa5&&styleBytes[7]==0xa5);
 intptr_t setStyle[]{220,63,static_cast<int32_t>(UINT32_C(0x817f1305))};
 assert(invoke(Call::CG_R_SET_LIGHT_STYLE,setStyle)==0);
 assert(invoke(Call::CG_R_GET_LIGHT_STYLE,getStyle)==0);
 assert(styleBytes[3]==5&&styleBytes[4]==19&&styleBytes[5]==127&&styleBytes[6]==129);
 auto beforeStyle=styleBytes;
 for(int invalid:{-1,64,INT_MIN,INT_MAX}){
  setStyle[1]=invalid;getStyle[1]=invalid;
  rejected([&]{invoke(Call::CG_R_SET_LIGHT_STYLE,setStyle);});
  rejected([&]{invoke(Call::CG_R_GET_LIGHT_STYLE,getStyle);});assert(styleBytes==beforeStyle);
 }
 setStyle[1]=0;setStyle[2]=INT32_C(0x00fffffe);
 rejected([&]{invoke(Call::CG_R_SET_LIGHT_STYLE,setStyle);});
 assert(std::strstr(lastDrop,"cannot apply colored style zero"));
 setStyle[2]=-1;assert(invoke(Call::CG_R_SET_LIGHT_STYLE,setStyle)==0);
 getStyle[1]=63;getStyle[2]=0;rejected([&]{invoke(Call::CG_R_GET_LIGHT_STYLE,getStyle);});
 getStyle[2]=intptr_t(std::numeric_limits<std::uintptr_t>::max()-2);
 rejected([&]{invoke(Call::CG_R_GET_LIGHT_STYLE,getStyle);});
 if(sizeof(intptr_t)>4){setStyle[2]=INT64_C(0x100000000);rejected([&]{invoke(Call::CG_R_SET_LIGHT_STYLE,setStyle);});}
 // One native ordinal word is sufficient: empty cleanup does not read
 // further arguments or consume any independent sound/renderer state.
 intptr_t worldEffect[]{349,0};
 const auto worldEffectStyles=styleBytes;
 const auto worldEffectVoice=voiceEntity;
 const int worldEffectSounds=sounds,worldEffectMusic=music;
 assert(invoke(Call::CG_R_WORLDEFFECTCOMMAND,worldEffect)==0);
 for(const char *command:{"", "die", "DIE", "DiE"}){
  worldEffect[1]=reinterpret_cast<intptr_t>(command);
  for(int repeated=0;repeated<3;++repeated)
   assert(invoke(Call::CG_R_WORLDEFFECTCOMMAND,worldEffect)==0);
  assert(styleBytes==worldEffectStyles&&voiceEntity==worldEffectVoice);
  assert(sounds==worldEffectSounds&&music==worldEffectMusic);
 }
 for(const char *command:{"rain", "clear", "freeze", "wind", "snow", "die ", "die;", " die"}){
  worldEffect[1]=reinterpret_cast<intptr_t>(command);
  rejected([&]{invoke(Call::CG_R_WORLDEFFECTCOMMAND,worldEffect);});
 }
 const char unterminatedWorldEffect[4]={'d','i','e','x'};
 worldEffect[1]=reinterpret_cast<intptr_t>(unterminatedWorldEffect);
 rejected([&]{invoke(Call::CG_R_WORLDEFFECTCOMMAND,worldEffect);});
 for(auto address:{std::numeric_limits<std::uintptr_t>::max(),
                   std::numeric_limits<std::uintptr_t>::max()-2}){
  worldEffect[1]=static_cast<intptr_t>(address);
  rejected([&]{invoke(Call::CG_R_WORLDEFFECTCOMMAND,worldEffect);});
 }
 worldEffect[1]=reinterpret_cast<intptr_t>("die");vm.index=VM_UI;
 rejected([&]{invoke(Call::CG_R_WORLDEFFECTCOMMAND,worldEffect);});vm.index=VM_CGAME;
 assert(invoke(Call::CG_R_WORLDEFFECTCOMMAND,worldEffect)==0);
 intptr_t cleanRoff[]{283};
 const auto voiceBeforeRoff=voiceEntity;
 const auto styleBeforeRoff=styleBytes;
 for(int repeated=0;repeated<3;++repeated){
  assert(invoke(Call::CG_ROFF_CLEAN,cleanRoff)==1);
  assert(voiceEntity==voiceBeforeRoff&&styleBytes==styleBeforeRoff);
 }
 vm.index=VM_UI;rejected([&]{invoke(Call::CG_ROFF_CLEAN,cleanRoff);});vm.index=VM_CGAME;
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);
 rejected([&]{invoke(Call::CG_ROFF_CLEAN,cleanRoff);});vm.dllExports=nullptr;
 intptr_t roffArgs[]{284};assert(invoke(Call::CG_ROFF_UPDATE_ENTITIES,roffArgs)==0);
 assert(invoke(Call::CG_ROFF_CLEAN,cleanRoff)==1);
 assert(fntech3::jka::cgame::SharedImport(Call::CG_ROFF_PURGE_ENT)==-1);
 assert(fntech3::jka::cgame::SharedImport(Call::CG_ROFF_CACHE)==-1&&fntech3::jka::cgame::SharedImport(Call::CG_ROFF_PLAY)==-1);
 vm.entryPoint=nullptr;rejected([&]{invoke(Call::CG_R_INITWIREFRAMEAUTO,automapArgs);});
 rejected([&]{invoke(Call::CG_R_SET_LIGHT_STYLE,setStyle);});
 rejected([&]{invoke(Call::CG_ROFF_UPDATE_ENTITIES,roffArgs);});
 rejected([&]{invoke(Call::CG_ROFF_CLEAN,cleanRoff);});
 rejected([&]{invoke(Call::CG_R_WORLDEFFECTCOMMAND,worldEffect);});

}
'''


class JKACgameHostTests(unittest.TestCase):
    def test_frozen_outputs_native_guard_and_distinct_audio_arguments(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jka-cgame-host'; parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary).resolve()
            cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(fixture(), encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-ffast-math',
                '-I', str(ROOT), str(cpp), '-o', str(exe)], cwd=folder, text=True,
                capture_output=True, timeout=90)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=folder, text=True, capture_output=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertTrue(folder.is_relative_to(parent.resolve()))
            self.assertFalse(any(p.is_symlink() or p.stat().st_file_attributes & 0x400
                for p in folder.rglob('*')) if os.name == 'nt' else any(p.is_symlink() for p in folder.rglob('*')))


if __name__ == '__main__':
    unittest.main()
