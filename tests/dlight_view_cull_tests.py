"""A dynamic light the view cannot see lights none of the view's surfaces.

The world walk passed a light by when its reach lay wholly outside one of the
view's four side planes (R_CullDlight), but a model tested its lights only
against its own bounds. A light behind the eye therefore still listed the
models in view that its sphere touched, and the shadow planner gave it a cube
of six faces. On Medal of Honor's m1l1 truck ride the lantern hanging behind
the player cost 4 ms of a frozen frame for no pixel
(docs/fntech3/2026-10-26-mohaa-truck-ride-lights.md).

code/renderercommon/tr_dlight_view_cull.h marks such a light once a view and
R_LightCullBounds refuses it. The mark is compiled here beside each
renderer's own R_CullDlight and must agree with it on every light; the wiring
in the three renderers is checked too.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

HOST = r'''
#include <cmath>
#include <cstdio>
#include <cstdlib>
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec3_t[3];
typedef struct cplane_s { vec3_t normal; float dist; unsigned char type, signbits, pad[2]; } cplane_t;
typedef struct { int integer; } cvar_t;
#define ID_INLINE inline
#define USE_PMLIGHT
#define DotProduct( a, b ) ( (a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2] )
@CULL@
typedef struct {
	vec3_t origin, origin2, transformed, transformed2;
	float radius;
	qboolean linear;
	qboolean outsideView;
} dlight_t;
static struct {
	struct { cplane_t frustum[5]; dlight_t *dlights; int num_dlights; } viewParms;
} tr;
static cvar_t nocull;
static cvar_t *r_nocull = &nocull;
#include "code/renderercommon/tr_dlight_view_cull.h"
@RENDERERS@

static float Random( float low, float high ) { return low + ( high - low ) * ( rand() / (float)RAND_MAX ); }

static void Normalize( float *v ) {
	const float length = std::sqrt( DotProduct( v, v ) );
	v[0] /= length; v[1] /= length; v[2] /= length;
}

static void Cross( const float *a, const float *b, float *c ) {
	c[0] = a[1] * b[2] - a[2] * b[1];
	c[1] = a[2] * b[0] - a[0] * b[2];
	c[2] = a[0] * b[1] - a[1] * b[0];
}

/* the planes R_SetupFrustum builds from an eye, its axes and half-angles */
static void Frustum( const float *eye, const float axis[3][3], float halfX, float halfY ) {
	const float xs = std::sin( halfX ), xc = std::cos( halfX ), ys = std::sin( halfY ), yc = std::cos( halfY );
	for ( int i = 0; i < 4; i++ ) {
		const float s = i < 2 ? xs : ys, c = ( i < 2 ? xc : yc ) * ( ( i & 1 ) ? -1.0f : 1.0f );
		const float *side = i < 2 ? axis[1] : axis[2];
		for ( int k = 0; k < 3; k++ ) {
			tr.viewParms.frustum[i].normal[k] = axis[0][k] * s + side[k] * c;
		}
		tr.viewParms.frustum[i].dist = DotProduct( eye, tr.viewParms.frustum[i].normal );
	}
}

static void Axis( const float *forward, float axis[3][3] ) {
	float up[3] = { 0, 0, 1 };
	for ( int k = 0; k < 3; k++ ) axis[0][k] = forward[k];
	Normalize( axis[0] );
	if ( std::fabs( axis[0][2] ) > 0.95f ) { up[0] = 1; up[2] = 0; }
	Cross( up, axis[0], axis[1] );
	Normalize( axis[1] );
	Cross( axis[0], axis[1], axis[2] );
}

static int failures;
#define CHECK( x ) do { if ( !( x ) ) { printf( "line %d: %s\n", __LINE__, #x ); failures++; } } while ( 0 )

int main( void ) {
	const float eye[3] = { 2541.8f, 86.7f, 1006.9f };
	const float forward[3] = { 0.91f, 0.43f, 0.0f };
	float axis[3][3];
	dlight_t lights[4];
	int outside = 0, inside = 0;

	/* m1l1's frozen frame: the lantern 76 units behind the eye plane at radius 64
	 * to 76 is out (the widened 16:9 field's far side puts it 93 out); at twice
	 * that radius it reaches in */
	Axis( forward, axis );
	Frustum( eye, axis, 48.5f * 3.14159265f / 180.0f, 30.0f * 3.14159265f / 180.0f );
	{
		const vec3_t lantern = { 2496.6f, 4.8f, 1012.2f };
		CHECK( R_DlightOutsideFrustum( lantern, lantern, qfalse, 76.0f, tr.viewParms.frustum ) );
		CHECK( !R_DlightOutsideFrustum( lantern, lantern, qfalse, 152.0f, tr.viewParms.frustum ) );
		/* a light at the eye, or ahead of it, is in */
		CHECK( !R_DlightOutsideFrustum( eye, eye, qfalse, 1.0f, tr.viewParms.frustum ) );
	}

	/* a linear light is out only when both ends are out past the same plane */
	{
		const vec3_t behind = { eye[0] - 400.0f * axis[0][0], eye[1] - 400.0f * axis[0][1], eye[2] };
		const vec3_t ahead = { eye[0] + 400.0f * axis[0][0], eye[1] + 400.0f * axis[0][1], eye[2] };
		CHECK( R_DlightOutsideFrustum( behind, behind, qtrue, 10.0f, tr.viewParms.frustum ) );
		CHECK( !R_DlightOutsideFrustum( behind, ahead, qtrue, 10.0f, tr.viewParms.frustum ) );
		CHECK( !R_DlightOutsideFrustum( ahead, behind, qtrue, 10.0f, tr.viewParms.frustum ) );
	}

	/* the mark agrees with every renderer's R_CullDlight on random views and lights */
	srand( 26 );
	for ( int view = 0; view < 200; view++ ) {
		const float at[3] = { Random( -4000, 4000 ), Random( -4000, 4000 ), Random( -500, 1500 ) };
		const float look[3] = { Random( -1, 1 ), Random( -1, 1 ), Random( -0.6f, 0.6f ) };
		if ( DotProduct( look, look ) < 0.01f ) continue;
		Axis( look, axis );
		Frustum( at, axis, Random( 0.2f, 1.4f ), Random( 0.2f, 1.2f ) );
		for ( int n = 0; n < 200; n++ ) {
			dlight_t dl = dlight_t();
			for ( int k = 0; k < 3; k++ ) {
				dl.origin[k] = at[k] + Random( -600, 600 );
				dl.origin2[k] = at[k] + Random( -600, 600 );
			}
			dl.radius = Random( 1, 400 );
			dl.linear = ( n & 1 ) ? qtrue : qfalse;
			for ( int k = 0; k < 3; k++ ) {
				dl.transformed[k] = dl.origin[k];
				dl.transformed2[k] = dl.origin2[k];
			}
			const qboolean out = R_DlightOutsideFrustum( dl.origin, dl.origin2, dl.linear, dl.radius,
				tr.viewParms.frustum );
			@COMPARE@
			out ? outside++ : inside++;
		}
	}
	CHECK( outside > 5000 && inside > 5000 );

	/* the view's lights are marked; r_nocull keeps every one */
	Axis( forward, axis );
	Frustum( eye, axis, 48.5f * 3.14159265f / 180.0f, 30.0f * 3.14159265f / 180.0f );
	for ( int i = 0; i < 4; i++ ) {
		lights[i] = dlight_t();
		lights[i].radius = 76.0f;
		lights[i].outsideView = i == 1 ? qtrue : qfalse;
	}
	lights[0].origin[0] = 2496.6f; lights[0].origin[1] = 4.8f; lights[0].origin[2] = 1012.2f;
	for ( int k = 0; k < 3; k++ ) lights[1].origin[k] = eye[k] + 100.0f * axis[0][k];
	lights[2] = lights[0]; lights[2].radius = 152.0f;
	lights[3] = lights[0];
	tr.viewParms.dlights = lights;
	tr.viewParms.num_dlights = 3;
	R_MarkDlightsOutsideView();
	CHECK( lights[0].outsideView && !lights[1].outsideView && !lights[2].outsideView );
	CHECK( !lights[3].outsideView );
	nocull.integer = 1;
	R_MarkDlightsOutsideView();
	CHECK( !lights[0].outsideView && !lights[1].outsideView && !lights[2].outsideView );

	if ( failures ) return 1;
	printf( "ok\n" );
	return 0;
}
'''


def read(*parts):
    return (ROOT.joinpath(*parts)).read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start) + 2]


def section(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first)]


class DlightViewCullTests(unittest.TestCase):
    def test_mark_agrees_with_every_renderers_frustum_cull(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        cull = '\n'.join(re.findall(r'^#define\s+CULL_(?:IN|CLIP|OUT)\b[^\n]*', read('code', 'renderer', 'tr_local.h'),
                                    re.M))
        functions, compare = [], []
        for renderer in RENDERERS:
            self.assertEqual(cull, '\n'.join(re.findall(r'^#define\s+CULL_(?:IN|CLIP|OUT)\b[^\n]*',
                                                         read('code', renderer, 'tr_local.h'), re.M)))
            body = function(read('code', renderer, 'tr_main.c'), 'int R_CullDlight( const dlight_t* dl )')
            functions.append(body.replace('int R_CullDlight(', f'static int R_CullDlight_{renderer}('))
            compare.append(f'CHECK( ( R_CullDlight_{renderer}( &dl ) == CULL_OUT ) == !!out );')
        host = (HOST.replace('@CULL@', cull).replace('@RENDERERS@', '\n'.join(functions))
                .replace('@COMPARE@', '\n\t\t\t'.join(compare)))
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='dlight-view-cull-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'host.cpp', Path(temp) / 'host.exe'
            cpp.write_text(host, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-ffp-contract=off',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual(run.stdout.strip(), 'ok')

    def test_every_renderer_marks_its_views_lights(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                local = read('code', renderer, 'tr_local.h')
                record = section(local, 'typedef struct dlight_s {', '} dlight_t;')
                self.assertIn('\tstruct litSurf_s\t*tail;\n', record)
                self.assertIn('\tqboolean outsideView;\n', record)

                world = read('code', renderer, 'tr_world.c')
                bounds = function(world, 'qboolean R_LightCullBounds( const dlight_t* dl, const vec3_t mins, '
                                         'const vec3_t maxs )')
                self.assertTrue(bounds.split('{', 1)[1].startswith('\n\tif ( dl->outsideView ) {\n\t\treturn qtrue;'),
                                bounds[:200])

                main = read('code', renderer, 'tr_main.c')
                self.assertLess(main.index('#include "../renderercommon/tr_dlight_view_cull.h"'),
                                main.index('static void R_GenerateDrawSurfs( void ) {'))
                generate = function(main, 'static void R_GenerateDrawSurfs( void ) {')
                # marked after the view's lists are emptied, before any surface is added
                self.assertLess(generate.index('tr.viewParms.dlights[i].head = tr.viewParms.dlights[i].tail = NULL;'),
                                generate.index('R_MarkDlightsOutsideView();'))
                self.assertLess(generate.index('R_MarkDlightsOutsideView();'), generate.index('R_AddWorldSurfaces ();'))
                self.assertLess(generate.index('R_MarkDlightsOutsideView();'), generate.index('R_AddEntitySurfaces();'))

        # a planned light's casters are gathered whatever the view sees
        casters = read('code', 'renderercommon', 'tr_dlight_shadow_casters.h')
        planned = function(casters, 'void R_AddPlannedDlightShadowCasters( void )')
        self.assertLess(planned.index('shadowLight = *dl;'), planned.index('shadowLight.outsideView = qfalse;'))
        self.assertLess(planned.index('shadowLight.outsideView = qfalse;'),
                        planned.index('context.cullLight = shadowLight;'))


if __name__ == '__main__':
    unittest.main()
