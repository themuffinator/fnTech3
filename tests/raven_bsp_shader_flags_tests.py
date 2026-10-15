"""Run a Raven map's shader records through each renderer's own loader.

A synthetic RBSP carrying the flag words of retail mp/duel9's shader records
goes through the production projection (tr_raven_bsp_format.h), then each
renderer's R_LoadShaders, and for RTX ShaderForShaderNum, RT_ShaderIsSky and
RT_FindSun: only the real sky is the compiler's sky, and the path tracer takes
its sun. A Quake III IBSP shader lump goes through the same loaders untouched.
See docs/fntech3/2026-10-15-raven-bsp-shader-flags.md.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
NODRAW_TEST = 's_worldData.shaders[ LittleLong( ds->shaderNum ) ].surfaceFlags & SURF_NODRAW'

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/renderercommon/tr_raven_bsp_format.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define USE_VULKAN 1
#define RT_COMPILED_ALPHASHADOW 1
#define RT_COMPILED_LIGHTFILTER 2
#define RT_LIGHT_MAX_COORD 1048576.0f
#define RT_LIGHT_MAX_VALUE 1.0e7f
enum { GLHW_GENERIC, GLHW_PERMEDIA2 };
enum { LIGHTMAP_WHITEIMAGE = -2, LIGHTMAP_BY_VERTEX = -3 };
typedef struct shader_s {
	char name[MAX_QPATH]; qboolean defaultShader, isSky; int surfaceFlags;
	qboolean rtCompiledSky, rtCompiledTranslucent; int rtCompiledFilter;
	qboolean rtSunValid; vec3_t rtSunColor, rtSunDirection;
	float rtSunIntensity, rtSunDeviance; int rtSunSamples;
} shader_t;
typedef struct { shader_t *shader; } msurface_t;
typedef struct {
	char name[MAX_QPATH], baseName[MAX_QPATH]; dshader_t *shaders; int numShaders;
	msurface_t *surfaces; int numsurfaces;
} world_t;
static world_t s_worldData;
static byte *fileBase;
static char printed[1024];
static void *HunkAlloc( int size, ha_pref pref ) { (void)pref; return calloc( 1, (size_t)size ); }
static void Error( int code, const char *fmt, ... ) { (void)code; (void)fmt; abort(); }
static void Printf( int level, const char *fmt, ... ) {
	va_list args; size_t used = strlen( printed ); (void)level;
	va_start( args, fmt ); vsnprintf( printed + used, sizeof( printed ) - used, fmt, args ); va_end( args );
}
static struct {
	void *(*Hunk_Alloc)( int, ha_pref ); void (*Error)( int, const char *, ... ); void (*Printf)( int, const char *, ... );
} ri = { HunkAlloc, Error, Printf };
static unsigned int crc32_buffer( const byte *buf, unsigned int len ) { (void)buf; (void)len; return 0; }
int Q_stricmp( const char *a, const char *b ) {
	for ( ; tolower( (unsigned char)*a ) == tolower( (unsigned char)*b ); a++, b++ ) if ( !*a ) return 0;
	return tolower( (unsigned char)*a ) - tolower( (unsigned char)*b );
}
#ifdef RTX_CHAIN
static shader_t defaultShader, parsed[8];
static int numParsed;
static struct { shader_t *defaultShader; world_t *world; } tr = { &defaultShader, &s_worldData };
static cvar_t vertexLight, fullbright, singleShader, uiFullscreen;
static cvar_t *r_vertexLight = &vertexLight, *r_fullbright = &fullbright, *r_singleShader = &singleShader;
static struct { cvar_t *uiFullscreen; } qlRendererCvars = { &uiFullscreen };
static struct { int hardwareType; } glConfig;
static struct {
	qboolean q3map2, noShaderSun, sunValid; vec3_t sunLight, sunDirection; float sunDeviance; int sunSamples;
} s_rtLights;
static void R_SetWorldSunFromShader( const shader_t *shader ) { (void)shader; }
/* The parsed shaders: the sky's skyparms make it isSky, and it has a sun. */
static shader_t *R_FindShader( const char *name, int lightmapNum, qboolean mipRawImage ) {
	int i;
	(void)lightmapNum; (void)mipRawImage;
	for ( i = 0; i < numParsed; i++ ) {
		if ( !strcmp( parsed[i].name, name ) ) return &parsed[i];
	}
	assert( numParsed < 8 );
	snprintf( parsed[numParsed].name, sizeof( parsed[numParsed].name ), "%s", name );
	if ( strstr( name, "sky" ) ) {
		parsed[numParsed].isSky = qtrue; parsed[numParsed].rtSunValid = qtrue;
		VectorSet( parsed[numParsed].rtSunColor, 3, 4, 0 ); parsed[numParsed].rtSunIntensity = 2;
		VectorSet( parsed[numParsed].rtSunDirection, 0, 0, 1 );
	}
	return &parsed[numParsed++];
}
#endif
'''

CHECKS = r'''
static const struct { const char *name; unsigned surface, content; } records[] = {
	{ "textures/hoth/h_outground_3_clip", 0x00000017u, 0x00008001u },
	{ "textures/hoth/snowbiga", 0x0000000eu, 0x00008001u },
	{ "textures/mp/s_hothsky", 0x00182000u, 0x00008001u },
	{ "textures/system/caulk", 0x00300000u, 0x00008001u },
	{ "textures/mp/s_hothpit_fog", 0x00000000u, 0x80000008u },
};
enum { RECORDS = sizeof( records ) / sizeof( records[0] ) };

static void Put( unsigned char *p, unsigned value ) { FN_RavenBspPut32( p, value ); }

/* The smallest RBSP the projection accepts, with these shader records. */
static unsigned char *RavenMap( int *length ) {
	static const int strides[18] = { 1,72,16,36,48,4,4,40,12,12,80,4,72,148,49152,30,1,2 };
	int sizes[18] = { 0 }, offset = 152, i;
	unsigned char *file;
	sizes[0] = 3; sizes[1] = RECORDS * 72; sizes[7] = 40;
	for ( i = 0; i < 18; i++ ) offset += sizes[i];
	file = calloc( 1, (size_t)offset ); *length = offset;
	Put( file, 0x50534252u ); Put( file + 4, 1 );
	for ( offset = 152, i = 0; i < 18; i++ ) {
		assert( sizes[i] % strides[i] == 0 );
		if ( sizes[i] ) { Put( file + 8 + i * 8, (unsigned)offset ); Put( file + 12 + i * 8, (unsigned)sizes[i] ); }
		offset += sizes[i];
	}
	memcpy( file + FN_RavenBspI32( file + 8 ), "{}", 3 );
	for ( i = 0; i < RECORDS; i++ ) {
		unsigned char *record = file + FN_RavenBspI32( file + 16 ) + i * 72;
		memcpy( record, records[i].name, strlen( records[i].name ) );
		Put( record + 64, records[i].surface ); Put( record + 68, records[i].content );
	}
	return file;
}

static void *Allocate( int size ) { return malloc( (size_t)size ); }

static void LoadShaderLump( unsigned char *file ) {
	lump_t lump;
	lump.fileofs = FN_RavenBspI32( file + 8 + LUMP_SHADERS * 8 );
	lump.filelen = FN_RavenBspI32( file + 12 + LUMP_SHADERS * 8 );
	fileBase = file;
	R_LoadShaders( &lump );
}

int main( void ) {
	fnRavenBspProjection_t projection; fnRavenBspError_t error;
	int length, i;
	unsigned char *raven = RavenMap( &length );

	assert( FN_RavenBspProject( raven, length, Allocate, &projection, &error ) );
	LoadShaderLump( projection.data );
	assert( s_worldData.numShaders == RECORDS );
	for ( i = 0; i < RECORDS; i++ ) {
		const dshader_t *ds = &s_worldData.shaders[i];
		assert( !strcmp( ds->shader, records[i].name ) );
		/* ParseMesh's nodraw test: only caulk, Raven's 0x200000. */
		assert( !( ds->surfaceFlags & SURF_NODRAW ) == ( i != 3 ) );
		assert( !( ds->surfaceFlags & SURF_SKY ) == ( i != 2 ) );
		assert( !( ds->contentFlags & CONTENTS_TRANSLUCENT ) == ( i != 4 ) );
		assert( !( ds->contentFlags & CONTENTS_FOG ) == ( i != 4 ) );
	}
#ifdef RTX_CHAIN
	{
		msurface_t surfaces[RECORDS];
		for ( i = 0; i < RECORDS; i++ ) {
			surfaces[i].shader = ShaderForShaderNum( i, 0 );
			assert( surfaces[i].shader->rtCompiledSky == ( i == 2 ) );
			assert( RT_ShaderIsSky( surfaces[i].shader ) == ( i == 2 ) );
			assert( surfaces[i].shader->rtCompiledTranslucent == ( i == 4 ) );
			assert( !surfaces[i].shader->rtCompiledFilter );
		}
		s_worldData.surfaces = surfaces; s_worldData.numsurfaces = RECORDS;
		RT_FindSun();
		assert( !printed[0] );
		assert( s_rtLights.sunValid );
		assert( fabsf( s_rtLights.sunLight[0] - 1.2f ) < 1e-5f && fabsf( s_rtLights.sunLight[1] - 1.6f ) < 1e-5f );
	}
#endif
	free( projection.data );

	/* A Quake III IBSP lump keeps Quake III's own bits: 0x4 is sky there. */
	{
		unsigned char *q3 = calloc( 1, 152 + 72 * 2 );
		Put( q3 + 8 + LUMP_SHADERS * 8, 152 ); Put( q3 + 12 + LUMP_SHADERS * 8, 144 );
		memcpy( q3 + 152, "textures/skies/q3sky", 20 ); Put( q3 + 152 + 64, SURF_SKY | SURF_NOIMPACT ); Put( q3 + 152 + 68, CONTENTS_SOLID );
		memcpy( q3 + 224, "textures/common/caulk", 21 ); Put( q3 + 224 + 64, SURF_NODRAW ); Put( q3 + 224 + 68, CONTENTS_SOLID | CONTENTS_TRANSLUCENT );
		LoadShaderLump( q3 );
		assert( s_worldData.shaders[0].surfaceFlags == ( SURF_SKY | SURF_NOIMPACT ) );
		assert( s_worldData.shaders[1].surfaceFlags == SURF_NODRAW );
		assert( s_worldData.shaders[1].contentFlags == ( CONTENTS_SOLID | CONTENTS_TRANSLUCENT ) );
#ifdef RTX_CHAIN
		numParsed = 0; memset( parsed, 0, sizeof( parsed ) );
		assert( ShaderForShaderNum( 0, 0 )->rtCompiledSky );
		assert( !ShaderForShaderNum( 1, 0 )->rtCompiledSky && ShaderForShaderNum( 1, 0 )->rtCompiledTranslucent );
#endif
		free( q3 );
	}
	free( raven );
	puts( "PASS" );
	return 0;
}
'''


class RavenBspShaderFlagsTests(unittest.TestCase):
    def test_every_renderer_reads_raven_records_in_quake3_bits(self):
        compiler = shutil.which('clang')
        if not compiler:
            self.skipTest('Clang C compiler unavailable')
        math_source = (ROOT / 'code/qcommon/q_math.c').read_text(encoding='utf-8')
        normalize = function(math_source, 'vec_t VectorNormalize2(')
        lights = (ROOT / 'code/rendererrtx/rt/rt_lights.c').read_text(encoding='utf-8')
        material = (ROOT / 'code/rendererrtx/rt/rt_material.c').read_text(encoding='utf-8')
        for renderer in RENDERERS:
            source = (ROOT / 'code' / renderer / 'tr_bsp.c').read_text(encoding='utf-8')
            # ParseMesh tests the loaded record, which the checks above read
            self.assertIn(NODRAW_TEST, function(source, 'static void ParseMesh('))
            parts = [PRELUDE, normalize, function(source, 'static void R_ReplaceMapShaders('),
                     function(source, 'static void R_LoadShaders(')]
            defines = []
            if renderer == 'rendererrtx':
                defines = ['-DRTX_CHAIN']
                parts += [function(source, 'static shader_t *ShaderForShaderNum('),
                          function(material, 'static qboolean RT_ShaderIsSky('),
                          function(lights, 'static qboolean RT_FloatValid('),
                          function(lights, 'static qboolean RT_Vec3Valid('),
                          function(lights, 'static void RT_FindSun(')]
            with self.subTest(renderer=renderer):
                (ROOT / '.tmp').mkdir(exist_ok=True)
                with tempfile.TemporaryDirectory(prefix='raven-flags-', dir=ROOT / '.tmp') as temp:
                    folder = Path(temp); probe = folder / 'probe.c'
                    executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                    probe.write_text('\n'.join((*parts, CHECKS)), encoding='utf-8')
                    compiled = subprocess.run([compiler, *engine_fixture_args(compiler), *defines, '-std=c11',
                        '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-I', str(ROOT), str(probe),
                        '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    executed = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True,
                        timeout=30, **options)
                    self.assertEqual(executed.returncode, 0, executed.stdout + executed.stderr)
                    self.assertIn('PASS', executed.stdout)


if __name__ == '__main__':
    unittest.main()
