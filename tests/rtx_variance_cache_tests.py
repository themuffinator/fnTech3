"""Run the denoiser's shared-tile GLSL against a direct image convolution."""
from pathlib import Path
import re
import unittest

from mesh_tag_tests import function
from rtx_surface_owner_tests import compile_run

ROOT = Path(__file__).resolve().parents[1]


class RtxVarianceCacheTests(unittest.TestCase):
    def test_halo_partial_groups_and_both_ping_pong_images(self):
        shader = (ROOT / 'code/rendererrtx/shaders/rt_denoise_atrous.comp').read_text()
        cache = function(shader, 'void cache_variance(')
        blur = function(shader, 'vec3 blurred_variance(')
        declarations = shader[shader.index('const int RT_VARIANCE_TILE'):shader.index('void cache_variance(')]
        glsl = declarations + cache + blur
        glsl = glsl.replace('shared ', '').replace('.xy', '')
        glsl = re.sub(r'float\[2\]\(([^;]+)\)', r'{\1}', glsl)
        source = r'''
#include <cassert>
#include <cmath>
#include <algorithm>
#include <initializer_list>
using uint = unsigned;
using std::max;
float max(float a,double b) {return a>b?a:float(b);}
struct ivec2 {
    int x,y;
    explicit ivec2(int v=0):x(v),y(v){}
    ivec2(int a,int b):x(a),y(b){}
};
ivec2 operator+(ivec2 a,ivec2 b) {return {a.x+b.x,a.y+b.y};}
ivec2 operator-(ivec2 a,ivec2 b) {return {a.x-b.x,a.y-b.y};}
ivec2 operator*(ivec2 a,int b) {return {a.x*b,a.y*b};}
struct vec3 {
    float x,y,z;
    explicit vec3(float v=0):x(v),y(v),z(v){}
    vec3(float a,float b,float c):x(a),y(b),z(c){}
    vec3 &operator+=(vec3 b) {x+=b.x;y+=b.y;z+=b.z;return *this;}
};
vec3 operator*(vec3 a,float b) {return {a.x*b,a.y*b,a.z*b};}
vec3 operator/(vec3 a,double b) {return {float(a.x/b),float(a.y/b),float(a.z/b)};}
struct Pixel {float a;};
ivec2 size,gl_WorkGroupID,gl_LocalInvocationID;
uint gl_LocalInvocationIndex;
int reads,barriers;
int u_filterDiffuse[2]={0,3},u_filterSpecular[2]={1,4},u_filterIndirect[2]={2,5};
bool in_trace(ivec2 p) {return p.x>=0 && p.y>=0 && p.x<size.x && p.y<size.y;}
float variance(int image,ivec2 p) {
    return float(((p.x*17+p.y*31+image*43) % 101)*(image+1)) / 8;
}
Pixel imageLoad(int image,ivec2 p) {
    assert(in_trace(p)); ++reads; return {variance(image,p)};
}
void barrier() {++barriers;}
''' + glsl + r'''
int main() {
    for (ivec2 dimensions : {ivec2(1,1),ivec2(7,9),ivec2(9,7),ivec2(17,13),ivec2(32,24)}) {
        size=dimensions;
        for (uint src=0;src<2;++src) {
            for(int gy=0;gy<size.y;gy+=8) for(int gx=0;gx<size.x;gx+=8) {
                gl_WorkGroupID={gx/8,gy/8}; reads=barriers=0;
                // Poison shared memory to detect missing halo loads. Simulate
                // the whole workgroup before resuming beyond its barrier.
                for(auto &v:s_variance) v=vec3(NAN);
                for(uint lane=0;lane<64;++lane) {
                    gl_LocalInvocationIndex=lane; cache_variance(src);
                }
                assert(barriers==64 && reads<=300);
                const int cachedReads=reads;
                for(int ly=0;ly<8;++ly) for(int lx=0;lx<8;++lx) {
                    ivec2 p(gx+lx,gy+ly);
                    if(!in_trace(p)) continue;
                    gl_LocalInvocationID={lx,ly};
                    vec3 actual=blurred_variance(p);
                    float lanes[3]={actual.x,actual.y,actual.z};
                    for(int signal=0;signal<3;++signal) {
                        // Independent separable [1,2,1] kernel and edge
                        // normalization, with distinct values in all images.
                        double expected=0,weight=0;
                        for(int y=max(0,p.y-1);y<=std::min(size.y-1,p.y+1);++y)
                            for(int x=max(0,p.x-1);x<=std::min(size.x-1,p.x+1);++x) {
                                int w=(x==p.x?2:1)*(y==p.y?2:1);
                                expected+=w*variance(int(src)*3+signal,{x,y}); weight+=w;
                            }
                        assert(std::abs(lanes[signal]-expected/weight)<1e-5);
                    }
                }
                assert(reads==cachedReads); // convolution does no image reads
            }
        }
    }
}
'''
        compile_run(self, source)

    def test_every_invocation_reaches_the_shared_memory_barrier(self):
        shader = (ROOT / 'code/rendererrtx/shaders/rt_denoise_atrous.comp').read_text()
        main = function(shader, 'void main(')
        prefix = main[:main.index('cache_variance(src);')]
        self.assertNotIn('return', prefix)
        self.assertNotIn('if (', prefix)
        cache = function(shader, 'void cache_variance(')
        self.assertRegex(cache, r'\}\s*barrier\(\);\s*\}')


if __name__ == '__main__':
    unittest.main()
