/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDC_MODEL_H
#define FNTECH3_TR_MDC_MODEL_H

fnMdcNormals_t r_mdcNormals;

static mdcModel_t *R_LoadMDC( model_t *model, const void *buffer, int bytes, const char *name, int frames ) {
	fnMdcView_t view;
	mdcModel_t *result;
	byte *storage;
	uint64_t allocation;
	uint32_t offset, i;
	int *shaders;
	if ( bytes < 0 || !FnMdc_Open( buffer, (size_t)bytes, &view ) ||
		( frames && view.numFrames != (uint32_t)frames ) ) {
		ri.Printf( PRINT_WARNING, "Invalid MDC model or inconsistent LOD frames: %s\n", name );
		return NULL;
	}
	allocation = sizeof( mdcModel_t ) + (uint64_t)view.numFrames * sizeof( md3Frame_t ) +
		(uint64_t)view.numSurfaces * sizeof( mdcMeshSurface_t ) + view.bytes;
	offset = view.surfaces;
	for ( i = 0; i < view.numSurfaces; ++i ) {
		fnMdcSurfaceView_t surface;
		FnMdc_Surface( &view, offset, &surface );
		if ( surface.numVerts >= SHADER_MAX_VERTEXES || surface.numTriangles > (SHADER_MAX_INDEXES - 1) / 3 ) {
			ri.Printf( PRINT_WARNING, "MDC surface exceeds renderer geometry capacity: %s\n", name );
			return NULL;
		}
		allocation += (uint64_t)surface.numShaders * sizeof( int );
		offset += (uint32_t)surface.bytes;
	}
	if ( allocation > INT_MAX || allocation > (uint64_t)( INT_MAX - model->dataSize ) ) {
		ri.Printf( PRINT_WARNING, "MDC model exceeds allocation capacity: %s\n", name );
		return NULL;
	}
	result = ri.Hunk_Alloc( (int)allocation, h_low );
	result->frames = (md3Frame_t *)( result + 1 );
	result->surfaces = (mdcMeshSurface_t *)( result->frames + view.numFrames );
	shaders = (int *)( result->surfaces + view.numSurfaces );
	storage = (byte *)result + (size_t)allocation - view.bytes;
	memcpy( storage, view.data, view.bytes );
	result->view = view;
	result->view.data = storage;
	for ( i = 0; i < view.numFrames; ++i ) {
		fnMdcFrame_t frame;
		FnMdc_Frame( &view, i, &frame );
		memcpy( result->frames[i].bounds, frame.bounds, sizeof( frame.bounds ) );
		memcpy( result->frames[i].localOrigin, frame.origin, sizeof( frame.origin ) );
		result->frames[i].radius = frame.radius;
		/* RTCW's loader gives these animated models a fixed culling envelope.
		 * Keep the historical inverted bounds in engine metadata; disk bytes
		 * and the standalone format reader remain unchanged. */
		if ( strstr( name, "sherman" ) || strstr( name, "mg42" ) ) {
			int axis;
			result->frames[i].radius = 256;
			for ( axis = 0; axis < 3; ++axis ) {
				result->frames[i].bounds[0][axis] = 128;
				result->frames[i].bounds[1][axis] = -128;
			}
		}
	}
	offset = view.surfaces;
	for ( i = 0; i < view.numSurfaces; ++i ) {
		mdcMeshSurface_t *surface = result->surfaces + i;
		uint32_t j;
		size_t length;
		surface->surfaceType = SF_MDC;
		FnMdc_Surface( &result->view, offset, &surface->view );
		Q_strncpyz( surface->name, (const char *)surface->view.data + 4, sizeof( surface->name ) );
		Q_strlwr( surface->name );
		length = strlen( surface->name );
		if ( length > 2 && surface->name[length - 2] == '_' ) surface->name[length - 2] = 0;
		surface->shaders = shaders;
		for ( j = 0; j < surface->view.numShaders; ++j ) {
			const char *shaderName = (const char *)surface->view.data + surface->view.shaders + (size_t)j * FN_MDC_SHADER_BYTES;
			shader_t *shader = R_FindShader( shaderName, LIGHTMAP_NONE, qtrue );
			*shaders++ = shader->defaultShader ? 0 : shader->index;
		}
		offset += (uint32_t)surface->view.bytes;
	}
	model->dataSize += (int)allocation;
	return result;
}

static qhandle_t R_RegisterMDC( const char *name, model_t *model ) {
	char base[MAX_QPATH], filename[MAX_QPATH + 20];
	mdcModel_t *lods[MD3_MAX_LODS] = { NULL };
	int lod, highest = -1, frames = 0;
	COM_StripExtension( name, base, sizeof( base ) );
	for ( lod = 0; lod < MD3_MAX_LODS; ++lod ) {
		void *buffer = NULL;
		int bytes;
		if ( lod ) Com_sprintf( filename, sizeof( filename ), "%s_%d.mdc", base, lod );
		else Com_sprintf( filename, sizeof( filename ), "%s.mdc", base );
		bytes = ri.FS_ReadFile( filename, &buffer );
		if ( !buffer ) continue;
		lods[lod] = R_LoadMDC( model, buffer, bytes, filename, frames );
		ri.FS_FreeFile( buffer );
		if ( lods[lod] ) {
			frames = (int)lods[lod]->view.numFrames;
			highest = lod;
		}
	}
	if ( highest < 0 ) return 0;
	/* Missing detail levels use the nearest available coarser level. All array
	 * slots are valid even when only a suffix model is installed. */
	for ( lod = highest - 1; lod >= 0; --lod ) if ( !lods[lod] ) lods[lod] = lods[lod + 1];
	for ( lod = highest + 1; lod < MD3_MAX_LODS; ++lod ) lods[lod] = lods[highest];
	/* An earlier format loader may have allocated data before rejecting the
	 * file. Do not let those stale pointers select a different tag/mesh path. */
	memset( model->md3, 0, sizeof( model->md3 ) );
	model->modelData = NULL;
	model->bmodel = NULL;
	model->mds = NULL;
	memcpy( model->mdc, lods, sizeof( lods ) );
	model->numLods = highest + 1;
	model->type = MOD_MDC;
	return model->index;
}

static md3Tag_t *R_GetMDCTag( const mdcModel_t *model, int frame, const char *name, md3Tag_t *tag ) {
	uint32_t index;
	float angles[3];
	if ( frame < 0 ) frame = 0;
	if ( (uint32_t)frame >= model->view.numFrames ) frame = (int)model->view.numFrames - 1;
	for ( index = 0; index < model->view.numTags; ++index ) {
		if ( !strcmp( name, (const char *)model->view.data + model->view.tagNames + (size_t)index * 64 ) ) {
			FnMdc_Tag( &model->view, (uint32_t)frame, index, tag->origin, angles );
			AnglesToAxis( angles, tag->axis );
			return tag;
		}
	}
	return NULL;
}

#endif
