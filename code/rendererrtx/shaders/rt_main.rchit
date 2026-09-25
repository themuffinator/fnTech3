#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

// Radiance closest hit: record which triangle the ray reached. Ray
// generation fetches the triangle, evaluates its material and lights it.

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT RtHitPayload payload;
hitAttributeEXT vec2 bary;

void main()
{
	payload.hitT = gl_HitTEXT;
	payload.customIndex = gl_InstanceCustomIndexEXT;
	payload.primitive = uint(gl_PrimitiveID);
	payload.geometry = uint(gl_GeometryIndexEXT) |
		(gl_HitKindEXT == gl_HitKindFrontFacingTriangleEXT ? RT_HIT_FRONT_FACING : 0u);
	payload.bary = bary;
}
