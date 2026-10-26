"""A dynamic light's shadow casters are built once for its six cube faces.

Each face of a point light's shadow cube used to tessellate every caster again
and push each vertex along its normal on the CPU, although the push reads
only the light's origin and the entity's orientation and the six faces get the
same vertices. code/renderercommon/tr_dlight_caster_reuse.h keeps the first
face's pushed vertices for the light's other faces. On Medal of Honor's m1l1
it took most of a Vulkan frame (docs/fntech3/2026-10-25-mohaa-frame-time.md).

The push moved into the header unchanged: it is compiled here beside the loop
the three renderers ran before and must give the same floats. The store, and
the renderers' use of it, are checked too.
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
#include <cstdlib>
#include <cstring>
#include <initializer_list>
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec3_t[3];
typedef float vec4_t[4];
typedef struct { vec3_t origin; vec3_t axis[3]; } orientationr_t;
enum { SF_BAD, SF_FACE, SF_GRID, SF_TRIANGLES, SF_MD3, SF_MDS, SF_WOLFET, SF_ALICE_TIKI, SF_MOHAA_TIKI };
#define ID_INLINE inline
#define Com_Memcpy memcpy
#define DotProduct( a, b ) ( (a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2] )
#define VectorSubtract( a, b, c ) ( (c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2] )
#define VectorCopy( a, b ) ( (b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2] )
#define VectorScale( v, s, o ) ( (o)[0] = (v)[0] * (s), (o)[1] = (v)[1] * (s), (o)[2] = (v)[2] * (s) )
#define VectorMA( v, s, b, o ) ( (o)[0] = (v)[0] + (b)[0] * (s), (o)[1] = (v)[1] + (b)[1] * (s), (o)[2] = (v)[2] + (b)[2] * (s) )
static float Com_Clamp( float min, float max, float value ) { return value < min ? min : value > max ? max : value; }
static float VectorNormalize( float *v ) {
	float length = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( length ) { float il = 1.0f / length; v[0] *= il; v[1] *= il; v[2] *= il; }
	return length;
}
qboolean rb_dlightCasterOffsetApplied;
unsigned int rb_surfaceBegins;
#include "code/renderercommon/tr_dlight_caster_reuse.h"

/* the loop the three renderers' RB_ApplyDlightShadowCasterNormalBias ran */
static void Before( vec4_t *xyz, const vec4_t *normal, int count, const orientationr_t *orient,
	const float *lightOrigin, const float *csmDirection, float normalBias ) {
	for ( int i = 0; i < count; i++ ) {
		vec3_t world, normalWorld, lightToVertex;
		float normalLength, lightSide, normalScale, scale;
		normalWorld[0] = normal[i][0] * orient->axis[0][0] + normal[i][1] * orient->axis[1][0] + normal[i][2] * orient->axis[2][0];
		normalWorld[1] = normal[i][0] * orient->axis[0][1] + normal[i][1] * orient->axis[1][1] + normal[i][2] * orient->axis[2][1];
		normalWorld[2] = normal[i][0] * orient->axis[0][2] + normal[i][1] * orient->axis[1][2] + normal[i][2] * orient->axis[2][2];
		normalLength = VectorNormalize( normalWorld );
		if ( normalLength <= 0.0f ) continue;
		if ( csmDirection ) {
			VectorScale( csmDirection, -1.0f, lightToVertex );
		} else {
			world[0] = orient->origin[0] + xyz[i][0] * orient->axis[0][0] + xyz[i][1] * orient->axis[1][0] + xyz[i][2] * orient->axis[2][0];
			world[1] = orient->origin[1] + xyz[i][0] * orient->axis[0][1] + xyz[i][1] * orient->axis[1][1] + xyz[i][2] * orient->axis[2][1];
			world[2] = orient->origin[2] + xyz[i][0] * orient->axis[0][2] + xyz[i][1] * orient->axis[1][2] + xyz[i][2] * orient->axis[2][2];
			VectorSubtract( world, lightOrigin, lightToVertex );
			if ( VectorNormalize( lightToVertex ) <= 0.0f ) continue;
		}
		lightSide = DotProduct( normalWorld, lightToVertex );
		normalScale = 0.25f + 0.50f * ( 1.0f - Com_Clamp( 0.0f, 1.0f, std::fabs( lightSide ) ) );
		scale = ( normalBias * normalScale ) / normalLength;
		if ( lightSide < 0.0f ) scale = -scale;
		VectorMA( xyz[i], scale, normal[i], xyz[i] );
	}
}

static float Random( float span ) { return ( (float)std::rand() / RAND_MAX * 2.0f - 1.0f ) * span; }

static dlightCasterReuse_t reuse;
static vec4_t a[512], b[512], normal[512], tess[4096];
static unsigned int tessIndexes[12288];

int main() {
	std::srand( 1025 );
	/* a turned, moved entity; a vertex on the light; a normal of nought */
	orientationr_t orient;
	const float turn = 0.6f;
	orient.origin[0] = 120; orient.origin[1] = -64; orient.origin[2] = 18;
	orient.axis[0][0] = std::cos( turn ); orient.axis[0][1] = std::sin( turn ); orient.axis[0][2] = 0;
	orient.axis[1][0] = -std::sin( turn ); orient.axis[1][1] = std::cos( turn ); orient.axis[1][2] = 0;
	orient.axis[2][0] = 0; orient.axis[2][1] = 0; orient.axis[2][2] = 1;
	const float light[3] = { 150, -40, 60 };
	const float sun[3] = { 0.3f, -0.2f, -0.93f };
	for ( int i = 0; i < 512; i++ ) {
		for ( int k = 0; k < 3; k++ ) { a[i][k] = Random( 200 ); normal[i][k] = Random( 1.5f ); }
		a[i][3] = 1; normal[i][3] = 0;
	}
	normal[3][0] = normal[3][1] = normal[3][2] = 0;
	/* the vertex that lands on the light once turned and moved */
	{
		const float rel[3] = { light[0] - orient.origin[0], light[1] - orient.origin[1], light[2] - orient.origin[2] };
		for ( int k = 0; k < 3; k++ ) a[5][k] = DotProduct( rel, orient.axis[k] );
	}
	for ( int pass = 0; pass < 2; pass++ ) {
		for ( float bias : { 0.25f, 1.0f, 8.0f } ) {
			std::memcpy( b, a, sizeof( a ) );
			vec4_t before[512];
			std::memcpy( before, a, sizeof( a ) );
			Before( before, normal, 512, &orient, light, pass ? sun : nullptr, bias );
			float toward[3];
			if ( pass ) VectorScale( sun, -1.0f, toward );
			R_DlightShadowCasterOffset( b, normal, 0, 512, &orient, light, pass ? toward : nullptr, bias );
			assert( std::memcmp( before, b, sizeof( b ) ) == 0 );
		}
	}
	/* a range leaves the rest alone */
	std::memcpy( b, a, sizeof( a ) );
	R_DlightShadowCasterOffset( b, normal, 100, 200, &orient, light, nullptr, 0.25f );
	assert( std::memcmp( b, a, 100 * sizeof( vec4_t ) ) == 0 );
	assert( std::memcmp( b + 200, a + 200, 312 * sizeof( vec4_t ) ) == 0 );
	assert( std::memcmp( b + 100, a + 100, 100 * sizeof( vec4_t ) ) != 0 );

	/* the casters no face's axis changes */
	assert( R_DlightCasterReusable( SF_FACE ) && R_DlightCasterReusable( SF_TRIANGLES ) );
	assert( R_DlightCasterReusable( SF_MD3 ) && R_DlightCasterReusable( SF_ALICE_TIKI ) );
	assert( R_DlightCasterReusable( SF_MOHAA_TIKI ) );
	assert( !R_DlightCasterReusable( SF_GRID ) && !R_DlightCasterReusable( SF_MDS ) );
	assert( !R_DlightCasterReusable( SF_WOLFET ) && !R_DlightCasterReusable( SF_BAD ) );

	/* a light's first face keeps a caster written at 40.. in tess */
	R_DlightCasterReuseBeginLight( &reuse );
	assert( !R_DlightCasterReuseKept( &reuse, 7 ) );
	for ( int i = 0; i < 4; i++ ) { tess[40 + i][0] = (float)i; tess[40 + i][1] = 9; tess[40 + i][2] = 0; tess[40 + i][3] = 1; }
	const unsigned int quad[6] = { 40, 41, 42, 40, 42, 43 };
	std::memcpy( tessIndexes + 90, quad, sizeof( quad ) );
	R_DlightCasterReuseKeep( &reuse, 7, tess, tessIndexes, 40, 4, 90, 6 );
	const dlightCasterKept_t *kept = R_DlightCasterReuseKept( &reuse, 7 );
	assert( kept && kept->numVerts == 4 && kept->numIndexes == 6 && reuse.usedVerts == 4 );
	/* another face copies it after what it has, its indexes moved there */
	int numVerts = 10, numIndexes = 30;
	std::memset( tess, 0, sizeof( tess ) );
	R_DlightCasterReuseCopy( &reuse, kept, tess, tessIndexes, &numVerts, &numIndexes );
	assert( numVerts == 14 && numIndexes == 36 );
	assert( tess[10][0] == 0 && tess[13][0] == 3 && tess[12][1] == 9 && tess[11][3] == 1 );
	assert( tessIndexes[30] == 10 && tessIndexes[32] == 12 && tessIndexes[35] == 13 );
	/* a caster that names a vertex outside its own is not kept */
	const unsigned int stray[3] = { 40, 41, 44 };
	std::memcpy( tessIndexes + 90, stray, sizeof( stray ) );
	R_DlightCasterReuseKeep( &reuse, 8, tess, tessIndexes, 40, 4, 90, 3 );
	assert( !R_DlightCasterReuseKept( &reuse, 8 ) );
	/* nor one past the chain the store numbers, nor one without room */
	R_DlightCasterReuseKeep( &reuse, DLIGHT_CASTER_REUSE_SURFACES, tess, tessIndexes, 40, 4, 90, 6 );
	R_DlightCasterReuseKeep( &reuse, -1, tess, tessIndexes, 40, 4, 90, 6 );
	assert( !R_DlightCasterReuseKept( &reuse, DLIGHT_CASTER_REUSE_SURFACES ) && !R_DlightCasterReuseKept( &reuse, -1 ) );
	std::memcpy( tessIndexes + 90, quad, sizeof( quad ) );
	reuse.usedVerts = DLIGHT_CASTER_REUSE_VERTS - 3;
	R_DlightCasterReuseKeep( &reuse, 9, tess, tessIndexes, 40, 4, 90, 6 );
	assert( !R_DlightCasterReuseKept( &reuse, 9 ) && reuse.usedVerts == DLIGHT_CASTER_REUSE_VERTS - 3 );
	/* the next light keeps nothing of this one's */
	R_DlightCasterReuseBeginLight( &reuse );
	assert( !R_DlightCasterReuseKept( &reuse, 7 ) && reuse.usedVerts == 0 && reuse.usedIndexes == 0 );
	std::puts( "ok" );
	return 0;
}
'''


def read(*parts):
    return (ROOT.joinpath(*parts)).read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start) + 2]


class DlightCasterReuseTests(unittest.TestCase):
    def test_push_and_store(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='dlight-caster-reuse-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'host.cpp', Path(temp) / 'host.exe'
            cpp.write_text(HOST, encoding='utf-8')
            # no contraction: the renderers' builds keep each product rounded
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-ffp-contract=off',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual(run.stdout.strip(), 'ok')

    def test_every_renderer_builds_a_lights_casters_once(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                shade = read('code', renderer, 'tr_shade.c')
                self.assertIn('#include "../renderercommon/tr_dlight_caster_reuse.h"', shade)
                self.assertIn('qboolean rb_dlightCasterOffsetApplied;\nunsigned int rb_surfaceBegins;', shade)
                begin = function(shade, 'void RB_BeginSurface( shader_t *shader, int fogNum ) {')
                self.assertIn('\trb_surfaceBegins++;\n', begin)
                bias = function(shade, 'static void RB_ApplyDlightShadowCasterNormalBias( void )')
                self.assertLess(bias.index('if ( rb_dlightCasterOffsetApplied && !tess.csmCasterPass ) {\n\t\treturn;'),
                                bias.index('R_DlightShadowCasterOffset('))

                backend = read('code', renderer, 'tr_backend.c')
                self.assertIn('#include "../renderercommon/tr_dlight_caster_reuse.h"', backend)
                self.assertIn('\t\tRB_DlightShadowCasterBoundsMemoReset();\n'
                              '\t\tR_DlightCasterReuseBeginLight( &rb_dlightCasterReuse );', backend)
                casters = function(backend, 'static int RB_RenderDlightShadowCasters( const dlight_t *dl )')
                self.assertIn('reusable = reuse && R_DlightCasterReusable( *litSurf->surface ) ? qtrue : qfalse;',
                              casters)
                # a batch never mixes pushed casters with ones the iterator pushes
                self.assertIn('reusable != rb_dlightCasterOffsetApplied ) ) {', casters)
                self.assertIn('rb_dlightCasterOffsetApplied = reusable;', casters)
                kept = casters.index('const dlightCasterKept_t *kept = R_DlightCasterReuseKept( '
                                     '&rb_dlightCasterReuse, chainIndex );')
                copy = casters.index('R_DlightCasterReuseCopy( &rb_dlightCasterReuse, kept, tess.xyz, tess.indexes,')
                build = casters.index('begins = rb_surfaceBegins;')
                push = casters.index('R_DlightShadowCasterOffset( tess.xyz, tess.normal, firstVert, tess.numVertexes, '
                                     '&backEnd.or,')
                keep = casters.index('R_DlightCasterReuseKeep( &rb_dlightCasterReuse, chainIndex, tess.xyz, '
                                     'tess.indexes, firstVert,')
                self.assertLess(kept, copy)
                self.assertLess(copy, build)
                self.assertLess(build, push)
                self.assertLess(push, keep)
                # a copy that would overflow tess draws what tess holds first
                self.assertIn('if ( tess.numVertexes + kept->numVerts > SHADER_MAX_VERTEXES ||', casters)
                self.assertIn('if ( rb_surfaceBegins != begins ) {\n\t\t\t\tfirstVert = 0;', casters)
                self.assertGreater(casters.rindex('rb_dlightCasterOffsetApplied = qfalse;'),
                                   casters.rindex('RB_EndSurface();'))

                init = read('code', renderer, 'tr_init.c')
                self.assertIn('r_dlightShadowCasterReuse = ri.Cvar_Get( "r_dlightShadowCasterReuse", "1", '
                              'CVAR_ARCHIVE_ND );', init)
                self.assertIn('extern cvar_t\t*r_dlightShadowCasterReuse;', read('code', renderer, 'tr_local.h'))


if __name__ == '__main__':
    unittest.main()
