"""Malformed Ritual light records and entity keys stay out of model lighting.

These are engine input-validation policies, exercised through the production
FAKK 42/12 reader with strict and fast floating-point optimization. Valid
retail-map arithmetic is covered by ritual_model_lights_tests.py.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from ritual_model_lights_tests import MAP_BUILDER, ROOT


RECORD_CHECKS = MAP_BUILDER + r'''
int main( int argc, char **argv ) {
    assert( argc == 2 );
    const unsigned bad = (unsigned)strtoul( argv[1], nullptr, 16 );
    for ( int version : { 42, 12 } ) {
        const Light light = { { 120, 0, 32 }, { 1, 0.5f, 0.25f }, 30,
            0, 0, 0, { 0, 0, -1 }, 1, 100 };
        for ( int field : { 0, 4, 8, 12, 16, 20, 24, 40, 44, 48, 52, 56 } ) {
            if ( version == 12 && field == 56 ) continue;
            auto file = Map( version, { light }, { 0, -1 }, { 0 }, "{}", true );
            const int lightOffset = RitualModelLight_ReadInt( file.data() + 12 + 8 * 17 );
            memcpy( file.data() + lightOffset + field, &bad, sizeof( bad ) );
            ritualModelLights_t *map = Parse( file );
            assert( map );
            const ritualModelLight_t *parsed = RITUAL_ML_AT( map, ritualModelLight_t, ofsLights );
            if ( parsed->intensity != 0.0f ) {
                fprintf( stderr, "nonfinite light active: version %i field %i bits %x\n", version, field, bad );
                return 1;
            }
            free( map );
        }
    }
    puts( "ok" );
}
'''

KEY_CHECKS = MAP_BUILDER + r'''
int main() {
    for ( int version : { 42, 12 } ) {
        const Light light = { { 120, 0, 32 }, { 1, 0.5f, 0.25f }, 30,
            0, 0, 0, { 0, 0, -1 }, 1, 100 };
        /* A malformed component preserves the last valid component, while
         * the other components and later valid keys still take effect. */
        const char keys[] =
            "{ ambientlight \"1 2 3\" suncolor \"4 5 6\" sundirection \"-90 0 0\" }"
            "{ ambientlight \"nan 7 inf\" suncolor \"-inf nan 8\" sundirection \"nan inf -inf\" }";
        ritualModelLights_t *map = Parse( Map( version, { light }, { 0, -1 }, { 0 }, keys, true ) );
        assert( map );
        if ( map->ambient[0] != 1 || map->ambient[1] != 7 || map->ambient[2] != 3
            || map->sunColor[0] != 4 || map->sunColor[1] != 5 || map->sunColor[2] != 8
            || std::fabs( map->sunDirection[0] ) > 1e-6f || std::fabs( map->sunDirection[1] ) > 1e-6f
            || std::fabs( map->sunDirection[2] - 1 ) > 1e-6f ) {
            fputs( "malformed worldspawn key entered model light\n", stderr );
            return 1;
        }
        free( map );
        /* Overflowing numeric text receives the same treatment. Partial
         * valid lists and repeated keys retain the original parser policy. */
        map = Parse( Map( version, { light }, { 0, -1 }, { 0 },
            "{ ambientlight \"2 3 4\" ambientlight \"1e999 9\" suncolor \"-5 0.25 20\" }", true ) );
        assert( map );
        if ( map->ambient[0] != 2 || map->ambient[1] != 9 || map->ambient[2] != 4
            || map->sunColor[0] != -5 || map->sunColor[1] != 0.25f || map->sunColor[2] != 20 ) return 1;
        free( map );
    }
    puts( "ok" );
}
'''

BOUND_CHECKS = MAP_BUILDER + r'''
int main( int argc, char **argv ) {
    assert( argc == 3 );
    const unsigned bits = (unsigned)strtoul( argv[1], nullptr, 16 );
    const int expected = atoi( argv[2] );
    float value;
    memcpy( &value, &bits, sizeof( value ) );
    if ( RitualModelLight_FiniteBits( bits ) != expected || RitualModelLight_Finite( &value ) != expected ) {
        fprintf( stderr, "finite bound changed: bits %x expected %i\n", bits, expected );
        return 1;
    }
    puts( "ok" );
}
'''


class RitualModelLightValidationTests(unittest.TestCase):
    def run_host(self, source, args=()):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        temp_root = ROOT / '.tmp' / 'ritual-model-light-validation-tests'
        temp_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='model-light-validation-', dir=temp_root) as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            for math_mode in ('-fno-fast-math', '-ffast-math'):
                with self.subTest(math_mode=math_mode):
                    built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2',
                                            math_mode, '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    for argv in args or ((),):
                        run = subprocess.run([str(exe), *argv], cwd=ROOT, capture_output=True, text=True, timeout=120)
                        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                        self.assertEqual(run.stdout.strip(), 'ok')

    def test_nonfinite_compiled_lights_are_inactive_in_both_profiles(self):
        self.run_host(RECORD_CHECKS, [(bits,) for bits in
                                     ('7fc01234', 'ffc01234', '7f800001', 'ff800001', '7f800000', 'ff800000')])

    def test_nonfinite_worldspawn_components_preserve_the_last_valid_value(self):
        self.run_host(KEY_CHECKS)

    def test_existing_finite_bounds_and_signed_zero_are_preserved(self):
        accepted = ('00000000', '80000000', '3f800000', 'bf800000', '7f7fc99e', 'ff7fc99e')
        rejected = ('7f7fc99f', 'ff7fc99f', '7fc01234', 'ffc01234', '7f800000', 'ff800000')
        self.run_host(BOUND_CHECKS, [(bits, '1') for bits in accepted] + [(bits, '0') for bits in rejected])


if __name__ == '__main__':
    unittest.main()
