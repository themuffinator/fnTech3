"""Execute RTX draw-list filtering and uniform allocation without a GPU."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


class RtxRasterExecutionTests(unittest.TestCase):
    def test_depth_base_preserves_cutouts_and_equal_depth_dependencies(self):
        shade = (ROOT / 'code/rendererrtx/tr_shade.c').read_text()
        fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define Com_Memset memset
typedef int qboolean;
enum {qfalse, qtrue, MAX_SHADER_STAGES=8, TYPE_COLOR_WHITE=1, PV_MIRROR=2,
      GLS_DEPTHMASK_TRUE=1, GLS_ATEST_BITS=2, GLS_ALICE_ATEST_BITS=4,
      GLS_DEPTHFUNC_EQUAL=8, GLS_SRCBLEND_BITS=16, GLS_DSTBLEND_BITS=32, TESS_XYZ=1};
typedef struct {unsigned stateBits; int skip;} shaderStage_t;
typedef struct {int cullType, polygonOffset;} shader_t;
typedef struct {unsigned state_bits; int shader_type, face_culling, polygon_offset, mirror,
    depth_only, fog_stage, depth_fade;} Vk_Pipeline_Def;
static struct {shaderStage_t *xstages[8]; shader_t *shader; int depthRange;} tess;
static struct {struct {int portalView;} viewParms;} backEnd;
static Vk_Pipeline_Def sourceDef, boundDef;
static int draws, attributes;
static int RB_AliceSkipStage(const shaderStage_t *s) {return s->skip;}
static uint32_t vk_find_pipeline_ext(int, const Vk_Pipeline_Def *d, int) {boundDef=*d;return 1;}
static void vk_get_pipeline_def(uint32_t, Vk_Pipeline_Def *d) {*d=sourceDef;}
static void vk_bind_pipeline(uint32_t) {}
static void vk_bind_index(void) {}
static void vk_bind_geometry(int a) {attributes=a;}
static void vk_draw_geometry(int, int) {draws++;}
''' + function(shade, 'static qboolean VK_RTDepthOpaque(') + function(shade, 'static uint32_t VK_RTDepthPipeline(') + r'''
int main(void) {
    shader_t shader={3,1}; shaderStage_t cutout={GLS_DEPTHMASK_TRUE|GLS_ATEST_BITS,0};
    shaderStage_t equal={GLS_DEPTHMASK_TRUE|GLS_DEPTHFUNC_EQUAL,0};
    tess.shader=&shader; tess.xstages[0]=&cutout; tess.xstages[1]=&equal;
    assert(!VK_RTDepthOpaque() && draws==0); // EQUAL must follow the actual alpha coverage
    sourceDef.state_bits=cutout.stateBits|GLS_SRCBLEND_BITS|GLS_DSTBLEND_BITS;
    sourceDef.fog_stage=sourceDef.depth_fade=1; sourceDef.face_culling=3;
    VK_RTDepthPipeline(1);
    assert(boundDef.depth_only && boundDef.state_bits==cutout.stateBits);
    assert(!boundDef.fog_stage && !boundDef.depth_fade && boundDef.face_culling==3);
    equal.stateBits=GLS_DEPTHMASK_TRUE;
    assert(VK_RTDepthOpaque() && draws==1 && attributes==TESS_XYZ);
    assert(boundDef.depth_only && boundDef.shader_type==TYPE_COLOR_WHITE && boundDef.polygon_offset);
    equal.skip=1; assert(!VK_RTDepthOpaque() && draws==1);
    return 0;
}
'''
        # The host uses C++ argument syntax just like several existing fixtures.
        from rtx_surface_owner_tests import compile_run
        compile_run(self, fixture)

    def run_fixture(self, fixture: str) -> None:
        compiler = next((path for name in ("clang", "gcc", "cc")
                         if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C11 compiler")
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="rtx-raster-", dir=scratch) as folder:
            path = Path(folder)
            source = path / "raster.c"
            executable = path / ("raster.exe" if os.name == "nt" else "raster")
            source.write_text(fixture, encoding="utf-8")
            command = [compiler, "-std=c11"]
            if (os.name == "nt" and "clang" in Path(compiler).name.lower()
                    and os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() == "x86"):
                command.append("--target=i686-pc-windows-msvc")
            build = subprocess.run([*command, str(source), "-o", str(executable)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_personal_shadow_models_never_enter_color_batches_or_liquid_snapshots(self) -> None:
        source = (ROOT / "code/rendererrtx/tr_backend.c").read_text(encoding="utf-8")
        header = (ROOT / "code/rendererrtx/tr_local.h").read_text(encoding="utf-8")
        flag = re.search(r"^#define DSF_SHADOW_CASTER_ONLY .+$", header, re.M).group(0)
        mode = source[source.index("typedef enum {\n\tRB_DRAWSURFS_ALL"):
                      source.index("} rbDrawSurfsMode_t;") + len("} rbDrawSurfsMode_t;")]
        vk_header = (ROOT / "code/rendererrtx/vk.h").read_text(encoding="utf-8")
        owner = vk_header[vk_header.index("typedef enum {\n\tRT_SURFACE_RASTER"):
                          vk_header.index("} rtSurfaceOwner_t;") + len("} rtSurfaceOwner_t;")]
        fog_overlay = re.search(r"^static qboolean rb_rtFogOverlay;$", source, re.M).group(0)
        fixture = r'''
#include <assert.h>
#include <limits.h>
#include <string.h>
#define USE_VULKAN
#define Com_Memcpy memcpy
#define MAX_UINT UINT_MAX
#define QSORT_REFENTITYNUM_MASK 255u
#define VK_NULL_HANDLE 0
typedef int qboolean;
enum { qfalse, qtrue, SF_BAD = 0, REFENTITYNUM_WORLD = 0, RF_DEPTHHACK = 1, RF_CROSSHAIR = 2,
       RENDER_PASS_MAIN = 1, RENDER_PASS_SCREENMAP = 2, RENDER_PASS_POST_BLOOM = 3,
       SS_OPAQUE = 3, SS_FOG = 5, DEPTH_RANGE_NORMAL = 0, DEPTH_RANGE_WEAPON = 1 };
typedef struct { int integer; float value; } cvar_t;
static cvar_t enabled = {1, 1.0f}, disabled = {0, 0.0f};
static cvar_t *r_liquid = &enabled, *r_liquidRefraction = &enabled,
              *r_liquidReflection = &enabled;
static struct { cvar_t *forceMergeEntities; } qlRendererCvars = { &disabled };
typedef struct { int entityMergable, nativeSupported, liquid; float sort, timeOffset; } shader_t;
/* the engine's surfaceType_t is the int a surface pointer points at */
typedef int surfaceType_t;
typedef struct { surfaceType_t type; int id; } fixtureSurface_t;
typedef struct { unsigned sort, flags; surfaceType_t *surface; } drawSurf_t;
typedef struct { float modelMatrix[16]; } orientation_t;
typedef struct {
    int intShaderTime;
    struct { int renderfx; union { int i; float f; } shaderTime; } e;
} entity_t;
typedef struct { orientation_t world; } viewParms_t;
static struct {
    struct { double floatTime; entity_t entities[4]; } refdef;
    entity_t *currentEntity; viewParms_t viewParms; orientation_t or;
    struct { int c_surfaces; } pc;
} backEnd;
static struct { entity_t worldEntity; } tr;
static struct { shader_t *shader; double shaderTime; int depthRange; int rtFogOnly, rtDepthOnly; } tess;
static struct { int fboActive, renderPassIndex;
                struct { int color_descriptor; } liquidSnapshot; } vk = {1, RENDER_PASS_MAIN, {1}};
static struct { float modelview_transform[16]; } vk_world;
/* nativeSupported: 0 raster, 1 traced, 2 traced inside a fog volume */
/* shaders[4]: traced, sorted past opaque (an alpha-tested see-through surface) */
static shader_t shaders[5] = {{0}, {0, 1, 0, SS_OPAQUE, 0}, {0, 0, 0, SS_OPAQUE, 0}, {0, 2, 0, SS_OPAQUE, 0},
                              {0, 1, 0, SS_OPAQUE + 1, 0}};
static int rendered[16], fogOnlyAtDraw[16], depthOnlyAtDraw[16], renderedCount, snapshotChecks, decompositions;
static void R_DecomposeSort(unsigned sort, int *entity, shader_t **shader, int *fog, int *dlight) {
    ++decompositions; *entity = sort & 255; *shader = &shaders[sort >> 8]; *fog = 0; *dlight = 0;
}
static int RB_ShaderNeedsLiquidSnapshot(const shader_t *shader) { ++snapshotChecks; return shader->liquid; }
static int RB_IsPrimaryFullView(void) { return 1; }
static int vk_depth_fade_ready(void) { return 0; }
static int depthFadeAvailable, depthFadeCopies;
static int vk_depth_fade_available(void) { return depthFadeAvailable; }
static void vk_copy_depth_fade(void) { ++depthFadeCopies; }
static void RB_EndSurface(void) {}
// Elite Force's forced entity alpha changes how a batch is drawn, not which.
static void RB_EndSurfaceWithSTVEFAlpha(void) { RB_EndSurface(); }
static void RB_BeginSurface(shader_t *shader, int fog) { (void)fog; tess.shader = shader; }
static void RB_DrawWorldCelOutlineForScene(qboolean *drawn) { *drawn = qtrue; }
static int vk_capture_liquid_scene(void) { assert(0); return 0; }
static void R_RotateForEntity(entity_t *entity, viewParms_t *view, orientation_t *out) {
    (void)entity; *out = view->world;
}
static void vk_update_mvp(const void *matrix) { (void)matrix; }
static void Draw(surfaceType_t *surface) {
    fogOnlyAtDraw[renderedCount] = tess.rtFogOnly;
    depthOnlyAtDraw[renderedCount] = tess.rtDepthOnly;
    rendered[renderedCount++] = ((fixtureSurface_t *)surface)->id;
}
static void (*rb_surfaceTable[])(surfaceType_t *) = {Draw};
''' + flag + '\n' + mode + '\n' + owner + '\n' + fog_overlay + r'''
static rtSurfaceOwner_t vk_rt_surface_owner(int entityNum, const shader_t *shader, int fogNum, int surfaceType) {
    (void)fogNum; (void)surfaceType;
    /* this fixture's entity surfaces stay with raster */
    if (entityNum != REFENTITYNUM_WORLD) return RT_SURFACE_RASTER;
    return shader->nativeSupported == 2 ? RT_SURFACE_TRACED_FOGGED :
        shader->nativeSupported ? RT_SURFACE_TRACED : RT_SURFACE_RASTER;
}
''' + '\n'.join(function(source, signature) for signature in (
            "static qboolean RB_DrawSurfIncluded(",
            "static qboolean RB_DrawSurfListNeedsLiquidSnapshot(",
            "static void RB_RenderDrawSurfList(")) + r'''
static void Expect(drawSurf_t *draws, int count, rbDrawSurfsMode_t mode, const int *ids, int numIds) {
    int i;
    renderedCount = 0;
    backEnd.refdef.floatTime = 15.0;
    RB_RenderDrawSurfList(draws, count, mode);
    assert(renderedCount == numIds);
    for (i = 0; i < numIds; ++i) assert(rendered[i] == ids[i]);
    assert(backEnd.refdef.floatTime == 15.0);
}
int main(void) {
    fixtureSurface_t surfaces[8];
    drawSurf_t draws[8];
    unsigned sorts[] = {256, 256, 256, 256, 512, 257, 512, 257};
    int expectedAll[] = {1, 3, 4, 5}, expectedBase[] = {1, 3}, expectedOverlay[] = {4, 5};
    int i, mode;
    for (i = 0; i < 8; ++i) {
        surfaces[i].type = 0; surfaces[i].id = i;
        draws[i].sort = sorts[i]; draws[i].surface = &surfaces[i].type;
        draws[i].flags = (i == 0 || i == 2 || i >= 6) ? DSF_SHADOW_CASTER_ONLY : 0;
    }
    /* A caster preceding and sharing the key of a visible batch must be skipped,
     * as must one following that batch through the equal-sort fast path. */
    Expect(draws, 8, RB_DRAWSURFS_ALL, expectedAll, 4);
    Expect(draws, 8, RB_DRAWSURFS_RT_BASE, expectedBase, 2);
    assert(depthOnlyAtDraw[0] && depthOnlyAtDraw[1]);
    assert(!tess.rtDepthOnly);
    Expect(draws, 8, RB_DRAWSURFS_RT_OVERLAY, expectedOverlay, 2);
    assert(!depthOnlyAtDraw[0] && !depthOnlyAtDraw[1]);
    for (mode = RB_DRAWSURFS_ALL; mode <= RB_DRAWSURFS_RT_OVERLAY; ++mode) {
        Expect(draws, 1, (rbDrawSurfsMode_t)mode, NULL, 0);
        Expect(draws, 0, (rbDrawSurfsMode_t)mode, NULL, 0);
        shaders[1].liquid = 1;
        snapshotChecks = decompositions = 0;
        assert(!RB_DrawSurfListNeedsLiquidSnapshot(draws, 1, (rbDrawSurfsMode_t)mode));
        assert(snapshotChecks == 0 && decompositions == 0);
        shaders[1].liquid = 0;
    }
    shaders[1].liquid = 1;
    assert(RB_DrawSurfListNeedsLiquidSnapshot(draws + 1, 1, RB_DRAWSURFS_ALL));
    assert(RB_DrawSurfListNeedsLiquidSnapshot(draws + 1, 1, RB_DRAWSURFS_RT_BASE));
    assert(!RB_DrawSurfListNeedsLiquidSnapshot(draws + 1, 1, RB_DRAWSURFS_RT_OVERLAY));
    /* Visibility filtering must preserve the list for subsequent shadow passes. */
    assert(draws[0].flags == DSF_SHADOW_CASTER_ONLY && draws[2].flags == DSF_SHADOW_CASTER_ONLY);
    /* A traced surface inside fog: the base pass draws it; the overlay returns
     * it, for its fog pass alone, only after a completed trace. */
    {
        drawSurf_t fogged[2];
        int expectedFogged[] = {6, 7};
        fogged[0].sort = fogged[1].sort = 768;
        fogged[0].surface = &surfaces[6].type; fogged[1].surface = &surfaces[7].type;
        fogged[0].flags = fogged[1].flags = 0;
        rb_rtFogOverlay = qfalse;
        Expect(fogged, 2, RB_DRAWSURFS_RT_BASE, expectedFogged, 2);
        assert(!fogOnlyAtDraw[0] && !fogOnlyAtDraw[1]);
        assert(depthOnlyAtDraw[0] && depthOnlyAtDraw[1]);
        Expect(fogged, 2, RB_DRAWSURFS_RT_OVERLAY, NULL, 0);
        rb_rtFogOverlay = qtrue;
        Expect(fogged, 2, RB_DRAWSURFS_RT_OVERLAY, expectedFogged, 2);
        assert(fogOnlyAtDraw[0] && fogOnlyAtDraw[1]);
        assert(!tess.rtFogOnly);
        rb_rtFogOverlay = qfalse;
    }
    /* A traced surface sorted past opaque, in a map with depth fade: the base
     * pass before the trace leaves the depth snapshot (and the render pass it
     * opens) to the overlay list; a raster frame takes it there. */
    {
        drawSurf_t seeThrough[1];
        int expectedSeeThrough[] = {5};
        seeThrough[0].sort = 1024; seeThrough[0].surface = &surfaces[5].type; seeThrough[0].flags = 0;
        depthFadeAvailable = 1;
        depthFadeCopies = 0;
        Expect(seeThrough, 1, RB_DRAWSURFS_RT_BASE, expectedSeeThrough, 1);
        assert(depthFadeCopies == 0);
        Expect(seeThrough, 1, RB_DRAWSURFS_ALL, expectedSeeThrough, 1);
        assert(depthFadeCopies == 1);
        depthFadeAvailable = 0;
    }
    return 0;
}
'''
        self.run_fixture(fixture)

    def test_uniform_exhaustion_preserves_binding_and_requests_recovery(self) -> None:
        source = (ROOT / "code/rendererrtx/tr_shade.c").read_text(encoding="utf-8")
        fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define Com_Memcpy memcpy
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
typedef uint64_t VkDeviceSize;
typedef struct { unsigned char data[64]; } vkUniform_t;
enum { VK_DESC_UNIFORM = 0 };
static unsigned char buffer[512];
typedef struct {
    VkDeviceSize vertex_buffer_offset; uint32_t uniform_read_offset;
    unsigned char *vertex_buffer_ptr; int uniform_descriptor;
} command_t;
static command_t cmd;
static struct {
    command_t *cmd;
    uint32_t uniform_alignment, uniform_item_size;
    VkDeviceSize geometry_buffer_size, geometry_buffer_size_new;
} vk;
static int bindings;
static uint32_t boundOffset;
static void vk_reset_descriptor(int set) { assert(set == VK_DESC_UNIFORM); ++bindings; }
static void vk_update_descriptor(int set, int descriptor) {
    assert(set == VK_DESC_UNIFORM && descriptor == 7); ++bindings;
}
static void vk_update_descriptor_offset(int set, uint32_t offset) {
    assert(set == VK_DESC_UNIFORM); boundOffset = offset; ++bindings;
}
'''
        fixture += function(source, "uint32_t VK_PushUniform( const vkUniform_t *uniformData ) {") + r'''
static void Reset(void) {
    memset(buffer, 0xcc, sizeof(buffer)); memset(&cmd, 0, sizeof(cmd)); memset(&vk, 0, sizeof(vk));
    vk.cmd = &cmd; vk.uniform_alignment = 64; vk.uniform_item_size = 64;
    vk.geometry_buffer_size = sizeof(buffer); cmd.vertex_buffer_ptr = buffer;
    cmd.uniform_descriptor = 7; cmd.uniform_read_offset = 192; boundOffset = 192; bindings = 0;
}
static void Reject(VkDeviceSize offset, const vkUniform_t *data) {
    unsigned char original[sizeof(buffer)]; memcpy(original, buffer, sizeof(buffer));
    cmd.vertex_buffer_offset = offset;
    assert(VK_PushUniform(data) == ~0u);
    assert(cmd.vertex_buffer_offset == offset && cmd.uniform_read_offset == 192);
    assert(bindings == 0 && boundOffset == 192 && !memcmp(original, buffer, sizeof(buffer)));
}
int main(void) {
    vkUniform_t data; int i; memset(&data, 0x5a, sizeof(data));
    Reset(); cmd.vertex_buffer_offset = 1;
    assert(VK_PushUniform(&data) == 64 && cmd.vertex_buffer_offset == 128);
    assert(cmd.uniform_read_offset == 64 && boundOffset == 64 && bindings == 3);
    assert(!memcmp(buffer + 64, &data, sizeof(data)));
    for (i = 0; i < 64; ++i) assert(buffer[i] == 0xcc);
    Reset(); cmd.vertex_buffer_offset = 448;
    assert(VK_PushUniform(&data) == 448 && cmd.vertex_buffer_offset == sizeof(buffer));
    Reset(); Reject(449, &data); assert(vk.geometry_buffer_size_new >= 576);
    /* An earlier geometry overflow defers all uniform writes until resize. */
    Reset(); vk.geometry_buffer_size_new = 1024; Reject(1, &data);
    assert(vk.geometry_buffer_size_new == 1024);
    Reset(); Reject(UINT64_MAX, &data); assert(!vk.geometry_buffer_size_new);
    Reset(); Reject(UINT32_MAX, &data); assert(!vk.geometry_buffer_size_new);
    Reset(); vk.uniform_alignment = 0; Reject(0, &data);
    Reset(); vk.uniform_alignment = 3; Reject(0, &data);
    Reset(); vk.uniform_item_size = 1; Reject(0, &data);
    /* A wide arena must not allow a 32-bit end offset to wrap into its start. */
    Reset(); vk.geometry_buffer_size = UINT64_C(1) << 33; Reject(UINT32_MAX - 63, &data);
    /* After the existing frame-end resize, the formerly failing upload succeeds. */
    Reset(); vk.geometry_buffer_size = 64; Reject(1, &data);
    assert(vk.geometry_buffer_size_new >= 128);
    vk.geometry_buffer_size = vk.geometry_buffer_size_new; vk.geometry_buffer_size_new = 0;
    assert(VK_PushUniform(&data) == 64 && cmd.uniform_read_offset == 64);
    return 0;
}
'''
        self.run_fixture(fixture)

    def test_geometry_capacity_excludes_driver_allocation_padding_in_both_backends(self) -> None:
        harness = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint64_t VkDeviceSize;
typedef unsigned char byte;
typedef struct { VkDeviceSize size; uint32_t memoryTypeBits; } VkMemoryRequirements;
typedef struct {
    int sType; const void *pNext; unsigned flags, sharingMode, queueFamilyIndexCount;
    const uint32_t *pQueueFamilyIndices; VkDeviceSize size; unsigned usage;
} VkBufferCreateInfo;
typedef struct {
    int sType; const void *pNext; VkDeviceSize allocationSize; uint32_t memoryTypeIndex;
} VkMemoryAllocateInfo;
typedef struct { int memory; } allocation_t;
typedef struct {
    int vertex_buffer; byte *vertex_buffer_ptr; VkDeviceSize vertex_buffer_offset;
} command_t;
enum { NUM_COMMAND_BUFFERS = 2, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
       VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, VK_SHARING_MODE_EXCLUSIVE,
       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 1, VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 2,
       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 4, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 1,
       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 2, VK_MEMORY_CATEGORY_GEOMETRY = 1,
       RTX_VK_RESOURCE_GEOMETRY_BUFFERS = 1 };
#define VK_WHOLE_SIZE UINT64_MAX
#define VK_CHECK(call) assert((call) == 0)
#define Com_Memset memset
#define Com_Memcpy memcpy
#define SET_OBJECT_NAME(...) ((void)0)
static struct {
    int device, geometry_buffer_memory; allocation_t geometry_buffer_allocation;
    command_t tess[NUM_COMMAND_BUFFERS], *cmd;
    VkDeviceSize geometry_buffer_size, geometry_buffer_size_new;
    struct { int marker; } stats;
} vk;
static struct { uint32_t indexes[1]; } tess;
static byte mapped[512];
static VkDeviceSize requestedSize, allocationSize, mappedSize;
static int created, bound;
static int qvkCreateBuffer(int device, const VkBufferCreateInfo *desc, const void *allocator, int *buffer) {
    (void)device; (void)allocator;
    assert(desc->size == requestedSize);
    *buffer = ++created; return 0;
}
static void qvkGetBufferMemoryRequirements(int device, int buffer, VkMemoryRequirements *req) {
    (void)device; assert(buffer > 0 && buffer <= NUM_COMMAND_BUFFERS);
    /* A legal driver can require a larger allocation than the buffer range. */
    req->size = 256; req->memoryTypeBits = 7;
}
static uint32_t find_memory_type(uint32_t bits, uint32_t flags) {
    assert(bits == 7 && flags == 3); return 2;
}
static int qvkAllocateMemory(int device, const VkMemoryAllocateInfo *desc, const void *allocator, int *memory) {
    (void)device; (void)allocator; assert(desc->memoryTypeIndex == 2);
    allocationSize = desc->allocationSize; *memory = 9; return 0;
}
static void vk_allocate_memory(allocation_t *allocation, VkDeviceSize size, uint32_t type,
    int category, const char *name, const void *flags) {
    (void)name; (void)flags; assert(type == 2 && category == VK_MEMORY_CATEGORY_GEOMETRY);
    allocationSize = size; allocation->memory = 9;
}
static int qvkMapMemory(int device, int memory, VkDeviceSize offset, VkDeviceSize size, int flags, void **data) {
    (void)device; (void)flags; assert(memory == 9 && offset == 0);
    mappedSize = size; *data = mapped; return 0;
}
static int qvkBindBufferMemory(int device, int buffer, int memory, VkDeviceSize offset) {
    (void)device; assert(memory == 9 && buffer == bound + 1);
    assert(offset == (VkDeviceSize)bound * 256); ++bound; return 0;
}
static void vk_reset_transient_stats(void) {}
static void vk_lifetime_mark_create(int resource, const char *name) { (void)resource; (void)name; }
static uint32_t log2pad(uint32_t size, int step) {
    uint32_t result = 1; (void)step; while (result < size) result *= 2; return result;
}
'''
        checks = r'''
int main(void) {
    uint32_t index = 0x12345678; byte before[sizeof(mapped)];
    requestedSize = 65;
    memset(mapped, 0xcd, sizeof(mapped));
    vk_create_geometry_buffers(requestedSize);
    assert(created == NUM_COMMAND_BUFFERS && bound == NUM_COMMAND_BUFFERS);
    assert(allocationSize == sizeof(mapped) && mappedSize == VK_WHOLE_SIZE);
    assert(vk.geometry_buffer_size == requestedSize);
    assert(vk.tess[0].vertex_buffer_ptr == mapped);
    assert(vk.tess[1].vertex_buffer_ptr == mapped + 256);
    vk.cmd = &vk.tess[1];
    vk.cmd->vertex_buffer_offset = 60;
    assert(vk_tess_index(1, &index) == 60);
    assert(!memcmp(mapped + 256 + 60, &index, sizeof(index)));
    assert(vk.cmd->vertex_buffer_offset == 64);
    memcpy(before, mapped, sizeof(mapped));
    /* Bytes 65..255 belong to allocation padding, not to this VkBuffer. */
    assert(vk_tess_index(1, &index) == ~0u);
    assert(vk.cmd->vertex_buffer_offset == 64);
    assert(vk.geometry_buffer_size_new >= 68);
    assert(!memcmp(before, mapped, sizeof(mapped)));
    return 0;
}
'''
        for renderer in ("renderervk", "rendererrtx"):
            with self.subTest(renderer=renderer):
                source = (ROOT / "code" / renderer / "vk.c").read_text(encoding="utf-8")
                self.run_fixture(harness + '\n'.join(function(source, signature) for signature in (
                    "static void vk_create_geometry_buffers( VkDeviceSize size )",
                    "uint32_t vk_tess_index(")) + checks)


if __name__ == "__main__":
    unittest.main()
