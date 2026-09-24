/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_PARTICLE_SURFACE_H
#define FNTECH3_TR_Q3_IHV_PARTICLE_SURFACE_H

static void RB_SurfaceIHVParticles( const srfPoly_t *particles ) {
	fnQ3IHVParticleView_t view;
	int i, vertex, base;
	if ( ri.gameId != COM_GAME_QUAKE3_IHV || !particles || !particles->verts || particles->numVerts <= 0 ) return;
	if ( !Q3IHV_ParticleView( backEnd.viewParms.or.origin, backEnd.viewParms.or.axis,
		R_ViewPassIsMirror( &backEnd.viewParms ), &view ) ) return;
#ifdef USE_VBO
	VBO_Flush();
#endif
	for ( i = 0; i < particles->numVerts; ++i ) {
		float xyz[3][3];
		if ( !Q3IHV_ParticleTriangle( &view, particles->verts[i].xyz, xyz ) ) continue;
		RB_CHECKOVERFLOW( 3, 3 );
#ifdef RENDERER_GLX
		GLX_CompatMarkDynamicCategory( GLX_DYNAMIC_CATEGORY_MASK_PARTICLE );
#endif
#if defined(USE_VBO) || defined(RENDERER_GLX)
		tess.surfType = SF_IHV_PARTICLES;
#endif
		base = tess.numVertexes;
		for ( vertex = 0; vertex < 3; ++vertex ) {
			memcpy( tess.xyz[base + vertex], xyz[vertex], sizeof( xyz[vertex] ) );
			tess.xyz[base + vertex][3] = 1;
			VectorCopy( view.forward, tess.normal[base + vertex] );
			tess.texCoords[0][base + vertex][0] = vertex == 1 ? 1.125f : 0;
			tess.texCoords[0][base + vertex][1] = vertex == 2 ? 1.125f : 0;
			tess.vertexColors[base + vertex] = particles->verts[i].modulate;
			tess.indexes[tess.numIndexes + vertex] = base + vertex;
		}
		tess.numVertexes += 3;
		tess.numIndexes += 3;
	}
}

#endif
