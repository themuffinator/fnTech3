/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_STATE_ADAPTER_HPP
#define FNTECH3_JK2_STATE_ADAPTER_HPP

#include "jk2_game_abi.h"
#include "rtcw_state_adapter.hpp"

#include <cstdint>
#include <cstring>
#include <type_traits>

/* Field-by-field transfers between the frozen Jedi Outcast records and the
 * engine's canonical state.
 *
 * Outcast's entity state is seventy-four fields. Twenty-nine are Quake III's,
 * with Quake III's meanings, and go to the canonical record. The other
 * twenty-two are Raven's and go to the engine's raven extension, which is an
 * engine-owned record and no part of any ABI.
 *
 * This is a separate adapter from Jedi Academy's and reaches none of its
 * records. The two external layouts are 296 bytes and 532, and every member
 * that survived from one title to the other moved: nothing here is shared with
 * Academy except the engine-side carrier, which interprets nothing.
 *
 * What the canonical record carries and Outcast does not -- armor, location,
 * health, jumpTime, doubleJumped -- is not written outward and comes back
 * zero. The external record is where an Outcast entity lives, so nothing is
 * lost that this title ever set. */
namespace fntech3::jk2::state {
namespace detail {

using rtcw::state::detail::Field;

/* Outcast's trajectory numbering has not been measured. Its evaluator lives in
 * the module, which is bytecode, and reading a switch out of that is a
 * different job from reading one out of a compiled binary. Until it is done,
 * only the type this engine can carry without deciding what it means is
 * admitted: the number is passed through unchanged in both directions, and a
 * number outside the seven a Raven title is known to use is refused rather
 * than turned into a canonical meaning that may be the wrong one.
 *
 * That is narrower than Academy's, where the numbering *was* measured and the
 * mapping is exact. The difference is stated here rather than papered over. */
inline constexpr std::int32_t TrajectoryTypeCount = 7;

template<bool Out, class External, class Internal>
bool Trajectory( External &external, Internal &internal ) noexcept {
	if constexpr ( Out ) {
		std::uint32_t type;
		static_assert( sizeof( type ) == sizeof( internal.trType ) );
		std::memcpy( &type, &internal.trType, sizeof( type ) );
		if ( internal.gravity != 0.0f ) return false;
		if ( type >= std::uint32_t( TrajectoryTypeCount ) ) return false;
		external.trType = std::int32_t( type );
	} else {
		if ( external.trType < 0 || external.trType >= TrajectoryTypeCount ) return false;
		std::memcpy( &internal.trType, &external.trType, sizeof( external.trType ) );
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

	/* Quake III's fields, with Quake III's meanings. */
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
	Field<Out>( external.generic1, internal.generic1 );

	/* Raven's own, in the order this title's record declares them. */
	Field<Out>( external.bolt1, internal.raven.bolt1 );
	Field<Out>( external.bolt2, internal.raven.bolt2 );
	Field<Out>( external.trickedentindex, internal.raven.trickedentindex );
	Field<Out>( external.trickedentindex2, internal.raven.trickedentindex2 );
	Field<Out>( external.trickedentindex3, internal.raven.trickedentindex3 );
	Field<Out>( external.trickedentindex4, internal.raven.trickedentindex4 );
	Field<Out>( external.speed, internal.raven.speed );
	Field<Out>( external.fireflag, internal.raven.fireflag );
	Field<Out>( external.genericenemyindex, internal.raven.genericenemyindex );
	Field<Out>( external.activeForcePass, internal.raven.activeForcePass );
	Field<Out>( external.emplacedOwner, internal.raven.emplacedOwner );
	Field<Out>( external.modelGhoul2, internal.raven.modelGhoul2 );
	Field<Out>( external.g2radius, internal.raven.g2radius );
	Field<Out>( external.saberInFlight, internal.raven.saberInFlight );
	Field<Out>( external.saberEntityNum, internal.raven.saberEntityNum );
	Field<Out>( external.saberMove, internal.raven.saberMove );
	Field<Out>( external.forcePowersActive, internal.raven.forcePowersActive );
	Field<Out>( external.isJediMaster, internal.raven.isJediMaster );
	Field<Out>( external.owner, internal.raven.owner );
	Field<Out>( external.teamowner, internal.raven.teamowner );
	Field<Out>( external.shouldtarget, internal.raven.shouldtarget );
	Field<Out>( external.forceFrame, internal.raven.forceFrame );
	return true;
}

/* One client's input frame. The first twenty-one bytes are Quake III's and
 * mean what Quake III means; the three after them are Raven's, where Quake III
 * keeps a second weapon and a field of view, so the three movement bytes sit
 * one byte later. The engine carries Raven's three and reads none of them.
 *
 * This is a separate adapter from Jedi Academy's and reaches none of its
 * records, even though the two records came out the same shape. */
template<bool Out, class External, class Internal>
bool Command( External &external, Internal &internal ) noexcept {
	Field<Out>( external.serverTime, internal.serverTime );
	Field<Out>( external.angles, internal.angles );
	Field<Out>( external.buttons, internal.buttons );
	Field<Out>( external.weapon, internal.weapon );
	Field<Out>( external.forcesel, internal.raven.forcesel );
	Field<Out>( external.invensel, internal.raven.invensel );
	Field<Out>( external.generic_cmd, internal.raven.generic_cmd );
	Field<Out>( external.forwardmove, internal.forwardmove );
	Field<Out>( external.rightmove, internal.rightmove );
	Field<Out>( external.upmove, internal.upmove );
	return true;
}

} // namespace detail

inline bool FromInternal( const entityState_t &source,
	fntech3_jk2_entityState_t &destination ) noexcept
{
	fntech3_jk2_entityState_t temporary{};
	if ( !detail::Entity<true>( temporary, source ) ) return false;
	destination = temporary;
	return true;
}

inline bool ToInternal( const fntech3_jk2_entityState_t &source,
	entityState_t &destination ) noexcept
{
	entityState_t temporary{};
	if ( !detail::Entity<false>( source, temporary ) ) return false;
	destination = temporary;
	return true;
}

inline bool FromInternal( const usercmd_t &source,
	fntech3_jk2_usercmd_t &destination ) noexcept
{
	fntech3_jk2_usercmd_t temporary{};
	if ( !detail::Command<true>( temporary, source ) ) return false;
	destination = temporary;
	return true;
}

inline bool ToInternal( const fntech3_jk2_usercmd_t &source,
	usercmd_t &destination ) noexcept
{
	usercmd_t temporary{};
	if ( !detail::Command<false>( source, temporary ) ) return false;
	destination = temporary;
	return true;
}

/* The measured fields cross individually. Unknown members are carried in a
 * private backing record so a module -> snapshot -> module round trip cannot
 * replace its untransmitted state with zeros. Writing overlays the named
 * members on that backing record; it does not assign meaning to unknown bytes. */
namespace detail {
template<bool Out, class External, class Internal>
bool Player( External &external, Internal &internal ) noexcept {
	/* Quake III's fields, with Quake III's meanings. */
	Field<Out>( external.commandTime, internal.commandTime );
	Field<Out>( external.pm_type, internal.pm_type );
	Field<Out>( external.bobCycle, internal.bobCycle );
	Field<Out>( external.pm_flags, internal.pm_flags );
	Field<Out>( external.pm_time, internal.pm_time );
	Field<Out>( external.origin, internal.origin );
	Field<Out>( external.velocity, internal.velocity );
	Field<Out>( external.weaponTime, internal.weaponTime );
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
	Field<Out>( external.events, internal.events );
	Field<Out>( external.eventParms, internal.eventParms );
	Field<Out>( external.externalEvent, internal.externalEvent );
	Field<Out>( external.externalEventParm, internal.externalEventParm );
	Field<Out>( external.clientNum, internal.clientNum );
	Field<Out>( external.weapon, internal.weapon );
	Field<Out>( external.weaponstate, internal.weaponstate );
	Field<Out>( external.viewangles, internal.viewangles );
	Field<Out>( external.viewheight, internal.viewheight );
	Field<Out>( external.damageEvent, internal.damageEvent );
	Field<Out>( external.damageYaw, internal.damageYaw );
	Field<Out>( external.damagePitch, internal.damagePitch );
	Field<Out>( external.damageCount, internal.damageCount );
	Field<Out>( external.stats, internal.stats );
	Field<Out>( external.persistant, internal.persistant );
	Field<Out>( external.powerups, internal.powerups );
	Field<Out>( external.ammo, internal.ammo );
	Field<Out>( external.generic1, internal.generic1 );
	Field<Out>( external.loopSound, internal.loopSound );
	Field<Out>( external.jumppad_ent, internal.jumppad_ent );
	Field<Out>( external.ping, internal.ping );

	/* Raven's own. */
	Field<Out>( external.weaponChargeTime, internal.raven.weaponChargeTime );
	Field<Out>( external.weaponChargeSubtractTime, internal.raven.weaponChargeSubtractTime );
	Field<Out>( external.basespeed, internal.raven.basespeed );
	Field<Out>( external.damageType, internal.raven.damageType );
	Field<Out>( external.saberInFlight, internal.raven.saberInFlight );
	Field<Out>( external.saberActive, internal.raven.saberActive );
	Field<Out>( external.saberMove, internal.raven.saberMove );
	Field<Out>( external.saberBlocked, internal.raven.saberBlocked );
	Field<Out>( external.saberLockTime, internal.raven.saberLockTime );
	Field<Out>( external.saberLockEnemy, internal.raven.saberLockEnemy );
	Field<Out>( external.saberLockFrame, internal.raven.saberLockFrame );
	Field<Out>( external.saberLockAdvance, internal.raven.saberLockAdvance );
	Field<Out>( external.saberEntityNum, internal.raven.saberEntityNum );
	Field<Out>( external.saberCanThrow, internal.raven.saberCanThrow );
	Field<Out>( external.rocketLockIndex, internal.raven.rocketLockIndex );
	Field<Out>( external.rocketLockTime, internal.raven.rocketLockTime );
	Field<Out>( external.rocketTargetTime, internal.raven.rocketTargetTime );
	Field<Out>( external.emplacedIndex, internal.raven.emplacedIndex );
	Field<Out>( external.isJediMaster, internal.raven.isJediMaster );
	Field<Out>( external.genericEnemyIndex, internal.raven.genericEnemyIndex );
	Field<Out>( external.activeForcePass, internal.raven.activeForcePass );
	Field<Out>( external.hasDetPackPlanted, internal.raven.hasDetPackPlanted );
	Field<Out>( external.holocronBits, internal.raven.holocronBits );
	Field<Out>( external.electrifyTime, internal.raven.electrifyTime );
	Field<Out>( external.fd_forcePowerDebounce_FP_LEVITATION, internal.raven.fd_forcePowerDebounce_FP_LEVITATION );
	Field<Out>( external.fd_forcePowersKnown, internal.raven.fd_forcePowersKnown );
	Field<Out>( external.fd_forcePowersActive, internal.raven.fd_forcePowersActive );
	Field<Out>( external.fd_forcePowerSelected, internal.raven.fd_forcePowerSelected );
	Field<Out>( external.fd_forcePower, internal.raven.fd_forcePower );
	Field<Out>( external.fd_forcePowerLevel_FP_LEVITATION, internal.raven.fd_forcePowerLevel_FP_LEVITATION );
	Field<Out>( external.fd_forcePowerLevel_FP_SEE, internal.raven.fd_forcePowerLevel_FP_SEE );
	Field<Out>( external.fd_forceJumpZStart, internal.raven.fd_forceJumpZStart );
	Field<Out>( external.fd_forceMindtrickTargetIndex, internal.raven.fd_forceMindtrickTargetIndex );
	Field<Out>( external.fd_forceMindtrickTargetIndex2, internal.raven.fd_forceMindtrickTargetIndex2 );
	Field<Out>( external.fd_forceMindtrickTargetIndex3, internal.raven.fd_forceMindtrickTargetIndex3 );
	Field<Out>( external.fd_forceMindtrickTargetIndex4, internal.raven.fd_forceMindtrickTargetIndex4 );
	Field<Out>( external.fd_forceRageRecoveryTime, internal.raven.fd_forceRageRecoveryTime );
	Field<Out>( external.fd_forceSide, internal.raven.fd_forceSide );
	Field<Out>( external.fd_sentryDeployed, internal.raven.fd_sentryDeployed );
	Field<Out>( external.fd_saberAnimLevel, internal.raven.fd_saberAnimLevel );
	Field<Out>( external.fd_saberDrawAnimLevel, internal.raven.fd_saberDrawAnimLevel );
	Field<Out>( external.forceHandExtend, internal.raven.forceHandExtend );
	Field<Out>( external.forceDodgeAnim, internal.raven.forceDodgeAnim );
	Field<Out>( external.duelIndex, internal.raven.duelIndex );
	Field<Out>( external.duelTime, internal.raven.duelTime );
	Field<Out>( external.duelInProgress, internal.raven.duelInProgress );
	Field<Out>( external.saberAttackChainCount, internal.raven.saberAttackChainCount );
	Field<Out>( external.saberHolstered, internal.raven.saberHolstered );
	Field<Out>( external.usingATST, internal.raven.usingATST );
	Field<Out>( external.atstAltFire, internal.raven.atstAltFire );
	Field<Out>( external.zoomMode, internal.raven.zoomMode );
	Field<Out>( external.zoomTime, internal.raven.zoomTime );
	Field<Out>( external.zoomLocked, internal.raven.zoomLocked );
	Field<Out>( external.zoomFov, internal.raven.zoomFov );
	Field<Out>( external.fallingToDeath, internal.raven.fallingToDeath );
	Field<Out>( external.inAirAnim, internal.raven.inAirAnim );
	Field<Out>( external.dualBlade, internal.raven.dualBlade );
	Field<Out>( external.lastHitLoc, internal.raven.lastHitLoc );
	return true;
}
} // namespace detail

inline bool FromInternal( const playerState_t &source,
	fntech3_jk2_playerState_t &destination ) noexcept
{
	fntech3_jk2_playerState_t temporary{};
	static_assert( sizeof( source.raven.jk2Player ) == sizeof( temporary ) );
	std::memcpy( &temporary, source.raven.jk2Player, sizeof( temporary ) );
	if ( !detail::Player<true>( temporary, source ) ) return false;
	destination = temporary;
	return true;
}

inline bool ToInternal( const fntech3_jk2_playerState_t &source,
	playerState_t &destination ) noexcept
{
	playerState_t temporary{};
	std::memcpy( temporary.raven.jk2Player, &source, sizeof( source ) );
	if ( !detail::Player<false>( source, temporary ) ) return false;
	destination = temporary;
	return true;
}

} // namespace fntech3::jk2::state
#endif
