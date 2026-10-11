// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#ifndef RT_SIGNAL_PACKING_GLSL
#define RT_SIGNAL_PACKING_GLSL

// Octahedral normals and screen-space motion need two signed normalized
// components, while view depth needs its full floating-point range. An
// RG32UI image preserves depth bit-for-bit and uses 8 bytes per pixel.
uvec4 rt_pack_geometry(vec2 direction, float depth)
{
	return uvec4(packSnorm2x16(direction), floatBitsToUint(depth), 0u, 0u);
}

vec4 rt_unpack_geometry(uvec2 packed)
{
	return vec4(unpackSnorm2x16(packed.x), uintBitsToFloat(packed.y), 0.0);
}

// Store mean and standard deviation in half precision. Storing the squared
// moment directly loses small variances when two large half floats cancel.
vec4 rt_pack_moments(vec4 moments)
{
	float varianceD = max(moments.y - moments.x * moments.x, 0.0);
	float varianceS = max(moments.w - moments.z * moments.z, 0.0);
	return vec4(moments.x, sqrt(varianceD), moments.z, sqrt(varianceS));
}

vec4 rt_unpack_moments(vec4 packed)
{
	return vec4(packed.x, packed.x * packed.x + packed.y * packed.y,
		packed.z, packed.z * packed.z + packed.w * packed.w);
}

#endif
