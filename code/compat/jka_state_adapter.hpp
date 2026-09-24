/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_STATE_ADAPTER_HPP
#define FNTECH3_JKA_STATE_ADAPTER_HPP

#include "jka_game_abi.h"
#include "rtcw_state_adapter.hpp"

#include <cstdint>
#include <cstring>
#include <type_traits>

/* Field-by-field transfers between the frozen Jedi Academy records and the
 * engine's canonical state.
 *
 * Academy's entity state is ninety-four fields. Twenty-nine are Quake III's,
 * with Quake III's meanings, and go to the canonical record. The other
 * sixty-five are Academy's own and go to the engine's jka extension, which is
 * an engine-owned record and no part of any ABI. Every field is transferred
 * by name, so the accounting stays checkable against jka_wire_layout.def.
 *
 * Nothing is copied wholesale between the two: the external record has its own
 * frozen layout and the internal one is the engine's to change.
 *
 * What the canonical record carries and Academy does not -- armor, location,
 * jumpTime, doubleJumped -- is not written outward and comes back zero. The
 * external record is where an Academy entity lives, so nothing is lost that
 * this title ever set. */
namespace fntech3::jka::state {
namespace detail {

using rtcw::state::detail::Field;

/* Academy's trajectory numbering, measured from its own evaluator. */
inline constexpr trType_t TrajectoryTypes[]{
#define FNTECH3_JKA_TRAJECTORY(reference, canonical, ordinal) canonical,
#include "jka_trajectory_types.def"
#undef FNTECH3_JKA_TRAJECTORY
};
inline constexpr std::int32_t TrajectoryTypeCount =
	sizeof( TrajectoryTypes ) / sizeof( TrajectoryTypes[0] );
static_assert( TrajectoryTypeCount == 7 );
#define FNTECH3_JKA_TRAJECTORY(reference, canonical, ordinal) \
	static_assert( TrajectoryTypes[ordinal] == canonical );
#include "jka_trajectory_types.def"
#undef FNTECH3_JKA_TRAJECTORY
/* The two Academy renumbered. Stated here so that a change to either enum
 * fails the build rather than quietly turning falling into oscillating. */
static_assert( TrajectoryTypes[5] == TR_SINE && TrajectoryTypes[6] == TR_GRAVITY );

template<bool Out, class External, class Internal>
bool Trajectory( External &external, Internal &internal ) noexcept {
	if constexpr ( Out ) {
		/* Read the canonical type as bytes so a value outside the enum is
		 * caught rather than compared as though it were one. */
		std::uint32_t type;
		static_assert( sizeof( type ) == sizeof( internal.trType ) );
		std::memcpy( &type, &internal.trType, sizeof( type ) );
		/* Academy has no trajectory acceleration scalar, so a state carrying
		 * one cannot be published to it. */
		if ( internal.gravity != 0.0f ) return false;
		bool found = false;
		for ( std::int32_t index = 0; index < TrajectoryTypeCount; ++index ) {
			if ( type == static_cast<std::uint32_t>( TrajectoryTypes[index] ) ) {
				external.trType = index;
				found = true;
				break;
			}
		}
		if ( !found ) return false;
	} else {
		if ( external.trType < 0 || external.trType >= TrajectoryTypeCount ) return false;
		internal.trType = TrajectoryTypes[external.trType];
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
	Field<Out>( external.health, internal.health );

	/* Academy's own, in the order its record declares them. */
	Field<Out>( external.eFlags2, internal.raven.eFlags2 );
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
	Field<Out>( external.loopIsSoundset, internal.raven.loopIsSoundset );
	Field<Out>( external.soundSetIndex, internal.raven.soundSetIndex );
	Field<Out>( external.modelGhoul2, internal.raven.modelGhoul2 );
	Field<Out>( external.g2radius, internal.raven.g2radius );
	Field<Out>( external.saberInFlight, internal.raven.saberInFlight );
	Field<Out>( external.saberEntityNum, internal.raven.saberEntityNum );
	Field<Out>( external.saberMove, internal.raven.saberMove );
	Field<Out>( external.forcePowersActive, internal.raven.forcePowersActive );
	Field<Out>( external.saberHolstered, internal.raven.saberHolstered );
	Field<Out>( external.isJediMaster, internal.raven.isJediMaster );
	Field<Out>( external.isPortalEnt, internal.raven.isPortalEnt );
	Field<Out>( external.owner, internal.raven.owner );
	Field<Out>( external.teamowner, internal.raven.teamowner );
	Field<Out>( external.shouldtarget, internal.raven.shouldtarget );
	Field<Out>( external.legsFlip, internal.raven.legsFlip );
	Field<Out>( external.torsoFlip, internal.raven.torsoFlip );
	Field<Out>( external.forceFrame, internal.raven.forceFrame );
	Field<Out>( external.heldByClient, internal.raven.heldByClient );
	Field<Out>( external.ragAttach, internal.raven.ragAttach );
	Field<Out>( external.iModelScale, internal.raven.iModelScale );
	Field<Out>( external.brokenLimbs, internal.raven.brokenLimbs );
	Field<Out>( external.boltToPlayer, internal.raven.boltToPlayer );
	Field<Out>( external.hasLookTarget, internal.raven.hasLookTarget );
	Field<Out>( external.lookTarget, internal.raven.lookTarget );
	Field<Out>( external.customRGBA, internal.raven.customRGBA );
	Field<Out>( external.maxhealth, internal.raven.maxhealth );
	Field<Out>( external.npcSaber1, internal.raven.npcSaber1 );
	Field<Out>( external.npcSaber2, internal.raven.npcSaber2 );
	Field<Out>( external.csSounds_Std, internal.raven.csSounds_Std );
	Field<Out>( external.csSounds_Combat, internal.raven.csSounds_Combat );
	Field<Out>( external.csSounds_Extra, internal.raven.csSounds_Extra );
	Field<Out>( external.csSounds_Jedi, internal.raven.csSounds_Jedi );
	Field<Out>( external.surfacesOn, internal.raven.surfacesOn );
	Field<Out>( external.surfacesOff, internal.raven.surfacesOff );
	Field<Out>( external.boneIndex1, internal.raven.boneIndex1 );
	Field<Out>( external.boneIndex2, internal.raven.boneIndex2 );
	Field<Out>( external.boneIndex3, internal.raven.boneIndex3 );
	Field<Out>( external.boneIndex4, internal.raven.boneIndex4 );
	Field<Out>( external.boneOrient, internal.raven.boneOrient );
	Field<Out>( external.boneAngles1, internal.raven.boneAngles1 );
	Field<Out>( external.boneAngles2, internal.raven.boneAngles2 );
	Field<Out>( external.boneAngles3, internal.raven.boneAngles3 );
	Field<Out>( external.boneAngles4, internal.raven.boneAngles4 );
	Field<Out>( external.NPC_class, internal.raven.NPC_class );
	Field<Out>( external.m_iVehicleNum, internal.raven.m_iVehicleNum );
	Field<Out>( external.userInt1, internal.raven.userInt1 );
	Field<Out>( external.userInt2, internal.raven.userInt2 );
	Field<Out>( external.userInt3, internal.raven.userInt3 );
	Field<Out>( external.userFloat1, internal.raven.userFloat1 );
	Field<Out>( external.userFloat2, internal.raven.userFloat2 );
	Field<Out>( external.userFloat3, internal.raven.userFloat3 );
	Field<Out>( external.userVec1, internal.raven.userVec1 );
	Field<Out>( external.userVec2, internal.raven.userVec2 );
	return true;
}

/* A trace, on its way back to the game.
 *
 * Only this direction exists: the engine traces and the module reads. The
 * record is 48 bytes with a packed head, so every field is placed by name and
 * nothing is copied across. An entity number the record cannot hold is
 * refused rather than truncated into a different entity. */
template<class External, class Internal>
bool TraceOut( External &external, const Internal &internal ) noexcept {
	if ( internal.entityNum < INT16_MIN || internal.entityNum > INT16_MAX ) return false;
	std::uint32_t allsolid, startsolid;
	static_assert( sizeof( allsolid ) == sizeof( internal.allsolid ) );
	std::memcpy( &allsolid, &internal.allsolid, sizeof( allsolid ) );
	std::memcpy( &startsolid, &internal.startsolid, sizeof( startsolid ) );
	/* Raven's flags are one byte each, so a canonical value that is neither
	 * true nor false cannot be published without inventing which it meant. */
	if ( allsolid > 1 || startsolid > 1 ) return false;
	External result{};
	result.allsolid = static_cast<std::uint8_t>( allsolid );
	result.startsolid = static_cast<std::uint8_t>( startsolid );
	result.entityNum = static_cast<std::int16_t>( internal.entityNum );
	result.fraction = internal.fraction;
	for ( int axis = 0; axis < 3; ++axis ) {
		result.endpos[axis] = internal.endpos[axis];
		result.plane.normal[axis] = internal.plane.normal[axis];
	}
	result.plane.dist = internal.plane.dist;
	result.plane.type = internal.plane.type;
	result.plane.signbits = internal.plane.signbits;
	result.surfaceFlags = internal.surfaceFlags;
	result.contents = internal.contents;
	external = result;
	return true;
}

/* One client's input frame.
 *
 * The first twenty-one bytes are Quake III's and mean what Quake III means.
 * The three that follow are Raven's -- a force power, an inventory item and a
 * general command -- where Quake III keeps a second weapon and a field of
 * view, so the three movement bytes after them sit one byte later than Quake
 * III puts them. The engine carries Raven's three and reads none of them.
 *
 * Both directions exist: the game hands the engine a bot's input, and reads a
 * client's back. */
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

/* One client, whole.
 *
 * Academy's player state is 168 fields. Forty-three are Quake III's, at
 * Quake III's widths, and go to the canonical record; the other 124 are
 * Raven's and go to the engine's raven extension. Ammunition is the one
 * field that is neither: this title carries nineteen slots where the
 * canonical record holds sixteen, so the first sixteen keep their
 * canonical meaning and the last three are carried.
 *
 * Every field is transferred by name, so the accounting stays checkable
 * against jka_wire_layout.def, and nothing is copied wholesale. */
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
	Field<Out>( external.stats, internal.stats );
	Field<Out>( external.persistant, internal.persistant );
	Field<Out>( external.powerups, internal.powerups );
	Field<Out>( external.generic1, internal.generic1 );
	Field<Out>( external.loopSound, internal.loopSound );
	Field<Out>( external.jumppad_ent, internal.jumppad_ent );
	Field<Out>( external.ping, internal.ping );
	Field<Out>( external.pmove_framecount, internal.pmove_framecount );
	Field<Out>( external.jumppad_frame, internal.jumppad_frame );
	Field<Out>( external.entityEventSequence, internal.entityEventSequence );

	/* Raven's own, in the order this title's record declares them. */
	Field<Out>( external.moveDir, internal.raven.moveDir );
	Field<Out>( external.weaponChargeTime, internal.raven.weaponChargeTime );
	Field<Out>( external.weaponChargeSubtractTime, internal.raven.weaponChargeSubtractTime );
	Field<Out>( external.basespeed, internal.raven.basespeed );
	Field<Out>( external.slopeRecalcTime, internal.raven.slopeRecalcTime );
	Field<Out>( external.useTime, internal.raven.useTime );
	Field<Out>( external.legsFlip, internal.raven.legsFlip );
	Field<Out>( external.torsoFlip, internal.raven.torsoFlip );
	Field<Out>( external.eFlags2, internal.raven.eFlags2 );
	Field<Out>( external.damageType, internal.raven.damageType );
	Field<Out>( external.painTime, internal.raven.painTime );
	Field<Out>( external.painDirection, internal.raven.painDirection );
	Field<Out>( external.yawAngle, internal.raven.yawAngle );
	Field<Out>( external.yawing, internal.raven.yawing );
	Field<Out>( external.pitchAngle, internal.raven.pitchAngle );
	Field<Out>( external.pitching, internal.raven.pitching );
	Field<Out>( external.lastOnGround, internal.raven.lastOnGround );
	Field<Out>( external.saberInFlight, internal.raven.saberInFlight );
	Field<Out>( external.saberMove, internal.raven.saberMove );
	Field<Out>( external.saberBlocking, internal.raven.saberBlocking );
	Field<Out>( external.saberBlocked, internal.raven.saberBlocked );
	Field<Out>( external.saberLockTime, internal.raven.saberLockTime );
	Field<Out>( external.saberLockEnemy, internal.raven.saberLockEnemy );
	Field<Out>( external.saberLockFrame, internal.raven.saberLockFrame );
	Field<Out>( external.saberLockHits, internal.raven.saberLockHits );
	Field<Out>( external.saberLockHitCheckTime, internal.raven.saberLockHitCheckTime );
	Field<Out>( external.saberLockHitIncrementTime, internal.raven.saberLockHitIncrementTime );
	Field<Out>( external.saberLockAdvance, internal.raven.saberLockAdvance );
	Field<Out>( external.saberEntityNum, internal.raven.saberEntityNum );
	Field<Out>( external.saberEntityDist, internal.raven.saberEntityDist );
	Field<Out>( external.saberEntityState, internal.raven.saberEntityState );
	Field<Out>( external.saberThrowDelay, internal.raven.saberThrowDelay );
	Field<Out>( external.saberCanThrow, internal.raven.saberCanThrow );
	Field<Out>( external.saberDidThrowTime, internal.raven.saberDidThrowTime );
	Field<Out>( external.saberDamageDebounceTime, internal.raven.saberDamageDebounceTime );
	Field<Out>( external.saberHitWallSoundDebounceTime, internal.raven.saberHitWallSoundDebounceTime );
	Field<Out>( external.saberEventFlags, internal.raven.saberEventFlags );
	Field<Out>( external.rocketLockIndex, internal.raven.rocketLockIndex );
	Field<Out>( external.rocketLastValidTime, internal.raven.rocketLastValidTime );
	Field<Out>( external.rocketLockTime, internal.raven.rocketLockTime );
	Field<Out>( external.rocketTargetTime, internal.raven.rocketTargetTime );
	Field<Out>( external.emplacedIndex, internal.raven.emplacedIndex );
	Field<Out>( external.emplacedTime, internal.raven.emplacedTime );
	Field<Out>( external.isJediMaster, internal.raven.isJediMaster );
	Field<Out>( external.forceRestricted, internal.raven.forceRestricted );
	Field<Out>( external.trueJedi, internal.raven.trueJedi );
	Field<Out>( external.trueNonJedi, internal.raven.trueNonJedi );
	Field<Out>( external.saberIndex, internal.raven.saberIndex );
	Field<Out>( external.genericEnemyIndex, internal.raven.genericEnemyIndex );
	Field<Out>( external.droneFireTime, internal.raven.droneFireTime );
	Field<Out>( external.droneExistTime, internal.raven.droneExistTime );
	Field<Out>( external.activeForcePass, internal.raven.activeForcePass );
	Field<Out>( external.hasDetPackPlanted, internal.raven.hasDetPackPlanted );
	Field<Out>( external.holocronsCarried, internal.raven.holocronsCarried );
	Field<Out>( external.holocronCantTouch, internal.raven.holocronCantTouch );
	Field<Out>( external.holocronCantTouchTime, internal.raven.holocronCantTouchTime );
	Field<Out>( external.holocronBits, internal.raven.holocronBits );
	Field<Out>( external.electrifyTime, internal.raven.electrifyTime );
	Field<Out>( external.saberAttackSequence, internal.raven.saberAttackSequence );
	Field<Out>( external.saberIdleWound, internal.raven.saberIdleWound );
	Field<Out>( external.saberAttackWound, internal.raven.saberAttackWound );
	Field<Out>( external.saberBlockTime, internal.raven.saberBlockTime );
	Field<Out>( external.otherKiller, internal.raven.otherKiller );
	Field<Out>( external.otherKillerTime, internal.raven.otherKillerTime );
	Field<Out>( external.otherKillerDebounceTime, internal.raven.otherKillerDebounceTime );
	Field<Out>( external.fd, internal.raven.fd );
	Field<Out>( external.forceJumpFlip, internal.raven.forceJumpFlip );
	Field<Out>( external.forceHandExtend, internal.raven.forceHandExtend );
	Field<Out>( external.forceHandExtendTime, internal.raven.forceHandExtendTime );
	Field<Out>( external.forceRageDrainTime, internal.raven.forceRageDrainTime );
	Field<Out>( external.forceDodgeAnim, internal.raven.forceDodgeAnim );
	Field<Out>( external.quickerGetup, internal.raven.quickerGetup );
	Field<Out>( external.groundTime, internal.raven.groundTime );
	Field<Out>( external.footstepTime, internal.raven.footstepTime );
	Field<Out>( external.otherSoundTime, internal.raven.otherSoundTime );
	Field<Out>( external.otherSoundLen, internal.raven.otherSoundLen );
	Field<Out>( external.forceGripMoveInterval, internal.raven.forceGripMoveInterval );
	Field<Out>( external.forceGripChangeMovetype, internal.raven.forceGripChangeMovetype );
	Field<Out>( external.forceKickFlip, internal.raven.forceKickFlip );
	Field<Out>( external.duelIndex, internal.raven.duelIndex );
	Field<Out>( external.duelTime, internal.raven.duelTime );
	Field<Out>( external.duelInProgress, internal.raven.duelInProgress );
	Field<Out>( external.saberAttackChainCount, internal.raven.saberAttackChainCount );
	Field<Out>( external.saberHolstered, internal.raven.saberHolstered );
	Field<Out>( external.forceAllowDeactivateTime, internal.raven.forceAllowDeactivateTime );
	Field<Out>( external.zoomMode, internal.raven.zoomMode );
	Field<Out>( external.zoomTime, internal.raven.zoomTime );
	Field<Out>( external.zoomLocked, internal.raven.zoomLocked );
	Field<Out>( external.zoomFov, internal.raven.zoomFov );
	Field<Out>( external.zoomLockTime, internal.raven.zoomLockTime );
	Field<Out>( external.fallingToDeath, internal.raven.fallingToDeath );
	Field<Out>( external.useDelay, internal.raven.useDelay );
	Field<Out>( external.inAirAnim, internal.raven.inAirAnim );
	Field<Out>( external.lastHitLoc, internal.raven.lastHitLoc );
	Field<Out>( external.heldByClient, internal.raven.heldByClient );
	Field<Out>( external.ragAttach, internal.raven.ragAttach );
	Field<Out>( external.iModelScale, internal.raven.iModelScale );
	Field<Out>( external.brokenLimbs, internal.raven.brokenLimbs );
	Field<Out>( external.hasLookTarget, internal.raven.hasLookTarget );
	Field<Out>( external.lookTarget, internal.raven.lookTarget );
	Field<Out>( external.customRGBA, internal.raven.customRGBA );
	Field<Out>( external.standheight, internal.raven.standheight );
	Field<Out>( external.crouchheight, internal.raven.crouchheight );
	Field<Out>( external.m_iVehicleNum, internal.raven.m_iVehicleNum );
	Field<Out>( external.vehOrientation, internal.raven.vehOrientation );
	Field<Out>( external.vehBoarding, internal.raven.vehBoarding );
	Field<Out>( external.vehSurfaces, internal.raven.vehSurfaces );
	Field<Out>( external.vehTurnaroundIndex, internal.raven.vehTurnaroundIndex );
	Field<Out>( external.vehTurnaroundTime, internal.raven.vehTurnaroundTime );
	Field<Out>( external.vehWeaponsLinked, internal.raven.vehWeaponsLinked );
	Field<Out>( external.hyperSpaceTime, internal.raven.hyperSpaceTime );
	Field<Out>( external.hyperSpaceAngles, internal.raven.hyperSpaceAngles );
	Field<Out>( external.hackingTime, internal.raven.hackingTime );
	Field<Out>( external.hackingBaseTime, internal.raven.hackingBaseTime );
	Field<Out>( external.jetpackFuel, internal.raven.jetpackFuel );
	Field<Out>( external.cloakFuel, internal.raven.cloakFuel );
	Field<Out>( external.userInt1, internal.raven.userInt1 );
	Field<Out>( external.userInt2, internal.raven.userInt2 );
	Field<Out>( external.userInt3, internal.raven.userInt3 );
	Field<Out>( external.userFloat1, internal.raven.userFloat1 );
	Field<Out>( external.userFloat2, internal.raven.userFloat2 );
	Field<Out>( external.userFloat3, internal.raven.userFloat3 );
	Field<Out>( external.userVec1, internal.raven.userVec1 );
	Field<Out>( external.userVec2, internal.raven.userVec2 );

	/* Nineteen ammunition slots into sixteen and three. */
	for ( int slot = 0; slot < 19; ++slot ) {
		if ( slot < MAX_WEAPONS ) Field<Out>( external.ammo[slot], internal.ammo[slot] );
		else Field<Out>( external.ammo[slot], internal.raven.ammoExtra[slot - MAX_WEAPONS] );
	}
	return true;
}
} // namespace detail

#define JKA_TRANSFER(kind, externalType, internalType) \
inline bool FromInternal( const internalType &source, externalType &destination ) noexcept { \
	externalType temporary{}; \
	if ( !detail::kind<true>( temporary, source ) ) return false; \
	destination = temporary; return true; } \
inline bool ToInternal( const externalType &source, internalType &destination ) noexcept { \
	internalType temporary{}; \
	if ( !detail::kind<false>( source, temporary ) ) return false; \
	destination = temporary; return true; }
JKA_TRANSFER( Entity, fntech3_jka_entityState_t, entityState_t )
JKA_TRANSFER( Command, fntech3_jka_usercmd_t, usercmd_t )
JKA_TRANSFER( Player, fntech3_jka_playerState_t, playerState_t )
#undef JKA_TRANSFER

inline bool FromInternal( const trace_t &source, fntech3_jka_trace_t &destination ) noexcept {
	return detail::TraceOut( destination, source );
}

} // namespace fntech3::jka::state
#endif
