/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_ROUTECACHE_LOADER_HPP
#define FNTECH3_RTCW_ROUTECACHE_LOADER_HPP
#include "../compat/rtcw_routecache_format.hpp"
#include <new>

void AAS_FreeRTCWVisibility(void);

// Included by be_aas_route.cpp after its cache allocation/link helpers.
namespace fntech3::rtcw::navigation {
class RouteTopology {
	const aas_t &world_;
public:
	explicit RouteTopology(const aas_t &world) : world_(world) {}
	std::size_t AreaCount() const { return world_.numareas; }
	std::size_t ClusterCount() const { return world_.numclusters; }
	std::size_t PortalCount() const { return world_.numportals; }
	routecache::Area GetArea(std::size_t i) const {
		const auto &area = world_.areasettings[i];
		return {area.cluster, area.clusterareanum, area.numreachableareas};
	}
	routecache::Cluster GetCluster(std::size_t i) const {
		const auto &cluster = world_.clusters[i];
		return {cluster.numareas, cluster.numreachabilityareas};
	}
	routecache::Portal GetPortal(std::size_t i) const {
		const auto &portal = world_.portals[i];
		return {portal.areanum, {portal.frontcluster, portal.backcluster},
			{portal.clusterareanum[0], portal.clusterareanum[1]}};
	}
};
inline void CloseRouteInput() {
	if (aasworld.rtcwLoadFile) botimport.FS_FCloseFile(aasworld.rtcwLoadFile);
	aasworld.rtcwLoadFile = 0;
	if (aasworld.rtcwLoadBytes) FreeMemory(aasworld.rtcwLoadBytes);
	aasworld.rtcwLoadBytes = nullptr;
}
inline void ReplaceRouteRecords(const routecache::View &view) {
	// Validation completed without changing the old tables. From here on every
	// allocation is world-owned before the next engine call can raise an error.
	AAS_FreeAllClusterAreaCache();
	AAS_FreeAllPortalCache();
	AAS_InitClusterAreaCache();
	AAS_InitPortalCache();
	for (const auto record : view.Caches()) {
		auto *cache = AAS_AllocRoutingCache(static_cast<int>(record.Count()));
		cache->type = record.IsPortal() ? CACHETYPE_PORTAL : CACHETYPE_AREA;
		cache->time = record.Time(); cache->cluster = record.Cluster(); cache->areanum = record.Area();
		for (unsigned axis = 0; axis < 3; ++axis) cache->origin[axis] = record.Origin(axis);
		cache->starttraveltime = record.StartTravelTime(); cache->travelflags = record.TravelFlags();
		for (std::size_t i = 0; i < record.Count(); ++i) {
			const unsigned short time = record.TravelTime(i);
			std::memcpy(reinterpret_cast<byte *>(cache) + offsetof(aas_routingcache_t, traveltimes) + i * 2, &time, 2);
			cache->reachabilities[i] = static_cast<byte>(record.Reachability(i));
		}
		auto **head = record.IsPortal() ? &aasworld.portalcache[cache->areanum]
			: &aasworld.clusterareacache[cache->cluster][AAS_ClusterAreaNum(cache->cluster, cache->areanum)];
		cache->next = *head;
		if (*head) (*head)->prev = cache;
		*head = cache;
		AAS_LinkCache(cache);
	}
	AAS_FreeRTCWVisibility();
	aasworld.rtcwVisibility = botlib::ClearedAlloc<aas_rtcw_visibility_t>(view.AreaCount());
	std::size_t bytes = 0;
	for (const auto row : view.Visibility()) bytes += row.bytes;
	if (bytes) aasworld.rtcwVisibilityData = botlib::Alloc<byte>(bytes);
	std::size_t at = 0, area = 0;
	for (const auto row : view.Visibility()) {
		if (row.bytes) {
			aasworld.rtcwVisibility[area].data = aasworld.rtcwVisibilityData + at;
			aasworld.rtcwVisibility[area].bytes = static_cast<int>(row.bytes);
			std::memcpy(aasworld.rtcwVisibilityData + at, row.data, row.bytes);
		}
		at += row.bytes; ++area;
	}
	aasworld.rtcwDecodedVisibility = botlib::ClearedAlloc<byte>(view.AreaCount());
	aasworld.rtcwWaypoints = botlib::Alloc<vec3_t>(view.AreaCount());
	for (std::size_t i = 0; i < view.AreaCount(); ++i)
		for (unsigned axis = 0; axis < 3; ++axis) aasworld.rtcwWaypoints[i][axis] = view.Waypoint(i, axis);
}
} // namespace fntech3::rtcw::navigation

static int AAS_ReadRTCWRouteCache() {
	using namespace fntech3::rtcw;
	if (!aasworld.loaded || !Com_GameIsRTCW()) return qfalse;
	char filename[MAX_QPATH];
	Com_sprintf(filename, sizeof(filename), "maps/%s.rcd", aasworld.mapname);
	const int bytes = botimport.FS_FOpenFile(filename, &aasworld.rtcwLoadFile, FS_READ);
	if (!aasworld.rtcwLoadFile) return qfalse;
	if (bytes < static_cast<int>(routecache::HeaderBytes) || bytes > static_cast<int>(routecache::MaxFileBytes)) {
		navigation::CloseRouteInput();
		botimport.Print(PRT_WARNING, "Invalid RTCW route cache size: %s\n", filename);
		return qfalse;
	}
	aasworld.rtcwLoadBytes = botlib::Alloc<byte>(bytes);
	const int read = botimport.FS_Read(aasworld.rtcwLoadBytes, bytes, aasworld.rtcwLoadFile);
	botimport.FS_FCloseFile(aasworld.rtcwLoadFile); aasworld.rtcwLoadFile = 0;
	routecache::Signature signature{static_cast<std::uint32_t>(aasworld.numareas),
		static_cast<std::uint32_t>(aasworld.numclusters), {}};
	for (unsigned i = 0; i < 3; ++i) signature.crc[i] = aasworld.rtcwNavigationCrc[i];
	routecache::View view;
	auto error = read != bytes ? routecache::Error::Extent : routecache::Parse(
		aasworld.rtcwLoadBytes, bytes, Com_GameIsRTCWSP() ? routecache::Version::SP : routecache::Version::MP,
		signature, view);
	if (error == routecache::Error::None) {
		try { error = routecache::ValidateTopology(view, navigation::RouteTopology(aasworld)); }
		catch (const std::bad_alloc &) {
			navigation::CloseRouteInput();
			botimport.Print(PRT_ERROR, "Insufficient memory to validate RTCW route cache: %s\n", filename);
			return qfalse;
		}
	}
	if (error != routecache::Error::None) {
		navigation::CloseRouteInput();
		botimport.Print(PRT_WARNING, "Invalid RTCW route cache %s: %s\n", filename, routecache::ErrorName(error));
		return qfalse;
	}
	aasworld.rtcwRouteCacheLoaded = qfalse;
	navigation::ReplaceRouteRecords(view);
	navigation::CloseRouteInput();
	aasworld.rtcwRouteCacheLoaded = qtrue;
	botimport.Print(PRT_MESSAGE, "RTCW routing cache loaded: %s\n", filename);
	return qtrue;
}
#endif
