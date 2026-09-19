"""Elite Force's environment mapping (stvoyHM.exe RB_CalcEnvironmentTexCoords
0x47cb50) in all three renderers.

The probe compiles renderercommon/tr_stvef_env.h against a minimal tess and
backEnd and checks both of retail's branches on known vectors. The source
checks pin that each renderer takes that path for Elite Force, first person
or not, and that the GPU environment shortcuts, which compute Quake III's
mapping, are off for it.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

PROBE = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef enum { qfalse, qtrue } qboolean;
typedef float vec3_t[3];
#define ID_INLINE inline
#define RF_FIRST_PERSON 4
#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
static void VectorNormalizeFast( vec3_t v ) {
	const float length = sqrtf( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( length ) { v[0] /= length; v[1] /= length; v[2] /= length; }
}
enum { COM_GAME_QUAKE3, COM_GAME_STVEF_SP, COM_GAME_STVEF_MP };
static struct { int gameId; } ri;
typedef struct { struct { int renderfx; } e; vec3_t lightDir; } trRefEntity_t;
static struct { trRefEntity_t *currentEntity; struct { vec3_t viewOrigin; } or; } backEnd;
static struct { int numVertexes; float xyz[4][4]; float normal[4][4]; } tess;
#include "code/renderercommon/tr_stvef_env.h"
static int Near( float a, float b ) { return fabsf( a - b ) < 1e-5f; }
int main( void ) {
	trRefEntity_t entity;
	float st[8];
	memset( &entity, 0, sizeof( entity ) );

	ri.gameId = COM_GAME_QUAKE3; assert( !R_STVEFEnvironment() );
	ri.gameId = COM_GAME_STVEF_SP; assert( R_STVEFEnvironment() );
	ri.gameId = COM_GAME_STVEF_MP; assert( R_STVEFEnvironment() );

	/* A surface facing the viewer head on: v = n, so s = n.x - v.x / 2. */
	tess.numVertexes = 2;
	tess.xyz[0][0] = 0; tess.xyz[0][1] = 0; tess.xyz[0][2] = 0;
	tess.normal[0][0] = 1; tess.normal[0][1] = 0; tess.normal[0][2] = 0;
	/* One seen from the side: v = (0.6, 0.8, 0), n = (0, 1, 0). */
	tess.xyz[1][0] = 0; tess.xyz[1][1] = 0; tess.xyz[1][2] = 0;
	tess.normal[1][0] = 0; tess.normal[1][1] = 1; tess.normal[1][2] = 0;
	backEnd.or.viewOrigin[0] = 30; backEnd.or.viewOrigin[1] = 0; backEnd.or.viewOrigin[2] = 0;
	backEnd.currentEntity = &entity;
	RB_CalcEnvironmentTexCoordsSTVEF( st );
	assert( Near( st[0], 1 - 0.5f ) && Near( st[1], 0 ) );
	/* second vertex: v = (1, 0, 0) too, d = 0: s = -0.5, t = 0 */
	assert( Near( st[2], -0.5f ) && Near( st[3], 0 ) );
	backEnd.or.viewOrigin[0] = 6; backEnd.or.viewOrigin[1] = 8;
	RB_CalcEnvironmentTexCoordsSTVEF( st );
	/* v = (0.6, 0.8, 0). First: d = 0.6, s = 0.6 - 0.3, t = 0 - 0.4. */
	assert( Near( st[0], 0.3f ) && Near( st[1], -0.4f ) );
	/* Second: d = 0.8, s = 0 - 0.3, t = 0.8 - 0.4. */
	assert( Near( st[2], -0.3f ) && Near( st[3], 0.4f ) );

	/* First person: the entity's light direction L, with no halving. */
	entity.e.renderfx = RF_FIRST_PERSON;
	entity.lightDir[0] = 0; entity.lightDir[1] = 0.6f; entity.lightDir[2] = 0.8f;
	RB_CalcEnvironmentTexCoordsSTVEF( st );
	/* First: d = 0, s = -L.x, t = -L.y. Second: d = 0.6, s = 0, t = 0.6 - 0.6. */
	assert( Near( st[0], 0 ) && Near( st[1], -0.6f ) );
	assert( Near( st[2], 0 ) && Near( st[3], 0 ) );
	backEnd.currentEntity = NULL;   /* no entity: the view branch */
	RB_CalcEnvironmentTexCoordsSTVEF( st );
	assert( Near( st[0], 0.3f ) && Near( st[1], -0.4f ) );
	puts( "ok" );
	return 0;
}
'''


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def body(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start)]


class STVEFEnvironmentMapTests(unittest.TestCase):
    def test_retail_formula(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-env-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source = folder / 'probe.c'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(PROBE, encoding='utf-8')
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c99', '-I', str(ROOT),
                str(source), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('ok', run.stdout)

    def test_every_renderer_maps_elite_force_on_the_cpu(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                calc = read(f'code/{renderer}/tr_shade_calc.c')
                self.assertIn('#include "../renderercommon/tr_stvef_env.h"', calc)
                for signature in ('void RB_CalcEnvironmentTexCoords( float *st )',
                                  'void RB_CalcEnvironmentTexCoordsFP( float *st,'):
                    self.assertIn('if ( R_STVEFEnvironment() ) {', body(calc, signature)[:600])
        vbo = read('code/renderer/tr_vbo.c')
        self.assertIn('ri.gameId != COM_GAME_STVEF_SP && ri.gameId != COM_GAME_STVEF_MP ) {',
                      body(vbo, 'static qboolean isStaticTCgen('))
        for renderer in ('renderervk', 'rendererrtx'):
            shader = read(f'code/{renderer}/tr_shader.c')
            self.assertIn('''			if ( env_mask == 1 && !pStage->depthFragment &&
				ri.gameId != COM_GAME_STVEF_SP && ri.gameId != COM_GAME_STVEF_MP ) {''', shader)


if __name__ == '__main__':
    unittest.main()
