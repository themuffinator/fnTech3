/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_LIGHT_SCENE_H
#define FNTECH3_WOLFET_LIGHT_SCENE_H
#include "tr_rtcw_light_math.h"
#include "tr_rtcw_engine_lights.h"

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
	/* Retail's 32 a frame count module lights alone: the engine's own and a
	 * portal view's copies never take one (tr_rtcw_engine_lights.h). */
	if ( !tr.registered || r_numModuleDlights < 0 || r_numModuleDlights >= RTCW_FRAME_MODULE_LIGHTS ||
		r_numdlights < 0 || r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) return;
	/* The original's own refusals, in its order: a light with no radius or no
	 * brightness, these cards, and an unforced light while dynamic lights are
	 * off. A forced light ignores that setting, exactly as ET's does. */
	if ( radius <= 0 || intensity <= 0 ) return;
	if ( glConfig.hardwareType == GLHW_RIVA128 || glConfig.hardwareType == GLHW_PERMEDIA2 ) return;
	if ( !( bits & FNTECH3_WOLFET_REF_FORCE_DLIGHT ) && r_dynamiclight->integer == 0 ) return;
	/* The original stores every light past them, so each takes one of the
	 * frame's 32 whether or not this renderer draws it. */
	r_numModuleDlights++;
	/* An unrepresentable magnitude, position or colour is never published. */
	if ( !RTCW_LightFinite( radius ) || !RTCW_LightFinite( intensity ) ) return;
	if ( !origin || !RTCW_LightFinite( origin[0] ) || !RTCW_LightFinite( origin[1] ) ||
		!RTCW_LightFinite( origin[2] ) ) return;
	if ( !RTCW_LightFinite( r ) || !RTCW_LightFinite( g ) || !RTCW_LightFinite( b ) ) return;
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
