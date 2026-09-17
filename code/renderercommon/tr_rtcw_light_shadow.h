/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_LIGHT_SHADOW_H
#define FNTECH3_RTCW_LIGHT_SHADOW_H

#include "tr_dlight_shadow.h"

/* A stationary lightstyle can cross a power-of-two radius boundary every
 * flicker. Keep its largest observed projection while it remains in the scene;
 * only the depth projection is stabilized, never the visible light radius.
 * Exact origins avoid attaching a moving effect to a nearby torch. */
typedef struct {
	vec3_t origin;
	float projectionFar;
	int frame;
} rtcwLightShadowHistory_t;

static ID_INLINE float R_RTCWLightShadowProjection( rtcwLightShadowHistory_t *history,
	int count, const vec3_t origin, float radius, int frame ) {
	const float projection = R_DlightShadowProjectionFarForRadius( radius, 1.0f );
	int i, oldest = 0;
	if ( projection <= 0.0f || !history || count <= 0 || !origin ) return projection;
	for ( i = 0; i < 3; ++i ) if ( !R_DlightShadowFloatIsFinite( origin[i] ) ) return 0;
	for ( i = 0; i < count; ++i ) {
		const int64_t age = (int64_t)frame - history[i].frame;
		if ( history[i].projectionFar > 0 && age >= 0 && age <= 120 &&
			VectorCompare( history[i].origin, origin ) ) {
			history[i].projectionFar = MAX( history[i].projectionFar, projection );
			history[i].frame = frame;
			return history[i].projectionFar;
		}
		if ( history[i].frame < history[oldest].frame ) oldest = i;
		if ( history[i].projectionFar <= 0 || age < 0 || age > 120 ) oldest = i;
	}
	VectorCopy( origin, history[oldest].origin );
	history[oldest].projectionFar = projection;
	history[oldest].frame = frame;
	return projection;
}
#endif
