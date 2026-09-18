/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_FOLIAGE_WORLD_H
#define FNTECH3_TR_WOLFET_FOLIAGE_WORLD_H

/* Enemy Territory culls a foliage surface by the sphere, and then the box,
 * that enclose every instance, and draws none at all while r_drawfoliage is
 * zero. It has no plane to cull against. */
static qboolean R_CullWolfETFoliage( const srfWolfETFoliage_t *foliage ) {
	int cull;
	if ( r_drawfoliage && !r_drawfoliage->value ) {
		return qtrue;
	}
	if ( tr.currentEntityNum != REFENTITYNUM_WORLD ) {
		cull = R_CullLocalPointAndRadius( foliage->origin, foliage->radius );
	} else {
		cull = R_CullPointAndRadius( foliage->origin, foliage->radius );
	}
	if ( cull == CULL_OUT ) {
		return qtrue;
	}
	if ( cull == CULL_CLIP && R_CullLocalBox( foliage->bounds ) == CULL_OUT ) {
		return qtrue;
	}
	return qfalse;
}

#endif
