/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_UI_ABI_H
#define FNTECH3_JKA_UI_ABI_H
#include <stdint.h>

/* Legacy multiplayer UI transfers from Raven's published contracts as
 * carried by OpenJK at 1a6a643427aa347553e9073dac5570b33337c4d9:
 * codemp/ui/ui_public.h and codemp/qcommon/q_shared.h. */
typedef struct {
    int32_t connState, connectPacketCount, clientNum;
    char servername[1024], updateInfoString[1024], messageString[1024];
} fntech3_jka_mp_uiClientState_t;

typedef struct {
    int32_t type, subtype, intvalue;
    float floatvalue;
    char string[1024];
} fntech3_jka_mp_pc_token_t;

typedef struct {
    int32_t tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int32_t tm_wday, tm_yday, tm_isdst;
} fntech3_jka_mp_qtime_t;

/* Retail JKA 1.01 jamp.exe SHA256
 * e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b:
 * UI_GETGLCONFIG(44) dispatches at 0x4246fb; the rep movsd at 0x42470f
 * copies 19 words from 0x914070. Width/height use +56/+60; renderer/vendor
 * are pointers at +0/+4; colorBits is printed from +28. The 76-byte record
 * agrees with pinned OpenJK codemp/rd-common/tr_types.h at x86. These are
 * native addresses, not QVM offsets and not engine-internal char arrays. */
typedef struct {
    uint32_t renderer_string, vendor_string, version_string, extensions_string;
    int32_t maxTextureSize, maxActiveTextures;
    float maxTextureFilterAnisotropy;
    int32_t colorBits, depthBits, stencilBits;
    int32_t deviceSupportsGamma, textureCompression, textureEnvAddAvailable;
    int32_t clampToEdgeAvailable, vidWidth, vidHeight, displayFrequency;
    int32_t isFullscreen, stereoEnabled;
} fntech3_jka_mp_glconfig_t;
#endif
