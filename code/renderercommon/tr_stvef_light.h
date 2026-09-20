/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_LIGHT_H
#define FNTECH3_TR_STVEF_LIGHT_H
#include <math.h>

/* Retail uses nearest-even for ambient packing and diffuse vertex colours
 * (SP 0x45ba87/0x465f00, Holomatch 0x47a201). Define it independently of
 * the host FPU mode, and keep malformed lighting finite and within a byte. */
static inline int FnSTVEF_LightByte( float value ) {
	float integral, fraction;
	int result;
	if ( !isfinite( value ) || value <= 0.0f ) return 0;
	if ( value >= 255.0f ) return 255;
	integral = floorf( value ); fraction = value - integral;
	result = (int)integral;
	if ( fraction > 0.5f || ( fraction == 0.5f && ( result & 1 ) ) ) ++result;
	return result;
}
#endif
