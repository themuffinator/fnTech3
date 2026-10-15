/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_JEDI_SPRITE_SHADER_H
#define FNTECH3_TR_JEDI_SPRITE_SHADER_H

/*
 * Raven's surface-sprite stage words, read as all four Jedi engines read
 * them (record 2026-10-15-jedi-surface-sprites). Each renderer's tr_shader.c
 * includes this after tr_jka_shader.h; it supplies shader, stages, texMods,
 * InitShader and FinishShader.
 *
 * ParseStage (jamp.exe 0x4b3fef, jasp.exe 0x47a1a6, jk2mp.exe 0x48d712,
 * jk2sp.exe 0x47d542) sends `surfaceSprites`, and then any word starting
 * with "ss" in any case, the rest of the line joined with single spaces, and
 * reads on: a bad or unknown sprite word only warns, so it never fails the
 * shader. Retail joins the words into a 1024-byte buffer without a bound;
 * this one stops at the buffer's end.
 */

#define JEDI_SPRITE_LINE_SIZE 1024
#define JEDI_SPRITE_PARAM_SIZE 128

static shader_t *FinishShader( void );
static void InitShader( const char *name, int lightmapIndex );

static qboolean R_JediSurfaceSpritesKeyword( const char *token ) {
	return R_JediShaderProfile() && !Q_stricmp( token, "surfaceSprites" ) ? qtrue : qfalse;
}

static qboolean R_JediSpriteOptionKeyword( const char *token ) {
	return R_JediShaderProfile() && !Q_stricmpn( token, "ss", 2 ) ? qtrue : qfalse;
}

static void R_JediSpriteLine( const char **text, char *line, int size ) {
	const char *token;

	line[0] = '\0';
	while ( 1 ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			break;
		}
		Q_strcat( line, size, token );
		Q_strcat( line, size, " " );
	}
}

/* The next word of a sprite line, or NULL after warning that it is missing. */
static const char *R_JediSpriteWord( const char **text, const char *missing ) {
	const char *token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing %s in shader '%s'\n", missing, shader.name );
		return NULL;
	}
	return token;
}

/*
 * surfaceSprites <type> <width> <height> <density> <fadedist>
 * (jamp.exe 0x4b2cb0, jasp.exe 0x478de0, jk2mp.exe 0x48e510, jk2sp.exe
 * 0x47e340). Types: vertical 1, oriented 2, effect 3; Academy single player
 * alone adds flattened, which it numbers 5. Width, height and density must
 * be above 0 and the fade distance at least 32, each read with atof into a
 * float; any failure warns and stores nothing. A good line sets the record's
 * defaults, the fade maximum at 1.33 times the fade distance in double
 * precision, and leaves its vertical skew as it was.
 */
static void R_JediParseSurfaceSprites( const char *line, jediSurfaceSprite_t *ss ) {
	const char *text = line;
	const char *token;
	const char *missing = "surfaceSprites params";
	float width, height, density, fadeDist;
	int type;

	token = R_JediSpriteWord( &text, missing );
	if ( !token ) {
		return;
	}
	if ( !Q_stricmp( token, "vertical" ) ) {
		type = JEDI_SPRITE_VERTICAL;
	} else if ( !Q_stricmp( token, "oriented" ) ) {
		type = JEDI_SPRITE_ORIENTED;
	} else if ( !Q_stricmp( token, "effect" ) ) {
		type = JEDI_SPRITE_EFFECT;
	} else if ( ri.gameId == COM_GAME_JKA_SP && !Q_stricmp( token, "flattened" ) ) {
		type = JEDI_SPRITE_FLATTENED;
	} else {
		ri.Printf( PRINT_WARNING, "WARNING: invalid type in shader '%s'\n", shader.name );
		return;
	}

	if ( !( token = R_JediSpriteWord( &text, missing ) ) ) return;
	width = (float)atof( token );
	if ( width <= 0.0f ) {
		ri.Printf( PRINT_WARNING, "WARNING: invalid width in shader '%s'\n", shader.name );
		return;
	}
	if ( !( token = R_JediSpriteWord( &text, missing ) ) ) return;
	height = (float)atof( token );
	if ( height <= 0.0f ) {
		ri.Printf( PRINT_WARNING, "WARNING: invalid height in shader '%s'\n", shader.name );
		return;
	}
	if ( !( token = R_JediSpriteWord( &text, missing ) ) ) return;
	density = (float)atof( token );
	if ( density <= 0.0f ) {
		ri.Printf( PRINT_WARNING, "WARNING: invalid density in shader '%s'\n", shader.name );
		return;
	}
	if ( !( token = R_JediSpriteWord( &text, missing ) ) ) return;
	fadeDist = (float)atof( token );
	if ( fadeDist < 32.0f ) {
		ri.Printf( PRINT_WARNING, "WARNING: invalid fadedist (%f < 32) in shader '%s'\n", fadeDist, shader.name );
		return;
	}

	ss->type = type;
	ss->width = width;
	ss->height = height;
	ss->density = density;
	ss->fadeDist = fadeDist;
	ss->fadeMax = (float)( (double)fadeDist * 1.33 );
	ss->fadeScale = 0.0f;
	ss->wind = 0.0f;
	ss->windIdle = 0.0f;
	ss->variance[0] = ss->variance[1] = 0.0f;
	ss->facing = JEDI_SPRITE_FACING_NORMAL;
	ss->fxDuration = 1000.0f;
	ss->fxGrow[0] = ss->fxGrow[1] = 0.0f;
	ss->fxAlphaStart = 1.0f;
	ss->fxAlphaEnd = 0.0f;
}

/* One number of an ss word, read with atof into a float; qfalse after a
 * warning when it is missing. */
static qboolean R_JediSpriteNumber( const char **text, const char *what, float *value ) {
	const char *token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing surfacesprite %s in shader '%s'\n", what, shader.name );
		return qfalse;
	}
	*value = (float)atof( token );
	return qtrue;
}

static void R_JediSpriteInvalid( const char *what ) {
	ri.Printf( PRINT_WARNING, "WARNING: invalid surfacesprite %s in shader '%s'\n", what, shader.name );
}

/* A facing word takes effect only on a record still facing normally. */
static void R_JediSpriteFacing( jediSurfaceSprite_t *ss, int facing, const char *name ) {
	if ( ss->facing != JEDI_SPRITE_FACING_NORMAL ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s facing overrides previous facing in shader '%s'\n", name, shader.name );
		return;
	}
	ss->facing = facing;
}

/*
 * The ss words (jamp.exe 0x4b2f80, jasp.exe 0x4790d0, jk2mp.exe 0x48e7e0,
 * jk2sp.exe 0x47e610), each matched without regard to case and each able to
 * come before `surfaceSprites` on a zeroed record. A two-number word stores
 * its first number before reading its second.
 */
static void R_JediParseSpriteOption( const char *param, const char *line, jediSurfaceSprite_t *ss ) {
	const char *text = line;
	float value;

	if ( !Q_stricmp( param, "ssFademax" ) ) {
		if ( !R_JediSpriteNumber( &text, "fademax", &value ) ) return;
		if ( value <= ss->fadeDist ) {
			ri.Printf( PRINT_WARNING, "WARNING: invalid surfacesprite fademax (%.2f <= fadeDist(%.2f)) in shader '%s'\n",
				value, ss->fadeDist, shader.name );
			return;
		}
		ss->fadeMax = value;
	} else if ( !Q_stricmp( param, "ssFadescale" ) ) {
		if ( !R_JediSpriteNumber( &text, "fadescale", &value ) ) return;
		ss->fadeScale = value;
	} else if ( !Q_stricmp( param, "ssVariance" ) ) {
		if ( !R_JediSpriteNumber( &text, "variance width", &value ) ) return;
		if ( value < 0.0f ) { R_JediSpriteInvalid( "variance width" ); return; }
		ss->variance[0] = value;
		if ( !R_JediSpriteNumber( &text, "variance height", &value ) ) return;
		if ( value < 0.0f ) { R_JediSpriteInvalid( "variance height" ); return; }
		ss->variance[1] = value;
	} else if ( !Q_stricmp( param, "ssHangdown" ) ) {
		R_JediSpriteFacing( ss, JEDI_SPRITE_FACING_DOWN, "Hangdown" );
	} else if ( !Q_stricmp( param, "ssAnyangle" ) ) {
		R_JediSpriteFacing( ss, JEDI_SPRITE_FACING_ANY, "Anyangle" );
	} else if ( !Q_stricmp( param, "ssFaceup" ) ) {
		R_JediSpriteFacing( ss, JEDI_SPRITE_FACING_UP, "Faceup" );
	} else if ( !Q_stricmp( param, "ssWind" ) ) {
		if ( !R_JediSpriteNumber( &text, "wind", &value ) ) return;
		if ( value < 0.0f ) { R_JediSpriteInvalid( "wind" ); return; }
		ss->wind = value;
		// the idle sway follows the wind unless it was given first
		if ( ss->windIdle <= 0.0f ) {
			ss->windIdle = value;
		}
	} else if ( !Q_stricmp( param, "ssWindidle" ) ) {
		if ( !R_JediSpriteNumber( &text, "windidle", &value ) ) return;
		if ( value < 0.0f ) { R_JediSpriteInvalid( "windidle" ); return; }
		ss->windIdle = value;
	} else if ( !Q_stricmp( param, "ssVertskew" ) ) {
		if ( !R_JediSpriteNumber( &text, "vertskew", &value ) ) return;
		if ( value < 0.0f ) { R_JediSpriteInvalid( "vertskew" ); return; }
		ss->vertSkew = value;
	} else if ( !Q_stricmp( param, "ssFXDuration" ) ) {
		if ( !R_JediSpriteNumber( &text, "duration", &value ) ) return;
		if ( value <= 0.0f ) { R_JediSpriteInvalid( "duration" ); return; }
		ss->fxDuration = value;
	} else if ( !Q_stricmp( param, "ssFXGrow" ) ) {
		if ( !R_JediSpriteNumber( &text, "grow width", &value ) ) return;
		if ( value < 0.0f ) { R_JediSpriteInvalid( "grow width" ); return; }
		ss->fxGrow[0] = value;
		if ( !R_JediSpriteNumber( &text, "grow height", &value ) ) return;
		if ( value < 0.0f ) { R_JediSpriteInvalid( "grow height" ); return; }
		ss->fxGrow[1] = value;
	} else if ( !Q_stricmp( param, "ssFXAlphaRange" ) ) {
		if ( !R_JediSpriteNumber( &text, "fxalpha start", &value ) ) return;
		if ( value < 0.0f || value > 1.0f ) { R_JediSpriteInvalid( "fxalpha start" ); return; }
		ss->fxAlphaStart = value;
		if ( !R_JediSpriteNumber( &text, "fxalpha end", &value ) ) return;
		if ( value < 0.0f || value > 1.0f ) { R_JediSpriteInvalid( "fxalpha end" ); return; }
		ss->fxAlphaEnd = value;
	} else if ( !Q_stricmp( param, "ssFXWeather" ) ) {
		if ( ss->type != JEDI_SPRITE_EFFECT ) {
			ri.Printf( PRINT_WARNING, "WARNING: weather applied to non-effect surfacesprite in shader '%s'\n", shader.name );
			return;
		}
		ss->type = JEDI_SPRITE_WEATHERFX;
	} else {
		ri.Printf( PRINT_WARNING, "WARNING: invalid optional surfacesprite param '%s' in shader '%s'\n", param, shader.name );
	}
}

/* ParseStage's two branches; qtrue when the token was a sprite word. */
static qboolean R_JediParseSpriteStageWord( const char *token, const char **text, shaderStage_t *stage ) {
	char line[JEDI_SPRITE_LINE_SIZE];
	char param[JEDI_SPRITE_PARAM_SIZE];

	if ( R_JediSurfaceSpritesKeyword( token ) ) {
		R_JediSpriteLine( text, line, sizeof( line ) );
		R_JediParseSurfaceSprites( line, &stage->jediSprite );
		return qtrue;
	}
	if ( R_JediSpriteOptionKeyword( token ) ) {
		Q_strncpyz( param, token, sizeof( param ) );
		R_JediSpriteLine( text, line, sizeof( line ) );
		R_JediParseSpriteOption( param, line, &stage->jediSprite );
		return qtrue;
	}
	return qfalse;
}

/*
 * FinishShader, once it has chosen the sort: retail's stage loops skip a
 * sprite stage (jamp.exe 0x4aef88, jk2mp.exe 0x488dad) and draw it after
 * the surface from the second stage on (jamp.exe 0x4af48f
 * walks stages 1 .. numUnfoggedPasses - 1, jk2mp.exe 0x48867a stages 1..7),
 * so a first-stage sprite never draws. The sprite stages leave the surface's
 * own passes here, the drawn ones kept in `taken`; the surface needs its
 * normals for them, as jk2mp.exe and jk2sp.exe mark the shader (0x48e7a8,
 * 0x47e5d8). Returns the stages left.
 */
static int R_JediTakeSpriteStages( int numStages, shaderStage_t *taken, int *numTaken ) {
	int i, kept;

	*numTaken = 0;
	if ( !R_JediShaderProfile() || shader.defaultShader ) {
		return numStages;
	}
	for ( i = 0, kept = 0; i < numStages; i++ ) {
		if ( !stages[i].active || stages[i].jediSprite.type == JEDI_SPRITE_NONE ) {
			if ( kept != i ) {
				stages[kept] = stages[i];
			}
			kept++;
			continue;
		}
		if ( i > 0 ) {
			taken[( *numTaken )++] = stages[i];
		}
	}
	for ( i = kept; i < numStages; i++ ) {
		Com_Memset( &stages[i], 0, sizeof( stages[i] ) );
		stages[i].bundle[0].texMods = texMods[i];
	}
	if ( kept != numStages ) {
		shader.needsNormal = qtrue;
	}
	return kept;
}

/*
 * After the surface's own shader is permanent: each sprite stage becomes a
 * shader of its own, two-sided, its texture stretched once over each sprite
 * and its colour and alpha taken from the sprite's vertices, as retail's
 * sprite batches draw with the stage's image and state bits alone
 * (OpenJK's CQuickSpriteSystem). Fn's FinishShader then gives it the sort,
 * fog adjustment and pipelines of any blended stage.
 */
static void R_JediFinishSpriteShaders( shader_t *parent, const shaderStage_t *taken, int numTaken ) {
	jediSpritePass_t *passes;
	char name[MAX_QPATH];
	int i;

	if ( !numTaken || parent == tr.defaultShader ) {
		return;
	}
	passes = ri.Hunk_Alloc( numTaken * sizeof( *passes ), h_low );
	for ( i = 0; i < numTaken; i++ ) {
		passes[i].parms = taken[i].jediSprite;
		Com_sprintf( name, sizeof( name ), "%s#sprite%d", parent->name, i + 1 );
		InitShader( name, LIGHTMAP_NONE );
		stages[0] = taken[i];
		Com_Memset( &stages[0].jediSprite, 0, sizeof( stages[0].jediSprite ) );
		stages[0].bundle[0].texMods = texMods[0];
		stages[0].bundle[0].numTexMods = 0;
		stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
#ifdef USE_VULKAN
		stages[0].bundle[0].rgbGen = CGEN_VERTEX;
		stages[0].bundle[0].alphaGen = AGEN_VERTEX;
#else
		stages[0].rgbGen = CGEN_VERTEX;
		stages[0].alphaGen = AGEN_VERTEX;
#endif
		stages[0].isDetail = qfalse;
		shader.cullType = CT_TWO_SIDED;
		passes[i].shader = FinishShader();
	}
	parent->jediSprites = passes;
	parent->numJediSprites = numTaken;
}

#endif
