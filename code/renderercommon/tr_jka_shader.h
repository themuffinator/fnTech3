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
#endif
