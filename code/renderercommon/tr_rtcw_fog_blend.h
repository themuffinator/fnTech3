/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_BLEND_H
#define FNTECH3_TR_RTCW_FOG_BLEND_H
#include "tr_rtcw_fog_gpu.h"

/* A blended effect is drawn over an already fogged framebuffer. Fade its
 * contribution toward the blend's neutral color, otherwise black additive
 * texels acquire fog RGB and expose the entire sprite polygon. Ordinary
 * opaque/alpha passes still use the view's fog color. Keep alpha and the fog
 * equation unchanged; this also works with fixed-function GL fog. */
static inline void FnRTCWFogBlendColor( fnRTCWFogDraw_t *draw, unsigned stateBits ) {
	const unsigned blend = stateBits & GLS_BLEND_BITS;
	const unsigned destination = blend & GLS_DSTBLEND_BITS;
	float neutral;
	if ( !draw || draw->color[3] == 0 ) return;
	if ( destination == GLS_DSTBLEND_ONE || destination == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) {
		neutral = 0;
	} else if ( blend == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO ) ||
		blend == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR ) ) {
		neutral = 1;
	} else if ( blend == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_SRC_COLOR ) ) {
		neutral = 0.5f;
	} else {
		return;
	}
	draw->color[0] = draw->color[1] = draw->color[2] = neutral;
}
#endif
