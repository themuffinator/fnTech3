/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Alice's `TAN ` 2 model boundary. Every
 * model here is written by this test; no retail model is used. */

#include "../code/compat/alice_tan_format.hpp"

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
	std::memset( file.data() + at, 0, NameBytes );
	std::memcpy( file.data() + at, text, std::strlen( text ) );
}

/* Two frames, two surfaces of different sizes, and two tags -- enough that a
 * wrong stride or a wrong surface end lands somewhere it should not. */
static const int kFrames = 2;
static const int kTags = 2;
static const int kSurfaces = 2;
static const int kVerts[kSurfaces] = { 3, 5 };
static const int kTris[kSurfaces] = { 1, 4 };

static Bytes Fixture()
{
	Bytes file( TanHeaderBytes, 0 );
	PutInt( file, 0, std::int32_t( TanIdent ) );
	PutInt( file, 4, TanVersion );
	PutName( file, 8, "models/alice/fixture.tan" );
	PutInt( file, 72, kFrames );
	PutInt( file, 76, kTags );
	PutInt( file, 80, kSurfaces );
	PutInt( file, 84, 0x3f800000 );        /* totaltime, as bits */

	const std::size_t framesAt = file.size();
	file.resize( file.size() + FrameBytes * kFrames, 0 );
	PutInt( file, 100, std::int32_t( framesAt ) );

	for ( int t = 0; t < kTags; ++t ) {
		const std::size_t tagAt = file.size();
		file.resize( file.size() + NameBytes + TagDataBytes * kFrames, 0 );
		PutName( file, tagAt, t ? "tag_head" : "tag_weapon" );
		PutInt( file, 108 + 4 * std::size_t( t ), std::int32_t( tagAt ) );
	}

	const std::size_t surfacesAt = file.size();
	PutInt( file, 104, std::int32_t( surfacesAt ) );
	for ( int s = 0; s < kSurfaces; ++s ) {
		const std::size_t at = file.size();
		const std::size_t tris = TriangleBytes * std::size_t( kTris[s] );
		const std::size_t collapse = CollapseBytes * std::size_t( kVerts[s] );
		const std::size_t st = TexCoordBytes * std::size_t( kVerts[s] );
		const std::size_t xyz = XyzNormalBytes * std::size_t( kVerts[s] ) * kFrames;
		const std::size_t end = SurfaceHeaderBytes + tris + collapse + st + xyz;
		file.resize( file.size() + end, 0 );
		PutInt( file, at, 0 );
		PutName( file, at + 4, s ? "body" : "head" );
		PutInt( file, at + 68, kFrames );
		PutInt( file, at + 72, kVerts[s] );
		PutInt( file, at + 76, 0 );                 /* minLod */
		PutInt( file, at + 80, kTris[s] );
		std::size_t cursor = SurfaceHeaderBytes;
		PutInt( file, at + 84, std::int32_t( cursor ) ); cursor += tris;
		PutInt( file, at + 88, std::int32_t( cursor ) ); cursor += collapse;
		PutInt( file, at + 92, std::int32_t( cursor ) ); cursor += st;
		PutInt( file, at + 96, std::int32_t( cursor ) ); cursor += xyz;
		PutInt( file, at + 100, std::int32_t( end ) );
	}

	PutInt( file, 172, std::int32_t( file.size() ) );
	return file;
}

static void CheckParse()
{
	const Bytes file = Fixture();
	Tan tan{};
	CHECK( ParseTan( file.data(), file.size(), tan ) == Error::None );
	CHECK( tan.frames == kFrames && tan.tags == kTags && tan.surfaces == kSurfaces );
	CHECK( tan.name == 8 );
	CHECK( tan.frameData >= TanHeaderBytes );
	CHECK( tan.frameData + FrameBytes * kFrames <= file.size() );

	/* Each tag's poses follow its name, one per frame, inside the file. */
	for ( int t = 0; t < kTags; ++t ) {
		CHECK( tan.tag[t].data == tan.tag[t].name + NameBytes );
		CHECK( tan.tag[t].data + TagDataBytes * kFrames <= file.size() );
	}

	/* The surfaces chain forward without overlapping, and each one's blocks
	 * lie inside it. A reader that walked a stale stride would land outside. */
	std::size_t previousEnd = 0;
	for ( int s = 0; s < kSurfaces; ++s ) {
		const TanSurface &surface = tan.surface[s];
		CHECK( surface.verts == kVerts[s] && surface.triangles == kTris[s] );
		CHECK( surface.header >= previousEnd );
		CHECK( surface.nextSurface > surface.header && surface.nextSurface <= file.size() );
		CHECK( surface.name == surface.header + 4 );
		for ( std::size_t block : { surface.triangleData, surface.collapseData,
			surface.texCoordData, surface.xyzNormalData } ) {
			CHECK( block >= surface.header + SurfaceHeaderBytes );
			CHECK( block < surface.nextSurface );
		}
		/* The vertex block carries every frame, not just one. */
		CHECK( surface.xyzNormalData
			+ XyzNormalBytes * std::size_t( surface.verts ) * kFrames
			<= surface.nextSurface );
		previousEnd = surface.nextSurface;
	}
	CHECK( previousEnd == file.size() );
}

static void CheckRejections()
{
	const Bytes valid = Fixture();
	Tan tan{};

	/* A file that is not this format is refused rather than reinterpreted. */
	for ( std::uint32_t ident : { 0u, 0xffffffffu,
		std::uint32_t( ( 'P' << 24 ) | ( 'S' << 16 ) | ( 'B' << 8 ) | 'I' ) } ) {
		Bytes wrong = valid;
		std::memcpy( wrong.data(), &ident, sizeof( ident ) );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Ident );
	}
	for ( std::int32_t version : { 0, 1, 3, 12, -1 } ) {
		Bytes wrong = valid;
		PutInt( wrong, 4, version );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Version );
	}

	/* A truncated file is refused before any record is read. */
	for ( std::size_t length : { std::size_t( 0 ), std::size_t( 8 ),
		TanHeaderBytes - 1 } ) {
		CHECK( ParseTan( valid.data(), length, tan ) == Error::Size );
	}
	CHECK( ParseTan( nullptr, valid.size(), tan ) == Error::Size );

	/* The declared length must be the real one: this is what catches a file
	 * that was cut short after its header was written. */
	for ( std::int32_t claimed : { 0, -1, std::int32_t( valid.size() ) - 1,
		std::int32_t( valid.size() ) + 1 } ) {
		Bytes wrong = valid;
		PutInt( wrong, 172, claimed );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Extent );
	}

	/* Counts outside the compiler's own limits are refused, not clamped. */
	const struct { std::size_t at; std::int32_t value; } counts[] = {
		{ 72, 0 }, { 72, -1 }, { 72, MaxFrames + 1 },
		{ 76, -1 }, { 76, MaxTags + 1 },
		{ 80, -1 }, { 80, MaxSurfaces + 1 },
	};
	for ( const auto &bad : counts ) {
		Bytes wrong = valid;
		PutInt( wrong, bad.at, bad.value );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Counts );
	}

	/* An offset that leaves the file, or reaches back into the header, is
	 * refused wherever it appears. */
	for ( std::size_t at : { std::size_t( 100 ), std::size_t( 108 ) } ) {
		for ( std::int32_t value : { -1, 0, std::int32_t( TanHeaderBytes ) - 1 } ) {
			Bytes wrong = valid;
			PutInt( wrong, at, value );
			CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Offset );
		}
	}
	{
		Bytes wrong = valid;
		PutInt( wrong, 100, std::int32_t( wrong.size() ) - 1 );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Offset );
	}

	/* A name field with no terminator inside it is refused. */
	{
		Bytes wrong = valid;
		std::memset( wrong.data() + 8, 'a', NameBytes );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Name );
	}

	/* A surface that animates over a different number of frames than the model
	 * is a file assembled from two different models. */
	{
		Bytes wrong = valid;
		Tan good{};
		CHECK( ParseTan( wrong.data(), wrong.size(), good ) == Error::None );
		PutInt( wrong, good.surface[0].header + 68, kFrames + 1 );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Frames );
	}

	/* A surface block that reaches past the surface that declares it would
	 * read the next surface's vertices. */
	{
		Bytes wrong = valid;
		Tan good{};
		CHECK( ParseTan( wrong.data(), wrong.size(), good ) == Error::None );
		const std::size_t header = good.surface[0].header;
		const std::int32_t end = std::int32_t( good.surface[0].nextSurface - header );
		PutInt( wrong, header + 96, end );          /* vertices start at the end */
		const Error error = ParseTan( wrong.data(), wrong.size(), tan );
		CHECK( error == Error::Order || error == Error::Extent );
	}

	/* A surface whose declared end leaves the file is refused. */
	{
		Bytes wrong = valid;
		Tan good{};
		CHECK( ParseTan( wrong.data(), wrong.size(), good ) == Error::None );
		PutInt( wrong, good.surface[0].header + 100, std::int32_t( wrong.size() ) );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Extent );
	}
	{
		Bytes wrong = valid;
		Tan good{};
		CHECK( ParseTan( wrong.data(), wrong.size(), good ) == Error::None );
		PutInt( wrong, good.surface[0].header + 100, 0 );
		CHECK( ParseTan( wrong.data(), wrong.size(), tan ) == Error::Extent );
	}

	/* Every error has a name, so a refusal can be reported rather than
	 * counted. */
	for ( Error error : { Error::None, Error::Size, Error::Ident, Error::Version,
		Error::Counts, Error::Offset, Error::Extent, Error::Name, Error::Order,
		Error::Frames } ) {
		CHECK( ErrorName( error ) && ErrorName( error )[0] );
		CHECK( std::strcmp( ErrorName( error ), "unknown" ) );
	}
}

int main()
{
	CheckParse();
	CheckRejections();
	std::cout << "Alice TAN 2 model reader tests passed\n";
	return 0;
}
