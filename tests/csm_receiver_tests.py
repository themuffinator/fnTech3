"""Which sun shadows a receiver takes, and how much of its light they take.

The cascaded sun shadows (r_csmShadows) used to darken every receiver by the
sun colour wherever any caster stood between it and the sun. A world surface
whose lightmap or vertex colours already hold the world's own shadow was
shaded twice, and a lamp-lit room under a roof darkened by a sun that never
lit it (F.A.K.K.2's `shield`: 31 % of the frame). Now the atlas keeps the
world's casters and the entities' in two rows; a model, or a surface lit from
the light grid, takes either row's shadow as before, while a baked surface
takes only the models' shadow, only where the world's row leaves it in the
sun, scaled per channel by the sun's share of its baked light. See
docs/fntech3/2026-10-22-fnq-shadow-formats.md, "Sun shadows on baked light".

Here:

* the shared receiver rules (code/renderercommon/tr_csm_receivers.h),
  compiled against each renderer's own colour and texture-coordinate enums,
  for the GLx stage colour and the Vulkan renderers' bundle colour;
* each renderer's atlas layout and sun-in-lightmap-units helper, compiled
  from its tr_main.c;
* the casters' rows, the receiver passes and the three back ends' sampling,
  from source;
* the embedded Vulkan SPIR-V against its GLSL, when the Vulkan SDK's
  glslangValidator is present.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(text, signature):
    # the definition, past any prototype
    start = text.index(signature)
    while ';' in text[start:text.index('{', start)]:
        start = text.index(signature, start + len(signature))
    return text[start:text.index('\n}\n', start) + 3]


def enum(text, name):
    match = re.search(r'typedef enum \{([^{}]*)\} %s;' % name, text)
    assert match, name
    return 'typedef enum {%s} %s;\n' % (match.group(1), name)


def define(text, name):
    match = re.search(r'^#define %s\s+(\S+)' % name, text, re.M)
    assert match, name
    return '#define %s %s\n' % (name, match.group(1))


def compile_and_run(test, source):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler required')
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='csm-receiver-', dir=ROOT / '.tmp') as temp:
        cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2', '-I', str(ROOT),
                                str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, timeout=120)
        test.assertEqual(run.returncode, 0, (run.stdout + run.stderr).decode('utf-8', 'replace'))


def kinds_prelude(renderer):
    local = read(f'code/{renderer}/tr_local.h')
    vulkan = renderer != 'renderer'
    defines = ''.join(define(local, n) for n in (
        'MAX_SHADER_STAGES', 'LIGHTMAP_INDEX_NONE', 'LIGHTMAP_INDEX_SHADER', 'CSM_RECEIVER_NONE',
        'CSM_RECEIVER_ALL', 'CSM_RECEIVER_LIGHTMAP', 'CSM_RECEIVER_VERTEX'))
    colour = 'colorGen_t rgbGen;' if vulkan else ''
    stage_colour = '' if vulkan else 'colorGen_t rgbGen;'
    return (
        '#include <cassert>\n#include <cstring>\n'
        + ('#define USE_VULKAN\n' if vulkan else '')
        + 'typedef int qboolean;\nenum { qfalse, qtrue };\n'
        + defines + '#define NUM_TEXTURE_BUNDLES 2\n'
        + enum(local, 'colorGen_t') + enum(local, 'texCoordGen_t')
        + 'struct image_t { int id; };\n'
        + 'typedef struct { image_t *image[8]; texCoordGen_t tcGen; int numTexMods; int lightmap; '
        + colour + ' } textureBundle_t;\n'
        + 'typedef struct { qboolean active; textureBundle_t bundle[NUM_TEXTURE_BUNDLES]; ' + stage_colour
        + ' } shaderStage_t;\n'
        + 'typedef struct { shaderStage_t *stages[MAX_SHADER_STAGES]; } shader_t;\n'
        + 'static image_t white{ 0 }, page0{ 1 }, page1{ 2 };\n'
        + 'static struct { image_t *whiteImage; } tr = { &white };\n')


KINDS_CHECKS = r'''
static shaderStage_t stages[MAX_SHADER_STAGES];
static shader_t shader;
static void Reset() {
    std::memset( stages, 0, sizeof( stages ) );
    std::memset( &shader, 0, sizeof( shader ) );
    for ( int i = 0; i < MAX_SHADER_STAGES; i++ ) shader.stages[i] = &stages[i];
}
static void Colour( int stage, colorGen_t gen ) {
#ifdef USE_VULKAN
    stages[stage].bundle[0].rgbGen = gen;
#else
    stages[stage].rgbGen = gen;
#endif
}
static void Lightmap( textureBundle_t *b, image_t *page ) {
    b->lightmap = LIGHTMAP_INDEX_SHADER; b->tcGen = TCGEN_LIGHTMAP; b->image[0] = page;
}
static int Kind( qboolean model, const textureBundle_t **lightmap ) {
    return R_CSMReceiverKind( &shader, model, lightmap );
}
int main() {
    const textureBundle_t *lm;
    /* a model takes every caster, whatever its shader */
    Reset(); stages[0].active = qtrue; Colour( 0, CGEN_VERTEX );
    assert( Kind( qtrue, &lm ) == CSM_RECEIVER_ALL && !lm );
    assert( R_CSMReceiverKind( nullptr, qfalse, &lm ) == CSM_RECEIVER_NONE && !lm );

    /* a collapsed diffuse x lightmap stage: the lightmap bundle */
    Reset(); stages[0].active = qtrue; Colour( 0, CGEN_IDENTITY );
    stages[0].bundle[0].tcGen = TCGEN_TEXTURE; stages[0].bundle[0].image[0] = &page1;
    Lightmap( &stages[0].bundle[1], &page0 );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_LIGHTMAP && lm == &stages[0].bundle[1] );
    /* ... or a lightmap stage before the diffuse one */
    Reset(); stages[0].active = stages[1].active = qtrue;
    Lightmap( &stages[0].bundle[0], &page0 ); Colour( 1, CGEN_IDENTITY );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_LIGHTMAP && lm == &stages[0].bundle[0] );
    /* a white stand-in is no baked light; a lightmap mapped some other way neither */
    Reset(); stages[0].active = qtrue; Lightmap( &stages[0].bundle[1], &white ); Colour( 0, CGEN_IDENTITY );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_NONE && !lm );
    Reset(); stages[0].active = qtrue; Lightmap( &stages[0].bundle[1], &page0 );
    stages[0].bundle[1].tcGen = TCGEN_TEXTURE; Colour( 0, CGEN_VERTEX );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_VERTEX && !lm );
    Reset(); stages[0].active = qtrue; Lightmap( &stages[0].bundle[1], nullptr );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_NONE );

    /* baked vertex colours, the light grid, and light of neither */
    const colorGen_t vertex[] = { CGEN_VERTEX, CGEN_EXACT_VERTEX, CGEN_MOHAA_LIGHTING };
    for ( colorGen_t gen : vertex ) {
        Reset(); stages[0].active = qtrue; Colour( 0, gen );
        assert( Kind( qfalse, &lm ) == CSM_RECEIVER_VERTEX && !lm );
    }
    Reset(); stages[0].active = qtrue; Colour( 0, CGEN_LIGHTING_DIFFUSE );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_ALL );
    const colorGen_t unlit[] = { CGEN_IDENTITY, CGEN_IDENTITY_LIGHTING, CGEN_CONST, CGEN_ENTITY, CGEN_WAVEFORM };
    for ( colorGen_t gen : unlit ) {
        Reset(); stages[0].active = qtrue; Colour( 0, gen );
        assert( Kind( qfalse, &lm ) == CSM_RECEIVER_NONE );
    }
    /* a later stage counts; past the first inactive one, nothing does */
    Reset(); stages[0].active = stages[1].active = qtrue; Colour( 0, CGEN_IDENTITY ); Colour( 1, CGEN_EXACT_VERTEX );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_VERTEX );
    Reset(); stages[0].active = qtrue; Colour( 0, CGEN_IDENTITY ); Colour( 1, CGEN_EXACT_VERTEX );
    Lightmap( &stages[1].bundle[0], &page0 );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_NONE );
    Reset(); shader.stages[1] = nullptr; stages[0].active = qtrue; Colour( 0, CGEN_IDENTITY );
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_NONE );
#ifdef USE_VULKAN
    /* the Vulkan renderers read every bundle's colour */
    Reset(); stages[0].active = qtrue; stages[0].bundle[1].rgbGen = CGEN_VERTEX;
    assert( Kind( qfalse, &lm ) == CSM_RECEIVER_VERTEX );
#endif

    /* batches: one kind, and one lightmap page reached the same way */
    textureBundle_t a{}, b{}, c{};
    Lightmap( &a, &page0 ); Lightmap( &b, &page0 ); Lightmap( &c, &page1 );
    assert( R_CSMReceiverBatchMatches( CSM_RECEIVER_LIGHTMAP, &a, CSM_RECEIVER_LIGHTMAP, &b ) );
    assert( !R_CSMReceiverBatchMatches( CSM_RECEIVER_LIGHTMAP, &a, CSM_RECEIVER_LIGHTMAP, &c ) );
    b.numTexMods = 1;
    assert( !R_CSMReceiverBatchMatches( CSM_RECEIVER_LIGHTMAP, &a, CSM_RECEIVER_LIGHTMAP, &b ) );
    assert( R_CSMReceiverBatchMatches( CSM_RECEIVER_LIGHTMAP, &b, CSM_RECEIVER_LIGHTMAP, &b ) );
    assert( !R_CSMReceiverBatchMatches( CSM_RECEIVER_LIGHTMAP, &a, CSM_RECEIVER_VERTEX, nullptr ) );
    assert( !R_CSMReceiverBatchMatches( CSM_RECEIVER_LIGHTMAP, nullptr, CSM_RECEIVER_LIGHTMAP, &a ) );
    assert( R_CSMReceiverBatchMatches( CSM_RECEIVER_VERTEX, nullptr, CSM_RECEIVER_VERTEX, nullptr ) );
    assert( R_CSMReceiverBatchMatches( CSM_RECEIVER_ALL, nullptr, CSM_RECEIVER_ALL, nullptr ) );
    assert( !R_CSMReceiverBatchMatches( CSM_RECEIVER_ALL, nullptr, CSM_RECEIVER_VERTEX, nullptr ) );
    return 0;
}
'''

LAYOUT_PRELUDE = r'''
#include <cassert>
#include <cmath>
#include <cstring>
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec3_t[3];
#define CSM_MAX_CASCADES 4
#define Com_Memset memset
#define VectorScale( v, s, o ) ( ( o )[0] = ( v )[0] * ( s ), ( o )[1] = ( v )[1] * ( s ), ( o )[2] = ( v )[2] * ( s ) )
#define VectorLength( v ) std::sqrt( ( v )[0] * ( v )[0] + ( v )[1] * ( v )[1] + ( v )[2] * ( v )[2] )
static float Com_Clamp( float min, float max, float value ) { return value < min ? min : value > max ? max : value; }
struct cvar_t { int integer; };
static cvar_t mapOverBright = { 2 };
static cvar_t *r_mapOverBrightBits = &mapOverBright;
static struct { int overbrightBits; struct { vec3_t color; vec3_t light; qboolean unitColour; } worldSun; } tr;
'''

LAYOUT_CHECKS = r'''
static bool Near( float a, float b ) { return std::fabs( a - b ) <= 1e-5f * std::fmax( 1.0f, std::fabs( b ) ); }
int main() {
    csmShadowAtlasLayout_t layout;
    /* a row of cascades for the world's casters, one for the entities' */
    assert( R_CSMShadowAtlasLayout( 4, 1024, 16384, &layout ) );
    assert( layout.cascadeCount == 4 && layout.cascadeSize == 1024 );
    assert( layout.width == 4096 && layout.height == 1024 * CSM_ATLAS_LAYERS && CSM_ATLAS_LAYERS == 2 );
    assert( CSM_LAYER_WORLD == 0 && CSM_LAYER_MODELS == 1 );
    /* the rows fit the texture limit too */
    assert( R_CSMShadowAtlasLayout( 1, 4096, 4096, &layout ) );
    assert( layout.cascadeSize == 2048 && layout.width == 2048 && layout.height == 4096 );
    assert( R_CSMShadowAtlasLayout( 4, 4096, 4096, &layout ) );
    assert( layout.cascadeSize == 1024 && layout.height == 2048 );
    assert( R_CSMShadowAtlasLayout( 1, 128, 256, &layout ) && layout.height == 256 );
    assert( !R_CSMShadowAtlasLayout( 1, 128, 255, &layout ) );
    assert( !R_CSMShadowAtlasLayout( 0, 1024, 4096, &layout ) );

    /* the sun shifted as the lightmaps are, over 255: F.A.K.K.2's fakkhouse,
       an entity sun given in lightmap units */
    vec3_t out;
    tr.worldSun.unitColour = qfalse;
    tr.worldSun.color[0] = 1.0f; tr.worldSun.color[1] = 1.0f; tr.worldSun.color[2] = 50.0f / 56.0f;
    tr.worldSun.light[0] = 56.0f; tr.worldSun.light[1] = 56.0f; tr.worldSun.light[2] = 50.0f;
    tr.overbrightBits = 0;
    R_CSMSunLightmapColor( out );
    assert( Near( out[0], 224.0f / 255.0f ) && Near( out[2], 200.0f / 255.0f ) );
    tr.overbrightBits = 1;
    R_CSMSunLightmapColor( out );
    assert( Near( out[0], 112.0f / 255.0f ) );
    mapOverBright.integer = 0;
    R_CSMSunLightmapColor( out );
    assert( Near( out[0], 28.0f / 255.0f ) );
    mapOverBright.integer = 99;
    R_CSMSunLightmapColor( out );
    assert( std::isfinite( out[0] ) && Near( out[0], 56.0f * 65536.0f / 255.0f ) );

    /* a sky sun its compiler stored at unit length (tr_sky_sun_units.h):
       Quake III's q3dm6, `q3map_sun 0.5 0.6 0.8 150 30 60`, kept as
       0.625 0.75 1 x 150 */
    mapOverBright.integer = 1; tr.overbrightBits = 1;
    tr.worldSun.color[0] = 0.625f; tr.worldSun.color[1] = 0.75f; tr.worldSun.color[2] = 1.0f;
    tr.worldSun.light[0] = 93.75f; tr.worldSun.light[1] = 112.5f; tr.worldSun.light[2] = 150.0f;
    tr.worldSun.unitColour = qtrue;
    R_CSMSunLightmapColor( out );
    assert( Near( out[0], 0.5f / std::sqrt( 1.25f ) * 150.0f / 255.0f ) );
    assert( Near( out[2], 0.8f / std::sqrt( 1.25f ) * 150.0f / 255.0f ) );
    /* q3map2's and Wolfenstein's: a brightest channel of 1, as kept */
    tr.worldSun.unitColour = qfalse;
    R_CSMSunLightmapColor( out );
    assert( Near( out[0], 93.75f / 255.0f ) && Near( out[2], 150.0f / 255.0f ) );
    /* a colourless sun is left as it is */
    tr.worldSun.unitColour = qtrue;
    tr.worldSun.color[0] = tr.worldSun.color[1] = tr.worldSun.color[2] = 0.0f;
    R_CSMSunLightmapColor( out );
    assert( Near( out[0], 93.75f / 255.0f ) );
    return 0;
}
'''


class ReceiverKindTests(unittest.TestCase):
    def test_the_shared_rules_on_each_renderers_types(self):
        header = read('code/renderercommon/tr_csm_receivers.h')
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                compile_and_run(self, kinds_prelude(renderer) + header + KINDS_CHECKS)

    def test_the_header_reads_sun_units_from_the_planner(self):
        header = read('code/renderercommon/tr_csm_receivers.h')
        self.assertIn('S x max(0, N . toSun) / L', header)
        self.assertIn('csmPlan_t.sunLightmapColor', header)


class AtlasLayoutTests(unittest.TestCase):
    def test_each_renderers_layout_and_sun_units(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                local = read(f'code/{renderer}/tr_local.h')
                main = read(f'code/{renderer}/tr_main.c')
                layout = re.search(r'typedef struct \{[^{}]*\} csmShadowAtlasLayout_t;', local).group(0)
                defines = ''.join(define(local, n) for n in ('CSM_ATLAS_LAYERS', 'CSM_LAYER_WORLD', 'CSM_LAYER_MODELS'))
                start = main.index('static int R_ShadowFloorPowerOfTwo( int value )')
                end = main.index('\n}\n', main.index('qboolean R_CSMShadowAtlasLayout(')) + 3
                compile_and_run(self, LAYOUT_PRELUDE + defines + layout + '\n' + main[start:end] + LAYOUT_CHECKS)

    def test_the_planner_and_shadow_manager_use_both_rows(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                main = read(f'code/{renderer}/tr_main.c')
                self.assertIn('\tR_CSMSunLightmapColor( tr.csm.sunLightmapColor );\n',
                              function(main, 'static void R_PlanCascadedShadows( void )'))
                self.assertIn('manager->csmAtlasHeight = tr.csm.resolution * CSM_ATLAS_LAYERS;', main)
                self.assertIn('vec3_t\t\tsunLightmapColor;', read(f'code/{renderer}/tr_local.h'))


class BackendSourceTests(unittest.TestCase):
    def test_casters_fill_their_rows(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                backend = read(f'code/{renderer}/tr_backend.c')
                view = function(backend, 'static void RB_BuildCSMShadowView( int cascadeIndex, int layer,')
                self.assertIn('shadowParms->viewportY = layer * cascadeSize;', view)
                cascade = function(backend, 'static int RB_RenderCSMShadowCascade(')
                self.assertIn('if ( ( entityNum == REFENTITYNUM_WORLD ) != ( layer == CSM_LAYER_WORLD ) ) {', cascade)
                atlas = function(backend, 'static void RB_RenderCSMShadowAtlas(')
                self.assertIn('for ( layer = 0; layer < CSM_ATLAS_LAYERS; layer++ ) {', atlas)
                self.assertIn('RB_BuildCSMShadowView( cascadeIndex, layer, &shadowParms );', atlas)
                self.assertIn('&tr.csm.cascades[cascadeIndex], layer, originalTime );', atlas)

    def test_receivers_take_the_shadows_their_light_calls_for(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                backend = read(f'code/{renderer}/tr_backend.c')
                self.assertLess(backend.index('#include "../renderercommon/tr_csm_receivers.h"'),
                                backend.index('static qboolean RB_CSMShadowEntityAllowed( int entityNum )'))
                model = function(backend, 'static qboolean RB_CSMShadowEntityIsModel( int entityNum )')
                self.assertIn('return ( model && model->type != MOD_BRUSH ) ? qtrue : qfalse;', model)
                passes = function(backend, 'static void RB_CSMShadowReceiverPass(')
                order = [
                    'kind = R_CSMReceiverKind( shader, RB_CSMShadowEntityIsModel( entityNum ), &lightmap );',
                    'if ( kind == CSM_RECEIVER_NONE ) {',
                    '!R_CSMReceiverBatchMatches( kind, lightmap, batchKind, batchLightmap ) ) {',
                    'RB_BeginSurface( tr.whiteShader, 0 );',
                    'tess.csmReceiverKind = kind;',
                    'tess.csmReceiverLightmap = lightmap;',
                    'tess.needsNormal = ( kind != CSM_RECEIVER_ALL );',
                    'bakedSurfaces++;',
                    'backEnd.pc.c_csmShadowReceiverBakedSurfaces += bakedSurfaces;',
                ]
                position = 0
                for needle in order:
                    found = passes.find(needle, position)
                    self.assertGreaterEqual(found, 0, needle)
                    position = found + len(needle)
                self.assertIn('recv world:%i ent:%i baked:%i', read(f'code/{renderer}/tr_cmds.c'))
                # a buffer restart keeps the normals a baked receiver asked for
                surface = read(f'code/{renderer}/tr_surface.c')
                self.assertIn('\tstate->needsNormal = tess.needsNormal;\n', surface)
                self.assertIn('\ttess.needsNormal = state->needsNormal;\n', surface)

    def test_glx_pass_binds_the_baked_light(self):
        arb = read('code/renderer/tr_arb.c')
        shadow = function(arb, 'void ARB_CSMShadowPass( void )')
        for needle in ('qglProgramLocalParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 7, sunLight );',
                       'qglProgramLocalParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 8, toSun );',
                       'qglProgramLocalParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 9, rows );',
                       'rows[0] = (float)cascadeSize / (float)atlasHeight;',
                       'VectorCopy( tr.csm.sunLightmapColor, sunLight );',
                       'toSun[0] = DotProduct( tr.csm.directionToSun, backEnd.or.axis[0] );',
                       'GL_Bind( lightmap ? lightmap->image[0] : tr.whiteImage );',
                       'R_ComputeTexCoords( 0, lightmap );',
                       'qglNormalPointer( GL_FLOAT, sizeof( tess.normal[0] ), tess.normal );'):
            self.assertIn(needle, shadow)
        vp = arb[arb.index('static const char *csmShadowVP'):arb.index('static const char *csmShadowFP')]
        for needle in ('MOV result.texcoord[2], vertex.normal;', 'MOV result.texcoord[3], vertex.texcoord[0];',
                       'MOV result.color, vertex.color;'):
            self.assertIn(needle, vp)
        fp = arb[arb.index('static const char *csmShadowFP'):]
        fp = fp[:fp.index('\n};\n')]
        for needle in ('MAX factor, occWorld, occModels;', 'MUL occModels, occModels, occWorld;',
                       'LRP factor.x, sunLight.w, factor.x, factor.y;',
                       'TEX baked, fragment.texcoord[3], texture[0], 2D;',
                       'DP3 surfNormal.w, surfNormal, toSun;', 'MIN share.xyz, share, one;',
                       'MAX share.xyz, share, sunLight.w;', 'MAD factor.xyz, -factor.x, share, one;'):
            self.assertIn(needle, fp)
        self.assertEqual(fp.count('TEX sample, offset, texture[1], 2D;'), 8)

    def test_vulkan_passes_bind_the_baked_light(self):
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                shade = function(read(f'code/{renderer}/tr_shade.c'), 'void VK_CSMShadowPass( void )')
                for needle in ('uniform.texFactors[0] = (float)cascadeSize / (float)atlasHeight;',
                               'uniform.texFactors[1] = 1.0f;',
                               'VectorCopy( tr.csm.sunLightmapColor, uniform.light.color );',
                               'uniform.light.vector[0] = DotProduct( tr.csm.directionToSun, backEnd.or.axis[0] );',
                               'R_ComputeTexCoords( 1, lightmap );',
                               'lightmap ? lightmap->image[0]->descriptor : tr.whiteImage->descriptor );',
                               'vk_bind_geometry( TESS_XYZ | TESS_RGBA0 | TESS_ST1 | TESS_NNN );'):
                    self.assertIn(needle, shade)
                vk = read(f'code/{renderer}/vk.c')
                inputs = vk[vk.index('\t\tcase TYPE_CSM_SHADOW:\n\t\t\tpush_bind'):]
                inputs = inputs[:inputs.index('break;')]
                for needle in ('push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );',
                               'push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );',
                               'push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );'):
                    self.assertIn(needle, inputs)
                vert = read(f'code/{renderer}/shaders/csm_shadow.vert')
                for needle in ('layout(location = 1) in vec4 in_color;',
                               'layout(location = 3) in vec2 in_lightmap_st;',
                               'layout(location = 5) in vec3 in_normal;'):
                    self.assertIn(needle, vert)
                frag = read(f'code/{renderer}/shaders/csm_shadow.frag')
                for needle in ('layout(set = 2, binding = 0) uniform sampler2D baked_light_texture;',
                               'every_caster += max(world, models);',
                               'models_only += models * (1.0 - world);',
                               'float occlusion = 0.25 * mix(models_only, every_caster, texFactors.y);',
                               'vec3 share = clamp(lightColor.rgb * facing / max(baked, vec3(1.0 / 255.0)), 0.0, 1.0);',
                               'share = max(share, vec3(texFactors.y));'):
                    self.assertIn(needle, frag)
        self.assertEqual(read('code/renderervk/shaders/csm_shadow.frag'),
                         read('code/rendererrtx/shaders/csm_shadow.frag'))
        self.assertEqual(read('code/renderervk/shaders/csm_shadow.vert'),
                         read('code/rendererrtx/shaders/csm_shadow.vert'))


def glslang():
    found = shutil.which('glslangValidator')
    if found:
        return found
    sdk = os.environ.get('VULKAN_SDK')
    candidate = Path(sdk) / 'Bin' / 'glslangValidator.exe' if sdk else None
    return str(candidate) if candidate and candidate.exists() else None


class SpirvTests(unittest.TestCase):
    def test_embedded_vulkan_spirv_matches_its_source(self):
        validator = glslang()
        if not validator:
            self.skipTest('glslangValidator required')
        data = read('code/renderervk/shaders/spirv/shader_data.c')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        for stage in ('vert', 'frag'):
            with self.subTest(stage=stage), tempfile.TemporaryDirectory(prefix='csm-spirv-', dir=ROOT / '.tmp') as temp:
                out = Path(temp) / 'data.spv'
                subprocess.run([validator, '-S', stage, '-V', '-o', str(out), f'csm_shadow.{stage}'],
                               cwd=ROOT / 'code' / 'renderervk' / 'shaders', check=True, capture_output=True)
                spirv = out.read_bytes()
                match = re.search(r'const unsigned char csm_shadow_%s_spv\[(\d+)\] = \{(.*?)\};' % stage, data, re.S)
                embedded = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-F]{2})', match.group(2)))
                self.assertEqual(int(match.group(1)), len(spirv))
                self.assertEqual(embedded, spirv)


if __name__ == '__main__':
    unittest.main()
