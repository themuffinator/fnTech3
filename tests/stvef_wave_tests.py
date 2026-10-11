"""Run Raven random/noise waves against retail Elite Force and Outcast facts."""
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
    def test_outcast_noise_stretch_uses_float_view_clock_in_all_backends(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        noise_source = (ROOT / 'code/renderercommon/tr_noise.c').read_text()
        noise = function(noise_source, 'static float GetNoiseValue(')
        noise += function(noise_source, 'float R_NoiseGet4f(').replace(
            'float R_NoiseGet4f(', 'static float ProductionNoise(')
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            production = (ROOT / f'code/{renderer}/tr_shade_calc.c').read_text()
            macro = next(line for line in production.splitlines() if line.startswith('#define\tWAVEVALUE'))
            source = r'''
#include <assert.h>
#include <stdint.h>
#include <math.h>
#define NOISE_MASK 255
#define VAL(a) s_noise_perm[(a)&NOISE_MASK]
#define INDEX(x,y,z,t) VAL((x)+VAL((y)+VAL((z)+VAL(t))))
#define LERP(a,b,f) ((a)+(f)*((b)-(a)))
#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK 1023
enum { GF_SIN=1, GF_NOISE=6, GF_RANDOM=7, COM_GAME_STVEF_SP=5, COM_GAME_STVEF_MP=6, COM_GAME_JK2_SP=7, COM_GAME_JK2_MP=8 };
typedef struct { int func; float base, amplitude, phase, frequency; } waveForm_t;
typedef struct { float matrix[2][2], translate[2]; } texModInfo_t;
static struct { int gameId; } ri;
static struct { struct { int time; double floatTime; } refdef; } backEnd;
static struct { double shaderTime; int numVertexes; } tess;
static float s_noise_table[256], ordinaryTable[1024];
static int s_noise_perm[256], noiseCalls, tableCalls;
static double coordinate;
static float *TableForFunc(int func) { (void)func; ++tableCalls; return ordinaryTable; }
static float R_NoiseRandom(int clock) { (void)clock; assert(0); return 0; }
static const waveForm_t *R_AliceWave(const waveForm_t *wf,waveForm_t *resolved) { (void)resolved; return wf; }
PRODUCTION_NOISE
static float R_NoiseGet4f(float x,float y,float z,double t) {
    ++noiseCalls; coordinate=t; return ProductionNoise(x,y,z,t);
}
WAVE_MACRO
EVALUATE
TRANSFORM
STRETCH
static const struct { double clock; float phase,frequency; double coordinate; } cases[] = {
    {0.25,0.125f,2.0f,0.75},
    {10000.123456789,0.1f,179.0f,1790039.875},
    {16777.216999,0.0001f,179.0f,3003121.75},
    {1.0,-2.25f,2.0f,-2.5},
    {1.0,0.0f,179.0f,179.0}
};
int main(void) {
    unsigned i,j; int profile;
    for(i=0;i<256;++i) { s_noise_perm[i]=(int)i; s_noise_table[i]=(float)i/256.0f; }
    for(i=0;i<1024;++i) ordinaryTable[i]=0.25f;
    for(profile=0;profile<16;++profile) {
        ri.gameId=profile;
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
            waveForm_t wave={GF_NOISE,1.0f,0.1f,cases[i].phase,cases[i].frequency};
            backEnd.refdef.floatTime=cases[i].clock;
            backEnd.refdef.time=7654321; // Not the noise-wave clock.
            for(j=0;j<3;++j) {
                float actual;
                tess.shaderTime=(double)j*9876.125;noiseCalls=tableCalls=0;
                actual=EvalWaveForm(&wave);
                if(profile==COM_GAME_JK2_SP || profile==COM_GAME_JK2_MP) {
                    const double integral=floor(cases[i].coordinate);
                    const float blend=(float)(cases[i].coordinate-integral);
                    const float first=(float)(((int)integral)&255)/256.0f;
                    const float last=(float)(((int)integral+1)&255)/256.0f;
                    const float expected=1.0f+0.1f*(first+blend*(last-first));
                    float src[]={0,0,1,1},dst[4];
                    assert(noiseCalls==1&&!tableCalls&&coordinate==cases[i].coordinate);
                    assert(fabsf(actual-expected)<0.000001f);
                    tess.numVertexes=2;RB_CalcStretchTexCoords(&wave,src,dst);
                    assert(noiseCalls==2&&!tableCalls);
                    assert(fabsf(dst[0]-(0.5f-0.5f/expected))<0.000001f);
                    assert(fabsf(dst[1]-dst[0])<0.000001f);
                    assert(fabsf(dst[2]-(0.5f+0.5f/expected))<0.000001f);
                    assert(fabsf(dst[3]-dst[2])<0.000001f);
                } else if(profile==COM_GAME_STVEF_SP || profile==COM_GAME_STVEF_MP) {
                    assert(noiseCalls==1&&!tableCalls);
                    /* Compare stored doubles, not an extended x87 temporary. */
                    volatile double expectedCoordinate=(cases[i].clock+(double)cases[i].phase)*(double)cases[i].frequency;
                    assert(coordinate==expectedCoordinate);
                } else {
                    assert(!noiseCalls&&tableCalls==1); // Keep existing refusal/table path.
                }
            }
            wave.func=GF_SIN;noiseCalls=tableCalls=0;
            assert(fabsf(EvalWaveForm(&wave)-1.025f)<0.000001f&&!noiseCalls&&tableCalls==1);
        }
    }
    return 0;
}
'''.replace('PRODUCTION_NOISE', noise).replace('WAVE_MACRO', macro).replace(
                'EVALUATE', function(production, 'static float EvalWaveForm(')).replace(
                'TRANSFORM', function(production, 'void RB_CalcTransformTexCoords(')).replace(
                'STRETCH', function(production, 'void RB_CalcStretchTexCoords('))
            for fast in (False, True):
                with self.subTest(renderer=renderer, fast_math=fast), tempfile.TemporaryDirectory(
                        prefix='jk2-noise-', dir=ROOT / '.tmp') as folder:
                    folder = Path(folder); source_file = folder / 'noise.c'; source_file.write_text(source)
                    binary = folder / ('noise.exe' if os.name == 'nt' else 'noise')
                    target = compiler_target_args(compiler)
                    flags = ['-O2', '-ffast-math'] if fast else ['-O0']
                    libraries = [] if os.name == 'nt' else ['-lm']
                    built = subprocess.run([compiler, *target, '-std=c99', *flags, str(source_file),
                                            '-o', str(binary), *libraries], cwd=ROOT,
                                           capture_output=True, text=True, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_noise_table_matches_retail_elite_force_and_outcast_only(self):
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
enum { COM_GAME_STVEF_SP=5, COM_GAME_STVEF_MP=6, COM_GAME_JK2_SP=7, COM_GAME_JK2_MP=8 };
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
    for(profile=0;profile<10;++profile) for(repeat=0;repeat<2;++repeat) {
        ri.gameId=(int)profile; calls=0;
        memset(s_noise_table,0xa5,sizeof(s_noise_table));
        memset(s_noise_perm,0xa5,sizeof(s_noise_perm));
        R_NoiseInit();
        if(profile==COM_GAME_STVEF_SP || profile==COM_GAME_STVEF_MP || profile==COM_GAME_JK2_SP || profile==COM_GAME_JK2_MP) {
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
enum { GF_SIN=1, GF_NOISE=6, GF_RANDOM=7, COM_GAME_STVEF_SP=5, COM_GAME_STVEF_MP=6, COM_GAME_JK2_SP=7, COM_GAME_JK2_MP=8 };
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
    for(profile=COM_GAME_STVEF_SP;profile<=COM_GAME_JK2_MP;++profile) {
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
    printf("%u native vectors, four profiles, three independent wall clocks\n", (unsigned)(sizeof(cases)/sizeof(cases[0])));
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
