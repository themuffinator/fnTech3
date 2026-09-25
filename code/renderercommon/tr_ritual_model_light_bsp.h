/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RITUAL_MODEL_LIGHT_BSP_H
#define FNTECH3_TR_RITUAL_MODEL_LIGHT_BSP_H

/* Loading a Ritual map's compiled lights for model light
 * (tr_ritual_model_light.h). The FAKK translation keeps none of them, so
 * R_LoadWorldMap reads them from the map file before it translates, and keeps
 * them on the hunk with the world once its leaves are in. Included by a
 * renderer's tr_bsp.c after s_worldData. */

static ritualModelLights_t *r_ritualModelLightsPending;

static void *R_RitualModelLightsAlloc( int size ) {
	return ri.Malloc( size );
}

/* Before the translation: the lights of the map file as it is. A malformed
 * light lump, which retail refuses the map for, leaves the map's models on
 * the grid, with a warning. */
static void R_RitualModelLightsRead( const byte *file, int length, const char *name ) {
	char error[128];
	if ( r_ritualModelLightsPending ) {
		ri.Free( r_ritualModelLightsPending );
		r_ritualModelLightsPending = NULL;
	}
	r_ritualModelLightsPending = RitualModelLights_Parse( file, length, R_RitualModelLightsAlloc,
		error, (int)sizeof( error ) );
	if ( !r_ritualModelLightsPending && error[0] ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s: models keep the light grid: %s\n", name, error );
	}
}

/* Once the world is in: keep them with it, if they list its leaves. */
static void R_RitualModelLightsKeep( void ) {
	ritualModelLights_t *pending = r_ritualModelLightsPending;
	r_ritualModelLightsPending = NULL;
	s_worldData.ritualModelLights = NULL;
	if ( !pending ) {
		return;
	}
	if ( pending->numLeafs != s_worldData.numnodes - s_worldData.numDecisionNodes ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s: models keep the light grid: %i light lists for %i leaves\n",
			s_worldData.name, pending->numLeafs, s_worldData.numnodes - s_worldData.numDecisionNodes );
	} else {
		ritualModelLights_t *kept = (ritualModelLights_t *)ri.Hunk_Alloc( pending->size, h_low );
		Com_Memcpy( kept, pending, pending->size );
		s_worldData.ritualModelLights = kept;
	}
	ri.Free( pending );
}

#endif
