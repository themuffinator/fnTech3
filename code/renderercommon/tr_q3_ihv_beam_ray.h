/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_BEAM_RAY_H
#define FNTECH3_TR_Q3_IHV_BEAM_RAY_H

#include "tr_q3_ihv_beam.h"

static qboolean vk_rt_append_ihv_beam_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *entity ) {
	float vertices[12][3];
	uint32_t materialIndex = 0, base;
	vec4_t tangent = { 1, 0, 0, 1 };
	vec2_t uv = { 0, 0 };
	const byte rgba[4] = { 255, 0, 0, 255 };
	int i;
	if ( !Q3IHV_BeamVertices( entity->e.origin, entity->e.oldorigin, vertices ) ) return qtrue;
	if ( !vk_rt_cpu_geometry_find_or_add_material( geometry, tr.ihvBeamShader, &materialIndex ) ||
		!vk_rt_cpu_geometry_reserve( geometry, 12, 36 ) ) return qfalse;
	vk_rt_apply_effect_material_tuning( geometry, materialIndex, qtrue, qfalse );
	base = geometry->numVertices;
	for ( i = 0; i < 12; ++i ) {
		vec3_t world, normal, localNormal;
		VectorScale( vertices[i & ~1], .25f, localNormal );
		vk_rt_transform_point( entity, vertices[i], world );
		vk_rt_transform_direction( entity, localNormal, normal );
		if ( !vk_rt_cpu_geometry_add_vertex( geometry, world, normal, tangent, uv,
			vk_rt_color_u32_from_bytes( rgba ), materialIndex, NULL ) ) return qfalse;
	}
	for ( i = 0; i < 6; ++i ) {
		const uint32_t a = base + i * 2, b = a + 1, c = base + ( ( i + 1 ) % 6 ) * 2, d = c + 1;
		if ( !vk_rt_cpu_geometry_add_triangle( geometry, a, b, c ) ||
			!vk_rt_cpu_geometry_add_triangle( geometry, c, b, d ) ) return qfalse;
	}
	geometry->sourceTriangleCount += 12;
	if ( geometry->materials[materialIndex].flags & RTX_RT_MATFLAG_MASKED ) geometry->maskedTriangleCount += 12;
	++geometry->sourceEffectCount;
	return qtrue;
}

#endif
