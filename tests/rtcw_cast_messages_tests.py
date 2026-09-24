"""Execute cast command/configstring delivery through real entity boundaries."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap
import rtcw_server_world_tests as world
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def source():
    result = world.PRELUDE[:world.PRELUDE.index('const vec3_t vec3_origin')]
    result += r'''
#include "code/qcommon/netchan_safety.hpp"
#include <cstdarg>
#include <string>
serverStatic_t svs{};
static cvar_t gameType{}, dedicated{};
cvar_t *sv_gametype=&gameType,*com_dedicated=&dedicated;
int Com_GameIsRTCWSP(){return profile.id==COM_GAME_RTCW_SP;}
int Com_GameIsRTCWMP(){return profile.id==COM_GAME_RTCW_MP;}
int Com_GameConfigstringLimit(){return 32;}
static int drops;
void SV_DropClient(client_t *client,const char*){++drops;client->state=CS_ZOMBIE;}
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);std::strncpy(out,in,size);out[size-1]=0;}
char *CopyString(const char *s){size_t n=std::strlen(s)+1;auto *p=static_cast<char*>(std::malloc(n));assert(p);std::memcpy(p,s,n);return p;}
void Z_Free(void *p){std::free(p);}
#ifdef _WIN32
int Q_vsnprintf(char *s,size_t n,const char *format,va_list args){return vsnprintf(s,n,format,args);}
#endif
'''
    game = (ROOT / 'code/server/sv_game.cpp').read_text()
    result += game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
    for sig in ('qboolean SV_ReadGameEntity(', 'qboolean SV_ReadGameEntityByNum(',
                'qboolean SV_ClientIsRTCWCastAI(', 'qboolean SV_ClientIsWolfETBot('):
        result += '\n' + function(game, sig)
    main = (ROOT / 'code/server/sv_main.cpp').read_text()
    for sig in ('static const char *SV_ExpandNewlines(', 'void SV_AddServerCommand(',
                'void QDECL SV_SendServerCommand('):
        result += '\n' + function(main, sig)
    init = (ROOT / 'code/server/sv_init.cpp').read_text()
    for sig in ('static bool SV_SkipCastConfigstrings(', 'static void SV_SendConfigstring(',
                'void SV_UpdateConfigstrings(', 'void SV_SetConfigstring ('):
        result += '\n' + function(init, sig)
    return result


CHECKS = r'''
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
template<class Entity,class Player> void run(comGameId_t id){
 profile.id=id;const bool rtcw=Com_GameIsRTCW(),sp=Com_GameIsRTCWSP(),mp=Com_GameIsRTCWMP();
 // A bot rings its profile's window, as bot_reliable_window_tests.py pins it.
 profile.bots.reliableCommands=rtcw?256:64;
 constexpr size_t count=6,base=128,stride=sizeof(Entity)+48;
 std::vector<byte> memory(base+count*stride+128,0xa5),players(count*sizeof(Player));
 std::vector<client_t> clients(count);svs.clients=clients.data();sv.maxclients=count;
 for(size_t i=0;i<count;++i){Entity e{};e.s.number=int(i);
  e.r.svFlags=i==1?0x18:i==2?8:i==3?0x10:0;
  std::memcpy(memory.data()+base+i*stride,&e,sizeof(e));
  clients[i].gentity=reinterpret_cast<sharedEntity_t*>(memory.data()+base+i*stride);
  clients[i].state=CS_ACTIVE;
  clients[i].netchan.remoteAddress.type=i==1||i==2?NA_BOT:NA_LOOPBACK;
  clients[i].netchan.wireProfile=sp?NETCHAN_WIRE_RTCW_SP:mp?NETCHAN_WIRE_RTCW_MP:
   id==COM_GAME_QUAKELIVE?NETCHAN_WIRE_QL_RETAIL:NETCHAN_WIRE_LEGACY_Q3;
 }
 auto before=memory;
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=memory.data()+base;binding.entitySpanBytes=count*stride;
 binding.entityCount=count;binding.entityStride=stride;binding.clientBase=players.data();
 binding.clientSpanBytes=players.size();binding.clientCount=count;binding.clientStride=sizeof(Player);
 assert(serverGameData.BindNative(id,binding)==fntech3::server::GameDataBindError::None);
 for(int i=0;i<4;++i)assert(bool(SV_ClientIsRTCWCastAI(&clients[i]))==(rtcw&&(i==1||i==3)));
 clients[4].state=CS_FREE;clients[4].gentity=reinterpret_cast<sharedEntity_t*>(1);
 clients[5].state=CS_CONNECTED;clients[5].gentity=nullptr;
 assert(!SV_ClientIsRTCWCastAI(&clients[5]));
 if(rtcw)rejected([&]{SV_ClientIsRTCWCastAI(&clients[4]);});
 else assert(!SV_ClientIsRTCWCastAI(&clients[4]));
 for(int mode:{0,2,5}){
  gameType.integer=mode;drops=0;
  const bool skipConfig=sp||(mp&&mode==2);
  for(auto &c:clients){c.reliableSequence=c.reliableAcknowledge=0;for(auto &updated:c.csUpdated)updated=qfalse;}
  for(int i=0;i<Com_GameConfigstringLimit();++i)sv.configstrings[i]=CopyString("");
  sv.state=SS_GAME;
  // Cast AI have no command consumer. Sustained spawn/config churn must not
  // drop them; humans and ordinary bots acknowledge every admitted command.
  for(int n=0;n<320;++n){
   SV_SendServerCommand(nullptr,"print \"spawn %d\"",n);
   for(int i=0;i<4;++i){
    const bool cast=i==1||i==3;
    int expected=(!cast||!rtcw)*(n+1)+(!cast||!skipConfig)*n;
    assert(clients[i].reliableSequence==expected);
    if(!cast||!rtcw)clients[i].reliableAcknowledge=clients[i].reliableSequence;
   }
   SV_SetConfigstring(2,std::to_string(n).c_str());
   for(int i=0;i<4;++i){
    const bool cast=i==1||i==3;
    int expected=((!cast||!rtcw)+(!cast||!skipConfig))*(n+1);
    assert(clients[i].reliableSequence==expected);
    if(!cast||!skipConfig)clients[i].reliableAcknowledge=clients[i].reliableSequence;
   }
   assert(drops==0&&clients[4].reliableSequence==0&&clients[5].reliableSequence==0);
  }
  assert(memory==before);
  // Direct addressing is a separate contract, including casts.
  int seq=clients[1].reliableSequence;
  SV_SendServerCommand(&clients[1],"print direct");assert(clients[1].reliableSequence==seq+1);
  assert(!std::strcmp(clients[1].reliableCommands[(seq+1)&(SV_ReliableCommandLimit(&clients[1])-1)],"print direct"));
  clients[1].reliableAcknowledge=clients[1].reliableSequence;
  // An explicit download failure still reaches a connected downloader.
  clients[5].downloading=qtrue;SV_SendServerCommand(&clients[5],"disconnect download");
  assert(clients[5].reliableSequence==1);clients[5].downloading=qfalse;
  // Pending updates and long chunked configstrings obey the same cast rule.
  clients[1].state=CS_PRIMED;clients[1].csUpdated[3]=qfalse;
  seq=clients[1].reliableSequence;SV_SetConfigstring(3,"deferred");
  assert(clients[1].reliableSequence==seq&&bool(clients[1].csUpdated[3])!=skipConfig);
  clients[1].csUpdated[3]=qtrue;clients[1].state=CS_ACTIVE;
  SV_UpdateConfigstrings(&clients[1]);
  assert(clients[1].reliableSequence==seq+!skipConfig&&!clients[1].csUpdated[3]);
  clients[1].reliableAcknowledge=clients[1].reliableSequence;
  seq=clients[1].reliableSequence;SV_SetConfigstring(4,std::string(2200,'x').c_str());
  assert(clients[1].reliableSequence==seq+(skipConfig?0:3));
  // No update for an unchanged value or while a new server is loading.
  seq=clients[0].reliableSequence;SV_SetConfigstring(4,std::string(2200,'x').c_str());
  assert(clients[0].reliableSequence==seq);sv.state=SS_LOADING;SV_SetConfigstring(4,"loading");
  assert(clients[0].reliableSequence==seq&&memory==before);
  for(int i=0;i<Com_GameConfigstringLimit();++i)SV_ZFree(sv.configstrings[i]);
 }
 // A stale native game binding fails before direct classification succeeds.
 serverGameData.Invalidate();if(rtcw)rejected([&]{SV_ClientIsRTCWCastAI(&clients[1]);});
 else assert(!SV_ClientIsRTCWCastAI(&clients[1]));
 svs.clients=nullptr;sv.maxclients=0;
}
int main(){
 run<fntech3_q3_sharedEntity_t,fntech3_q3_playerState_t>(COM_GAME_QUAKE3);
 run<fntech3_ql_sharedEntity_t,fntech3_ql_playerState_t>(COM_GAME_QUAKELIVE);
 run<fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_sp_playerState_t>(COM_GAME_RTCW_SP);
 run<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP);
}
'''


class RTCWCastMessagesTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_cast_and_regular_client_delivery_in_all_profiles(self):
        self.compile_and_run(source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')])


if __name__ == '__main__':
    unittest.main()
