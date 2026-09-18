/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_DECAL_WORLD_H
#define FNTECH3_TR_WOLFET_DECAL_WORLD_H

/* Enemy Territory's decals meet the map here. In every view, each projector
 * of the scene still in sight is pressed onto every map surface the view adds
 * that it reaches, and whatever survives the clip is kept on the surface's
 * brush model -- 1024 decals for the world, 128 for any other -- replacing the
 * one that fades soonest when there is no room. A kept decal is drawn in any
 * view that adds the surface it lies on, and ages as it is drawn. Included by
 * a renderer's world code. */

static srfWolfETDecal_t wolfetDecalSurfaces[FN_WOLFET_MAX_DECALS];
static int wolfetNumDecalSurfaces;

void R_ResetWolfETDecalSurfaces( void ) {
	wolfetNumDecalSurfaces = 0;
}

/* ET culls the scene's projectors against each view as it begins, and one
 * culled stays off for the rest of the scene. */
static void R_CullWolfETDecalProjectors( void ) {
	int i, last = 0;
	for ( i = 0; i < tr.numWolfETDecalProjectors; i++ ) {
		fnWolfETDecalProjector_t *dp = &tr.wolfetDecalProjectors[i];
		if ( R_CullPointAndRadius( dp->center, dp->radius ) == CULL_OUT ) {
			dp->shader = -1;
		} else {
			last = i + 1;
		}
	}
	tr.numWolfETDecalProjectors = last;
}

static void R_WolfETDecalOntoTriangle( const fnWolfETDecalProjector_t *dp, const float *a,
	const float *b, const float *c, msurface_t *surf, bmodel_t *bmodel ) {
	float points[2][FN_WOLFET_MAX_DECAL_VERTS][3];
	fnWolfETDecal_t decal;
	int slot;
	VectorCopy( a, points[0][0] );
	VectorCopy( b, points[0][1] );
	VectorCopy( c, points[0][2] );
	if ( !FnWolfET_DecalWinding( dp, 3, points, &decal ) ) return;
	slot = FnWolfET_DecalSlot( bmodel->wolfetDecals, bmodel->numWolfETDecals );
	if ( slot < 0 ) return;
	decal.parent = surf;
	decal.fogIndex = surf->fogIndex;
	bmodel->wolfetDecals[slot] = decal;
}

/* A projector reaches a surface when their spheres overlap and, for a surface
 * with a plane, the surface faces the projector and its plane passes within
 * the projector's sphere. Surfaces that take no marks, and fog, take none. */
static void R_ProjectWolfETDecalOntoSurface( const fnWolfETDecalProjector_t *dp, msurface_t *surf,
	bmodel_t *bmodel ) {
	const cplane_t *plane = NULL;
	const float *mins, *maxs;
	vec3_t center, extent, faceBounds[2];
	float d;
	int i, x, y;
	if ( dp->shader < 0 || !bmodel->wolfetDecals ) return;
	switch ( *surf->data ) {
	case SF_FACE: {
		/* Not every renderer keeps a face's bounds, so they are measured. */
		const srfSurfaceFace_t *face = (const srfSurfaceFace_t *)surf->data;
		ClearBounds( faceBounds[0], faceBounds[1] );
		for ( i = 0; i < face->numPoints; i++ ) AddPointToBounds( face->points[i], faceBounds[0], faceBounds[1] );
		mins = faceBounds[0];
		maxs = faceBounds[1];
		plane = &face->plane;
		break;
	}
	case SF_TRIANGLES: {
		const srfTriangles_t *tris = (const srfTriangles_t *)surf->data;
		mins = tris->bounds[0];
		maxs = tris->bounds[1];
		break;
	}
	case SF_GRID: {
		const srfGridMesh_t *grid = (const srfGridMesh_t *)surf->data;
		mins = grid->meshBounds[0];
		maxs = grid->meshBounds[1];
		break;
	}
	default:
		return;
	}
	for ( i = 0; i < 3; i++ ) {
		center[i] = ( mins[i] + maxs[i] ) * 0.5f;
		extent[i] = maxs[i] - center[i];
	}
	if ( !FnWolfET_DecalTouchesSphere( dp, center, DotProduct( extent, extent ) ) ) return;
	if ( plane && ( plane->normal[0] || plane->normal[1] || plane->normal[2] ) ) {
		if ( DotProduct( dp->planes[0], plane->normal ) < -0.0001f ) return;
		d = DotProduct( dp->center, plane->normal ) - plane->dist;
		if ( fabsf( d ) >= dp->radius ) return;
	}
	if ( ( surf->shader->surfaceFlags & ( SURF_NOIMPACT | SURF_NOMARKS ) ) ||
		( surf->shader->contentFlags & CONTENTS_FOG ) ) return;

	switch ( *surf->data ) {
	case SF_FACE: {
		const srfSurfaceFace_t *face = (const srfSurfaceFace_t *)surf->data;
		const int *indexes = (const int *)( (const byte *)face + face->ofsIndices );
		for ( i = 0; i + 2 < face->numIndices; i += 3 ) {
			if ( indexes[i] >= face->numPoints || indexes[i + 1] >= face->numPoints ||
				indexes[i + 2] >= face->numPoints ) continue;
			R_WolfETDecalOntoTriangle( dp, face->points[indexes[i]], face->points[indexes[i + 1]],
				face->points[indexes[i + 2]], surf, bmodel );
		}
		break;
	}
	case SF_TRIANGLES: {
		const srfTriangles_t *tris = (const srfTriangles_t *)surf->data;
		for ( i = 0; i + 2 < tris->numIndexes; i += 3 ) {
			R_WolfETDecalOntoTriangle( dp, tris->verts[tris->indexes[i]].xyz,
				tris->verts[tris->indexes[i + 1]].xyz, tris->verts[tris->indexes[i + 2]].xyz, surf, bmodel );
		}
		break;
	}
	case SF_GRID: {
		const srfGridMesh_t *grid = (const srfGridMesh_t *)surf->data;
		for ( y = 0; y < grid->height - 1; y++ ) {
			for ( x = 0; x < grid->width - 1; x++ ) {
				const drawVert_t *dv = grid->verts + y * grid->width + x;
				R_WolfETDecalOntoTriangle( dp, dv[0].xyz, dv[grid->width].xyz, dv[1].xyz, surf, bmodel );
				R_WolfETDecalOntoTriangle( dp, dv[1].xyz, dv[grid->width].xyz, dv[grid->width + 1].xyz, surf, bmodel );
			}
		}
		break;
	}
	default:
		break;
	}
}

/* Every projector of the scene still on, pressed onto one world surface the
 * view has just added. */
static void R_ProjectWolfETDecals( msurface_t *surf ) {
	int i;
	if ( !tr.world ) return;
	for ( i = 0; i < tr.numWolfETDecalProjectors; i++ ) {
		R_ProjectWolfETDecalOntoSurface( &tr.wolfetDecalProjectors[i], surf, tr.world->bmodels );
	}
}

/* A brush model takes the scene's projectors in its own space. As in ET, a
 * projector is tried against the model's box moved to where the model is, but
 * not turned with it. */
static int R_WolfETBrushModelProjectors( const bmodel_t *bmodel, fnWolfETDecalProjector_t *local ) {
	vec3_t mins, maxs;
	int i, count = 0;
	if ( !bmodel->wolfetDecals ) return 0;
	for ( i = 0; i < tr.numWolfETDecalProjectors; i++ ) {
		const fnWolfETDecalProjector_t *dp = &tr.wolfetDecalProjectors[i];
		if ( dp->shader < 0 ) continue;
		VectorAdd( bmodel->bounds[0], tr.or.origin, mins );
		VectorAdd( bmodel->bounds[1], tr.or.origin, maxs );
		if ( FnWolfET_DecalTouchesBox( dp, mins, maxs ) ) {
			FnWolfET_DecalTransform( dp, (const float (*)[3])tr.or.axis, tr.or.origin, &local[count++] );
		}
	}
	return count;
}

/* Draw the decals of a brush model whose surfaces this view added. The copy
 * drawn is taken before the kept decal fades again; one that has finished
 * fading frees its slot. */
static void R_AddWolfETDecalSurfaces( bmodel_t *bmodel ) {
	int i;
	for ( i = 0; i < bmodel->numWolfETDecals; i++ ) {
		fnWolfETDecal_t *decal = &bmodel->wolfetDecals[i];
		const msurface_t *parent = (const msurface_t *)decal->parent;
		srfWolfETDecal_t *surface;
		shader_t *shader;
		int dlight = 0;
		if ( !decal->inUse || !parent || parent->viewCount != tr.viewCount ) continue;
		if ( wolfetNumDecalSurfaces >= FN_WOLFET_MAX_DECALS ) return;
		surface = &wolfetDecalSurfaces[wolfetNumDecalSurfaces++];
		surface->surfaceType = SF_WOLFET_DECAL;
		surface->numVerts = decal->numVerts;
		memcpy( surface->verts, decal->verts, (size_t)decal->numVerts * sizeof( surface->verts[0] ) );
		shader = R_GetShaderByHandle( decal->shader );
#ifdef USE_LEGACY_DLIGHTS
		switch ( *parent->data ) {
		case SF_FACE: dlight = ( (const srfSurfaceFace_t *)parent->data )->dlightBits != 0; break;
		case SF_GRID: dlight = ( (const srfGridMesh_t *)parent->data )->dlightBits != 0; break;
		case SF_TRIANGLES: dlight = ( (const srfTriangles_t *)parent->data )->dlightBits != 0; break;
		default: break;
		}
#endif
		FnWolfET_DecalAge( decal, tr.refdef.time );
		R_AddDrawSurf( (surfaceType_t *)surface, shader, decal->fogIndex, dlight );
	}
}

#endif
