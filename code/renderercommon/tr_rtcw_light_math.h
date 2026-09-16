/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Independently authored from original RTCW SP/MP light-contract observations. */
#ifndef FNTECH3_RTCW_LIGHT_MATH_H
#define FNTECH3_RTCW_LIGHT_MATH_H
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	float radius;
	int overdraw;
	int material; /* 0: ordinary, 1: negative, 2: flame */
} rtcwLightParameters_t;

static int RTCW_LightFinite( float value ) {
	uint32_t bits;
	/* Inspect the external IEEE-754 representation. Callers also require strict
	 * floating-point arithmetic: fast math can remove surrounding guards. */
	memcpy( &bits, &value, sizeof( bits ) );
	return ( bits & UINT32_C(0x7f800000) ) != UINT32_C(0x7f800000);
}

/* -1: invalid, 0: disabled, 1: accepted. Publish only complete parameters.
 * The references strip JUNIOR without using it, and their mode-2 test reads
 * an old slot's forced flag. Use the submitted FORCE bit deterministically. */
static int RTCW_LightParameters( int singlePlayer, int dynamicMode, float scale,
	const float origin[3], float intensity, const float color[3], uint32_t flags,
	rtcwLightParameters_t *output ) {
	rtcwLightParameters_t staged;
	unsigned overdraw = flags & UINT32_C(0x3fffffff);
	int axis;
	if ( !origin || !color || !output || !RTCW_LightFinite( intensity ) ) return -1;
	for ( axis = 0; axis < 3; ++axis )
		if ( !RTCW_LightFinite( origin[axis] ) || !RTCW_LightFinite( color[axis] ) ) return -1;
	if ( intensity <= 0.0f ) return 0;
	if ( !( flags & UINT32_C(0x80000000) ) && ( dynamicMode == 0 || dynamicMode == 2 ) ) return 0;
	if ( singlePlayer && !RTCW_LightFinite( scale ) ) return -1;
	if ( singlePlayer && scale <= 0.0f ) return 0;
	staged.radius = singlePlayer ? intensity * scale : intensity;
	/* Keep inverse radius and entity-light power representable. */
	if ( !RTCW_LightFinite( staged.radius ) || staged.radius < FLT_MIN ||
		(double)staged.radius * staged.radius > (double)FLT_MAX / 16.0 ) return -1;
	/* Bound work requested by malformed modules rather than scheduling billions
	 * of repeated GPU passes. Stock effects use zero, one or two overdraws. */
	if ( overdraw > 64 ) return -1;
	staged.material = overdraw == 10 ? 1 : overdraw == 11 ? 2 : 0;
	staged.overdraw = staged.material ? 0 : (int)overdraw;
	*output = staged;
	return 1;
}

typedef struct {
	float st[2];
	unsigned char color[4];
	unsigned char clip;
} rtcwLightVertex_t;

static unsigned char RTCW_LightByte( float value ) {
	/* Preserve reference conversion/wrapping for representable values without
	 * undefined float-to-int conversion on extreme inputs. */
	if ( !RTCW_LightFinite( value ) || (double)value < INT_MIN || (double)value > INT_MAX )
		return value > 0.0f ? 255 : 0;
#if defined( _WIN32 ) && ( defined( _M_IX86 ) || defined( __i386__ ) )
	/* Original Win32 uses x87 FISTP (the current rounding mode); its portable
	 * C path and iortcw's later conversion service truncate instead. */
	return (unsigned char)lrintf( value );
#else
	return (unsigned char)(int)value;
#endif
}

static rtcwLightVertex_t RTCW_ProjectLightVertex( const float origin[3], float radius,
	const float color[3], const float point[3] ) {
	rtcwLightVertex_t result = { { 0, 0 }, { 0, 0, 0, 255 }, 255 };
	float dist[3], inverse, strength;
	int axis;
	if ( !origin || !color || !point || !RTCW_LightFinite( radius ) || radius < FLT_MIN ) return result;
	for ( axis = 0; axis < 3; ++axis ) {
		dist[axis] = origin[axis] - point[axis];
		if ( !RTCW_LightFinite( dist[axis] ) ) return result;
	}
	inverse = 1.0f / radius;
	result.st[0] = 0.5f + dist[0] * inverse;
	result.st[1] = 0.5f + dist[1] * inverse;
	if ( !RTCW_LightFinite( result.st[0] ) || !RTCW_LightFinite( result.st[1] ) ) {
		result.st[0] = result.st[1] = 0.0f;
		return result;
	}
	result.clip = 0;
	for ( axis = 0; axis < 2; ++axis ) {
		if ( result.st[axis] < 0.0f ) result.clip |= 1u << (axis * 2);
		if ( result.st[axis] > 1.0f ) result.clip |= 2u << (axis * 2);
	}
	if ( dist[2] > radius || dist[2] < -radius ) {
		result.clip |= dist[2] > radius ? 16 : 32;
		strength = 0.0f;
	} else {
		const float height = fabsf( dist[2] );
		strength = height < radius * 0.5f ? 1.0f : 2.0f * (radius - height) * inverse;
	}
	for ( axis = 0; axis < 3; ++axis )
		result.color[axis] = RTCW_LightByte( (color[axis] * 255.0f) * strength );
	return result;
}
#endif
