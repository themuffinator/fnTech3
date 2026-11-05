/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * q_shared.c's token skipper on inputs a console line can hold. Found by
 * fuzzing (docs/fntech3/2026-11-04-hardening-round.md): a string ending in
 * separators with fewer tokens than asked stepped over its own NUL and went
 * on counting separators in the memory after it. */
#include "../code/qcommon/q_shared.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {

void QDECL Com_Printf( const char *, ... ) {
}

void QDECL Com_Error( errorParm_t, const char *, ... ) {
	std::fputs( "unexpected Com_Error in q_shared text test\n", stderr );
	std::abort();
}

float Q_atof( const char *text ) {
	return static_cast<float>( std::atof( text ) );
}

} // extern "C"

#define CHECK( expression ) do { if ( !( expression ) ) { \
	std::fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression ); return 1; } } while ( false )

int main() {
	// The tokens a bind or cvar command skips, as before.
	{
		const char line[] = "bind x +attack";
		CHECK( std::strcmp( Com_SkipTokens( line, 1, " " ), "x +attack" ) == 0 );
		CHECK( std::strcmp( Com_SkipTokens( line, 2, " " ), "+attack" ) == 0 );
		CHECK( Com_SkipTokens( line, 3, " " ) == line );
		CHECK( Com_SkipTokens( line, 0, " " ) == line );
	}
	{
		const char line[] = "set   name    value";
		CHECK( std::strcmp( Com_SkipTokens( line, 2, " " ), "value" ) == 0 );
		const char trailing[] = "a b ";
		CHECK( std::strcmp( Com_SkipTokens( trailing, 2, " " ), "" ) == 0 );
	}
	// Fewer tokens than asked, ending in a separator: the text after the NUL
	// must never be read. Before the fix, "x " asked for two tokens counted
	// the separator after the NUL and returned a pointer into "a b".
	{
		const char memory[] = "x \0 a b";
		const char *result = Com_SkipTokens( memory, 2, " " );
		CHECK( result == memory );
	}
	{
		const char memory[] = "\0 a b c";
		CHECK( Com_SkipTokens( memory, 1, " " ) == memory );
		const char separators[] = "   \0   ";
		CHECK( Com_SkipTokens( separators, 2, " " ) == separators );
	}
	// Com_SkipCharset stops at the NUL, which no set holds.
	{
		const char memory[] = "  \0  x";
		CHECK( *Com_SkipCharset( memory, " " ) == '\0' );
		CHECK( Com_SkipCharset( memory, " " ) == memory + 2 );
	}
	// Info_Tokenize reads at most MAX_INFO_STRING - 1 characters (found by
	// fuzzing, 2026-11-04): a longer string had written past its token
	// buffer and arrays. Every caller passes a userinfo bounded that way, so
	// a short string tokenises as before.
	{
		Info_Tokenize( "\\name\\Munro\\model\\munro/default\\\\empty\\" );
		CHECK( std::strcmp( Info_ValueForKeyToken( "name" ), "Munro" ) == 0 );
		CHECK( std::strcmp( Info_ValueForKeyToken( "MODEL" ), "munro/default" ) == 0 );
		CHECK( std::strcmp( Info_ValueForKeyToken( "empty" ), "" ) == 0 );
		std::string big;
		for ( int i = 0; big.size() < 4 * MAX_INFO_STRING; ++i ) {
			big += "\\k" + std::to_string( i ) + "\\v" + std::to_string( i );
		}
		Info_Tokenize( big.c_str() );
		CHECK( std::strcmp( Info_ValueForKeyToken( "k0" ), "v0" ) == 0 );
		CHECK( std::strcmp( Info_ValueForKeyToken( "k10" ), "v10" ) == 0 );
		CHECK( std::strcmp( Info_ValueForKeyToken( "k400" ), "" ) == 0 );
		// pairs as short as they can be, to the token arrays' limit and past it
		std::string pairs;
		while ( pairs.size() < 2 * MAX_INFO_STRING ) pairs += "a\\\\";
		Info_Tokenize( pairs.c_str() );
		CHECK( std::strcmp( Info_ValueForKeyToken( "a" ), "" ) == 0 );
		const std::string keys( 3 * MAX_INFO_STRING, 'x' );
		Info_Tokenize( keys.c_str() );
		CHECK( std::strlen( Info_ValueForKeyToken( "" ) ) == 0 );
	}
	std::puts( "q_shared text: ok" );
	return 0;
}
