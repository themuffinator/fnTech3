/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_MOHAA_MARKS_SURFACE_H
#define FNTECH3_MOHAA_MARKS_SURFACE_H

/*
Medal of Honor's decal fragments, shared by the three world renderers after
their inherited Quake III clipper (R_ChopPolyBehindPlane and
R_AddMarkFragments, above).

1.00's client game cuts every decal through two imports (its mark helper at
0x30023b10): the world's fragments (slot 49), each naming what it lies on --
0 for the world's own surfaces, a terrain patch's number plus one for
terrain, which it draws through slot 92 -- and then, for each brush model in
the decal's bounds, that model's (slot 50), in the model's own space, which
it carries with the model. Written anew after OpenMoHAA's renderergl1/
tr_marks.c (reference only):

- the world's faces and curves are cut as Quake III cuts them, between
  planes 32 units before the decal and 20 behind it;
- a terrain patch's triangles are cut -- those its variance tree takes out
  are not in the built world (tr_mohaa_world.c) -- between planes 96 units
  further before and 108 further behind, 128 each way. Measured, along the
  projection of the nearest decal of the same shader: the 3,371 terrain
  fragments of 1.00's permanent decal files reach 128.00 units behind their
  decals (three exactly) and 128.00 before (37 exactly, and 13 a little
  further, to 139.1), where the 2,032 on the world's surfaces stay within
  32.4 before and, bar three, 20.5 behind;
- a decal whose squared size -- its two half-widths squared, added -- is no
  more than ter_minMarkRadius squared cuts no terrain: at its default of 8,
  a bullet hole (1.00's are 2 units, half as much again for a heavy round,
  either give or take a fifth; 0x3002d331) never does. No 1.00 file names
  the variable; its default is OpenMoHAA's;
- the built world's other triangle surfaces, its static models and its
  permanent decals, take none, as 2015's renderer, which kept both apart
  from the world's surfaces, cut none;
- a brush model's faces and curves are cut in its own space, the decal
  taken into it by the model's angles and origin, and the fragments stay
  there.
*/

#define MOHAA_MARK_SURFACES			256
#define MOHAA_MARK_WORLD_FRONT		32.0f
#define MOHAA_MARK_WORLD_BACK		20.0f
#define MOHAA_MARK_TERRAIN_FRONT	96.0f
#define MOHAA_MARK_TERRAIN_BACK		108.0f

typedef struct {
	vec3_t			direction;
	vec3_t			mins, maxs;
	int				numPlanes;
	vec3_t			normals[MAX_VERTS_ON_POLY + 2];
	float			dists[MAX_VERTS_ON_POLY + 2];
	float			terrainDists[MAX_VERTS_ON_POLY + 2];
	qboolean		cutTerrain;
	msurface_t		*surfaces[MOHAA_MARK_SURFACES];
	int				numSurfaces;
	int				maxPoints;
	vec_t			*pointBuffer;
	int				maxFragments;
	markFragment_t	*fragments;
	int				*patches;
	int				returnedPoints;
	int				returnedFragments;
} mohaaMarkQuery_t;

/* Whether a decal may be cut against this surface: a face its bounds cross
 * that faces the projection, a curve, or a terrain patch. */
static qboolean R_MOHAAMarkSurface( const mohaaMarkQuery_t *query, const msurface_t *surf ) {
	if ( ( surf->shader->surfaceFlags & ( SURF_NOIMPACT | SURF_NOMARKS ) )
		|| ( surf->shader->contentFlags & CONTENTS_FOG ) ) {
		return qfalse;
	}
	switch ( *surf->data ) {
	case SF_FACE: {
		srfSurfaceFace_t *face = (srfSurfaceFace_t *)surf->data;
		const int side = BoxOnPlaneSide( (float *)query->mins, (float *)query->maxs, &face->plane );
		return side != 1 && side != 2 && DotProduct( face->plane.normal, query->direction ) <= -0.5f
			? qtrue : qfalse;
	}
	case SF_GRID:
		return qtrue;
	case SF_TRIANGLES:
		return query->cutTerrain && R_MOHAATerrainPatch( tr.world, surf ) >= 0 ? qtrue : qfalse;
	default:
		return qfalse;
	}
}

static void R_MOHAAMarkCollect( mohaaMarkQuery_t *query, mnode_t *node ) {
	msurface_t **mark;
	int count;

	while ( node->contents == CONTENTS_NODE ) {
		const int side = BoxOnPlaneSide( query->mins, query->maxs, node->plane );
		if ( side == 1 ) {
			node = node->children[0];
		} else if ( side == 2 ) {
			node = node->children[1];
		} else {
			R_MOHAAMarkCollect( query, node->children[0] );
			node = node->children[1];
		}
	}

	mark = node->firstmarksurface;
	for ( count = node->nummarksurfaces; count > 0 && query->numSurfaces < MOHAA_MARK_SURFACES; count--, mark++ ) {
		msurface_t *surf = *mark;
		/* a surface in more than one leaf is weighed once */
		if ( surf->viewCount == tr.viewCount ) {
			continue;
		}
		surf->viewCount = tr.viewCount;
		if ( R_MOHAAMarkSurface( query, surf ) ) {
			query->surfaces[query->numSurfaces++] = surf;
		}
	}
}

/* One polygon clipped by the decal's planes; what is left, a fragment on
 * `index` (a terrain patch plus one, or 0). */
static void R_MOHAAMarkPolygon( mohaaMarkQuery_t *query, vec3_t clipPoints[2][MAX_VERTS_ON_POLY], float *dists,
	int index ) {
	const int before = query->returnedFragments;

	R_AddMarkFragments( 3, clipPoints, query->numPlanes, query->normals, dists, query->maxPoints,
		query->pointBuffer, query->maxFragments, query->fragments, &query->returnedPoints,
		&query->returnedFragments, query->mins, query->maxs );
	if ( query->patches && query->returnedFragments > before ) {
		query->patches[before] = index;
	}
}

static void R_MOHAAMarkCut( mohaaMarkQuery_t *query, const msurface_t *surf ) {
	vec3_t clipPoints[2][MAX_VERTS_ON_POLY];
	vec3_t v1, v2, normal;
	int j, k, m, n;

	switch ( *surf->data ) {
	case SF_FACE: {
		const srfSurfaceFace_t *face = (const srfSurfaceFace_t *)surf->data;
		const int *indexes = (const int *)( (const byte *)face + face->ofsIndices );
		for ( k = 0; k + 2 < face->numIndices && query->returnedFragments < query->maxFragments; k += 3 ) {
			for ( j = 0; j < 3; j++ ) {
				const float *v = &face->points[0][0] + VERTEXSIZE * indexes[k + j];
				VectorMA( v, MARKER_OFFSET, face->plane.normal, clipPoints[0][j] );
			}
			R_MOHAAMarkPolygon( query, clipPoints, query->dists, 0 );
		}
		break;
	}
	case SF_GRID: {
		/* each quad as two triangles, each cut only where it faces the
		 * projection, as Quake III cuts a curve */
		const srfGridMesh_t *grid = (const srfGridMesh_t *)surf->data;
		for ( m = 0; m < grid->height - 1; m++ ) {
			for ( n = 0; n < grid->width - 1 && query->returnedFragments < query->maxFragments; n++ ) {
				const drawVert_t *dv = grid->verts + m * grid->width + n;

				VectorMA( dv[0].xyz, MARKER_OFFSET, dv[0].normal, clipPoints[0][0] );
				VectorMA( dv[grid->width].xyz, MARKER_OFFSET, dv[grid->width].normal, clipPoints[0][1] );
				VectorMA( dv[1].xyz, MARKER_OFFSET, dv[1].normal, clipPoints[0][2] );
				VectorSubtract( clipPoints[0][0], clipPoints[0][1], v1 );
				VectorSubtract( clipPoints[0][2], clipPoints[0][1], v2 );
				CrossProduct( v1, v2, normal );
				VectorNormalizeFast( normal );
				if ( DotProduct( normal, query->direction ) < -0.1f ) {
					R_MOHAAMarkPolygon( query, clipPoints, query->dists, 0 );
					if ( query->returnedFragments >= query->maxFragments ) {
						return;
					}
				}

				VectorMA( dv[1].xyz, MARKER_OFFSET, dv[1].normal, clipPoints[0][0] );
				VectorMA( dv[grid->width].xyz, MARKER_OFFSET, dv[grid->width].normal, clipPoints[0][1] );
				VectorMA( dv[grid->width + 1].xyz, MARKER_OFFSET, dv[grid->width + 1].normal, clipPoints[0][2] );
				VectorSubtract( clipPoints[0][0], clipPoints[0][1], v1 );
				VectorSubtract( clipPoints[0][2], clipPoints[0][1], v2 );
				CrossProduct( v1, v2, normal );
				VectorNormalizeFast( normal );
				if ( DotProduct( normal, query->direction ) < -0.05f ) {
					R_MOHAAMarkPolygon( query, clipPoints, query->dists, 0 );
				}
			}
		}
		break;
	}
	case SF_TRIANGLES: {
		/* a terrain patch, every triangle it keeps */
		const srfTriangles_t *tris = (const srfTriangles_t *)surf->data;
		const int patch = R_MOHAATerrainPatch( tr.world, surf );
		for ( k = 0; k + 2 < tris->numIndexes && query->returnedFragments < query->maxFragments; k += 3 ) {
			for ( j = 0; j < 3; j++ ) {
				VectorCopy( tris->verts[tris->indexes[k + j]].xyz, clipPoints[0][j] );
			}
			R_MOHAAMarkPolygon( query, clipPoints, query->terrainDists, patch + 1 );
		}
		break;
	}
	default:
		break;
	}
}

int R_MarkMOHAAFragments( int model, const vec3_t angles, const vec3_t origin, int numPoints, const vec3_t *points,
	const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragments,
	int *patches, float radiusSquared ) {
	mohaaMarkQuery_t query;
	vec3_t local[MAX_VERTS_ON_POLY];
	vec3_t localProjection;
	int i;

	if ( ri.gameId != COM_GAME_MOHAA || !tr.world || !tr.world->nodes ) {
		return 0;
	}
	if ( numPoints < 3 || !points || !projection || maxPoints <= 0 || !pointBuffer || maxFragments <= 0
		|| !fragments ) {
		return 0;
	}
	if ( model < 0 || model >= tr.world->numBmodels || ( model > 0 && ( !angles || !origin ) ) ) {
		return 0;
	}
	if ( numPoints > MAX_VERTS_ON_POLY ) {
		numPoints = MAX_VERTS_ON_POLY;
	}

	/* a brush model's decal is cut in the model's space */
	if ( model > 0 ) {
		vec3_t axis[3], delta;
		if ( angles[0] || angles[1] || angles[2] ) {
			AngleVectors( angles, axis[0], axis[1], axis[2] );
			VectorInverse( axis[1] );
			for ( i = 0; i < numPoints; i++ ) {
				VectorSubtract( points[i], origin, delta );
				local[i][0] = DotProduct( delta, axis[0] );
				local[i][1] = DotProduct( delta, axis[1] );
				local[i][2] = DotProduct( delta, axis[2] );
			}
			localProjection[0] = DotProduct( projection, axis[0] );
			localProjection[1] = DotProduct( projection, axis[1] );
			localProjection[2] = DotProduct( projection, axis[2] );
		} else {
			for ( i = 0; i < numPoints; i++ ) {
				VectorSubtract( points[i], origin, local[i] );
			}
			VectorCopy( projection, localProjection );
		}
	} else {
		for ( i = 0; i < numPoints; i++ ) {
			VectorCopy( points[i], local[i] );
		}
		VectorCopy( projection, localProjection );
	}

	Com_Memset( &query, 0, sizeof( query ) );
	if ( VectorNormalize2( localProjection, query.direction ) == 0.0f ) {
		return 0;
	}
	ClearBounds( query.mins, query.maxs );
	for ( i = 0; i < numPoints; i++ ) {
		vec3_t temp;
		AddPointToBounds( local[i], query.mins, query.maxs );
		VectorAdd( local[i], localProjection, temp );
		AddPointToBounds( temp, query.mins, query.maxs );
		/* and the leafs in front of the surface hit */
		VectorMA( local[i], -20.0f, query.direction, temp );
		AddPointToBounds( temp, query.mins, query.maxs );
	}

	/* the planes through the decal's edges, then before and behind it */
	for ( i = 0; i < numPoints; i++ ) {
		vec3_t v1, v2;
		VectorSubtract( local[( i + 1 ) % numPoints], local[i], v1 );
		VectorAdd( local[i], localProjection, v2 );
		VectorSubtract( local[i], v2, v2 );
		CrossProduct( v1, v2, query.normals[i] );
		VectorNormalizeFast( query.normals[i] );
		query.dists[i] = DotProduct( query.normals[i], local[i] );
	}
	VectorCopy( query.direction, query.normals[numPoints] );
	query.dists[numPoints] = DotProduct( query.normals[numPoints], local[0] ) - MOHAA_MARK_WORLD_FRONT;
	VectorCopy( query.direction, query.normals[numPoints + 1] );
	VectorInverse( query.normals[numPoints + 1] );
	query.dists[numPoints + 1] = DotProduct( query.normals[numPoints + 1], local[0] ) - MOHAA_MARK_WORLD_BACK;
	query.numPlanes = numPoints + 2;
	Com_Memcpy( query.terrainDists, query.dists, sizeof( query.dists ) );
	query.terrainDists[numPoints] -= MOHAA_MARK_TERRAIN_FRONT;
	query.terrainDists[numPoints + 1] -= MOHAA_MARK_TERRAIN_BACK;
	query.cutTerrain = model == 0 && tr.world->mohaaNumTerrain > 0
		&& !( ter_minMarkRadius && ter_minMarkRadius->value * ter_minMarkRadius->value >= radiusSquared );

	query.maxPoints = maxPoints;
	query.pointBuffer = pointBuffer;
	query.maxFragments = maxFragments;
	query.fragments = fragments;
	query.patches = patches;

	tr.viewCount++;
	if ( model == 0 ) {
		R_MOHAAMarkCollect( &query, tr.world->nodes );
	} else {
		const bmodel_t *bmodel = &tr.world->bmodels[model];
		for ( i = 0; i < bmodel->numSurfaces && query.numSurfaces < MOHAA_MARK_SURFACES; i++ ) {
			msurface_t *surf = bmodel->firstSurface + i;
			if ( surf->viewCount == tr.viewCount ) {
				continue;
			}
			surf->viewCount = tr.viewCount;
			if ( R_MOHAAMarkSurface( &query, surf ) ) {
				query.surfaces[query.numSurfaces++] = surf;
			}
		}
	}

	for ( i = 0; i < query.numSurfaces && query.returnedFragments < query.maxFragments; i++ ) {
		R_MOHAAMarkCut( &query, query.surfaces[i] );
	}
	return query.returnedFragments;
}

#endif
