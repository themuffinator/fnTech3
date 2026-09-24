"""Execute MP match transitions and real restart-command preflight for all profiles."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include "code/compat/rtcw_match_state.hpp"
#include <cassert>
#include <array>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
struct { int restartedServerId, restartTime, time; } sv;
struct Cvar { int integer; } running;
static Cvar *com_sv_running=&running;
static int com_frameTime=99, profile=3, configIndex, restoreCalls, nativeSpawns;
static std::map<std::string,int> cvars;
static std::vector<std::string> args;
static std::string configValue;
static constexpr int CS_WARMUP=5;
int Com_GameIsRTCWSP(){return profile==2;}
int Com_GameIsRTCWMP(){return profile==3;}
int Com_GameIsRTCW(){return profile==2||profile==3;}
int Com_GameIsQuakeLive(){return profile==1;}
int Com_GameIsWolfET(){return profile==4;}
int Com_GameIsQuake3IHV(){return profile==5;}
static constexpr int MAX_QPATH=64,qfalse=0;
template<typename T,size_t N>int SV_ArraySize(const std::array<T,N>&){return static_cast<int>(N);}
void Q_strncpyz(char *out,const char *in,int capacity){std::snprintf(out,capacity,"%s",in);}
const char *Cvar_VariableString(const char *){return "test";}
void SV_SpawnServer(const char *map,int flag){assert(std::string(map)=="test"&&!flag);++nativeSpawns;}
// ET's gameinfo.dat names game type 0 single-player and 1 cooperative.
static Cvar gametype;
static Cvar *sv_gametype=&gametype;
bool SV_GameTypeIsSinglePlayer(int type){return type==0;}
bool SV_WolfETGameTypeIsCoop(int type){return type==1;}
// RTCW MP reads its world record's worldflags, and getting g_gametype takes a
// type the console latched.
static int worldFlags, worldReads, gameTypeGets, gameTypeSets, latchedGameType;
static bool latched;
static constexpr int CVAR_SERVERINFO=4, CVAR_LATCH=32;
struct GameProfile { struct { const char *gameType; } server; };
const GameProfile *Com_GameProfile(){static const GameProfile rtcwMp{{"5"}};return &rtcwMp;}
int SV_RTCWMPWorldflags(){++worldReads;return worldFlags;}
Cvar *Cvar_Get(const char *name,const char *value,int flags){
 assert(std::string(name)=="g_gametype"&&std::string(value)=="5"&&flags==(CVAR_SERVERINFO|CVAR_LATCH));
 ++gameTypeGets;if(latched)gametype.integer=latchedGameType;latched=false;return &gametype;}
int Cmd_Argc(){return int(args.size());}
const char *Cmd_Argv(int index){return args.at(index).c_str();}
int SV_ParseInt(const char *s){return std::stoi(s);}
int Cvar_VariableIntegerValue(const char *s){return cvars[s];}
void Cvar_SetIntegerValue(const char *s,int v){cvars[s]=v;if(std::string(s)=="g_gametype"){gametype.integer=v;++gameTypeSets;}}
void Com_Printf(const char*,...){}
bool SV_HandleQuitOnExitLevel(const char*){return false;}
bool SV_RTCWRestoreSaveTime(){++restoreCalls;return true;}
const char *va(const char *fmt,...){static char b[64];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);return b;}
void SV_SetConfigstring(int i,const char *s){configIndex=i;configValue=s;}
static void reset(int p){profile=p;sv={0,0,10000};running.integer=1;gametype.integer=4;cvars.clear();cvars["gamestate"]=1;args={"map_restart"};nativeSpawns=restoreCalls=0;configValue.clear();}
'''
CLIENTS = r'''
enum { CS_FREE, CS_ZOMBIE, CS_CONNECTED, CS_PRIMED, CS_ACTIVE };
struct client_t { int state, oldServerTime; };
static client_t slots[5];
client_t (&SV_Clients())[5]{return slots;}
'''
CHECKS = r'''
int main(){
 // Columns: playing, countdown, warmup, intermission, waiting, reset.
 // Each row states the resulting unlocked-team state; -99 means refusal.
 constexpr int expected[7][6]={
  {0,1,2,3,4,2}, {0,1,2,3,4,2}, {0,-99,2,3,4,2},
  {0,1,-99,3,4,2}, {2,-99,2,-99,-99,-99},
  {-99,-99,2,-99,-99,-99}, {-99,-99,2,-99,4,-99}
 };
 for(int locked:{0,1})for(int old=-1;old<=5;++old)for(int wanted=0;wanted<=5;++wanted){
  int result=777;bool accepted=fntech3::rtcw::RestartMatchState(old,wanted,locked,result);
  int want=expected[old+1][wanted];assert(accepted==(want!=-99));
  if(accepted)assert(result==(wanted==5&&locked?4:want));else assert(result==777);
  reset(3);cvars["gamestate"]=old;cvars["g_noTeamSwitching"]=locked;
  args={"map_restart","0",std::to_string(wanted)};SV_MapRestart_f();
  assert(!sv.restartTime&&cvars["gamestate"]==(accepted?result:old));
 }
 // ET's team game types keep MP's transitions, but resolve a reset to warmup
 // whatever the team lock.
 for(int locked:{0,1})for(int old=-1;old<=5;++old)for(int wanted=0;wanted<=5;++wanted){
  int result=777;bool accepted=fntech3::rtcw::RestartMatchState(old,wanted,false,result);
  reset(4);cvars["gamestate"]=old;cvars["g_noTeamSwitching"]=locked;
  args={"map_restart","0",std::to_string(wanted)};SV_MapRestart_f();
  assert(!sv.restartTime&&cvars["gamestate"]==(accepted?result:old));
  if(accepted&&wanted==5)assert(cvars["gamestate"]==2);
 }
 // ET's single-player and co-op restarts ignore the requested state and play
 // straight through, so an intermission cannot restart into play.
 for(int type:{0,1})for(int old=-1;old<=5;++old){
  reset(4);gametype.integer=type;cvars["gamestate"]=old;
  args={"map_restart","0","2"};SV_MapRestart_f();
  assert(!sv.restartTime&&cvars["gamestate"]==(old<=2?0:old));
 }
 // RTCW MP keeps g_gametype to a type the map allows, before any delay. Rows
 // are types 5 to 8 and columns worldflags 0 to 7, as WolfMP.exe sets them;
 // higher bits change nothing, and no other type is ever replaced.
 constexpr int kept[4][8]={{5,7,5,7,5,7,5,7},{6,6,5,7,6,6,5,7},{7,7,7,7,5,7,5,7},{8,8,8,8,5,7,5,7}};
 constexpr int replaced[4][8]={{0,1,0,1,0,1,0,1},{0,0,1,1,0,0,1,1},{0,0,0,0,1,1,1,1},{0,0,0,0,1,1,1,1}};
 for(int type=-1;type<=9;++type)for(int flags:{0,1,2,3,4,5,6,7,8,9,12,15,-8,-1})
 for(bool pending:{false,true})for(const char *delay:{"0","3"}){
  const bool wolf=type>=5&&type<=8, set=wolf&&replaced[type-5][flags&7];
  reset(3);worldFlags=flags;worldReads=gameTypeGets=gameTypeSets=0;
  gametype.integer=pending?4:type;latched=pending;latchedGameType=type;
  args={"map_restart",delay};SV_MapRestart_f();
  assert(gametype.integer==(wolf?kept[type-5][flags&7]:type)&&!latched&&worldReads==1);
  assert(gameTypeSets==set&&gameTypeGets==(set?2:1)&&sv_gametype==&gametype);
  assert(sv.restartTime==(delay[0]=='3'?13000:0)&&cvars["gamestate"]==(delay[0]=='3'?1:0));
 }
 // The replacement stands even when the match state then refuses the restart.
 reset(3);worldFlags=1;gametype.integer=5;cvars["gamestate"]=4;args={"map_restart","0","0"};
 SV_MapRestart_f();assert(gametype.integer==7&&cvars["gamestate"]==4);
 // The same-frame, stopped-server and pending-restart refusals come first, and
 // no other profile reads the world record or replaces a type.
 for(int refusal=0;refusal<3;++refusal){
  reset(3);worldFlags=1;worldReads=gameTypeGets=gameTypeSets=0;gametype.integer=5;latched=true;latchedGameType=6;
  if(refusal==0)sv.restartedServerId=com_frameTime;
  if(refusal==1)running.integer=0;
  if(refusal==2)sv.restartTime=20000;
  SV_MapRestart_f();assert(!worldReads&&!gameTypeGets&&!gameTypeSets&&gametype.integer==5&&latched);
 }
 for(int p:{0,1,2,4}){
  reset(p);worldFlags=7;worldReads=gameTypeGets=gameTypeSets=0;gametype.integer=5;latched=true;latchedGameType=6;
  args={"map_restart","0"};SV_MapRestart_f();
  assert(!worldReads&&!gameTypeGets&&!gameTypeSets&&gametype.integer==5&&latched);
 }
 worldFlags=0;latched=false;
 for(int invalid:{INT_MIN,-2,6,INT_MAX}){
  int result=777;assert(!fntech3::rtcw::RestartMatchState(invalid,0,false,result)&&result==777);
  assert(!fntech3::rtcw::RestartMatchState(0,invalid,false,result)&&result==777);
 }
 for(int p=0;p<5;++p){
  reset(p);SV_MapRestart_f();assert(sv.restartTime==(p<2?15000:0));
  assert(restoreCalls==(p>=2));assert(cvars["gamestate"]==(p>=3?0:1));
  reset(p);cvars["g_doWarmup"]=1;args={"map_restart","3"};SV_MapRestart_f();
  assert(sv.restartTime==(p>=3?13000:0));
  if(p>=3)assert(configValue=="13000"&&configIndex==5&&cvars["gamestate"]==1);
  reset(p);args={"map_restart","0"};SV_MapRestart_f();assert(!sv.restartTime&&restoreCalls==1);
  reset(p);sv.restartedServerId=com_frameTime;SV_MapRestart_f();assert(!sv.restartTime&&!restoreCalls);
  reset(p);running.integer=0;SV_MapRestart_f();assert(!sv.restartTime&&!restoreCalls);
  // Only ET lets a restart through while a delayed one is pending, as et.exe
  // does. An immediate one leaves the pending time to fire later, and a new
  // delay replaces it, earlier or later, and CS_WARMUP with it.
  reset(p);sv.restartTime=20000;SV_MapRestart_f();
  assert(sv.restartTime==20000&&restoreCalls==(p==4)&&cvars["gamestate"]==(p==4?0:1)&&configValue.empty());
  for(int pending:{12000,20000}){
   reset(p);sv.restartTime=pending;args={"map_restart","3"};SV_MapRestart_f();
   assert(sv.restartTime==(p==4?13000:pending)&&!restoreCalls&&cvars["gamestate"]==1);
   assert(p==4?configValue=="13000"&&configIndex==5:configValue.empty());
  }
 }
 // The level restart then gives a loading client no clock offset, though
 // ET's pending restart time is still set.
 sv.restartTime=20000;for(int s=0;s<5;++s)slots[s]={s,777};SV_PrimeRestartClocks();
 for(int s=0;s<5;++s)assert(slots[s].oldServerTime==(s==CS_PRIMED?0:777));
 reset(2);cvars["savegame_loading"]=2;args={"map_restart","5"};SV_MapRestart_f();assert(!sv.restartTime&&restoreCalls==1);
 // Native IHV uses a full level spawn and never publishes a retail warmup CS.
 reset(5);SV_MapRestart_f();assert(nativeSpawns==1&&!restoreCalls&&configValue.empty());
 reset(5);args={"map_restart","3"};SV_MapRestart_f();assert(!nativeSpawns&&sv.restartTime==13000&&configValue.empty());
}
'''


class RTCWMatchRestartTests(unittest.TestCase):
    def test_match_transitions_and_restart_preflight(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        command = function((ROOT / 'code/server/sv_ccmds.cpp').read_text(), 'static void SV_MapRestart_f(')
        # Compile the actual command up to its first VM/full-map restart branch.
        prefix = command[:command.index('\n\tif ( !Com_GameIsQuakeLive()')]
        # And the loop that sets a loading client's clock offset at the restart.
        start = command.index('\tfor ( client_t &client : SV_Clients() ) {\n\t\tif ( client.state == CS_PRIMED')
        primed = command[start:command.index('\n\t}\n', start) + 4]
        source = (PRELUDE + prefix + '\n}\n' + CLIENTS + 'static void SV_PrimeRestartClocks(){\n' + primed + '}\n'
                  + CHECKS)
        scratch = ROOT / '.tmp/rtcw-campaign/tests'; scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='restart-', dir=scratch) as directory:
            folder = Path(directory); cpp = folder / 'test.cpp'; exe = folder / 'test.exe'
            cpp.write_text(source)
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=folder, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        self.assertLess(command.index('Cvar_Set("sv_serverRestarting", "1")'), command.index('SV_RestartGameProgs();'))
        self.assertGreater(command.index('Cvar_Set("sv_serverRestarting", "0")'), command.rindex('GAME_RUN_FRAME'))
