#version 460
#extension GL_EXT_ray_tracing : require

// Shadow miss: nothing blocked the path to the light.

layout(location = 1) rayPayloadInEXT float shadowVisible;

void main()
{
	shadowVisible = 1.0;
}
