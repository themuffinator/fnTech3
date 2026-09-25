/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3TEST_2D_H
#define FNTECH3_TR_Q3TEST_2D_H

/* Q3Test draws a 2D picture as its shader's first image, alpha-blended in the
 * picture's colour, whatever else the shader says: retail's RB_StretchPic
 * (1.08 0x4363f0) binds that image under its own 2D state (0x10065: source
 * alpha blending, no depth test). The releases' menu and HUD art relies on
 * it -- gfx/2d/menuinfo2 is white under an alpha mask and its shader names no
 * blend, and the menu cursor is red under one. Under the q3test profile a
 * picture is drawn with this engine's 2D shader of that image, made once per
 * shader; an animated or imageless first stage keeps the shader itself. */
static shader_t *R_Q3Test2DShader( shader_t *sh ) {
	const shaderStage_t *stage;
	image_t *image;
	char name[MAX_QPATH];
	if ( ri.gameId != COM_GAME_Q3TEST || !sh || sh->defaultShader ) return sh;
	if ( sh->q3test2D ) return R_GetShaderByHandle( sh->q3test2D );
	stage = sh->stages[0];
	image = stage && stage->active && stage->bundle[0].numImageAnimations <= 1 ? stage->bundle[0].image[0] : NULL;
	if ( !image ) {
		sh->q3test2D = sh->index;
		return sh;
	}
	Com_sprintf( name, sizeof( name ), "*q3test2d/%p", (void *)image );
	sh->q3test2D = RE_RegisterShaderFromImage( name, LIGHTMAP_2D, image, qfalse );
	return R_GetShaderByHandle( sh->q3test2D );
}

#endif
