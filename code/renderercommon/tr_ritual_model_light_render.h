/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RITUAL_MODEL_LIGHT_RENDER_H
#define FNTECH3_TR_RITUAL_MODEL_LIGHT_RENDER_H

/* Model light from a Ritual map's compiled lights, in a renderer
 * (tr_ritual_model_light.h). Included by a renderer's tr_light.c after
 * R_SetupEntityLightingGrid; R_SetupAliceEntityLighting calls it. */

extern cvar_t *r_ritualModelLights;
extern cvar_t *r_ritualModelLightsDebug;

/* The map's leaf holding a point, walked as the renderer's own lookup does. */
static int R_RitualModelLightLeafAt( void *context, const float point[3] ) {
	const mnode_t *node;
	(void)context;
	if ( !tr.world || !tr.world->nodes ) {
		return -1;
	}
	node = tr.world->nodes;
	while ( node->contents == -1 ) {
		const float d = DotProduct( point, node->plane->normal ) - node->plane->dist;
		node = d > 0 ? node->children[0] : node->children[1];
	}
	return (int)( node - ( tr.world->nodes + tr.world->numDecisionNodes ) );
}

/* Retail's traces: a point, solid brushes only, the world alone
 * (alice.exe 0x004ab8b1, 0x004aa88c). */
static int R_RitualModelLightClear( void *context, const float start[3], const float end[3] ) {
	trace_t trace;
	(void)context;
	if ( !ri.CM_BoxTrace ) {
		return 1;
	}
	ri.CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_SOLID, qfalse );
	return trace.fraction >= 1.0f;
}

static int R_RitualModelLightSky( void *context, const float start[3], const float end[3] ) {
	trace_t trace;
	(void)context;
	if ( !ri.CM_BoxTrace ) {
		return 0;
	}
	ri.CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_SOLID, qfalse );
	return ( trace.surfaceFlags & SURF_SKY ) != 0;
}

static void R_RitualModelLightDebug( const trRefdef_t *refdef, const trRefEntity_t *ent, const vec3_t lightOrigin,
	float reach, int result, const ritualModelLightStats_t *stats, const float cube[6][3] ) {
	static int lastSecond = -1, printTime = -1;
	trRefEntity_t grid;
	const int second = refdef->time / 1000;
	if ( second != lastSecond ) {
		lastSecond = second;
		printTime = refdef->time;
	}
	if ( refdef->time != printTime ) {
		return;
	}
	ri.Printf( PRINT_ALL, "ritual model light: %s fx 0x%x at %.1f %.1f %.1f reach %.1f: %s, %i leaves, "
		"%i of %i lights, sun %i\n", tr.currentModel ? tr.currentModel->name : "?", ent->e.alice.renderfx,
		lightOrigin[0], lightOrigin[1], lightOrigin[2], reach,
		result == RITUAL_ML_LIT ? "compiled lights" : "identity", stats->leaves, stats->lit, stats->candidates,
		stats->sun );
	ri.Printf( PRINT_ALL, "  +x %.0f %.0f %.0f  -x %.0f %.0f %.0f  +y %.0f %.0f %.0f  -y %.0f %.0f %.0f  "
		"+z %.0f %.0f %.0f  -z %.0f %.0f %.0f\n", cube[0][0], cube[0][1], cube[0][2], cube[1][0], cube[1][1],
		cube[1][2], cube[2][0], cube[2][1], cube[2][2], cube[3][0], cube[3][1], cube[3][2], cube[4][0], cube[4][1],
		cube[4][2], cube[5][0], cube[5][1], cube[5][2] );
	ri.Printf( PRINT_ALL, "  ambient %.0f %.0f %.0f  directed %.0f %.0f %.0f  toward %.2f %.2f %.2f\n",
		ent->ambientLight[0], ent->ambientLight[1], ent->ambientLight[2], ent->directedLight[0],
		ent->directedLight[1], ent->directedLight[2], ent->lightDir[0], ent->lightDir[1], ent->lightDir[2] );
	if ( tr.world->lightGridData ) {
		grid = *ent;
		grid.e.renderfx |= RF_LIGHTING_ORIGIN;
		VectorCopy( lightOrigin, grid.e.lightingOrigin );
		R_SetupEntityLightingGrid( &grid );
		ri.Printf( PRINT_ALL, "  grid ambient %.0f %.0f %.0f  directed %.0f %.0f %.0f  toward %.2f %.2f %.2f\n",
			grid.ambientLight[0], grid.ambientLight[1], grid.ambientLight[2], grid.directedLight[0],
			grid.directedLight[1], grid.directedLight[2], grid.lightDir[0], grid.lightDir[1], grid.lightDir[2] );
	}
}

/*
=================
R_RitualModelLight

Lights an entity its game module flags RF_EXTRALIGHT or RF_INVISIBLE from the
map's compiled lights, as Alice's and F.A.K.K.2's renderers do, into the
ambient and directed light and world direction every renderer shades a model
with. localPoint and reach are where it is lit in its own frame and half its
frame's radius, scaled. Returns qfalse -- for the grid, as before -- when the
entity is not flagged or is full bright, the view has no world, the map
carries no usable lights, or r_ritualModelLights is 0.

The light is shifted as the renderer shifts the map's lightmaps and grid
(R_ColorShiftLightingBytes), which is each game's own shift at its defaults:
none for Alice, two map bits for F.A.K.K.2.
=================
*/
static qboolean R_RitualModelLight( const trRefdef_t *refdef, trRefEntity_t *ent, const vec3_t lightOrigin,
	const float *localPoint, float reach ) {
	const ritualModelLights_t *map;
	ritualModelLightWorld_t world;
	ritualModelLightEntity_t entity;
	ritualModelLightStats_t stats;
	float cube[6][3], direction[3];
	const int renderfx = ent->e.alice.renderfx;
	const int bits = r_mapOverBrightBits->integer - tr.overbrightBits;
	const float shift = (float)ldexp( 1.0, bits < -16 ? -16 : bits > 16 ? 16 : bits );
	int result, k;

	if ( !r_ritualModelLights || !r_ritualModelLights->integer || !localPoint
		|| ( refdef->rdflags & RDF_NOWORLDMODEL ) || !tr.world || !tr.world->ritualModelLights ) {
		return qfalse;
	}
	if ( !( renderfx & ( RITUAL_ML_RF_EXTRALIGHT | RITUAL_ML_RF_INVISIBLE ) )
		|| ( renderfx & RITUAL_ML_RF_FULLBRIGHT ) ) {
		return qfalse;
	}
	map = tr.world->ritualModelLights;

	world.context = NULL;
	world.leafAt = R_RitualModelLightLeafAt;
	world.clear = R_RitualModelLightClear;
	world.sky = R_RitualModelLightSky;
	world.areamask = refdef->areamask;
	world.areamaskBytes = (int)sizeof( refdef->areamask );

	VectorCopy( ent->e.origin, entity.origin );
	for ( k = 0; k < 3; k++ ) {
		VectorCopy( ent->e.axis[k], entity.axis[k] );
		entity.point[k] = lightOrigin[k];
		entity.localPoint[k] = localPoint[k];
		entity.lightOffset[k] = ent->e.shader.rgba[k];
	}
	entity.reach = reach;
	entity.renderfx = renderfx;

	result = RitualModelLight_Cube( map, &world, &entity, shift, cube, &stats );
	RitualModelLight_Fold( cube, ent->ambientLight, ent->directedLight, direction );
	for ( k = 0; k < 3; k++ ) {
		ent->lightDir[k] = direction[0] * ent->e.axis[0][k] + direction[1] * ent->e.axis[1][k]
			+ direction[2] * ent->e.axis[2][k];
	}
	VectorNormalize( ent->lightDir );

	if ( r_ritualModelLightsDebug && r_ritualModelLightsDebug->integer ) {
		R_RitualModelLightDebug( refdef, ent, lightOrigin, reach, result, &stats, (const float (*)[3])cube );
	}
	return qtrue;
}

#endif
