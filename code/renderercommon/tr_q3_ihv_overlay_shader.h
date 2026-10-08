/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_OVERLAY_SHADER_H
#define FNTECH3_TR_Q3_IHV_OVERLAY_SHADER_H

/* The IHV Test's overlay square (R_BlendBlobs 0x442700) binds its shader's
 * first stage image (0x442764..0x442771) and draws it in the overlay colour,
 * glColor4fv of the scene's four overlay words (0x4427b4), with alpha
 * blending and no depth test (state 0x10065, 0x442733); the stage's own colour
 * and alpha rules are not read. Its client game draws its damage blood splat
 * that way, with models/weaphits/blood201, whose stage asks for rgbGen
 * entity -- which a 2D picture here would draw black. This returns a 2D
 * shader holding that first image with the vertex colour and alpha, once per
 * source shader; 0 when the source has no active first stage, where the
 * original draws nothing ("R_BlendBlobs attempted to use inactive shader"). */
qhandle_t RE_RegisterIHVOverlayShader( qhandle_t hShader ) {
	const shader_t *source;
	const shaderStage_t *first;
	shader_t *found;
	image_t *image;
	char name[MAX_QPATH];
	if ( ri.gameId != COM_GAME_QUAKE3_IHV || !tr.registered ) return 0;
	source = R_GetShaderByHandle( hShader );
	if ( !source || source->defaultShader ) return 0;
	first = source->stages[0];
	if ( !first || !first->active || !first->bundle[0].image[0] ) return 0;
	image = first->bundle[0].image[0];
	Com_sprintf( name, sizeof( name ), "%s#ihvoverlay", source->name );
	found = R_FindShaderByName( name );
	if ( found && found != tr.defaultShader && !found->defaultShader ) return found->index;
	InitShader( name, LIGHTMAP_2D );
	stages[0].bundle[0].image[0] = image;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
#ifdef USE_VULKAN
	stages[0].bundle[0].rgbGen = CGEN_VERTEX;
	stages[0].bundle[0].alphaGen = AGEN_VERTEX;
#else
	stages[0].rgbGen = CGEN_VERTEX;
	stages[0].alphaGen = AGEN_VERTEX;
#endif
	stages[0].stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHTEST_DISABLE;
	shader.cullType = CT_TWO_SIDED;
	shader.sort = SS_BLEND0;
	return FinishShader()->index;
}

#endif
