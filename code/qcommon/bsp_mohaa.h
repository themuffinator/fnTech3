/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

"2015" version 19 compatibility

Medal of Honor: Allied Assault 1.00's fifty-four maps are 2015's own format:
ident "2015", version 19, a checksum after the version as Ritual's maps have,
and 28 lumps. What follows was measured on all fifty-four; every lump divides
into its record size below and the directory tiles each file up to an
84-byte table of counts that no lump names.

Most collision records are Quake III's own, byte for byte: planes, nodes,
leaf brushes, leaf surfaces, brushes, models, draw vertices and indexes, the
entity text and the visibility lump. Four differ:

  shader      140 bytes: Quake III's 72, a subdivision count, and the name of
              a fence mask image the collision model consults for a point
              trace through a fence brush
  surface     108 bytes: Quake III's 104 and a subdivision count, as Ritual's
  leaf        64 bytes: Quake III's 48 and two ranges, one into the terrain
              index lump and one into the static model index lump
  brush side  12 bytes: Quake III's 8 and an index into the side equations,
              which map a fence brush's side onto its mask

and eight lumps have no Quake III counterpart at all: the side equations,
the terrain patches and their per-leaf indexes, the sphere lights and their
visibility, the per-surface light definitions, and the static models with
their vertex colours and indexes. The light grid is a palette, per-cell
offsets and packed data rather than Quake III's eight bytes a point.

This translator presents a version 19 map to the existing collision loader
as the IBSP 46 layout it reads, and carries what collision needs beyond it --
each shader's subdivision count, each leaf's terrain range, each brush side's
equation, the terrain patches and the terrain indexes -- after the last lump,
where an IBSP 46 reader never looks. The static models -- their
definitions, per-leaf ranges, indexes and vertex colours -- are carried the
same way for the renderers, which build them into the world
(code/renderercommon/tr_mohaa_world.c). So are the light grid and the
sphere lights, which the renderers fold into a Quake III light grid there.
The light definitions are dropped.

Format reference: the lump order and records follow OpenMoHAA's qfiles.h
(GPL-2.0-or-later), and every size was re-measured against the retail maps;
see docs/fntech3/2026-09-24-mohaa-maps.md. No implementation text is taken
from it.
===========================================================================
*/
#ifndef __BSP_MOHAA_H__
#define __BSP_MOHAA_H__

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "qfiles.h"

// little-endian "2015"
#define MOHAA_BSP_IDENT			( ( '5' << 24 ) + ( '1' << 16 ) + ( '0' << 8 ) + '2' )
#define MOHAA_BSP_VERSION		19

#define LUMP_MOHAA_SHADERS				0
#define LUMP_MOHAA_PLANES				1
#define LUMP_MOHAA_LIGHTMAPS			2
#define LUMP_MOHAA_SURFACES				3
#define LUMP_MOHAA_DRAWVERTS			4
#define LUMP_MOHAA_DRAWINDEXES			5
#define LUMP_MOHAA_LEAFBRUSHES			6
#define LUMP_MOHAA_LEAFSURFACES			7
#define LUMP_MOHAA_LEAFS				8
#define LUMP_MOHAA_NODES				9
#define LUMP_MOHAA_SIDEEQUATIONS		10
#define LUMP_MOHAA_BRUSHSIDES			11
#define LUMP_MOHAA_BRUSHES				12
#define LUMP_MOHAA_MODELS				13
#define LUMP_MOHAA_ENTITIES				14
#define LUMP_MOHAA_VISIBILITY			15
#define LUMP_MOHAA_LIGHTGRIDPALETTE		16
#define LUMP_MOHAA_LIGHTGRIDOFFSETS		17
#define LUMP_MOHAA_LIGHTGRIDDATA		18
#define LUMP_MOHAA_SPHERELIGHTS			19
#define LUMP_MOHAA_SPHERELIGHTVIS		20
#define LUMP_MOHAA_LIGHTDEFS			21
#define LUMP_MOHAA_TERRAIN				22
#define LUMP_MOHAA_TERRAININDEXES		23
#define LUMP_MOHAA_STATICMODELDATA		24
#define LUMP_MOHAA_STATICMODELDEF		25
#define LUMP_MOHAA_STATICMODELINDEXES	26
#define LUMP_MOHAA_DUMMY				27
#define MOHAA_HEADER_LUMPS				28

/* A terrain patch covers 8 by 8 squares of 64 units: 512 by 512 from its
 * corner at (x, y) * 64, with a 9 by 9 heightmap of byte steps of two units
 * above its base height. */
#define MOHAA_TERRAIN_SQUARES		8
#define MOHAA_TERRAIN_SQUARE_SIZE	64
#define MOHAA_TERRAIN_VARNODES		63
#define MOHAA_TERPATCH_FLIP			0x40
#define MOHAA_TERPATCH_NEIGHBOR		0x80

typedef struct {
	int32_t		ident;
	int32_t		version;
	int32_t		checksum;

	lump_t		lumps[MOHAA_HEADER_LUMPS];
} dheaderMOHAA_t;

typedef struct {
	char		shader[MAX_QPATH];
	int32_t		surfaceFlags;
	int32_t		contentFlags;
	int32_t		subdivisions;
	char		fenceMaskImage[MAX_QPATH];
} dshaderMOHAA_t;

typedef struct {
	int32_t		shaderNum;
	int32_t		fogNum;
	int32_t		surfaceType;

	int32_t		firstVert;
	int32_t		numVerts;

	int32_t		firstIndex;
	int32_t		numIndexes;

	int32_t		lightmapNum;
	int32_t		lightmapX, lightmapY;
	int32_t		lightmapWidth, lightmapHeight;

	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[3];

	int32_t		patchWidth;
	int32_t		patchHeight;

	float		subdivisions;
} dsurfaceMOHAA_t;

typedef struct {
	int32_t		cluster;
	int32_t		area;

	int32_t		mins[3];
	int32_t		maxs[3];

	int32_t		firstLeafSurface;
	int32_t		numLeafSurfaces;

	int32_t		firstLeafBrush;
	int32_t		numLeafBrushes;

	int32_t		firstTerraPatch;
	int32_t		numTerraPatches;

	int32_t		firstStaticModel;
	int32_t		numStaticModels;
} dleafMOHAA_t;

typedef struct {
	int32_t		planeNum;
	int32_t		shaderNum;
	int32_t		equationNum;
} dbrushsideMOHAA_t;

/* A fence brush side's texture mapping: s and t are dot products of a hit
 * point with these planes, so a point trace can find its mask pixel. */
typedef struct {
	float		fSeq[4];
	float		fTeq[4];
} dsideEquationMOHAA_t;

typedef struct {
	uint16_t	flags;
} dvarnodeMOHAA_t;

typedef struct {
	uint8_t			flags;
	uint8_t			lmapScale;
	uint8_t			s;
	uint8_t			t;

	float			texCoord[2][2][2];

	int8_t			x;
	int8_t			y;

	int16_t			iBaseHeight;
	uint16_t		iShader;
	uint16_t		iLightMap;

	int16_t			iNorth;
	int16_t			iEast;
	int16_t			iSouth;
	int16_t			iWest;

	dvarnodeMOHAA_t	varTree[2][MOHAA_TERRAIN_VARNODES];

	uint8_t			heightmap[9 * 9];
} dterrainPatchMOHAA_t;

/* A static model: a TIKI definition placed in the world by the map, with the
 * vertex colours the map compiler lit it with, three bytes a vertex from
 * `firstVertexData` bytes into the static model data lump, for its
 * `numVertexData` vertices in the order its surfaces list them. */
typedef struct {
	char		model[128];
	float		origin[3];
	float		angles[3];
	float		scale;
	int32_t		firstVertexData;
	int32_t		numVertexData;
} dstaticModelMOHAA_t;

typedef char mohaa_shader_size_check[sizeof( dshaderMOHAA_t ) == 140 ? 1 : -1];
typedef char mohaa_surface_size_check[sizeof( dsurfaceMOHAA_t ) == 108 ? 1 : -1];
typedef char mohaa_leaf_size_check[sizeof( dleafMOHAA_t ) == 64 ? 1 : -1];
typedef char mohaa_brushside_size_check[sizeof( dbrushsideMOHAA_t ) == 12 ? 1 : -1];
typedef char mohaa_equation_size_check[sizeof( dsideEquationMOHAA_t ) == 32 ? 1 : -1];
typedef char mohaa_terrain_size_check[sizeof( dterrainPatchMOHAA_t ) == 388 ? 1 : -1];
typedef char mohaa_static_model_size_check[sizeof( dstaticModelMOHAA_t ) == 164 ? 1 : -1];

/* A light the map compiler kept for lighting models: a point light, or a
 * spot light whose cone widens by `spotRadiusByDistance` units a unit along
 * `spotDir`. A negative intensity lights only invisible entities. The leaf is
 * the one the light is in (OpenMoHAA qcommon/qfiles.h mapspherel_t,
 * reference only). */
typedef struct {
	float		origin[3];
	float		color[3];
	float		intensity;
	int32_t		leaf;
	int32_t		needsTrace;
	int32_t		spotLight;
	float		spotDir[3];
	float		spotRadiusByDistance;
} dsphereLightMOHAA_t;

typedef char mohaa_sphere_light_size_check[sizeof( dsphereLightMOHAA_t ) == 56 ? 1 : -1];

typedef struct {
	void	*data;
	int		length;

	/* Everything below lives inside data, past the last lump, and is freed
	 * with it. Each is in the map's own order and little-endian as stored. */
	const int32_t				*shaderSubdivisions;	/* numShaders */
	int							numShaders;
	const int32_t				*leafTerrain;			/* numLeafs pairs: first, count */
	int							numLeafs;
	const int32_t				*brushSideEquations;	/* numBrushSides; -1 for none */
	int							numBrushSides;
	const dsideEquationMOHAA_t	*sideEquations;
	int							numSideEquations;
	const dterrainPatchMOHAA_t	*terrain;
	int							numTerrain;
	const int16_t				*terrainIndexes;
	int							numTerrainIndexes;
	/* The static models, which only the renderers draw: each leaf's range of
	 * the index lump, the definitions, the indexes and the vertex colours. */
	const int32_t				*leafStaticModels;		/* numLeafs pairs: first, count */
	const dstaticModelMOHAA_t	*staticModels;
	int							numStaticModels;
	const uint16_t				*staticModelIndexes;
	int							numStaticModelIndexes;
	const byte					*staticModelData;
	int							staticModelDataLength;
	/* The light grid and the sphere lights, which only the renderers read:
	 * the grid's 256-colour palette, its column offsets and run-length column
	 * data, the lights, and for each leaf in turn the lights that reach it,
	 * -2 first for the sun, each list ending in -1. */
	const byte					*lightGridPalette;		/* 768 bytes, or NULL */
	const uint16_t				*lightGridOffsets;
	int							numLightGridOffsets;
	const byte					*lightGridData;
	int							lightGridDataLength;
	const dsphereLightMOHAA_t	*sphereLights;
	int							numSphereLights;
	const int32_t				*sphereLightVis;
	int							numSphereLightVis;
} mohaa_translation_t;

static void MOHAA_SetError( char *error, const size_t errorSize, const char *message )
{
	if ( !error || !errorSize ) {
		return;
	}
	Q_strncpyz( error, message, (int)errorSize );
}

static qboolean MOHAA_LumpValid( const lump_t *lump, const int inputLength )
{
	const int32_t offset = LittleLong( lump->fileofs );
	const int32_t length = LittleLong( lump->filelen );

	if ( offset < 0 || length < 0 || offset > inputLength || length > inputLength - offset ) {
		return qfalse;
	}
	if ( length && offset < (int)sizeof( dheaderMOHAA_t ) ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean MOHAA_LumpCount( const lump_t *lump, const int recordSize, int *count )
{
	const int32_t length = LittleLong( lump->filelen );

	if ( recordSize <= 0 || ( length % recordSize ) != 0 ) {
		return qfalse;
	}
	*count = length / recordSize;
	return qtrue;
}

static qboolean MOHAA_AppendLump( int *length, const int bytes, int *offset )
{
	int aligned;

	if ( !length || !offset || *length < 0 || bytes < 0 || *length > INT_MAX - 3 ) {
		return qfalse;
	}
	aligned = ( *length + 3 ) & ~3;
	if ( bytes > INT_MAX - aligned ) {
		return qfalse;
	}
	*offset = aligned;
	*length = aligned + bytes;
	return qtrue;
}

/*
==================
MOHAA_TranslateToV46

Present a "2015" 19 map to an IBSP 46 loader, with what collision needs
beyond Quake III's records carried after the last lump. The caller owns the
returned buffer through freeFn. Every read is bounded by the validated lump
directory; no index inside a record is followed here, because the loaders
validate those themselves.
==================
*/
static qboolean MOHAA_TranslateToV46(
	const void *inputData,
	const int inputLength,
	void *( *allocFn )( int ),
	void ( *freeFn )( void * ),
	mohaa_translation_t *translation,
	char *error,
	const size_t errorSize )
{
	/* Where each lump lands in the Quake III directory; -1 drops it or, for
	 * the four records that change shape and the extras, handles it below. */
	static const int mohaaLumpTargets[MOHAA_HEADER_LUMPS] = {
		-1,					/* 0  shaders: reshaped */
		LUMP_PLANES,		/* 1  planes */
		LUMP_LIGHTMAPS,		/* 2  lightmaps */
		-1,					/* 3  surfaces: reshaped */
		LUMP_DRAWVERTS,		/* 4  draw vertices */
		LUMP_DRAWINDEXES,	/* 5  draw indexes */
		LUMP_LEAFBRUSHES,	/* 6  leaf brushes */
		LUMP_LEAFSURFACES,	/* 7  leaf surfaces */
		-1,					/* 8  leafs: reshaped */
		LUMP_NODES,			/* 9  nodes */
		-1,					/* 10 side equations: carried */
		-1,					/* 11 brush sides: reshaped */
		LUMP_BRUSHES,		/* 12 brushes */
		LUMP_MODELS,		/* 13 models */
		LUMP_ENTITIES,		/* 14 entity text */
		LUMP_VISIBILITY,	/* 15 visibility */
		-1, -1, -1,			/* 16-18 light grid */
		-1, -1,				/* 19-20 sphere lights */
		-1,					/* 21 light definitions */
		-1, -1,				/* 22-23 terrain: carried */
		-1, -1, -1,			/* 24-26 static models */
		-1					/* 27 */
	};

	dheaderMOHAA_t	source;
	const lump_t	*lumps;
	const byte		*input = (const byte *)inputData;
	byte			*output = NULL;
	dheader_t		*header;
	int				offsets[HEADER_LUMPS];
	int				lengths[HEADER_LUMPS];
	int				shaderCount, surfaceCount, leafCount, sideCount;
	int				equationCount, terrainCount, terrainIndexCount;
	int				outputLength;
	int				subdivisionsOffset, leafTerrainOffset, sideEquationOffset;
	int				equationsOffset, terrainOffset, terrainIndexesOffset;
	int				staticCount, staticIndexCount, staticDataLength;
	int				leafStaticOffset, staticOffset, staticIndexesOffset, staticDataOffset;
	int				gridOffsetCount, sphereLightCount, sphereVisCount;
	int				paletteOffset, gridOffsetsOffset, gridDataOffset, sphereLightsOffset, sphereVisOffset;
	int				i;

	if ( !translation ) {
		MOHAA_SetError( error, errorSize, "2015 translation target is missing" );
		return qfalse;
	}
	Com_Memset( translation, 0, sizeof( *translation ) );
	MOHAA_SetError( error, errorSize, "" );

	if ( !allocFn || !freeFn ) {
		MOHAA_SetError( error, errorSize, "2015 translation needs an allocator" );
		return qfalse;
	}
	if ( !inputData || inputLength < (int)sizeof( dheaderMOHAA_t ) ) {
		MOHAA_SetError( error, errorSize, "2015 header is truncated" );
		return qfalse;
	}

	memcpy( &source, inputData, sizeof( source ) );
	if ( LittleLong( source.ident ) != MOHAA_BSP_IDENT || LittleLong( source.version ) != MOHAA_BSP_VERSION ) {
		MOHAA_SetError( error, errorSize, "2015 translator received the wrong map format" );
		return qfalse;
	}

	lumps = source.lumps;
	for ( i = 0; i < MOHAA_HEADER_LUMPS; i++ ) {
		if ( !MOHAA_LumpValid( &lumps[i], inputLength ) ) {
			MOHAA_SetError( error, errorSize, "2015 map has an invalid lump directory" );
			return qfalse;
		}
	}

	if ( !MOHAA_LumpCount( &lumps[LUMP_MOHAA_SHADERS], (int)sizeof( dshaderMOHAA_t ), &shaderCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_SURFACES], (int)sizeof( dsurfaceMOHAA_t ), &surfaceCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_LEAFS], (int)sizeof( dleafMOHAA_t ), &leafCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_BRUSHSIDES], (int)sizeof( dbrushsideMOHAA_t ), &sideCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_SIDEEQUATIONS], (int)sizeof( dsideEquationMOHAA_t ), &equationCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_TERRAIN], (int)sizeof( dterrainPatchMOHAA_t ), &terrainCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_TERRAININDEXES], (int)sizeof( int16_t ), &terrainIndexCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_STATICMODELDEF], (int)sizeof( dstaticModelMOHAA_t ), &staticCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_STATICMODELINDEXES], (int)sizeof( uint16_t ), &staticIndexCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_LIGHTGRIDOFFSETS], (int)sizeof( uint16_t ), &gridOffsetCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_SPHERELIGHTS], (int)sizeof( dsphereLightMOHAA_t ), &sphereLightCount )
		|| !MOHAA_LumpCount( &lumps[LUMP_MOHAA_SPHERELIGHTVIS], (int)sizeof( int32_t ), &sphereVisCount ) ) {
		MOHAA_SetError( error, errorSize, "2015 lump size is not a whole number of records" );
		return qfalse;
	}

	Com_Memset( offsets, 0, sizeof( offsets ) );
	Com_Memset( lengths, 0, sizeof( lengths ) );
	for ( i = 0; i < MOHAA_HEADER_LUMPS; i++ ) {
		if ( mohaaLumpTargets[i] >= 0 ) {
			lengths[mohaaLumpTargets[i]] = LittleLong( lumps[i].filelen );
		}
	}
	lengths[LUMP_SHADERS] = shaderCount * (int)sizeof( dshader_t );
	lengths[LUMP_SURFACES] = surfaceCount * (int)sizeof( dsurface_t );
	lengths[LUMP_LEAFS] = leafCount * (int)sizeof( dleaf_t );
	lengths[LUMP_BRUSHSIDES] = sideCount * (int)sizeof( dbrushside_t );

	outputLength = (int)sizeof( dheader_t );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		if ( !MOHAA_AppendLump( &outputLength, lengths[i], &offsets[i] ) ) {
			MOHAA_SetError( error, errorSize, "2015 translation exceeds the addressable map size" );
			return qfalse;
		}
	}
	/* The extras follow the last lump. Every count is bounded by the input's
	 * length over a record at least as large as what is written per entry,
	 * so no product here can overflow. */
	if ( !MOHAA_AppendLump( &outputLength, shaderCount * (int)sizeof( int32_t ), &subdivisionsOffset )
		|| !MOHAA_AppendLump( &outputLength, leafCount * 2 * (int)sizeof( int32_t ), &leafTerrainOffset )
		|| !MOHAA_AppendLump( &outputLength, sideCount * (int)sizeof( int32_t ), &sideEquationOffset )
		|| !MOHAA_AppendLump( &outputLength, equationCount * (int)sizeof( dsideEquationMOHAA_t ), &equationsOffset )
		|| !MOHAA_AppendLump( &outputLength, terrainCount * (int)sizeof( dterrainPatchMOHAA_t ), &terrainOffset )
		|| !MOHAA_AppendLump( &outputLength, terrainIndexCount * (int)sizeof( int16_t ), &terrainIndexesOffset )
		|| !MOHAA_AppendLump( &outputLength, leafCount * 2 * (int)sizeof( int32_t ), &leafStaticOffset )
		|| !MOHAA_AppendLump( &outputLength, staticCount * (int)sizeof( dstaticModelMOHAA_t ), &staticOffset )
		|| !MOHAA_AppendLump( &outputLength, staticIndexCount * (int)sizeof( uint16_t ), &staticIndexesOffset )
		|| !MOHAA_AppendLump( &outputLength, LittleLong( lumps[LUMP_MOHAA_STATICMODELDATA].filelen ), &staticDataOffset )
		|| !MOHAA_AppendLump( &outputLength, LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDPALETTE].filelen ), &paletteOffset )
		|| !MOHAA_AppendLump( &outputLength, gridOffsetCount * (int)sizeof( uint16_t ), &gridOffsetsOffset )
		|| !MOHAA_AppendLump( &outputLength, LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDDATA].filelen ), &gridDataOffset )
		|| !MOHAA_AppendLump( &outputLength, sphereLightCount * (int)sizeof( dsphereLightMOHAA_t ), &sphereLightsOffset )
		|| !MOHAA_AppendLump( &outputLength, sphereVisCount * (int)sizeof( int32_t ), &sphereVisOffset ) ) {
		MOHAA_SetError( error, errorSize, "2015 translation exceeds the addressable map size" );
		return qfalse;
	}

	output = (byte *)allocFn( outputLength );
	if ( !output ) {
		MOHAA_SetError( error, errorSize, "2015 translation could not allocate its map" );
		return qfalse;
	}
	Com_Memset( output, 0, outputLength );

	header = (dheader_t *)output;
	header->ident = LittleLong( BSP_IDENT );
	header->version = LittleLong( BSP_VERSION );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		header->lumps[i].fileofs = LittleLong( offsets[i] );
		header->lumps[i].filelen = LittleLong( lengths[i] );
	}

	for ( i = 0; i < MOHAA_HEADER_LUMPS; i++ ) {
		const int target = mohaaLumpTargets[i];

		if ( target >= 0 && lengths[target] ) {
			memcpy( output + offsets[target], input + LittleLong( lumps[i].fileofs ), lengths[target] );
		}
	}

	{
		const dshaderMOHAA_t *in = (const dshaderMOHAA_t *)( input + LittleLong( lumps[LUMP_MOHAA_SHADERS].fileofs ) );
		dshader_t *out = (dshader_t *)( output + offsets[LUMP_SHADERS] );
		int32_t *subdivisions = (int32_t *)( output + subdivisionsOffset );

		for ( i = 0; i < shaderCount; i++ ) {
			memcpy( out[i].shader, in[i].shader, sizeof( out[i].shader ) );
			out[i].surfaceFlags = in[i].surfaceFlags;
			out[i].contentFlags = in[i].contentFlags;
			subdivisions[i] = LittleLong( in[i].subdivisions );
		}
		translation->shaderSubdivisions = subdivisions;
		translation->numShaders = shaderCount;
	}

	{
		const dsurfaceMOHAA_t *in = (const dsurfaceMOHAA_t *)( input + LittleLong( lumps[LUMP_MOHAA_SURFACES].fileofs ) );
		dsurface_t *out = (dsurface_t *)( output + offsets[LUMP_SURFACES] );

		/* Quake III's 104 bytes are the first 104 of 2015's 108. */
		for ( i = 0; i < surfaceCount; i++ ) {
			memcpy( &out[i], &in[i], sizeof( out[i] ) );
		}
	}

	{
		const dleafMOHAA_t *in = (const dleafMOHAA_t *)( input + LittleLong( lumps[LUMP_MOHAA_LEAFS].fileofs ) );
		dleaf_t *out = (dleaf_t *)( output + offsets[LUMP_LEAFS] );
		int32_t *terrain = (int32_t *)( output + leafTerrainOffset );

		/* Quake III's 48 bytes are the first 48 of 2015's 64. */
		for ( i = 0; i < leafCount; i++ ) {
			memcpy( &out[i], &in[i], sizeof( out[i] ) );
			terrain[i * 2] = LittleLong( in[i].firstTerraPatch );
			terrain[i * 2 + 1] = LittleLong( in[i].numTerraPatches );
		}
		translation->leafTerrain = terrain;
		translation->numLeafs = leafCount;
	}

	{
		const dbrushsideMOHAA_t *in = (const dbrushsideMOHAA_t *)( input + LittleLong( lumps[LUMP_MOHAA_BRUSHSIDES].fileofs ) );
		dbrushside_t *out = (dbrushside_t *)( output + offsets[LUMP_BRUSHSIDES] );
		int32_t *equations = (int32_t *)( output + sideEquationOffset );

		for ( i = 0; i < sideCount; i++ ) {
			out[i].planeNum = in[i].planeNum;
			out[i].shaderNum = in[i].shaderNum;
			equations[i] = LittleLong( in[i].equationNum );
		}
		translation->brushSideEquations = equations;
		translation->numBrushSides = sideCount;
	}

	if ( equationCount ) {
		memcpy( output + equationsOffset, input + LittleLong( lumps[LUMP_MOHAA_SIDEEQUATIONS].fileofs ),
			equationCount * sizeof( dsideEquationMOHAA_t ) );
	}
	translation->sideEquations = (const dsideEquationMOHAA_t *)( output + equationsOffset );
	translation->numSideEquations = equationCount;

	if ( terrainCount ) {
		memcpy( output + terrainOffset, input + LittleLong( lumps[LUMP_MOHAA_TERRAIN].fileofs ),
			terrainCount * sizeof( dterrainPatchMOHAA_t ) );
	}
	translation->terrain = (const dterrainPatchMOHAA_t *)( output + terrainOffset );
	translation->numTerrain = terrainCount;

	if ( terrainIndexCount ) {
		memcpy( output + terrainIndexesOffset, input + LittleLong( lumps[LUMP_MOHAA_TERRAININDEXES].fileofs ),
			terrainIndexCount * sizeof( int16_t ) );
	}
	translation->terrainIndexes = (const int16_t *)( output + terrainIndexesOffset );
	translation->numTerrainIndexes = terrainIndexCount;

	{
		const dleafMOHAA_t *in = (const dleafMOHAA_t *)( input + LittleLong( lumps[LUMP_MOHAA_LEAFS].fileofs ) );
		int32_t *ranges = (int32_t *)( output + leafStaticOffset );

		for ( i = 0; i < leafCount; i++ ) {
			ranges[i * 2] = LittleLong( in[i].firstStaticModel );
			ranges[i * 2 + 1] = LittleLong( in[i].numStaticModels );
		}
		translation->leafStaticModels = ranges;
	}
	if ( staticCount ) {
		memcpy( output + staticOffset, input + LittleLong( lumps[LUMP_MOHAA_STATICMODELDEF].fileofs ),
			staticCount * sizeof( dstaticModelMOHAA_t ) );
	}
	translation->staticModels = (const dstaticModelMOHAA_t *)( output + staticOffset );
	translation->numStaticModels = staticCount;
	if ( staticIndexCount ) {
		memcpy( output + staticIndexesOffset, input + LittleLong( lumps[LUMP_MOHAA_STATICMODELINDEXES].fileofs ),
			staticIndexCount * sizeof( uint16_t ) );
	}
	translation->staticModelIndexes = (const uint16_t *)( output + staticIndexesOffset );
	translation->numStaticModelIndexes = staticIndexCount;
	staticDataLength = LittleLong( lumps[LUMP_MOHAA_STATICMODELDATA].filelen );
	if ( staticDataLength ) {
		memcpy( output + staticDataOffset, input + LittleLong( lumps[LUMP_MOHAA_STATICMODELDATA].fileofs ),
			staticDataLength );
	}
	translation->staticModelData = output + staticDataOffset;
	translation->staticModelDataLength = staticDataLength;

	/* The grid needs all three of its lumps, the palette whole. */
	if ( LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDPALETTE].filelen ) == 768 && gridOffsetCount
		&& LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDDATA].filelen ) ) {
		uint16_t *gridOffsets = (uint16_t *)( output + gridOffsetsOffset );
		const byte *inOffsets = input + LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDOFFSETS].fileofs );

		memcpy( output + paletteOffset, input + LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDPALETTE].fileofs ), 768 );
		for ( i = 0; i < gridOffsetCount; i++ ) {
			gridOffsets[i] = (uint16_t)( inOffsets[i * 2] | ( inOffsets[i * 2 + 1] << 8 ) );
		}
		memcpy( output + gridDataOffset, input + LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDDATA].fileofs ),
			LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDDATA].filelen ) );
		translation->lightGridPalette = output + paletteOffset;
		translation->lightGridOffsets = gridOffsets;
		translation->numLightGridOffsets = gridOffsetCount;
		translation->lightGridData = output + gridDataOffset;
		translation->lightGridDataLength = LittleLong( lumps[LUMP_MOHAA_LIGHTGRIDDATA].filelen );
	}
	if ( sphereLightCount ) {
		memcpy( output + sphereLightsOffset, input + LittleLong( lumps[LUMP_MOHAA_SPHERELIGHTS].fileofs ),
			sphereLightCount * sizeof( dsphereLightMOHAA_t ) );
	}
	translation->sphereLights = (const dsphereLightMOHAA_t *)( output + sphereLightsOffset );
	translation->numSphereLights = sphereLightCount;
	if ( sphereVisCount ) {
		memcpy( output + sphereVisOffset, input + LittleLong( lumps[LUMP_MOHAA_SPHERELIGHTVIS].fileofs ),
			sphereVisCount * sizeof( int32_t ) );
	}
	translation->sphereLightVis = (const int32_t *)( output + sphereVisOffset );
	translation->numSphereLightVis = sphereVisCount;

	translation->data = output;
	translation->length = outputLength;
	return qtrue;
}

#endif
