/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_SCENE_H
#define FNTECH3_TR_RTCW_FOG_SCENE_H

/* Enemy Territory's fog is the other Wolfenstein profile's multiplayer fog,
 * with one difference in its own renderer: the colour is scaled by the
 * identity light as it is stored. Its scene selection and backend fog are
 * otherwise the same, line for line. */
void RE_SetRTCWFog( int command, int first, int second,
	float red, float green, float blue, float density ) {
	const qboolean wolfet = ri.gameId == COM_GAME_WOLFET ? qtrue : qfalse;
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP && !wolfet ) {
		ri.Error( ERR_DROP, "RTCW fog command outside its game profile" );
		return;
	}
	if ( wolfet ) {
		red *= tr.identityLight;
		green *= tr.identityLight;
		blue *= tr.identityLight;
	}
	if ( !FnRTCWFogSet( &tr.rtcwFog, ri.gameId != COM_GAME_RTCW_SP ? qtrue : qfalse,
		command, first, second, red, green, blue, density, tr.refdef.time ) )
		ri.Error( ERR_DROP, "Invalid RTCW fog command (%d %d %d)", command, first, second );
}

static void R_CopyRTCWFogToScene( const refdef_t *view ) {
	tr.refdef.rtcwFog = FnRTCWFogCapture( NULL, qfalse, qfalse, NULL );
	if ( ri.gameId == COM_GAME_WOLFET ) {
		/* ET's view record carries its own portal and sky flags; it has no
		 * snooper view, and the multiplayer rules never read a separate
		 * draw-skybox flag. */
		refdef_t translated = *view;
		memset( &translated.rtcw, 0, sizeof( translated.rtcw ) );
		translated.rtcw.skyboxPortal = view->wolfet.skyboxPortal;
		translated.rtcw.drawingSky = view->wolfet.drawingSky;
		translated.rtcw.underwater = view->wolfet.underwater;
		if ( translated.rtcw.skyboxPortal ) tr.rtcwSkyPortal = qtrue;
		tr.refdef.rtcwFog = FnRTCWFogCapture( &tr.rtcwFog, qtrue, tr.rtcwSkyPortal, &translated );
		if ( r_wolffog && !r_wolffog->integer ) {
			tr.refdef.rtcwFog.fogEnabled = tr.refdef.rtcwFog.skyEnabled = qfalse;
		}
		return;
	}
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP ) return;
	if ( view->rtcw.skyboxPortal ) tr.rtcwSkyPortal = qtrue;
	tr.refdef.rtcwFog = FnRTCWFogCapture( &tr.rtcwFog,
		ri.gameId == COM_GAME_RTCW_MP ? qtrue : qfalse, tr.rtcwSkyPortal, view );
	if ( r_wolffog && !r_wolffog->integer ) {
		tr.refdef.rtcwFog.fogEnabled = tr.refdef.rtcwFog.skyEnabled = qfalse;
	}
	/* All backends consume this immutable snapshot at their draw boundary. */
}
#endif
