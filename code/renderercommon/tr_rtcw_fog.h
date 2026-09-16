/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_H
#define FNTECH3_TR_RTCW_FOG_H

#include "../qcommon/q_shared.h"
#include "tr_types.h"
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

/* Engine-owned state, independent of the frozen module fog records. Slot
 * meanings and transition behavior are observations of original SP/MP and
 * iortcw; no reference implementation text is incorporated here. Consumers
 * require IEEE arithmetic, including float argument passing and overflow.
 * The legacy build's RENDERER_FP_CFLAGS preserves this for tr_scene.c. */
enum {
	FN_RTCW_FOG_NONE = 0, FN_RTCW_FOG_SKY = 1,
	FN_RTCW_FOG_PORTAL = 2, FN_RTCW_FOG_HUD = 3,
	FN_RTCW_FOG_MAP = 4, FN_RTCW_FOG_WATER = 5,
	FN_RTCW_FOG_SERVER = 6, FN_RTCW_FOG_CURRENT = 7,
	FN_RTCW_FOG_LAST = 8, FN_RTCW_FOG_TARGET = 9,
	FN_RTCW_FOG_SLOTS = 10, FN_RTCW_FOG_SWITCH = 10,
	FN_RTCW_FOG_EXP = 0x0800, FN_RTCW_FOG_LINEAR = 0x2601,
	FN_RTCW_FOG_HINT = 0x1100
};

typedef struct {
	rtcwRenderFog_t slots[FN_RTCW_FOG_SLOTS];
	int selected;
} fnRTCWFogState_t;

static inline void FnRTCWFogReset( fnRTCWFogState_t *state ) {
	if ( state ) memset( state, 0, sizeof( *state ) );
}

static inline qboolean FnRTCWFogFinite( float value ) {
	uint32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	return ( bits & UINT32_C(0x7f800000) ) != UINT32_C(0x7f800000) ? qtrue : qfalse;
}

/* A rejected command leaves every slot unchanged. Unregister and switch
 * commands deliberately ignore the unused color/density arguments. */
static inline qboolean FnRTCWFogSet( fnRTCWFogState_t *state, qboolean multiplayer, int command,
	int first, int second, float red, float green, float blue, float density, int now ) {
	rtcwRenderFog_t *fog;
	if ( !state || command < 0 || command > FN_RTCW_FOG_SWITCH ) return qfalse;
	if ( command == FN_RTCW_FOG_SWITCH ) {
		int64_t finish;
		if ( first < 0 || first >= FN_RTCW_FOG_SLOTS ) return qfalse;
		/* SP uses the map selection as its explicit fog-off command. */
		if ( !multiplayer && first == FN_RTCW_FOG_MAP ) {
			if ( state->slots[FN_RTCW_FOG_CURRENT].registered )
				state->slots[FN_RTCW_FOG_LAST] = state->slots[FN_RTCW_FOG_CURRENT];
			memset( &state->slots[FN_RTCW_FOG_MAP], 0, sizeof( *fog ) );
			memset( &state->slots[FN_RTCW_FOG_TARGET], 0, sizeof( *fog ) );
			state->selected = FN_RTCW_FOG_NONE;
			return qtrue;
		}
		if ( !state->slots[first].registered ) return qtrue;
		finish = (int64_t)now + second;
		if ( finish < INT_MIN || finish > INT_MAX ) return qfalse;
		state->selected = first;
		state->slots[FN_RTCW_FOG_LAST] = state->slots[
			state->slots[FN_RTCW_FOG_CURRENT].registered ? FN_RTCW_FOG_CURRENT :
			( multiplayer ? FN_RTCW_FOG_MAP : first )];
		state->slots[FN_RTCW_FOG_TARGET] = state->slots[first];
		fog = &state->slots[FN_RTCW_FOG_TARGET];
		fog->startTime = now;
		fog->finishTime = (int)finish;
		if ( !multiplayer && second == 0 ) {
			fog->startTime = fog->finishTime = 0;
			fog->dirty = 1;
			state->slots[FN_RTCW_FOG_CURRENT].dirty = 1;
		}
		return qtrue;
	}

	fog = &state->slots[command];
	if ( first == 0 && second == 0 ) {
		fog->registered = qfalse;
		return qtrue;
	}
	if ( !FnRTCWFogFinite( red ) || !FnRTCWFogFinite( green ) ||
		!FnRTCWFogFinite( blue ) || !FnRTCWFogFinite( density ) || density < 0 ) return qfalse;
	fog->color[0] = red; fog->color[1] = green; fog->color[2] = blue; fog->color[3] = 1;
	fog->start = (float)first; fog->end = (float)second;
	fog->mode = ( multiplayer ? density > 1 : density >= 1 ) ? FN_RTCW_FOG_LINEAR : FN_RTCW_FOG_EXP;
	fog->density = fog->mode == FN_RTCW_FOG_LINEAR ? 1 : density;
	fog->drawsky = fog->mode == FN_RTCW_FOG_LINEAR ? qfalse : qtrue;
	fog->clearscreen = fog->mode == FN_RTCW_FOG_LINEAR ? qtrue : qfalse;
	fog->hint = FN_RTCW_FOG_HINT;
	fog->registered = qtrue;
	return qtrue;
}

static inline float FnRTCWFogMix( float from, float to, float fraction ) {
	const float difference = to - from;
	if ( FnRTCWFogFinite( difference ) ) return from + difference * fraction;
	/* Finite out-of-range colors can have an unrepresentable float difference.
	 * Interpolate them without producing infinity; the GPU clamps the color. */
	return (float)( (double)from + ( (double)to - from ) * fraction );
}

/* Called for a world view, using that view's simulation time. The caller owns
 * the resulting snapshot so later commands cannot mutate a queued view.
 * MP leaves its current record alone when no target is registered. SP still
 * advances/copies that record. MP zero-duration fades retain the source on
 * their exact starting timestamp; SP marks an immediate switch dirty. */
static inline float FnRTCWFogAdvance( fnRTCWFogState_t *state, int now,
	qboolean multiplayer, qboolean snooperView, float farClip ) {
	rtcwRenderFog_t *current, *target;
	const rtcwRenderFog_t *last;
	qboolean fading, differentModes;
	if ( !state ) return farClip;
	current = &state->slots[FN_RTCW_FOG_CURRENT];
	target = &state->slots[FN_RTCW_FOG_TARGET];
	last = &state->slots[FN_RTCW_FOG_LAST];
	if ( multiplayer && !target->registered ) return farClip;
	fading = target->finishTime != 0 && target->finishTime >= now ? qtrue : qfalse;
	differentModes = ( last->mode == FN_RTCW_FOG_EXP && target->mode == FN_RTCW_FOG_LINEAR ) ||
		( last->mode == FN_RTCW_FOG_LINEAR && target->mode == FN_RTCW_FOG_EXP ) ? qtrue : qfalse;
	if ( !fading || differentModes ) {
		*current = *target;
		if ( fading && differentModes ) target->finishTime = 0;
	} else {
		const int64_t elapsed = (int64_t)now - target->startTime;
		const int64_t duration = (int64_t)target->finishTime - target->startTime;
		float fraction = (float)elapsed / (float)( duration > 0 ? duration : 1 );
		int channel;
		/* Clock rewinds freeze at the source instead of extrapolating invalid
		 * colors/distances. All timestamp arithmetic is wider than the ABI. */
		if ( fraction < 0 ) fraction = 0;
		if ( fraction > 1 ) fraction = 1;
		current->start = FnRTCWFogMix( last->start, target->start, fraction );
		current->end = FnRTCWFogMix( last->end, target->end, fraction );
		for ( channel = 0; channel < 3; ++channel )
			current->color[channel] = FnRTCWFogMix( last->color[channel], target->color[channel], fraction );
		current->mode = target->mode;
		current->density = target->density;
		current->registered = qtrue;
		current->clearscreen = last->clearscreen || target->clearscreen ? qtrue : qfalse;
	}
	if ( !multiplayer ) current->dirty = fading ? 1 : 0;
	if ( current->mode == FN_RTCW_FOG_LINEAR ) {
		if ( current->end < farClip ) farClip = current->end;
		if ( !multiplayer && snooperView ) farClip += 1000;
	}
	return farClip;
}

#endif
