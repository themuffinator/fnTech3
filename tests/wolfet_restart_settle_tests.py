"""How map_restart settles the restarted game and brings its clients back.

Every retail engine runs each settling frame at the current time and advances
the time after it, so the game's first frame runs at the time GAME_INIT was
given; Quake3e, which fnTech3's restart came from, advanced it first. Quake III
1.32 (quake3.exe 0x432ab1..0x432add), Quake Live (quakelive_steam.exe
0x4de8ae..0x4de8e7), RTCW (WolfSP.exe 0x430993..0x4309bf, WolfMP.exe
0x4330ad..0x4330da), Holomatch (stvoyHM.exe 0x42e796..0x42e7c2) and the Jedi
multiplayer engines (jamp.exe 0x456396..0x4563c6, jk2mp.exe
0x447c03..0x447c2f) run three and then their last frame the same way. Enemy
Territory 2.60b (et.exe 0x43fa20, etded.exe 0x425e70) runs six (et.exe
0x43fc3b..0x43fc63) and its last (0x43fd57..0x43fd65). In a single-player or
co-op game type it leaves bots out of the restart altogether
(0x43fca1..0x43fcbe). The single-player engines have no map_restart, and
their map loads settle the same way. This compiles the production block of
SV_MapRestart_f, from the game restart to its last frame, and runs it for all
thirteen profiles.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
enum { Q3, QL, RTCW_SP, RTCW_MP, WOLFET, STVEF_SP, STVEF_MP, Q3_IHV, JKA_SP, JKA_MP, JK2_SP, JK2_MP, ALICE, PROFILES };
static int profile;
int Com_GameIsRTCWSP(){return profile==RTCW_SP;}
int Com_GameIsRTCWMP(){return profile==RTCW_MP;}
int Com_GameIsWolfET(){return profile==WOLFET;}
// Game types 0 and 1 are single player and co-op, as a gameinfo.dat can name
// them for ET. Every profile gets the same answers, so only ET acting on them
// shows that the restart asks for ET alone.
struct Cvar { int integer; };
static Cvar gametype;
static Cvar *sv_gametype=&gametype;
bool SV_GameTypeIsSinglePlayer(int type){return type==0;}
bool SV_WolfETGameTypeIsCoop(int type){return type==1;}
enum { CS_FREE, CS_ZOMBIE, CS_CONNECTED, CS_PRIMED, CS_ACTIVE };
enum { NA_BAD, NA_BOT, NA_LOOPBACK, NA_IP };
enum { SS_DEAD, SS_LOADING, SS_GAME };
enum { GAME_INIT, GAME_SHUTDOWN, GAME_CLIENT_CONNECT, GAME_CLIENT_BEGIN, GAME_RUN_FRAME=8 };
static constexpr int qfalse=0;
struct client_t { int state; struct { struct { int type; } remoteAddress; } netchan; bool refused; };
struct SV_ClientSlot { client_t &client; int index; };
static client_t clients[8];
std::vector<SV_ClientSlot> SV_ClientSlots(){std::vector<SV_ClientSlot> slots;for(int i=0;i<8;++i)slots.push_back({clients[i],i});return slots;}
std::vector<int> SV_Indices(int count){std::vector<int> indices;for(int i=0;i<count;++i)indices.push_back(i);return indices;}
static struct { int time, state, restarting; } sv;
static struct { int time; } svs;
static void *gvm=&sv;
// Every call the block makes, in order: its kind, two values and sv.time.
struct Event {
 std::string kind; int a, b, time;
 bool operator==(const Event &o) const {return kind==o.kind&&a==o.a&&b==o.b&&time==o.time;}
};
static std::vector<Event> events;
static int initTime, lastFrame, waits, latchedType;
// The restarted game registers its cvars, which takes a g_gametype the
// console latched; its GAME_INIT is given sv.time as the level's start.
void SV_RestartGameProgs(){gametype.integer=latchedType;initTime=sv.time;events.push_back({"init",0,0,sv.time});}
void SV_BotFrame(int time){events.push_back({"botframe",time,0,sv.time});}
void Cbuf_Wait(){++waits;}
void Cvar_Set(const char *name,const char *value){events.push_back({std::string(name)+"="+value,0,0,sv.time});}
intptr_t VM_Call(void *vm,int count,int call,...){
 assert(vm==gvm);
 va_list args;va_start(args,call);intptr_t result=0;
 if(call==GAME_RUN_FRAME){
  assert(count==1);lastFrame=va_arg(args,int);events.push_back({"frame",lastFrame,0,sv.time});
 }else if(call==GAME_CLIENT_CONNECT){
  assert(count==3&&sv.state==SS_GAME&&!sv.restarting);
  const int index=va_arg(args,int),firstTime=va_arg(args,int),isBot=va_arg(args,int);
  assert(!firstTime);events.push_back({"connect",index,isBot,sv.time});result=clients[index].refused;
 }else assert(!"unexpected game call");
 va_end(args);return result;
}
const void *GVM_ArgPtr(intptr_t refused){return refused?"refused":nullptr;}
void SV_AddServerCommand(client_t *client,const char *text){
 assert(std::string(text)=="map_restart\n");events.push_back({"command",int(client-clients),0,sv.time});
}
void SV_DropClient(client_t *client,const char *reason){
 assert(std::string(reason)=="refused");events.push_back({"drop",int(client-clients),0,sv.time});
}
void Com_Printf(const char *,...){}
// Entering the world begins the client in the game. ET's ClientBegin sends a
// team player to limbo once the game's time is more than 600 ms past the
// level's start (qagame_mp_x86.dll 0x2004010e..0x20040120).
void SV_ClientEnterWorld(client_t *client,const void *cmd){
 assert(!cmd);events.push_back({"enter",int(client-clients),lastFrame-initTime,sv.time});
}
'''
CHECKS = r'''
static std::vector<Event> Expected(int p,int type){
 const bool et=p==WOLFET, wolf=p==RTCW_MP||et, soloBots=et&&(type==0||type==1);
 const int start=10000, frames=et?6:3, settled=start+frames*100;
 std::vector<Event> want;
 if(wolf)want.push_back({"sv_serverRestarting=1",0,0,start});
 want.push_back({"init",0,0,start});
 // RTCW SP's bot frame comes first, at the time the game was given.
 if(p==RTCW_SP)want.push_back({"botframe",start,0,start});
 // Every profile runs each frame at the current time, then advances it, so
 // the first frame is given the level's start.
 for(int i=0;i<frames;++i)want.push_back({"frame",start+i*100,0,start+i*100});
 // Slots: 2 connected, 3 loading, 4 active, 5 an active bot, 6 a loading bot,
 // 7 an active client the game refuses. Only an active client enters: 200 ms
 // after the level's start after three frames, and 500 ms after it after ET's
 // six, as et.exe does.
 for(int slot=2;slot<8;++slot){
  const bool bot=slot==5||slot==6;
  if(bot&&soloBots)continue;
  want.push_back({"command",slot,0,settled});
  want.push_back({"connect",slot,bot?1:0,settled});
  if(slot==7)want.push_back({"drop",slot,0,settled});
  else if(slot==4||slot==5)want.push_back({"enter",slot,(frames-1)*100,settled});
 }
 want.push_back({"frame",settled,0,settled});
 if(wolf)want.push_back({"sv_serverRestarting=0",0,0,settled+100});
 return want;
}
int main(){
 for(int p=0;p<PROFILES;++p)for(int before=0;before<6;++before)for(int after=0;after<6;++after){
  profile=p;gametype.integer=before;latchedType=after;
  events.clear();waits=0;lastFrame=-1;
  sv.time=10000;sv.state=SS_LOADING;sv.restarting=1;svs.time=50000;
  const int states[8]={CS_FREE,CS_ZOMBIE,CS_CONNECTED,CS_PRIMED,CS_ACTIVE,CS_ACTIVE,CS_PRIMED,CS_ACTIVE};
  for(int i=0;i<8;++i)clients[i]={states[i],{{i==5||i==6?NA_BOT:NA_IP}},i==7};
  Restart();
  // The game type that counts is the one the restarted game runs.
  const std::vector<Event> want=Expected(p,after);
  if(events!=want){
   for(size_t i=0;i<events.size()||i<want.size();++i){
    if(i<events.size())std::printf("got  %s %d %d %d\n",events[i].kind.c_str(),events[i].a,events[i].b,events[i].time);
    if(i<want.size())std::printf("want %s %d %d %d\n",want[i].kind.c_str(),want[i].a,want[i].b,want[i].time);
   }
   std::printf("profile %d, game type %d then %d\n",p,before,after);
   return 1;
  }
  // ET leaves the level 700 ms on, Quake III's restart 400 ms. Each frame
  // counts down a console wait, and the server's own clock takes 100 ms.
  assert(sv.time==(p==WOLFET?10700:10400)&&svs.time==50100);
  assert(waits==(p==WOLFET?7:4)&&sv.state==SS_GAME&&!sv.restarting);
 }
 return 0;
}
'''
# Each mutation must make the checks fail.
MUTATIONS = {
    'ET settles in three frames': (
        'SV_Indices( wolfETFrames ? 6 : 3 )', 'SV_Indices( 3 )'),
    'every profile settles in six frames': (
        'SV_Indices( wolfETFrames ? 6 : 3 )', 'SV_Indices( 6 )'),
    # Quake3e advanced the time before each frame. So did this restart, for
    # every profile but ET, until the retail order was found in every engine.
    'the time advances before each settling frame, as in Quake3e': (
        '\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n\t\tsv.time += 100;',
        '\t\tsv.time += 100;\n\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );'),
    'the time advances before the last frame, as in Quake3e': (
        '\n\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n\tsv.time += 100;',
        '\n\tsv.time += 100;\n\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );'),
    'only ET runs its frames at the current time': (
        '\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n\t\tsv.time += 100;',
        '\t\tif ( !wolfETFrames ) sv.time += 100;\n\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );\n'
        '\t\tif ( wolfETFrames ) sv.time += 100;'),
    'ET brings bots back in single player and co-op': (
        'if ( isBot && Com_GameIsWolfET() && (', 'if ( false && ('),
    'every profile leaves bots out in single player and co-op': (
        'isBot && Com_GameIsWolfET() && (', 'isBot && ('),
    'ET leaves people out too': (
        'if ( isBot && Com_GameIsWolfET() && (', 'if ( Com_GameIsWolfET() && ('),
    'ET brings co-op bots back': (
        '\n\t\t\t|| SV_WolfETGameTypeIsCoop( sv_gametype->integer ) ) ) {', ' ) ) {'),
    'ET brings single-player bots back': (
        '( SV_GameTypeIsSinglePlayer( sv_gametype->integer )\n\t\t\t|| ', '( '),
    # et.exe enters every client it reconnects and relies on its serverId check
    # to send them a gamestate, as the other retail engines read do. This server
    # would never send one to a connected client entered here, nor resend a
    # loading client's lost one (2026-09-24-wolfet-restart-settle.md).
    'every reconnected client enters, as in et.exe': (
        'if ( slot.client.state == CS_ACTIVE ) {', 'if ( slot.client.state >= CS_CONNECTED ) {'),
}


def restart_block(command):
    """The production block from the game restart to its last frame."""
    start = command.index('\tif ( Com_GameIsRTCWMP() || Com_GameIsWolfET() ) Cvar_Set("sv_serverRestarting", "1");')
    marker = '\tif ( Com_GameIsRTCWMP() || Com_GameIsWolfET() ) Cvar_Set("sv_serverRestarting", "0");\n'
    return command[start:command.index(marker, start) + len(marker)]


class WolfETRestartSettleTests(unittest.TestCase):
    def run_checks(self, block, folder, name):
        source = PRELUDE + 'static void Restart() {\n\tint delay = 0;\n' + block + '}\n' + CHECKS
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(source)
        built = subprocess.run([self.compiler, *compiler_target_args(self.compiler), '-std=c++17',
                                str(cpp), '-o', str(exe)], cwd=folder, capture_output=True, text=True)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        return subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True)

    def test_only_et_settles_in_six_frames_and_leaves_solo_bots_out(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler unavailable')
        command = function((ROOT / 'code/server/sv_ccmds.cpp').read_text(), 'static void SV_MapRestart_f(')
        block = restart_block(command)
        # The restart spawns its active clients before it moves their command
        # gate, which follows the block.
        self.assertLess(command.index(block) + len(block), command.index('client.lastUsercmd.serverTime ='))
        scratch = ROOT / '.tmp/wolfet-restart-settle/tests'
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
