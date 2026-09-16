/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_BSP_QUERY_HPP
#define FNTECH3_RTCW_BSP_QUERY_HPP
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Original RTCW reads numeric BSP properties through a 128-byte string buffer.
// Vector components use the host C library's double scanner, including partial
// conversions. Keep that contract separate from Quake's token/Q_atof parser.
namespace fntech3::rtcw::bsp {
inline bool Float(double value, float &out) {
	std::uint64_t bits;
	static_assert(sizeof(bits) == sizeof(value));
	std::memcpy(&bits, &value, sizeof(bits));
	// The magnitude of FLT_MAX in IEEE binary64. This also rejects infinities
	// and NaNs without relying on floating-point comparisons under fast-math.
	if ((bits & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x47efffffe0000000)) return false;
	out = static_cast<float>(value);
	return true;
}
inline bool Vector(const char *text, float *out) {
	std::array<double, 3> scanned{};
	std::sscanf(text, "%lf %lf %lf", &scanned[0], &scanned[1], &scanned[2]);
	std::array<float, 3> converted{};
	for (unsigned i = 0; i < 3; ++i) if (!Float(scanned[i], converted[i])) return false;
	std::memcpy(out, converted.data(), sizeof(converted));
	return true;
}
inline bool Scalar(const char *text, float &out) {
	return Float(std::strtod(text, nullptr), out);
}
inline bool Integer(const char *text, int &out) {
	errno = 0;
	const auto value = std::strtoll(text, nullptr, 10);
	if (errno == ERANGE || value < INT32_MIN || value > INT32_MAX) return false;
	out = static_cast<int>(value);
	return true;
}
} // namespace fntech3::rtcw::bsp
#endif
