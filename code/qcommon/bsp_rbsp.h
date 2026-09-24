/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

RBSP v1 compatibility

Jedi Academy's maps are Raven's "RBSP" version 1: Quake III's seventeen lumps
plus a light-array lump, with four lightmap sets and styles per surface, per
vertex and per light-grid point, and a draw-surface reference on every brush
side. Its collision-bearing lumps are otherwise Quake III's own records, so
this translator presents an RBSP map to the existing collision loader as the
IBSP 46 layout it already reads.

The translation is lossy by design and is collision evidence only: the extra
lightmap sets, their styles and the light array are dropped, so it cannot
stand in for a renderer that draws Jedi Academy's lighting. Nothing here
changes the map's own checksum, which peers advertise.

Format reference: Raven's released sources as carried by OpenJK,
codemp/qcommon/qfiles.h:307-499. No implementation text is incorporated.
===========================================================================
*/
#ifndef __BSP_RBSP_H__
#define __BSP_RBSP_H__

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "qfiles.h"

// little-endian "RBSP"
#define RBSP_IDENT			( ( 'P' << 24 ) + ( 'S' << 16 ) + ( 'B' << 8 ) + 'R' )
#define RBSP_VERSION		1

/* Lumps 0-16 are Quake III's own indices; Raven appended the light array. */
#define LUMP_RBSP_LIGHTARRAY	17
#define RBSP_HEADER_LUMPS		18

#define RBSP_MAXLIGHTMAPS	4
/* Raven's light-style values, kept so a reader can name what it drops. */
#define RBSP_LS_NORMAL		0x00
#define RBSP_LS_UNUSED		0xfe
#define RBSP_LS_NONE		0xff
#define RBSP_LIGHTMAP_BYTES	( 128 * 128 * 3 )
#define RBSP_GRID_BYTES		8	/* Quake III's light-grid point, in bytes. */

typedef struct {
	void	*data;
	int		length;
} rbsp_translation_t;

typedef struct {
	int32_t		ident;
	int32_t		version;
	lump_t		lumps[RBSP_HEADER_LUMPS];
} dheaderRBSP_t;

typedef struct {
	int32_t		planeNum;
	int32_t		shaderNum;
	int32_t		drawSurfNum;
} dbrushsideRBSP_t;

typedef struct {
	vec3_t		xyz;
	float		st[2];
	float		lightmap[RBSP_MAXLIGHTMAPS][2];
	vec3_t		normal;
	byte		color[RBSP_MAXLIGHTMAPS][4];
} drawVertRBSP_t;

typedef struct {
	byte		ambientLight[RBSP_MAXLIGHTMAPS][3];
	byte		directLight[RBSP_MAXLIGHTMAPS][3];
	byte		styles[RBSP_MAXLIGHTMAPS];
	byte		latLong[2];
} dgridRBSP_t;

typedef struct {
	int32_t		shaderNum;
	int32_t		fogNum;
	int32_t		surfaceType;

	int32_t		firstVert;
	int32_t		numVerts;

	int32_t		firstIndex;
	int32_t		numIndexes;

	byte		lightmapStyles[RBSP_MAXLIGHTMAPS];
	byte		vertexStyles[RBSP_MAXLIGHTMAPS];
	int32_t		lightmapNum[RBSP_MAXLIGHTMAPS];
	int32_t		lightmapX[RBSP_MAXLIGHTMAPS];
	int32_t		lightmapY[RBSP_MAXLIGHTMAPS];
	int32_t		lightmapWidth, lightmapHeight;

	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[3];

	int32_t		patchWidth;
	int32_t		patchHeight;
} dsurfaceRBSP_t;

static void RBSP_SetError( char *error, const size_t errorSize, const char *message )
{
	if ( !error || !errorSize ) {
		return;
	}
	Q_strncpyz( error, message, (int)errorSize );
}

static qboolean RBSP_LumpValid( const lump_t *lump, const int inputLength )
{
	const int32_t offset = LittleLong( lump->fileofs );
	const int32_t length = LittleLong( lump->filelen );

	if ( offset < 0 || length < 0 || offset > inputLength || length > inputLength - offset ) {
		return qfalse;
	}
	/* A populated lump may not overlap the directory it is described by. */
	if ( length && offset < (int)sizeof( dheaderRBSP_t ) ) {
		return qfalse;
	}
	return qtrue;
}

/* Records are fixed size, so a remainder means the file is not what it says. */
static qboolean RBSP_LumpCount( const lump_t *lump, const int recordSize, int *count )
{
	const int32_t length = LittleLong( lump->filelen );

	if ( recordSize <= 0 || ( length % recordSize ) != 0 ) {
		return qfalse;
	}
	*count = length / recordSize;
	return qtrue;
}

static qboolean RBSP_AppendLump( int *length, const int bytes, int *offset )
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
RBSP_TranslateToV46

Present an RBSP v1 map to the IBSP 46 collision loader. The caller owns the
returned buffer through freeFn. Every read is bounded by the validated lump
directory; no index inside a record is followed here, because the collision
loader validates those itself.
==================
*/
static qboolean RBSP_TranslateToV46(
	const void *inputData,
	const int inputLength,
	void *( *allocFn )( int ),
	void ( *freeFn )( void * ),
	rbsp_translation_t *translation,
	char *error,
	const size_t errorSize )
{
	dheaderRBSP_t	source;
	const lump_t	*lumps;
	const byte		*input = (const byte *)inputData;
	byte			*output = NULL;
	dheader_t		*header;
	int				offsets[HEADER_LUMPS];
	int				lengths[HEADER_LUMPS];
	int				brushSideCount, drawVertCount, surfaceCount, gridCount;
	int				outputLength;
	int				i;

	if ( !translation ) {
		RBSP_SetError( error, errorSize, "RBSP translation target is missing" );
		return qfalse;
	}
	Com_Memset( translation, 0, sizeof( *translation ) );
	RBSP_SetError( error, errorSize, "" );

	if ( !allocFn || !freeFn ) {
		RBSP_SetError( error, errorSize, "RBSP translation needs an allocator" );
		return qfalse;
	}
	if ( !inputData || inputLength < (int)sizeof( dheaderRBSP_t ) ) {
		RBSP_SetError( error, errorSize, "RBSP header is truncated" );
		return qfalse;
	}

	memcpy( &source, inputData, sizeof( source ) );
	if ( LittleLong( source.ident ) != RBSP_IDENT || LittleLong( source.version ) != RBSP_VERSION ) {
		RBSP_SetError( error, errorSize, "RBSP translator received the wrong map format" );
		return qfalse;
	}

	lumps = source.lumps;
	for ( i = 0; i < RBSP_HEADER_LUMPS; i++ ) {
		if ( !RBSP_LumpValid( &lumps[i], inputLength ) ) {
			RBSP_SetError( error, errorSize, "RBSP has an invalid lump directory" );
			return qfalse;
		}
	}

	/* The lumps that change shape are counted first: a bad record size is a
	 * malformed file, not a map with an unusual amount of content. */
	if ( !RBSP_LumpCount( &lumps[LUMP_BRUSHSIDES], (int)sizeof( dbrushsideRBSP_t ), &brushSideCount )
		|| !RBSP_LumpCount( &lumps[LUMP_DRAWVERTS], (int)sizeof( drawVertRBSP_t ), &drawVertCount )
		|| !RBSP_LumpCount( &lumps[LUMP_SURFACES], (int)sizeof( dsurfaceRBSP_t ), &surfaceCount )
		|| !RBSP_LumpCount( &lumps[LUMP_LIGHTGRID], (int)sizeof( dgridRBSP_t ), &gridCount ) ) {
		RBSP_SetError( error, errorSize, "RBSP lump size is not a whole number of records" );
		return qfalse;
	}

	Com_Memset( offsets, 0, sizeof( offsets ) );
	Com_Memset( lengths, 0, sizeof( lengths ) );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		lengths[i] = LittleLong( lumps[i].filelen );
	}
	lengths[LUMP_BRUSHSIDES] = brushSideCount * (int)sizeof( dbrushside_t );
	lengths[LUMP_DRAWVERTS] = drawVertCount * (int)sizeof( drawVert_t );
	lengths[LUMP_SURFACES] = surfaceCount * (int)sizeof( dsurface_t );
	lengths[LUMP_LIGHTGRID] = gridCount * RBSP_GRID_BYTES;

	outputLength = (int)sizeof( dheader_t );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		if ( !RBSP_AppendLump( &outputLength, lengths[i], &offsets[i] ) ) {
			RBSP_SetError( error, errorSize, "RBSP translation exceeds the addressable map size" );
			return qfalse;
		}
	}

	output = (byte *)allocFn( outputLength );
	if ( !output ) {
		RBSP_SetError( error, errorSize, "RBSP translation could not allocate its map" );
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
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		if ( i == LUMP_BRUSHSIDES || i == LUMP_DRAWVERTS || i == LUMP_SURFACES
			|| i == LUMP_LIGHTGRID ) {
			continue;
		}
		if ( lengths[i] ) {
			memcpy( output + offsets[i], input + LittleLong( lumps[i].fileofs ), lengths[i] );
		}
	}

	{
		const dbrushsideRBSP_t *sides = (const dbrushsideRBSP_t *)( input + LittleLong( lumps[LUMP_BRUSHSIDES].fileofs ) );
		dbrushside_t *out = (dbrushside_t *)( output + offsets[LUMP_BRUSHSIDES] );

		/* The draw-surface reference each side carries is Raven's own and has
		 * no collision meaning; the plane and shader are Quake III's. */
		for ( i = 0; i < brushSideCount; i++ ) {
			out[i].planeNum = sides[i].planeNum;
			out[i].shaderNum = sides[i].shaderNum;
		}
	}

	{
		const drawVertRBSP_t *verts = (const drawVertRBSP_t *)( input + LittleLong( lumps[LUMP_DRAWVERTS].fileofs ) );
		drawVert_t *out = (drawVert_t *)( output + offsets[LUMP_DRAWVERTS] );

		/* Patch collision reads positions; the first lightmap set and colour
		 * are kept so the record stays a complete Quake III vertex. */
		for ( i = 0; i < drawVertCount; i++ ) {
			memcpy( out[i].xyz, verts[i].xyz, sizeof( out[i].xyz ) );
			memcpy( out[i].st, verts[i].st, sizeof( out[i].st ) );
			memcpy( out[i].lightmap, verts[i].lightmap[0], sizeof( out[i].lightmap ) );
			memcpy( out[i].normal, verts[i].normal, sizeof( out[i].normal ) );
			memcpy( out[i].color.rgba, verts[i].color[0], sizeof( out[i].color.rgba ) );
		}
	}

	{
		const dsurfaceRBSP_t *surfaces = (const dsurfaceRBSP_t *)( input + LittleLong( lumps[LUMP_SURFACES].fileofs ) );
		dsurface_t *out = (dsurface_t *)( output + offsets[LUMP_SURFACES] );

		for ( i = 0; i < surfaceCount; i++ ) {
			out[i].shaderNum = surfaces[i].shaderNum;
			out[i].fogNum = surfaces[i].fogNum;
			out[i].surfaceType = surfaces[i].surfaceType;
			out[i].firstVert = surfaces[i].firstVert;
			out[i].numVerts = surfaces[i].numVerts;
			out[i].firstIndex = surfaces[i].firstIndex;
			out[i].numIndexes = surfaces[i].numIndexes;
			/* Only the first lightmap set survives: its styles, the other
			 * three sets and the light array belong to a renderer that reads
			 * RBSP itself. */
			out[i].lightmapNum = surfaces[i].lightmapNum[0];
			out[i].lightmapX = surfaces[i].lightmapX[0];
			out[i].lightmapY = surfaces[i].lightmapY[0];
			out[i].lightmapWidth = surfaces[i].lightmapWidth;
			out[i].lightmapHeight = surfaces[i].lightmapHeight;
			memcpy( out[i].lightmapOrigin, surfaces[i].lightmapOrigin, sizeof( out[i].lightmapOrigin ) );
			memcpy( out[i].lightmapVecs, surfaces[i].lightmapVecs, sizeof( out[i].lightmapVecs ) );
			out[i].patchWidth = surfaces[i].patchWidth;
			out[i].patchHeight = surfaces[i].patchHeight;
		}
	}

	{
		const dgridRBSP_t *grid = (const dgridRBSP_t *)( input + LittleLong( lumps[LUMP_LIGHTGRID].fileofs ) );
		byte *out = output + offsets[LUMP_LIGHTGRID];

		for ( i = 0; i < gridCount; i++ ) {
			memcpy( out + i * RBSP_GRID_BYTES, grid[i].ambientLight[0], 3 );
			memcpy( out + i * RBSP_GRID_BYTES + 3, grid[i].directLight[0], 3 );
			memcpy( out + i * RBSP_GRID_BYTES + 6, grid[i].latLong, 2 );
		}
	}

	translation->data = output;
	translation->length = outputLength;
	return qtrue;
}

#endif
