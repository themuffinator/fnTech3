/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_DECAL_SURFACE_H
#define FNTECH3_TR_WOLFET_DECAL_SURFACE_H

/* An Enemy Territory decal is drawn as the fan ET draws it: its clipped
 * polygon, textured by the projector and coloured by its fade. */
static void RB_SurfaceWolfETDecal( void *data ) {
	const srfWolfETDecal_t *srf = (const srfWolfETDecal_t *)data;
	vec3_t normal;
	int i, numv;

	if ( srf->numVerts < 3 ) return;
#ifdef USE_VBO
	VBO_Flush();
	tess.surfType = SF_WOLFET_DECAL;
#endif
	RB_CHECKOVERFLOW( srf->numVerts, 3 * ( srf->numVerts - 2 ) );

	VectorSet( normal, 0.0f, 0.0f, 1.0f );
#ifdef USE_TESS_NEEDS_NORMAL
	if ( tess.needsNormal )
#endif
	{
		vec3_t edge0, edge1;
		VectorSubtract( srf->verts[1].xyz, srf->verts[0].xyz, edge0 );
		VectorSubtract( srf->verts[2].xyz, srf->verts[0].xyz, edge1 );
		CrossProduct( edge0, edge1, normal );
		if ( VectorNormalize( normal ) == 0.0f ) VectorSet( normal, 0.0f, 0.0f, 1.0f );
	}

	numv = tess.numVertexes;
	for ( i = 0; i < srf->numVerts; i++, numv++ ) {
		VectorCopy( srf->verts[i].xyz, tess.xyz[numv] );
		VectorCopy( normal, tess.normal[numv] );
		tess.texCoords[0][numv][0] = srf->verts[i].st[0];
		tess.texCoords[0][numv][1] = srf->verts[i].st[1];
		memcpy( tess.vertexColors[numv].rgba, srf->verts[i].modulate, 4 );
	}
	for ( i = 0; i < srf->numVerts - 2; i++ ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}
	tess.numVertexes = numv;
}

#endif
