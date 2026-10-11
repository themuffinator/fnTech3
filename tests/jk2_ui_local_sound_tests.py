"""Outcast UI import 32 plays a menu sound through Fn's own interface entry.

The production case and the host's refusal branch are sliced from the
Outcast UI host and compiled against a recording stub of the engine's entry
for a menu's sound: a local sound on its channel, played as interface audio.
Retail jk2mp.exe 1.02a (2f08dcf9...) serves import 32 at 0x40ffb0 by handing
the handle and channel to S_StartLocalSound (0x45d370), as client-game
import 33 (0x401c7f) does, and returns zero. Its S_StartSound stores one
level, 240 (0x45d315), for every one-shot it starts, with no channel or
listener rule, and its channel picker (0x45d090) treats only channel 0
apart. Outcast numbers its channels as Quake III does: its retail client
game (assets0.pk3 vm/cgame.qvm, 82c6543a...) plays sound/player/talk.wav for
chat on 6 and Mon Mothma's announcer lines on 7, and its menus (vm/ui.qvm,
19e372d0...) play `play` on 0 and focus sounds on 6. So the handle and the
channel reach Fn unchanged, at the profile's ordinary level.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / 'code/client/client_jk2_ui.hpp'
SHARED = ROOT / 'code/client/cl_ui.cpp'


def host_text():
    return HOST.read_text(encoding='utf-8')


def call_function():
    host = host_text()
    return host[host.index('static bool CL_JK2UICall('):]


def case_text():
    body = call_function()
    start = body.index('\tcase 32:\n')
    following = [at for at in (body.find('\n\tcase ', start + 1), body.find('\n\tdefault:', start + 1)) if at >= 0]
    return body[start:min(following) + 1]


def default_text():
    # The host's own refusal of every number it does not serve.
    body = call_function()
    start = body.index('\n\tdefault:\n') + 1
    end = body.index('\n\t}\n}\n', start) + 1
    return body[start:end]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/ui/ui_public.h"
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
'''

DISPATCH = r'''
static bool Dispatch(intptr_t *args, intptr_t &operation, intptr_t &result) {
 result = 0;
 switch (args[0]) {
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
 intptr_t args[]{32, handle, channel}, operation = -7, result = -1;
 assert(Dispatch(args, operation, result));
 assert(operation == -7);
 return result;
}
int main() {
#ifdef _MSC_VER
 _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
 // Outcast's numbers are the engine's: its client game plays chat on 6 and
 // its announcer on 7, its menus' focus sounds on 6 and `play` on 0.
 static_assert(CHAN_AUTO == 0 && CHAN_LOCAL == 1 && CHAN_WEAPON == 2 && CHAN_VOICE == 3, "");
 static_assert(CHAN_ITEM == 4 && CHAN_BODY == 5 && CHAN_LOCAL_SOUND == 6 && CHAN_ANNOUNCER == 7, "");
 // The canonical dispatcher never sees import 32: the host's case serves it.
 assert(CL_JK2UISharedCall(32) == -1);

 // Every channel arrives as itself, with the handle, at the full level, and
 // the module gets zero back.
 for (int channel = CHAN_AUTO; channel <= CHAN_ANNOUNCER; ++channel) {
  const int before = starts;
  assert(Play(97, channel) == 0);
  assert(starts == before + 1 && lastHandle == 97 && lastChannel == channel && lastVolume == 1.0f);
 }
 // Numbers no Outcast channel uses are not moved onto another channel's rules.
 for (int channel : {-1, INT_MIN, 8, 13, INT_MAX}) {
  const int before = starts;
  assert(Play(97, channel) == 0);
  assert(starts == before + 1 && lastChannel == channel && lastVolume == 1.0f);
 }
 // The channels the retail UI sends: `play` on 0, both focus paths on 6.
 struct Case { int channel, engine; };
 for (const Case c : {Case{0, CHAN_AUTO}, Case{6, CHAN_LOCAL_SOUND}}) {
  const int before = starts;
  assert(Play(211, c.channel) == 0);
  assert(starts == before + 1 && lastHandle == 211 && lastChannel == c.engine && lastVolume == 1.0f);
 }
 // A handle the module never registered, such as the zero a `play` of a
 // missing file registers, still reaches Fn's own bounds check.
 for (intptr_t handle : {intptr_t(0), intptr_t(-1), intptr_t(INT_MAX)}) {
  assert(Play(handle, 6) == 0);
  assert(lastHandle == static_cast<sfxHandle_t>(handle) && lastChannel == CHAN_LOCAL_SOUND);
 }

 // A word wider than the module's int never reaches the engine truncated.
 if (sizeof(intptr_t) > sizeof(int)) {
  for (int index : {1, 2}) for (long long value : {(long long)INT_MAX + 1, (long long)INT_MIN - 1}) {
   intptr_t args[]{32, 211, 6}, operation = -7, result = -1;
   args[index] = static_cast<intptr_t>(value);
   lastError[0] = 0;
   Refuses([&] { Dispatch(args, operation, result); });
   assert(std::strstr(lastError, "Invalid Jedi Outcast UI local sound handle or channel"));
  }
  std::puts("over-wide words refused");
 }
 // On the x86 target the module's words are the engine's int already.

 // Numbers the host does not serve still meet its refusal, and start nothing.
 for (intptr_t ordinal : {intptr_t(-1), intptr_t(500)}) {
  intptr_t args[]{ordinal, 211, 6}, operation = -7, result = -1;
  lastError[0] = 0;
  Refuses([&] { Dispatch(args, operation, result); });
  assert(std::strstr(lastError, "Unsupported jk2-mp UI import"));
 }
 assert(Play(5, 6) == 0 && lastChannel == CHAN_LOCAL_SOUND);
 std::puts("outcast ui local sound passed");
 return 0;
}
'''


def probe_source():
    shared = function(host_text(), 'static intptr_t CL_JK2UISharedCall(')
    return PRELUDE + shared + '\n' + DISPATCH + case_text() + default_text() + CHECKS


def build_and_run(test, compiler, target, folder, name, env=None):
    cpp = folder / f'{name}.cpp'
    exe = folder / f'{name}.exe'
    cpp.write_text(probe_source(), encoding='utf-8')
    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Wall', '-I', str(ROOT),
                            str(cpp), '-o', str(exe)], cwd=folder, text=True,
                           capture_output=True, timeout=120, env=env)
    test.assertEqual(build.returncode, 0, build.stdout + build.stderr)
    run = subprocess.run([str(exe)], cwd=folder, text=True, capture_output=True, timeout=20)
    test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
    test.assertIn('outcast ui local sound passed', run.stdout)
    return run.stdout


class JK2UILocalSoundTests(unittest.TestCase):
    def test_case_plays_the_modules_channel_at_the_profiles_level(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jk2-ui-local-sound'
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

    def test_host_serves_the_import_through_the_interface_entry(self):
        case = case_text()
        # Fn's entry for a menu's sound, with Outcast's channel and the full
        # level: no channel table, no listener level, no raw local sound.
        self.assertIn('S_StartInterfaceSound( static_cast<sfxHandle_t>( args[1] ), static_cast<int>( args[2] ), 1.0f );',
                      case)
        self.assertIn('args[1] < INT_MIN || args[1] > INT_MAX || args[2] < INT_MIN || args[2] > INT_MAX', case)
        for absent in ('S_StartLocalSound(', 'S_StartLocalSoundVolume(', 'EngineChannel', 'ListenerVolume'):
            self.assertNotIn(absent, case)
        self.assertNotIn('case 32:', function(host_text(), 'static intptr_t CL_JK2UISharedCall('))
        # The shared UI dispatcher enters the Outcast host only under its profile.
        shared = SHARED.read_text(encoding='utf-8')
        calls = [match.start() for match in re.finditer(r'\bCL_JK2UICall\(', shared)]
        self.assertEqual(len(calls), 1, 'one entry into the Outcast UI host')
        guard = shared.rfind('if ( Com_GameIsJK2MP() ) {', 0, calls[0])
        self.assertGreaterEqual(guard, 0)
        self.assertNotIn('\n\t}', shared[guard:calls[0]], 'the entry sits inside the profile guard')


if __name__ == '__main__':
    unittest.main()
