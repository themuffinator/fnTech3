"""The path tracer adds an additive Ritual light to the world without the surface's reflectance.

Alice's and F.A.K.K.2's client games mark a dynamic light additive (its
type's bit 4). Retail adds such a light to the world and brush models GL_ONE
GL_ONE, whatever the surface (fakk2.exe 1.02 0x0047d32b..0x0047d33d, alice.exe
0x004935ab..0x004935bd), and the raster light passes light those surfaces from
white in place of their texture (tr_ritual_additive_light.h).

The tracer marks such a light RT_LIGHT_ADDITIVE (rt_backend.c
RT_UploadLights) and the frame RT_FRAME_ADDITIVE_LIGHTS. A world or
brush-model surface then takes the frame's additive lights in a second light
sample of their own, through the sampler's one call site, and the light it
returns as radiance the reflectance does not scale: the primary hit's joins
the denoised specular signal, a bounce's the path's radiance. Models keep the
reflectance, and a frame with no additive light samples as before. The
receiver masks are run here as C against the shader's own lines, and the
regenerated SPIR-V must carry the change.
"""
from pathlib import Path
import re
import unittest

import rtx_surface_owner_tests

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8").replace("\r\n", "\n")


def section(text, start, end="\n}\n"):
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


def glsl_constant(text, name):
    match = re.search(rf"const uint {name} = (?:1u << (\d+)|(\d+)u);", text)
    return 1 << int(match.group(1)) if match.group(1) else int(match.group(2))


def host_constant(text, name):
    match = re.search(rf"#define {name}\s+(?:\( 1u << (\d+) \)|(\d+)u)", text)
    return 1 << int(match.group(1)) if match.group(1) else int(match.group(2))


LIGHT_BITS = ("RT_LIGHT_NO_SURFACES", "RT_LIGHT_NO_MODELS", "RT_LIGHT_TRACED", "RT_LIGHT_SKY", "RT_LIGHT_DYNAMIC",
              "RT_LIGHT_ADDITIVE")

# rt_light_eval's receiver test and shade_direct's masks for its samples, as C
# (their GLSL lines are pinned by the tests below)
CHECKS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static bool Takes( uint32_t word, uint32_t exclude ) {
    return ( ( word & exclude ) | ( ~word & ( exclude >> RT_LIGHT_REQUIRE_SHIFT ) ) ) == 0u;
}
/* the pass's mask: one sample, or the map's lights then the additive ones */
static uint32_t PassMask( bool split, uint32_t pass, uint32_t exclude ) {
    return !split ? exclude :
        ( pass == 0u ? exclude | RT_LIGHT_ADDITIVE : exclude | ( RT_LIGHT_ADDITIVE << RT_LIGHT_REQUIRE_SHIFT ) );
}
/* the second sample draws among the dynamic lights alone (dynamicOnly) */
static bool Reaches( uint32_t word, bool dynamicOnly ) {
    return !dynamicOnly || ( word & RT_LIGHT_DYNAMIC ) != 0u;
}

int main() {
    const uint32_t lights[] = {
        RT_LIGHT_POINT, RT_LIGHT_SPOT, RT_LIGHT_POINT | RT_LIGHT_NO_SURFACES, RT_LIGHT_POINT | RT_LIGHT_NO_MODELS,
        RT_LIGHT_TRIANGLE | RT_LIGHT_TRACED, RT_LIGHT_TRIANGLE | RT_LIGHT_SKY,
        RT_LIGHT_DLIGHT | RT_LIGHT_DYNAMIC, RT_LIGHT_DLIGHT | RT_LIGHT_DYNAMIC | RT_LIGHT_ADDITIVE,
    };
    /* a world surface, a model, a world surface the light pass skips */
    const uint32_t receivers[] = { RT_LIGHT_NO_SURFACES, RT_LIGHT_NO_MODELS, RT_LIGHT_NO_SURFACES | RT_LIGHT_DYNAMIC };
    int additiveSeen = 0;
    for ( uint32_t exclude : receivers ) {
        for ( uint32_t word : lights ) {
            const bool single = Takes( word, PassMask( false, 0u, exclude ) );
            /* the one sample is the old test, word & exclude */
            assert( single == ( ( word & exclude ) == 0u ) );
            const bool first = Takes( word, PassMask( true, 0u, exclude ) ) && Reaches( word, false );
            const bool second = Takes( word, PassMask( true, 1u, exclude ) ) && Reaches( word, true );
            /* split, every light the one sample takes is taken exactly once:
             * an additive light in the second sample, any other in the first */
            assert( !( first && second ) );
            assert( ( first || second ) == single );
            assert( second == ( single && ( word & RT_LIGHT_ADDITIVE ) != 0u ) );
            additiveSeen += second;
        }
    }
    /* the light pass's receivers take the additive light; a skipped one not */
    assert( additiveSeen == 2 );
    /* without required bits, the test is the old one for every light word and
     * receiver mask the tracer forms */
    for ( uint32_t word = 0; word < ( 1u << RT_LIGHT_REQUIRE_SHIFT ); word += 7u ) {
        for ( uint32_t exclude = 0; exclude < 0x400u; exclude += 0x10u ) {
            assert( Takes( word, exclude ) == ( ( word & exclude ) == 0u ) );
        }
    }
    puts( "ok" );
    return 0;
}
'''


class RtxRitualAdditiveLightTests(unittest.TestCase):
    def test_constants_agree_and_stay_below_the_required_bits(self):
        common = read("code/rendererrtx/shaders/rt_common.glsl")
        gpu = read("code/rendererrtx/rt/rt_gpu.h")
        shift = glsl_constant(common, "RT_LIGHT_REQUIRE_SHIFT")
        self.assertEqual(shift, 16)
        type_mask = int(re.search(r"const uint RT_LIGHT_TYPE_MASK = (0x[0-9A-Fa-f]+)u;", common).group(1), 16)
        self.assertLess(type_mask, 1 << shift)
        for name in LIGHT_BITS:
            value = glsl_constant(common, name)
            self.assertLess(value, 1 << shift, name)
            self.assertEqual(host_constant(gpu, name), value, name)
        self.assertEqual(glsl_constant(common, "RT_LIGHT_ADDITIVE"), 1 << 9)
        self.assertEqual(host_constant(gpu, "RT_FRAME_ADDITIVE_LIGHTS"), glsl_constant(common, "RT_FRAME_ADDITIVE_LIGHTS"))
        frame_bits = [glsl_constant(common, name) for name in re.findall(r"const uint (RT_FRAME_\w+) =", common)]
        self.assertEqual(len(frame_bits), len(set(frame_bits)), "frame flags overlap")

    def test_the_receiver_masks_run_as_the_shader_writes_them(self):
        common = read("code/rendererrtx/shaders/rt_common.glsl")
        source = ""
        for name in ("RT_LIGHT_POINT", "RT_LIGHT_SPOT", "RT_LIGHT_TRIANGLE", "RT_LIGHT_DLIGHT", "RT_LIGHT_REQUIRE_SHIFT") \
                + LIGHT_BITS:
            source += f"static const uint32_t {name} = {glsl_constant(common, name)}u;\n"
        rtx_surface_owner_tests.compile_run(self, "#include <stdint.h>\n" + source + CHECKS)
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        evaluate = section(lighting, "bool rt_light_eval(")
        self.assertIn("if (((word & exclude) | (~word & (exclude >> RT_LIGHT_REQUIRE_SHIFT))) != 0u) {\n"
                      "\t\treturn false;", evaluate)
        direct = section(read("code/rendererrtx/shaders/rt_path.glsl"), "ShadeResult shade_direct(")
        self.assertIn("uint passExclude = !split ? exclude :\n"
                      "\t\t\t(pass == 0u ? exclude | RT_LIGHT_ADDITIVE : exclude | (RT_LIGHT_ADDITIVE << RT_LIGHT_REQUIRE_SHIFT));",
                      direct)
        self.assertIn("rt_sample_lights(P, N, Ng, candidates, passExclude, baked || pass == 1u, seed, L,", direct)

    def test_only_an_additive_ritual_light_is_marked(self):
        backend = read("code/rendererrtx/rt/rt_backend.c")
        upload = section(backend, "static qboolean RT_UploadLights( rtFrame_t *frame )")
        # the light side of the raster passes' predicate (RB_RitualAdditiveWorldLight)
        self.assertIn("const qboolean additiveLight = ( dl->additive && dl->ritualRadius > 0.0f ) ? qtrue : qfalse;",
                      upload)
        self.assertIn("if ( !dl || !dl->additive || !( dl->ritualRadius > 0.0f ) || !backEnd.currentEntity ) {",
                      read("code/renderercommon/tr_ritual_additive_light.h"))
        self.assertIn("light->color[3] = (float)( RT_LIGHT_DLIGHT | RT_LIGHT_DYNAMIC | "
                      "( additiveLight ? RT_LIGHT_ADDITIVE : 0u ) );", upload)
        self.assertIn("s_rt.numAdditiveLights = additive;", upload)
        # the static lights and RTCW's light materials carry no such bit
        self.assertEqual(upload.count("RT_LIGHT_ADDITIVE"), 1)
        self.assertNotIn("RT_LIGHT_ADDITIVE", read("code/rendererrtx/rt/rt_lights.c"))
        uniforms = section(backend, "static void RT_WriteFrameUniforms(")
        self.assertIn("( s_rt.numAdditiveLights ? RT_FRAME_ADDITIVE_LIGHTS : 0u );", uniforms)
        # only a Ritual client game's light has a Ritual radius
        scene = read("code/rendererrtx/tr_scene.c")
        self.assertIn("dl->ritualRadius = COM_GAME_ID_IS_RITUAL( ri.gameId ) ? intensity : 0.0f;", scene)
        self.assertGreaterEqual(scene.count("dl->ritualRadius = 0.0f;"), 2)

    def test_world_and_brush_surfaces_take_it_without_their_reflectance(self):
        geometry = read("code/rendererrtx/shaders/rt_geometry.glsl")
        self.assertIn("return !tri.dynamic || tri.v0.lightmapData.w > 0.5;", section(geometry, "bool rt_world_or_brush("))
        # a brush model's vertices carry that marker, a model's do not
        stamp = section(read("code/rendererrtx/rt/rt_backend.c"), "static void RT_StampEntityVertices(")
        self.assertIn("v->lightmapData[3] = 1.0f;", stamp)
        self.assertIn("v->lightmapData[3] = 0.0f;", stamp)
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        direct = section(path, "ShadeResult shade_direct(")
        self.assertIn("bool split = worldOrBrush && !oneSample && (pick & 1u) != 0u && (exclude & RT_LIGHT_DYNAMIC) == 0u &&\n"
                      "\t\t(u_frame.flags.x & RT_FRAME_ADDITIVE_LIGHTS) != 0u && u_frame.materialLights.z == 0u;", direct)
        self.assertIn("[[dont_unroll]] for (uint pass = 0u; pass < (split ? 2u : 1u); pass++) {", direct)
        self.assertIn("if (worldOrBrush && passChoice.index != ~0u &&\n"
                      "\t\t\t(uint(u_lights.lights[passChoice.index].color.w) & RT_LIGHT_ADDITIVE) != 0u) {\n"
                      "\t\t\tr.additive += E;\n"
                      "\t\t} else {\n"
                      "\t\t\tr.irradiance += E;", direct)
        # a reflection is weighed against the map's lights' sample
        self.assertIn("if (pass == 0u) {\n\t\t\tchoice = passChoice;", direct)
        # both hits say what they are; the primary's joins the specular
        # signal, a bounce's the radiance, neither scaled by the reflectance
        self.assertEqual(path.count("rt_world_or_brush(tri), seed);"), 2)
        self.assertIn("vec3 specular = (liquid ? vec3(0.0) : direct.specular) + direct.additive * (1.0 - liquidF);", path)
        self.assertIn("radiance += throughput * (sh.diffuse * irradiance + direct.additive + emission * u_frame.params0.y "
                      "+ lightSeen);", path)
        self.assertNotIn("sh.diffuse * direct.additive", path)
        self.assertNotIn("reflectance * direct.additive", path)
        # the receivers RTCW's light materials reach (the same predicate)
        self.assertIn("(material.flags & RT_MF_NODLIGHT) == 0u &&\n\t\t(!tri.dynamic || tri.v0.lightmapData.w > 0.5)) {",
                      path)

    def test_the_sampler_keeps_one_call_site(self):
        # Inlined twice, the sampler or its shadow traversal made the compute
        # trace too large for the 32-bit driver to compile (rt_path.glsl)
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        self.assertEqual(len(re.findall(r"\brt_sample_lights\(", path)), 1)
        self.assertEqual(len(re.findall(r"\brt_trace_shadow_mask\(", lighting)), 1)
        self.assertNotIn("rt_trace_shadow_mask(", path)

    def test_the_embedded_spirv_was_regenerated(self):
        data = read("code/rendererrtx/shaders/spirv/shader_data.c")
        for symbol in ("rt_trace_comp_spv", "rt_main_rgen_spv"):
            match = re.search(rf"const unsigned char {symbol}\[(\d+)\] = \{{(.*?)\}};", data, re.S)
            self.assertIsNotNone(match, symbol)
            spirv = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", match.group(2)))
            self.assertEqual(len(spirv), int(match.group(1)), symbol)
            # glslang keeps the source's names: the helper and the result's
            # new member are in the module the renderer loads
            self.assertIn(b"rt_world_or_brush(", spirv, symbol)
            self.assertIn(b"additive\x00", spirv, symbol)


if __name__ == "__main__":
    unittest.main()
