/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_GLOBAL_FOG_SCENE_H
#define FNTECH3_TR_WOLFET_GLOBAL_FOG_SCENE_H

/* Writes Enemy Territory's retuned global fog into the volume the map compiled
 * for it. The volume's own fields are written, not the shader's: a shader can
 * be shared between volumes, and this profile's request names one volume. */
static void R_WolfETWriteGlobalFog( const float value[4] ) {
	fog_t *fog;
	vec3_t color;
	float depth;
	int i;
	if ( !tr.world || tr.world->wolfetGlobalFog.index <= 0 ||
		tr.world->wolfetGlobalFog.index >= tr.world->numfogs ) return;
	fog = &tr.world->fogs[tr.world->wolfetGlobalFog.index];
	VectorCopy( value, fog->parms.color );
	fog->parms.depthForOpaque = value[3];
	VectorCopy( value, color );
	/* The same two adjustments the map's own fog was loaded through, so a
	 * retuned fog and an authored one are shaded alike. */
	if ( r_mapGreyScale->value > 0 ) {
		const float luminance = LUMA( color[0], color[1], color[2] );
		for ( i = 0; i < 3; i++ ) color[i] = LERP( color[i], luminance, r_mapGreyScale->value );
	}
	for ( i = 0; i < 3; i++ ) {
		float channel = color[i] * tr.identityLight * 255.0f;
		if ( !( channel >= 0 ) ) channel = 0;
		if ( channel > 255.0f ) channel = 255.0f;
		fog->colorInt.rgba[i] = (byte)channel;
	}
	fog->colorInt.rgba[3] = 255;
	for ( i = 0; i < 4; i++ ) fog->color[i] = (float)fog->colorInt.rgba[i] / 255.0f;
	depth = value[3] < 1 ? 1 : value[3];
	fog->tcScale = 1.0f / ( depth * 8 );
}

/* Moves a running fade on by one frame. */
static void R_WolfETAdvanceGlobalFog( void ) {
	float value[4];
	if ( !tr.world ) return;
	if ( FnWolfET_GlobalFogFade( &tr.world->wolfetGlobalFog, tr.refdef.time, value ) )
		R_WolfETWriteGlobalFog( value );
}

void RE_SetWolfETGlobalFog( qboolean restore, int duration, float red, float green,
	float blue, float depthForOpaque ) {
	const vec3_t rgb = { red, green, blue };
	float current[4], value[4];
	const fog_t *fog;
	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "WolfET global fog outside its game profile" );
		return;
	}
	/* A map that compiled no world-covering volume has no global fog to
	 * retune. ET's own renderer does the same nothing, without complaint. */
	if ( !tr.world || tr.world->wolfetGlobalFog.index <= 0 ||
		tr.world->wolfetGlobalFog.index >= tr.world->numfogs ) return;
	fog = &tr.world->fogs[tr.world->wolfetGlobalFog.index];
	VectorCopy( fog->parms.color, current );
	current[3] = fog->parms.depthForOpaque;
	if ( !FnWolfET_GlobalFogSet( &tr.world->wolfetGlobalFog, current, restore ? 1 : 0,
		duration, tr.refdef.time, rgb, depthForOpaque, value ) ) {
		ri.Error( ERR_DROP, "Invalid WolfET global fog request" );
		return;
	}
	R_WolfETWriteGlobalFog( value );
}

#endif
