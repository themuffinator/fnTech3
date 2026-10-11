/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_RENDER_ABI_H
#define FNTECH3_JK2_RENDER_ABI_H
#include <stdint.h>
#include <stddef.h>

/* Independently declared Outcast 1.02a MP records. jk2mp.exe SHA256
 * 2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a:
 * entity0x487bf0 copies53 words, reads Ghoul2 at208; ui.qvm model drawing
 * procedure58210 zeroes212 bytes, writes axis12, origin52, oldorigin64,
 * lightingOrigin108, model8 and flags4. The other entity field meanings
 * agree with the pinned OpenJK codemp/rd-common/tr_types.h layout.
 * View0x487f20 reads axis36, time72, flags76, mask80 and text112..367.
 * The QVM zeroes368 bytes: this is NOT Academy's384-byte view record.
 * Poly0x4879a0 copies24 bytes per vertex. Tag0x485a00 writes origin then
 * axes through48 bytes. No proprietary code or data is incorporated. */
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
    int32_t skinNum, customSkin, miniStart, miniCount;
    uint8_t effectData[28];
    float endTime, saberLength, angles[3], modelScale[3];
    uint32_t ghoul2;
} fntech3_jk2_mp_refEntity_t;

typedef struct {
    int32_t x, y, width, height;
    float fov_x, fov_y, vieworg[3], viewaxis[3][3];
    int32_t time, rdflags;
    uint8_t areamask[32];
    char text[8][32];
} fntech3_jk2_mp_refdef_t;

typedef struct { float xyz[3], st[2]; uint8_t modulate[4]; } fntech3_jk2_mp_polyVert_t;
typedef struct { float origin[3], axis[3][3]; } fntech3_jk2_mp_orientation_t;

#ifdef __cplusplus
static_assert(sizeof(fntech3_jk2_mp_refEntity_t) == 212);
static_assert(offsetof(fntech3_jk2_mp_refEntity_t, axis) == 12);
static_assert(offsetof(fntech3_jk2_mp_refEntity_t, origin) == 52);
static_assert(offsetof(fntech3_jk2_mp_refEntity_t, oldorigin) == 64);
static_assert(offsetof(fntech3_jk2_mp_refEntity_t, lightingOrigin) == 108);
static_assert(offsetof(fntech3_jk2_mp_refEntity_t, ghoul2) == 208);
static_assert(sizeof(fntech3_jk2_mp_refdef_t) == 368);
static_assert(offsetof(fntech3_jk2_mp_refdef_t, viewaxis) == 36);
static_assert(offsetof(fntech3_jk2_mp_refdef_t, time) == 72);
static_assert(offsetof(fntech3_jk2_mp_refdef_t, areamask) == 80);
static_assert(offsetof(fntech3_jk2_mp_refdef_t, text) == 112);
static_assert(sizeof(fntech3_jk2_mp_polyVert_t) == 24);
static_assert(offsetof(fntech3_jk2_mp_polyVert_t, st) == 12);
static_assert(offsetof(fntech3_jk2_mp_polyVert_t, modulate) == 20);
static_assert(sizeof(fntech3_jk2_mp_orientation_t) == 48);
static_assert(offsetof(fntech3_jk2_mp_orientation_t, axis) == 12);
#endif
#endif
