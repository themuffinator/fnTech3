"""Jedi Academy's distortion copies (renderfx 0x2000) take Fn's entity
refraction pass in GLx, Vulkan and RTX.

Retail jamp.exe 1.01 and the 1.01 cgamex86.dll were measured with Capstone
(docs/fntech3/2026-10-15-jka-entity-refraction.md). These probes compile:

- the client adapter, which turns 0x2000 into refEntity_t.jka.distortion,
  marks the copy shadowless and still refuses every other unserved flag;
- renderercommon/tr_entity_refraction.h's rules: the profile that allocates
  the snapshot, the presence a forced alpha gives, the tint an entity-coloured
  stage takes, and the bend at a view height.

Source checks pin where each renderer captures the snapshot, draws the pass in
place of the copy's stages, and multiplies the bent scene by the tint (white
for liquids) in every program tier; the checked-in SPIR-V must match its
source when glslang is available.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from jka_entity_override_tests import compile_run, function, read, records

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


ADAPTER = r'''
#include "code/compat/jka_render_adapter.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace fntech3::jka::render;
static fntech3_jka_mp_refEntity_t Model(int renderfx) {
    fntech3_jka_mp_refEntity_t source{};
    source.reType = 0; source.renderfx = renderfx; source.hModel = 7; source.radius = 256;
    source.axis[0][0] = source.axis[1][1] = source.axis[2][2] = 0.5f; source.nonNormalizedAxes = 1;
    source.shaderRGBA[0] = 0; source.shaderRGBA[1] = 255; source.shaderRGBA[2] = 0; source.shaderRGBA[3] = 100;
    return source;
}
int main() {
    refEntity_t out{};
    /* The spawn shield's copy (cgamex86.dll 0x3004db31): distortion and forced
     * alpha, with its colour, radius and scaled axes kept for the pass. */
    assert(Entity(Model(0x2400), out) == Status::Ok);
    assert(out.jka.distortion && out.jka.forceAlpha && !out.jka.rgbTint);
    assert(out.renderfx == RF_NOSHADOW && out.radius == 256 && out.nonNormalizedAxes);
    assert(out.shader.rgba[0] == 0 && out.shader.rgba[1] == 255 && out.shader.rgba[3] == 100);
    /* Alone, with the tint (the heavy repeater's 0x2800) and with both. */
    for (int flags : {0x2000, 0x2800, 0x2c00}) {
        assert(Entity(Model(flags), out) == Status::Ok && out.jka.distortion);
        assert(out.jka.rgbTint == ((flags & 0x800) ? qtrue : qfalse));
        assert(out.jka.forceAlpha == ((flags & 0x400) ? qtrue : qfalse));
        assert(out.renderfx == RF_NOSHADOW);
    }
    /* The common flags beside it keep their meanings. */
    assert(Entity(Model(0x2000 | 0x2 | 0x80), out) == Status::Ok);
    assert(out.renderfx == (RF_THIRD_PERSON | RF_LIGHTING_ORIGIN | RF_NOSHADOW));
    /* Without it, nothing changes. */
    assert(Entity(Model(0x400), out) == Status::Ok && !out.jka.distortion && out.renderfx == 0);
    /* Every other unserved flag still refuses beside it, and so does an
     * unserved type or an invalid record. */
    for (int flag : {0x10, 0x20, 0x1000, 0x4000, 0x100000, 0x200000})
        assert(Entity(Model(0x2400 | flag), out) == Status::EntityFlags);
    auto electricity = Model(0x2400); electricity.reType = 6;
    assert(Entity(electricity, out) == Status::EntityType);
    auto lerp = Model(0x2400); lerp.backlerp = 2;
    assert(Entity(lerp, out) == Status::InvalidRecord);
    puts("ok");
    return 0;
}
'''


def rules_fixture():
    local = read('code/renderer/tr_local.h')
    return r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_types.h"
#include <cassert>
#include <cmath>
#include <cstdio>
float Com_Clamp(float min, float max, float value) { return value < min ? min : value > max ? max : value; }
''' + records(local, ('colorGen_t',)) + r'''
struct trRefEntity_t { refEntity_t e; };
static struct { comGameId_t gameId; } ri;
static cvar_t refraction;
static cvar_t *r_entityRefraction = &refraction;
#include "code/renderercommon/tr_entity_refraction.h"
static bool Near(float a, float b) { return std::fabs(a - b) <= 1e-5f * (std::fabs(b) > 1.0f ? std::fabs(b) : 1.0f); }
int main() {
    trRefEntity_t shield{};
    shield.e.jka.distortion = qtrue; shield.e.jka.forceAlpha = qtrue;
    shield.e.shader.rgba[0] = 0; shield.e.shader.rgba[1] = 255; shield.e.shader.rgba[2] = 0;
    shield.e.shader.rgba[3] = 100;
    vec3_t tint;

    /* Only Academy multiplayer, with the pass on, allocates the snapshot. */
    refraction.value = 1.0f;
    for (int id = 0; id < COM_GAME_COUNT; ++id) {
        ri.gameId = (comGameId_t)id;
        assert(R_EntityRefractionAllocates() == (id == COM_GAME_JKA_MP ? qtrue : qfalse));
    }
    ri.gameId = COM_GAME_JKA_MP;
    refraction.value = 0.0f; assert(!R_EntityRefractionAllocates());
    r_entityRefraction = nullptr; assert(!R_EntityRefractionAllocates());
    r_entityRefraction = &refraction; refraction.value = 1.0f;

    assert(R_EntityRefracts(&shield) && !R_EntityRefracts(nullptr));
    trRefEntity_t plain{}; assert(!R_EntityRefracts(&plain));

    /* Presence: the forced alpha, 100/255 for the shield; full without one. */
    assert(Near(R_EntityRefractionPresence(&shield), 100.0f / 255.0f));
    shield.e.shader.rgba[3] = 0; assert(Near(R_EntityRefractionPresence(&shield), 0.0f));
    shield.e.shader.rgba[3] = 255; assert(Near(R_EntityRefractionPresence(&shield), 1.0f));
    shield.e.shader.rgba[3] = 100;
    shield.e.jka.forceAlpha = qfalse; assert(Near(R_EntityRefractionPresence(&shield), 1.0f));
    shield.e.jka.forceAlpha = qtrue;

    /* Tint: effects/refract_2's rgbGen entity filters the bent scene green at
     * the shield's presence; an identity stage leaves it white; the 0x800 tint
     * gives the entity colour to any stage. */
    const float p = 100.0f / 255.0f;
    R_EntityRefractionTint(&shield, CGEN_ENTITY, tint);
    assert(Near(tint[0], 1.0f - p) && Near(tint[1], 1.0f) && Near(tint[2], 1.0f - p));
    R_EntityRefractionTint(&shield, CGEN_IDENTITY, tint);
    assert(Near(tint[0], 1.0f) && Near(tint[1], 1.0f) && Near(tint[2], 1.0f));
    R_EntityRefractionTint(&shield, CGEN_CONST, tint);
    assert(Near(tint[0], 1.0f) && Near(tint[2], 1.0f));
    shield.e.jka.rgbTint = qtrue;
    R_EntityRefractionTint(&shield, CGEN_IDENTITY, tint);
    assert(Near(tint[0], 1.0f - p) && Near(tint[1], 1.0f));
    /* At full presence the filter is the entity colour itself. */
    shield.e.jka.forceAlpha = qfalse; shield.e.shader.rgba[2] = 51;
    R_EntityRefractionTint(&shield, CGEN_IDENTITY, tint);
    assert(Near(tint[0], 0.0f) && Near(tint[1], 1.0f) && Near(tint[2], 0.2f));
    shield.e.jka.forceAlpha = qtrue; shield.e.jka.rgbTint = qfalse; shield.e.shader.rgba[2] = 0;

    /* Bend: a fifth of the view height at full presence and r_entityRefraction
     * 1, scaled by presence and the cvar, clamped to 4, none for a bad height. */
    static_assert(ENTITY_REFRACTION_WARP_FRACTION == 0.20f, "the measured default");
    assert(Near(R_EntityRefractionWarpPixels(&shield, 1080), 216.0f * p));
    shield.e.jka.forceAlpha = qfalse;
    assert(Near(R_EntityRefractionWarpPixels(&shield, 1080), 216.0f));
    assert(Near(R_EntityRefractionWarpPixels(&shield, 480), 96.0f));
    assert(Near(R_EntityRefractionWarpPixels(&shield, -5), 0.0f));
    refraction.value = 2.0f; assert(Near(R_EntityRefractionWarpPixels(&shield, 1080), 432.0f));
    refraction.value = 9.0f; assert(Near(R_EntityRefractionWarpPixels(&shield, 1080), 864.0f));
    refraction.value = 0.0f; assert(Near(R_EntityRefractionWarpPixels(&shield, 1080), 0.0f));
    r_entityRefraction = nullptr; assert(Near(R_EntityRefractionWarpPixels(&shield, 1080), 0.0f));
    puts("ok");
    return 0;
}
'''


def glslang():
    sdk = os.environ.get('VULKAN_SDK')
    candidates = [shutil.which('glslangValidator')]
    if sdk:
        candidates.append(str(Path(sdk) / 'Bin' / ('glslangValidator.exe' if os.name == 'nt' else 'glslangValidator')))
    return next((c for c in candidates if c and Path(c).exists()), None)


class JKAEntityRefractionTests(unittest.TestCase):
    def test_adapter_carries_distortion_as_an_engine_field(self):
        compile_run(self, ADAPTER, 'jka-refraction-adapter-')

    def test_refraction_rules(self):
        compile_run(self, rules_fixture(), 'jka-refraction-rules-')

    def test_the_cvar_is_the_same_in_every_renderer(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                init = read(f'code/{renderer}/tr_init.c')
                self.assertIn('r_entityRefraction = ri.Cvar_Get( "r_entityRefraction", "1.0", CVAR_ARCHIVE_ND );', init)
                self.assertIn('ri.Cvar_CheckRange( r_entityRefraction, "0.0", "4.0", CV_FLOAT );', init)
                self.assertIn('extern cvar_t\t*r_entityRefraction;', read(f'code/{renderer}/tr_local.h'))

    def test_the_snapshot_is_allocated_for_the_pass(self):
        arb = read('code/renderer/tr_arb.c')
        self.assertIn('( ( r_liquid && r_liquid->integer > 0 ) || R_EntityRefractionAllocates() ) &&', arb)
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                vk = read(f'code/{renderer}/vk.c')
                body = function(vk, 'static qboolean vk_liquid_requested( void )')
                self.assertIn('R_EntityRefractionAllocates()', body)
                # Every other resource gate goes through it.
                self.assertNotRegex(vk.replace(body, ''), r'r_liquid\s*&&\s*r_liquid->integer\s*\)')
                capture = function(vk, 'qboolean vk_capture_refraction_scene( void )')
                self.assertIn('backEnd.liquidScreenMapDone = qfalse;', capture)
                self.assertIn('backEnd.liquidScreenMapDone = liquidDone;', capture)

    def test_the_backend_captures_before_each_distortion_copy(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                backend = read(f'code/{renderer}/tr_backend.c')
                self.assertIn('#include "../renderercommon/tr_entity_refraction.h"', backend)
                start = backend.index('RB_RenderDrawSurfList( drawSurf_t *drawSurfs')
                body = backend[start:backend.index('\n}\n', start)]
                capture = body.index('R_EntityRefracts( &backEnd.refdef.entities[entityNum] ) ) {')
                # after the sort is decomposed, before the batch begins
                self.assertLess(body.index('R_DecomposeSort( drawSurf->sort'), capture)
                self.assertLess(capture, body.index('// change the tess parameters if needed'))
                self.assertIn('entityNum != refractionEntityNum', body)
                self.assertIn('refractionEntityNum = -1;', body)
                self.assertIn('rb_entityRefractionReady = qfalse;', body)
                block = body[capture:body.index('// change the tess parameters if needed')]
                self.assertIn('RB_EndSurfaceWithSTVEFAlpha();', block)
                self.assertIn('oldSort = MAX_UINT;', block)
                self.assertIn('oldEntityNum = -1;', block)
                if renderer == 'renderer':
                    self.assertIn('FBO_CopyLiquidScreen()', block)
                    self.assertIn('FBO_CopyLiquidDepth();', block)
                    # its normals feed the wave field
                    self.assertIn('if ( R_EntityRefracts( backEnd.currentEntity ) ) {\n\t\t\ttess.needsNormal = qtrue;', body)
                else:
                    self.assertIn('vk_capture_refraction_scene()', block)
                if renderer == 'rendererrtx':
                    active = function(backend, 'static qboolean RB_EntityRefractionViewActive( rbDrawSurfsMode_t mode )')
                    self.assertIn('mode != RB_DRAWSURFS_RT_BASE', active)
                    self.assertIn('RB_IsPrimaryFullView()', active)
                else:
                    active = function(backend, 'static qboolean RB_EntityRefractionViewActive( void )')
                    self.assertIn('!R_ViewPassIsPortal( &backEnd.viewParms )', active)
                    self.assertIn('!( backEnd.refdef.rdflags & RDF_NOWORLDMODEL )', active)
                self.assertIn('r_entityRefraction->value > 0.0f', active)

    def test_the_pass_replaces_the_copys_stages(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                shade = read(f'code/{renderer}/tr_shade.c')
                self.assertIn('#include "../renderercommon/tr_entity_refraction.h"', shade)
                pass_name = 'RB_DrawLiquidPass' if renderer == 'renderer' else 'VK_DrawLiquidPass'
                entity_name = 'RB_DrawEntityRefraction' if renderer == 'renderer' else 'VK_DrawEntityRefraction'
                generic = function(shade, 'void RB_StageIteratorGeneric( void )')
                hook = generic.index('if ( R_EntityRefracts( backEnd.currentEntity ) ) {')
                self.assertLess(hook, generic.index(f'{entity_name}();'))
                self.assertLess(generic.index(f'{entity_name}();'), generic.index('return;'))
                # before any stage, light or fog work of the batch
                for later in ('tess.dlightPass', 'RB_IterateStagesGeneric', 'RB_FogPass'):
                    self.assertLess(hook, generic.index(later))
                entity = function(shade, f'static void {entity_name}( void )')
                self.assertIn('tess.dlightPass', entity)
                self.assertIn('RB_JKAShadowPass()', entity)
                self.assertIn('!RB_EntityRefractionReady()', entity)
                self.assertIn('RB_DeformTessGeometry();', entity)
                self.assertIn(f'{pass_name}( &tess, qtrue, backEnd.currentEntity );', entity)
                if renderer == 'renderer':
                    self.assertIn('GL_Cull( CT_TWO_SIDED );', entity)
                else:
                    body = function(shade, f'static void {pass_name}(')
                    self.assertIn('vk.liquid_pipelines[refractor ? CT_TWO_SIDED : input->shader->cullType]', body)
                if renderer == 'rendererrtx':
                    self.assertIn('tess.rtDepthOnly || tess.rtFogOnly', entity)
                body = function(shade, f'static void {pass_name}(')
                self.assertIn('R_EntityRefractionWarpPixels( refractor,', body)
                self.assertIn('R_EntityRefractionTint( refractor,', body)
                self.assertIn('VectorSet( refractionTint, 1.0f, 1.0f, 1.0f );', body)
                # liquids pass no entity
                self.assertIn(f'{pass_name}( &tess, qtrue, NULL );', shade)
                self.assertIn(f'{pass_name}( &tess, qfalse, NULL );', shade)

    def test_every_program_tier_multiplies_by_the_tint(self):
        shade = read('code/renderer/tr_shade.c')
        body = function(shade, 'static void RB_DrawLiquidPass(')
        self.assertIn('Vector4Set( glxReflect, refractionTint[0], refractionTint[1], refractionTint[2],', body)
        self.assertIn('VectorCopy( refractionTint, arb.refractionTint );', body)
        self.assertIn('( refractionBase ? refractionTint[0] : fresnelColor[0] ) * 255.0f + 0.5f', body)
        env = function(shade, 'static void RB_LiquidFixedFunctionTexEnv(')
        self.assertIn('sampleScene ? GL_MODULATE : GL_REPLACE', env)
        self.assertIn('qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR );', env)
        arb = read('code/renderer/tr_arb.c')
        program = arb[arb.index('static const char *liquidRefractionFP'):arb.index('static const char *liquidSheenFP')]
        self.assertIn('"PARAM tint = program.local[8]; \\n"', program)
        self.assertLess(program.index('"TEX base, uv, texture[0], 2D; \\n"'), program.index('"MUL base.xyz, base, tint; \\n"'))
        enable = function(arb, 'void GL_LiquidProgramEnable(')
        self.assertIn('lp->refractionTint[0], lp->refractionTint[1], lp->refractionTint[2], 1.0f );', enable)
        glx = read('code/rendererglx/glx_material.cpp')
        self.assertIn('"        gl_FragColor = vec4(sceneColor * u_Reflect.rgb, alpha);\\n"', glx)
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                frag = read(f'code/{renderer}/shaders/liquid.frag')
                self.assertIn('out_color = vec4(scene * liquid_reflect.rgb, alpha);', frag)
                self.assertNotIn('out_color = vec4(scene, alpha);', frag)
                body = function(read(f'code/{renderer}/tr_shade.c'), 'static void VK_DrawLiquidPass(')
                self.assertIn('VectorCopy( refractionBase ? refractionTint : fresnelColor, slots[12] );', body)

    def test_checked_in_spirv_matches_its_source(self):
        tool = glslang()
        if not tool:
            self.skipTest('glslangValidator unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        def stored(data, name):
            match = re.search(r'const unsigned char %s\[(\d+)\] = \{(.*?)\};' % name, data, re.S)
            values = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', match[2]))
            self.assertEqual(len(values), int(match[1]))
            return values

        def compiled(renderer, stage, name, temp):
            out = Path(temp) / f'{renderer}-{name}.spv'
            built = subprocess.run([tool, '-S', stage, '-V', '-o', str(out),
                                    str(ROOT / f'code/{renderer}/shaders/{name}')],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            return out.read_bytes()

        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                data = read(f'code/{renderer}/shaders/spirv/shader_data.c')
                with tempfile.TemporaryDirectory(prefix='jka-refraction-spv-', dir=ROOT / '.tmp') as temp:
                    # Another glslang release may emit other bytes for the same
                    # source: the vertex shader, which this slice does not
                    # change, shows whether this one matches the checked-in
                    # toolchain before the fragment shader is judged.
                    if compiled(renderer, 'vert', 'liquid.vert', temp) != stored(data, 'liquid_vert_spv'):
                        self.skipTest('glslang differs from the toolchain that built shader_data.c')
                    self.assertEqual(compiled(renderer, 'frag', 'liquid.frag', temp),
                                     stored(data, 'liquid_frag_spv'))

    def test_path_tracer_leaves_the_copy_to_raster(self):
        tracer = read('code/rendererrtx/rt/rt_backend.c')
        self.assertIn('if ( ent->e.jka.rgbTint || ent->e.jka.forceAlpha || ent->e.jka.distortion ) {',
                      function(tracer, 'static int RT_EntityClass( const trRefEntity_t *ent )'))

    def test_the_host_submits_the_copy(self):
        host = read('code/client/client_jka_cgame_host.hpp')
        case = host[host.index('    case Call::CG_R_ADDREFENTITYTOSCENE:'):host.index('    case Call::CG_R_RENDERSCENE:')]
        # no longer consumed undrawn
        self.assertNotIn('0x2000u', case)
        self.assertNotIn('result = 0;', case)
        self.assertIn('render::ReadEntity(&source, value)', case)


if __name__ == '__main__':
    unittest.main()
