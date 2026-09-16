/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDC_SURFACE_H
#define FNTECH3_TR_MDC_SURFACE_H

static void RB_SurfaceMDC( void *data ) {
	const mdcMeshSurface_t *mesh = (const mdcMeshSurface_t *)data;
	const fnMdcSurfaceView_t *surface = &mesh->view;
	const refEntity_t *entity = &backEnd.currentEntity->e;
	const float backlerp = entity->oldframe == entity->frame ? 0 : entity->backlerp;
	uint32_t vertex, index;
	int base;
	if ( !surface->numVerts || !surface->numTriangles ) return;
#ifdef USE_VBO
	VBO_Flush();
#endif
	RB_CHECKOVERFLOW( (int)surface->numVerts, (int)surface->numTriangles * 3 );
	base = tess.numVertexes;
#ifdef USE_VBO
	tess.surfType = SF_MDC;
#endif
	for ( vertex = 0; vertex < surface->numVerts; ++vertex ) {
		const byte *st = surface->data + surface->st + (size_t)vertex * 8;
		if ( !FnMdc_AnimatedVertex( surface, &r_mdcNormals, (uint32_t)entity->oldframe,
			(uint32_t)entity->frame, backlerp, vertex, tess.xyz[base + vertex], tess.normal[base + vertex] ) ) {
			ri.Error( ERR_DROP, "Invalid MDC animation state" );
			return;
		}
		tess.texCoords[0][base + vertex][0] = FnMdc_Float( st );
		tess.texCoords[0][base + vertex][1] = FnMdc_Float( st + 4 );
	}
	for ( index = 0; index < surface->numTriangles * 3; ++index )
		tess.indexes[tess.numIndexes + index] = base + FnMdc_U32( surface->data + surface->triangles + (size_t)index * 4 );
	tess.numIndexes += (int)surface->numTriangles * 3;
	tess.numVertexes += (int)surface->numVerts;
}

#endif
