/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_NAVIGATION_HPP
#define FNTECH3_WOLFET_NAVIGATION_HPP
#include <cmath>
#include <cstdint>
#include <cstring>

namespace fntech3::wolfet::navigation {
inline bool NonnegativeInteger(float value, int &out) {
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	// Decode bounded binary32 values before casting, including under fast math.
	// Negative zero is a valid zero; NaN, infinities and integer overflow are not.
	if (bits == 0x80000000u) bits = 0;
	if (bits >= 0x4f000000u) return false;
	const unsigned exponent = bits >> 23;
	if (exponent < 127) { out = 0; return true; }
	const std::uint32_t mantissa = (bits & 0x7fffffu) | 0x800000u;
	out = static_cast<int>(exponent < 150 ? mantissa >> (150 - exponent)
		: mantissa << (exponent - 150));
	return true;
}
inline int CacheBudget(float kilobytes) {
	int value;
	// ET defaults to 16 MiB. Keep valid zero/fractional budgets and truncate
	// before converting KiB to bytes; invalid settings use the profile default.
	return NonnegativeInteger(kilobytes, value) && value <= 0x1fffff
		? value * 1024 : 16384 * 1024;
}
inline int FrameRoutingUpdates(float value) {
	int result;
	return NonnegativeInteger(value, result) ? result : 1000;
}
inline bool Coordinate(float value) {
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	const volatile auto magnitude = bits & 0x7fffffffu;
	// Match the accepted EAAS geometry domain, including negative zero.
	return magnitude <= 0x48000000u;
}
inline bool AreaTravelTime(const float *start, const float *end, bool crouch,
	std::uint16_t &out) {
	if (!start || !end) return false;
	for (unsigned axis = 0; axis < 3; ++axis)
		if (!Coordinate(start[axis]) || !Coordinate(end[axis])) return false;
	// Numeric observations of et.exe 0x0047d470 and its vector math. Preserve
	// binary32 intermediates and the double speed constants under fast math.
	volatile float x = start[0] - end[0], y = start[1] - end[1], z = start[2] - end[2];
	volatile float xx = x * x, yy = y * y, zz = z * z;
	volatile float sum = xx + yy;
	sum = sum + zz;
	volatile float distance = static_cast<float>(std::sqrt(static_cast<double>(sum)));
	// ET's cost ignores liquid state and ground steepness. Crouch depends only
	// on the absence of the normal-presence bit in the area record.
	volatile float cost = static_cast<float>(static_cast<double>(distance) * (crouch ? 1.3 : 0.33));
	auto integer = static_cast<std::uint32_t>(cost);
	volatile float fraction = cost - static_cast<float>(integer);
	// The original myftol uses nearest-even. Express the integer decision
	// explicitly so it is portable and does not inherit a host rounding call.
	if (fraction > 0.5f || (fraction == 0.5f && (integer & 1u))) ++integer;
	// Minimum one is applied before storing the low sixteen bits; wrapped zero
	// is a measured result rather than a missing/invalid cost.
	out = static_cast<std::uint16_t>(integer ? integer : 1);
	return true;
}
// Numeric observations of et.exe 0x00481483..0x00481588. These are ET's
// navigation bits, independent of Quake III's bridge and team restrictions.
constexpr std::uint32_t AreaAvoid = 0x10;
constexpr std::uint32_t AreaAvoidAxis = 0x800;
constexpr std::uint32_t AreaAvoidAllies = 0x1000;
constexpr std::uint32_t TravelAxis = 0x04000000;
constexpr std::uint32_t TravelAllies = 0x08000000;
inline std::uint16_t AvoidTime(std::uint16_t time, std::uint32_t areaFlags,
	std::uint32_t travelFlags) {
	unsigned penalty = 0;
	if (areaFlags & AreaAvoid) penalty = 1000;
	else if ((areaFlags & AreaAvoidAxis) && (travelFlags & TravelAxis)) penalty = 200;
	else if ((areaFlags & AreaAvoidAllies) && (travelFlags & TravelAllies)) penalty = 200;
	// The stored time and the comparison both use the low sixteen bits.
	return static_cast<std::uint16_t>(time + penalty);
}
} // namespace fntech3::wolfet::navigation
#endif
