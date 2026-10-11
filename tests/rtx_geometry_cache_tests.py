"""Execute the actual bounded model-pose cache, not a Python replica."""
from pathlib import Path
import unittest
from rtx_surface_owner_tests import compile_run

ROOT = Path(__file__).resolve().parents[1]


class RtxGeometryCacheTests(unittest.TestCase):
    def test_pose_transform_material_lifetime_and_failure(self):
        source = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#define Com_Memset memset
#define VectorClear(a) memset(a, 0, sizeof(a))
static void AxisClear(float a[3][3]) { memset(a, 0, 9*sizeof(float)); a[0][0]=a[1][1]=a[2][2]=1; }
typedef int qboolean;
enum {qfalse, qtrue, RT_MODEL=1, COM_GAME_QUAKE3_IHV=9, RF_WRAP_FRAMES=16,
      MOD_BRUSH=1, MOD_MESH, MOD_MDR, MOD_IQM, MOD_OTHER};
struct shader_t { int tag; };
struct model_t { int type; };
struct trRefEntity_t { struct { int reType, hModel, frame, oldframe, customSkin, skinNum,
    customShader, renderfx; float backlerp, origin[3], axis[3][3]; struct { int blink; } rtcw; } e; };
struct rtxRtPackedVertex_t { float xyz[3]; uint32_t materialIndex; };
struct rtxRtMaterial_t { uint32_t flags; };
struct rtxRtCpuGeometry_t {
    rtxRtPackedVertex_t *vertices; uint32_t *indices; rtxRtMaterial_t *materials; const shader_t **materialShaders;
    uint32_t numVertices, numIndices, numMaterials, vertexCapacity, indexCapacity, materialCapacity;
    uint32_t sourceTriangleCount, maskedTriangleCount, anyHitTriangleCount, sourceEntityCount;
};
static struct { int gameId; } ri;
static struct Cvar { int integer; } skinning={1}, *rtx_rt_iqm_skinning=&skinning;
static model_t models[100]; static shader_t shaders[100];
static int extracts, failExtract, failReserve;
static const model_t *R_GetModelByHandle(int i) { return i>0 && i<100 ? &models[i] : nullptr; }
static void vk_rt_cpu_geometry_free(rtxRtCpuGeometry_t *g) {
    free(g->vertices); free(g->indices); free(g->materials); free(g->materialShaders); memset(g,0,sizeof(*g));
}
static int vk_rt_cpu_geometry_reserve(rtxRtCpuGeometry_t *g, uint32_t v, uint32_t i) {
    if(failReserve) return 0;
    g->vertexCapacity=g->numVertices+v; g->indexCapacity=g->numIndices+i;
    g->vertices=(rtxRtPackedVertex_t*)realloc(g->vertices,g->vertexCapacity*sizeof(*g->vertices));
    g->indices=(uint32_t*)realloc(g->indices,g->indexCapacity*sizeof(*g->indices)); return 1;
}
static int vk_rt_cpu_geometry_reserve_materials(rtxRtCpuGeometry_t *g, uint32_t m) {
    if(failReserve) return 0;
    g->materialCapacity=g->numMaterials+m;
    g->materials=(rtxRtMaterial_t*)realloc(g->materials,g->materialCapacity*sizeof(*g->materials));
    g->materialShaders=(const shader_t**)realloc(g->materialShaders,g->materialCapacity*sizeof(*g->materialShaders)); return 1;
}
static void vk_rt_transform_geometry_range(rtxRtCpuGeometry_t *g, uint32_t first, const trRefEntity_t *e) {
    for(unsigned i=first;i<g->numVertices;++i) {
        float p[3]; memcpy(p,g->vertices[i].xyz,sizeof(p));
        for(int a=0;a<3;++a) g->vertices[i].xyz[a]=e->e.origin[a]+p[0]*e->e.axis[0][a]+p[1]*e->e.axis[1][a]+p[2]*e->e.axis[2][a];
    }
}
static int vk_rt_extract_dynamic_model_entity_geometry(rtxRtCpuGeometry_t *g, const trRefEntity_t *e, uint32_t *n) {
    ++extracts; if(failExtract) return 0;
    if(!g || !e || e->e.reType!=RT_MODEL || e->e.hModel<=0) return 1;
    assert(vk_rt_cpu_geometry_reserve(g,3,3)); assert(vk_rt_cpu_geometry_reserve_materials(g,1));
    unsigned first=g->numVertices, mat=g->numMaterials++;
    g->materialShaders[mat]=&shaders[e->e.customShader]; g->materials[mat].flags=0;
    for(int i=0;i<3;++i) {
        g->vertices[g->numVertices++]={{float(e->e.frame)+e->e.backlerp, float(i), 0},mat};
        g->indices[g->numIndices++]=first+i;
    }
    vk_rt_transform_geometry_range(g,first,e); g->sourceTriangleCount++; g->sourceEntityCount++;
    if(n) ++*n; return 1;
}
#include "code/rendererrtx/rt/rt_geometry_cache.h"
static void Drop(rtxRtCpuGeometry_t &g) { vk_rt_cpu_geometry_free(&g); }
int main() {
    for(auto &m:models) m.type=MOD_MESH;
    trRefEntity_t e={}; e.e.reType=RT_MODEL; e.e.hModel=1; e.e.frame=2; e.e.backlerp=.25f; e.e.customShader=3;
    AxisClear(e.e.axis); e.e.origin[0]=10;
    rtxRtCpuGeometry_t out={}; uint32_t n=0;
    assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n));
    assert(extracts==1 && n==1 && out.vertices[0].xyz[0]==12.25f);
    e.e.origin[0]=20; e.e.axis[0][0]=0; e.e.axis[0][1]=1; e.e.axis[1][0]=-1; e.e.axis[1][1]=0;
    assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n));
    assert(extracts==1 && n==2 && out.vertices[3].xyz[0]==20 && out.vertices[3].xyz[1]==2.25f);
    assert(out.indices[3]==3 && out.vertices[3].materialIndex==1 && out.materialShaders[1]==&shaders[3]);
    // Sub-frame pose changes MUST miss; neither time nor frames are throttled.
    e.e.backlerp=.251f; assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); assert(extracts==2);
    e.e.customShader=4; assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); assert(extracts==3);
    e.e.rtcw.blink=1; assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); assert(extracts==4);
    e.e.renderfx=RF_WRAP_FRAMES; assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); assert(extracts==5);
    skinning.integer=0; assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); assert(extracts==6);
    Drop(out); RT_GeometryCacheClear(); assert(s_rtGeometryHits==0);
    assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); assert(extracts==7);
    Drop(out);
    // Failed appends/extractions cannot publish partially filled cache entries.
    failReserve=1; assert(!vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); failReserve=0;
    e.e.frame=5; failExtract=1; assert(!vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); failExtract=0;
    assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,&n)); assert(out.vertices[0].xyz[1]==5.251f); Drop(out);
    // More than the slot count evicts; no frame's pose depends on old contents.
    for(int i=1;i<=32;++i) { e.e.hModel=i; assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,nullptr)); Drop(out); }
    int before=extracts; e.e.hModel=1; assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,nullptr)); assert(extracts==before+1); Drop(out);
    // Procedural skeletons and IHV bypass until all their pose inputs are keyed.
    models[1].type=MOD_OTHER; before=extracts;
    for(int i=0;i<2;++i) { assert(vk_rt_append_dynamic_model_entity_geometry(&out,&e,nullptr)); Drop(out); }
    assert(extracts==before+2); RT_GeometryCacheClear(); return 0;
}
'''
        compile_run(self, source)


if __name__ == '__main__':
    unittest.main()
