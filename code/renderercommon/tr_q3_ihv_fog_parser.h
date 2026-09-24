/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_FOG_PARSER_H
#define FNTECH3_TR_Q3_IHV_FOG_PARSER_H
#include <errno.h>
#include <limits.h>

static qboolean R_Q3IHVShaderInt( const char **text, int *value ) {
	const char *cursor = *text;
	const char *token = COM_ParseExt( &cursor, qfalse );
	char *end;
	long number;
	if ( !*token ) return qfalse;
	errno = 0;
	number = strtol( token, &end, 10 );
	if ( errno == ERANGE || end == token || *end || number < INT_MIN || number > INT_MAX ) return qfalse;
	*value = (int)number; *text = cursor;
	return qtrue;
}

/* Called from the fogparms branch, after the keyword has been recognized.
 * A lookahead uses COM's token buffer, so callers must not reuse that keyword. */
static int R_Q3IHVFogParms( const char **text ) {
	const char *cursor = *text;
	fogParms_t parsed = shader.fogParms;
	int i;
	if ( ( ri.gameId != COM_GAME_QUAKE3 && ri.gameId != COM_GAME_QUAKE3_IHV ) ) return 0;
	if ( !strcmp( COM_ParseExt( &cursor, qfalse ), "(" ) ) return 0;
	cursor = *text;
	for ( i = 0; i < 3; ++i ) {
		if ( !R_Q3IHVShaderFloat( &cursor, &parsed.color[i] ) ||
			parsed.color[i] < 0 || parsed.color[i] > 1 ) return -1;
	}
	if ( !R_Q3IHVShaderFloat( &cursor, &parsed.depthForOpaque ) ||
		!R_Q3IHVShaderInt( &cursor, &parsed.ihv.gradient ) ) return -1;
	parsed.ihv.enabled = 1;
	shader.fogParms = parsed;
	*text = cursor;
	return 1;
}

static int R_Q3IHVFogDirective( const char *keyword, const char **text ) {
	const char *token, *cursor = *text;
	q3IHVFogParms_t parsed = shader.fogParms.ihv;
	if ( ( ri.gameId != COM_GAME_QUAKE3 && ri.gameId != COM_GAME_QUAKE3_IHV ) ) return 0;
	if ( !Q_stricmp( keyword, "fogonly" ) ) {
		shader.contentFlags |= CONTENTS_FOG;
		shader.fogParms.ihv.enabled = 1;
		return 1;
	}
	if ( Q_stricmp( keyword, "foggen" ) ) return 0;
	token = COM_ParseExt( &cursor, qfalse );
	if ( !Q_stricmp( token, "sin" ) ) parsed.wave = 1;
	else if ( !Q_stricmp( token, "square" ) ) parsed.wave = 2;
	else if ( !Q_stricmp( token, "triangle" ) ) parsed.wave = 3;
	else return -1;
	if ( !R_Q3IHVShaderFloat( &cursor, &parsed.base ) ||
		!R_Q3IHVShaderFloat( &cursor, &parsed.amplitude ) ||
		!R_Q3IHVShaderFloat( &cursor, &parsed.phase ) ||
		!R_Q3IHVShaderFloat( &cursor, &parsed.frequency ) ) return -1;
	parsed.enabled = 1;
	shader.fogParms.ihv = parsed;
	*text = cursor;
	return 1;
}
#endif
