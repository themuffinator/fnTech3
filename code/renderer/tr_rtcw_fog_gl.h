/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_GL_H
#define FNTECH3_TR_RTCW_FOG_GL_H
#include "../renderercommon/tr_rtcw_fog_gpu.h"

static fnRTCWFogDraw_t rtcwStageFog;

void RB_RTCWFogToggle( qboolean enabled ) {
	fnRTCWFogDraw_t draw = rtcwStageFog;
	vec4_t color;
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP ) return;
	if ( !enabled || backEnd.projection2D ) draw.color[3] = 0;
#ifdef RENDERER_GLX
	GLX_Renderer_SetRTCWFog( draw.color, draw.params );
#endif
	if ( draw.color[3] == 0 ) {
		qglDisable( GL_FOG );
		return;
	}
	VectorCopy( draw.color, color ); color[3] = 1;
	qglFogfv( GL_FOG_COLOR, color );
	qglFogf( GL_FOG_MODE, draw.color[3] == 2 ? GL_EXP : GL_LINEAR );
	qglFogf( GL_FOG_START, draw.params[0] );
	qglFogf( GL_FOG_END, draw.params[1] );
	qglFogf( GL_FOG_DENSITY, draw.params[2] );
	qglHint( GL_FOG_HINT, GL_DONT_CARE );
	if ( rtcwRadialFogAvailable ) {
		/* GL_NV_fog_distance tokens; only issue them when advertised. */
		qglFogf( 0x855a, 0x855b );
	}
	qglEnable( GL_FOG );
}

void RB_RTCWStageFog( const shaderStage_t *stage ) {
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP ) return;
	rtcwStageFog = FnRTCWFogDraw( &backEnd.refdef.rtcwFog,
		tess.shader && tess.shader->isSky ? qtrue : qfalse,
		backEnd.projection2D, qfalse, qfalse,
		ri.gameId == COM_GAME_RTCW_SP && backEnd.refdef.rtcw.snooperView ? qtrue : qfalse,
		rtcwRadialFogAvailable );
	RB_RTCWFogToggle( !tess.shader || !tess.shader->rtcwNoFog ||
		( stage && stage->rtcwFog ) ? qtrue : qfalse );
}

qboolean RB_RTCWHasFog( void ) {
	return backEnd.refdef.rtcwFog.fogEnabled || backEnd.refdef.rtcwFog.skyEnabled ? qtrue : qfalse;
}
#endif
