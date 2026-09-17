/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_CAST_HPP
#define FNTECH3_SERVER_RTCW_CAST_HPP
#include "../botlib/rtcw_navigation.h"
#include "../compat/rtcw_routing.hpp"
#include "../compat/rtcw_game_abi.h"
#include <array>
#include <cstring>

static bool SV_RTCWCastAvailable() {
	if (!Com_GameIsRTCW() || !gvm) return false;
	if (gvm->index != VM_GAME || !gvm->entryPoint || gvm->dllExports
		|| sizeof(void *) != sizeof(int32_t))
		Com_Error(ERR_DROP, "RTCW cast queries require a legacy x86 game module");
	return true;
}

static std::array<float, 3> SV_RTCWCastPosition(const void *source) {
	if (!source) Com_Error(ERR_DROP, "Missing RTCW cast position");
	std::array<float, 3> position;
	// Read bytes without assuming the caller's vector is aligned.
	std::memcpy(position.data(), source, sizeof(position));
	for (float value : position) {
		if (!fntech3::rtcw::routing::Coordinate(value))
			Com_Error(ERR_DROP, "Invalid RTCW cast position");
	}
	return position;
}

static void SV_RTCWCastEntities(int source, int destination) {
	// SP's botlib reserves 2,048 slots, but both original game modules expose
	// only 1,024 g_entities. Internal allocation capacity is not this ABI limit.
	constexpr int limit = 1 << FNTECH3_RTCW_GENTITYNUM_BITS;
	if (source < 0 || source >= limit || destination < 0 || destination >= limit)
		Com_Error(ERR_DROP, "RTCW cast entity outside game bounds");
}

static int SV_RTCWCastResult(intptr_t result) {
	if (result != qfalse && result != qtrue)
		Com_Error(ERR_DROP, "Invalid RTCW cast query result");
	return static_cast<int>(result);
}

static int SV_RTCWCastVisible(const float *source, int sourceEntity,
	const float *destination, int destinationEntity, int update) {
	if (!SV_RTCWCastAvailable()) return qfalse;
	SV_RTCWCastEntities(sourceEntity, destinationEntity);
	if (update != 0 && update != 1) Com_Error(ERR_DROP, "Invalid RTCW visibility update flag");
	auto from = SV_RTCWCastPosition(source);
	auto to = SV_RTCWCastPosition(destination);
	// The native module may write to its arguments; never expose botlib storage.
	return SV_RTCWCastResult(VM_Call(gvm, 5, AICAST_VISIBLEFROMPOS,
		static_cast<int32_t>(reinterpret_cast<intptr_t>(from.data())), sourceEntity,
		static_cast<int32_t>(reinterpret_cast<intptr_t>(to.data())), destinationEntity, update));
}

static int SV_RTCWCastAttack(int entity, int enemy, const float *position,
	int ducking, int allowHitWorld) {
	if (!SV_RTCWCastAvailable()) return qfalse;
	SV_RTCWCastEntities(entity, enemy);
	if ((ducking != 0 && ducking != 1) || (allowHitWorld != 0 && allowHitWorld != 1))
		Com_Error(ERR_DROP, "Invalid RTCW attack query flags");
	auto point = SV_RTCWCastPosition(position);
	return SV_RTCWCastResult(VM_Call(gvm, 5, AICAST_CHECKATTACKATPOS, entity, enemy,
		static_cast<int32_t>(reinterpret_cast<intptr_t>(point.data())), ducking, allowHitWorld));
}
#endif
