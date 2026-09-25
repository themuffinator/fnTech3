/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_RAY_GEOMETRY_H
#define FNTECH3_TR_WOLFET_RAY_GEOMETRY_H

/* Included by the RTX transport after its checked geometry/material helpers. */
#include "tr_wolfet_entity.h"
#include "tr_wolfet_material.h"

static qboolean vk_rt_append_wolfet_entity_geometry( rtxRtCpuGeometry_t *geometry,
	const trRefEntity_t *ent ) {
	const model_t *model = R_GetModelByHandle( ent->e.hModel );
	const wolfetMeshModel_t *mesh;
	fnWolfETPose_t pose;
	uint32_t i;
	if ( !model || model->type != MOD_WOLFET_MESH || !model->wolfetMesh ) return qtrue;
	mesh = model->wolfetMesh;
	/* Full detail caster geometry is independent of the camera, so no collapse
	 * map is applied here. An entity whose skeletons cannot be resolved has no
	 * geometry to trace rather than a wrong one. */
	if ( !R_WolfETEntityPose( &ent->e, &pose ) ) return qtrue;
	for ( i = 0; i < mesh->view.numSurfaces; ++i ) {
		const wolfetMeshSurface_t *surface = mesh->surfaces + i;
		const fnWolfETMeshSurfaceView_t *view = &surface->view;
		const shader_t *shader;
		uint32_t material, base, vertex, triangle, indexCount;
		vec4_t tangent = { 1, 0, 0, 1 };
		if ( !view->numVerts || !view->numTriangles ) continue;
		shader = R_WolfETSurfaceShader( &ent->e, surface );
		if ( !vk_rt_checked_u32_multiply( view->numTriangles, 3, &indexCount ) ||
			!vk_rt_cpu_geometry_find_or_add_material( geometry, shader, &material ) ||
			!vk_rt_cpu_geometry_reserve( geometry, view->numVerts, indexCount ) ) return qfalse;
		base = geometry->numVertices;
		for ( vertex = 0; vertex < view->numVerts; ++vertex ) {
			vec3_t local, normal, world, worldNormal;
			vec2_t uv;
			if ( !FnWolfET_SkinVertex( &surface->vertices[vertex], &pose, local, normal ) ) return qfalse;
			vk_rt_transform_point( ent, local, world );
			vk_rt_transform_direction( ent, normal, worldNormal );
			memcpy( uv, surface->vertices[vertex].st, sizeof( uv ) );
			if ( !vk_rt_cpu_geometry_add_vertex( geometry, world, worldNormal, tangent, uv,
				0xffffffffu, material, NULL ) ) return qfalse;
		}
		for ( triangle = 0; triangle < view->numTriangles; ++triangle ) {
			const byte *indices = view->data + view->triangles + (size_t)triangle * 12;
			if ( !vk_rt_cpu_geometry_add_triangle( geometry, base + FnWolfET_U32( indices ),
				base + FnWolfET_U32( indices + 4 ), base + FnWolfET_U32( indices + 8 ) ) ) return qfalse;
		}
		geometry->sourceTriangleCount += view->numTriangles;
		if ( geometry->materials[material].flags & RTX_RT_MATFLAG_MASKED )
			geometry->maskedTriangleCount += view->numTriangles;
		++geometry->sourceEntityCount;
	}
	return qtrue;
}

#endif
