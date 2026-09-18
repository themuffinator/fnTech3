/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_LIGHT_SCENE_H
#define FNTECH3_WOLFET_LIGHT_SCENE_H
#include "tr_rtcw_light_math.h"

/* Enemy Territory's dynamic light is its own contract. It names a radius and a
 * brightness separately, where every other profile sends one magnitude, and it
 * carries a shader handle and three flags. Its flags are recorded from the
 * original game/q_shared.h; the top three bits are the whole vocabulary. */
#define FNTECH3_WOLFET_REF_FORCE_DLIGHT    ( 1u << 31 )
#define FNTECH3_WOLFET_REF_JUNIOR_DLIGHT   ( 1u << 30 )
#define FNTECH3_WOLFET_REF_DIRECTED_DLIGHT ( 1u << 29 )

static void RE_WolfETLightNotice( const char *what, qboolean *reported ) {
	if ( *reported ) return;
	*reported = qtrue;
	ri.Printf( PRINT_ALL, "ET %s are not drawn by this renderer; those lights are skipped.\n", what );
}

void RE_AddWolfETLightToScene( const vec3_t origin, float radius, float intensity,
	float r, float g, float b, qhandle_t hShader, int flags ) {
	static qboolean reportedDirected, reportedJunior, reportedShader;
	const uint32_t bits = (uint32_t)flags;
	dlight_t light;
	float gain;
	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "ET light submitted outside its game profile" );
		return;
	}
	if ( !tr.registered || r_numdlights < 0 || r_numdlights >= 32 ||
		r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) return;
	if ( glConfig.hardwareType == GLHW_RIVA128 || glConfig.hardwareType == GLHW_PERMEDIA2 ) return;
	/* The original discards a light with no radius or no brightness before it
	 * reads anything else, and never publishes an unrepresentable position. */
	if ( !RTCW_LightFinite( radius ) || radius <= 0 ) return;
	if ( !RTCW_LightFinite( intensity ) || intensity <= 0 ) return;
	if ( !origin || !RTCW_LightFinite( origin[0] ) || !RTCW_LightFinite( origin[1] ) ||
		!RTCW_LightFinite( origin[2] ) ) return;
	if ( !RTCW_LightFinite( r ) || !RTCW_LightFinite( g ) || !RTCW_LightFinite( b ) ) return;
	/* A forced light ignores the dynamic-light setting, exactly as ET's does. */
	if ( !( bits & FNTECH3_WOLFET_REF_FORCE_DLIGHT ) && r_dynamiclight->integer == 0 ) return;
	/* A directed light asks for a global directional source whose origin is a
	 * direction, and a junior light asks to affect only the light grid. This
	 * renderer has neither, so those lights are skipped and said to be skipped
	 * rather than drawn as ordinary point lights in the wrong place. */
	if ( bits & FNTECH3_WOLFET_REF_DIRECTED_DLIGHT ) {
		RE_WolfETLightNotice( "global directional lights", &reportedDirected );
		return;
	}
	if ( bits & FNTECH3_WOLFET_REF_JUNIOR_DLIGHT ) {
		RE_WolfETLightNotice( "light-grid-only lights", &reportedJunior );
		return;
	}
	if ( hShader ) {
		RE_WolfETLightNotice( "per-light shaders", &reportedShader );
	}
	Com_Memset( &light, 0, sizeof( light ) );
	VectorCopy( origin, light.origin );
	light.radius = radius;
	/* ET scales its light's colour by the brightness it sends alongside the
	 * radius. Fold it in here so the separate value is not discarded. */
	gain = intensity;
	light.color[0] = r * gain;
	light.color[1] = g * gain;
	light.color[2] = b * gain;
	if ( !RTCW_LightFinite( light.color[0] ) || !RTCW_LightFinite( light.color[1] ) ||
		!RTCW_LightFinite( light.color[2] ) ) return;
#ifdef USE_PMLIGHT
	light.shadowIndex = light.shadowAtlasBaseFace = light.shadowSpotSource = light.shadowSpotSourceIndex = -1;
	light.shadowPriorityMultiplier = 1.0f;
	if ( R_GetDlightMode() ) {
		R_ApplyDlightColorControls( &light.color[0], &light.color[1], &light.color[2] );
		if ( !RTCW_LightFinite( light.color[0] ) || !RTCW_LightFinite( light.color[1] ) ||
			!RTCW_LightFinite( light.color[2] ) ) return;
		if ( light.radius > WORLD_SIZE ) return;
		light.shadowEligible = qtrue;
	}
#endif
	backEndData->dlights[r_numdlights++] = light;
}
#endif
