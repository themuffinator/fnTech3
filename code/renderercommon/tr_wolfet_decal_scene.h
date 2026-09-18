/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_DECAL_SCENE_H
#define FNTECH3_TR_WOLFET_DECAL_SCENE_H

/* Enemy Territory's decal requests, as its modules make them between scenes.
 * A request is checked, culled against the last view drawn -- as ET does, for
 * ET builds its projector before the scene it belongs to -- and kept until the
 * next scene takes it. ET keeps the requests of one frame in a ring of 32, so
 * a scene is given at most the 32 most recent. Included by a renderer's scene
 * code. */

static fnWolfETDecalProjector_t wolfetDecalRing[FN_WOLFET_MAX_DECAL_PROJECTORS];
static int wolfetDecalCount, wolfetDecalFirst;

void RE_ProjectWolfETDecal( qhandle_t hShader, int numPoints, const vec3_t *points,
	const vec4_t projection, const vec4_t color, int lifeTime, int fadeTime ) {
	fnWolfETDecalProjector_t projector;
	fnWolfETDecalCorner_t corners[4];
	float direction[4];
	int footprint;
	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "ET decal projected outside its game profile" );
		return;
	}
	if ( !tr.registered ) return;
	if ( !FnWolfET_DecalProjectorStart( hShader, numPoints, (const float (*)[3])points, projection, color,
		lifeTime, fadeTime, tr.refdef.time, &projector, corners, direction, &footprint ) ) return;
	if ( R_CullPointAndRadius( projector.center, projector.radius ) == CULL_OUT ) return;
	if ( !FnWolfET_DecalProjectorPlanes( &projector, corners, direction, footprint ) ) return;
	wolfetDecalRing[wolfetDecalCount % FN_WOLFET_MAX_DECAL_PROJECTORS] = projector;
	wolfetDecalCount++;
}

/* Every decal on the world and on every brush model goes at once. */
void RE_ClearWolfETDecals( void ) {
	int i, j;
	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "ET decals cleared outside their game profile" );
		return;
	}
	if ( !tr.world ) return;
	for ( i = 0; i < tr.world->numBmodels; i++ ) {
		bmodel_t *bmodel = &tr.world->bmodels[i];
		for ( j = 0; j < bmodel->numWolfETDecals; j++ ) bmodel->wolfetDecals[j].inUse = 0;
	}
}

static void R_ResetWolfETDecalRequests( void ) {
	wolfetDecalCount = wolfetDecalFirst = 0;
}

/* A scene takes every request made since the previous scene, oldest first. */
static void R_CopyWolfETDecalProjectorsToScene( void ) {
	int count = wolfetDecalCount - wolfetDecalFirst, first = wolfetDecalFirst, i;
	tr.numWolfETDecalProjectors = 0;
	if ( ri.gameId != COM_GAME_WOLFET ) return;
	if ( count > FN_WOLFET_MAX_DECAL_PROJECTORS ) {
		first += count - FN_WOLFET_MAX_DECAL_PROJECTORS;
		count = FN_WOLFET_MAX_DECAL_PROJECTORS;
	}
	for ( i = 0; i < count; i++ ) {
		tr.wolfetDecalProjectors[i] = wolfetDecalRing[( first + i ) % FN_WOLFET_MAX_DECAL_PROJECTORS];
	}
	tr.numWolfETDecalProjectors = count;
	wolfetDecalFirst = wolfetDecalCount;
}

#endif
