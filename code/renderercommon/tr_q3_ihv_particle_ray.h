/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_PARTICLE_RAY_H
#define FNTECH3_TR_Q3_IHV_PARTICLE_RAY_H

/* Feed independent view-facing triangles through RTX's existing material and
 * geometry path. Treating the stored particle points as a fan joins unrelated
 * particles and gives different silhouettes from the raster overlay. */
static qboolean vk_rt_append_ihv_particle_geometry( rtxRtCpuGeometry_t *geometry, const srfPoly_t *points ) {
	fnQ3IHVParticleView_t view;
	srfPoly_t triangle = *points;
	polyVert_t vertices[3];
	int i, vertex;
	if ( ri.gameId != COM_GAME_QUAKE3_IHV ||
		!Q3IHV_ParticleView( backEnd.viewParms.or.origin, backEnd.viewParms.or.axis,
			R_ViewPassIsMirror( &backEnd.viewParms ), &view ) ) return qfalse;
	triangle.surfaceType = SF_POLY;
	triangle.numVerts = 3;
	triangle.verts = vertices;
	for ( i = 0; i < points->numVerts; ++i ) {
		float xyz[3][3];
		if ( !Q3IHV_ParticleTriangle( &view, points->verts[i].xyz, xyz ) ) continue;
		for ( vertex = 0; vertex < 3; ++vertex ) {
			memcpy( vertices[vertex].xyz, xyz[vertex], sizeof( xyz[vertex] ) );
			vertices[vertex].st[0] = vertex == 1 ? 1.125f : 0;
			vertices[vertex].st[1] = vertex == 2 ? 1.125f : 0;
			vertices[vertex].modulate = points->verts[i].modulate;
		}
		if ( !vk_rt_append_poly_geometry( geometry, &triangle ) ) return qfalse;
	}
	return qtrue;
}

#endif
