/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_GAME_ABI_H
#define FNTECH3_Q3_IHV_GAME_ABI_H

#include <stdint.h>

/* Independently declared Win32 IHV records. These are external storage, never
 * aliases for retail Q3 or engine records. Addresses in POD records are 32-bit
 * addresses, even in diagnostic builds on other architectures. The native
 * function table may only be called by an x86 host. Observations and hashes:
 * docs/fntech3/Q3_IHV.md (native game boundary).
 */
enum {
	FNTECH3_Q3_IHV_GAME_API = 4,
	FNTECH3_Q3_IHV_GAME_IMPORTS = 35,
	FNTECH3_Q3_IHV_ENTITY_STATE_BYTES = 164,
	FNTECH3_Q3_IHV_PLAYER_STATE_BYTES = 276,
	FNTECH3_Q3_IHV_PLAYER_PING_OFFSET = 272,
	/* SV_BuildClientSnapshot 0x41b290..0x41b2a9 reads the view point from
	 * the player state's origin and integer view height. */
	FNTECH3_Q3_IHV_PLAYER_ORIGIN = 8,
	FNTECH3_Q3_IHV_PLAYER_VIEWHEIGHT = 0x5c,
	/* A portal expands its view from the entity state's second origin. */
	FNTECH3_Q3_IHV_ENTITY_ORIGIN2 = 100,
	/* SV_CreateBaseline 0x4196cb tests these three state fields. */
	FNTECH3_Q3_IHV_ENTITY_EFFECTS = 4,
	FNTECH3_Q3_IHV_ENTITY_LOOPSOUND = 148,
	FNTECH3_Q3_IHV_ENTITY_MODELINDEX = 149,
	FNTECH3_Q3_IHV_GAME_ENTITY_BYTES = 556,
	FNTECH3_Q3_IHV_GAME_CLIENT_BYTES = 1464,
	FNTECH3_Q3_IHV_MAX_CONFIGSTRINGS = 1024,
	/* SV_Init 0x419737 sizes the shared snapshot ring per client. */
	FNTECH3_Q3_IHV_SNAPSHOT_RING_PER_CLIENT = 2048,
	/* SV_BuildClientSnapshot 0x41b267 clears 32 bytes of area bits, and
	 * SV_AddEntToSnapshot 0x41b01e stops at 1024 selected entities. */
	FNTECH3_Q3_IHV_AREABITS_BYTES = 32,
	FNTECH3_Q3_IHV_SNAPSHOT_ENTITIES = 1024,
	/* SV_EmitPacketEntities 0x41ace1 stops once the message passes this. */
	FNTECH3_Q3_IHV_SNAPSHOT_MESSAGE_LIMIT = 1350,
	FNTECH3_Q3_IHV_TAG_GAME = 765,
	FNTECH3_Q3_IHV_TAG_LEVEL = 766,
	FNTECH3_Q3_IHV_SVF_NOCLIENT = 0x1,
	FNTECH3_Q3_IHV_SVF_BOT = 0x8,
	FNTECH3_Q3_IHV_SVF_BROADCAST = 0x20,
	FNTECH3_Q3_IHV_SVF_PORTAL = 0x40,
	FNTECH3_Q3_IHV_SVF_CURRENT_ORIGIN = 0x80
};

/* Exact state storage for native snapshot transfers. Keep padding and fields
 * that the shared engine does not interpret; protocol 33 consumes these bytes. */
typedef struct { uint8_t bytes[FNTECH3_Q3_IHV_ENTITY_STATE_BYTES]; } fntech3_q3_ihv_entityState_t;
typedef struct { uint8_t bytes[FNTECH3_Q3_IHV_PLAYER_STATE_BYTES]; } fntech3_q3_ihv_playerState_t;

/* Cvar_Get 0x40de80 allocates 32 bytes; numeric value is at +24, with no
 * modificationCount or integer field. Module writes to modified are allowed. */
typedef struct fntech3_q3_ihv_cvar_s {
	uint32_t name;
	uint32_t string;
	uint32_t resetString;
	uint32_t latchedString;
	int32_t flags;
	int32_t modified;
	float value;
	uint32_t next;
} fntech3_q3_ihv_cvar_t;

/* Only the shared prefix is declared. The remaining bytes in each 556-byte
 * module allocation belong to the game. The state remains separate external
 * storage: q3_ihv_wire.hpp implements its observed wire layout; conversion to
 * engine gameplay records is still required. Shared-prefix accessors live in
 * q3_ihv_native_boundary.hpp. */
typedef struct fntech3_q3_ihv_entity_s {
	uint8_t state[FNTECH3_Q3_IHV_ENTITY_STATE_BYTES];
	uint32_t client;
	int32_t inuse;
	int32_t linked;
	int32_t linkcount;
	int32_t svFlags;
	int32_t bmodel;
	float mins[3];
	float maxs[3];
	int32_t contents;
	float absmin[3];
	float absmax[3];
	float currentOrigin[3];
	float currentAngles[3];
	uint32_t owner;
} fntech3_q3_ihv_entity_t;

typedef struct fntech3_q3_ihv_trace_s {
	int32_t allsolid;
	int32_t startsolid;
	float fraction;
	float endpos[3];
	float planeNormal[3];
	float planeDist;
	uint8_t planeType;
	uint8_t planeSignbits;
	uint8_t planePadding[2];
	int32_t surfaceFlags;
	int32_t contents;
	int32_t entityNum;
} fntech3_q3_ihv_trace_t;

/* The engine receives fourteen bytes per command. q3_ihv_wire.hpp preserves
 * its duration, buttons, weapon, short angles, signed movement and padding;
 * it must not be passed to a retail usercmd_t callback. */
typedef struct fntech3_q3_ihv_usercmd_s {
	uint8_t bytes[14];
} fntech3_q3_ihv_usercmd_t;

#ifdef _MSC_VER
#define FNTECH3_Q3_IHV_CALL __cdecl
#elif defined(__i386__)
#define FNTECH3_Q3_IHV_CALL __attribute__((cdecl))
#else
#define FNTECH3_Q3_IHV_CALL
#endif

/* Original engine 0x419430 constructs these 35 imports in order. Trace returns
 * its 56-byte record BY VALUE: x86 supplies a hidden result pointer. */
typedef struct fntech3_q3_ihv_game_import_s {
	void (FNTECH3_Q3_IHV_CALL *Printf)(const char *fmt, ...);
	void (FNTECH3_Q3_IHV_CALL *Error)(int32_t level, const char *fmt, ...);
	int32_t (FNTECH3_Q3_IHV_CALL *Milliseconds)(void);
	void *(FNTECH3_Q3_IHV_CALL *TagMalloc)(int32_t bytes, int32_t tag);
	void (FNTECH3_Q3_IHV_CALL *Free)(void *ptr);
	void (FNTECH3_Q3_IHV_CALL *FreeTags)(int32_t tag);
	fntech3_q3_ihv_cvar_t *(FNTECH3_Q3_IHV_CALL *CvarGet)(const char *name, const char *value, int32_t flags);
	void (FNTECH3_Q3_IHV_CALL *CvarSet)(const char *name, const char *value);
	int32_t (FNTECH3_Q3_IHV_CALL *Argc)(void);
	char *(FNTECH3_Q3_IHV_CALL *Argv)(int32_t index);
	int32_t (FNTECH3_Q3_IHV_CALL *ReadFile)(const char *name, void **buffer);
	void (FNTECH3_Q3_IHV_CALL *FreeFile)(void *buffer);
	void (FNTECH3_Q3_IHV_CALL *WriteFile)(const char *name, const void *buffer, int32_t length);
	char *(FNTECH3_Q3_IHV_CALL *FileForWriting)(const char *name);
	void (FNTECH3_Q3_IHV_CALL *AddCommandText)(const char *text);
	void (FNTECH3_Q3_IHV_CALL *DebugGraph)(float value, int32_t color);
	void (FNTECH3_Q3_IHV_CALL *DropClient)(fntech3_q3_ihv_entity_t *entity, const char *reason);
	void (FNTECH3_Q3_IHV_CALL *SendServerCommand)(fntech3_q3_ihv_entity_t *entity, const char *fmt, ...);
	void (FNTECH3_Q3_IHV_CALL *SetConfigstring)(int32_t index, const char *value);
	char *(FNTECH3_Q3_IHV_CALL *GetConfigstring)(int32_t index);
	void (FNTECH3_Q3_IHV_CALL *SetBrushModel)(fntech3_q3_ihv_entity_t *entity, const char *name);
	fntech3_q3_ihv_trace_t (FNTECH3_Q3_IHV_CALL *Trace)(const float *start, const float *mins,
		const float *maxs, const float *end, int32_t passEntityNum, int32_t mask);
	int32_t (FNTECH3_Q3_IHV_CALL *PointContents)(const float *point, int32_t passEntityNum);
	int32_t (FNTECH3_Q3_IHV_CALL *InPVS)(const float *p1, const float *p2);
	int32_t (FNTECH3_Q3_IHV_CALL *InPVSIgnorePortals)(const float *p1, const float *p2);
	void (FNTECH3_Q3_IHV_CALL *AdjustAreaPortalState)(fntech3_q3_ihv_entity_t *entity, int32_t open);
	int32_t (FNTECH3_Q3_IHV_CALL *AreasConnected)(int32_t area1, int32_t area2);
	void (FNTECH3_Q3_IHV_CALL *LinkEntity)(fntech3_q3_ihv_entity_t *entity);
	void (FNTECH3_Q3_IHV_CALL *UnlinkEntity)(fntech3_q3_ihv_entity_t *entity);
	int32_t (FNTECH3_Q3_IHV_CALL *ContactEntities)(const float *mins, const float *maxs,
		fntech3_q3_ihv_entity_t **list, int32_t count);
	int32_t (FNTECH3_Q3_IHV_CALL *SnapshotEntityCount)(fntech3_q3_ihv_entity_t *client);
	int32_t (FNTECH3_Q3_IHV_CALL *SnapshotEntityNumber)(fntech3_q3_ihv_entity_t *client, int32_t index);
	fntech3_q3_ihv_entity_t *(FNTECH3_Q3_IHV_CALL *BotAllocateClient)(void);
	void (FNTECH3_Q3_IHV_CALL *BotFreeClient)(fntech3_q3_ihv_entity_t *client);
	void (FNTECH3_Q3_IHV_CALL *BotSetUserinfo)(fntech3_q3_ihv_entity_t *client, const char *userinfo);
} fntech3_q3_ihv_game_import_t;

typedef struct fntech3_q3_ihv_game_export_s {
	int32_t apiVersion;
	void (FNTECH3_Q3_IHV_CALL *Init)(void);
	void (FNTECH3_Q3_IHV_CALL *Shutdown)(void);
	void (FNTECH3_Q3_IHV_CALL *SpawnEntities)(const char *map, const char *entities, int32_t time);
	char *(FNTECH3_Q3_IHV_CALL *ClientConnect)(fntech3_q3_ihv_entity_t *entity, char *userinfo);
	void (FNTECH3_Q3_IHV_CALL *ClientBegin)(fntech3_q3_ihv_entity_t *entity, fntech3_q3_ihv_usercmd_t *cmd);
	void (FNTECH3_Q3_IHV_CALL *ClientUserinfoChanged)(fntech3_q3_ihv_entity_t *entity, char *userinfo);
	void (FNTECH3_Q3_IHV_CALL *ClientDisconnect)(fntech3_q3_ihv_entity_t *entity);
	void (FNTECH3_Q3_IHV_CALL *ClientCommand)(fntech3_q3_ihv_entity_t *entity);
	void (FNTECH3_Q3_IHV_CALL *ClientThink)(fntech3_q3_ihv_entity_t *entity, fntech3_q3_ihv_usercmd_t *cmd);
	/* Engine 0x418ecd calls this once after processing a packet's commands. */
	void (FNTECH3_Q3_IHV_CALL *ClientAfterCommands)(fntech3_q3_ihv_entity_t *entity, fntech3_q3_ihv_usercmd_t *cmd);
	void (FNTECH3_Q3_IHV_CALL *BotBegin)(fntech3_q3_ihv_entity_t *entity);
	void (FNTECH3_Q3_IHV_CALL *BotThink)(fntech3_q3_ihv_entity_t *entity, int32_t frameMsec);
	/* Null in the observed export table; no signature assigned. */
	void *reserved;
	void (FNTECH3_Q3_IHV_CALL *RunFrame)(int32_t time);
	int32_t (FNTECH3_Q3_IHV_CALL *ConsoleCommand)(void);
	fntech3_q3_ihv_entity_t *entities;
	int32_t entityStride;
	int32_t entityCount;
	int32_t entityCapacity;
} fntech3_q3_ihv_game_export_t;

typedef fntech3_q3_ihv_game_export_t *(FNTECH3_Q3_IHV_CALL *fntech3_q3_ihv_GetGameAPI_t)(fntech3_q3_ihv_game_import_t *imports);

#undef FNTECH3_Q3_IHV_CALL
#endif
