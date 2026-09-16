/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_BOT_AI_HPP
#define FNTECH3_RTCW_BOT_AI_HPP
#include "rtcw_bot_entity.hpp"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"

namespace fntech3::rtcw::bot {
inline bool MoveState(const fntech3_rtcw_bot_initmove_t &in, int clients, bot_initmove_t &out) {
	static_assert(sizeof(in) == 68 && alignof(fntech3_rtcw_bot_initmove_t) == 4);
	std::uint32_t timeBits;
	std::memcpy(&timeBits, &in.thinktime, sizeof(timeBits));
	if ((timeBits & 0x7f800000u) == 0x7f800000u || !FiniteVector(in.origin)
		|| !FiniteVector(in.velocity) || !FiniteVector(in.viewoffset) || !FiniteVector(in.viewangles)
		|| in.entitynum < 0 || in.entitynum >= (1 << FNTECH3_RTCW_GENTITYNUM_BITS)
		|| in.client < 0 || in.client >= clients) return false;
	out = {};
	VectorCopy(in.origin, out.origin); VectorCopy(in.velocity, out.velocity);
	VectorCopy(in.viewoffset, out.viewoffset); VectorCopy(in.viewangles, out.viewangles);
	out.entitynum = in.entitynum; out.client = in.client;
	out.thinktime = in.thinktime; out.presencetype = in.presencetype;
	// Only these input bits replace persistent movement state in original RTCW.
	if (in.or_moveflags & FNTECH3_RTCW_MFL_ONGROUND) out.or_moveflags |= MFL_ONGROUND;
	if (in.or_moveflags & FNTECH3_RTCW_MFL_WATERJUMP) out.or_moveflags |= MFL_WATERJUMP;
	if (in.or_moveflags & FNTECH3_RTCW_MFL_TELEPORTED) out.or_moveflags |= MFL_TELEPORTED;
	if (in.or_moveflags & FNTECH3_RTCW_MFL_WALK) out.or_moveflags |= MFL_WALK;
	return true;
}
} // namespace fntech3::rtcw::bot
#endif
