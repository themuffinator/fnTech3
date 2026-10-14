/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_JKA_SURFACE_H
#define FNTECH3_TR_JKA_SURFACE_H

/*
 * Jedi Academy multiplayer's Raven primitives, tessellated as retail jamp.exe
 * 1.01's renderer does: RB_SurfaceEntity (0x4bdcd0) sends the saber glow to
 * 0x4ba780, the line to 0x4bb3c0 and the oriented line to 0x4bb4c0. Each
 * renderer's tr_surface.c includes this after tr_stvef_surface.h, whose
 * vertex, triangle and sprite-axis helpers it shares; it supplies tess,
 * backEnd, RB_CHECKOVERFLOW, RB_AddQuadStampExt and R_ViewPassIsMirror.
 *
 * Retail's x87 code runs at its 53-bit precision and stores each sum as a
 * float; the glow's running length and radius are kept that way here, so a
 * blade takes retail's number of sprites. Retail writes no vertex normal for
 * a line; it takes the view-facing one Elite Force's lines write.
 */

/* An engine bound on one blade's sprites: a malformed length must not stall
 * the frame. Retail has none; its sabers take about twenty. */
#define JKA_SABER_GLOW_MAX_SPRITES 1024

/* 0x4ba600: a view-facing quad at origin, rotated by degrees, its left
 * negated in a mirror, the entity's colour over the whole texture. */
static void RB_JKASprite( const vec3_t origin, float radius, float rotation ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t left, up;
	RB_STVEFSpriteAxes( backEnd.viewParms.or.axis[1], backEnd.viewParms.or.axis[2],
		radius, rotation, left, up );
	if ( R_ViewPassIsMirror( &backEnd.viewParms ) ) {
		VectorSubtract( vec3_origin, left, left );
	}
	RB_AddQuadStampExt( origin, left, up, e->shader, 0, 0, 1, 1 );
}

/* 0x4ba780: a sprite every 0.65 of its radius down the blade from its tip,
 * the radius growing 0.017 a sprite, then a hilt sprite of 5.5 to 5.75.
 * Retail grows the entity's own radius as it goes, so a second view of the
 * scene in the same frame would start larger; each view starts from the
 * module's radius here. */
static void RB_SurfaceJKASaberGlow( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	float along = e->jka.saberLength;
	float radius = e->radius;
	vec3_t point;
	int sprites, i;
	for ( sprites = 0; along > 0.0f && sprites < JKA_SABER_GLOW_MAX_SPRITES; ++sprites ) {
		for ( i = 0; i < 3; ++i ) {
			point[i] = (float)( (double)along * e->axis[0][i] + e->origin[i] );
		}
		RB_JKASprite( point, radius, 0.0f );
		radius = (float)( (double)radius + (double)0.017f );
		along = (float)( (double)along - (double)radius * (double)0.65f );
	}
	RB_JKASprite( e->origin,
		(float)( (double)( rand() & 0x7fff ) / 32767.0 * 0.25 + 5.5 ), 0.0f );
}

/* 0x4ba860 and 0x4bb040: one quad from start to end, halfWidth either side
 * along up, every corner the entity's whole colour, t running to endT. */
static void RB_JKALineQuad( const vec3_t start, const vec3_t end, const vec3_t up,
	float halfWidth, float endT ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	const byte *rgba = e->shader.rgba;
	vec3_t corner;
	int ndx;
	RB_CHECKOVERFLOW( 4, 6 );
	ndx = tess.numVertexes;
	VectorMA( start, halfWidth, up, corner );
	RB_STVEFVertex( ndx, corner, 0, 0, rgba, 1.0f, rgba[3] );
	VectorMA( start, -halfWidth, up, corner );
	RB_STVEFVertex( ndx + 1, corner, 1, 0, rgba, 1.0f, rgba[3] );
	VectorMA( end, halfWidth, up, corner );
	RB_STVEFVertex( ndx + 2, corner, 0, endT, rgba, 1.0f, rgba[3] );
	VectorMA( end, -halfWidth, up, corner );
	RB_STVEFVertex( ndx + 3, corner, 1, endT, rgba, 1.0f, rgba[3] );
	tess.numVertexes += 4;
	RB_STVEFTriangle( ndx, ndx + 1, ndx + 2 );
	RB_STVEFTriangle( ndx + 2, ndx + 1, ndx + 3 );
}

/* 0x4bb3c0: from origin to oldorigin, across the plane through the eye and
 * both ends, the entity's radius either side. */
static void RB_SurfaceJKALine( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t toStart, toEnd, across;
	VectorSubtract( e->origin, backEnd.viewParms.or.origin, toStart );
	VectorSubtract( e->oldorigin, backEnd.viewParms.or.origin, toEnd );
	CrossProduct( toStart, toEnd, across );
	VectorNormalize( across );
	RB_JKALineQuad( e->origin, e->oldorigin, across, e->radius, 1.0f );
}

/* 0x4bb4c0: the same quad across the entity's own second axis, half the
 * line's width either side, t running to its texture scale. */
static void RB_SurfaceJKAOrientedLine( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t across;
	VectorCopy( e->axis[1], across );
	VectorNormalize( across );
	RB_JKALineQuad( e->origin, e->oldorigin, across, e->jka.lineWidth * 0.5f, e->jka.lineSTScale );
}

/* Returns qtrue when the current entity is one of Academy's primitives. */
static qboolean RB_SurfaceJKA( void ) {
	switch ( backEnd.currentEntity->e.reType ) {
	case RT_JKA_SABER_GLOW: RB_SurfaceJKASaberGlow(); return qtrue;
	case RT_JKA_LINE: RB_SurfaceJKALine(); return qtrue;
	case RT_JKA_ORIENTEDLINE: RB_SurfaceJKAOrientedLine(); return qtrue;
	default: return qfalse;
	}
}

#endif /* FNTECH3_TR_JKA_SURFACE_H */
