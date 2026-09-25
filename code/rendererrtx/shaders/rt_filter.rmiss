#version 460
#extension GL_EXT_ray_tracing : require

// Miss of a filter ray: it has passed every filtering surface on its segment,
// and its transmission is what their any-hits left.

rayPayloadInEXT vec3 filterTransmission;

void main()
{
}
