/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_ALICE_SPRITE_SORT_H
#define FNTECH3_TR_ALICE_SPRITE_SORT_H

/* Alice sorts each sprite shader's particles farthest first (alice.exe
 * 0x0047910e..0x00479183: squared eye distance below the shader index).
 * The shared sort key instead contains an entity index. Keep that key for
 * backend dispatch, but order adjacent sprites of the same shader by distance
 * after the normal radix sort. Other surfaces and profiles keep their order.
 * This feeds the existing transparent passes in GLx, Vulkan and RTX. */
static const trRefEntity_t *R_AliceSpriteDrawEntity( const drawSurf_t *draw ) {
	const unsigned int index = ( draw->sort >> QSORT_REFENTITYNUM_SHIFT ) & REFENTITYNUM_MASK;
	if ( index >= (unsigned int)tr.refdef.num_entities || !draw->surface || *draw->surface != SF_ENTITY ) {
		return NULL;
	}
	return tr.refdef.entities[index].e.reType == RT_ALICE_SPRITE ? &tr.refdef.entities[index] : NULL;
}

static double R_AliceSpriteDistanceSquared( const drawSurf_t *draw ) {
	const trRefEntity_t *ent = R_AliceSpriteDrawEntity( draw );
	double distance = 0.0;
	int i;
	for ( i = 0; i < 3; i++ ) {
		uint32_t entityBits, viewBits;
		/* Bit checks survive fast-math builds. Finite float coordinates have
		 * finite squared differences in double precision. */
		memcpy( &entityBits, &ent->e.origin[i], sizeof( entityBits ) );
		memcpy( &viewBits, &tr.viewParms.or.origin[i], sizeof( viewBits ) );
		if ( ( entityBits & 0x7f800000u ) == 0x7f800000u ||
			( viewBits & 0x7f800000u ) == 0x7f800000u ) return 0.0;
		const double delta = (double)ent->e.origin[i] - tr.viewParms.or.origin[i];
		distance += delta * delta;
	}
	return distance;
}

static int QDECL R_AliceCompareSprites( const void *a, const void *b ) {
	const drawSurf_t *left = (const drawSurf_t *)a, *right = (const drawSurf_t *)b;
	const double ld = R_AliceSpriteDistanceSquared( left );
	const double rd = R_AliceSpriteDistanceSquared( right );
	if ( ld > rd ) return -1;
	if ( ld < rd ) return 1;
	return ( left->sort > right->sort ) - ( left->sort < right->sort );
}

static void R_SortAliceSprites( drawSurf_t *draws, int count ) {
	int first, end;
	if ( ri.gameId != COM_GAME_ALICE || count < 2 ) return;
	for ( first = 0; first < count; first = end ) {
		end = first + 1;
		if ( !R_AliceSpriteDrawEntity( &draws[first] ) ) continue;
		while ( end < count &&
			( draws[end].sort >> QSORT_SHADERNUM_SHIFT ) == ( draws[first].sort >> QSORT_SHADERNUM_SHIFT ) &&
			R_AliceSpriteDrawEntity( &draws[end] ) ) {
			end++;
		}
		if ( end - first > 1 ) {
			qsort( draws + first, end - first, sizeof( *draws ), R_AliceCompareSprites );
		}
	}
}

#endif
