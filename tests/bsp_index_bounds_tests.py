"""A map's loaders refuse an index that leaves its table before following it.

scripts/probe_bsp_faults.py damaged stock Quake III, Quake Live and RTCW maps
one lump at a time on 2026-09-26, and 24 of 409 variants read through an index
no loader checked: a node's plane or child, a leaf's area, a leaf surface, an
inline model's brush or surface count, a brush's sides and a face's vertices
(docs/fntech3/2026-09-26-bsp-faults.md). Windows' exception filter turned each
access violation into an error; elsewhere it would end the process. The
collision loader and every renderer's world loader now bound each index of
that kind before following it -- a node's plane and children, a leaf's area,
cluster and lists, a leaf surface, a model's brushes and surfaces, a brush's
sides, a side's plane, a surface's vertices and indexes -- with the checks in
code/qcommon/bsp_index_checks.h, and refuse the map with ERR_DROP when one
leaves its table. Hunk_Alloc refuses a negative size, which a wrapped count
used to turn into a memset off the hunk. Medal of Honor's world rebuild, which
reads the leafs' surface lists before the renderers' loaders run, copies only
the part of each list inside the map's and declines to rebuild when the lists
overlap, rather than writing past the list it copies them into.

No installed map of any profile is refused: every one passes these checks and
loads as before (docs/fntech3/2026-09-27-bsp-index-checks.md). Two cases keep
today's behaviour instead of refusing: a visibility table that does not cover
the leafs' clusters leaves the map unvised, as a table that does not fit its
payload already did, and a leaf brush naming no brush is still repaired.

The fixtures compile the engine's own loader functions and feed them a small
synthetic map; no retail map is read.
"""
from pathlib import Path
import re
import unittest

from alice_patch_collision_tests import build_and_run
from mohaa_permanent_decal_tests import BUILD_FIXTURE, build_and_run as build_and_run_cpp_aware

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


HELPER_CHECKS = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/bsp_index_checks.h"
#include <assert.h>
#include <limits.h>

int main( void ) {
	/* Empty and whole ranges fit, even at the end of a table or of an empty one. */
	assert( BSP_RangeInside( 0, 0, 0 ) && BSP_RangeInside( 5, 0, 5 ) && BSP_RangeInside( 0, 5, 5 ) );
	assert( BSP_RangeInside( 2, 3, 5 ) );
	/* Negative starts or counts, starts past the end, and ends past it do not,
	 * including ends whose sum would overflow. */
	assert( !BSP_RangeInside( -1, 0, 5 ) && !BSP_RangeInside( 0, -1, 5 ) && !BSP_RangeInside( 6, 0, 5 ) );
	assert( !BSP_RangeInside( 3, 3, 5 ) && !BSP_RangeInside( INT_MAX, INT_MAX, INT_MAX ) );
	assert( !BSP_RangeInside( 1, INT_MAX, INT_MAX ) && !BSP_RangeInside( INT_MIN, 0, 5 ) );
	assert( BSP_IndexInside( 0, 1 ) && BSP_IndexInside( 4, 5 ) );
	assert( !BSP_IndexInside( 5, 5 ) && !BSP_IndexInside( -1, 5 ) && !BSP_IndexInside( 0, 0 ) );
	assert( !BSP_IndexInside( INT_MIN, INT_MAX ) );
	/* A child is a node, or a leaf as -1 - leaf. */
	assert( BSP_ChildInside( 0, 1, 1 ) && BSP_ChildInside( -1, 1, 1 ) && BSP_ChildInside( -3, 1, 3 ) );
	assert( !BSP_ChildInside( 1, 1, 1 ) && !BSP_ChildInside( -2, 1, 1 ) && !BSP_ChildInside( -4, 1, 3 ) );
	assert( !BSP_ChildInside( INT_MIN, INT_MAX, INT_MAX ) && !BSP_ChildInside( INT_MAX, INT_MAX, 1 ) );
	/* Areas: none, or one of the 256 the area bits hold. Clusters: none or any. */
	assert( MAX_MAP_AREAS == 256 && MAX_MAP_AREAS == MAX_MAP_AREA_BYTES * 8 );
	assert( BSP_LeafAreaValid( -1 ) && BSP_LeafAreaValid( 0 ) && BSP_LeafAreaValid( 255 ) );
	assert( !BSP_LeafAreaValid( -2 ) && !BSP_LeafAreaValid( 256 ) && !BSP_LeafAreaValid( 0x12000000 ) );
	assert( !BSP_LeafAreaValid( INT_MIN ) );
	assert( BSP_LeafClusterValid( -1 ) && BSP_LeafClusterValid( 0 ) && BSP_LeafClusterValid( INT_MAX ) );
	assert( !BSP_LeafClusterValid( -2 ) && !BSP_LeafClusterValid( INT_MIN ) );
	return 0;
}
'''


COLLISION_PRELUDE = r'''
#include "code/qcommon/cm_local.h"
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BOX_BRUSHES 1
#define BOX_SIDES 6
#define BOX_LEAFS 2
#define BOX_PLANES 12
clipMap_t cm;
static byte *cmod_base;
static jmp_buf errors;
static char lastError[256];
static int lastLevel;
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile( void ) { return &profile; }
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
	va_list args;
	lastLevel = level;
	va_start( args, fmt );
	vsnprintf( lastError, sizeof( lastError ), fmt, args );
	va_end( args );
	longjmp( errors, 1 );
}
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
/* The hunk refuses a negative size since this change; the fixture asserts
 * that no loader asks for one. */
static union { double alignment; byte bytes[1 << 20]; } arena;
static int used, allocations;
void *Hunk_Alloc( int size, ha_pref preference ) {
	void *result;
	(void)preference;
	assert( size >= 0 && size <= (int)sizeof( arena ) - used );
	result = arena.bytes + used;
	used += ( size + 15 ) & ~15;
	allocations++;
	return result;
}
'''


COLLISION_CHECKS = r'''
/* A small map: two brushes of six axial sides, a node over a leaf and a node
 * over two more, two surfaces and a two-cluster visibility table. */
typedef struct {
	dshader_t shaders[2];
	dplane_t planes[8];
	dnode_t nodes[2];
	dleaf_t leafs[3];
	int leafSurfaces[2];
	int leafBrushes[2];
	dmodel_t models[2];
	dbrush_t brushes[2];
	dbrushside_t sides[12];
	dsurface_t surfaces[2];
	byte vis[8 + 18];
} map_t;
static map_t map;

static void Valid( void ) {
	int i;
	memset( &map, 0, sizeof( map ) );
	for ( i = 0; i < 2; i++ ) map.shaders[i].contentFlags = CONTENTS_SOLID;
	for ( i = 0; i < 8; i++ ) {
		map.planes[i].normal[i % 3] = i & 1 ? -1.0f : 1.0f;
		map.planes[i].dist = 64.0f;
	}
	map.nodes[0].planeNum = 6; map.nodes[0].children[0] = 1; map.nodes[0].children[1] = -1;
	map.nodes[1].planeNum = 7; map.nodes[1].children[0] = -2; map.nodes[1].children[1] = -3;
	/* leaf 0 is the solid outside, with no cluster, area or lists */
	map.leafs[0].cluster = -1; map.leafs[0].area = -1;
	map.leafs[1].cluster = 0; map.leafs[1].area = 0;
	map.leafs[1].firstLeafSurface = 0; map.leafs[1].numLeafSurfaces = 1;
	map.leafs[1].firstLeafBrush = 0; map.leafs[1].numLeafBrushes = 1;
	map.leafs[2].cluster = 1; map.leafs[2].area = 255;
	map.leafs[2].firstLeafSurface = 1; map.leafs[2].numLeafSurfaces = 1;
	map.leafs[2].firstLeafBrush = 1; map.leafs[2].numLeafBrushes = 1;
	map.leafSurfaces[0] = 0; map.leafSurfaces[1] = 1;
	map.leafBrushes[0] = 0; map.leafBrushes[1] = 1;
	map.models[0].firstSurface = 0; map.models[0].numSurfaces = 2;
	map.models[0].firstBrush = 0; map.models[0].numBrushes = 2;
	map.models[1].firstSurface = 1; map.models[1].numSurfaces = 1;
	map.models[1].firstBrush = 1; map.models[1].numBrushes = 1;
	for ( i = 0; i < 2; i++ ) { map.brushes[i].firstSide = 6 * i; map.brushes[i].numSides = 6; }
	for ( i = 0; i < 12; i++ ) map.sides[i].planeNum = i % 6;
	for ( i = 0; i < 2; i++ ) map.surfaces[i].surfaceType = MST_PLANAR;
	((int *)map.vis)[0] = 2; ((int *)map.vis)[1] = 1;
	map.vis[8] = 0x3; map.vis[9] = 0x3;
}

#define LUMP( field ) { (int)offsetof( map_t, field ), (int)sizeof( map.field ) }

/* As CM_LoadMap loads it; 1 if it loaded, 0 if it was refused. */
static int Load( void ) {
	const lump_t shaders = LUMP( shaders ), planes = LUMP( planes ), nodes = LUMP( nodes );
	const lump_t leafs = LUMP( leafs ), leafSurfaces = LUMP( leafSurfaces ), leafBrushes = LUMP( leafBrushes );
	const lump_t models = LUMP( models ), brushes = LUMP( brushes ), sides = LUMP( sides );
	const lump_t surfaces = LUMP( surfaces ), vis = LUMP( vis );
	memset( &cm, 0, sizeof( cm ) );
	used = allocations = 0;
	lastError[0] = 0;
	cmod_base = (byte *)&map;
	if ( setjmp( errors ) ) {
		assert( lastLevel == ERR_DROP );
		return 0;
	}
	CMod_LoadShaders( &shaders );
	CMod_LoadLeafs( &leafs );
	CMod_LoadLeafBrushes( &leafBrushes );
	CMod_LoadLeafSurfaces( &leafSurfaces, &surfaces );
	CMod_LoadPlanes( &planes );
	CMod_LoadBrushSides( &sides );
	CMod_LoadBrushes( &brushes );
	CMod_LoadSubmodels( &models, &surfaces );
	CMod_LoadNodes( &nodes );
	CMod_LoadVisibility( &vis );
	CMod_CheckLeafBrushes();
	return 1;
}

static void Refused( const char *why ) {
	if ( Load() || !strstr( lastError, why ) ) {
		fprintf( stderr, "expected a refusal naming \"%s\", got \"%s\"\n", why, lastError );
		abort();
	}
	Valid();
}

static void Loads( void ) {
	if ( !Load() ) {
		fprintf( stderr, "refused: %s\n", lastError );
		abort();
	}
	Valid();
}

int main( void ) {
	int i;

	profile.collisionModelHandleCount = 256;
	Valid();
	Loads();
	assert( Load() && cm.numNodes == 2 && cm.numLeafs == 3 && cm.numAreas == 256 && cm.vised );
	assert( cm.nodes[1].children[1] == -3 && cm.brushes[1].sides == cm.brushsides + 6 );
	assert( cm.leafsurfaces[cm.cmodels[1].leaf.firstLeafSurface] == 1 );
	Valid();

	/* Lump 3, nodes: a plane or a child that is not the map's. */
	map.nodes[1].planeNum = -1; Refused( "node 1 has a bad plane" );
	map.nodes[1].planeNum = 8; Refused( "node 1 has a bad plane" );
	map.nodes[0].children[0] = 2; Refused( "node 0 has a bad child" );
	map.nodes[1].children[1] = -4; Refused( "node 1 has a bad child" );
	map.nodes[1].children[1] = INT_MIN; Refused( "node 1 has a bad child" );
	/* the last leaf is the map's own */
	map.nodes[0].children[1] = -3; Loads();

	/* Lump 4, leafs: an area outside the 256 the area bits hold, such as the
	 * seeded corruption's 0x12000000 that wrapped the portal table's size,
	 * and a cluster below none. None and the last area load. */
	map.leafs[2].area = 0x12000000; Refused( "leaf 2 has a bad area or cluster" );
	map.leafs[2].area = 256; Refused( "leaf 2 has a bad area or cluster" );
	map.leafs[2].area = -2; Refused( "leaf 2 has a bad area or cluster" );
	map.leafs[1].cluster = -2; Refused( "leaf 1 has a bad area or cluster" );
	map.leafs[2].area = -1; map.leafs[2].cluster = -1; Loads();

	/* A leaf's lists must lie inside the leaf brush and leaf surface lumps;
	 * an empty one may sit at the very end. */
	map.leafs[1].firstLeafBrush = 2; Refused( "leaf 1 has a bad leaf brush range" );
	map.leafs[1].numLeafBrushes = -1; Refused( "leaf 1 has a bad leaf brush range" );
	map.leafs[2].firstLeafBrush = INT_MAX; map.leafs[2].numLeafBrushes = INT_MAX;
	Refused( "leaf 2 has a bad leaf brush range" );
	map.leafs[2].numLeafSurfaces = 2; Refused( "leaf 2 has a bad leaf surface range" );
	map.leafs[1].firstLeafSurface = -1; Refused( "leaf 1 has a bad leaf surface range" );
	map.leafs[2].firstLeafBrush = 2; map.leafs[2].numLeafBrushes = 0;
	map.leafs[2].firstLeafSurface = 2; map.leafs[2].numLeafSurfaces = 0; Loads();

	/* Lump 5, leaf surfaces: each names one of the map's surfaces. */
	map.leafSurfaces[1] = 2; Refused( "leaf surface 1 names a bad surface" );
	map.leafSurfaces[0] = -1; Refused( "leaf surface 0 names a bad surface" );

	/* Lump 6, leaf brushes: a brush that is not the map's is still repaired,
	 * as before, rather than refused. */
	map.leafBrushes[1] = 7;
	assert( Load() && cm.leafbrushes[1] == 0 );
	Valid();

	/* Lump 7, models: an inline model's brushes and surfaces are the map's,
	 * before their counts size its lists, so a count that would wrap a list's
	 * size is refused too. The world model's own, which collision never
	 * reads, are left alone. */
	map.models[1].numBrushes = 2; Refused( "model 1 has a bad brush or surface range" );
	map.models[1].firstSurface = -1; Refused( "model 1 has a bad brush or surface range" );
	map.models[1].numSurfaces = 0x40000001; Refused( "model 1 has a bad brush or surface range" );
	map.models[1].numBrushes = -1; Refused( "model 1 has a bad brush or surface range" );
	map.models[0].numBrushes = -1; map.models[0].firstSurface = 99; Loads();

	/* Lump 8, brushes: the sides a brush claims, and the six CM_BoundBrush
	 * reads, are the map's. A brush claiming fewer whose six are there loads,
	 * as it always has. */
	map.brushes[1].firstSide = 7; Refused( "brush 1 has a bad side range" );
	map.brushes[0].numSides = 13; Refused( "brush 0 has a bad side range" );
	map.brushes[0].firstSide = -6; Refused( "brush 0 has a bad side range" );
	map.brushes[1].firstSide = 8; map.brushes[1].numSides = 3; Refused( "brush 1 has a bad side range" );
	map.brushes[1].numSides = 3; Loads();

	/* Lump 9, brush sides: each plane is the map's. */
	map.sides[11].planeNum = 8; Refused( "brush side 11 has a bad plane" );
	map.sides[0].planeNum = -1; Refused( "brush side 0 has a bad plane" );

	/* Lump 16: a table that names fewer clusters than the leafs do, or whose
	 * rows are too short for its clusters, leaves the map unvised, keeping
	 * the leafs' own count, as a table that does not fit its payload does. */
	((int *)map.vis)[0] = 1;
	assert( Load() && !cm.vised && cm.numClusters == 2 && cm.clusterBytes >= 1 );
	Valid();
	((int *)map.vis)[0] = 9; ((int *)map.vis)[1] = 1;
	map.leafs[2].cluster = 8;
	assert( Load() && !cm.vised && cm.numClusters == 9 );
	Valid();
	((int *)map.vis)[0] = 9; ((int *)map.vis)[1] = 2;
	map.leafs[2].cluster = 8;
	assert( Load() && cm.vised && cm.numClusters == 9 && cm.clusterBytes == 2 );
	Valid();
	((int *)map.vis)[0] = 17;
	assert( Load() && !cm.vised && cm.numClusters == 2 );
	Valid();
	assert( Load() && cm.vised && cm.numClusters == 2 );
	Valid();

	/* Nothing a refusal left behind changes the next load. */
	for ( i = 0; i < 3; i++ ) Loads();
	return 0;
}
'''


def collision_sources():
    load = read('code/qcommon/cm_load.c')
    functions = ['static void CMod_LoadShaders(', 'static void CMod_LoadSubmodels(', 'static void CMod_LoadNodes(',
                 'static void CM_BoundBrush(', 'static void CMod_LoadBrushes(', 'static void CMod_LoadLeafs(',
                 'static void CMod_LoadPlanes(', 'static void CMod_LoadLeafBrushes(',
                 'static void CMod_LoadLeafSurfaces(', 'static void CMod_CheckLeafBrushes(',
                 'static void CMod_LoadBrushSides(']
    source = COLLISION_PRELUDE
    for signature in functions:
        source += function(load, signature)
    visibility = load[load.index('#define\tVIS_HEADER'):load.index('//====', load.index('#define\tVIS_HEADER'))]
    return source + visibility + COLLISION_CHECKS


HUNK_PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <string.h>
typedef struct { int mark, permanent, temp, tempHighwater; } hunkUsed_t;
typedef struct hunkblock_s { int size; byte printed; struct hunkblock_s *next; const char *label, *file; int line; } hunkblock_t;
static hunkUsed_t hunk_low, hunk_high, *hunk_permanent = &hunk_low, *hunk_temp = &hunk_high;
static union { double alignment; byte bytes[4096]; } storage;
static byte *s_hunkData = storage.bytes;
static int s_hunkTotal = sizeof( storage );
static jmp_buf errors;
static const char *lastError;
static int cleared;
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { assert( level == ERR_DROP ); lastError = fmt; longjmp( errors, 1 ); }
static void Hunk_SwapBanks( void ) {}
#undef Com_Memset
static void Com_Memset( void *buf, int value, size_t count ) {
	assert( (byte *)buf >= storage.bytes && (byte *)buf + count <= storage.bytes + sizeof( storage ) );
	cleared++; memset( buf, value, count );
}
'''


HUNK_CHECKS = r'''
static int Refused( int size ) {
	const hunkUsed_t low = hunk_low, high = hunk_high;
	cleared = 0;
	if ( setjmp( errors ) ) {
		assert( !cleared && !memcmp( &low, &hunk_low, sizeof( low ) ) && !memcmp( &high, &hunk_high, sizeof( high ) ) );
		return 1;
	}
	Hunk_Alloc( size, h_low );
	return 0;
}

int main( void ) {
	/* A negative size, as a wrapped count gives, and sizes whose padding would
	 * wrap are refused before anything is cleared: among them the area table
	 * a leaf's area of 0x12000000 used to size (0x12000001 eight-byte areas). */
	assert( Refused( -1 ) && !strcmp( lastError, "Hunk_Alloc: invalid size %i" ) );
	assert( Refused( INT_MIN ) && Refused( INT_MAX ) && Refused( INT_MAX - 63 ) );
	assert( Refused( (int)( 0x12000001u * 8u ) ) );
	/* An allocation that fits is cleared and kept, rounded to a cacheline. */
	assert( !Refused( 100 ) && cleared == 1 && hunk_low.permanent == 128 );
	assert( !Refused( 0 ) && hunk_low.permanent == 128 );
	/* One that does not fit is refused as before, and the comparison does not
	 * wrap when the banks already hold nearly all of a large hunk. */
	assert( Refused( sizeof( storage ) ) && !strcmp( lastError, "Hunk_Alloc failed on %i" ) );
	s_hunkTotal = INT_MAX - 10; hunk_low.temp = INT_MAX - 100; hunk_high.temp = 50;
	assert( Refused( 1000 ) && !strcmp( lastError, "Hunk_Alloc failed on %i" ) );
	return 0;
}
'''


def hunk_source():
    common = read('code/qcommon/common.c')
    start = common.index('#ifdef HUNK_DEBUG\nvoid *Hunk_AllocDebug( int size, ha_pref preference')
    return HUNK_PRELUDE + common[start:common.index('\n}\n', start) + 3] + HUNK_CHECKS


RENDERER_HOST = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/bsp_index_checks.h"
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct shader_s { int numUnfoggedPasses, lightingStage; } shader_t;
'''


RENDERER_WORLD = r'''
/* Only what R_LoadSubmodels writes of a brush model and its model. */
typedef struct { int unused; } fixtureDecal_t;
typedef struct {
	vec3_t bounds[2];
	msurface_t *firstSurface; int numSurfaces;
	int numWolfETDecals; fixtureDecal_t *wolfetDecals;
} bmodel_t;
enum { MOD_BAD, MOD_BRUSH };
typedef struct { char name[MAX_QPATH]; int type; bmodel_t *bmodel; } model_t;
enum { FN_WOLFET_MAX_WORLD_DECALS = 1024, FN_WOLFET_MAX_ENTITY_DECALS = 128 };
static model_t models[4];
static int numModels;
static model_t *R_AllocModel( void ) { return numModels < 4 ? &models[numModels++] : NULL; }
int QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) {
	va_list args; int n;
	va_start( args, fmt ); n = vsnprintf( dest, size, fmt, args ); va_end( args );
	return n;
}
static struct {
	char name[MAX_QPATH];
	bmodel_t *bmodels; int numBmodels;
	msurface_t *surfaces; int numsurfaces;
	msurface_t **marksurfaces; int nummarksurfaces;
	mnode_t *nodes; int numnodes, numDecisionNodes;
	cplane_t *planes; int numplanes;
	int numClusters, clusterBytes;
	byte *novis, *vis;
	dshader_t *shaders; int numShaders;
} s_worldData;
static byte *fileBase;
static shader_t defaultShader, mapShader;
static struct {
	qboolean mergeLightmaps; float lightmapOffset[2], lightmapScale[2];
	shader_t *defaultShader; byte *externalVisData;
} tr = { qfalse, { 0, 0 }, { 1, 1 }, &defaultShader, NULL };
static jmp_buf errors;
static char lastError[256];
static void QDECL Error( errorParm_t level, const char *fmt, ... ) {
	va_list args;
	assert( level == ERR_DROP );
	va_start( args, fmt );
	vsnprintf( lastError, sizeof( lastError ), fmt, args );
	va_end( args );
	longjmp( errors, 1 );
}
static void QDECL Print( int level, const char *fmt, ... ) { (void)level; (void)fmt; }
static union { double alignment; byte bytes[1 << 20]; } arena;
static int used;
static void *HunkAlloc( int size, ha_pref preference ) {
	void *result;
	(void)preference;
	assert( size >= 0 && size <= (int)sizeof( arena ) - used );
	result = arena.bytes + used;
	used += ( size + 15 ) & ~15;
	memset( result, 0, size );
	return result;
}
static struct {
	void ( QDECL *Error )( errorParm_t, const char *, ... );
	void ( QDECL *Printf )( int, const char *, ... );
	void *( *Hunk_Alloc )( int, ha_pref );
	comGameId_t gameId;
} ri = { Error, Print, HunkAlloc, COM_GAME_QUAKE3 };
static shader_t *ShaderForShaderNum( const int shaderNum, int lightmapNum ) {
	(void)lightmapNum;
	if ( shaderNum < 0 || shaderNum >= s_worldData.numShaders ) {
		ri.Error( ERR_DROP, "ShaderForShaderNum: bad num %i", shaderNum );
	}
	return &mapShader;
}
static int R_GetLightmapCoords( const int lightmapIndex, float *x, float *y ) { *x = *y = 0; return lightmapIndex; }
static void R_ColorShiftLightingBytes( const byte in[4], byte out[4], qboolean hasAlpha ) { (void)hasAlpha; memcpy( out, in, 4 ); }
static float R_ClampDenorm( float v ) { return v; }
static srfGridMesh_t grid;
static int subdivided;
static srfGridMesh_t *R_SubdividePatchToGrid( int width, int height, drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] ) {
	(void)width; (void)height; (void)points;
	subdivided++;
	return &grid;
}
#define R_MOHAAStaticDecal( world, surf ) qfalse
'''


RENDERER_CHECKS = r'''
/* Two nodes over three leafs, two surfaces each listed by a leaf, and a
 * two-cluster visibility table; the vertex and index lumps the surface
 * parsers read. */
typedef struct {
	dnode_t nodes[2];
	dleaf_t leafs[3];
	int leafSurfaces[2];
	byte vis[8 + 18];
	dmodel_t models[2];
} map_t;
static map_t map;
static cplane_t planes[8];
static msurface_t surfaces[4];
static dshader_t shaders[2];
static drawVert_t verts[1100];
static int indexes[400];

static void Valid( void ) {
	int i;
	memset( &map, 0, sizeof( map ) );
	map.nodes[0].planeNum = 6; map.nodes[0].children[0] = 1; map.nodes[0].children[1] = -1;
	map.nodes[1].planeNum = 7; map.nodes[1].children[0] = -2; map.nodes[1].children[1] = -3;
	map.leafs[0].cluster = -1; map.leafs[0].area = -1;
	map.leafs[1].cluster = 0; map.leafs[1].area = 0; map.leafs[1].numLeafSurfaces = 1;
	map.leafs[2].cluster = 1; map.leafs[2].area = 255;
	map.leafs[2].firstLeafSurface = 1; map.leafs[2].numLeafSurfaces = 1;
	map.leafSurfaces[0] = 0; map.leafSurfaces[1] = 1;
	((int *)map.vis)[0] = 2; ((int *)map.vis)[1] = 1;
	map.models[0].numSurfaces = 2;
	map.models[1].firstSurface = 1; map.models[1].numSurfaces = 1;
	for ( i = 0; i < 400; i++ ) indexes[i] = i % 3;
}

#define LUMP( field ) { (int)offsetof( map_t, field ), (int)sizeof( map.field ) }

static void Reset( void ) {
	memset( &s_worldData, 0, sizeof( s_worldData ) );
	strcpy( s_worldData.name, "maps/fixture.bsp" );
	s_worldData.planes = planes; s_worldData.numplanes = 8;
	s_worldData.surfaces = surfaces; s_worldData.numsurfaces = 2;
	s_worldData.shaders = shaders; s_worldData.numShaders = 2;
	fileBase = (byte *)&map;
	used = 0; lastError[0] = 0; subdivided = 0; numModels = 0;
	ri.gameId = COM_GAME_QUAKE3;
}

/* The brush models as RE_LoadWorldMap loads them. */
static int Models( void ) {
	const lump_t models = LUMP( models );
	Reset();
	if ( setjmp( errors ) ) return 0;
	R_LoadSubmodels( &models );
	return 1;
}

/* The tree as RE_LoadWorldMap loads it; 1 if it loaded, 0 if it was refused. */
static int Tree( void ) {
	const lump_t nodes = LUMP( nodes ), leafs = LUMP( leafs ), leafSurfaces = LUMP( leafSurfaces ), vis = LUMP( vis );
	Reset();
	if ( setjmp( errors ) ) return 0;
	R_LoadMarksurfaces( &leafSurfaces );
	R_LoadNodesAndLeafs( &nodes, &leafs );
	R_LoadVisibility( &vis );
	return 1;
}

/* One surface record, parsed as R_LoadSurfaces parses it. */
static int Surface( const dsurface_t *ds, int lumpVerts, int lumpIndexes ) {
	Reset();
	memset( surfaces, 0, sizeof( surfaces ) );
	if ( setjmp( errors ) ) return 0;
	switch ( ds->surfaceType ) {
	case MST_PLANAR: ParseFace( ds, verts, lumpVerts, &surfaces[0], indexes, lumpIndexes ); break;
	case MST_TRIANGLE_SOUP: ParseTriSurf( ds, verts, lumpVerts, &surfaces[0], indexes, lumpIndexes ); break;
	case MST_PATCH: ParseMesh( ds, verts, lumpVerts, &surfaces[0] ); break;
	default: assert( 0 );
	}
	return 1;
}

static void Expect( int loaded, const char *why ) {
	if ( loaded || !strstr( lastError, why ) ) {
		fprintf( stderr, "expected a refusal naming \"%s\", got \"%s\"\n", why, loaded ? "a load" : lastError );
		abort();
	}
	Valid();
}

static dsurface_t Face( int firstVert, int numVerts, int firstIndex, int numIndexes ) {
	dsurface_t ds;
	memset( &ds, 0, sizeof( ds ) );
	ds.surfaceType = MST_PLANAR; ds.fogNum = -1;
	ds.firstVert = firstVert; ds.numVerts = numVerts;
	ds.firstIndex = firstIndex; ds.numIndexes = numIndexes;
	return ds;
}

static dsurface_t Patch( int firstVert, int width, int height ) {
	dsurface_t ds;
	memset( &ds, 0, sizeof( ds ) );
	ds.surfaceType = MST_PATCH; ds.fogNum = -1;
	ds.firstVert = firstVert; ds.patchWidth = width; ds.patchHeight = height;
	return ds;
}

int main( void ) {
	dsurface_t ds;
	int i;

	Valid();
	assert( Tree() && s_worldData.numnodes == 5 && s_worldData.nodes[3].parent == &s_worldData.nodes[1] );
	assert( s_worldData.nodes[4].firstmarksurface[0] == &surfaces[1] && s_worldData.vis );
	assert( s_worldData.numClusters == 2 );
	Valid();

	/* Nodes: a plane or a child that is not the map's; the last leaf is. */
	map.nodes[1].planeNum = 8; Expect( Tree(), "node 1 has a bad plane" );
	map.nodes[0].planeNum = -1; Expect( Tree(), "node 0 has a bad plane" );
	map.nodes[0].children[0] = 2; Expect( Tree(), "node 0 has a bad child" );
	map.nodes[1].children[1] = -4; Expect( Tree(), "node 1 has a bad child" );
	map.nodes[1].children[0] = INT_MIN; Expect( Tree(), "node 1 has a bad child" );
	map.nodes[0].children[1] = -3; assert( Tree() ); Valid();

	/* Leafs: an area outside the area mask, a cluster below none, a surface
	 * range outside the leaf surfaces; an empty range at the end loads. */
	map.leafs[2].area = 256; Expect( Tree(), "leaf 2 has a bad area, cluster or surface range" );
	map.leafs[1].area = -2; Expect( Tree(), "leaf 1 has a bad area, cluster or surface range" );
	map.leafs[1].cluster = -2; Expect( Tree(), "leaf 1 has a bad area, cluster or surface range" );
	map.leafs[2].numLeafSurfaces = 2; Expect( Tree(), "leaf 2 has a bad area, cluster or surface range" );
	map.leafs[1].firstLeafSurface = -1; Expect( Tree(), "leaf 1 has a bad area, cluster or surface range" );
	map.leafs[2].firstLeafSurface = 2; map.leafs[2].numLeafSurfaces = 0; assert( Tree() ); Valid();

	/* Leaf surfaces name the map's surfaces. */
	map.leafSurfaces[1] = 2; Expect( Tree(), "leaf surface 1 names a bad surface" );
	map.leafSurfaces[0] = -1; Expect( Tree(), "leaf surface 0 names a bad surface" );

	/* Visibility: a partial header is refused, as the collision loader
	 * refuses it; a table that does not fit, names fewer clusters than the
	 * leafs or has rows too short leaves the map unvised with the leafs'
	 * own count. */
	{
		const lump_t partial = { (int)offsetof( map_t, vis ), 4 };
		Reset();
		if ( !setjmp( errors ) ) { R_LoadVisibility( &partial ); assert( 0 ); }
		assert( strstr( lastError, "truncated visibility lump" ) );
		Valid();
	}
	((int *)map.vis)[0] = 19; assert( Tree() && !s_worldData.vis && s_worldData.numClusters == 2 ); Valid();
	((int *)map.vis)[0] = 1; assert( Tree() && !s_worldData.vis && s_worldData.numClusters == 2 ); Valid();
	((int *)map.vis)[0] = 17; assert( Tree() && !s_worldData.vis && s_worldData.numClusters == 2 ); Valid();
	((int *)map.vis)[0] = 9; ((int *)map.vis)[1] = 2; map.leafs[2].cluster = 8;
	assert( Tree() && s_worldData.vis && s_worldData.numClusters == 9 && s_worldData.clusterBytes == 2 ); Valid();

	/* Faces: their vertices and indexes lie inside the lumps, and each index
	 * names one of the face's own vertices. */
	ds = Face( 0, 4, 0, 6 ); assert( Surface( &ds, 4, 6 ) && surfaces[0].data );
	ds = Face( 1, 4, 0, 6 ); Expect( Surface( &ds, 4, 6 ), "surface 0 has a bad vertex or index range" );
	ds = Face( -1, 4, 0, 6 ); Expect( Surface( &ds, 200, 6 ), "surface 0 has a bad vertex or index range" );
	ds = Face( 0, -1, 0, 6 ); Expect( Surface( &ds, 200, 6 ), "surface 0 has a bad vertex or index range" );
	ds = Face( 0, 4, 1, 6 ); Expect( Surface( &ds, 4, 6 ), "surface 0 has a bad vertex or index range" );
	ds = Face( 0, 4, 0, -3 ); Expect( Surface( &ds, 4, 6 ), "surface 0 has a bad vertex or index range" );
	ds = Face( 0, 4, INT_MAX, INT_MAX ); Expect( Surface( &ds, 4, 6 ), "surface 0 has a bad vertex or index range" );
	indexes[4] = 4; ds = Face( 0, 4, 0, 6 ); Expect( Surface( &ds, 4, 6 ), "Bad index in face surface" );
	indexes[0] = -1; ds = Face( 0, 4, 0, 6 ); Expect( Surface( &ds, 4, 6 ), "Bad index in face surface" );
	/* A face over the vertex limit keeps its first 64 and loads as before,
	 * its indexes still measured against every vertex it declares (Elite
	 * Force's scav5 has two of 109). Enemy Territory's limit is a batch. */
	for ( i = 0; i < 321; i++ ) indexes[i] = i % 109;
	ds = Face( 0, 109, 0, 321 ); assert( Surface( &ds, 200, 400 ) && surfaces[0].shader == &defaultShader );
	ri.gameId = COM_GAME_WOLFET;
	ds = Face( 0, 109, 0, 321 );
	Reset(); ri.gameId = COM_GAME_WOLFET; memset( surfaces, 0, sizeof( surfaces ) );
	if ( setjmp( errors ) ) assert( 0 );
	ParseFace( &ds, verts, 200, &surfaces[0], indexes, 400 );
	assert( surfaces[0].shader == &mapShader );
	Valid();

	/* Triangle surfaces: the same ranges; their index values were checked. */
	ds = Face( 0, 4, 0, 6 ); ds.surfaceType = MST_TRIANGLE_SOUP; assert( Surface( &ds, 4, 6 ) );
	ds.firstVert = 3; Expect( Surface( &ds, 4, 6 ), "surface 0 has a bad vertex or index range" );
	ds = Face( 0, 4, 3, 6 ); ds.surfaceType = MST_TRIANGLE_SOUP; Expect( Surface( &ds, 4, 6 ), "surface 0 has a bad vertex or index range" );
	ds = Face( 0, INT_MAX, 0, 6 ); ds.surfaceType = MST_TRIANGLE_SOUP; Expect( Surface( &ds, 4, 6 ), "surface 0 has a bad vertex or index range" );
	indexes[2] = 4; ds = Face( 0, 4, 0, 6 ); ds.surfaceType = MST_TRIANGLE_SOUP;
	Expect( Surface( &ds, 4, 6 ), "Bad index in triangle surface" );

	/* Patches: the control points lie inside the vertex lump and fit the
	 * arrays they are copied to. A patch that is never drawn is never read. */
	ds = Patch( 0, 3, 3 ); assert( Surface( &ds, 9, 0 ) && subdivided == 1 );
	ds = Patch( 1, 3, 3 ); Expect( Surface( &ds, 9, 0 ), "surface 0 has a bad size or vertex range" );
	ds = Patch( 0, 0, 3 ); Expect( Surface( &ds, 9, 0 ), "surface 0 has a bad size or vertex range" );
	ds = Patch( 0, 3, -3 ); Expect( Surface( &ds, 9, 0 ), "surface 0 has a bad size or vertex range" );
	ds = Patch( 0, MAX_GRID_SIZE + 1, 1 ); Expect( Surface( &ds, 200, 0 ), "surface 0 has a bad size or vertex range" );
	ds = Patch( 0, 33, 33 ); Expect( Surface( &ds, 1100, 0 ), "surface 0 has a bad size or vertex range" );
	ds = Patch( 0, 0x10001, 0x10001 ); Expect( Surface( &ds, 200, 0 ), "surface 0 has a bad size or vertex range" );
	ds = Patch( 0, 31, 3 ); assert( Surface( &ds, 200, 0 ) && subdivided == 1 );
	shaders[1].surfaceFlags = SURF_NODRAW;
	ds = Patch( 9999, -1, 0x10001 ); ds.shaderNum = 1; assert( Surface( &ds, 9, 0 ) && !subdivided );
	shaders[1].surfaceFlags = 0;

	/* Brush models: each one's surfaces are the map's, the world's included,
	 * before a model is made for it. */
	assert( Models() && numModels == 2 && s_worldData.bmodels[1].firstSurface == &surfaces[1] );
	Valid();
	map.models[1].numSurfaces = 2; Expect( Models(), "model 1 has a bad surface range" );
	assert( numModels == 1 );
	map.models[0].firstSurface = -1; Expect( Models(), "model 0 has a bad surface range" );
	map.models[1].firstSurface = INT_MAX; map.models[1].numSurfaces = INT_MAX;
	Expect( Models(), "model 1 has a bad surface range" );
	map.models[1].firstSurface = 2; map.models[1].numSurfaces = 0; assert( Models() );
	return 0;
}
'''


def renderer_source(renderer):
    local = read(f'code/{renderer}/tr_local.h')
    bsp = read(f'code/{renderer}/tr_bsp.c')

    def record(name):
        end = local.index('} %s;' % name)
        return local[local.rindex('typedef struct', 0, end):end + len(name) + 3] + '\n'

    source = RENDERER_HOST
    source += '\n'.join(re.findall(r'^#define LIGHTMAP_[^\n]+', read(f'code/{renderer}/tr_common.h'), re.M)) + '\n'
    source += re.search(r'typedef enum\s*\{[^{}]*SF_BAD[^{}]*\}\s*surfaceType_t;', local)[0] + '\n'
    for name in ('VERTEXSIZE', 'MAX_FACE_POINTS', 'MAX_PATCH_SIZE', 'MAX_GRID_SIZE'):
        source += re.search(r'^#define\s+%s\b[^\n]*' % name, local, re.M)[0] + '\n'
    for name in ('srfSurfaceFace_t', 'srfTriangles_t', 'srfGridMesh_t', 'msurface_t', 'mnode_t'):
        source += record(name)
    source += RENDERER_WORLD
    if 'void qsort_idx(' in bsp:
        source += function(bsp, 'void qsort_idx(')
    for signature in ('static void R_LoadVisibility(', 'static void ParseFace(', 'static void ParseMesh(',
                      'static void ParseTriSurf(', 'static void R_SetParent(', 'static void R_LoadNodesAndLeafs(',
                      'static void R_LoadMarksurfaces(', 'static void R_LoadSubmodels('):
        source += function(bsp, signature)
    return source + RENDERER_CHECKS


MOHAA_CHECKS = r'''
/* Medal of Honor's world rebuild (code/renderercommon/tr_mohaa_world.c) runs
 * before the renderers' loaders and copies each leaf's own surface list into a
 * list the size of the map's. Two leafs over a two-entry list, whose ranges
 * each case sets. */
static const byte palette[4];
static const int ownList[2] = { 7, 9 };

static void Lists( int first0, int count0, int first1, int count1 ) {
	dheader_t *header = (dheader_t *)map;
	dleaf_t *leafs;
	BuildMap();
	Lump( header, LUMP_LEAFSURFACES, ownList, sizeof( ownList ) );
	leafs = (dleaf_t *)( map + header->lumps[LUMP_LEAFS].fileofs );
	leafs[0].firstLeafSurface = first0; leafs[0].numLeafSurfaces = count0;
	leafs[1].firstLeafSurface = first1; leafs[1].numLeafSurfaces = count1;
}

/* Rebuilds, with a light grid asked for so that something is built; the
 * fixture's world is too small for one, which only warns. */
static qboolean Rebuild( byte **out ) {
	mohaa_translation_t translation;
	mohaaBuiltSurfaces_t built;
	void *data = NULL;
	int size = 0;
	qboolean result;
	memset( &translation, 0, sizeof( translation ) );
	translation.data = map;
	translation.length = mapLength;
	translation.lightGridPalette = palette;
	warnings = reads = 0;
	result = R_MOHAABuildWorld( &translation, "maps/fixture.bsp", &data, &size, &built );
	*out = data;
	return result;
}

static void Expect( const byte *out, int leaf, int count, const int *values ) {
	int leafCount, listCount, i;
	const dleaf_t *leafs = Out( out, LUMP_LEAFS, sizeof( dleaf_t ), &leafCount );
	const int *list = Out( out, LUMP_LEAFSURFACES, sizeof( int ), &listCount );
	assert( leaf < leafCount && leafs[leaf].numLeafSurfaces == count );
	for ( i = 0; i < count; i++ ) {
		assert( leafs[leaf].firstLeafSurface + i < listCount && list[leafs[leaf].firstLeafSurface + i] == values[i] );
	}
}

int main( void ) {
	static const int first[1] = { 7 }, second[1] = { 9 };
	byte *out;

	ri.Printf = Printf; ri.Malloc = Malloc; ri.Free = Free;
	ri.FS_ReadFile = ReadFile; ri.FS_FreeFile = FreeFile; ri.Cvar_VariableIntegerValue = CvarInteger;

	/* Lists that partition the map's, as every installed map's do, are
	 * copied as they were. */
	Lists( 0, 1, 1, 1 );
	assert( Rebuild( &out ) && reads == 1 );
	Expect( out, 0, 1, first ); Expect( out, 1, 1, second );
	free( out );

	/* A list that starts or ends outside the map's keeps the part inside,
	 * as before, without stepping through the rest of a huge count. */
	Lists( -5, 7, 0, 0 );
	assert( Rebuild( &out ) );
	Expect( out, 0, 2, ownList ); Expect( out, 1, 0, NULL );
	free( out );
	Lists( 1, INT_MAX, INT_MIN, INT_MAX );
	assert( Rebuild( &out ) );
	Expect( out, 0, 1, second ); Expect( out, 1, 0, NULL );
	free( out );

	/* Lists that overlap would outgrow the list they are copied into: the
	 * rebuild is declined, with a warning, before anything is read or
	 * allocated, and the map loads as translated. */
	Lists( 0, 2, 1, 1 );
	assert( !Rebuild( &out ) && !out && warnings == 1 && reads == 0 );
	Lists( 0, 2, 0, 2 );
	assert( !Rebuild( &out ) && !out && warnings == 1 && reads == 0 );
	puts( "mohaa lists ok" );
	return 0;
}
'''


class MohaaWorldRebuildTests(unittest.TestCase):
    def test_the_rebuild_copies_only_leaf_lists_that_fit_the_map(self):
        output = build_and_run_cpp_aware(self, 'mohaalists', [
            BUILD_FIXTURE + MOHAA_CHECKS,
            ROOT / 'code/renderercommon/tr_mohaa_world.c',
            ROOT / 'code/qcommon/q_shared.c',
            ROOT / 'code/qcommon/q_math.c',
        ])
        self.assertIn('mohaa lists ok', output)


class BspIndexHelperTests(unittest.TestCase):
    def test_the_shared_checks_bound_every_range_and_index(self):
        build_and_run(self, 'indexchecks', [HELPER_CHECKS])


class CollisionLoaderIndexTests(unittest.TestCase):
    def test_the_collision_loader_refuses_indexes_outside_their_tables(self):
        build_and_run(self, 'cmindexes', [collision_sources()])

    def test_every_check_comes_before_the_first_use_of_its_index(self):
        load = read('code/qcommon/cm_load.c')
        nodes = function(load, 'static void CMod_LoadNodes(')
        self.assertLess(nodes.index('BSP_IndexInside( LittleLong( in->planeNum ), cm.numPlanes )'),
                        nodes.index('out->plane = cm.planes + LittleLong( in->planeNum );'))
        self.assertLess(nodes.index('BSP_ChildInside( child, count, cm.numLeafs )'),
                        nodes.index('out->children[j] = child;'))
        brushes = function(load, 'static void CMod_LoadBrushes(')
        self.assertLess(brushes.index('BSP_RangeInside( LittleLong( in->firstSide ), 6, cm.numBrushSides )'),
                        brushes.index('CM_BoundBrush( out );'))
        leafs = function(load, 'static void CMod_LoadLeafs(')
        self.assertLess(leafs.index('BSP_LeafAreaValid( out->area )'),
                        leafs.index('cm.areas = Hunk_Alloc( cm.numAreas * sizeof( *cm.areas ), h_high );'))
        models = function(load, 'static void CMod_LoadSubmodels(')
        self.assertLess(models.index('LittleLong( in->numBrushes ), cm.numBrushes )'),
                        models.index('indexes = Hunk_Alloc( out->leaf.numLeafBrushes * 4, h_high );'))
        # both lists that name surfaces are measured against the surface lump
        self.assertIn('CMod_LoadLeafSurfaces (&header.lumps[LUMP_LEAFSURFACES], &header.lumps[LUMP_SURFACES]);', load)
        self.assertIn('CMod_LoadSubmodels (&header.lumps[LUMP_MODELS], &header.lumps[LUMP_SURFACES]);', load)
        self.assertIn('#include "bsp_index_checks.h"', read('code/qcommon/cm_local.h'))


class RendererLoaderIndexTests(unittest.TestCase):
    def test_every_renderer_refuses_indexes_outside_their_tables(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                build_and_run(self, f'rindexes{RENDERERS.index(renderer)}', [
                    renderer_source(renderer), ROOT / 'code/qcommon/q_math.c'])

    def test_every_renderer_bounds_submodel_surfaces_and_fog_planes_before_reading_them(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                bsp = read(f'code/{renderer}/tr_bsp.c')
                self.assertIn('#include "../qcommon/bsp_index_checks.h"', bsp)
                models = function(bsp, 'static void R_LoadSubmodels(')
                self.assertLess(models.index('BSP_RangeInside( LittleLong( in->firstSurface ), '
                                             'LittleLong( in->numSurfaces ), s_worldData.numsurfaces )'),
                                models.index('model = R_AllocModel();'))
                fogs = function(bsp, 'static void R_LoadFogs(')
                self.assertNotIn('sidesCount - 6', fogs)
                check = fogs.index('if ( !BSP_RangeInside( firstSide, 6, sidesCount ) ) {')
                planes = fogs.index('if ( !BSP_IndexInside( LittleLong( sides[ sideNum ].planeNum ), '
                                    's_worldData.numplanes ) ) {')
                self.assertLess(check, planes)
                self.assertLess(planes, fogs.index('out->bounds[0][0] = -s_worldData.planes[ planeNum ].dist;'))
                visible = fogs.index('BSP_IndexInside( LittleLong( sides[ sideOffset ].planeNum ), s_worldData.numplanes )')
                self.assertLess(visible, fogs.index('out->surface[3] = -s_worldData.planes[ planeNum ].dist;'))
                surfaces = function(bsp, 'static void R_LoadSurfaces(')
                for call in ('ParseMesh( in, dv, lumpVerts, out );',
                             'ParseTriSurf( in, dv, lumpVerts, out, indexes, lumpIndexes );',
                             'ParseFace( in, dv, lumpVerts, out, indexes, lumpIndexes );'):
                    self.assertIn(call, surfaces)


class HunkAllocationTests(unittest.TestCase):
    def test_a_negative_or_wrapping_size_is_refused_before_anything_is_cleared(self):
        build_and_run(self, 'hunkalloc', [hunk_source()])


if __name__ == '__main__':
    unittest.main()
