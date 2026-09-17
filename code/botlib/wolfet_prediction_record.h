/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_PREDICTION_RECORD_H
#define FNTECH3_WOLFET_PREDICTION_RECORD_H
#include "botlib.h"

/* ET's stop-event bits agree with Quake's up to 1024 and diverge above it:
 * where Quake has a bounding-box hit and a cluster-portal touch, ET has an
 * entity hit and a stuck report. Never pass one profile's word to the other. */
#define SE_WOLFET_HITENT	2048
#define SE_WOLFET_STUCK		4096

/* ET's prediction result. Its trace is a world trace with a plane, a surface
 * and contents, which Quake's client-box prediction does not produce, and its
 * end contents are a float. Engine storage; marshalled at the boundary. */
struct aas_wolfet_clientmove_t {
	vec3_t endpos;
	vec3_t velocity;
	bsp_trace_t trace;
	int presencetype;
	int stopevent;
	float endcontents;
	float time;
	int frames;
};

/* Predicts up to maxframes of movement. A negative cmdframes walks towards
 * cmdmove as a point rather than treating it as a command. */
int AAS_WolfETPredictClientMovement( aas_wolfet_clientmove_t *move, int entnum,
	vec3_t origin, int hitent, int onground, vec3_t velocity, vec3_t cmdmove,
	int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum );

#endif
