/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_RTCW_GAME_ABI_H
#define FNTECH3_COMPAT_RTCW_GAME_ABI_H

#include <stdint.h>

enum {
#define FNTECH3_RTCW_KEY(name, value) FNTECH3_RTCW_##name = value,
#include "rtcw_keycodes.def"
#undef FNTECH3_RTCW_KEY
	FNTECH3_RTCW_K_CHAR_FLAG = 1024,
	FNTECH3_RTCW_KEYCATCH_CONSOLE = 1,
	FNTECH3_RTCW_KEYCATCH_UI = 2,
	FNTECH3_RTCW_KEYCATCH_MESSAGE = 4,
	FNTECH3_RTCW_KEYCATCH_CGAME = 8
};

/* Frozen, external RTCW memory contracts. These grouped POD records are
 * independently authored from size, offset and field-encoding observations of
 * id Software's published SP/MP headers, compiled as Windows x86 and x64.
 * Source identities and provenance: docs/fntech3/2026-09-14-rtcw-contracts.md.
 * No game logic is included. Never cast an engine-internal object to these
 * types; SP and MP deliberately have distinct command, player and render types.
 */
enum {
	FNTECH3_RTCW_MAX_CONFIGSTRINGS = 2048,
	FNTECH3_RTCW_SOURCE_FILENAME_CHARS = 128,
	FNTECH3_RTCW_MAX_GAMESTATE_CHARS = 16000,
	FNTECH3_RTCW_MAX_EVENTS = 4,
	FNTECH3_RTCW_MAX_WEAPONS = 64,
	FNTECH3_RTCW_SP_MAX_CLIENTS = 128,
	FNTECH3_RTCW_MP_MAX_CLIENTS = 64,
	FNTECH3_RTCW_MAX_ENTITIES_IN_SNAPSHOT = 256,
	FNTECH3_RTCW_MAX_MAP_AREA_BYTES = 32,
	FNTECH3_RTCW_GENTITYNUM_BITS = 10
};

/* RTCW MP's gamestate cvar is shared by the engine, game and cgame. */
enum {
	FNTECH3_RTCW_MP_GS_INITIALIZE = -1,
	FNTECH3_RTCW_MP_GS_PLAYING = 0,
	FNTECH3_RTCW_MP_GS_WARMUP_COUNTDOWN = 1,
	FNTECH3_RTCW_MP_GS_WARMUP = 2,
	FNTECH3_RTCW_MP_GS_INTERMISSION = 3,
	FNTECH3_RTCW_MP_GS_WAITING_FOR_PLAYERS = 4,
	FNTECH3_RTCW_MP_GS_RESET = 5
};

/* SP and MP BOTLIB_UPDATENTITY share this 112-byte input record. */
typedef struct {
	int32_t type, flags;
	float origin[3], angles[3], old_origin[3], mins[3], maxs[3];
	int32_t groundent, solid, modelindex, modelindex2, frame, event, eventParm;
	int32_t powerups, weapon, legsAnim, torsoAnim;
} fntech3_rtcw_bot_entitystate_t;

typedef struct {
	float origin[3], velocity[3], viewoffset[3];
	int32_t entitynum, client;
	float thinktime;
	int32_t presencetype;
	float viewangles[3];
	int32_t or_moveflags;
} fntech3_rtcw_bot_initmove_t;

/* Goal records share a shape with Quake, but their flag meanings differ. */
typedef struct {
	float origin[3];
	int32_t areanum;
	float mins[3], maxs[3];
	int32_t entitynum, number, flags, iteminfo;
} fntech3_rtcw_bot_goal_t;

/* A separate result contract: RTCW bit 256 means future view, not avoidance. */
typedef struct {
	int32_t failure, type, blocked, blockentity, traveltype, flags, weapon;
	float movedir[3], ideal_viewangles[3];
} fntech3_rtcw_bot_moveresult_t;

typedef struct {
	float thinktime, dir[3], speed, viewangles[3];
	int32_t actionflags, weapon;
} fntech3_rtcw_bot_input_t;

/* Navigation traces are distinct from the engine collision trace above the
 * game boundary. The shared SP/MP prediction result has no Quake endarea and
 * encodes endcontents as a numeric float, not integer bits. */
typedef struct {
	int32_t startsolid;
	float fraction, endpos[3];
	int32_t ent, lastarea, area, planenum;
} fntech3_rtcw_aas_trace_t;

typedef struct {
	float endpos[3], velocity[3];
	fntech3_rtcw_aas_trace_t trace;
	int32_t presencetype, stopevent;
	float endcontents, time;
	int32_t frames;
} fntech3_rtcw_aas_clientmove_t;

enum {
	FNTECH3_RTCW_ACTION_ATTACK = 1, FNTECH3_RTCW_ACTION_USE = 2,
	FNTECH3_RTCW_ACTION_RESPAWN = 4, FNTECH3_RTCW_ACTION_JUMP = 8,
	FNTECH3_RTCW_ACTION_MOVEUP = 8, FNTECH3_RTCW_ACTION_CROUCH = 16,
	FNTECH3_RTCW_ACTION_MOVEDOWN = 16, FNTECH3_RTCW_ACTION_MOVEFORWARD = 32,
	FNTECH3_RTCW_ACTION_MOVEBACK = 64, FNTECH3_RTCW_ACTION_MOVELEFT = 128,
	FNTECH3_RTCW_ACTION_MOVERIGHT = 256, FNTECH3_RTCW_ACTION_DELAYEDJUMP = 512,
	FNTECH3_RTCW_ACTION_TALK = 1024, FNTECH3_RTCW_ACTION_GESTURE = 2048,
	FNTECH3_RTCW_ACTION_WALK = 4096, FNTECH3_RTCW_ACTION_RELOAD = 8192
};

enum {
	FNTECH3_RTCW_MFL_ONGROUND = 2,
	FNTECH3_RTCW_MFL_WATERJUMP = 16,
	FNTECH3_RTCW_MFL_TELEPORTED = 32,
	FNTECH3_RTCW_MFL_WALK = 256
};

/* Original retail cvar flags. iortcw subsequently assigned 0x0800 to
 * SERVER_CREATED and moved MP WOLFINFO to 0x4000; those are distinct ABIs. */
enum {
	FNTECH3_RTCW_CVAR_ARCHIVE = 0x0001,
	FNTECH3_RTCW_CVAR_USERINFO = 0x0002,
	FNTECH3_RTCW_CVAR_SERVERINFO = 0x0004,
	FNTECH3_RTCW_CVAR_SYSTEMINFO = 0x0008,
	FNTECH3_RTCW_CVAR_INIT = 0x0010,
	FNTECH3_RTCW_CVAR_LATCH = 0x0020,
	FNTECH3_RTCW_CVAR_ROM = 0x0040,
	FNTECH3_RTCW_CVAR_USER_CREATED = 0x0080,
	FNTECH3_RTCW_CVAR_TEMP = 0x0100,
	FNTECH3_RTCW_CVAR_CHEAT = 0x0200,
	FNTECH3_RTCW_CVAR_NORESTART = 0x0400,
	FNTECH3_RTCW_MP_CVAR_WOLFINFO = 0x0800,
	FNTECH3_IORTCW_MP_CVAR_WOLFINFO = 0x4000,
	FNTECH3_RTCW_MP_CS_WOLFINFO = 36
};

typedef struct {
	int32_t handle, modificationCount;
	float value;
	int32_t integer;
	char string[256];
} fntech3_rtcw_vmCvar_t;

enum {
	FNTECH3_RTCW_CA_UNINITIALIZED = 0,
	FNTECH3_RTCW_CA_DISCONNECTED = 1,
	FNTECH3_RTCW_CA_AUTHORIZING = 2,
	FNTECH3_RTCW_CA_CONNECTING = 3,
	FNTECH3_RTCW_CA_CHALLENGING = 4,
	FNTECH3_RTCW_CA_CONNECTED = 5,
	FNTECH3_RTCW_CA_LOADING = 6,
	FNTECH3_RTCW_CA_PRIMED = 7,
	FNTECH3_RTCW_CA_ACTIVE = 8,
	FNTECH3_RTCW_CA_CINEMATIC = 9,
	FNTECH3_RTCW_TC_NONE = 0,
	FNTECH3_RTCW_TC_S3TC = 1,
	FNTECH3_RTCW_TC_EXT_COMP_S3TC = 2,
	FNTECH3_IORTCW_TC_EXT_COMP_S3TC = 3,
	FNTECH3_RTCW_GLDRV_ICD = 0,
	FNTECH3_RTCW_GLDRV_STANDALONE = 1,
	FNTECH3_RTCW_GLDRV_VOODOO = 2,
	FNTECH3_RTCW_GLHW_GENERIC = 0,
	FNTECH3_RTCW_GLHW_3DFX_2D3D = 1,
	FNTECH3_RTCW_GLHW_RIVA128 = 2,
	FNTECH3_RTCW_GLHW_RAGEPRO = 3,
	FNTECH3_RTCW_GLHW_PERMEDIA2 = 4
};

enum {
	FNTECH3_RTCW_TT_STRING = 1,
	FNTECH3_RTCW_TT_LITERAL = 2,
	FNTECH3_RTCW_TT_NUMBER = 3,
	FNTECH3_RTCW_TT_NAME = 4,
	FNTECH3_RTCW_TT_PUNCTUATION = 5
};

/* Shared and RTCW-only menu requests have distinct SP/MP values. */
enum {
#define FNTECH3_RTCW_MENU(name, sp, mp) FNTECH3_RTCW_SP_UIMENU_##name = sp, FNTECH3_RTCW_MP_UIMENU_##name = mp,
#include "rtcw_menus.def"
#undef FNTECH3_RTCW_MENU
	FNTECH3_RTCW_CGAME_EVENT_NONE = 0,
	FNTECH3_RTCW_CGAME_EVENT_TEAMMENU = 1,
	FNTECH3_RTCW_CGAME_EVENT_SCOREBOARD = 2,
	FNTECH3_RTCW_CGAME_EVENT_EDITHUD = 3,
	FNTECH3_RTCW_STEREO_CENTER = 0,
	FNTECH3_RTCW_STEREO_LEFT = 1,
	FNTECH3_RTCW_STEREO_RIGHT = 2
};

typedef struct {
	int32_t connState, connectPacketCount, clientNum;
	char servername[1024], updateInfoString[1024], messageString[1024];
} fntech3_rtcw_uiClientState_t;

typedef struct {
	char renderer_string[1024], vendor_string[1024], version_string[1024];
	char extensions_string[4096];
	int32_t maxTextureSize, maxActiveTextures, colorBits, depthBits, stencilBits;
	int32_t driverType, hardwareType, deviceSupportsGamma, textureCompression;
	int32_t textureEnvAddAvailable, anisotropicAvailable;
	float maxAnisotropy;
	int32_t NVFogAvailable, NVFogMode, ATIMaxTruformTess, ATINormalMode, ATIPointMode;
	int32_t vidWidth, vidHeight;
	float windowAspect;
	int32_t displayFrequency, isFullscreen, stereoEnabled, smpActive;
	int32_t textureFilterAnisotropicAvailable;
} fntech3_rtcw_glconfig_t;

typedef struct {
	int32_t type, subtype, intvalue;
	float floatvalue;
	char string[1024];
} fntech3_rtcw_pc_token_t;

typedef struct {
	int32_t tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
} fntech3_rtcw_qtime_t;

typedef struct {
	int32_t height, top, bottom, pitch, xSkip, imageWidth, imageHeight;
	float s, t, s2, t2;
	int32_t glyph;
	char shaderName[32];
} fntech3_rtcw_glyphInfo_t;

typedef struct {
	fntech3_rtcw_glyphInfo_t glyphs[256];
	float glyphScale;
	char name[64];
} fntech3_rtcw_fontInfo_t;

/* Equal in original SP/MP and iortcw; several bits collide with Q3/QL flags. */
enum {
	FNTECH3_RTCW_SVF_NOCLIENT = 0x0001,
	FNTECH3_RTCW_SVF_VISDUMMY = 0x0004,
	FNTECH3_RTCW_SVF_BOT = 0x0008,
	FNTECH3_RTCW_SVF_CASTAI = 0x0010,
	FNTECH3_RTCW_SVF_BROADCAST = 0x0020,
	FNTECH3_RTCW_SVF_PORTAL = 0x0040,
	FNTECH3_RTCW_SVF_USE_CURRENT_ORIGIN = 0x0080,
	FNTECH3_RTCW_SVF_NOFOOTSTEPS = 0x0100,
	FNTECH3_RTCW_SVF_CAPSULE = 0x0200,
	FNTECH3_RTCW_SVF_VISDUMMY_MULTIPLE = 0x0400,
	FNTECH3_RTCW_SVF_SINGLECLIENT = 0x0800,
	FNTECH3_RTCW_SVF_NOSERVERINFO = 0x1000,
	FNTECH3_RTCW_SVF_NOTSINGLECLIENT = 0x2000
};

typedef struct {
	int32_t trType, trTime, trDuration;
	float trBase[3], trDelta[3];
} fntech3_rtcw_trajectory_t;

typedef struct {
	float normal[3], dist;
	uint8_t type, signbits, padding[2];
} fntech3_rtcw_plane_t;

typedef struct {
	int32_t allsolid, startsolid;
	float fraction, endpos[3];
	fntech3_rtcw_plane_t plane;
	int32_t surfaceFlags, contents, entityNum;
} fntech3_rtcw_trace_t;

typedef struct {
	int32_t stringOffsets[FNTECH3_RTCW_MAX_CONFIGSTRINGS];
	char stringData[FNTECH3_RTCW_MAX_GAMESTATE_CHARS];
	int32_t dataCount;
} fntech3_rtcw_gameState_t;

/* Equal record sizes do not imply equal commands: SP ends in a 16-bit damage
 * value; MP uses those bytes for team/class/weapon setup and identification. */
typedef struct {
	int32_t serverTime;
	uint8_t buttons, wbuttons, weapon, holdable;
	int32_t angles[3];
	int8_t forwardmove, rightmove, upmove, wolfkick;
} fntech3_rtcw_usercmdPrefix_t;

typedef struct {
	fntech3_rtcw_usercmdPrefix_t common;
	uint16_t cld;
} fntech3_rtcw_sp_usercmd_t;

typedef struct {
	fntech3_rtcw_usercmdPrefix_t common;
	uint8_t mpSetup, identClient;
} fntech3_rtcw_mp_usercmd_t;

typedef struct {
	int32_t number, eType, eFlags;
	fntech3_rtcw_trajectory_t pos, apos;
	int32_t time, time2;
	float origin[3], origin2[3], angles[3], angles2[3];
	int32_t otherEntityNum, otherEntityNum2, groundEntityNum;
	int32_t constantLight, dl_intensity, loopSound;
	int32_t modelindex, modelindex2, clientNum, frame, solid;
	int32_t event, eventParm, eventSequence;
	int32_t events[FNTECH3_RTCW_MAX_EVENTS], eventParms[FNTECH3_RTCW_MAX_EVENTS];
	int32_t powerups, weapon, legsAnim, torsoAnim, density, dmgFlags;
	int32_t onFireStart, onFireEnd, aiChar, teamNum;
	int32_t effect1Time, effect2Time, effect3Time, aiState, animMovetype;
} fntech3_rtcw_entityState_t;

typedef struct {
	int32_t commandTime, pm_type, bobCycle, pm_flags, pm_time;
	float origin[3], velocity[3];
	int32_t weaponTime, weaponDelay, grenadeTimeLeft, gravity;
	float leanf;
	int32_t speed, delta_angles[3], groundEntityNum;
	int32_t legsTimer, legsAnim, torsoTimer, torsoAnim, movementDir;
	int32_t eFlags, eventSequence;
	int32_t events[FNTECH3_RTCW_MAX_EVENTS], eventParms[FNTECH3_RTCW_MAX_EVENTS];
	int32_t oldEventSequence, externalEvent, externalEventParm, externalEventTime;
	int32_t clientNum, weapon, weaponstate, item;
	float viewangles[3];
	int32_t viewheight, damageEvent, damageYaw, damagePitch, damageCount;
	int32_t stats[16], persistant[16], powerups[16];
	int32_t ammo[FNTECH3_RTCW_MAX_WEAPONS], ammoclip[FNTECH3_RTCW_MAX_WEAPONS];
	int32_t holdable[16], holding, weapons[2];
	float mins[3], maxs[3], crouchMaxZ;
	float crouchViewHeight, standViewHeight, deadViewHeight;
	float runSpeedScale, sprintSpeedScale, crouchSpeedScale;
	int32_t viewlocked, viewlocked_entNum;
	float friction;
	int32_t aiChar, teamNum, gunfx, onFireStart;
	int32_t serverCursorHint, serverCursorHintVal;
	fntech3_rtcw_trace_t serverCursorHintTrace;
	int32_t ping, pmove_framecount, entityEventSequence, sprintTime, sprintExertTime;
	int32_t classWeaponTime, jumpTime, weapAnimTimer, weapAnim, releasedFire;
	float aimSpreadScaleFloat;
	int32_t aimSpreadScale, lastFireTime, quickGrenTime, leanStopDebounceTime;
	int32_t weapHeat[FNTECH3_RTCW_MAX_WEAPONS], curWeapHeat, venomTime;
} fntech3_rtcw_playerStatePrefix_t;

typedef struct {
	fntech3_rtcw_playerStatePrefix_t common;
	int32_t accShowBits, accHideBits, aiState;
	float footstepCount;
} fntech3_rtcw_sp_playerState_t;

typedef struct {
	fntech3_rtcw_playerStatePrefix_t common;
	int32_t aiState, identifyClient;
} fntech3_rtcw_mp_playerState_t;

/* RTCW's shared server record contains a second entityState, inside r.
 * The linked flag is therefore at byte 576, not at the end of the outer s. */
typedef struct {
	fntech3_rtcw_entityState_t s;
	int32_t linked, linkcount, svFlags, singleClient, bmodel;
	float mins[3], maxs[3];
	int32_t contents;
	float absmin[3], absmax[3], currentOrigin[3], currentAngles[3];
	int32_t ownerNum, eventTime;
} fntech3_rtcw_entitySharedPrefix_t;

typedef struct {
	fntech3_rtcw_entityState_t s;
	fntech3_rtcw_entitySharedPrefix_t r;
} fntech3_rtcw_sp_sharedEntity_t;

typedef struct {
	fntech3_rtcw_entityState_t s;
	fntech3_rtcw_entitySharedPrefix_t r;
	int32_t worldflags;
} fntech3_rtcw_mp_sharedEntity_t;

typedef struct {
	int32_t snapFlags, ping, serverTime;
	uint8_t areamask[FNTECH3_RTCW_MAX_MAP_AREA_BYTES];
	fntech3_rtcw_sp_playerState_t ps;
	int32_t numEntities;
	fntech3_rtcw_entityState_t entities[FNTECH3_RTCW_MAX_ENTITIES_IN_SNAPSHOT];
	int32_t numServerCommands, serverCommandSequence;
} fntech3_rtcw_sp_snapshot_t;

typedef struct {
	int32_t snapFlags, ping, serverTime;
	uint8_t areamask[FNTECH3_RTCW_MAX_MAP_AREA_BYTES];
	fntech3_rtcw_mp_playerState_t ps;
	int32_t numEntities;
	fntech3_rtcw_entityState_t entities[FNTECH3_RTCW_MAX_ENTITIES_IN_SNAPSHOT];
	int32_t numServerCommands, serverCommandSequence;
} fntech3_rtcw_mp_snapshot_t;

/* Original SP/MP renderer meanings, verified by probe_rtcw_contracts.py.
 * Identical numeric flags can intentionally have multiple meanings. */
enum {
	FNTECH3_RTCW_RT_MODEL = 0, FNTECH3_RTCW_RT_POLY = 1,
	FNTECH3_RTCW_RT_SPRITE = 2, FNTECH3_RTCW_RT_SPLASH = 3,
	FNTECH3_RTCW_RT_BEAM = 4, FNTECH3_RTCW_RT_RAIL_CORE = 5,
	FNTECH3_RTCW_RT_RAIL_CORE_TAPER = 6, FNTECH3_RTCW_RT_RAIL_RINGS = 7,
	FNTECH3_RTCW_RT_LIGHTNING = 8, FNTECH3_RTCW_RT_PORTALSURFACE = 9,
	FNTECH3_RTCW_RT_MAX_REF_ENTITY_TYPE = 10,
	FNTECH3_RTCW_RF_MINLIGHT = 1, FNTECH3_RTCW_RF_THIRD_PERSON = 2,
	FNTECH3_RTCW_RF_FIRST_PERSON = 4, FNTECH3_RTCW_RF_DEPTHHACK = 8,
	FNTECH3_RTCW_RF_NOSHADOW = 64, FNTECH3_RTCW_RF_LIGHTING_ORIGIN = 128,
	FNTECH3_RTCW_RF_SHADOW_PLANE = 256, FNTECH3_RTCW_RF_WRAP_FRAMES = 512,
	FNTECH3_RTCW_RF_HILIGHT = 256, FNTECH3_RTCW_RF_BLINK = 512,
	FNTECH3_RTCW_RDF_NOWORLDMODEL = 1, FNTECH3_RTCW_RDF_HYPERSPACE = 4,
	FNTECH3_RTCW_RDF_SKYBOXPORTAL = 8, FNTECH3_RTCW_RDF_UNDERWATER = 16,
	FNTECH3_RTCW_RDF_DRAWINGSKY = 32, FNTECH3_RTCW_RDF_SNOOPERVIEW = 64,
	FNTECH3_RTCW_SP_RDF_DRAWSKYBOX = 16,
	FNTECH3_RTCW_REFLAG_ONLYHAND = 1, FNTECH3_RTCW_REFLAG_FORCE_LOD = 8,
	FNTECH3_RTCW_REFLAG_ORIENT_LOD = 16, FNTECH3_RTCW_REFLAG_DEAD_LOD = 32,
	FNTECH3_RTCW_SP_REFLAG_ZOMBIEFX = 2, FNTECH3_RTCW_SP_REFLAG_ZOMBIEFX2 = 4,
	FNTECH3_RTCW_SP_REFLAG_SCALEDSPHERECULL = 64, FNTECH3_RTCW_SP_REFLAG_FULL_LOD = 8
};

typedef struct {
	int32_t reType, renderfx, hModel;
	float lightingOrigin[3], shadowPlane, axis[3][3], torsoAxis[3][3];
	int32_t nonNormalizedAxes;
	float origin[3];
	int32_t frame, torsoFrame;
} fntech3_rtcw_refEntityPrefix_t;

typedef struct {
	float oldorigin[3];
	int32_t oldframe, oldTorsoFrame;
	float backlerp, torsoBacklerp;
	int32_t skinNum, customSkin, customShader;
	uint8_t shaderRGBA[4];
	float shaderTexCoord[2], shaderTime, radius, rotation, fireRiseDir[3];
	int32_t fadeStartTime, fadeEndTime;
	float hilightIntensity;
	int32_t reFlags, entityNum;
} fntech3_rtcw_refEntitySuffix_t;

typedef struct {
	fntech3_rtcw_refEntityPrefix_t common;
	float scale[3];
	fntech3_rtcw_refEntitySuffix_t appearance;
} fntech3_rtcw_sp_refEntity_t;

typedef struct {
	fntech3_rtcw_refEntityPrefix_t common;
	fntech3_rtcw_refEntitySuffix_t appearance;
} fntech3_rtcw_mp_refEntity_t;

typedef struct {
	int32_t mode, hint, startTime, finishTime;
	float color[4], start, end;
	int32_t useEndForClip;
	float density;
	int32_t registered, drawsky, clearscreen;
} fntech3_rtcw_fog_t;

typedef struct {
	int32_t x, y, width, height;
	float fov_x, fov_y, vieworg[3], viewaxis[3][3];
	int32_t time, rdflags;
	uint8_t areamask[FNTECH3_RTCW_MAX_MAP_AREA_BYTES];
	char text[8][32];
} fntech3_rtcw_refdefPrefix_t;

typedef struct {
	fntech3_rtcw_refdefPrefix_t common;
	fntech3_rtcw_fog_t glfog;
	int32_t fogDirty;
} fntech3_rtcw_sp_refdef_t;

typedef struct {
	fntech3_rtcw_refdefPrefix_t common;
	fntech3_rtcw_fog_t glfog;
} fntech3_rtcw_mp_refdef_t;

typedef struct {
	float xyz[3], st[2];
	uint8_t modulate[4];
} fntech3_rtcw_polyVert_t;

/* CG_CM_MARKFRAGMENTS returns XYZST vertices at a twenty-byte stride.
 * Negative numPoints denotes texture coordinates supplied by the renderer. */
typedef struct {
	int32_t firstPoint, numPoints;
} fntech3_rtcw_markFragment_t;

typedef struct {
	float xyz[3], st[2];
} fntech3_rtcw_markPoint_t;

typedef struct {
	float origin[3], axis[3][3];
} fntech3_rtcw_orientation_t;

#endif
