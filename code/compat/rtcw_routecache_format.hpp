/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_ROUTECACHE_FORMAT_HPP
#define FNTECH3_RTCW_ROUTECACHE_FORMAT_HPP

#include "rtcw_aas_format.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

// RTCW's RCD records contain obsolete 32-bit process pointers. They are never
// imported as native structures. Views below refer only to validated bytes;
// callers must keep those bytes alive and unchanged for the view's lifetime.
namespace fntech3::rtcw::routecache {
constexpr std::size_t HeaderBytes = 36, CacheBytes = 52;
constexpr std::size_t MaxFileBytes = 256 * 1024 * 1024;
enum class Version : std::uint32_t { SP = 15, MP = 12 };
enum class Error { None, Header, Identity, Version, Map, Checksum, Extent,
	Cache, Geometry, Visibility, Trailing };
inline const char *ErrorName(Error error) {
	switch (error) {
	case Error::None: return "none";
	case Error::Header: return "invalid header size";
	case Error::Identity: return "wrong cache identity";
	case Error::Version: return "wrong cache version";
	case Error::Map: return "navigation counts differ";
	case Error::Checksum: return "navigation CRC differs";
	case Error::Extent: return "truncated cache data";
	case Error::Cache: return "invalid routing record";
	case Error::Geometry: return "invalid floating-point value";
	case Error::Visibility: return "invalid visibility run";
	case Error::Trailing: return "trailing cache data";
	}
	return "unknown";
}
using aas::U32;
using aas::I32;
using aas::F32;

inline constexpr auto CRCTable() {
	std::array<std::uint16_t, 256> table{};
	for (unsigned i = 0; i < table.size(); ++i) {
		unsigned value = i << 8;
		for (unsigned bit = 0; bit < 8; ++bit)
			value = (value << 1) ^ ((value & 0x8000) ? 0x1021 : 0);
		table[i] = static_cast<std::uint16_t>(value);
	}
	return table;
}
inline std::uint16_t CRC(const void *input, std::size_t bytes) {
	static constexpr auto table = CRCTable();
	const auto *data = static_cast<const unsigned char *>(input);
	std::uint16_t crc = 0xffff;
	for (std::size_t i = 0; i < bytes; ++i)
		crc = static_cast<std::uint16_t>((crc << 8) ^ table[(crc >> 8) ^ data[i]]);
	return crc;
}

struct Signature {
	std::uint32_t areas = 0, clusters = 0;
	std::array<std::uint16_t, 3> crc{}; // raw LE areas, clusters, reachabilities
};
inline Signature MapSignature(const aas::View &map) {
	Signature result{static_cast<std::uint32_t>(map.Count(aas::Areas)),
		static_cast<std::uint32_t>(map.Count(aas::Clusters)), {}};
	const aas::Lump lumps[] = {aas::Areas, aas::Clusters, aas::Reachability};
	for (unsigned i = 0; i < 3; ++i)
		result.crc[i] = CRC(map.Record(lumps[i], 0), map.Count(lumps[i]) * aas::Strides[lumps[i]]);
	return result;
}

struct Span { const unsigned char *data = nullptr; std::size_t bytes = 0; };
inline bool ValidVisibility(Span row, std::size_t areas) {
	if (!row.bytes) return true; // absent row
	if (!row.data || row.bytes % 2) return false;
	std::size_t expanded = 0;
	for (std::size_t i = 0; i < row.bytes; i += 2) {
		const unsigned count = row.data[i + 1];
		if (!count || count > areas - expanded) return false;
		expanded += count;
	}
	return expanded == areas;
}
inline bool DecompressVisibility(Span row, std::size_t areas, void *output, std::size_t capacity) {
	if (capacity < areas || (areas && !output) || !ValidVisibility(row, areas)) return false;
	if (!areas) return true;
	auto *out = static_cast<unsigned char *>(output);
	if (!row.bytes) { std::memset(out, 0, areas); return true; }
	// Source and destination must not overlap, as in the original bot library.
	for (std::size_t i = 0, written = 0; i < row.bytes; i += 2) {
		std::memset(out + written, row.data[i] ? 1 : 0, row.data[i + 1]);
		written += row.data[i + 1];
	}
	return true;
}

class Cache {
	const unsigned char *data_ = nullptr;
	bool portal_ = false;
	friend class CacheIterator;
	Cache(const unsigned char *data, bool portal) : data_(data), portal_(portal) {}
public:
	bool IsPortal() const { return portal_; }
	std::size_t Bytes() const { return U32(data_); }
	std::size_t Count() const { return (Bytes() - CacheBytes) / 3; }
	float Time() const { return F32(data_ + 4); }
	std::int32_t Cluster() const { return I32(data_ + 8); }
	std::int32_t Area() const { return I32(data_ + 12); }
	float Origin(unsigned axis) const { return axis < 3 ? F32(data_ + 16 + axis * 4) : 0; }
	float StartTravelTime() const { return F32(data_ + 28); }
	std::int32_t TravelFlags() const { return I32(data_ + 32); }
	std::uint16_t TravelTime(std::size_t index) const {
		if (index >= Count()) return 0;
		const auto *p = data_ + 48 + index * 2;
		return static_cast<std::uint16_t>(p[0] | (unsigned(p[1]) << 8));
	}
	unsigned Reachability(std::size_t index) const {
		return index < Count() ? data_[CacheBytes + Count() * 2 + index] : 0;
	}
};
class CacheIterator {
	const unsigned char *data_;
	std::size_t index_, portals_;
public:
	CacheIterator(const unsigned char *data, std::size_t index, std::size_t portals)
		: data_(data), index_(index), portals_(portals) {}
	Cache operator*() const { return Cache(data_, index_ < portals_); }
	CacheIterator &operator++() { data_ += U32(data_); ++index_; return *this; }
	bool operator!=(const CacheIterator &other) const { return index_ != other.index_; }
};
struct CacheRange {
	const unsigned char *data;
	std::size_t portals, areas;
	CacheIterator begin() const { return {data, 0, portals}; }
	CacheIterator end() const { return {nullptr, portals + areas, portals}; }
};
class VisibilityIterator {
	const unsigned char *data_;
	std::size_t index_;
public:
	VisibilityIterator(const unsigned char *data, std::size_t index) : data_(data), index_(index) {}
	Span operator*() const { return {data_ + 4, U32(data_)}; }
	VisibilityIterator &operator++() { data_ += 4 + U32(data_); ++index_; return *this; }
	bool operator!=(const VisibilityIterator &other) const { return index_ != other.index_; }
};
struct VisibilityRange {
	const unsigned char *data;
	std::size_t areas;
	VisibilityIterator begin() const { return {data, 0}; }
	VisibilityIterator end() const { return {nullptr, areas}; }
};
class View {
	const unsigned char *data_ = nullptr, *visibility_ = nullptr, *waypoints_ = nullptr;
	std::size_t areas_ = 0, portals_ = 0, caches_ = 0;
	friend Error Parse(const void *, std::size_t, Version, const Signature &, View &);
public:
	std::size_t AreaCount() const { return areas_; }
	CacheRange Caches() const { return {data_ ? data_ + HeaderBytes : nullptr, portals_, caches_}; }
	VisibilityRange Visibility() const { return {visibility_, areas_}; }
	float Waypoint(std::size_t area, unsigned axis) const {
		return area < areas_ && axis < 3 ? F32(waypoints_ + area * 12 + axis * 4) : 0;
	}
};

inline Error Parse(const void *input, std::size_t bytes, Version version,
	const Signature &map, View &destination) {
	if (!input || bytes < HeaderBytes || bytes > MaxFileBytes) return Error::Header;
	const auto *data = static_cast<const unsigned char *>(input);
	if (U32(data) != 0x4352454du) return Error::Identity; // MERC
	if ((version != Version::SP && version != Version::MP)
		|| U32(data + 4) != static_cast<std::uint32_t>(version)) return Error::Version;
	if (map.areas < 2 || map.areas > 65535 || !map.clusters
		|| U32(data + 8) != map.areas || U32(data + 12) != map.clusters) return Error::Map;
	for (unsigned i = 0; i < 3; ++i)
		if (U32(data + 16 + i * 4) != map.crc[i]) return Error::Checksum;
	View view;
	view.data_ = data; view.areas_ = map.areas;
	view.portals_ = U32(data + 28); view.caches_ = U32(data + 32);
	const std::size_t maxCaches = (bytes - HeaderBytes) / CacheBytes;
	if (view.portals_ > maxCaches || view.caches_ > maxCaches - view.portals_) return Error::Extent;
	std::size_t at = HeaderBytes;
	for (std::size_t i = 0; i < view.portals_ + view.caches_; ++i) {
		if (bytes - at < CacheBytes) return Error::Extent;
		const auto *p = data + at;
		const std::size_t size = U32(p);
		if (size < CacheBytes || (size - CacheBytes) % 3 || (size - CacheBytes) / 3 > 65535)
			return Error::Cache;
		if (size > bytes - at) return Error::Extent;
		if (I32(p + 8) <= 0 || U32(p + 8) >= map.clusters || I32(p + 12) <= 0
			|| U32(p + 12) >= map.areas) return Error::Cache;
		for (unsigned offset : {4u, 16u, 20u, 24u, 28u})
			if (!aas::Finite(p + offset)) return Error::Geometry;
		for (unsigned axis = 0; axis < 3; ++axis)
			if (F32(p + 16 + axis * 4) < -131072 || F32(p + 16 + axis * 4) > 131072)
				return Error::Geometry;
		if (F32(p + 28) < 0 || F32(p + 28) > 65535) return Error::Geometry;
		at += size;
	}
	view.visibility_ = data + at;
	for (std::size_t i = 0; i < map.areas; ++i) {
		if (bytes - at < 4) return Error::Extent;
		const std::size_t size = U32(data + at); at += 4;
		if (size > bytes - at) return Error::Extent;
		if (!ValidVisibility({data + at, size}, map.areas)) return Error::Visibility;
		at += size;
	}
	if (bytes - at < map.areas * 12u) return Error::Extent;
	view.waypoints_ = data + at;
	for (std::size_t i = 0; i < map.areas * 3u; ++i) {
		const auto *p = data + at + i * 4;
		if (!aas::Finite(p) || F32(p) < -131072 || F32(p) > 131072) return Error::Geometry;
	}
	if (bytes - at != map.areas * 12u) return Error::Trailing;
	destination = view;
	return Error::None;
}

struct Area { std::int32_t cluster, local, reachabilities; };
struct Cluster { std::int32_t areas, reachable; };
struct Portal { std::int32_t area, clusters[2], locals[2]; };
// The same validation also accepts a native-map adapter with these accessors.
// This keeps external AAS8 bytes separate from the engine's internal layouts.
class AAS8Topology {
	const aas::View &map_;
public:
	explicit AAS8Topology(const aas::View &map) : map_(map) {}
	std::size_t AreaCount() const { return map_.Count(aas::Areas); }
	std::size_t ClusterCount() const { return map_.Count(aas::Clusters); }
	std::size_t PortalCount() const { return map_.Count(aas::Portals); }
	Area GetArea(std::size_t i) const {
		const auto *p = map_.Record(aas::Settings, i);
		return {I32(p + 12), I32(p + 16), I32(p + 20)};
	}
	Cluster GetCluster(std::size_t i) const {
		const auto *p = map_.Record(aas::Clusters, i);
		return {I32(p), I32(p + 4)};
	}
	Portal GetPortal(std::size_t i) const {
		const auto *p = map_.Record(aas::Portals, i);
		return {I32(p), {I32(p + 4), I32(p + 8)}, {I32(p + 12), I32(p + 16)}};
	}
};
template<class Topology>
Error ValidateTopology(const View &view, const Topology &map) {
	const auto areas = map.AreaCount(), clusters = map.ClusterCount(), portals = map.PortalCount();
	if (areas < 2 || areas > 65535 || areas != view.AreaCount() || !clusters
		|| clusters > areas || !portals || portals > areas) return Error::Map;
	// Every ordinary area contributes one local index and a portal at most two.
	// Bound the complete index allocation before using any map-provided count.
	const auto maxLocals = areas + portals * 2;
	std::vector<std::size_t> first(clusters + 1);
	for (std::size_t c = 0; c < clusters; ++c) {
		const auto cluster = map.GetCluster(c);
		if (cluster.areas < 0 || cluster.reachable < 0 || cluster.reachable > cluster.areas
			|| static_cast<std::size_t>(cluster.areas) > maxLocals - first[c]) return Error::Map;
		first[c + 1] = first[c] + cluster.areas;
	}
	std::vector<std::int32_t> localAreas(first.back(), 0);
	const auto assign = [&](std::int32_t cluster, std::int32_t local, std::int32_t area) {
		if (cluster <= 0 || static_cast<std::size_t>(cluster) >= clusters || local < 0
			|| local >= map.GetCluster(cluster).areas) return false;
		auto &slot = localAreas[first[cluster] + local];
		if (slot) return false;
		slot = area;
		return true;
	};
	for (std::size_t a = 1; a < areas; ++a) {
		const auto area = map.GetArea(a);
		if (area.reachabilities < 0) return Error::Map;
		if (area.cluster > 0) {
			if (!assign(area.cluster, area.local, static_cast<std::int32_t>(a))) return Error::Map;
		} else if (area.cluster < 0) {
			const auto p = -static_cast<std::int64_t>(area.cluster);
			if (static_cast<std::uint64_t>(p) >= portals) return Error::Map;
			const auto portal = map.GetPortal(static_cast<std::size_t>(p));
			if (portal.area != static_cast<std::int32_t>(a)) return Error::Map;
			for (unsigned side = 0; side < 2; ++side)
				if (!assign(portal.clusters[side], portal.locals[side], portal.area)) return Error::Map;
		}
	}
	for (std::size_t p = 1; p < portals; ++p) {
		const auto area = map.GetPortal(p).area;
		if (area <= 0 || static_cast<std::size_t>(area) >= areas
			|| map.GetArea(area).cluster != -static_cast<std::int32_t>(p)) return Error::Map;
	}
	for (const auto cache : view.Caches()) {
		if (cache.Cluster() <= 0 || static_cast<std::size_t>(cache.Cluster()) >= clusters
			|| cache.Area() <= 0 || static_cast<std::size_t>(cache.Area()) >= areas) return Error::Cache;
		const auto goal = map.GetArea(cache.Area());
		bool member = goal.cluster == cache.Cluster();
		if (goal.cluster < 0) {
			const auto portal = map.GetPortal(static_cast<std::size_t>(-static_cast<std::int64_t>(goal.cluster)));
			member = portal.clusters[0] == cache.Cluster() || portal.clusters[1] == cache.Cluster();
		}
		if (!member || cache.Count() != (cache.IsPortal() ? portals
			: static_cast<std::size_t>(map.GetCluster(cache.Cluster()).reachable))) return Error::Cache;
		for (std::size_t i = 0; i < cache.Count(); ++i) {
			if (!cache.TravelTime(i)) continue;
			const auto source = cache.IsPortal() ? map.GetPortal(i).area : localAreas[first[cache.Cluster()] + i];
			if (source <= 0 || static_cast<std::size_t>(source) >= areas) return Error::Cache;
			if (cache.Reachability(i) >= static_cast<unsigned>(map.GetArea(source).reachabilities)
				&& !(source == cache.Area() && cache.Reachability(i) == 0)) return Error::Cache;
		}
	}
	return Error::None;
}
} // namespace fntech3::rtcw::routecache
#endif
