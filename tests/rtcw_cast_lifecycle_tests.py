"""Execute SP cast snapshot, timeout and disconnect policies in all profiles."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap
import rtcw_server_world_tests as world
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def source():
    result = world.PRELUDE[:world.PRELUDE.index('const vec3_t vec3_origin')]
    result += r'''
#include "code/server/server_cvar_compat.hpp"
#include <cstdarg>
#include <functional>
#include <string>
serverStatic_t svs{};
int Com_GameIsSTVEFSP(){return profile.id==COM_GAME_STVEF_SP;}
int Com_GameIsRTCWSP(){return profile.id==COM_GAME_RTCW_SP;}
int Com_GameIsRTCWMP(){return profile.id==COM_GAME_RTCW_MP;}
int Com_GameIsMOHAA(){return profile.id==COM_GAME_MOHAA;}
int Com_GameIsRitual(){return profile.id==COM_GAME_ALICE||profile.id==COM_GAME_FAKK2;}
// Quake Live announces a client its game marked with ragequit its own way.
int Com_GameIsQuakeLive(){return profile.id==COM_GAME_QUAKELIVE;}
// Q3Test drops a client each release's way (server_q3test_net.hpp); the
// shared path stands in for its catalog entry here.
int Com_GameIsQ3Test(){return 0;}
void SV_Q3TestDropClient(client_t*,const char*){assert(false);}
int Com_GameClientLimit(){return Com_GameIsRTCW()?128:64;}
static cvar_t timeout{},zombieTime{},quitOnEmpty{},maxclients{},gametype{};
cvar_t *sv_timeout=&timeout,*sv_zombietime=&zombieTime,*sv_quitOnEmpty=&quitOnEmpty,*sv_maxclients=&maxclients;
cvar_t *sv_gametype=&gametype;
// Only Enemy Territory's drop notice asks about the game type.
qboolean SV_GameTypeIsSinglePlayer(int){assert(false);return qfalse;}
static fnql::server::cvars::InactivityTimer emptyServerTimer;
static std::vector<std::string> events;
static std::function<void()> onDisconnect;
static bool overflow;
vm_t *gvm=reinterpret_cast<vm_t*>(1);
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);std::strncpy(out,in,size);out[size-1]=0;}
void SV_FlushClientStats(int n){assert(n==0);events.emplace_back("stats");}
struct Stats {void Reset(){events.emplace_back("resetStats");}};
static Stats clientStatsSessions[128];
void SV_SteamP2PCloseClient(uint64_t id){assert(id==73);events.emplace_back("p2p");}
void FNQL_Steam_EndAuthSession(uint64_t id){assert(id==73);events.emplace_back("auth");}
void SV_StopDemoRecord(client_t *c,qboolean silent){assert(c==svs.clients&&!silent);events.emplace_back("demo");}
void SV_Netchan_FreeQueue(client_t *c){assert(c==svs.clients);events.emplace_back("queue");}
void SV_CloseDownload(client_t *c){assert(c==svs.clients);events.emplace_back("download");}
void QDECL SV_SendServerCommand(client_t *c,const char *format,...){
 assert(!c||c==svs.clients);va_list args;va_start(args,format);char text[256];
 vsnprintf(text,sizeof(text),format,args);va_end(args);events.emplace_back(text);
}
intptr_t QDECL VM_Call(vm_t *vm,int nargs,int call,...){
 assert(vm==gvm&&nargs==1&&call==GAME_CLIENT_DISCONNECT);va_list args;va_start(args,call);
 assert(va_arg(args,int)==0);va_end(args);events.emplace_back("gameDisconnect");
 if(onDisconnect)onDisconnect();return 0;
}
void SV_BotFreeClient(int n){assert(n==0);events.emplace_back("botFree");}
void SV_SetUserinfo(int n,const char *value){assert(n==0&&!*value);events.emplace_back("userinfo");svs.clients[n].name[0]=0;svs.clients[n].userinfo[0]=0;}
void SV_PrintClientStateChange(const client_t *c,clientState_t state){assert(c==svs.clients);events.emplace_back(state==CS_FREE?"free":"zombie");}
void SV_Heartbeat_f(){events.emplace_back("heartbeat");}
void SVC_RateDropAddress(const netadr_t*,int burst,int period){assert(burst==10&&period==1000);events.emplace_back("rateDrop");}
void Cbuf_AddText(const char *text){assert(std::string(text)=="quit\n");events.emplace_back("quit");}
static void SV_BuildClientSnapshot(client_t *c){assert(c==svs.clients);events.emplace_back("snapshot");}
void MSG_Init(msg_t *msg,byte *data,int size){*msg={};msg->data=data;msg->maxsize=size;events.emplace_back("message");}
void MSG_WriteLong(msg_t *msg,int n){assert(n==73&&msg->allowoverflow);events.emplace_back("ack");}
void MSG_STVEFBitstream(msg_t*){assert(false);} // no Elite Force client is driven here
void SV_MOHAASendClientSnapshot(client_t*){assert(false);} // nor any Medal of Honor client
void SV_UpdateServerCommandsToClient(client_t*,msg_t*){events.emplace_back("commands");}
static void SV_WriteSnapshotToClient(const client_t*,msg_t *msg){events.emplace_back("writeSnapshot");msg->overflowed=overflow?qtrue:qfalse;}
void MSG_Clear(msg_t *msg){msg->cursize=0;events.emplace_back("clear");}
void SV_SendMessageToClient(msg_t*,client_t*){events.emplace_back("send");}
'''
    game = (ROOT / 'code/server/sv_game.cpp').read_text()
    result += game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
    for signature in ('qboolean SV_ReadGameEntity(', 'qboolean SV_ClientIsRTCWCastAI('):
        result += '\n' + function(game, signature)
    client = (ROOT / 'code/server/sv_client.cpp').read_text()
    for signature in ('void SV_FreeClient(', 'static bool SV_AnnouncesDrop(', 'void SV_DropClient('):
        result += '\n' + function(client, signature)
    main = (ROOT / 'code/server/sv_main.cpp').read_text()
    for signature in ('bool SV_HasHumanClients()', 'static void SV_CheckTimeouts('):
        result += '\n' + function(main, signature)
    result += '\n' + function((ROOT / 'code/server/sv_snapshot.cpp').read_text(),
                              'void SV_SendClientSnapshot(')
    return result


FIXTURE = r'''
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
static int count(const char *s){return int(std::count(events.begin(),events.end(),s));}
static bool hasPrefix(const char *s){for(const auto &e:events)if(e.find(s)==0)return true;return false;}
template<class Entity,class Player> struct Fixture {
 static constexpr size_t base=128,stride=sizeof(Entity)+48,slots=2;
 std::vector<byte> memory=std::vector<byte>(base+slots*stride+128,0xa5),players=std::vector<byte>(slots*sizeof(Player));
 std::vector<client_t> clients=std::vector<client_t>(slots);
 explicit Fixture(comGameId_t id){
  profile.id=id;svs.clients=clients.data();sv.maxclients=slots;maxclients.integer=slots;
  for(int i=0;i<int(slots);++i){Entity e{};e.s.number=i;std::memcpy(memory.data()+base+i*stride,&e,sizeof(e));}
  fntech3::server::NativeGameDataBinding b;
  b.entityBase=memory.data()+base;b.entitySpanBytes=slots*stride;b.entityCount=slots;b.entityStride=stride;
  b.clientBase=players.data();b.clientSpanBytes=players.size();b.clientCount=slots;b.clientStride=sizeof(Player);
  assert(serverGameData.BindNative(id,b)==fntech3::server::GameDataBindError::None);
  reset();
 }
 ~Fixture(){serverGameData.Invalidate();svs.clients=nullptr;sv.maxclients=0;onDisconnect={};}
 client_t &client(){return clients[0];}
 void flags(int value){Entity e;std::memcpy(&e,memory.data()+base,sizeof(e));e.r.svFlags=value;std::memcpy(memory.data()+base,&e,sizeof(e));}
 void reset(int flagsValue=0,bool bot=false){
  for(auto &c:clients)c={};auto &c=client();c.state=CS_ACTIVE;
  c.gentity=reinterpret_cast<sharedEntity_t*>(memory.data()+base);flags(flagsValue);
  c.netchan.remoteAddress.type=bot?NA_BOT:NA_LOOPBACK;
  c.netchan.wireProfile=profile.id==COM_GAME_RTCW_SP?NETCHAN_WIRE_RTCW_SP:
   profile.id==COM_GAME_RTCW_MP?NETCHAN_WIRE_RTCW_MP:profile.id==COM_GAME_QUAKELIVE?NETCHAN_WIRE_QL_RETAIL:NETCHAN_WIRE_LEGACY_Q3;
  std::strcpy(c.name,"soldier");std::strcpy(c.userinfo,"userinfo");std::strcpy(c.platformSteamId,"73");
  c.lastPacketTime=9000;c.lastClientCommand=73;svs.time=20000;
  timeout.integer=10;zombieTime.integer=2;quitOnEmpty.integer=0;emptyServerTimer.Reset();
  events.clear();onDisconnect={};overflow=false;
 }
};
template<class F> void snapshots(F &f){
 const bool sp=Com_GameIsRTCWSP();
 for(int flags:{0,8,16,24})for(bool bot:{false,true})for(bool absent:{false,true})for(bool full:{false,true}){
  f.reset(flags,bot);auto &c=f.client();if(absent)c.gentity=nullptr;overflow=full;auto before=f.memory;
  SV_SendClientSnapshot(&c);
  if(sp&&(flags&16)&&!absent)assert(events.empty());
  else if(bot)assert(events==std::vector<std::string>{"snapshot"});
  else {std::vector<std::string> expected{"snapshot","message","ack","commands","writeSnapshot"};
   if(full)expected.emplace_back("clear");expected.emplace_back("send");assert(events==expected);}
  assert(before==f.memory);
 }
 f.reset(16,true);f.client().gentity=reinterpret_cast<sharedEntity_t*>(reinterpret_cast<byte*>(f.client().gentity)+1);
 if(sp){rejected([&]{SV_SendClientSnapshot(&f.client());});assert(events.empty());}
 else {SV_SendClientSnapshot(&f.client());assert(events==std::vector<std::string>{"snapshot"});}
 f.reset(16,true);serverGameData.Invalidate();
 if(sp){rejected([&]{SV_SendClientSnapshot(&f.client());});assert(events.empty());}
 else SV_SendClientSnapshot(&f.client());
}
template<class F> void disconnects(F &f){
 const bool sp=Com_GameIsRTCWSP();
 for(int flags:{0,8,16,24})for(bool bot:{false,true})for(int after:{-2,-1,0,16})
 for(bool reason:{false,true})for(bool peer:{false,true}){
  f.reset(flags,bot);auto &c=f.client();if(peer)f.clients[1].state=CS_CONNECTED;
  c.platformAuthTicketSession=c.platformAuthSession=c.platformAuthValidated=qtrue;
  c.platformSteamIdValue=73;c.justConnected=qtrue;
  onDisconnect=[&]{if(after==-1)c.gentity=nullptr;else if(after!=-2)f.flags(after);};
  const int finalFlags=after==-2?flags:after;auto before=f.memory;
  SV_DropClient(&c,reason?"removed":nullptr);
  std::vector<std::string> expected{"stats","resetStats","p2p","auth","demo","queue","download"};
  // RTCW MP leaves the name untranslated and lets the reason translate.
  if(reason&&!(sp&&(flags&16)))expected.emplace_back(Com_GameIsRTCWMP()
   ?"print \"[lof]soldier^7 [lon]removed\n\"":"print \"soldier^7 removed\n\"");
  expected.emplace_back("gameDisconnect");
  if(reason&&!(sp&&after!=-1&&(finalFlags&16)))expected.emplace_back("disconnect \"removed\"");
  if(bot)expected.emplace_back("botFree");expected.emplace_back("userinfo");
  if(!bot)expected.emplace_back("zombie");if(reason&&!peer)expected.emplace_back("heartbeat");
  assert(events==expected&&c.state==(bot?CS_FREE:CS_ZOMBIE));
  assert(!c.platformAuthTicketSession&&!c.platformAuthSession&&!c.platformAuthValidated);
  assert(!c.justConnected&&!c.userinfo[0]&&!c.platformSteamId[0]&&c.lastDisconnectTime==svs.time);
  assert(std::string(c.name)==(bot?"":"soldier"));
  if(after>=0){auto changed=f.memory;f.flags(flags);assert(f.memory==before);f.memory=changed;}
  else assert(f.memory==before);
  if(!bot){events.clear();SV_DropClient(&c,"again");assert(events.empty());}
 }
 // Missing entities and silent errors still release every resource.
 f.reset();f.client().gentity=nullptr;SV_DropClient(&f.client(),"removed");
 assert(count("gameDisconnect")==1&&hasPrefix("print")&&hasPrefix("disconnect"));
 f.reset();f.client().state=CS_ZOMBIE;f.client().gentity=reinterpret_cast<sharedEntity_t*>(1);
 SV_DropClient(&f.client(),"again");assert(events.empty());
}
template<class F> void timeouts(F &f){
 const bool sp=Com_GameIsRTCWSP();
 for(int flags:{0,8,16,24})for(bool bot:{false,true})for(bool absent:{false,true}){
  f.reset(flags,bot);auto &c=f.client();if(absent)c.gentity=nullptr;
  const bool exempt=sp&&(absent||(flags&16));auto before=f.memory;
  for(int i=0;i<5;++i){SV_CheckTimeouts();assert(c.state==CS_ACTIVE&&c.timeoutCount==(exempt?0:i+1));}
  SV_CheckTimeouts();assert(c.state==(exempt?CS_ACTIVE:CS_FREE));
  assert(count("gameDisconnect")==!exempt&&f.memory==before);
  f.reset(flags,bot);if(absent)c.gentity=nullptr;c.timeoutCount=4;c.lastPacketTime=svs.time+1;
  SV_CheckTimeouts();assert(c.lastPacketTime==svs.time&&c.timeoutCount==(exempt?4:0)&&events.empty());
 }
 // Cast clients skip admission packet deadlines; absent, abandoned network
 // connections still expire, even in SP before their game entity is bound.
 for(int flags:{0,16})for(bool absent:{false,true}){
  f.reset(flags,true);auto &c=f.client();if(absent)c.gentity=nullptr;c.justConnected=qtrue;
  SV_CheckTimeouts();bool kept=sp&&flags&&!absent;
  assert(c.state==(kept?CS_ACTIVE:CS_FREE)&&count("rateDrop")==!kept);
 }
 // Zombie reclamation and clock correction precede cast checks. An already
 // free slot or expired zombie may retain an unusable old entity pointer.
 for(auto state:{CS_FREE,CS_ZOMBIE}){
  f.reset(16,true);auto &c=f.client();c.state=state;c.gentity=reinterpret_cast<sharedEntity_t*>(1);
  SV_CheckTimeouts();assert(c.state==CS_FREE&&count("free")==int(state==CS_ZOMBIE));
 }
 f.reset(16,true);auto &c=f.client();c.state=CS_ZOMBIE;c.lastPacketTime=svs.time+1;c.timeoutCount=4;
 SV_CheckTimeouts();assert(c.state==CS_ZOMBIE&&c.lastPacketTime==svs.time&&c.timeoutCount==(sp?4:0));
 // Authentication remains authoritative even if a game sets the cast bit.
 f.reset(16,true);c.platformAuthSession=qtrue;c.platformAuthStartedTime=svs.time-10001;
 SV_CheckTimeouts();assert(c.state==CS_FREE&&count("gameDisconnect")==1&&!c.platformAuthSession);
 if(!sp)assert(hasPrefix("disconnect \"Platform authentication timed out"));
 // Skipping every cast must not skip the shared empty-server timer.
 f.reset(16,true);c.lastPacketTime=svs.time;quitOnEmpty.integer=1;
 SV_CheckTimeouts();assert(!count("quit"));svs.time+=1001;SV_CheckTimeouts();assert(count("quit")==1);
 SV_CheckTimeouts();assert(count("quit")==1);
 // Non-SP profiles never interpret cast layouts in these policies.
 f.reset(16,true);c.gentity=reinterpret_cast<sharedEntity_t*>(1);
 if(sp){rejected([&]{SV_CheckTimeouts();});assert(c.timeoutCount==0&&events.empty());}
 else {SV_CheckTimeouts();assert(c.timeoutCount==1);}
}
template<class Entity,class Player> void run(comGameId_t id){
 Fixture<Entity,Player> f(id);CHECK(f);
}
int main(){
 run<fntech3_q3_sharedEntity_t,fntech3_q3_playerState_t>(COM_GAME_QUAKE3);
 run<fntech3_ql_sharedEntity_t,fntech3_ql_playerState_t>(COM_GAME_QUAKELIVE);
 run<fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_sp_playerState_t>(COM_GAME_RTCW_SP);
 run<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP);
}
'''


class RTCWCastLifecycleTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def check(self, name):
        self.compile_and_run(source() + FIXTURE.replace('CHECK(f)', name + '(f)'),
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')])

    def test_snapshot_policy_preserves_other_profiles_and_regular_bots(self):
        self.check('snapshots')

    def test_disconnect_notifications_callback_mutations_and_cleanup(self):
        self.check('disconnects')

    def test_timeouts_preserve_auth_admission_reclamation_and_empty_server(self):
        self.check('timeouts')


if __name__ == '__main__':
    unittest.main()
