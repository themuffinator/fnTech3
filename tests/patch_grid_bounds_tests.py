"""A damaged patch cannot grow the collision grid past MAX_GRID_SIZE.

CM_SubdivideGridColumns adds two columns wherever a curve's midpoint lies
`subdivisions` or more from its chord, and the id Tech 3 sources it descends
from never checked that the grid had room for them. A patch whose control
points lie far off its curve -- a seeded corruption of q3dm1's vertex lump on
2026-09-26 -- grew the grid past its 129 columns on the stack, and the client
crashed while loading the map. The generator now refuses such a patch with the
loader's ERR_DROP before writing past the grid, which is the only case the
check can change: any patch it refuses used to overflow.

The fixture compiles the engine's own patch generator, as
alice_patch_collision_tests.py slices it, and feeds it synthetic patches; no
retail map is read. See docs/fntech3/2026-09-26-bsp-faults.md.
"""
from pathlib import Path
import unittest

from alice_patch_collision_tests import GENERATOR_PRELUDE, build_and_run, generator_source

ROOT = Path(__file__).resolve().parents[1]

ABORTING_ERROR = ('void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) '
                  '{ (void)level; (void)fmt; abort(); }')

RECORDING_ERROR = r'''
#include <setjmp.h>
#include <stdio.h>
static jmp_buf errors;
static char lastError[256];
static int lastLevel;
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
	lastLevel = level;
	snprintf( lastError, sizeof( lastError ), "%s", fmt );
	longjmp( errors, 1 );
}
'''

CHECKS = r'''
/* A 3x3 patch bent along one axis: its middle column (or row) stands `height`
 * units above the other two. */
static void Arch( vec3_t *points, int alongRows, float height ) {
	int i, j;
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			const int bent = alongRows ? j : i;
			VectorSet( points[j * 3 + i], i * 32.0f, j * 32.0f, bent == 1 ? height : 0.0f );
		}
	}
}

/* 1 when the generator refused the patch, 0 when it built one. */
static int Refused( int alongRows, float height ) {
	vec3_t points[9];
	Arch( points, alongRows, height );
	lastError[0] = 0;
	if ( setjmp( errors ) ) {
		assert( lastLevel == ERR_DROP );
		assert( strstr( lastError, "MAX_GRID_SIZE" ) );
		return 1;
	}
	assert( CM_GeneratePatchCollide( 3, 3, points, SUBDIVIDE_DISTANCE )->numFacets > 0 );
	return 0;
}

int main( void ) {
	/* An arch thousands of units high is a legitimate, finely cut patch. */
	assert( !Refused( 0, 4096.0f ) && !Refused( 1, 4096.0f ) );
	/* Control points thrown far off the curve, as damaged vertices are, need
	 * more columns than the grid holds, along either axis. */
	assert( Refused( 0, 1.0e30f ) && Refused( 1, 1.0e30f ) );
	assert( Refused( 0, -3.0e12f ) && Refused( 1, -3.0e12f ) );
	/* A refusal leaves nothing behind: the next patch builds as before. */
	assert( !Refused( 0, 64.0f ) && !Refused( 1, 64.0f ) );
	return 0;
}
'''


class PatchGridBoundsTests(unittest.TestCase):
    def test_damaged_patches_are_refused_before_the_grid_overflows(self):
        self.assertEqual(GENERATOR_PRELUDE.count(ABORTING_ERROR), 1)
        prelude = GENERATOR_PRELUDE.replace(ABORTING_ERROR, RECORDING_ERROR)
        # not "patch": Windows asks to elevate an executable named like one
        build_and_run(self, 'gridbounds', [
            prelude + generator_source() + CHECKS,
            ROOT / 'code/qcommon/cm_polylib.c',
            ROOT / 'code/qcommon/q_math.c',
        ])

    def test_the_column_subdivider_checks_room_before_it_writes(self):
        source = (ROOT / 'code/qcommon/cm_patch.c').read_text(encoding='utf-8')
        start = source.index('static void CM_SubdivideGridColumns(')
        body = source[start:source.index('\n}\n', start)]
        check = body.index('if ( grid->width + 2 > MAX_GRID_SIZE ) {')
        self.assertLess(check, body.index('VectorCopy( grid->points[k][j], grid->points[k+2][j] );'))
        self.assertIn('Com_Error( ERR_DROP, "CM_SubdivideGridColumns: subdivided grid is > MAX_GRID_SIZE" );',
                      body[check:])


if __name__ == '__main__':
    unittest.main()
