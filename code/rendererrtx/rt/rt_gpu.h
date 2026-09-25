/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

GPU-visible structures of the RTX renderer's path tracer. Each mirrors a
declaration in code/rendererrtx/shaders/rt_common.glsl; the checks below pin
their sizes and the offsets the shaders depend on.
===========================================================================
*/

#ifndef RT_GPU_H
#define RT_GPU_H

#include <stddef.h>
#include <stdint.h>

#define RT_MAX_TEXTURES			2048
#define RT_NO_IMAGE				0xFFFFu
#define RT_GPU_MAX_TEXMODS		5
#define RT_GPU_MAX_STAGES		8

// Instance classes (bits 20..23 of instanceCustomIndex) and TLAS masks
typedef enum {
	RT_CLASS_WORLD,
	RT_CLASS_WORLD_SPECIAL,
	RT_CLASS_DYNAMIC,
	RT_CLASS_WEAPON,
	RT_CLASS_BODY,
	RT_CLASS_WORLD_NOSHADOW,	// world surfaces the map compiler's lights pass through
	RT_CLASS_WORLD_FILTER,		// world surfaces its lights pass filtered (shadow rays only)
	RT_CLASS_COUNT
} rtClass_t;

#define RT_MASK_WORLD			0x01u
#define RT_MASK_DYNAMIC			0x02u
#define RT_MASK_WEAPON			0x04u
#define RT_MASK_BODY			0x08u
#define RT_MASK_SPECIAL			0x10u
#define RT_MASK_NOSHADOW			0x20u
#define RT_MASK_FILTER			0x40u

// Instances the compute backend traverses: four world BLASes and the
// dynamic classes (rt_trace_sw.glsl)
#define RT_SW_MAX_INSTANCES		8

// RtMaterial.flags
#define RT_MF_MASKED			( 1u << 0 )
#define RT_MF_SKY				( 1u << 1 )
#define RT_MF_PORTAL			( 1u << 2 )
#define RT_MF_TWO_SIDED			( 1u << 3 )
#define RT_MF_CULL_FRONT		( 1u << 4 )
#define RT_MF_LIGHTMAPPED		( 1u << 5 )
#define RT_MF_VERTEX_LIT		( 1u << 6 )
#define RT_MF_SURFACE_LIGHT		( 1u << 7 )
#define RT_MF_ENV_MAPPED		( 1u << 8 )
#define RT_MF_NOLIGHT			( 1u << 9 )
#define RT_MF_ENTITY_LIT		( 1u << 10 )
#define RT_MF_EMISSIVE_ONLY		( 1u << 11 )
#define RT_MF_ALPHASHADOW		( 1u << 12 )
#define RT_MF_LIGHTFILTER		( 1u << 13 )
#define RT_MF_MATERIAL_MAPS		( 1u << 14 )
// the raster light pass skips it (no lighting stage: nodlight, sky and the
// like), so no dynamic light reaches it
#define RT_MF_NODLIGHT			( 1u << 15 )
// an RTCW nofog material with no stage that turns fog back on: the view's
// global fog leaves it, as the raster pass does
#define RT_MF_NOFOG				( 1u << 16 )
// a shader drawn fullbright that the scene lights all the same, only its
// bright texels glowing (rtx_rt_lighting 0; rt_material.glsl)
#define RT_MF_FULLBRIGHT_LIT	( 1u << 17 )

// A model vertex's fourth position lane: the top byte of the raster pass's
// light for it, RGB8 below (RT_StampEntityLight), without or with the dynamic
// lights; any other value (a world vertex's 1.0f) carries none
#define RT_ENTITY_LIGHT_GRID	0x40u
#define RT_ENTITY_LIGHT_ALL		0x41u

// rtGpuMaterial_t.lightImage[1]
#define RT_LIGHT_IMAGE_MISSING	( 1u << 0 )	// none found: the compilers' white, opaque default
#define RT_LIGHT_IMAGE_SRGB		( 1u << 1 )	// its view decodes sRGB colour

// Stage roles and flags
#define RT_ROLE_BASE			0u
#define RT_ROLE_LIGHTMAP		1u
#define RT_ROLE_ENVIRONMENT		2u
#define RT_ROLE_SKIP			3u

#define RT_SF_LINEAR_SAMPLE		( 1u << 16 )
#define RT_SF_VERTEX_IS_LIGHT	( 1u << 17 )
#define RT_SF_ALPHA_TEST		( 1u << 18 )
#define RT_SF_DETAIL			( 1u << 19 )

// RtFrameData.flags.x
#define RT_FRAME_BAKED_LIGHTING	( 1u << 0 )
#define RT_FRAME_SUN			( 1u << 1 )
#define RT_FRAME_SCENE_LINEAR	( 1u << 2 )
#define RT_FRAME_WEAPON			( 1u << 3 )
#define RT_FRAME_SPECULAR		( 1u << 4 )
#define RT_FRAME_FILTERS		( 1u << 5 )
#define RT_FRAME_Q3MAP2			( 1u << 6 )
#define RT_FRAME_MIS			( 1u << 7 )
#define RT_FRAME_ACCUMULATE		( 1u << 8 )
// the sky lights the scene with its own colour, one cosine-weighted ray a
// sample (rt_lighting.glsl rt_sample_sky_light)
#define RT_FRAME_SKY_RADIANCE	( 1u << 9 )
// the diffuse bounce is traced in a checkerboard of warp-sized tiles that
// alternates each frame, counted twice (rtx_rt_checkerboard; rt_path.glsl)
#define RT_FRAME_CHECKERBOARD	( 1u << 10 )
// light is traced in linear units, as Q2RTX traces it: the colours the
// shaders and the sky show are decoded from the display's gamma, and the
// composite encodes the tone mapped result (the default lighting on a frame
// whose textures are not already decoded)
#define RT_FRAME_LINEAR_LIGHT	( 1u << 11 )

// Sky flags (skyImages[1][3])
#define RT_SKY_BOX				( 1u << 0 )
#define RT_SKY_CLOUDS			( 1u << 1 )
#define RT_SKY_LIGHTS			( 1u << 2 )

// Pixel information flags
#define RT_PIXEL_OWNED			( 1u << 0 )
#define RT_PIXEL_SKY			( 1u << 1 )
#define RT_PIXEL_PORTAL			( 1u << 2 )
#define RT_PIXEL_DYNAMIC		( 1u << 3 )
#define RT_PIXEL_WEAPON			( 1u << 4 )

// Light types
#define RT_LIGHT_POINT			0u
#define RT_LIGHT_POINT_LINEAR	1u
#define RT_LIGHT_SPOT			2u
#define RT_LIGHT_TRIANGLE		3u
#define RT_LIGHT_DLIGHT			4u
// an RTCW light material (negdlightshader, flamedlightshader): never sampled,
// it multiplies what the surfaces it reaches draw (rt_material_light_factor)
#define RT_LIGHT_MATERIAL		5u
// RtLight.color.w holds the type in its low bits and the receivers a light
// skips above them: a lightJunior lights only models (the compilers' light
// grid), a light with the nogrid spawnflag only world surfaces
#define RT_LIGHT_TYPE_MASK		0xFu
#define RT_LIGHT_NO_SURFACES	( 1u << 4 )
#define RT_LIGHT_NO_MODELS		( 1u << 5 )
// a triangle light a glossy reflection can meet, and a sky surface's
// (rt/rt_lights.c RT_MapPrimitiveLights, rt_path.glsl)
#define RT_LIGHT_TRACED			( 1u << 6 )
#define RT_LIGHT_SKY			( 1u << 7 )
// a dynamic light, which an RT_MF_NODLIGHT surface skips
#define RT_LIGHT_DYNAMIC		( 1u << 8 )

typedef struct {
	uint32_t	flags;
	uint32_t	firstStage;
	uint32_t	stageCount;
	uint32_t	lightmapImage;
	float		emit[4];
	float		params[4];
	float		timing[4];
	// companion maps (tr_shader.c R_RtFindMaterialMaps): normal map and
	// material map images (RT_NO_IMAGE when absent), the material map's
	// RT_MATERIAL_MAP_* type, and the stage whose coordinates they follow
	uint32_t	maps[4];
	// the image the map compilers filtered light with at an alphashadow or
	// lightfilter surface (RT_NO_IMAGE for none), RT_LIGHT_IMAGE_* flags
	uint32_t	lightImage[4];
} rtGpuMaterial_t;

typedef struct {
	uint32_t	images[4];
	uint32_t	state[4];
	float		animTc0[4];
	float		tc1[4];
	float		rgbWave[4];
	float		alphaWave[4];
	float		texMod[RT_GPU_MAX_TEXMODS * 2][4];
} rtGpuStage_t;

typedef struct {
	float		position[4];
	float		color[4];
	float		dir[4];
	float		extra[4];
} rtGpuLight_t;

// std140 uniform block
typedef struct {
	float		camOrigin[4];
	float		camForward[4];
	float		camRight[4];
	float		camUp[4];
	float		prevCamOrigin[4];
	float		prevCamForward[4];
	float		prevCamRight[4];
	float		prevCamUp[4];
	float		sunDirection[4];
	float		sunColor[4];
	float		time[4];
	float		lightGridOrigin[4];
	uint32_t	lightGridDims[4];
	float		params0[4];
	float		params1[4];
	uint32_t	flags[4];
	uint32_t	counts[4];
	uint32_t	classPrims[RT_CLASS_COUNT][4];
	float		ambient[4];
	float		jitter[4];
	uint32_t	skyImages[2][4];	// farbox faces 0..5, cloud image, RT_SKY_* flags
	float		minLight[3][4];		// q3map2 floors: lightmapped, vertex-lit, models
	// the compute backend's instances (rt_trace_sw.glsl): node and primitive
	// offsets, class, mask; first opaque and first masked primitive, dynamic
	uint32_t	swInfo[4];			// instance count
	uint32_t	swInstances[RT_SW_MAX_INSTANCES * 2][4];
	float		sunShape[4];		// q3map_sunExt: deviance (radians), the exact direction's chance
	float		skyLight[4];		// q3map_skyLight: colour * value (lightmap units), iterations
	float		dlightShape[4];		// the raster light pass's falloff: its smoothstep share (r_dlightFalloff)
	uint32_t	materialLights[4];	// RTCW light materials: first light, count; rtx_rt_debug_light + 1, its mode
	// the view's global fog, as the raster pass fogs its surfaces (RTCW, ET;
	// Alice's far plane): colour and mode (0 none, 1 linear, 2 exp), then
	// start, end, density, and 1 for radial or 0 for eye-plane distance
	float		fogColor[4];
	float		fogParams[4];
} rtFrameUniforms_t;

// Push constants of the denoiser and composite (rt_denoise_common.glsl)
typedef struct {
	float		traceSize[4];
	float		outputSize[4];
	float		params[4];
	uint32_t	state[4];
	uint32_t	extra[4];
	uint32_t	view[4];
	float		fog[4];				// the view's fog colour, which the composite blends toward
	// the composite's exposure (RT_EXPOSURE_*), then an automatic one's bias
	// in stops and its tone curve's share, or a fixed one's multiplier
	float		exposure[4];
} rtDenoisePush_t;

#define RT_EXPOSURE_NONE		0u
#define RT_EXPOSURE_AUTO		1u	// Q2RTX's tone mapper (rt_tonemap.glsl)
#define RT_EXPOSURE_FIXED		2u
// push.extra[3] bits: raster's lightmap ceiling on irradiance, a highlight
// knee on the final colour, a scene-linear frame (a tone mapper follows)
#define RT_COMPOSITE_SHOULDER	( 1u << 0 )
#define RT_COMPOSITE_KNEE		( 1u << 1 )
#define RT_COMPOSITE_LINEAR		( 1u << 2 )

// The tone mapper's state (rt_denoise_common.glsl's u_tone): this frame's
// histogram of log luminance in fixed point, the curve fitted to the last
// (log2 display luminance at each bin), the adapted log2 luminance, and 1
// once a curve has been fitted since the buffer was cleared. rt_tonemap.comp
// fills the histogram (pass 0, push.view[2]), then fits the curve (pass 1).
#define RT_TONE_BINS			128
#define RT_TONE_LOG_MIN			-24.0f	// log2 luminance at the first bin (rt_tonemap.glsl)
#define RT_TONE_LOG_MAX			8.0f
#define RT_TONE_PASS_HISTOGRAM	0u
#define RT_TONE_PASS_CURVE		1u
typedef struct {
	uint32_t	histogram[RT_TONE_BINS];
	float		curve[RT_TONE_BINS];
	float		adaptedLog;
	uint32_t	frames;
	uint32_t	pad[2];
} rtToneBuffer_t;

typedef char rtGpuMaterialSize[ ( sizeof( rtGpuMaterial_t ) == 96 ) ? 1 : -1 ];
typedef char rtGpuStageSize[ ( sizeof( rtGpuStage_t ) == 256 ) ? 1 : -1 ];
typedef char rtGpuStageTexModOffset[ ( offsetof( rtGpuStage_t, texMod ) == 96 ) ? 1 : -1 ];
typedef char rtGpuLightSize[ ( sizeof( rtGpuLight_t ) == 64 ) ? 1 : -1 ];
typedef char rtFrameUniformsClassOffset[ ( offsetof( rtFrameUniforms_t, classPrims ) == 272 ) ? 1 : -1 ];
typedef char rtFrameUniformsMinLightOffset[ ( offsetof( rtFrameUniforms_t, minLight ) == 272 + 16 * RT_CLASS_COUNT + 64 ) ? 1 : -1 ];
typedef char rtFrameUniformsSwOffset[ ( offsetof( rtFrameUniforms_t, swInfo ) == 272 + 16 * RT_CLASS_COUNT + 64 + 48 ) ? 1 : -1 ];
typedef char rtFrameUniformsSunShapeOffset[ ( offsetof( rtFrameUniforms_t, sunShape ) == 272 + 16 * RT_CLASS_COUNT + 64 + 48 + 16 + 32 * RT_SW_MAX_INSTANCES ) ? 1 : -1 ];
typedef char rtFrameUniformsMaterialLightsOffset[ ( offsetof( rtFrameUniforms_t, materialLights ) == 272 + 16 * RT_CLASS_COUNT + 64 + 48 + 16 + 32 * RT_SW_MAX_INSTANCES + 48 ) ? 1 : -1 ];
typedef char rtFrameUniformsFogOffset[ ( offsetof( rtFrameUniforms_t, fogColor ) == 272 + 16 * RT_CLASS_COUNT + 64 + 48 + 16 + 32 * RT_SW_MAX_INSTANCES + 64 ) ? 1 : -1 ];
typedef char rtFrameUniformsSize[ ( sizeof( rtFrameUniforms_t ) == 272 + 16 * RT_CLASS_COUNT + 64 + 48 + 16 + 32 * RT_SW_MAX_INSTANCES + 96 ) ? 1 : -1 ];
// the 128 bytes every Vulkan device offers
typedef char rtDenoisePushSize[ ( sizeof( rtDenoisePush_t ) == 128 ) ? 1 : -1 ];
typedef char rtToneBufferSize[ ( sizeof( rtToneBuffer_t ) == 8 * RT_TONE_BINS + 16 ) ? 1 : -1 ];

// Descriptor bindings of the ray-tracing pipeline (rt_bindings.glsl, rt_main.rgen)
typedef enum {
	RT_BINDING_TLAS,
	RT_BINDING_WORLD_VERTICES,
	RT_BINDING_WORLD_INDICES,
	RT_BINDING_DYNAMIC_VERTICES,
	RT_BINDING_DYNAMIC_INDICES,
	RT_BINDING_MATERIALS,
	RT_BINDING_STAGES,
	RT_BINDING_LIGHTS,
	RT_BINDING_LIGHT_GRID,
	RT_BINDING_LIGHT_INDICES,
	RT_BINDING_FRAME,
	RT_BINDING_TEXTURES,
	RT_BINDING_OUT_COLOR,
	RT_BINDING_OUT_SPECULAR,
	RT_BINDING_OUT_REFLECTANCE,
	RT_BINDING_OUT_NORMAL_DEPTH,
	RT_BINDING_OUT_EMISSION,
	RT_BINDING_OUT_MOTION,
	RT_BINDING_OUT_INFO,
	RT_BINDING_DYNAMIC_PREVIOUS,	// dynamic vertices' world positions last frame
	RT_BINDING_PRIMITIVE_LIGHTS,	// the light each world triangle emits as
	RT_BINDING_COUNT,
	// the compute backend traces its own BVHs: RT_BINDING_TLAS holds the
	// world's nodes, and these follow
	RT_BINDING_SW_WORLD_TRIS = RT_BINDING_COUNT,
	RT_BINDING_SW_DYNAMIC_NODES,
	RT_BINDING_SW_DYNAMIC_TRIS,
	RT_BINDING_SW_COUNT
} rtBinding_t;

// A BVH node of the compute backend (rt/rt_bvh.c, rt_trace_sw.glsl)
typedef struct {
	float		bmin[3];
	uint32_t	first;		// interior: left child (the right follows it); leaf: first triangle
	float		bmax[3];
	uint32_t	count;		// leaf: triangle count; interior: 0
} rtBvhNode_t;

// A leaf triangle of the compute backend: its corners, and in v0[3] (as
// uint bits) the triangle's index in the world or dynamic index buffer
typedef struct {
	float		v0[4];
	float		v1[4];
	float		v2[4];
} rtBvhTri_t;

typedef char rtBvhNodeSize[ ( sizeof( rtBvhNode_t ) == 32 ) ? 1 : -1 ];
typedef char rtBvhTriSize[ ( sizeof( rtBvhTri_t ) == 48 ) ? 1 : -1 ];

#endif
