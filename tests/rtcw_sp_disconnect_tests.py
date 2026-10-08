"""Run the client's disconnect command against stubs: RTCW SP clears both
loading states first, as WolfSP.exe's CL_Disconnect_f does (0x40b790), and
no other profile touches them."""
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
#include <map>
#include <string>
#include <vector>
enum { CA_DISCONNECTED = 1, CA_ACTIVE = 8, CA_CINEMATIC = 9 };
enum { ERR_DISCONNECT = 3 };
enum { UI_SET_ACTIVE_MENU = 7, UIMENU_NONE = 0 };
struct cvar_t { int integer; };
static struct { int state; } cls;
static cvar_t *com_sv_running, *com_dedicated;
static void *uivm;
static bool rtcwSP;
static std::map<std::string, std::string> values;
static std::vector<std::string> calls;
static int Com_GameIsRTCWSP() { return rtcwSP; }
static void SCR_StopCinematic() { calls.push_back("cinematic"); }
static void Cvar_Set(const char *name, const char *value) { values[name] = value; calls.push_back(name); }
static void SV_Shutdown(const char *) { calls.push_back("shutdown"); cls.state = CA_DISCONNECTED; }
static int VM_Call(void *, int, int, int) { return 0; }
static void CL_RestoreMainMenuAfterDisconnect() { calls.push_back("menu"); }
static void Com_Error(int, const char *) { calls.push_back("error"); throw 1; }
'''

CHECKS = r'''
int main() {
 cvar_t running{1}, dedicated{0};
 for (bool sp : {false, true}) for (int state : {CA_DISCONNECTED, CA_CINEMATIC, CA_ACTIVE}) for (int local : {0, 1}) {
  rtcwSP = sp; cls.state = state; running.integer = local;
  com_sv_running = &running; com_dedicated = &dedicated;
  values = {{"savegame_loading", "2"}, {"g_reloading", "4"}}; calls.clear();
  try { CL_Disconnect_f(); } catch (int) {}
  assert(values["savegame_loading"] == (sp ? "0" : "2"));
  assert(values["g_reloading"] == (sp ? "0" : "4"));
  // Retail clears them right after the cinematic stops, before any
  // shutdown or error, so every route out of the level sees them clear.
  assert(calls[0] == "cinematic");
  if (sp) assert(calls[1] == "savegame_loading" && calls[2] == "g_reloading");
  const bool leaving = state == CA_ACTIVE;
  assert(leaving == (calls.back() == (local ? "menu" : "error")));
 }
}
'''


class RTCWSPDisconnectTests(unittest.TestCase):
    def test_disconnect_clears_loading_states_in_rtcw_sp_only(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        source = PRELUDE + function(main, 'void CL_Disconnect_f( void ) {') + CHECKS
        with tempfile.TemporaryDirectory(prefix='rtcw-sp-disconnect-') as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                                   capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
