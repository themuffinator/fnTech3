/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_CORONA_SCENE_H
#define FNTECH3_RTCW_CORONA_SCENE_H

static fnRTCWCorona_t rtcwCoronas[FN_RTCW_MAX_CORONAS];
static int rtcwCoronaCount, rtcwCoronaFirst;

void RE_AddRTCWCoronaToScene( const fnRTCWCorona_t *corona ) {
	if ( !FnRTCWCoronaProfile( ri.gameId ) )
		ri.Error( ERR_DROP, "RTCW corona outside its game profile" );
	if ( !FnRTCWCoronaValid( corona ) ) ri.Error( ERR_DROP, "Invalid RTCW corona" );
	if ( !tr.registered || rtcwCoronaCount >= FN_RTCW_MAX_CORONAS ) return;
	rtcwCoronas[rtcwCoronaCount++] = *corona;
}

static void R_CopyRTCWCoronasToScene( void ) {
	tr.refdef.numRTCWCoronas = 0;
	if ( !FnRTCWCoronaProfile( ri.gameId ) ) return;
	tr.refdef.numRTCWCoronas = rtcwCoronaCount - rtcwCoronaFirst;
	Com_Memcpy( tr.refdef.rtcwCoronas, rtcwCoronas + rtcwCoronaFirst,
		tr.refdef.numRTCWCoronas * sizeof( rtcwCoronas[0] ) );
	// Draw commands receive the complete snapshot, before later scenes reuse it.
	rtcwCoronaFirst = rtcwCoronaCount;
}

#endif
