"""Execute ET's registered input handlers and command builder without device input."""
from pathlib import Path
import unittest

import rtcw_player_interaction_tests as interaction

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
#include "code/compat/wolfet_state_adapter.hpp"
static void Invoke(const std::string &name, const char *key="42", const char *generation="fnql-gen:1") {
 arguments[1]=key; arguments[2]="100"; arguments[3]=generation; commands.at(name)();
}
static usercmd_t Sample() { usercmd_t cmd{}; CL_CmdButtons(&cmd); return cmd; }
int main() {
 run.integer=1;
 // The crash happened when an ordinary key set Q3's 16-bit any-key value.
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_STVEF_MP,COM_GAME_WOLFET}) {
  profile=id; IN_AddCommandBindings();
  for(int capture:{0,KEYCATCH_RETAIL_MOUSEPASS,KEYCATCH_UI,KEYCATCH_CONSOLE,KEYCATCH_CGAME}) {
   catcher=capture; anykeydown=1;
   auto cmd=Sample();
   const int any=(Com_GameIsRTCW()||Com_GameIsSTVEFMP()||Com_GameIsWolfET()) ? 128 : BUTTON_ANY;
   assert(cmd.buttons==((capture&~KEYCATCH_RETAIL_MOUSEPASS)?BUTTON_TALK:any));
   if(Com_GameIsWolfET()) {
    fntech3_wolfet_usercmd_t wire{};
    assert(fntech3::wolfet::state::FromInternal(cmd,wire));
    assert(wire.buttons==cmd.buttons);
   }
  }
  anykeydown=catcher=0;
  IN_ClearCommandInputState(); IN_RemoveCommandBindings();
 }
 profile=COM_GAME_WOLFET; IN_AddCommandBindings();
 struct Action { const char *name; int buttons,wbuttons; };
 for(const auto &a: {Action{"attack",1,0},Action{"useitem",4,0},Action{"salute",8,0},
      Action{"sprint",32,0},Action{"activate",64,0},Action{"attack2",0,1},Action{"zoom",0,2},
      Action{"reload",0,8},Action{"leanleft",0,16},Action{"leanright",0,32},Action{"prone",0,128}}) {
  const std::string down=std::string("+")+a.name,up=std::string("-")+a.name;
  assert(commands.count(down)&&commands.count(up));
  assert(CL_IsEngineStatefulInputCommand(down.c_str()));
  Invoke(down); Invoke(up); // a tap between frames still appears once
  auto cmd=Sample();
  assert(cmd.buttons==a.buttons&&cmd.rtcw.wbuttons==a.wbuttons&&!cmd.rtcw.wolfkick);
  fntech3_wolfet_usercmd_t wire{};
  assert(fntech3::wolfet::state::FromInternal(cmd,wire));
  assert(wire.buttons==a.buttons&&wire.wbuttons==a.wbuttons);
  assert(!Sample().buttons&&!Sample().rtcw.wbuttons);
  Invoke(down); Invoke(up,"42","fnql-gen:0");
  cmd=Sample(); assert(cmd.buttons==a.buttons&&cmd.rtcw.wbuttons==a.wbuttons);
  IN_ClearCommandInputState(); cmd=Sample(); assert(!cmd.buttons&&!cmd.rtcw.wbuttons);
 }
 for(int index=7;index<16;++index) {
  Invoke("+button"+std::to_string(index)); auto cmd=Sample();
  assert(!cmd.buttons); // no unrepresentable Q3 action bits reach the writer
  fntech3_wolfet_usercmd_t wire{}; assert(fntech3::wolfet::state::FromInternal(cmd,wire));
  Invoke("-button"+std::to_string(index));
 }
 // The boundary still rejects malformed commands rather than truncating them.
 usercmd_t invalid{}; invalid.buttons=BUTTON_ANY;
 fntech3_wolfet_usercmd_t wire{}; assert(!fntech3::wolfet::state::FromInternal(invalid,wire));
 IN_ClearCommandInputState(); IN_RemoveCommandBindings(); assert(commands.empty());
}
'''


class WolfETInputTests(unittest.TestCase):
    compile_and_run = interaction.RTCWPlayerInteractionTests.compile_and_run

    def test_profile_buttons_named_actions_taps_and_restart_releases(self):
        source = interaction.input_fixture((ROOT / 'code/client/cl_input.cpp').read_text())
        self.compile_and_run(source + CHECKS)


if __name__ == '__main__':
    unittest.main()
