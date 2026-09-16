/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_ROUTING_INIT_HPP
#define FNTECH3_RTCW_ROUTING_INIT_HPP

// Included by be_aas_route.cpp. These are engine-owned tables, never module PODs.
namespace fntech3::rtcw::navigation {
constexpr std::size_t MaxRoutingBytes = 256 * 1024 * 1024;
inline bool AddRoutingBytes(std::size_t &bytes, std::size_t count, std::size_t stride) {
	if (count > (MaxRoutingBytes - bytes) / stride) return false;
	bytes += count * stride;
	return true;
}
inline bool RoutingGeometry(std::size_t &links) {
	if (aasworld.numareas < 2 || aasworld.numareas > 65535 || aasworld.numclusters < 2
		|| aasworld.numareasettings != aasworld.numareas || !aasworld.rtcwGroundSteepness
		|| !aasworld.areasettings || !aasworld.clusters || !aasworld.portals
		|| aasworld.numportals < 1 || aasworld.portals[0].areanum != 0) return false;
	links = 0;
	for (int area = 0; area < aasworld.numareas; ++area) {
		const auto &settings = aasworld.areasettings[area];
		if (settings.numreachableareas < 0 || settings.numreachableareas > 256
			|| settings.firstreachablearea < 0 || settings.firstreachablearea > aasworld.reachabilitysize
			|| settings.numreachableareas > aasworld.reachabilitysize - settings.firstreachablearea) return false;
		// RTCW skips area zero when reversing links, but reserves its cost rows.
		// Retail dam_b0 contains a nonempty span in that reserved area.
		if (area) links += settings.numreachableareas;
		for (int i = 0; i < settings.numreachableareas; ++i) {
			const auto &reach = aasworld.reachability[settings.firstreachablearea + i];
			if (reach.areanum < 0 || reach.areanum >= aasworld.numareas) return false;
		}
	}
	std::size_t bytes = 0;
	return AddRoutingBytes(bytes, aasworld.numareas, sizeof(aas_reversedreachability_t))
		&& AddRoutingBytes(bytes, links, sizeof(aas_reversedlink_t));
}
inline void ReversedRouting(std::size_t links) {
	const auto prefix = aasworld.numareas * sizeof(aas_reversedreachability_t);
	aasworld.reversedreachability = botlib::ClearedAllocBytes<aas_reversedreachability_t>(
		prefix + links * sizeof(aas_reversedlink_t));
	auto *next = reinterpret_cast<aas_reversedlink_t *>(reinterpret_cast<byte *>(aasworld.reversedreachability) + prefix);
	for (int area = 1; area < aasworld.numareas; ++area) {
		const auto &settings = aasworld.areasettings[area];
		for (int i = 0; i < settings.numreachableareas; ++i) {
			const int index = settings.firstreachablearea + i;
			auto &destination = aasworld.reversedreachability[aasworld.reachability[index].areanum];
			*next = {index, area, destination.first};
			destination.first = next++; ++destination.numlinks;
		}
	}
}
inline bool RoutingCosts(std::size_t links) {
	std::size_t bytes = 0;
	const auto rowCount = links + aasworld.areasettings[0].numreachableareas;
	if (!AddRoutingBytes(bytes, aasworld.numareas, sizeof(unsigned short **))
		|| !AddRoutingBytes(bytes, rowCount, sizeof(unsigned short *))) return false;
	for (int area = 0; area < aasworld.numareas; ++area) {
		const auto samples = static_cast<std::size_t>(aasworld.areasettings[area].numreachableareas)
			* aasworld.reversedreachability[area].numlinks;
		if (!AddRoutingBytes(bytes, samples, sizeof(unsigned short))) return false;
	}
	aasworld.areatraveltimes = botlib::ClearedAllocBytes<unsigned short **>(bytes);
	auto **rows = reinterpret_cast<unsigned short **>(aasworld.areatraveltimes + aasworld.numareas);
	auto *samples = reinterpret_cast<unsigned short *>(rows + rowCount);
	for (int area = 0; area < aasworld.numareas; ++area) {
		const auto &settings = aasworld.areasettings[area];
		aasworld.areatraveltimes[area] = rows;
		for (int outgoing = 0; outgoing < settings.numreachableareas; ++outgoing) {
			*rows++ = samples;
			const auto &reach = aasworld.reachability[settings.firstreachablearea + outgoing];
			for (auto *incoming = aasworld.reversedreachability[area].first; incoming; incoming = incoming->next) {
				std::uint16_t cost;
				if (!routing::AreaTravelTime(aasworld.reachability[incoming->linknum].end, reach.start,
					aasworld.rtcwGroundSteepness[area], !(settings.presencetype & PRESENCE_NORMAL),
					(settings.areaflags & AREA_LIQUID) != 0, Com_GameIsRTCWSP() != 0, cost)) return false;
				*samples++ = cost;
			}
		}
	}
	return true;
}
} // namespace fntech3::rtcw::navigation

bool AAS_InitRTCWRouting(void) {
	using namespace fntech3::rtcw;
	std::size_t links = 0;
	if (!Com_GameIsRTCW() || !aasworld.loaded || !navigation::RoutingGeometry(links)) {
		botimport.Print(PRT_ERROR, "RTCW routing requires valid precompiled navigation: %s\n", aasworld.mapname);
		return false;
	}
	AAS_FreeRoutingCaches();
	// This also validates the map's cluster/portal topology before graph access.
	if (!AAS_ReadRTCWRouteCache()) {
		botimport.Print(PRT_ERROR, "RTCW routing cache missing or invalid for %s; cache generation is not implemented\n", aasworld.mapname);
		return false;
	}
	AAS_InitTravelFlagFromType();
	AAS_InitAreaContentsTravelFlags();
	// RTCW's hide-position searches address these records by global area number.
	aasworld.areaupdate = botlib::ClearedAlloc<aas_routingupdate_t>(aasworld.numareas);
	aasworld.portalupdate = botlib::ClearedAlloc<aas_routingupdate_t>(aasworld.numportals + 1);
	navigation::ReversedRouting(links);
	if (!navigation::RoutingCosts(links)) {
		botimport.Print(PRT_ERROR, "Invalid or oversized RTCW area travel costs: %s\n", aasworld.mapname);
		return false;
	}
	AAS_InitPortalMaxTravelTimes();
	aasworld.max_routingcachesize = routing::CacheBudget(LibVarValue("max_routingcache", "4096"));
	aasworld.frameroutingupdates = 0;
#ifdef ROUTING_DEBUG
	numareacacheupdates = numportalcacheupdates = 0;
#endif
	return true;
}
#endif
