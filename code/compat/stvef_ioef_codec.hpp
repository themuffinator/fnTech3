/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_IOEF_CODEC_HPP
#define FNTECH3_STVEF_IOEF_CODEC_HPP

#include "stvef_message_codec.hpp"

/* Independently implemented ioEF/Lilium Voyager protocol-26 message bodies.
 * The frozen records, field order and widths match Holomatch, but the framing
 * does not: Huffman-coded messages carry last-changed field counts, entity
 * zero shortcuts, an aggregate player-array flag and unkeyed user commands.
 * Reference: Lilium Voyager 0f7dcd89, code/qcommon/msg.c. See the protocol-26
 * evidence record for reference comparisons and live interoperability.
 * The bounded message adapter owns Huffman coding; failed reads publish no
 * partially decoded state. Retail protocol 24 keeps its own codec. */
namespace fntech3::stvef::message::ioef {

template<class Record, std::size_t Count>
std::size_t LastChanged( const Record &from, const Record &to,
	const std::array<Field, Count> &fields ) noexcept {
	for ( std::size_t count = Count; count; --count )
		if ( Word( from, fields[count - 1].offset ) != Word( to, fields[count - 1].offset ) ) return count;
	return 0;
}

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
	const auto count = LastChanged( *from, *to, EntityFields );
	if ( !count && !force ) return io.Good();
	io.Write( static_cast<std::uint32_t>( number ), kEntityNumberBits );
	io.Write( 0, 1 );
	io.Write( count != 0, 1 );
	if ( count ) io.Write( static_cast<std::uint32_t>( count ), 8 );
	for ( std::size_t i = 0; i < count; ++i ) {
		const auto field = EntityFields[i];
		const auto value = Word( *to, field.offset );
		const bool changed = value != Word( *from, field.offset );
		io.Write( changed, 1 );
		if ( !changed ) continue;
		// Both floating-point zeros use the zero shortcut; integer fields
		// compare the entire word before truncating to their wire width.
		const bool nonzero = ( field.bits ? value : value & 0x7fffffffu ) != 0;
		io.Write( nonzero, 1 );
		if ( nonzero ) WriteScalar( io, value, field.bits );
	}
	return io.Good();
}

template<class Reader> bool ReadEntity( Reader &io, const Entity *from, Entity *to, int number ) {
	if ( !to || number < 0 || number >= FNTECH3_STVEF_MAX_GENTITIES ) return io.Fail();
	Entity result{};
	if ( io.Read( 1 ) ) {
		result.number = FNTECH3_STVEF_MAX_GENTITIES - 1;
	} else {
		if ( from ) result = *from;
		result.number = number;
		if ( io.Read( 1 ) ) {
			const auto count = io.Read( 8 );
			if ( count > EntityFields.size() ) return io.Fail();
			for ( std::size_t i = 0; i < count && io.Good(); ++i ) {
				if ( io.Read( 1 ) ) SetWord( result, EntityFields[i].offset,
					io.Read( 1 ) ? ReadScalar( io, EntityFields[i].bits ) : 0u );
			}
		}
	}
	if ( !io.Good() ) return false;
	*to = result;
	return true;
}

template<class Writer> bool WritePlayer( Writer &io, const Player *from, const Player *to ) {
	if ( !to ) return io.Fail();
	const Player empty{};
	if ( !from ) from = &empty;
	const auto count = LastChanged( *from, *to, PlayerFields );
	io.Write( static_cast<std::uint32_t>( count ), 8 );
	for ( std::size_t i = 0; i < count; ++i ) {
		const auto field = PlayerFields[i];
		const auto value = Word( *to, field.offset );
		const bool changed = value != Word( *from, field.offset );
		io.Write( changed, 1 );
		if ( changed ) WriteScalar( io, value, field.bits );
	}
	const auto stats = ArrayChanges( from->stats, to->stats );
	const auto persistant = ArrayChanges( from->persistant, to->persistant );
	const auto ammo = ArrayChanges( from->ammo, to->ammo );
	const auto powerups = ArrayChanges( from->powerups, to->powerups );
	const bool arraysChanged = ( stats | persistant | ammo | powerups ) != 0;
	io.Write( arraysChanged, 1 );
	if ( arraysChanged ) {
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
	const auto count = io.Read( 8 );
	if ( count > PlayerFields.size() ) return io.Fail();
	for ( std::size_t i = 0; i < count && io.Good(); ++i )
		if ( io.Read( 1 ) ) SetWord( result, PlayerFields[i].offset, ReadScalar( io, PlayerFields[i].bits ) );
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

// Both retail and ioEF call MSG_WriteDeltaUsercmd, not the keyed helper.
// Lilium's reader additionally normalizes the three movement bytes.
template<class Writer> bool WriteCommand( Writer &io, const Command *from, const Command *to ) {
	return message::WriteCommand( io, from, to );
}

template<class Reader> bool ReadCommand( Reader &io, const Command *from, Command *to ) {
	if ( !to ) return io.Fail();
	Command result{};
	if ( !message::ReadCommand( io, from, &result ) ) return false;
	if ( result.forwardmove == -128 ) result.forwardmove = -127;
	if ( result.rightmove == -128 ) result.rightmove = -127;
	if ( result.upmove == -128 ) result.upmove = -127;
	*to = result;
	return true;
}

} // namespace fntech3::stvef::message::ioef
#endif
