/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_SHADER_H
#define FNTECH3_TR_WOLFET_SHADER_H

/* Enemy Territory's shader dialect is the other Wolfenstein profile's
 * multiplayer dialect with three additions, kept here: implicit mapping, the
 * stages it builds for a shader made from a bare image, and shader names that
 * may be written with backslashes. The rules are the original ET renderer's;
 * this code is written anew. Included by a renderer's shader parser after its
 * parse globals. */

/* Implicit mapping: a shader names an image, or '-' for its own name, and the
 * blend and culling to draw it with; its stages are then made as they would
 * be for that bare image. ET resets this for every shader it looks up. */
static char s_wolfetImplicitMap[MAX_QPATH];
static unsigned s_wolfetImplicitStateBits;
static cullType_t s_wolfetImplicitCullType;

static void R_WolfETResetImplicit( void ) {
	s_wolfetImplicitMap[0] = '\0';
	s_wolfetImplicitStateBits = GLS_DEFAULT;
	s_wolfetImplicitCullType = CT_FRONT_SIDED;
}

/* Any keyword beginning "implicit" is accepted, as in ET; only Blend and Mask
 * differ from Map. Both draw two sided. */
static void R_WolfETImplicitDirective( const char *keyword, const char **text ) {
	const char *token;
	if ( !Q_stricmp( keyword, "implicitBlend" ) ) {
		s_wolfetImplicitStateBits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		s_wolfetImplicitCullType = CT_TWO_SIDED;
	} else if ( !Q_stricmp( keyword, "implicitMask" ) ) {
		s_wolfetImplicitStateBits = GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_80;
		s_wolfetImplicitCullType = CT_TWO_SIDED;
	} else {
		s_wolfetImplicitStateBits = GLS_DEPTHMASK_TRUE;
		s_wolfetImplicitCullType = CT_FRONT_SIDED;
	}
	token = COM_ParseExt( text, qfalse );
	Q_strncpyz( s_wolfetImplicitMap, token[0] ? token : "-", sizeof( s_wolfetImplicitMap ) );
}

/* The image an implicit shader is drawn with: its own name for '-'. */
static const char *R_WolfETImplicitImage( const char *shaderName ) {
	return s_wolfetImplicitMap[0] && s_wolfetImplicitMap[0] != '-' ? s_wolfetImplicitMap : shaderName;
}

/* ET reads a backslash in a shader's name as a slash: models name their
 * shaders with either. */
static void R_WolfETFixShaderName( char *name ) {
	for ( ; *name; ++name ) {
		if ( *name == '\\' ) *name = '/';
	}
}

/* The Vulkan renderers keep colour generation with each texture bundle, the
 * GL one with the stage. */
#if defined( RENDERER_VULKAN ) || defined( RENDERER_RTX )
#define R_WOLFET_RGBGEN( stage ) ( stage ).bundle[0].rgbGen
#define R_WOLFET_ALPHAGEN( stage ) ( stage ).bundle[0].alphaGen
#else
#define R_WOLFET_RGBGEN( stage ) ( stage ).rgbGen
#define R_WOLFET_ALPHAGEN( stage ) ( stage ).alphaGen
#endif

/* ET builds every shader made from a bare image this way, implicit or not:
 * the image lit as the lightmap index asks and drawn with the implicit blend.
 * A masked or blended image is drawn before its lightmap, which then darkens
 * only what the image left in the depth buffer. ET draws a fullbright request
 * with vertex light. */
static void R_CreateWolfETDefaultShading( image_t *image ) {
	shaderStage_t *texture, *lightmap;

	if ( s_wolfetImplicitCullType != CT_FRONT_SIDED && shader.cullType == CT_FRONT_SIDED ) {
		shader.cullType = s_wolfetImplicitCullType;
	}

	switch ( shader.lightmapIndex ) {
	case LIGHTMAP_NONE:
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
		R_WOLFET_RGBGEN( stages[0] ) = CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = s_wolfetImplicitStateBits;
		return;

	case LIGHTMAP_2D:
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
		R_WOLFET_RGBGEN( stages[0] ) = CGEN_VERTEX;
		R_WOLFET_ALPHAGEN( stages[0] ) = AGEN_SKIP;
		stages[0].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		return;

	case LIGHTMAP_WHITEIMAGE:
	case LIGHTMAP_BY_VERTEX:
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
		R_WOLFET_RGBGEN( stages[0] ) = CGEN_EXACT_VERTEX;
		R_WOLFET_ALPHAGEN( stages[0] ) = AGEN_SKIP;
		stages[0].stateBits = s_wolfetImplicitStateBits;
		return;

	default:
		break;
	}

	if ( s_wolfetImplicitStateBits & ( GLS_ATEST_BITS | GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) {
		texture = &stages[0];
		lightmap = &stages[1];
		texture->stateBits = s_wolfetImplicitStateBits;
		lightmap->stateBits = GLS_DEFAULT | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_EQUAL;
	} else {
		lightmap = &stages[0];
		texture = &stages[1];
		lightmap->stateBits = GLS_DEFAULT;
		texture->stateBits = GLS_DEFAULT | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}
	lightmap->bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
	lightmap->bundle[0].lightmap = LIGHTMAP_INDEX_SHADER;
	lightmap->bundle[0].tcGen = TCGEN_LIGHTMAP;
	lightmap->active = qtrue;
	R_WOLFET_RGBGEN( *lightmap ) = CGEN_IDENTITY;	// lightmaps are scaled on creation for identitylight
	texture->bundle[0].image[0] = image;
	texture->bundle[0].tcGen = TCGEN_TEXTURE;
	texture->active = qtrue;
	R_WOLFET_RGBGEN( *texture ) = CGEN_IDENTITY;
}

#endif
