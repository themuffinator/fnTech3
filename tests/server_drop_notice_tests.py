"""A client's drop is announced in the words of the selected profile's engine.

Retail WolfMP.exe broadcasts `print "[lof]%s^7 [lon]%s\n"` when a client
leaves (the format at 0x4eeea4, pushed at 0x434f3d), so RTCW MP's client game
leaves the name as it is and translates the reason. Enemy Territory 2.60b pops
the notice up with `cpm "%s^7 %s\n"` instead (et.exe 0x441937, etded.exe
0x427f57), and says nothing when a bot leaves a single-player game type: a
client is a bot there by its entity's SVF_BOT (8) at +0x128, or by its
address (et.exe 0x441892..0x44192d, etded.exe 0x427eb2..0x427f4d). Quake III
1.32 and every other profile keep `print "%s^7 %s\n"`, and the `disconnect`
command addressed to the client that left is the same in every profile.

This compiles the production SV_AnnouncesDrop and SV_DropClient with the real
game-data view, entity readers and ET game-type rules, binds frozen module
records and runs every profile in the catalog. Each mutation below must make
the checks fail.
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
#include "code/compat/wolfet_gameinfo.hpp"
#include <cstdarg>
#include <cstddef>
#include <functional>
#include <string>
serverStatic_t svs{};
int Com_GameIsRTCWSP(){return profile.id==COM_GAME_RTCW_SP;}
int Com_GameIsRTCWMP(){return profile.id==COM_GAME_RTCW_MP;}
static cvar_t gametype{};
cvar_t *sv_gametype=&gametype;
vm_t *gvm=reinterpret_cast<vm_t*>(1);
// Everything SV_DropClient does that another function carries out, in order.
static std::vector<std::string> events;
static std::function<void()> onDisconnect;
// gameinfo.dat as the filesystem finds it, if at all.
static std::string gameinfo;
static bool gameinfoPresent;
static int gameinfoReads;
int FS_ReadFile(const char *name,void **buffer){
 assert(!std::strcmp(name,"gameinfo.dat")&&buffer);++gameinfoReads;
 if(!gameinfoPresent){*buffer=nullptr;return -1;}
 *buffer=&gameinfo[0];return int(gameinfo.size());
}
void FS_FreeFile(void *buffer){assert(buffer==&gameinfo[0]);}
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);std::strncpy(out,in,size);out[size-1]=0;}
void SV_FreeClient(client_t *c){assert(c==svs.clients);events.emplace_back("free");}
void QDECL SV_SendServerCommand(client_t *c,const char *format,...){
 assert(!c||c==svs.clients);va_list args;va_start(args,format);char text[1024];
 const int length=vsnprintf(text,sizeof(text),format,args);va_end(args);
 assert(length>=0&&size_t(length)<sizeof(text));
 events.emplace_back(std::string(c?"to the client: ":"to everyone: ")+text);
}
intptr_t QDECL VM_Call(vm_t *vm,int nargs,int call,...){
 assert(vm==gvm&&nargs==1&&call==GAME_CLIENT_DISCONNECT);va_list args;va_start(args,call);
 assert(va_arg(args,int)==0);va_end(args);events.emplace_back("game");
 if(onDisconnect)onDisconnect();return 0;
}
void SV_BotFreeClient(int n){assert(n==0);events.emplace_back("bot freed");}
void SV_SetUserinfo(int n,const char *value){assert(n==0&&!*value);svs.clients[n].name[0]=0;}
void SV_PrintClientStateChange(const client_t *c,clientState_t){assert(c==svs.clients);}
void SV_Heartbeat_f(){}
'''

CHECKS = r'''
static_assert(offsetof(fntech3_wolfet_sharedEntity_t,r.svFlags)==0x128,"et.exe and etded.exe test +0x128");
// ET's SVF_BOT, and its SVF_POW, which holds RTCW's cast-AI bit.
static constexpr int ET_BOT=0x8,ET_POW=0x10;
template<class F> static bool refused(F f){try{f();}catch(Drop&){return true;}return false;}

// Retail ET 2.60b's gameinfo.dat, from its pak0.pk3: game type 0 is single
// player and 1 is co-op.
static const char retailGameinfo[]="//spEnabled\r\nspGameTypes 0\r\ndefaultSPGameType 0\r\n"
 "coopGameTypes 1\r\ndefaultCoopGameType 1\r\ndefaultGameType 4\r\nusesProfiles 1\r\n";
static void useGameinfo(const char *text){
 gameinfoPresent=text!=nullptr;gameinfo=text?text:"";svWolfETGameInfoLoaded=false;gameinfoReads=0;
}

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
 sharedEntity_t *entity(size_t i){return reinterpret_cast<sharedEntity_t*>(record(i));}
};

static std::vector<client_t> clients(2);
static sharedEntity_t *const unreadable=reinterpret_cast<sharedEntity_t*>(1);

// Drops client 0, a network player or a bot, while client 1 plays on.
static void drop(bool bot,sharedEntity_t *gentity,const char *reason,const char *name="Player"){
 for(client_t &c:clients)c={};
 client_t &c=clients[0];c.state=CS_ACTIVE;c.netchan.remoteAddress.type=bot?NA_BOT:NA_IP;
 c.gentity=gentity;Q_strncpyz(c.name,name,sizeof c.name);clients[1].state=CS_ACTIVE;
 events.clear();SV_DropClient(&c,reason);
 assert(c.state==(bot?CS_FREE:CS_ZOMBIE));
}

// What each retail engine's SV_DropClient broadcasts for a name and a reason.
static std::string notice(unsigned id,const std::string &name,const std::string &reason){
 if(id==COM_GAME_RTCW_MP)return "print \"[lof]"+name+"^7 [lon]"+reason+"\n\"";
 if(id==COM_GAME_WOLFET)return "cpm \""+name+"^7 "+reason+"\n\"";
 return "print \""+name+"^7 "+reason+"\n\"";
}
static std::vector<std::string> sequence(bool bot,const std::string *announced,const char *reason){
 std::vector<std::string> result{"free"};
 if(announced)result.push_back("to everyone: "+*announced);
 result.emplace_back("game");
 // The addressed command is Quake III's in every profile.
 if(reason)result.push_back(std::string("to the client: disconnect \"")+reason+"\"");
 if(bot)result.emplace_back("bot freed");
 return result;
}
static void expect(bool bot,bool announced,const char *reason="was kicked",const char *name="Player"){
 const std::string text=notice(profile.id,name,reason?reason:"");
 assert(events==sequence(bot,announced&&reason?&text:nullptr,reason));
}

// The three formats, written out: WolfMP.exe 0x4eeea4, et.exe 0x561e48 and
// etded.exe 0x4c3440, and quake3.exe 1.32 0x4c7360.
static void formats(){
 assert(notice(COM_GAME_RTCW_MP,"Player","was kicked")=="print \"[lof]Player^7 [lon]was kicked\n\"");
 assert(notice(COM_GAME_WOLFET,"Player","was kicked")=="cpm \"Player^7 was kicked\n\"");
 assert(notice(COM_GAME_QUAKE3,"Player","was kicked")=="print \"Player^7 was kicked\n\"");
}

// Every profile, with no module bound: a player and a bot leave, with a
// reason and without one, in every game type.
static void everyProfile(){
 serverGameData.Invalidate();
 for(unsigned id=COM_GAME_QUAKE3;id<COM_GAME_COUNT;++id){
  profile.id=comGameId_t(id);useGameinfo(retailGameinfo);
  for(int type:{-1,0,1,2,3,4,5,6,7,32})for(bool bot:{false,true}){
   gametype.integer=type;
   // ET's single-player game type is the one place a bot goes unannounced.
   drop(bot,nullptr,"was kicked");expect(bot,!(id==COM_GAME_WOLFET&&bot&&type==0));
   // No reason: no notice and no disconnect command, in every profile.
   drop(bot,nullptr,nullptr);expect(bot,false,nullptr);
  }
  // Names and reasons are text, never formats.
  gametype.integer=2;
  drop(false,nullptr,"%s %d \"quoted\" ^1red","50%s^3x");
  expect(false,true,"%s %d \"quoted\" ^1red","50%s^3x");
  // Only ET asks gameinfo.dat which game types are single player.
  assert(gameinfoReads==(id==COM_GAME_WOLFET?1:0));
 }
}

// ET with its records bound: the entity's SVF_BOT or the address makes a bot.
static void wolfETBots(){
 profile.id=COM_GAME_WOLFET;useGameinfo(retailGameinfo);
 Module module;module.bind<fntech3_wolfet_sharedEntity_t,fntech3_wolfet_playerState_t>(COM_GAME_WOLFET,2);
 auto setFlags=[&](int flags){std::memcpy(module.record(0)+0x128,&flags,sizeof flags);};
 for(int type:{-1,0,1,2,3,4,5,6,31,32})for(int flags:{0,ET_BOT,ET_BOT|ET_POW,ET_POW,~ET_BOT,-1})
 for(bool bot:{false,true}){
  gametype.integer=type;setFlags(flags);const auto records=module.memory;
  drop(bot,module.entity(0),"was kicked");
  expect(bot,!(type==0&&(bot||(flags&ET_BOT))));
  assert(module.memory==records);
 }
 // Co-op is not single player here, for players or bots.
 gametype.integer=1;setFlags(ET_BOT);
 for(bool bot:{false,true}){drop(bot,module.entity(0),"was kicked");expect(bot,true);}
 // The notice is decided before the game hears of the drop. The game may
 // change the entity; the client is told why it left all the same.
 gametype.integer=0;
 for(int before:{0,ET_BOT})for(int after:{0,ET_BOT}){
  setFlags(before);onDisconnect=[&]{setFlags(after);};
  drop(false,module.entity(0),"was kicked");onDisconnect={};
  expect(false,before==0);
 }
 // An entity the view cannot read is refused where it would be read, not
 // taken for a player's. A bot's address or another game type reads none.
 gametype.integer=0;
 assert(refused([&]{drop(false,unreadable,"was kicked");}));
 assert(events==std::vector<std::string>{"free"});
 drop(true,unreadable,"was kicked");expect(true,false);
 drop(false,nullptr,"was kicked");expect(false,true);
 gametype.integer=2;drop(false,unreadable,"was kicked");expect(false,true);
 // A stale binding fails the same way.
 gametype.integer=0;setFlags(0);serverGameData.Invalidate();
 assert(refused([&]{drop(false,module.entity(0),"was kicked");}));
 assert(gameinfoReads==1);
}

// The single-player game types are gameinfo.dat's, not this engine's.
static void wolfETGameTypes(){
 profile.id=COM_GAME_WOLFET;serverGameData.Invalidate();
 useGameinfo("spGameTypes 0 3\ncoopGameTypes 1\n");
 for(int type:{0,1,2,3,4})for(bool bot:{false,true}){
  gametype.integer=type;drop(bot,nullptr,"was kicked");expect(bot,!(bot&&(type==0||type==3)));
 }
 assert(gameinfoReads==1);
 // Without the file no game type is single player.
 useGameinfo(nullptr);
 for(int type:{0,1,2,3,4}){gametype.integer=type;drop(true,nullptr,"was kicked");expect(true,true);}
 assert(gameinfoReads==1);
}

// RTCW SP keeps quiet about its cast AI, notice and command alike, and RTCW
// MP announces every client as WolfMP.exe does, cast bit or not.
template<class Entity,class Player> static void rtcwCast(comGameId_t id){
 profile.id=id;useGameinfo(retailGameinfo);
 Module module;module.bind<Entity,Player>(id,2);
 for(int flags:{0,int(FNTECH3_RTCW_SVF_CASTAI),int(FNTECH3_RTCW_SVF_BOT),
                int(FNTECH3_RTCW_SVF_CASTAI|FNTECH3_RTCW_SVF_BOT)})
 for(int type:{0,2})for(bool bot:{false,true}){
  Entity e;std::memcpy(&e,module.record(0),sizeof e);e.r.svFlags=flags;std::memcpy(module.record(0),&e,sizeof e);
  gametype.integer=type;drop(bot,module.entity(0),"was kicked");
  if(id==COM_GAME_RTCW_SP&&(flags&FNTECH3_RTCW_SVF_CASTAI))
   assert(events==(bot?std::vector<std::string>{"free","game","bot freed"}:std::vector<std::string>{"free","game"}));
  else expect(bot,true);
 }
 assert(gameinfoReads==0);
 serverGameData.Invalidate();
}

int main(){
 svs.clients=clients.data();sv.maxclients=int(clients.size());svs.time=4321;
 formats();
 everyProfile();
 wolfETBots();
 wolfETGameTypes();
 rtcwCast<fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_sp_playerState_t>(COM_GAME_RTCW_SP);
 rtcwCast<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP);
 svs.clients=nullptr;sv.maxclients=0;
}
'''

GAME_FUNCTIONS = ('qboolean SV_ReadGameEntity(', 'qboolean SV_ClientIsRTCWCastAI(')
MAIN_FUNCTIONS = ('void SV_WolfETLoadGameInfo( void ) {',
                  'static const fntech3::wolfet::gameinfo::GameInfo &SV_WolfETGameInfo( void ) {',
                  'qboolean SV_GameTypeIsSinglePlayer( int gameType ) {',
                  'qboolean SV_WolfETGameTypeIsCoop( int gameType ) {')
CLIENT_FUNCTIONS = ('static bool SV_AnnouncesDrop(', 'void SV_DropClient(')

# Each mutation replaces text in one production function and must fail.
MUTATIONS = {
    'RTCW MP prints the notice without its tokens, as before': (
        'void SV_DropClient(', '"print \\"[lof]%s" S_COLOR_WHITE " [lon]%s\\n\\""',
        '"print \\"%s" S_COLOR_WHITE " %s\\n\\""'),
    'the tokens trade places': (
        'void SV_DropClient(', '"print \\"[lof]%s" S_COLOR_WHITE " [lon]%s\\n\\""',
        '"print \\"[lon]%s" S_COLOR_WHITE " [lof]%s\\n\\""'),
    'RTCW SP takes the tokens too': (
        'void SV_DropClient(', 'if ( Com_GameIsRTCWMP() ) {', 'if ( Com_GameIsRTCW() ) {'),
    'ET prints its notice': (
        'void SV_DropClient(', '"cpm \\"%s"', '"print \\"%s"'),
    'ET tells only the client that left': (
        'void SV_DropClient(', 'SV_SendServerCommand( nullptr, "cpm', 'SV_SendServerCommand( drop, "cpm'),
    'the addressed command follows the notice': (
        'void SV_DropClient(', 'SV_SendServerCommand( drop, "disconnect \\"%s\\"", reason );',
        'SV_SendServerCommand( drop, Com_GameIsRTCWMP() ? "disconnect \\"[lon]%s\\"" : '
        '"disconnect \\"%s\\"", reason );'),
    'a bot by its address is announced': (
        'static bool SV_AnnouncesDrop(', 'if ( addressIsBot ) {\n\t\treturn false;',
        'if ( addressIsBot ) {\n\t\treturn true;'),
    'every ET game type is single player': (
        'static bool SV_AnnouncesDrop(', '!SV_GameTypeIsSinglePlayer( sv_gametype->integer )', 'false'),
    'co-op counts as single player': (
        'static bool SV_AnnouncesDrop(', '!SV_GameTypeIsSinglePlayer( sv_gametype->integer )',
        '!( SV_GameTypeIsSinglePlayer( sv_gametype->integer ) '
        '|| SV_WolfETGameTypeIsCoop( sv_gametype->integer ) )'),
    "Quake III's single-player game type decides": (
        'static bool SV_AnnouncesDrop(', 'SV_GameTypeIsSinglePlayer( sv_gametype->integer )',
        '( sv_gametype->integer == GT_SINGLE_PLAYER )'),
    'every profile keeps quiet about single-player bots': (
        'static bool SV_AnnouncesDrop(', '!Com_GameIsWolfET() || ', ''),
    'the address alone decides': (
        'static bool SV_AnnouncesDrop(', 'return ( entity.r.svFlags & SVF_BOT ) == 0;', 'return true;'),
    "RTCW's cast bit decides": (
        'static bool SV_AnnouncesDrop(', 'entity.r.svFlags & SVF_BOT', 'entity.r.svFlags & SVF_RTCW_CASTAI'),
    'an unreadable entity counts as a player': (
        'static bool SV_AnnouncesDrop(',
        'Com_Error( ERR_DROP, "%s: invalid client gentity", __func__ );\n\t\treturn false;',
        'return true;'),
    'RTCW MP keeps quiet about its cast AI': (
        'static bool SV_AnnouncesDrop(', 'if ( Com_GameIsRTCWSP() ) {', 'if ( Com_GameIsRTCW() ) {'),
}


def pieces():
    game = (ROOT / 'code/server/sv_game.cpp').read_text()
    main = (ROOT / 'code/server/sv_main.cpp').read_text()
    client = (ROOT / 'code/server/sv_client.cpp').read_text()
    state = main[main.index('static fntech3::wolfet::gameinfo::GameInfo svWolfETGameInfo;'):
                 main.index('void SV_WolfETLoadGameInfo( void ) {')]
    result = {'flags': game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')],
              'gameinfo state': state}
    result.update((signature, function(game, signature)) for signature in GAME_FUNCTIONS)
    result.update((signature, function(main, signature)) for signature in MAIN_FUNCTIONS)
    result.update((signature, function(client, signature)) for signature in CLIENT_FUNCTIONS)
    return result


def source(parts):
    return PRELUDE + '\n'.join(parts.values()) + '\n' + CHECKS


class ServerDropNoticeTests(unittest.TestCase):
    def run_probe(self, folder, name, text, objects):
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(text, encoding='utf-8')
        built = subprocess.run([self.compiler, *self.flags, str(cpp), *objects, '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=180)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        return subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)

    def test_each_profile_announces_a_drop_as_its_engine_does(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(self.compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        self.flags = [*target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT)]
        production = pieces()
        with tempfile.TemporaryDirectory(prefix='drop-notice-') as directory:
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
