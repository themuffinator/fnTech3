/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_FOG_H
#define FNTECH3_TR_Q3_IHV_FOG_H
#include <math.h>
#include <float.h>

/* Engine-internal parameters; not the IHV DLL's fog POD. The original binary
 * observations and executable probe are recorded in docs/fntech3/Q3_IHV.md. */
typedef struct {
	int enabled, gradient, wave;
	float base, amplitude, phase, frequency;
} q3IHVFogParms_t;

static inline float Q3IHV_FogDepth( const q3IHVFogParms_t *fog, float authored, int time ) {
	double depth = authored;
	if ( fog->wave ) {
		double ticks = ( (double)time * 0.001 * fog->frequency + fog->phase ) * 1024.0;
		int index = (int)fmod( trunc( ticks ), 1024.0 );
		float sample;
		if ( index < 0 ) index += 1024;
		if ( fog->wave == 1 ) {
			/* IHV uses 1023 in its sine table, without the later epsilon. */
			sample = (float)sin( index * (double)( 360.0f / 1023.0f ) *
				3.14159265358979323846 / 180.0 );
		} else if ( fog->wave == 2 ) {
			sample = index < 512 ? 1.0f : -1.0f;
		} else {
			int half = index & 511;
			sample = half < 256 ? half / 256.0f : ( 512 - half ) / 256.0f;
			if ( index >= 512 ) sample = -sample;
		}
		depth = fog->base + (double)fog->amplitude * sample;
	}
	return depth < 1.0 ? 1.0f : depth > FLT_MAX ? FLT_MAX : (float)depth;
}

/* Transform the original Z-density plane into the entity's local space.
 * IHV's zero gradient leaves a zero plane; it does not mean retail Q3 fog. */
static inline void Q3IHV_FogPlane( int gradient, float depth, float minZ, float maxZ,
	const float *origin, const float axis[3][3], double plane[4] ) {
	const float sign = gradient > 0 ? -1.0f : gradient < 0 ? 1.0f : 0.0f;
	const float distance = gradient > 0 ? -maxZ : gradient < 0 ? minZ : 0.0f;
	const double d = depth < 1.0f ? 1.0 : depth;
	const double scale = gradient ? 2.0 / ( d * d ) : 1.0 / d;
	int i;
	for ( i = 0; i < 3; ++i ) plane[i] = (double)sign * axis[i][2] * scale;
	plane[3] = ( (double)distance - (double)sign * origin[2] ) * scale;
}

/* Original 0x4448f0: integrate the linear density along the visible ray.
 * A vertex outside the plane has zero density even when the eye is inside.
 * These are per-vertex factors, not the later sqrt fog texture coordinates. */
static inline float Q3IHV_FogFactor( const double plane[4], const float eye[3], const float vertex[3] ) {
	double eyeDepth = -plane[3], vertexDepth = -plane[3], lengthSquared = 0, factor;
	int i;
	for ( i = 0; i < 3; ++i ) {
		const double delta = (double)eye[i] - vertex[i];
		eyeDepth += (double)plane[i] * eye[i];
		vertexDepth += (double)plane[i] * vertex[i];
		lengthSquared += delta * delta;
	}
	if ( vertexDepth < 0 ) return 0;
	if ( eyeDepth < 0 ) {
		factor = 0.5 * sqrt( lengthSquared ) * vertexDepth *
			( vertexDepth / ( vertexDepth - eyeDepth ) );
	} else {
		factor = 0.5 * sqrt( lengthSquared ) * ( vertexDepth + eyeDepth );
	}
	return factor > 1.0 ? 1.0f : (float)factor;
}
#endif
