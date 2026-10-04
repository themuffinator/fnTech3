/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JK2_SP_GAME_ABI_H
#define FNTECH3_COMPAT_JK2_SP_GAME_ABI_H
#include <stdint.h>
#include <stddef.h>

/* Retail jk2sp.exe 1.02a delta table 0x4c8688 gives 62 entity members.
 * Its highest member, boltInfo at 248, ends immediately before the game's
 * client pointer at 252. Link/unlink 0x43a620/0x43a5b0, brush-model setup
 * 0x4364f0, snapshot flags 0x43a21e and trace owner checks 0x43ac40 pin the
 * rest of the engine-visible prefix. These are SP records, not JK2 MP. */
#define FNTECH3_JK2_SP_ENTITY_STATE_BYTES 252
#define FNTECH3_JK2_SP_GENTITY_PREFIX_BYTES 352
#define FNTECH3_JK2_SP_MAX_GENTITIES 1024

typedef struct {
	int32_t trType, trTime, trDuration;
	float trBase[3], trDelta[3];
} fntech3_jk2_sp_trajectory_t;

typedef struct {
	int32_t number, eType, eFlags;
	fntech3_jk2_sp_trajectory_t pos, apos;
	int32_t time, time2;
	float origin[3], origin2[3], angles[3], angles2[3];
	int32_t otherEntityNum, otherEntityNum2, groundEntityNum;
	int32_t constantLight, loopSound;
	int32_t modelindex, modelindex2, modelindex3, clientNum, frame;
	int32_t solid, event, eventParm, powerups, weapon;
	int32_t legsAnim, legsAnimTimer, torsoAnim, torsoAnimTimer, scale;
	int32_t saberInFlight, saberActive, vehicleModel;
	float modelScale[3];
	int32_t radius, boltInfo;
} fntech3_jk2_sp_entityState_t;

typedef struct {
	fntech3_jk2_sp_entityState_t s;
	/* Fixed-width native addresses. A client is not a published player
	 * array, and owner is an entity POINTER, never an owner number. */
	uint32_t client;
	int32_t inuse, linked, svFlags, bmodel;
	float mins[3], maxs[3];
	int32_t contents;
	float absmin[3], absmax[3], currentOrigin[3], currentAngles[3];
	uint32_t owner;
	/* Ghoul2 begins at 352 and belongs to the game, not to this prefix. */
} fntech3_jk2_sp_gentity_t;

#ifdef __cplusplus
static_assert( sizeof( fntech3_jk2_sp_trajectory_t ) == 36 );
static_assert( sizeof( fntech3_jk2_sp_entityState_t ) == FNTECH3_JK2_SP_ENTITY_STATE_BYTES );
static_assert( sizeof( fntech3_jk2_sp_gentity_t ) == FNTECH3_JK2_SP_GENTITY_PREFIX_BYTES );
static_assert( offsetof( fntech3_jk2_sp_entityState_t, modelindex ) == 160 );
static_assert( offsetof( fntech3_jk2_sp_entityState_t, solid ) == 180 );
static_assert( offsetof( fntech3_jk2_sp_entityState_t, saberInFlight ) == 220 );
static_assert( offsetof( fntech3_jk2_sp_entityState_t, boltInfo ) == 248 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, client ) == 252 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, inuse ) == 256 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, linked ) == 260 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, svFlags ) == 264 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, bmodel ) == 268 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, mins ) == 272 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, maxs ) == 284 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, contents ) == 296 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, absmin ) == 300 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, absmax ) == 312 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, currentOrigin ) == 324 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, currentAngles ) == 336 );
static_assert( offsetof( fntech3_jk2_sp_gentity_t, owner ) == 348 );
#endif
#endif
