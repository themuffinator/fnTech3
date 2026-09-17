/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_SCENE_H
#define FNTECH3_TR_RTCW_FOG_SCENE_H

void RE_SetRTCWFog( int command, int first, int second,
	float red, float green, float blue, float density ) {
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP ) {
		ri.Error( ERR_DROP, "RTCW fog command outside its game profile" );
		return;
	}
	if ( !FnRTCWFogSet( &tr.rtcwFog, ri.gameId == COM_GAME_RTCW_MP ? qtrue : qfalse,
		command, first, second, red, green, blue, density, tr.refdef.time ) )
		ri.Error( ERR_DROP, "Invalid RTCW fog command (%d %d %d)", command, first, second );
}

static void R_CopyRTCWFogToScene( const refdef_t *view ) {
	tr.refdef.rtcwFog = FnRTCWFogCapture( NULL, qfalse, qfalse, NULL );
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP ) return;
	if ( view->rtcw.skyboxPortal ) tr.rtcwSkyPortal = qtrue;
	tr.refdef.rtcwFog = FnRTCWFogCapture( &tr.rtcwFog,
		ri.gameId == COM_GAME_RTCW_MP ? qtrue : qfalse, tr.rtcwSkyPortal, view );
	if ( r_wolffog && !r_wolffog->integer ) {
		tr.refdef.rtcwFog.fogEnabled = tr.refdef.rtcwFog.skyEnabled = qfalse;
	}
#ifndef FNTECH3_RTCW_GL_FOG
	/* Vulkan/RTX stage and sky paths have not yet admitted this contract. */
	if ( tr.refdef.rtcwFog.fogEnabled ||
		( tr.refdef.rtcwFog.skyEnabled && tr.refdef.rtcwFog.drawSky && !r_fastsky->integer ) )
		ri.Error( ERR_DROP, "RTCW scene fog GPU application is not implemented" );
#endif
}
#endif
