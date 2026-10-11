"""Execute the liquid shader's Fresnel and distance law; pin the transport boundary."""
from pathlib import Path
import unittest

from rtx_surface_owner_tests import compile_run
from rtx_transport_tests import function

ROOT = Path(__file__).resolve().parents[1]


class RtxLiquidTests(unittest.TestCase):
    def test_executed_optics(self):
        liquid = (ROOT / 'code/rendererrtx/shaders/rt_liquid.glsl').read_text()
        source = r'''
#include <cmath>
#include <cassert>
#include <algorithm>
static float max(float a, float b) { return std::max(a,b); }
static float clamp(float x,float a,float b) { return std::clamp(x,a,b); }
struct vec3 {
    float x,y,z;
    vec3(float a):x(a),y(a),z(a){}
    vec3(float a,float b,float c):x(a),y(b),z(c){}
};
static vec3 operator-(vec3 v) { return {-v.x,-v.y,-v.z}; }
static vec3 operator*(vec3 v,float f) { return {v.x*f,v.y*f,v.z*f}; }
static vec3 max(vec3 a,vec3 b) { return {max(a.x,b.x),max(a.y,b.y),max(a.z,b.z)}; }
static vec3 exp(vec3 a) { return {std::exp(a.x),std::exp(a.y),std::exp(a.z)}; }
'''
        source += function(liquid, 'rt_liquid_fresnel')
        source += function(liquid, 'rt_liquid_transmittance')
        source += r'''
int main() {
    const float ior=1.333f;
    const float f0=std::pow((ior-1)/(ior+1),2);
    assert(std::abs(rt_liquid_fresnel(1,1/ior)-f0)<1e-6f);
    assert(std::abs(rt_liquid_fresnel(1,ior)-f0)<1e-6f);
    float previous=f0;
    for(int i=1000;i>=0;--i) {
        float c=i/1000.f, F=rt_liquid_fresnel(c,1/ior);
        assert(std::isfinite(F)&&F>=0&&F<=1&&F+1e-6f>=previous);
        assert(rt_liquid_fresnel(c,1)==0);
        previous=F;
    }
    float critical=std::sqrt(1-1/(ior*ior));
    assert(rt_liquid_fresnel(critical-0.001f,ior)==1);
    assert(rt_liquid_fresnel(critical+0.001f,ior)<1);
    assert(rt_liquid_fresnel(0.5f,ior)==1);
    assert(rt_liquid_fresnel(0,1/ior)==1);
    // Clipped cosines and distance cannot amplify energy or produce NaNs.
    assert(rt_liquid_fresnel(2,ior)==rt_liquid_fresnel(1,ior));
    vec3 a(0.0035f,0.0015f,0.0008f);
    assert(rt_liquid_transmittance(a,0).x==1);
    assert(rt_liquid_transmittance(a,-100).x==1);
    assert(rt_liquid_transmittance(vec3(-1),100).x==1);
    vec3 t=rt_liquid_transmittance(a,100), t2=rt_liquid_transmittance(a,200);
    assert(t.x<t.y&&t.y<t.z&&t.z<1);
    assert(std::abs(t2.x-t.x*t.x)<1e-6f);
    assert(rt_liquid_transmittance(a,1e30f).x>=0);
}
'''
        compile_run(self, source)

    def test_interfaces_share_backends_and_do_not_spend_diffuse_depth(self):
        path = (ROOT / 'code/rendererrtx/shaders/rt_path.glsl').read_text()
        self.assertEqual(path.count(' * bounce_radiance('), 1)
        self.assertIn('step < depth + 8u && d < depth', path)
        self.assertIn('depth = bounces + 1u;', path)
        self.assertIn('if (!reflected) extinction = liq.entering ? liq.extinction : vec3(0.0);', path)
        self.assertIn('if (transmitting && !liq.entering)', path)
        self.assertIn('rt_liquid_transmittance(extinction, hit.hitT)', path)
        for name in ('rt_main.rgen', 'rt_trace.comp'):
            self.assertIn('#include "rt_path.glsl"', (ROOT / 'code/rendererrtx/shaders' / name).read_text())
        liquid = (ROOT / 'code/rendererrtx/shaders/rt_liquid.glsl').read_text()
        self.assertIn('refract(dir, liq.normal, eta)', liquid)
        self.assertNotIn('scene_color', liquid)
        self.assertNotIn('reflected_uv', liquid)


if __name__ == '__main__':
    unittest.main()
