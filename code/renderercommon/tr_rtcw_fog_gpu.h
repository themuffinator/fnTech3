/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_GPU_H
#define FNTECH3_TR_RTCW_FOG_GPU_H
#include "tr_rtcw_fog_view.h"

/* One material pass, after the view's immutable slot selection. RGB follows
 * glFog's clamping; alpha is never fogged. color.w selects off/linear/exp.
 * params carries start/end/density and radial (1) or eye-plane (0) distance.
 * No module pointer or mutable controller slot reaches a GPU draw. */
typedef struct {
	vec4_t color;
	vec4_t params;
} fnRTCWFogDraw_t;

static inline fnRTCWFogDraw_t FnRTCWFogDraw( const fnRTCWFogFrame_t *frame,
	qboolean sky, qboolean projection2D, qboolean noFog, qboolean stageFog,
	qboolean snooper, qboolean radial ) {
	fnRTCWFogDraw_t draw;
	const rtcwRenderFog_t *fog;
	int channel;
	memset( &draw, 0, sizeof( draw ) );
	if ( !frame || projection2D || ( noFog && !stageFog ) ||
		!( sky ? frame->skyEnabled : frame->fogEnabled ) ) return draw;
	fog = sky ? &frame->skyFog : &frame->fog;
	if ( !fog->registered ) return draw;
	draw.color[3] = fog->mode == FN_RTCW_FOG_EXP ? 2.0f : 1.0f;
	for ( channel = 0; channel < 3; ++channel ) {
		const float value = fog->color[channel];
		draw.color[channel] = value < 0 ? 0 : value > 1 ? 1 : value;
	}
	draw.params[0] = snooper ? fog->end : fog->start;
	draw.params[1] = fog->end + ( snooper ? 1000.0f : 0.0f );
	/* Original R_Fog supplies density one when a registered record stores 0. */
	draw.params[2] = fog->density == 0 ? 1 : fog->density;
	draw.params[3] = radial ? 1.0f : 0.0f;
	return draw;
}

/* The same equations as the draw shaders, useful for boundary probes. Linear
 * zero-span fog has a defined step instead of a division producing NaN. */
static inline float FnRTCWFogVisibility( const fnRTCWFogDraw_t *draw, float distance ) {
	float visible;
	if ( !draw || draw->color[3] == 0 ) return 1;
	if ( draw->color[3] == 2 ) return expf( -draw->params[2] * fabsf( distance ) );
	if ( draw->params[1] == draw->params[0] ) return distance < draw->params[1] ? 1 : 0;
	visible = ( draw->params[1] - distance ) / ( draw->params[1] - draw->params[0] );
	return visible < 0 ? 0 : visible > 1 ? 1 : visible;
}
#endif
