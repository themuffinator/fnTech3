/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Gameplay clock contracts observed in the installed engines. See
 * docs/fntech3/2026-10-09-profile-frame-rules.md. No retail code incorporated. */
#ifndef FNTECH3_PROFILE_FRAME_TIME_H
#define FNTECH3_PROFILE_FRAME_TIME_H

#include "game_profile.h"

enum {
	FRAME_RETAIL = 1,
	FRAME_FIXED_TICK = 2,
	FRAME_X87_PRODUCT = 4,
	FRAME_ALWAYS_FLOOR = 8,
	FRAME_CAP_200 = 16,
	FRAME_CINEMATIC_CAP = 32,
	FRAME_CAMERA_SCALE = 64,
	FRAME_FRACTION = 128,
	FRAME_FLOAT_INPUT = 256,
	FRAME_FIXEDTIME_FLOAT = 512,
	/* The game runs on the server's own clock (svs.time), which a map change
	 * keeps and only a shutdown clears: GAME_INIT, the settling frames and
	 * every snapshot carry on from the last level's time (jamp.exe 1.01 hands
	 * it to GAME_INIT at 0x45dd50, jk2mp.exe 1.02 at 0x44cf54). Quake3e's
	 * empty-server reset and sv_levelTimeReset do not apply. See
	 * docs/fntech3/2026-10-15-jedi-broadcast-level-time.md. */
	FRAME_KEEP_LEVEL_TIME = 1024
};

static inline unsigned int Com_ProfileFrameRules( comGameId_t profile ) {
	switch ( profile ) {
	case COM_GAME_QUAKE3:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_CAMERA_SCALE;
	case COM_GAME_JK2_MP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_CAMERA_SCALE | FRAME_KEEP_LEVEL_TIME;
	case COM_GAME_QUAKELIVE:
		return FRAME_RETAIL | FRAME_X87_PRODUCT | FRAME_CAMERA_SCALE;
	case COM_GAME_RTCW_SP:
	case COM_GAME_RTCW_MP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT;
	case COM_GAME_WOLFET:
		return FRAME_RETAIL | FRAME_FIXED_TICK;
	case COM_GAME_JKA_MP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_CAMERA_SCALE | FRAME_FLOAT_INPUT
			| FRAME_KEEP_LEVEL_TIME;
	case COM_GAME_JKA_SP:
	case COM_GAME_JK2_SP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_ALWAYS_FLOOR | FRAME_CINEMATIC_CAP | FRAME_FRACTION;
	case COM_GAME_STVEF_SP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_ALWAYS_FLOOR | FRAME_CINEMATIC_CAP;
	case COM_GAME_STVEF_MP:
	case COM_GAME_ALICE:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_ALWAYS_FLOOR;
	case COM_GAME_QUAKE3_IHV:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_ALWAYS_FLOOR | FRAME_CAP_200 | FRAME_FIXEDTIME_FLOAT;
	case COM_GAME_Q3TEST:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_ALWAYS_FLOOR | FRAME_CAP_200;
	default:
		/* F.A.K.K.2 and MOHAA's encrypted engines do not establish these
		 * contracts. Keep their existing rules until direct evidence does. */
		return 0;
	}
}

#endif
