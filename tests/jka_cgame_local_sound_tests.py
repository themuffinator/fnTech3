"""Academy cgame import 38 plays a local sound through Fn's own path.

The production case is sliced from the cgame host and compiled against a
recording stub of the engine's local-sound entry. Retail jamp.exe 1.01
(0x412934) passes the handle and Academy's own channel number to
S_StartLocalSound; the retail cgamex86.dll (e5d55258...) sends channels 0, 1,
8 and 9 there. Academy numbers its channels differently from the engine from
4 on, so each must arrive as the engine channel of the same meaning.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / 'code/client/client_jka_cgame_host.hpp'


def case_text():
    host = HOST.read_text(encoding='utf-8')
    start = host.index('    case Call::CG_S_STARTLOCALSOUND: {')
    end = host.index('    case Call::CG_S_GETVOICEVOLUME:', start)
    return host[start:end]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/cgame/cg_public.h"
#include "code/client/client_jka_cgame.hpp"
#include "code/client/client_jka_sound.hpp"
#include <cassert>
#include <climits>
#include <cstdint>
#include <initializer_list>
struct Drop {};
void QDECL Com_Error(errorParm_t, const char *, ...) { throw Drop{}; }
static int starts, lastChannel;
static sfxHandle_t lastHandle;
static float lastVolume;
void S_StartLocalSoundVolume(sfxHandle_t sfx, int channel, float volume) {
 ++starts; lastHandle = sfx; lastChannel = channel; lastVolume = volume;
}
static bool dispatch(fntech3::jka::module::Call call, intptr_t *args, intptr_t &result) {
 using namespace fntech3::jka;
 using module::Call;
 result = 0;
 switch (call) {
'''

CHECKS = r'''
 default: assert(false);
 }
 return false;
}
template<class F> static void Refuses(F callback) {
 bool dropped = false; try { callback(); } catch (Drop) { dropped = true; } assert(dropped);
}
int main() {
 using namespace fntech3::jka;
 using module::Call;
 namespace s = fntech3::jka::sound;
 // The ordinal and the frozen numbering it carries.
 static_assert(module::Lookup(COM_GAME_JKA_MP, module::Module::CGAME, module::Direction::IMPORT, 38) ==
  Call::CG_S_STARTLOCALSOUND);
 static_assert(s::ChanAmbient == 7 && s::ChanLocalSound == 8 && s::ChanAnnouncer == 9 &&
  s::ChanVoiceAtten == 4 && s::ChanLessAtten == 10 && s::ChanVoiceGlobal == 12 && s::ChanMusic == 13);
 // The canonical dispatcher would hand Academy's raw number to the engine.
 assert(cgame::SharedImport(Call::CG_S_STARTLOCALSOUND) == -1);

 // Every Academy channel: the engine's channel of the same meaning, or one
 // of its own above the engine's.
 const int expected[14] = {CHAN_AUTO, CHAN_LOCAL, CHAN_WEAPON, CHAN_VOICE, CHAN_ANNOUNCER + 1 + 4,
  CHAN_ITEM, CHAN_BODY, CHAN_ANNOUNCER + 1 + 7, CHAN_LOCAL_SOUND, CHAN_ANNOUNCER,
  CHAN_ANNOUNCER + 1 + 10, CHAN_ANNOUNCER + 1 + 11, CHAN_ANNOUNCER + 1 + 12, CHAN_ANNOUNCER + 1 + 13};
 for (int channel = 0; channel < 14; ++channel) assert(s::EngineChannel(channel) == expected[channel]);
 // No other number takes on an engine channel's rules, and none shares one.
 static int seen[2101];
 for (int channel = -1000; channel <= 1000; ++channel) {
  const int engine = s::EngineChannel(channel);
  const bool shared = channel == 0 || channel == 1 || channel == 2 || channel == 3 || channel == 5 ||
   channel == 6 || channel == 8 || channel == 9;
  assert((engine >= CHAN_AUTO && engine <= CHAN_ANNOUNCER) == shared);
  if (engine >= -1000 && engine <= 1100) { assert(!seen[engine + 1000]); seen[engine + 1000] = 1; }
 }
 for (int channel : {INT_MIN, INT_MIN + 1, INT_MAX - s::OwnChannelBase, INT_MAX - s::OwnChannelBase + 1, INT_MAX}) {
  const int engine = s::EngineChannel(channel);
  assert(engine < CHAN_AUTO || engine > CHAN_ANNOUNCER);
 }
 assert(s::EngineChannel(INT_MAX - s::OwnChannelBase) == INT_MAX && s::EngineChannel(INT_MAX) == INT_MAX);
 // Retail 0x46ae4d: below ambient the listener's own sounds are quieter.
 for (int channel : {INT_MIN, -1, 0, 1, 2, 3, 4, 5, 6}) assert(s::ListenerVolume(channel) == 0.75f);
 for (int channel : {7, 8, 9, 10, 11, 12, 13, 14, INT_MAX}) assert(s::ListenerVolume(channel) == 1.0f);

 // The channels the retail cgame sends: disruptor zoom on 0, 1, chat on 8,
 // announcer on 9. Each starts exactly one Fn local sound, returns zero.
 struct Case { int channel, engine; float volume; };
 for (const Case c : {Case{0, CHAN_AUTO, 0.75f}, Case{1, CHAN_LOCAL, 0.75f},
   Case{8, CHAN_LOCAL_SOUND, 1.0f}, Case{9, CHAN_ANNOUNCER, 1.0f}}) {
  intptr_t args[]{38, 211, c.channel};
  intptr_t result = -1;
  const int before = starts;
  assert(dispatch(Call::CG_S_STARTLOCALSOUND, args, result) && result == 0);
  assert(starts == before + 1 && lastHandle == 211 && lastChannel == c.engine && lastVolume == c.volume);
 }
 // A handle the module never registered still reaches Fn's own bounds check
 // (a warning there; retail's own drop is recorded, not reproduced).
 for (intptr_t handle : {intptr_t(0), intptr_t(-1), intptr_t(INT_MAX)}) {
  intptr_t args[]{38, handle, 8};
  intptr_t result = -1;
  assert(dispatch(Call::CG_S_STARTLOCALSOUND, args, result) && result == 0);
  assert(lastHandle == static_cast<sfxHandle_t>(handle) && lastChannel == CHAN_LOCAL_SOUND);
 }
 // A word wider than the module's int never reaches the engine truncated.
 if (sizeof(intptr_t) > sizeof(int)) {
  const int before = starts;
  for (int index : {1, 2}) for (long long value : {(long long)INT_MAX + 1, (long long)INT_MIN - 1}) {
   intptr_t args[]{38, 211, 8};
   args[index] = static_cast<intptr_t>(value);
   intptr_t result = -1;
   Refuses([&] { dispatch(Call::CG_S_STARTLOCALSOUND, args, result); });
  }
  assert(starts == before);
 }
 return 0;
}
'''


class JKACgameLocalSoundTests(unittest.TestCase):
    def test_case_translates_channel_and_level_into_fn_local_sound(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jka-cgame-local-sound'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary).resolve()
            cpp = folder / 'probe.cpp'
            exe = folder / 'probe.exe'
            cpp.write_text(PRELUDE + case_text() + CHECKS, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Wall', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], cwd=folder, text=True,
                                   capture_output=True, timeout=90)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=folder, text=True, capture_output=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_host_includes_the_channel_table_and_refuses_no_retail_channel(self):
        host = HOST.read_text(encoding='utf-8')
        self.assertIn('#include "client_jka_sound.hpp"', host)
        case = case_text()
        # Through Fn's local-sound entry, never the untranslated canonical one.
        self.assertIn('S_StartLocalSoundVolume(', case)
        self.assertIn('sound::EngineChannel(channel)', case)
        self.assertNotIn('S_StartLocalSound(', case)
        self.assertNotIn('Unsupported', case)


if __name__ == '__main__':
    unittest.main()
