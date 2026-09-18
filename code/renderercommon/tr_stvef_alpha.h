/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_ALPHA_H
#define FNTECH3_TR_STVEF_ALPHA_H

/*
 * Elite Force's forced entity alpha, render flag 0x800, as stvoyHM.exe 1.20
 * applies it. When the surface list flushes a batch (RB_RenderDrawSurfList
 * 0x463831), the entity the batch was drawn for sets the alpha: its own when
 * that is from 1 to 254, otherwise none. Every stage with a single texture
 * then takes that alpha at each vertex and is drawn with source-alpha
 * blending in place of all of its own state, so with no depth write and no
 * alpha test (RB_StageIteratorGeneric 0x46c5e2). Multitexture stages keep
 * their state, and the list's final batch is flushed without the alpha.
 *
 * Each renderer's tr_backend.c and tr_shade.c include this after tr_local.h.
 * Shadow-map passes are not colour passes and never take the alpha.
 */

#define STVEF_FORCED_ALPHA_STATE ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )

static ID_INLINE int R_STVEFForcedAlpha( const trRefEntity_t *entity ) {
	if ( !entity || !entity->e.stvef.forceAlpha ) {
		return 0;
	}
	return entity->e.shader.rgba[3] < 255 ? entity->e.shader.rgba[3] : 0;
}

/* Flushes one batch of the surface list with its entity's forced alpha. */
static ID_INLINE void RB_EndSurfaceWithSTVEFAlpha( void ) {
	backEnd.stvefForcedAlpha = R_STVEFForcedAlpha( backEnd.currentEntity );
	RB_EndSurface();
	backEnd.stvefForcedAlpha = 0;
}

/* The alpha a stage of the batch being flushed takes, or 0 to keep its own. */
static ID_INLINE int RB_STVEFStageAlpha( const shaderStage_t *stage ) {
	if ( !backEnd.stvefForcedAlpha || stage->bundle[1].image[0] ) {
		return 0;
	}
#ifdef USE_PMLIGHT
	if ( tess.csmCasterPass || ( backEnd.viewParms.passFlags & VPF_DLIGHT_SHADOW ) ) {
		return 0;
	}
#endif
	return backEnd.stvefForcedAlpha;
}

static ID_INLINE void RB_STVEFSetVertexAlpha( color4ub_t *colors, int count, int alpha ) {
	int i;
	for ( i = 0; i < count; ++i ) {
		colors[i].rgba[3] = (byte)alpha;
	}
}

#endif /* FNTECH3_TR_STVEF_ALPHA_H */
