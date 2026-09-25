"""Path tracer shader contracts, and numerical checks of the actual shader math.

The GLSL functions under test are compiled verbatim into a C++ harness that
supplies only the GLSL built-ins they use, so a check cannot pass against a
second, hand-copied implementation.
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "code/rendererrtx/shaders"
GPU_HEADER = ROOT / "code/rendererrtx/rt/rt_gpu.h"


def shader(name: str) -> str:
    return (SHADERS / name).read_text(encoding="utf-8")


def function(source: str, name: str) -> str:
    match = re.search(rf"^(?:float|vec4|vec3|vec2|uint|bool) {name}\([^{{]*\)\n\{{", source, re.M)
    if match is None:
        raise AssertionError(f"missing shader function {name}")
    cursor = source.index("{", match.start()) + 1
    depth = 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[match.start():cursor]


def struct_members(source: str, name: str) -> list[tuple[str, str]]:
    match = re.search(rf"struct {name} \{{(.*?)\n\}};", source, re.S)
    if match is None:
        raise AssertionError(f"missing struct {name}")
    body = re.sub(r"//[^\n]*", "", match.group(1))
    return re.findall(r"(\w+)\s+(\w+)(?:\[[^\]]+\])?\s*;", body)


def c_struct_members(source: str, name: str) -> list[tuple[str, str]]:
    end = source.find("\n} " + name + ";")
    if end < 0:
        raise AssertionError(f"missing C struct {name}")
    start = source.rfind("typedef struct {", 0, end) + len("typedef struct {")
    body = re.sub(r"//[^\n]*", "", source[start:end])
    return re.findall(r"(\w+)\s+(\w+)(?:\[[^\]]+\])*\s*;", body)


def glsl_bindings(source: str) -> dict[str, int]:
    """Binding number of each declaration, blocks included, keyed by instance name."""
    declarations = re.finditer(
        r"binding = (\d+)[^;{]*?(?:\{[^}]*\})?\s*\b(\w+)(?:\[[^\]]*\])?\s*;", source)
    return {match.group(2): int(match.group(1)) for match in declarations}


class RtxTransportTests(unittest.TestCase):
    def test_every_ray_tracing_stage_shares_the_common_definitions(self) -> None:
        for name in ("rt_main.rgen", "rt_main.rchit", "rt_main.rahit", "rt_main.rmiss", "rt_trace.comp"):
            with self.subTest(stage=name):
                self.assertIn('#include "rt_common.glsl"', shader(name))
        common = shader("rt_common.glsl")
        self.assertEqual(common.count("struct RtHitPayload"), 1)
        # radiance rays record the hit and shade in ray generation
        closest = shader("rt_main.rchit")
        self.assertIn("rayPayloadInEXT RtHitPayload payload", closest)
        self.assertIn("payload.hitT = gl_HitTEXT", closest)
        self.assertIn("payload.hitT = -1.0", shader("rt_main.rmiss"))
        self.assertIn("rayPayloadInEXT float shadowVisible", shader("rt_shadow.rmiss"))
        self.assertIn("shadowVisible = 1.0", shader("rt_shadow.rmiss"))

    def test_only_ray_generation_traces(self) -> None:
        # The pipeline declares a recursion depth of one: every ray, shadow
        # rays included, starts in ray generation.
        # The per-pixel work (rt_path.glsl) traces through its backend's
        # trace_radiance and rt_trace_shadow_mask: rt_trace_hw.glsl, which only
        # ray generation includes, or the compute backend's own traversal.
        for name in ("rt_main.rchit", "rt_main.rahit", "rt_main.rmiss", "rt_shadow.rmiss",
                     "rt_path.glsl", "rt_lighting.glsl", "rt_trace_sw.glsl", "rt_trace.comp"):
            with self.subTest(stage=name):
                self.assertNotIn("traceRayEXT", shader(name))
        self.assertIn("traceRayEXT", shader("rt_trace_hw.glsl"))
        self.assertIn('#include "rt_trace_hw.glsl"', shader("rt_main.rgen"))
        self.assertIn('#include "rt_path.glsl"', shader("rt_main.rgen"))
        compute = shader("rt_trace.comp")
        self.assertIn("#define RT_SOFTWARE_TRACE 1", compute)
        self.assertIn('#include "rt_trace_sw.glsl"', compute)
        self.assertIn('#include "rt_path.glsl"', compute)
        self.assertNotIn("rt_trace_hw.glsl", compute)
        for backend_file in ("rt_trace_hw.glsl", "rt_trace_sw.glsl"):
            with self.subTest(backend=backend_file):
                source = shader(backend_file)
                self.assertIn("bool trace_radiance(vec3 origin, vec3 dir, float tMin, float tMax, uint mask)", source)
                self.assertIn("bool trace_solid(vec3 origin, vec3 dir, float tMin, float tMax, uint mask)", source)
                self.assertIn("float rt_trace_shadow_mask(vec3 origin, vec3 dir, float tMax, uint mask, bool solid)",
                              source)
        backend = (ROOT / "code/rendererrtx/rt/rt_backend.c").read_text(encoding="utf-8")
        self.assertIn("pipelineInfo.maxPipelineRayRecursionDepth = 1;", backend)

    def test_any_hit_serves_radiance_and_shadow_rays_without_a_payload(self) -> None:
        any_hit = shader("rt_main.rahit")
        self.assertNotIn("rayPayloadInEXT", any_hit)
        self.assertIn("rt_hit_accepted(tri, bary, front, gl_WorldRayDirectionEXT)", any_hit)
        accepted = function(shader("rt_geometry.glsl"), "rt_hit_accepted")
        self.assertIn("rt_material_covered(m, h)", accepted)
        self.assertIn("RT_MF_CULL_FRONT", accepted)
        # the compute backend asks the same of each masked candidate
        software = shader("rt_trace_sw.glsl")
        self.assertIn("if (!rt_hit_accepted(shaded, bary, front, dir)) {", software)
        # shadow rays skip closest hit and stop at the first opaque hit
        self.assertIn("gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT",
                      shader("rt_trace_hw.glsl"))
        self.assertIn("return rt_sw_trace(origin, dir, 0.01, tMax, mask, true, solid, g_shadowBlocker) ? 0.0 : 1.0;",
                      software)
        self.assertIn("g_shadowMask", shader("rt_lighting.glsl"))
        # a solid ray takes the world as opaque, as the map compilers' light
        # rays did (the pipeline's opaque flag, the compute backend's skipped
        # alpha test), and keeps the alpha tests of the models, movers, weapon
        # and body they never saw (escape1's cell door stands in the torch's
        # light, which its compile placed at the world's origin)
        hardware = shader("rt_trace_hw.glsl")
        self.assertIn("const uint RT_MASK_MOVING = RT_MASK_DYNAMIC | RT_MASK_WEAPON | RT_MASK_BODY;",
                      shader("rt_common.glsl"))
        self.assertIn("if (masked && (!solid || dynamic)) {", software)
        shadow = function(hardware, "rt_trace_shadow_mask")
        self.assertIn("uint still = mask & ~RT_MASK_MOVING;", shadow)
        self.assertIn("gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT,\n"
                      "\t\t\tstill,", shadow)
        self.assertIn("gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,\n"
                      "\t\t\tmask & RT_MASK_MOVING,", shadow)
        solid = function(hardware, "trace_solid")
        self.assertIn("traceRayEXT(u_tlas, gl_RayFlagsOpaqueEXT, still,", solid)
        self.assertIn("traceRayEXT(u_tlas, gl_RayFlagsNoneEXT, mask & RT_MASK_MOVING,", solid)
        # ... reaching no further than the world's own hit
        self.assertIn("float reach = world.hitT >= 0.0 ? world.hitT : tMax;", solid)

    def test_glsl_and_c_structures_have_matching_members(self) -> None:
        common = shader("rt_common.glsl")
        header = GPU_HEADER.read_text(encoding="utf-8")
        pairs = (
            ("RtMaterial", "rtGpuMaterial_t"),
            ("RtStage", "rtGpuStage_t"),
            ("RtLight", "rtGpuLight_t"),
            ("RtFrameData", "rtFrameUniforms_t"),
        )
        for glsl, c in pairs:
            with self.subTest(struct=glsl):
                glsl_names = [n for _, n in struct_members(common, glsl)]
                c_names = [n for _, n in c_struct_members(header, c)]
                self.assertEqual(glsl_names, c_names)
        self.assertIn("sizeof( rtGpuMaterial_t ) == 96", header)
        self.assertIn("sizeof( rtGpuStage_t ) == 256", header)
        self.assertIn("sizeof( rtGpuLight_t ) == 64", header)

    def test_binding_numbers_agree_between_shaders_and_host(self) -> None:
        header = GPU_HEADER.read_text(encoding="utf-8")
        enum = re.search(r"typedef enum \{([^{}]*)\} rtBinding_t;", header)
        assert enum is not None
        # enumerators in order, an explicit value naming an earlier one
        index: dict[str, int] = {}
        value = -1
        for entry in re.sub(r"//[^\n]*", "", enum.group(1)).split(","):
            match = re.fullmatch(r"\s*RT_BINDING_(\w+)(?:\s*=\s*RT_BINDING_(\w+))?\s*", entry)
            if match is None:
                continue
            value = index[match.group(2)] if match.group(2) else value + 1
            index[match.group(1)] = value
        source = shader("rt_bindings.glsl")
        bindings = glsl_bindings(source)
        for glsl, name in (("u_tlas", "TLAS"), ("u_worldVertices", "WORLD_VERTICES"),
                           ("u_worldIndices", "WORLD_INDICES"), ("u_dynamicVertices", "DYNAMIC_VERTICES"),
                           ("u_dynamicIndices", "DYNAMIC_INDICES"), ("u_materials", "MATERIALS"),
                           ("u_stages", "STAGES"), ("u_lights", "LIGHTS"), ("u_lightGrid", "LIGHT_GRID"),
                           ("u_lightIndices", "LIGHT_INDICES"), ("u_textures", "TEXTURES")):
            with self.subTest(binding=glsl):
                self.assertEqual(bindings.get(glsl), index[name])
        # the frame uniforms are an anonymous block
        frame = re.search(r"binding = (\d+), std140\) uniform RtFrameBlock", source)
        assert frame is not None
        self.assertEqual(int(frame.group(1)), index["FRAME"])
        # the compute backend's BVHs: the world's nodes where the TLAS would be
        for glsl, name in (("u_worldBvh", "TLAS"), ("u_worldTris", "SW_WORLD_TRIS"),
                           ("u_dynamicBvh", "SW_DYNAMIC_NODES"), ("u_dynamicTris", "SW_DYNAMIC_TRIS")):
            with self.subTest(binding=glsl):
                self.assertEqual(bindings.get(glsl), index[name])
        self.assertEqual(index["SW_WORLD_TRIS"], index["COUNT"])
        raygen = glsl_bindings(shader("rt_path.glsl"))
        for glsl, name in (("u_outColor", "OUT_COLOR"), ("u_outSpecular", "OUT_SPECULAR"),
                           ("u_outReflectance", "OUT_REFLECTANCE"), ("u_outNormalDepth", "OUT_NORMAL_DEPTH"),
                           ("u_outEmission", "OUT_EMISSION"), ("u_outMotion", "OUT_MOTION"),
                           ("u_outInfo", "OUT_INFO")):
            with self.subTest(binding=glsl):
                self.assertEqual(raygen.get(glsl), index[name])

    def test_denoiser_rejects_mismatched_history_and_keeps_the_first_filter_as_history(self) -> None:
        temporal = shader("rt_denoise_temporal.comp")
        self.assertIn("(pinfo >> 8u) != (info >> 8u)", temporal)
        self.assertIn("abs(pnd.z - expectedPrevZ)", temporal)
        self.assertIn("dot(oct_decode(pnd.xy), n) < 0.85", temporal)
        self.assertIn("RT_DENOISE_FLAG_HISTORY_VALID", temporal)
        # moving surfaces keep a short history
        self.assertIn("(info & (RT_PIXEL_DYNAMIC | RT_PIXEL_WEAPON)) != 0u", temporal)
        atrous = shader("rt_denoise_atrous.comp")
        self.assertIn("iteration == 0u && (pc.state.w & RT_DENOISE_FLAG_WRITE_HISTORY) != 0u", atrous)
        self.assertIn("exp(-abs(lD - denoise_luminance(qD.rgb)) / (phiD + 1e-6))", atrous)
        composite = shader("rt_composite.comp")
        # the traced view may sit inset in the scene colour (the push
        # constants carry its corner)
        self.assertIn("vec4 result = texelFetch(u_sceneColor, px + ivec2(pc.view.xy), 0);", composite)
        self.assertIn("if ((info & RT_PIXEL_OWNED) != 0u)", composite)

    def test_executed_shader_math(self) -> None:
        compiler = next((path for name in ("clang++", "g++", "c++")
                         if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler for the shader math harness")
        common = shader("rt_common.glsl")
        material = shader("rt_material.glsl")
        constants = "\n".join(re.findall(r"^const (?:uint|float) \w+ = [^;]+;", common, re.M))
        bodies = "\n".join([
            function(material, "rt_wave_table"),
            function(material, "rt_blend_src"),
            function(material, "rt_blend_dst"),
            function(material, "rt_blend"),
            function(shader("rt_path.glsl"), "ggx_d"),
            function(shader("rt_path.glsl"), "smith_g1"),
            function(shader("rt_composite.comp"), "shoulder_irradiance"),
        ])
        # the renderer's own function tables, built by its own loop
        init = (ROOT / "code/rendererrtx/tr_init.c").read_text(encoding="utf-8")
        loop_start = init.index("for ( i = 0; i < FUNCTABLE_SIZE; i++ ) {", init.index("// init function tables"))
        cursor = init.index("{", loop_start) + 1
        depth = 1
        while depth:
            depth += (init[cursor] == "{") - (init[cursor] == "}")
            cursor += 1
        table_loop = init[loop_start:cursor]
        shared = (ROOT / "code/qcommon/q_shared.h").read_text(encoding="utf-8")
        macros = "\n".join(re.findall(r"^#define\s+(?:M_PI|DEG2RAD)\b[^\n]*", shared, re.M))
        epsilon = re.search(r"^#define EPSILON [^\n]+", init, re.M)
        assert epsilon is not None
        fixture = r'''
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
typedef unsigned uint;
using std::exp;
using std::floor;
using std::sin;
using std::sqrt;
using std::trunc;
struct vec3 { float r, g, b; };
static vec3 operator*(vec3 v, float s) { return { v.r * s, v.g * s, v.b * s }; }
struct vec4 {
    float r, g, b, a;
    vec4(double s = 0.0) : r(float(s)), g(float(s)), b(float(s)), a(float(s)) {}
    vec4(double x, double y, double z, double w) : r(float(x)), g(float(y)), b(float(z)), a(float(w)) {}
};
static vec4 operator*(vec4 x, vec4 y) { return vec4(x.r * y.r, x.g * y.g, x.b * y.b, x.a * y.a); }
static vec4 operator+(vec4 x, vec4 y) { return vec4(x.r + y.r, x.g + y.g, x.b + y.b, x.a + y.a); }
static vec4 operator-(vec4 x, vec4 y) { return vec4(x.r - y.r, x.g - y.g, x.b - y.b, x.a - y.a); }
static float max(float a, float b) { return a > b ? a : b; }
static float min(double a, double b) { return float(a < b ? a : b); }
''' + macros + "\n" + epsilon.group(0) + r'''
#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK ( FUNCTABLE_SIZE - 1 )
static struct {
    float sinTable[FUNCTABLE_SIZE], squareTable[FUNCTABLE_SIZE], triangleTable[FUNCTABLE_SIZE],
        sawToothTable[FUNCTABLE_SIZE], inverseSawToothTable[FUNCTABLE_SIZE];
} tr;
static void init_tables() {
    int i;
    ''' + table_loop + r'''
}
''' + constants + "\n" + bodies + r'''
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #c); return 1; } } while (0)
// WAVEVALUE's entry: (int64_t)(x * FUNCTABLE_SIZE) & FUNCTABLE_MASK
static float table_value(const float *table, float x) {
    return table[ (int64_t)( x * FUNCTABLE_SIZE ) & FUNCTABLE_MASK ];
}
// The stage lighting term L enters Quake III's blends linearly: C(L) = A + B L.
static bool affine(const uint src[], const uint dst[], const int lit[], const vec4 texel[], int count) {
    vec4 result[3];
    const float lights[3] = { 0.0f, 0.37f, 1.0f };
    for (int l = 0; l < 3; l++) {
        vec4 acc(0.0, 0.0, 0.0, 1.0);
        for (int s = 0; s < count; s++) {
            // lit: 1 scales the texel by L; 2 is a $lightmap stage, whose texel is L
            vec4 colour = lit[s] == 2 ? vec4(lights[l], lights[l], lights[l], 1.0) :
                lit[s] == 1 ? texel[s] * vec4(lights[l], lights[l], lights[l], 1.0) : texel[s];
            acc = rt_blend(src[s], dst[s], colour, acc);
        }
        result[l] = acc;
    }
    const float predicted = result[0].r + 0.37f * (result[2].r - result[0].r);
    return std::abs(result[1].r - predicted) < 1e-5f &&
        std::abs(result[1].g - (result[0].g + 0.37f * (result[2].g - result[0].g))) < 1e-5f;
}
int main() {
    init_tables();
    for (int i = -3000; i < 3000; i++) {
        // mid-entry phases, negative ones and a few table periods out
        const float x = (i + 0.3f) / 1024.0f;
        CHECK(std::abs(rt_wave_table(GF_SIN, x) - table_value(tr.sinTable, x)) < 2e-6f);
        CHECK(rt_wave_table(GF_SQUARE, x) == table_value(tr.squareTable, x));
        CHECK(std::abs(rt_wave_table(GF_TRIANGLE, x) - table_value(tr.triangleTable, x)) < 1e-6f);
        CHECK(std::abs(rt_wave_table(GF_SAWTOOTH, x) - table_value(tr.sawToothTable, x)) < 1e-6f);
        CHECK(std::abs(rt_wave_table(GF_INVERSE_SAWTOOTH, x) - table_value(tr.inverseSawToothTable, x)) < 1e-6f);
    }
    // blend factors in GLS numbering: src 2 one, 3 dst colour, 5 src alpha;
    // dst 1 zero, 2 one, 3 src colour, 6 one minus src alpha
    const vec4 tex(0.8, 0.5, 0.3, 0.6), glow(0.2, 0.4, 0.1, 1.0), env(0.9, 0.9, 1.0, 1.0);
    {   // $lightmap, then the texture as a filter
        const uint s[] = { 0, 3 }, d[] = { 0, 1 }; const int lit[] = { 2, 0 }; const vec4 t[] = { tex, tex };
        CHECK(affine(s, d, lit, t, 2));
    }
    {   // texture, $lightmap filter, additive glow
        const uint s[] = { 0, 3, 2 }, d[] = { 0, 1, 2 }; const int lit[] = { 0, 2, 0 };
        const vec4 t[] = { tex, tex, glow };
        CHECK(affine(s, d, lit, t, 3));
    }
    {   // $lightmap, then a 2x modulate (dst colour, src colour)
        const uint s[] = { 0, 3 }, d[] = { 0, 3 }; const int lit[] = { 2, 0 }; const vec4 t[] = { tex, tex };
        CHECK(affine(s, d, lit, t, 2));
    }
    {   // an unlit environment map under a lightingDiffuse texture, alpha blended
        const uint s[] = { 0, 5 }, d[] = { 0, 6 }; const int lit[] = { 0, 1 }; const vec4 t[] = { env, tex };
        CHECK(affine(s, d, lit, t, 2));
    }
    {   // a lit base and a lit additive environment map
        const uint s[] = { 0, 2 }, d[] = { 0, 2 }; const int lit[] = { 1, 1 }; const vec4 t[] = { tex, env };
        CHECK(affine(s, d, lit, t, 2));
    }
    // GGX: normalised over the hemisphere and a bounded Smith term
    for (float alpha : {0.05f, 0.1f, 0.3f, 0.6f, 1.0f}) {
        double integral = 0.0;
        const int count = 200000;
        for (int i = 0; i < count; i++) {
            double mu = (i + 0.5) / count;
            integral += 2.0 * PI * ggx_d(float(mu), alpha) * mu / count;
        }
        CHECK(std::abs(integral - 1.0) < 0.02);
        for (int i = 1; i <= 100; i++) {
            float g = smith_g1(i / 100.0f, alpha);
            CHECK(std::isfinite(g) && g > 0.0f && g <= 1.0001f);
        }
    }
    // the highlight shoulder: identity below the knee, continuous, monotonic,
    // bounded by the ceiling and hue-preserving
    float previous = 0.0f;
    for (int i = 0; i <= 4000; i++) {
        float e = i * 0.005f;
        vec3 out = shoulder_irradiance(vec3{ e, e * 0.5f, e * 0.25f }, 0.8f, 1.25f);
        if (e <= 0.8f) CHECK(std::abs(out.r - e) < 1e-6f);
        CHECK(out.r >= previous - 1e-6f && out.r <= 1.25f + 1e-5f);
        CHECK(std::abs(out.g - out.r * 0.5f) < 1e-5f && std::abs(out.b - out.r * 0.25f) < 1e-5f);
        previous = out.r;
    }
    return 0;
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="rtx-transport-", dir=scratch) as folder:
            path = Path(folder)
            cpp = path / "transport.cpp"
            executable = path / ("transport.exe" if os.name == "nt" else "transport")
            cpp.write_text(fixture, encoding="utf-8")
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                       "-Wno-unused-const-variable"]
            if os.name == "nt" and "clang" in Path(compiler).name:
                command.append("--target=i686-pc-windows-msvc")
            build = subprocess.run([*command, str(cpp), "-o", str(executable)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
