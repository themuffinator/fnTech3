/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_MODEL_H
#define FNTECH3_TR_MDS_MODEL_H

static mdsModel_t *R_LoadMDS( model_t *model, const void *buffer, int bytes, const char *name ) {
	fnMdsView_t view;
	mdsModel_t *result;
	fnMdsVertexView_t *vertices;
	byte *storage;
	uint64_t allocation;
	uint32_t offset, i;
	if ( bytes < 0 || !FnMds_Open( buffer, (size_t)bytes, &view ) ) {
		ri.Printf( PRINT_WARNING, "Invalid MDS model: %s\n", name );
		return NULL;
	}
	allocation = sizeof( mdsModel_t ) + (uint64_t)view.numFrames * sizeof( md3Frame_t ) +
		(uint64_t)view.numSurfaces * sizeof( mdsMeshSurface_t ) + view.bytes;
	offset = view.surfaces;
	for ( i = 0; i < view.numSurfaces; ++i ) {
		fnMdsSurfaceView_t surface;
		FnMds_Surface( &view, offset, &surface );
		if ( surface.numVerts >= SHADER_MAX_VERTEXES || surface.numTriangles > ( SHADER_MAX_INDEXES - 1 ) / 3 ) {
			ri.Printf( PRINT_WARNING, "MDS surface exceeds renderer geometry capacity: %s\n", name );
			return NULL;
		}
		allocation += (uint64_t)surface.numVerts * sizeof( fnMdsVertexView_t );
		offset += (uint32_t)surface.bytes;
	}
	if ( model->dataSize < 0 || allocation > INT_MAX || allocation > (uint64_t)( INT_MAX - model->dataSize ) ) {
		ri.Printf( PRINT_WARNING, "MDS model exceeds allocation capacity: %s\n", name );
		return NULL;
	}
	result = ri.Hunk_Alloc( (int)allocation, h_low );
	result->frames = (md3Frame_t *)( result + 1 );
	result->surfaces = (mdsMeshSurface_t *)( result->frames + view.numFrames );
	vertices = (fnMdsVertexView_t *)( result->surfaces + view.numSurfaces );
	storage = (byte *)result + (size_t)allocation - view.bytes;
	memcpy( storage, view.data, view.bytes );
	result->view = view;
	result->view.data = storage;
	for ( i = 0; i < view.numFrames; ++i ) {
		fnMdsFrame_t frame;
		FnMds_Frame( &view, i, &frame );
		memset( &result->frames[i], 0, sizeof( result->frames[i] ) );
		memcpy( result->frames[i].bounds, frame.bounds, sizeof( frame.bounds ) );
		memcpy( result->frames[i].localOrigin, frame.origin, sizeof( frame.origin ) );
		result->frames[i].radius = frame.radius;
	}
	offset = view.surfaces;
	for ( i = 0; i < view.numSurfaces; ++i ) {
		mdsMeshSurface_t *surface = result->surfaces + i;
		uint32_t j, vertexOffset;
		const char *shaderName;
		surface->surfaceType = SF_MDS;
		surface->model = result;
		FnMds_Surface( &result->view, offset, &surface->view );
		/* MDS preserves the authored name, including suffixes and case. */
		Q_strncpyz( surface->name, (const char *)surface->view.data + 4, sizeof( surface->name ) );
		surface->vertices = vertices;
		vertexOffset = surface->view.vertices;
		for ( j = 0; j < surface->view.numVerts; ++j, ++vertices ) {
			FnMds_Vertex( &surface->view, vertexOffset, vertices );
			vertexOffset = vertices->next;
		}
		surface->shader = 0;
		shaderName = (const char *)surface->view.data + 68;
		if ( shaderName[0] ) {
			shader_t *shader = R_FindShader( shaderName, LIGHTMAP_NONE, qtrue );
			if ( !shader->defaultShader ) surface->shader = shader->index;
		}
		offset += (uint32_t)surface->view.bytes;
	}
	model->dataSize += (int)allocation;
	return result;
}

static qhandle_t R_RegisterMDS( const char *name, model_t *model ) {
	void *buffer = NULL;
	mdsModel_t *mds;
	const int bytes = ri.FS_ReadFile( name, &buffer );
	if ( !buffer ) return 0;
	mds = R_LoadMDS( model, buffer, bytes, name );
	ri.FS_FreeFile( buffer );
	if ( !mds ) return 0;
	memset( model->md3, 0, sizeof( model->md3 ) );
	memset( model->mdc, 0, sizeof( model->mdc ) );
	model->modelData = NULL;
	model->bmodel = NULL;
	model->mds = mds;
	model->numLods = 1; // MDS reduces its own vertices, without separate LOD files.
	model->type = MOD_MDS;
	return model->index;
}

#endif
