/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_PARTICLES_H
#define FNTECH3_TR_Q3_IHV_PARTICLES_H

#include <float.h>
#include <stdint.h>
#include <string.h>

/* Engine-owned particle input; no native module addresses cross this API. */
typedef struct {
	float origin[3];
	unsigned char rgba[4];
} fnQ3IHVParticle_t;

typedef struct {
	float origin[3], forward[3], up[3], right[3];
} fnQ3IHVParticleView_t;

static inline int Q3IHV_ParticleFinite( const float *value ) {
	uint32_t bits;
	memcpy( &bits, value, sizeof( bits ) );
	return ( bits & UINT32_C(0x7f800000) ) != UINT32_C(0x7f800000);
}

/* Original 0x4428a0 scales the two view axes before evaluating each point.
 * Explicit stored intermediates preserve its x87 precision-53 operations
 * even when the renderer enables reassociation or fused arithmetic. */
static inline int Q3IHV_ParticleView( const float origin[3], const float axis[3][3],
	int mirror, fnQ3IHVParticleView_t *view ) {
	fnQ3IHVParticleView_t next;
	int i, j;
	if ( !origin || !axis || !view ) return 0;
	for ( i = 0; i < 3; ++i ) {
		volatile float up, right;
		if ( !Q3IHV_ParticleFinite( origin + i ) ) return 0;
		for ( j = 0; j < 3; ++j ) if ( !Q3IHV_ParticleFinite( &axis[j][i] ) ) return 0;
		if ( axis[1][i] > FLT_MAX / 1.5 || axis[1][i] < -FLT_MAX / 1.5 ||
			axis[2][i] > FLT_MAX / 1.5 || axis[2][i] < -FLT_MAX / 1.5 ) return 0;
		up = (float)( (double)axis[2][i] * 1.5 );
		right = (float)( (double)axis[1][i] * ( mirror ? 1.5 : -1.5 ) );
		next.origin[i] = origin[i]; next.forward[i] = axis[0][i];
		next.up[i] = up; next.right[i] = right;
	}
	*view = next;
	return 1;
}

/* One triangle per point: depth < 20 uses scale 1, otherwise 1 + depth*.004.
 * Preserve z,x,y accumulation, the threshold discontinuity and mirror sign.
 * Invalid/extreme points produce no partial triangle. View comes from above. */
static inline int Q3IHV_ParticleTriangle( const fnQ3IHVParticleView_t *view,
	const float origin[3], float xyz[3][3] ) {
	float next[3][3];
	volatile double delta[3], projected[3], depth, scale;
	int i;
	if ( !view || !origin || !xyz ) return 0;
	for ( i = 0; i < 3; ++i ) {
		if ( !Q3IHV_ParticleFinite( origin + i ) ) return 0;
		delta[i] = (double)origin[i] - view->origin[i];
		projected[i] = delta[i] * view->forward[i];
	}
	depth = projected[2] + projected[0];
	depth = depth + projected[1];
	scale = 1;
	if ( depth >= 20 ) { scale = depth * .004; scale = 1 + scale; }
	for ( i = 0; i < 3; ++i ) {
		volatile double up = (double)view->up[i] * scale;
		volatile double right = (double)view->right[i] * scale;
		up = (double)origin[i] + up; right = (double)origin[i] + right;
		if ( up > FLT_MAX || up < -FLT_MAX || right > FLT_MAX || right < -FLT_MAX ) return 0;
		next[0][i] = origin[i]; next[1][i] = (float)up; next[2][i] = (float)right;
	}
	memcpy( xyz, next, sizeof( next ) );
	return 1;
}

/* Original built-in *particle image (0x439650): a 16x16 binary RGBA disc,
 * centered at (5,5), radius squared 20. All four channels have the same byte. */
static inline void Q3IHV_ParticleImage( unsigned char rgba[16 * 16 * 4] ) {
	int x, y;
	for ( y = 0; y < 16; ++y ) for ( x = 0; x < 16; ++x ) {
		const unsigned char value = ( x - 5 ) * ( x - 5 ) + ( y - 5 ) * ( y - 5 ) <= 20 ? 255 : 0;
		memset( rgba + 4 * ( y * 16 + x ), value, 4 );
	}
}

#endif
