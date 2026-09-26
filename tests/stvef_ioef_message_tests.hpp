/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Included by message_codec_tests.cpp. These golden bytes come from the
 * pinned Lilium encoder, independently of the production codec. */
#include "../code/compat/stvef_state_adapter.hpp"

namespace stvef_ioef_tests {

using IOEF_Entity = fntech3_stvef_mp_entityState_t;
using IOEF_Player = fntech3_stvef_mp_playerState_t;
using IOEF_Command = fntech3_stvef_mp_usercmd_t;
#include "fixtures/stvef_ioef_fixture.h"

struct Vector { int bits; const char *hex; };
#define IOEF_VECTOR(name, bits, hex) inline constexpr Vector name{ bits, hex };
#include "fixtures/stvef_ioef_vectors.inc"
#undef IOEF_VECTOR

inline constexpr auto Wire = NETCHAN_WIRE_IOEF;
inline constexpr int Key = 0x12345678;

inline std::array<byte, 2048> Bytes( Vector vector ) {
	std::array<byte, 2048> result{};
	auto digit = []( char c ) { return c <= '9' ? c - '0' : c - 'a' + 10; };
	for ( std::size_t i = 0; vector.hex[2 * i]; ++i )
		result[i] = static_cast<byte>( digit( vector.hex[2 * i] ) * 16 + digit( vector.hex[2 * i + 1] ) );
	return result;
}

template<class Record, class Write, class Read>
int CheckRecord( Vector vector, const Record *baseline, const Record *sent,
	const Record &expected, Write write, Read read ) {
	const int length = static_cast<int>( std::strlen( vector.hex ) / 2 );
	const auto bytes = Bytes( vector );
	std::array<byte, 2048> encoded{};
	msg_t msg{};
	MSG_Init( &msg, encoded.data(), static_cast<int>( encoded.size() ) );
	CHECK( write( &msg, baseline, sent ) );
	CHECK( msg.bit == vector.bits && msg.cursize == length && !msg.overflowed );
	CHECK( std::memcmp( encoded.data(), bytes.data(), length ) == 0 );
	// Records also begin between Huffman codewords. Exercise every bit offset,
	// every byte truncation and in-place decode without using our encoder to
	// produce the input stream.
	for ( int offset = 0; offset < 8; ++offset ) {
		std::array<byte, 2048> shifted{};
		for ( int bit = 0; bit < vector.bits; ++bit )
			shifted[( bit + offset ) / 8] |= static_cast<byte>(
				( ( bytes[bit / 8] >> ( bit % 8 ) ) & 1 ) << ( ( bit + offset ) % 8 ) );
		const int completeBytes = ( offset + vector.bits ) / 8 + 1;
		for ( int size = 0; size <= completeBytes; ++size ) {
			for ( bool inPlace : { false, true } ) {
				struct { unsigned before = 0xdeadbeef; Record value{}; unsigned after = 0x12345678; } output;
				if ( baseline ) output.value = *baseline;
				if ( !inPlace ) std::memset( &output.value, 0x5a, sizeof( output.value ) );
				const Record untouched = output.value;
				MSG_Init( &msg, shifted.data(), static_cast<int>( shifted.size() ) );
				msg.cursize = size;
				MSG_BeginReading( &msg ); msg.bit = offset; msg.readcount = offset ? 1 : 0;
				const bool complete = size == completeBytes;
				CHECK( !!read( &msg, inPlace ? &output.value : baseline, &output.value ) == complete );
				CHECK( std::memcmp( &output.value, complete ? &expected : &untouched, sizeof( Record ) ) == 0 );
				CHECK( output.before == 0xdeadbeef && output.after == 0x12345678 );
				if ( complete ) CHECK( msg.bit == offset + vector.bits );
				else CHECK( msg.readcount > msg.cursize );
			}
		}
	}
	// An exhausted output buffer must never overwrite its trailing guard.
	for ( int size = 0; size < length; ++size ) {
		encoded.fill( 0xa5 );
		MSG_Init( &msg, encoded.data() + 1, size );
		CHECK( !write( &msg, baseline, sent ) && msg.overflowed );
		CHECK( encoded[0] == 0xa5 && encoded[size + 1] == 0xa5 );
	}
	return 0;
}

inline int TestGoldenRecords() {
	IOEF_Entity externalEntity{};
	IOEF_FillEntity( &externalEntity );
	entityState_t entity{}, entityBase{}, entityZero{}, removal{};
	CHECK( fntech3::stvef::state::ToInternal( externalEntity, entity ) );
	entityZero.number = 7; removal.number = 1023;
	auto writeEntity = []( msg_t *m, const auto *from, const auto *to ) {
		return MSG_WriteDeltaEntityForWireProfile( m, from, to, qtrue, Wire );
	};
	auto readEntity = []( msg_t *m, const auto *from, auto *to ) {
		const int number = MSG_ReadEntitynum( m );
		return MSG_ReadDeltaEntityForWireProfile( m, from, to, number, Wire );
	};
	if ( int error = CheckRecord( entity_all, &entityBase, &entity, entity, writeEntity, readEntity ) ) return error;
	if ( int error = CheckRecord( entity_zero, &entity, &entityZero, entityZero, writeEntity, readEntity ) ) return error;
	if ( int error = CheckRecord( entity_remove, &entity, static_cast<const entityState_t *>( nullptr ), removal, writeEntity, readEntity ) ) return error;
	if ( int error = CheckRecord( entity_same, &entity, &entity, entity, writeEntity, readEntity ) ) return error;

	IOEF_Player externalPlayer{};
	IOEF_FillPlayer( &externalPlayer );
	playerState_t player{}, playerBase{};
	CHECK( fntech3::stvef::state::ToInternal( externalPlayer, player ) );
	auto writePlayer = []( msg_t *m, const auto *from, const auto *to ) {
		return MSG_WriteDeltaPlayerstateForWireProfile( m, from, to, Wire );
	};
	auto readPlayer = []( msg_t *m, const auto *from, auto *to ) {
		return MSG_ReadDeltaPlayerstateForWireProfile( m, from, to, Wire );
	};
	if ( int error = CheckRecord( player_all, &playerBase, &player, player, writePlayer, readPlayer ) ) return error;
	if ( int error = CheckRecord( player_same, &player, &player, player, writePlayer, readPlayer ) ) return error;

	IOEF_Command externalCommand{};
	IOEF_FillCommand( &externalCommand );
	usercmd_t command{}, commandBase{}, decoded{};
	CHECK( fntech3::stvef::state::ToInternal( externalCommand, command ) );
	commandBase.serverTime = 100000;
	decoded = command; decoded.forwardmove = -127;
	auto writeCommand = []( msg_t *m, const auto *from, const auto *to ) {
		return MSG_WriteDeltaUsercmdKeyForWireProfile( m, Key, from, to, Wire );
	};
	auto readCommand = []( msg_t *m, const auto *from, auto *to ) {
		return MSG_ReadDeltaUsercmdKeyForWireProfile( m, Key, from, to, Wire );
	};
	if ( int error = CheckRecord( command_all, &commandBase, &command, decoded, writeCommand, readCommand ) ) return error;
	commandBase = command; command.serverTime += 300; decoded = command; decoded.forwardmove = -127;
	if ( int error = CheckRecord( command_same, &commandBase, &command, decoded, writeCommand, readCommand ) ) return error;
	return 0;
}

inline int TestStringsAndMalformedRecords() {
	std::array<byte, 2048> bytes{};
	msg_t msg{};
	MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
	MSG_WriteBits( &msg, 5, 3 );
	MSG_WriteStringForWireProfile( &msg, "h\xe9%", Wire );
	CHECK( msg.bit == string.bits && msg.cursize == 4 && std::memcmp( bytes.data(), Bytes( string ).data(), 4 ) == 0 );
	MSG_BeginReading( &msg );
	msg.bit = 3; msg.readcount = 1;
	CHECK( std::strcmp( MSG_ReadStringForWireProfile( &msg, Wire ), "h\xe9." ) == 0 );
	CHECK( MSG_HashKeyForWireProfile( Wire, "say 100% ready", 32 ) == 140893 );
	CHECK( MSG_HashKeyForWireProfile( Wire, "\x80\xff%", 32 ) == 16544 );

	for ( int count : { 51, 255 } ) {
		MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
		MSG_WriteBits( &msg, 0, 1 ); MSG_WriteBits( &msg, 1, 1 ); MSG_WriteByte( &msg, count );
		MSG_BeginReading( &msg );
		entityState_t entity{}; entity.number = 7; entity.time = 999;
		const auto unchanged = entity;
		CHECK( !MSG_ReadDeltaEntityForWireProfile( &msg, &entity, &entity, 7, Wire ) );
		CHECK( msg.readcount > msg.cursize && std::memcmp( &entity, &unchanged, sizeof( entity ) ) == 0 );
	}
	for ( int count : { 49, 255 } ) {
		MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
		MSG_WriteByte( &msg, count ); MSG_BeginReading( &msg );
		playerState_t player{}; player.commandTime = 999;
		const auto unchanged = player;
		CHECK( !MSG_ReadDeltaPlayerstateForWireProfile( &msg, &player, &player, Wire ) );
		CHECK( msg.readcount > msg.cursize && std::memcmp( &player, &unchanged, sizeof( player ) ) == 0 );
	}
	return 0;
}

} // namespace stvef_ioef_tests

inline int TestIOEFMessageCodecs() {
	if ( const int error = stvef_ioef_tests::TestGoldenRecords() ) return error;
	return stvef_ioef_tests::TestStringsAndMalformedRecords();
}
