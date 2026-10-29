/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MOHAA_RAY_GEOMETRY_H
#define FNTECH3_TR_MOHAA_RAY_GEOMETRY_H

/* Included by the RTX transport after its checked geometry/material helpers.
 *
 * Medal of Honor's TIKI models are posed and skinned by the engine
 * (tr_mohaa_render.h), and the trace takes each surface as
 * RB_SurfaceMOHAATiki draws it: the pose its entity's views computed this
 * frame, or one computed now for an entity no view drew but whose shadow or
 * reflection the trace can still see, skinned at the detail the main view
 * measured (whole for an entity it did not measure). The traced shape is
 * then the shape the base pass writes depth for. Each surface carries the
 * shader R_AddMOHAATikiSurfaces draws it with; a hidden surface, one the
 * definition marks nodraw, and one too small on screen to draw are left
 * out, as raster leaves them. An entity that layers a second pass over a
 * surface is not traced at all (R_MOHAATraceable). */

static float (*s_rtMohaaXyz)[4];
static float (*s_rtMohaaNormal)[4];
static float (*s_rtMohaaSt)[2];
static unsigned int *s_rtMohaaIndexes;
static uint32_t s_rtMohaaScratchVerts, s_rtMohaaScratchIndexes;

static void RT_MOHAAScratchFree( void ) {
	RT_HostFree( s_rtMohaaXyz );
	RT_HostFree( s_rtMohaaIndexes );
	s_rtMohaaXyz = NULL;
	s_rtMohaaNormal = NULL;
	s_rtMohaaSt = NULL;
	s_rtMohaaIndexes = NULL;
	s_rtMohaaScratchVerts = s_rtMohaaScratchIndexes = 0;
}

/* Room for a whole surface, which the engine's skinning requires. The
 * positions, normals and texture coordinates share one block. */
static qboolean RT_MOHAAScratch( uint32_t verts, uint32_t indexes ) {
	uint32_t capacity;
	size_t bytes;
	if ( verts > s_rtMohaaScratchVerts ) {
		byte *block;
		if ( !vk_rt_cpu_array_growth( s_rtMohaaScratchVerts, verts, 1024u,
			sizeof( float[4] ) * 2 + sizeof( float[2] ), "Medal of Honor skinning", &capacity, &bytes ) ) return qfalse;
		block = (byte *)RT_HostAlloc( bytes );
		if ( !block ) return qfalse;
		RT_HostFree( s_rtMohaaXyz );
		s_rtMohaaXyz = (float (*)[4])block;
		s_rtMohaaNormal = (float (*)[4])( block + (size_t)capacity * sizeof( float[4] ) );
		s_rtMohaaSt = (float (*)[2])( block + (size_t)capacity * sizeof( float[4] ) * 2 );
		s_rtMohaaScratchVerts = capacity;
	}
	if ( indexes > s_rtMohaaScratchIndexes ) {
		unsigned int *block;
		if ( !vk_rt_cpu_array_growth( s_rtMohaaScratchIndexes, indexes, 3072u,
			sizeof( unsigned int ), "Medal of Honor skinning", &capacity, &bytes ) ) return qfalse;
		block = (unsigned int *)RT_HostAlloc( bytes );
		if ( !block ) return qfalse;
		RT_HostFree( s_rtMohaaIndexes );
		s_rtMohaaIndexes = block;
		s_rtMohaaScratchIndexes = capacity;
	}
	return qtrue;
}

static qboolean vk_rt_append_mohaa_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent ) {
	const model_t *model = R_GetModelByHandle( ent->e.hModel );
	const float *bones;
	int i;
	if ( !model || model->type != MOD_MOHAA_TIKI || !model->mohaaTiki || !ri.MOHAARender ) return qtrue;
	/* An entity the engine cannot pose has no geometry to trace, as it has
	 * none to draw. */
	bones = R_MOHAATracePose( ent, model );
	if ( !bones ) return qtrue;
	for ( i = 0; i < model->numMohaaSurfaces; ++i ) {
		const mohaaSurface_t *surface = model->mohaaSurfaces + i;
		const shader_t *shader = R_MOHAASurfaceShader( ent, surface, NULL, NULL );
		vec4_t tangent = { 1, 0, 0, 1 };
		uint32_t material, base, valid = 0;
		int verts, indexCount = 0, v, t;
		if ( !shader ) continue;
		if ( !RT_MOHAAScratch( (uint32_t)surface->numVerts, (uint32_t)surface->numIndexes ) ) return qfalse;
		verts = R_MOHAASkinTraceSurface( ent, bones, surface, s_rtMohaaXyz, s_rtMohaaNormal, s_rtMohaaSt,
			s_rtMohaaIndexes, (int)s_rtMohaaScratchVerts, (int)s_rtMohaaScratchIndexes, &indexCount );
		if ( verts <= 0 || indexCount < 3 ) continue;
		if ( !vk_rt_cpu_geometry_find_or_add_material( geometry, shader, &material ) ||
			!vk_rt_cpu_geometry_reserve( geometry, (uint32_t)verts, (uint32_t)indexCount ) ) return qfalse;
		base = geometry->numVertices;
		for ( v = 0; v < verts; ++v ) {
			vec3_t world, worldNormal;
			vk_rt_transform_point( ent, s_rtMohaaXyz[v], world );
			vk_rt_transform_direction( ent, s_rtMohaaNormal[v], worldNormal );
			if ( !vk_rt_cpu_geometry_add_vertex( geometry, world, worldNormal, tangent, s_rtMohaaSt[v],
				0xffffffffu, material, NULL ) ) return qfalse;
		}
		for ( t = 0; t + 2 < indexCount; t += 3 ) {
			const unsigned int *tri = s_rtMohaaIndexes + t;
			if ( tri[0] >= (unsigned int)verts || tri[1] >= (unsigned int)verts || tri[2] >= (unsigned int)verts ) continue;
			if ( !vk_rt_cpu_geometry_add_triangle( geometry, base + tri[0], base + tri[1], base + tri[2] ) ) return qfalse;
			++valid;
		}
		geometry->sourceTriangleCount += valid;
		if ( geometry->materials[material].flags & RTX_RT_MATFLAG_MASKED ) geometry->maskedTriangleCount += valid;
		++geometry->sourceEntityCount;
	}
	return qtrue;
}

#endif
