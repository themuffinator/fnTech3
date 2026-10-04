/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_MODEL_FORMAT_HPP
#define FNTECH3_COMPAT_JKA_MODEL_FORMAT_HPP

/* Bounded readers for Jedi Academy's Ghoul2 pair: a `.glm` mesh holds the
 * weighted surfaces and their hierarchy, and the `.gla` skeleton it names holds
 * the bones and a pool of compressed poses the frames index into. Every offset
 * and count is checked against the supplied buffer before anything is read, so
 * a truncated or hostile file is rejected rather than trusted.
 *
 * Layout and traversal facts come from Raven's released sources as carried by
 * OpenJK: codemp/rd-common/mdx_format.h:42-441 for the records, and
 * codemp/rd-vanilla/tr_model.cpp:740-1030 for how the retail renderer walks
 * them. No implementation text is incorporated, and nothing here draws
 * anything. See docs/fntech3/2026-09-21-jka-models.md. */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace fntech3 {
namespace jka {
namespace model {

/* Raven's identifiers spell "2LGM" and "2LGA" in file order. */
inline constexpr std::uint32_t MeshIdent = ( 'M' << 24 ) | ( 'G' << 16 ) | ( 'L' << 8 ) | '2';
inline constexpr std::uint32_t SkeletonIdent = ( 'A' << 24 ) | ( 'G' << 16 ) | ( 'L' << 8 ) | '2';
inline constexpr int MeshVersion = 6;
inline constexpr int SkeletonVersion = 6;

inline constexpr std::size_t NameBytes = 64;
inline constexpr std::size_t MeshHeaderBytes = 164;
inline constexpr std::size_t SurfHierarchyBytes = 144;   /* before childIndexes */
inline constexpr std::size_t LodHeaderBytes = 4;
inline constexpr std::size_t SurfaceBytes = 40;
inline constexpr std::size_t TriangleBytes = 12;
inline constexpr std::size_t VertexBytes = 32;
inline constexpr std::size_t TexCoordBytes = 8;
inline constexpr std::size_t SkeletonHeaderBytes = 100;
inline constexpr std::size_t BoneBytes = 172;            /* before children */
inline constexpr std::size_t FrameIndexBytes = 3;
inline constexpr std::size_t CompressedBoneBytes = 14;

/* A compressed record contains seven little-endian words: quaternion w,x,y,z
 * and three translation components. Retail jamp.exe's decompressor at
 * RVA0x86d30 uses word/16383-2 for rotation and word/64-512 for translation.
 * The three-byte pool index and fourteen-byte stride are confirmed at
 * VA0x497a19/0x497a23. Do not normalize away the encoded quaternion residual.
 *
 * Each decoded matrix is a parent-relative deformation, not an independent
 * model-global displacement. Retail engine-query observations supersede the
 * earlier rigidity-based inference: frame-zero defaults, inherited timing,
 * parent composition and bolt extraction are implemented in ghoul2_pose.hpp.
 * A bone's base pose is applied for raw bone bolts; weighted vertices use the
 * deformation palette directly. See tests/jka_pose_retail_cases.hpp for the
 * ninety recorded engine queries, with installed assets supplied separately. */
inline constexpr float PoseRotationDivisor = 16383.0f;
inline constexpr float PoseRotationOffset = 2.0f;
inline constexpr float PoseTranslationDivisor = 64.0f;
inline constexpr float PoseTranslationOffset = 512.0f;

struct CompressedPose {
	/* w, x, y, z, decoded. */
	float rotation[4];
	/* Translation in the parent-relative deformation, in game units. */
	float translation[3];
};

/* Decode one pose from fourteen bytes. The caller has already bounds-checked
 * the pool; this does arithmetic only. */
inline CompressedPose DecodePose( const unsigned char *bytes ) {
	CompressedPose out{};
	for ( int i = 0; i < 4; ++i ) {
		const int raw = int( bytes[i * 2] ) | ( int( bytes[i * 2 + 1] ) << 8 );
		out.rotation[i] = float( raw ) / PoseRotationDivisor - PoseRotationOffset;
	}
	for ( int i = 0; i < 3; ++i ) {
		const int raw = int( bytes[8 + i * 2] ) | ( int( bytes[9 + i * 2] ) << 8 );
		out.translation[i] = float( raw ) / PoseTranslationDivisor - PoseTranslationOffset;
	}
	return out;
}

/* A pose as a three-by-four, row major, translation in the fourth column --
 * the same shape the skeleton states its rest matrices in. */
inline void PoseMatrix( const CompressedPose &pose, float out[12] ) {
	const float w = pose.rotation[0], x = pose.rotation[1];
	const float y = pose.rotation[2], z = pose.rotation[3];
	out[0] = 1.0f - 2.0f * ( y * y + z * z );
	out[1] = 2.0f * ( x * y - w * z );
	out[2] = 2.0f * ( x * z + w * y );
	out[3] = pose.translation[0];
	out[4] = 2.0f * ( x * y + w * z );
	out[5] = 1.0f - 2.0f * ( x * x + z * z );
	out[6] = 2.0f * ( y * z - w * x );
	out[7] = pose.translation[1];
	out[8] = 2.0f * ( x * z - w * y );
	out[9] = 2.0f * ( y * z + w * x );
	out[10] = 1.0f - 2.0f * ( x * x + y * y );
	out[11] = pose.translation[2];
}

/* Compose two of those, left applied to right. The result is written as it is
 * worked out, so it must not be either input. */
inline void ComposeMatrix( const float left[12], const float right[12], float out[12] ) {
	for ( int row = 0; row < 3; ++row ) {
		for ( int col = 0; col < 3; ++col ) {
			out[row * 4 + col] = left[row * 4] * right[col]
				+ left[row * 4 + 1] * right[4 + col]
				+ left[row * 4 + 2] * right[8 + col];
		}
		out[row * 4 + 3] = left[row * 4] * right[3]
			+ left[row * 4 + 1] * right[7]
			+ left[row * 4 + 2] * right[11]
			+ left[row * 4 + 3];
	}
}

/* A surface names at most 32 bones because a vertex references one of them in
 * five bits, and weights at most four of them. */
inline constexpr int BoneReferenceBits = 5;
inline constexpr int MaxBoneReferences = 1 << BoneReferenceBits;
inline constexpr int MaxWeightsPerVertex = 4;
inline constexpr int WeightTopBitsShift = BoneReferenceBits * MaxWeightsPerVertex - 8;
inline constexpr std::uint32_t WeightTopBitsMask = 0x300;
inline constexpr float WeightReciprocal = 1.0f / 1023.0f;

enum class Error {
	None, Size, Ident, Version, Counts, Offset, Extent, Name, Order,
	Hierarchy, Weights, Index, Bone
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
	case Error::Hierarchy: return "hierarchy index out of range";
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

	float Float( std::size_t offset ) const {
		float value = 0;
		std::memcpy( &value, bytes_ + offset, sizeof( value ) );
		return value;
	}

	std::uint8_t Byte( std::size_t offset ) const { return bytes_[offset]; }

	// Raven stores a pose index in three bytes, least significant first.
	std::uint32_t Index24( std::size_t offset ) const {
		return std::uint32_t( bytes_[offset] )
			| ( std::uint32_t( bytes_[offset + 1] ) << 8 )
			| ( std::uint32_t( bytes_[offset + 2] ) << 16 );
	}

	// A fixed-width name field must be terminated inside itself.
	bool Name( std::size_t offset ) const {
		return std::memchr( bytes_ + offset, 0, NameBytes ) != nullptr;
	}

private:
	const std::uint8_t *bytes_;
	std::size_t size_;
};

/* A vertex packs its weight count, its bone references and the high bits of
 * its weights into one word; the last weight is whatever is left over. */
inline int WeightCount( std::uint32_t packed ) {
	return int( packed >> 30 ) + 1;
}

inline int WeightBoneReference( std::uint32_t packed, int weight ) {
	return int( ( packed >> ( BoneReferenceBits * weight ) )
		& ( ( 1u << BoneReferenceBits ) - 1u ) );
}

inline float WeightValue( std::uint32_t packed, const std::uint8_t weights[MaxWeightsPerVertex],
	int weight, int count, float &running ) {
	if ( weight == count - 1 ) return 1.0f - running;
	const std::uint32_t low = weights[weight];
	const std::uint32_t high = ( packed >> ( WeightTopBitsShift + weight * 2 ) ) & WeightTopBitsMask;
	const float value = WeightReciprocal * float( low | high );
	running += value;
	return value;
}

struct MeshSurface {
	std::size_t offset = 0;
	int numVerts = 0;
	int numTriangles = 0;
	int numBoneReferences = 0;
	std::size_t vertexOffset = 0;
	std::size_t texCoordOffset = 0;
	std::size_t triangleOffset = 0;
	std::size_t boneReferenceOffset = 0;
};

struct Mesh {
	int version = 0;
	int numBones = 0;
	int numLODs = 0;
	int numSurfaces = 0;
	int totalVerts = 0;
	int totalTriangles = 0;
	int maxBoneReference = -1;
	int maxWeightsPerVertex = 0;
	std::size_t animNameOffset = 0;
	/* Where the levels of detail begin, checked against the buffer by the
	 * time it is set, so a surface can be walked to afterwards. */
	std::size_t lodOffset = 0;
};

struct Skeleton {
	int version = 0;
	int numFrames = 0;
	int numBones = 0;
	float scale = 0;
	int maxPoseIndex = -1;
	int maxBoneChildren = 0;
	/* Where the frame indices and the pose pool begin, both already checked
	 * against the buffer by the time they are set. */
	std::size_t frameOffset = 0;
	std::size_t poseOffset = 0;
};

/* Walk a `.glm`: its surface hierarchy, then every level of detail, surface,
 * triangle and vertex. `skeletonBones` is the bone count of the `.gla` this
 * mesh names, or zero to skip that cross-check. */
inline Error ParseMesh( const void *data, std::size_t bytes, int skeletonBones, Mesh &out ) {
	Reader reader( data, bytes );
	out = {};
	if ( !reader.Fits( 0, MeshHeaderBytes ) ) return Error::Size;
	if ( reader.Unsigned( 0 ) != MeshIdent ) return Error::Ident;
	out.version = reader.Int( 4 );
	if ( out.version != MeshVersion ) return Error::Version;
	if ( !reader.Name( 8 ) || !reader.Name( 72 ) ) return Error::Name;
	out.animNameOffset = 72;

	out.numBones = reader.Int( 140 );
	out.numLODs = reader.Int( 144 );
	out.numSurfaces = reader.Int( 152 );
	const std::int32_t ofsLODs = reader.Int( 148 );
	const std::int32_t ofsSurfHierarchy = reader.Int( 156 );
	const std::int32_t ofsEnd = reader.Int( 160 );
	if ( out.numBones < 0 || out.numLODs < 0 || out.numSurfaces < 0 ) return Error::Counts;
	if ( skeletonBones && out.numBones > skeletonBones ) return Error::Bone;
	if ( ofsEnd < 0 || std::size_t( ofsEnd ) > reader.Size() ) return Error::Extent;
	/* The offset table sits straight after the header, where the retail
	 * renderer reads it, and the stated offset points past that table at the
	 * first hierarchy record. */
	if ( !reader.Fits( MeshHeaderBytes, sizeof( std::int32_t ), std::size_t( out.numSurfaces ) ) ) {
		return Error::Extent;
	}
	if ( ofsSurfHierarchy < 0 || std::size_t( ofsSurfHierarchy )
		!= MeshHeaderBytes + std::size_t( out.numSurfaces ) * sizeof( std::int32_t ) ) {
		return Error::Offset;
	}

	/* Each hierarchy record is followed by its children, so the records are
	 * variable sized and walked in order; the offset table must agree. */
	std::size_t record = MeshHeaderBytes
		+ std::size_t( out.numSurfaces ) * sizeof( std::int32_t );
	for ( int i = 0; i < out.numSurfaces; ++i ) {
		const std::int32_t stated = reader.Int( MeshHeaderBytes + std::size_t( i ) * 4 );
		if ( stated < 0 || MeshHeaderBytes + std::size_t( stated ) != record ) return Error::Order;
		if ( !reader.Fits( record, SurfHierarchyBytes ) ) return Error::Extent;
		if ( !reader.Name( record ) || !reader.Name( record + 68 ) ) return Error::Name;
		const std::int32_t parent = reader.Int( record + 136 );
		const std::int32_t children = reader.Int( record + 140 );
		if ( parent < -1 || parent >= out.numSurfaces ) return Error::Hierarchy;
		if ( children < 0 || children > out.numSurfaces ) return Error::Counts;
		if ( !reader.Fits( record + SurfHierarchyBytes, sizeof( std::int32_t ),
			std::size_t( children ) ) ) return Error::Extent;
		for ( int child = 0; child < children; ++child ) {
			const std::int32_t index = reader.Int( record + SurfHierarchyBytes
				+ std::size_t( child ) * 4 );
			if ( index < 0 || index >= out.numSurfaces ) return Error::Hierarchy;
		}
		record += SurfHierarchyBytes + std::size_t( children ) * 4;
	}

	if ( ofsLODs < 0 || std::size_t( ofsLODs ) < record
		|| !reader.Fits( std::size_t( ofsLODs ), LodHeaderBytes ) ) return Error::Offset;

	out.lodOffset = std::size_t( ofsLODs );
	std::size_t lod = std::size_t( ofsLODs );
	for ( int level = 0; level < out.numLODs; ++level ) {
		if ( !reader.Fits( lod, LodHeaderBytes ) ) return Error::Extent;
		const std::int32_t lodEnd = reader.Int( lod );
		const std::size_t table = lod + LodHeaderBytes;
		if ( !reader.Fits( table, sizeof( std::int32_t ), std::size_t( out.numSurfaces ) ) ) {
			return Error::Extent;
		}
		std::size_t surface = table + std::size_t( out.numSurfaces ) * sizeof( std::int32_t );
		for ( int i = 0; i < out.numSurfaces; ++i ) {
			if ( !reader.Fits( surface, SurfaceBytes ) ) return Error::Extent;
			MeshSurface record2;
			record2.offset = surface;
			if ( reader.Int( surface + 4 ) != i ) return Error::Order;
			/* The surface points back at the file's own header. */
			const std::int32_t ofsHeader = reader.Int( surface + 8 );
			if ( std::int64_t( surface ) + ofsHeader != 0 ) return Error::Offset;
			record2.numVerts = reader.Int( surface + 12 );
			record2.numTriangles = reader.Int( surface + 20 );
			record2.numBoneReferences = reader.Int( surface + 28 );
			const std::int32_t ofsVerts = reader.Int( surface + 16 );
			const std::int32_t ofsTriangles = reader.Int( surface + 24 );
			const std::int32_t ofsBoneReferences = reader.Int( surface + 32 );
			const std::int32_t surfaceEnd = reader.Int( surface + 36 );
			if ( record2.numVerts < 0 || record2.numTriangles < 0
				|| record2.numBoneReferences < 0
				|| record2.numBoneReferences > MaxBoneReferences ) return Error::Counts;
			if ( ofsVerts < 0 || ofsTriangles < 0 || ofsBoneReferences < 0 ) return Error::Offset;
			/* Bring each offset inside the file before adding it to the
			 * surface's own position, so the sum cannot wrap where a size_t
			 * is 32 bits, which is this engine's own target. */
			if ( !reader.Fits( surface, std::size_t( ofsVerts ) )
				|| !reader.Fits( surface, std::size_t( ofsTriangles ) )
				|| !reader.Fits( surface, std::size_t( ofsBoneReferences ) ) ) return Error::Extent;
			record2.vertexOffset = surface + std::size_t( ofsVerts );
			record2.triangleOffset = surface + std::size_t( ofsTriangles );
			record2.boneReferenceOffset = surface + std::size_t( ofsBoneReferences );
			if ( !reader.Fits( record2.vertexOffset, VertexBytes, std::size_t( record2.numVerts ) )
				|| !reader.Fits( record2.triangleOffset, TriangleBytes, std::size_t( record2.numTriangles ) )
				|| !reader.Fits( record2.boneReferenceOffset, sizeof( std::int32_t ),
					std::size_t( record2.numBoneReferences ) ) ) return Error::Extent;
			record2.texCoordOffset = record2.vertexOffset
				+ std::size_t( record2.numVerts ) * VertexBytes;
			if ( !reader.Fits( record2.texCoordOffset, TexCoordBytes,
				std::size_t( record2.numVerts ) ) ) return Error::Extent;

			for ( int bone = 0; bone < record2.numBoneReferences; ++bone ) {
				const std::int32_t reference = reader.Int( record2.boneReferenceOffset
					+ std::size_t( bone ) * 4 );
				if ( reference < 0 || ( skeletonBones && reference >= skeletonBones ) ) {
					return Error::Bone;
				}
				if ( reference > out.maxBoneReference ) out.maxBoneReference = reference;
			}
			for ( int triangle = 0; triangle < record2.numTriangles; ++triangle ) {
				for ( int corner = 0; corner < 3; ++corner ) {
					const std::int32_t index = reader.Int( record2.triangleOffset
						+ std::size_t( triangle ) * TriangleBytes + std::size_t( corner ) * 4 );
					if ( index < 0 || index >= record2.numVerts ) return Error::Index;
				}
			}
			for ( int vertex = 0; vertex < record2.numVerts; ++vertex ) {
				const std::size_t at = record2.vertexOffset + std::size_t( vertex ) * VertexBytes;
				const std::uint32_t packed = reader.Unsigned( at + 24 );
				const int count = WeightCount( packed );
				if ( count < 1 || count > MaxWeightsPerVertex ) return Error::Weights;
				if ( count > out.maxWeightsPerVertex ) out.maxWeightsPerVertex = count;
				for ( int weight = 0; weight < count; ++weight ) {
					if ( WeightBoneReference( packed, weight ) >= record2.numBoneReferences ) {
						return Error::Bone;
					}
				}
			}
			if ( record2.numVerts > std::numeric_limits<int>::max() - out.totalVerts
				|| record2.numTriangles > std::numeric_limits<int>::max() - out.totalTriangles ) {
				return Error::Counts;
			}
			out.totalVerts += record2.numVerts;
			out.totalTriangles += record2.numTriangles;

			/* Surfaces follow one another, so the step must move forward and
			 * stay inside this level of detail. */
			if ( surfaceEnd <= 0 ) return Error::Order;
			if ( !reader.Fits( surface, std::size_t( surfaceEnd ) ) ) return Error::Extent;
			surface += std::size_t( surfaceEnd );
		}
		if ( lodEnd <= int( LodHeaderBytes ) ) return Error::Order;
		if ( !reader.Fits( lod, std::size_t( lodEnd ) ) ) return Error::Extent;
		const std::size_t nextLod = lod + std::size_t( lodEnd );
		if ( nextLod < surface ) return Error::Extent;
		lod = nextLod;
	}
	return Error::None;
}

/* Walk a `.gla`: its bones, the pose index every frame holds for each of them,
 * and the compressed pose pool those indices reach into. */
inline Error ParseSkeleton( const void *data, std::size_t bytes, Skeleton &out ) {
	Reader reader( data, bytes );
	out = {};
	if ( !reader.Fits( 0, SkeletonHeaderBytes ) ) return Error::Size;
	if ( reader.Unsigned( 0 ) != SkeletonIdent ) return Error::Ident;
	out.version = reader.Int( 4 );
	if ( out.version != SkeletonVersion ) return Error::Version;
	if ( !reader.Name( 8 ) ) return Error::Name;
	out.scale = reader.Float( 72 );
	out.numFrames = reader.Int( 76 );
	out.numBones = reader.Int( 84 );
	const std::int32_t ofsFrames = reader.Int( 80 );
	const std::int32_t ofsCompBonePool = reader.Int( 88 );
	const std::int32_t ofsSkel = reader.Int( 92 );
	const std::int32_t ofsEnd = reader.Int( 96 );
	if ( out.numFrames < 0 || out.numBones < 0 ) return Error::Counts;
	if ( ofsEnd < 0 || std::size_t( ofsEnd ) > reader.Size() ) return Error::Extent;
	/* As with a mesh, the bone offsets sit straight after the header and the
	 * stated offset points past them at the first bone record. */
	if ( !reader.Fits( SkeletonHeaderBytes, sizeof( std::int32_t ),
		std::size_t( out.numBones ) ) ) return Error::Extent;
	if ( ofsSkel < 0 || std::size_t( ofsSkel )
		!= SkeletonHeaderBytes + std::size_t( out.numBones ) * sizeof( std::int32_t ) ) {
		return Error::Offset;
	}

	for ( int i = 0; i < out.numBones; ++i ) {
		const std::int32_t stated = reader.Int( SkeletonHeaderBytes + std::size_t( i ) * 4 );
		if ( stated < 0 ) return Error::Offset;
		const std::size_t record = SkeletonHeaderBytes + std::size_t( stated );
		if ( !reader.Fits( record, BoneBytes ) ) return Error::Extent;
		if ( !reader.Name( record ) ) return Error::Name;
		const std::int32_t parent = reader.Int( record + 68 );
		const std::int32_t children = reader.Int( record + 168 );
		if ( parent < -1 || parent >= out.numBones ) return Error::Hierarchy;
		if ( children < 0 || children > out.numBones ) return Error::Counts;
		if ( !reader.Fits( record + BoneBytes, sizeof( std::int32_t ),
			std::size_t( children ) ) ) return Error::Extent;
		for ( int child = 0; child < children; ++child ) {
			const std::int32_t index = reader.Int( record + BoneBytes + std::size_t( child ) * 4 );
			if ( index < 0 || index >= out.numBones ) return Error::Hierarchy;
		}
		if ( children > out.maxBoneChildren ) out.maxBoneChildren = children;
	}

	if ( ofsFrames < 0 ) return Error::Offset;
	const std::size_t frames = std::size_t( ofsFrames );
	if ( out.numFrames && out.numBones ) {
		if ( std::size_t( out.numFrames ) > reader.Size() / ( std::size_t( out.numBones ) * FrameIndexBytes ) ) {
			return Error::Extent;
		}
	}
	if ( !reader.Fits( frames, FrameIndexBytes,
		std::size_t( out.numFrames ) * std::size_t( out.numBones ) ) ) return Error::Extent;
	for ( std::size_t entry = 0;
		entry < std::size_t( out.numFrames ) * std::size_t( out.numBones ); ++entry ) {
		const std::uint32_t index = reader.Index24( frames + entry * FrameIndexBytes );
		if ( int( index ) > out.maxPoseIndex ) out.maxPoseIndex = int( index );
	}

	if ( ofsCompBonePool < 0 ) return Error::Offset;
	if ( out.maxPoseIndex >= 0 && !reader.Fits( std::size_t( ofsCompBonePool ),
		CompressedBoneBytes, std::size_t( out.maxPoseIndex ) + 1 ) ) return Error::Extent;
	out.frameOffset = frames;
	out.poseOffset = std::size_t( ofsCompBonePool );
	return Error::None;
}

/* The names a bolt is looked up by.
 *
 * Both readers already prove that every one of these fields is terminated
 * inside its own sixty-four bytes, and both walk the same offset arrays to
 * reach the records: the bone offsets sit straight after the skeleton header
 * and the surface offsets straight after the mesh header, each pointing at a
 * record whose name is its first field. So these read what was already
 * checked, and report false rather than reach for a record that is not there.
 *
 * The name is copied out rather than pointed at, because the file it lives in
 * is not kept once a model is loaded. */

/* One of the skeleton's bone names. */
inline bool BoneName( const void *data, std::size_t bytes, const Skeleton &skeleton,
	int index, char *out, std::size_t capacity )
{
	if ( !out || capacity == 0 ) return false;
	out[0] = 0;
	if ( !data || index < 0 || index >= skeleton.numBones ) return false;
	Reader reader( data, bytes );
	if ( !reader.Fits( SkeletonHeaderBytes, sizeof( std::int32_t ),
		std::size_t( index ) + 1 ) ) return false;
	const std::int32_t stated = reader.Int( SkeletonHeaderBytes + std::size_t( index ) * 4 );
	if ( stated < 0 ) return false;
	const std::size_t record = SkeletonHeaderBytes + std::size_t( stated );
	if ( !reader.Fits( record, BoneBytes ) || !reader.Name( record ) ) return false;
	const char *name = static_cast<const char *>( data ) + record;
	std::size_t at = 0;
	while ( at + 1 < capacity && at < NameBytes && name[at] ) { out[at] = name[at]; ++at; }
	out[at] = 0;
	return at != 0;
}

/* One bone's rest matrix, as the skeleton states it: where that bone sits in
 * the model when nothing has moved it. Row major, translation in the fourth
 * column, reached through the same offset array BoneName walks. */
inline bool BoneRestMatrix( const void *data, std::size_t bytes,
	const Skeleton &skeleton, int index, float out[12] )
{
	if ( !out ) return false;
	for ( int i = 0; i < 12; ++i ) out[i] = 0.0f;
	if ( !data || index < 0 || index >= skeleton.numBones ) return false;
	Reader reader( data, bytes );
	if ( !reader.Fits( SkeletonHeaderBytes, sizeof( std::int32_t ),
		std::size_t( index ) + 1 ) ) return false;
	const std::int32_t stated = reader.Int( SkeletonHeaderBytes + std::size_t( index ) * 4 );
	if ( stated < 0 ) return false;
	const std::size_t record = SkeletonHeaderBytes + std::size_t( stated );
	if ( !reader.Fits( record, BoneBytes ) ) return false;
	/* Name, flags and parent come first; the rest matrix is the twelve floats
	 * after them, and the inverse the twelve after that. */
	for ( int i = 0; i < 12; ++i ) {
		out[i] = reader.Float( record + NameBytes + 8 + std::size_t( i ) * 4 );
	}
	return true;
}

/* The undo of that rest matrix, which the skeleton states beside it.
 *
 * A mesh's vertices are already in the model's own space -- a tag on a hand is
 * modelled where the hand is -- so moving one with a bone means first undoing
 * where that bone rests and then applying where it is now. The file carries
 * both halves, twelve floats each, the inverse directly after the rest. */
inline bool BoneRestMatrixInverse( const void *data, std::size_t bytes,
	const Skeleton &skeleton, int index, float out[12] )
{
	if ( !out ) return false;
	for ( int i = 0; i < 12; ++i ) out[i] = 0.0f;
	if ( !data || index < 0 || index >= skeleton.numBones ) return false;
	Reader reader( data, bytes );
	if ( !reader.Fits( SkeletonHeaderBytes, sizeof( std::int32_t ),
		std::size_t( index ) + 1 ) ) return false;
	const std::int32_t stated = reader.Int( SkeletonHeaderBytes + std::size_t( index ) * 4 );
	if ( stated < 0 ) return false;
	const std::size_t record = SkeletonHeaderBytes + std::size_t( stated );
	if ( !reader.Fits( record, BoneBytes ) ) return false;
	for ( int i = 0; i < 12; ++i ) {
		out[i] = reader.Float( record + NameBytes + 8 + 48 + std::size_t( i ) * 4 );
	}
	return true;
}

/* One bone's pose in one frame, decoded. */
inline bool BonePose( const void *data, std::size_t bytes, const Skeleton &skeleton,
	int frame, int index, CompressedPose &out )
{
	out = {};
	if ( !data || index < 0 || index >= skeleton.numBones ) return false;
	if ( frame < 0 || frame >= skeleton.numFrames ) return false;
	Reader reader( data, bytes );
	const std::size_t entry = ( std::size_t( frame ) * std::size_t( skeleton.numBones )
		+ std::size_t( index ) ) * FrameIndexBytes;
	if ( !reader.Fits( skeleton.frameOffset + entry, FrameIndexBytes ) ) return false;
	const std::uint32_t pose = reader.Index24( skeleton.frameOffset + entry );
	if ( int( pose ) > skeleton.maxPoseIndex ) return false;
	const std::size_t at = skeleton.poseOffset + std::size_t( pose ) * CompressedBoneBytes;
	if ( !reader.Fits( at, CompressedBoneBytes ) ) return false;
	out = DecodePose( static_cast<const unsigned char *>( data ) + at );
	return true;
}

/* One compressed storage transform composed with its base pose. This local
 * arithmetic helper does not evaluate parents or animation inheritance and
 * cannot establish a complete model-space bone pose; use ghoul2_pose.hpp. */
inline bool BoneMatrix( const void *data, std::size_t bytes, const Skeleton &skeleton,
	int frame, int index, float out[12] )
{
	if ( !out ) return false;
	for ( int i = 0; i < 12; ++i ) out[i] = 0.0f;
	CompressedPose pose;
	float rest[12], posed[12];
	if ( !BonePose( data, bytes, skeleton, frame, index, pose ) ) return false;
	if ( !BoneRestMatrix( data, bytes, skeleton, index, rest ) ) return false;
	PoseMatrix( pose, posed );
	ComposeMatrix( posed, rest, out );
	return true;
}

/* One surface of one level of detail, reached the way the reader walked it:
 * levels follow one another by the size each states, and surfaces follow one
 * another inside a level the same way. Everything here was bounds-checked when
 * the mesh was parsed; this walks to it again rather than keeping a table. */
inline bool SurfaceAt( const void *data, std::size_t bytes, const Mesh &mesh,
	int level, int index, MeshSurface &out )
{
	out = MeshSurface{};
	if ( !data || level < 0 || level >= mesh.numLODs ) return false;
	if ( index < 0 || index >= mesh.numSurfaces ) return false;
	Reader reader( data, bytes );
	std::size_t lod = mesh.lodOffset;
	for ( int step = 0; step < level; ++step ) {
		if ( !reader.Fits( lod, LodHeaderBytes ) ) return false;
		const std::int32_t size = reader.Int( lod );
		if ( size <= int( LodHeaderBytes ) ) return false;
		lod += std::size_t( size );
	}
	if ( !reader.Fits( lod, LodHeaderBytes ) ) return false;
	std::size_t surface = lod + LodHeaderBytes
		+ std::size_t( mesh.numSurfaces ) * sizeof( std::int32_t );
	for ( int step = 0; step < index; ++step ) {
		if ( !reader.Fits( surface, SurfaceBytes ) ) return false;
		const std::int32_t size = reader.Int( surface + 36 );
		if ( size <= 0 ) return false;
		surface += std::size_t( size );
	}
	if ( !reader.Fits( surface, SurfaceBytes ) ) return false;
	if ( reader.Int( surface + 4 ) != index ) return false;
	out.offset = surface;
	out.numVerts = reader.Int( surface + 12 );
	out.numTriangles = reader.Int( surface + 20 );
	out.numBoneReferences = reader.Int( surface + 28 );
	const std::int32_t ofsVerts = reader.Int( surface + 16 );
	const std::int32_t ofsTriangles = reader.Int( surface + 24 );
	const std::int32_t ofsBoneReferences = reader.Int( surface + 32 );
	if ( ofsVerts < 0 || ofsTriangles < 0 || ofsBoneReferences < 0 ) return false;
	if ( out.numVerts < 0 || out.numTriangles < 0 || out.numBoneReferences < 0 ) return false;
	out.vertexOffset = surface + std::size_t( ofsVerts );
	out.triangleOffset = surface + std::size_t( ofsTriangles );
	out.boneReferenceOffset = surface + std::size_t( ofsBoneReferences );
	out.texCoordOffset = out.vertexOffset + std::size_t( out.numVerts ) * VertexBytes;
	if ( !reader.Fits( out.vertexOffset, VertexBytes, std::size_t( out.numVerts ) )
		|| !reader.Fits( out.triangleOffset, TriangleBytes, std::size_t( out.numTriangles ) )
		|| !reader.Fits( out.boneReferenceOffset, sizeof( std::int32_t ),
			std::size_t( out.numBoneReferences ) ) ) return false;
	return true;
}

/* The three vertices one triangle names. */
inline bool TriangleCorners( const void *data, std::size_t bytes,
	const MeshSurface &surface, int triangle, int out[3] )
{
	if ( !out ) return false;
	out[0] = out[1] = out[2] = 0;
	if ( !data || triangle < 0 || triangle >= surface.numTriangles ) return false;
	Reader reader( data, bytes );
	const std::size_t at = surface.triangleOffset
		+ std::size_t( triangle ) * TriangleBytes;
	if ( !reader.Fits( at, TriangleBytes ) ) return false;
	for ( int corner = 0; corner < 3; ++corner ) {
		const std::int32_t index = reader.Int( at + std::size_t( corner ) * 4 );
		if ( index < 0 || index >= surface.numVerts ) return false;
		out[corner] = int( index );
	}
	return true;
}

/* One vertex, as a skinner needs it: where it sits when nothing has moved it,
 * and which bones of the model move it by how much.
 *
 * A vertex holds its normal first and its position twelve bytes in -- the
 * retail engine reads those three floats at that offset when it skins -- then
 * the packed bone references and the four weight bytes. The references are
 * into the surface's own short list, which this resolves to model bone numbers
 * so the caller does not have to know the surface exists. */
struct SkinnedVertex {
	float position[3] = { 0.0f, 0.0f, 0.0f };
	int count = 0;
	int bone[MaxWeightsPerVertex] = { 0, 0, 0, 0 };
	float weight[MaxWeightsPerVertex] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

inline bool VertexAt( const void *data, std::size_t bytes, const MeshSurface &surface,
	int index, SkinnedVertex &out )
{
	out = SkinnedVertex{};
	if ( !data || index < 0 || index >= surface.numVerts ) return false;
	Reader reader( data, bytes );
	const std::size_t at = surface.vertexOffset + std::size_t( index ) * VertexBytes;
	if ( !reader.Fits( at, VertexBytes ) ) return false;
	for ( int axis = 0; axis < 3; ++axis ) {
		out.position[axis] = reader.Float( at + 12 + std::size_t( axis ) * 4 );
	}
	const std::uint32_t packed = reader.Unsigned( at + 24 );
	const unsigned char *raw = static_cast<const unsigned char *>( data ) + at + 28;
	std::uint8_t bytesOfWeight[MaxWeightsPerVertex];
	for ( int i = 0; i < MaxWeightsPerVertex; ++i ) bytesOfWeight[i] = raw[i];
	out.count = WeightCount( packed );
	if ( out.count < 1 || out.count > MaxWeightsPerVertex ) return false;
	/* The final weight is implicit. Reject a negative remainder using the
	 * encoded integer units, so float accumulation cannot conceal it. */
	unsigned int encodedTotal = 0;
	for ( int weight = 0; weight + 1 < out.count; ++weight ) {
		encodedTotal += bytesOfWeight[weight]
			| ( ( packed >> ( WeightTopBitsShift + weight * 2 ) ) & WeightTopBitsMask );
	}
	if ( encodedTotal > 1023 ) return false;
	float running = 0.0f;
	for ( int weight = 0; weight < out.count; ++weight ) {
		const int reference = WeightBoneReference( packed, weight );
		if ( reference < 0 || reference >= surface.numBoneReferences ) return false;
		const std::size_t slot = surface.boneReferenceOffset
			+ std::size_t( reference ) * sizeof( std::int32_t );
		if ( !reader.Fits( slot, sizeof( std::int32_t ) ) ) return false;
		out.bone[weight] = int( reader.Int( slot ) );
		out.weight[weight] = WeightValue( packed, bytesOfWeight, weight, out.count, running );
	}
	return true;
}

/* One of the mesh's surface names, from the hierarchy the reader walked. */
inline bool SurfaceName( const void *data, std::size_t bytes, const Mesh &mesh,
	int index, char *out, std::size_t capacity )
{
	if ( !out || capacity == 0 ) return false;
	out[0] = 0;
	if ( !data || index < 0 || index >= mesh.numSurfaces ) return false;
	Reader reader( data, bytes );
	if ( !reader.Fits( MeshHeaderBytes, sizeof( std::int32_t ),
		std::size_t( index ) + 1 ) ) return false;
	const std::int32_t stated = reader.Int( MeshHeaderBytes + std::size_t( index ) * 4 );
	if ( stated < 0 ) return false;
	const std::size_t record = MeshHeaderBytes + std::size_t( stated );
	if ( !reader.Fits( record, SurfHierarchyBytes ) || !reader.Name( record ) ) return false;
	const char *name = static_cast<const char *>( data ) + record;
	std::size_t at = 0;
	while ( at + 1 < capacity && at < NameBytes && name[at] ) { out[at] = name[at]; ++at; }
	out[at] = 0;
	return at != 0;
}

} // namespace model
} // namespace jka
} // namespace fntech3

#endif
