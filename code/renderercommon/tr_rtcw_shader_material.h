/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_SHADER_MATERIAL_H
#define FNTECH3_RTCW_SHADER_MATERIAL_H

/* Compression directives belong to the material being parsed. Do not let a
 * previous material's opt-out affect later registrations or another profile.
 */
static qboolean R_RTCWShaderCompressionDirective( const char *token, int *policy ) {
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP ) return qfalse;
	if ( !Q_stricmp( token, "nocompress" ) ) {
		*policy = -1;
		return qtrue;
	}
	if ( !Q_stricmp( token, "allowcompress" ) ) {
		*policy = 1;
		return qtrue;
	}
	return qfalse;
}

static imgFlags_t R_RTCWShaderImageFlags( const char *name, imgFlags_t flags, int policy ) {
	if ( ri.gameId == COM_GAME_RTCW_SP || ri.gameId == COM_GAME_RTCW_MP ) {
		/* Original RTCW exempts lightmaps, skies and weapon images. Mode 2
		 * additionally requires each material to opt into compression.
		 */
		if ( policy < 0 || ( r_ext_compressed_textures->integer == 2 && policy != 1 ) ||
			!strncmp( name, "*lightmap", 9 ) || strstr( name, "skies" ) || strstr( name, "weapons" ) )
			flags = (imgFlags_t)( flags | IMGFLAG_NO_COMPRESSION );
	}
	return flags;
}
#endif
