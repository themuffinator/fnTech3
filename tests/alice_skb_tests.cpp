/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Alice's `SKL ` skeleton boundary. Every
 * skeleton here is written by this test; no retail model is used. */

#include "../code/compat/alice_skb_format.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::model;
using Bytes = std::vector<std::uint8_t>;

static void PutInt( Bytes &file, std::size_t at, std::int32_t value )
{
	std::memcpy( file.data() + at, &value, sizeof( value ) );
}

static void PutName( Bytes &file, std::size_t at, const char *text )
{
	std::memset( file.data() + at, 0, SkbNameBytes );
	std::memcpy( file.data() + at, text, std::strlen( text ) );
}

/* Four bones with a two-level hierarchy and two roots -- retail skeletons have
 * more than one -- and two surfaces whose vertices carry different weight
 * counts, so a fixed-stride walk lands somewhere it should not. */
static const int kBones = 4;
static const std::int32_t kParents[kBones] = { -1, 0, 1, -1 };
static const int kSurfaces = 2;
static const int kVerts[kSurfaces] = { 3, 2 };
static const int kTris[kSurfaces] = { 1, 0 };
static const int kWeights[kSurfaces][3] = { { 1, 2, 3 }, { 4, 1, 0 } };

static Bytes Fixture( int version = 3 )
{
	Bytes file( SkbHeaderBytes, 0 );
	std::uint32_t ident = SkbIdent;
	std::memcpy( file.data(), &ident, sizeof( ident ) );
	PutInt( file, 4, version );
	PutName( file, 8, "models/alice/fixture.skb" );
	PutInt( file, 72, kSurfaces );
	PutInt( file, 76, kBones );
	PutInt( file, 80, std::int32_t( SkbHeaderBytes ) );

	for ( int b = 0; b < kBones; ++b ) {
		const std::size_t at = file.size();
		file.resize( file.size() + SkbBoneBytes, 0 );
		PutInt( file, at, kParents[b] );
		PutInt( file, at + 4, b == 2 ? 1 : 0 );        /* the flag retail sets */
		PutName( file, at + 8, b ? "bone_child" : "bone_root" );
	}
	PutInt( file, 84, std::int32_t( file.size() ) );   /* ofsSurfaces */

	for ( int s = 0; s < kSurfaces; ++s ) {
		const std::size_t at = file.size();
		std::size_t vertexBytes = 0;
		for ( int v = 0; v < kVerts[s]; ++v ) {
			vertexBytes += SkbVertexFixedBytes + SkbWeightBytes * std::size_t( kWeights[s][v] );
		}
		const std::size_t triBytes = SkbTriangleBytes * std::size_t( kTris[s] );
		const std::size_t collapseBytes = SkbCollapseBytes * std::size_t( kVerts[s] );
		const std::size_t end = SkbSurfaceHeaderBytes + triBytes + vertexBytes + collapseBytes;
		file.resize( file.size() + end, 0 );
		std::memcpy( file.data() + at, &ident, sizeof( ident ) );
		PutName( file, at + 4, s ? "skin_body" : "skin_head" );
		PutInt( file, at + 68, kTris[s] );
		PutInt( file, at + 72, kVerts[s] );
		PutInt( file, at + 76, kVerts[s] );            /* minLod, at its maximum */
		PutInt( file, at + 80, std::int32_t( SkbSurfaceHeaderBytes ) );
		PutInt( file, at + 84, std::int32_t( SkbSurfaceHeaderBytes + triBytes ) );
		PutInt( file, at + 88, std::int32_t( SkbSurfaceHeaderBytes + triBytes + vertexBytes ) );
		PutInt( file, at + 92, std::int32_t( end ) );

		std::size_t v = at + SkbSurfaceHeaderBytes + triBytes;
		for ( int i = 0; i < kVerts[s]; ++i ) {
			const int count = kWeights[s][i];
			PutInt( file, v + SkbWeightCountOffset, count );
			for ( int w = 0; w < count; ++w ) {
				PutInt( file, v + SkbVertexFixedBytes + SkbWeightBytes * std::size_t( w ),
					w % kBones );
			}
			v += SkbVertexFixedBytes + SkbWeightBytes * std::size_t( count );
		}
	}
	PutInt( file, 88, std::int32_t( file.size() ) );   /* ofsEnd */
	return file;
}

static void CheckParse()
{
	for ( int version : { SkbVersionLow, SkbVersionHigh } ) {
		const Bytes file = Fixture( version );
		Skb skb{};
		CHECK( ParseSkb( file.data(), file.size(), skb ) == SkbError::None );
		CHECK( skb.version == version );
		CHECK( skb.bones == kBones && skb.surfaces == kSurfaces );
		CHECK( skb.roots == 2 );
		CHECK( skb.boneData == SkbHeaderBytes );

		/* Every bone reaches a root, in at most as many steps as there are
		 * bones, which is what refusing a later parent buys. */
		for ( int b = 0; b < kBones; ++b ) {
			CHECK( SkbBoneParent( file.data(), skb, b ) == kParents[b] );
			int steps = 0;
			for ( std::int32_t at = b; at != -1;
				at = SkbBoneParent( file.data(), skb, at ) ) {
				CHECK( ++steps <= kBones );
			}
			CHECK( SkbBoneName( skb, b ) == SkbBoneAt( skb, b ) + 8 );
		}

		std::size_t previousEnd = 0;
		for ( int s = 0; s < kSurfaces; ++s ) {
			const SkbSurface &surface = skb.surface[s];
			CHECK( surface.verts == kVerts[s] && surface.triangles == kTris[s] );
			CHECK( surface.minLod == kVerts[s] );
			CHECK( surface.header >= previousEnd );
			CHECK( surface.name == surface.header + 4 );
			/* The walk counted every weight, which a fixed-stride reader
			 * could not have done. */
			int expected = 0;
			for ( int v = 0; v < kVerts[s]; ++v ) expected += kWeights[s][v];
			CHECK( surface.weights == expected );
			CHECK( surface.triangleData == surface.header + SkbSurfaceHeaderBytes );
			CHECK( surface.vertexData >= surface.triangleData );
			CHECK( surface.collapseData >= surface.vertexData );
			CHECK( surface.collapseData + SkbCollapseBytes * std::size_t( surface.verts )
				== surface.nextSurface );
			previousEnd = surface.nextSurface;
		}
		CHECK( previousEnd == file.size() );
	}
}

static void CheckRejections()
{
	const Bytes valid = Fixture();
	Skb skb{};

	for ( std::uint32_t ident : { 0u, 0xffffffffu,
		std::uint32_t( ( ' ' << 24 ) | ( 'N' << 16 ) | ( 'A' << 8 ) | 'T' ) } ) {
		Bytes wrong = valid;
		std::memcpy( wrong.data(), &ident, sizeof( ident ) );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Ident );
	}
	/* The engine accepts a range of versions; outside it a file is refused. */
	for ( std::int32_t version : { 0, 1, 4, 12, -1 } ) {
		Bytes wrong = valid;
		PutInt( wrong, 4, version );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Version );
	}
	for ( std::size_t length : { std::size_t( 0 ), std::size_t( 8 ), SkbHeaderBytes - 1 } ) {
		CHECK( ParseSkb( valid.data(), length, skb ) == SkbError::Size );
	}
	CHECK( ParseSkb( nullptr, valid.size(), skb ) == SkbError::Size );

	for ( std::int32_t claimed : { 0, -1, std::int32_t( valid.size() ) - 1,
		std::int32_t( valid.size() ) + 1 } ) {
		Bytes wrong = valid;
		PutInt( wrong, 88, claimed );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Extent );
	}

	const struct { std::size_t at; std::int32_t value; SkbError error; } counts[] = {
		{ 76, 0, SkbError::Counts }, { 76, -1, SkbError::Counts },
		{ 76, SkbMaxBones + 1, SkbError::Counts },
		{ 72, -1, SkbError::Counts }, { 72, SkbMaxSurfaces + 1, SkbError::Counts },
	};
	for ( const auto &bad : counts ) {
		Bytes wrong = valid;
		PutInt( wrong, bad.at, bad.value );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == bad.error );
	}

	/* The bones must begin where the header ends and reach the surfaces. */
	for ( std::int32_t value : { 0, -1, std::int32_t( SkbHeaderBytes ) + 4 } ) {
		Bytes wrong = valid;
		PutInt( wrong, 80, value );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Offset );
	}
	{
		Bytes wrong = valid;
		PutInt( wrong, 84, std::int32_t( SkbHeaderBytes ) );   /* surfaces overlap bones */
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Order );
	}

	/* A parent that is not an earlier bone would make a hierarchy walk loop. */
	for ( std::int32_t parent : { 1, 2, 3, kBones, -2 } ) {
		Bytes wrong = valid;
		PutInt( wrong, SkbHeaderBytes + SkbBoneBytes, parent );   /* bone 1 */
		const SkbError error = ParseSkb( wrong.data(), wrong.size(), skb );
		CHECK( error == SkbError::Parent );
	}
	/* A skeleton with no root at all is refused. */
	{
		Bytes wrong = valid;
		PutInt( wrong, SkbHeaderBytes, 0 );                      /* bone 0 parents itself */
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Parent );
	}

	/* A name with no terminator inside its own field is refused. */
	{
		Bytes wrong = valid;
		std::memset( wrong.data() + SkbHeaderBytes + 8, 'a', SkbNameBytes );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Name );
	}

	/* A weight naming a bone the skeleton does not have would index past the
	 * bone array when the model is posed. */
	{
		Bytes wrong = valid;
		Skb good{};
		CHECK( ParseSkb( wrong.data(), wrong.size(), good ) == SkbError::None );
		PutInt( wrong, good.surface[0].vertexData + SkbVertexFixedBytes, kBones );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Bone );
		PutInt( wrong, good.surface[0].vertexData + SkbVertexFixedBytes, -1 );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Bone );
	}

	/* A weight count that does not fit inside the block is refused, and one
	 * that merely disagrees leaves the walk short of the collapse map. */
	{
		Bytes wrong = valid;
		Skb good{};
		CHECK( ParseSkb( wrong.data(), wrong.size(), good ) == SkbError::None );
		PutInt( wrong, good.surface[0].vertexData + SkbWeightCountOffset, kBones + 1 );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Counts );
		PutInt( wrong, good.surface[0].vertexData + SkbWeightCountOffset, 0 );
		const SkbError error = ParseSkb( wrong.data(), wrong.size(), skb );
		CHECK( error == SkbError::Order || error == SkbError::Extent );
	}

	/* A surface whose blocks do not tile it is refused. */
	{
		Bytes wrong = valid;
		Skb good{};
		CHECK( ParseSkb( wrong.data(), wrong.size(), good ) == SkbError::None );
		PutInt( wrong, good.surface[0].header + 92, std::int32_t( wrong.size() ) );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Extent );
	}
	{
		Bytes wrong = valid;
		Skb good{};
		CHECK( ParseSkb( wrong.data(), wrong.size(), good ) == SkbError::None );
		PutInt( wrong, good.surface[0].header + 80, std::int32_t( SkbSurfaceHeaderBytes ) + 4 );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Offset );
	}
	/* minLod is a vertex count, so one larger than the surface has is refused. */
	{
		Bytes wrong = valid;
		Skb good{};
		CHECK( ParseSkb( wrong.data(), wrong.size(), good ) == SkbError::None );
		PutInt( wrong, good.surface[0].header + 76, kVerts[0] + 1 );
		CHECK( ParseSkb( wrong.data(), wrong.size(), skb ) == SkbError::Counts );
	}

	for ( SkbError error : { SkbError::None, SkbError::Size, SkbError::Ident,
		SkbError::Version, SkbError::Counts, SkbError::Offset, SkbError::Extent,
		SkbError::Name, SkbError::Order, SkbError::Parent, SkbError::Bone } ) {
		CHECK( SkbErrorName( error ) && SkbErrorName( error )[0] );
		CHECK( std::strcmp( SkbErrorName( error ), "unknown" ) );
	}
}

int main()
{
	CheckParse();
	CheckRejections();
	std::cout << "Alice SKL skeleton reader tests passed\n";
	return 0;
}
