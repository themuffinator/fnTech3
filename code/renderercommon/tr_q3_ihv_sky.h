/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_SKY_H
#define FNTECH3_TR_Q3_IHV_SKY_H
#include <stdint.h>

/* Original 4.01 sky iterator observations: see tests' reference probe and
 * docs/fntech3/Q3_IHV.md. The unsigned arithmetic deliberately wraps at 32 bits. */
static inline float Q3IHV_SkyRandom( uint32_t *seed ) {
	*seed = *seed * UINT32_C(69069) + 1;
	return ( *seed & UINT32_C(65535) ) * ( 1.0f / 65536.0f );
}

/* Call before testing whether the stage is active: the original also advances
 * its random state for the inactive terminator. Stage zero is always white. */
static inline int Q3IHV_SkyStageColor( uint32_t *seed, float *lastFlash,
	int milliseconds, int lightning, int stage ) {
	double now, elapsed;
	float random;
	if ( !lightning || stage <= 0 ) return 255;
	random = Q3IHV_SkyRandom( seed );
	now = (double)milliseconds * (double)0.001f;
	elapsed = ( now - *lastFlash ) + random;
	if ( elapsed <= 3.0 ) return 0;
	random = Q3IHV_SkyRandom( seed );
	if ( (double)(float)elapsed + random > 4.0 ) *lastFlash = (float)now;
	return 255;
}
#endif
