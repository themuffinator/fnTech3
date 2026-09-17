/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_MARKS_SURFACE_H
#define FNTECH3_RTCW_MARKS_SURFACE_H

/* Shared by the three world renderers, after their inherited Q3 polygon
 * clipper. Independently authored from RTCW SP/MP contract observations.
 * Keep the original Q3 entrypoint, vertex stride and facing tests separate. */
#include "tr_rtcw_marks.h"
#include <limits.h>

typedef struct {
	vec3_t center, direction, mins, maxs;
	vec3_t planes[6];
	float distances[6], radius;
	int rotation, maxPoints, maxFragments, usedPoints, usedFragments;
	qboolean oldMapping;
	fnRTCWMarkPoint_t *points;
	markFragment_t *fragments;
	surfaceType_t *surfaces[4096];
	int numSurfaces, remainingNodes;
} rtcwMarkQuery_t;

static qboolean R_RTCWMarkFinite( const vec3_t value ) {
	return FnRTCWMarkFinite( value[0] ) && FnRTCWMarkFinite( value[1] ) && FnRTCWMarkFinite( value[2] ) ? qtrue : qfalse;
}

static float R_RTCWMarkNormalize( const vec3_t input, vec3_t output ) {
	const float squared = DotProduct( input, input );
	const float length = sqrtf( squared );
	if ( !FnRTCWMarkFinite( length ) ) ri.Error( ERR_DROP, "RTCW mark geometry overflow" );
	if ( length > 0 ) { VectorScale( input, 1.0f / length, output ); }
	else { VectorClear( output ); }
	return length;
}

static qboolean R_RTCWMarkPlanes( const vec3_t *corners, const vec3_t direction,
	vec3_t *planes, float *distances ) {
	int i;
	for ( i = 0; i < 4; ++i ) {
		vec3_t edge, normal;
		VectorSubtract( corners[(i + 1) % 4], corners[i], edge );
		CrossProduct( edge, direction, normal );
		if ( !R_RTCWMarkNormalize( normal, planes[i] ) ) return qfalse;
		distances[i] = DotProduct( planes[i], corners[i] );
		if ( !FnRTCWMarkFinite( distances[i] ) ) ri.Error( ERR_DROP, "RTCW mark plane overflow" );
	}
	return qtrue;
}

static void R_RTCWMarkCollect( rtcwMarkQuery_t *query, mnode_t *node, int depth ) {
	int i;
	if ( depth > 256 ) ri.Error( ERR_DROP, "RTCW mark BSP depth exceeds limit" );
	// Like RTCW, visibility is checked on entry to each recursive branch.
	if ( (int64_t)node->visframe < (int64_t)tr.visCount - 2 ) return;
	for ( ;; ) {
		int side;
		if ( --query->remainingNodes < 0 ) ri.Error( ERR_DROP, "Invalid RTCW mark BSP traversal" );
		if ( node->contents != CONTENTS_NODE ) break;
		side = BoxOnPlaneSide( query->mins, query->maxs, node->plane );
		if ( side == 3 ) R_RTCWMarkCollect( query, node->children[0], depth + 1 );
		node = node->children[side == 1 ? 0 : 1];
		if ( query->numSurfaces == 4096 ) return;
	}
	if ( node->contents & CONTENTS_TRANSLUCENT ) return;
	for ( i = 0; i < node->nummarksurfaces && query->numSurfaces < 4096; ++i ) {
		msurface_t *surface = node->firstmarksurface[i];
		int seen;
		if ( (surface->shader->surfaceFlags & (SURF_NOIMPACT | SURF_NOMARKS)) ||
			(surface->shader->contentFlags & CONTENTS_FOG) ) continue;
		if ( *surface->data == SF_FACE ) {
			srfSurfaceFace_t *face = (srfSurfaceFace_t *)surface->data;
			if ( BoxOnPlaneSide( query->mins, query->maxs, &face->plane ) != 3 ||
				DotProduct( face->plane.normal, query->direction ) < -0.5f ) continue;
		} else if ( *surface->data != SF_GRID ) continue;
		// A query owns its deduplication, without modifying render visibility.
		for ( seen = 0; seen < query->numSurfaces; ++seen )
			if ( query->surfaces[seen] == surface->data ) break;
		if ( seen == query->numSurfaces ) query->surfaces[query->numSurfaces++] = surface->data;
	}
}

static void R_RTCWMarkTriangle( rtcwMarkQuery_t *query, const vec3_t *triangle,
	int numPlanes, vec3_t *planes, float *distances,
	const vec3_t textureOrigin, const vec3_t sAxis, const vec3_t tAxis ) {
	vec3_t clipped[2][MAX_VERTS_ON_POLY];
	int count = 3, current = 0, i;
	markFragment_t *fragment;
	if ( query->usedFragments >= query->maxFragments ) return;
	for ( i = 0; i < 3; ++i ) {
		if ( !R_RTCWMarkFinite( triangle[i] ) ) ri.Error( ERR_DROP, "Invalid RTCW mark surface vertex" );
		VectorCopy( triangle[i], clipped[0][i] );
	}
	for ( i = 0; i < numPlanes && count; ++i ) {
		R_ChopPolyBehindPlane( count, clipped[current], &count, clipped[!current], planes[i], distances[i], 0.5f );
		current = !current;
	}
	if ( count < 3 || count > query->maxPoints - query->usedPoints ) return;
	fragment = &query->fragments[query->usedFragments++];
	fragment->firstPoint = query->usedPoints;
	fragment->numPoints = textureOrigin ? -count : count;
	for ( i = 0; i < count; ++i ) {
		fnRTCWMarkPoint_t *point = &query->points[query->usedPoints++];
		VectorCopy( clipped[current][i], point->xyz );
		if ( textureOrigin ) {
			vec3_t delta;
			VectorSubtract( point->xyz, textureOrigin, delta );
			point->st[0] = 0.5f + DotProduct( delta, sAxis ) * (0.5f / query->radius);
			point->st[1] = 0.5f + DotProduct( delta, tAxis ) * (0.5f / query->radius);
		}
	}
}

static qboolean R_RTCWMarkFace( rtcwMarkQuery_t *query, srfSurfaceFace_t *face ) {
	vec3_t planes[4], origin, sAxis, tAxis;
	float distances[4];
	int i, j;
	const int *indices = (const int *)((const byte *)face + face->ofsIndices);
	if ( !query->oldMapping ) {
		vec3_t normal, perpendicular, corners[4];
		float distance = DotProduct( query->center, face->plane.normal ) - face->plane.dist;
		if ( (distance < -0.5f && DotProduct( face->plane.normal, query->direction ) >= 0.01f) ||
			fabsf( distance ) > query->radius ) return qfalse;
		VectorMA( query->center, -distance, query->direction, origin );
		distance = DotProduct( origin, face->plane.normal ) - face->plane.dist;
		VectorMA( origin, -distance, face->plane.normal, origin );
		if ( !R_RTCWMarkNormalize( face->plane.normal, normal ) ) return qfalse;
		PerpendicularVector( perpendicular, normal );
		RotatePointAroundVector( tAxis, normal, perpendicular, (float)query->rotation );
		CrossProduct( normal, tAxis, sAxis );
		for ( i = 0; i < 4; ++i ) for ( j = 0; j < 3; ++j )
			corners[i][j] = origin[j] + ((i == 1 || i == 2) ? 1 : -1) * query->radius * sAxis[j] +
				(i >= 2 ? 1 : -1) * query->radius * tAxis[j];
		if ( !R_RTCWMarkPlanes( corners, face->plane.normal, planes, distances ) ) return qfalse;
	}
	if ( face->numIndices < 0 || face->numIndices % 3 ) ri.Error( ERR_DROP, "Invalid RTCW mark face indices" );
	for ( i = 0; i < face->numIndices && query->usedFragments < query->maxFragments; i += 3 ) {
		vec3_t triangle[3];
		for ( j = 0; j < 3; ++j ) {
			const int index = indices[i + j];
			if ( index < 0 || index >= face->numPoints ) ri.Error( ERR_DROP, "Invalid RTCW mark face vertex" );
			VectorCopy( face->points[index], triangle[j] );
		}
		R_RTCWMarkTriangle( query, triangle, query->oldMapping ? 6 : 4,
			query->oldMapping ? query->planes : planes, query->oldMapping ? query->distances : distances,
			query->oldMapping ? NULL : origin, sAxis, tAxis );
	}
	return qtrue;
}

static void R_RTCWMarkGrid( rtcwMarkQuery_t *query, const srfGridMesh_t *grid, int numPlanes ) {
	int row, column, half, j;
	if ( grid->width < 2 || grid->height < 2 ) return;
	if ( grid->width > MAX_GRID_SIZE || grid->height > MAX_GRID_SIZE )
		ri.Error( ERR_DROP, "Invalid RTCW mark grid dimensions" );
	for ( row = 0; row < grid->height - 1; ++row ) for ( column = 0; column < grid->width - 1; ++column ) {
		const drawVert_t *vertices = &grid->verts[row * grid->width + column];
		const int indices[2][3] = { { 0, grid->width, 1 }, { 1, grid->width, grid->width + 1 } };
		for ( half = 0; half < 2; ++half ) {
			vec3_t triangle[3], edge1, edge2, normal;
			if ( query->usedFragments >= query->maxFragments ) return;
			for ( j = 0; j < 3; ++j ) VectorCopy( vertices[indices[half][j]].xyz, triangle[j] );
			VectorSubtract( triangle[0], triangle[1], edge1 );
			VectorSubtract( triangle[2], triangle[1], edge2 );
			CrossProduct( edge1, edge2, normal );
			R_RTCWMarkNormalize( normal, normal );
			if ( DotProduct( normal, query->direction ) < (half ? -0.05f : -0.1f) )
				R_RTCWMarkTriangle( query, triangle, numPlanes, query->planes, query->distances, NULL, NULL, NULL );
		}
	}
}

int R_MarkRTCWFragments( int rotation, const vec3_t *corners, const vec3_t projection,
	int maxPoints, fnRTCWMarkPoint_t *points, int maxFragments, markFragment_t *fragments ) {
	rtcwMarkQuery_t query;
	vec3_t reverse;
	int i, j, gridPlanes = 6;
	if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP )
		ri.Error( ERR_DROP, "RTCW mark query requires an RTCW profile" );
	if ( maxPoints < 0 || maxFragments == INT_MIN ) ri.Error( ERR_DROP, "Invalid RTCW mark capacity" );
	if ( !maxPoints || !maxFragments || !tr.world || !tr.world->nodes || !tr.world->numnodes ) return 0;
	if ( !corners || !projection || !points || !fragments ) ri.Error( ERR_DROP, "Null RTCW mark buffer" );
	Com_Memset( &query, 0, sizeof( query ) );
	query.rotation = rotation;
	query.oldMapping = maxFragments < 0 ? qtrue : qfalse;
	query.maxFragments = maxFragments < 0 ? -maxFragments : maxFragments;
	query.maxPoints = maxPoints;
	query.points = points;
	query.fragments = fragments;
	query.remainingNodes = tr.world->numnodes;
	if ( !R_RTCWMarkFinite( projection ) ) ri.Error( ERR_DROP, "Invalid RTCW mark projection" );
	query.radius = 0.5f * R_RTCWMarkNormalize( projection, query.direction );
	if ( !query.radius ) return 0;
	ClearBounds( query.mins, query.maxs );
	for ( i = 0; i < 4; ++i ) {
		vec3_t forward, backward;
		if ( !R_RTCWMarkFinite( corners[i] ) ) ri.Error( ERR_DROP, "Invalid RTCW mark corner" );
		for ( j = 0; j < 3; ++j ) query.center[j] += corners[i][j] * 0.25f;
		VectorMA( corners[i], 1.0f + (query.oldMapping ? query.radius * 4.0f : 0), projection, forward );
		VectorMA( corners[i], -20.0f * (1.0f + (query.oldMapping ? query.radius / 20.0f * 4.0f : 0)), query.direction, backward );
		if ( !R_RTCWMarkFinite( forward ) || !R_RTCWMarkFinite( backward ) )
			ri.Error( ERR_DROP, "RTCW mark bounds overflow" );
		AddPointToBounds( corners[i], query.mins, query.maxs );
		AddPointToBounds( forward, query.mins, query.maxs );
		AddPointToBounds( backward, query.mins, query.maxs );
	}
	VectorNegate( projection, reverse );
	if ( !R_RTCWMarkPlanes( corners, reverse, query.planes, query.distances ) ) return 0;
	for ( i = 4; i < 6; ++i ) {
		VectorScale( query.direction, i == 4 ? 1.0f : -1.0f, query.planes[i] );
		query.distances[i] = DotProduct( query.planes[i], corners[0] ) - query.radius * (query.oldMapping ? 11.0f : 1.0f);
		if ( !FnRTCWMarkFinite( query.distances[i] ) ) ri.Error( ERR_DROP, "RTCW mark depth overflow" );
	}
	R_RTCWMarkCollect( &query, tr.world->nodes, 0 );
	for ( i = 0; i < query.numSurfaces && query.usedFragments < query.maxFragments; ++i ) {
		if ( *query.surfaces[i] == SF_FACE ) {
			if ( R_RTCWMarkFace( &query, (srfSurfaceFace_t *)query.surfaces[i] ) && !query.oldMapping )
				gridPlanes = 4; // Original RTCW retains this count for subsequent curves.
		} else R_RTCWMarkGrid( &query, (srfGridMesh_t *)query.surfaces[i], gridPlanes );
	}
	return query.usedFragments;
}

#endif
