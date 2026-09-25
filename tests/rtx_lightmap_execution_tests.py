"""Execute the path tracer's material classification and packed vertex contracts.

The functions under test are compiled from code/rendererrtx/rt/rt_material.c
and vk.c into small C harnesses; the GLS, sort and surface-flag values come
from the engine headers, so the harness cannot drift from the renderer.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature + "\n{")
    return source[start:source.index("\n}", start) + 2]


def engine_constants():
    """The GLS bits, shader sorts and surface flags the classifier reads."""
    local = (ROOT / "code/rendererrtx/tr_local.h").read_text(encoding="utf-8")
    gls = "\n".join(re.findall(r"^#define\s+GLS_\w+\s+[^\n]+", local, re.M))
    sort = re.search(r"typedef enum \{\s*SS_BAD,.*?\} shaderSort_t;", local, re.S)
    assert sort is not None
    flags = (ROOT / "code/qcommon/surfaceflags.h").read_text(encoding="utf-8")
    surf = "\n".join(re.findall(r"^#define\s+(?:SURF_SKY|SURF_NODRAW|CONTENTS_FOG)\s+\w+", flags, re.M))
    return gls + "\n" + sort.group(0) + "\n" + surf + "\n"


class RtxLightmapExecutionTests(unittest.TestCase):
    def run_c(self, source):
        compiler = next((p for n in ("clang", "gcc", "cc") if (p := shutil.which(n))), None)
        if not compiler:
            self.skipTest("requires a C11 compiler")
        (ROOT / ".tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="rtx-lightmap-", dir=ROOT / ".tmp") as folder:
            path = Path(folder)
            code = path / "test.c"
            exe = path / ("test.exe" if os.name == "nt" else "test")
            code.write_text(source, encoding="utf-8")
            command = [compiler, "-std=c11"]
            if (os.name == "nt" and "clang" in Path(compiler).name.lower()
                    and os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() == "x86"):
                command.append("--target=i686-pc-windows-msvc")
            result = subprocess.run([*command, str(code), "-o", str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def material_harness(self):
        source = (ROOT / "code/rendererrtx/rt/rt_material.c").read_text(encoding="utf-8")
        owner = re.search(r"typedef enum \{\s*RT_OWNER_RASTER,.*?\} rtOwner_t;", source, re.S)
        assert owner is not None
        pieces = [
            owner.group(0),
            function(source, "static qboolean RT_StageIsLightmap( const rtStageSource_t *s )"),
            function(source, "static unsigned RT_StageBlend( const rtStageSource_t *s )"),
            function(source, "static qboolean RT_StageAdds( const rtStageSource_t *s )"),
            function(source, "static qboolean RT_ShaderIsSky( const shader_t *shader )"),
            function(source, "static rtOwner_t RT_ClassifyOwner( const shader_t *shader, qboolean *masked )"),
            function(source, "static uint32_t RT_StageRole( const rtStageSource_t *s, int index, int lightingStage )"),
        ]
        gpu = (ROOT / "code/rendererrtx/rt/rt_gpu.h").read_text(encoding="utf-8")
        roles = "\n".join(re.findall(r"^#define\s+RT_ROLE_\w+\s+\w+", gpu, re.M))
        return r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef int qboolean;
enum { qfalse, qtrue };
enum { LIGHTMAP_INDEX_NONE = -1, LIGHTMAP_INDEX_SHADER = 0, IMGFLAG_LIGHTMAP = 8,
       TCGEN_BAD, TCGEN_IDENTITY, TCGEN_LIGHTMAP, TCGEN_TEXTURE, TCGEN_ENVIRONMENT_MAPPED,
       TCGEN_ENVIRONMENT_MAPPED_FP, TCGEN_FOG, CGEN_IDENTITY = 100, CGEN_FOG, CGEN_LIGHTING_DIFFUSE };
''' + engine_constants() + roles + r'''
typedef struct { int flags; } image_t;
typedef struct { image_t *images[8]; unsigned stateBits; int tcGen, rgbGen, lightmap, isScreenMap; } rtStageSource_t;
typedef struct { int isSky, surfaceFlags, sort, rtNumStages, polygonOffset, numDeforms, hasScreenMap;
    rtStageSource_t *rtStages; int rtCompiledSky; } shader_t;
''' + "\n".join(pieces)

    def test_trace_owns_only_opaque_or_alpha_tested_first_stages(self):
        self.run_c(self.material_harness() + r'''
static rtStageSource_t stages[2];
static shader_t shader;
static rtOwner_t classify(qboolean *masked) { return RT_ClassifyOwner(&shader, masked); }
static void reset(void) {
    memset(stages, 0, sizeof(stages)); memset(&shader, 0, sizeof(shader));
    shader.sort = SS_OPAQUE; shader.rtNumStages = 1; shader.rtStages = stages;
    stages[0].stateBits = GLS_DEPTHMASK_TRUE; stages[0].tcGen = TCGEN_TEXTURE;
}
int main(void) {
    qboolean masked;
    reset(); assert(classify(&masked) == RT_OWNER_TRACED && !masked);
    /* an explicit replace blend is still opaque */
    stages[0].stateBits |= GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO;
    assert(classify(&masked) == RT_OWNER_TRACED);
    /* an alpha test anywhere makes the material masked, still traced */
    reset(); shader.rtNumStages = 2; stages[1].stateBits = GLS_ATEST_GE_80;
    assert(classify(&masked) == RT_OWNER_TRACED && masked);
    reset(); shader.sort = SS_SEE_THROUGH; assert(classify(&masked) == RT_OWNER_TRACED);
    /* sky and portals keep their raster colour but stay in the scene */
    reset(); shader.isSky = 1; assert(classify(&masked) == RT_OWNER_BASE_RASTER);
    reset(); shader.surfaceFlags = SURF_SKY; assert(classify(&masked) == RT_OWNER_BASE_RASTER);
    /* the map's BSP says sky though a later script dropped the parm */
    reset(); shader.rtCompiledSky = 1; assert(classify(&masked) == RT_OWNER_BASE_RASTER);
    reset(); shader.sort = SS_PORTAL; assert(classify(&masked) == RT_OWNER_BASE_RASTER);
    /* everything the trace cannot reproduce stays raster */
    reset(); stages[0].stateBits |= GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); stages[0].stateBits = 0; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); stages[0].stateBits |= GLS_DEPTHTEST_DISABLE; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); stages[0].stateBits |= GLS_POLYMODE_LINE; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.polygonOffset = 1; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.numDeforms = 1; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.hasScreenMap = 1; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.surfaceFlags = SURF_NODRAW; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.sort = SS_DECAL; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.sort = SS_BLEND0; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.rtNumStages = 2; stages[1].isScreenMap = 1; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.rtNumStages = 0; assert(classify(&masked) == RT_OWNER_RASTER);
    reset(); shader.rtStages = 0; assert(classify(&masked) == RT_OWNER_RASTER);
    return 0;
}
''')

    def test_stage_roles_mark_the_lighting_term_and_authored_reflections(self):
        self.run_c(self.material_harness() + r'''
int main(void) {
    image_t plain = {0}, lightmapImage = {IMGFLAG_LIGHTMAP};
    rtStageSource_t s; memset(&s, 0, sizeof(s));
    s.lightmap = LIGHTMAP_INDEX_NONE; s.tcGen = TCGEN_TEXTURE; s.images[0] = &plain;
    assert(RT_StageRole(&s, 0, -1) == RT_ROLE_BASE);
    /* the $lightmap stage, however it is recognised */
    s.lightmap = LIGHTMAP_INDEX_SHADER; assert(RT_StageRole(&s, 0, 0) == RT_ROLE_LIGHTMAP);
    s.lightmap = LIGHTMAP_INDEX_NONE; s.tcGen = TCGEN_LIGHTMAP; assert(RT_StageRole(&s, 0, 0) == RT_ROLE_LIGHTMAP);
    s.tcGen = TCGEN_TEXTURE; s.images[0] = &lightmapImage; assert(RT_StageRole(&s, 0, 0) == RT_ROLE_LIGHTMAP);
    s.images[0] = &plain;
    /* an additive environment map after the lighting term is a fake reflection */
    s.tcGen = TCGEN_ENVIRONMENT_MAPPED; s.stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
    assert(RT_StageRole(&s, 1, 0) == RT_ROLE_ENVIRONMENT);
    s.tcGen = TCGEN_ENVIRONMENT_MAPPED_FP; assert(RT_StageRole(&s, 1, 0) == RT_ROLE_ENVIRONMENT);
    /* ... but not before it, not without a lighting term, and not when it
       replaces or filters rather than adds */
    assert(RT_StageRole(&s, 0, 1) == RT_ROLE_BASE);
    assert(RT_StageRole(&s, 1, -1) == RT_ROLE_BASE);
    s.stateBits = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO; assert(RT_StageRole(&s, 1, 0) == RT_ROLE_BASE);
    /* an additive glow after the lighting term blends like any stage: the
       two-pass evaluation finds it independent of the light */
    s.tcGen = TCGEN_TEXTURE; s.stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
    assert(RT_StageRole(&s, 1, 0) == RT_ROLE_BASE);
    /* fog stages are raster's */
    s.tcGen = TCGEN_FOG; assert(RT_StageRole(&s, 1, 0) == RT_ROLE_SKIP);
    s.tcGen = TCGEN_TEXTURE; s.rgbGen = CGEN_FOG; assert(RT_StageRole(&s, 1, 0) == RT_ROLE_SKIP);
    return 0;
}
''')

    def test_vertex_layout_and_unlit_geometry_do_not_inherit_lightmap_data(self):
        source = (ROOT / "code/rendererrtx/vk.c").read_text(encoding="utf-8")
        end = source.index("} rtxRtPackedVertex_t;") + len("} rtxRtPackedVertex_t;")
        start = source.rindex("typedef struct {", 0, end)
        vertex = source[start:end]
        signature = "static qboolean vk_rt_cpu_geometry_add_vertex( rtxRtCpuGeometry_t *geometry, const vec3_t xyz, const vec3_t normal, const vec4_t tangent, const vec2_t texCoord, uint32_t color, uint32_t materialIndex, uint32_t *outIndex )"
        self.run_c(r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef int qboolean;
enum {qfalse,qtrue};
typedef float vec2_t[2], vec3_t[3], vec4_t[4];
#define Com_Memset memset
''' + vertex + r'''
typedef struct { rtxRtPackedVertex_t vertices[2]; uint32_t numVertices; } rtxRtCpuGeometry_t;
static qboolean vk_rt_cpu_geometry_reserve(rtxRtCpuGeometry_t *g, uint32_t v, uint32_t i) {
    (void)i; return g->numVertices+v<=2;
}
''' + function(source, signature) + r'''
int main(void) {
    _Static_assert(sizeof(rtxRtPackedVertex_t)==80, "GPU vertex stride");
    _Static_assert(offsetof(rtxRtPackedVertex_t, lightmapData)==64, "GPU lightmap offset");
    rtxRtCpuGeometry_t g; memset(&g,0xa5,sizeof(g)); g.numVertices=0;
    vec3_t xyz={1,2,3}, n={0,0,1}; vec4_t t={1,0,0,1}; vec2_t uv={.2f,.8f}; uint32_t index=7;
    assert(vk_rt_cpu_geometry_add_vertex(&g,xyz,n,t,uv,0xff123456,3,&index));
    assert(index==0 && g.numVertices==1 && g.vertices[0].materialIndex==3);
    for (int i=0;i<4;++i) assert(g.vertices[0].lightmapData[i]==0);
    /* Verify the packed float bytes, independent of x87 excess precision. */
    assert(!memcmp(g.vertices[0].texCoord,uv,sizeof(uv)) && g.vertices[0].xyz[2]==3);
    assert(vk_rt_cpu_geometry_add_vertex(&g,xyz,n,t,uv,0,0,0));
    assert(!vk_rt_cpu_geometry_add_vertex(&g,xyz,n,t,uv,0,0,0));
    assert(g.numVertices==2);
    return 0;
}
''')

    def test_vertex_lit_lightmap_stages_carry_the_vertex_light(self):
        # r_vertexLight (or a surface without a lightmap) binds the white image
        # to $lightmap; the path tracer then takes the vertex colours as the
        # baked light, as raster's VertexLightingCollapse does, and reads the
        # stages as authored, before that collapse.
        material = (ROOT / "code/rendererrtx/rt/rt_material.c").read_text(encoding="utf-8")
        translate = function(material, "static qboolean RT_TranslateMaterial( const shader_t *shader, rtGpuMaterial_t *out, rtMaterialInfo_t *info )")
        self.assertIn("if ( shader->lightmapIndex == LIGHTMAP_BY_VERTEX ) {", translate)
        self.assertIn("vertexLit = qtrue;", translate)
        self.assertIn("( RT_StageIsLightmap( s ) && shader->lightmapIndex == LIGHTMAP_BY_VERTEX )", translate)
        shader = (ROOT / "code/rendererrtx/tr_shader.c").read_text(encoding="utf-8")
        snapshot = shader.index("\tR_RtSnapshotStages( stage );")
        collapse = shader.index("\t\tVertexLightingCollapse();")
        self.assertLess(snapshot, collapse)
        self.assertEqual(shader.count("R_RtSnapshotStages( stage );"), 1)


if __name__ == "__main__":
    unittest.main()
