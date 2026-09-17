"""RTCW MD3 heads must decode the same packed normals as MDC base frames."""
from pathlib import Path
import unittest

from mesh_tag_tests import function
import rtcw_player_interaction_tests as interaction

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_mdc_decode.h"
#include <cassert>
#include <cstddef>
#include <initializer_list>
constexpr int FUNCTABLE_SIZE=1024, FUNCTABLE_MASK=1023;
static fnMdcNormals_t r_mdcNormals;
static struct { comGameId_t gameId; } ri;
static struct { float sinTable[FUNCTABLE_SIZE]; } tr;
static struct { struct { int frame,oldframe; } e; } entity;
static struct { decltype(entity) *currentEntity=&entity; } backEnd;
static struct { int numVertexes; vec4_t xyz[2],normal[2]; } tess;
static void Expect(const float *actual,const float *expected) {
 for(int k=0;k<3;++k) assert(fabsf(actual[k]-expected[k])<0.000002f);
}
static void LegacyNormal(unsigned packed,float *out) {
 const auto *table=tr.sinTable;
 unsigned a=4*(packed>>8), b=4*(packed&255);
 out[0]=table[(a+256)&1023]*table[b]; out[1]=table[a]*table[b]; out[2]=table[(b+256)&1023];
}
'''

CHECKS = r'''
int main() {
 FnMdc_InitNormals(&r_mdcNormals);
 for(int i=0;i<1024;++i) tr.sinTable[i]=sin(DEG2RAD(i*360.0f/1024)+0.0001f);
 struct { md3Surface_t header; md3XyzNormal_t vertices[2]; } surface{};
 surface.header.numVerts=1; surface.header.ofsXyzNormals=offsetof(decltype(surface),vertices);
 entity.e.frame=1; entity.e.oldframe=0; tess.numVertexes=1;
 surface.vertices[0].xyz[0]=-64;surface.vertices[1].xyz[0]=128;
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  ri.gameId=profile;
  bool rtcw=profile==COM_GAME_RTCW_SP||profile==COM_GAME_RTCW_MP;
  for(unsigned packed=0;packed<65536;++packed) {
   fnMdcVertex_t base{};base.normal=packed;
   vec3_t expected;
   if(rtcw) FnMdc_DecodeNormal(&r_mdcNormals,&base,expected);
   else LegacyNormal(packed,expected);
   surface.vertices[1].normal=static_cast<short>(packed);
   LerpMeshVertexes_scalar(&surface.header,0); Expect(tess.normal[1],expected);
   assert(tess.xyz[1][0]==2);
   if((packed&255)==0) {
    // Interpolated head animation must use the selected table for both frames.
    base.normal=packed^0x3549;surface.vertices[0].normal=static_cast<short>(base.normal);
    vec3_t old;
    if(rtcw) FnMdc_DecodeNormal(&r_mdcNormals,&base,old); else LegacyNormal(base.normal,old);
    for(float lerp:{0.25f,0.75f,1.0f}) {
     vec3_t blended;
     for(int k=0;k<3;++k) blended[k]=old[k]*lerp+expected[k]*(1-lerp);
     VectorNormalizeFast(blended);
     LerpMeshVertexes_scalar(&surface.header,lerp);Expect(tess.normal[1],blended);
     assert(tess.xyz[1][0]==-lerp+2*(1-lerp));
    }
   }
   CHECK_RAY(packed,expected,rtcw);
  }
 }
}
'''


def fixture(renderer, source=None):
    surface = source or (ROOT / 'code' / renderer / 'tr_surface.c').read_text()
    result = PRELUDE + function((ROOT / 'code/qcommon/q_math.c').read_text(), 'float Q_rsqrt( float number )\n{')
    result += function(surface, 'static void VectorArrayNormalize(')
    result += function(surface, 'static void LerpMeshVertexes_scalar(')
    if renderer == 'rendererrtx':
        result += function((ROOT / 'code/rendererrtx/vk.c').read_text(), 'static void vk_rt_decode_md3_normal( short packedNormal, vec3_t outNormal )\n{')
        result += r'''
static void CheckRay(unsigned packed,const float *raster,bool rtcw) {
 vec3_t ray;vk_rt_decode_md3_normal(static_cast<short>(packed),ray);
 if(rtcw) Expect(ray,raster);
 else {
  // Preserve the native Q3/QL ray path's existing 255-period analytical decoder.
  float a=(packed>>8)*(2.0f*(float)M_PI/255.0f), b=(packed&255)*(2.0f*(float)M_PI/255.0f);
  vec3_t expected={cosf(a)*sinf(b),sinf(a)*sinf(b),cosf(b)};Expect(ray,expected);
 }
}
#define CHECK_RAY CheckRay
'''
    else:
        result += '#define CHECK_RAY(p,n,r) ((void)0)\n'
    return result + CHECKS


class RTCWMD3NormalTests(unittest.TestCase):
    def test_all_packed_normals_profile_isolation_and_head_interpolation(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                interaction.RTCWPlayerInteractionTests.compile_and_run(self, fixture(renderer))


if __name__ == '__main__':
    unittest.main()
