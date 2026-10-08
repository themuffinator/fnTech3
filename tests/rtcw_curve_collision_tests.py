"""RTCW's curves: how each mode cuts them into facets and finds a start inside.

Retail WolfMP.exe 1.41 starts a facet's winding 8192 units out from the
plane's point nearest the origin (BaseWindingForPlane 0x419ae8) and keeps a
facet no wider than 4096 units on any axis, with no other check
(CM_ValidateFacet 0x4185d7..0x4185f0). WolfSP.exe starts the winding 131072
units out (0x416fd8) and keeps Quake III's checks at that bound
(0x415987..0x4159bf). MP finds a start inside a curve by placing it against
each plane, widened by the box -- a capsule is tested as its box
(0x418f00) -- where SP keeps Quake III's capsule-aware test (0x4162d0).

The fixture compiles the engine's own patch generator and position test, as
alice_patch_collision_tests.py slices them, and feeds them synthetic patches;
no retail map is read. See docs/fntech3/2026-10-08-rtcw-traces.md.
"""
from pathlib import Path
import unittest

from alice_patch_collision_tests import GENERATOR_PRELUDE, build_and_run, function, generator_source

ROOT = Path(__file__).resolve().parents[1]

QUAKE3, RTCW_SP, RTCW_MP = 0, 5, 6

POSITION_PRELUDE = r'''
static qboolean CM_Q3TestPositionTest( const traceWork_t *tw, const struct patchCollide_s *pc ) {
	(void)tw; (void)pc; abort();
}
'''

CHECKS = r'''
/* A flat 3x3 patch, one facet: a square `width` across lying on z = 0, or a
 * 64-unit square standing in the x = 0 plane `along` units out on y. */
static int FlatFacets( int selectedGame, float width ) {
	vec3_t points[9];
	int i, j;
	game = selectedGame;
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			VectorSet( points[j * 3 + i], i * width * 0.5f, j * width * 0.5f, 0.0f );
		}
	}
	return CM_GeneratePatchCollide( 3, 3, points, SUBDIVIDE_DISTANCE )->numFacets;
}

static int StandingFacets( int selectedGame, float along ) {
	vec3_t points[9];
	int i, j;
	game = selectedGame;
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			VectorSet( points[j * 3 + i], 0.0f, along + i * 32.0f, j * 32.0f );
		}
	}
	return CM_GeneratePatchCollide( 3, 3, points, SUBDIVIDE_DISTANCE )->numFacets;
}

/* A 45-degree ramp 128 units square, and a player hull centred `distance`
 * units above its middle, along its normal: its box reaches 43 * sqrt(1/2),
 * about 30.4 units, along that normal, its capsule 15 + 13 * sqrt(1/2), about
 * 24.2. */
static qboolean StartsInRamp( int selectedGame, float distance, qboolean capsule ) {
	const float half = 0.70710678f;
	vec3_t points[9];
	traceWork_t tw;
	const struct patchCollide_s *pc;
	int i, j;
	game = selectedGame;
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			VectorSet( points[j * 3 + i], i * 64.0f, j * 64.0f, i * 64.0f );
		}
	}
	pc = CM_GeneratePatchCollide( 3, 3, points, SUBDIVIDE_DISTANCE );
	assert( pc->numFacets == 1 );

	memset( &tw, 0, sizeof( tw ) );
	VectorSet( tw.size[0], -15, -15, -28 );
	VectorSet( tw.size[1], 15, 15, 28 );
	for ( i = 0; i < 8; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			tw.offsets[i][j] = tw.size[( i >> j ) & 1][j];
		}
	}
	VectorSet( tw.start, 64.0f - distance * half, 64.0f, 64.0f + distance * half );
	VectorCopy( tw.start, tw.end );
	for ( j = 0; j < 3; j++ ) {
		tw.bounds[0][j] = tw.start[j] + tw.size[0][j];
		tw.bounds[1][j] = tw.start[j] + tw.size[1][j];
	}
	if ( capsule ) {
		tw.sphere.use = qtrue;
		tw.sphere.radius = 15;
		tw.sphere.halfheight = 28;
		VectorSet( tw.sphere.offset, 0, 0, 13 );
	}
	return CM_PositionTestInPatchCollide( &tw, pc );
}

int main( void ) {
	/* Quake III and SP keep a facet 5000 units across; MP keeps one no wider
	 * than 4096. */
	assert( FlatFacets( 0, 5000.0f ) == 1 );
	assert( FlatFacets( 5, 5000.0f ) == 1 );
	assert( FlatFacets( 6, 5000.0f ) == 0 );
	assert( FlatFacets( 6, 4000.0f ) == 1 );

	/* MP's first winding reaches 8192 units from the plane's point nearest
	 * the origin, so a facet standing in the x = 0 plane farther out than
	 * that along it is chopped away; SP's reaches 131072. */
	assert( StandingFacets( 0, 10000.0f ) == 1 );
	assert( StandingFacets( 5, 10000.0f ) == 1 );
	assert( StandingFacets( 6, 10000.0f ) == 0 );
	assert( StandingFacets( 6, 8000.0f ) == 1 );
	assert( StandingFacets( 0, 70000.0f ) == 0 );
	assert( StandingFacets( 5, 70000.0f ) == 1 );

	/* 27 units out the box reaches the ramp and the capsule does not: SP
	 * tests the capsule as Quake III does, MP tests its box. */
	assert( StartsInRamp( 0, 27.0f, qfalse ) && StartsInRamp( 5, 27.0f, qfalse ) );
	assert( StartsInRamp( 6, 27.0f, qfalse ) );
	assert( !StartsInRamp( 0, 27.0f, qtrue ) && !StartsInRamp( 5, 27.0f, qtrue ) );
	assert( StartsInRamp( 6, 27.0f, qtrue ) );
	/* 22 units out both reach it, and 34 neither, in every game. */
	assert( StartsInRamp( 0, 22.0f, qtrue ) && StartsInRamp( 5, 22.0f, qtrue ) && StartsInRamp( 6, 22.0f, qtrue ) );
	assert( !StartsInRamp( 0, 34.0f, qfalse ) && !StartsInRamp( 5, 34.0f, qfalse ) && !StartsInRamp( 6, 34.0f, qfalse ) );
	return 0;
}
'''


class RTCWCurveCollisionTests(unittest.TestCase):
    def test_each_mode_cuts_and_tests_curves_as_its_engine_does(self):
        patch = (ROOT / 'code/qcommon/cm_patch.c').read_text(encoding='utf-8')
        position = ''.join('\n' + function(patch, signature) + '\n' for signature in [
            'static qboolean CM_PatchBoundsMeet(',
            'static qboolean CM_PlaneSidePositionTest(',
            'qboolean CM_PositionTestInPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc ) {',
        ])
        # not "patch": Windows asks to elevate an executable named like one
        build_and_run(self, 'rtcwcurves', [
            GENERATOR_PRELUDE + generator_source() + POSITION_PRELUDE + position + CHECKS,
            ROOT / 'code/qcommon/cm_polylib.c',
            ROOT / 'code/qcommon/q_math.c',
        ])

    def test_mp_traces_points_against_curves_whatever_the_cvar(self):
        patch = (ROOT / 'code/qcommon/cm_patch.c').read_text(encoding='utf-8')
        guard = function(patch, 'static void CM_TracePointThroughPatchCollide(')
        self.assertIn('if ( CM_RTCWMPCurves() ? !cm_playerCurveClip->integer && !tw->isPoint\n'
                      '\t\t\t: !cm_playerCurveClip->integer || !tw->isPoint ) {', guard)


if __name__ == '__main__':
    unittest.main()
