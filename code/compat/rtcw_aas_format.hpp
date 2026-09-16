/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_AAS_FORMAT_HPP
#define FNTECH3_RTCW_AAS_FORMAT_HPP
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Independent reader for RTCW's EAAS version 8. The input remains immutable;
// no disk POD aliases an engine navigation object. The original SP/MP
// aasfile.h layouts are pinned by rtcw_aas_layout.def reference probes.
namespace fntech3::rtcw::aas {
enum Lump : unsigned {
	Boxes, Vertices, Planes, Edges, EdgeIndices, Faces, FaceIndices,
	Areas, Settings, Reachability, Nodes, Portals, PortalIndices, Clusters, LumpCount
};
inline constexpr std::size_t HeaderBytes = 124;
inline constexpr std::array<unsigned, LumpCount> Strides{32,12,20,8,4,24,4,48,32,44,12,20,4,16};
enum class Error { None, Header, Identity, Version, Checksum, Extent, Stride, Overlap,
	Empty, Geometry, Index, Tree, Routing };
inline const char *ErrorName( Error error ) {
	switch ( error ) {
	case Error::None: return "none";
	case Error::Header: return "truncated header";
	case Error::Identity: return "wrong identity";
	case Error::Version: return "wrong version";
	case Error::Checksum: return "BSP checksum mismatch";
	case Error::Extent: return "lump outside file";
	case Error::Stride: return "partial record";
	case Error::Overlap: return "overlapping lumps";
	case Error::Empty: return "missing navigation geometry";
	case Error::Geometry: return "invalid geometry";
	case Error::Index: return "invalid geometry index";
	case Error::Tree: return "cyclic or shared node tree";
	case Error::Routing: return "invalid routing index";
	}
	return "unknown";
}
inline std::uint32_t U32( const unsigned char *p ) {
	return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8
		| std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
inline std::int32_t I32( const unsigned char *p ) {
	const std::uint32_t bits = U32( p );
	std::int32_t value;
	std::memcpy( &value, &bits, sizeof( value ) );
	return value;
}
inline float F32( const unsigned char *p ) {
	const std::uint32_t bits = U32( p );
	float value;
	std::memcpy( &value, &bits, sizeof( value ) );
	return value;
}
inline bool Finite( const unsigned char *p ) { return ( U32(p) & 0x7f800000u ) != 0x7f800000u; }

class View {
	struct Span { std::size_t offset = 0, count = 0; };
	const unsigned char *data_ = nullptr;
	std::array<Span, LumpCount> lumps_{};
	std::uint32_t checksum_ = 0;
	friend Error Parse( const void *, std::size_t, std::uint32_t, View & );
public:
	std::size_t Count( Lump lump ) const { return lump < LumpCount ? lumps_[lump].count : 0; }
	std::uint32_t Checksum() const { return checksum_; }
	const unsigned char *Record( Lump lump, std::size_t index ) const {
		if ( !data_ || lump >= LumpCount || index >= lumps_[lump].count ) return nullptr;
		return data_ + lumps_[lump].offset + index * Strides[lump];
	}
};

inline Error Parse( const void *input, std::size_t bytes, std::uint32_t checksum, View &destination ) {
	if ( !input || bytes < HeaderBytes ) return Error::Header;
	const auto *data = static_cast<const unsigned char *>( input );
	if ( U32(data) != 0x53414145u ) return Error::Identity;
	if ( U32(data + 4) != 8 ) return Error::Version;
	std::array<unsigned char, HeaderBytes> header;
	std::memcpy( header.data(), data, HeaderBytes );
	for ( std::size_t i = 8; i < HeaderBytes; ++i ) header[i] ^= static_cast<unsigned char>((i - 8) * 119);
	if ( U32(header.data() + 8) != checksum ) return Error::Checksum;
	View view;
	view.data_ = data; view.checksum_ = checksum;
	for ( unsigned i = 0; i < LumpCount; ++i ) {
		const std::size_t offset = U32(header.data() + 12 + i * 8);
		const std::size_t length = U32(header.data() + 16 + i * 8);
		if ( offset > bytes || length > bytes - offset || (length && offset < HeaderBytes) ) return Error::Extent;
		if ( length % Strides[i] ) return Error::Stride;
		view.lumps_[i] = {offset, length / Strides[i]};
		for ( unsigned j = 0; length && j < i; ++j ) {
			const auto &other = view.lumps_[j];
			if ( other.count && offset < other.offset + other.count * Strides[j] && other.offset < offset + length )
				return Error::Overlap;
		}
	}
	const auto count = [&view]( Lump lump ) { return view.Count(lump); };
	const auto record = [&view]( Lump lump, std::size_t i ) { return view.Record(lump, i); };
	if ( !count(Boxes) || !count(Vertices) || count(Planes) < 2 || count(Planes) % 2
		|| !count(Edges) || !count(Faces) || count(Areas) < 2 || count(Nodes) < 2
		|| count(Settings) != count(Areas) || !count(Portals) || !count(Clusters) ) return Error::Empty;
	// Routing uses unsigned-short area numbers. Tree stack occupancy is not
	// depth: installed crypt1 navigation contains valid paths 160 nodes deep.
	if ( count(Areas) > 65535 ) return Error::Routing;
	const auto index = [&count]( std::int32_t value, Lump target ) {
		return value >= 0 && static_cast<std::size_t>(value) < count(target);
	};
	const auto signedIndex = [&index]( std::int32_t value, Lump target ) {
		if ( value == INT32_MIN ) return false;
		return index( value < 0 ? -value : value, target );
	};
	const auto range = [&count]( std::int32_t first, std::int32_t length, Lump target ) {
		return first >= 0 && length >= 0 && static_cast<std::size_t>(first) <= count(target)
			&& static_cast<std::size_t>(length) <= count(target) - static_cast<std::size_t>(first);
	};
	const auto vector = []( const unsigned char *p ) {
		for ( unsigned i = 0; i < 3; ++i ) {
			if ( !Finite(p + i * 4) || F32(p + i * 4) < -131072.0f || F32(p + i * 4) > 131072.0f ) return false;
		}
		return true;
	};
	const auto bounds = [&vector]( const unsigned char *p ) {
		if ( !vector(p) || !vector(p + 12) ) return false;
		for ( unsigned i = 0; i < 3; ++i ) if ( F32(p + i * 4) > F32(p + 12 + i * 4) ) return false;
		return true;
	};
	for ( std::size_t i = 0; i < count(Boxes); ++i ) if ( !bounds(record(Boxes,i) + 8) ) return Error::Geometry;
	for ( std::size_t i = 0; i < count(Vertices); ++i ) if ( !vector(record(Vertices,i)) ) return Error::Geometry;
	for ( std::size_t i = 0; i < count(Planes); ++i ) {
		const auto *p = record(Planes,i);
		if ( !vector(p) || !Finite(p + 12) || I32(p + 16) < 0 || I32(p + 16) > 5 ) return Error::Geometry;
	}
	for ( std::size_t i = 0; i < count(Edges); ++i ) {
		const auto *p = record(Edges,i);
		if ( !index(I32(p),Vertices) || !index(I32(p + 4),Vertices) ) return Error::Index;
	}
	for ( std::size_t i = 0; i < count(EdgeIndices); ++i )
		if ( !signedIndex(I32(record(EdgeIndices,i)),Edges) ) return Error::Index;
	for ( std::size_t i = 1; i < count(Faces); ++i ) {
		const auto *p = record(Faces,i);
		if ( !index(I32(p),Planes) || !range(I32(p + 12),I32(p + 8),EdgeIndices)
			|| !index(I32(p + 16),Areas) || !index(I32(p + 20),Areas) ) return Error::Index;
	}
	for ( std::size_t i = 0; i < count(FaceIndices); ++i )
		if ( !signedIndex(I32(record(FaceIndices,i)),Faces) ) return Error::Index;
	for ( std::size_t i = 1; i < count(Areas); ++i ) {
		const auto *p = record(Areas,i);
		if ( I32(p) != static_cast<std::int32_t>(i) || !range(I32(p + 8),I32(p + 4),FaceIndices) ) return Error::Index;
		if ( !bounds(p + 12) || !vector(p + 36) ) return Error::Geometry;
		p = record(Settings,i);
		const std::int32_t cluster = I32(p + 12), local = I32(p + 16);
		if ( !Finite(p + 28) || F32(p + 28) < 0 || F32(p + 28) > 1 ) return Error::Geometry;
		if ( !range(I32(p + 24),I32(p + 20),Reachability) || local < 0 ) return Error::Routing;
		if ( cluster > 0 ) {
			if ( !index(cluster,Clusters) || local >= I32(record(Clusters,cluster)) ) return Error::Routing;
		} else if ( cluster < 0 && (!signedIndex(cluster,Portals) || I32(record(Portals,-cluster)) != static_cast<std::int32_t>(i)) ) {
			return Error::Routing;
		}
	}
	for ( std::size_t i = 0; i < count(Reachability); ++i ) {
		const auto *p = record(Reachability,i);
		if ( !index(I32(p),Areas) ) return Error::Routing;
		if ( !vector(p + 12) || !vector(p + 24) ) return Error::Geometry;
		// facenum/edgenum are overloaded by jump-pad and mover reachabilities.
		// They are validated by the travel-specific consumer, not as plain indices.
	}
	std::vector<unsigned> incoming(count(Nodes),0);
	std::vector<std::size_t> ready;
	for ( std::size_t i = 1; i < count(Nodes); ++i ) {
		const auto *p = record(Nodes,i);
		if ( !index(I32(p),Planes) ) return Error::Index;
		for ( unsigned side = 0; side < 2; ++side ) {
			const auto child = I32(p + 4 + side * 4);
			if ( child > 0 ) {
				if ( !index(child,Nodes) ) return Error::Index;
				if ( ++incoming[child] > 1 ) return Error::Tree;
			} else if ( !signedIndex(child,Areas) ) return Error::Index;
		}
	}
	if ( incoming[1] ) return Error::Tree;
	for ( std::size_t i = 1; i < count(Nodes); ++i ) if ( !incoming[i] ) ready.push_back(i);
	for ( std::size_t i = 0; i < ready.size(); ++i ) {
		const auto node = ready[i];
		for ( unsigned side = 0; side < 2; ++side ) {
			const auto child = I32(record(Nodes,node) + 4 + side * 4);
			if ( child <= 0 ) continue;
			if ( !--incoming[child] ) ready.push_back(child);
		}
	}
	if ( ready.size() != count(Nodes) - 1 ) return Error::Tree;
	for ( std::size_t i = 1; i < count(Portals); ++i ) {
		const auto *p = record(Portals,i);
		if ( !index(I32(p),Areas) ) return Error::Routing;
		for ( unsigned side = 0; side < 2; ++side ) {
			const auto cluster = I32(p + 4 + side * 4), local = I32(p + 12 + side * 4);
			if ( !index(cluster,Clusters) || local < 0 || local >= I32(record(Clusters,cluster)) ) return Error::Routing;
		}
	}
	for ( std::size_t i = 0; i < count(PortalIndices); ++i )
		if ( !index(I32(record(PortalIndices,i)),Portals) ) return Error::Routing;
	for ( std::size_t i = 1; i < count(Clusters); ++i ) {
		const auto *p = record(Clusters,i);
		if ( I32(p) < 0 || I32(p + 4) < 0 || I32(p + 4) > I32(p)
			|| static_cast<std::size_t>(I32(p)) > count(Areas)
			|| !range(I32(p + 12),I32(p + 8),PortalIndices) ) return Error::Routing;
	}
	destination = view;
	return Error::None;
}
} // namespace fntech3::rtcw::aas
#endif
