"""Run RTX's actual fog compositor with a recording Vulkan host.

A full-screen sky portal must leave the frame's fog pass available to the
main view. Rejected/disabled views must not consume it either, and a successful
draw must restore depth layout and material state before subsequent draws.
"""
import unittest

from entity_sun_shadow_tests import compile_and_run, function, read


HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_global_fog.h"
#include <cassert>
#include <cstring>

enum { VK_NULL_HANDLE = 0, VK_IMAGE_ASPECT_DEPTH_BIT = 1, VK_IMAGE_ASPECT_STENCIL_BIT = 2,
       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
       VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
       VK_PIPELINE_BIND_POINT_GRAPHICS = 5, RTX_VK_PIPELINE_LAYOUT_POST_PROCESS = 6,
       VK_SHADER_STAGE_FRAGMENT_BIT = 7, DEPTH_RANGE_COUNT = 8, VK_DESC_COUNT = 9,
       RDF_NOWORLDMODEL = 1, PV_NONE = 0, STEREO_CENTER = 0,
       RENDER_PASS_MAIN = 0, RENDER_PASS_POST_BLOOM = 1 };
using VkImageAspectFlags = int;
static struct { globalFog_t globalFog; } world;
static struct { decltype(world) *world; int frameCount, overbrightBits; } tr;
static struct {
    struct { int rdflags; qboolean skyboxPortalScene; } refdef;
    struct { int portalView, stereoFrame, viewportX, viewportY, viewportWidth, viewportHeight;
             float zFar; } viewParms;
} backEnd;
static struct { int vidWidth, vidHeight, stencilBits; } glConfig = { 800, 600, 8 };
static struct {
    int command_buffer, last_pipeline, depth_range;
    struct { int start, end; } descriptor_set;
    int scissor_rect[4];
} cmd;
static struct {
    int global_fog_pipeline, depth_sample_descriptor, depth_image, renderPassIndex, maxBoundDescriptorSets;
    struct { int global_fog; } render_pass, framebuffers;
    decltype(cmd) *cmd;
} vk;
static cvar_t strength, nearPlane, exposure;
static cvar_t *r_globalFogStrength = &strength, *r_znear = &nearPlane, *r_tonemapExposure = &exposure;
static bool enabled = true;
static int draws, ends, begins, resumes, transitions;
static float pushed[12];
float Com_Clamp( float min, float max, float value ) { return value < min ? min : value > max ? max : value; }
static bool vk_global_fog_enabled() { return enabled; }
static qboolean vk_scene_linear_enabled() { return qfalse; }
static int vk_pipeline_layout_handle( int value ) { return value; }
static void vk_end_render_pass() { ends++; }
static void vk_begin_global_fog_render_pass() { begins++; }
static void vk_begin_post_bloom_render_pass() { resumes++; vk.renderPassIndex = RENDER_PASS_POST_BLOOM; }
template<typename... T> void qvkCmdBindPipeline( T... ) {}
template<typename... T> void qvkCmdBindDescriptorSets( T... ) {}
static void qvkCmdPushConstants( int, int, int, int, size_t size, const float *data ) {
    assert( size == sizeof( pushed ) ); memcpy( pushed, data, size );
}
static void qvkCmdDraw( int, int vertices, int instances, int, int ) {
    assert( vertices == 4 && instances == 1 ); draws++;
}
static void record_image_layout_transition( int, int image, int aspects, int from, int to, int, int ) {
    assert( image == vk.depth_image );
    assert( aspects == ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT ) );
    assert( from == ( transitions % 2 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
                                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) );
    assert( to == ( transitions % 2 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) );
    transitions++;
}
'''


CHECKS = r'''
static void Frame() {
    tr.world = &world; tr.frameCount++; tr.overbrightBits = 1;
    R_GlobalFogClear( &world.globalFog );
    world.globalFog.loaded = qtrue; world.globalFog.density = 0.0015f;
    world.globalFog.opacity = 0.8f; world.globalFog.start = 64;
    VectorSet( world.globalFog.color, 0.25f, 0.4f, 0.55f );
    backEnd = {}; backEnd.viewParms.viewportWidth = 800;
    backEnd.viewParms.viewportHeight = 600; backEnd.viewParms.zFar = 4096;
    cmd = {}; cmd.last_pipeline = 99;
    vk = {}; vk.cmd = &cmd; vk.global_fog_pipeline = 11;
    vk.depth_sample_descriptor = 12; vk.depth_image = 13;
    vk.render_pass.global_fog = 14; vk.framebuffers.global_fog = 15;
    vk.maxBoundDescriptorSets = 8;
    strength.value = 1; nearPlane.value = 4; exposure.value = 1;
    enabled = true;
}

template<typename Change> static void SkippedThenMain( Change change ) {
    Frame();
    auto savedBackEnd = backEnd; auto savedVK = vk; auto savedWorld = world;
    int priorDraws = draws, priorEnds = ends, priorTransitions = transitions;
    change();
    vk_draw_global_fog();
    assert( draws == priorDraws && ends == priorEnds && transitions == priorTransitions );
    backEnd = savedBackEnd; vk = savedVK; world = savedWorld; tr.world = &world;
    enabled = true; strength.value = 1; nearPlane.value = 4;
    vk_draw_global_fog();
    assert( draws == priorDraws + 1 && ends == priorEnds + 2 && transitions == priorTransitions + 2 );
    assert( begins == draws && resumes == draws && vk.renderPassIndex == RENDER_PASS_POST_BLOOM );
    assert( cmd.last_pipeline == VK_NULL_HANDLE && cmd.depth_range == DEPTH_RANGE_COUNT );
    assert( cmd.descriptor_set.start == 0 && cmd.descriptor_set.end == 7 );
    for ( int value : cmd.scissor_rect ) assert( value == -1 );
    assert( pushed[3] == 0.8f && pushed[4] == 64 && pushed[8] == 4 && pushed[9] == 4096 );
#ifdef USE_REVERSED_DEPTH
    assert( pushed[11] == 1 );
#else
    assert( pushed[11] == 0 );
#endif
    vk_draw_global_fog();
    assert( draws == priorDraws + 1 );
}

int main() {
    SkippedThenMain( [] { backEnd.refdef.skyboxPortalScene = qtrue; } );
    SkippedThenMain( [] {
        backEnd.refdef.skyboxPortalScene = qtrue; vk.renderPassIndex = RENDER_PASS_POST_BLOOM;
    } );
    SkippedThenMain( [] { backEnd.refdef.rdflags = RDF_NOWORLDMODEL; } );
    SkippedThenMain( [] { backEnd.viewParms.portalView = 1; } );
    SkippedThenMain( [] { backEnd.viewParms.stereoFrame = 1; } );
    SkippedThenMain( [] { backEnd.viewParms.viewportX = 10; } );
    SkippedThenMain( [] { backEnd.viewParms.viewportWidth = 400; } );
    SkippedThenMain( [] { vk.renderPassIndex = 99; } );
    SkippedThenMain( [] { vk.global_fog_pipeline = VK_NULL_HANDLE; } );
    SkippedThenMain( [] { vk.depth_sample_descriptor = VK_NULL_HANDLE; } );
    SkippedThenMain( [] { vk.render_pass.global_fog = VK_NULL_HANDLE; } );
    SkippedThenMain( [] { vk.framebuffers.global_fog = VK_NULL_HANDLE; } );
    SkippedThenMain( [] { tr.world = nullptr; } );
    SkippedThenMain( [] { world.globalFog.loaded = qfalse; } );
    SkippedThenMain( [] { enabled = false; } );
    SkippedThenMain( [] { strength.value = 0; } );
    SkippedThenMain( [] { world.globalFog.density = 0; } );
    SkippedThenMain( [] { nearPlane.value = 0; } );
    SkippedThenMain( [] { backEnd.viewParms.zFar = 4; } );
    Frame(); vk.renderPassIndex = RENDER_PASS_POST_BLOOM;
    int prior = draws;
    vk_draw_global_fog();
    assert( draws == prior + 1 );
    return 0;
}
'''


class RTXGlobalFogViewTests(unittest.TestCase):
    def test_only_the_eligible_world_view_consumes_the_frame_fog_pass(self):
        draw = function(read('code/rendererrtx/vk.c'), 'void vk_draw_global_fog( void )')
        for reversed_depth in (False, True):
            with self.subTest(reversed_depth=reversed_depth):
                defines = '#define USE_REVERSED_DEPTH 1\n' if reversed_depth else ''
                compile_and_run(self, defines + HOST + draw + CHECKS, prefix='rtx-fog-view-')


if __name__ == '__main__':
    unittest.main()
