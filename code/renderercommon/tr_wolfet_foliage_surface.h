/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_FOLIAGE_SURFACE_H
#define FNTECH3_TR_WOLFET_FOLIAGE_SURFACE_H

/* Draw every visible instance of an Enemy Territory foliage surface as its own
 * copy of the mesh. With a distancecull on the shader, an instance's colour
 * carries its fade in alpha; without one, its colour is drawn as stored. */
static void RB_SurfaceWolfETFoliage( void *data ) {
	const srfWolfETFoliage_t *srf = (const srfWolfETFoliage_t *)data;
	const float *cull = tess.shader->wolfetDistanceCull;
	const int active = FnWolfET_DistanceCullActive( cull );
	const float fovX = backEnd.viewParms.fovX;
	float axis[4], farNormal[3], farDist;
	vec3_t local;
	int o, i;
#ifdef USE_LEGACY_DLIGHTS
	const int dlightBits = srf->dlightBits;
#endif

	if ( !srf->numInstances ) return;

	/* The view's forward axis in the surface's space, and the depth of that
	 * space's origin. */
	axis[0] = -backEnd.or.modelMatrix[2];
	axis[1] = -backEnd.or.modelMatrix[6];
	axis[2] = -backEnd.or.modelMatrix[10];
	VectorSubtract( backEnd.or.origin, backEnd.viewParms.or.origin, local );
	axis[3] = DotProduct( local, backEnd.viewParms.or.axis[0] );

	if ( active && !FnWolfET_FoliageSurfaceVisible( cull,
		FnWolfET_FoliageDepth( axis, srf->origin, fovX ), srf->radius, fovX ) ) {
		return;
	}

	/* ET's fifth plane is the far plane. */
	VectorNegate( backEnd.viewParms.or.axis[0], farNormal );
	farDist = DotProduct( backEnd.viewParms.or.origin, farNormal ) - backEnd.viewParms.zFar;

#ifdef USE_VBO
	VBO_Flush();
	tess.surfType = SF_WOLFET_FOLIAGE;
#endif

	for ( o = 0; o < srf->numInstances; ++o ) {
		const wolfetFoliageInstance_t *instance = &srf->instances[o];
		color4ub_t color = instance->color;
		float *xyz, *normal, *texCoords0, *texCoords1;
		uint32_t *target;

		if ( active ) {
			const int alpha = FnWolfET_FoliageInstanceAlpha( cull,
				FnWolfET_FoliageDepth( axis, instance->origin, fovX ), (unsigned)o );
			if ( alpha < 0 ) continue;
			color.rgba[3] = (byte)alpha;
		}
		for ( i = 0; i < 4; ++i ) {
			if ( !FnWolfET_FoliagePlaneKeeps( backEnd.viewParms.frustum[i].normal,
				backEnd.viewParms.frustum[i].dist, instance->origin ) ) break;
		}
		if ( i != 4 || ( backEnd.viewParms.zFar > 0 &&
			!FnWolfET_FoliagePlaneKeeps( farNormal, farDist, instance->origin ) ) ) continue;

		RB_CHECKOVERFLOW( srf->numVerts, srf->numIndexes );
#ifdef USE_LEGACY_DLIGHTS
		tess.dlightBits |= dlightBits;
#endif
		for ( i = 0; i < srf->numIndexes; ++i ) {
			tess.indexes[tess.numIndexes + i] = tess.numVertexes + srf->indexes[i];
		}
		tess.numIndexes += srf->numIndexes;

		xyz = tess.xyz[tess.numVertexes];
		normal = tess.normal[tess.numVertexes];
		texCoords0 = tess.texCoords[0][tess.numVertexes];
		texCoords1 = tess.texCoords[1][tess.numVertexes];
		target = &tess.vertexColors[tess.numVertexes].u32;
		for ( i = 0; i < srf->numVerts; ++i, xyz += 4, normal += 4, texCoords0 += 2 ) {
			VectorAdd( srf->xyz[i], instance->origin, xyz );
#ifdef USE_TESS_NEEDS_NORMAL
			if ( tess.needsNormal )
#endif
			{
				VectorCopy( srf->normal[i], normal );
			}
			texCoords0[0] = srf->texCoords[i][0];
			texCoords0[1] = srf->texCoords[i][1];
#ifdef USE_TESS_NEEDS_ST2
			if ( tess.needsST2 )
#endif
			{
				texCoords1[0] = srf->lmTexCoords[i][0];
				texCoords1[1] = srf->lmTexCoords[i][1];
				texCoords1 += 2;
			}
			target[i] = color.u32;
#ifdef USE_LEGACY_DLIGHTS
			tess.vertexDlightBits[tess.numVertexes + i] = dlightBits;
#endif
		}
		tess.numVertexes += srf->numVerts;
	}
}

#endif
