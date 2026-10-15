"""cl_viewShake: the engine-owned view-shake scale, its factor, the Academy
record it scales, and the profile coverage record beside the ledger."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]
RECORD = ROOT / 'docs/fntech3/2026-10-15-view-shake.md'
COMPANION = RECORD.with_suffix('.json')
PROFILES = {'q3', 'q3-ihv', 'q3test', 'ql', 'rtcw-sp', 'rtcw-mp', 'wolfet', 'stvef-sp', 'stvef-mp',
            'jka-sp', 'jka-mp', 'jk2-sp', 'jk2-mp', 'alice', 'fakk2', 'mohaa'}


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


# Independently authored stand-ins; the factor itself is sliced from cl_main.cpp.
FACTOR = r'''
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <limits>
struct cvar_t{float value;};
static cvar_t *cl_viewShake;
'''


class ViewShakeFactorTests(unittest.TestCase):
    def test_factor_holds_the_cvar_to_zero_through_one(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        body = FACTOR + function(read('code/client/cl_main.cpp'), 'float CL_ViewShakeScale( void )') + r'''
int main(){
 assert(CL_ViewShakeScale()==1.0f); // Before CL_Init registers it: retail's shake.
 cvar_t cvar{1};cl_viewShake=&cvar;
 const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
 struct Case{float value,factor;};
 for(const Case &item:{Case{1,1},Case{0.5f,0.5f},Case{0.25f,0.25f},Case{0,0},Case{-0.0f,0},Case{-3,0},
  Case{4,1},Case{inf,1},Case{-inf,0},Case{nan,1}}){
  cvar.value=item.value;const float factor=CL_ViewShakeScale();
  assert(factor==item.factor&&!std::signbit(factor));
 }
}
'''
        scratch = ROOT / '.tmp/tests/view-shake'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            source = Path(folder) / 'factor.cpp'
            source.write_text(body)
            executable = Path(folder) / ('factor.exe' if os.name == 'nt' else 'factor')
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2', '-Werror',
                str(source), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class ViewShakeSourceTests(unittest.TestCase):
    def test_one_archived_client_cvar_in_every_profile(self):
        main = read('code/client/cl_main.cpp')
        header = read('code/client/client.h')
        self.assertIn('cl_viewShake = Cvar_Get( "cl_viewShake", "1", CVAR_ARCHIVE );', main)
        self.assertIn('Cvar_CheckRange( cl_viewShake, "0", "1", CV_FLOAT );', main)
        self.assertEqual(main.count('Cvar_Get( "cl_viewShake"'), 1)
        self.assertIn('extern\tcvar_t\t*cl_viewShake;', header)
        self.assertIn('float CL_ViewShakeScale( void );', header)
        # Registered beside cl_menuBlur, outside every profile branch.
        registration = main.index('cl_viewShake = Cvar_Get(')
        self.assertLess(main.index('cl_menuBlur = Cvar_Get('), registration)
        self.assertLess(registration, main.index('cl_cinematicAspect = Cvar_Get('))

    def test_academy_scales_the_export_31_record_and_keeps_the_call(self):
        host = read('code/client/client_jka_cgame_effects.hpp')
        shake = function(host, 'static void CL_JKACgameEffectCameraShake(')
        self.assertIn('record.intensity=spawn.shakeIntensity*CL_ViewShakeScale();', shake)
        self.assertEqual(shake.count('CL_ViewShakeScale'), 1)
        # fx_debug 2 prints the intensity as handed over.
        self.assertIn('spawn.shakeIntensity*CL_ViewShakeScale(),spawn.shakeRadius,spawn.life,state.time);', host)
        # No early return: the module receives export 31 at every scale.
        self.assertNotIn('return', shake)
        self.assertEqual(shake.count('VM_Call(module,0,CG_RAVEN_CAMERA_SHAKE);'), 1)
        # The factor is read nowhere else: no other profile's data is scaled.
        users = [path.relative_to(ROOT).as_posix() for path in (ROOT / 'code').rglob('*')
                 if path.suffix in {'.c', '.cpp', '.h', '.hpp'} and 'CL_ViewShakeScale' in path.read_text(
                     encoding='utf-8', errors='replace')]
        self.assertEqual(sorted(users), ['code/client/cl_main.cpp', 'code/client/client.h',
                                         'code/client/client_jka_cgame_effects.hpp'])


class ViewShakeRecordTests(unittest.TestCase):
    def test_companion_covers_every_profile(self):
        data = json.loads(COMPANION.read_text(encoding='utf-8'))
        self.assertEqual(data['cvar']['name'], 'cl_viewShake')
        self.assertEqual(data['cvar']['default'], '1')
        rows = {row['profile']: row for row in data['profiles']}
        self.assertEqual(set(rows), PROFILES)
        for profile, row in rows.items():
            with self.subTest(profile=profile):
                self.assertIn(row['honours'], {'yes', 'no'})
                self.assertTrue(row['why'].strip())
                self.assertIn(row['evidence'], {'observed', 'inferred', 'observed-and-inferred'})
        self.assertEqual({p for p, row in rows.items() if row['honours'] == 'yes'}, {'jka-mp'})

    def test_record_ledger_and_guide_agree(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('2026-10-15-view-shake.json', record)
        self.assertIn('cl_viewShake', read('docs/DISPLAY.md'))
        self.assertIn('fntech3/2026-10-15-view-shake.md', read('docs/DISPLAY.md'))
        self.assertIn('2026-10-15-view-shake.md', read('docs/fntech3/COMPATIBILITY.md'))
        self.assertIn('fntech3/2026-10-15-view-shake.md', read('docs/README.md'))


if __name__ == '__main__':
    unittest.main()
