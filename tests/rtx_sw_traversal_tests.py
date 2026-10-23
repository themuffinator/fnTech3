"""Execute compute traversal GLSL against a brute-force triangle reference."""
from pathlib import Path
import re
import unittest

from rtx_surface_owner_tests import compile_run

ROOT = Path(__file__).resolve().parents[1]


class RtxSoftwareTraversalTests(unittest.TestCase):
    def test_closest_shadow_masked_solid_and_dynamic_hits(self):
        shader = (ROOT / 'code/rendererrtx/shaders/rt_trace_sw.glsl').read_text()
        shader = shader[shader.index('const uint RT_SW_STACK'):shader.index('// The nearest hit along the ray')]
        shader = re.sub(r'\b(?:inout|out) (\w+) (\w+)', r'\1 &\2', shader)
        shader = shader.replace('.xyz', '.xyz()')
        source = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>
using uint=uint32_t;
using std::min; using std::max; using std::abs;
float max(float a,double b) {return a>b?a:float(b);}
struct vec2 {
    float x,y;
    vec2(float v=0):x(v),y(v){}
    vec2(float a,float b):x(a),y(b){}
};
vec2 clamp(vec2 v,vec2 lo,vec2 hi) {return {min(max(v.x,lo.x),hi.x),min(max(v.y,lo.y),hi.y)};}
struct vec3 {
    float x,y,z;
    explicit vec3(float v=0):x(v),y(v),z(v){}
    vec3(float a,float b,float c):x(a),y(b),z(c){}
};
vec3 operator-(vec3 a,vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
vec3 operator*(vec3 a,vec3 b) {return {a.x*b.x,a.y*b.y,a.z*b.z};}
vec3 operator/(double a,vec3 b) {return {float(a/b.x),float(a/b.y),float(a/b.z)};}
vec3 min(vec3 a,vec3 b) {return {min(a.x,b.x),min(a.y,b.y),min(a.z,b.z)};}
vec3 max(vec3 a,vec3 b) {return {max(a.x,b.x),max(a.y,b.y),max(a.z,b.z)};}
vec3 cross(vec3 a,vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(vec3 a,vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
struct vec4 {
    float x,y,z,w;
    vec4(vec3 v=vec3(),float a=0):x(v.x),y(v.y),z(v.z),w(a){}
    vec3 xyz() const {return {x,y,z};}
};
struct uvec2 {uint x,y; uvec2(uint a=0,uint b=0):x(a),y(b){}};
struct uvec4 {uint x,y,z,w;};
struct RtBvhNode {vec4 bmin,bmax;};
struct RtBvhTri {vec4 v0,v1,v2;};
struct RtHitPayload {float hitT; uint customIndex,primitive,geometry; vec2 bary;};
struct RtTriangle {uint cls,local;};
constexpr uint RT_SW_MAX_INSTANCES=8u,RT_HIT_FRONT_FACING=0x80000000u;
struct {uvec4 swInfo; uvec4 swInstances[16];} u_frame;
template<class T> struct Buffer : std::vector<T> {
    const T &operator[](size_t i) const {return this->at(i);}
    T &operator[](size_t i) {return this->at(i);}
};
struct {Buffer<RtBvhNode> nodes;} u_worldBvh,u_dynamicBvh;
struct {Buffer<RtBvhTri> tris;} u_worldTris,u_dynamicTris;
uint floatBitsToUint(float x) {uint y;std::memcpy(&y,&x,4);return y;}
float uintBitsToFloat(uint x) {float y;std::memcpy(&y,&x,4);return y;}
RtTriangle rt_fetch_triangle(uint cls,uint geometry,uint local) {assert(geometry==1);return {cls>>20,local};}
bool rt_casts_shadow(RtTriangle t) {return (t.local+t.cls)%5!=0;}
bool rt_hit_accepted(RtTriangle t,vec2,bool front,vec3) {return (t.local+2*t.cls)%3!=0 && (front || t.local%2==0);}
''' + shader + r'''
uint seed=7129;
float random01() {seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216;}
// The fixture builds an independent balanced hierarchy over planar triangles.
// A nonzero base offset in both buffers checks per-instance addressing.
void build(Buffer<RtBvhNode> &nodes,const Buffer<RtBvhTri> &tris,uint base,uint triBase,uint n,uint first,uint count) {
    vec3 lo(1e9f),hi(-1e9f);
    for(uint i=first;i<first+count;++i) for(vec3 v:{tris[triBase+i].v0.xyz(),tris[triBase+i].v1.xyz(),tris[triBase+i].v2.xyz()}) {
        lo=min(lo,v);hi=max(hi,v);
    }
    nodes[base+n]={vec4(lo),vec4(hi)};
    if(count==1) {
        nodes[base+n].bmin.w=uintBitsToFloat(first);
        nodes[base+n].bmax.w=uintBitsToFloat(count);
    } else {
        uint child=uint(nodes.size())-base;
        nodes.resize(nodes.size()+2);
        nodes[base+n].bmin.w=uintBitsToFloat(child);
        build(nodes,tris,base,triBase,child,first,count/2);
        build(nodes,tris,base,triBase,child+1,first+count/2,count-count/2);
    }
}
// Double-precision plane intersection and 2D containment are independent of
// the shader's Moller-Trumbore test. Fixtures avoid shared-edge ambiguity.
bool reference(const RtBvhTri &tri,vec3 o,vec3 d,double lo,double hi,double &t,bool &front) {
    if(d.z==0) return false;
    t=(double(tri.v0.z)-o.z)/d.z;
    if(!(t>lo && t<hi)) return false;
    double x=o.x+t*d.x,y=o.y+t*d.y;
    vec3 a=tri.v0.xyz(),b=tri.v1.xyz(),c=tri.v2.xyz();
    double area=(double(b.x)-a.x)*(double(c.y)-a.y)-(double(b.y)-a.y)*(double(c.x)-a.x);
    double u=((x-a.x)*(c.y-a.y)-(y-a.y)*(c.x-a.x))/area;
    double v=((b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x))/area;
    front=area*d.z>0;
    return u>=-1e-6 && v>=-1e-6 && u+v<=1+1e-6;
}
int main() {
    u_frame.swInfo.x=2;
    for(uint cls=0;cls<2;++cls) {
        auto &tris=cls?u_dynamicTris.tris:u_worldTris.tris;
        auto &nodes=cls?u_dynamicBvh.nodes:u_worldBvh.nodes;
        tris.resize(3);nodes.resize(6);
        u_frame.swInstances[cls*2]={5,3,cls,1u<<cls};
        u_frame.swInstances[cls*2+1]={100,164,cls,0};
        for(uint i=0;i<128;++i) {
            float x=8*random01()-4,y=8*random01()-4,z=1+i*0.23f+cls*0.07f;
            vec3 a(x-1,y-1,z),b(x+1,y-1,z),c(x,y+1,z);
            if(i%2) std::swap(b,c);
            tris.push_back({vec4(a,uintBitsToFloat(100+i)),vec4(b),vec4(c)});
        }
        build(nodes,tris,5,3,0,0,128);
    }
    int hits=0,misses=0;
    for(int ray=0;ray<2000;++ray) {
        vec3 o(10*random01()-5,10*random01()-5,ray%2?40.0f:0.0f);
        vec3 d(0.1f*(random01()-0.5f),0.1f*(random01()-0.5f),ray%2?-1.0f:1.0f);
        if(ray%7==0) d.x=0; if(ray%11==0) d.y=0; if(ray%13==0) d.z=0;
        float lo=ray%3?0.01f:8.0f,hi=ray%5?100.0f:15.0f;
        for(uint mask=0;mask<4;++mask) for(bool solid:{false,true}) for(bool shadow:{false,true}) {
            double best=hi;bool expected=false;uint clsBest=0,primBest=0;bool frontBest=false;
            for(uint cls=0;cls<2;++cls) if(mask&(1u<<cls)) {
                const auto &tris=cls?u_dynamicTris.tris:u_worldTris.tris;
                for(uint i=0;i<128;++i) {
                    double t;bool front;
                    if(!reference(tris[3+i],o,d,lo,best,t,front)) continue;
                    if(i>=64 && (!solid || cls!=0)) {
                        uint local=i-64;
                        // Independently stated synthetic material policy.
                        if((shadow || solid) && (local+cls)%5==0) continue;
                        if((local+2*cls)%3==0 || (!front && local%2!=0)) continue;
                    }
                    expected=true;best=t;clsBest=cls;primBest=i;frontBest=front;
                }
            }
            RtHitPayload hit{};
            bool actual=rt_sw_trace(o,d,lo,hi,mask,shadow,solid,hit);
            assert(actual==expected);
            if(!actual) {++misses;continue;}
            ++hits;
            if(!shadow) {
                assert(std::abs(hit.hitT-best)<2e-5);
                assert(hit.customIndex==clsBest<<20);
                assert(hit.primitive==primBest%64);
                assert(hit.geometry==(uint(primBest>=64)|(frontBest?RT_HIT_FRONT_FACING:0u)));
            }
            uint cls=hit.customIndex>>20,prim=hit.primitive+((hit.geometry&1u)?64:0);
            assert(mask&(1u<<cls));
            double t;bool front;
            const auto &tris=cls?u_dynamicTris.tris:u_worldTris.tris;
            assert(reference(tris[3+prim],o,d,lo,hi,t,front));
            if(prim>=64 && (!solid || cls!=0)) {
                RtTriangle material{cls,prim-64};
                assert(!(shadow||solid) || rt_casts_shadow(material));
                assert(rt_hit_accepted(material,hit.bary,front,d));
            }
        }
    }
    assert(hits>1000 && misses>1000);
}
'''
        compile_run(self, source)


if __name__ == '__main__':
    unittest.main()
