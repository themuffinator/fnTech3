"""Execute RTCW activation commands and pickup userinfo without device input."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def input_fixture(client):
    source = r'''
#include "code/client/input_compat.hpp"
#include "code/compat/rtcw_state_adapter.hpp"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <map>
#include <string>
using xcommand_t = void (*)();
enum { CL_INPUT_COMMAND_STALE=-2, CL_INPUT_COMMAND_LEGACY=-1, MAX_KEYS=512 };
static comGameId_t profile;
int Com_GameIsRTCW() { return profile==COM_GAME_RTCW_SP || profile==COM_GAME_RTCW_MP; }
int Com_GameIsRTCWSP() { return profile==COM_GAME_RTCW_SP; }
int Com_GameIsRTCWMP() { return profile==COM_GAME_RTCW_MP; }
static int catcher, anykeydown, com_frameTime=100;
static unsigned frame_msec=16;
static int Key_GetCatcher() { return catcher; }
static unsigned Key_GetBindingGeneration(int) { return 1; }
static const char *arguments[4]={"", "", "", ""};
static const char *Cmd_Argv(int i) { return arguments[i]; }
void QDECL Com_Printf(const char *, ...) {}
static std::map<std::string,xcommand_t> commands;
static void Cmd_AddCommand(const char *name,xcommand_t fn) { assert(commands.emplace(name,fn).second); }
static void Cmd_RemoveCommand(const char *name) { assert(commands.erase(name)==1); }
static cvar_t run{}, freelook{};
static cvar_t *cl_run=&run, *cl_freelook=&freelook;
static void IN_CenterView() {}
static void IN_MLookDown() {}
static void IN_MLookUp() {}
signed char ClampCharMove(int value) { return value < -127 ? -127 : value > 127 ? 127 : value; }
'''
    source += re.search(r'constexpr std::size_t kInputButtonCount[^;]+;', client)[0] + '\n'
    source += re.search(r'struct InputCommandBinding\s*\{.*?\n};', client, re.S)[0] + '\n'
    start = client.index('struct kbutton_t {')
    source += client[start:client.index('static cvar_t *cl_nodelta;', start)]
    for signature in ('int CL_ValidateInputCommandSource(', 'static std::array<kbutton_t *, 13> IN_CommandButtons(',
                      'static void IN_ClearCommandInputState(', 'static void IN_RemoveCommandInputSource(',
                      'static void IN_KeyDown( kbutton_t *b ) {', 'static void IN_KeyUp( kbutton_t *b ) {',
                      'static float CL_KeyState(', 'static int CL_SafeMoveValue('):
        source += function(client, signature) + '\n'
    start = client.index('static void IN_UpDown()')
    source += client[start:client.index('//==========================================================================', start)]
    source += function(client, 'static void CL_CmdButtons(') + '\n'
    source += function(client, 'static void CL_KeyMove(') + '\n'
    return source


INPUT_CHECKS = r'''
static void Invoke(const char *command, const char *key="42", const char *tag="fnql-gen:1") {
 arguments[1]=key; arguments[2]="100"; arguments[3]=tag; commands.at(command)();
}
static usercmd_t Sample() { usercmd_t cmd{}; CL_CmdButtons(&cmd); return cmd; }
int main() {
 run.integer=freelook.integer=1;
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  profile=id; bool rtcw=Com_GameIsRTCW(); IN_ClearCommandInputState(); IN_AddCommandBindings();
  assert(commands.count("+activate")==rtcw && commands.count("-activate")==rtcw);
  assert(bool(CL_IsEngineStatefulInputCommand("+activate"))==rtcw);
  assert(!CL_IsEngineStatefulInputCommand("+activate_more"));
  assert(CL_IsEngineStatefulInputCommand("+attack"));
  anykeydown=1; catcher=0; assert(Sample().buttons==(rtcw?128:BUTTON_ANY));
  catcher=2; assert(Sample().buttons==BUTTON_TALK);
  catcher=0; anykeydown=0;
  if(rtcw) {
   // A sub-frame tap survives once, held keys persist, and either alias releases it.
   Invoke("+activate"); Invoke("-activate"); auto cmd=Sample(); assert(cmd.buttons==64);
   assert(!Sample().buttons);
   Invoke("+activate"); Invoke("+activate"); Invoke("+button6","43");
   Invoke("-activate"); assert(Sample().buttons==64);
   Invoke("-button6","43"); assert(!Sample().buttons);
   Invoke("+activate","42","fnql-gen:0"); assert(!Sample().buttons);
   Invoke("+activate"); Invoke("-activate","42","fnql-gen:0"); assert(Sample().buttons==64);
   IN_RemoveCommandInputSource(42); assert(!Sample().buttons);
   Invoke("+activate"); IN_ClearCommandInputState(); assert(!Sample().buttons);
   Invoke("+activate","",""); assert(Sample().buttons==64); Invoke("-activate","","");
   assert(!Sample().buttons);
   // Both module ABIs receive the activation bit (and the existing wire codec owns it).
   cmd.buttons=64|128;
   fntech3_rtcw_sp_usercmd_t sp{}; fntech3_rtcw_mp_usercmd_t mp{};
   assert(fntech3::rtcw::state::FromInternal(cmd,sp) && sp.common.buttons==192);
   assert(fntech3::rtcw::state::FromInternal(cmd,mp) && mp.common.buttons==192);
  }
  for(int direction:{-1,0,1}) {
   IN_ClearCommandInputState(); in_moveleft.active=direction<0; in_moveright.active=direction>0;
   in_forward.active=true; usercmd_t cmd{}; cmd.buttons=64; CL_KeyMove(&cmd);
   assert(cmd.forwardmove==127 && cmd.rightmove==(rtcw?0:direction*127));
   assert(cmd.rtcw.wbuttons==(rtcw?(direction<0?16:direction>0?32:0):0));
  }
  IN_ClearCommandInputState(); IN_RemoveCommandBindings(); assert(commands.empty());
 }
}
'''


def userinfo_fixture(client):
    start = client.index('Cvar_Get ("password",')
    registration = client[start:client.index('// FNQL_Steam_Init()', start)]
    return r'''
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <map>
#include <string>
enum { CVAR_ARCHIVE=1,CVAR_USERINFO=2,CVAR_NORESTART=1024 };
struct Value { std::string text; int flags; };
static std::map<std::string,Value> values;
static comGameId_t profile;
int Com_GameIsRTCW() { return profile==COM_GAME_RTCW_SP || profile==COM_GAME_RTCW_MP; }
int Com_GameIsRTCWSP() { return profile==COM_GAME_RTCW_SP; }
static void Cvar_Get(const char *name,const char *value,int flags) {
 auto entry=values.emplace(name,Value{value,flags}); entry.first->second.flags|=flags;
}
static std::string Userinfo(const char *key) {
 auto it=values.find(key); return it!=values.end() && (it->second.flags&CVAR_USERINFO)?it->second.text:"";
}
static void Register() {
''' + registration + r'''
}
int main() {
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  profile=id; bool rtcw=Com_GameIsRTCW();
  for(int existing:{-1,0,1}) {
   values.clear();
   if(existing>=0) Cvar_Get("cg_autoactivate",existing?"1":"0",CVAR_ARCHIVE);
   Register();
   assert(Userinfo("cg_autoactivate")== (rtcw?(existing==0?"0":"1"):""));
   assert(Userinfo("cg_emptyswitch")== (id==COM_GAME_RTCW_SP?"0":""));
   // Retail cgame registration must not remove the engine's userinfo flag.
   Cvar_Get("cg_autoactivate","1",CVAR_ARCHIVE);
   if(rtcw) {
    assert(values.at("cg_autoactivate").flags==(CVAR_ARCHIVE|CVAR_USERINFO));
    values.at("cg_autoactivate").text="0"; assert(Userinfo("cg_autoactivate")=="0");
    values.at("cg_autoactivate").text="1"; assert(Userinfo("cg_autoactivate")=="1");
   }
  }
 }
}
'''


class RTCWPlayerInteractionTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-interaction-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-fno-operator-names', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_activation_lifecycle_profile_buttons_and_lean(self):
        self.compile_and_run(input_fixture((ROOT / 'code/client/cl_input.cpp').read_text()) + INPUT_CHECKS)

    def test_pickup_policy_reaches_server_and_preserves_user_preference(self):
        self.compile_and_run(userinfo_fixture((ROOT / 'code/client/cl_main.cpp').read_text()))


if __name__ == '__main__':
    unittest.main()
