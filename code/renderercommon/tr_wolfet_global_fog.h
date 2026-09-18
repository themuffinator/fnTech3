/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_GLOBAL_FOG_H
#define FNTECH3_TR_WOLFET_GLOBAL_FOG_H

/* Enemy Territory compiles one fog volume with no brush of its own, covering
 * the whole world, and lets a module retune its colour and its depth at run
 * time with an optional fade. The map marks it by writing a brush number of
 * -1, which the Quake lineage's fog loader reads as a brush index and refuses
 * as out of range.
 *
 * Only the arithmetic and the state live here, so they can be checked without
 * a renderer: what a request turns into, and what the fade is showing at a
 * given moment. Writing that into a fog volume is the renderer's business. */

typedef struct {
	/* The fog volume ET's global fog occupies, 0 when the map compiled none. */
	int index;
	/* Colour and depth for opaque, as the map authored them. */
	float original[4];
	float from[4], to[4];
	/* An end time of zero means no fade is running. */
	int startTime, endTime;
} fnWolfETGlobalFog_t;

static inline void FnWolfET_GlobalFogClear( fnWolfETGlobalFog_t *state ) {
	if ( state ) memset( state, 0, sizeof( *state ) );
}

/* Records what the map authored, so a later request can restore it. */
static inline void FnWolfET_GlobalFogAuthored( fnWolfETGlobalFog_t *state, int index,
	const float rgb[3], float depthForOpaque ) {
	int i;
	if ( !state || index <= 0 || !rgb ) return;
	FnWolfET_GlobalFogClear( state );
	state->index = index;
	for ( i = 0; i < 3; ++i ) state->original[i] = rgb[i];
	state->original[3] = depthForOpaque;
}

/* Turns a module's request into what the fog volume should show now. A depth
 * for opaque never goes below one, as ET's does not: the texture coordinate
 * scale is its reciprocal. A restore ignores the colour it is given and heads
 * back to what the map authored. Returns 0 without disturbing anything when
 * the map has no global fog or the request cannot be evaluated. */
static inline int FnWolfET_GlobalFogSet( fnWolfETGlobalFog_t *state, const float current[4],
	int restore, int duration, int time, const float rgb[3], float depthForOpaque,
	float destination[4] ) {
	float target[4];
	int i;
	if ( !state || state->index <= 0 || !current || !destination ) return 0;
	if ( restore ) {
		for ( i = 0; i < 4; ++i ) target[i] = state->original[i];
	} else {
		if ( !rgb ) return 0;
		for ( i = 0; i < 3; ++i ) target[i] = rgb[i];
		target[3] = depthForOpaque < 1 ? 1 : depthForOpaque;
	}
	for ( i = 0; i < 4; ++i )
		if ( !isfinite( target[i] ) || !isfinite( current[i] ) ) return 0;
	if ( duration > 0 ) {
		/* A duration that runs the end time past the counter's range would
		 * leave a fade that never finishes. */
		if ( time > INT_MAX - duration ) return 0;
		for ( i = 0; i < 4; ++i ) { state->from[i] = current[i]; state->to[i] = target[i]; }
		state->startTime = time;
		state->endTime = time + duration;
		/* The fade starts where the fog already is, so this frame shows no
		 * jump; the next frame reads the fade. */
		for ( i = 0; i < 4; ++i ) destination[i] = current[i];
		return 1;
	}
	state->endTime = 0;
	for ( i = 0; i < 4; ++i ) destination[i] = target[i];
	return 1;
}

/* What a running fade shows at this moment. Returns 0 when none is running.
 * The last step is the end value exactly, and the fade then stops. */
static inline int FnWolfET_GlobalFogFade( fnWolfETGlobalFog_t *state, int time,
	float destination[4] ) {
	int i;
	if ( !state || state->index <= 0 || !state->endTime || !destination ) return 0;
	if ( state->endTime >= time ) {
		const int span = state->endTime - state->startTime;
		float fraction;
		if ( span <= 0 ) { state->endTime = 0; return 0; }
		fraction = (float)( time - state->startTime ) / (float)span;
		if ( fraction < 0 ) fraction = 0;
		if ( fraction > 1 ) fraction = 1;
		for ( i = 0; i < 4; ++i )
			destination[i] = state->from[i] + ( state->to[i] - state->from[i] ) * fraction;
		return 1;
	}
	for ( i = 0; i < 4; ++i ) destination[i] = state->to[i];
	state->endTime = 0;
	return 1;
}

#endif
