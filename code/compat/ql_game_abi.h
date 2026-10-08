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

#ifndef FNTECH3_COMPAT_QL_GAME_ABI_H
#define FNTECH3_COMPAT_QL_GAME_ABI_H

#include <stdint.h>

/*
 * Quake Live module-boundary layouts, transcribed from FnQL q_shared.h and
 * game/g_public.h at commit fce44fe714564621e6ceb79e3da7c53089646af2
 * (blobs 8707c0d210f0093884be004ea9844bb96d4bf15c and
 * 34047ce66dea935af9f0a0661b104dcedf74a0f6).
 * The 0x250-byte retail snapshot player-state envelope is independently
 * recorded by code/client/cl_cgame.cpp at that commit.
 *
 * These PODs describe memory copied to or supplied by retail QL modules. They
 * are not fnTech3's internal canonical state. Adapters must copy and translate
 * fields at the boundary instead of casting internal objects to these types.
 *
 * The retail ABI assumes eight-bit bytes, 32-bit signed integers and floats,
 * and normal four-byte structure alignment. No packing directive is applied.
 */

enum {
	FNTECH3_QL_MAX_CONFIGSTRINGS = 1024,
	FNTECH3_QL_MAX_GAMESTATE_CHARS = 16000,
	FNTECH3_QL_MAX_CVAR_VALUE_STRING = 256,
	FNTECH3_QL_MAX_STATS = 16,
	FNTECH3_QL_MAX_PERSISTANT = 16,
	FNTECH3_QL_MAX_POWERUPS = 16,
	FNTECH3_QL_MAX_WEAPONS = 16,
	FNTECH3_QL_MAX_PS_EVENTS = 2,
	FNTECH3_QL_PS_PMOVEFRAMECOUNTBITS = 6,
	FNTECH3_QL_MAX_CLIENTS = 64,
	FNTECH3_QL_GENTITYNUM_BITS = 10,
	FNTECH3_QL_MAX_GENTITIES = 1 << FNTECH3_QL_GENTITYNUM_BITS,
	FNTECH3_QL_ENTITYNUM_NONE = FNTECH3_QL_MAX_GENTITIES - 1,
	FNTECH3_QL_ENTITYNUM_WORLD = FNTECH3_QL_MAX_GENTITIES - 2,
	FNTECH3_QL_ENTITYNUM_MAX_NORMAL = FNTECH3_QL_MAX_GENTITIES - 2,
	FNTECH3_QL_SOLID_BMODEL = 0x00ffffff,
	FNTECH3_QL_MOVE_RUN = 120,
	FNTECH3_QL_MAX_MAP_AREA_BYTES = 32,

	/* The reconstructed, source-known player-state bytes below are 0x1f8. */
	FNTECH3_QL_PLAYERSTATE_KNOWN_BYTES = 0x1f8,

	/*
	 * The retail native snapshot reserves this larger state region. Its last
	 * 0x58 bytes are the game module's own state, never transmitted; the
	 * retail client hands them over as zeroes
	 * (docs/fntech3/2026-10-08-ql-snapshot-tail.md).
	 */
	FNTECH3_QL_RETAIL_SNAPSHOT_PLAYERSTATE_BYTES = 0x250
};

/* Public retail cvar bits passed through the structured QL module API. */
#define FNTECH3_QL_CVAR_ARCHIVE              UINT32_C( 0x00000001 )
#define FNTECH3_QL_CVAR_USERINFO             UINT32_C( 0x00000002 )
#define FNTECH3_QL_CVAR_SERVERINFO           UINT32_C( 0x00000004 )
#define FNTECH3_QL_CVAR_SYSTEMINFO           UINT32_C( 0x00000008 )
#define FNTECH3_QL_CVAR_INIT                 UINT32_C( 0x00000010 )
#define FNTECH3_QL_CVAR_LATCH                UINT32_C( 0x00000020 )
#define FNTECH3_QL_CVAR_ROM                  UINT32_C( 0x00000040 )
#define FNTECH3_QL_CVAR_USER_CREATED         UINT32_C( 0x00000080 )
#define FNTECH3_QL_CVAR_TEMP                 UINT32_C( 0x00000100 )
#define FNTECH3_QL_CVAR_CHEAT                UINT32_C( 0x00000200 )
#define FNTECH3_QL_CVAR_NORESTART            UINT32_C( 0x00000400 )
#define FNTECH3_QL_CVAR_PROTECTED            UINT32_C( 0x00000800 )
#define FNTECH3_QL_CVAR_VM_CREATED           UINT32_C( 0x00001000 )
#define FNTECH3_QL_CVAR_BOUNDED_DISCRETE     UINT32_C( 0x00002000 )
#define FNTECH3_QL_CVAR_PMOVE_SETTINGS       UINT32_C( 0x00004000 )
#define FNTECH3_QL_CVAR_WEAPON_RELOAD        UINT32_C( 0x00008000 )
#define FNTECH3_QL_CVAR_PLAYER_APPEARANCE    UINT32_C( 0x00010000 )
#define FNTECH3_QL_CVAR_ARMOR_TIERED         UINT32_C( 0x00020000 )
#define FNTECH3_QL_CVAR_CLOUD                UINT32_C( 0x00080000 )
#define FNTECH3_QL_CVAR_GAMERULE             UINT32_C( 0x00100000 )
#define FNTECH3_QL_CVAR_MODIFIED             UINT32_C( 0x40000000 )
#define FNTECH3_QL_CVAR_NONEXISTENT          UINT32_C( 0x80000000 )

/* Renderer config: pinned reference layout; external enums are 32-bit scalars. */
typedef struct {
	char renderer_string[1024], vendor_string[1024], version_string[1024];
	char extensions_string[8192];
	int32_t maxTextureSize;
	int32_t maxActiveTextures;
	int32_t colorBits;
	int32_t depthBits;
	int32_t stencilBits;
	int32_t driverType;
	int32_t hardwareType;
	int32_t deviceSupportsGamma;
	int32_t textureCompression;
	int32_t textureEnvAddAvailable;
	int32_t multitextureAvailable;
	int32_t vidWidth;
	int32_t vidHeight;
	float windowAspect;
	int32_t displayFrequency;
	int32_t isFullscreen;
	int32_t stereoEnabled;
} fntech3_ql_glconfig_t;

/* Configstring transfer layout from the pinned q_shared.h reference above. */
typedef struct {
	int32_t stringOffsets[FNTECH3_QL_MAX_CONFIGSTRINGS];
	char stringData[FNTECH3_QL_MAX_GAMESTATE_CHARS];
	int32_t dataCount;
} fntech3_ql_gameState_t;

/* usercmd_t button bits generated or consumed by the engine boundary. */
enum {
	FNTECH3_QL_BUTTON_ATTACK = 1,
	FNTECH3_QL_BUTTON_TALK = 2,
	FNTECH3_QL_BUTTON_USE_HOLDABLE = 4,
	FNTECH3_QL_BUTTON_GESTURE = 8,
	FNTECH3_QL_BUTTON_WALKING = 16,
	FNTECH3_QL_BUTTON_AFFIRMATIVE = 32,
	FNTECH3_QL_BUTTON_NEGATIVE = 64,
	FNTECH3_QL_BUTTON_GETFLAG = 128,
	FNTECH3_QL_BUTTON_GUARDBASE = 256,
	FNTECH3_QL_BUTTON_PATROL = 512,
	FNTECH3_QL_BUTTON_FOLLOWME = 1024,
	FNTECH3_QL_BUTTON_ANY = 2048
};

typedef int32_t fntech3_ql_cvarHandle_t;

typedef struct fntech3_ql_vmCvar_s {
	fntech3_ql_cvarHandle_t handle;
	int32_t modificationCount;
	float value;
	int32_t integer;
	int32_t flags;
	char string[FNTECH3_QL_MAX_CVAR_VALUE_STRING];
} fntech3_ql_vmCvar_t;

/* A fixed-width integer freezes the four-byte enum representation. */
typedef int32_t fntech3_ql_trType_t;

enum {
	FNTECH3_QL_TR_STATIONARY = 0,
	FNTECH3_QL_TR_INTERPOLATE = 1,
	FNTECH3_QL_TR_LINEAR = 2,
	FNTECH3_QL_TR_LINEAR_STOP = 3,
	FNTECH3_QL_TR_SINE = 4,
	FNTECH3_QL_TR_GRAVITY = 5,
	FNTECH3_QL_TR_ACCEL = 6
};

typedef struct fntech3_ql_trajectory_s {
	fntech3_ql_trType_t trType;
	int32_t trTime;
	int32_t trDuration;
	float trBase[3];
	float trDelta[3];
	float gravity;
} fntech3_ql_trajectory_t;

typedef struct fntech3_ql_usercmd_s {
	int32_t serverTime;
	int32_t angles[3];
	int32_t buttons;
	uint8_t weapon;
	uint8_t weaponPrimary;
	uint8_t fov;
	int8_t forwardmove;
	int8_t rightmove;
	int8_t upmove;
} fntech3_ql_usercmd_t;

typedef struct fntech3_ql_entityState_s {
	int32_t number;
	int32_t eType;
	int32_t eFlags;
	fntech3_ql_trajectory_t pos;
	fntech3_ql_trajectory_t apos;
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
	int32_t health;
	int32_t armor;
	int32_t weapon;
	int32_t location;
	int32_t legsAnim;
	int32_t torsoAnim;
	int32_t generic1;
	int32_t jumpTime;
	int32_t doubleJumped;
} fntech3_ql_entityState_t;

/*
 * This is the exact 0x1f8 source-known prefix used by current FnQL. Retail
 * native snapshots reserve 0x250 bytes for player state. Bytes 0x1f8..0x24f
 * belong to the retail game module (its medal and scoring bookkeeping): the
 * engine copies them into server frames but never encodes them, and every
 * snapshot the retail client hands its client game holds them as zeroes
 * (docs/fntech3/2026-10-08-ql-snapshot-tail.md). This type does not name
 * them, and adapters zero them on the client.
 */
typedef struct fntech3_ql_playerState_s {
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
	int32_t events[FNTECH3_QL_MAX_PS_EVENTS];
	int32_t eventParms[FNTECH3_QL_MAX_PS_EVENTS];
	int32_t externalEvent;
	int32_t externalEventParm;
	int32_t clientNum;
	int32_t location;
	int32_t weapon;
	int32_t weaponPrimary;
	int32_t weaponstate;
	int32_t fov;
	float viewangles[3];
	int32_t viewheight;
	int32_t damageEvent;
	int32_t damageYaw;
	int32_t damagePitch;
	int32_t damageCount;
	int32_t stats[FNTECH3_QL_MAX_STATS];
	int32_t persistant[FNTECH3_QL_MAX_PERSISTANT];
	int32_t powerups[FNTECH3_QL_MAX_POWERUPS];
	int32_t ammo[FNTECH3_QL_MAX_WEAPONS];
	int32_t generic1;
	int32_t loopSound;
	int32_t jumppad_ent;
	int32_t jumpTime;
	int32_t doubleJumped;
	int32_t crouchTime;
	int32_t crouchSlideTime;
	int8_t forwardmove;
	int8_t rightmove;
	int8_t upmove;
	int8_t commandMirrorPad;
	int32_t ping;
	int32_t pmove_framecount;
	int32_t jumppad_frame;
	int32_t entityEventSequence;
	int32_t externalEventTime;
	int32_t armorTier;
} fntech3_ql_playerState_t;

/*
 * The server-visible prefix of a QL gentity_t.  As in Q3, r has an embedded
 * state of its own; it must not be flattened into the outer state.
 */
typedef struct fntech3_ql_entityShared_s {
	fntech3_ql_entityState_t s;
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
} fntech3_ql_entityShared_t;

typedef struct fntech3_ql_sharedEntity_s {
	fntech3_ql_entityState_t s;
	fntech3_ql_entityShared_t r;
} fntech3_ql_sharedEntity_t;

/* Renderer records from renderercommon/tr_types.h at the pinned FnQL commit.
 * Keep a distinct QL boundary type even where the Q3 field layout agrees.
 * shaderTimeBits is copied without converting its scalar representation. */
typedef struct {
	int32_t reType, renderfx, hModel;
	float lightingOrigin[3], shadowPlane, axis[3][3];
	int32_t nonNormalizedAxes;
	float origin[3];
	int32_t frame;
	float oldorigin[3];
	int32_t oldframe;
	float backlerp;
	int32_t skinNum, customSkin, customShader;
	uint8_t shaderRGBA[4];
	float shaderTexCoord[2];
	uint32_t shaderTimeBits;
	float radius, rotation;
} fntech3_ql_refEntity_t;

typedef struct {
	int32_t x, y, width, height;
	float fov_x, fov_y, vieworg[3], viewaxis[3][3];
	int32_t time, rdflags;
	uint8_t areamask[FNTECH3_QL_MAX_MAP_AREA_BYTES];
	char text[8][32];
} fntech3_ql_refdef_t;

#endif /* FNTECH3_COMPAT_QL_GAME_ABI_H */
