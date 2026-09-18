/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_STATE_ADAPTER_HPP
#define FNTECH3_STVEF_STATE_ADAPTER_HPP

#include "stvef_game_abi.h"
#include "rtcw_state_adapter.hpp"

#include <cstdint>
#include <cstring>

/* Field-by-field transfers between the frozen Elite Force Holomatch records
 * and the engine's canonical state. Holomatch keeps Quake III's field meanings
 * and numbers; its third and fourth player events, recharge, use and intro
 * timers and damage shield count live in the engine's stvef extension. Every
 * conversion publishes only a complete, validated record. */
namespace fntech3::stvef::state {
namespace detail {

using rtcw::state::detail::Field;

// Holomatch's trajectory types are Quake III's first six, with their numbers.
inline constexpr std::uint32_t kTrajectoryTypeCount = 6;
static_assert( TR_STATIONARY == 0 && TR_GRAVITY == 5 );

template<bool Out, class External, class Internal>
bool Trajectory( External &external, Internal &internal ) noexcept {
	if constexpr ( Out ) {
		std::uint32_t type;
		static_assert( sizeof( type ) == sizeof( internal.trType ) );
		std::memcpy( &type, &internal.trType, sizeof( type ) );
		if ( type >= kTrajectoryTypeCount || internal.gravity != 0.0f ) return false;
		external.trType = static_cast<std::int32_t>( type );
	} else {
		if ( external.trType < 0 ||
			static_cast<std::uint32_t>( external.trType ) >= kTrajectoryTypeCount ) return false;
		internal.trType = static_cast<trType_t>( external.trType );
		internal.gravity = 0.0f;
	}
	Field<Out>( external.trTime, internal.trTime );
	Field<Out>( external.trDuration, internal.trDuration );
	Field<Out>( external.trBase, internal.trBase );
	Field<Out>( external.trDelta, internal.trDelta );
	return true;
}

template<bool Out, class External, class Internal>
bool Entity( External &external, Internal &internal ) noexcept {
	if ( !Trajectory<Out>( external.pos, internal.pos ) ||
		!Trajectory<Out>( external.apos, internal.apos ) ) return false;
	Field<Out>( external.number, internal.number );
	Field<Out>( external.eType, internal.eType );
	Field<Out>( external.eFlags, internal.eFlags );
	Field<Out>( external.time, internal.time );
	Field<Out>( external.time2, internal.time2 );
	Field<Out>( external.origin, internal.origin );
	Field<Out>( external.origin2, internal.origin2 );
	Field<Out>( external.angles, internal.angles );
	Field<Out>( external.angles2, internal.angles2 );
	Field<Out>( external.otherEntityNum, internal.otherEntityNum );
	Field<Out>( external.otherEntityNum2, internal.otherEntityNum2 );
	Field<Out>( external.groundEntityNum, internal.groundEntityNum );
	Field<Out>( external.constantLight, internal.constantLight );
	Field<Out>( external.loopSound, internal.loopSound );
	Field<Out>( external.modelindex, internal.modelindex );
	Field<Out>( external.modelindex2, internal.modelindex2 );
	Field<Out>( external.clientNum, internal.clientNum );
	Field<Out>( external.frame, internal.frame );
	Field<Out>( external.solid, internal.solid );
	Field<Out>( external.event, internal.event );
	Field<Out>( external.eventParm, internal.eventParm );
	Field<Out>( external.powerups, internal.powerups );
	Field<Out>( external.weapon, internal.weapon );
	Field<Out>( external.legsAnim, internal.legsAnim );
	Field<Out>( external.torsoAnim, internal.torsoAnim );
	return true;
}

template<bool Out, class External, class Internal>
bool Player( External &external, Internal &internal ) noexcept {
	Field<Out>( external.commandTime, internal.commandTime );
	Field<Out>( external.pm_type, internal.pm_type );
	Field<Out>( external.bobCycle, internal.bobCycle );
	Field<Out>( external.pm_flags, internal.pm_flags );
	Field<Out>( external.pm_time, internal.pm_time );
	Field<Out>( external.origin, internal.origin );
	Field<Out>( external.velocity, internal.velocity );
	Field<Out>( external.weaponTime, internal.weaponTime );
	Field<Out>( external.rechargeTime, internal.stvef.rechargeTime );
	// useTime is a short followed by two bytes of padding.
	if constexpr ( Out ) {
		const int useTime = internal.stvef.useTime;
		if ( useTime < INT16_MIN || useTime > INT16_MAX ) return false;
		external.useTime = static_cast<std::int16_t>( useTime );
	} else {
		internal.stvef.useTime = external.useTime;
	}
	Field<Out>( external.introTime, internal.stvef.introTime );
	Field<Out>( external.gravity, internal.gravity );
	Field<Out>( external.speed, internal.speed );
	Field<Out>( external.delta_angles, internal.delta_angles );
	Field<Out>( external.groundEntityNum, internal.groundEntityNum );
	Field<Out>( external.legsTimer, internal.legsTimer );
	Field<Out>( external.legsAnim, internal.legsAnim );
	Field<Out>( external.torsoTimer, internal.torsoTimer );
	Field<Out>( external.torsoAnim, internal.torsoAnim );
	Field<Out>( external.movementDir, internal.movementDir );
	Field<Out>( external.eFlags, internal.eFlags );
	Field<Out>( external.eventSequence, internal.eventSequence );
	static_assert( FNTECH3_STVEF_MP_MAX_PS_EVENTS == MAX_PS_EVENTS + 2 );
	for ( unsigned n = 0; n < FNTECH3_STVEF_MP_MAX_PS_EVENTS; ++n ) {
		if ( n < MAX_PS_EVENTS ) {
			Field<Out>( external.events[n], internal.events[n] );
			Field<Out>( external.eventParms[n], internal.eventParms[n] );
		} else {
			Field<Out>( external.events[n], internal.stvef.events[n - MAX_PS_EVENTS] );
			Field<Out>( external.eventParms[n], internal.stvef.eventParms[n - MAX_PS_EVENTS] );
		}
	}
	Field<Out>( external.externalEvent, internal.externalEvent );
	Field<Out>( external.externalEventParm, internal.externalEventParm );
	Field<Out>( external.externalEventTime, internal.externalEventTime );
	Field<Out>( external.clientNum, internal.clientNum );
	Field<Out>( external.weapon, internal.weapon );
	Field<Out>( external.weaponstate, internal.weaponstate );
	Field<Out>( external.viewangles, internal.viewangles );
	Field<Out>( external.viewheight, internal.viewheight );
	Field<Out>( external.damageEvent, internal.damageEvent );
	Field<Out>( external.damageYaw, internal.damageYaw );
	Field<Out>( external.damagePitch, internal.damagePitch );
	Field<Out>( external.damageCount, internal.damageCount );
	Field<Out>( external.damageShieldCount, internal.stvef.damageShieldCount );
	Field<Out>( external.stats, internal.stats );
	Field<Out>( external.persistant, internal.persistant );
	Field<Out>( external.powerups, internal.powerups );
	Field<Out>( external.ammo, internal.ammo );
	Field<Out>( external.ping, internal.ping );
	Field<Out>( external.entityEventSequence, internal.entityEventSequence );
	return true;
}

template<bool Out, class External, class Internal>
bool Command( External &external, Internal &internal ) noexcept {
	Field<Out>( external.serverTime, internal.serverTime );
	Field<Out>( external.angles, internal.angles );
	Field<Out>( external.forwardmove, internal.forwardmove );
	Field<Out>( external.rightmove, internal.rightmove );
	Field<Out>( external.upmove, internal.upmove );
	// Holomatch buttons are one byte.
	if constexpr ( Out ) {
		if ( internal.buttons < 0 || internal.buttons > 255 ) return false;
		external.buttons = static_cast<std::uint8_t>( internal.buttons );
		external.weapon = internal.weapon;
	} else {
		internal.buttons = external.buttons;
		internal.weapon = external.weapon;
	}
	return true;
}

} // namespace detail

#define STVEF_TRANSFER(kind, externalType, internalType) \
inline bool FromInternal( const internalType &source, externalType &destination ) noexcept { \
	externalType temporary{}; \
	if ( !detail::kind<true>( temporary, source ) ) return false; \
	destination = temporary; return true; } \
inline bool ToInternal( const externalType &source, internalType &destination ) noexcept { \
	internalType temporary{}; \
	if ( !detail::kind<false>( source, temporary ) ) return false; \
	destination = temporary; return true; }
STVEF_TRANSFER( Entity, fntech3_stvef_mp_entityState_t, entityState_t )
STVEF_TRANSFER( Player, fntech3_stvef_mp_playerState_t, playerState_t )
STVEF_TRANSFER( Command, fntech3_stvef_mp_usercmd_t, usercmd_t )
#undef STVEF_TRANSFER

// Holomatch's gamestate holds 1,024 configstrings in 16,000 bytes. Publish
// only an internally consistent state whose offsets address its own text.
inline bool FromInternal( const gameState_t &source, fntech3_stvef_gameState_t &destination ) noexcept {
	static_assert( MAX_GAME_CONFIGSTRINGS >= FNTECH3_STVEF_MAX_CONFIGSTRINGS );
	static_assert( MAX_GAMESTATE_CHARS >= FNTECH3_STVEF_MAX_GAMESTATE_CHARS );
	if ( source.dataCount < 1 || source.dataCount > FNTECH3_STVEF_MAX_GAMESTATE_CHARS ||
		source.stringData[0] != '\0' || source.stringData[source.dataCount - 1] != '\0' ) return false;
	for ( int index = 0; index < FNTECH3_STVEF_MAX_CONFIGSTRINGS; ++index ) {
		if ( source.stringOffsets[index] < 0 || source.stringOffsets[index] >= source.dataCount ) return false;
	}
	fntech3_stvef_gameState_t result{};
	for ( int index = 0; index < FNTECH3_STVEF_MAX_CONFIGSTRINGS; ++index )
		result.stringOffsets[index] = source.stringOffsets[index];
	std::memcpy( result.stringData, source.stringData, static_cast<std::size_t>( source.dataCount ) );
	result.dataCount = source.dataCount;
	destination = result;
	return true;
}

} // namespace fntech3::stvef::state
#endif
