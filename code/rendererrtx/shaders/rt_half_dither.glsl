// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#ifndef RT_HALF_DITHER_GLSL
#define RT_HALF_DITHER_GLSL

// A still view's history is the average of up to thousands of samples, kept
// in half floats (an 11-bit significand). Each sample then moves it by less
// than a half float's step, and a store that rounds loses part of every move:
// rounding toward zero, which Vulkan allows a store to a half float, takes
// half a step off each frame's value, and rounding to the nearest drops the
// small moves of the common samples while keeping the rare large ones (a
// shadowed sample among lit ones). Either way the average drifts from the
// samples' own, further the longer it gets. Rounded here to a whole half
// float step, down or up with the chance that keeps its expected value, the
// stored average is exactly a half float, which the store keeps as it is,
// and stays the samples' average. hash is a fresh random word for the pixel
// and frame.
vec3 rt_half_dither(vec3 value, uint hash)
{
	uvec3 bits = uvec3(hash, hash * 0x9e3779b9u, hash * 0x85ebca6bu) >> 8u;
	vec3 chance = vec3(bits) * (1.0 / 16777216.0);
	vec3 v = max(value, vec3(0.0));
	// a half float's step at the value: 2^-10 of its power of two, and the
	// subnormals' 2^-24 below 2^-14
	vec3 step = exp2(floor(log2(max(v, vec3(6.103515625e-05)))) - 10.0);
	return floor(v / step + chance) * step;
}

#endif
