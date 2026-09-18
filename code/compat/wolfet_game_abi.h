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
	/* Renderer record sizes from ET's cgame/tr_types.h and game/q_shared.h. */
	FNTECH3_WOLFET_MAX_MAP_AREA_BYTES = 32,
	FNTECH3_WOLFET_MAX_RENDER_STRINGS = 8,
	FNTECH3_WOLFET_MAX_RENDER_STRING_LENGTH = 32,
	FNTECH3_WOLFET_MAX_STRING_CHARS = 1024,
	FNTECH3_WOLFET_CS_PLAYERS = 689,
	FNTECH3_WOLFET_CS_WOLFINFO = 21,
	FNTECH3_WOLFET_UI_API_VERSION = 4,
	FNTECH3_WOLFET_CGAME_API_VERSION = 3,
	FNTECH3_WOLFET_GAME_API_VERSION = 8,
	FNTECH3_WOLFET_SVF_IGNOREBMODELEXTENTS = 0x00004000,
	FNTECH3_WOLFET_SVF_SELF_PORTAL = 0x00008000,
	FNTECH3_WOLFET_SVF_SELF_PORTAL_EXCLUSIVE = 0x00010000,
	/* Client-boundary sizes from ET's cgame/tr_types.h, game/q_shared.h and
	 * ui/ui_public.h. */
	FNTECH3_WOLFET_MAX_QPATH = 64,
	/* ET's own callers reserve this much for a parser source filename. */
	FNTECH3_WOLFET_SOURCE_FILENAME_CHARS = 128,
	FNTECH3_WOLFET_GLYPHS_PER_FONT = 256,
	FNTECH3_WOLFET_MAX_PB_VERTS = 1025,
	FNTECH3_WOLFET_MAX_PB_INDICIES = FNTECH3_WOLFET_MAX_PB_VERTS * 6,
	/* ET reference-entity render flags. Bits 0x100 and 0x200 are ET's own
	 * highlight and blink; 0x400 forces the highest detail level. */
	FNTECH3_WOLFET_RF_MINLIGHT = 0x0001,
	FNTECH3_WOLFET_RF_THIRD_PERSON = 0x0002,
	FNTECH3_WOLFET_RF_FIRST_PERSON = 0x0004,
	FNTECH3_WOLFET_RF_DEPTHHACK = 0x0008,
	FNTECH3_WOLFET_RF_NOSHADOW = 0x0010,
	FNTECH3_WOLFET_RF_LIGHTING_ORIGIN = 0x0020,
	FNTECH3_WOLFET_RF_SHADOW_PLANE = 0x0040,
	FNTECH3_WOLFET_RF_WRAP_FRAMES = 0x0080,
	FNTECH3_WOLFET_RF_HILIGHT = 0x0100,
	FNTECH3_WOLFET_RF_BLINK = 0x0200,
	FNTECH3_WOLFET_RF_FORCENOLOD = 0x0400,
	/* ET scene flags. Bit 8 is a skybox portal, not Quake's no-first-person. */
	FNTECH3_WOLFET_RDF_NOWORLDMODEL = 1,
	FNTECH3_WOLFET_RDF_HYPERSPACE = 4,
	FNTECH3_WOLFET_RDF_SKYBOXPORTAL = 8,
	FNTECH3_WOLFET_RDF_UNDERWATER = 16,
	FNTECH3_WOLFET_RDF_DRAWINGSKY = 32,
	FNTECH3_WOLFET_RDF_SNOOPERVIEW = 64,
	/* ET reference-entity types, in ET's own ordinal order. */
	FNTECH3_WOLFET_RT_MODEL = 0,
	FNTECH3_WOLFET_RT_POLY = 1,
	FNTECH3_WOLFET_RT_SPRITE = 2,
	FNTECH3_WOLFET_RT_SPLASH = 3,
	FNTECH3_WOLFET_RT_BEAM = 4,
	FNTECH3_WOLFET_RT_RAIL_CORE = 5,
	FNTECH3_WOLFET_RT_RAIL_CORE_TAPER = 6,
	FNTECH3_WOLFET_RT_RAIL_RINGS = 7,
	FNTECH3_WOLFET_RT_LIGHTNING = 8,
	FNTECH3_WOLFET_RT_PORTALSURFACE = 9,
	/* ET parser token types, shared with its UI and cgame parsers. */
	FNTECH3_WOLFET_TT_STRING = 1,
	FNTECH3_WOLFET_TT_LITERAL = 2,
	FNTECH3_WOLFET_TT_NUMBER = 3,
	FNTECH3_WOLFET_TT_NAME = 4,
	FNTECH3_WOLFET_TT_PUNCTUATION = 5,
	/* ET renderer driver, hardware and compression ordinals. */
	FNTECH3_WOLFET_GLDRV_ICD = 0,
	FNTECH3_WOLFET_GLDRV_STANDALONE = 1,
	FNTECH3_WOLFET_GLDRV_VOODOO = 2,
	FNTECH3_WOLFET_GLHW_GENERIC = 0,
	FNTECH3_WOLFET_GLHW_3DFX_2D3D = 1,
	FNTECH3_WOLFET_GLHW_RIVA128 = 2,
	FNTECH3_WOLFET_GLHW_RAGEPRO = 3,
	FNTECH3_WOLFET_GLHW_PERMEDIA2 = 4,
	FNTECH3_WOLFET_TC_NONE = 0,
	FNTECH3_WOLFET_TC_S3TC = 1,
	FNTECH3_WOLFET_TC_EXT_COMP_S3TC = 2,
	/* ET connection states, as its UI reports them. */
	FNTECH3_WOLFET_CA_UNINITIALIZED = 0,
	FNTECH3_WOLFET_CA_DISCONNECTED = 1,
	FNTECH3_WOLFET_CA_AUTHORIZING = 2,
	FNTECH3_WOLFET_CA_CONNECTING = 3,
	FNTECH3_WOLFET_CA_CHALLENGING = 4,
	FNTECH3_WOLFET_CA_CONNECTED = 5,
	FNTECH3_WOLFET_CA_LOADING = 6,
	FNTECH3_WOLFET_CA_PRIMED = 7,
	FNTECH3_WOLFET_CA_ACTIVE = 8,
	FNTECH3_WOLFET_CA_CINEMATIC = 9
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

/* ET's prediction result, independently declared from its game/be_aas.h and
 * game/botlib.h. The embedded trace is a world trace: Quake's prediction
 * reports a navigation trace with none of these fields. */
typedef struct {
	char name[16];
	int32_t flags, value;
} fntech3_wolfet_bsp_surface_t;

typedef struct {
	float normal[3];
	float dist;
	uint8_t type, signbits, pad[2];
} fntech3_wolfet_cplane_t;

typedef struct {
	int32_t allsolid, startsolid;
	float fraction;
	float endpos[3];
	fntech3_wolfet_cplane_t plane;
	float exp_dist;
	int32_t sidenum;
	fntech3_wolfet_bsp_surface_t surface;
	int32_t contents, ent;
} fntech3_wolfet_bsp_trace_t;

typedef struct {
	float endpos[3];
	float velocity[3];
	fntech3_wolfet_bsp_trace_t trace;
	int32_t presencetype, stopevent;
	float endcontents, time;
	int32_t frames;
} fntech3_wolfet_aas_clientmove_t;

/* ET renderer records, independently declared from its cgame/tr_types.h and
 * game/q_shared.h. ET's reference entity carries a torso skeleton, per-frame
 * model handles and fire/fade fields that Quake's does not, and its scene
 * definition embeds a fog description; neither may be shared with another
 * profile's renderer records. */
typedef struct {
	float xyz[3];
	float st[2];
	uint8_t modulate[4];
} fntech3_wolfet_polyVert_t;

typedef struct {
	float origin[3];
	float axis[3][3];
} fntech3_wolfet_orientation_t;

typedef struct {
	int32_t firstPoint;
	int32_t numPoints;
} fntech3_wolfet_markFragment_t;

typedef struct {
	int32_t reType, renderfx, hModel;
	float lightingOrigin[3];
	float shadowPlane;
	float axis[3][3], torsoAxis[3][3];
	int32_t nonNormalizedAxes;
	float origin[3];
	int32_t frame, frameModel, torsoFrame, torsoFrameModel;
	float oldorigin[3];
	int32_t oldframe, oldframeModel, oldTorsoFrame, oldTorsoFrameModel;
	float backlerp, torsoBacklerp;
	int32_t skinNum, customSkin, customShader;
	uint8_t shaderRGBA[4];
	float shaderTexCoord[2];
	float shaderTime, radius, rotation;
	float fireRiseDir[3];
	int32_t fadeStartTime, fadeEndTime;
	float hilightIntensity;
	int32_t reFlags, entityNum;
} fntech3_wolfet_refEntity_t;

typedef struct {
	int32_t mode, hint, startTime, finishTime;
	float color[4];
	float start, end;
	int32_t useEndForClip;
	float density;
	int32_t registered, drawsky, clearscreen;
} fntech3_wolfet_glfog_t;

typedef struct {
	int32_t x, y, width, height;
	float fov_x, fov_y;
	float vieworg[3];
	float viewaxis[3][3];
	int32_t time, rdflags;
	uint8_t areamask[FNTECH3_WOLFET_MAX_MAP_AREA_BYTES];
	char text[FNTECH3_WOLFET_MAX_RENDER_STRINGS][FNTECH3_WOLFET_MAX_RENDER_STRING_LENGTH];
	fntech3_wolfet_glfog_t glfog;
} fntech3_wolfet_refdef_t;

typedef struct {
	char renderer_string[FNTECH3_WOLFET_MAX_STRING_CHARS];
	char vendor_string[FNTECH3_WOLFET_MAX_STRING_CHARS];
	char version_string[FNTECH3_WOLFET_MAX_STRING_CHARS];
	char extensions_string[FNTECH3_WOLFET_MAX_STRING_CHARS * 4];
	int32_t maxTextureSize, maxActiveTextures;
	int32_t colorBits, depthBits, stencilBits;
	int32_t driverType, hardwareType;
	int32_t deviceSupportsGamma, textureCompression;
	int32_t textureEnvAddAvailable, anisotropicAvailable;
	float maxAnisotropy;
	int32_t NVFogAvailable, NVFogMode;
	int32_t ATIMaxTruformTess, ATINormalMode, ATIPointMode;
	int32_t vidWidth, vidHeight;
	float windowAspect;
	int32_t displayFrequency;
	int32_t isFullscreen, stereoEnabled, smpActive;
} fntech3_wolfet_glconfig_t;

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

/* ET client-boundary records. The font record is the one ET's UI and cgame
 * both fill in place; the polygon buffer is ET's only batched geometry
 * submission and is over fifty kilobytes, so it is never copied onto a stack.
 * ET's client state is what its UI reads to drive the connection screens. */
typedef struct {
	int32_t height, top, bottom, pitch, xSkip, imageWidth, imageHeight;
	float s, t, s2, t2;
	int32_t glyph;
	char shaderName[32];
} fntech3_wolfet_glyphInfo_t;

typedef struct {
	fntech3_wolfet_glyphInfo_t glyphs[FNTECH3_WOLFET_GLYPHS_PER_FONT];
	float glyphScale;
	char name[FNTECH3_WOLFET_MAX_QPATH];
} fntech3_wolfet_fontInfo_t;

typedef struct {
	float xyz[FNTECH3_WOLFET_MAX_PB_VERTS][4];
	float st[FNTECH3_WOLFET_MAX_PB_VERTS][2];
	uint8_t color[FNTECH3_WOLFET_MAX_PB_VERTS][4];
	int32_t numVerts;
	int32_t indicies[FNTECH3_WOLFET_MAX_PB_INDICIES];
	int32_t numIndicies;
	int32_t shader;
} fntech3_wolfet_polyBuffer_t;

typedef struct {
	int32_t connState, connectPacketCount, clientNum;
	char servername[FNTECH3_WOLFET_MAX_STRING_CHARS];
	char updateInfoString[FNTECH3_WOLFET_MAX_STRING_CHARS];
	char messageString[FNTECH3_WOLFET_MAX_STRING_CHARS];
} fntech3_wolfet_uiClientState_t;

#endif
