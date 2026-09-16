/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_AAS_QUERY_HPP
#define FNTECH3_SERVER_RTCW_AAS_QUERY_HPP
#include "server_rtcw_args.hpp"
#include "../compat/rtcw_routing.hpp"
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
static bool SV_RTCWAASQueryCall(const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result) {
	using fntech3::rtcw::module::Call;
	if (!Com_GameIsRTCW()) return false;
	switch (call) {
	case Call::BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
	case Call::BOTLIB_AAS_POINT_AREA_NUM:
	case Call::BOTLIB_AAS_TRACE_AREAS:
	case Call::BOTLIB_AAS_POINT_CONTENTS:
	case Call::BOTLIB_AAS_AREA_REACHABILITY:
	case Call::BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
	case Call::BOTLIB_AAS_SWIMMING: break;
	default: return false;
	}
	if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
	if (call == Call::BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA) {
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
