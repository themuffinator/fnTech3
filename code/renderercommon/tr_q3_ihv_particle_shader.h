/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_PARTICLE_SHADER_H
#define FNTECH3_TR_Q3_IHV_PARTICLE_SHADER_H

/* Original 0x43c570: exact vertex RGBA, depth testing without depth writes,
 * ONE / ONE_MINUS_SRC_ALPHA, and the procedural *particle image. */
static void R_CreateIHVParticleShader( void ) {
	byte rgba[16 * 16 * 4];
	image_t *image;
	tr.ihvParticleShader = NULL;
	if ( ri.gameId != COM_GAME_QUAKE3_IHV ) return;
	Q3IHV_ParticleImage( rgba );
	image = R_CreateImage( "*particle", NULL, rgba, 16, 16, IMGFLAG_NONE );
	InitShader( "<particle>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = image;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
#ifdef USE_VULKAN
	stages[0].bundle[0].rgbGen = CGEN_EXACT_VERTEX;
	stages[0].bundle[0].alphaGen = AGEN_VERTEX;
#else
	stages[0].rgbGen = CGEN_EXACT_VERTEX;
	stages[0].alphaGen = AGEN_VERTEX;
#endif
	stages[0].stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	shader.sort = SS_BLEND0;
	tr.ihvParticleShader = FinishShader();
}

#endif
