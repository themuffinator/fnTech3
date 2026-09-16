/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MESH_VIEW_H
#define FNTECH3_TR_MESH_VIEW_H

/* The frontend consumes one mesh interface while each format keeps its own
 * validated geometry and animation representation. */
typedef struct {
	surfaceType_t *data;
	const char *name;
	int numShaders;
	md3Surface_t *md3;
	mdcMeshSurface_t *mdc;
	mdsMeshSurface_t *mds;
} meshSurfaceView_t;

static inline int R_MeshFrameCount( const model_t *model ) {
	if ( model->type == MOD_MDS ) return (int)model->mds->view.numFrames;
	return model->type == MOD_MDC ? (int)model->mdc[0]->view.numFrames : model->md3[0]->numFrames;
}

static inline const md3Frame_t *R_MeshFrame( const model_t *model, int lod, int frame ) {
	if ( model->type == MOD_MDS ) return model->mds->frames + frame;
	if ( model->type == MOD_MDC ) return model->mdc[lod]->frames + frame;
	return (const md3Frame_t *)( (const byte *)model->md3[lod] + model->md3[lod]->ofsFrames ) + frame;
}

static inline int R_MeshSurfaceCount( const model_t *model, int lod ) {
	if ( model->type == MOD_MDS ) return (int)model->mds->view.numSurfaces;
	return model->type == MOD_MDC ? (int)model->mdc[lod]->view.numSurfaces : model->md3[lod]->numSurfaces;
}

static inline void R_MeshSurfaceDescribe( meshSurfaceView_t *surface ) {
	if ( surface->mds ) {
		surface->data = &surface->mds->surfaceType;
		surface->name = surface->mds->name;
		surface->numShaders = 1;
	} else if ( surface->mdc ) {
		surface->data = &surface->mdc->surfaceType;
		surface->name = surface->mdc->name;
		surface->numShaders = (int)surface->mdc->view.numShaders;
	} else {
		surface->data = (surfaceType_t *)surface->md3;
		surface->name = surface->md3->name;
		surface->numShaders = surface->md3->numShaders;
	}
}

static inline void R_MeshFirstSurface( const model_t *model, int lod, meshSurfaceView_t *surface ) {
	surface->mds = model->type == MOD_MDS ? model->mds->surfaces : NULL;
	surface->mdc = model->type == MOD_MDC ? model->mdc[lod]->surfaces : NULL;
	surface->md3 = model->type != MOD_MESH ? NULL :
		(md3Surface_t *)( (byte *)model->md3[lod] + model->md3[lod]->ofsSurfaces );
}

static inline void R_MeshNextSurface( meshSurfaceView_t *surface ) {
	if ( surface->mds ) ++surface->mds;
	else if ( surface->mdc ) ++surface->mdc;
	else surface->md3 = (md3Surface_t *)( (byte *)surface->md3 + surface->md3->ofsEnd );
}

static inline int R_MeshShaderIndex( const meshSurfaceView_t *surface, int skinNum ) {
	int index;
	if ( surface->numShaders <= 0 ) return 0;
	if ( surface->mds ) return surface->mds->shader;
	index = skinNum < 0 ? 0 : skinNum % surface->numShaders;
	if ( surface->mdc ) return surface->mdc->shaders[index];
	return ( (const md3Shader_t *)( (const byte *)surface->md3 + surface->md3->ofsShaders ) )[index].shaderIndex;
}

#endif
