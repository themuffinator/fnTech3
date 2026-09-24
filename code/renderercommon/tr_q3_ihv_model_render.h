/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_MODEL_RENDER_H
#define FNTECH3_TR_Q3_IHV_MODEL_RENDER_H

#include "tr_q3_ihv_model_surface.h"

static int RB_LerpIHVModel( const md3Surface_t *surface, float fraction ) {
	const refEntity_t *entity = &backEnd.currentEntity->e;
	float xyz[SHADER_MAX_VERTEXES][3], normal[SHADER_MAX_VERTEXES][3], move[3];
	int vertex;
	if ( !Q3IHV_EntityOriginLerp( entity->q3IHV.flags, entity->origin, entity->oldorigin,
		backEnd.or.axis, fraction, move ) ||
		!Q3IHV_ModelSurface( surface, entity->oldframe, entity->frame, fraction, move,
			tr.ihvModelSines, xyz, normal, SHADER_MAX_VERTEXES ) ) return 0;
	for ( vertex = 0; vertex < surface->numVerts; ++vertex ) {
		memcpy( tess.xyz[tess.numVertexes + vertex], xyz[vertex], sizeof( xyz[vertex] ) );
		memcpy( tess.normal[tess.numVertexes + vertex], normal[vertex], sizeof( normal[vertex] ) );
	}
	return 1;
}

#endif
