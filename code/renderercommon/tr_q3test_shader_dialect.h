/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3TEST_SHADER_DIALECT_H
#define FNTECH3_TR_Q3TEST_SHADER_DIALECT_H

/* The Q3Test releases' shader keywords that Quake III 1.32 dropped or
 * renamed (retail ParseShader, 1.08 0x438570; docs/fntech3/Q3TEST.md):
 *
 *   fogParms r g b depth type   five bare values, no parentheses; the type's
 *                               sign names the fog volume's visible face
 *                               (+z above zero, -z below, none at zero),
 *                               which R_LoadFogs takes from the shader;
 *   fogonly                     a shader that is only a fog volume;
 *   foggen <wave> base amp phase freq
 *                               a fog depth wave, carried only by an unused
 *                               test shader and not drawn here;
 *   clampTexCoords              a stage keyword that clamps the stage's
 *                               texture coordinates (a state bit in retail,
 *                               0x4388e5), wherever it stands in the stage.
 *
 * They are read only under the q3test profile. What the keywords describe is
 * drawn with this engine's own fog and images (AGENTS.md invariant 11). */

static qboolean R_Q3TestShaderDialect( void ) {
	return ri.gameId == COM_GAME_Q3TEST ? qtrue : qfalse;
}

/* Called from the fogParms branch after the keyword. Returns 0 when the
 * profile or the syntax is not Q3Test's, 1 when parsed. Retail warns about a
 * missing value and keeps the shader. */
static int R_Q3TestFogParms( const char **text ) {
	const char *cursor = *text;
	const char *token;
	float values[4];
	int i;
	if ( !R_Q3TestShaderDialect() ) return 0;
	token = COM_ParseExt( &cursor, qfalse );
	if ( !strcmp( token, "(" ) ) return 0;
	cursor = *text;
	for ( i = 0; i < 4; ++i ) {
		token = COM_ParseExt( &cursor, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name );
			*text = cursor;
			return 1;
		}
		values[i] = Q_atof( token );
	}
	token = COM_ParseExt( &cursor, qfalse );
	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name );
		*text = cursor;
		return 1;
	}
	VectorCopy( values, shader.fogParms.color );
	shader.fogParms.depthForOpaque = values[3];
	shader.fogParms.q3testType = atoi( token );
	*text = cursor;
	return 1;
}

/* The general keywords fogonly and foggen: 1 when handled, else 0. */
static int R_Q3TestFogDirective( const char *keyword, const char **text ) {
	int i;
	if ( !R_Q3TestShaderDialect() ) return 0;
	if ( !Q_stricmp( keyword, "fogonly" ) ) {
		shader.contentFlags |= CONTENTS_FOG;
		return 1;
	}
	if ( Q_stricmp( keyword, "foggen" ) ) return 0;
	for ( i = 0; i < 5; ++i ) {
		if ( !COM_ParseExt( text, qfalse )[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing foggen parm in shader '%s'\n", shader.name );
			break;
		}
	}
	return 1;
}

/* Whether the stage whose text starts here carries clampTexCoords, which may
 * follow the map it clamps. Comments and quoted strings are skipped as the
 * tokenizer skips them; the scan ends at the stage's closing brace. */
static qboolean R_Q3TestStageClamps( const char *text ) {
	int depth = 0;
	while ( text && *text ) {
		const char *start;
		size_t length;
		if ( text[0] == '/' && text[1] == '/' ) {
			while ( *text && *text != '\n' ) ++text;
			continue;
		}
		if ( text[0] == '/' && text[1] == '*' ) {
			text += 2;
			while ( *text && !( text[0] == '*' && text[1] == '/' ) ) ++text;
			if ( *text ) text += 2;
			continue;
		}
		if ( (unsigned char)*text <= ' ' ) {
			++text;
			continue;
		}
		if ( *text == '"' ) {
			++text;
			while ( *text && *text != '"' ) ++text;
			if ( *text ) ++text;
			continue;
		}
		start = text;
		while ( (unsigned char)*text > ' ' ) ++text;
		length = (size_t)( text - start );
		if ( length == 1 && *start == '{' ) {
			++depth;
		} else if ( length == 1 && *start == '}' ) {
			if ( depth-- == 0 ) return qfalse;
		} else if ( depth == 0 && length == 14 ) {
			static const char keyword[] = "clamptexcoords";
			size_t i;
			for ( i = 0; i < length && ( start[i] | 0x20 ) == keyword[i]; ++i ) {
			}
			if ( i == length ) return qtrue;
		}
	}
	return qfalse;
}

/* 1.02-1.05's sky shader (textures/skies/blacksky) carries `surfaceparm
 * sky` and no stage: retail's renderer read no surfaceparm and had nothing to
 * draw on those faces, and 1.06 gave the same shader a black stage
 * (gfx/colors/black.tga). A stageless Q3Test sky face is drawn black the
 * same way. */
static const char R_Q3TestBlackStage[] = "map *white\nrgbGen const ( 0 0 0 )\n}\n";
static qboolean R_Q3TestStagelessSky( int surfaceFlags, qboolean isSky ) {
	return R_Q3TestShaderDialect() && ( surfaceFlags & SURF_SKY ) && !isSky ? qtrue : qfalse;
}

/* Whether the next value names an image file rather than $lightmap,
 * $whiteimage or a built-in image. */
static qboolean R_Q3TestImageFollows( const char *text ) {
	while ( text && *text && (unsigned char)*text <= ' ' && *text != '\n' ) ++text;
	return text && *text && *text != '\n' && *text != '$' && *text != '*' ? qtrue : qfalse;
}

#endif
