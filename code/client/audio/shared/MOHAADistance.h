/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_MOHAA_DISTANCE_H
#define FNTECH3_AUDIO_MOHAA_DISTANCE_H

namespace fntech3::audio::mohaa {

/* How far a Medal of Honor sound carries: the min and max distances of the
 * alias that chose it (soundparms' fifth and sixth numbers; 1.00's 6,443
 * aliases all give a min above zero and a max no nearer, from 160/1,600 for
 * a footstep to 10,000/10,000 for a line meant to be heard everywhere). A
 * distance below zero is the game's "none": 200 units of full volume, and
 * silence 64 times that far (OpenMoHAA snd_openal_new.cpp S_OPENAL_StartSound
 * and S_OPENAL_AddLoopSounds [O]). */
struct Span {
	float full = 200.0f;
	float silent = 12800.0f;
};

inline Span Resolve( float minDist, float maxDist ) noexcept {
	Span span;
	span.full = minDist >= 0.0f ? minDist : 200.0f;
	span.silent = maxDist >= 0.0f ? maxDist : span.full * 64.0f;
	return span;
}

/* A sound starts only inside its max distance (S_OPENAL_ShouldStart [O]:
 * the max squared above the distance squared), and a loop is silent from
 * there on. */
inline bool Audible( float distance, const Span &span ) noexcept {
	return distance < span.silent;
}

/* The share of a sound's volume heard `distance` away: all of it inside the
 * min distance, none from the max, and a straight line between (the 2D
 * channels' law in S_OPENAL_Respatialize, and what the 3D channels' linear
 * clamped model gives [O]). */
inline float Gain( float distance, const Span &span ) noexcept {
	if ( distance <= span.full ) {
		return 1.0f;
	}
	if ( distance >= span.silent ) {
		return 0.0f;
	}
	return 1.0f - ( distance - span.full ) / ( span.silent - span.full );
}

}
#endif
