/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_CLIENT_ADAPTER_HPP
#define FNTECH3_JK2_CLIENT_ADAPTER_HPP

#include "jk2_client_abi.h"
#include "jk2_state_adapter.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3::jk2::client {

template<class Internal>
bool GameState( const Internal &source, fntech3_jk2_mp_gameState_t &destination ) noexcept {
	static_assert( sizeof( source.stringOffsets ) / sizeof( source.stringOffsets[0] ) >= FNTECH3_JK2_MP_CONFIGSTRINGS );
	static_assert( sizeof( source.stringData ) >= FNTECH3_JK2_MP_GAMESTATE_CHARS );
	if ( source.dataCount < 1 || source.dataCount > FNTECH3_JK2_MP_GAMESTATE_CHARS || source.stringData[0] ) return false;
	fntech3_jk2_mp_gameState_t result{};
	for ( std::size_t i = 0; i < FNTECH3_JK2_MP_CONFIGSTRINGS; ++i ) {
		const auto offset = source.stringOffsets[i];
		if ( offset < 0 || offset >= source.dataCount ||
			!std::memchr( source.stringData + offset, 0, source.dataCount - offset ) ) return false;
		result.stringOffsets[i] = offset;
	}
	/* A configstring beyond this title's own bound cannot silently disappear. */
	for ( std::size_t i = FNTECH3_JK2_MP_CONFIGSTRINGS; i < sizeof( source.stringOffsets ) / sizeof( source.stringOffsets[0] ); ++i ) {
		if ( source.stringOffsets[i] ) return false;
	}
	std::memcpy( result.stringData, source.stringData, source.dataCount );
	result.dataCount = source.dataCount;
	destination = result;
	return true;
}

template<class Snapshot, class Entity>
bool SnapshotTransfer( const Snapshot &source, const Entity *parsed, std::size_t ringSize,
	fntech3_jk2_mp_snapshot_t &destination ) noexcept {
	if ( !parsed || !ringSize || ( ringSize & ( ringSize - 1 ) ) || source.numEntities < 0 ||
		static_cast<std::size_t>( source.numEntities ) > ringSize ) return false;
	fntech3_jk2_mp_snapshot_t result{};
	result.snapFlags = source.snapFlags;
	result.ping = source.ping;
	result.serverTime = source.serverTime;
	static_assert( sizeof( source.areamask ) == sizeof( result.areamask ) );
	std::memcpy( result.areamask, source.areamask, sizeof( result.areamask ) );
	if ( !state::FromInternal( source.ps, result.ps ) ) return false;
	result.numEntities = std::min( source.numEntities, int( FNTECH3_JK2_MP_SNAPSHOT_ENTITIES ) );
	for ( int i = 0; i < result.numEntities; ++i ) {
		const auto index = ( static_cast<std::uint32_t>( source.parseEntitiesNum ) + i ) & ( ringSize - 1 );
		if ( !state::FromInternal( parsed[index], result.entities[i] ) ) return false;
	}
	result.serverCommandSequence = source.serverCommandNum;
	destination = result;
	return true;
}

} // namespace fntech3::jk2::client
#endif
