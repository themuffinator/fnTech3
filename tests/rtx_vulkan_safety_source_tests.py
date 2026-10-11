from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8", errors="strict")


def section(source: str, start: str, end: str) -> str:
    begin = source.index(start)
    return source[begin : source.index(end, begin + len(start))]


class RtxVulkanSafetySourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.vk = read("code/rendererrtx/vk.c")
        cls.header = read("code/rendererrtx/vk.h")
        cls.backend = read("code/rendererrtx/tr_backend.c")
        cls.rt = read("code/rendererrtx/rt/rt_backend.c")

    def test_vulkan_12_core_promotion_and_entrypoint_aliases_are_supported(self) -> None:
        instance = section(
            self.vk,
            "static void create_instance( void )\n{",
            "static void vk_destroy_instance",
        )
        device = section(
            self.vk,
            "static qboolean vk_create_device( VkPhysicalDevice physical_device",
            "static void vk_destroy_instance",
        )
        loader = section(
            self.vk,
            "static void init_vulkan_library( void )\n{",
            "static void deinit_instance_functions",
        )

        self.assertIn("PFN_vkEnumerateInstanceVersion", self.vk)
        self.assertIn("qvkEnumerateInstanceVersion", instance)
        self.assertIn("appInfo.apiVersion = s_vkInstanceApiVersion", instance)
        self.assertIn("VK_API_VERSION_1_2", instance)
        self.assertIn("INIT_INSTANCE_FUNCTION_EXT( vkEnumerateInstanceVersion )", loader)

        self.assertIn("core12 =", device)
        self.assertIn("s_vkInstanceApiVersion", device)
        self.assertIn("physicalDeviceProperties.apiVersion", device)
        self.assertIn("bufferDeviceAddressExtension || core12", device)
        self.assertIn("descriptorIndexingExtension || core12", device)
        self.assertGreaterEqual(device.count("if ( !core12 )"), 2)
        self.assertIn("VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME", device)
        self.assertIn("VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME", device)
        self.assertIn("VK_KHR_SPIRV_1_4_EXTENSION_NAME", device)
        self.assertIn("VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME", device)

        self.assertIn("INIT_DEVICE_FUNCTION_EXT(vkGetBufferDeviceAddress)", loader)
        self.assertIn("INIT_DEVICE_FUNCTION_EXT(vkGetBufferDeviceAddressKHR)", loader)
        address = section(
            self.vk,
            "static VkDeviceAddress vk_get_buffer_device_address",
            "static qboolean vk_rt_create_buffer",
        )
        self.assertIn("qvkGetBufferDeviceAddress( vk.device", address)
        self.assertIn("qvkGetBufferDeviceAddressKHR( vk.device", address)
        self.assertIn("return 0;", address)

    def test_shader_binding_table_layout_is_checked_before_allocation(self) -> None:
        sbt = section(self.rt, "static qboolean RT_CreateSbt( void )\n{", "\n}\n")

        # every mask below needs a power-of-two alignment, and the stride and
        # base alignment must fit the device's limits and the handle scratch
        checks = sbt.index("vk_rt_is_power_of_two( handleAlignment )")
        self.assertIn("vk_rt_is_power_of_two( baseAlignment )", sbt)
        self.assertIn("handleSize == 0 || handleSize > 64", sbt)
        self.assertIn("baseAlignment > 4096", sbt)
        self.assertIn("stride > vk.rt.rt_pipeline_props.maxShaderGroupStride", sbt)
        self.assertIn("byte handles[RT_SBT_GROUPS * 64];", sbt)
        self.assertIn("hitOffset = ( missOffset + RT_SBT_MISSES * stride + baseAlignment - 1 )", sbt)
        self.assertIn("s_rt.sbtHit.size = stride * RT_SBT_HITS;", sbt)
        allocate = sbt.index("vk_rt_create_buffer( total + baseAlignment")
        self.assertLess(checks, allocate)
        self.assertLess(sbt.index("maxShaderGroupStride"), allocate)
        # the table starts on a base-aligned device address inside the slack
        self.assertIn(
            "( s_rt.sbt.address + baseAlignment - 1 ) & ~( (VkDeviceAddress)baseAlignment - 1 )", sbt
        )
        self.assertIn("vk_rt_destroy_buffer( &s_rt.sbt );", sbt)

        create_buffer = section(
            self.vk,
            "static qboolean vk_rt_create_buffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProps, qboolean needDeviceAddress, const char *debugName, rtxVkRtBuffer_t *outBuffer )\n{",
            "static void vk_rt_destroy_buffer",
        )
        self.assertIn("needDeviceAddress && outBuffer->address == 0", create_buffer)
        self.assertIn("Com_Memset( outBuffer, 0, sizeof( *outBuffer ) )", create_buffer)

    def test_descriptor_capacity_is_gated_and_large_update_scratch_is_not_on_stack(self) -> None:
        device = section(
            self.vk,
            "static qboolean vk_create_device( VkPhysicalDevice physical_device",
            "static void vk_destroy_instance",
        )
        update = section(self.rt, "static void RT_UpdateDescriptors( rtFrame_t *frame )\n{", "\n}\n")

        for limit in (
            "maxPerStageDescriptorSampledImages",
            "maxDescriptorSetSampledImages",
            "maxPerStageDescriptorSamplers",
            "maxDescriptorSetSamplers",
            "maxPerStageDescriptorStorageBuffers",
            "maxPerStageDescriptorStorageImages",
            "maxPerStageResources",
            "maxPerStageDescriptorAccelerationStructures",
        ):
            with self.subTest(limit=limit):
                self.assertIn(limit, device)

        # the gate counts the layouts the path tracer creates
        self.assertIn(
            "#define RTX_RT_STORAGE_BUFFER_COUNT ( RT_BINDING_LIGHT_INDICES - RT_BINDING_WORLD_VERTICES + 3u )",
            self.vk,
        )
        self.assertIn(
            "#define RTX_RT_TRACE_STORAGE_IMAGE_COUNT ( RT_BINDING_OUT_INDIRECT - RT_BINDING_OUT_COLOR + 1u )",
            self.vk,
        )
        self.assertIn("#define RTX_RT_SAMPLED_IMAGE_COUNT RT_MAX_TEXTURES", self.vk)
        self.assertIn("static VkDescriptorImageInfo s_vkRtDescriptorImageInfos[RT_MAX_TEXTURES];", self.vk)
        self.assertIn("VkDescriptorImageInfo *textures = s_vkRtDescriptorImageInfos;", update)
        self.assertNotRegex(update, r"VkDescriptorImageInfo\s+\w+\s*\[\s*RT_MAX_TEXTURES")
        self.assertRegex(
            self.rt,
            r"typedef char rtTextureTableCoversImages\[\s*\(\s*RT_MAX_TEXTURES >= MAX_DRAWIMAGES\s*\)\s*\?\s*1\s*:\s*-1\s*\];",
        )

    def test_masked_hits_use_any_hit_with_bounded_materials_and_explicit_lod(self) -> None:
        any_hit = read("code/rendererrtx/shaders/rt_main.rahit")
        geometry = read("code/rendererrtx/shaders/rt_geometry.glsl")
        material = read("code/rendererrtx/shaders/rt_material.glsl")
        raygen = read("code/rendererrtx/shaders/rt_main.rgen")
        hardware_trace = read("code/rendererrtx/shaders/rt_trace_hw.glsl")
        accepted = section(geometry, "bool rt_hit_accepted(", "\n}\n")
        closest_hit = read("code/rendererrtx/shaders/rt_main.rchit")
        reflection = json.loads(
            read("code/rendererrtx/shaders/spirv/shader_reflection.json")
        )
        pipeline = section(self.rt, "static qboolean RT_EnsurePipelines( void )\n{", "\n}\n")

        # face culling and the alpha test reject a hit; the material index and
        # the texture index are clamped; an any-hit shader has no derivatives
        self.assertIn("ignoreIntersectionEXT;", any_hit)
        self.assertIn("if (!rt_hit_accepted(tri, bary, front, gl_WorldRayDirectionEXT)) {", any_hit)
        self.assertIn("rt_material_covered(m, h)", accepted)
        self.assertIn("RT_MF_CULL_FRONT", accepted)
        self.assertIn("min(tri.v0.materialIndex, count - 1u)", geometry)
        self.assertIn("image != RT_NO_IMAGE && image < RT_MAX_TEXTURES", material)
        self.assertIn("textureLod(u_textures[nonuniformEXT(image)]", material)
        self.assertNotRegex(material, r"\btexture\(u_textures")
        self.assertNotIn("gl_RayFlagsOpaqueEXT", closest_hit)
        self.assertNotIn("gl_RayFlagsOpaqueEXT", any_hit)
        self.assertIn("VkPipelineShaderStageCreateInfo stages[8];", pipeline)
        self.assertIn("vk.modules.rt_rahit", pipeline)
        self.assertIn("VK_SHADER_STAGE_ANY_HIT_BIT_KHR", pipeline)
        self.assertIn("groups[4].anyHitShader = 5;", pipeline)
        self.assertIn("groups[4].closestHitShader = 4;", pipeline)
        # filter rays: their own miss and an any-hit-only hit group
        self.assertIn("vk.modules.rt_rmiss_filter", pipeline)
        self.assertIn("groups[5].anyHitShader = 6;", pipeline)
        self.assertIn("pipelineInfo.groupCount = RT_SBT_GROUPS;", pipeline)
        self.assertIn("pipelineInfo.maxPipelineRayRecursionDepth = 1;", pipeline)
        self.assertIn(
            "VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR", pipeline
        )
        # the raygen shades the traced hits itself; recursion stays at one
        self.assertIn('#include "rt_trace_hw.glsl"', raygen)
        self.assertIn("traceRayEXT", hardware_trace)
        self.assertTrue(
            any(
                shader["source"] == "rt_main.rahit" and shader["stage"] == "rahit"
                for shader in reflection["shaders"]
            )
        )

    def test_a_screenshot_the_device_has_no_memory_for_is_skipped(self) -> None:
        # With another program holding the GPU's memory, the readback's
        # allocation failed and ended the game (a sweep's escape1); now the
        # screenshot or video frame is skipped with a warning
        vk = read("code/rendererrtx/vk.c")
        readback = section(vk, "qboolean vk_read_pixels( byte *buffer, uint32_t width, uint32_t height )", "\n}\n")
        self.assertIn("if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) != VK_SUCCESS ) {", readback)
        self.assertIn("if ( qvkCreateImage( vk.device, &desc, NULL, &dstImage ) != VK_SUCCESS ) {", readback)
        self.assertNotIn("ERR_FATAL", readback)
        self.assertEqual(readback.count("qvkDestroyImage( vk.device, dstImage, NULL );"), 3)
        self.assertTrue(readback.rstrip().endswith("return qtrue;"))
        init = read("code/rendererrtx/tr_init.c")
        pixels = section(init, "static byte *RB_ReadPixels(", "#else")
        self.assertIn("if ( !vk_read_pixels( bufstart, width, height ) ) {\n\t\tri.Hunk_FreeTempMemory( buffer );\n"
                      "\t\treturn NULL;", pixels)
        for name, variable in (("qboolean RB_TakeScreenshotPNG(", "buffer"), ("qboolean RB_TakeScreenshot(", "allbuf"),
                               ("qboolean RB_TakeScreenshotJPEG(", "buffer"),
                               ("qboolean RB_TakeScreenshotBMP(", "allbuf")):
            body = section(init, name, "\n}\n")
            read_at = body.index("RB_ReadPixels(")
            self.assertLess(read_at, body.index(f"if ( !{variable} ) {{"), name)
            # a frame that could not be read back writes nothing, and says so
            # rather than reporting the file as written
            failed = body[body.index(f"if ( !{variable} ) {{"):]
            self.assertIn("\t\treturn qfalse;\n\t}", failed[:failed.index("\n\t}") + 3], name)
            self.assertTrue(body.rstrip().endswith("return qtrue;"), name)
        backend = read("code/rendererrtx/tr_backend.c")
        for writer, silent, target in (("RB_TakeScreenshotPNG(", "screenShotPNGsilent", "screenshotPNG"),
                                       ("RB_TakeScreenshot(", "screenShotTGAsilent", "screenshotTGA"),
                                       ("RB_TakeScreenshotJPEG(", "screenShotJPGsilent", "screenshotJPG"),
                                       ("RB_TakeScreenshotBMP(", "screenShotBMPsilent", "screenshotBMP")):
            call = backend[backend.index(f"if ( {writer}"):]
            call = call[:call.index(f'ri.Printf( PRINT_ALL, "Wrote %s\\n", backEnd.{target} );')]
            self.assertIn(f"&& !backEnd.{silent} ) {{", " ".join(call.split()), writer)
        video = section(init, "const void *RB_TakeVideoFrameCmd( const void *data )", "\n}\n")
        self.assertIn("if ( !vk_read_pixels( cBuf, cmd->width, cmd->height ) ) {", video)

    def test_a_view_after_the_portal_sky_takes_its_own_depth_snapshot(self) -> None:
        # RTCW's and ET's portal sky is a full-screen view whose soft-particle
        # depth copy ends the main pass; the view behind it clears depth in
        # the post-bloom pass, which must drop that copy so its own soft
        # particles fade against its own depth
        vk = read("code/rendererrtx/vk.c")
        clear = section(vk, "static void vk_clear_depth_internal( qboolean clear_stencil, qboolean force )", "\n}\n")
        reset = ("if ( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) {\n"
                 "\t\tvk.liquidDepth.copied = qfalse;")
        self.assertIn(reset, clear)
        self.assertLess(clear.index(reset), clear.index("if ( !force && vk_world.dirty_depth_attachment == 0 )"))
        copy = section(vk, "void vk_copy_depth_fade( void )", "\n}\n")
        self.assertIn("( vk.renderPassIndex != RENDER_PASS_MAIN &&\n\t\t  vk.renderPassIndex != RENDER_PASS_POST_BLOOM )",
                      copy)

    def test_a_portal_sky_leaves_the_trace_to_the_view_behind_it(self) -> None:
        # RTCW's and ET's client games draw a map's portal sky as a scene of
        # its own, before the view it is seen from, flagging only that scene
        # RDF_SKYBOXPORTAL (their cg_view.c CG_DrawSkyBoxPortal). The trace
        # takes one view a frame: the portal sky must not be it.
        scene = read("code/rendererrtx/tr_scene.c")
        self.assertIn("tr.refdef.skyboxPortalScene = ri.gameId == COM_GAME_WOLFET ? fd->wolfet.skyboxPortal :\n"
                      "\t\tR_UsesRTCWLighting() ? fd->rtcw.skyboxPortal : qfalse;", scene)
        adapter = read("code/compat/render_record_adapter.hpp")
        self.assertIn("result.rtcw.skyboxPortal = flags & FNTECH3_RTCW_RDF_SKYBOXPORTAL ? qtrue : qfalse;", adapter)
        self.assertIn("result.wolfet.skyboxPortal = flags & FNTECH3_WOLFET_RDF_SKYBOXPORTAL ? qtrue : qfalse;", adapter)
        primary = section(
            self.vk,
            "qboolean vk_rt_primary_view_eligible( void )\n{",
            "qboolean vk_rt_trace_frame( void )",
        )
        self.assertIn("!backEnd.refdef.skyboxPortalScene &&", primary)
        # RTCW's snooper scope view stays raster, which draws its heat picture
        # (white lightmaps, bright players) as the original renderer did
        self.assertIn("!backEnd.refdef.rtcw.snooperView &&", primary)
        # a raster portal sky may have ended the main pass (a depth-fade
        # snapshot): the view behind it then draws, and is traced, in the
        # post-bloom pass, which keeps its depth for the overlay after the trace
        self.assertIn("( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM )",
                      primary)
        post_bloom = self.vk[self.vk.index("// post-bloom pass"):self.vk.index("&vk.render_pass.post_bloom )")]
        store = post_bloom[post_bloom.index("attachments[1].storeOp ="):post_bloom.index("attachments[1].stencilLoadOp")]
        self.assertIn("vk_rt_trace_available()", store)
        # its refusal is not reported: it would take the report of the view
        # behind it
        report = primary[primary.index("// why a world view is left to raster"):]
        self.assertIn("!backEnd.refdef.skyboxPortalScene && vk_rt_debug_should_log_skip( &lastRefusedFrame )", report)
        self.assertIn("qboolean	skyboxPortalScene;", read("code/rendererrtx/tr_local.h"))

    def test_primary_dispatch_and_output_composition_fail_before_raster_state_is_lost(self) -> None:
        primary = section(
            self.vk,
            "qboolean vk_rt_primary_view_eligible( void )\n{",
            "qboolean vk_rt_trace_frame( void )",
        )
        trace = section(self.rt, "static qboolean RT_TraceFrame( void )\n{", "\n}\n")

        for gate in (
            "vk.renderPassIndex == RENDER_PASS_MAIN",
            "portalView == PV_NONE",
            "stereoFrame == STEREO_CENTER",
            "viewportX >= 0",
            "viewportY >= 0",
            "viewportX + backEnd.viewParms.viewportWidth <= glConfig.vidWidth",
            "viewportY + backEnd.viewParms.viewportHeight <= glConfig.vidHeight",
            "backEnd.viewParms.viewportHeight * 2 >=",
            "vk.rt.lastFrameBuilt == (uint32_t)tr.frameCount",
            "RDF_HYPERSPACE",
        ):
            with self.subTest(gate=gate):
                self.assertIn(gate, primary)

        # everything that can fail on the host runs while the raster pass is
        # still open; once it ends, every return resumes in post-bloom
        end_pass = trace.index("vk_end_render_pass();")
        for preflight in (
            "RT_EnsurePipelines()",
            "RT_EnsureImages( view.extent.width, view.extent.height )",
            "RT_BuildWorld()",
            "RT_ExtractDynamic()",
            "RT_UploadMaterials( frame )",
        ):
            with self.subTest(preflight=preflight):
                self.assertLess(trace.index(preflight), end_pass)
        after = trace[end_pass:]
        returns = [m.start() for m in re.finditer(r"return (?:q(?:true|false)|RT_FrameFailed\()", after)]
        self.assertTrue(returns)
        for position in returns:
            self.assertIn("vk_begin_post_bloom_render_pass();", after[:position])
        # a failure there says why, once per reason
        self.assertIn("vk_begin_post_bloom_render_pass();\n\t\treturn RT_FrameFailed(", after)
        failed = section(self.rt, "static qboolean RT_FrameFailed(", "\n}\n")
        self.assertIn("if ( !( s_rt.loggedFailures & ( 1u << reason ) ) ) {", failed)
        self.assertIn("return qfalse;", failed)
        # the composite samples the raster frame, then is blitted over it
        self.assertIn("VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT", trace)
        self.assertIn("RT_TraceStage() | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT", trace)
        self.assertIn("RT_ComputeBarrier( cmd, RT_TraceStage() );", trace)
        # the trace's stage: compute shaders without the ray-tracing pipeline
        stage = section(self.rt, "static VkPipelineStageFlags RT_TraceStage( void )\n{", "\n}\n")
        self.assertIn(
            "RT_Compute() ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR",
            stage,
        )
        self.assertIn("VK_ACCESS_TRANSFER_WRITE_BIT", trace)
        self.assertIn("VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT", trace)
        self.assertIn("qvkCmdBlitImage( cmd, s_rt.output.image", trace)
        # the trace covers the main view's rectangle, which the composite
        # reads and the blit writes back in place
        self.assertIn("RT_ViewRect( &view );", trace)
        self.assertIn("blit.dstOffsets[0].x = view.offset.x;", trace)
        self.assertIn("blit.dstOffsets[1].y = view.offset.y + (int32_t)s_rt.outputHeight;", trace)
        self.assertIn("push.view[0] = (uint32_t)view.offset.x;", trace)
        rect = section(self.rt, "static void RT_ViewRect( VkRect2D *rect )\n{", "\n}\n")
        self.assertIn("get_viewport_rect( rect );", rect)
        self.assertIn("MIN( (int32_t)rect->extent.width, (int32_t)vk.renderWidth - x )", rect)
        self.assertIn("refusing silent raster fallback", self.backend)

    def test_frame_mutable_resources_and_upload_synchronization_are_explicit(self) -> None:
        # host-written buffers and descriptor sets are per command buffer
        self.assertIn("#define RT_FRAMES NUM_COMMAND_BUFFERS", self.rt)
        trace = section(self.rt, "static qboolean RT_TraceFrame( void )\n{", "\n}\n")
        self.assertIn("rtFrame_t *frame = &s_rt.frames[ vk.cmd_index % RT_FRAMES ];", trace)
        frame = section(self.rt, "typedef struct {\n\trtMappedBuffer_t", "} rtFrame_t;")
        for member in ("vertices", "indices", "materials", "stages", "lights", "frame", "instances"):
            with self.subTest(member=member):
                self.assertRegex(frame, rf"rtMappedBuffer_t\s+{member};")
        self.assertIn("VkDescriptorSet\t\trtSet;", frame)
        self.assertIn("VkDescriptorSet\t\tdenoiseSet;", frame)
        # history ping-pongs by frame parity; the previous frame's image reads
        # and writes complete before this frame's passes
        self.assertIn("s_rt.parity ^= 1u;", trace)
        self.assertIn("push.state[0] = s_rt.parity;", trace)
        self.assertIn(
            "memory.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;",
            trace,
        )

        self.assertGreaterEqual(
            self.vk.count(
                "const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT"
            ),
            2,
        )
        release = section(
            self.vk,
            "void vk_release_resources( refShutdownCode_t code )",
            "#if 0\nstatic void record_buffer_memory_barrier",
        )
        wait = release.index("vk_wait_staging_buffer();")
        reset = release.index("qvkResetCommandBuffer( vk.staging_command_buffer, 0 )", wait)
        destroy = release.index("vk_destroy_sync_primitives();", reset)
        create = release.index("vk_create_sync_primitives();", destroy)
        self.assertLess(wait, reset)
        self.assertLess(reset, destroy)
        self.assertLess(destroy, create)

    def test_attachment_pool_covers_the_maximum_supported_feature_mix(self) -> None:
        bloom_passes = re.search(
            r"#define\s+VK_NUM_BLOOM_PASSES\s+(\d+)", self.header
        )
        menu_blur_images = re.search(
            r"#define\s+VK_NUM_MENU_BLUR_IMAGES\s+(\d+)", self.header
        )
        pool_size = re.search(
            r"#define\s+MAX_ATTACHMENTS_IN_POOL\s+"
            r"\(\s*(\d+)\s*\+\s*VK_NUM_MENU_BLUR_IMAGES\s*"
            r"\+\s*VK_NUM_BLOOM_PASSES\s*\*\s*2\s*\)",
            self.header,
        )

        self.assertIsNotNone(bloom_passes)
        self.assertIsNotNone(menu_blur_images)
        self.assertIsNotNone(pool_size)
        assert (
            bloom_passes is not None
            and menu_blur_images is not None
            and pool_size is not None
        )

        # Nine bloom images and the three-image menu soft-focus pyramid, plus
        # twelve possible non-bloom attachments: scene, motion, liquid,
        # screen-map color/MSAA/depth, main MSAA or depth snapshot, capture,
        # main depth, and three shadow atlases.
        capacity = (
            int(pool_size.group(1))
            + int(menu_blur_images.group(1))
            + int(bloom_passes.group(1)) * 2
        )
        self.assertGreaterEqual(capacity, 24)


if __name__ == "__main__":
    unittest.main()
