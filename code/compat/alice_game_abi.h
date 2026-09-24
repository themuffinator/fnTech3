/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_GAME_ABI_H
#define FNTECH3_COMPAT_ALICE_GAME_ABI_H

/* American McGee's Alice's entity and player state records, laid out from its
 * own delta field tables in code/compat/alice_wire_fields.def rather than
 * written by hand: the generator places every networked member at the offset
 * its table gives, and refuses to emit a record that does not tile from zero
 * to its measured size.
 *
 * Those sizes come from the engine, not from the tables. `MSG_ReadDeltaEntity`
 * clears and copies 512 bytes of an entity state, and the client's parse ring
 * strides by 512 (`shl esi, 9` at `0x0042d9e8`); `MSG_WriteDeltaPlayerstate`
 * and its reader both clear and copy 1,048 bytes of a player state, and the
 * client frame that holds one is 1,104 bytes -- 1,048 plus the four-byte area
 * count, the thirty-two area bytes and five trailing integers. The delta
 * tables cover 456 and 1,044 of those, so each record has a tail its own
 * engine never sends; the members in those tails that some other part of the
 * engine reads are named, and the rest are declared as unnamed words.
 *
 * Nothing here is a contract about meaning. The names are the retail engine's
 * own, taken from its tables. What fnTech3 does with each one is decided in
 * code/compat/alice_state_adapter.hpp and code/compat/alice_message_codec.hpp.
 *
 * See docs/fntech3/2026-09-22-alice-wire.md. */

#include "alice_module_abi.h"

#include <stdint.h>

#ifdef __cplusplus
#include <cstddef>
#endif

/* Quake III's trajectory, byte for byte: this part of the record did not
 * change when Ritual took the engine on. */
typedef struct {
	int32_t trType;
	int32_t trTime;
	int32_t trDuration;
	float trBase[3];
	float trDelta[3];
} fntech3_alice_trajectory_t;

/* One of five per-entity bone modifiers, 36 bytes each. The delta table names
 * its tag, its three angles and its scale; the sixteen bytes between the
 * angles and the scale are not networked and are not named here. */
typedef struct {
	int32_t tag;
	float angles[3];
	int32_t unnamed[4];
	float scale;
} fntech3_alice_bonemod_t;

/* The game state the client game reads its configstrings out of. Quake III's
 * three members at Alice's own configstring count: 1,328 offsets, 16,000
 * bytes of string data and the count of what is used, which is 21,316 bytes
 * -- exactly what the retail engine copies into the module's buffer at
 * `0x00423b20`. The 1,328 is not assumed from the size: the profile already
 * carries it, read out of the engine's own bounds checks, and the three
 * members then add up on their own. */
typedef struct {
	int32_t stringOffsets[FNTECH3_ALICE_MAX_CONFIGSTRINGS]; /*     0 */
	char stringData[16000];                    /*  5312 */
	int32_t dataCount;                         /* 21312 */
} fntech3_alice_gamestate_t;

/* The graphics configuration the client game asks for before it draws. It is
 * Quake III's `glconfig_t` with smaller buffers: three 1,024-byte strings, a
 * 2,048-byte extension list and the same seventeen trailing scalars in the
 * same order, which is 5,188 bytes -- exactly what the retail engine copies
 * into the module's buffer.
 *
 * Three offsets anchor it, each from a different part of the engine. The
 * record's first byte is handed to `Info_SetValueForKey` under the key
 * `renderer` at `0x0042adfd`, so the renderer name leads. `+5160` and `+5164`
 * are pushed as a width and a height into `glViewport` and an ortho set-up at
 * `0x0040c560`. And `+5180` is the flag `SCR_UpdateScreen` tests at
 * `0x0042e5c1` to draw a left and a right field instead of one, which is what
 * stereo is. The three land at the positions Quake III's own member order
 * puts them, given those buffer sizes, and the sizes are what makes the
 * record 5,188 rather than any other number. */
typedef struct {
	char renderer_string[1024];                /*    0 */
	char vendor_string[1024];                  /* 1024 */
	char version_string[1024];                 /* 2048 */
	char extensions_string[2048];              /* 3072 */
	int32_t maxTextureSize;                    /* 5120 */
	int32_t maxActiveTextures;                 /* 5124 */
	int32_t colorBits;                         /* 5128 */
	int32_t depthBits;                         /* 5132 */
	int32_t stencilBits;                       /* 5136 */
	int32_t driverType;                        /* 5140 */
	int32_t hardwareType;                      /* 5144 */
	int32_t deviceSupportsGamma;               /* 5148 */
	int32_t textureCompression;                /* 5152 */
	int32_t textureEnvAddAvailable;            /* 5156 */
	int32_t vidWidth;                          /* 5160, pushed into glViewport */
	int32_t vidHeight;                         /* 5164, pushed into glViewport */
	float windowAspect;                        /* 5168 */
	int32_t displayFrequency;                  /* 5172 */
	int32_t isFullscreen;                      /* 5176 */
	int32_t stereoEnabled;                     /* 5180, SCR_UpdateScreen tests it */
	int32_t smpActive;                         /* 5184 */
} fntech3_alice_glconfig_t;

/* The twenty-byte user command. It has no delta table of its own -- its codec
 * names each member in line -- so this is written out rather than generated,
 * from the builder at `0x00428800` and the delta writer at `0x00445bb0`.
 *
 * The three move bytes are forward, right and up, in that order. The key
 * builder stores its three results through ClampChar at +14, +15 and +16
 * (`0x00428287`, `0x00428290`, `0x0042829b`), and fgamex86.dll's pmove, which
 * holds the command at +4 of its own record, reads +14 and +15 into its
 * horizontal wish velocity and +16 as the vertical one (`0x10065319` to
 * `0x10065355`).
 *
 * The byte at +4 sits where Quake III keeps a command's msec, but the retail
 * client zeroes it (`0x0042881b`) and never writes it, the writer never sends
 * it and pmove derives its own frame time, so it keeps a positional name. The
 * byte at +6 is sent but no builder fills it, and no reader of it in the game
 * is established. A command is copied whole, so a member this engine cannot
 * name still reaches the module intact. */
typedef struct {
	int32_t serverTime;                        /*    0, delta-coded against the previous command */
	uint8_t unnamed_4;                         /*    4, never sent; zeroed by the builder and never written */
	uint8_t buttons;                           /*    5, 8 bits; the builder sets its bits at 0x00428750 */
	uint8_t unnamed_6;                         /*    6, 8 bits; never filled by the retail client */
	uint8_t unnamed_7;                         /*    7, never sent */
	int16_t angles[3];                         /*    8, 16 bits each, from the view at 0x004287c0 */
	int8_t move[3];                            /*   14, 8 signed bits each: forward, right, up */
	uint8_t unnamed_17[3];                     /*   17, never sent */
} fntech3_alice_usercmd_t;

typedef struct {
	int32_t number;                            /*    0, the entity number the engine bounds by 1,024 */
	int32_t eType;                             /*    4, 8 bits */
	int32_t eFlags;                            /*    8, 16 bits */
	fntech3_alice_trajectory_t pos;            /*   12, a trajectory */
	fntech3_alice_trajectory_t apos;           /*   48, a trajectory */
	float netorigin[3];                        /*   84, 3 floats */
	int32_t origin[3];                         /*   96, the origin the retail linker adds to a rotated brush */
	float origin2[3];                          /*  108, 3 floats */
	float netangles[3];                        /*  120, 3 floats */
	int32_t angles_unnamed[3];                 /*  132, not in the delta table */
	int32_t constantLight;                     /*  144, 32 bits */
	int32_t loopSound;                         /*  148, 16 bits */
	float loopSoundVolume;                     /*  152, float */
	float loopSoundMinDist;                    /*  156, float */
	int32_t parent;                            /*  160, 16 bits */
	int32_t tag_num;                           /*  164, 10 bits */
	int32_t attach_use_angles;                 /*  168, 1 bit */
	float attach_offset[3];                    /*  172, 3 floats */
	int32_t modelindex;                        /*  184, 16 bits */
	int32_t skinNum;                           /*  188, 16 bits */
	int32_t anim;                              /*  192, 11 bits */
	int32_t frame;                             /*  196, 10 bits */
	int32_t crossblend_time;                   /*  200, 16 signed bits */
	int32_t torso_anim;                        /*  204, 11 bits */
	int32_t torso_frame;                       /*  208, 10 bits */
	int32_t torso_crossblend_time;             /*  212, 16 signed bits */
	fntech3_alice_bonemod_t bonemod[5];        /*  216, five of 36 bytes */
	uint8_t tagged[32];                        /*  396, thirty-two bytes the delta writer sends one at a time under a thirty-two bit mask */
	int32_t clientNum;                         /*  428, 8 bits */
	int32_t groundEntityNum;                   /*  432, 10 bits */
	int32_t solid;                             /*  436, 32 bits */
	float scale;                               /*  440, float */
	float alpha;                               /*  444, float */
	int32_t renderfx;                          /*  448, 32 bits */
	int32_t renderfx2;                         /*  452, 32 bits */
	int32_t ownerNum;                          /*  456, past the delta table: the snapshot walk compares it against 1,023 */
	int32_t unnamed[4];                        /*  460, past the delta table and never networked */
	float axis[3][3];                          /*  476, the rotation the retail linker hands to its bounds transform */
} fntech3_alice_entityState_t;

typedef struct {
	int32_t commandTime;                       /*    0, 32 bits */
	int32_t pm_type;                           /*    4, 8 bits */
	int32_t bobCycle;                          /*    8, 8 bits */
	int32_t pm_flags;                          /*   12, 32 bits */
	int32_t pm_time;                           /*   16, 16 signed bits */
	int32_t pm_runtime;                        /*   20, 16 signed bits */
	float origin[3];                           /*   24, 3 floats */
	float velocity[3];                         /*   36, 3 floats */
	int32_t gravity;                           /*   48, 32 bits */
	int32_t speed;                             /*   52, 16 bits */
	int32_t delta_angles[3];                   /*   56, 3 of 16 bits */
	int32_t groundEntityNum;                   /*   68, 10 bits */
	int32_t unnamed_72[2];                     /*   72, not in the delta table */
	int32_t feetfalling;                       /*   80, 8 bits */
	int32_t unnamed_84[18];                    /*   84, not in the delta table */
	int32_t clientNum;                         /*  156, 8 bits */
	float viewangles[3];                       /*  160, 3 floats */
	int32_t viewheight;                        /*  172, 8 signed bits */
	int32_t block[6][32];                      /*  176, six arrays the delta writer sends under a thirty-two bit mask each, sixteen bits per value */
	int32_t current_music_mood;                /*  944, 8 bits */
	int32_t fallback_music_mood;               /*  948, 8 bits */
	float music_volume;                        /*  952, float */
	float music_volume_fade_time;              /*  956, float */
	int32_t reverb_type;                       /*  960, 6 bits */
	float reverb_level;                        /*  964, float */
	float blend[4];                            /*  968, 4 floats */
	float fov;                                 /*  984, float */
	float camera_origin[3];                    /*  988, 3 floats */
	float camera_angles[3];                    /* 1000, 3 floats */
	float camera_time;                         /* 1012, float */
	float camera_offset[3];                    /* 1016, 3 floats */
	float damage_angles[3];                    /* 1028, 3 floats */
	int32_t camera_flags;                      /* 1040, 16 bits */
	int32_t ping;                              /* 1044, past the delta table: the engine writes a client ping here */
} fntech3_alice_playerState_t;

#ifdef __cplusplus
static_assert( sizeof( fntech3_alice_gamestate_t ) == 21316,
	"the game state is the 21,316 bytes the client game is handed" );
static_assert( sizeof( fntech3_alice_glconfig_t ) == 5188,
	"the graphics record is the 5,188 bytes the client game is handed" );
static_assert( offsetof( fntech3_alice_glconfig_t, vidWidth ) == 5160,
	"the width the engine pushes into glViewport" );
static_assert( offsetof( fntech3_alice_glconfig_t, stereoEnabled ) == 5180,
	"the flag SCR_UpdateScreen tests" );
static_assert( sizeof( fntech3_alice_usercmd_t ) == 20,
	"the client's command ring strides by twenty bytes" );
static_assert( sizeof( fntech3_alice_trajectory_t ) == 36,
	"Alice's trajectory is Quake III's 36 bytes" );
static_assert( sizeof( fntech3_alice_bonemod_t ) == 36,
	"five bone modifiers span 216 to 395, so each is 36 bytes" );
static_assert( sizeof( fntech3_alice_entityState_t )
	== FNTECH3_ALICE_ENTITY_STATE_BYTES,
	"the entity state is the 512 bytes its reader clears and its parse ring strides" );
static_assert( sizeof( fntech3_alice_playerState_t )
	== FNTECH3_ALICE_PLAYER_STATE_BYTES,
	"the player state is the 1,048 bytes its codec clears and copies" );
static_assert( offsetof( fntech3_alice_entityState_t, eType ) == 4, "eType" );
static_assert( offsetof( fntech3_alice_entityState_t, eFlags ) == 8, "eFlags" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trType ) == 12, "pos.trType" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trTime ) == 16, "pos.trTime" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trDuration ) == 20, "pos.trDuration" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trBase[0] ) == 24, "pos.trBase[0]" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trBase[1] ) == 28, "pos.trBase[1]" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trBase[2] ) == 32, "pos.trBase[2]" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trDelta[0] ) == 36, "pos.trDelta[0]" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trDelta[1] ) == 40, "pos.trDelta[1]" );
static_assert( offsetof( fntech3_alice_entityState_t, pos.trDelta[2] ) == 44, "pos.trDelta[2]" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trType ) == 48, "apos.trType" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trTime ) == 52, "apos.trTime" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trDuration ) == 56, "apos.trDuration" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trBase[0] ) == 60, "apos.trBase[0]" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trBase[1] ) == 64, "apos.trBase[1]" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trBase[2] ) == 68, "apos.trBase[2]" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trDelta[0] ) == 72, "apos.trDelta[0]" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trDelta[1] ) == 76, "apos.trDelta[1]" );
static_assert( offsetof( fntech3_alice_entityState_t, apos.trDelta[2] ) == 80, "apos.trDelta[2]" );
static_assert( offsetof( fntech3_alice_entityState_t, netorigin[0] ) == 84, "netorigin[0]" );
static_assert( offsetof( fntech3_alice_entityState_t, netorigin[1] ) == 88, "netorigin[1]" );
static_assert( offsetof( fntech3_alice_entityState_t, netorigin[2] ) == 92, "netorigin[2]" );
static_assert( offsetof( fntech3_alice_entityState_t, origin2[0] ) == 108, "origin2[0]" );
static_assert( offsetof( fntech3_alice_entityState_t, origin2[1] ) == 112, "origin2[1]" );
static_assert( offsetof( fntech3_alice_entityState_t, origin2[2] ) == 116, "origin2[2]" );
static_assert( offsetof( fntech3_alice_entityState_t, netangles[0] ) == 120, "netangles[0]" );
static_assert( offsetof( fntech3_alice_entityState_t, netangles[1] ) == 124, "netangles[1]" );
static_assert( offsetof( fntech3_alice_entityState_t, netangles[2] ) == 128, "netangles[2]" );
static_assert( offsetof( fntech3_alice_entityState_t, constantLight ) == 144, "constantLight" );
static_assert( offsetof( fntech3_alice_entityState_t, loopSound ) == 148, "loopSound" );
static_assert( offsetof( fntech3_alice_entityState_t, loopSoundVolume ) == 152, "loopSoundVolume" );
static_assert( offsetof( fntech3_alice_entityState_t, loopSoundMinDist ) == 156, "loopSoundMinDist" );
static_assert( offsetof( fntech3_alice_entityState_t, parent ) == 160, "parent" );
static_assert( offsetof( fntech3_alice_entityState_t, tag_num ) == 164, "tag_num" );
static_assert( offsetof( fntech3_alice_entityState_t, attach_use_angles ) == 168, "attach_use_angles" );
static_assert( offsetof( fntech3_alice_entityState_t, attach_offset[0] ) == 172, "attach_offset[0]" );
static_assert( offsetof( fntech3_alice_entityState_t, attach_offset[1] ) == 176, "attach_offset[1]" );
static_assert( offsetof( fntech3_alice_entityState_t, attach_offset[2] ) == 180, "attach_offset[2]" );
static_assert( offsetof( fntech3_alice_entityState_t, modelindex ) == 184, "modelindex" );
static_assert( offsetof( fntech3_alice_entityState_t, skinNum ) == 188, "skinNum" );
static_assert( offsetof( fntech3_alice_entityState_t, anim ) == 192, "anim" );
static_assert( offsetof( fntech3_alice_entityState_t, frame ) == 196, "frame" );
static_assert( offsetof( fntech3_alice_entityState_t, crossblend_time ) == 200, "crossblend_time" );
static_assert( offsetof( fntech3_alice_entityState_t, torso_anim ) == 204, "torso_anim" );
static_assert( offsetof( fntech3_alice_entityState_t, torso_frame ) == 208, "torso_frame" );
static_assert( offsetof( fntech3_alice_entityState_t, torso_crossblend_time ) == 212, "torso_crossblend_time" );
static_assert( offsetof( fntech3_alice_entityState_t, clientNum ) == 428, "clientNum" );
static_assert( offsetof( fntech3_alice_entityState_t, groundEntityNum ) == 432, "groundEntityNum" );
static_assert( offsetof( fntech3_alice_entityState_t, solid ) == 436, "solid" );
static_assert( offsetof( fntech3_alice_entityState_t, scale ) == 440, "scale" );
static_assert( offsetof( fntech3_alice_entityState_t, alpha ) == 444, "alpha" );
static_assert( offsetof( fntech3_alice_entityState_t, renderfx ) == 448, "renderfx" );
static_assert( offsetof( fntech3_alice_entityState_t, renderfx2 ) == 452, "renderfx2" );
static_assert( offsetof( fntech3_alice_playerState_t, commandTime ) == 0, "commandTime" );
static_assert( offsetof( fntech3_alice_playerState_t, pm_type ) == 4, "pm_type" );
static_assert( offsetof( fntech3_alice_playerState_t, bobCycle ) == 8, "bobCycle" );
static_assert( offsetof( fntech3_alice_playerState_t, pm_flags ) == 12, "pm_flags" );
static_assert( offsetof( fntech3_alice_playerState_t, pm_time ) == 16, "pm_time" );
static_assert( offsetof( fntech3_alice_playerState_t, pm_runtime ) == 20, "pm_runtime" );
static_assert( offsetof( fntech3_alice_playerState_t, origin[0] ) == 24, "origin[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, origin[1] ) == 28, "origin[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, origin[2] ) == 32, "origin[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, velocity[0] ) == 36, "velocity[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, velocity[1] ) == 40, "velocity[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, velocity[2] ) == 44, "velocity[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, gravity ) == 48, "gravity" );
static_assert( offsetof( fntech3_alice_playerState_t, speed ) == 52, "speed" );
static_assert( offsetof( fntech3_alice_playerState_t, delta_angles[0] ) == 56, "delta_angles[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, delta_angles[1] ) == 60, "delta_angles[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, delta_angles[2] ) == 64, "delta_angles[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, groundEntityNum ) == 68, "groundEntityNum" );
static_assert( offsetof( fntech3_alice_playerState_t, feetfalling ) == 80, "feetfalling" );
static_assert( offsetof( fntech3_alice_playerState_t, clientNum ) == 156, "clientNum" );
static_assert( offsetof( fntech3_alice_playerState_t, viewangles[0] ) == 160, "viewangles[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, viewangles[1] ) == 164, "viewangles[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, viewangles[2] ) == 168, "viewangles[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, viewheight ) == 172, "viewheight" );
static_assert( offsetof( fntech3_alice_playerState_t, current_music_mood ) == 944, "current_music_mood" );
static_assert( offsetof( fntech3_alice_playerState_t, fallback_music_mood ) == 948, "fallback_music_mood" );
static_assert( offsetof( fntech3_alice_playerState_t, music_volume ) == 952, "music_volume" );
static_assert( offsetof( fntech3_alice_playerState_t, music_volume_fade_time ) == 956, "music_volume_fade_time" );
static_assert( offsetof( fntech3_alice_playerState_t, reverb_type ) == 960, "reverb_type" );
static_assert( offsetof( fntech3_alice_playerState_t, reverb_level ) == 964, "reverb_level" );
static_assert( offsetof( fntech3_alice_playerState_t, blend[0] ) == 968, "blend[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, blend[1] ) == 972, "blend[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, blend[2] ) == 976, "blend[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, blend[3] ) == 980, "blend[3]" );
static_assert( offsetof( fntech3_alice_playerState_t, fov ) == 984, "fov" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_angles[0] ) == 1000, "camera_angles[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_angles[1] ) == 1004, "camera_angles[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_angles[2] ) == 1008, "camera_angles[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_origin[0] ) == 988, "camera_origin[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_origin[1] ) == 992, "camera_origin[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_origin[2] ) == 996, "camera_origin[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_offset[0] ) == 1016, "camera_offset[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_offset[1] ) == 1020, "camera_offset[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_offset[2] ) == 1024, "camera_offset[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, damage_angles[0] ) == 1028, "damage_angles[0]" );
static_assert( offsetof( fntech3_alice_playerState_t, damage_angles[1] ) == 1032, "damage_angles[1]" );
static_assert( offsetof( fntech3_alice_playerState_t, damage_angles[2] ) == 1036, "damage_angles[2]" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_time ) == 1012, "camera_time" );
static_assert( offsetof( fntech3_alice_playerState_t, camera_flags ) == 1040, "camera_flags" );
#endif

#endif
