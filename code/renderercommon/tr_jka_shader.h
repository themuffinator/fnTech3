/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_SHADER_H
#define FNTECH3_JKA_SHADER_H
/* Retail jasp0x47b069..7b sets the material's noTC byte at0xcd3013.
 * Its map/clampmap/animmap/sky image requests invert that byte into the
 * compression-permitted argument (0x4798af,0x47993b,0x47a4a0,0x47aa8c).
 * Adapt that data into fnTech3's existing image flags; mip selection,
 * lighting and the renderer's feature pipeline retain their own policies. */
static qboolean R_JKAShaderProfile( void ) {
	return ri.gameId == COM_GAME_JKA_SP || ri.gameId == COM_GAME_JKA_MP ? qtrue : qfalse;
}
static qboolean R_JKAShaderNoTCDirective( const char *token, qboolean *noTC ) {
	if ( R_JKAShaderProfile() && !Q_stricmp( token, "noTC" ) ) {
		*noTC = qtrue;
		return qtrue;
	}
	return qfalse;
}
static imgFlags_t R_JKAShaderImageFlags( imgFlags_t flags, qboolean noTC ) {
	return R_JKAShaderProfile() && noTC ? (imgFlags_t)( flags | IMGFLAG_NO_COMPRESSION ) : flags;
}
/* Retail reads animMap, clampAnimMap and oneShotAnimMap in one ParseStage
 * branch (jamp.exe 0x4b426c, jasp.exe 0x47a42e). clampAnimMap loads its
 * frames clamped (GL_CLAMP), oneShotAnimMap sets the bundle's one-shot byte
 * (stage +0x1d, bundle +0x19), and all three keep up to 32 frames
 * (0x4b42e4, 0x47a497). R_BindAnimatedImage (jamp.exe 0x4ac3d0, jasp.exe
 * 0x472fd0) then holds a one-shot bundle on its last frame instead of
 * wrapping. */
#define MAX_IMAGE_ANIMATIONS_JKA 32
static inline qboolean R_JKAClampAnimMapKeyword( const char *token ) {
	return R_JKAShaderProfile() && !Q_stricmp( token, "clampAnimMap" ) ? qtrue : qfalse;
}
static inline qboolean R_JKAOneShotAnimMapKeyword( const char *token ) {
	return R_JKAShaderProfile() && !Q_stricmp( token, "oneShotAnimMap" ) ? qtrue : qfalse;
}
#endif
