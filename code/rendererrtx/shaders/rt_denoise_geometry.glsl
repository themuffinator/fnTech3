// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#ifndef RT_DENOISE_GEOMETRY_GLSL
#define RT_DENOISE_GEOMETRY_GLSL

// Reciprocal view depth is affine across a projected plane. Use the smaller
// one-sided derivative so an adjacent foreground/background surface cannot
// turn a depth discontinuity into the receiver's slope.
float rt_depth_slope(float backward, float forward, bool hasBackward, bool hasForward)
{
	if (hasBackward && (!hasForward || abs(backward) < abs(forward))) return backward;
	return hasForward ? forward : 0.0;
}

// Return the residual in the same depth units as the existing edge stop.
// Raw abs(z-qz) mistakes perspective foreshortening for an object boundary,
// leaving isolated bright samples on floors and other grazing receivers.
float rt_depth_plane_distance(float z, float qz, float slopeOffset)
{
	if (!(z > 0.0) || !(qz > 0.0)) return 1e30;
	float predicted = 1.0 / z + slopeOffset;
	if (!(predicted > 0.0)) return 1e30;
	return abs(qz * predicted - 1.0) * z;
}

#endif
