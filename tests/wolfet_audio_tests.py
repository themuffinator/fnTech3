"""Exercise ET's sound boundary: every value it submits must be carried."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT/'code/client/client_wolfet_audio_compat.hpp'
MAIN = ROOT/'code/client/audio/snd_main.cpp'
LEGACY = ROOT/'code/client/audio/legacy/snd_dma.cpp'
OPENAL = ROOT/'code/client/audio/openal/AudioSystemBackend.inl'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-audio.md'

PROBE = r'''
#include "code/client/audio/shared/RTCWLoops.h"
#include <cassert>

using namespace fntech3::audio::rtcw;

int main() {
 const float origin[3] = {1, 2, 3}, velocity[3] = {0, 0, 0};

 // The other Wolfenstein profile submits no start time and must keep none, so
 // its loops stay on the mixer's own phase.
 LoopFrame frame;
 assert(frame.Append(origin, velocity, 1250, 4, 127) == 0);
 assert(frame[0].startTime == 0);

 // ET's submitted time is carried through untouched.
 assert(frame.Append(origin, velocity, 1250, 4, 127, 1.0f, 987654) == 1);
 assert(frame[1].startTime == 987654);
 assert(frame[1].handle == 4);

 // A refused submission stores nothing at all.
 assert(frame.Append(origin, velocity, -1, 4, 127, 1.0f, 5) < 0);
 assert(frame.Append(origin, velocity, 1250, 4, 0, 1.0f, 5) < 0);
 assert(frame.Size() == 2);

 // The phase helper never runs past the sample and refuses nonsense rates.
 assert(LoopSampleOffset(0, 44100, 1000) == 0);
 assert(LoopSampleOffset(1000000, 44100, 44100) == 0);
 assert(LoopSampleOffset(500000, 44100, 44100) == 22050);
 assert(LoopSampleOffset(-1, 44100, 1000) == 0);
 assert(LoopSampleOffset(1000, 0, 1000) == 0);
 assert(LoopSampleOffset(1000, 44100, 0) == 0);
 for (int i = 0; i < 64; ++i) {
  const int offset = LoopSampleOffset(i * 7919, 22050, 513);
  assert(offset >= 0 && offset < 513);
 }
 return 0;
}
'''

# Every sound ordinal ET declares, on either module boundary.
CGAME_SOUND = [36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
               100, 111, 112]
UI_SOUND = [39, 40, 41, 42, 79, 80]


def _ordinals(module):
    text = (ROOT/'code/compat/wolfet_module_calls.def').read_text(encoding='utf-8')
    return {name: int(number) for group, direction, name, number in re.findall(
        r'FNTECH3_WOLFET_MODULE_CALL\((\w+), (\w+), (\w+), (\d+)\)', text)
        if group == module and direction == 'IMPORT'}


def _function(path, signature):
    lines = path.read_text(encoding='utf-8').split('\n')
    first = next(i for i, line in enumerate(lines) if line.startswith(signature))
    last = next(i for i in range(first + 1, len(lines)) if lines[i] == '}')
    return '\n'.join(lines[first:last + 1])


class WolfETAudioTests(unittest.TestCase):
    def test_loop_start_time_is_carried(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-audio'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'loops.cpp'
            executable = folder/('loops.exe' if os.name == 'nt' else 'loops')
            source.write_text(PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                '-I', str(ROOT), str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True,
                                 text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_every_sound_ordinal_is_handled(self):
        region = _function(AUDIO, 'inline bool CL_WolfETAudioCall')
        handled = set(re.findall(r'case Call::(\w+)', region))
        cgame, ui = _ordinals('CGAME'), _ordinals('UI')
        for ordinal in CGAME_SOUND:
            name = next(n for n, v in cgame.items() if v == ordinal)
            self.assertIn(name, handled, name)
        for ordinal in UI_SOUND:
            name = next(n for n, v in ui.items() if v == ordinal)
            self.assertIn(name, handled, name)

    def test_no_submitted_value_is_dropped(self):
        """ET's volume, start time and stop-sounds flag reach the mixer."""
        region = _function(AUDIO, 'inline bool CL_WolfETAudioCall')
        self.assertIn('S_StartWolfETSound( origin.data(), entity, channel, handle, flags, volume )', region)
        self.assertIn('S_AddWolfETLoopingSound( origin.data(), velocity.data(), range, handle, volume, soundTime )', region)
        self.assertIn('args[3] ? qtrue : qfalse', region)
        # The one-shot volume word moves with the extended form's extra argument.
        self.assertIn('CL_WolfETAudioInteger( extended ? args[6] : args[5] )', region)

    def test_every_entry_refuses_another_profile(self):
        """ET's sound path must be unreachable from any other profile."""
        text = MAIN.read_text(encoding='utf-8')
        for entry in ('S_StartWolfETSound', 'S_StartWolfETLocalSound',
                      'S_AddWolfETLoopingSound', 'S_ClearWolfETLoopingSounds',
                      'S_ClearWolfETSounds', 'S_FadeWolfETAllSounds',
                      'S_FadeWolfETStreamingSound', 'S_StartWolfETBackgroundTrack',
                      'S_StartWolfETEntityStream', 'S_StopWolfETEntityStream',
                      'S_WolfETVoiceAmplitude', 'S_WolfETSoundLength',
                      'S_WolfETCurrentSoundTime'):
            start = text.index(entry + '(')
            window = text[start:start + 700]
            self.assertIn('Com_GameIsWolfET()', window, entry)

    def test_both_backends_implement_the_surface(self):
        legacy = LEGACY.read_text(encoding='utf-8')
        openal = OPENAL.read_text(encoding='utf-8')
        for entry in ('StartWolfETSound', 'StartWolfETLocalSound',
                      'AddWolfETLoopingSound', 'FadeWolfETAllSounds',
                      'WolfETSoundLength', 'WolfETCurrentSoundTime'):
            self.assertIn('si->%s = ' % entry, legacy, entry)
            self.assertIn('si->%s = ' % entry, openal, entry)

    def test_the_two_profiles_keep_separate_master_fades(self):
        """A fade level belongs to the profile that set it."""
        text = MAIN.read_text(encoding='utf-8')
        self.assertIn('static fntech3::audio::rtcw::MasterFade wolfetMasterFade;', text)
        self.assertIn('wolfetMasterFade = {};', text)
        self.assertIn('S_WolfETMasterFade()', text)

    def test_the_record_states_the_coverage(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('125 of the 147', record)
        self.assertIn('110 of the 121', record)


if __name__ == '__main__':
    unittest.main()
