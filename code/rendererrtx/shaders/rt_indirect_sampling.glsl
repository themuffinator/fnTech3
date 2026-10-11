// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#ifndef RT_INDIRECT_SAMPLING_GLSL
#define RT_INDIRECT_SAMPLING_GLSL

// One sample per divisor-squared pixel block; rotate its phase every frame.
// Alpha carries inverse density, zero explicitly means no observation.
// A checkerboard changes only after a full phase cycle, otherwise some
// pixels would be skipped forever when the divisor is even.
float rt_indirect_density(uint x, uint y, uint frame, uint divisor, bool checker)
{
	uint count = divisor * divisor;
	uint phase = frame % count;
	if (x % divisor != phase % divisor || y % divisor != phase / divisor) return 0.0;
	if (checker && (((x / 8u) + (y / 8u) + frame / count) & 1u) != 0u) return 0.0;
	return float(count) * (checker ? 2.0 : 1.0);
}

#endif
