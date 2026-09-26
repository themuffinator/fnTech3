"""Run the shipping random-wave evaluation against retail SP/MP observations."""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


class STVEFWaveTests(unittest.TestCase):
    def test_noise_table_matches_both_retail_initializers_only_for_elite_force(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        fixture = json.loads((ROOT / 'tests/fixtures/stvef_wave_retail.json').read_text())
        expected = ',\n'.join('{%uu,%uu}' % tuple(row) for row in fixture['noiseTable'])
        initialize = function((ROOT / 'code/renderercommon/tr_noise.c').read_text(), 'void R_NoiseInit(')
        source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define NOISE_SIZE 256
#define NOISE_MASK 255
#define RAND_MAX 32767
enum { COM_GAME_STVEF_SP=5, COM_GAME_STVEF_MP=6 };
static float s_noise_table[NOISE_SIZE];
static int s_noise_perm[NOISE_SIZE];
static struct { int gameId; } ri;
static unsigned calls;
/* An independent legacy random stream with exact endpoint expectations. */
static int rand(void) { const int values[]={0,32767,32767,0}; return values[calls++&3]; }
INITIALIZE
static const uint32_t expected[256][2] = { EXPECTED };
int main(void) {
    unsigned profile,repeat,i;
    for(profile=0;profile<7;++profile) for(repeat=0;repeat<2;++repeat) {
        ri.gameId=(int)profile; calls=0;
        memset(s_noise_table,0xa5,sizeof(s_noise_table));
        memset(s_noise_perm,0xa5,sizeof(s_noise_perm));
        R_NoiseInit();
        if(profile==COM_GAME_STVEF_SP || profile==COM_GAME_STVEF_MP) {
            assert(calls==0); /* Do not reseed or consume the process PRNG. */
            for(i=0;i<256;++i) {
                uint32_t bits;memcpy(&bits,s_noise_table+i,4);
                if(bits!=expected[i][0] || (uint32_t)s_noise_perm[i]!=expected[i][1]) {
                    fprintf(stderr,"profile %u entry %u: %08x/%d != %08x/%u\n",
                        profile,i,bits,s_noise_perm[i],expected[i][0],expected[i][1]);
                    return 1;
                }
            }
        } else {
            assert(calls==512);
            for(i=0;i<256;++i) {
                assert(s_noise_table[i]==((i&1)?1.0f:-1.0f));
                assert(s_noise_perm[i]==((i&1)?0:255));
            }
        }
    }
}
'''.replace('INITIALIZE', initialize).replace('EXPECTED', expected)
        with tempfile.TemporaryDirectory(prefix='stvef-noise-', dir=ROOT / '.tmp') as folder:
            folder = Path(folder); cpp = folder / 'noise.c'; cpp.write_text(source)
            binary = folder / ('noise.exe' if os.name == 'nt' else 'noise')
            target = compiler_target_args(compiler)
            sanitizer = [] if os.name == 'nt' else ['-fsanitize=undefined', '-fno-sanitize-recover=all']
            built = subprocess.run([compiler, *target, *sanitizer, '-std=c99', '-O1', str(cpp),
                '-o', str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_random_wave_uses_render_time_and_keeps_large_clock_precision(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        fixture = json.loads((ROOT / 'tests/fixtures/stvef_wave_retail.json').read_text())
        cases = fixture['cases']
        native_cases = ',\n'.join('{%d, %.9gf, %.9gf, %.9gf}' %
            (row['time'], row['phase'], row['threshold'], row['value']) for row in cases)
        # C floating constants require a decimal point when no exponent is used.
        native_cases = re.sub(r'(?<![\w.])(-?\d+)f\b', r'\1.0f', native_cases)
        noise = function((ROOT / 'code/renderercommon/tr_noise.c').read_text(), 'float R_NoiseRandom(')
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(
                    prefix='stvef-wave-', dir=ROOT / '.tmp') as folder:
                folder = Path(folder)
                production = (ROOT / f'code/{renderer}/tr_shade_calc.c').read_text()
                macro = next(line for line in production.splitlines() if line.startswith('#define\tWAVEVALUE'))
                source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define NOISE_MASK 255
#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK 1023
enum { GF_SIN=1, GF_NOISE=6, GF_RANDOM=7, COM_GAME_STVEF_SP=5, COM_GAME_STVEF_MP=6 };
typedef struct { int func; float base, amplitude, phase, frequency; } waveForm_t;
static float s_noise_table[256], ordinaryTable[1024];
static int s_noise_perm[256], wallClock, wallClockReads;
static int Milliseconds(void) { ++wallClockReads; return wallClock; }
static struct { int gameId; int (*Milliseconds)(void); } ri;
static struct { struct { int time; double floatTime; } refdef; } backEnd;
static struct { double shaderTime; } tess;
static float *TableForFunc(int func) { assert(func==GF_SIN); return ordinaryTable; }
static float R_NoiseGet4f(float x,float y,float z,double t) { (void)x;(void)y;(void)z;(void)t; assert(0); return 0; }
/* Alice's entity-supplied wave parameters (R_AliceWave) leave every other profile's wave as it is. */
static const waveForm_t *R_AliceWave(const waveForm_t *wf, waveForm_t *resolved) { (void)resolved; return wf; }
MACRO
NOISE
EVALUATE
static const struct { int time; float phase, threshold, expected; } cases[] = { CASES };
int main(void) {
    const int wallTimes[] = {7, 100001, 3000000};
    unsigned i, j; int profile;
    ri.Milliseconds=Milliseconds;
    for(i=0;i<256;++i) { s_noise_table[i]=(float)((int)(i%17)-8)/8; s_noise_perm[i]=(i*73+19)&255; }
    for(i=0;i<1024;++i) ordinaryTable[i]=.25f;
    for(profile=COM_GAME_STVEF_SP;profile<=COM_GAME_STVEF_MP;++profile) {
        ri.gameId=profile;
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
            waveForm_t wave={GF_RANDOM,.125f,.625f,cases[i].phase,cases[i].threshold};
            backEnd.refdef.time=cases[i].time;
            backEnd.refdef.floatTime=(double)cases[i].time/1000;
            for(j=0;j<sizeof(wallTimes)/sizeof(wallTimes[0]);++j) {
                float actual;
                wallClock=wallTimes[j]; actual=EvalWaveForm(&wave);
                if(actual!=cases[i].expected) {
                    fprintf(stderr,"case %u time %d phase %.9g threshold %.9g: %.9g != %.9g\n",
                        i,cases[i].time,wave.phase,wave.frequency,actual,cases[i].expected);
                    return 1;
                }
            }
        }
    }
    assert(wallClockReads==0);
    /* Existing table waves still use the material's time, not the random clock. */
    {
        waveForm_t wave={GF_SIN,.125f,.5f,.25f,1};
        tess.shaderTime=10;
        assert(EvalWaveForm(&wave)==.25f);
    }
    printf("%u native vectors, both profiles, three independent wall clocks\n", (unsigned)(sizeof(cases)/sizeof(cases[0])));
}
'''.replace('MACRO', macro).replace('NOISE\n', noise + '\n').replace(
                    'EVALUATE', function(production, 'static float EvalWaveForm(')).replace('CASES', native_cases)
                cpp = folder / 'wave.c'; cpp.write_text(source)
                binary = folder / ('wave.exe' if os.name == 'nt' else 'wave')
                target = compiler_target_args(compiler)
                sanitizer = [] if os.name == 'nt' else ['-fsanitize=undefined,float-cast-overflow', '-fno-sanitize-recover=all']
                built = subprocess.run([compiler, *target, *sanitizer, '-std=c99', '-O1', str(cpp),
                    '-o', str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True,
                    timeout=30, **options)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
