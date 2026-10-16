/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_CLIENT_ABI_H
#define FNTECH3_JK2_CLIENT_ABI_H

#include "jk2_game_abi.h"
#include <stddef.h>

/* Jedi Outcast 1.02a multiplayer client transfers, measured from jk2mp.exe:
 * GetGameState 0x401000 copies 0x1519 words (1400 offsets, 16000 chars,
 * count); GetGlconfig 0x401020 and UI 0x4114d0 copy 0xb11 words.
 * GetSnapshot 0x4010c0 writes its prefix at 0/4/8, its 32-byte area mask at
 * 12, 0x156 player words at 44, count at 1412, and 256 296-byte entities at
 * 1416. It writes the command sequence at 77196. The word at 77192 is unused.
 * UI client state 0x410960 writes three scalars and three 1024-byte strings.
 * These records are separate from Q3 and Academy, including equal prefixes.
 */
enum {
	FNTECH3_JK2_MP_GAMESTATE_CHARS = 16000,
	FNTECH3_JK2_MP_CONFIGSTRINGS = 1400,
	FNTECH3_JK2_MP_SNAPSHOT_ENTITIES = 256,
	FNTECH3_JK2_MP_SNAPSHOT_BYTES = 77200,
	FNTECH3_JK2_MP_UI_CLIENTSTATE_BYTES = 3084,
	/* svc8 has no payload and calls cgame export24 (0x40edd7). */
	FNTECH3_JK2_MP_CG_MAP_CHANGE = 24
};

typedef struct {
	int32_t stringOffsets[FNTECH3_JK2_MP_CONFIGSTRINGS];
	char stringData[FNTECH3_JK2_MP_GAMESTATE_CHARS];
	int32_t dataCount;
} fntech3_jk2_mp_gameState_t;

typedef struct {
	char renderer_string[1024], vendor_string[1024], version_string[1024];
	char extensions_string[8192];
	int32_t maxTextureSize, numTextureUnits;
	int32_t colorBits, depthBits, stencilBits;
	int32_t driverType, hardwareType, deviceSupportsGamma;
	int32_t textureCompression, textureEnvAddAvailable;
	int32_t vidWidth, vidHeight;
	float windowAspect;
	int32_t displayFrequency, isFullscreen, stereoEnabled, smpActive;
} fntech3_jk2_mp_glconfig_t;

typedef struct {
	int32_t snapFlags, ping, serverTime;
	uint8_t areamask[32];
	fntech3_jk2_playerState_t ps;
	int32_t numEntities;
	fntech3_jk2_entityState_t entities[FNTECH3_JK2_MP_SNAPSHOT_ENTITIES];
	int32_t numServerCommands, serverCommandSequence;
} fntech3_jk2_mp_snapshot_t;

typedef struct {
	int32_t connState, connectPacketCount, clientNum;
	char servername[1024], updateInfoString[1024], messageString[1024];
} fntech3_jk2_mp_uiClientState_t;

/* UI66: jk2mp.exe 0x4b5a9c/0x4b5acb/0x4b5adc..e1 place the
 * 1024-character token after three signed words and one binary32 value. */
typedef struct {
	int32_t type, subtype, intvalue;
	float floatvalue;
	char string[1024];
} fntech3_jk2_mp_pc_token_t;

/* UI72: 0x428789..0x4287ba copies the nine 32-bit CRT calendar fields. */
typedef struct {
	int32_t tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
	int32_t tm_wday, tm_yday, tm_isdst;
} fntech3_jk2_mp_qtime_t;

/* Retail assets0.pk3 vm/ui.qvm diagnostic procedures at instructions
 * 38438/38486 use filename local28..155, then another local at156. */
enum { FNTECH3_JK2_MP_SOURCE_FILENAME_CHARS = 128 };

/* A collision result as jk2mp.exe hands it to a module. CM_BoxTrace 0x426a60
 * clears its work area (0x42a5f0, 0x51c bytes) and copies 0x10e words to the
 * caller (0x426ff8..0x426ffd): Quake III's 56-byte trace, then a 1024-byte
 * Ghoul2 collision map (sixteen 64-byte records) that a world trace leaves
 * zero. Its effects engine's SFxHelper::Trace 0x4126a0 writes its request
 * after that record in the client game's shared buffer, calls client-game
 * export 16 and copies the same 1080 bytes back; its decals go to export 23
 * as the 52-byte record CreateEffect's Decal case fills (0x418989..0x418a0d).
 */
enum {
	FNTECH3_JK2_MP_GHOUL2_COLLISION_BYTES = 1024,
	FNTECH3_JK2_MP_CG_TRACE = 16,
	FNTECH3_JK2_MP_CG_IMPACT_MARK = 23
};

typedef struct {
	int32_t allsolid, startsolid;
	float fraction, endpos[3];
	float normal[3], dist;
	uint8_t type, signbits, pad[2];
	int32_t surfaceFlags, contents, entityNum;
	uint8_t ghoul2Collisions[FNTECH3_JK2_MP_GHOUL2_COLLISION_BYTES];
} fntech3_jk2_mp_trace_t;

typedef struct {
	fntech3_jk2_mp_trace_t result;
	float start[3], mins[3], maxs[3], end[3];
	int32_t skipEntity, mask;
} fntech3_jk2_mp_cgame_trace_t;

typedef struct {
	int32_t shader;
	float origin[3], direction[3], rotation, rgb[3], alpha, radius;
} fntech3_jk2_mp_cgame_impact_mark_t;

#ifdef __cplusplus
static_assert( sizeof( fntech3_jk2_mp_trace_t ) == 1080, "0x10e words" );
static_assert( offsetof( fntech3_jk2_mp_trace_t, fraction ) == 8, "the server's import 24 reads it there" );
static_assert( offsetof( fntech3_jk2_mp_trace_t, entityNum ) == 52, "the server's import 24 writes it there" );
static_assert( sizeof( fntech3_jk2_mp_cgame_trace_t ) == 0x470, "0x4126c5" );
static_assert( offsetof( fntech3_jk2_mp_cgame_trace_t, start ) == 0x438, "0x4126d0" );
static_assert( offsetof( fntech3_jk2_mp_cgame_trace_t, mins ) == 0x444, "0x4126ea" );
static_assert( offsetof( fntech3_jk2_mp_cgame_trace_t, maxs ) == 0x450, "0x412704" );
static_assert( offsetof( fntech3_jk2_mp_cgame_trace_t, end ) == 0x45c, "0x412722" );
static_assert( offsetof( fntech3_jk2_mp_cgame_trace_t, skipEntity ) == 0x468, "0x412742" );
static_assert( offsetof( fntech3_jk2_mp_cgame_trace_t, mask ) == 0x46c, "0x412748" );
static_assert( sizeof( fntech3_jk2_mp_cgame_impact_mark_t ) == 52, "0x418997..0x418a02" );
static_assert( offsetof( fntech3_jk2_mp_cgame_impact_mark_t, rotation ) == 0x1c, "0x4189ce" );
static_assert( offsetof( fntech3_jk2_mp_cgame_impact_mark_t, alpha ) == 0x2c, "0x4189f4" );
#endif

#endif
