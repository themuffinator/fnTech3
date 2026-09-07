/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "game_abi_adapter.h"

#include <string.h>

#define FNTECH3_MAP_Q3_CVAR_BIT( sourceBit, destinationBit ) \
	do { \
		if ( q3Flags & ( sourceBit ) ) { \
			translated |= (uint32_t)( destinationBit ); \
		} \
	} while ( 0 )

uint32_t FnTech3_Q3CvarFlagsToInternal( uint32_t q3Flags ) {
	uint32_t translated = 0;

	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_ARCHIVE, CVAR_ARCHIVE );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_USERINFO, CVAR_USERINFO );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_SERVERINFO, CVAR_SERVERINFO );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_SYSTEMINFO, CVAR_SYSTEMINFO );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_INIT, CVAR_INIT );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_LATCH, CVAR_LATCH );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_ROM, CVAR_ROM );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_USER_CREATED, CVAR_USER_CREATED );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_TEMP, CVAR_TEMP );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_CHEAT, CVAR_CHEAT );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_NORESTART, CVAR_NORESTART );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_SERVER_CREATED, CVAR_SERVER_CREATED );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_VM_CREATED, CVAR_VM_CREATED );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_PROTECTED, CVAR_PROTECTED );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_NODEFAULT, CVAR_NODEFAULT );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_PRIVATE, CVAR_PRIVATE );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_DEVELOPER, CVAR_DEVELOPER );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_NOTABCOMPLETE, CVAR_NOTABCOMPLETE );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_MODIFIED, CVAR_MODIFIED );
	FNTECH3_MAP_Q3_CVAR_BIT( FNTECH3_Q3_CVAR_NONEXISTENT, CVAR_NONEXISTENT );

	return translated;
}

#undef FNTECH3_MAP_Q3_CVAR_BIT

static void FnTech3_CopyFloat3( float destination[3], const float source[3] ) {
	destination[0] = source[0];
	destination[1] = source[1];
	destination[2] = source[2];
}

static void FnTech3_CopyInt32Array( int32_t *destination, const int *source,
	size_t count ) {
	size_t index;

	for ( index = 0; index < count; ++index ) {
		destination[index] = (int32_t)source[index];
	}
}

static void FnTech3_CopyInternalIntArray( int *destination,
	const int32_t *source, size_t count ) {
	size_t index;

	for ( index = 0; index < count; ++index ) {
		destination[index] = (int)source[index];
	}
}

void FnTech3_Q3UsercmdFromInternal( fntech3_q3_usercmd_t *destination,
	const usercmd_t *source ) {
	if ( !destination || !source ) {
		return;
	}

	memset( destination, 0, sizeof( *destination ) );
	destination->serverTime = source->serverTime;
	FnTech3_CopyInt32Array( destination->angles, source->angles, 3 );
	destination->buttons = source->buttons;
	destination->weapon = source->weapon;
	destination->forwardmove = source->forwardmove;
	destination->rightmove = source->rightmove;
	destination->upmove = source->upmove;
}

void FnTech3_InternalUsercmdFromQ3( usercmd_t *destination,
	const fntech3_q3_usercmd_t *source ) {
	if ( !destination || !source ) {
		return;
	}

	memset( destination, 0, sizeof( *destination ) );
	destination->serverTime = source->serverTime;
	FnTech3_CopyInternalIntArray( destination->angles, source->angles, 3 );
	destination->buttons = source->buttons;
	destination->weapon = source->weapon;
	destination->forwardmove = source->forwardmove;
	destination->rightmove = source->rightmove;
	destination->upmove = source->upmove;
}

void FnTech3_Q3TrajectoryFromInternal( fntech3_q3_trajectory_t *destination,
	const trajectory_t *source ) {
	if ( !destination || !source ) {
		return;
	}

	memset( destination, 0, sizeof( *destination ) );
	destination->trType = (fntech3_q3_trType_t)source->trType;
	destination->trTime = source->trTime;
	destination->trDuration = source->trDuration;
	FnTech3_CopyFloat3( destination->trBase, source->trBase );
	FnTech3_CopyFloat3( destination->trDelta, source->trDelta );
}

void FnTech3_InternalTrajectoryFromQ3( trajectory_t *destination,
	const fntech3_q3_trajectory_t *source ) {
	if ( !destination || !source ) {
		return;
	}

	memset( destination, 0, sizeof( *destination ) );
	destination->trType = (trType_t)source->trType;
	destination->trTime = source->trTime;
	destination->trDuration = source->trDuration;
	FnTech3_CopyFloat3( destination->trBase, source->trBase );
	FnTech3_CopyFloat3( destination->trDelta, source->trDelta );
}

void FnTech3_Q3EntityStateFromInternal( fntech3_q3_entityState_t *destination,
	const entityState_t *source ) {
	if ( !destination || !source ) {
		return;
	}

	memset( destination, 0, sizeof( *destination ) );
	destination->number = source->number;
	destination->eType = source->eType;
	destination->eFlags = source->eFlags;
	FnTech3_Q3TrajectoryFromInternal( &destination->pos, &source->pos );
	FnTech3_Q3TrajectoryFromInternal( &destination->apos, &source->apos );
	destination->time = source->time;
	destination->time2 = source->time2;
	FnTech3_CopyFloat3( destination->origin, source->origin );
	FnTech3_CopyFloat3( destination->origin2, source->origin2 );
	FnTech3_CopyFloat3( destination->angles, source->angles );
	FnTech3_CopyFloat3( destination->angles2, source->angles2 );
	destination->otherEntityNum = source->otherEntityNum;
	destination->otherEntityNum2 = source->otherEntityNum2;
	destination->groundEntityNum = source->groundEntityNum;
	destination->constantLight = source->constantLight;
	destination->loopSound = source->loopSound;
	destination->modelindex = source->modelindex;
	destination->modelindex2 = source->modelindex2;
	destination->clientNum = source->clientNum;
	destination->frame = source->frame;
	destination->solid = source->solid;
	destination->event = source->event;
	destination->eventParm = source->eventParm;
	destination->powerups = source->powerups;
	destination->weapon = source->weapon;
	destination->legsAnim = source->legsAnim;
	destination->torsoAnim = source->torsoAnim;
	destination->generic1 = source->generic1;
}

void FnTech3_InternalEntityStateFromQ3( entityState_t *destination,
	const fntech3_q3_entityState_t *source ) {
	if ( !destination || !source ) {
		return;
	}

	memset( destination, 0, sizeof( *destination ) );
	destination->number = source->number;
	destination->eType = source->eType;
	destination->eFlags = source->eFlags;
	FnTech3_InternalTrajectoryFromQ3( &destination->pos, &source->pos );
	FnTech3_InternalTrajectoryFromQ3( &destination->apos, &source->apos );
	destination->time = source->time;
	destination->time2 = source->time2;
	FnTech3_CopyFloat3( destination->origin, source->origin );
	FnTech3_CopyFloat3( destination->origin2, source->origin2 );
	FnTech3_CopyFloat3( destination->angles, source->angles );
	FnTech3_CopyFloat3( destination->angles2, source->angles2 );
	destination->otherEntityNum = source->otherEntityNum;
	destination->otherEntityNum2 = source->otherEntityNum2;
	destination->groundEntityNum = source->groundEntityNum;
	destination->constantLight = source->constantLight;
	destination->loopSound = source->loopSound;
	destination->modelindex = source->modelindex;
	destination->modelindex2 = source->modelindex2;
	destination->clientNum = source->clientNum;
	destination->frame = source->frame;
	destination->solid = source->solid;
	destination->event = source->event;
	destination->eventParm = source->eventParm;
	destination->powerups = source->powerups;
	destination->weapon = source->weapon;
	destination->legsAnim = source->legsAnim;
	destination->torsoAnim = source->torsoAnim;
	destination->generic1 = source->generic1;
}

void FnTech3_Q3PlayerStateFromInternal( fntech3_q3_playerState_t *destination,
	const playerState_t *source ) {
	if ( !destination || !source ) {
		return;
	}

	memset( destination, 0, sizeof( *destination ) );
	destination->commandTime = source->commandTime;
	destination->pm_type = source->pm_type;
	destination->bobCycle = source->bobCycle;
	destination->pm_flags = source->pm_flags & 0xffff;
	destination->pm_time = source->pm_time;
	FnTech3_CopyFloat3( destination->origin, source->origin );
	FnTech3_CopyFloat3( destination->velocity, source->velocity );
	destination->weaponTime = source->weaponTime;
	destination->gravity = source->gravity;
	destination->speed = source->speed;
	FnTech3_CopyInt32Array( destination->delta_angles, source->delta_angles, 3 );
	destination->groundEntityNum = source->groundEntityNum;
	destination->legsTimer = source->legsTimer;
	destination->legsAnim = source->legsAnim;
	destination->torsoTimer = source->torsoTimer;
	destination->torsoAnim = source->torsoAnim;
	destination->movementDir = source->movementDir;
	FnTech3_CopyFloat3( destination->grapplePoint, source->grapplePoint );
	destination->eFlags = source->eFlags;
	destination->eventSequence = source->eventSequence;
	FnTech3_CopyInt32Array( destination->events, source->events,
		FNTECH3_Q3_MAX_PS_EVENTS );
	FnTech3_CopyInt32Array( destination->eventParms, source->eventParms,
		FNTECH3_Q3_MAX_PS_EVENTS );
	destination->externalEvent = source->externalEvent;
	destination->externalEventParm = source->externalEventParm;
	destination->externalEventTime = source->externalEventTime;
	destination->clientNum = source->clientNum;
	destination->weapon = source->weapon;
	destination->weaponstate = source->weaponstate;
	FnTech3_CopyFloat3( destination->viewangles, source->viewangles );
	destination->viewheight = source->viewheight;
	destination->damageEvent = source->damageEvent;
	destination->damageYaw = source->damageYaw;
	destination->damagePitch = source->damagePitch;
	destination->damageCount = source->damageCount;
	FnTech3_CopyInt32Array( destination->stats, source->stats,
		FNTECH3_Q3_MAX_STATS );
	FnTech3_CopyInt32Array( destination->persistant, source->persistant,
		FNTECH3_Q3_MAX_PERSISTANT );
	FnTech3_CopyInt32Array( destination->powerups, source->powerups,
		FNTECH3_Q3_MAX_POWERUPS );
	FnTech3_CopyInt32Array( destination->ammo, source->ammo,
		FNTECH3_Q3_MAX_WEAPONS );
	destination->generic1 = source->generic1;
	destination->loopSound = source->loopSound;
	destination->jumppad_ent = source->jumppad_ent;
	destination->ping = source->ping;
	destination->pmove_framecount = source->pmove_framecount;
	destination->jumppad_frame = source->jumppad_frame;
	destination->entityEventSequence = source->entityEventSequence;
}

void FnTech3_InternalPlayerStateFromQ3( playerState_t *destination,
	const fntech3_q3_playerState_t *source ) {
	if ( !destination || !source ) {
		return;
	}

	memset( destination, 0, sizeof( *destination ) );
	destination->commandTime = source->commandTime;
	destination->pm_type = source->pm_type;
	destination->bobCycle = source->bobCycle;
	destination->pm_flags = source->pm_flags & 0xffff;
	destination->pm_time = source->pm_time;
	FnTech3_CopyFloat3( destination->origin, source->origin );
	FnTech3_CopyFloat3( destination->velocity, source->velocity );
	destination->weaponTime = source->weaponTime;
	destination->gravity = source->gravity;
	destination->speed = source->speed;
	FnTech3_CopyInternalIntArray( destination->delta_angles,
		source->delta_angles, 3 );
	destination->groundEntityNum = source->groundEntityNum;
	destination->legsTimer = source->legsTimer;
	destination->legsAnim = source->legsAnim;
	destination->torsoTimer = source->torsoTimer;
	destination->torsoAnim = source->torsoAnim;
	destination->movementDir = source->movementDir;
	FnTech3_CopyFloat3( destination->grapplePoint, source->grapplePoint );
	destination->eFlags = source->eFlags;
	destination->eventSequence = source->eventSequence;
	FnTech3_CopyInternalIntArray( destination->events, source->events,
		FNTECH3_Q3_MAX_PS_EVENTS );
	FnTech3_CopyInternalIntArray( destination->eventParms, source->eventParms,
		FNTECH3_Q3_MAX_PS_EVENTS );
	destination->externalEvent = source->externalEvent;
	destination->externalEventParm = source->externalEventParm;
	destination->externalEventTime = source->externalEventTime;
	destination->clientNum = source->clientNum;
	destination->weapon = source->weapon;
	destination->weaponstate = source->weaponstate;
	FnTech3_CopyFloat3( destination->viewangles, source->viewangles );
	destination->viewheight = source->viewheight;
	destination->damageEvent = source->damageEvent;
	destination->damageYaw = source->damageYaw;
	destination->damagePitch = source->damagePitch;
	destination->damageCount = source->damageCount;
	FnTech3_CopyInternalIntArray( destination->stats, source->stats,
		FNTECH3_Q3_MAX_STATS );
	FnTech3_CopyInternalIntArray( destination->persistant, source->persistant,
		FNTECH3_Q3_MAX_PERSISTANT );
	FnTech3_CopyInternalIntArray( destination->powerups, source->powerups,
		FNTECH3_Q3_MAX_POWERUPS );
	FnTech3_CopyInternalIntArray( destination->ammo, source->ammo,
		FNTECH3_Q3_MAX_WEAPONS );
	destination->generic1 = source->generic1;
	destination->loopSound = source->loopSound;
	destination->jumppad_ent = source->jumppad_ent;
	destination->ping = source->ping;
	destination->pmove_framecount = source->pmove_framecount;
	destination->jumppad_frame = source->jumppad_frame;
	destination->entityEventSequence = source->entityEventSequence;
}
