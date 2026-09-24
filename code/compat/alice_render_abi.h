/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_RENDER_ABI_H
#define FNTECH3_COMPAT_ALICE_RENDER_ABI_H

/* The records American McGee's Alice's client game hands its engine and its
 * renderer, measured in the retail alice.exe and base/cgamex86.dll.
 *
 * Every size here was read out of an instruction rather than out of a
 * declaration: a `rep movsd` count, a stride, a buffer's extent, or the gap
 * between two adjacent module buffers. Ritual's F.A.K.K.2 SDK declares
 * earlier versions of most of these records, and was used only as a
 * hypothesis; where Alice differs, the measured shape is the one here.
 *
 * See docs/fntech3/2026-09-23-alice-cgame.md. */

#include "alice_game_abi.h"

#include <stdint.h>

#ifdef __cplusplus
#include <cstddef>
#endif

enum {
	/* The renderer refuses an entity type at or above this (0x00491675). */
	FNTECH3_ALICE_RT_MAX = 8,
	FNTECH3_ALICE_RT_MODEL = 0,
	FNTECH3_ALICE_RT_SPRITE = 2,
	FNTECH3_ALICE_RT_BEAM = 3,
	FNTECH3_ALICE_RT_PORTALSURFACE = 7,

	/* The five per-entity bone controllers the entity state also carries. */
	FNTECH3_ALICE_BONE_CONTROLLERS = 5,
	/* The surface state bytes, copied as eight dwords. */
	FNTECH3_ALICE_MODEL_SURFACES = 32,

	/* The renderer's entity and sprite limits, from its own bounds checks. */
	FNTECH3_ALICE_MAX_RENDER_ENTITIES = 0x3fd,
	FNTECH3_ALICE_MAX_SPRITES = 0x400,
	FNTECH3_ALICE_MAX_POLYS = 768,
	FNTECH3_ALICE_MAX_POLYVERTS = 3072,

	/* The snapshot the client game reads: 256 entities and 32 sounds, each
	 * bounded by the engine (the sound count is read in six bits and refused
	 * above 32 at 0x00446e90). */
	FNTECH3_ALICE_MAX_SNAPSHOT_ENTITIES = 256,
	FNTECH3_ALICE_MAX_SERVER_SOUNDS = 32,

	/* The command list a model definition hands the client game, bounded by
	 * "Too many cmds" at 192 and "Too many args" past 13. */
	FNTECH3_ALICE_MAX_TIKI_CMDS = 192,
	FNTECH3_ALICE_MAX_TIKI_CMD_ARGS = 13
};

/* The render flags the retail renderer tests, at the values it tests. They
 * are Quake III's where Quake III has the flag and Ritual's past that. */
enum {
	FNTECH3_ALICE_RF_THIRD_PERSON = 0x00000001,
	FNTECH3_ALICE_RF_FIRST_PERSON = 0x00000002,
	FNTECH3_ALICE_RF_DEPTHHACK = 0x00000004,
	FNTECH3_ALICE_RF_VIEWLENSFLARE = 0x00000008,
	FNTECH3_ALICE_RF_FRAMELERP = 0x00000010,
	FNTECH3_ALICE_RF_DONTDRAW = 0x00000080,
	FNTECH3_ALICE_RF_LENSFLARE = 0x00000100,
	FNTECH3_ALICE_RF_SHADOW = 0x00000800,
	FNTECH3_ALICE_RF_SKYORIGIN = 0x00002000,
	FNTECH3_ALICE_RF_SKYENTITY = 0x00004000,
	FNTECH3_ALICE_RF_CUSTOMSHADERPASS = 0x00010000,
	FNTECH3_ALICE_RF_MINLIGHT = 0x00020000,
	FNTECH3_ALICE_RF_FULLBRIGHT = 0x00040000,
	FNTECH3_ALICE_RF_LIGHTING_ORIGIN = 0x00080000,
	FNTECH3_ALICE_RF_SHADOW_PLANE = 0x00100000,
	FNTECH3_ALICE_RF_PORTALENTITY = 0x00400000,
	FNTECH3_ALICE_RF_DUALENTITY = 0x00800000,
	FNTECH3_ALICE_RF_SHADOW_PRECISE = 0x04000000
};

/* One bone controller, 36 bytes: a tag, twelve bytes no path reads, a
 * rotation as a quaternion, and a scale the renderer multiplies in at
 * 0x004a5b02. The entity state's own controller record is the same size and
 * carries the same tag and scale at the same offsets. */
typedef struct {
	int32_t tag;
	int32_t unread[3];
	float quat[4];
	float scale;
} fntech3_alice_bonectrl_t;

/* The 460-byte entity the client game submits: `AddRefEntityToScene` at
 * 0x00491650 copies 115 dwords, and every producer in the module clears the
 * same count. Quake III's members at the front, one word Alice inserted at
 * +0x08, Ritual's animation state from +0x54, and a vector of angles Alice
 * appended at +0x1c0. */
typedef struct {
	int32_t reType;                            /* 0x000 */
	int32_t renderfx;                          /* 0x004 */
	int32_t aliceFlags;                        /* 0x008, Alice's; no renderer read found */
	int32_t hModel;                            /* 0x00c */
	float lightingOrigin[3];                   /* 0x010 */
	float shadowPlane;                         /* 0x01c */
	float axis[3][3];                          /* 0x020 */
	int32_t nonNormalizedAxes;                 /* 0x044 */
	float origin[3];                           /* 0x048 */
	int32_t anim;                              /* 0x054 */
	int32_t oldanim;                           /* 0x058 */
	int16_t oldframe;                          /* 0x05c */
	int16_t frame;                             /* 0x05e */
	float scale;                               /* 0x060 */
	int32_t useAngles;                         /* 0x064 */
	int32_t torso_anim;                        /* 0x068 */
	int32_t torso_oldanim;                     /* 0x06c */
	int16_t torso_frame;                       /* 0x070 */
	int16_t torso_oldframe;                    /* 0x072 */
	float crossblend_lerp;                     /* 0x074 */
	float crossblend_backlerp;                 /* 0x078 */
	int32_t crossblend_anim;                   /* 0x07c */
	int16_t crossblend_frame;                  /* 0x080 */
	int16_t crossblend_oldframe;               /* 0x082 */
	int32_t uselegs;                           /* 0x084 */
	float torso_crossblend_lerp;               /* 0x088 */
	float torso_crossblend_backlerp;           /* 0x08c */
	int32_t torso_crossblend_anim;             /* 0x090 */
	int16_t torso_crossblend_frame;            /* 0x094 */
	int16_t torso_crossblend_oldframe;         /* 0x096 */
	int32_t usetorso;                          /* 0x098 */
	float oldorigin[3];                        /* 0x09c */
	float backlerp;                            /* 0x0a8 */
	float torso_backlerp;                      /* 0x0ac */
	int32_t skinNum;                           /* 0x0b0 */
	int32_t customSkin;                        /* 0x0b4 */
	int32_t customShader;                      /* 0x0b8 */
	uint8_t shaderRGBA[4];                     /* 0x0bc */
	float shaderTexCoord[2];                   /* 0x0c0, inferred from the gap */
	float shaderTime;                          /* 0x0c8 */
	int32_t entityNumber;                      /* 0x0cc */
	uint8_t surfaces[FNTECH3_ALICE_MODEL_SURFACES]; /* 0x0d0 */
	int16_t shader_data[2];                    /* 0x0f0 */
	fntech3_alice_bonectrl_t bonectrl[FNTECH3_ALICE_BONE_CONTROLLERS]; /* 0x0f4 */
	int32_t rendererOnly[4];                   /* 0x1a8, the renderer's own frame and model pointers */
	float radius;                              /* 0x1b8 */
	float rotation;                            /* 0x1bc */
	float angles[3];                           /* 0x1c0, Alice's */
} fntech3_alice_refEntity_t;

/* The 448-byte view: the module clears 112 dwords of it before filling it.
 * Quake III's members, Ritual's far plane and sky portal, and one word Alice
 * appended that the renderer reads as "keep the depth buffer". */
typedef struct {
	int32_t x, y, width, height;               /* 0x000 */
	float fov_x, fov_y;                        /* 0x010 */
	float vieworg[3];                          /* 0x018 */
	float viewaxis[3][3];                      /* 0x024 */
	int32_t time;                              /* 0x048 */
	int32_t rdflags;                           /* 0x04c */
	uint8_t areamask[32];                      /* 0x050 */
	char text[8][32];                          /* 0x070 */
	float farplane_distance;                   /* 0x170 */
	float farplane_color[3];                   /* 0x174 */
	int32_t farplane_cull;                     /* 0x180 */
	int32_t sky_portal;                        /* 0x184 */
	float sky_alpha;                           /* 0x188 */
	float sky_origin[3];                       /* 0x18c */
	float sky_axis[3][3];                      /* 0x198 */
	int32_t keepDepth;                         /* 0x1bc, Alice's */
} fntech3_alice_refdef_t;

typedef struct {
	float xyz[3];
	float st[2];
	uint8_t modulate[4];
} fntech3_alice_polyVert_t;

typedef struct {
	float origin[3];
	float axis[3][3];
} fntech3_alice_orientation_t;

/* Quake III's plane, byte for byte. */
typedef struct {
	float normal[3];
	float dist;
	uint8_t type;
	uint8_t signbits;
	uint8_t pad[2];
} fntech3_alice_plane_t;

/* The sixty-byte trace every table fills: the engine, the client game and
 * the game each copy fifteen dwords out of it. There is no shader number and
 * no location, which Quake III later added; the last word is the entity the
 * game module points at, and only the game fills it. */
typedef struct {
	int32_t allsolid;                          /* 0x00 */
	int32_t startsolid;                        /* 0x04 */
	float fraction;                            /* 0x08 */
	float endpos[3];                           /* 0x0c */
	fntech3_alice_plane_t plane;               /* 0x18 */
	int32_t surfaceFlags;                      /* 0x2c */
	int32_t contents;                          /* 0x30 */
	int32_t entityNum;                         /* 0x34 */
	uint32_t ent;                              /* 0x38, a game-module pointer on Win32 */
} fntech3_alice_trace_t;

/* A mark fragment, 24 bytes: the points it uses and the plane they lie in. */
typedef struct {
	int32_t firstPoint;
	int32_t numPoints;
	float normal[3];
	float dist;
} fntech3_alice_markFragment_t;

/* The command list a model definition answers with. */
typedef struct {
	int32_t num_args;
	uint32_t args[FNTECH3_ALICE_MAX_TIKI_CMD_ARGS];  /* char *, Win32 */
} fntech3_alice_tiki_singlecmd_t;

typedef struct {
	int32_t num_cmds;
	fntech3_alice_tiki_singlecmd_t cmds[FNTECH3_ALICE_MAX_TIKI_CMDS];
} fntech3_alice_tiki_cmd_t;

/* A sound the server started, carried in the snapshot. */
typedef struct {
	float origin[3];                           /* 0x00 */
	int32_t entity_number;                     /* 0x0c */
	int32_t channel;                           /* 0x10 */
	int16_t sound_index;                       /* 0x14 */
	int16_t pad;
	float volume;                              /* 0x18 */
	float min_dist;                            /* 0x1c */
	int32_t stop_flag;                         /* 0x20 */
} fntech3_alice_server_sound_t;

/* A debug line, as the renderer's R_DebugLine (0x004766f0) appends it to the
 * array the game module owns: two ends, a colour, an alpha, a width of one and
 * a solid stipple. */
typedef struct {
	float start[3];                            /* 0x00 */
	float end[3];                              /* 0x0c */
	float color[3];                            /* 0x18 */
	float alpha;                               /* 0x24 */
	float width;                               /* 0x28 */
	uint16_t factor;                           /* 0x2c */
	uint16_t pattern;                          /* 0x2e */
} fntech3_alice_debugline_t;

/* The snapshot the client game reads, 133,332 bytes: the module's two
 * snapshot buffers sit exactly that far apart. */
typedef struct {
	int32_t snapFlags;                         /* 0x00000 */
	int32_t ping;                              /* 0x00004 */
	int32_t serverTime;                        /* 0x00008 */
	uint8_t areamask[32];                      /* 0x0000c */
	fntech3_alice_playerState_t ps;            /* 0x0002c */
	int32_t numEntities;                       /* 0x00444 */
	fntech3_alice_entityState_t entities[FNTECH3_ALICE_MAX_SNAPSHOT_ENTITIES]; /* 0x00448 */
	int32_t numServerCommands;                 /* 0x20448 */
	int32_t serverCommandSequence;             /* 0x2044c */
	int32_t number_of_sounds;                  /* 0x20450 */
	fntech3_alice_server_sound_t sounds[FNTECH3_ALICE_MAX_SERVER_SOUNDS]; /* 0x20454 */
} fntech3_alice_snapshot_t;

#ifdef __cplusplus
static_assert( sizeof( fntech3_alice_bonectrl_t ) == 36, "a controller is 36 bytes" );
static_assert( sizeof( fntech3_alice_refEntity_t ) == 460,
	"AddRefEntityToScene copies 115 dwords" );
static_assert( offsetof( fntech3_alice_refEntity_t, hModel ) == 0x0c, "hModel" );
static_assert( offsetof( fntech3_alice_refEntity_t, origin ) == 0x48, "origin" );
static_assert( offsetof( fntech3_alice_refEntity_t, anim ) == 0x54, "anim" );
static_assert( offsetof( fntech3_alice_refEntity_t, frame ) == 0x5e, "frame" );
static_assert( offsetof( fntech3_alice_refEntity_t, scale ) == 0x60, "scale" );
static_assert( offsetof( fntech3_alice_refEntity_t, usetorso ) == 0x98, "usetorso" );
static_assert( offsetof( fntech3_alice_refEntity_t, backlerp ) == 0xa8, "backlerp" );
static_assert( offsetof( fntech3_alice_refEntity_t, customShader ) == 0xb8, "customShader" );
static_assert( offsetof( fntech3_alice_refEntity_t, shaderTime ) == 0xc8, "shaderTime" );
static_assert( offsetof( fntech3_alice_refEntity_t, entityNumber ) == 0xcc, "entityNumber" );
static_assert( offsetof( fntech3_alice_refEntity_t, surfaces ) == 0xd0, "surfaces" );
static_assert( offsetof( fntech3_alice_refEntity_t, bonectrl ) == 0xf4, "controllers" );
static_assert( offsetof( fntech3_alice_refEntity_t, radius ) == 0x1b8, "radius" );
static_assert( offsetof( fntech3_alice_refEntity_t, angles ) == 0x1c0, "angles" );
static_assert( sizeof( fntech3_alice_refdef_t ) == 448, "the module clears 112 dwords" );
static_assert( offsetof( fntech3_alice_refdef_t, time ) == 0x48, "time" );
static_assert( offsetof( fntech3_alice_refdef_t, text ) == 0x70, "text" );
static_assert( offsetof( fntech3_alice_refdef_t, farplane_distance ) == 0x170, "far plane" );
static_assert( offsetof( fntech3_alice_refdef_t, sky_portal ) == 0x184, "sky portal" );
static_assert( offsetof( fntech3_alice_refdef_t, keepDepth ) == 0x1bc, "depth" );
static_assert( sizeof( fntech3_alice_polyVert_t ) == 24, "numVerts * 24 bytes" );
static_assert( sizeof( fntech3_alice_orientation_t ) == 48, "twelve dwords" );
static_assert( sizeof( fntech3_alice_trace_t ) == 60, "fifteen dwords" );
static_assert( offsetof( fntech3_alice_trace_t, plane ) == 0x18, "plane" );
static_assert( offsetof( fntech3_alice_trace_t, entityNum ) == 0x34, "entityNum" );
static_assert( sizeof( fntech3_alice_markFragment_t ) == 24, "fragment stride" );
static_assert( sizeof( fntech3_alice_tiki_cmd_t ) == 10756, "the module's buffer" );
static_assert( sizeof( fntech3_alice_server_sound_t ) == 36, "a server sound" );
static_assert( sizeof( fntech3_alice_debugline_t ) == 48, "the game allocates 48 bytes a line" );
static_assert( sizeof( fntech3_alice_snapshot_t ) == 0x208d4,
	"the module's two snapshot buffers are this far apart" );
static_assert( offsetof( fntech3_alice_snapshot_t, ps ) == 0x2c, "ps" );
static_assert( offsetof( fntech3_alice_snapshot_t, numEntities ) == 0x444, "numEntities" );
static_assert( offsetof( fntech3_alice_snapshot_t, sounds ) == 0x20454, "sounds" );
#endif

#endif
