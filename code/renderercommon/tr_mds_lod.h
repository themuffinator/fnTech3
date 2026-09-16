/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_LOD_H
#define FNTECH3_TR_MDS_LOD_H

/* The selected profile resolves RTCW's alias: SP bit 8 means full detail,
 * while MP bit 8 halves detail. No effect flags are read from legacy records. */
static inline float FnMds_LodFraction( comGameId_t game, unsigned flags, float projected,
	float scale, float bias, float modelScale, float modelBias ) {
	float fraction;
	if ( game == COM_GAME_RTCW_SP && ( flags & 8u ) ) return 1;
	if ( game != COM_GAME_RTCW_SP && game != COM_GAME_RTCW_MP ) flags = 0;
	if ( !isfinite( projected ) || !isfinite( scale ) || !isfinite( bias ) ) return 1;
	fraction = projected != 0 ? projected * scale * modelScale : 1;
	if ( flags & 8u ) fraction = (float)( fraction * 0.5 );
	if ( flags & 32u ) fraction = (float)( fraction * 0.8 );
	fraction = (float)( fraction - ( 0.25 * bias + modelBias ) );
	if ( !isfinite( fraction ) ) return 1;
	return fraction < 0 ? 0 : fraction > 1 ? 1 : fraction;
}

static inline uint32_t FnMds_LodVertexCount( const fnMdsSurfaceView_t *surface, float fraction, int dead ) {
	uint32_t count;
	if ( !isfinite( fraction ) ) fraction = 1;
	if ( fraction < 0 ) fraction = 0;
	if ( fraction > 1 ) fraction = 1;
	if ( dead && fraction < 0.35 ) fraction = 0.35f;
	count = (uint32_t)( (float)surface->numVerts * fraction );
	if ( !dead && count < surface->minLod ) count = surface->minLod;
	return count > surface->numVerts ? surface->numVerts : count;
}

static inline int FnMds_CollapseVertices( const fnMdsSurfaceView_t *surface, uint32_t retained,
	uint32_t *map, size_t capacity ) {
	uint32_t i;
	if ( !surface || !map || !retained || retained > surface->numVerts || capacity < surface->numVerts ) return 0;
	for ( i = 0; i < retained; ++i ) map[i] = i;
	for ( ; i < surface->numVerts; ++i ) {
		const uint32_t parent = FnMds_U32( surface->data + surface->collapseMap + (size_t)i * 4 );
		if ( parent >= i ) return 0;
		map[i] = map[parent];
	}
	return 1;
}

#endif
