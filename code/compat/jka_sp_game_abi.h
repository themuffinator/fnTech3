/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_GAME_ABI_H
#define FNTECH3_COMPAT_JKA_SP_GAME_ABI_H
#include <stdint.h>

/* Raven's single-player records, observed in OpenJK's GPLv2 released-source
 * declarations at 1a6a643427aa347553e9073dac5570b33337c4d9. No game logic is
 * incorporated. They are separate from Academy multiplayer's records.
 * Retail jasp.exe's linker confirms solid at 0xb4, client at 0x110, inuse
 * at 0x114, bmodel at 0x120 and contents at 0x13c. GetGameAPI publishes a
 * 1,252-byte complete entity; only the engine-owned prefix is defined here.
 * Trace and user-command records have Source/Unit evidence, not live proof. */
enum {
	FNTECH3_JKA_SP_GENTITY_BYTES = 1252,
	FNTECH3_JKA_SP_ENTITY_STATE_BYTES = 272,
	FNTECH3_JKA_SP_MAX_GENTITIES = 1024,
	FNTECH3_JKA_SP_MAX_G2_COLLISIONS = 16,
	FNTECH3_JKA_SP_SAVED_NO = 0,
	FNTECH3_JKA_SP_SAVED_FULL = 1,
	FNTECH3_JKA_SP_SAVED_AUTO = 2
};
typedef struct {
	int32_t serverTime, buttons;
	uint8_t weapon, padding[3];
	int32_t angles[3];
	uint8_t generic_cmd;
	int8_t forwardmove, rightmove, upmove;
} fntech3_jka_sp_usercmd_t;
typedef struct {
	int32_t trType, trTime, trDuration;
	float trBase[3], trDelta[3];
} fntech3_jka_sp_trajectory_t;
typedef struct {
	int32_t number, eType, eFlags;
	fntech3_jka_sp_trajectory_t pos, apos;
	int32_t time, time2;
	float origin[3], origin2[3], angles[3], angles2[3];
	int32_t otherEntityNum, otherEntityNum2, groundEntityNum;
	int32_t constantLight, loopSound, modelindex, modelindex2, modelindex3;
	int32_t clientNum, frame, solid, event, eventParm, powerups, weapon;
	int32_t legsAnim, legsAnimTimer, torsoAnim, torsoAnimTimer, scale;
	int32_t saberInFlight, saberActive;
	float vehicleAngles[3];
	int32_t vehicleArmor, vehicleNum;
	float modelScale[3];
	int32_t radius, boltInfo, isPortalEnt;
} fntech3_jka_sp_entityState_t;
typedef struct {
	fntech3_jka_sp_entityState_t s;
	uint32_t client_address;
	int32_t inuse, linked, svFlags, bmodel;
	float mins[3], maxs[3];
	int32_t contents;
	float absmin[3], absmax[3], currentOrigin[3], currentAngles[3];
	uint32_t owner_address;
	int32_t ghoul2_handle;
} fntech3_jka_sp_gentity_prefix_t;
typedef struct {
	float normal[3], dist;
	uint8_t type, signbits, padding[2];
} fntech3_jka_sp_cplane_t;
typedef struct {
	float distance;
	int32_t entityNum, modelIndex, polyIndex, surfaceIndex;
	float collisionPosition[3], collisionNormal[3];
	int32_t flags, material, location;
	float barycentricI, barycentricJ;
} fntech3_jka_sp_collision_t;
typedef struct {
	int32_t allsolid, startsolid;
	float fraction, endpos[3];
	fntech3_jka_sp_cplane_t plane;
	int32_t surfaceFlags, contents, entityNum;
	fntech3_jka_sp_collision_t collision[FNTECH3_JKA_SP_MAX_G2_COLLISIONS];
} fntech3_jka_sp_trace_t;
/* Only the common cvar prefix is exposed. Engine linked lists and validation
 * metadata are deliberately not shared with the module. */
typedef struct {
	char *name, *string, *resetString, *latchedString;
	int32_t flags, modified, modificationCount;
	float value;
	int32_t integer;
} fntech3_jka_sp_cvar_prefix_t;
#endif
