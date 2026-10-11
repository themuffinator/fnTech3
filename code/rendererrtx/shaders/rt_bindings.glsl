// Descriptor set 0 of the path tracer's ray-tracing pipeline, or of its
// compute backend (RT_SOFTWARE_TRACE), which traces BVHs of its own in place
// of the acceleration structures. Binding numbers mirror rtBinding_t in
// code/rendererrtx/rt/rt_gpu.h.

#ifndef RT_BINDINGS_GLSL
#define RT_BINDINGS_GLSL

#ifdef RT_SOFTWARE_TRACE
// A node's bmin.w holds its first child or triangle and bmax.w its triangle
// count (0 for an interior node), as uint bits (rtBvhNode_t).
struct RtBvhNode {
	vec4 bmin;
	vec4 bmax;
};

// A leaf triangle's corners; v0.w holds its index in the world or dynamic
// index buffer, as uint bits (rtBvhTri_t).
struct RtBvhTri {
	vec4 v0;
	vec4 v1;
	vec4 v2;
};

layout(set = 0, binding = 0, std430) readonly buffer RtWorldBvh {
	RtBvhNode nodes[];
} u_worldBvh;

layout(set = 0, binding = 22, std430) readonly buffer RtWorldBvhTris {
	RtBvhTri tris[];
} u_worldTris;

layout(set = 0, binding = 23, std430) readonly buffer RtDynamicBvh {
	RtBvhNode nodes[];
} u_dynamicBvh;

layout(set = 0, binding = 24, std430) readonly buffer RtDynamicBvhTris {
	RtBvhTri tris[];
} u_dynamicTris;
#else
layout(set = 0, binding = 0) uniform accelerationStructureEXT u_tlas;
#endif

layout(set = 0, binding = 1, std430) readonly buffer RtWorldVertices {
	RtVertex vertices[];
} u_worldVertices;

layout(set = 0, binding = 2, std430) readonly buffer RtWorldIndices {
	uint indices[];
} u_worldIndices;

layout(set = 0, binding = 3, std430) readonly buffer RtDynamicVertices {
	RtVertex vertices[];
} u_dynamicVertices;

layout(set = 0, binding = 4, std430) readonly buffer RtDynamicIndices {
	uint indices[];
} u_dynamicIndices;

layout(set = 0, binding = 5, std430) readonly buffer RtMaterials {
	RtMaterial materials[];
} u_materials;

layout(set = 0, binding = 6, std430) readonly buffer RtStages {
	RtStage stages[];
} u_stages;

layout(set = 0, binding = 7, std430) readonly buffer RtLights {
	RtLight lights[];
} u_lights;

// Per light-grid cell: first index into u_lightIndices and the count.
layout(set = 0, binding = 8, std430) readonly buffer RtLightGrid {
	uvec2 cells[];
} u_lightGrid;

layout(set = 0, binding = 9, std430) readonly buffer RtLightIndices {
	uint indices[];
} u_lightIndices;

layout(set = 0, binding = 10, std140) uniform RtFrameBlock {
	RtFrameData u_frame;
};

layout(set = 0, binding = 11) uniform sampler2D u_textures[RT_MAX_TEXTURES];

// Each dynamic vertex's world position last frame; w is 0 for a vertex whose
// entity was not there (rt/rt_backend.c RT_WritePreviousPositions).
layout(set = 0, binding = 20, std430) readonly buffer RtDynamicPrevious {
	vec4 positions[];
} u_dynamicPrevious;

// The triangle light each world triangle emits as, by its index in the world
// index buffer, ~0 for none (rt/rt_lights.c RT_MapPrimitiveLights).
layout(set = 0, binding = 21, std430) readonly buffer RtPrimitiveLights {
	uint lights[];
} u_primitiveLights;

#endif
