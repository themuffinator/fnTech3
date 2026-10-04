/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_IEEE_DELTA_FLOAT_HPP
#define FNTECH3_IEEE_DELTA_FLOAT_HPP
#include <cstdint>

namespace fntech3::delta {
/* The 13-bit delta form represents exact binary32 integers in [-4096,4095].
 * Inspect the transmitted word directly: fast-math must not collapse NaNs,
 * infinities or subnormals before the raw-word fallback. Both signed zeros
 * use the compact zero representation, matching the format's float test. */
inline constexpr bool CompactFloatInteger(std::uint32_t word, int &integer) noexcept {
    const auto magnitude = word & 0x7fffffffu;
    if (!magnitude) { integer = 0; return true; }
    const auto exponent = magnitude >> 23;
    if (exponent < 127 || exponent > 139) return false;
    const auto fractionalBits = 23 - (exponent - 127);
    const auto significand = (magnitude & 0x007fffffu) | 0x00800000u;
    if (significand & ((1u << fractionalBits) - 1)) return false;
    const auto absolute = static_cast<int>(significand >> fractionalBits);
    const auto result = (word & 0x80000000u) ? -absolute : absolute;
    if (result < -4096 || result >= 4096) return false;
    integer = result;
    return true;
}
} // namespace fntech3::delta
#endif
