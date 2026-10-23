"""Execute light proposal/candidate GLSL: overflow support and expected energy."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtx_transport_tests import function

ROOT = Path(__file__).resolve().parents[1]


class RtxLightProposalTests(unittest.TestCase):
    def test_mixture_density_and_ris_across_cell_boundaries(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        source = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
using uint = uint32_t;
static uint min(uint a,uint b) { return a<b?a:b; }
struct uvec2 { uint x,y; uvec2(uint a=0,uint b=0):x(a),y(b){} };
struct uvec4 { uint x,y,z,w; };
struct vec3 { float x,y,z; vec3(float a=0,float b=0,float c=0):x(a),y(b),z(c){} };
struct {uvec4 lightGridDims,counts,flags;} u_frame;
struct {uvec2 cells[1000];} u_lightGrid;
struct {uint indices[128];} u_lightIndices;
static uint floatBitsToUint(float x) { uint u; std::memcpy(&u,&x,4); return u; }
static float uintBitsToFloat(uint u) { float x; std::memcpy(&x,&u,4); return x; }
static float rand01(uint &seed) { seed=seed*1664525u+1013904223u; return float(seed>>8)/16777216.0f; }
struct RtLightSample {vec3 E;};
static float energy(uint i) { return float(1+i%11); }
static float luminance(vec3 c) { return c.x; }
static bool rt_light_eval(uint i,vec3,vec3,uint exclude,uint&,RtLightSample &s) {
    s.E=vec3(exclude==2 ? 0 : energy(i)); return exclude!=1;
}
'''
        source += (ROOT / 'code/rendererrtx/shaders/rt_light_proposal.glsl').read_text()
        lighting = (ROOT / 'code/rendererrtx/shaders/rt_lighting.glsl').read_text()
        source += function((ROOT / 'code/rendererrtx/shaders/rt_common.glsl').read_text(), 'hash_u32')
        source += function(lighting, 'rt_light_stream')
        source += (ROOT / 'code/rendererrtx/shaders/rt_light_reuse.glsl').read_text()
        candidate = function(lighting, 'rt_light_candidate')
        candidate = re.sub(r'out (\w+) (\w+)', r'\1 &\2', candidate)
        source += candidate
        source += r'''
int main() {
    const uint lights=160;
    u_frame.lightGridDims={2,1,1,lights};
    u_frame.counts={0,0,0,lights};
    u_frame.flags={0,0,0,2};
    double sum=0;
    for(uint i=0;i<lights;i++) sum+=energy(i);
    double cumulative=0; float previous=0;
    for(uint i=0;i<lights;i++) {
        cumulative+=energy(i);
        float cdf=i+1==lights?1.0f:float(0.9*cumulative/sum+0.1*(i+1)/lights);
        u_lightGrid.cells[2+i]=uvec2(floatBitsToUint(cdf),floatBitsToUint(cdf-previous));
        previous=cdf;
    }
    // Adjacent cells contain disjoint top-64 lists. Another 32 emitters
    // occur in neither list; two dynamic lights occur in both proposals.
    for(uint i=0;i<128;i++) u_lightIndices.indices[i]=i;
    const double expected=sum+energy(lights)+energy(lights+1);
    uint seed=812931;
    // Each reused stream lasts four frames, but exactly one of the four
    // changes per frame. Receiver/key changes invalidate the replay.
    for(uint frame=1;frame<1000;frame++) {
        uint changed=0;
        for(uint k=0;k<4;k++) {
            changed += rt_reused_light_stream(987,frame,k)!=rt_reused_light_stream(987,frame-1,k);
            assert(rt_reused_light_stream(987,frame,k)!=rt_reused_light_stream(654,frame,k));
        }
        assert(changed==1);
    }
    for (uint offset : {0u,64u}) for(uint reuse : {0u,1u}) {
        uvec2 cell(offset,64);
        double pdfSum=0;
        for(uint i=0;i<lights+2;i++) {
            float pdf=rt_light_proposal_pdf(cell,66,0.25f,i);
            assert(pdf>0); pdfSum+=pdf;
        }
        assert(std::abs(pdfSum-1)<1e-6);
        double estimateSum=0; uint seenOverflow=0;
        // RIS with sixteen candidates, optionally replaying four of them.
        // A moving blocker flips visibility every 32 frames; reuse must not
        // carry the previous blocker result into this frame's estimate.
        double visibleExpected=0;
        for(uint i=0;i<lights+2;i++) if(i%3) visibleExpected+=energy(i);
        double expectedSum=0;
        for(uint trial=0;trial<180000;trial++) {
            float weightSum=0,targetSelected=0; uint selected=0;
            for(uint k=0;k<16;k++) {
                uint i; float target,weight; RtLightSample sample;
                rand01(seed);
                uint stream=reuse && k<4 ? rt_reused_light_stream(812931+offset,trial,k) : rt_light_stream(seed,k);
                assert(rt_light_candidate(vec3(),vec3(),cell,66,false,0.25f,0,stream,k,i,sample,target,weight));
                weightSum+=weight;
                if(rand01(seed)*weightSum<weight) {selected=i;targetSelected=target;}
            }
            if(selected>=128 && selected<160) seenOverflow++;
            bool flip=(trial/32)%2;
            float visibility=(selected%3 != 0) != flip ? 1.0f : 0.0f;
            expectedSum+=flip ? expected-visibleExpected : visibleExpected;
            estimateSum+=energy(selected)*visibility*(weightSum/16)/targetSelected;
        }
        assert(seenOverflow>1000);
        assert(std::abs(estimateSum-expectedSum)/expectedSum<0.012);
    }
    // An empty local list still reaches every static emitter.
    double globalSum=0;
    for(uint i=0;i<lights;i++) globalSum+=rt_light_proposal_pdf(uvec2(),0,1.0f,i);
    assert(std::abs(globalSum-1)<1e-6);
    for(uint k=0;k<10000;k++) {
        uint i;float target,weight;RtLightSample s;
        rand01(seed);
        assert(rt_light_candidate(vec3(),vec3(),uvec2(),0,false,1.0f,0,seed,k,i,s,target,weight));
        assert(i<lights && weight>0 && std::isfinite(weight));
    }
    // Dynamic-only enumeration has no global contribution or lost energy.
    float exact=0;
    for(uint k=0;k<2;k++) {
        uint i;float target,weight;RtLightSample s;
        assert(rt_light_candidate(vec3(),vec3(),uvec2(),2,true,0,0,seed,k,i,s,target,weight));
        assert(i==lights+k && weight==energy(i)); exact+=weight;
    }
    assert(exact==energy(lights)+energy(lights+1));
    // Excluded/backfacing and zero-energy draws must retain zero reservoir
    // weight with either proposal, including an empty local list.
    for(uint exclude : {1u,2u}) for(float globalShare : {0.0f,0.25f,1.0f}) {
        for(uint k=0;k<1000;k++) {
            uint i; float target=-1,weight=-1; RtLightSample s;
            rand01(seed);
            uvec2 cell=globalShare==1 ? uvec2() : uvec2(0,64);
            uint total=globalShare==1 ? 0 : 66;
            assert(!rt_light_candidate(vec3(),vec3(),cell,total,false,globalShare,exclude,seed,k,i,s,target,weight));
            assert(target==0 && weight==0);
        }
    }
    std::puts("overflow support, cell continuity and fresh-visibility RIS: PASS");
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-proposal-') as temp:
            path = Path(temp)
            cpp = path / 'proposal.cpp'
            exe = path / ('proposal.exe' if os.name == 'nt' else 'proposal')
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2', str(cpp), '-o', str(exe)],
                           cwd=path, check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], cwd=path, check=True, capture_output=True, text=True)
            self.assertIn('PASS', result.stdout)


if __name__ == '__main__':
    unittest.main()
