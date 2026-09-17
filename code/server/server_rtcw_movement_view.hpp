/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_MOVEMENT_VIEW_HPP
#define FNTECH3_SERVER_RTCW_MOVEMENT_VIEW_HPP
#include "server_rtcw_args.hpp"
#include "../botlib/be_ai_goal.h"
#include "../compat/rtcw_prediction.hpp"

static_assert(sizeof(fntech3_rtcw_bot_goal_t) == 56, "RTCW goal ABI");

static bot_goal_t SV_RTCWViewGoal(const vm_t *vm, intptr_t address, bool withEntity) {
	fntech3_rtcw_bot_goal_t input;
	std::memcpy(&input, SV_RTCWPointer(vm, address, sizeof(input)), sizeof(input));
	if (!fntech3::rtcw::movement::Position(input.origin) || input.areanum < 0 || input.areanum >= 65535
		|| (withEntity && (input.entitynum < -1 || input.entitynum >= (1 << FNTECH3_RTCW_GENTITYNUM_BITS))))
		Com_Error(ERR_DROP, "Invalid RTCW movement-view goal");
	// Project only fields read by these view queries. In particular, RTCW flag
	// bit 4 must never be interpreted as Quake's dropped-item flag. Unused bounds
	// and metadata need not be initialized by a game making a view-only query.
	bot_goal_t output{};
	std::memcpy(output.origin, input.origin, sizeof(output.origin));
	output.areanum = input.areanum;
	if (withEntity) output.entitynum = input.entitynum;
	return output;
}

static bool SV_RTCWMovementViewCall(const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result) {
	using fntech3::rtcw::module::Call;
	if (!Com_GameIsRTCW() || (call != Call::BOTLIB_AI_MOVEMENT_VIEW_TARGET
		&& call != Call::BOTLIB_AI_PREDICT_VISIBLE_POSITION)) return false;
	if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
	const bool visible = call == Call::BOTLIB_AI_PREDICT_VISIBLE_POSITION;
	const intptr_t goalAddress = args[visible ? 3 : 2];
	bot_goal_t goal{};
	if (goalAddress) goal = SV_RTCWViewGoal(vm, goalAddress, visible);
	void *destination = SV_RTCWPointer(vm, args[5], sizeof(vec3_t));
	std::array<float, 3> target, before;
	std::memcpy(before.data(), destination, sizeof(vec3_t));
	std::memcpy(target.data(), before.data(), sizeof(vec3_t));
	if (visible) {
		auto origin = SV_RTCWVector(vm, args[1]);
		const int area = SV_RTCWInteger(args[2]), flags = SV_RTCWInteger(args[4]);
		if (!fntech3::rtcw::movement::Position(origin.data()) || area < 0 || area >= 65535)
			Com_Error(ERR_DROP, "Invalid RTCW visible-position arguments");
		result = botlib_export->ai.BotPredictVisiblePosition(origin.data(), area,
			goalAddress ? &goal : nullptr, flags, target.data());
	} else {
		const int state = SV_RTCWInteger(args[1]), flags = SV_RTCWInteger(args[3]);
		const float lookahead = SV_RTCWFloat(args[4]);
		if (!fntech3::rtcw::routing::Coordinate(lookahead))
			Com_Error(ERR_DROP, "Invalid RTCW movement-view lookahead");
		result = botlib_export->ai.BotMovementViewTarget(state, goalAddress ? &goal : nullptr,
			flags, lookahead, target.data());
	}
	if ((result != 0 && result != 1) || ((result || std::memcmp(target.data(), before.data(), sizeof(vec3_t)))
		&& !fntech3::rtcw::movement::Position(target.data())))
		Com_Error(ERR_DROP, "Invalid RTCW movement-view result");
	// Original view targeting can publish a partial target even on failure.
	// An untouched output may contain arbitrary caller bytes; preserve those.
	std::memcpy(destination, target.data(), sizeof(vec3_t));
	return true;
}
#endif
