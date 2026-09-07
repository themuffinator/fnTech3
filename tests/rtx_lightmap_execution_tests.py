"""Execute the native Q3/QL shader-ownership and packed vertex contracts."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature + "\n{")
    return source[start:source.index("\n}", start) + 2]


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

    def test_lightmap_ownership_accepts_only_reproducible_static_composition(self):
        source = (ROOT / "code/rendererrtx/tr_shader.c").read_text(encoding="utf-8")
        native = function(source, "qboolean R_RtShaderNativeSupported( const shader_t *materialShader )")
        self.run_c(r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define USE_VULKAN
typedef int qboolean;
enum { qfalse, qtrue, SS_OPAQUE=3, SURF_SKY=4, IMGFLAG_LIGHTMAP=8,
       LIGHTMAP_INDEX_NONE=-1, LIGHTMAP_BY_VERTEX=-3, TCGEN_LIGHTMAP=3, TCGEN_TEXTURE=4,
       CGEN_IDENTITY=0, CGEN_IDENTITY_LIGHTING=1, CGEN_CONST=2, CGEN_VERTEX=3, CGEN_EXACT_VERTEX=4,
       AGEN_SKIP=0, AGEN_IDENTITY=1, AGEN_CONST=2, GL_MODULATE=10, GL_BLEND_MODULATE=11 };
enum { GLS_DEPTHMASK_TRUE=1, GLS_BLEND_BITS=2, GLS_ATEST_BITS=4,
       GLS_DEPTHTEST_DISABLE=8, GLS_DEPTHFUNC_EQUAL=16, GLS_POLYMODE_LINE=32 };
typedef struct { int flags; } image_t;
typedef struct { int tcGen, lightmap, numImageAnimations, numTexMods, isVideoMap,
    isScreenMap, rgbGen, alphaGen, vertexLightmap; image_t *image[2]; struct { unsigned char rgba[4]; } constantColor;
} textureBundle_t;
typedef struct { int active, depthFragment; unsigned stateBits, numTexBundles;
    int mtEnv, mtEnv3; textureBundle_t bundle[3]; } shaderStage_t;
typedef struct shader_s { struct shader_s *remappedShader; int sort, isSky,
    surfaceFlags, polygonOffset, numDeforms, hasScreenMap, numUnfoggedPasses,
    explicitlyDefined, lightmapIndex;
    shaderStage_t *stages[2]; } shader_t;
''' + native + r'''
int main(void) {
    image_t base={0}, light={IMGFLAG_LIGHTMAP};
    shaderStage_t stage={0}; shader_t shader={0};
    shader.sort=SS_OPAQUE; shader.numUnfoggedPasses=1; shader.stages[0]=&stage;
    stage.active=1; stage.stateBits=GLS_DEPTHMASK_TRUE; stage.numTexBundles=1;
    stage.bundle[0].image[0]=&base; stage.bundle[0].lightmap=LIGHTMAP_INDEX_NONE;
    stage.bundle[0].tcGen=TCGEN_TEXTURE;
    assert(R_RtShaderNativeSupported(&shader));
    /* Renderer-generated vertex lighting is baked irradiance, while an
     * arbitrary authored rgbGen vertex stage remains outside native scope. */
    for (int generator=CGEN_VERTEX; generator<=CGEN_EXACT_VERTEX; ++generator) {
        stage.bundle[0].rgbGen=generator;
        assert(!R_RtShaderNativeSupported(&shader));
        stage.bundle[0].vertexLightmap=1;
        assert(R_RtShaderNativeSupported(&shader));
        stage.bundle[0].numTexMods=1; assert(!R_RtShaderNativeSupported(&shader));
        stage.bundle[0].numTexMods=0;
        stage.bundle[0].numImageAnimations=2; assert(!R_RtShaderNativeSupported(&shader));
        stage.bundle[0].numImageAnimations=0;
        stage.bundle[0].alphaGen=99; assert(!R_RtShaderNativeSupported(&shader));
        stage.bundle[0].alphaGen=AGEN_SKIP;
        stage.bundle[0].vertexLightmap=0;
    }
    /* R_CreateDefaultShading directly creates implicit prelit materials;
     * it does not set the optional collapse-only vertexLightmap marker. */
    shader.lightmapIndex=LIGHTMAP_BY_VERTEX;
    stage.bundle[0].rgbGen=CGEN_EXACT_VERTEX;
    assert(R_RtShaderNativeSupported(&shader));
    shader.explicitlyDefined=1; assert(!R_RtShaderNativeSupported(&shader));
    shader.explicitlyDefined=0;
    stage.bundle[0].rgbGen=CGEN_VERTEX; assert(!R_RtShaderNativeSupported(&shader));
    shader.lightmapIndex=0;
    stage.bundle[0].rgbGen=CGEN_IDENTITY;
    stage.numTexBundles=2; stage.mtEnv=GL_MODULATE;
    stage.bundle[1].image[0]=&light; stage.bundle[1].tcGen=TCGEN_LIGHTMAP;
    assert(R_RtShaderNativeSupported(&shader));
    stage.bundle[0].rgbGen=CGEN_EXACT_VERTEX; stage.bundle[0].vertexLightmap=1;
    assert(!R_RtShaderNativeSupported(&shader));
    stage.bundle[0].rgbGen=CGEN_IDENTITY; stage.bundle[0].vertexLightmap=0;
    /* The real collapse selects the separate-color multiplication variant
     * when base identity-lighting differs from lightmap identity. */
    stage.mtEnv=GL_BLEND_MODULATE;
    stage.bundle[0].rgbGen=CGEN_IDENTITY_LIGHTING;
    assert(R_RtShaderNativeSupported(&shader));
    stage.bundle[0].rgbGen=CGEN_CONST;
    assert(R_RtShaderNativeSupported(&shader));
    stage.bundle[0].rgbGen=99;
    assert(!R_RtShaderNativeSupported(&shader));
    stage.bundle[0].rgbGen=CGEN_IDENTITY;
    stage.mtEnv3=GL_BLEND_MODULATE;
    assert(!R_RtShaderNativeSupported(&shader));
    stage.mtEnv3=0;
    textureBundle_t tmp=stage.bundle[0]; stage.bundle[0]=stage.bundle[1]; stage.bundle[1]=tmp;
    assert(R_RtShaderNativeSupported(&shader));
    stage.bundle[0].rgbGen=CGEN_IDENTITY_LIGHTING;
    assert(R_RtShaderNativeSupported(&shader));
    stage.bundle[0].rgbGen=CGEN_CONST; assert(!R_RtShaderNativeSupported(&shader));
    stage.bundle[0].rgbGen=CGEN_IDENTITY;
    for (int which=0;which<2;++which) {
        stage.bundle[which].numImageAnimations=2; assert(!R_RtShaderNativeSupported(&shader));
        stage.bundle[which].numImageAnimations=0;
        stage.bundle[which].numTexMods=1; assert(!R_RtShaderNativeSupported(&shader));
        stage.bundle[which].numTexMods=0;
        stage.bundle[which].isVideoMap=1; assert(!R_RtShaderNativeSupported(&shader));
        stage.bundle[which].isVideoMap=0;
        stage.bundle[which].isScreenMap=1; assert(!R_RtShaderNativeSupported(&shader));
        stage.bundle[which].isScreenMap=0;
    }
    stage.mtEnv=123; assert(!R_RtShaderNativeSupported(&shader)); stage.mtEnv=GL_MODULATE;
    for (unsigned bit=2;bit<=32;bit*=2) {
        stage.stateBits=GLS_DEPTHMASK_TRUE|bit; assert(!R_RtShaderNativeSupported(&shader));
    }
    stage.stateBits=GLS_DEPTHMASK_TRUE;
    shader.numDeforms=1; assert(!R_RtShaderNativeSupported(&shader)); shader.numDeforms=0;
    shader.numUnfoggedPasses=2; assert(!R_RtShaderNativeSupported(&shader)); shader.numUnfoggedPasses=1;
    stage.bundle[1]=stage.bundle[0]; assert(!R_RtShaderNativeSupported(&shader));
    assert(!R_RtShaderNativeSupported(0));
    return 0;
}
''')

    def test_baked_vertex_irradiance_is_separate_from_base_albedo(self):
        source = (ROOT / "code/rendererrtx/vk.c").read_text(encoding="utf-8")
        end = source.index("} rtxRtMaterial_t;") + len("} rtxRtMaterial_t;")
        start = source.rindex("typedef struct {", 0, end)
        material = source[start:end]
        signature = ("static void vk_rt_apply_native_bundle_lighting( const shader_t *shader,\n"
                     "\tconst textureBundle_t *bundle, qboolean lightmap, rtxRtMaterial_t *material )")
        self.run_c(r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define MAX_QPATH 64
#define VectorSet(v,x,y,z) ((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
typedef int qboolean;
enum {qfalse,qtrue,CGEN_IDENTITY_LIGHTING=2,CGEN_EXACT_VERTEX=3,CGEN_VERTEX=4,CGEN_CONST=5,LIGHTMAP_BY_VERTEX=-3};
typedef struct { int index; } image_t;
typedef struct { int supported, explicitlyDefined, lightmapIndex; } shader_t;
typedef struct { int vertexLightmap, rgbGen; image_t *image[1]; } textureBundle_t;
static struct { float identityLight; } tr = {.25f};
static qboolean R_RtShaderNativeSupported(const shader_t *shader) {
    return shader && shader->supported;
}
static int32_t vk_rt_image_to_scene_texture_index(const image_t *image) {
    return image ? image->index : -1;
}
''' + material + function(source, signature) + r'''
static rtxRtMaterial_t blank(void) {
    rtxRtMaterial_t material; memset(&material,0,sizeof(material));
    for (int i=0;i<4;++i) material.baseColor[i]=1;
    material.lightmapTextureIndex=-1; material.lightmapScale=1;
    return material;
}
int main(void) {
    shader_t shader={1}; image_t image={23};
    textureBundle_t bundle={1,CGEN_EXACT_VERTEX,{&image}};
    rtxRtMaterial_t material=blank();
    vk_rt_apply_native_bundle_lighting(&shader,&bundle,qfalse,&material);
    assert(material.vertexLighting==1 && material.lightmapTextureIndex==-1);
    assert(material.lightmapScale==1 && material.baseColor[0]==1 && material.baseColor[3]==1);
    material=blank(); bundle.rgbGen=CGEN_VERTEX;
    vk_rt_apply_native_bundle_lighting(&shader,&bundle,qfalse,&material);
    assert(material.vertexLighting==1 && material.lightmapScale==.25f);
    assert(material.baseColor[0]==1 && material.baseColor[1]==1 && material.baseColor[2]==1);
    material=blank(); shader.supported=0;
    vk_rt_apply_native_bundle_lighting(&shader,&bundle,qfalse,&material);
    assert(material.vertexLighting==0 && material.lightmapTextureIndex==-1 && material.lightmapScale==1);
    shader.supported=1; bundle.vertexLightmap=0;
    vk_rt_apply_native_bundle_lighting(&shader,&bundle,qfalse,&material);
    assert(material.vertexLighting==0 && material.lightmapScale==1);
    shader.lightmapIndex=LIGHTMAP_BY_VERTEX; bundle.rgbGen=CGEN_EXACT_VERTEX;
    vk_rt_apply_native_bundle_lighting(&shader,&bundle,qfalse,&material);
    assert(material.vertexLighting==1 && material.lightmapScale==1 && material.lightmapTextureIndex==-1);
    assert(material.baseColor[0]==1);
    material=blank(); shader.explicitlyDefined=1;
    vk_rt_apply_native_bundle_lighting(&shader,&bundle,qfalse,&material);
    assert(material.vertexLighting==0);
    shader.explicitlyDefined=0; shader.lightmapIndex=0;
    bundle.rgbGen=CGEN_IDENTITY_LIGHTING;
    vk_rt_apply_native_bundle_lighting(&shader,&bundle,qtrue,&material);
    assert(material.vertexLighting==0 && material.lightmapTextureIndex==23 && material.lightmapScale==.25f);
    assert(material.baseColor[0]==1);
    material=blank();
    vk_rt_apply_native_bundle_lighting(&shader,&bundle,qfalse,&material);
    assert(material.vertexLighting==0 && material.lightmapTextureIndex==-1);
    assert(material.baseColor[0]==.25f && material.baseColor[1]==.25f && material.baseColor[2]==.25f);
    assert(material.baseColor[3]==1);
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


if __name__ == "__main__":
    unittest.main()
