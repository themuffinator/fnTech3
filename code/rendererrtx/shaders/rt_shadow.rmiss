#version 460
#extension GL_EXT_ray_tracing : require

struct RtRadiancePayload {
	vec3 color;
	float hitT;
	vec3 normal;
	uint flags;
	float shadowFactor;
	float shadowResponse;
	vec3 shadowLoss;
	vec3 nextOrigin;
	vec3 nextDirection;
	vec3 nextWeight;
	uint pathDepth;
};

layout(location = 1) rayPayloadInEXT RtRadiancePayload payloadShadow;

void main()
{
	payloadShadow.shadowFactor = 1.0;
}
