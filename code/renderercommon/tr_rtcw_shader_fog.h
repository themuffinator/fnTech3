/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_SHADER_FOG_H
#define FNTECH3_TR_RTCW_SHADER_FOG_H

/* Return 0 for another shader keyword, 1 for a consumed RTCW directive and
 * -1 for malformed input. Original SP publishes map/water parameters to its
 * cgame via cvars; MP installs those slots directly. */
static int R_RTCWShaderFogDirective( const char *keyword, const char **text ) {
	const qboolean mp = ri.gameId == COM_GAME_RTCW_MP ? qtrue : qfalse;
	qboolean sky, water, linear;
	vec3_t color;
	const char *token;
	char *end;
	double parsed;
	float value;
	int farDistance;
	char command[256];
	if ( !mp && ri.gameId != COM_GAME_RTCW_SP ) return 0;
	if ( !Q_stricmp( keyword, "nofog" ) ) {
		shader.rtcwNoFog = qtrue;
		return 1;
	}
	sky = !Q_stricmp( keyword, "skyfogvars" ) ? qtrue : qfalse;
	water = !Q_stricmp( keyword, "waterfogvars" ) ? qtrue : qfalse;
	if ( !sky && !water && Q_stricmp( keyword, "fogvars" ) ) return 0;
	if ( !ParseVector( text, 3, color ) ) return -1;
	token = COM_ParseExt( text, qfalse );
	parsed = strtod( token, &end );
	if ( !*token || *end || !( parsed >= 0 && parsed < INT_MAX ) ||
		!FnRTCWFogFinite( color[0] ) || !FnRTCWFogFinite( color[1] ) ||
		!FnRTCWFogFinite( color[2] ) ) return -1;
	value = (float)parsed;
	/* The rounded float must also fit before conversion to the legacy int. */
	if ( (double)value >= INT_MAX || ( sky && value > 1 ) ) return -1;
	if ( sky ) {
		return FnRTCWFogSet( &tr.rtcwFog, mp, FN_RTCW_FOG_SKY, 0, 5,
			color[0], color[1], color[2], value, tr.refdef.time ) ? 1 : -1;
	}
	if ( water && value == 0 ) return 1; // Original's unimplemented inheritance is a no-op.
	linear = ( water || mp ? value > 1 : value >= 1 ) ? qtrue : qfalse;
	farDistance = linear ? (int)value : 5;
	if ( water && linear ) value = 1.1f;
	if ( mp ) {
		const int slot = water ? FN_RTCW_FOG_WATER : FN_RTCW_FOG_MAP;
		if ( !FnRTCWFogSet( &tr.rtcwFog, qtrue, slot, 0, farDistance,
			color[0], color[1], color[2], value, tr.refdef.time ) ) return -1;
		if ( !water ) FnRTCWFogSet( &tr.rtcwFog, qtrue, FN_RTCW_FOG_SWITCH,
			slot, 50, 0, 0, 0, 0, tr.refdef.time );
	} else {
		Com_sprintf( command, sizeof( command ), "0 %d %f %f %f %f %d",
			farDistance, value, color[0], color[1], color[2], water ? 200 : 0 );
		ri.Cvar_Set( water ? "r_waterFogColor" : "r_mapFogColor", command );
	}
	return 1;
}
#endif
