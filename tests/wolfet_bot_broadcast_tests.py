"""Enemy Territory leaves bots out of broadcast server commands.

Retail ET 2.60b's SV_SendServerCommand passes over a primed client whose
entity carries SVF_BOT, 8 in the module record's r.svFlags at +0x128
(et.exe 0x448875..0x44888b, etded.exe 0x42f0e0..0x42f0f6), as ET's GPL server
does. Only the game's bot AI acknowledges a bot's reliable commands, so a
broadcast queued for one does nothing but fill its window. RTCW's loops test
SVF_CASTAI instead (WolfMP.exe 0x439fe2..0x439ff8, WolfSP.exe
0x4378bd..0x4378d3), and Quake III 1.32's tests no flag at all. This compiles
the production SV_ReadGameEntity, SV_ClientIsRTCWCastAI, SV_ClientIsWolfETBot,
SV_ExpandNewlines, SV_AddServerCommand and SV_SendServerCommand with the real
game-data view, binds frozen module records and runs every profile. Each
mutation below must make the checks fail.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
import rtcw_server_world_tests as world
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = world.PRELUDE[:world.PRELUDE.index('const vec3_t vec3_origin')] + r'''
#include "code/compat/wolfet_game_abi.h"
#include "code/qcommon/netchan_safety.hpp"
#include <cstdarg>
#include <cstddef>
#include <string>
serverStatic_t svs{};
static cvar_t dedicated{};
cvar_t *com_dedicated=&dedicated;
int Com_GameIsRTCWMP(){return profile.id==COM_GAME_RTCW_MP;}
static std::vector<int> dropped;
void SV_DropClient(client_t *client,const char *){dropped.push_back(SV_ClientIndex(client));client->state=CS_ZOMBIE;}
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);std::strncpy(out,in,size);out[size-1]=0;}
#ifdef _WIN32
int Q_vsnprintf(char *s,size_t n,const char *format,va_list args){return vsnprintf(s,n,format,args);}
#endif
'''

CHECKS = r'''
static_assert(offsetof(fntech3_wolfet_sharedEntity_t,r.svFlags)==0x128,"et.exe and etded.exe test +0x128");
// ET's SVF_BOT, and its SVF_POW, which holds RTCW's cast-AI bit.
static constexpr int ET_BOT=0x8,ET_POW=0x10;
template<class F> bool refused(F f){try{f();}catch(Drop&){return true;}return false;}

// A game's records, each followed by module-private bytes, and its players.
struct Module {
 std::vector<byte> memory,players;
 size_t base=128,stride=0;
 template<class Entity,class Player> void bind(comGameId_t id,size_t count){
  stride=sizeof(Entity)+48;memory.assign(base+count*stride+128,0xa5);players.assign(count*sizeof(Player),0);
  for(size_t i=0;i<count;++i){Entity e{};e.s.number=int(i);std::memcpy(record(i),&e,sizeof e);}
  fntech3::server::NativeGameDataBinding binding;
  binding.entityBase=record(0);binding.entitySpanBytes=count*stride;binding.entityCount=int(count);
  binding.entityStride=stride;binding.clientBase=players.data();binding.clientSpanBytes=players.size();
  binding.clientCount=int(count);binding.clientStride=sizeof(Player);
  assert(serverGameData.BindNative(id,binding)==fntech3::server::GameDataBindError::None);
 }
 byte *record(size_t i){return memory.data()+base+i*stride;}
};

static std::vector<client_t> clients;
static sharedEntity_t *const unreadable=reinterpret_cast<sharedEntity_t*>(1);
struct Slot { clientState_t state; netadrtype_t address; sharedEntity_t *gentity; };
static void setup(const std::vector<Slot> &slots,netchanWireProfile_t wire=NETCHAN_WIRE_LEGACY_Q3){
 assert(slots.size()==clients.size());dropped.clear();
 // A bot rings its profile's window, as bot_reliable_window_tests.py pins it:
 // 256 in RTCW and ET, and 64 here for the others.
 profile.bots.reliableCommands=Com_GameIsRTCW()||Com_GameIsWolfET()?256:64;
 for(size_t i=0;i<slots.size();++i){
  client_t &c=clients[i];std::memset(&c,0,sizeof c);
  c.state=slots[i].state;c.netchan.remoteAddress.type=slots[i].address;
  // A bot never sets up a channel, so it keeps a fresh slot's wire profile.
  if(slots[i].address!=NA_BOT)c.netchan.wireProfile=wire;
  c.gentity=slots[i].gentity;
 }
}
// Sends one command and checks that exactly `receivers` stored it.
static void send(client_t *target,const std::string &command,const std::vector<int> &receivers){
 std::vector<int> before;for(const client_t &c:clients)before.push_back(c.reliableSequence);
 SV_SendServerCommand(target,"%s",command.c_str());
 for(size_t i=0;i<clients.size();++i){
  const client_t &c=clients[i];
  const bool receives=std::find(receivers.begin(),receivers.end(),int(i))!=receivers.end();
  assert(c.reliableSequence==before[i]+(receives?1:0));
  if(receives){
   const std::string stored=command.substr(0,sizeof c.reliableCommands[0]-1);
   assert(stored==c.reliableCommands[c.reliableSequence&(SV_ReliableCommandLimit(&c)-1)]);
  }
 }
}
static void acknowledge(const std::vector<int> &players){
 for(int i:players)clients[i].reliableAcknowledge=clients[i].reliableSequence;
}

static void wolfET(){
 profile.id=COM_GAME_WOLFET;
 Module module;module.bind<fntech3_wolfet_sharedEntity_t,fntech3_wolfet_playerState_t>(COM_GAME_WOLFET,9);
 // The module's flags, written where et.exe reads them.
 const int flags[9]={0,ET_BOT,ET_BOT|ET_POW,~ET_BOT,0,ET_BOT,ET_BOT,ET_BOT,ET_BOT};
 for(int i=0;i<9;++i)std::memcpy(module.record(i)+0x128,&flags[i],sizeof flags[i]);
 auto entity=[&](int i){return reinterpret_cast<sharedEntity_t*>(module.record(i));};
 setup({{CS_ACTIVE,NA_IP,entity(0)},      // a player
        {CS_ACTIVE,NA_BOT,entity(1)},     // a bot, flagged as G_AddBot flags it
        {CS_ACTIVE,NA_BOT,entity(2)},     // a prisoner-of-war bot
        {CS_ACTIVE,NA_IP,entity(3)},      // every other bit, RTCW's cast bit among them
        {CS_ACTIVE,NA_BOT,entity(4)},     // an engine bot slot whose entity is unflagged
        {CS_PRIMED,NA_IP,entity(5)},      // a flagged network client, still primed
        {CS_PRIMED,NA_BOT,entity(6)},     // a primed bot
        {CS_CONNECTED,NA_IP,unreadable},  // not primed, so never read
        {CS_FREE,NA_BOT,unreadable}},     // free, so never read
       NETCHAN_WIRE_WOLFET);
 const std::vector<int> players={0,3,4};
 for(int i=0;i<7;++i)assert(bool(SV_ClientIsWolfETBot(&clients[i]))==((flags[i]&ET_BOT)!=0));
 // An entity the view cannot read is refused rather than taken for a player.
 assert(refused([&]{SV_ClientIsWolfETBot(&clients[8]);}));
 clients[8].gentity=nullptr;assert(!SV_ClientIsWolfETBot(&clients[8]));clients[8].gentity=unreadable;
 // Retail's order: a bot is passed over before the length rule is applied.
 clients[1].longstr=clients[3].longstr=qtrue;
 const auto records=module.memory;
 for(int server:{0,1}){
  dedicated.integer=server;
  for(int n=0;n<700;++n){
   send(nullptr,"print \"broadcast "+std::to_string(n)+"\n\"",players);acknowledge(players);
   send(nullptr,"chat \"console: "+std::to_string(n)+"\"",players);acknowledge(players);
  }
  send(nullptr,"cpm \""+std::string(1100,'x')+"\"",{3});acknowledge(players);
 }
 // Nothing acknowledged a bot's commands, and none was queued any to overflow.
 assert(dropped.empty());
 for(int i:{1,2,5,6})assert(clients[i].reliableSequence==0);
 // Commands addressed to a bot still reach it.
 send(&clients[1],"print \"direct\n\"",{1});
 send(&clients[5],"cs 689 \"n\\wolfBot\"",{5});
 send(&clients[6],"print \"direct\n\"",{6});
 assert(module.memory==records);
 // A stale binding fails before a bot could be told from a player.
 serverGameData.Invalidate();
 assert(refused([&]{SV_ClientIsWolfETBot(&clients[1]);}));
 assert(refused([&]{SV_SendServerCommand(nullptr,"print \"stale\n\"");}));
 dedicated.integer=0;
}

// No other profile reads an entity to leave a client out, except RTCW's
// cast rule: every primed client there gets the broadcast.
static void otherProfiles(){
 serverGameData.Invalidate();
 for(unsigned id=COM_GAME_QUAKE3;id<COM_GAME_COUNT;++id){
  if(id==COM_GAME_WOLFET)continue;
  profile.id=comGameId_t(id);
  setup({{CS_ACTIVE,NA_IP,unreadable},{CS_ACTIVE,NA_BOT,unreadable},{CS_PRIMED,NA_BOT,unreadable},
         {CS_CONNECTED,NA_BOT,unreadable},{CS_ZOMBIE,NA_BOT,unreadable},{CS_FREE,NA_BOT,unreadable},
         {CS_ACTIVE,NA_BOT,nullptr},{CS_PRIMED,NA_IP,nullptr},{CS_FREE,NA_BOT,nullptr}});
  for(client_t &c:clients)assert(!SV_ClientIsWolfETBot(&c));
  if(Com_GameIsRTCW())continue;
  for(int n=0;n<300;++n){send(nullptr,"print \"x\n\"",{0,1,2,6,7});acknowledge({0,1,2,6,7});}
  assert(dropped.empty());
 }
}

// With readable records, a flagged bot still gets the broadcast in Quake III
// and RTCW MP, and RTCW MP still leaves its cast AI out.
template<class Entity,class Player> static void flaggedBots(comGameId_t id){
 profile.id=id;
 Module module;module.bind<Entity,Player>(id,9);
 for(int i=0;i<9;++i){
  Entity e;std::memcpy(&e,module.record(i),sizeof e);
  e.r.svFlags=i==1||i==2?ET_BOT:i==3?ET_POW:0;std::memcpy(module.record(i),&e,sizeof e);
 }
 std::vector<Slot> slots;
 for(int i=0;i<9;++i)slots.push_back({CS_ACTIVE,i?NA_BOT:NA_IP,reinterpret_cast<sharedEntity_t*>(module.record(i))});
 setup(slots);
 const std::vector<int> receivers=Com_GameIsRTCW()?std::vector<int>{0,1,2,4,5,6,7,8}
  :std::vector<int>{0,1,2,3,4,5,6,7,8};
 for(int n=0;n<300;++n){send(nullptr,"print \"x\n\"",receivers);acknowledge(receivers);}
 assert(dropped.empty());
 serverGameData.Invalidate();
}

int main(){
 clients=std::vector<client_t>(9);
 svs.clients=clients.data();sv.maxclients=int(clients.size());
 wolfET();
 otherProfiles();
 flaggedBots<fntech3_q3_sharedEntity_t,fntech3_q3_playerState_t>(COM_GAME_QUAKE3);
 flaggedBots<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP);
 svs.clients=nullptr;sv.maxclients=0;
}
'''

GAME_FUNCTIONS = ('qboolean SV_ReadGameEntity(', 'qboolean SV_ClientIsRTCWCastAI(',
                  'qboolean SV_ClientIsWolfETBot(')
MAIN_FUNCTIONS = ('static const char *SV_ExpandNewlines(', 'void SV_AddServerCommand(',
                  'void QDECL SV_SendServerCommand(')

# Each mutation replaces text in one production function and must fail.
MUTATIONS = {
    'ET sends its bots every broadcast, as before': (
        'void QDECL SV_SendServerCommand(', '\n\t\t\t|| SV_ClientIsWolfETBot( &client )', ''),
    'every profile leaves its bots out': (
        'qboolean SV_ClientIsWolfETBot(', 'if ( !Com_GameIsWolfET() || !client->gentity )',
        'if ( !client->gentity )'),
    'the address type decides, not the flag': (
        'qboolean SV_ClientIsWolfETBot(', 'return SV_QBool( ( entity.r.svFlags & SVF_BOT ) != 0 );',
        'return SV_QBool( client->netchan.remoteAddress.type == NA_BOT );'),
    "RTCW's cast bit decides": (
        'qboolean SV_ClientIsWolfETBot(', 'entity.r.svFlags & SVF_BOT', 'entity.r.svFlags & SVF_RTCW_CASTAI'),
    'any flag decides': (
        'qboolean SV_ClientIsWolfETBot(', '( entity.r.svFlags & SVF_BOT ) != 0', 'entity.r.svFlags != 0'),
    'clients that are not primed are read': (
        'void QDECL SV_SendServerCommand(', 'client.state >= CS_PRIMED && ', ''),
    'an unreadable entity counts as a player': (
        'qboolean SV_ClientIsWolfETBot(',
        'Com_Error( ERR_DROP, "%s: invalid client gentity", __func__ );\n\t\treturn qfalse;',
        'return qfalse;'),
    'addressed commands leave bots out too': (
        'void QDECL SV_SendServerCommand(', 'if ( len <= 1022 || cl->longstr ) {',
        'if ( ( len <= 1022 || cl->longstr ) && !SV_ClientIsWolfETBot( cl ) ) {'),
}


def pieces():
    game = (ROOT / 'code/server/sv_game.cpp').read_text()
    main = (ROOT / 'code/server/sv_main.cpp').read_text()
    result = {'flags': game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]}
    result.update((signature, function(game, signature)) for signature in GAME_FUNCTIONS)
    result.update((signature, function(main, signature)) for signature in MAIN_FUNCTIONS)
    return result


def source(parts):
    return PRELUDE + '\n'.join(parts.values()) + '\n' + CHECKS


class WolfETBotBroadcastTests(unittest.TestCase):
    def run_probe(self, folder, name, text, objects):
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(text, encoding='utf-8')
        built = subprocess.run([self.compiler, *self.flags, str(cpp), *objects, '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=180)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        return subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)

    def test_et_leaves_flagged_bots_out_of_broadcasts_and_no_other_profile_changes(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(self.compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        self.flags = [*target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT)]
        production = pieces()
        with tempfile.TemporaryDirectory(prefix='wolfet-bot-') as directory:
            folder = Path(directory)
            objects = []
            for name, path, language in (('view', 'code/server/server_game_data.cpp', []),
                                         ('adapter', 'code/compat/game_abi_adapter.c', ['-x', 'c++'])):
                built = subprocess.run([self.compiler, *self.flags, '-c', *language, str(ROOT / path),
                                        '-o', str(folder / f'{name}.o')],
                                       cwd=ROOT, capture_output=True, text=True, timeout=180)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                objects.append(str(folder / f'{name}.o'))
            ran = self.run_probe(folder, 'production', source(production), objects)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            for index, (description, (signature, old, new)) in enumerate(MUTATIONS.items()):
                with self.subTest(mutation=description):
                    self.assertEqual(production[signature].count(old), 1, old)
                    mutated = dict(production, **{signature: production[signature].replace(old, new)})
                    ran = self.run_probe(folder, f'mutation{index}', source(mutated), objects)
                    self.assertNotEqual(ran.returncode, 0, description)


if __name__ == '__main__':
    unittest.main()
