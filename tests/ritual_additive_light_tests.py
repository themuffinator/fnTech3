"""Alice's and F.A.K.K.2's additive dynamic lights add their own colour to the world.

A Ritual client game marks a dynamic light additive with its type's bit 4.
Retail's projected light adds such a light to the world and brush models
GL_ONE GL_ONE, whatever the surface, and modulates every other light by the
surface, GL_DST_COLOR GL_ONE (fakk2.exe 1.02 0x0047d32b..0x0047d33d, alice.exe
0x004935ab..0x004935bd); it projects the light over its whole radius. The
green glow round the dung pile on creeperpens is such a light.

fnTech3 keeps its per-pixel lights and lights those surfaces from white in
place of their texture (RB_RitualAdditiveWorldLight), in each renderer's light
pass, and gives the light the radius its game asked for rather than
r_dlightScale's. A model keeps its texture, and every other light and profile
is as before. The predicate is compiled against stand-in renderer types; the
renderers' sources must route their light pass texture through it.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

HOST = r'''
#include <cassert>
#include <cstdio>
typedef int qboolean;
enum { qfalse, qtrue };
typedef int qhandle_t;
#define ID_INLINE inline
enum { RT_MODEL, RT_SPRITE };
enum { MOD_BAD, MOD_BRUSH, MOD_MESH };
struct dlight_t { int additive; float ritualRadius; };
struct trRefEntity_t { struct { int reType; qhandle_t hModel; } e; };
struct model_t { int type; };
static struct { trRefEntity_t *currentEntity; } backEnd;
static struct { trRefEntity_t worldEntity; } tr;
static model_t models[3] = { { MOD_BAD }, { MOD_BRUSH }, { MOD_MESH } };
static model_t *R_GetModelByHandle( qhandle_t h ) { return &models[h < 0 || h > 2 ? 0 : h]; }
#include "code/renderercommon/tr_ritual_additive_light.h"
int main() {
	dlight_t additive = { 1, 96 }, plain = { 0, 96 }, other = { 1, 0 };
	trRefEntity_t brush = { { RT_MODEL, 1 } }, mesh = { { RT_MODEL, 2 } }, sprite = { { RT_SPRITE, 1 } };
	/* nothing is drawn: no entity */
	assert( !RB_RitualAdditiveWorldLight( &additive ) );
	backEnd.currentEntity = &tr.worldEntity;
	assert( RB_RitualAdditiveWorldLight( &additive ) );
	/* a modulated light, a light no Ritual client game gave, no light */
	assert( !RB_RitualAdditiveWorldLight( &plain ) && !RB_RitualAdditiveWorldLight( &other ) );
	assert( !RB_RitualAdditiveWorldLight( nullptr ) );
	/* brush models as the world; models and sprites keep their texture */
	backEnd.currentEntity = &brush;
	assert( RB_RitualAdditiveWorldLight( &additive ) );
	backEnd.currentEntity = &mesh;
	assert( !RB_RitualAdditiveWorldLight( &additive ) );
	backEnd.currentEntity = &sprite;
	assert( !RB_RitualAdditiveWorldLight( &additive ) );
	std::puts( "ok" );
	return 0;
}
'''


def read(*parts):
    return (ROOT.joinpath(*parts)).read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]


class RitualAdditiveLightTests(unittest.TestCase):
    def test_predicate(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        with tempfile.TemporaryDirectory(prefix='ritual-additive-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'host.cpp', Path(temp) / 'host.exe'
            cpp.write_text(HOST, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual(run.stdout.strip(), 'ok')

    def test_light_passes_route_their_texture(self):
        gl_routed = ('if ( RB_RitualAdditiveWorldLight( tess.light ) ) {\n'
                     '\t\tGL_Bind( tr.whiteImage );\n'
                     '\t} else {\n'
                     '\t\tR_BindAnimatedImage( &pStage->bundle[ tess.shader->lightingBundle ] );\n'
                     '\t}')
        arb = read('code', 'renderer', 'tr_arb.c')
        self.assertIn('#include "../renderercommon/tr_ritual_additive_light.h"', arb)
        for name in ('static void ARB_Lighting( const shaderStage_t* pStage )',
                     'static void ARB_Lighting_Fast( const shaderStage_t* pStage )'):
            with self.subTest(function=name):
                body = function(arb, name)
                self.assertIn(gl_routed, body)
                self.assertEqual(body.count('R_BindAnimatedImage('), 1)
        vbo = function(read('code', 'renderer', 'tr_vbo.c'), 'qboolean RB_StageIteratorVBODlight( void )')
        self.assertIn('if ( RB_RitualAdditiveWorldLight( tess.light ) ) {\n\t\tGL_Bind( tr.whiteImage );', vbo)
        vk = function(read('code', 'renderervk', 'tr_shade.c'), 'void VK_LightingPass( void )')
        self.assertIn('RB_RitualAdditiveWorldLight( tess.light ) ?\n\t\t\tR_ImageDescriptor( tr.whiteImage ) :', vk)
        rtx = function(read('code', 'rendererrtx', 'tr_shade.c'), 'void VK_LightingPass( void )')
        self.assertIn(gl_routed, rtx)
        for renderer in ('renderervk', 'rendererrtx'):
            self.assertIn('#include "../renderercommon/tr_ritual_additive_light.h"',
                          read('code', renderer, 'tr_shade.c'))

    def test_additive_lights_keep_their_radius(self):
        bodies = []
        for renderer in RENDERERS:
            body = function(read('code', renderer, 'tr_scene.c'), 'void RE_AddAliceLightToScene(')
            bodies.append(body)
            with self.subTest(renderer=renderer):
                self.assertIn('dl->ritualRadius = COM_GAME_ID_IS_RITUAL( ri.gameId ) ? intensity : 0.0f;', body)
                self.assertIn('VectorSet( dl->ritualColor, r, g, b );', body)
                self.assertIn('if ( dl->ritualRadius > 0.0f && dl->additive ) {\n\t\tdl->radius = intensity;', body)
                self.assertIn('R_DlightShadowProjectionFarForRadius( intensity, 1.0f );', body)
                # every other light starts with no Ritual radius
                scene = read('code', renderer, 'tr_scene.c')
                for creator in ('static void RE_AddDynamicLightToSceneWithShadowEligibility(',
                                'void RE_AddLinearLightToScene('):
                    self.assertIn('dl->ritualRadius = 0.0f;', function(scene, creator))
        self.assertEqual(len(set(bodies)), 1)


if __name__ == '__main__':
    unittest.main()
