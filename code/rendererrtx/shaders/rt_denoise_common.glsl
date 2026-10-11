// Shared bindings and helpers of the path tracer's denoiser and composite.
//
// The denoiser is a spatiotemporal variance-guided filter after Schied et
// al. 2017 ("Spatiotemporal Variance-Guided Filtering: Real-Time
// Reconstruction for Path-Traced Global Illumination"), written independently
// for this renderer. It filters diffuse irradiance (the trace's output before
// the reflectance multiply) and specular radiance separately, so texture
// detail never passes through the filter.
//
// Every pass uses descriptor set 0 below; images that alternate between
// frames or iterations are two-element arrays indexed by push constants.
// Binding numbers mirror rtDenoiseBinding_t in code/rendererrtx/rt/rt_gpu.h.

#ifndef RT_DENOISE_COMMON_GLSL
#define RT_DENOISE_COMMON_GLSL

#include "rt_signal_packing.glsl"

layout(set = 0, binding = 0, rgba16f) uniform image2D u_noisyDiffuse;
layout(set = 0, binding = 1, rgba16f) uniform image2D u_noisySpecular;
layout(set = 0, binding = 2, rgba16f) uniform image2D u_reflectance;
layout(set = 0, binding = 3, rg32ui) uniform uimage2D u_normalDepth[2];
layout(set = 0, binding = 4, rg32ui) uniform uimage2D u_motion;
layout(set = 0, binding = 5, r32ui) uniform uimage2D u_info[2];
layout(set = 0, binding = 6, rgba16f) uniform image2D u_histDiffuse[2];
layout(set = 0, binding = 7, rgba16f) uniform image2D u_histSpecular[2];
layout(set = 0, binding = 8, rgba16f) uniform image2D u_histMoments[2];
layout(set = 0, binding = 9, rgba16f) uniform image2D u_filterDiffuse[2];
layout(set = 0, binding = 10, rgba16f) uniform image2D u_filterSpecular[2];
layout(set = 0, binding = 11, rgba16f) uniform image2D u_emission;
layout(set = 0, binding = 12, rgba16f) uniform image2D u_output;
layout(set = 0, binding = 13) uniform sampler2D u_sceneColor;
// the tone mapper's state (rtToneBuffer_t, rt_tonemap.glsl): this frame's
// histogram of log luminance, the fitted curve (log2 display luminance at
// each bin), the adapted log2 luminance, and 1 once a curve is fitted
const uint RT_TONE_BINS = 128u;
layout(set = 0, binding = 14, std430) buffer RtTone {
	uint histogram[RT_TONE_BINS];
	float curve[RT_TONE_BINS];
	float adaptedLog;
	uint frames;
	uint pad0;
	uint pad1;
} u_tone;

// Indirect samples have their own history. Alpha of the noisy image is
// sample validity: a skipped checkerboard tile is not a black observation.
layout(set = 0, binding = 15, rgba16f) uniform image2D u_noisyIndirect;
layout(set = 0, binding = 16, rgba16f) uniform image2D u_histIndirect[2];
layout(set = 0, binding = 17, rgba16f) uniform image2D u_filterIndirect[2];

layout(push_constant) uniform DenoisePush {
	vec4 traceSize;		// width, height, 1 / width, 1 / height
	vec4 outputSize;	// width, height, 1 / width, 1 / height
	vec4 params;		// elapsed seconds (0: frozen average), luminance sigma, normal power, depth sigma
	uvec4 state;		// x: current parity, y: iteration, z: step size, w: flags
	uvec4 extra;		// x: max history, y: debug view, z: iterations, w: RT_COMPOSITE_* flags
	uvec4 view;			// x, y: the traced view's corner in the scene colour; z: the tone mapper's pass
	vec4 fog;			// the view's fog colour, blended in by each pixel's visibility (emission alpha)
	vec4 exposure;		// x: mode (0 none, 1 auto, 2 fixed), y: bias (stops) or multiplier, z: tone curve share
} pc;

const uint RT_DENOISE_FLAG_HISTORY_VALID = 1u << 0;
const uint RT_DENOISE_FLAG_WRITE_HISTORY = 1u << 1;
const uint RT_DENOISE_FLAG_DISABLED = 1u << 2;

float denoise_luminance(vec3 c)
{
	return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec4 read_normal_depth(uint parity, ivec2 p)
{
	return rt_unpack_geometry(imageLoad(u_normalDepth[parity], p).xy);
}

vec4 read_motion(ivec2 p)
{
	vec4 motion = rt_unpack_geometry(imageLoad(u_motion, p).xy);
	// Valid reprojection always has positive previous view depth. Zero
	// replaces the former separate invalid-history lane.
	motion.w = motion.z > 0.0 ? 0.0 : 1.0;
	return motion;
}

bool in_trace(ivec2 p)
{
	return all(greaterThanEqual(p, ivec2(0))) && all(lessThan(p, ivec2(pc.traceSize.xy)));
}

vec3 denoised_irradiance(ivec2 p, uint final)
{
	return imageLoad(u_filterDiffuse[final], p).rgb + imageLoad(u_filterIndirect[final], p).rgb;
}

#endif
