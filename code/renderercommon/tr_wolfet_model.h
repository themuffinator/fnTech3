/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_MODEL_H
#define FNTECH3_TR_WOLFET_MODEL_H

/* Loads Enemy Territory's two skeletal files into the renderer's model table.
 * A mesh (MDMW) and a skeleton (MDXW) are separate models: the mesh holds the
 * surfaces that are drawn, the skeleton holds the frames that pose them and is
 * never drawn on its own. */

static void R_ResetWolfETModel( model_t *model ) {
	memset( model->md3, 0, sizeof( model->md3 ) );
	memset( model->mdc, 0, sizeof( model->mdc ) );
	model->mds = NULL;
	model->wolfetMesh = NULL;
	model->wolfetSkeleton = NULL;
	model->modelData = NULL;
	model->bmodel = NULL;
}

static wolfetMeshModel_t *R_LoadWolfETMesh( model_t *model, const void *buffer, int bytes,
	const char *name ) {
	fnWolfETMeshView_t view;
	wolfetMeshModel_t *result;
	fnWolfETVertexView_t *vertices;
	byte *storage;
	uint64_t allocation;
	uint32_t offset, i;
	if ( bytes < 0 || !FnWolfET_Mesh( (const unsigned char *)buffer, (size_t)bytes, &view ) ) {
		ri.Printf( PRINT_WARNING, "Invalid WolfET mesh: %s\n", name );
		return NULL;
	}
	allocation = sizeof( wolfetMeshModel_t ) +
		(uint64_t)view.numSurfaces * sizeof( wolfetMeshSurface_t ) + view.bytes;
	offset = view.surfaces;
	for ( i = 0; i < view.numSurfaces; ++i ) {
		fnWolfETMeshSurfaceView_t surface;
		if ( !FnWolfET_MeshSurfaceAt( &view, offset, &surface ) ) {
			ri.Printf( PRINT_WARNING, "Invalid WolfET mesh surface: %s\n", name );
			return NULL;
		}
		if ( surface.numVerts >= SHADER_MAX_VERTEXES ||
			surface.numTriangles > ( SHADER_MAX_INDEXES - 1 ) / 3 ) {
			ri.Printf( PRINT_WARNING, "WolfET surface exceeds renderer geometry capacity: %s\n", name );
			return NULL;
		}
		allocation += (uint64_t)surface.numVerts * sizeof( fnWolfETVertexView_t );
		offset += surface.end;
	}
	if ( model->dataSize < 0 || allocation > INT_MAX ||
		allocation > (uint64_t)( INT_MAX - model->dataSize ) ) {
		ri.Printf( PRINT_WARNING, "WolfET mesh exceeds allocation capacity: %s\n", name );
		return NULL;
	}
	result = ri.Hunk_Alloc( (int)allocation, h_low );
	result->surfaces = (wolfetMeshSurface_t *)( result + 1 );
	vertices = (fnWolfETVertexView_t *)( result->surfaces + view.numSurfaces );
	storage = (byte *)result + (size_t)allocation - view.bytes;
	memcpy( storage, view.data, view.bytes );
	result->view = view;
	result->view.data = storage;
	offset = view.surfaces;
	for ( i = 0; i < view.numSurfaces; ++i ) {
		wolfetMeshSurface_t *surface = result->surfaces + i;
		uint32_t j, vertexOffset;
		const char *shaderName;
		surface->surfaceType = SF_WOLFET;
		surface->model = result;
		if ( !FnWolfET_MeshSurfaceAt( &result->view, offset, &surface->view ) ) return NULL;
		/* The authored surface name is preserved, so a skin can name it. */
		Q_strncpyz( surface->name, (const char *)surface->view.data + offset + 4,
			sizeof( surface->name ) );
		surface->vertices = vertices;
		vertexOffset = surface->view.vertices;
		for ( j = 0; j < surface->view.numVerts; ++j, ++vertices ) {
			if ( !FnWolfET_MeshVertexAt( &surface->view, vertexOffset, vertices ) ) return NULL;
			vertexOffset = vertices->next;
		}
		/* The compiler writes no usable shader index, so the name is resolved
		 * here, as ET's own loader does. */
		surface->shader = 0;
		shaderName = (const char *)surface->view.data + offset + 68;
		if ( shaderName[0] ) {
			shader_t *shader = R_FindShader( shaderName, LIGHTMAP_NONE, qtrue );
			if ( !shader->defaultShader ) surface->shader = shader->index;
		}
		offset += surface->view.end;
	}
	model->dataSize += (int)allocation;
	return result;
}

static qhandle_t R_RegisterWolfETMesh( const char *name, model_t *model ) {
	void *buffer = NULL;
	wolfetMeshModel_t *mesh;
	const int bytes = ri.FS_ReadFile( name, &buffer );
	if ( !buffer ) return 0;
	mesh = R_LoadWolfETMesh( model, buffer, bytes, name );
	ri.FS_FreeFile( buffer );
	if ( !mesh ) return 0;
	R_ResetWolfETModel( model );
	model->wolfetMesh = mesh;
	/* A mesh reduces its own vertices and ships no separate detail files. */
	model->numLods = 1;
	model->type = MOD_WOLFET_MESH;
	return model->index;
}

static wolfetSkeletonModel_t *R_LoadWolfETSkeleton( model_t *model, const void *buffer,
	int bytes, const char *name ) {
	fnWolfETSkeletonView_t view;
	wolfetSkeletonModel_t *result;
	byte *storage;
	uint64_t allocation;
	uint32_t i;
	if ( bytes < 0 || !FnWolfET_Skeleton( (const unsigned char *)buffer, (size_t)bytes, &view ) ) {
		ri.Printf( PRINT_WARNING, "Invalid WolfET skeleton: %s\n", name );
		return NULL;
	}
	allocation = sizeof( wolfetSkeletonModel_t ) +
		(uint64_t)view.numFrames * sizeof( md3Frame_t ) + view.bytes;
	if ( model->dataSize < 0 || allocation > INT_MAX ||
		allocation > (uint64_t)( INT_MAX - model->dataSize ) ) {
		ri.Printf( PRINT_WARNING, "WolfET skeleton exceeds allocation capacity: %s\n", name );
		return NULL;
	}
	result = ri.Hunk_Alloc( (int)allocation, h_low );
	result->frames = (md3Frame_t *)( result + 1 );
	storage = (byte *)result + (size_t)allocation - view.bytes;
	memcpy( storage, view.data, view.bytes );
	result->view = view;
	result->view.data = storage;
	for ( i = 0; i < view.numFrames; ++i ) {
		fnWolfETFrame_t frame;
		if ( !FnWolfET_Frame( &result->view, i, &frame ) ) return NULL;
		memset( &result->frames[i], 0, sizeof( result->frames[i] ) );
		memcpy( result->frames[i].bounds, frame.bounds, sizeof( frame.bounds ) );
		memcpy( result->frames[i].localOrigin, frame.origin, sizeof( frame.origin ) );
		result->frames[i].radius = frame.radius;
	}
	model->dataSize += (int)allocation;
	return result;
}

static qhandle_t R_RegisterWolfETSkeleton( const char *name, model_t *model ) {
	void *buffer = NULL;
	wolfetSkeletonModel_t *skeleton;
	const int bytes = ri.FS_ReadFile( name, &buffer );
	if ( !buffer ) return 0;
	skeleton = R_LoadWolfETSkeleton( model, buffer, bytes, name );
	ri.FS_FreeFile( buffer );
	if ( !skeleton ) return 0;
	R_ResetWolfETModel( model );
	model->wolfetSkeleton = skeleton;
	model->numLods = 1;
	model->type = MOD_WOLFET_SKELETON;
	return model->index;
}

#endif
