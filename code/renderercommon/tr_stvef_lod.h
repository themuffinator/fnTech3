/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_LOD_H
#define FNTECH3_TR_STVEF_LOD_H
#include <math.h>
#include <stdint.h>

/* R_ComputeLOD uses nearest-even conversion (SP 0x461d60/0x465d10,
 * Holomatch 0x477ff0/0x47cf20). The caller caps Holomatch's scale at 20;
 * SP has no such cap. Bound before integer casts
 * so invalid/extreme cvars cannot cause undefined conversions or overflow. */
static inline int FnSTVEF_ComputeLOD( int count, float projectedRadius, float scale, int bias ) {
	float value;
	int lod;
	int64_t biased;
	if ( count < 2 ) return 0;
	value = projectedRadius == 0.0f ? 0.0f : count * ( 1.0f - projectedRadius * scale );
	if ( !isfinite( value ) ) lod = 0;
	else if ( value <= 0.0f ) lod = 0;
	else if ( (double)value >= (double)count - 1.0 ) lod = count - 1;
	else {
		const float integral = floorf( value );
		const float fraction = value - integral;
		lod = (int)integral;
		if ( fraction > 0.5f || ( fraction == 0.5f && ( lod & 1 ) ) ) ++lod;
	}
	biased = (int64_t)lod + bias;
	return biased < 0 ? 0 : biased >= count ? count - 1 : (int)biased;
}
#endif
