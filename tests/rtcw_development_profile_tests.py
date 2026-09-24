"""The explicit RTCW developer build keeps release gates and profile immutability."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = r'''
#include "code/qcommon/game_manager.h"
#include <cassert>
#include <cstring>
#include <initializer_list>
static comGameDetectionResult_t Detect(const char *name, unsigned markers) {
    comGameMarkerInputs_t input{name, markers, 0, 0}; return Com_DetectGameProfileMarkers(&input);
}
int main(int argc, char **argv) {
    assert(argc == 2 && !Com_GameProfile());
#if defined(FNTECH3_RTCW_DEVELOPMENT) && !defined(FNTECH3_Q3_ONLY)
    const bool allowed = true;
#else
    const bool allowed = false;
#endif
    for (auto id : {COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        const auto *profile = Com_GameProfileById(id);
        assert(profile && profile->runtimeBlocker && profile->titleId == COM_GAME_TITLE_RTCW);
        assert((Com_GameManagerCanActivate(profile) != 0) == allowed);
        assert(!profile->usesSteam && profile->preferredProtocol == (id == COM_GAME_RTCW_SP ? 49 : 60));
        comGameProfile_t forged = *profile; forged.runtimeBlocker = nullptr;
        assert(!Com_GameManagerCanActivate(&forged));
        for (const char *name : {profile->key, "auto"}) {
            const auto result = Detect(name, id == COM_GAME_RTCW_SP ? COM_GAME_MARKER_RTCW_SP : COM_GAME_MARKER_RTCW_MP);
            if (allowed) assert(result.status == COM_GAME_DETECTION_OK && result.profile == profile);
            else {
#if defined(FNTECH3_Q3_ONLY)
                assert(result.status == COM_GAME_DETECTION_UNSUPPORTED_BUILD);
#else
                assert(result.status == COM_GAME_DETECTION_INCOMPLETE_ENGINE);
#endif
                assert(!result.profile);
            }
        }
    }
    assert(Detect("rtcw",0).status == COM_GAME_DETECTION_MODE_REQUIRED);
    assert(Detect("auto", COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_RTCW_MP).status == COM_GAME_DETECTION_MODE_REQUIRED);
    assert(Detect("auto", COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_QUAKE3).status == COM_GAME_DETECTION_AMBIGUOUS);
    assert(Com_GameManagerCanActivate(Com_GameProfileById(COM_GAME_QUAKE3)));
    const auto *chosen = Com_GameManagerFindProfile(argv[1]); assert(chosen);
    const auto other = chosen->id == COM_GAME_RTCW_SP ? COM_GAME_RTCW_MP : COM_GAME_RTCW_SP;
    assert((Com_ActivateGameProfile(chosen->id) != 0) == allowed);
    if (allowed) {
        assert(Com_GameProfile() == chosen && Com_GameIsRTCW());
        assert(Com_ActivateGameProfile(chosen->id));
        assert(!Com_ActivateGameProfile(other) && !Com_ActivateGameProfile(COM_GAME_QUAKE3));
        assert(Com_GameProfile() == chosen);
    } else {
        assert(!Com_GameProfile() && !Com_GameIsRTCW());
        assert(Com_ActivateGameProfile(COM_GAME_QUAKE3));
        assert(!Com_ActivateGameProfile(chosen->id));
    }
}
'''


class RTCWDevelopmentProfileTests(unittest.TestCase):
    def test_explicit_build_gate_preserves_detection_and_immutable_modes(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        for macros in ([], ['-DFNTECH3_RTCW_DEVELOPMENT=1'], ['-DFNTECH3_RTCW_DEVELOPMENT=1', '-DFNTECH3_Q3_ONLY=1']):
            with self.subTest(macros=macros), tempfile.TemporaryDirectory(prefix='rtcw-development-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp); source = folder / 'probe.cpp'
                executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(FIXTURE, encoding='utf-8')
                build = subprocess.run([compiler, *target, *macros, '-std=c++17', '-I', str(ROOT), str(source),
                    str(ROOT / 'code/qcommon/game_profile.cpp'), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=90)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                for mode in ('rtcw-sp', 'rtcw-mp'):
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(executable), mode], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
