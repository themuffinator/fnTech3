/* fnTech3 RT scene change quantization (GPL-2.0-or-later). */
#ifndef RTX_SCENE_SIGNATURE_H
#define RTX_SCENE_SIGNATURE_H

#include <stdint.h>
#include <float.h>
#include <math.h>

static uint32_t RTX_QuantizeSceneFloat( float value, float step )
{
	double scaled;
	/* Never pass NaN, infinity, or an out-of-range float to an integer cast.
	 * Fixed exceptional values keep malformed input deterministic. */
	if ( value != value ) return UINT32_C( 0x7fc00000 );
	if ( value > FLT_MAX ) return UINT32_C( 0x7f800000 );
	if ( value < -FLT_MAX ) return UINT32_C( 0xff800000 );
	if ( !( step > 0.0f && step <= FLT_MAX ) ) step = 1.0f;
	scaled = (double)value / (double)step;
	if ( scaled >= 2147483647.0 ) return UINT32_C( 0x7fffffff );
	if ( scaled <= -2147483648.0 ) return UINT32_C( 0x80000000 );
	scaled = scaled >= 0.0 ? floor( scaled + 0.5 ) : ceil( scaled - 0.5 );
	return (uint32_t)(int32_t)scaled;
}

#endif
