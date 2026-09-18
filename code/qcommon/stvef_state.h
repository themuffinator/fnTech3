/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_INTERNAL_STATE_H
#define FNTECH3_STVEF_INTERNAL_STATE_H

/* Engine-owned Elite Force extensions, included by q_shared.h after trace_t.
 * These are not module or wire records: the frozen SP and Holomatch
 * declarations in compat/stvef_game_abi.h and their adapters own external
 * layout. Fields Quake III already carries stay in the canonical state;
 * Holomatch's third and fourth player events extend its two event slots. */
typedef struct {
	int rechargeTime, useTime, introTime, damageShieldCount;
	int events[2], eventParms[2];
	/* Single player only. */
	int leanofs, friction, scale, leanStopDebounceTime;
	int borgAdaptHits[32];
	float pushVec[3];
} stvefPlayerStateExtension_t;

/* Single player's extra entity fields. */
typedef struct {
	int modelindex3, legsAnimTimer, torsoAnimTimer, scale;
	float pushVec[3];
} stvefEntityStateExtension_t;

#endif
