/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3TEST_GAME_ABI_H
#define FNTECH3_Q3TEST_GAME_ABI_H

#include <stddef.h>
#include <stdint.h>

/* Independently declared Win32 Q3Test game-module records: the tables each
 * release's quake3.exe builds for its qagamex86.dll and the records that
 * cross that boundary. They are external storage, never aliases of engine
 * records. The releases fall into four families (docs/fntech3/Q3TEST.md):
 *
 *   A  1.02, 1.03  GetGameAPI API 4, 33 imports, 20-word exports,
 *                  SpawnEntities; clients named by entity pointer, client n
 *                  is entity n + 1 and the world is entity 0
 *   B  1.05        API 4, 33 imports, 18-word exports, Init(map, ents, time,
 *                  seed); otherwise as A
 *   C  1.06, 1.07  API 6, 31 imports, 14-word exports; clients named by
 *                  number, client n is entity n, the world 1022, none 1023
 *   D  1.08        as C with GetServerinfo inserted and two bot imports
 *                  appended: 34 imports
 *
 * Every import and export is __cdecl. Addresses in the comments are in the
 * named release's quake3.exe unless they carry the DLL base 0x2000xxxx. The
 * native tables may only be called by an x86 host. */

#ifdef _MSC_VER
#define FNTECH3_Q3TEST_CALL __cdecl
#elif defined(__i386__)
#define FNTECH3_Q3TEST_CALL __attribute__((cdecl))
#else
#define FNTECH3_Q3TEST_CALL
#endif

enum {
	FNTECH3_Q3TEST_GAME_API_AB = 4,
	FNTECH3_Q3TEST_GAME_API_CD = 6,
	FNTECH3_Q3TEST_GAME_IMPORTS_AB = 33,
	FNTECH3_Q3TEST_GAME_IMPORTS_C = 31,
	FNTECH3_Q3TEST_GAME_IMPORTS_D = 34,
	FNTECH3_Q3TEST_ENTITY_STATE_BYTES = 204,
	/* The engine-visible prefix of every gentity_t; the rest is the game's. */
	FNTECH3_Q3TEST_ENTITY_PREFIX_BYTES = 308,
	FNTECH3_Q3TEST_ENTITY_BYTES_AB = 612,
	FNTECH3_Q3TEST_ENTITY_BYTES_CD = 580,
	FNTECH3_Q3TEST_TRACE_BYTES = 56,
	FNTECH3_Q3TEST_MAX_GENTITIES = 1024,
	FNTECH3_Q3TEST_MAX_CONFIGSTRINGS = 1024,
	FNTECH3_Q3TEST_MAX_CLIENTS = 128,
	/* C/D trace results: the world after a world trace, none on a clean one
	 * (1.06 SV_Trace 0x41f557..0x41f566). A/B report entity 0 for both. */
	FNTECH3_Q3TEST_WORLD_CD = 1022,
	FNTECH3_Q3TEST_NONE_CD = 1023,
	/* svFlags the engine tests (1.02 0x41d5bc, 0x419c7b, 0x41d5ec, 0x41d6af). */
	FNTECH3_Q3TEST_SVF_NOCLIENT = 0x01,
	FNTECH3_Q3TEST_SVF_BOT = 0x08,
	FNTECH3_Q3TEST_SVF_BROADCAST = 0x20,
	FNTECH3_Q3TEST_SVF_PORTAL = 0x40
};

typedef struct fntech3_q3test_trajectory_s {
	int32_t trType, trTime, trDuration;
	float trBase[3], trDelta[3];
} fntech3_q3test_trajectory_t;

/* 204 bytes in every release (SV_CreateBaseline copies 51 dwords, 1.02
 * 0x41bb55). The table order on the wire differs from this memory order. */
typedef struct fntech3_q3test_entityState_s {
	int32_t number;
	int32_t eType, eFlags;
	fntech3_q3test_trajectory_t pos, apos;
	int32_t time, time2;
	float origin[3], origin2[3], angles[3], angles2[3];
	int32_t otherEntityNum, otherEntityNum2, groundEntityNum;
	int32_t constantLight, loopSound;
	int32_t modelindex, modelindex2;
	int32_t clientNum; /* "skinNum" in the 1.02/1.03 delta table */
	int32_t frame, solid, event, eventParm;
	int32_t powerups, weapon, legsAnim, torsoAnim;
} fntech3_q3test_entityState_t;

/* The first 308 bytes of every gentity_t, identical in all releases
 * (1.02 SV_LinkEntity 0x41de30). owner is a pointer, not a number. */
typedef struct fntech3_q3test_entity_s {
	fntech3_q3test_entityState_t s;
	uint32_t client;        /* gclient_t *, whose first member is the player state */
	int32_t inuse;
	int32_t linked;
	int32_t linkcount;
	int32_t svFlags;
	int32_t bmodel;
	float mins[3], maxs[3];
	int32_t contents;
	float absmin[3], absmax[3];
	float currentOrigin[3], currentAngles[3];
	uint32_t owner;         /* gentity_t * */
} fntech3_q3test_entity_t;

typedef struct fntech3_q3test_trace_s {
	int32_t allsolid, startsolid;
	float fraction;
	float endpos[3];
	float planeNormal[3];
	float planeDist;
	uint8_t planeType, planeSignbits, planePadding[2];
	int32_t surfaceFlags, contents, entityNum;
} fntech3_q3test_trace_t;

/* 1.02 allocates a 36-byte record with no modificationCount (Cvar_Get
 * 0x410033); 1.03 to 1.08 hand out 40-byte records from a static array. */
typedef struct fntech3_q3test_cvar102_s {
	uint32_t name, string, resetString, latchedString;
	int32_t flags, modified;
	float value;
	int32_t integer;
	uint32_t next;
} fntech3_q3test_cvar102_t;

typedef struct fntech3_q3test_cvar_s {
	uint32_t name, string, resetString, latchedString;
	int32_t flags, modified, modificationCount;
	float value;
	int32_t integer;
	uint32_t next;
} fntech3_q3test_cvar_t;

/* Cvar flag bits in each family's scheme (5.7 of the ABI record). */
enum {
	FNTECH3_Q3TEST_CVAR_ARCHIVE = 0x001,
	FNTECH3_Q3TEST_CVAR_INIT = 0x010,
	FNTECH3_Q3TEST_CVAR_LATCH = 0x020,
	FNTECH3_Q3TEST_CVAR_AB_USERINFO = 0x004,
	FNTECH3_Q3TEST_CVAR_AB_SERVERINFO = 0x008,
	FNTECH3_Q3TEST_CVAR_AB_CHEAT = 0x040,
	FNTECH3_Q3TEST_CVAR_AB_ROM = 0x080,
	FNTECH3_Q3TEST_CVAR_AB_USER_CREATED = 0x100,
	FNTECH3_Q3TEST_CVAR_AB_SYSTEMINFO = 0x200,
	FNTECH3_Q3TEST_CVAR_CD_USERINFO = 0x002,
	FNTECH3_Q3TEST_CVAR_CD_SERVERINFO = 0x004,
	FNTECH3_Q3TEST_CVAR_CD_SYSTEMINFO = 0x008,
	FNTECH3_Q3TEST_CVAR_CD_ROM = 0x040,
	FNTECH3_Q3TEST_CVAR_CD_USER_CREATED = 0x080,
	FNTECH3_Q3TEST_CVAR_D_CHEAT = 0x200
};

/* The in-memory usercmds the game receives: 14 bytes (A), 20 (B, C) or 24
 * (D), from each engine's MSG_ReadDeltaUsercmd and SV_UserMove stride. */
typedef struct fntech3_q3test_usercmd14_s {
	uint8_t msec, buttons, weapon, pad0;
	int16_t angles[3];
	int8_t forwardmove, rightmove, upmove, pad1;
} fntech3_q3test_usercmd14_t;

typedef struct fntech3_q3test_usercmd20_s {
	uint8_t msec, buttons, weapon, pad0;
	int32_t angles[3];
	int8_t forwardmove, rightmove, upmove, pad1;
} fntech3_q3test_usercmd20_t;

typedef struct fntech3_q3test_usercmd24_s {
	int32_t serverTime;
	uint8_t buttons, weapon, pad0[2];
	int32_t angles[3];
	int8_t forwardmove, rightmove, upmove, pad1;
} fntech3_q3test_usercmd24_t;

typedef fntech3_q3test_entity_t q3tEnt;

/* Family A/B imports (33 words), built by SV_InitGameProgs (1.02 0x41b880). */
typedef struct fntech3_q3test_game_import_ab_s {
	void (FNTECH3_Q3TEST_CALL *Printf)(const char *fmt, ...);
	void (FNTECH3_Q3TEST_CALL *Error)(int32_t level, const char *fmt, ...);
	int32_t (FNTECH3_Q3TEST_CALL *Milliseconds)(void);
	void *(FNTECH3_Q3TEST_CALL *CvarGet)(const char *name, const char *value, int32_t flags);
	void (FNTECH3_Q3TEST_CALL *CvarSet)(const char *name, const char *value);
	int32_t (FNTECH3_Q3TEST_CALL *Argc)(void);
	char *(FNTECH3_Q3TEST_CALL *Argv)(int32_t n);
	int32_t (FNTECH3_Q3TEST_CALL *FOpenFileRead)(const char *qpath, void **file, int32_t unique);
	void (FNTECH3_Q3TEST_CALL *Read)(void *buffer, int32_t length, void *file);
	void *(FNTECH3_Q3TEST_CALL *FOpenFileWrite)(const char *qpath);
	void (FNTECH3_Q3TEST_CALL *Write)(const void *buffer, int32_t length, void *file);
	void (FNTECH3_Q3TEST_CALL *FCloseFile)(void *file);
	void (FNTECH3_Q3TEST_CALL *AddCommandText)(const char *text);
	void (FNTECH3_Q3TEST_CALL *DebugGraph)(float value, int32_t color);
	void (FNTECH3_Q3TEST_CALL *DropClient)(q3tEnt *ent, const char *reason);
	void (FNTECH3_Q3TEST_CALL *SendServerCommand)(q3tEnt *ent, const char *fmt, ...);
	void (FNTECH3_Q3TEST_CALL *SetConfigstring)(int32_t index, const char *value);
	const char *(FNTECH3_Q3TEST_CALL *GetConfigstring)(int32_t index);
	void (FNTECH3_Q3TEST_CALL *SetBrushModel)(q3tEnt *ent, const char *name);
	/* The first argument is the result record; nothing reads eax. */
	void (FNTECH3_Q3TEST_CALL *Trace)(fntech3_q3test_trace_t *results, const float *start,
		const float *mins, const float *maxs, const float *end, int32_t passEntityNum, int32_t mask);
	int32_t (FNTECH3_Q3TEST_CALL *PointContents)(const float *point, int32_t passEntityNum);
	int32_t (FNTECH3_Q3TEST_CALL *InPVS)(const float *p1, const float *p2);
	int32_t (FNTECH3_Q3TEST_CALL *InPVSIgnorePortals)(const float *p1, const float *p2);
	void (FNTECH3_Q3TEST_CALL *AdjustAreaPortalState)(q3tEnt *ent, int32_t open);
	int32_t (FNTECH3_Q3TEST_CALL *AreasConnected)(int32_t area1, int32_t area2);
	void (FNTECH3_Q3TEST_CALL *LinkEntity)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *UnlinkEntity)(q3tEnt *ent);
	int32_t (FNTECH3_Q3TEST_CALL *ContactEntities)(const float *mins, const float *maxs, q3tEnt **list, int32_t maxcount);
	int32_t (FNTECH3_Q3TEST_CALL *SnapshotEntityCount)(q3tEnt *clientEnt);
	int32_t (FNTECH3_Q3TEST_CALL *SnapshotEntityNumber)(q3tEnt *clientEnt, int32_t index);
	q3tEnt *(FNTECH3_Q3TEST_CALL *BotAllocateClient)(void);
	void (FNTECH3_Q3TEST_CALL *BotFreeClient)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *BotSetUserinfo)(q3tEnt *ent, const char *userinfo);
} fntech3_q3test_game_import_ab_t;

/* Family C/D imports: C's 31 words, D's 34. D inserts GetServerinfo before
 * SetBrushModel and appends the two bot imports (1.08 0x41d058..0x41d167). */
typedef struct fntech3_q3test_game_import_cd_head_s {
	void (FNTECH3_Q3TEST_CALL *Printf)(const char *fmt, ...);
	void (FNTECH3_Q3TEST_CALL *Error)(const char *text);
	int32_t (FNTECH3_Q3TEST_CALL *Milliseconds)(void);
	void *(FNTECH3_Q3TEST_CALL *CvarGet)(const char *name, const char *value, int32_t flags);
	void (FNTECH3_Q3TEST_CALL *CvarSet)(const char *name, const char *value);
	int32_t (FNTECH3_Q3TEST_CALL *CvarVariableIntegerValue)(const char *name);
	void (FNTECH3_Q3TEST_CALL *CvarVariableStringBuffer)(const char *name, char *buffer, int32_t size);
	int32_t (FNTECH3_Q3TEST_CALL *Argc)(void);
	char *(FNTECH3_Q3TEST_CALL *Argv)(int32_t n);
	int32_t (FNTECH3_Q3TEST_CALL *FOpenFileByMode)(const char *qpath, int32_t *handle, int32_t mode);
	void (FNTECH3_Q3TEST_CALL *Read)(void *buffer, int32_t length, int32_t handle);
	void (FNTECH3_Q3TEST_CALL *Write)(const void *buffer, int32_t length, int32_t handle);
	void (FNTECH3_Q3TEST_CALL *FCloseFile)(int32_t handle);
	void (FNTECH3_Q3TEST_CALL *AddCommandText)(const char *text);
	void (FNTECH3_Q3TEST_CALL *DropClient)(int32_t clientNum, const char *reason);
	void (FNTECH3_Q3TEST_CALL *SendServerCommand)(int32_t clientNum, const char *fmt, ...);
	void (FNTECH3_Q3TEST_CALL *SetConfigstring)(int32_t index, const char *value);
	void (FNTECH3_Q3TEST_CALL *GetConfigstring)(int32_t index, char *buffer, int32_t size);
	void (FNTECH3_Q3TEST_CALL *GetUserinfo)(int32_t clientNum, char *buffer, int32_t size);
	void (FNTECH3_Q3TEST_CALL *SetUserinfo)(int32_t clientNum, const char *userinfo);
} fntech3_q3test_game_import_cd_head_t;

typedef struct fntech3_q3test_game_import_cd_tail_s {
	void (FNTECH3_Q3TEST_CALL *SetBrushModel)(q3tEnt *ent, const char *name);
	void (FNTECH3_Q3TEST_CALL *Trace)(fntech3_q3test_trace_t *results, const float *start,
		const float *mins, const float *maxs, const float *end, int32_t passEntityNum, int32_t mask);
	int32_t (FNTECH3_Q3TEST_CALL *PointContents)(const float *point, int32_t passEntityNum);
	int32_t (FNTECH3_Q3TEST_CALL *InPVS)(const float *p1, const float *p2);
	int32_t (FNTECH3_Q3TEST_CALL *InPVSIgnorePortals)(const float *p1, const float *p2);
	void (FNTECH3_Q3TEST_CALL *AdjustAreaPortalState)(q3tEnt *ent, int32_t open);
	int32_t (FNTECH3_Q3TEST_CALL *AreasConnected)(int32_t area1, int32_t area2);
	void (FNTECH3_Q3TEST_CALL *LinkEntity)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *UnlinkEntity)(q3tEnt *ent);
	int32_t (FNTECH3_Q3TEST_CALL *EntitiesInBox)(const float *mins, const float *maxs, q3tEnt **list, int32_t maxcount);
	int32_t (FNTECH3_Q3TEST_CALL *EntityContact)(const float *mins, const float *maxs, const q3tEnt *ent);
} fntech3_q3test_game_import_cd_tail_t;

typedef struct fntech3_q3test_game_import_c_s {
	fntech3_q3test_game_import_cd_head_t head;
	fntech3_q3test_game_import_cd_tail_t tail;
} fntech3_q3test_game_import_c_t;

typedef struct fntech3_q3test_game_import_d_s {
	fntech3_q3test_game_import_cd_head_t head;
	void (FNTECH3_Q3TEST_CALL *GetServerinfo)(char *buffer, int32_t size);
	fntech3_q3test_game_import_cd_tail_t tail;
	q3tEnt *(FNTECH3_Q3TEST_CALL *BotAllocateClient)(void);
	void (FNTECH3_Q3TEST_CALL *BotFreeClient)(q3tEnt *ent);
} fntech3_q3test_game_import_d_t;

/* Family A exports: apiVersion 4, 20 words (1.02 GetGameAPI 0x20012df0). */
typedef struct fntech3_q3test_game_export_a_s {
	int32_t apiVersion;
	void (FNTECH3_Q3TEST_CALL *Init)(void);
	void (FNTECH3_Q3TEST_CALL *Shutdown)(void);
	void (FNTECH3_Q3TEST_CALL *SpawnEntities)(const char *mapname, const char *entities, int32_t levelTime);
	char *(FNTECH3_Q3TEST_CALL *ClientConnect)(q3tEnt *ent, char *userinfo);
	void (FNTECH3_Q3TEST_CALL *ClientBegin)(q3tEnt *ent, void *cmd);
	void (FNTECH3_Q3TEST_CALL *ClientUserinfoChanged)(q3tEnt *ent, char *userinfo);
	void (FNTECH3_Q3TEST_CALL *ClientDisconnect)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *ClientCommand)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *ClientThink)(q3tEnt *ent, void *cmd);
	void (FNTECH3_Q3TEST_CALL *ClientAfterCommands)(q3tEnt *ent, void *cmd);
	void (FNTECH3_Q3TEST_CALL *BotBegin)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *BotThink)(q3tEnt *ent, int32_t msec);
	void *reserved;
	void (FNTECH3_Q3TEST_CALL *RunFrame)(int32_t levelTime);
	int32_t (FNTECH3_Q3TEST_CALL *ConsoleCommand)(void);
	q3tEnt *entities;
	int32_t entitySize;
	int32_t numEntities;
	int32_t maxEntities;    /* written by the game, never read by the engine */
} fntech3_q3test_game_export_a_t;

/* Family B exports: apiVersion 4, 18 words (1.05 GetGameAPI 0x20013070). */
typedef struct fntech3_q3test_game_export_b_s {
	int32_t apiVersion;
	void (FNTECH3_Q3TEST_CALL *Init)(const char *mapname, const char *entities, int32_t levelTime, int32_t randomSeed);
	void (FNTECH3_Q3TEST_CALL *Shutdown)(void);
	char *(FNTECH3_Q3TEST_CALL *ClientConnect)(q3tEnt *ent, char *userinfo);
	void (FNTECH3_Q3TEST_CALL *ClientBegin)(q3tEnt *ent, void *cmd);
	void (FNTECH3_Q3TEST_CALL *ClientUserinfoChanged)(q3tEnt *ent, char *userinfo);
	void (FNTECH3_Q3TEST_CALL *ClientDisconnect)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *ClientCommand)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *ClientThink)(q3tEnt *ent, void *cmd);
	void (FNTECH3_Q3TEST_CALL *ClientAfterCommands)(q3tEnt *ent, void *cmd);
	void (FNTECH3_Q3TEST_CALL *BotBegin)(q3tEnt *ent);
	void (FNTECH3_Q3TEST_CALL *BotThink)(q3tEnt *ent, int32_t msec);
	void *reserved;
	void (FNTECH3_Q3TEST_CALL *RunFrame)(int32_t levelTime);
	int32_t (FNTECH3_Q3TEST_CALL *ConsoleCommand)(void);
	q3tEnt *entities;
	int32_t entitySize;
	int32_t numEntities;
} fntech3_q3test_game_export_b_t;

/* Family C/D exports: apiVersion 6, 14 words (1.06 GetGameAPI 0x2000f8c0). */
typedef struct fntech3_q3test_game_export_cd_s {
	int32_t apiVersion;
	void (FNTECH3_Q3TEST_CALL *Init)(const char *mapname, const char *entities, int32_t levelTime,
		int32_t randomSeed, int32_t startTime);
	void (FNTECH3_Q3TEST_CALL *Shutdown)(void);
	char *(FNTECH3_Q3TEST_CALL *ClientConnect)(int32_t clientNum, int32_t firstTime);
	void (FNTECH3_Q3TEST_CALL *ClientBegin)(int32_t clientNum, void *cmd);
	void (FNTECH3_Q3TEST_CALL *ClientUserinfoChanged)(int32_t clientNum);
	void (FNTECH3_Q3TEST_CALL *ClientDisconnect)(int32_t clientNum);
	void (FNTECH3_Q3TEST_CALL *ClientCommand)(int32_t clientNum);
	void (FNTECH3_Q3TEST_CALL *ClientThink)(int32_t clientNum, void *cmd);
	void (FNTECH3_Q3TEST_CALL *RunFrame)(int32_t levelTime);
	int32_t (FNTECH3_Q3TEST_CALL *ConsoleCommand)(void);
	q3tEnt *entities;
	int32_t entitySize;
	int32_t numEntities;
} fntech3_q3test_game_export_cd_t;

typedef void *(FNTECH3_Q3TEST_CALL *fntech3_q3test_GetGameAPI_t)(void *imports);

#if defined(__cplusplus) && (defined(_M_IX86) || defined(__i386__))
static_assert(sizeof(fntech3_q3test_entityState_t) == FNTECH3_Q3TEST_ENTITY_STATE_BYTES, "Q3Test entityState_t");
static_assert(sizeof(fntech3_q3test_entity_t) == FNTECH3_Q3TEST_ENTITY_PREFIX_BYTES, "Q3Test entity prefix");
static_assert(offsetof(fntech3_q3test_entity_t, client) == 0xcc && offsetof(fntech3_q3test_entity_t, svFlags) == 0xdc
	&& offsetof(fntech3_q3test_entity_t, contents) == 0xfc && offsetof(fntech3_q3test_entity_t, currentOrigin) == 0x118
	&& offsetof(fntech3_q3test_entity_t, owner) == 0x130, "Q3Test entity prefix offsets");
static_assert(sizeof(fntech3_q3test_trace_t) == FNTECH3_Q3TEST_TRACE_BYTES, "Q3Test trace_t");
static_assert(sizeof(fntech3_q3test_cvar102_t) == 36 && sizeof(fntech3_q3test_cvar_t) == 40, "Q3Test cvar_t");
static_assert(sizeof(fntech3_q3test_usercmd14_t) == 14 && sizeof(fntech3_q3test_usercmd20_t) == 20
	&& sizeof(fntech3_q3test_usercmd24_t) == 24, "Q3Test usercmd_t");
static_assert(sizeof(fntech3_q3test_game_import_ab_t) == 4 * FNTECH3_Q3TEST_GAME_IMPORTS_AB, "Q3Test A/B imports");
static_assert(sizeof(fntech3_q3test_game_import_c_t) == 4 * FNTECH3_Q3TEST_GAME_IMPORTS_C, "Q3Test C imports");
static_assert(sizeof(fntech3_q3test_game_import_d_t) == 4 * FNTECH3_Q3TEST_GAME_IMPORTS_D, "Q3Test D imports");
static_assert(sizeof(fntech3_q3test_game_export_a_t) == 80 && sizeof(fntech3_q3test_game_export_b_t) == 72
	&& sizeof(fntech3_q3test_game_export_cd_t) == 56, "Q3Test export tables");
static_assert(offsetof(fntech3_q3test_game_export_a_t, entities) == 0x40
	&& offsetof(fntech3_q3test_game_export_b_t, entities) == 0x3c
	&& offsetof(fntech3_q3test_game_export_cd_t, entities) == 0x2c, "Q3Test export entity fields");
#endif

#undef FNTECH3_Q3TEST_CALL
#endif
