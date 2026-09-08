/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

fnTech3 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.
===========================================================================
*/

#ifndef FNTECH3_COMPAT_Q3_GAME_ABI_H
#define FNTECH3_COMPAT_Q3_GAME_ABI_H

#include <stdint.h>

/*
 * Quake III module-boundary layouts, transcribed from FnQuake3 q_shared.h and
 * game/g_public.h at commit 7d8c9629cdc7979f09697266470359101ffd9eda
 * (blobs 1b85d1aa6aa7b74e70394ae873ed83b80e4776a0 and
 * b08854c0a717681423fd87ae0078d76076a6e5bf).
 *
 * These PODs describe memory owned by a Q3 QVM or legacy native module. They
 * are not fnTech3's internal canonical state. Adapters must copy and translate
 * fields at the boundary instead of casting internal objects to these types.
 *
 * The retail ABI assumes eight-bit bytes, 32-bit signed integers and floats,
 * and normal four-byte structure alignment. No packing directive is applied.
 */

enum {
	FNTECH3_Q3_MAX_CONFIGSTRINGS = 1024,
	FNTECH3_Q3_MAX_GAMESTATE_CHARS = 16000,
	FNTECH3_Q3_MAX_CVAR_VALUE_STRING = 256,
	FNTECH3_Q3_MAX_STATS = 16,
	FNTECH3_Q3_MAX_PERSISTANT = 16,
	FNTECH3_Q3_MAX_POWERUPS = 16,
	FNTECH3_Q3_MAX_WEAPONS = 16,
	FNTECH3_Q3_MAX_PS_EVENTS = 2,
	FNTECH3_Q3_PS_PMOVEFRAMECOUNTBITS = 6,
	FNTECH3_Q3_MAX_CLIENTS = 64,
	FNTECH3_Q3_GENTITYNUM_BITS = 10,
	FNTECH3_Q3_MAX_GENTITIES = 1 << FNTECH3_Q3_GENTITYNUM_BITS,
	FNTECH3_Q3_ENTITYNUM_NONE = FNTECH3_Q3_MAX_GENTITIES - 1,
	FNTECH3_Q3_ENTITYNUM_WORLD = FNTECH3_Q3_MAX_GENTITIES - 2,
	FNTECH3_Q3_ENTITYNUM_MAX_NORMAL = FNTECH3_Q3_MAX_GENTITIES - 2,
	FNTECH3_Q3_SOLID_BMODEL = 0x00ffffff,
	FNTECH3_Q3_MOVE_RUN = 120,
	FNTECH3_Q3_MAX_MAP_AREA_BYTES = 32,
	FNTECH3_Q3_MAX_ENTITIES_IN_SNAPSHOT = 256
};

/* FnQuake3 cgameImport_t extensions from code/cgame/cg_public.h at
 * 7d8c9629cdc7979f09697266470359101ffd9eda. These fixed Q3 ordinals also
 * apply to the values returned by the extension discovery trap. QL inserts
 * advertisement imports at 112..115, so its extension ordinals differ. */
enum {
	FNTECH3_Q3_CG_R_ADDREFENTITYTOSCENE2 = 112,
	FNTECH3_Q3_CG_R_FORCEFIXEDDLIGHTS = 113,
	FNTECH3_Q3_CG_R_ADDLINEARLIGHTTOSCENE = 114,
	FNTECH3_Q3_CG_IS_RECORDING_DEMO = 115,
	FNTECH3_Q3_CG_CVAR_SETDESCRIPTION = 116
};

/* Public cvar bits passed through the Q3 module API. */
#define FNTECH3_Q3_CVAR_ARCHIVE        UINT32_C( 0x00000001 )
#define FNTECH3_Q3_CVAR_USERINFO       UINT32_C( 0x00000002 )
#define FNTECH3_Q3_CVAR_SERVERINFO     UINT32_C( 0x00000004 )
#define FNTECH3_Q3_CVAR_SYSTEMINFO     UINT32_C( 0x00000008 )
#define FNTECH3_Q3_CVAR_INIT           UINT32_C( 0x00000010 )
#define FNTECH3_Q3_CVAR_LATCH          UINT32_C( 0x00000020 )
#define FNTECH3_Q3_CVAR_ROM            UINT32_C( 0x00000040 )
#define FNTECH3_Q3_CVAR_USER_CREATED   UINT32_C( 0x00000080 )
#define FNTECH3_Q3_CVAR_TEMP           UINT32_C( 0x00000100 )
#define FNTECH3_Q3_CVAR_CHEAT          UINT32_C( 0x00000200 )
#define FNTECH3_Q3_CVAR_NORESTART      UINT32_C( 0x00000400 )
#define FNTECH3_Q3_CVAR_SERVER_CREATED UINT32_C( 0x00000800 )
#define FNTECH3_Q3_CVAR_VM_CREATED     UINT32_C( 0x00001000 )
#define FNTECH3_Q3_CVAR_PROTECTED      UINT32_C( 0x00002000 )
#define FNTECH3_Q3_CVAR_NODEFAULT      UINT32_C( 0x00004000 )
#define FNTECH3_Q3_CVAR_PRIVATE        UINT32_C( 0x00008000 )
#define FNTECH3_Q3_CVAR_DEVELOPER      UINT32_C( 0x00010000 )
#define FNTECH3_Q3_CVAR_NOTABCOMPLETE  UINT32_C( 0x00020000 )
#define FNTECH3_Q3_CVAR_ARCHIVE_ND \
	( FNTECH3_Q3_CVAR_ARCHIVE | FNTECH3_Q3_CVAR_NODEFAULT )
#define FNTECH3_Q3_CVAR_MODIFIED       UINT32_C( 0x40000000 )
#define FNTECH3_Q3_CVAR_NONEXISTENT    UINT32_C( 0x80000000 )

/* Configstring transfer layout from the pinned q_shared.h reference above. */
typedef struct {
	int32_t stringOffsets[FNTECH3_Q3_MAX_CONFIGSTRINGS];
	char stringData[FNTECH3_Q3_MAX_GAMESTATE_CHARS];
	int32_t dataCount;
} fntech3_q3_gameState_t;

/* usercmd_t button bits generated or consumed by the engine boundary. */
enum {
	FNTECH3_Q3_BUTTON_ATTACK = 1,
	FNTECH3_Q3_BUTTON_TALK = 2,
	FNTECH3_Q3_BUTTON_USE_HOLDABLE = 4,
	FNTECH3_Q3_BUTTON_GESTURE = 8,
	FNTECH3_Q3_BUTTON_WALKING = 16,
	FNTECH3_Q3_BUTTON_AFFIRMATIVE = 32,
	FNTECH3_Q3_BUTTON_NEGATIVE = 64,
	FNTECH3_Q3_BUTTON_GETFLAG = 128,
	FNTECH3_Q3_BUTTON_GUARDBASE = 256,
	FNTECH3_Q3_BUTTON_PATROL = 512,
	FNTECH3_Q3_BUTTON_FOLLOWME = 1024,
	FNTECH3_Q3_BUTTON_ANY = 2048
};

typedef int32_t fntech3_q3_cvarHandle_t;

typedef struct fntech3_q3_vmCvar_s {
	fntech3_q3_cvarHandle_t handle;
	int32_t modificationCount;
	float value;
	int32_t integer;
	char string[FNTECH3_Q3_MAX_CVAR_VALUE_STRING];
} fntech3_q3_vmCvar_t;

/* A fixed-width integer freezes the four-byte enum representation. */
typedef int32_t fntech3_q3_trType_t;

enum {
	FNTECH3_Q3_TR_STATIONARY = 0,
	FNTECH3_Q3_TR_INTERPOLATE = 1,
	FNTECH3_Q3_TR_LINEAR = 2,
	FNTECH3_Q3_TR_LINEAR_STOP = 3,
	FNTECH3_Q3_TR_SINE = 4,
	FNTECH3_Q3_TR_GRAVITY = 5
};

typedef struct fntech3_q3_trajectory_s {
	fntech3_q3_trType_t trType;
	int32_t trTime;
	int32_t trDuration;
	float trBase[3];
	float trDelta[3];
} fntech3_q3_trajectory_t;

typedef struct fntech3_q3_usercmd_s {
	int32_t serverTime;
	int32_t angles[3];
	int32_t buttons;
	uint8_t weapon;
	int8_t forwardmove;
	int8_t rightmove;
	int8_t upmove;
} fntech3_q3_usercmd_t;

typedef struct fntech3_q3_entityState_s {
	int32_t number;
	int32_t eType;
	int32_t eFlags;
	fntech3_q3_trajectory_t pos;
	fntech3_q3_trajectory_t apos;
	int32_t time;
	int32_t time2;
	float origin[3];
	float origin2[3];
	float angles[3];
	float angles2[3];
	int32_t otherEntityNum;
	int32_t otherEntityNum2;
	int32_t groundEntityNum;
	int32_t constantLight;
	int32_t loopSound;
	int32_t modelindex;
	int32_t modelindex2;
	int32_t clientNum;
	int32_t frame;
	int32_t solid;
	int32_t event;
	int32_t eventParm;
	int32_t powerups;
	int32_t weapon;
	int32_t legsAnim;
	int32_t torsoAnim;
	int32_t generic1;
} fntech3_q3_entityState_t;

typedef struct fntech3_q3_playerState_s {
	int32_t commandTime;
	int32_t pm_type;
	int32_t bobCycle;
	int32_t pm_flags;
	int32_t pm_time;
	float origin[3];
	float velocity[3];
	int32_t weaponTime;
	int32_t gravity;
	int32_t speed;
	int32_t delta_angles[3];
	int32_t groundEntityNum;
	int32_t legsTimer;
	int32_t legsAnim;
	int32_t torsoTimer;
	int32_t torsoAnim;
	int32_t movementDir;
	float grapplePoint[3];
	int32_t eFlags;
	int32_t eventSequence;
	int32_t events[FNTECH3_Q3_MAX_PS_EVENTS];
	int32_t eventParms[FNTECH3_Q3_MAX_PS_EVENTS];
	int32_t externalEvent;
	int32_t externalEventParm;
	int32_t externalEventTime;
	int32_t clientNum;
	int32_t weapon;
	int32_t weaponstate;
	float viewangles[3];
	int32_t viewheight;
	int32_t damageEvent;
	int32_t damageYaw;
	int32_t damagePitch;
	int32_t damageCount;
	int32_t stats[FNTECH3_Q3_MAX_STATS];
	int32_t persistant[FNTECH3_Q3_MAX_PERSISTANT];
	int32_t powerups[FNTECH3_Q3_MAX_POWERUPS];
	int32_t ammo[FNTECH3_Q3_MAX_WEAPONS];
	int32_t generic1;
	int32_t loopSound;
	int32_t jumppad_ent;
	int32_t ping;
	int32_t pmove_framecount;
	int32_t jumppad_frame;
	int32_t entityEventSequence;
} fntech3_q3_playerState_t;

/*
 * The server-visible prefix of a Q3 gentity_t.  entityShared_t deliberately
 * contains its own entityState_t: the outer sharedEntity_t therefore has two
 * states, and server code uses both s and r.s (notably r.s.origin2).
 */
typedef struct fntech3_q3_entityShared_s {
	fntech3_q3_entityState_t s;
	int32_t linked;
	int32_t linkcount;
	int32_t svFlags;
	int32_t singleClient;
	int32_t bmodel;
	float mins[3];
	float maxs[3];
	int32_t contents;
	float absmin[3];
	float absmax[3];
	float currentOrigin[3];
	float currentAngles[3];
	int32_t ownerNum;
} fntech3_q3_entityShared_t;

typedef struct fntech3_q3_sharedEntity_s {
	fntech3_q3_entityState_t s;
	fntech3_q3_entityShared_t r;
} fntech3_q3_sharedEntity_t;

/* Quake III cgame snapshot_t from cg_public.h at the pinned FnQuake3 commit. */
typedef struct fntech3_q3_snapshot_s {
	int32_t snapFlags;
	int32_t ping;
	int32_t serverTime;
	uint8_t areamask[FNTECH3_Q3_MAX_MAP_AREA_BYTES];
	fntech3_q3_playerState_t ps;
	int32_t numEntities;
	fntech3_q3_entityState_t entities[FNTECH3_Q3_MAX_ENTITIES_IN_SNAPSHOT];
	int32_t numServerCommands;
	int32_t serverCommandSequence;
} fntech3_q3_snapshot_t;

#endif /* FNTECH3_COMPAT_Q3_GAME_ABI_H */
