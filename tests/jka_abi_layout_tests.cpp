/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Hold fnTech3's Jedi Academy records to the layout its retail modules and its
 * protocol use. Every record's size and every member's offset and size is
 * asserted at compile time against the measured table, so a member that moves,
 * changes width or goes missing cannot reach a module or a packet. */

#include "../code/compat/jka_game_abi.h"

#include <cstddef>
#include <cstdio>
#include <type_traits>

/* The same three kinds the measuring probe reported: 0 integer, 1 floating
 * point, 2 anything else, which here is a nested record or a pointer. A
 * member of the right width but the wrong kind would read as nonsense. */
template<class T> constexpr int MemberKind()
{
	using Element = std::remove_all_extents_t<T>;
	return std::is_floating_point_v<Element> ? 1
		: std::is_integral_v<Element> || std::is_enum_v<Element> ? 0 : 2;
}

/* The table names Raven's types; fnTech3's carry its own prefix. */
using vmCvar_t = fntech3_jka_vmCvar_t;
using cplane_t = fntech3_jka_cplane_t;
using trace_t = fntech3_jka_trace_t;
using trajectory_t = fntech3_jka_trajectory_t;
using usercmd_t = fntech3_jka_usercmd_t;
using forcedata_t = fntech3_jka_forcedata_t;
using entityState_t = fntech3_jka_entityState_t;
using playerState_t = fntech3_jka_playerState_t;
using entityShared_t = fntech3_jka_entityShared_t;
using sharedEntity_t = fntech3_jka_sharedEntity_t;
using siegePers_t = fntech3_jka_siegePers_t;

#define FNTECH3_JKA_WIRE_RECORD( record, size ) \
	static_assert( sizeof( record ) == size, #record " is a different size" );
#define FNTECH3_JKA_WIRE_FIELD( record, member, offset, size, kind ) \
	static_assert( offsetof( record, member ) == offset, \
		#record "::" #member " sits at a different offset" ); \
	static_assert( sizeof( ( ( record * )0 )->member ) == size, \
		#record "::" #member " is a different size" ); \
	static_assert( MemberKind<decltype( ( ( record * )0 )->member )>() == kind, \
		#record "::" #member " holds a different kind of value" );
#include "../code/compat/jka_wire_layout.def"
#undef FNTECH3_JKA_WIRE_RECORD
#undef FNTECH3_JKA_WIRE_FIELD

/* Count what the table covers, so a record cannot quietly lose its members. */
#define FNTECH3_JKA_WIRE_RECORD( record, size ) + 0
#define FNTECH3_JKA_WIRE_FIELD( record, member, offset, size, kind ) + 1
constexpr int measuredFields = 0
#include "../code/compat/jka_wire_layout.def"
	;
#undef FNTECH3_JKA_WIRE_RECORD
#undef FNTECH3_JKA_WIRE_FIELD

#define FNTECH3_JKA_WIRE_RECORD( record, size ) + 1
#define FNTECH3_JKA_WIRE_FIELD( record, member, offset, size, kind ) + 0
constexpr int measuredRecords = 0
#include "../code/compat/jka_wire_layout.def"
	;
#undef FNTECH3_JKA_WIRE_RECORD
#undef FNTECH3_JKA_WIRE_FIELD

static_assert( measuredRecords == 11, "every shared record is measured" );
static_assert( measuredFields == 379, "every member of every record is measured" );

/* The records these numbers belong to, kept beside them so a change to one
 * without the other is a build failure rather than a wire defect. */
static_assert( sizeof( entityState_t ) == 532 && sizeof( playerState_t ) == 1552 );
static_assert( sizeof( sharedEntity_t ) == 864 && sizeof( entityShared_t ) == 112 );
static_assert( sizeof( usercmd_t ) == 28 && sizeof( trajectory_t ) == 36 );
static_assert( sizeof( forcedata_t ) == 464 && sizeof( vmCvar_t ) == 272 );
static_assert( sizeof( trace_t ) == 48 && sizeof( cplane_t ) == 20 );
static_assert( sizeof( siegePers_t ) == 12 );

/* A shared entity is the two halves the engine and the game each own, in that
 * order, with the game's own fields between them. */
static_assert( offsetof( sharedEntity_t, s ) == 0 );
static_assert( offsetof( sharedEntity_t, r ) == 560 );
static_assert( sizeof( ( ( sharedEntity_t * )0 )->s ) == sizeof( entityState_t ) );
static_assert( sizeof( ( ( sharedEntity_t * )0 )->r ) == sizeof( entityShared_t ) );

/* Raven's limits, which the records and the protocol both depend on. */
static_assert( FNTECH3_JKA_MAX_CLIENTS == 32 && FNTECH3_JKA_MAX_GENTITIES == 1024 );
static_assert( 1 << FNTECH3_JKA_GENTITYNUM_BITS == FNTECH3_JKA_MAX_GENTITIES );
static_assert( FNTECH3_JKA_MAX_CONFIGSTRINGS == 1700 );
static_assert( FNTECH3_JKA_MAX_GAMESTATE_CHARS == 16000 );

int main()
{
	std::printf( "Jedi Academy record layouts: %d members across %d records\n",
		measuredFields, measuredRecords );
	return 0;
}
