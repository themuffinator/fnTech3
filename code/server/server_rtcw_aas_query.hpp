/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_AAS_QUERY_HPP
#define FNTECH3_SERVER_RTCW_AAS_QUERY_HPP
#include "server_rtcw_args.hpp"
#include "../compat/rtcw_routing.hpp"
#include "../compat/rtcw_prediction.hpp"
#include "../botlib/rtcw_navigation.h"
#include <memory>
#include <vector>

static int SV_RTCWTraceAreas(const vm_t *vm, const intptr_t *args) {
	const int capacity = SV_RTCWInteger(args[5]);
	// RTCW's checked AAS8 loader limits routing to 16-bit area indices.
	// Bound caller-controlled scratch storage to that same graph capacity.
	if (capacity < 0 || capacity > 65535)
		Com_Error(ERR_DROP, "Invalid RTCW area trace capacity");
	if (!capacity) return 0;
	auto start = SV_RTCWVector(vm, args[1]);
	auto end = SV_RTCWVector(vm, args[2]);
	auto *areas = static_cast<byte *>(SV_RTCWPointer(vm, args[3], capacity * sizeof(int)));
	auto *points = args[4] ? static_cast<byte *>(SV_RTCWPointer(vm, args[4], capacity * sizeof(vec3_t))) : nullptr;
	int count;
	bool valid;
	{
		std::vector<int> areaValues(capacity);
		std::unique_ptr<vec3_t[]> pointValues(points ? new vec3_t[capacity]{} : nullptr);
		count = botlib_export->aas.AAS_TraceAreas(start.data(), end.data(),
			areaValues.data(), pointValues.get(), capacity);
		valid = count >= 0 && count <= capacity;
		for (int i = 0; valid && i < count; ++i) {
			valid = areaValues[i] > 0 && areaValues[i] < 65535
				&& (!points || SV_FiniteWorldVector(pointValues[i]));
		}
		if (valid) {
			// The original writes a zero sentinel even for an unloaded/solid world.
			// Preserve per-hit write order when the two output arrays overlap.
			const int zero = 0;
			std::memcpy(areas, &zero, sizeof(zero));
			for (int i = 0; i < count; ++i) {
				std::memcpy(areas + i * sizeof(int), &areaValues[i], sizeof(int));
				if (points) std::memcpy(points + i * sizeof(vec3_t), pointValues[i], sizeof(vec3_t));
			}
		}
	}
	// Release scratch allocations before the engine's nonlocal error exit.
	if (!valid) Com_Error(ERR_DROP, "Invalid RTCW area trace result");
	return count;
}

// These queries use scalars/vectors, not a Quake or RTCW module POD. Own the
// input vectors so native/QVM alignment and overlapping arguments are harmless.
static int SV_RTCWTacticalEntity(intptr_t value, bool allowForced = false) {
	const int number = SV_RTCWInteger(value);
	const int64_t decoded = number < 0 && allowForced ? -static_cast<int64_t>(number) - 1 : number;
	if (decoded < 0 || decoded >= (1 << FNTECH3_RTCW_GENTITYNUM_BITS))
		Com_Error(ERR_DROP, "RTCW tactical entity outside game bounds");
	return number;
}

static int SV_RTCWPredictMovement(const vm_t *vm, const intptr_t *args) {
	using namespace fntech3::rtcw::movement;
	void *output = SV_RTCWPointer(vm, args[1], sizeof(fntech3_rtcw_aas_clientmove_t));
	const int entity = SV_RTCWInteger(args[2]);
	if (entity < -1 || entity >= (1 << FNTECH3_RTCW_GENTITYNUM_BITS))
		Com_Error(ERR_DROP, "RTCW prediction entity outside game bounds");
	auto origin = SV_RTCWVector(vm, args[3]);
	const int presence = SV_RTCWInteger(args[4]);
	const int grounded = SV_RTCWInteger(args[5]);
	auto velocity = SV_RTCWVector(vm, args[6]);
	auto command = SV_RTCWVector(vm, args[7]);
	const int commandFrames = SV_RTCWInteger(args[8]);
	const int maximumFrames = SV_RTCWInteger(args[9]);
	const float frameTime = SV_RTCWFloat(args[10]);
	const int events = SV_RTCWInteger(args[11]);
	const int stopArea = SV_RTCWInteger(args[12]);
	const int visualize = SV_RTCWInteger(args[13]);
	if (!Position(origin.data()) || !Position(velocity.data()) || !Position(command.data())
		|| maximumFrames > MaxFrames || (grounded != 0 && grounded != 1) || (visualize != 0 && visualize != 1))
		Com_Error(ERR_DROP, "Invalid RTCW movement prediction arguments");
	aas_clientmove_t internal{};
	const int result = botlib_export->aas.AAS_PredictClientMovement(&internal, entity, origin.data(),
		presence, grounded, velocity.data(), command.data(), commandFrames, maximumFrames, frameTime,
		events, stopArea, visualize);
	fntech3_rtcw_aas_clientmove_t external;
	if ((result != 0 && result != 1) || !Result(internal, external))
		Com_Error(ERR_DROP, "Invalid RTCW movement prediction result");
	// The original clears the result before predicting, including a failed
	// search. Unlike hide/attack, its false result still publishes that record.
	std::memcpy(output, &external, sizeof(external));
	return result;
}

static bool SV_RTCWAASQueryCall(const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result) {
	using fntech3::rtcw::module::Call;
	if (!Com_GameIsRTCW()) return false;
	if (call == Call::BOTLIB_AAS_RT_SHOWROUTE) {
		// Both original release bodies are empty; their visualization exists
		// only under DEBUG. This import consumes no memory or navigation data.
		result = 0;
		return true;
	}
	if (call == Call::BOTLIB_AAS_GETROUTEFIRSTVISPOS && Com_GameProfile()->id != COM_GAME_RTCW_SP)
		return false;
	switch (call) {
	case Call::BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
	case Call::BOTLIB_AAS_POINT_AREA_NUM:
	case Call::BOTLIB_AAS_TRACE_AREAS:
	case Call::BOTLIB_AAS_POINT_CONTENTS:
	case Call::BOTLIB_AAS_AREA_REACHABILITY:
	case Call::BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
	case Call::BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
	case Call::BOTLIB_AAS_SWIMMING: break;
	case Call::BOTLIB_AAS_GETROUTEFIRSTVISPOS: break;
	case Call::BOTLIB_AAS_RT_GETHIDEPOS:
	case Call::BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE: break;
	default: return false;
	}
	if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
	if (call == Call::BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT) {
		result = SV_RTCWPredictMovement(vm, args);
	} else if (call == Call::BOTLIB_AAS_RT_GETHIDEPOS || call == Call::BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE) {
		vec3_t position{};
		void *output;
		if (call == Call::BOTLIB_AAS_RT_GETHIDEPOS) {
			auto source = SV_RTCWVector(vm, args[1]);
			const int entity = SV_RTCWTacticalEntity(args[2], Com_GameProfile()->id == COM_GAME_RTCW_SP);
			const int area = SV_RTCWInteger(args[3]);
			auto enemy = SV_RTCWVector(vm, args[4]);
			const int enemyEntity = SV_RTCWTacticalEntity(args[5]);
			const int enemyArea = SV_RTCWInteger(args[6]);
			output = SV_RTCWPointer(vm, args[7], sizeof(position));
			for (unsigned axis = 0; axis < 3; ++axis) {
				if (!fntech3::rtcw::routing::Coordinate(source[axis]) || !fntech3::rtcw::routing::Coordinate(enemy[axis]))
					Com_Error(ERR_DROP, "RTCW hide position exceeds navigation bounds");
			}
			result = AAS_RTCWHidePosition(source.data(), entity, area, enemy.data(), enemyEntity, enemyArea, position);
		} else {
			const int entity = SV_RTCWTacticalEntity(args[1]);
			const int rangeEntity = SV_RTCWTacticalEntity(args[2]);
			const int enemyEntity = SV_RTCWTacticalEntity(args[3]);
			const float range = SV_RTCWFloat(args[4]);
			const int flags = SV_RTCWInteger(args[5]);
			if (range < 0) Com_Error(ERR_DROP, "Invalid RTCW tactical range");
			output = SV_RTCWPointer(vm, args[6], sizeof(position));
			result = AAS_RTCWAttackPosition(entity, rangeEntity, enemyEntity, range, flags, position);
		}
		if (result) {
			for (float value : position) {
				if (!fntech3::rtcw::routing::Coordinate(value)) Com_Error(ERR_DROP, "Invalid RTCW tactical result");
			}
			std::memcpy(output, position, sizeof(position));
		}
	} else if (call == Call::BOTLIB_AAS_GETROUTEFIRSTVISPOS) {
		auto source = SV_RTCWVector(vm, args[1]);
		auto destination = SV_RTCWVector(vm, args[2]);
		const int flags = SV_RTCWInteger(args[3]);
		void *output = SV_RTCWPointer(vm, args[4], sizeof(vec3_t));
		for (unsigned axis = 0; axis < 3; ++axis) {
			if (!fntech3::rtcw::routing::Coordinate(source[axis])
				|| !fntech3::rtcw::routing::Coordinate(destination[axis]))
				Com_Error(ERR_DROP, "RTCW visibility query exceeds navigation bounds");
		}
		vec3_t position{};
		result = AAS_RTCWRouteFirstVisiblePosition(source.data(), destination.data(), flags, position);
		if (result) {
			for (float value : position) {
				if (!fntech3::rtcw::routing::Coordinate(value))
					Com_Error(ERR_DROP, "Invalid RTCW route visibility result");
			}
			std::memcpy(output, position, sizeof(position));
		}
	} else if (call == Call::BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA) {
		const int area = SV_RTCWInteger(args[1]);
		const int goal = SV_RTCWInteger(args[3]);
		const int flags = SV_RTCWInteger(args[4]);
		std::array<float, 3> origin{};
		if (args[2]) {
			origin = SV_RTCWVector(vm, args[2]);
			for (float value : origin) {
				if (!fntech3::rtcw::routing::Coordinate(value))
					Com_Error(ERR_DROP, "RTCW route origin exceeds navigation bounds");
			}
		}
		result = botlib_export->aas.AAS_AreaTravelTimeToGoalArea(area,
			args[2] ? origin.data() : nullptr, goal, flags);
	} else if (call == Call::BOTLIB_AAS_TRACE_AREAS) {
		result = SV_RTCWTraceAreas(vm, args);
	} else if (call == Call::BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX) {
		const int presence = SV_RTCWInteger(args[1]);
		void *mins = SV_RTCWPointer(vm, args[2], sizeof(vec3_t));
		void *maxs = SV_RTCWPointer(vm, args[3], sizeof(vec3_t));
		vec3_t lower, upper;
		// Published SP/MP and Quake share selectors 2/4 and the same bounds,
		// including the library's crouching fallback for unknown selectors.
		botlib_export->aas.AAS_PresenceTypeBoundingBox(presence, lower, upper);
		std::memcpy(mins, lower, sizeof(lower));
		std::memcpy(maxs, upper, sizeof(upper));
		result = 0;
	} else if (call == Call::BOTLIB_AAS_AREA_REACHABILITY) {
		result = botlib_export->aas.AAS_AreaReachability(SV_RTCWInteger(args[1]));
	} else {
		auto point = SV_RTCWVector(vm, args[1]);
		if (call == Call::BOTLIB_AAS_POINT_AREA_NUM)
			result = botlib_export->aas.AAS_PointAreaNum(point.data());
		else if (call == Call::BOTLIB_AAS_POINT_CONTENTS)
			result = botlib_export->aas.AAS_PointContents(point.data());
		else result = botlib_export->aas.AAS_Swimming(point.data());
	}
	return true;
}
#endif
