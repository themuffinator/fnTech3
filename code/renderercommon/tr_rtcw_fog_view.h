/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_VIEW_H
#define FNTECH3_TR_RTCW_FOG_VIEW_H
#include "tr_rtcw_fog.h"

/* Copied with a renderer view command. The backend never reads mutable
 * controller slots or retains a pointer into a module's refdef. */
typedef struct {
	rtcwRenderFog_t fog, skyFog;
	float farClip;
	qboolean fogEnabled, skyEnabled, drawSky;
} fnRTCWFogFrame_t;

static inline fnRTCWFogFrame_t FnRTCWFogCapture( fnRTCWFogState_t *state,
	qboolean multiplayer, qboolean seenPortal, const refdef_t *view ) {
	fnRTCWFogFrame_t frame;
	qboolean gate;
	int slot;
	memset( &frame, 0, sizeof( frame ) );
	frame.farClip = FLT_MAX;
	frame.drawSky = qtrue;
	if ( !state || !view || ( view->rdflags & RDF_NOWORLDMODEL ) ) return frame;
	frame.farClip = FnRTCWFogAdvance( state, view->time, multiplayer, view->rtcw.snooperView, FLT_MAX );
	slot = view->rtcw.drawingSky ? FN_RTCW_FOG_SKY :
		( view->rtcw.skyboxPortal ? FN_RTCW_FOG_PORTAL : FN_RTCW_FOG_CURRENT );
	frame.fog = state->slots[slot];
	frame.skyFog = state->slots[FN_RTCW_FOG_SKY];
	gate = view->rtcw.skyboxPortal ? state->slots[FN_RTCW_FOG_PORTAL].registered :
		( state->selected > FN_RTCW_FOG_NONE ? qtrue : qfalse );
	frame.fogEnabled = gate && frame.fog.registered ? qtrue : qfalse;
	frame.skyEnabled = gate && frame.skyFog.registered ? qtrue : qfalse;
	if ( seenPortal && !view->rtcw.skyboxPortal && ( multiplayer || !view->rtcw.drawSkybox ) )
		frame.drawSky = qfalse;
	if ( state->selected > FN_RTCW_FOG_NONE && !state->slots[FN_RTCW_FOG_CURRENT].drawsky )
		frame.drawSky = qfalse;
	return frame;
}
#endif
