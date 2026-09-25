"""Medal of Honor's terrain as it is walked on, drawn and marked.

A terrain patch's variance tree takes triangles out of the terrain: the
collision model walks through them and 2015's renderer never drew them.
code/qcommon/mohaa_terrain_squares.h is the one reading of those bits, which
the collision model (code/qcommon/cm_mohaa_terrain.c) and the renderers'
translated world (code/renderercommon/tr_mohaa_world.c) share; 1.00's maps
take out from 1.6% (m6l1c) to 80% (m5l3) of their terrain's triangles.

Decals are cut by code/renderercommon/tr_mohaa_marks_surface.h, which the
three renderers include after their Quake III clipper: the world's faces and
curves as Quake III cuts them, a terrain patch's triangles 128 units before
and behind a decal with the patch's number plus one returned beside each
fragment, nothing on static models or the level's baked decals, no terrain
for a decal no larger than ter_minMarkRadius, and a brush model's surfaces in
the model's own space. The client game's two mark imports serve it.

The fixtures compile the shared header, the world builder and the cutter
itself (with the clipper sliced from code/renderer/tr_marks.c); no retail
file is read."""
import re
import unittest
from pathlib import Path

from mohaa_permanent_decal_tests import BUILD_FIXTURE, build_and_run, function, read

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

SQUARES = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/mohaa_terrain_squares.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; exit( 3 ); }
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }

static mohaaTerrainSquareTable_t table;

static void Bit( dterrainPatchMOHAA_t *patch, const mohaaVarnodeIndex_t *v ) {
	patch->varTree[v->iTreeAndMask & 1][v->iNode].flags |= (uint16_t)( v->iTreeAndMask & 0xFFFE );
}

int main( void ) {
	static int seen[2][2][MOHAA_TERRAIN_VARNODES][2];
	dterrainPatchMOHAA_t patch;
	int diag, i, j, h, ii, jj, mode;

	MOHAA_BuildTerrainSquareTable( &table );

	/* every triangle has a bit of its own, on a node of the last level */
	for ( diag = 0; diag < 2; diag++ ) {
		for ( i = 0; i < 8; i++ ) {
			for ( j = 0; j < 8; j++ ) {
				for ( h = 0; h < 2; h++ ) {
					const mohaaVarnodeIndex_t *v = &table.vni[diag][i][j][h];
					const int mask = v->iTreeAndMask & 0xFFFE;
					assert( v->iNode >= 31 && v->iNode <= 62 );
					assert( mask == 0x1000 || mask == 0x2000 );
					assert( !seen[diag][v->iTreeAndMask & 1][v->iNode][mask == 0x2000] );
					seen[diag][v->iTreeAndMask & 1][v->iNode][mask == 0x2000] = 1;
				}
			}
		}
	}

	for ( diag = 0; diag < 2; diag++ ) {
		/* no bit set: both halves of every square */
		memset( &patch, 0, sizeof( patch ) );
		patch.flags = diag ? MOHAA_TERPATCH_NEIGHBOR : 0;
		for ( i = 0; i < 8; i++ ) {
			for ( j = 0; j < 8; j++ ) {
				assert( MOHAA_TerrainSquareMode( &table, &patch, i, j ) == 2 );
			}
		}
		/* each bit takes out its own triangle and no other; the variance
		 * and the other flags beside it take out none */
		for ( i = 0; i < 8; i++ ) {
			for ( j = 0; j < 8; j++ ) {
				for ( h = 0; h < 2; h++ ) {
					memset( patch.varTree, 0, sizeof( patch.varTree ) );
					for ( ii = 0; ii < MOHAA_TERRAIN_VARNODES; ii++ ) {
						patch.varTree[0][ii].flags = patch.varTree[1][ii].flags = 0xC7FF;
					}
					Bit( &patch, &table.vni[diag][i][j][h] );
					for ( ii = 0; ii < 8; ii++ ) {
						for ( jj = 0; jj < 8; jj++ ) {
							mode = MOHAA_TerrainSquareMode( &table, &patch, ii, jj );
							if ( ii == i && jj == j ) {
								assert( !MOHAA_TerrainSquareHasTriangle( mode, h ) );
								assert( MOHAA_TerrainSquareHasTriangle( mode, !h ) );
							} else {
								assert( mode == 2 );
							}
						}
					}
				}
				/* both: the square is gone */
				memset( patch.varTree, 0, sizeof( patch.varTree ) );
				Bit( &patch, &table.vni[diag][i][j][0] );
				Bit( &patch, &table.vni[diag][i][j][1] );
				assert( MOHAA_TerrainSquareMode( &table, &patch, i, j ) == 0 );
			}
		}
	}

	/* the modes as the collision model reads them */
	assert( !MOHAA_TerrainSquareHasTriangle( 0, 0 ) && !MOHAA_TerrainSquareHasTriangle( 0, 1 ) );
	for ( mode = 1; mode <= 2; mode++ ) {
		assert( MOHAA_TerrainSquareHasTriangle( mode, 0 ) && MOHAA_TerrainSquareHasTriangle( mode, 1 ) );
	}
	for ( mode = 3; mode <= 4; mode++ ) {
		assert( MOHAA_TerrainSquareHasTriangle( mode, 0 ) && !MOHAA_TerrainSquareHasTriangle( mode, 1 ) );
	}
	for ( mode = 5; mode <= 6; mode++ ) {
		assert( !MOHAA_TerrainSquareHasTriangle( mode, 0 ) && MOHAA_TerrainSquareHasTriangle( mode, 1 ) );
	}
	assert( !MOHAA_TerrainSquareHasTriangle( 7, 0 ) && !MOHAA_TerrainSquareHasTriangle( -1, 1 ) );
	puts( "squares ok" );
	return 0;
}
'''

TERRAIN_CHECKS = r'''
#include "code/qcommon/mohaa_terrain_squares.h"

/* The indexes of square (i, j)'s triangle `half`, sorted: the first of a
 * square whose i + j is odd holds its (i + 1, j + 1) corner, of an even one
 * its (i, j + 1) corner; the second the opposite corner. */
static void Triangle( int i, int j, int half, int out[3] ) {
	const int v1 = j * 9 + i, v2 = v1 + 1, v3 = v2 + 9, v4 = v1 + 9;
	int t;
	if ( ( i + j ) & 1 ) {
		out[0] = half ? v4 : v2; out[1] = half ? v2 : v4; out[2] = half ? v1 : v3;
	} else {
		out[0] = half ? v1 : v3; out[1] = half ? v3 : v1; out[2] = half ? v2 : v4;
	}
	if ( out[0] > out[1] ) { t = out[0]; out[0] = out[1]; out[1] = t; }
	if ( out[1] > out[2] ) { t = out[1]; out[1] = out[2]; out[2] = t; }
	if ( out[0] > out[1] ) { t = out[0]; out[0] = out[1]; out[1] = t; }
}

static int Has( const int *indexes, int count, const int want[3] ) {
	int k;
	for ( k = 0; k + 2 < count; k += 3 ) {
		int got[3], t;
		got[0] = indexes[k]; got[1] = indexes[k + 1]; got[2] = indexes[k + 2];
		if ( got[0] > got[1] ) { t = got[0]; got[0] = got[1]; got[1] = t; }
		if ( got[1] > got[2] ) { t = got[1]; got[1] = got[2]; got[2] = t; }
		if ( got[0] > got[1] ) { t = got[0]; got[0] = got[1]; got[1] = t; }
		if ( got[0] == want[0] && got[1] == want[1] && got[2] == want[2] ) {
			return 1;
		}
	}
	return 0;
}

int main( void ) {
	static dterrainPatchMOHAA_t patches[2];
	static const int32_t leafTerrain[4] = { 0, 2, 0, 0 };
	static const int16_t terrainIndexes[2] = { 0, 1 };
	mohaaTerrainSquareTable_t table;
	mohaa_translation_t translation;
	mohaaBuiltSurfaces_t built;
	void *data = NULL;
	int size = 0, n, i, j, h, want[3];
	const dsurface_t *surfaces;
	const drawVert_t *verts;
	const int *indexes, *leafSurfaces;
	const dleaf_t *leafs;

	ri.Printf = Printf;
	ri.Malloc = Malloc;
	ri.Free = Free;
	ri.FS_ReadFile = ReadFile;
	ri.FS_FreeFile = FreeFile;
	ri.Cvar_VariableIntegerValue = CvarInteger;
	BuildMap();
	MOHAA_BuildTerrainSquareTable( &table );

	/* the first patch loses square (2, 3)'s first triangle, (5, 5)'s second
	 * and the whole of (7, 0); the second, on the other root diagonal,
	 * loses every triangle */
	memset( patches, 0, sizeof( patches ) );
	patches[0].x = 2;
	patches[0].y = -1;
	patches[0].iBaseHeight = 100;
	for ( i = 0; i < 81; i++ ) {
		patches[0].heightmap[i] = (byte)i;
	}
	patches[0].varTree[table.vni[0][2][3][0].iTreeAndMask & 1][table.vni[0][2][3][0].iNode].flags |=
		table.vni[0][2][3][0].iTreeAndMask & 0xFFFE;
	patches[0].varTree[table.vni[0][5][5][1].iTreeAndMask & 1][table.vni[0][5][5][1].iNode].flags |=
		table.vni[0][5][5][1].iTreeAndMask & 0xFFFE;
	for ( h = 0; h < 2; h++ ) {
		patches[0].varTree[table.vni[0][7][0][h].iTreeAndMask & 1][table.vni[0][7][0][h].iNode].flags |=
			table.vni[0][7][0][h].iTreeAndMask & 0xFFFE;
	}
	patches[1].flags = MOHAA_TERPATCH_NEIGHBOR;
	patches[1].x = 10;
	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < MOHAA_TERRAIN_VARNODES; j++ ) {
			patches[1].varTree[i][j].flags = 0x3000;
		}
	}

	memset( &translation, 0, sizeof( translation ) );
	translation.data = map;
	translation.length = mapLength;
	translation.terrain = patches;
	translation.numTerrain = 2;
	translation.leafTerrain = leafTerrain;
	translation.numLeafs = 2;
	translation.terrainIndexes = terrainIndexes;
	translation.numTerrainIndexes = 2;
	assert( R_MOHAABuildWorld( &translation, "maps/fixture.bsp", &data, &size, &built ) );
	assert( built.firstTerrain == 0 && built.numTerrain == 2 && built.numDecals == 0 );

	surfaces = Out( data, LUMP_SURFACES, sizeof( dsurface_t ), &n );
	assert( n == 2 );
	/* each patch keeps its surface, however few triangles are left */
	assert( surfaces[0].numVerts == 81 && surfaces[0].numIndexes == ( 128 - 4 ) * 3 );
	assert( surfaces[1].numVerts == 81 && surfaces[1].numIndexes == 0 );
	assert( surfaces[1].firstIndex == surfaces[0].firstIndex + surfaces[0].numIndexes );
	indexes = Out( data, LUMP_DRAWINDEXES, sizeof( int ), &n );
	assert( n == ( 128 - 4 ) * 3 );
	for ( i = 0; i < 8; i++ ) {
		for ( j = 0; j < 8; j++ ) {
			for ( h = 0; h < 2; h++ ) {
				const int gone = ( i == 2 && j == 3 && h == 0 ) || ( i == 5 && j == 5 && h == 1 ) || ( i == 7 && j == 0 );
				Triangle( i, j, h, want );
				assert( Has( indexes, surfaces[0].numIndexes, want ) == !gone );
			}
		}
	}
	/* the points: 64 units apart from the patch's corner, two a step up */
	verts = Out( data, LUMP_DRAWVERTS, sizeof( drawVert_t ), &n );
	assert( n == 162 );
	assert( verts[10].xyz[0] == 128.0f + 64.0f && verts[10].xyz[1] == -64.0f + 64.0f && verts[10].xyz[2] == 120.0f );
	/* and both listed in the leaf the map names them in */
	leafs = Out( data, LUMP_LEAFS, sizeof( dleaf_t ), &n );
	leafSurfaces = Out( data, LUMP_LEAFSURFACES, sizeof( int ), &n );
	assert( leafs[0].numLeafSurfaces == 2 && leafSurfaces[leafs[0].firstLeafSurface] == 0 );
	assert( leafSurfaces[leafs[0].firstLeafSurface + 1] == 1 && leafs[1].numLeafSurfaces == 0 );
	free( data );
	puts( "terrain ok" );
	return 0;
}
'''

MARKS_HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/surfaceflags.h"
#include "code/qcommon/game_profile.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; exit( 3 ); }
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }

#define MAX_VERTS_ON_POLY		64
#define MARKER_OFFSET			0
#define VERTEXSIZE				8
#define CONTENTS_NODE			-1
#define SIDE_FRONT				0
#define SIDE_BACK				1
#define SIDE_ON					2

typedef enum { SF_BAD, SF_SKIP, SF_FACE, SF_GRID, SF_TRIANGLES } surfaceType_t;
typedef struct { int surfaceFlags, contentFlags; } shader_t;
typedef struct { vec3_t xyz; vec2_t st; vec2_t lightmap; vec3_t normal; } drawVert_t;
typedef struct {
	surfaceType_t surfaceType;
	cplane_t plane;
	int numPoints, numIndices, ofsIndices;
	float points[4][VERTEXSIZE];
	int indices[6];
} srfSurfaceFace_t;
typedef struct { surfaceType_t surfaceType; int width, height; drawVert_t verts[4]; } srfGridMesh_t;
typedef struct { surfaceType_t surfaceType; int numIndexes; int *indexes; int numVerts; drawVert_t *verts; } srfTriangles_t;
typedef struct { int viewCount; shader_t *shader; surfaceType_t *data; } msurface_t;
typedef struct mnode_s {
	int contents;
	cplane_t *plane;
	struct mnode_s *children[2];
	int nummarksurfaces;
	msurface_t **firstmarksurface;
} mnode_t;
typedef struct { vec3_t bounds[2]; msurface_t *firstSurface; int numSurfaces; } bmodel_t;
typedef struct {
	mnode_t *nodes;
	msurface_t *surfaces;
	int numBmodels;
	bmodel_t *bmodels;
	int mohaaFirstTerrain, mohaaNumTerrain;
} world_t;
static struct { world_t *world; int viewCount; } tr;
static struct { comGameId_t gameId; } ri = { COM_GAME_MOHAA };
static cvar_t minRadius;
static cvar_t *ter_minMarkRadius = &minRadius;
'''

MARKS_CHECKS = r'''
/* The world: a floor face at z = 0 and a wall face as deep as the terrain,
 * 100 units down (both 64 by 64 about the origin); a terrain patch there
 * too, two triangles, the built world's surface 1 and its only patch; a
 * static model's triangle just above the floor; each listed in two leafs
 * either side of x = 0. Brush model 1 is a face at z = 0 of its own. */
static shader_t plain, nomarks;
static srfSurfaceFace_t floorFace, deepFace, doorFace;
static srfTriangles_t terrain, statics;
static drawVert_t terrainVerts[4], staticVerts[3];
static int terrainIndexes[6] = { 0, 1, 2, 0, 2, 3 }, staticIndexes[3] = { 0, 1, 2 };
static msurface_t surfaces[5];
static msurface_t *marks[4];
static cplane_t split;
static mnode_t node, leafs[2];
static bmodel_t bmodels[2];
static world_t world;

static void Face( srfSurfaceFace_t *face, float z ) {
	static const float corners[4][2] = { { -32, -32 }, { -32, 32 }, { 32, 32 }, { 32, -32 } };
	int i;
	memset( face, 0, sizeof( *face ) );
	face->surfaceType = SF_FACE;
	face->plane.normal[2] = 1.0f;
	face->plane.dist = z;
	face->plane.type = PlaneTypeForNormal( face->plane.normal );
	SetPlaneSignbits( &face->plane );
	face->numPoints = 4;
	face->numIndices = 6;
	face->ofsIndices = (int)( (byte *)face->indices - (byte *)face );
	for ( i = 0; i < 4; i++ ) {
		face->points[i][0] = corners[i][0];
		face->points[i][1] = corners[i][1];
		face->points[i][2] = z;
	}
	face->indices[0] = 0; face->indices[1] = 1; face->indices[2] = 2;
	face->indices[3] = 0; face->indices[4] = 2; face->indices[5] = 3;
}

static void Setup( void ) {
	int i;
	Face( &floorFace, 0.0f );
	Face( &deepFace, -100.0f );
	Face( &doorFace, 0.0f );
	memset( &terrain, 0, sizeof( terrain ) );
	terrain.surfaceType = SF_TRIANGLES;
	terrain.numIndexes = 6;
	terrain.indexes = terrainIndexes;
	terrain.numVerts = 4;
	terrain.verts = terrainVerts;
	for ( i = 0; i < 4; i++ ) {
		VectorCopy( deepFace.points[i], terrainVerts[i].xyz );
	}
	statics = terrain;
	statics.numIndexes = 3;
	statics.indexes = staticIndexes;
	statics.numVerts = 3;
	statics.verts = staticVerts;
	for ( i = 0; i < 3; i++ ) {
		staticVerts[i] = terrainVerts[i];
		staticVerts[i].xyz[2] = 0.5f;
	}
	memset( surfaces, 0, sizeof( surfaces ) );
	surfaces[0].data = &floorFace.surfaceType;
	surfaces[1].data = &terrain.surfaceType;
	surfaces[2].data = &statics.surfaceType;
	surfaces[3].data = &deepFace.surfaceType;
	surfaces[4].data = &doorFace.surfaceType;
	for ( i = 0; i < 5; i++ ) {
		surfaces[i].shader = &plain;
		surfaces[i].viewCount = -1;
	}
	for ( i = 0; i < 4; i++ ) {
		marks[i] = &surfaces[i];
	}
	memset( &split, 0, sizeof( split ) );
	split.normal[0] = 1.0f;
	split.type = PlaneTypeForNormal( split.normal );
	SetPlaneSignbits( &split );
	memset( &node, 0, sizeof( node ) );
	memset( leafs, 0, sizeof( leafs ) );
	node.contents = CONTENTS_NODE;
	node.plane = &split;
	node.children[0] = &leafs[0];
	node.children[1] = &leafs[1];
	for ( i = 0; i < 2; i++ ) {
		leafs[i].nummarksurfaces = 4;
		leafs[i].firstmarksurface = marks;
	}
	bmodels[1].firstSurface = &surfaces[4];
	bmodels[1].numSurfaces = 1;
	world.nodes = &node;
	world.surfaces = surfaces;
	world.numBmodels = 2;
	world.bmodels = bmodels;
	world.mohaaFirstTerrain = 1;
	world.mohaaNumTerrain = 1;
	tr.world = &world;
	ter_minMarkRadius = &minRadius;
	minRadius.value = 8.0f;
	ri.gameId = COM_GAME_MOHAA;
}

/* A decal 8 units across about (x, y, 1), projected straight down, its
 * corners clockwise seen from above as a client game lays them. */
static int Cut( int model, const vec3_t angles, const vec3_t origin, float radiusSquared, float x, float y,
	vec3_t *points, markFragment_t *fragments, int *patches, int maxPoints, int maxFragments ) {
	vec3_t corners[4];
	const vec3_t projection = { 0.0f, 0.0f, -20.0f };
	int i;
	for ( i = 0; i < 4; i++ ) {
		corners[i][0] = x + ( i < 2 ? -4.0f : 4.0f );
		corners[i][1] = y + ( ( i == 1 || i == 2 ) ? 4.0f : -4.0f );
		corners[i][2] = 1.0f;
	}
	return R_MarkMOHAAFragments( model, angles, origin, 4, (const vec3_t *)corners, projection, maxPoints,
		points[0], maxFragments, fragments, patches, radiusSquared );
}

/* How many of `count` fragments lie at height z on `index`, each inside the
 * box (x0, y0)-(x1, y1). */
static int Count( const vec3_t *points, const markFragment_t *fragments, const int *patches, int count, float z,
	int index, float x0, float y0, float x1, float y1 ) {
	int i, k, n = 0;
	for ( i = 0; i < count; i++ ) {
		if ( points[fragments[i].firstPoint][2] != z || ( patches && patches[i] != index ) ) {
			continue;
		}
		assert( fragments[i].numPoints >= 3 );
		for ( k = 0; k < fragments[i].numPoints; k++ ) {
			const float *p = points[fragments[i].firstPoint + k];
			assert( p[2] == z );
			assert( p[0] >= x0 - 0.01f && p[0] <= x1 + 0.01f && p[1] >= y0 - 0.01f && p[1] <= y1 + 0.01f );
		}
		n++;
	}
	return n;
}

int main( void ) {
	vec3_t points[64];
	markFragment_t fragments[8];
	int patches[8], count;
	const vec3_t zero = { 0, 0, 0 }, turned = { 0, 90, 0 }, doorOrigin = { 100, 50, 0 };

	Setup();

	/* the floor's two triangles, and the terrain's 100 units down, within
	 * terrain's 128 though the wall face as deep lies past the world's 20;
	 * the static model's triangle takes none; the leafs either side of the
	 * split both hold every surface, and each is cut once */
	count = Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 64, 8 );
	assert( count == 4 );
	assert( Count( points, fragments, patches, count, 0.0f, 0, -4, -4, 4, 4 ) == 2 );
	assert( Count( points, fragments, patches, count, -100.0f, 1, -4, -4, 4, 4 ) == 2 );

	/* a decal no larger than ter_minMarkRadius leaves the terrain alone;
	 * without the variable, it does not */
	count = Cut( 0, zero, zero, 64.0f, 0.0f, 0.0f, points, fragments, patches, 64, 8 );
	assert( count == 2 && Count( points, fragments, patches, count, 0.0f, 0, -4, -4, 4, 4 ) == 2 );
	count = Cut( 0, zero, zero, 64.1f, 0.0f, 0.0f, points, fragments, patches, 64, 8 );
	assert( count == 4 );
	ter_minMarkRadius = NULL;
	count = Cut( 0, zero, zero, 8.0f, 0.0f, 0.0f, points, fragments, patches, 64, 8 );
	assert( count == 4 );
	ter_minMarkRadius = &minRadius;

	/* a surface that takes no marks is passed over; patches may be NULL */
	surfaces[0].shader = &nomarks;
	nomarks.surfaceFlags = SURF_NOMARKS;
	count = Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, NULL, 64, 8 );
	assert( count == 2 && Count( points, fragments, NULL, count, -100.0f, 1, -4, -4, 4, 4 ) == 2 );
	nomarks.surfaceFlags = 0;
	nomarks.contentFlags = CONTENTS_FOG;
	count = Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, NULL, 64, 8 );
	assert( count == 2 );
	surfaces[0].shader = &plain;

	/* the capacities hold */
	count = Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 64, 1 );
	assert( count == 1 );
	count = Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 2, 8 );
	assert( count == 0 );

	/* a brush model's face, in the model's own space: a decal 10 units
	 * along the world's x from a model turned 90 degrees lies 10 units to
	 * its right, within one of the face's two triangles, and the world's
	 * own surfaces are not cut */
	count = Cut( 1, turned, doorOrigin, 800.0f, 110.0f, 50.0f, points, fragments, patches, 64, 8 );
	assert( count == 1 && Count( points, fragments, patches, count, 0.0f, 0, -4, -14, 4, -6 ) == 1 );
	count = Cut( 1, zero, doorOrigin, 800.0f, 110.0f, 50.0f, points, fragments, patches, 64, 8 );
	assert( count == 1 && Count( points, fragments, patches, count, 0.0f, 0, 6, -4, 14, 4 ) == 1 );

	/* what is refused */
	assert( Cut( 2, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 64, 8 ) == 0 );
	assert( Cut( -1, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 64, 8 ) == 0 );
	assert( Cut( 1, NULL, doorOrigin, 800.0f, 110.0f, 50.0f, points, fragments, patches, 64, 8 ) == 0 );
	assert( Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, NULL, patches, 64, 8 ) == 0 );
	assert( Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 0, 8 ) == 0 );
	assert( Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 64, 0 ) == 0 );
	{
		const vec3_t line[2] = { { 0, 0, 1 }, { 4, 0, 1 } }, down = { 0, 0, -20 }, flat = { 0, 0, 0 };
		assert( R_MarkMOHAAFragments( 0, zero, zero, 2, line, down, 64, points[0], 8, fragments, patches, 800.0f ) == 0 );
		{
			vec3_t corners[4] = { { -4, -4, 1 }, { -4, 4, 1 }, { 4, 4, 1 }, { 4, -4, 1 } };
			assert( R_MarkMOHAAFragments( 0, zero, zero, 4, (const vec3_t *)corners, flat, 64, points[0], 8,
				fragments, patches, 800.0f ) == 0 );
		}
	}
	ri.gameId = COM_GAME_QUAKE3;
	assert( Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 64, 8 ) == 0 );
	ri.gameId = COM_GAME_MOHAA;
	tr.world = NULL;
	assert( Cut( 0, zero, zero, 800.0f, 0.0f, 0.0f, points, fragments, patches, 64, 8 ) == 0 );
	tr.world = &world;
	puts( "marks ok" );
	return 0;
}
'''


def marks_source(checks):
    marks = read('code/renderer/tr_marks.c')
    local = read('code/renderer/tr_local.h')
    source = MARKS_HOST
    source += function(local, 'static ID_INLINE int R_MOHAATerrainPatch(')
    source += function(marks, 'static void R_ChopPolyBehindPlane(')
    source += function(marks, 'static void R_AddMarkFragments(')
    source += '#include "code/renderercommon/tr_mohaa_marks_surface.h"\n'
    return source + checks


class MohaaTerrainMarksTests(unittest.TestCase):
    def test_each_bit_of_the_variance_tree_takes_out_one_triangle(self):
        output = build_and_run(self, 'squares', [
            SQUARES,
            ROOT / 'code/qcommon/q_shared.c',
            ROOT / 'code/qcommon/q_math.c',
        ])
        self.assertIn('squares ok', output)

    def test_the_built_terrain_leaves_out_what_the_collision_model_does(self):
        output = build_and_run(self, 'terrain', [
            BUILD_FIXTURE + TERRAIN_CHECKS,
            ROOT / 'code/renderercommon/tr_mohaa_world.c',
            ROOT / 'code/qcommon/q_shared.c',
            ROOT / 'code/qcommon/q_math.c',
        ])
        self.assertIn('terrain ok', output)

    def test_decals_cut_the_world_its_terrain_and_brush_models(self):
        output = build_and_run(self, 'marks', [
            marks_source(MARKS_CHECKS),
            ROOT / 'code/qcommon/q_shared.c',
            ROOT / 'code/qcommon/q_math.c',
        ])
        self.assertIn('marks ok', output)

    def test_one_reading_of_the_variance_tree_serves_collision_and_drawing(self):
        collision = read('code/qcommon/cm_mohaa_terrain.c')
        self.assertIn('#include "mohaa_terrain_squares.h"', collision)
        self.assertIn('MOHAA_BuildTerrainSquareTable( &g_squareTable );', collision)
        self.assertIn('square->eMode = MOHAA_TerrainSquareMode( &g_squareTable, patch, i, j );', collision)
        for private in ('modeTable', 'g_vni', 'CM_MOHAA_CalculateTerrainIndices', 'varnodeIndex_t'):
            self.assertNotIn(private, collision)
        world = read('code/renderercommon/tr_mohaa_world.c')
        build = function(world, 'static int R_MOHAABuildTerrain(')
        self.assertIn('const int mode = MOHAA_TerrainSquareMode( squares, patch, i, j );', build)
        self.assertIn('if ( !MOHAA_TerrainSquareHasTriangle( mode, t ) ) {\n\t\t\t\t\tcontinue;', build)
        self.assertIn('return n;', build)
        whole = function(world, 'qboolean R_MOHAABuildWorld(')
        self.assertIn('count = R_MOHAABuildTerrain( patch, &squares, verts + vertCursor, indexes + indexCursor );',
                      whole)
        self.assertIn('vertCursor, TERRAIN_VERTS, indexCursor, count );', whole)
        self.assertIn('built->firstTerrain = terrainFirstSurface;', whole)
        self.assertIn('built->numTerrain = translation->numTerrain;', whole)

    def test_every_renderer_serves_the_mark_cutter_and_its_terrain_range(self):
        public = read('code/renderercommon/tr_public.h')
        exports = public[:public.index('} refexport_t;')]
        self.assertIn('int (*MarkMOHAAFragments)( int model, const vec3_t angles, const vec3_t origin, '
                      'int numPoints,', exports)
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                marks = read(f'code/{renderer}/tr_marks.c')
                self.assertLess(marks.index('#include "../renderercommon/tr_rtcw_marks_surface.h"'),
                                marks.index('#include "../renderercommon/tr_mohaa_marks_surface.h"'))
                init = read(f'code/{renderer}/tr_init.c')
                self.assertIn('re.MarkMOHAAFragments = R_MarkMOHAAFragments;', init)
                mohaa = init[init.index('} else if ( ri.gameId == COM_GAME_MOHAA ) {'):]
                mohaa = mohaa[:mohaa.index('\n\t}')]
                self.assertIn('ter_minMarkRadius = ri.Cvar_Get( "ter_minMarkRadius", "8", CVAR_ARCHIVE );', mohaa)
                self.assertEqual(init.count('ri.Cvar_Get( "ter_minMarkRadius"'), 1)
                local = read(f'code/{renderer}/tr_local.h')
                self.assertIn('int\t\t\tmohaaFirstTerrain;', local)
                self.assertIn('static ID_INLINE int R_MOHAATerrainPatch( const world_t *world, '
                              'const msurface_t *surf ) {', local)
                self.assertIn('int R_MarkMOHAAFragments( int model,', local)
                bsp = read(f'code/{renderer}/tr_bsp.c')
                load = function(bsp, 'void RE_LoadWorldMap( const char *name ) {')
                self.assertLess(load.index('Com_Memset( &s_worldData, 0, sizeof( s_worldData ) );'),
                                load.index('s_worldData.mohaaNumTerrain = mohaaBuilt.numTerrain;'))
                self.assertIn('s_worldData.mohaaFirstTerrain = mohaaBuilt.firstTerrain;', load)

    def test_the_client_games_mark_imports_are_served_by_the_cutter(self):
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        cut = function(cgame, 'int CutFragments(')
        self.assertIn('re.MarkMOHAAFragments( model, angles, origin, numPoints, points, projection, maxPoints,', cut)
        self.assertIn('fragmentBuffer[i].iIndex = patches[i];', cut)
        world = function(cgame, 'int QDECL MarkFragments(')
        self.assertIn('return CutFragments( 0, vec3_origin, vec3_origin,', world)
        inline = function(cgame, 'int QDECL MarkFragmentsForInlineModel(')
        self.assertIn('if ( model <= 0 || model >= CM_NumInlineModels() || !angles || !origin ) {', inline)
        self.assertIn('return CutFragments( model, angles, origin,', inline)
        # both are served in their slots
        self.assertIn('SERVE( R_MarkFragments, MarkFragments );', cgame)
        self.assertIn('SERVE( R_MarkFragmentsForInlineModel, MarkFragmentsForInlineModel );', cgame)
        imports = read('code/compat/mohaa_cgame_imports.def')
        self.assertRegex(imports, r'FNTECH3_MOHAA_CGAME_IMPORT\(\s+49, R_MarkFragments,\s+8,')
        self.assertRegex(imports, r'FNTECH3_MOHAA_CGAME_IMPORT\(\s+50, R_MarkFragmentsForInlineModel, 11,')
        self.assertRegex(imports, r'FNTECH3_MOHAA_CGAME_IMPORT\(\s+92, R_AddTerrainMarkToScene,\s+5,')


if __name__ == '__main__':
    unittest.main()
