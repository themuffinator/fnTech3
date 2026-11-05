"""Execute the still view's half-float history dither (rt_half_dither.glsl).

A still view averages thousands of samples into half-float history. Rounded
to the nearest half float, the small moves of the common samples vanish while
the rare large ones stay, and the average drifts away from the samples' own.
The fixture runs the shader's dither against a half float's own rounding.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class RtxHalfDitherTests(unittest.TestCase):
    def test_dithered_history_keeps_the_running_mean(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        shader = (ROOT / 'code/rendererrtx/shaders/rt_half_dither.glsl').read_text(encoding='utf-8')
        # The GLSL constructors and functions the dither uses, in C++.
        source = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
typedef uint32_t uint;
struct uvec3 {
    uint x, y, z;
    uvec3(uint a, uint b, uint c) : x(a), y(b), z(c) {}
};
static uvec3 operator>>(uvec3 a, uint s) { return uvec3(a.x >> s, a.y >> s, a.z >> s); }
struct vec3 {
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    explicit vec3(float v) : x(v), y(v), z(v) {}
    vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    explicit vec3(uvec3 u) : x(float(u.x)), y(float(u.y)), z(float(u.z)) {}
};
static vec3 operator*(vec3 a, float s) { return vec3(a.x * s, a.y * s, a.z * s); }
static vec3 operator*(vec3 a, vec3 b) { return vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
static vec3 operator+(vec3 a, vec3 b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
static vec3 operator/(vec3 a, vec3 b) { return vec3(a.x / b.x, a.y / b.y, a.z / b.z); }
static vec3 operator-(vec3 a, float s) { return vec3(a.x - s, a.y - s, a.z - s); }
static vec3 operator-(vec3 a, vec3 b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
static vec3 abs(vec3 a) { return vec3(std::fabs(a.x), std::fabs(a.y), std::fabs(a.z)); }
static vec3 max(vec3 a, vec3 b) { return vec3(std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z)); }
static vec3 floor(vec3 a) { return vec3(std::floor(a.x), std::floor(a.y), std::floor(a.z)); }
static vec3 log2(vec3 a) { return vec3(std::log2(a.x), std::log2(a.y), std::log2(a.z)); }
static vec3 exp2(vec3 a) { return vec3(std::exp2(a.x), std::exp2(a.y), std::exp2(a.z)); }
''' + shader + r'''
// IEEE half precision, as an imageStore converts: rounded to the nearest
// (ties to even) or toward zero, the two conversions Vulkan allows
static float half_step(float magnitude) {
    return magnitude < 6.103515625e-05f ? std::ldexp(1.0f, -24)
        : std::ldexp(1.0f, int(std::floor(std::log2(magnitude))) - 10);
}
static float half_round(float v, bool towardZero) {
    if (v == 0.0f) return 0.0f;
    const float magnitude = std::fabs(v);
    const float step = half_step(magnitude);
    const float q = towardZero ? std::floor(magnitude / step) : std::nearbyint(magnitude / step);
    return std::copysign(q * step, v);
}
static uint hash_u32(uint x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x;
}
int main() {
    // The dither leaves a value a half float exactly, within one step of it,
    // never below zero, as often above as its distance asks; zero stays zero.
    double drift = 0.0;
    for (uint i = 1; i < 200000; ++i) {
        const float v = std::ldexp(float(hash_u32(i) & 0xffffu) / 65536.0f, int(hash_u32(i * 3u) % 30u) - 24);
        const vec3 d = rt_half_dither(vec3(v), hash_u32(i * 7u));
        const float step = half_step(v);
        for (float x : {d.x, d.y, d.z}) {
            assert(x >= 0.0f && std::fabs(x - v) < step * 1.0001f);
            assert(half_round(x, false) == x && half_round(x, true) == x);
        }
        drift += (double(d.x) - v) / step;
    }
    assert(std::fabs(drift / 199999.0) < 0.01);
    const vec3 zero = rt_half_dither(vec3(0.0f), 12345u);
    assert(zero.x == 0.0f && zero.y == 0.0f && zero.z == 0.0f);

    // A still view's running mean over 2048 frames of a lit surface with a
    // soft shadow passing over it one sample in eight, of a dark one a rare
    // bright bounce reaches, and of a brighter one, each over 256 pixels and
    // under both of a store's roundings.
    struct Signal { float common, rare; uint every; } signals[] = {
        { 0.52f, 0.0f, 8u }, { 0.03f, 1.9f, 16u }, { 3.1f, 0.4f, 5u },
    };
    for (bool towardZero : {false, true}) for (const Signal &s : signals) {
        // against the exact mean of the very samples drawn
        double truthSum = 0.0, plainSum = 0.0, ditherSum = 0.0;
        for (uint pixel = 0; pixel < 256; ++pixel) {
            float plain = 0.0f, dithered = 0.0f;
            double exact = 0.0;
            for (uint frame = 1; frame <= 2048; ++frame) {
                const float sample = ( hash_u32(pixel * 7919u + frame) % s.every ) == 0 ? s.rare : s.common;
                const float alpha = 1.0f / float(frame);
                exact += (double(sample) - exact) / double(frame);
                plain = half_round(plain + (sample - plain) * alpha, towardZero);
                const float next = dithered + (sample - dithered) * alpha;
                dithered = half_round(rt_half_dither(vec3(next), hash_u32(pixel * 0x8da6b343u ^ frame * 0xcb1ab31fu)).x,
                    towardZero);
            }
            truthSum += exact;
            plainSum += plain;
            ditherSum += dithered;
        }
        const double truth = truthSum / 256.0;
        const double plainError = std::fabs(plainSum / 256.0 - truth) / truth;
        const double ditherError = std::fabs(ditherSum / 256.0 - truth) / truth;
        std::printf("%s, signal %.2f/%.2f: truth %.5f, stored %.5f (%.2f%%), dithered %.5f (%.3f%%)\n",
            towardZero ? "toward zero" : "to nearest", s.common, s.rare, truth, plainSum / 256.0, 100.0 * plainError,
            ditherSum / 256.0, 100.0 * ditherError);
        std::fflush(stdout);
        // the dithered average stays the samples', where a rounded one drifts
        assert(ditherError < 0.001);
        if (towardZero || s.every == 8u) assert(plainError > 0.01);
    }
    std::puts("half-float history dither: PASS");
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-half-dither-') as temp:
            path = Path(temp)
            cpp = path / 'dither.cpp'
            exe = path / ('dither.exe' if os.name == 'nt' else 'dither')
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1', str(cpp), '-o', str(exe)],
                           cwd=path, check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], cwd=path, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('PASS', result.stdout)


if __name__ == '__main__':
    unittest.main()
