/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RUNTIME_MDR_H
#define FNTECH3_TR_RUNTIME_MDR_H

/* Engine-only mutable MDR models, included after the ordinary MDR loader.
 * Geometry is copied into renderer-owned storage by R_LoadMDR. The only
 * mutation is an independently validated single frame. No module/private
 * vector or caller buffer is retained. ownerNonce must be fresh for every
 * registration lifetime, including a renderer restart; engine callers own
 * that monotonic identity because ordinary qhandles are reused after reset. */
#include <float.h>
#include <stdint.h>

qboolean R_RuntimeMDRSceneUsesModel( qhandle_t handle );
void R_IssueRuntimeMDRCommands( void );

enum { FN_RUNTIME_MDR_MAX_BYTES = 64 * 1024 * 1024,
	FN_RUNTIME_MDR_MAX_SURFACES = 4096 };
typedef struct {
	model_t *model;
	mdrHeader_t *header;
	mdrHeader_t shape;
	uint64_t ownerNonce, generation;
	int bytes;
} fnRuntimeMdrRecord_t;
static fnRuntimeMdrRecord_t r_runtimeMdr[MAX_MOD_KNOWN];
static uint64_t r_runtimeMdrGeneration = 1;

static void R_RuntimeMDRReset( void ) {
	memset( r_runtimeMdr, 0, sizeof( r_runtimeMdr ) );
	/* Retire at overflow rather than reviving a prior lifetime. */
	if ( r_runtimeMdrGeneration && r_runtimeMdrGeneration != UINT64_MAX ) ++r_runtimeMdrGeneration;
	else r_runtimeMdrGeneration = 0;
}
static qboolean R_IsRuntimeMDR( qhandle_t handle ) {
	return handle > 0 && handle < MAX_MOD_KNOWN && r_runtimeMdr[handle].ownerNonce &&
		r_runtimeMdr[handle].generation == r_runtimeMdrGeneration ? qtrue : qfalse;
}
/* A runtime MDR's one frame is rewritten in place by every pose update, so
 * anything caching posed geometry by model, frame and lerp must not cache it. */
qboolean R_ModelIsRuntimeMDR( qhandle_t handle );
qboolean R_ModelIsRuntimeMDR( qhandle_t handle ) {
	return R_IsRuntimeMDR( handle );
}
/* Use bits, not isfinite: release renderers enable finite-math optimizations. */
static qboolean R_RuntimeMDRFinite( const void *address, size_t count ) {
	const byte *data = (const byte *)address;
	size_t i;
	for ( i = 0; i < count; ++i ) {
		uint32_t bits;
		memcpy( &bits, data + i * sizeof( float ), sizeof( bits ) );
		if ( ( bits & UINT32_C(0x7f800000) ) == UINT32_C(0x7f800000) ) return qfalse;
	}
	return qtrue;
}
static qboolean R_RuntimeMDRInput( const void *data, int bytes, fnMdrView_t *view ) {
	fnMdrLodView_t lod;
	uint32_t i, surfaceOffset;
	if ( !data || bytes < FN_MDR_HEADER_BYTES || bytes > FN_RUNTIME_MDR_MAX_BYTES ||
		!FnMdr_Open( data, (size_t)bytes, SHADER_MAX_VERTEXES, SHADER_MAX_INDEXES / 3, view ) ||
		view->bytes != (uint32_t)bytes || view->expandedBytes > FN_RUNTIME_MDR_MAX_BYTES ||
		view->compressed || view->numFrames != 1 ||
		view->numLODs != 1 || view->numTags ||
		!R_RuntimeMDRFinite( view->data + view->frames, 10 ) ||
		!R_RuntimeMDRFinite( view->data + view->frames + 56, (size_t)view->numBones * 12 ) ||
		!FnMdr_Lod( view, view->lods, &lod ) || !lod.numSurfaces ||
		lod.numSurfaces > FN_RUNTIME_MDR_MAX_SURFACES ) return qfalse;
	surfaceOffset = lod.surfaces;
	for ( i = 0; i < lod.numSurfaces; ++i ) {
		fnMdrSurfaceView_t surface;
		uint32_t v, vertexOffset;
		if ( !FnMdr_Surface( &lod, surfaceOffset, &surface ) ) return qfalse;
		vertexOffset = surface.vertices;
		for ( v = 0; v < surface.numVerts; ++v ) {
			uint32_t weights, next, w;
			const byte *vertex = surface.data + vertexOffset;
			if ( !FnMdr_Vertex( &surface, vertexOffset, &weights, &next ) ||
				!weights || weights > MDR_MAX_BONES || !R_RuntimeMDRFinite( vertex, 5 ) ) return qfalse;
			for ( w = 0; w < weights; ++w ) {
				const byte *weight = vertex + FN_MDR_VERTEX_BYTES + (size_t)w * FN_MDR_WEIGHT_BYTES;
				if ( !R_RuntimeMDRFinite( weight + 4, 4 ) || FnMdr_Float( weight + 4 ) < 0 ) return qfalse;
			}
			vertexOffset = next;
		}
		surfaceOffset += surface.bytes;
	}
	return qtrue;
}
static fnRuntimeMdrRecord_t *R_RuntimeMDRRecord( qhandle_t handle, uint64_t ownerNonce ) {
	fnRuntimeMdrRecord_t *record;
	model_t *model;
	if ( !tr.registered || !ownerNonce || handle <= 0 || handle >= tr.numModels ||
		!R_IsRuntimeMDR( handle ) ) return NULL;
	record = &r_runtimeMdr[handle];
	model = tr.models[handle];
	if ( record->ownerNonce != ownerNonce || !model || record->model != model ||
		model->index != handle || model->type != MOD_MDR || model->modelData != record->header ||
		model->dataSize != record->bytes || model->numLods != 1 ||
		record->bytes < FN_MDR_HEADER_BYTES ||
		memcmp( record->header, &record->shape, sizeof( record->shape ) ) ||
		record->shape.numFrames != 1 || record->shape.numLODs != 1 ||
		record->shape.numBones < 1 || record->shape.numBones > MDR_MAX_BONES ||
		record->shape.ofsFrames != FN_MDR_HEADER_BYTES || record->shape.ofsEnd != record->bytes ||
		(size_t)record->bytes < FN_MDR_HEADER_BYTES + 56 + (size_t)record->shape.numBones * sizeof( mdrBone_t ) ) return NULL;
	return record;
}
qhandle_t RE_RegisterRuntimeMDR( uint64_t ownerNonce, const void *data, int bytes ) {
	fnMdrView_t view;
	fnRuntimeMdrRecord_t *record;
	model_t *model;
	uint64_t generation = r_runtimeMdrGeneration;
	int i;
	if ( !tr.registered || !r_runtimeMdrGeneration || !ownerNonce || tr.numModels < 1 ||
		tr.numModels >= MAX_MOD_KNOWN || !R_RuntimeMDRInput( data, bytes, &view ) ) return 0;
	for ( i = 1; i < tr.numModels; ++i )
		if ( R_IsRuntimeMDR( i ) && r_runtimeMdr[i].ownerNonce == ownerNonce ) return 0;
	R_IssueRuntimeMDRCommands();
	if ( !tr.registered || generation != r_runtimeMdrGeneration || tr.numModels < 1 ||
		tr.numModels >= MAX_MOD_KNOWN ) return 0;
	model = R_AllocModel();
	if ( !model ) return 0;
	model->type = MOD_BAD; model->dataSize = 0; model->numLods = 0; model->modelData = NULL;
	/* This diagnostic name is never returned by ordinary filename lookup. */
	Com_sprintf( model->name, (int)sizeof( model->name ), "@runtime/mdr_%08x_%08x",
		(unsigned int)( ownerNonce >> 32 ), (unsigned int)ownerNonce );
	if ( !R_LoadMDR( model, (void *)data, bytes, model->name ) ) return 0;
	record = &r_runtimeMdr[model->index];
	record->model = model; record->header = (mdrHeader_t *)model->modelData;
	record->shape = *record->header; record->bytes = model->dataSize;
	record->ownerNonce = ownerNonce; record->generation = r_runtimeMdrGeneration;
	return model->index;
}
qboolean RE_UpdateRuntimeMDRPose( qhandle_t handle, uint64_t ownerNonce,
	const float *matrices, int boneCount, const vec3_t mins, const vec3_t maxs ) {
	fnRuntimeMdrRecord_t *record = R_RuntimeMDRRecord( handle, ownerNonce );
	mdrBone_t staged[MDR_MAX_BONES];
	vec3_t bounds[2], midpoint;
	mdrFrame_t *frame;
	double radiusSquared = 0;
	float radius;
	int axis;
	if ( !record || !matrices || !mins || !maxs || boneCount != record->shape.numBones ||
		!R_RuntimeMDRFinite( matrices, (size_t)boneCount * 12 ) ||
		!R_RuntimeMDRFinite( mins, 3 ) || !R_RuntimeMDRFinite( maxs, 3 ) ) return qfalse;
	memcpy( staged, matrices, (size_t)boneCount * sizeof( mdrBone_t ) );
	memcpy( bounds[0], mins, sizeof( vec3_t ) ); memcpy( bounds[1], maxs, sizeof( vec3_t ) );
	for ( axis = 0; axis < 3; ++axis ) {
		double half;
		if ( bounds[0][axis] > bounds[1][axis] ) return qfalse;
		half = ( (double)bounds[1][axis] - bounds[0][axis] ) * 0.5;
		midpoint[axis] = (float)( (double)bounds[0][axis] + half );
		/* Cover the rounded midpoint too, rather than underestimating a sphere. */
		half = fmax( (double)bounds[1][axis] - midpoint[axis], (double)midpoint[axis] - bounds[0][axis] );
		radiusSquared += half * half;
	}
	if ( sqrt( radiusSquared ) > FLT_MAX ) return qfalse;
	radius = (float)sqrt( radiusSquared );
	/* Round outward when the conversion rounded the radius down. */
	if ( (double)radius * radius < radiusSquared ) radius = nextafterf( radius, FLT_MAX );
	if ( !R_RuntimeMDRFinite( &radius, 1 ) || !R_RuntimeMDRFinite( midpoint, 3 ) ) return qfalse;
	frame = (mdrFrame_t *)( (byte *)record->header + record->shape.ofsFrames );
	if ( !memcmp( frame->bones, staged, (size_t)boneCount * sizeof( mdrBone_t ) ) &&
		!memcmp( frame->bounds, bounds, sizeof( bounds ) ) &&
		!memcmp( frame->localOrigin, midpoint, sizeof( midpoint ) ) && frame->radius == radius ) return qtrue;
	/* A pending frontend scene is not part of the render-command flush. Its
	 * refs still share this frame, so conflicting poses must refuse. */
	if ( R_RuntimeMDRSceneUsesModel( handle ) ) return qfalse;
	/* Consume the previous CPU pose without presenting the current Vulkan
	 * frame: its next scene still needs the open command buffer. */
	R_IssueRuntimeMDRCommands();
	record = R_RuntimeMDRRecord( handle, ownerNonce );
	if ( !record || R_RuntimeMDRSceneUsesModel( handle ) ) return qfalse;
	frame = (mdrFrame_t *)( (byte *)record->header + record->shape.ofsFrames );
	memcpy( frame->bones, staged, (size_t)boneCount * sizeof( mdrBone_t ) );
	memcpy( frame->bounds, bounds, sizeof( bounds ) );
	memcpy( frame->localOrigin, midpoint, sizeof( midpoint ) ); frame->radius = radius;
	return qtrue;
}
#endif
