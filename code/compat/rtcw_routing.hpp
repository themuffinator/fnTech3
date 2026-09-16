/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_ROUTING_HPP
#define FNTECH3_RTCW_ROUTING_HPP
#include <cmath>
#include <cstdint>
#include <cstring>

namespace fntech3::rtcw::routing {
constexpr int DoNotEnterLarge = 0x02000000;
inline int TravelFlag(std::uint32_t type) {
	if (type >= 2 && type <= 6) return 1 << (type - 1);
	if (type >= 7 && type <= 18) return 1 << type;
	return type == 19 ? 0x01000000 : 1;
}
inline int ContentsFlags(std::uint32_t contents) {
	// Original RTCW returns immediately for liquids, before entry restrictions.
	if (contents & 1) return 0x00100000;
	if (contents & 4) return 0x00200000;
	if (contents & 2) return 0x00400000;
	return 0x00080000 | ((contents & 256) ? 0x00800000 : 0)
		| ((contents & 1024) ? DoNotEnterLarge : 0);
}
inline std::uint32_t Bits(float value) {
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}
inline bool Finite(float value) { return (Bits(value) & 0x7f800000u) != 0x7f800000u; }
inline bool Coordinate(float value) { return (Bits(value) & 0x7fffffffu) <= 0x48000000u; }
inline int CacheBudget(float kilobytes) {
	// Decode a bounded positive binary32 integer without an unsafe float cast
	// in legacy fast-math builds. The setting is truncated, as in RTCW.
	const auto bits = Bits(kilobytes);
	if (bits < 0x3f800000u || bits > 0x49800000u) return 4096 * 1024;
	return static_cast<int>(((bits & 0x7fffffu) | 0x800000u) >> (150 - (bits >> 23))) * 1024;
}
inline bool AreaTravelTime(const float *start, const float *end, float steepness,
	bool crouch, bool swim, bool singlePlayer, std::uint16_t &out) {
	if (!start || !end || !Finite(steepness) || steepness < 0 || steepness > 1) return false;
	for (unsigned axis = 0; axis < 3; ++axis)
		if (!Coordinate(start[axis]) || !Coordinate(end[axis])) return false;
	// Preserve each binary32 rounding point and the original double-precision
	// speed constants even when the enclosing legacy build uses fast math.
	volatile float x = start[0] - end[0], y = start[1] - end[1], z = start[2] - end[2];
	volatile float xx = x * x, yy = y * y, zz = z * z;
	volatile float sum = xx + yy;
	sum = sum + zz;
	volatile float distance = static_cast<float>(std::sqrt(static_cast<double>(sum)));
	volatile float slope = steepness * 19.0f;
	volatile float scale = static_cast<float>(1.0 + static_cast<double>(slope));
	volatile float scaled = distance * scale;
	volatile float cost = static_cast<float>(static_cast<double>(scaled) * (crouch ? 1.3 : swim ? 1.0 : 0.33));
	// SP rounds upward; MP truncates. This is a profile contract, not a cvar.
	const auto rounded = static_cast<std::uint32_t>(singlePlayer ? std::ceil(static_cast<double>(cost)) : cost);
	// The validated coordinates bound this conversion below INT32_MAX. Preserve
	// RTCW's unsigned-short wrap, including a wrapped zero at exact multiples.
	out = static_cast<std::uint16_t>(rounded ? rounded : 1);
	return true;
}
} // namespace fntech3::rtcw::routing
#endif
