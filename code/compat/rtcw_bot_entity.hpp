/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_BOT_ENTITY_HPP
#define FNTECH3_RTCW_BOT_ENTITY_HPP
#include "../qcommon/q_shared.h"
#include "../botlib/botlib.h"
#include "rtcw_game_abi.h"
#include <cstdint>
#include <cstring>
#include <limits>

namespace fntech3::rtcw::bot {
inline bool FiniteVector(const float *v) {
	for (unsigned axis = 0; axis < 3; ++axis) {
		std::uint32_t bits;
		std::memcpy(&bits, v + axis, sizeof(bits));
		if ((bits & 0x7f800000u) == 0x7f800000u) return false;
	}
	return true;
}

// Check translation before it can overflow a binary32 AAS bounding box.
inline bool ValidBounds(const float *origin, const float *mins, const float *maxs) {
	if (!FiniteVector(origin) || !FiniteVector(mins) || !FiniteVector(maxs)) return false;
	constexpr double limit = std::numeric_limits<float>::max();
	for (unsigned axis = 0; axis < 3; ++axis) {
		if (mins[axis] > maxs[axis]) return false;
		const double lower = static_cast<double>(origin[axis]) + mins[axis];
		const double upper = static_cast<double>(origin[axis]) + maxs[axis];
		if (lower < -limit || upper > limit) return false;
	}
	return true;
}

inline bool EntityState(const fntech3_rtcw_bot_entitystate_t &in, bot_entitystate_t &out) {
	static_assert(sizeof(in) == 112 && alignof(fntech3_rtcw_bot_entitystate_t) == 4);
	if (!FiniteVector(in.origin) || !FiniteVector(in.angles) || !FiniteVector(in.old_origin)
		|| !FiniteVector(in.mins) || !FiniteVector(in.maxs)) return false;
	// SOLID_BBOX is 2 in both published game ABIs. BSP bounds come from the map.
	if (in.solid == 2 && !ValidBounds(in.origin, in.mins, in.maxs)) return false;
	out = {};
	out.type = in.type; out.flags = in.flags;
	VectorCopy(in.origin, out.origin); VectorCopy(in.angles, out.angles);
	VectorCopy(in.old_origin, out.old_origin);
	VectorCopy(in.mins, out.mins); VectorCopy(in.maxs, out.maxs);
	out.groundent = in.groundent; out.solid = in.solid;
	out.modelindex = in.modelindex; out.modelindex2 = in.modelindex2;
	out.frame = in.frame; out.event = in.event; out.eventParm = in.eventParm;
	out.powerups = in.powerups; out.weapon = in.weapon;
	out.legsAnim = in.legsAnim; out.torsoAnim = in.torsoAnim;
	return true;
}
} // namespace fntech3::rtcw::bot
#endif
