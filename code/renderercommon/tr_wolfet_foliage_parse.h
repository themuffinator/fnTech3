/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_FOLIAGE_PARSE_H
#define FNTECH3_TR_WOLFET_FOLIAGE_PARSE_H

/* Included by a renderer's BSP loader after ShaderForShaderNum and
 * R_ColorShiftLightingBytes. Foliage is vertex lit, as ET draws it. */
static void ParseWolfETFoliage( const dsurface_t *ds, const drawVert_t *verts, int lumpVerts,
	msurface_t *surf, const int *indexes, int lumpIndexes ) {
	static surfaceType_t skipData = SF_SKIP;
	fnWolfETFoliageCounts_t counts;
	srfWolfETFoliage_t *foliage;
	vec3_t meshBounds[2], corner;
	byte *block;
	size_t size;
	float scale;
	int i, j;

	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	surf->shader = ShaderForShaderNum( LittleLong( ds->shaderNum ), LIGHTMAP_BY_VERTEX );

	if ( !FnWolfET_FoliageCounts( LittleLong( ds->patchWidth ), LittleLong( ds->patchHeight ),
		LittleLong( ds->numIndexes ), LittleLong( ds->firstVert ), LittleLong( ds->firstIndex ),
		lumpVerts, lumpIndexes, SHADER_MAX_VERTEXES, SHADER_MAX_INDEXES, &counts ) ) {
		ri.Error( ERR_DROP, "Bad foliage surface in %s", s_worldData.name );
	}
	if ( !counts.instances ) {
		surf->data = &skipData;
		return;
	}

	size = sizeof( *foliage ) +
		(size_t)counts.meshVerts * ( 2 * sizeof( vec4_t ) + 2 * sizeof( vec2_t ) ) +
		(size_t)counts.indexes * sizeof( int ) +
		(size_t)counts.instances * sizeof( wolfetFoliageInstance_t );
	if ( size > INT_MAX ) ri.Error( ERR_DROP, "Foliage surface too large in %s", s_worldData.name );
	block = ri.Hunk_Alloc( (int)size, h_low );
	foliage = (srfWolfETFoliage_t *)block;
	block += sizeof( *foliage );
	foliage->surfaceType = SF_WOLFET_FOLIAGE;
	foliage->numVerts = counts.meshVerts;
	foliage->numIndexes = counts.indexes;
	foliage->numInstances = counts.instances;
	foliage->xyz = (vec4_t *)block; block += counts.meshVerts * sizeof( vec4_t );
	foliage->normal = (vec4_t *)block; block += counts.meshVerts * sizeof( vec4_t );
	foliage->texCoords = (vec2_t *)block; block += counts.meshVerts * sizeof( vec2_t );
	foliage->lmTexCoords = (vec2_t *)block; block += counts.meshVerts * sizeof( vec2_t );
	foliage->indexes = (int *)block; block += counts.indexes * sizeof( int );
	foliage->instances = (wolfetFoliageInstance_t *)block;

	/* The mesh, with its height scaled as the map loads. */
	scale = FnWolfET_FoliageHeightScale( r_drawfoliage ? r_drawfoliage->value : 1.0f );
	verts += LittleLong( ds->firstVert );
	ClearBounds( meshBounds[0], meshBounds[1] );
	for ( i = 0; i < counts.meshVerts; ++i ) {
		for ( j = 0; j < 3; ++j ) {
			foliage->xyz[i][j] = LittleFloat( verts[i].xyz[j] );
			foliage->normal[i][j] = R_ClampDenorm( LittleFloat( verts[i].normal[j] ) );
		}
		foliage->xyz[i][2] *= scale;
		foliage->xyz[i][3] = foliage->normal[i][3] = 0;
		for ( j = 0; j < 2; ++j ) {
			foliage->texCoords[i][j] = LittleFloat( verts[i].st[j] );
			foliage->lmTexCoords[i][j] = LittleFloat( verts[i].lightmap[j] );
		}
		AddPointToBounds( foliage->xyz[i], meshBounds[0], meshBounds[1] );
	}

	indexes += LittleLong( ds->firstIndex );
	for ( i = 0; i < counts.indexes; ++i ) {
		foliage->indexes[i] = LittleLong( indexes[i] );
		if ( foliage->indexes[i] < 0 || foliage->indexes[i] >= counts.meshVerts ) {
			ri.Error( ERR_DROP, "Bad index in foliage surface in %s", s_worldData.name );
		}
	}

	/* The instances follow the mesh, one vertex each. The surface's bounds are
	 * the mesh's bounds carried to every instance. */
	verts += counts.meshVerts;
	ClearBounds( foliage->bounds[0], foliage->bounds[1] );
	for ( i = 0; i < counts.instances; ++i ) {
		wolfetFoliageInstance_t *instance = &foliage->instances[i];
		for ( j = 0; j < 3; ++j ) instance->origin[j] = LittleFloat( verts[i].xyz[j] );
		VectorAdd( meshBounds[0], instance->origin, corner );
		AddPointToBounds( corner, foliage->bounds[0], foliage->bounds[1] );
		VectorAdd( meshBounds[1], instance->origin, corner );
		AddPointToBounds( corner, foliage->bounds[0], foliage->bounds[1] );
		R_ColorShiftLightingBytes( verts[i].color.rgba, instance->color.rgba, qtrue );
	}
	VectorAdd( foliage->bounds[0], foliage->bounds[1], foliage->origin );
	VectorScale( foliage->origin, 0.5f, foliage->origin );
	VectorSubtract( foliage->bounds[1], foliage->origin, corner );
	foliage->radius = VectorLength( corner );

	surf->data = (surfaceType_t *)foliage;
}

#endif
