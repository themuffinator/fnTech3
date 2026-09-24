/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_COLLISION_HPP
#define FNTECH3_Q3_IHV_COLLISION_HPP

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace fntech3::q3ihv {

struct LinkBounds {
	std::uint16_t solid = 0;
	float mins[3]{}, maxs[3]{};
};

struct SweepBounds {
	float mins[3]{}, maxs[3]{};
};

inline bool FiniteVector(const float *value) {
	if (!value) return false;
	for (int i = 0; i < 3; ++i) {
		std::uint32_t bits; std::memcpy(&bits, value + i, sizeof(bits));
		if ((bits & 0x7f800000u) == 0x7f800000u) return false;
	}
	return true;
}

/* Original SV_Trace 0x41c1f1..0x41c24c adds the start/end, extent and one-unit
 * query pad before storing a float. Keep its x87 PC53 rounding order in SSE
 * and fast-math builds too. Invalid/overflowing bounds leave output intact. */
inline bool CalculateSweepBounds(const float *start, const float *mins,
	const float *maxs, const float *end, SweepBounds &output) {
	if (!FiniteVector(start) || !FiniteVector(mins) || !FiniteVector(maxs) || !FiniteVector(end)) return false;
	SweepBounds result;
	for (int axis = 0; axis < 3; ++axis) {
		if (mins[axis] > maxs[axis]) return false;
		const bool forward = end[axis] > start[axis];
		volatile double lowSum = static_cast<double>(forward ? start[axis] : end[axis]) + mins[axis];
		volatile double highSum = static_cast<double>(forward ? end[axis] : start[axis]) + maxs[axis];
		const double low = lowSum - 1.0, high = highSum + 1.0;
		constexpr double limit = (std::numeric_limits<float>::max)();
		if (low < -limit || low > limit || high < -limit || high > limit) return false;
		result.mins[axis] = static_cast<float>(low);
		result.maxs[axis] = static_cast<float>(high);
	}
	output = result;
	return true;
}

/* SV_LinkEntity 0x41b9d3 uses _ftol's signed 64-bit result, then only EAX.
 * Preserve that truncation/wrap before clamping; a direct float-to-int cast
 * is undefined outside int's range. Original invalid int64 conversion has
 * low word zero. Callers validate finite input before doing arithmetic. */
inline int SolidComponent(double value, int maximum) {
	std::uint32_t low = 0;
	if (value >= -9223372036854775808.0 && value < 9223372036854775808.0)
		low = static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
	std::int32_t truncated; std::memcpy(&truncated, &low, sizeof(truncated));
	return truncated < 1 ? 1 : truncated > maximum ? maximum : truncated;
}

/* IHV's 16-bit solid is x | down<<5 | up<<10, with eight-unit quantization;
 * 31 is the brush-model sentinel. Contents other than the two excluded bits
 * (0x04000000, 0x40000000) make a box solid, unlike retail Q3's solid/body test.
 * Original observations: 0x41b970, constants 0x45b4b8/0x45b4bc, and 0x414360.
 * The chosen origin/angles are supplied by the native boundary: svFlags 0x80
 * selects currentOrigin/currentAngles, otherwise state.origin/state.angles.
 * This computes link geometry only, not BSP leaves, PVS or collision traces.
 * Invalid bounds or nonfinite/overflowing results leave output unchanged. */
inline bool CalculateLinkBounds(const float *mins, const float *maxs,
	const float *origin, const float *angles, bool brushModel, std::uint32_t contents,
	LinkBounds &output) {
	if (!FiniteVector(mins) || !FiniteVector(maxs) || !FiniteVector(origin) || !FiniteVector(angles)) return false;
	for (int i = 0; i < 3; ++i) if (mins[i] > maxs[i]) return false;
	LinkBounds result;
	if (brushModel) result.solid = 31;
	else if (contents & 0xbbffffffu) {
		const int x = SolidComponent(static_cast<double>(maxs[0]) * 0.125, 31);
		const int down = SolidComponent(-static_cast<double>(mins[2]) * 0.125, 31);
		const int up = SolidComponent((static_cast<double>(maxs[2]) + 32.0) * 0.125, 63);
		result.solid = static_cast<std::uint16_t>(x | (down << 5) | (up << 10));
	}
	const bool rotated = brushModel && (angles[0] != 0.0f || angles[1] != 0.0f || angles[2] != 0.0f);
	double radius = 0;
	if (rotated) {
		double extent[3];
		for (int i = 0; i < 3; ++i) {
			const double a = std::fabs(static_cast<double>(mins[i])), b = std::fabs(static_cast<double>(maxs[i]));
			extent[i] = a > b ? a : b;
		}
		// Preserve the original sum order even in the engine's fast-math build.
		volatile double xy = extent[0] * extent[0] + extent[1] * extent[1];
		radius = std::sqrt(xy + extent[2] * extent[2]);
	}
	for (int i = 0; i < 3; ++i) {
		// The original stores binary32 bounds before applying its one-unit pad.
		const double a = static_cast<double>(origin[i]) + (rotated ? -radius : mins[i]);
		const double b = static_cast<double>(origin[i]) + (rotated ? radius : maxs[i]);
		constexpr double limit = (std::numeric_limits<float>::max)();
		if (a < -limit || a > limit || b < -limit || b > limit) return false;
		volatile float low = static_cast<float>(a);
		volatile float high = static_cast<float>(b);
		result.mins[i] = low - 1.0f;
		result.maxs[i] = high + 1.0f;
	}
	if (!FiniteVector(result.mins) || !FiniteVector(result.maxs)) return false;
	output = result; return true;
}

} // namespace fntech3::q3ihv
#endif
