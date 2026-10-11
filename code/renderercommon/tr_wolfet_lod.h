/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_LOD_H
#define FNTECH3_TR_WOLFET_LOD_H

#include "tr_wolfet_model_format.h"

/* Enemy Territory's detail reduction for skeletal meshes. Its reference entity
 * carries two flags that bear on it, from its own cgame/tr_types.h: bit 8 asks
 * for a lower level of detail than the distance would choose, and bit 32 --
 * which its own comment attaches to corpses -- allows the mesh to drop below
 * the detail level the surface itself asks for. The other Wolfenstein profile
 * reads bit 8 differently in single player; ET has only the one reading. */
#define FN_WOLFET_REFLAG_FORCE_LOD 8u
#define FN_WOLFET_REFLAG_DEAD_LOD 32u

static inline float FnWolfET_LodFraction( unsigned flags, float projected,
	float scale, float bias, float modelScale, float modelBias ) {
	float fraction;
	if ( !isfinite( projected ) || !isfinite( scale ) || !isfinite( bias ) ) return 1;
	/* A model that meets the near plane, such as a view weapon, projects to
	 * nothing and is drawn at full detail. */
	fraction = projected != 0 ? projected * scale * modelScale : 1;
	if ( flags & FN_WOLFET_REFLAG_FORCE_LOD ) fraction = (float)( fraction * 0.5 );
	if ( flags & FN_WOLFET_REFLAG_DEAD_LOD ) fraction = (float)( fraction * 0.8 );
	fraction = (float)( fraction - ( 0.25 * bias + modelBias ) );
	if ( !isfinite( fraction ) ) return 1;
	return fraction < 0 ? 0 : fraction > 1 ? 1 : fraction;
}

/* The surface's own minimum detail is honoured, except for a mesh flagged as a
 * corpse, which may fall below it but never below the 35% floor ET chose. */
static inline uint32_t FnWolfET_LodVertexCount( const fnWolfETMeshSurfaceView_t *surface,
	float fraction, int dead ) {
	uint32_t count;
	volatile float scaledCount;
	if ( !surface ) return 0;
	if ( !isfinite( fraction ) ) fraction = 1;
	if ( fraction < 0 ) fraction = 0;
	if ( fraction > 1 ) fraction = 1;
	if ( dead && fraction < 0.35f ) fraction = 0.35f;
	/* Round the product to float before truncation, including on x87 hosts.
	 * Otherwise 100 * 0.35f can retain 34 vertices instead of 35. */
	scaledCount = (float)surface->numVerts * fraction;
	count = (uint32_t)scaledCount;
	if ( !dead && count < surface->minLod ) count = surface->minLod;
	return count > surface->numVerts ? surface->numVerts : count;
}

/* Every vertex past the retained count collapses onto an earlier one. A map
 * entry that does not point backwards would let a mesh fold onto itself or
 * read past what has been resolved, so it is refused. */
static inline int FnWolfET_CollapseVertices( const fnWolfETMeshSurfaceView_t *surface,
	uint32_t retained, uint32_t *map, size_t capacity ) {
	uint32_t i;
	if ( !surface || !map || !retained || retained > surface->numVerts ||
		capacity < surface->numVerts ) return 0;
	for ( i = 0; i < retained; ++i ) map[i] = i;
	for ( ; i < surface->numVerts; ++i ) {
		const uint32_t parent = FnWolfET_U32( surface->data + surface->collapseMap + (size_t)i * 4 );
		if ( parent >= i ) return 0;
		map[i] = map[parent];
	}
	return 1;
}

#endif
