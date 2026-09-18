/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_FOLIAGE_H
#define FNTECH3_TR_WOLFET_FOLIAGE_H

/* Enemy Territory's foliage is one small mesh drawn at many origins, each with
 * a colour of its own. A shader's distancecull fades it out with distance.
 * The rules are the original ET renderer's; this arithmetic is written anew
 * and depends on nothing in the renderer, so it can be tested alone. */

/* ET's map compiler stores a foliage surface's mesh vertex count where a patch
 * keeps its height, and its instance count where a patch keeps its width. The
 * instances follow the mesh in the vertex lump, one vertex each: its position
 * is the instance's origin and its colour is the instance's colour. */
typedef struct {
	int meshVerts, instances, indexes;
} fnWolfETFoliageCounts_t;

static inline int FnWolfET_FoliageCounts( int patchWidth, int patchHeight, int numIndexes,
	int firstVert, int firstIndex, int lumpVerts, int lumpIndexes, int maxVerts, int maxIndexes,
	fnWolfETFoliageCounts_t *counts ) {
	if ( patchWidth < 0 || patchHeight <= 0 || numIndexes <= 0 || numIndexes % 3 ) return 0;
	if ( firstVert < 0 || firstIndex < 0 || lumpVerts < 0 || lumpIndexes < 0 ) return 0;
	/* Every instance is drawn as one copy of the mesh, so one copy must fit a
	 * batch, and the mesh and its instances must all lie inside the lumps. */
	if ( patchHeight > maxVerts || numIndexes > maxIndexes ) return 0;
	if ( firstVert > lumpVerts || patchHeight > lumpVerts - firstVert ||
		patchWidth > lumpVerts - firstVert - patchHeight ) return 0;
	if ( firstIndex > lumpIndexes || numIndexes > lumpIndexes - firstIndex ) return 0;
	counts->meshVerts = patchHeight;
	counts->instances = patchWidth;
	counts->indexes = numIndexes;
	return 1;
}

/* r_drawfoliage scales the mesh's height as the map loads. A negative value
 * reads as one and nothing grows past twice its authored height; zero flattens
 * the mesh, and draws nothing at all. */
static inline float FnWolfET_FoliageHeightScale( float drawFoliage ) {
	if ( drawFoliage < 0.0f ) return 1.0f;
	if ( drawFoliage > 2.0f ) return 2.0f;
	return drawFoliage;
}

/* distancecull <opaque> <transparent> <alpha>: the fourth value, kept beside
 * the three that were read, is the reciprocal of the fade's length. A fade of
 * no length leaves the shader without one. */
static inline void FnWolfET_DistanceCullFinish( float cull[4] ) {
	if ( cull[1] - cull[0] > 0 ) {
		cull[3] = 1.0f / ( cull[1] - cull[0] );
	} else {
		cull[0] = cull[1] = cull[2] = cull[3] = 0;
	}
}

static inline int FnWolfET_DistanceCullActive( const float cull[4] ) {
	return cull[1] > 0;
}

/* Depth along the view axis, widened by the horizontal field of view so a
 * zoomed view keeps its foliage for as long as it looks the same size.
 * axis[0..2] is the view's forward axis in the space of the point, axis[3]
 * the depth of that space's origin. */
static inline float FnWolfET_FoliageDepth( const float axis[4], const float point[3], float fovX ) {
	return fovX * ( 1.0f / 90.0f ) *
		( point[0] * axis[0] + point[1] * axis[1] + point[2] * axis[2] + axis[3] );
}

/* A whole surface is dropped once even its nearest edge has faded past the
 * shader's alpha. */
static inline int FnWolfET_FoliageSurfaceVisible( const float cull[4], float depth, float radius, float fovX ) {
	const float z = depth - fovX * ( 1.0f / 90.0f ) * radius;
	if ( !FnWolfET_DistanceCullActive( cull ) ) return 1;
	return ( cull[1] - z ) * cull[3] >= cull[2];
}

/* An instance's alpha byte, or -1 when it is not drawn. Every second
 * instance, and again every fourth, is treated as a quarter further away, so a
 * stand thins out gradually rather than all at once. An instance more than 64
 * units behind the eye is never drawn. */
static inline int FnWolfET_FoliageInstanceAlpha( const float cull[4], float z, unsigned index ) {
	float alpha;
	if ( z < -64.0f ) return -1;
	if ( index & 1 ) z *= 1.25f;
	if ( index & 2 ) z *= 1.25f;
	alpha = ( cull[1] - z ) * cull[3];
	if ( alpha < cull[2] ) return -1;
	/* ET stores the product in the colour's top byte; below zero it wraps,
	 * as its own byte does. */
	return alpha > 1.0f ? 255 : (int)(unsigned char)(int)( alpha * 255.0f );
}

/* Instances are tested against the view's four sides and its far plane, each
 * with 64 units of slack so nothing pops at the edge of the screen. */
static inline int FnWolfET_FoliagePlaneKeeps( const float normal[3], float dist, const float point[3] ) {
	return point[0] * normal[0] + point[1] * normal[1] + point[2] * normal[2] - dist >= -64.0f;
}

#endif
