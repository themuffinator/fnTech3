/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MATERIAL_WORDS_H
#define FNTECH3_TR_MATERIAL_WORDS_H

/*
 * The physically based material words a texture pack's shader script may put
 * in a stage, as OpenJK's rend2 renderer reads them (a reference observation
 * of codemp/rd-rend2/tr_shader.cpp ParseStage at OpenJK 1a6a643; ioquake3's
 * rend2 introduced most of them):
 *
 *   normalMap <image>, normalHeightMap <image>   tangent-space normal map
 *                                                (height in the alpha of the
 *                                                second)
 *   specMap <image>, specularMap <image>         specular colour, gloss in alpha
 *   rmoMap, rmosMap, moxrMap, mosrMap,
 *   ormMap, ormsMap <image>                      packed roughness (R),
 *                                                metalness (M), occlusion (O),
 *                                                specular (S) and unused (X)
 *                                                channels in the named order
 *   specularReflectance <f>                      specular colour, grey
 *   specularExponent <f>                         gloss from a Phong exponent
 *   gloss <f>, roughness <f>
 *   parallaxDepth <f>, parallaxBias <f>
 *   normalScale <xy> | <x> <y> | <x> <y> <depth>
 *   specularScale <rgb> <gloss> | <r> <g> <b> [<gloss>]
 *                 or, with a packed map, <metalness> <specular> <occlusion> <gloss>
 *
 * Quake III's own stage words never use these names, and no retail shader
 * script carries them (a retail engine fails the shader on them). The raster
 * renderers accept them and draw the stage as the rest of it says; only the
 * path tracer gives them meaning (rendererrtx/tr_shader.c
 * R_RtFindMaterialMaps). An image named `$whiteimage` stands for a white map,
 * so a packed or specular map of constants comes from the scales alone.
 */

#define R_PBR_MAP_NONE		0
#define R_PBR_MAP_SPECGLOSS	1	// specular colour, gloss in alpha
#define R_PBR_MAP_RMO		2	// roughness, metalness, occlusion
#define R_PBR_MAP_ORM		3	// occlusion, roughness, metalness
#define R_PBR_MAP_RMOS		4	// roughness, metalness, occlusion, specular
#define R_PBR_MAP_MOXR		5	// metalness, occlusion, unused, roughness
#define R_PBR_MAP_MOSR		6	// metalness, occlusion, specular, roughness
#define R_PBR_MAP_ORMS		7	// occlusion, roughness, metalness, specular

typedef struct {
	char		normalMap[MAX_QPATH];
	qboolean	normalHeight;		// normalHeightMap: height in the alpha
	char		materialMap[MAX_QPATH];
	int			materialType;		// R_PBR_MAP_*
	// rend2's specularScale: with a specular map the specular colour's scale
	// and, in w, one less the gloss's; with a packed map the scales of its
	// metalness, specular, occlusion and roughness, in that order
	float		specularScale[4];
	// rend2's normalScale: x, y, unused, parallax depth
	float		normalScale[4];
	float		parallaxBias;
	qboolean	words;				// the stage gave any of these words
	qboolean	scalars;			// ... a scale, gloss, roughness or reflectance
	qboolean	normalScaleSet;
	qboolean	parallaxDepthSet;
} rMaterialWords_t;

/* A stage's defaults before its words (rend2's, with r_baseSpecular 0.04). */
static void R_MaterialWordsInit( rMaterialWords_t *words )
{
	Com_Memset( words, 0, sizeof( *words ) );
	words->specularScale[0] = words->specularScale[1] = words->specularScale[2] = 0.04f;
	words->specularScale[3] = 0.99f;
	words->normalScale[0] = words->normalScale[1] = words->normalScale[2] = 1.0f;
	words->normalScale[3] = 0.05f;
}

/* The word's value, or warn and leave it as it was. */
static qboolean R_MaterialWordFloat( const char **text, const char *word, const char *shaderName, float *out )
{
	const char *token = COM_ParseExt( text, qfalse );

	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parameter for '%s' in shader '%s'\n", word, shaderName );
		return qfalse;
	}
	*out = Q_atof( token );
	return qtrue;
}

static qboolean R_MaterialWordImage( const char **text, const char *word, const char *shaderName, char *out )
{
	const char *token = COM_ParseExt( text, qfalse );

	if ( !token[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parameter for '%s' in shader '%s'\n", word, shaderName );
		return qfalse;
	}
	Q_strncpyz( out, token, MAX_QPATH );
	return qtrue;
}

/*
 * Reads one material word and its values into `words`. Returns qfalse for any
 * other word. A word whose value is missing warns and is otherwise ignored,
 * as rend2 reads on past a missing scale; it never fails the shader.
 */
static qboolean R_MaterialStageWord( const char *token, const char **text, rMaterialWords_t *words,
	const char *shaderName )
{
	static const struct {
		const char	*word;
		int			type;
	} packed[] = {
		{ "rmoMap", R_PBR_MAP_RMO }, { "rmosMap", R_PBR_MAP_RMOS },
		{ "moxrMap", R_PBR_MAP_MOXR }, { "mosrMap", R_PBR_MAP_MOSR },
		{ "ormMap", R_PBR_MAP_ORM }, { "ormsMap", R_PBR_MAP_ORMS },
	};
	char word[32];
	float value;
	int i;

	Q_strncpyz( word, token, sizeof( word ) );

	if ( !Q_stricmp( word, "normalMap" ) || !Q_stricmp( word, "normalHeightMap" ) ) {
		words->words = qtrue;
		if ( R_MaterialWordImage( text, word, shaderName, words->normalMap ) ) {
			words->normalHeight = !Q_stricmp( word, "normalHeightMap" ) ? qtrue : qfalse;
		}
		return qtrue;
	}
	if ( !Q_stricmp( word, "specMap" ) || !Q_stricmp( word, "specularMap" ) ) {
		words->words = qtrue;
		if ( R_MaterialWordImage( text, word, shaderName, words->materialMap ) ) {
			words->materialType = R_PBR_MAP_SPECGLOSS;
			// rend2: the map's colour and gloss as they are, until a later
			// word scales them
			words->specularScale[0] = words->specularScale[1] = words->specularScale[2] = 1.0f;
			words->specularScale[3] = 0.0f;
		}
		return qtrue;
	}
	for ( i = 0; i < (int)ARRAY_LEN( packed ); i++ ) {
		if ( !Q_stricmp( word, packed[i].word ) ) {
			words->words = qtrue;
			if ( R_MaterialWordImage( text, word, shaderName, words->materialMap ) ) {
				words->materialType = packed[i].type;
			}
			return qtrue;
		}
	}
	if ( !Q_stricmp( word, "specularReflectance" ) ) {
		words->words = qtrue;
		if ( R_MaterialWordFloat( text, word, shaderName, &value ) ) {
			words->specularScale[0] = words->specularScale[1] = words->specularScale[2] =
				Com_Clamp( 0.0f, 1.0f, value );
			words->scalars = qtrue;
		}
		return qtrue;
	}
	if ( !Q_stricmp( word, "specularExponent" ) ) {
		words->words = qtrue;
		if ( R_MaterialWordFloat( text, word, shaderName, &value ) ) {
			// rend2's gloss for a Phong exponent of 1 to 8192
			value = Com_Clamp( 1.0f, 8192.0f, value );
			words->specularScale[3] = 1.0f - logf( value ) / logf( 8192.0f );
			words->scalars = qtrue;
		}
		return qtrue;
	}
	if ( !Q_stricmp( word, "gloss" ) || !Q_stricmp( word, "roughness" ) ) {
		words->words = qtrue;
		if ( R_MaterialWordFloat( text, word, shaderName, &value ) ) {
			words->specularScale[3] = !Q_stricmp( word, "gloss" ) ? 1.0f - value : value;
			words->scalars = qtrue;
		}
		return qtrue;
	}
	if ( !Q_stricmp( word, "parallaxDepth" ) ) {
		words->words = qtrue;
		if ( R_MaterialWordFloat( text, word, shaderName, &words->normalScale[3] ) ) {
			words->parallaxDepthSet = qtrue;
		}
		return qtrue;
	}
	if ( !Q_stricmp( word, "parallaxBias" ) ) {
		words->words = qtrue;
		R_MaterialWordFloat( text, word, shaderName, &words->parallaxBias );
		return qtrue;
	}
	if ( !Q_stricmp( word, "normalScale" ) ) {
		words->words = qtrue;
		if ( !R_MaterialWordFloat( text, word, shaderName, &words->normalScale[0] ) ) {
			return qtrue;
		}
		words->normalScaleSet = qtrue;
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			// one value: x and y
			words->normalScale[1] = words->normalScale[0];
			return qtrue;
		}
		words->normalScale[1] = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] ) {
			words->normalScale[3] = Q_atof( token );
			words->parallaxDepthSet = qtrue;
		}
		return qtrue;
	}
	if ( !Q_stricmp( word, "specularScale" ) ) {
		float scale[4];

		words->words = qtrue;
		if ( !R_MaterialWordFloat( text, word, shaderName, &scale[0] ) ||
			!R_MaterialWordFloat( text, word, shaderName, &scale[1] ) ) {
			return qtrue;
		}
		words->scalars = qtrue;
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			// two values: grey colour, then gloss
			words->specularScale[0] = words->specularScale[1] = words->specularScale[2] = scale[0];
			words->specularScale[3] = 1.0f - scale[1];
			return qtrue;
		}
		words->specularScale[0] = scale[0];
		words->specularScale[1] = scale[1];
		words->specularScale[2] = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] ) {
			words->specularScale[3] = 1.0f - Q_atof( token );
		}
		return qtrue;
	}
	return qfalse;
}

#endif
