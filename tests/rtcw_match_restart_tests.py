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
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
struct { int restartedServerId, restartTime, time; } sv;
struct Cvar { int integer; } running;
static Cvar *com_sv_running=&running;
static int com_frameTime=99, profile=3, configIndex, restoreCalls;
static std::map<std::string,int> cvars;
static std::vector<std::string> args;
static std::string configValue;
static constexpr int CS_WARMUP=5;
int Com_GameIsRTCWSP(){return profile==2;}
int Com_GameIsRTCWMP(){return profile==3;}
int Com_GameIsRTCW(){return profile==2||profile==3;}
int Com_GameIsQuakeLive(){return profile==1;}
int Com_GameIsWolfET(){return profile==4;}
// ET's gameinfo.dat names game type 0 single-player and 1 cooperative.
static Cvar gametype;
static Cvar *sv_gametype=&gametype;
bool SV_GameTypeIsSinglePlayer(int type){return type==0;}
bool SV_WolfETGameTypeIsCoop(int type){return type==1;}
int Cmd_Argc(){return int(args.size());}
const char *Cmd_Argv(int index){return args.at(index).c_str();}
int SV_ParseInt(const char *s){return std::stoi(s);}
int Cvar_VariableIntegerValue(const char *s){return cvars[s];}
void Cvar_SetIntegerValue(const char *s,int v){cvars[s]=v;}
void Com_Printf(const char*,...){}
bool SV_HandleQuitOnExitLevel(const char*){return false;}
bool SV_RTCWRestoreSaveTime(){++restoreCalls;return true;}
const char *va(const char *fmt,...){static char b[64];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);return b;}
void SV_SetConfigstring(int i,const char *s){configIndex=i;configValue=s;}
static void reset(int p){profile=p;sv={0,0,10000};running.integer=1;gametype.integer=4;cvars.clear();cvars["gamestate"]=1;args={"map_restart"};restoreCalls=0;configValue.clear();}
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
  reset(p);sv.restartTime=20000;SV_MapRestart_f();assert(sv.restartTime==20000&&!restoreCalls);
 }
 reset(2);cvars["savegame_loading"]=2;args={"map_restart","5"};SV_MapRestart_f();assert(!sv.restartTime&&restoreCalls==1);
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
        source = PRELUDE + prefix + '\n}\n' + CHECKS
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
