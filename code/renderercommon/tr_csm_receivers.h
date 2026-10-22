/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_CSM_RECEIVERS_H
#define FNTECH3_TR_CSM_RECEIVERS_H

/* Which sun shadows (r_csmShadows) a receiver takes, and how much of its
 * light they take away.
 *
 * The cascade atlas keeps two layers for every cascade: the world's own
 * surfaces (CSM_LAYER_WORLD) and every entity's, brush models among them
 * (CSM_LAYER_MODELS). A model, or a surface its shader lights from the light
 * grid, takes the shadow of either layer, darkened by the sun colour as
 * before (CSM_RECEIVER_ALL).
 *
 * A surface whose light is baked, in a lightmap (CSM_RECEIVER_LIGHTMAP) or in
 * its vertex colours (CSM_RECEIVER_VERTEX), already holds the world's shadow:
 * the compiler traced its sun past the level's own brushes and models. It
 * takes only the models' shadow, only where the world layer leaves it in the
 * sun, and only the sun's share of its light, per channel:
 * S x max(0, N . toSun) / L, held to 0..1, where L is its baked light and S
 * the sun in the same units. Every profile's compiler stored its sun so, in
 * lightmap bytes: q3map's `q3map_sun` as colour x intensity x cos, 2015's and
 * Ritual's worldspawn suns as `suncolor` x cos, which their lightmaps were
 * measured to hold (docs/fntech3/2026-10-16-mohaa-worldspawn-sun.md,
 * docs/fntech3/2026-10-22-fnq-shadow-formats.md). The renderer shifts S as
 * it shifts the lightmaps (csmPlan_t.sunLightmapColor).
 *
 * A world or brush surface lit by neither -- full bright, a constant or
 * entity colour -- is not lit by the sun and takes none (CSM_RECEIVER_NONE).
 *
 * Included by a renderer's tr_backend.c after its types. */

/* The bundle a baked receiver's light is read from: a lightmap that the
 * stage maps with tcGen lightmap. A white stand-in (no page, full bright) is
 * no baked light. */
static const textureBundle_t *R_CSMReceiverLightmapBundle( const shader_t *shader ) {
	int stage, b;

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
		const shaderStage_t *pStage = shader->stages[stage];

		if ( !pStage || !pStage->active ) {
			break;
		}
		for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
			const textureBundle_t *bundle = &pStage->bundle[b];

			if ( bundle->lightmap != LIGHTMAP_INDEX_NONE && bundle->tcGen == TCGEN_LIGHTMAP &&
				bundle->image[0] && bundle->image[0] != tr.whiteImage ) {
				return bundle;
			}
		}
	}
	return NULL;
}

/* The Vulkan renderers give each bundle its own colour; GLx, each stage. */
#ifdef USE_VULKAN
#define R_CSM_RGBGEN_BUNDLES NUM_TEXTURE_BUNDLES
#define R_CSM_RGBGEN( pStage, b ) ( pStage )->bundle[b].rgbGen
#else
#define R_CSM_RGBGEN_BUNDLES 1
#define R_CSM_RGBGEN( pStage, b ) ( pStage )->rgbGen
#endif

/* model: an entity that is not a brush model. */
static int R_CSMReceiverKind( const shader_t *shader, qboolean model, const textureBundle_t **lightmap ) {
	int stage, b;

	*lightmap = NULL;
	if ( !shader ) {
		return CSM_RECEIVER_NONE;
	}
	if ( model ) {
		return CSM_RECEIVER_ALL;
	}

	*lightmap = R_CSMReceiverLightmapBundle( shader );
	if ( *lightmap ) {
		return CSM_RECEIVER_LIGHTMAP;
	}

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
		const shaderStage_t *pStage = shader->stages[stage];

		if ( !pStage || !pStage->active ) {
			break;
		}
		for ( b = 0; b < R_CSM_RGBGEN_BUNDLES; b++ ) {
			switch ( R_CSM_RGBGEN( pStage, b ) ) {
				case CGEN_EXACT_VERTEX:
				case CGEN_VERTEX:
				case CGEN_MOHAA_LIGHTING:	/* the map's colours, on the world */
					return CSM_RECEIVER_VERTEX;
				case CGEN_LIGHTING_DIFFUSE:
					return CSM_RECEIVER_ALL;
				default:
					break;
			}
		}
	}
	return CSM_RECEIVER_NONE;
}

/* Whether a receiver can join the batch in progress: the same kind, and for
 * lightmaps the same page reached the same way. */
static qboolean R_CSMReceiverBatchMatches( int kind, const textureBundle_t *lightmap,
	int batchKind, const textureBundle_t *batchLightmap ) {
	if ( kind != batchKind ) {
		return qfalse;
	}
	if ( kind != CSM_RECEIVER_LIGHTMAP || lightmap == batchLightmap ) {
		return qtrue;
	}
	return ( lightmap && batchLightmap &&
		lightmap->image[0] == batchLightmap->image[0] &&
		lightmap->numTexMods == 0 && batchLightmap->numTexMods == 0 ) ? qtrue : qfalse;
}

#endif
