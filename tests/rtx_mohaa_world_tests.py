"""Execute RTX's world extraction with MOHAA's appended world surfaces.

The real extraction functions run on a synthetic BSP with an inline door
between model zero and the terrain/static scenery. Geometry uploads and
material lookup are bounded stubs; no retail assets are needed.
"""
from pathlib import Path
import unittest

from mesh_tag_tests import function
from rtx_surface_owner_tests import compile_run

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
typedef int qboolean;
enum { qfalse, qtrue };
#define Com_Memset memset
enum { SF_FACE, SF_GRID, SF_TRIANGLES };
enum rtOwner_t { RT_OWNER_RASTER, RT_OWNER_TRACED, RT_OWNER_BASE_RASTER };
enum { RT_COMPILED_ALPHASHADOW = 1, RT_COMPILED_LIGHTFILTER = 2,
       RT_SKY_BOX = 1, RT_SKY_CLOUDS = 2, RT_MARK_FILTER = 4, RT_MARK_NOSHADOW = 8 };
struct shader_t { int material, rtCompiledFilter, isSky, rtCompiledTranslucent; };
struct srfTriangles_t { int type, tag; };
typedef srfTriangles_t srfSurfaceFace_t;
typedef srfTriangles_t srfGridMesh_t;
struct msurface_t { const shader_t *shader; const int *data; };
struct bmodel_t { msurface_t *firstSurface; int numSurfaces; };
struct advertisement_t { const bmodel_t *bmodel; };
struct world_t {
    bmodel_t *bmodels;
    msurface_t *surfaces;
    int numsurfaces, mohaaFirstBuiltSurface, mohaaNumBuiltSurfaces;
    int numAdvertisements;
    advertisement_t advertisements[2];
};
struct vertex_t { float lightmapData[4]; };
struct rtxRtCpuGeometry_t { unsigned numVertices; vertex_t vertices[16]; int tags[16]; };
static struct { world_t *world; int worldMapLoaded; const shader_t *defaultShader; } tr;
static struct { unsigned skyImages[2][4]; } s_rt;
static struct { int integer; } patchShadows = { 1 }, *rtx_rt_patch_shadows = &patchShadows;
static int failTag = -1, freed, remapped, q3map2;
static int RT_MapIsQ3map2() { return q3map2; }
static uint32_t RT_MaterialIndexForShader(const shader_t *s) { return s->material; }
static rtOwner_t RT_MaterialOwner(uint32_t i) { return (rtOwner_t)i; }
static int RT_ShaderIsSky(const shader_t *s) { return s->isSky; }
static int RT_ShaderIsLiquid(const shader_t *) { return 0; }
static void RT_RecordSky(const shader_t *) { s_rt.skyImages[1][3] = RT_SKY_BOX; }
static void vk_rt_cpu_geometry_init(rtxRtCpuGeometry_t *g) { memset(g, 0, sizeof(*g)); }
static void vk_rt_cpu_geometry_free(rtxRtCpuGeometry_t *g) { freed++; g->numVertices = 0; }
static void RT_RemapMaterials(rtxRtCpuGeometry_t *, int entity) { assert(entity == 0); remapped++; }
static int Append(rtxRtCpuGeometry_t *g, const srfTriangles_t *s, const shader_t *, int entity) {
    assert(entity == 0 && g->numVertices < 16);
    if (s->tag == failTag) return qfalse;
    g->tags[g->numVertices++] = s->tag;
    return qtrue;
}
#define vk_rt_append_surface_face_geometry Append
#define vk_rt_append_surface_grid_geometry Append
#define vk_rt_append_surface_triangles_geometry Append
'''

CHECKS = r'''
int main() {
    const shader_t solid = { RT_OWNER_TRACED, 0, 0, 0 };
    const shader_t decal = { RT_OWNER_RASTER, 0, 0, 0 };
    const shader_t filter = { RT_OWNER_RASTER, RT_COMPILED_LIGHTFILTER, 0, 1 };
    srfTriangles_t data[8] = { {SF_FACE,0}, {SF_GRID,1}, {SF_FACE,2},
        {SF_TRIANGLES,3}, {SF_TRIANGLES,4}, {SF_TRIANGLES,5},
        {SF_TRIANGLES,6}, {SF_FACE,7} };
    msurface_t surfaces[8];
    for (int i=0; i<8; ++i) surfaces[i] = { &solid, &data[i].type };
    surfaces[5].shader = &decal;
    surfaces[6].shader = &filter;
    bmodel_t models[2] = { {surfaces, 2}, {surfaces+2, 1} };
    bmodel_t ad = { surfaces+7, 1 };
    world_t world = { models, surfaces, 7, 3, 4, 0, {} };
    tr.world = &world; tr.worldMapLoaded = 1; tr.defaultShader = &solid;
    rtxRtCpuGeometry_t geometry;

    // Model zero then terrain and static scenery, without the inline door
    // or raster decal. A material with q3map2 lightfilter also casts shadows.
    q3map2 = 1;
    assert(RT_ExtractWorld(&geometry));
    const int want[] = { 0, 1, 3, 4, 6 };
    assert(geometry.numVertices == 5 && remapped == 1 && freed == 0);
    for (int i=0; i<5; ++i) assert(geometry.tags[i] == want[i]);
    assert(geometry.vertices[4].lightmapData[3] == RT_MARK_FILTER);
    q3map2 = 0;
    assert(RT_ExtractWorld(&geometry) && geometry.numVertices == 4);

    // A map with static scenery but no terrain still traces the appended
    // range. The final count excludes unfilled slots after a failed skin.
    world.mohaaFirstBuiltSurface = 4; world.mohaaNumBuiltSurfaces = 1;
    assert(RT_ExtractWorld(&geometry) && geometry.numVertices == 3);
    assert(geometry.tags[2] == 4);

    // Other profiles and unaugmented maps have an empty generated range.
    world.mohaaFirstBuiltSurface = world.mohaaNumBuiltSurfaces = 0;
    assert(RT_ExtractWorld(&geometry) && geometry.numVertices == 2);
    world.numAdvertisements = 2;
    world.advertisements[0].bmodel = &ad;
    world.advertisements[1].bmodel = NULL;
    assert(RT_ExtractWorld(&geometry) && geometry.numVertices == 3);
    assert(geometry.tags[2] == 7); // QL's separate static brush model

    // A failed append releases the entire partial extraction, whichever
    // range it occurs in, without proceeding to material remapping.
    world.mohaaFirstBuiltSurface = 3; world.mohaaNumBuiltSurfaces = 4;
    q3map2 = 1;
    for (int tag=0; tag<8; ++tag) {
        if (tag == 2 || tag == 5) continue; // intentionally never appended
        int oldFreed = freed, oldRemapped = remapped;
        failTag = tag;
        assert(!RT_ExtractWorld(&geometry) && geometry.numVertices == 0);
        assert(freed == oldFreed+1 && remapped == oldRemapped);
    }
    failTag = -1;

    // Reject invalid generated ranges before reading a surface. Both the
    // subtraction and the zero-length endpoint are safe at integer limits.
    const int invalid[][2] = { {-1,1}, {3,-1}, {8,0}, {7,1}, {3,INT_MAX}, {INT_MAX,1} };
    for (const auto &range : invalid) {
        int oldRemapped = remapped;
        world.mohaaFirstBuiltSurface = range[0]; world.mohaaNumBuiltSurfaces = range[1];
        assert(!RT_ExtractWorld(&geometry) && geometry.numVertices == 0);
        assert(remapped == oldRemapped);
    }
    world.mohaaFirstBuiltSurface = 7; world.mohaaNumBuiltSurfaces = 0;
    assert(RT_ExtractWorld(&geometry));
    world.mohaaFirstBuiltSurface = 3; world.mohaaNumBuiltSurfaces = 4;
    world.surfaces = NULL;
    assert(!RT_ExtractWorld(&geometry));
    world.surfaces = surfaces;
    world.bmodels = NULL;
    assert(!RT_ExtractWorld(&geometry));
    tr.world = NULL;
    assert(!RT_ExtractWorld(&geometry));
    tr.world = &world; world.bmodels = models; tr.worldMapLoaded = 0;
    assert(!RT_ExtractWorld(&geometry));
    return 0;
}
'''


class RtxMohaaWorldTests(unittest.TestCase):
    def test_generated_world_geometry_and_failure_paths(self):
        backend = (ROOT / 'code/rendererrtx/rt/rt_backend.c').read_text(encoding='utf-8')
        source = HOST + function(backend, 'static qboolean RT_ExtractWorldSurface(')
        source += function(backend, 'static qboolean RT_ExtractWorld(')
        compile_run(self, source + CHECKS)

    def test_builder_range_reaches_rtx_world(self):
        bsp = (ROOT / 'code/rendererrtx/tr_bsp.c').read_text(encoding='utf-8')
        for field in ('FirstBuiltSurface', 'NumBuiltSurfaces'):
            self.assertIn(f's_worldData.mohaa{field} = mohaaBuilt.{field[0].lower() + field[1:]};', bsp)


if __name__ == '__main__':
    unittest.main()
