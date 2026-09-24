/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_PARTICLE_SCENE_H
#define FNTECH3_TR_Q3_IHV_PARTICLE_SCENE_H

/* One special polygon stores points, not a fan. Existing frame-owned polygon
 * storage preserves queued scenes until the backend consumes the frame. Each
 * view expands its own triangles, including mirrors and screenshot cubemaps. */
qboolean RE_AddIHVParticlesToScene( int count, const fnQ3IHVParticle_t *particles ) {
	srfPoly_t *poly;
	int i, axis;
	if ( ri.gameId != COM_GAME_QUAKE3_IHV ) {
		ri.Error( ERR_DROP, "IHV particles submitted outside their game profile" );
		return qfalse;
	}
	if ( !tr.registered || count < 0 || ( count && !particles ) ) return qfalse;
	if ( !count ) return qtrue;
	if ( !tr.ihvParticleShader || r_numpolys >= max_polys ||
		r_numpolyverts > max_polyverts || count > max_polyverts - r_numpolyverts ) return qfalse;
	for ( i = 0; i < count; ++i ) for ( axis = 0; axis < 3; ++axis )
		if ( !Q3IHV_ParticleFinite( &particles[i].origin[axis] ) ) return qfalse;
	poly = &backEndData->polys[r_numpolys];
	poly->surfaceType = SF_IHV_PARTICLES;
	poly->hShader = tr.ihvParticleShader->index;
	poly->numVerts = count;
	poly->verts = &backEndData->polyVerts[r_numpolyverts];
	poly->fogIndex = 0;
	for ( i = 0; i < count; ++i ) {
		memcpy( poly->verts[i].xyz, particles[i].origin, sizeof( particles[i].origin ) );
		memcpy( poly->verts[i].modulate.rgba, particles[i].rgba, sizeof( particles[i].rgba ) );
		poly->verts[i].st[0] = poly->verts[i].st[1] = 0;
	}
	r_numpolyverts += count;
	++r_numpolys;
	return qtrue;
}

#endif
