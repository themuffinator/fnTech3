"""Alice's and F.A.K.K.2's projected dynamic light follows each vertex's normal.

In the projected pass (r_dlightMode 0) Quake III maps every vertex into the
light's image by world x and y and fades the light by height. Retail Ritual
maps it across the plane of its normal's dominant axis -- y and z for a
normal mostly along x, else x and z for one mostly along y, else x and y --
with the image spanning the radius over 0.75 about its middle, and fades it
by the distance along the normal, (radius - |dist . normal|) / radius
(fakk2.exe 1.02 0x0047d0a1..0x0047d268, alice.exe 0x004932b3..0x0049348b).
A wall beside the light on creeperpens takes the dung pile's glow that way.

The helper is compiled against stand-in types; the three renderers' projected
passes must route Ritual vertices through it before Quake III's mapping.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

HOST = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec3_t[3];
#define ID_INLINE inline
#define DotProduct( a, b ) ( (a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2] )
#include "code/renderercommon/tr_ritual_projected_light.h"
static bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }
int main() {
	float st[2], m;
	/* a wall facing +x, 66 units from a light of radius 96: retail lights it
	 * at 30/96, where Quake III's x would put it outside the image */
	const vec3_t wall = { 1, 0, 0 }, beside = { 66, 10, -24 };
	int clip = RB_RitualProjectDlightVertex( beside, wall, 96, qfalse, st, &m );
	assert( clip == 0 && Near( st[0], 0.5f + 10 * 0.75f / 96 ) && Near( st[1], 0.5f - 24 * 0.75f / 96 ) );
	assert( Near( m, 30.0f / 96 ) );
	assert( 0.5f + 66.0f / 96 > 1.0f );
	/* a floor: x and y, and the fade by height along the normal */
	const vec3_t floor = { 0, 0, 1 }, above = { 12, -36, 48 };
	clip = RB_RitualProjectDlightVertex( above, floor, 96, qfalse, st, &m );
	assert( clip == 0 && Near( st[0], 0.5f + 12 * 0.75f / 96 ) && Near( st[1], 0.5f - 36 * 0.75f / 96 ) );
	assert( Near( m, 0.5f ) );
	/* a normal mostly along y: x and z */
	const vec3_t side = { 0.3f, -0.9f, 0.3f }, near = { 20, -20, 8 };
	clip = RB_RitualProjectDlightVertex( near, side, 96, qfalse, st, &m );
	assert( Near( st[0], 0.5f + 20 * 0.75f / 96 ) && Near( st[1], 0.5f + 8 * 0.75f / 96 ) );
	/* ties go as retail's comparisons do: x only when it is the strict
	 * largest, then y only over z */
	const vec3_t xy = { 0.7f, 0.7f, 0.1f }, yz = { 0.1f, 0.7f, 0.7f }, one = { 1, 2, 3 };
	RB_RitualProjectDlightVertex( one, xy, 96, qfalse, st, &m );
	assert( Near( st[1], 0.5f + 3 * 0.75f / 96 ) && Near( st[0], 0.5f + 1 * 0.75f / 96 ) );
	RB_RitualProjectDlightVertex( one, yz, 96, qfalse, st, &m );
	assert( Near( st[0], 0.5f + 1 * 0.75f / 96 ) && Near( st[1], 0.5f + 2 * 0.75f / 96 ) );
	/* past the radius on either side of the face: no light, and clipped */
	const vec3_t far = { 100, 0, 0 }, behind = { -100, 0, 0 };
	clip = RB_RitualProjectDlightVertex( far, wall, 96, qfalse, st, &m );
	assert( ( clip & 16 ) && m == 0 );
	clip = RB_RitualProjectDlightVertex( behind, wall, 96, qfalse, st, &m );
	assert( ( clip & 32 ) && m == 0 );
	/* behind the face, lit both ways unless the caller culls back faces */
	const vec3_t back = { -40, 0, 0 };
	clip = RB_RitualProjectDlightVertex( back, wall, 96, qfalse, st, &m );
	assert( clip == 0 && Near( m, 56.0f / 96 ) );
	clip = RB_RitualProjectDlightVertex( back, wall, 96, qtrue, st, &m );
	assert( clip == 63 && m == 0 );
	/* outside the image: the clip bits of each edge */
	const vec3_t wide = { 10, 80, -80 };
	clip = RB_RitualProjectDlightVertex( wide, wall, 96, qfalse, st, &m );
	assert( ( clip & 2 ) && ( clip & 4 ) && !( clip & 1 ) && !( clip & 8 ) );
	std::puts( "ok" );
	return 0;
}
'''


def read(*parts):
    return (ROOT.joinpath(*parts)).read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')


class RitualProjectedLightTests(unittest.TestCase):
    def test_projection(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        with tempfile.TemporaryDirectory(prefix='ritual-projected-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'host.cpp', Path(temp) / 'host.exe'
            cpp.write_text(HOST, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual(run.stdout.strip(), 'ok')

    def test_projected_passes_route_ritual_vertices(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                text = read('code', renderer, 'tr_shade.c')
                self.assertIn('#include "../renderercommon/tr_ritual_projected_light.h"', text)
                start = text.index('backEnd.pc.c_dlightVertexes++;')
                ritual = text.index('if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {\n'
                                    '\t\t\t\tconst float *own = dl->ritualRadius > 0.0f ? dl->ritualColor : dl->color;',
                                    start)
                quake = text.index('texCoords[0] = 0.5f + dist[0] * scale;', start)
                self.assertLess(ritual, quake)
                block = text[ritual:quake]
                self.assertIn('clipBits[i] = RB_RitualProjectDlightVertex( dist, tess.normal[i], radius,', block)
                self.assertRegex(block, r'own\[(0|k)\] \* modulate')
                self.assertIn('continue;', block)
                if renderer != 'renderer':
                    # bytes there: clamped, never wrapped
                    self.assertIn('value >= 255.0f ? 255 : (byte)value', block)

    def test_gl_tessellates_normals_for_the_projection(self):
        # GL fills tess.normal only for shaders that ask (USE_TESS_NEEDS_NORMAL);
        # the Ritual projection reads it on every lit vertex
        self.assertIn('#define USE_TESS_NEEDS_NORMAL', read('code', 'renderer', 'tr_local.h'))
        shade = read('code', 'renderer', 'tr_shade.c')
        self.assertIn('if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) && !R_GetDlightMode() && '
                      'backEnd.refdef.num_dlights > 0 ) {\n\t\ttess.needsNormal = qtrue;', shade)

    def test_glx_gpu_projection_leaves_ritual_to_the_cpu_pass(self):
        # GLX's developer-only GPU projection evaluates Quake III's rule
        shade = read('code', 'renderer', 'tr_shade.c')
        self.assertIn('projectedProgram = ( lightMask && GLX_CompatDlightProjectedProgramEnabled() &&\n'
                      '\t\t!COM_GAME_ID_IS_RITUAL( ri.gameId ) ) ? qtrue : qfalse;', shade)
        vbo = read('code', 'renderer', 'tr_vbo.c')
        self.assertIn('if ( !GLX_CompatProjectedDlightWorldOverlayActive() || COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {',
                      vbo)


if __name__ == '__main__':
    unittest.main()
