/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_JKA_ENTITY_H
#define FNTECH3_TR_JKA_ENTITY_H

/*
 * Jedi Academy multiplayer's entity colour and alpha overrides, as jamp.exe
 * 1.01's renderer applies them. The client adapter turns the client game's
 * render flags 0x800 and 0x400 into refEntity_t.jka.
 *
 * - Tint (0x800). The stage iterator (0x4aef7e) hands the colour function
 *   (0x4ae280) the entity colour source (its case 3, 0x4ae689) in place of
 *   each stage's own, for single- and multitexture stages alike. Every
 *   vertex takes the entity's four bytes; an identity alpha still writes
 *   255 afterwards, since the forced source is neither identity nor vertex
 *   (0x4ae70a). The stage's own alpha source is otherwise kept.
 * - Forced alpha (0x400). A single-texture stage (0x4af168) takes the
 *   entity's alpha at every vertex (0x4aed40), whatever it is, and is drawn
 *   with source-alpha blending in place of all its own state, so with no
 *   depth write and no alpha test (0x65). Multitexture stages keep theirs.
 * - The surface list holds a forced-alpha entity's surfaces back until the
 *   rest of the view is drawn (0x48aba6, mask 0x202400), since they write no
 *   depth; a distortion copy (0x2000) is held back by the same test, so it
 *   refracts a finished scene (renderercommon/tr_entity_refraction.h). Fn
 *   draws them after the others in its own sort order; retail draws its
 *   post-render list in reverse and caps it at 128 surfaces.
 *
 * Each renderer's tr_shade.c, tr_backend.c and tr_main.c include this after
 * tr_local.h. Shadow-map passes take neither override.
 */

#define JKA_FORCED_ALPHA_STATE ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )

#ifdef FNTECH3_TR_STVEF_ALPHA_H
/* Elite Force's forced alpha draws with the same state, so the Vulkan
 * renderers build one forced pipeline for both games. */
typedef char rJKAForcedAlphaStateIsSTVEF[ ( JKA_FORCED_ALPHA_STATE == STVEF_FORCED_ALPHA_STATE ) ? 1 : -1 ];
#endif

/* Whether an entity carries either override. */
static ID_INLINE qboolean R_JKAEntityOverrides( const trRefEntity_t *entity ) {
	return entity && ( entity->e.jka.rgbTint || entity->e.jka.forceAlpha ) ? qtrue : qfalse;
}

static ID_INLINE qboolean RB_JKAShadowPass( void ) {
#ifdef USE_PMLIGHT
	if ( tess.csmCasterPass || ( backEnd.viewParms.passFlags & VPF_DLIGHT_SHADOW ) ) {
		return qtrue;
	}
#endif
	return qfalse;
}

/* Whether the batch's entity gives its stages its colour. */
static ID_INLINE qboolean RB_JKAStageTint( void ) {
	const trRefEntity_t *entity = backEnd.currentEntity;

	return entity && entity->e.jka.rgbTint && !RB_JKAShadowPass() ? qtrue : qfalse;
}

/* The colour source a stage's bundle computes: the entity's for the first
 * bundle under a tint, which carries the stage's one colour in retail. */
static ID_INLINE colorGen_t RB_JKAStageRGBGen( colorGen_t rgbGen, int bundle ) {
	return bundle == 0 && RB_JKAStageTint() ? CGEN_ENTITY : rgbGen;
}

/* Whether a stage of the batch takes its entity's alpha, and which. */
static ID_INLINE qboolean RB_JKAStageAlpha( const shaderStage_t *stage, int *alpha ) {
	const trRefEntity_t *entity = backEnd.currentEntity;

	if ( !entity || !entity->e.jka.forceAlpha || stage->bundle[1].image[0] || RB_JKAShadowPass() ) {
		return qfalse;
	}
	*alpha = entity->e.shader.rgba[3];
	return qtrue;
}

/* Whether retail's surface list posts an entity: forced alpha or distortion. */
static ID_INLINE qboolean R_JKAEntityDrawsLast( const trRefEntity_t *entity ) {
	return entity->e.jka.forceAlpha || entity->e.jka.distortion ? qtrue : qfalse;
}

/* Whether a sorted surface belongs to a posted entity of this scene. */
static ID_INLINE qboolean R_JKASurfaceDrawsLast( const drawSurf_t *drawSurf ) {
	const int entityNum = ( drawSurf->sort >> QSORT_REFENTITYNUM_SHIFT ) & REFENTITYNUM_MASK;

	return entityNum != REFENTITYNUM_WORLD && entityNum < tr.refdef.num_entities &&
		R_JKAEntityDrawsLast( &tr.refdef.entities[entityNum] ) ? qtrue : qfalse;
}

/* Moves the surfaces posted entities draw to the end of a sorted list,
 * keeping the order within both parts. */
static ID_INLINE void R_JKADeferForcedAlphaSurfaces( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	drawSurf_t *deferred;
	int i, kept, count;

	for ( i = 0; i < tr.refdef.num_entities; i++ ) {
		if ( R_JKAEntityDrawsLast( &tr.refdef.entities[i] ) ) {
			break;
		}
	}
	if ( i == tr.refdef.num_entities ) {
		return;
	}
	for ( i = count = 0; i < numDrawSurfs; i++ ) {
		if ( R_JKASurfaceDrawsLast( &drawSurfs[i] ) ) {
			count++;
		}
	}
	if ( count == 0 || count == numDrawSurfs ) {
		return;
	}
	deferred = (drawSurf_t *)ri.Hunk_AllocateTempMemory( count * (int)sizeof( *deferred ) );
	for ( i = kept = count = 0; i < numDrawSurfs; i++ ) {
		if ( R_JKASurfaceDrawsLast( &drawSurfs[i] ) ) {
			deferred[count++] = drawSurfs[i];
		} else {
			drawSurfs[kept++] = drawSurfs[i];
		}
	}
	Com_Memcpy( drawSurfs + kept, deferred, count * sizeof( *deferred ) );
	ri.Hunk_FreeTempMemory( deferred );
}

#endif /* FNTECH3_TR_JKA_ENTITY_H */
