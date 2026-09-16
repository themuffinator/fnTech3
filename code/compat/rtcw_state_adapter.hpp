/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_STATE_ADAPTER_HPP
#define FNTECH3_RTCW_STATE_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "rtcw_game_abi.h"
#include <cstdint>
#include <cstring>
#include <type_traits>

/* Field-by-field RTCW/module-to-engine transfers. The engine's extension
 * records are not an external ABI. No profile's record is cast to another
 * profile or to the canonical state. All conversions publish transactionally. */
namespace fntech3::rtcw::state {
namespace detail {

template<bool Out, class External, class Internal>
void Field( External &external, Internal &internal ) noexcept {
	static_assert( sizeof( External ) == sizeof( Internal ) );
	static_assert( std::is_trivially_copyable_v<External> && std::is_trivially_copyable_v<Internal> );
	if constexpr ( Out ) std::memcpy( &external, &internal, sizeof( external ) );
	else std::memcpy( &internal, &external, sizeof( internal ) );
}

inline constexpr trType_t TrajectoryTypes[]{
	TR_STATIONARY, TR_INTERPOLATE, TR_LINEAR, TR_LINEAR_STOP,
	TR_RTCW_LINEAR_STOP_BACK, TR_SINE, TR_GRAVITY, TR_RTCW_GRAVITY_LOW,
	TR_RTCW_GRAVITY_FLOAT, TR_RTCW_ACCELERATE, TR_RTCW_DECELERATE
};

template<bool Out, class External, class Internal>
bool Trajectory( External &external, Internal &internal ) noexcept {
	if constexpr ( Out ) {
		std::uint32_t type;
		static_assert( sizeof( type ) == sizeof( internal.trType ) );
		std::memcpy( &type, &internal.trType, sizeof( type ) );
		bool found = false;
		for ( std::int32_t index = 0; index < 11; ++index ) {
			if ( type == static_cast<std::uint32_t>( TrajectoryTypes[index] ) ) {
				external.trType = index;
				found = true;
				break;
			}
		}
		if ( !found ) return false;
	} else {
		if ( external.trType < 0 || external.trType >= 11 ) return false;
		internal.trType = TrajectoryTypes[external.trType];
	}
	Field<Out>( external.trTime, internal.trTime );
	Field<Out>( external.trDuration, internal.trDuration );
	Field<Out>( external.trBase, internal.trBase );
	Field<Out>( external.trDelta, internal.trDelta );
	return true;
}

template<bool Out, class External, class Internal>
bool Trace( External &external, Internal &internal ) noexcept {
	// Read enum representations as bytes so an invalid module boolean is
	// rejected before it can become an invalid canonical qboolean value.
	std::uint32_t allsolid, startsolid;
	if constexpr ( Out ) {
		std::memcpy( &allsolid, &internal.allsolid, 4 );
		std::memcpy( &startsolid, &internal.startsolid, 4 );
	} else {
		allsolid = static_cast<std::uint32_t>( external.allsolid );
		startsolid = static_cast<std::uint32_t>( external.startsolid );
	}
	if ( allsolid > 1 || startsolid > 1 ) return false;
	Field<Out>( external.allsolid, internal.allsolid );
	Field<Out>( external.startsolid, internal.startsolid );
	Field<Out>( external.fraction, internal.fraction );
	Field<Out>( external.endpos, internal.endpos );
	Field<Out>( external.surfaceFlags, internal.surfaceFlags );
	Field<Out>( external.contents, internal.contents );
	Field<Out>( external.entityNum, internal.entityNum );
	Field<Out>( external.plane.normal, internal.plane.normal );
	Field<Out>( external.plane.dist, internal.plane.dist );
	Field<Out>( external.plane.type, internal.plane.type );
	Field<Out>( external.plane.signbits, internal.plane.signbits );
	Field<Out>( external.plane.padding, internal.plane.pad );
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
	Field<Out>( external.dl_intensity, internal.rtcw.dl_intensity );
	Field<Out>( external.eventSequence, internal.rtcw.eventSequence );
	Field<Out>( external.events, internal.rtcw.events );
	Field<Out>( external.eventParms, internal.rtcw.eventParms );
	Field<Out>( external.density, internal.rtcw.density );
	Field<Out>( external.dmgFlags, internal.rtcw.dmgFlags );
	Field<Out>( external.onFireStart, internal.rtcw.onFireStart );
	Field<Out>( external.onFireEnd, internal.rtcw.onFireEnd );
	Field<Out>( external.aiChar, internal.rtcw.aiChar );
	Field<Out>( external.teamNum, internal.rtcw.teamNum );
	Field<Out>( external.effect1Time, internal.rtcw.effect1Time );
	Field<Out>( external.effect2Time, internal.rtcw.effect2Time );
	Field<Out>( external.effect3Time, internal.rtcw.effect3Time );
	Field<Out>( external.aiState, internal.rtcw.aiState );
	Field<Out>( external.animMovetype, internal.rtcw.animMovetype );
	return true;
}

template<bool Out, bool SP, class External, class Internal>
bool Player( External &external, Internal &internal ) noexcept {
	auto &common = external.common;
	if ( !Trace<Out>( common.serverCursorHintTrace, internal.rtcw.serverCursorHintTrace ) ) return false;
	Field<Out>( common.commandTime, internal.commandTime );
	Field<Out>( common.pm_type, internal.pm_type );
	Field<Out>( common.bobCycle, internal.bobCycle );
	Field<Out>( common.pm_flags, internal.pm_flags );
	Field<Out>( common.pm_time, internal.pm_time );
	Field<Out>( common.origin, internal.origin );
	Field<Out>( common.velocity, internal.velocity );
	Field<Out>( common.weaponTime, internal.weaponTime );
	Field<Out>( common.gravity, internal.gravity );
	Field<Out>( common.speed, internal.speed );
	Field<Out>( common.delta_angles, internal.delta_angles );
	Field<Out>( common.groundEntityNum, internal.groundEntityNum );
	Field<Out>( common.legsTimer, internal.legsTimer );
	Field<Out>( common.legsAnim, internal.legsAnim );
	Field<Out>( common.torsoTimer, internal.torsoTimer );
	Field<Out>( common.torsoAnim, internal.torsoAnim );
	Field<Out>( common.movementDir, internal.movementDir );
	Field<Out>( common.eFlags, internal.eFlags );
	Field<Out>( common.eventSequence, internal.eventSequence );
	Field<Out>( common.externalEvent, internal.externalEvent );
	Field<Out>( common.externalEventParm, internal.externalEventParm );
	Field<Out>( common.externalEventTime, internal.externalEventTime );
	Field<Out>( common.clientNum, internal.clientNum );
	Field<Out>( common.weapon, internal.weapon );
	Field<Out>( common.weaponstate, internal.weaponstate );
	Field<Out>( common.viewangles, internal.viewangles );
	Field<Out>( common.viewheight, internal.viewheight );
	Field<Out>( common.damageEvent, internal.damageEvent );
	Field<Out>( common.damageYaw, internal.damageYaw );
	Field<Out>( common.damagePitch, internal.damagePitch );
	Field<Out>( common.damageCount, internal.damageCount );
	Field<Out>( common.stats, internal.stats );
	Field<Out>( common.persistant, internal.persistant );
	Field<Out>( common.powerups, internal.powerups );
	Field<Out>( common.ping, internal.ping );
	Field<Out>( common.pmove_framecount, internal.pmove_framecount );
	Field<Out>( common.entityEventSequence, internal.entityEventSequence );
	Field<Out>( common.jumpTime, internal.jumpTime );
	Field<Out>( common.weaponDelay, internal.rtcw.weaponDelay );
	Field<Out>( common.grenadeTimeLeft, internal.rtcw.grenadeTimeLeft );
	Field<Out>( common.leanf, internal.rtcw.leanf );
	Field<Out>( common.oldEventSequence, internal.rtcw.oldEventSequence );
	Field<Out>( common.item, internal.rtcw.item );
	Field<Out>( common.ammoclip, internal.rtcw.ammoclip );
	Field<Out>( common.holdable, internal.rtcw.holdable );
	Field<Out>( common.holding, internal.rtcw.holding );
	Field<Out>( common.weapons, internal.rtcw.weapons );
	Field<Out>( common.mins, internal.rtcw.mins );
	Field<Out>( common.maxs, internal.rtcw.maxs );
	Field<Out>( common.crouchMaxZ, internal.rtcw.crouchMaxZ );
	Field<Out>( common.crouchViewHeight, internal.rtcw.crouchViewHeight );
	Field<Out>( common.standViewHeight, internal.rtcw.standViewHeight );
	Field<Out>( common.deadViewHeight, internal.rtcw.deadViewHeight );
	Field<Out>( common.runSpeedScale, internal.rtcw.runSpeedScale );
	Field<Out>( common.sprintSpeedScale, internal.rtcw.sprintSpeedScale );
	Field<Out>( common.crouchSpeedScale, internal.rtcw.crouchSpeedScale );
	Field<Out>( common.viewlocked, internal.rtcw.viewlocked );
	Field<Out>( common.viewlocked_entNum, internal.rtcw.viewlocked_entNum );
	Field<Out>( common.friction, internal.rtcw.friction );
	Field<Out>( common.aiChar, internal.rtcw.aiChar );
	Field<Out>( common.teamNum, internal.rtcw.teamNum );
	Field<Out>( common.gunfx, internal.rtcw.gunfx );
	Field<Out>( common.onFireStart, internal.rtcw.onFireStart );
	Field<Out>( common.serverCursorHint, internal.rtcw.serverCursorHint );
	Field<Out>( common.serverCursorHintVal, internal.rtcw.serverCursorHintVal );
	Field<Out>( common.sprintTime, internal.rtcw.sprintTime );
	Field<Out>( common.sprintExertTime, internal.rtcw.sprintExertTime );
	Field<Out>( common.classWeaponTime, internal.rtcw.classWeaponTime );
	Field<Out>( common.weapAnimTimer, internal.rtcw.weapAnimTimer );
	Field<Out>( common.weapAnim, internal.rtcw.weapAnim );
	Field<Out>( common.releasedFire, internal.rtcw.releasedFire );
	Field<Out>( common.aimSpreadScaleFloat, internal.rtcw.aimSpreadScaleFloat );
	Field<Out>( common.aimSpreadScale, internal.rtcw.aimSpreadScale );
	Field<Out>( common.lastFireTime, internal.rtcw.lastFireTime );
	Field<Out>( common.quickGrenTime, internal.rtcw.quickGrenTime );
	Field<Out>( common.leanStopDebounceTime, internal.rtcw.leanStopDebounceTime );
	Field<Out>( common.weapHeat, internal.rtcw.weapHeat );
	Field<Out>( common.curWeapHeat, internal.rtcw.curWeapHeat );
	Field<Out>( common.venomTime, internal.rtcw.venomTime );
	static_assert( MAX_PS_EVENTS == 2 && MAX_WEAPONS == 16 );
	for ( unsigned index = 0; index < 4; ++index ) {
		if ( index < 2 ) {
			Field<Out>( common.events[index], internal.events[index] );
			Field<Out>( common.eventParms[index], internal.eventParms[index] );
		} else {
			Field<Out>( common.events[index], internal.rtcw.events[index - 2] );
			Field<Out>( common.eventParms[index], internal.rtcw.eventParms[index - 2] );
		}
	}
	for ( unsigned index = 0; index < 64; ++index ) {
		if ( index < 16 ) Field<Out>( common.ammo[index], internal.ammo[index] );
		else Field<Out>( common.ammo[index], internal.rtcw.ammo[index - 16] );
	}
	Field<Out>( external.aiState, internal.rtcw.aiState );
	if constexpr ( SP ) {
		Field<Out>( external.accShowBits, internal.rtcw.accShowBits );
		Field<Out>( external.accHideBits, internal.rtcw.accHideBits );
		Field<Out>( external.footstepCount, internal.rtcw.footstepCount );
	} else Field<Out>( external.identifyClient, internal.rtcw.identifyClient );
	return true;
}

template<bool Out, bool SP, class External, class Internal>
bool Command( External &external, Internal &internal ) noexcept {
	auto &common = external.common;
	Field<Out>( common.serverTime, internal.serverTime );
	Field<Out>( common.angles, internal.angles );
	Field<Out>( common.weapon, internal.weapon );
	Field<Out>( common.forwardmove, internal.forwardmove );
	Field<Out>( common.rightmove, internal.rightmove );
	Field<Out>( common.upmove, internal.upmove );
	Field<Out>( common.wbuttons, internal.rtcw.wbuttons );
	Field<Out>( common.holdable, internal.rtcw.holdable );
	Field<Out>( common.wolfkick, internal.rtcw.wolfkick );
	if constexpr ( Out ) common.buttons = static_cast<std::uint8_t>( internal.buttons );
	else internal.buttons = common.buttons;
	if constexpr ( SP ) Field<Out>( external.cld, internal.rtcw.cld );
	else {
		Field<Out>( external.mpSetup, internal.rtcw.mpSetup );
		Field<Out>( external.identClient, internal.rtcw.identClient );
	}
	return true;
}

} // namespace detail

inline bool FromInternal( const gameState_t &source, fntech3_rtcw_gameState_t &destination ) noexcept {
	static_assert( MAX_GAME_CONFIGSTRINGS >= FNTECH3_RTCW_MAX_CONFIGSTRINGS );
	static_assert( MAX_GAMESTATE_CHARS == FNTECH3_RTCW_MAX_GAMESTATE_CHARS );
	if ( source.dataCount < 1 || source.dataCount > MAX_GAMESTATE_CHARS ||
		source.stringData[0] != '\0' || source.stringData[source.dataCount - 1] != '\0' ) return false;
	for ( int index = 0; index < FNTECH3_RTCW_MAX_CONFIGSTRINGS; ++index ) {
		if ( source.stringOffsets[index] < 0 || source.stringOffsets[index] >= source.dataCount ) return false;
	}
	fntech3_rtcw_gameState_t result{};
	std::memcpy( result.stringOffsets, source.stringOffsets, sizeof( result.stringOffsets ) );
	std::memcpy( result.stringData, source.stringData, static_cast<std::size_t>( source.dataCount ) );
	result.dataCount = source.dataCount;
	destination = result;
	return true;
}

inline bool FromInternal( const trace_t &source, fntech3_rtcw_trace_t &destination ) noexcept {
	fntech3_rtcw_trace_t result{};
	if ( !detail::Trace<true>( result, source ) ) return false;
	destination = result;
	return true;
}

inline bool ToInternal( const fntech3_rtcw_entityState_t &source, entityState_t &destination ) noexcept {
	entityState_t result{};
	if ( !detail::Entity<false>( source, result ) ) return false;
	destination = result;
	return true;
}

inline bool FromInternal( const entityState_t &source, fntech3_rtcw_entityState_t &destination ) noexcept {
	fntech3_rtcw_entityState_t result{};
	if ( !detail::Entity<true>( result, source ) ) return false;
	destination = result;
	return true;
}

inline bool ToInternal( const fntech3_rtcw_sp_playerState_t &source, playerState_t &destination ) noexcept {
	playerState_t result{};
	if ( !detail::Player<false, true>( source, result ) ) return false;
	destination = result;
	return true;
}

inline bool FromInternal( const playerState_t &source, fntech3_rtcw_sp_playerState_t &destination ) noexcept {
	fntech3_rtcw_sp_playerState_t result{};
	if ( !detail::Player<true, true>( result, source ) ) return false;
	destination = result;
	return true;
}

inline bool ToInternal( const fntech3_rtcw_mp_playerState_t &source, playerState_t &destination ) noexcept {
	playerState_t result{};
	if ( !detail::Player<false, false>( source, result ) ) return false;
	destination = result;
	return true;
}

inline bool FromInternal( const playerState_t &source, fntech3_rtcw_mp_playerState_t &destination ) noexcept {
	fntech3_rtcw_mp_playerState_t result{};
	if ( !detail::Player<true, false>( result, source ) ) return false;
	destination = result;
	return true;
}

inline bool ToInternal( const fntech3_rtcw_sp_usercmd_t &source, usercmd_t &destination ) noexcept {
	usercmd_t result{};
	if ( !detail::Command<false, true>( source, result ) ) return false;
	destination = result;
	return true;
}

inline bool FromInternal( const usercmd_t &source, fntech3_rtcw_sp_usercmd_t &destination ) noexcept {
	fntech3_rtcw_sp_usercmd_t result{};
	if ( !detail::Command<true, true>( result, source ) ) return false;
	destination = result;
	return true;
}

inline bool ToInternal( const fntech3_rtcw_mp_usercmd_t &source, usercmd_t &destination ) noexcept {
	usercmd_t result{};
	if ( !detail::Command<false, false>( source, result ) ) return false;
	destination = result;
	return true;
}

inline bool FromInternal( const usercmd_t &source, fntech3_rtcw_mp_usercmd_t &destination ) noexcept {
	fntech3_rtcw_mp_usercmd_t result{};
	if ( !detail::Command<true, false>( result, source ) ) return false;
	destination = result;
	return true;
}

} // namespace fntech3::rtcw::state
#endif
