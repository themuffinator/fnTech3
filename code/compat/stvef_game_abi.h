/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_GAME_ABI_H
#define FNTECH3_STVEF_GAME_ABI_H

#include <stdint.h>

/* Independently declared Elite Force interoperability records, never engine
 * storage. SP layouts are confirmed against the retail 1.20 stvoy.exe,
 * efgamex86.dll and efuix86.dll (copy sizes and field reads cited in
 * docs/fntech3/2026-09-19-stvef-sp-host.md); Holomatch layouts follow the
 * retail QVM contract (docs/fntech3/2026-09-18-stvef-holomatch.md). Raven's
 * SP mod SDK is consulted for names and ordinals only, under its non-free
 * licence; no text from it is incorporated.
 *
 * SP modules are Win32 x86 only, so pointer-carrying SP records are frozen for
 * a 32-bit host; tests assert their sizes only there. */
enum {
	FNTECH3_STVEF_SP_PROTOCOL = 40,
	FNTECH3_STVEF_MP_PROTOCOL = 24,
	FNTECH3_STVEF_IOEF_PROTOCOL = 26,
	FNTECH3_STVEF_GENTITYNUM_BITS = 10,
	FNTECH3_STVEF_MAX_GENTITIES = 1 << 10,
	FNTECH3_STVEF_ENTITYNUM_NONE = FNTECH3_STVEF_MAX_GENTITIES - 1,
	FNTECH3_STVEF_ENTITYNUM_WORLD = FNTECH3_STVEF_MAX_GENTITIES - 2,
	FNTECH3_STVEF_MAX_CONFIGSTRINGS = 1024,
	FNTECH3_STVEF_MAX_GAMESTATE_CHARS = 16000,
	FNTECH3_STVEF_MAX_MAP_AREA_BYTES = 32,
	FNTECH3_STVEF_MAX_ENTITIES_IN_SNAPSHOT = 256,
	FNTECH3_STVEF_MAX_STRING_CHARS = 1024,
	FNTECH3_STVEF_MAX_QPATH = 64,
	FNTECH3_STVEF_MAX_CVAR_VALUE_STRING = 256,

	/* Single player: one client, 16-entry stat arrays, four ammo types and
	 * 32 Borg adaptation counters. */
	FNTECH3_STVEF_SP_MAX_CLIENTS = 1,
	FNTECH3_STVEF_SP_MAX_PS_EVENTS = 2,
	FNTECH3_STVEF_SP_MAX_STATS = 16,
	FNTECH3_STVEF_SP_MAX_AMMO = 4,
	FNTECH3_STVEF_SP_MAX_WEAPONS = 32,
	FNTECH3_STVEF_SP_GAME_API_VERSION = 6,
	FNTECH3_STVEF_SP_UI_API_VERSION = 2,
	FNTECH3_STVEF_SP_CS_PLAYERS = 522,
	FNTECH3_STVEF_SP_CS_AMBIENT_SET = 5,
	FNTECH3_STVEF_SP_CMD_BACKUP = 64,
	FNTECH3_STVEF_SP_PACKET_BACKUP = 32,
	/* Engine-visible SVF bits; every other bit belongs to the game. */
	FNTECH3_STVEF_SP_SVF_NOCLIENT = 0x00000001,
	FNTECH3_STVEF_SP_SVF_BOT = 0x00000008,
	FNTECH3_STVEF_SP_SVF_BROADCAST = 0x00000020,
	FNTECH3_STVEF_SP_SVF_PORTAL = 0x00000040,
	/* SP render records keep Quake III's eight entity types. */
	FNTECH3_STVEF_SP_RT_MAX = 8,
	/* SP sound channels, as Raven's SP SDK numbers them: an attenuated voice
	 * after the voice, ambient sounds after the body, two menu channels. */
	FNTECH3_STVEF_SP_CHAN_AUTO = 0,
	FNTECH3_STVEF_SP_CHAN_LOCAL = 1,
	FNTECH3_STVEF_SP_CHAN_WEAPON = 2,
	FNTECH3_STVEF_SP_CHAN_VOICE = 3,
	FNTECH3_STVEF_SP_CHAN_VOICE_ATTEN = 4,
	FNTECH3_STVEF_SP_CHAN_ITEM = 5,
	FNTECH3_STVEF_SP_CHAN_BODY = 6,
	FNTECH3_STVEF_SP_CHAN_AMBIENT = 7,
	FNTECH3_STVEF_SP_CHAN_LOCAL_SOUND = 8,
	FNTECH3_STVEF_SP_CHAN_ANNOUNCER = 9,
	FNTECH3_STVEF_SP_CHAN_MENU1 = 10,
	FNTECH3_STVEF_SP_CHAN_MENU2 = 11,
	FNTECH3_STVEF_SP_SAVE_MAGIC = 0x1234ABCD,
	FNTECH3_STVEF_SP_SCREENSHOT_SIZE = 256,
	FNTECH3_STVEF_SP_LIPSYNC_ENTITIES = 1024,

	/* Holomatch: 64 clients, four player events, 16 weapons. */
	FNTECH3_STVEF_MP_MAX_CLIENTS = 64,
	/* A one-byte button field: seven buttons (attack, talk, use item,
	 * gesture, walking, alternate attack, use) and 0x80 for "any key". */
	FNTECH3_STVEF_MP_BUTTON_COUNT = 7,
	FNTECH3_STVEF_MP_BUTTON_ANY = 0x80,
	FNTECH3_STVEF_MP_MAX_PS_EVENTS = 4,
	FNTECH3_STVEF_MP_MAX_STATS = 16,
	FNTECH3_STVEF_MP_MAX_WEAPONS = 16,
	FNTECH3_STVEF_MP_CS_PLAYERS = 544,
	/* Holomatch shares Quake III's SVF bits except 0x2 (shield bounds) and
	 * 0x4/0x10, which Raven gave game-side meanings. */
	FNTECH3_STVEF_MP_SVF_NOCLIENT = 0x00000001,
	FNTECH3_STVEF_MP_SVF_SHIELD_BBOX = 0x00000002,
	FNTECH3_STVEF_MP_SVF_BOT = 0x00000008,
	FNTECH3_STVEF_MP_SVF_ELIMINATED = 0x00000010,
	FNTECH3_STVEF_MP_SVF_BROADCAST = 0x00000020,
	FNTECH3_STVEF_MP_SVF_PORTAL = 0x00000040,
	FNTECH3_STVEF_MP_SVF_USE_CURRENT_ORIGIN = 0x00000080,
	FNTECH3_STVEF_MP_SVF_SINGLECLIENT = 0x00000100,
	FNTECH3_STVEF_MP_SVF_NOSERVERINFO = 0x00000200,
	FNTECH3_STVEF_MP_SVF_CAPSULE = 0x00000400,
	FNTECH3_STVEF_MP_SVF_NOTSINGLECLIENT = 0x00000800,
	/* Entity flags the engine sets on a linked shield box. */
	FNTECH3_STVEF_MP_EF_SHIELD_BOX_X = 0x00000800,
	FNTECH3_STVEF_MP_EF_SHIELD_BOX_Y = 0x00400000,
	/* Holomatch render entity types, in Raven's ordinal order. */
	FNTECH3_STVEF_MP_RT_MODEL = 0,
	FNTECH3_STVEF_MP_RT_SPRITE = 1,
	FNTECH3_STVEF_MP_RT_ORIENTEDSPRITE = 2,
	FNTECH3_STVEF_MP_RT_ALPHAVERTPOLY = 3,
	FNTECH3_STVEF_MP_RT_BEAM = 4,
	FNTECH3_STVEF_MP_RT_RAIL_CORE = 5,
	FNTECH3_STVEF_MP_RT_RAIL_RINGS = 6,
	FNTECH3_STVEF_MP_RT_LIGHTNING = 7,
	FNTECH3_STVEF_MP_RT_PORTALSURFACE = 8,
	FNTECH3_STVEF_MP_RT_LINE = 9,
	FNTECH3_STVEF_MP_RT_ORIENTEDLINE = 10,
	FNTECH3_STVEF_MP_RT_LINE2 = 11,
	FNTECH3_STVEF_MP_RT_BEZIER = 12,
	FNTECH3_STVEF_MP_RT_CYLINDER = 13,
	FNTECH3_STVEF_MP_RT_ELECTRICITY = 14,
	FNTECH3_STVEF_MP_RT_MAX = 15,
	/* Holomatch render flags. Raven named 0x1 RF_MINLIGHT, but the retail 1.20
	 * renderer darkens an entity with it, and brightens one with 0x10
	 * (R_SetupEntityLighting 0x479f3c); 0x800 forces the entity's alpha. */
	FNTECH3_STVEF_MP_RF_MINLIGHT = 0x0001,
	FNTECH3_STVEF_MP_RF_THIRD_PERSON = 0x0002,
	FNTECH3_STVEF_MP_RF_FIRST_PERSON = 0x0004,
	FNTECH3_STVEF_MP_RF_DEPTHHACK = 0x0008,
	FNTECH3_STVEF_MP_RF_FULLBRIGHT = 0x0010,
	FNTECH3_STVEF_MP_RF_NOSHADOW = 0x0040,
	FNTECH3_STVEF_MP_RF_LIGHTING_ORIGIN = 0x0080,
	FNTECH3_STVEF_MP_RF_SHADOW_PLANE = 0x0100,
	FNTECH3_STVEF_MP_RF_WRAP_FRAMES = 0x0200,
	FNTECH3_STVEF_MP_RF_FORCE_ENT_ALPHA = 0x0800,
	FNTECH3_STVEF_MP_RDF_NOWORLDMODEL = 0x0001,
	FNTECH3_STVEF_MP_RDF_HYPERSPACE = 0x0004
};

typedef struct {
	int32_t trType, trTime, trDuration;
	float trBase[3], trDelta[3];
} fntech3_stvef_trajectory_t;

typedef struct {
	float normal[3];
	float dist;
	uint8_t type, signbits, pad[2];
} fntech3_stvef_cplane_t;

typedef struct {
	int32_t allsolid, startsolid;
	float fraction;
	float endpos[3];
	fntech3_stvef_cplane_t plane;
	int32_t surfaceFlags, contents, entityNum;
} fntech3_stvef_trace_t;

typedef struct {
	int32_t handle, modificationCount;
	float value;
	int32_t integer;
	char string[FNTECH3_STVEF_MAX_CVAR_VALUE_STRING];
} fntech3_stvef_vmCvar_t;

typedef struct {
	int32_t stringOffsets[FNTECH3_STVEF_MAX_CONFIGSTRINGS];
	char stringData[FNTECH3_STVEF_MAX_GAMESTATE_CHARS];
	int32_t dataCount;
} fntech3_stvef_gameState_t;

typedef struct {
	float xyz[3];
	float st[2];
	uint8_t modulate[4];
} fntech3_stvef_polyVert_t;

typedef struct {
	float origin[3];
	float axis[3][3];
} fntech3_stvef_orientation_t;

typedef struct {
	int32_t firstPoint, numPoints;
} fntech3_stvef_markFragment_t;

/* Both modes share refdef_t: Quake III's without the render-string text. */
typedef struct {
	int32_t x, y, width, height;
	float fov_x, fov_y;
	float vieworg[3];
	float viewaxis[3][3];
	int32_t time;
	int32_t rdflags;
	uint8_t areamask[FNTECH3_STVEF_MAX_MAP_AREA_BYTES];
} fntech3_stvef_refdef_t;

/* ---- Single player ---------------------------------------------------- */

typedef struct {
	int32_t number, eType, eFlags;
	fntech3_stvef_trajectory_t pos, apos;
	int32_t time, time2;
	float origin[3], origin2[3], angles[3], angles2[3];
	int32_t otherEntityNum, otherEntityNum2, groundEntityNum;
	int32_t constantLight, loopSound;
	int32_t modelindex, modelindex2, modelindex3;
	int32_t clientNum, frame, solid, event, eventParm;
	int32_t powerups, weapon;
	int32_t legsAnim, legsAnimTimer, torsoAnim, torsoAnimTimer;
	int32_t scale;
	float pushVec[3];
} fntech3_stvef_sp_entityState_t;

typedef struct {
	int32_t commandTime, pm_type, bobCycle, pm_flags, pm_time;
	float origin[3], velocity[3];
	int32_t weaponTime, rechargeTime;
	int16_t useTime;
	int16_t useTimePad;
	int32_t gravity;
	int8_t leanofs;
	uint8_t leanofsPad;
	int16_t friction;
	int32_t speed;
	int32_t delta_angles[3];
	int32_t groundEntityNum;
	int32_t legsAnim, legsAnimTimer, torsoAnim, torsoAnimTimer;
	int32_t scale, movementDir, eFlags;
	int32_t eventSequence;
	int32_t events[FNTECH3_STVEF_SP_MAX_PS_EVENTS], eventParms[FNTECH3_STVEF_SP_MAX_PS_EVENTS];
	int32_t externalEvent, externalEventParm, externalEventTime;
	int32_t clientNum, weapon, weaponstate;
	float viewangles[3];
	int32_t viewheight;
	int32_t damageEvent, damageYaw, damagePitch, damageCount;
	int32_t stats[FNTECH3_STVEF_SP_MAX_STATS];
	int32_t persistant[FNTECH3_STVEF_SP_MAX_STATS];
	int32_t powerups[FNTECH3_STVEF_SP_MAX_STATS];
	int32_t ammo[FNTECH3_STVEF_SP_MAX_AMMO];
	int32_t borgAdaptHits[FNTECH3_STVEF_SP_MAX_WEAPONS];
	float pushVec[3];
	int32_t ping;
	uint8_t leanStopDebounceTime;
	uint8_t tailPad[3];
} fntech3_stvef_sp_playerState_t;

typedef struct {
	int32_t serverTime;
	int32_t buttons;
	uint8_t weapon;
	uint8_t weaponPad[3];
	int32_t angles[3];
	int8_t forwardmove, rightmove, upmove;
	uint8_t tailPad;
} fntech3_stvef_sp_usercmd_t;

/* The server-visible prefix of the SP game's gentity_t. The game owns the
 * rest of its 1056-byte record. Pointers are 32-bit on the only host. */
typedef struct fntech3_stvef_sp_gentity_s fntech3_stvef_sp_gentity_t;
struct fntech3_stvef_sp_gentity_s {
	fntech3_stvef_sp_entityState_t s;
	fntech3_stvef_sp_playerState_t *client;
	int32_t inuse, linked;
	int32_t svFlags;
	int32_t bmodel;
	float mins[3], maxs[3];
	int32_t contents;
	float absmin[3], absmax[3];
	float currentOrigin[3], currentAngles[3];
	fntech3_stvef_sp_gentity_t *owner;
};

typedef struct {
	int32_t snapFlags, ping, serverTime;
	uint8_t areamask[FNTECH3_STVEF_MAX_MAP_AREA_BYTES];
	int32_t cmdNum;
	fntech3_stvef_sp_playerState_t ps;
	int32_t numEntities;
	fntech3_stvef_sp_entityState_t entities[FNTECH3_STVEF_MAX_ENTITIES_IN_SNAPSHOT];
	int32_t numConfigstringChanges, configstringNum;
	int32_t numServerCommands, serverCommandSequence;
} fntech3_stvef_sp_snapshot_t;

/* The game holds cvar_t pointers into engine storage and reads only string,
 * value and integer. */
typedef struct fntech3_stvef_sp_cvar_s fntech3_stvef_sp_cvar_t;
struct fntech3_stvef_sp_cvar_s {
	char *name, *string, *resetString, *latchedString;
	int32_t flags, modified, modificationCount;
	float value;
	int32_t integer;
	fntech3_stvef_sp_cvar_t *next;
};

typedef struct {
	int32_t reType, renderfx, hModel;
	float lightingOrigin[3];
	float shadowPlane;
	float axis[3][3];
	int32_t nonNormalizedAxes;
	float origin[3];
	int32_t frame;
	float oldorigin[3];
	int32_t oldframe;
	float backlerp;
	int32_t skinNum, customSkin, customShader;
	uint8_t shaderRGBA[4];
	float shaderTexCoord[2];
	float shaderTime;
	float lightDir[3];
	float radius, rotation;
} fntech3_stvef_sp_refEntity_t;

typedef struct {
	char renderer_string[FNTECH3_STVEF_MAX_STRING_CHARS];
	char vendor_string[FNTECH3_STVEF_MAX_STRING_CHARS];
	char version_string[FNTECH3_STVEF_MAX_STRING_CHARS];
	char extensions_string[2 * FNTECH3_STVEF_MAX_STRING_CHARS];
	int32_t maxTextureSize, maxActiveTextures;
	int32_t colorBits, depthBits, stencilBits;
	int32_t driverType, hardwareType;
	int32_t deviceSupportsGamma, textureCompression;
	int32_t textureEnvAddAvailable, textureFilterAnisotropicAvailable, clampToEdgeAvailable;
	int32_t vidWidth, vidHeight;
	float windowAspect;
	int32_t displayFrequency;
	int32_t isFullscreen, stereoEnabled, smpActive;
} fntech3_stvef_sp_glconfig_t;

/* ---- Holomatch -------------------------------------------------------- */

/* Quake III's entityState_t without generic1. */
typedef struct {
	int32_t number, eType, eFlags;
	fntech3_stvef_trajectory_t pos, apos;
	int32_t time, time2;
	float origin[3], origin2[3], angles[3], angles2[3];
	int32_t otherEntityNum, otherEntityNum2, groundEntityNum;
	int32_t constantLight, loopSound;
	int32_t modelindex, modelindex2;
	int32_t clientNum, frame, solid, event, eventParm;
	int32_t powerups, weapon, legsAnim, torsoAnim;
} fntech3_stvef_mp_entityState_t;

/* Quake III's entityShared_t without its unused leading entityState_t. */
typedef struct {
	int32_t linked, linkcount, svFlags, singleClient, bmodel;
	float mins[3], maxs[3];
	int32_t contents;
	float absmin[3], absmax[3];
	float currentOrigin[3], currentAngles[3];
	int32_t ownerNum;
} fntech3_stvef_mp_entityShared_t;

typedef struct {
	fntech3_stvef_mp_entityState_t s;
	fntech3_stvef_mp_entityShared_t r;
} fntech3_stvef_mp_sharedEntity_t;

typedef struct {
	int32_t commandTime, pm_type, bobCycle, pm_flags, pm_time;
	float origin[3], velocity[3];
	int32_t weaponTime, rechargeTime;
	int16_t useTime;
	int16_t useTimePad;
	int32_t introTime;
	int32_t gravity, speed;
	int32_t delta_angles[3];
	int32_t groundEntityNum;
	int32_t legsTimer, legsAnim, torsoTimer, torsoAnim;
	int32_t movementDir, eFlags;
	int32_t eventSequence;
	int32_t events[FNTECH3_STVEF_MP_MAX_PS_EVENTS], eventParms[FNTECH3_STVEF_MP_MAX_PS_EVENTS];
	int32_t externalEvent, externalEventParm, externalEventTime;
	int32_t clientNum, weapon, weaponstate;
	float viewangles[3];
	int32_t viewheight;
	int32_t damageEvent, damageYaw, damagePitch, damageCount, damageShieldCount;
	int32_t stats[FNTECH3_STVEF_MP_MAX_STATS];
	int32_t persistant[FNTECH3_STVEF_MP_MAX_STATS];
	int32_t powerups[FNTECH3_STVEF_MP_MAX_STATS];
	int32_t ammo[FNTECH3_STVEF_MP_MAX_WEAPONS];
	int32_t ping;
	int32_t entityEventSequence;
} fntech3_stvef_mp_playerState_t;

typedef struct {
	int32_t serverTime;
	uint8_t buttons, weapon;
	uint8_t pad[2];
	int32_t angles[3];
	int8_t forwardmove, rightmove, upmove;
	uint8_t tailPad;
} fntech3_stvef_mp_usercmd_t;

typedef struct {
	int32_t snapFlags, ping, serverTime;
	uint8_t areamask[FNTECH3_STVEF_MAX_MAP_AREA_BYTES];
	fntech3_stvef_mp_playerState_t ps;
	int32_t numEntities;
	fntech3_stvef_mp_entityState_t entities[FNTECH3_STVEF_MAX_ENTITIES_IN_SNAPSHOT];
	int32_t numServerCommands, serverCommandSequence;
} fntech3_stvef_mp_snapshot_t;

/* Quake III's bot_input_t; its actionflags carry the 1.17 bit order. */
typedef struct {
	float thinktime;
	float dir[3];
	float speed;
	float viewangles[3];
	int32_t actionflags;
	int32_t weapon;
} fntech3_stvef_bot_input_t;

/* Raven's FX primitives share one 28-byte union after shaderTime. */
typedef union {
	struct { float rotation, radius; uint8_t vertRGBA[4][4]; } sprite;
	struct { float width, width2, stscale; } line;
	struct { float width; float control1[3], control2[3]; } bezier;
	struct { float width, width2, stscale, height, bias; int32_t wrap; } cylinder;
	struct { float width, deviation, stscale; int32_t wrap, taper; } electricity;
} fntech3_stvef_mp_refEntityData_t;

typedef struct {
	int32_t reType, renderfx, hModel;
	float lightingOrigin[3];
	float shadowPlane;
	float axis[3][3];
	int32_t nonNormalizedAxes;
	float origin[3];
	int32_t frame;
	float oldorigin[3];
	int32_t oldframe;
	float backlerp;
	int32_t skinNum, customSkin, customShader;
	uint8_t shaderRGBA[4];
	float shaderTexCoord[2];
	float shaderTime;
	fntech3_stvef_mp_refEntityData_t data;
} fntech3_stvef_mp_refEntity_t;

typedef struct {
	char renderer_string[FNTECH3_STVEF_MAX_STRING_CHARS];
	char vendor_string[FNTECH3_STVEF_MAX_STRING_CHARS];
	char version_string[FNTECH3_STVEF_MAX_STRING_CHARS];
	char extensions_string[2 * FNTECH3_STVEF_MAX_STRING_CHARS];
	int32_t maxTextureSize, numTextureUnits;
	int32_t colorBits, depthBits, stencilBits;
	int32_t driverType, hardwareType;
	int32_t deviceSupportsGamma, textureCompression;
	int32_t textureEnvAddAvailable, textureFilterAnisotropicAvailable;
	int32_t vidWidth, vidHeight;
	float windowAspect;
	int32_t displayFrequency;
	int32_t isFullscreen, stereoEnabled, smpActive;
} fntech3_stvef_mp_glconfig_t;

#endif /* FNTECH3_STVEF_GAME_ABI_H */
