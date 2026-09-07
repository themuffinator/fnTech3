/*
 * fnTech3 MDR ray-tracing geometry helpers (GPL-2.0-or-later).
 *
 * Operate on the renderer-owned, decompressed MDR representation.  Bone
 * interpolation and direct bone indices follow RB_MDRSurfaceAnim; this is
 * deliberately not a second on-disk MDR loader.  No Vulkan state is needed.
 */
#ifndef RTX_MDR_GEOMETRY_H
#define RTX_MDR_GEOMETRY_H

#include <stddef.h>
#include <float.h>

static qboolean RTX_MDRRange( size_t bytes, int offset, int count, size_t stride )
{
	return ( offset >= 0 && count >= 0 && stride > 0 &&
		(size_t)offset <= bytes && (size_t)count <= ( bytes - (size_t)offset ) / stride )
		? qtrue : qfalse;
}

static qboolean RTX_MDRFinite( float value )
{
	return ( value >= -FLT_MAX && value <= FLT_MAX ) ? qtrue : qfalse;
}

static qboolean RTX_MDRPreparePose( const mdrHeader_t *header, size_t allocationBytes,
	int frame, int oldFrame, qboolean wrapFrames, float backlerp,
	mdrBone_t bones[MDR_MAX_BONES] )
{
	size_t frameBytes;
	const mdrFrame_t *current;
	const mdrFrame_t *previous;
	int i, row, column;

	if ( !header || !bones || allocationBytes < sizeof( *header ) ||
		header->ofsEnd < (int)sizeof( *header ) || (size_t)header->ofsEnd > allocationBytes ||
		header->numBones < 1 || header->numBones > MDR_MAX_BONES || header->numFrames < 1 ||
		header->ofsFrames < (int)sizeof( *header ) || header->ofsFrames % sizeof( float ) != 0 ) {
		return qfalse;
	}
	frameBytes = offsetof( mdrFrame_t, bones ) + (size_t)header->numBones * sizeof( mdrBone_t );
	if ( !RTX_MDRRange( (size_t)header->ofsEnd, header->ofsFrames, header->numFrames, frameBytes ) ) {
		return qfalse;
	}
	if ( wrapFrames ) {
		frame %= header->numFrames;
		oldFrame %= header->numFrames;
	}
	/* Match raster validation: either invalid frame resets both to frame zero. */
	if ( frame < 0 || oldFrame < 0 || frame >= header->numFrames || oldFrame >= header->numFrames ) {
		frame = oldFrame = 0;
	}
	if ( !RTX_MDRFinite( backlerp ) || backlerp < 0.0f || frame == oldFrame ) {
		backlerp = 0.0f;
	} else if ( backlerp > 1.0f ) {
		backlerp = 1.0f;
	}
	current = (const mdrFrame_t *)( (const byte *)header + header->ofsFrames + (size_t)frame * frameBytes );
	previous = (const mdrFrame_t *)( (const byte *)header + header->ofsFrames + (size_t)oldFrame * frameBytes );
	for ( i = 0; i < header->numBones; i++ ) {
		for ( row = 0; row < 3; row++ ) {
			for ( column = 0; column < 4; column++ ) {
				float value = current->bones[i].matrix[row][column];
				if ( backlerp > 0.0f ) {
					value = value * ( 1.0f - backlerp ) + previous->bones[i].matrix[row][column] * backlerp;
				}
				if ( !RTX_MDRFinite( value ) ) {
					return qfalse;
				}
				bones[i].matrix[row][column] = value;
			}
		}
	}
	return qtrue;
}

static const mdrLOD_t *RTX_MDRGetLOD( const mdrHeader_t *header, int lodIndex )
{
	size_t offset;
	int i;

	/* The caller must first validate the model through RTX_MDRPreparePose. */
	if ( !header || header->numLODs < 1 ||
		header->numLODs > header->ofsEnd / (int)sizeof( mdrLOD_t ) ||
		header->ofsLODs < (int)sizeof( *header ) || header->ofsLODs % sizeof( float ) != 0 ) {
		return NULL;
	}
	if ( lodIndex < 0 ) lodIndex = 0;
	if ( lodIndex >= header->numLODs ) lodIndex = header->numLODs - 1;
	offset = (size_t)header->ofsLODs;
	for ( i = 0; i <= lodIndex; i++ ) {
		const mdrLOD_t *lod;
		if ( offset > (size_t)header->ofsEnd || sizeof( *lod ) > (size_t)header->ofsEnd - offset ) {
			return NULL;
		}
		lod = (const mdrLOD_t *)( (const byte *)header + offset );
		if ( lod->ofsEnd < (int)sizeof( *lod ) || lod->ofsEnd % sizeof( float ) != 0 ||
			(size_t)lod->ofsEnd > (size_t)header->ofsEnd - offset ||
			lod->ofsSurfaces < (int)sizeof( *lod ) || lod->ofsSurfaces % sizeof( float ) != 0 ||
			!RTX_MDRRange( (size_t)lod->ofsEnd, lod->ofsSurfaces, lod->numSurfaces, sizeof( mdrSurface_t ) ) ) {
			return NULL;
		}
		if ( i == lodIndex ) return lod;
		offset += (size_t)lod->ofsEnd;
	}
	return NULL;
}

static qboolean RTX_MDRValidateSurface( const mdrSurface_t *surface, size_t availableBytes, int numBones )
{
	size_t offset;
	int i, j;

	if ( !surface || availableBytes < sizeof( *surface ) || numBones < 1 || numBones > MDR_MAX_BONES ||
		surface->ofsEnd < (int)sizeof( *surface ) || surface->ofsEnd % sizeof( float ) != 0 ||
		(size_t)surface->ofsEnd > availableBytes || surface->ofsVerts < (int)sizeof( *surface ) ||
		surface->ofsVerts % sizeof( float ) != 0 || surface->ofsTriangles % sizeof( float ) != 0 ||
		surface->ofsTriangles < surface->ofsVerts ||
		!RTX_MDRRange( (size_t)surface->ofsTriangles, surface->ofsVerts, surface->numVerts,
			offsetof( mdrVertex_t, weights ) ) ||
		!RTX_MDRRange( (size_t)surface->ofsEnd, surface->ofsTriangles, surface->numTriangles,
			sizeof( mdrTriangle_t ) ) ) {
		return qfalse;
	}
	offset = (size_t)surface->ofsVerts;
	for ( i = 0; i < surface->numVerts; i++ ) {
		const mdrVertex_t *vertex;
		size_t weightsOffset;
		if ( offset > (size_t)surface->ofsTriangles ||
			offsetof( mdrVertex_t, weights ) > (size_t)surface->ofsTriangles - offset ) {
			return qfalse;
		}
		vertex = (const mdrVertex_t *)( (const byte *)surface + offset );
		weightsOffset = offset + offsetof( mdrVertex_t, weights );
		if ( vertex->numWeights < 1 ||
			(size_t)vertex->numWeights > ( (size_t)surface->ofsTriangles - weightsOffset ) / sizeof( mdrWeight_t ) ) {
			return qfalse;
		}
		for ( j = 0; j < 3; j++ ) {
			if ( !RTX_MDRFinite( vertex->normal[j] ) ) return qfalse;
		}
		if ( !RTX_MDRFinite( vertex->texCoords[0] ) || !RTX_MDRFinite( vertex->texCoords[1] ) ) return qfalse;
		for ( j = 0; j < vertex->numWeights; j++ ) {
			const mdrWeight_t *weight = &vertex->weights[j];
			if ( weight->boneIndex < 0 || weight->boneIndex >= numBones ||
				!RTX_MDRFinite( weight->boneWeight ) || weight->boneWeight < 0.0f ||
				!RTX_MDRFinite( weight->offset[0] ) || !RTX_MDRFinite( weight->offset[1] ) ||
				!RTX_MDRFinite( weight->offset[2] ) ) {
				return qfalse;
			}
		}
		offset = weightsOffset + (size_t)vertex->numWeights * sizeof( mdrWeight_t );
	}
	return qtrue;
}

static qboolean RTX_MDRSkinVertex( const mdrVertex_t *vertex, const mdrBone_t *bones,
	vec3_t position, vec3_t normal )
{
	int row, i;
	/* The caller has validated the complete surface and its bone indices. */
	for ( row = 0; row < 3; row++ ) {
		float p = 0.0f, n = 0.0f;
		for ( i = 0; i < vertex->numWeights; i++ ) {
			const mdrWeight_t *weight = &vertex->weights[i];
			const float *matrix = bones[weight->boneIndex].matrix[row];
			p += weight->boneWeight * ( matrix[0] * weight->offset[0] + matrix[1] * weight->offset[1] +
				matrix[2] * weight->offset[2] + matrix[3] );
			n += weight->boneWeight * ( matrix[0] * vertex->normal[0] + matrix[1] * vertex->normal[1] +
				matrix[2] * vertex->normal[2] );
		}
		if ( !RTX_MDRFinite( p ) || !RTX_MDRFinite( n ) ) return qfalse;
		position[row] = p;
		normal[row] = n;
	}
	return qtrue;
}

#endif
