/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_FAKK2_GAME_ABI_H
#define FNTECH3_COMPAT_FAKK2_GAME_ABI_H

/* The records Heavy Metal: F.A.K.K.2's two modules hand the engine.
 *
 * Every record size here is measured in the retail modules: each is cleared
 * or copied whole somewhere in cgamex86.dll, by `rep stosd` or `rep movsd`
 * with the count in `ecx` -- 122 dwords for an entity state, 262 for a player
 * state, 91 for a render entity, 111 for a view, 15 for a trace and 12 for an
 * orientation. The member order inside them is the one Ritual's F.A.K.K.2
 * SDK declares, used as a reference observation only, and each record was
 * checked against the Alice record fnTech3 measured out of alice.exe: where
 * the two titles agree the offsets are the same, and every difference is one
 * of a handful of members Alice's engine added or rearranged.
 *
 *  - An entity state is 488 bytes where Alice's is 512. F.A.K.K.2 keeps its
 *    five bone controllers as three parallel arrays -- tags, Euler angles and
 *    quaternions -- where Alice made them five 36-byte records carrying a
 *    scale too, and it has no second render-flags word.
 *  - A player state is Alice's 1,048 bytes, member for member.
 *  - A render entity is 364 bytes where Alice's is 460: no word at +0x08, the
 *    bone controllers as two arrays, and no trailing angles.
 *  - A view is 444 bytes where Alice's is 448: Alice appended one word.
 *
 * Nothing here is interpreted; the conversions to Alice's records, which the
 * engine's Ritual paths already read, are in code/compat/fakk2_convert.hpp.
 * See docs/fntech3/2026-09-24-fakk2-contracts.md. */

#include "alice_game_abi.h"
#include "alice_render_abi.h"
#include "fakk2_module_abi.h"

#include <stdint.h>

#ifdef __cplusplus
#include <cstddef>
#endif

typedef fntech3_alice_trajectory_t fntech3_fakk2_trajectory_t;
typedef fntech3_alice_playerState_t fntech3_fakk2_playerState_t;
typedef fntech3_alice_usercmd_t fntech3_fakk2_usercmd_t;
typedef fntech3_alice_glconfig_t fntech3_fakk2_glconfig_t;
typedef fntech3_alice_trace_t fntech3_fakk2_trace_t;
typedef fntech3_alice_orientation_t fntech3_fakk2_orientation_t;
typedef fntech3_alice_polyVert_t fntech3_fakk2_polyVert_t;
typedef fntech3_alice_server_sound_t fntech3_fakk2_server_sound_t;

enum {
	/* The command list a model definition answers InitCommands and
	 * Frame_Commands with. The game's loop over it at 0x1008854f steps 44
	 * bytes a command -- a count and ten argument pointers -- inside a
	 * 0x1608-byte frame that holds exactly the count and 128 of them.
	 * Alice's engine raised both, to 13 arguments and 192 commands. */
	FNTECH3_FAKK2_MAX_TIKI_CMDS = 128,
	FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS = 10
};

typedef struct {
	int32_t num_args;
	uint32_t args[FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS];  /* char *, Win32 */
} fntech3_fakk2_tiki_singlecmd_t;

typedef struct {
	int32_t num_cmds;
	fntech3_fakk2_tiki_singlecmd_t cmds[FNTECH3_FAKK2_MAX_TIKI_CMDS];
} fntech3_fakk2_tiki_cmd_t;

/* A mark fragment is Quake III's eight bytes: the client game's only
 * CM_MarkFragments site (0x300229da) asks for 128 of them beside 384 points in
 * a frame too small for Alice's 24-byte fragments, which added a plane. */
typedef struct {
	int32_t firstPoint;
	int32_t numPoints;
} fntech3_fakk2_markFragment_t;

typedef struct {
	int32_t number;                            /*    0 */
	int32_t eType;                             /*    4 */
	int32_t eFlags;                            /*    8 */
	fntech3_fakk2_trajectory_t pos;            /*   12 */
	fntech3_fakk2_trajectory_t apos;           /*   48 */
	float netorigin[3];                        /*   84 */
	float origin[3];                           /*   96 */
	float origin2[3];                          /*  108 */
	float netangles[3];                        /*  120 */
	float angles[3];                           /*  132 */
	int32_t constantLight;                     /*  144 */
	int32_t loopSound;                         /*  148 */
	float loopSoundVolume;                     /*  152 */
	float loopSoundMinDist;                    /*  156 */
	int32_t parent;                            /*  160 */
	int32_t tag_num;                           /*  164 */
	int32_t attach_use_angles;                 /*  168 */
	float attach_offset[3];                    /*  172 */
	int32_t modelindex;                        /*  184 */
	int32_t skinNum;                           /*  188 */
	int32_t anim;                              /*  192 */
	int32_t frame;                             /*  196 */
	int32_t crossblend_time;                   /*  200 */
	int32_t torso_anim;                        /*  204 */
	int32_t torso_frame;                       /*  208 */
	int32_t torso_crossblend_time;             /*  212 */
	int32_t bone_tag[FNTECH3_FAKK2_BONE_CONTROLLERS];      /*  216 */
	float bone_angles[FNTECH3_FAKK2_BONE_CONTROLLERS][3];  /*  236 */
	float bone_quat[FNTECH3_FAKK2_BONE_CONTROLLERS][4];    /*  296, the module's own; never networked */
	uint8_t surfaces[FNTECH3_FAKK2_MODEL_SURFACES];        /*  376 */
	int32_t clientNum;                         /*  408 */
	int32_t groundEntityNum;                   /*  412 */
	int32_t solid;                             /*  416 */
	float scale;                               /*  420 */
	float alpha;                               /*  424 */
	int32_t renderfx;                          /*  428 */
	int32_t bindparent;                        /*  432 */
	float quat[4];                             /*  436 */
	float mat[3][3];                           /*  452 */
} fntech3_fakk2_entityState_t;

/* The render entity the client game submits: 91 dwords, cleared whole by ten
 * producers and copied whole three times in cgamex86.dll. */
typedef struct {
	int32_t reType;                            /* 0x000 */
	int32_t renderfx;                          /* 0x004 */
	int32_t hModel;                            /* 0x008 */
	float lightingOrigin[3];                   /* 0x00c */
	float shadowPlane;                         /* 0x018 */
	float axis[3][3];                          /* 0x01c */
	int32_t nonNormalizedAxes;                 /* 0x040 */
	float origin[3];                           /* 0x044 */
	int32_t anim;                              /* 0x050 */
	int32_t oldanim;                           /* 0x054 */
	int16_t oldframe;                          /* 0x058 */
	int16_t frame;                             /* 0x05a */
	float scale;                               /* 0x05c */
	int32_t useAngles;                         /* 0x060 */
	int32_t torso_anim;                        /* 0x064 */
	int32_t torso_oldanim;                     /* 0x068 */
	int16_t torso_frame;                       /* 0x06c */
	int16_t torso_oldframe;                    /* 0x06e */
	float crossblend_lerp;                     /* 0x070 */
	float crossblend_backlerp;                 /* 0x074 */
	int32_t crossblend_anim;                   /* 0x078 */
	int16_t crossblend_frame;                  /* 0x07c */
	int16_t crossblend_oldframe;               /* 0x07e */
	int32_t uselegs;                           /* 0x080 */
	float torso_crossblend_lerp;               /* 0x084 */
	float torso_crossblend_backlerp;           /* 0x088 */
	int32_t torso_crossblend_anim;             /* 0x08c */
	int16_t torso_crossblend_frame;            /* 0x090 */
	int16_t torso_crossblend_oldframe;         /* 0x092 */
	int32_t usetorso;                          /* 0x094 */
	float oldorigin[3];                        /* 0x098 */
	float backlerp;                            /* 0x0a4 */
	float torso_backlerp;                      /* 0x0a8 */
	int32_t skinNum;                           /* 0x0ac */
	int32_t customSkin;                        /* 0x0b0 */
	int32_t customShader;                      /* 0x0b4 */
	uint8_t shaderRGBA[4];                     /* 0x0b8 */
	float shaderTexCoord[2];                   /* 0x0bc */
	float shaderTime;                          /* 0x0c4 */
	int32_t entityNumber;                      /* 0x0c8 */
	uint8_t surfaces[FNTECH3_FAKK2_MODEL_SURFACES]; /* 0x0cc */
	int16_t shader_data[2];                    /* 0x0ec */
	int32_t bone_tag[FNTECH3_FAKK2_BONE_CONTROLLERS];      /* 0x0f0 */
	float bone_quat[FNTECH3_FAKK2_BONE_CONTROLLERS][4];    /* 0x104 */
	int32_t rendererOnly[4];                   /* 0x154, the renderer's own frame and model pointers */
	float radius;                              /* 0x164 */
	float rotation;                            /* 0x168 */
} fntech3_fakk2_refEntity_t;

/* The view: 111 dwords, cleared whole before it is filled. Alice's without
 * the word Alice appended. */
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
} fntech3_fakk2_refdef_t;

/* The game state: F.A.K.K.2's 1,200 configstrings over the same 16,000 bytes
 * of string data Alice's carries. */
typedef struct {
	int32_t stringOffsets[FNTECH3_FAKK2_MAX_CONFIGSTRINGS];
	char stringData[FNTECH3_FAKK2_GAMESTATE_CHARS];
	int32_t dataCount;
} fntech3_fakk2_gamestate_t;

/* The snapshot the client game reads: Alice's with F.A.K.K.2's entity. */
typedef struct {
	int32_t snapFlags;                         /* 0x00000 */
	int32_t ping;                              /* 0x00004 */
	int32_t serverTime;                        /* 0x00008 */
	uint8_t areamask[32];                      /* 0x0000c */
	fntech3_fakk2_playerState_t ps;            /* 0x0002c */
	int32_t numEntities;                       /* 0x00444 */
	fntech3_fakk2_entityState_t entities[FNTECH3_ALICE_MAX_SNAPSHOT_ENTITIES]; /* 0x00448 */
	int32_t numServerCommands;
	int32_t serverCommandSequence;
	int32_t number_of_sounds;
	fntech3_fakk2_server_sound_t sounds[FNTECH3_ALICE_MAX_SERVER_SOUNDS];
} fntech3_fakk2_snapshot_t;

#ifdef __cplusplus
static_assert( sizeof( fntech3_fakk2_entityState_t ) == FNTECH3_FAKK2_ENTITY_STATE_BYTES,
	"the client game copies an entity state as 122 dwords" );
static_assert( sizeof( fntech3_fakk2_playerState_t ) == FNTECH3_FAKK2_PLAYER_STATE_BYTES,
	"and a player state as 262" );
static_assert( sizeof( fntech3_fakk2_refEntity_t ) == 364,
	"a render entity is cleared and copied as 91 dwords" );
static_assert( sizeof( fntech3_fakk2_refdef_t ) == 444, "a view is cleared as 111 dwords" );
static_assert( sizeof( fntech3_fakk2_gamestate_t ) == 20804, "1,200 offsets and 16,000 bytes" );
static_assert( sizeof( fntech3_fakk2_tiki_singlecmd_t ) == 0x2c, "the game steps 44 bytes a command" );
static_assert( sizeof( fntech3_fakk2_tiki_cmd_t ) == 5636, "a count and 128 commands" );
static_assert( sizeof( fntech3_fakk2_markFragment_t ) == 8, "Quake III's fragment" );
static_assert( offsetof( fntech3_fakk2_entityState_t, origin ) == FNTECH3_FAKK2_S_ORIGIN, "origin" );
static_assert( offsetof( fntech3_fakk2_entityState_t, origin2 ) == FNTECH3_FAKK2_S_ORIGIN2, "origin2" );
static_assert( offsetof( fntech3_fakk2_entityState_t, loopSound ) == FNTECH3_FAKK2_S_LOOPSOUND, "loopSound" );
static_assert( offsetof( fntech3_fakk2_entityState_t, loopSoundMinDist ) == FNTECH3_FAKK2_S_LOOPSOUNDMINDIST,
	"loopSoundMinDist" );
static_assert( offsetof( fntech3_fakk2_entityState_t, parent ) == FNTECH3_FAKK2_S_PARENT, "parent" );
static_assert( offsetof( fntech3_fakk2_entityState_t, modelindex ) == FNTECH3_FAKK2_S_MODELINDEX, "modelindex" );
static_assert( offsetof( fntech3_fakk2_entityState_t, bone_tag ) == 216, "the controllers start where Alice's do" );
static_assert( offsetof( fntech3_fakk2_entityState_t, surfaces ) == 376, "surfaces" );
static_assert( offsetof( fntech3_fakk2_entityState_t, solid ) == FNTECH3_FAKK2_S_SOLID, "solid" );
static_assert( offsetof( fntech3_fakk2_entityState_t, renderfx ) == FNTECH3_FAKK2_S_RENDERFX, "renderfx" );
static_assert( offsetof( fntech3_fakk2_entityState_t, bindparent ) == FNTECH3_FAKK2_S_BINDPARENT, "bindparent" );
static_assert( offsetof( fntech3_fakk2_entityState_t, torso_crossblend_time )
	== offsetof( fntech3_alice_entityState_t, torso_crossblend_time ),
	"everything before the controllers is at Alice's offsets" );
static_assert( offsetof( fntech3_fakk2_refEntity_t, surfaces ) + 4
	== offsetof( fntech3_alice_refEntity_t, surfaces ), "Alice inserted one word at +0x08" );
static_assert( offsetof( fntech3_fakk2_refEntity_t, bone_tag ) + 4
	== offsetof( fntech3_alice_refEntity_t, bonectrl ), "the controllers start at the same place" );
static_assert( offsetof( fntech3_fakk2_refEntity_t, radius ) == 0x164, "radius" );
static_assert( offsetof( fntech3_fakk2_refdef_t, sky_axis ) == offsetof( fntech3_alice_refdef_t, sky_axis ),
	"the view is Alice's up to her appended word" );
static_assert( offsetof( fntech3_fakk2_snapshot_t, ps ) == 0x2c, "ps" );
static_assert( offsetof( fntech3_fakk2_snapshot_t, entities ) == 0x448, "entities" );
#endif

#endif
