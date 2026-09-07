/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

extern "C" {
#include "../code/compat/game_abi_adapter.h"
}

#include <cstdint>
#include <cstring>

#define CHECK( expression ) do { if ( !( expression ) ) return __LINE__; } while ( false )

static int TestCvarFlagTranslation() {
	CHECK( FnTech3_Q3CvarFlagsToInternal( FNTECH3_Q3_CVAR_SERVER_CREATED ) ==
		static_cast<std::uint32_t>( CVAR_SERVER_CREATED ) );
	CHECK( FnTech3_Q3CvarFlagsToInternal( FNTECH3_Q3_CVAR_PROTECTED ) ==
		static_cast<std::uint32_t>( CVAR_PROTECTED ) );
	CHECK( FnTech3_Q3CvarFlagsToInternal( FNTECH3_Q3_CVAR_PRIVATE ) ==
		static_cast<std::uint32_t>( CVAR_PRIVATE ) );
	CHECK( FnTech3_Q3CvarFlagsToInternal( FNTECH3_Q3_CVAR_ARCHIVE_ND ) ==
		static_cast<std::uint32_t>( CVAR_ARCHIVE_ND ) );
	CHECK( FnTech3_Q3CvarFlagsToInternal( UINT32_C( 0x20000000 ) ) == 0 );
	return 0;
}

static int TestUsercmdTranslation() {
	fntech3_q3_usercmd_t q3{};
	q3.serverTime = 1234;
	q3.angles[0] = 10;
	q3.angles[1] = 20;
	q3.angles[2] = 30;
	q3.buttons = FNTECH3_Q3_BUTTON_ATTACK | FNTECH3_Q3_BUTTON_WALKING;
	q3.weapon = 7;
	q3.forwardmove = -40;
	q3.rightmove = 50;
	q3.upmove = -60;

	usercmd_t internal;
	std::memset( &internal, 0x7f, sizeof( internal ) );
	FnTech3_InternalUsercmdFromQ3( &internal, &q3 );
	CHECK( internal.serverTime == q3.serverTime );
	CHECK( internal.weapon == q3.weapon );
	CHECK( internal.weaponPrimary == 0 );
	CHECK( internal.fov == 0 );
	CHECK( internal.forwardmove == q3.forwardmove );

	fntech3_q3_usercmd_t roundTrip{};
	FnTech3_Q3UsercmdFromInternal( &roundTrip, &internal );
	CHECK( std::memcmp( &q3, &roundTrip, sizeof( q3 ) ) == 0 );
	return 0;
}

static int TestEntityTranslation() {
	fntech3_q3_entityState_t q3{};
	q3.number = 17;
	q3.eType = 3;
	q3.pos.trType = FNTECH3_Q3_TR_GRAVITY;
	q3.pos.trTime = 900;
	q3.pos.trBase[2] = 72.5f;
	q3.event = 511;
	q3.weapon = 8;
	q3.generic1 = 42;

	entityState_t internal;
	std::memset( &internal, 0x7f, sizeof( internal ) );
	FnTech3_InternalEntityStateFromQ3( &internal, &q3 );
	CHECK( internal.number == q3.number );
	CHECK( internal.pos.gravity == 0.0f );
	CHECK( internal.apos.gravity == 0.0f );
	CHECK( internal.event == q3.event );
	CHECK( internal.health == 0 );
	CHECK( internal.armor == 0 );
	CHECK( internal.location == 0 );
	CHECK( internal.jumpTime == 0 );
	CHECK( internal.doubleJumped == 0 );

	fntech3_q3_entityState_t roundTrip{};
	FnTech3_Q3EntityStateFromInternal( &roundTrip, &internal );
	CHECK( std::memcmp( &q3, &roundTrip, sizeof( q3 ) ) == 0 );
	return 0;
}

static int TestPlayerTranslation() {
	fntech3_q3_playerState_t q3{};
	q3.commandTime = 2048;
	q3.pm_flags = 0x1234;
	q3.origin[0] = 1.25f;
	q3.externalEvent = 9;
	q3.externalEventTime = 777;
	q3.clientNum = 11;
	q3.weapon = 5;
	q3.stats[3] = 100;
	q3.powerups[7] = 54321;
	q3.entityEventSequence = 19;

	playerState_t internal;
	std::memset( &internal, 0x7f, sizeof( internal ) );
	FnTech3_InternalPlayerStateFromQ3( &internal, &q3 );
	CHECK( internal.commandTime == q3.commandTime );
	CHECK( internal.externalEventTime == q3.externalEventTime );
	CHECK( internal.weaponPrimary == 0 );
	CHECK( internal.fov == 0 );
	CHECK( internal.location == 0 );
	CHECK( internal.jumpTime == 0 );
	CHECK( internal.doubleJumped == 0 );
	CHECK( internal.crouchTime == 0 );
	CHECK( internal.crouchSlideTime == 0 );
	CHECK( internal.forwardmove == 0 );
	CHECK( internal.rightmove == 0 );
	CHECK( internal.upmove == 0 );
	CHECK( internal.armorTier == 0 );

	fntech3_q3_playerState_t roundTrip{};
	FnTech3_Q3PlayerStateFromInternal( &roundTrip, &internal );
	CHECK( std::memcmp( &q3, &roundTrip, sizeof( q3 ) ) == 0 );

	internal.pm_flags = 0x7f1234;
	FnTech3_Q3PlayerStateFromInternal( &roundTrip, &internal );
	CHECK( roundTrip.pm_flags == 0x1234 );
	return 0;
}

// The current internal layouts match the frozen QL prefix. Distinct finite
// words make this a byte-for-byte baseline check of every field and array slot,
// while guard bytes detect writes beyond the native player-state envelope.
template<class T> static void FillQLBaseline( T &value ) {
	static_assert( sizeof( T ) % 4 == 0 );
	for ( std::size_t i = 0; i < sizeof( T ) / 4; ++i ) {
		const std::uint32_t word = 0x3f000000u + static_cast<std::uint32_t>( i );
		std::memcpy( reinterpret_cast<unsigned char *>( &value ) + i * 4, &word, 4 );
	}
}

static int TestQLSnapshotTranslation() {
	playerState_t player;
	FillQLBaseline( player );
	player.forwardmove = -127;
	player.rightmove = 127;
	player.upmove = -1;
	struct GuardedPlayer {
		unsigned char before[16];
		unsigned char envelope[FNTECH3_QL_RETAIL_SNAPSHOT_PLAYERSTATE_BYTES];
		unsigned char after[16];
	} guarded;
	std::memset( &guarded, 0xa5, sizeof( guarded ) );
	FnTech3_QLSnapshotPlayerStateFromInternal( guarded.envelope, &player );
	static_assert( sizeof( player ) == sizeof( fntech3_ql_playerState_t ) );
	CHECK( std::memcmp( guarded.envelope, &player, sizeof( player ) ) == 0 );
	for ( std::size_t i = sizeof( player ); i < sizeof( guarded.envelope ); ++i )
		CHECK( guarded.envelope[i] == 0 );
	for ( unsigned char byte : guarded.before ) CHECK( byte == 0xa5 );
	for ( unsigned char byte : guarded.after ) CHECK( byte == 0xa5 );

	entityState_t entity;
	FillQLBaseline( entity );
	entity.pos.trType = TR_QL_ACCEL;
	entity.apos.trType = TR_GRAVITY;
	struct GuardedEntity {
		std::uint32_t before;
		fntech3_ql_entityState_t value;
		std::uint32_t after;
	} output;
	std::memset( &output, 0xa5, sizeof( output ) );
	FnTech3_QLEntityStateFromInternal( &output.value, &entity );
	static_assert( sizeof( entity ) == sizeof( output.value ) );
	CHECK( std::memcmp( &output.value, &entity, sizeof( entity ) ) == 0 );
	CHECK( output.before == 0xa5a5a5a5u && output.after == 0xa5a5a5a5u );
	return 0;
}

int main() {
	if ( const int result = TestQLSnapshotTranslation() ) return result;
	if ( const int result = TestCvarFlagTranslation() ) return result;
	if ( const int result = TestUsercmdTranslation() ) return result;
	if ( const int result = TestEntityTranslation() ) return result;
	if ( const int result = TestPlayerTranslation() ) return result;
	return 0;
}
