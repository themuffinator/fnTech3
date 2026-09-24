/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_TAN_FORMAT_HPP
#define FNTECH3_COMPAT_ALICE_TAN_FORMAT_HPP

/* Bounded reader for Alice's `TAN ` vertex-animation models.
 *
 * A `.tan` holds one model's frames and its surfaces together: every surface
 * stores a full set of positions for every frame, quantized to three unsigned
 * shorts against that frame's own scale and offset, with a packed normal. The
 * surfaces form a chain, each declaring where the next begins, and up to
 * sixteen tags each carry a name followed by one orientation per frame.
 *
 * Every offset and count is checked against the supplied buffer before
 * anything is read, so a truncated or hostile file is rejected rather than
 * trusted. Nothing here decodes a frame, poses a model or draws anything.
 *
 * Layout and ordering come from Ritual's published F.A.K.K.2 v1.02 SDK
 * (qcommon/qfiles.h), used under its own non-free licence for record names and
 * layouts only, and every record was re-measured against the 694 `.tan` files
 * a retail Alice installation ships before being written here. No
 * implementation text is incorporated. See
 * docs/fntech3/2026-09-22-alice-models.md. */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace alice {
namespace model {

/* Ritual's identifier spells "TAN " in file order. Alice ships version 2 and
 * nothing else; the ident alone does not identify the format, because the same
 * lineage reuses its idents across titles. */
inline constexpr std::uint32_t TanIdent =
	( ' ' << 24 ) | ( 'N' << 16 ) | ( 'A' << 8 ) | 'T';
inline constexpr int TanVersion = 2;

inline constexpr std::size_t NameBytes = 64;
inline constexpr std::size_t TanHeaderBytes = 176;
inline constexpr std::size_t SurfaceHeaderBytes = 104;
/* Seventeen floats: bounds (six), scale, offset and delta (three each), radius
 * and frame time. Every one of the 694 retail files spaces its frames 68 bytes
 * apart; the value was 64 here until that was measured, which only loosened
 * the bounds check on the frame array. */
inline constexpr std::size_t FrameBytes = 68;
inline constexpr std::size_t TriangleBytes = 12;    /* three vertex indices */
inline constexpr std::size_t TexCoordBytes = 8;     /* s, t */
inline constexpr std::size_t XyzNormalBytes = 8;    /* three shorts and a packed normal */
inline constexpr std::size_t CollapseBytes = 4;     /* one index per vertex */
inline constexpr std::size_t TagDataBytes = 48;     /* origin and a three-vector axis */

/* The compiler's own limits, which a retail file never exceeds. Reading a file
 * that claims more is refused rather than clamped: it is not this format. */
inline constexpr int MaxTriangles = 4096;
inline constexpr int MaxVerts = 1200;
inline constexpr int MaxFrames = 2048;
inline constexpr int MaxSurfaces = 32;
inline constexpr int MaxTags = 16;

enum class Error {
	None, Size, Ident, Version, Counts, Offset, Extent, Name, Order, Frames
};

inline const char *ErrorName( Error error ) {
	switch ( error ) {
	case Error::None: return "none";
	case Error::Size: return "file too small";
	case Error::Ident: return "wrong identifier";
	case Error::Version: return "wrong version";
	case Error::Counts: return "count out of range";
	case Error::Offset: return "offset out of range";
	case Error::Extent: return "record past end of file";
	case Error::Name: return "unterminated name";
	case Error::Order: return "record out of order";
	case Error::Frames: return "surface frame count disagrees with the model";
	}
	return "unknown";
}

class Reader {
public:
	Reader( const void *data, std::size_t bytes )
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

	float Float( std::size_t offset ) const {
		float value = 0;
		std::memcpy( &value, bytes_ + offset, sizeof( value ) );
		return value;
	}

	// A fixed-width name field must be terminated inside itself.
	bool Name( std::size_t offset ) const {
		return std::memchr( bytes_ + offset, 0, NameBytes ) != nullptr;
	}

private:
	const std::uint8_t *bytes_;
	std::size_t size_;
};

struct TanSurface {
	std::size_t header;         /* where this surface's record begins */
	std::size_t name;           /* its name field */
	int verts;
	int triangles;
	int minLod;
	std::size_t triangleData;
	std::size_t texCoordData;
	std::size_t xyzNormalData;
	std::size_t collapseData;
	std::size_t nextSurface;    /* absolute, so a walk cannot loop */
};

struct TanTag {
	std::size_t name;
	std::size_t data;           /* one orientation per frame */
};

struct Tan {
	std::size_t name;
	int frames;
	int tags;
	int surfaces;
	float totalTime;
	std::size_t frameData;
	TanSurface surface[MaxSurfaces];
	TanTag tag[MaxTags];
};

/*
==================
ParseTan

Read a `TAN ` 2 model's directory. Every record is located and bounded; none
is decoded. A file that disagrees with itself anywhere is refused whole.
==================
*/
inline Error ParseTan( const void *data, std::size_t bytes, Tan &out ) {
	out = Tan{};
	const Reader reader( data, bytes );
	if ( reader.Size() < TanHeaderBytes ) return Error::Size;
	if ( reader.Unsigned( 0 ) != TanIdent ) return Error::Ident;
	if ( reader.Int( 4 ) != TanVersion ) return Error::Version;
	if ( !reader.Name( 8 ) ) return Error::Name;
	out.name = 8;

	out.frames = reader.Int( 72 );
	out.tags = reader.Int( 76 );
	out.surfaces = reader.Int( 80 );
	out.totalTime = reader.Float( 84 );
	if ( out.frames <= 0 || out.frames > MaxFrames ) return Error::Counts;
	if ( out.tags < 0 || out.tags > MaxTags ) return Error::Counts;
	if ( out.surfaces < 0 || out.surfaces > MaxSurfaces ) return Error::Counts;

	const std::int32_t ofsFrames = reader.Int( 100 );
	const std::int32_t ofsSurfaces = reader.Int( 104 );
	const std::int32_t ofsEnd = reader.Int( 172 );
	/* The file declares its own length, which is what makes a truncated one
	 * detectable before any record is read. */
	if ( ofsEnd < 0 || std::size_t( ofsEnd ) != reader.Size() ) return Error::Extent;

	if ( ofsFrames < std::int32_t( TanHeaderBytes )
		|| !reader.Fits( std::size_t( ofsFrames ), FrameBytes, std::size_t( out.frames ) ) ) {
		return Error::Offset;
	}
	out.frameData = std::size_t( ofsFrames );

	for ( int i = 0; i < out.tags; ++i ) {
		const std::int32_t offset = reader.Int( 108 + 4 * std::size_t( i ) );
		if ( offset < std::int32_t( TanHeaderBytes )
			|| !reader.Fits( std::size_t( offset ), NameBytes ) ) return Error::Offset;
		if ( !reader.Name( std::size_t( offset ) ) ) return Error::Name;
		const std::size_t dataAt = std::size_t( offset ) + NameBytes;
		if ( !reader.Fits( dataAt, TagDataBytes, std::size_t( out.frames ) ) ) return Error::Extent;
		out.tag[i].name = std::size_t( offset );
		out.tag[i].data = dataAt;
	}

	std::size_t cursor = std::size_t( ofsSurfaces );
	if ( out.surfaces ) {
		if ( ofsSurfaces < std::int32_t( TanHeaderBytes )
			|| !reader.Fits( cursor, SurfaceHeaderBytes ) ) return Error::Offset;
	}
	for ( int s = 0; s < out.surfaces; ++s ) {
		if ( !reader.Fits( cursor, SurfaceHeaderBytes ) ) return Error::Extent;
		if ( !reader.Name( cursor + 4 ) ) return Error::Name;

		TanSurface &surface = out.surface[s];
		surface.header = cursor;
		surface.name = cursor + 4;
		const std::int32_t surfaceFrames = reader.Int( cursor + 68 );
		surface.verts = reader.Int( cursor + 72 );
		surface.minLod = reader.Int( cursor + 76 );
		surface.triangles = reader.Int( cursor + 80 );
		/* Every surface animates over the model's own frames; one that does
		 * not is a file whose parts came from different models. */
		if ( surfaceFrames != out.frames ) return Error::Frames;
		if ( surface.verts <= 0 || surface.verts > MaxVerts ) return Error::Counts;
		if ( surface.triangles < 0 || surface.triangles > MaxTriangles ) return Error::Counts;
		/* minLod is how many vertices survive at the lowest detail, not a
		 * level number: across the installed models it runs from 0 to 381 and
		 * never exceeds the surface's own vertex count. */
		if ( surface.minLod < 0 || surface.minLod > surface.verts ) return Error::Counts;

		const std::int32_t ofsSurfaceEnd = reader.Int( cursor + 100 );
		if ( ofsSurfaceEnd <= 0 || !reader.Fits( cursor, std::size_t( ofsSurfaceEnd ) ) ) {
			return Error::Extent;
		}
		const std::size_t limit = cursor + std::size_t( ofsSurfaceEnd );

		struct Block { std::int32_t offset; std::size_t stride; std::size_t count; std::size_t *out; };
		const Block blocks[] = {
			{ reader.Int( cursor + 84 ), TriangleBytes, std::size_t( surface.triangles ), &surface.triangleData },
			{ reader.Int( cursor + 88 ), CollapseBytes, std::size_t( surface.verts ), &surface.collapseData },
			{ reader.Int( cursor + 92 ), TexCoordBytes, std::size_t( surface.verts ), &surface.texCoordData },
			{ reader.Int( cursor + 96 ), XyzNormalBytes,
				std::size_t( surface.verts ) * std::size_t( out.frames ), &surface.xyzNormalData },
		};
		for ( const Block &block : blocks ) {
			if ( block.offset < 0 ) return Error::Offset;
			const std::size_t at = cursor + std::size_t( block.offset );
			if ( !reader.Fits( at, block.stride, block.count ) ) return Error::Extent;
			/* A block must stay inside the surface that declares it, or one
			 * surface's vertices would be read out of the next one. */
			if ( at < cursor || at + block.stride * block.count > limit ) return Error::Order;
			*block.out = at;
		}

		surface.nextSurface = limit;
		cursor = limit;
	}
	return Error::None;
}

} // namespace model
} // namespace alice
} // namespace fntech3

#endif
