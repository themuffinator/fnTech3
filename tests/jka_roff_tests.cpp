/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Jedi Academy's ROFF motion paths. Every
 * path here is written by this test; no retail file is used, and nothing is
 * moved along one. */

#include "../code/compat/jka_roff_format.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace roff = fntech3::jka::roff;
using roff::Error;
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

static void AppendString( Bytes &file, const std::string &text )
{
	for ( char c : text ) file.push_back( std::uint8_t( c ) );
	file.push_back( 0 );
}

/* Retail ships version 2 only: a frame rate, entries with note ranges, and a
 * note track of terminated strings. */
static Bytes Version2( int entries, int notes, bool useNotes = true )
{
	Bytes file;
	AppendInt( file, std::int32_t( roff::Ident ) );
	AppendInt( file, roff::Version2 );
	AppendInt( file, entries );
	AppendInt( file, 50 );                     // frame rate, as every retail path uses
	AppendInt( file, notes );
	for ( int entry = 0; entry < entries; ++entry ) {
		for ( int axis = 0; axis < 3; ++axis ) AppendFloat( file, float( entry * 4 + axis ) );
		for ( int axis = 0; axis < 3; ++axis ) AppendFloat( file, float( axis ) );
		const bool fires = useNotes && notes && entry == entries - 1;
		AppendInt( file, fires ? 0 : -1 );     // first note, -1 when none fires
		AppendInt( file, fires ? notes : 0 );
	}
	for ( int note = 0; note < notes; ++note ) AppendString( file, "note" + std::to_string( note ) );
	return file;
}

static Bytes Version1( int entries )
{
	Bytes file;
	AppendInt( file, std::int32_t( roff::Ident ) );
	AppendInt( file, roff::Version1 );
	AppendFloat( file, float( entries ) );     // the exporter wrote a float count
	for ( int entry = 0; entry < entries; ++entry ) {
		for ( int axis = 0; axis < 3; ++axis ) AppendFloat( file, float( entry ) );
		for ( int axis = 0; axis < 3; ++axis ) AppendFloat( file, 0 );
	}
	return file;
}

static void CheckVersion2()
{
	const Bytes file = Version2( 4, 2 );
	roff::Path path;
	CHECK( roff::Parse( file.data(), file.size(), path ) == Error::None );
	CHECK( path.version == roff::Version2 && path.entries == 4 );
	CHECK( path.frameRate == 50 && path.notes == 2 && path.maxNoteIndex == 1 );
	CHECK( path.entryOffset == roff::Version2HeaderBytes );
	CHECK( path.noteOffset == roff::Version2HeaderBytes + 4 * roff::Version2EntryBytes );

	// A path that fires no note at all still reads, and reports none used.
	const Bytes quiet = Version2( 3, 0 );
	CHECK( roff::Parse( quiet.data(), quiet.size(), path ) == Error::None );
	CHECK( path.notes == 0 && path.maxNoteIndex == -1 && path.entries == 3 );

	// A note track nothing points at is still a well-formed file.
	const Bytes unused = Version2( 2, 2, false );
	CHECK( roff::Parse( unused.data(), unused.size(), path ) == Error::None );
	CHECK( path.notes == 2 && path.maxNoteIndex == -1 );

	// An empty path is a header and nothing else.
	const Bytes empty = Version2( 0, 0 );
	CHECK( roff::Parse( empty.data(), empty.size(), path ) == Error::None );
	CHECK( path.entries == 0 && empty.size() == roff::Version2HeaderBytes );
}

static void CheckVersion1()
{
	const Bytes file = Version1( 3 );
	roff::Path path;
	CHECK( roff::Parse( file.data(), file.size(), path ) == Error::None );
	CHECK( path.version == roff::Version1 && path.entries == 3 );
	CHECK( path.frameRate == 0 && path.notes == 0 && path.maxNoteIndex == -1 );
	CHECK( path.entryOffset == roff::Version1HeaderBytes );
	CHECK( file.size() == roff::Version1HeaderBytes + 3 * roff::Version1EntryBytes );

	// Its count is a float, so a fractional or negative one is malformed.
	for ( float count : { 2.5f, -1.0f, 1e12f } ) {
		Bytes damaged = file;
		std::uint32_t word;
		std::memcpy( &word, &count, sizeof( word ) );
		PutInt( damaged, 8, std::int32_t( word ) );
		CHECK( roff::Parse( damaged.data(), damaged.size(), path ) != Error::None );
	}

	// Anything after the entries does not belong to a version 1 path.
	Bytes trailing = file;
	trailing.push_back( 0 );
	CHECK( roff::Parse( trailing.data(), trailing.size(), path ) == Error::Trailing );
}

static void CheckRejections()
{
	const Bytes file = Version2( 4, 2 );
	roff::Path path;

	CHECK( roff::Parse( nullptr, file.size(), path ) == Error::Size );
	for ( std::size_t length = 0; length < file.size(); ++length ) {
		CHECK( roff::Parse( file.data(), length, path ) != Error::None );
	}

	struct Damage { std::size_t at; std::int32_t value; Error error; };
	for ( auto damage : { Damage{ 0, 0x46464f52 + 1, Error::Ident },
		{ 4, 0, Error::Version }, { 4, 3, Error::Version },
		{ 8, -1, Error::Counts },                    // entries
		{ 12, 0, Error::Counts },                    // frame rate
		{ 12, -50, Error::Counts },
		{ 16, -1, Error::Counts },                   // notes
		{ 8, 1 << 24, Error::Extent },               // entries past the end
		{ 16, 4, Error::Extent } } ) {               // more notes than the file holds
		Bytes damaged = file;
		PutInt( damaged, damage.at, damage.value );
		CHECK( roff::Parse( damaged.data(), damaged.size(), path ) == damage.error );
	}

	// An entry whose note range leaves the track is refused.
	const std::size_t last = roff::Version2HeaderBytes + 3 * roff::Version2EntryBytes;
	for ( auto damage : { Damage{ last + 24, 2, Error::Note },     // start past the end
		{ last + 24, -2, Error::Note },
		{ last + 28, 3, Error::Note },                             // count past the end
		{ last + 28, -1, Error::Note } } ) {
		Bytes damaged = file;
		PutInt( damaged, damage.at, damage.value );
		CHECK( roff::Parse( damaged.data(), damaged.size(), path ) == damage.error );
	}

	// A note track that is not terminated, or that leaves bytes over.
	{
		Bytes damaged = file;
		damaged.back() = 'x';
		CHECK( roff::Parse( damaged.data(), damaged.size(), path ) == Error::Extent );
	}
	{
		Bytes damaged = file;
		damaged.push_back( 0 );
		CHECK( roff::Parse( damaged.data(), damaged.size(), path ) == Error::Trailing );
	}

	for ( auto error : { Error::None, Error::Size, Error::Ident, Error::Version,
		Error::Counts, Error::Extent, Error::Note, Error::Trailing } ) {
		const char *name = roff::ErrorName( error );
		CHECK( name && name[0] && std::strcmp( name, "unknown" ) );
	}
	CHECK( !std::strcmp( roff::ErrorName( (Error)42 ), "unknown" ) );
}

int main()
{
	CheckVersion2();
	CheckVersion1();
	CheckRejections();
	std::cout << "Jedi Academy ROFF motion path and malformed-path tests passed\n";
	return 0;
}
