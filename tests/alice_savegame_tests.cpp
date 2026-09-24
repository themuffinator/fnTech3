/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's savegame byte rules (compat/alice_savegame.hpp), pinned against
 * the retail alice.exe measurements in docs/fntech3/2026-09-23-alice-savegames.md.
 * No retail file is read here. */

#include "../code/compat/alice_savegame.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::savegame;

static void CheckNames() {
	char path[PathBytes];

	CHECK( ArchiveFileName( "config.cfg", "quicksave", "sav", path ) );
	CHECK( !std::strcmp( path, "save/config_quicksave.sav" ) );
	/* Cut at the first dot, not the last. */
	CHECK( ArchiveFileName( "my.pro.cfg", "x", "tga", path ) );
	CHECK( !std::strcmp( path, "save/my_x.tga" ) );
	/* A config with no dot is used whole; the list pattern is a name too. */
	CHECK( ArchiveFileName( "alice", "*", "ssv", path ) );
	CHECK( !std::strcmp( path, "save/alice_*.ssv" ) );
	CHECK( ArchiveFileName( "config.cfg", "current", "spv", path ) );
	CHECK( !std::strcmp( path, "save/config_current.spv" ) );

	/* Refusals: traversal in either part, an empty name, and a path that
	 * would not fit, which retail truncates silently. */
	CHECK( !ArchiveFileName( "config.cfg", "../x", "sav", path ) && !path[0] );
	CHECK( !ArchiveFileName( "config.cfg", "a/b", "sav", path ) );
	CHECK( !ArchiveFileName( "config.cfg", "a\\b", "sav", path ) );
	CHECK( !ArchiveFileName( "../config.cfg", "x", "sav", path ) );
	CHECK( !ArchiveFileName( "sub/config.cfg", "x", "sav", path ) );
	CHECK( !ArchiveFileName( "config.cfg", "", "sav", path ) );
	CHECK( !ArchiveFileName( "config.cfg", nullptr, "sav", path ) );
	const std::string longName( 120, 'n' );
	CHECK( !ArchiveFileName( "config.cfg", longName.c_str(), "sav", path ) && !path[0] );
	/* "save/" + "config" + "_" + name + ".sav" is 16 bytes around the name,
	 * so 111 characters is the last name that fits in 127. */
	CHECK( ArchiveFileName( "config.cfg", std::string( 111, 'n' ).c_str(), "sav", path ) );
	CHECK( std::strlen( path ) == PathBytes - 1 );
	CHECK( !ArchiveFileName( "config.cfg", std::string( 112, 'n' ).c_str(), "sav", path ) );

	CHECK( NameAllowed( "quicksave" ) );
	CHECK( NameAllowed( "garden10000" ) );
	CHECK( NameAllowed( std::string( 63, 'a' ).c_str() ) );
	CHECK( !NameAllowed( std::string( 64, 'a' ).c_str() ) );
	CHECK( !NameAllowed( "" ) );
	CHECK( !NameAllowed( nullptr ) );
	CHECK( !NameAllowed( ".." ) );
	CHECK( !NameAllowed( "a..b" ) );
	CHECK( !NameAllowed( "a/b" ) );
	CHECK( !NameAllowed( "a\\b" ) );

	char name[NameBytes];
	char prefix[NameBytes];
	AutosavePrefix( "garden1", prefix );
	CHECK( AutosaveName( prefix, 0, name ) && !std::strcmp( name, "garden10000" ) );
	CHECK( AutosaveName( prefix, 42, name ) && !std::strcmp( name, "garden10042" ) );
	CHECK( AutosaveName( prefix, 9999, name ) && !std::strcmp( name, "garden19999" ) );
	CHECK( !AutosaveName( prefix, 10000, name ) && !name[0] );
	CHECK( !AutosaveName( prefix, -1, name ) );
	AutosavePrefix( "a/b/c", prefix );
	CHECK( !std::strcmp( prefix, "a_b_c" ) );
	CHECK( AutosaveName( prefix, 7, name ) && !std::strcmp( name, "a_b_c0007" ) );
	/* 59 characters leave room for the four digits; 60 do not. */
	CHECK( AutosaveName( std::string( 59, 'm' ).c_str(), 1, name ) );
	CHECK( !AutosaveName( std::string( 60, 'm' ).c_str(), 1, name ) );
}

static void CheckHeader() {
	static_assert( HeaderBytes == 200, "SV_ArchiveServerFile writes 200 bytes" );
	static_assert( HeaderSaveTime == 0 && HeaderLevelTime == 4, "the two times" );
	static_assert( HeaderComment == 8 && HeaderMap == 0x48 && HeaderName == 0x88, "the three strings" );

	ServerHeader header;
	header.saveTime = 0x5f5e1000;
	header.levelTime = -123456;
	std::snprintf( header.comment, sizeof( header.comment ), "%s", "Starting The Vale of Tears" );
	std::snprintf( header.mapName, sizeof( header.mapName ), "%s", "garden1" );
	std::snprintf( header.saveName, sizeof( header.saveName ), "%s", "garden10003" );

	uint8_t bytes[HeaderBytes];
	Encode( header, bytes );
	/* Little endian, at the measured offsets. */
	CHECK( bytes[0] == 0x00 && bytes[1] == 0x10 && bytes[2] == 0x5e && bytes[3] == 0x5f );
	CHECK( ReadInt32( bytes + 4 ) == -123456 );
	CHECK( !std::memcmp( bytes + 8, "Starting The Vale of Tears", 27 ) );
	CHECK( !std::memcmp( bytes + 0x48, "garden1", 8 ) );
	CHECK( !std::memcmp( bytes + 0x88, "garden10003", 12 ) );
	for ( std::size_t i = 0x88 + 12; i < HeaderBytes; ++i ) {
		CHECK( bytes[i] == 0 );
	}

	ServerHeader back;
	Decode( bytes, back );
	CHECK( back.saveTime == header.saveTime && back.levelTime == header.levelTime );
	CHECK( !std::strcmp( back.comment, header.comment ) );
	CHECK( !std::strcmp( back.mapName, header.mapName ) );
	CHECK( !std::strcmp( back.saveName, header.saveName ) );

	/* A field written full has no terminator in the file (strncpy); the
	 * reader bounds it. The comment is Com_sprintf'd, so it keeps one. */
	ServerHeader full;
	const std::string sixtyFour( 64, 'z' );
	std::memcpy( full.mapName, sixtyFour.data(), 64 );
	full.mapName[64] = '\0';
	std::memcpy( full.saveName, sixtyFour.data(), 64 );
	std::memcpy( full.comment, sixtyFour.data(), 64 );
	Encode( full, bytes );
	CHECK( bytes[0x48 + 63] == 'z' && bytes[0x88 + 63] == 'z' );
	CHECK( bytes[0x08 + 62] == 'z' && bytes[0x08 + 63] == 0 );
	Decode( bytes, back );
	CHECK( std::strlen( back.mapName ) == 64 && std::strlen( back.saveName ) == 64 );
	CHECK( std::strlen( back.comment ) == 63 );

	/* Garbage that runs into the next field is still bounded per field. */
	std::memset( bytes, 'q', sizeof( bytes ) );
	Decode( bytes, back );
	CHECK( std::strlen( back.comment ) == 64 && std::strlen( back.mapName ) == 64
		&& std::strlen( back.saveName ) == 64 );

	std::size_t portal = 0;
	CHECK( PortalBytes( 1, portal ) && portal == 8 );
	CHECK( PortalBytes( 17, portal ) && portal == 4 + 4 * 17 * 17 );
	CHECK( PortalBytes( MaxAreas, portal ) && portal == 4 + 4 * 256 * 256 );
	CHECK( !PortalBytes( 0, portal ) && portal == 0 );
	CHECK( !PortalBytes( -1, portal ) );
	CHECK( !PortalBytes( MaxAreas + 1, portal ) );
}

/* A frame of one colour, `pad` extra bytes on each row. */
static std::vector<uint8_t> Frame( int width, int height, int pad, uint8_t r, uint8_t g, uint8_t b ) {
	std::vector<uint8_t> frame( static_cast<std::size_t>( width * 3 + pad ) * height, 0xee );
	for ( int y = 0; y < height; ++y ) {
		for ( int x = 0; x < width; ++x ) {
			uint8_t *p = &frame[static_cast<std::size_t>( y ) * ( width * 3 + pad ) + x * 3];
			p[0] = r;
			p[1] = g;
			p[2] = b;
		}
	}
	return frame;
}

/* Retail's second pass: times the float 2.55f, compared and truncated at
 * more than float precision. */
static uint8_t Gain( int value ) {
	const double scaled = static_cast<double>( value ) * static_cast<double>( 2.55f );
	return static_cast<uint8_t>( scaled > 255.0 ? 255 : static_cast<int>( scaled ) );
}

static uint8_t Uniform( uint8_t r, uint8_t g, uint8_t b ) {
	const std::vector<uint8_t> frame = Frame( 64, 48, 0, r, g, b );
	uint8_t out[4 * 4 * 3];
	Saveshot( frame.data(), 64, 48, 64 * 3, out, 4, 4 );
	for ( std::size_t i = 1; i < sizeof( out ); ++i ) {
		CHECK( out[i] == out[0] );
	}
	return out[0];
}

static void CheckSaveshot() {
	/* Red wins; 100 * 2.55f is 254.9999952, truncated. Float arithmetic
	 * would round the product to 255.0 first, which is the retail
	 * difference this pins. */
	CHECK( Uniform( 100, 50, 20 ) == 254 );
	CHECK( Gain( 100 ) == 254 );
	/* The clamp. */
	CHECK( Uniform( 120, 0, 0 ) == 255 );
	CHECK( Uniform( 0, 0, 0 ) == 0 );
	/* Blue wins when it beats both. */
	CHECK( Uniform( 10, 20, 90 ) == Gain( 90 ) );
	/* A tie between the two largest falls to green, even when green is the
	 * smallest: red must beat both and so must blue. */
	CHECK( Uniform( 50, 50, 10 ) == 127 );
	CHECK( Uniform( 60, 10, 60 ) == 25 );
	CHECK( Uniform( 10, 10, 10 ) == 25 );

	/* The sample positions: 1280 by 960 into 256 is a step of 1.25 both
	 * ways, so the first output column reads source columns 0 to 3 and the
	 * second reads 5 to 8, skipping 4; rows likewise read 0 to 2 and 3, 5, 6,
	 * skipping 4. */
	const int w = 1280, h = 960;
	std::vector<uint8_t> frame = Frame( w, h, 0, 0, 0, 0 );
	std::vector<uint8_t> out( 256 * 256 * 3 );
	auto set = [&]( int x, int y, uint8_t v ) {
		uint8_t *p = &frame[( static_cast<std::size_t>( y ) * w + x ) * 3];
		p[0] = v;
	};
	set( 4, 0, 255 );   /* never sampled */
	set( 0, 4, 255 );   /* never sampled */
	set( 5, 0, 120 );   /* output column 1, row 0 */
	set( 0, 3, 240 );   /* output column 0, row 1 */
	Saveshot( frame.data(), w, h, w * 3, out.data() );
	CHECK( out[0] == 0 );
	CHECK( out[3] == Gain( 120 / 12 ) );
	CHECK( out[256 * 3] == Gain( 240 / 12 ) );
	CHECK( out[3 + 1] == out[3] && out[3 + 2] == out[3] );

	/* The stride is honoured: padding bytes, here 0xee, are never read. */
	const std::vector<uint8_t> padded = Frame( 30, 21, 2, 12, 12, 12 );
	uint8_t small[10 * 7 * 3];
	Saveshot( padded.data(), 30, 21, 30 * 3 + 2, small, 10, 7 );
	for ( uint8_t v : small ) {
		CHECK( v == Gain( 12 ) );
	}
	/* A stride shorter than a row is refused rather than read past. */
	std::memset( small, 0x77, sizeof( small ) );
	Saveshot( padded.data(), 30, 21, 30 * 3 - 1, small, 10, 7 );
	CHECK( small[0] == 0x77 );

	uint8_t header[TgaHeaderBytes];
	TgaHeader( header, 256, 256 );
	const uint8_t expected[TgaHeaderBytes] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0x00, 0x01, 0x00, 0x01, 24, 0 };
	CHECK( !std::memcmp( header, expected, sizeof( header ) ) );
	static_assert( ShotBytes == 196626, "18 + 256 * 256 * 3" );
}

static uint16_t TableCRC( const uint8_t *data, int length ) {
	uint16_t table[256];
	for ( int i = 0; i < 256; ++i ) {
		uint16_t c = static_cast<uint16_t>( i << 8 );
		for ( int k = 0; k < 8; ++k ) {
			c = static_cast<uint16_t>( ( c & 0x8000 ) ? ( c << 1 ) ^ 0x1021 : c << 1 );
		}
		table[i] = c;
	}
	uint16_t crc = 0xffff;
	for ( int i = 0; i < length; ++i ) {
		crc = static_cast<uint16_t>( ( crc << 8 ) ^ table[( crc >> 8 ) ^ data[i]] );
	}
	return crc;
}

static void CheckCRC() {
	CHECK( CalcCRC( nullptr, 0 ) == 0xffff );
	CHECK( CalcCRC( reinterpret_cast<const uint8_t *>( "" ), 0 ) == 0xffff );
	CHECK( CalcCRC( reinterpret_cast<const uint8_t *>( "x" ), -5 ) == 0xffff );
	const char *check = "123456789";
	CHECK( CalcCRC( reinterpret_cast<const uint8_t *>( check ), 9 ) == 0x29b1 );
	/* The retail engine's table-driven form, which the bitwise one must equal
	 * on every input. */
	uint32_t seed = 12345;
	std::vector<uint8_t> buffer( 4096 );
	for ( int round = 0; round < 64; ++round ) {
		for ( uint8_t &b : buffer ) {
			seed = seed * 1103515245u + 12345u;
			b = static_cast<uint8_t>( seed >> 16 );
		}
		const int length = static_cast<int>( seed % buffer.size() );
		CHECK( CalcCRC( buffer.data(), length ) == TableCRC( buffer.data(), length ) );
	}
}

int main() {
	CheckNames();
	CheckHeader();
	CheckSaveshot();
	CheckCRC();

	/* The values the UI's CheckErrorState reads (0x00419ff0). */
	static_assert( static_cast<int>( SaveError::None ) == 0, "no error" );
	static_assert( static_cast<int>( SaveError::DiskFull ) == 1, "0x00451e33" );
	static_assert( static_cast<int>( SaveError::Deathmatch ) == 2, "0x00451dc6" );
	static_assert( static_cast<int>( SaveError::NotInGame ) == 3, "0x00451da1" );
	static_assert( static_cast<int>( SaveError::Dead ) == 4, "0x00451dfc" );
	static_assert( static_cast<int>( SaveError::BadName ) == 5, "0x00451ef0" );
	CHECK( !std::strcmp( WipeOrder[0], "sav" ) && !std::strcmp( WipeOrder[1], "ssv" )
		&& !std::strcmp( WipeOrder[2], "spv" ) && !std::strcmp( WipeOrder[3], "tga" ) );
	static_assert( MinimumFreeKilobytes == 4096, "0x1000 at 0x00451e33" );

	std::cout << "alice savegame tests passed\n";
	return 0;
}
