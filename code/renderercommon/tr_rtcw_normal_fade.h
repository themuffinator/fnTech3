/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_NORMAL_FADE_H
#define FNTECH3_RTCW_NORMAL_FADE_H

#include <math.h>

/* RTCW's fire shell uses a triangular alpha envelope along fireRiseDir.
 * Either -1000 bound selects the separate entity-alpha zombie effect.
 * Evaluate in model coordinates without mutating the submitted entity.
 */
static byte R_RTCWNormalZAlpha( const refEntity_t *entity, const float *normal,
	const float bounds[2], byte peak ) {
	vec3_t direction, localDirection;
	float projection, alpha;
	int i;
	VectorCopy( entity->rtcw.fireRiseDir, direction );
	if ( direction[0] == 0 && direction[1] == 0 && direction[2] == 0 ) direction[2] = 1;
	for ( i = 0; i < 3; ++i )
		localDirection[i] = entity->hModel ? DotProduct( direction, entity->axis[i] ) : direction[i];
	projection = DotProduct( normal, localDirection );
	if ( !isfinite( projection ) ) return 0;
	if ( bounds[0] == -1000 || bounds[1] == -1000 ) {
		const double weight = ( projection + 1.0 ) * 0.5;
		const double opacity = entity->shader.rgba[3];
		/* Keep the intermediate float rounding of the two weighted terms. */
		alpha = (float)( opacity * weight );
		alpha = (float)( alpha + 2.0 * opacity * ( 1.0 - weight ) );
	} else {
		const float width = bounds[1] - bounds[0];
		const float halfWidth = width * 0.5f;
		float offset;
		if ( !( width > 0 ) || !isfinite( width ) || projection <= bounds[0] || projection >= bounds[1] ) return 0;
		if ( !( halfWidth > 0 ) ) return 0;
		offset = projection - bounds[0];
		/* Preserve the two half-intervals and double-precision attenuation:
		 * folding these into one float phase changes truncated alpha bytes.
		 */
		if ( projection < bounds[0] + halfWidth ) alpha = peak * ( offset / halfWidth );
		else alpha = (float)( peak * ( 1.0 - ( offset - halfWidth ) / halfWidth ) );
		alpha = alpha <= 0 ? 0 : alpha >= 255 ? 255 : alpha;
		if ( entity->hModel ) alpha = (float)( alpha * ( entity->shader.rgba[3] / 255.0 ) );
	}
	return alpha <= 0 ? 0 : alpha >= 255 ? 255 : (byte)alpha;
}
#endif
