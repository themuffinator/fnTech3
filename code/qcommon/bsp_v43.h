/*
===========================================================================
IBSP v43 compatibility

This translator is adapted from Spearmint's Q3 IHV BSP loader
(`code/qcommon/bsp_q3ihv.c`) so fnTech3 can feed IHV maps through
the existing retail Quake III collision and renderer loaders.
Public Q3Test builds use different BSP versions. Map translation does not
establish compatibility with the IHV executable, modules, network or demos.
===========================================================================
*/
#ifndef __BSP_V43_H__
#define __BSP_V43_H__

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include "qfiles.h"
#include "surfaceflags.h"

typedef struct {
	void	*data;
	int		length;
} bsp43_translation_t;

typedef struct {
	int firstSurface;
	int numSurfaces;
	int firstBrush;
	int numBrushes;
} bsp43_modelinfo_t;

static qboolean BSP43_RangeValid( int first, int count, int capacity ) {
	return first >= 0 && count >= 0 && first <= capacity && count <= capacity - first
		? qtrue : qfalse;
}

static qboolean BSP43_AppendLump( int *length, int bytes, int *offset ) {
	int aligned;
	if ( !length || !offset || *length < 0 || bytes < 0 || *length > INT_MAX - 3 ) return qfalse;
	aligned = ( *length + 3 ) & ~3;
	if ( bytes > INT_MAX - aligned ) return qfalse;
	*offset = aligned;
	*length = aligned + bytes;
	return qtrue;
}

static void BSP43_SetError( char *error, const size_t errorSize, const char *message ) {
	if ( !error || !errorSize ) {
		return;
	}

	Q_strncpyz( error, message, (int)errorSize );
}

static qboolean BSP43_ValidateLump( const lump_t *lump, const int length ) {
	int32_t ofs;
	int32_t len;

	ofs = LittleLong( lump->fileofs );
	len = LittleLong( lump->filelen );

	return BSP43_RangeValid( ofs, len, length );
}

/* Normalize the byte directory before using typed records. Neither a caller's
 * buffer nor an on-disk lump offset has to be naturally aligned. The original
 * bytes remain unchanged, including for the map checksum. */
static byte *BSP43_AlignedInput( const void *input, const dheader43_t *header ) {
	dheader43_t normalized = *header;
	byte *copy;
	int size = sizeof( normalized ), i;
	for ( i = 0; i < HEADER_LUMPS_43; ++i ) {
		int offset;
		if ( !BSP43_AppendLump( &size, LittleLong( header->lumps[i].filelen ), &offset ) ) return NULL;
		normalized.lumps[i].fileofs = LittleLong( offset );
	}
	copy = (byte *)malloc( (size_t)size );
	if ( !copy ) return NULL;
	memset( copy, 0, (size_t)size );
	memcpy( copy, &normalized, sizeof( normalized ) );
	for ( i = 0; i < HEADER_LUMPS_43; ++i ) {
		memcpy( copy + LittleLong( normalized.lumps[i].fileofs ),
			(const byte *)input + LittleLong( header->lumps[i].fileofs ),
			(size_t)LittleLong( header->lumps[i].filelen ) );
	}
	return copy;
}

/* Inspect IEEE bits; isfinite() may be optimized away in fast-math builds. */
static qboolean BSP43_FiniteFloats( const void *values, int count ) {
	int i;
	for ( i = 0; i < count; ++i ) {
		uint32_t bits;
		memcpy( &bits, (const byte *)values + i * sizeof( bits ), sizeof( bits ) );
		if ( ( (uint32_t)LittleLong( bits ) & 0x7f800000u ) == 0x7f800000u ) return qfalse;
	}
	return qtrue;
}

/* All downstream renderers recursively set parents. Reject cycles and a tree
 * too deep for that shared traversal before publishing any translated data. */
static qboolean BSP43_NodeTreeValid( const dnode_t *nodes, int count ) {
	int *state;
	int *stack, *edge, start, depth, valid = 1;
	/* 0 = unseen, 1 = active, >= 2 = completed subtree height + 1. */
	state = (int *)calloc( (size_t)count, sizeof( *state ) );
	stack = (int *)malloc( (size_t)count * sizeof( *stack ) );
	edge = (int *)calloc( (size_t)count, sizeof( *edge ) );
	if ( !state || !stack || !edge ) { valid = 0; goto cleanup; }
	/* A shared internal subtree has no unique parent and can make the retail
	 * recursive walk exponential even without a cycle. Shared solid leaves
	 * are normal, so only count references to internal nodes. */
	for ( start = 0; start < count; ++start ) {
		int side;
		for ( side = 0; side < 2; ++side ) {
			int child = LittleLong( nodes[start].children[side] );
			if ( child >= 0 && ( child >= count || ++edge[child] > 1 ) ) {
				valid = 0; goto cleanup;
			}
		}
	}
	for ( start = 0; start < count && valid; ++start ) {
		if ( state[start] ) continue;
		depth = 1; stack[0] = start; edge[0] = 0; state[start] = 1;
		while ( depth && valid ) {
			const int node = stack[depth - 1];
			int child;
			if ( edge[depth - 1] == 2 ) {
				int side, height = 1;
				for ( side = 0; side < 2; ++side ) {
					child = LittleLong( nodes[node].children[side] );
					if ( child >= 0 ) height = MAX( height, state[child] );
				}
				if ( height > 2048 ) { valid = 0; break; }
				state[node] = height + 1; --depth; continue;
			}
			child = LittleLong( nodes[node].children[edge[depth - 1]++] );
			if ( child < 0 ) continue;
			if ( child >= count || state[child] == 1 || depth >= 2048 ) { valid = 0; break; }
			if ( state[child] >= 2 ) continue;
			stack[depth] = child; edge[depth++] = 0; state[child] = 1;
		}
	}
cleanup:
	free( state ); free( stack ); free( edge );
	return valid ? qtrue : qfalse;
}

static void BSP43_SetPatchBounds( const dsurface43_t *surface, const drawVert_t *drawVerts, dsurface_t *outSurface ) {
	vec3_t mins, maxs;
	int firstVert;
	int numVerts;
	int i;

	firstVert = LittleLong( surface->firstVert );
	numVerts = LittleLong( surface->numVerts );

	/*
	 * Preserve Spearmint's q3ihv patch-bound behavior so the renderer sees
	 * the same LoD inputs that its native loader would produce.
	 */
	mins[0] = 0.0f;
	mins[1] = 0.0f;
	mins[2] = 0.0f;
	maxs[0] = 0.0f;
	maxs[1] = 0.0f;
	maxs[2] = 0.0f;

	for ( i = 0; i < numVerts; i++ ) {
		const drawVert_t *point = &drawVerts[firstVert + i];
		float x = LittleFloat( point->xyz[0] );
		float y = LittleFloat( point->xyz[1] );
		float z = LittleFloat( point->xyz[2] );

		if ( x < mins[0] ) {
			mins[0] = x;
		}
		if ( y < mins[1] ) {
			mins[1] = y;
		}
		if ( z < mins[2] ) {
			mins[2] = z;
		}

		if ( x > maxs[0] ) {
			maxs[0] = x;
		}
		if ( y > maxs[1] ) {
			maxs[1] = y;
		}
		if ( z > maxs[2] ) {
			maxs[2] = z;
		}
	}
	for ( i = 0; i < 3; ++i ) {
		outSurface->lightmapVecs[0][i] = LittleFloat( mins[i] );
		outSurface->lightmapVecs[1][i] = LittleFloat( maxs[i] );
	}
}

static qboolean BSP43_TranslateToV46(
	const void *inputData,
	const int inputLength,
	void *(*allocFn)( int ),
	void (*freeFn)( void * ),
	bsp43_translation_t *translation,
	char *error,
	const size_t errorSize )
{
	const dheader43_t *header43;
	dheader43_t originalHeader;
	byte *alignedInput = NULL;
	const lump_t *lumps43;
	const dnode_t *nodes43;
	const dleaf_t *leafs43;
	const int *leafSurfaces43;
	const int *leafBrushes43;
	const dmodel43_t *models43;
	const dbrush43_t *brushes43;
	const dbrushside43_t *brushSides43;
	const drawVert_t *drawVerts43;
	const dsurface43_t *surfaces43;
	const dfog43_t *fogs43;
	bsp43_modelinfo_t *modelInfo = NULL;
	int *brushSideContents = NULL;
	int *surfaceShaderNum = NULL;
	int *surfaceFirstIndex = NULL;
	int *surfaceNumIndexes = NULL;
	int *modelBrushes = NULL;
	int translatedBrushCount;
	byte *output = NULL;
	dheader_t *header46;
	int offsets[HEADER_LUMPS];
	int lengths[HEADER_LUMPS];
	int i, j;
	int planeCount, nodeCount, leafCount, leafSurfaceCount, leafBrushCount;
	int modelCount, brushCount, brushSideCount, drawVertCount, surfaceCount, fogCount;
	int lightmapCount, totalIndexes;
	int brushShaderBase;
	int brushSideShaderBase;
	int surfaceShaderBase;
	int totalShaders;
	int outputLength;

	if ( !translation ) {
		BSP43_SetError( error, errorSize, "IBSP v43 translation target is missing" );
		return qfalse;
	}

	Com_Memset( translation, 0, sizeof( *translation ) );
	BSP43_SetError( error, errorSize, "" );

	if ( !allocFn ) {
		BSP43_SetError( error, errorSize, "IBSP v43 allocator is missing" );
		return qfalse;
	}
	if ( !freeFn ) {
		BSP43_SetError( error, errorSize, "IBSP v43 free callback is missing" );
		return qfalse;
	}

	if ( !inputData || inputLength < (int)sizeof( dheader43_t ) ) {
		BSP43_SetError( error, errorSize, "IBSP v43 header is truncated" );
		return qfalse;
	}

	memcpy( &originalHeader, inputData, sizeof( originalHeader ) );
	header43 = &originalHeader;
	if ( LittleLong( header43->ident ) != BSP_IDENT || LittleLong( header43->version ) != BSP_VERSION_IHV ) {
		BSP43_SetError( error, errorSize, "IBSP v43 translator received the wrong map format" );
		return qfalse;
	}

	lumps43 = header43->lumps;
	for ( i = 0; i < HEADER_LUMPS_43; i++ ) {
		if ( !BSP43_ValidateLump( &lumps43[i], inputLength ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 has an invalid lump directory" );
			return qfalse;
		}
		if ( LittleLong( lumps43[i].filelen ) && LittleLong( lumps43[i].fileofs ) < (int)sizeof( originalHeader ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 lump overlaps the header" );
			return qfalse;
		}
	}

	planeCount = LittleLong( lumps43[LUMP43_PLANES].filelen ) / (int)sizeof( dplane43_t );
	nodeCount = LittleLong( lumps43[LUMP43_NODES].filelen ) / (int)sizeof( dnode_t );
	leafCount = LittleLong( lumps43[LUMP43_LEAFS].filelen ) / (int)sizeof( dleaf_t );
	leafSurfaceCount = LittleLong( lumps43[LUMP43_LEAFSURFACES].filelen ) / (int)sizeof( int32_t );
	leafBrushCount = LittleLong( lumps43[LUMP43_LEAFBRUSHES].filelen ) / (int)sizeof( int32_t );
	modelCount = LittleLong( lumps43[LUMP43_MODELS].filelen ) / (int)sizeof( dmodel43_t );
	brushCount = LittleLong( lumps43[LUMP43_BRUSHES].filelen ) / (int)sizeof( dbrush43_t );
	brushSideCount = LittleLong( lumps43[LUMP43_BRUSHSIDES].filelen ) / (int)sizeof( dbrushside43_t );
	drawVertCount = LittleLong( lumps43[LUMP43_DRAWVERTS].filelen ) / (int)sizeof( drawVert_t );
	surfaceCount = LittleLong( lumps43[LUMP43_SURFACES].filelen ) / (int)sizeof( dsurface43_t );
	fogCount = LittleLong( lumps43[LUMP43_FOGS].filelen ) / (int)sizeof( dfog43_t );
	lightmapCount = LittleLong( lumps43[LUMP43_LIGHTMAPS].filelen ) / ( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3 );

	if ( LittleLong( lumps43[LUMP43_PLANES].filelen ) % sizeof( dplane43_t )
		|| LittleLong( lumps43[LUMP43_NODES].filelen ) % sizeof( dnode_t )
		|| LittleLong( lumps43[LUMP43_LEAFS].filelen ) % sizeof( dleaf_t )
		|| LittleLong( lumps43[LUMP43_LEAFSURFACES].filelen ) % sizeof( int32_t )
		|| LittleLong( lumps43[LUMP43_LEAFBRUSHES].filelen ) % sizeof( int32_t )
		|| LittleLong( lumps43[LUMP43_MODELS].filelen ) % sizeof( dmodel43_t )
		|| LittleLong( lumps43[LUMP43_BRUSHES].filelen ) % sizeof( dbrush43_t )
		|| LittleLong( lumps43[LUMP43_BRUSHSIDES].filelen ) % sizeof( dbrushside43_t )
		|| LittleLong( lumps43[LUMP43_DRAWVERTS].filelen ) % sizeof( drawVert_t )
		|| LittleLong( lumps43[LUMP43_SURFACES].filelen ) % sizeof( dsurface43_t )
		|| LittleLong( lumps43[LUMP43_FOGS].filelen ) % sizeof( dfog43_t ) ) {
		BSP43_SetError( error, errorSize, "IBSP v43 has a malformed lump size" );
		goto cleanup;
	}

	if ( modelCount < 1 ) {
		BSP43_SetError( error, errorSize, "IBSP v43 map has no models" );
		goto cleanup;
	}
	if ( planeCount < 1 || nodeCount < 1 || leafCount < 1 ||
		modelCount > MAX_MAP_MODELS - 2 || planeCount > MAX_MAP_PLANES ||
		nodeCount > MAX_MAP_NODES || leafCount > MAX_MAP_LEAFS ||
		leafSurfaceCount > MAX_MAP_LEAFFACES || leafBrushCount > MAX_MAP_LEAFBRUSHES ||
		brushCount > MAX_MAP_BRUSHES || brushSideCount > MAX_MAP_BRUSHSIDES ||
		drawVertCount > MAX_MAP_DRAW_VERTS || surfaceCount > MAX_MAP_DRAW_SURFS ||
		fogCount > MAX_MAP_FOGS ||
		LittleLong( lumps43[LUMP43_ENTITIES].filelen ) > MAX_MAP_ENTSTRING ||
		LittleLong( lumps43[LUMP43_LIGHTMAPS].filelen ) > MAX_MAP_LIGHTING ||
		LittleLong( lumps43[LUMP43_VISIBILITY].filelen ) > MAX_MAP_VISIBILITY ) {
		BSP43_SetError( error, errorSize, "IBSP v43 lump exceeds map limits or has no collision tree" );
		goto cleanup;
	}
	if ( lightmapCount * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3 != LittleLong( lumps43[LUMP43_LIGHTMAPS].filelen ) ) {
		BSP43_SetError( error, errorSize, "IBSP v43 lightmap lump is malformed" );
		goto cleanup;
	}
	alignedInput = BSP43_AlignedInput( inputData, &originalHeader );
	if ( !alignedInput ) {
		BSP43_SetError( error, errorSize, "IBSP v43 aligned input allocation failed" );
		goto cleanup;
	}
	inputData = alignedInput;
	header43 = (const dheader43_t *)alignedInput;
	lumps43 = header43->lumps;

	nodes43 = (const dnode_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_NODES].fileofs ) );
	leafs43 = (const dleaf_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_LEAFS].fileofs ) );
	leafSurfaces43 = (const int *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_LEAFSURFACES].fileofs ) );
	leafBrushes43 = (const int *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_LEAFBRUSHES].fileofs ) );
	models43 = (const dmodel43_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_MODELS].fileofs ) );
	brushes43 = (const dbrush43_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_BRUSHES].fileofs ) );
	brushSides43 = (const dbrushside43_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_BRUSHSIDES].fileofs ) );
	drawVerts43 = (const drawVert_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_DRAWVERTS].fileofs ) );
	surfaces43 = (const dsurface43_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_SURFACES].fileofs ) );
	fogs43 = (const dfog43_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_FOGS].fileofs ) );
	for ( i = 0; i < planeCount; ++i ) {
		const dplane43_t *planes = (const dplane43_t *)( alignedInput + LittleLong( lumps43[LUMP43_PLANES].fileofs ) );
		if ( !BSP43_FiniteFloats( planes[i].normal, 4 ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 plane is not finite" ); goto cleanup;
		}
	}
	for ( i = 0; i < drawVertCount; ++i ) {
		if ( !BSP43_FiniteFloats( drawVerts43[i].xyz, 10 ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 vertex is not finite" ); goto cleanup;
		}
	}

	modelInfo = (bsp43_modelinfo_t *)malloc( modelCount * sizeof( *modelInfo ) );
	brushSideContents = (int *)malloc( MAX( brushSideCount, 1 ) * sizeof( *brushSideContents ) );
	surfaceShaderNum = (int *)malloc( MAX( surfaceCount, 1 ) * sizeof( *surfaceShaderNum ) );
	surfaceFirstIndex = (int *)malloc( MAX( surfaceCount, 1 ) * sizeof( *surfaceFirstIndex ) );
	surfaceNumIndexes = (int *)malloc( MAX( surfaceCount, 1 ) * sizeof( *surfaceNumIndexes ) );

	if ( !modelInfo || !brushSideContents
		|| !surfaceShaderNum || !surfaceFirstIndex || !surfaceNumIndexes ) {
		BSP43_SetError( error, errorSize, "IBSP v43 scratch allocation failed" );
		goto cleanup;
	}

	Com_Memset( modelInfo, 0, modelCount * sizeof( *modelInfo ) );
	Com_Memset( brushSideContents, 0, MAX( brushSideCount, 1 ) * sizeof( *brushSideContents ) );

	for ( i = 0; i < nodeCount; i++ ) {
		int planeNum;
		int child0;
		int child1;

		planeNum = LittleLong( nodes43[i].planeNum );
		child0 = LittleLong( nodes43[i].children[0] );
		child1 = LittleLong( nodes43[i].children[1] );

		if ( planeNum < 0 || planeNum >= planeCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 node references an invalid plane" );
			goto cleanup;
		}
		if ( child0 >= nodeCount || child1 >= nodeCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 node references an invalid child node" );
			goto cleanup;
		}
		if ( child0 < -leafCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 node references an invalid child leaf" );
			goto cleanup;
		}
		if ( child1 < -leafCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 node references an invalid child leaf" );
			goto cleanup;
		}
	}
	if ( !BSP43_NodeTreeValid( nodes43, nodeCount ) ) {
		BSP43_SetError( error, errorSize, "IBSP v43 node tree is cyclic, shared, too deep or unavailable" );
		goto cleanup;
	}

	for ( i = 0; i < leafCount; i++ ) {
		int firstLeafSurface;
		int numLeafSurfaces;
		int firstLeafBrush;
		int numLeafBrushes;

		firstLeafSurface = LittleLong( leafs43[i].firstLeafSurface );
		numLeafSurfaces = LittleLong( leafs43[i].numLeafSurfaces );
		firstLeafBrush = LittleLong( leafs43[i].firstLeafBrush );
		numLeafBrushes = LittleLong( leafs43[i].numLeafBrushes );

		if ( !BSP43_RangeValid( firstLeafSurface, numLeafSurfaces, leafSurfaceCount ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 leaf surface list is out of bounds" );
			goto cleanup;
		}
		if ( !BSP43_RangeValid( firstLeafBrush, numLeafBrushes, leafBrushCount ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 leaf brush list is out of bounds" );
			goto cleanup;
		}
		if ( LittleLong( leafs43[i].cluster ) < -1 || LittleLong( leafs43[i].cluster ) >= MAX_MAP_LEAFS ||
			LittleLong( leafs43[i].area ) < -1 || LittleLong( leafs43[i].area ) >= MAX_MAP_AREAS ) {
			BSP43_SetError( error, errorSize, "IBSP v43 leaf has an invalid cluster or area" ); goto cleanup;
		}
	}
	{
		int bytes = LittleLong( lumps43[LUMP43_VISIBILITY].filelen );
		if ( bytes ) {
			const int *vis = (const int *)( alignedInput + LittleLong( lumps43[LUMP43_VISIBILITY].fileofs ) );
			int clusters, row;
			if ( bytes < 8 ) {
				BSP43_SetError( error, errorSize, "IBSP v43 visibility header is truncated" ); goto cleanup;
			}
			clusters = LittleLong( vis[0] ); row = LittleLong( vis[1] );
			if ( clusters < 1 || clusters > MAX_MAP_LEAFS || row < ( clusters + 7 ) / 8 ||
				clusters > ( bytes - 8 ) / row ) {
				BSP43_SetError( error, errorSize, "IBSP v43 visibility table is out of bounds" ); goto cleanup;
			}
			for ( i = 0; i < leafCount; ++i ) {
				if ( LittleLong( leafs43[i].cluster ) >= clusters ) {
					BSP43_SetError( error, errorSize, "IBSP v43 leaf references an invalid visibility row" ); goto cleanup;
				}
			}
		}
	}

	for ( i = 0; i < leafSurfaceCount; i++ ) {
		int surfaceNum;

		surfaceNum = LittleLong( leafSurfaces43[i] );
		if ( surfaceNum < 0 || surfaceNum >= surfaceCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 leaf references an invalid surface" );
			goto cleanup;
		}
	}

	for ( i = 0; i < leafBrushCount; i++ ) {
		int brushNum;

		brushNum = LittleLong( leafBrushes43[i] );
		if ( brushNum < 0 || brushNum >= brushCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 leaf references an invalid brush" );
			goto cleanup;
		}
	}

	for ( i = 0; i < brushCount; i++ ) {
		int firstSide;
		int numSides;
		int contents;

		firstSide = LittleLong( brushes43[i].firstSide );
		numSides = LittleLong( brushes43[i].numSides );
		contents = LittleLong( brushes43[i].contentFlags );

		if ( numSides < 6 || !BSP43_RangeValid( firstSide, numSides, brushSideCount ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 brush side range is out of bounds" );
			goto cleanup;
		}

		for ( j = 0; j < numSides; j++ ) {
			brushSideContents[firstSide + j] = contents;
		}
	}

	for ( i = 0; i < brushSideCount; i++ ) {
		int planeNum;

		planeNum = LittleLong( brushSides43[i].planeNum );
		if ( planeNum < 0 || planeNum >= planeCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 brush side references an invalid plane" );
			goto cleanup;
		}
	}

	totalIndexes = 0;
	for ( i = 0; i < surfaceCount; i++ ) {
		int firstVert;
		int numVerts;
		int patchWidth;
		int patchHeight;
		int fogNum;
		int lightmapNum;

		firstVert = LittleLong( surfaces43[i].firstVert );
		numVerts = LittleLong( surfaces43[i].numVerts );
		patchWidth = LittleLong( surfaces43[i].patchWidth );
		patchHeight = LittleLong( surfaces43[i].patchHeight );
		fogNum = LittleLong( surfaces43[i].fogNum );
		lightmapNum = LittleLong( surfaces43[i].lightmapNum );

		if ( numVerts < 3 || !BSP43_RangeValid( firstVert, numVerts, drawVertCount ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 surface vertex range is out of bounds" );
			goto cleanup;
		}
		if ( !memchr( surfaces43[i].shader, 0, sizeof( surfaces43[i].shader ) ) ||
			!BSP43_FiniteFloats( surfaces43[i].lightmapOrigin, 12 ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 surface has an invalid shader name or lightmap vectors" ); goto cleanup;
		}
		if ( ( patchWidth || patchHeight ) &&
			( patchWidth < 3 || patchHeight < 3 || patchWidth > 32 || patchHeight > 32 ||
			!( patchWidth & 1 ) || !( patchHeight & 1 ) || patchWidth * patchHeight != numVerts ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 patch dimensions do not match its control points" ); goto cleanup;
		}
		if ( fogNum < -1 || fogNum >= fogCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 surface references an invalid fog" );
			goto cleanup;
		}
		if ( lightmapNum < -1 || lightmapNum >= lightmapCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 surface references an invalid lightmap" );
			goto cleanup;
		}


		surfaceShaderNum[i] = 0;
		surfaceFirstIndex[i] = totalIndexes;

		if ( patchWidth > 0 && patchHeight > 0 ) {
			surfaceNumIndexes[i] = 0;
		} else if ( numVerts >= 3 ) {
			if ( numVerts - 2 > ( MAX_MAP_DRAW_INDEXES - totalIndexes ) / 3 ) {
				BSP43_SetError( error, errorSize, "IBSP v43 triangle fans exceed the index limit" ); goto cleanup;
			}
			surfaceNumIndexes[i] = ( numVerts - 2 ) * 3;
			totalIndexes += surfaceNumIndexes[i];
		} else {
			surfaceNumIndexes[i] = 0;
		}
	}

	for ( i = 0; i < modelCount; i++ ) {
		int firstFace;
		int numFaces;
		int nodeNum;
		int leafNum;

		firstFace = LittleLong( models43[i].firstFace );
		numFaces = LittleLong( models43[i].numFaces );
		nodeNum = LittleLong( models43[i].headnode );
		leafNum = -1 - nodeNum;

		if ( !BSP43_FiniteFloats( models43[i].mins, 9 ) ||
			!BSP43_RangeValid( firstFace, numFaces, surfaceCount ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 model surface range is out of bounds" );
			goto cleanup;
		}
		if ( nodeNum >= nodeCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 model has an invalid headnode" );
			goto cleanup;
		}
		if ( nodeNum < 0 && ( leafNum < 0 || leafNum >= leafCount ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 model has an invalid headnode" );
			goto cleanup;
		}
	}

	for ( i = 0; i < fogCount; i++ ) {
		int brushNum;

		brushNum = LittleLong( fogs43[i].brushNum );
		if ( !memchr( fogs43[i].shader, 0, sizeof( fogs43[i].shader ) ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 fog shader name is unterminated" ); goto cleanup;
		}
		if ( brushNum < 0 || brushNum >= brushCount ) {
			BSP43_SetError( error, errorSize, "IBSP v43 fog references an invalid brush" );
			goto cleanup;
		}
	}

	/* v43 models own a headnode, not a contiguous brush range. Preserve
	 * exact membership, including unordered lists and node-rooted inline
	 * models. Keep original brush numbers for world leaf references; only
	 * duplicate records when a model's unique brush set is not contiguous. */
	modelBrushes = (int *)malloc( MAX_MAP_BRUSHES * sizeof( *modelBrushes ) );
	if ( !modelBrushes ) {
		BSP43_SetError( error, errorSize, "IBSP v43 model brush allocation failed" ); goto cleanup;
	}
	translatedBrushCount = brushCount;
	for ( i = 0; i < brushCount; ++i ) modelBrushes[i] = i;
	for ( i = 0; i < modelCount; ++i ) {
		modelInfo[i].firstSurface = LittleLong( models43[i].firstFace );
		modelInfo[i].numSurfaces = LittleLong( models43[i].numFaces );
	}
	modelInfo[0].numBrushes = brushCount;
	{
		byte *seen = (byte *)malloc( MAX( brushCount, 1 ) );
		int *stack = (int *)malloc( ( nodeCount + 1 ) * sizeof( *stack ) );
		qboolean valid = qtrue;
		if ( !seen || !stack ) valid = qfalse;
		for ( i = 1; i < modelCount && valid; ++i ) {
			int depth = 1, count = 0, first = brushCount, last = -1;
			memset( seen, 0, MAX( brushCount, 1 ) );
			stack[0] = LittleLong( models43[i].headnode );
			while ( depth ) {
				int node = stack[--depth];
				if ( node >= 0 ) {
					stack[depth++] = LittleLong( nodes43[node].children[0] );
					stack[depth++] = LittleLong( nodes43[node].children[1] );
				} else {
					const dleaf_t *leaf = &leafs43[-1 - node];
					int offset = LittleLong( leaf->firstLeafBrush );
					int n = LittleLong( leaf->numLeafBrushes );
					for ( j = 0; j < n; ++j ) {
						int brush = LittleLong( leafBrushes43[offset + j] );
						if ( seen[brush] ) continue;
						seen[brush] = 1; ++count;
						first = MIN( first, brush ); last = MAX( last, brush );
					}
				}
			}
			modelInfo[i].numBrushes = count;
			if ( !count ) continue;
			if ( last - first + 1 == count ) {
				modelInfo[i].firstBrush = first;
			} else {
				if ( count > MAX_MAP_BRUSHES - translatedBrushCount ) { valid = qfalse; break; }
				modelInfo[i].firstBrush = translatedBrushCount;
				for ( j = first; j <= last; ++j ) {
					if ( seen[j] ) modelBrushes[translatedBrushCount++] = j;
				}
			}
			modelInfo[0].numBrushes = MIN( modelInfo[0].numBrushes, first );
		}
		free( seen ); free( stack );
		if ( !valid ) {
			BSP43_SetError( error, errorSize, "IBSP v43 inline brush sets exceed available storage" ); goto cleanup;
		}
	}

	brushShaderBase = 0;
	brushSideShaderBase = brushShaderBase + brushCount;
	surfaceShaderBase = brushSideShaderBase + brushSideCount;
	totalShaders = surfaceShaderBase + surfaceCount;

	lengths[LUMP_ENTITIES] = LittleLong( lumps43[LUMP43_ENTITIES].filelen );
	lengths[LUMP_SHADERS] = totalShaders * (int)sizeof( dshader_t );
	lengths[LUMP_PLANES] = planeCount * (int)sizeof( dplane_t );
	lengths[LUMP_NODES] = LittleLong( lumps43[LUMP43_NODES].filelen );
	lengths[LUMP_LEAFS] = LittleLong( lumps43[LUMP43_LEAFS].filelen );
	lengths[LUMP_LEAFSURFACES] = LittleLong( lumps43[LUMP43_LEAFSURFACES].filelen );
	lengths[LUMP_LEAFBRUSHES] = LittleLong( lumps43[LUMP43_LEAFBRUSHES].filelen );
	lengths[LUMP_MODELS] = modelCount * (int)sizeof( dmodel_t );
	lengths[LUMP_BRUSHES] = translatedBrushCount * (int)sizeof( dbrush_t );
	lengths[LUMP_BRUSHSIDES] = brushSideCount * (int)sizeof( dbrushside_t );
	lengths[LUMP_DRAWVERTS] = LittleLong( lumps43[LUMP43_DRAWVERTS].filelen );
	lengths[LUMP_DRAWINDEXES] = totalIndexes * (int)sizeof( int32_t );
	lengths[LUMP_FOGS] = fogCount * (int)sizeof( dfog_t );
	lengths[LUMP_SURFACES] = surfaceCount * (int)sizeof( dsurface_t );
	lengths[LUMP_LIGHTMAPS] = LittleLong( lumps43[LUMP43_LIGHTMAPS].filelen );
	lengths[LUMP_LIGHTGRID] = 0;
	lengths[LUMP_VISIBILITY] = LittleLong( lumps43[LUMP43_VISIBILITY].filelen );

	outputLength = sizeof( dheader_t );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		if ( !BSP43_AppendLump( &outputLength, lengths[i], &offsets[i] ) ) {
			BSP43_SetError( error, errorSize, "IBSP v43 translated size overflows" ); goto cleanup;
		}
	}

	output = (byte *)allocFn( outputLength );
	if ( !output ) {
		BSP43_SetError( error, errorSize, "IBSP v43 output allocation failed" );
		goto cleanup;
	}
	Com_Memset( output, 0, outputLength );

	header46 = (dheader_t *)output;
	header46->ident = LittleLong( BSP_IDENT );
	header46->version = LittleLong( BSP_VERSION );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		header46->lumps[i].fileofs = LittleLong( offsets[i] );
		header46->lumps[i].filelen = LittleLong( lengths[i] );
	}

	Com_Memcpy( output + offsets[LUMP_ENTITIES],
		(const byte *)inputData + LittleLong( lumps43[LUMP43_ENTITIES].fileofs ),
		lengths[LUMP_ENTITIES] );

	{
		dshader_t *outShaders;

		outShaders = (dshader_t *)( output + offsets[LUMP_SHADERS] );

		for ( i = 0; i < brushCount; i++ ) {
			int shaderIndex;

			shaderIndex = brushShaderBase + i;
			Q_strncpyz( outShaders[shaderIndex].shader, "*default", sizeof( outShaders[shaderIndex].shader ) );
			outShaders[shaderIndex].surfaceFlags = LittleLong( 0 );
			outShaders[shaderIndex].contentFlags = LittleLong( LittleLong( brushes43[i].contentFlags ) );
		}

		for ( i = 0; i < brushSideCount; i++ ) {
			int shaderIndex;

			shaderIndex = brushSideShaderBase + i;
			Q_strncpyz( outShaders[shaderIndex].shader, "*default", sizeof( outShaders[shaderIndex].shader ) );
			outShaders[shaderIndex].surfaceFlags = LittleLong( LittleLong( brushSides43[i].surfaceFlags ) );
			outShaders[shaderIndex].contentFlags = LittleLong( brushSideContents[i] );
		}

		for ( i = 0; i < surfaceCount; i++ ) {
			const int shaderIndex = surfaceShaderBase + i;
			/* The dword at surface offset 68 is not a usable brush-side
			 * reference: ihv_test1/tim_dm1 contain values past that table.
			 * Original 4.01 CM patch loading (0x4083f0..0x4084c0) reads
			 * dimensions/vertices and sets surface flags to zero. Do not
			 * borrow flags or material names from an unrelated brush side. */
			Q_strncpyz( outShaders[shaderIndex].shader, surfaces43[i].shader, sizeof( outShaders[shaderIndex].shader ) );
			outShaders[shaderIndex].surfaceFlags = 0;
			outShaders[shaderIndex].contentFlags = LittleLong( CONTENTS_SOLID );
			surfaceShaderNum[i] = shaderIndex;
		}

	}

	{
		const dplane43_t *inPlanes;
		dplane_t *outPlanes;

		inPlanes = (const dplane43_t *)( (const byte *)inputData + LittleLong( lumps43[LUMP43_PLANES].fileofs ) );
		outPlanes = (dplane_t *)( output + offsets[LUMP_PLANES] );

		for ( i = 0; i < planeCount; i++ ) {
			outPlanes[i].normal[0] = inPlanes[i].normal[0];
			outPlanes[i].normal[1] = inPlanes[i].normal[1];
			outPlanes[i].normal[2] = inPlanes[i].normal[2];
			outPlanes[i].dist = inPlanes[i].dist;
		}
	}

	Com_Memcpy( output + offsets[LUMP_NODES],
		(const byte *)inputData + LittleLong( lumps43[LUMP43_NODES].fileofs ),
		lengths[LUMP_NODES] );
	Com_Memcpy( output + offsets[LUMP_LEAFS],
		(const byte *)inputData + LittleLong( lumps43[LUMP43_LEAFS].fileofs ),
		lengths[LUMP_LEAFS] );
	Com_Memcpy( output + offsets[LUMP_LEAFSURFACES],
		(const byte *)inputData + LittleLong( lumps43[LUMP43_LEAFSURFACES].fileofs ),
		lengths[LUMP_LEAFSURFACES] );
	Com_Memcpy( output + offsets[LUMP_LEAFBRUSHES],
		(const byte *)inputData + LittleLong( lumps43[LUMP43_LEAFBRUSHES].fileofs ),
		lengths[LUMP_LEAFBRUSHES] );

	{
		dmodel_t *outModels;

		outModels = (dmodel_t *)( output + offsets[LUMP_MODELS] );

		for ( i = 0; i < modelCount; i++ ) {
			for ( j = 0; j < 3; j++ ) {
				outModels[i].mins[j] = models43[i].mins[j];
				outModels[i].maxs[j] = models43[i].maxs[j];
			}

			outModels[i].firstSurface = LittleLong( modelInfo[i].firstSurface );
			outModels[i].numSurfaces = LittleLong( modelInfo[i].numSurfaces );
			outModels[i].firstBrush = LittleLong( modelInfo[i].firstBrush );
			outModels[i].numBrushes = LittleLong( modelInfo[i].numBrushes );
		}
	}

	{
		dbrush_t *outBrushes;

		outBrushes = (dbrush_t *)( output + offsets[LUMP_BRUSHES] );

		for ( i = 0; i < translatedBrushCount; i++ ) {
			outBrushes[i].firstSide = LittleLong( LittleLong( brushes43[modelBrushes[i]].firstSide ) );
			outBrushes[i].numSides = LittleLong( LittleLong( brushes43[modelBrushes[i]].numSides ) );
			outBrushes[i].shaderNum = LittleLong( brushShaderBase + modelBrushes[i] );
		}
	}

	{
		dbrushside_t *outBrushSides;

		outBrushSides = (dbrushside_t *)( output + offsets[LUMP_BRUSHSIDES] );

		for ( i = 0; i < brushSideCount; i++ ) {
			outBrushSides[i].planeNum = LittleLong( LittleLong( brushSides43[i].planeNum ) );
			outBrushSides[i].shaderNum = LittleLong( brushSideShaderBase + i );
		}
	}

	Com_Memcpy( output + offsets[LUMP_DRAWVERTS],
		(const byte *)inputData + LittleLong( lumps43[LUMP43_DRAWVERTS].fileofs ),
		lengths[LUMP_DRAWVERTS] );

	{
		int *outIndexes;
		int cursor;

		outIndexes = (int *)( output + offsets[LUMP_DRAWINDEXES] );
		cursor = 0;

		for ( i = 0; i < surfaceCount; i++ ) {
			int numTris;
			int patchWidth;
			int patchHeight;

			patchWidth = LittleLong( surfaces43[i].patchWidth );
			patchHeight = LittleLong( surfaces43[i].patchHeight );
			if ( patchWidth > 0 && patchHeight > 0 ) {
				continue;
			}

			numTris = LittleLong( surfaces43[i].numVerts ) - 2;
			for ( j = 0; j < numTris; j++ ) {
				outIndexes[cursor++] = LittleLong( 0 );
				outIndexes[cursor++] = LittleLong( j + 1 );
				outIndexes[cursor++] = LittleLong( j + 2 );
			}
		}
	}

	{
		dfog_t *outFogs;

		outFogs = (dfog_t *)( output + offsets[LUMP_FOGS] );

		for ( i = 0; i < fogCount; i++ ) {
			Q_strncpyz( outFogs[i].shader, fogs43[i].shader, sizeof( outFogs[i].shader ) );
			outFogs[i].brushNum = LittleLong( LittleLong( fogs43[i].brushNum ) );
			outFogs[i].visibleSide = LittleLong( 5 );
		}
	}

	{
		dsurface_t *outSurfaces;

		outSurfaces = (dsurface_t *)( output + offsets[LUMP_SURFACES] );

		for ( i = 0; i < surfaceCount; i++ ) {
			int patchWidth;
			int patchHeight;

			patchWidth = LittleLong( surfaces43[i].patchWidth );
			patchHeight = LittleLong( surfaces43[i].patchHeight );

			outSurfaces[i].shaderNum = LittleLong( surfaceShaderNum[i] );
			outSurfaces[i].fogNum = LittleLong( LittleLong( surfaces43[i].fogNum ) );
			outSurfaces[i].surfaceType = LittleLong( ( patchWidth > 0 && patchHeight > 0 ) ? MST_PATCH : MST_PLANAR );
			outSurfaces[i].firstVert = LittleLong( LittleLong( surfaces43[i].firstVert ) );
			outSurfaces[i].numVerts = LittleLong( LittleLong( surfaces43[i].numVerts ) );
			outSurfaces[i].firstIndex = LittleLong( surfaceFirstIndex[i] );
			outSurfaces[i].numIndexes = LittleLong( surfaceNumIndexes[i] );
			outSurfaces[i].lightmapNum = LittleLong( LittleLong( surfaces43[i].lightmapNum ) );
			outSurfaces[i].lightmapX = LittleLong( LittleLong( surfaces43[i].lightmapX ) );
			outSurfaces[i].lightmapY = LittleLong( LittleLong( surfaces43[i].lightmapY ) );
			outSurfaces[i].lightmapWidth = LittleLong( LittleLong( surfaces43[i].lightmapWidth ) );
			outSurfaces[i].lightmapHeight = LittleLong( LittleLong( surfaces43[i].lightmapHeight ) );

			for ( j = 0; j < 3; j++ ) {
				outSurfaces[i].lightmapOrigin[j] = surfaces43[i].lightmapOrigin[j];
				outSurfaces[i].lightmapVecs[0][j] = surfaces43[i].lightmapVecs[0][j];
				outSurfaces[i].lightmapVecs[1][j] = surfaces43[i].lightmapVecs[1][j];
				outSurfaces[i].lightmapVecs[2][j] = surfaces43[i].lightmapVecs[2][j];
			}

			outSurfaces[i].patchWidth = LittleLong( patchWidth );
			outSurfaces[i].patchHeight = LittleLong( patchHeight );

			if ( patchWidth > 0 && patchHeight > 0 ) {
				BSP43_SetPatchBounds( &surfaces43[i], drawVerts43, &outSurfaces[i] );
			}
		}
	}

	Com_Memcpy( output + offsets[LUMP_LIGHTMAPS],
		(const byte *)inputData + LittleLong( lumps43[LUMP43_LIGHTMAPS].fileofs ),
		lengths[LUMP_LIGHTMAPS] );
	Com_Memcpy( output + offsets[LUMP_VISIBILITY],
		(const byte *)inputData + LittleLong( lumps43[LUMP43_VISIBILITY].fileofs ),
		lengths[LUMP_VISIBILITY] );

	translation->data = output;
	translation->length = outputLength;
	output = NULL;

cleanup:
	free( alignedInput );
	if ( output ) {
		freeFn( output );
	}

	free( modelInfo );
	free( modelBrushes );
	free( brushSideContents );
	free( surfaceShaderNum );
	free( surfaceFirstIndex );
	free( surfaceNumIndexes );

	return translation->data != NULL ? qtrue : qfalse;
}

#endif
