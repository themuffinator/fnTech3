/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_SHADER_MATERIAL_H
#define FNTECH3_RTCW_SHADER_MATERIAL_H

/* Original RTCW SP/MP tr_local.h allows sixteen frames in ordinary shaders.
 * The shared bundle also has room for the existing 24-frame shaderx extension.
 */
#define MAX_IMAGE_ANIMATIONS_RTCW 16

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

static imgFlags_t R_RTCWShaderImageFlags( const char *name, imgFlags_t flags, int policy,
	qboolean characterMip ) {
	if ( ri.gameId == COM_GAME_RTCW_SP || ri.gameId == COM_GAME_RTCW_MP ) {
		/* Original RTCW exempts lightmaps, skies and weapon images. Mode 2
		 * additionally requires each material to opt into compression.
		 */
		if ( policy < 0 || ( r_ext_compressed_textures->integer == 2 && policy != 1 ) ||
			!strncmp( name, "*lightmap", 9 ) || strstr( name, "skies" ) || strstr( name, "weapons" ) )
			flags = (imgFlags_t)( flags | IMGFLAG_NO_COMPRESSION );
	}
	if ( ri.gameId == COM_GAME_RTCW_SP && characterMip && ( flags & IMGFLAG_PICMIP ) )
		flags = (imgFlags_t)( flags | IMGFLAG_RTCW_CHARACTER_MIP );
	return flags;
}

typedef enum {
	RTCW_MAP_UNCHANGED, RTCW_MAP_STATIC, RTCW_MAP_ANIMATED,
	RTCW_MAP_SKIP_STATIC, RTCW_MAP_SKIP_ANIMATED
} rtcwMapDirective_t;

/* Original SP/MP select these alternatives using the negotiated framebuffer
 * and compression mode, not the image's material-local compression policy.
 */
static rtcwMapDirective_t R_RTCWMapDirective( const char *token, int colorBits,
	int compressionAvailable, int compressionEnabled ) {
	int selected;
	qboolean animated = qfalse;
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP ) return RTCW_MAP_UNCHANGED;
	if ( !Q_stricmp( token, "map16" ) ) selected = colorBits <= 16;
	else if ( !Q_stricmp( token, "map32" ) ) selected = colorBits > 16;
	else if ( !Q_stricmp( token, "mapcomp" ) ) selected = compressionAvailable && compressionEnabled;
	else if ( !Q_stricmp( token, "mapnocomp" ) ) selected = !compressionAvailable;
	else if ( !Q_stricmp( token, "animmapcomp" ) ) {
		animated = qtrue;
		selected = compressionAvailable && compressionEnabled;
	} else if ( !Q_stricmp( token, "animmapnocomp" ) ) {
		animated = qtrue;
		selected = !compressionAvailable;
	} else return RTCW_MAP_UNCHANGED;
	return selected ? ( animated ? RTCW_MAP_ANIMATED : RTCW_MAP_STATIC ) :
		( animated ? RTCW_MAP_SKIP_ANIMATED : RTCW_MAP_SKIP_STATIC );
}

/* An unselected static map owns one token, an animation owns the remainder of
 * its line. Keep closing braces for the stage parser even on malformed input.
 */
static qboolean R_RTCWSkipMapArguments( const char **text, qboolean animated ) {
	int arguments = 0;
	do {
		const char *before = *text;
		const char *argument = COM_ParseExt( text, qfalse );
		if ( !argument[0] ) break;
		if ( !strcmp( argument, "{" ) || !strcmp( argument, "}" ) ) {
			*text = before;
			break;
		}
		++arguments;
	} while ( animated );
	return arguments >= ( animated ? 2 : 1 ) ? qtrue : qfalse;
}
#endif
