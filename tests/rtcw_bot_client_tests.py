"""Run AI client allocation/free through real profile entity and import adapters."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap
import rtcw_server_world_tests as world
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def client_source():
    source = world.PRELUDE[:world.PRELUDE.index('const vec3_t vec3_origin')]
    source += r'''
#include "code/botlib/botlib.h"
#include "code/botlib/rtcw_navigation.h"
#include <cstdarg>
serverStatic_t svs{};
vm_t *gvm;
static int thinks,thinkClient;
intptr_t QDECL VM_Call(vm_t *vm,int nargs,int call,...){
 assert(vm==gvm&&nargs==1&&call==GAME_CLIENT_THINK);
 va_list args;va_start(args,call);thinkClient=va_arg(args,int);va_end(args);++thinks;return 0;
}
static cvar_t fps{};cvar_t *sv_fps=&fps;
static bool readFailure,writeFailure;
botlib_export_t *botlib_export;
int SV_BotLibSetup(){assert(false);return 0;}
void BotInitAvoidReach(int){assert(false);}
bool EA_ClientInputAvailable(int){assert(false);return false;}
unsigned EA_RTCWExtraInputFlags(int){assert(false);return 0;}
void EA_RestoreRTCWInput(int,const bot_input_t*,unsigned){assert(false);}
int SV_BotLibShutdown(){assert(false);return 0;}
bool AAS_SetCurrentWorld(int){assert(false);return false;}
void AAS_SetBlockingEntity(const float*,const float*,int){assert(false);}
'''
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
    source += catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
    game = (ROOT / 'code/server/sv_game.cpp').read_text()
    source += game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
    source += function(game, 'sharedEntity_t *SV_GentityNum(')
    source += function(game, 'qboolean SV_ReadGameEntity(').replace('SV_ReadGameEntity(', 'ReadEntity(')
    source += function(game, 'qboolean SV_WriteGameEntity(').replace('SV_WriteGameEntity(', 'WriteEntity(')
    source += r'''
qboolean SV_ReadGameEntity(const void *in,sharedEntity_t *out){return readFailure?qfalse:ReadEntity(in,out);}
qboolean SV_WriteGameEntity(void *out,const sharedEntity_t *in){return writeFailure?qfalse:WriteEntity(out,in);}
'''
    server = (ROOT / 'code/server/sv_bot.cpp').read_text()
    source += function(server, 'int SV_BotAllocateClient(')
    source += function(server, 'void SV_BotFreeClient(')
    source += function((ROOT / 'code/server/sv_client.cpp').read_text(), 'void SV_ClientThink (')
    source += '\n#include "code/server/server_rtcw_botlib.hpp"\n'
    return source


CHECKS = r'''
using namespace fntech3::rtcw::module;
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
template<class Entity,class Player> void run(comGameId_t id){
 for(const auto &p:profiles)if(p.id==id)profile=p;
 const bool rtcw=Com_GameIsRTCW();
 assert(profile.bots.firstClientSlot==(rtcw?1:0));
 assert(profile.bots.clientRate==(rtcw?16384:0)&&profile.bots.setSnapshotInterval==!rtcw);
 constexpr int count=4;constexpr size_t base=16,stride=sizeof(Entity)+48;
 std::vector<byte> memory(base+count*stride+16,0xa5),players(count*sizeof(Player));
 auto read=[&](int i){Entity result;std::memcpy(&result,memory.data()+base+i*stride,sizeof(result));return result;};
 auto write=[&](int i,const Entity &entity){std::memcpy(memory.data()+base+i*stride,&entity,sizeof(entity));};
 for(int i=0;i<count;++i){Entity entity{};entity.s.number=97;entity.r.svFlags=static_cast<int>(0x8000000au);write(i,entity);}
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=memory.data()+base;binding.entitySpanBytes=count*stride;
 binding.entityCount=count;binding.entityStride=stride;
 binding.clientBase=players.data();binding.clientSpanBytes=players.size();
 binding.clientCount=count;binding.clientStride=sizeof(Player);
 assert(serverGameData.BindNative(id,binding)==fntech3::server::GameDataBindError::None);
 std::vector<client_t> clients(count);svs.clients=clients.data();sv.maxclients=count;svs.time=731;fps.integer=25;
 auto expectedMemory=memory;
 auto allocate=[&](bool throughModule){
  if(!throughModule)return SV_BotAllocateClient();
  vm_t vm{};intptr_t args[]={Ordinal(id,Call::G_BOT_ALLOCATE_CLIENT)},result=777;
  assert(SV_RTCWBotLibCall(&vm,Lookup(id,Module::GAME,Direction::IMPORT,int(args[0])),args,result));return int(result);
 };
 auto release=[&](int client,bool throughModule){
  if(!throughModule){SV_BotFreeClient(client);return;}
  vm_t vm{};intptr_t args[]={Ordinal(id,Call::G_BOT_FREE_CLIENT),client},result=777;
  assert(SV_RTCWBotLibCall(&vm,Lookup(id,Module::GAME,Direction::IMPORT,int(args[0])),args,result)&&result==0);
 };
 const int first=rtcw?1:0;
 for(int i=first;i<count;++i){
  clients[i].snapshotMsec=123;clients[i].rate=9;clients[i].platformSteamId[0]='x';clients[i].tld[0]='x';
  assert(allocate(rtcw)==i);auto &client=clients[i];
  assert(client.state==CS_ACTIVE&&client.lastPacketTime==731&&client.netchan.remoteAddress.type==NA_BOT);
  assert(client.rate==(rtcw?16384:0)&&client.snapshotMsec==(rtcw?123:40));
  assert(!client.platformSteamId[0]&&!client.tld[0]&&!std::strcmp(client.country,"BOT"));
  assert(client.gentity==reinterpret_cast<sharedEntity_t*>(memory.data()+base+i*stride));
  auto expected=read(i);expected.s.number=97;
  assert(!std::memcmp(&expected,expectedMemory.data()+base+i*stride,sizeof(expected)));
  expected.s.number=i;std::memcpy(expectedMemory.data()+base+i*stride,&expected,sizeof(expected));
  assert(memory==expectedMemory);
 }
 if(rtcw)assert(clients[0].state==CS_FREE&&!clients[0].gentity&&read(0).s.number==97);
 assert(allocate(rtcw)==-1&&memory==expectedMemory);
 // Zero/one configured slots and a missing client array have no eligible RTCW slot.
 sv.maxclients=0;assert(allocate(rtcw)==-1);sv.maxclients=count;
 auto *saved=svs.clients;svs.clients=nullptr;assert(allocate(rtcw)==-1);svs.clients=saved;
 if(rtcw){sv.maxclients=1;assert(allocate(true)==-1);sv.maxclients=count;}
 auto &chosen=clients[first];std::strcpy(chosen.name,"cast");std::strcpy(chosen.platformSteamId,"old");
 for(int failure=0;failure<2;++failure){readFailure=failure==0;writeFailure=failure==1;
  rejected([&]{release(first,rtcw);});assert(chosen.state==CS_ACTIVE&&!strcmp(chosen.name,"cast"));
  assert(!strcmp(chosen.platformSteamId,"old")&&memory==expectedMemory);}
 readFailure=writeFailure=false;release(first,rtcw);
 assert(chosen.state==CS_FREE&&!chosen.name[0]&&!chosen.platformSteamId[0]);
 auto expected=read(first);assert(!(expected.r.svFlags&8));
 expected.r.svFlags|=8;assert(!std::memcmp(&expected,expectedMemory.data()+base+first*stride,sizeof(expected)));
 expected.r.svFlags&=~8;std::memcpy(expectedMemory.data()+base+first*stride,&expected,sizeof(expected));
 assert(memory==expectedMemory);
 // Failure before publication must preserve the previous client handle/state.
 auto *previous=chosen.gentity;chosen.gentity=nullptr;
 for(int failure=0;failure<2;++failure){readFailure=failure==0;writeFailure=failure==1;
  rejected([&]{allocate(rtcw);});assert(chosen.state==CS_FREE&&!chosen.gentity&&memory==expectedMemory);}
 readFailure=writeFailure=false;chosen.gentity=previous;assert(allocate(rtcw)==first);
 for(int invalid:{-1,count,INT_MAX})rejected([&]{release(invalid,rtcw);});
 if(rtcw&&sizeof(intptr_t)>4){vm_t vm{};intptr_t args[]={0,static_cast<intptr_t>(INT64_C(0x100000000))},result=77;
  rejected([&]{SV_RTCWBotLibCall(&vm,Call::G_BOT_FREE_CLIENT,args,result);});assert(result==77&&chosen.state==CS_ACTIVE);}
 chosen.gentity=nullptr;release(first,rtcw);release(first,rtcw);assert(chosen.state==CS_FREE);
 serverGameData.Invalidate();rejected([&]{allocate(rtcw);});assert(chosen.state==CS_FREE&&!chosen.gentity);
 svs.clients=nullptr;sv.maxclients=0;
}
int main(){
 run<fntech3_q3_sharedEntity_t,fntech3_q3_playerState_t>(COM_GAME_QUAKE3);
 run<fntech3_ql_sharedEntity_t,fntech3_ql_playerState_t>(COM_GAME_QUAKELIVE);
 run<fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_sp_playerState_t>(COM_GAME_RTCW_SP);
 run<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP);
}
'''


class RTCWBotClientTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_profile_slot_reservation_entity_adapters_and_failed_publication(self):
        self.compile_and_run(client_source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])

    def test_native_and_qvm_ai_command_delivery(self):
        self.compile_and_run(client_source() + COMMAND_CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


COMMAND_CHECKS = r'''
using namespace fntech3::rtcw::module;
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
template<class Command> void commands(comGameId_t id,bool native){
 profile=profiles[id==COM_GAME_RTCW_SP?2:3];
 std::vector<client_t> clients(8);svs.clients=clients.data();sv.maxclients=8;
 std::array<byte,256> memory;memory.fill(0xa5);vm_t vm{};gvm=&vm;
 vm.dataBase=memory.data();vm.exactDataLength=int(memory.size());
 if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 auto address=[&](size_t at)->intptr_t{return native?reinterpret_cast<intptr_t>(memory.data()+at):intptr_t(at);};
 Command command{};command.common.serverTime=INT_MIN;command.common.angles[0]=INT_MAX;
 command.common.angles[1]=INT_MIN;command.common.angles[2]=-17;command.common.buttons=255;
 command.common.wbuttons=254;command.common.weapon=253;command.common.holdable=252;
 command.common.forwardmove=-128;command.common.rightmove=64;command.common.upmove=127;command.common.wolfkick=251;
 if constexpr(std::is_same_v<Command,fntech3_rtcw_sp_usercmd_t>)command.cld=65535;
 else{command.mpSetup=249;command.identClient=248;}
 auto invoke=[&](intptr_t client,intptr_t input){
  intptr_t args[]={211,client,input},result=73;
  assert(SV_RTCWBotLibCall(&vm,Lookup(id,Module::GAME,Direction::IMPORT,211),args,result)&&result==0);
 };
 botlib_export=nullptr; // This import delivers game commands, independently of botlib allocation.
 for(size_t at:{size_t(3),memory.size()-sizeof(Command)})for(int client:{0,7})
  for(auto state:{CS_FREE,CS_ZOMBIE,CS_CONNECTED,CS_PRIMED,CS_ACTIVE}){
   std::memcpy(memory.data()+at,&command,sizeof(command));auto before=memory;
   clients[client].state=state;thinks=0;thinkClient=-1;invoke(client,address(at));
   Command returned;assert(fntech3::rtcw::state::FromInternal(clients[client].lastUsercmd,returned));
   assert(!std::memcmp(&returned,&command,sizeof(command))&&memory==before);
   assert(thinks==(state==CS_ACTIVE)&&clients[client].state==state);
   if(thinks)assert(thinkClient==client);
  }
 auto unchanged=[&](auto action){auto before=memory;auto commands=clients;int count=thinks;
  rejected(action);assert(before==memory&&count==thinks);
  for(size_t i=0;i<clients.size();++i)assert(!std::memcmp(&clients[i].lastUsercmd,&commands[i].lastUsercmd,sizeof(usercmd_t)));};
 for(int invalid:{-1,8,INT_MAX,INT_MIN})unchanged([&]{invoke(invalid,0);});
 unchanged([&]{invoke(7,0);});
 if(!native)for(int at:{-1,int(memory.size()-sizeof(Command)+1),int(memory.size()),INT_MAX})
  unchanged([&]{invoke(7,at);});
 else unchanged([&]{invoke(7,static_cast<intptr_t>((std::numeric_limits<uintptr_t>::max)()-4));});
 if(sizeof(intptr_t)>4)unchanged([&]{invoke(INT64_C(0x100000000),address(3));});
 sv.maxclients=profile.maxClients+1;unchanged([&]{invoke(profile.maxClients,0);});sv.maxclients=8;
 svs.clients=nullptr;unchanged([&]{invoke(7,0);});sv.maxclients=0;gvm=nullptr;
}
int main(){
 commands<fntech3_rtcw_sp_usercmd_t>(COM_GAME_RTCW_SP,false);
 commands<fntech3_rtcw_sp_usercmd_t>(COM_GAME_RTCW_SP,true);
 commands<fntech3_rtcw_mp_usercmd_t>(COM_GAME_RTCW_MP,false);
 commands<fntech3_rtcw_mp_usercmd_t>(COM_GAME_RTCW_MP,true);
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){profile.id=id;intptr_t result=77;
  assert(!SV_RTCWBotLibCall(nullptr,Call::BOTLIB_USER_COMMAND,nullptr,result)&&result==77);}
}
'''


if __name__ == '__main__':
    unittest.main()
