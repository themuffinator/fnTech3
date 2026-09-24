/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_GL_H
#define FNTECH3_TR_RTCW_FOG_GL_H
#include "../renderercommon/tr_rtcw_fog_blend.h"

static fnRTCWFogDraw_t rtcwStageFog;

/* American McGee's Alice's far plane fogs linearly from the eye to its
 * distance in its colour: retail's far-plane fog pass sets alpha to
 * min( |v - eye| / distance, 1 ) per vertex (alice.exe 0x0049ebb0, colour
 * 0x0049eb40), and its GL fog path is linear from 0 to the distance
 * (0x00474a8d, 0x004779f5). A sky is fogged only when the far plane culls
 * (RB_BeginSurface 0x004927d3..0x004927ff). The pass reuses RTCW's per-stage
 * GL fog, including its neutral colour for blended stages. */
static qboolean RB_AliceFarFogActive( void ) {
	return ri.gameId == COM_GAME_ALICE && backEnd.viewParms.aliceFarFog &&
		backEnd.viewParms.aliceFarDistance > 0.0f ? qtrue : qfalse;
}

static fnRTCWFogDraw_t RB_AliceFarFogDraw( void ) {
	fnRTCWFogDraw_t draw;
	int channel;
	memset( &draw, 0, sizeof( draw ) );
	if ( !RB_AliceFarFogActive() || backEnd.projection2D ||
		( tess.shader && tess.shader->isSky && !backEnd.viewParms.aliceFarCull ) ) {
		return draw;
	}
	for ( channel = 0; channel < 3; ++channel ) {
		const float value = backEnd.viewParms.aliceFarColor[channel] * tr.identityLight;
		draw.color[channel] = value < 0 ? 0 : value > 1 ? 1 : value;
	}
	draw.color[3] = 1.0f;	/* linear */
	draw.params[0] = 0.0f;
	draw.params[1] = backEnd.viewParms.aliceFarDistance;
	draw.params[2] = 1.0f;
	draw.params[3] = rtcwRadialFogAvailable ? 1.0f : 0.0f;
	return draw;
}

void RB_RTCWFogToggle( qboolean enabled ) {
	fnRTCWFogDraw_t draw = rtcwStageFog;
	vec4_t color;
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP && ri.gameId != COM_GAME_ALICE ) return;
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
	if ( ri.gameId == COM_GAME_ALICE ) {
		rtcwStageFog = RB_AliceFarFogDraw();
		FnRTCWFogBlendColor( &rtcwStageFog, stage ? stage->stateBits : 0 );
		RB_RTCWFogToggle( qtrue );
		return;
	}
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP ) return;
	rtcwStageFog = FnRTCWFogDraw( &backEnd.refdef.rtcwFog,
		tess.shader && tess.shader->isSky ? qtrue : qfalse,
		backEnd.projection2D, qfalse, qfalse,
		ri.gameId == COM_GAME_RTCW_SP && backEnd.refdef.rtcw.snooperView ? qtrue : qfalse,
		rtcwRadialFogAvailable );
	FnRTCWFogBlendColor( &rtcwStageFog, stage ? stage->stateBits : 0 );
	RB_RTCWFogToggle( !tess.shader || !tess.shader->rtcwNoFog ||
		( stage && stage->rtcwFog ) ? qtrue : qfalse );
}

qboolean RB_RTCWHasFog( void ) {
	if ( ri.gameId == COM_GAME_ALICE ) {
		return RB_AliceFarFogActive();
	}
	return backEnd.refdef.rtcwFog.fogEnabled || backEnd.refdef.rtcwFog.skyEnabled ? qtrue : qfalse;
}
#endif
