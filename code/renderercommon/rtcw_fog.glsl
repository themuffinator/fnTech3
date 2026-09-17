// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
// RGB-only fog, applied before the authored material blend operation.
float rtcwFogVisibility(vec4 color, vec4 params, float distance)
{
	if (color.w == 0.0) return 1.0;
	if (color.w == 2.0) return exp(-params.z * abs(distance));
	if (params.y == params.x) return distance < params.y ? 1.0 : 0.0;
	return clamp((params.y - distance) / (params.y - params.x), 0.0, 1.0);
}
