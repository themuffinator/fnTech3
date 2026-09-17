/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_MOVEMENT_HPP
#define FNTECH3_RTCW_MOVEMENT_HPP
#include "../compat/rtcw_prediction.hpp"
#include "../qcommon/game_profile.h"
#include <cmath>

namespace fntech3::rtcw::movement {
inline float Normalize(float *vector) {
	volatile float x = vector[0] * vector[0], y = vector[1] * vector[1], z = vector[2] * vector[2];
	volatile float square = x + y;
	square = square + z;
	const float length = static_cast<float>(std::sqrt(static_cast<double>(square)));
	if (length) {
		volatile float reciprocal = 1.0f / length;
		for (int axis = 0; axis < 3; ++axis) vector[axis] *= reciprocal;
	}
	return length;
}

inline void Settings(aas_settings_t &settings, bool sp) {
	settings = {};
	settings.phys_gravitydirection[2] = -1;
	settings.phys_friction = 6;
	settings.phys_stopspeed = 100;
	settings.phys_gravity = 800;
	settings.phys_waterfriction = 1;
	settings.phys_watergravity = 400;
	settings.phys_maxvelocity = 320;
	settings.phys_maxwalkvelocity = 300;
	settings.phys_maxcrouchvelocity = 100;
	settings.phys_maxswimvelocity = 150;
	settings.phys_walkaccelerate = 10;
	settings.phys_airaccelerate = 1;
	settings.phys_swimaccelerate = 4;
	settings.phys_maxstep = 18;
	settings.phys_maxsteepness = 0.7f;
	settings.phys_maxwaterjump = 17;
	settings.phys_jumpvel = 270;
	volatile float ascent = settings.phys_jumpvel / settings.phys_gravity;
	settings.phys_maxbarrier = sp ? 49.0f : static_cast<float>(0.5 * settings.phys_gravity * ascent * ascent - 0.8);
}
} // namespace fntech3::rtcw::movement
#endif
