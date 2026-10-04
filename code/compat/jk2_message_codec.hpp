/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_MESSAGE_CODEC_HPP
#define FNTECH3_JK2_MESSAGE_CODEC_HPP

#include "jk2_game_abi.h"
#include "ieee_delta_float.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

/* Independently implemented Outcast multiplayer protocol-15 message bodies.
 * Retail jk2mp.exe 1.02a (SHA-256 in jk2_wire_fields.def): entity writer/reader
 * 0x431aa0/0x431cd0, player 0x431f60/0x432340, keyed command 0x431750/0x431900.
 * The bodies use the engine's existing bounded Huffman bit IO, observed at
 * 0x431010. The external records and tables belong solely to this profile.
 * Readers publish only complete records; untransmitted player bytes retain
 * the baseline. No external implementation text was incorporated. */
namespace fntech3::jk2::message {

using Entity = fntech3_jk2_entityState_t;
using Player = fntech3_jk2_playerState_t;
using Command = fntech3_jk2_usercmd_t;

struct Field { std::size_t offset; int bits; };

#define FNTECH3_JK2_ENTITY_FIELD(member, offset, bits) Field{ offset, bits },
#define FNTECH3_JK2_PLAYER_FIELD(member, offset, bits)
inline constexpr std::array EntityFields{
#include "jk2_wire_fields.def"
};
#undef FNTECH3_JK2_ENTITY_FIELD
#undef FNTECH3_JK2_PLAYER_FIELD
#define FNTECH3_JK2_ENTITY_FIELD(member, offset, bits)
#define FNTECH3_JK2_PLAYER_FIELD(member, offset, bits) Field{ offset, bits },
inline constexpr std::array PlayerFields{
#include "jk2_wire_fields.def"
};
#undef FNTECH3_JK2_ENTITY_FIELD
#undef FNTECH3_JK2_PLAYER_FIELD
#define FNTECH3_JK2_ENTITY_FIELD(member, offset, bits) \
	static_assert( offsetof( Entity, member ) == offset );
#define FNTECH3_JK2_PLAYER_FIELD(member, offset, bits) \
	static_assert( offsetof( Player, member ) == offset );
#include "jk2_wire_fields.def"
#undef FNTECH3_JK2_ENTITY_FIELD
#undef FNTECH3_JK2_PLAYER_FIELD
static_assert( EntityFields.size() == 73 && PlayerFields.size() == 105 );
static_assert( sizeof( Entity ) == ( EntityFields.size() + 1 ) * 4 );
static_assert( sizeof( Player ) == 1368 && sizeof( Command ) == 28 );

inline constexpr int EntityNumberBits = 10;
inline constexpr int MaxEntities = 1 << EntityNumberBits;
inline constexpr int FloatIntBits = 13;
inline constexpr int FloatIntBias = 1 << ( FloatIntBits - 1 );

template<class T> std::uint32_t Word( const T &object, std::size_t offset ) noexcept {
	std::uint32_t result;
	std::memcpy( &result, reinterpret_cast<const std::uint8_t *>( &object ) + offset, 4 );
	return result;
}
template<class T> void SetWord( T &object, std::size_t offset, std::uint32_t value ) noexcept {
	std::memcpy( reinterpret_cast<std::uint8_t *>( &object ) + offset, &value, 4 );
}
inline std::uint32_t FloatWord( float value ) noexcept {
	std::uint32_t result;
	std::memcpy( &result, &value, 4 );
	return result;
}

template<class Writer> void WriteScalar( Writer &io, std::uint32_t value,
	int width, bool entity ) {
	if ( entity ) {
		const bool nonzero = width ? value != 0 : ( value & 0x7fffffffu ) != 0;
		io.Write( nonzero, 1 );
		if ( !nonzero ) return;
	}
	if ( width ) { io.Write( value, width < 0 ? -width : width ); return; }
	/* Retail's conversion sentinel selects raw binary32 outside this range.
	 * Inspect its encoding so fast-math preserves exceptional payloads too. */
	int integer;
	if ( delta::CompactFloatInteger( value, integer ) ) {
		io.Write( 0, 1 );
		io.Write( static_cast<std::uint32_t>( integer + FloatIntBias ), FloatIntBits );
	} else { io.Write( 1, 1 ); io.Write( value, 32 ); }
}
template<class Reader> std::uint32_t ReadScalar( Reader &io, int width, bool entity ) {
	if ( entity && !io.Read( 1 ) ) return 0;
	if ( width ) return io.Read( width );
	if ( io.Read( 1 ) ) return io.Read( 32 );
	return FloatWord( static_cast<float>( static_cast<int>( io.Read( FloatIntBits ) ) - FloatIntBias ) );
}

template<class T, std::size_t N> std::size_t LastChange( const T &from, const T &to,
	const std::array<Field, N> &fields ) noexcept {
	std::size_t last = 0;
	for ( std::size_t i = 0; i < N; ++i )
		if ( Word( from, fields[i].offset ) != Word( to, fields[i].offset ) ) last = i + 1;
	return last;
}
template<class Writer, class T, std::size_t N> void WriteFields( Writer &io,
	const T &from, const T &to, const std::array<Field, N> &fields, std::size_t last,
	bool entity ) {
	io.Write( static_cast<std::uint32_t>( last ), 8 );
	for ( std::size_t i = 0; i < last && io.Good(); ++i ) {
		const auto value = Word( to, fields[i].offset );
		const bool changed = Word( from, fields[i].offset ) != value;
		io.Write( changed, 1 );
		if ( changed ) WriteScalar( io, value, fields[i].bits, entity );
	}
}
template<class Reader, class T, std::size_t N> bool ReadFields( Reader &io,
	T &to, const std::array<Field, N> &fields, bool entity ) {
	const auto last = io.Read( 8 );
	if ( last > N ) return io.Fail();
	for ( std::size_t i = 0; i < last && io.Good(); ++i )
		if ( io.Read( 1 ) ) SetWord( to, fields[i].offset, ReadScalar( io, fields[i].bits, entity ) );
	return io.Good();
}

template<class Writer> bool WriteEntity( Writer &io, const Entity *from,
	const Entity *to, bool force ) {
	if ( !from && !to ) return io.Good();
	const int number = to ? to->number : from->number;
	if ( number < 0 || number >= MaxEntities ) return io.Fail();
	if ( !to ) {
		io.Write( static_cast<std::uint32_t>( number ), EntityNumberBits );
		io.Write( 1, 1 );
		return io.Good();
	}
	const Entity empty{};
	if ( !from ) from = &empty;
	const auto last = LastChange( *from, *to, EntityFields );
	if ( !last && !force ) return io.Good();
	io.Write( static_cast<std::uint32_t>( number ), EntityNumberBits );
	io.Write( 0, 1 );
	io.Write( last != 0, 1 );
	if ( last ) WriteFields( io, *from, *to, EntityFields, last, true );
	return io.Good();
}
template<class Reader> bool ReadEntity( Reader &io, const Entity *from,
	Entity *to, int number ) {
	if ( !to || number < 0 || number >= MaxEntities ) return io.Fail();
	Entity result{};
	if ( io.Read( 1 ) ) result.number = MaxEntities - 1;
	else {
		if ( from ) result = *from;
		result.number = number;
		if ( io.Read( 1 ) && !ReadFields( io, result, EntityFields, true ) ) return false;
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

inline std::uint32_t ArrayMask( const std::int32_t *from, const std::int32_t *to ) noexcept {
	std::uint32_t mask = 0;
	for ( unsigned i = 0; i < 16; ++i ) if ( from[i] != to[i] ) mask |= 1u << i;
	return mask;
}
template<class Writer> void WriteArray( Writer &io, const std::int32_t *to,
	std::uint32_t mask, int width ) {
	io.Write( mask != 0, 1 );
	if ( !mask ) return;
	io.Write( mask, 16 );
	for ( unsigned i = 0; i < 16 && io.Good(); ++i )
		if ( mask & ( 1u << i ) ) io.Write( static_cast<std::uint32_t>( to[i] ), width );
}
template<class Reader> void ReadArray( Reader &io, std::int32_t *to, int width ) {
	if ( !io.Read( 1 ) ) return;
	const auto mask = io.Read( 16 );
	for ( unsigned i = 0; i < 16 && io.Good(); ++i ) {
		if ( mask & ( 1u << i ) ) {
			const auto value = io.Read( width );
			std::memcpy( &to[i], &value, 4 );
		}
	}
}
template<class Writer> bool WritePlayer( Writer &io, const Player *from, const Player *to ) {
	if ( !to ) return io.Fail();
	const Player empty{};
	if ( !from ) from = &empty;
	WriteFields( io, *from, *to, PlayerFields, LastChange( *from, *to, PlayerFields ), false );
	const auto stats = ArrayMask( from->stats, to->stats );
	const auto persistant = ArrayMask( from->persistant, to->persistant );
	const auto ammo = ArrayMask( from->ammo, to->ammo );
	const auto powerups = ArrayMask( from->powerups, to->powerups );
	io.Write( ( stats | persistant | ammo | powerups ) != 0, 1 );
	if ( stats | persistant | ammo | powerups ) {
		WriteArray( io, to->stats, stats, 16 );
		WriteArray( io, to->persistant, persistant, 16 );
		WriteArray( io, to->ammo, ammo, 16 );
		WriteArray( io, to->powerups, powerups, 32 );
	}
	return io.Good();
}
template<class Reader> bool ReadPlayer( Reader &io, const Player *from, Player *to ) {
	if ( !to ) return io.Fail();
	Player result{};
	if ( from ) result = *from;
	if ( !ReadFields( io, result, PlayerFields, false ) ) return false;
	if ( io.Read( 1 ) ) {
		ReadArray( io, result.stats, -16 );
		ReadArray( io, result.persistant, -16 );
		ReadArray( io, result.ammo, -16 );
		ReadArray( io, result.powerups, 32 );
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

struct CommandField { std::size_t offset; int bits; int bytes; bool signedByte; };
/* Retail 0x43182d..0x4318eb and 0x431960..0x431a0d. */
inline constexpr std::array<CommandField, 11> CommandFields{{
	{4,16,4,false}, {8,16,4,false}, {12,16,4,false},
	{24,8,1,true}, {25,8,1,true}, {26,8,1,true},
	{16,16,4,false}, {20,8,1,false}, {21,8,1,false},
	{22,8,1,false}, {23,8,1,false}
}};
inline std::uint32_t CommandWord( const Command &cmd, const CommandField &field ) noexcept {
	if ( field.bytes == 4 ) return Word( cmd, field.offset );
	const auto byte = reinterpret_cast<const std::uint8_t *>( &cmd )[field.offset];
	return field.signedByte && byte >= 128 ? std::uint32_t( int( byte ) - 256 ) : byte;
}
inline void SetCommandWord( Command &cmd, const CommandField &field, std::uint32_t value ) noexcept {
	if ( field.bytes == 4 ) SetWord( cmd, field.offset, value );
	else reinterpret_cast<std::uint8_t *>( &cmd )[field.offset] = static_cast<std::uint8_t>( value );
}
template<class Writer> bool WriteCommand( Writer &io, std::uint32_t key,
	const Command *from, const Command *to ) {
	if ( !to ) return io.Fail();
	const Command empty{};
	if ( !from ) from = &empty;
	const auto delta = std::uint32_t( to->serverTime ) - std::uint32_t( from->serverTime );
	/* Retail compares the modular delta as a signed x86 integer. A backwards
	 * command therefore uses its low byte too; avoid signed C++ overflow. */
	const bool shortTime = delta < 256u || ( delta & 0x80000000u );
	io.Write( shortTime, 1 );
	io.Write( shortTime ? delta : std::uint32_t( to->serverTime ), shortTime ? 8 : 32 );
	bool changed = false;
	for ( const auto &field : CommandFields )
		changed |= CommandWord( *from, field ) != CommandWord( *to, field );
	io.Write( changed, 1 );
	if ( changed ) {
		key ^= std::uint32_t( to->serverTime );
		for ( const auto &field : CommandFields ) {
			const auto value = CommandWord( *to, field );
			const bool different = value != CommandWord( *from, field );
			io.Write( different, 1 );
			if ( different ) io.Write( value ^ key, field.bits );
		}
	}
	return io.Good();
}
template<class Reader> bool ReadCommand( Reader &io, std::uint32_t key,
	const Command *from, Command *to ) {
	if ( !to ) return io.Fail();
	Command result{};
	if ( from ) result = *from;
	const auto time = io.Read( 1 ) ? std::uint32_t( result.serverTime ) + io.Read( 8 ) : io.Read( 32 );
	SetWord( result, 0, time );
	if ( io.Read( 1 ) ) {
		key ^= time;
		for ( const auto &field : CommandFields ) {
			if ( io.Read( 1 ) ) {
				/* Retail 0x431730 indexes its 1,3,7,... table by width,
				 * retaining one extra key bit in angle/button int fields. */
				const auto mask = ( 1u << ( field.bits + 1 ) ) - 1u;
				SetCommandWord( result, field, io.Read( field.bits ) ^ ( key & mask ) );
			}
		}
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

} // namespace fntech3::jk2::message
#endif
