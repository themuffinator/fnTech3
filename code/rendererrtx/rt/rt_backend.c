/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Path tracer backend

Scene acceleration structures, descriptors, the trace and the composite of
the RTX renderer's native mode (rtx_rt_mode 2). Included by vk.c, whose
static Vulkan entry points and RT buffer helpers it uses.

Geometry
  The static world is extracted once per map into one vertex/index buffer
  pair. Its triangles are ordered opaque, masked, then special (sky and
  portal), and two BLASes cover them: class WORLD with an opaque and a masked
  geometry, and class WORLD_SPECIAL. Each frame the entities are extracted
  into three classes - the scene's models (DYNAMIC), the first-person weapon
  (WEAPON) and the viewer's own body, which only shadows and reflections see
  (BODY) - and built into per-frame BLASes of the same two-geometry shape.

Tracing backends
  With VK_KHR_ray_tracing_pipeline the instances are acceleration structures
  and rt_main.rgen traces them. Without it, or with rtx_rt_backend 2, the
  compute backend builds the same instances as BVHs of its own on the CPU
  (rt_bvh.c) - the world's once per map, a frame's dynamic classes with the
  frame - and rt_trace.comp runs the same per-pixel work (rt_path.glsl),
  traversing them (rt_trace_sw.glsl). Both read the same geometry, materials
  and lights.

Filters
  World surfaces compiled with surfaceparm alphashadow or lightfilter (id's
  q3map only knows alphashadow) filter the map's light instead of blocking it.
  They form one more world instance, class WORLD_FILTER, that only filter
  rays test (rt_filter.glsl): a traced one is also in the no-shadow partition,
  where primary and secondary rays see it; a blended one, drawn by raster,
  is in the filter instance alone.

Frames in flight
  Everything a frame's commands read that changes per frame (dynamic
  geometry, BLASes, TLAS, materials, uniforms, descriptor sets) has one copy
  per command buffer. vk_begin_frame waits for a command buffer's fence
  before recording into it again, so its copy is idle when rewritten, and
  every build is recorded into the frame's own command buffer: nothing here
  waits for the GPU during a frame.

Ownership
  The raster renderer draws the traced surfaces first (their colour is
  replaced, their depth kept for the overlay), then the trace runs, the
  composite writes traced pixels over the raster frame, and the overlay pass
  draws everything the trace does not own, plus the fog pass of each traced
  surface inside a fog volume. rt_material.c decides ownership;
  vk_rt_surface_owner applies it to a draw surface for tr_backend.c.
===========================================================================
*/

#define RT_FRAMES NUM_COMMAND_BUFFERS
#include "rt_quality.h"
#define RT_DYNAMIC_CLASSES 3	// DYNAMIC, WEAPON, BODY
#define RT_WORLD_INSTANCES 4	// world, no-shadow, special, filter
#define RT_MAX_INSTANCES 8		// TLAS instances: the world's and the dynamic classes
typedef char rtInstancesFit[ ( RT_WORLD_INSTANCES + RT_DYNAMIC_CLASSES <= RT_MAX_INSTANCES ) ? 1 : -1 ];
typedef char rtSwInstancesFit[ ( RT_WORLD_INSTANCES + RT_DYNAMIC_CLASSES <= RT_SW_MAX_INSTANCES ) ? 1 : -1 ];

// The world's triangle partitions, in buffer order: the surfaces the map's
// lights are blocked by, those they pass through (RT_CLASS_WORLD_NOSHADOW),
// each opaque then masked, sky and portal surfaces, and those that filter
// the map's light (RT_CLASS_WORLD_FILTER). A traced filtering surface is in
// a no-shadow partition and the filter partition both.
enum {
	RT_PART_OPAQUE,
	RT_PART_MASKED,
	RT_PART_NOSHADOW_OPAQUE,
	RT_PART_NOSHADOW_MASKED,
	RT_PART_SPECIAL,
	RT_PART_FILTER,
	RT_WORLD_PARTS
};

// World vertex markers (lightmapData[3], extraction only)
#define RT_MARK_NOSHADOW	1		// the map's lights pass through
#define RT_MARK_FILTER		2		// ... filtered

// every renderer image has a slot in the texture table
typedef char rtTextureTableCoversImages[ ( RT_MAX_TEXTURES >= MAX_DRAWIMAGES ) ? 1 : -1 ];
#define RT_TIMING_STAGES 4		// scene build, trace, denoise, composite
#define RT_TIMING_QUERIES ( RT_TIMING_STAGES + 1 )

typedef struct {
	rtxVkRtBuffer_t	buffer;
	void			*mapped;
} rtMappedBuffer_t;

typedef struct {
	rtxVkRtAccelerationStructure_t	as;
	VkDeviceSize	buildScratchSize;
	uint32_t		geometryCount;		// 2: opaque, masked; 1: special
	uint32_t		primCount[2];
	uint32_t		firstPrim[2];		// global primitive index of each geometry
	qboolean		built;
	uint32_t		vertexCount;
	uint32_t		vertexBase;
	VkBuildAccelerationStructureFlagsKHR buildFlags;
	qboolean		anyHitOnly;
	uint32_t		*topology;
	uint32_t		topologyCapacity, topologyCount, updateAge;
} rtBlas_t;

typedef struct {
	VkImage			image;
	VkImageView		view;
	VkDeviceMemory	memory;
	VkFormat		format;
	VkDeviceSize	allocatedBytes;
} rtImage_t;

// The trace's outputs and the denoiser's working images. Pairs alternate:
// normal/depth, info and history by frame, filter images by iteration.
typedef enum {
	RT_IMG_DIFFUSE,			// noisy diffuse irradiance
	RT_IMG_SPECULAR,		// noisy specular radiance
	RT_IMG_REFLECTANCE,
	RT_IMG_EMISSION,
	RT_IMG_MOTION,			// motion, previous view depth
	RT_IMG_NORMAL_DEPTH,	// and RT_IMG_NORMAL_DEPTH + 1
	RT_IMG_INFO = RT_IMG_NORMAL_DEPTH + 2,
	RT_IMG_HIST_DIFFUSE = RT_IMG_INFO + 2,
	RT_IMG_HIST_SPECULAR = RT_IMG_HIST_DIFFUSE + 2,
	RT_IMG_HIST_MOMENTS = RT_IMG_HIST_SPECULAR + 2,
	RT_IMG_FILTER_DIFFUSE = RT_IMG_HIST_MOMENTS + 2,
	RT_IMG_FILTER_SPECULAR = RT_IMG_FILTER_DIFFUSE + 2,
	RT_IMG_INDIRECT = RT_IMG_FILTER_SPECULAR + 2,
	RT_IMG_HIST_INDIRECT,
	RT_IMG_FILTER_INDIRECT = RT_IMG_HIST_INDIRECT + 2,
	RT_IMG_COUNT = RT_IMG_FILTER_INDIRECT + 2
} rtImageId_t;

typedef struct {
	rtMappedBuffer_t	vertices;
	rtMappedBuffer_t	indices;
	rtMappedBuffer_t	materials;
	rtMappedBuffer_t	stages;
	rtMappedBuffer_t	frame;
	rtMappedBuffer_t	instances;
	rtMappedBuffer_t	lights;			// static lights, then this frame's dlights
	rtMappedBuffer_t	previous;		// dynamic vertices' positions last frame
	uint32_t			lightGeneration;
	uint32_t			materialGeneration;
	uint32_t			uploadedMaterials;
	uint32_t			uploadedStages;
	rtBlas_t			blas[RT_DYNAMIC_CLASSES];
	rtxVkRtAccelerationStructure_t tlas;
	VkDeviceSize		tlasScratchSize;
	uint32_t			tlasInstanceCapacity;
	rtxVkRtBuffer_t		scratch;
	// the compute backend: the dynamic classes' BVHs and every instance
	rtMappedBuffer_t	bvhNodes;
	rtMappedBuffer_t	bvhTris;
	uint32_t			swInstanceCount;
	uint32_t			swInstances[RT_SW_MAX_INSTANCES * 2][4];
	VkDescriptorSet		rtSet;
	VkDescriptorSet		denoiseSet;
	int					textureCount;		// tr.numImages the texture array was written for
	// the tone mapper's state as this slot's frame left it (rtToneBuffer_t),
	// read when the slot is reused
	rtMappedBuffer_t	tone;
	qboolean			toneWritten;
} rtFrame_t;

// Motion uses profile identities where the module supplies one; ambiguous
// identity-free submissions are never paired merely by proximity.
#include "rt_motion.h"

typedef struct {
	rtMotionEntity_t	*entities;
	uint32_t			numEntities, entityCapacity;
	float				*positions;		// last frame: xyz per vertex
	uint32_t			numPositions, positionCapacity;
} rtMotionFrame_t;

// A compute backend tree: where it starts in its node and triangle buffers
typedef struct {
	uint32_t			nodeOffset;
	uint32_t			triOffset;
	qboolean			valid;
} rtSwTree_t;

typedef struct {
	rtBvh_t bvh;
	uint32_t *indices;
	uint32_t indexCapacity, numIndices, numVertices, primBias;
	uint32_t age;
	float buildCost;
} rtDynamicBvh_t;

typedef struct {
	qboolean			failed;				// a resource could not be created; stay raster
	const world_t		*world;
	const shader_t		*defaultShader;

	// static world
	rtxVkRtBuffer_t		worldVertices;
	rtxVkRtBuffer_t		worldIndices;
	uint32_t			worldVertexCount;
	uint32_t			worldIndexCount;
	rtBlas_t			worldBlas;
	rtBlas_t			noShadowBlas;	// surfaces the map's lights pass through
	rtBlas_t			specialBlas;
	rtBlas_t			filterBlas;		// surfaces they pass through filtered
	qboolean			worldReady;
	uint32_t			worldTriangles[RT_WORLD_PARTS];
	uint32_t			skyImages[2][4];	// the world sky's farbox, cloud layer and flags
	rtxVkRtBuffer_t		lightGrid;
	rtxVkRtBuffer_t		lightIndices;
	rtxVkRtBuffer_t		primitiveLights;	// per world triangle: the light it emits as, ~0 for none
	// the compute backend's world: a tree per world instance (world,
	// no-shadow, special, filter), and the builder's arrays, kept between
	// frames
	rtxVkRtBuffer_t		worldBvhNodes;
	rtxVkRtBuffer_t		worldBvhTris;
	rtSwTree_t			worldTrees[RT_WORLD_INSTANCES];
	rtBvh_t				bvh;
	rtDynamicBvh_t		dynamicBvh[RT_DYNAMIC_CLASSES];
	uint32_t			bvhBuilds, bvhRefits;
	uint32_t			blasBuilds, blasUpdates;

	rtFrame_t			frames[RT_FRAMES];

	// trace-resolution images and the display-resolution composite
	rtImage_t			images[RT_IMG_COUNT];
	rtImage_t			output;
	uint32_t			traceWidth, traceHeight;
	uint32_t			outputWidth, outputHeight;
	uint32_t			parity;				// which normal/depth, info and history set is current
	qboolean			historyValid;
	int					temporalTime;	// wall clock of the previous traced frame
	int					indirectDivisor, budgetDivisor, budgetMinimum, budgetCooldown;
	qboolean			budgetSample;
	rtFrameStats_t		gpuFrameStats;

	VkDescriptorPool		pool;
	VkDescriptorSetLayout	rtSetLayout;
	VkDescriptorSetLayout	denoiseSetLayout;
	VkPipelineLayout		rtPipelineLayout;
	VkPipelineLayout		denoisePipelineLayout;
	VkPipeline				rtPipeline;
	VkPipeline				temporalPipeline;
	VkPipeline				atrousPipeline;
	VkPipeline				compositePipeline;
	VkPipeline				tonemapPipeline;
	rtxVkRtBuffer_t			sbt;
	VkStridedDeviceAddressRegionKHR sbtRaygen, sbtMiss, sbtHit, sbtCallable;

	// a small buffer bound where a frame has nothing (no lights yet)
	rtxVkRtBuffer_t			dummy;

	// the tone mapper's histogram, curve and adapted luminance
	// (rtToneBuffer_t), cleared before its first use and after a new world;
	// when it last fitted a curve (ri.Milliseconds), for the adaptation
	rtxVkRtBuffer_t			tone;
	qboolean				toneClear;
	qboolean				toneActive;
	int						toneTime;
	// the last state read back: the metered log2 luminance and the curve
	float					toneMeteredLog;
	float					toneCurve[RT_TONE_BINS];
	qboolean				toneRead;

	// per-frame CPU scratch geometry, kept between frames
	rtxRtCpuGeometry_t		dynamic[RT_DYNAMIC_CLASSES];
	// this frame's dynamic entities as extracted, and last frame's
	rtMotionFrame_t			motion[2];
	uint32_t				*partition;
	uint32_t				partitionCapacity;
	uint32_t				dynamicTriangles[RT_DYNAMIC_CLASSES][2];

	// previous camera for reprojection
	qboolean				hasPrevCamera;
	float					prevOrigin[4], prevForward[4], prevRight[4], prevUp[4];

	// still-frame accumulation
	uint32_t				accumulated;
	double					accumulateTime;
	int						settingsCount;	// RT_TraceSettingsCount
	// what this frame and the last traced of the models and dynamic lights
	// (RT_ExtractDynamic, RT_UploadLights)
	uint32_t				sceneHash, prevSceneHash;
	uint32_t				numDlights;
	uint32_t				numMaterialLights;	// RTCW light materials after the dynamic lights

	qboolean				loggedActive;
	uint32_t				loggedFailures;	// RT_FrameFailed reasons already reported

	// GPU timestamps: scene build, trace, denoise, composite, per frame slot
	VkQueryPool				timingPool;
	qboolean				timingWritten[RT_FRAMES];
	float					gpuMs[RT_TIMING_STAGES];	// smoothed
	float					cpuMs;						// smoothed extraction and upload
	int						timingReports;
} rtBackend_t;

static rtBackend_t s_rt;

static const uint32_t s_rtDynamicClass[RT_DYNAMIC_CLASSES] = {
	RT_CLASS_DYNAMIC, RT_CLASS_WEAPON, RT_CLASS_BODY
};
static const uint32_t s_rtDynamicMask[RT_DYNAMIC_CLASSES] = {
	RT_MASK_DYNAMIC, RT_MASK_WEAPON, RT_MASK_BODY
};

// The compute backend (rtx_rt_backend) traces BVHs of its own in place of
// acceleration structures
static qboolean RT_Compute( void )
{
	return ( vk.caps.rtBackend == RTX_RT_BACKEND_COMPUTE ) ? qtrue : qfalse;
}

// The pipeline stage the trace runs in
static VkPipelineStageFlags RT_TraceStage( void )
{
	return RT_Compute() ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
}

/*
====================================================================

Buffers and images

====================================================================
*/

static void RT_DestroyMapped( rtMappedBuffer_t *buffer )
{
	if ( buffer->mapped && buffer->buffer.memory != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, buffer->buffer.memory );
	}
	buffer->mapped = NULL;
	vk_rt_destroy_buffer( &buffer->buffer );
}

// Host-visible, coherent and persistently mapped; grows to minSize. With
// deviceLocal, in memory the GPU reads locally where the device offers it.
static qboolean RT_EnsureMapped( rtMappedBuffer_t *buffer, VkDeviceSize minSize, VkBufferUsageFlags usage,
	qboolean deviceAddress, qboolean deviceLocal, const char *name )
{
	VkDeviceSize size;
	VkResult result;

	if ( minSize == 0 ) {
		minSize = 256;
	}
	if ( buffer->buffer.buffer != VK_NULL_HANDLE && buffer->buffer.capacity >= minSize ) {
		return qtrue;
	}
	size = buffer->buffer.capacity ? buffer->buffer.capacity : 4096;
	while ( size < minSize ) {
		if ( size > ( (VkDeviceSize)-1 ) / 2 ) {
			ri.Printf( PRINT_ERROR, "RTX RT: '%s' cannot grow to %llu bytes\n", name, (unsigned long long)minSize );
			return qfalse;
		}
		size *= 2;
	}
	RT_DestroyMapped( buffer );
	if ( !vk_rt_create_buffer_ex( size, usage,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			deviceLocal && ( !rtx_rt_device_local || rtx_rt_device_local->integer ) ?
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0, deviceAddress, name, &buffer->buffer ) ) {
		return qfalse;
	}
	result = qvkMapMemory( vk.device, buffer->buffer.memory, 0, VK_WHOLE_SIZE, 0, &buffer->mapped );
	if ( result != VK_SUCCESS || !buffer->mapped ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to map '%s': %s\n", name, vk_result_string( result ) );
		buffer->mapped = NULL;
		vk_rt_destroy_buffer( &buffer->buffer );
		return qfalse;
	}
	return qtrue;
}

/*
A device-local buffer holding bytes of data, copied through a staging
buffer; the copy's barrier makes it visible to every later command.
*/
static qboolean RT_CreateStatic( rtxVkRtBuffer_t *buffer, const void *data, size_t bytes, VkBufferUsageFlags usage,
	qboolean deviceAddress, const char *name )
{
	rtxVkRtBuffer_t staging;
	VkBufferCopy region;
	VkMemoryBarrier barrier;
	VkCommandBuffer cmd;
	qboolean ok;

	if ( !vk_rt_create_buffer( (VkDeviceSize)bytes, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, deviceAddress, name, buffer ) ) {
		return qfalse;
	}
	if ( !vk_rt_create_buffer( (VkDeviceSize)bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, qfalse, "rt staging", &staging ) ) {
		vk_rt_destroy_buffer( buffer );
		return qfalse;
	}
	if ( !vk_rt_upload_buffer_data( &staging, data, bytes ) ) {
		vk_rt_destroy_buffer( &staging );
		vk_rt_destroy_buffer( buffer );
		return qfalse;
	}
	cmd = begin_command_buffer();
	Com_Memset( &region, 0, sizeof( region ) );
	region.size = (VkDeviceSize)bytes;
	qvkCmdCopyBuffer( cmd, staging.buffer, buffer->buffer, 1, &region );
	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 1, &barrier, 0, NULL, 0, NULL );
	ok = end_command_buffer_soft( cmd, __func__ );
	vk_rt_destroy_buffer( &staging );
	if ( !ok ) {
		vk_rt_destroy_buffer( buffer );
	}
	return ok;
}

static void RT_DestroyImage( rtImage_t *image )
{
	if ( image->view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
	}
	if ( image->image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, image->image, NULL );
	}
	if ( image->memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, image->memory, NULL );
	}
	Com_Memset( image, 0, sizeof( *image ) );
}

static qboolean RT_CreateImage( rtImage_t *image, uint32_t width, uint32_t height, VkFormat format,
	VkImageUsageFlags usage, const char *name )
{
	VkImageCreateInfo imageInfo;
	VkMemoryRequirements requirements;
	VkMemoryAllocateInfo allocInfo;
	VkImageViewCreateInfo viewInfo;
	VkResult result;

	Com_Memset( image, 0, sizeof( *image ) );
	Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	result = qvkCreateImage( vk.device, &imageInfo, NULL, &image->image );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to create image '%s': %s\n", name, vk_result_string( result ) );
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, image->image, &requirements );
	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = requirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	result = qvkAllocateMemory( vk.device, &allocInfo, NULL, &image->memory );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to allocate image '%s': %s\n", name, vk_result_string( result ) );
		RT_DestroyImage( image );
		return qfalse;
	}
	result = qvkBindImageMemory( vk.device, image->image, image->memory, 0 );
	if ( result != VK_SUCCESS ) {
		RT_DestroyImage( image );
		return qfalse;
	}
	Com_Memset( &viewInfo, 0, sizeof( viewInfo ) );
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image->image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;
	result = qvkCreateImageView( vk.device, &viewInfo, NULL, &image->view );
	if ( result != VK_SUCCESS ) {
		RT_DestroyImage( image );
		return qfalse;
	}
	image->format = format;
	image->allocatedBytes = requirements.size;
	SET_OBJECT_NAME( image->image, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	return qtrue;
}

static const struct {
	VkFormat	format;
	const char	*name;
} s_rtImageInfo[RT_IMG_COUNT] = {
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt diffuse" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt specular" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt reflectance" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt emission" },
	{ VK_FORMAT_R32G32_UINT, "rt motion" },
	{ VK_FORMAT_R32G32_UINT, "rt normal depth 0" },
	{ VK_FORMAT_R32G32_UINT, "rt normal depth 1" },
	{ VK_FORMAT_R32_UINT, "rt info 0" },
	{ VK_FORMAT_R32_UINT, "rt info 1" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt history diffuse 0" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt history diffuse 1" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt history specular 0" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt history specular 1" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt history moments 0" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt history moments 1" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt filter diffuse 0" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt filter diffuse 1" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt filter specular 0" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt filter specular 1" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt indirect" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt history indirect 0" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt history indirect 1" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt filter indirect 0" },
	{ VK_FORMAT_R16G16B16A16_SFLOAT, "rt filter indirect 1" },
};

static void RT_DestroyImages( void )
{
	int i;
	for ( i = 0; i < RT_IMG_COUNT; i++ ) {
		RT_DestroyImage( &s_rt.images[i] );
	}
	RT_DestroyImage( &s_rt.output );
	s_rt.traceWidth = s_rt.traceHeight = 0;
	s_rt.outputWidth = s_rt.outputHeight = 0;
	s_rt.historyValid = qfalse;
}

/*
The traced view's rectangle in the scene colour, in render pixels from its
top-left corner: the raster passes' viewport (get_viewport_rect), kept inside
the image. A game may inset its main view (Enemy Territory's is one pixel in
from each edge); the trace covers the view and the composite lands on it.
*/
static void RT_ViewRect( VkRect2D *rect )
{
	int32_t x, y, width, height;

	get_viewport_rect( rect );
	x = MIN( MAX( rect->offset.x, 0 ), MAX( (int32_t)vk.renderWidth - 1, 0 ) );
	y = MIN( MAX( rect->offset.y, 0 ), MAX( (int32_t)vk.renderHeight - 1, 0 ) );
	width = MIN( (int32_t)rect->extent.width, (int32_t)vk.renderWidth - x );
	height = MIN( (int32_t)rect->extent.height, (int32_t)vk.renderHeight - y );
	rect->offset.x = x;
	rect->offset.y = y;
	rect->extent.width = (uint32_t)MAX( 1, width );
	rect->extent.height = (uint32_t)MAX( 1, height );
}

static qboolean RT_EnsureImages( uint32_t width, uint32_t height )
{
	VkImageMemoryBarrier barriers[ RT_IMG_COUNT + 1 ];
	VkCommandBuffer cmd;
	int i;

	if ( s_rt.traceWidth == width && s_rt.traceHeight == height &&
		s_rt.outputWidth == width && s_rt.outputHeight == height && s_rt.output.image != VK_NULL_HANDLE ) {
		return qtrue;
	}
	if ( s_rt.output.image != VK_NULL_HANDLE && !vk_rt_wait_for_inflight_frames( "RT image resize" ) ) {
		return qfalse;
	}
	RT_DestroyImages();

	for ( i = 0; i < RT_IMG_COUNT; i++ ) {
		if ( !RT_CreateImage( &s_rt.images[i], width, height, s_rtImageInfo[i].format, VK_IMAGE_USAGE_STORAGE_BIT,
				s_rtImageInfo[i].name ) ) {
			RT_DestroyImages();
			return qfalse;
		}
	}
	if ( !RT_CreateImage( &s_rt.output, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "rt composite" ) ) {
		RT_DestroyImages();
		return qfalse;
	}

	Com_Memset( barriers, 0, sizeof( barriers ) );
	for ( i = 0; i < (int)ARRAY_LEN( barriers ); i++ ) {
		barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[i].image = i < RT_IMG_COUNT ? s_rt.images[i].image : s_rt.output.image;
		barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[i].subresourceRange.levelCount = 1;
		barriers[i].subresourceRange.layerCount = 1;
		barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	}
	cmd = begin_command_buffer();
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 0, NULL, 0, NULL, ARRAY_LEN( barriers ), barriers );
	if ( !end_command_buffer_soft( cmd, __func__ ) ) {
		RT_DestroyImages();
		return qfalse;
	}

	s_rt.traceWidth = s_rt.outputWidth = width;
	s_rt.traceHeight = s_rt.outputHeight = height;
	{
		VkDeviceSize bytes = s_rt.output.allocatedBytes;
		for ( i = 0; i < RT_IMG_COUNT; i++ ) bytes += s_rt.images[i].allocatedBytes;
		ri.Printf( PRINT_ALL, "RTX RT images: %ux%u, %u images, %.2f MiB allocated\n",
			width, height, RT_IMG_COUNT + 1u, (double)bytes / (1024.0 * 1024.0) );
	}
	s_rt.hasPrevCamera = qfalse;
	s_rt.historyValid = qfalse;
	return qtrue;
}

/*
====================================================================

Geometry

====================================================================
*/

static void RT_ResetCpuGeometry( rtxRtCpuGeometry_t *geometry )
{
	geometry->numVertices = 0;
	geometry->numIndices = 0;
	geometry->numMaterials = 0;
	geometry->sourceTriangleCount = 0;
	geometry->degenerateTriangleCount = 0;
	geometry->maskedTriangleCount = 0;
	geometry->anyHitTriangleCount = 0;
	geometry->sourceEntityCount = 0;
	geometry->sourceEffectCount = 0;
}

// Per-geometry material slots to global materials, from firstVertex on.
static void RT_RemapMaterials( rtxRtCpuGeometry_t *geometry, uint32_t firstVertex )
{
	uint32_t i;

	for ( i = firstVertex; i < geometry->numVertices; i++ ) {
		rtxRtPackedVertex_t *v = &geometry->vertices[i];
		const shader_t *shader = ( v->materialIndex < geometry->numMaterials ) ?
			geometry->materialShaders[ v->materialIndex ] : tr.defaultShader;
		v->materialIndex = RT_MaterialIndexForShader( shader );
	}
	geometry->numMaterials = 0;
}

static qboolean RT_EnsurePartitionScratch( uint32_t count )
{
	if ( count > ( 1u << 29 ) ) {
		return qfalse;
	}
	if ( count <= s_rt.partitionCapacity ) {
		return qtrue;
	}
	if ( s_rt.partition ) {
		RT_HostFree( s_rt.partition );
	}
	s_rt.partitionCapacity = MAX( count, 4096u );
	s_rt.partition = (uint32_t *)RT_HostAlloc( (int)( s_rt.partitionCapacity * sizeof( uint32_t ) ) );
	if ( !s_rt.partition ) {
		s_rt.partitionCapacity = 0;
		return qfalse;
	}
	return qtrue;
}

/*
Reorder a geometry's triangles into the partitions by material owner and the
world's markers, dropping degenerate triangles and those the trace neither
owns nor filters light with. Special (sky, portal) and filter triangles are
world only; a traced filtering triangle is written to its no-shadow
partition and to the filter partition.
*/
static qboolean RT_PartitionTriangles( rtxRtCpuGeometry_t *geometry, qboolean world, uint32_t counts[RT_WORLD_PARTS] )
{
	uint32_t pass;
	uint32_t write = 0;
	uint32_t i;

	Com_Memset( counts, 0, RT_WORLD_PARTS * sizeof( counts[0] ) );
	if ( geometry->numIndices < 3 ) {
		geometry->numIndices = 0;
		return qtrue;
	}
	if ( !RT_EnsurePartitionScratch( geometry->numIndices * ( world ? 2u : 1u ) ) ) {
		return qfalse;
	}

	for ( pass = 0; pass < RT_WORLD_PARTS; pass++ ) {
		for ( i = 0; i + 2 < geometry->numIndices; i += 3 ) {
			const uint32_t i0 = geometry->indices[i + 0];
			const uint32_t i1 = geometry->indices[i + 1];
			const uint32_t i2 = geometry->indices[i + 2];
			uint32_t material, marker;
			rtOwner_t owner;
			uint32_t bucket = RT_WORLD_PARTS;

			if ( i0 >= geometry->numVertices || i1 >= geometry->numVertices || i2 >= geometry->numVertices ||
				i0 == i1 || i1 == i2 || i0 == i2 ) {
				continue;
			}
			material = geometry->vertices[i0].materialIndex;
			owner = RT_MaterialOwner( material );
			// RT_ExtractWorld marks what the map's lights pass through, or
			// pass through filtered
			marker = world ? (uint32_t)( geometry->vertices[i0].lightmapData[3] + 0.5f ) : 0u;
			if ( owner == RT_OWNER_TRACED ) {
				// An entity's no-shadow policy needs any-hit on shadow rays,
				// even when its material is otherwise opaque.
				bucket = ( RT_MaterialMasked( material ) || ( !world && geometry->vertices[i0].normal[3] < 0.0f ) ) ?
					RT_PART_MASKED : RT_PART_OPAQUE;
				if ( marker & ( RT_MARK_NOSHADOW | RT_MARK_FILTER ) ) {
					bucket += RT_PART_NOSHADOW_OPAQUE;
				}
			} else if ( owner == RT_OWNER_BASE_RASTER && world ) {
				bucket = RT_PART_SPECIAL;
				marker = 0;
			}
			if ( !( bucket == pass || ( pass == RT_PART_FILTER && ( marker & RT_MARK_FILTER ) ) ) ) {
				continue;
			}
			s_rt.partition[write + 0] = i0;
			s_rt.partition[write + 1] = i1;
			s_rt.partition[write + 2] = i2;
			write += 3;
			counts[pass]++;
		}
	}
	if ( write > geometry->numIndices &&
		!vk_rt_cpu_geometry_reserve( geometry, 0, write - geometry->numIndices ) ) {
		return qfalse;
	}
	Com_Memcpy( geometry->indices, s_rt.partition, write * sizeof( uint32_t ) );
	geometry->numIndices = write;
	return qtrue;
}

// The skybox faces (tr_sky.c's rt bk lf ft up dn order), the first cloud
// layer and whether the sky is itself a q3map surface light.
static void RT_RecordSky( const shader_t *sky )
{
	int face;

	sky = RT_ResolveShader( sky );
	Com_Memset( s_rt.skyImages, 0xff, sizeof( s_rt.skyImages ) );
	s_rt.skyImages[1][3] = 0;
	for ( face = 0; face < 6; face++ ) {
		const int32_t index = RT_ImageIndex( sky->sky.outerbox[face] );
		s_rt.skyImages[face >> 2][face & 3] = index >= 0 ? (uint32_t)index : RT_NO_IMAGE;
	}
	if ( sky->sky.outerbox[0] && s_rt.skyImages[0][0] != RT_NO_IMAGE ) {
		s_rt.skyImages[1][3] |= RT_SKY_BOX;
	}
	if ( sky->rtNumStages > 0 && sky->rtStages ) {
		const int32_t index = RT_ImageIndex( sky->rtStages[0].images[0] );
		if ( index >= 0 ) {
			s_rt.skyImages[1][2] = (uint32_t)index;
			s_rt.skyImages[1][3] |= RT_SKY_CLOUDS;
		}
	}
	if ( sky->surfaceLightValid && sky->surfaceLight > 0.0f ) {
		s_rt.skyImages[1][3] |= RT_SKY_LIGHTS;
	}
}

// One surface of the static world into its geometry; qfalse when the
// geometry could not grow
static qboolean RT_ExtractWorldSurface( rtxRtCpuGeometry_t *geometry, const msurface_t *surface, int filterFlags )
{
	const shader_t *shader = surface->shader ? surface->shader : tr.defaultShader;
	const uint32_t material = RT_MaterialIndexForShader( shader );
	const rtOwner_t owner = RT_MaterialOwner( material );
	const qboolean filters = ( shader->rtCompiledFilter & filterFlags ) && !RT_ShaderIsSky( shader );
	uint32_t firstVertex;
	uint32_t marker = 0;
	qboolean ok = qtrue;

	if ( shader->isSky && !( s_rt.skyImages[1][3] & ( RT_SKY_BOX | RT_SKY_CLOUDS ) ) ) {
		RT_RecordSky( shader );
	}
	// a blended surface stays raster, but a filtering one still shadows
	if ( owner == RT_OWNER_RASTER && !filters ) {
		return qtrue;
	}
	firstVertex = geometry->numVertices;
	switch ( *surface->data ) {
	case SF_FACE:
		ok = vk_rt_append_surface_face_geometry( geometry, (const srfSurfaceFace_t *)surface->data, shader, 0 );
		break;
	case SF_GRID:
		ok = vk_rt_append_surface_grid_geometry( geometry, (const srfGridMesh_t *)surface->data, shader, 0 );
		break;
	case SF_TRIANGLES:
		ok = vk_rt_append_surface_triangles_geometry( geometry, (const srfTriangles_t *)surface->data, shader, 0 );
		break;
	default:
		break;
	}
	if ( !ok ) {
		return qfalse;
	}
	// the compilers cast no shadows from translucent surfaces that do
	// not filter, nor from patches unless run with -patchshadows
	// (rtx_rt_patch_shadows); a filtering surface filters instead
	if ( filters ) {
		marker = RT_MARK_FILTER;
	} else if ( shader->rtCompiledTranslucent ||
		( *surface->data == SF_GRID && rtx_rt_patch_shadows && !rtx_rt_patch_shadows->integer ) ) {
		marker = RT_MARK_NOSHADOW;
	}
	if ( marker ) {
		uint32_t v;
		for ( v = firstVertex; v < geometry->numVertices; v++ ) {
			geometry->vertices[v].lightmapData[3] = (float)marker;
		}
	}
	return qtrue;
}

static qboolean RT_ExtractWorld( rtxRtCpuGeometry_t *geometry )
{
	const bmodel_t *worldModel;
	// id's q3map filters with alphashadow alone; q3map2 with lightfilter too
	const int filterFlags = RT_MapIsQ3map2() ? ( RT_COMPILED_ALPHASHADOW | RT_COMPILED_LIGHTFILTER ) :
		RT_COMPILED_ALPHASHADOW;
	int i, s;

	vk_rt_cpu_geometry_init( geometry );
	Com_Memset( s_rt.skyImages, 0, sizeof( s_rt.skyImages ) );
	if ( !tr.world || !tr.worldMapLoaded || !tr.world->bmodels ) {
		return qfalse;
	}
	if ( tr.world->mohaaFirstBuiltSurface < 0 || tr.world->mohaaNumBuiltSurfaces < 0 ||
		tr.world->mohaaFirstBuiltSurface > tr.world->numsurfaces ||
		tr.world->mohaaNumBuiltSurfaces > tr.world->numsurfaces - tr.world->mohaaFirstBuiltSurface ||
		( tr.world->mohaaNumBuiltSurfaces && !tr.world->surfaces ) ) {
		return qfalse;
	}
	worldModel = &tr.world->bmodels[0];
	for ( i = 0; i < worldModel->numSurfaces; i++ ) {
		if ( !RT_ExtractWorldSurface( geometry, &worldModel->firstSurface[i], filterFlags ) ) {
			vk_rt_cpu_geometry_free( geometry );
			return qfalse;
		}
	}
	// MOHAA appends its terrain and static scenery after the BSP's original
	// surfaces, including the inline models. Raster finds them in the leaf
	// lists, but model zero's contiguous range cannot include them. Trace
	// the builder's separate range too, without baking doors into the world.
	// Decals retain their material's raster ownership in ExtractWorldSurface.
	for ( i = 0; i < tr.world->mohaaNumBuiltSurfaces; i++ ) {
		const msurface_t *surface = &tr.world->surfaces[tr.world->mohaaFirstBuiltSurface + i];
		if ( !RT_ExtractWorldSurface( geometry, surface, filterFlags ) ) {
			vk_rt_cpu_geometry_free( geometry );
			return qfalse;
		}
	}
	// Quake Live's advertisements: brush models the renderer draws as world
	// surfaces (R_UpdateAdvertisements), in place, which traced pixels would
	// otherwise cover with whatever lies behind them
	for ( i = 0; i < tr.world->numAdvertisements; i++ ) {
		const bmodel_t *bmodel = tr.world->advertisements[i].bmodel;
		for ( s = 0; bmodel && s < bmodel->numSurfaces; s++ ) {
			if ( !RT_ExtractWorldSurface( geometry, &bmodel->firstSurface[s], filterFlags ) ) {
				vk_rt_cpu_geometry_free( geometry );
				return qfalse;
			}
		}
	}
	RT_RemapMaterials( geometry, 0 );
	return qtrue;
}

// Which dynamic class an entity belongs to, or -1 if the trace skips it.
static int RT_EntityClass( const trRefEntity_t *ent )
{
	const model_t *model;
	int renderfx = ent->e.renderfx;

	if ( ent->e.reType != RT_MODEL || ent->e.hModel <= 0 ) {
		return -1;
	}
	// a Jedi Academy entity's own colour or alpha replaces its stages'
	// (tr_jka_entity.h), which the trace's per-shader materials cannot carry,
	// and a distortion copy only refracts what is behind it
	// (tr_entity_refraction.h): raster draws both
	if ( ent->e.jka.rgbTint || ent->e.jka.forceAlpha || ent->e.jka.distortion ) {
		return -1;
	}
	// nor can they carry Elite Force's forced alpha, which blends an entity's
	// single-texture stages while its alpha is 1 to 254 (R_STVEFForcedAlpha,
	// tr_stvef_alpha.h), as Holomatch's holodeck room fades away after a
	// player joins: raster draws such an entity while the alpha is in force
	if ( ent->e.stvef.forceAlpha && ent->e.shader.rgba[3] > 0 && ent->e.shader.rgba[3] < 255 ) {
		return -1;
	}
	model = R_GetModelByHandle( ent->e.hModel );
	if ( !model ) {
		return -1;
	}
	switch ( model->type ) {
	case MOD_MESH:
	case MOD_MDR:
	case MOD_IQM:
	case MOD_BRUSH:
	case MOD_MDS:
	case MOD_MDC:
	case MOD_WOLFET_MESH:
		break;
	default:
		return -1;
	}
	// the IHV test marks its portal markers, hidden body and view weapon with
	// its own entity flags; raster draws them as Q3IHV_EntityVisible says
	if ( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active ) {
		if ( ent->e.q3IHV.flags & Q3IHV_ENTITY_PORTAL ) {
			return -1;
		}
		if ( ent->e.q3IHV.flags & Q3IHV_ENTITY_FIRST_PERSON ) {
			renderfx |= RF_FIRST_PERSON;
		}
		if ( ent->e.q3IHV.flags & Q3IHV_ENTITY_THIRD_PERSON ) {
			renderfx |= RF_THIRD_PERSON;
		}
	}
	// raster hides a third-person model from the primary view whatever else
	// it is flagged; the trace keeps it for its shadow
	if ( renderfx & RF_THIRD_PERSON ) {
		return 2;	// BODY
	}
	if ( renderfx & ( RF_FIRST_PERSON | RF_DEPTHHACK ) ) {
		if ( backEnd.viewParms.portalView != PV_NONE || ( backEnd.refdef.rdflags & RDF_NOFIRSTPERSON ) ) {
			return -1;
		}
		return 1;	// WEAPON
	}
	return 0;		// DYNAMIC
}

/*
A model vertex's light as the raster pass draws it (RB_CalcDiffuseColor): the
entity's light-grid ambient, plus its directed light by the normal's cosine,
each channel truncated and clamped at 255. It is the model's lighting term,
which lightmap mode takes as its baked light (rt_geometry.glsl). The top byte
marks it: RT_ENTITY_LIGHT_GRID when the entity's light leaves the dynamic
lights to the per-pixel light pass, RT_ENTITY_LIGHT_ALL when it holds them.
Both markers keep the word a normal float, which any copy preserves; the
word is written into the vertex's unused fourth position lane.
*/
static void RT_StampEntityLight( rtxRtPackedVertex_t *v, const trRefEntity_t *ent, const vec3_t lightDir, uint32_t marker )
{
	const float incoming = DotProduct( v->normal, lightDir );
	uint32_t word = marker << 24;
	int c;

	for ( c = 0; c < 3; c++ ) {
		// Raster truncates the evaluated expression directly to a light byte.
		// Avoid an extra binary32 rounding here on x87 builds.
		const double value = ent->ambientLight[c] + ( incoming > 0.0f ? incoming * ent->directedLight[c] : 0.0f );
		const uint32_t byte = !( value > 0.0f ) ? 0u : value >= 255.0f ? 255u : (uint32_t)value;
		word |= byte << ( 8 * c );
	}
	Com_Memcpy( &v->xyz[3], &word, sizeof( word ) );
}

static void RT_StampEntityVertices( rtxRtCpuGeometry_t *geometry, uint32_t firstVertex,
	const trRefEntity_t *ent, qboolean brush )
{
	uint32_t i;
	const uint32_t color = ent->e.shader.u32;
	// the raster pass lit this model (R_SetupEntityLighting), unless it drew
	// it by Quake III IHV's own rules
	const qboolean lit = ( !brush && ent->lightingCalculated &&
		!( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active ) ) ? qtrue : qfalse;
	// only the per-pixel light pass leaves the dynamic lights out of it
	const uint32_t marker = R_GetDlightMode() == 2 ? RT_ENTITY_LIGHT_GRID : RT_ENTITY_LIGHT_ALL;
	vec3_t lightDir;

	if ( lit ) {
		vk_rt_transform_direction( ent, ent->lightDir, lightDir );
	}
	for ( i = firstVertex; i < geometry->numVertices; i++ ) {
		rtxRtPackedVertex_t *v = &geometry->vertices[i];
		// Renderer-internal metadata; no external refEntity layout changes.
		v->normal[3] = ( ent->e.renderfx & RF_NOSHADOW ) ? -1.0f : 0.0f;
		if ( brush ) {
			// brush models keep BSP lightmap coordinates and vertex colours
			v->lightmapData[2] = ent->e.shaderTime.f;
			v->lightmapData[3] = 1.0f;
		} else {
			v->color = color;
			v->lightmapData[0] = ent->e.shaderTexCoord[0];
			v->lightmapData[1] = ent->e.shaderTexCoord[1];
			v->lightmapData[2] = ent->e.shaderTime.f;
			v->lightmapData[3] = 0.0f;
			if ( lit ) {
				RT_StampEntityLight( v, ent, lightDir, marker );
			}
		}
	}
}

static void RT_MotionFree( rtMotionFrame_t *motion )
{
	if ( motion->entities ) RT_HostFree( motion->entities );
	if ( motion->positions ) RT_HostFree( motion->positions );
	Com_Memset( motion, 0, sizeof( *motion ) );
}

// Record an entity this frame extracted, for the next frame to match.
static void RT_MotionRecord( const trRefEntity_t *ent, int cls, uint32_t first, uint32_t firstIndex )
{
	rtMotionFrame_t *motion = &s_rt.motion[0];
	const rtxRtCpuGeometry_t *geometry = &s_rt.dynamic[cls];
	const uint32_t count = geometry->numVertices - first;
	rtMotionEntity_t *record;
	uint64_t topology = 14695981039346656037ull;
	uint32_t v, k;
	int identity = -1;

	if ( count == 0 || !RT_Vec3Valid( ent->e.origin, RT_LIGHT_MAX_COORD ) ||
		!RT_BvhGrow( (void **)&motion->entities, &motion->entityCapacity, motion->numEntities,
			motion->numEntities + 1, sizeof( rtMotionEntity_t ) ) ) {
		return;
	}
	record = &motion->entities[ motion->numEntities++ ];
	record->model = ent->e.hModel;
	record->cls = cls;
	record->first = first;
	record->count = count;
	// These are engine-owned translations of frozen module records. Never
	// infer that a field belonging to a different profile is an identity.
	if ( ri.gameId == COM_GAME_RTCW_SP || ri.gameId == COM_GAME_RTCW_MP ) identity = ent->e.rtcw.entityNum;
	else if ( ri.gameId == COM_GAME_WOLFET ) identity = ent->e.wolfet.entityNum;
	record->identity = identity >= 0 && identity < 1022 ? identity : -1;
	record->materialGeneration = s_rtMaterials.generation;
	for ( v = firstIndex; v < geometry->numIndices; v++ ) {
		topology = ( topology ^ ( geometry->indices[v] - first ) ) * 1099511628211ull;
	}
	for ( v = first; v < geometry->numVertices; v++ ) {
		const rtxRtPackedVertex_t *vertex = &geometry->vertices[v];
		topology = ( topology ^ vertex->materialIndex ) * 1099511628211ull;
		// UV and material order identify parts even when connectivity and
		// vertex counts alone coincide. Position/normal vary with the pose.
		for ( k = 0; k < 2; k++ ) {
			uint32_t word;
			Com_Memcpy( &word, &vertex->texCoord[k], sizeof( word ) );
			topology = ( topology ^ word ) * 1099511628211ull;
		}
	}
	record->topology = topology;
	VectorCopy( ent->e.origin, record->origin );
	record->taken = qfalse;
}

// FNV-1a over 32-bit words: changing any one word changes the result
static uint32_t RT_HashWords( uint32_t hash, const void *data, uint32_t count )
{
	const byte *bytes = (const byte *)data;
	uint32_t i;

	for ( i = 0; i < count; i++ ) {
		uint32_t word;
		Com_Memcpy( &word, bytes + i * sizeof( word ), sizeof( word ) );
		hash = ( hash ^ word ) * 16777619u;
	}
	return hash;
}

// The same over the words of each vertex a still frame can show changed:
// position, normal, tangent/handedness, texture coordinates, colour, material,
// and the entity's shader coordinates and time. A pose can change lighting
// while vertex positions stay fixed. Four
// lanes keep the multiplies from waiting on one another.
static uint32_t RT_HashVertices( uint32_t hash, const rtxRtPackedVertex_t *vertices, uint32_t count )
{
	uint32_t lanes[4];
	uint32_t i;

	lanes[0] = hash;
	lanes[1] = hash ^ 0x9e3779b9u;
	lanes[2] = hash ^ 0x7f4a7c15u;
	lanes[3] = hash ^ 0x85ebca6bu;
	for ( i = 0; i < count; i++ ) {
		uint32_t w[20];
		Com_Memcpy( w, &vertices[i], sizeof( w ) );
		lanes[0] = ( ( ( lanes[0] ^ w[0] ) * 16777619u ) ^ w[12] ) * 16777619u;
		lanes[1] = ( ( ( lanes[1] ^ w[1] ) * 16777619u ) ^ w[13] ) * 16777619u;
		lanes[2] = ( ( ( lanes[2] ^ w[2] ) * 16777619u ) ^ w[14] ) * 16777619u;
		lanes[3] = ( ( ( lanes[3] ^ w[15] ) * 16777619u ) ^ w[16] ) * 16777619u;
		lanes[0] = ( ( ( lanes[0] ^ w[4] ) * 16777619u ) ^ w[8] ) * 16777619u;
		lanes[1] = ( ( ( lanes[1] ^ w[5] ) * 16777619u ) ^ w[9] ) * 16777619u;
		lanes[2] = ( ( ( lanes[2] ^ w[6] ) * 16777619u ) ^ w[10] ) * 16777619u;
		lanes[3] = ( ( ( lanes[3] ^ w[7] ) * 16777619u ) ^ w[11] ) * 16777619u;
		lanes[0] = ( lanes[0] ^ w[17] ) * 16777619u;
		lanes[1] = ( lanes[1] ^ w[18] ) * 16777619u;
		lanes[2] = ( lanes[2] ^ w[19] ) * 16777619u;
	}
	return RT_HashWords( hash, lanes, 4 );
}

// A deterministic BODY caster for engine screenshot probes. It traverses
// the same partition, acceleration structure and shadow paths as the own
// player's body. Scripted offsets change every rendered frame without
// synthesizing game input or depending on a particular game module.
static qboolean RT_AppendTestCaster( void )
{
	static const uint32_t indices[36] = {
		0,2,3, 0,3,1, 4,5,7, 4,7,6, 0,1,5, 0,5,4,
		2,6,7, 2,7,3, 0,4,6, 0,6,2, 1,3,7, 1,7,5
	};
	rtxRtCpuGeometry_t *g = &s_rt.dynamic[2];
	vec3_t origin;
	float halfWidth;
	int noShadow = 0;
	uint32_t i, first, material;
	if ( !rtx_rt_test_caster || !rtx_rt_test_caster->string[0] ) return qtrue;
	if ( sscanf( rtx_rt_test_caster->string, "%f %f %f %f %d", &origin[0], &origin[1], &origin[2], &halfWidth, &noShadow ) < 4 ||
		!RT_Vec3Valid( origin, RT_LIGHT_MAX_COORD ) || !RT_FloatValid( halfWidth, 128.0f ) || halfWidth <= 0.0f ) {
		return qtrue;
	}
	if ( !vk_rt_cpu_geometry_reserve( g, 8, 36 ) ) return qfalse;
	first = g->numVertices;
	material = RT_MaterialIndexForShader( tr.defaultShader );
	if ( rtx_rt_test_caster->modified ) {
		ri.Printf( PRINT_DEVELOPER, "RTX RT test BODY: %.1f %.1f %.1f, width %.1f, noShadow %d, material %u owner %d\n",
			origin[0], origin[1], origin[2], halfWidth, noShadow, material, (int)RT_MaterialOwner( material ) );
		rtx_rt_test_caster->modified = qfalse;
	}
	for ( i = 0; i < 8; i++ ) {
		rtxRtPackedVertex_t *v = &g->vertices[g->numVertices++];
		Com_Memset( v, 0, sizeof( *v ) );
		v->xyz[0] = origin[0] + ( ( i & 1 ) ? halfWidth : -halfWidth );
		v->xyz[1] = origin[1] + ( ( i & 2 ) ? halfWidth : -halfWidth );
		v->xyz[2] = origin[2] + ( ( i & 4 ) ? 56.0f : 0.0f );
		v->xyz[3] = 1.0f;
		v->normal[2] = 1.0f;
		v->normal[3] = noShadow ? -1.0f : 0.0f;
		v->color = 0xffffffffu;
		v->materialIndex = material;
	}
	for ( i = 0; i < 36; i++ ) g->indices[g->numIndices++] = first + indices[i];
	return qtrue;
}

static qboolean RT_ExtractDynamic( void )
{
	int c;
	int i;

	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		RT_ResetCpuGeometry( &s_rt.dynamic[c] );
	}
	s_rt.motion[0].numEntities = 0;
	for ( i = 0; backEnd.refdef.entities && i < backEnd.refdef.num_entities; i++ ) {
		const trRefEntity_t *ent = &backEnd.refdef.entities[i];
		const int cls = RT_EntityClass( ent );
		rtxRtCpuGeometry_t *geometry;
		const model_t *model;
		uint32_t firstVertex, firstIndex;

		if ( cls < 0 ) {
			continue;
		}
		geometry = &s_rt.dynamic[cls];
		firstVertex = geometry->numVertices;
		firstIndex = geometry->numIndices;
		if ( !vk_rt_append_dynamic_model_entity_geometry( geometry, ent, NULL ) ) {
			return qfalse;
		}
		model = R_GetModelByHandle( ent->e.hModel );
		RT_StampEntityVertices( geometry, firstVertex, ent, ( model && model->type == MOD_BRUSH ) ? qtrue : qfalse );
		RT_RemapMaterials( geometry, firstVertex );
		RT_MotionRecord( ent, cls, firstVertex, firstIndex );
	}
	if ( !RT_AppendTestCaster() ) return qfalse;
	s_rt.sceneHash = 2166136261u;
	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		const rtxRtCpuGeometry_t *geometry = &s_rt.dynamic[c];
		uint32_t counts[RT_WORLD_PARTS];
		if ( !RT_PartitionTriangles( &s_rt.dynamic[c], qfalse, counts ) ) {
			return qfalse;
		}
		s_rt.dynamicTriangles[c][0] = counts[0];
		s_rt.dynamicTriangles[c][1] = counts[1];
		// only frames whose clock stood still are averaged (RT_WriteFrameUniforms),
		// so only they pay for the models' hash
		if ( s_rt.accumulateTime == backEnd.refdef.floatTime ) {
			s_rt.sceneHash = RT_HashWords( s_rt.sceneHash, &geometry->numVertices, 1 );
			s_rt.sceneHash = RT_HashWords( s_rt.sceneHash, &geometry->numIndices, 1 );
			s_rt.sceneHash = RT_HashVertices( s_rt.sceneHash, geometry->vertices, geometry->numVertices );
		}
	}
	return qtrue;
}

/*
====================================================================

Acceleration structures

====================================================================
*/

static VkDeviceSize RT_AlignScratch( VkDeviceSize size )
{
	VkDeviceSize alignment = MAX( 256u, vk.rt.as_props.minAccelerationStructureScratchOffsetAlignment );
	return ( size + alignment - 1 ) & ~( alignment - 1 );
}

static void RT_FillBlasGeometry( VkAccelerationStructureGeometryKHR *geometry, VkDeviceAddress vertexAddress,
	uint32_t vertexCount, VkDeviceAddress indexAddress, qboolean opaque )
{
	Com_Memset( geometry, 0, sizeof( *geometry ) );
	geometry->sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry->flags = opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
	geometry->geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	geometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	geometry->geometry.triangles.vertexData.deviceAddress = vertexAddress;
	geometry->geometry.triangles.vertexStride = sizeof( rtxRtPackedVertex_t );
	geometry->geometry.triangles.maxVertex = vertexCount ? vertexCount - 1 : 0;
	geometry->geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	geometry->geometry.triangles.indexData.deviceAddress = indexAddress;
}

/*
Size (and when needed recreate) a BLAS for the given ranges, then record its
build into cmd with the given scratch address. Growing an AS is only safe
while nothing in flight reads it: the world's is built at load, a frame's
while its command buffer is idle.
*/
static qboolean RT_PrepareBlas( rtBlas_t *blas, VkDeviceAddress vertexAddress, uint32_t vertexCount,
	VkDeviceAddress indexAddress, uint32_t geometryCount, const uint32_t primCount[2], const uint32_t firstPrim[2],
	VkBuildAccelerationStructureFlagsKHR flags, qboolean anyHitOnly, const char *name,
	VkAccelerationStructureGeometryKHR geometries[2], VkAccelerationStructureBuildRangeInfoKHR ranges[2],
	VkAccelerationStructureBuildGeometryInfoKHR *buildInfo )
{
	VkAccelerationStructureBuildSizesInfoKHR sizes;
	uint32_t g;
	qboolean update = blas->built && ( flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR ) &&
		blas->geometryCount == geometryCount && blas->vertexCount == vertexCount &&
		blas->buildFlags == flags && blas->anyHitOnly == anyHitOnly;

	for ( g = 0; g < geometryCount; g++ ) {
		if ( blas->primCount[g] != primCount[g] || blas->firstPrim[g] != firstPrim[g] ) update = qfalse;
		// the first geometry is opaque, unless every hit goes to any-hit
		RT_FillBlasGeometry( &geometries[g], vertexAddress, vertexCount, indexAddress,
			( g == 0 && !anyHitOnly ) ? qtrue : qfalse );
		Com_Memset( &ranges[g], 0, sizeof( ranges[g] ) );
		ranges[g].primitiveCount = primCount[g];
		ranges[g].primitiveOffset = firstPrim[g] * 3u * sizeof( uint32_t );
		blas->primCount[g] = primCount[g];
		blas->firstPrim[g] = firstPrim[g];
	}
	blas->geometryCount = geometryCount;
	blas->vertexCount = vertexCount;
	blas->buildFlags = flags;
	blas->anyHitOnly = anyHitOnly;

	Com_Memset( buildInfo, 0, sizeof( *buildInfo ) );
	buildInfo->sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo->type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo->flags = flags;
	buildInfo->mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo->geometryCount = geometryCount;
	buildInfo->pGeometries = geometries;

	Com_Memset( &sizes, 0, sizeof( sizes ) );
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		buildInfo, primCount, &sizes );

	if ( blas->as.handle == VK_NULL_HANDLE || blas->as.size < sizes.accelerationStructureSize ) {
		update = qfalse;
		VkDeviceSize size = sizes.accelerationStructureSize;
		if ( blas->as.handle != VK_NULL_HANDLE ) {
			size = MAX( size, blas->as.size + blas->as.size / 2 );
		}
		vk_rt_destroy_as( &blas->as );
		if ( !vk_rt_create_as( VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, size, name, &blas->as ) ) {
			return qfalse;
		}
		{
			VkAccelerationStructureDeviceAddressInfoKHR addressInfo;
			addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
			addressInfo.pNext = NULL;
			addressInfo.accelerationStructure = blas->as.handle;
			blas->as.deviceAddress = qvkGetAccelerationStructureDeviceAddressKHR( vk.device, &addressInfo );
		}
	}
	buildInfo->dstAccelerationStructure = blas->as.handle;
	if ( update ) {
		buildInfo->mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
		buildInfo->srcAccelerationStructure = blas->as.handle;
	}
	blas->updateAge = update ? blas->updateAge + 1 : 0;
	if ( flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR ) {
		if ( update ) s_rt.blasUpdates++; else s_rt.blasBuilds++;
	}
	blas->buildScratchSize = RT_AlignScratch( update ? sizes.updateScratchSize : sizes.buildScratchSize );
	return qtrue;
}

/*
The ray-tracing pipeline's world: up to three BLASes over the world buffers,
built once per map.
*/
static qboolean RT_BuildWorldBlases( const uint32_t counts[RT_WORLD_PARTS] )
{
	VkAccelerationStructureGeometryKHR geometries[RT_WORLD_INSTANCES][2];
	VkAccelerationStructureBuildRangeInfoKHR ranges[RT_WORLD_INSTANCES][2];
	VkAccelerationStructureBuildGeometryInfoKHR buildInfos[RT_WORLD_INSTANCES];
	const VkAccelerationStructureBuildRangeInfoKHR *rangePointers[RT_WORLD_INSTANCES];
	rtBlas_t *built[RT_WORLD_INSTANCES];
	rtxVkRtBuffer_t scratch;
	VkDeviceAddress vertexAddress, indexAddress;
	VkDeviceSize scratchOffsets[RT_WORLD_INSTANCES];
	VkDeviceSize scratchSize = 0;
	uint32_t buildCount = 0;
	uint32_t b;
	VkCommandBuffer cmd;
	const VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

	vertexAddress = s_rt.worldVertices.address;
	indexAddress = s_rt.worldIndices.address;
	{
		const uint32_t prims[2] = { counts[RT_PART_OPAQUE], counts[RT_PART_MASKED] };
		const uint32_t first[2] = { 0, counts[RT_PART_OPAQUE] };
		if ( !RT_PrepareBlas( &s_rt.worldBlas, vertexAddress, s_rt.worldVertexCount, indexAddress, 2, prims, first,
				flags, qfalse, "rt world blas", geometries[buildCount], ranges[buildCount], &buildInfos[buildCount] ) ) {
			return qfalse;
		}
		built[buildCount] = &s_rt.worldBlas;
		rangePointers[buildCount] = ranges[buildCount];
		buildCount++;
	}
	if ( counts[RT_PART_NOSHADOW_OPAQUE] + counts[RT_PART_NOSHADOW_MASKED] > 0 ) {
		const uint32_t prims[2] = { counts[RT_PART_NOSHADOW_OPAQUE], counts[RT_PART_NOSHADOW_MASKED] };
		const uint32_t first[2] = { counts[RT_PART_OPAQUE] + counts[RT_PART_MASKED],
			counts[RT_PART_OPAQUE] + counts[RT_PART_MASKED] + counts[RT_PART_NOSHADOW_OPAQUE] };
		if ( !RT_PrepareBlas( &s_rt.noShadowBlas, vertexAddress, s_rt.worldVertexCount, indexAddress, 2, prims, first,
				flags, qfalse, "rt world no-shadow blas", geometries[buildCount], ranges[buildCount],
				&buildInfos[buildCount] ) ) {
			return qfalse;
		}
		built[buildCount] = &s_rt.noShadowBlas;
		rangePointers[buildCount] = ranges[buildCount];
		buildCount++;
	}
	if ( counts[RT_PART_SPECIAL] > 0 ) {
		const uint32_t prims[2] = { counts[RT_PART_SPECIAL], 0 };
		const uint32_t first[2] = { counts[RT_PART_OPAQUE] + counts[RT_PART_MASKED] +
			counts[RT_PART_NOSHADOW_OPAQUE] + counts[RT_PART_NOSHADOW_MASKED], 0 };
		if ( !RT_PrepareBlas( &s_rt.specialBlas, vertexAddress, s_rt.worldVertexCount, indexAddress, 1, prims, first,
				flags, qfalse, "rt world special blas", geometries[buildCount], ranges[buildCount],
				&buildInfos[buildCount] ) ) {
			return qfalse;
		}
		built[buildCount] = &s_rt.specialBlas;
		rangePointers[buildCount] = ranges[buildCount];
		buildCount++;
	}
	if ( counts[RT_PART_FILTER] > 0 ) {
		const uint32_t prims[2] = { counts[RT_PART_FILTER], 0 };
		const uint32_t first[2] = { counts[RT_PART_OPAQUE] + counts[RT_PART_MASKED] + counts[RT_PART_NOSHADOW_OPAQUE] +
			counts[RT_PART_NOSHADOW_MASKED] + counts[RT_PART_SPECIAL], 0 };
		if ( !RT_PrepareBlas( &s_rt.filterBlas, vertexAddress, s_rt.worldVertexCount, indexAddress, 1, prims, first,
				flags, qtrue, "rt world filter blas", geometries[buildCount], ranges[buildCount],
				&buildInfos[buildCount] ) ) {
			return qfalse;
		}
		built[buildCount] = &s_rt.filterBlas;
		rangePointers[buildCount] = ranges[buildCount];
		buildCount++;
	}

	// one scratch buffer, each build's region aligned
	{
		const VkDeviceSize alignment = MAX( 256u, vk.rt.as_props.minAccelerationStructureScratchOffsetAlignment );
		for ( b = 0; b < buildCount; b++ ) {
			scratchOffsets[b] = scratchSize;
			scratchSize += ( built[b]->buildScratchSize + alignment - 1 ) & ~( alignment - 1 );
		}
		if ( !vk_rt_create_buffer( scratchSize + alignment,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, qtrue, "rt world scratch", &scratch ) ) {
			return qfalse;
		}
		for ( b = 0; b < buildCount; b++ ) {
			const VkDeviceAddress base = ( scratch.address + alignment - 1 ) & ~( (VkDeviceAddress)alignment - 1 );
			buildInfos[b].scratchData.deviceAddress = base + scratchOffsets[b];
		}
	}

	cmd = begin_command_buffer();
	qvkCmdBuildAccelerationStructuresKHR( cmd, buildCount, buildInfos, rangePointers );
	if ( !end_command_buffer_soft( cmd, __func__ ) ) {
		vk_rt_destroy_buffer( &scratch );
		return qfalse;
	}
	vk_rt_destroy_buffer( &scratch );

	for ( b = 0; b < buildCount; b++ ) {
		built[b]->built = qtrue;
	}
	return qtrue;
}

/*
The compute backend's world: a BVH per world instance over the partitioned
geometry, uploaded once per map.
*/
static qboolean RT_BuildWorldTrees( const rtxRtCpuGeometry_t *geometry, const uint32_t counts[RT_WORLD_PARTS] )
{
	const int64_t start = vk_rt_now_microseconds();
	uint32_t first[RT_WORLD_PARTS];
	uint32_t part, sum = 0;
	int t;

	for ( part = 0; part < RT_WORLD_PARTS; part++ ) {
		first[part] = sum;
		sum += counts[part];
	}
	RT_BvhReset( &s_rt.bvh );
	{
		// world: its opaque then masked part; no-shadow likewise; special; filter
		const uint32_t firstTri[RT_WORLD_INSTANCES] = { first[RT_PART_OPAQUE], first[RT_PART_NOSHADOW_OPAQUE],
			first[RT_PART_SPECIAL], first[RT_PART_FILTER] };
		const uint32_t count[RT_WORLD_INSTANCES] = {
			counts[RT_PART_OPAQUE] + counts[RT_PART_MASKED],
			counts[RT_PART_NOSHADOW_OPAQUE] + counts[RT_PART_NOSHADOW_MASKED],
			counts[RT_PART_SPECIAL],
			counts[RT_PART_FILTER]
		};
		for ( t = 0; t < RT_WORLD_INSTANCES; t++ ) {
			rtSwTree_t *tree = &s_rt.worldTrees[t];
			tree->valid = qfalse;
			if ( count[t] == 0 ) {
				continue;
			}
			if ( !RT_BvhAppend( &s_rt.bvh, geometry->vertices, geometry->numVertices,
					geometry->indices + (size_t)firstTri[t] * 3u, count[t], firstTri[t],
					&tree->nodeOffset, &tree->triOffset ) ) {
				return qfalse;
			}
			tree->valid = qtrue;
		}
	}
	if ( s_rt.bvh.numNodes == 0 ||
		!RT_CreateStatic( &s_rt.worldBvhNodes, s_rt.bvh.nodes, (size_t)s_rt.bvh.numNodes * sizeof( rtBvhNode_t ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, "rt world bvh nodes" ) ||
		!RT_CreateStatic( &s_rt.worldBvhTris, s_rt.bvh.tris, (size_t)s_rt.bvh.numTris * sizeof( rtBvhTri_t ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, "rt world bvh triangles" ) ) {
		return qfalse;
	}
	ri.Printf( PRINT_ALL, "RTX RT: compute backend world BVH: %u nodes over %u triangles in %.1f ms\n",
		s_rt.bvh.numNodes, s_rt.bvh.numTris, vk_rt_elapsed_ms( start, vk_rt_now_microseconds() ) );
	// the frames' model trees reuse these arrays at their own, smaller size:
	// a 32-bit client's address space is too short to keep the world's
	RT_BvhFree( &s_rt.bvh );
	return qtrue;
}

static void RT_DestroyWorld( void )
{
	vk_rt_destroy_as( &s_rt.worldBlas.as );
	vk_rt_destroy_as( &s_rt.noShadowBlas.as );
	vk_rt_destroy_as( &s_rt.specialBlas.as );
	vk_rt_destroy_as( &s_rt.filterBlas.as );
	vk_rt_destroy_buffer( &s_rt.worldVertices );
	vk_rt_destroy_buffer( &s_rt.worldIndices );
	vk_rt_destroy_buffer( &s_rt.worldBvhNodes );
	vk_rt_destroy_buffer( &s_rt.worldBvhTris );
	vk_rt_destroy_buffer( &s_rt.primitiveLights );
	Com_Memset( &s_rt.worldBlas, 0, sizeof( s_rt.worldBlas ) );
	Com_Memset( &s_rt.noShadowBlas, 0, sizeof( s_rt.noShadowBlas ) );
	Com_Memset( &s_rt.specialBlas, 0, sizeof( s_rt.specialBlas ) );
	Com_Memset( &s_rt.filterBlas, 0, sizeof( s_rt.filterBlas ) );
	Com_Memset( s_rt.worldTrees, 0, sizeof( s_rt.worldTrees ) );
}

static qboolean RT_BuildWorld( void )
{
	rtxRtCpuGeometry_t geometry;
	uint32_t counts[RT_WORLD_PARTS];
	const qboolean compute = RT_Compute();
	// the ray-tracing pipeline builds its BLASes from these buffers
	const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | ( compute ? 0 :
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR );
	qboolean ok;

	if ( !RT_ExtractWorld( &geometry ) ) {
		return qfalse;
	}
	if ( !RT_PartitionTriangles( &geometry, qtrue, counts ) || geometry.numIndices < 3 ) {
		vk_rt_cpu_geometry_free( &geometry );
		return qfalse;
	}

	RT_DestroyWorld();
	if ( !RT_CreateStatic( &s_rt.worldVertices, geometry.vertices,
			(size_t)geometry.numVertices * sizeof( rtxRtPackedVertex_t ), usage, !compute, "rt world vertices" ) ||
		!RT_CreateStatic( &s_rt.worldIndices, geometry.indices,
			(size_t)geometry.numIndices * sizeof( uint32_t ), usage, !compute, "rt world indices" ) ) {
		vk_rt_cpu_geometry_free( &geometry );
		return qfalse;
	}
	s_rt.worldVertexCount = geometry.numVertices;
	s_rt.worldIndexCount = geometry.numIndices;
	Com_Memcpy( s_rt.worldTriangles, counts, sizeof( s_rt.worldTriangles ) );
	ok = compute ? RT_BuildWorldTrees( &geometry, counts ) : RT_BuildWorldBlases( counts );
	if ( !ok ) {
		vk_rt_cpu_geometry_free( &geometry );
		return qfalse;
	}
	s_rt.worldReady = qtrue;
	s_rt.world = tr.world;

	vk_rt_destroy_buffer( &s_rt.lightGrid );
	vk_rt_destroy_buffer( &s_rt.lightIndices );
	if ( RT_BuildLights() ) {
		const uint32_t numCells = (uint32_t)( s_rtLights.dims[0] * s_rtLights.dims[1] * s_rtLights.dims[2] );
		static const uint32_t noIndex = 0;
		if ( !RT_CreateStatic( &s_rt.lightGrid, s_rtLights.cells, (size_t)( numCells + s_rtLights.numLights ) * 2 * sizeof( uint32_t ),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, "rt light grid" ) ||
			!RT_CreateStatic( &s_rt.lightIndices, s_rtLights.numIndices ? (const void *)s_rtLights.indices : &noIndex,
				(size_t)MAX( s_rtLights.numIndices, 1u ) * sizeof( uint32_t ),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, "rt light indices" ) ) {
			ri.Printf( PRINT_WARNING, "RTX RT: light grid upload failed; tracing without static lights\n" );
			vk_rt_destroy_buffer( &s_rt.lightGrid );
			vk_rt_destroy_buffer( &s_rt.lightIndices );
			RT_LightsFree();
		}
	}
	{
		// the light each world triangle emits as, for glossy reflections
		// that meet an emitter (rt/rt_lights.c RT_MapPrimitiveLights)
		static const uint32_t none = ~0u;
		uint32_t *map = RT_MapPrimitiveLights( &geometry, geometry.numIndices / 3u - counts[RT_PART_FILTER] );
		ok = RT_CreateStatic( &s_rt.primitiveLights, map ? (const void *)map : &none,
			map ? (size_t)MAX( geometry.numIndices / 3u, 1u ) * sizeof( uint32_t ) : sizeof( none ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, "rt primitive lights" );
		if ( map ) {
			RT_HostFree( map );
		}
	}
	vk_rt_cpu_geometry_free( &geometry );
	if ( !ok ) {
		RT_DestroyWorld();
		s_rt.worldReady = qfalse;
		return qfalse;
	}
	ri.Printf( PRINT_ALL, "RTX RT: world traced triangles opaque=%u masked=%u special=%u, casting no q3map shadow %u,"
		" filtering q3map's light %u, vertices=%u, materials=%u\n",
		counts[RT_PART_OPAQUE], counts[RT_PART_MASKED], counts[RT_PART_SPECIAL],
		counts[RT_PART_NOSHADOW_OPAQUE] + counts[RT_PART_NOSHADOW_MASKED], counts[RT_PART_FILTER],
		s_rt.worldVertexCount, s_rtMaterials.numMaterials );
	return qtrue;
}

static void RT_WriteInstance( VkAccelerationStructureInstanceKHR *instance, VkDeviceAddress blas,
	uint32_t cls, uint32_t mask )
{
	Com_Memset( instance, 0, sizeof( *instance ) );
	instance->transform.matrix[0][0] = 1.0f;
	instance->transform.matrix[1][1] = 1.0f;
	instance->transform.matrix[2][2] = 1.0f;
	instance->instanceCustomIndex = cls << 20;
	instance->mask = mask;
	instance->instanceShaderBindingTableRecordOffset = 0;
	// Quake's front faces wind clockwise; any-hit culls per material
	instance->flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
		VK_GEOMETRY_INSTANCE_TRIANGLE_FLIP_FACING_BIT_KHR;
	instance->accelerationStructureReference = blas;
}

static void RT_BarrierAccelerationBuilds( VkCommandBuffer cmd, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess )
{
	VkMemoryBarrier barrier;
	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	barrier.dstAccessMask = dstAccess;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, dstStage,
		0, 1, &barrier, 0, NULL, 0, NULL );
}

/*
Record the ray-tracing pipeline's scene into cmd: this frame's dynamic BLASes
and the TLAS over every instance. Returns the number of TLAS instances.
*/
static qboolean RT_RecordAccelerationStructures( rtFrame_t *frame, VkCommandBuffer cmd, uint32_t totalVertices,
	const uint32_t primBase[RT_DYNAMIC_CLASSES], uint32_t *outInstances )
{
	VkAccelerationStructureGeometryKHR geometries[RT_DYNAMIC_CLASSES][2];
	VkAccelerationStructureBuildRangeInfoKHR ranges[RT_DYNAMIC_CLASSES][2];
	VkAccelerationStructureBuildGeometryInfoKHR buildInfos[RT_DYNAMIC_CLASSES];
	const VkAccelerationStructureBuildRangeInfoKHR *rangePointers[RT_DYNAMIC_CLASSES];
	VkAccelerationStructureInstanceKHR *instances;
	uint32_t buildCount = 0;
	uint32_t instanceCount = 0;
	uint32_t vertexBase = 0;
	VkDeviceSize scratchSize = 0;
	VkDeviceSize scratchOffsets[RT_DYNAMIC_CLASSES + 1];
	int c;

	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		const uint32_t prims[2] = { s_rt.dynamicTriangles[c][0], s_rt.dynamicTriangles[c][1] };
		const uint32_t first[2] = { primBase[c], primBase[c] + prims[0] };
		const rtxRtCpuGeometry_t *g = &s_rt.dynamic[c];
		rtBlas_t *blas = &frame->blas[c];
		const uint32_t base = vertexBase;
		vertexBase += g->numVertices;

		if ( prims[0] + prims[1] == 0 ) {
			frame->blas[c].built = qfalse;
			continue;
		}
		// Vulkan updates require identical connectivity, not just identical
		// triangle counts. Match exact local indices and refresh periodically
		// so large deformations cannot degrade traversal indefinitely.
		if ( blas->vertexBase != base || blas->topologyCount != g->numIndices || blas->updateAge >= 60 ||
			!blas->topology || memcmp( blas->topology, g->indices, (size_t)g->numIndices * sizeof( uint32_t ) ) ) {
			blas->built = qfalse;
		}
		if ( !RT_BvhGrow( (void **)&blas->topology, &blas->topologyCapacity, 0, g->numIndices, sizeof( uint32_t ) ) ) return qfalse;
		Com_Memcpy( blas->topology, g->indices, (size_t)g->numIndices * sizeof( uint32_t ) );
		blas->topologyCount = g->numIndices;
		blas->vertexBase = base;
		if ( !RT_PrepareBlas( &frame->blas[c], frame->vertices.buffer.address, totalVertices,
				frame->indices.buffer.address, 2, prims, first,
				VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR, qfalse,
				va( "rt dynamic blas %d", c ), geometries[buildCount], ranges[buildCount], &buildInfos[buildCount] ) ) {
			return qfalse;
		}
		scratchOffsets[buildCount] = scratchSize;
		scratchSize += frame->blas[c].buildScratchSize;
		rangePointers[buildCount] = ranges[buildCount];
		frame->blas[c].built = qtrue;
		buildCount++;
	}

	// TLAS instances
	if ( !RT_EnsureMapped( &frame->instances, RT_MAX_INSTANCES * sizeof( VkAccelerationStructureInstanceKHR ),
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			qtrue, qfalse, "rt tlas instances" ) ) {
		return qfalse;
	}
	instances = (VkAccelerationStructureInstanceKHR *)frame->instances.mapped;
	if ( s_rt.worldBlas.built ) {
		RT_WriteInstance( &instances[instanceCount++], s_rt.worldBlas.as.deviceAddress, RT_CLASS_WORLD, RT_MASK_WORLD );
	}
	if ( s_rt.noShadowBlas.built ) {
		RT_WriteInstance( &instances[instanceCount++], s_rt.noShadowBlas.as.deviceAddress, RT_CLASS_WORLD_NOSHADOW,
			RT_MASK_NOSHADOW );
	}
	if ( s_rt.specialBlas.built ) {
		RT_WriteInstance( &instances[instanceCount++], s_rt.specialBlas.as.deviceAddress, RT_CLASS_WORLD_SPECIAL, RT_MASK_SPECIAL );
	}
	if ( s_rt.filterBlas.built ) {
		// filter rays select hit group 1 themselves (rt_trace_hw.glsl)
		RT_WriteInstance( &instances[instanceCount++], s_rt.filterBlas.as.deviceAddress, RT_CLASS_WORLD_FILTER,
			RT_MASK_FILTER );
	}
	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		if ( frame->blas[c].built ) {
			RT_WriteInstance( &instances[instanceCount++], frame->blas[c].as.deviceAddress, s_rtDynamicClass[c],
				s_rtDynamicMask[c] );
		}
	}

	// TLAS sizing
	{
		VkAccelerationStructureGeometryKHR tlasGeometry;
		VkAccelerationStructureBuildGeometryInfoKHR tlasBuild;
		VkAccelerationStructureBuildSizesInfoKHR sizes;
		VkAccelerationStructureBuildRangeInfoKHR tlasRange;
		const VkAccelerationStructureBuildRangeInfoKHR *tlasRangePointer = &tlasRange;
		const uint32_t capacity = 8;
		VkDeviceSize tlasScratchOffset;
		VkDeviceAddress scratchBase;
		const VkDeviceSize alignment = MAX( 256u, vk.rt.as_props.minAccelerationStructureScratchOffsetAlignment );

		Com_Memset( &tlasGeometry, 0, sizeof( tlasGeometry ) );
		tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		tlasGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		tlasGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		tlasGeometry.geometry.instances.data.deviceAddress = frame->instances.buffer.address;

		Com_Memset( &tlasBuild, 0, sizeof( tlasBuild ) );
		tlasBuild.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		tlasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		tlasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		tlasBuild.geometryCount = 1;
		tlasBuild.pGeometries = &tlasGeometry;

		if ( frame->tlas.handle == VK_NULL_HANDLE ) {
			Com_Memset( &sizes, 0, sizeof( sizes ) );
			sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
			qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&tlasBuild, &capacity, &sizes );
			if ( !vk_rt_create_as( VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, sizes.accelerationStructureSize,
					"rt tlas", &frame->tlas ) ) {
				return qfalse;
			}
			frame->tlasScratchSize = RT_AlignScratch( sizes.buildScratchSize );
			frame->tlasInstanceCapacity = capacity;
		}
		tlasBuild.dstAccelerationStructure = frame->tlas.handle;

		tlasScratchOffset = scratchSize;
		scratchSize += frame->tlasScratchSize;
		if ( frame->scratch.buffer == VK_NULL_HANDLE || frame->scratch.capacity < scratchSize + alignment ) {
			vk_rt_destroy_buffer( &frame->scratch );
			if ( !vk_rt_create_buffer( scratchSize + alignment + ( scratchSize >> 1 ),
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, qtrue, "rt frame scratch", &frame->scratch ) ) {
				return qfalse;
			}
		}
		scratchBase = ( frame->scratch.address + alignment - 1 ) & ~( (VkDeviceAddress)alignment - 1 );

		if ( buildCount > 0 ) {
			uint32_t b;
			for ( b = 0; b < buildCount; b++ ) {
				buildInfos[b].scratchData.deviceAddress = scratchBase + scratchOffsets[b];
			}
			qvkCmdBuildAccelerationStructuresKHR( cmd, buildCount, buildInfos, rangePointers );
			RT_BarrierAccelerationBuilds( cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR );
		}

		tlasBuild.scratchData.deviceAddress = scratchBase + tlasScratchOffset;
		Com_Memset( &tlasRange, 0, sizeof( tlasRange ) );
		tlasRange.primitiveCount = instanceCount;
		qvkCmdBuildAccelerationStructuresKHR( cmd, 1, &tlasBuild, &tlasRangePointer );
		RT_BarrierAccelerationBuilds( cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR );
	}

	*outInstances = instanceCount;
	return qtrue;
}

// One instance of the compute backend's table (rt_trace_sw.glsl)
static void RT_AddComputeInstance( rtFrame_t *frame, const rtSwTree_t *tree, uint32_t cls, uint32_t mask,
	uint32_t firstOpaque, uint32_t firstMasked, qboolean dynamic )
{
	uint32_t *a, *b;

	if ( !tree->valid || frame->swInstanceCount >= RT_SW_MAX_INSTANCES ) {
		return;
	}
	a = frame->swInstances[ frame->swInstanceCount * 2 ];
	b = frame->swInstances[ frame->swInstanceCount * 2 + 1 ];
	a[0] = tree->nodeOffset;
	a[1] = tree->triOffset;
	a[2] = cls;
	a[3] = mask;
	b[0] = firstOpaque;
	b[1] = firstMasked;
	b[2] = dynamic ? 1u : 0u;
	b[3] = 0;
	frame->swInstanceCount++;
}

/*
The compute backend's scene: BVHs over this frame's dynamic classes in the
frame's buffers, and the instance table, world trees first, that
RT_WriteFrameUniforms passes to the trace. A tree names each triangle by its
index in the world or dynamic index buffer; the first opaque and first masked
indices recover the geometry and primitive a BLAS hit would report.
*/
static qboolean RT_RecordComputeScene( rtFrame_t *frame, const uint32_t worldFirst[RT_WORLD_PARTS],
	const uint32_t primBase[RT_DYNAMIC_CLASSES], uint32_t *outInstances )
{
	rtSwTree_t trees[RT_DYNAMIC_CLASSES];
	uint32_t numNodes = 0, numTris = 0;
	int c;

	Com_Memset( trees, 0, sizeof( trees ) );
	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		const rtxRtCpuGeometry_t *g = &s_rt.dynamic[c];
		rtDynamicBvh_t *cache = &s_rt.dynamicBvh[c];
		const uint32_t count = g->numIndices / 3;
		qboolean refit;
		if ( count == 0 ) {
			cache->numIndices = 0;
			RT_BvhReset( &cache->bvh );
			continue;
		}
		refit = cache->bvh.numNodes && cache->numIndices == g->numIndices && cache->numVertices == g->numVertices &&
			cache->age < 120 && !memcmp( cache->indices, g->indices, (size_t)g->numIndices * sizeof( uint32_t ) ) &&
			RT_BvhRefit( &cache->bvh, g->vertices, g->numVertices, g->indices, count, cache->primBias, primBase[c] );
		if ( refit && cache->age >= 8 && RT_BvhCost( &cache->bvh ) > cache->buildCost * 1.5f ) refit = qfalse;
		if ( !refit ) {
			uint32_t nodeOffset, triOffset;
			// Never update a tree after connectivity changes, or indefinitely
			// retain a poor partition after entities move through each other.
			RT_BvhReset( &cache->bvh );
			if ( !RT_BvhAppend( &cache->bvh, g->vertices, g->numVertices, g->indices, count, primBase[c],
					&nodeOffset, &triOffset ) ||
				!RT_BvhGrow( (void **)&cache->indices, &cache->indexCapacity, 0, g->numIndices, sizeof( uint32_t ) ) ) {
				cache->numIndices = 0;
				return qfalse;
			}
			Com_Memcpy( cache->indices, g->indices, (size_t)g->numIndices * sizeof( uint32_t ) );
			cache->buildCost = RT_BvhCost( &cache->bvh );
			cache->age = 0;
			s_rt.bvhBuilds++;
		} else {
			cache->age++;
			s_rt.bvhRefits++;
		}
		cache->primBias = primBase[c];
		cache->numVertices = g->numVertices;
		cache->numIndices = g->numIndices;
		trees[c].nodeOffset = numNodes;
		trees[c].triOffset = numTris;
		trees[c].valid = qtrue;
		numNodes += cache->bvh.numNodes;
		numTris += cache->bvh.numTris;
	}
	if ( numNodes > 0 ) {
		if ( !RT_EnsureMapped( &frame->bvhNodes, (VkDeviceSize)numNodes * sizeof( rtBvhNode_t ),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, qtrue, "rt dynamic bvh nodes" ) ||
			!RT_EnsureMapped( &frame->bvhTris, (VkDeviceSize)numTris * sizeof( rtBvhTri_t ),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, qtrue, "rt dynamic bvh triangles" ) ) return qfalse;
		for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
			const rtBvh_t *bvh = &s_rt.dynamicBvh[c].bvh;
			if ( !trees[c].valid ) continue;
			Com_Memcpy( (rtBvhNode_t *)frame->bvhNodes.mapped + trees[c].nodeOffset, bvh->nodes,
				(size_t)bvh->numNodes * sizeof( rtBvhNode_t ) );
			Com_Memcpy( (rtBvhTri_t *)frame->bvhTris.mapped + trees[c].triOffset, bvh->tris,
				(size_t)bvh->numTris * sizeof( rtBvhTri_t ) );
		}
	}

	frame->swInstanceCount = 0;
	Com_Memset( frame->swInstances, 0, sizeof( frame->swInstances ) );
	RT_AddComputeInstance( frame, &s_rt.worldTrees[0], RT_CLASS_WORLD, RT_MASK_WORLD,
		worldFirst[RT_PART_OPAQUE], worldFirst[RT_PART_MASKED], qfalse );
	RT_AddComputeInstance( frame, &s_rt.worldTrees[1], RT_CLASS_WORLD_NOSHADOW, RT_MASK_NOSHADOW,
		worldFirst[RT_PART_NOSHADOW_OPAQUE], worldFirst[RT_PART_NOSHADOW_MASKED], qfalse );
	// sky and portal surfaces are opaque, as the special BLAS's one geometry
	RT_AddComputeInstance( frame, &s_rt.worldTrees[2], RT_CLASS_WORLD_SPECIAL, RT_MASK_SPECIAL,
		worldFirst[RT_PART_SPECIAL], 0xFFFFFFFFu, qfalse );
	RT_AddComputeInstance( frame, &s_rt.worldTrees[3], RT_CLASS_WORLD_FILTER, RT_MASK_FILTER,
		worldFirst[RT_PART_FILTER], 0xFFFFFFFFu, qfalse );
	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		RT_AddComputeInstance( frame, &trees[c], s_rtDynamicClass[c], s_rtDynamicMask[c],
			primBase[c], primBase[c] + s_rt.dynamicTriangles[c][0], qtrue );
	}
	*outInstances = frame->swInstanceCount;
	return qtrue;
}

/*
Each dynamic vertex's world position last frame, into the frame's buffer: an
entity matched to last frame's (same model, class and vertex count, nearest
within RT_MOTION_MATCH_DISTANCE) gives its vertices' old positions, w 1; an
unmatched one its current positions, w 0, so the denoiser starts it afresh.
This frame's positions then become the next frame's.
*/
static qboolean RT_WritePreviousPositions( rtFrame_t *frame, const uint32_t vertexBase[RT_DYNAMIC_CLASSES],
	uint32_t totalVertices )
{
	rtMotionFrame_t *current = &s_rt.motion[0];
	rtMotionFrame_t *last = &s_rt.motion[1];
	float *dst;
	uint32_t e, v, total = 0;
	int c;

	if ( !RT_EnsureMapped( &frame->previous, (VkDeviceSize)MAX( totalVertices, 1u ) * 4 * sizeof( float ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, qtrue, "rt dynamic previous positions" ) ) {
		return qfalse;
	}
	dst = (float *)frame->previous.mapped;
	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		const rtxRtCpuGeometry_t *g = &s_rt.dynamic[c];
		for ( v = 0; v < g->numVertices; v++ ) {
			float *p = dst + ( vertexBase[c] + v ) * 4u;
			p[0] = g->vertices[v].xyz[0];
			p[1] = g->vertices[v].xyz[1];
			p[2] = g->vertices[v].xyz[2];
			p[3] = 0.0f;
		}
	}
	if ( !s_rt.hasPrevCamera ) {
		last->numEntities = 0;
	}
	for ( e = 0; e < current->numEntities; e++ ) {
		const rtMotionEntity_t *now = &current->entities[e];
		const float elapsed = s_rt.temporalTime ? Com_Clamp( 0.001f, 0.05f,
			(float)( ri.Milliseconds() - s_rt.temporalTime ) * 0.001f ) : 1.0f / 60.0f;
		const int matched = RT_MotionMatch( current->entities, current->numEntities, e,
			last->entities, last->numEntities, 8.0f + 2000.0f * elapsed );
		rtMotionEntity_t *best = matched >= 0 ? &last->entities[matched] : NULL;
		if ( best && best->first + best->count <= last->numPositions ) {
			best->taken = qtrue;
			for ( v = 0; v < now->count; v++ ) {
				float *p = dst + ( vertexBase[now->cls] + now->first + v ) * 4u;
				const float *old = last->positions + ( best->first + v ) * 3u;
				p[0] = old[0];
				p[1] = old[1];
				p[2] = old[2];
				p[3] = 1.0f;
			}
		}
		total += now->count;
	}

	// this frame's positions, for the next
	if ( !RT_BvhGrow( (void **)&current->positions, &current->positionCapacity, 0, total * 3u, sizeof( float ) ) ) {
		current->numEntities = 0;
		current->numPositions = 0;
	} else {
		uint32_t offset = 0;
		for ( e = 0; e < current->numEntities; e++ ) {
			rtMotionEntity_t *now = &current->entities[e];
			const rtxRtCpuGeometry_t *g = &s_rt.dynamic[now->cls];
			for ( v = 0; v < now->count; v++ ) {
				VectorCopy( g->vertices[ now->first + v ].xyz, current->positions + ( offset + v ) * 3u );
			}
			now->first = offset;
			now->taken = qfalse;
			offset += now->count;
		}
		current->numPositions = offset;
	}
	{
		const rtMotionFrame_t swap = s_rt.motion[0];
		s_rt.motion[0] = s_rt.motion[1];
		s_rt.motion[1] = swap;
	}
	return qtrue;
}

/*
Upload this frame's dynamic geometry, then record the backend's scene: the
ray-tracing pipeline's acceleration structure builds, or the compute
backend's BVHs. Returns the number of instances.
*/
static qboolean RT_RecordSceneBuild( rtFrame_t *frame, VkCommandBuffer cmd, uint32_t *outInstances,
	uint32_t classPrims[RT_CLASS_COUNT][4] )
{
	const qboolean compute = RT_Compute();
	// the ray-tracing pipeline builds its BLASes from these buffers
	const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | ( compute ? 0 :
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR );
	uint32_t vertexBase[RT_DYNAMIC_CLASSES];
	uint32_t primBase[RT_DYNAMIC_CLASSES];
	uint32_t worldFirst[RT_WORLD_PARTS];
	uint32_t totalVertices = 0;
	uint32_t totalIndices = 0;
	int c;

	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		vertexBase[c] = totalVertices;
		primBase[c] = totalIndices / 3u;
		totalVertices += s_rt.dynamic[c].numVertices;
		totalIndices += s_rt.dynamic[c].numIndices;
	}

	// each class geometry's first triangle in the world or dynamic buffers
	{
		uint32_t part, sum = 0;
		for ( part = 0; part < RT_WORLD_PARTS; part++ ) {
			worldFirst[part] = sum;
			sum += s_rt.worldTriangles[part];
		}
		classPrims[RT_CLASS_WORLD][0] = worldFirst[RT_PART_OPAQUE];
		classPrims[RT_CLASS_WORLD][1] = worldFirst[RT_PART_MASKED];
		classPrims[RT_CLASS_WORLD_NOSHADOW][0] = worldFirst[RT_PART_NOSHADOW_OPAQUE];
		classPrims[RT_CLASS_WORLD_NOSHADOW][1] = worldFirst[RT_PART_NOSHADOW_MASKED];
		classPrims[RT_CLASS_WORLD_SPECIAL][0] = worldFirst[RT_PART_SPECIAL];
		classPrims[RT_CLASS_WORLD_SPECIAL][1] = worldFirst[RT_PART_SPECIAL];
		classPrims[RT_CLASS_WORLD_FILTER][0] = worldFirst[RT_PART_FILTER];
		classPrims[RT_CLASS_WORLD_FILTER][1] = worldFirst[RT_PART_FILTER];
		for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
			classPrims[ s_rtDynamicClass[c] ][0] = primBase[c];
			classPrims[ s_rtDynamicClass[c] ][1] = primBase[c] + s_rt.dynamicTriangles[c][0];
		}
	}

	if ( totalIndices > 0 ) {
		rtxRtPackedVertex_t *dstVertices;
		uint32_t *dstIndices;

		if ( !RT_EnsureMapped( &frame->vertices, (VkDeviceSize)totalVertices * sizeof( rtxRtPackedVertex_t ),
				usage, !compute, qtrue, "rt dynamic vertices" ) ||
			!RT_EnsureMapped( &frame->indices, (VkDeviceSize)totalIndices * sizeof( uint32_t ),
				usage, !compute, qtrue, "rt dynamic indices" ) ) {
			return qfalse;
		}
		dstVertices = (rtxRtPackedVertex_t *)frame->vertices.mapped;
		dstIndices = (uint32_t *)frame->indices.mapped;
		for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
			const rtxRtCpuGeometry_t *g = &s_rt.dynamic[c];
			uint32_t i;
			if ( g->numVertices ) {
				Com_Memcpy( dstVertices + vertexBase[c], g->vertices, (size_t)g->numVertices * sizeof( rtxRtPackedVertex_t ) );
			}
			for ( i = 0; i < g->numIndices; i++ ) {
				dstIndices[ primBase[c] * 3u + i ] = g->indices[i] + vertexBase[c];
			}
		}
	}
	if ( !RT_WritePreviousPositions( frame, vertexBase, totalVertices ) ) {
		return qfalse;
	}

	if ( compute ) {
		return RT_RecordComputeScene( frame, worldFirst, primBase, outInstances );
	}
	return RT_RecordAccelerationStructures( frame, cmd, totalVertices, primBase, outInstances );
}

/*
====================================================================

Pipelines and descriptors

====================================================================
*/

/*
The shader binding table: ray generation; the radiance, shadow and filter
misses; the radiance and shadow hit group and the filter rays' (their
sbtRecordOffset 1 selects it).
*/
#define RT_SBT_GROUPS	7
#define RT_SBT_MISSES	3
#define RT_SBT_HITS		3

static qboolean RT_CreateSbt( void )
{
	const uint32_t groupCount = RT_SBT_GROUPS;
	const uint32_t handleSize = vk.rt.rt_pipeline_props.shaderGroupHandleSize;
	const VkDeviceSize handleAlignment = vk.rt.rt_pipeline_props.shaderGroupHandleAlignment;
	const VkDeviceSize baseAlignment = vk.rt.rt_pipeline_props.shaderGroupBaseAlignment;
	VkDeviceSize stride, missOffset, hitOffset, total;
	byte handles[RT_SBT_GROUPS * 64];
	void *mapped = NULL;
	byte *dst;

	// the masks below need power-of-two alignments
	if ( handleSize == 0 || handleSize > 64 || !vk_rt_is_power_of_two( handleAlignment ) ||
		!vk_rt_is_power_of_two( baseAlignment ) || baseAlignment > 4096 ) {
		return qfalse;
	}
	stride = ( handleSize + handleAlignment - 1 ) & ~( handleAlignment - 1 );
	if ( stride > vk.rt.rt_pipeline_props.maxShaderGroupStride ) {
		return qfalse;
	}
	missOffset = ( stride + baseAlignment - 1 ) & ~( baseAlignment - 1 );
	hitOffset = ( missOffset + RT_SBT_MISSES * stride + baseAlignment - 1 ) & ~( baseAlignment - 1 );
	total = hitOffset + RT_SBT_HITS * stride;

	if ( qvkGetRayTracingShaderGroupHandlesKHR( vk.device, s_rt.rtPipeline, 0, groupCount,
			handleSize * groupCount, handles ) != VK_SUCCESS ) {
		return qfalse;
	}
	vk_rt_destroy_buffer( &s_rt.sbt );
	if ( !vk_rt_create_buffer( total + baseAlignment,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, qtrue, "rt sbt", &s_rt.sbt ) ) {
		return qfalse;
	}
	{
		// the table must start on a base-aligned address
		const VkDeviceAddress base = ( s_rt.sbt.address + baseAlignment - 1 ) & ~( (VkDeviceAddress)baseAlignment - 1 );
		const VkDeviceSize shift = base - s_rt.sbt.address;
		if ( qvkMapMemory( vk.device, s_rt.sbt.memory, 0, VK_WHOLE_SIZE, 0, &mapped ) != VK_SUCCESS ) {
			return qfalse;
		}
		dst = (byte *)mapped + shift;
		Com_Memset( mapped, 0, (size_t)( total + baseAlignment ) );
		uint32_t g;
		Com_Memcpy( dst, handles, handleSize );
		for ( g = 0; g < RT_SBT_MISSES; g++ ) {
			Com_Memcpy( dst + missOffset + g * stride, handles + handleSize * ( 1 + g ), handleSize );
		}
		for ( g = 0; g < RT_SBT_HITS; g++ ) {
			Com_Memcpy( dst + hitOffset + g * stride, handles + handleSize * ( 1 + RT_SBT_MISSES + g ), handleSize );
		}
		qvkUnmapMemory( vk.device, s_rt.sbt.memory );

		s_rt.sbtRaygen.deviceAddress = base;
		s_rt.sbtRaygen.stride = stride;
		s_rt.sbtRaygen.size = stride;
		s_rt.sbtMiss.deviceAddress = base + missOffset;
		s_rt.sbtMiss.stride = stride;
		s_rt.sbtMiss.size = stride * RT_SBT_MISSES;
		s_rt.sbtHit.deviceAddress = base + hitOffset;
		s_rt.sbtHit.stride = stride;
		s_rt.sbtHit.size = stride * RT_SBT_HITS;
		Com_Memset( &s_rt.sbtCallable, 0, sizeof( s_rt.sbtCallable ) );
	}
	return qtrue;
}

// Descriptor bindings of the denoiser and composite (rt_denoise_common.glsl)
typedef enum {
	RT_DN_NOISY_DIFFUSE,
	RT_DN_NOISY_SPECULAR,
	RT_DN_REFLECTANCE,
	RT_DN_NORMAL_DEPTH,
	RT_DN_MOTION,
	RT_DN_INFO,
	RT_DN_HIST_DIFFUSE,
	RT_DN_HIST_SPECULAR,
	RT_DN_HIST_MOMENTS,
	RT_DN_FILTER_DIFFUSE,
	RT_DN_FILTER_SPECULAR,
	RT_DN_EMISSION,
	RT_DN_OUTPUT,
	RT_DN_SCENE_COLOR,
	RT_DN_TONE,				// the tone mapper's buffer
	RT_DN_NOISY_INDIRECT,
	RT_DN_HIST_INDIRECT,
	RT_DN_FILTER_INDIRECT,
	RT_DN_COUNT
} rtDenoiseBinding_t;

// The image (or first of a pair) behind each denoiser binding
static const int s_rtDenoiseImage[RT_DN_COUNT] = {
	RT_IMG_DIFFUSE, RT_IMG_SPECULAR, RT_IMG_REFLECTANCE, RT_IMG_NORMAL_DEPTH, RT_IMG_MOTION, RT_IMG_INFO,
	RT_IMG_HIST_DIFFUSE, RT_IMG_HIST_SPECULAR, RT_IMG_HIST_MOMENTS, RT_IMG_FILTER_DIFFUSE,
	RT_IMG_FILTER_SPECULAR, RT_IMG_EMISSION, -1, -1, -1,
	RT_IMG_INDIRECT, RT_IMG_HIST_INDIRECT, RT_IMG_FILTER_INDIRECT
};
static const uint32_t s_rtDenoiseCount[RT_DN_COUNT] = {
	1, 1, 1, 2, 1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2
};

static qboolean RT_CreateComputePipeline( VkShaderModule module, VkPipelineLayout layout, VkPipeline *pipeline,
	const char *name )
{
	VkComputePipelineCreateInfo computeInfo;
	Com_Memset( &computeInfo, 0, sizeof( computeInfo ) );
	computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeInfo.stage.module = module;
	computeInfo.stage.pName = "main";
	computeInfo.layout = layout;
	const VkResult result = qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &computeInfo, NULL, pipeline );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to create the %s pipeline (VkResult %d)\n", name, result );
		return qfalse;
	}
	return qtrue;
}

static qboolean RT_EnsurePipelines( void )
{
	const qboolean compute = RT_Compute();
	// the compute backend's set holds its BVH buffers in place of the TLAS
	const uint32_t rtBindingCount = compute ? RT_BINDING_SW_COUNT : RT_BINDING_COUNT;
	VkDescriptorSetLayoutBinding rtBindings[RT_BINDING_SW_COUNT];
	VkDescriptorSetLayoutBinding denoiseBindings[RT_DN_COUNT];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkDescriptorPoolSize poolSizes[5];
	uint32_t poolSizeCount = 0;
	VkDescriptorPoolCreateInfo poolInfo;
	VkDescriptorSetLayout layouts[RT_FRAMES];
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorSet sets[RT_FRAMES];
	VkPushConstantRange pushRange;
	const VkShaderStageFlags rtStages = compute ? VK_SHADER_STAGE_COMPUTE_BIT :
		( VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
		  VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR );
	uint32_t i;

	if ( s_rt.rtPipeline != VK_NULL_HANDLE && s_rt.compositePipeline != VK_NULL_HANDLE ) {
		return qtrue;
	}
	if ( ( compute ? vk.modules.rt_trace == VK_NULL_HANDLE :
			( vk.modules.rt_rgen == VK_NULL_HANDLE || vk.modules.rt_rmiss == VK_NULL_HANDLE ||
			  vk.modules.rt_rmiss_shadow == VK_NULL_HANDLE || vk.modules.rt_rmiss_filter == VK_NULL_HANDLE ||
			  vk.modules.rt_rchit == VK_NULL_HANDLE || vk.modules.rt_rahit == VK_NULL_HANDLE ||
			  vk.modules.rt_rahit_filter == VK_NULL_HANDLE || vk.modules.rt_rahit_shadow == VK_NULL_HANDLE ) ) ||
		vk.modules.rt_composite == VK_NULL_HANDLE || vk.modules.rt_tonemap == VK_NULL_HANDLE ||
		vk.modules.rt_denoise_temporal == VK_NULL_HANDLE || vk.modules.rt_denoise_atrous == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_ERROR, "RTX RT: path tracer shader modules are missing\n" );
		return qfalse;
	}

	// trace set
	Com_Memset( rtBindings, 0, sizeof( rtBindings ) );
	for ( i = 0; i < rtBindingCount; i++ ) {
		rtBindings[i].binding = i;
		rtBindings[i].descriptorCount = 1;
		rtBindings[i].stageFlags = rtStages;
		rtBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}
	if ( !compute ) {
		rtBindings[RT_BINDING_TLAS].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	}
	rtBindings[RT_BINDING_FRAME].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	rtBindings[RT_BINDING_TEXTURES].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	rtBindings[RT_BINDING_TEXTURES].descriptorCount = RT_MAX_TEXTURES;
	for ( i = RT_BINDING_OUT_COLOR; i <= RT_BINDING_OUT_INDIRECT; i++ ) {
		rtBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		rtBindings[i].stageFlags = compute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	}
	Com_Memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = rtBindingCount;
	layoutInfo.pBindings = rtBindings;
	if ( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &s_rt.rtSetLayout ) != VK_SUCCESS ) {
		return qfalse;
	}

	// denoiser and composite set
	Com_Memset( denoiseBindings, 0, sizeof( denoiseBindings ) );
	for ( i = 0; i < RT_DN_COUNT; i++ ) {
		denoiseBindings[i].binding = i;
		denoiseBindings[i].descriptorCount = s_rtDenoiseCount[i];
		denoiseBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		denoiseBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	}
	denoiseBindings[RT_DN_SCENE_COLOR].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	denoiseBindings[RT_DN_TONE].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutInfo.bindingCount = RT_DN_COUNT;
	layoutInfo.pBindings = denoiseBindings;
	if ( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &s_rt.denoiseSetLayout ) != VK_SUCCESS ) {
		return qfalse;
	}

	// pipeline layouts
	Com_Memset( &pipelineLayoutInfo, 0, sizeof( pipelineLayoutInfo ) );
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &s_rt.rtSetLayout;
	if ( qvkCreatePipelineLayout( vk.device, &pipelineLayoutInfo, NULL, &s_rt.rtPipelineLayout ) != VK_SUCCESS ) {
		return qfalse;
	}
	pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof( rtDenoisePush_t );
	pipelineLayoutInfo.pSetLayouts = &s_rt.denoiseSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushRange;
	if ( qvkCreatePipelineLayout( vk.device, &pipelineLayoutInfo, NULL, &s_rt.denoisePipelineLayout ) != VK_SUCCESS ) {
		return qfalse;
	}

	// descriptor pool and per-frame sets
	if ( !compute ) {
		poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		poolSizes[poolSizeCount].descriptorCount = RT_FRAMES;
		poolSizeCount++;
	}
	poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[poolSizeCount].descriptorCount = RT_FRAMES * 20;
	poolSizeCount++;
	poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[poolSizeCount].descriptorCount = RT_FRAMES;
	poolSizeCount++;
	poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[poolSizeCount].descriptorCount = RT_FRAMES * ( RT_MAX_TEXTURES + 1 );
	poolSizeCount++;
	poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[poolSizeCount].descriptorCount = RT_FRAMES * ( RT_IMG_COUNT + 1 +
		RT_BINDING_OUT_INDIRECT - RT_BINDING_OUT_COLOR + 1 );
	poolSizeCount++;
	Com_Memset( &poolInfo, 0, sizeof( poolInfo ) );
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = RT_FRAMES * 2;
	poolInfo.poolSizeCount = poolSizeCount;
	poolInfo.pPoolSizes = poolSizes;
	if ( qvkCreateDescriptorPool( vk.device, &poolInfo, NULL, &s_rt.pool ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = s_rt.pool;
	allocInfo.descriptorSetCount = RT_FRAMES;
	allocInfo.pSetLayouts = layouts;
	for ( i = 0; i < RT_FRAMES; i++ ) {
		layouts[i] = s_rt.rtSetLayout;
	}
	if ( qvkAllocateDescriptorSets( vk.device, &allocInfo, sets ) != VK_SUCCESS ) {
		return qfalse;
	}
	for ( i = 0; i < RT_FRAMES; i++ ) {
		s_rt.frames[i].rtSet = sets[i];
		layouts[i] = s_rt.denoiseSetLayout;
	}
	if ( qvkAllocateDescriptorSets( vk.device, &allocInfo, sets ) != VK_SUCCESS ) {
		return qfalse;
	}
	for ( i = 0; i < RT_FRAMES; i++ ) {
		s_rt.frames[i].denoiseSet = sets[i];
		s_rt.frames[i].textureCount = -1;
	}

	if ( compute ) {
		// the compute backend's trace: one compute shader
		if ( !RT_CreateComputePipeline( vk.modules.rt_trace, s_rt.rtPipelineLayout, &s_rt.rtPipeline,
				"compute path tracing" ) ) {
			return qfalse;
		}
	} else {
		// ray-tracing pipeline: raygen, the radiance, shadow and filter
		// misses, the radiance hit group and the filter rays' (RT_CreateSbt)
		VkPipelineShaderStageCreateInfo stages[8];
		VkRayTracingShaderGroupCreateInfoKHR groups[RT_SBT_GROUPS];
		VkRayTracingPipelineCreateInfoKHR pipelineInfo;
		const VkShaderStageFlagBits stageBits[8] = {
			VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_SHADER_STAGE_MISS_BIT_KHR, VK_SHADER_STAGE_MISS_BIT_KHR,
			VK_SHADER_STAGE_MISS_BIT_KHR, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_SHADER_STAGE_ANY_HIT_BIT_KHR
		};
		const VkShaderModule modules[8] = {
			vk.modules.rt_rgen, vk.modules.rt_rmiss, vk.modules.rt_rmiss_shadow, vk.modules.rt_rmiss_filter,
			vk.modules.rt_rchit, vk.modules.rt_rahit, vk.modules.rt_rahit_filter, vk.modules.rt_rahit_shadow
		};

		Com_Memset( stages, 0, sizeof( stages ) );
		for ( i = 0; i < ARRAY_LEN( stages ); i++ ) {
			stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[i].stage = stageBits[i];
			stages[i].module = modules[i];
			stages[i].pName = "main";
		}
		Com_Memset( groups, 0, sizeof( groups ) );
		for ( i = 0; i < RT_SBT_GROUPS; i++ ) {
			groups[i].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			groups[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			groups[i].generalShader = i;
			groups[i].closestHitShader = VK_SHADER_UNUSED_KHR;
			groups[i].anyHitShader = VK_SHADER_UNUSED_KHR;
			groups[i].intersectionShader = VK_SHADER_UNUSED_KHR;
		}
		groups[4].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		groups[4].generalShader = VK_SHADER_UNUSED_KHR;
		groups[4].closestHitShader = 4;
		groups[4].anyHitShader = 5;
		groups[5].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		groups[5].generalShader = VK_SHADER_UNUSED_KHR;
		groups[5].anyHitShader = 6;
		groups[6].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		groups[6].generalShader = VK_SHADER_UNUSED_KHR;
		groups[6].closestHitShader = 4;
		groups[6].anyHitShader = 7;

		Com_Memset( &pipelineInfo, 0, sizeof( pipelineInfo ) );
		pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		pipelineInfo.stageCount = ARRAY_LEN( stages );
		pipelineInfo.pStages = stages;
		pipelineInfo.groupCount = RT_SBT_GROUPS;
		pipelineInfo.pGroups = groups;
		pipelineInfo.maxPipelineRayRecursionDepth = 1;
		pipelineInfo.layout = s_rt.rtPipelineLayout;
		if ( qvkCreateRayTracingPipelinesKHR( vk.device, VK_NULL_HANDLE, vk.pipelineCache, 1, &pipelineInfo,
				NULL, &s_rt.rtPipeline ) != VK_SUCCESS ) {
			ri.Printf( PRINT_ERROR, "RTX RT: failed to create the path tracing pipeline\n" );
			return qfalse;
		}
		if ( !RT_CreateSbt() ) {
			ri.Printf( PRINT_ERROR, "RTX RT: failed to create the shader binding table\n" );
			return qfalse;
		}
	}

	if ( !RT_CreateComputePipeline( vk.modules.rt_denoise_temporal, s_rt.denoisePipelineLayout,
			&s_rt.temporalPipeline, "temporal denoise" ) ||
		!RT_CreateComputePipeline( vk.modules.rt_denoise_atrous, s_rt.denoisePipelineLayout,
			&s_rt.atrousPipeline, "spatial denoise" ) ||
		!RT_CreateComputePipeline( vk.modules.rt_composite, s_rt.denoisePipelineLayout,
			&s_rt.compositePipeline, "composite" ) ||
		!RT_CreateComputePipeline( vk.modules.rt_tonemap, s_rt.denoisePipelineLayout,
			&s_rt.tonemapPipeline, "tone mapping" ) ) {
		return qfalse;
	}
	if ( s_rt.tone.buffer == VK_NULL_HANDLE ) {
		// cleared by a fill, copied out for the host (RT_ToneMap)
		if ( !vk_rt_create_buffer( sizeof( rtToneBuffer_t ),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, qfalse, "rt tone", &s_rt.tone ) ) {
			return qfalse;
		}
		s_rt.toneClear = qtrue;
	}

	if ( s_rt.dummy.buffer == VK_NULL_HANDLE ) {
		static const uint32_t zero[64] = { 0 };
		if ( !vk_rt_create_buffer( sizeof( zero ), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, qfalse,
				"rt empty", &s_rt.dummy ) ||
			!vk_rt_upload_buffer_data( &s_rt.dummy, zero, sizeof( zero ) ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

static qboolean RT_UploadMaterials( rtFrame_t *frame )
{
	if ( frame->materialGeneration == s_rtMaterials.generation && frame->materials.mapped &&
		frame->uploadedMaterials == s_rtMaterials.numMaterials ) {
		return qtrue;
	}
	if ( !RT_EnsureMapped( &frame->materials, (VkDeviceSize)MAX( s_rtMaterials.numMaterials, 1u ) * sizeof( rtGpuMaterial_t ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, qtrue, "rt materials" ) ||
		!RT_EnsureMapped( &frame->stages, (VkDeviceSize)MAX( s_rtMaterials.numStages, 1u ) * sizeof( rtGpuStage_t ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, qtrue, "rt stages" ) ) {
		return qfalse;
	}
	if ( s_rtMaterials.numMaterials ) {
		Com_Memcpy( frame->materials.mapped, s_rtMaterials.materials, s_rtMaterials.numMaterials * sizeof( rtGpuMaterial_t ) );
	}
	if ( s_rtMaterials.numStages ) {
		Com_Memcpy( frame->stages.mapped, s_rtMaterials.stages, s_rtMaterials.numStages * sizeof( rtGpuStage_t ) );
	}
	frame->materialGeneration = s_rtMaterials.generation;
	frame->uploadedMaterials = s_rtMaterials.numMaterials;
	frame->uploadedStages = s_rtMaterials.numStages;
	return qtrue;
}

/*
How an RTCW light material's stages treat what a surface drew
(tr_rtcw_light_backend.h draws each with the projected light): a GL_DST_COLOR
GL_ONE stage multiplies it by one plus the light (flamedlightshader has two),
a GL_ZERO GL_ONE_MINUS_SRC_COLOR stage by one minus it (negdlightshader).
False for a material with neither, whose light the tracer cannot follow.
*/
static qboolean RT_MaterialLightPasses( const shader_t *shader, uint32_t *brighten, uint32_t *darken )
{
	int i;

	*brighten = 0;
	*darken = 0;
	for ( i = 0; i < shader->numUnfoggedPasses && i < MAX_SHADER_STAGES; i++ ) {
		const shaderStage_t *stage = shader->stages[i];
		uint32_t blend;
		if ( !stage ) {
			break;
		}
		blend = stage->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS );
		if ( blend == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE ) ) {
			( *brighten )++;
		} else if ( blend == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) {
			( *darken )++;
		}
	}
	return ( *brighten || *darken ) ? qtrue : qfalse;
}

// Static lights (copied once per world into each frame's buffer), this
// frame's dynamic lights, then RTCW's light materials, which multiply what
// they reach instead of lighting it (rt_lighting.glsl
// rt_material_light_factor). Hashes what they show into s_rt.sceneHash.
static qboolean RT_UploadLights( rtFrame_t *frame )
{
	const uint32_t staticCount = s_rtLights.numLights;
	const float dlightScale = rtx_rt_dlight_scale ? MAX( 0.0f, rtx_rt_dlight_scale->value ) : 1.0f;
	const VkDeviceSize size = (VkDeviceSize)( staticCount + MAX_DLIGHTS + 1 ) * sizeof( rtGpuLight_t );
	rtGpuLight_t *dst;
	uint32_t count = 0;
	uint32_t materials = 0;
	int i;

	if ( !frame->lights.mapped || frame->lights.buffer.capacity < size ) {
		// RT_EnsureMapped replaces the buffer: it holds no static lights yet
		frame->lightGeneration = (uint32_t)-1;
	}
	if ( !RT_EnsureMapped( &frame->lights, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, qfalse, qtrue, "rt lights" ) ) {
		return qfalse;
	}
	dst = (rtGpuLight_t *)frame->lights.mapped;
	if ( frame->lightGeneration != s_rtLights.generation ) {
		if ( staticCount ) {
			Com_Memcpy( dst, s_rtLights.lights, staticCount * sizeof( rtGpuLight_t ) );
		}
		frame->lightGeneration = s_rtLights.generation;
	}
	for ( i = 0; i < (int)backEnd.refdef.num_dlights && count < MAX_DLIGHTS; i++ ) {
		const dlight_t *dl = &backEnd.refdef.dlights[i];
		rtGpuLight_t *light = &dst[ staticCount + count ];
		// raster's stand-ins for surface lights (r_surfaceLightProxies) are
		// lights the tracer already casts from the surfaces themselves
		if ( dl->sourceFlags & DLIGHT_SOURCE_SURFACE ) {
			continue;
		}
		// game code supplies these: reject a light that is not finite
		if ( dl->rtcwShader || !( dl->radius > 0.0f ) || !RT_FloatValid( dl->radius, RT_LIGHT_MAX_COORD ) ||
			!RT_Vec3Valid( dl->origin, RT_LIGHT_MAX_COORD ) || !RT_Vec3Valid( dl->color, RT_LIGHT_MAX_VALUE ) ||
			( dl->linear && !RT_Vec3Valid( dl->origin2, RT_LIGHT_MAX_COORD ) ) ) {
			continue;
		}
		Com_Memset( light, 0, sizeof( *light ) );
		if ( dl->linear ) {
			// a beam light: the midpoint stands in for the segment
			VectorAdd( dl->origin, dl->origin2, light->position );
			VectorScale( light->position, 0.5f, light->position );
		} else {
			VectorCopy( dl->origin, light->position );
		}
		light->position[3] = dl->radius;
		VectorScale( dl->color, dlightScale, light->color );
		light->color[3] = (float)( RT_LIGHT_DLIGHT | RT_LIGHT_DYNAMIC );
		// not its radius: several games draw one a random size each frame
		// (Quake III's powerups and flags), which the still-frame average
		// can take
		s_rt.sceneHash = RT_HashWords( s_rt.sceneHash, light->position, 3 );
		s_rt.sceneHash = RT_HashWords( s_rt.sceneHash, light->color, 4 );
		count++;
	}
	for ( i = 0; i < (int)backEnd.refdef.num_dlights && count + materials < MAX_DLIGHTS; i++ ) {
		const dlight_t *dl = &backEnd.refdef.dlights[i];
		rtGpuLight_t *light = &dst[ staticCount + count + materials ];
		uint32_t brighten, darken;
		if ( !dl->rtcwShader || !RT_MaterialLightPasses( dl->rtcwShader, &brighten, &darken ) ||
			!( dl->radius > 0.0f ) || !RT_FloatValid( dl->radius, RT_LIGHT_MAX_COORD ) ||
			!RT_Vec3Valid( dl->origin, RT_LIGHT_MAX_COORD ) || !RT_Vec3Valid( dl->color, RT_LIGHT_MAX_VALUE ) ) {
			continue;
		}
		Com_Memset( light, 0, sizeof( *light ) );
		VectorCopy( dl->origin, light->position );
		light->position[3] = dl->radius;
		VectorCopy( dl->color, light->color );
		light->color[3] = (float)RT_LIGHT_MATERIAL;
		light->extra[0] = (float)MIN( brighten, 8u );
		light->extra[1] = (float)MIN( darken, 8u );
		s_rt.sceneHash = RT_HashWords( s_rt.sceneHash, light, sizeof( *light ) / sizeof( uint32_t ) );
		materials++;
	}
	s_rt.numDlights = count;
	s_rt.numMaterialLights = materials;
	return qtrue;
}

static void RT_BufferInfo( VkDescriptorBufferInfo *info, const rtxVkRtBuffer_t *buffer )
{
	const rtxVkRtBuffer_t *source = ( buffer && buffer->buffer != VK_NULL_HANDLE ) ? buffer : &s_rt.dummy;
	info->buffer = source->buffer;
	info->offset = 0;
	info->range = VK_WHOLE_SIZE;
}

static void RT_UpdateDescriptors( rtFrame_t *frame )
{
	VkWriteDescriptorSetAccelerationStructureKHR asInfo;
	VkDescriptorBufferInfo buffers[RT_BINDING_SW_COUNT];
	VkDescriptorImageInfo storage[RT_BINDING_COUNT];
	VkDescriptorImageInfo denoiseImages[RT_DN_COUNT][2];
	VkDescriptorBufferInfo toneInfo;
	VkWriteDescriptorSet writes[(int)RT_BINDING_SW_COUNT + (int)RT_DN_COUNT];
	VkDescriptorImageInfo *textures = s_vkRtDescriptorImageInfos;
	// the trace writes this frame's normal/depth and info
	const int outputs[RT_BINDING_OUT_INDIRECT - RT_BINDING_OUT_COLOR + 1] = {
		RT_IMG_DIFFUSE, RT_IMG_SPECULAR, RT_IMG_REFLECTANCE, RT_IMG_NORMAL_DEPTH + (int)s_rt.parity,
		RT_IMG_EMISSION, RT_IMG_MOTION, RT_IMG_INFO + (int)s_rt.parity, RT_IMG_INDIRECT
	};
	Vk_Sampler_Def samplerDef;
	uint32_t writeCount = 0;
	uint32_t i;
	const qboolean texturesChanged = ( frame->textureCount != tr.numImages ) ? qtrue : qfalse;

	Com_Memset( writes, 0, sizeof( writes ) );
	if ( RT_Compute() ) {
		// the BVHs: the world's nodes where the TLAS would be, then the rest
		static const uint32_t bvhBindings[4] = {
			RT_BINDING_TLAS, RT_BINDING_SW_WORLD_TRIS, RT_BINDING_SW_DYNAMIC_NODES, RT_BINDING_SW_DYNAMIC_TRIS
		};
		RT_BufferInfo( &buffers[RT_BINDING_TLAS], &s_rt.worldBvhNodes );
		RT_BufferInfo( &buffers[RT_BINDING_SW_WORLD_TRIS], &s_rt.worldBvhTris );
		RT_BufferInfo( &buffers[RT_BINDING_SW_DYNAMIC_NODES], &frame->bvhNodes.buffer );
		RT_BufferInfo( &buffers[RT_BINDING_SW_DYNAMIC_TRIS], &frame->bvhTris.buffer );
		for ( i = 0; i < ARRAY_LEN( bvhBindings ); i++ ) {
			writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[writeCount].dstSet = frame->rtSet;
			writes[writeCount].dstBinding = bvhBindings[i];
			writes[writeCount].descriptorCount = 1;
			writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[writeCount].pBufferInfo = &buffers[ bvhBindings[i] ];
			writeCount++;
		}
	} else {
		Com_Memset( &asInfo, 0, sizeof( asInfo ) );
		asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		asInfo.accelerationStructureCount = 1;
		asInfo.pAccelerationStructures = &frame->tlas.handle;
		writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeCount].pNext = &asInfo;
		writes[writeCount].dstSet = frame->rtSet;
		writes[writeCount].dstBinding = RT_BINDING_TLAS;
		writes[writeCount].descriptorCount = 1;
		writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		writeCount++;
	}

	RT_BufferInfo( &buffers[RT_BINDING_WORLD_VERTICES], &s_rt.worldVertices );
	RT_BufferInfo( &buffers[RT_BINDING_WORLD_INDICES], &s_rt.worldIndices );
	RT_BufferInfo( &buffers[RT_BINDING_DYNAMIC_VERTICES], &frame->vertices.buffer );
	RT_BufferInfo( &buffers[RT_BINDING_DYNAMIC_INDICES], &frame->indices.buffer );
	RT_BufferInfo( &buffers[RT_BINDING_MATERIALS], &frame->materials.buffer );
	RT_BufferInfo( &buffers[RT_BINDING_STAGES], &frame->stages.buffer );
	RT_BufferInfo( &buffers[RT_BINDING_LIGHTS], &frame->lights.buffer );
	RT_BufferInfo( &buffers[RT_BINDING_LIGHT_GRID], &s_rt.lightGrid );
	RT_BufferInfo( &buffers[RT_BINDING_LIGHT_INDICES], &s_rt.lightIndices );
	RT_BufferInfo( &buffers[RT_BINDING_FRAME], &frame->frame.buffer );
	RT_BufferInfo( &buffers[RT_BINDING_DYNAMIC_PREVIOUS], &frame->previous.buffer );
	RT_BufferInfo( &buffers[RT_BINDING_PRIMITIVE_LIGHTS], &s_rt.primitiveLights );
	for ( i = RT_BINDING_WORLD_VERTICES; i <= RT_BINDING_FRAME; i++ ) {
		writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeCount].dstSet = frame->rtSet;
		writes[writeCount].dstBinding = i;
		writes[writeCount].descriptorCount = 1;
		writes[writeCount].descriptorType = ( i == RT_BINDING_FRAME ) ?
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[writeCount].pBufferInfo = &buffers[i];
		writeCount++;
	}
	writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[writeCount].dstSet = frame->rtSet;
	writes[writeCount].dstBinding = RT_BINDING_DYNAMIC_PREVIOUS;
	writes[writeCount].descriptorCount = 1;
	writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[writeCount].pBufferInfo = &buffers[RT_BINDING_DYNAMIC_PREVIOUS];
	writeCount++;
	writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[writeCount].dstSet = frame->rtSet;
	writes[writeCount].dstBinding = RT_BINDING_PRIMITIVE_LIGHTS;
	writes[writeCount].descriptorCount = 1;
	writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[writeCount].pBufferInfo = &buffers[RT_BINDING_PRIMITIVE_LIGHTS];
	writeCount++;

	if ( texturesChanged ) {
		for ( i = 0; i < RT_MAX_TEXTURES; i++ ) {
			const image_t *image = ( i < (uint32_t)tr.numImages ) ? tr.images[i] : NULL;
			if ( !image || image->view == VK_NULL_HANDLE ) {
				image = tr.whiteImage;
			}
			Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
			samplerDef.address_mode = image->wrapClampMode;
			if ( image->flags & IMGFLAG_MIPMAP ) {
				samplerDef.gl_mag_filter = gl_filter_max;
				samplerDef.gl_min_filter = gl_filter_min;
			} else {
				samplerDef.gl_mag_filter = GL_LINEAR;
				samplerDef.gl_min_filter = GL_LINEAR;
				samplerDef.noAnisotropy = qtrue;
			}
			textures[i].sampler = vk_find_sampler( &samplerDef );
			textures[i].imageView = image->view;
			textures[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeCount].dstSet = frame->rtSet;
		writes[writeCount].dstBinding = RT_BINDING_TEXTURES;
		writes[writeCount].descriptorCount = RT_MAX_TEXTURES;
		writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[writeCount].pImageInfo = textures;
		writeCount++;
		frame->textureCount = tr.numImages;
	}

	for ( i = 0; i < ARRAY_LEN( outputs ); i++ ) {
		storage[i].sampler = VK_NULL_HANDLE;
		storage[i].imageView = s_rt.images[ outputs[i] ].view;
		storage[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeCount].dstSet = frame->rtSet;
		writes[writeCount].dstBinding = RT_BINDING_OUT_COLOR + i;
		writes[writeCount].descriptorCount = 1;
		writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[writeCount].pImageInfo = &storage[i];
		writeCount++;
	}

	// denoiser and composite
	Com_Memset( denoiseImages, 0, sizeof( denoiseImages ) );
	for ( i = 0; i < RT_DN_COUNT; i++ ) {
		uint32_t e;
		if ( i == RT_DN_TONE ) {
			RT_BufferInfo( &toneInfo, &s_rt.tone );
			writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[writeCount].dstSet = frame->denoiseSet;
			writes[writeCount].dstBinding = i;
			writes[writeCount].descriptorCount = 1;
			writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[writeCount].pBufferInfo = &toneInfo;
			writeCount++;
			continue;
		}
		for ( e = 0; e < s_rtDenoiseCount[i]; e++ ) {
			VkDescriptorImageInfo *info = &denoiseImages[i][e];
			info->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			if ( i == RT_DN_OUTPUT ) {
				info->imageView = s_rt.output.view;
			} else if ( i == RT_DN_SCENE_COLOR ) {
				Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
				samplerDef.gl_mag_filter = GL_NEAREST;
				samplerDef.gl_min_filter = GL_NEAREST;
				samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				samplerDef.max_lod_1_0 = qtrue;
				samplerDef.noAnisotropy = qtrue;
				info->sampler = vk_find_sampler( &samplerDef );
				info->imageView = vk.color_image_view;
				info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			} else {
				info->imageView = s_rt.images[ s_rtDenoiseImage[i] + (int)e ].view;
			}
		}
		writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeCount].dstSet = frame->denoiseSet;
		writes[writeCount].dstBinding = i;
		writes[writeCount].descriptorCount = s_rtDenoiseCount[i];
		writes[writeCount].descriptorType = ( i == RT_DN_SCENE_COLOR ) ?
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[writeCount].pImageInfo = denoiseImages[i];
		writeCount++;
	}

	qvkUpdateDescriptorSets( vk.device, writeCount, writes, 0, NULL );
}

/*
====================================================================

Frame

====================================================================
*/

static float RT_Halton( uint32_t index, uint32_t base )
{
	float f = 1.0f;
	float r = 0.0f;
	while ( index > 0 ) {
		f /= (float)base;
		r += f * (float)( index % base );
		index /= base;
	}
	return r;
}

/*
The settings that change what a frame traces, by how often they have been
changed. A change starts the still-frame average and the denoiser's history
over: an average thousands of frames deep would otherwise take as many to
show it.
*/
static int RT_TraceSettingsCount( void )
{
	// r_dlightScale sizes the dynamic lights, which the scene hash leaves out
	const cvar_t *settings[] = {
		rtx_rt_lighting, rtx_rt_bounces, rtx_rt_indirect_strength, rtx_rt_emissive_scale, rtx_rt_sky_intensity,
		rtx_rt_sun_intensity, rtx_rt_specular, rtx_rt_mis, rtx_rt_dlight_scale, rtx_rt_iqm_skinning,
		rtx_rt_patch_shadows, r_dlightFalloff, r_dlightScale, rtx_rt_debug_light, rtx_rt_debug_light_mode,
		rtx_rt_emissive_light, rtx_rt_checkerboard, rtx_rt_camera, rtx_rt_indirect_divisor,
		r_liquidWarpScale, r_liquidRipples
	};
	int count = 0;
	int i;

	for ( i = 0; i < (int)ARRAY_LEN( settings ); i++ ) {
		count += settings[i] ? settings[i]->modificationCount : 0;
	}
	return count;
}

/*
The fog the raster pass gives the traced view's surfaces (tr_rtcw_fog_vk.h):
RTCW's and Enemy Territory's global fog, or Alice's far plane. A traced
surface is never sky, a 2D draw or a blended stage, and the snooper view is
left to raster; a nofog material is marked RT_MF_NOFOG. The trace finds each
pixel's visibility and the composite blends toward the colour, so the fog
never enters the accumulated light.
*/
static fnRTCWFogDraw_t RT_ViewFog( void )
{
	fnRTCWFogDraw_t draw;
	int channel;

	if ( !R_GAME_HAS_RITUAL_VIEW( ri.gameId ) ) {
		return FnRTCWFogDraw( &backEnd.refdef.rtcwFog, qfalse, qfalse, qfalse, qfalse, qfalse, qtrue );
	}
	Com_Memset( &draw, 0, sizeof( draw ) );
	if ( backEnd.viewParms.aliceFarFog && backEnd.viewParms.aliceFarDistance > 0.0f &&
		backEnd.viewParms.aliceFarDistance < 1.0e9f ) {
		for ( channel = 0; channel < 3; channel++ ) {
			draw.color[channel] = Com_Clamp( 0.0f, 1.0f, backEnd.viewParms.aliceFarColor[channel] * tr.identityLight );
		}
		draw.color[3] = 1.0f;	// linear, from the eye out to the far plane's distance
		draw.params[1] = backEnd.viewParms.aliceFarDistance;
		draw.params[2] = 1.0f;
		draw.params[3] = 1.0f;	// radial
	}
	return draw;
}

static int RT_IndirectDivisor( void )
{
	const int requested = rtx_rt_indirect_divisor ? rtx_rt_indirect_divisor->integer : 0;
	const int minimum = requested ? MIN( MAX( requested, 1 ), 4 ) : ( RT_Compute() ? 2 : 1 );
	const float budget = rtx_rt_frame_budget_ms ? rtx_rt_frame_budget_ms->value : 0.0f;
	if ( s_rt.budgetMinimum != minimum || !( budget > 0.0f ) ) {
		s_rt.budgetMinimum = s_rt.budgetDivisor = minimum;
		s_rt.budgetCooldown = 45;
	}
	if ( s_rt.budgetCooldown > 0 ) s_rt.budgetCooldown--;
	if ( budget > 0.0f && s_rt.budgetSample && s_rt.budgetCooldown == 0 ) {
		const float ms = s_rt.gpuMs[0] + s_rt.gpuMs[1] + s_rt.gpuMs[2] + s_rt.gpuMs[3];
		s_rt.budgetDivisor = RT_BudgetDivisor( s_rt.budgetDivisor, minimum, ms, budget );
		s_rt.budgetCooldown = 45;
	}
	s_rt.budgetSample = qfalse;
	// A complete seed after cuts/resizes avoids dark holes at a new surface.
	return s_rt.historyValid ? s_rt.budgetDivisor : 1;
}

static void RT_WriteFrameUniforms( rtFrame_t *frame, uint32_t classPrims[RT_CLASS_COUNT][4], qboolean weapon )
{
	rtFrameUniforms_t *u = (rtFrameUniforms_t *)frame->frame.mapped;
	const float tanX = tanf( DEG2RAD( backEnd.viewParms.fovX * 0.5f ) );
	const float tanY = tanf( DEG2RAD( backEnd.viewParms.fovY * 0.5f ) );
	// the lightmaps light the world only when asked (1); by default the
	// level's own lights do (RT_BuildLights)
	const qboolean bakedLighting = s_rtLights.mode == RT_LIGHTING_LIGHTMAPS ? qtrue : qfalse;
	const int settings = RT_TraceSettingsCount();
	qboolean still;
	int c;

	Com_Memset( u, 0, sizeof( *u ) );
	VectorCopy( backEnd.viewParms.or.origin, u->camOrigin );
	u->camOrigin[3] = tanX;
	VectorCopy( backEnd.viewParms.or.axis[0], u->camForward );
	u->camForward[3] = tanY;
	VectorScale( backEnd.viewParms.or.axis[1], -1.0f, u->camRight );
	u->camRight[3] = (float)s_rt.traceWidth;
	VectorCopy( backEnd.viewParms.or.axis[2], u->camUp );
	u->camUp[3] = (float)s_rt.traceHeight;

	if ( !s_rt.hasPrevCamera ) {
		Com_Memcpy( s_rt.prevOrigin, u->camOrigin, sizeof( s_rt.prevOrigin ) );
		Com_Memcpy( s_rt.prevForward, u->camForward, sizeof( s_rt.prevForward ) );
		Com_Memcpy( s_rt.prevRight, u->camRight, sizeof( s_rt.prevRight ) );
		Com_Memcpy( s_rt.prevUp, u->camUp, sizeof( s_rt.prevUp ) );
		s_rt.hasPrevCamera = qtrue;
		s_rt.accumulated = 0;
	}
	if ( settings != s_rt.settingsCount ) {
		s_rt.settingsCount = settings;
		s_rt.historyValid = qfalse;
		s_rt.accumulated = 0;
	}
	// a view that has not moved and a scene whose clock has not advanced
	still = ( !memcmp( s_rt.prevOrigin, u->camOrigin, sizeof( s_rt.prevOrigin ) ) &&
		!memcmp( s_rt.prevForward, u->camForward, sizeof( s_rt.prevForward ) ) &&
		!memcmp( s_rt.prevRight, u->camRight, sizeof( s_rt.prevRight ) ) &&
		!memcmp( s_rt.prevUp, u->camUp, sizeof( s_rt.prevUp ) ) &&
		s_rt.accumulateTime == backEnd.refdef.floatTime && s_rt.historyValid ) ? qtrue : qfalse;
	// ... whose models and dynamic lights are as they were. One changed while
	// the clock stood still (a light switched off, a model hidden) starts the
	// average over, as a setting does; a scene that keeps changing is not
	// averaged, as a moving one is not.
	if ( still && s_rt.sceneHash != s_rt.prevSceneHash ) {
		still = qfalse;
		if ( s_rt.accumulated > 0 ) {
			s_rt.historyValid = qfalse;
		}
	}
	s_rt.prevSceneHash = s_rt.sceneHash;
	Com_Memcpy( u->prevCamOrigin, s_rt.prevOrigin, sizeof( s_rt.prevOrigin ) );
	Com_Memcpy( u->prevCamForward, s_rt.prevForward, sizeof( s_rt.prevForward ) );
	Com_Memcpy( u->prevCamRight, s_rt.prevRight, sizeof( s_rt.prevRight ) );
	Com_Memcpy( u->prevCamUp, s_rt.prevUp, sizeof( s_rt.prevUp ) );
	Com_Memcpy( s_rt.prevOrigin, u->camOrigin, sizeof( s_rt.prevOrigin ) );
	Com_Memcpy( s_rt.prevForward, u->camForward, sizeof( s_rt.prevForward ) );
	Com_Memcpy( s_rt.prevRight, u->camRight, sizeof( s_rt.prevRight ) );
	Com_Memcpy( s_rt.prevUp, u->camUp, sizeof( s_rt.prevUp ) );
	s_rt.accumulateTime = backEnd.refdef.floatTime;

	if ( still && rtx_rt_accumulate && rtx_rt_accumulate->integer ) {
		if ( s_rt.accumulated < 65535u ) {
			s_rt.accumulated++;
		}
	} else {
		s_rt.accumulated = 0;
	}

	// sun: the map compiler's q3map_sun (RT_FindSun, validated there), in
	// lightmap units
	if ( s_rtLights.sunValid ) {
		const float sunScale = ( rtx_rt_sun_intensity ? MAX( 0.0f, rtx_rt_sun_intensity->value ) : 1.0f ) *
			s_rtLights.scale;
		VectorCopy( s_rtLights.sunDirection, u->sunDirection );
		u->sunDirection[3] = cosf( DEG2RAD( 0.5f ) );
		VectorScale( s_rtLights.sunLight, sunScale, u->sunColor );
		if ( s_rtLights.sunDeviance > 0.0f && s_rtLights.sunSamples > 1 ) {
			u->sunShape[0] = s_rtLights.sunDeviance;
			u->sunShape[1] = 1.0f / (float)s_rtLights.sunSamples;
		}
		u->flags[0] |= RT_FRAME_SUN;
	}

	u->time[0] = (float)backEnd.refdef.floatTime;
	u->time[1] = (float)tr.frameCount;
	u->liquidParams[0] = R_LiquidWaveTime( backEnd.refdef.floatTime );
	u->liquidParams[1] = r_liquidWarpScale ? Com_Clamp( 0.0f, 2.0f, r_liquidWarpScale->value ) : 1.0f;
	{
		int i, count = 0;
		for ( i = 0; i < backEnd.refdef.numLiquidInteractions && count < 8; i++ ) {
			const liquidInteraction_t *ring = &backEnd.refdef.liquidInteractions[i];
			float age, life;
			if ( !R_LiquidInteractionActive( ring, backEnd.refdef.time ) ) continue;
			age = (float)( backEnd.refdef.time - ring->time ) * 0.001f;
			life = 1.0f - age * ( 1000.0f / LIQUID_IMPULSE_LIFETIME_MSEC );
			VectorCopy( ring->origin, u->liquidImpulses[count] );
			u->liquidImpulses[count][3] = ring->radius + age * LIQUID_RIPPLE_EXPAND_SPEED;
			u->liquidAmplitudes[count / 4][count & 3] = ring->strength * life *
				( r_liquidRipples ? Com_Clamp( 0.0f, 2.0f, r_liquidRipples->value ) : 1.0f );
			count++;
		}
		u->liquidParams[2] = (float)count;
	}
	VectorCopy( s_rtLights.gridOrigin, u->lightGridOrigin );
	u->lightGridOrigin[3] = s_rtLights.cellSize > 0.0f ? 1.0f / s_rtLights.cellSize : 0.0f;
	if ( s_rt.lightGrid.buffer != VK_NULL_HANDLE ) {
		u->lightGridDims[0] = (uint32_t)s_rtLights.dims[0];
		u->lightGridDims[1] = (uint32_t)s_rtLights.dims[1];
		u->lightGridDims[2] = (uint32_t)s_rtLights.dims[2];
		u->lightGridDims[3] = s_rtLights.numLights;
	}
	u->params0[0] = rtx_rt_indirect_strength ? MAX( 0.0f, rtx_rt_indirect_strength->value ) : 1.0f;
	u->params0[1] = rtx_rt_emissive_scale ? MAX( 0.0f, rtx_rt_emissive_scale->value ) : 1.0f;
	u->params0[2] = 0.05f;		// roughness floor
	u->params0[3] = rtx_rt_sky_intensity ? MAX( 0.0f, rtx_rt_sky_intensity->value ) : 1.0f;
	u->params1[0] = tr.identityLight;
	s_rt.indirectDivisor = RT_IndirectDivisor();
	u->params1[1] = (float)s_rt.indirectDivisor;
	u->params1[2] = 64.0f;		// radiance clamp
	u->params1[3] = 2.0f * tanY / (float)MAX( 1u, s_rt.traceHeight );
	u->flags[0] |= ( bakedLighting ? RT_FRAME_BAKED_LIGHTING : 0u ) |
		( ( rtx_rt_light_reuse && rtx_rt_light_reuse->integer ) ? RT_FRAME_LIGHT_REUSE : 0u ) |
		( weapon ? RT_FRAME_WEAPON : 0u ) |
		( s_rt.worldTriangles[RT_PART_FILTER] ? RT_FRAME_FILTERS : 0u ) |
		( s_rtLights.q3map2 ? RT_FRAME_Q3MAP2 : 0u ) |
		( vk_scene_linear_enabled() ? RT_FRAME_SCENE_LINEAR : 0u ) |
		( ( RT_LightingCamera( s_rtLights.mode ) == RT_CAMERA_LINEAR && !vk_scene_linear_enabled() ) ?
			RT_FRAME_LINEAR_LIGHT : 0u ) |
		( ( !rtx_rt_specular || rtx_rt_specular->integer ) ? RT_FRAME_SPECULAR : 0u ) |
		( ( !rtx_rt_mis || rtx_rt_mis->integer ) ? RT_FRAME_MIS : 0u ) |
		( s_rt.accumulated > 0 ? RT_FRAME_ACCUMULATE : 0u );
	// the compute backend traces the bounce in a checkerboard by default: its
	// incoherent rays cost it twice what the primary view does
	{
		const int checkerboard = rtx_rt_checkerboard ? rtx_rt_checkerboard->integer : 0;
		// Seed a complete history after a cut/resize; a missing tile has no
		// observation to reconstruct yet.
		if ( s_rt.historyValid && ( checkerboard == 1 ||
			( checkerboard == 0 && RT_Compute() && s_rt.indirectDivisor == 1 ) ) ) {
			u->flags[0] |= RT_FRAME_CHECKERBOARD;
		}
	}
	u->flags[1] = rtx_rt_bounces ? (uint32_t)Com_Clamp( 0, 4, rtx_rt_bounces->integer ) : 1u;
	u->flags[2] = rtx_rt_debug_visualizer ? (uint32_t)MAX( 0, rtx_rt_debug_visualizer->integer ) : 0u;
	u->flags[3] = s_rt.numDlights;
	u->counts[0] = s_rtMaterials.numMaterials;
	u->counts[1] = s_rtMaterials.numStages;
	u->counts[2] = s_rtLights.numLights + s_rt.numDlights;
	u->counts[3] = s_rtLights.numLights;
	for ( c = 0; c < RT_CLASS_COUNT; c++ ) {
		u->classPrims[c][0] = classPrims[c][0];
		u->classPrims[c][1] = classPrims[c][1];
	}
	VectorCopy( s_rtLights.ambient, u->ambient );
	VectorCopy( s_rtLights.minLight, u->minLight[0] );
	VectorCopy( s_rtLights.minVertexLight, u->minLight[1] );
	VectorCopy( s_rtLights.minGridLight, u->minLight[2] );
	Com_Memcpy( u->skyImages, s_rt.skyImages, sizeof( u->skyImages ) );
	// q3map2's sky light, or the sky's own colour: the light samples reach
	// the sky, not the bounces
	if ( s_rtLights.mode == RT_LIGHTING_EMITTERS ) {
		// an even sky's colour, or the gain on the sky's images
		// (RT_FindSkyLightColor)
		if ( s_rtLights.skyColor[3] > 0.5f ) {
			VectorCopy( s_rtLights.skyColor, u->skyLight );
			u->skyLight[3] = 1.0f;
		} else {
			VectorSet( u->skyLight, 1.0f, 1.0f, 1.0f );
		}
		u->flags[0] |= RT_FRAME_SKY_RADIANCE;
		u->skyImages[1][3] |= RT_SKY_LIGHTS;
	} else if ( s_rtLights.skyLightIterations >= 2 ) {
		VectorScale( s_rtLights.skyLight, s_rtLights.scale, u->skyLight );
		u->skyLight[3] = (float)s_rtLights.skyLightIterations;
		u->skyImages[1][3] |= RT_SKY_LIGHTS;
	}
	// the dynamic lights' falloff, as the raster light pass eases it
	// (tr_shade.c VK_SetLightParams), and RTCW's light materials after them
	u->dlightShape[0] = r_dlightFalloff ? Com_Clamp( 0.0f, 1.0f, r_dlightFalloff->value ) : 1.0f;
	u->materialLights[0] = s_rtLights.numLights + s_rt.numDlights;
	u->materialLights[1] = s_rt.numMaterialLights;
	// rtx_rt_debug_light: one light alone (its number + 1; 0 for all), and what
	// of it to show
	u->materialLights[2] = ( rtx_rt_debug_light && rtx_rt_debug_light->integer >= 0 &&
		(uint32_t)rtx_rt_debug_light->integer < u->materialLights[0] ) ? (uint32_t)rtx_rt_debug_light->integer + 1u : 0u;
	u->materialLights[3] = rtx_rt_debug_light_mode ? (uint32_t)Com_Clamp( 0, 3, rtx_rt_debug_light_mode->integer ) : 0u;
	{
		const fnRTCWFogDraw_t fog = RT_ViewFog();
		Com_Memcpy( u->fogColor, fog.color, sizeof( u->fogColor ) );
		Com_Memcpy( u->fogParams, fog.params, sizeof( u->fogParams ) );
	}
	if ( s_rt.accumulated > 0 ) {
		// jitter the pixel footprint so accumulation also anti-aliases
		u->jitter[0] = RT_Halton( s_rt.accumulated & 1023u, 2 ) - 0.5f;
		u->jitter[1] = RT_Halton( s_rt.accumulated & 1023u, 3 ) - 0.5f;
	}
	u->jitter[2] = (float)s_rt.accumulated;
	// the compute backend's instances (RT_RecordComputeScene)
	u->swInfo[0] = frame->swInstanceCount;
	Com_Memcpy( u->swInstances, frame->swInstances, sizeof( u->swInstances ) );
}

static void RT_ImageBarrier( VkImageMemoryBarrier *barrier, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
	VkAccessFlags srcAccess, VkAccessFlags dstAccess )
{
	Com_Memset( barrier, 0, sizeof( *barrier ) );
	barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier->oldLayout = oldLayout;
	barrier->newLayout = newLayout;
	barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier->image = image;
	barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier->subresourceRange.levelCount = 1;
	barrier->subresourceRange.layerCount = 1;
	barrier->srcAccessMask = srcAccess;
	barrier->dstAccessMask = dstAccess;
}

static void RT_ComputeBarrier( VkCommandBuffer cmd, VkPipelineStageFlags srcStage )
{
	VkMemoryBarrier barrier;
	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	qvkCmdPipelineBarrier( cmd, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL );
}

static void RT_Dispatch( VkCommandBuffer cmd, VkPipeline pipeline, const rtDenoisePush_t *push, uint32_t width, uint32_t height )
{
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline );
	qvkCmdPushConstants( cmd, s_rt.denoisePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( *push ), push );
	qvkCmdDispatch( cmd, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
}

/*
The tone mapper's passes (rt_tonemap.comp), after the denoiser and before
the composite reads its curve: this frame's histogram, then the curve fitted
to it, which adapts toward it by the real time since the last (the game's
time stands still under timescale 0). A cleared buffer fits its first curve
at once.
*/
static void RT_ToneMap( VkCommandBuffer cmd, rtFrame_t *frame, const rtDenoisePush_t *push )
{
	const int now = ri.Milliseconds();
	rtDenoisePush_t tone = *push;

	// what this slot's last frame left, its fence waited for
	if ( frame->toneWritten && frame->tone.mapped ) {
		const rtToneBuffer_t *state = (const rtToneBuffer_t *)frame->tone.mapped;
		if ( state->frames ) {
			s_rt.toneMeteredLog = state->adaptedLog;
			Com_Memcpy( s_rt.toneCurve, state->curve, sizeof( s_rt.toneCurve ) );
			s_rt.toneRead = qtrue;
		}
	}
	frame->toneWritten = qfalse;

	if ( s_rt.toneClear ) {
		VkMemoryBarrier cleared;
		qvkCmdFillBuffer( cmd, s_rt.tone.buffer, 0, VK_WHOLE_SIZE, 0 );
		Com_Memset( &cleared, 0, sizeof( cleared ) );
		cleared.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		cleared.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		cleared.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 1, &cleared, 0, NULL, 0, NULL );
		s_rt.toneClear = qfalse;
		s_rt.toneTime = now;
	}

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_rt.tonemapPipeline );
	tone.view[2] = RT_TONE_PASS_HISTOGRAM;
	qvkCmdPushConstants( cmd, s_rt.denoisePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( tone ), &tone );
	qvkCmdDispatch( cmd, ( s_rt.traceWidth + 15 ) / 16, ( s_rt.traceHeight + 15 ) / 16, 1 );
	RT_ComputeBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	// seconds since the last curve, the adaptation's speed, and the
	// display's range in stops
	tone.view[2] = RT_TONE_PASS_CURVE;
	tone.params[0] = Com_Clamp( 0.0f, 1.0f, (float)( now - s_rt.toneTime ) * 0.001f );
	tone.params[1] = rtx_rt_exposure_speed ? Com_Clamp( 0.0f, 100.0f, rtx_rt_exposure_speed->value ) : 1.0f;
	tone.params[2] = rtx_rt_tonemap_range ? Com_Clamp( 1.0f, 16.0f, rtx_rt_tonemap_range->value ) : 7.0f;
	tone.params[3] = 0.0f;
	qvkCmdPushConstants( cmd, s_rt.denoisePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( tone ), &tone );
	qvkCmdDispatch( cmd, 1, 1, 1 );
	s_rt.toneTime = now;

	// a copy the host reads when this slot comes round again
	if ( RT_EnsureMapped( &frame->tone, sizeof( rtToneBuffer_t ), VK_BUFFER_USAGE_TRANSFER_DST_BIT, qfalse, qfalse,
			"rt tone readback" ) ) {
		VkMemoryBarrier barrier;
		VkBufferCopy region;
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 1, &barrier, 0, NULL, 0, NULL );
		region.srcOffset = 0;
		region.dstOffset = 0;
		region.size = sizeof( rtToneBuffer_t );
		qvkCmdCopyBuffer( cmd, s_rt.tone.buffer, frame->tone.buffer.buffer, 1, &region );
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
			0, 1, &barrier, 0, NULL, 0, NULL );
		frame->toneWritten = qtrue;
	}
	RT_ComputeBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

/*
GPU timing: five timestamps per frame slot around the scene build, trace,
denoiser and composite. A slot's results are read when it is reused, after
vk_begin_frame waited for its fence, so reading never stalls.
*/
static qboolean RT_TimingEnabled( void )
{
	return ( ( rtx_rt_perf_timing && rtx_rt_perf_timing->integer ) ||
		( rtx_rt_frame_budget_ms && rtx_rt_frame_budget_ms->value > 0.0f ) ) ? qtrue : qfalse;
}

static void RT_TimingBegin( VkCommandBuffer cmd, uint32_t slot )
{
	if ( !RT_TimingEnabled() ) {
		return;
	}
	if ( s_rt.timingPool == VK_NULL_HANDLE ) {
		VkQueryPoolCreateInfo info;
		Com_Memset( &info, 0, sizeof( info ) );
		info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		info.queryCount = RT_FRAMES * RT_TIMING_QUERIES;
		if ( qvkCreateQueryPool( vk.device, &info, NULL, &s_rt.timingPool ) != VK_SUCCESS ) {
			return;
		}
		Com_Memset( s_rt.timingWritten, 0, sizeof( s_rt.timingWritten ) );
	}
	if ( s_rt.timingWritten[slot] ) {
		uint64_t stamps[RT_TIMING_QUERIES];
		if ( qvkGetQueryPoolResults( vk.device, s_rt.timingPool, slot * RT_TIMING_QUERIES, RT_TIMING_QUERIES,
				sizeof( stamps ), stamps, sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT ) == VK_SUCCESS ) {
			const float toMs = vk.rt.gpu_timestamp_period_ns * 1e-6f;
			int i;
			for ( i = 0; i < RT_TIMING_STAGES; i++ ) {
				const float ms = (float)( stamps[i + 1] - stamps[i] ) * toMs;
				s_rt.gpuMs[i] = s_rt.gpuMs[i] * 0.9f + ms * 0.1f;
			}
			s_rt.budgetSample = qtrue;
			RT_FrameStatsAdd( &s_rt.gpuFrameStats, (float)( stamps[RT_TIMING_STAGES] - stamps[0] ) * toMs );
		}
	}
	qvkCmdResetQueryPool( cmd, s_rt.timingPool, slot * RT_TIMING_QUERIES, RT_TIMING_QUERIES );
	s_rt.timingWritten[slot] = qtrue;
}

static void RT_TimingStamp( VkCommandBuffer cmd, uint32_t slot, uint32_t index, VkPipelineStageFlagBits stage )
{
	if ( s_rt.timingPool == VK_NULL_HANDLE || !RT_TimingEnabled() ) {
		return;
	}
	qvkCmdWriteTimestamp( cmd, stage, s_rt.timingPool, slot * RT_TIMING_QUERIES + index );
}

static void RT_TimingReport( void )
{
	const int interval = rtx_rt_perf_timing && rtx_rt_perf_timing->integer == 2 ? 1 :
		( rtx_rt_perf_interval ? MAX( 1, rtx_rt_perf_interval->integer ) : 120 );
	if ( !rtx_rt_perf_timing || !rtx_rt_perf_timing->integer ) {
		return;
	}
	if ( ++s_rt.timingReports % interval == 0 ) {
		float p50, p95, p99;
		RT_FrameStatsPercentiles( &s_rt.gpuFrameStats, &p50, &p95, &p99 );
		ri.Printf( PRINT_ALL, "RTX RT GPU p50/p95/p99: %.2f/%.2f/%.2f ms (%u frames)\n",
			p50, p95, p99, s_rt.gpuFrameStats.count );
		ri.Printf( PRINT_ALL, "RTX RT quality: %s backend, direct %ux%u, indirect divisor %d, budget %.2f ms%s\n",
			RT_Compute() ? "compute" : "hardware", s_rt.traceWidth, s_rt.traceHeight, s_rt.indirectDivisor,
			rtx_rt_frame_budget_ms ? rtx_rt_frame_budget_ms->value : 0.0f,
			s_rt.indirectDivisor == 4 ? " (minimum indirect density)" : "" );
		ri.Printf( PRINT_ALL, "RTX RT timing: gpu build %.2f trace %.2f denoise %.2f composite %.2f = %.2f ms, cpu %.2f ms, %ux%u, "
			"%u still frames\n",
			s_rt.gpuMs[0], s_rt.gpuMs[1], s_rt.gpuMs[2], s_rt.gpuMs[3],
			s_rt.gpuMs[0] + s_rt.gpuMs[1] + s_rt.gpuMs[2] + s_rt.gpuMs[3], s_rt.cpuMs,
			s_rt.traceWidth, s_rt.traceHeight, s_rt.accumulated );
		if ( RT_Compute() ) {
			ri.Printf( PRINT_ALL, "RTX RT acceleration: %u dynamic builds, %u refits (all current-frame vertices)\n",
				s_rt.bvhBuilds, s_rt.bvhRefits );
		} else {
			ri.Printf( PRINT_ALL, "RTX RT acceleration: %u dynamic BLAS builds, %u updates (all current-frame vertices)\n",
				s_rt.blasBuilds, s_rt.blasUpdates );
		}
		ri.Printf( PRINT_ALL, "RTX RT geometry cache: %llu exact-pose hits, %llu extractions, IQM scratch %u influences\n",
			(unsigned long long)s_rtGeometryHits, (unsigned long long)s_rtGeometryMisses, s_rtIqmScratchCount );
		if ( s_rt.toneActive && s_rt.toneRead ) {
			// where the curve takes light 4 and 2 stops either side of the
			// metered luminance, in log2 display luminance before the bias
			const float stops[5] = { -4.0f, -2.0f, 0.0f, 2.0f, 4.0f };
			float mapped[5];
			int i;
			for ( i = 0; i < 5; i++ ) {
				const float x = Com_Clamp( 0.0f, (float)( RT_TONE_BINS - 1 ),
					( s_rt.toneMeteredLog + stops[i] - RT_TONE_LOG_MIN ) *
					( (float)RT_TONE_BINS / ( RT_TONE_LOG_MAX - RT_TONE_LOG_MIN ) ) );
				const int left = (int)x;
				const int right = MIN( left + 1, RT_TONE_BINS - 1 );
				mapped[i] = s_rt.toneCurve[left] + ( s_rt.toneCurve[right] - s_rt.toneCurve[left] ) * ( x - (float)left );
			}
			ri.Printf( PRINT_ALL, "RTX RT tone: metered luminance %.4g (%.2f stops); curve at -4 -2 0 +2 +4 stops: "
				"%.2f %.2f %.2f %.2f %.2f\n", exp2f( s_rt.toneMeteredLog ), s_rt.toneMeteredLog,
				mapped[0], mapped[1], mapped[2], mapped[3], mapped[4] );
		}
	}
}

// A traced frame that cannot be drawn says why, once per reason and session.
static qboolean RT_FrameFailed( uint32_t reason, const char *what )
{
	if ( !( s_rt.loggedFailures & ( 1u << reason ) ) ) {
		s_rt.loggedFailures |= 1u << reason;
		ri.Printf( PRINT_WARNING, "RTX RT: %s; the frame is not traced\n", what );
	}
	return qfalse;
}

static qboolean RT_TraceFrame( void )
{
	rtFrame_t *frame = &s_rt.frames[ vk.cmd_index % RT_FRAMES ];
	const uint32_t slot = (uint32_t)( vk.cmd_index % RT_FRAMES );
	const int64_t cpuStart = vk_rt_now_microseconds();
	VkCommandBuffer cmd = vk.cmd->command_buffer;
	uint32_t classPrims[RT_CLASS_COUNT][4];
	uint32_t instanceCount = 0;
	rtDenoisePush_t push;
	VkRect2D view;
	qboolean weapon;
	qboolean denoise;
	qboolean tonemap;
	uint32_t iterations;
	uint32_t it;
	int i;

	if ( s_rt.failed ) {
		return RT_FrameFailed( 7, "its pipelines failed earlier" );
	}
	if ( rtx_rt_patch_shadows && rtx_rt_patch_shadows->modified ) {
		// which world surfaces cast shadows is part of the world's BLASes
		rtx_rt_patch_shadows->modified = qfalse;
		s_rt.worldReady = qfalse;
	}
	if ( ( rtx_rt_lighting && rtx_rt_lighting->modified ) || ( rtx_rt_emissive_light && rtx_rt_emissive_light->modified ) ||
		( rtx_rt_camera && rtx_rt_camera->modified ) ) {
		// the lights, and which shaders are lights, are built for one mode,
		// and a glowing shader's light for one camera's units
		if ( rtx_rt_lighting ) {
			rtx_rt_lighting->modified = qfalse;
		}
		if ( rtx_rt_emissive_light ) {
			rtx_rt_emissive_light->modified = qfalse;
		}
		if ( rtx_rt_camera ) {
			rtx_rt_camera->modified = qfalse;
		}
		if ( s_rt.worldReady && vk_rt_wait_for_inflight_frames( "RT lighting change" ) ) {
			RT_MaterialsFree();
			s_rt.worldReady = qfalse;
			for ( i = 0; i < RT_FRAMES; i++ ) {
				s_rt.frames[i].materialGeneration = (uint32_t)-1;
			}
		}
	}
	if ( s_rt.defaultShader != tr.defaultShader ) {
		// the renderer restarted: shaders, images and the world are new
		s_rt.worldReady = qfalse;
		s_rt.defaultShader = tr.defaultShader;
		for ( i = 0; i < RT_FRAMES; i++ ) {
			s_rt.frames[i].textureCount = -1;
			s_rt.frames[i].materialGeneration = (uint32_t)-1;
		}
	}
	if ( !RT_EnsurePipelines() ) {
		s_rt.failed = qtrue;
		return RT_FrameFailed( 8, "its pipelines could not be created" );
	}
	RT_ViewRect( &view );
	if ( !RT_EnsureImages( view.extent.width, view.extent.height ) ) {
		return RT_FrameFailed( 0, "its images could not be created" );
	}
	if ( !s_rt.worldReady || s_rt.world != tr.world ) {
		if ( !vk_rt_wait_for_inflight_frames( "RT world build" ) || !RT_BuildWorld() ) {
			ri.Printf( PRINT_WARNING, "RTX RT: world acceleration structure build failed\n" );
			return qfalse;
		}
		s_rt.historyValid = qfalse;
		s_rt.toneClear = qtrue;
	}
	if ( !RT_ExtractDynamic() ) {
		return RT_FrameFailed( 1, "the scene's models could not be extracted" );
	}
	if ( !RT_UploadMaterials( frame ) ) {
		return RT_FrameFailed( 2, "its materials could not be uploaded" );
	}
	if ( !RT_EnsureMapped( &frame->frame, sizeof( rtFrameUniforms_t ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, qfalse, qfalse,
			"rt frame" ) ) {
		return RT_FrameFailed( 3, "its uniform buffer could not be created" );
	}

	vk_end_render_pass();
	RT_TimingBegin( cmd, slot );
	RT_TimingStamp( cmd, slot, 0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );

	Com_Memset( classPrims, 0, sizeof( classPrims ) );
	if ( !RT_RecordSceneBuild( frame, cmd, &instanceCount, classPrims ) ) {
		vk_begin_post_bloom_render_pass();
		return RT_FrameFailed( 4, "its scene could not be built" );
	}
	if ( instanceCount == 0 ) {
		vk_begin_post_bloom_render_pass();
		return RT_FrameFailed( 5, "its scene is empty" );
	}
	if ( !RT_UploadLights( frame ) ) {
		vk_begin_post_bloom_render_pass();
		return RT_FrameFailed( 6, "its lights could not be uploaded" );
	}
	weapon = ( s_rt.dynamicTriangles[1][0] + s_rt.dynamicTriangles[1][1] > 0 ) ? qtrue : qfalse;
	s_rt.parity ^= 1u;
	RT_WriteFrameUniforms( frame, classPrims, weapon );
	RT_UpdateDescriptors( frame );
	s_rt.cpuMs = s_rt.cpuMs * 0.9f + vk_rt_elapsed_ms( cpuStart, vk_rt_now_microseconds() ) * 0.1f;
	RT_TimingStamp( cmd, slot, 1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );

	// the previous frame's passes read and wrote these images
	{
		VkImageMemoryBarrier barriers[1];
		VkMemoryBarrier memory;
		Com_Memset( &memory, 0, sizeof( memory ) );
		memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memory.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
		memory.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		// the raster frame is sampled by the composite
		RT_ImageBarrier( &barriers[0], vk.color_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
		qvkCmdPipelineBarrier( cmd,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			RT_TraceStage() | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 1, &memory, 0, NULL, ARRAY_LEN( barriers ), barriers );
	}

	if ( RT_Compute() ) {
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_rt.rtPipeline );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_rt.rtPipelineLayout, 0, 1,
			&frame->rtSet, 0, NULL );
		qvkCmdDispatch( cmd, ( s_rt.traceWidth + 7 ) / 8, ( s_rt.traceHeight + 7 ) / 8, 1 );
	} else {
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, s_rt.rtPipeline );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, s_rt.rtPipelineLayout, 0, 1,
			&frame->rtSet, 0, NULL );
		qvkCmdTraceRaysKHR( cmd, &s_rt.sbtRaygen, &s_rt.sbtMiss, &s_rt.sbtHit, &s_rt.sbtCallable,
			s_rt.traceWidth, s_rt.traceHeight, 1 );
	}
	RT_TimingStamp( cmd, slot, 2, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );
	RT_ComputeBarrier( cmd, RT_TraceStage() );

	// denoise: temporal accumulation, then a-trous iterations
	denoise = ( !rtx_rt_denoise || rtx_rt_denoise->integer ) ? qtrue : qfalse;
	iterations = denoise ? (uint32_t)( rtx_rt_denoise_iterations ? Com_Clamp( 0, 6, rtx_rt_denoise_iterations->integer ) : 4 ) : 0u;
	push.traceSize[0] = (float)s_rt.traceWidth;
	push.traceSize[1] = (float)s_rt.traceHeight;
	push.traceSize[2] = 1.0f / (float)s_rt.traceWidth;
	push.traceSize[3] = 1.0f / (float)s_rt.traceHeight;
	push.outputSize[0] = (float)s_rt.outputWidth;
	push.outputSize[1] = (float)s_rt.outputHeight;
	push.outputSize[2] = 1.0f / (float)s_rt.outputWidth;
	push.outputSize[3] = 1.0f / (float)s_rt.outputHeight;
	// Direct shadows and indirect light use different response times. A slow
	// frame must not turn a short lighting response into seconds of history.
	{
		const int now = ri.Milliseconds();
		const float elapsed = s_rt.temporalTime ? (float)( now - s_rt.temporalTime ) * 0.001f : 1.0f / 60.0f;
		push.params[0] = s_rt.accumulated > 0 ? 0.0f : Com_Clamp( 0.001f, 0.25f, elapsed );
		s_rt.temporalTime = now;
	}
	push.params[1] = 4.0f;		// luminance edge stop (sigma)
	push.params[2] = 64.0f;		// normal edge stop (power)
	push.params[3] = 1.0f;		// depth edge stop
	push.state[0] = s_rt.parity;
	push.state[3] = ( ( s_rt.historyValid && denoise ) ? 1u : 0u ) | ( denoise ? 0u : 4u );
	push.extra[0] = s_rt.accumulated > 0 ? 4096u : 128u;
	push.extra[1] = rtx_rt_debug_visualizer ? (uint32_t)Com_Clamp( 0, 13, rtx_rt_debug_visualizer->integer ) : 0u;
	push.extra[2] = iterations;
	push.view[3] = (uint32_t)s_rt.indirectDivisor | ( ( (uint32_t)tr.frameCount %
		(uint32_t)( s_rt.indirectDivisor * s_rt.indirectDivisor ) ) << 8 );
	// Light in a map compiler's units is shown as it was traced: the light a
	// surface receives times its colour, as the engine shows a surface's
	// light, an SDR scene keeping the engine's lightmap ceiling on it.
	// Linear light is seen through a camera, as Q2RTX's is: its tone mapper,
	// or rtx_rt_exposure's fixed exposure and a filmic knee, unless a tone
	// mapper follows this pass
	tonemap = qfalse;
	{
		const int camera = RT_LightingCamera( s_rtLights.mode );
		const qboolean sceneLinear = vk_scene_linear_enabled();
		push.extra[3] = ( !sceneLinear && camera != RT_CAMERA_LINEAR ) ? RT_COMPOSITE_SHOULDER : 0u;
		if ( sceneLinear ) {
			push.extra[3] |= RT_COMPOSITE_LINEAR;
		}
		push.exposure[0] = (float)RT_EXPOSURE_NONE;
		push.exposure[1] = 1.0f;
		push.exposure[2] = 1.0f;
		push.exposure[3] = 0.0f;
		if ( camera == RT_CAMERA_LINEAR ) {
			const float fixed = rtx_rt_exposure ? rtx_rt_exposure->value : 0.0f;
			if ( fixed > 0.0f ) {
				push.exposure[0] = (float)RT_EXPOSURE_FIXED;
				push.exposure[1] = fixed;
				if ( !sceneLinear ) {
					push.extra[3] |= RT_COMPOSITE_KNEE;
				}
			} else {
				push.exposure[0] = (float)RT_EXPOSURE_AUTO;
				push.exposure[1] = rtx_rt_exposure_bias ? Com_Clamp( -8.0f, 8.0f, rtx_rt_exposure_bias->value ) : 0.0f;
				push.exposure[2] = rtx_rt_tonemap_curve ? Com_Clamp( 0.0f, 1.0f, rtx_rt_tonemap_curve->value ) : 0.5f;
				tonemap = qtrue;
			}
		}
	}
	// a curve fitted before the tone mapper last stopped is stale
	if ( tonemap && !s_rt.toneActive ) {
		s_rt.toneClear = qtrue;
	}
	s_rt.toneActive = tonemap;
	push.view[0] = (uint32_t)view.offset.x;
	push.view[1] = (uint32_t)view.offset.y;
	push.view[2] = 0;
	{
		const fnRTCWFogDraw_t fog = RT_ViewFog();
		Com_Memcpy( push.fog, fog.color, sizeof( push.fog ) );
	}

	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_rt.denoisePipelineLayout, 0, 1,
		&frame->denoiseSet, 0, NULL );
	RT_Dispatch( cmd, s_rt.temporalPipeline, &push, s_rt.traceWidth, s_rt.traceHeight );
	for ( it = 0; it < iterations; it++ ) {
		RT_ComputeBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
		push.state[1] = it;
		push.state[2] = 1u << it;
		push.state[3] = ( it == 0 ) ? 2u : 0u;
		RT_Dispatch( cmd, s_rt.atrousPipeline, &push, s_rt.traceWidth, s_rt.traceHeight );
	}
	RT_TimingStamp( cmd, slot, 3, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );
	RT_ComputeBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	if ( tonemap ) {
		RT_ToneMap( cmd, frame, &push );
	}
	RT_Dispatch( cmd, s_rt.compositePipeline, &push, s_rt.outputWidth, s_rt.outputHeight );
	s_rt.historyValid = qtrue;

	// composite -> scene colour
	{
		VkImageMemoryBarrier barriers[2];
		VkImageBlit blit;
		RT_ImageBarrier( &barriers[0], s_rt.output.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
		RT_ImageBarrier( &barriers[1], vk.color_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, NULL, 0, NULL, 2, barriers );

		Com_Memset( &blit, 0, sizeof( blit ) );
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.layerCount = 1;
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.layerCount = 1;
		blit.srcOffsets[1].x = (int32_t)s_rt.outputWidth;
		blit.srcOffsets[1].y = (int32_t)s_rt.outputHeight;
		blit.srcOffsets[1].z = 1;
		blit.dstOffsets[0].x = view.offset.x;
		blit.dstOffsets[0].y = view.offset.y;
		blit.dstOffsets[1].x = view.offset.x + (int32_t)s_rt.outputWidth;
		blit.dstOffsets[1].y = view.offset.y + (int32_t)s_rt.outputHeight;
		blit.dstOffsets[1].z = 1;
		qvkCmdBlitImage( cmd, s_rt.output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST );

		RT_ImageBarrier( &barriers[0], s_rt.output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT );
		RT_ImageBarrier( &barriers[1], vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, NULL, 0, NULL, 2, barriers );
	}

	RT_TimingStamp( cmd, slot, 4, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );
	RT_TimingReport();
	vk_begin_post_bloom_render_pass();

	if ( !s_rt.loggedActive ) {
		ri.Printf( PRINT_ALL, "RTX RT primary dispatch validation: path tracer traced %ux%u, composite copied to scene color; shading=native_rt, tracing=%s\n",
			s_rt.traceWidth, s_rt.traceHeight, RT_Compute() ? "compute" : "ray_tracing_pipeline" );
		s_rt.loggedActive = qtrue;
	}
	vk.rt.dispatchValid = qtrue;
	vk.rt.lastFrameBuilt = tr.frameCount;
	vk.rt.stats.dispatch_count++;
	return qtrue;
}

/*
====================================================================

Lifetime

====================================================================
*/

static void RT_DestroyFrame( rtFrame_t *frame )
{
	int c;
	RT_DestroyMapped( &frame->vertices );
	RT_DestroyMapped( &frame->indices );
	RT_DestroyMapped( &frame->materials );
	RT_DestroyMapped( &frame->stages );
	RT_DestroyMapped( &frame->frame );
	RT_DestroyMapped( &frame->instances );
	RT_DestroyMapped( &frame->lights );
	RT_DestroyMapped( &frame->previous );
	frame->lightGeneration = (uint32_t)-1;
	for ( c = 0; c < RT_DYNAMIC_CLASSES; c++ ) {
		vk_rt_destroy_as( &frame->blas[c].as );
		if ( frame->blas[c].topology ) RT_HostFree( frame->blas[c].topology );
		Com_Memset( &frame->blas[c], 0, sizeof( frame->blas[c] ) );
	}
	vk_rt_destroy_as( &frame->tlas );
	vk_rt_destroy_buffer( &frame->scratch );
	RT_DestroyMapped( &frame->bvhNodes );
	RT_DestroyMapped( &frame->bvhTris );
	RT_DestroyMapped( &frame->tone );
	frame->toneWritten = qfalse;
	frame->swInstanceCount = 0;
	frame->materialGeneration = (uint32_t)-1;
	frame->uploadedMaterials = 0;
	frame->uploadedStages = 0;
	frame->textureCount = -1;
	frame->tlasScratchSize = 0;
	frame->tlasInstanceCapacity = 0;
}

// Drop the world and the per-frame scene; pipelines and images stay.
static void RT_InvalidateScene( void )
{
	int i;
	RT_GeometryCacheClear();
	RT_HostFree( s_rtIqmScratch );
	s_rtIqmScratch = NULL;
	s_rtIqmScratchCount = 0;
	RT_DestroyWorld();
	vk_rt_destroy_buffer( &s_rt.lightGrid );
	vk_rt_destroy_buffer( &s_rt.lightIndices );
	s_rt.worldReady = qfalse;
	s_rt.world = NULL;
	RT_LightsFree();
	for ( i = 0; i < RT_FRAMES; i++ ) {
		VkDescriptorSet rtSet = s_rt.frames[i].rtSet;
		VkDescriptorSet denoiseSet = s_rt.frames[i].denoiseSet;
		RT_DestroyFrame( &s_rt.frames[i] );
		s_rt.frames[i].rtSet = rtSet;
		s_rt.frames[i].denoiseSet = denoiseSet;
	}
	s_rt.hasPrevCamera = qfalse;
	s_rt.historyValid = qfalse;
	for ( i = 0; i < RT_DYNAMIC_CLASSES; i++ ) {
		rtDynamicBvh_t *cache = &s_rt.dynamicBvh[i];
		RT_BvhFree( &cache->bvh );
		if ( cache->indices ) RT_HostFree( cache->indices );
		Com_Memset( cache, 0, sizeof( *cache ) );
	}
	s_rt.bvhBuilds = s_rt.bvhRefits = 0;
	s_rt.blasBuilds = s_rt.blasUpdates = 0;
	RT_MotionFree( &s_rt.motion[0] );
	RT_MotionFree( &s_rt.motion[1] );
	RT_MaterialsFree();
}

static void RT_Shutdown( void )
{
	int i;

	RT_InvalidateScene();
	RT_DestroyImages();
	for ( i = 0; i < RT_FRAMES; i++ ) {
		s_rt.frames[i].rtSet = VK_NULL_HANDLE;
		s_rt.frames[i].denoiseSet = VK_NULL_HANDLE;
	}
	if ( s_rt.rtPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, s_rt.rtPipeline, NULL );
	}
	if ( s_rt.temporalPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, s_rt.temporalPipeline, NULL );
	}
	if ( s_rt.atrousPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, s_rt.atrousPipeline, NULL );
	}
	if ( s_rt.compositePipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, s_rt.compositePipeline, NULL );
	}
	if ( s_rt.tonemapPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, s_rt.tonemapPipeline, NULL );
	}
	if ( s_rt.rtPipelineLayout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, s_rt.rtPipelineLayout, NULL );
	}
	if ( s_rt.denoisePipelineLayout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, s_rt.denoisePipelineLayout, NULL );
	}
	if ( s_rt.rtSetLayout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, s_rt.rtSetLayout, NULL );
	}
	if ( s_rt.denoiseSetLayout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, s_rt.denoiseSetLayout, NULL );
	}
	if ( s_rt.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, s_rt.pool, NULL );
	}
	vk_rt_destroy_buffer( &s_rt.sbt );
	vk_rt_destroy_buffer( &s_rt.dummy );
	vk_rt_destroy_buffer( &s_rt.tone );
	if ( s_rt.timingPool != VK_NULL_HANDLE ) {
		qvkDestroyQueryPool( vk.device, s_rt.timingPool, NULL );
	}
	for ( i = 0; i < RT_DYNAMIC_CLASSES; i++ ) {
		vk_rt_cpu_geometry_free( &s_rt.dynamic[i] );
	}
	if ( s_rt.partition ) {
		RT_HostFree( s_rt.partition );
	}
	RT_BvhFree( &s_rt.bvh );
	Com_Memset( &s_rt, 0, sizeof( s_rt ) );
}

/*
==================
vk_rt_pick_f

rtx_rt_pick x y z dx dy dz: the traced world surface a ray meets first, with
its material, hit point, normal and BSP lightmap coordinates. A diagnostic
for comparing traced shading with the map's baked result.
==================
*/
void vk_rt_pick_f( void )
{
	rtxRtCpuGeometry_t geometry;
	vec3_t origin, dir;
	float bestT = 1e30f;
	uint32_t best = (uint32_t)-1;
	float bestU = 0.0f, bestV = 0.0f;
	uint32_t t;
	int i;

	if ( ri.Cmd_Argc() == 1 && s_rt.hasPrevCamera ) {
		// the last traced view's centre
		for ( i = 0; i < 3; i++ ) {
			origin[i] = s_rt.prevOrigin[i];
			dir[i] = s_rt.prevForward[i];
		}
	} else if ( ri.Cmd_Argc() < 7 ) {
		ri.Printf( PRINT_ALL, "usage: rtx_rt_pick [x y z dx dy dz] (no arguments: the traced view's centre)\n" );
		return;
	} else {
		for ( i = 0; i < 3; i++ ) {
			origin[i] = Q_atof( ri.Cmd_Argv( i + 1 ) );
			dir[i] = Q_atof( ri.Cmd_Argv( i + 4 ) );
		}
	}
	if ( VectorNormalize( dir ) == 0.0f || !RT_ExtractWorld( &geometry ) ) {
		ri.Printf( PRINT_ALL, "rtx_rt_pick: no world or no direction\n" );
		return;
	}
	for ( t = 0; t + 2 < geometry.numIndices; t += 3 ) {
		const float *a = geometry.vertices[ geometry.indices[t + 0] ].xyz;
		const float *b = geometry.vertices[ geometry.indices[t + 1] ].xyz;
		const float *c = geometry.vertices[ geometry.indices[t + 2] ].xyz;
		vec3_t e1, e2, p, q, s;
		float det, inv, u, v, dist;

		// Moller-Trumbore, both faces
		VectorSubtract( b, a, e1 );
		VectorSubtract( c, a, e2 );
		CrossProduct( dir, e2, p );
		det = DotProduct( e1, p );
		if ( fabsf( det ) < 1e-8f ) {
			continue;
		}
		inv = 1.0f / det;
		VectorSubtract( origin, a, s );
		u = DotProduct( s, p ) * inv;
		if ( u < 0.0f || u > 1.0f ) {
			continue;
		}
		CrossProduct( s, e1, q );
		v = DotProduct( dir, q ) * inv;
		if ( v < 0.0f || u + v > 1.0f ) {
			continue;
		}
		dist = DotProduct( e2, q ) * inv;
		if ( dist > 0.01f && dist < bestT ) {
			bestT = dist;
			best = t;
			bestU = u;
			bestV = v;
		}
	}
	if ( best == (uint32_t)-1 ) {
		ri.Printf( PRINT_ALL, "rtx_rt_pick: no traced surface along the ray\n" );
	} else {
		const rtxRtPackedVertex_t *a = &geometry.vertices[ geometry.indices[best + 0] ];
		const rtxRtPackedVertex_t *b = &geometry.vertices[ geometry.indices[best + 1] ];
		const rtxRtPackedVertex_t *c = &geometry.vertices[ geometry.indices[best + 2] ];
		const float w = 1.0f - bestU - bestV;
		const uint32_t material = a->materialIndex;
		const rtMaterialInfo_t *info = material < s_rtMaterials.numMaterials ? &s_rtMaterials.info[material] : NULL;
		vec3_t hit;
		VectorMA( origin, bestT, dir, hit );
		ri.Printf( PRINT_ALL, "rtx_rt_pick: %s (material %u, owner %d, flags 0x%x) at %.1f %.1f %.1f, distance %.1f,"
			" normal %.2f %.2f %.2f, lightmap st %.4f %.4f, vertex colour %08x\n",
			info && info->shader ? info->shader->name : "?", material, info ? (int)info->owner : -1,
			material < s_rtMaterials.numMaterials ? s_rtMaterials.materials[material].flags : 0u,
			hit[0], hit[1], hit[2], bestT, a->normal[0], a->normal[1], a->normal[2],
			w * a->lightmapData[0] + bestU * b->lightmapData[0] + bestV * c->lightmapData[0],
			w * a->lightmapData[1] + bestU * b->lightmapData[1] + bestV * c->lightmapData[1], a->color );
		{
			// where the base texture's s and t grow, as a normal map leans
			// (rt_path.glsl rt_shading)
			const float s1 = b->texCoord[0] - a->texCoord[0], t1 = b->texCoord[1] - a->texCoord[1];
			const float s2 = c->texCoord[0] - a->texCoord[0], t2 = c->texCoord[1] - a->texCoord[1];
			const float det = s1 * t2 - s2 * t1;
			vec3_t e1, e2, sdir, tdir;
			VectorSubtract( b->xyz, a->xyz, e1 );
			VectorSubtract( c->xyz, a->xyz, e2 );
			if ( fabsf( det ) > 1e-12f ) {
				for ( i = 0; i < 3; i++ ) {
					sdir[i] = ( e1[i] * t2 - e2[i] * t1 ) / det;
					tdir[i] = ( e2[i] * s1 - e1[i] * s2 ) / det;
				}
				VectorNormalize( sdir );
				VectorNormalize( tdir );
				ri.Printf( PRINT_ALL, "rtx_rt_pick: texture s grows along %.3f %.3f %.3f, t along %.3f %.3f %.3f\n",
					sdir[0], sdir[1], sdir[2], tdir[0], tdir[1], tdir[2] );
			}
		}
		if ( info && info->shader ) {
			const uint32_t marker = (uint32_t)( a->lightmapData[3] + 0.5f );
			ri.Printf( PRINT_ALL, "rtx_rt_pick: shader surface flags 0x%x, contents 0x%x, sky %d, sun %d, "
				"surface light %.1f, lightmap %d, q3map's light %s\n",
				info->shader->surfaceFlags, info->shader->contentFlags, info->shader->isSky,
				info->shader->rtSunValid, info->shader->surfaceLight, info->shader->lightmapIndex,
				( marker & RT_MARK_FILTER ) ? "passes filtered" : ( marker & RT_MARK_NOSHADOW ) ? "passes" : "stops" );
			if ( info->shader->rtNormalMap || info->shader->rtMaterialMap ) {
				ri.Printf( PRINT_ALL, "rtx_rt_pick: stage %d takes normal map %s, material map %s\n",
					info->shader->rtMaterialStage,
					info->shader->rtNormalMap ? info->shader->rtNormalMap->imgName : "none",
					info->shader->rtMaterialMap ? info->shader->rtMaterialMap->imgName : "none" );
			}
		}
	}
	vk_rt_cpu_geometry_free( &geometry );
}

/*
====================================================================

Draw-surface ownership for the raster passes

====================================================================
*/

rtSurfaceOwner_t vk_rt_surface_owner( int entityNum, const shader_t *shader, int fogNum, int surfaceType )
{
	uint32_t material;
	rtOwner_t owner;
	qboolean world = ( entityNum == REFENTITYNUM_WORLD ) ? qtrue : qfalse;

	if ( vk.caps.activeRtMode != 2 || s_rt.failed ) {
		return RT_SURFACE_RASTER;
	}
	switch ( surfaceType ) {
	case SF_FACE:
	case SF_GRID:
	case SF_TRIANGLES:
		break;
	case SF_MD3:
	case SF_MDR:
	case SF_IQM:
	case SF_MDC:
	case SF_MDS:
	case SF_WOLFET:
		// the model formats RT_ExtractDynamic poses
		if ( world ) {
			return RT_SURFACE_RASTER;
		}
		break;
	default:
		return RT_SURFACE_RASTER;
	}
	if ( !world ) {
		const trRefEntity_t *ent = &backEnd.refdef.entities[ entityNum ];
		if ( RT_EntityClass( ent ) < 0 ) {
			return RT_SURFACE_RASTER;
		}
	}
	material = RT_MaterialIndexForShader( shader );
	owner = RT_MaterialOwner( material );
	if ( owner == RT_OWNER_BASE_RASTER ) {
		return world ? RT_SURFACE_BASE_RASTER : RT_SURFACE_RASTER;
	}
	if ( owner != RT_OWNER_TRACED ) {
		return RT_SURFACE_RASTER;
	}
	// a fogged surface is traced; the overlay blends Quake III's fog pass
	// over the traced colour, as raster blends it over the stages
	return fogNum != 0 ? RT_SURFACE_TRACED_FOGGED : RT_SURFACE_TRACED;
}
