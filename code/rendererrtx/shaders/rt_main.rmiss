#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

// Radiance miss: the ray left the world. Ray generation supplies the sky.

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT RtHitPayload payload;

void main()
{
	payload.hitT = -1.0;
}
