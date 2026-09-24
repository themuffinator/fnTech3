/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_SHADER_H
#define FNTECH3_TR_Q3_IHV_SHADER_H

/* Independently implemented from the supplied 4.01 IHV executable. See
 * docs/fntech3/Q3_IHV.md for binary identity and observations. Both q3-ihv and
 * retail Q3's legacy-asset path accept these material commands; accepting an
 * asset dialect never changes the selected module or network contracts. */
static qboolean R_Q3IHVFromClient( const char *token ) {
	return ( ri.gameId == COM_GAME_QUAKE3 || ri.gameId == COM_GAME_QUAKE3_IHV ) && !Q_stricmp( token, "fromClient" ) ? qtrue : qfalse;
}

static qboolean R_Q3IHVDepthMask( const char *token ) {
	/* Original 0x43b45f sets the depth-write state bit (0x100). */
	return ( ri.gameId == COM_GAME_QUAKE3 || ri.gameId == COM_GAME_QUAKE3_IHV ) && !Q_stricmp( token, "depthMask" ) ? qtrue : qfalse;
}

static qboolean R_Q3IHVSeparateSkyStages( void ) {
	return ( ri.gameId == COM_GAME_QUAKE3 || ri.gameId == COM_GAME_QUAKE3_IHV ) &&
		( shader.sky.ihvClouds || shader.sky.ihvLightning ) ? qtrue : qfalse;
}

/* Consume one finite argument on this line without eating a brace on error.
 * Avoid isfinite: renderer builds may enable finite-math-only optimizations. */
static qboolean R_Q3IHVShaderFloat( const char **text, float *value ) {
	const char *cursor = *text;
	const char *token = COM_ParseExt( &cursor, qfalse );
	char *end;
	double number;
	unsigned int bits;
	float parsed;
	if ( !*token || !strcmp( token, "{" ) || !strcmp( token, "}" ) ) return qfalse;
	number = strtod( token, &end );
	if ( *end || number < -FLT_MAX || number > FLT_MAX ) return qfalse;
	parsed = (float)number;
	memcpy( &bits, &parsed, sizeof( bits ) );
	if ( ( bits & 0x7f800000u ) == 0x7f800000u ) return qfalse;
	*value = parsed;
	*text = cursor;
	return qtrue;
}

/* Return 0 for another dialect/command, 1 on success, -1 for bad arguments.
 * Sky names and cloud height/coverage are separate in IHV. Neither command
 * overwrites the other, unlike the later three-argument skyparms directive. */
static int R_Q3IHVSkyDirective( const char *keyword, const char **text ) {
	const char *token;
	char name[MAX_QPATH];
	char pathname[MAX_QPATH];
	static const char *suffix[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	float height;
	qboolean full;
	imgFlags_t flags;
	int i;
	if ( ( ri.gameId != COM_GAME_QUAKE3 && ri.gameId != COM_GAME_QUAKE3_IHV ) ) return 0;
	if ( !Q_stricmp( keyword, "lightning" ) ) {
		shader.sky.ihvLightning = 1;
		return 1;
	}
	if ( !Q_stricmp( keyword, "surfacelight" ) ) {
		/* IHV's renderer skips this compiler-only directive (0x43a802). */
		SkipRestOfLine( text );
		return 1;
	}
	if ( !Q_stricmp( keyword, "cloudparms" ) ) {
		if ( !R_Q3IHVShaderFloat( text, &height ) || height < 0 || height > 65536 ) return -1;
		token = COM_ParseExt( text, qfalse );
		if ( !Q_stricmp( token, "full" ) ) full = qtrue;
		else if ( !Q_stricmp( token, "half" ) ) full = qfalse;
		else return -1;
		shader.sky.cloudHeight = height;
		shader.sky.ihvClouds = full ? 1 : 2;
		shader.isSky = qtrue;
		if ( height > 0 ) R_InitSkyTexCoords( height );
		return 1;
	}
	if ( Q_stricmp( keyword, "sky" ) ) return 0;
	token = COM_ParseExt( text, qfalse );
	if ( !*token || !strcmp( token, "{" ) || !strcmp( token, "}" ) ||
		strlen( token ) + sizeof( "_rt.tga" ) > sizeof( pathname ) ) return -1;
	Q_strncpyz( name, token, sizeof( name ) );
	flags = IMGFLAG_MIPMAP | IMGFLAG_CLAMPTOEDGE;
	if ( !shader.noPicMip ) flags |= IMGFLAG_PICMIP;
	if ( r_neatsky->integer ) flags = IMGFLAG_CLAMPTOEDGE;
	for ( i = 0; i < 6; ++i ) {
		Com_sprintf( pathname, sizeof( pathname ), "%s_%s.tga", name, suffix[i] );
		shader.sky.outerbox[i] = R_FindShaderImage( pathname, flags );
		if ( !shader.sky.outerbox[i] ) shader.sky.outerbox[i] = tr.defaultImage;
	}
	shader.isSky = qtrue;
	return 1;
}

#endif
