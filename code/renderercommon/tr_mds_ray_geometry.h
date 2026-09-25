/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_RAY_GEOMETRY_H
#define FNTECH3_TR_MDS_RAY_GEOMETRY_H

/* Included by the RTX transport after its checked geometry/material helpers. */
#include "tr_mds_entity.h"
#include "tr_mds_material.h"

static qboolean vk_rt_append_mds_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent ) {
	const model_t *model = R_GetModelByHandle( ent->e.hModel );
	const mdsModel_t *mds;
	refEntity_t entity = ent->e;
	fnMdsPose_t pose;
	uint32_t i;
	if ( !model || model->type != MOD_MDS ) return qtrue;
	mds = model->mds; // Full detail caster geometry is independent of the camera.
	R_MdsSanitizeFrames( &mds->view, &entity, ri.gameId );
	if ( !R_MdsEntityPose( &mds->view, &entity, ri.gameId, &pose ) ) return qfalse;
	for ( i = 0; i < mds->view.numSurfaces; ++i ) {
		const mdsMeshSurface_t *mesh = mds->surfaces + i;
		const fnMdsSurfaceView_t *surface = &mesh->view;
		const shader_t *shader;
		uint32_t material, base, vertex, triangle, indexCount;
		vec4_t tangent = { 1, 0, 0, 1 };
		if ( !surface->numVerts || !surface->numTriangles || R_MeshSurfaceHidden( &entity, mesh->name ) ) continue;
		shader = R_MdsSurfaceShader( &entity, mesh );
		if ( !vk_rt_checked_u32_multiply( surface->numTriangles, 3, &indexCount ) ||
			!vk_rt_cpu_geometry_find_or_add_material( geometry, shader, &material ) ||
			!vk_rt_cpu_geometry_reserve( geometry, surface->numVerts, indexCount ) ) return qfalse;
		base = geometry->numVertices;
		for ( vertex = 0; vertex < surface->numVerts; ++vertex ) {
			vec3_t local, normal, world, worldNormal;
			vec2_t uv;
			if ( !FnMds_SkinVertex( &mesh->vertices[vertex], &pose, local, normal ) ) return qfalse;
			vk_rt_transform_point( ent, local, world );
			vk_rt_transform_direction( ent, normal, worldNormal );
			memcpy( uv, mesh->vertices[vertex].st, sizeof( uv ) );
			if ( !vk_rt_cpu_geometry_add_vertex( geometry, world, worldNormal, tangent, uv, 0xffffffffu, material, NULL ) )
				return qfalse;
		}
		for ( triangle = 0; triangle < surface->numTriangles; ++triangle ) {
			const byte *indices = surface->data + surface->triangles + (size_t)triangle * 12;
			if ( !vk_rt_cpu_geometry_add_triangle( geometry, base + FnMds_U32( indices ),
				base + FnMds_U32( indices + 4 ), base + FnMds_U32( indices + 8 ) ) ) return qfalse;
		}
		geometry->sourceTriangleCount += surface->numTriangles;
		if ( geometry->materials[material].flags & RTX_RT_MATFLAG_MASKED ) geometry->maskedTriangleCount += surface->numTriangles;
		++geometry->sourceEntityCount;
	}
	return qtrue;
}

#endif
