"""Execute map/default policy and RTCW campaign allocation in all profiles."""
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
#include "code/server/server.h"
#include "code/compat/rtcw_savegame.hpp"
#include "code/compat/rtcw_match_state.hpp"
#include "code/compat/wolfet_gameinfo.hpp"
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <map>
#include <string>
static const comGameProfile_t *profile;
const comGameProfile_t *Com_GameProfile(){return profile;}
int Com_GameIsRTCWSP(){return profile->id==COM_GAME_RTCW_SP;}
int Com_GameIsRTCWMP(){return profile->id==COM_GAME_RTCW_MP;}
int Com_GameIsWolfET(){return profile->id==COM_GAME_WOLFET;}
int Com_GameIsMOHAA(){return profile->titleId==COM_GAME_TITLE_MOHAA;}
void QDECL Com_Error(errorParm_t,const char*,...){throw 7;}
int Com_GameIsQuakeLive(){return profile->id==COM_GAME_QUAKELIVE;}
int Com_GameIsJKAMP(){return profile->id==COM_GAME_JKA_MP;}
int Com_GameIsJK2MP(){return profile->id==COM_GAME_JK2_MP;}
int Com_GameIsSTVEFMP(){return profile->id==COM_GAME_STVEF_MP;}
// The map command asks the real single-player rule, but never ET's, whose
// gameinfo.dat has its own fixtures.
static const fntech3::wolfet::gameinfo::GameInfo &SV_WolfETGameInfo(){
 assert(false);static fntech3::wolfet::gameinfo::GameInfo info;return info;}
struct Value {cvar_t cvar{};std::string value,latched;};
static std::map<std::string,Value> values;
static void update(Value &v,const char *value){v.value=value;v.cvar.string=v.value.data();v.cvar.integer=atoi(value);}
cvar_t *Cvar_Get(const char *name,const char *text,int flags){
 auto [it,created]=values.try_emplace(name);auto &v=it->second;
 if(created)update(v,text);
 if(!v.latched.empty()){update(v,v.latched.c_str());v.latched.clear();v.cvar.latchedString=nullptr;}
 v.cvar.flags|=flags;return &v.cvar;
}
void Cvar_Set(const char *name,const char *text){auto &v=values[name];update(v,text);v.latched.clear();v.cvar.latchedString=nullptr;}
static bool SV_RTCWReadSaveHeader(const char*,fntech3::rtcw::savegame::Header&){assert(false);return false;}
static bool SV_RTCWStageSave(const char*,const fntech3::rtcw::savegame::Header&){assert(false);return false;}
static void SV_RTCWClearSaveRequest(){Cvar_Set("savegame_loading","0");Cvar_Set("savegame_filename","");}
const char *COM_GetExtension(const char *s){const char *p=strrchr(s,'.');return p?p+1:"";}
void Cvar_SetLatched(const char *name,const char *text){auto &v=values[name];v.latched=text;v.cvar.latchedString=v.latched.data();}
void Cvar_SetIntegerValue(const char *name,int value){Cvar_Set(name,std::to_string(value).c_str());}
int Cvar_VariableIntegerValue(const char *name){auto it=values.find(name);return it==values.end()?0:it->second.cvar.integer;}
cvar_t *sv_gametype,*sv_maxclients;
static cvar_t running{},dedicated{};
cvar_t *com_sv_running=&running,*com_dedicated=&dedicated,*com_developer;
void QDECL Com_Printf(const char *,...){}
int QDECL Com_sprintf(char *dest,int size,const char *format,...){va_list args;va_start(args,format);int n=vsnprintf(dest,size,format,args);va_end(args);return n;}
int Q_stricmp(const char *a,const char *b){return strcmp(a,b);}
int Q_stricmpn(const char *a,const char *b,int n){return strncmp(a,b,n);}
void Q_strncpyz(char *dest,const char *text,int capacity){assert(strlen(text)<(size_t)capacity);strcpy(dest,text);}
static const char *command="spdevmap",*map="synthetic";
static int argc=2,spawns,preflight,prepares,deactivates,pure,probes;
static bool available=true,activeFactory=false;
const char *Cmd_Argv(int n){return n==0?command:n==1?map:"factory";}
int Cmd_Argc(){return argc;}
void FS_BypassPure(){++pure;}
void FS_RestorePure(){--pure;}
int FS_FOpenFileRead(const char *name,fileHandle_t*,qboolean){assert(pure==1);++probes;return available?1:-1;}
void SV_FactoryPrintMapUsage(const char*){++preflight;}
qboolean SV_FactoryHasActive(){return activeFactory?qtrue:qfalse;}
qboolean SV_HandleQuitOnExitLevel(const char*){return qfalse;}
void SV_FactoryDeactivate(){++deactivates;}
qboolean SV_FactoryPrepareMap(const char*,const char*,qboolean,qboolean){++prepares;return qtrue;}
// ET's single-player switch and default type come from its gameinfo.dat.
static bool etSinglePlayer;
qboolean SV_WolfETSinglePlayerEnabled(){return etSinglePlayer?qtrue:qfalse;}
int SV_WolfETDefaultSinglePlayerGameType(){return 0;}
'''

CHECKS = r'''
int main(){
 for(const auto &p:profiles){profile=&p;
  if(p.id==COM_GAME_QUAKE3_IHV){
   values.clear();auto *mode=SV_RegisterPureMode();
   assert(mode->integer==0&&!(mode->flags&CVAR_LATCH));
   SV_TestServerDefaults();assert(sv_maxclients->integer==8&&p.maxClients==256);
   assert(sv_gametype->integer==0&&values["fraglimit"].cvar.integer==0);
   continue; // Native IHV map startup remains gated.
  }
  if(p.id==COM_GAME_WOLFET){
   // Retail ET starts pure and does not latch the cvar.
   values.clear();auto *mode=SV_RegisterPureMode();
   assert(mode->integer==1&&!(mode->flags&CVAR_LATCH)&&(mode->flags&CVAR_SYSTEMINFO));
   SV_TestServerDefaults();assert(sv_gametype->integer==4&&sv_maxclients->integer==20);
   Cvar_Set("sv_pure","0");assert(SV_RegisterPureMode()->integer==0);
   Cvar_Set("sv_pure","1");assert(SV_RegisterPureMode()->integer==1);
   // Single-player commands are refused, before any map lookup, unless the
   // gameinfo enables them; then they select its single-player type.
   for(const char *cmd:{"spmap","spdevmap"}){
    command=cmd;map="synthetic";values.clear();SV_TestServerDefaults();
    spawns=pure=probes=0;available=true;argc=2;etSinglePlayer=false;SV_Map_f();
    assert(!spawns&&!probes&&pure==0&&sv_gametype->integer==4);
    etSinglePlayer=true;SV_Map_f();assert(spawns==1&&probes==1&&sv_gametype->integer==0);
   }
   etSinglePlayer=false;
   // An ordinary map command keeps every ET game type -- its game reloads a
   // map in Objective, type 2 -- and starts from a fresh match state.
   for(const char *cmd:{"map","devmap"})for(int type:{0,2,4}){
    command=cmd;map="synthetic";values.clear();SV_TestServerDefaults();
    Cvar_Set("g_gametype",std::to_string(type).c_str());
    Cvar_Set("gamestate","3");Cvar_Set("g_currentRound","1");Cvar_Set("g_nextTimeLimit","7");
    spawns=pure=probes=0;available=true;argc=2;SV_Map_f();
    assert(spawns==1&&probes==1&&pure==0&&sv_gametype->integer==type);
    assert(values.at("gamestate").value=="-1"&&values.at("g_currentRound").value=="0");
    assert(values.at("g_nextTimeLimit").value=="0");
    assert(values["sv_cheats"].cvar.integer==(cmd[0]=='d'));
   }
   continue;
  }
  if(p.id==COM_GAME_STVEF_SP){
   // Elite Force SP serves one local client over loopback: never pure.
   values.clear();auto *mode=SV_RegisterPureMode();
   assert(mode->integer==0&&!(mode->flags&CVAR_LATCH)&&(mode->flags&CVAR_SYSTEMINFO));
   SV_TestServerDefaults();assert(sv_gametype->integer==0&&sv_maxclients->integer==1);
   assert(values["fraglimit"].cvar.integer==0&&!(values["fraglimit"].cvar.flags&CVAR_SERVERINFO));
   continue;
  }
  if(p.titleId==COM_GAME_TITLE_JKA||p.titleId==COM_GAME_TITLE_JK2){
   // Raven's own defaults, shared by both titles: no mode starts pure or
   // latches the cvar, and none owns a skill cvar. Single player's map
   // startup stays gated.
   const bool single=p.id==COM_GAME_JKA_SP||p.id==COM_GAME_JK2_SP;
   values.clear();auto *mode=SV_RegisterPureMode();
   assert(mode->integer==0&&!(mode->flags&CVAR_LATCH)&&(mode->flags&CVAR_SYSTEMINFO));
   Cvar_Set("sv_pure","1");assert(SV_RegisterPureMode()->integer==1);
   values.clear();SV_TestServerDefaults();
   assert(sv_gametype->integer==0&&sv_maxclients->integer==(single?1:8));
   assert(values["fraglimit"].cvar.integer==(single?0:20));
   assert(bool(values["fraglimit"].cvar.flags&CVAR_SERVERINFO)==!single);
   assert(values["timelimit"].cvar.flags&CVAR_SERVERINFO);
   assert(!values.count("g_gameskill"));
   // Each multiplayer engine's spmap selects its own single-player type,
   // Academy's 5 and Outcast's 4, where Quake III's 2 is Jedi Master. An
   // ordinary map keeps every Academy game type and turns Outcast's single
   // player alone into a free-for-all (jamp.exe 1.01 0x4560b0, jk2mp.exe
   // 1.02a 0x447910).
   if(!single){
    const bool academy=p.id==COM_GAME_JKA_MP;const int solo=academy?5:4;
    for(const char *cmd:{"spmap","spdevmap","map","devmap"})for(int type:{0,1,2,3,4,5,6}){
     command=cmd;map="synthetic";values.clear();SV_TestServerDefaults();
     Cvar_Set("g_gametype",std::to_string(type).c_str());
     spawns=pure=probes=0;available=true;argc=2;SV_Map_f();
     assert(spawns==1&&probes==1&&pure==0);
     assert(sv_gametype->integer==(cmd[0]=='s'?solo:!academy&&type==solo?0:type));
     assert(sv_maxclients->integer==8&&!values.count("gamestate"));
    }
   }
   continue;
  }
  if(p.titleId==COM_GAME_TITLE_ALICE||p.titleId==COM_GAME_TITLE_FAKK2){
   // No sv_pure registration was found in the retail binary, so the
   // profile claims none: the cvar starts at zero and unlatched. Its one
   // mode takes the one slot retail's SV_Init registers (0x0045567c) and
   // Quake's frag limit, and the engine owns no skill cvar here.
   // F.A.K.K.2's profile carries Alice's policy unchanged.
   values.clear();auto *mode=SV_RegisterPureMode();
   assert(mode->integer==0&&!(mode->flags&CVAR_LATCH)&&(mode->flags&CVAR_SYSTEMINFO));
   Cvar_Set("sv_pure","1");assert(SV_RegisterPureMode()->integer==1);
   values.clear();SV_TestServerDefaults();
   assert(sv_gametype->integer==0&&sv_maxclients->integer==1);
   assert(values["fraglimit"].cvar.integer==20);
   assert(values["fraglimit"].cvar.flags&CVAR_SERVERINFO);
   assert(values["timelimit"].cvar.flags&CVAR_SERVERINFO);
   assert(!values.count("g_gameskill"));
   continue;
  }
  if(p.id==COM_GAME_MOHAA){
   // Medal of Honor claims no sv_pure registration either, takes one slot
   // and Quake's frag limit, and owns no skill cvar.
   values.clear();auto *mode=SV_RegisterPureMode();
   assert(mode->integer==0&&!(mode->flags&CVAR_LATCH)&&(mode->flags&CVAR_SYSTEMINFO));
   Cvar_Set("sv_pure","1");assert(SV_RegisterPureMode()->integer==1);
   values.clear();SV_TestServerDefaults();
   assert(sv_gametype->integer==0&&sv_maxclients->integer==1);
   assert(values["fraglimit"].cvar.integer==20);
   assert(values["fraglimit"].cvar.flags&CVAR_SERVERINFO);
   assert(values["timelimit"].cvar.flags&CVAR_SERVERINFO);
   assert(!values.count("g_gameskill"));
   // It keeps Quake III's map commands, but its single player is game type
   // 0, which spmap selects, and Quake III's 2 is its team match: map and
   // devmap keep whatever type is set.
   for(const char *cmd:{"spmap","spdevmap","map","devmap"})for(int type:{0,2,4}){
    command=cmd;map="synthetic";values.clear();SV_TestServerDefaults();
    Cvar_Set("g_gametype",std::to_string(type).c_str());
    spawns=pure=probes=0;available=true;argc=2;SV_Map_f();
    assert(spawns==1&&probes==1&&pure==0);
    assert(sv_gametype->integer==(cmd[0]=='s'?0:type)&&sv_maxclients->integer==1);
    assert(values["sv_cheats"].cvar.integer==(std::string(cmd).find("dev")!=std::string::npos));
    assert(!values.count("gamestate")&&!values.count("g_gameskill"));
   }
   continue;
  }
  if(p.id==COM_GAME_Q3TEST){
   // No Q3Test engine registers sv_pure (1.08 SV_Init 0x0041d8f0), so the
   // profile claims none; it takes eight slots and Quake's frag limit, and
   // owns no skill cvar.
   values.clear();auto *mode=SV_RegisterPureMode();
   assert(mode->integer==0&&!(mode->flags&CVAR_LATCH)&&(mode->flags&CVAR_SYSTEMINFO));
   Cvar_Set("sv_pure","1");assert(SV_RegisterPureMode()->integer==1);
   values.clear();SV_TestServerDefaults();
   assert(sv_gametype->integer==0&&sv_maxclients->integer==8);
   assert(values["fraglimit"].cvar.integer==20);
   assert(values["fraglimit"].cvar.flags&CVAR_SERVERINFO);
   assert(values["timelimit"].cvar.flags&CVAR_SERVERINFO);
   assert(!values.count("g_gameskill"));
   continue;
  }
  const bool sp=p.id==COM_GAME_RTCW_SP,mp=p.id==COM_GAME_RTCW_MP,ql=p.id==COM_GAME_QUAKELIVE;
  const bool holomatch=p.id==COM_GAME_STVEF_MP;
  values.clear();auto *pureMode=SV_RegisterPureMode();assert(pureMode->integer==(sp?0:1));
  assert(pureMode->flags&CVAR_SYSTEMINFO);assert(bool(pureMode->flags&CVAR_LATCH)==!(sp||mp||holomatch));
  // Holomatch alone archives sv_pure and publishes it in its serverinfo
  // (stvoyHM.exe 1.20 SV_Init 0x433f88).
  assert(bool(pureMode->flags&CVAR_SERVERINFO)==holomatch&&bool(pureMode->flags&CVAR_ARCHIVE)==holomatch);
  Cvar_Set("sv_pure",sp?"1":"0");assert(SV_RegisterPureMode()->integer==(sp?1:0));
  Cvar_SetLatched("sv_pure","1");assert(SV_RegisterPureMode()->integer==1&&!pureMode->latchedString);
  values.clear();SV_TestServerDefaults();assert(sv_gametype->integer==(mp?5:0));
  assert(sv_maxclients->integer==(mp?20:8));
  assert(values["fraglimit"].cvar.integer==(mp?0:ql?50:20));
  assert(bool(values["fraglimit"].cvar.flags&CVAR_SERVERINFO)==!mp);
  assert(values["timelimit"].cvar.flags&CVAR_SERVERINFO);
  // Existing explicit settings survive default registration.
  Cvar_Set("sv_maxclients","41");Cvar_Set("g_gametype","4");Cvar_Set("g_gameskill","0");
  SV_TestServerDefaults();assert(sv_maxclients->integer==41&&sv_gametype->integer==4&&values["g_gameskill"].cvar.integer==0);
  for(const char *cmd:{"spmap","spdevmap","map","devmap"}){
   command=cmd;map="synthetic";values.clear();SV_TestServerDefaults();
   spawns=preflight=prepares=deactivates=pure=probes=0;available=true;activeFactory=false;argc=ql?3:2;
   bool spCommand=cmd[0]=='s';SV_Map_f();assert(spawns==1&&pure==0&&probes==1);
   assert(values["sv_cheats"].cvar.integer==(std::string(cmd).find("dev")!=std::string::npos));
   assert(sv_gametype->integer==(spCommand?2:mp?5:0));
   assert(sv_maxclients->integer==(spCommand?(sp?64:mp?32:8):(mp?20:8)));
   assert(prepares==(ql&&!spCommand)&&deactivates==(ql&&spCommand));
   if(mp){assert(values.at("gamestate").value=="-1");assert(values.at("sv_serverRestarting").value=="0");}
   else {assert(!values.count("gamestate")&&!values.count("sv_serverRestarting"));}
   if(sp||mp)assert(values["g_gameskill"].cvar.integer==(sp?1:3));
   if(spCommand&&sp)assert(values["bot_enable"].cvar.integer==1);
   for(const char *fog:{"r_mapFogColor","r_waterFogColor","r_savegameFogColor"}){
    if(sp)assert(values.at(fog).value=="0");else assert(!values.count(fog));
   }
  }
  // WolfSP.exe ignores every map command while its game is reloading,
  // unless the game starts its next level itself (2) or a build script
  // runs (0x430c6d). No other engine here has the guard.
  for(int reloading:{1,2,4,8,16})for(int build:{0,1})for(const char *cmd:{"spmap","spdevmap","map","devmap"}){
   command=cmd;map="synthetic";values.clear();SV_TestServerDefaults();
   Cvar_Set("g_reloading",std::to_string(reloading).c_str());Cvar_Set("com_buildScript",build?"1":"0");
   spawns=pure=probes=0;available=true;activeFactory=false;argc=ql?3:2;SV_Map_f();
   const int started=!(sp&&!build&&reloading!=2);
   assert(spawns==started&&probes==started&&pure==0);
  }
  // A nonexistent map cannot tear down the server or apply new cvars.
  values.clear();SV_TestServerDefaults();Cvar_Set("sv_maxclients","37");
  command="spdevmap";map="missing";available=false;spawns=0;SV_Map_f();
  assert(!spawns&&sv_maxclients->integer==37&&sv_gametype->integer==(mp?5:0)&&pure==0);
  assert(!values.count("gamestate")&&!values.count("sv_serverRestarting"));
  available=true;map="synthetic";
  if(ql){command="devmap";argc=2;preflight=spawns=probes=0;activeFactory=false;SV_Map_f();assert(preflight==1&&!spawns&&!probes);}
  // Direct campaign loads use the same preparation as map transitions; SP
  // consumes a pending slot count and forces AI on before shutdown.
  for(int slots:{1,8,32,64,100,128}){
   values.clear();SV_TestServerDefaults();Cvar_Set("g_gametype","2");Cvar_Set("bot_enable","0");
   Cvar_SetLatched("sv_maxclients",std::to_string(slots).c_str());
   SV_PrepareRTCWCampaign();
   if(sp){assert(!sv_maxclients->latchedString);assert(sv_maxclients->integer==(slots<128?64:128));assert(values["bot_enable"].cvar.integer==1);}
   else {assert(sv_maxclients->latchedString&&values["bot_enable"].cvar.integer==0);}
  }
 }
}
'''


class RTCWServerPolicyTests(unittest.TestCase):
    def test_defaults_map_aliases_and_campaign_slot_allocation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
        catalog = catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
        init = (ROOT / 'code/server/sv_init.cpp').read_text()
        commands = (ROOT / 'code/server/sv_ccmds.cpp').read_text()
        defaults = init[init.index('\tconst auto &serverPolicy ='):init.index('\n\tCvar_SetDescription( sv_gametype')]
        # Max-client registration is separate in SV_Init; execute its actual line.
        defaults += '\n' + next(line for line in init.splitlines() if 'sv_maxclients = Cvar_Get' in line)
        source = PRELUDE + catalog + '\n' + function(init, 'static void SV_PrepareRTCWCampaign(')
        source += '\n' + function(init, 'static cvar_t *SV_RegisterPureMode(')
        source += '\nstatic void SV_TestServerDefaults(){\n' + defaults + '\n}\n'
        source += r'''
void SV_SpawnServer(const char*,qboolean){++spawns;SV_PrepareRTCWCampaign();Cvar_Get("sv_maxclients",profile->server.clients,0);}
// Elite Force mode rules have their own fixture; spmap only asks for them.
static int modeClears;void SV_STVEFClearModes(){++modeClears;}
'''
        main = (ROOT / 'code/server/sv_main.cpp').read_text()
        source += function(main, 'qboolean SV_GameTypeIsSinglePlayer( int gameType ) {') + '\n'
        source += function(commands, 'static void SV_Map_f(') + CHECKS
        spawn = function(init, 'void SV_SpawnServer(')
        self.assertLess(spawn.index('SV_PrepareRTCWCampaign();'), spawn.index('SV_ShutdownGameProgs();'))
        with tempfile.TemporaryDirectory(prefix='rtcw-server-policy-') as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
