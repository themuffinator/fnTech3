/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Medal of Honor: Allied Assault's load records, maps/<map>.min
 * (code/compat/mohaa_load_record.hpp): the version and the name-byte check
 * retail's reader makes, the kinds and cached resources left out of the
 * total, and the bar's advance as a load reaches each resource. The records
 * here are written for the test; no retail file is read. */

#include "../code/compat/mohaa_load_record.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;
#define CHECK( condition ) do { if ( !( condition ) ) { \
	std::fprintf( stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition ); \
	++failures; } } while ( 0 )

using namespace fntech3::mohaa::loadrecord;

namespace {

/* Every kind but images counts, and nothing is cached. */
bool CountAllButImages( const std::string &name ) {
	return !name.empty() && name[0] != 'n';
}

Record ParseText( const std::string &text ) {
	return Parse( text.data(), static_cast<int>( text.size() ), CountAllButImages );
}

std::string RecordText( const char *const *names, const int *times, int count, int bytesAdjust = 0,
	const char *version = "3", const char *newline = "\n" ) {
	int bytes = 0;
	for ( int i = 0; i < count; i++ ) bytes += static_cast<int>( std::strlen( names[i] ) ) + 1;
	std::string text = std::string( version ) + newline + std::to_string( count ) + " "
		+ std::to_string( bytes + bytesAdjust ) + newline;
	for ( int i = 0; i < count; i++ ) {
		text += std::string( names[i] ) + newline + std::to_string( times[i] ) + newline;
	}
	return text;
}

const char *const kNames[] = { "*113", "amodels/ammo/colt_clip.tik", "gmodels/human/walk.skc",
	"ksound/amb/fire.wav", "nenv/mohnight1_bk.tga", "i120" };
const int kTimes[] = { 1, 4, 2, 3, 50, 1 };

void CheckParse() {
	Record record = ParseText( RecordText( kNames, kTimes, 6 ) );
	CHECK( record.valid );
	CHECK( record.entries.size() == 6 );
	/* Sorted by name, and the image's 50 is not counted. */
	CHECK( record.entries.front().first == "*113" );
	CHECK( record.total == 1 + 4 + 2 + 3 + 1 );
	for ( std::size_t i = 1; i < record.entries.size(); i++ ) {
		CHECK( record.entries[i - 1].first < record.entries[i].first );
	}
	CHECK( Fraction( record ) == 0.0f );

	/* The bar advances once per resource, by its time, whatever the case of
	 * the name reported; an uncounted or unknown one leaves it. */
	CHECK( Reach( record, "AMODELS/Ammo/Colt_Clip.tik" ) );
	CHECK( record.current == 4 );
	CHECK( !Reach( record, "amodels/ammo/colt_clip.tik" ) );
	CHECK( !Reach( record, "nenv/mohnight1_bk.tga" ) );
	CHECK( !Reach( record, "kno/such/sound.wav" ) );
	CHECK( !Reach( record, "" ) );
	CHECK( record.current == 4 );
	for ( const char *name : { "*113", "gmodels/human/walk.skc", "ksound/amb/fire.wav", "i120" } ) {
		CHECK( Reach( record, name ) );
	}
	CHECK( record.current == record.total );
	CHECK( Fraction( record ) == 1.0f );
}

void CheckCachedLeftOut() {
	const std::string text = RecordText( kNames, kTimes, 6 );
	/* A cached model is left out of the total and never moves the bar. */
	Record record = Parse( text.data(), static_cast<int>( text.size() ), []( const std::string &name ) {
		return CountAllButImages( name ) && name != "amodels/ammo/colt_clip.tik";
	} );
	CHECK( record.valid && record.total == 1 + 2 + 3 + 1 );
	CHECK( !Reach( record, "amodels/ammo/colt_clip.tik" ) );
	CHECK( Reach( record, "ksound/amb/fire.wav" ) );
	CHECK( std::fabs( Fraction( record ) - 3.0f / 7.0f ) < 1e-6f );
	/* A record nothing of which is counted shows no bar. */
	Record nothing = Parse( text.data(), static_cast<int>( text.size() ), []( const std::string & ) { return false; } );
	CHECK( !nothing.valid && nothing.total == 0 && Fraction( nothing ) == 0.0f );
	CHECK( !Reach( nothing, "i120" ) );
}

void CheckLayout() {
	/* CRLF lines, blanks, spacing around a line and a trailing NUL are all
	 * read as retail's own writer leaves them. */
	std::string text = RecordText( kNames, kTimes, 6, 0, "3", "\r\n" );
	CHECK( ParseText( text ).valid );
	std::string spaced = "  3  \n\n6 " + std::to_string( 5 + 27 + 23 + 20 + 22 + 5 ) + "\n";
	for ( int i = 0; i < 6; i++ ) spaced += std::string( "\t" ) + kNames[i] + " \n\n" + std::to_string( kTimes[i] ) + "\n";
	spaced.push_back( '\0' );
	const Record record = ParseText( spaced );
	CHECK( record.valid && record.entries.size() == 6 );
	/* A negative time counts as none. */
	const char *const names[] = { "i1", "i2" };
	const int times[] = { -5, 3 };
	const Record negative = ParseText( RecordText( names, times, 2 ) );
	CHECK( negative.valid && negative.total == 3 );
}

void CheckRefused() {
	/* Another version, names that do not add up to the byte count, a count
	 * the lines do not hold, no resources, too many, and nothing at all. */
	CHECK( !ParseText( RecordText( kNames, kTimes, 6, 0, "2" ) ).valid );
	CHECK( !ParseText( RecordText( kNames, kTimes, 6, 1 ) ).valid );
	CHECK( ParseText( RecordText( kNames, kTimes, 6, 1 ) ).entries.empty() );
	CHECK( !ParseText( RecordText( kNames, kTimes, 6, -1 ) ).valid );
	std::string truncated = RecordText( kNames, kTimes, 6 );
	truncated.resize( truncated.rfind( '\n', truncated.size() - 2 ) + 1 );
	CHECK( !ParseText( truncated ).valid );
	CHECK( !ParseText( "3\n0 0\n" ).valid );
	CHECK( !ParseText( "3\n70000 10\n" ).valid );
	CHECK( !ParseText( "3\nsix bytes\n" ).valid );
	CHECK( !ParseText( "" ).valid );
	CHECK( !Parse( nullptr, 10, CountAllButImages ).valid );
	CHECK( !Parse( "3\n1 3\nab\n5\n", -1, CountAllButImages ).valid );
}

} // namespace

int main() {
	CheckParse();
	CheckCachedLeftOut();
	CheckLayout();
	CheckRefused();
	if ( failures ) {
		std::fprintf( stderr, "%d check(s) failed\n", failures );
		return 1;
	}
	std::printf( "mohaa load record tests passed\n" );
	return 0;
}
