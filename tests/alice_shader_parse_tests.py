"""American McGee's Alice shader scripts through the real shader parsers.

The fixture compiles a renderer's tr_shader.c itself -- ParseShader,
ParseStage, ParseDeform, ParseTexMod, FinishShader and the Alice dialect in
code/renderercommon/tr_alice_shader.h -- into a small host that stubs image
loading (every image name resolves) and every other renderer service. Symbols
the parser never reaches stay unresolved and are linked as null. The GL
parser (code/renderer) is the reference; the Vulkan and RTX parsers
(code/renderervk, code/rendererrtx), which keep a stage's colour sources on
its texture bundles, run the same checks and must store the same fields.

Two things run through it:

* an independently written mini-corpus of Ritual's dialect, checking the
  parsed fields keyword by keyword and that no other profile accepts the
  Alice-only syntax -- Medal of Honor's scripts are written in it too, and
  its own keywords are no other profile's; and
* when a legitimate retail installation is present, every shader in
  base/pak0.pk3's scripts/*.shader, read in memory and piped to the host (no
  retail text is written to disk), counting the definitions the parser
  rejects. Set FNTECH3_ALICE_ROOT to point at another installation. Heavy
  Metal: F.A.K.K.2's fakk/pak0.pk3 runs the same way under its own profile;
  set FNTECH3_FAKK2_ROOT to an installed copy.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
ALICE_ROOT = Path(os.environ.get('FNTECH3_ALICE_ROOT', r'E:\Games\Alice'))
# An installed Heavy Metal: F.A.K.K.2: the folder holding fakk/pak0.pk3.
FAKK2_ROOT = Path(os.environ.get('FNTECH3_FAKK2_ROOT', str(ROOT / '.tmp' / 'fakk2-install' / 'game')))

HOST = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/renderer/tr_shader.c"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

refimport_t ri;
trGlobals_t tr;
glconfig_t glConfig;
static cvar_t hostCvars[16];
cvar_t *r_ext_compressed_textures = &hostCvars[0];
cvar_t *r_neatsky = &hostCvars[1];
cvar_t *r_picmipFilter = &hostCvars[2];
cvar_t *r_vertexLight = &hostCvars[3];
cvar_t *r_printShaders = &hostCvars[4];
cvar_t *r_ext_multitexture = &hostCvars[5];
cvar_t *r_detailTextures = &hostCvars[6];
cvar_t *r_depthFade = &hostCvars[7];
HOST_EXTRA_CVARS

static image_t hostImages[4];
static int hostImageLookups;
image_t *R_FindImageFile( const char *name, imgFlags_t flags ) {
	(void)name; (void)flags;
	++hostImageLookups;
	return &hostImages[0];
}
image_t *R_CreateImage( const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags ) {
	(void)name; (void)name2; (void)pic; (void)width; (void)height; (void)flags;
	return &hostImages[3];
}
void R_InitSkyTexCoords( float height ) { (void)height; }
qboolean R_ImageAverageColor( const char *name, vec3_t color ) { (void)name; VectorClear( color ); return qfalse; }

static char hostWarning[256];
static void QDECL HostPrintf( printParm_t level, const char *format, ... ) {
	char line[1024];
	va_list args;
	va_start( args, format );
	vsnprintf( line, sizeof( line ), format, args );
	va_end( args );
	if ( level == PRINT_WARNING && !hostWarning[0] ) {
		size_t n = strlen( line );
		while ( n && ( line[n - 1] == '\n' || line[n - 1] == '\r' ) ) line[--n] = 0;
		Q_strncpyz( hostWarning, line, sizeof( hostWarning ) );
	}
}
static jmp_buf hostDrop;
static void NORETURN QDECL HostError( errorParm_t level, const char *format, ... ) {
	char line[1024];
	va_list args;
	(void)level;
	va_start( args, format );
	vsnprintf( line, sizeof( line ), format, args );
	va_end( args );
	if ( !hostWarning[0] ) Q_strncpyz( hostWarning, line, sizeof( hostWarning ) );
	longjmp( hostDrop, 1 );
}
static void HostCvarString( const char *name, char *buffer, int size ) {
	(void)name;
	if ( size > 0 ) buffer[0] = 0;
}
static int HostCinematic( const char *name, int x, int y, int w, int h, int bits ) {
	(void)name; (void)x; (void)y; (void)w; (void)h; (void)bits;
	return -1;
}

static void HostInit( comGameId_t game ) {
	memset( &ri, 0, sizeof( ri ) );
	ri.Printf = HostPrintf;
	ri.Error = HostError;
	ri.Cvar_VariableStringBuffer = HostCvarString;
	ri.CIN_PlayCinematic = HostCinematic;
	ri.gameId = game;
	memset( &tr, 0, sizeof( tr ) );
	tr.whiteImage = &hostImages[1];
	tr.defaultImage = &hostImages[2];
	tr.fogImage = &hostImages[2];
	glConfig.colorBits = 32;
	glConfig.numTextureUnits = 2;
	HOST_EXTRA_INIT
}

/* Parse one definition exactly as R_FindShader does after locating its text,
 * leaving the result in the parser's working `shader` and `stages`. */
static qboolean HostParse( const char *name, const char *body, const char *textEnd ) {
	const char *text = body;
	qboolean parsed;
	s_extensionOffset = textEnd + 1;
	hostWarning[0] = 0;
	InitShader( name, LIGHTMAP_NONE );
	if ( setjmp( hostDrop ) ) {
		return qfalse;
	}
	parsed = ParseShader( &text );
	return parsed;
}

/* The parsed fields any profile's parser writes, for comparing two revisions
 * of the parser on the same text. */
static void HostDigest( const char *name ) {
	int i, b, m;
	printf( "DIGEST %s sort=%g cull=%d po=%d sky=%d flags=%x/%x deforms=%d", name, shader.sort, (int)shader.cullType,
		(int)shader.polygonOffset, (int)shader.isSky, shader.surfaceFlags, shader.contentFlags, shader.numDeforms );
	for ( i = 0; i < shader.numDeforms; i++ ) {
		printf( " d%d:%g,%g,%g,%g,%g", (int)shader.deforms[i].deformation, shader.deforms[i].deformationSpread,
			shader.deforms[i].deformationWave.base, shader.deforms[i].deformationWave.amplitude,
			shader.deforms[i].deformationWave.phase, shader.deforms[i].deformationWave.frequency );
	}
	for ( i = 0; i < MAX_SHADER_STAGES && stages[i].active; i++ ) {
		const shaderStage_t *st = &stages[i];
		/* (the Vulkan family numbers its texture environments its own way) */
		printf( " [%x r%d a%d c%02x%02x%02x%02x det%d mt%s", st->stateBits, (int)R_ALICE_GEN( st )->rgbGen,
			(int)R_ALICE_GEN( st )->alphaGen, R_ALICE_GEN( st )->constantColor.rgba[0],
			R_ALICE_GEN( st )->constantColor.rgba[1], R_ALICE_GEN( st )->constantColor.rgba[2],
			R_ALICE_GEN( st )->constantColor.rgba[3], (int)st->isDetail,
			!st->mtEnv ? "0" : st->mtEnv == GL_MODULATE ? "modulate" : "other" );
		/* the first two bundles: the Vulkan family's third is never parsed */
		for ( b = 0; b < NUM_TEXTURE_BUNDLES && b < 2; b++ ) {
			printf( " t%d:%d:%d", (int)st->bundle[b].tcGen, st->bundle[b].numImageAnimations, st->bundle[b].numTexMods );
			for ( m = 0; m < st->bundle[b].numTexMods; m++ ) printf( ",m%d", (int)st->bundle[b].texMods[m].type );
		}
		printf( "]" );
	}
	printf( "\n" );
}

static char *HostReadAll( FILE *in, long *length ) {
	size_t capacity = 1 << 20, used = 0, n;
	char *data = (char *)malloc( capacity + 1 );
	while ( data && ( n = fread( data + used, 1, capacity - used, in ) ) > 0 ) {
		used += n;
		if ( used == capacity ) {
			char *grown = (char *)realloc( data, capacity * 2 + 1 );
			if ( !grown ) { free( data ); return NULL; }
			data = grown; capacity *= 2;
		}
	}
	if ( data ) data[used] = 0;
	*length = (long)used;
	return data;
}

/* Walk every `name { ... }` definition of the text on stdin; print one line
 * per rejected definition and the totals. */
static int HostCorpus( comGameId_t game ) {
	long length = 0;
	char *text = HostReadAll( stdin, &length );
	const char *p = text, *end;
	char name[MAX_QPATH];
	int total = 0, failed = 0;
	if ( !text ) return 2;
	setvbuf( stdout, NULL, _IONBF, 0 );
	end = text + length;
	HostInit( game );
	for ( ;; ) {
		const char *token = COM_ParseExt( &p, qtrue ), *body;
		if ( !token[0] ) break;
		Q_strncpyz( name, token, sizeof( name ) );
		body = p;
		if ( !SkipBracedSection( &p, 0 ) ) {
			printf( "UNBALANCED %s\n", name );
			break;
		}
		++total;
		if ( getenv( "FNTECH3_HOST_TRACE" ) ) fprintf( stderr, "PARSE %s\n", name );
		if ( !HostParse( name, body, end ) ) {
			++failed;
			printf( "FAIL %s | %s\n", name, hostWarning );
		} else if ( hostWarning[0] ) {
			printf( "WARN %s | %s\n", name, hostWarning );
		}
		if ( getenv( "FNTECH3_HOST_DIGEST" ) ) HostDigest( name );
	}
	printf( "TOTAL %d FAILED %d\n", total, failed );
	free( text );
	return 0;
}

HOST_CHECKS

int main( int argc, char **argv ) {
	if ( argc > 1 && !strcmp( argv[1], "corpus" ) ) {
		return HostCorpus( argc > 2 ? (comGameId_t)atoi( argv[2] ) : COM_GAME_ALICE );
	}
	return HostChecks();
}
'''

NO_CHECKS = 'static int HostChecks( void ) { return 0; }\n'

# An independently written mini-corpus of Ritual's dialect: one construct at
# a time, then the fields the parser stored. Keywords that take optional
# arguments end their line, as they must in a shader script.
CHECKS = r'''
#include <math.h>
static int hostFailures;
#define CHECK( cond ) do { if ( !( cond ) ) { fprintf( stderr, "line %d: CHECK( %s ) failed [%s]\n", __LINE__, #cond, hostWarning ); ++hostFailures; } } while ( 0 )

static qboolean HostParseText( comGameId_t game, const char *text ) {
	HostInit( game );
	return HostParse( "test/alice", text, text + strlen( text ) );
}
static qboolean Near( float a, float b ) { return fabsf( a - b ) < 1e-5f ? qtrue : qfalse; }
static qboolean Color( const shaderStage_t *st, int r, int g, int b ) {
	return R_ALICE_GEN( st )->constantColor.rgba[0] == r && R_ALICE_GEN( st )->constantColor.rgba[1] == g && R_ALICE_GEN( st )->constantColor.rgba[2] == b ? qtrue : qfalse;
}
static unsigned AliceTest( unsigned func, unsigned ref ) {
	return ( func << GLS_ALICE_ATEST_FUNC_SHIFT ) | ( ref << GLS_ALICE_ATEST_REF_SHIFT );
}

static int HostChecks( void ) {
	comGameId_t game;

	/* noDepthTest: depth test off, depth writes off even after depthWrite */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nnoDepthTest\ndepthWrite\n}\n}\n" ) );
	CHECK( ( stages[0].stateBits & GLS_DEPTHTEST_DISABLE ) && !( stages[0].stateBits & GLS_DEPTHMASK_TRUE ) );
	/* depthMask is depthWrite */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nblendFunc blend\ndepthMask\n}\n}\n" ) );
	CHECK( stages[0].stateBits & GLS_DEPTHMASK_TRUE );

	/* bare-float constant colours, truncated and clamped to a byte */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen const 1 0.5 0\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_CONST && Color( &stages[0], 255, 127, 0 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen constant 2 -1 0.3\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_CONST && Color( &stages[0], 255, 0, 76 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen constant ( 0 1 0.25 )\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_CONST && Color( &stages[0], 0, 255, 63 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen colorwave 1 0.5 0 sin 0 1 0 2\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_ALICE_COLORWAVE && Color( &stages[0], 255, 127, 0 ) &&
		R_ALICE_GEN( &stages[0] )->rgbWave.func == GF_SIN && Near( R_ALICE_GEN( &stages[0] )->rgbWave.frequency, 2 ) );

	/* alphaGen const/constant: a * 255 within a byte, else ignored */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaGen constant .5\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_CONST && R_ALICE_GEN( &stages[0] )->constantColor.rgba[3] == 127 );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaGen const 2\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->alphaGen != AGEN_CONST );

	/* dot / oneMinusDot and their optional range */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaGen dot .7 .6\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_ALICE_DOT && Near( stages[0].aliceDotRange[0], .7f ) && Near( stages[0].aliceDotRange[1], .6f ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaGen oneMinusDot\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_ALICE_ONE_MINUS_DOT && Near( stages[0].aliceDotRange[0], 0 ) && Near( stages[0].aliceDotRange[1], 1 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaGen dot 0.5\n}\n}\n" ) );
	CHECK( Near( stages[0].aliceDotRange[0], .5f ) && Near( stages[0].aliceDotRange[1], 1 ) );

	/* alphatest: its own bits; the later of alphatest and alphaFunc wins */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphatest greaterequal .3\n}\n}\n" ) );
	CHECK( ( stages[0].stateBits & ( GLS_ALICE_ATEST_BITS | GLS_ATEST_BITS ) ) == AliceTest( 5, 76 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphatest less 0.5\nalphaFunc GT0\n}\n}\n" ) );
	CHECK( ( stages[0].stateBits & ( GLS_ALICE_ATEST_BITS | GLS_ATEST_BITS ) ) == GLS_ATEST_GT_0 );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaFunc GE128\nalphatest equal 1\n}\n}\n" ) );
	CHECK( ( stages[0].stateBits & ( GLS_ALICE_ATEST_BITS | GLS_ATEST_BITS ) ) == AliceTest( 1, 255 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphatest sometimes .5\n}\n}\n" ) );
	CHECK( !( stages[0].stateBits & ( GLS_ALICE_ATEST_BITS | GLS_ATEST_BITS ) ) );

	/* fog volumes: bare colour, distance, the rest of the line ignored */
	CHECK( HostParseText( COM_GAME_ALICE, "{\nfogParms .5 .25 .1 1024 -1\nfogonly\n}\n" ) );
	CHECK( shader.aliceFogOnly && ( shader.contentFlags & CONTENTS_FOG ) &&
		Near( shader.fogParms.color[1], .25f ) && Near( shader.fogParms.depthForOpaque, 1024 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\nfogParms ( .5 .25 .1 ) 256\nsurfaceparm fog\n}\n" ) );
	CHECK( Near( shader.fogParms.color[2], .1f ) && Near( shader.fogParms.depthForOpaque, 256 ) );
	CHECK( !HostParseText( COM_GAME_ALICE, "{\nfogParms .5 .25\nfogonly\n}\n" ) );

	/* if / endif: shaderlod against r_shaderlod 0.5, mtex, 0, 1, nesting */
	{
		static const char *text =
			"{\nif shaderlod 0.45\n{\nmap a.tga\nrgbGen const 1 0 0\n}\nendif\n"
			"if shaderlod 0.5\n{\nmap b.tga\n}\n{\nmap c.tga\n}\nendif\n"
			"if 0\n{\nmap d.tga\n}\nif 1\n{\nmap e.tga\n}\nendif\nendif\n"
			"if 1\nif junk\n{\nmap f.tga\n}\nendif\n{\nmap g.tga\nrgbGen const 0 1 0\n}\nendif\n"
			"if mtex\n{\nmap h.tga\nrgbGen const 0 0 1\n}\nendif\n"
			"if no_mtex\n{\nmap i.tga\n}\nendif\n"
			"endif\n"
			"}\n";
		CHECK( HostParseText( COM_GAME_ALICE, text ) );
		CHECK( stages[0].active && Color( &stages[0], 255, 0, 0 ) );
		CHECK( stages[1].active && Color( &stages[1], 0, 255, 0 ) );
		CHECK( stages[2].active && Color( &stages[2], 0, 0, 255 ) );
		CHECK( !stages[3].active );
		HostInit( COM_GAME_ALICE );
		glConfig.numTextureUnits = 1;
		r_shaderlod->value = 0.4f;
		CHECK( HostParse( "test/alice", text, text + strlen( text ) ) );
		CHECK( stages[0].active && Color( &stages[0], 0, 255, 0 ) );
		CHECK( stages[1].active && !Color( &stages[1], 0, 0, 255 ) && !stages[2].active );
		glConfig.numTextureUnits = 2;
	}

	/* wavenormal, and an unknown deform keeping off the next keyword */
	CHECK( HostParseText( COM_GAME_ALICE,
		"{\ndeformVertexes wavenormal 512 0.04 0.03 0 sin 0 1 0 .3\ndeformVertexes bogus 1 2 3\n{\nmap a.tga\n}\n}\n" ) );
	CHECK( shader.numDeforms == 2 && shader.deforms[0].deformation == DEFORM_ALICE_WAVENORMAL &&
		Near( shader.deforms[0].deformationSpread, 1.0f / 512 ) && Near( shader.deforms[0].moveVector[0], .04f ) &&
		Near( shader.deforms[0].moveVector[1], .03f ) && shader.deforms[0].deformationWave.func == GF_SIN &&
		Near( shader.deforms[0].deformationWave.frequency, .3f ) );

	/* compiler-only keywords, portalsky, subdivisions, force32bit, foggen */
	CHECK( HostParseText( COM_GAME_ALICE, "{\nsurfaceLight 300\nsurfaceColor .5 .5 .6\nsurfaceAngle 10\n"
		"surfaceDensity 2\nportalsky\nsubdivisions 4\nforce32bit\nfoggen sin 0 1 0 1\n{\nmap a.tga\n}\n}\n" ) );
	CHECK( shader.alicePortalSky && Near( shader.sort, ALICE_SORT_PORTALSKY ) && Near( shader.aliceSubdivisions, 4 ) &&
		shader.aliceFogGen.func == GF_SIN );

	/* Alice's sort scale */
	CHECK( HostParseText( COM_GAME_ALICE, "{\nsort 5\n{\nmap a.tga\n}\n}\n" ) && Near( shader.sort, 4 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\nsort 2\n{\nmap a.tga\n}\n}\n" ) && Near( shader.sort, ALICE_SORT_PORTALSKY ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\nsort 1\n{\nmap a.tga\n}\n}\n" ) && Near( shader.sort, SS_PORTAL ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\nsort decal\n{\nmap a.tga\n}\n}\n" ) && Near( shader.sort, SS_DECAL ) );

	/* texture modifiers */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\ntcMod offset 0 -1\ntcMod offset fromEntity 0\n"
		"tcMod macro 0.1 0.2\ntcMod parallax .1 .2\n}\n}\n" ) );
	CHECK( stages[0].bundle[0].numTexMods == 4 );
	CHECK( stages[0].bundle[0].texMods[0].type == TMOD_OFFSET && Near( stages[0].bundle[0].texMods[0].offset[1], -1 ) );
	CHECK( stages[0].bundle[0].texMods[1].type == TMOD_ALICE_OFFSET &&
		stages[0].bundle[0].texMods[1].offset[0] == ALICE_SHADER_FROM_ENTITY );
	CHECK( stages[0].bundle[0].texMods[2].type == TMOD_ALICE_MACRO && Near( stages[0].bundle[0].texMods[2].scale[0], 10 ) &&
		Near( stages[0].bundle[0].texMods[2].scale[1], 5 ) );
	CHECK( stages[0].bundle[0].texMods[3].type == TMOD_ALICE_PARALLAX && Near( stages[0].bundle[0].texMods[3].scale[1], .2f ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\ntcMod scroll fromEntity 0\ntcMod scroll 1 2\n"
		"tcMod rotate fromEntity\ntcMod rotate 10\n}\n}\n" ) );
	CHECK( stages[0].bundle[0].texMods[0].type == TMOD_ALICE_SCROLL && stages[0].bundle[0].texMods[1].type == TMOD_SCROLL &&
		stages[0].bundle[0].texMods[2].type == TMOD_ALICE_ROTATE && stages[0].bundle[0].texMods[3].type == TMOD_ROTATE );

	/* colour and alpha sources */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen fromEntity\nalphaGen fromClient\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_ENTITY && R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_VERTEX );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen fromClient\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_VERTEX && R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_VERTEX );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen globalColor\nalphaGen globalAlpha\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_ALICE_GLOBAL && R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_ALICE_GLOBAL );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen global\nalphaGen fromEntity\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_ALICE_GLOBAL && R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_ENTITY );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaGen skyAlpha\n}\n{\nmap a.tga\nalphaGen oneMinusSkyAlpha\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_ALICE_SKY_ALPHA && R_ALICE_GEN( &stages[1] )->alphaGen == AGEN_ALICE_ONE_MINUS_SKY_ALPHA );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaGen lightingSpecular 1 10 20 30\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_LIGHTING_SPECULAR && stages[0].aliceSpecularCustom &&
		Near( stages[0].aliceSpecular[0], 255 ) && Near( stages[0].aliceSpecular[3], 30 ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nalphaGen lightingSpecular\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_LIGHTING_SPECULAR && !stages[0].aliceSpecularCustom );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nrgbGen wave sin fromEntity 1 random 1\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbWave.base == ALICE_SHADER_FROM_ENTITY && R_ALICE_GEN( &stages[0] )->rgbWave.phase >= 0 && R_ALICE_GEN( &stages[0] )->rgbWave.phase <= 1 &&
		Near( R_ALICE_GEN( &stages[0] )->rgbWave.amplitude, 1 ) );

	/* animMapOnce, frameFromEntity, tcGen vector */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nanimMapOnce 10 a.tga b.tga c.tga\nframeFromEntity\n"
		"tcGen vector 1 0 0 0 1 0\n}\n}\n" ) );
	CHECK( stages[0].bundle[0].numImageAnimations == 3 && stages[0].bundle[0].aliceAnimMapOnce &&
		stages[0].bundle[0].aliceFrameFromEntity && stages[0].bundle[0].imageAnimationSpeed == 10 );
	CHECK( stages[0].bundle[0].tcGen == TCGEN_VECTOR && Near( stages[0].bundle[0].tcGenVectors[1][1], 1 ) );

	/* nextBundle: the images, texture coordinates and modifiers after it fill
	 * bundle 1, drawn with GL_MODULATE; colour and alpha stay the stage's */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nblendFunc GL_ONE GL_ONE\nrgbGen const 1 0 0\ntcMod scale 2 2\n"
		"nextBundle\nmap b.tga\ntcMod scroll 1 0\ntcMod scale 3 3\nframeFromEntity\nalphaGen global\n}\n}\n" ) );
	CHECK( stages[0].bundle[0].numTexMods == 1 && stages[0].bundle[1].numTexMods == 2 &&
		stages[0].bundle[1].texMods[0].type == TMOD_SCROLL && stages[0].bundle[1].image[0] != NULL &&
		stages[0].bundle[1].aliceFrameFromEntity && !stages[0].bundle[0].aliceFrameFromEntity );
	CHECK( stages[0].mtEnv == GL_MODULATE && shader.multitextureEnv && R_AliceStageHasNextBundle( &stages[0] ) &&
		R_AliceStageBundles( &stages[0] ) == 2 && !R_AliceStageCollapses( &stages[0] ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_CONST && R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_ALICE_GLOBAL &&
		stages[0].bundle[1].tcGen == TCGEN_TEXTURE );
	CHECK( HostParseText( COM_GAME_FAKK2, "{\n{\nmap a.tga\nnextbundle\nmap b.tga\ntcGen environment\n}\n}\n" ) );
	CHECK( stages[0].bundle[1].tcGen == TCGEN_ENVIRONMENT_MAPPED && stages[0].bundle[0].tcGen == TCGEN_TEXTURE &&
		stages[0].mtEnv == GL_MODULATE );
	/* a third bundle is refused; one left without an image is dropped */
	CHECK( !HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nnextBundle\nmap b.tga\nnextBundle\nmap c.tga\n}\n}\n" ) );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\nnextBundle\n}\n}\n" ) && !stages[0].bundle[1].image[0] &&
		!stages[0].mtEnv && R_AliceStageBundles( &stages[0] ) == 1 );
	/* without multitexture the shader is refused */
	{
		static const char *text = "{\n{\nmap a.tga\nnextBundle\nmap b.tga\n}\n}\n";
		HostInit( COM_GAME_ALICE );
		glConfig.numTextureUnits = 1;
		CHECK( !HostParse( "test/alice", text, text + strlen( text ) ) );
		glConfig.numTextureUnits = 2;
	}
	/* a collapse still fills bundle 1 of two single-bundle stages */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\n}\n}\n" ) && R_AliceStageCollapses( &stages[0] ) &&
		!R_AliceStageHasNextBundle( &stages[0] ) );

	/* F.A.K.K.2: a stage ending on a first bundle of `$lightmap` with no rgbGen
	 * takes identity, as the texture filtering it does, so the two stages'
	 * colours match and collapse as fakk2.exe's do (0x00476e8c, 0x00478fa8) */
	CHECK( HostParseText( COM_GAME_FAKK2, "{\n{\nmap $lightmap\n}\n{\nmap a.tga\nblendFunc GL_DST_COLOR GL_ZERO\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY && R_ALICE_GEN( &stages[1] )->rgbGen == CGEN_IDENTITY &&
		R_ALICE_GEN( &stages[0] )->alphaGen == R_ALICE_GEN( &stages[1] )->alphaGen );
	/* not after nextBundle, nor over a named rgbGen */
	CHECK( HostParseText( COM_GAME_FAKK2, "{\n{\nmap $lightmap\nnextBundle\nmap a.tga\n}\n}\n" ) &&
		R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY_LIGHTING );
	CHECK( HostParseText( COM_GAME_FAKK2, "{\n{\nmap $lightmap\nrgbGen identityLighting\n}\n}\n" ) &&
		R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY_LIGHTING );
	CHECK( HostParseText( COM_GAME_FAKK2, "{\n{\nmap a.tga\n}\n}\n" ) &&
		R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY_LIGHTING );
	/* Alice: a stage ending on a first bundle of `$lightmap` with no rgbGen
	 * takes identity whatever its blend, as alice.exe's parser gives it
	 * (0x0048c648..0x0048c678); Alice draws with no overbright bits, as
	 * alice.exe registers (0x00475136, 0x0047529e) */
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap $lightmap\n}\n{\nmap a.tga\nblendFunc GL_DST_COLOR GL_ZERO\n}\n}\n" ) );
	CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY && R_ALICE_GEN( &stages[1] )->rgbGen == CGEN_IDENTITY &&
		R_ALICE_GEN( &stages[0] )->alphaGen == R_ALICE_GEN( &stages[1] )->alphaGen );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap $lightmap\nblendFunc add\n}\n}\n" ) &&
		R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap $lightmap\nrgbGen identityLighting\n}\n}\n" ) &&
		R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY_LIGHTING );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nmap a.tga\n}\n}\n" ) &&
		R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY_LIGHTING );
	/* every other profile keeps Quake III's rule */
	for ( game = COM_GAME_QUAKE3; game < COM_GAME_COUNT; game = (comGameId_t)( game + 1 ) ) {
		if ( COM_GAME_ID_IS_RITUAL( game ) ) continue;
		CHECK( HostParseText( game, "{\n{\nmap $lightmap\n}\n}\n" ) &&
			R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY_LIGHTING );
		CHECK( HostParseText( game, "{\n{\nmap $lightmap\nblendFunc add\n}\n}\n" ) &&
			R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY_LIGHTING );
	}

	/* Alice's nomipmaps leaves picmip alone; stage-level flags end with the stage */
	CHECK( HostParseText( COM_GAME_ALICE, "{\nnomipmaps\n{\nmap a.tga\n}\n}\n" ) );
	CHECK( shader.noMipMaps && !shader.noPicMip );
	CHECK( HostParseText( COM_GAME_ALICE, "{\n{\nnomipmaps\nnopicmip\nforce32bit\nmap a.tga\n}\n}\n" ) );
	CHECK( !shader.noMipMaps && !shader.noPicMip );

	/* no other profile takes the dialect */
	for ( game = COM_GAME_QUAKE3; game < COM_GAME_COUNT; game = (comGameId_t)( game + 1 ) ) {
		/* Heavy Metal: F.A.K.K.2's scripts are the same dialect, and so are
		 * Medal of Honor's (R_AliceShaderDialect) */
		if ( COM_GAME_ID_IS_RITUAL( game ) || game == COM_GAME_MOHAA ) continue;
		CHECK( !HostParseText( game, "{\n{\nmap a.tga\nnoDepthTest\n}\n}\n" ) );
		CHECK( !HostParseText( game, "{\n{\nmap a.tga\nalphaGen dot 0 1\n}\n}\n" ) );
		CHECK( !HostParseText( game, "{\n{\nmap a.tga\nalphatest greaterequal .3\n}\n}\n" ) );
		CHECK( !HostParseText( game, "{\nportalsky\n{\nmap a.tga\n}\n}\n" ) );
		/* (Quake III's IHV dialect has a surfacelight of its own) */
		CHECK( !HostParseText( game, "{\nsurfaceDensity 2\n{\nmap a.tga\n}\n}\n" ) );
		CHECK( !HostParseText( game, "{\nif mtex\n{\nmap a.tga\n}\nendif\n}\n" ) );
		CHECK( HostParseText( game, "{\nsort 5\n{\nmap a.tga\n}\n}\n" ) && Near( shader.sort, 5 ) );
		CHECK( HostParseText( game, "{\n{\nmap a.tga\nrgbGen const ( 1 0 0 )\n}\n}\n" ) &&
			R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_CONST && Color( &stages[0], 255, 0, 0 ) );
		CHECK( HostParseText( game, "{\nnomipmaps\n{\nmap a.tga\n}\n}\n" ) && shader.noMipMaps && shader.noPicMip );
		CHECK( HostParseText( game, "{\n{\nmap a.tga\ntcMod scroll 1 2\n}\n}\n" ) &&
			stages[0].bundle[0].texMods[0].type == TMOD_SCROLL );
		CHECK( !shader.alicePortalSky && !shader.aliceFogOnly && !stages[0].aliceDynamic );
	}
	/* Medal of Honor takes it, sort scale and all; its own keywords are no
	 * other profile's, Ritual or not */
	CHECK( HostParseText( COM_GAME_MOHAA, "{\n{\nmap a.tga\nnoDepthTest\n}\n}\n" ) );
	CHECK( HostParseText( COM_GAME_MOHAA, "{\nsort 5\n{\nmap a.tga\n}\n}\n" ) && Near( shader.sort, 4 ) );
	CHECK( HostParseText( COM_GAME_MOHAA, "{\nnoMerge\n{\nmap a.tga\nalphaGen distFade\n}\n}\n" ) &&
		R_ALICE_GEN( &stages[0] )->alphaGen == AGEN_MOHAA_DIST_FADE );
	for ( game = COM_GAME_QUAKE3; game < COM_GAME_COUNT; game = (comGameId_t)( game + 1 ) ) {
		if ( game == COM_GAME_MOHAA ) continue;
		CHECK( !HostParseText( game, "{\nnoMerge\n{\nmap a.tga\n}\n}\n" ) );
		HostParseText( game, "{\n{\nmap a.tga\nalphaGen distFade\n}\n}\n" );
		CHECK( R_ALICE_GEN( &stages[0] )->alphaGen != AGEN_MOHAA_DIST_FADE );
	}

	/* the retail sort translation and waveform parameter reader */
	HostInit( COM_GAME_ALICE );
	CHECK( Near( R_AliceSortValue( 17 ), SS_NEAREST ) && Near( R_AliceSortValue( 3 ), SS_ENVIRONMENT ) &&
		Near( R_AliceSortValue( 0.5f ), 0.5f ) );
	CHECK( R_ShaderWaveParm( "fromEntity", qfalse ) == ALICE_SHADER_FROM_ENTITY );
	HostInit( COM_GAME_QUAKE3 );
	CHECK( R_ShaderWaveParm( "fromEntity", qfalse ) == 0.0f );

	if ( hostFailures ) {
		fprintf( stderr, "%d checks failed\n", hostFailures );
		return 1;
	}
	printf( "ALL CHECKS PASSED\n" );
	return 0;
}
'''


# Each renderer's parser, with the defines its build compiles it with.
RENDERERS = {
    'renderer': ['-DRENDERER_CLASSIC', '-DRENDERER_GLX'],
    'renderervk': ['-DRENDERER_VULKAN'],
    'rendererrtx': ['-DRENDERER_RTX'],
}


def host_source(checks=NO_CHECKS, renderer='renderer'):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    cvars, init = '', ''
    if 'r_shaderlod' in local:
        cvars = 'cvar_t *r_shaderlod = &hostCvars[8];\n'
        init = 'hostCvars[8].value = 0.5f; hostCvars[8].integer = 0;'
    return (HOST.replace('HOST_EXTRA_CVARS', cvars).replace('HOST_EXTRA_INIT', init)
            .replace('HOST_CHECKS', checks)
            .replace('#include "code/renderer/tr_shader.c"', f'#include "code/{renderer}/tr_shader.c"'))


def probe_target(compiler):
    # Outside the x86 developer shell Clang targets x86_64 on Windows, where
    # only the Q3-only diagnostic build is valid; the parser checks do not
    # depend on the pointer width.
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target and os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() != 'x86':
        target = ['-DFNTECH3_Q3_ONLY']
    return target


def build_host(folder, name, checks=NO_CHECKS, shader_source=None, renderer='renderer'):
    compiler = shutil.which('clang')
    if not compiler:
        raise unittest.SkipTest('Clang unavailable')
    target = probe_target(compiler)
    source = host_source(checks, renderer)
    if shader_source is not None:
        # Measure another revision of the parser against the same host.
        source = source.replace(f'#include "code/{renderer}/tr_shader.c"',
                                '#include "' + str(shader_source).replace('\\', '/') + '"')
    c = folder / f'{name}.c'
    exe = folder / f'{name}.exe'
    c.write_text(source, encoding='utf-8')
    command = [compiler, *target, '-std=c11', '-O1', '-w', '-I', str(ROOT), '-I', str(ROOT / 'code' / renderer),
               '-I', str(ROOT / 'code/renderercommon'),
               '-DWIN32', '-D_WINDOWS', '-DNOMINMAX', *RENDERERS[renderer],
               '-D_CRT_SECURE_NO_WARNINGS',
               str(c), str(ROOT / 'code/qcommon/q_shared.c'), str(ROOT / 'code/qcommon/q_math.c'),
               '-o', str(exe)]
    if os.name == 'nt':
        command += ['-fuse-ld=lld', '-Wl,/force:unresolved']
    else:
        command += ['-Wl,--unresolved-symbols=ignore-all', '-lm']
    built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
    if built.returncode != 0:
        raise AssertionError(built.stdout + built.stderr)
    return exe


def retail_scripts(root=ALICE_ROOT, game='base'):
    """The concatenated scripts/*.shader text of <game>/pak0.pk3, in memory."""
    pak = root / game / 'pak0.pk3'
    if not pak.is_file():
        return None
    parts = []
    with zipfile.ZipFile(pak) as archive:
        for member in sorted(archive.namelist()):
            lower = member.lower()
            if lower.startswith('scripts/') and lower.endswith('.shader'):
                parts.append(archive.read(member).decode('latin-1'))
    return '\n'.join(parts) + '\n'


def run_corpus(exe, text, game_id, digest=False):
    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    env = dict(os.environ, FNTECH3_HOST_DIGEST='1') if digest else None
    run = subprocess.run([str(exe), 'corpus', str(game_id)], input=text.encode('latin-1'),
                         capture_output=True, timeout=120, env=env, **options)
    if run.returncode != 0:
        raise AssertionError(run.stdout.decode('latin-1') + run.stderr.decode('latin-1'))
    lines = run.stdout.decode('latin-1').splitlines()
    total = next(line for line in lines if line.startswith('TOTAL '))
    parts = total.split()
    if digest:
        return [line for line in lines if line.startswith('DIGEST ')]
    return int(parts[1]), int(parts[3]), [line for line in lines if line.startswith('FAIL ')]


BACKEND_HOST = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#define or orientation
#define ALICE_SHADER_FROM_ENTITY 1234567.0f
#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK ( FUNCTABLE_SIZE - 1 )
enum { AGEN_ALICE_DOT = 1, AGEN_ALICE_ONE_MINUS_DOT };
enum { TMOD_SCROLL = 1, TMOD_ALICE_SCROLL, TMOD_ALICE_ROTATE, TMOD_ALICE_OFFSET, TMOD_ALICE_MACRO, TMOD_ALICE_PARALLAX };
enum genFunc_t { GF_NONE, GF_SIN, GF_SQUARE, GF_TRIANGLE, GF_SAWTOOTH, GF_INVERSE_SAWTOOTH, GF_NOISE };
struct waveForm_t { float base, amplitude, phase, frequency; genFunc_t func; };
struct deformStage_t { vec3_t moveVector; waveForm_t deformationWave; float deformationSpread; };
struct shaderStage_t { int alphaGen; float aliceDotRange[2]; struct { struct image_t *image[1]; } bundle[1]; };
struct image_t { int uploadWidth, uploadHeight; };
struct shader_t { int numUnfoggedPasses; shaderStage_t *stages[1]; };
struct texModInfo_t { int type; float scroll[2], offset[2], scale[2], rotateSpeed; };
struct trRefEntity_t { struct { struct { short shaderData[2]; byte surfaces[32]; } alice; } e; };
static struct { int numVertexes; float xyz[4][4], normal[4][4]; double shaderTime; shader_t *shader; } tess;
static struct { struct { vec3_t viewOrigin; } orientation; struct { vec3_t vieworg; } refdef; trRefEntity_t *currentEntity; } backEnd;
static float sinTable[FUNCTABLE_SIZE];
static float *TableForFunc( genFunc_t ) { return sinTable; }
static float scrolled[2], rotated;
static void RB_CalcScrollTexCoords( const float speed[2], float *, float * ) { scrolled[0] = speed[0]; scrolled[1] = speed[1]; }
static void RB_CalcRotateTexCoords( float speed, float *, float * ) { rotated = speed; }
'''

BACKEND_CHECKS = r'''
static bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }
int main() {
	for ( int i = 0; i < FUNCTABLE_SIZE; i++ ) sinTable[i] = std::sin( i * 2 * M_PI / FUNCTABLE_SIZE );
	/* dot and oneMinusDot, with the viewer straight above, at 60 degrees and below */
	shaderStage_t stage{};
	stage.alphaGen = AGEN_ALICE_DOT; stage.aliceDotRange[0] = 0; stage.aliceDotRange[1] = 1;
	tess.numVertexes = 3;
	for ( int i = 0; i < 3; i++ ) { VectorSet( tess.normal[i], 0, 0, 1 ); VectorClear( tess.xyz[i] ); }
	tess.xyz[1][0] = -100 * std::sqrt( 3.0f ); tess.xyz[1][2] = 0;	/* 60 degrees off the normal */
	tess.xyz[2][2] = 200;											/* the viewer is below it */
	VectorSet( backEnd.orientation.viewOrigin, 0, 0, 100 );
	byte colors[4][4] = {};
	RB_AliceCalcDotAlpha( &stage, colors[0] );
	assert( colors[0][3] == 255 && colors[1][3] == 127 && colors[2][3] == 0 );
	stage.aliceDotRange[0] = 1; stage.aliceDotRange[1] = 0;
	RB_AliceCalcDotAlpha( &stage, colors[0] );
	assert( colors[0][3] == 0 && colors[1][3] == 127 && colors[2][3] == 255 );
	stage.alphaGen = AGEN_ALICE_ONE_MINUS_DOT; stage.aliceDotRange[0] = 0; stage.aliceDotRange[1] = 1;
	RB_AliceCalcDotAlpha( &stage, colors[0] );
	assert( colors[0][3] == 0 && colors[1][3] == 63 && colors[2][3] == 255 );	/* (1-.5)^2, and (1+1)^2 capped */

	/* wavenormal: the normal moves, the position does not */
	deformStage_t ds{};
	VectorSet( ds.moveVector, 1, 0, 0 );
	ds.deformationWave = { 0, 1, 0.25f, 0, GF_SIN };
	tess.numVertexes = 1; VectorSet( tess.xyz[0], 5, 6, 7 ); VectorSet( tess.normal[0], 0, 0, 1 ); tess.shaderTime = 3;
	RB_AliceCalcWaveNormals( &ds );
	assert( Near( tess.normal[0][0], std::sqrt( 0.5f ) ) && Near( tess.normal[0][2], std::sqrt( 0.5f ) ) );
	assert( tess.xyz[0][0] == 5 && tess.xyz[0][1] == 6 && tess.xyz[0][2] == 7 );
	ds.deformationWave.func = GF_NOISE; VectorSet( tess.normal[0], 0, 0, 1 );
	RB_AliceCalcWaveNormals( &ds );
	assert( tess.normal[0][0] == 0 && tess.normal[0][2] == 1 );

	/* texture modifiers */
	trRefEntity_t entity{}; entity.e.alice.shaderData[0] = 16384; entity.e.alice.shaderData[1] = -8192;
	backEnd.currentEntity = &entity;
	float src[8] = { 0.5f, 0.5f }, dst[8] = {};
	texModInfo_t tmi{};
	tmi.type = TMOD_ALICE_OFFSET; tmi.offset[0] = ALICE_SHADER_FROM_ENTITY; tmi.offset[1] = ALICE_SHADER_FROM_ENTITY;
	RB_AliceCalcTexMod( &tmi, src, dst );
	assert( Near( dst[0], 1.5f ) && Near( dst[1], 0 ) );
	tmi.offset[1] = 2; RB_AliceCalcTexMod( &tmi, src, dst ); assert( Near( dst[1], 2.5f ) );
	tmi = {}; tmi.type = TMOD_ALICE_PARALLAX; tmi.scale[0] = 0.01f; tmi.scale[1] = 0.02f;
	VectorSet( backEnd.refdef.vieworg, 100, 200, 300 );
	RB_AliceCalcTexMod( &tmi, src, dst ); assert( Near( dst[0], 1.5f ) && Near( dst[1], 4.5f ) );
	entity.e.alice.shaderData[0] = 2048; entity.e.alice.shaderData[1] = 64 * 90;
	tmi = {}; tmi.type = TMOD_ALICE_SCROLL; tmi.scroll[0] = ALICE_SHADER_FROM_ENTITY; tmi.scroll[1] = 3;
	RB_AliceCalcTexMod( &tmi, src, dst ); assert( Near( scrolled[0], 1 ) && Near( scrolled[1], 3 ) );
	entity.e.alice.shaderData[0] = 64 * 90;
	tmi = {}; tmi.type = TMOD_ALICE_ROTATE; tmi.rotateSpeed = ALICE_SHADER_FROM_ENTITY;
	RB_AliceCalcTexMod( &tmi, src, dst ); assert( Near( rotated, 90 ) );
	/* macro: a floor projects on (x, -y), a west wall on (y, -z), over the first image */
	image_t image{ 128, 64 }; shaderStage_t first{}; first.bundle[0].image[0] = &image;
	shader_t shader{ 1, { &first } }; tess.shader = &shader;
	tess.numVertexes = 2;
	VectorSet( tess.xyz[0], 64, 32, 0 ); VectorSet( tess.normal[0], 0, 0, 1 );
	VectorSet( tess.xyz[1], 0, 64, 16 ); VectorSet( tess.normal[1], 0.9f, 0.1f, 0.2f );
	tmi = {}; tmi.type = TMOD_ALICE_MACRO; tmi.scale[0] = 1 / 0.5f; tmi.scale[1] = 1 / 0.25f;
	RB_AliceCalcTexMod( &tmi, src, dst );
	assert( Near( dst[0], 1 ) && Near( dst[1], -2 ) && Near( dst[2], 1 ) && Near( dst[3], -1 ) );

	/* the far-plane pulse phase */
	assert( R_AliceFarPlaneIndex( 0.5, 2 ) == 256 && R_AliceFarPlaneIndex( 5, 0 ) == 0 );
	assert( R_AliceFarPlaneIndex( 1e30, 1e-30 ) == 0 && R_AliceFarPlaneIndex( 2.5, 1 ) == 512 );
	std::puts( "BACKEND CHECKS PASSED" );
	return 0;
}
'''


def backend_source():
    from mesh_tag_tests import function
    import re
    calc = (ROOT / 'code/renderer/tr_shade_calc.c').read_text()
    main = (ROOT / 'code/renderer/tr_main.c').read_text()
    math = (ROOT / 'code/qcommon/q_math.c').read_text()
    source = BACKEND_HOST + re.search(r'^#define\s+WAVEVALUE[^\n]+', calc, re.M)[0] + '\n'
    source += function(math, 'vec_t VectorNormalize(')
    start = calc.index('static const vec3_t aliceBaseAxis[18]')
    source += calc[start:calc.index('};', start) + 2] + '\n'
    for signature in ('static byte RB_AliceByte(', 'static void RB_AliceCalcWaveNormals(',
                      'void RB_AliceCalcDotAlpha(', 'static float RB_AliceEntityShaderData(',
                      'void RB_AliceCalcTexMod('):
        source += function(calc, signature)
    source += function(main, 'static int R_AliceFarPlaneIndex(')
    return source + BACKEND_CHECKS


def work_folder(prefix):
    (ROOT / '.tmp').mkdir(exist_ok=True)
    return tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp')


class AliceShaderParseTests(unittest.TestCase):
    def test_dialect_through_the_real_parser(self):
        with work_folder('alice-shader-checks-') as temp:
            exe = build_host(Path(temp), 'checks', CHECKS)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('ALL CHECKS PASSED', run.stdout)

    def test_back_end_formulas(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = probe_target(compiler)
        with work_folder('alice-shader-backend-') as temp:
            cpp = Path(temp) / 'backend.cpp'
            exe = Path(temp) / 'backend.exe'
            cpp.write_text(backend_source(), encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O1', '-D_USE_MATH_DEFINES', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('BACKEND CHECKS PASSED', run.stdout)

    def test_retail_scripts_parse(self):
        text = retail_scripts()
        if text is None:
            self.skipTest('no Alice installation at ' + str(ALICE_ROOT))
        with work_folder('alice-shader-corpus-') as temp:
            exe = build_host(Path(temp), 'corpus')
            total, failed, fails = run_corpus(exe, text, 13)
            self.assertEqual(total, 2162)
            # Only the stageless compiler shaders (nodraw, clip, hint ...)
            # remain; Quake III's parser rejects a shader without stages,
            # sky or fog in the same way, and they fall back to the default.
            for line in fails:
                self.assertTrue(line.endswith('| '), line)
            self.assertEqual(failed, 23, '\n'.join(fails))

    def test_dialect_through_the_vulkan_parsers(self):
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer), work_folder(f'alice-shader-checks-{renderer}-') as temp:
                exe = build_host(Path(temp), 'checks', CHECKS, renderer=renderer)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                self.assertIn('ALL CHECKS PASSED', run.stdout)

    def test_retail_scripts_parse_alike_in_every_renderer(self):
        text = retail_scripts()
        if text is None:
            self.skipTest('no Alice installation at ' + str(ALICE_ROOT))
        digests = {}
        for renderer in RENDERERS:
            with work_folder(f'alice-shader-corpus-{renderer}-') as temp:
                exe = build_host(Path(temp), 'corpus', renderer=renderer)
                if renderer != 'renderer':
                    total, failed, fails = run_corpus(exe, text, 13)
                    self.assertEqual(total, 2162, renderer)
                    for line in fails:
                        self.assertTrue(line.endswith('| '), renderer + ': ' + line)
                    self.assertEqual(failed, 23, renderer + '\n' + '\n'.join(fails))
                digests[renderer] = run_corpus(exe, text, 13, digest=True)
        # every parsed field the digest covers, definition by definition
        self.assertEqual(len(digests['renderer']), 2162)
        for renderer in ('renderervk', 'rendererrtx'):
            differ = [(gl, vk) for gl, vk in zip(digests['renderer'], digests[renderer]) if gl != vk]
            self.assertEqual(len(digests[renderer]), 2162, renderer)
            self.assertFalse(differ, renderer + ': ' + repr(differ[:3]))

    def test_fakk2_retail_scripts_parse(self):
        """Heavy Metal: F.A.K.K.2's scripts, the same dialect with 59
        `nextBundle` stages, through every parser. Besides the stageless
        compiler shaders only two definitions are refused, as retail refuses
        them: `crank` lost its stage's opening brace and `textures/aldiewater`
        says `deform`, and an unknown general keyword fails a shader
        (alice.exe 0x0048d141)."""
        text = retail_scripts(FAKK2_ROOT, 'fakk')
        if text is None:
            self.skipTest('no Heavy Metal: F.A.K.K.2 installation at ' + str(FAKK2_ROOT))
        digests = {}
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer), work_folder(f'fakk2-shader-corpus-{renderer}-') as temp:
                exe = build_host(Path(temp), 'corpus', renderer=renderer)
                total, failed, fails = run_corpus(exe, text, 14)
                self.assertEqual(total, 1347)
                refused = sorted(line[5:line.index(' | ')] for line in fails if not line.endswith('| '))
                self.assertEqual(refused, ['crank', 'textures/aldiewater'], '\n'.join(fails))
                self.assertEqual(failed, 17, '\n'.join(fails))
                digests[renderer] = run_corpus(exe, text, 14, digest=True)
        # every `nextBundle` stage is a GL_MODULATE pair, alike in every parser:
        # 59 in the text, one inside textures/phook/volumetric_fade's `if 0`
        self.assertEqual(sum(line.count(' mtmodulate ') for line in digests['renderer']), 58)
        for renderer in ('renderervk', 'rendererrtx'):
            differ = [(gl, vk) for gl, vk in zip(digests['renderer'], digests[renderer]) if gl != vk]
            self.assertFalse(differ, renderer + ': ' + repr(differ[:3]))

    def test_vulkan_back_end_matches_the_tested_formulas(self):
        """The Vulkan family's Alice sources and view features are GL's, which
        test_back_end_formulas measures; only where a stage keeps its alpha
        source differs."""
        from mesh_tag_tests import function
        gl_calc = (ROOT / 'code/renderer/tr_shade_calc.c').read_text()
        gl_main = (ROOT / 'code/renderer/tr_main.c').read_text()
        calc_functions = ('static const waveForm_t *R_AliceWave(', 'static void RB_AliceCalcWaveNormals(',
                          'static byte RB_AliceByte(', 'void RB_AliceCalcGlobalColor(',
                          'void RB_AliceCalcGlobalAlpha(', 'void RB_AliceCalcColorWave(',
                          'void RB_AliceCalcDotAlpha(', 'void RB_AliceCalcSkyAlpha(',
                          'void RB_AliceCalcSpecularAlpha(', 'static float RB_AliceEntityShaderData(',
                          'void RB_AliceCalcTexMod(')
        main_functions = ('static int R_AliceFarPlaneIndex(', 'static void R_AliceSetupFarPlane(',
                          'qboolean R_AliceCollectSkyPortalSurface(', 'static void R_AliceRenderSkyPortal(')
        for renderer in ('renderervk', 'rendererrtx'):
            calc = (ROOT / 'code' / renderer / 'tr_shade_calc.c').read_text()
            main = (ROOT / 'code' / renderer / 'tr_main.c').read_text()
            for signature in calc_functions:
                with self.subTest(renderer=renderer, function=signature):
                    ours = function(calc, signature).replace(
                        '\t// the Vulkan family keeps a stage\'s alpha source on its bundle 0\n', '').replace(
                        'stage->bundle[0].alphaGen', 'stage->alphaGen')
                    self.assertEqual(ours, function(gl_calc, signature))
            for signature in main_functions:
                with self.subTest(renderer=renderer, function=signature):
                    self.assertEqual(function(main, signature), function(gl_main, signature))
            start = gl_calc.index('static const vec3_t aliceBaseAxis[18]')
            table = gl_calc[start:gl_calc.index('};', start)]
            self.assertIn(table, calc)

    def test_vulkan_sources_route_alice_through_the_dialect(self):
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                code = ROOT / 'code' / renderer
                shader = (code / 'tr_shader.c').read_text()
                self.assertIn('#define R_ALICE_GEN( stage ) ( &( stage )->bundle[0] )\n'
                              '#include "../renderercommon/tr_alice_shader.h"', shader)
                for call in ('R_AliceShaderDirective(', 'R_AliceStageDirective(', 'R_AliceParseTexMod(',
                             'R_AliceParseDeform(', 'R_AliceFinishShaderSort(', 'R_AliceFinishShaderStages(',
                             'R_AliceStageCollapses(', 'R_AliceKeepsDetailStage(', 'R_AliceSortValue(',
                             'R_AliceStageNeedsNormal(', 'R_ShaderWaveParm('):
                    self.assertIn(call, shader)
                init = (code / 'tr_init.c').read_text()
                start = init.index('if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {\n\t\t// alice.exe 0x004759ac')
                block = init[start:init.index('\n\t}\n', start)]
                for name in ('r_shaderlod", "0.5"', 'r_farplane", "0"', 'r_farplane_color", ".5 .5 .5"',
                             'r_skyportal_origin", "0 0 0"', 'r_debuglines_depthmask", "0"', 'r_stipplelines", "1"'):
                    self.assertIn(name, block)
                self.assertIn('COM_GAME_ID_IS_RITUAL( ri.gameId ) ? "0" : "1"', init)
                self.assertIn('if ( shader->aliceNoStatic )', (code / 'vk_vbo.c').read_text())
                shade = (code / 'tr_shade.c').read_text()
                self.assertIn('if ( RB_AliceSkipStage( pStage ) )', shade)
                for case in ('CGEN_ALICE_COLORWAVE', 'AGEN_ALICE_ONE_MINUS_SKY_ALPHA', 'TMOD_ALICE_PARALLAX'):
                    self.assertIn('case ' + case + ':', shade)
                main = (code / 'tr_main.c').read_text()
                for call in ('R_AliceSetupFarPlane();', 'R_AliceRenderSkyPortal();', 'VPF_ALICE_SKY_PORTAL'):
                    self.assertIn(call, main)
                self.assertIn('R_AliceQueueDebugLines();', (code / 'tr_scene.c').read_text())
                self.assertIn('case RC_ALICE_DEBUG_LINES:', (code / 'tr_backend.c').read_text())
                vk = (code / 'vk.c').read_text()
                self.assertIn('GLS_ALICE_ATEST_FUNC_BITS', vk)
                self.assertIn('VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME', vk)
                frag = (code / 'shaders' / 'gen_frag.tmpl').read_text()
                self.assertIn('if (color0.a != alpha_test_value) discard;', frag)
        fog = (ROOT / 'code/renderercommon/tr_rtcw_fog_vk.h').read_text()
        self.assertIn('#ifdef FN_RTCW_FOG_VK_ALICE\n\tif ( R_GAME_HAS_RITUAL_VIEW( ri.gameId ) ) {\n'
                      '\t\tdraw = VK_AliceFarFogDraw();', fog)
        # the Ritual profiles' view, and Medal of Honor's, which inherits it
        self.assertIn('#define R_GAME_HAS_RITUAL_VIEW( id ) ( COM_GAME_ID_IS_RITUAL( id ) || ( id ) == COM_GAME_MOHAA )',
                      (ROOT / 'code/renderercommon/tr_types.h').read_text())
        for renderer in ('renderervk', 'rendererrtx'):
            self.assertIn('#define FN_RTCW_FOG_VK_ALICE\n#include "../renderercommon/tr_rtcw_fog_vk.h"',
                          (ROOT / 'code' / renderer / 'tr_shade.c').read_text())

    def test_sources_route_alice_through_the_dialect(self):
        shader = (ROOT / 'code/renderer/tr_shader.c').read_text()
        self.assertIn('#include "../renderercommon/tr_alice_shader.h"', shader)
        for call in ('R_AliceShaderDirective(', 'R_AliceStageDirective(', 'R_AliceParseTexMod(',
                     'R_AliceParseDeform(', 'R_AliceFinishShaderSort(', 'R_AliceFinishShaderStages(',
                     'R_AliceStageCollapses(', 'R_AliceKeepsDetailStage(', 'R_AliceSortValue('):
            self.assertIn(call, shader)
        init = (ROOT / 'code/renderer/tr_init.c').read_text()
        start = init.index('if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {\n\t\t// alice.exe 0x004759ac')
        block = init[start:init.index('\n\t}\n', start)]
        for name in ('r_shaderlod", "0.5"', 'r_farplane", "0"', 'r_farplane_color", ".5 .5 .5"',
                     'r_skyportal_origin", "0 0 0"', 'r_debuglines_depthmask", "0"', 'r_stipplelines", "1"'):
            self.assertIn(name, block)
        self.assertIn('COM_GAME_ID_IS_RITUAL( ri.gameId ) ? "0" : "1"', init)
        vbo = (ROOT / 'code/renderer/tr_vbo.c').read_text()
        self.assertIn('if ( shader->aliceNoStatic )', vbo)
        public = (ROOT / 'code/renderercommon/tr_public.h').read_text()
        self.assertIn('const void *(*AliceDebugLines)( int *count );', public)
        client = (ROOT / 'code/client/cl_main.cpp').read_text()
        self.assertIn('rimp.AliceDebugLines =', client)


if __name__ == '__main__':
    unittest.main()
