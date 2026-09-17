"""Execute backend fog transfers and composite transparent material passes."""
import unittest
import rtcw_fog_gpu_tests as gpu


CHECKS = r'''
static float Composite(float source, float alpha, float destination, unsigned bits) {
 float src=1, dst=0;
 switch(bits & GLS_SRCBLEND_BITS) {
 case GLS_SRCBLEND_ZERO: src=0; break;
 case GLS_SRCBLEND_DST_COLOR: src=destination; break;
 case GLS_SRCBLEND_ONE_MINUS_DST_COLOR: src=1-destination; break;
 case GLS_SRCBLEND_SRC_ALPHA: src=alpha; break;
 }
 switch(bits & GLS_DSTBLEND_BITS) {
 case GLS_DSTBLEND_ONE: dst=1; break;
 case GLS_DSTBLEND_SRC_COLOR: dst=source; break;
 case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR: dst=1-source; break;
 case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA: dst=1-alpha; break;
 }
 return source*src+destination*dst;
}
int main() {
 shader_t shader{}; shaderStage_t stage{}; tess.shader=&shader;
 auto &frame=backEnd.refdef.rtcwFog;
 frame.fogEnabled=frame.skyEnabled=qtrue;
 frame.fog.registered=qtrue; frame.fog.mode=FN_RTCW_FOG_LINEAR;
 frame.fog.start=0; frame.fog.end=100;
 frame.fog.color[0]=0.2f; frame.fog.color[1]=0.3f; frame.fog.color[2]=0.4f;
 frame.skyFog=frame.fog;
 struct Blend { unsigned bits; float neutral; };
 const Blend blends[] = {
  {GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, 0},
  {GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE, 0}, // RTCW torch passes
  {GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE, 0},
  {GLS_SRCBLEND_ONE_MINUS_DST_COLOR | GLS_DSTBLEND_ONE, 0},
  {GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR, 0},
  {GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR, 0},
  {GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR, 0},
  {GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, 1},
  {GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR, 1},
  {GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_SRC_COLOR, 0.5f},
 };
 for(auto profile:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  ri.gameId=profile;
  for(auto sky:{qfalse,qtrue}) for(auto blend:blends) {
   shader.isSky=sky;
   stage.stateBits=blend.bits | 0x100; // Depth-write is independent of blending.
   auto draw=StageFog(&stage);
   for(float visibility:{0.0f,0.25f,1.0f}) for(float alpha:{0.0f,0.4f,1.0f}) {
    for(float destination:{0.0f,0.17f,0.8f,1.0f}) for(float source:{blend.neutral,0.3f}) {
     for(int channel=0;channel<3;++channel) {
      const float fogged=visibility*source+(1-visibility)*draw.color[channel];
      const float actual=Composite(fogged,alpha,destination,blend.bits);
      const float expected=visibility*Composite(source,alpha,destination,blend.bits)+(1-visibility)*destination;
      assert(fabsf(actual-expected)<1e-6f);
     }
    }
   }
   // A nofog stage stays disabled, including when reusing a previous UBO.
   shader.rtcwNoFog=qtrue;
   assert(StageFog(&stage).color[3]==0);
   stage.rtcwFog=qtrue;
   assert(StageFog(&stage).color[3]==1);
   stage.rtcwFog=qfalse; shader.rtcwNoFog=qfalse;
  }
  // Opaque and standard alpha materials retain the selected fog color.
  for(unsigned bits:{0u, unsigned(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO),
      unsigned(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)}) {
   stage.stateBits=bits; auto draw=StageFog(&stage);
   for(int channel=0;channel<3;++channel) assert(draw.color[channel]==frame.fog.color[channel]);
   // Fully transparent alpha fragments must leave the framebuffer unchanged.
   if(bits & GLS_SRCBLEND_SRC_ALPHA)
    assert(Composite(draw.color[0],0,0.7f,bits)==0.7f);
  }
  backEnd.projection2D=qtrue; assert(StageFog(&stage).color[3]==0);
  backEnd.projection2D=qfalse;
 }
}
'''

GL_STAGE = r'''
static fnRTCWFogDraw_t StageFog(const shaderStage_t *stage) {
 RB_RTCWStageFog(stage);
 fnRTCWFogDraw_t draw;
 memcpy(draw.color,glslColor,16); memcpy(draw.params,glslParams,16);
 if(draw.color[3]) for(int i=0;i<3;++i) assert(draw.color[i]==fogColor[i]);
 return draw;
}
'''

VK_STAGE = r'''
static fnRTCWFogDraw_t StageFog(const shaderStage_t *stage) {
 vkUniform_t data; memset(&data,0xa5,sizeof(data));
 VK_SetRTCWFogParams(&data,stage);
 fnRTCWFogDraw_t draw;
 memcpy(draw.color,data.rtcwFogColor,16); memcpy(draw.params,data.rtcwFogParams,16);
 return draw;
}
'''


class RTCWFogBlendTests(unittest.TestCase):
    def test_transparent_pass_compositing(self):
        for backend in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(backend=backend):
                if backend == 'renderer':
                    body = gpu.GL.split('int main()')[0] + GL_STAGE
                else:
                    source = (gpu.ROOT / 'code' / backend / 'vk.h').read_text()
                    begin = source.index('typedef struct vkUniform_s {')
                    end = source.index('} vkUniform_t;', begin) + len('} vkUniform_t;')
                    body = source[begin:end] + gpu.VK.split('int main()')[0] + VK_STAGE
                gpu.RTCWFogGPUTests.compile_run(self, body + CHECKS)
