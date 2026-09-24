/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_ENTITIES_H
#define FNTECH3_TR_Q3_IHV_ENTITIES_H

#include <float.h>
#include <stdint.h>
#include <string.h>

/* Engine-owned input. Handles belong to the current renderer registration;
 * these are not the pointer words in the native 100-byte module record. */
typedef struct {
	int model, skin, shader;
	float axis[3][3], origin[3], oldorigin[3], backlerp;
	int frame, oldframe, skinNum, nonNormalizedAxes;
	uint32_t flags;
	unsigned char rgba[4];
} fnQ3IHVEntity_t;

/* Only the IHV submission entry point sets active. Other module adapters
 * continue to marshal their frozen records into their own internal fields. */
typedef struct { uint32_t flags; int active; } fnQ3IHVEntityState_t;

#define Q3IHV_ENTITY_THIRD_PERSON UINT32_C(0x2)
#define Q3IHV_ENTITY_FIRST_PERSON UINT32_C(0x4)
#define Q3IHV_ENTITY_DEPTH_HACK UINT32_C(0x10)
#define Q3IHV_ENTITY_ORIGIN_LERP UINT32_C(0x40)
#define Q3IHV_ENTITY_KEEP_TRANSFORM UINT32_C(0x80)
#define Q3IHV_ENTITY_FACE_VIEW UINT32_C(0x400)
#define Q3IHV_ENTITY_PORTAL UINT32_C(0x800)

static inline int Q3IHV_EntityFinite( const float *value ) {
	uint32_t bits;
	memcpy( &bits, value, 4 );
	return ( bits & UINT32_C(0x7f800000) ) != UINT32_C(0x7f800000);
}

/* Original frontend 0x42bac0: hidden bodies do not submit ordinary surfaces
 * in the main view. Portal markers remain in the scene for portal lookup. */
static inline int Q3IHV_EntityVisible( uint32_t flags, int portalView ) {
	return !( flags & Q3IHV_ENTITY_PORTAL ) &&
		!( ( flags & Q3IHV_ENTITY_THIRD_PERSON ) && !portalView ) &&
		!( ( flags & Q3IHV_ENTITY_FIRST_PERSON ) && portalView );
}

/* 0x4423a0 projects oldorigin-origin in z,y,x order, stores float, then
 * multiplies by the interpolation fraction. Do not clamp the fraction.
 * Staged writes and bitwise finite checks are safe under fast-math. */
static inline int Q3IHV_EntityOriginLerp( uint32_t flags, const float origin[3],
	const float oldorigin[3], const float axis[3][3], float fraction, float move[3] ) {
	float next[3] = { 0, 0, 0 };
	volatile double delta[3];
	int i, j;
	if ( !move ) return 0;
	if ( !( flags & Q3IHV_ENTITY_ORIGIN_LERP ) ) { memcpy( move, next, sizeof( next ) ); return 1; }
	if ( !origin || !oldorigin || !axis || !Q3IHV_EntityFinite( &fraction ) ) return 0;
	for ( i = 0; i < 3; ++i ) {
		if ( !Q3IHV_EntityFinite( origin + i ) || !Q3IHV_EntityFinite( oldorigin + i ) ) return 0;
		for ( j = 0; j < 3; ++j ) if ( !Q3IHV_EntityFinite( &axis[i][j] ) ) return 0;
		delta[i] = (double)oldorigin[i] - origin[i];
	}
	for ( i = 0; i < 3; ++i ) {
		volatile double z = delta[2] * axis[i][2], y = delta[1] * axis[i][1], x = delta[0] * axis[i][0];
		volatile double sum = z + y;
		volatile float local;
		sum = sum + x;
		if ( sum > FLT_MAX || sum < -FLT_MAX ) return 0;
		local = (float)sum;
		sum = (double)local * fraction;
		if ( sum > FLT_MAX || sum < -FLT_MAX ) return 0;
		next[i] = (float)sum;
	}
	memcpy( move, next, sizeof( next ) );
	return 1;
}

#endif
