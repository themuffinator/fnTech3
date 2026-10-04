/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jk2_client_adapter.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>

using namespace fntech3::jk2;
static_assert( sizeof( fntech3_jk2_mp_gameState_t ) == 21604 );
static_assert( offsetof( fntech3_jk2_mp_gameState_t, stringData ) == 5600 );
static_assert( offsetof( fntech3_jk2_mp_gameState_t, dataCount ) == 21600 );
static_assert( sizeof( fntech3_jk2_mp_glconfig_t ) == 11332 );
static_assert( offsetof( fntech3_jk2_mp_glconfig_t, vidWidth ) == 11304 );
static_assert( sizeof( fntech3_jk2_mp_uiClientState_t ) == 3084 );
static_assert( offsetof( fntech3_jk2_mp_uiClientState_t, servername ) == 12 );
static_assert( offsetof( fntech3_jk2_mp_uiClientState_t, updateInfoString ) == 1036 );
static_assert( offsetof( fntech3_jk2_mp_uiClientState_t, messageString ) == 2060 );
static_assert( sizeof( fntech3_jk2_mp_snapshot_t ) == 77200 );
static_assert( offsetof( fntech3_jk2_mp_snapshot_t, ps ) == 44 );
static_assert( offsetof( fntech3_jk2_mp_snapshot_t, numEntities ) == 1412 );
static_assert( offsetof( fntech3_jk2_mp_snapshot_t, entities ) == 1416 );
static_assert( offsetof( fntech3_jk2_mp_snapshot_t, numServerCommands ) == 77192 );
static_assert( offsetof( fntech3_jk2_mp_snapshot_t, serverCommandSequence ) == 77196 );

struct GameState {
	int stringOffsets[1500]{};
	char stringData[32000]{};
	int dataCount = 1;
};
struct Snapshot {
	int snapFlags = 2, ping = 33, serverTime = 1200;
	unsigned char areamask[32]{};
	playerState_t ps{};
	int numEntities = 2, parseEntitiesNum = 7, serverCommandNum = 123;
};

int main() {
	GameState source;
	fntech3_jk2_mp_gameState_t gs{};
	std::memcpy( source.stringData + 1, "measured", 9 );
	source.dataCount = 10;
	source.stringOffsets[1399] = 1;
	assert( client::GameState( source, gs ) );
	assert( gs.stringOffsets[1399] == 1 && !std::strcmp( gs.stringData + 1, "measured" ) );
	auto original = gs;
	for ( int bad : { -1, source.dataCount } ) {
		source.stringOffsets[1399] = bad;
		assert( !client::GameState( source, gs ) );
		assert( !std::memcmp( &original, &gs, sizeof( gs ) ) );
	}
	source.stringOffsets[1399] = 1;
	source.stringOffsets[1400] = 1;
	assert( !client::GameState( source, gs ) );
	source.stringOffsets[1400] = 0;
	source.stringData[9] = 'x';
	assert( !client::GameState( source, gs ) );
	source.stringData[9] = 0;
	source.dataCount = 16001;
	assert( !client::GameState( source, gs ) );
	source.dataCount = 16000;
	source.stringOffsets[1399] = 15999;
	assert( client::GameState( source, gs ) );

	Snapshot snap;
	fntech3_jk2_playerState_t raw{};
	/* Opaque non-networked data must survive the client boundary as well. */
	std::memset( &raw, 0x37, sizeof( raw ) );
	assert( state::ToInternal( raw, snap.ps ) );
	std::array<entityState_t, 512> entities{};
	entities[511].number = 31;
	entities[0].number = 32;
	snap.parseEntitiesNum = 511;
	fntech3_jk2_mp_snapshot_t out{};
	assert( client::SnapshotTransfer( snap, entities.data(), entities.size(), out ) );
	assert( out.numEntities == 2 && out.entities[0].number == 31 && out.entities[1].number == 32 );
	assert( !std::memcmp( &out.ps, &raw, sizeof( raw ) ) );
	assert( out.serverCommandSequence == 123 && out.numServerCommands == 0 );
	auto saved = out;
	snap.numEntities = -1;
	assert( !client::SnapshotTransfer( snap, entities.data(), entities.size(), out ) );
	assert( !std::memcmp( &out, &saved, sizeof( out ) ) );
	snap.numEntities = 2;
	assert( !client::SnapshotTransfer( snap, entities.data(), 511, out ) );
	assert( !client::SnapshotTransfer( snap, static_cast<const entityState_t *>( nullptr ), 512, out ) );
	entities[0].pos.trType = static_cast<trType_t>( 999 );
	assert( !client::SnapshotTransfer( snap, entities.data(), entities.size(), out ) );
	assert( !std::memcmp( &out, &saved, sizeof( out ) ) );
	entities[0].pos.trType = TR_STATIONARY;
	snap.numEntities = 300;
	assert( client::SnapshotTransfer( snap, entities.data(), entities.size(), out ) );
	assert( out.numEntities == 256 );
	std::cout << "Jedi Outcast client records, lossless snapshots and malformed transfers hold\n";
}
