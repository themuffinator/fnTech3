/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_LIGHT_SCENE_H
#define FNTECH3_RTCW_LIGHT_SCENE_H
#include "tr_rtcw_light_math.h"
static cvar_t *rtcwRadiusScale;

void RE_AddRTCWLightToScene( const vec3_t origin, float intensity, float r, float g, float b, int flags ) {
	rtcwLightParameters_t parameters;
	dlight_t light;
	const float color[3] = { r, g, b };
	int accepted;
	float scale = 1.0f;
	if ( !R_UsesRTCWLighting() ) {
		ri.Error( ERR_DROP, "RTCW light submitted outside its game profile" );
		return;
	}
	if ( !tr.registered || r_numdlights < 0 || r_numdlights >= 32 ||
		r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) return;
	if ( glConfig.hardwareType == GLHW_RIVA128 || glConfig.hardwareType == GLHW_PERMEDIA2 ) return;
	/* Retail MP menus draw a zero-sized player preview whose UI computes an
	 * infinite light origin. The empty view is discarded by R_RenderView.
	 * Cull unrepresentable positions here without publishing them to any scene. */
	if ( origin && ( !RTCW_LightFinite( origin[0] ) || !RTCW_LightFinite( origin[1] ) ||
		!RTCW_LightFinite( origin[2] ) ) ) return;
#ifndef USE_LEGACY_DLIGHTS
	ri.Error( ERR_DROP, "RTCW requires a renderer built with projected-light support" );
	return;
#endif
	if ( ri.gameId == COM_GAME_RTCW_SP ) {
		if ( !rtcwRadiusScale ) rtcwRadiusScale = ri.Cvar_Get( "r_dlightScale", "1.0", CVAR_ARCHIVE );
		scale = rtcwRadiusScale->value;
	}
	accepted = RTCW_LightParameters( ri.gameId == COM_GAME_RTCW_SP,
		r_dynamiclight->integer, scale, origin, intensity, color, (uint32_t)flags, &parameters );
	if ( accepted < 0 ) {
		ri.Error( ERR_DROP, "Invalid RTCW light: origin=(%g %g %g), radius=%g, color=(%g %g %g), flags=0x%x",
			origin ? origin[0] : 0, origin ? origin[1] : 0, origin ? origin[2] : 0, intensity, r, g, b, (unsigned)flags );
		return;
	}
	if ( !accepted ) return;
	Com_Memset( &light, 0, sizeof( light ) );
	VectorCopy( origin, light.origin );
	VectorCopy( color, light.color );
	light.radius = parameters.radius;
	light.rtcwOverdraw = parameters.overdraw;
	if ( parameters.material ) {
		light.rtcwShader = R_FindRTCWLightShader( parameters.material == 1 ? "negdlightshader" : "flamedlightshader" );
	}
#ifdef USE_PMLIGHT
	light.shadowIndex = light.shadowAtlasBaseFace = light.shadowSpotSource = light.shadowSpotSourceIndex = -1;
	light.shadowPriorityMultiplier = 1.0f;
#endif
	backEndData->dlights[r_numdlights++] = light;
}
#endif
