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
	 * every snapshot carry on from the last level's time (GAME_INIT takes it
	 * in quake3.exe 1.32 at 0x438514, WolfSP.exe at 0x435fd5, WolfMP.exe at
	 * 0x4389d7, etded.exe 2.60b at 0x42bcb8, stvoyHM.exe 1.20 at
	 * 0x433268, jamp.exe 1.01 at 0x45dd50 and jk2mp.exe 1.02 at 0x44cf54).
	 * Quake3e's empty-server reset and sv_levelTimeReset do not apply. See
	 * docs/fntech3/2026-10-15-jedi-broadcast-level-time.md and
	 * docs/fntech3/2026-10-15-level-clock.md. */
	FRAME_KEEP_LEVEL_TIME = 1024,
	/* The level's own clock (sv.time) starts at zero with every level,
	 * whoever is connected: quakelive_steam.exe hands each connected client
	 * the old level's time (0x4e35f6), which its snapshots add until the
	 * client acknowledges the new gamestate (0x4e5185, cleared at 0x4e06ab),
	 * then clears the clock with the level (0x4e363a). sv_levelTimeReset is
	 * always on. See docs/fntech3/2026-10-15-level-clock.md. */
	FRAME_RESET_LEVEL_TIME = 2048,
	/* Raven's single-player engines clear the level, clock included, with
	 * every spawn and start the new level at 1000, whoever is connected:
	 * stvoy.exe 1.20 (0x425a9b, 0x425ac9), jasp.exe 1.01 (0x439d0d, 0x439d35)
	 * and jk2sp.exe 1.02a (0x437226, 0x437250). GAME_INIT takes 1000 and the
	 * settling frames run at 1000, 1100 and 1200. A full save's own time
	 * replaces it after the spawn (jasp.exe 0x43bac4, jk2sp.exe 0x43908e).
	 * See docs/fntech3/2026-10-16-jedi-sp-level-clock.md. */
	FRAME_LEVEL_TIME_1000 = 4096
};

static inline unsigned int Com_ProfileFrameRules( comGameId_t profile ) {
	switch ( profile ) {
	case COM_GAME_QUAKE3:
	case COM_GAME_JK2_MP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_CAMERA_SCALE | FRAME_KEEP_LEVEL_TIME;
	case COM_GAME_QUAKELIVE:
		return FRAME_RETAIL | FRAME_X87_PRODUCT | FRAME_CAMERA_SCALE | FRAME_RESET_LEVEL_TIME;
	case COM_GAME_RTCW_SP:
	case COM_GAME_RTCW_MP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_KEEP_LEVEL_TIME;
	case COM_GAME_WOLFET:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_KEEP_LEVEL_TIME;
	case COM_GAME_JKA_MP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_CAMERA_SCALE | FRAME_FLOAT_INPUT
			| FRAME_KEEP_LEVEL_TIME;
	case COM_GAME_JKA_SP:
	case COM_GAME_JK2_SP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_ALWAYS_FLOOR | FRAME_CINEMATIC_CAP | FRAME_FRACTION
			| FRAME_LEVEL_TIME_1000;
	case COM_GAME_STVEF_SP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_ALWAYS_FLOOR | FRAME_CINEMATIC_CAP
			| FRAME_LEVEL_TIME_1000;
	case COM_GAME_STVEF_MP:
		return FRAME_RETAIL | FRAME_FIXED_TICK | FRAME_X87_PRODUCT | FRAME_ALWAYS_FLOOR | FRAME_KEEP_LEVEL_TIME;
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
