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

/* One client, as this title records one.
 *
 * Read only, and deliberately so. The record is 1,368 bytes and 688 of them
 * have no name in any table this engine has read, so they are not carried.
 * That is why there is no way to write one: a write would put zeroes over
 * half a record it cannot name. Nothing in the engine writes a player state
 * for this title -- the ping is written at its own measured offset, not
 * through here -- and if something ever needs to, the rest of the record
 * has to be measured first.
 *
 * Jedi Academy's player state is carried whole, all 168 fields of it. The
 * difference between the two is the difference between what has been
 * measured for each, and is said here rather than smoothed over. */
inline bool ToInternal( const fntech3_jk2_playerState_t &source,
	playerState_t &destination ) noexcept
{
	playerState_t out{};
	/* Quake III's fields, with Quake III's meanings. */
	detail::Field<false>( source.commandTime, out.commandTime );
	detail::Field<false>( source.pm_type, out.pm_type );
	detail::Field<false>( source.bobCycle, out.bobCycle );
	detail::Field<false>( source.pm_flags, out.pm_flags );
	detail::Field<false>( source.pm_time, out.pm_time );
	detail::Field<false>( source.origin, out.origin );
	detail::Field<false>( source.velocity, out.velocity );
	detail::Field<false>( source.weaponTime, out.weaponTime );
	detail::Field<false>( source.gravity, out.gravity );
	detail::Field<false>( source.speed, out.speed );
	detail::Field<false>( source.delta_angles, out.delta_angles );
	detail::Field<false>( source.groundEntityNum, out.groundEntityNum );
	detail::Field<false>( source.legsTimer, out.legsTimer );
	detail::Field<false>( source.legsAnim, out.legsAnim );
	detail::Field<false>( source.torsoTimer, out.torsoTimer );
	detail::Field<false>( source.torsoAnim, out.torsoAnim );
	detail::Field<false>( source.movementDir, out.movementDir );
	detail::Field<false>( source.eFlags, out.eFlags );
	detail::Field<false>( source.eventSequence, out.eventSequence );
	detail::Field<false>( source.events, out.events );
	detail::Field<false>( source.eventParms, out.eventParms );
	detail::Field<false>( source.externalEvent, out.externalEvent );
	detail::Field<false>( source.externalEventParm, out.externalEventParm );
	detail::Field<false>( source.clientNum, out.clientNum );
	detail::Field<false>( source.weapon, out.weapon );
	detail::Field<false>( source.weaponstate, out.weaponstate );
	detail::Field<false>( source.viewangles, out.viewangles );
	detail::Field<false>( source.viewheight, out.viewheight );
	detail::Field<false>( source.damageEvent, out.damageEvent );
	detail::Field<false>( source.damageYaw, out.damageYaw );
	detail::Field<false>( source.damagePitch, out.damagePitch );
	detail::Field<false>( source.damageCount, out.damageCount );
	detail::Field<false>( source.stats, out.stats );
	detail::Field<false>( source.persistant, out.persistant );
	detail::Field<false>( source.powerups, out.powerups );
	detail::Field<false>( source.ammo, out.ammo );
	detail::Field<false>( source.generic1, out.generic1 );
	detail::Field<false>( source.loopSound, out.loopSound );
	detail::Field<false>( source.jumppad_ent, out.jumppad_ent );
	detail::Field<false>( source.ping, out.ping );

	/* Raven's own. */
	detail::Field<false>( source.weaponChargeTime, out.raven.weaponChargeTime );
	detail::Field<false>( source.weaponChargeSubtractTime, out.raven.weaponChargeSubtractTime );
	detail::Field<false>( source.basespeed, out.raven.basespeed );
	detail::Field<false>( source.damageType, out.raven.damageType );
	detail::Field<false>( source.saberInFlight, out.raven.saberInFlight );
	detail::Field<false>( source.saberActive, out.raven.saberActive );
	detail::Field<false>( source.saberMove, out.raven.saberMove );
	detail::Field<false>( source.saberBlocked, out.raven.saberBlocked );
	detail::Field<false>( source.saberLockTime, out.raven.saberLockTime );
	detail::Field<false>( source.saberLockEnemy, out.raven.saberLockEnemy );
	detail::Field<false>( source.saberLockFrame, out.raven.saberLockFrame );
	detail::Field<false>( source.saberLockAdvance, out.raven.saberLockAdvance );
	detail::Field<false>( source.saberEntityNum, out.raven.saberEntityNum );
	detail::Field<false>( source.saberCanThrow, out.raven.saberCanThrow );
	detail::Field<false>( source.rocketLockIndex, out.raven.rocketLockIndex );
	detail::Field<false>( source.rocketLockTime, out.raven.rocketLockTime );
	detail::Field<false>( source.rocketTargetTime, out.raven.rocketTargetTime );
	detail::Field<false>( source.emplacedIndex, out.raven.emplacedIndex );
	detail::Field<false>( source.isJediMaster, out.raven.isJediMaster );
	detail::Field<false>( source.genericEnemyIndex, out.raven.genericEnemyIndex );
	detail::Field<false>( source.activeForcePass, out.raven.activeForcePass );
	detail::Field<false>( source.hasDetPackPlanted, out.raven.hasDetPackPlanted );
	detail::Field<false>( source.holocronBits, out.raven.holocronBits );
	detail::Field<false>( source.electrifyTime, out.raven.electrifyTime );
	detail::Field<false>( source.fd_forcePowerDebounce_FP_LEVITATION, out.raven.fd_forcePowerDebounce_FP_LEVITATION );
	detail::Field<false>( source.fd_forcePowersKnown, out.raven.fd_forcePowersKnown );
	detail::Field<false>( source.fd_forcePowersActive, out.raven.fd_forcePowersActive );
	detail::Field<false>( source.fd_forcePowerSelected, out.raven.fd_forcePowerSelected );
	detail::Field<false>( source.fd_forcePower, out.raven.fd_forcePower );
	detail::Field<false>( source.fd_forcePowerLevel_FP_LEVITATION, out.raven.fd_forcePowerLevel_FP_LEVITATION );
	detail::Field<false>( source.fd_forcePowerLevel_FP_SEE, out.raven.fd_forcePowerLevel_FP_SEE );
	detail::Field<false>( source.fd_forceJumpZStart, out.raven.fd_forceJumpZStart );
	detail::Field<false>( source.fd_forceMindtrickTargetIndex, out.raven.fd_forceMindtrickTargetIndex );
	detail::Field<false>( source.fd_forceMindtrickTargetIndex2, out.raven.fd_forceMindtrickTargetIndex2 );
	detail::Field<false>( source.fd_forceMindtrickTargetIndex3, out.raven.fd_forceMindtrickTargetIndex3 );
	detail::Field<false>( source.fd_forceMindtrickTargetIndex4, out.raven.fd_forceMindtrickTargetIndex4 );
	detail::Field<false>( source.fd_forceRageRecoveryTime, out.raven.fd_forceRageRecoveryTime );
	detail::Field<false>( source.fd_forceSide, out.raven.fd_forceSide );
	detail::Field<false>( source.fd_sentryDeployed, out.raven.fd_sentryDeployed );
	detail::Field<false>( source.fd_saberAnimLevel, out.raven.fd_saberAnimLevel );
	detail::Field<false>( source.fd_saberDrawAnimLevel, out.raven.fd_saberDrawAnimLevel );
	detail::Field<false>( source.forceHandExtend, out.raven.forceHandExtend );
	detail::Field<false>( source.forceDodgeAnim, out.raven.forceDodgeAnim );
	detail::Field<false>( source.duelIndex, out.raven.duelIndex );
	detail::Field<false>( source.duelTime, out.raven.duelTime );
	detail::Field<false>( source.duelInProgress, out.raven.duelInProgress );
	detail::Field<false>( source.saberAttackChainCount, out.raven.saberAttackChainCount );
	detail::Field<false>( source.saberHolstered, out.raven.saberHolstered );
	detail::Field<false>( source.usingATST, out.raven.usingATST );
	detail::Field<false>( source.atstAltFire, out.raven.atstAltFire );
	detail::Field<false>( source.zoomMode, out.raven.zoomMode );
	detail::Field<false>( source.zoomTime, out.raven.zoomTime );
	detail::Field<false>( source.zoomLocked, out.raven.zoomLocked );
	detail::Field<false>( source.zoomFov, out.raven.zoomFov );
	detail::Field<false>( source.fallingToDeath, out.raven.fallingToDeath );
	detail::Field<false>( source.inAirAnim, out.raven.inAirAnim );
	detail::Field<false>( source.dualBlade, out.raven.dualBlade );
	detail::Field<false>( source.lastHitLoc, out.raven.lastHitLoc );
	destination = out;
	return true;
}

} // namespace fntech3::jk2::state
#endif
