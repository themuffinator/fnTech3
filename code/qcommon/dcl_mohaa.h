/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor: Allied Assault's permanent decals

A level's `maps/<name>.dcl` holds the decals its designers laid with 2015's
decal editor: each decal's definition; then the fragments the client game
cut the decals into on the map, each with its shader, what it lies on and
its vertices with their lit colour; then, for each brush model and each
leaf in the map's order, the fragments it holds. The renderers build the
fragments into the map (code/renderercommon/tr_mohaa_world.c); the editor
(`dcl_editmode`, code/client/cl_mohaa_decals.cpp) reads the definitions,
has the client game cut them again, and writes the whole file back.

The layout was measured on the twelve files 1.00 ships in Pak5.pk3, all of
them version 1 with a checksum of 0. Version 2, which later engines write
(OpenMoHAA renderergl1/tr_marks_permanent.c, reference only), puts the map's
`map_time` worldspawn key after the header, so that a file cut against an
older compile of its map can be refused; version 1 has nothing to check.

  header    20 bytes: "DCL ", version, checksum, decals, fragments
  map time  32 bytes, version 2 only
  decal     124 bytes: shader[64]; position; projection, which the client
            game leaves as the normal of the surface it hit; radius; height
            and width scales; rotation, -1 for a decal laid along a path;
            colour and alpha; whether it is lit
  fragment  76 bytes: shader[64]; fog, 0; what it lies on, 0 the world, a
            terrain patch above 0 and a brush model below; its vertex count;
            then that many of Quake III's 24-byte polygon vertices, position,
            texture coordinate and colour
  lists     for each brush model, then each leaf: a count and that many
            fragment numbers
===========================================================================
*/
#ifndef FNTECH3_DCL_MOHAA_H
#define FNTECH3_DCL_MOHAA_H

#include <string.h>

#define MOHAA_DCL_VERSION_1				1
#define MOHAA_DCL_VERSION_2				2

#define MOHAA_DCL_HEADER_BYTES			20
#define MOHAA_DCL_MAP_TIME_BYTES		32
#define MOHAA_DCL_SHADER_BYTES			64
#define MOHAA_DCL_DECAL_BYTES			124
#define MOHAA_DCL_FRAGMENT_BYTES		76
#define MOHAA_DCL_VERT_BYTES			24

/* A decal's fields, from its start. */
#define MOHAA_DCL_DECAL_POSITION		64
#define MOHAA_DCL_DECAL_PROJECTION		76
#define MOHAA_DCL_DECAL_RADIUS			88
#define MOHAA_DCL_DECAL_HEIGHT_SCALE	92
#define MOHAA_DCL_DECAL_WIDTH_SCALE		96
#define MOHAA_DCL_DECAL_ROTATION		100
#define MOHAA_DCL_DECAL_COLOR			104
#define MOHAA_DCL_DECAL_LIGHTING		120

/* A fragment's fields, from its start. */
#define MOHAA_DCL_FRAGMENT_FOG			64
#define MOHAA_DCL_FRAGMENT_INDEX		68
#define MOHAA_DCL_FRAGMENT_VERTS		72

/* The client game keeps at most eight points a fragment; a file claiming
 * more than this is not one. */
#define MOHAA_DCL_MAX_FRAGMENT_VERTS	64

typedef struct {
	int		version;
	int		numDecals;
	int		numFragments;
	int		mapTime;		/* offset of the map time, -1 in version 1 */
	int		decals;			/* offset of the first decal */
	int		fragments;		/* offset of the first fragment */
	int		lists;			/* offset of the lists, which run to the end */
} dclLayoutMOHAA_t;

static ID_INLINE int MOHAA_DCLInt( const byte *p ) {
	int value;
	memcpy( &value, p, sizeof( value ) );
	return LittleLong( value );
}

static ID_INLINE float MOHAA_DCLFloat( const byte *p ) {
	float value;
	memcpy( &value, p, sizeof( value ) );
	return LittleFloat( value );
}

/* Where a file's parts start, or false when it is not a decal file of a
 * version this engine reads or a part runs past its end. */
static ID_INLINE qboolean MOHAA_DCLLayout( const byte *data, int length, dclLayoutMOHAA_t *layout ) {
	int offset, i;

	memset( layout, 0, sizeof( *layout ) );
	layout->mapTime = -1;
	if ( !data || length < MOHAA_DCL_HEADER_BYTES || memcmp( data, "DCL ", 4 ) ) {
		return qfalse;
	}
	layout->version = MOHAA_DCLInt( data + 4 );
	layout->numDecals = MOHAA_DCLInt( data + 12 );
	layout->numFragments = MOHAA_DCLInt( data + 16 );
	if ( ( layout->version != MOHAA_DCL_VERSION_1 && layout->version != MOHAA_DCL_VERSION_2 )
		|| layout->numDecals < 0 || layout->numFragments < 0 ) {
		return qfalse;
	}

	offset = MOHAA_DCL_HEADER_BYTES;
	if ( layout->version == MOHAA_DCL_VERSION_2 ) {
		if ( length - offset < MOHAA_DCL_MAP_TIME_BYTES ) {
			return qfalse;
		}
		layout->mapTime = offset;
		offset += MOHAA_DCL_MAP_TIME_BYTES;
	}

	layout->decals = offset;
	if ( layout->numDecals > ( length - offset ) / MOHAA_DCL_DECAL_BYTES ) {
		return qfalse;
	}
	offset += layout->numDecals * MOHAA_DCL_DECAL_BYTES;

	layout->fragments = offset;
	for ( i = 0; i < layout->numFragments; i++ ) {
		int verts;
		if ( length - offset < MOHAA_DCL_FRAGMENT_BYTES ) {
			return qfalse;
		}
		verts = MOHAA_DCLInt( data + offset + MOHAA_DCL_FRAGMENT_VERTS );
		offset += MOHAA_DCL_FRAGMENT_BYTES;
		if ( verts < 0 || verts > MOHAA_DCL_MAX_FRAGMENT_VERTS
			|| verts > ( length - offset ) / MOHAA_DCL_VERT_BYTES ) {
			return qfalse;
		}
		offset += verts * MOHAA_DCL_VERT_BYTES;
	}
	layout->lists = offset;
	return qtrue;
}

/* The shader a decal or fragment names, held to a path's length. */
static ID_INLINE void MOHAA_DCLShader( const byte *record, char *out, int size ) {
	int i;
	for ( i = 0; i < size - 1 && i < MOHAA_DCL_SHADER_BYTES && record[i]; i++ ) {
		out[i] = (char)record[i];
	}
	out[i] = '\0';
}

#endif
