/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_ALICE_MESSAGE_CODEC_HPP
#define FNTECH3_ALICE_MESSAGE_CODEC_HPP

#include "alice_game_abi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

/* Independently implemented American McGee's Alice protocol-1 message bodies.
 *
 * Retail alice.exe writes every message as plain LSB-first bits with no
 * Huffman stage (MSG_WriteBits at 0x00445520). What it puts in them is
 * Ritual's, not id's, and every rule below was read out of the engine's own
 * encoder and decoder rather than taken from a relative:
 *
 *   MSG_WriteDeltaEntity      0x00445e10   MSG_ReadDeltaEntity      0x004460c0
 *   MSG_WriteDeltaPlayerstate 0x004464e0   MSG_ReadDeltaPlayerstate 0x004468e0
 *   MSG_WriteDeltaUsercmd     0x00445bb0   MSG_ReadDeltaUsercmd     0x00445ca0
 *
 * An entity delta carries a five-bit index into a thirty-one entry dictionary
 * of changed-field vectors, or thirty-one followed by the raw seventy-nine
 * bit vector; the changed values follow with no per-field flag, and then a
 * thirty-two byte block sent one byte at a time under its own mask. A player
 * delta flags each of its forty-nine fields, then six thirty-two entry arrays
 * under a mask each. A changed float goes as a biased thirteen-bit integer
 * when it is integral and in range, else as thirty-two raw bits, with no zero
 * shortcut. Elite Force Holomatch frames its own bodies the same way, which
 * is a family resemblance and not a source: the dictionary, the field tables
 * and every width here are Alice's own.
 *
 * Storage is the frozen external POD, never engine state. The IO adapter
 * supplies bounded Read/Write, Good and Fail; a read decodes into a temporary
 * and publishes only a complete record. See
 * docs/fntech3/2026-09-22-alice-wire.md. */
namespace fntech3::alice::message {

struct Field {
	std::size_t offset;
	int bits; // zero: binary32; negative: sign-extended integer
};

using Entity = fntech3_alice_entityState_t;
using Player = fntech3_alice_playerState_t;

inline constexpr int kEntityNumberBits = 10;
inline constexpr int kMaxGentities = 1 << kEntityNumberBits;
inline constexpr int kFloatIntBits = 13;
inline constexpr int kFloatIntBias = 1 << ( kFloatIntBits - 1 );

/* The two field tables, in the order the retail engine walks them. Both come
 * from alice_wire_fields.def, which is generated from the engine's own
 * arrays, so neither the order nor a width is chosen here. */
#define FNTECH3_ALICE_ENTITY_FIELD( name, offset, bits ) Field{ offset, bits },
inline constexpr std::array<Field, FNTECH3_ALICE_ENTITY_STATE_FIELDS> EntityFields{ {
#include "alice_wire_fields.def"
} };
#undef FNTECH3_ALICE_ENTITY_FIELD

#define FNTECH3_ALICE_PLAYER_FIELD( name, offset, bits ) Field{ offset, bits },
inline constexpr std::array<Field, FNTECH3_ALICE_PLAYER_STATE_FIELDS> PlayerFields{ {
#include "alice_wire_fields.def"
} };
#undef FNTECH3_ALICE_PLAYER_FIELD

inline constexpr std::size_t kVectorBytes = 10;
using ChangeVector = std::array<std::uint8_t, kVectorBytes>;

#define FNTECH3_ALICE_DELTA_PATTERN( ... ) ChangeVector{ { __VA_ARGS__ } },
inline constexpr std::array<ChangeVector, 31> ChangeVectors{ {
#include "alice_delta_patterns.def"
} };
#undef FNTECH3_ALICE_DELTA_PATTERN
inline constexpr std::uint32_t kRawVectorSymbol = 31;

/* The entity's thirty-two byte block, and the player's six thirty-two entry
 * arrays: each is delta-coded under a mask of its own rather than as fields. */
inline constexpr std::size_t kTaggedBytes = 32;
inline constexpr std::size_t kPlayerBlocks = 6;
inline constexpr std::size_t kPlayerBlockEntries = 32;
inline constexpr int kPlayerBlockBits = 16;

template<class T>
std::uint32_t Word( const T &object, std::size_t offset ) noexcept {
	std::uint32_t value;
	std::memcpy( &value, reinterpret_cast<const std::uint8_t *>( &object ) + offset, 4 );
	return value;
}

template<class T>
void SetWord( T &object, std::size_t offset, std::uint32_t value ) noexcept {
	std::memcpy( reinterpret_cast<std::uint8_t *>( &object ) + offset, &value, 4 );
}

inline std::uint32_t FloatWord( float value ) noexcept {
	std::uint32_t bits;
	std::memcpy( &bits, &value, 4 );
	return bits;
}

/* A changed float is sent as a biased thirteen-bit integer when it is
 * integral and in range, else as thirty-two raw bits; a changed integer is
 * sent at its field width, signed when the width is negative. */
template<class Writer>
void WriteScalar( Writer &io, std::uint32_t value, int width ) {
	if ( width ) {
		io.Write( value, width < 0 ? -width : width );
		return;
	}
	float real;
	std::memcpy( &real, &value, 4 );
	/* Range-check before converting: a NaN, an infinity or an out-of-range
	 * value must never reach an undefined float-to-int cast. */
	if ( real >= -static_cast<float>( kFloatIntBias )
		&& real < static_cast<float>( kFloatIntBias )
		&& static_cast<float>( static_cast<int>( real ) ) == real ) {
		io.Write( 0, 1 );
		io.Write( static_cast<std::uint32_t>(
			static_cast<int>( real ) + kFloatIntBias ), kFloatIntBits );
	} else {
		io.Write( 1, 1 );
		io.Write( value, 32 );
	}
}

template<class Reader>
std::uint32_t ReadScalar( Reader &io, int width ) {
	if ( width ) return io.Read( width );
	if ( io.Read( 1 ) ) return io.Read( 32 );
	return FloatWord( static_cast<float>(
		static_cast<int>( io.Read( kFloatIntBits ) ) - kFloatIntBias ) );
}

inline bool VectorHas( const ChangeVector &vector, std::size_t field ) noexcept {
	return ( vector[field >> 3] >> ( field & 7 ) ) & 1u;
}

inline std::uint32_t TaggedChanges( const Entity &from, const Entity &to ) noexcept {
	std::uint32_t mask = 0;
	for ( std::size_t i = 0; i < kTaggedBytes; ++i ) {
		if ( from.tagged[i] != to.tagged[i] ) mask |= 1u << i;
	}
	return mask;
}

/* Writes the entity number too, as the retail encoder does. */
template<class Writer>
bool WriteEntity( Writer &io, const Entity *from, const Entity *to, bool force ) {
	if ( !from && !to ) return io.Good();
	const int number = to ? to->number : from->number;
	if ( number < 0 || number >= kMaxGentities ) return io.Fail();
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
		if ( Word( *from, EntityFields[i].offset )
			!= Word( *to, EntityFields[i].offset ) ) {
			vector[i >> 3] |= static_cast<std::uint8_t>( 1u << ( i & 7 ) );
			any = true;
		}
	}
	const std::uint32_t tagged = TaggedChanges( *from, *to );

	/* The retail encoder's quiet case needs both to be empty: a block-only
	 * change still sends a delta, with an all-zero field vector. An unchanged
	 * entity then writes nothing at all unless it is forced, and a forced one
	 * only its number, not-removed and no-delta (0x00445ee1..0x00445f2d). */
	if ( !any && !tagged ) {
		if ( !force ) return io.Good();
		io.Write( static_cast<std::uint32_t>( number ), kEntityNumberBits );
		io.Write( 0, 1 );
		io.Write( 0, 1 );
		return io.Good();
	}

	std::uint32_t symbol = kRawVectorSymbol;
	for ( std::size_t i = 0; i < ChangeVectors.size(); ++i ) {
		if ( ChangeVectors[i] == vector ) {
			symbol = static_cast<std::uint32_t>( i );
			break;
		}
	}
	io.Write( static_cast<std::uint32_t>( number ), kEntityNumberBits );
	io.Write( 0, 1 );
	io.Write( 1, 1 );
	io.Write( symbol, 5 );
	if ( symbol == kRawVectorSymbol ) {
		std::size_t i = 0;
		for ( ; i + 8 <= EntityFields.size(); i += 8 ) io.Write( vector[i >> 3], 8 );
		if ( EntityFields.size() & 7 ) {
			io.Write( vector[i >> 3], static_cast<int>( EntityFields.size() & 7 ) );
		}
	}
	for ( std::size_t i = 0; i < EntityFields.size(); ++i ) {
		if ( VectorHas( vector, i ) ) {
			WriteScalar( io, Word( *to, EntityFields[i].offset ), EntityFields[i].bits );
		}
	}
	io.Write( tagged != 0, 1 );
	if ( tagged ) {
		io.Write( tagged, 32 );
		for ( std::size_t i = 0; i < kTaggedBytes; ++i ) {
			if ( tagged & ( 1u << i ) ) io.Write( to->tagged[i], 8 );
		}
	}
	return io.Good();
}

/* The caller has read the entity number. A removal publishes number 1023. */
template<class Reader>
bool ReadEntity( Reader &io, const Entity *from, Entity *to, int number ) {
	if ( !to || number < 0 || number >= kMaxGentities ) return io.Fail();
	Entity result{};
	if ( io.Read( 1 ) ) {
		result.number = kMaxGentities - 1;
	} else {
		if ( from ) result = *from;
		result.number = number;
		if ( io.Read( 1 ) ) {
			const std::uint32_t symbol = io.Read( 5 );
			ChangeVector vector{};
			if ( symbol == kRawVectorSymbol ) {
				std::size_t i = 0;
				for ( ; i + 8 <= EntityFields.size(); i += 8 ) {
					vector[i >> 3] = static_cast<std::uint8_t>( io.Read( 8 ) );
				}
				if ( EntityFields.size() & 7 ) {
					vector[i >> 3] = static_cast<std::uint8_t>(
						io.Read( static_cast<int>( EntityFields.size() & 7 ) ) );
				}
			} else if ( symbol < ChangeVectors.size() ) {
				vector = ChangeVectors[symbol];
			} else {
				return io.Fail();
			}
			for ( std::size_t i = 0; i < EntityFields.size() && io.Good(); ++i ) {
				if ( VectorHas( vector, i ) ) {
					SetWord( result, EntityFields[i].offset,
						ReadScalar( io, EntityFields[i].bits ) );
				}
			}
			if ( io.Good() && io.Read( 1 ) ) {
				const std::uint32_t tagged = io.Read( 32 );
				for ( std::size_t i = 0; i < kTaggedBytes && io.Good(); ++i ) {
					if ( tagged & ( 1u << i ) ) {
						result.tagged[i] = static_cast<std::uint8_t>( io.Read( 8 ) );
					}
				}
			}
		}
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

/* The entity a gamestate's baselines are delta-coded from. Quake III's is all
 * zero; Alice's is not. Retail alice.exe fills it at 0x00446460 -- clear the
 * 512 bytes, then constantLight 0x00ffffff (+0x90), parent 1023 (+0xa0),
 * scale and alpha 1.0 (+0x1b8, +0x1bc), renderfx 0x10 (+0x1c0) and each of
 * the five bone modifiers' tag -1 (+0xd8, stride 36) -- and all three of its
 * callers delta from it: the server's gamestate (0x00452b6f), the client's
 * gamestate parse (0x0042e0ab) and the demo recorder's gamestate (0x0042a60b).
 * A field equal to this record is not sent, so a writer and a reader that
 * disagree about it disagree about every such field. */
inline void InitNullEntity( Entity &entity ) noexcept {
	entity = Entity{};
	entity.constantLight = 0x00ffffff;
	entity.parent = kMaxGentities - 1;
	entity.scale = 1.0f;
	entity.alpha = 1.0f;
	entity.renderfx = 0x10;
	for ( auto &bonemod : entity.bonemod ) bonemod.tag = -1;
}

inline std::uint32_t BlockChanges( const std::int32_t *from,
	const std::int32_t *to ) noexcept {
	std::uint32_t mask = 0;
	for ( std::size_t i = 0; i < kPlayerBlockEntries; ++i ) {
		if ( from[i] != to[i] ) mask |= 1u << i;
	}
	return mask;
}

template<class Writer>
void WriteBlock( Writer &io, const std::int32_t *to, std::uint32_t mask ) {
	io.Write( mask != 0, 1 );
	if ( !mask ) return;
	io.Write( mask, 32 );
	for ( std::size_t i = 0; i < kPlayerBlockEntries; ++i ) {
		if ( mask & ( 1u << i ) ) {
			io.Write( static_cast<std::uint32_t>( to[i] ), kPlayerBlockBits );
		}
	}
}

/* Each value is read as a signed short: MSG_ReadDeltaPlayerstate takes them
 * through MSG_ReadShort (0x00446aa9 -> 0x00445900), which sign-extends its
 * sixteen bits (`movsx eax, ax` at 0x00445920). A -1 the game sends -- no
 * weapon in the hand, for one -- arrives as -1, not 65,535. */
template<class Reader>
void ReadBlock( Reader &io, std::int32_t *to ) {
	if ( !io.Read( 1 ) ) return;
	const std::uint32_t mask = io.Read( 32 );
	for ( std::size_t i = 0; i < kPlayerBlockEntries && io.Good(); ++i ) {
		if ( mask & ( 1u << i ) ) {
			to[i] = static_cast<std::int16_t>( io.Read( kPlayerBlockBits ) & 0xffffu );
		}
	}
}

template<class Writer>
bool WritePlayer( Writer &io, const Player *from, const Player *to ) {
	if ( !to ) return io.Fail();
	const Player empty{};
	if ( !from ) from = &empty;
	for ( const Field &field : PlayerFields ) {
		const std::uint32_t value = Word( *to, field.offset );
		const bool changed = value != Word( *from, field.offset );
		io.Write( changed, 1 );
		if ( changed ) WriteScalar( io, value, field.bits );
	}
	for ( std::size_t i = 0; i < kPlayerBlocks; ++i ) {
		WriteBlock( io, to->block[i], BlockChanges( from->block[i], to->block[i] ) );
	}
	return io.Good();
}

template<class Reader>
bool ReadPlayer( Reader &io, const Player *from, Player *to ) {
	if ( !to ) return io.Fail();
	Player result{};
	if ( from ) result = *from;
	for ( const Field &field : PlayerFields ) {
		if ( !io.Good() ) return false;
		if ( io.Read( 1 ) ) {
			SetWord( result, field.offset, ReadScalar( io, field.bits ) );
		}
	}
	for ( std::size_t i = 0; i < kPlayerBlocks && io.Good(); ++i ) {
		ReadBlock( io, result.block[i] );
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

/* The user command. It carries no change vector: the time is sent either as
 * an eight-bit step from the previous command or in full, and every other
 * member is flagged one bit at a time in the order the retail writer takes
 * them -- the three view angles, the three move bytes, then the buttons and
 * the byte after them. */
using Command = fntech3_alice_usercmd_t;

template<class Writer>
void WriteKey( Writer &io, std::int32_t from, std::int32_t to, int bits ) {
	if ( from == to ) {
		io.Write( 0, 1 );
		return;
	}
	io.Write( 1, 1 );
	io.Write( static_cast<std::uint32_t>( to ), bits );
}

template<class Reader>
std::int32_t ReadKey( Reader &io, std::int32_t from, int bits ) {
	if ( !io.Read( 1 ) ) return from;
	return static_cast<std::int32_t>( io.Read( bits ) );
}

template<class Writer>
bool WriteCommand( Writer &io, const Command *from, const Command *to ) {
	if ( !to ) return io.Fail();
	const Command empty{};
	if ( !from ) from = &empty;
	const std::int32_t step = to->serverTime - from->serverTime;
	if ( step >= 0 && step < 256 ) {
		io.Write( 1, 1 );
		io.Write( static_cast<std::uint32_t>( step ), 8 );
	} else {
		io.Write( 0, 1 );
		io.Write( static_cast<std::uint32_t>( to->serverTime ), 32 );
	}
	for ( int i = 0; i < 3; ++i ) WriteKey( io, from->angles[i], to->angles[i], 16 );
	for ( int i = 0; i < 3; ++i ) WriteKey( io, from->move[i], to->move[i], 8 );
	WriteKey( io, from->buttons, to->buttons, 8 );
	WriteKey( io, from->unnamed_6, to->unnamed_6, 8 );
	return io.Good();
}

template<class Reader>
bool ReadCommand( Reader &io, const Command *from, Command *to ) {
	if ( !to ) return io.Fail();
	const Command empty{};
	if ( !from ) from = &empty;
	Command result = *from;
	if ( io.Read( 1 ) ) {
		result.serverTime = from->serverTime + static_cast<std::int32_t>( io.Read( 8 ) );
	} else {
		result.serverTime = static_cast<std::int32_t>( io.Read( 32 ) );
	}
	for ( int i = 0; i < 3; ++i ) {
		result.angles[i] = static_cast<std::int16_t>( ReadKey( io, from->angles[i], 16 ) );
	}
	for ( int i = 0; i < 3; ++i ) {
		result.move[i] = static_cast<std::int8_t>( ReadKey( io, from->move[i], 8 ) );
	}
	result.buttons = static_cast<std::uint8_t>( ReadKey( io, from->buttons, 8 ) );
	result.unnamed_6 = static_cast<std::uint8_t>( ReadKey( io, from->unnamed_6, 8 ) );
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

/* The sound block a snapshot carries after its packet entities (written at
 * 0x00446d00, read at 0x00446e90): a presence bit, a six-bit count, and for
 * each sound either a stop -- one bit, the entity in ten bits and the channel
 * in six -- or a start: one bit, an origin flag and the origin only when it
 * is not all zero, the entity and the channel, the sound index in eight bits,
 * and the volume and the minimum distance each behind a flag, sent only when
 * it is not -1. Floats go as their thirty-two raw bits.
 *
 * `Sound` is any record with the members origin[3], entityNumber, channel,
 * soundIndex, volume, minDist, stop and hasOrigin; the engine's and the
 * tests' both qualify. A snapshot holds at most `kMaxSnapshotSounds`. */
inline constexpr int kMaxSnapshotSounds = 32;

template<class Writer, class Sound>
bool WriteSounds( Writer &io, const Sound *sounds, int count ) {
	if ( count < 0 || count > kMaxSnapshotSounds || ( count && !sounds ) ) return io.Fail();
	io.Write( count > 0 ? 1u : 0u, 1 );
	if ( !count ) return io.Good();
	io.Write( static_cast<std::uint32_t>( count ), 6 );
	for ( int i = 0; i < count; ++i ) {
		const Sound &sound = sounds[i];
		if ( sound.stop ) {
			io.Write( 1, 1 );
			io.Write( static_cast<std::uint32_t>( sound.entityNumber ), 10 );
			io.Write( static_cast<std::uint32_t>( sound.channel ), 6 );
			continue;
		}
		io.Write( 0, 1 );
		const bool origin = sound.origin[0] != 0.0f || sound.origin[1] != 0.0f
			|| sound.origin[2] != 0.0f;
		io.Write( origin ? 1u : 0u, 1 );
		if ( origin ) {
			for ( int k = 0; k < 3; ++k ) io.Write( FloatWord( sound.origin[k] ), 32 );
		}
		io.Write( static_cast<std::uint32_t>( sound.entityNumber ), 10 );
		io.Write( static_cast<std::uint32_t>( sound.channel ), 6 );
		io.Write( static_cast<std::uint32_t>( sound.soundIndex ), 8 );
		io.Write( sound.volume != -1.0f ? 1u : 0u, 1 );
		if ( sound.volume != -1.0f ) io.Write( FloatWord( sound.volume ), 32 );
		io.Write( sound.minDist != -1.0f ? 1u : 0u, 1 );
		if ( sound.minDist != -1.0f ) io.Write( FloatWord( sound.minDist ), 32 );
	}
	return io.Good();
}

template<class Reader, class Sound>
bool ReadSounds( Reader &io, Sound *sounds, int &count ) {
	count = 0;
	if ( !sounds ) return io.Fail();
	if ( !io.Read( 1 ) ) return io.Good();
	const int total = static_cast<int>( io.Read( 6 ) );
	/* Six bits reach 63; the retail snapshot holds 32. */
	if ( total > kMaxSnapshotSounds ) return io.Fail();
	const auto readFloat = [&io]() {
		const std::uint32_t bits = io.Read( 32 );
		float value;
		std::memcpy( &value, &bits, 4 );
		return value;
	};
	for ( int i = 0; i < total; ++i ) {
		Sound sound{};
		if ( io.Read( 1 ) ) {
			sound.stop = static_cast<decltype( sound.stop )>( 1 );
			sound.entityNumber = static_cast<int>( io.Read( 10 ) );
			sound.channel = static_cast<int>( io.Read( 6 ) );
		} else {
			if ( io.Read( 1 ) ) {
				sound.hasOrigin = static_cast<decltype( sound.hasOrigin )>( 1 );
				for ( int k = 0; k < 3; ++k ) sound.origin[k] = readFloat();
			}
			sound.entityNumber = static_cast<int>( io.Read( 10 ) );
			sound.channel = static_cast<int>( io.Read( 6 ) );
			sound.soundIndex = static_cast<int>( io.Read( 8 ) );
			sound.volume = io.Read( 1 ) ? readFloat() : -1.0f;
			sound.minDist = io.Read( 1 ) ? readFloat() : -1.0f;
		}
		if ( !io.Good() ) return false;
		sounds[i] = sound;
	}
	count = total;
	return io.Good();
}

} // namespace fntech3::alice::message

#endif
