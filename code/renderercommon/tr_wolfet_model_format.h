/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_MODEL_FORMAT_H
#define FNTECH3_TR_WOLFET_MODEL_FORMAT_H

/* Independent byte readers for Enemy Territory's MDMW version 3 mesh format and
 * its MDXW version 2 skeleton. Views borrow immutable storage; no disk record
 * is an engine-internal POD. Layout observations are recorded in
 * compat/wolfet_model_layout.def and checked there against every field offset.
 *
 * ET splits what the other Wolfenstein profile keeps in one file: a mesh names
 * the skeleton it is posed with, so the two are read separately and paired at
 * use. A mesh vertex is variable length -- its single declared weight is
 * followed by as many more as it claims -- so a surface must be walked rather
 * than indexed by a fixed stride.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

enum {
	FN_WOLFET_MDM_IDENT = 0x574d444du, FN_WOLFET_MDM_VERSION = 3,
	FN_WOLFET_MDX_IDENT = 0x5758444du, FN_WOLFET_MDX_VERSION = 2,
	FN_WOLFET_MDM_HEADER_BYTES = 100, FN_WOLFET_MDM_SURFACE_BYTES = 176,
	FN_WOLFET_MDM_TAG_BYTES = 128, FN_WOLFET_MDM_VERTEX_PREFIX_BYTES = 24,
	FN_WOLFET_MDM_WEIGHT_BYTES = 20, FN_WOLFET_MDM_TRIANGLE_BYTES = 12,
	FN_WOLFET_MDX_HEADER_BYTES = 96, FN_WOLFET_MDX_FRAME_BYTES = 52,
	FN_WOLFET_MDX_BONE_FRAME_BYTES = 12, FN_WOLFET_MDX_BONE_BYTES = 80,
	/* ET's own limits, from its qfiles.h. */
	FN_WOLFET_MAX_BONES = 128, FN_WOLFET_MAX_SURFACES = 32,
	FN_WOLFET_MAX_TAGS = 128, FN_WOLFET_MAX_VERTS = 6000,
	FN_WOLFET_MAX_TRIANGLES = 8192
};

typedef struct {
	const unsigned char *data;
	size_t bytes;
	uint32_t numSurfaces, numTags;
	uint32_t surfaces, tags;
	float lodScale, lodBias;
} fnWolfETMeshView_t;

typedef struct {
	const unsigned char *data;
	size_t bytes;
	uint32_t numVerts, numTriangles, numBoneReferences, minLod;
	uint32_t vertices, triangles, collapseMap, boneReferences, end;
	int shader;
} fnWolfETMeshSurfaceView_t;

typedef struct {
	const unsigned char *data;
	size_t bytes;
	uint32_t numFrames, numBones;
	uint32_t frames, bones, frameStride;
	int torsoParent;
} fnWolfETSkeletonView_t;

typedef struct { float bounds[2][3], origin[3], radius, parentOffset[3]; } fnWolfETFrame_t;
typedef struct { int parent; float torsoWeight, parentDist; uint32_t flags; } fnWolfETBone_t;
typedef struct { int angles[3], offsetAngles[2]; } fnWolfETBonePose_t;
typedef struct { uint32_t bone; float axis[3][3], offset[3]; } fnWolfETTag_t;
typedef struct { uint32_t bone; float weight, offset[3]; } fnWolfETWeight_t;
typedef struct {
	const unsigned char *weights;
	uint32_t numWeights, next;
	float normal[3], st[2];
} fnWolfETVertexView_t;

static inline uint32_t FnWolfET_U32( const unsigned char *p ) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline int32_t FnWolfET_I32( const unsigned char *p ) {
	const uint32_t value = FnWolfET_U32( p );
	return value <= INT32_MAX ? (int32_t)value : -(int32_t)( UINT32_MAX - value ) - 1;
}
static inline int FnWolfET_I16( const unsigned char *p ) {
	const unsigned value = (unsigned)p[0] | (unsigned)p[1] << 8;
	return value < 32768u ? (int)value : (int)value - 65536;
}
static inline float FnWolfET_Float( const unsigned char *p ) {
	const uint32_t bits = FnWolfET_U32( p );
	float value;
	memcpy( &value, &bits, sizeof( value ) );
	return value;
}
/* A count multiplied by a stride must be bounded before it is used, so a
 * declared count cannot wrap into a small number of bytes. */
static inline int FnWolfET_Range( size_t bytes, uint32_t offset, size_t count,
	size_t stride, size_t minimum ) {
	return stride && offset <= bytes && ( !count || offset >= minimum ) &&
		count <= ( bytes - offset ) / stride;
}
/* A fixed-width name must terminate inside its own field. */
static inline int FnWolfET_Name( const unsigned char *p, size_t field ) {
	size_t i;
	for ( i = 0; i < field; ++i ) if ( !p[i] ) return 1;
	return 0;
}

static inline int FnWolfET_Mesh( const unsigned char *data, size_t bytes, fnWolfETMeshView_t *out ) {
	fnWolfETMeshView_t view;
	if ( !data || !out || bytes < FN_WOLFET_MDM_HEADER_BYTES ) return 0;
	if ( FnWolfET_U32( data ) != FN_WOLFET_MDM_IDENT ) return 0;
	if ( FnWolfET_I32( data + 4 ) != FN_WOLFET_MDM_VERSION ) return 0;
	if ( !FnWolfET_Name( data + 8, 64 ) ) return 0;
	memset( &view, 0, sizeof( view ) );
	view.data = data;
	view.bytes = bytes;
	view.lodScale = FnWolfET_Float( data + 72 );
	view.lodBias = FnWolfET_Float( data + 76 );
	if ( FnWolfET_I32( data + 80 ) < 0 || FnWolfET_I32( data + 84 ) < 0 ||
		FnWolfET_I32( data + 88 ) < 0 || FnWolfET_I32( data + 92 ) < 0 ) return 0;
	view.numSurfaces = FnWolfET_U32( data + 80 );
	view.surfaces = FnWolfET_U32( data + 84 );
	view.numTags = FnWolfET_U32( data + 88 );
	view.tags = FnWolfET_U32( data + 92 );
	if ( view.numSurfaces > FN_WOLFET_MAX_SURFACES || view.numTags > FN_WOLFET_MAX_TAGS ) return 0;
	/* Surfaces and tags are both variable length and chained by their own end
	 * offsets, so only the first record of each is bounded here; the rest are
	 * checked as they are walked. */
	if ( view.numSurfaces && !FnWolfET_Range( bytes, view.surfaces, 1,
		FN_WOLFET_MDM_SURFACE_BYTES, FN_WOLFET_MDM_HEADER_BYTES ) ) return 0;
	if ( view.numTags && !FnWolfET_Range( bytes, view.tags, 1,
		FN_WOLFET_MDM_TAG_BYTES, FN_WOLFET_MDM_HEADER_BYTES ) ) return 0;
	*out = view;
	return 1;
}

static inline int FnWolfET_MeshVertexAt( const fnWolfETMeshSurfaceView_t *surface,
	uint32_t offset, fnWolfETVertexView_t *out );

/* Checks what the drawing path would otherwise index without bounds: a
 * triangle names a vertex of this surface, a collapse entry points backwards,
 * and every weight names a bone a skeleton could hold. A weight's bone index
 * cannot be checked against a bone count here, because an ET mesh does not name
 * the skeleton it will be posed with; the pose refuses an index its own
 * skeleton does not hold. */
static inline int FnWolfET_ValidateMeshSurface( const fnWolfETMeshSurfaceView_t *surface ) {
	uint32_t i, offset;
	if ( !surface || !surface->data ) return 0;
	for ( i = 0; i < surface->numBoneReferences; ++i )
		if ( FnWolfET_U32( surface->data + surface->boneReferences + (size_t)i * 4 ) >=
			FN_WOLFET_MAX_BONES ) return 0;
	offset = surface->vertices;
	for ( i = 0; i < surface->numVerts; ++i ) {
		fnWolfETVertexView_t vertex;
		uint32_t w;
		const uint32_t collapse = FnWolfET_U32( surface->data + surface->collapseMap + (size_t)i * 4 );
		if ( collapse >= ( i ? i : 1 ) ) return 0;
		if ( !FnWolfET_MeshVertexAt( surface, offset, &vertex ) ) return 0;
		for ( w = 0; w < vertex.numWeights; ++w ) {
			const unsigned char *p = vertex.weights + (size_t)w * FN_WOLFET_MDM_WEIGHT_BYTES;
			if ( FnWolfET_I32( p ) < 0 || FnWolfET_U32( p ) >= FN_WOLFET_MAX_BONES ) return 0;
		}
		offset = vertex.next;
	}
	for ( i = 0; i < surface->numTriangles; ++i ) {
		const unsigned char *triangle = surface->data + surface->triangles + (size_t)i * 12;
		int j;
		for ( j = 0; j < 3; ++j )
			if ( FnWolfET_U32( triangle + j * 4 ) >= surface->numVerts ) return 0;
	}
	return 1;
}

/* Reads the surface that begins at a byte offset. A surface's length depends on
 * the vertices inside it, so the next one begins at `offset + out->end`.
 * Returns 0 when any record it names would leave the file. */
static inline int FnWolfET_MeshSurfaceAt( const fnWolfETMeshView_t *view, uint32_t offset,
	fnWolfETMeshSurfaceView_t *out ) {
	fnWolfETMeshSurfaceView_t surface;
	const unsigned char *p;
	if ( !view || !view->data || !out ) return 0;
	if ( !FnWolfET_Range( view->bytes, offset, 1, FN_WOLFET_MDM_SURFACE_BYTES,
		FN_WOLFET_MDM_HEADER_BYTES ) ) return 0;
	p = view->data + offset;
	if ( !FnWolfET_Name( p + 4, 64 ) || !FnWolfET_Name( p + 68, 64 ) ) return 0;
	memset( &surface, 0, sizeof( surface ) );
	surface.data = view->data;
	surface.bytes = view->bytes;
	surface.shader = FnWolfET_I32( p + 132 );
	if ( FnWolfET_I32( p + 136 ) < 0 || FnWolfET_I32( p + 144 ) < 0 ||
		FnWolfET_I32( p + 148 ) < 0 || FnWolfET_I32( p + 152 ) < 0 ||
		FnWolfET_I32( p + 156 ) < 0 || FnWolfET_I32( p + 160 ) < 0 ||
		FnWolfET_I32( p + 164 ) < 0 || FnWolfET_I32( p + 168 ) < 0 ||
		FnWolfET_I32( p + 172 ) <= 0 ) return 0;
	surface.minLod = FnWolfET_U32( p + 136 );
	surface.numVerts = FnWolfET_U32( p + 144 );
	surface.vertices = offset + FnWolfET_U32( p + 148 );
	surface.numTriangles = FnWolfET_U32( p + 152 );
	surface.triangles = offset + FnWolfET_U32( p + 156 );
	surface.collapseMap = offset + FnWolfET_U32( p + 160 );
	surface.numBoneReferences = FnWolfET_U32( p + 164 );
	surface.boneReferences = offset + FnWolfET_U32( p + 168 );
	surface.end = FnWolfET_U32( p + 172 );
	if ( surface.numVerts > FN_WOLFET_MAX_VERTS ||
		surface.numTriangles > FN_WOLFET_MAX_TRIANGLES ||
		surface.numBoneReferences > FN_WOLFET_MAX_BONES ) return 0;
	/* Every record named by this surface must lie inside the file, and the
	 * surface's own end must not run past it either. */
	if ( !FnWolfET_Range( view->bytes, surface.triangles, surface.numTriangles,
			FN_WOLFET_MDM_TRIANGLE_BYTES, FN_WOLFET_MDM_HEADER_BYTES ) ||
		!FnWolfET_Range( view->bytes, surface.collapseMap, surface.numVerts, 4,
			FN_WOLFET_MDM_HEADER_BYTES ) ||
		!FnWolfET_Range( view->bytes, surface.boneReferences, surface.numBoneReferences, 4,
			FN_WOLFET_MDM_HEADER_BYTES ) ||
		surface.end > view->bytes - offset ) return 0;
	if ( !FnWolfET_ValidateMeshSurface( &surface ) ) return 0;
	*out = surface;
	return 1;
}

/* Walks to the surface at an index. */
static inline int FnWolfET_MeshSurface( const fnWolfETMeshView_t *view, uint32_t index,
	fnWolfETMeshSurfaceView_t *out ) {
	uint32_t offset, i;
	if ( !view || !out || index >= view->numSurfaces ) return 0;
	offset = view->surfaces;
	for ( i = 0; ; ++i ) {
		if ( !FnWolfET_MeshSurfaceAt( view, offset, out ) ) return 0;
		if ( i == index ) return 1;
		offset += out->end;
	}
}

/* Reads the vertex that begins at a byte offset. A vertex's single declared
 * weight is followed by as many more as it claims, so the next one begins at
 * `out->next`. */
static inline int FnWolfET_MeshVertexAt( const fnWolfETMeshSurfaceView_t *surface,
	uint32_t offset, fnWolfETVertexView_t *out ) {
	fnWolfETVertexView_t vertex;
	const unsigned char *p;
	uint32_t weights;
	if ( !surface || !surface->data || !out ) return 0;
	if ( !FnWolfET_Range( surface->bytes, offset, 1, FN_WOLFET_MDM_VERTEX_PREFIX_BYTES,
		FN_WOLFET_MDM_HEADER_BYTES ) ) return 0;
	p = surface->data + offset;
	if ( FnWolfET_I32( p + 20 ) <= 0 ) return 0;
	weights = FnWolfET_U32( p + 20 );
	/* A vertex may be weighted onto as many bones as the skeleton has. */
	if ( weights > FN_WOLFET_MAX_BONES ) return 0;
	if ( !FnWolfET_Range( surface->bytes, offset + FN_WOLFET_MDM_VERTEX_PREFIX_BYTES,
		weights, FN_WOLFET_MDM_WEIGHT_BYTES, FN_WOLFET_MDM_HEADER_BYTES ) ) return 0;
	memset( &vertex, 0, sizeof( vertex ) );
	vertex.normal[0] = FnWolfET_Float( p );
	vertex.normal[1] = FnWolfET_Float( p + 4 );
	vertex.normal[2] = FnWolfET_Float( p + 8 );
	vertex.st[0] = FnWolfET_Float( p + 12 );
	vertex.st[1] = FnWolfET_Float( p + 16 );
	vertex.numWeights = weights;
	vertex.weights = surface->data + offset + FN_WOLFET_MDM_VERTEX_PREFIX_BYTES;
	vertex.next = offset + FN_WOLFET_MDM_VERTEX_PREFIX_BYTES +
		weights * FN_WOLFET_MDM_WEIGHT_BYTES;
	*out = vertex;
	return 1;
}

/* Walks to the vertex at an index within a surface. */
static inline int FnWolfET_MeshVertex( const fnWolfETMeshSurfaceView_t *surface,
	uint32_t index, fnWolfETVertexView_t *out ) {
	uint32_t offset, i;
	if ( !surface || !out || index >= surface->numVerts ) return 0;
	offset = surface->vertices;
	for ( i = 0; ; ++i ) {
		if ( !FnWolfET_MeshVertexAt( surface, offset, out ) ) return 0;
		if ( i == index ) return 1;
		offset = out->next;
	}
}

static inline int FnWolfET_MeshWeight( const fnWolfETVertexView_t *vertex, uint32_t index,
	fnWolfETWeight_t *out ) {
	const unsigned char *p;
	int i;
	if ( !vertex || !vertex->weights || !out || index >= vertex->numWeights ) return 0;
	p = vertex->weights + (size_t)index * FN_WOLFET_MDM_WEIGHT_BYTES;
	if ( FnWolfET_I32( p ) < 0 ) return 0;
	out->bone = FnWolfET_U32( p );
	out->weight = FnWolfET_Float( p + 4 );
	for ( i = 0; i < 3; ++i ) out->offset[i] = FnWolfET_Float( p + 8 + i * 4 );
	return 1;
}

/* Walks to the tag at an index. Tags carry their own bone references, so like
 * surfaces they are variable length and chained by an end offset rather than
 * indexed by a fixed stride. */
static inline int FnWolfET_MeshTag( const fnWolfETMeshView_t *view, uint32_t index,
	fnWolfETTag_t *out, const char **name ) {
	uint32_t offset;
	uint32_t i;
	if ( !view || !view->data || !out || index >= view->numTags ) return 0;
	offset = view->tags;
	for ( i = 0; ; ++i ) {
		const unsigned char *p;
		uint32_t references, referenceOffset, end;
		if ( !FnWolfET_Range( view->bytes, offset, 1, FN_WOLFET_MDM_TAG_BYTES,
			FN_WOLFET_MDM_HEADER_BYTES ) ) return 0;
		p = view->data + offset;
		if ( !FnWolfET_Name( p, 64 ) ) return 0;
		if ( FnWolfET_I32( p + 100 ) < 0 || FnWolfET_I32( p + 116 ) < 0 ||
			FnWolfET_I32( p + 120 ) < 0 || FnWolfET_I32( p + 124 ) <= 0 ) return 0;
		references = FnWolfET_U32( p + 116 );
		referenceOffset = offset + FnWolfET_U32( p + 120 );
		end = FnWolfET_U32( p + 124 );
		if ( references > FN_WOLFET_MAX_BONES ) return 0;
		if ( !FnWolfET_Range( view->bytes, referenceOffset, references, 4,
				FN_WOLFET_MDM_HEADER_BYTES ) ||
			end > view->bytes - offset ) return 0;
		if ( i == index ) {
			int axis, component;
			for ( axis = 0; axis < 3; ++axis ) for ( component = 0; component < 3; ++component )
				out->axis[axis][component] = FnWolfET_Float( p + 64 + ( axis * 3 + component ) * 4 );
			out->bone = FnWolfET_U32( p + 100 );
			for ( axis = 0; axis < 3; ++axis )
				out->offset[axis] = FnWolfET_Float( p + 104 + axis * 4 );
			if ( name ) *name = (const char *)p;
			return 1;
		}
		offset += end;
	}
}

static inline int FnWolfET_Skeleton( const unsigned char *data, size_t bytes,
	fnWolfETSkeletonView_t *out ) {
	fnWolfETSkeletonView_t view;
	uint32_t i;
	if ( !data || !out || bytes < FN_WOLFET_MDX_HEADER_BYTES ) return 0;
	if ( FnWolfET_U32( data ) != FN_WOLFET_MDX_IDENT ) return 0;
	if ( FnWolfET_I32( data + 4 ) != FN_WOLFET_MDX_VERSION ) return 0;
	if ( !FnWolfET_Name( data + 8, 64 ) ) return 0;
	memset( &view, 0, sizeof( view ) );
	view.data = data;
	view.bytes = bytes;
	if ( FnWolfET_I32( data + 72 ) < 0 || FnWolfET_I32( data + 76 ) < 0 ||
		FnWolfET_I32( data + 80 ) < 0 || FnWolfET_I32( data + 84 ) < 0 ) return 0;
	view.numFrames = FnWolfET_U32( data + 72 );
	view.numBones = FnWolfET_U32( data + 76 );
	view.frames = FnWolfET_U32( data + 80 );
	view.bones = FnWolfET_U32( data + 84 );
	view.torsoParent = FnWolfET_I32( data + 88 );
	if ( view.numBones > FN_WOLFET_MAX_BONES || !view.numBones ) return 0;
	/* A frame is a bounds record followed by one compressed pose per bone. */
	view.frameStride = FN_WOLFET_MDX_FRAME_BYTES + view.numBones * FN_WOLFET_MDX_BONE_FRAME_BYTES;
	if ( !FnWolfET_Range( bytes, view.frames, view.numFrames, view.frameStride,
			FN_WOLFET_MDX_HEADER_BYTES ) ||
		!FnWolfET_Range( bytes, view.bones, view.numBones, FN_WOLFET_MDX_BONE_BYTES,
			FN_WOLFET_MDX_HEADER_BYTES ) ) return 0;
	/* The torso parent names a bone or nothing at all. */
	if ( view.torsoParent != -1 &&
		( view.torsoParent < 0 || (uint32_t)view.torsoParent >= view.numBones ) ) return 0;
	/* Every bone's parent must precede it, so no file can describe a cycle. */
	for ( i = 0; i < view.numBones; ++i ) {
		const unsigned char *p = data + view.bones + (size_t)i * FN_WOLFET_MDX_BONE_BYTES;
		const int32_t parent = FnWolfET_I32( p + 64 );
		if ( !FnWolfET_Name( p, 64 ) ) return 0;
		if ( parent != -1 && ( parent < 0 || (uint32_t)parent >= i ) ) return 0;
	}
	*out = view;
	return 1;
}

static inline int FnWolfET_Frame( const fnWolfETSkeletonView_t *view, uint32_t index,
	fnWolfETFrame_t *out ) {
	const unsigned char *p;
	int i;
	if ( !view || !view->data || !out || index >= view->numFrames ) return 0;
	p = view->data + view->frames + (size_t)index * view->frameStride;
	for ( i = 0; i < 3; ++i ) {
		out->bounds[0][i] = FnWolfET_Float( p + i * 4 );
		out->bounds[1][i] = FnWolfET_Float( p + 12 + i * 4 );
		out->origin[i] = FnWolfET_Float( p + 24 + i * 4 );
		out->parentOffset[i] = FnWolfET_Float( p + 40 + i * 4 );
	}
	out->radius = FnWolfET_Float( p + 36 );
	return 1;
}

static inline int FnWolfET_Bone( const fnWolfETSkeletonView_t *view, uint32_t index,
	fnWolfETBone_t *out, const char **name ) {
	const unsigned char *p;
	if ( !view || !view->data || !out || index >= view->numBones ) return 0;
	p = view->data + view->bones + (size_t)index * FN_WOLFET_MDX_BONE_BYTES;
	out->parent = FnWolfET_I32( p + 64 );
	out->torsoWeight = FnWolfET_Float( p + 68 );
	out->parentDist = FnWolfET_Float( p + 72 );
	out->flags = FnWolfET_U32( p + 76 );
	if ( name ) *name = (const char *)p;
	return 1;
}

static inline int FnWolfET_BonePose( const fnWolfETSkeletonView_t *view, uint32_t frame,
	uint32_t bone, fnWolfETBonePose_t *out ) {
	const unsigned char *p;
	int i;
	if ( !view || !view->data || !out || frame >= view->numFrames || bone >= view->numBones ) return 0;
	p = view->data + view->frames + (size_t)frame * view->frameStride +
		FN_WOLFET_MDX_FRAME_BYTES + (size_t)bone * FN_WOLFET_MDX_BONE_FRAME_BYTES;
	for ( i = 0; i < 3; ++i ) out->angles[i] = FnWolfET_I16( p + i * 2 );
	for ( i = 0; i < 2; ++i ) out->offsetAngles[i] = FnWolfET_I16( p + 8 + i * 2 );
	return 1;
}

#endif
