"""Every profile remains available independently of legacy build switches."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
PROFILES = ('quake3', 'quakelive', 'rtcw-sp', 'rtcw-mp', 'wolfet', 'stvef-sp',
            'stvef-mp', 'q3-ihv', 'jka-sp', 'jka-mp', 'jk2-sp', 'jk2-mp',
            'alice', 'fakk2', 'mohaa', 'q3test')
LEGACY = ('RTCW', 'WOLFET', 'STVEF', 'Q3_IHV', 'JKA', 'JK2', 'ALICE',
          'FAKK2', 'MOHAA', 'Q3TEST')

HARNESS = r'''
#include "game_manager.h"
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv) {
    CHECK(argc == 2 && !Com_GameProfile());
    CHECK(Com_GameManagerProfileCount() == 16);
    for (unsigned index = 0; index < Com_GameManagerProfileCount(); ++index) {
        const auto *profile = Com_GameManagerProfileAt(index);
        CHECK(Com_GameManagerCanActivate(profile));
        comGameProfile_t copy = *profile;
        CHECK(!Com_GameManagerCanActivate(&copy));
        comGameMarkerInputs_t automatic{"auto", profile->installMarker, 0, 0};
        CHECK(Com_DetectGameProfileMarkers(&automatic).profile == profile);
        comGameMarkerInputs_t explicitRequest{profile->key, 0, 0, 0};
        CHECK(Com_DetectGameProfileMarkers(&explicitRequest).profile == profile);
        CHECK(Com_GameProfileAllowsProtocol(profile, profile->preferredProtocol));
        CHECK(Com_GameProfileAllowsModuleApi(profile, profile->preferredModuleApi));
    }
    CHECK(!Com_GameManagerCanActivate(nullptr));
    const auto *chosen = Com_GameManagerFindProfile(argv[1]);
    CHECK(chosen && Com_ActivateGameProfile(chosen->id));
    CHECK(Com_GameProfile() == chosen && Com_ActivateGameProfile(chosen->id));
    for (unsigned index = 0; index < Com_GameManagerProfileCount(); ++index) {
        const auto *other = Com_GameManagerProfileAt(index);
        if (other != chosen) CHECK(!Com_ActivateGameProfile(other->id));
    }
    CHECK(Com_GameProfile() == chosen);
}
'''


class GameProfileAvailabilityTests(unittest.TestCase):
    def test_every_profile_detects_and_activates_in_every_build_configuration(self):
        compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp' / 'always-available-games'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='profiles-', dir=scratch) as folder:
            folder = Path(folder)
            source = folder / 'probe.cpp'
            source.write_text(HARNESS, encoding='utf-8')
            exe = folder / 'probe.exe'
            configurations = {
                'default': [],
                'legacy-flags-off': [f'-DFNTECH3_{name}_DEVELOPMENT=0' for name in LEGACY],
                'legacy-flags-on': [f'-DFNTECH3_{name}_DEVELOPMENT=1' for name in LEGACY],
                'dedicated': ['-DDEDICATED'],
                'legacy-diagnostic': ['-DFNTECH3_Q3_ONLY=1'],
            }
            for name, defines in configurations.items():
                with self.subTest(configuration=name):
                    result = subprocess.run([compiler, *compiler_target_args(compiler), *defines,
                                             '-std=c++17', '-I', str(ROOT / 'code/qcommon'),
                                             str(source), str(ROOT / 'code/qcommon/game_profile.cpp'),
                                             '-o', str(exe)], cwd=ROOT, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    for profile in PROFILES:
                        with self.subTest(profile=profile):
                            result = subprocess.run([str(exe), profile], cwd=ROOT,
                                                    capture_output=True, text=True)
                            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
