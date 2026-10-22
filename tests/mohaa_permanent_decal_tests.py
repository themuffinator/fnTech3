"""Medal of Honor's permanent decals and skeletal level of detail.

A level's `maps/<name>.dcl` (code/qcommon/dcl_mohaa.h) is built into its
world by code/renderercommon/tr_mohaa_world.c, one fan of triangles a
fragment, listed in the open leafs a unit in front of its points and their
centre; the file's own leaf lists are not read, as eight of the eleven 1.00
files whose map ships were saved against an older compile. The decal editor
(code/client/cl_mohaa_decals.cpp) takes the file instead while
`dcl_editmode` is set. The skeletal models lose vertices with their size on
screen (code/mohaa/mohaa_render_api.cpp), by the level-of-detail records
1.00 ships beside its skeletons.

The fixtures compile the builder itself against a two-leaf map, and the
level-of-detail arithmetic sliced from the engine. No retail file is read:
the one level-of-detail record below is 1.00's barclip.lod, pinned as
numbers."""
import os
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def c_bytes(data):
    return ', '.join(str(b) for b in data)


def dcl(fragments, version=1, map_time=b'', decals=1, lists=b''):
    """A decal file: `fragments` are (shader, index, points), each point
    (x, y, z, s, t, (r, g, b, a))."""
    out = b'DCL ' + struct.pack('<4i', version, 0, decals, len(fragments))
    if version == 2:
        out += map_time.ljust(32, b'\0')[:32]
    for _ in range(decals):
        out += b'blastmark'.ljust(64, b'\0') + struct.pack('<14fi', *([0.0] * 14), 1)
    for shader, index, points in fragments:
        out += shader.ljust(64, b'\0') + struct.pack('<3i', 0, index, len(points))
        for x, y, z, s, t, rgba in points:
            out += struct.pack('<5f', x, y, z, s, t) + bytes(rgba)
    return out + lists


# a quad on the plane x = 0 facing +x (clockwise seen from +x), a triangle
# behind the plane facing -x, and a terrain triangle facing +x
FRONT_QUAD = [(0, 0, 0, 0.0, 0.0, (10, 20, 30, 40)), (0, 0, 8, 0.0, 1.0, (50, 60, 70, 80)),
              (0, 8, 8, 1.0, 1.0, (90, 100, 110, 120)), (0, 8, 0, 1.0, 0.0, (130, 140, 150, 160))]
BACK_TRIANGLE = [(-4, 0, 0, 0, 0, (1, 1, 1, 1)), (-4, 8, 0, 0, 0, (1, 1, 1, 1)), (-4, 0, 8, 0, 0, (1, 1, 1, 1))]
TERRAIN_TRIANGLE = [(0, 20, 0, 0, 0, (7, 7, 7, 255)), (0, 20, 8, 0, 1, (7, 7, 7, 255)),
                    (0, 28, 8, 1, 1, (7, 7, 7, 255))]
MAP_TIME = b'Thu Nov 29 12:41:01 2001'

BUILD_FIXTURE = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_public.h"
#include "code/renderercommon/tr_mohaa_world.h"
#include "code/qcommon/dcl_mohaa.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

refimport_t ri;
static const byte *dclData;
static int dclLength, editMode, warnings, reads;

static void QDECL Printf( printParm_t level, const char *fmt, ... ) {
	if ( level == PRINT_WARNING ) {
		warnings++;
	}
	(void)fmt;
}
static void *Malloc( int bytes ) { return calloc( 1, bytes > 0 ? bytes : 1 ); }
static void Free( void *p ) { free( p ); }
static int ReadFile( const char *name, void **buffer ) {
	reads++;
	*buffer = NULL;
	if ( !dclData || strcmp( name, "maps/fixture.dcl" ) ) {
		return -1;
	}
	*buffer = malloc( dclLength );
	memcpy( *buffer, dclData, dclLength );
	return dclLength;
}
static void FreeFile( void *buffer ) { free( buffer ); }
static int CvarInteger( const char *name ) { return !strcmp( name, "dcl_editmode" ) ? editMode : 0; }
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; exit( 3 ); }
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }

/* A two-leaf map: the plane x = 0, open in front (leaf 0), solid behind. */
static byte map[4096];
static int mapLength;
static void Lump( dheader_t *header, int lump, const void *data, int length ) {
	header->lumps[lump].fileofs = mapLength;
	header->lumps[lump].filelen = length;
	memcpy( map + mapLength, data, length );
	mapLength += ( length + 3 ) & ~3;
}
static void BuildMap( void ) {
	static const char entities[] = "{\n\"classname\" \"worldspawn\"\n\"map_time\" \"Thu Nov 29 12:41:01 2001\"\n}\n";
	dheader_t *header = (dheader_t *)map;
	dshader_t shader;
	dplane_t plane;
	dnode_t node;
	dleaf_t leafs[2];
	dmodel_t model;
	memset( map, 0, sizeof( map ) );
	memset( &shader, 0, sizeof( shader ) );
	memset( &plane, 0, sizeof( plane ) );
	memset( &node, 0, sizeof( node ) );
	memset( leafs, 0, sizeof( leafs ) );
	memset( &model, 0, sizeof( model ) );
	header->ident = BSP_IDENT;
	header->version = BSP_VERSION;
	mapLength = sizeof( dheader_t );
	Q_strncpyz( shader.shader, "textures/fixture/wall", sizeof( shader.shader ) );
	plane.normal[0] = 1.0f;
	node.planeNum = 0;
	node.children[0] = -1;
	node.children[1] = -2;
	leafs[0].cluster = 0;
	leafs[1].cluster = -1;
	Lump( header, LUMP_ENTITIES, entities, (int)strlen( entities ) );
	Lump( header, LUMP_SHADERS, &shader, sizeof( shader ) );
	Lump( header, LUMP_PLANES, &plane, sizeof( plane ) );
	Lump( header, LUMP_NODES, &node, sizeof( node ) );
	Lump( header, LUMP_LEAFS, leafs, sizeof( leafs ) );
	Lump( header, LUMP_MODELS, &model, sizeof( model ) );
}

static const void *Out( const byte *data, int lump, int size, int *count ) {
	const dheader_t *header = (const dheader_t *)data;
	*count = header->lumps[lump].filelen / size;
	return data + header->lumps[lump].fileofs;
}

static qboolean Build( const byte *file, int length, int *first, int *count, byte **out ) {
	mohaa_translation_t translation;
	mohaaBuiltSurfaces_t surfaces;
	void *data = NULL;
	int size = 0;
	qboolean built;
	memset( &translation, 0, sizeof( translation ) );
	memset( &surfaces, 0xff, sizeof( surfaces ) );
	translation.data = map;
	translation.length = mapLength;
	dclData = file;
	dclLength = length;
	built = R_MOHAABuildWorld( &translation, "maps/fixture.bsp", &data, &size, &surfaces );
	/* no terrain here: its range is empty, built or not */
	assert( surfaces.numTerrain == 0 );
	assert( surfaces.firstBuiltSurface == 0 && surfaces.numBuiltSurfaces == surfaces.numDecals );
	*first = surfaces.firstDecal;
	*count = surfaces.numDecals;
	*out = data;
	return built;
}
'''

BUILD_CHECKS = r'''
int main( void ) {
	const byte file[] = { %(file)s };
	const byte v2[] = { %(v2)s };
	const byte stale[] = { %(stale)s };
	const byte broken[] = { %(broken)s };
	int first = -1, count = -1, n, i;
	byte *out = NULL;
	dclLayoutMOHAA_t layout;

	ri.Printf = Printf;
	ri.Malloc = Malloc;
	ri.Free = Free;
	ri.FS_ReadFile = ReadFile;
	ri.FS_FreeFile = FreeFile;
	ri.Cvar_VariableIntegerValue = CvarInteger;
	BuildMap();

	/* the file walks: version 1 has no map time and the lists follow the
	 * fragments */
	assert( MOHAA_DCLLayout( file, sizeof( file ), &layout ) );
	assert( layout.version == 1 && layout.mapTime == -1 && layout.numDecals == 1 && layout.numFragments == 5 );
	assert( layout.decals == 20 && layout.fragments == 20 + 124 );
	assert( MOHAA_DCLLayout( v2, sizeof( v2 ), &layout ) && layout.mapTime == 20 && layout.decals == 52 );
	assert( !MOHAA_DCLLayout( broken, sizeof( broken ), &layout ) );
	assert( !MOHAA_DCLLayout( file, 19, &layout ) );
	assert( !MOHAA_DCLLayout( file, 20 + 123, &layout ) );
	{
		byte copy[sizeof( file )];
		memcpy( copy, file, sizeof( copy ) );
		copy[0] = 'X';
		assert( !MOHAA_DCLLayout( copy, sizeof( copy ), &layout ) );
		memcpy( copy, file, sizeof( copy ) );
		copy[4] = 3;
		assert( !MOHAA_DCLLayout( copy, sizeof( copy ), &layout ) );
		memcpy( copy, file, sizeof( copy ) );
		copy[15] = 0x80;	/* a negative decal count */
		assert( !MOHAA_DCLLayout( copy, sizeof( copy ), &layout ) );
	}

	/* built: the front quad and the terrain triangle; the brush model's,
	 * the two-point one and the one behind the wall are left out */
	assert( Build( file, sizeof( file ), &first, &count, &out ) );
	assert( first == 0 && count == 2 && warnings == 0 );
	{
		const dsurface_t *surfaces = Out( out, LUMP_SURFACES, sizeof( dsurface_t ), &n );
		const dshader_t *shaders;
		const drawVert_t *verts;
		const int *indexes, *leafSurfaces;
		const dleaf_t *leafs;
		assert( n == 2 );
		assert( surfaces[0].surfaceType == MST_TRIANGLE_SOUP && surfaces[0].numVerts == 4 );
		assert( surfaces[0].numIndexes == 6 && surfaces[0].lightmapNum == -3 && surfaces[0].fogNum == -1 );
		assert( surfaces[1].numVerts == 3 && surfaces[1].numIndexes == 3 );
		/* one shader appended for both, its name matched without case */
		shaders = Out( out, LUMP_SHADERS, sizeof( dshader_t ), &n );
		assert( n == 2 && !strcmp( shaders[1].shader, "blastmark" ) );
		assert( surfaces[0].shaderNum == 1 && surfaces[1].shaderNum == 1 );
		/* the points as the file has them, colours unshifted, facing +x */
		verts = Out( out, LUMP_DRAWVERTS, sizeof( drawVert_t ), &n );
		assert( n == 7 );
		assert( verts[2].xyz[1] == 8.0f && verts[2].xyz[2] == 8.0f && verts[2].st[0] == 1.0f );
		for ( i = 0; i < 4; i++ ) {
			assert( verts[i].color.rgba[0] == 10 + 40 * i && verts[i].color.rgba[3] == 40 + 40 * i );
			assert( fabs( verts[i].normal[0] - 1.0f ) < 1e-6f && verts[i].normal[1] == 0.0f );
		}
		/* a fan, relative to the surface's first point */
		indexes = Out( out, LUMP_DRAWINDEXES, sizeof( int ), &n );
		assert( n == 9 );
		assert( indexes[0] == 0 && indexes[1] == 1 && indexes[2] == 2 );
		assert( indexes[3] == 0 && indexes[4] == 2 && indexes[5] == 3 );
		/* listed in the open leaf only */
		leafs = Out( out, LUMP_LEAFS, sizeof( dleaf_t ), &n );
		leafSurfaces = Out( out, LUMP_LEAFSURFACES, sizeof( int ), &n );
		assert( n == 2 );
		assert( leafs[0].numLeafSurfaces == 2 && leafSurfaces[leafs[0].firstLeafSurface] == 0 );
		assert( leafSurfaces[leafs[0].firstLeafSurface + 1] == 1 );
		assert( leafs[1].numLeafSurfaces == 0 );
	}
	free( out );

	/* the editor takes the file instead: nothing is read or built */
	editMode = 1;
	reads = 0;
	assert( !Build( file, sizeof( file ), &first, &count, &out ) && count == 0 && reads == 0 && !out );
	editMode = 0;

	/* version 2 is built only against its own compile of the map */
	assert( Build( v2, sizeof( v2 ), &first, &count, &out ) && count == 1 && warnings == 0 );
	free( out );
	assert( !Build( stale, sizeof( stale ), &first, &count, &out ) && count == 0 && warnings == 1 );

	/* a malformed file is refused whole */
	assert( !Build( broken, sizeof( broken ), &first, &count, &out ) && count == 0 && warnings == 2 );

	/* no file, no decals */
	assert( !Build( NULL, 0, &first, &count, &out ) && count == 0 );
	puts( "decals ok" );
	return 0;
}
'''

LOD_PRELUDE = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
struct cvar_t { float value; int integer; };
static cvar_t uselod{ 1.0f, 1 }, lodscale{ 1.1f, 1 }, lodcap{ 1.0f, 1 }, lodviewmodelcap{ 0.25f, 0 };
static cvar_t *r_uselod = &uselod, *r_lodscale = &lodscale, *r_lodcap = &lodcap,
	*r_lodviewmodelcap = &lodviewmodelcap;
#define MAX_LOD_CURVE_POINTS 5
#define MAX_LOD_CURVE_CONSTS 4
typedef short skelIndex_t;
struct skelSurfaceGame_t {
	int numTriangles, numVerts;
	skelIndex_t *pTriangles, *pCollapse, *pCollapseIndex;
};
'''

LOD_CHECKS = r'''
static bool Near( float a, float b, float tolerance ) { return std::fabs( a - b ) <= tolerance; }

int main() {
	/* 1.00's barclip.lod: its metrics and curve, and the constants the
	 * retail tool saved beside them, which the engine's own arithmetic
	 * makes again */
	lodControl_t lod{};
	lod.minMetric = 0.8166670203208923f;
	lod.maxMetric = 0.009999999776482582f;
	const float curve[5][2] = { { 0.0f, 0.0f }, { 0.5f, 15.184596061706543f }, { 0.800000011920929f, 29.238468170166016f },
		{ 0.949999988079071f, 36.9922981262207f }, { 1.0f, 42.0f } };
	for ( int i = 0; i < 5; i++ ) {
		lod.curve[i].pos = curve[i][0];
		lod.curve[i].val = curve[i][1];
	}
	TIKI_CalcLodConsts( &lod );
	const float saved[4][3] = { { 30.745670318603516f, -37.647743225097656f, 0.8166670203208923f },
		{ 39.188453674316406f, -58.073822021484375f, 0.4133335053920746f },
		{ 40.21772003173828f, -64.08122253417969f, 0.17133338749408722f },
		{ 43.2415771484375f, -124.1578140258789f, 0.05033336207270622f } };
	for ( int i = 0; i < 4; i++ ) {
		assert( Near( lod.consts[i].base, saved[i][0], 1e-3f ) );
		assert( Near( lod.consts[i].scale, saved[i][1], 1e-3f ) );
		assert( Near( lod.consts[i].cutoff, saved[i][2], 1e-5f ) );
	}

	/* near: the curve's first cutoff; far: its last */
	assert( LodCutoff( &lod, 5.0f, false ) == 0 );
	assert( LodCutoff( &lod, 0.001f, false ) == 42 );
	/* between, read off its segment: r_lodscale 1.1 stretches the metric */
	for ( const float metric : { 0.6f, 0.3f, 0.12f, 0.03f } ) {
		const float cap = ( metric - lod.maxMetric ) * 1.1f + lod.maxMetric;
		int segment = 0;
		for ( int i = 3; i > 0; i-- ) {
			if ( cap <= lod.consts[i].cutoff ) {
				segment = i;
				break;
			}
		}
		const int expected = static_cast<int>( cap * lod.consts[segment].scale + lod.consts[segment].base );
		assert( LodCutoff( &lod, metric, false ) == expected );
	}
	/* r_lodcap holds the detail back from the near end */
	lodcap.value = 0.5f;
	assert( LodCutoff( &lod, 5.0f, false ) > 0 );
	lodcap.value = 1.0f;
	/* a view weapon sits at r_lodviewmodelcap whatever its metric */
	assert( LodCutoff( &lod, 5.0f, true ) == LodCutoff( &lod, 0.001f, true ) );
	assert( LodCutoff( &lod, 5.0f, true ) > 0 );
	/* r_uselod 0: every model at the first cutoff */
	uselod.integer = 0;
	assert( LodCutoff( &lod, 0.001f, false ) == 0 );
	uselod.integer = 1;
	/* a malformed record cannot ask for more than an int holds */
	lod.curve[4].val = 1e30f;
	assert( LodCutoff( &lod, 0.001f, false ) == 0 );

	/* the vertices kept: the leading ones reaching the cutoff, at least
	 * three, none when the third falls short */
	skelIndex_t collapseIndex[10] = { 10, 10, 9, 8, 7, 5, 5, 3, 2, 1 };
	skelSurfaceGame_t surface{};
	surface.numVerts = 10;
	surface.pCollapseIndex = collapseIndex;
	assert( LodVertexCount( &surface, 6 ) == 5 );
	assert( LodVertexCount( &surface, 9 ) == 3 );
	assert( LodVertexCount( &surface, 0 ) == 10 );
	assert( LodVertexCount( &surface, 1 ) == 10 );
	assert( LodVertexCount( &surface, 11 ) == 0 );

	/* a strip of four triangles over six points; with four kept, point 4
	 * collapses onto 2 and point 5 onto 4, so the third triangle's corners
	 * meet and the surface ends there */
	skelIndex_t triangles[12] = { 0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4 };
	skelIndex_t collapse[6] = { 0, 0, 1, 2, 2, 4 };
	skelIndex_t order[6] = { 9, 9, 9, 9, 1, 1 };
	skelSurfaceGame_t strip{};
	strip.numVerts = 6;
	strip.numTriangles = 4;
	strip.pTriangles = triangles;
	strip.pCollapse = collapse;
	strip.pCollapseIndex = order;
	assert( LodVertexCount( &strip, 5 ) == 4 );
	short stand[6];
	LodCollapse( &strip, 4, stand );
	assert( stand[3] == 3 && stand[4] == 2 && stand[5] == 2 );
	unsigned int indexes[12];
	assert( LodTriangles( &strip, stand, indexes, 100 ) == 6 );
	assert( indexes[0] == 100 && indexes[3] == 101 && indexes[5] == 102 );
	/* a collapse onto a later point, which no skeleton holds, lands on the first */
	collapse[5] = 5;
	LodCollapse( &strip, 4, stand );
	assert( stand[5] == 0 );
	puts( "lod ok" );
	return 0;
}
'''


MERGE_PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <utility>
#include <vector>
vec_t VectorNormalize( vec3_t v ) {
	const float length = std::sqrt( DotProduct( v, v ) );
	if ( length > 0.0f ) {
		VectorScale( v, 1.0f / length, v );
	}
	return length;
}
namespace mohaa {
struct PolyVert { float xyz[3]; float st[2]; unsigned char modulate[4]; };
}
constexpr int kFragmentPoints = 8;
struct Fragment {
	int index = 0;
	std::vector<mohaa::PolyVert> points;
	std::vector<int> leafs;
};
'''

MERGE_CHECKS = r'''
/* a fragment on the plane x = 0 from (y, z) pairs, wound clockwise as the
 * client game winds them */
static Fragment Make( int index, std::initializer_list<std::pair<float, float>> points, float x = 0.0f ) {
	Fragment fragment;
	fragment.index = index;
	for ( const auto &p : points ) {
		mohaa::PolyVert v{};
		v.xyz[0] = x;
		v.xyz[1] = p.first;
		v.xyz[2] = p.second;
		fragment.points.push_back( v );
	}
	return fragment;
}

int main() {
	/* two squares sharing an edge: one rectangle, its middle points dropped */
	std::vector<Fragment> pair = { Make( 0, { { 0, 0 }, { 0, 8 }, { 8, 8 }, { 8, 0 } } ),
		Make( 0, { { 8, 0 }, { 8, 8 }, { 16, 8 }, { 16, 0 } } ) };
	MergeCoplanar( pair );
	assert( pair.size() == 1 && pair[0].points.size() == 4 );
	float low = 1e9f, high = -1e9f;
	for ( const mohaa::PolyVert &v : pair[0].points ) {
		low = std::min( low, v.xyz[1] );
		high = std::max( high, v.xyz[1] );
	}
	assert( low == 0.0f && high == 16.0f );
	vec3_t normal;
	assert( FragmentNormal( pair[0], normal ) && normal[0] > 0.999f );

	/* off the plane, or on a terrain patch: apart */
	std::vector<Fragment> apart = { Make( 0, { { 0, 0 }, { 0, 8 }, { 8, 8 }, { 8, 0 } } ),
		Make( 0, { { 8, 0 }, { 8, 8 }, { 16, 8 }, { 16, 0 } }, 1.0f ) };
	MergeCoplanar( apart );
	assert( apart.size() == 2 );
	std::vector<Fragment> terrain = { Make( 3, { { 0, 0 }, { 0, 8 }, { 8, 8 }, { 8, 0 } } ),
		Make( 3, { { 8, 0 }, { 8, 8 }, { 16, 8 }, { 16, 0 } } ) };
	MergeCoplanar( terrain );
	assert( terrain.size() == 2 );

	/* a dart split along its inner diagonal would join concave: apart */
	std::vector<Fragment> dart = { Make( 0, { { 0, 0 }, { 5, 10 }, { 5, 3 } } ),
		Make( 0, { { 5, 10 }, { 10, 0 }, { 5, 3 } } ) };
	MergeCoplanar( dart );
	assert( dart.size() == 2 );

	/* a twelve-sided polygon split in two sevens would join past eight points */
	std::vector<std::pair<float, float>> ring;
	for ( int i = 0; i < 12; i++ ) {
		const float angle = -i * static_cast<float>( M_PI ) / 6.0f;
		ring.push_back( { 10.0f * std::cos( angle ), 10.0f * std::sin( angle ) } );
	}
	Fragment first, second;
	for ( int i = 0; i <= 6; i++ ) {
		first.points.push_back( Make( 0, { ring[i] } ).points[0] );
	}
	for ( int i = 6; i <= 12; i++ ) {
		second.points.push_back( Make( 0, { ring[i % 12] } ).points[0] );
	}
	std::vector<Fragment> big = { first, second };
	vec3_t facing;
	assert( FragmentNormal( big[0], facing ) && facing[0] > 0.999f );
	MergeCoplanar( big );
	assert( big.size() == 2 );
	puts( "merge ok" );
	return 0;
}
'''


def build_and_run(test, name, sources, cpp=False):
    compiler = shutil.which('clang++' if cpp else 'clang')
    if not compiler:
        test.skipTest('Clang unavailable')
    target = compiler_target_args(compiler)
    abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as directory:
        paths = []
        for index, text in enumerate(sources):
            if isinstance(text, Path):
                paths.append(str(text))
                continue
            path = Path(directory) / f'{name}{index}.{"cpp" if cpp else "c"}'
            path.write_text(text, encoding='utf-8')
            paths.append(str(path))
        exe = Path(directory) / f'{name}.exe'
        command = [compiler, *target, *abi, '-std=c++17' if cpp else '-std=c11', '-I', str(ROOT),
                   *paths, '-o', str(exe)]
        if os.name != 'nt':
            command.append('-lm')
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        run = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=30, **options)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        return run.stdout


class MohaaPermanentDecalTests(unittest.TestCase):
    def test_the_world_builds_a_decal_files_fragments_into_its_open_leafs(self):
        fragments = [
            (b'blastmark', 0, FRONT_QUAD),
            (b'blastmark', -1, FRONT_QUAD[:3]),
            (b'blastmark', 0, FRONT_QUAD[:2]),
            (b'blastmark', 0, BACK_TRIANGLE),
            (b'BlastMark', 5, TERRAIN_TRIANGLE),
        ]
        # lists that name leafs the map lacks, which are not read
        lists = struct.pack('<6i', 0, 3, 99, 98, 97, 1)
        file = dcl(fragments, lists=lists)
        v2 = dcl([(b'blastmark', 0, FRONT_QUAD)], version=2, map_time=MAP_TIME)
        stale = dcl([(b'blastmark', 0, FRONT_QUAD)], version=2, map_time=b'Mon Nov 19 14:26:53 2001')
        broken = bytearray(dcl([(b'blastmark', 0, FRONT_QUAD)]))
        struct.pack_into('<i', broken, 20 + 124 + 72, 1000)
        checks = BUILD_CHECKS % {'file': c_bytes(file), 'v2': c_bytes(v2), 'stale': c_bytes(stale),
                                 'broken': c_bytes(broken)}
        output = build_and_run(self, 'decals', [
            BUILD_FIXTURE + checks,
            ROOT / 'code/renderercommon/tr_mohaa_world.c',
            ROOT / 'code/qcommon/q_shared.c',
            ROOT / 'code/qcommon/q_math.c',
        ])
        self.assertIn('decals ok', output)

    def test_level_of_detail_follows_the_retail_records_arithmetic(self):
        api = read('code/mohaa/mohaa_render_api.cpp')
        shared = read('code/mohaa/tiki/tiki_shared.h')
        skel = read('code/mohaa/tiki/tiki_skel.cpp')
        records = shared[shared.index('typedef struct {\n    float pos;'):shared.index('} lodControl_t;') + 15]
        source = LOD_PRELUDE + records + '\n'
        source += function(skel, 'void TIKI_CalcLodConsts(lodControl_t *LOD)')
        for signature in ('int Truncate( float value ) {', 'int LodCutoff(', 'int LodVertexCount(',
                          'void LodCollapse(', 'int LodTriangles('):
            source += '\n' + function(api, signature)
        output = build_and_run(self, 'lod', [source + LOD_CHECKS], cpp=True)
        self.assertIn('lod ok', output)

    def test_the_editor_joins_coplanar_fragments_as_2015s_does(self):
        editor = read('code/client/cl_mohaa_decals.cpp')
        source = MERGE_PRELUDE
        for signature in ('bool FragmentNormal(', 'bool SamePoint(', 'void DropCollinear(', 'bool Convex(',
                          'void MergeCoplanar('):
            source += '\n' + function(editor, signature)
        output = build_and_run(self, 'merge', [source + MERGE_CHECKS], cpp=True)
        self.assertIn('merge ok', output)
        # the merge runs on a decal's kept fragments before their leafs
        apply = function(editor, 'Decal *Apply(')
        self.assertLess(apply.index('MergeCoplanar( kept );'), apply.index('fragment.leafs = FragmentLeafs( fragment );'))

    def test_every_renderer_keeps_decal_colours_and_honours_r_drawstaticdecals(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                local = read(f'code/{renderer}/tr_local.h')
                self.assertIn('mohaaFirstDecal;', local)
                self.assertIn('static ID_INLINE qboolean R_MOHAAStaticDecal( const world_t *world, '
                              'const msurface_t *surf ) {', local)
                bsp = read(f'code/{renderer}/tr_bsp.c')
                load = function(bsp, 'void RE_LoadWorldMap( const char *name ) {')
                self.assertIn('R_MOHAABuildWorld( &mohaaTranslated, name, &world, &worldLength, &mohaaBuilt )',
                              load)
                # the range survives the world's reset
                self.assertLess(load.index('Com_Memset( &s_worldData, 0, sizeof( s_worldData ) );'),
                                load.index('s_worldData.mohaaNumDecals = mohaaBuilt.numDecals;'))
                tri = function(bsp, 'static void ParseTriSurf(')
                self.assertIn('if ( R_MOHAAStaticDecal( &s_worldData, surf ) ) {\n'
                              '\t\t\tCom_Memcpy( tri->verts[i].color.rgba, verts[i].color.rgba, 4 );', tri)
                world = read(f'code/{renderer}/tr_world.c')
                add = world[world.index('static void R_AddWorldSurface('):]
                add = add[:add.index('\n}\n')]
                self.assertIn('if ( tr.world->mohaaNumDecals && r_drawstaticdecals && !r_drawstaticdecals->integer\n'
                              '\t\t&& R_MOHAAStaticDecal( tr.world, surf ) ) {\n\t\treturn;', add)
                init = read(f'code/{renderer}/tr_init.c')
                mohaa = init[init.index('} else if ( ri.gameId == COM_GAME_MOHAA ) {'):]
                mohaa = mohaa[:mohaa.index('\n\t}')]
                self.assertIn('r_drawstaticdecals = ri.Cvar_Get( "r_drawstaticdecals", "1", CVAR_ARCHIVE );', mohaa)
                self.assertEqual(init.count('ri.Cvar_Get( "r_drawstaticdecals"'), 1)
                # the skeletal level of detail's scale is a player setting here
                self.assertIn('r_lodscale = ri.Cvar_Get( "r_lodscale", ri.gameId == COM_GAME_MOHAA ? "1.1" : "5",\n'
                              '\t\tCOM_GAME_ID_IS_RITUAL( ri.gameId ) ? 0 : ri.gameId == COM_GAME_MOHAA ? '
                              'CVAR_ARCHIVE : CVAR_CHEAT );', init)

    def test_every_renderer_measures_a_models_detail_in_the_main_view(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                model = read(f'code/{renderer}/tr_mohaa.c')
                add = function(model, 'void R_AddMOHAATikiSurfaces( trRefEntity_t *ent, dlightShadowCasterContext_t *casterContext ) {')
                self.assertLess(add.index('R_MOHAAPose( ent, mod )'), add.index('LodMetric('))
                self.assertIn('if ( !R_ViewPassIsPortal( &tr.viewParms ) || ( ent->e.mohaa.renderfx & '
                              'MOHAA_RF_SKYENTITY ) ) {', add)
                # the field the game gave, not the one r_fovCorrection widened
                self.assertIn('tr.viewParms.or.origin, tr.refdef.fovXAuthored > 0.0f ? tr.refdef.fovXAuthored '
                              ': tr.viewParms.fovX );', add)
                surface = function(model, 'void RB_SurfaceMOHAATiki( void *data ) {')
                self.assertIn('ent->e.mohaa.lod, ( ent->e.renderfx & RF_DEPTHHACK ) ? MOHAA_LOD_VIEWMODEL : 0,',
                              surface)
        world = read('code/renderercommon/tr_mohaa_world.c')
        # a static model is built once, whole
        self.assertIn('engine->Skin( models[i].tiki, j, pose, scale, -1.0f, 0,', world)
        # an entity starts unmeasured, and so whole
        self.assertIn('out.lod = -1.0f;', read('code/client/cl_mohaa_cgame.cpp'))
        self.assertIn('ent.mohaa.lod = -1.0f;', read('code/client/cl_mohaa_ui.cpp'))

    def test_the_engine_keeps_2015s_level_of_detail_cvars_and_tool(self):
        api = read('code/mohaa/mohaa_render_api.cpp')
        register = function(api, 'extern "C" void MOHAA_LodRegister( void ) {')
        for name, default, flags in [('r_uselod', '1', 'CVAR_TEMP'), ('r_lodscale', '1.1', 'CVAR_ARCHIVE'),
                                     ('r_lodcap', '1.0', 'CVAR_ARCHIVE'), ('r_lodviewmodelcap', '0.25', 'CVAR_ARCHIVE'),
                                     ('lod_tool', '0', 'CVAR_TEMP'), ('lod_tikiname', '', 'CVAR_TEMP'),
                                     ('lod_position', '0 0 0', 'CVAR_TEMP'), ('lod_minLOD', '1.0', 'CVAR_TEMP'),
                                     ('lod_maxLOD', '0.3', 'CVAR_TEMP'), ('lod_LOD_slider', '0.5', 'CVAR_TEMP'),
                                     ('lod_save', '0', 'CVAR_TEMP'), ('lod_mesh', '0', 'CVAR_TEMP')]:
            self.assertIn(f'Cvar_Get( "{name}", "{default}", {flags} )', register)
        for pattern in ('"lod_edit_%i"', '"lod_curve_%i_val"', '"lod_curve_%i_slider"'):
            self.assertIn(pattern, register)
        tool = function(api, 'extern "C" void MOHAA_LodToolFrame(')
        # a button moves the LOD slider to its curve point
        self.assertIn('SetCvarValue( t.slider, 1.0f - lod->curve[i].pos );', tool)
        # the sliders span the mesh's largest index, or the record's largest
        # cutoff: 540 of 1.00's 580 records pass OpenMoHAA's first positive one
        self.assertIn('t.range = std::max( t.range, static_cast<float>( skel->lodIndex[i] ) );', tool)
        self.assertIn('t.range = std::max( t.range, lod->curve[i].val );', tool)
        self.assertIn('lod->curve[i].val = t.curve[i]->value * t.range;', tool)
        # the record is saved beside its skeleton
        self.assertIn('LodSave( cache->path, lod );', tool)
        save = function(api, 'void LodSave( const char *skeleton, const lodControl_t *lod ) {')
        self.assertIn('Q_stricmp( extension, ".skd" )', save)
        self.assertIn('FS_FCloseFile( file );', save)
        # a character is measured by 92 units over its scale
        self.assertIn('constexpr float kCharacterLodRadius = 92.0f;', api)
        metric = function(api, 'float LodMetric(')
        self.assertIn('kCharacterLodRadius / scale', metric)
        self.assertIn('std::fabs( radius ) * ( 100.0f / field ) / distance', metric)
        # the client runs the tool and the editor on the world view
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        scene = function(cgame, 'void QDECL RenderScene( const mohaa::Refdef *fd ) {')
        self.assertLess(scene.index('MOHAA_LodToolFrame( view.vieworg, view.viewaxis, view.fov_x );'),
                        scene.index('re.RenderScene( &view );'))
        self.assertIn('if ( !( view.rdflags & RDF_NOWORLDMODEL ) ) {', scene)

    def test_a_malformed_level_of_detail_file_or_skeleton_cannot_overrun(self):
        skel = read('code/mohaa/tiki/tiki_skel.cpp')
        load = function(skel, 'void GetLODFile(skelcache_t *cache)')
        self.assertIn('if (length >= 0 && length < (int)offsetof(lodControl_t, consts)) {', load)
        self.assertIn('memcpy(LOD, buf, length < (int)sizeof(lodControl_t) ? length : sizeof(lodControl_t));', load)
        self.assertNotIn('strstr(pathLOD, "skd")', load)
        self.assertNotIn('lodIndex[i]', load.replace('TIKI_SkelLodIndex(cache->skel, i)', ''))
        sort = function(skel, 'void TIKI_SortLOD(skelHeaderGame_t *skelmodel)')
        self.assertIn('if (surf->pCollapse[i] < 0 || surf->pCollapse[i] >= surf->numVerts) {', sort)
        self.assertIn('if (surf->pTriangles[i] < 0 || surf->pTriangles[i] >= surf->numVerts) {', sort)

    def test_decal_light_is_on_the_byte_scale_its_callers_store(self):
        # every caller of client-game import 52 stores its light in a colour
        # byte, times its own 0..1 colour or as it is (cgamex86 0x300251f1,
        # 0x300272c4, 0x300274cf, 0x30027bec, 0x30027ea9), so a 0..1 light
        # drew every lit mark black; import 53's smoke light is 0..1
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        light = function(cgame, 'void LightAt( vec3_t light, const vec3_t origin ) {')
        self.assertIn('light[i] = ambient[i] + directed[i];', light)
        self.assertIn('VectorScale( light, 255.0f / peak, light );', light)
        self.assertIn('VectorSet( light, 255.0f, 255.0f, 255.0f );', light)
        self.assertNotIn('/ 255.0f', light)
        decal = function(cgame, 'void QDECL GetLightingForDecal(')
        self.assertNotIn('255', decal)
        # a point walled in reads unlit; the light is looked for out from the face
        self.assertIn('VectorMA( origin, step, facing, out );', decal)
        smoke = function(cgame, 'void QDECL GetLightingForSmoke(')
        self.assertIn('VectorScale( light, 1.0f / 255.0f, light );', smoke)

    def test_the_editor_serves_the_retail_decal_menu(self):
        editor = read('code/client/cl_mohaa_decals.cpp')
        # every command ui/decals.URC stuffs
        for command in ('dclc_save', 'dclc_shiftleft', 'dclc_shiftdown', 'dclc_shiftup', 'dclc_shiftright',
                        'dclc_selectprev', 'dclc_selectnext', 'dclc_getinfo', 'dclc_reapply', 'dclc_make',
                        'dclc_randomroll', 'dclc_delete'):
            self.assertRegex(editor, r'\{ "%s", \w+_f \}' % command)
        init = function(editor, 'void CL_MOHAADecalsInit( void ) {')
        for name, default, flags in [('dcl_editmode', '0', 'CVAR_LATCH'), ('dcl_showcurrent', '1', '0'),
                                     ('dcl_autogetinfo', '1', '0'), ('dcl_shiftstep', '4', '0'),
                                     ('dcl_shader', 'blastmark', '0'), ('dcl_radius', '16', '0'),
                                     ('dcl_heightscale', '1', '0'), ('dcl_widthscale', '1', '0'),
                                     ('dcl_rotation', '0', '0'), ('dcl_alpha', '1', '0'),
                                     ('dcl_dolighting', '1', '0'), ('dcl_doworld', '1', '0'),
                                     ('dcl_doterrain', '1', '0'), ('dcl_dobmodels', '1', '0'),
                                     ('dcl_dostring', 'apply to all', '0'), ('dcl_pathmode', '0', '0'),
                                     ('dcl_maxsegment', '512', '0'), ('dcl_minsegment', '24', '0'),
                                     ('dcl_maxoffset', '10', '0'), ('dcl_texturescale', '32', '0')]:
            self.assertIn(f'Cvar_Get( "{name}", "{default}", {flags} )', init)
        save = function(editor, 'void Save_f() {')
        # 1.00's own version, a zero checksum, then per brush model and leaf
        self.assertIn('out.Int( MOHAA_DCL_VERSION_1 );\n\tout.Int( 0 );', save)
        self.assertLess(save.index('CM_NumInlineModels()'), save.index('leafFragments )'))
        # the level's decals are taken before the world would build them
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        load = function(cgame, 'void QDECL LoadWorldMap( const char *name ) {')
        self.assertLess(load.index('CL_MOHAADecalsBeginLevel( name );'), load.index('re.LoadWorld( name );'))
        # the client game's decal exports take 14, 5 and 6 arguments
        self.assertRegex(cgame, r'typedef int \( QDECL \*PermanentMark_t \)\( float \*origin, float \*direction, '
                         r'float orientation, float sScale,\s+float tScale, float red, float green, float blue, '
                         r'float alpha, qboolean doLighting, float sCenter,\s+float tCenter, '
                         r'mohaa::MarkFragment \*fragments, mohaa::PolyVert \*points \);')
        abi = read('code/compat/mohaa_cgame_abi.h')
        self.assertIn('FNTECH3_MOHAA_MARK_FRAGMENTS = 128,', abi)
        self.assertIn('FNTECH3_MOHAA_MARK_POINTS = 384,', abi)
        self.assertIn('static_assert( sizeof( TreadMark ) == FNTECH3_MOHAA_TREAD_MARK_BYTES );', abi)
        self.assertIn('FNTECH3_MOHAA_TREAD_MARK_BYTES = 152', abi)


if __name__ == '__main__':
    unittest.main()
