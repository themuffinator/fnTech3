"""Elite Force's forced entity alpha (render flag 0x800) in all three renderers.

The probe compiles the shared rules in renderercommon/tr_stvef_alpha.h: which
entity alpha is forced, that only the surface list's own flushes carry it,
which stages take it, and the retail blend state 0x65. The source checks pin
where each renderer applies them.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

PROBE = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char byte;
typedef enum { qfalse, qtrue } qboolean;
#define ID_INLINE inline
typedef union { byte rgba[4]; unsigned u32; } color4ub_t;
#define GLS_SRCBLEND_SRC_ALPHA 0x00000005
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA 0x00000060
#define USE_PMLIGHT
enum { VPF_DLIGHT_SHADOW = 0x20 };
typedef struct { void *image[8]; } textureBundle_t;
typedef struct { textureBundle_t bundle[2]; } shaderStage_t;
typedef struct { struct { struct { qboolean forceAlpha; } stvef; color4ub_t shader; } e; } trRefEntity_t;
static struct {
	int stvefForcedAlpha;
	trRefEntity_t *currentEntity;
	struct { int passFlags; } viewParms;
} backEnd;
static struct { qboolean csmCasterPass; } tess;
static int flushes, alphaAtFlush;
static void RB_EndSurface( void ) { ++flushes; alphaAtFlush = backEnd.stvefForcedAlpha; }
#include "code/renderercommon/tr_stvef_alpha.h"

int main( void ) {
	trRefEntity_t entity;
	shaderStage_t single, multi;
	color4ub_t colors[4];
	int alpha, i;

	/* Retail's state for a forced stage: source-alpha blending and nothing else. */
	assert( STVEF_FORCED_ALPHA_STATE == 0x65 );

	/* Only an alpha from 1 to 254 is forced; 0 and 255 leave the entity as is. */
	memset( &entity, 0, sizeof( entity ) );
	assert( R_STVEFForcedAlpha( NULL ) == 0 );
	for ( alpha = 0; alpha < 256; ++alpha ) {
		entity.e.shader.rgba[3] = (byte)alpha;
		entity.e.stvef.forceAlpha = qfalse;
		assert( R_STVEFForcedAlpha( &entity ) == 0 );
		entity.e.stvef.forceAlpha = qtrue;
		assert( R_STVEFForcedAlpha( &entity ) == ( alpha > 0 && alpha < 255 ? alpha : 0 ) );
	}

	/* A flush from the surface list carries its entity's alpha, and only
	 * while it draws. */
	entity.e.shader.rgba[3] = 150;
	backEnd.currentEntity = &entity;
	RB_EndSurfaceWithSTVEFAlpha();
	assert( flushes == 1 && alphaAtFlush == 150 && backEnd.stvefForcedAlpha == 0 );
	entity.e.stvef.forceAlpha = qfalse;
	RB_EndSurfaceWithSTVEFAlpha();
	assert( flushes == 2 && alphaAtFlush == 0 );
	backEnd.currentEntity = NULL;
	RB_EndSurfaceWithSTVEFAlpha();
	assert( flushes == 3 && alphaAtFlush == 0 );

	/* Single-texture stages take it; multitexture stages and shadow-map
	 * passes never do. */
	memset( &single, 0, sizeof( single ) );
	memset( &multi, 0, sizeof( multi ) );
	multi.bundle[1].image[0] = &multi;
	backEnd.stvefForcedAlpha = 0;
	assert( RB_STVEFStageAlpha( &single ) == 0 );
	backEnd.stvefForcedAlpha = 77;
	assert( RB_STVEFStageAlpha( &single ) == 77 && RB_STVEFStageAlpha( &multi ) == 0 );
	tess.csmCasterPass = qtrue;
	assert( RB_STVEFStageAlpha( &single ) == 0 );
	tess.csmCasterPass = qfalse;
	backEnd.viewParms.passFlags = VPF_DLIGHT_SHADOW;
	assert( RB_STVEFStageAlpha( &single ) == 0 );
	backEnd.viewParms.passFlags = 0;
	assert( RB_STVEFStageAlpha( &single ) == 77 );

	/* Every vertex takes the alpha; its color stays. */
	for ( i = 0; i < 4; ++i ) {
		colors[i].rgba[0] = (byte)( 10 * i ); colors[i].rgba[1] = 20;
		colors[i].rgba[2] = 30; colors[i].rgba[3] = 40;
	}
	RB_STVEFSetVertexAlpha( colors, 3, 99 );
	for ( i = 0; i < 3; ++i ) {
		assert( colors[i].rgba[0] == 10 * i && colors[i].rgba[1] == 20 && colors[i].rgba[2] == 30 );
		assert( colors[i].rgba[3] == 99 );
	}
	assert( colors[3].rgba[3] == 40 );
	puts( "ok" );
	return 0;
}
'''


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start)]


class STVEFForcedAlphaTests(unittest.TestCase):
    def test_shared_rules(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-alpha-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source = folder / 'probe.c'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(PROBE, encoding='utf-8')
            target = compiler_target_args(compiler)
            build = subprocess.run([compiler, *target, '-std=c99', '-I', str(ROOT), str(source),
                '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('ok', run.stdout)

    def test_every_surface_list_flush_but_the_last_carries_the_alpha(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                backend = read(f'code/{renderer}/tr_backend.c')
                self.assertIn('#include "../renderercommon/tr_stvef_alpha.h"', backend)
                body = function_body(backend, 'RB_RenderDrawSurfList( drawSurf_t *drawSurfs')
                loop, tail = body.split('// draw the contents of the last shader batch', 1)
                self.assertGreaterEqual(loop.count('RB_EndSurfaceWithSTVEFAlpha();'), 1)
                self.assertNotIn('RB_EndSurface();', loop)
                self.assertIn('RB_EndSurface();', tail)
                self.assertNotIn('RB_EndSurfaceWithSTVEFAlpha', tail)
                local = read(f'code/{renderer}/tr_local.h')
                state = local[:local.index('} backEndState_t;')]
                self.assertIn('int stvefForcedAlpha;', state[state.rindex('typedef struct'):])

    def test_stage_iterators_apply_it_to_single_texture_stages(self):
        shade = read('code/renderer/tr_shade.c')
        self.assertIn('#include "../renderercommon/tr_stvef_alpha.h"', shade)
        generic = function_body(shade, 'static void RB_IterateStagesGeneric(')
        # Jedi Academy's forced alpha shares the branch (jka_entity_override_tests.py).
        self.assertIn('int forcedAlpha = RB_STVEFStageAlpha( pStage );', generic)
        self.assertIn('unsigned forcedState = STVEF_FORCED_ALPHA_STATE;', generic)
        self.assertIn('qboolean forced = forcedAlpha ? qtrue : qfalse;', generic)
        self.assertIn('RB_STVEFSetVertexAlpha( tess.svars.colors, tess.numVertexes, forcedAlpha );', generic)
        self.assertIn('GL_State( forced ? forcedState : pStage->stateBits );', generic)
        self.assertIn('if ( pStage->depthFragment && !forced )', generic)
        self.assertIn('if ( !depthFade && !forced ) {', generic)
        single = ['TYPE_SIGNLE_TEXTURE', 'TYPE_SIGNLE_TEXTURE_IDENTITY', 'TYPE_SIGNLE_TEXTURE_FIXED_COLOR',
                  'TYPE_SIGNLE_TEXTURE_ENT_COLOR', 'TYPE_SIGNLE_TEXTURE_DF', 'TYPE_SIGNLE_TEXTURE_ENV',
                  'TYPE_SIGNLE_TEXTURE_IDENTITY_ENV', 'TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV',
                  'TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV']
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                shade = read(f'code/{renderer}/tr_shade.c')
                self.assertIn('#include "../renderercommon/tr_stvef_alpha.h"', shade)
                pipeline = function_body(shade, 'static qboolean VK_STVEFForcedAlphaPipeline(')
                cases = set(re.findall(r'case (TYPE_\w+):', pipeline))
                self.assertEqual(cases, set(single))
                self.assertIn('def.state_bits = STVEF_FORCED_ALPHA_STATE;', pipeline)
                self.assertIn('def.depth_fade = 0;', pipeline)
                self.assertIn('def.allow_discard = 0;', pipeline)
                generic = function_body(shade, 'static void RB_IterateStagesGeneric( const shaderCommands_t *input, qboolean fogCollapse )')
                self.assertIn('RB_STVEFStageAlpha( pStage )', generic)
                self.assertIn('RB_STVEFSetVertexAlpha( tess.svars.colors[0], tess.numVertexes, forcedAlpha );', generic)
                self.assertIn('tess_flags |= TESS_RGBA0;', generic)
                self.assertIn('pipeline = forcedPipeline;', generic)
                depth_only = ' && !tess.rtDepthOnly' if renderer == 'rendererrtx' else ''
                self.assertIn(f'if ( pStage->depthFragment && !forced{depth_only} ) {{', generic)


if __name__ == '__main__':
    unittest.main()
