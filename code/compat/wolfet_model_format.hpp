/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_MODEL_FORMAT_HPP
#define FNTECH3_WOLFET_MODEL_FORMAT_HPP

/* Bounded readers for ET's skeletal pair: MDM holds weighted meshes and tags,
 * MDX holds the bones and their compressed per-frame poses. Every offset and
 * count is checked against the supplied buffer before anything is read, so a
 * truncated or hostile file is rejected rather than trusted. Layout facts:
 * code/compat/wolfet_model_layout.def, observations:
 * docs/fntech3/2026-09-18-wolfet-models.md. */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace wolfet {
namespace model {

inline constexpr std::uint32_t MdmIdent = ( 'W' << 24 ) | ( 'M' << 16 ) | ( 'D' << 8 ) | 'M';
inline constexpr std::uint32_t MdxIdent = ( 'W' << 24 ) | ( 'X' << 16 ) | ( 'D' << 8 ) | 'M';
inline constexpr int MdmVersion = 3;
inline constexpr int MdxVersion = 2;

inline constexpr int MdmMaxVerts = 6000;
inline constexpr int MdmMaxTriangles = 8192;
inline constexpr int MdmMaxSurfaces = 32;
inline constexpr int MdmMaxTags = 128;
inline constexpr int MdxMaxBones = 128;

inline constexpr std::size_t NameBytes = 64;
inline constexpr std::size_t MdmHeaderBytes = 100;
inline constexpr std::size_t MdmSurfaceBytes = 176;
inline constexpr std::size_t MdmTagBytes = 128;
inline constexpr std::size_t MdmVertexBytes = 44;
inline constexpr std::size_t MdmWeightBytes = 20;
inline constexpr std::size_t MdmTriangleBytes = 12;
inline constexpr std::size_t MdxHeaderBytes = 96;
inline constexpr std::size_t MdxFrameBytes = 52;
inline constexpr std::size_t MdxBoneFrameBytes = 12;
inline constexpr std::size_t MdxBoneInfoBytes = 80;

enum class Error {
	None, Size, Ident, Version, Counts, Offset, Extent, Name, Weights, Index, Bone
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
	case Error::Weights: return "weight count out of range";
	case Error::Index: return "vertex index out of range";
	case Error::Bone: return "bone index out of range";
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

	std::int16_t Short( std::size_t offset ) const {
		std::int16_t value = 0;
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

struct MdmSurface {
	std::size_t offset = 0;
	int numVerts = 0;
	int numTriangles = 0;
	int numBoneReferences = 0;
	std::size_t vertexOffset = 0;
	std::size_t triangleOffset = 0;
	std::size_t boneReferenceOffset = 0;
};

struct Mdm {
	int version = 0;
	int numSurfaces = 0;
	int numTags = 0;
	int totalVerts = 0;
	int totalTriangles = 0;
	int maxBoneReference = -1;
};

struct Mdx {
	int version = 0;
	int numFrames = 0;
	int numBones = 0;
	int torsoParent = 0;
};

/* Walks every surface, vertex and tag. `bones` is the bone count of the MDX the
 * mesh will be posed with, or zero to skip that cross-check. */
inline Error ParseMdm( const void *data, std::size_t bytes, int bones, Mdm &out ) {
	Reader reader( data, bytes );
	if ( !reader.Fits( 0, MdmHeaderBytes ) ) return Error::Size;
	if ( reader.Unsigned( 0 ) != MdmIdent ) return Error::Ident;
	out = {};
	out.version = reader.Int( 4 );
	if ( out.version != MdmVersion ) return Error::Version;
	if ( !reader.Name( 8 ) ) return Error::Name;

	out.numSurfaces = reader.Int( 80 );
	out.numTags = reader.Int( 88 );
	if ( out.numSurfaces < 0 || out.numSurfaces > MdmMaxSurfaces
		|| out.numTags < 0 || out.numTags > MdmMaxTags ) {
		return Error::Counts;
	}

	const std::int32_t surfaceOffset = reader.Int( 84 );
	const std::int32_t tagOffset = reader.Int( 92 );
	if ( surfaceOffset < 0 || tagOffset < 0 ) return Error::Offset;
	if ( !reader.Fits( static_cast<std::size_t>( surfaceOffset ), MdmSurfaceBytes,
			static_cast<std::size_t>( out.numSurfaces ) ) ) {
		return Error::Extent;
	}

	std::size_t cursor = static_cast<std::size_t>( surfaceOffset );
	for ( int index = 0; index < out.numSurfaces; ++index ) {
		if ( !reader.Fits( cursor, MdmSurfaceBytes ) ) return Error::Extent;
		if ( !reader.Name( cursor + 4 ) || !reader.Name( cursor + 68 ) ) return Error::Name;
		const int verts = reader.Int( cursor + 144 );
		const int triangles = reader.Int( cursor + 152 );
		const int references = reader.Int( cursor + 164 );
		if ( verts < 0 || verts > MdmMaxVerts || triangles < 0 || triangles > MdmMaxTriangles
			|| references < 0 || references > MdxMaxBones ) {
			return Error::Counts;
		}
		const std::int32_t vertexOffset = reader.Int( cursor + 148 );
		const std::int32_t triangleOffset = reader.Int( cursor + 156 );
		const std::int32_t referenceOffset = reader.Int( cursor + 168 );
		if ( vertexOffset < 0 || triangleOffset < 0 || referenceOffset < 0 ) return Error::Offset;

		// Surface offsets are relative to the surface record.
		const std::size_t triangles0 = cursor + static_cast<std::size_t>( triangleOffset );
		if ( !reader.Fits( triangles0, MdmTriangleBytes,
				static_cast<std::size_t>( triangles ) ) ) {
			return Error::Extent;
		}
		for ( int triangle = 0; triangle < triangles; ++triangle ) {
			const std::size_t at = triangles0 + triangle * MdmTriangleBytes;
			for ( int axis = 0; axis < 3; ++axis ) {
				const int vertex = reader.Int( at + axis * 4 );
				if ( vertex < 0 || vertex >= verts ) return Error::Index;
			}
		}

		const std::size_t references0 = cursor + static_cast<std::size_t>( referenceOffset );
		if ( !reader.Fits( references0, 4, static_cast<std::size_t>( references ) ) ) {
			return Error::Extent;
		}
		for ( int reference = 0; reference < references; ++reference ) {
			const int bone = reader.Int( references0 + reference * 4 );
			if ( bone < 0 || bone >= MdxMaxBones ) return Error::Bone;
			if ( bones && bone >= bones ) return Error::Bone;
			if ( bone > out.maxBoneReference ) out.maxBoneReference = bone;
		}

		// Vertices are variable length: one weight record per extra weight.
		std::size_t vertex = cursor + static_cast<std::size_t>( vertexOffset );
		for ( int index2 = 0; index2 < verts; ++index2 ) {
			if ( !reader.Fits( vertex, MdmVertexBytes ) ) return Error::Extent;
			const int weights = reader.Int( vertex + 20 );
			if ( weights < 1 || weights > MdxMaxBones ) return Error::Weights;
			const std::size_t weightBytes = MdmWeightBytes * static_cast<std::size_t>( weights );
			if ( !reader.Fits( vertex + 24, weightBytes ) ) return Error::Extent;
			for ( int weight = 0; weight < weights; ++weight ) {
				const int bone = reader.Int( vertex + 24 + weight * MdmWeightBytes );
				if ( bone < 0 || bone >= MdxMaxBones ) return Error::Bone;
				if ( bones && bone >= bones ) return Error::Bone;
			}
			vertex += 24 + weightBytes;
		}

		out.totalVerts += verts;
		out.totalTriangles += triangles;
		const std::int32_t end = reader.Int( cursor + 172 );
		if ( end <= 0 ) return Error::Offset;
		cursor += static_cast<std::size_t>( end );
		if ( cursor > reader.Size() ) return Error::Extent;
	}

	std::size_t tag = static_cast<std::size_t>( tagOffset );
	for ( int index = 0; index < out.numTags; ++index ) {
		if ( !reader.Fits( tag, MdmTagBytes ) ) return Error::Extent;
		if ( !reader.Name( tag ) ) return Error::Name;
		const int bone = reader.Int( tag + 100 );
		if ( bone < 0 || bone >= MdxMaxBones ) return Error::Bone;
		if ( bones && bone >= bones ) return Error::Bone;
		const int references = reader.Int( tag + 116 );
		const std::int32_t referenceOffset = reader.Int( tag + 120 );
		if ( references < 0 || references > MdxMaxBones || referenceOffset < 0 ) {
			return Error::Counts;
		}
		if ( !reader.Fits( tag + static_cast<std::size_t>( referenceOffset ), 4,
				static_cast<std::size_t>( references ) ) ) {
			return Error::Extent;
		}
		const std::int32_t end = reader.Int( tag + 124 );
		if ( end <= 0 ) return Error::Offset;
		tag += static_cast<std::size_t>( end );
		if ( tag > reader.Size() ) return Error::Extent;
	}
	return Error::None;
}

/* Walks the bone table and every compressed pose. */
inline Error ParseMdx( const void *data, std::size_t bytes, Mdx &out ) {
	Reader reader( data, bytes );
	if ( !reader.Fits( 0, MdxHeaderBytes ) ) return Error::Size;
	if ( reader.Unsigned( 0 ) != MdxIdent ) return Error::Ident;
	out = {};
	out.version = reader.Int( 4 );
	if ( out.version != MdxVersion ) return Error::Version;
	if ( !reader.Name( 8 ) ) return Error::Name;

	out.numFrames = reader.Int( 72 );
	out.numBones = reader.Int( 76 );
	out.torsoParent = reader.Int( 88 );
	if ( out.numFrames < 0 || out.numBones < 0 || out.numBones > MdxMaxBones ) {
		return Error::Counts;
	}
	// The torso parent is a bone index, or -1 when the model has no torso.
	if ( out.torsoParent < -1 || out.torsoParent >= out.numBones ) return Error::Bone;

	const std::int32_t frameOffset = reader.Int( 80 );
	const std::int32_t boneOffset = reader.Int( 84 );
	if ( frameOffset < 0 || boneOffset < 0 ) return Error::Offset;

	// Each frame is a bounds record followed by one compressed pose per bone.
	const std::size_t frameBytes =
		MdxFrameBytes + MdxBoneFrameBytes * static_cast<std::size_t>( out.numBones );
	if ( !reader.Fits( static_cast<std::size_t>( frameOffset ), frameBytes,
			static_cast<std::size_t>( out.numFrames ) ) ) {
		return Error::Extent;
	}
	if ( !reader.Fits( static_cast<std::size_t>( boneOffset ), MdxBoneInfoBytes,
			static_cast<std::size_t>( out.numBones ) ) ) {
		return Error::Extent;
	}

	for ( int bone = 0; bone < out.numBones; ++bone ) {
		const std::size_t at = static_cast<std::size_t>( boneOffset ) + bone * MdxBoneInfoBytes;
		if ( !reader.Name( at ) ) return Error::Name;
		const int parent = reader.Int( at + 64 );
		// A root bone has no parent, and a parent always precedes its child.
		if ( parent < -1 || parent >= out.numBones || parent >= bone ) return Error::Bone;
	}
	return Error::None;
}

} // namespace model
} // namespace wolfet
} // namespace fntech3

#endif
