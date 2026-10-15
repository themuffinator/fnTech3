/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Hold fnTech3's Jedi Outcast records to the layout its retail engine uses.
 * Every size and offset is asserted at compile time against the measured
 * table, so a member that moves, changes width or goes missing cannot reach a
 * module or a packet. */

#include "../code/compat/jk2_game_abi.h"

#include <cstddef>
#include <cstdio>
#include <type_traits>

template<class T> constexpr int MemberKind()
{
	using Element = std::remove_all_extents_t<T>;
	return std::is_floating_point_v<Element> ? 1
		: std::is_integral_v<Element> || std::is_enum_v<Element> ? 0 : 2;
}

/* The table names Raven's types; fnTech3's carry its own prefix. */
using usercmd_t = fntech3_jk2_usercmd_t;
using playerState_t = fntech3_jk2_playerState_t;
using trajectory_t = fntech3_jk2_trajectory_t;
using entityState_t = fntech3_jk2_entityState_t;

#define FNTECH3_JK2_WIRE_RECORD( record, size ) \
	static_assert( sizeof( record ) == ( size ), #record " is a different size" );
#define FNTECH3_JK2_WIRE_FIELD( record, member, offset, size, kind ) \
	static_assert( offsetof( record, member ) == ( offset ), #record "." #member " moved" ); \
	static_assert( sizeof( ( (record *)0 )->member ) == ( size ), #record "." #member " changed width" ); \
	static_assert( MemberKind<decltype( ( (record *)0 )->member )>() == ( kind ), \
		#record "." #member " changed kind" );
#include "../code/compat/jk2_wire_layout.def"
#undef FNTECH3_JK2_WIRE_RECORD
#undef FNTECH3_JK2_WIRE_FIELD

/* Count what the table covers, so a record cannot quietly lose its members. */
#define FNTECH3_JK2_WIRE_RECORD( record, size ) + 0
#define FNTECH3_JK2_WIRE_FIELD( record, member, offset, size, kind ) + 1
constexpr int measuredFields = 0
#include "../code/compat/jk2_wire_layout.def"
	;
#undef FNTECH3_JK2_WIRE_RECORD
#undef FNTECH3_JK2_WIRE_FIELD

#define FNTECH3_JK2_WIRE_RECORD( record, size ) + 1
#define FNTECH3_JK2_WIRE_FIELD( record, member, offset, size, kind ) + 0
constexpr int measuredRecords = 0
#include "../code/compat/jk2_wire_layout.def"
	;
#undef FNTECH3_JK2_WIRE_RECORD
#undef FNTECH3_JK2_WIRE_FIELD

static_assert( measuredRecords == 3, "every measured record is covered" );
static_assert( measuredFields == 65, "every member of every record is covered" );

/* The engine's own half follows the state, and the two together are the
 * record the module publishes. */
static_assert( sizeof( entityState_t ) == FNTECH3_JK2_ENTITY_STATE_BYTES );
static_assert( sizeof( usercmd_t ) == 28 );
/* Quake III's input frame is 24 bytes. This title's is not it. */
static_assert( sizeof( usercmd_t ) != 24 );
static_assert( FNTECH3_JK2_ENTITY_SHARED_OFFSET == FNTECH3_JK2_ENTITY_STATE_BYTES );
static_assert( FNTECH3_JK2_R_OWNERNUM == 396 ); // retail jk2mp.exe 0x45093d/0x4509cc
// Its snapshot walk reads singleClient at entity +0x134 (0x44fd93) and the
// broadcast mask's words at +0x190 (0x44fe1a), the last the engine reads.
static_assert( FNTECH3_JK2_R_SINGLECLIENT == 0x134 );
static_assert( FNTECH3_JK2_R_BROADCASTCLIENTS == 0x190 );
static_assert( FNTECH3_JK2_SHARED_ENTITY_BYTES == FNTECH3_JK2_R_BROADCASTCLIENTS + 8 );
static_assert( FNTECH3_JK2_SHARED_ENTITY_BYTES
	== FNTECH3_JK2_ENTITY_STATE_BYTES + FNTECH3_JK2_ENTITY_SHARED_BYTES );

/* The offsets the engine reads directly are the ones the table gives. */
static_assert( offsetof( entityState_t, number ) == FNTECH3_JK2_S_NUMBER );
static_assert( offsetof( entityState_t, eType ) == FNTECH3_JK2_S_ETYPE );
static_assert( offsetof( entityState_t, eFlags ) == FNTECH3_JK2_S_EFLAGS );
static_assert( offsetof( entityState_t, origin ) == FNTECH3_JK2_S_ORIGIN );
static_assert( offsetof( entityState_t, origin2 ) == FNTECH3_JK2_S_ORIGIN2 );
static_assert( offsetof( entityState_t, solid ) == FNTECH3_JK2_S_SOLID );
static_assert( offsetof( entityState_t, clientNum ) == FNTECH3_JK2_S_CLIENTNUM );
static_assert( offsetof( entityState_t, generic1 ) == FNTECH3_JK2_S_GENERIC1 );

/* This title is not Jedi Academy, and its records are not Academy's: Outcast's
 * entity state is 296 bytes where Academy's is 532. Nothing may be shared
 * between the two by assuming they are the same lineage. */
static_assert( FNTECH3_JK2_ENTITY_STATE_BYTES != 532 );

/* The player state, held to what was measured rather than to what a sibling
 * title does. Its size and the offsets of the members the engine itself reads
 * come from the delta table at 0xf5e20; the four arrays come from the engine's
 * own delta reader, which prints their names as it walks them and reads the
 * powerups as long words because they are times; and the ping comes from the
 * one place the engine writes it. */
static_assert( sizeof( playerState_t ) == FNTECH3_JK2_PLAYER_STATE_BYTES );
static_assert( sizeof( playerState_t ) == 1368 );
static_assert( offsetof( playerState_t, stats ) == 216 );
static_assert( offsetof( playerState_t, persistant ) == 280 );
static_assert( offsetof( playerState_t, powerups ) == 344 );
static_assert( offsetof( playerState_t, ammo ) == 408 );
static_assert( sizeof( ( (playerState_t *)0 )->stats ) == 64 );
static_assert( sizeof( ( (playerState_t *)0 )->persistant ) == 64 );
static_assert( sizeof( ( (playerState_t *)0 )->powerups ) == 64 );
static_assert( sizeof( ( (playerState_t *)0 )->ammo ) == 64 );
static_assert( offsetof( playerState_t, ping ) == FNTECH3_JK2_PS_PING );
static_assert( offsetof( playerState_t, ping ) == 484 );
static_assert( offsetof( playerState_t, commandTime ) == 0 );
static_assert( offsetof( playerState_t, origin ) == 20 );
static_assert( offsetof( playerState_t, clientNum ) == FNTECH3_JK2_PS_CLIENTNUM );
static_assert( offsetof( playerState_t, generic1 ) == 472 );

/* Jedi Academy's player state is 1,552 bytes and carries nineteen ammunition
 * slots. This title's is 1,368 and carries sixteen. Neither record may be
 * read through the other's adapter, and this says so where it is used. */
static_assert( sizeof( playerState_t ) != 1552 );

/* The input frame, read out of the client that builds one: the time at 0, the
 * buttons OR-ed together at 16, four bytes from four globals at 20 to 23 with
 * the fourth cleared right after, and the three moves at 24 to 26. */
static_assert( offsetof( usercmd_t, serverTime ) == 0 );
static_assert( offsetof( usercmd_t, angles ) == 4 );
static_assert( offsetof( usercmd_t, buttons ) == 16 );
static_assert( offsetof( usercmd_t, weapon ) == 20 );
static_assert( offsetof( usercmd_t, forcesel ) == 21 );
static_assert( offsetof( usercmd_t, invensel ) == 22 );
static_assert( offsetof( usercmd_t, generic_cmd ) == 23 );
static_assert( offsetof( usercmd_t, forwardmove ) == 24 );
static_assert( offsetof( usercmd_t, rightmove ) == 25 );
static_assert( offsetof( usercmd_t, upmove ) == 26 );

int main()
{
	std::printf( "Jedi Outcast record layouts hold: %d records, %d members\n",
		measuredRecords, measuredFields );
	return 0;
}
