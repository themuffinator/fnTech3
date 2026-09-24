/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_ROUTECACHE_WRITER_HPP
#define FNTECH3_RTCW_ROUTECACHE_WRITER_HPP

// Serialize the RCD15/RCD12 disk contract, never native pointers or padding.
// Included after the loader; its checked reader validates the completed image
// before any writable file is opened. Reference observations: CREDITS.md.
namespace fntech3::rtcw::navigation {
inline void RouteU32(byte *out, std::uint32_t value) {
	for (unsigned i = 0; i < 4; ++i) out[i] = static_cast<byte>(value >> (i * 8));
}
inline void RouteFloat(byte *out, float value) {
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	RouteU32(out, bits);
}
template<class Visitor>
bool VisitRouteCaches(Visitor visit) {
	if (!aasworld.portalcache || !aasworld.clusterareacache) return false;
	for (int area = 0; area < aasworld.numareas; ++area)
		for (const auto *cache = aasworld.portalcache[area]; cache; cache = cache->next)
			if (!visit(*cache, aasworld.numportals, true)) return false;
	for (int cluster = 0; cluster < aasworld.numclusters; ++cluster)
		for (int local = 0; local < aasworld.clusters[cluster].numareas; ++local)
			for (const auto *cache = aasworld.clusterareacache[cluster][local]; cache; cache = cache->next)
				if (!visit(*cache, aasworld.clusters[cluster].numreachabilityareas, false)) return false;
	return true;
}
inline bool EncodeRouteCache(std::size_t &bytes) {
	std::uint32_t portals = 0, areas = 0;
	bytes = routecache::HeaderBytes;
	const auto add = [&](std::size_t n) {
		if (n > routecache::MaxFileBytes - bytes) return false;
		bytes += n; return true;
	};
	if (!VisitRouteCaches([&](const aas_routingcache_t &cache, int count, bool portal) {
		if (count < 0 || count > 65535 || cache.size != static_cast<int>(sizeof(cache) + count * 3u)
			|| cache.reachabilities != reinterpret_cast<const byte *>(&cache) + sizeof(cache) + count * 2u)
			return false;
		if (!add(routecache::CacheBytes + count * 3u)) return false;
		++(portal ? portals : areas); return true;
	})) return false;
	for (int area = 0; area < aasworld.numareas; ++area) {
		const auto &row = aasworld.rtcwVisibility[area];
		if (row.bytes < 0 || !routecache::ValidVisibility({row.data, static_cast<std::size_t>(row.bytes)},
			aasworld.numareas) || !add(4u + row.bytes)) return false;
	}
	if (!add(static_cast<std::size_t>(aasworld.numareas) * 12)) return false;
	// Ownership is published before allocation can be followed by an engine
	// error. Map teardown also closes/frees this buffer during a nonlocal exit.
	aasworld.rtcwLoadBytes = botlib::ClearedAlloc<byte>(bytes);
	auto *out = aasworld.rtcwLoadBytes;
	RouteU32(out, 0x4352454d);
	RouteU32(out + 4, Com_GameIsRTCWSP() ? 15 : 12);
	RouteU32(out + 8, aasworld.numareas); RouteU32(out + 12, aasworld.numclusters);
	for (unsigned i = 0; i < 3; ++i) RouteU32(out + 16 + i * 4, aasworld.rtcwNavigationCrc[i]);
	RouteU32(out + 28, portals); RouteU32(out + 32, areas);
	out += routecache::HeaderBytes;
	VisitRouteCaches([&](const aas_routingcache_t &cache, int count, bool) {
		const auto size = routecache::CacheBytes + count * 3u;
		RouteU32(out, static_cast<std::uint32_t>(size)); RouteFloat(out + 4, cache.time);
		RouteU32(out + 8, cache.cluster); RouteU32(out + 12, cache.areanum);
		for (unsigned axis = 0; axis < 3; ++axis) RouteFloat(out + 16 + axis * 4, cache.origin[axis]);
		RouteFloat(out + 28, cache.starttraveltime); RouteU32(out + 32, cache.travelflags);
		const auto *times = reinterpret_cast<const byte *>(&cache) + offsetof(aas_routingcache_t, traveltimes);
		for (int i = 0; i < count; ++i) {
			std::uint16_t value; std::memcpy(&value, times + i * 2, 2);
			out[48 + i * 2] = static_cast<byte>(value); out[49 + i * 2] = static_cast<byte>(value >> 8);
			out[routecache::CacheBytes + count * 2u + i] = cache.reachabilities[i];
		}
		out += size; return true;
	});
	for (int area = 0; area < aasworld.numareas; ++area) {
		const auto &row = aasworld.rtcwVisibility[area];
		RouteU32(out, row.bytes); out += 4;
		if (row.bytes) { std::memcpy(out, row.data, row.bytes); out += row.bytes; }
	}
	for (int area = 0; area < aasworld.numareas; ++area)
		for (unsigned axis = 0; axis < 3; ++axis, out += 4) RouteFloat(out, aasworld.rtcwWaypoints[area][axis]);
	return true;
}
} // namespace fntech3::rtcw::navigation

static bool AAS_WriteRTCWRouteCache() {
	using namespace fntech3::rtcw;
	if (!Com_GameIsRTCW() || !aasworld.loaded || !aasworld.rtcwRouteCacheLoaded
		|| !aasworld.rtcwVisibility || !aasworld.rtcwWaypoints || aasworld.rtcwLoadBytes
		|| aasworld.rtcwLoadFile || !botimport.FS_Write || !botimport.FS_FOpenFile || !botimport.FS_FCloseFile)
		return false;
	const char *map = aasworld.mapname;
	if (!map[0] || map[0] == '/' || std::strchr(map, '\\') || std::strchr(map, ':')
		|| std::strstr(map, "..") || std::strlen(map) + 10 > MAX_QPATH) return false;
	std::size_t bytes = 0;
	bool valid = false;
	try {
		if (routecache::ValidateTopology(navigation::RouteTopology(aasworld)) == routecache::Error::None
			&& navigation::EncodeRouteCache(bytes)) {
			routecache::Signature signature{static_cast<std::uint32_t>(aasworld.numareas),
				static_cast<std::uint32_t>(aasworld.numclusters), {}};
			for (unsigned i = 0; i < 3; ++i) signature.crc[i] = aasworld.rtcwNavigationCrc[i];
			routecache::View view;
			valid = routecache::Parse(aasworld.rtcwLoadBytes, bytes,
				Com_GameIsRTCWSP() ? routecache::Version::SP : routecache::Version::MP,
				signature, view) == routecache::Error::None
				&& routecache::ValidateTopology(view, navigation::RouteTopology(aasworld)) == routecache::Error::None;
		}
	} catch (const std::bad_alloc &) { valid = false; }
	if (!valid) {
		navigation::CloseRouteInput();
		botimport.Print(PRT_WARNING, "RTCW route cache could not be encoded: %s\n", map);
		return false;
	}
	char filename[MAX_QPATH];
	Com_sprintf(filename, sizeof(filename), "maps/%s.rcd", map);
	botimport.FS_FOpenFile(filename, &aasworld.rtcwLoadFile, FS_WRITE);
	const bool written = aasworld.rtcwLoadFile
		&& botimport.FS_Write(aasworld.rtcwLoadBytes, static_cast<int>(bytes), aasworld.rtcwLoadFile) == static_cast<int>(bytes);
	navigation::CloseRouteInput();
	// The in-memory cache remains usable on a read-only/full filesystem. An
	// incomplete file fails the checked reader and is regenerated next time.
	botimport.Print(written ? PRT_MESSAGE : PRT_WARNING, written
		? "RTCW routing cache written: %s\n" : "RTCW routing cache could not be written: %s\n", filename);
	return written;
}
#endif
