/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_SURFACE_H
#define FNTECH3_TR_MDS_SURFACE_H

#include "tr_mds_entity.h"
#include "tr_mds_lod.h"

static float RB_MdsLodFraction( const mdsModel_t *model, const refEntity_t *entity ) {
	const md3Frame_t *frame = &model->frames[entity->frame];
	const viewParms_t *view = &backEnd.viewParms;
	vec3_t location;
	float distance, projected = 0;
	VectorAdd( entity->origin, frame->localOrigin, location );
	distance = DotProduct( view->or.axis[0], location ) - DotProduct( view->or.axis[0], view->or.origin );
	if ( distance > 0 ) {
		const float radius = fabsf( frame->radius );
		const float y = radius * view->projectionMatrix[5] + -distance * view->projectionMatrix[9] + view->projectionMatrix[13];
		const float w = radius * view->projectionMatrix[7] + -distance * view->projectionMatrix[11] + view->projectionMatrix[15];
		projected = y / w;
		if ( projected > 1 ) projected = 1;
	}
	return FnMds_LodFraction( ri.gameId, (unsigned)entity->rtcw.reFlags, projected,
		r_lodscale->value, r_lodbias->value, model->view.lodScale, model->view.lodBias );
}

static void RB_SurfaceMDS( void *data ) {
	const mdsMeshSurface_t *mesh = (const mdsMeshSurface_t *)data;
	const fnMdsSurfaceView_t *surface = &mesh->view;
	const refEntity_t *entity = &backEnd.currentEntity->e;
	fnMdsPose_t pose;
	uint32_t collapse[SHADER_MAX_VERTEXES], retained, vertex, triangle, indices = 0;
	const int dead = ( ri.gameId == COM_GAME_RTCW_SP || ri.gameId == COM_GAME_RTCW_MP ) && ( entity->rtcw.reFlags & 32 );
	int base;
	if ( !surface->numVerts || !surface->numTriangles ) return;
	if ( !R_MdsEntityPose( &mesh->model->view, entity, ri.gameId, &pose ) ) {
		ri.Error( ERR_DROP, "Invalid MDS animation state" );
		return;
	}
	retained = FnMds_LodVertexCount( surface, RB_MdsLodFraction( mesh->model, entity ), dead );
	if ( !retained ) return;
	if ( !FnMds_CollapseVertices( surface, retained, collapse, ARRAY_LEN( collapse ) ) ) {
		ri.Error( ERR_DROP, "Invalid MDS collapse map" );
		return;
	}
#ifdef USE_VBO
	VBO_Flush();
#endif
	RB_CHECKOVERFLOW( (int)retained, (int)surface->numTriangles * 3 );
	base = tess.numVertexes;
#ifdef USE_VBO
	tess.surfType = SF_MDS;
#endif
	for ( vertex = 0; vertex < retained; ++vertex ) {
		if ( !FnMds_SkinVertex( &mesh->vertices[vertex], &pose, tess.xyz[base + vertex], tess.normal[base + vertex] ) ) {
			ri.Error( ERR_DROP, "Invalid MDS vertex deformation" );
			return;
		}
		memcpy( tess.texCoords[0][base + vertex], mesh->vertices[vertex].st, sizeof( mesh->vertices[vertex].st ) );
	}
	for ( triangle = 0; triangle < surface->numTriangles; ++triangle ) {
		const byte *source = surface->data + surface->triangles + (size_t)triangle * 12;
		const uint32_t a = collapse[FnMds_U32( source )], b = collapse[FnMds_U32( source + 4 )], c = collapse[FnMds_U32( source + 8 )];
		/* Preserve authored degenerates at full detail, as in RTCW. */
		if ( retained != surface->numVerts && ( a == b || b == c || c == a ) ) continue;
		tess.indexes[tess.numIndexes + indices++] = base + a;
		tess.indexes[tess.numIndexes + indices++] = base + b;
		tess.indexes[tess.numIndexes + indices++] = base + c;
	}
	tess.numIndexes += (int)indices;
	tess.numVertexes += (int)retained;
}

#endif
