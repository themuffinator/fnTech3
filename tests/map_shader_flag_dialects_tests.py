"""Read Ritual's and 2015's map shader flags in Quake III's bits.

Runs the production translation (code/renderercommon/tr_map_shader_flags.h)
over each format's records and RTX's own ShaderForShaderNum after it: Alice's
and F.A.K.K.2's wood (0x10000) and Medal of Honor's metal (0x8000) and rock
(0x10000) no longer filter the path tracer's light, while each title's own
alphashadow (0x800, 0x200) does. Every other format keeps its flags. Each
renderer loads the world through it right after R_LoadShaders.
See docs/fntech3/2026-10-15-map-shader-flag-dialects.md.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_map_shader_flags.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define USE_VULKAN 1
#define RT_COMPILED_ALPHASHADOW 1
#define RT_COMPILED_LIGHTFILTER 2
enum { GLHW_GENERIC, GLHW_PERMEDIA2 };
enum { LIGHTMAP_WHITEIMAGE = -2, LIGHTMAP_BY_VERTEX = -3 };
typedef struct shader_s {
	qboolean defaultShader, isSky, rtCompiledSky, rtCompiledTranslucent; int rtCompiledFilter;
} shader_t;
static struct { dshader_t *shaders; int numShaders; } s_worldData;
static shader_t parsed[8], defaultShader;
static struct { shader_t *defaultShader; } tr = { &defaultShader };
static void Error( int code, const char *fmt, ... ) { (void)code; (void)fmt; assert( 0 ); }
static struct { void (*Error)( int, const char *, ... ); } ri = { Error };
static cvar_t vertexLight, fullbright, singleShader, uiFullscreen;
static cvar_t *r_vertexLight = &vertexLight, *r_fullbright = &fullbright, *r_singleShader = &singleShader;
static struct { cvar_t *uiFullscreen; } qlRendererCvars = { &uiFullscreen };
static struct { int hardwareType; } glConfig;
static void R_SetWorldSunFromShader( const shader_t *shader ) { (void)shader; }
static shader_t *R_FindShader( const char *name, int lightmapNum, qboolean mipRawImage ) {
	int i = (int)( strchr( name, '#' )[1] - '0' );
	(void)lightmapNum; (void)mipRawImage;
	return &parsed[i];
}
'''

CHECKS = r'''
typedef struct { const char *name; int flags, fnFlags, filter; } record_t;

static void Load( comGameBspFormat_t format, const record_t *records, int count ) {
	static dshader_t shaders[8];
	int i;
	memset( shaders, 0, sizeof( shaders ) ); memset( parsed, 0, sizeof( parsed ) );
	for ( i = 0; i < count; i++ ) {
		snprintf( shaders[i].shader, sizeof( shaders[i].shader ), "%s#%d", records[i].name, i );
		shaders[i].surfaceFlags = records[i].flags;
		shaders[i].contentFlags = CONTENTS_SOLID | CONTENTS_TRANSLUCENT;
	}
	s_worldData.shaders = shaders; s_worldData.numShaders = count;
	R_MapShaderFlagsToFn( format, s_worldData.shaders, s_worldData.numShaders );
	for ( i = 0; i < count; i++ ) {
		shader_t *shader = ShaderForShaderNum( i, 0 );
		if ( s_worldData.shaders[i].surfaceFlags != records[i].fnFlags ) {
			printf( "%d %s: %#x, wanted %#x\n", format, records[i].name, s_worldData.shaders[i].surfaceFlags, records[i].fnFlags );
		}
		assert( s_worldData.shaders[i].surfaceFlags == records[i].fnFlags );
		assert( s_worldData.shaders[i].contentFlags == ( CONTENTS_SOLID | CONTENTS_TRANSLUCENT ) );
		assert( shader->rtCompiledFilter == records[i].filter );
		assert( shader->rtCompiledSky == !!( records[i].fnFlags & SURF_SKY ) && shader->rtCompiledTranslucent );
	}
}

int main( void ) {
	/* alice.exe's infoparm table: wood 0x10000, alphashadow 0x800, castshadow
	 * 0x40, nodlight 0x800000, hint 0x1000000, slide 0x2000000 */
	const record_t ritual[] = {
		{ "textures/wood/plank", 0x10000, 0, 0 },
		{ "textures/metal/wrought", 0x800 | 0x20000, SURF_ALPHASHADOW, RT_COMPILED_ALPHASHADOW },
		{ "textures/sky/tearssky", SURF_SKY | SURF_NOIMPACT | SURF_NOMARKS | 0x800000,
			SURF_SKY | SURF_NOIMPACT | SURF_NOMARKS | SURF_NODLIGHT, 0 },
		{ "textures/common/caulkshadow", SURF_NODRAW | 0x40 | 0x400, SURF_NODRAW | SURF_NOLIGHTMAP, 0 },
		{ "textures/natural/slide", 0x2000000 | 0x1000000 | 0x4000 | 0x2000, SURF_HINT | SURF_NONSOLID | SURF_NOSTEPS, 0 },
		{ "textures/q3/filter", SURF_LIGHTFILTER, 0, 0 },
	};
	/* OpenMoHAA's surfaceflags.h: metal 0x8000, rock 0x10000, alphashadow
	 * 0x200, nolightmap 0x100, nosteps 0x400, nonsolid 0x800, nodlight
	 * 0x20000000, hint 0x40000000, backside 0x10000000, patch 0x80000000 */
	const record_t mohaa[] = {
		{ "textures/algiers/afrik_wall1a", 0x10000, 0, 0 },
		{ "textures/algiers/jh_corrdoor1a", 0x8000 | SURF_NOMARKS, SURF_NOMARKS, 0 },
		{ "textures/algiers/fort_canopy", 0x10002200, SURF_ALPHASHADOW, RT_COMPILED_ALPHASHADOW },
		{ "textures/sky/mohday1", SURF_SKY | 0x20000000, SURF_SKY | SURF_NODLIGHT, 0 },
		{ "textures/common/caulk", SURF_NODRAW | 0x100 | 0x400 | 0x800 | 0x40000000 | (int)0x80000000u,
			SURF_NODRAW | SURF_NOLIGHTMAP | SURF_NOSTEPS | SURF_NONSOLID | SURF_HINT, 0 },
	};
	/* Quake III's own values, as every other format writes them */
	const record_t quake3[] = {
		{ "textures/q3/fence", SURF_ALPHASHADOW | SURF_NOMARKS, SURF_ALPHASHADOW | SURF_NOMARKS, RT_COMPILED_ALPHASHADOW },
		{ "textures/q3/glass", SURF_LIGHTFILTER, SURF_LIGHTFILTER, RT_COMPILED_LIGHTFILTER },
		{ "textures/q3/sky", SURF_SKY | SURF_NODLIGHT | 0x40000, SURF_SKY | SURF_NODLIGHT | 0x40000, 0 },
	};
	const comGameBspFormat_t others[] = { COM_GAME_BSP_QUAKE3_IHV, COM_GAME_BSP_QUAKE3, COM_GAME_BSP_QUAKELIVE,
		COM_GAME_BSP_RTCW, COM_GAME_BSP_WOLFET, COM_GAME_BSP_RBSP, COM_GAME_BSP_Q3TEST };
	size_t i;
	int bit;

	Load( COM_GAME_BSP_FAKK, ritual, (int)( sizeof( ritual ) / sizeof( ritual[0] ) ) );
	Load( COM_GAME_BSP_MOHAA, mohaa, (int)( sizeof( mohaa ) / sizeof( mohaa[0] ) ) );
	for ( i = 0; i < sizeof( others ) / sizeof( others[0] ); i++ ) {
		Load( others[i], quake3, (int)( sizeof( quake3 ) / sizeof( quake3[0] ) ) );
		for ( bit = 0; bit < 32; bit++ ) {
			assert( R_MapSurfaceFlagsToFn( others[i], (int)( 1u << bit ) ) == (int)( 1u << bit ) );
		}
	}
	puts( "PASS" );
	return 0;
}
'''

CALL = 'R_MapShaderFlagsToFn( bspFormat, s_worldData.shaders, s_worldData.numShaders );'


class MapShaderFlagDialectTests(unittest.TestCase):
    def test_each_renderer_translates_right_after_loading_the_records(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                source = (ROOT / 'code' / renderer / 'tr_bsp.c').read_text(encoding='utf-8')
                load = function(source, 'void RE_LoadWorldMap( const char *name ) {')
                self.assertEqual(load.count(CALL), 1)
                self.assertRegex(load, r'R_LoadShaders\( &header->lumps\[LUMP_SHADERS\] \);\n(?:\t//[^\n]*\n)*\t'
                                 + re.escape(CALL))
                self.assertIn('#include "../renderercommon/tr_map_shader_flags.h"', source)

    def test_rtx_reads_each_dialect_in_quake3_bits(self):
        compiler = shutil.which('clang')
        if not compiler:
            self.skipTest('Clang C compiler unavailable')
        source = (ROOT / 'code/rendererrtx/tr_bsp.c').read_text(encoding='utf-8')
        probe_source = '\n'.join((PRELUDE, function(source, 'static shader_t *ShaderForShaderNum('), CHECKS))
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='map-flag-dialects-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); probe = folder / 'probe.c'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            probe.write_text(probe_source, encoding='utf-8')
            compiled = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c11', '-Wall', '-Wextra',
                '-Werror', '-I', str(ROOT), str(probe), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            executed = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True,
                timeout=30, **options)
            self.assertEqual(executed.returncode, 0, executed.stdout + executed.stderr)
            self.assertIn('PASS', executed.stdout)


if __name__ == '__main__':
    unittest.main()
