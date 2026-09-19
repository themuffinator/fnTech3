/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_MESSAGE_CODEC_HPP
#define FNTECH3_STVEF_MESSAGE_CODEC_HPP

#include "stvef_game_abi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

/* Independently implemented Elite Force Holomatch protocol-24 message bodies.
 * Retail stvoyHM.exe (1.20) writes every message as plain LSB-first bits with
 * no Huffman stage (MSG_WriteBits at 0x422bb0). Entity deltas carry a 5-bit
 * index into a 31-entry dictionary of changed-field vectors, or 31 followed
 * by the raw 50-bit vector; changed values follow with no per-field flags.
 * Player deltas flag each of 48 fields, then the four 16-entry arrays.
 * User commands are not keyed. Field order, widths and the dictionary are
 * protocol facts observed in the retail engine and in Lilium Voyager; see
 * docs/fntech3/2026-09-18-stvef-holomatch.md.
 *
 * Storage is the frozen external POD, never engine state. The IO adapter
 * supplies bounded Read/Write, Good and Fail; reads decode into a temporary
 * and publish only a complete record. Byte alignment for strings and raw data
 * belongs to the message layer, not these bodies. */
namespace fntech3::stvef::message {

struct Field {
	std::size_t offset;
	int bits; // zero: binary32; negative: sign-extended integer
};

using Entity = fntech3_stvef_mp_entityState_t;
using Player = fntech3_stvef_mp_playerState_t;
using Command = fntech3_stvef_mp_usercmd_t;

inline constexpr int kEntityNumberBits = FNTECH3_STVEF_GENTITYNUM_BITS;
inline constexpr int kFloatIntBits = 13;
inline constexpr int kFloatIntBias = 1 << ( kFloatIntBits - 1 );

#define E(member, width) Field{ offsetof( Entity, member ), width }
inline constexpr std::array EntityFields{
	E(eType, 8), E(eFlags, 24),
	E(pos.trType, 8), E(pos.trTime, 32), E(pos.trDuration, 32),
	E(pos.trBase[0], 0), E(pos.trBase[1], 0), E(pos.trBase[2], 0),
	E(pos.trDelta[0], 0), E(pos.trDelta[1], 0), E(pos.trDelta[2], 0),
	E(apos.trType, 8), E(apos.trTime, 32), E(apos.trDuration, 32),
	E(apos.trBase[0], 0), E(apos.trBase[1], 0), E(apos.trBase[2], 0),
	E(apos.trDelta[0], 0), E(apos.trDelta[1], 0), E(apos.trDelta[2], 0),
	E(time, 32), E(time2, 32),
	E(origin[0], 0), E(origin[1], 0), E(origin[2], 0),
	E(origin2[0], 0), E(origin2[1], 0), E(origin2[2], 0),
	E(angles[0], 0), E(angles[1], 0), E(angles[2], 0),
	E(angles2[0], 0), E(angles2[1], 0), E(angles2[2], 0),
	E(otherEntityNum, 10), E(otherEntityNum2, 10), E(groundEntityNum, 10),
	E(loopSound, 16), E(constantLight, 32),
	E(modelindex, 8), E(modelindex2, 8), E(frame, 16), E(clientNum, 8),
	E(solid, 24), E(event, 10), E(eventParm, 8), E(powerups, 16),
	E(weapon, 8), E(legsAnim, 8), E(torsoAnim, 8)
};
#undef E
static_assert( EntityFields.size() == 50 );
static_assert( sizeof( Entity ) == ( EntityFields.size() + 1 ) * 4,
	"every entity word except the number is a wire field" );

inline constexpr std::size_t kVectorBytes = 8;
using ChangeVector = std::array<std::uint8_t, kVectorBytes>;

/* The retail dictionary: bit i of the little-endian byte vector marks entity
 * field i. Symbol 31 means a raw vector follows. */
inline constexpr std::array<ChangeVector, 31> ChangeVectors{ {
	{ 0x60, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x40, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x20, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x20, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x40, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x60, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x60, 0x80, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00 },
	{ 0x60, 0x80, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00 },
	{ 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xe0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x60, 0xc0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 },
	{ 0x60, 0xc0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00 },
	{ 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x60, 0xc0, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00 },
	{ 0x60, 0xc0, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00 },
	{ 0x60, 0x80, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00 },
	{ 0x60, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00 },
	{ 0xe1, 0x00, 0xc0, 0x01, 0x90, 0x00, 0x00, 0x00 },
	{ 0xed, 0x07, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00 },
	{ 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 },
	{ 0x62, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 },
	{ 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00 },
	{ 0xe0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x60, 0x80, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 }
} };
inline constexpr std::uint32_t kRawVectorSymbol = 31;

#define P(member, width) Field{ offsetof( Player, member ), width }
inline constexpr std::array PlayerFields{
	P(commandTime, 32), P(pm_type, 8), P(bobCycle, 8), P(pm_flags, 16), P(pm_time, -16),
	P(origin[0], 0), P(origin[1], 0), P(origin[2], 0),
	P(velocity[0], 0), P(velocity[1], 0), P(velocity[2], 0),
	P(weaponTime, -16), P(gravity, 16), P(speed, 16),
	P(delta_angles[0], 16), P(delta_angles[1], 16), P(delta_angles[2], 16),
	P(groundEntityNum, 10), P(legsTimer, 8), P(torsoTimer, 12),
	P(legsAnim, 8), P(torsoAnim, 8), P(movementDir, 4), P(eFlags, 16),
	P(eventSequence, 16), P(events[0], 8), P(events[1], 8), P(events[2], 8), P(events[3], 8),
	P(eventParms[0], 8), P(eventParms[1], 8), P(eventParms[2], 8), P(eventParms[3], 8),
	P(externalEvent, 10), P(externalEventParm, 8), P(clientNum, 8),
	P(weapon, 5), P(weaponstate, 4),
	P(viewangles[0], 0), P(viewangles[1], 0), P(viewangles[2], 0), P(viewheight, -8),
	P(damageEvent, 8), P(damageYaw, 8), P(damagePitch, 8), P(damageCount, 8),
	P(damageShieldCount, 8), P(introTime, 32)
};
#undef P
static_assert( PlayerFields.size() == 48 );

template<class T> std::uint32_t Word( const T &object, std::size_t offset ) noexcept {
	std::uint32_t value;
	std::memcpy( &value, reinterpret_cast<const std::uint8_t *>( &object ) + offset, 4 );
	return value;
}

template<class T> void SetWord( T &object, std::size_t offset, std::uint32_t value ) noexcept {
	std::memcpy( reinterpret_cast<std::uint8_t *>( &object ) + offset, &value, 4 );
}

inline std::uint32_t FloatWord( float value ) noexcept {
	std::uint32_t bits;
	std::memcpy( &bits, &value, 4 );
	return bits;
}

/* Protocol 24 sends a changed float as a biased 13-bit integer when it is
 * integral and in range, else as 32 raw bits; a changed integer is written
 * with its field width. There is no zero shortcut. */
template<class Writer> void WriteScalar( Writer &io, std::uint32_t value, int width ) {
	if ( width ) {
		io.Write( value, width < 0 ? -width : width );
		return;
	}
	float real;
	std::memcpy( &real, &value, 4 );
	// Range-check before converting: NaN, infinity and out-of-range values
	// must never reach an undefined float-to-int cast.
	if ( real >= -static_cast<float>( kFloatIntBias ) && real < static_cast<float>( kFloatIntBias )
		&& static_cast<float>( static_cast<int>( real ) ) == real ) {
		io.Write( 0, 1 );
		io.Write( static_cast<std::uint32_t>( static_cast<int>( real ) + kFloatIntBias ), kFloatIntBits );
	} else {
		io.Write( 1, 1 );
		io.Write( value, 32 );
	}
}

template<class Reader> std::uint32_t ReadScalar( Reader &io, int width ) {
	if ( width ) return io.Read( width );
	if ( io.Read( 1 ) ) return io.Read( 32 );
	return FloatWord( static_cast<float>( static_cast<int>( io.Read( kFloatIntBits ) ) - kFloatIntBias ) );
}

inline bool VectorHas( const ChangeVector &vector, std::size_t field ) noexcept {
	return ( vector[field >> 3] >> ( field & 7 ) ) & 1u;
}

/* Writes the entity number too, as the retail encoder does. */
template<class Writer> bool WriteEntity( Writer &io, const Entity *from,
	const Entity *to, bool force ) {
	if ( !from && !to ) return io.Good();
	const int number = to ? to->number : from->number;
	if ( number < 0 || number >= FNTECH3_STVEF_MAX_GENTITIES ) return io.Fail();
	if ( !to ) {
		io.Write( static_cast<std::uint32_t>( number ), kEntityNumberBits );
		io.Write( 1, 1 );
		return io.Good();
	}
	const Entity empty{};
	if ( !from ) from = &empty;
	ChangeVector vector{};
	bool any = false;
	for ( std::size_t i = 0; i < EntityFields.size(); ++i ) {
		if ( Word( *from, EntityFields[i].offset ) != Word( *to, EntityFields[i].offset ) ) {
			vector[i >> 3] |= static_cast<std::uint8_t>( 1u << ( i & 7 ) );
			any = true;
		}
	}
	if ( !any ) {
		if ( !force ) return io.Good();
		io.Write( static_cast<std::uint32_t>( number ), kEntityNumberBits );
		io.Write( 0, 1 );
		io.Write( 0, 1 );
		return io.Good();
	}
	std::uint32_t symbol = kRawVectorSymbol;
	for ( std::size_t i = 0; i < ChangeVectors.size(); ++i ) {
		if ( ChangeVectors[i] == vector ) { symbol = static_cast<std::uint32_t>( i ); break; }
	}
	io.Write( static_cast<std::uint32_t>( number ), kEntityNumberBits );
	io.Write( 0, 1 );
	io.Write( 1, 1 );
	io.Write( symbol, 5 );
	if ( symbol == kRawVectorSymbol ) {
		std::size_t i = 0;
		for ( ; i + 8 <= EntityFields.size(); i += 8 ) io.Write( vector[i >> 3], 8 );
		if ( EntityFields.size() & 7 ) io.Write( vector[i >> 3], static_cast<int>( EntityFields.size() & 7 ) );
	}
	for ( std::size_t i = 0; i < EntityFields.size(); ++i ) {
		if ( VectorHas( vector, i ) ) WriteScalar( io, Word( *to, EntityFields[i].offset ), EntityFields[i].bits );
	}
	return io.Good();
}

/* The caller has read the entity number. A removal publishes number 1023. */
template<class Reader> bool ReadEntity( Reader &io, const Entity *from, Entity *to, int number ) {
	if ( !to || number < 0 || number >= FNTECH3_STVEF_MAX_GENTITIES ) return io.Fail();
	Entity result{};
	if ( io.Read( 1 ) ) {
		result.number = FNTECH3_STVEF_MAX_GENTITIES - 1;
	} else {
		if ( from ) result = *from;
		result.number = number;
		if ( io.Read( 1 ) ) {
			const std::uint32_t symbol = io.Read( 5 );
			ChangeVector vector{};
			if ( symbol == kRawVectorSymbol ) {
				std::size_t i = 0;
				for ( ; i + 8 < EntityFields.size(); i += 8 )
					vector[i >> 3] = static_cast<std::uint8_t>( io.Read( 8 ) );
				if ( EntityFields.size() & 7 )
					vector[i >> 3] = static_cast<std::uint8_t>( io.Read( static_cast<int>( EntityFields.size() & 7 ) ) );
			} else if ( symbol < ChangeVectors.size() ) {
				vector = ChangeVectors[symbol];
			} else {
				return io.Fail();
			}
			for ( std::size_t i = 0; i < EntityFields.size() && io.Good(); ++i ) {
				if ( VectorHas( vector, i ) )
					SetWord( result, EntityFields[i].offset, ReadScalar( io, EntityFields[i].bits ) );
			}
		}
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

inline std::uint32_t ArrayChanges( const std::int32_t *from, const std::int32_t *to ) noexcept {
	std::uint32_t mask = 0;
	for ( unsigned i = 0; i < 16; ++i ) if ( from[i] != to[i] ) mask |= 1u << i;
	return mask;
}

template<class Writer> void WriteArray( Writer &io, const std::int32_t *to,
	std::uint32_t mask, int width ) {
	io.Write( mask != 0, 1 );
	if ( !mask ) return;
	io.Write( mask, 16 );
	for ( unsigned i = 0; i < 16; ++i )
		if ( mask & ( 1u << i ) ) io.Write( static_cast<std::uint32_t>( to[i] ), width );
}

template<class Reader> void ReadArray( Reader &io, std::int32_t *to, int width ) {
	if ( !io.Read( 1 ) ) return;
	const auto mask = io.Read( 16 );
	for ( unsigned i = 0; i < 16 && io.Good(); ++i ) {
		if ( mask & ( 1u << i ) ) {
			const auto word = io.Read( width );
			std::memcpy( &to[i], &word, 4 );
		}
	}
}

/* Stats, persistant and ammo are 16-bit shorts; powerups are 32-bit. */
template<class Writer> bool WritePlayer( Writer &io, const Player *from, const Player *to ) {
	if ( !to ) return io.Fail();
	const Player empty{};
	if ( !from ) from = &empty;
	for ( const Field &field : PlayerFields ) {
		const auto value = Word( *to, field.offset );
		const bool changed = value != Word( *from, field.offset );
		io.Write( changed, 1 );
		if ( changed ) WriteScalar( io, value, field.bits );
	}
	WriteArray( io, to->stats, ArrayChanges( from->stats, to->stats ), 16 );
	WriteArray( io, to->persistant, ArrayChanges( from->persistant, to->persistant ), 16 );
	WriteArray( io, to->ammo, ArrayChanges( from->ammo, to->ammo ), 16 );
	WriteArray( io, to->powerups, ArrayChanges( from->powerups, to->powerups ), 32 );
	return io.Good();
}

template<class Reader> bool ReadPlayer( Reader &io, const Player *from, Player *to ) {
	if ( !to ) return io.Fail();
	Player result{};
	if ( from ) result = *from;
	for ( std::size_t i = 0; i < PlayerFields.size() && io.Good(); ++i )
		if ( io.Read( 1 ) ) SetWord( result, PlayerFields[i].offset, ReadScalar( io, PlayerFields[i].bits ) );
	ReadArray( io, result.stats, -16 );
	ReadArray( io, result.persistant, -16 );
	ReadArray( io, result.ammo, -16 );
	ReadArray( io, result.powerups, 32 );
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

/* Unkeyed Quake III-style command delta with Elite Force's 8-bit buttons. */
struct CommandField { std::size_t offset; unsigned bytes; int bits; };
inline constexpr std::array CommandFields{
	CommandField{ offsetof( Command, angles[0] ), 4, 16 },
	CommandField{ offsetof( Command, angles[1] ), 4, 16 },
	CommandField{ offsetof( Command, angles[2] ), 4, 16 },
	CommandField{ offsetof( Command, forwardmove ), 1, 8 },
	CommandField{ offsetof( Command, rightmove ), 1, 8 },
	CommandField{ offsetof( Command, upmove ), 1, 8 },
	CommandField{ offsetof( Command, buttons ), 1, 8 },
	CommandField{ offsetof( Command, weapon ), 1, 8 }
};

inline std::uint32_t CommandValue( const Command &cmd, CommandField field ) noexcept {
	const auto *address = reinterpret_cast<const std::uint8_t *>( &cmd ) + field.offset;
	if ( field.bytes == 1 ) return *address;
	return Word( cmd, field.offset );
}

inline void SetCommandValue( Command &cmd, CommandField field, std::uint32_t value ) noexcept {
	auto *address = reinterpret_cast<std::uint8_t *>( &cmd ) + field.offset;
	if ( field.bytes == 1 ) *address = static_cast<std::uint8_t>( value );
	else SetWord( cmd, field.offset, value );
}

template<class Writer> bool WriteCommand( Writer &io, const Command *from, const Command *to ) {
	if ( !to ) return io.Fail();
	const Command empty{};
	if ( !from ) from = &empty;
	const auto time = static_cast<std::uint32_t>( to->serverTime );
	const auto elapsed = time - static_cast<std::uint32_t>( from->serverTime );
	// Unsigned subtraction handles clock wrap; rewinding clocks send the full
	// timestamp instead of truncating a negative delta.
	io.Write( elapsed < 256u, 1 );
	io.Write( elapsed < 256u ? elapsed : time, elapsed < 256u ? 8 : 32 );
	for ( auto field : CommandFields ) {
		const auto value = CommandValue( *to, field );
		const bool different = CommandValue( *from, field ) != value;
		io.Write( different, 1 );
		if ( different ) io.Write( value, field.bits );
	}
	return io.Good();
}

template<class Reader> bool ReadCommand( Reader &io, const Command *from, Command *to ) {
	if ( !to ) return io.Fail();
	Command result{};
	if ( from ) result = *from;
	const auto time = io.Read( 1 ) ? static_cast<std::uint32_t>( result.serverTime ) + io.Read( 8 ) : io.Read( 32 );
	std::memcpy( &result.serverTime, &time, 4 );
	// Retail keeps a movement byte of -128 (MSG_ReadDeltaUsercmd 0x4235e0);
	// Quake III's later clamp to -127 is not part of this protocol.
	for ( auto field : CommandFields ) {
		if ( io.Read( 1 ) ) SetCommandValue( result, field, io.Read( field.bits ) );
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

/* Single player. stvoy.exe talks only to itself over loopback and records no
 * demos, so no retail program ever reads these bodies; they are fnTech3's own.
 * A record is a run of 32-bit words. A body gives the count of words up to the
 * last changed one, a changed bit for each of those, and each changed word in
 * the shortest of five lossless forms: zero, an 8- or 16-bit integer, a whole
 * float from -4096 to 4095, or all 32 bits. An entity starts with its number
 * and a removal bit, as Holomatch's does. A gamestate carries every baseline
 * this way within single player's 16 KB message. */
namespace sp {

using Entity = fntech3_stvef_sp_entityState_t;
using Player = fntech3_stvef_sp_playerState_t;
using Command = fntech3_stvef_sp_usercmd_t;
static_assert( sizeof( Entity ) % 4 == 0 && sizeof( Player ) % 4 == 0 && sizeof( Command ) % 4 == 0 );
static_assert( sizeof( Player ) / 4 < 256, "Word counts fit in a byte" );

inline constexpr int kWholeFloatBits = 13;
inline constexpr int kWholeFloatBias = 4096;

/* A word that is a float holding a whole number from -4096 to 4095, as that
 * number; a negative zero is not one. */
inline bool WholeFloat( std::uint32_t value, int &whole ) noexcept {
	float number;
	std::memcpy( &number, &value, 4 );
	if ( !( number >= -kWholeFloatBias && number < kWholeFloatBias ) ) return false;
	whole = static_cast<int>( number );
	return FloatWord( static_cast<float>( whole ) ) == value && value != 0;
}

template<class Writer> void WriteValue( Writer &io, std::uint32_t value ) {
	const auto signedValue = static_cast<std::int32_t>( value );
	int whole = 0;
	if ( !value ) {
		io.Write( 0, 1 );
	} else if ( signedValue >= -128 && signedValue <= 127 ) {
		io.Write( 1, 2 );
		io.Write( value & 0xffu, 8 );
	} else if ( signedValue >= -32768 && signedValue <= 32767 ) {
		io.Write( 3, 3 );
		io.Write( value & 0xffffu, 16 );
	} else if ( WholeFloat( value, whole ) ) {
		io.Write( 7, 4 );
		io.Write( static_cast<std::uint32_t>( whole + kWholeFloatBias ), kWholeFloatBits );
	} else {
		io.Write( 15, 4 );
		io.Write( value, 32 );
	}
}

template<class Reader> std::uint32_t ReadValue( Reader &io ) {
	if ( !io.Read( 1 ) ) return 0;
	if ( !io.Read( 1 ) ) {
		return static_cast<std::uint32_t>( static_cast<std::int32_t>( static_cast<std::int8_t>( io.Read( 8 ) ) ) );
	}
	if ( !io.Read( 1 ) ) {
		return static_cast<std::uint32_t>( static_cast<std::int32_t>( static_cast<std::int16_t>( io.Read( 16 ) ) ) );
	}
	if ( !io.Read( 1 ) ) {
		return FloatWord( static_cast<float>( static_cast<int>( io.Read( kWholeFloatBits ) ) - kWholeFloatBias ) );
	}
	return io.Read( 32 );
}

template<class Writer, class Record> bool WriteWords( Writer &io, const Record &from, const Record &to ) {
	constexpr std::size_t words = sizeof( Record ) / 4;
	std::size_t count = 0;
	for ( std::size_t word = 0; word < words; ++word ) {
		if ( Word( to, word * 4 ) != Word( from, word * 4 ) ) count = word + 1;
	}
	io.Write( static_cast<std::uint32_t>( count ), 8 );
	for ( std::size_t word = 0; word < count; ++word ) {
		const auto value = Word( to, word * 4 );
		const bool changed = value != Word( from, word * 4 );
		io.Write( changed, 1 );
		if ( changed ) WriteValue( io, value );
	}
	return io.Good();
}

template<class Reader, class Record> bool ReadWords( Reader &io, Record &result ) {
	const std::size_t count = io.Read( 8 );
	if ( count > sizeof( Record ) / 4 ) return io.Fail();
	for ( std::size_t word = 0; word < count && io.Good(); ++word ) {
		if ( io.Read( 1 ) ) SetWord( result, word * 4, ReadValue( io ) );
	}
	return io.Good();
}

template<class Writer> bool WriteEntity( Writer &io, const Entity *from, const Entity *to, bool force ) {
	if ( !from && !to ) return io.Good();
	const int number = to ? to->number : from->number;
	if ( number < 0 || number >= FNTECH3_STVEF_MAX_GENTITIES ) return io.Fail();
	if ( !to ) {
		io.Write( static_cast<std::uint32_t>( number ), kEntityNumberBits );
		io.Write( 1, 1 );
		return io.Good();
	}
	const Entity empty{};
	if ( !from ) from = &empty;
	if ( !force && !std::memcmp( from, to, sizeof( Entity ) ) ) return io.Good();
	io.Write( static_cast<std::uint32_t>( number ), kEntityNumberBits );
	io.Write( 0, 1 );
	return WriteWords( io, *from, *to );
}

/* The caller has read the entity number. A removal publishes number 1023. */
template<class Reader> bool ReadEntity( Reader &io, const Entity *from, Entity *to, int number ) {
	if ( !to || number < 0 || number >= FNTECH3_STVEF_MAX_GENTITIES ) return io.Fail();
	Entity result{};
	if ( io.Read( 1 ) ) {
		result.number = FNTECH3_STVEF_MAX_GENTITIES - 1;
	} else {
		if ( from ) result = *from;
		if ( !ReadWords( io, result ) ) return false;
		result.number = number;
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

template<class Writer> bool WritePlayer( Writer &io, const Player *from, const Player *to ) {
	if ( !to ) return io.Fail();
	const Player empty{};
	return WriteWords( io, from ? *from : empty, *to );
}

template<class Reader> bool ReadPlayer( Reader &io, const Player *from, Player *to ) {
	if ( !to ) return io.Fail();
	Player result{};
	if ( from ) result = *from;
	if ( !ReadWords( io, result ) ) return false;
	*to = result;
	return true;
}

template<class Writer> bool WriteCommand( Writer &io, const Command *from, const Command *to ) {
	if ( !to ) return io.Fail();
	const Command empty{};
	return WriteWords( io, from ? *from : empty, *to );
}

template<class Reader> bool ReadCommand( Reader &io, const Command *from, Command *to ) {
	if ( !to ) return io.Fail();
	Command result{};
	if ( from ) result = *from;
	if ( !ReadWords( io, result ) ) return false;
	*to = result;
	return true;
}

} // namespace sp

} // namespace fntech3::stvef::message
#endif
