/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_CLIENT_ABI_H
#define FNTECH3_JKA_CLIENT_ABI_H

#include "jka_game_abi.h"
#include "jka_ui_abi.h"
#include <stddef.h>

/* Retail multiplayer client contracts, never canonical engine structures.
 * jamp.exe SHA256 e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b:
 * CG_GETGAMESTATE at VA 0x412f2e copies 0x1645 dwords (22804 bytes).
 * CL_GetSnapshot at 0x4117d0 copies both 1552-byte player records, with the
 * vehicle at 1596, entity count at 3148, 532-byte entities at 3152 and command
 * sequence at 139348. The entity cap is 256. GetUserCmd at 0x411780 copies 28.
 * RE_AddRefEntityToScene at 0x4ab300 copies 53 dwords (212 bytes), with the
 * Ghoul2 address read from 208. RE_RenderScene at 0x4abc90 consumes the view
 * fields and text through 384 bytes; jka_render_adapter.hpp pins each read.
 * Their remaining field/type meanings use OpenJK
 * 1a6a643427aa347553e9073dac5570b33337c4d9 codemp/rd-common/tr_types.h.
 * The static renderer subset does not enable Raven effects or Ghoul2.
 * Independently declared interoperability facts; no external implementation
 * text is incorporated. */
#define FNTECH3_JKA_MAX_ENTITIES_IN_SNAPSHOT 256
#define FNTECH3_JKA_CLIENT_AREAMASK_BYTES 32
#define FNTECH3_JKA_CGAME_SHARED_BUFFER_BYTES 2048 /* pinned cg_public.h */

/* Retail jamp.exe0x40931f..0x4093e2 writes this shared record then invokes
 * cgame export15. It includes entity collision, not just the world model. */
typedef struct {
    fntech3_jka_trace_t result;
    float start[3], mins[3], maxs[3], end[3];
    int32_t skipEntity, mask;
} fntech3_jka_cgame_trace_t;

/* Retail jamp.exe 0x40c560 writes this shared record for CreateEffect's
 * CameraShake case (0x40b2ca) then invokes cgame export 31; retail
 * cgamex86.dll 0x3004281c passes its four fields to the client game's own
 * view shake (0x30062c70). */
typedef struct {
    float origin[3];
    float intensity;
    int32_t radius, time;
} fntech3_jka_cgame_camera_shake_t;

typedef struct {
    int32_t stringOffsets[FNTECH3_JKA_MAX_CONFIGSTRINGS];
    char stringData[FNTECH3_JKA_MAX_GAMESTATE_CHARS];
    int32_t dataCount;
} fntech3_jka_mp_gameState_t;

typedef struct {
    int32_t snapFlags, ping, serverTime;
    uint8_t areamask[FNTECH3_JKA_CLIENT_AREAMASK_BYTES];
    fntech3_jka_playerState_t ps, vps;
    int32_t numEntities;
    fntech3_jka_entityState_t entities[FNTECH3_JKA_MAX_ENTITIES_IN_SNAPSHOT];
    int32_t numServerCommands, serverCommandSequence;
} fntech3_jka_mp_snapshot_t;

/* Retail RE_AddPolyToScene0x4ab090 copies 24 bytes per vertex and walks
 * positions at that stride. Field offsets use the separately pinned OpenJK
 * tr_types.h layout; this is an independently declared module POD. */
typedef struct {
    float xyz[3], st[2];
    uint8_t modulate[4];
} fntech3_jka_mp_polyVert_t;

/* Retail import213 and tag query0x4a9860 publish 48 bytes: origin followed
 * by three axes. This independently declared POD never aliases Fn types. */
typedef struct { float origin[3], axis[3][3]; } fntech3_jka_mp_orientation_t;

/* Retail core publishes a contiguous three-by-four float matrix. */
typedef struct { float matrix[3][4]; } fntech3_jka_mp_mdxaBone_t;

typedef union {
    struct { float rotation, radius; uint8_t vertRGBA[4][4]; } sprite;
    struct { float width, width2, stscale; } line;
    struct { float width, control1[3], control2[3]; } bezier;
    struct { float width, width2, stscale, height, bias; int32_t wrap; } cylinder;
    struct { float width, deviation, stscale; int32_t wrap, taper; } electricity;
} fntech3_jka_mp_refEntity_data_t;

typedef struct {
    int32_t reType, renderfx, hModel;
    float axis[3][3];
    int32_t nonNormalizedAxes;
    float origin[3], oldorigin[3];
    int32_t customShader;
    uint8_t shaderRGBA[4];
    float shaderTexCoord[2], radius, rotation, shaderTime;
    int32_t frame;
    float lightingOrigin[3], shadowPlane;
    int32_t oldframe;
    float backlerp;
    int32_t skinNum, customSkin;
    struct { int32_t miniStart, miniCount; } uRefEnt;
    fntech3_jka_mp_refEntity_data_t data;
    float endTime, saberLength, angles[3], modelScale[3];
    uint32_t ghoul2; /* Win32 module address; never an engine-owned pointer. */
} fntech3_jka_mp_refEntity_t;

typedef struct {
    int32_t x, y, width, height;
    float fov_x, fov_y, vieworg[3], viewangles[3], viewaxis[3][3];
    int32_t viewContents, time, rdflags;
    uint8_t areamask[FNTECH3_JKA_CLIENT_AREAMASK_BYTES];
    char text[8][32];
} fntech3_jka_mp_refdef_t;

#ifdef __cplusplus
static_assert(sizeof(fntech3_jka_cgame_trace_t)==104);
static_assert(offsetof(fntech3_jka_cgame_trace_t,start)==48);
static_assert(offsetof(fntech3_jka_cgame_trace_t,mins)==60);
static_assert(offsetof(fntech3_jka_cgame_trace_t,maxs)==72);
static_assert(offsetof(fntech3_jka_cgame_trace_t,end)==84);
static_assert(offsetof(fntech3_jka_cgame_trace_t,skipEntity)==96);
static_assert(offsetof(fntech3_jka_cgame_trace_t,mask)==100);
static_assert(sizeof(fntech3_jka_cgame_camera_shake_t)==24);
static_assert(offsetof(fntech3_jka_cgame_camera_shake_t,intensity)==12);
static_assert(offsetof(fntech3_jka_cgame_camera_shake_t,radius)==16);
static_assert(offsetof(fntech3_jka_cgame_camera_shake_t,time)==20);
static_assert(sizeof(fntech3_jka_mp_orientation_t) == 48);
static_assert(offsetof(fntech3_jka_mp_orientation_t, origin) == 0);
static_assert(offsetof(fntech3_jka_mp_orientation_t, axis) == 12);
static_assert(sizeof(fntech3_jka_mp_mdxaBone_t) == 48);
static_assert(offsetof(fntech3_jka_mp_mdxaBone_t, matrix) == 0);
static_assert(sizeof(fntech3_jka_mp_polyVert_t) == 24);
static_assert(offsetof(fntech3_jka_mp_polyVert_t, xyz) == 0);
static_assert(offsetof(fntech3_jka_mp_polyVert_t, st) == 12);
static_assert(offsetof(fntech3_jka_mp_polyVert_t, modulate) == 20);
static_assert(sizeof(fntech3_jka_mp_gameState_t) == 22804);
static_assert(offsetof(fntech3_jka_mp_gameState_t, dataCount) == 22800);
static_assert(sizeof(fntech3_jka_mp_snapshot_t) == 139352);
static_assert(offsetof(fntech3_jka_mp_snapshot_t, ps) == 44);
static_assert(offsetof(fntech3_jka_mp_snapshot_t, vps) == 1596);
static_assert(offsetof(fntech3_jka_mp_snapshot_t, numEntities) == 3148);
static_assert(offsetof(fntech3_jka_mp_snapshot_t, entities) == 3152);
static_assert(offsetof(fntech3_jka_mp_snapshot_t, serverCommandSequence) == 139348);
static_assert(sizeof(fntech3_jka_mp_refEntity_t) == 212);
static_assert(offsetof(fntech3_jka_mp_refEntity_t, ghoul2) == 208);
static_assert(sizeof(fntech3_jka_mp_refdef_t) == 384);
static_assert(offsetof(fntech3_jka_mp_refdef_t, areamask) == 96);
#endif
#endif
