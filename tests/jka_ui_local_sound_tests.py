"""Academy UI import 33 plays a menu sound through Fn's own local-sound path.

The production case and the host's refusal branch are sliced from the UI host
and compiled against a recording stub of the engine's entry for a menu's
sound: a local sound on its channel, played as interface audio.
Retail jamp.exe 1.01 (0x424603) hands the handle and Academy's own channel
number to S_StartLocalSound exactly as client-game import 38 (0x412934) does;
the retail uix86.dll (1179892f...) sends channel 0 from its `play` script
command, 1 from its move previews and 8 from its focus sounds. Academy
numbers its channels differently from the engine from 4 on, so each must
arrive as the engine channel of the same meaning, at retail's level.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / 'code/client/client_jka_ui_host.hpp'
SHARED = ROOT / 'code/client/cl_ui.cpp'


def host_text():
    return HOST.read_text(encoding='utf-8')


def case_text():
    host = host_text()
    start = host.index('\tcase Call::UI_S_STARTLOCALSOUND: {')
    following = [at for at in (host.find('\n\tcase ', start + 1), host.find('\n\tdefault:', start + 1)) if at >= 0]
    return host[start:min(following) + 1]


def default_text():
    # The host's own refusal of every number it does not serve.
    host = host_text()
    function = host.index('static bool CL_JKAUICall(')
    start = host.index('\n\tdefault:\n', function) + 1
    end = host.index('\n\t}\n}\n', start) + 1
    return host[start:end]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/client/client_jka_ui.hpp"
#include "code/client/client_jka_sound.hpp"
#include <array>
#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
struct Drop {};
static char lastError[512];
void QDECL Com_Error(errorParm_t, const char *format, ...) {
 va_list list; va_start(list, format); std::vsnprintf(lastError, sizeof(lastError), format, list); va_end(list);
 throw Drop{};
}
static int starts, lastChannel;
static sfxHandle_t lastHandle;
static float lastVolume;
void S_StartInterfaceSound(sfxHandle_t sfx, int channel, float volume) {
 ++starts; lastHandle = sfx; lastChannel = channel; lastVolume = volume;
}
// The production host resolves numbers with Academy multiplayer's table, and
// cl_ui.cpp enters it only under that profile; resolving with the selected
// profile shows what any other profile's number 33 would meet there.
static comGameId_t profile = COM_GAME_JKA_MP;
static bool Dispatch(intptr_t *args, intptr_t &operation, intptr_t &result,
 std::array<intptr_t, 6> &browserArguments, intptr_t *&sharedArguments) {
 using namespace fntech3::jka;
 using module::Call;
 const auto call = module::Lookup(profile, module::Module::UI, module::Direction::IMPORT, args[0]);
 result = 0;
 switch (call) {
'''

CHECKS = r'''
 }
}
template<class F> static void Refuses(F callback) {
 const int before = starts;
 bool dropped = false; try { callback(); } catch (Drop) { dropped = true; }
 assert(dropped && starts == before);
}
static intptr_t Play(intptr_t handle, intptr_t channel) {
 intptr_t args[]{33, handle, channel}, operation = -7, result = -1;
 std::array<intptr_t, 6> browser{};
 intptr_t *shared = args;
 assert(Dispatch(args, operation, result, browser, shared));
 assert(operation == -7 && shared == args);
 return result;
}
int main() {
#ifdef _MSC_VER
 _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
 using namespace fntech3::jka;
 using module::Call;
 namespace s = fntech3::jka::sound;
 // The ordinal, and the canonical dispatcher that would pass Academy's raw
 // channel number through.
 static_assert(module::Lookup(COM_GAME_JKA_MP, module::Module::UI, module::Direction::IMPORT, 33) ==
  Call::UI_S_STARTLOCALSOUND);
 static_assert(module::Ordinal(COM_GAME_JKA_MP, Call::UI_S_STARTLOCALSOUND) == 33);
 assert(ui::SharedImport(Call::UI_S_STARTLOCALSOUND) == -1 && ui::SharedImport(33) == -1);

 // Every Academy channel arrives as the engine channel of the same meaning,
 // or as one of its own above the engine's, at retail's level: 0.75 below
 // the ambient channel 7, full from it on.
 const int expected[14] = {CHAN_AUTO, CHAN_LOCAL, CHAN_WEAPON, CHAN_VOICE, CHAN_ANNOUNCER + 1 + 4,
  CHAN_ITEM, CHAN_BODY, CHAN_ANNOUNCER + 1 + 7, CHAN_LOCAL_SOUND, CHAN_ANNOUNCER,
  CHAN_ANNOUNCER + 1 + 10, CHAN_ANNOUNCER + 1 + 11, CHAN_ANNOUNCER + 1 + 12, CHAN_ANNOUNCER + 1 + 13};
 for (int channel = 0; channel < 14; ++channel) {
  const int before = starts;
  assert(Play(97, channel) == 0);
  assert(starts == before + 1 && lastHandle == 97 && lastChannel == expected[channel]);
  assert(lastChannel == s::EngineChannel(channel));
  assert(lastVolume == (channel < s::ChanAmbient ? 0.75f : 1.0f));
 }
 // Both levels around the ambient channel 7.
 assert(Play(97, 6) == 0 && lastVolume == 0.75f && lastChannel == CHAN_BODY);
 assert(Play(97, 7) == 0 && lastVolume == 1.0f && lastChannel == CHAN_ANNOUNCER + 1 + 7);
 // Numbers no Academy channel uses take on no engine channel's rules.
 struct Level { int channel; float volume; };
 for (const Level l : {Level{-1, 0.75f}, Level{INT_MIN, 0.75f}, Level{14, 1.0f}, Level{INT_MAX, 1.0f}}) {
  assert(Play(97, l.channel) == 0);
  assert(lastVolume == l.volume && lastChannel == s::EngineChannel(l.channel));
  assert(lastChannel < CHAN_AUTO || lastChannel > CHAN_ANNOUNCER);
 }

 // The channels the retail UI sends: its `play` script command on 0, the
 // move previews on 1, focus sounds on 8.
 struct Case { int channel, engine; float volume; };
 for (const Case c : {Case{0, CHAN_AUTO, 0.75f}, Case{1, CHAN_LOCAL, 0.75f},
   Case{8, CHAN_LOCAL_SOUND, 1.0f}}) {
  const int before = starts;
  assert(Play(211, c.channel) == 0);
  assert(starts == before + 1 && lastHandle == 211 && lastChannel == c.engine && lastVolume == c.volume);
 }
 // A handle the module never registered, such as the zero a `play` of a
 // missing file registers, still reaches Fn's own bounds check.
 for (intptr_t handle : {intptr_t(0), intptr_t(-1), intptr_t(INT_MAX)}) {
  assert(Play(handle, 8) == 0);
  assert(lastHandle == static_cast<sfxHandle_t>(handle) && lastChannel == CHAN_LOCAL_SOUND);
 }

 // A word wider than the module's int never reaches the engine truncated.
 if (sizeof(intptr_t) > sizeof(int)) {
  for (int index : {1, 2}) for (long long value : {(long long)INT_MAX + 1, (long long)INT_MIN - 1}) {
   intptr_t args[]{33, 211, 8}, operation = -7, result = -1;
   args[index] = static_cast<intptr_t>(value);
   std::array<intptr_t, 6> browser{};
   intptr_t *shared = args;
   Refuses([&] { Dispatch(args, operation, result, browser, shared); });
   assert(std::strstr(lastError, "local sound handle or channel"));
  }
  std::puts("over-wide words refused");
 }
 // On the x86 target the module's words are the engine's int already.

 // No other profile resolves Academy's number 33: the host's refusal branch
 // takes it, and no sound starts.
 for (int id = COM_GAME_QUAKE3; id < COM_GAME_COUNT; ++id) {
  if (id == COM_GAME_JKA_MP) continue;
  profile = static_cast<comGameId_t>(id);
  assert(module::Lookup(profile, module::Module::UI, module::Direction::IMPORT, 33) == Call::Invalid);
  assert(module::Ordinal(profile, Call::UI_S_STARTLOCALSOUND) == -1);
  intptr_t args[]{33, 211, 8}, operation = -7, result = -1;
  std::array<intptr_t, 6> browser{};
  intptr_t *shared = args;
  lastError[0] = 0;
  Refuses([&] { Dispatch(args, operation, result, browser, shared); });
  assert(std::strstr(lastError, "(33) is not served yet"));
 }
 profile = COM_GAME_JKA_MP;
 assert(Play(5, 8) == 0 && lastChannel == CHAN_LOCAL_SOUND);
 std::puts("academy ui local sound passed");
 return 0;
}
'''


def build_and_run(test, compiler, target, folder, name, env=None):
    cpp = folder / f'{name}.cpp'
    exe = folder / f'{name}.exe'
    cpp.write_text(PRELUDE + case_text() + default_text() + CHECKS, encoding='utf-8')
    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Wall', '-I', str(ROOT),
                            str(cpp), '-o', str(exe)], cwd=folder, text=True,
                           capture_output=True, timeout=120, env=env)
    test.assertEqual(build.returncode, 0, build.stdout + build.stderr)
    run = subprocess.run([str(exe)], cwd=folder, text=True, capture_output=True, timeout=20)
    test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
    test.assertIn('academy ui local sound passed', run.stdout)
    return run.stdout


class JKAUILocalSoundTests(unittest.TestCase):
    def test_case_translates_channel_and_level_into_fn_local_sound(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jka-ui-local-sound'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary).resolve()
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            output = build_and_run(self, compiler, target, folder, 'probe')
            wide = 'over-wide words refused' in output
            # The shared client is x86, where no word is wider than int. A
            # 64-bit build of the same case runs the refusal of wider words.
            if not wide and os.name == 'nt' and 'clang' in Path(compiler).name.lower():
                # An x86 developer shell's library paths are x86 only; Clang
                # finds the x64 libraries of the same installation itself.
                env = {key: value for key, value in os.environ.items() if key.upper() not in ('LIB', 'LIBPATH')}
                with self.subTest(target='x86_64'):
                    output = build_and_run(self, compiler, ['--target=x86_64-pc-windows-msvc', '-DFNTECH3_Q3_ONLY'],
                                           folder, 'probe64', env)
                    self.assertIn('over-wide words refused', output)

    def test_host_serves_the_import_through_the_channel_table(self):
        host = host_text()
        self.assertIn('#include "client_jka_sound.hpp"', host)
        case = case_text()
        # Through Fn's entry for a menu's sound, which plays it as interface
        # audio whatever its channel, never the untranslated canonical one.
        self.assertIn('S_StartInterfaceSound(', case)
        self.assertIn('sound::EngineChannel( channel )', case)
        self.assertIn('sound::ListenerVolume( channel )', case)
        self.assertNotIn('S_StartLocalSound(', case)
        self.assertNotIn('S_StartLocalSoundVolume(', case)
        self.assertNotIn('not served yet', case)
        # The host only ever resolves Academy multiplayer's numbers, and the
        # shared UI dispatcher enters it only under that profile.
        function = host[host.index('static bool CL_JKAUICall('):]
        self.assertRegex(function[:400], r'module::Lookup\( COM_GAME_JKA_MP, module::Module::UI,')
        shared = SHARED.read_text(encoding='utf-8')
        calls = [match.start() for match in re.finditer(r'\bCL_JKAUICall\(', shared)]
        self.assertEqual(len(calls), 1, 'one entry into the Academy UI host')
        guard = shared.rfind('if ( Com_GameIsJKAMP() ) {', 0, calls[0])
        self.assertGreaterEqual(guard, 0)
        self.assertNotIn('\n\t}', shared[guard:calls[0]], 'the entry sits inside the profile guard')


if __name__ == '__main__':
    unittest.main()
