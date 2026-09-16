/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDC_FORMAT_H
#define FNTECH3_TR_MDC_FORMAT_H

/* Independent byte reader for RTCW's IDPC version 2 format. It borrows immutable
 * input storage and never exposes an on-disk record as an engine object.
 * Layout/encoding observations: id Software RTCW SP/MP qfiles.h and iortcw.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <limits.h>

enum { FN_MDC_HEADER_BYTES = 112, FN_MDC_SURFACE_BYTES = 124,
	FN_MDC_FRAME_BYTES = 56, FN_MDC_TAG_BYTES = 12, FN_MDC_SHADER_BYTES = 68 };

typedef struct {
	const unsigned char *data;
	size_t bytes;
	uint32_t numFrames, numTags, numSurfaces;
	uint32_t frames, tagNames, tags, surfaces;
} fnMdcView_t;

typedef struct {
	const unsigned char *data;
	size_t bytes;
	uint32_t numFrames, numBaseFrames, numCompFrames, numShaders, numVerts, numTriangles;
	uint32_t triangles, shaders, st, baseVertices, compVertices, baseFrames, compFrames;
} fnMdcSurfaceView_t;

typedef struct {
	float bounds[2][3], origin[3], radius;
} fnMdcFrame_t;

typedef struct {
	float xyz[3];
	float base[3], offset[3]; // Retained separately for RTCW's interpolation order.
	/* A base vertex uses the MD3 two-byte normal. A compressed vertex uses
	 * the MDC eight-bit direction dictionary, not an MD3 normal. */
	uint16_t normal;
	int compressedNormal;
} fnMdcVertex_t;

static inline uint32_t FnMdc_U32( const unsigned char *p ) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline int FnMdc_I16( const unsigned char *p ) {
	const unsigned value = (unsigned)p[0] | (unsigned)p[1] << 8;
	return value < 32768u ? (int)value : (int)value - 65536;
}

static inline float FnMdc_Float( const unsigned char *p ) {
	const uint32_t bits = FnMdc_U32( p );
	float value;
	memcpy( &value, &bits, sizeof( value ) );
	return value;
}

static inline int FnMdc_Range( size_t bytes, uint32_t offset, size_t count, size_t stride, size_t minimum ) {
	if ( !stride || offset > bytes || ( count && offset < minimum ) ) return 0;
	return count <= ( bytes - offset ) / stride;
}

static inline int FnMdc_Frame( const fnMdcView_t *view, uint32_t frame, fnMdcFrame_t *destination ) {
	fnMdcFrame_t result;
	const unsigned char *p;
	int i;
	if ( !view || !view->data || !destination || frame >= view->numFrames ||
		!FnMdc_Range( view->bytes, view->frames, view->numFrames, FN_MDC_FRAME_BYTES, FN_MDC_HEADER_BYTES ) ) return 0;
	p = view->data + view->frames + (size_t)frame * FN_MDC_FRAME_BYTES;
	for ( i = 0; i < 3; ++i ) {
		result.bounds[0][i] = FnMdc_Float( p + i * 4 );
		result.bounds[1][i] = FnMdc_Float( p + 12 + i * 4 );
		result.origin[i] = FnMdc_Float( p + 24 + i * 4 );
		/* Retail exporters use inverted bounds for tag-only models and some
		 * visible map objects. Preserve these finite sentinel values. */
		if ( !isfinite( result.bounds[0][i] ) || !isfinite( result.bounds[1][i] ) ||
			!isfinite( result.origin[i] ) ) return 0;
	}
	result.radius = FnMdc_Float( p + 36 );
	if ( !isfinite( result.radius ) || result.radius < 0 ) return 0;
	*destination = result;
	return 1;
}

/* offset is relative to the model. Advance by destination->bytes to walk the
 * surface chain; the root validator does this once in linear time. */
static inline int FnMdc_Surface( const fnMdcView_t *view, uint32_t offset, fnMdcSurfaceView_t *destination ) {
	fnMdcSurfaceView_t result;
	const unsigned char *p;
	if ( !view || !view->data || !destination ||
		!FnMdc_Range( view->bytes, offset, 1, FN_MDC_SURFACE_BYTES, FN_MDC_HEADER_BYTES ) ) return 0;
	p = view->data + offset;
	result.data = p;
	result.bytes = FnMdc_U32( p + 120 );
	if ( result.bytes < FN_MDC_SURFACE_BYTES || result.bytes > view->bytes - offset ||
		!memchr( p + 4, 0, 64 ) ) return 0;
	/* The retail writer stores its internal surface discriminator (7), not
	 * an IDPC magic here. Treat this unused disk word as opaque. */
	result.numFrames = view->numFrames;
	result.numCompFrames = FnMdc_U32( p + 72 );
	result.numBaseFrames = FnMdc_U32( p + 76 );
	result.numShaders = FnMdc_U32( p + 80 );
	result.numVerts = FnMdc_U32( p + 84 );
	result.numTriangles = FnMdc_U32( p + 88 );
	if ( !result.numBaseFrames || result.numBaseFrames > INT_MAX || result.numCompFrames > INT_MAX ||
		result.numShaders > INT_MAX || result.numVerts > INT_MAX || result.numTriangles > INT_MAX ) return 0;
	result.triangles = FnMdc_U32( p + 92 );
	result.shaders = FnMdc_U32( p + 96 );
	result.st = FnMdc_U32( p + 100 );
	result.baseVertices = FnMdc_U32( p + 104 );
	result.compVertices = FnMdc_U32( p + 108 );
	result.baseFrames = FnMdc_U32( p + 112 );
	result.compFrames = FnMdc_U32( p + 116 );
	if ( !FnMdc_Range( result.bytes, result.triangles, result.numTriangles, 12, FN_MDC_SURFACE_BYTES ) ||
		!FnMdc_Range( result.bytes, result.shaders, result.numShaders, FN_MDC_SHADER_BYTES, FN_MDC_SURFACE_BYTES ) ||
		!FnMdc_Range( result.bytes, result.st, result.numVerts, 8, FN_MDC_SURFACE_BYTES ) ||
		!FnMdc_Range( result.bytes, result.baseFrames, result.numFrames, 2, FN_MDC_SURFACE_BYTES ) ||
		!FnMdc_Range( result.bytes, result.compFrames, result.numFrames, 2, FN_MDC_SURFACE_BYTES ) ) return 0;
	/* Divide before multiplying; hostile frame/vertex counts must not wrap on
	 * x86. A frame stride of zero is legitimate for an empty surface. */
	if ( result.numVerts > result.bytes / 8 ||
		!FnMdc_Range( result.bytes, result.baseVertices, result.numVerts ? result.numBaseFrames : 0,
			result.numVerts ? (size_t)result.numVerts * 8 : 1, FN_MDC_SURFACE_BYTES ) ||
		!FnMdc_Range( result.bytes, result.compVertices, result.numVerts ? result.numCompFrames : 0,
			result.numVerts ? (size_t)result.numVerts * 4 : 1, FN_MDC_SURFACE_BYTES ) ) return 0;
	*destination = result;
	return 1;
}

static inline int FnMdc_ValidateSurface( const fnMdcSurfaceView_t *surface ) {
	uint32_t i;
	for ( i = 0; i < surface->numFrames; ++i ) {
		const int base = FnMdc_I16( surface->data + surface->baseFrames + (size_t)i * 2 );
		const int compressed = FnMdc_I16( surface->data + surface->compFrames + (size_t)i * 2 );
		if ( base < 0 || (uint32_t)base >= surface->numBaseFrames || compressed < -1 ||
			( compressed >= 0 && (uint32_t)compressed >= surface->numCompFrames ) ) return 0;
	}
	for ( i = 0; i < surface->numTriangles; ++i ) {
		const unsigned char *triangle = surface->data + surface->triangles + (size_t)i * 12;
		int j;
		for ( j = 0; j < 3; ++j ) if ( FnMdc_U32( triangle + j * 4 ) >= surface->numVerts ) return 0;
	}
	for ( i = 0; i < surface->numVerts; ++i ) {
		const unsigned char *st = surface->data + surface->st + (size_t)i * 8;
		if ( !isfinite( FnMdc_Float( st ) ) || !isfinite( FnMdc_Float( st + 4 ) ) ) return 0;
	}
	for ( i = 0; i < surface->numShaders; ++i )
		if ( !memchr( surface->data + surface->shaders + (size_t)i * FN_MDC_SHADER_BYTES, 0, 64 ) ) return 0;
	return 1;
}

static inline int FnMdc_Open( const void *data, size_t bytes, fnMdcView_t *destination ) {
	fnMdcView_t result;
	const unsigned char *p = (const unsigned char *)data;
	uint32_t i, offset;
	if ( !p || !destination || bytes < FN_MDC_HEADER_BYTES ||
		FnMdc_U32( p ) != UINT32_C(0x43504449) || FnMdc_U32( p + 4 ) != 2 ) return 0;
	result.data = p;
	result.bytes = FnMdc_U32( p + 108 );
	result.numFrames = FnMdc_U32( p + 76 );
	result.numTags = FnMdc_U32( p + 80 );
	result.numSurfaces = FnMdc_U32( p + 84 );
	result.frames = FnMdc_U32( p + 92 );
	result.tagNames = FnMdc_U32( p + 96 );
	result.tags = FnMdc_U32( p + 100 );
	result.surfaces = FnMdc_U32( p + 104 );
	if ( result.bytes < FN_MDC_HEADER_BYTES || result.bytes > bytes || result.bytes > INT_MAX ||
		!result.numFrames || result.numFrames > INT_MAX || result.numTags > INT_MAX || result.numSurfaces > INT_MAX ||
		FnMdc_U32( p + 88 ) > INT_MAX || !memchr( p + 8, 0, 64 ) ||
		!FnMdc_Range( result.bytes, result.frames, result.numFrames, FN_MDC_FRAME_BYTES, FN_MDC_HEADER_BYTES ) ||
		!FnMdc_Range( result.bytes, result.tagNames, result.numTags, 64, FN_MDC_HEADER_BYTES ) ||
		!FnMdc_Range( result.bytes, result.surfaces, result.numSurfaces, FN_MDC_SURFACE_BYTES, FN_MDC_HEADER_BYTES ) ) return 0;
	if ( result.numTags > result.bytes / FN_MDC_TAG_BYTES ||
		!FnMdc_Range( result.bytes, result.tags, result.numTags ? result.numFrames : 0,
			result.numTags ? (size_t)result.numTags * FN_MDC_TAG_BYTES : 1, FN_MDC_HEADER_BYTES ) ) return 0;
	for ( i = 0; i < result.numFrames; ++i ) {
		fnMdcFrame_t frame;
		if ( !FnMdc_Frame( &result, i, &frame ) ) return 0;
	}
	for ( i = 0; i < result.numTags; ++i )
		if ( !memchr( p + result.tagNames + (size_t)i * 64, 0, 64 ) ) return 0;
	offset = result.surfaces;
	for ( i = 0; i < result.numSurfaces; ++i ) {
		fnMdcSurfaceView_t surface;
		if ( !FnMdc_Surface( &result, offset, &surface ) || !FnMdc_ValidateSurface( &surface ) ) return 0;
		offset += (uint32_t)surface.bytes; // Bounded by result.bytes, which is <= INT_MAX.
	}
	*destination = result;
	return 1;
}

/* Readers below consume a surface/model returned by the validating functions
 * above. The caller must keep its backing bytes alive and immutable. */
static inline int FnMdc_Vertex( const fnMdcSurfaceView_t *surface, uint32_t frame,
	uint32_t vertex, fnMdcVertex_t *destination ) {
	fnMdcVertex_t result;
	const unsigned char *base;
	int baseFrame, compFrame, i;
	if ( !surface || !surface->data || !destination || frame >= surface->numFrames || vertex >= surface->numVerts ) return 0;
	baseFrame = FnMdc_I16( surface->data + surface->baseFrames + (size_t)frame * 2 );
	compFrame = FnMdc_I16( surface->data + surface->compFrames + (size_t)frame * 2 );
	if ( baseFrame < 0 || (uint32_t)baseFrame >= surface->numBaseFrames || compFrame < -1 ||
		( compFrame >= 0 && (uint32_t)compFrame >= surface->numCompFrames ) ) return 0;
	base = surface->data + surface->baseVertices + ( (size_t)baseFrame * surface->numVerts + vertex ) * 8;
	for ( i = 0; i < 3; ++i ) {
		result.base[i] = result.xyz[i] = FnMdc_I16( base + i * 2 ) * (1.0f / 64.0f);
		result.offset[i] = 0;
	}
	result.normal = (uint16_t)( (unsigned)base[6] | (unsigned)base[7] << 8 );
	result.compressedNormal = compFrame >= 0;
	if ( compFrame >= 0 ) {
		const uint32_t packed = FnMdc_U32( surface->data + surface->compVertices +
			( (size_t)compFrame * surface->numVerts + vertex ) * 4 );
		for ( i = 0; i < 3; ++i ) {
			result.offset[i] = (float)( ( (int)( ( packed >> ( i * 8 ) ) & 255u ) - 127 ) * 0.05 );
			result.xyz[i] += result.offset[i];
		}
		result.normal = (uint16_t)( packed >> 24 );
	}
	*destination = result;
	return 1;
}

static inline int FnMdc_Tag( const fnMdcView_t *view, uint32_t frame, uint32_t tag,
	float origin[3], float angles[3] ) {
	const unsigned char *p;
	float xyz[3], rotation[3];
	int i;
	if ( !view || !view->data || !origin || !angles || frame >= view->numFrames || tag >= view->numTags ) return 0;
	p = view->data + view->tags + ( (size_t)frame * view->numTags + tag ) * FN_MDC_TAG_BYTES;
	for ( i = 0; i < 3; ++i ) {
		xyz[i] = FnMdc_I16( p + i * 2 ) * (1.0f / 64.0f);
		rotation[i] = (float)( FnMdc_I16( p + 6 + i * 2 ) * (360.0 / 32700.0) );
	}
	memcpy( origin, xyz, sizeof( xyz ) );
	memcpy( angles, rotation, sizeof( rotation ) );
	return 1;
}

#endif
