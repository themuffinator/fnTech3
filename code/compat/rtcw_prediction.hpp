/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_PREDICTION_HPP
#define FNTECH3_RTCW_PREDICTION_HPP
#include "../qcommon/q_shared.h"
#include "../botlib/be_aas.h"
#include "rtcw_game_abi.h"
#include "rtcw_routing.hpp"
#include <cstddef>

namespace fntech3::rtcw::movement {
// Engine work limit, not a number defined by the external RTCW ABI.
inline constexpr int MaxFrames = 4096;
inline constexpr int StopEvents = 0x07ff;

inline bool Position(const float *point) {
	return point && routing::Coordinate(point[0]) && routing::Coordinate(point[1])
		&& routing::Coordinate(point[2]);
}

inline bool Result(const aas_clientmove_t &value, fntech3_rtcw_aas_clientmove_t &out) {
	static_assert(sizeof(out) == 80 && alignof(fntech3_rtcw_aas_clientmove_t) == 4);
	static_assert(sizeof(fntech3_rtcw_aas_trace_t) == 36 && offsetof(fntech3_rtcw_aas_clientmove_t, trace) == 24);
	static_assert(offsetof(fntech3_rtcw_aas_clientmove_t, endcontents) == 68);
	if (!Position(value.endpos) || !Position(value.trace.endpos)
		|| !routing::Finite(value.velocity[0]) || !routing::Finite(value.velocity[1]) || !routing::Finite(value.velocity[2])
		|| !routing::Finite(value.trace.fraction) || value.trace.fraction < 0 || value.trace.fraction > 1
		|| (value.trace.startsolid != 0 && value.trace.startsolid != 1)
		|| !routing::Finite(value.time) || value.time < 0 || value.frames < 0 || value.frames > MaxFrames
		|| (value.stopevent & ~StopEvents)) return false;
	out = {};
	VectorCopy(value.endpos, out.endpos);
	VectorCopy(value.velocity, out.velocity);
	out.trace.startsolid = value.trace.startsolid;
	out.trace.fraction = value.trace.fraction;
	VectorCopy(value.trace.endpos, out.trace.endpos);
	out.trace.ent = value.trace.ent;
	out.trace.lastarea = value.trace.lastarea;
	out.trace.area = value.trace.area;
	out.trace.planenum = value.trace.planenum;
	out.presencetype = value.presencetype;
	out.stopevent = value.stopevent;
	out.endcontents = static_cast<float>(value.endcontents);
	out.time = value.time;
	out.frames = value.frames;
	return true;
}
} // namespace fntech3::rtcw::movement
#endif
