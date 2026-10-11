"""Execute the shader packing helpers and check their actual storage precision."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class RtxSignalPackingTests(unittest.TestCase):
    def test_depth_motion_and_half_precision_variance(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        helpers = (ROOT / 'code/rendererrtx/shaders/rt_signal_packing.glsl').read_text()
        source = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
using std::sqrt;
static float max(float a, float b) { return a > b ? a : b; }
struct vec2 { float x,y; vec2(float a,float b):x(a),y(b){} };
struct vec4 {
    float x,y,z,w;
    vec4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){}
    vec4(vec2 a,float b,float c):x(a.x),y(a.y),z(b),w(c){}
};
struct uvec2 { uint32_t x,y; uvec2(uint32_t a,uint32_t b):x(a),y(b){} };
struct uvec4 { uint32_t x,y,z,w; uvec4(uint32_t a,uint32_t b,uint32_t c,uint32_t d):x(a),y(b),z(c),w(d){} };
static uint32_t floatBitsToUint(float x) { uint32_t u; std::memcpy(&u,&x,4); return u; }
static float uintBitsToFloat(uint32_t u) { float x; std::memcpy(&x,&u,4); return x; }
static uint32_t packSnorm2x16(vec2 v) {
    auto p = [](float x) { return uint16_t(int16_t(std::round(std::fmax(-1.0f,std::fmin(1.0f,x))*32767))); };
    return uint32_t(p(v.x)) | (uint32_t(p(v.y)) << 16);
}
static vec2 unpackSnorm2x16(uint32_t p) {
    return vec2(max(float(int16_t(p&65535))/32767,-1),max(float(int16_t(p>>16))/32767,-1));
}
// Round a nonnegative finite scalar to binary16's ten fraction bits,
// including its subnormals; test inputs stay below its finite maximum.
static float half(float x) {
    if (x == 0) return 0;
    int e; std::frexp(x,&e);
    float unit = std::ldexp(1.0f, e < -13 ? -24 : e-11);
    return std::round(x/unit)*unit;
}
''' + helpers + r'''
int main() {
    for (float depth : {0.0f,0.0001f,2.0f,65504.0f,1234567.0f,1e30f}) {
        for (int j=-10000;j<=10000;j++) {
            vec2 v(j/10000.0f, -j/10000.0f);
            auto p=rt_pack_geometry(v,depth);
            auto u=rt_unpack_geometry(uvec2(p.x,p.y));
            assert(floatBitsToUint(u.z)==floatBitsToUint(depth));
            // At 4K, motion's error is below 0.06 pixel per component.
            assert(std::abs(u.x-v.x)*3840 < 0.06f);
            assert(std::abs(u.y-v.y)*3840 < 0.06f);
            assert(p.z==0 && p.w==0);
        }
    }
    // Means up to the radiance limit and small variances must not suffer
    // the catastrophic cancellation of separately rounded squared moments.
    for (float mean : {0.0f,0.01f,1.0f,10.0f,64.0f}) {
        for (float variance : {0.01f,0.25f,1.0f,100.0f}) {
            vec4 m(mean,mean*mean+variance,mean,mean*mean+variance);
            auto packed=rt_pack_moments(m);
            auto restored=rt_unpack_moments(vec4(half(packed.x),half(packed.y),half(packed.z),half(packed.w)));
            float measured=restored.y-restored.x*restored.x;
            float represented=m.y-m.x*m.x;
            assert(std::abs(measured-represented) <= represented*0.03f + 0.0005f);
            assert(std::abs(restored.x-mean) <= max(mean*0.0005f,1e-7f));
            assert(std::isfinite(restored.y) && measured >= 0);
        }
    }
    auto bad=rt_pack_moments(vec4(1,0.999f,2,3.99f));
    assert(bad.y==0 && bad.w==0);
    std::puts("depth, reprojection and variance precision: PASS");
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-packing-') as temp:
            path = Path(temp)
            cpp = path / 'packing.cpp'
            exe = path / ('packing.exe' if os.name == 'nt' else 'packing')
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                           cwd=path, check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], cwd=path, check=True, capture_output=True, text=True)
            self.assertIn('PASS', result.stdout)

    def test_host_and_shader_formats_agree(self):
        backend = (ROOT / 'code/rendererrtx/rt/rt_backend.c').read_text()
        formats = re.findall(r'\{ (VK_FORMAT_\w+), "rt ([^"]+)" \}', backend)
        sizes = {'VK_FORMAT_R32_UINT': 4, 'VK_FORMAT_R32G32_UINT': 8,
                 'VK_FORMAT_R16G16B16A16_SFLOAT': 8}
        self.assertEqual(len(formats), 24)
        self.assertEqual(sum(sizes[f] for f, _ in formats) + 8, 192)
        common = (ROOT / 'code/rendererrtx/shaders/rt_denoise_common.glsl').read_text()
        self.assertIn('binding = 3, rg32ui) uniform uimage2D u_normalDepth', common)
        self.assertIn('binding = 4, rg32ui) uniform uimage2D u_motion', common)
        self.assertIn('binding = 8, rgba16f) uniform image2D u_histMoments', common)
        self.assertIn('motion.z > 0.0 ? 0.0 : 1.0', common)


if __name__ == '__main__':
    unittest.main()
