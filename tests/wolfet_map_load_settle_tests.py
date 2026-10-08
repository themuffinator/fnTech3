"""How a map load settles the new game and brings its clients back.

Retail Enemy Territory 2.60b's SV_SpawnServer (et.exe 0x4467f0, etded.exe
0x42cf20) runs six frames after starting its game, each with its bots at the
current time before advancing it (et.exe 0x4469e1..0x446a12), and its last
frame the same way (0x446b14..0x446b3a). Between them it drops every bot from a
single-player or co-op game type, with no reason (0x446a3f..0x446a7b). Every
other retail engine runs three frames and its last one the same way, each at
the current time before advancing it (2026-09-24-settle-frame-order.md), and
so does every other profile here, but for the IHV test's frames, Alice's
own, which F.A.K.K.2 shares, and Medal of Honor's own, where this tree has
them. Q3Test settles as the others do, but frees its bots' slots without
asking the reloaded game (1.02 SV_SpawnServer 0x41be5c, 1.06 0x41cede) and
runs no last frame, except 1.08, which runs three (0x41d876..0x41d8c7). This
compiles the production block of SV_SpawnServer, from the settling frames to
the last frame, and runs it for all sixteen profiles, Q3Test's 1.08 apart.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
enum { Q3, QL, RTCW_SP, RTCW_MP, WOLFET, STVEF_SP, STVEF_MP, Q3_IHV, JKA_SP, JKA_MP, JK2_SP, JK2_MP, ALICE, FAKK2,
       MOHAA, Q3TEST, Q3TEST_1_08, PROFILES };
static int profile;
int Com_GameIsWolfET(){return profile==WOLFET;}
int Com_GameIsQuake3IHV(){return profile==Q3_IHV;}
int Com_GameIsAlice(){return profile==ALICE;}
int Com_GameIsRitual(){return profile==ALICE||profile==FAKK2;}
int Com_GameIsMOHAA(){return profile==MOHAA;}
// Q3Test's releases differ here only in 1.08, the one on protocol 40.
int Com_GameIsQ3Test(){return profile==Q3TEST||profile==Q3TEST_1_08;}
struct Q3TestRelease { int protocol; };
const Q3TestRelease *Com_Q3TestRelease(){
 static Q3TestRelease release;release.protocol=profile==Q3TEST_1_08?40:39;return &release;
}
typedef int qboolean;
static constexpr int qfalse=0;
// Game types 0 and 1 are single player and co-op, as a gameinfo.dat can name
// them for ET. Every profile gets the same answers, so only ET acting on them
// shows that the load asks for ET alone.
struct Cvar { int integer; };
static Cvar gametype;
static Cvar *sv_gametype=&gametype;
qboolean SV_GameTypeIsSinglePlayer(int type){return type==0;}
qboolean SV_WolfETGameTypeIsCoop(int type){return type==1;}
enum { CS_FREE, CS_ZOMBIE, CS_CONNECTED, CS_PRIMED, CS_ACTIVE };
enum { NA_BAD, NA_BOT, NA_LOOPBACK, NA_IP };
enum { GSA_INIT, GSA_SENT_ONCE, GSA_SENT_MANY, GSA_ACKED };
enum { GAME_INIT, GAME_SHUTDOWN, GAME_CLIENT_CONNECT, GAME_CLIENT_BEGIN, GAME_RUN_FRAME=8 };
struct client_t {
 int state; struct { struct { int type; } remoteAddress; } netchan; int gamestateAck; void *gentity; bool refused;
 char name[32];
};
struct SV_ClientSlot { client_t &client; int index; };
static client_t clients[8];
std::vector<SV_ClientSlot> SV_ClientSlots(){std::vector<SV_ClientSlot> slots;for(int i=0;i<8;++i)slots.push_back({clients[i],i});return slots;}
std::vector<int> SV_Indices(int count){std::vector<int> indices;for(int i=0;i<count;++i)indices.push_back(i);return indices;}
static struct { int time; } sv;
static struct { int time; } svs;
static void *gvm=&sv;
// Every call the block makes, in order: its kind, two values, sv.time and a
// drop's reason.
struct Event {
 std::string kind; int a, b, time; std::string reason;
 bool operator==(const Event &o) const {return kind==o.kind&&a==o.a&&b==o.b&&time==o.time&&reason==o.reason;}
};
static std::vector<Event> events;
static int initTime, lastFrame, waits;
void SV_BotFrame(int time){events.push_back({"botframe",time,0,sv.time,""});}
// Jedi Academy's Ghoul2 clock: jka_server_attachment_tests.py checks where it is set.
void SV_JKAGhoul2SetTime(int){}
void Cbuf_Wait(){++waits;}
void SV_CreateBaseline(){events.push_back({"baseline",0,0,sv.time,""});}
// Alice's own frames are its record's; here the call is all that counts.
void SV_AliceSettle(qboolean restored){events.push_back({"alice",restored,0,sv.time,""});}
// So are Medal of Honor's, which its own game runs.
void SV_MOHAASettle(){events.push_back({"mohaa",0,0,sv.time,""});}
intptr_t VM_Call(void *vm,int count,int call,...){
 assert(vm==gvm);
 va_list args;va_start(args,call);intptr_t result=0;
 if(call==GAME_RUN_FRAME){
  assert(count==1);lastFrame=va_arg(args,int);events.push_back({"frame",lastFrame,0,sv.time,""});
 }else if(call==GAME_CLIENT_CONNECT){
  assert(count==3);
  const int index=va_arg(args,int),firstTime=va_arg(args,int),isBot=va_arg(args,int);
  assert(!firstTime);events.push_back({"connect",index,isBot,sv.time,""});result=clients[index].refused;
 }else assert(!"unexpected game call");
 va_end(args);return result;
}
const void *GVM_ArgPtr(intptr_t refused){return refused?"refused":nullptr;}
void SV_DropClient(client_t *client,const char *reason){
 assert(reason);events.push_back({"drop",int(client-clients),0,sv.time,reason});
 client->state=client->netchan.remoteAddress.type==NA_BOT?CS_FREE:CS_ZOMBIE;
}
// Entering the world begins the client in the game. ET's ClientBegin sends a
// team player to limbo once the game's time is more than 600 ms past the
// level's start (qagame_mp_x86.dll 0x2004010e..0x20040120).
void SV_ClientEnterWorld(client_t *client,const void *cmd){
 assert(!cmd);events.push_back({"enter",int(client-clients),lastFrame-initTime,sv.time,""});
 client->state=CS_ACTIVE;
}
// The IHV test offers every client, bots included, to its game through its
// own sessions (server_q3_ihv_net.hpp); here the call is all that counts.
void SV_IHVNetReconnectClients(){events.push_back({"ihvreconnect",0,0,sv.time,""});}
'''
CHECKS = r'''
static std::vector<Event> Expected(int p,int type,bool killBots){
 const bool et=p==WOLFET, ihv=p==Q3_IHV&&ihvPath, alice=(p==ALICE||p==FAKK2)&&alicePath, mohaa=p==MOHAA&&mohaaPath;
 const bool q3test=(p==Q3TEST||p==Q3TEST_1_08)&&q3testPath;
 const int start=10000;
 int time=start, game=start;
 std::vector<Event> want;
 if(alice){
  want.push_back({"alice",1,0,start,""});
 }else if(mohaa){
  want.push_back({"mohaa",0,0,start,""});
 }else{
  // Every profile runs each frame at the current time, then advances it, as
  // every retail engine does, so the first frame and its bots are given the
  // level's start.
  for(int i=0;i<(et?6:3);++i){
   want.push_back({"frame",time,0,time,""});
   want.push_back({"botframe",time,0,time,""});
   game=time;
   time+=100;
  }
 }
 want.push_back({"baseline",0,0,time,""});
 // Slots: 2 connected, 3 loading, 4 active, 5 an active bot, 6 a loading bot,
 // 7 an active client the game refuses. ET drops its bots from a single-player
 // or co-op game as well as when told to, with no reason, and begins the rest
 // 500 ms after the level's start.
 const bool dropBots=killBots||(et&&(type==0||type==1));
 if(ihv)want.push_back({"ihvreconnect",0,0,time,""});
 for(int slot=2;slot<8&&!ihv;++slot){
  const bool bot=slot==5||slot==6;
  // Q3Test frees its bots' slots and tells the game nothing.
  if(bot&&q3test)continue;
  if(bot&&dropBots){want.push_back({"drop",slot,0,time,et?"":"was kicked"});continue;}
  want.push_back({"connect",slot,bot?1:0,time,""});
  if(slot==7)want.push_back({"drop",slot,0,time,"refused"});
  else if(bot)want.push_back({"enter",slot,game-start,time,""});
 }
 if(q3test){
  // Only 1.08 runs frames after reconnecting its clients: three, with no bots.
  for(int i=0;i<(p==Q3TEST_1_08?3:0);++i){want.push_back({"frame",time,0,time,""});time+=100;}
 }else if(!ihv&&!alice&&!mohaa){
  want.push_back({"frame",time,0,time,""});
  want.push_back({"botframe",time,0,time,""});
 }
 return want;
}
int main(){
 for(int p=0;p<PROFILES;++p)for(int type=0;type<6;++type)for(int killBots=0;killBots<2;++killBots){
  profile=p;gametype.integer=type;
  events.clear();waits=0;
  // GAME_INIT was given sv.time as the level's start.
  sv.time=initTime=lastFrame=10000;svs.time=50000;
  const int states[8]={CS_FREE,CS_ZOMBIE,CS_CONNECTED,CS_PRIMED,CS_ACTIVE,CS_ACTIVE,CS_PRIMED,CS_ACTIVE};
  for(int i=0;i<8;++i){
   clients[i]={states[i],{{i==5||i==6?NA_BOT:NA_IP}},GSA_ACKED,&clients[i],i==7};
   std::snprintf(clients[i].name,sizeof clients[i].name,"player%d",i);
  }
  Spawn(killBots,1);
  const std::vector<Event> want=Expected(p,type,killBots);
  if(events!=want){
   for(size_t i=0;i<events.size()||i<want.size();++i){
    if(i<events.size())std::printf("got  %s %d %d %d \"%s\"\n",events[i].kind.c_str(),events[i].a,events[i].b,events[i].time,events[i].reason.c_str());
    if(i<want.size())std::printf("want %s %d %d %d \"%s\"\n",want[i].kind.c_str(),want[i].a,want[i].b,want[i].time,want[i].reason.c_str());
   }
   std::printf("profile %d, game type %d, killBots %d\n",p,type,killBots);
   return 1;
  }
  const bool et=p==WOLFET, ihv=p==Q3_IHV&&ihvPath, alice=(p==ALICE||p==FAKK2)&&alicePath, mohaa=p==MOHAA&&mohaaPath;
  const bool q3test=(p==Q3TEST||p==Q3TEST_1_08)&&q3testPath, q3test108=q3test&&p==Q3TEST_1_08;
  // ET leaves the level 700 ms on, Quake III's load 400 ms and the IHV
  // test's 300 ms. Each frame counts down a console wait, and a last frame
  // takes 100 ms of the server's own clock. Alice's and Medal of Honor's
  // games run their own frames, and neither has a last frame here. Q3Test
  // leaves at 300 ms, but for 1.08's three last frames, which count no wait.
  const int end=et?10700:ihv?10300:alice||mohaa?10000:q3test?(q3test108?10600:10300):10400;
  const int serverEnd=ihv||alice||mohaa?50000:q3test?(q3test108?50300:50000):50100;
  if(sv.time!=end||svs.time!=serverEnd||waits!=(et?7:alice||mohaa?1:4)){
   std::printf("profile %d: sv.time %d, svs.time %d, %d waits\n",p,sv.time,svs.time,waits);
   return 1;
  }
  // Each person the game takes back waits for the new gamestate; the IHV
  // test's sessions keep their own records.
  for(int i=2;i<5&&!ihv;++i)assert(clients[i].state==CS_CONNECTED&&clients[i].gamestateAck==GSA_INIT&&!clients[i].gentity);
  assert(clients[0].state==CS_FREE&&clients[1].state==CS_ZOMBIE&&(ihv||clients[7].state==CS_ZOMBIE));
  // Q3Test's bots leave their slots free and nameless.
  if(q3test&&!(clients[5].state==CS_FREE&&clients[6].state==CS_FREE&&!clients[5].name[0]&&!clients[6].name[0])){
   std::printf("profile %d: a bot kept its slot or name\n",p);
   return 1;
  }
 }
 return 0;
}
'''
# Each mutation must make the checks fail. ET's frames and every other
# profile's now read alike, so each anchor starts with text only one of them has.
FRAME = ('\t\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n\t\t\tSV_BotFrame( sv.time );\n'
         '\t\t\tsv.time += 100;\n')
LAST_FRAME = ('\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n\t\tSV_BotFrame( sv.time );\n'
              '\t\tsv.time += 100;\n\t\tsvs.time += 100;')
ET_LOOP = 'SV_Indices( 6 ) ) {\n\t\t\t(void)ignored;\n\t\t\tCbuf_Wait();\n'
OTHER_LOOP = 'SV_Indices( 3 ) ) {\n\t\t\t(void)ignored;\n\t\t\tCbuf_Wait();\n'
ET_LAST = 'etded.exe 0x42d244..0x42d26a).\n'
OTHER_LAST = 'jk2mp.exe\n\t\t// 0x44db99..0x44dbc8).\n'
MUTATIONS = {
    'ET settles in three frames': ('SV_Indices( 6 )', 'SV_Indices( 3 )'),
    'ET advances the time before each settling frame': (
        ET_LOOP + FRAME, ET_LOOP + '\t\t\tsv.time += 100;\n\t\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n'
                                   '\t\t\tSV_BotFrame( sv.time );\n'),
    'ET runs its bots before its game': (
        ET_LOOP + FRAME, ET_LOOP + '\t\t\tSV_BotFrame( sv.time );\n\t\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n'
                                   '\t\t\tsv.time += 100;\n'),
    'ET settles without its bots': (
        ET_LOOP + FRAME, ET_LOOP + '\t\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n\t\t\tsv.time += 100;\n'),
    'every profile settles as ET does': (
        'Com_GameIsWolfET() ) {\n\t\t// Enemy Territory runs six', 'true ) {\n\t\t// Enemy Territory runs six'),
    'ET advances the time before its last frame': (
        ET_LAST + LAST_FRAME, ET_LAST + '\t\tsv.time += 100;\n\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n'
                                        '\t\tSV_BotFrame( sv.time );\n\t\tsvs.time += 100;'),
    # Quake3e advanced the time before each frame, and so did every profile
    # but ET and the IHV test here until the retail order was read in every
    # other engine.
    'the others advance the time before each settling frame, as in Quake3e': (
        OTHER_LOOP + FRAME, OTHER_LOOP + '\t\t\tsv.time += 100;\n\t\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n'
                                         '\t\t\tSV_BotFrame( sv.time );\n'),
    'only the IHV test runs its three at the current time, as before': (
        OTHER_LOOP + FRAME, OTHER_LOOP + '\t\t\tif ( !Com_GameIsQuake3IHV() ) sv.time += 100;\n'
                                         '\t\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n\t\t\tSV_BotFrame( sv.time );\n'
                                         '\t\t\tif ( Com_GameIsQuake3IHV() ) sv.time += 100;\n'),
    'the others advance the time before their last frame, as in Quake3e': (
        OTHER_LAST + LAST_FRAME, OTHER_LAST + '\t\tsv.time += 100;\n\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n'
                                              '\t\tSV_BotFrame( sv.time );\n\t\tsvs.time += 100;'),
    'every profile runs its last frame as ET does': (
        "if ( Com_GameIsWolfET() ) {\n\t\t// Enemy Territory's runs", "if ( true ) {\n\t\t// Enemy Territory's runs"),
    'ET keeps its single-player and co-op bots': ('( wolfET && (', '( false && ('),
    'every profile drops its single-player and co-op bots': ('( wolfET && (', '( true && ('),
    'ET keeps its co-op bots': (
        '\n\t\t\t\t\t|| SV_WolfETGameTypeIsCoop( sv_gametype->integer ) ) ) ) {', ' ) ) ) {'),
    'ET keeps its single-player bots': (
        '( SV_GameTypeIsSinglePlayer( sv_gametype->integer )\n\t\t\t\t\t|| ', '( '),
    'ET keeps its bots when told to drop them': (
        'if ( killBots || ( wolfET', 'if ( ( !wolfET && killBots ) || ( wolfET'),
    'ET drops its bots with a reason': ('wolfET ? "" : "was kicked"', '"was kicked"'),
    'every profile drops its bots with no reason': ('wolfET ? "" : "was kicked"', '""'),
    'Medal of Honor settles in the engine\'s three frames': (
        'Com_GameIsMOHAA() ) {\n\t\t// Medal of Honor', 'false ) {\n\t\t// Medal of Honor'),
    'Medal of Honor runs a last frame': ('&& !Com_GameIsMOHAA() ) {', ') {'),
    'Q3Test asks its reloaded game about its bots': ('NA_BOT && Com_GameIsQ3Test() ) {', 'NA_BOT && false ) {'),
    "Q3Test keeps its bots' names": ('\t\t\t\tslot.client.name[0] = 0;\n', ''),
    'Q3Test runs the others\' last frame': (
        '} else if ( Com_GameIsQ3Test() ) {\n\t\t/* Q3Test runs no frame here',
        '} else if ( false ) {\n\t\t/* Q3Test runs no frame here'),
    'Q3Test 1.08 runs no last frames': ('Com_Q3TestRelease()->protocol == 40', 'false'),
    "every Q3Test release runs 1.08's last frames": ('Com_Q3TestRelease()->protocol == 40', 'true'),
    'the IHV test reconnects through the engine as well': (
        'if ( Com_GameIsQuake3IHV() ) {\n\t\t\tbreak;', 'if ( false ) {\n\t\t\tbreak;'),
    'the IHV test reconnects only through the engine': (
        'if ( Com_GameIsQuake3IHV() ) {\n\t\tSV_IHVNetReconnectClients();', 'if ( false ) {\n\t\tSV_IHVNetReconnectClients();'),
}


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def settle_block(init):
    """The production block of SV_SpawnServer from its settling frames to its
    last frame."""
    spawn = init[init.index('void SV_SpawnServer( const char *mapname, qboolean killBots ) {'):]
    start = spawn.index('\t// run a few frames to allow everything to settle\n')
    return spawn[start:spawn.index("\t// Publish the selected profile's native module paks", start)]


class WolfETMapLoadSettleTests(unittest.TestCase):
    def run_checks(self, block, folder, name):
        # The IHV test, Alice, Medal of Honor and Q3Test take their own paths
        # where this tree has them.
        paths = ('static const bool ihvPath = %s, alicePath = %s, mohaaPath = %s, q3testPath = %s;\n'
                 % (str('Com_GameIsQuake3IHV()' in block).lower(), str('SV_AliceSettle(' in block).lower(),
                    str('SV_MOHAASettle(' in block).lower(), str('Com_GameIsQ3Test()' in block).lower()))
        source = (PRELUDE + paths + 'static void Spawn( qboolean killBots, qboolean aliceRestored ) {\n'
                  '\tbool isBot;\n\t(void)aliceRestored;\n' + block + '}\n' + CHECKS)
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(source)
        built = subprocess.run([self.compiler, *compiler_target_args(self.compiler), '-std=c++17',
                                str(cpp), '-o', str(exe)], cwd=folder, capture_output=True, text=True)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        return subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True)

    def test_only_et_settles_in_six_frames_and_drops_solo_bots(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler unavailable')
        init = read('code/server/sv_init.cpp')
        block = settle_block(init)
        # The game has started, and the level's clients are reconnected
        # between the settling frames and the last one.
        spawn = init[init.index('void SV_SpawnServer( const char *mapname, qboolean killBots ) {'):]
        self.assertLess(spawn.index('SV_InitGameProgs();'), spawn.index(block))
        self.assertIn('SV_ClientEnterWorld( &slot.client, nullptr );', block)
        scratch = ROOT / '.tmp/wolfet-map-load-settle/tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='settle-', dir=scratch) as directory:
            folder = Path(directory)
            ran = self.run_checks(block, folder, 'production')
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            for index, (description, (old, new)) in enumerate(MUTATIONS.items()):
                with self.subTest(mutation=description):
                    self.assertEqual(block.count(old), 1, old)
                    mutated = self.run_checks(block.replace(old, new), folder, f'mutation{index}')
                    self.assertNotEqual(mutated.returncode, 0, description)


if __name__ == '__main__':
    unittest.main()
