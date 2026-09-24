/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_BEAM_SHADER_H
#define FNTECH3_TR_Q3_IHV_BEAM_SHADER_H

/* The original immediate beam path uses white, red vertex color and ONE/ONE
 * blending with depth testing and no depth writes. Express that state through
 * the ordinary material pipeline for every backend. */
static void R_CreateIHVBeamShader( void ) {
	tr.ihvBeamShader = NULL;
	if ( ri.gameId != COM_GAME_QUAKE3_IHV ) return;
	InitShader( "<ihv-beam>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.whiteImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
#ifdef USE_VULKAN
	stages[0].bundle[0].rgbGen = CGEN_EXACT_VERTEX;
	stages[0].bundle[0].alphaGen = AGEN_VERTEX;
#else
	stages[0].rgbGen = CGEN_EXACT_VERTEX;
	stages[0].alphaGen = AGEN_VERTEX;
#endif
	stages[0].stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
	shader.sort = SS_BLEND0;
	tr.ihvBeamShader = FinishShader();
}

#endif
