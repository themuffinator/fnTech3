/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Heavy Metal: F.A.K.K.2's module tables and records against Alice's: the
 * slot maps between the two titles' import tables, and every record crossing
 * at the module boundary (code/compat/fakk2_convert.hpp). The frozen sizes are
 * static_asserts in code/compat/fakk2_game_abi.h; this checks that a crossing
 * loses and invents nothing. No retail file is read here. See
 * docs/fntech3/2026-09-24-fakk2-contracts.md. */

#include "../code/compat/fakk2_convert.hpp"
#include "../code/compat/fakk2_module_abi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace convert = fntech3::fakk2::convert;
namespace module = fntech3::fakk2::module;

namespace {

/* A record filled with a byte pattern no member would hold by accident. */
template<typename T>
void Pattern( T &record, unsigned seed ) {
	unsigned char *bytes = reinterpret_cast<unsigned char *>( &record );
	for ( std::size_t i = 0; i < sizeof( T ); ++i ) {
		bytes[i] = static_cast<unsigned char>( ( seed + i * 37u ) & 0xffu );
	}
}

template<typename T>
bool Same( const T &a, const T &b ) {
	return std::memcmp( &a, &b, sizeof( T ) ) == 0;
}

bool AllZero( const void *data, std::size_t bytes ) {
	const unsigned char *p = static_cast<const unsigned char *>( data );
	for ( std::size_t i = 0; i < bytes; ++i ) {
		if ( p[i] ) {
			return false;
		}
	}
	return true;
}

void SlotMaps() {
	// The game's first hundred slots are Alice's, its last five one lower.
	for ( int slot = 0; slot < 100; ++slot ) {
		CHECK( module::AliceGameSlot( slot ) == slot );
	}
	for ( int slot = 100; slot < FNTECH3_FAKK2_GAME_IMPORT_SLOTS; ++slot ) {
		CHECK( module::AliceGameSlot( slot ) == slot + 1 );
	}
	CHECK( module::AliceGameSlot( -1 ) == -1 );
	CHECK( module::AliceGameSlot( FNTECH3_FAKK2_GAME_IMPORT_SLOTS ) == -1 );
	// Every mapped game slot is inside Alice's table and none is Alice's hole.
	for ( int slot = 0; slot < FNTECH3_FAKK2_GAME_IMPORT_SLOTS; ++slot ) {
		const int alice = module::AliceGameSlot( slot );
		CHECK( alice >= 0 && alice < FNTECH3_ALICE_GAME_IMPORT_SLOTS && alice != 100 );
	}
	// The client game's 124 are Alice's first 124.
	for ( int slot = 0; slot < FNTECH3_FAKK2_CGAME_IMPORT_SLOTS; ++slot ) {
		CHECK( module::AliceCgameSlot( slot ) == slot );
	}
	CHECK( module::AliceCgameSlot( FNTECH3_FAKK2_CGAME_IMPORT_SLOTS ) == -1 );
}

void EntityStates() {
	fntech3_fakk2_entityState_t fakk2;
	Pattern( fakk2, 11 );
	fntech3_alice_entityState_t alice;
	convert::EntityToAlice( fakk2, alice );

	// The shared prefix, and each controller's three arrays in Alice's record.
	CHECK( std::memcmp( &alice, &fakk2, convert::kSharedEntityPrefix ) == 0 );
	for ( int i = 0; i < FNTECH3_FAKK2_BONE_CONTROLLERS; ++i ) {
		CHECK( alice.bonemod[i].tag == fakk2.bone_tag[i] );
		CHECK( std::memcmp( alice.bonemod[i].angles, fakk2.bone_angles[i], sizeof( fakk2.bone_angles[i] ) ) == 0 );
		CHECK( std::memcmp( alice.bonemod[i].unnamed, fakk2.bone_quat[i], sizeof( fakk2.bone_quat[i] ) ) == 0 );
	}
	CHECK( alice.renderfx == fakk2.renderfx );
	CHECK( alice.ownerNum == fakk2.bindparent );
	// Alice's own second render-flags word is not F.A.K.K.2's to set.
	CHECK( alice.renderfx2 == 0 );

	// And back: nothing F.A.K.K.2 has is lost.
	fntech3_fakk2_entityState_t back;
	Pattern( back, 99 );
	convert::EntityFromAlice( alice, back );
	CHECK( Same( back, fakk2 ) );
}

void RenderEntities() {
	fntech3_fakk2_refEntity_t fakk2;
	Pattern( fakk2, 23 );
	fntech3_alice_refEntity_t alice;
	convert::RefEntityToAlice( fakk2, alice );

	// Alice's inserted word after the head is zero, and the run after it moved.
	const unsigned char *raw = reinterpret_cast<const unsigned char *>( &alice );
	CHECK( std::memcmp( &alice, &fakk2, convert::kRefHead ) == 0 );
	CHECK( AllZero( raw + convert::kRefHead, 4 ) );
	CHECK( std::memcmp( raw + convert::kRefHead + 4,
		reinterpret_cast<const unsigned char *>( &fakk2 ) + convert::kRefHead, convert::kRefRun ) == 0 );
	for ( int i = 0; i < FNTECH3_FAKK2_BONE_CONTROLLERS; ++i ) {
		CHECK( alice.bonectrl[i].tag == fakk2.bone_tag[i] );
		CHECK( std::memcmp( alice.bonectrl[i].quat, fakk2.bone_quat[i], sizeof( fakk2.bone_quat[i] ) ) == 0 );
		// F.A.K.K.2's controllers carry no scale; one multiplies in as nothing.
		CHECK( alice.bonectrl[i].scale == 1.0f );
	}
	CHECK( alice.radius == fakk2.radius && alice.rotation == fakk2.rotation );

	fntech3_fakk2_refEntity_t back;
	Pattern( back, 5 );
	convert::RefEntityFromAlice( alice, back );
	CHECK( Same( back, fakk2 ) );
}

void Controllers() {
	fntech3_alice_bonectrl_t out[FNTECH3_ALICE_BONE_CONTROLLERS];
	convert::ControllersToAlice( nullptr, nullptr, out );
	for ( const fntech3_alice_bonectrl_t &c : out ) {
		// untagged, which the pose skips
		CHECK( c.tag == -1 && c.scale == 1.0f );
		CHECK( AllZero( c.quat, sizeof( c.quat ) ) );
	}
	const int32_t tags[FNTECH3_FAKK2_BONE_CONTROLLERS] = { 3, -1, 7, 0, 12 };
	float quats[FNTECH3_FAKK2_BONE_CONTROLLERS][4] = {};
	quats[2][3] = 1.0f;
	convert::ControllersToAlice( tags, quats, out );
	CHECK( out[0].tag == 3 && out[1].tag == -1 && out[4].tag == 12 && out[2].quat[3] == 1.0f );
}

void Views() {
	fntech3_fakk2_refdef_t fakk2;
	Pattern( fakk2, 41 );
	fntech3_alice_refdef_t alice;
	convert::RefdefToAlice( fakk2, alice );
	CHECK( std::memcmp( &alice, &fakk2, sizeof( fakk2 ) ) == 0 );
	// Alice's appended members are hers alone.
	CHECK( AllZero( reinterpret_cast<const unsigned char *>( &alice ) + sizeof( fakk2 ),
		sizeof( alice ) - sizeof( fakk2 ) ) );
	CHECK( std::memcmp( alice.sky_origin, fakk2.sky_origin, sizeof( fakk2.sky_origin ) ) == 0 );
	CHECK( alice.sky_portal == fakk2.sky_portal && alice.sky_alpha == fakk2.sky_alpha );
}

void CommandLists() {
	/* The argument slots are the module's 32-bit string pointers, copied as
	 * they are; distinct stand-ins show each lands where it should. */
	auto word = []( int command, int argument ) {
		return static_cast<std::uint32_t>( 0x10000000u + command * 0x100u + argument );
	};
	std::vector<unsigned char> storage( sizeof( fntech3_alice_tiki_cmd_t ) );
	fntech3_alice_tiki_cmd_t &alice = *reinterpret_cast<fntech3_alice_tiki_cmd_t *>( storage.data() );
	fntech3_fakk2_tiki_cmd_t fakk2;

	// Within F.A.K.K.2's bounds a list crosses whole.
	std::memset( &alice, 0, sizeof( alice ) );
	alice.num_cmds = 2;
	alice.cmds[0].num_args = 3;
	alice.cmds[1].num_args = FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS;
	for ( int a = 0; a < FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS; ++a ) {
		alice.cmds[0].args[a] = word( 0, a );
		alice.cmds[1].args[a] = word( 1, a );
	}
	std::memset( &fakk2, 0, sizeof( fakk2 ) );
	CHECK( convert::CommandsFromAlice( alice, fakk2 ) );
	CHECK( fakk2.num_cmds == 2 && fakk2.cmds[0].num_args == 3 );
	CHECK( fakk2.cmds[1].num_args == FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS );
	CHECK( fakk2.cmds[0].args[2] == word( 0, 2 ) && fakk2.cmds[1].args[9] == word( 1, 9 ) );

	// An eleventh argument, or a 129th command, is cut and reported.
	alice.cmds[1].num_args = FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS + 1;
	alice.cmds[1].args[FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS] = word( 1, 10 );
	CHECK( !convert::CommandsFromAlice( alice, fakk2 ) );
	CHECK( fakk2.cmds[1].num_args == FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS );
	alice.cmds[1].num_args = 1;
	alice.num_cmds = FNTECH3_FAKK2_MAX_TIKI_CMDS + 1;
	CHECK( !convert::CommandsFromAlice( alice, fakk2 ) );
	CHECK( fakk2.num_cmds == FNTECH3_FAKK2_MAX_TIKI_CMDS );
	// A negative count is none.
	alice.num_cmds = -4;
	CHECK( convert::CommandsFromAlice( alice, fakk2 ) && fakk2.num_cmds == 0 );
}

void Fragments() {
	fntech3_alice_markFragment_t alice[3];
	Pattern( alice, 3 );
	for ( int i = 0; i < 3; ++i ) {
		alice[i].firstPoint = i * 4;
		alice[i].numPoints = 3 + i;
	}
	fntech3_fakk2_markFragment_t fakk2[3];
	convert::FragmentsFromAlice( alice, 3, fakk2 );
	for ( int i = 0; i < 3; ++i ) {
		CHECK( fakk2[i].firstPoint == i * 4 && fakk2[i].numPoints == 3 + i );
	}
}

} // namespace

int main() {
	SlotMaps();
	EntityStates();
	RenderEntities();
	Controllers();
	Views();
	CommandLists();
	Fragments();
	std::puts( "fakk2 module ABI tests passed" );
	return 0;
}
