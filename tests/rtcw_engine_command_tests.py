"""Run engine command handlers against frozen RTCW facts, without device input."""
from pathlib import Path
import importlib.util
import json
import unittest

from mesh_tag_tests import function
import rtcw_player_interaction_tests as interaction

ROOT = Path(__file__).resolve().parents[1]

INPUT_CHECKS = r'''
static void Invoke(const std::string &name,const char *key="42",const char *tag="fnql-gen:1") {
 arguments[1]=key; arguments[2]="100"; arguments[3]=tag; commands.at(name)();
}
static usercmd_t Sample() { usercmd_t cmd{}; CL_CmdButtons(&cmd); return cmd; }
struct Action { const char *name; int button,weapon,kick; comGameId_t only=COM_GAME_AUTO; };
static const Action actions[]={
 {"activate",64,0,0}, {"useitem",4,0,0}, {"salute",8,0,0}, {"sprint",32,0,0},
 {"attack2",0,1,0}, {"zoom",0,2,0}, {"quickgren",0,4,0}, {"reload",0,8,0},
 {"leanleft",0,16,0}, {"leanright",0,32,0},
 {"wbutton6",0,64,0,COM_GAME_RTCW_SP}, {"dropweapon",0,64,0,COM_GAME_RTCW_MP},
 {"wbutton7",0,128,0}, {"kick",0,0,1}
};
static void Check(const Action &a,bool pressed) {
 auto cmd=Sample();
 assert(cmd.buttons==(pressed?a.button:0));
 assert(cmd.rtcw.wbuttons==(pressed?a.weapon:0));
 assert(cmd.rtcw.wolfkick==(pressed?a.kick:0));
 if(Com_GameIsRTCWSP()) {
  fntech3_rtcw_sp_usercmd_t out{}; assert(fntech3::rtcw::state::FromInternal(cmd,out));
  assert(out.common.buttons==cmd.buttons && out.common.wbuttons==cmd.rtcw.wbuttons && out.common.wolfkick==cmd.rtcw.wolfkick);
 } else if(Com_GameIsRTCWMP()) {
  fntech3_rtcw_mp_usercmd_t out{}; assert(fntech3::rtcw::state::FromInternal(cmd,out));
  assert(out.common.buttons==cmd.buttons && out.common.wbuttons==cmd.rtcw.wbuttons && out.common.wolfkick==cmd.rtcw.wolfkick);
 }
}
int main() {
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  profile=id; IN_AddCommandBindings();
  for(const char *moduleCommand:{"+scores","+inventory","weapalt","weaponbank","savegame"}) {
   assert(!commands.count(moduleCommand));
   assert(!CL_IsEngineStatefulInputCommand(moduleCommand));
  }
  for(const auto &a:actions) {
   const bool present=Com_GameIsRTCW() && (a.only==COM_GAME_AUTO || a.only==id);
   const std::string down=std::string("+")+a.name, up=std::string("-")+a.name;
   assert(commands.count(down)==present && commands.count(up)==present);
   assert(bool(CL_IsEngineStatefulInputCommand(down.c_str()))==present);
   assert(!CL_IsEngineStatefulInputCommand((down+"_invalid").c_str()));
   if(!present) continue;
   IN_ClearCommandInputState();
   Invoke(down); Invoke(up); Check(a,true); Check(a,false); // sub-frame tap exactly once
   Invoke(down); Check(a,true); Check(a,true); // held across commands
   Invoke(up,"42","fnql-gen:0"); Check(a,true); // stale release cannot cancel current owner
   Invoke(up); Check(a,false);
   Invoke(down,"42","fnql-gen:0"); Check(a,false);
   Invoke(down,"42","fnql-gen:bad"); Check(a,false);
   Invoke(down,"9999"); Check(a,false);
   Invoke(down); Invoke(down,"43"); Check(a,true);
   IN_RemoveCommandInputSource(42); Check(a,true);
   IN_RemoveCommandInputSource(43); Check(a,false);
   Invoke(down); Check(a,true); IN_ClearCommandInputState(); Check(a,false);
   Invoke(down,"",""); Check(a,true); Invoke(up,"",""); Check(a,false);
  }
  if(Com_GameIsRTCW()) {
   for(const auto &a:actions) if(a.only==COM_GAME_AUTO || a.only==id) Invoke(std::string("+")+a.name);
   auto cmd=Sample(); assert(cmd.buttons==108 && cmd.rtcw.wbuttons==255 && cmd.rtcw.wolfkick==1);
   IN_ClearCommandInputState();
   // Existing raw-button aliases share ownership with their named commands.
   Invoke("+sprint"); Invoke("+button5","43"); Invoke("-sprint"); assert(Sample().buttons==32);
   Invoke("-button5","43"); assert(!Sample().buttons);
  } else {
   Invoke("+button8"); auto cmd=Sample(); assert(cmd.buttons==256 && !cmd.rtcw.wbuttons && !cmd.rtcw.wolfkick);
  }
  IN_ClearCommandInputState(); IN_RemoveCommandBindings(); assert(commands.empty());
 }
}
'''


def client_fixture(source):
    start = source.index('static void CL_RTCWNotebook_f(')
    end = source.index('void CL_RTCWLimboChat(', start)
    return r'''
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <map>
#include <string>
using xcommand_t=void(*)();
enum { CA_ACTIVE=8, CVAR_ARCHIVE=1, UI_SET_ACTIVE_MENU=7, UIMENU_NOTEBOOK=9, UIMENU_HELP=11 };
static const bool qtrue=true,qfalse=false;
static comGameId_t profile;
int Com_GameIsRTCW() { return profile==COM_GAME_RTCW_SP || profile==COM_GAME_RTCW_MP; }
int Com_GameIsRTCWSP() { return profile==COM_GAME_RTCW_SP; }
static struct { int state; bool uiStarted; } cls;
static struct { bool demoplaying; } clc;
static struct { bool rtcwLimboMode; } cl;
static void *uivm;
static int menu,refreshes,reloads,shutdowns,inits,argc=1;
static bool lowMemory;
static std::string queued;
static std::map<std::string,std::string> cvars;
static std::map<std::string,xcommand_t> commands;
static struct { int integer=1; } quality;
static auto Cvar_Get(const char *,const char *,int) { return &quality; }
static void Cvar_Set(const char *name,const char *value) { cvars[name]=value; }
static bool Sys_LowPhysicalMemory() { return lowMemory; }
static int Cmd_Argc() { return argc; }
static void Cbuf_AddText(const char *s) { queued+=s; }
static void VM_Call(void *vm,int nargs,int call,int value) { assert(vm && nargs==1 && call==UI_SET_ACTIVE_MENU); menu=value; }
static void SCR_UpdateScreen() { ++refreshes; }
static void CL_RTCWReloadTranslations() { ++reloads; }
static void CL_ShutdownUI() { ++shutdowns; }
static void CL_InitUI() { ++inits; }
static void Cmd_AddCommand(const char *s,xcommand_t f) { assert(commands.emplace(s,f).second); }
static void Cmd_RemoveCommand(const char *s) { assert(commands.erase(s)==1); }
''' + source[start:end] + r'''
int main() {
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  profile=id; CL_AddRTCWCommands();
  if(!Com_GameIsRTCW()) { assert(commands.empty()); CL_RemoveRTCWCommands(); continue; }
  const bool sp=Com_GameIsRTCWSP();
  assert(commands.count("notebook")==sp && commands.count("cld")==sp);
  assert(commands.count("help")==!sp && commands.count("ui_restart")==!sp && commands.count("LoadTranslations")==!sp);
  commands.at("startLimboMode")(); assert(cl.rtcwLimboMode);
  commands.at("stopLimboMode")(); assert(!cl.rtcwLimboMode);
  refreshes=0; commands.at("updatescreen")(); assert(refreshes==1);
  auto open=commands.at(sp?"notebook":"help");
  for(int state:{0,int(CA_ACTIVE)}) for(bool demo:{false,true}) for(bool vm:{false,true}) {
   menu=0; cvars["cg_youGotMail"]="1"; cls.state=state; clc.demoplaying=demo; uivm=vm?&menu:nullptr;
   open(); const bool active=state==CA_ACTIVE && !demo && vm;
   assert(menu==(active?(sp?UIMENU_NOTEBOOK:UIMENU_HELP):0));
   assert(cvars["cg_youGotMail"]==(active&&sp?"0":"1"));
  }
  if(!sp) {
   shutdowns=inits=0; cls.uiStarted=false; commands.at("ui_restart")(); assert(!shutdowns && !inits);
   cls.uiStarted=true; commands.at("ui_restart")(); assert(shutdowns==1 && inits==1);
   reloads=0; commands.at("LoadTranslations")(); assert(reloads==1);
  }
  for(int high:{0,1}) for(int count:{1,2}) for(bool low:{false,true}) {
   quality.integer=high; argc=count; lowMemory=low; queued.clear(); cvars.clear();
   commands.at("setRecommended")();
   assert(queued==std::string(high?"exec highVidhighCPU.cfg\n":"exec lowVidhighCPU.cfg\n")+(count>1?"vid_restart\n":""));
   assert(cvars.at("ui_glCustom")=="999"); assert(cvars.count("s_khz")==low);
   assert(cvars.count("r_lowMemTextureSize")==bool(low&&!high));
  }
  CL_RemoveRTCWCommands(); assert(commands.empty());
 }
}
'''


def chat_fixture(keys, console):
    return r'''
#include "code/qcommon/game_profile.h"
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
enum { MAX_STRING_CHARS=1024, K_ESCAPE=27, K_ENTER=13, K_KP_ENTER=169,
 KEYCATCH_MESSAGE=4, CA_ACTIVE=8, CG_CHAT_UP=13, CG_CHAT_DOWN=12,
 CG_GET_CHAT_FIELD_Y=14, CG_GET_CHAT_FIELD_PIXEL_WIDTH=15, CG_GET_CHAT_FIELD_WIDTH_IN_CHARS=16, SCREEN_WIDTH=640 };
using qboolean=bool;
static constexpr bool qfalse=false,qtrue=true;
static comGameId_t profile;
static comGameProfile_t selected;
const comGameProfile_t *Com_GameProfile() { selected.id=profile; return &selected; }
int Com_GameIsRTCW() { return profile==COM_GAME_RTCW_SP || profile==COM_GAME_RTCW_MP; }
int Com_GameIsQuakeLive() { return profile==COM_GAME_QUAKELIVE; }
int Com_GameIsWolfET() { return profile==COM_GAME_WOLFET; }
static struct { int state; } cls;
static struct { bool rtcwLimboMode; } cl;
static struct { bool dllExports=true; } vm, *cgvm=&vm;
static struct Field { char buffer[128]; int widthInChars; } chatField;
static bool chat_team,chat_buddy;
static int chat_playerNum=-1,catcher,notifications;
static std::string sent;
static int Key_GetCatcher() { return catcher; }
static void Key_SetCatcher(int x) { catcher=x; }
static void Field_Clear(Field *f) { f->buffer[0]=0; }
static void Field_KeyDownEvent(Field *,int) {}
static int VM_Call(void *,int,int call) {
 assert(Com_GameIsQuakeLive());
 if(call==CG_GET_CHAT_FIELD_Y) return 410;
 if(call==CG_GET_CHAT_FIELD_PIXEL_WIDTH) return 400;
 if(call==CG_GET_CHAT_FIELD_WIDTH_IN_CHARS) return 45;
 ++notifications; return 0;
}
static void CL_AddReliableCommand(const char *s,bool) { sent=s; }
static void Com_sprintf(char *out,int len,const char *fmt,...) { va_list ap; va_start(ap,fmt); vsnprintf(out,len,fmt,ap); va_end(ap); }
''' + function(keys, 'static void Message_Key(') + ''.join(
        console[console.index(start):console.index('\n};', console.index(start)) + 3] + '\n'
        for start in ('enum class ChatMode {', 'struct NotifyContract {')) + ''.join(function(console, name) for name in (
        'static comGameId_t Con_ProfileId(', 'static NotifyContract Con_NotifyContract(',
        'static void Con_ToggleMessageCatcher(', 'static int Con_GetChatFieldY(',
        'static int Con_GetChatFieldPixelWidth(', 'static int Con_GetChatFieldWidthInChars(',
        'static int Con_ChatFieldWidth(', 'static void Con_ResetChatField(',
        'static void Con_MessageMode_f(', 'static void Con_MessageMode2_f(')) + r'''
int main() {
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  profile=id;
  assert(Con_GetChatFieldY()==(Com_GameIsQuakeLive()?410:413));
  assert(Con_GetChatFieldPixelWidth()==(Com_GameIsQuakeLive()?400:640));
  // Quake Live's cgame sizes its field; Quake III and RTCW open 30 and 25.
  catcher=0; Con_MessageMode_f(); assert(catcher==KEYCATCH_MESSAGE && !chat_team && chat_playerNum==-1);
  assert(chatField.widthInChars==(Com_GameIsQuakeLive()?45:30));
  catcher=0; Con_MessageMode2_f(); assert(catcher==KEYCATCH_MESSAGE && chat_team);
  assert(chatField.widthInChars==(Com_GameIsQuakeLive()?40:25));
  for(bool limbo:{false,true}) for(bool team:{false,true}) for(int player:{-1,2}) {
   cl.rtcwLimboMode=limbo; chat_team=team; chat_playerNum=player;
   cls.state=CA_ACTIVE; strcpy(chatField.buffer,"hello"); sent.clear(); catcher=0; notifications=0;
   Con_ToggleMessageCatcher(); assert(catcher==KEYCATCH_MESSAGE);
   Message_Key(K_ENTER); assert(!catcher && !chatField.buffer[0]);
   const char *command=player>=0?"tell 2":team?"say_team":Com_GameIsRTCW()&&limbo?"say_limbo":"say";
   assert(sent==std::string(command)+" \"hello\"\n");
   assert(notifications==(Com_GameIsQuakeLive()?2:0));
  }
  sent.clear(); strcpy(chatField.buffer,"cancel"); Message_Key(K_ESCAPE); assert(sent.empty());
  cls.state=0; strcpy(chatField.buffer,"offline"); Message_Key(K_ENTER); assert(sent.empty());
 }
}
'''


def sound_fixture(source):
    return r'''
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
static comGameId_t profile;
int Com_GameIsRTCWSP() { return profile==COM_GAME_RTCW_SP; }
int Com_GameIsRTCW() { return Com_GameIsRTCWSP() || profile==COM_GAME_RTCW_MP; }
static const char *args[4]; static int argc,calls,queueType;
static std::string intro,loop;
static int Cmd_Argc() { return argc; }
static const char *Cmd_Argv(int i) { return i<argc?args[i]:""; }
static void Com_Printf(const char *,...) {}
static void Capture(const char *a,const char *b) { ++calls; intro=a; loop=b?b:"<null>"; }
static struct { void (*StartBackgroundTrack)(const char *,const char *)=Capture; } si;
static void S_StartRTCWBackgroundTrack(const char *a,const char *b,int type) {
 assert(Com_GameIsRTCWSP()); queueType=type; Capture(a,b);
}
''' + function(source, 'extern "C" void S_StartBackgroundTrack(') + function(source, 'static bool S_RTCWStreamInteger(') + function(source, 'static void S_Music_f(') + function(source, 'static void S_QueueMusic_f(') + r'''
int main() {
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  profile=id; args[0]="music"; args[1]="intro"; args[2]="loop";
  for(argc=1;argc<=4;++argc) {
   calls=0; queueType=99; S_Music_f(); assert(calls==(argc==2||argc==3));
   if(calls) { assert(intro=="intro"); assert(loop==(argc==3?"loop":Com_GameIsRTCW()?"intro":"<null>")); }
   if(calls&&Com_GameIsRTCWSP()) assert(queueType==0);
  }
 }
 profile=COM_GAME_RTCW_SP; args[1]="next";
 argc=2; calls=0; S_QueueMusic_f(); assert(calls==1 && queueType==-2 && intro=="next" && loop=="next");
 argc=3;
 for(const char *type:{"-1","-2","0","123","no","99999999999999999999999"}) {
  args[2]=type; calls=0; S_QueueMusic_f();
  const bool valid=std::string(type)!="no" && strlen(type)<10;
  assert(calls==valid); if(valid) assert(queueType==(std::string(type)=="-1"?-1:-2));
 }
 argc=2; args[1]=""; calls=0; S_QueueMusic_f(); assert(!calls);
}
'''


class RTCWEngineCommandTests(unittest.TestCase):
    compile_and_run = interaction.RTCWPlayerInteractionTests.compile_and_run

    def test_reviewed_engine_command_inventory(self):
        spec = importlib.util.spec_from_file_location('rtcw_command_audit', ROOT / 'scripts/audit_rtcw_engine_commands.py')
        audit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(audit)
        inventory = json.loads((ROOT / 'docs/fntech3/rtcw-engine-commands.json').read_text())
        self.assertEqual(len(inventory['commands']), 183)
        for mode, count in (('rtcw-sp', 171), ('rtcw-mp', 176)):
            self.assertEqual(sum(mode in row['original'] for row in inventory['commands']), count)
        self.assertEqual(audit.validate(inventory, audit.registrations(ROOT / 'code', current=True)), [])

    def test_action_commands_preserve_profile_abi_and_input_lifecycle(self):
        self.compile_and_run(interaction.input_fixture((ROOT / 'code/client/cl_input.cpp').read_text()) + INPUT_CHECKS)

    def test_client_registration_menus_restarts_translation_and_recommendations(self):
        self.compile_and_run(client_fixture((ROOT / 'code/client/cl_main.cpp').read_text()))

    def test_chat_routing_and_ql_only_notifications(self):
        self.compile_and_run(chat_fixture((ROOT / 'code/client/cl_keys.cpp').read_text(),
                                          (ROOT / 'code/client/cl_console.cpp').read_text()))

    def test_music_dispatch_and_queue_argument_bounds(self):
        self.compile_and_run(sound_fixture((ROOT / 'code/client/audio/snd_main.cpp').read_text()))


if __name__ == '__main__':
    unittest.main()
