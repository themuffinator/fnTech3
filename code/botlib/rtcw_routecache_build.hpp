/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_ROUTECACHE_BUILD_HPP
#define FNTECH3_RTCW_ROUTECACHE_BUILD_HPP

// Independently authored from original SP/MP generation observations. Their
// waypoint masks and visibility traversal differ; do not use ET's generator.
namespace fntech3::rtcw::navigation {
constexpr int BuildTravelFlags = 0x01380fbe; // RTCW default without jump pads.
constexpr int UseForRouting = 1024;

inline bool ValidRouteTrace(const bsp_trace_t &trace) {
	if (!routing::Finite(trace.fraction) || trace.fraction < 0 || trace.fraction > 1) return false;
	for (float coordinate : trace.endpos) if (!routing::Coordinate(coordinate)) return false;
	return true;
}
inline bool BuildRouteVisibility() {
	const int count = aasworld.numareas;
	const bool sp = Com_GameIsRTCWSP() != 0;
	AAS_FreeRTCWVisibility();
	aasworld.rtcwVisibility = botlib::ClearedAlloc<aas_rtcw_visibility_t>(count);
	aasworld.rtcwVisibilityOwnedRows = qtrue;
	aasworld.rtcwDecodedVisibility = botlib::ClearedAlloc<byte>(count);
	aasworld.rtcwWaypoints = botlib::ClearedAlloc<vec3_t>(count);
	// No local owning allocations cross trace callbacks, which can unwind the
	// engine frame without running C++ destructors.
	aasworld.rtcwVisibilityScratch = botlib::ClearedAlloc<byte>(static_cast<std::size_t>(count) * 3);
	auto *valid = aasworld.rtcwVisibilityScratch;
	auto *encoded = valid + count;
	vec3_t mins, maxs;
	AAS_PresenceTypeBoundingBox(PRESENCE_NORMAL, mins, maxs);
	const int mask = CONTENTS_SOLID | (sp ? CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP : 0);
	for (int area = 1; area < count; ++area) {
		if (!AAS_AreaReachability(area)) continue;
		vec3_t end; VectorCopy(aasworld.areas[area].center, end); end[2] -= 256;
		auto trace = AAS_Trace(aasworld.areas[area].center, mins, maxs, end, -1, mask);
		if (!ValidRouteTrace(trace)) return false;
		if (!trace.startsolid && trace.fraction < 1 && AAS_PointAreaNum(trace.endpos) == area) {
			VectorCopy(trace.endpos, aasworld.rtcwWaypoints[area]); valid[area] = 1;
		}
	}
	std::size_t bytes = 0;
	for (int from = 1; from < count; ++from) {
		if (!valid[from]) continue;
		auto *row = aasworld.rtcwDecodedVisibility;
		std::memset(row, 0, count); row[from] = 1;
		for (int to = 1; to < count; ++to) {
			if (!valid[to] || from == to) continue;
			// Original SP (also iortcw SP) never fills the reverse-result bits
			// except on the diagonal. Its generated lower triangle stays zero.
			// MP traces both directions and does not consult the PVS.
			if (sp && (to < from || !AAS_inPVS(aasworld.rtcwWaypoints[from], aasworld.rtcwWaypoints[to]))) continue;
			const auto trace = AAS_Trace(aasworld.rtcwWaypoints[from], nullptr, nullptr,
				aasworld.rtcwWaypoints[to], -1, CONTENTS_SOLID);
			if (!ValidRouteTrace(trace)) return false;
			row[to] = trace.fraction >= 1 ? 1 : 0;
		}
		std::size_t used = 0;
		for (int first = 0; first < count;) {
			int end = first + 1;
			while (end < count && end - first < 255 && row[end] == row[first]) ++end;
			encoded[used++] = row[first]; encoded[used++] = static_cast<byte>(end - first); first = end;
		}
		if (used > routecache::MaxFileBytes - bytes) return false;
		bytes += used;
		auto &stored = aasworld.rtcwVisibility[from];
		stored.data = botlib::Alloc<byte>(used); stored.bytes = static_cast<int>(used);
		std::memcpy(stored.data, encoded, used);
	}
	FreeMemory(aasworld.rtcwVisibilityScratch); aasworld.rtcwVisibilityScratch = nullptr;
	std::memset(aasworld.rtcwDecodedVisibility, 0, count);
	aasworld.rtcwRouteCacheLoaded = qtrue;
	return true;
}
inline void BuildRouteCaches() {
	for (int area = 1; area < aasworld.numareas; ++area) {
		if (!AAS_AreaReachability(area)) continue;
		auto &settings = aasworld.areasettings[area];
		for (int i = 0; i < settings.numreachableareas; ++i)
			if (routing::TravelFlag(aasworld.reachability[settings.firstreachablearea + i].traveltype) & BuildTravelFlags) {
				settings.areaflags |= UseForRouting; break;
			}
	}
	// Permit internal route queries without publishing AAS_Initialized to the
	// game while generation is incomplete. Teardown clears this on failure.
	aasworld.rtcwRoutingBuilding = qtrue;
	for (int goal = 1; goal < aasworld.numareas; ++goal) {
		if (!(aasworld.areasettings[goal].areaflags & UseForRouting)) continue;
		for (int from = 1; from < aasworld.numareas; ++from) {
			if (from == goal || !(aasworld.areasettings[from].areaflags & UseForRouting)) continue;
			AAS_AreaTravelTimeToGoalArea(from, aasworld.rtcwWaypoints[from], goal, BuildTravelFlags);
			aasworld.frameroutingupdates = 0;
		}
	}
	aasworld.rtcwRoutingBuilding = qfalse;
}
} // namespace fntech3::rtcw::navigation

static bool AAS_BuildRTCWRouteCache() {
	using namespace fntech3::rtcw::navigation;
	if (!BuildRouteVisibility()) return false;
	BuildRouteCaches();
	AAS_WriteRTCWRouteCache();
	return true;
}
#endif
