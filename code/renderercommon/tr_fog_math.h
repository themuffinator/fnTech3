/*
===========================================================================
Copyright (C) 2026

This file is part of FnQL.

FnQL is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
===========================================================================
*/

#ifndef TR_FOG_MATH_H
#define TR_FOG_MATH_H

#include <math.h>

#define FOG_DISTANCE_BIAS  ( 1.0f / 512.0f )
#define FOG_DEPTH_MIN      ( 1.0f / 32.0f )
#define FOG_DEPTH_MAX      ( 31.0f / 32.0f )
#define FOG_DEPTH_RANGE    ( 30.0f / 32.0f )
#define FOG_DISTANCE_SCALE 8.0f

static ID_INLINE float R_AnalyticFogFactor( float s, float t )
{
	s -= FOG_DISTANCE_BIAS;
	if ( s <= 0.0f || t < FOG_DEPTH_MIN ) {
		return 0.0f;
	}
	if ( t < FOG_DEPTH_MAX ) {
		s *= ( t - FOG_DEPTH_MIN ) / FOG_DEPTH_RANGE;
	}
	s *= FOG_DISTANCE_SCALE;
	return s >= 1.0f ? 1.0f : sqrtf( s );
}

static ID_INLINE float R_LegacyFogFactor( float s, float t,
	const float *fogTable, int fogTableSize )
{
	s -= FOG_DISTANCE_BIAS;
	if ( s < 0.0f || t < FOG_DEPTH_MIN || !fogTable || fogTableSize < 2 ) {
		return 0.0f;
	}
	if ( t < FOG_DEPTH_MAX ) {
		s *= ( t - FOG_DEPTH_MIN ) / FOG_DEPTH_RANGE;
	}
	s = MIN( s * FOG_DISTANCE_SCALE, 1.0f );
	return fogTable[(int)( s * ( fogTableSize - 1 ) )];
}

/* The fog image of Ritual's engines (fakk2.exe 1.02 0x0046fdbb..0x0046fef1;
 * alice.exe 0x00484b83..0x00484c9a, the same code). It is Quake III's texel
 * rule with the image's width, 256, where Quake III's has its height, 32: a
 * texel nearer than 255/256 of the fog's depth scales the distance by t less
 * 1/256, times 32/254. No row of the 32-row image reaches 255/256, so every
 * texel takes that scale, and a point inside a fog volume (t = 31/32, as
 * Quake III's texture coordinates give it, 0x00494f9a) keeps about an eighth
 * of Quake III's distance: Ritual's fog volumes are that much thinner. The
 * distance scale, 1 / (8 x depth), and the square-root table are Quake III's
 * (0x00485c7b, 0x0046fb50). */
static ID_INLINE float R_RitualFogFactor( float s, float t,
	const float *fogTable, int fogTableSize )
{
	double d = (double)s - 1.0 / 512.0;
	if ( d < 0.0 || (double)t < 1.0 / 32.0 || !fogTable || fogTableSize < 2 ) {
		return 0.0f;
	}
	if ( t < 255.0f / 256.0f ) {
		d *= ( (double)t - 1.0 / 256.0 ) * ( 32.0 / 254.0 );
	}
	d *= FOG_DISTANCE_SCALE;
	if ( d > 1.0 ) {
		d = 1.0;
	}
	return fogTable[(int)( d * ( fogTableSize - 1 ) )];
}

#endif // TR_FOG_MATH_H
