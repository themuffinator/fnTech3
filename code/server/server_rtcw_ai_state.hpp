/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_AI_STATE_HPP
#define FNTECH3_SERVER_RTCW_AI_STATE_HPP
#include "server_rtcw_args.hpp"
#include "../compat/rtcw_bot_ai.hpp"

static bool SV_RTCWAIStateCall(const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result) {
	using fntech3::rtcw::module::Call;
	if (!Com_GameIsRTCW()) return false;
	// Select the service before reading arguments or requiring a live interface.
	switch (call) {
	case Call::BOTLIB_AI_ALLOC_GOAL_STATE: case Call::BOTLIB_AI_FREE_GOAL_STATE:
	case Call::BOTLIB_AI_RESET_GOAL_STATE: case Call::BOTLIB_AI_RESET_AVOID_GOALS:
	case Call::BOTLIB_AI_ALLOC_MOVE_STATE: case Call::BOTLIB_AI_FREE_MOVE_STATE:
	case Call::BOTLIB_AI_RESET_MOVE_STATE: case Call::BOTLIB_AI_INIT_MOVE_STATE:
	case Call::BOTLIB_AI_RESET_AVOID_REACH: case Call::BOTLIB_AI_RESET_LAST_AVOID_REACH:
	case Call::BOTLIB_AI_INIT_AVOID_REACH: break;
	default: return false;
	}
	if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
	auto &ai = botlib_export->ai;
	if (call == Call::BOTLIB_AI_ALLOC_MOVE_STATE) {
		result = ai.BotAllocMoveState(); return true;
	}
	const int argument = SV_RTCWInteger(args[1]);
	if (call == Call::BOTLIB_AI_ALLOC_GOAL_STATE) {
		if (argument < 0 || argument >= Com_GameProfile()->maxClients)
			Com_Error(ERR_DROP, "RTCW goal-state client outside profile");
		result = ai.BotAllocGoalState(argument); return true;
	}
	// Handle validity and diagnostics belong to the real state pool. Zero,
	// exhausted, stale and out-of-range handles retain its nonfatal behavior.
	switch (call) {
	case Call::BOTLIB_AI_FREE_GOAL_STATE: ai.BotFreeGoalState(argument); break;
	case Call::BOTLIB_AI_RESET_GOAL_STATE: ai.BotResetGoalState(argument); break;
	case Call::BOTLIB_AI_RESET_AVOID_GOALS: ai.BotResetAvoidGoals(argument); break;
	case Call::BOTLIB_AI_FREE_MOVE_STATE: ai.BotFreeMoveState(argument); break;
	case Call::BOTLIB_AI_RESET_MOVE_STATE: ai.BotResetMoveState(argument); break;
	case Call::BOTLIB_AI_RESET_AVOID_REACH: ai.BotResetAvoidReach(argument); break;
	case Call::BOTLIB_AI_INIT_AVOID_REACH: BotInitAvoidReach(argument); break;
	case Call::BOTLIB_AI_RESET_LAST_AVOID_REACH: ai.BotResetLastAvoidReach(argument); break;
	case Call::BOTLIB_AI_INIT_MOVE_STATE: {
		fntech3_rtcw_bot_initmove_t input;
		std::memcpy(&input, SV_RTCWPointer(vm, args[2], sizeof(input)), sizeof(input));
		bot_initmove_t converted;
		if (!fntech3::rtcw::bot::MoveState(input, Com_GameProfile()->maxClients, converted))
			Com_Error(ERR_DROP, "Invalid RTCW movement state");
		ai.BotInitMoveState(argument, &converted); break;
	}
	default: return false;
	}
	result = 0; return true;
}
#endif
