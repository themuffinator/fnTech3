/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_ENTITY_SUN_H
#define FNTECH3_TR_ENTITY_SUN_H

/* The sun a level gives in its entities rather than in a sky shader, which
 * the renderer's sun shadows (r_csmShadows) then take as they take a
 * `q3map_sun`:
 *
 * - Alice and F.A.K.K.2: `suncolor` and `sundirection` from anywhere in the
 *   entity text, the last of each winning, as their engines read them for
 *   model light (alice.exe 0x004ac3b0, fakk2.exe 0x0048e8d0;
 *   RitualModelLight_Keys);
 * - Medal of Honor: the worldspawn's `suncolor`, or `sunlight` under its
 *   other name, and `sundirection` (R_MOHAASunKey).
 *
 * In all three `sundirection` is angles whose forward vector points toward
 * the sun: Ritual's model light traces that way to the sky, and 2015's
 * lightmaps are brightest on the walls facing it
 * (docs/fntech3/2026-10-16-mohaa-worldspawn-sun.md). A level without a
 * direction, or whose sun has no colour, keeps the sun its sky shader gave,
 * if any; none of the three games' installed maps names both. Nor does a
 * sun at or below the horizon cast: F.A.K.K.2's `otto` alone points down,
 * 65 degrees, and a shadow map lit from beneath would shade every floor.
 *
 * Only the sun that the shadows read is set. tr.sunDirection, which places
 * the sky's sun image and lights a model where no light grid reaches -- the
 * F.A.K.K.2 HUD's weapons, for one -- stays the sky shader's, as those
 * engines left it. Included by a renderer's tr_bsp.c after s_worldData. */

static qboolean R_EntitySunFinite( const vec3_t v ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		unsigned int bits;
		/* bits, not v == v, which a renderer's fast-math options may fold */
		Com_Memcpy( &bits, &v[i], sizeof( bits ) );
		if ( ( ( bits >> 23 ) & 0xffu ) == 0xffu ) {
			return qfalse;
		}
	}
	return qtrue;
}

static void R_SetWorldSunFromEntities( const char *source, const vec3_t light, const vec3_t towardSun ) {
	vec3_t direction, color;
	float brightest;

	if ( !R_EntitySunFinite( light ) || !R_EntitySunFinite( towardSun ) ) {
		return;
	}
	brightest = MAX( light[0], MAX( light[1], light[2] ) );
	if ( !( brightest > 0.0f ) || VectorNormalize2( towardSun, direction ) <= 0.0f || !( direction[2] > 0.0f ) ) {
		return;
	}
	color[0] = Com_Clamp( 0.0f, 1.0f, light[0] / brightest );
	color[1] = Com_Clamp( 0.0f, 1.0f, light[1] / brightest );
	color[2] = Com_Clamp( 0.0f, 1.0f, light[2] / brightest );

	Com_Memset( &tr.worldSun, 0, sizeof( tr.worldSun ) );
	tr.worldSun.valid = qtrue;
	Q_strncpyz( tr.worldSun.shaderName, source, sizeof( tr.worldSun.shaderName ) );
	VectorCopy( color, tr.worldSun.color );
	VectorCopy( direction, tr.worldSun.directionToSun );
	VectorScale( direction, -1.0f, tr.worldSun.lightDirection );
	VectorCopy( light, tr.worldSun.light );
	tr.worldSun.intensity = brightest;

	VectorCopy( tr.worldSun.color, tr.sunColor );
	VectorCopy( tr.worldSun.light, tr.sunLight );
	tr.sunIntensity = tr.worldSun.intensity;
	tr.sunParmsValid = qtrue;
}

/* After R_LoadEntities, which keeps the entity text and reads Medal of
 * Honor's sun keys. */
static void R_LoadEntitySun( void ) {
	vec3_t light, towardSun;

	if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {
		ritualModelLights_t keys;
		if ( !s_worldData.entityString ) {
			return;
		}
		Com_Memset( &keys, 0, sizeof( keys ) );
		RitualModelLight_Keys( s_worldData.entityString, (int)strlen( s_worldData.entityString ), &keys );
		/* A colour without `sundirection` leaves the direction cleared, which
		 * lights nothing in the model light and is no sun to shadow. */
		if ( !keys.sun || VectorLengthSquared( keys.sunDirection ) <= 0.0f ) {
			return;
		}
		VectorCopy( keys.sunColor, light );
		VectorCopy( keys.sunDirection, towardSun );
		R_SetWorldSunFromEntities( "entities", light, towardSun );
	} else if ( ri.gameId == COM_GAME_MOHAA ) {
		if ( R_MOHAAWorldSun( light, towardSun ) ) {
			R_SetWorldSunFromEntities( "worldspawn", light, towardSun );
		}
	}
}

#endif
