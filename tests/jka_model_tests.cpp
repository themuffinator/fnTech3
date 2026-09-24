/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Jedi Academy's Ghoul2 model boundary.
 * Every mesh and skeleton here is written by this test; no retail model is
 * used, and nothing is posed or drawn. */

#include "../code/compat/jka_model_format.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace model = fntech3::jka::model;
using model::Error;
using Bytes = std::vector<std::uint8_t>;

static void PutInt( Bytes &file, std::size_t at, std::int32_t value )
{
	for ( int i = 0; i < 4; ++i ) file[at + i] = std::uint8_t( std::uint32_t( value ) >> ( 8 * i ) );
}

static void AppendInt( Bytes &file, std::int32_t value )
{
	file.resize( file.size() + 4 );
	PutInt( file, file.size() - 4, value );
}

static void AppendFloat( Bytes &file, float value )
{
	std::uint32_t word;
	std::memcpy( &word, &value, sizeof( word ) );
	AppendInt( file, std::int32_t( word ) );
}

static void AppendName( Bytes &file, const std::string &name )
{
	const std::size_t at = file.size();
	file.resize( at + model::NameBytes, 0 );
	std::memcpy( file.data() + at, name.c_str(), name.size() );
}

/* One vertex's packed word: a weight count, one bone reference per weight and
 * the top bits of every weight but the last. */
static std::uint32_t Packed( int count, const int references[4], const int weights[4] )
{
	std::uint32_t packed = std::uint32_t( count - 1 ) << 30;
	for ( int i = 0; i < count; ++i ) {
		packed |= std::uint32_t( references[i] ) << ( model::BoneReferenceBits * i );
		const std::uint32_t high = std::uint32_t( weights[i] ) & model::WeightTopBitsMask;
		packed |= high << ( model::WeightTopBitsShift + i * 2 );
	}
	return packed;
}

struct SurfaceShape {
	int verts;
	int triangles;
	int boneReferences;
};

/* Append one surface: its header, bone references, triangles, vertices and
 * their texture coordinates, in the order Raven writes them. */
static void AppendSurface( Bytes &file, int index, const SurfaceShape &shape )
{
	const std::size_t start = file.size();
	const std::size_t boneRefs = model::SurfaceBytes;
	const std::size_t triangles = boneRefs + std::size_t( shape.boneReferences ) * 4;
	const std::size_t verts = triangles + std::size_t( shape.triangles ) * model::TriangleBytes;
	const std::size_t texCoords = verts + std::size_t( shape.verts ) * model::VertexBytes;
	const std::size_t end = texCoords + std::size_t( shape.verts ) * model::TexCoordBytes;

	AppendInt( file, 0 );                           // ident, zero as carcass writes it
	AppendInt( file, index );                       // thisSurfaceIndex
	AppendInt( file, -std::int32_t( start ) );      // ofsHeader, back to the file header
	AppendInt( file, shape.verts );
	AppendInt( file, std::int32_t( verts ) );
	AppendInt( file, shape.triangles );
	AppendInt( file, std::int32_t( triangles ) );
	AppendInt( file, shape.boneReferences );
	AppendInt( file, std::int32_t( boneRefs ) );
	AppendInt( file, std::int32_t( end ) );

	for ( int i = 0; i < shape.boneReferences; ++i ) AppendInt( file, i );
	for ( int i = 0; i < shape.triangles; ++i ) {
		for ( int corner = 0; corner < 3; ++corner ) {
			AppendInt( file, ( i + corner ) % shape.verts );
		}
	}
	for ( int i = 0; i < shape.verts; ++i ) {
		AppendFloat( file, 0 ); AppendFloat( file, 0 ); AppendFloat( file, 1 );   // normal
		AppendFloat( file, float( i ) ); AppendFloat( file, 0 ); AppendFloat( file, 0 );
		const int count = 1 + i % model::MaxWeightsPerVertex;
		int references[4] = { 0, 0, 0, 0 }, weights[4] = { 0, 0, 0, 0 };
		for ( int weight = 0; weight < count; ++weight ) {
			references[weight] = weight % shape.boneReferences;
			weights[weight] = 256 * weight;
		}
		AppendInt( file, std::int32_t( Packed( count, references, weights ) ) );
		for ( int weight = 0; weight < model::MaxWeightsPerVertex; ++weight ) {
			file.push_back( std::uint8_t( weights[weight] & 0xff ) );
		}
	}
	for ( int i = 0; i < shape.verts; ++i ) {
		AppendFloat( file, float( i ) / 8.0f );
		AppendFloat( file, 0.5f );
	}
	CHECK( file.size() - start == end );
}

/* Two surfaces, one the child of the other, at two levels of detail. */
static Bytes Mesh()
{
	const SurfaceShape shapes[2] = { { 6, 4, 3 }, { 4, 2, 2 } };
	Bytes file;
	AppendInt( file, std::int32_t( model::MeshIdent ) );
	AppendInt( file, model::MeshVersion );
	AppendName( file, "models/players/jedi/model.glm" );
	AppendName( file, "models/players/_humanoid/_humanoid" );
	AppendInt( file, 0 );                 // animIndex
	AppendInt( file, 3 );                 // numBones
	AppendInt( file, 2 );                 // numLODs
	const std::size_t lodOffsetField = file.size();
	AppendInt( file, 0 );                 // ofsLODs, filled in below
	AppendInt( file, 2 );                 // numSurfaces
	// The stated offset points past the offset table at the first record.
	AppendInt( file, std::int32_t( model::MeshHeaderBytes + 2 * 4 ) );
	const std::size_t endField = file.size();
	AppendInt( file, 0 );                 // ofsEnd, filled in below
	CHECK( file.size() == model::MeshHeaderBytes );

	const std::size_t hierarchyTable = file.size();
	AppendInt( file, 0 );
	AppendInt( file, 0 );
	for ( int surface = 0; surface < 2; ++surface ) {
		PutInt( file, hierarchyTable + std::size_t( surface ) * 4,
			std::int32_t( file.size() - model::MeshHeaderBytes ) );
		AppendName( file, surface ? "l_arm" : "torso" );
		AppendInt( file, 0 );                          // flags
		AppendName( file, "models/players/jedi/torso" );
		AppendInt( file, 0 );                          // shaderIndex
		AppendInt( file, surface ? 0 : -1 );           // parentIndex
		AppendInt( file, surface ? 0 : 1 );            // numChildren
		if ( !surface ) AppendInt( file, 1 );          // childIndexes
	}

	PutInt( file, lodOffsetField, std::int32_t( file.size() ) );
	for ( int level = 0; level < 2; ++level ) {
		const std::size_t lod = file.size();
		AppendInt( file, 0 );                          // ofsEnd, filled in below
		const std::size_t table = file.size();
		AppendInt( file, 0 );
		AppendInt( file, 0 );
		for ( int surface = 0; surface < 2; ++surface ) {
			PutInt( file, table + std::size_t( surface ) * 4,
				std::int32_t( file.size() - table ) );
			SurfaceShape shape = shapes[surface];
			if ( level ) { shape.verts -= 1; shape.triangles -= 1; }
			AppendSurface( file, surface, shape );
		}
		PutInt( file, lod, std::int32_t( file.size() - lod ) );
	}
	PutInt( file, endField, std::int32_t( file.size() ) );
	return file;
}

/* Three bones, two frames, four poses in the pool. */
static Bytes Skeleton()
{
	Bytes file;
	AppendInt( file, std::int32_t( model::SkeletonIdent ) );
	AppendInt( file, model::SkeletonVersion );
	AppendName( file, "models/players/_humanoid/_humanoid" );
	AppendFloat( file, 1.0f );            // fScale
	AppendInt( file, 2 );                 // numFrames
	const std::size_t framesField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, 3 );                 // numBones
	const std::size_t poolField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, std::int32_t( model::SkeletonHeaderBytes + 3 * 4 ) );
	const std::size_t endField = file.size();
	AppendInt( file, 0 );
	CHECK( file.size() == model::SkeletonHeaderBytes );

	const std::size_t table = file.size();
	for ( int bone = 0; bone < 3; ++bone ) AppendInt( file, 0 );
	for ( int bone = 0; bone < 3; ++bone ) {
		PutInt( file, table + std::size_t( bone ) * 4,
			std::int32_t( file.size() - model::SkeletonHeaderBytes ) );
		AppendName( file, bone ? ( bone == 1 ? "lower_lumbar" : "thoracic" ) : "model_root" );
		AppendInt( file, 0 );                          // flags
		AppendInt( file, bone - 1 );                   // parent, -1 for the root
		for ( int matrix = 0; matrix < 24; ++matrix ) AppendFloat( file, matrix % 5 ? 0.0f : 1.0f );
		AppendInt( file, bone == 2 ? 0 : 1 );          // numChildren
		if ( bone != 2 ) AppendInt( file, bone + 1 );
	}

	PutInt( file, framesField, std::int32_t( file.size() ) );
	for ( int frame = 0; frame < 2; ++frame ) {
		for ( int bone = 0; bone < 3; ++bone ) {
			const std::uint32_t index = std::uint32_t( frame * 3 + bone ) % 4;
			file.push_back( std::uint8_t( index ) );
			file.push_back( std::uint8_t( index >> 8 ) );
			file.push_back( std::uint8_t( index >> 16 ) );
		}
	}
	while ( file.size() % 4 ) file.push_back( 0 );     // Raven pads back to alignment
	PutInt( file, poolField, std::int32_t( file.size() ) );
	for ( int pose = 0; pose < 4; ++pose ) {
		for ( std::size_t i = 0; i < model::CompressedBoneBytes; ++i ) {
			file.push_back( std::uint8_t( pose * 16 + i ) );
		}
	}
	PutInt( file, endField, std::int32_t( file.size() ) );
	return file;
}

static void CheckWeights()
{
	const int references[4] = { 3, 1, 0, 2 };
	const int weights[4] = { 0x123, 0x2ff, 0x080, 0 };
	const std::uint32_t packed = Packed( 4, references, weights );
	std::uint8_t low[4];
	for ( int i = 0; i < 4; ++i ) low[i] = std::uint8_t( weights[i] & 0xff );
	CHECK( model::WeightCount( packed ) == 4 );
	for ( int i = 0; i < 4; ++i ) CHECK( model::WeightBoneReference( packed, i ) == references[i] );

	float running = 0;
	float decoded[4];
	for ( int i = 0; i < 4; ++i ) {
		decoded[i] = model::WeightValue( packed, low, i, 4, running );
	}
	for ( int i = 0; i < 3; ++i ) {
		CHECK( decoded[i] > float( weights[i] ) / 1023.0f - 0.0005f );
		CHECK( decoded[i] < float( weights[i] ) / 1023.0f + 0.0005f );
	}
	// The last weight is whatever the others leave, so they always sum to one.
	float total = 0;
	for ( float value : decoded ) total += value;
	CHECK( total > 0.9999f && total < 1.0001f );

	// A single-weight vertex is entirely its one bone.
	const int one[4] = { 5, 0, 0, 0 };
	const int none[4] = { 0, 0, 0, 0 };
	const std::uint32_t single = Packed( 1, one, none );
	running = 0;
	CHECK( model::WeightCount( single ) == 1 );
	CHECK( model::WeightBoneReference( single, 0 ) == 5 );
	CHECK( model::WeightValue( single, low, 0, 1, running ) == 1.0f );
}

static void CheckMesh()
{
	const Bytes file = Mesh();
	model::Mesh mesh;
	CHECK( model::ParseMesh( file.data(), file.size(), 3, mesh ) == Error::None );
	CHECK( mesh.version == model::MeshVersion && mesh.numBones == 3 );
	CHECK( mesh.numLODs == 2 && mesh.numSurfaces == 2 );
	CHECK( mesh.totalVerts == 6 + 4 + 5 + 3 );
	CHECK( mesh.totalTriangles == 4 + 2 + 3 + 1 );
	CHECK( mesh.maxBoneReference == 2 && mesh.maxWeightsPerVertex == 4 );

	// The mesh may not reference more bones than the skeleton it names has.
	CHECK( model::ParseMesh( file.data(), file.size(), 2, mesh ) == Error::Bone );
	CHECK( model::ParseMesh( file.data(), file.size(), 0, mesh ) == Error::None );

	// Nothing, and no mesh at all, is not a mesh.
	CHECK( model::ParseMesh( nullptr, file.size(), 3, mesh ) == Error::Size );
	CHECK( model::ParseMesh( file.data(), 0, 3, mesh ) == Error::Size );

	// Every truncation is refused rather than read past the end.
	for ( std::size_t length = 0; length < file.size(); ++length ) {
		CHECK( model::ParseMesh( file.data(), length, 3, mesh ) != Error::None );
	}

	struct Damage { std::size_t at; std::int32_t value; Error error; };
	for ( auto damage : { Damage{ 0, 0x50444924, Error::Ident },
		{ 4, 5, Error::Version }, { 4, 7, Error::Version },
		{ 140, -1, Error::Counts },              // numBones
		{ 144, -1, Error::Counts },              // numLODs
		{ 152, -1, Error::Counts },              // numSurfaces
		{ 156, 168, Error::Offset },             // ofsSurfHierarchy elsewhere
		{ 156, -4, Error::Offset },
		{ 160, -1, Error::Extent },              // ofsEnd before the file
		{ 148, 4, Error::Offset },               // LODs inside the hierarchy
		{ 164, 4, Error::Order } } ) {           // a hierarchy offset that lies
		Bytes damaged = file;
		PutInt( damaged, damage.at, damage.value );
		CHECK( model::ParseMesh( damaged.data(), damaged.size(), 3, mesh ) == damage.error );
	}

	// A name that never ends inside its own field is refused.
	for ( std::size_t name : { std::size_t( 8 ), std::size_t( 72 ) } ) {
		Bytes damaged = file;
		for ( std::size_t i = 0; i < model::NameBytes; ++i ) damaged[name + i] = 'x';
		CHECK( model::ParseMesh( damaged.data(), damaged.size(), 3, mesh ) == Error::Name );
	}

	// Hierarchy indices that leave the surface list are refused.
	const std::size_t firstRecord = model::MeshHeaderBytes + 8;
	struct Index { std::size_t at; std::int32_t value; Error error; };
	for ( auto index : { Index{ firstRecord + 136, 2, Error::Hierarchy },   // parent
		{ firstRecord + 136, -2, Error::Hierarchy },
		{ firstRecord + 140, -1, Error::Counts },                          // numChildren
		{ firstRecord + 140, 3, Error::Counts },
		{ firstRecord + model::SurfHierarchyBytes, 2, Error::Hierarchy },  // child
		{ firstRecord + model::SurfHierarchyBytes, -1, Error::Hierarchy } } ) {
		Bytes damaged = file;
		PutInt( damaged, index.at, index.value );
		CHECK( model::ParseMesh( damaged.data(), damaged.size(), 3, mesh ) == index.error );
	}

	// The first surface of the first level of detail, damaged in turn.
	model::Mesh good;
	CHECK( model::ParseMesh( file.data(), file.size(), 3, good ) == Error::None );
	std::size_t surface = 0;
	{
		// Find it the way the reader does, through the level's offset table.
		const std::int32_t ofsLODs = std::int32_t( file[148] | ( file[149] << 8 )
			| ( file[150] << 16 ) | ( std::uint32_t( file[151] ) << 24 ) );
		surface = std::size_t( ofsLODs ) + model::LodHeaderBytes + 2 * 4;
	}
	for ( auto damage : { Damage{ surface + 4, 1, Error::Order },        // thisSurfaceIndex
		{ surface + 8, 0, Error::Offset },                               // ofsHeader
		{ surface + 12, -1, Error::Counts },                             // numVerts
		{ surface + 20, -1, Error::Counts },                             // numTriangles
		{ surface + 28, 33, Error::Counts },                             // numBoneReferences
		{ surface + 28, -1, Error::Counts },
		{ surface + 16, -4, Error::Offset },                             // ofsVerts
		{ surface + 24, 1 << 28, Error::Extent },                        // ofsTriangles
		{ surface + 36, 0, Error::Order } } ) {                          // ofsEnd
		Bytes damaged = file;
		PutInt( damaged, damage.at, damage.value );
		CHECK( model::ParseMesh( damaged.data(), damaged.size(), 3, mesh ) == damage.error );
	}

	// A triangle that names a vertex the surface does not have, and a vertex
	// weighted to a bone the surface never references.
	{
		Bytes damaged = file;
		PutInt( damaged, surface + model::SurfaceBytes + 3 * 4, 6 );
		CHECK( model::ParseMesh( damaged.data(), damaged.size(), 3, mesh ) == Error::Index );
	}
	{
		Bytes damaged = file;
		const std::size_t verts = surface + model::SurfaceBytes + 3 * 4
			+ 4 * model::TriangleBytes;
		const int references[4] = { 3, 0, 0, 0 }, weights[4] = { 0, 0, 0, 0 };
		PutInt( damaged, verts + 24, std::int32_t( Packed( 1, references, weights ) ) );
		CHECK( model::ParseMesh( damaged.data(), damaged.size(), 3, mesh ) == Error::Bone );
	}
}

static void CheckSkeleton()
{
	const Bytes file = Skeleton();
	model::Skeleton skeleton;
	CHECK( model::ParseSkeleton( file.data(), file.size(), skeleton ) == Error::None );
	CHECK( skeleton.version == model::SkeletonVersion );
	CHECK( skeleton.numFrames == 2 && skeleton.numBones == 3 );
	CHECK( skeleton.scale == 1.0f );
	CHECK( skeleton.maxPoseIndex == 3 && skeleton.maxBoneChildren == 1 );

	CHECK( model::ParseSkeleton( nullptr, file.size(), skeleton ) == Error::Size );
	for ( std::size_t length = 0; length < file.size(); ++length ) {
		CHECK( model::ParseSkeleton( file.data(), length, skeleton ) != Error::None );
	}

	struct Damage { std::size_t at; std::int32_t value; Error error; };
	for ( auto damage : { Damage{ 0, 0x50444924, Error::Ident },
		{ 4, 5, Error::Version },
		{ 76, -1, Error::Counts },               // numFrames
		{ 84, -1, Error::Counts },               // numBones
		{ 92, 104, Error::Offset },              // ofsSkel elsewhere
		{ 96, -1, Error::Extent },               // ofsEnd before the file
		{ 80, 1 << 28, Error::Extent },          // frames past the end
		{ 88, 1 << 28, Error::Extent },          // pose pool past the end
		{ 88, -1, Error::Offset },
		{ 100, -1, Error::Offset } } ) {         // a bone offset that lies
		Bytes damaged = file;
		PutInt( damaged, damage.at, damage.value );
		CHECK( model::ParseSkeleton( damaged.data(), damaged.size(), skeleton ) == damage.error );
	}

	{
		Bytes damaged = file;
		for ( std::size_t i = 0; i < model::NameBytes; ++i ) damaged[8 + i] = 'x';
		CHECK( model::ParseSkeleton( damaged.data(), damaged.size(), skeleton ) == Error::Name );
	}

	// Bone parents and children must name bones this skeleton has.
	const std::size_t firstBone = model::SkeletonHeaderBytes + 3 * 4;
	for ( auto damage : { Damage{ firstBone + 68, 3, Error::Hierarchy },   // parent
		{ firstBone + 68, -2, Error::Hierarchy },
		{ firstBone + 168, 4, Error::Counts },                            // numChildren
		{ firstBone + 168, -1, Error::Counts },
		{ firstBone + model::BoneBytes, 3, Error::Hierarchy },            // child
		{ firstBone + model::BoneBytes, -1, Error::Hierarchy } } ) {
		Bytes damaged = file;
		PutInt( damaged, damage.at, damage.value );
		CHECK( model::ParseSkeleton( damaged.data(), damaged.size(), skeleton ) == damage.error );
	}

	// A frame that indexes past the pose pool is refused.
	{
		Bytes damaged = file;
		const std::int32_t frames = std::int32_t( damaged[80] | ( damaged[81] << 8 )
			| ( damaged[82] << 16 ) | ( std::uint32_t( damaged[83] ) << 24 ) );
		damaged[std::size_t( frames )] = 0xff;
		damaged[std::size_t( frames ) + 1] = 0xff;
		CHECK( model::ParseSkeleton( damaged.data(), damaged.size(), skeleton ) == Error::Extent );
	}

	// A skeleton with no frames at all still describes its bones.
	{
		Bytes empty = file;
		PutInt( empty, 76, 0 );
		CHECK( model::ParseSkeleton( empty.data(), empty.size(), skeleton ) == Error::None );
		CHECK( skeleton.numFrames == 0 && skeleton.maxPoseIndex == -1 );
	}
}

static bool Near( float value, float want )
{
	return std::fabs( value - want ) < 1e-4f;
}

/* The pose encoding, as the retail engine's own decompressor does it. The
 * constants are read out of that routine rather than fitted to the data, and
 * the two halves of a pose disagree with each other -- a rotation component
 * is word / 16383 - 2 and a displacement is word / 64 - 512 -- so a component
 * of zero is 32,766 in one half and 32,768 in the other. That asymmetry is
 * the engine's, and it is asserted here so nobody tidies it away. */
static void CheckPose()
{
	static_assert( model::CompressedBoneBytes == 14, "pool stride" );
	static_assert( model::FrameIndexBytes == 3, "frame index width" );
	/* The root deviations of all 87 shipped skeletons are whole units at this
	 * divisor, and the engine divides by exactly this. */
	CHECK( Near( 1536.0f / model::PoseTranslationDivisor, 24.0f ) );
	CHECK( Near( 9856.0f / model::PoseTranslationDivisor, 154.0f ) );

	unsigned char bytes[model::CompressedBoneBytes];
	auto put = [&]( int at, int value ) {
		bytes[at] = (unsigned char)( value & 0xff );
		bytes[at + 1] = (unsigned char)( ( value >> 8 ) & 0xff );
	};

	// A rotation of nothing and a displacement of nothing.
	put( 0, 3 * 16383 );          // w = 1
	put( 2, 2 * 16383 );          // x = 0
	put( 4, 2 * 16383 );
	put( 6, 2 * 16383 );
	put( 8, 512 * 64 );           // no displacement
	put( 10, 512 * 64 );
	put( 12, 512 * 64 );
	model::CompressedPose pose = model::DecodePose( bytes );
	CHECK( Near( pose.rotation[0], 1.0f ) );
	CHECK( Near( pose.rotation[1], 0.0f ) );
	CHECK( Near( pose.rotation[2], 0.0f ) );
	CHECK( Near( pose.rotation[3], 0.0f ) );
	CHECK( Near( pose.translation[0], 0.0f ) );
	CHECK( Near( pose.translation[1], 0.0f ) );
	CHECK( Near( pose.translation[2], 0.0f ) );

	// The two halves really do use different mid-points.
	CHECK( !Near( 2.0f * model::PoseRotationDivisor,
		model::PoseTranslationOffset * model::PoseTranslationDivisor ) );

	// A pose that does nothing composes to the rest matrix unchanged.
	{
		float posed[12], rest[12], out[12];
		for ( int i = 0; i < 12; ++i ) rest[i] = 0.0f;
		rest[0] = rest[5] = rest[10] = 1.0f;
		rest[3] = 3.0f; rest[7] = -4.0f; rest[11] = 5.0f;
		model::PoseMatrix( pose, posed );
		model::ComposeMatrix( posed, rest, out );
		for ( int i = 0; i < 12; ++i ) CHECK( Near( out[i], rest[i] ) );
	}

	// Below the mid-point is negative, in both halves of the record.
	put( 2, 16383 );              // x = -1
	put( 8, 512 * 64 - 1000 );
	put( 12, 512 * 64 + 1000 );
	pose = model::DecodePose( bytes );
	CHECK( Near( pose.rotation[1], -1.0f ) );
	CHECK( Near( pose.translation[0], -1000.0f / 64.0f ) );
	CHECK( Near( pose.translation[2], 1000.0f / 64.0f ) );

	// The extremes stay in range and stay signed.
	put( 0, 0 );
	put( 8, 0 );
	put( 10, 65535 );
	pose = model::DecodePose( bytes );
	CHECK( Near( pose.rotation[0], -2.0f ) );
	CHECK( Near( pose.translation[0], -512.0f ) );
	CHECK( Near( pose.translation[1], 65535.0f / 64.0f - 512.0f ) );

	// A quarter turn about z, then a displacement, carries a point where the
	// arithmetic says and not where the operands happen to sit.
	{
		model::CompressedPose turn{};
		const float root = 0.70710678f;
		turn.rotation[0] = root; turn.rotation[3] = root;
		turn.translation[0] = 10.0f;
		float posed[12], rest[12], out[12];
		for ( int i = 0; i < 12; ++i ) rest[i] = 0.0f;
		rest[0] = rest[5] = rest[10] = 1.0f;
		rest[3] = 2.0f;
		model::PoseMatrix( turn, posed );
		model::ComposeMatrix( posed, rest, out );
		// The rest point (2,0,0) turns to (0,2,0), then shifts by (10,0,0).
		CHECK( Near( out[3], 10.0f ) );
		CHECK( Near( out[7], 2.0f ) );
		CHECK( Near( out[11], 0.0f ) );
	}
}

/* The three accessors that answer where a bone is. The fixture's rest
 * matrices are the identity with no translation, so a bone's matrix in a
 * frame has to come out as exactly its decoded pose -- which is what makes
 * this worth asserting rather than restating the arithmetic. */
static void CheckBoneAccessors()
{
	const Bytes file = Skeleton();
	model::Skeleton skeleton;
	CHECK( model::ParseSkeleton( file.data(), file.size(), skeleton ) == Error::None );
	CHECK( skeleton.numBones == 3 && skeleton.numFrames == 2 );
	CHECK( skeleton.frameOffset != 0 && skeleton.poseOffset != 0 );

	float rest[12];
	for ( int bone = 0; bone < 3; ++bone ) {
		CHECK( model::BoneRestMatrix( file.data(), file.size(), skeleton, bone, rest ) );
		for ( int i = 0; i < 12; ++i ) {
			const float want = ( i == 0 || i == 5 || i == 10 ) ? 1.0f : 0.0f;
			CHECK( Near( rest[i], want ) );
		}
	}
	CHECK( !model::BoneRestMatrix( file.data(), file.size(), skeleton, 3, rest ) );
	CHECK( !model::BoneRestMatrix( file.data(), file.size(), skeleton, -1, rest ) );

	// Frame 0 bone 0 reaches pose 0, whose fourteen bytes are 0 to 13.
	model::CompressedPose pose;
	CHECK( model::BonePose( file.data(), file.size(), skeleton, 0, 0, pose ) );
	CHECK( Near( pose.rotation[0], 256.0f / 16383.0f - 2.0f ) );
	CHECK( Near( pose.rotation[3], 1798.0f / 16383.0f - 2.0f ) );
	CHECK( Near( pose.translation[0], 2312.0f / 64.0f - 512.0f ) );
	CHECK( Near( pose.translation[2], 3340.0f / 64.0f - 512.0f ) );

	// A frame or a bone that is not there is refused rather than guessed.
	CHECK( !model::BonePose( file.data(), file.size(), skeleton, 2, 0, pose ) );
	CHECK( !model::BonePose( file.data(), file.size(), skeleton, -1, 0, pose ) );
	CHECK( !model::BonePose( file.data(), file.size(), skeleton, 0, 3, pose ) );

	// With an identity rest matrix, a bone's matrix is its pose exactly.
	float posed[12], world[12];
	for ( int frame = 0; frame < 2; ++frame ) {
		for ( int bone = 0; bone < 3; ++bone ) {
			CHECK( model::BonePose( file.data(), file.size(), skeleton, frame, bone, pose ) );
			model::PoseMatrix( pose, posed );
			CHECK( model::BoneMatrix( file.data(), file.size(), skeleton, frame, bone, world ) );
			for ( int i = 0; i < 12; ++i ) CHECK( Near( world[i], posed[i] ) );
		}
	}
	CHECK( !model::BoneMatrix( file.data(), file.size(), skeleton, 2, 0, world ) );
}

static void CheckErrorNames()
{
	for ( auto error : { Error::None, Error::Size, Error::Ident, Error::Version,
		Error::Counts, Error::Offset, Error::Extent, Error::Name, Error::Order,
		Error::Hierarchy, Error::Weights, Error::Index, Error::Bone } ) {
		const char *name = model::ErrorName( error );
		CHECK( name && name[0] && std::strcmp( name, "unknown" ) );
	}
	CHECK( !std::strcmp( model::ErrorName( (Error)99 ), "unknown" ) );
}

int main()
{
	CheckWeights();
	CheckMesh();
	CheckSkeleton();
	CheckPose();
	CheckBoneAccessors();
	CheckErrorNames();
	std::cout << "Jedi Academy Ghoul2 mesh, skeleton and malformed-model tests passed\n";
	return 0;
}
