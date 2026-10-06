"""Jedi Academy's entity tint (0x800) and forced alpha (0x400) in Fn's renderers.

Retail jamp.exe 1.01's stage iterator, colour function and surface list were
measured with Capstone (docs/fntech3/2026-10-06-jka-entity-tint.md). These
probes compile:

- the client adapter, which turns the two flags into engine-owned
  refEntity_t.jka fields and still refuses every other unserved flag;
- each renderer's real R_ComputeColors with the shared rules in
  renderercommon/tr_jka_entity.h: the entity colour source under a tint, the
  identity alpha's 255 after it, the forced alpha on single-texture stages
  only, shadow passes untouched, and the stable deferral of a forced-alpha
  entity's surfaces;
- the Vulkan and RTX tint pipeline remap over every real shader type;
- the path tracer's ownership, which leaves these entities to raster.

Source checks pin where each renderer applies them.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
import rtx_surface_owner_tests

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def records(local, names):
    result = ''
    for name in names:
        end = local.index('} ' + name + ';') + len('} ' + name + ';')
        result += local[local.rindex('typedef ', 0, end):end] + '\n'
    return result


def defines(local, names):
    found = []
    for name in names:
        match = re.search(r'^#define\s+' + name + r'\b[^\n]*$', local, re.M)
        if not match:
            raise AssertionError(f'{name} is not defined')
        found.append(match[0])
    return '\n'.join(found) + '\n'


def compile_run(test, source, prefix):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler unavailable')
    flags = compiler_target_args(compiler)
    if os.name == 'nt' and not flags:
        flags = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp') as temp:
        cpp = Path(temp) / 'probe.cpp'
        exe = Path(temp) / ('probe.exe' if os.name == 'nt' else 'probe')
        cpp.write_text(source, encoding='utf-8')
        # The renderers' backEnd.viewParms.or is an alternative token in C++.
        built = subprocess.run([compiler, *flags, '-std=c++17', '-fno-operator-names', '-O1', '-I', str(ROOT),
                                str(cpp), '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        test.assertIn('ok', ran.stdout)


ADAPTER = r'''
#include "code/compat/jka_render_adapter.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace fntech3::jka::render;
static fntech3_jka_mp_refEntity_t Model(int renderfx) {
    fntech3_jka_mp_refEntity_t source{};
    source.reType = 0; source.renderfx = renderfx; source.hModel = 7;
    source.axis[0][0] = source.axis[1][1] = source.axis[2][2] = 1;
    source.shaderRGBA[0] = 10; source.shaderRGBA[1] = 20; source.shaderRGBA[2] = 30; source.shaderRGBA[3] = 40;
    return source;
}
int main() {
    refEntity_t out{};
    /* Neither override acquires a Quake III flag; each becomes its own field. */
    assert(Entity(Model(0x800), out) == Status::Ok);
    assert(out.jka.rgbTint && !out.jka.forceAlpha && out.renderfx == 0);
    assert(Entity(Model(0x400), out) == Status::Ok);
    assert(!out.jka.rgbTint && out.jka.forceAlpha && out.renderfx == 0);
    assert(Entity(Model(0xc00), out) == Status::Ok);
    assert(out.jka.rgbTint && out.jka.forceAlpha && out.renderfx == 0);
    assert(out.shader.rgba[0] == 10 && out.shader.rgba[3] == 40 && out.hModel == 7);
    /* The common flags beside them keep their own meanings. */
    assert(Entity(Model(0xc00 | 0x1 | 0x40), out) == Status::Ok);
    assert(out.jka.rgbTint && out.jka.forceAlpha && out.renderfx == (RF_MINLIGHT | RF_NOSHADOW));
    assert(Entity(Model(0), out) == Status::Ok);
    assert(!out.jka.rgbTint && !out.jka.forceAlpha);
    /* Sprites and beams take them too, as every entity's stages do in retail. */
    auto sprite = Model(0xc00); sprite.reType = 2; sprite.radius = 4;
    assert(Entity(sprite, out) == Status::Ok && out.reType == RT_SPRITE && out.jka.forceAlpha);
    /* Retail's other render flags stay refused by name: node depth, volumetric,
     * shadow only, distortion, forked, tapered, grow, both disintegrations,
     * the animation index, alpha depth and forced post-render. */
    for (int flag : {0x10, 0x20, 0x1000, 0x2000, 0x4000, 0x8000, 0x10000, 0x20000, 0x40000,
                     0x80000, 0x100000, 0x200000, 0x400000, int(0x80000000u)}) {
        assert(Entity(Model(0xc00 | flag), out) == Status::EntityFlags);
        assert(Entity(Model(flag), out) == Status::EntityFlags);
    }
    puts("ok");
    return 0;
}
'''


def color_fixture(renderer):
    local = read(f'code/{renderer}/tr_local.h')
    shade = read(f'code/{renderer}/tr_shade.c')
    calc = read(f'code/{renderer}/tr_shade_calc.c')
    vulkan = renderer != 'renderer'
    shadow = re.search(r'VPF_DLIGHT_SHADOW\s*=\s*(0x[0-9a-fA-F]+)', local)[1]
    source = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_types.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#define USE_PMLIGHT
#define Com_Memset memset
#define Com_Memcpy memcpy
'''
    source += defines(local, ('GLS_SRCBLEND_SRC_ALPHA', 'GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA', 'REFENTITYNUM_BITS',
                              'REFENTITYNUM_MASK', 'REFENTITYNUM_WORLD', 'DLIGHT_BITS', 'FOGNUM_BITS',
                              'QSORT_FOGNUM_SHIFT', 'QSORT_REFENTITYNUM_SHIFT'))
    source += records(local, ('alphaGen_t', 'colorGen_t', 'acff_t'))
    source += f'enum {{ VPF_DLIGHT_SHADOW = {shadow} }};\n'
    source += r'''
constexpr int N = 8;
struct image_t;
struct waveForm_t { int func; };
struct fog_t { color4ub_t colorInt; };
struct world_t { fog_t *fogs; };
struct shader_t { float portalRangeR; };
'''
    if vulkan:
        source += r'''
struct textureBundle_t {
    image_t *image[1]; waveForm_t rgbWave; colorGen_t rgbGen; waveForm_t alphaWave; alphaGen_t alphaGen;
    color4ub_t constantColor; float rtcwNormalZBounds[2]; acff_t adjustColorsForFog;
};
struct shaderStage_t { textureBundle_t bundle[2]; qboolean aliceSpecularCustom; };
#define GEN(stage) (stage).bundle[0]
'''
    else:
        source += r'''
struct textureBundle_t { image_t *image[1]; };
struct shaderStage_t {
    textureBundle_t bundle[2]; waveForm_t rgbWave; colorGen_t rgbGen; waveForm_t alphaWave; alphaGen_t alphaGen;
    color4ub_t constantColor; float rtcwNormalZBounds[2]; acff_t adjustColorsForFog; qboolean aliceSpecularCustom;
};
#define GEN(stage) (stage)
'''
    source += r'''
struct trRefEntity_t { refEntity_t e; };
struct drawSurf_t { unsigned sort; int *surface; unsigned flags; };
static struct {
    float identityLight; byte identityLightByte; world_t *world;
    struct { trRefEntity_t *entities; int num_entities; } refdef;
} tr;
static struct {
    trRefEntity_t *currentEntity; int stvefForcedAlpha;
    struct { int passFlags; struct { vec3_t origin; } or; } viewParms;
} backEnd;
static struct {
    int numVertexes, fogNum; qboolean csmCasterPass; shader_t *shader;
    struct { color4ub_t colors[N]; } svars; color4ub_t vertexColors[N]; vec4_t xyz[N], normal[N];
} tess;
static int temporaryBlocks;
static void *TempAlloc(int bytes) { ++temporaryBlocks; return malloc(bytes); }
static void TempFree(void *block) { --temporaryBlocks; free(block); }
static struct { void *(*Hunk_AllocateTempMemory)(int); void (*Hunk_FreeTempMemory)(void *); } ri{TempAlloc, TempFree};
static void RB_EndSurface(void) {}
static int diffuseCalls, otherCalls;
static void RB_CalcDiffuseColor(unsigned char *colors) { ++diffuseCalls; memset(colors, 0x11, tess.numVertexes * 4); }
#define STUB(name) template<class... A> static void name(A...) { ++otherCalls; }
STUB(RB_MOHAACalcLightingColor) STUB(RB_CalcWaveColor) STUB(RB_CalcColorFromOneMinusEntity)
STUB(RB_AliceCalcGlobalColor) STUB(RB_AliceCalcColorWave) STUB(RB_MOHAACalcDotColor) STUB(RB_MOHAACalcCoordColors)
STUB(RB_CalcWaveAlpha) STUB(RB_AliceCalcSpecularAlpha) STUB(RB_CalcSpecularAlpha) STUB(RB_AliceCalcDotAlpha)
STUB(RB_AliceCalcGlobalAlpha) STUB(RB_AliceCalcSkyAlpha) STUB(RB_MOHAACalcDistFadeAlpha)
STUB(RB_CalcAlphaFromOneMinusEntity) STUB(RB_CalcModulateColorsByFog) STUB(RB_CalcModulateAlphasByFog)
STUB(RB_CalcModulateRGBAsByFog)
template<class... A> static byte R_RTCWNormalZAlpha(A...) { return 0; }
template<class... A> static qboolean RB_Q3IHVSkyColors(A...) { return qfalse; }
#include "code/renderercommon/tr_stvef_alpha.h"
#include "code/renderercommon/tr_jka_entity.h"
'''
    source += function(calc, 'void RB_CalcColorFromEntity(')
    source += function(calc, 'void RB_CalcAlphaFromEntity(')
    source += function(shade, 'void R_ComputeColors(')
    if vulkan:
        source += r'''
static color4ub_t computed[N];
static const color4ub_t *Compute(const shaderStage_t &stage, int bundle = 0) {
    memset(computed, 0x5a, sizeof(computed)); R_ComputeColors(bundle, computed, &stage); return computed;
}
'''
    else:
        source += r'''
static const color4ub_t *Compute(const shaderStage_t &stage, int bundle = 0) {
    assert(bundle == 0);
    memset(tess.svars.colors, 0x5a, sizeof(tess.svars.colors)); R_ComputeColors(&stage); return tess.svars.colors;
}
'''
    source += r'''
static void Expect(const color4ub_t *colors, int r, int g, int b, int a) {
    for (int i = 0; i < tess.numVertexes; ++i)
        assert(colors[i].rgba[0] == r && colors[i].rgba[1] == g && colors[i].rgba[2] == b && colors[i].rgba[3] == a);
}
static shaderStage_t Stage(colorGen_t rgbGen, alphaGen_t alphaGen) {
    shaderStage_t stage{}; GEN(stage).rgbGen = rgbGen; GEN(stage).alphaGen = alphaGen;
    GEN(stage).adjustColorsForFog = ACFF_NONE; GEN(stage).constantColor.rgba[3] = 99;
    return stage;
}
static int ids[16];
static drawSurf_t Surf(int entity, int id) {
    drawSurf_t surface{}; surface.sort = (unsigned)entity << QSORT_REFENTITYNUM_SHIFT; surface.surface = &ids[id];
    return surface;
}
int main() {
    trRefEntity_t entity{}, entities[4]{};
    entity.e.shader.rgba[0] = 10; entity.e.shader.rgba[1] = 20; entity.e.shader.rgba[2] = 30; entity.e.shader.rgba[3] = 40;
    tess.numVertexes = 4; tr.identityLight = 1; tr.identityLightByte = 255;
    for (int i = 0; i < N; ++i) { tess.vertexColors[i].rgba[0] = 1; tess.vertexColors[i].rgba[1] = 2;
        tess.vertexColors[i].rgba[2] = 3; tess.vertexColors[i].rgba[3] = 66; }
    backEnd.currentEntity = &entity;

    /* Retail's forced state (0x65), shared with Elite Force's pipeline. */
    static_assert(JKA_FORCED_ALPHA_STATE == 0x65, "source-alpha blending and nothing else");
    assert(!R_JKAEntityOverrides(nullptr) && !R_JKAEntityOverrides(&entity));

    /* Untinted, a stage keeps its own source: diffuse light, vertex alpha. */
    auto lit = Stage(CGEN_LIGHTING_DIFFUSE, AGEN_IDENTITY);
    Expect(Compute(lit), 0x11, 0x11, 0x11, 255); assert(diffuseCalls == 1);
    auto vertex = Stage(CGEN_VERTEX, AGEN_IDENTITY);
    Expect(Compute(vertex), 1, 2, 3, 66);

    /* Tinted, every stage takes the entity colour (0x4ae689) and its source
     * is never computed; an identity alpha still writes 255 (0x4ae70a), even
     * for a vertex stage under identity light, whose own alpha it keeps. */
    entity.e.jka.rgbTint = qtrue;
    assert(R_JKAEntityOverrides(&entity) && RB_JKAStageTint());
    Expect(Compute(lit), 10, 20, 30, 255); assert(diffuseCalls == 1);
    Expect(Compute(vertex), 10, 20, 30, 255);
    Expect(Compute(Stage(CGEN_IDENTITY, AGEN_IDENTITY)), 10, 20, 30, 255);
    Expect(Compute(Stage(CGEN_CONST, AGEN_IDENTITY)), 10, 20, 30, 255);
    /* The stage's own alpha source is kept: the entity's, none (the entity
     * colour's own fourth byte), the vertex's, or its constant. */
    Expect(Compute(Stage(CGEN_IDENTITY, AGEN_ENTITY)), 10, 20, 30, 40);
    Expect(Compute(Stage(CGEN_VERTEX, AGEN_SKIP)), 10, 20, 30, 40);
    Expect(Compute(Stage(CGEN_EXACT_VERTEX, AGEN_VERTEX)), 10, 20, 30, 66);
    Expect(Compute(Stage(CGEN_LIGHTING_DIFFUSE, AGEN_CONST)), 10, 20, 30, 99);
    assert(RB_JKAStageRGBGen(CGEN_FOG, 0) == CGEN_ENTITY && RB_JKAStageRGBGen(CGEN_FOG, 1) == CGEN_FOG);
'''
    if vulkan:
        source += r'''
    /* A collapsed stage's second bundle keeps its own colour. */
    shaderStage_t collapsed = Stage(CGEN_LIGHTING_DIFFUSE, AGEN_IDENTITY);
    collapsed.bundle[1].rgbGen = CGEN_IDENTITY; collapsed.bundle[1].alphaGen = AGEN_IDENTITY;
    collapsed.bundle[1].adjustColorsForFog = ACFF_NONE;
    Expect(Compute(collapsed, 1), 255, 255, 255, 255);
    Expect(Compute(collapsed, 0), 10, 20, 30, 255);
'''
    source += r'''
    /* Shadow-map passes and a missing entity take no tint. */
    tess.csmCasterPass = qtrue; Expect(Compute(vertex), 1, 2, 3, 66); assert(!RB_JKAStageTint());
    tess.csmCasterPass = qfalse; backEnd.viewParms.passFlags = VPF_DLIGHT_SHADOW;
    Expect(Compute(vertex), 1, 2, 3, 66);
    backEnd.viewParms.passFlags = 0; backEnd.currentEntity = nullptr; Expect(Compute(vertex), 1, 2, 3, 66);
    assert(!RB_JKAStageTint());
    backEnd.currentEntity = &entity; entity.e.jka.rgbTint = qfalse;

    /* Forced alpha: single-texture stages take the entity's alpha, whatever
     * it is, 0 and 255 included (0x4aed40); multitexture stages and
     * shadow-map passes never do. */
    shaderStage_t single{}, multi{};
    multi.bundle[1].image[0] = reinterpret_cast<image_t *>(&multi);
    int alpha = -1;
    assert(!RB_JKAStageAlpha(&single, &alpha) && alpha == -1);
    entity.e.jka.forceAlpha = qtrue;
    for (int value : {0, 1, 40, 254, 255}) {
        entity.e.shader.rgba[3] = (byte)value; alpha = -1;
        assert(RB_JKAStageAlpha(&single, &alpha) && alpha == value);
    }
    assert(!RB_JKAStageAlpha(&multi, &alpha));
    tess.csmCasterPass = qtrue; assert(!RB_JKAStageAlpha(&single, &alpha)); tess.csmCasterPass = qfalse;
    backEnd.viewParms.passFlags = VPF_DLIGHT_SHADOW; assert(!RB_JKAStageAlpha(&single, &alpha));
    backEnd.viewParms.passFlags = 0;
    backEnd.currentEntity = nullptr; assert(!RB_JKAStageAlpha(&single, &alpha));
    backEnd.currentEntity = &entity;
    /* It never changes a stage's colour source by itself. */
    Expect(Compute(vertex), 1, 2, 3, 66);

    /* The surface list draws a forced-alpha entity's surfaces after the
     * rest, keeping the order within both parts (0x48aba6). The world,
     * other entities and stale entity numbers stay in place. */
    tr.refdef.entities = entities; tr.refdef.num_entities = 3;
    const int world = REFENTITYNUM_WORLD;
    drawSurf_t list[9] = {Surf(world, 0), Surf(1, 1), Surf(0, 2), Surf(1, 3), Surf(2, 4), Surf(world, 5),
                          Surf(1, 6), Surf(3, 7), Surf(0, 8)};
    drawSurf_t copy[9]; memcpy(copy, list, sizeof(list));
    R_JKADeferForcedAlphaSurfaces(list, 9);
    assert(!memcmp(copy, list, sizeof(list)));    // no forced-alpha entity in the scene
    entities[1].e.jka.forceAlpha = qtrue; entities[2].e.jka.rgbTint = qtrue;
    entities[3].e.jka.forceAlpha = qtrue;    // a stale slot past this scene's entities
    R_JKADeferForcedAlphaSurfaces(list, 9);
    const int order[9] = {0, 2, 4, 5, 7, 8, 1, 3, 6};
    for (int i = 0; i < 9; ++i) assert(list[i].surface == &ids[order[i]]);
    assert(temporaryBlocks == 0);
    /* A list of only deferred surfaces, or none, is left as it is. */
    drawSurf_t only[2] = {Surf(1, 9), Surf(1, 10)};
    R_JKADeferForcedAlphaSurfaces(only, 2); assert(only[0].surface == &ids[9] && only[1].surface == &ids[10]);
    R_JKADeferForcedAlphaSurfaces(only, 0);
    assert(temporaryBlocks == 0);
    puts("ok");
    return 0;
}
'''
    return source


def pipeline_fixture(renderer):
    vk = read(f'code/{renderer}/vk.h')
    local = read(f'code/{renderer}/tr_local.h')
    shade = read(f'code/{renderer}/tr_shade.c')
    types = re.search(r'typedef enum\s*\{[^{}]*TYPE_SIGNLE_TEXTURE[^{}]*\}\s*Vk_Shader_Type;', vk)[0]
    env = re.search(r'typedef enum\s*\{\s*GL_NEAREST,[^{}]*\}\s*\w+;', local)[0]
    names = re.findall(r'^\s*(TYPE_\w+)', types, re.M)
    expected = []
    for name in names:
        match = re.fullmatch(r'TYPE_(SIGNLE_TEXTURE|MULTI_TEXTURE_MUL2|MULTI_TEXTURE_ADD2)_(IDENTITY|FIXED_COLOR|ENT_COLOR)(_ENV)?', name)
        if not match:
            expected.append((name, 'TYPE_BAD_UNCHANGED', 'TYPE_BAD_UNCHANGED'))
            continue
        base, suffix = match[1], match[3] or ''
        if base == 'MULTI_TEXTURE_ADD2':
            expected.append((name, f'TYPE_MULTI_TEXTURE_ADD2_1_1{suffix}', f'TYPE_MULTI_TEXTURE_ADD2{suffix}'))
        else:
            expected.append((name, f'TYPE_{base}{suffix}', f'TYPE_{base}{suffix}'))
    remapped = sum(1 for e in expected if e[1] != 'TYPE_BAD_UNCHANGED')
    rows = ',\n'.join(f'    {{{n}, {a}, {b}}}' for n, a, b in expected)
    source = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
typedef unsigned char byte;
enum qboolean { qfalse, qtrue };
''' + types + '\n' + env + r'''
enum { TYPE_BAD_UNCHANGED = -1 };
struct Vk_Pipeline_Def {
    int shader_type; unsigned state_bits; int mirror, fog_stage, depth_fade, allow_discard;
    struct { byte rgb, alpha; } color;
};
struct shaderStage_t { int mtEnv; };
static Vk_Pipeline_Def table[8];
static uint32_t created;
static void vk_get_pipeline_def(uint32_t pipeline, Vk_Pipeline_Def *def) { *def = table[pipeline]; }
static uint32_t vk_find_pipeline_ext(uint32_t, const Vk_Pipeline_Def *def, qboolean) { table[1] = *def; ++created; return 1; }
''' + function(shade, 'static qboolean VK_JKATintPipeline(') + r'''
struct Row { int type, add, nonIdentity; };
static const Row rows[] = {
''' + rows + r'''
};
int main() {
    int count = 0;
    for (const Row &row : rows) for (int env : {GL_MODULATE, GL_ADD, GL_ADD_NONIDENTITY}) {
        const int want = env == GL_ADD_NONIDENTITY ? row.nonIdentity : row.add;
        shaderStage_t stage{env};
        table[0] = {}; table[0].shader_type = row.type; table[0].state_bits = 0x123; table[0].mirror = 1;
        table[0].fog_stage = 1; table[0].depth_fade = 1; table[0].allow_discard = 1;
        table[0].color.rgb = 7; table[0].color.alpha = 9;
        uint32_t tinted = 77; const uint32_t before = created;
        const bool remapped = VK_JKATintPipeline(0, &stage, &tinted);
        assert(remapped == (want != TYPE_BAD_UNCHANGED));
        if (!remapped) { assert(tinted == 77 && created == before); continue; }
        if (env == GL_MODULATE) ++count;
        /* the vertex-colour type, with everything else of the stage kept */
        const Vk_Pipeline_Def &out = table[1];
        assert(tinted == 1 && created == before + 1 && out.shader_type == want);
        assert(out.state_bits == 0x123 && out.mirror == 1 && out.fog_stage == 1);
        assert(out.depth_fade == 1 && out.allow_discard == 1 && out.color.rgb == 0 && out.color.alpha == 0);
    }
    assert(count == REMAPPED);
    puts("ok");
    return 0;
}
'''
    return source.replace('REMAPPED', str(remapped)), remapped


RT_CHECKS = r'''
int main() {
    /* An entity whose module colours or fades it is left to raster, which
     * draws it with its overrides; the same entity untouched is traced. */
    for ( int flag = 0; flag < 3; ++flag ) {
        trRefEntity_t *entity = Model( MOD_MESH );
        entity->e.jka.rgbTint = flag == 1 ? qtrue : qfalse;
        entity->e.jka.forceAlpha = flag == 2 ? qtrue : qfalse;
        if ( flag == 0 ) {
            assert( RT_EntityClass( entity ) == 0 );
            assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_MD3 ) == RT_SURFACE_TRACED );
        } else {
            assert( RT_EntityClass( entity ) < 0 );
            assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_MD3 ) == RT_SURFACE_RASTER );
        }
    }
    trRefEntity_t *brush = Model( MOD_BRUSH );
    brush->e.jka.rgbTint = qtrue;
    assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_FACE ) == RT_SURFACE_RASTER );
    puts( "ok" );
    return 0;
}
'''


class JKAEntityOverrideTests(unittest.TestCase):
    def test_adapter_carries_the_overrides_as_engine_fields(self):
        compile_run(self, ADAPTER, 'jka-override-adapter-')

    def test_color_source_alpha_and_surface_order_in_each_renderer(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                compile_run(self, color_fixture(renderer), 'jka-override-color-')

    def test_vulkan_tint_pipelines(self):
        for renderer in RENDERERS[1:]:
            with self.subTest(renderer=renderer):
                source, remapped = pipeline_fixture(renderer)
                # Single texture (identity, fixed and entity colour), and both
                # two-texture combines (identity and fixed colour), with and
                # without environment mapping.
                self.assertEqual(remapped, 14)
                compile_run(self, source, 'jka-override-vk-')

    def test_path_tracer_leaves_overridden_entities_to_raster(self):
        compile_run(self, rtx_surface_owner_tests.host() + RT_CHECKS, 'jka-override-rt-')

    def test_each_renderer_applies_them_where_retail_does(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                shade = read(f'code/{renderer}/tr_shade.c')
                backend = read(f'code/{renderer}/tr_backend.c')
                main = read(f'code/{renderer}/tr_main.c')
                for text in (shade, backend, main):
                    self.assertIn('#include "../renderercommon/tr_jka_entity.h"', text)
                colors = function(shade, 'void R_ComputeColors(')
                self.assertIn('const colorGen_t rgbGen = RB_JKAStageRGBGen(', colors)
                self.assertIn('switch ( rgbGen )', colors)
                self.assertIn('if ( ( rgbGen == CGEN_VERTEX && tr.identityLight != 1 ) ||', colors)
                # Retail never forces alpha on a multitexture stage.
                if renderer == 'renderer':
                    generic = function(shade, 'static void RB_IterateStagesGeneric(')
                    single = generic[generic.index('DrawMultitextured( input, stage );'):]
                    self.assertNotIn('RB_JKAStageAlpha', generic[:generic.index('DrawMultitextured( input, stage );')])
                    self.assertIn('if ( !forced && RB_JKAStageAlpha( pStage, &forcedAlpha ) ) {', single)
                    self.assertIn('forcedState = JKA_FORCED_ALPHA_STATE;', single)
                    self.assertIn('GL_State( forced ? forcedState : pStage->stateBits );', single)
                    multi = function(shade, 'static void DrawMultitextured(')
                    self.assertIn('R_ComputeColors( pStage );', multi)
                    self.assertNotIn('RB_JKAStageAlpha', multi)
                else:
                    generic = function(shade, 'static void RB_IterateStagesGeneric( const shaderCommands_t *input, qboolean fogCollapse )')
                    self.assertIn('RB_JKAStageTint()', generic)
                    self.assertIn('if ( !forced && RB_JKAStageAlpha( pStage, &forcedAlpha ) ) {', generic)
                    self.assertIn('forced = VK_STVEFForcedAlphaPipeline( stagePipeline, &forcedPipeline );', generic)
                    self.assertIn('tinted = VK_JKATintPipeline( stagePipeline, pStage, &forcedPipeline );', generic)
                    self.assertIn('if ( forced || tint ) {', generic)
                    self.assertIn('if ( forced || tinted ) {', generic)
                # The world buffer's precomputed colours never draw an overridden entity.
                body = function(backend, 'RB_RenderDrawSurfList( drawSurf_t *drawSurfs')
                loop = body[:body.index('// draw the contents of the last shader batch')]
                guard = loop.rindex('if ( tess.allowVBO && R_JKAEntityOverrides( backEnd.currentEntity ) ) {')
                self.assertLess(guard, loop.rindex('rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );'))
                self.assertGreater(guard, loop.rindex('if ( entityNum != oldEntityNum ) {'))
                # The deferral follows the sort and any portal views, and
                # precedes the list's submission.
                order = function(main, 'static void R_SortDrawSurfs( drawSurf_t *drawSurfs, int numDrawSurfs ) {')
                defer = order.index('R_JKADeferForcedAlphaSurfaces( drawSurfs, numDrawSurfs );')
                self.assertLess(order.index('R_RadixSort( drawSurfs, numDrawSurfs );'), defer)
                portal = 'R_MirrorViewBySurface' if renderer == 'rendererrtx' else 'R_RenderPortalViewQueue( &portalViews );'
                self.assertLess(order.index(portal), defer)
                self.assertLess(defer, order.rindex('R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );'))
        tracer = read('code/rendererrtx/rt/rt_backend.c')
        self.assertIn('if ( ent->e.jka.rgbTint || ent->e.jka.forceAlpha ) {',
                      function(tracer, 'static int RT_EntityClass( const trRefEntity_t *ent )'))

    def test_decal_clear_returns_zero_with_no_decal_served(self):
        """Retail 0x412c71 calls the renderer's decal clear and returns 0; Fn
        never receives an Academy decal (import 205 stays refused by name)."""
        calls = read('code/compat/jka_module_calls.def')
        self.assertIn('FNTECH3_JKA_MODULE_CALL(CGAME, IMPORT, CG_R_CLEARDECALS, 201)', calls)
        self.assertIn('FNTECH3_JKA_MODULE_CALL(CGAME, IMPORT, CG_R_ADDDECALTOSCENE, 205)', calls)
        host = read('code/client/client_jka_cgame_host.hpp')
        start = host.index('    case Call::CG_R_CLEARDECALS:')
        case = host[start:host.index('    case Call::', start + 1)]
        self.assertNotIn('CG_R_ADDDECALTOSCENE', host)
        source = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
enum class Call { CG_R_CLEARDECALS = 201, CG_R_ADDDECALTOSCENE = 205 };
static bool Dispatch(Call call, intptr_t &result) {
    switch (call) {
''' + case + r'''
    default: return false;
    }
}
int main() {
    intptr_t result = 5;
    assert(Dispatch(Call::CG_R_CLEARDECALS, result) && result == 0);
    result = 5;
    assert(!Dispatch(Call::CG_R_ADDDECALTOSCENE, result) && result == 5);
    puts("ok");
    return 0;
}
'''
        compile_run(self, source, 'jka-override-decals-')


if __name__ == '__main__':
    unittest.main()
