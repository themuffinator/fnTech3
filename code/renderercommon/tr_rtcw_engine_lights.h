/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* The engine's own scene lights in an RTCW or Enemy Territory scene: static
 * map lights (maps/<map>.lights.json), surface-light proxies and r_dlightTest.
 *
 * RTCW SP and MP, and ET after them, accept 32 module lights a frame, counted
 * across every scene the frame renders (their RE_AddLightToScene refuses once
 * the frame holds MAX_DLIGHTS). The engine adds its own lights as a scene is
 * rendered, after that scene's module lights, and both cgames render a sky
 * portal as a scene of its own before the main view submits any. So the
 * engine's lights, and a portal view's copies of them, take only slots the
 * frame's modules can no longer claim.
 *
 * In RTCW they are drawn before the module lights, because both RTCW light
 * materials multiply what is already drawn: negdlightshader darkens it
 * (GL_ZERO, GL_ONE_MINUS_SRC_COLOR) and flamedlightshader brightens it
 * (GL_DST_COLOR, GL_ONE). An engine light is part of the world's own light
 * beneath them, as the lightmap is. ET draws no light material, so its lights
 * keep the order they were added in. */
#ifndef FNTECH3_RTCW_ENGINE_LIGHTS_H
#define FNTECH3_RTCW_ENGINE_LIGHTS_H

#define RTCW_FRAME_MODULE_LIGHTS 32
/* A scene's lights are named by 32-bit surface masks. */
#define RTCW_SCENE_LIGHTS 32

/* Whether an RTCW scene takes the engine's lights. They join only the shared
 * per-pixel pass (r_dlightMode 1 or 2): mode 0 is the original projected-light
 * option, whose projection has no spot form and no per-pixel falloff.
 * r_dynamiclight 0 and 2 keep only a module's forced lights, which the
 * engine's are not. The SP snooper scope's heat view draws no scene light on
 * the world, and lights its models itself. */
static ID_INLINE int RTCW_SceneTakesEngineLights( int dlightMode, int dynamicLight, int snooperView ) {
	return dlightMode != 0 && dynamicLight != 0 && dynamicLight != 2 && !snooperView;
}

/* Whether an Enemy Territory scene takes the engine's lights. ET's
 * r_dynamiclight 0 keeps only a module's forced lights, which the engine's are
 * not. ET has no projected-light mode of its own, so every shared mode draws
 * them, as it does for every other profile. */
static ID_INLINE int WolfET_SceneTakesEngineLights( int dynamicLight ) {
	return dynamicLight != 0;
}

/* One past the last frame slot the engine's lights may fill in a light set
 * that starts at slot first, once the frame's modules have submitted
 * moduleLights: every slot the modules may still claim stays free, and the
 * set keeps within its surface masks. */
static ID_INLINE int RTCW_EngineLightEnd( int capacity, int moduleLights, int first ) {
	int end;
	if ( moduleLights < 0 ) moduleLights = 0;
	if ( moduleLights > RTCW_FRAME_MODULE_LIGHTS ) moduleLights = RTCW_FRAME_MODULE_LIGHTS;
	end = capacity - ( RTCW_FRAME_MODULE_LIGHTS - moduleLights );
	if ( first >= 0 && first < end - RTCW_SCENE_LIGHTS ) end = first + RTCW_SCENE_LIGHTS;
	return end;
}

/* How many of a parent view's lights a portal view copies, from slot used on.
 * The scene's module lights lead every view's set; they are copied while the
 * frame has room, as for any profile. The engine's that follow them are
 * copied only up to engineEnd. The copy is always a leading run, so a nested
 * view's own set keeps the same order. */
static ID_INLINE int RTCW_PortalLightCopies( int parentLights, int sceneModuleLights, int used,
	int capacity, int engineEnd ) {
	int module, copies;
	if ( parentLights <= 0 || used < 0 || used >= capacity ) return 0;
	module = sceneModuleLights < 0 ? 0 : sceneModuleLights < parentLights ? sceneModuleLights : parentLights;
	copies = module < capacity - used ? module : capacity - used;
	if ( copies == module && engineEnd - used - copies > 0 ) {
		const int engine = parentLights - module;
		copies += engine < engineEnd - used - copies ? engine : engineEnd - used - copies;
	}
	return copies;
}

/* The light a view's per-light pass draws at position: the engine's, which
 * follow the scene's leading moduleLights, first; then the module's, in the
 * order they were submitted. */
static ID_INLINE int RTCW_LightDrawIndex( int position, int count, int moduleLights ) {
	if ( moduleLights <= 0 || moduleLights >= count ) return position;
	return position < count - moduleLights ? moduleLights + position : position - ( count - moduleLights );
}
#endif
