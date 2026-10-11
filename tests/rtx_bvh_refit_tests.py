"""Execute the production BVH builder/refitter on moving and invalid geometry."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class RtxBvhRefitTests(unittest.TestCase):
    def test_refit_updates_all_leaf_and_parent_bounds(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        source = r'''
#include <cassert>
#include <cmath>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <vector>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))
#define VectorCopy(a,b) std::memcpy((b),(a),3*sizeof(float))
#define Com_Memcpy std::memcpy
#define Com_Memset std::memset
#define RT_HostAlloc std::malloc
#define RT_HostFree std::free
#define PRINT_WARNING 1
typedef bool qboolean;
const bool qtrue=true, qfalse=false;
static void print_stub(int,const char*,...) {}
struct { void (*Printf)(int,const char*,...); } ri = { print_stub };
struct rtxRtPackedVertex_t { float xyz[4]; };
''' + '#include "' + (ROOT / 'code/rendererrtx/rt/rt_gpu.h').as_posix() + '"\n' + (
            ROOT / 'code/rendererrtx/rt/rt_bvh.c').read_text() + r'''
static void validate(const rtBvh_t& bvh, const std::vector<rtxRtPackedVertex_t>& verts,
                     const std::vector<uint32_t>& indices, uint32_t bias) {
    for (uint32_t i=0; i<bvh.numNodes; ++i) {
        const auto& n=bvh.nodes[i];
        if (!n.count) {
            for (uint32_t c=n.first; c<=n.first+1; ++c)
                for (int k=0; k<3; ++k) {
                    assert(n.bmin[k]<=bvh.nodes[c].bmin[k]);
                    assert(n.bmax[k]>=bvh.nodes[c].bmax[k]);
                }
        } else {
            for (uint32_t j=n.first; j<n.first+n.count; ++j) {
                const auto& t=bvh.tris[j]; uint32_t id;
                memcpy(&id,&t.v0[3],sizeof(id)); assert(id>=bias); id-=bias;
                const float* corners[3]={t.v0,t.v1,t.v2};
                for (int c=0;c<3;++c) for(int k=0;k<3;++k) {
                    assert(corners[c][k]==verts[indices[id*3+c]].xyz[k]);
                    assert(corners[c][k]>=n.bmin[k] && corners[c][k]<=n.bmax[k]);
                }
            }
        }
    }
}
int main() {
    std::vector<rtxRtPackedVertex_t> vertices(96*3);
    std::vector<uint32_t> indices(vertices.size());
    for (unsigned t=0;t<96;++t) for(unsigned c=0;c<3;++c) {
        auto& v=vertices[t*3+c]; indices[t*3+c]=t*3+c;
        v.xyz[0]=float(t*4+(c==1)); v.xyz[1]=float(c==2); v.xyz[2]=float(t%3);
    }
    rtBvh_t bvh={}; uint32_t no,to;
    assert(RT_BvhAppend(&bvh,vertices.data(),unsigned(vertices.size()),indices.data(),96,17,&no,&to));
    assert(no==0 && to==0 && bvh.numNodes>1);
    const float cost=RT_BvhCost(&bvh);
    std::vector<rtBvhNode_t> topology(bvh.nodes,bvh.nodes+bvh.numNodes);
    uint32_t oldBias=17;
    for(unsigned f=0;f<80;++f) {
        for(auto& v:vertices) {v.xyz[0]+=13; v.xyz[1]-=7; v.xyz[2]+=0.25f;}
        assert(RT_BvhRefit(&bvh,vertices.data(),unsigned(vertices.size()),indices.data(),96,oldBias,f*101));
        oldBias=f*101;
        validate(bvh,vertices,indices,oldBias);
        assert(std::abs(cost-RT_BvhCost(&bvh))<1e-5f);
        for(unsigned i=0;i<bvh.numNodes;++i) {
            assert(bvh.nodes[i].first==topology[i].first && bvh.nodes[i].count==topology[i].count);
        }
    }
    // Disperse formerly adjacent triangles: overlap makes refitting poor.
    for(unsigned t=0;t<96;++t) for(unsigned c=0;c<3;++c)
        vertices[t*3+c].xyz[0]=float(((t*37)%96)*4+(c==1));
    assert(RT_BvhRefit(&bvh,vertices.data(),unsigned(vertices.size()),indices.data(),96,oldBias,oldBias));
    validate(bvh,vertices,indices,oldBias);
    assert(RT_BvhCost(&bvh)>cost*1.5f);
    assert(!RT_BvhRefit(&bvh,vertices.data(),unsigned(vertices.size()),indices.data(),95,oldBias,oldBias));
    indices[0]=999999;
    assert(!RT_BvhRefit(&bvh,vertices.data(),unsigned(vertices.size()),indices.data(),96,oldBias,oldBias));
    indices[0]=0; vertices[0].xyz[0]=std::numeric_limits<float>::quiet_NaN();
    assert(!RT_BvhRefit(&bvh,vertices.data(),unsigned(vertices.size()),indices.data(),96,oldBias,oldBias));
    RT_BvhFree(&bvh); assert(!bvh.nodes && !bvh.tris && !bvh.numNodes);
    std::puts("BVH refit: translation, deformation, quality, invalid input and cleanup PASS");
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-bvh-') as temp:
            path = Path(temp)
            cpp = path / 'refit.cpp'
            exe = path / ('refit.exe' if os.name == 'nt' else 'refit')
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                                   cwd=path, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stderr)
            result = subprocess.run([str(exe)], cwd=path, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('PASS', result.stdout)


if __name__ == '__main__':
    unittest.main()
