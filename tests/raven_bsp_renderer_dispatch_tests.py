"""Execute all three Fn light-grid samplers with Raven index/style records."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_raven_bsp_format.h"
#include <cassert>
#include <cstring>
enum { RF_LIGHTING_ORIGIN = 0x80, FUNCTABLE_SIZE = 1024, FUNCTABLE_MASK = 1023 };
struct trRefEntity_t {
    struct { int renderfx; vec3_t lightingOrigin,origin; } e;
    vec3_t ambientLight,directedLight,lightDir;
};
struct world_t {
    vec3_t lightGridOrigin,lightGridInverseSize;
    int lightGridBounds[3]; byte *lightGridData; fnRavenBspView_t ravenBsp;
};
static struct { world_t *world; float sinTable[1024]; int rtcwWorld; } tr;
static cvar_t ambient{},directed{};
static cvar_t *r_ambientScale=&ambient,*r_directedScale=&directed;
static void FnRTCWWorldGridScale(const int *,float *,float *) {}
'''

CHECKS = r'''
int main() {
    world_t world{}; tr.world=&world; byte grid[64]{};
    world.lightGridData=grid; ambient.value=directed.value=1;
    for(int axis=0;axis<3;++axis) { world.lightGridBounds[axis]=2; world.lightGridInverseSize[axis]=1.0f/64; }
    for(int cell=0;cell<8;++cell) { grid[cell*8+3]=40; grid[cell*8+4]=50; grid[cell*8+5]=60; }
    for(int i=0;i<1024;++i) tr.sinTable[i]=static_cast<float>(sin(i*2.0*M_PI/1023));
    // Pool row 0 is a wall; row 1 is valid black ambient with directed light.
    // The compressed array deliberately repeats/permutates those rows.
    byte original[76]{}; original[24]=255;
    original[54]=0; original[55]=original[56]=original[57]=255;
    for(int cell=0;cell<8;++cell) original[60+cell*2]=1;
    world.ravenBsp.bytes=original; world.ravenBsp.length=sizeof(original);
    world.ravenBsp.offsets[15]=0; world.ravenBsp.offsets[17]=60;
    world.ravenBsp.gridRows=2; world.ravenBsp.gridCells=8;
    trRefEntity_t entity{}; entity.e.renderfx=RF_LIGHTING_ORIGIN;
    VectorSet(entity.e.lightingOrigin,32,32,32);
    R_SetupEntityLightingGrid(&entity);
    assert(entity.ambientLight[0]==0 && entity.directedLight[0]==40 && entity.directedLight[1]==50 && entity.directedLight[2]==60);
    // A hole with deliberately bright projected bytes must never contribute;
    // the remaining samples retain the existing Fn weight normalization.
    original[60]=0; grid[3]=240;
    R_SetupEntityLightingGrid(&entity);
    assert(fabsf(entity.directedLight[0]-40)<0.0001f && fabsf(entity.directedLight[1]-50)<0.0001f && fabsf(entity.directedLight[2]-60)<0.0001f);
    for(int cell=0;cell<8;++cell) original[60+cell*2]=0;
    R_SetupEntityLightingGrid(&entity); assert(entity.directedLight[0]==0);
    // The pre-existing Quake III/QL/RTCW/ET/EF heuristic stays unchanged when
    // the world owns no Raven records: black ambient is still skipped.
    world.ravenBsp={}; R_SetupEntityLightingGrid(&entity); assert(entity.directedLight[0]==0);
    grid[0]=10; grid[1]=20; grid[2]=30;
    VectorClear(entity.e.lightingOrigin); R_SetupEntityLightingGrid(&entity);
    assert(entity.ambientLight[0]==10 && entity.ambientLight[1]==20 && entity.ambientLight[2]==30 && entity.directedLight[0]==240);
    // Reinitializing a Raven world replaces all prior validity metadata.
    world.ravenBsp.bytes=original; world.ravenBsp.offsets[17]=60; world.ravenBsp.gridRows=2; world.ravenBsp.gridCells=8;
    original[60]=1; R_SetupEntityLightingGrid(&entity); assert(entity.directedLight[0]==240);
}
'''


class RavenBspRendererDispatchTests(unittest.TestCase):
    def test_real_grid_samplers_and_other_profile_branch(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang C++ compiler unavailable')
        math_source = (ROOT / 'code/qcommon/q_math.c').read_text()
        normalize = function(math_source, 'vec_t VectorNormalize2(')
        for renderer in RENDERERS:
            source = (ROOT / 'code' / renderer / 'tr_light.c').read_text()
            grid = function(source, 'static void R_SetupEntityLightingGrid(')
            for flags in ([], ['-O2', '-ffast-math']):
                with self.subTest(renderer=renderer, flags=flags):
                    with tempfile.TemporaryDirectory(prefix='raven-grid-', dir=ROOT / '.tmp/jka-compat') as temp:
                        folder = Path(temp); probe = folder / 'probe.cpp'
                        executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                        probe.write_text('\n'.join((PRELUDE, normalize, grid, CHECKS)))
                        compiled = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17',
                            '-Wall', '-Wextra', '-Werror', *flags, '-I', str(ROOT), str(probe), '-o', str(executable)],
                            cwd=ROOT, capture_output=True, text=True, timeout=60)
                        self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
                        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                        executed = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                        self.assertEqual(executed.returncode, 0, executed.stdout + executed.stderr)


if __name__ == '__main__':
    unittest.main()
