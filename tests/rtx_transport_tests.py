"""Native RT transport contracts and numerical checks of the actual shader math."""

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


def shader(name: str) -> str:
    return (SHADERS / name).read_text(encoding="utf-8")


def function(source: str, name: str) -> str:
    match = re.search(rf"float {name}\([^{{]+\)\n\{{", source)
    if match is None:
        raise AssertionError(f"missing scalar shader function {name}")
    start = match.start()
    cursor = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[start:cursor]


class RtxTransportTests(unittest.TestCase):
    def test_all_radiance_stages_agree_on_payload(self) -> None:
        payloads = []
        for name in ("rt_main.rgen", "rt_main.rchit", "rt_main.rahit", "rt_main.rmiss", "rt_shadow.rmiss"):
            source = shader(name)
            match = re.search(r"struct RtRadiancePayload \{([^}]+)\};", source)
            self.assertIsNotNone(match, name)
            assert match is not None
            payloads.append(re.findall(r"(\w+)\s+(\w+);", match.group(1)))
        for payload in payloads[1:]:
            self.assertEqual(payload, payloads[0])
        self.assertIn(("uint", "pathDepth"), payloads[0])
        self.assertIn(("vec3", "nextWeight"), payloads[0])
        self.assertIn("rayPayloadEXT RtRadiancePayload payloadShadow", shader("rt_main.rchit"))
        self.assertIn("rayPayloadInEXT RtRadiancePayload payloadShadow", shader("rt_shadow.rmiss"))

    def test_radiance_continuation_keeps_two_level_pipeline_contract(self) -> None:
        raygen = shader("rt_main.rgen")
        closest = shader("rt_main.rchit")
        # Nested radiance calls in closest-hit would exceed the host's declared
        # recursion depth. All scene continuations must return to raygen first.
        self.assertEqual(closest.count("traceRayEXT("), 1)
        self.assertIn("gl_RayFlagsSkipClosestHitShaderEXT", closest)
        self.assertIn("min(pc.indirectBounce, 4u)", raygen)
        self.assertIn("surface = trace_surface(surface.nextOrigin, surface.nextDirection, bounce + 1u)", raygen)
        self.assertIn("throughput *= surface.nextWeight", raygen)
        self.assertIn("primary.color = max(radiance", raygen)
        self.assertIn("payloadRadiance.nextWeight = vec3(0.0)", shader("rt_main.rmiss"))
        self.assertNotIn("gl_RayFlagsOpaqueEXT", raygen + closest)

    def test_secondary_models_do_not_replace_camera_overlays(self) -> None:
        any_hit = shader("rt_main.rahit")
        overlay = any_hit[any_hit.index("if ((materialFlags & RTX_RT_MATFLAG_RASTER_OVERLAY)"):]
        self.assertIn("if (!shadowRay)", overlay)
        self.assertIn("payloadRadiance.pathDepth == 0u", overlay)
        self.assertIn("(materialFlags & RTX_RT_MATFLAG_SECONDARY_VISIBLE) == 0u", overlay)
        self.assertLess(overlay.index("payloadRadiance.pathDepth"), overlay.index("ignoreIntersectionEXT;"))
        self.assertIn("RTX_RT_MATFLAG_MASKED", overlay)

    def test_authored_lightmap_layout_is_shared_and_descriptor_is_bounded(self) -> None:
        vertex_layouts = []
        for name in ("rt_main.rchit", "rt_main.rahit"):
            source = shader(name)
            match = re.search(r"struct RtPackedVertex \{([^}]+)\};", source)
            self.assertIsNotNone(match)
            assert match is not None
            vertex_layouts.append(re.findall(r"(\w+)\s+(\w+);", match.group(1)))
        self.assertEqual(vertex_layouts[0], vertex_layouts[1])
        self.assertEqual(vertex_layouts[0][-2:], [("uint", "materialIndex"), ("vec4", "lightmapData")])
        closest = shader("rt_main.rchit")
        baked = closest[closest.index("uint lightmapIndex = material.textureInfo.y;"):]
        self.assertLess(baked.index("lightmapIndex < RTX_RT_MAX_SCENE_TEXTURES"), baked.index("textureLod("))
        self.assertIn("uintBitsToFloat(material.textureInfo.w)", baked)
        self.assertNotIn("decode_albedo_sample", baked)

    def test_vertex_lightmap_is_an_alternative_baked_source(self) -> None:
        closest = shader("rt_main.rchit")
        baked = closest[closest.index("uint lightmapIndex = material.textureInfo.y;"):]
        self.assertIn("material.textureInfo.z & RTX_RT_LIGHTING_VERTEX_LIGHTMAP", baked)
        self.assertIn("bakedDiffuse = vertexColor", baked)
        self.assertIn("else if (lightmapIndex != RTX_RT_INVALID_TEXTURE_INDEX", baked)
        self.assertEqual(baked.count("Lo += albedo * (1.0 - metallic) * max(bakedDiffuse"), 1)
        self.assertLess(baked.index("!isnan(lightmapScale) && !isinf(lightmapScale)"),
                        baked.index("bakedDiffuse = vertexColor"))
        self.assertNotIn("albedo *= vertexColor", closest)

    def test_history_disocclusion_uses_previous_camera_depth(self) -> None:
        raygen = shader("rt_main.rgen")
        self.assertIn("currentHit == previousHit", raygen)
        self.assertIn("length(worldPos - u_temporal.prevCameraOriginTanHalfFovX.xyz)", raygen)
        self.assertIn("abs(historySample.w - expectedPreviousDepth)", raygen)
        self.assertNotIn("abs(historySample.w - currentDepth)", raygen)
        self.assertIn("project_prev_uv(u_temporal.prevCameraOriginTanHalfFovX.xyz + direction * 2048.0", raygen)
        self.assertIn("!any(isnan(historySample))", raygen)
        self.assertIn("RTX_RT_PAYLOAD_FLAG_DYNAMIC | RTX_RT_PAYLOAD_FLAG_REACTIVE", raygen)
        self.assertIn("primary.flags |= RTX_RT_PAYLOAD_FLAG_REACTIVE", raygen)

    def test_half_float_output_and_tone_mapping_are_finite(self) -> None:
        raygen = shader("rt_main.rgen")
        self.assertIn("finite_radiance(finalColor, 65504.0)", raygen)
        self.assertIn("finite_radiance(accumulatedColor, 65504.0)", raygen)
        self.assertIn("finite_radiance(color * max(exposure, 0.001), 1e16)", raygen)
        self.assertIn("finite_radiance(radiance + throughput * surface.color, 1e16)", raygen)

    def test_executed_shader_math_is_finite_normalized_and_handles_tir(self) -> None:
        compiler = next((path for name in ("clang++", "g++", "c++")
                         if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler for the shader math harness")
        source = shader("rt_main.rchit")
        # Compile the scalar GLSL functions verbatim: the harness supplies only
        # GLSL builtin names, so these checks cannot silently test a second copy.
        bodies = "\n".join(function(source, name) for name in (
            "native_ggx_distribution", "native_ggx_masking", "dielectric_fresnel"))
        bodies += "\n" + function(shader("rt_main.rgen"), "finite_radiance_channel")
        fixture = r'''
#include <cmath>
#include <cstdio>
#include <limits>
constexpr float PI = 3.14159265359f;
float max(float a, float b) { return a > b ? a : b; }
float min(float a, float b) { return a < b ? a : b; }
float clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
#define isnan std::isnan
''' + bodies + r'''
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); return 1; } } while (0)
int main() {
    CHECK(finite_radiance_channel(std::numeric_limits<float>::quiet_NaN(), 65504.0f) == 0.0f);
    CHECK(finite_radiance_channel(std::numeric_limits<float>::infinity(), 65504.0f) == 65504.0f);
    CHECK(finite_radiance_channel(-std::numeric_limits<float>::infinity(), 65504.0f) == 0.0f);
    CHECK(finite_radiance_channel(70000.0f, 65504.0f) == 65504.0f);
    CHECK(finite_radiance_channel(65504.0f, 65504.0f) == 65504.0f);
    CHECK(finite_radiance_channel(0.125f, 65504.0f) == 0.125f);
    CHECK(std::abs(dielectric_fresnel(1.0f, 1.0f / 1.5f) - 0.04f) < 1e-6f);
    CHECK(std::abs(dielectric_fresnel(1.0f, 1.5f) - 0.04f) < 1e-6f);
    CHECK(dielectric_fresnel(0.5f, 1.5f) == 1.0f);
    CHECK(dielectric_fresnel(0.01f, 1.0f / 1.5f) > 0.94f);
    for (int i = 1; i <= 100; i++) {
        float mu = i / 100.0f;
        CHECK(std::abs(dielectric_fresnel(mu, 1.0f)) < 1e-6f);
        float entered = std::sqrt(1.0f - (1.0f - mu * mu) / 2.25f);
        CHECK(std::abs(dielectric_fresnel(mu, 1.0f / 1.5f) - dielectric_fresnel(entered, 1.5f)) < 3e-4f);
        for (int j = 1; j <= 20; j++) {
            float roughness = max(j / 20.0f, 0.04f);
            float d = native_ggx_distribution(mu, roughness);
            float g = native_ggx_masking(mu, roughness);
            CHECK(std::isfinite(d) && d >= 0.0f);
            CHECK(std::isfinite(g) && g >= 0.0f && g <= 1.00001f);
        }
    }
    // A near-mirror peak must not be suppressed by a broad epsilon clamp.
    float peak = native_ggx_distribution(1.0f, 0.04f);
    CHECK(std::abs(peak * PI * std::pow(0.04f, 4.0f) - 1.0f) < 0.001f);
    // Integrate D(h) cos(theta) over the hemisphere, with logarithmic slope
    // quadrature resolving even the narrow near-mirror normal distribution.
    for (float roughness : {0.04f, 0.1f, 0.3f, 0.6f, 1.0f}) {
        double integral = 0.0;
        const double alpha2 = std::pow(roughness, 4.0);
        constexpr int count = 50000;
        for (int i = 0; i < count; i++) {
            double slope = std::exp(-12.0 + (i + 0.5) * 24.0 / count);
            double factor = 1.0 + alpha2 * slope * slope;
            double cosine = 1.0 / std::sqrt(factor);
            double jacobian = alpha2 * slope * slope / std::pow(factor, 1.5);
            integral += 2.0 * PI * native_ggx_distribution(float(cosine), roughness) * cosine * jacobian * 24.0 / count;
        }
        CHECK(std::abs(integral - 1.0) < 0.035);
    }
    // Unit-Fresnel white conductor reflects no more than its incident energy.
    for (float roughness : {0.3f, 0.6f, 1.0f}) {
        double energy = 0.0;
        constexpr int count = 30000;
        for (int i = 0; i < count; i++) {
            float mu = float((i + 0.5) / count);
            float halfCosine = std::sqrt(0.5f * (1.0f + mu));
            energy += 0.5 * PI * native_ggx_distribution(halfCosine, roughness) * native_ggx_masking(mu, roughness) / count;
        }
        CHECK(energy > 0.0 && energy <= 1.001);
    }
    return 0;
}
'''
        fixture = fixture.replace("#include <cstdio>", "#include <cstdio>\n#include <initializer_list>")
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="rtx-transport-", dir=scratch) as folder:
            path = Path(folder)
            cpp = path / "transport.cpp"
            executable = path / ("transport.exe" if os.name == "nt" else "transport")
            cpp.write_text(fixture, encoding="utf-8")
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror"]
            if os.name == "nt" and "clang" in Path(compiler).name:
                command.append("--target=i686-pc-windows-msvc")
            build = subprocess.run([*command, str(cpp), "-o", str(executable)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
