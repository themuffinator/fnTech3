/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_ALICE_SHADER_H
#define FNTECH3_TR_ALICE_SHADER_H

/* American McGee's Alice's shader-script dialect.
 *
 * Ritual's language is Quake III's with extra keywords, a different argument
 * syntax for vectors (bare floats, no parentheses), a line-based `if`/`endif`
 * preprocessor and a renumbered sort scale. Every fact below is pinned to the
 * retail alice.exe (image base 0x00400000); "[I]" marks an inference. The
 * investigation behind it is the shader report recorded with the Alice
 * renderer work; no Ritual SDK or OpenAlice text is used.
 *
 * Included by a renderer's tr_shader.c after its `shader` and `stages`
 * working copies, R_FindShaderImage and the Quake III IHV headers. Every
 * entry point returns "not handled" unless the Alice profile is selected, so
 * no other profile's parsing changes.
 *
 * Retail bugs deliberately not reproduced (none is used by the shipped
 * scripts, and none is part of a module or file contract):
 *  - `alphaGen oneMinusEntity` inverts twice and equals entity alpha
 *    (0x0049e58b); fnTech3 keeps Quake III's one-minus.
 *  - `alphaGen oneMinusVertex` has no back-end case and leaves the alpha the
 *    colour pass produced (0x00493cc4); fnTech3 keeps Quake III's.
 *  - a waveform's `random` phase is written into the amplitude slot
 *    (0x0048d618); fnTech3 draws the random phase the keyword names.
 *  - `rgbGen entity` multiplies the packed colour, as one integer, by
 *    identityLight (0x0049e260); fnTech3 copies the colour, which is what
 *    retail produces at its default identityLight of one.
 *  - GL_State reapplies `alphatest` only when the reference byte changes
 *    (0x00485dd7); fnTech3 reapplies it whenever the function or reference
 *    does.
 *  - `tcGen vector` always writes bundle 0 (0x0048c4c2), even after
 *    `nextBundle`; fnTech3 writes the bundle `nextBundle` selected, as every
 *    other texture keyword does. No shipped Alice or Heavy Metal: F.A.K.K.2
 *    script puts `tcGen vector` after `nextBundle`.
 * Out-of-range constant colours are clamped to a byte rather than wrapped as
 * retail's ftol-to-byte store does (0x0048bd5b); no shipped value exceeds 1.
 * One retail bug is reproduced, because shipped scripts look as it leaves
 * them: the colours the multitexture fold writes (R_RitualFoldStageColours). */

/* Retail marks a `fromEntity` waveform or texture-modifier parameter with this
 * value (0x0048d542, 0x0048d882). */
#ifndef ALICE_SHADER_FROM_ENTITY
#define ALICE_SHADER_FROM_ENTITY 1234567.0f
#endif
/* Retail's `portalsky` sort 2.0, between a portal and the sky (0x0048e41e). */
#ifndef ALICE_SORT_PORTALSKY
#define ALICE_SORT_PORTALSKY 1.5f
#endif

/* Where a stage keeps its colour and alpha sources, constant colour and
 * colour wave. The GL renderer keeps them on the stage; the Vulkan-family
 * renderers keep them on each texture bundle and define this, before the
 * include, to name bundle 0 -- the only bundle a parsed stage writes, and
 * the one an Alice source stays on, since R_AliceStageCollapses keeps such a
 * stage out of every multitexture collapse. */
#ifndef R_ALICE_GEN
#define R_ALICE_GEN( stage ) ( stage )
#endif

static void ParseWaveForm( const char **text, waveForm_t *wave );
static qboolean ParseVector( const char **text, int count, float *v );

/* Medal of Honor's scripts are written in the same dialect: 2015's renderer
 * is F.A.K.K.2's descendant, with the same `nextBundle` second bundle, the
 * same sort scale with a portal sky at 2 and the same sprite keywords
 * (OpenMoHAA renderergl1/tr_shader.c and tr_local.h, reference only; its
 * own executable is SafeDisc-wrapped). Its extra keywords are its own. */
static qboolean R_AliceShaderDialect( void ) {
	return COM_GAME_ID_IS_RITUAL( ri.gameId ) || ri.gameId == COM_GAME_MOHAA ? qtrue : qfalse;
}

/* Retail reads a vector as N bare tokens (0x0048d1d0): `rgbGen const 1 1 1`,
 * `fogParms .5 .25 .1 1024`, `tcGen vector 1 0 0 0 1 0`. A missing element
 * is an error. fnTech3 also takes Quake III's `( x y z )` form, which retail
 * would misread, so ported scripts keep working. */
static qboolean R_AliceParseFloats( const char **text, int count, float *v ) {
	const char *cursor = *text;
	const char *token = COM_ParseExt( &cursor, qfalse );
	int i;
	if ( !strcmp( token, "(" ) ) {
		return ParseVector( text, count, v );
	}
	for ( i = 0; i < count; i++ ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name );
			return qfalse;
		}
		v[i] = Q_atof( token );
	}
	return qtrue;
}

/* A colour component as retail stores it: value * 255, truncated (0x0048bd5b),
 * clamped here instead of wrapping. */
static byte R_AliceColorByte( float value ) {
	const double scaled = (double)value * 255.0;
	if ( !( scaled > 0.0 ) ) return 0;
	if ( scaled >= 255.0 ) return 255;
	return (byte)scaled;
}

/* Alice's sort scale is Quake III's plus one from 2 up; 2 is its own
 * `portalsky` slot (ParseSort 0x0048ac40, FinishShader 0x0048e41e). Named
 * sorts already land on fnTech3's enumeration; a numeric one is translated. */
static float R_AliceSortValue( float value ) {
	if ( value >= 3.0f ) return value - 1.0f;
	if ( value >= 2.0f ) return ALICE_SORT_PORTALSKY + ( value - 2.0f ) * ( SS_ENVIRONMENT - ALICE_SORT_PORTALSKY );
	return value;
}

/* A waveform parameter: `fromEntity` marks it for the entity's surface bytes
 * (0x0048d542); `random` as a phase is a random phase (see the list above). */
static float R_ShaderWaveParm( const char *token, qboolean phase ) {
	if ( R_AliceShaderDialect() ) {
		if ( !Q_stricmp( token, "fromEntity" ) ) return ALICE_SHADER_FROM_ENTITY;
		if ( phase && !Q_stricmp( token, "random" ) ) return random();
	}
	return Q_atof( token );
}

/* ---- the `if` preprocessor (0x0048cdc5..0x0048cf65) ---------------------- */

/* Multitexture is what `mtex` asks for: retail tests its
 * glActiveTextureARB pointer (0x0048cdf6). fnTech3's GL paths always have
 * two units; the renderer's own unit count answers it. */
#ifndef R_ALICE_MULTITEXTURE
#define R_ALICE_MULTITEXTURE() ( glConfig.numTextureUnits >= 2 ? qtrue : qfalse )
#endif

static qboolean R_AliceShaderIfArgument( const char **text ) {
	const char *token = COM_ParseExt( text, qfalse );
	if ( !Q_stricmp( token, "mtex" ) ) {
		return R_ALICE_MULTITEXTURE();
	}
	if ( !Q_stricmp( token, "no_mtex" ) ) {
		return R_ALICE_MULTITEXTURE() ? qfalse : qtrue;
	}
	if ( !Q_stricmp( token, "shaderlod" ) ) {
		/* r_shaderlod->value > f (0x0048ce4c): the default 0.5 skips
		 * `if shaderlod 0.5` and passes `if shaderlod 0.45`. */
		token = COM_ParseExt( text, qfalse );
		return r_shaderlod && r_shaderlod->value > Q_atof( token ) ? qtrue : qfalse;
	}
	if ( !strcmp( token, "0" ) ) {
		return qfalse;
	}
	if ( !strcmp( token, "1" ) ) {
		return qtrue;
	}
	ri.Printf( PRINT_WARNING, "WARNING: invalid if argument '%s' in shader '%s', not passing\n", token, shader.name );
	return qfalse;
}

/* A false `if` skips line by line up to its matching `endif`, counting
 * nested `if`s; braces on the skipped lines are just more tokens. */
static void R_AliceShaderSkipIf( const char **text ) {
	int depth = 1;
	SkipRestOfLine( text );
	for ( ;; ) {
		const char *token = COM_ParseExt( text, qtrue );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: no matching endif in shader '%s'\n", shader.name );
			return;
		}
		if ( !Q_stricmp( token, "if" ) ) {
			depth++;
			SkipRestOfLine( text );
		} else if ( !Q_stricmp( token, "endif" ) ) {
			if ( --depth == 0 ) {
				return;
			}
		} else {
			SkipRestOfLine( text );
		}
	}
}

/* ---- general section (ParseShader 0x0048b450) ----------------------------- */

/* Returns 1 when the keyword was Alice's and has been consumed, 0 when it is
 * not (nothing consumed, the token untouched), -1 when the shader fails.
 * `ifDepth` counts the `if`s passed; `extended` is fnTech3's own extended
 * shader text, which keeps its `if ( ... )` syntax. */
static int R_AliceShaderDirective( const char *keyword, const char **text, int *ifDepth, qboolean extended ) {
	const char *token;
	if ( !R_AliceShaderDialect() ) {
		return 0;
	}
	if ( !extended && !Q_stricmp( keyword, "if" ) ) {
		if ( R_AliceShaderIfArgument( text ) ) {
			( *ifDepth )++;
		} else {
			R_AliceShaderSkipIf( text );
		}
		return 1;
	}
	if ( !extended && !Q_stricmp( keyword, "endif" ) ) {
		if ( --( *ifDepth ) < 0 ) {
			ri.Printf( PRINT_WARNING, "WARNING: endif without if in shader '%s'\n", shader.name );
		}
		return 1;
	}
	/* Compiler-only; retail skips the rest of the line (0x0048c771). */
	if ( !Q_stricmp( keyword, "surfaceLight" ) || !Q_stricmp( keyword, "surfaceColor" ) ||
		!Q_stricmp( keyword, "surfaceAngle" ) || !Q_stricmp( keyword, "surfaceDensity" ) ) {
		SkipRestOfLine( text );
		return 1;
	}
	/* A window onto the sky portal (0x0048caf9); FinishShader sorts it. */
	if ( !Q_stricmp( keyword, "portalsky" ) ) {
		shader.alicePortalSky = qtrue;
		shader.sort = ALICE_SORT_PORTALSKY;
		return 1;
	}
	/* The volume draws the fog pass alone (0x0048cd5b, FinishShader
	 * 0x0048e45f): a stageless fog shader, as Quake III's own FinishShader
	 * already treats one. */
	if ( !Q_stricmp( keyword, "fogonly" ) ) {
		shader.aliceFogOnly = qtrue;
		shader.contentFlags |= CONTENTS_FOG;
		return 1;
	}
	/* Three bare floats, a distance, and anything else on the line ignored
	 * (0x0048ca0f..0x0048ca91). A missing colour element fails the shader,
	 * a missing distance only warns. */
	if ( !Q_stricmp( keyword, "fogParms" ) ) {
		if ( !R_AliceParseFloats( text, 3, shader.fogParms.color ) ) {
			return -1;
		}
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name );
		} else {
			shader.fogParms.depthForOpaque = Q_atof( token );
		}
		SkipRestOfLine( text );
		return 1;
	}
	/* Parsed and kept (0x0048ca96); the per-volume density animation it
	 * drives (0x0049ecb5) is not traced far enough to draw. */
	if ( !Q_stricmp( keyword, "foggen" ) ) {
		ParseWaveForm( text, &shader.aliceFogGen );
		return 1;
	}
	/* Kept (0x0048c970); no back-end reader was found. */
	if ( !Q_stricmp( keyword, "subdivisions" ) ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing subdivisions parm in shader '%s'\n", shader.name );
		} else {
			shader.aliceSubdivisions = Q_atof( token );
		}
		return 1;
	}
	/* Asks for 32-bit uploads (0x0048c9ca); fnTech3 uploads by r_texturebits
	 * alone. */
	if ( !Q_stricmp( keyword, "force32bit" ) ) {
		return 1;
	}
	/* Alice's `nomipmaps` turns mipmaps off and nothing else; unlike Quake
	 * III it leaves picmip to `nopicmip` (0x0048c97e). */
	if ( !Q_stricmp( keyword, "nomipmaps" ) ) {
		shader.noMipMaps = 1;
		return 1;
	}
	/* Medal of Honor keeps surfaces with this shader out of its surface
	 * merging, which this engine does not do. */
	if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( keyword, "noMerge" ) ) {
		return 1;
	}
	return 0;
}

/* ---- stages (0x0048b4ec..0x0048c6e2) -------------------------------------- */

typedef struct {
	int depthTestBits;		/* GLS_DEPTHTEST_DISABLE after `noDepthTest` */
	int bundle;				/* the texture bundle the image and texture-coordinate
							 * keywords fill: 0, or 1 after `nextBundle` */
	unsigned noMipMaps : 1;	/* the shader-level flags, restored at the stage's end */
	unsigned noPicMip : 1;
} aliceStageParse_t;

/* A second bundle's texture modifiers, one set per stage; bundle 0's are the
 * renderer's own. GeneratePermanentShader copies both. */
static texModInfo_t aliceBundleTexMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS + 1];

static void R_AliceBeginStage( aliceStageParse_t *parse ) {
	Com_Memset( parse, 0, sizeof( *parse ) );
	parse->noMipMaps = shader.noMipMaps;
	parse->noPicMip = shader.noPicMip;
}

/* Stage-level `nomipmaps`/`nopicmip` last to the stage's end (0x0048b4d8,
 * 0x0048b561); image loads inside the stage read the shader's flags.
 *
 * A stage that took `nextBundle` draws both bundles at once, the second
 * through the next texture unit with GL_MODULATE: the stage's multitexture
 * bits (+0x280) hold 1 (0x0048c3a0), which the draw turns into GL_MODULATE,
 * 2 being GL_ADD (0x00492d8f..0x00492dbe). The stage's colour, alpha and
 * blend are its own, as Quake III's collapsed stages' are. */
static void R_AliceEndStage( shaderStage_t *stage, const aliceStageParse_t *parse ) {
	if ( !R_AliceShaderDialect() ) {
		return;
	}
	shader.noMipMaps = parse->noMipMaps;
	shader.noPicMip = parse->noPicMip;
	if ( !parse->bundle ) {
		return;
	}
	if ( !stage->bundle[1].image[0] ) {
		/* [I] retail would bind no image here; no shipped Alice script does
		 * it, while Medal of Honor's debris sprites do, so for it this is
		 * only a developer's note */
		ri.Printf( ri.gameId == COM_GAME_MOHAA ? PRINT_DEVELOPER : PRINT_WARNING,
			"WARNING: nextBundle without an image in shader '%s'\n", shader.name );
		Com_Memset( &stage->bundle[1], 0, sizeof( stage->bundle[1] ) );
		return;
	}
	stage->mtEnv = GL_MODULATE;
	stage->tessFlags |= TESS_ST1;
	shader.multitextureEnv = qtrue;
}

/* A stage that names no `rgbGen` and ends on its first bundle, that bundle
 * being `$lightmap`, takes identity (fakk2.exe 1.02 0x00476e8c..0x00476ebc,
 * alice.exe 0x0048c648..0x0048c678), where Quake III's rule gives an
 * unblended stage identity lighting. Ritual numbers identity 1 and identity
 * lighting 2 (0x00476454, 0x00476475), and collapses two stages only when
 * their colours match (FinishShader 0x00478fa8), so a lightmap and the
 * texture filtering it draw as one multitexture pass. With no overbright
 * bits, both engines' default, the two colours are the same. Every lightmap
 * stage Alice ships is identity already, 1,369 of 1,387 by name and the rest
 * by Quake III's rule for their blends, so the rule changes none of them. */
static qboolean R_RitualLightmapStageIdentity( const shaderStage_t *stage, const aliceStageParse_t *parse ) {
	return COM_GAME_ID_IS_RITUAL( ri.gameId ) && !parse->bundle && stage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE
		? qtrue : qfalse;
}

/* Whether a stage's second bundle is one `nextBundle` filled rather than one
 * a multitexture collapse moved there: only the former keeps its modifiers
 * in aliceBundleTexMods, and a stage keeps them when stages move. */
static qboolean R_AliceStageHasNextBundle( const shaderStage_t *stage ) {
	int i;
	if ( !stage->bundle[1].image[0] ) {
		return qfalse;
	}
	for ( i = 0; i < MAX_SHADER_STAGES; i++ ) {
		if ( stage->bundle[1].texMods == aliceBundleTexMods[i] ) {
			return qtrue;
		}
	}
	return qfalse;
}

/* How many bundles a parsed stage fills before any collapse: two after
 * `nextBundle`. */
static int R_AliceStageBundles( const shaderStage_t *stage ) {
	return R_AliceShaderDialect() && R_AliceStageHasNextBundle( stage ) ? 2 : 1;
}

static imgFlags_t R_AliceStageImageFlags( void ) {
	imgFlags_t flags = IMGFLAG_NONE;
	if ( !shader.noMipMaps ) flags |= IMGFLAG_MIPMAP;
	if ( !shader.noPicMip ) flags |= IMGFLAG_PICMIP;
	if ( shader.noLightScale ) flags |= IMGFLAG_NOLIGHTSCALE;
	return flags;
}

/* `alphatest <func> <ref>` (0x0048b7d0..0x0048b86c): func 1..6, ref
 * atof * 255 truncated, both into the stage's state bits. */
static unsigned R_AliceAlphaTestBits( const char **text ) {
	static const char *names[] = { "equal", "notequal", "greater", "less", "greaterequal", "lessequal" };
	unsigned func = 0, ref = 0;
	const char *token = COM_ParseExt( text, qfalse );
	int i;
	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing alphatest function in shader '%s'\n", shader.name );
	} else {
		for ( i = 0; i < (int)ARRAY_LEN( names ); i++ ) {
			if ( !Q_stricmp( token, names[i] ) ) {
				func = i + 1;
				break;
			}
		}
		if ( !func ) {
			ri.Printf( PRINT_WARNING, "WARNING: invalid alphatest function '%s' in shader '%s'\n", token, shader.name );
		}
	}
	token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing alphatest reference in shader '%s'\n", shader.name );
	} else {
		const double scaled = (double)Q_atof( token ) * 255.0;
		ref = scaled <= 0.0 ? 0 : scaled >= 255.0 ? 255 : (unsigned)scaled;
	}
	return func ? ( func << GLS_ALICE_ATEST_FUNC_SHIFT ) | ( ref << GLS_ALICE_ATEST_REF_SHIFT ) : 0;
}

/* The optional numbers after `alphaGen dot`/`oneMinusDot`: min 0, max 1
 * (0x0048c042..0x0048c095). */
static void R_AliceParseDotRange( const char **text, shaderStage_t *stage ) {
	const char *token;
	stage->aliceDotRange[0] = 0.0f;
	stage->aliceDotRange[1] = 1.0f;
	token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) return;
	stage->aliceDotRange[0] = Q_atof( token );
	token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) return;
	stage->aliceDotRange[1] = Q_atof( token );
}

/* Medal of Honor's `sCoord`/`tCoord` numbers: min 0 and max 1, then byte
 * caps low 0 and high 255 given as fractions of one. */
static void R_MOHAAParseCoordRange( const char **text, shaderStage_t *stage ) {
	const char *token;
	stage->mohaaCoordRange[0] = 0.0f;
	stage->mohaaCoordRange[1] = 1.0f;
	stage->mohaaCoordCaps[0] = 0;
	stage->mohaaCoordCaps[1] = 255;
	token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) return;
	stage->mohaaCoordRange[0] = Q_atof( token );
	token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) return;
	stage->mohaaCoordRange[1] = Q_atof( token );
	token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) return;
	stage->mohaaCoordCaps[0] = (int)( Q_atof( token ) * 255.0f );
	token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing sCoord or tCoord parm 'max' in shader '%s'\n", shader.name );
		return;
	}
	stage->mohaaCoordCaps[1] = (int)( Q_atof( token ) * 255.0f );
}

/* Returns 1 handled, 0 not Alice's (text untouched), -1 fail. `keyword` must
 * not be COM's token buffer: rgbGen/alphaGen/tcGen look ahead at their value
 * and decline Quake III's. */
static int R_AliceStageDirective( shaderStage_t *stage, const char *keyword, const char **text,
	aliceStageParse_t *parse, int *depthMaskBits, qboolean *depthMaskExplicit, int *atestBits ) {
	const char *cursor = *text;
	const char *token;

	if ( !R_AliceShaderDialect() ) {
		return 0;
	}
	/* 0x0048c36b: the depth test off; the stage's end drops depth writes. */
	if ( !Q_stricmp( keyword, "noDepthTest" ) ) {
		parse->depthTestBits = GLS_DEPTHTEST_DISABLE;
		return 1;
	}
	/* `depthMask` is `depthWrite` (0x0048c33d/0x0048c353). */
	if ( !Q_stricmp( keyword, "depthMask" ) ) {
		*depthMaskBits = GLS_DEPTHMASK_TRUE;
		*depthMaskExplicit = qtrue;
		return 1;
	}
	/* Shares the variable alphaFunc writes: the later keyword wins. */
	if ( !Q_stricmp( keyword, "alphatest" ) ) {
		*atestBits = (int)R_AliceAlphaTestBits( text );
		return 1;
	}
	/* The bundle being filled's flag (0x0048c3d5). */
	if ( !Q_stricmp( keyword, "frameFromEntity" ) ) {
		stage->bundle[parse->bundle].aliceFrameFromEntity = 1;
		return 1;
	}
	/* 0x0048c378: the keywords after it fill the stage's second bundle. A
	 * card without multitexture, or a third bundle, fails the shader
	 * (0x0048d051, 0x0048d073). */
	if ( !Q_stricmp( keyword, "nextBundle" ) ) {
		const int index = (int)( stage - stages );
		if ( !R_ALICE_MULTITEXTURE() ) {
			ri.Printf( PRINT_WARNING, "WARNING: nextBundle on a card without multitexturing in shader '%s'\n", shader.name );
			return -1;
		}
		if ( parse->bundle + 1 >= 2 || index < 0 || index >= MAX_SHADER_STAGES ) {
			ri.Printf( PRINT_WARNING, "WARNING: too many nextBundle commands in shader '%s'\n", shader.name );
			return -1;
		}
		parse->bundle = 1;
		stage->bundle[1].texMods = aliceBundleTexMods[index];
		stage->bundle[1].numTexMods = 0;
		return 1;
	}
	if ( !Q_stricmp( keyword, "nomipmaps" ) ) {
		shader.noMipMaps = 1;
		return 1;
	}
	if ( !Q_stricmp( keyword, "nopicmip" ) ) {
		shader.noPicMip = 1;
		return 1;
	}
	if ( !Q_stricmp( keyword, "force32bit" ) ) {
		return 1;
	}
	/* `animMapOnce` is animMap stopping on its last frame (0x0048c556). It
	 * keeps MAX_IMAGE_ANIMATIONS frames, retail's 64 (0x0048c5bc); the shipped
	 * scripts use at most 8. */
	if ( !Q_stricmp( keyword, "animMapOnce" ) ) {
		textureBundle_t *bundle = &stage->bundle[parse->bundle];
		int total = 0;
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'animMapOnce' keyword in shader '%s'\n", shader.name );
			return -1;
		}
		bundle->imageAnimationSpeed = Q_atof( token );
		bundle->aliceAnimMapOnce = 1;
		for ( ;; ) {
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) break;
			if ( bundle->numImageAnimations < MAX_IMAGE_ANIMATIONS ) {
				bundle->image[bundle->numImageAnimations] = R_FindShaderImage( token, R_AliceStageImageFlags() );
				if ( !bundle->image[bundle->numImageAnimations] ) {
					ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
					return -1;
				}
				bundle->numImageAnimations++;
			}
			total++;
		}
		if ( total > MAX_IMAGE_ANIMATIONS ) {
			ri.Printf( PRINT_WARNING, "WARNING: ignoring excess images for 'animMapOnce' (found %d, max is %d) in shader '%s'\n",
				total, MAX_IMAGE_ANIMATIONS, shader.name );
		}
		return 1;
	}
	/* Medal of Honor: `animMap` whose frames start `phase` seconds in. */
	if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( keyword, "animMapPhase" ) ) {
		textureBundle_t *bundle = &stage->bundle[parse->bundle];
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'animMapPhase' keyword in shader '%s'\n", shader.name );
			return -1;
		}
		bundle->imageAnimationSpeed = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing phase for 'animMapPhase' keyword in shader '%s'\n", shader.name );
			return -1;
		}
		bundle->mohaaAnimPhase = Q_atof( token );
		for ( ;; ) {
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) break;
			if ( bundle->numImageAnimations < MAX_IMAGE_ANIMATIONS ) {
				bundle->image[bundle->numImageAnimations] = R_FindShaderImage( token, R_AliceStageImageFlags() );
				if ( !bundle->image[bundle->numImageAnimations] ) {
					ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
					return -1;
				}
				bundle->numImageAnimations++;
			}
		}
		return 1;
	}
	/* Medal of Honor's `clampmapx`/`clampmapy` clamp one axis and repeat
	 * the other. The engine's images clamp both or neither; the image repeats
	 * here, which only its clamped edge can tell apart. */
	if ( ri.gameId == COM_GAME_MOHAA && ( !Q_stricmp( keyword, "clampmapx" ) || !Q_stricmp( keyword, "clampmapy" ) ) ) {
		textureBundle_t *bundle = &stage->bundle[parse->bundle];
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing parameter for '%s' keyword in shader '%s'\n", keyword, shader.name );
			return -1;
		}
		bundle->image[0] = R_FindShaderImage( token, R_AliceStageImageFlags() );
		if ( !bundle->image[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
			return -1;
		}
		return 1;
	}
	/* Medal of Honor's `depthFunc always` passes every fragment. */
	if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( keyword, "depthFunc" ) ) {
		token = COM_ParseExt( &cursor, qfalse );
		if ( Q_stricmp( token, "always" ) ) {
			return 0;
		}
		*text = cursor;
		parse->depthTestBits = GLS_DEPTHTEST_DISABLE;
		return 1;
	}
	if ( !Q_stricmp( keyword, "rgbGen" ) ) {
		token = COM_ParseExt( &cursor, qfalse );
		/* 0x0048bacd..0x0048bde0 */
		if ( !Q_stricmp( token, "fromEntity" ) ) {
			R_ALICE_GEN( stage )->rgbGen = CGEN_ENTITY;
		} else if ( !Q_stricmp( token, "fromClient" ) ) {
			R_ALICE_GEN( stage )->rgbGen = CGEN_VERTEX;
			if ( R_ALICE_GEN( stage )->alphaGen == AGEN_IDENTITY ) {
				R_ALICE_GEN( stage )->alphaGen = AGEN_VERTEX;
			}
		} else if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( token, "static" ) ) {
			/* Medal of Honor's static models are lit by the colours their map
			 * gives each vertex, which the world builder puts on the vertices
			 * (tr_mohaa_world.c). */
			R_ALICE_GEN( stage )->rgbGen = CGEN_VERTEX;
		} else if ( ri.gameId == COM_GAME_MOHAA && ( !Q_stricmp( token, "lightingSpherical" )
			|| !Q_stricmp( token, "lightingGrid" ) ) ) {
			/* Its models' lighting from the map's lights and light grid is the
			 * engine's own entity lighting here, and a static model's the
			 * colours the map lit it with (AGENTS.md invariant 11;
			 * tr_mohaa_shade.h). */
			R_ALICE_GEN( stage )->rgbGen = CGEN_MOHAA_LIGHTING;
		} else if ( ri.gameId == COM_GAME_MOHAA && ( !Q_stricmp( token, "dot" ) || !Q_stricmp( token, "oneMinusDot" ) ) ) {
			R_ALICE_GEN( stage )->rgbGen = !Q_stricmp( token, "dot" ) ? CGEN_MOHAA_DOT : CGEN_MOHAA_ONE_MINUS_DOT;
			*text = cursor;
			R_AliceParseDotRange( text, stage );
			return 1;
		} else if ( ri.gameId == COM_GAME_MOHAA && ( !Q_stricmp( token, "sCoord" ) || !Q_stricmp( token, "tCoord" ) ) ) {
			R_ALICE_GEN( stage )->rgbGen = !Q_stricmp( token, "sCoord" ) ? CGEN_MOHAA_SCOORD : CGEN_MOHAA_TCOORD;
			*text = cursor;
			R_MOHAAParseCoordRange( text, stage );
			return 1;
		} else if ( !Q_stricmp( token, "global" ) || !Q_stricmp( token, "globalColor" ) ) {
			R_ALICE_GEN( stage )->rgbGen = CGEN_ALICE_GLOBAL;
		} else if ( !Q_stricmp( token, "const" ) || !Q_stricmp( token, "constant" ) ) {
			vec3_t color = { 0.0f, 0.0f, 0.0f };
			*text = cursor;
			R_AliceParseFloats( text, 3, color );
			R_ALICE_GEN( stage )->constantColor.rgba[0] = R_AliceColorByte( color[0] );
			R_ALICE_GEN( stage )->constantColor.rgba[1] = R_AliceColorByte( color[1] );
			R_ALICE_GEN( stage )->constantColor.rgba[2] = R_AliceColorByte( color[2] );
			R_ALICE_GEN( stage )->rgbGen = CGEN_CONST;
			return 1;
		} else if ( !Q_stricmp( token, "colorwave" ) ) {
			vec3_t color = { 0.0f, 0.0f, 0.0f };
			*text = cursor;
			R_AliceParseFloats( text, 3, color );
			R_ALICE_GEN( stage )->constantColor.rgba[0] = R_AliceColorByte( color[0] );
			R_ALICE_GEN( stage )->constantColor.rgba[1] = R_AliceColorByte( color[1] );
			R_ALICE_GEN( stage )->constantColor.rgba[2] = R_AliceColorByte( color[2] );
			ParseWaveForm( text, &R_ALICE_GEN( stage )->rgbWave );
			R_ALICE_GEN( stage )->rgbGen = CGEN_ALICE_COLORWAVE;
			return 1;
		} else {
			return 0;
		}
		*text = cursor;
		return 1;
	}
	if ( !Q_stricmp( keyword, "alphaGen" ) ) {
		token = COM_ParseExt( &cursor, qfalse );
		/* 0x0048be52..0x0048c23e */
		if ( !Q_stricmp( token, "fromEntity" ) ) {
			R_ALICE_GEN( stage )->alphaGen = AGEN_ENTITY;
		} else if ( !Q_stricmp( token, "fromClient" ) ) {
			R_ALICE_GEN( stage )->alphaGen = AGEN_VERTEX;
		} else if ( ri.gameId == COM_GAME_MOHAA && ( !Q_stricmp( token, "distFade" ) || !Q_stricmp( token, "oneMinusDistFade" ) ) ) {
			/* The distances are the shader's: the last stage to name them sets
			 * them for all. */
			R_ALICE_GEN( stage )->alphaGen = !Q_stricmp( token, "distFade" ) ? AGEN_MOHAA_DIST_FADE : AGEN_MOHAA_ONE_MINUS_DIST_FADE;
			*text = cursor;
			shader.mohaaDistNear = 256.0f;
			shader.mohaaDistRange = 256.0f;
			token = COM_ParseExt( text, qfalse );
			if ( token[0] ) {
				shader.mohaaDistNear = Q_atof( token );
				token = COM_ParseExt( text, qfalse );
				if ( token[0] ) {
					shader.mohaaDistRange = Q_atof( token );
				}
			}
			return 1;
		} else if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( token, "oneMinusVertex" ) ) {
			/* No alpha of its own in Ritual's back end (0x00493cc4), unlike
			 * Quake III's one-minus kept for Alice above; Medal of Honor's HUD
			 * art is drawn in opaque white through it. */
			R_ALICE_GEN( stage )->alphaGen = AGEN_MOHAA_COLOR_PASS;
		} else if ( ri.gameId == COM_GAME_MOHAA && ( !Q_stricmp( token, "sCoord" ) || !Q_stricmp( token, "tCoord" ) ) ) {
			R_ALICE_GEN( stage )->alphaGen = !Q_stricmp( token, "sCoord" ) ? AGEN_MOHAA_SCOORD : AGEN_MOHAA_TCOORD;
			*text = cursor;
			R_MOHAAParseCoordRange( text, stage );
			return 1;
		} else if ( !Q_stricmp( token, "global" ) || !Q_stricmp( token, "globalAlpha" ) ) {
			R_ALICE_GEN( stage )->alphaGen = AGEN_ALICE_GLOBAL;
		} else if ( !Q_stricmp( token, "skyAlpha" ) ) {
			R_ALICE_GEN( stage )->alphaGen = AGEN_ALICE_SKY_ALPHA;
		} else if ( !Q_stricmp( token, "oneMinusSkyAlpha" ) ) {
			R_ALICE_GEN( stage )->alphaGen = AGEN_ALICE_ONE_MINUS_SKY_ALPHA;
		} else if ( !Q_stricmp( token, "dot" ) || !Q_stricmp( token, "oneMinusDot" ) ) {
			R_ALICE_GEN( stage )->alphaGen = !Q_stricmp( token, "dot" ) ? AGEN_ALICE_DOT : AGEN_ALICE_ONE_MINUS_DOT;
			*text = cursor;
			R_AliceParseDotRange( text, stage );
			return 1;
		} else if ( !Q_stricmp( token, "const" ) || !Q_stricmp( token, "constant" ) ) {
			/* a * 255 accepted only within a byte, else ignored (0x0048c1c4). */
			double alpha;
			*text = cursor;
			token = COM_ParseExt( text, qfalse );
			alpha = (double)Q_atof( token ) * 255.0;
			if ( alpha < 0.0 || alpha >= 256.0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: alphaGen const value '%s' out of range in shader '%s'\n", token, shader.name );
			} else {
				R_ALICE_GEN( stage )->constantColor.rgba[3] = (byte)alpha;
				R_ALICE_GEN( stage )->alphaGen = AGEN_CONST;
			}
			return 1;
		} else if ( !Q_stricmp( token, "lightingSpecular" ) ) {
			/* An optional scale (times 255) and light origin; without them
			 * Quake III's 255 and (-960, 1980, 96) (0x0048bf3c). */
			R_ALICE_GEN( stage )->alphaGen = AGEN_LIGHTING_SPECULAR;
			*text = cursor;
			cursor = *text;
			token = COM_ParseExt( &cursor, qfalse );
			if ( token[0] ) {
				stage->aliceSpecular[0] = Q_atof( token ) * 255.0f;
				stage->aliceSpecular[1] = -960.0f;
				stage->aliceSpecular[2] = 1980.0f;
				stage->aliceSpecular[3] = 96.0f;
				stage->aliceSpecularCustom = qtrue;
				*text = cursor;
				R_AliceParseFloats( text, 3, &stage->aliceSpecular[1] );
			}
			return 1;
		} else {
			return 0;
		}
		*text = cursor;
		return 1;
	}
	/* Bare-float vectors (0x0048c4c2..0x0048c4eb). */
	if ( !Q_stricmp( keyword, "tcGen" ) || !Q_stricmp( keyword, "texgen" ) ) {
		token = COM_ParseExt( &cursor, qfalse );
		if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( token, "environmentModel" ) ) {
			stage->bundle[parse->bundle].tcGen = TCGEN_MOHAA_ENVIRONMENT_MODEL;
			*text = cursor;
			return 1;
		}
		if ( Q_stricmp( token, "vector" ) ) {
			return 0;
		}
		*text = cursor;
		R_AliceParseFloats( text, 3, stage->bundle[parse->bundle].tcGenVectors[0] );
		R_AliceParseFloats( text, 3, stage->bundle[parse->bundle].tcGenVectors[1] );
		stage->bundle[parse->bundle].tcGen = TCGEN_VECTOR;
		return 1;
	}
	return 0;
}

/* After the stage's keywords: `noDepthTest` also turns depth writes off
 * (0x0048c6b5), after GL_ONE GL_ZERO turned them on. */
static unsigned R_AliceStageStateBits( const aliceStageParse_t *parse, int *depthMaskBits ) {
	if ( !R_AliceShaderDialect() ) {
		return 0;
	}
	if ( parse->depthTestBits ) {
		*depthMaskBits = 0;
	}
	return (unsigned)parse->depthTestBits;
}

/* ---- texture modifiers (ParseTexMod 0x0048d680) ------------------------- */

static qboolean R_AliceTexModFloat( const char **text, float *value, const char *name ) {
	const char *token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing tcMod %s parms in shader '%s'\n", name, shader.name );
		return qfalse;
	}
	*value = !Q_stricmp( token, "fromEntity" ) ? ALICE_SHADER_FROM_ENTITY : Q_atof( token );
	return qtrue;
}

/* Returns qtrue when `type` was Alice's to parse; a missing parameter leaves
 * the slot without a type, which stops the stage's later modifiers as it
 * does in Quake III and retail (0x00493e63). */
static qboolean R_AliceParseTexMod( const char *type, const char **text, texModInfo_t *tmi ) {
	float values[2];
	if ( !R_AliceShaderDialect() ) {
		return qfalse;
	}
	/* Either speed may be `fromEntity` (0x0048d882, 0x0048d8d7). */
	if ( !Q_stricmp( type, "scroll" ) ) {
		if ( !R_AliceTexModFloat( text, &values[0], "scroll" ) || !R_AliceTexModFloat( text, &values[1], "scroll" ) ) {
			return qtrue;
		}
		tmi->scroll[0] = values[0];
		tmi->scroll[1] = values[1];
		tmi->type = values[0] == ALICE_SHADER_FROM_ENTITY || values[1] == ALICE_SHADER_FROM_ENTITY ?
			TMOD_ALICE_SCROLL : TMOD_SCROLL;
		return qtrue;
	}
	if ( !Q_stricmp( type, "rotate" ) ) {
		if ( !R_AliceTexModFloat( text, &values[0], "rotate" ) ) {
			return qtrue;
		}
		tmi->rotateSpeed = values[0];
		tmi->type = values[0] == ALICE_SHADER_FROM_ENTITY ? TMOD_ALICE_ROTATE : TMOD_ROTATE;
		return qtrue;
	}
	/* A constant shift, either part of which may be `fromEntity` (type 9,
	 * 0x0048db03). */
	if ( !Q_stricmp( type, "offset" ) ) {
		if ( !R_AliceTexModFloat( text, &values[0], "offset" ) || !R_AliceTexModFloat( text, &values[1], "offset" ) ) {
			return qtrue;
		}
		tmi->scale[0] = tmi->scale[1] = 1.0f;
		tmi->offset[0] = values[0];
		tmi->offset[1] = values[1];
		tmi->type = values[0] == ALICE_SHADER_FROM_ENTITY || values[1] == ALICE_SHADER_FROM_ENTITY ?
			TMOD_ALICE_OFFSET : TMOD_OFFSET;
		return qtrue;
	}
	/* Planar projection by the vertex normal's base axis, `1 / X` and
	 * `1 / Y` texels per unit of the first image (type 11, 0x0048dcbe). */
	if ( !Q_stricmp( type, "macro" ) ) {
		if ( !R_AliceTexModFloat( text, &values[0], "macro" ) || !R_AliceTexModFloat( text, &values[1], "macro" ) ) {
			return qtrue;
		}
		if ( values[0] == 0.0f || values[1] == 0.0f ) {
			/* retail divides by zero here; an infinite scale draws nothing */
			ri.Printf( PRINT_WARNING, "WARNING: zero tcMod macro size in shader '%s'\n", shader.name );
			return qtrue;
		}
		tmi->scale[0] = 1.0f / values[0];
		tmi->scale[1] = 1.0f / values[1];
		tmi->type = TMOD_ALICE_MACRO;
		return qtrue;
	}
	/* Medal of Honor: s or t shifted by a waveform. */
	if ( ri.gameId == COM_GAME_MOHAA && ( !Q_stricmp( type, "wavetrans" ) || !Q_stricmp( type, "wavetrant" ) ) ) {
		/* decided before the waveform's tokens reuse `type`'s buffer */
		const texMod_t mod = !Q_stricmp( type, "wavetrans" ) ? TMOD_MOHAA_WAVETRANS : TMOD_MOHAA_WAVETRANT;
		ParseWaveForm( text, &tmi->wave );
		tmi->type = mod;
		return qtrue;
	}
	/* Medal of Honor: width, amplitude, frequency and phase, which change
	 * nothing (code/renderercommon/tr_mohaa_shade.h). */
	if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( type, "bulge" ) ) {
		if ( !R_AliceTexModFloat( text, &tmi->wave.base, "bulge" ) || !R_AliceTexModFloat( text, &tmi->wave.amplitude, "bulge" )
			|| !R_AliceTexModFloat( text, &tmi->wave.frequency, "bulge" ) || !R_AliceTexModFloat( text, &tmi->wave.phase, "bulge" ) ) {
			return qtrue;
		}
		tmi->type = TMOD_MOHAA_BULGE;
		return qtrue;
	}
	/* Shifted by the viewer's x and y times two rates (type 10, 0x0048dbed). */
	if ( !Q_stricmp( type, "parallax" ) ) {
		if ( !R_AliceTexModFloat( text, &values[0], "parallax" ) || !R_AliceTexModFloat( text, &values[1], "parallax" ) ) {
			return qtrue;
		}
		tmi->scale[0] = values[0];
		tmi->scale[1] = values[1];
		tmi->type = TMOD_ALICE_PARALLAX;
		return qtrue;
	}
	return qfalse;
}

/* ---- deforms (ParseDeform 0x0048dd40) ------------------------------------ */

/* `deformVertexes wavenormal <div> <x> <y> <z> <waveform>` (type 16,
 * 0x0048dfee..0x0048e0c4). An unknown subtype's arguments are skipped rather
 * than read as the next general keyword. Returns qtrue when handled. */
static qboolean R_AliceParseDeform( const char *subtype, const char **text, deformStage_t *ds ) {
	const char *token;
	float div;
	int i;
	if ( !R_AliceShaderDialect() ) {
		return qfalse;
	}
	/* Medal of Honor's `lightglow`: autosprite, pulled toward the viewer. */
	if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( subtype, "lightglow" ) ) {
		ds->deformation = DEFORM_MOHAA_LIGHTGLOW;
		return qtrue;
	}
	/* Medal of Honor's `flap <s|t> <div> <waveform> [min max]`. */
	if ( ri.gameId == COM_GAME_MOHAA && !Q_stricmp( subtype, "flap" ) ) {
		qboolean useT;
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 's' || token[0] == 'S' ) {
			useT = qfalse;
		} else if ( token[0] == 't' || token[0] == 'T' ) {
			useT = qtrue;
		} else {
			ri.Printf( PRINT_WARNING, "WARNING: deformVertexes flap requires 's' or 't' in shader '%s'\n", shader.name );
			return qtrue;
		}
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes flap parm in shader '%s'\n", shader.name );
			return qtrue;
		}
		div = Q_atof( token );
		if ( div != 0.0f ) {
			ds->deformationSpread = 1.0f / div;
		} else {
			ds->deformationSpread = 100.0f;
			ri.Printf( PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
		}
		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = useT ? DEFORM_MOHAA_FLAP_T : DEFORM_MOHAA_FLAP_S;
		ds->bulgeWidth = 0.0f;
		ds->bulgeHeight = 1.0f;
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			return qtrue;
		}
		ds->bulgeWidth = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm 'max' in shader '%s'\n", shader.name );
			return qtrue;
		}
		ds->bulgeHeight = Q_atof( token );
		return qtrue;
	}
	if ( Q_stricmp( subtype, "wavenormal" ) ) {
		return qfalse;
	}
	token = COM_ParseExt( text, qfalse );
	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
		return qtrue;
	}
	div = Q_atof( token );
	if ( div != 0.0f ) {
		ds->deformationSpread = 1.0f / div;
	} else {
		ds->deformationSpread = 100.0f;
		ri.Printf( PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
	}
	for ( i = 0; i < 3; i++ ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return qtrue;
		}
		ds->moveVector[i] = Q_atof( token );
	}
	ParseWaveForm( text, &ds->deformationWave );
	ds->deformation = DEFORM_ALICE_WAVENORMAL;
	return qtrue;
}

/* ---- FinishShader --------------------------------------------------------- */

static qboolean R_AliceStageIsCPUOnly( const shaderStage_t *stage ) {
	int b, m;
	if ( stage->stateBits & GLS_ALICE_ATEST_BITS ) return qtrue;
	if ( stage->isDetail ) return qtrue;	/* skipped per draw by r_detailtextures */
	switch ( R_ALICE_GEN( stage )->rgbGen ) {
	case CGEN_ALICE_GLOBAL:
	case CGEN_ALICE_COLORWAVE:
	case CGEN_MOHAA_DOT:
	case CGEN_MOHAA_ONE_MINUS_DOT:
	case CGEN_MOHAA_SCOORD:
	case CGEN_MOHAA_TCOORD:
	case CGEN_MOHAA_LIGHTING:
		return qtrue;
	default:
		break;
	}
	switch ( R_ALICE_GEN( stage )->alphaGen ) {
	case AGEN_ALICE_DOT:
	case AGEN_ALICE_ONE_MINUS_DOT:
	case AGEN_ALICE_GLOBAL:
	case AGEN_ALICE_SKY_ALPHA:
	case AGEN_ALICE_ONE_MINUS_SKY_ALPHA:
	case AGEN_MOHAA_DIST_FADE:
	case AGEN_MOHAA_ONE_MINUS_DIST_FADE:
	case AGEN_MOHAA_SCOORD:
	case AGEN_MOHAA_TCOORD:
	case AGEN_MOHAA_COLOR_PASS:
		return qtrue;
	case AGEN_LIGHTING_SPECULAR:
		if ( stage->aliceSpecularCustom ) return qtrue;
		break;
	default:
		break;
	}
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		if ( stage->bundle[b].aliceAnimMapOnce || stage->bundle[b].aliceFrameFromEntity ) return qtrue;
		if ( stage->bundle[b].tcGen == TCGEN_MOHAA_ENVIRONMENT_MODEL || stage->bundle[b].mohaaAnimPhase != 0.0f ) return qtrue;
		for ( m = 0; m < stage->bundle[b].numTexMods; m++ ) {
			if ( stage->bundle[b].texMods[m].type >= TMOD_ALICE_SCROLL ) return qtrue;
		}
	}
	return qfalse;
}

/* Before FinishShader's stage loop: the sort a `portalsky` or `fogonly`
 * shader takes (0x0048e41e, 0x0048e476) -- a sky shader still sorts as sky --
 * and a fog-only volume's own stages dropped (0x0048e482). */
static void R_AliceFinishShaderSort( void ) {
	int i;
	if ( !R_AliceShaderDialect() ) {
		return;
	}
	if ( shader.alicePortalSky ) {
		shader.sort = ALICE_SORT_PORTALSKY;
	}
	if ( shader.aliceFogOnly ) {
		if ( stages[0].active ) {
			ri.Printf( PRINT_WARNING, "WARNING: fogonly shader '%s' has stages\n", shader.name );
		}
		for ( i = 0; i < MAX_SHADER_STAGES; i++ ) {
			stages[i].active = qfalse;
		}
		shader.sort = SS_FOG;
	}
}

/* After the stages are final: which ones need the CPU path. */
static void R_AliceFinishShaderStages( int numStages ) {
	int i;
	if ( !R_AliceShaderDialect() ) {
		return;
	}
	for ( i = 0; i < numStages; i++ ) {
		stages[i].aliceDynamic = R_AliceStageIsCPUOnly( &stages[i] );
		if ( stages[i].aliceDynamic ) {
			shader.aliceNoStatic = qtrue;
		}
	}
}

/* A multitexture collapse would lose a detail stage's per-draw skip or an
 * Alice colour or alpha source's parameters, and would overwrite the second
 * bundle a `nextBundle` stage already fills. */
static qboolean R_AliceStageCollapses( const shaderStage_t *stage ) {
	if ( !R_AliceShaderDialect() ) {
		return qtrue;
	}
	if ( R_AliceStageHasNextBundle( stage ) ) return qfalse;
	if ( stage->isDetail ) return qfalse;
	if ( R_ALICE_GEN( stage )->rgbGen >= CGEN_ALICE_GLOBAL ) return qfalse;
	if ( R_ALICE_GEN( stage )->alphaGen >= AGEN_ALICE_DOT ) return qfalse;
	if ( R_ALICE_GEN( stage )->alphaGen == AGEN_LIGHTING_SPECULAR && stage->aliceSpecularCustom ) return qfalse;
	return qtrue;
}

/* The constant colours Ritual's multitexture fold leaves (alice.exe
 * 0x0048e798..0x0048ea83; fakk2.exe 1.02 0x00478ead..0x004791c0, the same
 * code). FinishShader walks neighbouring stages and folds a pair into one
 * pass when neither has a texture environment yet (`nextBundle` gives one),
 * their state but for blending and depth writes is the same, the first is
 * opaque or blends as the second does, the second modulates (DST_COLOR ZERO,
 * ZERO SRC_COLOR) or adds (ONE ONE), and their colour and alpha sources --
 * and the waves of either -- are the same. It combines the two constant
 * colours, a product for a modulate and for an add the sum, which its
 * inverted clamp raises to at least 1, and stores the four bytes 0x2ed
 * apart where its stages lie 0x2ec apart: channel k goes to the constant
 * colour of the k-th stage on from the folded one, as the fold leaves them.
 * So the stage after a folded pair takes the pair's blue and the one after
 * that its alpha -- 0 after a modulate of two stages with no constant, 255
 * after such an add. The shipped scripts' look depends on it: the canal
 * water `textures/under/tunnelwater` (`rgbGen constant 0.5 0.8 0.5` after a
 * lightmap and a modulated texture) is lime green in retail, its blue gone.
 * fnTech3 folds by rules of its own, so the colours are put where retail's
 * fold leaves them before any of that, and a folded pair's second stage
 * gives its constant up, since retail's pass draws with the first's alone
 * whether or not this renderer folds it. Medal of Honor is not included:
 * its executable cannot be read, and OpenMoHAA's fold combines no colour. */
static void R_RitualFoldStageColours( void ) {
	shaderStage_t *list[MAX_SHADER_STAGES];
	qboolean folded[MAX_SHADER_STAGES];
	int count = 0, i, k;
	if ( !COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {
		return;
	}
	for ( i = 0; i < MAX_SHADER_STAGES && stages[i].active && stages[i].bundle[0].image[0]; i++ ) {
		list[count] = &stages[i];
		folded[count] = R_AliceStageHasNextBundle( &stages[i] );
		count++;
	}
	for ( i = 0; i + 1 < count; i++ ) {
		const shaderStage_t *a = list[i], *b = list[i + 1];
		const unsigned blendBits = GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS;
		unsigned aBlend = a->stateBits & blendBits, bBlend = b->stateBits & blendBits;
		float combined[4];
		qboolean add;
		if ( folded[i] ) {
			continue;
		}
		if ( aBlend == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO ) ) {
			aBlend = 0;
		}
		if ( ( a->stateBits ^ b->stateBits ) & ~( blendBits | GLS_DEPTHMASK_TRUE ) ) {
			continue;
		}
		if ( aBlend && aBlend != bBlend ) {
			continue;
		}
		if ( bBlend == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO ) || bBlend == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR ) ) {
			add = qfalse;
		} else if ( bBlend == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) ) {
			add = qtrue;
		} else {
			continue;
		}
		if ( R_ALICE_GEN( a )->rgbGen != R_ALICE_GEN( b )->rgbGen || R_ALICE_GEN( a )->alphaGen != R_ALICE_GEN( b )->alphaGen ) {
			continue;
		}
		if ( ( R_ALICE_GEN( a )->rgbGen == CGEN_WAVEFORM || R_ALICE_GEN( a )->rgbGen == CGEN_ALICE_COLORWAVE )
			&& memcmp( &R_ALICE_GEN( a )->rgbWave, &R_ALICE_GEN( b )->rgbWave, sizeof( waveForm_t ) ) ) {
			continue;
		}
		if ( R_ALICE_GEN( a )->alphaGen == AGEN_WAVEFORM
			&& memcmp( &R_ALICE_GEN( a )->alphaWave, &R_ALICE_GEN( b )->alphaWave, sizeof( waveForm_t ) ) ) {
			continue;
		}
		for ( k = 0; k < 4; k++ ) {
			/* bytes over 255 as floats, then the product or the sum stored as
			 * a float (0x0047903c..0x004790f4) */
			const float scale = 1.0f / 255.0f;
			const float x = (float)( (double)R_ALICE_GEN( a )->constantColor.rgba[k] * scale );
			const float y = (float)( (double)R_ALICE_GEN( b )->constantColor.rgba[k] * scale );
			if ( add ) {
				const float sum = (float)( (double)x + (double)y );
				combined[k] = sum < 1.0f ? 1.0f : sum;
			} else {
				combined[k] = (float)( (double)x * (double)y );
			}
		}
		for ( k = 0; k < 4 && i + k < count; k++ ) {
			/* truncated, its low byte kept (0x00479105..0x0047911e) */
			R_ALICE_GEN( list[i + k] )->constantColor.rgba[k] = (byte)( (unsigned)(int)( (double)combined[k] * 255.0 ) & 0xff );
		}
		if ( R_ALICE_GEN( b )->rgbGen == CGEN_CONST || R_ALICE_GEN( b )->alphaGen == AGEN_CONST ) {
			for ( k = 0; k < 4; k++ ) {
				R_ALICE_GEN( list[i + 1] )->constantColor.rgba[k] = 255;
			}
		}
		folded[i] = qtrue;
		for ( k = i + 1; k + 1 < count; k++ ) {
			list[k] = list[k + 1];
			folded[k] = folded[k + 1];
		}
		count--;
	}
}

/* A detail stage stays in an Alice shader: retail skips it while drawing,
 * so r_detailtextures takes effect at once (0x00492a60). */
static qboolean R_AliceKeepsDetailStage( void ) {
	return R_AliceShaderDialect();
}

/* Alice's own needs: the dot alphas and the macro projection read normals. */
static qboolean R_AliceStageNeedsNormal( const shaderStage_t *stage ) {
	int b, m;
	if ( R_ALICE_GEN( stage )->alphaGen == AGEN_ALICE_DOT || R_ALICE_GEN( stage )->alphaGen == AGEN_ALICE_ONE_MINUS_DOT ) return qtrue;
	if ( R_ALICE_GEN( stage )->rgbGen == CGEN_MOHAA_DOT || R_ALICE_GEN( stage )->rgbGen == CGEN_MOHAA_ONE_MINUS_DOT
		|| R_ALICE_GEN( stage )->rgbGen == CGEN_MOHAA_LIGHTING ) return qtrue;
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		if ( stage->bundle[b].tcGen == TCGEN_MOHAA_ENVIRONMENT_MODEL ) return qtrue;
		for ( m = 0; m < stage->bundle[b].numTexMods; m++ ) {
			if ( stage->bundle[b].texMods[m].type == TMOD_ALICE_MACRO ) return qtrue;
		}
	}
	return qfalse;
}

#endif
