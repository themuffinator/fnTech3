/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Jedi Academy multiplayer module numbers. These are recorded interoperability
 * facts with no engine behaviour behind them yet: the checks below prove the
 * table is internally consistent and that it answers for no other profile. */

#include "../code/compat/jka_module_abi.h"
#include "../code/qcommon/game_manager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::fprintf( stderr, "%d: %s\n", __LINE__, #x ); std::exit( 1 ); } } while ( 0 )

namespace jka = fntech3::jka::module;
using jka::Call;
using jka::Direction;
using jka::Module;

static unsigned Count( Module module, Direction direction )
{
	unsigned count = 0;
	for ( const auto &descriptor : jka::descriptors ) {
		if ( descriptor.module == module && descriptor.direction == direction ) ++count;
	}
	return count;
}

static int Highest( Module module, Direction direction )
{
	int highest = -1;
	for ( const auto &descriptor : jka::descriptors ) {
		if ( descriptor.module == module && descriptor.direction == direction
			&& descriptor.ordinal > highest ) highest = descriptor.ordinal;
	}
	return highest;
}

int main()
{
	/* Raven's own counts, read from its legacy enumerations. A module that
	 * gained or lost a call would change what the retail library expects. */
	struct Expected { Module module; Direction direction; unsigned count; int highest; };
	for ( auto expected : { Expected{ Module::GAME, Direction::IMPORT, 329, 634 },
		{ Module::GAME, Direction::EXPORT, 40, 39 },
		{ Module::CGAME, Direction::IMPORT, 233, 350 },
		{ Module::CGAME, Direction::EXPORT, 32, 31 },
		{ Module::UI, Direction::IMPORT, 150, 235 },
		{ Module::UI, Direction::EXPORT, 12, 11 } } ) {
		CHECK( Count( expected.module, expected.direction ) == expected.count );
		CHECK( Highest( expected.module, expected.direction ) == expected.highest );
	}
	static_assert( ( unsigned )Call::Count == 796 + 1, "Raven's own call count" );
	static_assert( jka::maxOrdinal == 634, "the highest number any module passes" );

	// Every call resolves from its own number and back again, for multiplayer
	// alone. No other profile, module or direction answers for it.
	for ( const auto &descriptor : jka::descriptors ) {
		CHECK( jka::Lookup( COM_GAME_JKA_MP, descriptor.module, descriptor.direction,
			descriptor.ordinal ) == descriptor.call );
		CHECK( jka::Ordinal( COM_GAME_JKA_MP, descriptor.call ) == descriptor.ordinal );
		CHECK( jka::Describe( descriptor.call ) == &descriptor );
		CHECK( descriptor.name && descriptor.name[0] );
		for ( auto other : { COM_GAME_JKA_SP, COM_GAME_QUAKE3, COM_GAME_QUAKELIVE,
			COM_GAME_RTCW_SP, COM_GAME_RTCW_MP, COM_GAME_WOLFET, COM_GAME_STVEF_SP,
			COM_GAME_STVEF_MP, COM_GAME_QUAKE3_IHV, COM_GAME_AUTO, COM_GAME_COUNT } ) {
			CHECK( jka::Lookup( other, descriptor.module, descriptor.direction,
				descriptor.ordinal ) == Call::Invalid );
			CHECK( jka::Ordinal( other, descriptor.call ) == -1 );
		}
		const auto opposite = descriptor.direction == Direction::IMPORT
			? Direction::EXPORT : Direction::IMPORT;
		const auto resolved = jka::Lookup( COM_GAME_JKA_MP, descriptor.module,
			opposite, descriptor.ordinal );
		CHECK( resolved == Call::Invalid || resolved != descriptor.call );
	}

	// Raven left holes where it moved a block to a round number; a hole is an
	// explicit refusal, not the neighbouring call.
	struct Hole { Module module; Direction direction; int ordinal; };
	for ( auto hole : { Hole{ Module::GAME, Direction::IMPORT, 79 },
		{ Module::GAME, Direction::IMPORT, 99 },
		{ Module::CGAME, Direction::IMPORT, 66 },
		{ Module::UI, Direction::IMPORT, 96 },
		{ Module::UI, Direction::IMPORT, 99 } } ) {
		CHECK( jka::Lookup( COM_GAME_JKA_MP, hole.module, hole.direction,
			hole.ordinal ) == Call::Invalid );
	}

	// Numbers outside any table, and invalid enumerations, resolve to nothing.
	for ( intptr_t ordinal : { intptr_t( -1 ), intptr_t( 635 ), intptr_t( 4096 ),
		intptr_t( INTPTR_MAX ) } ) {
		CHECK( jka::Lookup( COM_GAME_JKA_MP, Module::GAME, Direction::IMPORT, ordinal )
			== Call::Invalid );
	}
	CHECK( jka::Lookup( COM_GAME_JKA_MP, (Module)3, Direction::IMPORT, 0 ) == Call::Invalid );
	CHECK( jka::Lookup( COM_GAME_JKA_MP, Module::GAME, (Direction)2, 0 ) == Call::Invalid );
	CHECK( !jka::Describe( Call::Invalid ) && !jka::Describe( Call::Count ) );
	CHECK( jka::Ordinal( COM_GAME_JKA_MP, Call::Invalid ) == -1 );

	// The anchors Raven kept from Quake III, and the blocks it started at a
	// round number of its own.
	struct Anchor { Call call; int ordinal; const char *name; };
	for ( auto anchor : { Anchor{ Call::G_PRINT, 0, "G_PRINT" },
		{ Call::G_ERROR, 1, "G_ERROR" },
		{ Call::G_MEMSET, 100, "G_MEMSET" },
		{ Call::G_NAV_INIT, 200, "G_NAV_INIT" },
		{ Call::BOTLIB_SETUP, 250, "BOTLIB_SETUP" },
		{ Call::BOTLIB_AAS_ENABLE_ROUTING_AREA, 300, "BOTLIB_AAS_ENABLE_ROUTING_AREA" },
		{ Call::BOTLIB_EA_SAY, 400, "BOTLIB_EA_SAY" },
		{ Call::BOTLIB_AI_LOAD_CHARACTER, 500, "BOTLIB_AI_LOAD_CHARACTER" },
		{ Call::G_BOT_CALCULATEPATHS, 634, "G_BOT_CALCULATEPATHS" },
		{ Call::GAME_INIT, 0, "GAME_INIT" },
		{ Call::CG_PRINT, 0, "CG_PRINT" },
		{ Call::CGAME_MEMSET, 100, "CGAME_MEMSET" },
		{ Call::UI_ERROR, 0, "UI_ERROR" },
		{ Call::UI_MEMSET, 100, "UI_MEMSET" } } ) {
		CHECK( jka::Ordinal( COM_GAME_JKA_MP, anchor.call ) == anchor.ordinal );
		CHECK( !std::strcmp( jka::Describe( anchor.call )->name, anchor.name ) );
	}

	// The version numbers each retail module negotiates with.
	static_assert( FNTECH3_JKA_MP_GAME_API_VERSION == 1, "game API version" );
	static_assert( FNTECH3_JKA_MP_CGAME_API_VERSION == 2, "client game API version" );
	static_assert( FNTECH3_JKA_MP_UI_API_VERSION == 3, "menu API version" );
	static_assert( FNTECH3_JKA_SP_GAME_API_VERSION == 8, "retail single-player game API version" );
	static_assert( FNTECH3_JKA_G_PRINT == 0 && FNTECH3_JKA_G_MEMSET == 100,
		"the plain enumeration carries the same numbers" );

	std::puts( "Jedi Academy module ordinals, lookup and refusal tests passed" );
	return 0;
}
