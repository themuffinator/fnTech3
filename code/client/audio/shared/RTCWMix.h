/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_RTCW_MIX_H
#define FNTECH3_AUDIO_RTCW_MIX_H
#include <algorithm>
#include <cmath>
#include <limits>

namespace fntech3::audio::rtcw {
inline float PlaybackGain(float value) {
	return std::isfinite(value) ? std::clamp(value, 0.0f, 16.0f) : 0.0f;
}

// SP truncates loop volume at submission using the last serviced master fade.
inline int LoopVolume(int volume, float master) {
	return static_cast<int>(static_cast<float>(std::clamp(volume, 0, 255)) * PlaybackGain(master));
}

// Effects use a second, independently quantized fade during mixing. Above
// unity only streams and the submitted loop volume amplify, not this stage.
inline int EffectsMixVolume(float volume, float master) {
	const int base = static_cast<int>(std::clamp(PlaybackGain(volume), 0.0f, 2.0f) * 256.0f);
	return static_cast<int>(static_cast<float>(base) * (std::min)(PlaybackGain(master), 1.0f));
}

inline int ScaleStreamSample(int sample, float gain) {
	return static_cast<int>(std::clamp(static_cast<double>(sample) * PlaybackGain(gain),
		static_cast<double>((std::numeric_limits<int>::min)()),
		static_cast<double>((std::numeric_limits<int>::max)())));
}
} // namespace fntech3::audio::rtcw
#endif
