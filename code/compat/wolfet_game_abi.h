/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_GAME_ABI_H
#define FNTECH3_WOLFET_GAME_ABI_H

#include <stdint.h>

/* Independently declared interoperability records, never engine storage.
 * Layout/encoding observations: docs/fntech3/2026-09-17-wolfet-contracts.md.
 * The ET GPLv3 implementation is a reference, not incorporated code. */
enum {
	FNTECH3_WOLFET_PROTOCOL = 84,
	FNTECH3_WOLFET_MAX_CLIENTS = 64,
	FNTECH3_WOLFET_MAX_CONFIGSTRINGS = 1024,
	FNTECH3_WOLFET_MAX_GAMESTATE_CHARS = 16000,
	FNTECH3_WOLFET_MAX_ENTITIES_IN_SNAPSHOT = 512,
	FNTECH3_WOLFET_GENTITYNUM_BITS = 10,
	/* Largest module-supplied AAS result count fnTech3 will accept in one
	 * query; the original engine trusts the caller without a bound. */
	FNTECH3_WOLFET_MAX_AAS_QUERY = 4096,
	/* ET limits chat records to 150 bytes where the engine uses 256. */
	FNTECH3_WOLFET_MAX_MESSAGE_SIZE = 150,
	FNTECH3_WOLFET_MAX_MATCHVARIABLES = 8,
	FNTECH3_WOLFET_CS_PLAYERS = 689,
	FNTECH3_WOLFET_CS_WOLFINFO = 21,
	FNTECH3_WOLFET_UI_API_VERSION = 4,
	FNTECH3_WOLFET_CGAME_API_VERSION = 3,
	FNTECH3_WOLFET_GAME_API_VERSION = 8,
	FNTECH3_WOLFET_SVF_IGNOREBMODELEXTENTS = 0x00004000,
	FNTECH3_WOLFET_SVF_SELF_PORTAL = 0x00008000,
	FNTECH3_WOLFET_SVF_SELF_PORTAL_EXCLUSIVE = 0x00010000
};

typedef struct {
	int32_t trType, trTime, trDuration;
	float trBase[3], trDelta[3];
} fntech3_wolfet_trajectory_t;

typedef struct {
	int32_t handle, modificationCount;
	float value;
	int32_t integer;
	char string[256];
} fntech3_wolfet_vmCvar_t;

typedef struct {
	int32_t tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
} fntech3_wolfet_qtime_t;

typedef struct {
	int32_t type, subtype, intvalue;
	float floatvalue;
	char string[1024];
	int32_t line, linescrossed;
} fntech3_wolfet_pc_token_t;

typedef struct {
	float normal[3], dist;
	uint8_t type, signbits, pad[2];
} fntech3_wolfet_plane_t;

typedef struct {
	int32_t allsolid, startsolid;
	float fraction, endpos[3];
	fntech3_wolfet_plane_t plane;
	int32_t surfaceFlags, contents, entityNum;
} fntech3_wolfet_trace_t;

typedef struct {
	int32_t serverTime;
	uint8_t buttons, wbuttons, weapon, flags;
	int32_t angles[3];
	int8_t forwardmove, rightmove, upmove;
	uint8_t doubleTap, identClient;
} fntech3_wolfet_usercmd_t;

typedef struct {
	int32_t number, eType, eFlags;
	fntech3_wolfet_trajectory_t pos, apos;
	int32_t time, time2;
	float origin[3], origin2[3], angles[3], angles2[3];
	int32_t otherEntityNum, otherEntityNum2, groundEntityNum;
	int32_t constantLight, dl_intensity, loopSound;
	int32_t modelindex, modelindex2, clientNum, frame, solid;
	int32_t event, eventParm, eventSequence, events[4], eventParms[4];
	int32_t powerups, weapon, legsAnim, torsoAnim, density, dmgFlags;
	int32_t onFireStart, onFireEnd, nextWeapon, teamNum;
	int32_t effect1Time, effect2Time, effect3Time, aiState, animMovetype;
} fntech3_wolfet_entityState_t;

typedef struct {
	int32_t commandTime, pm_type, bobCycle, pm_flags, pm_time;
	float origin[3], velocity[3];
	int32_t weaponTime, weaponDelay, grenadeTimeLeft, gravity;
	float leanf;
	int32_t speed, delta_angles[3], groundEntityNum;
	int32_t legsTimer, legsAnim, torsoTimer, torsoAnim, movementDir;
	int32_t eFlags, eventSequence, events[4], eventParms[4];
	int32_t oldEventSequence, externalEvent, externalEventParm, externalEventTime;
	int32_t clientNum, weapon, weaponstate, item;
	float viewangles[3];
	int32_t viewheight, damageEvent, damageYaw, damagePitch, damageCount;
	int32_t stats[16], persistant[16], powerups[16], ammo[64], ammoclip[64];
	int32_t holdable[16], holding, weapons[2];
	float mins[3], maxs[3], crouchMaxZ;
	float crouchViewHeight, standViewHeight, deadViewHeight;
	float runSpeedScale, sprintSpeedScale, crouchSpeedScale;
	int32_t viewlocked, viewlocked_entNum;
	float friction;
	int32_t nextWeapon, teamNum, onFireStart, serverCursorHint, serverCursorHintVal;
	fntech3_wolfet_trace_t serverCursorHintTrace;
	int32_t ping, pmove_framecount, entityEventSequence, sprintExertTime;
	int32_t classWeaponTime, jumpTime, weapAnim, releasedFire;
	float aimSpreadScaleFloat;
	int32_t aimSpreadScale, lastFireTime, quickGrenTime, leanStopDebounceTime;
	int32_t weapHeat[64], curWeapHeat, identifyClient, identifyClientHealth, aiState;
} fntech3_wolfet_playerState_t;

typedef struct {
	int32_t linked, linkcount, svFlags, singleClient, bmodel;
	float mins[3], maxs[3];
	int32_t contents;
	float absmin[3], absmax[3], currentOrigin[3], currentAngles[3];
	int32_t ownerNum, eventTime, worldflags, snapshotCallback;
} fntech3_wolfet_entityShared_t;

typedef struct {
	fntech3_wolfet_entityState_t s;
	fntech3_wolfet_entityShared_t r;
} fntech3_wolfet_sharedEntity_t;

typedef struct {
	int32_t stringOffsets[FNTECH3_WOLFET_MAX_CONFIGSTRINGS];
	char stringData[FNTECH3_WOLFET_MAX_GAMESTATE_CHARS];
	int32_t dataCount;
} fntech3_wolfet_gameState_t;

/* ET chat-match record. Its variables hold raw pointers into the module's own
 * copy of the matched string, where this engine stores byte offsets, and its
 * string is 150 bytes where the engine keeps 256. */
typedef struct {
	uint32_t ptr;
	int32_t length;
} fntech3_wolfet_bot_matchvariable_t;

typedef struct {
	char string[FNTECH3_WOLFET_MAX_MESSAGE_SIZE];
	int32_t type, subtype;
	fntech3_wolfet_bot_matchvariable_t variables[FNTECH3_WOLFET_MAX_MATCHVARIABLES];
} fntech3_wolfet_bot_match_t;

/* ET botlib goal, movement, console-message and weapon records, independently
 * declared from its game/be_ai_*.h. ET's goal carries two fields RTCW lacks and
 * its console message is limited to 150 bytes, so none of these may be shared
 * with another profile's records or copied straight into engine storage. */
typedef struct {
	float origin[3];
	int32_t areanum;
	float mins[3], maxs[3];
	int32_t entitynum, number, flags, iteminfo, urgency, goalEndTime;
} fntech3_wolfet_bot_goal_t;

typedef struct {
	float origin[3], velocity[3], viewoffset[3];
	int32_t entitynum, client;
	float thinktime;
	int32_t presencetype;
	float viewangles[3];
	int32_t or_moveflags, areanum;
} fntech3_wolfet_bot_initmove_t;

typedef struct {
	int32_t failure, type, blocked, blockentity, traveltype, flags, weapon;
	float movedir[3], ideal_viewangles[3];
} fntech3_wolfet_bot_moveresult_t;

typedef struct {
	int32_t handle;
	float time;
	int32_t type;
	char message[FNTECH3_WOLFET_MAX_MESSAGE_SIZE];
	uint32_t prev, next;
} fntech3_wolfet_bot_consolemessage_t;

/* ET botlib entity record, independently declared from its game/botlib.h. */
typedef struct {
	int32_t type, flags;
	float origin[3], angles[3], old_origin[3], mins[3], maxs[3];
	int32_t groundent, solid, modelindex, modelindex2, frame, event, eventParm;
	int32_t powerups, weapon, legsAnim, torsoAnim;
} fntech3_wolfet_bot_entitystate_t;

/* ET botlib input record. ET shares RTCW's 40-byte layout and adds ACTION_PRONE
 * as the fifteenth external action bit (botlib.h ACTION_PRONE 16384). */
typedef struct {
	float thinktime;
	float dir[3];
	float speed;
	float viewangles[3];
	int32_t actionflags, weapon;
} fntech3_wolfet_bot_input_t;

typedef struct {
	int32_t snapFlags, ping, serverTime;
	uint8_t areamask[32];
	fntech3_wolfet_playerState_t ps;
	int32_t numEntities;
	fntech3_wolfet_entityState_t entities[FNTECH3_WOLFET_MAX_ENTITIES_IN_SNAPSHOT];
	int32_t numServerCommands, serverCommandSequence;
} fntech3_wolfet_snapshot_t;

#endif
