/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_LIGHT_SCENE_H
#define FNTECH3_RTCW_LIGHT_SCENE_H
#include "tr_rtcw_light_math.h"
#include "tr_rtcw_engine_lights.h"
#ifdef USE_PMLIGHT
#include "tr_rtcw_light_shadow.h"
static rtcwLightShadowHistory_t rtcwLightShadowHistory[32];
#endif
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
	/* Retail's 32 a frame count module lights alone: the engine's own and a
	 * portal view's copies never take one (tr_rtcw_engine_lights.h). */
	if ( !tr.registered || r_numModuleDlights < 0 || r_numModuleDlights >= RTCW_FRAME_MODULE_LIGHTS ||
		r_numdlights < 0 || r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) return;
	if ( glConfig.hardwareType == GLHW_RIVA128 || glConfig.hardwareType == GLHW_PERMEDIA2 ) return;
	if ( ri.gameId == COM_GAME_RTCW_SP ) {
		if ( !rtcwRadiusScale ) rtcwRadiusScale = ri.Cvar_Get( "r_dlightScale", "1.0", CVAR_ARCHIVE );
		scale = rtcwRadiusScale->value;
	}
	/* Retail MP menus draw a zero-sized player preview whose UI computes an
	 * infinite light origin. The empty view is discarded by R_RenderView.
	 * Cull unrepresentable positions here without publishing them to any scene,
	 * but count them: retail stores any light its enable checks pass, whatever
	 * its position, and it takes one of the 32. */
	if ( origin && ( !RTCW_LightFinite( origin[0] ) || !RTCW_LightFinite( origin[1] ) ||
		!RTCW_LightFinite( origin[2] ) ) ) {
		static const float anywhere[3] = { 0.0f, 0.0f, 0.0f };
		if ( RTCW_LightParameters( ri.gameId == COM_GAME_RTCW_SP, r_dynamiclight->integer, scale,
			anywhere, intensity, color, (uint32_t)flags, &parameters ) > 0 ) r_numModuleDlights++;
		return;
	}
#ifndef USE_LEGACY_DLIGHTS
	ri.Error( ERR_DROP, "RTCW requires a renderer built with projected-light support" );
	return;
#endif
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
	/* Retail has stored the light by now, so it takes one of the 32 even when
	 * the per-pixel path below cannot draw it. */
	r_numModuleDlights++;
#ifdef USE_PMLIGHT
	light.shadowIndex = light.shadowAtlasBaseFace = light.shadowSpotSource = light.shadowSpotSourceIndex = -1;
	light.shadowPriorityMultiplier = 1.0f;
	/* Ordinary RTCW lights share the Q3/QL per-pixel pass and shadow atlas.
	 * Authored subtractive/flame materials keep their projected blend passes. */
	if ( R_GetDlightMode() && !parameters.material ) {
		const float gain = r_dlightIntensity->value * ( parameters.overdraw + 1 );
		if ( ri.gameId == COM_GAME_RTCW_MP ) {
			scale = r_dlightScale->value;
			if ( !RTCW_LightFinite( scale ) || scale <= 0 ) return;
			light.radius *= scale;
		}
		if ( !RTCW_LightFinite( light.radius ) || light.radius <= 0 ||
			light.radius > WORLD_SIZE || !RTCW_LightFinite( gain ) || gain <= 0 ) return;
		VectorScale( light.color, gain, light.color );
		R_ApplyDlightColorControls( &light.color[0], &light.color[1], &light.color[2] );
		if ( !RTCW_LightFinite( light.color[0] ) || !RTCW_LightFinite( light.color[1] ) ||
			!RTCW_LightFinite( light.color[2] ) ) return;
		light.shadowEligible = qtrue;
		light.shadowProjectionFar = R_RTCWLightShadowProjection( rtcwLightShadowHistory,
			ARRAY_LEN( rtcwLightShadowHistory ), origin, light.radius, tr.frameCount );
	}
#endif
	backEndData->dlights[r_numdlights++] = light;
}
#endif
