"""Execute RTCW save envelope, staging and load-command failure boundaries."""
from pathlib import Path
import os
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
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include "code/compat/rtcw_savegame.hpp"
using fileHandle_t=int;
using qboolean=bool;
constexpr int qtrue=1;
constexpr int qfalse=0,SS_GAME=2,GAME_CONSOLE_COMMAND=9;
static bool sp=true,shortRead=false,shortWrite=false,renameFails=false;
static int openCount,writeCount,restarts;
static std::map<std::string,std::vector<unsigned char>> files;
struct Handle {std::string name;};
static std::map<int,Handle> handles;
static std::map<std::string,std::string> cvars;
static std::vector<std::string> arguments;
static std::string queued;
struct {int time=17,restartedServerId=0,restartTime=0,state=SS_GAME;} sv;
struct fntech3_rtcw_sp_playerState_t {struct {int eFlags;} common;};
static struct {bool readable=true;int flags=0;
 bool ReadPlayerState(int index,fntech3_rtcw_sp_playerState_t &state){assert(index==0);state.common.eFlags=flags;return readable;}
} serverGameData;
static int gvm=1,gameCommands;
int VM_Call(int vm,int arguments,int call){assert(vm==1&&!arguments&&call==GAME_CONSOLE_COMMAND);++gameCommands;return 1;}
bool SV_QBool(bool v){return v;}
struct Running {int integer=1;} running;
static Running *com_sv_running=&running;
static int com_frameTime=10;
int Com_GameIsRTCWSP(){return sp;}
int Cvar_VariableIntegerValue(const char *s){return std::atoi(cvars[s].c_str());}
const char *Cvar_VariableString(const char *s){return cvars[s].c_str();}
void Cvar_Set(const char *s,const char *v){cvars[s]=v;}
void Cvar_SetSafe(const char *s,const char *v){Cvar_Set(s,v);}
void Com_DPrintf(const char*,...){}
int FS_FOpenFileRead(const char *s,int *f,int){*f=0;auto it=files.find(s);if(it==files.end())return -1;
 *f=++openCount;handles[*f]={s};return int(it->second.size());}
int FS_FOpenFileWrite(const char *s){int f=++openCount;handles[f]={s};files[s].clear();++writeCount;return f;}
void FS_FCloseFile(int f){assert(handles.erase(f)==1);}
int FS_Read(void *p,int n,int f){assert(handles.count(f));auto &b=files[handles[f].name];
 n=std::min(n,int(b.size()));if(shortRead&&n)--n;std::memcpy(p,b.data(),n);return n;}
int FS_Write(const void *p,int n,int f){assert(handles.count(f));int count=shortWrite?n/2:n;
 auto *b=static_cast<const unsigned char*>(p);files[handles[f].name]={b,b+count};return shortWrite?0:n;}
void FS_VM_RenameFile(const char *a,const char *b){assert(handles.empty());
 if(renameFails)throw std::runtime_error("replace failed");files[b]=files.at(a);files.erase(a);}
void Com_Printf(const char*,...){}
int Cmd_Argc(){return int(arguments.size());}
const char *Cmd_Argv(int n){return n<int(arguments.size())?arguments[n].c_str():"";}
int Q_stricmp(const char *a,const char *b){return std::strcmp(a,b);}
const char *va(const char *f,...){static char s[1024];va_list a;va_start(a,f);vsnprintf(s,sizeof(s),f,a);va_end(a);return s;}
void Cbuf_AddText(const char *s){queued+=s;}
void SV_MapRestart_f(){++restarts;}
#include "code/server/server_rtcw_save.hpp"
'''
CHECKS = r'''
using namespace fntech3::rtcw::savegame;
static std::vector<unsigned char> save(){std::vector<unsigned char>b(256,0x5a);b[0]=18;b[1]=b[2]=b[3]=0;
 std::memset(b.data()+4,0,64);std::memcpy(b.data()+4,"escape2",7);b[68]=0x34;b[69]=0x12;b[70]=b[71]=0;return b;}
static void reset(){files.clear();handles.clear();cvars.clear();queued.clear();arguments={"loadgame","slot"};
 sp=true;shortRead=shortWrite=renameFails=false;restarts=writeCount=0;running.integer=1;sv={17,0,0};}
int main(){
 char path[PathBytes];
 for(const char *s:{"slot","slot.svg","save/slot","save\\slot.svg","SAVE/slot.SVG"}){
  assert(Filename(s,path)&&!std::strcmp(path,"save/slot.svg"));}
 assert(Filename("autosave\\escape1",path)&&!std::strcmp(path,"save/autosave/escape1.svg"));
 for(const char *s:{"","/slot","../slot","save/../slot","save//slot","C:\\slot","slot;quit","slot\nquit","slot\"","a.svg/../b","a.exe","save/"}){
  std::strcpy(path,"untouched");assert(!Filename(s,path)&&!std::strcmp(path,"untouched"));}
 assert(!Filename(nullptr,path));assert(!Filename(std::string(1000,'x').c_str(),path));
 auto b=save();Header h;assert(Parse(b.data(),b.size(),h)&&h.time==0x1234&&!std::strcmp(h.map,"escape2"));
 for(unsigned n=0;n<PrefixBytes;++n){Header v=h;assert(!Parse(b.data(),n,v)&&v.time==h.time);}
 for(int version:{13,14,15,17,18}){b[0]=version;assert(Parse(b.data(),b.size(),h));}
 for(int version:{0,12,16,19,255}){b[0]=version;assert(!Parse(b.data(),b.size(),h));}
 b=save();std::memset(b.data()+4,'a',64);assert(!Parse(b.data(),b.size(),h));
 for(const char *map:{"../escape2","a;quit","/a","a//b","a.b",""}){b=save();std::strcpy(reinterpret_cast<char*>(b.data()+4),map);assert(!Parse(b.data(),b.size(),h));}
 b=save();std::memset(b.data()+68,255,4);assert(!Parse(b.data(),b.size(),h));
 reset();files["save/slot.svg"]=save();files["save/current.svg"]={1,2,3};
 assert(SV_RTCWReadSaveHeader("save/slot.svg",h));const auto old=files["save/current.svg"];
 shortRead=true;assert(!SV_RTCWStageSave("save/slot.svg",h)&&files["save/current.svg"]==old&&handles.empty());
 shortRead=false;shortWrite=true;assert(!SV_RTCWStageSave("save/slot.svg",h)&&files["save/current.svg"]==old&&handles.empty());
 shortWrite=false;renameFails=true;try{SV_RTCWStageSave("save/slot.svg",h);assert(false);}catch(const std::runtime_error&){}
 assert(files["save/current.svg"]==old&&handles.empty());renameFails=false;
 assert(SV_RTCWStageSave("save/slot.svg",h)&&files["save/current.svg"]==save());
 assert(SV_RTCWStageSave("save/current.svg",h)&&files["save/current.svg"]==save());
 ++h.time;assert(!SV_RTCWStageSave("save/slot.svg",h));--h.time;
 cvars["savegame_loading"]="2";assert(SV_RTCWRestoreSaveTime()&&sv.time==0x1234);
 sp=false;sv.time=42;assert(SV_RTCWRestoreSaveTime()&&sv.time==42);sp=true;
 files["save/current.svg"]={1,2,3};assert(!SV_RTCWRestoreSaveTime()&&cvars["savegame_loading"]=="0"&&sv.time==42);
 reset();SV_LoadGame_f();assert(!restarts&&queued.empty()&&!writeCount&&handles.empty());
 files["save/slot.svg"]=save();cvars["mapname"]="escape2";SV_LoadGame_f();
 assert(restarts==1&&queued.empty()&&cvars["savegame_loading"]=="2"&&cvars["savegame_filename"]=="save/slot.svg");
 SV_LoadGame_f();assert(restarts==1);cvars["savegame_loading"]="0";cvars["g_reloading"]="1";SV_LoadGame_f();assert(restarts==1);
 cvars["g_reloading"]="0";sp=false;SV_LoadGame_f();assert(restarts==1);sp=true;
 arguments[1]="../slot";SV_LoadGame_f();assert(restarts==1&&queued.empty());
 arguments[1]="slot";cvars["mapname"]="escape1";SV_LoadGame_f();assert(queued=="spmap \"save/slot.svg\"\n"&&restarts==1);
 queued.clear();running.integer=0;cvars["sv_cheats"]="1";SV_LoadGame_f();assert(queued=="spdevmap \"save/slot.svg\"\n");
 assert(handles.empty());
 arguments={"savegame","slot"};sv.state=SS_GAME;
 serverGameData.flags=ViewingCamera;gameCommands=0;assert(SV_GameCommand()&&!gameCommands);
 serverGameData.flags=0;assert(SV_GameCommand()&&gameCommands==1);
 serverGameData.readable=false;assert(SV_GameCommand()&&gameCommands==1);
 serverGameData.readable=true;serverGameData.flags=ViewingCamera;
 arguments={"status"};assert(SV_GameCommand()&&gameCommands==2);
 arguments={"savegame","slot"};sp=false;assert(SV_GameCommand()&&gameCommands==3);
 sv.state=0;assert(!SV_GameCommand()&&gameCommands==3);
 sp=true;cvars["g_episode"]="3";SV_SetGameCvar("g_episode","(null)");assert(cvars["g_episode"]=="3");
 SV_SetGameCvar("g_episode","4");assert(cvars["g_episode"]=="4");
 SV_SetGameCvar("other","(null)");assert(cvars["other"]=="(null)");
 sp=false;SV_SetGameCvar("g_episode","(null)");assert(cvars["g_episode"]=="(null)");
}
'''

class RTCWSaveGameTests(unittest.TestCase):
    def test_save_envelope_staging_and_load_command(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/rtcw-campaign/tests'; scratch.mkdir(parents=True, exist_ok=True)
        source = PRELUDE + function((ROOT / 'code/server/sv_ccmds.cpp').read_text(), 'static void SV_LoadGame_f(')
        game = (ROOT / 'code/server/sv_game.cpp').read_text()
        source += function(game, 'qboolean SV_GameCommand(')
        source += function(game, 'static void SV_SetGameCvar(') + CHECKS
        with tempfile.TemporaryDirectory(prefix='save-', dir=scratch) as directory:
            folder = Path(directory); cpp = folder / 'test.cpp'; exe = folder / 'test.exe'; cpp.write_text(source)
            target = compiler_target_args(compiler)
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(exe)], cwd=folder, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
