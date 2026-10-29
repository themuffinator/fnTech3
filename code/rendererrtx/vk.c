#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "tr_local.h"
#include "../renderercommon/tr_q3_ihv_model_surface.h"
#include "../renderercommon/tr_mesh_material.h"
#include "../renderercommon/tr_rtcw_fog_gpu.h"
#include "../renderercommon/tr_entity_refraction.h"
#include "../renderercommon/tr_spot_cone.h"
#include "vk.h"
#include "rtx_mdr_geometry.h"
#include "rt/rt_gpu.h"
#include "rt/rt_stage_source.h"

// Path tracer (rt/rt_backend.c, included at the end of this file)
static qboolean RT_TraceFrame( void );
static void RT_InvalidateScene( void );
static void RT_Shutdown( void );

#include "../renderercommon/tr_menu_blur.h"
#include "../renderercommon/tr_motion_blur.h"
#include "../renderercommon/tr_underwater.h"

#define VK_POST_COLOR_SPACE_SDR 0
#define VK_POST_COLOR_SPACE_HDR10_ST2084 1

#if defined( _DEBUG ) || !defined( NDEBUG )
#define USE_VK_VALIDATION
#endif

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;
static motionBlurViewState_t vk_motion_blur_view;

static VkInstance vk_instance = VK_NULL_HANDLE;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
static qboolean vk_instance_swapchain_colorspace = qfalse;

#ifdef USE_VK_VALIDATION
VkDebugReportCallbackEXT vk_debug_callback = VK_NULL_HANDLE;
#endif

//
// Vulkan API functions used by the renderer.
//
static PFN_vkCreateInstance								qvkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;
static PFN_vkEnumerateInstanceVersion					qvkEnumerateInstanceVersion;

static PFN_vkCreateDevice								qvkCreateDevice;
static PFN_vkDestroyInstance							qvkDestroyInstance;
static PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
static PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceFeatures2				qvkGetPhysicalDeviceFeatures2;
static PFN_vkGetPhysicalDeviceFeatures2KHR				qvkGetPhysicalDeviceFeatures2KHR;
static PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugReportCallbackEXT				qvkCreateDebugReportCallbackEXT;
static PFN_vkDestroyDebugReportCallbackEXT				qvkDestroyDebugReportCallbackEXT;
#endif
static PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
static PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
static PFN_vkAllocateMemory								qvkAllocateMemory;
static PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
static PFN_vkBindBufferMemory							qvkBindBufferMemory;
static PFN_vkBindImageMemory							qvkBindImageMemory;
static PFN_vkCmdBeginRenderPass							qvkCmdBeginRenderPass;
static PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
static PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
static PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
static PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
static PFN_vkCmdBlitImage								qvkCmdBlitImage;
static PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
static PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
static PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
static PFN_vkCmdCopyImage								qvkCmdCopyImage;
static PFN_vkCmdDispatch								qvkCmdDispatch;
static PFN_vkCmdFillBuffer								qvkCmdFillBuffer;
static PFN_vkCmdDraw									qvkCmdDraw;
static PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
static PFN_vkCmdEndRenderPass							qvkCmdEndRenderPass;
static PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
static PFN_vkCmdPipelineBarrier							qvkCmdPipelineBarrier;
static PFN_vkCmdPushConstants							qvkCmdPushConstants;
static PFN_vkCmdResetQueryPool							qvkCmdResetQueryPool;
static PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
static PFN_vkCmdSetScissor								qvkCmdSetScissor;
static PFN_vkCmdSetLineWidth							qvkCmdSetLineWidth;
static PFN_vkCmdSetLineStippleEXT						qvkCmdSetLineStippleEXT;
static PFN_vkCmdSetViewport								qvkCmdSetViewport;
static PFN_vkCmdWriteTimestamp							qvkCmdWriteTimestamp;
static PFN_vkCreateBuffer								qvkCreateBuffer;
static PFN_vkCreateCommandPool							qvkCreateCommandPool;
static PFN_vkCreateComputePipelines					qvkCreateComputePipelines;
static PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
static PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
static PFN_vkCreateFence								qvkCreateFence;
static PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
static PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
static PFN_vkCreateImage								qvkCreateImage;
static PFN_vkCreateImageView							qvkCreateImageView;
static PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
static PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
static PFN_vkCreateRenderPass							qvkCreateRenderPass;
static PFN_vkCreateSampler								qvkCreateSampler;
static PFN_vkCreateSemaphore							qvkCreateSemaphore;
static PFN_vkCreateShaderModule							qvkCreateShaderModule;
static PFN_vkDestroyBuffer								qvkDestroyBuffer;
static PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
static PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
static PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
static PFN_vkDestroyDevice								qvkDestroyDevice;
static PFN_vkDestroyFence								qvkDestroyFence;
static PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
static PFN_vkDestroyImage								qvkDestroyImage;
static PFN_vkDestroyImageView							qvkDestroyImageView;
static PFN_vkDestroyPipeline							qvkDestroyPipeline;
static PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
static PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
static PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
static PFN_vkDestroySampler								qvkDestroySampler;
static PFN_vkDestroySemaphore							qvkDestroySemaphore;
static PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
static PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
static PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
static PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
static PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
static PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
static PFN_vkFreeMemory									qvkFreeMemory;
static PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
static PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
static PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
static PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
static PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
static PFN_vkMapMemory									qvkMapMemory;
static PFN_vkQueueSubmit								qvkQueueSubmit;
static PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
static PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
static PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
static PFN_vkResetFences								qvkResetFences;
static PFN_vkUnmapMemory								qvkUnmapMemory;
static PFN_vkUpdateDescriptorSets						qvkUpdateDescriptorSets;
static PFN_vkWaitForFences								qvkWaitForFences;
static PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
static PFN_vkCreateSwapchainKHR							qvkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
static PFN_vkQueuePresentKHR							qvkQueuePresentKHR;
static PFN_vkGetPhysicalDeviceProperties2				qvkGetPhysicalDeviceProperties2;
static PFN_vkGetPhysicalDeviceProperties2KHR			qvkGetPhysicalDeviceProperties2KHR;
static PFN_vkCreateQueryPool							qvkCreateQueryPool;
static PFN_vkDestroyQueryPool							qvkDestroyQueryPool;
static PFN_vkGetQueryPoolResults						qvkGetQueryPoolResults;
static PFN_vkSetHdrMetadataEXT							qvkSetHdrMetadataEXT;

static PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
static PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;
static PFN_vkGetBufferDeviceAddress						qvkGetBufferDeviceAddress;
static PFN_vkGetBufferDeviceAddressKHR					qvkGetBufferDeviceAddressKHR;
static PFN_vkCreateAccelerationStructureKHR				qvkCreateAccelerationStructureKHR;
static PFN_vkDestroyAccelerationStructureKHR			qvkDestroyAccelerationStructureKHR;
static PFN_vkGetAccelerationStructureBuildSizesKHR		qvkGetAccelerationStructureBuildSizesKHR;
static PFN_vkCmdBuildAccelerationStructuresKHR			qvkCmdBuildAccelerationStructuresKHR;
static PFN_vkGetAccelerationStructureDeviceAddressKHR	qvkGetAccelerationStructureDeviceAddressKHR;
static PFN_vkCreateRayTracingPipelinesKHR				qvkCreateRayTracingPipelinesKHR;
static PFN_vkGetRayTracingShaderGroupHandlesKHR			qvkGetRayTracingShaderGroupHandlesKHR;
static PFN_vkCmdTraceRaysKHR							qvkCmdTraceRaysKHR;
static PFN_vkCmdCopyAccelerationStructureKHR			qvkCmdCopyAccelerationStructureKHR;
static PFN_vkCmdWriteAccelerationStructuresPropertiesKHR qvkCmdWriteAccelerationStructuresPropertiesKHR;

static PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;
static PFN_vkCmdDebugMarkerBeginEXT					qvkCmdDebugMarkerBeginEXT;
static PFN_vkCmdDebugMarkerEndEXT						qvkCmdDebugMarkerEndEXT;
static PFN_vkCmdDebugMarkerInsertEXT					qvkCmdDebugMarkerInsertEXT;

////////////////////////////////////////////////////////////////////////////

// forward declaration
VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );
static const char *vk_result_string( VkResult code );
static uint32_t find_memory_type( uint32_t memory_type_bits, VkMemoryPropertyFlags properties );
static uint32_t find_memory_type2( uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops );
static VkCommandBuffer begin_command_buffer( void );
static void end_command_buffer( VkCommandBuffer command_buffer, const char *location );
static qboolean end_command_buffer_soft( VkCommandBuffer command_buffer, const char *location );
static void vk_begin_post_bloom_render_pass( void );
static void get_viewport_rect( VkRect2D *r );
static void vk_begin_global_fog_render_pass( void );
static void vk_begin_liquid_snapshot_render_pass( void );

/* The scene snapshot serves enhanced liquids and the entity refraction pass
 * (renderercommon/tr_entity_refraction.h). */
static qboolean vk_liquid_requested( void )
{
	return ( ( r_liquid && r_liquid->integer > 0 ) || R_EntityRefractionAllocates() ) ?
		qtrue : qfalse;
}

static qboolean vk_depth_fade_requested( void )
{
	return ( ( r_depthFade && r_depthFade->integer ) ||
		R_CelShadingWorldActive() ) ? qtrue : qfalse;
}

static qboolean vk_scene_depth_snapshot_requested( void )
{
	return ( vk_liquid_requested() || vk_depth_fade_requested() ) ?
		qtrue : qfalse;
}

static qboolean vk_global_fog_requested( void )
{
	return ( r_globalFog && r_globalFog->integer &&
		vk.globalFogDepthSampleSupported ) ? qtrue : qfalse;
}

static qboolean vk_global_fog_enabled( void )
{
	return ( vk_global_fog_requested() && vk.fboActive &&
		vkSamples == VK_SAMPLE_COUNT_1_BIT ) ? qtrue : qfalse;
}

static qboolean vk_underwater_requested( void )
{
	return ( r_underwater && r_underwater->integer ) ? qtrue : qfalse;
}

static qboolean vk_underwater_enabled( void )
{
	return ( vk_underwater_requested() && vk.fboActive ) ? qtrue : qfalse;
}

/* The warp and the edge falloff only need the resolved scene color, so the
 * layer still runs where depth cannot be sampled; only the distance absorption
 * depends on the shared depth-sample view, and it drops out on its own by
 * uploading a zero medium density. */
static qboolean vk_underwater_depth_enabled( void )
{
	return ( vk_underwater_enabled() && vk.globalFogDepthSampleSupported &&
		vkSamples == VK_SAMPLE_COUNT_1_BIT ) ? qtrue : qfalse;
}

typedef enum {
	RTX_VK_LIFETIME_UNINITIALIZED = 0,
	RTX_VK_LIFETIME_INITIALIZING,
	RTX_VK_LIFETIME_READY,
	RTX_VK_LIFETIME_RESIZING,
	RTX_VK_LIFETIME_SHUTTING_DOWN
} rtxVkLifetimeState_t;

typedef enum {
	RTX_VK_RESOURCE_SYNC_PRIMITIVES = 0,
	RTX_VK_RESOURCE_SWAPCHAIN,
	RTX_VK_RESOURCE_ATTACHMENTS,
	RTX_VK_RESOURCE_RENDER_PASSES,
	RTX_VK_RESOURCE_FRAMEBUFFERS,
	RTX_VK_RESOURCE_GEOMETRY_BUFFERS,
	RTX_VK_RESOURCE_STORAGE_BUFFER,
	RTX_VK_RESOURCE_COUNT
} rtxVkResource_t;

typedef struct {
	qboolean alive;
	uint32_t createCount;
	uint32_t destroyCount;
	uint32_t useCount;
	uint32_t lastSerial;
	uint32_t lastUseFrame;
} rtxVkResourceEntry_t;

typedef struct {
	rtxVkLifetimeState_t state;
	rtxVkResourceEntry_t resources[ RTX_VK_RESOURCE_COUNT ];
	uint32_t serial;
	uint32_t generation;
	uint32_t frameSerial;
	qboolean frameActive;
} rtxVkLifetimeTracker_t;

static rtxVkLifetimeTracker_t s_vkLifetimeTracker;
static qboolean s_vkSwapchainResizePending = qfalse;
static qboolean s_vkSwapchainResizeRestarting = qfalse;
static VkResult s_vkSwapchainResizeResult = VK_SUCCESS;
static qboolean s_vkValidationLayerEnabled = qfalse;
static uint32_t s_vkInstanceApiVersion = VK_API_VERSION_1_0;
static int s_vkRtTextureBudgetFrame = -1;
static VkDeviceSize s_vkRtTextureBudgetUsed = 0;

typedef enum {
	RTX_VK_DESC_LAYOUT_SAMPLER = 0,
	RTX_VK_DESC_LAYOUT_UNIFORM,
	RTX_VK_DESC_LAYOUT_STORAGE,
	RTX_VK_DESC_LAYOUT_COUNT
} rtxVkDescriptorLayoutId_t;

typedef enum {
	RTX_VK_PIPELINE_LAYOUT_MAIN = 0,
	RTX_VK_PIPELINE_LAYOUT_STORAGE,
	RTX_VK_PIPELINE_LAYOUT_POST_PROCESS,
	RTX_VK_PIPELINE_LAYOUT_BLEND,
	RTX_VK_PIPELINE_LAYOUT_COUNT
} rtxVkPipelineLayoutId_t;

typedef struct {
	float xyz[4];
	float normal[4];
	float tangent[4];
	float texCoord[2];
	uint32_t color;
	uint32_t materialIndex;
	// world: BSP lightmap st, unused, 1 where the map's lights pass through
	// (extraction only); dynamic: shaderTexCoord st, shaderTime, 1 for a
	// brush model (rt/rt_backend.c)
	float lightmapData[4];
} rtxRtPackedVertex_t;

typedef char rtxRtPackedVertexSizeCheck[(sizeof(rtxRtPackedVertex_t) == 80) ? 1 : -1];
typedef char rtxRtLightmapOffsetCheck[(offsetof(rtxRtPackedVertex_t, lightmapData) == 64) ? 1 : -1];

typedef enum {
	RTX_RT_MATFLAG_NONE = 0,
	RTX_RT_MATFLAG_MASKED = 1u << 0,
	RTX_RT_MATFLAG_CULL_FRONT = 1u << 9,
	RTX_RT_MATFLAG_CULL_BACK = 1u << 10,
	RTX_RT_MATFLAG_RASTER_OVERLAY = 1u << 11
} rtxRtMaterialFlags_t;

/* A geometry's material slot names its shader (materialShaders) and keeps
 * extraction flags; the path tracer maps the shader to its shared material
 * table (rt/rt_material.c). */
typedef struct {
	uint32_t flags;
} rtxRtMaterial_t;

typedef struct {
	rtxRtPackedVertex_t *vertices;
	uint32_t *indices;
	rtxRtMaterial_t *materials;
	const shader_t **materialShaders;
	uint32_t numVertices;
	uint32_t numIndices;
	uint32_t numMaterials;
	uint32_t vertexCapacity;
	uint32_t indexCapacity;
	uint32_t materialCapacity;
	uint32_t sourceTriangleCount;
	uint32_t degenerateTriangleCount;
	uint32_t maskedTriangleCount;
	uint32_t anyHitTriangleCount;
	uint32_t sourceEntityCount;
	uint32_t sourceEffectCount;
} rtxRtCpuGeometry_t;

/* The descriptors the path tracer's layouts bind (rt/rt_backend.c): the
 * ray-tracing set's buffers (bindings 1-9, the dynamic vertices' previous
 * positions and the world triangles' lights), uniform block, texture table
 * and output images
 * in each ray-tracing stage, and the denoiser set's images (RT_DN_*: 25
 * descriptors, the history and filter pairs counting twice) in compute. */
#define RTX_RT_STORAGE_BUFFER_COUNT ( RT_BINDING_LIGHT_INDICES - RT_BINDING_WORLD_VERTICES + 3u )
#define RTX_RT_UNIFORM_BUFFER_COUNT 1u
#define RTX_RT_TRACE_STORAGE_IMAGE_COUNT ( RT_BINDING_OUT_INDIRECT - RT_BINDING_OUT_COLOR + 1u )
#define RTX_RT_DENOISE_STORAGE_IMAGE_COUNT 25u
#define RTX_RT_STORAGE_IMAGE_COUNT RTX_RT_DENOISE_STORAGE_IMAGE_COUNT
#define RTX_RT_ACCELERATION_STRUCTURE_COUNT 1u
#define RTX_RT_SAMPLED_IMAGE_COUNT RT_MAX_TEXTURES
#define RTX_RT_PER_STAGE_RESOURCE_COUNT \
	( RTX_RT_SAMPLED_IMAGE_COUNT + RTX_RT_STORAGE_BUFFER_COUNT + RTX_RT_UNIFORM_BUFFER_COUNT + \
	  RTX_RT_TRACE_STORAGE_IMAGE_COUNT + RTX_RT_ACCELERATION_STRUCTURE_COUNT )
/* The compute backend binds its BVH buffers in place of the TLAS: the
 * world's nodes at the TLAS binding and three more (RT_BINDING_SW_*), all in
 * the compute stage. */
#define RTX_RT_COMPUTE_STORAGE_BUFFER_COUNT \
	( RTX_RT_STORAGE_BUFFER_COUNT + 1u + ( RT_BINDING_SW_COUNT - RT_BINDING_COUNT ) )
#define RTX_RT_COMPUTE_PER_STAGE_RESOURCE_COUNT \
	( RTX_RT_SAMPLED_IMAGE_COUNT + RTX_RT_COMPUTE_STORAGE_BUFFER_COUNT + RTX_RT_UNIFORM_BUFFER_COUNT + \
	  RTX_RT_TRACE_STORAGE_IMAGE_COUNT )

/* Descriptor updates run on the renderer thread.  Keep the full fixed shader
 * table out of the call stack without paying a per-frame heap allocation. */
static VkDescriptorImageInfo s_vkRtDescriptorImageInfos[RT_MAX_TEXTURES];

static void vk_rt_shutdown( void );
static qboolean vk_rt_trace_available( void );
static qboolean vk_rt_init( void );
static void vk_rt_reset_state( void );
static qboolean vk_rt_align_up_checked( VkDeviceSize value, VkDeviceSize alignment, VkDeviceSize *outValue );
static qboolean vk_rt_create_buffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProps, qboolean needDeviceAddress, const char *debugName, rtxVkRtBuffer_t *outBuffer );
static qboolean vk_rt_create_buffer_ex( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProps, VkMemoryPropertyFlags preferredProps, qboolean needDeviceAddress, const char *debugName, rtxVkRtBuffer_t *outBuffer );
static void vk_rt_destroy_buffer( rtxVkRtBuffer_t *buffer );
static qboolean vk_rt_upload_buffer_data( const rtxVkRtBuffer_t *buffer, const void *src, size_t bytes );
static qboolean vk_rt_wait_for_inflight_frames( const char *reason );
static void vk_rt_destroy_as( rtxVkRtAccelerationStructure_t *as );
static qboolean vk_rt_create_as( VkAccelerationStructureTypeKHR type, VkDeviceSize size, const char *debugName, rtxVkRtAccelerationStructure_t *outAs );
static void vk_rt_cpu_geometry_init( rtxRtCpuGeometry_t *geometry );
static void vk_rt_cpu_geometry_free( rtxRtCpuGeometry_t *geometry );
static qboolean vk_rt_cpu_geometry_reserve( rtxRtCpuGeometry_t *geometry, uint32_t additionalVertices, uint32_t additionalIndices );
static qboolean vk_rt_cpu_geometry_reserve_materials( rtxRtCpuGeometry_t *geometry, uint32_t additionalMaterials );
static qboolean vk_rt_cpu_geometry_add_vertex( rtxRtCpuGeometry_t *geometry, const vec3_t xyz, const vec3_t normal, const vec4_t tangent, const vec2_t texCoord, uint32_t color, uint32_t materialIndex, uint32_t *outIndex );
static qboolean vk_rt_cpu_geometry_add_index( rtxRtCpuGeometry_t *geometry, uint32_t index );
static qboolean vk_rt_cpu_geometry_add_triangle( rtxRtCpuGeometry_t *geometry, uint32_t i0, uint32_t i1, uint32_t i2 );
static qboolean vk_rt_cpu_geometry_find_or_add_material( rtxRtCpuGeometry_t *geometry, const shader_t *shader, uint32_t *outMaterialIndex );
static qboolean vk_rt_cpu_geometry_find_or_add_material_ex( rtxRtCpuGeometry_t *geometry, const shader_t *shader, uint32_t extraFlags, uint32_t *outMaterialIndex );
static qboolean vk_rt_append_surface_face_geometry( rtxRtCpuGeometry_t *geometry, const srfSurfaceFace_t *surface, const shader_t *shader, uint32_t materialFlags );
static qboolean vk_rt_append_surface_grid_geometry( rtxRtCpuGeometry_t *geometry, const srfGridMesh_t *surface, const shader_t *shader, uint32_t materialFlags );
static qboolean vk_rt_append_surface_triangles_geometry( rtxRtCpuGeometry_t *geometry, const srfTriangles_t *surface, const shader_t *shader, uint32_t materialFlags );
static qboolean vk_rt_append_brush_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent, const model_t *model );
static qboolean vk_rt_append_md3_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent );
static qboolean vk_rt_append_iqm_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent );
static qboolean vk_rt_append_mdr_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent );
static qboolean vk_rt_append_dynamic_model_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent, uint32_t *outModelCount );
static void vk_rt_transform_geometry_range( rtxRtCpuGeometry_t *geometry, uint32_t firstVertex, const trRefEntity_t *ent );
static const shader_t *vk_rt_resolve_md3_shader( const trRefEntity_t *ent, const md3Surface_t *surface, int ordinal );
static const shader_t *vk_rt_resolve_iqm_shader( const trRefEntity_t *ent, const srfIQModel_t *surface );
static void vk_rt_decode_md3_normal( short packedNormal, vec3_t outNormal );
static void vk_rt_transform_point( const trRefEntity_t *ent, const vec3_t local, vec3_t world );
static void vk_rt_transform_direction( const trRefEntity_t *ent, const vec3_t local, vec3_t world );
static uint32_t vk_rt_color_u32_from_bytes( const byte *rgba );
static VkSampler vk_find_sampler( const Vk_Sampler_Def *def );

static int vk_lifetime_debug_level( void )
{
	if ( !rtx_debug_resource_lifetime ) {
		return 0;
	}
	return rtx_debug_resource_lifetime->integer;
}

static int vk_debug_markers_mode( void )
{
	if ( !rtx_debug_gpu_markers ) {
		return 0;
	}
	return rtx_debug_gpu_markers->integer;
}

static qboolean vk_validation_requested( void )
{
	if ( !rtx_debug_vk_validation ) {
		return qfalse;
	}
	return rtx_debug_vk_validation->integer ? qtrue : qfalse;
}

typedef enum {
	RTX_RT_MODE_DISABLED = 0,
	RTX_RT_MODE_RAY_QUERY = 1,
	RTX_RT_MODE_RAY_TRACING_PIPELINE = 2
} rtxRtMode_t;

/* How the path tracer (mode 2) traces: rtx_rt_backend 0 takes the first
 * available, 1 and 2 ask for one. */
typedef enum {
	RTX_RT_BACKEND_NONE = 0,
	RTX_RT_BACKEND_HARDWARE = 1,	// VK_KHR_ray_tracing_pipeline over acceleration structures
	RTX_RT_BACKEND_COMPUTE = 2		// compute shaders over the renderer's own BVHs (rt/rt_bvh.c)
} rtxRtBackend_t;

static int vk_caps_report_level( void )
{
	if ( !rtx_caps_report ) {
		return 0;
	}
	return rtx_caps_report->integer;
}

static int vk_requested_rt_mode( void )
{
	if ( !rtx_rt_mode ) {
		return RTX_RT_MODE_DISABLED;
	}
	return rtx_rt_mode->integer;
}

static int vk_requested_rt_backend( void )
{
	if ( !rtx_rt_backend ) {
		return RTX_RT_BACKEND_NONE;
	}
	return (int)Com_Clamp( RTX_RT_BACKEND_NONE, RTX_RT_BACKEND_COMPUTE, rtx_rt_backend->integer );
}

static const char *vk_rt_backend_name( int backend )
{
	switch ( backend ) {
	case RTX_RT_BACKEND_HARDWARE: return "ray_tracing_pipeline";
	case RTX_RT_BACKEND_COMPUTE: return "compute";
	default: return "none";
	}
}

static qboolean vk_rt_mode_required( void )
{
	if ( !rtx_rt_require ) {
		return qfalse;
	}
	return rtx_rt_require->integer ? qtrue : qfalse;
}

static qboolean vk_rt_async_overlap_enabled( void )
{
	if ( !rtx_rt_async_overlap ) {
		return qfalse;
	}
	return rtx_rt_async_overlap->integer ? qtrue : qfalse;
}

static int vk_rt_texture_stream_budget_mb( void )
{
	if ( !rtx_rt_texture_stream_budget_mb ) {
		return 96;
	}
	return Com_Clamp( 0, 2048, rtx_rt_texture_stream_budget_mb->integer );
}

static int64_t vk_rt_now_microseconds( void )
{
	if ( ri.Microseconds ) {
		return ri.Microseconds();
	}
	return (int64_t)ri.Milliseconds() * 1000;
}

static float vk_rt_elapsed_ms( int64_t startUs, int64_t endUs )
{
	if ( endUs <= startUs ) {
		return 0.0f;
	}
	return (float)( (double)( endUs - startUs ) * ( 1.0 / 1000.0 ) );
}

#include "rt/rt_frame_stats.h"

static struct {
	int64_t events[REF_FRAME_EVENT_COUNT];
	int64_t submit, present, previousPresent;
	float earlyFence, lateFence, earlyAcquire, lateAcquire, submitMs, presentMs;
	qboolean preparingInput;
	int lastSubmittedSlot;
	unsigned reports;
	rtFrameStats_t frames;
} s_vkRtLatency;

static qboolean vk_rt_latency_timing( void )
{
	return rtx_rt_perf_timing && rtx_rt_perf_timing->integer && vk.caps.activeRtMode == 2;
}

static void vk_rt_latency_wait( int64_t start, qboolean acquire )
{
	float ms;
	if ( !vk_rt_latency_timing() ) return;
	ms = vk_rt_elapsed_ms( start, vk_rt_now_microseconds() );
	if ( acquire ) {
		if ( s_vkRtLatency.preparingInput ) s_vkRtLatency.earlyAcquire += ms;
		else s_vkRtLatency.lateAcquire += ms;
	} else {
		if ( s_vkRtLatency.preparingInput ) s_vkRtLatency.earlyFence += ms;
		else s_vkRtLatency.lateFence += ms;
	}
}

static void vk_rt_latency_report( int64_t presented )
{
	float p50, p95, p99;
	int interval;
	if ( !vk_rt_latency_timing() ) return;
	if ( s_vkRtLatency.previousPresent ) {
		RT_FrameStatsAdd( &s_vkRtLatency.frames, vk_rt_elapsed_ms( s_vkRtLatency.previousPresent, presented ) );
	}
	s_vkRtLatency.previousPresent = presented;
	interval = rtx_rt_perf_timing->integer == 2 ? 1 : MAX( 1, rtx_rt_perf_interval->integer );
	if ( ++s_vkRtLatency.reports % interval ) return;
	RT_FrameStatsPercentiles( &s_vkRtLatency.frames, &p50, &p95, &p99 );
	ri.Printf( PRINT_ALL, "RTX RT latency: CPU frame p50/p95/p99 %.2f/%.2f/%.2f ms (%u), input-to-submit %.2f ms, "
		"simulation %.2f, fence before/after input %.2f/%.2f, acquire %.2f/%.2f, submit/present API %.2f/%.2f ms, policy %d\n",
		p50, p95, p99, s_vkRtLatency.frames.count,
		s_vkRtLatency.events[REF_FRAME_INPUT_SAMPLE] ? vk_rt_elapsed_ms( s_vkRtLatency.events[REF_FRAME_INPUT_SAMPLE], s_vkRtLatency.submit ) : 0.0f,
		s_vkRtLatency.events[REF_FRAME_SIMULATION_BEGIN] ? vk_rt_elapsed_ms( s_vkRtLatency.events[REF_FRAME_SIMULATION_BEGIN],
			s_vkRtLatency.events[REF_FRAME_SIMULATION_END] ) : 0.0f,
		s_vkRtLatency.earlyFence, s_vkRtLatency.lateFence, s_vkRtLatency.earlyAcquire, s_vkRtLatency.lateAcquire,
		s_vkRtLatency.submitMs, s_vkRtLatency.presentMs, rtx_rt_low_latency ? rtx_rt_low_latency->integer : 0 );
	if ( rtx_rt_perf_timing->integer == 2 ) {
		ri.Printf( PRINT_ALL, "RTX RT CPU stamps us: prepare %lld poll %lld input %lld sim %lld..%lld submit %lld present %lld..%lld\n",
			(long long)s_vkRtLatency.events[REF_FRAME_PREPARE_INPUT], (long long)s_vkRtLatency.events[REF_FRAME_INPUT_POLLED],
			(long long)s_vkRtLatency.events[REF_FRAME_INPUT_SAMPLE], (long long)s_vkRtLatency.events[REF_FRAME_SIMULATION_BEGIN],
			(long long)s_vkRtLatency.events[REF_FRAME_SIMULATION_END], (long long)s_vkRtLatency.submit,
			(long long)s_vkRtLatency.present, (long long)presented );
	}
}

static int vk_rt_debug_as_stats_level( void )
{
	if ( !rtx_rt_debug_as_stats ) {
		return 0;
	}
	return rtx_rt_debug_as_stats->integer;
}

static qboolean vk_rt_debug_should_log_skip( int *lastLoggedFrame )
{
	const int logIntervalFrames = 128;
	const int frame = tr.frameCount;

	if ( vk_rt_debug_as_stats_level() < 2 ) {
		return qfalse;
	}
	if ( *lastLoggedFrame >= 0 &&
		frame >= *lastLoggedFrame &&
		frame - *lastLoggedFrame < logIntervalFrames ) {
		return qfalse;
	}

	*lastLoggedFrame = frame;
	return qtrue;
}

static qboolean vk_rt_ui_passthrough_enabled( void )
{
	if ( !rtx_rt_ui_passthrough ) {
		return qtrue;
	}
	return rtx_rt_ui_passthrough->integer ? qtrue : qfalse;
}

static const char *vk_rt_mode_name( int mode )
{
	switch ( mode ) {
	case RTX_RT_MODE_DISABLED: return "disabled";
	case RTX_RT_MODE_RAY_QUERY: return "ray_query";
	case RTX_RT_MODE_RAY_TRACING_PIPELINE: return "ray_tracing_pipeline";
	default: return "unknown";
	}
}

// The path tracer traces through VK_KHR_ray_tracing_pipeline
static qboolean vk_rt_pipeline_enabled( void )
{
	return ( vk.caps.activeRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
		vk.caps.rtBackend == RTX_RT_BACKEND_HARDWARE ) ? qtrue : qfalse;
}

// The path tracer runs, with either backend
static qboolean vk_rt_trace_available( void )
{
	return ( vk.caps.activeRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
		vk.caps.rtBackend != RTX_RT_BACKEND_NONE ) ? qtrue : qfalse;
}

// Whether this renderer instance traces: the shader parser looks for the
// path tracer's material maps only then.
qboolean vk_rt_tracing( void )
{
	return vk_rt_trace_available();
}

static void vk_rt_reset_state( void )
{
	Com_Memset( &vk.rt, 0, sizeof( vk.rt ) );
	vk.rt.gpu_timestamp_period_ns = 1.0f;
	s_vkRtTextureBudgetFrame = -1;
	s_vkRtTextureBudgetUsed = 0;
}

static qboolean vk_rt_align_up_checked( VkDeviceSize value, VkDeviceSize alignment, VkDeviceSize *outValue )
{
	VkDeviceSize remainder;
	VkDeviceSize padding;

	if ( !outValue ) {
		return qfalse;
	}
	if ( alignment == 0 ) {
		*outValue = value;
		return qtrue;
	}

	remainder = value % alignment;
	if ( remainder == 0 ) {
		*outValue = value;
		return qtrue;
	}

	padding = alignment - remainder;
	if ( value > ( (VkDeviceSize)-1 ) - padding ) {
		return qfalse;
	}

	*outValue = value + padding;
	return qtrue;
}

static qboolean vk_rt_is_power_of_two( VkDeviceSize value )
{
	return ( value != 0 && ( value & ( value - 1 ) ) == 0 ) ? qtrue : qfalse;
}

static qboolean vk_rt_wait_for_inflight_frames( const char *reason )
{
	VkFence fences[ NUM_COMMAND_BUFFERS ];
	uint32_t count = 0;
	uint32_t i;
	VkResult result;

	if ( !vk.active || vk.device == VK_NULL_HANDLE ) {
		return qtrue;
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.tess[i].waitForFence && vk.tess[i].rendering_finished_fence != VK_NULL_HANDLE ) {
			fences[count++] = vk.tess[i].rendering_finished_fence;
		}
	}

	if ( count == 0 ) {
		return qtrue;
	}

	result = qvkWaitForFences( vk.device, count, fences, VK_TRUE, 1000000000000ULL );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: wait-for-fences failed before %s: %s\n",
			reason ? reason : "resource rebuild", vk_result_string( result ) );
		return qfalse;
	}

	return qtrue;
}

static VkDeviceAddress vk_get_buffer_device_address( const VkBufferDeviceAddressInfo *addressInfo )
{
	if ( qvkGetBufferDeviceAddress ) {
		return qvkGetBufferDeviceAddress( vk.device, addressInfo );
	}
	if ( qvkGetBufferDeviceAddressKHR ) {
		return qvkGetBufferDeviceAddressKHR( vk.device, addressInfo );
	}
	return 0;
}

static qboolean vk_rt_create_buffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProps, qboolean needDeviceAddress, const char *debugName, rtxVkRtBuffer_t *outBuffer )
{
	return vk_rt_create_buffer_ex( size, usage, memoryProps, 0, needDeviceAddress, debugName, outBuffer );
}

/* memoryProps are required; preferredProps are taken as well where a memory
 * type offers them and its heap has room. */
static qboolean vk_rt_create_buffer_ex( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProps, VkMemoryPropertyFlags preferredProps, qboolean needDeviceAddress, const char *debugName, rtxVkRtBuffer_t *outBuffer )
{
	VkBufferCreateInfo bufferInfo;
	VkMemoryRequirements memoryRequirements;
	VkMemoryAllocateInfo allocInfo;
	VkMemoryAllocateFlagsInfo allocFlagsInfo;
	VkResult result;
	void *mapped = NULL;
	uint32_t memoryType;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;

	Com_Memset( outBuffer, 0, sizeof( *outBuffer ) );

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = NULL;
	bufferInfo.flags = 0;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.queueFamilyIndexCount = 0;
	bufferInfo.pQueueFamilyIndices = NULL;

	result = qvkCreateBuffer( vk.device, &bufferInfo, NULL, &buffer );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to create buffer '%s': %s\n",
			debugName ? debugName : "unnamed", vk_result_string( result ) );
		return qfalse;
	}

	qvkGetBufferMemoryRequirements( vk.device, buffer, &memoryRequirements );
	memoryType = ~0U;
	if ( preferredProps ) {
		memoryType = find_memory_type2( memoryRequirements.memoryTypeBits, memoryProps | preferredProps, NULL );
	}
	if ( memoryType == ~0U ) {
		memoryType = find_memory_type( memoryRequirements.memoryTypeBits, memoryProps );
	}

	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = memoryType;

	if ( needDeviceAddress ) {
		Com_Memset( &allocFlagsInfo, 0, sizeof( allocFlagsInfo ) );
		allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
		allocInfo.pNext = &allocFlagsInfo;
	}

	result = qvkAllocateMemory( vk.device, &allocInfo, NULL, &memory );
	if ( result != VK_SUCCESS && preferredProps ) {
		// the preferred heap is full (a small BAR window): take the required type
		const uint32_t requiredType = find_memory_type( memoryRequirements.memoryTypeBits, memoryProps );
		if ( requiredType != memoryType ) {
			allocInfo.memoryTypeIndex = requiredType;
			result = qvkAllocateMemory( vk.device, &allocInfo, NULL, &memory );
		}
	}
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to allocate buffer memory '%s': %s\n",
			debugName ? debugName : "unnamed", vk_result_string( result ) );
		qvkDestroyBuffer( vk.device, buffer, NULL );
		return qfalse;
	}

	result = qvkBindBufferMemory( vk.device, buffer, memory, 0 );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to bind buffer memory '%s': %s\n",
			debugName ? debugName : "unnamed", vk_result_string( result ) );
		qvkFreeMemory( vk.device, memory, NULL );
		qvkDestroyBuffer( vk.device, buffer, NULL );
		return qfalse;
	}

	if ( memoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) {
		result = qvkMapMemory( vk.device, memory, 0, VK_WHOLE_SIZE, 0, &mapped );
		if ( result != VK_SUCCESS ) {
			ri.Printf( PRINT_ERROR, "RTX RT: failed to map buffer '%s': %s\n",
				debugName ? debugName : "unnamed", vk_result_string( result ) );
			qvkFreeMemory( vk.device, memory, NULL );
			qvkDestroyBuffer( vk.device, buffer, NULL );
			return qfalse;
		}
	}

	outBuffer->buffer = buffer;
	outBuffer->memory = memory;
	outBuffer->capacity = size;
	outBuffer->allocationSize = memoryRequirements.size;
	outBuffer->address = 0;

	if ( needDeviceAddress ) {
		VkBufferDeviceAddressInfo addressInfo;
		addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addressInfo.pNext = NULL;
		addressInfo.buffer = buffer;
		outBuffer->address = vk_get_buffer_device_address( &addressInfo );
	}

	if ( mapped ) {
		qvkUnmapMemory( vk.device, memory );
	}

	if ( needDeviceAddress && outBuffer->address == 0 ) {
		ri.Printf( PRINT_ERROR, "RTX RT: buffer '%s' has no device address\n",
			debugName ? debugName : "unnamed" );
		qvkFreeMemory( vk.device, memory, NULL );
		qvkDestroyBuffer( vk.device, buffer, NULL );
		Com_Memset( outBuffer, 0, sizeof( *outBuffer ) );
		return qfalse;
	}
	if ( vk_caps_report_level() >= 2 ) {
		VkPhysicalDeviceMemoryProperties props;
		const uint32_t selected = allocInfo.memoryTypeIndex;
		qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &props );
		ri.Printf( PRINT_ALL, "RTX RT memory: %s, %llu bytes, type %u heap %u, flags 0x%x%s\n",
			debugName ? debugName : "unnamed", (unsigned long long)memoryRequirements.size,
			selected, props.memoryTypes[selected].heapIndex, props.memoryTypes[selected].propertyFlags,
			( props.memoryTypes[selected].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) ? " (GPU local)" : " (host)" );
	}

	return qtrue;
}

static void vk_rt_destroy_buffer( rtxVkRtBuffer_t *buffer )
{
	if ( !buffer ) {
		return;
	}

	if ( buffer->buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, buffer->buffer, NULL );
	}
	if ( buffer->memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, buffer->memory, NULL );
	}

	Com_Memset( buffer, 0, sizeof( *buffer ) );
}

static qboolean vk_rt_upload_buffer_data( const rtxVkRtBuffer_t *buffer, const void *src, size_t bytes )
{
	void *mapped = NULL;
	VkResult result;
	VkMappedMemoryRange range;
	VkDeviceSize uploadSize;
	VkDeviceSize atomSize = 1;
	VkDeviceSize flushSize;
	VkPhysicalDeviceProperties props;

	if ( !buffer || buffer->memory == VK_NULL_HANDLE || !src || bytes == 0 ) {
		return qfalse;
	}

	uploadSize = (VkDeviceSize)bytes;
	if ( (size_t)uploadSize != bytes ) {
		ri.Printf( PRINT_ERROR, "RTX RT: upload size is not representable by Vulkan\n" );
		return qfalse;
	}
	if ( uploadSize > buffer->capacity ) {
		ri.Printf( PRINT_ERROR, "RTX RT: upload size %llu exceeds buffer size %llu\n",
			(unsigned long long)uploadSize, (unsigned long long)buffer->capacity );
		return qfalse;
	}

	if ( qvkGetPhysicalDeviceProperties ) {
		qvkGetPhysicalDeviceProperties( vk.physical_device, &props );
		if ( props.limits.nonCoherentAtomSize > 0 ) {
			atomSize = props.limits.nonCoherentAtomSize;
		}
	}

	if ( !vk_rt_align_up_checked( uploadSize, atomSize, &flushSize ) ||
		flushSize > buffer->allocationSize ) {
		flushSize = buffer->allocationSize;
	}
	if ( flushSize < uploadSize ) {
		ri.Printf( PRINT_ERROR, "RTX RT: backing allocation is smaller than upload range\n" );
		return qfalse;
	}

	result = qvkMapMemory( vk.device, buffer->memory, 0, flushSize, 0, &mapped );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to map upload buffer: %s\n", vk_result_string( result ) );
		return qfalse;
	}

	Com_Memcpy( mapped, src, bytes );

	range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.pNext = NULL;
	range.memory = buffer->memory;
	range.offset = 0;
	range.size = flushSize;

	result = qvkFlushMappedMemoryRanges( vk.device, 1, &range );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to flush upload buffer: %s\n", vk_result_string( result ) );
		qvkUnmapMemory( vk.device, buffer->memory );
		return qfalse;
	}

	qvkUnmapMemory( vk.device, buffer->memory );
	return qtrue;
}

static void vk_rt_destroy_as( rtxVkRtAccelerationStructure_t *as )
{
	if ( !as ) {
		return;
	}

	if ( as->handle != VK_NULL_HANDLE ) {
		qvkDestroyAccelerationStructureKHR( vk.device, as->handle, NULL );
	}
	vk_rt_destroy_buffer( &as->storage );
	Com_Memset( as, 0, sizeof( *as ) );
}

static qboolean vk_rt_create_as( VkAccelerationStructureTypeKHR type, VkDeviceSize size, const char *debugName, rtxVkRtAccelerationStructure_t *outAs )
{
	VkAccelerationStructureCreateInfoKHR createInfo;
	VkResult result;

	Com_Memset( outAs, 0, sizeof( *outAs ) );

	if ( !vk_rt_create_buffer(
		size,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		qtrue,
		debugName,
		&outAs->storage ) ) {
		return qfalse;
	}

	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.createFlags = 0;
	createInfo.buffer = outAs->storage.buffer;
	createInfo.offset = 0;
	createInfo.size = size;
	createInfo.type = type;
	createInfo.deviceAddress = 0;

	result = qvkCreateAccelerationStructureKHR( vk.device, &createInfo, NULL, &outAs->handle );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "RTX RT: failed to create AS '%s': %s\n",
			debugName ? debugName : "unnamed", vk_result_string( result ) );
		vk_rt_destroy_buffer( &outAs->storage );
		return qfalse;
	}

	outAs->size = size;
	return qtrue;
}

/*
The path tracer's host arrays (the CPU geometry here, and those of rt/*.c)
come from the renderer's own C heap, not the engine's zone. The zone grows in
segments it never gives back, so a large map's world tree and light grid would
hold that much of a 32-bit client's 2 GB address space for the rest of the
session; the heap returns a large block to the system when it is freed.
*/
#define RT_HostAlloc( bytes ) malloc( (size_t)( bytes ) )
#define RT_HostFree( block ) free( block )

static void vk_rt_cpu_geometry_init( rtxRtCpuGeometry_t *geometry )
{
	Com_Memset( geometry, 0, sizeof( *geometry ) );
}

static void vk_rt_cpu_geometry_free( rtxRtCpuGeometry_t *geometry )
{
	if ( geometry->vertices ) {
		RT_HostFree( geometry->vertices );
	}
	if ( geometry->indices ) {
		RT_HostFree( geometry->indices );
	}
	if ( geometry->materials ) {
		RT_HostFree( geometry->materials );
	}
	if ( geometry->materialShaders ) {
		RT_HostFree( geometry->materialShaders );
	}
	Com_Memset( geometry, 0, sizeof( *geometry ) );
}

static qboolean vk_rt_cpu_array_growth( uint32_t currentCapacity, uint32_t requiredCount,
	uint32_t initialCapacity, size_t elementSize, const char *label,
	uint32_t *outCapacity, size_t *outBytes )
{
	const size_t allocatorMaxCount = elementSize ? (size_t)INT_MAX / elementSize : 0;
	const uint32_t maxCount = allocatorMaxCount > (size_t)UINT32_MAX ?
		UINT32_MAX : (uint32_t)allocatorMaxCount;
	uint32_t newCapacity;

	if ( !outCapacity || !outBytes || elementSize == 0 || requiredCount > maxCount ) {
		ri.Printf( PRINT_WARNING,
			"RTX RT: refusing oversized %s geometry allocation (%u elements)\n",
			label ? label : "CPU", requiredCount );
		return qfalse;
	}

	newCapacity = currentCapacity ? currentCapacity : initialCapacity;
	if ( newCapacity > maxCount ) {
		return qfalse;
	}
	while ( newCapacity < requiredCount ) {
		if ( newCapacity > maxCount / 2u ) {
			newCapacity = maxCount;
			break;
		}
		newCapacity *= 2u;
	}
	if ( newCapacity < requiredCount ) {
		return qfalse;
	}

	*outCapacity = newCapacity;
	*outBytes = (size_t)newCapacity * elementSize;
	return qtrue;
}

static qboolean vk_rt_checked_u32_multiply( uint32_t lhs, uint32_t rhs, uint32_t *outValue )
{
	if ( !outValue || ( lhs != 0 && rhs > UINT32_MAX / lhs ) ) {
		return qfalse;
	}
	*outValue = lhs * rhs;
	return qtrue;
}

static qboolean vk_rt_cpu_geometry_reserve( rtxRtCpuGeometry_t *geometry, uint32_t additionalVertices, uint32_t additionalIndices )
{
	rtxRtPackedVertex_t *newVertices = NULL;
	uint32_t *newIndices = NULL;
	uint32_t requiredVertices;
	uint32_t requiredIndices;
	uint32_t newVertexCapacity = 0;
	uint32_t newIndexCapacity = 0;
	size_t newVertexBytes = 0;
	size_t newIndexBytes = 0;

	if ( !geometry || additionalVertices > UINT32_MAX - geometry->numVertices ||
		additionalIndices > UINT32_MAX - geometry->numIndices ) {
		ri.Printf( PRINT_WARNING, "RTX RT: refusing overflowing CPU geometry counts\n" );
		return qfalse;
	}

	requiredVertices = geometry->numVertices + additionalVertices;
	requiredIndices = geometry->numIndices + additionalIndices;

	if ( requiredVertices > geometry->vertexCapacity &&
		!vk_rt_cpu_array_growth( geometry->vertexCapacity, requiredVertices, 1024u,
			sizeof( rtxRtPackedVertex_t ), "vertex", &newVertexCapacity, &newVertexBytes ) ) {
		return qfalse;
	}
	if ( requiredIndices > geometry->indexCapacity &&
		!vk_rt_cpu_array_growth( geometry->indexCapacity, requiredIndices, 2048u,
			sizeof( uint32_t ), "index", &newIndexCapacity, &newIndexBytes ) ) {
		return qfalse;
	}

	if ( newVertexBytes > 0 ) {
		newVertices = (rtxRtPackedVertex_t *)RT_HostAlloc( (int)newVertexBytes );
		if ( !newVertices ) {
			return qfalse;
		}
	}
	if ( newIndexBytes > 0 ) {
		newIndices = (uint32_t *)RT_HostAlloc( (int)newIndexBytes );
		if ( !newIndices ) {
			if ( newVertices ) {
				RT_HostFree( newVertices );
			}
			return qfalse;
		}
	}

	if ( newVertices ) {
		if ( geometry->vertices && geometry->numVertices > 0 ) {
			Com_Memcpy( newVertices, geometry->vertices,
				(size_t)geometry->numVertices * sizeof( rtxRtPackedVertex_t ) );
		}
		if ( geometry->vertices ) {
			RT_HostFree( geometry->vertices );
		}
		geometry->vertices = newVertices;
		geometry->vertexCapacity = newVertexCapacity;
	}
	if ( newIndices ) {
		if ( geometry->indices && geometry->numIndices > 0 ) {
			Com_Memcpy( newIndices, geometry->indices,
				(size_t)geometry->numIndices * sizeof( uint32_t ) );
		}
		if ( geometry->indices ) {
			RT_HostFree( geometry->indices );
		}
		geometry->indices = newIndices;
		geometry->indexCapacity = newIndexCapacity;
	}

	return qtrue;
}

static qboolean vk_rt_cpu_geometry_reserve_materials( rtxRtCpuGeometry_t *geometry, uint32_t additionalMaterials )
{
	rtxRtMaterial_t *newMaterials;
	const shader_t **newMaterialShaders;
	uint32_t requiredMaterials;
	uint32_t newCapacity;
	size_t allocationBytes;
	size_t materialBytes;
	size_t shaderBytes;
	const size_t maxElementSize = sizeof( rtxRtMaterial_t ) > sizeof( shader_t * ) ?
		sizeof( rtxRtMaterial_t ) : sizeof( shader_t * );

	if ( !geometry || additionalMaterials > UINT32_MAX - geometry->numMaterials ) {
		ri.Printf( PRINT_WARNING, "RTX RT: refusing overflowing material count\n" );
		return qfalse;
	}

	requiredMaterials = geometry->numMaterials + additionalMaterials;
	if ( requiredMaterials <= geometry->materialCapacity ) {
		return qtrue;
	}
	if ( !vk_rt_cpu_array_growth( geometry->materialCapacity, requiredMaterials, 128u,
		maxElementSize, "material", &newCapacity, &allocationBytes ) ) {
		return qfalse;
	}

	materialBytes = (size_t)newCapacity * sizeof( rtxRtMaterial_t );
	shaderBytes = (size_t)newCapacity * sizeof( shader_t * );
	newMaterials = (rtxRtMaterial_t *)RT_HostAlloc( (int)materialBytes );
	newMaterialShaders = (const shader_t **)RT_HostAlloc( (int)shaderBytes );
	if ( !newMaterials || !newMaterialShaders ) {
		if ( newMaterials ) {
			RT_HostFree( newMaterials );
		}
		if ( newMaterialShaders ) {
			RT_HostFree( newMaterialShaders );
		}
		return qfalse;
	}

	if ( geometry->materials && geometry->numMaterials > 0 ) {
		Com_Memcpy( newMaterials, geometry->materials,
			(size_t)geometry->numMaterials * sizeof( rtxRtMaterial_t ) );
		Com_Memcpy( newMaterialShaders, geometry->materialShaders,
			(size_t)geometry->numMaterials * sizeof( shader_t * ) );
	}
	if ( geometry->materials ) {
		RT_HostFree( geometry->materials );
	}
	if ( geometry->materialShaders ) {
		RT_HostFree( geometry->materialShaders );
	}

	geometry->materials = newMaterials;
	geometry->materialShaders = newMaterialShaders;
	geometry->materialCapacity = newCapacity;
	return qtrue;
}

static uint32_t vk_rt_color_u32_from_bytes( const byte *rgba )
{
	color4ub_t packed;
	packed.rgba[0] = rgba ? rgba[0] : 255;
	packed.rgba[1] = rgba ? rgba[1] : 255;
	packed.rgba[2] = rgba ? rgba[2] : 255;
	packed.rgba[3] = rgba ? rgba[3] : 255;
	return packed.u32;
}

static qboolean vk_rt_cpu_geometry_find_or_add_material_ex( rtxRtCpuGeometry_t *geometry, const shader_t *shader, uint32_t extraFlags, uint32_t *outMaterialIndex )
{
	uint32_t i;
	rtxRtMaterial_t material;
	const uint32_t ownershipFlags = RTX_RT_MATFLAG_RASTER_OVERLAY;

	for ( i = 0; i < geometry->numMaterials; i++ ) {
		if ( geometry->materialShaders[i] == shader &&
			( geometry->materials[i].flags & ownershipFlags ) ==
				( extraFlags & ownershipFlags ) ) {
			if ( outMaterialIndex ) {
				*outMaterialIndex = i;
			}
			return qtrue;
		}
	}

	if ( !vk_rt_cpu_geometry_reserve_materials( geometry, 1 ) ) {
		return qfalse;
	}

	Com_Memset( &material, 0, sizeof( material ) );
	material.flags = extraFlags;
	geometry->materials[ geometry->numMaterials ] = material;
	geometry->materialShaders[ geometry->numMaterials ] = shader;

	if ( outMaterialIndex ) {
		*outMaterialIndex = geometry->numMaterials;
	}

	geometry->numMaterials++;
	return qtrue;
}

static qboolean vk_rt_cpu_geometry_find_or_add_material( rtxRtCpuGeometry_t *geometry, const shader_t *shader, uint32_t *outMaterialIndex )
{
	return vk_rt_cpu_geometry_find_or_add_material_ex(
		geometry,
		shader,
		RTX_RT_MATFLAG_NONE,
		outMaterialIndex );
}

static qboolean vk_rt_cpu_geometry_add_vertex( rtxRtCpuGeometry_t *geometry, const vec3_t xyz, const vec3_t normal, const vec4_t tangent, const vec2_t texCoord, uint32_t color, uint32_t materialIndex, uint32_t *outIndex )
{
	rtxRtPackedVertex_t *vertex;

	if ( !vk_rt_cpu_geometry_reserve( geometry, 1, 0 ) ) {
		return qfalse;
	}

	vertex = &geometry->vertices[ geometry->numVertices ];
	vertex->xyz[0] = xyz[0];
	vertex->xyz[1] = xyz[1];
	vertex->xyz[2] = xyz[2];
	vertex->xyz[3] = 1.0f;
	vertex->normal[0] = normal[0];
	vertex->normal[1] = normal[1];
	vertex->normal[2] = normal[2];
	vertex->normal[3] = 0.0f;
	vertex->tangent[0] = tangent[0];
	vertex->tangent[1] = tangent[1];
	vertex->tangent[2] = tangent[2];
	vertex->tangent[3] = tangent[3];
	vertex->texCoord[0] = texCoord[0];
	vertex->texCoord[1] = texCoord[1];
	vertex->color = color;
	vertex->materialIndex = materialIndex;
	Com_Memset( vertex->lightmapData, 0, sizeof( vertex->lightmapData ) );

	if ( outIndex ) {
		*outIndex = geometry->numVertices;
	}

	geometry->numVertices++;
	return qtrue;
}

static qboolean vk_rt_cpu_geometry_add_index( rtxRtCpuGeometry_t *geometry, uint32_t index )
{
	if ( !vk_rt_cpu_geometry_reserve( geometry, 0, 1 ) ) {
		return qfalse;
	}

	geometry->indices[ geometry->numIndices ] = index;
	geometry->numIndices++;
	return qtrue;
}

static qboolean vk_rt_cpu_geometry_add_triangle( rtxRtCpuGeometry_t *geometry,
	uint32_t i0, uint32_t i1, uint32_t i2 )
{
	uint32_t firstIndex;

	if ( !vk_rt_cpu_geometry_reserve( geometry, 0, 3 ) ) {
		return qfalse;
	}

	firstIndex = geometry->numIndices;
	geometry->indices[firstIndex + 0] = i0;
	geometry->indices[firstIndex + 1] = i1;
	geometry->indices[firstIndex + 2] = i2;
	geometry->numIndices += 3;
	return qtrue;
}

static qboolean vk_rt_append_surface_face_geometry( rtxRtCpuGeometry_t *geometry, const srfSurfaceFace_t *surface, const shader_t *shader, uint32_t materialFlags )
{
	const int *indices;
	vec4_t tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
	uint32_t baseVertex;
	uint32_t materialIndex = 0;
	uint32_t validTriangleCount = 0;
	int completeIndexCount;
	int i;

	if ( !surface || surface->numPoints < 3 || surface->numIndices < 3 ) {
		return qtrue;
	}

	indices = (const int *)( (const byte *)surface + surface->ofsIndices );
	completeIndexCount = surface->numIndices - surface->numIndices % 3;
	for ( i = 0; i < completeIndexCount; i += 3 ) {
		if ( indices[i + 0] < 0 || indices[i + 0] >= surface->numPoints ||
			indices[i + 1] < 0 || indices[i + 1] >= surface->numPoints ||
			indices[i + 2] < 0 || indices[i + 2] >= surface->numPoints ) {
			continue;
		}
		validTriangleCount++;
	}

	if ( !vk_rt_cpu_geometry_find_or_add_material_ex( geometry, shader, materialFlags, &materialIndex ) ) {
		return qfalse;
	}

	if ( !vk_rt_cpu_geometry_reserve( geometry, (uint32_t)surface->numPoints, validTriangleCount * 3u ) ) {
		return qfalse;
	}

	baseVertex = geometry->numVertices;
	for ( i = 0; i < surface->numPoints; i++ ) {
		const float *p = surface->points[i];
		const vec3_t xyz = { p[0], p[1], p[2] };
		vec3_t normal;
		vec2_t texCoord;
		floatint_t packedColor;

		if ( surface->normals ) {
			// four floats a point (tr_bsp.c GenerateNormals, which smooths a
			// face whose points do not share a plane: Enemy Territory's terrain)
			VectorCopy( &surface->normals[i * 4], normal );
		} else {
			VectorCopy( surface->plane.normal, normal );
		}

		texCoord[0] = p[3];
		texCoord[1] = p[4];
		packedColor.f = p[7];

		if ( !vk_rt_cpu_geometry_add_vertex( geometry, xyz, normal, tangent, texCoord, packedColor.u, materialIndex, NULL ) ) {
			return qfalse;
		}
		geometry->vertices[baseVertex + i].lightmapData[0] = p[5];
		geometry->vertices[baseVertex + i].lightmapData[1] = p[6];
	}

	for ( i = 0; i < completeIndexCount; i += 3 ) {
		const int i0 = indices[i + 0];
		const int i1 = indices[i + 1];
		const int i2 = indices[i + 2];

		if ( i0 < 0 || i0 >= surface->numPoints ||
			i1 < 0 || i1 >= surface->numPoints ||
			i2 < 0 || i2 >= surface->numPoints ) {
			continue;
		}
		if ( !vk_rt_cpu_geometry_add_triangle( geometry,
			baseVertex + (uint32_t)i0,
			baseVertex + (uint32_t)i1,
			baseVertex + (uint32_t)i2 ) ) {
			return qfalse;
		}
	}

	geometry->sourceTriangleCount += validTriangleCount;
	if ( geometry->materials[ materialIndex ].flags & RTX_RT_MATFLAG_MASKED ) {
		geometry->maskedTriangleCount += validTriangleCount;
	}
	if ( geometry->materials[ materialIndex ].flags &
		( RTX_RT_MATFLAG_MASKED |
			RTX_RT_MATFLAG_CULL_FRONT |
			RTX_RT_MATFLAG_CULL_BACK |
			RTX_RT_MATFLAG_RASTER_OVERLAY ) ) {
		geometry->anyHitTriangleCount += validTriangleCount;
	}

	return qtrue;
}

static qboolean vk_rt_append_surface_grid_geometry( rtxRtCpuGeometry_t *geometry, const srfGridMesh_t *surface, const shader_t *shader, uint32_t materialFlags )
{
	int row;
	int col;
	vec4_t tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
	uint32_t baseVertex;
	uint32_t materialIndex = 0;
	uint32_t vertexCount;
	uint32_t cellCount;
	uint32_t indexCount;
	uint32_t triCount;

	if ( !surface || surface->width < 2 || surface->height < 2 ) {
		return qtrue;
	}
	if ( !vk_rt_checked_u32_multiply( (uint32_t)surface->width,
		(uint32_t)surface->height, &vertexCount ) ||
		!vk_rt_checked_u32_multiply( (uint32_t)( surface->width - 1 ),
			(uint32_t)( surface->height - 1 ), &cellCount ) ||
		!vk_rt_checked_u32_multiply( cellCount, 6u, &indexCount ) ||
		!vk_rt_checked_u32_multiply( cellCount, 2u, &triCount ) ) {
		ri.Printf( PRINT_WARNING, "RTX RT: refusing overflowing grid geometry\n" );
		return qfalse;
	}

	if ( !vk_rt_cpu_geometry_find_or_add_material_ex( geometry, shader, materialFlags, &materialIndex ) ) {
		return qfalse;
	}

	if ( !vk_rt_cpu_geometry_reserve(
		geometry,
		vertexCount,
		indexCount ) ) {
		return qfalse;
	}

	baseVertex = geometry->numVertices;
	for ( row = 0; row < surface->height; row++ ) {
		for ( col = 0; col < surface->width; col++ ) {
			const drawVert_t *vertex = &surface->verts[ row * surface->width + col ];
			if ( !vk_rt_cpu_geometry_add_vertex( geometry, vertex->xyz, vertex->normal, tangent, vertex->st, vertex->color.u32, materialIndex, NULL ) ) {
				return qfalse;
			}
			geometry->vertices[geometry->numVertices - 1].lightmapData[0] = vertex->lightmap[0];
			geometry->vertices[geometry->numVertices - 1].lightmapData[1] = vertex->lightmap[1];
		}
	}

	for ( row = 0; row < surface->height - 1; row++ ) {
		for ( col = 0; col < surface->width - 1; col++ ) {
			uint32_t i0 = baseVertex + (uint32_t)( row * surface->width + col );
			uint32_t i1 = baseVertex + (uint32_t)( row * surface->width + col + 1 );
			uint32_t i2 = baseVertex + (uint32_t)( ( row + 1 ) * surface->width + col );
			uint32_t i3 = baseVertex + (uint32_t)( ( row + 1 ) * surface->width + col + 1 );

			if ( !vk_rt_cpu_geometry_add_index( geometry, i0 ) ||
				!vk_rt_cpu_geometry_add_index( geometry, i2 ) ||
				!vk_rt_cpu_geometry_add_index( geometry, i1 ) ||
				!vk_rt_cpu_geometry_add_index( geometry, i1 ) ||
				!vk_rt_cpu_geometry_add_index( geometry, i2 ) ||
				!vk_rt_cpu_geometry_add_index( geometry, i3 ) ) {
				return qfalse;
			}
		}
	}

	geometry->sourceTriangleCount += triCount;
	if ( geometry->materials[ materialIndex ].flags & RTX_RT_MATFLAG_MASKED ) {
		geometry->maskedTriangleCount += triCount;
	}
	if ( geometry->materials[ materialIndex ].flags &
		( RTX_RT_MATFLAG_MASKED |
			RTX_RT_MATFLAG_CULL_FRONT |
			RTX_RT_MATFLAG_CULL_BACK |
			RTX_RT_MATFLAG_RASTER_OVERLAY ) ) {
		geometry->anyHitTriangleCount += triCount;
	}

	return qtrue;
}

static qboolean vk_rt_append_surface_triangles_geometry( rtxRtCpuGeometry_t *geometry, const srfTriangles_t *surface, const shader_t *shader, uint32_t materialFlags )
{
	uint32_t baseVertex;
	uint32_t materialIndex = 0;
	uint32_t validTriangleCount = 0;
	vec4_t tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
	int completeIndexCount;
	int i;

	if ( !surface || surface->numVerts < 3 || surface->numIndexes < 3 ) {
		return qtrue;
	}

	completeIndexCount = surface->numIndexes - surface->numIndexes % 3;
	for ( i = 0; i < completeIndexCount; i += 3 ) {
		if ( surface->indexes[i + 0] < 0 || surface->indexes[i + 0] >= surface->numVerts ||
			surface->indexes[i + 1] < 0 || surface->indexes[i + 1] >= surface->numVerts ||
			surface->indexes[i + 2] < 0 || surface->indexes[i + 2] >= surface->numVerts ) {
			continue;
		}
		validTriangleCount++;
	}

	if ( !vk_rt_cpu_geometry_find_or_add_material_ex( geometry, shader, materialFlags, &materialIndex ) ) {
		return qfalse;
	}

	if ( !vk_rt_cpu_geometry_reserve( geometry, (uint32_t)surface->numVerts, validTriangleCount * 3u ) ) {
		return qfalse;
	}

	baseVertex = geometry->numVertices;
	for ( i = 0; i < surface->numVerts; i++ ) {
		if ( !vk_rt_cpu_geometry_add_vertex( geometry, surface->verts[i].xyz, surface->verts[i].normal, tangent, surface->verts[i].st, surface->verts[i].color.u32, materialIndex, NULL ) ) {
			return qfalse;
		}
		geometry->vertices[baseVertex + i].lightmapData[0] = surface->verts[i].lightmap[0];
		geometry->vertices[baseVertex + i].lightmapData[1] = surface->verts[i].lightmap[1];
	}

	for ( i = 0; i < completeIndexCount; i += 3 ) {
		const int i0 = surface->indexes[i + 0];
		const int i1 = surface->indexes[i + 1];
		const int i2 = surface->indexes[i + 2];

		if ( i0 < 0 || i0 >= surface->numVerts ||
			i1 < 0 || i1 >= surface->numVerts ||
			i2 < 0 || i2 >= surface->numVerts ) {
			continue;
		}
		if ( !vk_rt_cpu_geometry_add_triangle( geometry,
			baseVertex + (uint32_t)i0,
			baseVertex + (uint32_t)i1,
			baseVertex + (uint32_t)i2 ) ) {
			return qfalse;
		}
	}

	geometry->sourceTriangleCount += validTriangleCount;
	if ( geometry->materials[ materialIndex ].flags & RTX_RT_MATFLAG_MASKED ) {
		geometry->maskedTriangleCount += validTriangleCount;
	}
	if ( geometry->materials[ materialIndex ].flags &
		( RTX_RT_MATFLAG_MASKED |
			RTX_RT_MATFLAG_CULL_FRONT |
			RTX_RT_MATFLAG_CULL_BACK |
			RTX_RT_MATFLAG_RASTER_OVERLAY ) ) {
		geometry->anyHitTriangleCount += validTriangleCount;
	}

	return qtrue;
}

static void vk_rt_transform_point( const trRefEntity_t *ent, const vec3_t local, vec3_t world )
{
	const vec3_t *axis = ( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active &&
		( ent->e.q3IHV.flags & Q3IHV_ENTITY_FACE_VIEW ) ) ? backEnd.refdef.viewaxis : ent->e.axis;
	VectorCopy( ent->e.origin, world );
	VectorMA( world, local[0], axis[0], world );
	VectorMA( world, local[1], axis[1], world );
	VectorMA( world, local[2], axis[2], world );
}

static void vk_rt_transform_direction( const trRefEntity_t *ent, const vec3_t local, vec3_t world )
{
	const vec3_t *axis = ( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active &&
		( ent->e.q3IHV.flags & Q3IHV_ENTITY_FACE_VIEW ) ) ? backEnd.refdef.viewaxis : ent->e.axis;
	world[0] = local[0] * axis[0][0] + local[1] * axis[1][0] + local[2] * axis[2][0];
	world[1] = local[0] * axis[0][1] + local[1] * axis[1][1] + local[2] * axis[2][1];
	world[2] = local[0] * axis[0][2] + local[1] * axis[1][2] + local[2] * axis[2][2];
	if ( VectorNormalize( world ) == 0.0f ) {
		world[0] = 0.0f;
		world[1] = 0.0f;
		world[2] = 1.0f;
	}
}

static void vk_rt_transform_geometry_range( rtxRtCpuGeometry_t *geometry, uint32_t firstVertex, const trRefEntity_t *ent )
{
	uint32_t v;

	if ( !geometry || !ent || firstVertex >= geometry->numVertices ) {
		return;
	}

	for ( v = firstVertex; v < geometry->numVertices; v++ ) {
		rtxRtPackedVertex_t *vertex = &geometry->vertices[v];
		vec3_t localPos = { vertex->xyz[0], vertex->xyz[1], vertex->xyz[2] };
		vec3_t localNormal = { vertex->normal[0], vertex->normal[1], vertex->normal[2] };
		vec3_t localTangent = { vertex->tangent[0], vertex->tangent[1], vertex->tangent[2] };
		vec3_t worldPos;
		vec3_t worldNormal;
		vec3_t worldTangent;

		vk_rt_transform_point( ent, localPos, worldPos );
		vk_rt_transform_direction( ent, localNormal, worldNormal );
		vk_rt_transform_direction( ent, localTangent, worldTangent );

		vertex->xyz[0] = worldPos[0];
		vertex->xyz[1] = worldPos[1];
		vertex->xyz[2] = worldPos[2];
		vertex->normal[0] = worldNormal[0];
		vertex->normal[1] = worldNormal[1];
		vertex->normal[2] = worldNormal[2];
		vertex->tangent[0] = worldTangent[0];
		vertex->tangent[1] = worldTangent[1];
		vertex->tangent[2] = worldTangent[2];
	}
}

static qboolean vk_rt_append_brush_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent, const model_t *model )
{
	const bmodel_t *bmodel;
	int s;

	if ( !geometry || !ent || !model || model->type != MOD_BRUSH || !model->bmodel ) {
		return qtrue;
	}

	bmodel = model->bmodel;
	for ( s = 0; s < bmodel->numSurfaces; s++ ) {
		const msurface_t *surface = &bmodel->firstSurface[s];
		const shader_t *shader = ( surface && surface->shader ) ? surface->shader : tr.defaultShader;
		uint32_t firstVertex = geometry->numVertices;

		if ( !surface || !surface->data ) {
			continue;
		}

		switch ( *surface->data ) {
		case SF_FACE:
			if ( !vk_rt_append_surface_face_geometry(
				geometry,
				(const srfSurfaceFace_t *)surface->data,
				shader,
				RTX_RT_MATFLAG_RASTER_OVERLAY ) ) {
				return qfalse;
			}
			break;
		case SF_GRID:
			if ( !vk_rt_append_surface_grid_geometry(
				geometry,
				(const srfGridMesh_t *)surface->data,
				shader,
				RTX_RT_MATFLAG_RASTER_OVERLAY ) ) {
				return qfalse;
			}
			break;
		case SF_TRIANGLES:
			if ( !vk_rt_append_surface_triangles_geometry(
				geometry,
				(const srfTriangles_t *)surface->data,
				shader,
				RTX_RT_MATFLAG_RASTER_OVERLAY ) ) {
				return qfalse;
			}
			break;
		default:
			break;
		}

		vk_rt_transform_geometry_range( geometry, firstVertex, ent );
	}

	return qtrue;
}

static void vk_rt_decode_md3_normal( short packedNormal, vec3_t outNormal )
{
	if ( ri.gameId == COM_GAME_RTCW_SP || ri.gameId == COM_GAME_RTCW_MP ) {
		fnMdcVertex_t vertex = { 0 };
		vertex.normal = (uint16_t)packedNormal;
		FnMdc_DecodeNormal( &r_mdcNormals, &vertex, outNormal );
		return;
	}

	const uint8_t lat = (uint8_t)( ( packedNormal >> 8 ) & 0xFF );
	const uint8_t lng = (uint8_t)( packedNormal & 0xFF );
	const float latRad = (float)lat * ( 2.0f * (float)M_PI / 255.0f );
	const float lngRad = (float)lng * ( 2.0f * (float)M_PI / 255.0f );
	outNormal[0] = cosf( latRad ) * sinf( lngRad );
	outNormal[1] = sinf( latRad ) * sinf( lngRad );
	outNormal[2] = cosf( lngRad );
}

static const shader_t *vk_rt_resolve_md3_shader( const trRefEntity_t *ent, const md3Surface_t *surface, int ordinal )
{
	const shader_t *shader = tr.defaultShader;

	if ( ent->e.customShader ) {
		shader = R_GetShaderByHandle( ent->e.customShader );
	} else if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) {
		const skin_t *skin = R_GetSkinByHandle( ent->e.customSkin );
		shader = R_MeshSkinShaderAt( &ent->e, skin, surface->name, ordinal );
	} else if ( surface->numShaders > 0 ) {
		const md3Shader_t *md3Shader = (const md3Shader_t *)( (const byte *)surface + surface->ofsShaders );
		const int shaderIndex = ent->e.skinNum < 0 ? 0 : ent->e.skinNum % surface->numShaders;
		md3Shader += shaderIndex;
		if ( md3Shader->shaderIndex >= 0 && md3Shader->shaderIndex < tr.numShaders ) {
			shader = tr.shaders[ md3Shader->shaderIndex ];
		}
	}

	return shader ? shader : tr.defaultShader;
}

static const shader_t *vk_rt_resolve_iqm_shader( const trRefEntity_t *ent, const srfIQModel_t *surface )
{
	const shader_t *shader = tr.defaultShader;

	if ( ent->e.customShader ) {
		shader = R_GetShaderByHandle( ent->e.customShader );
	} else if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) {
		const skin_t *skin = R_GetSkinByHandle( ent->e.customSkin );
		int i;
		for ( i = 0; i < skin->numSurfaces; i++ ) {
			if ( !strcmp( skin->surfaces[i].name, surface->name ) ) {
				shader = skin->surfaces[i].shader;
				break;
			}
		}
	} else if ( surface->shader ) {
		shader = surface->shader;
	}

	return shader ? shader : tr.defaultShader;
}


static const shader_t *vk_rt_resolve_mdr_shader( const trRefEntity_t *ent, const mdrSurface_t *surface )
{
	int i;
	if ( ent->e.customShader ) {
		return R_GetShaderByHandle( ent->e.customShader );
	}
	if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) {
		const skin_t *skin = R_GetSkinByHandle( ent->e.customSkin );
		if ( skin ) {
			for ( i = 0; i < skin->numSurfaces; i++ ) {
				if ( !strcmp( skin->surfaces[i].name, surface->name ) ) {
					return skin->surfaces[i].shader;
				}
			}
		}
		return tr.defaultShader;
	}
	return surface->shaderIndex > 0 ? R_GetShaderByHandle( surface->shaderIndex ) : tr.defaultShader;
}

static qboolean vk_rt_append_mdr_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent )
{
	const model_t *model = R_GetModelByHandle( ent->e.hModel );
	const mdrHeader_t *header;
	const mdrLOD_t *lod;
	mdrBone_t bones[MDR_MAX_BONES];
	size_t surfaceOffset;
	int i;

	if ( !model || model->type != MOD_MDR || !model->modelData ) {
		return qtrue;
	}
	header = (const mdrHeader_t *)model->modelData;
	if ( model->dataSize <= 0 ||
		!RTX_MDRPreparePose( header, (size_t)model->dataSize, ent->e.frame, ent->e.oldframe,
			( ent->e.renderfx & RF_WRAP_FRAMES ) ? qtrue : qfalse, ent->e.backlerp, bones ) ) {
		ri.Printf( PRINT_WARNING, "RTX RT: refusing malformed MDR pose for '%s'\n", model->name );
		return qfalse;
	}
	/* Secondary-ray silhouettes must not depend on the main camera's LOD or
	 * frontend tr.currentModel. Raster shadow-caster passes also use LOD zero. */
	lod = RTX_MDRGetLOD( header, 0 );
	if ( !lod ) {
		ri.Printf( PRINT_WARNING, "RTX RT: refusing malformed MDR LOD for '%s'\n", model->name );
		return qfalse;
	}
	surfaceOffset = (size_t)lod->ofsSurfaces;
	for ( i = 0; i < lod->numSurfaces; i++ ) {
		const mdrSurface_t *surface;
		const mdrVertex_t *vertex;
		const mdrTriangle_t *triangles;
		uint32_t materialIndex, baseVertex, surfaceIndexCount;
		uint32_t validTriangleCount = 0;
		vec4_t tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
		int v, t;

		if ( surfaceOffset > (size_t)lod->ofsEnd || sizeof( *surface ) > (size_t)lod->ofsEnd - surfaceOffset ) {
			return qfalse;
		}
		surface = (const mdrSurface_t *)( (const byte *)lod + surfaceOffset );
		if ( !RTX_MDRValidateSurface( surface, (size_t)lod->ofsEnd - surfaceOffset, header->numBones ) ||
			!vk_rt_checked_u32_multiply( (uint32_t)surface->numTriangles, 3u, &surfaceIndexCount ) ) {
			ri.Printf( PRINT_WARNING, "RTX RT: refusing malformed MDR surface for '%s'\n", model->name );
			return qfalse;
		}
		if ( !vk_rt_cpu_geometry_find_or_add_material( geometry, vk_rt_resolve_mdr_shader( ent, surface ), &materialIndex ) ||
			!vk_rt_cpu_geometry_reserve( geometry, (uint32_t)surface->numVerts, surfaceIndexCount ) ) {
			return qfalse;
		}
		baseVertex = geometry->numVertices;
		vertex = (const mdrVertex_t *)( (const byte *)surface + surface->ofsVerts );
		for ( v = 0; v < surface->numVerts; v++ ) {
			vec3_t position, normal, worldPosition, worldNormal;
			if ( !RTX_MDRSkinVertex( vertex, bones, position, normal ) ) {
				return qfalse;
			}
			vk_rt_transform_point( ent, position, worldPosition );
			vk_rt_transform_direction( ent, normal, worldNormal );
			if ( !vk_rt_cpu_geometry_add_vertex( geometry, worldPosition, worldNormal, tangent,
				vertex->texCoords, 0xFFFFFFFFu, materialIndex, NULL ) ) {
				return qfalse;
			}
			vertex = (const mdrVertex_t *)&vertex->weights[vertex->numWeights];
		}
		triangles = (const mdrTriangle_t *)( (const byte *)surface + surface->ofsTriangles );
		for ( t = 0; t < surface->numTriangles; t++ ) {
			int i0 = triangles[t].indexes[0], i1 = triangles[t].indexes[1], i2 = triangles[t].indexes[2];
			if ( i0 < 0 || i0 >= surface->numVerts || i1 < 0 || i1 >= surface->numVerts ||
				i2 < 0 || i2 >= surface->numVerts ) {
				continue;
			}
			if ( !vk_rt_cpu_geometry_add_triangle( geometry, baseVertex + (uint32_t)i0,
				baseVertex + (uint32_t)i1, baseVertex + (uint32_t)i2 ) ) {
				return qfalse;
			}
			validTriangleCount++;
		}
		geometry->sourceTriangleCount += validTriangleCount;
		if ( geometry->materials[materialIndex].flags & RTX_RT_MATFLAG_MASKED ) {
			geometry->maskedTriangleCount += validTriangleCount;
		}
		surfaceOffset += (size_t)surface->ofsEnd;
	}
	geometry->sourceEntityCount++;
	return qtrue;
}

static qboolean vk_rt_append_md3_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent )
{
	const model_t *model = R_GetModelByHandle( ent->e.hModel );
	const md3Header_t *header;
	int frame;
	int oldFrame;
	float backlerp;
	vec3_t ihvMove = { 0, 0, 0 };
	vec3_t ihvXYZ[SHADER_MAX_VERTEXES], ihvNormal[SHADER_MAX_VERTEXES];
	const qboolean ihv = ( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active ) ? qtrue : qfalse;
	const md3Surface_t *surface;
	int i;

	if ( !model || model->type != MOD_MESH ) {
		return qtrue;
	}

	/* Match shadow-caster geometry, including models outside the camera frustum.
	 * R_ComputeLOD uses frontend globals and cannot be called from this pass. */
	header = model->md3[0];
	if ( !header || header->numSurfaces <= 0 ) {
		return qtrue;
	}

	frame = ent->e.frame;
	oldFrame = ent->e.oldframe;
	if ( ihv && ( frame < 0 || oldFrame < 0 || frame >= header->numFrames || oldFrame >= header->numFrames ) ) {
		frame = oldFrame = 0;
	} else if ( header->numFrames > 0 ) {
		if ( frame < 0 ) {
			frame = 0;
		}
		if ( oldFrame < 0 ) {
			oldFrame = 0;
		}
		frame %= header->numFrames;
		oldFrame %= header->numFrames;
	}

	backlerp = ( oldFrame == frame ) ? 0.0f : ent->e.backlerp;
	if ( !ihv ) backlerp = Com_Clamp( 0.0f, 1.0f, backlerp );
	else {
		const vec3_t *axis = ( ent->e.q3IHV.flags & Q3IHV_ENTITY_FACE_VIEW ) ? backEnd.refdef.viewaxis : ent->e.axis;
		backlerp = Q3IHV_ModelFraction( frame, oldFrame, ent->e.backlerp, r_lerpmodels ? &r_lerpmodels->value : NULL );
		if ( !Q3IHV_EntityOriginLerp( ent->e.q3IHV.flags, ent->e.origin, ent->e.oldorigin, axis, backlerp, ihvMove ) ) return qfalse;
	}

	surface = (const md3Surface_t *)( (const byte *)header + header->ofsSurfaces );
	for ( i = 0; i < header->numSurfaces; i++ ) {
		const shader_t *shader = vk_rt_resolve_md3_shader( ent, surface, i );
		uint32_t materialIndex = 0;
		const md3XyzNormal_t *newVerts;
		const md3XyzNormal_t *oldVerts;
		const float *texCoords;
		const int *triangles;
		uint32_t baseVertex;
		uint32_t surfaceIndexCount;
		uint32_t validTriangleCount = 0;
		int v;
		int t;
		vec4_t tangent = { 1.0f, 0.0f, 0.0f, 1.0f };

		if ( R_MeshSurfaceHidden( &ent->e, surface->name ) ) {
			surface = (const md3Surface_t *)( (const byte *)surface + surface->ofsEnd );
			continue;
		}

		/* Stage native animation before this surface publishes any ray vertices. */
		if ( ihv && !Q3IHV_ModelSurface( surface, oldFrame, frame, backlerp, ihvMove,
			tr.ihvModelSines, ihvXYZ, ihvNormal, SHADER_MAX_VERTEXES ) ) return qfalse;

		if ( !vk_rt_cpu_geometry_find_or_add_material( geometry, shader, &materialIndex ) ) {
			return qfalse;
		}

		if ( surface->numVerts < 0 || surface->numTriangles < 0 ||
			!vk_rt_checked_u32_multiply( (uint32_t)surface->numTriangles, 3u,
				&surfaceIndexCount ) ) {
			ri.Printf( PRINT_WARNING, "RTX RT: refusing malformed MD3 geometry\n" );
			return qfalse;
		}
		if ( !vk_rt_cpu_geometry_reserve( geometry, (uint32_t)surface->numVerts, surfaceIndexCount ) ) {
			return qfalse;
		}

		newVerts = (const md3XyzNormal_t *)( (const byte *)surface + surface->ofsXyzNormals ) + frame * surface->numVerts;
		oldVerts = (const md3XyzNormal_t *)( (const byte *)surface + surface->ofsXyzNormals ) + oldFrame * surface->numVerts;
		texCoords = (const float *)( (const byte *)surface + surface->ofsSt );
		baseVertex = geometry->numVertices;

		for ( v = 0; v < surface->numVerts; v++ ) {
			vec3_t localPos;
			vec3_t worldPos;
			vec3_t normalNew;
			vec3_t normalOld;
			vec3_t normalLocal;
			vec3_t normalWorld;
			vec2_t uv;
			const float invScale = (float)MD3_XYZ_SCALE;

			if ( ihv ) {
				VectorCopy( ihvXYZ[v], localPos );
				VectorCopy( ihvNormal[v], normalLocal );
			} else {
				localPos[0] = newVerts[v].xyz[0] * invScale;
				localPos[1] = newVerts[v].xyz[1] * invScale;
				localPos[2] = newVerts[v].xyz[2] * invScale;

				vk_rt_decode_md3_normal( newVerts[v].normal, normalNew );

				if ( backlerp > 0.0f ) {
					vec3_t oldPos;
					const float frontlerp = 1.0f - backlerp;
					oldPos[0] = oldVerts[v].xyz[0] * invScale;
					oldPos[1] = oldVerts[v].xyz[1] * invScale;
					oldPos[2] = oldVerts[v].xyz[2] * invScale;
					vk_rt_decode_md3_normal( oldVerts[v].normal, normalOld );

					localPos[0] = localPos[0] * frontlerp + oldPos[0] * backlerp;
					localPos[1] = localPos[1] * frontlerp + oldPos[1] * backlerp;
					localPos[2] = localPos[2] * frontlerp + oldPos[2] * backlerp;
					normalLocal[0] = normalNew[0] * frontlerp + normalOld[0] * backlerp;
					normalLocal[1] = normalNew[1] * frontlerp + normalOld[1] * backlerp;
					normalLocal[2] = normalNew[2] * frontlerp + normalOld[2] * backlerp;
					VectorNormalize( normalLocal );
				} else {
					VectorCopy( normalNew, normalLocal );
				}
			}
			vk_rt_transform_point( ent, localPos, worldPos );
			vk_rt_transform_direction( ent, normalLocal, normalWorld );

			uv[0] = texCoords[v * 2 + 0];
			uv[1] = texCoords[v * 2 + 1];

			if ( !vk_rt_cpu_geometry_add_vertex( geometry, worldPos, normalWorld, tangent, uv, 0xFFFFFFFFu, materialIndex, NULL ) ) {
				return qfalse;
			}
		}

		triangles = (const int *)( (const byte *)surface + surface->ofsTriangles );
		for ( t = 0; t < surface->numTriangles; t++ ) {
			const int i0 = triangles[t * 3 + 0];
			const int i1 = triangles[t * 3 + 1];
			const int i2 = triangles[t * 3 + 2];
			if ( i0 < 0 || i0 >= surface->numVerts ||
				i1 < 0 || i1 >= surface->numVerts ||
				i2 < 0 || i2 >= surface->numVerts ) {
				continue;
			}
			if ( !vk_rt_cpu_geometry_add_triangle( geometry,
				baseVertex + (uint32_t)i0,
				baseVertex + (uint32_t)i1,
				baseVertex + (uint32_t)i2 ) ) {
				return qfalse;
			}
			validTriangleCount++;
		}

		geometry->sourceTriangleCount += validTriangleCount;
		if ( geometry->materials[ materialIndex ].flags & RTX_RT_MATFLAG_MASKED ) {
			geometry->maskedTriangleCount += validTriangleCount;
		}
		geometry->sourceEntityCount++;

		surface = (const md3Surface_t *)( (const byte *)surface + surface->ofsEnd );
	}

	return qtrue;
}

#include "../renderercommon/tr_mdc_ray_geometry.h"
#include "../renderercommon/tr_mds_ray_geometry.h"
#include "../renderercommon/tr_wolfet_ray_geometry.h"
#include "../renderercommon/tr_mohaa_ray_geometry.h"

static void vk_rt_iqm_matrix34_multiply( const float *a, const float *b, float *out )
{
	out[ 0] = a[0] * b[0] + a[1] * b[4] + a[ 2] * b[ 8];
	out[ 1] = a[0] * b[1] + a[1] * b[5] + a[ 2] * b[ 9];
	out[ 2] = a[0] * b[2] + a[1] * b[6] + a[ 2] * b[10];
	out[ 3] = a[0] * b[3] + a[1] * b[7] + a[ 2] * b[11] + a[ 3];
	out[ 4] = a[4] * b[0] + a[5] * b[4] + a[ 6] * b[ 8];
	out[ 5] = a[4] * b[1] + a[5] * b[5] + a[ 6] * b[ 9];
	out[ 6] = a[4] * b[2] + a[5] * b[6] + a[ 6] * b[10];
	out[ 7] = a[4] * b[3] + a[5] * b[7] + a[ 6] * b[11] + a[ 7];
	out[ 8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[ 8];
	out[ 9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[ 9];
	out[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10];
	out[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11];
}

static void vk_rt_iqm_joint_to_matrix( const quat_t rot, const vec3_t scale, const vec3_t trans, float *mat )
{
	float xx = 2.0f * rot[0] * rot[0];
	float yy = 2.0f * rot[1] * rot[1];
	float zz = 2.0f * rot[2] * rot[2];
	float xy = 2.0f * rot[0] * rot[1];
	float xz = 2.0f * rot[0] * rot[2];
	float yz = 2.0f * rot[1] * rot[2];
	float wx = 2.0f * rot[3] * rot[0];
	float wy = 2.0f * rot[3] * rot[1];
	float wz = 2.0f * rot[3] * rot[2];

	mat[ 0] = scale[0] * (1.0f - (yy + zz));
	mat[ 1] = scale[0] * (xy - wz);
	mat[ 2] = scale[0] * (xz + wy);
	mat[ 3] = trans[0];
	mat[ 4] = scale[1] * (xy + wz);
	mat[ 5] = scale[1] * (1.0f - (xx + zz));
	mat[ 6] = scale[1] * (yz - wx);
	mat[ 7] = trans[1];
	mat[ 8] = scale[2] * (xz - wy);
	mat[ 9] = scale[2] * (yz + wx);
	mat[10] = scale[2] * (1.0f - (xx + yy));
	mat[11] = trans[2];
}

static void vk_rt_iqm_quat_slerp( const quat_t from, const quat_t toIn, float fraction, quat_t out )
{
	float angle;
	float cosAngle;
	float sinAngle;
	float backlerp;
	float lerp;
	quat_t to;

	cosAngle = from[0] * toIn[0] + from[1] * toIn[1] + from[2] * toIn[2] + from[3] * toIn[3];
	if ( cosAngle < 0.0f ) {
		cosAngle = -cosAngle;
		to[0] = -toIn[0];
		to[1] = -toIn[1];
		to[2] = -toIn[2];
		to[3] = -toIn[3];
	} else {
		QuatCopy( toIn, to );
	}

	if ( cosAngle < 0.999999f ) {
		angle = acosf( cosAngle );
		sinAngle = sinf( angle );
		backlerp = sinf( ( 1.0f - fraction ) * angle ) / sinAngle;
		lerp = sinf( fraction * angle ) / sinAngle;
	} else {
		backlerp = 1.0f - fraction;
		lerp = fraction;
	}

	out[0] = from[0] * backlerp + to[0] * lerp;
	out[1] = from[1] * backlerp + to[1] * lerp;
	out[2] = from[2] * backlerp + to[2] * lerp;
	out[3] = from[3] * backlerp + to[3] * lerp;
}

static void vk_rt_iqm_compute_pose_mats( const iqmData_t *data, int frame, int oldframe, float backlerp, float *poseMats )
{
	iqmTransform_t relativeJoints[IQM_MAX_JOINTS];
	iqmTransform_t *relativeJoint;
	const iqmTransform_t *pose;
	const iqmTransform_t *oldPose;
	const int *jointParent;
	const float *invBindMat;
	float *poseMat;
	float lerp;
	int i;

	relativeJoint = relativeJoints;

	if ( oldframe == frame ) {
		pose = &data->poses[frame * data->num_poses];
		for ( i = 0; i < data->num_poses; i++, pose++, relativeJoint++ ) {
			VectorCopy( pose->translate, relativeJoint->translate );
			QuatCopy( pose->rotate, relativeJoint->rotate );
			VectorCopy( pose->scale, relativeJoint->scale );
		}
	} else {
		lerp = 1.0f - backlerp;
		pose = &data->poses[frame * data->num_poses];
		oldPose = &data->poses[oldframe * data->num_poses];
		for ( i = 0; i < data->num_poses; i++, oldPose++, pose++, relativeJoint++ ) {
			relativeJoint->translate[0] = oldPose->translate[0] * backlerp + pose->translate[0] * lerp;
			relativeJoint->translate[1] = oldPose->translate[1] * backlerp + pose->translate[1] * lerp;
			relativeJoint->translate[2] = oldPose->translate[2] * backlerp + pose->translate[2] * lerp;

			relativeJoint->scale[0] = oldPose->scale[0] * backlerp + pose->scale[0] * lerp;
			relativeJoint->scale[1] = oldPose->scale[1] * backlerp + pose->scale[1] * lerp;
			relativeJoint->scale[2] = oldPose->scale[2] * backlerp + pose->scale[2] * lerp;

			vk_rt_iqm_quat_slerp( oldPose->rotate, pose->rotate, lerp, relativeJoint->rotate );
		}
	}

	relativeJoint = relativeJoints;
	jointParent = data->jointParents;
	invBindMat = data->invBindJoints;
	poseMat = poseMats;
	for ( i = 0; i < data->num_poses; i++, relativeJoint++, jointParent++, invBindMat += 12, poseMat += 12 ) {
		float mat1[12];
		float mat2[12];

		vk_rt_iqm_joint_to_matrix( relativeJoint->rotate, relativeJoint->scale, relativeJoint->translate, mat1 );

		if ( *jointParent >= 0 ) {
			vk_rt_iqm_matrix34_multiply( &data->bindJoints[(*jointParent)*12], mat1, mat2 );
			vk_rt_iqm_matrix34_multiply( mat2, invBindMat, mat1 );
			vk_rt_iqm_matrix34_multiply( &poseMats[(*jointParent)*12], mat1, poseMat );
		} else {
			vk_rt_iqm_matrix34_multiply( mat1, invBindMat, poseMat );
		}
	}
}

static float *s_rtIqmScratch;
static uint32_t s_rtIqmScratchCount;

static qboolean RT_IqmScratch( uint32_t count )
{
	uint32_t capacity;
	size_t bytes;
	float *replacement;
	if ( count <= s_rtIqmScratchCount ) return qtrue;
	if ( !vk_rt_cpu_array_growth( s_rtIqmScratchCount, count, 256u,
		21u * sizeof( float ), "IQM influence scratch", &capacity, &bytes ) ) return qfalse;
	replacement = (float *)RT_HostAlloc( bytes );
	if ( !replacement ) return qfalse;
	RT_HostFree( s_rtIqmScratch );
	s_rtIqmScratch = replacement;
	s_rtIqmScratchCount = capacity;
	return qtrue;
}

static qboolean vk_rt_append_iqm_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent )
{
	static const float kIdentity34[12] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f
	};
	const model_t *model = R_GetModelByHandle( ent->e.hModel );
	const iqmData_t *data;
	float poseMats[IQM_MAX_JOINTS * 12];
	int frame;
	int oldframe;
	float backlerp;
	int skinningMode;
	int s;

	if ( !model || model->type != MOD_IQM ) {
		return qtrue;
	}

	data = (const iqmData_t *)model->modelData;
	if ( !data || data->num_surfaces <= 0 ) {
		return qtrue;
	}

	frame = data->num_frames ? ent->e.frame % data->num_frames : 0;
	oldframe = data->num_frames ? ent->e.oldframe % data->num_frames : 0;
	if ( frame < 0 ) {
		frame += data->num_frames;
	}
	if ( oldframe < 0 ) {
		oldframe += data->num_frames;
	}

	backlerp = Com_Clamp( 0.0f, 1.0f, ent->e.backlerp );
	skinningMode = rtx_rt_iqm_skinning ? rtx_rt_iqm_skinning->integer : 1;
	if ( skinningMode <= 0 || data->num_poses <= 0 ) {
		Com_Memset( poseMats, 0, sizeof( poseMats ) );
	} else {
		if ( skinningMode >= 2 ) {
			oldframe = frame;
			backlerp = 0.0f;
		}
		vk_rt_iqm_compute_pose_mats( data, frame, oldframe, backlerp, poseMats );
	}

	for ( s = 0; s < data->num_surfaces; s++ ) {
		const srfIQModel_t *surface = &data->surfaces[s];
		const shader_t *shader = vk_rt_resolve_iqm_shader( ent, surface );
		uint32_t materialIndex = 0;
		uint32_t baseVertex;
		uint32_t surfaceIndexCount;
		uint32_t validTriangleCount = 0;
		vec4_t tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
		float *influenceVtxMat = NULL;
		float *influenceNrmMat = NULL;
		qboolean useSkinning = qfalse;
		int i;

		if ( !vk_rt_cpu_geometry_find_or_add_material( geometry, shader, &materialIndex ) ) {
			return qfalse;
		}

		if ( surface->num_vertexes < 0 || surface->num_triangles < 0 ||
			!vk_rt_checked_u32_multiply( (uint32_t)surface->num_triangles, 3u,
				&surfaceIndexCount ) ) {
			ri.Printf( PRINT_WARNING, "RTX RT: refusing malformed IQM geometry\n" );
			return qfalse;
		}
		if ( !vk_rt_cpu_geometry_reserve( geometry, (uint32_t)surface->num_vertexes, surfaceIndexCount ) ) {
			return qfalse;
		}

		if ( skinningMode > 0 && data->num_poses > 0 && data->influences && surface->num_influences > 0 ) {
			useSkinning = qtrue;
			/* Reused across surfaces/frames; allocation failure must not silently
			 * freeze this animated shadow in the model's bind pose. */
			if ( !RT_IqmScratch( (uint32_t)surface->num_influences ) ) return qfalse;
			influenceVtxMat = s_rtIqmScratch;
			influenceNrmMat = s_rtIqmScratch + 12u * s_rtIqmScratchCount;

			for ( i = 0; useSkinning && i < surface->num_influences; i++ ) {
				int influence = surface->first_influence + i;
				float *vtxMat = &influenceVtxMat[12*i];
				float *nrmMat = &influenceNrmMat[9*i];
				float blendWeights[4];
				int j;

				if ( data->blendWeightsType == IQM_FLOAT ) {
					blendWeights[0] = data->influenceBlendWeights.f[4*influence + 0];
					blendWeights[1] = data->influenceBlendWeights.f[4*influence + 1];
					blendWeights[2] = data->influenceBlendWeights.f[4*influence + 2];
					blendWeights[3] = data->influenceBlendWeights.f[4*influence + 3];
				} else {
					blendWeights[0] = (float)data->influenceBlendWeights.b[4*influence + 0] / 255.0f;
					blendWeights[1] = (float)data->influenceBlendWeights.b[4*influence + 1] / 255.0f;
					blendWeights[2] = (float)data->influenceBlendWeights.b[4*influence + 2] / 255.0f;
					blendWeights[3] = (float)data->influenceBlendWeights.b[4*influence + 3] / 255.0f;
				}

				if ( blendWeights[0] <= 0.0f ) {
					Com_Memcpy( vtxMat, kIdentity34, sizeof( kIdentity34 ) );
				} else {
					vtxMat[0] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 0];
					vtxMat[1] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 1];
					vtxMat[2] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 2];
					vtxMat[3] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 3];
					vtxMat[4] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 4];
					vtxMat[5] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 5];
					vtxMat[6] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 6];
					vtxMat[7] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 7];
					vtxMat[8] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 8];
					vtxMat[9] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 9];
					vtxMat[10] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 10];
					vtxMat[11] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 11];

					for ( j = 1; j < 4; j++ ) {
						if ( blendWeights[j] <= 0.0f ) {
							break;
						}
						vtxMat[0] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 0];
						vtxMat[1] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 1];
						vtxMat[2] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 2];
						vtxMat[3] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 3];
						vtxMat[4] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 4];
						vtxMat[5] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 5];
						vtxMat[6] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 6];
						vtxMat[7] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 7];
						vtxMat[8] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 8];
						vtxMat[9] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 9];
						vtxMat[10] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 10];
						vtxMat[11] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 11];
					}
				}

				nrmMat[ 0] = vtxMat[ 5]*vtxMat[10] - vtxMat[ 6]*vtxMat[ 9];
				nrmMat[ 1] = vtxMat[ 6]*vtxMat[ 8] - vtxMat[ 4]*vtxMat[10];
				nrmMat[ 2] = vtxMat[ 4]*vtxMat[ 9] - vtxMat[ 5]*vtxMat[ 8];
				nrmMat[ 3] = vtxMat[ 2]*vtxMat[ 9] - vtxMat[ 1]*vtxMat[10];
				nrmMat[ 4] = vtxMat[ 0]*vtxMat[10] - vtxMat[ 2]*vtxMat[ 8];
				nrmMat[ 5] = vtxMat[ 1]*vtxMat[ 8] - vtxMat[ 0]*vtxMat[ 9];
				nrmMat[ 6] = vtxMat[ 1]*vtxMat[ 6] - vtxMat[ 2]*vtxMat[ 5];
				nrmMat[ 7] = vtxMat[ 2]*vtxMat[ 4] - vtxMat[ 0]*vtxMat[ 6];
				nrmMat[ 8] = vtxMat[ 0]*vtxMat[ 5] - vtxMat[ 1]*vtxMat[ 4];
			}
		}

		baseVertex = geometry->numVertices;
		for ( i = 0; i < surface->num_vertexes; i++ ) {
			const int vtx = surface->first_vertex + i;
			const float *xyz = &data->positions[vtx * 3];
			const float *normal = &data->normals[vtx * 3];
			const float *texCoord = &data->texcoords[vtx * 2];
			vec3_t localPos;
			vec3_t localNormal;
			vec3_t worldPos;
			vec3_t worldNormal;
			vec2_t uv;
			uint32_t color = 0xFFFFFFFFu;

			if ( useSkinning ) {
				int influence = data->influences[vtx] - surface->first_influence;
				float *vtxMat;
				float *nrmMat;

				if ( influence < 0 || influence >= surface->num_influences ) {
					influence = 0;
				}

				vtxMat = &influenceVtxMat[12*influence];
				nrmMat = &influenceNrmMat[9*influence];

				localPos[0] = vtxMat[0] * xyz[0] + vtxMat[1] * xyz[1] + vtxMat[2] * xyz[2] + vtxMat[3];
				localPos[1] = vtxMat[4] * xyz[0] + vtxMat[5] * xyz[1] + vtxMat[6] * xyz[2] + vtxMat[7];
				localPos[2] = vtxMat[8] * xyz[0] + vtxMat[9] * xyz[1] + vtxMat[10] * xyz[2] + vtxMat[11];

				localNormal[0] = nrmMat[0] * normal[0] + nrmMat[1] * normal[1] + nrmMat[2] * normal[2];
				localNormal[1] = nrmMat[3] * normal[0] + nrmMat[4] * normal[1] + nrmMat[5] * normal[2];
				localNormal[2] = nrmMat[6] * normal[0] + nrmMat[7] * normal[1] + nrmMat[8] * normal[2];
				VectorNormalize( localNormal );
			} else {
				localPos[0] = xyz[0];
				localPos[1] = xyz[1];
				localPos[2] = xyz[2];
				localNormal[0] = normal[0];
				localNormal[1] = normal[1];
				localNormal[2] = normal[2];
			}

			vk_rt_transform_point( ent, localPos, worldPos );
			vk_rt_transform_direction( ent, localNormal, worldNormal );
			uv[0] = texCoord[0];
			uv[1] = texCoord[1];

			if ( data->colors ) {
				color = vk_rt_color_u32_from_bytes( &data->colors[vtx * 4] );
			}

			if ( !vk_rt_cpu_geometry_add_vertex( geometry, worldPos, worldNormal, tangent, uv, color, materialIndex, NULL ) ) {
				return qfalse;
			}
		}

		for ( i = 0; i < surface->num_triangles; i++ ) {
			const int *tri = &data->triangles[3 * ( surface->first_triangle + i )];
			int i0 = tri[0] - surface->first_vertex;
			int i1 = tri[1] - surface->first_vertex;
			int i2 = tri[2] - surface->first_vertex;

			if ( i0 < 0 || i0 >= surface->num_vertexes ||
				i1 < 0 || i1 >= surface->num_vertexes ||
				i2 < 0 || i2 >= surface->num_vertexes ) {
				continue;
			}

			if ( !vk_rt_cpu_geometry_add_triangle( geometry,
				baseVertex + (uint32_t)i0,
				baseVertex + (uint32_t)i1,
				baseVertex + (uint32_t)i2 ) ) {
				return qfalse;
			}
			validTriangleCount++;
		}

		geometry->sourceTriangleCount += validTriangleCount;
		if ( geometry->materials[ materialIndex ].flags & RTX_RT_MATFLAG_MASKED ) {
			geometry->maskedTriangleCount += validTriangleCount;
		}
		geometry->sourceEntityCount++;
	}

	return qtrue;
}

static qboolean vk_rt_extract_dynamic_model_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent, uint32_t *outModelCount )
{
	const model_t *model;
	qboolean counted = qfalse;

	if ( !geometry || !ent || ent->e.reType != RT_MODEL || ent->e.hModel <= 0 ) {
		return qtrue;
	}

	model = R_GetModelByHandle( ent->e.hModel );
	if ( !model ) {
		return qtrue;
	}

	switch ( model->type ) {
	case MOD_MDS:
		if ( !vk_rt_append_mds_entity_geometry( geometry, ent ) ) return qfalse;
		counted = qtrue;
		break;
	case MOD_WOLFET_MESH:
		if ( !vk_rt_append_wolfet_entity_geometry( geometry, ent ) ) return qfalse;
		counted = qtrue;
		break;
	case MOD_MOHAA_TIKI:
		if ( !vk_rt_append_mohaa_entity_geometry( geometry, ent ) ) return qfalse;
		counted = qtrue;
		break;
	case MOD_MDC:
		if ( !vk_rt_append_mdc_entity_geometry( geometry, ent ) ) {
			return qfalse;
		}
		counted = qtrue;
		break;
	case MOD_MESH:
		if ( !vk_rt_append_md3_entity_geometry( geometry, ent ) ) {
			return qfalse;
		}
		counted = qtrue;
		break;
	case MOD_IQM:
		if ( !vk_rt_append_iqm_entity_geometry( geometry, ent ) ) {
			return qfalse;
		}
		counted = qtrue;
		break;
	case MOD_BRUSH:
		if ( !vk_rt_append_brush_entity_geometry( geometry, ent, model ) ) {
			return qfalse;
		}
		counted = qtrue;
		break;
	case MOD_MDR:
		if ( !vk_rt_append_mdr_entity_geometry( geometry, ent ) ) {
			return qfalse;
		}
		counted = qtrue;
		break;
	default:
		break;
	}

	if ( counted && outModelCount ) {
		*outModelCount += 1u;
	}

	return qtrue;
}

#include "rt/rt_geometry_cache.h"

static qboolean vk_rt_init( void )
{
	if ( vk.rt.initialized ) {
		return qtrue;
	}

	vk_rt_reset_state();

	vk.rt.rt_pipeline_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	vk.rt.rt_pipeline_props.pNext = &vk.rt.as_props;
	vk.rt.as_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
	vk.rt.as_props.pNext = NULL;

	if ( !qvkGetPhysicalDeviceProperties2 && !qvkGetPhysicalDeviceProperties2KHR ) {
		ri.Printf( PRINT_ERROR,
			"RTX RT: vkGetPhysicalDeviceProperties2/core-or-KHR is unavailable\n" );
		return qfalse;
	}

	{
		VkPhysicalDeviceProperties2 props2;
		Com_Memset( &props2, 0, sizeof( props2 ) );
		props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		// the compute backend has no ray-tracing properties to ask for
		props2.pNext = vk_rt_pipeline_enabled() ? &vk.rt.rt_pipeline_props : NULL;
		if ( qvkGetPhysicalDeviceProperties2 ) {
			qvkGetPhysicalDeviceProperties2( vk.physical_device, &props2 );
		} else {
			qvkGetPhysicalDeviceProperties2KHR( vk.physical_device, &props2 );
		}
		vk.rt.gpu_timestamp_period_ns = ( props2.properties.limits.timestampPeriod > 0.0f ) ?
			props2.properties.limits.timestampPeriod : 1.0f;
	}

	vk.rt.initialized = qtrue;
	return qtrue;
}

void vk_rt_invalidate( const char *reason )
{
	if ( !vk.rt.initialized ) {
		return;
	}
	if ( vk_rt_debug_as_stats_level() >= 1 ) {
		ri.Printf( PRINT_DEVELOPER, "RTX RT: invalidating RT state (%s)\n", reason ? reason : "unspecified" );
	}
	if ( !vk_rt_wait_for_inflight_frames( reason ? reason : "RT state invalidation" ) ) {
		return;
	}
	RT_InvalidateScene();
	vk.rt.dispatchValid = qfalse;
}

static void vk_rt_shutdown( void )
{
	if ( !vk.rt.initialized ) {
		return;
	}
	RT_Shutdown();
	vk_rt_reset_state();
}

qboolean vk_rt_primary_view_eligible( void )
{
	static int lastRefusedFrame = -1;
	const qboolean eligible = ( vk.caps.activeRtMode == 2 &&
		vk.fboActive &&
		!( backEnd.refdef.rdflags & ( RDF_NOWORLDMODEL | RDF_HYPERSPACE ) ) &&
		// an earlier view of the frame (RTCW's portal sky) may have ended the
		// main pass, and this one then draws in the post-bloom pass
		( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) &&
		backEnd.viewParms.portalView == PV_NONE &&
		// RTCW's and ET's portal sky is drawn first, behind the view it is
		// seen from: tracing it would leave that view to raster
		!backEnd.refdef.skyboxPortalScene &&
		// RTCW's snooper scope is a heat picture, not a lit scene: white
		// for every lightmap, bright players, no dynamic light or fog
		!backEnd.refdef.rtcw.snooperView &&
		backEnd.viewParms.stereoFrame == STEREO_CENTER &&
		!glConfig.stereoEnabled &&
		// the main view: most of the screen (Enemy Territory insets its own by
		// a pixel), where a picture-in-picture view stays raster
		backEnd.viewParms.viewportX >= 0 &&
		backEnd.viewParms.viewportY >= 0 &&
		backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth <= glConfig.vidWidth &&
		backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight <= glConfig.vidHeight &&
		(int64_t)backEnd.viewParms.viewportWidth * backEnd.viewParms.viewportHeight * 2 >=
			(int64_t)glConfig.vidWidth * glConfig.vidHeight &&
		!( vk.rt.dispatchValid &&
			vk.rt.lastFrameBuilt == (uint32_t)tr.frameCount ) ) ?
			qtrue : qfalse;

	// why a world view is left to raster (rtx_rt_debug_as_stats 2); a portal
	// sky always is, and would take the report from the view behind it
	if ( !eligible && vk.caps.activeRtMode == 2 && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) &&
		!backEnd.refdef.skyboxPortalScene && vk_rt_debug_should_log_skip( &lastRefusedFrame ) ) {
		ri.Printf( PRINT_DEVELOPER, "RTX RT: view left to raster: fbo=%d rdflags=0x%x renderPass=%d portalView=%d "
			"stereo=%d viewport=%d,%d %dx%d of %dx%d, traced this frame=%d\n",
			vk.fboActive ? 1 : 0, backEnd.refdef.rdflags, (int)vk.renderPassIndex,
			(int)backEnd.viewParms.portalView, (int)backEnd.viewParms.stereoFrame,
			backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight,
			glConfig.vidWidth, glConfig.vidHeight,
			( vk.rt.dispatchValid && vk.rt.lastFrameBuilt == (uint32_t)tr.frameCount ) ? 1 : 0 );
	}
	return eligible;
}

qboolean vk_rt_trace_frame( void )
{
	static int lastFboSkipFrame = -1;
	static int lastUiSkipFrame = -1;
	static int lastHyperspaceSkipFrame = -1;
	static int lastNonPrimarySkipFrame = -1;
	if ( rtx_rt_test_fail_trace && rtx_rt_test_fail_trace->integer ) return qfalse;

	if ( !vk_rt_trace_available() ) {
		ri.Printf( PRINT_DEVELOPER, "RTX RT: no tracing backend is available\n" );
		return qfalse;
	}

	if ( !vk.active || !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, "RTX RT: no frame is being recorded\n" );
		return qfalse;
	}

	if ( !vk.rt.initialized && !vk_rt_init() ) {
		ri.Printf( PRINT_DEVELOPER, "RTX RT: the path tracer could not be initialised\n" );
		return qfalse;
	}

	if ( !tr.worldMapLoaded || !tr.world ) {
		vk.rt.stats.skipped_dispatches++;
		ri.Printf( PRINT_DEVELOPER, "RTX RT: no world is loaded\n" );
		return qfalse;
	}

	if ( !vk.fboActive ) {
		vk.rt.stats.skipped_dispatches++;
		if ( vk_rt_debug_should_log_skip( &lastFboSkipFrame ) ) {
			ri.Printf( PRINT_DEVELOPER, "RTX RT: skipping dispatch because r_fbo is disabled\n" );
		}
		return qfalse;
	}

	if ( vk_rt_ui_passthrough_enabled() && ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk.rt.stats.skipped_dispatches++;
		if ( vk_rt_debug_should_log_skip( &lastUiSkipFrame ) ) {
			ri.Printf( PRINT_DEVELOPER, "RTX RT: skipping dispatch for RDF_NOWORLDMODEL UI/console composition view\n" );
		}
		return qfalse;
	}
	if ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) {
		vk.rt.stats.skipped_dispatches++;
		if ( vk_rt_debug_should_log_skip( &lastHyperspaceSkipFrame ) ) {
			ri.Printf( PRINT_DEVELOPER, "RTX RT: skipping dispatch for RDF_HYPERSPACE view\n" );
		}
		return qfalse;
	}

	/*
	 * The trace covers one view a frame, the main one.  Portal/mirror views
	 * and screen-map capture commands render into sub-views of the frame, and
	 * tracing them would replace the resources earlier commands reference.
	 * Their raster result is composited by the main view's portal shader.
	 */
	if ( !vk_rt_primary_view_eligible() ) {
		vk.rt.stats.skipped_dispatches++;
		if ( vk_rt_debug_should_log_skip( &lastNonPrimarySkipFrame ) ) {
			ri.Printf( PRINT_DEVELOPER,
				"RTX RT: skipping non-primary/duplicate view (renderPass=%d portalView=%d stereo=%d viewport=%d,%d %dx%d)\n",
				(int)vk.renderPassIndex,
				(int)backEnd.viewParms.portalView,
				(int)backEnd.viewParms.stereoFrame,
				backEnd.viewParms.viewportX,
				backEnd.viewParms.viewportY,
				backEnd.viewParms.viewportWidth,
				backEnd.viewParms.viewportHeight );
		}
		return qfalse;
	}

	return RT_TraceFrame();
}

static const char *vk_capability_value( qboolean supported )
{
	return supported ? "yes" : "no";
}

static void vk_report_capabilities( const char *context )
{
	const int reportLevel = vk_caps_report_level();

	if ( reportLevel <= 0 ) {
		return;
	}

	ri.Printf( PRINT_ALL,
		"RTX capabilities (%s): swapchain=%s debug_markers=%s descriptor_indexing=%s buffer_device_address=%s\n",
		context ? context : "runtime",
		vk_capability_value( vk.caps.swapchain ),
		vk_capability_value( vk.caps.debugMarkers ),
		vk_capability_value( vk.caps.descriptorIndexing ),
		vk_capability_value( vk.caps.bufferDeviceAddress ) );

	if ( reportLevel >= 2 ) {
		ri.Printf( PRINT_ALL,
			"RTX capabilities (%s): timeline_semaphore=%s memory_model=%s deferred_host_ops=%s dedicated_allocation=%s\n",
			context ? context : "runtime",
			vk_capability_value( vk.caps.timelineSemaphore ),
			vk_capability_value( vk.caps.vulkanMemoryModel ),
			vk_capability_value( vk.caps.deferredHostOperations ),
			vk_capability_value( vk.caps.dedicatedAllocation ) );
		ri.Printf( PRINT_ALL,
			"RTX capabilities (%s): acceleration_structure=%s ray_query=%s ray_tracing_pipeline=%s\n",
			context ? context : "runtime",
			vk_capability_value( vk.caps.accelerationStructure ),
			vk_capability_value( vk.caps.rayQuery ),
			vk_capability_value( vk.caps.rayTracingPipeline ) );
	}

	ri.Printf( PRINT_ALL, "RTX capability gate: requested=%s (%d), active=%s (%d), require=%d, backend=%s\n",
		vk_rt_mode_name( vk.caps.requestedRtMode ), vk.caps.requestedRtMode,
		vk_rt_mode_name( vk.caps.activeRtMode ), vk.caps.activeRtMode,
		vk_rt_mode_required() ? 1 : 0, vk_rt_backend_name( vk.caps.rtBackend ) );
}

static const char *vk_lifetime_state_name( rtxVkLifetimeState_t state )
{
	switch ( state ) {
	case RTX_VK_LIFETIME_UNINITIALIZED: return "uninitialized";
	case RTX_VK_LIFETIME_INITIALIZING: return "initializing";
	case RTX_VK_LIFETIME_READY: return "ready";
	case RTX_VK_LIFETIME_RESIZING: return "resizing";
	case RTX_VK_LIFETIME_SHUTTING_DOWN: return "shutting_down";
	default: return "unknown";
	}
}

static const char *vk_lifetime_resource_name( rtxVkResource_t resource )
{
	switch ( resource ) {
	case RTX_VK_RESOURCE_SYNC_PRIMITIVES: return "sync_primitives";
	case RTX_VK_RESOURCE_SWAPCHAIN: return "swapchain";
	case RTX_VK_RESOURCE_ATTACHMENTS: return "attachments";
	case RTX_VK_RESOURCE_RENDER_PASSES: return "render_passes";
	case RTX_VK_RESOURCE_FRAMEBUFFERS: return "framebuffers";
	case RTX_VK_RESOURCE_GEOMETRY_BUFFERS: return "geometry_buffers";
	case RTX_VK_RESOURCE_STORAGE_BUFFER: return "storage_buffer";
	default: return "unknown";
	}
}

static void vk_lifetime_reset( void )
{
	Com_Memset( &s_vkLifetimeTracker, 0, sizeof( s_vkLifetimeTracker ) );
	s_vkLifetimeTracker.state = RTX_VK_LIFETIME_UNINITIALIZED;
}

static void vk_lifetime_set_state( rtxVkLifetimeState_t state, const char *reason )
{
	const int debugLevel = vk_lifetime_debug_level();

	s_vkLifetimeTracker.state = state;

	if ( debugLevel >= 2 ) {
		ri.Printf( PRINT_DEVELOPER, "RTX resource lifetime: state=%s (%s), generation=%u\n",
			vk_lifetime_state_name( state ), reason ? reason : "no-reason", s_vkLifetimeTracker.generation );
	}
}

static void vk_lifetime_mark_create( rtxVkResource_t resource, const char *context )
{
	rtxVkResourceEntry_t *entry = &s_vkLifetimeTracker.resources[ resource ];
	const int debugLevel = vk_lifetime_debug_level();

	s_vkLifetimeTracker.serial++;

	if ( entry->alive && debugLevel >= 1 ) {
		ri.Printf( PRINT_WARNING, "RTX resource lifetime: duplicate create of %s in %s (state=%s)\n",
			vk_lifetime_resource_name( resource ), context, vk_lifetime_state_name( s_vkLifetimeTracker.state ) );
	}

	entry->alive = qtrue;
	entry->createCount++;
	entry->lastSerial = s_vkLifetimeTracker.serial;
}

static void vk_lifetime_mark_destroy( rtxVkResource_t resource, const char *context )
{
	rtxVkResourceEntry_t *entry = &s_vkLifetimeTracker.resources[ resource ];
	const int debugLevel = vk_lifetime_debug_level();

	s_vkLifetimeTracker.serial++;

	if ( !entry->alive && debugLevel >= 1 ) {
		ri.Printf( PRINT_WARNING, "RTX resource lifetime: destroy of non-live %s in %s (state=%s)\n",
			vk_lifetime_resource_name( resource ), context, vk_lifetime_state_name( s_vkLifetimeTracker.state ) );
	}

	entry->alive = qfalse;
	entry->destroyCount++;
	entry->lastSerial = s_vkLifetimeTracker.serial;
}

static void vk_lifetime_mark_use( rtxVkResource_t resource, const char *context )
{
	rtxVkResourceEntry_t *entry = &s_vkLifetimeTracker.resources[ resource ];
	const int debugLevel = vk_lifetime_debug_level();

	if ( !entry->alive ) {
		if ( debugLevel >= 1 ) {
			ri.Printf( PRINT_WARNING, "RTX resource lifetime: use of non-live %s in %s (state=%s)\n",
				vk_lifetime_resource_name( resource ), context, vk_lifetime_state_name( s_vkLifetimeTracker.state ) );
		}
		return;
	}

	entry->useCount++;
	entry->lastUseFrame = tr.frameCount;
}

static uint32_t vk_lifetime_ready_mask( void )
{
	return
		( 1u << RTX_VK_RESOURCE_SYNC_PRIMITIVES ) |
		( 1u << RTX_VK_RESOURCE_SWAPCHAIN ) |
		( 1u << RTX_VK_RESOURCE_ATTACHMENTS ) |
		( 1u << RTX_VK_RESOURCE_RENDER_PASSES ) |
		( 1u << RTX_VK_RESOURCE_FRAMEBUFFERS ) |
		( 1u << RTX_VK_RESOURCE_GEOMETRY_BUFFERS ) |
		( 1u << RTX_VK_RESOURCE_STORAGE_BUFFER );
}

static void vk_lifetime_check_ready( const char *context )
{
	const int debugLevel = vk_lifetime_debug_level();
	uint32_t mask = vk_lifetime_ready_mask();
	uint32_t i;

	if ( debugLevel < 1 ) {
		return;
	}

	for ( i = 0; i < RTX_VK_RESOURCE_COUNT; i++ ) {
		if ( !( mask & ( 1u << i ) ) ) {
			continue;
		}

		if ( !s_vkLifetimeTracker.resources[i].alive ) {
			ri.Printf( PRINT_WARNING, "RTX resource lifetime: %s missing at %s\n",
				vk_lifetime_resource_name( (rtxVkResource_t)i ), context );
		}
	}
}

static void vk_lifetime_check_shutdown( const char *context )
{
	const int debugLevel = vk_lifetime_debug_level();
	uint32_t i;

	if ( debugLevel < 1 ) {
		return;
	}

	for ( i = 0; i < RTX_VK_RESOURCE_COUNT; i++ ) {
		if ( s_vkLifetimeTracker.resources[i].alive ) {
			ri.Printf( PRINT_WARNING, "RTX resource lifetime: %s still live at %s\n",
				vk_lifetime_resource_name( (rtxVkResource_t)i ), context );
		}
	}
}

static void vk_lifetime_begin_init( void )
{
	vk_lifetime_reset();
	s_vkSwapchainResizePending = qfalse;
	s_vkSwapchainResizeRestarting = qfalse;
	s_vkSwapchainResizeResult = VK_SUCCESS;
	vk_lifetime_set_state( RTX_VK_LIFETIME_INITIALIZING, "vk_initialize begin" );
}

static void vk_lifetime_end_init( void )
{
	vk_lifetime_check_ready( "vk_initialize end" );
	vk_lifetime_set_state( RTX_VK_LIFETIME_READY, "vk_initialize end" );
}

static void vk_lifetime_begin_resize( const char *context )
{
	const int debugLevel = vk_lifetime_debug_level();

	if ( s_vkLifetimeTracker.state != RTX_VK_LIFETIME_READY && debugLevel >= 1 ) {
		ri.Printf( PRINT_WARNING, "RTX resource lifetime: resize entered from %s state (%s)\n",
			vk_lifetime_state_name( s_vkLifetimeTracker.state ), context );
	}

	s_vkLifetimeTracker.generation++;
	vk_lifetime_set_state( RTX_VK_LIFETIME_RESIZING, context );
}

static void vk_lifetime_end_resize( const char *context )
{
	vk_lifetime_check_ready( context );
	vk_lifetime_set_state( RTX_VK_LIFETIME_READY, context );
}

static void vk_lifetime_begin_shutdown( void )
{
	vk_lifetime_set_state( RTX_VK_LIFETIME_SHUTTING_DOWN, "vk_shutdown begin" );
}

static void vk_lifetime_end_shutdown( void )
{
	vk_lifetime_check_shutdown( "vk_shutdown end" );
	vk_lifetime_set_state( RTX_VK_LIFETIME_UNINITIALIZED, "vk_shutdown end" );
	s_vkLifetimeTracker.frameActive = qfalse;
	s_vkSwapchainResizePending = qfalse;
	s_vkSwapchainResizeRestarting = qfalse;
	s_vkSwapchainResizeResult = VK_SUCCESS;
}

static void vk_lifetime_begin_frame( void )
{
	const int debugLevel = vk_lifetime_debug_level();

	if ( s_vkLifetimeTracker.state != RTX_VK_LIFETIME_READY && debugLevel >= 1 ) {
		ri.Printf( PRINT_WARNING, "RTX resource lifetime: frame begin while state=%s\n",
			vk_lifetime_state_name( s_vkLifetimeTracker.state ) );
	}

	if ( s_vkLifetimeTracker.frameActive && debugLevel >= 1 ) {
		ri.Printf( PRINT_WARNING, "RTX resource lifetime: nested frame begin detected\n" );
	}

	s_vkLifetimeTracker.frameActive = qtrue;
	s_vkLifetimeTracker.frameSerial++;

	vk_lifetime_mark_use( RTX_VK_RESOURCE_SYNC_PRIMITIVES, "vk_begin_frame" );
	vk_lifetime_mark_use( RTX_VK_RESOURCE_ATTACHMENTS, "vk_begin_frame" );
	vk_lifetime_mark_use( RTX_VK_RESOURCE_RENDER_PASSES, "vk_begin_frame" );
	vk_lifetime_mark_use( RTX_VK_RESOURCE_FRAMEBUFFERS, "vk_begin_frame" );
	vk_lifetime_mark_use( RTX_VK_RESOURCE_GEOMETRY_BUFFERS, "vk_begin_frame" );
	vk_lifetime_mark_use( RTX_VK_RESOURCE_STORAGE_BUFFER, "vk_begin_frame" );
}

static void vk_lifetime_end_frame( qboolean submitted )
{
	const int debugLevel = vk_lifetime_debug_level();

	if ( !s_vkLifetimeTracker.frameActive ) {
		if ( debugLevel >= 1 ) {
			ri.Printf( PRINT_WARNING, "RTX resource lifetime: frame end without active frame\n" );
		}
		return;
	}

	if ( submitted ) {
		vk_lifetime_mark_use( RTX_VK_RESOURCE_SYNC_PRIMITIVES, "vk_end_frame submit" );
	}

	s_vkLifetimeTracker.frameActive = qfalse;
}

static void vk_schedule_swapchain_resize( const char *context, VkResult res, const char *reason )
{
	const int debugLevel = vk_lifetime_debug_level();
	const qboolean shouldLog =
		( !s_vkSwapchainResizePending || s_vkSwapchainResizeResult != res || debugLevel >= 2 );

	s_vkSwapchainResizePending = qtrue;
	s_vkSwapchainResizeResult = res;

	if ( shouldLog ) {
		ri.Printf( PRINT_WARNING, "Vulkan: swapchain resize deferred in %s (%s)%s%s\n",
			context ? context : "unknown",
			vk_result_string( res ),
			reason ? ": " : "",
			reason ? reason : "" );
	}
}

static void vk_clear_scheduled_swapchain_resize( void )
{
	s_vkSwapchainResizePending = qfalse;
	s_vkSwapchainResizeResult = VK_SUCCESS;
}

static qboolean vk_swapchain_can_restart( void )
{
	if ( ri.CL_IsMinimized() ) {
		return qfalse;
	}

	if ( gls.windowWidth <= 0 || gls.windowHeight <= 0 ) {
		return qfalse;
	}

	return qtrue;
}

static uint32_t find_memory_type( uint32_t memory_type_bits, VkMemoryPropertyFlags properties ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ((memory_type_bits & (1 << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	ri.Error( ERR_FATAL, "Vulkan: failed to find matching memory type with requested properties" );
	return ~0U;
}


static uint32_t find_memory_type2( uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ( (memory_type_bits & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
			if ( outprops ) {
				*outprops = memory_properties.memoryTypes[i].propertyFlags;
			}
			return i;
		}
	}

	return ~0U;
}


static const char *pmode_to_str( VkPresentModeKHR mode )
{
	static char buf[32];

	switch ( mode ) {
		case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
		case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
		case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
		case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT: return "FIFO_LATEST_READY";
		default: sprintf( buf, "mode#%x", mode ); return buf;
	};
}


#define CASE_STR(x) case (x): return #x

const char *vk_format_string( VkFormat format )
{
	static char buf[16];

	switch ( format ) {
		// color formats
		CASE_STR( VK_FORMAT_R5G5B5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G5R5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R5G6B5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G6R5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B8G8R8A8_SRGB );
		CASE_STR( VK_FORMAT_R8G8B8A8_SRGB );
		CASE_STR( VK_FORMAT_B8G8R8A8_SNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_SNORM );
		CASE_STR( VK_FORMAT_B8G8R8A8_UNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_UNORM );
		CASE_STR( VK_FORMAT_B4G4R4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R4G4B4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R16G16B16A16_UNORM );
		CASE_STR( VK_FORMAT_R16G16B16A16_SFLOAT );
		CASE_STR( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_A2R10G10B10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_B10G11R11_UFLOAT_PACK32 );
		// depth formats
		CASE_STR( VK_FORMAT_D16_UNORM );
		CASE_STR( VK_FORMAT_D16_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_X8_D24_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_D24_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_D32_SFLOAT );
		CASE_STR( VK_FORMAT_D32_SFLOAT_S8_UINT );
	default:
		Com_sprintf( buf, sizeof( buf ), "#%i", format );
		return buf;
	}
}


static const char *vk_result_string( VkResult code ) {
	static char buffer[32];

	switch ( code ) {
		CASE_STR( VK_SUCCESS );
		CASE_STR( VK_NOT_READY );
		CASE_STR( VK_TIMEOUT );
		CASE_STR( VK_EVENT_SET );
		CASE_STR( VK_EVENT_RESET );
		CASE_STR( VK_INCOMPLETE );
		CASE_STR( VK_ERROR_OUT_OF_HOST_MEMORY );
		CASE_STR( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		CASE_STR( VK_ERROR_INITIALIZATION_FAILED );
		CASE_STR( VK_ERROR_DEVICE_LOST );
		CASE_STR( VK_ERROR_MEMORY_MAP_FAILED );
		CASE_STR( VK_ERROR_LAYER_NOT_PRESENT );
		CASE_STR( VK_ERROR_EXTENSION_NOT_PRESENT );
		CASE_STR( VK_ERROR_FEATURE_NOT_PRESENT );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DRIVER );
		CASE_STR( VK_ERROR_TOO_MANY_OBJECTS );
		CASE_STR( VK_ERROR_FORMAT_NOT_SUPPORTED );
		CASE_STR( VK_ERROR_FRAGMENTED_POOL );
		CASE_STR( VK_ERROR_UNKNOWN );
		CASE_STR( VK_ERROR_OUT_OF_POOL_MEMORY );
		CASE_STR( VK_ERROR_INVALID_EXTERNAL_HANDLE );
		CASE_STR( VK_ERROR_FRAGMENTATION );
		CASE_STR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS );
		CASE_STR( VK_ERROR_SURFACE_LOST_KHR );
		CASE_STR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		CASE_STR( VK_SUBOPTIMAL_KHR );
		CASE_STR( VK_ERROR_OUT_OF_DATE_KHR );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		CASE_STR( VK_ERROR_VALIDATION_FAILED_EXT );
		CASE_STR( VK_ERROR_INVALID_SHADER_NV );
		CASE_STR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT );
		CASE_STR( VK_ERROR_NOT_PERMITTED_EXT );
		CASE_STR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT );
		CASE_STR( VK_THREAD_IDLE_KHR );
		CASE_STR( VK_THREAD_DONE_KHR );
		CASE_STR( VK_OPERATION_DEFERRED_KHR );
		CASE_STR( VK_OPERATION_NOT_DEFERRED_KHR );
		CASE_STR( VK_PIPELINE_COMPILE_REQUIRED_EXT );
	default:
		sprintf( buffer, "code %i", code );
		return buffer;
	}
}
#undef CASE_STR

#define VK_CHECK( function_call ) { \
	VkResult vkCheckResult = function_call; \
	if ( vkCheckResult < 0 ) { \
		ri.Error( ERR_FATAL, "Vulkan: %s returned %s", #function_call, vk_result_string( vkCheckResult ) ); \
	} \
}


/*
static VkFlags get_composite_alpha( VkCompositeAlphaFlagsKHR flags )
{
	const VkCompositeAlphaFlagBitsKHR compositeFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};
	int i;

	for ( i = 1; i < ARRAY_LEN( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/


static VkCommandBuffer begin_command_buffer( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkCommandBufferAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;

	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = vk.command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &command_buffer ) );

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( command_buffer, &begin_info ) );

	return command_buffer;
}


static qboolean end_command_buffer_internal( VkCommandBuffer command_buffer, const char *location, qboolean fatalOnError )
{
#ifdef USE_UPLOAD_QUEUE
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSemaphore waits;
#endif
	VkSubmitInfo submit_info;
	VkCommandBuffer cmdbuf[1];
	VkResult res;
	const char *ctx = location ? location : "unknown";

	cmdbuf[0] = command_buffer;

	res = qvkEndCommandBuffer( command_buffer );
	if ( res != VK_SUCCESS ) {
		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, cmdbuf );
		if ( fatalOnError ) {
			ri.Error( ERR_FATAL, "Vulkan: %s returned %s", "vkEndCommandBuffer", vk_result_string( res ) );
		}
		ri.Printf( PRINT_ERROR, "RTX RT: vkEndCommandBuffer failed at %s: %s\n", ctx, vk_result_string( res ) );
		return qfalse;
	}

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
#ifdef USE_UPLOAD_QUEUE
	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else 
#endif
	{
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = cmdbuf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	res = qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE );
	if ( res != VK_SUCCESS ) {
		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, cmdbuf );
		if ( fatalOnError ) {
			ri.Error( ERR_FATAL, "Vulkan: %s returned %s", "vkQueueSubmit", vk_result_string( res ) );
		}
		ri.Printf( PRINT_ERROR, "RTX RT: vkQueueSubmit failed at %s: %s\n", ctx, vk_result_string( res ) );
		return qfalse;
	}

	res = qvkQueueWaitIdle( vk.queue );
	if ( res != VK_SUCCESS ) {
		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, cmdbuf );
		if ( fatalOnError ) {
			ri.Error( ERR_FATAL, "Vulkan: %s returned %s", "vkQueueWaitIdle", vk_result_string( res ) );
		}
		ri.Printf( PRINT_ERROR, "RTX RT: vkQueueWaitIdle failed at %s: %s\n", ctx, vk_result_string( res ) );
		return qfalse;
	}

	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, cmdbuf );
	return qtrue;
}

static void end_command_buffer( VkCommandBuffer command_buffer, const char *location )
{
	(void)end_command_buffer_internal( command_buffer, location, qtrue );
}

static qboolean end_command_buffer_soft( VkCommandBuffer command_buffer, const char *location )
{
	return end_command_buffer_internal( command_buffer, location, qfalse );
}


static void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags, 
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override ) {
	VkImageMemoryBarrier barrier;
	uint32_t src_stage, dst_stage;

	/*
	 * The renderer does not enable separateDepthStencilLayouts.  A barrier
	 * covering any of our depth/stencil images must therefore transition
	 * both aspects, even when the image view and copy region intentionally
	 * address only the depth aspect.
	 */
	if ( ( image_aspect_flags & VK_IMAGE_ASPECT_DEPTH_BIT ) &&
		( vk.depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
			vk.depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
			vk.depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ) ) {
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	switch ( old_layout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			if ( src_stage_override != 0 )
				src_stage = src_stage_override;
			else
				src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
				VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported old layout %i", old_layout );
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
	}

	switch ( new_layout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
				VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported new layout %i", new_layout);
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
	}


	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	//barrier.srcAccessMask = src_access_flags;
	//barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier( command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier );
}


// debug markers
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

static qboolean vk_debug_object_names_enabled( void )
{
	return ( vk_debug_markers_mode() >= 1 && vk.debugMarkers && qvkDebugMarkerSetObjectNameEXT ) ? qtrue : qfalse;
}

static qboolean vk_debug_pass_markers_enabled( void )
{
	return ( vk_debug_markers_mode() >= 2 && vk.debugMarkers && qvkCmdDebugMarkerBeginEXT && qvkCmdDebugMarkerEndEXT ) ? qtrue : qfalse;
}

static void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType )
{
	if ( vk_debug_object_names_enabled() && obj )
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = NULL;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT( vk.device, &info );
	}
}

static void vk_cmd_debug_marker_begin( const char *markerName, const float color[4] )
{
	VkDebugMarkerMarkerInfoEXT info;

	if ( !vk_debug_pass_markers_enabled() || !vk.cmd ) {
		return;
	}

	info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
	info.pNext = NULL;
	info.pMarkerName = markerName;
	info.color[0] = color[0];
	info.color[1] = color[1];
	info.color[2] = color[2];
	info.color[3] = color[3];

	qvkCmdDebugMarkerBeginEXT( vk.cmd->command_buffer, &info );
}

static void vk_cmd_debug_marker_end( void )
{
	if ( !vk_debug_pass_markers_enabled() || !vk.cmd ) {
		return;
	}

	qvkCmdDebugMarkerEndEXT( vk.cmd->command_buffer );
}

static void vk_debug_render_pass_label( VkRenderPass renderPass, char *markerName, size_t markerNameSize, float color[4] )
{
	uint32_t i;

	Q_strncpyz( markerName, "RTX Pass: unknown", markerNameSize );
	color[0] = 0.5f; color[1] = 0.5f; color[2] = 0.5f; color[3] = 1.0f;

	if ( renderPass == vk.render_pass.main ) {
		Q_strncpyz( markerName, "RTX Pass: main", markerNameSize );
		color[0] = 0.10f; color[1] = 0.55f; color[2] = 0.95f; color[3] = 1.0f;
		return;
	}
	if ( renderPass == vk.render_pass.main_load ) {
		Q_strncpyz( markerName, "RTX Pass: main load", markerNameSize );
		color[0] = 0.10f; color[1] = 0.55f; color[2] = 0.95f; color[3] = 1.0f;
		return;
	}
	if ( renderPass == vk.render_pass.dlight_shadow ) {
		if ( vk.renderPassIndex == RENDER_PASS_CSM_SHADOW ) {
			Q_strncpyz( markerName, "RTX Pass: csm shadow atlas", markerNameSize );
		} else if ( vk.renderPassIndex == RENDER_PASS_SPOT_SHADOW ) {
			Q_strncpyz( markerName, "RTX Pass: spot shadow atlas", markerNameSize );
		} else {
			Q_strncpyz( markerName, "RTX Pass: dlight shadow atlas", markerNameSize );
		}
		color[0] = 0.72f; color[1] = 0.30f; color[2] = 0.16f; color[3] = 1.0f;
		return;
	}

	if ( renderPass == vk.render_pass.post_bloom ) {
		Q_strncpyz( markerName, "RTX Pass: post_bloom", markerNameSize );
		color[0] = 0.18f; color[1] = 0.62f; color[2] = 0.92f; color[3] = 1.0f;
		return;
	}

	if ( renderPass == vk.render_pass.global_fog ) {
		Q_strncpyz( markerName, "RTX Pass: global_fog", markerNameSize );
		color[0] = 0.55f; color[1] = 0.59f; color[2] = 0.61f; color[3] = 1.0f;
		return;
	}

	if ( renderPass == vk.render_pass.bloom_extract ) {
		Q_strncpyz( markerName, "RTX Pass: bloom_extract", markerNameSize );
		color[0] = 0.93f; color[1] = 0.68f; color[2] = 0.20f; color[3] = 1.0f;
		return;
	}

	for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i++ ) {
		if ( renderPass == vk.render_pass.blur[ i ] ) {
			Com_sprintf( markerName, (int)markerNameSize, "RTX Pass: blur %u", i );
			color[0] = 0.96f; color[1] = 0.80f; color[2] = 0.25f; color[3] = 1.0f;
			return;
		}
	}

	if ( renderPass == vk.render_pass.capture ) {
		Q_strncpyz( markerName, "RTX Pass: capture", markerNameSize );
		color[0] = 0.95f; color[1] = 0.35f; color[2] = 0.30f; color[3] = 1.0f;
		return;
	}

	if ( renderPass == vk.render_pass.gamma ) {
		Q_strncpyz( markerName, "RTX Pass: gamma", markerNameSize );
		color[0] = 0.38f; color[1] = 0.82f; color[2] = 0.45f; color[3] = 1.0f;
		return;
	}

	if ( renderPass == vk.render_pass.screenmap ) {
		Q_strncpyz( markerName, "RTX Pass: screenmap", markerNameSize );
		color[0] = 0.62f; color[1] = 0.52f; color[2] = 0.88f; color[3] = 1.0f;
		return;
	}

	if ( renderPass == vk.render_pass.motion_blur ) {
		Q_strncpyz( markerName, "RTX Pass: motion blur", markerNameSize );
		color[0] = 0.78f; color[1] = 0.36f; color[2] = 0.84f; color[3] = 1.0f;
		return;
	}
}

static void vk_debug_render_pass_begin( VkRenderPass renderPass )
{
	char markerName[64];
	float color[4];

	if ( !vk_debug_pass_markers_enabled() ) {
		return;
	}

	vk_debug_render_pass_label( renderPass, markerName, sizeof( markerName ), color );
	vk_cmd_debug_marker_begin( markerName, color );
}

static void vk_debug_render_pass_end( void )
{
	vk_cmd_debug_marker_end();
}


static void vk_set_hdr_metadata( void )
{
	VkHdrMetadataEXT metadata;
	float max_luminance;
	float max_cll;
	float max_fall;

	if ( !vk.hdrDisplayActive || !vk.hdrMetadata || !qvkSetHdrMetadataEXT ||
		vk.swapchain == VK_NULL_HANDLE ) {
		return;
	}
	max_luminance = Com_Clamp( 200.0f, 10000.0f,
		r_hdrDisplayMaxLuminance ? r_hdrDisplayMaxLuminance->value : 1000.0f );
	if ( vk.displayOutput.maxLuminanceNits >= 200.0f ) {
		max_luminance = Com_Clamp( 200.0f, max_luminance,
			vk.displayOutput.maxLuminanceNits );
	}
	max_cll = Com_Clamp( 200.0f, 10000.0f,
		r_hdrDisplayMaxCLL ? r_hdrDisplayMaxCLL->value : 1000.0f );
	if ( max_cll > max_luminance ) {
		max_cll = max_luminance;
	}
	max_fall = Com_Clamp( 80.0f, max_cll,
		r_hdrDisplayMaxFALL ? r_hdrDisplayMaxFALL->value : 400.0f );

	Com_Memset( &metadata, 0, sizeof( metadata ) );
	metadata.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
	metadata.pNext = NULL;
	metadata.displayPrimaryRed.x = 0.708f;
	metadata.displayPrimaryRed.y = 0.292f;
	metadata.displayPrimaryGreen.x = 0.170f;
	metadata.displayPrimaryGreen.y = 0.797f;
	metadata.displayPrimaryBlue.x = 0.131f;
	metadata.displayPrimaryBlue.y = 0.046f;
	metadata.whitePoint.x = 0.3127f;
	metadata.whitePoint.y = 0.3290f;
	metadata.maxLuminance = max_luminance;
	metadata.minLuminance = 0.005f;
	metadata.maxContentLightLevel = max_cll;
	metadata.maxFrameAverageLightLevel = max_fall;

	qvkSetHdrMetadataEXT( vk.device, 1, &vk.swapchain, &metadata );
}


static void vk_create_swapchain( VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain, qboolean verbose ) {
	VkImageViewCreateInfo view;
	VkSurfaceCapabilitiesKHR surface_caps;
	VkExtent2D image_extent;
	uint32_t present_mode_count, i;
	VkPresentModeKHR present_mode;
	VkPresentModeKHR *present_modes;
	uint32_t image_count;
	VkSwapchainCreateInfoKHR desc;
	qboolean mailbox_supported = qfalse;
	qboolean immediate_supported = qfalse;
	qboolean fifo_relaxed_supported = qfalse;
	int v;

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &surface_caps ) );

	image_extent = surface_caps.currentExtent;
	if ( image_extent.width == 0xffffffff && image_extent.height == 0xffffffff ) {
		image_extent.width = MIN( surface_caps.maxImageExtent.width, MAX( surface_caps.minImageExtent.width, (uint32_t) glConfig.vidWidth ) );
		image_extent.height = MIN( surface_caps.maxImageExtent.height, MAX( surface_caps.minImageExtent.height, (uint32_t) glConfig.vidHeight ) );
	}

	vk.clearAttachment = qtrue;

	if ( !vk.fboActive ) {
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
		if ( ( surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
			vk.clearAttachment = qfalse;
			ri.Printf( PRINT_WARNING, "VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain, \\r_clear might not work\n" );
		}
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required in order to take screenshots.
		if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
			ri.Error(ERR_FATAL, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain");
		}
	}

	// determine present mode and swapchain image count
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL));

	present_modes = (VkPresentModeKHR *) ri.Malloc( present_mode_count * sizeof( VkPresentModeKHR ) );
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes));

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...presentation modes:" );
	}
	for ( i = 0; i < present_mode_count; i++ ) {
		if ( verbose ) {
			ri.Printf( PRINT_ALL, " %s", pmode_to_str( present_modes[i] ) );
		}
		if ( present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
			mailbox_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR )
			immediate_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR )
			fifo_relaxed_supported = qtrue;
	}
	if ( verbose ) {
		ri.Printf( PRINT_ALL, "\n" );
	}

	ri.Free( present_modes );

	if ( ( v = ri.Cvar_VariableIntegerValue( "r_swapInterval" ) ) != 0 ) {
		if ( v == 2 && mailbox_supported )
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if ( fifo_relaxed_supported )
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		else
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
	} else {
		if ( immediate_supported ) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_IMM, surface_caps.minImageCount );
		} else if ( mailbox_supported ) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_MAILBOX, surface_caps.minImageCount );
		} else if ( fifo_relaxed_supported ) {
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		} else {
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		}
	}

	if ( image_count < 2 ) {
		image_count = 2;
	}

	if ( surface_caps.maxImageCount == 0 && present_mode == VK_PRESENT_MODE_FIFO_KHR ) {
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_0, surface_caps.minImageCount );
	} else if ( surface_caps.maxImageCount > 0 ) {
		image_count = MIN( MIN( image_count, surface_caps.maxImageCount ), MAX_SWAPCHAIN_IMAGES );
	}

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...selected presentation mode: %s, image count: %i\n", pmode_to_str( present_mode ), image_count );
	}

	// create swap chain
	desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.surface = surface;
	desc.minImageCount = image_count;
	desc.imageFormat = surface_format.format;
	desc.imageColorSpace = surface_format.colorSpace;
	desc.imageExtent = image_extent;
	desc.imageArrayLayers = 1;
	desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( !vk.fboActive ) {
		desc.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.preTransform = surface_caps.currentTransform;
	//desc.compositeAlpha = get_composite_alpha( surface_caps.supportedCompositeAlpha );
	desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	desc.presentMode = present_mode;
	desc.clipped = VK_TRUE;
	desc.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK( qvkCreateSwapchainKHR( device, &desc, NULL, swapchain ) );
	vk_set_hdr_metadata();
	if ( verbose && vk.hdrDisplayActive && !vk.hdrMetadata ) {
		ri.Printf( PRINT_WARNING,
			"...%s is not available, presenting HDR10 without static HDR metadata\n",
			VK_EXT_HDR_METADATA_EXTENSION_NAME );
	}

	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, NULL ) );
	vk.swapchain_image_count = MIN( vk.swapchain_image_count, MAX_SWAPCHAIN_IMAGES );
	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images ) );

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.pNext = NULL;
		view.flags = 0;
		view.image = vk.swapchain_images[i];
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = vk.present_format.format;
		view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &vk.swapchain_image_views[i] ) );

		SET_OBJECT_NAME( vk.swapchain_images[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.swapchain_image_views[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		VkSemaphoreCreateInfo s;
		s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		s.pNext = NULL;
		s.flags = 0;
		VK_CHECK( qvkCreateSemaphore( vk.device, &s, NULL, &vk.swapchain_rendering_finished[i] ) );
		SET_OBJECT_NAME( vk.swapchain_rendering_finished[i], va( "swapchain_rendering_finished semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
	}

	if ( vk.initSwapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		VkCommandBuffer command_buffer = begin_command_buffer();

		for ( i = 0; i < vk.swapchain_image_count; i++ ) {
			record_image_layout_transition( command_buffer, vk.swapchain_images[i],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, vk.initSwapchainLayout, 0, 0 );
		}

		end_command_buffer( command_buffer, __func__ );
	}

	vk_lifetime_mark_create( RTX_VK_RESOURCE_SWAPCHAIN, __func__ );
}


static void vk_create_dlight_shadow_render_pass( VkFormat depth_format )
{
	VkAttachmentDescription attachment;
	VkAttachmentReference depthRef;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[2];
	VkRenderPassCreateInfo desc;

	if ( vk.dlight_shadow_image_view == VK_NULL_HANDLE &&
		vk.spot_shadow_image_view == VK_NULL_HANDLE &&
		vk.csm_shadow_image_view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &attachment, 0, sizeof( attachment ) );
	attachment.format = depth_format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	depthRef.attachment = 0;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &depthRef;

	Com_Memset( deps, 0, sizeof( deps ) );
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pAttachments = &attachment;
	desc.attachmentCount = 1;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.pDependencies = deps;
	desc.dependencyCount = ARRAY_LEN( deps );

	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL,
		&vk.render_pass.dlight_shadow ) );
	SET_OBJECT_NAME( vk.render_pass.dlight_shadow,
		"render pass - sampled shadow atlas",
		VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
}


static void vk_create_render_passes( void )
{
	VkAttachmentDescription attachments[3]; // color | depth | msaa color
	VkAttachmentReference colorResolveRef;
	VkAttachmentReference colorRef0;
	VkAttachmentReference depthRef0;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[3];
	VkSubpassDependency fogDeps[2];
	VkRenderPassCreateInfo desc;
	VkFormat depth_format;
	VkDevice device;
	const qboolean bloomEnabled = ( r_bloom && r_bloom->integer ) ? qtrue : qfalse;
	const qboolean motionBlurEnabled =
		( r_motionBlur && r_motionBlur->integer ) ? qtrue : qfalse;
	const qboolean globalFogEnabled = vk_global_fog_enabled();
	const qboolean depthFadeActive = vk_depth_fade_supported();
	const qboolean liquidCaptureActive =
		( vk.fboActive && vk_liquid_requested() ) ? qtrue : qfalse;
	const qboolean shadowAtlasActive =
		( vk.dlight_shadow_image_view != VK_NULL_HANDLE ||
		  vk.spot_shadow_image_view != VK_NULL_HANDLE ||
		  vk.csm_shadow_image_view != VK_NULL_HANDLE ) ? qtrue : qfalse;
	/* The submerged view reads the finished depth after the main pass, so it has
	 * to keep the attachment alive on its own account as well; otherwise a
	 * configuration with no other depth consumer stores nothing and the
	 * absorption samples discarded contents. */
	const qboolean needsPostMainPass =
		( bloomEnabled || motionBlurEnabled || globalFogEnabled || depthFadeActive ||
			vk_underwater_depth_enabled() ||
			liquidCaptureActive || shadowAtlasActive ||
			vk_rt_trace_available() ) ? qtrue : qfalse;
	uint32_t i;

	depth_format = vk.depth_format;
	device = vk.device;

	if ( r_fbo->integer == 0 )
	{
		// presentation
		attachments[0].flags = 0;
		attachments[0].format = vk.present_format.format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for presentation
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = vk.initSwapchainLayout;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else
	{
		// resolve/color buffer
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

#ifdef USE_BUFFER_CLEAR
		if ( vk.msaaActive )
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
		else
			attachments[ 0 ].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[ 0 ].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
#endif

		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	// depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vkSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	if ( needsPostMainPass ) {
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep it for post-main pass (bloom/RT handoff)
		attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
	} else {
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;

	desc.subpassCount = 1;
	desc.attachmentCount = 2;

	if ( vk.msaaActive )
	{
		attachments[2].flags = 0;
		attachments[2].format = vk.color_format;
		attachments[2].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		if ( needsPostMainPass ) {
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep it for post-main pass (bloom/RT handoff)
		} else {
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Intermediate storage (not written)
		}
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // msaa image attachment
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorResolveRef.attachment = 0; // resolve image attachment
		colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	// subpass dependencies

	Com_Memset( &deps, 0, sizeof( deps ) );

	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;			// What access scopes are influence the dependency
	deps[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// What access scopes are waiting on the dependency
	deps[2].dependencyFlags = 0;

	if ( r_fbo->integer == 0 )
	{
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
		SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].stencilLoadOp = glConfig.stencilBits ?
			VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL,
			&vk.render_pass.main_load ) );
		SET_OBJECT_NAME( vk.render_pass.main_load,
			"render pass - main load",
			VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		vk_create_dlight_shadow_render_pass( depth_format );

		return;
	}

	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
	SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[1].stencilLoadOp = glConfig.stencilBits ?
		VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	if ( vk.msaaActive ) {
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	}
	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL,
		&vk.render_pass.main_load ) );
	SET_OBJECT_NAME( vk.render_pass.main_load, "render pass - main load",
		VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	vk_create_dlight_shadow_render_pass( depth_format );

	if ( needsPostMainPass ) {

		// post-bloom pass
		// color buffer
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // load from previous pass
		 // depth buffer; a view traced after an earlier view of the frame
		 // ended the main pass draws its base in this pass, and its overlay
		 // then loads that depth after the trace
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].storeOp =
			( motionBlurEnabled || globalFogEnabled || depthFadeActive ||
			  liquidCaptureActive || vk_rt_trace_available() ) ?
			VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].stencilStoreOp =
			( ( motionBlurEnabled || globalFogEnabled || depthFadeActive ||
				liquidCaptureActive || vk_rt_trace_available() ) &&
			  glConfig.stencilBits ) ?
				VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if ( vk.msaaActive ) {
			// msaa render target
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.post_bloom ) );
		SET_OBJECT_NAME( vk.render_pass.post_bloom, "render pass - post_bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( globalFogEnabled ) {
		/*
		 * Global fog samples the completed scene depth, so it uses a
		 * color-only load pass between two depth-owning main/post passes.
		 */
		Com_Memset( &attachments[0], 0, sizeof( attachments[0] ) );
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		Com_Memset( fogDeps, 0, sizeof( fogDeps ) );
		fogDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		fogDeps[0].dstSubpass = 0;
		fogDeps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		fogDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		fogDeps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		fogDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		fogDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		fogDeps[1].srcSubpass = 0;
		fogDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		fogDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		fogDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		fogDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		fogDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		fogDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.attachmentCount = 1;
		desc.pAttachments = attachments;
		desc.subpassCount = 1;
		desc.pSubpasses = &subpass;
		desc.dependencyCount = 2;
		desc.pDependencies = fogDeps;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.global_fog ) );
		SET_OBJECT_NAME( vk.render_pass.global_fog, "render pass - global fog", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( bloomEnabled ) {
		// bloom extraction, using resolved/main fbo as a source
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.bloom_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.bloom_extract ) );
		SET_OBJECT_NAME( vk.render_pass.bloom_extract, "render pass - bloom_extract", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ )
		{
			VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.blur[i] ) );
			SET_OBJECT_NAME( vk.render_pass.blur[i], va( "render pass - blur %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}
	}

	if ( vk.fboActive ) {
		// Menu soft focus. Independent of r_bloom: the effect is gated by the
		// client's cl_menuBlur, which the renderer cannot consult when it
		// allocates from the fixed attachment pool, so the pyramid is always
		// available and costs a few megabytes.
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// fully overwritten
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// sampled by the next step
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.menu_blur ) );
		SET_OBJECT_NAME( vk.render_pass.menu_blur, "render pass - menu soft focus", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( motionBlurEnabled ) {
		/* Single-sample scratch target for the camera-motion blur kernel. */
		desc.attachmentCount = 1;
		desc.dependencyCount = 2;
		desc.pDependencies = &deps[0];

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		desc.pSubpasses = &subpass;

		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL,
			&vk.render_pass.motion_blur ) );
		SET_OBJECT_NAME( vk.render_pass.motion_blur,
			"render pass - motion blur",
			VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( vk_underwater_requested() ) {
		/* Single-sample scratch target for the submerged-view composite. */
		desc.attachmentCount = 1;
		desc.dependencyCount = 2;
		desc.pDependencies = &deps[0];

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		desc.pSubpasses = &subpass;

		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL,
			&vk.render_pass.underwater ) );
		SET_OBJECT_NAME( vk.render_pass.underwater,
			"render pass - underwater view",
			VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	// capture render pass
	if ( vk.capture.image )
	{
		Com_Memset( &subpass, 0, sizeof( subpass ) );

		attachments[0].flags = 0;
		attachments[0].format = vk.capture_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // this will be completely overwritten
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.capture ) );
		SET_OBJECT_NAME( vk.render_pass.capture, "render pass - capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	desc.attachmentCount = 1;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;

	// gamma post-processing
	attachments[0].flags = 0;
	attachments[0].format = vk.present_format.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // needed for presentation
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = vk.initSwapchainLayout;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.gamma ) );
	SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// screenmap
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	// screenmap resolve/color buffer
	attachments[0].flags = 0;
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
	if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Assuming this will be completely overwritten
#endif
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// screenmap depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vk.screenMapSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.attachmentCount = 2;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {

		attachments[2].flags = 0;
		attachments[2].format = vk.color_format;
		attachments[2].samples = vk.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // screenmap msaa image attachment
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorResolveRef.attachment = 0; // screenmap resolve image attachment
		colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.screenmap ) );

	SET_OBJECT_NAME( vk.render_pass.screenmap, "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	if ( liquidCaptureActive ) {
		VkAttachmentDescription liquidAttachment;
		VkAttachmentReference liquidColorRef;
		VkSubpassDescription liquidSubpass;
		VkSubpassDependency liquidDeps[2];
		VkRenderPassCreateInfo liquidDesc;

		Com_Memset( &liquidAttachment, 0, sizeof( liquidAttachment ) );
		liquidAttachment.format = vk.color_format;
		liquidAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		liquidAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		liquidAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		liquidAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		liquidAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		liquidAttachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		liquidAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		liquidColorRef.attachment = 0;
		liquidColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &liquidSubpass, 0, sizeof( liquidSubpass ) );
		liquidSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		liquidSubpass.colorAttachmentCount = 1;
		liquidSubpass.pColorAttachments = &liquidColorRef;

		Com_Memset( liquidDeps, 0, sizeof( liquidDeps ) );
		liquidDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		liquidDeps[0].dstSubpass = 0;
		liquidDeps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		liquidDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		liquidDeps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		liquidDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		liquidDeps[1].srcSubpass = 0;
		liquidDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		liquidDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		liquidDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		liquidDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		liquidDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		Com_Memset( &liquidDesc, 0, sizeof( liquidDesc ) );
		liquidDesc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		liquidDesc.attachmentCount = 1;
		liquidDesc.pAttachments = &liquidAttachment;
		liquidDesc.subpassCount = 1;
		liquidDesc.pSubpasses = &liquidSubpass;
		liquidDesc.dependencyCount = ARRAY_LEN( liquidDeps );
		liquidDesc.pDependencies = liquidDeps;

		VK_CHECK( qvkCreateRenderPass( device, &liquidDesc, NULL,
			&vk.render_pass.liquid_snapshot ) );
		SET_OBJECT_NAME( vk.render_pass.liquid_snapshot,
			"render pass - liquid snapshot",
			VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}
	vk_lifetime_mark_create( RTX_VK_RESOURCE_RENDER_PASSES, __func__ );
}


static void allocate_and_bind_image_memory(VkImage image) {
	VkMemoryRequirements memory_requirements;
	VkDeviceSize alignment;
	ImageChunk *chunk;
	int i;

	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if ( memory_requirements.size > vk.image_chunk_size ) {
		ri.Error( ERR_FATAL, "Vulkan: could not allocate memory, image is too large (%ikbytes).",
			(int)(memory_requirements.size/1024) );
	}

	chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	alignment = memory_requirements.alignment;
	for ( i = 0; i < vk_world.num_image_chunks; i++ ) {
		// ensure that memory region has proper alignment
		VkDeviceSize offset = PAD( vk_world.image_chunks[i].used, alignment );

		if ( offset + memory_requirements.size <= vk.image_chunk_size ) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL) {
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS) {
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached" );
		}

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = vk.image_chunk_size;
		alloc_info.memoryTypeIndex = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		SET_OBJECT_NAME( memory, va( "image memory chunk %i", vk_world.num_image_chunks ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

		vk_world.num_image_chunks++;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}


static void vk_clean_staging_buffer( void )
{
	if ( vk_world.staging_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk_world.staging_buffer, NULL );
		vk_world.staging_buffer = VK_NULL_HANDLE;
	}

	if ( vk_world.staging_buffer_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk_world.staging_buffer_memory, NULL );
		vk_world.staging_buffer_memory = VK_NULL_HANDLE;
	}

	vk_world.staging_buffer_ptr = NULL;
	vk_world.staging_buffer_size = 0;
#ifdef USE_UPLOAD_QUEUE
	vk_world.staging_buffer_offset = 0;
#endif
}


#ifdef USE_UPLOAD_QUEUE
static void vk_wait_staging_buffer( void )
{
	if ( vk.aux_fence_wait )
	{
		VkResult res;
		res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		vk.aux_fence_wait = qfalse;
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
	}
}


static void vk_submit_staging_buffer( qboolean final )
{
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkQueue submitQueue = ( vk.async_queue_available && vk_rt_async_overlap_enabled() ) ? vk.async_queue : vk.queue;
	VkSemaphore waits;
	VkSubmitInfo submit_info;
	VkResult res;

	if ( vk_world.staging_buffer_offset == 0 ) {
		return;
	}

	//ri.Printf( PRINT_WARNING, S_COLOR_CYAN ">>> flush %i bytes (final=%i)<<<\n", (int)vk_world.staging_buffer_offset, final );

	vk_world.staging_buffer_offset = 0;
	vk.rt.stats.texture_stream_flushes++;

	VK_CHECK( qvkEndCommandBuffer( vk.staging_command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;

	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		// first call after previous queue submission?
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.staging_command_buffer;

	if ( vk.image_uploaded != VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "Vulkan: incorrect state during image upload" );
	}
	if ( final ) {
		// final submission before recording
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.image_uploaded2;
		vk.image_uploaded = vk.image_uploaded2;
		VK_CHECK( qvkQueueSubmit( submitQueue, 1, &submit_info, vk.aux_fence ) );
		vk.aux_fence_wait = qtrue;
		if ( submitQueue != vk.queue ) {
			vk.rt.stats.async_overlap_submits++;
		}
	} else {
		// if submission before another upload then do explicit wait
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
		VK_CHECK( qvkQueueSubmit( submitQueue, 1, &submit_info, vk.aux_fence ) );
		res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
		if ( submitQueue != vk.queue ) {
			vk.rt.stats.async_overlap_submits++;
		}
	}
}
#endif // USE_UPLOAD_QUEUE


static void ensure_staging_buffer_allocation( VkDeviceSize size )
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkDeviceSize budgetCap = 0;
	uint32_t memory_type;
	void *data;

#ifdef USE_UPLOAD_QUEUE
	if ( vk_world.staging_buffer_size - vk_world.staging_buffer_offset >= size ) {
		return;
	}

	vk_submit_staging_buffer( qfalse );

	if ( vk_world.staging_buffer_size /* - vk_world.staging_buffer_offset */ >= size ) {
		return;
	}
#else
	if ( vk_world.staging_buffer_size >= size ) {
		return;
	}
#endif
	vk_clean_staging_buffer();

	vk_world.staging_buffer_size = MAX( size, STAGING_BUFFER_SIZE );
	budgetCap = (VkDeviceSize)vk_rt_texture_stream_budget_mb() * 1024u * 1024u;
	if ( budgetCap > 0 ) {
		// Keep staging growth bounded by configured streaming budget while still satisfying this allocation.
		vk_world.staging_buffer_size = MIN( vk_world.staging_buffer_size, MAX( size, budgetCap ) );
	}
	vk_world.staging_buffer_size = PAD( vk_world.staging_buffer_size, 1024 * 1024 );

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = vk_world.staging_buffer_size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk_world.staging_buffer));

	qvkGetBufferMemoryRequirements( vk.device, vk_world.staging_buffer, &memory_requirements );

	memory_type = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk_world.staging_buffer_memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk_world.staging_buffer, vk_world.staging_buffer_memory, 0));

	VK_CHECK(qvkMapMemory(vk.device, vk_world.staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk_world.staging_buffer_ptr = (byte*)data;
#ifdef USE_UPLOAD_QUEUE
	vk_world.staging_buffer_offset = 0;
#endif
	SET_OBJECT_NAME( vk_world.staging_buffer, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk_world.staging_buffer_memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


#ifdef USE_VK_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
	int32_t message_code, const char* layer_prefix, const char* message, void* user_data) {
	if ( message && message[0] ) {
		ri.Printf( PRINT_WARNING, "Vulkan validation (%s): %s\n",
			( layer_prefix && layer_prefix[0] ) ? layer_prefix : "unknown",
			message );
	}
	return VK_FALSE;
}
#endif


static qboolean used_instance_extension( const char *ext )
{
	const char *u;

	// allow all VK_*_surface extensions
	u = strrchr( ext, '_' );
	if ( u && Q_stricmp( u + 1, "surface" ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_DISPLAY_EXTENSION_NAME ) == 0 )
		return qtrue; // needed for KMSDRM instances/devices?

	if ( Q_stricmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 )
		return qtrue;

#ifdef USE_VK_VALIDATION
	if ( vk_validation_requested() && Q_stricmp( ext, VK_EXT_DEBUG_REPORT_EXTENSION_NAME ) == 0 )
		return qtrue;
#endif

	if ( Q_stricmp( ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 )
		return qtrue;

	return qfalse;
}


static void create_instance( void )
{
#ifdef USE_VK_VALIDATION
	const char* validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
	const char* validation_layer_name2 = "VK_LAYER_KHRONOS_validation";
	const char* active_validation_layer = NULL;
#endif
	const qboolean wantValidation = vk_validation_requested();
	VkInstanceCreateInfo desc;
	VkInstanceCreateFlags flags;
	VkExtensionProperties *extension_properties;
	VkResult res;
	VkResult versionResult;
	const char **extension_names;
	uint32_t i, n, count, extension_count;
	uint32_t loaderApiVersion;
	VkApplicationInfo appInfo;

	flags = 0;
	count = 0;
	extension_count = 0;
	vk_instance_swapchain_colorspace = qfalse;
	s_vkValidationLayerEnabled = qfalse;
	VK_CHECK(qvkEnumerateInstanceExtensionProperties(NULL, &count, NULL));

	extension_properties = (VkExtensionProperties *)ri.Malloc(sizeof(VkExtensionProperties) * count);
	extension_names = (const char**)ri.Malloc(sizeof(char *) * count);

	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &count, extension_properties ) );
	for ( i = 0; i < count; i++ ) {
		const char *ext = extension_properties[i].extensionName;

		if ( !used_instance_extension( ext ) ) {
			continue;
		}

		// search for duplicates
		for ( n = 0; n < extension_count; n++ ) {
			if ( Q_stricmp( ext, extension_names[ n ] ) == 0 ) {
				break;
			}
		}
		if ( n != extension_count ) {
			continue;
		}

		extension_names[ extension_count++ ] = ext;

		if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 ) {
			flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}

		if ( Q_stricmp( ext, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME ) == 0 ) {
			vk_instance_swapchain_colorspace = qtrue;
		}

		ri.Printf(PRINT_DEVELOPER, "instance extension: %s\n", ext);
	}

	loaderApiVersion = VK_API_VERSION_1_0;
	if ( qvkEnumerateInstanceVersion ) {
		versionResult = qvkEnumerateInstanceVersion( &loaderApiVersion );
		if ( versionResult != VK_SUCCESS ) {
			ri.Printf( PRINT_WARNING,
				"Vulkan: could not query loader API version (%s); using Vulkan 1.0\n",
				vk_result_string( versionResult ) );
			loaderApiVersion = VK_API_VERSION_1_0;
		}
	}

	if ( VK_VERSION_MAJOR( loaderApiVersion ) > 1 ||
		( VK_VERSION_MAJOR( loaderApiVersion ) == 1 && VK_VERSION_MINOR( loaderApiVersion ) >= 2 ) ) {
		s_vkInstanceApiVersion = VK_API_VERSION_1_2;
	} else if ( VK_VERSION_MAJOR( loaderApiVersion ) == 1 &&
		VK_VERSION_MINOR( loaderApiVersion ) >= 1 ) {
		s_vkInstanceApiVersion = VK_API_VERSION_1_1;
	} else {
		s_vkInstanceApiVersion = VK_API_VERSION_1_0;
	}

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = NULL; // Q3_VERSION;
	appInfo.applicationVersion = 0x0;
	appInfo.pEngineName = NULL;
	appInfo.engineVersion = 0x0;
	appInfo.apiVersion = s_vkInstanceApiVersion;

	ri.Printf( PRINT_DEVELOPER, "Vulkan: loader API %u.%u, requesting %u.%u\n",
		VK_VERSION_MAJOR( loaderApiVersion ), VK_VERSION_MINOR( loaderApiVersion ),
		VK_VERSION_MAJOR( s_vkInstanceApiVersion ), VK_VERSION_MINOR( s_vkInstanceApiVersion ) );

	// create instance
	desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = flags;
	desc.pApplicationInfo = &appInfo;
	desc.enabledExtensionCount = extension_count;
	desc.ppEnabledExtensionNames = extension_names;

#ifdef USE_VK_VALIDATION
	if ( wantValidation ) {
		desc.enabledLayerCount = 1;
		desc.ppEnabledLayerNames = &validation_layer_name;
		res = qvkCreateInstance( &desc, NULL, &vk_instance );

		if ( res == VK_SUCCESS ) {
			active_validation_layer = validation_layer_name;
		}
		else if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {
			desc.enabledLayerCount = 1;
			desc.ppEnabledLayerNames = &validation_layer_name2;
			res = qvkCreateInstance( &desc, NULL, &vk_instance );

			if ( res == VK_SUCCESS ) {
				active_validation_layer = validation_layer_name2;
			}
			else if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {
				ri.Printf( PRINT_WARNING, "Vulkan: requested validation layers are not available\n" );

				// try without validation layer
				desc.enabledLayerCount = 0;
				desc.ppEnabledLayerNames = NULL;
				res = qvkCreateInstance( &desc, NULL, &vk_instance );
			}
		}
	} else {
		desc.enabledLayerCount = 0;
		desc.ppEnabledLayerNames = NULL;
		res = qvkCreateInstance( &desc, NULL, &vk_instance );
	}

	if ( active_validation_layer ) {
		s_vkValidationLayerEnabled = qtrue;
	}
#else
	if ( wantValidation ) {
		ri.Printf( PRINT_WARNING, "Vulkan: rtx_debug_vk_validation is enabled, but this build does not include validation-layer support\n" );
	}
	desc.enabledLayerCount = 0;
	desc.ppEnabledLayerNames = NULL;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );
#endif

	ri.Free( (void*)extension_names );
	ri.Free( extension_properties );

	if ( res != VK_SUCCESS ) {
		ri.Error( ERR_FATAL, "Vulkan: instance creation failed with %s", vk_result_string( res ) );
	}

#ifdef USE_VK_VALIDATION
	if ( s_vkValidationLayerEnabled ) {
		ri.Printf( PRINT_ALL, "Vulkan: validation layer enabled (%s)\n", active_validation_layer );
	} else if ( wantValidation ) {
		ri.Printf( PRINT_WARNING, "Vulkan: running without validation layers for this instance\n" );
	}
#endif
}


static VkFormat get_depth_format( VkPhysicalDevice physical_device ) {
	VkFormatProperties props;
	VkFormat formats[2];
	int i;

	if ( glConfig.stencilBits > 0 ) {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
	} else {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
	}

	for ( i = 0; i < ARRAY_LEN( formats ); i++ ) {
		qvkGetPhysicalDeviceFormatProperties( physical_device, formats[i], &props );
		if ( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0 ) {
			return formats[i];
		}
	}

	ri.Error( ERR_FATAL, "get_depth_format: failed to find depth attachment format" );
	return VK_FORMAT_UNDEFINED; // never get here
}


// Check if we can use vkCmdBlitImage for the given source and destination image formats.
static qboolean vk_blit_enabled( VkPhysicalDevice physical_device, const VkFormat srcFormat, const VkFormat dstFormat )
{
	VkFormatProperties formatProps;

	qvkGetPhysicalDeviceFormatProperties( physical_device, srcFormat, &formatProps );
	if ( ( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == 0 ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( physical_device, dstFormat, &formatProps );
	if ( ( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		return qfalse;
	}

	return qtrue;
}


static qboolean vk_hdr_scene_format_supported( VkPhysicalDevice physical_device,
	VkFormat format )
{
	VkFormatProperties properties;
	/*
	 * The scene target is sampled by post/capture and may receive the RT
	 * reconstruction blit. Final SDR capture no longer blits from it, so
	 * requiring BLIT_SRC would reject otherwise valid RGBA16F devices.
	 */
	const VkFormatFeatureFlags requiredFeatures =
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
		VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
		VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
		VK_FORMAT_FEATURE_BLIT_DST_BIT;

	qvkGetPhysicalDeviceFormatProperties( physical_device, format, &properties );
	return ( properties.optimalTilingFeatures & requiredFeatures ) ==
		requiredFeatures ? qtrue : qfalse;
}


static VkFormat get_hdr_format( VkPhysicalDevice physical_device,
	VkFormat base_format )
{
	static qboolean warnedUnsupportedHdrFormat = qfalse;
	int precision;

	if ( r_fbo->integer == 0 ) {
		return base_format;
	}

	if ( r_hdr && r_hdr->integer > 0 ) {
		const VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

		if ( vk_hdr_scene_format_supported( physical_device, hdrFormat ) ) {
			return hdrFormat;
		}
		if ( !warnedUnsupportedHdrFormat ) {
			ri.Printf( PRINT_WARNING,
				"RTX: r_hdr requested floating-point scene storage but %s lacks the required color, sampling, transfer, and blit features; using %s SDR storage\n",
				vk_format_string( hdrFormat ), vk_format_string( base_format ) );
			warnedUnsupportedHdrFormat = qtrue;
		}
		return base_format;
	}

	precision = r_hdrPrecision ? r_hdrPrecision->integer : 0;
	switch ( precision ) {
	case -1:
		return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
	case 8:
		return base_format;
	case 16:
		return VK_FORMAT_R16G16B16A16_UNORM;
	default:
		break;
	}

	if ( r_hdr && r_hdr->integer < 0 ) {
		return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
	}
	return base_format;
}


/*
 * r_hdr 1 on its own is the high-precision display-referred path, as it is in
 * GLx and Vulkan (docs/DISPLAY.md): textures, blending and lighting stay in
 * the space Quake III authored them for. Scene-linear colour is the opt-in
 * GLx's FBO_HdrSceneLinearColorMode makes it -- sRGB texture decode plus a
 * non-legacy tone mapper, colour grading or an active HDR presentation -- so
 * default translucent effects and 2D edges blend as they do everywhere else.
 */
qboolean vk_scene_linear_enabled( void )
{
	if ( !r_fbo || !r_fbo->integer || !r_hdr || r_hdr->integer <= 0 ||
		!r_srgbTextures || !r_srgbTextures->integer ||
		vk.color_format != VK_FORMAT_R16G16B16A16_SFLOAT ) {
		return qfalse;
	}
	return ( ( r_tonemap && r_tonemap->integer > 0 ) ||
		( r_colorGrade && r_colorGrade->integer > 0 ) ||
		vk.hdrDisplayActive ) ? qtrue : qfalse;
}


typedef struct {
	int bits;
	VkFormat rgb;
	VkFormat bgr;
} present_format_t;

static const present_format_t present_formats[] = {
	//{12, VK_FORMAT_B4G4R4A4_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16},
	//{15, VK_FORMAT_B5G5R5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16},
	{16, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16},
	{24, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
	{30, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32},
	//{32, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32}
};

static void get_present_format( int present_bits, VkFormat *bgr, VkFormat *rgb ) {
	const present_format_t *pf, *sel;
	int i;

	sel = NULL;
	pf = present_formats;
	for ( i = 0; i < ARRAY_LEN( present_formats ); i++, pf++ ) {
		if ( pf->bits <= present_bits  ) {
			sel = pf;
		}
	}
	if ( !sel ) {
		*bgr = VK_FORMAT_B8G8R8A8_UNORM;
		*rgb = VK_FORMAT_R8G8B8A8_UNORM;
	} else {
		*bgr = sel->bgr;
		*rgb = sel->rgb;
	}
}


static qboolean vk_find_surface_format( const VkSurfaceFormatKHR *candidates, uint32_t count,
	VkFormat bgr, VkFormat rgb, VkColorSpaceKHR colorSpace, VkSurfaceFormatKHR *out )
{
	uint32_t i;

	for ( i = 0; i < count; i++ ) {
		if ( ( candidates[i].format == bgr || candidates[i].format == rgb ) &&
			candidates[i].colorSpace == colorSpace ) {
			*out = candidates[i];
			return qtrue;
		}
	}

	return qfalse;
}


static qboolean vk_find_hdr10_surface_format( const VkSurfaceFormatKHR *candidates,
	uint32_t count, VkSurfaceFormatKHR *out )
{
	static const VkFormat hdr_formats[] = {
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32
	};
	uint32_t i;

	for ( i = 0; i < ARRAY_LEN( hdr_formats ); i++ ) {
		uint32_t j;

		for ( j = 0; j < count; j++ ) {
			if ( candidates[j].format == hdr_formats[i] &&
				candidates[j].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ) {
				*out = candidates[j];
				return qtrue;
			}
		}
	}

	return qfalse;
}


static void vk_init_display_output_defaults( rendererDisplayOutput_t *output )
{
	if ( !output ) {
		return;
	}

	Com_Memset( output, 0, sizeof( *output ) );
	output->displayIndex = -1;
	output->nativeBackend = ROUTPUT_BACKEND_SDR_SRGB;
	output->sdrWhiteNits = 203.0f;
	output->hdrHeadroom = 1.0f;
	output->maxLuminanceNits = 203.0f;
	output->maxFullFrameLuminanceNits = 203.0f;
	Q_strncpyz( output->reason, "display output query unavailable", sizeof( output->reason ) );
}


static rendererOutputRequest_t vk_output_request( void )
{
	const int request = r_outputBackend ? r_outputBackend->integer : ROUTPUT_REQUEST_AUTO;

	if ( request < ROUTPUT_REQUEST_AUTO || request >= ROUTPUT_REQUEST_COUNT ) {
		return ROUTPUT_REQUEST_AUTO;
	}

	return (rendererOutputRequest_t)request;
}


static void vk_query_display_output( void )
{
	vk_init_display_output_defaults( &vk.displayOutput );
	if ( ri.GLimp_QueryDisplayOutput ) {
		ri.GLimp_QueryDisplayOutput( &vk.displayOutput );
	}
}


static qboolean vk_output_request_wants_hdr10( void )
{
	vk.outputRequest = vk_output_request();

	switch ( vk.outputRequest ) {
	case ROUTPUT_REQUEST_SDR_SRGB:
		return qfalse;
	case ROUTPUT_REQUEST_HDR10_PQ:
		return qtrue;
	case ROUTPUT_REQUEST_LINUX_EXPERIMENTAL_HDR:
		return ( r_outputAllowExperimentalLinuxHDR &&
			r_outputAllowExperimentalLinuxHDR->integer &&
			vk.displayOutput.linuxHdrExperimental &&
			vk.displayOutput.explicitLinuxHdrProtocol ) ? qtrue : qfalse;
	case ROUTPUT_REQUEST_AUTO:
		return ( r_hdrDisplay && r_hdrDisplay->integer ) ? qtrue : qfalse;
	case ROUTPUT_REQUEST_WINDOWS_SCRGB:
	case ROUTPUT_REQUEST_MACOS_EDR:
	default:
		return qfalse;
	}
}


static qboolean vk_select_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface )
{
	VkFormat base_bgr, base_rgb;
	VkFormat ext_bgr, ext_rgb;
	VkSurfaceFormatKHR hdr10_format;
	VkSurfaceFormatKHR *candidates;
	uint32_t format_count;
	VkResult res;

	res = qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, NULL );
	if ( res < 0 ) {
		ri.Printf( PRINT_ERROR, "vkGetPhysicalDeviceSurfaceFormatsKHR returned %s\n", vk_result_string( res ) );
		return qfalse;
	}

	if ( format_count == 0 ) {
		ri.Printf( PRINT_ERROR, "...no surface formats found\n" );
		return qfalse;
	}

	candidates = (VkSurfaceFormatKHR*)ri.Malloc( format_count * sizeof(VkSurfaceFormatKHR) );

	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, candidates ) );

	get_present_format( 24, &base_bgr, &base_rgb );

	if ( r_fbo->integer ) {
		get_present_format( r_presentBits->integer, &ext_bgr, &ext_rgb );
	} else {
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	if ( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) {
		// special case that means we can choose any format
		vk.base_format.format = base_bgr;
		vk.base_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		vk.present_format.format = ext_bgr;
		vk.present_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
	else {
		if ( !vk_find_surface_format( candidates, format_count, base_bgr, base_rgb,
			VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, &vk.base_format ) ) {
			vk.base_format = candidates[0];
		}

		if ( !vk_find_surface_format( candidates, format_count, ext_bgr, ext_rgb,
			VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, &vk.present_format ) ) {
			vk.present_format = vk.base_format;
		}
	}

	vk_query_display_output();
	vk.outputBackend = ROUTPUT_BACKEND_SDR_SRGB;
	vk.hdrDisplayActive = qfalse;
	if ( vk_output_request_wants_hdr10() ) {
		if ( !r_fbo->integer ) {
			ri.Printf( PRINT_WARNING,
				"...native HDR presentation requires \\r_fbo 1, using SDR presentation\n" );
		} else if ( !vk.swapchainColorspace ) {
			ri.Printf( PRINT_WARNING, "...%s is not available, using SDR presentation\n",
				VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME );
		} else if ( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) {
			ri.Printf( PRINT_WARNING,
				"...surface reports arbitrary SDR format selection only, using SDR presentation\n" );
		} else if ( vk_find_hdr10_surface_format( candidates, format_count, &hdr10_format ) ) {
			vk.present_format = hdr10_format;
			vk.hdrDisplayActive = qtrue;
			vk.outputBackend = ROUTPUT_BACKEND_HDR10_PQ;
		} else {
			ri.Printf( PRINT_WARNING,
				"...HDR10 ST2084 surface format is not available, using SDR presentation\n" );
		}
	}

	if ( !r_fbo->integer ) {
		vk.present_format = vk.base_format;
	}

	ri.Free( candidates );

	return qtrue;
}


static void setup_surface_formats( VkPhysicalDevice physical_device )
{
	VkFormatProperties depthFormatProperties;

	vk.depth_format = get_depth_format( physical_device );
	qvkGetPhysicalDeviceFormatProperties( physical_device, vk.depth_format, &depthFormatProperties );
	vk.globalFogDepthSampleSupported =
		( depthFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT ) != 0
			? qtrue : qfalse;
	vk.liquidDepthSampleSupported = vk.globalFogDepthSampleSupported;

	vk.color_format = get_hdr_format( physical_device, vk.base_format.format );

	vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	vk.bloom_format = vk_scene_linear_enabled() ?
		VK_FORMAT_R16G16B16A16_SFLOAT : vk.base_format.format;

	if ( r_fbo->integer ) {
		vk.blitEnabled = vk_blit_enabled( physical_device,
			vk.capture_format, vk.capture_format );
	} else {
		vk.blitEnabled = vk_blit_enabled( physical_device,
			vk.present_format.format, vk.capture_format );
		if ( !vk.blitEnabled ) {
			/*
			 * vkCmdCopyImage requires matching formats. Preserve the
			 * swapchain channel order for direct non-FBO readback when a
			 * present-to-RGBA blit is unavailable.
			 */
			vk.capture_format = vk.present_format.format;
		}
	}

	if ( r_globalFog && r_globalFog->integer && !vk.globalFogDepthSampleSupported ) {
		ri.Printf( PRINT_WARNING,
			"RTX: depth format %s cannot be sampled; per-map global fog will be disabled on this device\n",
			vk_format_string( vk.depth_format ) );
	}
	if ( vk_depth_fade_requested() && !vk.liquidDepthSampleSupported ) {
		ri.Printf( PRINT_WARNING,
			"RTX: depth format %s cannot be sampled; soft-particle depth fade will use authored blending only\n",
			vk_format_string( vk.depth_format ) );
	}
	if ( vk_liquid_requested() && !vk.liquidDepthSampleSupported ) {
		ri.Printf( PRINT_WARNING,
			"RTX: depth format %s cannot be sampled; enhanced liquids will keep refraction/reflection but disable foreground depth rejection\n",
			vk_format_string( vk.depth_format ) );
	}
}


static const char *renderer_name( const VkPhysicalDeviceProperties *props ) {
	static char buf[sizeof( props->deviceName ) + 64];
	const char *device_type;

	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
		default: device_type = "OTHER"; break;
	}

	Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x",
		device_type, props->deviceName, props->deviceID );

	return buf;
}


static qboolean vk_query_rt_device_features(
	VkPhysicalDevice physical_device,
	qboolean queryBufferDeviceAddress,
	qboolean queryDescriptorIndexing,
	qboolean queryAccelerationStructure,
	qboolean queryRayQuery,
	qboolean queryRayTracingPipeline,
	qboolean *bufferDeviceAddress,
	qboolean *descriptorIndexing,
	qboolean *accelerationStructure,
	qboolean *rayQuery,
	qboolean *rayTracingPipeline )
{
	VkPhysicalDeviceFeatures2 features2;
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures;
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures;
	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures;
	void **pNextPtr;

	*bufferDeviceAddress = qfalse;
	*descriptorIndexing = qfalse;
	*accelerationStructure = qfalse;
	*rayQuery = qfalse;
	*rayTracingPipeline = qfalse;

	if ( qvkGetPhysicalDeviceFeatures2 == NULL &&
		qvkGetPhysicalDeviceFeatures2KHR == NULL ) {
		return qfalse;
	}

	Com_Memset( &features2, 0, sizeof( features2 ) );
	Com_Memset( &bufferDeviceAddressFeatures, 0, sizeof( bufferDeviceAddressFeatures ) );
	Com_Memset( &descriptorIndexingFeatures, 0, sizeof( descriptorIndexingFeatures ) );
	Com_Memset( &accelerationStructureFeatures, 0, sizeof( accelerationStructureFeatures ) );
	Com_Memset( &rayQueryFeatures, 0, sizeof( rayQueryFeatures ) );
	Com_Memset( &rayTracingPipelineFeatures, 0, sizeof( rayTracingPipelineFeatures ) );

	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	pNextPtr = (void **)&features2.pNext;

	if ( queryBufferDeviceAddress ) {
		bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		*pNextPtr = &bufferDeviceAddressFeatures;
		pNextPtr = (void **)&bufferDeviceAddressFeatures.pNext;
	}
	if ( queryDescriptorIndexing ) {
		descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		*pNextPtr = &descriptorIndexingFeatures;
		pNextPtr = (void **)&descriptorIndexingFeatures.pNext;
	}
	if ( queryAccelerationStructure ) {
		accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		*pNextPtr = &accelerationStructureFeatures;
		pNextPtr = (void **)&accelerationStructureFeatures.pNext;
	}
	if ( queryRayQuery ) {
		rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		*pNextPtr = &rayQueryFeatures;
		pNextPtr = (void **)&rayQueryFeatures.pNext;
	}
	if ( queryRayTracingPipeline ) {
		rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		*pNextPtr = &rayTracingPipelineFeatures;
	}

	if ( qvkGetPhysicalDeviceFeatures2 != NULL ) {
		qvkGetPhysicalDeviceFeatures2( physical_device, &features2 );
	} else {
		qvkGetPhysicalDeviceFeatures2KHR( physical_device, &features2 );
	}

	*bufferDeviceAddress = bufferDeviceAddressFeatures.bufferDeviceAddress ? qtrue : qfalse;
	*descriptorIndexing = descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing ? qtrue : qfalse;
	*accelerationStructure = accelerationStructureFeatures.accelerationStructure ? qtrue : qfalse;
	*rayQuery = rayQueryFeatures.rayQuery ? qtrue : qfalse;
	*rayTracingPipeline = rayTracingPipelineFeatures.rayTracingPipeline ? qtrue : qfalse;
	return qtrue;
}


/*
=================
vk_alice_line_rasterization

American McGee's Alice draws its game's debug lines with each line's own
stipple and width (alice.exe 0x004768ad..0x004768fe). Vulkan stipples only
through VK_EXT_line_rasterization's stippled Bresenham lines, or stippled
rectangular lines on a device with strict lines; this finds which, for the
Alice profile alone, and names the features to enable. Without them the
lines are drawn solid, and without wideLines one pixel wide.
=================
*/
static VkPhysicalDeviceLineRasterizationFeaturesEXT vk_alice_line_features;

static qboolean vk_alice_line_rasterization( VkPhysicalDevice physical_device )
{
	VkPhysicalDeviceFeatures2 features2;
	VkPhysicalDeviceLineRasterizationFeaturesEXT supported;
	VkPhysicalDeviceProperties props;
	VkExtensionProperties *extensions;
	uint32_t count = 0, i;
	qboolean found = qfalse;

	vk.lineStipple = qfalse;
	Com_Memset( &vk_alice_line_features, 0, sizeof( vk_alice_line_features ) );
	if ( !COM_GAME_ID_IS_RITUAL( ri.gameId ) ||
		( qvkGetPhysicalDeviceFeatures2 == NULL && qvkGetPhysicalDeviceFeatures2KHR == NULL ) ) {
		return qfalse;
	}
	if ( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, NULL ) != VK_SUCCESS || !count ) {
		return qfalse;
	}
	extensions = (VkExtensionProperties *)ri.Malloc( count * sizeof( *extensions ) );
	if ( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, extensions ) == VK_SUCCESS ) {
		for ( i = 0; i < count; i++ ) {
			if ( !strcmp( extensions[i].extensionName, VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME ) ) {
				found = qtrue;
				break;
			}
		}
	}
	ri.Free( extensions );
	if ( !found ) {
		return qfalse;
	}

	Com_Memset( &features2, 0, sizeof( features2 ) );
	Com_Memset( &supported, 0, sizeof( supported ) );
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &supported;
	supported.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
	if ( qvkGetPhysicalDeviceFeatures2 != NULL ) {
		qvkGetPhysicalDeviceFeatures2( physical_device, &features2 );
	} else {
		qvkGetPhysicalDeviceFeatures2KHR( physical_device, &features2 );
	}
	qvkGetPhysicalDeviceProperties( physical_device, &props );

	vk_alice_line_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
	if ( supported.bresenhamLines && supported.stippledBresenhamLines ) {
		vk_alice_line_features.bresenhamLines = VK_TRUE;
		vk_alice_line_features.stippledBresenhamLines = VK_TRUE;
		vk.lineStippleMode = VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;
	} else if ( supported.rectangularLines && supported.stippledRectangularLines && props.limits.strictLines ) {
		vk_alice_line_features.rectangularLines = VK_TRUE;
		vk_alice_line_features.stippledRectangularLines = VK_TRUE;
		vk.lineStippleMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT;
	} else {
		return qfalse;
	}
	return qtrue;
}


static qboolean vk_create_device( VkPhysicalDevice physical_device, int device_index ) {

	VkPhysicalDeviceBufferDeviceAddressFeatures devaddr_features;
	VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_struct_features;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_features;
	VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features;

#ifdef _DEBUG
	VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore;
	VkPhysicalDeviceVulkanMemoryModelFeatures memory_model;
	VkPhysicalDevice8BitStorageFeatures storage_8bit_features;
#endif

	ri.Printf( PRINT_ALL, "...selected physical device: %i\n", device_index );
	vk.hdrMetadata = qfalse;

	// select surface format
	if ( !vk_select_surface_format( physical_device, vk_surface ) ) {
		return qfalse;
	}

	setup_surface_formats( physical_device );

	// select queue family
	{
		VkQueueFamilyProperties *queue_families;
		uint32_t queue_family_count;
		uint32_t i;

		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, NULL );
		queue_families = (VkQueueFamilyProperties*)ri.Malloc( queue_family_count * sizeof( VkQueueFamilyProperties ) );
		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, queue_families );

		// select queue family with presentation and graphics support
		vk.queue_family_index = ~0U;
		vk.queue_family_queue_count = 0;
		for (i = 0; i < queue_family_count; i++) {
			VkBool32 presentation_supported;
			VK_CHECK( qvkGetPhysicalDeviceSurfaceSupportKHR( physical_device, i, vk_surface, &presentation_supported ) );

			if (presentation_supported && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				vk.queue_family_index = i;
				vk.queue_family_queue_count = queue_families[i].queueCount;
				break;
			}
		}

		ri.Free( queue_families );

		if ( vk.queue_family_index == ~0U ) {
			ri.Printf( PRINT_ERROR, "...failed to find graphics queue family\n" );

			return qfalse;
		}
	}

	// create VkDevice
	{
		const char *device_extension_list[24];
		uint32_t device_extension_count;
		const char *ext, *end;
		char *str;
		const float priorities[2] = { 1.0f, 1.0f };
		VkExtensionProperties *extension_properties;
		VkDeviceQueueCreateInfo queue_desc;
		VkPhysicalDeviceFeatures device_features;
		VkPhysicalDeviceFeatures features;
		VkDeviceCreateInfo device_desc;
		VkResult res;
		qboolean swapchainSupported = qfalse;
		qboolean portabilitySubset = qfalse;
		qboolean dedicatedAllocation = qfalse;
		qboolean memoryRequirements2 = qfalse;
		qboolean debugMarker = qfalse;
		qboolean hdrMetadata = qfalse;
		qboolean descriptorIndexingExtension = qfalse;
		qboolean descriptorIndexing = qfalse;
		qboolean timelineSemaphore = qfalse;
		qboolean memoryModel = qfalse;
		qboolean bufferDeviceAddressExtension = qfalse;
		qboolean devAddrFeat = qfalse;
		qboolean deferredHostOperations = qfalse;
		qboolean accelerationStructure = qfalse;
		qboolean rayQuery = qfalse;
		qboolean rayTracingPipeline = qfalse;
		qboolean spirv14Extension = qfalse;
		qboolean shaderFloatControlsExtension = qfalse;
		qboolean spirv14 = qfalse;
		qboolean shaderFloatControls = qfalse;
		qboolean core12 = qfalse;
		qboolean enableBufferDeviceAddress = qfalse;
		qboolean featureQueryAvailable = qfalse;
		qboolean bufferDeviceAddressFeature = qfalse;
		qboolean descriptorIndexingFeature = qfalse;
		qboolean accelerationStructureFeature = qfalse;
		qboolean rayQueryFeature = qfalse;
		qboolean rayTracingPipelineFeature = qfalse;
		VkPhysicalDeviceProperties physicalDeviceProperties;
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProperties;
		VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;
		VkFormatProperties rtOutputFormatProperties;
		VkFormatProperties rtColorFormatProperties;
		qboolean rtOutputFormatSupported = qfalse;
		qboolean hardwareRt = qfalse;
		qboolean computeRt = qfalse;
		qboolean rtPropertiesAvailable = qfalse;
		qboolean rtShaderGroupLayoutValid = qfalse;
		VkDeviceSize rtShaderGroupStride = 0;
		const int markerMode = vk_debug_markers_mode();
		const void **pNextPtr;
#ifdef _DEBUG
		qboolean storage8bit = qfalse;
#endif
		uint32_t i, len, count = 0;

		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, NULL ) );
		extension_properties = (VkExtensionProperties*)ri.Malloc( count * sizeof( VkExtensionProperties ) );
		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, extension_properties ) );

		// fill glConfig.extensions_string
		str = glConfig.extensions_string; *str = '\0';
		end = &glConfig.extensions_string[ sizeof( glConfig.extensions_string ) - 1];

		for ( i = 0; i < count; i++ ) {
			ext = extension_properties[i].extensionName;
			if ( strcmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 ) {
				swapchainSupported = qtrue;
			} else if ( strcmp( ext, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME ) == 0 ) {
				portabilitySubset = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME ) == 0 ) {
				dedicatedAllocation = qtrue;
			} else if ( strcmp( ext, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME ) == 0 ) {
				memoryRequirements2 = qtrue;
			} else if ( strcmp( ext, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				debugMarker = qtrue;
			} else if ( strcmp( ext, VK_EXT_HDR_METADATA_EXTENSION_NAME ) == 0 ) {
				hdrMetadata = qtrue;
			} else if ( strcmp( ext, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME ) == 0 ) {
				timelineSemaphore = qtrue;
			} else if ( strcmp( ext, VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME ) == 0 ) {
				memoryModel = qtrue;
			} else if ( strcmp( ext, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) == 0 ) {
				bufferDeviceAddressExtension = qtrue;
			} else if ( strcmp( ext, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME ) == 0 ) {
				descriptorIndexingExtension = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME ) == 0 ) {
				deferredHostOperations = qtrue;
			} else if ( strcmp( ext, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME ) == 0 ) {
				accelerationStructure = qtrue;
			} else if ( strcmp( ext, VK_KHR_RAY_QUERY_EXTENSION_NAME ) == 0 ) {
				rayQuery = qtrue;
			} else if ( strcmp( ext, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME ) == 0 ) {
				rayTracingPipeline = qtrue;
			} else if ( strcmp( ext, VK_KHR_SPIRV_1_4_EXTENSION_NAME ) == 0 ) {
				spirv14Extension = qtrue;
			} else if ( strcmp( ext, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME ) == 0 ) {
				shaderFloatControlsExtension = qtrue;
#ifdef _DEBUG
			} else if ( strcmp( ext, VK_KHR_8BIT_STORAGE_EXTENSION_NAME ) == 0 ) {
				storage8bit = qtrue;
#endif
			}
			// add this device extension to glConfig
			if ( i != 0 ) {
				if ( str + 1 >= end )
					continue;
				str = Q_stradd( str, " " );
			}
			len = (uint32_t)strlen( ext );
			if ( str + len >= end )
				continue;
			str = Q_stradd( str, ext );
		}

		ri.Free( extension_properties );

		qvkGetPhysicalDeviceProperties( physical_device, &physicalDeviceProperties );
		core12 = ( VK_VERSION_MAJOR( s_vkInstanceApiVersion ) > 1 ||
			( VK_VERSION_MAJOR( s_vkInstanceApiVersion ) == 1 &&
				VK_VERSION_MINOR( s_vkInstanceApiVersion ) >= 2 ) ) &&
			( VK_VERSION_MAJOR( physicalDeviceProperties.apiVersion ) > 1 ||
				( VK_VERSION_MAJOR( physicalDeviceProperties.apiVersion ) == 1 &&
					VK_VERSION_MINOR( physicalDeviceProperties.apiVersion ) >= 2 ) )
				? qtrue : qfalse;

		featureQueryAvailable = vk_query_rt_device_features(
			physical_device,
			bufferDeviceAddressExtension || core12,
			descriptorIndexingExtension || core12,
			accelerationStructure,
			rayQuery,
			rayTracingPipeline,
			&bufferDeviceAddressFeature,
			&descriptorIndexingFeature,
			&accelerationStructureFeature,
			&rayQueryFeature,
			&rayTracingPipelineFeature );

		devAddrFeat = ( ( bufferDeviceAddressExtension || core12 ) && bufferDeviceAddressFeature ) ? qtrue : qfalse;
		descriptorIndexing = ( ( descriptorIndexingExtension || core12 ) && descriptorIndexingFeature ) ? qtrue : qfalse;
		accelerationStructure = ( accelerationStructure && accelerationStructureFeature ) ? qtrue : qfalse;
		rayQuery = ( rayQuery && rayQueryFeature ) ? qtrue : qfalse;
		rayTracingPipeline = ( rayTracingPipeline && rayTracingPipelineFeature ) ? qtrue : qfalse;
		spirv14 = ( spirv14Extension || core12 ) ? qtrue : qfalse;
		shaderFloatControls = ( shaderFloatControlsExtension || core12 ) ? qtrue : qfalse;
		Com_Memset( &rtPipelineProperties, 0, sizeof( rtPipelineProperties ) );
		Com_Memset( &accelerationStructureProperties, 0, sizeof( accelerationStructureProperties ) );
		if ( ( qvkGetPhysicalDeviceProperties2 || qvkGetPhysicalDeviceProperties2KHR ) &&
			rayTracingPipeline ) {
			VkPhysicalDeviceProperties2 properties2;

			Com_Memset( &properties2, 0, sizeof( properties2 ) );
			properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			rtPipelineProperties.sType =
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
			rtPipelineProperties.pNext = &accelerationStructureProperties;
			accelerationStructureProperties.sType =
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
			properties2.pNext = &rtPipelineProperties;
			if ( qvkGetPhysicalDeviceProperties2 ) {
				qvkGetPhysicalDeviceProperties2( physical_device, &properties2 );
			} else {
				qvkGetPhysicalDeviceProperties2KHR( physical_device, &properties2 );
			}
			rtPropertiesAvailable = qtrue;
			rtShaderGroupLayoutValid =
				rtPipelineProperties.shaderGroupHandleSize > 0 &&
				vk_rt_is_power_of_two( rtPipelineProperties.shaderGroupHandleAlignment ) &&
				vk_rt_is_power_of_two( rtPipelineProperties.shaderGroupBaseAlignment ) &&
				vk_rt_align_up_checked( rtPipelineProperties.shaderGroupHandleSize,
					rtPipelineProperties.shaderGroupHandleAlignment, &rtShaderGroupStride ) &&
				rtShaderGroupStride <= rtPipelineProperties.maxShaderGroupStride
					? qtrue : qfalse;
		}
		qvkGetPhysicalDeviceFormatProperties(
			physical_device,
			VK_FORMAT_R16G16B16A16_SFLOAT,
			&rtOutputFormatProperties );
		qvkGetPhysicalDeviceFormatProperties(
			physical_device,
			vk.color_format,
			&rtColorFormatProperties );
		// the composite is a storage image blitted (nearest) into scene colour
		rtOutputFormatSupported =
			( rtOutputFormatProperties.optimalTilingFeatures &
				( VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT ) ) ==
				( VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT ) &&
			( rtColorFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) != 0
				? qtrue : qfalse;

		device_extension_count = 0;

		if ( !swapchainSupported ) {
			ri.Printf( PRINT_ERROR, "...required device extension is not available: %s\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME );
			return qfalse;
		}

		vk.caps.swapchain = swapchainSupported;
		vk.caps.memoryRequirements2 = memoryRequirements2;
		vk.caps.descriptorIndexing = descriptorIndexing;
		vk.caps.bufferDeviceAddress = devAddrFeat;
		vk.caps.timelineSemaphore = timelineSemaphore;
		vk.caps.vulkanMemoryModel = memoryModel;
		vk.caps.deferredHostOperations = deferredHostOperations;
		vk.caps.accelerationStructure = accelerationStructure;
		vk.caps.rayQuery = rayQuery;
		vk.caps.rayTracingPipeline = rayTracingPipeline;

		if ( !memoryRequirements2 )
			dedicatedAllocation = qfalse;
		else
			vk.dedicatedAllocation = dedicatedAllocation;

#ifndef USE_DEDICATED_ALLOCATION
		vk.dedicatedAllocation = qfalse;
#endif
		vk.caps.dedicatedAllocation = vk.dedicatedAllocation;

		{
			const int requestedRtModeRaw = vk_requested_rt_mode();
			const int requestedRtMode = ( requestedRtModeRaw < RTX_RT_MODE_DISABLED || requestedRtModeRaw > RTX_RT_MODE_RAY_TRACING_PIPELINE )
				? RTX_RT_MODE_DISABLED : requestedRtModeRaw;
			int activeRtMode = RTX_RT_MODE_DISABLED;
			int rtBackend = RTX_RT_BACKEND_NONE;
			const int requestedBackend = vk_requested_rt_backend();
			char missingCaps[256];
			char computeMissing[256];

			missingCaps[0] = '\0';
			computeMissing[0] = '\0';

			if ( requestedRtMode == RTX_RT_MODE_RAY_QUERY || requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE ) {
				if ( !featureQueryAvailable ) {
					Q_strcat( missingCaps, sizeof( missingCaps ), "vkGetPhysicalDeviceFeatures2" );
				}
				if ( !accelerationStructure ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME );
				}
				if ( !descriptorIndexing ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME );
				}
				if ( !deferredHostOperations ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME );
				}
				if ( !devAddrFeat ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_QUERY && !rayQuery ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), VK_KHR_RAY_QUERY_EXTENSION_NAME );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE && !rayTracingPipeline ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE && !spirv14 ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), VK_KHR_SPIRV_1_4_EXTENSION_NAME );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE && !shaderFloatControls ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
					physicalDeviceProperties.limits.maxPushConstantsSize < sizeof( rtDenoisePush_t ) ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), "maxPushConstantsSize" );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
					( physicalDeviceProperties.limits.maxPerStageDescriptorStorageBuffers < RTX_RT_STORAGE_BUFFER_COUNT ||
						physicalDeviceProperties.limits.maxDescriptorSetStorageBuffers < RTX_RT_STORAGE_BUFFER_COUNT ) ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), "storage-buffer descriptor limits" );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
					( physicalDeviceProperties.limits.maxPerStageDescriptorStorageImages < RTX_RT_STORAGE_IMAGE_COUNT ||
						physicalDeviceProperties.limits.maxDescriptorSetStorageImages < RTX_RT_STORAGE_IMAGE_COUNT ) ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), "storage-image descriptor limits" );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
					( physicalDeviceProperties.limits.maxPerStageDescriptorSampledImages < RTX_RT_SAMPLED_IMAGE_COUNT ||
						physicalDeviceProperties.limits.maxDescriptorSetSampledImages < RTX_RT_SAMPLED_IMAGE_COUNT ||
						physicalDeviceProperties.limits.maxPerStageDescriptorSamplers < RTX_RT_SAMPLED_IMAGE_COUNT ||
						physicalDeviceProperties.limits.maxDescriptorSetSamplers < RTX_RT_SAMPLED_IMAGE_COUNT ) ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), "sampled-image descriptor limits" );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
					physicalDeviceProperties.limits.maxPerStageResources <
						RTX_RT_PER_STAGE_RESOURCE_COUNT ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), "maxPerStageResources" );
				}
				// every ray starts in ray generation: a recursion depth of one
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
					( !rtPropertiesAvailable ||
						rtPipelineProperties.maxRayRecursionDepth < 1 ||
						!rtShaderGroupLayoutValid ) ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), "ray-tracing pipeline limits" );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE &&
					( !rtPropertiesAvailable ||
						accelerationStructureProperties.maxPerStageDescriptorAccelerationStructures < RTX_RT_ACCELERATION_STRUCTURE_COUNT ||
						accelerationStructureProperties.maxDescriptorSetAccelerationStructures < RTX_RT_ACCELERATION_STRUCTURE_COUNT ) ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), "acceleration-structure descriptor limits" );
				}
				if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE && !rtOutputFormatSupported ) {
					if ( missingCaps[0] ) Q_strcat( missingCaps, sizeof( missingCaps ), ", " );
					Q_strcat( missingCaps, sizeof( missingCaps ), "RT output composition format support" );
				}
			}

			/* The compute backend needs no acceleration structures: the
			 * texture table's non-uniform indexing, the SPIR-V 1.4 its compute
			 * shaders are built for, and the trace's descriptors in the
			 * compute stage. */
			if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE ) {
				if ( !featureQueryAvailable ) {
					Q_strcat( computeMissing, sizeof( computeMissing ), "vkGetPhysicalDeviceFeatures2" );
				}
				if ( !descriptorIndexing ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME );
				}
				if ( !spirv14 ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), VK_KHR_SPIRV_1_4_EXTENSION_NAME );
				}
				if ( !shaderFloatControls ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME );
				}
				if ( physicalDeviceProperties.limits.maxPushConstantsSize < sizeof( rtDenoisePush_t ) ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), "maxPushConstantsSize" );
				}
				if ( physicalDeviceProperties.limits.maxPerStageDescriptorStorageBuffers < RTX_RT_COMPUTE_STORAGE_BUFFER_COUNT ||
					physicalDeviceProperties.limits.maxDescriptorSetStorageBuffers < RTX_RT_COMPUTE_STORAGE_BUFFER_COUNT ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), "storage-buffer descriptor limits" );
				}
				if ( physicalDeviceProperties.limits.maxPerStageDescriptorStorageImages < RTX_RT_STORAGE_IMAGE_COUNT ||
					physicalDeviceProperties.limits.maxDescriptorSetStorageImages < RTX_RT_STORAGE_IMAGE_COUNT ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), "storage-image descriptor limits" );
				}
				if ( physicalDeviceProperties.limits.maxPerStageDescriptorSampledImages < RTX_RT_SAMPLED_IMAGE_COUNT ||
					physicalDeviceProperties.limits.maxDescriptorSetSampledImages < RTX_RT_SAMPLED_IMAGE_COUNT ||
					physicalDeviceProperties.limits.maxPerStageDescriptorSamplers < RTX_RT_SAMPLED_IMAGE_COUNT ||
					physicalDeviceProperties.limits.maxDescriptorSetSamplers < RTX_RT_SAMPLED_IMAGE_COUNT ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), "sampled-image descriptor limits" );
				}
				if ( physicalDeviceProperties.limits.maxPerStageResources < RTX_RT_COMPUTE_PER_STAGE_RESOURCE_COUNT ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), "maxPerStageResources" );
				}
				if ( !rtOutputFormatSupported ) {
					if ( computeMissing[0] ) Q_strcat( computeMissing, sizeof( computeMissing ), ", " );
					Q_strcat( computeMissing, sizeof( computeMissing ), "RT output composition format support" );
				}

				if ( requestedBackend != RTX_RT_BACKEND_COMPUTE && missingCaps[0] == '\0' ) {
					rtBackend = RTX_RT_BACKEND_HARDWARE;
				} else if ( requestedBackend != RTX_RT_BACKEND_HARDWARE && computeMissing[0] == '\0' ) {
					rtBackend = RTX_RT_BACKEND_COMPUTE;
					if ( requestedBackend == RTX_RT_BACKEND_NONE ) {
						ri.Printf( PRINT_ALL, "Vulkan: the ray-tracing pipeline is unavailable (missing: %s); "
							"the path tracer traces with compute shaders\n", missingCaps );
					}
				} else {
					// what the requested backend, or each backend, lacks
					char both[256];
					if ( requestedBackend == RTX_RT_BACKEND_COMPUTE ) {
						Q_strncpyz( both, computeMissing, sizeof( both ) );
					} else if ( requestedBackend == RTX_RT_BACKEND_NONE ) {
						Com_sprintf( both, sizeof( both ), "%s; compute backend: %s", missingCaps, computeMissing );
					} else {
						Q_strncpyz( both, missingCaps, sizeof( both ) );
					}
					Q_strncpyz( missingCaps, both, sizeof( missingCaps ) );
				}
			}

			if ( requestedRtMode == RTX_RT_MODE_DISABLED ) {
				activeRtMode = RTX_RT_MODE_DISABLED;
			} else if ( requestedRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE ? rtBackend != RTX_RT_BACKEND_NONE :
				missingCaps[0] == '\0' ) {
				activeRtMode = requestedRtMode;
			} else if ( vk_rt_mode_required() ) {
				ri.Error( ERR_FATAL,
					"Vulkan: requested rtx_rt_mode %s requires unsupported device capabilities: %s\n"
					"Set rtx_rt_mode 0 (fallback mode), or set rtx_rt_require 0 to allow automatic fallback.",
					vk_rt_mode_name( requestedRtMode ), missingCaps );
			} else {
				ri.Printf( PRINT_WARNING,
					"Vulkan: requested rtx_rt_mode %s is unavailable, missing: %s; falling back to disabled mode.\n"
					"Set rtx_rt_require 1 to enforce strict startup failure instead of fallback.\n",
					vk_rt_mode_name( requestedRtMode ), missingCaps );
				activeRtMode = RTX_RT_MODE_DISABLED;
			}

			vk.caps.requestedRtMode = requestedRtMode;
			vk.caps.activeRtMode = activeRtMode;
			vk.caps.rtBackend = ( activeRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE ) ? rtBackend : RTX_RT_BACKEND_NONE;
		}

		device_extension_list[ device_extension_count++ ] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		if ( portabilitySubset ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
		}

		// ray queries and the ray-tracing pipeline need acceleration structures
		hardwareRt = ( vk.caps.activeRtMode == RTX_RT_MODE_RAY_QUERY || vk_rt_pipeline_enabled() ) ? qtrue : qfalse;
		computeRt = ( vk_rt_trace_available() && !vk_rt_pipeline_enabled() ) ? qtrue : qfalse;
		if ( computeRt && !core12 ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_SPIRV_1_4_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME;
		}
		if ( hardwareRt ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;
			if ( !core12 ) {
				device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
				device_extension_list[ device_extension_count++ ] = VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME;
			}
			enableBufferDeviceAddress = qtrue;
			device_extension_list[ device_extension_count++ ] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;

			if ( vk.caps.activeRtMode == RTX_RT_MODE_RAY_QUERY ) {
				device_extension_list[ device_extension_count++ ] = VK_KHR_RAY_QUERY_EXTENSION_NAME;
			}

			if ( vk.caps.activeRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE ) {
				device_extension_list[ device_extension_count++ ] = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME;
				if ( !core12 ) {
					device_extension_list[ device_extension_count++ ] = VK_KHR_SPIRV_1_4_EXTENSION_NAME;
					device_extension_list[ device_extension_count++ ] = VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME;
				}
			}
		}

		if ( vk.dedicatedAllocation ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
		}

		vk.debugMarkers = qfalse;
		if ( debugMarker && markerMode > 0 ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
			vk.debugMarkers = qtrue;
		} else if ( markerMode > 0 && !debugMarker ) {
			ri.Printf( PRINT_WARNING, "Vulkan: GPU debug markers requested, but %s is not available on this device\n", VK_EXT_DEBUG_MARKER_EXTENSION_NAME );
		}
		vk.caps.debugMarkers = vk.debugMarkers;
		if ( hdrMetadata ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_HDR_METADATA_EXTENSION_NAME;
		}
#ifdef _DEBUG
		if ( timelineSemaphore ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
		}

		if ( memoryModel ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME;
		}

		if ( devAddrFeat && vk.caps.activeRtMode == RTX_RT_MODE_DISABLED ) {
			if ( !core12 ) {
				device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
			}
			enableBufferDeviceAddress = qtrue;
		}

		if ( storage8bit ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_8BIT_STORAGE_EXTENSION_NAME;
		}
#endif // _DEBUG
		qvkGetPhysicalDeviceFeatures( physical_device, &device_features );

		if ( device_features.fillModeNonSolid == VK_FALSE ) {
			ri.Printf( PRINT_ERROR, "...fillModeNonSolid feature is not supported\n" );
			return qfalse;
		}

		queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_desc.pNext = NULL;
		queue_desc.flags = 0;
		queue_desc.queueFamilyIndex = vk.queue_family_index;
		queue_desc.queueCount = ( vk_rt_async_overlap_enabled() && vk.queue_family_queue_count > 1 ) ? 2 : 1;
		queue_desc.pQueuePriorities = priorities;

		Com_Memset( &features, 0, sizeof( features ) );
		features.fillModeNonSolid = VK_TRUE;

#ifdef _DEBUG
		if ( device_features.shaderInt64 ) {
			features.shaderInt64 = VK_TRUE;
		}
#endif
		if ( device_features.wideLines ) { // needed for RB_SurfaceAxis
			features.wideLines = VK_TRUE;
			vk.wideLines = qtrue;
		}

		if ( device_features.fragmentStoresAndAtomics && device_features.vertexPipelineStoresAndAtomics ) {
			features.vertexPipelineStoresAndAtomics = VK_TRUE;
			features.fragmentStoresAndAtomics = VK_TRUE;
			vk.fragmentStores = qtrue;
		}

		if ( r_ext_texture_filter_anisotropic->integer && device_features.samplerAnisotropy ) {
			features.samplerAnisotropy = VK_TRUE;
			vk.samplerAnisotropy = qtrue;
		}

		device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_desc.pNext = NULL;
		device_desc.flags = 0;
		device_desc.queueCreateInfoCount = 1;
		device_desc.pQueueCreateInfos = &queue_desc;
		device_desc.enabledLayerCount = 0;
		device_desc.ppEnabledLayerNames = NULL;
		device_desc.enabledExtensionCount = device_extension_count;
		device_desc.ppEnabledExtensionNames = device_extension_list;
		device_desc.pEnabledFeatures = &features;

		pNextPtr = (const void **)&device_desc.pNext;

		if ( enableBufferDeviceAddress ) {
			Com_Memset( &devaddr_features, 0, sizeof( devaddr_features ) );
			*pNextPtr = &devaddr_features;
			devaddr_features.pNext = NULL;
			devaddr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
			devaddr_features.bufferDeviceAddress = VK_TRUE;
			devaddr_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
			devaddr_features.bufferDeviceAddressMultiDevice = VK_FALSE;
			pNextPtr = (const void **)&devaddr_features.pNext;
		}

		if ( hardwareRt || computeRt ) {
			Com_Memset( &descriptor_indexing_features, 0, sizeof( descriptor_indexing_features ) );
			*pNextPtr = &descriptor_indexing_features;
			descriptor_indexing_features.pNext = NULL;
			descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
			descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
			pNextPtr = (const void **)&descriptor_indexing_features.pNext;
		}
		if ( hardwareRt ) {
			Com_Memset( &accel_struct_features, 0, sizeof( accel_struct_features ) );
			*pNextPtr = &accel_struct_features;
			accel_struct_features.pNext = NULL;
			accel_struct_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
			accel_struct_features.accelerationStructure = VK_TRUE;
			accel_struct_features.accelerationStructureCaptureReplay = VK_FALSE;
			accel_struct_features.accelerationStructureIndirectBuild = VK_FALSE;
			accel_struct_features.accelerationStructureHostCommands = VK_FALSE;
			accel_struct_features.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;
			pNextPtr = (const void **)&accel_struct_features.pNext;
		}

		if ( vk.caps.activeRtMode == RTX_RT_MODE_RAY_QUERY ) {
			Com_Memset( &ray_query_features, 0, sizeof( ray_query_features ) );
			*pNextPtr = &ray_query_features;
			ray_query_features.pNext = NULL;
			ray_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
			ray_query_features.rayQuery = VK_TRUE;
			pNextPtr = (const void **)&ray_query_features.pNext;
		}

		if ( vk_rt_pipeline_enabled() ) {
			Com_Memset( &rt_pipeline_features, 0, sizeof( rt_pipeline_features ) );
			*pNextPtr = &rt_pipeline_features;
			rt_pipeline_features.pNext = NULL;
			rt_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
			rt_pipeline_features.rayTracingPipeline = VK_TRUE;
			rt_pipeline_features.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
			rt_pipeline_features.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
			rt_pipeline_features.rayTracingPipelineTraceRaysIndirect = VK_FALSE;
			rt_pipeline_features.rayTraversalPrimitiveCulling = VK_FALSE;
			pNextPtr = (const void **)&rt_pipeline_features.pNext;
		}

#ifdef _DEBUG
		if ( timelineSemaphore ) {
			*pNextPtr = &timeline_semaphore;
			timeline_semaphore.pNext = NULL;
			timeline_semaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
			timeline_semaphore.timelineSemaphore = VK_TRUE;
			pNextPtr = (const void **)&timeline_semaphore.pNext;
		}

		if ( memoryModel ) {
			*pNextPtr = &memory_model;
			memory_model.pNext = NULL;
			memory_model.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
			memory_model.vulkanMemoryModel = VK_TRUE;
			memory_model.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
			memory_model.vulkanMemoryModelDeviceScope = VK_TRUE;
			pNextPtr = (const void **)&memory_model.pNext;
		}

		if ( storage8bit ) {
			*pNextPtr = &storage_8bit_features;
			storage_8bit_features.pNext = NULL;
			storage_8bit_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
			storage_8bit_features.storageBuffer8BitAccess = VK_TRUE;
			storage_8bit_features.storagePushConstant8 = VK_FALSE;
			storage_8bit_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
			pNextPtr = (const void **)&storage_8bit_features.pNext;
		}
#endif
		// Alice's stippled debug lines, where the device draws them
		if ( vk_alice_line_rasterization( physical_device ) &&
			device_extension_count < ARRAY_LEN( device_extension_list ) ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME;
			device_desc.enabledExtensionCount = device_extension_count;
			*pNextPtr = &vk_alice_line_features;
			vk_alice_line_features.pNext = NULL;
			pNextPtr = (const void **)&vk_alice_line_features.pNext;
			vk.lineStipple = qtrue;
		} else {
			vk.lineStipple = qfalse;
		}

		res = qvkCreateDevice( physical_device, &device_desc, NULL, &vk.device );
		if ( res < 0 ) {
			ri.Printf( PRINT_ERROR, "vkCreateDevice returned %s\n", vk_result_string( res ) );
			return qfalse;
		}

		vk.hdrMetadata = hdrMetadata;
	}

	if ( vk.debugMarkers ) {
		ri.Printf( PRINT_ALL, "Vulkan: GPU debug markers enabled (rtx_debug_gpu_markers=%d)\n", vk_debug_markers_mode() );
	} else if ( vk_debug_markers_mode() > 0 ) {
		ri.Printf( PRINT_WARNING, "Vulkan: GPU debug markers requested but unavailable on this device\n" );
	}

	vk_report_capabilities( "startup" );

	return qtrue;
}


static void vk_copy_function_pointer( void *destination, size_t destination_size,
	const void *source, size_t source_size, const char *name )
{
	/* ISO C does not define conversions between data and function pointers, or
	 * between differently typed function pointers. Vulkan guarantees that its
	 * loader entry points are callable through the matching PFN type, so copy
	 * the representation after checking the platform uses equal sizes. */
	if ( destination_size != source_size ) {
		ri.Error( ERR_FATAL, "Vulkan entrypoint %s has an incompatible pointer size", name );
		return;
	}

	Com_Memcpy( destination, source, destination_size );
}

static void vk_load_instance_function( void *destination, size_t destination_size, const char *name )
{
	void *address = ri.VK_GetInstanceProcAddr( vk_instance, name );
	vk_copy_function_pointer( destination, destination_size, &address, sizeof( address ), name );
}

static void vk_load_device_function( void *destination, size_t destination_size, const char *name )
{
	PFN_vkVoidFunction address = qvkGetDeviceProcAddr( vk.device, name );
	vk_copy_function_pointer( destination, destination_size, &address, sizeof( address ), name );
}

#define INIT_INSTANCE_FUNCTION(func) \
	vk_load_instance_function( &q##func, sizeof( q##func ), #func ); \
	if (q##func == NULL) {											\
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	vk_load_instance_function( &q##func, sizeof( q##func ), #func );


#define INIT_DEVICE_FUNCTION(func) \
	vk_load_device_function( &q##func, sizeof( q##func ), #func );\
	if (q##func == NULL) {											\
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_DEVICE_FUNCTION_EXT(func) \
	vk_load_device_function( &q##func, sizeof( q##func ), #func );


static void vk_destroy_instance( void ) {
	if ( vk_surface != VK_NULL_HANDLE ) {
		if ( qvkDestroySurfaceKHR != NULL ) {
			qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		}
		vk_surface = VK_NULL_HANDLE;
	}

#ifdef USE_VK_VALIDATION
	if ( vk_debug_callback ) {
		if ( qvkDestroyDebugReportCallbackEXT != NULL ) {
			qvkDestroyDebugReportCallbackEXT( vk_instance, vk_debug_callback, NULL );
		}
		vk_debug_callback = VK_NULL_HANDLE;
	}
#endif

	if ( vk_instance != VK_NULL_HANDLE ) {
		if ( qvkDestroyInstance ) {
			qvkDestroyInstance( vk_instance, NULL );
		}
		vk_instance = VK_NULL_HANDLE;
	}

	s_vkValidationLayerEnabled = qfalse;
	s_vkInstanceApiVersion = VK_API_VERSION_1_0;
	vk_instance_swapchain_colorspace = qfalse;
}


static void init_vulkan_library( void )
{
	VkPhysicalDeviceProperties props;
	VkPhysicalDevice *physical_devices;
	uint32_t device_count;
	int device_index, i;
	VkResult res;

	Com_Memset( &vk, 0, sizeof( vk ) );

	if ( vk_instance == VK_NULL_HANDLE ) {

		// force cleanup
		vk_destroy_instance();

		// Get functions that do not depend on VkInstance (vk_instance == nullptr at this point).
		INIT_INSTANCE_FUNCTION( vkCreateInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateInstanceExtensionProperties )
		INIT_INSTANCE_FUNCTION_EXT( vkEnumerateInstanceVersion )

		// Get instance level functions.
		create_instance();

		INIT_INSTANCE_FUNCTION( vkCreateDevice )
		INIT_INSTANCE_FUNCTION( vkDestroyInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
		INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
		INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
		INIT_INSTANCE_FUNCTION_EXT( vkGetPhysicalDeviceFeatures2 )
		INIT_INSTANCE_FUNCTION_EXT( vkGetPhysicalDeviceFeatures2KHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
		INIT_INSTANCE_FUNCTION_EXT( vkGetPhysicalDeviceProperties2 )
		INIT_INSTANCE_FUNCTION_EXT( vkGetPhysicalDeviceProperties2KHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
		INIT_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceFormatsKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfacePresentModesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceSupportKHR )

#ifdef USE_VK_VALIDATION
		INIT_INSTANCE_FUNCTION_EXT( vkCreateDebugReportCallbackEXT )
		INIT_INSTANCE_FUNCTION_EXT( vkDestroyDebugReportCallbackEXT )

		// Create debug callback when validation layers are active.
		if ( s_vkValidationLayerEnabled && qvkCreateDebugReportCallbackEXT && qvkDestroyDebugReportCallbackEXT ) {
			VkDebugReportCallbackCreateInfoEXT desc;
			desc.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			desc.pNext = NULL;
			desc.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_ERROR_BIT_EXT;
			desc.pfnCallback = &debug_callback;
			desc.pUserData = NULL;

			VK_CHECK( qvkCreateDebugReportCallbackEXT( vk_instance, &desc, NULL, &vk_debug_callback ) );
		} else if ( s_vkValidationLayerEnabled ) {
			ri.Printf( PRINT_WARNING, "Vulkan: validation layer is enabled, but debug report callback extension is unavailable\n" );
		}
#endif

		// create surface
		if ( !ri.VK_CreateSurface( vk_instance, &vk_surface ) ) {
			ri.Error( ERR_FATAL, "Error creating Vulkan surface" );
			return;
		}
	} // vk_instance == VK_NULL_HANDLE

	vk.swapchainColorspace = vk_instance_swapchain_colorspace;

	res = qvkEnumeratePhysicalDevices( vk_instance, &device_count, NULL );
	if ( device_count == 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: no physical devices found" );
		return;
	}
	else if ( res < 0 ) {
		ri.Error( ERR_FATAL, "vkEnumeratePhysicalDevices returned %s", vk_result_string( res ) );
		return;
	}

	physical_devices = (VkPhysicalDevice*)ri.Malloc( device_count * sizeof( VkPhysicalDevice ) );
	VK_CHECK( qvkEnumeratePhysicalDevices( vk_instance, &device_count, physical_devices ) );

	// initial physical device index
	device_index = r_device->integer;

	ri.Printf( PRINT_ALL, ".......................\nAvailable physical devices:\n" );
	for ( i = 0; i < device_count; i++ ) {
		qvkGetPhysicalDeviceProperties( physical_devices[ i ], &props );
		ri.Printf( PRINT_ALL, " %i: %s\n", i, renderer_name( &props ) );
		if ( device_index == -1 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			device_index = i;
		} else if ( device_index == -2 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
			device_index = i;
		}
	}
	ri.Printf( PRINT_ALL, ".......................\n" );

	vk.physical_device = VK_NULL_HANDLE;
	for ( i = 0; i < device_count; i++, device_index++ ) {
		if ( device_index >= device_count || device_index < 0 ) {
			device_index = 0;
		}
		if ( vk_create_device( physical_devices[ device_index ], device_index ) ) {
			vk.physical_device = physical_devices[ device_index ];
			break;
		}
	}

	ri.Free( physical_devices );

	if ( vk.physical_device == VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "Vulkan: unable to find any suitable physical device" );
		return;
	}

	//
	// Get device level functions.
	//
	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdDispatch)
	INIT_DEVICE_FUNCTION(vkCmdFillBuffer)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdResetQueryPool)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetLineWidth)
	if ( vk.lineStipple ) {
		INIT_DEVICE_FUNCTION_EXT(vkCmdSetLineStippleEXT)
		if ( !qvkCmdSetLineStippleEXT ) {
			vk.lineStipple = qfalse;
		}
	}
	if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {
		ri.Printf( PRINT_ALL, "...Alice debug lines: %s, %s\n",
			vk.wideLines ? "each line's width" : "one pixel wide (no wideLines)",
			vk.lineStipple ? "stippled (VK_EXT_line_rasterization)" : "solid (no stippled VK_EXT_line_rasterization lines)" );
	}
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION(vkCmdWriteTimestamp)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateComputePipelines)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)
	INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
	INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
	INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
	INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
	INIT_DEVICE_FUNCTION(vkQueuePresentKHR)
	INIT_DEVICE_FUNCTION(vkCreateQueryPool)
	INIT_DEVICE_FUNCTION(vkDestroyQueryPool)
	INIT_DEVICE_FUNCTION(vkGetQueryPoolResults)

	if ( vk.dedicatedAllocation ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		if ( !qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR ) {
			vk.dedicatedAllocation = qfalse;
		}
	}

	if ( vk.hdrMetadata ) {
		INIT_DEVICE_FUNCTION_EXT(vkSetHdrMetadataEXT)
		if ( !qvkSetHdrMetadataEXT ) {
			vk.hdrMetadata = qfalse;
		}
	}

	if ( vk_rt_pipeline_enabled() ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferDeviceAddress)
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferDeviceAddressKHR)
		INIT_DEVICE_FUNCTION_EXT(vkCreateAccelerationStructureKHR)
		INIT_DEVICE_FUNCTION_EXT(vkDestroyAccelerationStructureKHR)
		INIT_DEVICE_FUNCTION_EXT(vkGetAccelerationStructureBuildSizesKHR)
		INIT_DEVICE_FUNCTION_EXT(vkCmdBuildAccelerationStructuresKHR)
		INIT_DEVICE_FUNCTION_EXT(vkGetAccelerationStructureDeviceAddressKHR)
		INIT_DEVICE_FUNCTION_EXT(vkCreateRayTracingPipelinesKHR)
		INIT_DEVICE_FUNCTION_EXT(vkGetRayTracingShaderGroupHandlesKHR)
		INIT_DEVICE_FUNCTION_EXT(vkCmdTraceRaysKHR)
		INIT_DEVICE_FUNCTION_EXT(vkCmdCopyAccelerationStructureKHR)
		INIT_DEVICE_FUNCTION_EXT(vkCmdWriteAccelerationStructuresPropertiesKHR)

		if ( ( !qvkGetBufferDeviceAddress && !qvkGetBufferDeviceAddressKHR ) ||
			!qvkCreateAccelerationStructureKHR ||
			!qvkDestroyAccelerationStructureKHR ||
			!qvkGetAccelerationStructureBuildSizesKHR ||
			!qvkCmdBuildAccelerationStructuresKHR ||
			!qvkGetAccelerationStructureDeviceAddressKHR ||
			!qvkCreateRayTracingPipelinesKHR ||
			!qvkGetRayTracingShaderGroupHandlesKHR ||
			!qvkCmdTraceRaysKHR ||
			!qvkCmdCopyAccelerationStructureKHR ||
			!qvkCmdWriteAccelerationStructuresPropertiesKHR ) {
			ri.Error( ERR_FATAL, "Vulkan: ray tracing mode requested, but one or more required RT device entry points are unavailable" );
		}
	}

	if ( vk.debugMarkers ) {
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
		INIT_DEVICE_FUNCTION_EXT(vkCmdDebugMarkerBeginEXT)
		INIT_DEVICE_FUNCTION_EXT(vkCmdDebugMarkerEndEXT)
		INIT_DEVICE_FUNCTION_EXT(vkCmdDebugMarkerInsertEXT)
		if ( vk_debug_markers_mode() >= 2 && ( !qvkCmdDebugMarkerBeginEXT || !qvkCmdDebugMarkerEndEXT ) ) {
			ri.Printf( PRINT_WARNING, "Vulkan: pass markers requested, but debug marker command entry points are unavailable\n" );
		}
	}
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

static void deinit_instance_functions( void )
{
	qvkCreateInstance = NULL;
	qvkEnumerateInstanceExtensionProperties = NULL;
	qvkEnumerateInstanceVersion = NULL;

	// instance functions:
	qvkCreateDevice = NULL;
	qvkDestroyInstance = NULL;
	qvkEnumerateDeviceExtensionProperties = NULL;
	qvkEnumeratePhysicalDevices = NULL;
	qvkGetDeviceProcAddr = NULL;
	qvkGetPhysicalDeviceFeatures = NULL;
	qvkGetPhysicalDeviceFeatures2 = NULL;
	qvkGetPhysicalDeviceFeatures2KHR = NULL;
	qvkGetPhysicalDeviceFormatProperties = NULL;
	qvkGetPhysicalDeviceMemoryProperties = NULL;
	qvkGetPhysicalDeviceProperties = NULL;
	qvkGetPhysicalDeviceProperties2 = NULL;
	qvkGetPhysicalDeviceProperties2KHR = NULL;
	qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
	qvkDestroySurfaceKHR = NULL;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
	qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugReportCallbackEXT = NULL;
	qvkDestroyDebugReportCallbackEXT = NULL;
#endif
}


static void deinit_device_functions( void )
{
	// device functions:
	qvkAllocateCommandBuffers					= NULL;
	qvkAllocateDescriptorSets					= NULL;
	qvkAllocateMemory							= NULL;
	qvkBeginCommandBuffer						= NULL;
	qvkBindBufferMemory							= NULL;
	qvkBindImageMemory							= NULL;
	qvkCmdBeginRenderPass						= NULL;
	qvkCmdBindDescriptorSets					= NULL;
	qvkCmdBindIndexBuffer						= NULL;
	qvkCmdBindPipeline							= NULL;
	qvkCmdBindVertexBuffers						= NULL;
	qvkCmdBlitImage								= NULL;
	qvkCmdClearAttachments						= NULL;
	qvkCmdCopyBuffer							= NULL;
	qvkCmdCopyBufferToImage						= NULL;
	qvkCmdCopyImage								= NULL;
	qvkCmdDispatch								= NULL;
	qvkCmdFillBuffer							= NULL;
	qvkCmdDraw									= NULL;
	qvkCmdDrawIndexed							= NULL;
	qvkCmdEndRenderPass							= NULL;
	qvkCmdNextSubpass							= NULL;
	qvkCmdPipelineBarrier						= NULL;
	qvkCmdPushConstants							= NULL;
	qvkCmdResetQueryPool						= NULL;
	qvkCmdSetDepthBias							= NULL;
	qvkCmdSetScissor							= NULL;
	qvkCmdSetLineWidth							= NULL;
	qvkCmdSetLineStippleEXT						= NULL;
	qvkCmdSetViewport							= NULL;
	qvkCmdWriteTimestamp						= NULL;
	qvkCreateBuffer								= NULL;
	qvkCreateCommandPool						= NULL;
	qvkCreateComputePipelines					= NULL;
	qvkCreateDescriptorPool						= NULL;
	qvkCreateDescriptorSetLayout				= NULL;
	qvkCreateFence								= NULL;
	qvkCreateFramebuffer						= NULL;
	qvkCreateGraphicsPipelines					= NULL;
	qvkCreateImage								= NULL;
	qvkCreateImageView							= NULL;
	qvkCreatePipelineCache						= NULL;
	qvkCreatePipelineLayout						= NULL;
	qvkCreateRenderPass							= NULL;
	qvkCreateSampler							= NULL;
	qvkCreateSemaphore							= NULL;
	qvkCreateShaderModule						= NULL;
	qvkDestroyBuffer							= NULL;
	qvkDestroyCommandPool						= NULL;
	qvkDestroyDescriptorPool					= NULL;
	qvkDestroyDescriptorSetLayout				= NULL;
	qvkDestroyDevice							= NULL;
	qvkDestroyFence								= NULL;
	qvkDestroyFramebuffer						= NULL;
	qvkDestroyImage								= NULL;
	qvkDestroyImageView							= NULL;
	qvkDestroyPipeline							= NULL;
	qvkDestroyPipelineCache						= NULL;
	qvkDestroyPipelineLayout					= NULL;
	qvkDestroyRenderPass						= NULL;
	qvkDestroySampler							= NULL;
	qvkDestroySemaphore							= NULL;
	qvkDestroyShaderModule						= NULL;
	qvkDeviceWaitIdle							= NULL;
	qvkEndCommandBuffer							= NULL;
	qvkFlushMappedMemoryRanges					= NULL;
	qvkFreeCommandBuffers						= NULL;
	qvkFreeDescriptorSets						= NULL;
	qvkFreeMemory								= NULL;
	qvkGetBufferMemoryRequirements				= NULL;
	qvkGetDeviceQueue							= NULL;
	qvkGetImageMemoryRequirements				= NULL;
	qvkGetImageSubresourceLayout				= NULL;
	qvkInvalidateMappedMemoryRanges				= NULL;
	qvkMapMemory								= NULL;
	qvkQueueSubmit								= NULL;
	qvkQueueWaitIdle							= NULL;
	qvkResetCommandBuffer						= NULL;
	qvkResetDescriptorPool						= NULL;
	qvkResetFences								= NULL;
	qvkUnmapMemory								= NULL;
	qvkUpdateDescriptorSets						= NULL;
	qvkWaitForFences							= NULL;
	qvkAcquireNextImageKHR						= NULL;
	qvkCreateSwapchainKHR						= NULL;
	qvkDestroySwapchainKHR						= NULL;
	qvkGetSwapchainImagesKHR					= NULL;
	qvkQueuePresentKHR							= NULL;
	qvkCreateQueryPool							= NULL;
	qvkDestroyQueryPool							= NULL;
	qvkGetQueryPoolResults						= NULL;
	qvkSetHdrMetadataEXT							= NULL;

	qvkGetBufferMemoryRequirements2KHR			= NULL;
	qvkGetImageMemoryRequirements2KHR			= NULL;
	qvkGetBufferDeviceAddress						= NULL;
	qvkGetBufferDeviceAddressKHR				= NULL;
	qvkCreateAccelerationStructureKHR			= NULL;
	qvkDestroyAccelerationStructureKHR			= NULL;
	qvkGetAccelerationStructureBuildSizesKHR	= NULL;
	qvkCmdBuildAccelerationStructuresKHR		= NULL;
	qvkGetAccelerationStructureDeviceAddressKHR	= NULL;
	qvkCreateRayTracingPipelinesKHR				= NULL;
	qvkGetRayTracingShaderGroupHandlesKHR		= NULL;
	qvkCmdTraceRaysKHR							= NULL;
	qvkCmdCopyAccelerationStructureKHR			= NULL;
	qvkCmdWriteAccelerationStructuresPropertiesKHR = NULL;

	qvkDebugMarkerSetObjectNameEXT				= NULL;
	qvkCmdDebugMarkerBeginEXT					= NULL;
	qvkCmdDebugMarkerEndEXT						= NULL;
	qvkCmdDebugMarkerInsertEXT					= NULL;
}


static VkShaderModule SHADER_MODULE(const uint8_t *bytes, const int count) {
	VkShaderModuleCreateInfo desc;
	VkShaderModule module;

	if ( count % 4 != 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not a multiple of 4" );
	}

	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)bytes;

	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));

	return module;
}


static void vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout )
{
	VkDescriptorSetLayoutBinding bind;
	VkDescriptorSetLayoutCreateInfo desc;

	bind.binding = binding;
	bind.descriptorType = type;
	bind.descriptorCount = 1;
	bind.stageFlags = flags;
	bind.pImmutableSamplers = NULL;

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = 1;
	desc.pBindings = &bind;

	VK_CHECK( qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, layout ) );
}

typedef struct {
	rtxVkDescriptorLayoutId_t id;
	int binding;
	VkDescriptorType type;
	VkShaderStageFlags stageFlags;
	const char *debugName;
} rtxVkDescriptorLayoutSpec_t;

static const rtxVkDescriptorLayoutSpec_t s_vkDescriptorLayoutSpecs[] = {
	{ RTX_VK_DESC_LAYOUT_SAMPLER, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, "descriptor layout - sampler" },
	{ RTX_VK_DESC_LAYOUT_UNIFORM, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, "descriptor layout - uniform" },
	{ RTX_VK_DESC_LAYOUT_STORAGE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, "descriptor layout - storage" }
};

static VkDescriptorSetLayout *vk_descriptor_layout_ref( rtxVkDescriptorLayoutId_t id )
{
	switch ( id ) {
	case RTX_VK_DESC_LAYOUT_SAMPLER:
		return &vk.set_layout_sampler;
	case RTX_VK_DESC_LAYOUT_UNIFORM:
		return &vk.set_layout_uniform;
	case RTX_VK_DESC_LAYOUT_STORAGE:
		return &vk.set_layout_storage;
	default:
		ri.Error( ERR_FATAL, "Vulkan: invalid descriptor layout id %i", (int)id );
		return &vk.set_layout_sampler;
	}
}

static VkPipelineLayout *vk_pipeline_layout_ref( rtxVkPipelineLayoutId_t id )
{
	switch ( id ) {
	case RTX_VK_PIPELINE_LAYOUT_MAIN:
		return &vk.pipeline_layout;
	case RTX_VK_PIPELINE_LAYOUT_STORAGE:
		return &vk.pipeline_layout_storage;
	case RTX_VK_PIPELINE_LAYOUT_POST_PROCESS:
		return &vk.pipeline_layout_post_process;
	case RTX_VK_PIPELINE_LAYOUT_BLEND:
		return &vk.pipeline_layout_blend;
	default:
		ri.Error( ERR_FATAL, "Vulkan: invalid pipeline layout id %i", (int)id );
		return &vk.pipeline_layout;
	}
}

static VkPipelineLayout vk_pipeline_layout_handle( rtxVkPipelineLayoutId_t id )
{
	return *vk_pipeline_layout_ref( id );
}

static void vk_create_descriptor_layout_registry( void )
{
	uint32_t i;

	for ( i = 0; i < ARRAY_LEN( s_vkDescriptorLayoutSpecs ); i++ ) {
		const rtxVkDescriptorLayoutSpec_t *spec = &s_vkDescriptorLayoutSpecs[i];
		VkDescriptorSetLayout *layout = vk_descriptor_layout_ref( spec->id );

		vk_create_layout_binding( spec->binding, spec->type, spec->stageFlags, layout );
		SET_OBJECT_NAME( *layout, spec->debugName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT );
	}
}

static void vk_destroy_descriptor_layout_registry( void )
{
	uint32_t i;

	for ( i = 0; i < ARRAY_LEN( s_vkDescriptorLayoutSpecs ); i++ ) {
		const rtxVkDescriptorLayoutSpec_t *spec = &s_vkDescriptorLayoutSpecs[i];
		VkDescriptorSetLayout *layout = vk_descriptor_layout_ref( spec->id );

		if ( *layout != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorSetLayout( vk.device, *layout, NULL );
			*layout = VK_NULL_HANDLE;
		}
	}
}

static void vk_create_pipeline_layout_from_registry(
	rtxVkPipelineLayoutId_t pipelineLayoutId,
	const rtxVkDescriptorLayoutId_t *descriptorLayouts,
	uint32_t descriptorLayoutCount,
	VkShaderStageFlags pushConstantStageFlags,
	uint32_t pushConstantSize,
	const char *debugName )
{
	VkDescriptorSetLayout setLayouts[8];
	VkPipelineLayoutCreateInfo desc;
	VkPushConstantRange pushRange;
	VkPipelineLayout *layout;
	uint32_t i;

	if ( descriptorLayoutCount > ARRAY_LEN( setLayouts ) ) {
		ri.Error( ERR_FATAL, "Vulkan: descriptor layout count %u exceeds registry limit", descriptorLayoutCount );
	}

	layout = vk_pipeline_layout_ref( pipelineLayoutId );

	for ( i = 0; i < descriptorLayoutCount; i++ ) {
		setLayouts[i] = *vk_descriptor_layout_ref( descriptorLayouts[i] );
	}

	desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.setLayoutCount = descriptorLayoutCount;
	desc.pSetLayouts = setLayouts;

	if ( pushConstantSize > 0 ) {
		pushRange.stageFlags = pushConstantStageFlags;
		pushRange.offset = 0;
		pushRange.size = pushConstantSize;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &pushRange;
	} else {
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;
	}

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, layout ) );
	SET_OBJECT_NAME( *layout, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
}

static void vk_create_pipeline_layout_registry( void )
{
	const rtxVkDescriptorLayoutId_t mainDescriptorLayouts[] = {
		RTX_VK_DESC_LAYOUT_UNIFORM,
		RTX_VK_DESC_LAYOUT_SAMPLER,
		RTX_VK_DESC_LAYOUT_SAMPLER,
		RTX_VK_DESC_LAYOUT_SAMPLER,
		RTX_VK_DESC_LAYOUT_SAMPLER
	};
	const rtxVkDescriptorLayoutId_t storageDescriptorLayouts[] = {
		RTX_VK_DESC_LAYOUT_STORAGE
	};
	const rtxVkDescriptorLayoutId_t postProcessDescriptorLayouts[] = {
		RTX_VK_DESC_LAYOUT_SAMPLER,
		RTX_VK_DESC_LAYOUT_SAMPLER
	};
	rtxVkDescriptorLayoutId_t blendDescriptorLayouts[ VK_NUM_BLOOM_PASSES ];
	uint32_t mainDescriptorLayoutCount;
	uint32_t i;

	mainDescriptorLayoutCount = ( vk.maxBoundDescriptorSets >= VK_DESC_COUNT ) ? VK_DESC_COUNT : 4;

	for ( i = 0; i < ARRAY_LEN( blendDescriptorLayouts ); i++ ) {
		blendDescriptorLayouts[i] = RTX_VK_DESC_LAYOUT_SAMPLER;
	}

	vk_create_pipeline_layout_from_registry(
		RTX_VK_PIPELINE_LAYOUT_MAIN,
		mainDescriptorLayouts,
		mainDescriptorLayoutCount,
		VK_SHADER_STAGE_VERTEX_BIT,
		64,
		"pipeline layout - main" );

	vk_create_pipeline_layout_from_registry(
		RTX_VK_PIPELINE_LAYOUT_STORAGE,
		storageDescriptorLayouts,
		ARRAY_LEN( storageDescriptorLayouts ),
		VK_SHADER_STAGE_VERTEX_BIT,
		64,
		"pipeline layout - storage" );

	vk_create_pipeline_layout_from_registry(
		RTX_VK_PIPELINE_LAYOUT_POST_PROCESS,
		postProcessDescriptorLayouts,
		ARRAY_LEN( postProcessDescriptorLayouts ),
		VK_SHADER_STAGE_FRAGMENT_BIT,
		/* The submerged-view compositor is the largest consumer at eight vec4s.
		 * 128 bytes is the Vulkan guaranteed minimum, so every compliant device
		 * accepts this range and the smaller post-process passes ignore the
		 * unused tail. */
		32 * sizeof( float ),
		"pipeline layout - post-processing" );

	vk_create_pipeline_layout_from_registry(
		RTX_VK_PIPELINE_LAYOUT_BLEND,
		blendDescriptorLayouts,
		ARRAY_LEN( blendDescriptorLayouts ),
		0,
		0,
		"pipeline layout - blend" );
}

static void vk_destroy_pipeline_layout_registry( void )
{
	uint32_t i;

	for ( i = 0; i < RTX_VK_PIPELINE_LAYOUT_COUNT; i++ ) {
		VkPipelineLayout *layout = vk_pipeline_layout_ref( (rtxVkPipelineLayoutId_t)i );

		if ( *layout != VK_NULL_HANDLE ) {
			qvkDestroyPipelineLayout( vk.device, *layout, NULL );
			*layout = VK_NULL_HANDLE;
		}
	}
}


void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;

	info.buffer = buffer;
	info.offset = 0;
	info.range = sizeof( vkUniform_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
}


static VkSampler vk_find_sampler( const Vk_Sampler_Def *def ) {
	VkSamplerAddressMode address_mode;
	VkSamplerCreateInfo desc;
	VkSampler sampler;
	VkFilter mag_filter;
	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	qboolean mipmapped;
	float maxLod;
	int i;

	// Look for sampler among existing samplers.
	for ( i = 0; i < vk_world.num_samplers; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk_world.sampler_defs[i];
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			return vk_world.samplers[i];
		}
	}

	// Create new sampler.
	if ( vk_world.num_samplers >= MAX_VK_SAMPLERS ) {
		ri.Error( ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
	}

	address_mode = def->address_mode;

	if (def->gl_mag_filter == GL_NEAREST) {
		mag_filter = VK_FILTER_NEAREST;
	} else if (def->gl_mag_filter == GL_LINEAR) {
		mag_filter = VK_FILTER_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter");
		return VK_NULL_HANDLE;
	}

	maxLod = vk.maxLod;
	mipmapped = qtrue;

	if (def->gl_min_filter == GL_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
		mipmapped = qfalse;
	} else if (def->gl_min_filter == GL_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
		mipmapped = qfalse;
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_min_filter");
		return VK_NULL_HANDLE;
	}

	if ( def->max_lod_1_0 ) {
		maxLod = 1.0f;
	}

	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = 0.0f;

	// Like the GL renderers, filter every mipmapped image anisotropically,
	// whether or not it blends between mip levels: Elite Force's own
	// GL_LINEAR_MIPMAP_NEAREST takes the same anisotropy as trilinear.
	if ( def->noAnisotropy || !mipmapped || mag_filter == VK_FILTER_NEAREST ) {
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	} else {
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		if ( desc.anisotropyEnable ) {
			desc.maxAnisotropy = MIN( r_ext_max_anisotropy->integer, vk.maxAnisotropy );
		}
	}

	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = (maxLod == vk.maxLod) ? VK_LOD_CLAMP_NONE : maxLod;
	desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &sampler ) );

	SET_OBJECT_NAME( sampler, va( "image sampler %i", vk_world.num_samplers ), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	vk_world.sampler_defs[ vk_world.num_samplers ] = *def;
	vk_world.samplers[ vk_world.num_samplers ] = sampler;
	vk_world.num_samplers++;

	return sampler;
}


static void vk_update_shadow_attachment_descriptor( VkImageView imageView,
	VkDescriptorSet descriptor )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	Vk_Sampler_Def samplerDef;

	if ( imageView == VK_NULL_HANDLE || descriptor == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
	samplerDef.gl_mag_filter = GL_NEAREST;
	samplerDef.gl_min_filter = GL_NEAREST;
	samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerDef.max_lod_1_0 = qtrue;
	samplerDef.noAnisotropy = qtrue;

	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = vk_find_sampler( &samplerDef );
	info.imageView = imageView;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;

	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}


static void vk_update_attachment_descriptors( void ) {

	if ( vk.color_image_view )
	{
		VkDescriptorImageInfo info;
		VkWriteDescriptorSet desc;
		Vk_Sampler_Def sd;

		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = vk.blitFilter;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
		info.imageView = vk.color_image_view;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstSet = vk.color_descriptor;
		desc.dstBinding = 0;
		desc.dstArrayElement = 0;
		desc.descriptorCount = 1;
		desc.pNext = NULL;
		desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc.pImageInfo = &info;
		desc.pBufferInfo = NULL;
		desc.pTexelBufferView = NULL;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		if ( vk.motion_blur_image_view != VK_NULL_HANDLE &&
			vk.motion_blur_descriptor != VK_NULL_HANDLE ) {
			info.imageView = vk.motion_blur_image_view;
			desc.dstSet = vk.motion_blur_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );

		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		if ( vk.liquidSnapshot.source_descriptor &&
			vk.liquidSnapshot.color_descriptor ) {
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = vk.color_image_view;
			desc.dstSet = vk.liquidSnapshot.source_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			info.imageView = vk.liquidSnapshot.color_image_view;
			desc.dstSet = vk.liquidSnapshot.color_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

		if ( vk.underwater_image_view && vk.underwater_descriptor ) {
			sd.gl_mag_filter = sd.gl_min_filter = vk.blitFilter;
			sd.max_lod_1_0 = qtrue;
			info.sampler = vk_find_sampler( &sd );
			/* The liquid-snapshot block above only reaches its layout
			 * assignment when the snapshot exists, so state it here rather
			 * than inheriting whatever the previous writer left behind. */
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = vk.underwater_image_view;
			desc.dstSet = vk.underwater_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

		if ( vk.depth_sample_image_view && vk.depth_sample_descriptor ) {
			sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;

			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.depth_sample_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.depth_sample_descriptor;

			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

		// bloom images
		if ( r_bloom->integer )
		{
			uint32_t i;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				info.imageView = vk.bloom_image_view[i];
				desc.dstSet = vk.bloom_image_descriptor[i];

				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}

		// menu soft-focus pyramid; the linear filter is what makes the
		// downsample steps average their source instead of point-picking it.
		{
			uint32_t i;

			Com_Memset( &sd, 0, sizeof( sd ) );
			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;

			info.sampler = vk_find_sampler( &sd );
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			for ( i = 0; i < VK_NUM_MENU_BLUR_IMAGES; i++ )
			{
				if ( vk.menu_blur_image_view[i] == VK_NULL_HANDLE ||
					vk.menu_blur_descriptor[i] == VK_NULL_HANDLE ) {
					continue;
				}
				info.imageView = vk.menu_blur_image_view[i];
				desc.dstSet = vk.menu_blur_descriptor[i];

				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}
	}

	if ( vk.liquidDepth.image_view && vk.liquidDepth.descriptor ) {
		VkDescriptorImageInfo info;
		VkWriteDescriptorSet desc;
		Vk_Sampler_Def sd;

		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
		info.imageView = vk.liquidDepth.image_view;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstSet = vk.liquidDepth.descriptor;
		desc.dstBinding = 0;
		desc.descriptorCount = 1;
		desc.descriptorType =
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc.pImageInfo = &info;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
	}

	vk_update_shadow_attachment_descriptor( vk.dlight_shadow_image_view,
		vk.dlight_shadow_descriptor );
	vk_update_shadow_attachment_descriptor( vk.spot_shadow_image_view,
		vk.spot_shadow_descriptor );
	vk_update_shadow_attachment_descriptor( vk.csm_shadow_image_view,
		vk.csm_shadow_descriptor );
}


void vk_init_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;
	uint32_t i;

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = vk_descriptor_layout_ref( RTX_VK_DESC_LAYOUT_STORAGE );

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.storage.descriptor ) );

	info.buffer = vk.storage.buffer;
	info.offset = 0;
	info.range = sizeof( uint32_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.storage.descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	// allocated and update descriptor set
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = vk_descriptor_layout_ref( RTX_VK_DESC_LAYOUT_UNIFORM );

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tess[i].uniform_descriptor ) );

		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

		SET_OBJECT_NAME( vk.tess[ i ].uniform_descriptor, va( "uniform descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.color_image_view )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = vk_descriptor_layout_ref( RTX_VK_DESC_LAYOUT_SAMPLER );

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_descriptor ) );
		if ( vk.motion_blur_image_view != VK_NULL_HANDLE ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc,
				&vk.motion_blur_descriptor ) );
			SET_OBJECT_NAME( vk.motion_blur_descriptor,
				"motion blur scratch descriptor",
				VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.underwater_image_view != VK_NULL_HANDLE ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc,
				&vk.underwater_descriptor ) );
			SET_OBJECT_NAME( vk.underwater_descriptor,
				"underwater view scratch descriptor",
				VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}

		if ( vk.depth_sample_image_view ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.depth_sample_descriptor ) );
			SET_OBJECT_NAME( vk.depth_sample_descriptor, "global fog depth descriptor", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}

		if ( r_bloom->integer )
		{
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_image_descriptor[i] ) );
			}
		}

		for ( i = 0; i < VK_NUM_MENU_BLUR_IMAGES; i++ )
		{
			if ( vk.menu_blur_image_view[i] == VK_NULL_HANDLE ) {
				continue;
			}
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.menu_blur_descriptor[i] ) );
			SET_OBJECT_NAME( vk.menu_blur_descriptor[i], va( "menu soft focus descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}

		alloc.descriptorSetCount = 1;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

		if ( vk.liquidSnapshot.color_image_view ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc,
				&vk.liquidSnapshot.source_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc,
				&vk.liquidSnapshot.color_descriptor ) );
			SET_OBJECT_NAME( vk.liquidSnapshot.source_descriptor,
				"liquid snapshot source descriptor",
				VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
			SET_OBJECT_NAME( vk.liquidSnapshot.color_descriptor,
				"liquid snapshot descriptor",
				VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.liquidDepth.image_view ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc,
				&vk.liquidDepth.descriptor ) );
			SET_OBJECT_NAME( vk.liquidDepth.descriptor,
				"liquid depth descriptor",
				VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}

	}

	if ( vk.dlight_shadow_image_view ) {
		alloc.pSetLayouts =
			vk_descriptor_layout_ref( RTX_VK_DESC_LAYOUT_SAMPLER );
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc,
			&vk.dlight_shadow_descriptor ) );
		SET_OBJECT_NAME( vk.dlight_shadow_descriptor,
			"dlight shadow atlas descriptor",
			VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.spot_shadow_image_view ) {
		alloc.pSetLayouts =
			vk_descriptor_layout_ref( RTX_VK_DESC_LAYOUT_SAMPLER );
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc,
			&vk.spot_shadow_descriptor ) );
		SET_OBJECT_NAME( vk.spot_shadow_descriptor,
			"spot shadow atlas descriptor",
			VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.csm_shadow_image_view ) {
		alloc.pSetLayouts =
			vk_descriptor_layout_ref( RTX_VK_DESC_LAYOUT_SAMPLER );
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc,
			&vk.csm_shadow_descriptor ) );
		SET_OBJECT_NAME( vk.csm_shadow_descriptor,
			"csm shadow atlas descriptor",
			VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	/* Pool reset invalidates retained handles, including shadow descriptors.
	 * Publish writes only after every attachment set has been reallocated. */
	vk_update_attachment_descriptors();
}


static void vk_release_geometry_buffers( void )
{
	int i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroyBuffer( vk.device, vk.tess[i].vertex_buffer, NULL );
		vk.tess[i].vertex_buffer = VK_NULL_HANDLE;
	}

	qvkFreeMemory( vk.device, vk.geometry_buffer_memory, NULL );
	vk.geometry_buffer_memory = VK_NULL_HANDLE;
	vk_lifetime_mark_destroy( RTX_VK_RESOURCE_GEOMETRY_BUFFERS, __func__ );
}


static void vk_create_geometry_buffers( VkDeviceSize size )
{
	VkMemoryRequirements vb_memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	uint32_t memory_type_bits;
	uint32_t memory_type;
	void *data;
	int i;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	Com_Memset( &vb_memory_requirements, 0, sizeof( vb_memory_requirements ) );

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		desc.size = size;
		desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.tess[i].vertex_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.tess[i].vertex_buffer, &vb_memory_requirements );
	}

	memory_type_bits = vb_memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = vb_memory_requirements.size * NUM_COMMAND_BUFFERS;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.geometry_buffer_memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data ) );

	vertex_buffer_offset = 0;

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkBindBufferMemory( vk.device, vk.tess[i].vertex_buffer, vk.geometry_buffer_memory, vertex_buffer_offset );
		vk.tess[i].vertex_buffer_ptr = (byte*)data + vertex_buffer_offset;
		vk.tess[i].vertex_buffer_offset = 0;
		vertex_buffer_offset += vb_memory_requirements.size;

		SET_OBJECT_NAME( vk.tess[i].vertex_buffer, va( "geometry buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	}

	SET_OBJECT_NAME( vk.geometry_buffer_memory, "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	/* Allocation padding separates the mapped buffers but is not addressable
	 * through VkBuffer. Upload/descriptor bounds must use its logical size. */
	vk.geometry_buffer_size = size;

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
	vk_lifetime_mark_create( RTX_VK_RESOURCE_GEOMETRY_BUFFERS, __func__ );
}


static void vk_create_storage_buffer( uint32_t size )
{
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	uint32_t memory_type_bits;
	uint32_t memory_type;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	Com_Memset( &memory_requirements, 0, sizeof( memory_requirements ) );

	desc.size = size;
	desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.storage.buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.storage.buffer, &memory_requirements );

	memory_type_bits = memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.storage.memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.storage.memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.storage.buffer_ptr ) );

	Com_Memset( vk.storage.buffer_ptr, 0, memory_requirements.size );

	qvkBindBufferMemory( vk.device, vk.storage.buffer, vk.storage.memory, 0 );

	SET_OBJECT_NAME( vk.storage.buffer, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.storage.descriptor, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	SET_OBJECT_NAME( vk.storage.memory, "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
	vk_lifetime_mark_create( RTX_VK_RESOURCE_STORAGE_BUFFER, __func__ );
}

static void vk_destroy_storage_buffer( void )
{
	if ( vk.storage.buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.storage.buffer, NULL );
		vk.storage.buffer = VK_NULL_HANDLE;
	}

	if ( vk.storage.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.storage.memory, NULL );
		vk.storage.memory = VK_NULL_HANDLE;
	}

	vk.storage.buffer_ptr = NULL;
	vk_lifetime_mark_destroy( RTX_VK_RESOURCE_STORAGE_BUFFER, __func__ );
}


#ifdef USE_VBO
void vk_release_vbo( void )
{
	if ( vk.vbo.vertex_buffer )
		qvkDestroyBuffer( vk.device, vk.vbo.vertex_buffer, NULL );
	vk.vbo.vertex_buffer = VK_NULL_HANDLE;

	if ( vk.vbo.buffer_memory )
		qvkFreeMemory( vk.device, vk.vbo.buffer_memory, NULL );
	vk.vbo.buffer_memory = VK_NULL_HANDLE;
}


qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size )
{
	VkMemoryRequirements vb_mem_reqs;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	VkDeviceSize allocationSize;
	uint32_t memory_type_bits;
	VkBuffer staging_vertex_buffer;
	VkDeviceMemory staging_buffer_memory;
	VkCommandBuffer command_buffer;
	VkBufferCopy copyRegion[1];
	void *data;

	vk_release_vbo();

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	// device-local buffer
	desc.size = vbo_size;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.vbo.vertex_buffer ) );

	// staging buffer
	desc.size = vbo_size;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &staging_vertex_buffer ) );

	// memory requirements
	qvkGetBufferMemoryRequirements( vk.device, vk.vbo.vertex_buffer, &vb_mem_reqs );
	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = allocationSize;
	alloc_info.memoryTypeIndex = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vbo.buffer_memory ) );
	qvkBindBufferMemory( vk.device, vk.vbo.vertex_buffer, vk.vbo.buffer_memory, vertex_buffer_offset );

	// staging buffers

	// memory requirements
	qvkGetBufferMemoryRequirements( vk.device, staging_vertex_buffer, &vb_mem_reqs );
	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = allocationSize;
	alloc_info.memoryTypeIndex = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &staging_buffer_memory ) );
	qvkBindBufferMemory( vk.device, staging_vertex_buffer, staging_buffer_memory, vertex_buffer_offset );

	VK_CHECK( qvkMapMemory( vk.device, staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data ) );
	memcpy( (byte*)data + vertex_buffer_offset, vbo_data, vbo_size );
	qvkUnmapMemory( vk.device, staging_buffer_memory );

	command_buffer = begin_command_buffer();
	copyRegion[0].srcOffset = 0;
	copyRegion[0].dstOffset = 0;
	copyRegion[0].size = vbo_size;
	qvkCmdCopyBuffer( command_buffer, staging_vertex_buffer, vk.vbo.vertex_buffer, 1, &copyRegion[0] );

	end_command_buffer( command_buffer, __func__ );

	qvkDestroyBuffer( vk.device, staging_vertex_buffer, NULL );
	qvkFreeMemory( vk.device, staging_buffer_memory, NULL );

	SET_OBJECT_NAME( vk.vbo.vertex_buffer, "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.vbo.buffer_memory, "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	return qtrue;
}
#endif

#include "shaders/spirv/shader_data.c"

// the path tracer: materials, lights, the compute backend's BVHs, acceleration
// structures, trace, denoiser
#include "rt/rt_material.c"
#include "../renderercommon/tr_sky_sun_units.h"
#include "rt/rt_lights.c"
#include "rt/rt_bvh.c"
#include "rt/rt_backend.c"
#define SHADER_MODULE(name) SHADER_MODULE(name,sizeof(name))

static void vk_create_shader_modules( void )
{
	int i, j, k, l;

	vk.modules.vert.gen[0][0][0][0] = SHADER_MODULE( vert_tx0 );
	vk.modules.vert.gen[0][0][0][1] = SHADER_MODULE( vert_tx0_fog );
	vk.modules.vert.gen[0][0][1][0] = SHADER_MODULE( vert_tx0_env );
	vk.modules.vert.gen[0][0][1][1] = SHADER_MODULE( vert_tx0_env_fog );

	vk.modules.vert.gen[1][0][0][0] = SHADER_MODULE( vert_tx1 );
	vk.modules.vert.gen[1][0][0][1] = SHADER_MODULE( vert_tx1_fog );
	vk.modules.vert.gen[1][0][1][0] = SHADER_MODULE( vert_tx1_env );
	vk.modules.vert.gen[1][0][1][1] = SHADER_MODULE( vert_tx1_env_fog );

	vk.modules.vert.gen[1][1][0][0] = SHADER_MODULE( vert_tx1_cl );
	vk.modules.vert.gen[1][1][0][1] = SHADER_MODULE( vert_tx1_cl_fog );
	vk.modules.vert.gen[1][1][1][0] = SHADER_MODULE( vert_tx1_cl_env );
	vk.modules.vert.gen[1][1][1][1] = SHADER_MODULE( vert_tx1_cl_env_fog );

	vk.modules.vert.gen[2][0][0][0] = SHADER_MODULE( vert_tx2 );
	vk.modules.vert.gen[2][0][0][1] = SHADER_MODULE( vert_tx2_fog );
	vk.modules.vert.gen[2][0][1][0] = SHADER_MODULE( vert_tx2_env );
	vk.modules.vert.gen[2][0][1][1] = SHADER_MODULE( vert_tx2_env_fog );

	vk.modules.vert.gen[2][1][0][0] = SHADER_MODULE( vert_tx2_cl );
	vk.modules.vert.gen[2][1][0][1] = SHADER_MODULE( vert_tx2_cl_fog );
	vk.modules.vert.gen[2][1][1][0] = SHADER_MODULE( vert_tx2_cl_env );
	vk.modules.vert.gen[2][1][1][1] = SHADER_MODULE( vert_tx2_cl_env_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					const char *s = va( "%s-texture%s%s%s vertex module", tx[i], cl[j], env[k], fog[l] );
					SET_OBJECT_NAME( vk.modules.vert.gen[i][j][k][l], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
				}
			}
		}
	}

	// specialized depth-fragment shader
	vk.modules.frag.gen0_df = SHADER_MODULE( frag_tx0_df );
	SET_OBJECT_NAME( vk.modules.frag.gen0_df, "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	// fixed-color (1.0) shader modules
	vk.modules.vert.ident1[0][0][0] = SHADER_MODULE( vert_tx0_ident1 );
	vk.modules.vert.ident1[0][0][1] = SHADER_MODULE( vert_tx0_ident1_fog );
	vk.modules.vert.ident1[0][1][0] = SHADER_MODULE( vert_tx0_ident1_env );
	vk.modules.vert.ident1[0][1][1] = SHADER_MODULE( vert_tx0_ident1_env_fog );
	vk.modules.vert.ident1[1][0][0] = SHADER_MODULE( vert_tx1_ident1 );
	vk.modules.vert.ident1[1][0][1] = SHADER_MODULE( vert_tx1_ident1_fog );
	vk.modules.vert.ident1[1][1][0] = SHADER_MODULE( vert_tx1_ident1_env );
	vk.modules.vert.ident1[1][1][1] = SHADER_MODULE( vert_tx1_ident1_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture identity%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.ident1[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.ident1[0][0] = SHADER_MODULE( frag_tx0_ident1 );
	vk.modules.frag.ident1[0][1] = SHADER_MODULE( frag_tx0_ident1_fog );
	vk.modules.frag.ident1[1][0] = SHADER_MODULE( frag_tx1_ident1 );
	vk.modules.frag.ident1[1][1] = SHADER_MODULE( frag_tx1_ident1_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture identity%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ident1[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.vert.fixed[0][0][0] = SHADER_MODULE( vert_tx0_fixed );
	vk.modules.vert.fixed[0][0][1] = SHADER_MODULE( vert_tx0_fixed_fog );
	vk.modules.vert.fixed[0][1][0] = SHADER_MODULE( vert_tx0_fixed_env );
	vk.modules.vert.fixed[0][1][1] = SHADER_MODULE( vert_tx0_fixed_env_fog );
	vk.modules.vert.fixed[1][0][0] = SHADER_MODULE( vert_tx1_fixed );
	vk.modules.vert.fixed[1][0][1] = SHADER_MODULE( vert_tx1_fixed_fog );
	vk.modules.vert.fixed[1][1][0] = SHADER_MODULE( vert_tx1_fixed_env );
	vk.modules.vert.fixed[1][1][1] = SHADER_MODULE( vert_tx1_fixed_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture fixed-color%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.fixed[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.fixed[0][0] = SHADER_MODULE( frag_tx0_fixed );
	vk.modules.frag.fixed[0][1] = SHADER_MODULE( frag_tx0_fixed_fog );
	vk.modules.frag.fixed[1][0] = SHADER_MODULE( frag_tx1_fixed );
	vk.modules.frag.fixed[1][1] = SHADER_MODULE( frag_tx1_fixed_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture fixed-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.fixed[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.ent[0][0] = SHADER_MODULE( frag_tx0_ent );
	vk.modules.frag.ent[0][1] = SHADER_MODULE( frag_tx0_ent_fog );
	//vk.modules.frag.ent[1][0] = SHADER_MODULE( frag_tx1_ent );
	//vk.modules.frag.ent[1][1] = SHADER_MODULE( frag_tx1_ent_fog );
	for ( i = 0; i < 1; i++ ) {
		const char *tx[] = { "single" /*, "double" */};
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture entity-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ent[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.gen[0][0][0] = SHADER_MODULE( frag_tx0 );
	vk.modules.frag.gen[0][0][1] = SHADER_MODULE( frag_tx0_fog );

	vk.modules.frag.gen[1][0][0] = SHADER_MODULE( frag_tx1 );
	vk.modules.frag.gen[1][0][1] = SHADER_MODULE( frag_tx1_fog );

	vk.modules.frag.gen[1][1][0] = SHADER_MODULE( frag_tx1_cl );
	vk.modules.frag.gen[1][1][1] = SHADER_MODULE( frag_tx1_cl_fog );

	vk.modules.frag.gen[2][0][0] = SHADER_MODULE( frag_tx2 );
	vk.modules.frag.gen[2][0][1] = SHADER_MODULE( frag_tx2_fog );

	vk.modules.frag.gen[2][1][0] = SHADER_MODULE( frag_tx2_cl );
	vk.modules.frag.gen[2][1][1] = SHADER_MODULE( frag_tx2_cl_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture%s%s fragment module", tx[i], cl[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.frag.gen[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}


	vk.modules.vert.light[0] = SHADER_MODULE( vert_light );
	vk.modules.vert.light[1] = SHADER_MODULE( vert_light_fog );
	SET_OBJECT_NAME( vk.modules.vert.light[0], "light vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.vert.light[1], "light fog vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.light[0][0] = SHADER_MODULE( frag_light );
	vk.modules.frag.light[0][1] = SHADER_MODULE( frag_light_fog );
	vk.modules.frag.light[1][0] = SHADER_MODULE( frag_light_line );
	vk.modules.frag.light[1][1] = SHADER_MODULE( frag_light_line_fog );
	SET_OBJECT_NAME( vk.modules.frag.light[0][0], "light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[0][1], "light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][0], "linear light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][1], "linear light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.vert.csm_shadow = SHADER_MODULE( csm_shadow_vert_spv );
	vk.modules.frag.csm_shadow = SHADER_MODULE( csm_shadow_frag_spv );
	SET_OBJECT_NAME( vk.modules.vert.csm_shadow, "csm shadow vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.csm_shadow, "csm shadow fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.color_fs = SHADER_MODULE( color_frag_spv );
	vk.modules.color_vs = SHADER_MODULE( color_vert_spv );

	SET_OBJECT_NAME( vk.modules.color_vs, "single-color vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.color_fs, "single-color fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.fog_vs = SHADER_MODULE( fog_vert_spv );
	vk.modules.fog_fs = SHADER_MODULE( fog_frag_spv );

	SET_OBJECT_NAME( vk.modules.fog_vs, "fog-only vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.fog_fs, "fog-only fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.dot_vs = SHADER_MODULE( dot_vert_spv );
	vk.modules.dot_fs = SHADER_MODULE( dot_frag_spv );

	SET_OBJECT_NAME( vk.modules.dot_vs, "dot vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.dot_fs, "dot fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	if ( vk_liquid_requested() ) {
		vk.modules.liquid_vs = SHADER_MODULE( liquid_vert_spv );
		vk.modules.liquid_fs = SHADER_MODULE( liquid_frag_spv );
		vk.modules.liquid_copy_fs = SHADER_MODULE( liquid_copy_frag_spv );

		SET_OBJECT_NAME( vk.modules.liquid_vs,
			"liquid effect vertex module",
			VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		SET_OBJECT_NAME( vk.modules.liquid_fs,
			"liquid effect fragment module",
			VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		SET_OBJECT_NAME( vk.modules.liquid_copy_fs,
			"liquid scene-copy fragment module",
			VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	}

	vk.modules.bloom_fs = SHADER_MODULE( bloom_frag_spv );
	vk.modules.blur_fs = SHADER_MODULE( blur_frag_spv );
	vk.modules.blend_fs = SHADER_MODULE( blend_frag_spv );
	vk.modules.motion_blur_fs = SHADER_MODULE( motion_blur_frag_spv );
	vk.modules.menu_blur_fs = SHADER_MODULE( menu_blur_frag_spv );
	vk.modules.world_outline_fs = SHADER_MODULE( world_outline_frag_spv );
	vk.modules.global_fog_fs = SHADER_MODULE( global_fog_frag_spv );
	if ( vk_underwater_requested() ) {
		vk.modules.underwater_fs = SHADER_MODULE( underwater_frag_spv );
	}

	SET_OBJECT_NAME( vk.modules.bloom_fs, "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blur_fs, "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blend_fs, "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.motion_blur_fs, "motion blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.menu_blur_fs, "menu soft focus fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.world_outline_fs, "world cel depth-outline fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.global_fog_fs, "global fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	if ( vk.modules.underwater_fs != VK_NULL_HANDLE ) {
		SET_OBJECT_NAME( vk.modules.underwater_fs, "underwater view fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	}

	vk.modules.gamma_fs = SHADER_MODULE( gamma_frag_spv );
	vk.modules.gamma_vs = SHADER_MODULE( gamma_vert_spv );

	SET_OBJECT_NAME( vk.modules.gamma_fs, "gamma post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.gamma_vs, "gamma post-processing vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	if ( vk_rt_trace_available() ) {
		if ( vk_rt_pipeline_enabled() ) {
			vk.modules.rt_rgen = SHADER_MODULE( rt_main_rgen_spv );
			vk.modules.rt_rmiss = SHADER_MODULE( rt_main_rmiss_spv );
			vk.modules.rt_rmiss_shadow = SHADER_MODULE( rt_shadow_rmiss_spv );
			vk.modules.rt_rmiss_filter = SHADER_MODULE( rt_filter_rmiss_spv );
			vk.modules.rt_rchit = SHADER_MODULE( rt_main_rchit_spv );
			vk.modules.rt_rahit = SHADER_MODULE( rt_main_rahit_spv );
			vk.modules.rt_rahit_filter = SHADER_MODULE( rt_filter_rahit_spv );
			vk.modules.rt_rahit_shadow = SHADER_MODULE( rt_shadow_rahit_spv );

			SET_OBJECT_NAME( vk.modules.rt_rgen, "rt raygen module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			SET_OBJECT_NAME( vk.modules.rt_rmiss, "rt miss module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			SET_OBJECT_NAME( vk.modules.rt_rmiss_shadow, "rt shadow miss module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			SET_OBJECT_NAME( vk.modules.rt_rchit, "rt closest-hit module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			SET_OBJECT_NAME( vk.modules.rt_rahit, "rt any-hit module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			SET_OBJECT_NAME( vk.modules.rt_rmiss_filter, "rt filter miss module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			SET_OBJECT_NAME( vk.modules.rt_rahit_filter, "rt filter any-hit module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		} else {
			vk.modules.rt_trace = SHADER_MODULE( rt_trace_comp_spv );
			SET_OBJECT_NAME( vk.modules.rt_trace, "rt compute trace module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
		vk.modules.rt_composite = SHADER_MODULE( rt_composite_comp_spv );
		vk.modules.rt_tonemap = SHADER_MODULE( rt_tonemap_comp_spv );
		vk.modules.rt_denoise_temporal = SHADER_MODULE( rt_denoise_temporal_comp_spv );
		vk.modules.rt_denoise_atrous = SHADER_MODULE( rt_denoise_atrous_comp_spv );
		SET_OBJECT_NAME( vk.modules.rt_composite, "rt composite module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		SET_OBJECT_NAME( vk.modules.rt_tonemap, "rt tone mapping module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	}
}


static void vk_alloc_persistent_pipelines( void )
{
	unsigned int state_bits;
	Vk_Pipeline_Def def;

	// skybox
	{
		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.skybox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		Com_Memset(&def, 0, sizeof(def));
		def.polygon_offset = qfalse;
		def.state_bits = 0;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

		for (i = 0; i < 2; i++) {
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++) {
				def.mirror = mirror_flags[j];
				vk.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
			}
		}
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
		vk.shadow_finish_pipeline = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,	// modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL			// additive
		};
		qboolean polygon_offset[2] = { qfalse, qtrue };
		int i, j, k;
#ifdef USE_PMLIGHT
		int l;
#endif

		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;

		for ( i = 0; i < 2; i++ ) {
			unsigned fog_state = fog_state_bits[ i ];
			unsigned dlight_state = dlight_state_bits[ i ];

			for ( j = 0; j < 3; j++ ) {
				def.face_culling = j; // cullType_t value

				for ( k = 0; k < 2; k++ ) {
					def.polygon_offset = polygon_offset[ k ];
#ifdef USE_FOG_ONLY
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk.fog_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );

					def.shader_type = TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, R_GetDlightMode() == 0 ? qtrue : qfalse );
#else
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		def.depth_fade =
			( ( ( ( r_dlightShadows && r_dlightShadows->integer ) ||
					( r_shadowCorrectness && r_shadowCorrectness->integer ) ) &&
				r_dlightMode && R_GetDlightMode() &&
				vk_dlight_shadow_atlas_available() ) ||
			  ( !( r_shadowCorrectness && r_shadowCorrectness->integer ) &&
				r_spotShadows && r_spotShadows->integer &&
				vk_spot_shadow_atlas_available() ) ) ? 1 : 0;
		//def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++) { // cullType
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) { // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for ( k = 0; k < 2; k++ ) {
					def.fog_stage = k; // fogStage
					for ( l = 0; l < 2; l++ ) {
						def.abs_light = l;
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
						vk.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR;
						vk.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
					}
				}
			}
		}

		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO |
			GLS_DEPTHFUNC_EQUAL;
		def.shader_type = TYPE_CSM_SHADOW;
		def.face_culling = CT_TWO_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.csm_shadow_pipeline = vk_find_pipeline_ext( 0, &def,
			( !( r_shadowCorrectness && r_shadowCorrectness->integer ) &&
			r_csmShadows && r_csmShadows->integer &&
			vk_csm_shadow_atlas_available() ) ? qtrue : qfalse );
#endif // USE_PMLIGHT
	}

	if ( vk_liquid_requested() ) {
		int i, j, k;

		Com_Memset( &def, 0, sizeof( def ) );
		def.shader_type = TYPE_LIQUID;
		def.state_bits = GLS_SRCBLEND_SRC_ALPHA |
			GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		for ( i = 0; i < 3; i++ ) {
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) {
				def.polygon_offset = j ? qtrue : qfalse;
				for ( k = 0; k < 2; k++ ) {
					def.mirror = k ? qtrue : qfalse;
					vk.liquid_pipelines[i][j][k] =
						vk_find_pipeline_ext( 0, &def, qfalse );
				}
			}
		}
	}

	// RT_BEAM surface
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk.surface_beam_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// axis for missing models
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if ( vk.wideLines )
			def.line_width = 3;
		vk.surface_axis_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// flare visibility test dot
	if ( vk.fragmentStores )
	{
		Com_Memset( &def, 0, sizeof( def ) );
		//def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = POINT_LIST;
		vk.dot_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// DrawNormals()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.normals_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_DebugPolygon()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		vk.surface_debug_pipeline_solid = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.surface_debug_pipeline_outline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_ShowImages
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_COLOR_BLACK;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline2 = vk_find_pipeline_ext( 0, &def, qfalse );
	}
}

static int vk_color_grade_mode( void )
{
	int mode;

	/* The grade is defined in scene-linear space, so legacy LDR rendering
	 * deliberately retains its established final-pass behavior. */
	if ( !vk_scene_linear_enabled() || !r_colorGrade ) {
		return 0;
	}

	mode = r_colorGrade->integer;
	return Com_Clamp( 0, 3, mode );
}

static qboolean vk_color_grade_uses_lgg( int mode )
{
	return ( mode == 1 || mode == 3 ) ? qtrue : qfalse;
}

static qboolean vk_color_grade_uses_lut( int mode )
{
	return ( mode == 2 || mode == 3 ) ? qtrue : qfalse;
}

static void vk_parse_vec3_cvar( const cvar_t *cvar, float fallback0,
	float fallback1, float fallback2, float minValue, float maxValue,
	float out[3] )
{
	float values[3] = { fallback0, fallback1, fallback2 };

	if ( cvar && cvar->string && cvar->string[0] ) {
		(void)sscanf( cvar->string, "%f %f %f",
			&values[0], &values[1], &values[2] );
	}

	out[0] = Com_Clamp( minValue, maxValue, values[0] );
	out[1] = Com_Clamp( minValue, maxValue, values[1] );
	out[2] = Com_Clamp( minValue, maxValue, values[2] );
}

static void vk_set_identity_3x3( float matrix[9] )
{
	matrix[0] = 1.0f; matrix[1] = 0.0f; matrix[2] = 0.0f;
	matrix[3] = 0.0f; matrix[4] = 1.0f; matrix[5] = 0.0f;
	matrix[6] = 0.0f; matrix[7] = 0.0f; matrix[8] = 1.0f;
}

static void vk_cct_to_xyz( float kelvin, float xyz[3] )
{
	float x, y;
	const float t = Com_Clamp( 1667.0f, 25000.0f, kelvin );
	const float t2 = t * t;
	const float t3 = t2 * t;

	if ( t <= 4000.0f ) {
		x = -0.2661239e9f / t3 - 0.2343580e6f / t2 +
			0.8776956e3f / t + 0.179910f;
	} else {
		x = -3.0258469e9f / t3 + 2.1070379e6f / t2 +
			0.2226347e3f / t + 0.240390f;
	}

	if ( t < 2222.0f ) {
		y = -1.1063814f * x * x * x - 1.34811020f * x * x +
			2.18555832f * x - 0.20219683f;
	} else if ( t < 4000.0f ) {
		y = -0.9549476f * x * x * x - 1.37418593f * x * x +
			2.09137015f * x - 0.16748867f;
	} else {
		y = 3.0817580f * x * x * x - 5.87338670f * x * x +
			3.75112997f * x - 0.37001483f;
	}

	if ( y <= 0.0001f ) {
		xyz[0] = 0.95047f;
		xyz[1] = 1.0f;
		xyz[2] = 1.08883f;
		return;
	}

	xyz[0] = x / y;
	xyz[1] = 1.0f;
	xyz[2] = ( 1.0f - x - y ) / y;
}

static void vk_build_bradford_adaptation( float sourceKelvin,
	float targetKelvin, float matrix[9] )
{
	static const float bradford[9] = {
		 0.8951f,  0.2664f, -0.1614f,
		-0.7502f,  1.7135f,  0.0367f,
		 0.0389f, -0.0685f,  1.0296f
	};
	static const float bradfordInv[9] = {
		 0.9869929f, -0.1470543f,  0.1599627f,
		 0.4323053f,  0.5183603f,  0.0492912f,
		-0.0085287f,  0.0400428f,  0.9684867f
	};
	float src[3], dst[3], srcCone[3], dstCone[3];
	float scale[3], scaledBradford[9];
	int row, col;

	vk_cct_to_xyz( sourceKelvin, src );
	vk_cct_to_xyz( targetKelvin, dst );

	for ( row = 0; row < 3; row++ ) {
		srcCone[row] = bradford[row * 3 + 0] * src[0] +
			bradford[row * 3 + 1] * src[1] +
			bradford[row * 3 + 2] * src[2];
		dstCone[row] = bradford[row * 3 + 0] * dst[0] +
			bradford[row * 3 + 1] * dst[1] +
			bradford[row * 3 + 2] * dst[2];
		scale[row] = fabs( srcCone[row] ) > 0.0001f ?
			dstCone[row] / srcCone[row] : 1.0f;
	}

	for ( row = 0; row < 3; row++ ) {
		for ( col = 0; col < 3; col++ ) {
			scaledBradford[row * 3 + col] =
				scale[row] * bradford[row * 3 + col];
		}
	}

	for ( row = 0; row < 3; row++ ) {
		for ( col = 0; col < 3; col++ ) {
			matrix[row * 3 + col] =
				bradfordInv[row * 3 + 0] * scaledBradford[0 * 3 + col] +
				bradfordInv[row * 3 + 1] * scaledBradford[1 * 3 + col] +
				bradfordInv[row * 3 + 2] * scaledBradford[2 * 3 + col];
		}
	}
}

static qboolean vk_validate_color_grade_lut_atlas( const image_t *image,
	int *size )
{
	int lutSize;

	if ( !image || image->width <= 0 || image->height <= 0 ||
		image->width != image->height * image->height ) {
		return qfalse;
	}

	lutSize = image->height;
	if ( lutSize < 2 || lutSize > 64 ) {
		return qfalse;
	}

	if ( size ) {
		*size = lutSize;
	}
	return qtrue;
}

static image_t *vk_create_identity_color_grade_lut( int *size )
{
	static image_t *identityLut;
	enum { IDENTITY_LUT_SIZE = 16 };
	byte data[ IDENTITY_LUT_SIZE * IDENTITY_LUT_SIZE *
		IDENTITY_LUT_SIZE * 4 ];
	const int width = IDENTITY_LUT_SIZE * IDENTITY_LUT_SIZE;
	int r, g, b;

	if ( identityLut ) {
		if ( size ) {
			*size = IDENTITY_LUT_SIZE;
		}
		return identityLut;
	}

	for ( b = 0; b < IDENTITY_LUT_SIZE; b++ ) {
		for ( g = 0; g < IDENTITY_LUT_SIZE; g++ ) {
			for ( r = 0; r < IDENTITY_LUT_SIZE; r++ ) {
				const int index =
					( g * width + b * IDENTITY_LUT_SIZE + r ) * 4;
				data[index + 0] =
					(byte)( r * 255 / ( IDENTITY_LUT_SIZE - 1 ) );
				data[index + 1] =
					(byte)( g * 255 / ( IDENTITY_LUT_SIZE - 1 ) );
				data[index + 2] =
					(byte)( b * 255 / ( IDENTITY_LUT_SIZE - 1 ) );
				data[index + 3] = 255;
			}
		}
	}

	identityLut = R_CreateImage( "*colorGradeIdentityLUT", NULL, data,
		width, IDENTITY_LUT_SIZE,
		IMGFLAG_CLAMPTOEDGE | IMGFLAG_NO_COMPRESSION |
		IMGFLAG_NOSCALE | IMGFLAG_COLORSPACE_LINEAR );
	if ( size ) {
		*size = IDENTITY_LUT_SIZE;
	}
	return identityLut;
}

static image_t *vk_color_grade_lut_image( int *size )
{
	static image_t *lutImage;
	static int lutSize;
	static int lutModificationCount = -1;
	const int flags = IMGFLAG_CLAMPTOEDGE | IMGFLAG_NO_COMPRESSION |
		IMGFLAG_NOSCALE | IMGFLAG_COLORSPACE_LINEAR;

	if ( !r_colorGradeLUT ||
		r_colorGradeLUT->modificationCount == lutModificationCount ) {
		if ( size ) {
			*size = lutSize;
		}
		return lutImage;
	}

	lutModificationCount = r_colorGradeLUT->modificationCount;
	lutImage = NULL;
	lutSize = 0;

	if ( r_colorGradeLUT->string && r_colorGradeLUT->string[0] ) {
		image_t *loaded = R_FindImageFile( r_colorGradeLUT->string, flags );

		if ( vk_validate_color_grade_lut_atlas( loaded, &lutSize ) ) {
			lutImage = loaded;
		} else {
			ri.Printf( PRINT_WARNING,
				"WARNING: color-grade LUT '%s' must use width N*N and "
				"height N; using identity LUT\n",
				r_colorGradeLUT->string );
		}
	}

	if ( !lutImage ) {
		lutImage = vk_create_identity_color_grade_lut( &lutSize );
	}

	if ( size ) {
		*size = lutSize;
	}
	return lutImage;
}

static VkDescriptorSet vk_color_grade_lut_descriptor( void )
{
	image_t *image;
	int size;

	if ( !vk_color_grade_uses_lut( vk_color_grade_mode() ) ) {
		if ( tr.whiteImage &&
			tr.whiteImage->descriptor != VK_NULL_HANDLE ) {
			return tr.whiteImage->descriptor;
		}
		return vk.color_descriptor;
	}

	image = vk_color_grade_lut_image( &size );
	if ( image && image->descriptor != VK_NULL_HANDLE ) {
		return image->descriptor;
	}
	if ( tr.whiteImage && tr.whiteImage->descriptor != VK_NULL_HANDLE ) {
		return tr.whiteImage->descriptor;
	}
	return vk.color_descriptor;
}

static void vk_bind_gamma_descriptor_sets( void )
{
	VkDescriptorSet post_sets[2];

	post_sets[0] = vk.color_descriptor;
	post_sets[1] = vk_color_grade_lut_descriptor();
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ),
		0, ARRAY_LEN( post_sets ), post_sets, 0, NULL );
}

static void vk_push_post_process_constants( void )
{
	float constants[4] = { 0.0f };
	const float invWidth = glConfig.vidWidth > 0 ?
		1.0f / (float)glConfig.vidWidth : 1.0f;
	const float invHeight = glConfig.vidHeight > 0 ?
		1.0f / (float)glConfig.vidHeight : 1.0f;

	constants[0] = tr.refdef.floatTime > 0.0 ?
		(float)tr.refdef.floatTime : (float)tr.frameCount * ( 1.0f / 60.0f );
	constants[1] = invWidth;
	constants[2] = invHeight;
	qvkCmdPushConstants( vk.cmd->command_buffer,
		vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ),
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( constants ), constants );
}

void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass );

void vk_update_post_process_pipelines( void )
{
	vk_set_hdr_metadata();

	if ( vk.fboActive ) {
		// update gamma shader
		vk_create_post_process_pipeline( 0, 0, 0 );
		if ( vk.render_pass.post_bloom != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 6,
				glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( vk.capture.image ) {
			// update capture pipeline
			vk_create_post_process_pipeline( 3, gls.captureWidth, gls.captureHeight );
		}
		if ( vk.render_pass.menu_blur != VK_NULL_HANDLE &&
			vk.menu_blur_image_view[0] != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 12, vk.menu_blur_width[0], vk.menu_blur_height[0] );
			vk_create_post_process_pipeline( 13, vk.menu_blur_width[1], vk.menu_blur_height[1] );
			vk_create_post_process_pipeline( 14, glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( vk.render_pass.global_fog != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 4, glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( vk.render_pass.liquid_snapshot != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 5,
				vk.liquidSnapshotWidth, vk.liquidSnapshotHeight );
		}
		if ( vk.render_pass.motion_blur != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 8,
				glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 9,
				glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( vk.render_pass.underwater != VK_NULL_HANDLE &&
			vk.modules.underwater_fs != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 10,
				glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 11,
				glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( r_bloom->integer ) {
			// update bloom shaders
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			uint32_t i;

			vk_create_post_process_pipeline( 1, width, height ); // bloom extraction

			for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i += 2 ) {
				width /= 2;
				height /= 2;
				vk_create_blur_pipeline( i + 0, width, height, qtrue ); // horizontal
				vk_create_blur_pipeline( i + 1, width, height, qfalse ); // vertical
			}

			vk_create_post_process_pipeline( 2, glConfig.vidWidth, glConfig.vidHeight ); // bloom blending
			vk_create_post_process_pipeline( 7, glConfig.vidWidth, glConfig.vidHeight ); // bloom blend preserving cel outlines
		}
	}
}


typedef struct vk_attach_desc_s  {
	VkImage descriptor;
	VkImageView *image_view;
	VkImageUsageFlags usage;
	VkMemoryRequirements reqs;
	uint32_t memoryTypeIndex;
	VkDeviceSize  memory_offset;
	// for layout transition:
	VkImageAspectFlags aspect_flags;
	VkImageLayout image_layout;
	VkFormat image_format;
} vk_attach_desc_t;

static vk_attach_desc_t attachments[ MAX_ATTACHMENTS_IN_POOL ];
static uint32_t num_attachments = 0;


static void vk_clear_attachment_pool( void )
{
	num_attachments = 0;
}


static void vk_alloc_attachments( void )
{
	VkImageViewCreateInfo view_desc;
	VkMemoryDedicatedAllocateInfoKHR alloc_info2;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	uint32_t memoryTypeBits;
	uint32_t memoryTypeIndex;
	uint32_t i;

	if ( num_attachments == 0 ) {
		return;
	}

	if ( vk.image_memory_count >= ARRAY_LEN( vk.image_memory ) ) {
		ri.Error( ERR_DROP, "vk.image_memory_count == %i", (int)ARRAY_LEN( vk.image_memory ) );
	}

	memoryTypeBits = ~0U;
	offset = 0;

	for ( i = 0; i < num_attachments; i++ ) {
#ifdef MIN_IMAGE_ALIGN
		VkDeviceSize alignment = MAX( attachments[ i ].reqs.alignment, MIN_IMAGE_ALIGN );
#else
		VkDeviceSize alignment = attachments[ i ].reqs.alignment;
#endif
		memoryTypeBits &= attachments[ i ].reqs.memoryTypeBits;
		offset = PAD( offset, alignment );
		attachments[ i ].memory_offset = offset;
		offset += attachments[ i ].reqs.size;
#ifdef _DEBUG
		ri.Printf( PRINT_ALL, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
			attachments[ i ].reqs.memoryTypeBits,
			(int)attachments[ i ].reqs.size,
			(int)attachments[ i ].reqs.alignment );
#endif
	}

	if ( num_attachments == 1 && attachments[ 0 ].usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) {
		// try lazy memory
		memoryTypeIndex = find_memory_type2( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryTypeIndex == ~0U ) {
			memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		}
	} else {
		memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	}

#ifdef _DEBUG
	ri.Printf( PRINT_ALL, "memory type bits: %04x\n", memoryTypeBits );
	ri.Printf( PRINT_ALL, "memory type index: %04x\n", memoryTypeIndex );
	ri.Printf( PRINT_ALL, "total size: %i\n", (int)offset );
#endif

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = offset;
	alloc_info.memoryTypeIndex = memoryTypeIndex;

	if ( num_attachments == 1 ) {
		if ( vk.dedicatedAllocation ) {
			Com_Memset( &alloc_info2, 0, sizeof( alloc_info2 ) );
			alloc_info2.sType =  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
			alloc_info2.image = attachments[ 0 ].descriptor;
			alloc_info.pNext = &alloc_info2;
		}
	}

	// allocate and bind memory
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

	vk.image_memory[ vk.image_memory_count++ ] = memory;

	for ( i = 0; i < num_attachments; i++ ) {

		VK_CHECK( qvkBindImageMemory( vk.device, attachments[i].descriptor, memory, attachments[i].memory_offset ) );

		// create color image view
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.pNext = NULL;
		view_desc.flags = 0;
		view_desc.image = attachments[ i ].descriptor;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = attachments[ i ].image_format;
		view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.subresourceRange.aspectMask = attachments[ i ].aspect_flags;
		view_desc.subresourceRange.baseMipLevel = 0;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.baseArrayLayer = 0;
		view_desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, attachments[ i ].image_view ) );
	}

	// perform layout transition
	command_buffer = begin_command_buffer();
	for ( i = 0; i < num_attachments; i++ ) {
		record_image_layout_transition( command_buffer,
			attachments[i].descriptor,
			attachments[i].aspect_flags,
			VK_IMAGE_LAYOUT_UNDEFINED, // old_layout
			attachments[i].image_layout,
			0, 0 );
	}
	end_command_buffer( command_buffer, __func__ );

	num_attachments = 0;
}


static void vk_add_attachment_desc( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout )
{
	if ( num_attachments >= ARRAY_LEN( attachments ) ) {
		ri.Error( ERR_FATAL, "Attachments array overflow" );
	} else {
		attachments[ num_attachments ].descriptor = desc;
		attachments[ num_attachments ].image_view = image_view;
		attachments[ num_attachments ].usage = usage;
		attachments[ num_attachments ].reqs = *reqs;
		attachments[ num_attachments ].aspect_flags = aspect_flags;
		attachments[ num_attachments ].image_layout = image_layout;
		attachments[ num_attachments ].image_format = image_format;
		attachments[ num_attachments ].memory_offset = 0;
		num_attachments++;
	}
}


static void vk_get_image_memory_erquirements( VkImage image, VkMemoryRequirements *memory_requirements )
{
	if ( vk.dedicatedAllocation ) {
		VkMemoryRequirements2KHR memory_requirements2;
		VkImageMemoryRequirementsInfo2KHR image_requirements2;
		VkMemoryDedicatedRequirementsKHR mem_req2;

		Com_Memset( &mem_req2, 0, sizeof( mem_req2 ) );
		mem_req2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

		image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
		image_requirements2.image = image;
		image_requirements2.pNext = NULL;

		Com_Memset( &memory_requirements2, 0, sizeof( memory_requirements2 ) );
		memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
		memory_requirements2.pNext = &mem_req2;

		qvkGetImageMemoryRequirements2KHR( vk.device, &image_requirements2, &memory_requirements2 );

		*memory_requirements = memory_requirements2.memoryRequirements;
	} else {
		qvkGetImageMemoryRequirements( vk.device, image, memory_requirements );
	}
}


static void create_color_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format,
	VkImageUsageFlags usage, VkImage *image, VkImageView *image_view, VkImageLayout image_layout, qboolean multisample )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	if ( multisample && !( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) )
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	// create color image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout );
}


static void create_depth_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples,
	VkImage *image, VkImageView *image_view, qboolean allowTransient, qboolean sampled )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;

	// create depth image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( image == &vk.depth_image && vk.fboActive &&
		vk_scene_depth_snapshot_requested() &&
		samples == VK_SAMPLE_COUNT_1_BIT &&
		vk.liquidDepthSampleSupported ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if ( sampled ) {
		create_desc.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	} else if ( allowTransient ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
}


static qboolean vk_depth_format_sampled_supported( void )
{
	VkFormatProperties props;

	if ( vk.depth_format == VK_FORMAT_UNDEFINED ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( vk.physical_device,
		vk.depth_format, &props );
	return ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT ) ?
		qtrue : qfalse;
}


static void vk_invalidate_dlight_shadow_atlas_generation( void )
{
	vk.dlight_shadow_generation++;
	if ( !vk.dlight_shadow_generation ) {
		vk.dlight_shadow_generation++;
	}
}


static void vk_invalidate_spot_shadow_atlas_generation( void )
{
	vk.spot_shadow_generation++;
	if ( !vk.spot_shadow_generation ) {
		vk.spot_shadow_generation++;
	}
}


static void vk_invalidate_csm_shadow_atlas_generation( void )
{
	vk.csm_shadow_generation++;
	if ( !vk.csm_shadow_generation ) {
		vk.csm_shadow_generation++;
	}
}


static void vk_clear_dlight_shadow_atlas_layout( void )
{
	vk.dlight_shadow_atlas_width = 0;
	vk.dlight_shadow_atlas_height = 0;
	vk.dlight_shadow_face_size = 0;
	vk.dlight_shadow_atlas_columns = 0;
	vk.dlight_shadow_atlas_rows = 0;
	vk.dlight_shadow_max_lights = 0;
	vk.dlight_shadow_rendered = qfalse;
	vk_invalidate_dlight_shadow_atlas_generation();
}


static void vk_clear_spot_shadow_atlas_layout( void )
{
	vk.spot_shadow_atlas_width = 0;
	vk.spot_shadow_atlas_height = 0;
	vk.spot_shadow_tile_size = 0;
	vk.spot_shadow_atlas_columns = 0;
	vk.spot_shadow_atlas_rows = 0;
	vk.spot_shadow_max_lights = 0;
	vk.spot_shadow_rendered = qfalse;
	vk_invalidate_spot_shadow_atlas_generation();
}


static void vk_clear_csm_shadow_atlas_layout( void )
{
	vk.csm_shadow_atlas_width = 0;
	vk.csm_shadow_atlas_height = 0;
	vk.csm_shadow_cascade_size = 0;
	vk.csm_shadow_cascade_count = 0;
	vk.csm_shadow_rendered = qfalse;
	vk_invalidate_csm_shadow_atlas_generation();
}


static qboolean vk_dlight_shadow_atlas_layout( dlightShadowAtlasLayout_t *layout )
{
#ifdef USE_PMLIGHT
	qboolean correctnessMode =
		( r_shadowCorrectness && r_shadowCorrectness->integer ) ?
			qtrue : qfalse;

	if ( ( !correctnessMode &&
		( !r_dlightShadows || !r_dlightShadows->integer ||
		  !r_dlightShadowMaxLights || r_dlightShadowMaxLights->integer <= 0 ) ) ||
		!r_dlightMode || !R_GetDlightMode() ||
		!vk_depth_format_sampled_supported() ) {
		return qfalse;
	}

	return R_DlightShadowAtlasLayout( correctnessMode ? 1 : r_dlightShadowMaxLights->integer,
		r_dlightShadowResolution ? r_dlightShadowResolution->integer : 1024,
		glConfig.maxTextureSize, layout );
#else
	(void)layout;
	return qfalse;
#endif
}


static qboolean vk_spot_shadow_atlas_layout( spotShadowAtlasLayout_t *layout )
{
#ifdef USE_PMLIGHT
	if ( ( r_shadowCorrectness && r_shadowCorrectness->integer ) ||
		!r_spotShadows || !r_spotShadows->integer ||
		!r_spotShadowMaxLights || r_spotShadowMaxLights->integer <= 0 ||
		!vk_depth_format_sampled_supported() ) {
		return qfalse;
	}

	return R_SpotShadowAtlasLayout( r_spotShadowMaxLights->integer,
		r_spotShadowResolution ? r_spotShadowResolution->integer : 1024,
		glConfig.maxTextureSize, layout );
#else
	(void)layout;
	return qfalse;
#endif
}


static qboolean vk_csm_shadow_atlas_layout( csmShadowAtlasLayout_t *layout )
{
#ifdef USE_PMLIGHT
	if ( ( r_shadowCorrectness && r_shadowCorrectness->integer ) ||
		!r_csmShadows || !r_csmShadows->integer ||
		!vk_depth_format_sampled_supported() ) {
		return qfalse;
	}

	return R_CSMShadowAtlasLayout(
		r_csmCascadeCount ? r_csmCascadeCount->integer : CSM_MAX_CASCADES,
		r_csmResolution ? r_csmResolution->integer : 1024,
		gls.maxRenderTargetSize, layout );
#else
	(void)layout;
	return qfalse;
#endif
}


static void vk_store_dlight_shadow_atlas_layout(
	const dlightShadowAtlasLayout_t *layout )
{
	vk.dlight_shadow_atlas_width = layout->width;
	vk.dlight_shadow_atlas_height = layout->height;
	vk.dlight_shadow_face_size = layout->faceSize;
	vk.dlight_shadow_atlas_columns = layout->columns;
	vk.dlight_shadow_atlas_rows = layout->rows;
	vk.dlight_shadow_max_lights = layout->maxLights;
	vk_invalidate_dlight_shadow_atlas_generation();
}


static void vk_store_spot_shadow_atlas_layout(
	const spotShadowAtlasLayout_t *layout )
{
	vk.spot_shadow_atlas_width = layout->width;
	vk.spot_shadow_atlas_height = layout->height;
	vk.spot_shadow_tile_size = layout->tileSize;
	vk.spot_shadow_atlas_columns = layout->columns;
	vk.spot_shadow_atlas_rows = layout->rows;
	vk.spot_shadow_max_lights = layout->maxLights;
	vk_invalidate_spot_shadow_atlas_generation();
}


static void vk_store_csm_shadow_atlas_layout(
	const csmShadowAtlasLayout_t *layout )
{
	vk.csm_shadow_atlas_width = layout->width;
	vk.csm_shadow_atlas_height = layout->height;
	vk.csm_shadow_cascade_size = layout->cascadeSize;
	vk.csm_shadow_cascade_count = layout->cascadeCount;
	vk_invalidate_csm_shadow_atlas_generation();
}


static void create_dlight_shadow_attachment( uint32_t width, uint32_t height,
	VkImage *image, VkImageView *image_view )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	Com_Memset( &create_desc, 0, sizeof( create_desc ) );
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );
	vk_get_image_memory_erquirements( *image, &memory_requirements );
	vk_add_attachment_desc( *image, image_view, create_desc.usage,
		&memory_requirements, vk.depth_format, VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
}

static void create_liquid_depth_attachment( uint32_t width, uint32_t height )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	Com_Memset( &create_desc, 0, sizeof( create_desc ) );
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL,
		&vk.liquidDepth.image ) );
	vk_get_image_memory_erquirements( vk.liquidDepth.image,
		&memory_requirements );
	vk_add_attachment_desc( vk.liquidDepth.image,
		&vk.liquidDepth.image_view, create_desc.usage,
		&memory_requirements, vk.depth_format,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
}

static void vk_create_depth_sample_view( void )
{
	VkImageViewCreateInfo view;

	if ( ( !vk_global_fog_enabled() && !vk_underwater_depth_enabled() ) ||
		vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &view, 0, sizeof( view ) );
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.image = vk.depth_image;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = vk.depth_format;
	view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount = 1;

	VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &vk.depth_sample_image_view ) );
	SET_OBJECT_NAME( vk.depth_sample_image_view, "global fog depth sample view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
}


/*
================
vk_menu_blur_resolve_sizes

Size the soft-focus pyramid for the current render target and report whether it
is usable. The plan is asked for at full strength because the attachments are
allocated once, at renderer init, while the strength the client sends varies per
frame; only the sampling offsets change with strength, never the sizes.
================
*/
static qboolean vk_menu_blur_resolve_sizes( menuBlurPlan_t *plan )
{
	if ( !R_MenuBlur_Plan( 1.0f, glConfig.vidWidth, glConfig.vidHeight, plan ) ) {
		return qfalse;
	}

	vk.menu_blur_width[0] = plan->halfWidth;
	vk.menu_blur_height[0] = plan->halfHeight;
	vk.menu_blur_width[1] = plan->levelWidth;
	vk.menu_blur_height[1] = plan->levelHeight;
	vk.menu_blur_width[2] = plan->levelWidth;
	vk.menu_blur_height[2] = plan->levelHeight;

	return qtrue;
}


static void vk_create_menu_blur_attachments( VkImageUsageFlags usage )
{
	menuBlurPlan_t plan;
	uint32_t i;

	if ( !vk_menu_blur_resolve_sizes( &plan ) ) {
		return;
	}

	for ( i = 0; i < VK_NUM_MENU_BLUR_IMAGES; i++ ) {
		create_color_attachment( vk.menu_blur_width[i], vk.menu_blur_height[i],
			VK_SAMPLE_COUNT_1_BIT, vk.color_format, usage,
			&vk.menu_blur_image[i], &vk.menu_blur_image_view[i],
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
	}
}


static void vk_create_attachments( void )
{
	dlightShadowAtlasLayout_t shadowLayout;
	spotShadowAtlasLayout_t spotShadowLayout;
	csmShadowAtlasLayout_t csmShadowLayout;
	uint32_t i;

	vk_clear_attachment_pool();
	vk_clear_dlight_shadow_atlas_layout();
	vk_clear_spot_shadow_atlas_layout();
	vk_clear_csm_shadow_atlas_layout();

	// It looks like resulting performance depends from order you're creating/allocating
	// memory for attachments in vulkan i.e. similar images grouped together will provide best results
	// so [resolve0][resolve1][msaa0][msaa1][depth0][depth1] is most optimal
	// while cases like [resolve0][depth0][color0][...] is the worst

	// TODO: preallocate first image chunk in attachment' memory pool?
	if ( vk.fboActive ) {

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// bloom
		if ( r_bloom->integer ) {
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
				usage, &vk.bloom_image[0], &vk.bloom_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

			for ( i = 1; i < ARRAY_LEN( vk.bloom_image ); i += 2 ) {
				width /= 2;
				height /= 2;
				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+0], &vk.bloom_image_view[i+0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+1], &vk.bloom_image_view[i+1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			}
		}

		// post-processing/msaa-resolve
		create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			&vk.color_image, &vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
		if ( r_motionBlur && r_motionBlur->integer ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight,
				VK_SAMPLE_COUNT_1_BIT, vk.color_format, usage,
				&vk.motion_blur_image, &vk.motion_blur_image_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
		}
		if ( vk_underwater_requested() ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight,
				VK_SAMPLE_COUNT_1_BIT, vk.color_format, usage,
				&vk.underwater_image, &vk.underwater_image_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
		}
		vk_create_menu_blur_attachments( usage );

		if ( vk_liquid_requested() &&
			vk.liquidSnapshotWidth > 0 && vk.liquidSnapshotHeight > 0 ) {
			create_color_attachment( vk.liquidSnapshotWidth,
				vk.liquidSnapshotHeight, VK_SAMPLE_COUNT_1_BIT,
				vk.color_format, usage,
				&vk.liquidSnapshot.color_image,
				&vk.liquidSnapshot.color_image_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
		}

		// screenmap-msaa
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {
			create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.screenMap.color_image_msaa, &vk.screenMap.color_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue );
		}

		// screenmap/msaa-resolve
		create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage, &vk.screenMap.color_image, &vk.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		// screenmap depth
		create_depth_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples,
			&vk.screenMap.depth_image, &vk.screenMap.depth_image_view, qtrue, qfalse );

		if ( vk.msaaActive ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.msaa_image, &vk.msaa_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue );
		}

		/*
		 * Always retain an SDR capture target so screenshots, levelshots, and
		 * video frames observe the same final gamma/exposure/tone transform as
		 * presentation, including when supersampling is disabled.
		 */
		usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		create_color_attachment( gls.captureWidth, gls.captureHeight, VK_SAMPLE_COUNT_1_BIT, vk.capture_format,
			usage, &vk.capture.image, &vk.capture.image_view, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, qfalse );
	} // if ( vk.fboActive )

	//vk_alloc_attachments();

	/* The submerged view samples this attachment directly, exactly as the
	 * global-fog sidecar does, so it has to ask for the sampled usage and drop
	 * the transient hint on its own account.  Deriving both from the fog layer
	 * alone left the depth image attachment-only whenever a map had no fog
	 * sidecar, and the absorption then silently uploaded a zero density: the
	 * warp and the edge falloff still ran, the medium tint never appeared. */
	create_depth_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples,
		&vk.depth_image, &vk.depth_image_view,
		( vk.fboActive && ( r_bloom->integer || vk_global_fog_enabled() ||
			vk_underwater_depth_enabled() ||
			vk_scene_depth_snapshot_requested() ) ) ? qfalse : qtrue,
		vk_global_fog_enabled() || vk_underwater_depth_enabled() );
	if ( vk.fboActive && vk_scene_depth_snapshot_requested() &&
		vkSamples == VK_SAMPLE_COUNT_1_BIT &&
		vk.liquidDepthSampleSupported ) {
		create_liquid_depth_attachment( glConfig.vidWidth,
			glConfig.vidHeight );
	}

	if ( vk_dlight_shadow_atlas_layout( &shadowLayout ) ) {
		create_dlight_shadow_attachment( shadowLayout.width,
			shadowLayout.height, &vk.dlight_shadow_image,
			&vk.dlight_shadow_image_view );
		vk_store_dlight_shadow_atlas_layout( &shadowLayout );
		ri.Printf( PRINT_ALL,
			"...dynamic-light shadow atlas %ix%i (%i px faces, %i lights)\n",
			shadowLayout.width, shadowLayout.height, shadowLayout.faceSize,
			shadowLayout.maxLights );
	}

	if ( vk_spot_shadow_atlas_layout( &spotShadowLayout ) ) {
		create_dlight_shadow_attachment( spotShadowLayout.width,
			spotShadowLayout.height, &vk.spot_shadow_image,
			&vk.spot_shadow_image_view );
		vk_store_spot_shadow_atlas_layout( &spotShadowLayout );
		ri.Printf( PRINT_ALL,
			"...spotlight shadow atlas %ix%i (%i px tiles, %i lights)\n",
			spotShadowLayout.width, spotShadowLayout.height,
			spotShadowLayout.tileSize, spotShadowLayout.maxLights );
	}

	if ( vk_csm_shadow_atlas_layout( &csmShadowLayout ) ) {
		create_dlight_shadow_attachment( csmShadowLayout.width,
			csmShadowLayout.height, &vk.csm_shadow_image,
			&vk.csm_shadow_image_view );
		vk_store_csm_shadow_atlas_layout( &csmShadowLayout );
		ri.Printf( PRINT_ALL,
			"...sky-sun shadow atlas %ix%i (%i px cascades, %i cascades)\n",
			csmShadowLayout.width, csmShadowLayout.height,
			csmShadowLayout.cascadeSize, csmShadowLayout.cascadeCount );
	}

	vk_alloc_attachments();
	vk_create_depth_sample_view();

	for ( i = 0; i < vk.image_memory_count; i++ )
	{
		SET_OBJECT_NAME( vk.image_memory[i], va( "framebuffer memory chunk %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
	}

	SET_OBJECT_NAME( vk.depth_image, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.depth_image_view, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.dlight_shadow_image, "dlight shadow atlas", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.dlight_shadow_image_view, "dlight shadow atlas", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.spot_shadow_image, "spot shadow atlas", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.spot_shadow_image_view, "spot shadow atlas", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.csm_shadow_image, "csm shadow atlas", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.csm_shadow_image_view, "csm shadow atlas", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.color_image, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.color_image_view, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.motion_blur_image, "motion blur scratch attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.motion_blur_image_view, "motion blur scratch attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.liquidSnapshot.color_image, "liquid snapshot attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.liquidSnapshot.color_image_view, "liquid snapshot attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.liquidDepth.image, "liquid depth snapshot", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.liquidDepth.image_view, "liquid depth snapshot", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.capture.image, "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.capture.image_view, "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ )
	{
		SET_OBJECT_NAME( vk.bloom_image[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.bloom_image_view[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	for ( i = 0; i < VK_NUM_MENU_BLUR_IMAGES; i++ )
	{
		SET_OBJECT_NAME( vk.menu_blur_image[i], va( "menu soft focus attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.menu_blur_image_view[i], va( "menu soft focus attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	vk_lifetime_mark_create( RTX_VK_RESOURCE_ATTACHMENTS, __func__ );
}


static void vk_create_framebuffers( void )
{
	VkImageView framebufferAttachments[3];
	VkFramebufferCreateInfo desc;
	uint32_t n;

	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = framebufferAttachments;
	desc.layers = 1;

	for ( n = 0; n < vk.swapchain_image_count; n++ )
	{
		desc.renderPass = vk.render_pass.main;
		desc.attachmentCount = 2;
		if ( r_fbo->integer == 0 )
		{
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			framebufferAttachments[0] = vk.swapchain_image_views[n];
			framebufferAttachments[1] = vk.depth_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.main[n], va( "framebuffer - main %i", n ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
		else
		{
			// same framebuffer configuration for main and post-bloom render passes
			if ( n == 0 )
			{
				desc.width = glConfig.vidWidth;
				desc.height = glConfig.vidHeight;
				framebufferAttachments[0] = vk.color_image_view;
				framebufferAttachments[1] = vk.depth_image_view;
				if ( vk.msaaActive )
				{
					desc.attachmentCount = 3;
					framebufferAttachments[2] = vk.msaa_image_view;
				}
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );
				SET_OBJECT_NAME( vk.framebuffers.main[n], "framebuffer - main", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
			else
			{
				vk.framebuffers.main[n] = vk.framebuffers.main[0];
			}

			// gamma correction
			desc.renderPass = vk.render_pass.gamma;
			desc.attachmentCount = 1;
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			framebufferAttachments[0] = vk.swapchain_image_views[n];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.gamma[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.gamma[n], "framebuffer - gamma-correction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}

	if ( vk.render_pass.motion_blur != VK_NULL_HANDLE &&
		vk.motion_blur_image_view != VK_NULL_HANDLE ) {
		desc.renderPass = vk.render_pass.motion_blur;
		desc.attachmentCount = 1;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		framebufferAttachments[0] = vk.motion_blur_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL,
			&vk.framebuffers.motion_blur ) );
		SET_OBJECT_NAME( vk.framebuffers.motion_blur,
			"framebuffer - motion blur scratch",
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.render_pass.underwater != VK_NULL_HANDLE &&
		vk.underwater_image_view != VK_NULL_HANDLE ) {
		desc.renderPass = vk.render_pass.underwater;
		desc.attachmentCount = 1;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		framebufferAttachments[0] = vk.underwater_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL,
			&vk.framebuffers.underwater ) );
		SET_OBJECT_NAME( vk.framebuffers.underwater,
			"framebuffer - underwater view scratch",
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.render_pass.dlight_shadow != VK_NULL_HANDLE &&
		vk.dlight_shadow_image_view != VK_NULL_HANDLE ) {
		desc.renderPass = vk.render_pass.dlight_shadow;
		desc.attachmentCount = 1;
		desc.width = vk.dlight_shadow_atlas_width;
		desc.height = vk.dlight_shadow_atlas_height;
		framebufferAttachments[0] = vk.dlight_shadow_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL,
			&vk.framebuffers.dlight_shadow ) );
		SET_OBJECT_NAME( vk.framebuffers.dlight_shadow,
			"framebuffer - dlight shadow atlas",
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.render_pass.dlight_shadow != VK_NULL_HANDLE &&
		vk.spot_shadow_image_view != VK_NULL_HANDLE ) {
		desc.renderPass = vk.render_pass.dlight_shadow;
		desc.attachmentCount = 1;
		desc.width = vk.spot_shadow_atlas_width;
		desc.height = vk.spot_shadow_atlas_height;
		framebufferAttachments[0] = vk.spot_shadow_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL,
			&vk.framebuffers.spot_shadow ) );
		SET_OBJECT_NAME( vk.framebuffers.spot_shadow,
			"framebuffer - spot shadow atlas",
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.render_pass.dlight_shadow != VK_NULL_HANDLE &&
		vk.csm_shadow_image_view != VK_NULL_HANDLE ) {
		desc.renderPass = vk.render_pass.dlight_shadow;
		desc.attachmentCount = 1;
		desc.width = vk.csm_shadow_atlas_width;
		desc.height = vk.csm_shadow_atlas_height;
		framebufferAttachments[0] = vk.csm_shadow_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL,
			&vk.framebuffers.csm_shadow ) );
		SET_OBJECT_NAME( vk.framebuffers.csm_shadow,
			"framebuffer - csm shadow atlas",
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.fboActive )
	{
		if ( vk.render_pass.global_fog != VK_NULL_HANDLE ) {
			desc.renderPass = vk.render_pass.global_fog;
			desc.attachmentCount = 1;
			desc.width = glConfig.vidWidth;
			desc.height = glConfig.vidHeight;
			framebufferAttachments[0] = vk.color_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.global_fog ) );
			SET_OBJECT_NAME( vk.framebuffers.global_fog, "framebuffer - global fog", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		if ( vk.render_pass.liquid_snapshot != VK_NULL_HANDLE &&
			vk.liquidSnapshot.color_image_view != VK_NULL_HANDLE ) {
			desc.renderPass = vk.render_pass.liquid_snapshot;
			desc.attachmentCount = 1;
			desc.width = vk.liquidSnapshotWidth;
			desc.height = vk.liquidSnapshotHeight;
			framebufferAttachments[0] = vk.liquidSnapshot.color_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL,
				&vk.framebuffers.liquid_snapshot ) );
			SET_OBJECT_NAME( vk.framebuffers.liquid_snapshot,
				"framebuffer - liquid snapshot",
				VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		// screenmap
		desc.renderPass = vk.render_pass.screenmap;
		desc.attachmentCount = 2;
		desc.width = vk.screenMapWidth;
		desc.height = vk.screenMapHeight;
		framebufferAttachments[0] = vk.screenMap.color_image_view;
		framebufferAttachments[1] = vk.screenMap.depth_image_view;
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		{
			desc.attachmentCount = 3;
			framebufferAttachments[2] = vk.screenMap.color_image_view_msaa;
		}
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.screenmap ) );
		SET_OBJECT_NAME( vk.framebuffers.screenmap, "framebuffer - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		if ( vk.capture.image != VK_NULL_HANDLE )
		{
			framebufferAttachments[0] = vk.capture.image_view;

			desc.renderPass = vk.render_pass.capture;
			desc.pAttachments = framebufferAttachments;
			desc.attachmentCount = 1;
			desc.width = gls.captureWidth;
			desc.height = gls.captureHeight;

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.capture ) );
			SET_OBJECT_NAME( vk.framebuffers.capture, "framebuffer - capture", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		if ( vk.render_pass.menu_blur != VK_NULL_HANDLE )
		{
			// Menu soft focus. One render pass, three differently sized
			// framebuffers: render pass compatibility ignores extent.
			for ( n = 0; n < VK_NUM_MENU_BLUR_IMAGES; n++ )
			{
				if ( vk.menu_blur_image_view[n] == VK_NULL_HANDLE ) {
					continue;
				}

				desc.renderPass = vk.render_pass.menu_blur;
				desc.width = vk.menu_blur_width[n];
				desc.height = vk.menu_blur_height[n];
				desc.attachmentCount = 1;
				framebufferAttachments[0] = vk.menu_blur_image_view[n];

				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.menu_blur[n] ) );
				SET_OBJECT_NAME( vk.framebuffers.menu_blur[n], va( "framebuffer - menu soft focus %i", n ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
		}

		if ( r_bloom->integer )
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			// bloom color extraction
			desc.renderPass = vk.render_pass.bloom_extract;
			desc.width = width;
			desc.height = height;

			desc.attachmentCount = 1;
			framebufferAttachments[0] = vk.bloom_image_view[0];

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.bloom_extract ) );

			SET_OBJECT_NAME( vk.framebuffers.bloom_extract, "framebuffer - bloom extraction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n += 2 )
			{
				width /= 2;
				height /= 2;

				desc.renderPass = vk.render_pass.blur[n];
				desc.width = width;
				desc.height = height;

				desc.attachmentCount = 1;

				framebufferAttachments[0] = vk.bloom_image_view[n+0+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+0] ) );

				framebufferAttachments[0] = vk.bloom_image_view[n+1+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+1] ) );

				SET_OBJECT_NAME( vk.framebuffers.blur[n+0], va( "framebuffer - blur %i", n+0 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
				SET_OBJECT_NAME( vk.framebuffers.blur[n+1], va( "framebuffer - blur %i", n+1 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
		}
	}

	vk_lifetime_mark_create( RTX_VK_RESOURCE_FRAMEBUFFERS, __func__ );
}


static void vk_create_sync_primitives( void ) {
	VkSemaphoreCreateInfo desc;
	VkFenceCreateInfo fence_desc;
	uint32_t i;
	Com_Memset( &s_vkRtLatency, 0, sizeof( s_vkRtLatency ) );
	s_vkRtLatency.lastSubmittedSlot = -1;

	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.image_uploaded2 ) );
#endif

	// all commands submitted
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;

		// swapchain image acquired
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].image_acquired ) );

#ifdef USE_UPLOAD_QUEUE
		// second semaphore to synchronize additional tasks (e.g. image upload)
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].rendering_finished2 ) );
#endif
		fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_desc.pNext = NULL;
		//fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so it can be used to start rendering
		fence_desc.flags = 0; // non-signalled state

		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.tess[i].rendering_finished_fence ) );
		vk.tess[i].waitForFence = qfalse;

		SET_OBJECT_NAME( vk.tess[i].image_acquired, va( "image_acquired semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#ifdef USE_UPLOAD_QUEUE
		SET_OBJECT_NAME( vk.tess[i].rendering_finished2, va( "rendering_finished2 semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#endif
		SET_OBJECT_NAME( vk.tess[i].rendering_finished_fence, va( "rendering_finished fence %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );
	}

	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.aux_fence ) );
	SET_OBJECT_NAME( vk.aux_fence, "aux fence", VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
	vk.aux_fence_wait = qfalse;
#endif

	vk_lifetime_mark_create( RTX_VK_RESOURCE_SYNC_PRIMITIVES, __func__ );
}


static void vk_destroy_sync_primitives( void  ) {
	uint32_t i;

#ifdef USE_UPLOAD_QUEUE
	qvkDestroySemaphore( vk.device, vk.image_uploaded2, NULL );
#endif

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroySemaphore( vk.device, vk.tess[i].image_acquired, NULL );
#ifdef USE_UPLOAD_QUEUE
		qvkDestroySemaphore( vk.device, vk.tess[i].rendering_finished2, NULL );
#endif
		qvkDestroyFence( vk.device, vk.tess[i].rendering_finished_fence, NULL );
		vk.tess[i].waitForFence = qfalse;
		vk.tess[i].swapchain_image_acquired = qfalse;
	}

#ifdef USE_UPLOAD_QUEUE
	qvkDestroyFence( vk.device, vk.aux_fence, NULL );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
#endif

	vk_lifetime_mark_destroy( RTX_VK_RESOURCE_SYNC_PRIMITIVES, __func__ );
}


static void vk_destroy_framebuffers( void ) {
	uint32_t n;

	for ( n = 0; n < vk.swapchain_image_count; n++ ) {
		if ( vk.framebuffers.main[n] != VK_NULL_HANDLE ) {
			if ( !vk.fboActive || n == 0 ) {
				qvkDestroyFramebuffer( vk.device, vk.framebuffers.main[n], NULL );
			}
			vk.framebuffers.main[n] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.gamma[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[n], NULL );
			vk.framebuffers.gamma[n] = VK_NULL_HANDLE;
		}
	}

	for ( n = 0; n < VK_NUM_MENU_BLUR_IMAGES; n++ ) {
		if ( vk.framebuffers.menu_blur[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.menu_blur[n], NULL );
			vk.framebuffers.menu_blur[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.bloom_extract, NULL );
		vk.framebuffers.bloom_extract = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.screenmap, NULL );
		vk.framebuffers.screenmap = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.capture != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.capture, NULL );
		vk.framebuffers.capture = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.motion_blur != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.motion_blur, NULL );
		vk.framebuffers.motion_blur = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.underwater != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.underwater, NULL );
		vk.framebuffers.underwater = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.global_fog != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.global_fog, NULL );
		vk.framebuffers.global_fog = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.liquid_snapshot != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device,
			vk.framebuffers.liquid_snapshot, NULL );
		vk.framebuffers.liquid_snapshot = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.dlight_shadow != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.dlight_shadow, NULL );
		vk.framebuffers.dlight_shadow = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.spot_shadow != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.spot_shadow, NULL );
		vk.framebuffers.spot_shadow = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.csm_shadow != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.csm_shadow, NULL );
		vk.framebuffers.csm_shadow = VK_NULL_HANDLE;
	}

	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n++ ) {
		if ( vk.framebuffers.blur[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.blur[n], NULL );
			vk.framebuffers.blur[n] = VK_NULL_HANDLE;
		}
	}

	vk_lifetime_mark_destroy( RTX_VK_RESOURCE_FRAMEBUFFERS, __func__ );
}


static void vk_destroy_swapchain( void ) {
	uint32_t i;

	for ( i = 0; i < ARRAY_LEN( vk.swapchain_image_views ); i++ ) {
		if ( vk.swapchain_image_views[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, vk.swapchain_image_views[i], NULL );
			vk.swapchain_image_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.swapchain_rendering_finished[i] != VK_NULL_HANDLE ) {
			qvkDestroySemaphore( vk.device, vk.swapchain_rendering_finished[i], NULL );
			vk.swapchain_rendering_finished[i] = VK_NULL_HANDLE;
		}
		vk.swapchain_images[i] = VK_NULL_HANDLE;
	}

	if ( vk.swapchain != VK_NULL_HANDLE ) {
		qvkDestroySwapchainKHR( vk.device, vk.swapchain, NULL );
		vk.swapchain = VK_NULL_HANDLE;
	}
	vk.swapchain_image_count = 0;
	vk_lifetime_mark_destroy( RTX_VK_RESOURCE_SWAPCHAIN, __func__ );
}

static void vk_destroy_attachments( void );
static void vk_destroy_render_passes( void );
static void vk_destroy_pipelines( qboolean resetCount );
static void vk_set_render_scale( void );

static qboolean vk_restart_swapchain( const char *funcname, VkResult res )
{
	uint32_t i;

	if ( s_vkSwapchainResizeRestarting ) {
		vk_schedule_swapchain_resize( funcname, res, "restart already in progress" );
		return qfalse;
	}

	if ( !vk_swapchain_can_restart() ) {
		vk_schedule_swapchain_resize( funcname, res, "window is minimized or has zero extent" );
		return qfalse;
	}

#ifdef _DEBUG
	ri.Printf( PRINT_WARNING, "%s(%s): restarting swapchain...\n", funcname, vk_result_string( res ) );
#else
	ri.Printf(PRINT_WARNING, "%s(): restarting swapchain...\n", funcname );
#endif

	if ( !vk_select_surface_format( vk.physical_device, vk_surface ) ) {
		vk_schedule_swapchain_resize( funcname, res, "surface format query failed" );
		return qfalse;
	}
	setup_surface_formats( vk.physical_device );

	s_vkSwapchainResizeRestarting = qtrue;
	vk_clear_scheduled_swapchain_resize();

	vk_lifetime_begin_resize( funcname );
	vk_wait_idle();

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkResetCommandBuffer( vk.tess[i].command_buffer, 0 );
	}

#ifdef USE_UPLOAD_QUEUE
	qvkResetCommandBuffer( vk.staging_command_buffer, 0 );
#endif

	vk_destroy_pipelines( qfalse );
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain();
	vk_destroy_sync_primitives();

	vk.blitFilter = GL_NEAREST;
	vk.windowAdjusted = qfalse;
	vk.blitX0 = vk.blitY0 = 0;
	vk_set_render_scale();

	vk_create_sync_primitives();
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qfalse );
	vk_create_attachments();
	vk_create_render_passes();
	vk_create_framebuffers();

	vk_update_attachment_descriptors();

	vk_update_post_process_pipelines();
	vk_lifetime_end_resize( funcname );
	s_vkSwapchainResizeRestarting = qfalse;
	return qtrue;
}


static void vk_set_render_scale( void )
{
	if ( gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight )
	{
		if ( r_renderScale->integer > 0 )
		{
			int scaleMode = r_renderScale->integer - 1;
			if ( scaleMode & 1 )
			{
				// preserve aspect ratio (black bars on sides)
				float windowAspect = (float) gls.windowWidth / (float) gls.windowHeight;
				float renderAspect = (float) glConfig.vidWidth / (float) glConfig.vidHeight;
				if ( windowAspect >= renderAspect )
				{
					float scale = (float)gls.windowHeight / ( float ) glConfig.vidHeight;
					int bias = ( gls.windowWidth - scale * (float) glConfig.vidWidth ) / 2;
					vk.blitX0 += bias;
				}
				else
				{
					float scale = (float)gls.windowWidth / ( float ) glConfig.vidWidth;
					int bias = ( gls.windowHeight - scale * (float) glConfig.vidHeight ) / 2;
					vk.blitY0 += bias;
				}
			}
			// linear filtering
			if ( scaleMode & 2 )
				vk.blitFilter = GL_LINEAR;
			else
				vk.blitFilter = GL_NEAREST;
		}

		vk.windowAdjusted = qtrue;
	}

	if ( r_fbo->integer && r_ext_supersample->integer && !r_renderScale->integer )
	{
		vk.blitFilter = GL_LINEAR;
	}
}


void vk_initialize( void )
{
	char buf[64], driver_version[64];
	const char *vendor_name;
	VkPhysicalDeviceProperties props;
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	uint32_t maxSize;
	uint32_t i;

	vk_lifetime_begin_init();
	init_vulkan_library();

	qvkGetDeviceQueue( vk.device, vk.queue_family_index, 0, &vk.queue );
	vk.async_queue = VK_NULL_HANDLE;
	vk.async_queue_available = qfalse;
	if ( vk_rt_async_overlap_enabled() && vk.queue_family_queue_count > 1 ) {
		qvkGetDeviceQueue( vk.device, vk.queue_family_index, 1, &vk.async_queue );
		if ( vk.async_queue != VK_NULL_HANDLE ) {
			vk.async_queue_available = qtrue;
			ri.Printf( PRINT_ALL, "RTX RT: async queue overlap enabled (family=%u queues=%u)\n",
				vk.queue_family_index, vk.queue_family_queue_count );
		}
	}

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	vk.cmd = vk.tess + 0;
	vk.uniform_alignment = props.limits.minUniformBufferOffsetAlignment;
	{
		const size_t alignment = (size_t)vk.uniform_alignment;
		vk.uniform_item_size = (uint32_t)( ( sizeof( vkUniform_t ) + alignment - 1 ) & ~( alignment - 1 ) );
	}

	// for flare visibility tests
	vk.storage_alignment = MAX( props.limits.minStorageBufferOffsetAlignment, sizeof( uint32_t ) );

	vk.maxAnisotropy = props.limits.maxSamplerAnisotropy;
	vk.lineWidthRange[0] = props.limits.lineWidthRange[0];
	vk.lineWidthRange[1] = props.limits.lineWidthRange[1];

	vk.blitFilter = GL_NEAREST;
	vk.windowAdjusted = qfalse;
	vk.blitX0 = vk.blitY0 = 0;

	vk_set_render_scale();

	if ( r_fbo->integer ) {
		const qboolean rtSingleSampleComposition =
			( vk.caps.activeRtMode == RTX_RT_MODE_RAY_TRACING_PIPELINE ) ?
				qtrue : qfalse;
		const qboolean depthFadeSingleSampleComposition =
			( vk_depth_fade_requested() &&
			  vk.liquidDepthSampleSupported &&
			  props.limits.maxBoundDescriptorSets > VK_DESC_DEPTH_FADE ) ?
				qtrue : qfalse;
		const qboolean singleSampleComposition =
			( rtSingleSampleComposition ||
			  depthFadeSingleSampleComposition ) ? qtrue : qfalse;

		vk.fboActive = qtrue;
		if ( r_ext_multisample->integer && !singleSampleComposition ) {
			vk.msaaActive = qtrue;
		} else if ( r_ext_multisample->integer && singleSampleComposition ) {
			if ( rtSingleSampleComposition ) {
				ri.Printf( PRINT_WARNING,
					"RTX RT: disabling raster MSAA; temporal/spatial RT reconstruction supplies antialiasing and the post-RT overlay is single-sample\n" );
			} else {
				ri.Printf( PRINT_WARNING,
					"RTX: disabling raster MSAA so soft-particle depth fade can sample the active scene depth; set r_depthFade 0 and vid_restart to retain MSAA\n" );
			}
		}
	} else {
		vk.fboActive = qfalse;
	}

	// multisampling

	vkMaxSamples = MIN( props.limits.sampledImageColorSampleCounts, props.limits.sampledImageDepthSampleCounts );

	if ( /*vk.fboActive &&*/ vk.msaaActive ) {
		VkSampleCountFlags mask = vkMaxSamples;
		vkSamples = MAX( log2pad( r_ext_multisample->integer, 1 ), VK_SAMPLE_COUNT_2_BIT );
		while ( vkSamples > mask )
				vkSamples >>= 1;
		ri.Printf( PRINT_ALL, "...using %ix MSAA\n", vkSamples );
	} else {
		vkSamples = VK_SAMPLE_COUNT_1_BIT;
	}

	if ( vk_global_fog_requested() && vkSamples != VK_SAMPLE_COUNT_1_BIT ) {
		ri.Printf( PRINT_WARNING,
			"RTX: per-map global fog is unavailable with raster MSAA enabled; disable r_ext_multisample or use RT pipeline mode 2\n" );
	}
	if ( vk_depth_fade_requested() &&
		vkSamples != VK_SAMPLE_COUNT_1_BIT ) {
		ri.Printf( PRINT_WARNING,
			"RTX: soft-particle depth fade is unavailable with raster MSAA enabled; authored particle blending remains active\n" );
	}
	if ( vk_liquid_requested() && vkSamples != VK_SAMPLE_COUNT_1_BIT ) {
		ri.Printf( PRINT_WARNING,
			"RTX: enhanced liquids keep color refraction/reflection with raster MSAA, but foreground depth rejection requires single-sample composition (or RT pipeline mode 2)\n" );
	}

	vk.screenMapSamples = MIN( vkMaxSamples, VK_SAMPLE_COUNT_4_BIT );

	vk.screenMapWidth = (float) glConfig.vidWidth / 16.0;
	if ( vk.screenMapWidth < 4 )
		vk.screenMapWidth = 4;

	vk.screenMapHeight = (float) glConfig.vidHeight / 16.0;
	if ( vk.screenMapHeight < 4 )
		vk.screenMapHeight = 4;

	vk.liquidSnapshotWidth = vk.liquidSnapshotHeight = 0;
	if ( vk_liquid_requested() ) {
		const float scale = Com_Clamp( 0.25f, 1.0f,
			r_liquidResolution ? r_liquidResolution->value : 1.0f );

		vk.liquidSnapshotWidth =
			MAX( 64, (uint32_t)( glConfig.vidWidth * scale + 0.5f ) );
		vk.liquidSnapshotHeight =
			MAX( 64, (uint32_t)( glConfig.vidHeight * scale + 0.5f ) );
	}

	// fill glConfig information

	// Render targets have dedicated attachments, not resampled image chunks.
	gls.maxRenderTargetSize = (int)MIN( props.limits.maxImageDimension2D,
		MIN( MIN( props.limits.maxFramebufferWidth, props.limits.maxFramebufferHeight ),
		MIN( props.limits.maxViewportDimensions[0], props.limits.maxViewportDimensions[1] ) ) );

	// maxTextureSize must not exceed IMAGE_CHUNK_SIZE
	maxSize = sqrtf( IMAGE_CHUNK_SIZE / 4 );
	// round down to next power of 2
	glConfig.maxTextureSize = MIN( props.limits.maxImageDimension2D, log2pad( maxSize, 0 ) );

	if ( glConfig.maxTextureSize > MAX_TEXTURE_SIZE )
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	// default chunk size, may be doubled on demand
	vk.image_chunk_size = IMAGE_CHUNK_SIZE;

	vk.maxLod = 1 + Q_log2( glConfig.maxTextureSize );

	if ( props.limits.maxPerStageDescriptorSamplers != 0xFFFFFFFF )
		glConfig.numTextureUnits = props.limits.maxPerStageDescriptorSamplers;
	else
		glConfig.numTextureUnits = props.limits.maxBoundDescriptorSets;
	if ( glConfig.numTextureUnits > MAX_TEXTURE_UNITS )
		glConfig.numTextureUnits = MAX_TEXTURE_UNITS;

	vk.maxBoundDescriptorSets = props.limits.maxBoundDescriptorSets;

	if ( r_ext_texture_env_add->integer != 0 )
		glConfig.textureEnvAddAvailable = qtrue;
	else
		glConfig.textureEnvAddAvailable = qfalse;

	glConfig.textureCompression = TC_NONE;

	major = VK_VERSION_MAJOR(props.apiVersion);
	minor = VK_VERSION_MINOR(props.apiVersion);
	patch = VK_VERSION_PATCH(props.apiVersion);

	// decode driver version
	switch ( props.vendorID ) {
		case 0x10DE: // NVidia
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i.%i",
				(props.driverVersion >> 22) & 0x3FF,
				(props.driverVersion >> 14) & 0x0FF,
				(props.driverVersion >> 6) & 0x0FF,
				(props.driverVersion >> 0) & 0x03F );
			break;
#ifdef _WIN32
		case 0x8086: // Intel
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i",
				(props.driverVersion >> 14),
				(props.driverVersion >> 0) & 0x3FFF );
			break;
#endif
		default:
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i",
				(props.driverVersion >> 22),
				(props.driverVersion >> 12) & 0x3FF,
				(props.driverVersion >> 0) & 0xFFF );
	}

	Com_sprintf( glConfig.version_string, sizeof( glConfig.version_string ), "API: %i.%i.%i, Driver: %s",
		major, minor, patch, driver_version );

	vk.offscreenRender = qtrue;

	if ( props.vendorID == 0x1002 ) {
		vendor_name = "Advanced Micro Devices, Inc.";
	} else if ( props.vendorID == 0x106B ) {
		vendor_name = "Apple Inc.";
	} else if ( props.vendorID == 0x10DE ) {
		// https://github.com/SaschaWillems/Vulkan/issues/493
		// we can't render to offscreen presentation surfaces on nvidia
		vk.offscreenRender = qfalse;
		vendor_name = "NVIDIA";
	} else if ( props.vendorID == 0x14E4 ) {
		vendor_name = "Broadcom Inc.";
	} else if ( props.vendorID == 0x1AE0 ) {
		vendor_name = "Google Inc.";
	} else if ( props.vendorID == 0x8086 ) {
		vendor_name = "Intel Corporation";
	} else if ( props.vendorID == VK_VENDOR_ID_MESA ) {
		vendor_name = "MESA";
	} else {
		Com_sprintf( buf, sizeof( buf ), "VendorID: %04x", props.vendorID );
		vendor_name = buf;
	}

	Q_strncpyz( glConfig.vendor_string, vendor_name, sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, renderer_name( &props ), sizeof( glConfig.renderer_string ) );

	SET_OBJECT_NAME( (intptr_t)vk.device, glConfig.renderer_string, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT );

	//
	// Sync primitives.
	//
	vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		VkCommandPoolCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		desc.queueFamilyIndex = vk.queue_family_index;

		VK_CHECK( qvkCreateCommandPool( vk.device, &desc, NULL, &vk.command_pool ) );

		SET_OBJECT_NAME( vk.command_pool, "command pool", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT );
	}

#ifdef USE_UPLOAD_QUEUE
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.staging_command_buffer ) );
	}
#endif

	//
	// Command buffers and color attachments.
	//
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.tess[i].command_buffer ) );

		//SET_OBJECT_NAME( vk.tess[i].command_buffer, va( "command buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}

	//
	// Descriptor pool.
	//
	{
		VkDescriptorPoolSize pool_size[3];
		VkDescriptorPoolCreateInfo desc;
		uint32_t poolIndex, maxSets;

		pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		// color, motion, underwater, screenmap, fog depth, liquid
		// source/color/depth, shadow atlases, the menu soft-focus pyramid, then
		// the bloom extract descriptor plus one pair per blur pass --
		// ARRAY_LEN( vk.bloom_image_descriptor ), which is
		// 1 + VK_NUM_BLOOM_PASSES * 2, not VK_NUM_BLOOM_PASSES * 2
		pool_size[0].descriptorCount =
			MAX_DRAWIMAGES + 1 + 1 + 1 + 1 + 1 + 3 + 3 +
			VK_NUM_MENU_BLUR_IMAGES + ( 1 + VK_NUM_BLOOM_PASSES * 2 );

		pool_size[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size[1].descriptorCount = NUM_COMMAND_BUFFERS;

		//pool_size[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		//pool_size[2].descriptorCount = NUM_COMMAND_BUFFERS;

		pool_size[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size[2].descriptorCount = 1;

		for ( poolIndex = 0, maxSets = 0; poolIndex < ARRAY_LEN( pool_size ); poolIndex++ ) {
			maxSets += pool_size[poolIndex].descriptorCount;
		}

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.maxSets = maxSets;
		desc.poolSizeCount = ARRAY_LEN( pool_size );
		desc.pPoolSizes = pool_size;

		VK_CHECK( qvkCreateDescriptorPool( vk.device, &desc, NULL, &vk.descriptor_pool ) );
	}

	//
	// Descriptor set layout.
	//
	vk_create_descriptor_layout_registry();

	//
	// Pipeline layouts.
	//
	vk_create_pipeline_layout_registry();

	vk.geometry_buffer_size_new = VERTEX_BUFFER_SIZE;
	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	vk_create_storage_buffer( MAX_FLARES * vk.storage_alignment );

	vk_create_shader_modules();

	{
		VkPipelineCacheCreateInfo ci;
		Com_Memset( &ci, 0, sizeof( ci ) );
		ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK( qvkCreatePipelineCache( vk.device, &ci, NULL, &vk.pipelineCache ) );
	}

	vk.renderPassIndex = RENDER_PASS_MAIN; // default render pass

	// swapchain
	vk.initSwapchainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//vk.initSwapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qtrue );

	// color/depth attachments
	vk_create_attachments();

	// renderpasses
	vk_create_render_passes();

	// framebuffers for each swapchain image
	vk_create_framebuffers();

	vk.active = qtrue;
	vk_lifetime_end_init();
}


void vk_create_pipelines( void )
{
	vk_alloc_persistent_pipelines();

	vk.pipelines_world_base = vk.pipelines_count;

	// The path tracer's pipelines are compiled now, before a level's images
	// and world take the address space: a 32-bit client's driver compiles the
	// compute backend's trace in the same 2 GB, and fails when a large level
	// has left it too little. A failure here leaves the first traced frame
	// to try again.
	if ( vk_rt_trace_available() && ( vk.rt.initialized || vk_rt_init() ) && !RT_EnsurePipelines() ) {
		vk_rt_shutdown();
	}
}


static void vk_destroy_attachments( void )
{
	uint32_t i;

	if ( vk.bloom_image[0] ) {
		for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ ) {
			qvkDestroyImage( vk.device, vk.bloom_image[i], NULL );
			qvkDestroyImageView( vk.device, vk.bloom_image_view[i], NULL );
			vk.bloom_image[i] = VK_NULL_HANDLE;
			vk.bloom_image_view[i] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < VK_NUM_MENU_BLUR_IMAGES; i++ ) {
		if ( vk.menu_blur_image[i] != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, vk.menu_blur_image[i], NULL );
			qvkDestroyImageView( vk.device, vk.menu_blur_image_view[i], NULL );
			vk.menu_blur_image[i] = VK_NULL_HANDLE;
			vk.menu_blur_image_view[i] = VK_NULL_HANDLE;
		}
		vk.menu_blur_width[i] = 0;
		vk.menu_blur_height[i] = 0;
	}

	if ( vk.color_image ) {
		qvkDestroyImage( vk.device, vk.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.color_image_view, NULL );
		vk.color_image = VK_NULL_HANDLE;
		vk.color_image_view = VK_NULL_HANDLE;
	}
	if ( vk.motion_blur_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.motion_blur_image_view, NULL );
		qvkDestroyImage( vk.device, vk.motion_blur_image, NULL );
		vk.motion_blur_image = VK_NULL_HANDLE;
		vk.motion_blur_image_view = VK_NULL_HANDLE;
		vk.motion_blur_descriptor = VK_NULL_HANDLE;
	}
	if ( vk.underwater_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.underwater_image_view, NULL );
		qvkDestroyImage( vk.device, vk.underwater_image, NULL );
		vk.underwater_image = VK_NULL_HANDLE;
		vk.underwater_image_view = VK_NULL_HANDLE;
		vk.underwater_descriptor = VK_NULL_HANDLE;
	}
	R_MotionBlur_ResetView( &vk_motion_blur_view );

	if ( vk.msaa_image ) {
		qvkDestroyImage( vk.device, vk.msaa_image, NULL );
		qvkDestroyImageView( vk.device, vk.msaa_image_view, NULL );
		vk.msaa_image = VK_NULL_HANDLE;
		vk.msaa_image_view = VK_NULL_HANDLE;
	}

	if ( vk.depth_sample_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.depth_sample_image_view, NULL );
		vk.depth_sample_image_view = VK_NULL_HANDLE;
	}
	if ( vk.liquidSnapshot.color_image ) {
		qvkDestroyImageView( vk.device,
			vk.liquidSnapshot.color_image_view, NULL );
		qvkDestroyImage( vk.device,
			vk.liquidSnapshot.color_image, NULL );
		vk.liquidSnapshot.color_image = VK_NULL_HANDLE;
		vk.liquidSnapshot.color_image_view = VK_NULL_HANDLE;
	}
	if ( vk.liquidDepth.image ) {
		qvkDestroyImageView( vk.device,
			vk.liquidDepth.image_view, NULL );
		qvkDestroyImage( vk.device, vk.liquidDepth.image, NULL );
		vk.liquidDepth.image = VK_NULL_HANDLE;
		vk.liquidDepth.image_view = VK_NULL_HANDLE;
		vk.liquidDepth.copied = qfalse;
	}
	qvkDestroyImageView( vk.device, vk.depth_image_view, NULL );
	qvkDestroyImage( vk.device, vk.depth_image, NULL );
	vk.depth_image = VK_NULL_HANDLE;
	vk.depth_image_view = VK_NULL_HANDLE;

	if ( vk.dlight_shadow_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.dlight_shadow_image_view, NULL );
		vk.dlight_shadow_image_view = VK_NULL_HANDLE;
	}
	if ( vk.dlight_shadow_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.dlight_shadow_image, NULL );
		vk.dlight_shadow_image = VK_NULL_HANDLE;
	}
	if ( vk.spot_shadow_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.spot_shadow_image_view, NULL );
		vk.spot_shadow_image_view = VK_NULL_HANDLE;
	}
	if ( vk.spot_shadow_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.spot_shadow_image, NULL );
		vk.spot_shadow_image = VK_NULL_HANDLE;
	}
	if ( vk.csm_shadow_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.csm_shadow_image_view, NULL );
		vk.csm_shadow_image_view = VK_NULL_HANDLE;
	}
	if ( vk.csm_shadow_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.csm_shadow_image, NULL );
		vk.csm_shadow_image = VK_NULL_HANDLE;
	}
	if ( vk.screenMap.color_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view, NULL );
		vk.screenMap.color_image = VK_NULL_HANDLE;
		vk.screenMap.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.color_image_msaa ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image_msaa, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view_msaa, NULL );
		vk.screenMap.color_image_msaa = VK_NULL_HANDLE;
		vk.screenMap.color_image_view_msaa = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.depth_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.depth_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.depth_image_view, NULL );
		vk.screenMap.depth_image = VK_NULL_HANDLE;
		vk.screenMap.depth_image_view = VK_NULL_HANDLE;
	}

	if ( vk.capture.image ) {
		qvkDestroyImage( vk.device, vk.capture.image, NULL );
		qvkDestroyImageView( vk.device, vk.capture.image_view, NULL );
		vk.capture.image = VK_NULL_HANDLE;
		vk.capture.image_view = VK_NULL_HANDLE;
	}

	for ( i = 0; i < vk.image_memory_count; i++ ) {
		qvkFreeMemory( vk.device, vk.image_memory[i], NULL );
	}

	vk.image_memory_count = 0;
	vk_clear_dlight_shadow_atlas_layout();
	vk_clear_spot_shadow_atlas_layout();
	vk_clear_csm_shadow_atlas_layout();
	vk_lifetime_mark_destroy( RTX_VK_RESOURCE_ATTACHMENTS, __func__ );
}


static void vk_destroy_render_passes( void )
{
	uint32_t i;

	if ( vk.render_pass.main != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main, NULL );
		vk.render_pass.main = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.main_load != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main_load, NULL );
		vk.render_pass.main_load = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.bloom_extract, NULL );
		vk.render_pass.bloom_extract = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.menu_blur != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.menu_blur, NULL );
		vk.render_pass.menu_blur = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
		if ( vk.render_pass.blur[i] != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, vk.render_pass.blur[i], NULL );
			vk.render_pass.blur[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.render_pass.post_bloom != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.post_bloom, NULL );
		vk.render_pass.post_bloom = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.global_fog != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.global_fog, NULL );
		vk.render_pass.global_fog = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.liquid_snapshot != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device,
			vk.render_pass.liquid_snapshot, NULL );
		vk.render_pass.liquid_snapshot = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.screenmap, NULL );
		vk.render_pass.screenmap = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.gamma != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.gamma, NULL );
		vk.render_pass.gamma = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.capture != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.capture, NULL );
		vk.render_pass.capture = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.motion_blur != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.motion_blur, NULL );
		vk.render_pass.motion_blur = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.underwater != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.underwater, NULL );
		vk.render_pass.underwater = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.dlight_shadow != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.dlight_shadow, NULL );
		vk.render_pass.dlight_shadow = VK_NULL_HANDLE;
	}

	vk_lifetime_mark_destroy( RTX_VK_RESOURCE_RENDER_PASSES, __func__ );
}


static void vk_destroy_pipelines( qboolean resetCounter )
{
	uint32_t i, j;

	for ( i = 0; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
	}

	if ( resetCounter ) {
		Com_Memset( &vk.pipelines, 0, sizeof( vk.pipelines ) );
		vk.pipelines_count = 0;
	}

	if ( vk.gamma_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.gamma_pipeline, NULL );
		vk.gamma_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.capture_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.capture_pipeline, NULL );
		vk.capture_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_extract_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_extract_pipeline, NULL );
		vk.bloom_extract_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_blend_pipeline, NULL );
		vk.bloom_blend_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.bloom_blend_cel_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_blend_cel_pipeline, NULL );
		vk.bloom_blend_cel_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.motion_blur_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.motion_blur_pipeline, NULL );
		vk.motion_blur_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.menu_blur_half_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.menu_blur_half_pipeline, NULL );
		vk.menu_blur_half_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.menu_blur_level_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.menu_blur_level_pipeline, NULL );
		vk.menu_blur_level_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.menu_blur_composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.menu_blur_composite_pipeline, NULL );
		vk.menu_blur_composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.motion_blur_copy_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.motion_blur_copy_pipeline, NULL );
		vk.motion_blur_copy_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.underwater_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.underwater_pipeline, NULL );
		vk.underwater_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.underwater_copy_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.underwater_copy_pipeline, NULL );
		vk.underwater_copy_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.world_outline_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.world_outline_pipeline, NULL );
		vk.world_outline_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.global_fog_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.global_fog_pipeline, NULL );
		vk.global_fog_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.liquid_snapshot_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device,
			vk.liquid_snapshot_pipeline, NULL );
		vk.liquid_snapshot_pipeline = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i++ ) {
		if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
			vk.blur_pipeline[i] = VK_NULL_HANDLE;
		}
	}
}


void vk_shutdown( refShutdownCode_t code )
{
	int i, j, k, l;

	vk_lifetime_begin_shutdown();
	if ( qvkQueuePresentKHR == NULL ) { // not fully initialized
		goto __cleanup;
	}

	vk_rt_shutdown();

	vk_destroy_framebuffers();

	vk_destroy_pipelines( qtrue ); // reset counter

	vk_destroy_render_passes();

	vk_destroy_attachments();

	vk_destroy_swapchain();

	if ( vk.pipelineCache != VK_NULL_HANDLE ) {
		qvkDestroyPipelineCache( vk.device, vk.pipelineCache, NULL );
		vk.pipelineCache = VK_NULL_HANDLE;
	}

	qvkDestroyCommandPool( vk.device, vk.command_pool, NULL );

	qvkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);

	vk_destroy_pipeline_layout_registry();
	vk_destroy_descriptor_layout_registry();

#ifdef USE_VBO
	vk_release_vbo();
#endif

	vk_release_geometry_buffers();

	vk_destroy_sync_primitives();
	vk_destroy_storage_buffer();

	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					if ( vk.modules.vert.gen[i][j][k][l] != VK_NULL_HANDLE ) {
						qvkDestroyShaderModule( vk.device, vk.modules.vert.gen[i][j][k][l], NULL );
						vk.modules.vert.gen[i][j][k][l] = VK_NULL_HANDLE;
					}
				}
			}
		}
	}
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				if ( vk.modules.frag.gen[i][j][k] != VK_NULL_HANDLE ) {
					qvkDestroyShaderModule( vk.device, vk.modules.frag.gen[i][j][k], NULL );
					vk.modules.frag.gen[i][j][k] = VK_NULL_HANDLE;
				}
			}
		}
	}
	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.vert.light[i] != VK_NULL_HANDLE ) {
			qvkDestroyShaderModule( vk.device, vk.modules.vert.light[i], NULL );
			vk.modules.vert.light[i] = VK_NULL_HANDLE;
		}
		for ( j = 0; j < 2; j++ ) {
			if ( vk.modules.frag.light[i][j] != VK_NULL_HANDLE ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.light[i][j], NULL );
				vk.modules.frag.light[i][j] = VK_NULL_HANDLE;
			}
		}
	}

	if ( vk.modules.vert.csm_shadow != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.vert.csm_shadow, NULL );
		vk.modules.vert.csm_shadow = VK_NULL_HANDLE;
	}
	if ( vk.modules.frag.csm_shadow != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.frag.csm_shadow, NULL );
		vk.modules.frag.csm_shadow = VK_NULL_HANDLE;
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.ident1[i][j][k], NULL );
				vk.modules.vert.ident1[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ident1[i][j], NULL );
			vk.modules.frag.ident1[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.fixed[i][j][k], NULL );
				vk.modules.vert.fixed[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.fixed[i][j], NULL );
			vk.modules.frag.fixed[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 1; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ent[i][j], NULL );
			vk.modules.frag.ent[i][j] = VK_NULL_HANDLE;
		}
	}

	qvkDestroyShaderModule( vk.device, vk.modules.frag.gen0_df, NULL );

	qvkDestroyShaderModule( vk.device, vk.modules.color_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.color_vs, NULL );

	qvkDestroyShaderModule(vk.device, vk.modules.fog_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fog_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.dot_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.dot_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.liquid_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.liquid_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.liquid_copy_fs, NULL);
	vk.modules.liquid_vs = VK_NULL_HANDLE;
	vk.modules.liquid_fs = VK_NULL_HANDLE;
	vk.modules.liquid_copy_fs = VK_NULL_HANDLE;

	qvkDestroyShaderModule(vk.device, vk.modules.bloom_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blur_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blend_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.motion_blur_fs, NULL);
	/* Created unconditionally beside the other post-process modules; it was the
	 * only one with no matching destroy, so every vid_restart leaked one. */
	qvkDestroyShaderModule(vk.device, vk.modules.menu_blur_fs, NULL);
	vk.modules.menu_blur_fs = VK_NULL_HANDLE;
	qvkDestroyShaderModule(vk.device, vk.modules.world_outline_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.global_fog_fs, NULL);
	vk.modules.global_fog_fs = VK_NULL_HANDLE;
	if ( vk.modules.underwater_fs != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.underwater_fs, NULL );
		vk.modules.underwater_fs = VK_NULL_HANDLE;
	}

	qvkDestroyShaderModule(vk.device, vk.modules.gamma_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.gamma_fs, NULL);

	if ( vk.modules.rt_rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_rgen, NULL );
		vk.modules.rt_rgen = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_rmiss != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_rmiss, NULL );
		vk.modules.rt_rmiss = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_rmiss_shadow != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_rmiss_shadow, NULL );
		vk.modules.rt_rmiss_shadow = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_rchit, NULL );
		vk.modules.rt_rchit = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_rahit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_rahit, NULL );
		vk.modules.rt_rahit = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_trace != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_trace, NULL );
		vk.modules.rt_trace = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_rmiss_filter != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_rmiss_filter, NULL );
		vk.modules.rt_rmiss_filter = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_rahit_filter != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_rahit_filter, NULL );
		vk.modules.rt_rahit_filter = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_rahit_shadow != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_rahit_shadow, NULL );
		vk.modules.rt_rahit_shadow = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_composite != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_composite, NULL );
		vk.modules.rt_composite = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_tonemap != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_tonemap, NULL );
		vk.modules.rt_tonemap = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_denoise_temporal != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_denoise_temporal, NULL );
		vk.modules.rt_denoise_temporal = VK_NULL_HANDLE;
	}
	if ( vk.modules.rt_denoise_atrous != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, vk.modules.rt_denoise_atrous, NULL );
		vk.modules.rt_denoise_atrous = VK_NULL_HANDLE;
	}

__cleanup:
	vk_lifetime_end_shutdown();
	if ( vk.device != VK_NULL_HANDLE ) {
		qvkDestroyDevice( vk.device, NULL );
	}

	deinit_device_functions();

	Com_Memset( &vk, 0, sizeof( vk ) );
	Com_Memset( &vk_world, 0, sizeof( vk_world ) );
	
	if ( code != REF_KEEP_CONTEXT ) {
		vk_destroy_instance();
		deinit_instance_functions();
	}
}


void vk_wait_idle( void )
{
	VK_CHECK( qvkDeviceWaitIdle( vk.device ) );
}


void vk_queue_wait_idle( void )
{
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
}


void vk_release_resources( refShutdownCode_t code ) {
	int i, j;

	/* Capability rejection can call RE_Shutdown before a device or its
	 * function table exists. There are no device-owned resources to release. */
	if ( vk.device == VK_NULL_HANDLE || qvkDeviceWaitIdle == NULL ) {
		return;
	}
	vk_wait_idle();
#ifdef USE_UPLOAD_QUEUE
	if ( code == REF_KEEP_CONTEXT ) {
		/*
		 * A registration restart can happen after a final staging submit but
		 * before a frame consumes its binary upload semaphore. Device idle
		 * completes that submit but does not return binary semaphores to the
		 * unsignaled state. Reset the reusable staging command buffer and
		 * rebuild the upload/frame sync chain before the retained device is
		 * used for the next registration.
		 */
		vk_wait_staging_buffer();
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
		vk_destroy_sync_primitives();
	}
#endif
	vk_rt_shutdown();

	for (i = 0; i < vk_world.num_image_chunks; i++)
		qvkFreeMemory(vk.device, vk_world.image_chunks[i].memory, NULL);

	vk_clean_staging_buffer();

	for (i = 0; i < vk_world.num_samplers; i++)
		qvkDestroySampler(vk.device, vk_world.samplers[i], NULL);

	for ( i = vk.pipelines_world_base; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
		Com_Memset( &vk.pipelines[i], 0, sizeof( vk.pipelines[0] ) );
	}
	vk.pipelines_count = vk.pipelines_world_base;

	if ( vk.descriptor_pool != VK_NULL_HANDLE ) {
		VK_CHECK( qvkResetDescriptorPool( vk.device, vk.descriptor_pool, 0 ) );
	}

	if ( vk_world.num_image_chunks > 1 ) {
		// if we allocated more than 2 image chunks - use doubled default size
		vk.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	} else if ( vk_world.num_image_chunks == 1 ) {
		// otherwise set to default if used less than a half
		if ( vk_world.image_chunks[0].used < ( IMAGE_CHUNK_SIZE - (IMAGE_CHUNK_SIZE / 10) ) ) {
			vk.image_chunk_size = IMAGE_CHUNK_SIZE;
		}
	}

	Com_Memset( &vk_world, 0, sizeof( vk_world ) );

	// Reset geometry buffers offsets
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
	}

	if ( vk.cmd ) {
		Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
		Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	}

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );

#ifdef USE_UPLOAD_QUEUE
	if ( code == REF_KEEP_CONTEXT ) {
		vk_create_sync_primitives();
	}
#endif
}

#if 0
static void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access) {

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = offset;
	barrier.size = size;

	qvkCmdPipelineBarrier( cb, src_stages, dst_stages, 0, 0, NULL, 1, &barrier, 0, NULL );
}
#endif

void vk_create_image( image_t *image, int width, int height, int mip_levels ) {

	VkFormat format = image->internalFormat;

	if ( image->handle ) {
		qvkDestroyImage( vk.device, image->handle, NULL );
		image->handle = VK_NULL_HANDLE;
	}

	if ( image->view ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
		image->view = VK_NULL_HANDLE;
	}

	// create image
	{
		VkImageCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = 1;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		allocate_and_bind_image_memory( image->handle );
	}

	// create image view
	{
		VkImageViewCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image->handle;
		desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		desc.format = format;
		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &image->view ) );
	}

	// create associated descriptor set
	if ( image->descriptor == VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = vk_descriptor_layout_ref( RTX_VK_DESC_LAYOUT_SAMPLER );

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &desc, &image->descriptor ) );
	}

	vk_update_descriptor_set( image, mip_levels > 1 ? qtrue : qfalse );

	SET_OBJECT_NAME( image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( image->view, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( image->descriptor, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
}


static byte *resample_image_data( const int target_format, byte *data, const int data_size, int *bytes_per_pixel )
{
	byte* buffer;
	uint16_t* p;
	int i, n;

	switch ( target_format ) {
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_B8G8R8A8_UNORM:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size );
		for ( i = 0; i < data_size; i += 4 ) {
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case VK_FORMAT_R8G8B8_UNORM: {
		buffer = (byte*)ri.Hunk_AllocateTempMemory( (data_size * 3) / 4 );
		for ( i = 0, n = 0; i < data_size; i += 4, n += 3 ) {
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}


void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update ) {

	VkCommandBuffer command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	const int streamBudgetMb = vk_rt_texture_stream_budget_mb();
	const VkDeviceSize streamBudgetBytes = ( streamBudgetMb > 0 ) ? (VkDeviceSize)streamBudgetMb * 1024u * 1024u : 0u;
	const int frameMarker = tr.frameCount;

	byte *buf;
	int bpp;
#ifdef USE_UPLOAD_QUEUE
	int i;
#endif

	int num_regions = 0;
	int buffer_size = 0;

	buf = resample_image_data( image->internalFormat, pixels, size, &bpp );

	while (qtrue) {
		Com_Memset(&region, 0, sizeof(region));
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * bpp;

		if ( num_regions >= mipmaps || (width == 1 && height == 1) || num_regions >= ARRAY_LEN( regions ) )
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1;
	}

#ifdef USE_UPLOAD_QUEUE
	vk_wait_staging_buffer();
	if ( s_vkRtTextureBudgetFrame != frameMarker ) {
		s_vkRtTextureBudgetFrame = frameMarker;
		s_vkRtTextureBudgetUsed = 0;
	}
	if ( streamBudgetBytes > 0 &&
		( s_vkRtTextureBudgetUsed + (VkDeviceSize)buffer_size ) > streamBudgetBytes &&
		vk_world.staging_buffer_offset > 0 ) {
		vk_submit_staging_buffer( qfalse );
	}

	ensure_staging_buffer_allocation( buffer_size );

	for ( i = 0; i < num_regions; i++ ) {
		regions[i].bufferOffset += vk_world.staging_buffer_offset;
	}

	Com_Memcpy( vk_world.staging_buffer_ptr + vk_world.staging_buffer_offset, buf, buffer_size );

	if ( vk_world.staging_buffer_offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}
	//ri.Printf( PRINT_WARNING, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
	vk_world.staging_buffer_offset += buffer_size;
	s_vkRtTextureBudgetUsed += (VkDeviceSize)buffer_size;

	command_buffer = vk.staging_command_buffer;
#else
	ensure_staging_buffer_allocation( buffer_size );

	Com_Memcpy( vk_world.staging_buffer_ptr, buf, buffer_size );

	command_buffer = begin_command_buffer();
#endif

	// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk_world.staging_buffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

#ifndef USE_UPLOAD_QUEUE
	end_command_buffer( command_buffer, __func__ );
#endif

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}

	vk.rt.stats.texture_stream_bytes += (uint64_t)buffer_size;
}


void vk_update_descriptor_set( image_t *image, qboolean mipmap ) {
	Vk_Sampler_Def sampler_def;
	VkDescriptorImageInfo image_info;
	VkWriteDescriptorSet descriptor_write;

	Com_Memset( &sampler_def, 0, sizeof( sampler_def ) );

	sampler_def.address_mode = image->wrapClampMode;

	if ( mipmap ) {
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	} else {
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = qtrue;
	}

	image_info.sampler = vk_find_sampler( &sampler_def );
	image_info.imageView = image->view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = image->descriptor;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write, 0, NULL );
}


void vk_destroy_image_resources( VkImage *image, VkImageView *imageView )
{
	if ( image != NULL ) {
		if ( *image != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, *image, NULL );
			*image = VK_NULL_HANDLE;
		}
	}
	if ( imageView != NULL ) {
		if ( *imageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, *imageView, NULL );
			*imageView = VK_NULL_HANDLE;
		}
	}
}


static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}


#define FORMAT_DEPTH(format, r_bits, g_bits, b_bits) case(VK_FORMAT_##format): *r = r_bits; *b = b_bits; *g = g_bits; return qtrue;
static qboolean vk_surface_format_color_depth( VkFormat format, int *r, int *g, int *b ) {
	switch (format) {
		// Common formats from https://vulkan.gpuinfo.org/listsurfaceformats.php
		FORMAT_DEPTH(B8G8R8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(B8G8R8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2B10G10R10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R8G8B8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(R8G8B8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2R10G10B10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R5G6B5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(R8G8B8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_UNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SRGB_PACK32, 255, 255, 255)
			FORMAT_DEPTH(R16G16B16A16_UNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(R16G16B16A16_SNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(B5G6R5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(B8G8R8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(R4G4B4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(B4G4R4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(A1R5G5B5_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(R5G5B5A1_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(B5G5R5A1_UNORM_PACK16, 31, 31, 31)
	default:
		*r = 255; *g = 255; *b = 255; return qfalse;
	}
}


void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry spec_entries[45];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;
	VkShaderModule fsmodule;
	VkRenderPass renderpass;
	VkPipelineLayout layout;
	VkSampleCountFlagBits samples;
	const char *pipeline_name;
	qboolean blend;

	struct FragSpecData {
		float gamma;
		float overbright;
		float greyscale;
		float bloom_threshold;
		float bloom_intensity;
		int bloom_threshold_mode;
		int bloom_modulate;
		int dither;
		int depth_r;
		int depth_g;
		int depth_b;
		int output_color_space;
		float hdr_paper_white;
		float hdr_max_luminance;
		int tonemap_mode;
		float tonemap_exposure;
		float bloom_soft_knee;
		int scene_linear_mode;
		int color_grade_mode;
		float grade_lift[3];
		float grade_gamma[3];
		float grade_gain[3];
		float white_point[9];
		int color_grade_lut_size;
		float color_grade_lut_scale;
		int crt_mode;
		float crt_amount;
		float crt_scanline_strength;
		float crt_mask_strength;
		float crt_curvature;
		float crt_chromatic;
	} frag_spec_data;

	switch ( program_index ) {
		case 1: // bloom extraction
			pipeline = &vk.bloom_extract_pipeline;
			fsmodule = vk.modules.bloom_fs;
			renderpass = vk.render_pass.bloom_extract;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "bloom extraction pipeline";
			blend = qfalse;
			break;
		case 2: // final bloom blend
			pipeline = &vk.bloom_blend_pipeline;
			fsmodule = vk.modules.blend_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_BLEND );
			samples = vkSamples;
			pipeline_name = "bloom blend pipeline";
			blend = qtrue;
			break;
		case 6: // world cel depth edge overlay in the resumed RTX composition pass
			pipeline = &vk.world_outline_pipeline;
			fsmodule = vk.modules.world_outline_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = vkSamples;
			pipeline_name = "world cel depth-outline pipeline";
			blend = qtrue;
			break;
		case 7: // final bloom blend that does not brighten dark cel outlines
			pipeline = &vk.bloom_blend_cel_pipeline;
			fsmodule = vk.modules.blend_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_BLEND );
			samples = vkSamples;
			pipeline_name = "bloom blend cel-outline pipeline";
			blend = qtrue;
			break;
		case 12: // menu soft focus: full frame down to the half-resolution step
			pipeline = &vk.menu_blur_half_pipeline;
			fsmodule = vk.modules.menu_blur_fs;
			renderpass = vk.render_pass.menu_blur;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "menu soft focus halve pipeline";
			blend = qfalse;
			break;
		case 13: // menu soft focus: the downsample into, and every iteration at,
		         // the pyramid level, which all share one target size
			pipeline = &vk.menu_blur_level_pipeline;
			fsmodule = vk.modules.menu_blur_fs;
			renderpass = vk.render_pass.menu_blur;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "menu soft focus level pipeline";
			blend = qfalse;
			break;
		case 14: // menu soft focus: composite the softened copy over the frame
			pipeline = &vk.menu_blur_composite_pipeline;
			fsmodule = vk.modules.menu_blur_fs;
			renderpass = vk.render_pass.main_load;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = vkSamples;
			pipeline_name = "menu soft focus composite pipeline";
			blend = qtrue;
			break;
		case 8: // camera-motion blur into the scratch attachment
			pipeline = &vk.motion_blur_pipeline;
			fsmodule = vk.modules.motion_blur_fs;
			renderpass = vk.render_pass.motion_blur;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "motion blur scene pipeline";
			blend = qfalse;
			break;
		case 9: // copy blurred scene back to the current composition target
			pipeline = &vk.motion_blur_copy_pipeline;
			fsmodule = vk.modules.motion_blur_fs;
			renderpass = vk.render_pass.main_load;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = vkSamples;
			pipeline_name = "motion blur scene copy pipeline";
			blend = qfalse;
			break;
		case 10: // submerged-view composite into the scratch attachment
			pipeline = &vk.underwater_pipeline;
			fsmodule = vk.modules.underwater_fs;
			renderpass = vk.render_pass.underwater;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "underwater view scene pipeline";
			blend = qfalse;
			break;
		case 11: // copy the composited scratch image back to the scene target
			pipeline = &vk.underwater_copy_pipeline;
			fsmodule = vk.modules.underwater_fs;
			renderpass = vk.render_pass.main_load;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = vkSamples;
			pipeline_name = "underwater view scene copy pipeline";
			blend = qfalse;
			break;
		case 3: // capture buffer extraction
			pipeline = &vk.capture_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.capture;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "capture buffer pipeline";
			blend = qfalse;
			break;
		case 4: // depth-aware global fog overlay
			pipeline = &vk.global_fog_pipeline;
			fsmodule = vk.modules.global_fog_fs;
			renderpass = vk.render_pass.global_fog;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "global fog overlay pipeline";
			blend = qtrue;
			break;
		case 5: // stable pre-fog liquid scene snapshot
			pipeline = &vk.liquid_snapshot_pipeline;
			fsmodule = vk.modules.liquid_copy_fs;
			renderpass = vk.render_pass.liquid_snapshot;
			layout = vk_pipeline_layout_handle(
				RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "liquid scene snapshot pipeline";
			blend = qfalse;
			break;
		default: // gamma correction
			pipeline = &vk.gamma_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.gamma;
			layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "gamma-correction pipeline";
			blend = qfalse;
			break;
	}

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, fsmodule, "main" );

	frag_spec_data.gamma = 1.0 / (r_gamma->value);
	frag_spec_data.overbright = (float)(1 << tr.overbrightBits);
	frag_spec_data.greyscale = r_greyscale->value;
	frag_spec_data.bloom_threshold = r_bloom_threshold->value;
	frag_spec_data.bloom_intensity = r_bloom_intensity->value;
	frag_spec_data.bloom_threshold_mode = r_bloom_threshold_mode->integer;
	frag_spec_data.bloom_modulate = r_bloom_modulate->integer;
	frag_spec_data.dither = r_dither->integer;
	frag_spec_data.output_color_space =
		( program_index == 0 && vk.hdrDisplayActive ) ?
			VK_POST_COLOR_SPACE_HDR10_ST2084 : VK_POST_COLOR_SPACE_SDR;
	frag_spec_data.hdr_paper_white = Com_Clamp( 80.0f, 500.0f,
		r_hdrDisplayPaperWhite ? r_hdrDisplayPaperWhite->value : 203.0f );
	frag_spec_data.hdr_max_luminance = Com_Clamp(
		frag_spec_data.hdr_paper_white, 10000.0f,
		r_hdrDisplayMaxLuminance ? r_hdrDisplayMaxLuminance->value : 1000.0f );
	if ( vk.hdrDisplayActive &&
		vk.displayOutput.maxLuminanceNits >= frag_spec_data.hdr_paper_white ) {
		frag_spec_data.hdr_max_luminance = Com_Clamp(
			frag_spec_data.hdr_paper_white,
			frag_spec_data.hdr_max_luminance,
			vk.displayOutput.maxLuminanceNits );
	}
	frag_spec_data.scene_linear_mode = vk_scene_linear_enabled() ? 1 : 0;
	frag_spec_data.tonemap_mode = ( frag_spec_data.scene_linear_mode &&
		( program_index == 0 || program_index == 1 || program_index == 3 ) ) ?
		r_tonemap->integer : 0;
	frag_spec_data.tonemap_exposure =
		Com_Clamp( 0.1f, 8.0f, r_tonemapExposure->value );
	frag_spec_data.bloom_soft_knee =
		Com_Clamp( 0.0f, 1.0f, r_bloom_soft_knee->value );
	{
		const int color_grade_mode = vk_color_grade_mode();
		const qboolean use_lgg =
			vk_color_grade_uses_lgg( color_grade_mode );
		const qboolean use_lut =
			vk_color_grade_uses_lut( color_grade_mode );
		const float sourceWhitePoint = r_colorGradeWhitePoint ?
			Com_Clamp( 1000.0f, 40000.0f,
				r_colorGradeWhitePoint->value ) : 6504.0f;
		const float targetWhitePoint = r_colorGradeAdaptWhitePoint ?
			Com_Clamp( 1000.0f, 40000.0f,
				r_colorGradeAdaptWhitePoint->value ) : 6504.0f;
		int lutSize = 0;

		frag_spec_data.color_grade_mode = color_grade_mode;
		frag_spec_data.color_grade_lut_scale = r_colorGradeLUTScale ?
			Com_Clamp( 1.0f, 32.0f, r_colorGradeLUTScale->value ) : 4.0f;

		if ( use_lgg ) {
			vk_parse_vec3_cvar( r_colorGradeLift,
				0.0f, 0.0f, 0.0f, -1.0f, 1.0f,
				frag_spec_data.grade_lift );
			vk_parse_vec3_cvar( r_colorGradeGamma,
				1.0f, 1.0f, 1.0f, 0.1f, 8.0f,
				frag_spec_data.grade_gamma );
			vk_parse_vec3_cvar( r_colorGradeGain,
				1.0f, 1.0f, 1.0f, 0.0f, 8.0f,
				frag_spec_data.grade_gain );
			vk_build_bradford_adaptation( sourceWhitePoint,
				targetWhitePoint, frag_spec_data.white_point );
		} else {
			frag_spec_data.grade_lift[0] = 0.0f;
			frag_spec_data.grade_lift[1] = 0.0f;
			frag_spec_data.grade_lift[2] = 0.0f;
			frag_spec_data.grade_gamma[0] = 1.0f;
			frag_spec_data.grade_gamma[1] = 1.0f;
			frag_spec_data.grade_gamma[2] = 1.0f;
			frag_spec_data.grade_gain[0] = 1.0f;
			frag_spec_data.grade_gain[1] = 1.0f;
			frag_spec_data.grade_gain[2] = 1.0f;
			vk_set_identity_3x3( frag_spec_data.white_point );
		}

		if ( use_lut ) {
			(void)vk_color_grade_lut_image( &lutSize );
		}
		frag_spec_data.color_grade_lut_size = use_lut ? lutSize : 0;
	}
	frag_spec_data.crt_amount = r_crtAmount ?
		Com_Clamp( 0.0f, 1.0f, r_crtAmount->value ) : 1.0f;
	frag_spec_data.crt_scanline_strength = r_crtScanlineStrength ?
		Com_Clamp( 0.0f, 1.0f, r_crtScanlineStrength->value ) : 0.55f;
	frag_spec_data.crt_mask_strength = r_crtMaskStrength ?
		Com_Clamp( 0.0f, 1.0f, r_crtMaskStrength->value ) : 0.35f;
	frag_spec_data.crt_curvature = r_crtCurvature ?
		Com_Clamp( 0.0f, 0.25f, r_crtCurvature->value ) : 0.01f;
	frag_spec_data.crt_chromatic = r_crtChromatic ?
		Com_Clamp( 0.0f, 8.0f, r_crtChromatic->value ) : 1.35f;
	frag_spec_data.crt_mode =
		( r_crt && r_crt->integer && frag_spec_data.crt_amount > 0.001f &&
		  ( program_index == 0 || program_index == 3 ) ) ? 1 : 0;

	if ( !vk_surface_format_color_depth( vk.present_format.format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b ) )
		ri.Printf( PRINT_ALL, "Format %s not recognized, dither to assume 8bpc\n", vk_format_string( vk.base_format.format ) );

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( struct FragSpecData, gamma );
	spec_entries[0].size = sizeof( frag_spec_data.gamma );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( struct FragSpecData, overbright );
	spec_entries[1].size = sizeof( frag_spec_data.overbright );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( struct FragSpecData, greyscale );
	spec_entries[2].size = sizeof( frag_spec_data.greyscale );

	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( struct FragSpecData, bloom_threshold );
	spec_entries[3].size = sizeof( frag_spec_data.bloom_threshold );

	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( struct FragSpecData, bloom_intensity );
	spec_entries[4].size = sizeof( frag_spec_data.bloom_intensity );

	spec_entries[5].constantID = 5;
	spec_entries[5].offset = offsetof( struct FragSpecData, bloom_threshold_mode );
	spec_entries[5].size = sizeof( frag_spec_data.bloom_threshold_mode );

	spec_entries[6].constantID = 6;
	spec_entries[6].offset = offsetof( struct FragSpecData, bloom_modulate );
	spec_entries[6].size = sizeof( frag_spec_data.bloom_modulate );

	spec_entries[7].constantID = 7;
	spec_entries[7].offset = offsetof( struct FragSpecData, dither );
	spec_entries[7].size = sizeof( frag_spec_data.dither );

	spec_entries[8].constantID = 8;
	spec_entries[8].offset = offsetof( struct FragSpecData, depth_r );
	spec_entries[8].size = sizeof( frag_spec_data.depth_r );

	spec_entries[9].constantID = 9;
	spec_entries[9].offset = offsetof(struct FragSpecData, depth_g);
	spec_entries[9].size = sizeof(frag_spec_data.depth_g);

	spec_entries[10].constantID = 10;
	spec_entries[10].offset = offsetof(struct FragSpecData, depth_b);
	spec_entries[10].size = sizeof(frag_spec_data.depth_b);

	#define VK_FRAG_SPEC_FIELD( index_, field_ ) do { \
		spec_entries[index_].constantID = (index_); \
		spec_entries[index_].offset = offsetof( struct FragSpecData, field_ ); \
		spec_entries[index_].size = sizeof( frag_spec_data.field_ ); \
	} while ( 0 )
	#define VK_FRAG_SPEC_ARRAY( index_, field_, element_ ) do { \
		spec_entries[index_].constantID = (index_); \
		spec_entries[index_].offset = offsetof( struct FragSpecData, field_ ) + \
			sizeof( frag_spec_data.field_[0] ) * (element_); \
		spec_entries[index_].size = sizeof( frag_spec_data.field_[0] ); \
	} while ( 0 )

	VK_FRAG_SPEC_FIELD( 11, output_color_space );
	VK_FRAG_SPEC_FIELD( 12, hdr_paper_white );
	VK_FRAG_SPEC_FIELD( 13, hdr_max_luminance );
	VK_FRAG_SPEC_FIELD( 14, tonemap_mode );
	VK_FRAG_SPEC_FIELD( 15, tonemap_exposure );
	VK_FRAG_SPEC_FIELD( 16, bloom_soft_knee );
	VK_FRAG_SPEC_FIELD( 17, scene_linear_mode );
	VK_FRAG_SPEC_FIELD( 18, color_grade_mode );
	VK_FRAG_SPEC_ARRAY( 19, grade_lift, 0 );
	VK_FRAG_SPEC_ARRAY( 20, grade_lift, 1 );
	VK_FRAG_SPEC_ARRAY( 21, grade_lift, 2 );
	VK_FRAG_SPEC_ARRAY( 22, grade_gamma, 0 );
	VK_FRAG_SPEC_ARRAY( 23, grade_gamma, 1 );
	VK_FRAG_SPEC_ARRAY( 24, grade_gamma, 2 );
	VK_FRAG_SPEC_ARRAY( 25, grade_gain, 0 );
	VK_FRAG_SPEC_ARRAY( 26, grade_gain, 1 );
	VK_FRAG_SPEC_ARRAY( 27, grade_gain, 2 );
	VK_FRAG_SPEC_ARRAY( 28, white_point, 0 );
	VK_FRAG_SPEC_ARRAY( 29, white_point, 1 );
	VK_FRAG_SPEC_ARRAY( 30, white_point, 2 );
	VK_FRAG_SPEC_ARRAY( 31, white_point, 3 );
	VK_FRAG_SPEC_ARRAY( 32, white_point, 4 );
	VK_FRAG_SPEC_ARRAY( 33, white_point, 5 );
	VK_FRAG_SPEC_ARRAY( 34, white_point, 6 );
	VK_FRAG_SPEC_ARRAY( 35, white_point, 7 );
	VK_FRAG_SPEC_ARRAY( 36, white_point, 8 );
	VK_FRAG_SPEC_FIELD( 37, color_grade_lut_size );
	VK_FRAG_SPEC_FIELD( 38, color_grade_lut_scale );
	VK_FRAG_SPEC_FIELD( 39, crt_mode );
	VK_FRAG_SPEC_FIELD( 40, crt_amount );
	VK_FRAG_SPEC_FIELD( 41, crt_scanline_strength );
	VK_FRAG_SPEC_FIELD( 42, crt_mask_strength );
	VK_FRAG_SPEC_FIELD( 43, crt_curvature );
	VK_FRAG_SPEC_FIELD( 44, crt_chromatic );

	#undef VK_FRAG_SPEC_ARRAY
	#undef VK_FRAG_SPEC_FIELD

	frag_spec_info.mapEntryCount = ARRAY_LEN( spec_entries );
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;

	shader_stages[1].pSpecializationInfo =
		( program_index == 4 || program_index == 5 || program_index == 6 ||
		  program_index == 8 || program_index == 9 ) ?
			NULL : &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	if ( program_index == 0 ) {
		// gamma correction
		viewport.x = 0.0 + vk.blitX0;
		viewport.y = 0.0 + vk.blitY0;
		viewport.width = gls.windowWidth - vk.blitX0 * 2;
		viewport.height = gls.windowHeight - vk.blitY0 * 2;
	} else {
		// other post-processing
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.width = width;
		viewport.height = height;
	}

	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = samples;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( blend ) {
		attachment_blend_state.blendEnable = VK_TRUE;
		if ( program_index == 4 || program_index == 6 || program_index == 14 ) {
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
			attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
		} else if ( program_index == 7 ) {
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
			attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
		} else {
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		}
	} else {
		attachment_blend_state.blendEnable = VK_FALSE;
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = VK_FALSE;
	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = (program_index == 2) ? &depth_stencil_state : NULL;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = layout;
	create_info.renderPass = renderpass;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}


void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	float frag_spec_data[3]; // x-offset, y-offset, correction
	VkSpecializationMapEntry spec_entries[3];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;

	pipeline = &vk.blur_pipeline[ index ];

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_fs, "main" );

	frag_spec_data[0] = 1.2 / (float) width; // x offset
	frag_spec_data[1] = 1.2 / (float) height; // y offset
	frag_spec_data[2] = 1.0; // intensity?

	if ( horizontal_pass ) {
		frag_spec_data[1] = 0.0;
	} else {
		frag_spec_data[0] = 0.0;
	}

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = 0 * sizeof( float );
	spec_entries[0].size = sizeof( float );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = 1 * sizeof( float );
	spec_entries[1].size = sizeof( float );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = 2 * sizeof( float );
	spec_entries[2].size = sizeof( float );

	frag_spec_info.mapEntryCount = 3;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = 3 * sizeof( float );
	frag_spec_info.pData = &frag_spec_data[0];

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	viewport.x = 0.0;
	viewport.y = 0.0;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = VK_FALSE;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = NULL;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ); // one input attachment
	create_info.renderPass = vk.render_pass.blur[ index ];
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, va( "%s blur pipeline %i", horizontal_pass ? "horizontal" : "vertical", index/2 + 1 ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}


static VkVertexInputBindingDescription bindings[8];
static VkVertexInputAttributeDescription attribs[8];
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind( uint32_t binding, uint32_t stride )
{
	bindings[ num_binds ].binding = binding;
	bindings[ num_binds ].stride = stride;
	bindings[ num_binds ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	num_binds++;
}

static void push_attr( uint32_t location, uint32_t binding, VkFormat format )
{
	attribs[ num_attrs ].location = location;
	attribs[ num_attrs ].binding = binding;
	attribs[ num_attrs ].format = format;
	attribs[ num_attrs ].offset = 0;
	num_attrs++;
}


VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {
	VkShaderModule *vs_module = NULL;
	VkShaderModule *fs_module = NULL;
	//int32_t vert_spec_data[1]; // clippping
	floatint_t frag_spec_data[12]; // 0:alpha-test-func, 1:alpha-test-value, 2:depth-fragment, 3:alpha-to-coverage, 4:color_mode, 5:abs_light, 6:multitexture mode, 7:discard mode, 8: ident.color, 9 - ident.alpha, 10 - acff, 11 - depth fade
	VkSpecializationMapEntry spec_entries[13];
	//VkSpecializationInfo vert_spec_info;
	VkSpecializationInfo frag_spec_info;
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_DEPTH_BIAS
	};
	VkGraphicsPipelineCreateInfo create_info;
	VkPipeline pipeline;
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkBool32 alphaToCoverage = VK_FALSE;
	VkPipelineRasterizationLineStateCreateInfoEXT alice_line_state;
	VkDynamicState alice_line_dynamic_states[4];
	unsigned int atest_bits;
	unsigned int state_bits = def->state_bits;

	switch ( def->shader_type ) {

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[1][0];
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.gen0_df;
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			vs_module = &vk.modules.vert.gen[0][0][0][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			vs_module = &vk.modules.vert.gen[0][0][1][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY:
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[0][1][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = &vk.modules.vert.ident1[1][0][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[1][1][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[1][0][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[1][1][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = &vk.modules.vert.gen[1][0][0][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = &vk.modules.vert.gen[1][0][1][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = &vk.modules.vert.gen[2][0][0][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = &vk.modules.vert.gen[2][0][1][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[1][1][0][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[1][1][1][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[2][1][0][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[2][1][1][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = &vk.modules.color_vs;
			fs_module = &vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = &vk.modules.fog_vs;
			fs_module = &vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = &vk.modules.dot_vs;
			fs_module = &vk.modules.dot_fs;
			break;

		case TYPE_LIQUID:
			vs_module = &vk.modules.liquid_vs;
			fs_module = &vk.modules.liquid_fs;
			break;

		case TYPE_CSM_SHADOW:
			vs_module = &vk.modules.vert.csm_shadow;
			fs_module = &vk.modules.frag.csm_shadow;
			break;

		default:
			ri.Error(ERR_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}

	if ( def->fog_stage ) {
		switch ( def->shader_type ) {
			case TYPE_FOG_ONLY:
			case TYPE_DOT:
			case TYPE_LIQUID:
			case TYPE_SIGNLE_TEXTURE_DF:
			case TYPE_COLOR_BLACK:
			case TYPE_COLOR_WHITE:
			case TYPE_COLOR_GREEN:
			case TYPE_COLOR_RED:
				break;
			default:
				// switch to fogged modules
				vs_module++;
				fs_module++;
				break;
		}
	}

	set_shader_stage_desc(shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, *vs_module, "main");
	set_shader_stage_desc(shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, *fs_module, "main");

	//Com_Memset( vert_spec_data, 0, sizeof( vert_spec_data ) );
	Com_Memset( frag_spec_data, 0, sizeof( frag_spec_data ) );

	//vert_spec_data[0] = def->clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch ( atest_bits ) {
		case GLS_ATEST_GT_0:
			frag_spec_data[0].i = 1; // not equal
			frag_spec_data[1].f = 0.0f;
			break;
		case GLS_ATEST_LT_80:
			frag_spec_data[0].i = 2; // less than
			frag_spec_data[1].f = 0.5f;
			break;
		case GLS_ATEST_GE_80:
			frag_spec_data[0].i = 3; // greater or equal
			frag_spec_data[1].f = 0.5f;
			break;
		default:
			frag_spec_data[0].i = 0;
			frag_spec_data[1].f = 0.0f;
			break;
	};

	// American McGee's Alice's `alphatest <func> <ref>`: the reference is
	// ref / 256 (alice.exe 0x00485dd7), and each comparison it passes is the
	// discard of its opposite -- equal, notequal, greater, less, gequal and
	// lequal as gen_frag.tmpl's functions 4, 1, 5, 2, 3 and 6. Only Alice
	// shaders set these bits, and never with GLS_ATEST_BITS.
	if ( state_bits & GLS_ALICE_ATEST_FUNC_BITS ) {
		static const int aliceDiscard[7] = { 0, 4, 1, 5, 2, 3, 6 };
		const unsigned func = ( state_bits & GLS_ALICE_ATEST_FUNC_BITS ) >> GLS_ALICE_ATEST_FUNC_SHIFT;
		const unsigned ref = ( state_bits & GLS_ALICE_ATEST_REF_BITS ) >> GLS_ALICE_ATEST_REF_SHIFT;
		frag_spec_data[0].i = func < ARRAY_LEN( aliceDiscard ) ? aliceDiscard[func] : 0;
		frag_spec_data[1].f = (float)ref * ( 1.0f / 256.0f );
	}

	// depth fragment threshold
	frag_spec_data[2].f = 0.85f;

#if 0
	if ( r_ext_alpha_to_coverage && r_ext_alpha_to_coverage->integer &&
		vkSamples != VK_SAMPLE_COUNT_1_BIT && frag_spec_data[0].i ) {
		frag_spec_data[3].i = 1;
		alphaToCoverage = VK_TRUE;
	}
#endif

	// constant color
	switch ( def->shader_type ) {
		default: frag_spec_data[4].i = 0; break;
		case TYPE_COLOR_WHITE: frag_spec_data[4].i = 1; break;
		case TYPE_COLOR_GREEN: frag_spec_data[4].i = 2; break;
		case TYPE_COLOR_RED:   frag_spec_data[4].i = 3; break;
	}

	// abs lighting
	switch ( def->shader_type ) {
		case TYPE_SIGNLE_TEXTURE_LIGHTING:
		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			frag_spec_data[5].i = def->abs_light ? 1 : 0;
		default:
			break;
	}

	// multutexture mode
	switch ( def->shader_type ) {
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_MUL_ENV:
			frag_spec_data[6].i = 0;
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
			frag_spec_data[6].i = 1;
			break;

		case TYPE_MULTI_TEXTURE_ADD2:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
		case TYPE_MULTI_TEXTURE_ADD3:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_ADD_ENV:
			frag_spec_data[6].i = 2;
			break;

		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ALPHA_ENV:
			frag_spec_data[6].i = 3;
			break;

		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 4;
			break;

		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
			frag_spec_data[6].i = 5;
			break;

		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 6;
			break;

		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			frag_spec_data[6].i = 7;
			break;

		default:
			break;
	}

	frag_spec_data[8].f = ((float)def->color.rgb) / 255.0;
	frag_spec_data[9].f = ((float)def->color.alpha) / 255.0;

	if ( def->fog_stage ) {
		frag_spec_data[10].i = def->acff;
	} else {
		frag_spec_data[10].i = 0;
	}
	frag_spec_data[11].i = def->depth_fade;

	//
	// vertex module specialization data
	//
#if 0
	spec_entries[0].constantID = 0; // clip_plane
	spec_entries[0].offset = 0 * sizeof( int32_t );
	spec_entries[0].size = sizeof( int32_t );

	vert_spec_info.mapEntryCount = 1;
	vert_spec_info.pMapEntries = spec_entries + 0;
	vert_spec_info.dataSize = 1 * sizeof( int32_t );
	vert_spec_info.pData = &vert_spec_data[0];
	shader_stages[0].pSpecializationInfo = &vert_spec_info;
#endif
	shader_stages[0].pSpecializationInfo = NULL;

	//
	// fragment module specialization data
	//

	spec_entries[1].constantID = 0;  // alpha-test-function
	spec_entries[1].offset = 0 * sizeof( int32_t );
	spec_entries[1].size = sizeof( int32_t );

	spec_entries[2].constantID = 1; // alpha-test-value
	spec_entries[2].offset = 1 * sizeof( int32_t );
	spec_entries[2].size = sizeof( float );

	spec_entries[3].constantID = 2; // depth-fragment
	spec_entries[3].offset = 2 * sizeof( int32_t );
	spec_entries[3].size = sizeof( float );

	spec_entries[4].constantID = 3; // alpha-to-coverage
	spec_entries[4].offset = 3 * sizeof( int32_t );
	spec_entries[4].size = sizeof( int32_t );

	spec_entries[5].constantID = 4; // color_mode
	spec_entries[5].offset = 4 * sizeof( int32_t );
	spec_entries[5].size = sizeof( int32_t );

	spec_entries[6].constantID = 5; // abs_light
	spec_entries[6].offset = 5 * sizeof( int32_t );
	spec_entries[6].size = sizeof( int32_t );

	spec_entries[7].constantID = 6; // multitexture mode
	spec_entries[7].offset = 6 * sizeof( int32_t );
	spec_entries[7].size = sizeof( int32_t );

	spec_entries[8].constantID = 7; // discard mode
	spec_entries[8].offset = 7 * sizeof( int32_t );
	spec_entries[8].size = sizeof( int32_t );

	spec_entries[9].constantID = 8; // fixed color
	spec_entries[9].offset = 8 * sizeof( int32_t );
	spec_entries[9].size = sizeof( float );

	spec_entries[10].constantID = 9; // fixed alpha
	spec_entries[10].offset = 9 * sizeof( int32_t );
	spec_entries[10].size = sizeof( float );

	spec_entries[11].constantID = 10; // acff
	spec_entries[11].offset = 10 * sizeof( int32_t );
	spec_entries[11].size = sizeof( int32_t );

	spec_entries[12].constantID = 11; // depth fade
	spec_entries[12].offset = 11 * sizeof( int32_t );
	spec_entries[12].size = sizeof( int32_t );

	frag_spec_info.mapEntryCount = 12;
	frag_spec_info.pMapEntries = spec_entries + 1;
	frag_spec_info.dataSize = sizeof( int32_t ) * 12;
	frag_spec_info.pData = &frag_spec_data[0];
	shader_stages[1].pSpecializationInfo =
		def->shader_type == TYPE_LIQUID ? NULL : &frag_spec_info;

	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch ( def->shader_type ) {

		case TYPE_LIQUID:
			push_bind( 0, sizeof( vec4_t ) );
			push_bind( 5, sizeof( vec4_t ) );
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_FOG_ONLY:
		case TYPE_DOT:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_CSM_SHADOW:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// baked vertex colours
			push_bind( 3, sizeof( vec2_t ) );					// lightmap st
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
		case TYPE_SIGNLE_TEXTURE_IDENTITY:
		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( vec2_t ) );					// st0 array
			push_bind( 2, sizeof( vec4_t ) );					// normals array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		default:
			ri.Error( ERR_DROP, "%s: invalid shader type - %i", __func__, def->shader_type );
			break;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.pVertexAttributeDescriptions = attribs;
	vertex_input_state.vertexBindingDescriptionCount = num_binds;
	vertex_input_state.vertexAttributeDescriptionCount = num_attrs;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	switch ( def->primitives ) {
		case LINE_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case POINT_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case TRIANGLE_STRIP: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		default: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	}

	//
	// Viewport.
	//
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL; // dynamic scissor state

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	if ( def->shader_type == TYPE_DOT ) {
		rasterization_state.polygonMode = VK_POLYGON_MODE_POINT;
	} else {
		rasterization_state.polygonMode = (state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	}

	switch ( def->face_culling ) {
		case CT_TWO_SIDED:
			rasterization_state.cullMode = VK_CULL_MODE_NONE;
			break;
		case CT_FRONT_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
			break;
		case CT_BACK_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
			break;
		default:
			ri.Error( ERR_DROP, "create_pipeline: invalid face culling mode %i\n", def->face_culling );
			break;
	}

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	 // depth bias state
	if ( renderPassIndex == RENDER_PASS_DLIGHT_SHADOW ||
		renderPassIndex == RENDER_PASS_SPOT_SHADOW ||
		renderPassIndex == RENDER_PASS_CSM_SHADOW ) {
		rasterization_state.depthBiasEnable = VK_TRUE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	} else if ( def->polygon_offset ) {
		rasterization_state.depthBiasEnable = VK_TRUE;
		rasterization_state.depthBiasClamp = 0.0f;
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
	} else {
		rasterization_state.depthBiasEnable = VK_FALSE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	if ( def->line_width )
		rasterization_state.lineWidth = (float)def->line_width;
	else
		rasterization_state.lineWidth = 1.0f;

	// Alice's stippled debug lines (vk_alice_line_rasterization)
	if ( def->alice_debug_line == 2 && vk.lineStipple ) {
		Com_Memset( &alice_line_state, 0, sizeof( alice_line_state ) );
		alice_line_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT;
		alice_line_state.lineRasterizationMode = vk.lineStippleMode;
		alice_line_state.stippledLineEnable = VK_TRUE;
		alice_line_state.lineStippleFactor = 1;
		alice_line_state.lineStipplePattern = 0xffff;
		rasterization_state.pNext = &alice_line_state;
	}

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;

	if ( renderPassIndex == RENDER_PASS_DLIGHT_SHADOW ||
		renderPassIndex == RENDER_PASS_SPOT_SHADOW ||
		renderPassIndex == RENDER_PASS_CSM_SHADOW ) {
		multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	} else {
		multisample_state.rasterizationSamples =
			( renderPassIndex == RENDER_PASS_SCREENMAP ) ?
				vk.screenMapSamples : vkSamples;
	}

	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
	if ( renderPassIndex == RENDER_PASS_CSM_SHADOW ) {
		depth_stencil_state.depthCompareOp =
			( state_bits & GLS_DEPTHFUNC_EQUAL ) ?
				VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	} else {
#ifdef USE_REVERSED_DEPTH
		depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
	}
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def->shadow_phase != SHADOW_DISABLED) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = (def->face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_FS_QUAD) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	} else if (def->shadow_phase == SHADOW_OUTLINE_MASK) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_REPLACE;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 1;

		depth_stencil_state.back = depth_stencil_state.front;
	} else if (def->shadow_phase == SHADOW_OUTLINE_SHELL) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 1;

		depth_stencil_state.back = depth_stencil_state.front;
	} else if (def->shadow_phase == SHADOW_OUTLINE_CLEAR) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_ZERO;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if ( def->depth_only || def->shadow_phase == SHADOW_EDGES || def->shadow_phase == SHADOW_OUTLINE_MASK ||
		def->shadow_phase == SHADOW_OUTLINE_CLEAR || def->shader_type == TYPE_SIGNLE_TEXTURE_DF )
		attachment_blend_state.colorWriteMask = 0;
	else if ( def->shader_type == TYPE_LIQUID )
		attachment_blend_state.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable) {
		switch (state_bits & GLS_SRCBLEND_BITS) {
			case GLS_SRCBLEND_ZERO:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid src blend state bits\n" );
				break;
		}
		switch (state_bits & GLS_DSTBLEND_BITS) {
			case GLS_DSTBLEND_ZERO:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid dst blend state bits\n" );
				break;
		}

		attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
		attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;

		if ( def->allow_discard && vkSamples != VK_SAMPLE_COUNT_1_BIT ) {
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ) {
				frag_spec_data[7].i = 1;
			} else if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE ) {
				frag_spec_data[7].i = 2;
			}
		}
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	if ( renderPassIndex == RENDER_PASS_DLIGHT_SHADOW ||
		renderPassIndex == RENDER_PASS_SPOT_SHADOW ||
		renderPassIndex == RENDER_PASS_CSM_SHADOW ) {
		blend_state.attachmentCount = 0;
		blend_state.pAttachments = NULL;
	} else {
		blend_state.attachmentCount = 1;
		blend_state.pAttachments = &attachment_blend_state;
	}
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount =
		( renderPassIndex == RENDER_PASS_DLIGHT_SHADOW ||
		  renderPassIndex == RENDER_PASS_SPOT_SHADOW ||
		  renderPassIndex == RENDER_PASS_CSM_SHADOW ) ?
			ARRAY_LEN( dynamic_state_array ) : 2;
	dynamic_state.pDynamicStates = dynamic_state_array;
	// Alice's debug lines set each run's width and stipple while drawing
	if ( def->alice_debug_line ) {
		uint32_t count = 0;
		alice_line_dynamic_states[count++] = VK_DYNAMIC_STATE_VIEWPORT;
		alice_line_dynamic_states[count++] = VK_DYNAMIC_STATE_SCISSOR;
		if ( vk.wideLines ) {
			alice_line_dynamic_states[count++] = VK_DYNAMIC_STATE_LINE_WIDTH;
		}
		if ( def->alice_debug_line == 2 && vk.lineStipple ) {
			alice_line_dynamic_states[count++] = VK_DYNAMIC_STATE_LINE_STIPPLE_EXT;
		}
		dynamic_state.dynamicStateCount = count;
		dynamic_state.pDynamicStates = alice_line_dynamic_states;
	}

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = ARRAY_LEN(shader_stages);
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;

	if ( def->shader_type == TYPE_DOT )
		create_info.layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_STORAGE );
	else
		create_info.layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_MAIN );

	if ( renderPassIndex == RENDER_PASS_DLIGHT_SHADOW ||
		renderPassIndex == RENDER_PASS_SPOT_SHADOW ||
		renderPassIndex == RENDER_PASS_CSM_SHADOW )
		create_info.renderPass = vk.render_pass.dlight_shadow;
	else if ( renderPassIndex == RENDER_PASS_SCREENMAP )
		create_info.renderPass = vk.render_pass.screenmap;
	else
		create_info.renderPass = vk.render_pass.main;

	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &pipeline ) );

	SET_OBJECT_NAME( pipeline, va( "pipeline def#%i, pass#%i", def_index, renderPassIndex ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	vk.pipeline_create_count++;

	return pipeline;
}


static uint32_t vk_alloc_pipeline( const Vk_Pipeline_Def *def ) {
	VK_Pipeline_t *pipeline;
	if ( vk.pipelines_count >= MAX_VK_PIPELINES ) {
		ri.Error( ERR_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached" );
		return 0;
	} else {
		int j;
		pipeline = &vk.pipelines[ vk.pipelines_count ];
		pipeline->def = *def;
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			pipeline->handle[j] = VK_NULL_HANDLE;
		}
		return vk.pipelines_count++;
	}
}


VkPipeline vk_gen_pipeline( uint32_t index ) {
	if ( index < vk.pipelines_count ) {
		VK_Pipeline_t *pipeline = vk.pipelines + index;
		const renderPass_t pass = vk.renderPassIndex;
		if ( pipeline->handle[ pass ] == VK_NULL_HANDLE ) {
			pipeline->handle[ pass ] = create_pipeline( &pipeline->def, pass, index );
		}
		return pipeline->handle[ pass ];
	} else {
		ri.Error( ERR_FATAL, "%s(%i): NULL pipeline", __func__, index );
		return VK_NULL_HANDLE;
	}
}


uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use ) {
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	for ( index = base; index < vk.pipelines_count; index++ ) {
		cur_def = &vk.pipelines[ index ].def;
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			goto found;
		}
	}

	index = vk_alloc_pipeline( def );
found:

	if ( use )
		vk_gen_pipeline( index );

	return index;
}


/*
=================
vk_alice_debug_line_pipeline

The pipeline for a run of Alice debug lines (tr_backend.c RB_AliceDebugLines):
world-space lines over white, depth tested, writing depth when asked. Their
width, and with stipple their stipple, are dynamic state set per run.
=================
*/
uint32_t vk_alice_debug_line_pipeline( qboolean depthMask, qboolean stipple )
{
	Vk_Pipeline_Def def;

	Com_Memset( &def, 0, sizeof( def ) );
	def.shader_type = TYPE_SIGNLE_TEXTURE;
	def.state_bits = depthMask ? GLS_DEPTHMASK_TRUE : 0;
	def.face_culling = CT_TWO_SIDED;
	def.primitives = LINE_LIST;
	def.alice_debug_line = ( stipple && vk.lineStipple ) ? 2 : 1;
	return vk_find_pipeline_ext( 0, &def, qfalse );
}


/*
=================
vk_set_alice_debug_line_state

After binding vk_alice_debug_line_pipeline: the run's width, within the
device's range, when it draws wide lines, and its stipple (factor 1..256, as
glLineStipple clamps it) when it stipples.
=================
*/
void vk_set_alice_debug_line_state( float width, int factor, unsigned short pattern, qboolean stipple )
{
	if ( vk.wideLines && qvkCmdSetLineWidth ) {
		if ( !( width >= 1.0f ) ) {
			width = 1.0f;
		}
		if ( width < vk.lineWidthRange[0] ) {
			width = vk.lineWidthRange[0];
		}
		if ( vk.lineWidthRange[1] >= 1.0f && width > vk.lineWidthRange[1] ) {
			width = vk.lineWidthRange[1];
		}
		qvkCmdSetLineWidth( vk.cmd->command_buffer, width );
	}
	if ( stipple && vk.lineStipple && qvkCmdSetLineStippleEXT ) {
		factor = factor < 1 ? 1 : factor > 256 ? 256 : factor;
		qvkCmdSetLineStippleEXT( vk.cmd->command_buffer, (uint32_t)factor, (uint16_t)pattern );
	}
}


void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def ) {
	if ( pipeline >= vk.pipelines_count ) {
		Com_Memset( def, 0, sizeof( *def ) );
	} else {
		Com_Memcpy( def, &vk.pipelines[ pipeline ].def, sizeof( *def ) );
	}
}


static void get_viewport_rect(VkRect2D *r)
{
	if ( backEnd.projection2D )
	{
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;
	}
	else
	{
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

static void get_viewport(VkViewport *viewport, Vk_Depth_Range depth_range) {
	VkRect2D r;

	get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
#ifdef USE_REVERSED_DEPTH
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
#else
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.3f;
			break;
#endif
	}
}

static void get_scissor_rect(VkRect2D *r) {
	if ( backEnd.projection2D && backEnd.scissor2DSet ) {
		r->offset.x = backEnd.scissor2D.x;
		r->offset.y = backEnd.scissor2D.y;
		r->extent.width = backEnd.scissor2D.width;
		r->extent.height = backEnd.scissor2D.height;
		return;
	}

	if ( backEnd.viewParms.portalView != PV_NONE )
	{
		r->offset.x = backEnd.viewParms.scissorX;
		r->offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r->extent.width = backEnd.viewParms.scissorWidth;
		r->extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r->offset.x < 0)
			r->offset.x = 0;
		if (r->offset.y < 0)
			r->offset.y = 0;

		// clamp to the render pass's own target: a shadow atlas tile lies
		// beyond the window's width
		if ( (uint32_t)r->offset.x >= vk.renderWidth )
			r->extent.width = 0;
		else if (r->offset.x + r->extent.width > vk.renderWidth)
			r->extent.width = vk.renderWidth - r->offset.x;
		if ( (uint32_t)r->offset.y >= vk.renderHeight )
			r->extent.height = 0;
		else if (r->offset.y + r->extent.height > vk.renderHeight)
			r->extent.height = vk.renderHeight - r->offset.y;
	}
}


static void get_mvp_transform( float *mvp )
{
	if ( backEnd.projection2D )
	{
		float mvp0 = 2.0f / glConfig.vidWidth;
		float mvp5 = 2.0f / glConfig.vidHeight;

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
#ifdef USE_REVERSED_DEPTH
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 0.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 1.0f; mvp[15] = 1.0f;
#else
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;
#endif
	}
	else
	{
		const float *p = backEnd.viewParms.projectionMatrix;
		float proj[16];
		Com_Memcpy( proj, p, 64 );

		// Update q3's OpenGL-style projection to Vulkan's clip-space Y convention.
		// Negate the whole clip-Y row: orthographic shadow projections carry
		// their light-space vertical offset in projectionMatrix[13].
		proj[1] = -p[1];
		proj[5] = -p[5];
		proj[9] = -p[9];
		proj[13] = -p[13];
		//proj[10] = ( p[10] - 1.0f ) / 2.0f;
		//proj[14] = p[14] / 2.0f;
		myGlMultMatrix( vk_world.modelview_transform, proj, mvp );
	}
}

void vk_get_liquid_mvp( float *mvp )
{
	get_mvp_transform( mvp );
}


void vk_clear_color( const vec4_t color ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	get_scissor_rect( &clear_rect.rect );
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;
	if ( clear_rect.rect.extent.width == 0 || clear_rect.rect.extent.height == 0 ) {
		return;	// nothing of the view lies in the target
	}

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
}


static void vk_clear_depth_internal( qboolean clear_stencil, qboolean force )
{
	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
		return;

	/*
	 * Depth can be cleared more than once in one command buffer (portals,
	 * stereo eyes, and later primary views).  A prior scene-depth snapshot is
	 * no longer valid even when this clear is elided for an already-clean
	 * attachment.  That includes a view begun in the post-bloom pass: RTCW's
	 * and ET's portal sky is a full-screen view whose snapshot ends the main
	 * pass, and the view behind it clears depth in the post-bloom pass that
	 * follows; its soft particles must fade against its own depth.
	 */
	if ( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) {
		vk.liquidDepth.copied = qfalse;
	}

	if ( !force && vk_world.dirty_depth_attachment == 0 )
		return;

	attachment.colorAttachment = 0;
	if ( vk.renderPassIndex == RENDER_PASS_CSM_SHADOW ) {
		attachment.clearValue.depthStencil.depth = 1.0f;
	} else {
#ifdef USE_REVERSED_DEPTH
		attachment.clearValue.depthStencil.depth = 0.0f;
#else
		attachment.clearValue.depthStencil.depth = 1.0f;
#endif
	}
	attachment.clearValue.depthStencil.stencil = 0;
	if ( clear_stencil && glConfig.stencilBits > 0 ) {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	} else {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	get_scissor_rect( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;
	if ( clear_rect[0].rect.extent.width == 0 || clear_rect[0].rect.extent.height == 0 ) {
		return;	// nothing of the view lies in the target
	}

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );
}


void vk_clear_depth( qboolean clear_stencil )
{
	vk_clear_depth_internal( clear_stencil, qfalse );
}


void vk_clear_depth_force( qboolean clear_stencil )
{
	vk_clear_depth_internal( clear_stencil, qtrue );
}


void vk_update_mvp( const float *m ) {
	float push_constants[16]; // mvp transform

	//
	// Specify push constants.
	//
	if ( m )
		Com_Memcpy( push_constants, m, sizeof( push_constants ) );
	else
		get_mvp_transform( push_constants );

	qvkCmdPushConstants( vk.cmd->command_buffer, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_MAIN ), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push_constants ), push_constants );

	vk.stats.push_size += sizeof( push_constants );
}


static VkBuffer shade_bufs[8];
static int bind_base;
static int bind_count;

static void vk_bind_index_attr( int index )
{
	if ( bind_base == -1 ) {
		bind_base = index;
		bind_count = 1;
	} else {
		bind_count = index - bind_base + 1;
	}
}


static void vk_bind_attr( int index, unsigned int item_size, const void *src ) {
	const uint32_t offset = PAD( vk.cmd->vertex_buffer_offset, 32 );
	const uint32_t size = tess.numVertexes * item_size;

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
	} else {
		vk.cmd->buf_offset[ index ] = offset;
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr( index );
}


uint32_t vk_tess_index( uint32_t numIndexes, const void *src ) {
	const uint32_t offset = vk.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof( tess.indexes[0] );

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
		return ~0U;
	} else {
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
		return offset;
	}
}


void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset )
{
	if ( vk.cmd->curr_index_buffer != buffer || vk.cmd->curr_index_offset != offset )
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32 );

	vk.cmd->curr_index_buffer = buffer;
	vk.cmd->curr_index_offset = offset;
}


#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex )
{
	qvkCmdDrawIndexed( vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0 );
}
#endif


void vk_bind_index( void )
{
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		vk.cmd->num_indexes = 0;
		//qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext( tess.numIndexes, tess.indexes );
}


void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes )
{
	uint32_t offset	= vk_tess_index( numIndexes, indexes );
	if ( offset != ~0U ) {
		vk_bind_index_buffer( vk.cmd->vertex_buffer, offset );
		vk.cmd->num_indexes = numIndexes;
	} else {
		// overflowed
		vk.cmd->num_indexes = 0;
	}
}


void vk_bind_geometry( uint32_t flags )
{
	//unsigned int size;
	bind_base = -1;
	bind_count = 0;

	if ( ( flags & ( TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2 ) ) == 0 )
		return;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.vertex_buffer;

		if ( flags & TESS_XYZ ) {  // 0
			vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr( 0 );
		}

		if ( flags & TESS_RGBA0 ) { // 1
			vk.cmd->vbo_offset[1] = tess.shader->stages[ tess.vboStage ]->rgb_offset[0];
			vk_bind_index_attr( 1 );
		}

		if ( flags & TESS_ST0 ) {  // 2
			vk.cmd->vbo_offset[2] = tess.shader->stages[ tess.vboStage ]->tex_offset[0];
			vk_bind_index_attr( 2 );
		}

		if ( flags & TESS_ST1 ) {  // 3
			vk.cmd->vbo_offset[3] = tess.shader->stages[ tess.vboStage ]->tex_offset[1];
			vk_bind_index_attr( 3 );
		}

		if ( flags & TESS_ST2 ) {  // 4
			vk.cmd->vbo_offset[4] = tess.shader->stages[ tess.vboStage ]->tex_offset[2];
			vk_bind_index_attr( 4 );
		}

		if ( flags & TESS_NNN ) { // 5
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );
		}

		if ( flags & TESS_RGBA1 ) { // 6
			vk.cmd->vbo_offset[6] = tess.shader->stages[ tess.vboStage ]->rgb_offset[1];
			vk_bind_index_attr( 6 );
		}

		if ( flags & TESS_RGBA2 ) { // 7
			vk.cmd->vbo_offset[7] = tess.shader->stages[ tess.vboStage ]->rgb_offset[2];
			vk_bind_index_attr( 7 );
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );

	} else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.cmd->vertex_buffer;

		if ( flags & TESS_XYZ ) {
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if ( flags & TESS_RGBA0 ) {
			vk_bind_attr(1, sizeof( color4ub_t ), tess.svars.colors[0][0].rgba);
		}

		if ( flags & TESS_ST0 ) {
			vk_bind_attr(2, sizeof( vec2_t ), tess.svars.texcoordPtr[0]);
		}

		if ( flags & TESS_ST1 ) {
			vk_bind_attr(3, sizeof( vec2_t ), tess.svars.texcoordPtr[1]);
		}

		if ( flags & TESS_ST2 ) {
			vk_bind_attr(4, sizeof( vec2_t ), tess.svars.texcoordPtr[2]);
		}

		if ( flags & TESS_NNN ) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if ( flags & TESS_RGBA1 ) {
			vk_bind_attr(6, sizeof( color4ub_t ), tess.svars.colors[1][0].rgba);
		}

		if ( flags & TESS_RGBA2 ) {
			vk_bind_attr(7, sizeof( color4ub_t ), tess.svars.colors[2][0].rgba);
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_bind_lighting( int stage, int bundle )
{
	bind_base = -1;
	bind_count = 0;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.vbo.vertex_buffer;

		vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk.cmd->vbo_offset[1] = tess.shader->stages[ stage ]->tex_offset[ bundle ];
		vk.cmd->vbo_offset[2] = tess.shader->normalOffset;

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 3, shade_bufs, vk.cmd->vbo_offset + 0 );

	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.cmd->vertex_buffer;

		vk_bind_attr( 0, sizeof( tess.xyz[0] ), &tess.xyz[0] );
		vk_bind_attr( 1, sizeof( vec2_t ), tess.svars.texcoordPtr[ bundle ] );
		vk_bind_attr( 2, sizeof( tess.normal[0] ), tess.normal );

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_reset_descriptor( int index )
{
	vk.cmd->descriptor_set.current[ index ] = VK_NULL_HANDLE;
}


void vk_update_descriptor( int index, VkDescriptorSet descriptor )
{
	if ( vk.cmd->descriptor_set.current[ index ] != descriptor ) {
		vk.cmd->descriptor_set.start = ( index < vk.cmd->descriptor_set.start ) ? index : vk.cmd->descriptor_set.start;
		vk.cmd->descriptor_set.end = ( index > vk.cmd->descriptor_set.end ) ? index : vk.cmd->descriptor_set.end;
	}
	vk.cmd->descriptor_set.current[ index ] = descriptor;
}


void vk_update_descriptor_offset( int index, uint32_t offset )
{
	vk.cmd->descriptor_set.offset[ index ] = offset;
}


void vk_bind_descriptor_sets( void )
{
	uint32_t offsets[2], offset_count;
	uint32_t start, end, count, i;

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U )
		return;

	end = vk.cmd->descriptor_set.end;

	/*
	 * Single-texture generic fragment variants declare the depth-fade sampler
	 * at set 2.  Vulkan treats that descriptor as statically used even when
	 * the specialization constant disables sampling, so the ordinary
	 * set-0/set-1 draw path must still bind a valid fallback set 2.
	 */
	if ( start <= VK_DESC_DEPTH_FADE &&
		end < VK_DESC_DEPTH_FADE &&
		vk.maxBoundDescriptorSets > VK_DESC_DEPTH_FADE ) {
		end = VK_DESC_DEPTH_FADE;
	}

	count = end - start + 1;

	/*
	 * Post-process passes invalidate main-layout bindings and can request a
	 * full-range restore before any raster overlay has pushed a uniform.
	 * Set zero is dynamic-uniform, so it needs its own descriptor and offset;
	 * sampler gaps use the ordinary white-image fallback.
	 */
	for ( i = start; i <= end; i++ ) {
		if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
			if ( i == VK_DESC_UNIFORM ) {
				vk.cmd->descriptor_set.current[i] =
					vk.cmd->uniform_descriptor;
				vk.cmd->descriptor_set.offset[i] =
					vk.cmd->uniform_read_offset;
			} else {
				vk.cmd->descriptor_set.current[i] =
					tr.whiteImage->descriptor;
			}
		}
	}

	/* The fallback above may supply both the uniform descriptor and offset. */
	offset_count = 0;
	if ( start == VK_DESC_UNIFORM ) {
		offsets[ offset_count++ ] = vk.cmd->descriptor_set.offset[start];
	}

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_MAIN ), start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}


static void vk_set_dlight_shadow_depth_bias( void )
{
	float depthBias;
	float slopeBias;

	if ( !qvkCmdSetDepthBias ) {
		return;
	}

	depthBias = r_dlightShadowCasterDepthBias ?
		r_dlightShadowCasterDepthBias->value : 1.0f;
	slopeBias = r_dlightShadowCasterSlopeBias ?
		r_dlightShadowCasterSlopeBias->value : 1.0f;
	depthBias = R_ShadowClampCasterDepthBias( depthBias );
	slopeBias = R_ShadowClampCasterSlopeBias( slopeBias );

#ifdef USE_REVERSED_DEPTH
	depthBias = -depthBias;
	slopeBias = -slopeBias;
#endif

	qvkCmdSetDepthBias( vk.cmd->command_buffer, depthBias, 0.0f, slopeBias );
}


static void vk_set_csm_shadow_depth_bias( void )
{
	float depthBias;
	float slopeBias;

	if ( !qvkCmdSetDepthBias ) {
		return;
	}

	depthBias = r_csmCasterDepthBias ? r_csmCasterDepthBias->value : 1.5f;
	slopeBias = r_csmCasterSlopeBias ? r_csmCasterSlopeBias->value : 1.5f;
	depthBias = R_ShadowClampCasterDepthBias( depthBias );
	slopeBias = R_ShadowClampCasterSlopeBias( slopeBias );

	qvkCmdSetDepthBias( vk.cmd->command_buffer, depthBias, 0.0f, slopeBias );
}


void vk_bind_pipeline( uint32_t pipeline ) {
	VkPipeline vkpipe;

	vkpipe = vk_gen_pipeline( pipeline );

	if ( vkpipe != vk.cmd->last_pipeline ) {
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe );
		vk.cmd->last_pipeline = vkpipe;
	}

	if ( vk.renderPassIndex == RENDER_PASS_DLIGHT_SHADOW ||
		vk.renderPassIndex == RENDER_PASS_SPOT_SHADOW ) {
		vk_set_dlight_shadow_depth_bias();
	} else if ( vk.renderPassIndex == RENDER_PASS_CSM_SHADOW ) {
		vk_set_csm_shadow_depth_bias();
	}

	vk_world.dirty_depth_attachment |= ( vk.pipelines[ pipeline ].def.state_bits & GLS_DEPTHMASK_TRUE );
}

static void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			vk.cmd->scissor_rect = scissor_rect;
		}

		get_viewport( &viewport, depth_range );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	}
}


void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if ( indexed ) {
		qvkCmdDrawIndexed( vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0 );
	} else {
		qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
	}
}


void vk_draw_dot( uint32_t storage_offset )
{
	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_STORAGE ), VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
}


static void vk_begin_render_pass( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[3];

	if ( renderPass == VK_NULL_HANDLE || frameBuffer == VK_NULL_HANDLE ) {
		ri.Error( ERR_DROP, "vk_begin_render_pass: null render pass or framebuffer handle" );
		return;
	}

	// Begin render pass.

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
		Com_Memset( clear_values, 0, sizeof( clear_values ) );
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = vk.msaaActive ? 3 : 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	vk_debug_render_pass_begin( renderPass );

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


void vk_begin_main_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_MAIN;
	vk.liquidDepth.copied = qfalse;
	vk.dlight_shadow_rendered = qfalse;
	vk.spot_shadow_rendered = qfalse;
	vk.csm_shadow_rendered = qfalse;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.main, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}


void vk_begin_main_render_pass_load( void )
{
	VkFramebuffer frameBuffer =
		vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_MAIN;
	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.main_load, frameBuffer, qfalse,
		vk.renderWidth, vk.renderHeight );
}


void vk_begin_post_bloom_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_POST_BLOOM;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.post_bloom, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}

static void vk_begin_global_fog_render_pass( void )
{
	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.global_fog, vk.framebuffers.global_fog,
		qfalse, vk.renderWidth, vk.renderHeight );
}

static void vk_begin_liquid_snapshot_render_pass( void )
{
	vk.renderPassIndex = RENDER_PASS_LIQUID_SNAPSHOT;
	vk.renderWidth = vk.liquidSnapshotWidth;
	vk.renderHeight = vk.liquidSnapshotHeight;
	vk.renderScaleX =
		(float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY =
		(float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass( vk.render_pass.liquid_snapshot,
		vk.framebuffers.liquid_snapshot, qfalse,
		vk.renderWidth, vk.renderHeight );
}


void vk_begin_bloom_extract_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.bloom_extract;

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth;
	vk.renderHeight = gls.captureHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.bloom_extract, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_blur_render_pass( uint32_t index )
{
	VkFramebuffer frameBuffer = vk.framebuffers.blur[ index ];

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth / ( 2 << ( index / 2 ) );
	vk.renderHeight = gls.captureHeight / ( 2 << ( index / 2 ) );

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.blur[ index ], frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


static void vk_begin_screenmap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.screenmap;

	vk.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk.renderWidth = vk.screenMapWidth;
	vk.renderHeight = vk.screenMapHeight;

	vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass( vk.render_pass.screenmap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}


void vk_end_render_pass( void )
{
	vk_debug_render_pass_end();
	qvkCmdEndRenderPass( vk.cmd->command_buffer );

//	vk.renderPassIndex = RENDER_PASS_MAIN;
}

static qboolean vk_scene_depth_snapshot_available( void )
{
	return ( vk.liquidDepth.image != VK_NULL_HANDLE &&
		vk.liquidDepth.image_view != VK_NULL_HANDLE &&
		vk.liquidDepth.descriptor != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

qboolean vk_depth_fade_supported( void )
{
	return ( vk_depth_fade_requested() &&
		vk.fboActive &&
		vkSamples == VK_SAMPLE_COUNT_1_BIT &&
		vk.liquidDepthSampleSupported &&
		vk.maxBoundDescriptorSets > VK_DESC_DEPTH_FADE ) ?
			qtrue : qfalse;
}

qboolean vk_depth_fade_available( void )
{
	return ( vk_depth_fade_supported() &&
		vk_scene_depth_snapshot_available() ) ? qtrue : qfalse;
}

/*
 * A shadow atlas interrupts the scene pass and resumes it afterwards. A view
 * can begin in the post-bloom pass rather than the main one: an earlier
 * full-screen view of the frame (the portal sky of RTCW, Enemy Territory or
 * Jedi Academy) whose depth-fade or liquid snapshot resumed the scene there
 * leaves the view behind it in that pass. Leave and resume whichever of the
 * two is open, as motion blur does.
 */
static renderPass_t vk_shadow_atlas_scene_pass = RENDER_PASS_MAIN;

static qboolean vk_shadow_atlas_scene_pass_open( void )
{
	if ( vk.renderPassIndex == RENDER_PASS_MAIN ) {
		return vk.render_pass.main_load != VK_NULL_HANDLE ? qtrue : qfalse;
	}
	if ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) {
		return vk.render_pass.post_bloom != VK_NULL_HANDLE ? qtrue : qfalse;
	}
	return qfalse;
}

static void vk_shadow_atlas_leave_scene_pass( void )
{
	vk_shadow_atlas_scene_pass = vk.renderPassIndex;
	vk_end_render_pass();
}

static void vk_shadow_atlas_resume_scene_pass( void )
{
	if ( vk_shadow_atlas_scene_pass == RENDER_PASS_POST_BLOOM ) {
		vk_begin_post_bloom_render_pass();
	} else {
		vk_begin_main_render_pass_load();
	}
}

qboolean vk_dlight_shadow_atlas_available( void )
{
	return ( vk.dlight_shadow_image != VK_NULL_HANDLE &&
		vk.dlight_shadow_image_view != VK_NULL_HANDLE &&
		vk.dlight_shadow_descriptor != VK_NULL_HANDLE &&
		vk.dlight_shadow_face_size > 0 ) ? qtrue : qfalse;
}

qboolean vk_dlight_shadow_atlas_ready( void )
{
	return ( vk_dlight_shadow_atlas_available() &&
		vk.dlight_shadow_rendered ) ? qtrue : qfalse;
}

qboolean vk_begin_dlight_shadow_render_pass( void )
{
	vk.dlight_shadow_rendered = qfalse;

	if ( !vk_dlight_shadow_atlas_available() ||
		vk.render_pass.dlight_shadow == VK_NULL_HANDLE ||
		vk.framebuffers.dlight_shadow == VK_NULL_HANDLE ||
		!vk_shadow_atlas_scene_pass_open() ) {
		return qfalse;
	}

	vk_shadow_atlas_leave_scene_pass();
	vk.renderPassIndex = RENDER_PASS_DLIGHT_SHADOW;
	vk.renderWidth = vk.dlight_shadow_atlas_width;
	vk.renderHeight = vk.dlight_shadow_atlas_height;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;
	vk_begin_render_pass( vk.render_pass.dlight_shadow,
		vk.framebuffers.dlight_shadow, qfalse,
		vk.renderWidth, vk.renderHeight );
	return qtrue;
}

void vk_end_dlight_shadow_render_pass( void )
{
	if ( vk.renderPassIndex != RENDER_PASS_DLIGHT_SHADOW ) {
		return;
	}

	vk_end_render_pass();
	vk_shadow_atlas_resume_scene_pass();
	vk.dlight_shadow_rendered = qtrue;
}

int vk_dlight_shadow_atlas_height( void )
{
	return (int)vk.dlight_shadow_atlas_height;
}

int vk_dlight_shadow_atlas_columns( void )
{
	return (int)vk.dlight_shadow_atlas_columns;
}

uint32_t vk_dlight_shadow_atlas_generation( void )
{
	return vk.dlight_shadow_generation;
}

qboolean vk_spot_shadow_atlas_available( void )
{
	return ( vk.spot_shadow_image != VK_NULL_HANDLE &&
		vk.spot_shadow_image_view != VK_NULL_HANDLE &&
		vk.spot_shadow_descriptor != VK_NULL_HANDLE &&
		vk.spot_shadow_tile_size > 0 ) ? qtrue : qfalse;
}

qboolean vk_spot_shadow_atlas_ready( void )
{
	return ( vk_spot_shadow_atlas_available() &&
		vk.spot_shadow_rendered ) ? qtrue : qfalse;
}

qboolean vk_begin_spot_shadow_render_pass( void )
{
	vk.spot_shadow_rendered = qfalse;

	if ( !vk_spot_shadow_atlas_available() ||
		vk.render_pass.dlight_shadow == VK_NULL_HANDLE ||
		vk.framebuffers.spot_shadow == VK_NULL_HANDLE ||
		!vk_shadow_atlas_scene_pass_open() ) {
		return qfalse;
	}

	vk_shadow_atlas_leave_scene_pass();
	vk.renderPassIndex = RENDER_PASS_SPOT_SHADOW;
	vk.renderWidth = vk.spot_shadow_atlas_width;
	vk.renderHeight = vk.spot_shadow_atlas_height;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;
	vk_begin_render_pass( vk.render_pass.dlight_shadow,
		vk.framebuffers.spot_shadow, qfalse,
		vk.renderWidth, vk.renderHeight );
	return qtrue;
}

void vk_end_spot_shadow_render_pass( void )
{
	if ( vk.renderPassIndex != RENDER_PASS_SPOT_SHADOW ) {
		return;
	}

	vk_end_render_pass();
	vk_shadow_atlas_resume_scene_pass();
	vk.spot_shadow_rendered = qtrue;
}

void vk_mark_spot_shadow_atlas_rendered( void )
{
	if ( vk_spot_shadow_atlas_available() ) {
		vk.spot_shadow_rendered = qtrue;
	}
}

int vk_spot_shadow_atlas_width( void )
{
	return (int)vk.spot_shadow_atlas_width;
}

int vk_spot_shadow_atlas_height( void )
{
	return (int)vk.spot_shadow_atlas_height;
}

int vk_spot_shadow_tile_size( void )
{
	return (int)vk.spot_shadow_tile_size;
}

uint32_t vk_spot_shadow_atlas_generation( void )
{
	return vk.spot_shadow_generation;
}

VkDescriptorSet vk_spot_shadow_descriptor( void )
{
	return vk.spot_shadow_descriptor;
}

qboolean vk_csm_shadow_atlas_available( void )
{
	return ( vk.csm_shadow_image != VK_NULL_HANDLE &&
		vk.csm_shadow_image_view != VK_NULL_HANDLE &&
		vk.csm_shadow_descriptor != VK_NULL_HANDLE &&
		vk.csm_shadow_cascade_size > 0 &&
		vk.csm_shadow_cascade_count > 0 ) ? qtrue : qfalse;
}

qboolean vk_csm_shadow_atlas_ready( void )
{
	return ( vk_csm_shadow_atlas_available() &&
		vk.csm_shadow_rendered ) ? qtrue : qfalse;
}

qboolean vk_begin_csm_shadow_render_pass( void )
{
	vk.csm_shadow_rendered = qfalse;

	if ( !vk_csm_shadow_atlas_available() ||
		vk.render_pass.dlight_shadow == VK_NULL_HANDLE ||
		vk.framebuffers.csm_shadow == VK_NULL_HANDLE ||
		!vk_shadow_atlas_scene_pass_open() ) {
		return qfalse;
	}

	vk_shadow_atlas_leave_scene_pass();
	vk.renderPassIndex = RENDER_PASS_CSM_SHADOW;
	vk.renderWidth = vk.csm_shadow_atlas_width;
	vk.renderHeight = vk.csm_shadow_atlas_height;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;
	vk_begin_render_pass( vk.render_pass.dlight_shadow,
		vk.framebuffers.csm_shadow, qfalse,
		vk.renderWidth, vk.renderHeight );
	return qtrue;
}

void vk_end_csm_shadow_render_pass( void )
{
	if ( vk.renderPassIndex != RENDER_PASS_CSM_SHADOW ) {
		return;
	}

	vk_end_render_pass();
	vk_shadow_atlas_resume_scene_pass();
	vk.csm_shadow_rendered = qtrue;
}

void vk_mark_csm_shadow_atlas_rendered( void )
{
	if ( vk_csm_shadow_atlas_available() ) {
		vk.csm_shadow_rendered = qtrue;
	}
}

int vk_csm_shadow_atlas_width( void )
{
	return (int)vk.csm_shadow_atlas_width;
}

int vk_csm_shadow_atlas_height( void )
{
	return (int)vk.csm_shadow_atlas_height;
}

int vk_csm_shadow_cascade_size( void )
{
	return (int)vk.csm_shadow_cascade_size;
}

uint32_t vk_csm_shadow_atlas_generation( void )
{
	return vk.csm_shadow_generation;
}

VkDescriptorSet vk_csm_shadow_descriptor( void )
{
	return vk.csm_shadow_descriptor;
}

qboolean vk_depth_fade_ready( void )
{
	return ( vk_depth_fade_available() && vk.liquidDepth.copied ) ?
		qtrue : qfalse;
}

qboolean vk_liquid_depth_ready( void )
{
	return ( vk_scene_depth_snapshot_available() &&
		vk.liquidDepth.copied ) ? qtrue : qfalse;
}

void vk_copy_depth_fade( void )
{
	VkImageCopy region;

	if ( !vk_depth_fade_available() || vk_depth_fade_ready() ||
		vk.render_pass.post_bloom == VK_NULL_HANDLE ||
		( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ||
		backEnd.viewParms.portalView != PV_NONE ||
		backEnd.viewParms.stereoFrame != STEREO_CENTER ||
		backEnd.viewParms.viewportX != 0 ||
		backEnd.viewParms.viewportY != 0 ||
		backEnd.viewParms.viewportWidth != glConfig.vidWidth ||
		backEnd.viewParms.viewportHeight != glConfig.vidHeight ||
		( vk.renderPassIndex != RENDER_PASS_MAIN &&
		  vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) ) {
		return;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer,
		vk.depth_image, VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer,
		vk.liquidDepth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	Com_Memset( &region, 0, sizeof( region ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	region.dstSubresource.layerCount = 1;
	region.extent.width = glConfig.vidWidth;
	region.extent.height = glConfig.vidHeight;
	region.extent.depth = 1;

	qvkCmdCopyImage( vk.cmd->command_buffer,
		vk.depth_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.liquidDepth.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region );

	record_image_layout_transition( vk.cmd->command_buffer,
		vk.liquidDepth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer,
		vk.depth_image, VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );

	vk.liquidDepth.copied = qtrue;
	vk_begin_post_bloom_render_pass();

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end =
		MIN( VK_DESC_COUNT, vk.maxBoundDescriptorSets ) - 1;
	Com_Memset( &vk.cmd->scissor_rect, 0xff,
		sizeof( vk.cmd->scissor_rect ) );
}

void vk_draw_world_cel_outline( void )
{
	float constants[4];
	float width;
	float alpha;
	float threshold;
	VkDescriptorSet descriptor;

	if ( !R_CelShadingWorldActive() ||
		( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ||
		backEnd.viewParms.portalView != PV_NONE ||
		backEnd.viewParms.stereoFrame != STEREO_CENTER ||
		backEnd.viewParms.viewportX != 0 ||
		backEnd.viewParms.viewportY != 0 ||
		backEnd.viewParms.viewportWidth != glConfig.vidWidth ||
		backEnd.viewParms.viewportHeight != glConfig.vidHeight ||
		!vk.fboActive || vk.world_outline_pipeline == VK_NULL_HANDLE ||
		!vk_depth_fade_available() ||
		( vk.renderPassIndex != RENDER_PASS_MAIN &&
		  vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) ) {
		return;
	}

	width = r_celShadingWorldWidth ?
		Com_Clamp( 1.0f, 8.0f, r_celShadingWorldWidth->value ) : 2.0f;
	alpha = r_celShadingWorldAlpha ?
		Com_Clamp( 0.0f, 1.0f, r_celShadingWorldAlpha->value ) : 1.0f;
	threshold = r_celShadingWorldDepthThreshold ?
		Com_Clamp( 0.0001f, 0.02f,
			r_celShadingWorldDepthThreshold->value ) : 0.0015f;
	if ( alpha <= 0.0f ) {
		return;
	}

	if ( !vk_depth_fade_ready() ) {
		vk_copy_depth_fade();
	}
	/* The RTX depth snapshot helper resumes composition in post-bloom so the
	 * overlay is compatible with both raster-only and successful RT frames. */
	if ( !vk_depth_fade_ready() ||
		vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return;
	}

	constants[0] = glConfig.vidWidth > 0 ?
		width / (float)glConfig.vidWidth : 1.0f;
	constants[1] = glConfig.vidHeight > 0 ?
		width / (float)glConfig.vidHeight : 1.0f;
	constants[2] = threshold;
	constants[3] = alpha;

	descriptor = vk.liquidDepth.descriptor;
	qvkCmdBindPipeline( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, vk.world_outline_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ),
		0, 1, &descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer,
		vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ),
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( constants ), constants );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end =
		MIN( VK_DESC_COUNT, vk.maxBoundDescriptorSets ) - 1;
	Com_Memset( &vk.cmd->scissor_rect, 0xff,
		sizeof( vk.cmd->scissor_rect ) );
}

qboolean vk_capture_liquid_scene( void )
{
	VkMemoryBarrier barrier;

	if ( backEnd.liquidScreenMapDone ) {
		return qtrue;
	}
	if ( !vk_liquid_requested() || !vk.fboActive ||
		vk.liquid_snapshot_pipeline == VK_NULL_HANDLE ||
		vk.liquidSnapshot.source_descriptor == VK_NULL_HANDLE ||
		vk.liquidSnapshot.color_descriptor == VK_NULL_HANDLE ||
		vk.render_pass.liquid_snapshot == VK_NULL_HANDLE ||
		vk.render_pass.post_bloom == VK_NULL_HANDLE ||
		vk.framebuffers.liquid_snapshot == VK_NULL_HANDLE ||
		( vk.renderPassIndex != RENDER_PASS_MAIN &&
		  vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) ) {
		return qfalse;
	}

	/*
	 * The active color target cannot be sampled in-place. End composition,
	 * copy optional single-sample depth, render the scaled immutable color
	 * snapshot, then resume the scene through the existing load pass.
	 */
	vk_end_render_pass();

	if ( !vk.liquidDepth.copied &&
		vk.liquidDepth.image != VK_NULL_HANDLE &&
		vkSamples == VK_SAMPLE_COUNT_1_BIT ) {
		VkImageCopy region;

		record_image_layout_transition( vk.cmd->command_buffer,
			vk.depth_image, VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
		record_image_layout_transition( vk.cmd->command_buffer,
			vk.liquidDepth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

		Com_Memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.dstSubresource.layerCount = 1;
		region.extent.width = glConfig.vidWidth;
		region.extent.height = glConfig.vidHeight;
		region.extent.depth = 1;

		qvkCmdCopyImage( vk.cmd->command_buffer,
			vk.depth_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.liquidDepth.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region );

		record_image_layout_transition( vk.cmd->command_buffer,
			vk.liquidDepth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
		record_image_layout_transition( vk.cmd->command_buffer,
			vk.depth_image, VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
		vk.liquidDepth.copied = qtrue;
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
			VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		0, 1, &barrier, 0, NULL, 0, NULL );

	vk_begin_liquid_snapshot_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.liquid_snapshot_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ),
		0, 1, &vk.liquidSnapshot.source_descriptor, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();
	vk_begin_post_bloom_render_pass();

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end =
		MIN( VK_DESC_COUNT, vk.maxBoundDescriptorSets ) - 1;
	Com_Memset( &vk.cmd->scissor_rect, 0xff,
		sizeof( vk.cmd->scissor_rect ) );
	backEnd.liquidScreenMapDone = qtrue;
	return qtrue;
}


/* The entity refraction pass recaptures the snapshot before each distortion
 * copy (renderercommon/tr_entity_refraction.h), whatever a liquid took
 * earlier. A liquid's own pending capture is left as it was. */
qboolean vk_capture_refraction_scene( void )
{
	const qboolean liquidDone = backEnd.liquidScreenMapDone;
	qboolean captured;

	backEnd.liquidScreenMapDone = qfalse;
	captured = vk_capture_liquid_scene();
	backEnd.liquidScreenMapDone = liquidDone;
	return captured;
}

void vk_draw_global_fog( void )
{
	static int lastFogFrame = -1;
	const globalFog_t *fog = tr.world ? &tr.world->globalFog : NULL;
	VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
	float constants[12];
	qboolean sceneLinear;
	vec3_t sceneColor;
	float opacity;
	float outputScale;
	float zNear;
	float zFar;

	if ( !vk_global_fog_enabled() || !fog || !fog->loaded ||
		vk.global_fog_pipeline == VK_NULL_HANDLE ||
		vk.render_pass.global_fog == VK_NULL_HANDLE ||
		vk.framebuffers.global_fog == VK_NULL_HANDLE ||
		vk.depth_sample_descriptor == VK_NULL_HANDLE ||
		( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ||
		/* A full-screen sky portal precedes the world view. Its depth must
		 * neither fog the world nor consume that view's once-per-frame pass. */
		backEnd.refdef.skyboxPortalScene ||
		backEnd.viewParms.portalView != PV_NONE ||
		backEnd.viewParms.stereoFrame != STEREO_CENTER ||
		backEnd.viewParms.viewportX != 0 ||
		backEnd.viewParms.viewportY != 0 ||
		backEnd.viewParms.viewportWidth != glConfig.vidWidth ||
		backEnd.viewParms.viewportHeight != glConfig.vidHeight ||
		( vk.renderPassIndex != RENDER_PASS_MAIN &&
			vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) ||
		lastFogFrame == tr.frameCount ) {
		return;
	}

	opacity = Com_Clamp( 0.0f, 1.0f,
		fog->opacity * ( r_globalFogStrength ? r_globalFogStrength->value : 1.0f ) );
	zNear = r_znear ? r_znear->value : 4.0f;
	zFar = backEnd.viewParms.zFar;
	if ( opacity <= 0.0f || fog->density <= 0.0f ||
		zNear <= 0.0f || zFar <= zNear ) {
		return;
	}

	/* Match the gamma pipeline's obScale/tone-map exposure so the authored
	 * color survives the output transform unchanged. */
	sceneLinear = vk_scene_linear_enabled();
	outputScale = (float)( 1 << tr.overbrightBits );
	if ( sceneLinear && r_tonemapExposure ) {
		outputScale *= Com_Clamp( 0.1f, 8.0f, r_tonemapExposure->value );
	}
	R_GlobalFogSceneColor( fog, outputScale, sceneLinear, sceneColor );

	constants[0] = sceneColor[0];
	constants[1] = sceneColor[1];
	constants[2] = sceneColor[2];
	constants[3] = opacity;
	constants[4] = fog->start;
	constants[5] = fog->end;
	constants[6] = fog->density;
	constants[7] = (float)fog->mode;
	constants[8] = zNear;
	constants[9] = zFar;
	constants[10] = fog->sky ? 1.0f : 0.0f;
#ifdef USE_REVERSED_DEPTH
	constants[11] = 1.0f;
#else
	constants[11] = 0.0f;
#endif

	if ( glConfig.stencilBits > 0 ) {
		depthAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();
	record_image_layout_transition( vk.cmd->command_buffer, vk.depth_image,
		depthAspects,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );

	vk_begin_global_fog_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.global_fog_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ),
		0, 1, &vk.depth_sample_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer,
		vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ),
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( constants ), constants );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.depth_image,
		depthAspects,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	vk_begin_post_bloom_render_pass();

	/*
	 * The fog pass binds the private post-process pipeline layout. Force the
	 * resumed material path to restore every main-layout descriptor and all
	 * dynamic viewport/scissor state before HUD or console draws.
	 */
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end =
		MIN( VK_DESC_COUNT, vk.maxBoundDescriptorSets ) - 1;
	Com_Memset( &vk.cmd->scissor_rect, 0xff, sizeof( vk.cmd->scissor_rect ) );

	lastFogFrame = tr.frameCount;
}


static qboolean vk_find_screenmap_drawsurfs( void )
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for ( ;; ) {
		curCmd = PADP( curCmd, sizeof(void *) );
		switch ( *(const int *)curCmd ) {
			case RC_DRAW_BUFFER:
				db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
			case RC_DRAW_SURFS:
				ds_cmd = (const drawSurfsCommand_t *)curCmd;
				return ds_cmd->refdef.needScreenMap;
			default:
				return qfalse;
		}
	}
}


#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

static void vk_acquire_frame_image( void )
{
	VkResult res;
	if ( s_vkSwapchainResizePending ) {
		( void )vk_restart_swapchain( __func__, s_vkSwapchainResizeResult );
	}

	if ( !ri.CL_IsMinimized() && !vk.cmd->swapchain_image_acquired ) {
		qboolean retry = qfalse;
		int64_t acquireStarted;
_retry:
		acquireStarted = vk_rt_now_microseconds();
		res = qvkAcquireNextImageKHR( vk.device, vk.swapchain, 1 * 1000000000ULL, vk.cmd->image_acquired, VK_NULL_HANDLE, &vk.cmd->swapchain_image_index );
		vk_rt_latency_wait( acquireStarted, qtrue );
		// when running via RDP: "Application has already acquired the maximum number of images (0x2)"
		// probably caused by "device lost" errors
		if ( res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR ) {
			vk.cmd->swapchain_image_acquired = qtrue;
			vk_lifetime_mark_use( RTX_VK_RESOURCE_SWAPCHAIN, "vkAcquireNextImageKHR" );

			if ( res == VK_SUBOPTIMAL_KHR ) {
				vk_schedule_swapchain_resize( __func__, res, "vkAcquireNextImageKHR returned VK_SUBOPTIMAL_KHR" );
			}
		} else if ( res == VK_ERROR_OUT_OF_DATE_KHR && retry == qfalse ) {
			// swapchain re-creation needed
			retry = qtrue;
			if ( vk_restart_swapchain( __func__, res ) ) {
				goto _retry;
			}
		} else if ( res == VK_ERROR_SURFACE_LOST_KHR || res == VK_TIMEOUT || res == VK_NOT_READY ) {
			vk_schedule_swapchain_resize( __func__, res, "vkAcquireNextImageKHR returned transient status" );
		} else if ( res < 0 ) {
			ri.Error( ERR_FATAL, "vkAcquireNextImageKHR returned %s", vk_result_string( res ) );
		} else {
			vk_schedule_swapchain_resize( __func__, res, "vkAcquireNextImageKHR returned non-success status" );
		}
		if ( !vk.cmd->swapchain_image_acquired && !vk.fboActive ) {
			ri.Error( ERR_FATAL, "vkAcquireNextImageKHR returned %s without an offscreen fallback path", vk_result_string( res ) );
		}
	}

}

void vk_rt_frame_event( refFrameEvent_t event, int64_t microseconds )
{
	int policy, slot;
	if ( event < 0 || event >= REF_FRAME_EVENT_COUNT ) return;
	if ( event == REF_FRAME_PREPARE_INPUT ) {
		Com_Memset( s_vkRtLatency.events, 0, sizeof( s_vkRtLatency.events ) );
		s_vkRtLatency.earlyFence = s_vkRtLatency.lateFence = 0.0f;
		s_vkRtLatency.earlyAcquire = s_vkRtLatency.lateAcquire = 0.0f;
		s_vkRtLatency.submitMs = s_vkRtLatency.presentMs = 0.0f;
	}
	s_vkRtLatency.events[event] = microseconds;
	if ( event != REF_FRAME_PREPARE_INPUT || vk.device == VK_NULL_HANDLE ||
		vk.caps.activeRtMode != 2 || !rtx_rt_low_latency || !rtx_rt_low_latency->integer ||
		ri.CL_IsMinimized() ) return;
	policy = rtx_rt_low_latency->integer;
	vk.cmd = &vk.tess[vk.cmd_index];
	slot = policy == 1 ? s_vkRtLatency.lastSubmittedSlot : (int)vk.cmd_index;
	s_vkRtLatency.preparingInput = qtrue;
	if ( slot >= 0 && slot < NUM_COMMAND_BUFFERS && vk.tess[slot].waitForFence ) {
		const int64_t started = vk_rt_now_microseconds();
		VkResult result = qvkWaitForFences( vk.device, 1, &vk.tess[slot].rendering_finished_fence, VK_TRUE, 10000000000ULL );
		vk_rt_latency_wait( started, qfalse );
		if ( result != VK_SUCCESS ) {
			s_vkRtLatency.preparingInput = qfalse;
			ri.Printf( PRINT_WARNING, "RTX RT: pre-input fence wait returned %s\n", vk_result_string( result ) );
			return;
		}
	}
	// Acquisition can block under FIFO presentation even after the GPU has
	// finished. Do it before input too. Fence reset stays in vk_begin_frame;
	// there is no command recording here and no simulation clock adjustment.
	vk_acquire_frame_image();
	s_vkRtLatency.preparingInput = qfalse;
}

void vk_begin_frame( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkResult res;

	if ( vk.frame_count++ ) // might happen during stereo rendering
		return;

	vk_lifetime_begin_frame();

#ifdef USE_UPLOAD_QUEUE
	vk_submit_staging_buffer( qtrue );
#endif

	vk.cmd = &vk.tess[ vk.cmd_index ];

	if ( vk.cmd->waitForFence ) {
		const int64_t waitStarted = vk_rt_now_microseconds();
		vk.cmd->waitForFence = qfalse;
		res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
		vk_rt_latency_wait( waitStarted, qfalse );
		if ( res != VK_SUCCESS ) {
			if ( res == VK_ERROR_DEVICE_LOST ) {
				// silently discard previous command buffer
				ri.Printf( PRINT_WARNING, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
			}
			else {
				ri.Error( ERR_FATAL, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
			}
		}
		VK_CHECK( qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence ) );
	}

	vk_acquire_frame_image();

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );

	// Ensure visibility of geometry buffers writes.
	//record_buffer_memory_barrier( vk.cmd->command_buffer, vk.cmd->vertex_buffer, vk.geometry_buffer_size, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT );

#if 0
	// add explicit layout transition dependency
	if ( vk.fboActive ) {
		record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( vk.cmd->command_buffer, vk.swapchain_images[ vk.swapchain_image_index ], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );
	}
#endif

	if ( vk.cmd->vertex_buffer_offset > vk.stats.vertex_buffer_max ) {
		vk.stats.vertex_buffer_max = vk.cmd->vertex_buffer_offset;
	}

	if ( vk.stats.push_size > vk.stats.push_size_max ) {
		vk.stats.push_size_max = vk.stats.push_size;
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;

	backEnd.screenMapDone = qfalse;

	if ( vk_find_screenmap_drawsurfs() ) {
		vk_begin_screenmap_render_pass();
	} else {
		vk_begin_main_render_pass();
	}

	// dynamic vertex buffer layout
	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;

	Com_Memset( &vk.cmd->descriptor_set, 0, sizeof( vk.cmd->descriptor_set ) );
	vk.cmd->descriptor_set.start = ~0U;
	//vk.cmd->descriptor_set.end = 0;

	Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

	// other stats
	vk.stats.push_size = 0;
}


static void vk_resize_geometry_buffer( void )
{
	int i;

	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

	ri.Printf( PRINT_DEVELOPER, "...geometry buffer resized to %iK\n", (int)( vk.geometry_buffer_size / 1024 ) );
}


void vk_end_frame( void )
{
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore waits[2], signals[2];
	const VkPipelineStageFlags wait_dst_stage_mask[2] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
	};
#else
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif
	VkSubmitInfo submit_info;

	if ( vk.frame_count == 0 )
		return;

	vk.frame_count = 0;

	if ( vk.geometry_buffer_size_new )
	{
		vk_resize_geometry_buffer();
		// issue: one frame may be lost during video recording
		// solution: re-record all commands again? (might be complicated though)
		vk_lifetime_end_frame( qfalse );
		return;
	}

	if ( vk.fboActive )
	{
		vk.cmd->last_pipeline = VK_NULL_HANDLE; // do not restore clobbered descriptors in vk_bloom()

		if ( r_bloom->integer )
		{
			vk_bloom();
		}

		if ( ( backEnd.screenshotMask || backEnd.levelshotPending || backEnd.stvefThumbnail )
			&& vk.capture.image )
		{
			vk_end_render_pass();

			// render to capture FBO
			vk_begin_render_pass( vk.render_pass.capture, vk.framebuffers.capture, qfalse, gls.captureWidth, gls.captureHeight );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.capture_pipeline );
			vk_push_post_process_constants();
			vk_bind_gamma_descriptor_sets();

			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		}

		if ( !ri.CL_IsMinimized() && vk.cmd->swapchain_image_acquired )
		{
			vk_end_render_pass();

			vk.renderWidth = gls.windowWidth;
			vk.renderHeight = gls.windowHeight;

			vk.renderScaleX = 1.0;
			vk.renderScaleY = 1.0;

			vk_begin_render_pass( vk.render_pass.gamma, vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ], qfalse, vk.renderWidth, vk.renderHeight );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.gamma_pipeline );
			vk_push_post_process_constants();
			vk_bind_gamma_descriptor_sets();

			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		}
	}

	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.cmd->command_buffer;
	if ( !ri.CL_IsMinimized() && vk.cmd->swapchain_image_acquired ) {
#ifdef USE_UPLOAD_QUEUE
		if ( vk.image_uploaded != VK_NULL_HANDLE ) {
			waits[0] = vk.cmd->image_acquired;
			waits[1] = vk.image_uploaded;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			signals[1] = vk.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk.rendering_finished = vk.cmd->rendering_finished2;
			vk.image_uploaded = VK_NULL_HANDLE;
		} else if ( vk.rendering_finished != VK_NULL_HANDLE ) {
			waits[0] = vk.cmd->image_acquired;
			waits[1] = vk.rendering_finished;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			signals[1] = vk.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk.rendering_finished = vk.cmd->rendering_finished2;
		} else {
			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
		}
#else
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
#endif
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
	}

	s_vkRtLatency.submit = vk_rt_now_microseconds();
	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence ) );
	s_vkRtLatency.submitMs = vk_rt_elapsed_ms( s_vkRtLatency.submit, vk_rt_now_microseconds() );
	s_vkRtLatency.lastSubmittedSlot = (int)vk.cmd_index;
	vk.cmd->waitForFence = qtrue;

	// presentation may take undefined time to complete, we can't measure it in a reliable way
	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;
	vk_lifetime_end_frame( qtrue );

	vk.renderPassIndex = RENDER_PASS_MAIN;
}


void vk_present_frame( void )
{
	VkPresentInfoKHR present_info;
	VkResult res;

	if ( ri.CL_IsMinimized() || !vk.cmd->swapchain_image_acquired ) {
		return;
	}

	if ( !vk.cmd->waitForFence ) {
		// nothing has been submitted this frame due to geometry buffer overflow?
		return;
	}

	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk.swapchain;
	present_info.pImageIndices = &vk.cmd->swapchain_image_index;
	present_info.pResults = NULL;

	vk.cmd->swapchain_image_acquired = qfalse;

	s_vkRtLatency.present = vk_rt_now_microseconds();
	res = qvkQueuePresentKHR( vk.queue, &present_info );
	{
		const int64_t presented = vk_rt_now_microseconds();
		s_vkRtLatency.presentMs = vk_rt_elapsed_ms( s_vkRtLatency.present, presented );
		vk_rt_latency_report( presented );
	}
	vk_lifetime_mark_use( RTX_VK_RESOURCE_SWAPCHAIN, "vkQueuePresentKHR" );
	switch ( res ) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
		case VK_ERROR_SURFACE_LOST_KHR:
			// swapchain re-creation needed
			( void )vk_restart_swapchain( __func__, res );
			return;
		case VK_ERROR_DEVICE_LOST:
			// we can ignore that
			ri.Printf( PRINT_DEVELOPER, "vkQueuePresentKHR: device lost\n" );
			break;
		default:
			// or we don't
			ri.Error( ERR_FATAL, "vkQueuePresentKHR returned %s", vk_result_string( res ) );
	}

	// pickup next command buffer for rendering
	vk.cmd_index++;
	vk.cmd_index %= NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];
}


static qboolean is_bgr( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
			return qtrue;
		default:
			return qfalse;
	}
}


/*
Read the frame back for a screenshot or a video frame. False, with nothing
read, when the device has no memory left for the copy: another program may
hold it, and a screenshot is not worth ending the game over.
*/
qboolean vk_read_pixels( byte *buffer, uint32_t width, uint32_t height )
{
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkMemoryRequirements memory_requirements;
	VkMemoryPropertyFlags memory_reqs;
	VkMemoryPropertyFlags memory_flags;
	VkMemoryAllocateInfo alloc_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	VkImageCreateInfo desc;
	VkImage srcImage;
	VkImageLayout srcImageLayout;
	VkImage dstImage;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	qboolean invalidate_ptr;

	VK_CHECK( qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e12 ) );

	if ( vk.fboActive ) {
		if ( vk.capture.image ) {
			// dedicated capture buffer
			srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			srcImage = vk.capture.image;
		} else {
			srcImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			srcImage = vk.color_image;
		}
	} else {
		srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		srcImage = vk.swapchain_images[ vk.cmd->swapchain_image_index ];
	}

	Com_Memset( &desc, 0, sizeof( desc ) );

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = vk.capture_format;
	desc.extent.width = width;
	desc.extent.height = height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( qvkCreateImage( vk.device, &desc, NULL, &dstImage ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s(): no image to read the frame back into\n", __func__ );
		return qfalse;
	}

	qvkGetImageMemoryRequirements( vk.device, dstImage, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;

	// host_cached bit is desirable for fast reads
	memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
	if ( alloc_info.memoryTypeIndex == ~0 ) {
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
		if ( alloc_info.memoryTypeIndex == ~0U ) {
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
			if ( alloc_info.memoryTypeIndex == ~0U ) {
				ri.Printf( PRINT_WARNING, "WARNING: %s(): no host-visible memory type for image capture\n", __func__ );
				qvkDestroyImage( vk.device, dstImage, NULL );
				return qfalse;
			}
		}
	}

	if ( memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
		invalidate_ptr = qfalse;
	} else {
		 // according to specification - must be performed if host_coherent is not set
		invalidate_ptr = qtrue;
	}

	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s(): no memory left to read the frame back (%u bytes)\n", __func__,
			(unsigned)alloc_info.allocationSize );
		qvkDestroyImage( vk.device, dstImage, NULL );
		return qfalse;
	}
	VK_CHECK(qvkBindImageMemory(vk.device, dstImage, memory, 0));

	command_buffer = begin_command_buffer();

	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			srcImageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			0, 0);
	}

	record_image_layout_transition( command_buffer, dstImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	// end_command_buffer( command_buffer );

	// command_buffer = begin_command_buffer();

	if ( vk.blitEnabled ) {
		VkImageBlit region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0].x = 0;
		region.srcOffsets[0].y = 0;
		region.srcOffsets[0].z = 0;
		region.srcOffsets[1].x = width;
		region.srcOffsets[1].y = height;
		region.srcOffsets[1].z = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstOffsets[0] = region.srcOffsets[0];
		region.dstOffsets[1] = region.srcOffsets[1];

		qvkCmdBlitImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST );

	} else {
		VkImageCopy region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffset.x = 0;
		region.srcOffset.y = 0;
		region.srcOffset.z = 0;
		region.dstSubresource = region.srcSubresource;
		region.dstOffset = region.srcOffset;
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		qvkCmdCopyImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	end_command_buffer( command_buffer, __func__ );

	// Copy data from destination image to memory buffer.
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	qvkGetImageSubresourceLayout( vk.device, dstImage, &subresource, &layout );

	VK_CHECK( qvkMapMemory( vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data ) );

	if ( invalidate_ptr )
	{
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = NULL;
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		range.offset = 0;
		qvkInvalidateMappedMemoryRanges( vk.device, 1, &range );
	}

	data += layout.offset;

	switch ( vk.capture_format ) {
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16: pixel_width = 2; break;
		case VK_FORMAT_R16G16B16A16_UNORM: pixel_width = 8; break;
		default: pixel_width = 4; break;
	}

	buffer_ptr = buffer + width * (height - 1) * 3;
	for ( i = 0; i < height; i++ ) {
		switch ( pixel_width ) {
			case 2: {
				uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = ((src[n]>>12)&0xF)<<4;
					buffer_ptr[n*3+1] = ((src[n]>>8)&0xF)<<4;
					buffer_ptr[n*3+2] = ((src[n]>>4)&0xF)<<4;
				}
			} break;

			case 4: {
				for ( n = 0; n < width; n++ ) {
					Com_Memcpy( &buffer_ptr[n*3], &data[n*4], 3 );
					//buffer_ptr[n*3+0] = data[n*4+0];
					//buffer_ptr[n*3+1] = data[n*4+1];
					//buffer_ptr[n*3+2] = data[n*4+2];
				}
			} break;

			case 8: {
				const uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = src[n*4+0]>>8;
					buffer_ptr[n*3+1] = src[n*4+1]>>8;
					buffer_ptr[n*3+2] = src[n*4+2]>>8;
				}
			} break;
		}
		buffer_ptr -= width * 3;
		data += layout.rowPitch;
	}

	if ( is_bgr( vk.capture_format ) ) {
		buffer_ptr = buffer;
		for ( i = 0; i < width * height; i++ ) {
			byte tmp = buffer_ptr[0];
			buffer_ptr[0] = buffer_ptr[2];
			buffer_ptr[2] = tmp;
			buffer_ptr += 3;
		}
	}

	qvkDestroyImage( vk.device, dstImage, NULL );
	qvkFreeMemory( vk.device, memory, NULL );

	// restore previous layout
	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		command_buffer = begin_command_buffer();

		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			srcImageLayout, 0, 0 );

		end_command_buffer( command_buffer, "restore layout" );
	}
	return qtrue;
}


qboolean vk_motion_blur( void )
{
	const qboolean requested =
		( r_motionBlur && r_motionBlur->integer &&
		  r_motionBlurStrength && r_motionBlurStrength->value > 0.0f ) ?
			qtrue : qfalse;
	const qboolean resumePostBloom =
		( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ? qtrue : qfalse;
	float constants[8] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 1.0f
	};
	int viewRect[4];
	VkPipelineLayout layout;

	if ( !requested ) {
		R_MotionBlur_ResetView( &vk_motion_blur_view );
		return qfalse;
	}
	if ( backEnd.doneMotionBlur ) {
		return qfalse;
	}
	if ( ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ||
		backEnd.viewParms.portalView != PV_NONE ||
		backEnd.viewParms.stereoFrame != STEREO_CENTER ||
		backEnd.viewParms.viewportX != 0 ||
		backEnd.viewParms.viewportY != 0 ||
		backEnd.viewParms.viewportWidth != glConfig.vidWidth ||
		backEnd.viewParms.viewportHeight != glConfig.vidHeight ) {
		return qfalse;
	}
	if ( vk.renderPassIndex != RENDER_PASS_MAIN &&
		vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return qfalse;
	}
	if ( !backEnd.doneSurfaces || !vk.fboActive || ri.CL_IsMinimized() ||
		glConfig.stereoEnabled ||
		vk.motion_blur_image == VK_NULL_HANDLE ||
		vk.motion_blur_descriptor == VK_NULL_HANDLE ||
		vk.render_pass.motion_blur == VK_NULL_HANDLE ||
		vk.framebuffers.motion_blur == VK_NULL_HANDLE ||
		vk.motion_blur_pipeline == VK_NULL_HANDLE ||
		vk.motion_blur_copy_pipeline == VK_NULL_HANDLE ) {
		R_MotionBlur_ResetView( &vk_motion_blur_view );
		return qfalse;
	}

	backEnd.doneMotionBlur = qtrue;
	if ( !R_MotionBlur_Calculate( &vk_motion_blur_view, qtrue,
		r_motionBlurStrength->value, ri.Milliseconds(),
		backEnd.refdef.vieworg, backEnd.refdef.viewaxis,
		backEnd.refdef.fov_x, backEnd.refdef.fov_y,
		glConfig.vidWidth, glConfig.vidHeight, constants ) ) {
		return qfalse;
	}
	if ( !R_MotionBlur_CalculateViewRect(
		glConfig.vidWidth, glConfig.vidHeight,
		backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight,
		viewRect, constants + 4 ) ) {
		return qfalse;
	}

	layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
	vk_end_render_pass();
	vk_begin_render_pass( vk.render_pass.motion_blur,
		vk.framebuffers.motion_blur, qfalse,
		glConfig.vidWidth, glConfig.vidHeight );

	qvkCmdBindPipeline( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, vk.motion_blur_pipeline );
	qvkCmdPushConstants( vk.cmd->command_buffer, layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( constants ), constants );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
		0, 1, &vk.color_descriptor, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	/* Resume the same raster/RT composition pass so later HUD and console
	 * batches stay sharp and a successful RT frame is never suppressed. */
	if ( resumePostBloom ) {
		vk_begin_post_bloom_render_pass();
	} else {
		vk_begin_main_render_pass_load();
	}
	constants[0] = constants[1] = 0.0f;
	qvkCmdBindPipeline( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, vk.motion_blur_copy_pipeline );
	qvkCmdPushConstants( vk.cmd->command_buffer, layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( constants ), constants );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
		0, 1, &vk.motion_blur_descriptor, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end =
		MIN( VK_DESC_COUNT, vk.maxBoundDescriptorSets ) - 1;
	Com_Memset( &vk.cmd->scissor_rect, 0xff,
		sizeof( vk.cmd->scissor_rect ) );
	return qtrue;
}


/*
====================
vk_draw_underwater

Composites the optional submerged view over the completed 3D scene: the whole
frame is resampled through an animated wave field with a small per-channel
dispersion, darkened toward the periphery, and absorbed toward the medium color
with eye distance.  It runs after global fog and before motion blur, bloom, and
gamma, so the layer belongs to the scene while later HUD and console drawing
stays sharp.

The effect draw renders into a private scratch attachment while sampling the
main color and the sampled scene depth, so it never reads the attachment it is
writing; a second draw with a zeroed warp, edge, and fog copies the result back
into whichever composition pass was active.  When depth sampling is unavailable
the medium density is uploaded as zero, which leaves the warp and the edge
falloff intact instead of dropping the whole layer.
====================
*/
static int vk_underwater_logged_state = -1;


qboolean vk_draw_underwater( void )
{
	const qboolean resumePostBloom =
		( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ? qtrue : qfalse;
	VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
	vec4_t phaseCoeffs[UNDERWATER_WAVE_OCTAVES];
	vec3_t dirAmp[UNDERWATER_WAVE_OCTAVES];
	vec3_t sceneColor;
	VkDescriptorSet sets[2];
	VkPipelineLayout layout;
	float constants[32];
	qboolean sceneLinear;
	qboolean depthReady;
	float strength;
	float warpScale;
	float warpPixels;
	float dispersion;
	float fogScale;
	float vignette;
	float waveTime;
	float density;
	float outputScale;
	float zNear;
	float zFar;
	float aspect;
	int contents;
	int i;

	if ( !vk_underwater_enabled() || !r_underwaterWarp ||
		!r_underwaterDispersion || !r_underwaterFog || !r_underwaterVignette ) {
		return qfalse;
	}
	if ( backEnd.doneUnderwater ) {
		return qfalse;
	}
	/* Recursive portal and mirror views carry their own camera, and a stereo
	 * pair would need one composite per eye.  The medium belongs to the eye, so
	 * the layer composites once, over the full primary view only. */
	if ( ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ||
		backEnd.viewParms.portalView != PV_NONE ||
		backEnd.viewParms.stereoFrame != STEREO_CENTER ||
		backEnd.viewParms.viewportX != 0 ||
		backEnd.viewParms.viewportY != 0 ||
		backEnd.viewParms.viewportWidth != glConfig.vidWidth ||
		backEnd.viewParms.viewportHeight != glConfig.vidHeight ) {
		return qfalse;
	}
	if ( vk.renderPassIndex != RENDER_PASS_MAIN &&
		vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return qfalse;
	}
	/* Deliberately no backEnd.doneSurfaces test here.  The layer composites from
	 * inside RB_DrawSurfs, right after the world pass and before that flag is
	 * raised for bloom, so it is still clear at this point; requiring it skipped
	 * the composite on every frame whose primary view was not preceded by
	 * another 3D pass.  vk_draw_global_fog and the OpenGL FBO_DrawUnderwater
	 * composite from the same place and do not test it either. */
	if ( ri.CL_IsMinimized() || glConfig.stereoEnabled ||
		vk.underwater_image == VK_NULL_HANDLE ||
		vk.underwater_descriptor == VK_NULL_HANDLE ||
		vk.render_pass.underwater == VK_NULL_HANDLE ||
		vk.framebuffers.underwater == VK_NULL_HANDLE ||
		vk.underwater_pipeline == VK_NULL_HANDLE ||
		vk.underwater_copy_pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	strength = backEnd.refdef.underwaterStrength;
	contents = backEnd.refdef.underwaterContents & UNDERWATER_CONTENTS_MASK;
	if ( strength <= 0.0f || !contents ) {
		return qfalse;
	}

	warpScale = Com_Clamp( 0.0f, UNDERWATER_WARP_SCALE_MAX, r_underwaterWarp->value ) *
		R_UnderwaterContentsWarpScale( contents );
	warpPixels = R_UnderwaterWarpPixels( warpScale, glConfig.vidHeight ) * strength;
	dispersion = R_UnderwaterDispersion( r_underwaterDispersion->value );
	fogScale = Com_Clamp( 0.0f, 1.0f, r_underwaterFog->value ) * strength;
	vignette = Com_Clamp( 0.0f, 1.0f, r_underwaterVignette->value ) * strength;
	if ( warpPixels <= 0.0f && fogScale <= 0.0f && vignette <= 0.0f ) {
		return qfalse;
	}

	zNear = r_znear ? r_znear->value : 4.0f;
	zFar = backEnd.viewParms.zFar;
	if ( zNear <= 0.0f || zFar <= zNear ) {
		return qfalse;
	}

	depthReady = ( vk_underwater_depth_enabled() &&
		vk.depth_sample_descriptor != VK_NULL_HANDLE ) ? qtrue : qfalse;
	density = depthReady ? R_UnderwaterContentsDensity( contents ) : 0.0f;

	/* Match the gamma pipeline's obScale/tone-map exposure so the authored
	 * medium color survives the output transform unchanged. */
	sceneLinear = vk_scene_linear_enabled();
	outputScale = (float)( 1 << tr.overbrightBits );
	if ( sceneLinear && r_tonemapExposure ) {
		outputScale *= Com_Clamp( 0.1f, 8.0f, r_tonemapExposure->value );
	}
	R_UnderwaterSceneColor( contents, outputScale, sceneLinear, sceneColor );

	backEnd.doneUnderwater = qtrue;

	/* The absorption drops out silently when the depth attachment cannot be
	 * sampled, and the live tuning cvars can zero the tint or the edge falloff
	 * on their own.  Report the resolved layer whenever that mix changes, so a
	 * composite that only warps says which of those it is. */
	if ( vk_underwater_logged_state != ( contents | ( depthReady ? 0x10000 : 0 ) ) ) {
		vk_underwater_logged_state = contents | ( depthReady ? 0x10000 : 0 );
		ri.Printf( PRINT_DEVELOPER,
			"Underwater view: RTX compositor active (contents 0x%x, density %.6f, depth %i,"
			" warp %.2f px, fog %.2f, edge %.2f)\n",
			contents, density, depthReady, warpPixels, fogScale, vignette );
	}

	waveTime = R_UnderwaterWaveTime( backEnd.refdef.floatTime );
	for ( i = 0; i < UNDERWATER_WAVE_OCTAVES; i++ ) {
		R_UnderwaterWaveOctave( i, phaseCoeffs[i], dirAmp[i] );
		/* The octave speed travels pre-multiplied by the wrapped scene time so
		 * the fragment stage evaluates a plain dot product plus a constant. */
		phaseCoeffs[i][3] *= waveTime;
	}
	aspect = glConfig.vidHeight > 0 ?
		(float)glConfig.vidWidth / (float)glConfig.vidHeight : 1.0f;

	Com_Memset( constants, 0, sizeof( constants ) );
	for ( i = 0; i < UNDERWATER_WAVE_OCTAVES; i++ ) {
		constants[0 + i] = phaseCoeffs[i][0] * aspect;
		constants[4 + i] = phaseCoeffs[i][1];
		constants[8 + i] = phaseCoeffs[i][3];
		constants[12 + i] = dirAmp[i][0];
		constants[16 + i] = dirAmp[i][1];
	}
	constants[15] = 1.0f + dispersion;
	constants[19] = 1.0f - dispersion;
	constants[20] = sceneColor[0];
	constants[21] = sceneColor[1];
	constants[22] = sceneColor[2];
	constants[23] = UNDERWATER_FOG_MAX_OPACITY * strength;
	constants[24] = density;
	constants[25] = zNear * zFar;
	constants[26] = zNear;
	constants[27] = zFar - zNear;
	constants[28] = warpPixels / (float)glConfig.vidWidth;
	constants[29] = warpPixels / (float)glConfig.vidHeight;
	constants[30] = vignette;
	constants[31] = fogScale;

	sets[0] = vk.color_descriptor;
	sets[1] = depthReady ? vk.depth_sample_descriptor : vk.color_descriptor;

	if ( glConfig.stencilBits > 0 ) {
		depthAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	layout = vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
	vk_end_render_pass();
	/* The scene depth attachment is sampled directly here rather than through a
	 * private copy, so it has to be moved out of its attachment layout for the
	 * duration of the composite and put back before the pass resumes. */
	if ( depthReady ) {
		record_image_layout_transition( vk.cmd->command_buffer, vk.depth_image,
			depthAspects,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );
	}
	vk_begin_render_pass( vk.render_pass.underwater, vk.framebuffers.underwater,
		qfalse, glConfig.vidWidth, glConfig.vidHeight );
	qvkCmdBindPipeline( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, vk.underwater_pipeline );
	qvkCmdPushConstants( vk.cmd->command_buffer, layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( constants ), constants );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
		0, ARRAY_LEN( sets ), sets, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( depthReady ) {
		record_image_layout_transition( vk.cmd->command_buffer, vk.depth_image,
			depthAspects,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}

	/* Resume the same raster/RT composition pass so later HUD and console
	 * batches stay sharp and a successful RT frame is never suppressed.  The
	 * copy zeroes the warp, the dispersion split, the edge falloff, and the
	 * absorption, so the same shader resolves to a straight passthrough rather
	 * than needing a second program. */
	if ( resumePostBloom ) {
		vk_begin_post_bloom_render_pass();
	} else {
		vk_begin_main_render_pass_load();
	}
	Com_Memset( constants, 0, sizeof( constants ) );
	constants[15] = 1.0f;
	constants[19] = 1.0f;
	constants[26] = 1.0f;
	sets[0] = vk.underwater_descriptor;
	qvkCmdBindPipeline( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, vk.underwater_copy_pipeline );
	qvkCmdPushConstants( vk.cmd->command_buffer, layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( constants ), constants );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
		0, ARRAY_LEN( sets ), sets, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );

	/* Direct post-process binds bypass the normal descriptor/pipeline caches. */
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end =
		MIN( VK_DESC_COUNT, vk.maxBoundDescriptorSets ) - 1;
	Com_Memset( &vk.cmd->scissor_rect, 0xff,
		sizeof( vk.cmd->scissor_rect ) );
	return qtrue;
}


/*
====================
vk_menu_blur

Replace the finished frame with a soft-focus copy of itself, so the in-game menu
drawn after this command is the only sharp thing on screen. See
renderercommon/tr_menu_blur.h for the sampling plan; this is its RTX backend and
it mirrors the Vulkan one.

The whole effect runs through one fragment shader. Its tap offset is a push
constant, so a zero offset turns the same pipeline into the plain bilinear
resample the two downsample steps and the composite want, and the pyramid needs
no copy shader of its own.
====================
*/
static void vk_menu_blur_decline( const char *reason )
{
	static const char *lastReason;

	if ( lastReason == reason ) {
		return;
	}
	lastReason = reason;
	// An in-game menu that stays sharp is indistinguishable from the effect
	// being switched off, so say why rather than failing silently.
	ri.Printf( PRINT_DEVELOPER, "Menu soft focus unavailable: %s\n", reason );
}


static void vk_menu_blur_draw( VkPipeline pipeline, VkDescriptorSet source,
	float offsetX, float offsetY, float alpha )
{
	const VkPipelineLayout layout =
		vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS );
	float constants[4];

	constants[0] = offsetX;
	constants[1] = offsetY;
	constants[2] = alpha;
	constants[3] = 0.0f;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	qvkCmdPushConstants( vk.cmd->command_buffer, layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( constants ), constants );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		layout, 0, 1, &source, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
}


static void vk_menu_blur_begin_level( uint32_t index )
{
	vk.renderWidth = vk.menu_blur_width[index];
	vk.renderHeight = vk.menu_blur_height[index];
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.menu_blur, vk.framebuffers.menu_blur[index],
		qfalse, vk.renderWidth, vk.renderHeight );
}


qboolean vk_menu_blur( float strength )
{
	menuBlurPlan_t plan;
	uint32_t i;

	if ( !R_MenuBlur_Plan( strength, glConfig.vidWidth, glConfig.vidHeight, &plan ) ) {
		return qfalse;
	}
	if ( !vk.fboActive ) {
		vk_menu_blur_decline( "the framebuffer post-processing path is not active" );
		return qfalse;
	}
	if ( ri.CL_IsMinimized() ) {
		return qfalse;
	}
	/* Test frame liveness, not vk.renderPassIndex alone.  That index is sticky
	 * state rather than a recording flag: vk_end_render_pass deliberately leaves
	 * it set, and vk_end_frame re-arms it to RENDER_PASS_MAIN *after*
	 * qvkEndCommandBuffer and qvkQueueSubmit.  A mid-client-frame drain - the
	 * one level of re-entrant SCR_UpdateScreen the client permits, or
	 * R_IssuePendingRenderCommands from the retained host font atlas - leaves
	 * the index reading MAIN with the frame already submitted, and ending a
	 * render pass on that dead command buffer faults the device inside the ICD.
	 *
	 * backEnd.doneSurfaces used to mask this by accident rather than by design:
	 * it is raised in RB_DrawSurfs and cleared only once the frame has been
	 * presented, so it doubled as "a frame is open".  Testing the real state
	 * instead is also what lets a frame with no 3D pass - the connection and
	 * level-loading screens - be softened safely.  This backend keeps no
	 * vk_current_render_pass tracker, so frame_count carries the liveness half
	 * on its own; every mid-frame end here is paired with a begin. */
	if ( vk.frame_count == 0 || vk.renderPassIndex != RENDER_PASS_MAIN ) {
		vk_menu_blur_decline( "no scene render pass is open on this frame" );
		return qfalse;
	}
	if ( vk.render_pass.menu_blur == VK_NULL_HANDLE ||
		vk.menu_blur_half_pipeline == VK_NULL_HANDLE ||
		vk.menu_blur_level_pipeline == VK_NULL_HANDLE ||
		vk.menu_blur_composite_pipeline == VK_NULL_HANDLE ) {
		vk_menu_blur_decline( "the soft-focus pipelines were not created" );
		return qfalse;
	}
	for ( i = 0; i < VK_NUM_MENU_BLUR_IMAGES; i++ ) {
		if ( vk.framebuffers.menu_blur[i] == VK_NULL_HANDLE ||
			vk.menu_blur_descriptor[i] == VK_NULL_HANDLE ) {
			vk_menu_blur_decline( "the soft-focus pyramid was not allocated" );
			return qfalse;
		}
	}
	/* The pipelines bake in their viewport, so a plan that no longer matches the
	 * allocated pyramid would render into the wrong part of the target. */
	if ( plan.halfWidth != (int)vk.menu_blur_width[0] ||
		plan.halfHeight != (int)vk.menu_blur_height[0] ||
		plan.levelWidth != (int)vk.menu_blur_width[1] ||
		plan.levelHeight != (int)vk.menu_blur_height[1] ) {
		vk_menu_blur_decline( "the soft-focus pyramid does not match the render target" );
		return qfalse;
	}

	vk_end_render_pass(); // end main

	/* Two exact halvings with the linear sampler, which averages the four
	 * source texels a 2:1 reduction lands between. One 4:1 step would keep one
	 * texel in four and make the backdrop crawl as the scene animates. */
	vk_menu_blur_begin_level( 0 );
	vk_menu_blur_draw( vk.menu_blur_half_pipeline, vk.color_descriptor, 0.0f, 0.0f, 1.0f );
	vk_end_render_pass();

	vk_menu_blur_begin_level( 1 );
	vk_menu_blur_draw( vk.menu_blur_level_pipeline, vk.menu_blur_descriptor[0], 0.0f, 0.0f, 1.0f );
	vk_end_render_pass();

	/* Separable iterations ping-pong between [1] and [2] and end back in [1]. */
	for ( i = 0; i < (uint32_t)plan.passCount; i++ ) {
		const float spacing = R_MenuBlur_TapSpacing( plan.passSigma[i],
			MENU_BLUR_KERNEL_VARIANCE_LINEAR3 ) * MENU_BLUR_LINEAR3_TAP;

		vk_menu_blur_begin_level( 2 );
		vk_menu_blur_draw( vk.menu_blur_level_pipeline, vk.menu_blur_descriptor[1],
			spacing / (float)plan.levelWidth, 0.0f, 1.0f );
		vk_end_render_pass();

		vk_menu_blur_begin_level( 1 );
		vk_menu_blur_draw( vk.menu_blur_level_pipeline, vk.menu_blur_descriptor[2],
			0.0f, spacing / (float)plan.levelHeight, 1.0f );
		vk_end_render_pass();
	}

	/* Composite back over the sharp frame. The upsample is the level texture's
	 * bilinear filter, which is all the reconstruction a blur this wide needs. */
	vk_begin_main_render_pass_load();
	vk_menu_blur_draw( vk.menu_blur_composite_pipeline, vk.menu_blur_descriptor[1],
		0.0f, 0.0f, plan.alpha );

	/* Direct post-process binds bypass the normal descriptor/pipeline caches.
	 * The null pipeline is load bearing: vk_menu_blur_draw binds descriptor set
	 * 0 through vk.pipeline_layout_post_process, and binding through a layout
	 * incompatible with vk.pipeline_layout disturbs the sets bound for it.
	 * Nulling the cache is what forces the next draw to rebind them. */
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end =
		MIN( VK_DESC_COUNT, vk.maxBoundDescriptorSets ) - 1;
	Com_Memset( &vk.cmd->scissor_rect, 0xff, sizeof( vk.cmd->scissor_rect ) );

	/* The same incompatibility leaves the 64-byte MVP push constant undefined,
	 * and unlike every other post-process detour in this file this one runs
	 * while backEnd.projection2D is already set - so the next RB_StretchPic
	 * skips RB_SetGL2D and nothing re-pushes it.  Every 2D draw after the
	 * composite would otherwise be transformed by undefined values. */
	vk_update_mvp( NULL );

	return qtrue;
}


qboolean vk_bloom( void )
{
	uint32_t i;

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
	{
		return qfalse;
	}

	if ( backEnd.doneBloom || !backEnd.doneSurfaces || !vk.fboActive )
	{
		return qfalse;
	}

	vk_end_render_pass(); // end main

	// bloom extraction
	vk_begin_bloom_extract_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_extract_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ), 0, 1, &vk.color_descriptor, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	for ( i = 0; i < VK_NUM_BLOOM_PASSES*2; i+=2 ) {
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ), 0, 1, &vk.bloom_image_descriptor[i+0], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ), 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
#if 0
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ), 0, 1, &vk.bloom_image_descriptor[i+2], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_POST_PROCESS ), 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
#endif
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ )
		{
			dset[i] = vk.bloom_image_descriptor[(i+1)*2];
		}

		// blend downscaled buffers to main fbo
		qvkCmdBindPipeline( vk.cmd->command_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			R_BloomProtectHighlightsActive() &&
				vk.bloom_blend_cel_pipeline != VK_NULL_HANDLE ?
					vk.bloom_blend_cel_pipeline : vk.bloom_blend_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout_handle( RTX_VK_PIPELINE_LAYOUT_BLEND ), 0, ARRAY_LEN(dset), dset, 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	}

	/*
	 * Bloom binds private pipeline layouts after the render-pass helper has
	 * already invalidated last_pipeline.  Force the next material/HUD draw to
	 * rebind every main-layout descriptor instead of attempting a partial,
	 * stale restoration here.
	 */
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end =
		MIN( VK_DESC_COUNT, vk.maxBoundDescriptorSets ) - 1;
	Com_Memset( &vk.cmd->scissor_rect, 0xff, sizeof( vk.cmd->scissor_rect ) );

	backEnd.doneBloom = qtrue;

	return qtrue;
}
