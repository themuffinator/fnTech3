/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

// Holomatch server rules that retail's engine owns: the Expansion Pack mode
// conflicts, the Quake III 1.17 bot action bits its qagame reads, which game
// imports keep Quake III 1.32's contract, and Elite Force's perpendicular.

extern "C" {
#include "q_shared.h"
}

#include "../code/compat/stvef_bot_actions.hpp"
#include "../code/compat/stvef_game_imports.hpp"
#include "../code/server/server_stvef_policy.hpp"

#include <cmath>
#include <cstring>

#define CHECK( expression ) do { if ( !( expression ) ) return __LINE__; } while ( false )

using namespace fntech3::stvef;

namespace {

constexpr server::PModSet Modes( bool assimilation, bool disintegration, bool actionHero,
	bool specialties, bool elimination ) {
	return { assimilation, disintegration, actionHero, specialties, elimination };
}

// std::array's comparison is not constexpr before C++20.
constexpr bool Same( const server::PModSet &a, const server::PModSet &b ) {
	for ( std::size_t i = 0; i < a.size(); ++i ) if ( a[i] != b[i] ) return false;
	return true;
}

// Every combination is resolved in retail's order; spot checks pin each rule.
static_assert( Same( server::ResolvePMods( 3, Modes( true, false, false, false, false ) ),
	Modes( true, false, false, false, false ) ) );
static_assert( Same( server::ResolvePMods( 0, Modes( true, false, false, false, false ) ),
	Modes( false, false, false, false, false ) ), "Assimilation needs team play" );
static_assert( Same( server::ResolvePMods( 4, Modes( false, false, false, false, true ) ),
	Modes( false, false, false, false, false ) ), "no Elimination in CTF" );
static_assert( Same( server::ResolvePMods( 3, Modes( false, false, true, false, false ) ),
	Modes( false, false, false, false, false ) ), "no Action Hero in team play" );
static_assert( Same( server::ResolvePMods( 3, Modes( true, true, true, true, true ) ),
	Modes( true, false, false, true, false ) ),
	"Assimilation removes Disintegration, Elimination and Action Hero; Specialties stays" );
static_assert( Same( server::ResolvePMods( 0, Modes( false, true, true, true, true ) ),
	Modes( false, false, false, true, true ) ),
	"Specialties removes Disintegration and Action Hero first" );
static_assert( Same( server::ResolvePMods( 0, Modes( false, true, true, false, true ) ),
	Modes( false, true, false, false, true ) ), "Disintegration then Elimination" );
static_assert( Same( server::ResolvePMods( 1, Modes( false, false, true, false, false ) ),
	Modes( false, false, true, false, false ) ), "Action Hero in tournament" );

static_assert( game::SharesQuake3Import( 0 ) && game::SharesQuake3Import( 40 ) );
static_assert( !game::SharesQuake3Import( 41 ) && !game::SharesQuake3Import( 99 ) );
static_assert( game::SharesQuake3Import( 109 ) && !game::SharesQuake3Import( 112 ) );
static_assert( game::SharesQuake3Import( 211 ) && !game::SharesQuake3Import( 212 ) );
static_assert( !game::SharesQuake3Import( 300 ) && !game::SharesQuake3Import( 302 ) );
static_assert( game::SharesQuake3Import( 303 ) && game::SharesQuake3Import( 318 ) );
static_assert( !game::SharesQuake3Import( 400 ) && !game::SharesQuake3Import( 427 ) );
static_assert( game::SharesQuake3Import( 500 ) && game::SharesQuake3Import( 572 ) );
static_assert( !game::SharesQuake3Import( 573 ) );

int TestActionBits() {
	// Canonical flags to the bits BotInputToUserCommand in the retail qagame
	// tests: 0x200 delayed jump, 0x2000 alternate attack, and so on.
	const struct { int canonical; std::int32_t retail; } bits[] = {
		{ ACTION_ATTACK, 0x1 }, { ACTION_USE, 0x2 }, { ACTION_RESPAWN, 0x4 },
		{ ACTION_JUMP, 0x8 }, { ACTION_CROUCH, 0x10 }, { ACTION_MOVEFORWARD, 0x20 },
		{ ACTION_MOVEBACK, 0x40 }, { ACTION_MOVELEFT, 0x80 }, { ACTION_MOVERIGHT, 0x100 },
		{ ACTION_DELAYEDJUMP, 0x200 }, { ACTION_TALK, 0x400 }, { ACTION_GESTURE, 0x800 },
		{ ACTION_WALK, 0x1000 }, { ACTION_STVEF_ALTATTACK, 0x2000 },
		// Quake III 1.32 extras have no 1.17 meaning and are dropped.
		{ ACTION_AFFIRMATIVE, 0 }, { ACTION_JUMPEDLASTFRAME, 0 }, { ACTION_RTCW_RELOAD, 0 }
	};
	for ( const auto &bit : bits ) {
		bot_input_t input{};
		input.actionflags = bit.canonical;
		CHECK( bot::ActionOutput( input ).actionflags == bit.retail );
	}
	bot_input_t input{};
	input.thinktime = 0.1f;
	input.dir[1] = 1.0f;
	input.speed = 400.0f;
	input.viewangles[0] = -12.5f;
	input.weapon = 7;
	input.actionflags = ACTION_ATTACK | ACTION_STVEF_ALTATTACK | ACTION_MOVELEFT;
	const auto output = bot::ActionOutput( input );
	// Compare the stored float bits; x87 may retain extra precision in a
	// literal, which is unrelated to the module boundary's copy contract.
	CHECK( !std::memcmp( &output.thinktime, &input.thinktime, sizeof( input.thinktime ) ) );
	CHECK( output.dir[1] == 1.0f && output.speed == 400.0f );
	CHECK( output.viewangles[0] == -12.5f && output.weapon == 7 );
	CHECK( output.actionflags == ( 0x1 | 0x2000 | 0x80 ) );
	return 0;
}

int TestPerpendicular() {
	// A diagonal of x and z ties; Elite Force keeps z's projection plane.
	const vec3_t source = { 0.70710678f, 0.0f, 0.70710678f };
	vec3_t result{};
	game::PerpendicularVector( result, source );
	CHECK( std::fabs( DotProduct( result, source ) ) < 1e-6f );
	// Scanning from z finds y (the zero axis) either way; a tie between x and
	// z with y larger selects z, where Quake III selects x.
	const vec3_t tie = { 0.5f, 0.70710678f, 0.5f };
	game::PerpendicularVector( result, tie );
	vec3_t expected{};
	const vec3_t zAxis = { 0.0f, 0.0f, 1.0f };
	ProjectPointOnPlane( expected, zAxis, tie );
	VectorNormalize( expected );
	CHECK( std::fabs( result[0] - expected[0] ) < 1e-6f &&
		std::fabs( result[1] - expected[1] ) < 1e-6f &&
		std::fabs( result[2] - expected[2] ) < 1e-6f );
	return 0;
}

} // namespace

int main() {
	if ( const int result = TestActionBits() ) return result;
	if ( const int result = TestPerpendicular() ) return 1000 + result;
	return 0;
}
