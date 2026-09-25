/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
/*
Q3Test IBSP 44 and 45

The public Q3Test releases shipped Quake III's map format before its 1.09
revision: IBSP 44 in 1.02 to 1.05 and IBSP 45 in 1.06 to 1.08. This header
translates either into an IBSP 46 image for the shared collision and renderer
loaders, leaving the original bytes -- and so the map checksum peers compare --
untouched. The layouts were read from each release's collision and world
loaders and checked against every map the six Win32 packages ship
(docs/fntech3/Q3TEST.md):

IBSP 45 is IBSP 46 except for three records: a plane carries a fifth word
(its axial type), a model carries its origin and head node before the ranges
IBSP 46 keeps, and a fog carries no visible side.

IBSP 44 has fifteen lumps and no shader lump. Brushes carry their contents
and brush sides their surface flags directly; surfaces carry their shader's
name inline and no surface type (a patch has a size, a triangle soup has
indexes, and anything else is a planar face its loader fans); a model names
only its head node and a surface range the retail renderers never read, which
1.05's q3test1 leaves stale. So the translation synthesizes the shader lump,
fans planar faces, gives each patch its bounds, and derives each submodel's
surfaces and brushes from its head node, reordering both so every model's are
contiguous as IBSP 46 requires. There is no light grid.

Two contracts ride on the synthesized records. Quake III collision takes a
patch's contents from its shader record, and 1.02-1.05 collide with every
patch as solid except one shader, which does not collide at all; so a patch's
record carries exactly those contents. A fog's visible side is not in either
format: Q3Test's renderers take it from the fog shader's fogParms type (above
zero the top, below zero the bottom, zero none), and every shipped fog's type
is 1, so each fog brush's top side (5, its sides being axial first) is written
here and a renderer that parses the shader may refine it.

This is independently written. It never loads anything a map names, and every
count, range and index is checked before any output is published.
*/
#ifndef FNTECH3_BSP_Q3TEST_H
#define FNTECH3_BSP_Q3TEST_H

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "qfiles.h"

#define Q3TEST_BSP_VERSION_44	44
#define Q3TEST_BSP_VERSION_45	45
#define Q3TEST_BSP_LUMPS_44		15
#define Q3TEST_BSP_LUMPS_45		17

enum {
	Q3T44_ENTITIES, Q3T44_PLANES, Q3T44_NODES, Q3T44_LEAFS, Q3T44_LEAFSURFACES,
	Q3T44_LEAFBRUSHES, Q3T44_MODELS, Q3T44_BRUSHES, Q3T44_BRUSHSIDES, Q3T44_LIGHTMAPS,
	Q3T44_VISIBILITY, Q3T44_DRAWVERTS, Q3T44_SURFACES, Q3T44_FOGS, Q3T44_DRAWINDEXES
};

typedef struct {
	float		normal[3];
	float		dist;
	int32_t		type;
} q3tBspPlane_t;

typedef struct {
	float		mins[3], maxs[3], origin[3];
	int32_t		headnode;
	int32_t		firstSurface, numSurfaces;
} q3tBspModel44_t;

typedef struct {
	float		mins[3], maxs[3], origin[3];
	int32_t		headnode;
	int32_t		firstSurface, numSurfaces;
	int32_t		firstBrush, numBrushes;
} q3tBspModel45_t;

typedef struct {
	char		shader[MAX_QPATH];
	int32_t		brushNum;
} q3tBspFog_t;

typedef struct {
	int32_t		firstSide, numSides;
	int32_t		contentFlags;
} q3tBspBrush44_t;

typedef struct {
	int32_t		planeNum;
	int32_t		surfaceFlags;
} q3tBspBrushSide44_t;

typedef struct {
	char		shader[MAX_QPATH];
	int32_t		fogNum;
	int32_t		unused;
	int32_t		firstVert, numVerts;
	int32_t		firstIndex, numIndexes;
	int32_t		patchWidth, patchHeight;
	int32_t		lightmapNum;
	int32_t		lightmapX, lightmapY;
	int32_t		lightmapWidth, lightmapHeight;
	float		lightmapOrigin[3];
	float		lightmapVecs[3][3];
} q3tBspSurface44_t;

typedef struct {
	void	*data;
	int		length;
} q3tBspTranslation_t;

/* Retail sizes (fs_bsp.md 4). */
typedef char q3tBspPlaneSize[ sizeof( q3tBspPlane_t ) == 20 ? 1 : -1 ];
typedef char q3tBspModel44Size[ sizeof( q3tBspModel44_t ) == 48 ? 1 : -1 ];
typedef char q3tBspModel45Size[ sizeof( q3tBspModel45_t ) == 56 ? 1 : -1 ];
typedef char q3tBspFogSize[ sizeof( q3tBspFog_t ) == 68 ? 1 : -1 ];
typedef char q3tBspSurface44Size[ sizeof( q3tBspSurface44_t ) == 164 ? 1 : -1 ];

static void Q3TBSP_Error( char *error, size_t size, const char *message ) {
	if ( error && size ) {
		size_t length = strlen( message );
		if ( length >= size ) length = size - 1;
		memcpy( error, message, length );
		error[length] = '\0';
	}
}

static int32_t Q3TBSP_ReadInt( const void *data ) {
	const unsigned char *p = (const unsigned char *)data;
	return (int32_t)( (uint32_t)p[0] | ( (uint32_t)p[1] << 8 ) | ( (uint32_t)p[2] << 16 ) | ( (uint32_t)p[3] << 24 ) );
}

static float Q3TBSP_ReadFloat( const void *data ) {
	const int32_t bits = Q3TBSP_ReadInt( data );
	float value;
	memcpy( &value, &bits, sizeof( value ) );
	return value;
}

static void Q3TBSP_WriteInt( void *data, int32_t value ) {
	unsigned char *p = (unsigned char *)data;
	const uint32_t bits = (uint32_t)value;
	p[0] = (unsigned char)bits; p[1] = (unsigned char)( bits >> 8 );
	p[2] = (unsigned char)( bits >> 16 ); p[3] = (unsigned char)( bits >> 24 );
}

static void Q3TBSP_WriteFloat( void *data, float value ) {
	int32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	Q3TBSP_WriteInt( data, bits );
}

static qboolean Q3TBSP_Range( int32_t first, int32_t count, int32_t capacity ) {
	return first >= 0 && count >= 0 && first <= capacity && count <= capacity - first ? qtrue : qfalse;
}

/* A lump directory entry of the input: in bounds and a whole number of records. */
typedef struct {
	const unsigned char	*data;
	int32_t				length;
	int32_t				count;
} q3tBspLump_t;

static qboolean Q3TBSP_Lump( const unsigned char *input, int inputLength, int headerBytes, int index,
	int recordSize, q3tBspLump_t *lump ) {
	const int32_t offset = Q3TBSP_ReadInt( input + 8 + index * 8 );
	const int32_t length = Q3TBSP_ReadInt( input + 12 + index * 8 );
	if ( !Q3TBSP_Range( offset, length, inputLength ) || ( length && offset < headerBytes ) ) return qfalse;
	if ( recordSize > 1 && length % recordSize ) return qfalse;
	lump->data = input + offset;
	lump->length = length;
	lump->count = recordSize > 1 ? length / recordSize : length;
	return qtrue;
}

/* Output assembly: seventeen lumps, each four-byte aligned. */
typedef struct {
	int32_t	offsets[HEADER_LUMPS];
	int32_t	lengths[HEADER_LUMPS];
	int32_t	total;
} q3tBspLayout_t;

static qboolean Q3TBSP_Layout( q3tBspLayout_t *layout ) {
	int64_t cursor = sizeof( dheader_t );
	int i;
	for ( i = 0; i < HEADER_LUMPS; ++i ) {
		if ( layout->lengths[i] < 0 ) return qfalse;
		cursor = ( cursor + 3 ) & ~(int64_t)3;
		layout->offsets[i] = (int32_t)cursor;
		cursor += layout->lengths[i];
		if ( cursor > INT_MAX ) return qfalse;
	}
	layout->total = (int32_t)cursor;
	return qtrue;
}

static unsigned char *Q3TBSP_Begin( const q3tBspLayout_t *layout, void *(*allocFn)( int ) ) {
	unsigned char *output = (unsigned char *)allocFn( layout->total );
	int i;
	if ( !output ) return NULL;
	memset( output, 0, (size_t)layout->total );
	Q3TBSP_WriteInt( output, BSP_IDENT );
	Q3TBSP_WriteInt( output + 4, BSP_VERSION );
	for ( i = 0; i < HEADER_LUMPS; ++i ) {
		Q3TBSP_WriteInt( output + 8 + i * 8, layout->offsets[i] );
		Q3TBSP_WriteInt( output + 12 + i * 8, layout->lengths[i] );
	}
	return output;
}

/* Planes lose their fifth word; the rest is Quake III's. */
static void Q3TBSP_Planes( const q3tBspLump_t *planes, unsigned char *out ) {
	int32_t i;
	for ( i = 0; i < planes->count; ++i ) memcpy( out + i * 16, planes->data + i * 20, 16 );
}

/* A fog's visible side is the one ray tests clip against. Q3Test's records
 * have none; its q3map wrote fog brushes with the six axial sides first, and
 * the top, side 5, is the plane the retail renderers clip at (fs_bsp.md). */
static int32_t Q3TBSP_FogSide( int32_t brushSides ) {
	return brushSides >= 6 ? 5 : -1;
}

static qboolean Q3TBSP_Translate45( const unsigned char *input, int inputLength, void *(*allocFn)( int ),
	q3tBspTranslation_t *out, char *error, size_t errorSize ) {
	static const int recordSizes[Q3TEST_BSP_LUMPS_45] = {
		1, sizeof( dshader_t ), 20, sizeof( dnode_t ), sizeof( dleaf_t ), 4, 4, 56,
		sizeof( dbrush_t ), sizeof( dbrushside_t ), sizeof( drawVert_t ), 4, 68,
		sizeof( dsurface_t ), LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3, 8, 1
	};
	q3tBspLump_t lumps[Q3TEST_BSP_LUMPS_45];
	q3tBspLayout_t layout;
	unsigned char *output;
	int i;
	const int headerBytes = 8 + Q3TEST_BSP_LUMPS_45 * 8;

	if ( inputLength < headerBytes ) { Q3TBSP_Error( error, errorSize, "truncated IBSP 45 header" ); return qfalse; }
	for ( i = 0; i < Q3TEST_BSP_LUMPS_45; ++i ) {
		if ( !Q3TBSP_Lump( input, inputLength, headerBytes, i, recordSizes[i], &lumps[i] ) ) {
			Q3TBSP_Error( error, errorSize, "IBSP 45 lump is out of range or not whole records" );
			return qfalse;
		}
	}
	/* Every brush a fog names and every model range must be in bounds. */
	for ( i = 0; i < lumps[12].count; ++i ) {
		const int32_t brush = Q3TBSP_ReadInt( lumps[12].data + i * 68 + MAX_QPATH );
		if ( brush < 0 || brush >= lumps[8].count || !memchr( lumps[12].data + i * 68, 0, MAX_QPATH ) ) {
			Q3TBSP_Error( error, errorSize, "IBSP 45 fog names an invalid brush" ); return qfalse;
		}
	}
	for ( i = 0; i < lumps[7].count; ++i ) {
		const unsigned char *model = lumps[7].data + i * 56;
		if ( !Q3TBSP_Range( Q3TBSP_ReadInt( model + 40 ), Q3TBSP_ReadInt( model + 44 ), lumps[13].count )
			|| !Q3TBSP_Range( Q3TBSP_ReadInt( model + 48 ), Q3TBSP_ReadInt( model + 52 ), lumps[8].count ) ) {
			Q3TBSP_Error( error, errorSize, "IBSP 45 model range is out of bounds" ); return qfalse;
		}
	}
	for ( i = 0; i < HEADER_LUMPS; ++i ) layout.lengths[i] = lumps[i].length;
	layout.lengths[LUMP_PLANES] = lumps[2].count * (int32_t)sizeof( dplane_t );
	layout.lengths[LUMP_MODELS] = lumps[7].count * (int32_t)sizeof( dmodel_t );
	layout.lengths[LUMP_FOGS] = lumps[12].count * (int32_t)sizeof( dfog_t );
	if ( !Q3TBSP_Layout( &layout ) || !( output = Q3TBSP_Begin( &layout, allocFn ) ) ) {
		Q3TBSP_Error( error, errorSize, "IBSP 45 translation is too large" ); return qfalse;
	}
	for ( i = 0; i < HEADER_LUMPS; ++i ) {
		if ( i == LUMP_PLANES || i == LUMP_MODELS || i == LUMP_FOGS ) continue;
		memcpy( output + layout.offsets[i], lumps[i].data, (size_t)lumps[i].length );
	}
	Q3TBSP_Planes( &lumps[2], output + layout.offsets[LUMP_PLANES] );
	for ( i = 0; i < lumps[7].count; ++i ) {
		const unsigned char *model = lumps[7].data + i * 56;
		unsigned char *target = output + layout.offsets[LUMP_MODELS] + i * (int)sizeof( dmodel_t );
		memcpy( target, model, 24 );            /* mins, maxs */
		memcpy( target + 24, model + 40, 16 );  /* surfaces, brushes; origin and head node dropped */
	}
	for ( i = 0; i < lumps[12].count; ++i ) {
		const unsigned char *fog = lumps[12].data + i * 68;
		unsigned char *target = output + layout.offsets[LUMP_FOGS] + i * (int)sizeof( dfog_t );
		const int32_t brush = Q3TBSP_ReadInt( fog + MAX_QPATH );
		memcpy( target, fog, 68 );
		Q3TBSP_WriteInt( target + 68, Q3TBSP_FogSide( Q3TBSP_ReadInt( lumps[8].data + brush * 12 + 4 ) ) );
	}
	out->data = output;
	out->length = layout.total;
	return qtrue;
}

/* ---- IBSP 44 ---------------------------------------------------------- */

typedef struct {
	int32_t	*surfaceModel;      /* the model owning each surface, -1 before assignment */
	int32_t	*brushModel;
	int32_t	*surfaceOrder;      /* new index -> old index */
	int32_t	*surfaceRemap;      /* old index -> new index */
	int32_t	*brushOrder;
	int32_t	*brushRemap;
	int32_t	*modelFirstSurface, *modelNumSurfaces, *modelFirstBrush, *modelNumBrushes;
	int32_t	*stack;
	int32_t	*shaderOfBrush, *shaderOfSide, *shaderOfSurface;
	int32_t	*fanFirstIndex;
	dshader_t *shaders;
} q3tBsp44Work_t;

static void Q3TBSP_Release( void (*freeFn)( void * ), void *block ) {
	if ( block ) freeFn( block );
}

static void Q3TBSP_Free44( q3tBsp44Work_t *w, void (*freeFn)( void * ) ) {
	Q3TBSP_Release( freeFn, w->surfaceModel ); Q3TBSP_Release( freeFn, w->brushModel );
	Q3TBSP_Release( freeFn, w->surfaceOrder ); Q3TBSP_Release( freeFn, w->surfaceRemap );
	Q3TBSP_Release( freeFn, w->brushOrder ); Q3TBSP_Release( freeFn, w->brushRemap );
	Q3TBSP_Release( freeFn, w->modelFirstSurface ); Q3TBSP_Release( freeFn, w->modelNumSurfaces );
	Q3TBSP_Release( freeFn, w->modelFirstBrush ); Q3TBSP_Release( freeFn, w->modelNumBrushes );
	Q3TBSP_Release( freeFn, w->stack );
	Q3TBSP_Release( freeFn, w->shaderOfBrush ); Q3TBSP_Release( freeFn, w->shaderOfSide );
	Q3TBSP_Release( freeFn, w->shaderOfSurface ); Q3TBSP_Release( freeFn, w->fanFirstIndex );
	Q3TBSP_Release( freeFn, w->shaders );
	memset( w, 0, sizeof( *w ) );
}

/* Zeroed scratch from the caller's allocator; NULL when the size overflows. */
static void *Q3TBSP_Scratch( void *(*allocFn)( int ), int64_t count, size_t size ) {
	void *block;
	if ( count < 0 || (uint64_t)count * size > (uint64_t)INT_MAX - 16 ) return NULL;
	block = allocFn( (int)( ( count ? count : 1 ) * (int64_t)size ) );
	if ( block ) memset( block, 0, (size_t)( ( count ? count : 1 ) * (int64_t)size ) );
	return block;
}

/* Claims every surface and brush the model's subtree reaches for that model.
 * A leaf index and a node index are checked before use; a node reached twice
 * within one model is a malformed tree. */
static qboolean Q3TBSP_ClaimModel( const q3tBspLump_t *lumps, q3tBsp44Work_t *w, int32_t model, int32_t headnode ) {
	const int32_t nodes = lumps[Q3T44_NODES].count, leafs = lumps[Q3T44_LEAFS].count;
	int32_t depth = 0, visited = 0;
	w->stack[depth++] = headnode;
	while ( depth ) {
		const int32_t node = w->stack[--depth];
		if ( ++visited > nodes + leafs ) return qfalse;
		if ( node < 0 ) {
			const int32_t leaf = -1 - node;
			const unsigned char *record;
			int32_t first, count, j;
			if ( leaf < 0 || leaf >= leafs ) return qfalse;
			record = lumps[Q3T44_LEAFS].data + leaf * (int32_t)sizeof( dleaf_t );
			first = Q3TBSP_ReadInt( record + 32 ); count = Q3TBSP_ReadInt( record + 36 );
			if ( !Q3TBSP_Range( first, count, lumps[Q3T44_LEAFSURFACES].count ) ) return qfalse;
			for ( j = 0; j < count; ++j ) {
				const int32_t surface = Q3TBSP_ReadInt( lumps[Q3T44_LEAFSURFACES].data + ( first + j ) * 4 );
				if ( surface < 0 || surface >= lumps[Q3T44_SURFACES].count ) return qfalse;
				if ( w->surfaceModel[surface] >= 0 && w->surfaceModel[surface] != model ) return qfalse;
				w->surfaceModel[surface] = model;
			}
			first = Q3TBSP_ReadInt( record + 40 ); count = Q3TBSP_ReadInt( record + 44 );
			if ( !Q3TBSP_Range( first, count, lumps[Q3T44_LEAFBRUSHES].count ) ) return qfalse;
			for ( j = 0; j < count; ++j ) {
				const int32_t brush = Q3TBSP_ReadInt( lumps[Q3T44_LEAFBRUSHES].data + ( first + j ) * 4 );
				if ( brush < 0 || brush >= lumps[Q3T44_BRUSHES].count ) return qfalse;
				if ( w->brushModel[brush] >= 0 && w->brushModel[brush] != model ) return qfalse;
				w->brushModel[brush] = model;
			}
			continue;
		}
		if ( node >= nodes || depth + 2 > nodes + leafs + 1 ) return qfalse;
		{
			const unsigned char *record = lumps[Q3T44_NODES].data + node * (int32_t)sizeof( dnode_t );
			w->stack[depth++] = Q3TBSP_ReadInt( record + 4 );
			w->stack[depth++] = Q3TBSP_ReadInt( record + 8 );
		}
	}
	return qtrue;
}

/* Orders surfaces or brushes so that each model's are contiguous, keeping
 * their original relative order: the world's first, then each submodel's. */
static void Q3TBSP_Order( const int32_t *owner, int32_t count, int32_t models,
	int32_t *order, int32_t *remap, int32_t *first, int32_t *number ) {
	int32_t model, i, next = 0;
	for ( model = 0; model < models; ++model ) {
		first[model] = next;
		for ( i = 0; i < count; ++i ) {
			if ( owner[i] != model ) continue;
			order[next] = i; remap[i] = next; ++next;
		}
		number[model] = next - first[model];
	}
}

/* 1.02-1.05 give every patch solid contents and no surface flags, except a
 * patch drawn with this one shader, which collides with nothing (1.02
 * CMod_LoadPatches 0x4086ac-0x4086e7, an exact name compare). */
static int32_t Q3TBSP_PatchContents( const char *shader ) {
	return strcmp( shader, "textures/sfx/powerupshit" ) ? 1 : 0;
}

static int32_t Q3TBSP_Shader( dshader_t *shaders, int32_t *count, int32_t capacity,
	const char *name, int32_t surfaceFlags, int32_t contentFlags ) {
	int32_t i;
	for ( i = 0; i < *count; ++i ) {
		if ( shaders[i].surfaceFlags == surfaceFlags && shaders[i].contentFlags == contentFlags
			&& !strncmp( shaders[i].shader, name, MAX_QPATH ) ) return i;
	}
	if ( *count >= capacity ) return -1;
	memset( &shaders[*count], 0, sizeof( shaders[*count] ) );
	strncpy( shaders[*count].shader, name, MAX_QPATH - 1 );
	shaders[*count].surfaceFlags = surfaceFlags;
	shaders[*count].contentFlags = contentFlags;
	return ( *count )++;
}

static qboolean Q3TBSP_Translate44( const unsigned char *input, int inputLength, void *(*allocFn)( int ),
	void (*freeFn)( void * ), q3tBspTranslation_t *out, char *error, size_t errorSize ) {
	static const int recordSizes[Q3TEST_BSP_LUMPS_44] = {
		1, 20, sizeof( dnode_t ), sizeof( dleaf_t ), 4, 4, 48, 12, 8,
		LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3, 1, sizeof( drawVert_t ), 164, 68, 4
	};
	q3tBspLump_t lumps[Q3TEST_BSP_LUMPS_44];
	q3tBspLayout_t layout;
	q3tBsp44Work_t w;
	unsigned char *output = NULL;
	int32_t i, j, models, surfaces, brushes, sides, verts, shaderCount = 0, shaderCapacity, fanIndexes = 0;
	const int headerBytes = 8 + Q3TEST_BSP_LUMPS_44 * 8;
	qboolean ok = qfalse;

	memset( &w, 0, sizeof( w ) );
	if ( inputLength < headerBytes ) { Q3TBSP_Error( error, errorSize, "truncated IBSP 44 header" ); return qfalse; }
	for ( i = 0; i < Q3TEST_BSP_LUMPS_44; ++i ) {
		if ( !Q3TBSP_Lump( input, inputLength, headerBytes, i, recordSizes[i], &lumps[i] ) ) {
			Q3TBSP_Error( error, errorSize, "IBSP 44 lump is out of range or not whole records" );
			return qfalse;
		}
	}
	models = lumps[Q3T44_MODELS].count; surfaces = lumps[Q3T44_SURFACES].count;
	brushes = lumps[Q3T44_BRUSHES].count; sides = lumps[Q3T44_BRUSHSIDES].count;
	verts = lumps[Q3T44_DRAWVERTS].count;
	if ( models < 1 || models > MAX_MAP_MODELS ) { Q3TBSP_Error( error, errorSize, "IBSP 44 has no world model" ); return qfalse; }
	shaderCapacity = brushes + sides + surfaces;
	w.surfaceModel = (int32_t *)Q3TBSP_Scratch( allocFn, surfaces, sizeof( int32_t ) );
	w.brushModel = (int32_t *)Q3TBSP_Scratch( allocFn, brushes, sizeof( int32_t ) );
	w.surfaceOrder = (int32_t *)Q3TBSP_Scratch( allocFn, surfaces, sizeof( int32_t ) );
	w.surfaceRemap = (int32_t *)Q3TBSP_Scratch( allocFn, surfaces, sizeof( int32_t ) );
	w.brushOrder = (int32_t *)Q3TBSP_Scratch( allocFn, brushes, sizeof( int32_t ) );
	w.brushRemap = (int32_t *)Q3TBSP_Scratch( allocFn, brushes, sizeof( int32_t ) );
	w.modelFirstSurface = (int32_t *)Q3TBSP_Scratch( allocFn, models, sizeof( int32_t ) );
	w.modelNumSurfaces = (int32_t *)Q3TBSP_Scratch( allocFn, models, sizeof( int32_t ) );
	w.modelFirstBrush = (int32_t *)Q3TBSP_Scratch( allocFn, models, sizeof( int32_t ) );
	w.modelNumBrushes = (int32_t *)Q3TBSP_Scratch( allocFn, models, sizeof( int32_t ) );
	w.stack = (int32_t *)Q3TBSP_Scratch( allocFn, (int64_t)lumps[Q3T44_NODES].count + lumps[Q3T44_LEAFS].count + 2, sizeof( int32_t ) );
	w.shaderOfBrush = (int32_t *)Q3TBSP_Scratch( allocFn, brushes, sizeof( int32_t ) );
	w.shaderOfSide = (int32_t *)Q3TBSP_Scratch( allocFn, sides, sizeof( int32_t ) );
	w.shaderOfSurface = (int32_t *)Q3TBSP_Scratch( allocFn, surfaces, sizeof( int32_t ) );
	w.fanFirstIndex = (int32_t *)Q3TBSP_Scratch( allocFn, surfaces, sizeof( int32_t ) );
	w.shaders = (dshader_t *)Q3TBSP_Scratch( allocFn, shaderCapacity, sizeof( dshader_t ) );
	if ( !w.surfaceModel || !w.brushModel || !w.surfaceOrder || !w.surfaceRemap || !w.brushOrder || !w.brushRemap
		|| !w.modelFirstSurface || !w.modelNumSurfaces || !w.modelFirstBrush || !w.modelNumBrushes || !w.stack
		|| !w.shaderOfBrush || !w.shaderOfSide || !w.shaderOfSurface || !w.fanFirstIndex || !w.shaders ) {
		Q3TBSP_Error( error, errorSize, "IBSP 44 translation is too large" ); goto cleanup;
	}
	for ( i = 0; i < surfaces; ++i ) w.surfaceModel[i] = -1;
	for ( i = 0; i < brushes; ++i ) w.brushModel[i] = -1;
	/* Submodels claim their subtrees; the world keeps whatever remains. */
	for ( i = 1; i < models; ++i ) {
		const int32_t headnode = Q3TBSP_ReadInt( lumps[Q3T44_MODELS].data + i * 48 + 36 );
		if ( !Q3TBSP_ClaimModel( lumps, &w, i, headnode ) ) {
			Q3TBSP_Error( error, errorSize, "IBSP 44 submodel tree is malformed or shared" ); goto cleanup;
		}
	}
	for ( i = 0; i < surfaces; ++i ) if ( w.surfaceModel[i] < 0 ) w.surfaceModel[i] = 0;
	for ( i = 0; i < brushes; ++i ) if ( w.brushModel[i] < 0 ) w.brushModel[i] = 0;
	Q3TBSP_Order( w.surfaceModel, surfaces, models, w.surfaceOrder, w.surfaceRemap, w.modelFirstSurface, w.modelNumSurfaces );
	Q3TBSP_Order( w.brushModel, brushes, models, w.brushOrder, w.brushRemap, w.modelFirstBrush, w.modelNumBrushes );

	/* Brushes and their sides keep their own flags through synthesized
	 * shaders; surfaces name theirs. None of these names is ever loaded as a
	 * texture by collision; the renderers load only the surfaces' names. */
	for ( i = 0; i < brushes; ++i ) {
		const unsigned char *brush = lumps[Q3T44_BRUSHES].data + i * 12;
		if ( !Q3TBSP_Range( Q3TBSP_ReadInt( brush ), Q3TBSP_ReadInt( brush + 4 ), sides ) ) {
			Q3TBSP_Error( error, errorSize, "IBSP 44 brush sides are out of range" ); goto cleanup;
		}
		w.shaderOfBrush[i] = Q3TBSP_Shader( w.shaders, &shaderCount, shaderCapacity, "noshader", 0, Q3TBSP_ReadInt( brush + 8 ) );
	}
	for ( i = 0; i < sides; ++i ) {
		const unsigned char *side = lumps[Q3T44_BRUSHSIDES].data + i * 8;
		if ( Q3TBSP_ReadInt( side ) < 0 || Q3TBSP_ReadInt( side ) >= lumps[Q3T44_PLANES].count ) {
			Q3TBSP_Error( error, errorSize, "IBSP 44 brush side plane is out of range" ); goto cleanup;
		}
		w.shaderOfSide[i] = Q3TBSP_Shader( w.shaders, &shaderCount, shaderCapacity, "noshader", Q3TBSP_ReadInt( side + 4 ), 0 );
	}
	for ( i = 0; i < surfaces; ++i ) {
		const unsigned char *surface = lumps[Q3T44_SURFACES].data + i * 164;
		const int32_t firstVert = Q3TBSP_ReadInt( surface + 72 ), numVerts = Q3TBSP_ReadInt( surface + 76 );
		const int32_t firstIndex = Q3TBSP_ReadInt( surface + 80 ), numIndexes = Q3TBSP_ReadInt( surface + 84 );
		const int32_t width = Q3TBSP_ReadInt( surface + 88 ), height = Q3TBSP_ReadInt( surface + 92 );
		const int32_t fog = Q3TBSP_ReadInt( surface + 64 );
		char name[MAX_QPATH];
		if ( !memchr( surface, 0, MAX_QPATH ) || !Q3TBSP_Range( firstVert, numVerts, verts )
			|| fog < -1 || fog >= lumps[Q3T44_FOGS].count ) {
			Q3TBSP_Error( error, errorSize, "IBSP 44 surface is malformed" ); goto cleanup;
		}
		if ( width || height ) {
			if ( width < 3 || height < 3 || width > 65 || height > 65 || (int64_t)width * height != numVerts ) {
				Q3TBSP_Error( error, errorSize, "IBSP 44 patch size is invalid" ); goto cleanup;
			}
		} else if ( numIndexes ) {
			if ( !Q3TBSP_Range( firstIndex, numIndexes, lumps[Q3T44_DRAWINDEXES].count ) || numIndexes % 3 ) {
				Q3TBSP_Error( error, errorSize, "IBSP 44 triangle indexes are out of range" ); goto cleanup;
			}
			for ( j = 0; j < numIndexes; ++j ) {
				const int32_t index = Q3TBSP_ReadInt( lumps[Q3T44_DRAWINDEXES].data + ( firstIndex + j ) * 4 );
				if ( index < 0 || index >= numVerts ) {
					Q3TBSP_Error( error, errorSize, "IBSP 44 triangle index is out of range" ); goto cleanup;
				}
			}
		} else if ( numVerts >= 3 ) {
			/* A planar face is a fan over its points. */
			w.fanFirstIndex[i] = lumps[Q3T44_DRAWINDEXES].count + fanIndexes;
			if ( fanIndexes > INT_MAX / 4 - ( numVerts - 2 ) * 3 - lumps[Q3T44_DRAWINDEXES].count ) {
				Q3TBSP_Error( error, errorSize, "IBSP 44 translation is too large" ); goto cleanup;
			}
			fanIndexes += ( numVerts - 2 ) * 3;
		}
		memcpy( name, surface, MAX_QPATH );
		w.shaderOfSurface[i] = Q3TBSP_Shader( w.shaders, &shaderCount, shaderCapacity, name, 0,
			( width || height ) ? Q3TBSP_PatchContents( name ) : 0 );
	}
	for ( i = 0; i < lumps[Q3T44_FOGS].count; ++i ) {
		const int32_t brush = Q3TBSP_ReadInt( lumps[Q3T44_FOGS].data + i * 68 + MAX_QPATH );
		if ( brush < 0 || brush >= brushes || !memchr( lumps[Q3T44_FOGS].data + i * 68, 0, MAX_QPATH ) ) {
			Q3TBSP_Error( error, errorSize, "IBSP 44 fog names an invalid brush" ); goto cleanup;
		}
	}
	for ( i = 0; i < lumps[Q3T44_LEAFSURFACES].count; ++i ) {
		const int32_t surface = Q3TBSP_ReadInt( lumps[Q3T44_LEAFSURFACES].data + i * 4 );
		if ( surface < 0 || surface >= surfaces ) { Q3TBSP_Error( error, errorSize, "IBSP 44 leaf surface is out of range" ); goto cleanup; }
	}
	for ( i = 0; i < lumps[Q3T44_LEAFBRUSHES].count; ++i ) {
		const int32_t brush = Q3TBSP_ReadInt( lumps[Q3T44_LEAFBRUSHES].data + i * 4 );
		if ( brush < 0 || brush >= brushes ) { Q3TBSP_Error( error, errorSize, "IBSP 44 leaf brush is out of range" ); goto cleanup; }
	}

	memset( &layout, 0, sizeof( layout ) );
	layout.lengths[LUMP_ENTITIES] = lumps[Q3T44_ENTITIES].length;
	layout.lengths[LUMP_SHADERS] = shaderCount * (int32_t)sizeof( dshader_t );
	layout.lengths[LUMP_PLANES] = lumps[Q3T44_PLANES].count * (int32_t)sizeof( dplane_t );
	layout.lengths[LUMP_NODES] = lumps[Q3T44_NODES].length;
	layout.lengths[LUMP_LEAFS] = lumps[Q3T44_LEAFS].length;
	layout.lengths[LUMP_LEAFSURFACES] = lumps[Q3T44_LEAFSURFACES].length;
	layout.lengths[LUMP_LEAFBRUSHES] = lumps[Q3T44_LEAFBRUSHES].length;
	layout.lengths[LUMP_MODELS] = models * (int32_t)sizeof( dmodel_t );
	layout.lengths[LUMP_BRUSHES] = brushes * (int32_t)sizeof( dbrush_t );
	layout.lengths[LUMP_BRUSHSIDES] = sides * (int32_t)sizeof( dbrushside_t );
	layout.lengths[LUMP_DRAWVERTS] = lumps[Q3T44_DRAWVERTS].length;
	layout.lengths[LUMP_DRAWINDEXES] = ( lumps[Q3T44_DRAWINDEXES].count + fanIndexes ) * 4;
	layout.lengths[LUMP_FOGS] = lumps[Q3T44_FOGS].count * (int32_t)sizeof( dfog_t );
	layout.lengths[LUMP_SURFACES] = surfaces * (int32_t)sizeof( dsurface_t );
	layout.lengths[LUMP_LIGHTMAPS] = lumps[Q3T44_LIGHTMAPS].length;
	layout.lengths[LUMP_LIGHTGRID] = 0;
	layout.lengths[LUMP_VISIBILITY] = lumps[Q3T44_VISIBILITY].length;
	if ( !Q3TBSP_Layout( &layout ) || !( output = Q3TBSP_Begin( &layout, allocFn ) ) ) {
		Q3TBSP_Error( error, errorSize, "IBSP 44 translation is too large" ); goto cleanup;
	}
	memcpy( output + layout.offsets[LUMP_ENTITIES], lumps[Q3T44_ENTITIES].data, (size_t)lumps[Q3T44_ENTITIES].length );
	for ( i = 0; i < shaderCount; ++i ) {
		unsigned char *target = output + layout.offsets[LUMP_SHADERS] + i * (int32_t)sizeof( dshader_t );
		memcpy( target, w.shaders[i].shader, MAX_QPATH );
		Q3TBSP_WriteInt( target + MAX_QPATH, w.shaders[i].surfaceFlags );
		Q3TBSP_WriteInt( target + MAX_QPATH + 4, w.shaders[i].contentFlags );
	}
	Q3TBSP_Planes( &lumps[Q3T44_PLANES], output + layout.offsets[LUMP_PLANES] );
	memcpy( output + layout.offsets[LUMP_NODES], lumps[Q3T44_NODES].data, (size_t)lumps[Q3T44_NODES].length );
	memcpy( output + layout.offsets[LUMP_LEAFS], lumps[Q3T44_LEAFS].data, (size_t)lumps[Q3T44_LEAFS].length );
	for ( i = 0; i < lumps[Q3T44_LEAFSURFACES].count; ++i ) {
		const int32_t surface = Q3TBSP_ReadInt( lumps[Q3T44_LEAFSURFACES].data + i * 4 );
		Q3TBSP_WriteInt( output + layout.offsets[LUMP_LEAFSURFACES] + i * 4, w.surfaceRemap[surface] );
	}
	for ( i = 0; i < lumps[Q3T44_LEAFBRUSHES].count; ++i ) {
		const int32_t brush = Q3TBSP_ReadInt( lumps[Q3T44_LEAFBRUSHES].data + i * 4 );
		Q3TBSP_WriteInt( output + layout.offsets[LUMP_LEAFBRUSHES] + i * 4, w.brushRemap[brush] );
	}
	for ( i = 0; i < models; ++i ) {
		const unsigned char *model = lumps[Q3T44_MODELS].data + i * 48;
		unsigned char *target = output + layout.offsets[LUMP_MODELS] + i * (int32_t)sizeof( dmodel_t );
		memcpy( target, model, 24 );
		Q3TBSP_WriteInt( target + 24, w.modelFirstSurface[i] );
		Q3TBSP_WriteInt( target + 28, w.modelNumSurfaces[i] );
		Q3TBSP_WriteInt( target + 32, w.modelFirstBrush[i] );
		Q3TBSP_WriteInt( target + 36, w.modelNumBrushes[i] );
	}
	/* A brush's sides stay where they were; only brushes move. */
	for ( i = 0; i < brushes; ++i ) {
		const int32_t source = w.brushOrder[i];
		const unsigned char *brush = lumps[Q3T44_BRUSHES].data + source * 12;
		unsigned char *target = output + layout.offsets[LUMP_BRUSHES] + i * (int32_t)sizeof( dbrush_t );
		Q3TBSP_WriteInt( target, Q3TBSP_ReadInt( brush ) );
		Q3TBSP_WriteInt( target + 4, Q3TBSP_ReadInt( brush + 4 ) );
		Q3TBSP_WriteInt( target + 8, w.shaderOfBrush[source] );
	}
	for ( i = 0; i < sides; ++i ) {
		unsigned char *target = output + layout.offsets[LUMP_BRUSHSIDES] + i * (int32_t)sizeof( dbrushside_t );
		Q3TBSP_WriteInt( target, Q3TBSP_ReadInt( lumps[Q3T44_BRUSHSIDES].data + i * 8 ) );
		Q3TBSP_WriteInt( target + 4, w.shaderOfSide[i] );
	}
	memcpy( output + layout.offsets[LUMP_DRAWVERTS], lumps[Q3T44_DRAWVERTS].data, (size_t)lumps[Q3T44_DRAWVERTS].length );
	memcpy( output + layout.offsets[LUMP_DRAWINDEXES], lumps[Q3T44_DRAWINDEXES].data, (size_t)lumps[Q3T44_DRAWINDEXES].length );
	for ( i = 0; i < lumps[Q3T44_FOGS].count; ++i ) {
		const unsigned char *fog = lumps[Q3T44_FOGS].data + i * 68;
		unsigned char *target = output + layout.offsets[LUMP_FOGS] + i * (int32_t)sizeof( dfog_t );
		const int32_t brush = Q3TBSP_ReadInt( fog + MAX_QPATH );
		memcpy( target, fog, MAX_QPATH );
		Q3TBSP_WriteInt( target + MAX_QPATH, w.brushRemap[brush] );
		Q3TBSP_WriteInt( target + MAX_QPATH + 4, Q3TBSP_FogSide( Q3TBSP_ReadInt( lumps[Q3T44_BRUSHES].data + brush * 12 + 4 ) ) );
	}
	for ( i = 0; i < surfaces; ++i ) {
		const int32_t source = w.surfaceOrder[i];
		const unsigned char *surface = lumps[Q3T44_SURFACES].data + source * 164;
		unsigned char *target = output + layout.offsets[LUMP_SURFACES] + i * (int32_t)sizeof( dsurface_t );
		const int32_t firstVert = Q3TBSP_ReadInt( surface + 72 ), numVerts = Q3TBSP_ReadInt( surface + 76 );
		const int32_t numIndexes = Q3TBSP_ReadInt( surface + 84 );
		const int32_t width = Q3TBSP_ReadInt( surface + 88 ), height = Q3TBSP_ReadInt( surface + 92 );
		int32_t type = MST_PLANAR, first = w.fanFirstIndex[source], count = numVerts >= 3 ? ( numVerts - 2 ) * 3 : 0;
		if ( width || height ) { type = MST_PATCH; first = 0; count = 0; }
		else if ( numIndexes ) { type = MST_TRIANGLE_SOUP; first = Q3TBSP_ReadInt( surface + 80 ); count = numIndexes; }
		else if ( numVerts < 3 ) { first = 0; count = 0; }
		Q3TBSP_WriteInt( target, w.shaderOfSurface[source] );
		Q3TBSP_WriteInt( target + 4, Q3TBSP_ReadInt( surface + 64 ) );
		Q3TBSP_WriteInt( target + 8, type );
		Q3TBSP_WriteInt( target + 12, firstVert );
		Q3TBSP_WriteInt( target + 16, numVerts );
		Q3TBSP_WriteInt( target + 20, first );
		Q3TBSP_WriteInt( target + 24, count );
		memcpy( target + 28, surface + 96, 20 );    /* lightmap number and rectangle */
		memcpy( target + 48, surface + 116, 48 );   /* lightmap origin and vectors */
		Q3TBSP_WriteInt( target + 96, width );
		Q3TBSP_WriteInt( target + 100, height );
		if ( type == MST_PLANAR && count ) {
			unsigned char *indexes = output + layout.offsets[LUMP_DRAWINDEXES] + first * 4;
			for ( j = 0; j < numVerts - 2; ++j ) {
				Q3TBSP_WriteInt( indexes + j * 12, 0 );
				Q3TBSP_WriteInt( indexes + j * 12 + 4, j + 1 );
				Q3TBSP_WriteInt( indexes + j * 12 + 8, j + 2 );
			}
		}
		if ( type == MST_PATCH ) {
			/* IBSP 46 keeps a patch's level-of-detail bounds in its first two
			 * lightmap vectors; IBSP 44 leaves them zero. */
			float mins[3], maxs[3];
			int k;
			for ( k = 0; k < 3; ++k ) {
				mins[k] = maxs[k] = Q3TBSP_ReadFloat( lumps[Q3T44_DRAWVERTS].data + firstVert * (int32_t)sizeof( drawVert_t ) + k * 4 );
			}
			for ( j = 1; j < numVerts; ++j ) {
				const unsigned char *vertex = lumps[Q3T44_DRAWVERTS].data + ( firstVert + j ) * (int32_t)sizeof( drawVert_t );
				for ( k = 0; k < 3; ++k ) {
					const float value = Q3TBSP_ReadFloat( vertex + k * 4 );
					if ( value < mins[k] ) mins[k] = value;
					if ( value > maxs[k] ) maxs[k] = value;
				}
			}
			for ( k = 0; k < 3; ++k ) {
				Q3TBSP_WriteFloat( target + 60 + k * 4, mins[k] );
				Q3TBSP_WriteFloat( target + 72 + k * 4, maxs[k] );
			}
		}
	}
	memcpy( output + layout.offsets[LUMP_LIGHTMAPS], lumps[Q3T44_LIGHTMAPS].data, (size_t)lumps[Q3T44_LIGHTMAPS].length );
	memcpy( output + layout.offsets[LUMP_VISIBILITY], lumps[Q3T44_VISIBILITY].data, (size_t)lumps[Q3T44_VISIBILITY].length );
	out->data = output;
	out->length = layout.total;
	output = NULL;
	ok = qtrue;
cleanup:
	Q3TBSP_Free44( &w, freeFn );
	return ok;
}

/* Translates a Q3Test map into a new IBSP 46 image from allocFn, which the
 * caller releases with freeFn; scratch comes from the same pair. On failure
 * nothing stays allocated and error names the first defect. */
static qboolean Q3TestBSP_TranslateToV46( const void *inputData, int inputLength, void *(*allocFn)( int ),
	void (*freeFn)( void * ), q3tBspTranslation_t *out, char *error, size_t errorSize ) {
	const unsigned char *input = (const unsigned char *)inputData;
	int32_t version;
	if ( out ) { out->data = NULL; out->length = 0; }
	if ( !input || !allocFn || !freeFn || !out || inputLength < 8 ) {
		Q3TBSP_Error( error, errorSize, "missing map data" ); return qfalse;
	}
	if ( Q3TBSP_ReadInt( input ) != BSP_IDENT ) { Q3TBSP_Error( error, errorSize, "not an IBSP map" ); return qfalse; }
	version = Q3TBSP_ReadInt( input + 4 );
	if ( version == Q3TEST_BSP_VERSION_45 ) return Q3TBSP_Translate45( input, inputLength, allocFn, out, error, errorSize );
	if ( version == Q3TEST_BSP_VERSION_44 ) return Q3TBSP_Translate44( input, inputLength, allocFn, freeFn, out, error, errorSize );
	Q3TBSP_Error( error, errorSize, "not a Q3Test map version" );
	return qfalse;
}

#endif
