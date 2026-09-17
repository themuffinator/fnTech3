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
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <map>
#include <string>
static const comGameProfile_t *profile;
const comGameProfile_t *Com_GameProfile(){return profile;}
int Com_GameIsRTCWSP(){return profile->id==COM_GAME_RTCW_SP;}
int Com_GameIsRTCWMP(){return profile->id==COM_GAME_RTCW_MP;}
int Com_GameIsQuakeLive(){return profile->id==COM_GAME_QUAKELIVE;}
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
cvar_t *sv_gametype,*sv_maxclients;
static cvar_t running{},dedicated{};
cvar_t *com_sv_running=&running,*com_dedicated=&dedicated;
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
'''

CHECKS = r'''
int main(){
 for(const auto &p:profiles){profile=&p;
  const bool sp=p.id==COM_GAME_RTCW_SP,mp=p.id==COM_GAME_RTCW_MP,ql=p.id==COM_GAME_QUAKELIVE;
  values.clear();auto *pureMode=SV_RegisterPureMode();assert(pureMode->integer==(sp?0:1));
  assert(pureMode->flags&CVAR_SYSTEMINFO);assert(bool(pureMode->flags&CVAR_LATCH)==!(sp||mp));
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
'''
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
