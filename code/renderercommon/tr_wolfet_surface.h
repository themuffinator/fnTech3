/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_SURFACE_H
#define FNTECH3_TR_WOLFET_SURFACE_H

#include "tr_wolfet_entity.h"

/* Detail is measured against the skeleton the current frame came from, not the
 * mesh, because an ET mesh carries no frames of its own. */
static inline float RB_WolfETLodFraction( const wolfetMeshModel_t *model, const refEntity_t *entity ) {
	const md3Frame_t *frame = R_WolfETEntityFrame( entity->wolfet.frameModel, entity->frame );
	const viewParms_t *view = &backEnd.viewParms;
	vec3_t location;
	float distance, projected = 0;
#ifdef USE_PMLIGHT
	/* Cube faces must see the same silhouette, regardless of their view axis. */
	if ( view->passFlags & VPF_DLIGHT_SHADOW ) return 1.0f;
#endif
	if ( !frame ) return 1.0f;
	VectorAdd( entity->origin, frame->localOrigin, location );
	distance = DotProduct( view->or.axis[0], location ) - DotProduct( view->or.axis[0], view->or.origin );
	if ( distance > 0 ) {
		const float radius = fabsf( frame->radius );
		const float y = radius * view->projectionMatrix[5] + -distance * view->projectionMatrix[9] + view->projectionMatrix[13];
		const float w = radius * view->projectionMatrix[7] + -distance * view->projectionMatrix[11] + view->projectionMatrix[15];
		projected = y / w;
		if ( projected > 1 ) projected = 1;
	}
	return FnWolfET_LodFraction( (unsigned)entity->wolfet.reFlags, projected,
		r_lodscale->value, r_lodbias->value, model->view.lodScale, model->view.lodBias );
}

static void RB_SurfaceWolfET( void *data ) {
	const wolfetMeshSurface_t *mesh = (const wolfetMeshSurface_t *)data;
	const fnWolfETMeshSurfaceView_t *surface = &mesh->view;
	const refEntity_t *entity = &backEnd.currentEntity->e;
	fnWolfETPose_t pose;
	uint32_t collapse[SHADER_MAX_VERTEXES], retained, vertex, triangle, indices = 0;
	const int dead = ( entity->wolfet.reFlags & FN_WOLFET_REFLAG_DEAD_LOD ) != 0;
	int base;
	if ( !surface->numVerts || !surface->numTriangles ) return;
	if ( !R_WolfETEntityPose( entity, &pose ) ) {
		ri.Error( ERR_DROP, "Invalid WolfET animation state" );
		return;
	}
	retained = FnWolfET_LodVertexCount( surface, RB_WolfETLodFraction( mesh->model, entity ), dead );
	if ( !retained ) return;
	if ( !FnWolfET_CollapseVertices( surface, retained, collapse, ARRAY_LEN( collapse ) ) ) {
		ri.Error( ERR_DROP, "Invalid WolfET collapse map" );
		return;
	}
#ifdef USE_VBO
	VBO_Flush();
#endif
	RB_CHECKOVERFLOW( (int)retained, (int)surface->numTriangles * 3 );
	base = tess.numVertexes;
#ifdef USE_VBO
	tess.surfType = SF_WOLFET;
#endif
	for ( vertex = 0; vertex < retained; ++vertex ) {
		if ( !FnWolfET_SkinVertex( &mesh->vertices[vertex], &pose,
			tess.xyz[base + vertex], tess.normal[base + vertex] ) ) {
			ri.Error( ERR_DROP, "Invalid WolfET vertex deformation" );
			return;
		}
		memcpy( tess.texCoords[0][base + vertex], mesh->vertices[vertex].st,
			sizeof( mesh->vertices[vertex].st ) );
	}
	for ( triangle = 0; triangle < surface->numTriangles; ++triangle ) {
		const byte *source = surface->data + surface->triangles + (size_t)triangle * 12;
		const uint32_t a = collapse[FnWolfET_U32( source )];
		const uint32_t b = collapse[FnWolfET_U32( source + 4 )];
		const uint32_t c = collapse[FnWolfET_U32( source + 8 )];
		/* Authored degenerates survive at full detail, as in ET. */
		if ( retained != surface->numVerts && ( a == b || b == c || c == a ) ) continue;
		tess.indexes[tess.numIndexes + indices++] = base + a;
		tess.indexes[tess.numIndexes + indices++] = base + b;
		tess.indexes[tess.numIndexes + indices++] = base + c;
	}
	tess.numIndexes += (int)indices;
	tess.numVertexes += (int)retained;
}

#endif
