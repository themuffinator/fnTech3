/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_MIX_H
#define FNTECH3_AUDIO_MIX_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace fntech3::audio {
inline int AddMixSample(int current, std::int64_t sample) {
	return static_cast<int>(std::clamp(static_cast<std::int64_t>(current) + sample,
		static_cast<std::int64_t>((std::numeric_limits<int>::min)()),
		static_cast<std::int64_t>((std::numeric_limits<int>::max)())));
}

// Effects carry eight fractional bits until the final DMA conversion. Widen
// before multiplying: even a single full-volume voice at s_volume 2 overflows
// a signed 32-bit product. Preserve the original arithmetic rounding.
inline int MixPCM16(int current, int sample, int volume) {
	const std::int64_t product = static_cast<std::int64_t>(sample) * volume;
	const std::int64_t scaled = product >= 0 ? product / 256 : -((-product + 255) / 256);
	return AddMixSample(current, scaled);
}

inline int AddFractionalMixSample(int current, float sample) {
	if (!std::isfinite(sample)) return current;
	const double sum = static_cast<float>(current) + sample;
	return static_cast<int>(std::clamp(sum,
		static_cast<double>((std::numeric_limits<int>::min)()),
		static_cast<double>((std::numeric_limits<int>::max)())));
}
} // namespace fntech3::audio
#endif
