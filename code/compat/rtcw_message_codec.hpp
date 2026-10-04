/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_MESSAGE_CODEC_HPP
#define FNTECH3_RTCW_MESSAGE_CODEC_HPP

#include "rtcw_game_abi.h"
#include "ieee_delta_float.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

/* Independently implemented RTCW 49/60 message bodies. Field order, widths
 * and the SP change-vector dictionary are protocol facts observed in the
 * published engines. See docs/fntech3/2026-09-14-rtcw-contracts.md.
 *
 * Storage is the frozen external POD, never the engine's Q3/QL state. The IO
 * adapter supplies bounded Read/Write, Good and Fail operations; all reads
 * decode into a temporary and publish only a complete, valid record. */
namespace fntech3::rtcw::message {

enum class Mode { SinglePlayer, Multiplayer };

struct Field {
	std::size_t offset;
	int bits; // zero: binary32; negative: sign-extended integer
};

using Entity = fntech3_rtcw_entityState_t;
using SPPlayer = fntech3_rtcw_sp_playerState_t;
using MPPlayer = fntech3_rtcw_mp_playerState_t;
using PlayerPrefix = fntech3_rtcw_playerStatePrefix_t;
using SPCommand = fntech3_rtcw_sp_usercmd_t;
using MPCommand = fntech3_rtcw_mp_usercmd_t;

constexpr auto EntityFields( Mode mode ) noexcept {
#define E(member, width) Field{ offsetof( Entity, member ), width }
	return std::array{
		E(eType, 8), E(eFlags, mode == Mode::SinglePlayer ? 32 : 24),
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
		E(loopSound, 8), E(constantLight, 32), E(dl_intensity, 32),
		E(modelindex, 9), E(modelindex2, 9), E(frame, 16), E(clientNum, 8),
		E(solid, 24), E(event, 10), E(eventParm, 8), E(eventSequence, 8),
		E(events[0], 8), E(events[1], 8), E(events[2], 8), E(events[3], 8),
		E(eventParms[0], 8), E(eventParms[1], 8),
		E(eventParms[2], 8), E(eventParms[3], 8),
		E(powerups, 16), E(weapon, 8), E(legsAnim, 10), E(torsoAnim, 10),
		E(density, 10), E(dmgFlags, 32), E(onFireStart, 32), E(onFireEnd, 32),
		E(aiChar, 8), E(teamNum, 8), E(effect1Time, 32), E(effect2Time, 32),
		E(effect3Time, 32), E(aiState, 2),
		E(animMovetype, mode == Mode::SinglePlayer ? 6 : 4)
	};
#undef E
}

inline constexpr auto SPEntityFields = EntityFields( Mode::SinglePlayer );
inline constexpr auto MPEntityFields = EntityFields( Mode::Multiplayer );
static_assert( SPEntityFields.size() == 71 );

#define P(member, width) Field{ offsetof( SPPlayer, common.member ), width }
inline constexpr std::array SPPlayerFields{
	P(commandTime, 32), P(pm_type, 8), P(bobCycle, 8), P(pm_flags, 16), P(pm_time, -16),
	P(origin[0], 0), P(origin[1], 0), P(origin[2], 0),
	P(velocity[0], 0), P(velocity[1], 0), P(velocity[2], 0),
	P(weaponTime, -16), P(weaponDelay, -16), P(grenadeTimeLeft, -16),
	P(gravity, 16), P(leanf, 0), P(speed, 16),
	P(delta_angles[0], 16), P(delta_angles[1], 16), P(delta_angles[2], 16),
	P(groundEntityNum, 10), P(legsTimer, 16), P(torsoTimer, 16),
	P(legsAnim, 10), P(torsoAnim, 10), P(movementDir, 8), P(eFlags, 16),
	P(eventSequence, 8), P(events[0], 8), P(events[1], 8), P(events[2], 8), P(events[3], 8),
	P(eventParms[0], 8), P(eventParms[1], 8), P(eventParms[2], 8), P(eventParms[3], 8),
	P(clientNum, 8), P(weapons[0], 32), P(weapons[1], 32), P(weapon, 7), P(weaponstate, 4),
	P(viewangles[0], 0), P(viewangles[1], 0), P(viewangles[2], 0), P(viewheight, -8),
	P(damageEvent, 8), P(damageYaw, 8), P(damagePitch, 8), P(damageCount, 8),
	P(mins[0], 0), P(mins[1], 0), P(mins[2], 0),
	P(maxs[0], 0), P(maxs[1], 0), P(maxs[2], 0), P(crouchMaxZ, 0),
	P(crouchViewHeight, 0), P(standViewHeight, 0), P(deadViewHeight, 0),
	P(runSpeedScale, 0), P(sprintSpeedScale, 0), P(crouchSpeedScale, 0), P(friction, 0),
	P(viewlocked, 8), P(viewlocked_entNum, 16), P(aiChar, 8), P(teamNum, 8),
	P(gunfx, 8), P(onFireStart, 32), P(curWeapHeat, 8), P(sprintTime, 16),
	P(aimSpreadScale, 8), Field{ offsetof( SPPlayer, aiState ), 2 },
	P(serverCursorHint, 8), P(serverCursorHintVal, 8),
	Field{ offsetof( SPPlayer, footstepCount ), 0 }
};
#undef P
static_assert( SPPlayerFields.size() == 76 );

constexpr auto MakeMPPlayerFields() noexcept {
	std::array<Field, 77> result{};
	std::size_t index = 0;
	for ( Field field : SPPlayerFields ) {
		if ( field.offset == offsetof( PlayerPrefix, viewangles ) )
			result[index++] = { offsetof( PlayerPrefix, weapAnim ), 10 };
		if ( field.offset == offsetof( PlayerPrefix, eFlags ) ) field.bits = 24;
		if ( field.offset == offsetof( SPPlayer, aiState ) )
			field.offset = offsetof( MPPlayer, aiState );
		if ( field.offset == offsetof( SPPlayer, footstepCount ) )
			field = { offsetof( PlayerPrefix, classWeaponTime ), 32 };
		result[index++] = field;
	}
	return result;
}
inline constexpr auto MPPlayerFields = MakeMPPlayerFields();

/* Protocol 49 dictionary: bit i identifies entity field i. The highest
 * selected field in these 32 wire symbols is below 64; raw vectors carry all
 * 71 fields. The reference encoder searches symbols 0..30, but its decoder
 * accepts symbol 31 too. Preserve that asymmetric contract. */
inline constexpr std::array<std::uint64_t, 32> SPChangeMasks{
	0x0000000000000008ULL, 0x0000040000000000ULL,
	0x1000008001c000e1ULL, 0x0000402001c000e1ULL,
	0x0000000000000028ULL, 0x0000110000000000ULL,
	0x0000118000000001ULL, 0x0000118000000003ULL,
	0x100000038ff000e3ULL, 0x0000000389f000e1ULL,
	0x2000110021c080e1ULL, 0x0000110000000003ULL,
	0x2000110001c030e9ULL, 0x0000011021c080e1ULL,
	0x0000011001c000e1ULL, 0x00000800000000e3ULL,
	0x0000400001c000e1ULL, 0x0000000001c000e0ULL,
	0x00000103a1c080e1ULL, 0x0000110000000011ULL,
	0x0000200000000000ULL, 0x0000004001c000e0ULL,
	0x0000200000000028ULL, 0x10000403c1c000e3ULL,
	0x0000010021c080e1ULL, 0x2000110000001019ULL,
	0x0000000000000068ULL, 0x10000403c1c000e1ULL,
	0x0000051021c080e1ULL, 0x1000402001c000e1ULL,
	0x0000110021c080e1ULL, 0x0000a00000000028ULL
};

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

template<class Writer> void WriteScalar( Writer &io, std::uint32_t value,
	int width, bool entity ) {
	if ( entity ) {
		const bool nonzero = width ? value != 0 : ( value & 0x7fffffffu ) != 0;
		io.Write( nonzero, 1 );
		if ( !nonzero ) return;
	}
	if ( width ) {
		io.Write( value, width );
		return;
	}
	int integer;
	if ( delta::CompactFloatInteger( value, integer ) ) {
		io.Write( 0, 1 );
		io.Write( static_cast<std::uint32_t>( integer + 4096 ), 13 );
	} else {
		io.Write( 1, 1 );
		io.Write( value, 32 );
	}
}

template<class Reader> std::uint32_t ReadScalar( Reader &io, int width, bool entity ) {
	if ( entity && !io.Read( 1 ) ) return 0;
	if ( width ) return io.Read( width );
	if ( io.Read( 1 ) ) return io.Read( 32 );
	return FloatWord( static_cast<float>( static_cast<int>( io.Read( 13 ) ) - 4096 ) );
}

template<class Writer> bool WriteEntity( Writer &io, Mode mode,
	const Entity *from, const Entity *to, bool force ) {
	if ( mode != Mode::SinglePlayer && mode != Mode::Multiplayer ) return io.Fail();
	if ( !from && !to ) return io.Good();
	const int number = to ? to->number : from->number;
	if ( number < 0 || number >= 1024 ) return io.Fail();
	if ( !to ) {
		io.Write( static_cast<std::uint32_t>( number ), 10 );
		io.Write( 1, 1 );
		return io.Good();
	}
	const Entity empty{};
	if ( !from ) from = &empty;
	const auto fields = EntityFields( mode );
	std::array<bool, 71> changed{};
	std::size_t last = 0;
	std::uint64_t mask = 0;
	bool highFields = false;
	for ( std::size_t i = 0; i < fields.size(); ++i ) {
		changed[i] = Word( *from, fields[i].offset ) != Word( *to, fields[i].offset );
		if ( changed[i] ) {
			last = i + 1;
			if ( i < 64 ) mask |= std::uint64_t{1} << i;
			else highFields = true;
		}
	}
	if ( !last && !force ) return io.Good();
	io.Write( static_cast<std::uint32_t>( number ), 10 );
	io.Write( 0, 1 );
	io.Write( last != 0, 1 );
	if ( !last ) return io.Good();
	if ( mode == Mode::Multiplayer ) {
		io.Write( static_cast<std::uint32_t>( last ), 8 );
	} else {
		std::size_t symbol = 31;
		if ( !highFields ) {
			for ( std::size_t i = 0; i < 31; ++i )
				if ( mask == SPChangeMasks[i] ) { symbol = i; break; }
		}
		io.Write( symbol == 31, 1 );
		if ( symbol != 31 ) io.Write( static_cast<std::uint32_t>( symbol ), 5 );
		else {
			for ( std::size_t first = 0; first < fields.size(); first += 8 ) {
				std::uint32_t byte = 0;
				const auto count = fields.size() - first < 8 ? fields.size() - first : 8;
				for ( std::size_t j = 0; j < count; ++j ) byte |= changed[first + j] << j;
				io.Write( byte, static_cast<int>( count ) );
			}
		}
	}
	for ( std::size_t i = 0; i < last; ++i ) {
		if ( mode == Mode::Multiplayer ) io.Write( changed[i], 1 );
		if ( changed[i] ) WriteScalar( io, Word( *to, fields[i].offset ), fields[i].bits, true );
	}
	return io.Good();
}

template<class Reader> bool ReadEntity( Reader &io, Mode mode,
	const Entity *from, Entity *to, int number ) {
	if ( !to || number < 0 || number >= 1024 ||
		( mode != Mode::SinglePlayer && mode != Mode::Multiplayer ) ) return io.Fail();
	Entity result{};
	if ( io.Read( 1 ) ) {
		result.number = 1023;
	} else {
		if ( from ) result = *from;
		result.number = number;
		if ( io.Read( 1 ) ) {
			const auto fields = EntityFields( mode );
			std::array<bool, 71> changed{};
			std::size_t last = fields.size();
			if ( mode == Mode::Multiplayer ) {
				last = io.Read( 8 );
				// Never reproduce the original decoder's silent count clamp.
				if ( last > fields.size() ) return io.Fail();
			} else if ( io.Read( 1 ) ) {
				for ( std::size_t first = 0; first < fields.size(); first += 8 ) {
					const auto count = fields.size() - first < 8 ? fields.size() - first : 8;
					const auto byte = io.Read( static_cast<int>( count ) );
					for ( std::size_t j = 0; j < count; ++j ) changed[first + j] = ( byte >> j ) & 1u;
				}
			} else {
				const auto symbol = io.Read( 5 );
				if ( !io.Good() || symbol >= SPChangeMasks.size() ) return io.Fail();
				for ( std::size_t i = 0; i < 64; ++i ) changed[i] = ( SPChangeMasks[symbol] >> i ) & 1u;
			}
			for ( std::size_t i = 0; i < last && io.Good(); ++i ) {
				if ( mode == Mode::Multiplayer ) changed[i] = io.Read( 1 ) != 0;
				if ( changed[i] ) SetWord( result, fields[i].offset, ReadScalar( io, fields[i].bits, true ) );
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
	std::uint32_t mask, int width = 16 ) {
	io.Write( mask != 0, 1 );
	if ( !mask ) return;
	io.Write( mask, 16 );
	for ( unsigned i = 0; i < 16; ++i )
		if ( mask & ( 1u << i ) ) io.Write( static_cast<std::uint32_t>( to[i] ), width );
}

template<class Reader> void ReadArray( Reader &io, std::int32_t *to, int width = -16 ) {
	if ( !io.Read( 1 ) ) return;
	const auto mask = io.Read( 16 );
	for ( unsigned i = 0; i < 16 && io.Good(); ++i ) {
		if ( mask & ( 1u << i ) ) {
			const auto word = io.Read( width );
			std::memcpy( &to[i], &word, 4 );
		}
	}
}

template<class Player> constexpr auto PlayerFields() noexcept {
	static_assert( std::is_same_v<Player, SPPlayer> || std::is_same_v<Player, MPPlayer> );
	if constexpr ( std::is_same_v<Player, SPPlayer> ) return SPPlayerFields;
	else return MPPlayerFields;
}

template<class Writer, class Player> bool WritePlayer( Writer &io,
	const Player *from, const Player *to ) {
	if ( !to ) return io.Fail();
	const Player empty{};
	if ( !from ) from = &empty;
	constexpr auto fields = PlayerFields<Player>();
	std::size_t last = fields.size();
	if constexpr ( std::is_same_v<Player, MPPlayer> ) {
		while ( last && Word( *from, fields[last - 1].offset ) == Word( *to, fields[last - 1].offset ) ) --last;
		io.Write( static_cast<std::uint32_t>( last ), 8 );
	}
	for ( std::size_t i = 0; i < last; ++i ) {
		const auto value = Word( *to, fields[i].offset );
		const bool changed = value != Word( *from, fields[i].offset );
		io.Write( changed, 1 );
		if ( changed ) WriteScalar( io, value, fields[i].bits, false );
	}
	const auto &old = from->common;
	const auto &next = to->common;
	const std::uint32_t arrays[]{ ArrayChanges( old.stats, next.stats ),
		ArrayChanges( old.persistant, next.persistant ),
		ArrayChanges( old.holdable, next.holdable ), ArrayChanges( old.powerups, next.powerups ) };
	const bool anyArray = ( arrays[0] | arrays[1] | arrays[2] | arrays[3] ) != 0;
	io.Write( anyArray, 1 );
	if ( anyArray ) {
		WriteArray( io, next.stats, arrays[0] );
		WriteArray( io, next.persistant, arrays[1] );
		WriteArray( io, next.holdable, arrays[2] );
		WriteArray( io, next.powerups, arrays[3], 32 );
	}
	std::array<std::uint32_t, 4> ammo{};
	for ( unsigned group = 0; group < 4; ++group )
		ammo[group] = ArrayChanges( old.ammo + group * 16, next.ammo + group * 16 );
	const bool anyAmmo = ( ammo[0] | ammo[1] | ammo[2] | ammo[3] ) != 0;
	io.Write( anyAmmo, 1 );
	if ( anyAmmo ) for ( unsigned group = 0; group < 4; ++group )
		WriteArray( io, next.ammo + group * 16, ammo[group] );
	for ( unsigned group = 0; group < 4; ++group )
		WriteArray( io, next.ammoclip + group * 16,
			ArrayChanges( old.ammoclip + group * 16, next.ammoclip + group * 16 ) );
	return io.Good();
}

template<class Reader, class Player> bool ReadPlayer( Reader &io,
	const Player *from, Player *to ) {
	if ( !to ) return io.Fail();
	Player result{};
	if ( from ) result = *from;
	constexpr auto fields = PlayerFields<Player>();
	std::size_t last = fields.size();
	if constexpr ( std::is_same_v<Player, MPPlayer> ) last = io.Read( 8 );
	if ( last > fields.size() ) return io.Fail();
	for ( std::size_t i = 0; i < last && io.Good(); ++i )
		if ( io.Read( 1 ) ) SetWord( result, fields[i].offset, ReadScalar( io, fields[i].bits, false ) );
	if ( io.Read( 1 ) ) {
		ReadArray( io, result.common.stats );
		ReadArray( io, result.common.persistant );
		ReadArray( io, result.common.holdable );
		ReadArray( io, result.common.powerups, 32 );
	}
	if ( io.Read( 1 ) ) for ( unsigned group = 0; group < 4; ++group )
		ReadArray( io, result.common.ammo + group * 16 );
	for ( unsigned group = 0; group < 4; ++group )
		ReadArray( io, result.common.ammoclip + group * 16 );
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

struct CommandField { std::size_t offset; unsigned bytes; int bits; };

template<class Command> constexpr auto CommandFields() noexcept {
	static_assert( std::is_same_v<Command, SPCommand> || std::is_same_v<Command, MPCommand> );
#define C(member, size, width) CommandField{ offsetof( Command, common.member ), size, width }
	constexpr std::array common{ C(angles[0], 4, 16), C(angles[1], 4, 16), C(angles[2], 4, 16),
		C(forwardmove, 1, 8), C(rightmove, 1, 8), C(upmove, 1, 8),
		C(buttons, 1, 8), C(wbuttons, 1, 8), C(weapon, 1, 8),
		C(holdable, 1, 8), C(wolfkick, 1, 8) };
#undef C
	std::array<CommandField, std::is_same_v<Command, SPCommand> ? 12 : 13> result{};
	for ( std::size_t i = 0; i < common.size(); ++i ) result[i] = common[i];
	if constexpr ( std::is_same_v<Command, SPCommand> ) result[11] = { offsetof( Command, cld ), 2, 16 };
	else {
		result[11] = { offsetof( Command, mpSetup ), 1, 8 };
		result[12] = { offsetof( Command, identClient ), 1, 8 };
	}
	return result;
}

template<class Command> std::uint32_t CommandValue( const Command &cmd, CommandField field ) noexcept {
	const auto *address = reinterpret_cast<const std::uint8_t *>( &cmd ) + field.offset;
	if ( field.bytes == 1 ) return *address;
	if ( field.bytes == 2 ) { std::uint16_t value; std::memcpy( &value, address, 2 ); return value; }
	return Word( cmd, field.offset );
}

template<class Command> void SetCommandValue( Command &cmd, CommandField field, std::uint32_t value ) noexcept {
	auto *address = reinterpret_cast<std::uint8_t *>( &cmd ) + field.offset;
	if ( field.bytes == 1 ) *address = static_cast<std::uint8_t>( value );
	else if ( field.bytes == 2 ) {
		const auto half = static_cast<std::uint16_t>( value );
		std::memcpy( address, &half, 2 );
	} else SetWord( cmd, field.offset, value );
}

template<class Writer, class Command> bool WriteCommand( Writer &io, std::uint32_t key,
	const Command *from, const Command *to ) {
	if ( !to ) return io.Fail();
	const Command empty{};
	if ( !from ) from = &empty;
	const auto time = static_cast<std::uint32_t>( to->common.serverTime );
	const auto elapsed = time - static_cast<std::uint32_t>( from->common.serverTime );
	// Unsigned subtraction handles clock wrap without undefined arithmetic.
	// Rewinding clocks use the full timestamp instead of truncating a negative delta.
	io.Write( elapsed < 256u, 1 );
	io.Write( elapsed < 256u ? elapsed : time, elapsed < 256u ? 8 : 32 );
	constexpr auto fields = CommandFields<Command>();
	bool changed = false;
	for ( auto field : fields ) changed |= CommandValue( *from, field ) != CommandValue( *to, field );
	io.Write( changed, 1 );
	if ( changed ) {
		key ^= time;
		for ( auto field : fields ) {
			const auto value = CommandValue( *to, field );
			const bool different = CommandValue( *from, field ) != value;
			io.Write( different, 1 );
			if ( different ) io.Write( value ^ key, field.bits );
		}
	}
	return io.Good();
}

template<class Reader, class Command> bool ReadCommand( Reader &io, std::uint32_t key,
	const Command *from, Command *to ) {
	if ( !to ) return io.Fail();
	Command result{};
	if ( from ) result = *from;
	const auto time = io.Read( 1 ) ? static_cast<std::uint32_t>( result.common.serverTime ) + io.Read( 8 ) : io.Read( 32 );
	std::memcpy( &result.common.serverTime, &time, 4 );
	if ( io.Read( 1 ) ) {
		key ^= time;
		for ( auto field : CommandFields<Command>() ) {
			if ( io.Read( 1 ) ) {
				const auto mask = ( 1u << field.bits ) - 1u;
				SetCommandValue( result, field, io.Read( field.bits ) ^ ( key & mask ) );
			}
		}
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

} // namespace fntech3::rtcw::message
#endif
