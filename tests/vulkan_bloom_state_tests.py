"""Execute bloom and descriptor binding against a driver-state recorder.

The RTCW reproducer draws the same head material on both sides of bloom, then
changes only the eyelid texture. No game assets or Vulkan device are required.
"""
from pathlib import Path
import unittest

from mesh_tag_tests import function
import rtcw_player_interaction_tests as interaction

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
using qboolean = bool;
constexpr bool qtrue=true, qfalse=false;
using VkDescriptorSet = uint64_t;
constexpr uint64_t VK_NULL_HANDLE=0;
constexpr int VK_PIPELINE_BIND_POINT_GRAPHICS=0, ERR_FATAL=1;
constexpr int RENDER_PASS_SCREENMAP=1, RENDER_PASS_MAIN=2;
constexpr int DEPTH_RANGE_COUNT=4, VK_NUM_BLOOM_PASSES=4;
constexpr int VK_DESC_UNIFORM=0, VK_DESC_TEXTURE_BASE=1, VK_DESC_DEPTH_FADE=2, VK_DESC_COUNT=5;
constexpr int RTX_VK_PIPELINE_LAYOUT_MAIN=10, RTX_VK_PIPELINE_LAYOUT_POST_PROCESS=20, RTX_VK_PIPELINE_LAYOUT_BLEND=30;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))
#define Com_Memset std::memset
struct Frame {
 int command_buffer, depth_range, last_pipeline;
 uint32_t scissor_rect[4];
 struct { uint32_t start,end,offset[VK_DESC_COUNT]; VkDescriptorSet current[VK_DESC_COUNT]; } descriptor_set;
 VkDescriptorSet uniform_descriptor;
 uint32_t uniform_read_offset;
} frame;
struct {
 Frame *cmd=&frame;
 int renderPassIndex, maxBoundDescriptorSets;
 bool fboActive;
 int pipeline_layout=10, pipeline_layout_post_process=20, pipeline_layout_blend=30;
 int bloom_extract_pipeline=1, blur_pipeline[8]={2,3,4,5,6,7,8,9};
 int bloom_blend_pipeline=11, bloom_blend_cel_pipeline=12;
 VkDescriptorSet color_descriptor=1001, bloom_image_descriptor[9];
 struct { int descriptor_bind_calls,descriptor_bind_sets; } stats;
} vk;
struct { bool doneBloom,doneSurfaces; } backEnd;
struct Image { VkDescriptorSet descriptor; } white{1000};
struct { Image *whiteImage=&white; } tr;
static void Error(int,const char *,...) { assert(false); }
struct { decltype(&::Error) Error; } ri{Error};
static int vk_pipeline_layout_handle(int layout) { return layout; }
static bool R_BloomProtectHighlightsActive() { return false; }
static void vk_update_mvp(const void *) {}
struct Binding { VkDescriptorSet descriptor; uint32_t offset; bool valid; } gpu[VK_DESC_COUNT];
static int calls, draws;
static void qvkCmdBindDescriptorSets(int,int,int layout,uint32_t first,uint32_t count,
                                    const VkDescriptorSet *sets,uint32_t offsetCount,const uint32_t *offsets) {
 ++calls;
 assert(first+count<=static_cast<unsigned>(vk.maxBoundDescriptorSets));
 // Bloom's sampler layout is incompatible with the main layout at set zero.
 if(layout!=vk.pipeline_layout) for(auto &binding:gpu) binding.valid=false;
 assert(offsetCount==static_cast<unsigned>(layout==vk.pipeline_layout && first==0));
 for(unsigned i=0;i<count;++i) {
  assert(sets[i]!=VK_NULL_HANDLE);
  auto &binding=gpu[first+i]; binding={sets[i],0,layout==vk.pipeline_layout};
  if(layout==vk.pipeline_layout && first+i==0) {
   assert(sets[i]==frame.uniform_descriptor);
   binding.offset=offsets[0];
  } else assert(sets[i]>=1000); // samplers must never receive the uniform descriptor
 }
}
static void qvkCmdBindPipeline(int,int,int) {}
static void qvkCmdDraw(int,int,int,int,int) { ++draws; }
static void vk_end_render_pass() {}
static void BeginPass() {
 // Actual vk_begin_render_pass clears this cache before bloom's composite.
 frame.last_pipeline=0; frame.depth_range=DEPTH_RANGE_COUNT;
}
static void vk_begin_bloom_extract_render_pass() { BeginPass(); }
static void vk_begin_blur_render_pass(unsigned) { BeginPass(); }
static void vk_begin_post_bloom_render_pass() { BeginPass(); }
'''

CHECKS = r'''
static void Reset(int capacity) {
 frame={}; frame.descriptor_set.start=~0U;
 frame.uniform_descriptor=100; frame.uniform_read_offset=768;
 vk.maxBoundDescriptorSets=capacity; vk.fboActive=true; vk.renderPassIndex=RENDER_PASS_MAIN;
 for(unsigned i=0;i<ARRAY_LEN(vk.bloom_image_descriptor);++i) vk.bloom_image_descriptor[i]=1100+i;
 backEnd={false,true}; calls=draws=0;
 for(auto &binding:gpu) binding={};
}
static void Head() {
 vk_update_descriptor(0,frame.uniform_descriptor);
 vk_update_descriptor_offset(0,512);
 vk_update_descriptor(1,2000);
 vk_bind_descriptor_sets();
}
static void CheckHead() {
 assert(gpu[0].valid && gpu[0].descriptor==100 && gpu[0].offset==512);
 assert(gpu[1].valid && gpu[1].descriptor==2000);
}
int main() {
 for(int capacity:{4,5}) {
  Reset(capacity); Head(); CheckHead();
  frame.last_pipeline=77;
  assert(vk_bloom()); assert(backEnd.doneBloom && draws==10);
  Head(); // same descriptors and offset: no material change can mask a stale cache
  CheckHead();
  for(int i=0;i<capacity;++i) assert(gpu[i].valid);
  assert(gpu[2].descriptor==white.descriptor);
  assert(frame.last_pipeline==0 && frame.depth_range==DEPTH_RANGE_COUNT);
  for(auto value:frame.scissor_rect) assert(value==~0U);
  const int restored=calls; Head(); assert(calls==restored);
  // Eyelids switch the diffuse map; the following open-eye draw restores it.
  vk_update_descriptor(1,2001); vk_bind_descriptor_sets();
  assert(gpu[1].valid && gpu[1].descriptor==2001);
  Head(); CheckHead();
  // A color-only draw after post-processing has not supplied any descriptors.
  Reset(capacity); assert(vk_bloom()); vk_bind_descriptor_sets();
  assert(gpu[0].valid && gpu[0].descriptor==100 && gpu[0].offset==768);
  for(int i=1;i<capacity;++i) assert(gpu[i].valid && gpu[i].descriptor==white.descriptor);
  // Guarded bloom calls must leave both the cache and driver state untouched.
  for(int guard=0;guard<4;++guard) {
   Reset(capacity); Head(); const auto saved=frame; const int before=calls;
   if(guard==0) vk.renderPassIndex=RENDER_PASS_SCREENMAP;
   if(guard==1) backEnd.doneBloom=true;
   if(guard==2) backEnd.doneSurfaces=false;
   if(guard==3) vk.fboActive=false;
   assert(!vk_bloom()); assert(calls==before && draws==0);
   assert(!std::memcmp(&saved,&frame,sizeof(frame))); CheckHead();
  }
 }
}
'''


def fixture(renderer, source=None):
    source = source or (ROOT / 'code' / renderer / 'vk.c').read_text()
    result = PRELUDE
    if renderer == 'renderervk':
        result += function(source, 'static void vk_dirty_descriptor_range(')
    for signature in ('void vk_update_descriptor(', 'void vk_update_descriptor_offset(',
                      'void vk_bind_descriptor_sets(', 'qboolean vk_bloom('):
        result += function(source, signature)
    return result + CHECKS


class VulkanBloomStateTests(unittest.TestCase):
    def test_head_eyelids_and_fallbacks_survive_bloom(self):
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                interaction.RTCWPlayerInteractionTests.compile_and_run(self, fixture(renderer))


if __name__ == '__main__':
    unittest.main()
