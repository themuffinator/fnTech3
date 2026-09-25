/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDC_RAY_GEOMETRY_H
#define FNTECH3_TR_MDC_RAY_GEOMETRY_H

/* Included by the RTX transport after its checked geometry/material helpers. */
#include "tr_mesh_material.h"

static const shader_t *vk_rt_resolve_mdc_shader( const trRefEntity_t *ent, const mdcMeshSurface_t *surface ) {
	if ( ent->e.customShader ) return R_GetShaderByHandle( ent->e.customShader );
	if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) {
		const skin_t *skin = R_GetSkinByHandle( ent->e.customSkin );
		return R_MeshSkinShader( &ent->e, skin, surface->name );
	}
	if ( surface->view.numShaders ) {
		const unsigned index = ent->e.skinNum < 0 ? 0 : (unsigned)ent->e.skinNum % surface->view.numShaders;
		return R_GetShaderByHandle( surface->shaders[index] );
	}
	return tr.defaultShader;
}

static qboolean vk_rt_append_mdc_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent ) {
	const model_t *model = R_GetModelByHandle( ent->e.hModel );
	const mdcModel_t *mdc;
	int frame = ent->e.frame, oldFrame = ent->e.oldframe;
	float backlerp;
	uint32_t i;
	if ( !model || model->type != MOD_MDC ) return qtrue;
	mdc = model->mdc[0]; // Stable highest-detail caster geometry, independent of the camera.
	if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		frame %= (int)mdc->view.numFrames;
		oldFrame %= (int)mdc->view.numFrames;
	}
	if ( frame < 0 || oldFrame < 0 || (uint32_t)frame >= mdc->view.numFrames || (uint32_t)oldFrame >= mdc->view.numFrames )
		frame = oldFrame = 0;
	backlerp = frame == oldFrame ? 0 : ent->e.backlerp;
	if ( !isfinite( backlerp ) ) return qfalse;
	for ( i = 0; i < mdc->view.numSurfaces; ++i ) {
		const mdcMeshSurface_t *mesh = mdc->surfaces + i;
		const fnMdcSurfaceView_t *surface = &mesh->view;
		const shader_t *shader = vk_rt_resolve_mdc_shader( ent, mesh );
		uint32_t material, base, vertex, triangle, indexCount;
		vec4_t tangent = { 1, 0, 0, 1 };
		if ( !surface->numVerts || !surface->numTriangles || R_MeshSurfaceHidden( &ent->e, mesh->name ) ) continue;
		if ( !vk_rt_checked_u32_multiply( surface->numTriangles, 3, &indexCount ) ||
			!vk_rt_cpu_geometry_find_or_add_material( geometry, shader, &material ) ||
			!vk_rt_cpu_geometry_reserve( geometry, surface->numVerts, indexCount ) ) return qfalse;
		base = geometry->numVertices;
		for ( vertex = 0; vertex < surface->numVerts; ++vertex ) {
			vec3_t local, normal, world, worldNormal;
			vec2_t uv;
			const byte *st = surface->data + surface->st + (size_t)vertex * 8;
			if ( !FnMdc_AnimatedVertex( surface, &r_mdcNormals, (uint32_t)oldFrame, (uint32_t)frame,
				backlerp, vertex, local, normal ) ) return qfalse;
			vk_rt_transform_point( ent, local, world );
			vk_rt_transform_direction( ent, normal, worldNormal );
			uv[0] = FnMdc_Float( st ); uv[1] = FnMdc_Float( st + 4 );
			if ( !vk_rt_cpu_geometry_add_vertex( geometry, world, worldNormal, tangent, uv, 0xffffffffu, material, NULL ) )
				return qfalse;
		}
		for ( triangle = 0; triangle < surface->numTriangles; ++triangle ) {
			const byte *indices = surface->data + surface->triangles + (size_t)triangle * 12;
			if ( !vk_rt_cpu_geometry_add_triangle( geometry, base + FnMdc_U32( indices ),
				base + FnMdc_U32( indices + 4 ), base + FnMdc_U32( indices + 8 ) ) ) return qfalse;
		}
		geometry->sourceTriangleCount += surface->numTriangles;
		if ( geometry->materials[material].flags & RTX_RT_MATFLAG_MASKED ) geometry->maskedTriangleCount += surface->numTriangles;
		++geometry->sourceEntityCount;
	}
	return qtrue;
}

#endif
