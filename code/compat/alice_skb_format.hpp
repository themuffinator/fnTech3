/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_SKB_FORMAT_HPP
#define FNTECH3_COMPAT_ALICE_SKB_FORMAT_HPP

/* Bounded reader for Alice's `SKL ` skeleton and base frame.
 *
 * A `.skb` holds a model's bone hierarchy and its skinned surfaces. The bones
 * are a flat array in topological order: every bone names its parent by an
 * index that is always earlier in the array, or -1 for a root, so a walk from
 * any bone to a root terminates without a visited set. The surfaces follow as
 * a chain, each declaring where the next begins, and each holds triangles, a
 * vertex block and a collapse map. The vertex block is the one part with no
 * fixed stride: each vertex carries a weight count and then that many weights,
 * each naming a bone.
 *
 * Unlike Alice's `TAN ` models, this format has no published declaration --
 * Ritual's F.A.K.K.2 v1.02 SDK predates it -- so every field below was derived
 * from the 79 `.skb` files a retail Alice installation ships and is admitted
 * only because all 79 agree. See docs/fntech3/2026-09-22-alice-models.md for
 * how each one was established.
 *
 * Every offset and count is checked against the supplied buffer before
 * anything is read. Nothing here poses a bone, skins a vertex or draws. */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace alice {
namespace model {

/* Ritual's identifier spells "SKL " in file order, and repeats at the head of
 * every surface. Alice ships versions 2 and 3, which share this layout. */
inline constexpr std::uint32_t SkbIdent =
	( ' ' << 24 ) | ( 'L' << 16 ) | ( 'K' << 8 ) | 'S';
inline constexpr int SkbVersionLow = 2;
inline constexpr int SkbVersionHigh = 3;

inline constexpr std::size_t SkbNameBytes = 64;
inline constexpr std::size_t SkbHeaderBytes = 92;
inline constexpr std::size_t SkbBoneBytes = 72;        /* parent, flags, name */
inline constexpr std::size_t SkbSurfaceHeaderBytes = 96;
inline constexpr std::size_t SkbTriangleBytes = 12;    /* three vertex indices */
inline constexpr std::size_t SkbVertexFixedBytes = 24; /* before the weights */
inline constexpr std::size_t SkbWeightCountOffset = 20;
inline constexpr std::size_t SkbWeightBytes = 20;      /* bone index first */
inline constexpr std::size_t SkbCollapseBytes = 4;     /* one index per vertex */

/* Bounds a retail file never reaches. A file that claims more is refused
 * rather than clamped: the largest installed skeleton has 131 bones. */
inline constexpr int SkbMaxBones = 1024;
inline constexpr int SkbMaxSurfaces = 256;
inline constexpr int SkbMaxVerts = 65536;
inline constexpr int SkbMaxTriangles = 65536;

enum class SkbError {
	None, Size, Ident, Version, Counts, Offset, Extent, Name, Order, Parent, Bone
};

inline const char *SkbErrorName( SkbError error ) {
	switch ( error ) {
	case SkbError::None: return "none";
	case SkbError::Size: return "file too small";
	case SkbError::Ident: return "wrong identifier";
	case SkbError::Version: return "wrong version";
	case SkbError::Counts: return "count out of range";
	case SkbError::Offset: return "offset out of range";
	case SkbError::Extent: return "record past end of file";
	case SkbError::Name: return "unterminated name";
	case SkbError::Order: return "record out of order";
	case SkbError::Parent: return "bone parent is not an earlier bone";
	case SkbError::Bone: return "weight names a bone that does not exist";
	}
	return "unknown";
}

class SkbReader {
public:
	SkbReader( const void *data, std::size_t bytes )
		: bytes_( data ? static_cast<const std::uint8_t *>( data ) : nullptr ),
		  size_( data ? bytes : 0 ) {}

	std::size_t Size() const { return size_; }

	bool Fits( std::size_t offset, std::size_t length ) const {
		return offset <= size_ && length <= size_ - offset;
	}

	// A count of records, checked before the multiplication can wrap.
	bool Fits( std::size_t offset, std::size_t stride, std::size_t count ) const {
		if ( stride && count > size_ / stride ) return false;
		return Fits( offset, stride * count );
	}

	std::int32_t Int( std::size_t offset ) const {
		std::int32_t value = 0;
		std::memcpy( &value, bytes_ + offset, sizeof( value ) );
		return value;
	}

	std::uint32_t Unsigned( std::size_t offset ) const {
		std::uint32_t value = 0;
		std::memcpy( &value, bytes_ + offset, sizeof( value ) );
		return value;
	}

	/* A name must terminate inside its own field. Retail files leave whatever
	 * was in the buffer after the terminator, so only the prefix is checked. */
	bool Name( std::size_t offset ) const {
		return std::memchr( bytes_ + offset, 0, SkbNameBytes ) != nullptr;
	}

private:
	const std::uint8_t *bytes_;
	std::size_t size_;
};

struct SkbSurface {
	std::size_t header;
	std::size_t name;
	int triangles;
	int verts;
	int minLod;               /* vertices kept at the lowest detail */
	std::size_t triangleData;
	std::size_t vertexData;   /* variable stride; walk it, do not index it */
	std::size_t collapseData;
	std::size_t nextSurface;
	int weights;              /* total across this surface's vertices */
};

struct Skb {
	int version;
	std::size_t name;
	int bones;
	int surfaces;
	std::size_t boneData;
	int roots;
	SkbSurface surface[SkbMaxSurfaces];
};

/* Where a bone's record and its name field begin. ParseSkb has already
 * proved every bone lies inside the file and every parent is an earlier
 * bone, so a caller may walk the hierarchy without a visited set. */
inline std::size_t SkbBoneAt( const Skb &skb, int bone ) {
	return skb.boneData + SkbBoneBytes * std::size_t( bone );
}

inline std::size_t SkbBoneName( const Skb &skb, int bone ) {
	return SkbBoneAt( skb, bone ) + 8;
}

inline std::int32_t SkbBoneParent( const void *data, const Skb &skb, int bone ) {
	std::int32_t value = 0;
	std::memcpy( &value, static_cast<const std::uint8_t *>( data )
		+ SkbBoneAt( skb, bone ), sizeof( value ) );
	return value;
}

/*
==================
ParseSkb

Read a `SKL ` 2 or 3 skeleton's directory, walking the variable-stride vertex
blocks so that a weight can never name a bone the file does not have. Every
record is located and bounded; none is decoded.
==================
*/
inline SkbError ParseSkb( const void *data, std::size_t bytes, Skb &out ) {
	out = Skb{};
	const SkbReader reader( data, bytes );
	if ( reader.Size() < SkbHeaderBytes ) return SkbError::Size;
	if ( reader.Unsigned( 0 ) != SkbIdent ) return SkbError::Ident;
	out.version = reader.Int( 4 );
	if ( out.version < SkbVersionLow || out.version > SkbVersionHigh ) {
		return SkbError::Version;
	}
	if ( !reader.Name( 8 ) ) return SkbError::Name;
	out.name = 8;

	out.surfaces = reader.Int( 72 );
	out.bones = reader.Int( 76 );
	const std::int32_t ofsBones = reader.Int( 80 );
	const std::int32_t ofsSurfaces = reader.Int( 84 );
	const std::int32_t ofsEnd = reader.Int( 88 );

	if ( ofsEnd < 0 || std::size_t( ofsEnd ) != reader.Size() ) return SkbError::Extent;
	if ( out.bones <= 0 || out.bones > SkbMaxBones ) return SkbError::Counts;
	if ( out.surfaces < 0 || out.surfaces > SkbMaxSurfaces ) return SkbError::Counts;
	/* The bones begin immediately after the header on every installed file,
	 * and the block reaches the first surface exactly. */
	if ( ofsBones != std::int32_t( SkbHeaderBytes ) ) return SkbError::Offset;
	if ( !reader.Fits( std::size_t( ofsBones ), SkbBoneBytes, std::size_t( out.bones ) ) ) {
		return SkbError::Extent;
	}
	if ( std::size_t( ofsBones ) + SkbBoneBytes * std::size_t( out.bones )
		!= std::size_t( ofsSurfaces ) ) {
		return SkbError::Order;
	}
	out.boneData = std::size_t( ofsBones );

	for ( int b = 0; b < out.bones; ++b ) {
		const std::size_t at = out.boneData + SkbBoneBytes * std::size_t( b );
		const std::int32_t parent = reader.Int( at );
		if ( parent == -1 ) {
			++out.roots;
		} else if ( parent < 0 || parent >= b ) {
			/* A parent must be an earlier bone. That is what makes a walk up
			 * the hierarchy terminate without tracking what it has visited. */
			return SkbError::Parent;
		}
		if ( !reader.Name( at + 8 ) ) return SkbError::Name;
	}
	if ( !out.roots ) return SkbError::Parent;

	std::size_t cursor = std::size_t( ofsSurfaces );
	for ( int s = 0; s < out.surfaces; ++s ) {
		if ( !reader.Fits( cursor, SkbSurfaceHeaderBytes ) ) return SkbError::Extent;
		if ( reader.Unsigned( cursor ) != SkbIdent ) return SkbError::Ident;
		if ( !reader.Name( cursor + 4 ) ) return SkbError::Name;

		SkbSurface &surface = out.surface[s];
		surface.header = cursor;
		surface.name = cursor + 4;
		surface.triangles = reader.Int( cursor + 68 );
		surface.verts = reader.Int( cursor + 72 );
		surface.minLod = reader.Int( cursor + 76 );
		const std::int32_t ofsTriangles = reader.Int( cursor + 80 );
		const std::int32_t ofsVerts = reader.Int( cursor + 84 );
		const std::int32_t ofsCollapse = reader.Int( cursor + 88 );
		const std::int32_t ofsSurfaceEnd = reader.Int( cursor + 92 );

		if ( surface.verts <= 0 || surface.verts > SkbMaxVerts ) return SkbError::Counts;
		if ( surface.triangles < 0 || surface.triangles > SkbMaxTriangles ) return SkbError::Counts;
		/* minLod is how many vertices survive at the lowest detail, as it is in
		 * the TAN models: it never exceeds the surface's own vertex count. */
		if ( surface.minLod < 0 || surface.minLod > surface.verts ) return SkbError::Counts;
		if ( ofsSurfaceEnd <= 0 || !reader.Fits( cursor, std::size_t( ofsSurfaceEnd ) ) ) {
			return SkbError::Extent;
		}
		const std::size_t limit = cursor + std::size_t( ofsSurfaceEnd );

		/* Every offset is checked for sign and order before any of them is
		 * widened: a negative one converted to an unsigned index first would
		 * wrap, and the sums below could then agree by accident. */
		if ( ofsTriangles != std::int32_t( SkbSurfaceHeaderBytes ) ) return SkbError::Offset;
		if ( ofsVerts < ofsTriangles || ofsCollapse < ofsVerts
			|| ofsSurfaceEnd < ofsCollapse ) {
			return SkbError::Offset;
		}
		if ( std::size_t( ofsTriangles ) + SkbTriangleBytes * std::size_t( surface.triangles )
			!= std::size_t( ofsVerts ) ) {
			return SkbError::Order;
		}
		if ( std::size_t( ofsCollapse ) + SkbCollapseBytes * std::size_t( surface.verts )
			!= std::size_t( ofsSurfaceEnd ) ) {
			return SkbError::Order;
		}
		surface.triangleData = cursor + std::size_t( ofsTriangles );
		surface.vertexData = cursor + std::size_t( ofsVerts );
		surface.collapseData = cursor + std::size_t( ofsCollapse );

		/* The vertex block has no stride: each vertex carries its own weight
		 * count, so it has to be walked. Landing anywhere but exactly on the
		 * collapse map means the layout is not this one. */
		std::size_t at = surface.vertexData;
		const std::size_t vertexLimit = surface.collapseData;
		for ( int v = 0; v < surface.verts; ++v ) {
			if ( at > vertexLimit || vertexLimit - at < SkbVertexFixedBytes ) {
				return SkbError::Extent;
			}
			const std::int32_t count = reader.Int( at + SkbWeightCountOffset );
			if ( count < 0 || count > out.bones ) return SkbError::Counts;
			const std::size_t span = SkbVertexFixedBytes
				+ SkbWeightBytes * std::size_t( count );
			if ( vertexLimit - at < span ) return SkbError::Extent;
			for ( int w = 0; w < count; ++w ) {
				const std::int32_t bone = reader.Int( at + SkbVertexFixedBytes
					+ SkbWeightBytes * std::size_t( w ) );
				if ( bone < 0 || bone >= out.bones ) return SkbError::Bone;
			}
			surface.weights += count;
			at += span;
		}
		if ( at != vertexLimit ) return SkbError::Order;

		surface.nextSurface = limit;
		cursor = limit;
	}
	/* The surfaces tile the rest of the file exactly. */
	if ( cursor != std::size_t( ofsEnd ) ) return SkbError::Order;
	return SkbError::None;
}

} // namespace model
} // namespace alice
} // namespace fntech3

#endif
