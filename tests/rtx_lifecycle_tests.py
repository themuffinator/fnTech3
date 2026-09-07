"""Execute RTX attachment barriers and descriptor recreation against Vulkan stubs."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VK_SOURCE = ROOT / "code/rendererrtx/vk.c"
PREAMBLE = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define VK_NO_PROTOTYPES
#define VK_USE_64_BIT_PTR_DEFINES 0
#include "vulkan/vulkan_core.h"
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define Com_Memset memset
enum { qfalse, qtrue };
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


class RtxLifecycleTests(unittest.TestCase):
    def run_c(self, fixture):
        compiler = next((path for name in ("clang", "gcc", "cc")
                         if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C11 compiler")
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="rtx-lifecycle-", dir=scratch) as folder:
            path = Path(folder)
            code = path / "lifecycle.c"
            executable = path / ("lifecycle.exe" if os.name == "nt" else "lifecycle")
            code.write_text(PREAMBLE + fixture, encoding="utf-8")
            command = [compiler, "-std=c11", "-I", str(ROOT / "code/renderercommon")]
            if (os.name == "nt" and "clang" in Path(compiler).name.lower()
                    and os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() == "x86"):
                command.append("--target=i686-pc-windows-msvc")
            build = subprocess.run([*command, str(code), "-o", str(executable)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_partial_initialization_shutdown_uses_only_available_resources(self):
        source = VK_SOURCE.read_text(encoding="utf-8")
        release = function(source, "void vk_release_resources( refShutdownCode_t code )")
        fixture = r'''
#define NUM_COMMAND_BUFFERS 3
#define RENDER_PASS_COUNT 3
#define IMAGE_CHUNK_SIZE 1024
#define VK_CHECK(expression) assert((expression) == VK_SUCCESS)
typedef enum { REF_KEEP_CONTEXT, REF_DESTROY_WINDOW } refShutdownCode_t;
typedef struct { unsigned buf_offset[4], vbo_offset[4]; } command_t;
static struct {
    VkDevice device; VkDescriptorPool descriptor_pool; VkCommandBuffer staging_command_buffer;
    int pipelines_world_base, pipelines_count, pipeline_create_count, image_chunk_size;
    struct { VkPipeline handle[RENDER_PASS_COUNT]; unsigned key; } pipelines[4];
    struct { unsigned uniform_read_offset, vertex_buffer_offset; } tess[NUM_COMMAND_BUFFERS];
    command_t *cmd;
    unsigned stats[4];
} vk;
static struct {
    int num_image_chunks, num_samplers;
    struct { VkDeviceMemory memory; unsigned used; } image_chunks[3];
    VkSampler samplers[3];
} vk_world;
static unsigned calls, waits, poolResets, memoryFrees, samplerFrees, pipelineFrees;
static unsigned rtShutdowns, stagingCleans, stagingWaits, commandResets, syncDestroys, syncCreates;
static VkResult VKAPI_PTR wait_device(VkDevice device) {
    assert(device != VK_NULL_HANDLE); ++calls; ++waits; return VK_SUCCESS;
}
static PFN_vkDeviceWaitIdle qvkDeviceWaitIdle = wait_device;
static void vk_wait_idle(void) {
    assert(vk.device != VK_NULL_HANDLE && qvkDeviceWaitIdle != NULL);
    VK_CHECK(qvkDeviceWaitIdle(vk.device));
}
static void vk_rt_shutdown(void) { ++calls; ++rtShutdowns; }
static void vk_clean_staging_buffer(void) { ++calls; ++stagingCleans; }
static void vk_wait_staging_buffer(void) { ++calls; ++stagingWaits; }
static void vk_destroy_sync_primitives(void) { ++calls; ++syncDestroys; }
static void vk_create_sync_primitives(void) { ++calls; ++syncCreates; }
static VkResult qvkResetCommandBuffer(VkCommandBuffer command, VkCommandBufferResetFlags flags) {
    assert(command != VK_NULL_HANDLE && flags == 0); ++calls; ++commandResets; return VK_SUCCESS;
}
static void qvkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator) {
    assert(device == vk.device && memory != VK_NULL_HANDLE && allocator == NULL); ++calls; ++memoryFrees;
}
static void qvkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks *allocator) {
    assert(device == vk.device && sampler != VK_NULL_HANDLE && allocator == NULL); ++calls; ++samplerFrees;
}
static void qvkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *allocator) {
    assert(device == vk.device && pipeline != VK_NULL_HANDLE && allocator == NULL);
    assert(pipeline != 11); ++calls; ++pipelineFrees;
}
static VkResult qvkResetDescriptorPool(VkDevice device, VkDescriptorPool pool, VkDescriptorPoolResetFlags flags) {
    assert(device == vk.device && pool != VK_NULL_HANDLE && flags == 0); ++calls; ++poolResets;
    return VK_SUCCESS;
}
''' + release + r'''
static void configure(void) {
    memset(&vk, 0, sizeof(vk)); memset(&vk_world, 0, sizeof(vk_world));
    calls = waits = poolResets = memoryFrees = samplerFrees = pipelineFrees = 0;
    rtShutdowns = stagingCleans = stagingWaits = commandResets = syncDestroys = syncCreates = 0;
    vk.device = (VkDevice)(uintptr_t)1; qvkDeviceWaitIdle = wait_device;
    vk.staging_command_buffer = (VkCommandBuffer)(uintptr_t)2;
    for (unsigned i = 0; i < NUM_COMMAND_BUFFERS; ++i) {
        vk.tess[i].uniform_read_offset = 7; vk.tess[i].vertex_buffer_offset = 9;
    }
    memset(vk.stats, 1, sizeof(vk.stats));
}
static void check_zero(const void *memory, size_t size) {
    const unsigned char *bytes = memory;
    for (size_t i = 0; i < size; ++i) assert(bytes[i] == 0);
}
int main(void) {
    // Capability rejection can occur before either the device or its dispatch
    // table exists. Repeated shutdown must make no Vulkan calls or state edits.
    for (int missing = 1; missing < 4; ++missing) {
        for (int mode = REF_KEEP_CONTEXT; mode <= REF_DESTROY_WINDOW; ++mode) {
            configure();
            if (missing & 1) vk.device = VK_NULL_HANDLE;
            if (missing & 2) qvkDeviceWaitIdle = NULL;
            unsigned char before[sizeof(vk)]; memcpy(before, &vk, sizeof(vk));
            vk_release_resources(mode); vk_release_resources(mode);
            assert(calls == 0 && memcmp(before, &vk, sizeof(vk)) == 0);
        }
    }
    // A valid device can precede descriptor-pool and active-command creation.
    // Test each independently, then verify full cleanup retains base pipelines.
    for (int resources = 0; resources < 4; ++resources) {
        for (int mode = REF_KEEP_CONTEXT; mode <= REF_DESTROY_WINDOW; ++mode) {
            configure();
            command_t command; memset(&command, 3, sizeof(command));
            if (resources & 1) vk.descriptor_pool = 3;
            if (resources & 2) vk.cmd = &command;
            vk_world.num_image_chunks = 2; vk_world.num_samplers = 2;
            vk_world.image_chunks[0].memory = 4; vk_world.image_chunks[1].memory = 5;
            vk_world.samplers[0] = 6; vk_world.samplers[1] = 7;
            vk.pipelines_world_base = 1; vk.pipelines_count = 3; vk.pipeline_create_count = 4;
            vk.pipelines[0].handle[0] = 11; vk.pipelines[1].handle[0] = 12;
            vk.pipelines[1].handle[2] = 13; vk.pipelines[2].handle[1] = 14;
            vk_release_resources(mode);
            assert(waits == 1 && rtShutdowns == 1 && stagingCleans == 1);
            assert(poolResets == ((resources & 1) != 0));
            assert(memoryFrees == 2 && samplerFrees == 2 && pipelineFrees == 3);
            assert(vk.pipelines_count == 1 && vk.pipeline_create_count == 1);
            assert(vk.pipelines[0].handle[0] == 11 && vk.image_chunk_size == 2 * IMAGE_CHUNK_SIZE);
            check_zero(&vk_world, sizeof(vk_world)); check_zero(vk.tess, sizeof(vk.tess));
            check_zero(vk.stats, sizeof(vk.stats));
            check_zero(&vk.pipelines[1], 2 * sizeof(vk.pipelines[1]));
            if (resources & 2) check_zero(&command, sizeof(command));
#ifdef USE_UPLOAD_QUEUE
            assert(stagingWaits == (mode == REF_KEEP_CONTEXT));
            assert(commandResets == (mode == REF_KEEP_CONTEXT));
            assert(syncDestroys == (mode == REF_KEEP_CONTEXT) && syncCreates == (mode == REF_KEEP_CONTEXT));
#else
            assert(stagingWaits == 0 && commandResets == 0 && syncDestroys == 0 && syncCreates == 0);
#endif
        }
    }
    return 0;
}
'''
        for upload_queue in (False, True):
            with self.subTest(upload_queue=upload_queue):
                self.run_c(("#define USE_UPLOAD_QUEUE\n" if upload_queue else "") + fixture)

    def test_every_combined_depth_stencil_attachment_transitions_both_aspects(self):
        source = VK_SOURCE.read_text(encoding="utf-8")
        transition = function(source, "static void record_image_layout_transition(")
        self.run_c(r'''
enum { ERR_DROP = 1 };
static struct { VkFormat depth_format; VkImage depth_image;
    struct { VkImage image; } liquidDepth; } vk;
static void fail(int level, const char *message, ...) {
    (void)level; (void)message; assert(!"unexpected layout rejection");
}
static struct { void (*Error)(int, const char *, ...); } ri = {fail};
static VkImageMemoryBarrier observed;
static uint32_t observedSourceStage, observedDestinationStage, barriers;
static void qvkCmdPipelineBarrier(VkCommandBuffer command, VkPipelineStageFlags source,
    VkPipelineStageFlags destination, VkDependencyFlags dependencies,
    uint32_t memoryCount, const VkMemoryBarrier *memory,
    uint32_t bufferCount, const VkBufferMemoryBarrier *buffers,
    uint32_t imageCount, const VkImageMemoryBarrier *images) {
    (void)command; (void)dependencies; (void)memory; (void)buffers;
    assert(memoryCount == 0 && bufferCount == 0 && imageCount == 1);
    observed = images[0]; observedSourceStage = source;
    observedDestinationStage = destination; ++barriers;
}
''' + transition + r'''
static void check(VkImage image, VkImageAspectFlags requested, VkImageAspectFlags expected,
    VkImageLayout oldLayout, VkImageLayout newLayout) {
    uint32_t previous = barriers;
    record_image_layout_transition((VkCommandBuffer)(uintptr_t)1, image,
        requested, oldLayout, newLayout, 0, 0);
    assert(barriers == previous + 1);
    assert(observed.image == image && observed.subresourceRange.aspectMask == expected);
    assert(observed.oldLayout == oldLayout && observed.newLayout == newLayout);
    assert(observed.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
    assert(observed.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
    assert(observed.subresourceRange.levelCount == VK_REMAINING_MIP_LEVELS);
    assert(observed.subresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS);
}
int main(void) {
    const VkFormat formats[] = {VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT};
    const VkImage images[] = {11, 12, 101, 102, 103};
    const VkImageAspectFlags depth = VK_IMAGE_ASPECT_DEPTH_BIT;
    const VkImageAspectFlags both = depth | VK_IMAGE_ASPECT_STENCIL_BIT;
    vk.depth_image = 11; vk.liquidDepth.image = 12;
    for (unsigned f = 0; f < ARRAY_LEN(formats); ++f) {
        vk.depth_format = formats[f];
        // Arbitrary point, spot and cascade image handles matter: checking
        // only the two global depth handles caused the native validation fault.
        for (unsigned i = 0; i < ARRAY_LEN(images); ++i) {
            check(images[i], depth, both, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            assert(observed.srcAccessMask == VK_ACCESS_NONE);
            assert(observed.dstAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            check(images[i], depth, both, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            assert(observed.srcAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            assert(observed.dstAccessMask & VK_ACCESS_SHADER_READ_BIT);
            assert(observedSourceStage & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
            assert(observedDestinationStage == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            check(images[i], both, both, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
        check(201, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }
    const VkFormat depthOnly[] = {VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT};
    for (unsigned f = 0; f < ARRAY_LEN(depthOnly); ++f) {
        vk.depth_format = depthOnly[f];
        check(101, depth, depth, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }
    return 0;
}
''')

    def test_descriptor_pool_recreation_never_writes_retained_freed_handles(self):
        source = VK_SOURCE.read_text(encoding="utf-8")
        production = "\n".join(function(source, signature) for signature in (
            "static void vk_update_shadow_attachment_descriptor(",
            "static void vk_update_attachment_descriptors( void )",
            "void vk_init_descriptors( void )"))
        self.run_c(r'''
#define NUM_COMMAND_BUFFERS 3
#define VK_NUM_MENU_BLUR_IMAGES 4
#define VK_CHECK(expression) assert((expression) == VK_SUCCESS)
#define SET_OBJECT_NAME(object, name, type) ((void)0)
enum { RTX_VK_DESC_LAYOUT_STORAGE, RTX_VK_DESC_LAYOUT_UNIFORM,
    RTX_VK_DESC_LAYOUT_SAMPLER, GL_LINEAR, GL_NEAREST };
typedef struct { int gl_mag_filter, gl_min_filter, address_mode,
    max_lod_1_0, noAnisotropy; } Vk_Sampler_Def;
static struct { int integer; } bloom;
#define r_bloom (&bloom)
static struct {
    VkDevice device; VkDescriptorPool descriptor_pool; int blitFilter;
    struct { VkDescriptorSet descriptor; VkBuffer buffer; } storage;
    struct { VkDescriptorSet uniform_descriptor; VkBuffer vertex_buffer; } tess[NUM_COMMAND_BUFFERS];
    VkImageView color_image_view, motion_blur_image_view, underwater_image_view,
        depth_sample_image_view, dlight_shadow_image_view, spot_shadow_image_view, csm_shadow_image_view;
    VkDescriptorSet color_descriptor, motion_blur_descriptor, underwater_descriptor,
        depth_sample_descriptor, dlight_shadow_descriptor, spot_shadow_descriptor, csm_shadow_descriptor;
    VkImageView bloom_image_view[3], menu_blur_image_view[VK_NUM_MENU_BLUR_IMAGES];
    VkDescriptorSet bloom_image_descriptor[3], menu_blur_descriptor[VK_NUM_MENU_BLUR_IMAGES];
    struct { VkImageView color_image_view; VkDescriptorSet color_descriptor; } screenMap;
    struct { VkImageView color_image_view; VkDescriptorSet source_descriptor, color_descriptor; } liquidSnapshot;
    struct { VkImageView image_view; VkDescriptorSet descriptor; } liquidDepth;
} vk;
static unsigned char alive[4096], updated[4096];
static uint32_t nextHandle, allocationCount, updateCount;
static const VkDescriptorSetLayout *vk_descriptor_layout_ref(int id) {
    static const VkDescriptorSetLayout layouts[] = {1, 2, 3}; return &layouts[id];
}
static VkResult qvkAllocateDescriptorSets(VkDevice device,
    const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *result) {
    (void)device;
    assert(info->descriptorPool == vk.descriptor_pool);
    for (uint32_t i = 0; i < info->descriptorSetCount; ++i) {
        assert(++nextHandle < ARRAY_LEN(alive));
        result[i] = nextHandle; alive[nextHandle] = 1; ++allocationCount;
    }
    return VK_SUCCESS;
}
static void validate_write(VkDescriptorSet set) {
    assert(set > 0 && set < ARRAY_LEN(alive));
    // Resetting a pool invalidates old handles without clearing the engine's
    // fields. A write before their reallocation must fail this assertion.
    assert(alive[set]); assert(updated[set] == 0);
    updated[set] = 1; ++updateCount;
}
static void qvkUpdateDescriptorSets(VkDevice device, uint32_t writeCount,
    const VkWriteDescriptorSet *writes, uint32_t copyCount, const VkCopyDescriptorSet *copies) {
    (void)device; (void)copies; assert(copyCount == 0);
    for (uint32_t i = 0; i < writeCount; ++i) {
        validate_write(writes[i].dstSet);
        if (writes[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            assert(writes[i].pImageInfo && writes[i].pImageInfo->imageView != VK_NULL_HANDLE);
            assert(writes[i].pImageInfo->sampler != VK_NULL_HANDLE);
        }
    }
}
static void vk_update_uniform_descriptor(VkDescriptorSet set, VkBuffer buffer) {
    (void)buffer; validate_write(set);
}
static VkSampler vk_find_sampler(const Vk_Sampler_Def *definition) {
    (void)definition; return 99;
}
''' + production + r'''
static void reset_pool(void) {
    memset(alive, 0, sizeof(alive)); memset(updated, 0, sizeof(updated));
    allocationCount = updateCount = 0;
}
static void configure(int withColor, int withBloom, int shadowMask) {
    memset(&vk, 0, sizeof(vk));
    vk.device = (VkDevice)(uintptr_t)1; vk.descriptor_pool = 1;
    bloom.integer = withBloom;
    if (withColor) {
        vk.color_image_view = 10; vk.screenMap.color_image_view = 11;
        vk.motion_blur_image_view = 12; vk.underwater_image_view = 13;
        vk.depth_sample_image_view = 14; vk.liquidSnapshot.color_image_view = 15;
        vk.liquidDepth.image_view = 16;
        for (unsigned i = 0; i < ARRAY_LEN(vk.bloom_image_view); ++i) vk.bloom_image_view[i] = 20 + i;
        // Leave alternating optional pyramid images absent to exercise guards.
        for (unsigned i = 0; i < VK_NUM_MENU_BLUR_IMAGES; i += 2) vk.menu_blur_image_view[i] = 30 + i;
    }
    if (shadowMask & 1) vk.dlight_shadow_image_view = 40;
    if (shadowMask & 2) vk.spot_shadow_image_view = 41;
    if (shadowMask & 4) vk.csm_shadow_image_view = 42;
}
int main(void) {
    for (int withColor = 0; withColor <= 1; ++withColor) {
        for (int withBloom = 0; withBloom <= withColor; ++withBloom) {
            for (int shadows = 0; shadows < 8; ++shadows) {
                configure(withColor, withBloom, shadows);
                reset_pool(); vk_init_descriptors();
                assert(allocationCount == updateCount);
                VkDescriptorSet previousShadow = vk.dlight_shadow_descriptor;
                VkDescriptorSet previousStorage = vk.storage.descriptor;
                reset_pool(); // deliberately retain every engine handle
                vk_init_descriptors();
                assert(allocationCount == updateCount);
                assert(vk.storage.descriptor != previousStorage && !alive[previousStorage]);
                if (shadows & 1) {
                    assert(vk.dlight_shadow_descriptor != previousShadow && !alive[previousShadow]);
                    assert(updated[vk.dlight_shadow_descriptor]);
                }
                if (shadows & 2) assert(updated[vk.spot_shadow_descriptor]);
                if (shadows & 4) assert(updated[vk.csm_shadow_descriptor]);
            }
        }
    }
    return 0;
}
''')


if __name__ == "__main__":
    unittest.main()
