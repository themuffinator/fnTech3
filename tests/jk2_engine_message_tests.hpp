/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_ENGINE_MESSAGE_TESTS_HPP
#define FNTECH3_JK2_ENGINE_MESSAGE_TESTS_HPP
#include "../code/compat/jk2_message_codec.hpp"
#include "../code/compat/jk2_state_adapter.hpp"
#include "fixtures/jk2_wire_vectors.h"

namespace jk2_engine_tests {
template<class T> T Patched( const jk2_fixture::WordPatch *patches, unsigned count ) {
	T out{};
	for ( unsigned i = 0; i < count; ++i )
		fntech3::jk2::message::SetWord( out, patches[i].offset, patches[i].value );
	return out;
}

template<class External, class Internal, class WriteExternal, class WriteInternal, class ReadInternal>
int Fixture( const jk2_fixture::Vector &vector, WriteExternal writeExternal,
	WriteInternal writeInternal, ReadInternal readInternal, bool entity ) {
	const auto baseline = Patched<External>( vector.baseline, vector.baselineCount );
	const auto sent = Patched<External>( vector.sent, vector.sentCount );
	const auto expected = Patched<External>( vector.decoded, vector.decodedCount );
	Internal base{}, canonical{}, decoded{}, wanted{};
	CHECK( fntech3::jk2::state::ToInternal( baseline, base ) );
	CHECK( fntech3::jk2::state::ToInternal( sent, canonical ) );
	CHECK( fntech3::jk2::state::ToInternal( expected, wanted ) );
	for ( const bool raw : { false, true } ) for ( unsigned alignment = 0; alignment < 8; ++alignment ) {
		std::array<byte, 4096> externalBytes{}, canonicalBytes{};
		msg_t externalWriter{}, writer{};
		MSG_Init( &externalWriter, externalBytes.data(), static_cast<int>( externalBytes.size() ) );
		MSG_Init( &writer, canonicalBytes.data(), static_cast<int>( canonicalBytes.size() ) );
		if ( raw ) { MSG_RawBitstream( &externalWriter ); MSG_RawBitstream( &writer ); }
		if ( alignment ) { MSG_WriteBits( &externalWriter, 0x55, alignment ); MSG_WriteBits( &writer, 0x55, alignment ); }
		CHECK( writeExternal( &externalWriter, &baseline, &sent ) );
		CHECK( writeInternal( &writer, &base, &canonical ) );
		CHECK( !writer.overflowed && !externalWriter.overflowed );
		CHECK( externalWriter.bit == writer.bit && externalWriter.cursize == writer.cursize );
		CHECK( !std::memcmp( externalBytes.data(), canonicalBytes.data(), writer.cursize ) );
		if ( raw && !alignment ) {
			CHECK( writer.bit == static_cast<int>( vector.wireBits ) && writer.cursize == static_cast<int>( vector.wireBytes ) );
			CHECK( !std::memcmp( writer.data, vector.wire, vector.wireBytes ) );
		}
		msg_t reader{};
		MSG_Init( &reader, canonicalBytes.data(), static_cast<int>( canonicalBytes.size() ) );
		reader.cursize = writer.cursize;
		MSG_BeginReading( &reader );
		if ( raw ) MSG_RawBitstream( &reader );
		if ( alignment ) (void)MSG_ReadBitsExternal( &reader, alignment );
		if ( entity ) (void)MSG_ReadBitsExternal( &reader, 10 );
		CHECK( readInternal( &reader, &base, &decoded ) );
		CHECK( !std::memcmp( &decoded, &wanted, sizeof( decoded ) ) );
		CHECK( reader.bit == writer.bit );
		/* Huffman messages require the engine's final cursor byte, including
		 * the exactly-aligned trailing byte. Try every shorter byte count. */
		if ( raw ) continue;
		for ( int length = 0; length < writer.cursize; ++length ) for ( const bool alias : { false, true } ) {
			struct Guarded { std::uint32_t before; Internal state; std::uint32_t after; } guarded{};
			std::memset( &guarded, 0xa5, sizeof( guarded ) );
			if ( alias ) guarded.state = base;
			const auto unchanged = guarded;
			MSG_Init( &reader, canonicalBytes.data(), static_cast<int>( canonicalBytes.size() ) );
			reader.cursize = length;
			MSG_BeginReading( &reader );
			if ( alignment ) (void)MSG_ReadBitsExternal( &reader, alignment );
			if ( entity ) (void)MSG_ReadBitsExternal( &reader, 10 );
			CHECK( !readInternal( &reader, alias ? &guarded.state : &base, &guarded.state ) );
			CHECK( !std::memcmp( &guarded, &unchanged, sizeof( guarded ) ) );
		}
	}
	return 0;
}

int Run() {
	for ( const auto &vector : jk2_fixture::Vectors ) {
		if ( vector.kind == jk2_fixture::Kind::Entity ) {
			const auto sent = Patched<fntech3_jk2_entityState_t>( vector.sent, vector.sentCount );
			const int result = Fixture<fntech3_jk2_entityState_t, entityState_t>( vector,
				[]( auto *msg, auto *from, auto *to ) { return MSG_WriteDeltaJK2Entity( msg, from, to, qtrue ); },
				[]( auto *msg, auto *from, auto *to ) { return MSG_WriteDeltaEntityForWireProfile( msg, from, to, qtrue, NETCHAN_WIRE_JK2_MP ); },
				[&]( auto *msg, auto *from, auto *to ) { return MSG_ReadDeltaEntityForWireProfile( msg, from, to, sent.number, NETCHAN_WIRE_JK2_MP ); }, true );
			if ( result ) return result;
		} else if ( vector.kind == jk2_fixture::Kind::Player ) {
			const int result = Fixture<fntech3_jk2_playerState_t, playerState_t>( vector,
				[]( auto *msg, auto *from, auto *to ) { return MSG_WriteDeltaJK2Playerstate( msg, from, to ); },
				[]( auto *msg, auto *from, auto *to ) { return MSG_WriteDeltaPlayerstateForWireProfile( msg, from, to, NETCHAN_WIRE_JK2_MP ); },
				[]( auto *msg, auto *from, auto *to ) { return MSG_ReadDeltaPlayerstateForWireProfile( msg, from, to, NETCHAN_WIRE_JK2_MP ); }, false );
			if ( result ) return result;
		} else {
			const int result = Fixture<fntech3_jk2_usercmd_t, usercmd_t>( vector,
				[&]( auto *msg, auto *from, auto *to ) { return MSG_WriteDeltaJK2UsercmdKey( msg, vector.key, from, to ); },
				[&]( auto *msg, auto *from, auto *to ) { return MSG_WriteDeltaUsercmdKeyForWireProfile( msg, vector.key, from, to, NETCHAN_WIRE_JK2_MP ); },
				[&]( auto *msg, auto *from, auto *to ) { return MSG_ReadDeltaUsercmdKeyForWireProfile( msg, vector.key, from, to, NETCHAN_WIRE_JK2_MP ); }, false );
			if ( result ) return result;
		}
	}
	/* Raw signed-char hash at retail 0x428710, string writer0x4313ef and
	 * readers0x4315d0/0x431630. Keep the observed percent asymmetry. */
	static const char command[] = { 'p', '%', '\x80', '\xff', 0 };
	CHECK( MSG_HashKeyForWireProfile( NETCHAN_WIRE_JK2_MP, "say 100% ready", 32 ) == 140014 );
	CHECK( MSG_HashKeyForWireProfile( NETCHAN_WIRE_JK2_MP, "\x80\xff%", 32 ) == -10865 );
	CHECK( MSG_HashKeyForWireProfile( NETCHAN_WIRE_LEGACY_Q3, "say 100% ready", 32 ) == 140893 );
	std::array<byte, 64> storage{};
	msg_t writer{}, reader{};
	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	MSG_WriteStringForWireProfile( &writer, command, NETCHAN_WIRE_JK2_MP );
	MSG_Init( &reader, storage.data(), static_cast<int>( storage.size() ) );
	reader.cursize = writer.cursize; MSG_BeginReading( &reader );
	CHECK( MSG_ReadByte( &reader ) == 'p' && MSG_ReadByte( &reader ) == '%' &&
		MSG_ReadByte( &reader ) == '.' && MSG_ReadByte( &reader ) == '.' && MSG_ReadByte( &reader ) == 0 );
	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	MSG_WriteData( &writer, command, sizeof( command ) );
	MSG_Init( &reader, storage.data(), static_cast<int>( storage.size() ) );
	reader.cursize = writer.cursize; MSG_BeginReading( &reader );
	const auto *read = MSG_ReadCommandStringForWireProfile( &reader, NETCHAN_WIRE_JK2_MP );
	CHECK( read[0] == 'p' && read[1] == '.' && static_cast<unsigned char>( read[2] ) == 0x80 &&
		static_cast<unsigned char>( read[3] ) == 0xff );
	return 0;
}
} // namespace jk2_engine_tests

int TestJK2MessageCodecs() { return jk2_engine_tests::Run(); }
#endif
