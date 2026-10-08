/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_2D_H
#define FNTECH3_TR_Q3_IHV_2D_H

/* Independently implemented from the supplied 4.01 IHV executable; see
 * docs/fntech3/Q3_IHV.md. Included by each renderer's tr_shader.c, which owns
 * `shader` and `stages`.
 *
 * The IHV build draws a 2D picture as its shader's first image times the
 * current 2D colour, whatever the stage asks for (Draw_StretchPic 0x43fd60,
 * the colour set by 0x43fd20), and its nomipmaps shaders are exactly its 2D
 * art (scripts/gfx.shader). So an IHV nomipmaps stage that would ignore that
 * colour takes it instead: an identity rgbGen, set or defaulted, becomes the
 * vertex colour, and an identity or skipped alphaGen the vertex alpha. The HUD
 * counters, the scoreboard's bar and coloured text depend on it. Stages that
 * already colour themselves keep their own rule, and no other profile, retail
 * Q3's legacy-asset path included, is touched. */
static void R_Q3IHV2DStageColors( void ) {
	int i;
	if ( ri.gameId != COM_GAME_QUAKE3_IHV || !shader.noMipMaps ) return;
	for ( i = 0; i < MAX_SHADER_STAGES; ++i ) {
		shaderStage_t *stage = &stages[i];
#ifdef USE_VULKAN
		textureBundle_t *gen = &stage->bundle[0];   // the Vulkan renderers keep them per bundle
#else
		shaderStage_t *gen = stage;
#endif
		if ( !stage->active ) break;
		if ( gen->rgbGen == CGEN_IDENTITY || gen->rgbGen == CGEN_IDENTITY_LIGHTING ) gen->rgbGen = CGEN_VERTEX;
		if ( gen->alphaGen == AGEN_IDENTITY || gen->alphaGen == AGEN_SKIP ) gen->alphaGen = AGEN_VERTEX;
	}
}

#endif
