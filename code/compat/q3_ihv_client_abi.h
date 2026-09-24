/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_CLIENT_ABI_H
#define FNTECH3_Q3_IHV_CLIENT_ABI_H

#include "q3_ihv_game_abi.h"

/* Independently observed native client transfers. All dwords in these byte
 * records are little endian, including on diagnostic hosts of another endian.
 * They are never aliases for retail gameState_t or snapshot_t. See the client
 * record observations in docs/fntech3/Q3_IHV.md. */
enum {
	FNTECH3_Q3_IHV_GAMESTATE_BYTES = 20100,
	FNTECH3_Q3_IHV_CONFIG_DATA_OFFSET = 4096,
	FNTECH3_Q3_IHV_CONFIG_DATA_BYTES = 16000,
	FNTECH3_Q3_IHV_CONFIG_COUNT_OFFSET = 20096,
	FNTECH3_Q3_IHV_SNAPSHOT_BYTES = 348,
	FNTECH3_Q3_IHV_SNAPSHOT_PLAYER_OFFSET = 60,
	FNTECH3_Q3_IHV_COMMAND_HISTORY = 64,
	FNTECH3_Q3_IHV_PARSE_ENTITIES = 1024
};
typedef struct { uint8_t bytes[FNTECH3_Q3_IHV_GAMESTATE_BYTES]; } fntech3_q3_ihv_gameState_t;
typedef struct { uint8_t bytes[FNTECH3_Q3_IHV_SNAPSHOT_BYTES]; } fntech3_q3_ihv_snapshot_t;

/* CM_LerpTag returns a pointer to a full MD3 tag, not retail orientation_t.
 * The original interpolator writes the pose at byte 64 and leaves the name
 * untouched. Observed at quake3.exe 0x40a4c0/0x40a570; see Q3_IHV.md. */
typedef struct {
	char name[64];
	float origin[3];
	float axis[3][3];
} fntech3_q3_ihv_tag_t;

typedef struct {
	float mins[3], maxs[3], localOrigin[3], radius;
	char name[16];
} fntech3_q3_ihv_model_frame_t;

/* Cgame reads model->frames directly when positioning HUD models (DLL RVA
 * 0x42cb). Engine-owned mirrors must retain both arrays for the registration
 * lifetime. These are 32-bit native addresses, never retail clip handles. */
typedef struct {
	int32_t headnode;
	float mins[3], maxs[3];
	char name[64];
	int32_t isMD3, frameCount, tagCount;
	uint32_t frames, tags;
} fntech3_q3_ihv_collision_model_t;

#endif
