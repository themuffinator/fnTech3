/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_RANGED_FOG_H
#define FNTECH3_JKA_RANGED_FOG_H
#include "../qcommon/q_shared.h"
#include <math.h>

/* Jedi Academy's zoom fog (cgame import 216, R_SetRangedFog). Retail jamp.exe
 * 1.01 stores the float word at 0x100d830 (dispatch 0x412ed2) and nothing
 * else; its fog stage (0x4aed70) reads it for the world-covering volume only.
 * Zero keeps the volume's own fog. Otherwise the fog becomes a linear ramp
 * that ends at the world's distanceCull (0x100d834) and starts at
 *   -range                         when the range is negative (a designer's
 *                                  start distance, cg_linearFogOverride),
 *   depthForOpaque                 when the ramp is already at least range
 *                                  long,
 *   max(16, distanceCull - range)  otherwise (0x4aee1d..0x4aee52).
 * Fn draws that ramp with its own fog volume pass rather than GL fog: the
 * volume's distance coordinate is rescaled so the pass reaches full fog at
 * the ramp's end. A ramp that would end before it starts is given one unit,
 * where retail's GL linear fog divides by its zero or negative length. */
static inline qboolean FnJKA_RangedFogSpan( float range, float depthForOpaque, float distanceCull,
	float *start, float *end ) {
	float first;
	if ( !start || !end || range == 0.0f || !isfinite( range ) || !isfinite( depthForOpaque )
		|| !isfinite( distanceCull ) ) return qfalse;
	first = depthForOpaque;
	if ( range < 0.0f ) {
		first = -range;
	} else if ( distanceCull - first < range ) {
		first = distanceCull - range;
		if ( first < 16.0f ) first = 16.0f;
	}
	*start = first;
	*end = distanceCull > first + 1.0f ? distanceCull : first + 1.0f;
	return qtrue;
}

/* The fog pass's distance scale and the distance where it begins: the pass
 * reaches full fog at 1 / ( 8 * scale ) past the start (R_FogFactor). */
static inline qboolean FnJKA_RangedFogScale( float range, float depthForOpaque, float distanceCull,
	float *scale, float *start ) {
	float first, last;
	if ( !scale || !start || !FnJKA_RangedFogSpan( range, depthForOpaque, distanceCull, &first, &last ) )
		return qfalse;
	/* A start too far out for one more unit to register leaves the volume's
	 * own fog in place rather than an infinite scale. */
	if ( !( last - first > 0.0f ) || !isfinite( ( last - first ) * 8.0f ) ) return qfalse;
	*scale = 1.0f / ( ( last - first ) * 8.0f );
	*start = first;
	return qtrue;
}
#endif
