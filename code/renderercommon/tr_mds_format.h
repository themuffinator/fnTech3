/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_FORMAT_H
#define FNTECH3_TR_MDS_FORMAT_H

/* Independent byte reader for RTCW's MDSW version 4 skeletal format.
 * Views borrow immutable storage. No disk record is an engine-internal POD.
 * Layout observations: original RTCW SP/MP and iortcw qfiles.h.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <limits.h>

enum { FN_MDS_HEADER_BYTES = 120, FN_MDS_SURFACE_BYTES = 176,
	FN_MDS_FRAME_PREFIX_BYTES = 52, FN_MDS_COMPRESSED_BONE_BYTES = 12,
	FN_MDS_BONE_BYTES = 80, FN_MDS_TAG_BYTES = 72,
	FN_MDS_VERTEX_PREFIX_BYTES = 32, FN_MDS_WEIGHT_BYTES = 20,
	FN_MDS_MAX_BONES = 128 };

typedef struct {
	const unsigned char *data;
	size_t bytes;
	uint32_t numFrames, numBones, numTags, numSurfaces, torsoParent;
	uint32_t frames, bones, tags, surfaces, frameStride;
	float lodScale, lodBias;
} fnMdsView_t;

typedef struct {
	const unsigned char *data;
	size_t bytes;
	uint32_t numBones, numVerts, numTriangles, numBoneReferences, minLod;
	uint32_t vertices, triangles, collapseMap, boneReferences;
} fnMdsSurfaceView_t;

typedef struct { float bounds[2][3], origin[3], radius, parentOffset[3]; } fnMdsFrame_t;
typedef struct { int parent; float torsoWeight, parentDist; uint32_t flags; } fnMdsBone_t;
typedef struct { int angles[3], offsetAngles[2]; } fnMdsCompressedBone_t;
typedef struct { uint32_t bone; float torsoWeight; } fnMdsTag_t;
typedef struct { uint32_t bone; float weight, offset[3]; } fnMdsWeight_t;
typedef struct {
	const unsigned char *weights;
	uint32_t numWeights, next;
	float normal[3], st[2];
} fnMdsVertexView_t;

static inline uint32_t FnMds_U32( const unsigned char *p ) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline int32_t FnMds_I32( const unsigned char *p ) {
	const uint32_t value = FnMds_U32( p );
	return value <= INT32_MAX ? (int32_t)value : -(int32_t)( UINT32_MAX - value ) - 1;
}
static inline int FnMds_I16( const unsigned char *p ) {
	const unsigned value = (unsigned)p[0] | (unsigned)p[1] << 8;
	return value < 32768u ? (int)value : (int)value - 65536;
}
static inline float FnMds_Float( const unsigned char *p ) {
	const uint32_t bits = FnMds_U32( p );
	float value;
	memcpy( &value, &bits, sizeof( value ) );
	return value;
}
static inline int FnMds_Range( size_t bytes, uint32_t offset, size_t count, size_t stride, size_t minimum ) {
	return stride && offset <= bytes && ( !count || offset >= minimum ) && count <= ( bytes - offset ) / stride;
}

static inline int FnMds_Frame( const fnMdsView_t *view, uint32_t index, fnMdsFrame_t *destination ) {
	fnMdsFrame_t result;
	const unsigned char *p;
	int i;
	if ( !view || !view->data || !destination || index >= view->numFrames ||
		view->frameStride < FN_MDS_FRAME_PREFIX_BYTES ||
		!FnMds_Range( view->bytes, view->frames, view->numFrames, view->frameStride, FN_MDS_HEADER_BYTES ) ) return 0;
	p = view->data + view->frames + (size_t)index * view->frameStride;
	for ( i = 0; i < 3; ++i ) {
		result.bounds[0][i] = FnMds_Float( p + i * 4 );
		result.bounds[1][i] = FnMds_Float( p + 12 + i * 4 );
		result.origin[i] = FnMds_Float( p + 24 + i * 4 );
		result.parentOffset[i] = FnMds_Float( p + 40 + i * 4 );
		if ( !isfinite( result.bounds[0][i] ) || !isfinite( result.bounds[1][i] ) ||
			!isfinite( result.origin[i] ) || !isfinite( result.parentOffset[i] ) ) return 0;
	}
	result.radius = FnMds_Float( p + 36 );
	if ( !isfinite( result.radius ) || result.radius < 0 ) return 0;
	*destination = result;
	return 1;
}

static inline int FnMds_Bone( const fnMdsView_t *view, uint32_t index, fnMdsBone_t *destination ) {
	fnMdsBone_t result;
	const unsigned char *p;
	if ( !view || !view->data || !destination || index >= view->numBones ||
		!FnMds_Range( view->bytes, view->bones, view->numBones, FN_MDS_BONE_BYTES, FN_MDS_HEADER_BYTES ) ) return 0;
	p = view->data + view->bones + (size_t)index * FN_MDS_BONE_BYTES;
	result.parent = FnMds_I32( p + 64 );
	result.torsoWeight = FnMds_Float( p + 68 );
	result.parentDist = FnMds_Float( p + 72 );
	result.flags = FnMds_U32( p + 76 );
	if ( !memchr( p, 0, 64 ) || result.parent < -1 ||
		( result.parent >= 0 && (uint32_t)result.parent >= view->numBones ) ||
		!isfinite( result.torsoWeight ) || !isfinite( result.parentDist ) || result.parentDist < 0 ) return 0;
	*destination = result;
	return 1;
}

static inline int FnMds_CompressedBone( const fnMdsView_t *view, uint32_t frame,
	uint32_t bone, fnMdsCompressedBone_t *destination ) {
	fnMdsCompressedBone_t result;
	const unsigned char *p;
	int i;
	if ( !view || !view->data || !destination || frame >= view->numFrames || bone >= view->numBones ||
		view->numBones > FN_MDS_MAX_BONES ||
		view->frameStride != FN_MDS_FRAME_PREFIX_BYTES + view->numBones * FN_MDS_COMPRESSED_BONE_BYTES ||
		!FnMds_Range( view->bytes, view->frames, view->numFrames, view->frameStride, FN_MDS_HEADER_BYTES ) ) return 0;
	p = view->data + view->frames + (size_t)frame * view->frameStride + FN_MDS_FRAME_PREFIX_BYTES +
		(size_t)bone * FN_MDS_COMPRESSED_BONE_BYTES;
	for ( i = 0; i < 3; ++i ) result.angles[i] = FnMds_I16( p + i * 2 );
	for ( i = 0; i < 2; ++i ) result.offsetAngles[i] = FnMds_I16( p + 8 + i * 2 );
	/* The fourth angle short is reserved; both reference renderers ignore it. */
	*destination = result;
	return 1;
}

static inline int FnMds_Tag( const fnMdsView_t *view, uint32_t index, fnMdsTag_t *destination ) {
	fnMdsTag_t result;
	const unsigned char *p;
	if ( !view || !view->data || !destination || index >= view->numTags ||
		!FnMds_Range( view->bytes, view->tags, view->numTags, FN_MDS_TAG_BYTES, FN_MDS_HEADER_BYTES ) ) return 0;
	p = view->data + view->tags + (size_t)index * FN_MDS_TAG_BYTES;
	result.bone = FnMds_U32( p + 68 );
	result.torsoWeight = FnMds_Float( p + 64 );
	if ( !memchr( p, 0, 64 ) || !isfinite( result.torsoWeight ) || result.bone >= view->numBones ) return 0;
	*destination = result;
	return 1;
}

/* Relative surface offsets are retained only after validating their entire
 * ranges. Its negative header link must point back to this same model. */
static inline int FnMds_Surface( const fnMdsView_t *view, uint32_t offset, fnMdsSurfaceView_t *destination ) {
	fnMdsSurfaceView_t result;
	const unsigned char *p;
	if ( !view || !view->data || !destination || offset > INT32_MAX ||
		!FnMds_Range( view->bytes, offset, 1, FN_MDS_SURFACE_BYTES, FN_MDS_HEADER_BYTES ) ) return 0;
	p = view->data + offset;
	result.data = p; result.bytes = FnMds_U32( p + 172 ); result.numBones = view->numBones;
	if ( result.bytes < FN_MDS_SURFACE_BYTES || result.bytes > view->bytes - offset ||
		FnMds_I32( p + 140 ) != -(int32_t)offset || !memchr( p + 4, 0, 64 ) || !memchr( p + 68, 0, 64 ) ) return 0;
	/* The disk surface discriminator and shader index are opaque, not handles. */
	result.minLod = FnMds_U32( p + 136 );
	result.numVerts = FnMds_U32( p + 144 ); result.vertices = FnMds_U32( p + 148 );
	result.numTriangles = FnMds_U32( p + 152 ); result.triangles = FnMds_U32( p + 156 );
	result.collapseMap = FnMds_U32( p + 160 );
	result.numBoneReferences = FnMds_U32( p + 164 ); result.boneReferences = FnMds_U32( p + 168 );
	if ( result.minLod > result.numVerts || result.numVerts > INT_MAX || result.numTriangles > INT_MAX ||
		result.numBoneReferences > FN_MDS_MAX_BONES ||
		!FnMds_Range( result.bytes, result.vertices, result.numVerts, FN_MDS_VERTEX_PREFIX_BYTES, FN_MDS_SURFACE_BYTES ) ||
		!FnMds_Range( result.bytes, result.triangles, result.numTriangles, 12, FN_MDS_SURFACE_BYTES ) ||
		!FnMds_Range( result.bytes, result.collapseMap, result.numVerts, 4, FN_MDS_SURFACE_BYTES ) ||
		!FnMds_Range( result.bytes, result.boneReferences, result.numBoneReferences, 4, FN_MDS_SURFACE_BYTES ) ) return 0;
	*destination = result;
	return 1;
}

/* Walk variable-length vertices once: begin at surface.vertices, then next.
 * This avoids quadratic rescans and supports unaligned exporter layouts. */
static inline int FnMds_Vertex( const fnMdsSurfaceView_t *surface, uint32_t offset, fnMdsVertexView_t *destination ) {
	fnMdsVertexView_t result;
	const unsigned char *p;
	int i;
	if ( !surface || !surface->data || !destination ||
		!FnMds_Range( surface->bytes, offset, 1, FN_MDS_VERTEX_PREFIX_BYTES, FN_MDS_SURFACE_BYTES ) ) return 0;
	p = surface->data + offset;
	result.numWeights = FnMds_U32( p + 20 );
	if ( !result.numWeights || !FnMds_Range( surface->bytes, offset + FN_MDS_VERTEX_PREFIX_BYTES,
		result.numWeights, FN_MDS_WEIGHT_BYTES, FN_MDS_SURFACE_BYTES ) ) return 0;
	result.weights = p + FN_MDS_VERTEX_PREFIX_BYTES;
	result.next = offset + FN_MDS_VERTEX_PREFIX_BYTES + result.numWeights * FN_MDS_WEIGHT_BYTES;
	for ( i = 0; i < 3; ++i ) {
		result.normal[i] = FnMds_Float( p + i * 4 );
		if ( !isfinite( result.normal[i] ) ) return 0;
	}
	for ( i = 0; i < 2; ++i ) {
		result.st[i] = FnMds_Float( p + 12 + i * 4 );
		if ( !isfinite( result.st[i] ) ) return 0;
	}
	/* fixedParent/fixedDist are unused exporter words in the reference backend. */
	*destination = result;
	return 1;
}

static inline int FnMds_Weight( const fnMdsVertexView_t *vertex, uint32_t index, fnMdsWeight_t *destination ) {
	fnMdsWeight_t result;
	const unsigned char *p;
	int i;
	if ( !vertex || !vertex->weights || !destination || index >= vertex->numWeights ) return 0;
	p = vertex->weights + (size_t)index * FN_MDS_WEIGHT_BYTES;
	result.bone = FnMds_U32( p ); result.weight = FnMds_Float( p + 4 );
	if ( !isfinite( result.weight ) ) return 0;
	for ( i = 0; i < 3; ++i ) {
		result.offset[i] = FnMds_Float( p + 8 + i * 4 );
		if ( !isfinite( result.offset[i] ) ) return 0;
	}
	*destination = result;
	return 1;
}

static inline int FnMds_ValidateSurface( const fnMdsSurfaceView_t *surface ) {
	unsigned char present[FN_MDS_MAX_BONES] = {0};
	uint32_t i, offset = surface->vertices;
	if ( !surface->numBones || surface->numBones > FN_MDS_MAX_BONES ) return 0;
	for ( i = 0; i < surface->numBoneReferences; ++i ) {
		const uint32_t bone = FnMds_U32( surface->data + surface->boneReferences + (size_t)i * 4 );
		if ( bone >= surface->numBones ) return 0;
		present[bone] = 1;
	}
	for ( i = 0; i < surface->numVerts; ++i ) {
		fnMdsVertexView_t vertex;
		uint32_t w, collapse = FnMds_U32( surface->data + surface->collapseMap + (size_t)i * 4 );
		if ( collapse >= ( i ? i : 1 ) || !FnMds_Vertex( surface, offset, &vertex ) ) return 0;
		for ( w = 0; w < vertex.numWeights; ++w ) {
			fnMdsWeight_t weight;
			/* Indices name global bones, despite the original header comment.
			 * Do not remap them through the surface's bone-reference array. */
			if ( !FnMds_Weight( &vertex, w, &weight ) || weight.bone >= surface->numBones || !present[weight.bone] ) return 0;
		}
		offset = vertex.next;
	}
	for ( i = 0; i < surface->numTriangles; ++i ) {
		const unsigned char *triangle = surface->data + surface->triangles + (size_t)i * 12;
		int j;
		for ( j = 0; j < 3; ++j ) if ( FnMds_U32( triangle + j * 4 ) >= surface->numVerts ) return 0;
	}
	return 1;
}

static inline int FnMds_Open( const void *data, size_t bytes, fnMdsView_t *destination ) {
	fnMdsView_t result;
	const unsigned char *p = (const unsigned char *)data;
	int parents[FN_MDS_MAX_BONES];
	uint32_t i, offset;
	if ( !p || !destination || bytes < FN_MDS_HEADER_BYTES ||
		FnMds_U32( p ) != UINT32_C(0x5753444d) || FnMds_U32( p + 4 ) != 4 ) return 0;
	result.data = p; result.bytes = FnMds_U32( p + 116 );
	result.lodScale = FnMds_Float( p + 72 ); result.lodBias = FnMds_Float( p + 76 );
	result.numFrames = FnMds_U32( p + 80 ); result.numBones = FnMds_U32( p + 84 );
	result.frames = FnMds_U32( p + 88 ); result.bones = FnMds_U32( p + 92 ); result.torsoParent = FnMds_U32( p + 96 );
	result.numSurfaces = FnMds_U32( p + 100 ); result.surfaces = FnMds_U32( p + 104 );
	result.numTags = FnMds_U32( p + 108 ); result.tags = FnMds_U32( p + 112 );
	if ( result.bytes < FN_MDS_HEADER_BYTES || result.bytes > bytes || result.bytes > INT_MAX ||
		!result.numFrames || result.numFrames > INT_MAX || !result.numBones || result.numBones > FN_MDS_MAX_BONES ||
		result.torsoParent >= result.numBones || result.numSurfaces > INT_MAX || result.numTags > INT_MAX ||
		!memchr( p + 8, 0, 64 ) || !isfinite( result.lodScale ) || !isfinite( result.lodBias ) ) return 0;
	result.frameStride = FN_MDS_FRAME_PREFIX_BYTES + result.numBones * FN_MDS_COMPRESSED_BONE_BYTES;
	if ( !FnMds_Range( result.bytes, result.frames, result.numFrames, result.frameStride, FN_MDS_HEADER_BYTES ) ||
		!FnMds_Range( result.bytes, result.bones, result.numBones, FN_MDS_BONE_BYTES, FN_MDS_HEADER_BYTES ) ||
		!FnMds_Range( result.bytes, result.tags, result.numTags, FN_MDS_TAG_BYTES, FN_MDS_HEADER_BYTES ) ||
		!FnMds_Range( result.bytes, result.surfaces, result.numSurfaces, FN_MDS_SURFACE_BYTES, FN_MDS_HEADER_BYTES ) ) return 0;
	for ( i = 0; i < result.numFrames; ++i ) { fnMdsFrame_t frame; if ( !FnMds_Frame( &result, i, &frame ) ) return 0; }
	for ( i = 0; i < result.numBones; ++i ) {
		fnMdsBone_t bone;
		if ( !FnMds_Bone( &result, i, &bone ) ) return 0;
		parents[i] = bone.parent;
	}
	/* A bounded ancestry walk permits forward references and multiple roots,
	 * while rejecting cycles before any animation recursion or caching. */
	for ( i = 0; i < result.numBones; ++i ) {
		int bone = (int)i;
		uint32_t depth = 0;
		while ( bone >= 0 ) { if ( depth++ >= result.numBones ) return 0; bone = parents[bone]; }
	}
	for ( i = 0; i < result.numTags; ++i ) { fnMdsTag_t tag; if ( !FnMds_Tag( &result, i, &tag ) ) return 0; }
	offset = result.surfaces;
	for ( i = 0; i < result.numSurfaces; ++i ) {
		fnMdsSurfaceView_t surface;
		if ( !FnMds_Surface( &result, offset, &surface ) || !FnMds_ValidateSurface( &surface ) ) return 0;
		offset += (uint32_t)surface.bytes;
	}
	*destination = result;
	return 1;
}

#endif
