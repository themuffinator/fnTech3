/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDR_FORMAT_H
#define FNTECH3_TR_MDR_FORMAT_H

/* Immutable, endian-independent reader for Raven RDM5 version 2. Disk offsets
 * never become pointers until their complete ranges have been checked. Bone
 * indices are global indices, despite the old qfiles.h comment about remapping
 * them through a surface's bone-reference list. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <limits.h>

enum { FN_MDR_HEADER_BYTES = 104, FN_MDR_LOD_BYTES = 12,
	FN_MDR_SURFACE_BYTES = 168, FN_MDR_VERTEX_BYTES = 24,
	FN_MDR_WEIGHT_BYTES = 20, FN_MDR_TAG_BYTES = 36, FN_MDR_MAX_BONES = 128 };

typedef struct {
	const unsigned char *data;
	size_t bytes;
	uint32_t numFrames, numBones, numLODs, numTags;
	uint32_t frames, frameStride, lods, tags, expandedBytes;
	int compressed;
} fnMdrView_t;

typedef struct {
	const unsigned char *data;
	uint32_t bytes, numSurfaces, surfaces;
} fnMdrLodView_t;

typedef struct {
	const unsigned char *data;
	uint32_t bytes, numVerts, vertices, numTriangles, triangles;
	uint32_t numBoneReferences, boneReferences;
} fnMdrSurfaceView_t;

static inline uint32_t FnMdr_U32( const unsigned char *p ) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline float FnMdr_Float( const unsigned char *p ) {
	const uint32_t bits = FnMdr_U32( p );
	float result;
	memcpy( &result, &bits, sizeof( result ) );
	return result;
}
static inline int FnMdr_Range( size_t bytes, uint32_t offset, size_t count, size_t stride, size_t minimum ) {
	return stride && offset <= bytes && ( !count || offset >= minimum ) && count <= ( bytes - offset ) / stride;
}
static inline int FnMdr_AddSize( uint32_t *size, size_t count, size_t stride ) {
	if ( !stride || *size > INT_MAX || count > ( INT_MAX - *size ) / stride ) return 0;
	*size += (uint32_t)( count * stride );
	return 1;
}
static inline int FnMdr_Finite( const unsigned char *p, size_t count ) {
	size_t i;
	/* Check the encoded exponent before creating a float. Renderer builds use
	 * finite-math optimizations, which can fold isfinite(float) to true. */
	for ( i = 0; i < count; ++i )
		if ( ( FnMdr_U32( p + i * 4 ) & UINT32_C(0x7f800000) ) == UINT32_C(0x7f800000) ) return 0;
	return 1;
}

/* Retail stvoy.exe 1.20, 0x462f70: three translations followed by the nine
 * matrix components. No normalization or quaternion interpolation is applied. */
static inline void FnMdr_Uncompress( const unsigned char *p, float matrix[3][4] ) {
	int i;
	for ( i = 0; i < 12; ++i ) {
		const int value = ( (int)p[i * 2] | (int)p[i * 2 + 1] << 8 ) - 32768;
		if ( i < 3 ) matrix[i][3] = value * ( 1.0f / 64.0f );
		else matrix[( i - 3 ) / 3][( i - 3 ) % 3] = value * ( 1.0f / 32766.0f );
	}
}

static inline int FnMdr_Bone( const fnMdrView_t *view, uint32_t frame,
	uint32_t bone, float matrix[3][4] ) {
	const unsigned char *p;
	int i;
	if ( !view || !view->data || !matrix || frame >= view->numFrames || bone >= view->numBones ||
		!view->numBones || view->numBones > FN_MDR_MAX_BONES ||
		view->frameStride != ( view->compressed ? 40 + view->numBones * 24 : 56 + view->numBones * 48 ) ||
		!FnMdr_Range( view->bytes, view->frames, view->numFrames, view->frameStride, FN_MDR_HEADER_BYTES ) ) return 0;
	p = view->data + view->frames + (size_t)frame * view->frameStride;
	if ( view->compressed ) FnMdr_Uncompress( p + 40 + (size_t)bone * 24, matrix );
	else {
		p += 56 + (size_t)bone * 48;
		for ( i = 0; i < 12; ++i ) matrix[i / 4][i % 4] = FnMdr_Float( p + i * 4 );
	}
	return 1;
}

static inline int FnMdr_Lod( const fnMdrView_t *view, uint32_t offset, fnMdrLodView_t *destination ) {
	fnMdrLodView_t result;
	if ( !view || !view->data || !destination ||
		!FnMdr_Range( view->bytes, offset, 1, FN_MDR_LOD_BYTES, FN_MDR_HEADER_BYTES ) ) return 0;
	result.data = view->data + offset;
	result.numSurfaces = FnMdr_U32( result.data );
	result.surfaces = FnMdr_U32( result.data + 4 );
	result.bytes = FnMdr_U32( result.data + 8 );
	if ( result.bytes < FN_MDR_LOD_BYTES || result.bytes > view->bytes - offset ||
		!FnMdr_Range( result.bytes, result.surfaces, result.numSurfaces, FN_MDR_SURFACE_BYTES, FN_MDR_LOD_BYTES ) ) return 0;
	*destination = result;
	return 1;
}

static inline int FnMdr_Surface( const fnMdrLodView_t *lod, uint32_t offset, fnMdrSurfaceView_t *destination ) {
	fnMdrSurfaceView_t result;
	const unsigned char *p;
	if ( !lod || !lod->data || !destination ||
		!FnMdr_Range( lod->bytes, offset, 1, FN_MDR_SURFACE_BYTES, FN_MDR_LOD_BYTES ) ) return 0;
	result.data = p = lod->data + offset;
	result.bytes = FnMdr_U32( p + 164 );
	result.numVerts = FnMdr_U32( p + 140 ); result.vertices = FnMdr_U32( p + 144 );
	result.numTriangles = FnMdr_U32( p + 148 ); result.triangles = FnMdr_U32( p + 152 );
	result.numBoneReferences = FnMdr_U32( p + 156 ); result.boneReferences = FnMdr_U32( p + 160 );
	if ( result.bytes < FN_MDR_SURFACE_BYTES || result.bytes > lod->bytes - offset ||
		!FnMdr_Range( result.bytes, result.vertices, result.numVerts, FN_MDR_VERTEX_BYTES, FN_MDR_SURFACE_BYTES ) ||
		!FnMdr_Range( result.bytes, result.triangles, result.numTriangles, 12, FN_MDR_SURFACE_BYTES ) ||
		!FnMdr_Range( result.bytes, result.boneReferences, result.numBoneReferences, 4, FN_MDR_SURFACE_BYTES ) ) return 0;
	*destination = result;
	return 1;
}

static inline int FnMdr_Vertex( const fnMdrSurfaceView_t *surface, uint32_t offset,
	uint32_t *numWeights, uint32_t *next ) {
	uint32_t count;
	if ( !surface || !surface->data || !numWeights || !next ||
		!FnMdr_Range( surface->bytes, offset, 1, FN_MDR_VERTEX_BYTES, FN_MDR_SURFACE_BYTES ) ) return 0;
	count = FnMdr_U32( surface->data + offset + 20 );
	if ( !FnMdr_Range( surface->bytes, offset + FN_MDR_VERTEX_BYTES, count, FN_MDR_WEIGHT_BYTES, FN_MDR_SURFACE_BYTES ) ) return 0;
	*numWeights = count;
	*next = offset + FN_MDR_VERTEX_BYTES + count * FN_MDR_WEIGHT_BYTES;
	return 1;
}

static inline int FnMdr_Open( const void *data, size_t bytes, uint32_t maxVerts,
	uint32_t maxTriangles, fnMdrView_t *destination ) {
	fnMdrView_t result;
	const unsigned char *p = (const unsigned char *)data;
	uint32_t i, offset, rawFrames;
	if ( !p || !destination || bytes < FN_MDR_HEADER_BYTES ||
		FnMdr_U32( p ) != UINT32_C(0x354d4452) || FnMdr_U32( p + 4 ) != 2 ) return 0;
	result.data = p; result.bytes = FnMdr_U32( p + 100 );
	result.numFrames = FnMdr_U32( p + 72 ); result.numBones = FnMdr_U32( p + 76 );
	rawFrames = FnMdr_U32( p + 80 ); result.compressed = rawFrames > INT32_MAX;
	result.frames = result.compressed ? UINT32_C(0) - rawFrames : rawFrames;
	result.numLODs = FnMdr_U32( p + 84 ); result.lods = FnMdr_U32( p + 88 );
	result.numTags = FnMdr_U32( p + 92 ); result.tags = FnMdr_U32( p + 96 );
	if ( result.bytes < FN_MDR_HEADER_BYTES || result.bytes > bytes || result.bytes > INT_MAX ||
		!result.numFrames || !result.numBones || result.numBones > FN_MDR_MAX_BONES || !result.numLODs ) return 0;
	result.frameStride = result.compressed ? 40 + result.numBones * 24 : 56 + result.numBones * 48;
	if ( !FnMdr_Range( result.bytes, result.frames, result.numFrames, result.frameStride, FN_MDR_HEADER_BYTES ) ||
		!FnMdr_Range( result.bytes, result.lods, result.numLODs, FN_MDR_LOD_BYTES, FN_MDR_HEADER_BYTES ) ||
		!FnMdr_Range( result.bytes, result.tags, result.numTags, FN_MDR_TAG_BYTES, FN_MDR_HEADER_BYTES ) ) return 0;
	result.expandedBytes = FN_MDR_HEADER_BYTES;
	if ( !FnMdr_AddSize( &result.expandedBytes, result.numFrames, 56 + result.numBones * 48 ) ||
		!FnMdr_AddSize( &result.expandedBytes, result.numTags, FN_MDR_TAG_BYTES ) ) return 0;
	for ( i = 0; i < result.numFrames; ++i ) {
		const unsigned char *frame = p + result.frames + (size_t)i * result.frameStride;
		int axis;
		if ( !FnMdr_Finite( frame, 10 ) || FnMdr_Float( frame + 36 ) < 0 ) return 0;
		for ( axis = 0; axis < 3; ++axis )
			if ( FnMdr_Float( frame + axis * 4 ) > FnMdr_Float( frame + 12 + axis * 4 ) ) return 0;
		if ( !result.compressed && !FnMdr_Finite( frame + 56, (size_t)result.numBones * 12 ) ) return 0;
	}
	for ( i = 0; i < result.numTags; ++i )
		if ( FnMdr_U32( p + result.tags + (size_t)i * FN_MDR_TAG_BYTES ) >= result.numBones ) return 0;
	offset = result.lods;
	for ( i = 0; i < result.numLODs; ++i ) {
		fnMdrLodView_t lod;
		uint32_t j, surfaceOffset;
		if ( !FnMdr_Lod( &result, offset, &lod ) || !FnMdr_AddSize( &result.expandedBytes, 1, FN_MDR_LOD_BYTES ) ) return 0;
		surfaceOffset = lod.surfaces;
		for ( j = 0; j < lod.numSurfaces; ++j ) {
			fnMdrSurfaceView_t surface;
			uint32_t k, vertexOffset;
			if ( !FnMdr_Surface( &lod, surfaceOffset, &surface ) || surface.numVerts > maxVerts ||
				surface.numTriangles > maxTriangles || surface.numBoneReferences > FN_MDR_MAX_BONES ||
				!FnMdr_AddSize( &result.expandedBytes, 1, FN_MDR_SURFACE_BYTES ) ||
				!FnMdr_AddSize( &result.expandedBytes, surface.numTriangles, 12 ) ) return 0;
			/* The back-link is ignored by the reader, then rebuilt in owned memory.
			 * Disk shader indices and surface identifiers are likewise opaque. */
			for ( k = 0; k < surface.numBoneReferences; ++k )
				if ( FnMdr_U32( surface.data + surface.boneReferences + (size_t)k * 4 ) >= result.numBones ) return 0;
			for ( k = 0; k < surface.numTriangles * 3; ++k )
				if ( FnMdr_U32( surface.data + surface.triangles + (size_t)k * 4 ) >= surface.numVerts ) return 0;
			vertexOffset = surface.vertices;
			for ( k = 0; k < surface.numVerts; ++k ) {
				uint32_t weights, next, w;
				const unsigned char *vertex = surface.data + vertexOffset;
				if ( !FnMdr_Vertex( &surface, vertexOffset, &weights, &next ) ||
					!FnMdr_Finite( vertex + 12, 2 ) || !FnMdr_AddSize( &result.expandedBytes, 1, next - vertexOffset ) ) return 0;
				/* Retail ships some vertices with three NaN normal components.
				 * Their geometry is usable; the loader supplies a finite fallback. */
				for ( w = 0; w < weights; ++w ) {
					const unsigned char *weight = vertex + FN_MDR_VERTEX_BYTES + (size_t)w * FN_MDR_WEIGHT_BYTES;
					if ( FnMdr_U32( weight ) >= result.numBones || !FnMdr_Finite( weight + 4, 4 ) ) return 0;
				}
				vertexOffset = next;
			}
			surfaceOffset += surface.bytes;
		}
		offset += lod.bytes;
	}
	*destination = result;
	return 1;
}
#endif
