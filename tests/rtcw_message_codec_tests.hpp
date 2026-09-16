/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_MESSAGE_CODEC_TESTS_HPP
#define FNTECH3_RTCW_MESSAGE_CODEC_TESTS_HPP

#include "../code/compat/rtcw_message_codec.hpp"
#include "../code/compat/rtcw_state_adapter.hpp"
#include <cstdio>
#include <vector>

namespace {
using namespace fntech3::rtcw::message;

struct RTCWWordPatch { std::size_t offset; std::uint32_t value; };
struct RTCWFixture {
	const char *name;
	comGameId_t profile;
	int kind;
	std::uint32_t key;
	unsigned flags;
	int offset, bits;
	std::vector<RTCWWordPatch> from, to, decoded;
	std::vector<byte> packet;
};
#include "fixtures/rtcw_message_vectors.inc"

template<class State> void RTCWApply( State &state, const std::vector<RTCWWordPatch> &patches ) {
	for ( const auto &patch : patches ) {
		if ( patch.offset > sizeof( State ) - 4 ) std::abort();
		SetWord( state, patch.offset, patch.value );
	}
}

template<class State> bool RTCWWrite( const RTCWFixture &fixture, msg_t *msg,
	const State *from, const State *to ) {
	if constexpr ( std::is_same_v<State, Entity> )
		return MSG_WriteDeltaRTCWEntity( msg, from, ( fixture.flags & 2 ) ? nullptr : to,
			( fixture.flags & 1 ) ? qtrue : qfalse, fixture.profile );
	else if constexpr ( std::is_same_v<State, SPCommand> )
		return MSG_WriteDeltaRTCWSPUsercmdKey( msg, fixture.key, from, to );
	else if constexpr ( std::is_same_v<State, MPCommand> )
		return MSG_WriteDeltaRTCWMPUsercmdKey( msg, fixture.key, from, to );
	else if constexpr ( std::is_same_v<State, SPPlayer> )
		return MSG_WriteDeltaRTCWSPPlayerstate( msg, from, to );
	else return MSG_WriteDeltaRTCWMPPlayerstate( msg, from, to );
}

template<class State> bool RTCWRead( const RTCWFixture &fixture, msg_t *msg,
	const State *from, State *to ) {
	if constexpr ( std::is_same_v<State, Entity> ) {
		const int number = MSG_ReadEntitynum( msg );
		return MSG_ReadDeltaRTCWEntity( msg, from, to, number, fixture.profile );
	} else if constexpr ( std::is_same_v<State, SPCommand> )
		return MSG_ReadDeltaRTCWSPUsercmdKey( msg, fixture.key, from, to );
	else if constexpr ( std::is_same_v<State, MPCommand> )
		return MSG_ReadDeltaRTCWMPUsercmdKey( msg, fixture.key, from, to );
	else if constexpr ( std::is_same_v<State, SPPlayer> )
		return MSG_ReadDeltaRTCWSPPlayerstate( msg, from, to );
	else return MSG_ReadDeltaRTCWMPPlayerstate( msg, from, to );
}

template<class State> using RTCWCanonical = std::conditional_t<std::is_same_v<State, Entity>,
	entityState_t, std::conditional_t<std::is_same_v<State, SPPlayer> || std::is_same_v<State, MPPlayer>,
	playerState_t, usercmd_t>>;

template<class State> bool RTCWCanonicalWrite( const RTCWFixture &fixture, msg_t *msg,
	const State *from, const State *to ) {
	const auto wire = fixture.profile == COM_GAME_RTCW_SP ? NETCHAN_WIRE_RTCW_SP : NETCHAN_WIRE_RTCW_MP;
	if constexpr ( std::is_same_v<State, entityState_t> )
		return MSG_WriteDeltaEntityForWireProfile( msg, from, ( fixture.flags & 2 ) ? nullptr : to,
			( fixture.flags & 1 ) ? qtrue : qfalse, wire );
	else if constexpr ( std::is_same_v<State, playerState_t> )
		return MSG_WriteDeltaPlayerstateForWireProfile( msg, from, to, wire );
	else return MSG_WriteDeltaUsercmdKeyForWireProfile( msg, fixture.key, from, to, wire );
}

template<class State> bool RTCWCanonicalRead( const RTCWFixture &fixture, msg_t *msg,
	const State *from, State *to ) {
	const auto wire = fixture.profile == COM_GAME_RTCW_SP ? NETCHAN_WIRE_RTCW_SP : NETCHAN_WIRE_RTCW_MP;
	if constexpr ( std::is_same_v<State, entityState_t> ) {
		const int number = MSG_ReadEntitynum( msg );
		return MSG_ReadDeltaEntityForWireProfile( msg, from, to, number, wire );
	} else if constexpr ( std::is_same_v<State, playerState_t> )
		return MSG_ReadDeltaPlayerstateForWireProfile( msg, from, to, wire );
	else return MSG_ReadDeltaUsercmdKeyForWireProfile( msg, fixture.key, from, to, wire );
}

template<class State> int RTCWCheckCanonicalFixture( const RTCWFixture &fixture ) {
	State baseline{}, sent{}, expected{};
	RTCWApply( baseline, fixture.from );
	RTCWApply( sent, fixture.to );
	RTCWApply( expected, fixture.decoded );
	RTCWCanonical<State> oldInternal{}, sentInternal{}, expectedInternal{};
	CHECK( fntech3::rtcw::state::ToInternal( baseline, oldInternal ) );
	CHECK( fntech3::rtcw::state::ToInternal( sent, sentInternal ) );
	CHECK( fntech3::rtcw::state::ToInternal( expected, expectedInternal ) );
	std::vector<byte> bytes( fixture.packet.size() );
	msg_t msg{};
	if ( !( fixture.flags & 4 ) ) {
		MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
		if ( fixture.offset ) MSG_WriteBits( &msg, 0x55, fixture.offset );
		CHECK( RTCWCanonicalWrite( fixture, &msg, &oldInternal, &sentInternal ) );
		CHECK( msg.bit == fixture.bits && bytes == fixture.packet );
	}
	bytes = fixture.packet;
	for ( std::size_t size = 0; size <= bytes.size(); ++size ) {
		for ( bool inPlace : {false, true} ) {
			struct { std::uint32_t before; RTCWCanonical<State> value; std::uint32_t after; } output{};
			std::memset( &output, 0xa5, sizeof( output ) );
			if ( inPlace ) output.value = oldInternal;
			const auto unchanged = output;
			MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
			msg.cursize = static_cast<int>( size );
			if ( fixture.offset ) { msg.bit = fixture.offset; msg.readcount = 1; }
			const bool complete = size == bytes.size();
			CHECK( RTCWCanonicalRead( fixture, &msg, inPlace ? &output.value : &oldInternal, &output.value ) == complete );
			CHECK( output.before == unchanged.before && output.after == unchanged.after );
			CHECK( std::memcmp( &output.value, complete ? &expectedInternal : &unchanged.value, sizeof( output.value ) ) == 0 );
		}
	}
	return 0;
}

template<class State> int RTCWCheckFixture( const RTCWFixture &fixture ) {
	State baseline{}, sent{}, expected{};
	RTCWApply( baseline, fixture.from );
	RTCWApply( sent, fixture.to );
	RTCWApply( expected, fixture.decoded );
	// The reserved dictionary symbol has decoder evidence only: the ordinary
	// writer deliberately retains the original engine's uncompressed choice.
	for ( std::size_t size = 0; !( fixture.flags & 4 ) && size <= fixture.packet.size(); ++size ) {
		std::vector<byte> guarded( size + 2, 0 );
		guarded.front() = 0xa5;
		guarded.back() = 0x5a;
		msg_t writer{};
		MSG_Init( &writer, guarded.data() + 1, static_cast<int>( size ) );
		if ( fixture.offset ) MSG_WriteBits( &writer, 0x55, fixture.offset );
		const bool complete = size == fixture.packet.size();
		CHECK( RTCWWrite( fixture, &writer, &baseline, &sent ) == complete );
		CHECK( guarded.front() == 0xa5 && guarded.back() == 0x5a );
		if ( complete ) {
			CHECK( writer.bit == fixture.bits && writer.cursize == static_cast<int>( size ) );
			CHECK( std::memcmp( guarded.data() + 1, fixture.packet.data(), size ) == 0 );
		} else CHECK( writer.overflowed );
	}
	// Relocate the reference packet at every bit alignment, then remove every
	// possible byte suffix. Neither an in-place baseline nor a separate output
	// may be partially overwritten by a failed read.
	const int payloadBits = fixture.bits - fixture.offset;
	for ( int offset = 0; offset < 8; ++offset ) {
		const int fullSize = ( offset + payloadBits ) / 8 + 1;
		std::vector<byte> packet( fullSize, 0 );
		for ( int bit = 0; bit < payloadBits; ++bit ) {
			const int sourceBit = bit + fixture.offset;
			packet[( bit + offset ) / 8] |= static_cast<byte>(
				( ( fixture.packet[sourceBit / 8] >> ( sourceBit % 8 ) ) & 1 ) << ( ( bit + offset ) % 8 ) );
		}
		for ( int size = 0; size <= fullSize; ++size ) {
			for ( bool inPlace : { false, true } ) {
				struct Guarded { std::uint32_t before; State state; std::uint32_t after; } output{};
				output.before = 0x12345678;
				output.after = 0x87654321;
				if ( inPlace ) output.state = baseline;
				else std::memset( &output.state, 0xcc, sizeof( State ) );
				const State untouched = output.state;
				msg_t reader{};
				MSG_Init( &reader, packet.data(), fullSize );
				reader.cursize = size;
				reader.bit = offset;
				reader.readcount = offset ? 1 : 0;
				const bool complete = size == fullSize;
				CHECK( RTCWRead( fixture, &reader, inPlace ? &output.state : &baseline, &output.state ) == complete );
				CHECK( std::memcmp( &output.state, complete ? &expected : &untouched, sizeof( State ) ) == 0 );
				CHECK( output.before == 0x12345678 && output.after == 0x87654321 );
				if ( complete ) CHECK( reader.bit == offset + payloadBits );
				else CHECK( reader.readcount > reader.cursize );
			}
		}
	}
	return RTCWCheckCanonicalFixture<State>( fixture );
}

int TestRTCWReferencePackets() {
	for ( const auto &fixture : rtcwFixtures ) {
		int result;
		if ( fixture.kind == 1 ) result = RTCWCheckFixture<Entity>( fixture );
		else if ( fixture.kind == 0 ) result = fixture.profile == COM_GAME_RTCW_SP
			? RTCWCheckFixture<SPCommand>( fixture ) : RTCWCheckFixture<MPCommand>( fixture );
		else result = fixture.profile == COM_GAME_RTCW_SP
			? RTCWCheckFixture<SPPlayer>( fixture ) : RTCWCheckFixture<MPPlayer>( fixture );
		if ( result ) { std::fprintf( stderr, "RTCW %s failed at line %d\n", fixture.name, result ); return result; }
	}
	return 0;
}

template<class State> int RTCWCheckFloats( comGameId_t profile ) {
	std::array<byte, 2048> buffer{};
	State baseline{};
	constexpr std::size_t origin = std::is_same_v<State, Entity> ? offsetof( Entity, origin ) : offsetof( PlayerPrefix, origin );
	RTCWFixture fixture{ "float", profile, 0, 0, 1, 0, 0, {}, {}, {}, {} };
	for ( std::uint32_t bits : { 0x80000000u, 0x7fc01234u, 0x7f801234u, 0xff800000u,
		0x7f800000u, 0x4f000000u, 0xcf000001u, 0x45800000u, 0xc5800000u } ) {
		State sent = baseline, output{};
		SetWord( sent, origin, bits );
		msg_t msg{};
		MSG_Init( &msg, buffer.data(), static_cast<int>( buffer.size() ) );
		CHECK( RTCWWrite( fixture, &msg, &baseline, &sent ) );
		MSG_BeginReading( &msg );
		CHECK( RTCWRead( fixture, &msg, &baseline, &output ) );
		CHECK( Word( output, origin ) == ( bits == 0x80000000u ? 0u : bits ) );
	}
	return 0;
}

template<class Command> int RTCWCheckClock( comGameId_t profile ) {
	std::array<byte, 128> buffer{};
	RTCWFixture fixture{ "clock", profile, 0, 0x76543210, 1, 0, 0, {}, {}, {}, {} };
	for ( const std::array<std::uint32_t, 2> times : {
		std::array<std::uint32_t, 2>{ 0x7ffffff0u, 0x80000010u },
		{ 0xfffffff0u, 0x10u }, { 300, 299 }, { 0, 255 }, { 0, 256 } } ) {
		Command baseline{}, sent{}, output{};
		SetWord( baseline, 0, times[0] );
		SetWord( sent, 0, times[1] );
		sent.common.forwardmove = -128;
		msg_t msg{};
		MSG_Init( &msg, buffer.data(), static_cast<int>( buffer.size() ) );
		CHECK( RTCWWrite( fixture, &msg, &baseline, &sent ) );
		MSG_BeginReading( &msg );
		CHECK( RTCWRead( fixture, &msg, &baseline, &output ) );
		CHECK( std::memcmp( &output, &sent, sizeof( sent ) ) == 0 );
	}
	return 0;
}

int TestRTCWMalformedPackets() {
	std::array<byte, 128> buffer{};
	msg_t msg{};
	Entity baseline{}, output{};
	output.number = 777;
	MSG_Init( &msg, buffer.data(), static_cast<int>( buffer.size() ) );
	MSG_WriteBits( &msg, 0, 1 ); // not removed; caller already consumed number
	MSG_WriteBits( &msg, 1, 1 ); // delta
	MSG_WriteByte( &msg, 72 ); // beyond the 71-field MP contract
	MSG_BeginReading( &msg );
	CHECK( !MSG_ReadDeltaRTCWEntity( &msg, &baseline, &output, 0, COM_GAME_RTCW_MP ) );
	CHECK( output.number == 777 && msg.readcount > msg.cursize );
	MPPlayer oldPlayer{}, newPlayer{};
	newPlayer.common.commandTime = 888;
	MSG_Init( &msg, buffer.data(), static_cast<int>( buffer.size() ) );
	MSG_WriteByte( &msg, 78 );
	MSG_BeginReading( &msg );
	CHECK( !MSG_ReadDeltaRTCWMPPlayerstate( &msg, &oldPlayer, &newPlayer ) );
	CHECK( newPlayer.common.commandTime == 888 && msg.readcount > msg.cursize );
	MSG_Init( &msg, buffer.data(), static_cast<int>( buffer.size() ) );
	CHECK( !MSG_WriteDeltaRTCWEntity( &msg, &baseline, &output, qtrue, COM_GAME_QUAKE3 ) );
	CHECK( !MSG_ReadDeltaRTCWEntity( &msg, &baseline, &output, 0, COM_GAME_QUAKELIVE ) );
	CHECK( msg.bit == 0 && msg.cursize == 0 && output.number == 777 );
	CHECK( !MSG_WriteDeltaRTCWSPUsercmdKey( nullptr, 0, nullptr, nullptr ) );
	CHECK( !MSG_ReadDeltaRTCWSPUsercmdKey( nullptr, 0, nullptr, nullptr ) );
	CHECK( !MSG_ReadDeltaRTCWMPPlayerstate( nullptr, nullptr, &newPlayer ) );
	CHECK( !MSG_WriteDeltaRTCWSPPlayerstate( nullptr, nullptr, nullptr ) );
	MSG_InitOOB( &msg, buffer.data(), static_cast<int>( buffer.size() ) );
	CHECK( !MSG_WriteDeltaRTCWEntity( &msg, &baseline, &output, qtrue, COM_GAME_RTCW_SP ) );
	return 0;
}

template<class State> int RTCWCheckRandomInput( comGameId_t profile ) {
	RTCWFixture fixture{ "random", profile, 0, 0xabcdef01, 1, 0, 0, {}, {}, {}, {} };
	std::uint32_t random = 9010;
	std::array<byte, 96> bytes{};
	State baseline{};
	for ( int attempt = 0; attempt < 256; ++attempt ) {
		for ( auto &value : bytes ) {
			random = random * 1664525u + 1013904223u;
			value = static_cast<byte>( random >> 24 );
		}
		struct { std::uint32_t before; State state; std::uint32_t after; } output{};
		output.before = 0x12345678;
		output.after = 0x87654321;
		std::memset( &output.state, 0xa5, sizeof( State ) );
		const State untouched = output.state;
		msg_t msg{};
		MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
		msg.cursize = attempt % static_cast<int>( bytes.size() );
		msg.bit = attempt % 8;
		msg.readcount = msg.bit ? 1 : 0;
		if ( !RTCWRead( fixture, &msg, &baseline, &output.state ) )
			CHECK( std::memcmp( &output.state, &untouched, sizeof( State ) ) == 0 );
		CHECK( output.before == 0x12345678 && output.after == 0x87654321 );
	}
	return 0;
}

int TestRTCWStringsAndCanonicalFailures() {
	std::array<byte, 256> bytes{};
	const char raw[]{'a', '%', '\x80', '\xff', 0};
	const char written[]{'a', '%', '.', '.', 0};
	const char bigRead[]{'a', '.', '\x80', '\xff', 0};
	for ( auto wire : {NETCHAN_WIRE_RTCW_SP, NETCHAN_WIRE_RTCW_MP} ) {
		CHECK( MSG_HashKeyForWireProfile( wire, "say 100% ready", 32 ) == 140014 );
		CHECK( MSG_HashKeyForWireProfile( wire, raw + 2, 32 ) == -15354 );
		for ( bool big : {false, true} ) {
			msg_t msg{};
			MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
			if ( big ) MSG_WriteBigStringForWireProfile( &msg, raw, wire );
			else MSG_WriteStringForWireProfile( &msg, raw, wire );
			MSG_BeginReading( &msg );
			for ( unsigned char value : written ) CHECK( MSG_ReadByte( &msg ) == value );
			MSG_Clear( &msg );
			MSG_WriteData( &msg, raw, sizeof( raw ) );
			MSG_BeginReading( &msg );
			const char *text = big ? MSG_ReadBigStringForWireProfile( &msg, wire )
				: MSG_ReadCommandStringForWireProfile( &msg, wire );
			CHECK( std::memcmp( text, big ? bigRead : "a...", sizeof( raw ) ) == 0 );
		}
		msg_t msg{};
		Entity baseline{}, malformed{};
		malformed.number = 9;
		malformed.apos.trType = 255;
		MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
		const auto mode = wire == NETCHAN_WIRE_RTCW_SP ? COM_GAME_RTCW_SP : COM_GAME_RTCW_MP;
		CHECK( MSG_WriteDeltaRTCWEntity( &msg, &baseline, &malformed, qtrue, mode ) );
		MSG_BeginReading( &msg );
		entityState_t internal{};
		internal.number = 777;
		const auto unchanged = internal;
		const int number = MSG_ReadEntitynum( &msg );
		CHECK( !MSG_ReadDeltaEntityForWireProfile( &msg, nullptr, &internal, number, wire ) );
		CHECK( msg.readcount > msg.cursize && std::memcmp( &internal, &unchanged, sizeof( internal ) ) == 0 );
		MSG_Clear( &msg );
		internal.pos.trType = TR_QL_ACCEL;
		CHECK( !MSG_WriteDeltaEntityForWireProfile( &msg, nullptr, &internal, qtrue, wire ) );
		CHECK( msg.overflowed && msg.bit == 0 );
		MSG_Clear( &msg );
		CHECK( !MSG_ReadDeltaPlayerstateForWireProfile( &msg, nullptr, nullptr, wire ) );
		CHECK( msg.readcount > msg.cursize );
	}
	return 0;
}

int TestQuakeWireClearsRTCWState() {
	std::array<byte, 256> bytes{};
	for ( auto wire : {NETCHAN_WIRE_LEGACY_Q3, NETCHAN_WIRE_IOQ3, NETCHAN_WIRE_QL_RETAIL} ) {
		for ( bool changed : {false, true} ) {
			entityState_t baseline{};
			std::memset( &baseline.rtcw, 0x55, sizeof( baseline.rtcw ) );
			auto sent = baseline;
			if ( changed ) sent.origin[2] = 24;
			entityState_t output{};
			msg_t msg{};
			MSG_Init( &msg, bytes.data(), static_cast<int>( bytes.size() ) );
			CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &baseline, &sent, qtrue, wire ) );
			MSG_BeginReading( &msg );
			const int number = MSG_ReadEntitynum( &msg );
			CHECK( MSG_ReadDeltaEntityForWireProfile( &msg, &baseline, &output, number, wire ) );
			const rtcwEntityStateExtension_t emptyEntity{};
			CHECK( std::memcmp( &output.rtcw, &emptyEntity, sizeof( emptyEntity ) ) == 0 );
			playerState_t player{};
			std::memset( &player.rtcw, 0x55, sizeof( player.rtcw ) );
			auto next = player;
			if ( changed ) next.stats[0] = 20;
			playerState_t decoded{};
			MSG_Clear( &msg );
			CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &msg, &player, &next, wire ) );
			MSG_BeginReading( &msg );
			CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &msg, &player, &decoded, wire ) );
			const rtcwPlayerStateExtension_t emptyPlayer{};
			CHECK( std::memcmp( &decoded.rtcw, &emptyPlayer, sizeof( emptyPlayer ) ) == 0 );
		}
	}
	return 0;
}

int TestRTCWMessageCodecs() {
	if ( int result = TestRTCWStringsAndCanonicalFailures() ) return result;
	if ( int result = TestQuakeWireClearsRTCWState() ) return result;
	if ( int result = TestRTCWReferencePackets() ) return result;
	if ( int result = RTCWCheckFloats<Entity>( COM_GAME_RTCW_SP ) ) return result;
	if ( int result = RTCWCheckFloats<Entity>( COM_GAME_RTCW_MP ) ) return result;
	if ( int result = RTCWCheckFloats<SPPlayer>( COM_GAME_RTCW_SP ) ) return result;
	if ( int result = RTCWCheckFloats<MPPlayer>( COM_GAME_RTCW_MP ) ) return result;
	if ( int result = RTCWCheckClock<SPCommand>( COM_GAME_RTCW_SP ) ) return result;
	if ( int result = RTCWCheckClock<MPCommand>( COM_GAME_RTCW_MP ) ) return result;
	if ( int result = RTCWCheckRandomInput<Entity>( COM_GAME_RTCW_SP ) ) return result;
	if ( int result = RTCWCheckRandomInput<Entity>( COM_GAME_RTCW_MP ) ) return result;
	if ( int result = RTCWCheckRandomInput<SPCommand>( COM_GAME_RTCW_SP ) ) return result;
	if ( int result = RTCWCheckRandomInput<MPCommand>( COM_GAME_RTCW_MP ) ) return result;
	if ( int result = RTCWCheckRandomInput<SPPlayer>( COM_GAME_RTCW_SP ) ) return result;
	if ( int result = RTCWCheckRandomInput<MPPlayer>( COM_GAME_RTCW_MP ) ) return result;
	return TestRTCWMalformedPackets();
}
} // namespace
#endif
