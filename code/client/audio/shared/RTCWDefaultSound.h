/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_RTCW_DEFAULT_SOUND_H
#define FNTECH3_AUDIO_RTCW_DEFAULT_SOUND_H
#include <vector>

namespace fntech3::audio::rtcw {
constexpr const char *DefaultSoundName = "***DEFAULT***";

// Original RTCW reserves handle zero independently of installed assets. SP's
// silent placeholder has eight frames; MP always keeps a 512-frame placeholder.
inline std::vector<short> DefaultSoundPCM( bool singlePlayer, bool audible ) {
	std::vector<short> samples(singlePlayer && !audible ? 8 : 512, 0);
	if ( audible ) for ( std::size_t i = 0; i < samples.size(); ++i ) samples[i] = static_cast<short>(i);
	return samples;
}
}
#endif
