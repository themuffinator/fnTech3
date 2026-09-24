/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

FAKK 42 compatibility

American McGee's Alice runs Ritual Entertainment's Heavy Metal: F.A.K.K.2
engine, and its maps are that engine's "FAKK" format at version 42. The
header carries a checksum Quake III's does not, and the twenty lumps are in
F.A.K.K.2's own order rather than Quake III's. Three of them -- the entity
lights, their visibility and the per-surface light definitions -- have no
IBSP 46 counterpart at all.

Its collision-bearing records are otherwise Quake III's own, byte for byte:
planes, nodes, leafs, leaf brushes, leaf surfaces, brushes, brush sides,
fogs, models, draw vertices, draw indexes, the entity text and the
visibility lump all match. Only two records differ, and both differ by one
trailing field: a shader carries a subdivision count and so is 76 bytes
rather than 72, and a surface carries one too and is 108 rather than 104.

This translator therefore presents a FAKK 42 map to the existing collision
loader as the IBSP 46 layout it already reads. The translation is lossy by
design and is collision evidence only: the light grid, the entity lights and
the light definitions are dropped, so it cannot stand in for a renderer that
reads Alice's lighting. Nothing here changes the map's own checksum, which
peers advertise.

Format reference: the lump order and record layout are F.A.K.K.2's, as
published in Ritual's own v1.02 SDK (qcommon/qfiles.h), and every one of them
was re-measured against the thirty-six retail Alice maps before being
written here; see docs/fntech3/2026-09-22-alice-maps.md. No implementation
text is incorporated from that SDK or from any other reconstruction.
===========================================================================
*/
#ifndef __BSP_FAKK_H__
#define __BSP_FAKK_H__

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "qfiles.h"

// little-endian "FAKK"
#define FAKK_IDENT			( ( 'K' << 24 ) + ( 'K' << 16 ) + ( 'A' << 8 ) + 'F' )
/* Ritual's own Heavy Metal: F.A.K.K.2 maps are version 12 and Medal of Honor:
 * Allied Assault reuses the ident again at other versions. Alice is 42, and
 * only 42 is admitted here: the ident alone does not identify the format. */
#define FAKK_VERSION_ALICE	42

/* F.A.K.K.2's lump order. These are not Quake III's indices, and the three
 * at the end have no Quake III counterpart. */
#define LUMP_FAKK_SHADERS		0
#define LUMP_FAKK_PLANES		1
#define LUMP_FAKK_LIGHTMAPS		2
#define LUMP_FAKK_SURFACES		3
#define LUMP_FAKK_DRAWVERTS		4
#define LUMP_FAKK_DRAWINDEXES	5
#define LUMP_FAKK_LEAFBRUSHES	6
#define LUMP_FAKK_LEAFSURFACES	7
#define LUMP_FAKK_LEAFS			8
#define LUMP_FAKK_NODES			9
#define LUMP_FAKK_BRUSHSIDES	10
#define LUMP_FAKK_BRUSHES		11
#define LUMP_FAKK_FOGS			12
#define LUMP_FAKK_MODELS		13
#define LUMP_FAKK_ENTITIES		14
#define LUMP_FAKK_VISIBILITY	15
#define LUMP_FAKK_LIGHTGRID		16
#define LUMP_FAKK_ENTLIGHTS		17
#define LUMP_FAKK_ENTLIGHTSVIS	18
#define LUMP_FAKK_LIGHTDEFS		19
#define FAKK_HEADER_LUMPS		20

/* Alice's light grid cell, read as immediates out of the retail renderer's own
 * light grid loader at 0x0049acb0, which writes 192, 192, 320 and their
 * reciprocals into adjacent globals before it touches the lump. Quake III
 * takes the same pair from a `gridsize` worldspawn key and defaults it to
 * 64x64x128; Alice reads no such key and uses these.
 *
 * With this cell, Quake III's own derivation against the world model's bounds
 * reproduces the light grid point count -- the lump length over eight -- on
 * every one of the thirty-six retail maps. See
 * docs/fntech3/2026-09-22-alice-maps.md. */
#define FAKK_LIGHTGRID_SIZE_X	192.0f
#define FAKK_LIGHTGRID_SIZE_Y	192.0f
#define FAKK_LIGHTGRID_SIZE_Z	320.0f

/*
==================
FAKK_LightGridCounts

The per-axis point counts a FAKK world of these bounds carries, by Quake III's
derivation: snap both bounds inward to the grid -- the low one up, the high one
down -- and count the cells between them inclusive. Returns the total, or 0 if
any axis is empty.
==================
*/
static ID_INLINE int FAKK_LightGridCounts( const float mins[3], const float maxs[3],
	int counts[3] ) {
	const float size[3] = {
		FAKK_LIGHTGRID_SIZE_X, FAKK_LIGHTGRID_SIZE_Y, FAKK_LIGHTGRID_SIZE_Z
	};
	int i, total = 1;

	for ( i = 0; i < 3; i++ ) {
		const float lo = size[i] * ceilf( mins[i] / size[i] );
		const float hi = size[i] * floorf( maxs[i] / size[i] );

		counts[i] = (int)( ( hi - lo ) / size[i] ) + 1;
		if ( counts[i] <= 0 ) {
			counts[0] = counts[1] = counts[2] = 0;
			return 0;
		}
		total *= counts[i];
	}
	return total;
}

typedef struct {
	void	*data;
	int		length;

	/* Each shader record's subdivision count, in the map's shader order, when
	 * the caller asked for FAKK_TRANSLATE_SHADER_SUBDIVISIONS; NULL otherwise.
	 * It lives inside data, past the last lump, and is freed with it. */
	const int32_t	*shaderSubdivisions;
	int				numShaderSubdivisions;
} fakk_translation_t;

/* Alice's collision loader reads two things from a patch's shader record
 * that Quake III's does not (alice.exe CMod_LoadPatches, 0x00430683..
 * 0x004307a9). Its shader records are kept whole, 76 bytes each
 * (CMod_LoadShaders 0x0042fc86..0x0042fcad), so the record's trailing
 * subdivision count is at +0x48.
 *
 * A patch whose shader's contents have neither bit 0 nor any of bits 16-19
 * gets no collision at all: its surface slot stays empty (0x004306a5,
 * 0x004306a9, 0x004306b5). Those bits are solid and the player, monster,
 * camera and weapon clips, in Ritual's content numbering. So water, fog and
 * non-solid patches are never traced against.
 *
 * Every other patch is subdivided for collision until no curve midpoint is
 * as far as the shader's subdivision count from its chord, a count raised
 * to 16 when smaller (0x00430780..0x004307a9, compared in CM_NeedsSubdivision
 * 0x00432877). Quake III subdivides every patch at 16. */
#define FAKK_PATCH_COLLISION_CONTENTS	0x000f0001
#define FAKK_PATCH_MIN_SUBDIVISIONS		16

/* What a translation carries beyond the collision records. The collision
 * loader reads none of these, so it asks for none of them; the renderer asks
 * for the light grid, whose record is Quake III's: the retail loader at
 * 0x0049acb0 refuses a lump that is not eight bytes a point ("light grid
 * mismatch") and colour-shifts bytes 0-2 and 3-4-5 of each point exactly as
 * Quake III's R_LoadLightGrid does. */
#define FAKK_TRANSLATE_LIGHTGRID	0x0001
/* The collision loader asks for each shader's subdivision count, which the
 * Quake III shader record has no room for. */
#define FAKK_TRANSLATE_SHADER_SUBDIVISIONS	0x0002

typedef struct {
	int32_t		ident;
	int32_t		version;
	int32_t		checksum;

	lump_t		lumps[FAKK_HEADER_LUMPS];
} dheaderFAKK_t;

typedef struct {
	char		shader[MAX_QPATH];
	int32_t		surfaceFlags;
	int32_t		contentFlags;
	int32_t		subdivisions;
} dshaderFAKK_t;

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
	vec3_t		lightmapVecs[3];	// for patches, [0] and [1] are lodbounds

	int32_t		patchWidth;
	int32_t		patchHeight;

	float		subdivisions;
} dsurfaceFAKK_t;

static void FAKK_SetError( char *error, const size_t errorSize, const char *message )
{
	if ( !error || !errorSize ) {
		return;
	}
	Q_strncpyz( error, message, (int)errorSize );
}

static qboolean FAKK_LumpValid( const lump_t *lump, const int inputLength )
{
	const int32_t offset = LittleLong( lump->fileofs );
	const int32_t length = LittleLong( lump->filelen );

	if ( offset < 0 || length < 0 || offset > inputLength || length > inputLength - offset ) {
		return qfalse;
	}
	/* A populated lump may not overlap the directory it is described by. */
	if ( length && offset < (int)sizeof( dheaderFAKK_t ) ) {
		return qfalse;
	}
	return qtrue;
}

/* Records are fixed size, so a remainder means the file is not what it says. */
static qboolean FAKK_LumpCount( const lump_t *lump, const int recordSize, int *count )
{
	const int32_t length = LittleLong( lump->filelen );

	if ( recordSize <= 0 || ( length % recordSize ) != 0 ) {
		return qfalse;
	}
	*count = length / recordSize;
	return qtrue;
}

static qboolean FAKK_AppendLump( int *length, const int bytes, int *offset )
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
FAKK_TranslateToV46Ex

Present a FAKK 42 map to an IBSP 46 loader. The caller owns the returned
buffer through freeFn. Every read is bounded by the validated lump directory;
no index inside a record is followed here, because the loaders validate those
themselves. `flags` selects what is carried beyond the collision records.
==================
*/
static qboolean FAKK_TranslateToV46Ex(
	const void *inputData,
	const int inputLength,
	void *( *allocFn )( int ),
	void ( *freeFn )( void * ),
	fakk_translation_t *translation,
	char *error,
	const size_t errorSize,
	const int flags )
{
	/* Where each FAKK lump lands in the Quake III directory, indexed by the FAKK
	 * lump number. -1 drops the lump: the entity lights and light definitions
	 * have no counterpart to land in, and the light grid is carried only when
	 * the caller asks for it. The collision loader reads none of the three. */
	int fakkLumpTargets[FAKK_HEADER_LUMPS] = {
		LUMP_SHADERS,		/* 0  shaders */
		LUMP_PLANES,		/* 1  planes */
		LUMP_LIGHTMAPS,		/* 2  lightmaps */
		LUMP_SURFACES,		/* 3  surfaces */
		LUMP_DRAWVERTS,		/* 4  draw vertices */
		LUMP_DRAWINDEXES,	/* 5  draw indexes */
		LUMP_LEAFBRUSHES,	/* 6  leaf brushes */
		LUMP_LEAFSURFACES,	/* 7  leaf surfaces */
		LUMP_LEAFS,			/* 8  leafs */
		LUMP_NODES,			/* 9  nodes */
		LUMP_BRUSHSIDES,	/* 10 brush sides */
		LUMP_BRUSHES,		/* 11 brushes */
		LUMP_FOGS,			/* 12 fogs */
		LUMP_MODELS,		/* 13 models */
		LUMP_ENTITIES,		/* 14 entity text */
		LUMP_VISIBILITY,	/* 15 visibility */
		-1,					/* 16 light grid */
		-1,					/* 17 entity lights */
		-1,					/* 18 entity light visibility */
		-1					/* 19 light definitions */
	};

	dheaderFAKK_t	source;
	const lump_t	*lumps;
	const byte		*input = (const byte *)inputData;
	byte			*output = NULL;
	dheader_t		*header;
	int				offsets[HEADER_LUMPS];
	int				lengths[HEADER_LUMPS];
	int				shaderCount, surfaceCount;
	int				outputLength;
	int				subdivisionsOffset = 0;
	int				i;

	if ( flags & FAKK_TRANSLATE_LIGHTGRID ) {
		fakkLumpTargets[LUMP_FAKK_LIGHTGRID] = LUMP_LIGHTGRID;
	}

	if ( !translation ) {
		FAKK_SetError( error, errorSize, "FAKK translation target is missing" );
		return qfalse;
	}
	Com_Memset( translation, 0, sizeof( *translation ) );
	FAKK_SetError( error, errorSize, "" );

	if ( !allocFn || !freeFn ) {
		FAKK_SetError( error, errorSize, "FAKK translation needs an allocator" );
		return qfalse;
	}
	if ( !inputData || inputLength < (int)sizeof( dheaderFAKK_t ) ) {
		FAKK_SetError( error, errorSize, "FAKK header is truncated" );
		return qfalse;
	}

	memcpy( &source, inputData, sizeof( source ) );
	if ( LittleLong( source.ident ) != FAKK_IDENT
		|| LittleLong( source.version ) != FAKK_VERSION_ALICE ) {
		FAKK_SetError( error, errorSize, "FAKK translator received the wrong map format" );
		return qfalse;
	}

	lumps = source.lumps;
	for ( i = 0; i < FAKK_HEADER_LUMPS; i++ ) {
		if ( !FAKK_LumpValid( &lumps[i], inputLength ) ) {
			FAKK_SetError( error, errorSize, "FAKK has an invalid lump directory" );
			return qfalse;
		}
	}

	/* The two lumps that change shape are counted first: a bad record size is
	 * a malformed file, not a map with an unusual amount of content. */
	if ( !FAKK_LumpCount( &lumps[LUMP_FAKK_SHADERS], (int)sizeof( dshaderFAKK_t ), &shaderCount )
		|| !FAKK_LumpCount( &lumps[LUMP_FAKK_SURFACES], (int)sizeof( dsurfaceFAKK_t ), &surfaceCount ) ) {
		FAKK_SetError( error, errorSize, "FAKK lump size is not a whole number of records" );
		return qfalse;
	}

	Com_Memset( offsets, 0, sizeof( offsets ) );
	Com_Memset( lengths, 0, sizeof( lengths ) );
	for ( i = 0; i < FAKK_HEADER_LUMPS; i++ ) {
		const int target = fakkLumpTargets[i];

		if ( target >= 0 ) {
			lengths[target] = LittleLong( lumps[i].filelen );
		}
	}
	lengths[LUMP_SHADERS] = shaderCount * (int)sizeof( dshader_t );
	lengths[LUMP_SURFACES] = surfaceCount * (int)sizeof( dsurface_t );

	outputLength = (int)sizeof( dheader_t );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		if ( !FAKK_AppendLump( &outputLength, lengths[i], &offsets[i] ) ) {
			FAKK_SetError( error, errorSize, "FAKK translation exceeds the addressable map size" );
			return qfalse;
		}
	}
	/* The subdivision counts follow the last lump, outside the directory, so
	 * an IBSP 46 reader never sees them. shaderCount is at most a quarter of
	 * the input's length, so the product cannot overflow. */
	if ( ( flags & FAKK_TRANSLATE_SHADER_SUBDIVISIONS )
		&& !FAKK_AppendLump( &outputLength, shaderCount * (int)sizeof( int32_t ), &subdivisionsOffset ) ) {
		FAKK_SetError( error, errorSize, "FAKK translation exceeds the addressable map size" );
		return qfalse;
	}

	output = (byte *)allocFn( outputLength );
	if ( !output ) {
		FAKK_SetError( error, errorSize, "FAKK translation could not allocate its map" );
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

	/* Records the two formats share are carried across byte for byte, so a
	 * translated map keeps the stored little-endian values on every host. */
	for ( i = 0; i < FAKK_HEADER_LUMPS; i++ ) {
		const int target = fakkLumpTargets[i];

		if ( target < 0 || target == LUMP_SHADERS || target == LUMP_SURFACES ) {
			continue;
		}
		if ( lengths[target] ) {
			memcpy( output + offsets[target], input + LittleLong( lumps[i].fileofs ),
				lengths[target] );
		}
	}

	{
		const dshaderFAKK_t *shaders = (const dshaderFAKK_t *)( input + LittleLong( lumps[LUMP_FAKK_SHADERS].fileofs ) );
		dshader_t *out = (dshader_t *)( output + offsets[LUMP_SHADERS] );

		/* The name and the two flag words are Quake III's. The subdivision
		 * count each shader carries has no place in Quake III's record; the
		 * collision loader asks for it separately, since Alice's patch
		 * collision is subdivided by it. */
		for ( i = 0; i < shaderCount; i++ ) {
			memcpy( out[i].shader, shaders[i].shader, sizeof( out[i].shader ) );
			out[i].surfaceFlags = shaders[i].surfaceFlags;
			out[i].contentFlags = shaders[i].contentFlags;
		}
		if ( flags & FAKK_TRANSLATE_SHADER_SUBDIVISIONS ) {
			int32_t *subdivisions = (int32_t *)( output + subdivisionsOffset );

			for ( i = 0; i < shaderCount; i++ ) {
				subdivisions[i] = LittleLong( shaders[i].subdivisions );
			}
			translation->shaderSubdivisions = subdivisions;
			translation->numShaderSubdivisions = shaderCount;
		}
	}

	{
		const dsurfaceFAKK_t *surfaces = (const dsurfaceFAKK_t *)( input + LittleLong( lumps[LUMP_FAKK_SURFACES].fileofs ) );
		dsurface_t *out = (dsurface_t *)( output + offsets[LUMP_SURFACES] );

		for ( i = 0; i < surfaceCount; i++ ) {
			out[i].shaderNum = surfaces[i].shaderNum;
			out[i].fogNum = surfaces[i].fogNum;
			out[i].surfaceType = surfaces[i].surfaceType;
			out[i].firstVert = surfaces[i].firstVert;
			out[i].numVerts = surfaces[i].numVerts;
			out[i].firstIndex = surfaces[i].firstIndex;
			out[i].numIndexes = surfaces[i].numIndexes;
			out[i].lightmapNum = surfaces[i].lightmapNum;
			out[i].lightmapX = surfaces[i].lightmapX;
			out[i].lightmapY = surfaces[i].lightmapY;
			out[i].lightmapWidth = surfaces[i].lightmapWidth;
			out[i].lightmapHeight = surfaces[i].lightmapHeight;
			memcpy( out[i].lightmapOrigin, surfaces[i].lightmapOrigin, sizeof( out[i].lightmapOrigin ) );
			memcpy( out[i].lightmapVecs, surfaces[i].lightmapVecs, sizeof( out[i].lightmapVecs ) );
			out[i].patchWidth = surfaces[i].patchWidth;
			out[i].patchHeight = surfaces[i].patchHeight;
			/* The trailing subdivision count is the surface's own; Alice's
			 * patch collision reads its shader's (alice.exe 0x00430780),
			 * which the shader table above carries. */
		}
	}

	translation->data = output;
	translation->length = outputLength;
	return qtrue;
}

/*
==================
FAKK_TranslateToV46

The collision translation: the map's collision records, and each shader's
subdivision count, which Alice's patch collision is built with.
==================
*/
static qboolean FAKK_TranslateToV46(
	const void *inputData,
	const int inputLength,
	void *( *allocFn )( int ),
	void ( *freeFn )( void * ),
	fakk_translation_t *translation,
	char *error,
	const size_t errorSize )
{
	return FAKK_TranslateToV46Ex( inputData, inputLength, allocFn, freeFn,
		translation, error, errorSize, FAKK_TRANSLATE_SHADER_SUBDIVISIONS );
}

#endif
