/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_MOVE_GOAL_HPP
#define FNTECH3_SERVER_RTCW_MOVE_GOAL_HPP
#include "server_rtcw_args.hpp"
#include "server_world_bounds.hpp"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../compat/rtcw_prediction.hpp"

static_assert(sizeof(fntech3_rtcw_bot_moveresult_t) == 52, "RTCW movement-result ABI");

static bool SV_RTCWMoveToGoalCall(const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result) {
	using fntech3::rtcw::module::Call;
	if (!Com_GameIsRTCW() || call != Call::BOTLIB_AI_MOVE_TO_GOAL) return false;
	if (!botlib_export || !botlib_export->ai.BotMoveToGoal)
		Com_Error(ERR_DROP, "RTCW movement-to-goal interface is unavailable");
	const int state = SV_RTCWInteger(args[2]), flags = SV_RTCWInteger(args[4]);
	void *destination = SV_RTCWPointer(vm, args[1], sizeof(fntech3_rtcw_bot_moveresult_t));
	fntech3_rtcw_bot_moveresult_t before;
	std::memcpy(&before, destination, sizeof(before));
	bot_goal_t goal{};
	if (args[3]) {
		fntech3_rtcw_bot_goal_t input;
		std::memcpy(&input, SV_RTCWPointer(vm, args[3], sizeof(input)), sizeof(input));
		if (!fntech3::rtcw::movement::Position(input.origin) || input.areanum < 0 || input.areanum >= 65535)
			Com_Error(ERR_DROP, "Invalid RTCW movement-to-goal goal");
		std::memcpy(goal.origin, input.origin, sizeof(goal.origin));
		goal.areanum = input.areanum;
		// RTCW's no-slow-approach flag occupies Quake's dropped-item bit.
		goal.flags = (input.flags & 4) ? GFL_NOSLOWAPPROACH : 0;
	}
	bot_moveresult_t movement{};
	// The released entry point initializes only six integer fields. Preserve
	// caller bytes in the optional fields on paths which do not write them.
	movement.weapon = before.weapon;
	std::memcpy(movement.movedir, before.movedir, sizeof(movement.movedir));
	std::memcpy(movement.ideal_viewangles, before.ideal_viewangles, sizeof(movement.ideal_viewangles));
	botlib_export->ai.BotMoveToGoal(&movement, state, args[3] ? &goal : nullptr, flags);
	if ((movement.failure != 0 && movement.failure != 1) || (movement.blocked != 0 && movement.blocked != 1)
		|| movement.blockentity < 0 || movement.blockentity >= (1 << FNTECH3_RTCW_GENTITYNUM_BITS)
		|| (movement.flags & ~(255 | MOVERESULT_FUTUREVIEW))
		|| (movement.type != 0 && movement.type != RESULTTYPE_ELEVATORUP && movement.type != RESULTTYPE_INVISIBLEGRAPPLE)
		|| (std::memcmp(movement.movedir, before.movedir, sizeof(movement.movedir)) && !SV_FiniteWorldVector(movement.movedir))
		|| (std::memcmp(movement.ideal_viewangles, before.ideal_viewangles, sizeof(movement.ideal_viewangles))
			&& !SV_FiniteWorldVector(movement.ideal_viewangles)))
		Com_Error(ERR_DROP, "Invalid RTCW movement-to-goal result");
	fntech3_rtcw_bot_moveresult_t output;
	output.failure = movement.failure;
	output.type = movement.type == RESULTTYPE_INVISIBLEGRAPPLE ? 2 : movement.type;
	output.blocked = movement.blocked;
	output.blockentity = movement.blockentity;
	output.traveltype = movement.traveltype;
	output.flags = (movement.flags & 255) | ((movement.flags & MOVERESULT_FUTUREVIEW) ? 256 : 0);
	output.weapon = movement.weapon;
	std::memcpy(output.movedir, movement.movedir, sizeof(output.movedir));
	std::memcpy(output.ideal_viewangles, movement.ideal_viewangles, sizeof(output.ideal_viewangles));
	std::memcpy(destination, &output, sizeof(output));
	result = 0;
	return true;
}
#endif
