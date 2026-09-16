/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_BOT_ACTIONS_HPP
#define FNTECH3_RTCW_BOT_ACTIONS_HPP
#include "rtcw_bot_entity.hpp"

namespace fntech3::rtcw::bot {
// Bit order is the frozen RTCW ACTION_* contract, including jump/up and
// crouch/down aliases. Unknown bits survive a reset/get round trip separately.
inline constexpr unsigned ActionBits[] = {
	ACTION_ATTACK, ACTION_USE, ACTION_RESPAWN, ACTION_JUMP, ACTION_CROUCH,
	ACTION_MOVEFORWARD, ACTION_MOVEBACK, ACTION_MOVELEFT, ACTION_MOVERIGHT,
	ACTION_DELAYEDJUMP, ACTION_TALK, ACTION_GESTURE, ACTION_WALK, ACTION_RTCW_RELOAD
};
inline constexpr unsigned KnownActionBits = 0x3fffu;

inline bool ActionInput(const fntech3_rtcw_bot_input_t &in, bot_input_t &out) {
	static_assert(sizeof(in) == 40 && alignof(fntech3_rtcw_bot_input_t) == 4);
	std::uint32_t time, speed;
	std::memcpy(&time, &in.thinktime, sizeof(time));
	std::memcpy(&speed, &in.speed, sizeof(speed));
	if ((time & 0x7f800000u) == 0x7f800000u || (speed & 0x7f800000u) == 0x7f800000u
		|| !FiniteVector(in.dir) || !FiniteVector(in.viewangles)) return false;
	out = {};
	out.thinktime = in.thinktime; out.speed = in.speed; out.weapon = in.weapon;
	VectorCopy(in.dir, out.dir); VectorCopy(in.viewangles, out.viewangles);
	for (unsigned bit = 0; bit < 14; ++bit)
		if (static_cast<unsigned>(in.actionflags) & (1u << bit)) out.actionflags |= ActionBits[bit];
	return true;
}

inline fntech3_rtcw_bot_input_t ActionOutput(const bot_input_t &in, unsigned extraFlags) {
	fntech3_rtcw_bot_input_t out{};
	out.thinktime = in.thinktime; out.speed = in.speed; out.weapon = in.weapon;
	VectorCopy(in.dir, out.dir); VectorCopy(in.viewangles, out.viewangles);
	unsigned flags = extraFlags & ~KnownActionBits;
	for (unsigned bit = 0; bit < 14; ++bit)
		if (static_cast<unsigned>(in.actionflags) & ActionBits[bit]) flags |= 1u << bit;
	std::memcpy(&out.actionflags, &flags, sizeof(flags));
	return out;
}
} // namespace fntech3::rtcw::bot
#endif
