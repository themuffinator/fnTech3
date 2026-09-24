/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Alice's `SKAN` animation boundary.
 * Every animation here is written by this test; no retail asset is used. */

#include "../code/compat/alice_ska_format.hpp"

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

static void PutFloat( Bytes &file, std::size_t at, float value )
{
	std::memcpy( file.data() + at, &value, sizeof( value ) );
}

static const int kFrames = 3;
static const int kBones = 5;
static const float kFrameTime = 0.05f;

static Bytes Fixture( int version )
{
	const std::size_t header = version == 2 ? SkaHeaderBytesV2 : SkaHeaderBytesV3;
	const std::size_t pose = version == 2 ? SkaPoseBytesV2 : SkaPoseBytesV3;
	const std::size_t stride = SkaFrameHeaderBytes + pose * kBones;
	Bytes file( header + stride * kFrames, 0 );

	const std::uint32_t ident = SkaIdent;
	std::memcpy( file.data(), &ident, sizeof( ident ) );
	PutInt( file, 4, version );
	std::memcpy( file.data() + 8, "fixture.ska", 11 );

	/* Version 3 inserted one integer before the counts. */
	const std::size_t counts = version == 2 ? 72 : 76;
	PutInt( file, counts, kFrames );
	PutInt( file, counts + 4, kBones );
	PutFloat( file, counts + 8, kFrames * kFrameTime );
	PutFloat( file, counts + 12, kFrameTime );

	/* A unit quaternion in every pose, so a reader that lands inside one
	 * instead of on it can be told apart from one that does not. */
	for ( int f = 0; f < kFrames; ++f ) {
		std::size_t at = header + stride * std::size_t( f ) + SkaFrameHeaderBytes;
		for ( int b = 0; b < kBones; ++b ) {
			if ( version == 2 ) {
				PutFloat( file, at + 0, 0.0f );
				PutFloat( file, at + 4, 0.0f );
				PutFloat( file, at + 8, 0.0f );
				PutFloat( file, at + 12, 1.0f );
			} else {
				const std::int16_t one = 32767;
				std::memcpy( file.data() + at + 6, &one, sizeof( one ) );
			}
			at += pose;
		}
	}
	return file;
}

static void CheckParse()
{
	for ( int version : { SkaVersionLow, SkaVersionHigh } ) {
		const Bytes file = Fixture( version );
		Ska ska{};
		CHECK( ParseSka( file.data(), file.size(), ska ) == SkaError::None );
		CHECK( ska.version == version );
		CHECK( ska.frames == kFrames && ska.bones == kBones );
		CHECK( ska.name == 8 );
		CHECK( ska.frameTime == kFrameTime );
		CHECK( ska.headerBytes == ( version == 2 ? SkaHeaderBytesV2 : SkaHeaderBytesV3 ) );
		CHECK( ska.poseBytes == ( version == 2 ? SkaPoseBytesV2 : SkaPoseBytesV3 ) );
		CHECK( ska.frameStride == SkaFrameHeaderBytes + ska.poseBytes * kBones );
		CHECK( ska.frameData == ska.headerBytes );

		/* Every located pose lies inside the file, frames do not overlap, and
		 * the last pose ends exactly at the end. */
		std::size_t previous = 0;
		for ( int f = 0; f < kFrames; ++f ) {
			const std::size_t frameAt = SkaFrameAt( ska, f );
			CHECK( frameAt >= previous );
			CHECK( frameAt + ska.frameStride <= file.size() );
			for ( int b = 0; b < kBones; ++b ) {
				const std::size_t at = SkaPoseAt( ska, f, b );
				CHECK( at >= frameAt + SkaFrameHeaderBytes );
				CHECK( at + ska.poseBytes <= frameAt + ska.frameStride );
				/* The quaternion the fixture wrote is where the reader says. */
				if ( version == 2 ) {
					float w = 0;
					std::memcpy( &w, file.data() + at + 12, sizeof( w ) );
					CHECK( w == 1.0f );
				} else {
					std::int16_t w = 0;
					std::memcpy( &w, file.data() + at + 6, sizeof( w ) );
					CHECK( w == 32767 );
				}
			}
			previous = frameAt + ska.frameStride;
		}
		CHECK( previous == file.size() );
	}
}

static void CheckRejections()
{
	Ska ska{};
	for ( int version : { SkaVersionLow, SkaVersionHigh } ) {
		const Bytes valid = Fixture( version );
		const std::size_t counts = version == 2 ? 72 : 76;

		for ( std::uint32_t ident : { 0u, 0xffffffffu,
			std::uint32_t( ( ' ' << 24 ) | ( 'L' << 16 ) | ( 'K' << 8 ) | 'S' ) } ) {
			Bytes wrong = valid;
			std::memcpy( wrong.data(), &ident, sizeof( ident ) );
			CHECK( ParseSka( wrong.data(), wrong.size(), ska ) == SkaError::Ident );
		}
		for ( std::int32_t bad : { 0, 1, 4, -1 } ) {
			Bytes wrong = valid;
			PutInt( wrong, 4, bad );
			CHECK( ParseSka( wrong.data(), wrong.size(), ska ) == SkaError::Version );
		}

		/* The format carries no length, so the file must be exactly the size
		 * its counts require. One byte either way is a different layout. */
		{
			Bytes shortened = valid;
			shortened.pop_back();
			CHECK( ParseSka( shortened.data(), shortened.size(), ska ) == SkaError::Extent );
			Bytes padded = valid;
			padded.push_back( 0 );
			CHECK( ParseSka( padded.data(), padded.size(), ska ) == SkaError::Extent );
		}
		/* A count that does not match the file is caught by the same rule,
		 * which is the whole reason the rule is exact. */
		for ( std::size_t at : { counts, counts + 4 } ) {
			for ( std::int32_t delta : { -1, 1 } ) {
				Bytes wrong = valid;
				std::int32_t value = 0;
				std::memcpy( &value, wrong.data() + at, sizeof( value ) );
				PutInt( wrong, at, value + delta );
				CHECK( ParseSka( wrong.data(), wrong.size(), ska ) == SkaError::Extent );
			}
		}
		for ( std::int32_t bad : { 0, -1, SkaMaxFrames + 1 } ) {
			Bytes wrong = valid;
			PutInt( wrong, counts, bad );
			CHECK( ParseSka( wrong.data(), wrong.size(), ska ) == SkaError::Counts );
		}
		for ( std::int32_t bad : { 0, -1, SkaMaxBones + 1 } ) {
			Bytes wrong = valid;
			PutInt( wrong, counts + 4, bad );
			CHECK( ParseSka( wrong.data(), wrong.size(), ska ) == SkaError::Counts );
		}

		/* The timing identity is what fixed these four fields, so a file that
		 * breaks it is not read as one of these. */
		for ( float total : { 0.0f, -1.0f, kFrames * kFrameTime * 2.0f } ) {
			Bytes wrong = valid;
			PutFloat( wrong, counts + 8, total );
			CHECK( ParseSka( wrong.data(), wrong.size(), ska ) == SkaError::Timing );
		}
		for ( float frame : { 0.0f, -0.05f } ) {
			Bytes wrong = valid;
			PutFloat( wrong, counts + 12, frame );
			CHECK( ParseSka( wrong.data(), wrong.size(), ska ) == SkaError::Timing );
		}

		/* A name with no terminator inside its own field is refused. */
		{
			Bytes wrong = valid;
			std::memset( wrong.data() + 8, 'a', SkaNameBytes );
			CHECK( ParseSka( wrong.data(), wrong.size(), ska ) == SkaError::Name );
		}
	}

	const Bytes valid = Fixture( SkaVersionHigh );
	for ( std::size_t length : { std::size_t( 0 ), std::size_t( 8 ),
		SkaHeaderBytesV3 - 1 } ) {
		CHECK( ParseSka( valid.data(), length, ska ) == SkaError::Size );
	}
	CHECK( ParseSka( nullptr, valid.size(), ska ) == SkaError::Size );

	for ( SkaError error : { SkaError::None, SkaError::Size, SkaError::Ident,
		SkaError::Version, SkaError::Counts, SkaError::Extent, SkaError::Name,
		SkaError::Timing } ) {
		CHECK( SkaErrorName( error ) && SkaErrorName( error )[0] );
		CHECK( std::strcmp( SkaErrorName( error ), "unknown" ) );
	}
}

int main()
{
	CheckParse();
	CheckRejections();
	std::cout << "Alice SKAN animation reader tests passed\n";
	return 0;
}
