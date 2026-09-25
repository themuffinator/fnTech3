"""Medal of Honor: the client game's swipes (imports 103..105), drawn as a
strip of scene polygons that fades out over the swipe's life.

The swipe code is production's, compiled from code/client/cl_mohaa_cgame.cpp
against a stub scene that records the polygons it is handed."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def swipe_code(source):
    start = source.index('constexpr std::size_t kMaxSwipePoints')
    end_signature = 'void QDECL SwipeEnd() {'
    end = source.index('\n}\n', source.index(end_signature)) + 3
    return source[start:end]


class MohaaSwipeTests(unittest.TestCase):
    def test_a_swipe_is_a_fading_strip_of_polygons(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_mohaa_cgame.cpp').read_text()
        harness = r'''
#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>
typedef float vec_t;
typedef vec_t vec3_t[3];
typedef int qhandle_t;
typedef unsigned char byte;
#define QDECL
#define VectorCopy( a, b ) ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )
typedef union { byte rgba[4]; unsigned int u32; } color4ub_t;
struct polyVert_t { vec3_t xyz; float st[2]; color4ub_t modulate; };
struct Poly { qhandle_t shader; std::vector<polyVert_t> verts; };
std::vector<Poly> polys;
struct RefExport { void ( *AddPolyToScene )( qhandle_t, int, const polyVert_t *, int ); } re;
void AddPoly( qhandle_t shader, int count, const polyVert_t *verts, int numPolys ) {
	assert( numPolys == 1 );
	polys.push_back( { shader, std::vector<polyVert_t>( verts, verts + count ) } );
}
''' + swipe_code(source) + r'''
static bool Near( byte value, float fraction ) {
	return std::fabs( static_cast<float>( value ) - fraction * 255.0f ) <= 1.0f;
}
static void Point( float x, float time ) {
	vec3_t p1 = { x, 0.0f, 10.0f }, p2 = { x, 0.0f, 0.0f };
	SwipePoint( p1, p2, time );
}
int main() {
	re.AddPolyToScene = AddPoly;
	/* Three pairs taken 400, 200 and 0 ms before the swipe's time, with a
	 * 500 ms life: two quads, each end at its pair's fade. */
	SwipeBegin( 1000.0f, 500.0f, 7 );
	Point( 0.0f, 600.0f );
	Point( 1.0f, 800.0f );
	Point( 2.0f, 1000.0f );
	SwipeEnd();
	assert( polys.size() == 2 );
	const Poly &first = polys[0];
	assert( first.shader == 7 && first.verts.size() == 4 );
	assert( Near( first.verts[0].modulate.rgba[3], 0.2f ) );
	assert( first.verts[0].modulate.rgba[0] == first.verts[0].modulate.rgba[3] );
	assert( Near( first.verts[2].modulate.rgba[3], 0.6f ) );
	/* p1 is the strip's top edge, p2 its bottom; the texture runs along it. */
	assert( first.verts[0].xyz[2] == 10.0f && first.verts[0].st[1] == 0.0f );
	assert( first.verts[1].xyz[2] == 0.0f && first.verts[1].st[1] == 1.0f );
	assert( first.verts[0].st[0] == 0.0f && first.verts[2].st[0] == 0.5f );
	assert( polys[1].verts[2].st[0] == 1.0f && polys[1].verts[2].modulate.rgba[3] == 255 );
	/* A segment faded out at both ends is not drawn. */
	polys.clear();
	SwipeBegin( 1000.0f, 500.0f, 7 );
	Point( 0.0f, 100.0f );
	Point( 1.0f, 400.0f );
	Point( 2.0f, 900.0f );
	SwipeEnd();
	assert( polys.size() == 1 && polys[0].verts[0].modulate.rgba[3] == 0 );
	/* The client game's clear -- a negative shader, no life -- and a single
	 * pair draw nothing, nor does a close without an open. */
	polys.clear();
	SwipeBegin( 0.0f, 0.0f, -1 );
	Point( 0.0f, 0.0f );
	Point( 1.0f, 0.0f );
	SwipeEnd();
	SwipeBegin( 1000.0f, 500.0f, 7 );
	Point( 0.0f, 1000.0f );
	SwipeEnd();
	SwipeEnd();
	SwipePoint( nullptr, nullptr, 0.0f );
	assert( polys.empty() );
	/* No more pairs than the limit are kept. */
	SwipeBegin( 1000.0f, 500.0f, 7 );
	for ( int i = 0; i < 1100; i++ ) Point( static_cast<float>( i ), 1000.0f );
	SwipeEnd();
	assert( polys.size() == kMaxSwipePoints - 1 );
	return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'swipe.cpp'
            exe = Path(directory) / 'swipe.exe'
            cpp.write_text(harness)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_client_game_is_served_them_and_a_level_clears_them(self):
        source = (ROOT / 'code/client/cl_mohaa_cgame.cpp').read_text()
        for slot in ('SERVE( R_SwipeBegin, SwipeBegin );', 'SERVE( R_SwipePoint, SwipePoint );',
                     'SERVE( R_SwipeEnd, SwipeEnd );'):
            self.assertIn(slot, source)
        clear = source[source.index('void CL_MOHAACGameClearModels( void ) {'):]
        clear = clear[:clear.index('\n}\n')]
        self.assertIn('swipe = SwipeState();', clear)


if __name__ == '__main__':
    unittest.main()
