/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_DECAL_H
#define FNTECH3_TR_WOLFET_DECAL_H

/* Enemy Territory's decals: a module describes a projector -- a quad or a
 * triangle swept along a direction, or a sphere -- and every map surface it
 * touches that frame takes a clipped, textured, coloured copy of it that stays
 * until it has faded. The other profiles mark surfaces through their own
 * fragment interface; this is a different contract and shares nothing with it.
 *
 * Only the arithmetic lives here: building a projector, clipping a polygon to
 * it, colouring the result and ageing it. Which surfaces are touched, where a
 * decal is kept and how it is drawn are the renderer's business. The caller
 * supplies the engine's PlaneFromPoints before this header. */

enum {
	/* ET's own limits. A polygon clipped by six planes has at most ten sides. */
	FN_WOLFET_MAX_DECAL_VERTS = 10,
	FN_WOLFET_MAX_DECAL_PROJECTORS = 32,
	FN_WOLFET_MAX_WORLD_DECALS = 1024,
	FN_WOLFET_MAX_ENTITY_DECALS = 128,
	FN_WOLFET_MAX_DECALS = 1024
};

typedef struct {
	/* A shader handle; below zero once the projector is culled for the rest of
	 * its scene, where ET clears its shader pointer. */
	int shader;
	unsigned char color[4];
	int fadeStartTime, fadeEndTime;
	float mins[3], maxs[3], center[3];
	float radius, radius2;
	int omnidirectional;
	/* Five planes for a triangle, six for a quad or a sphere. */
	int numPlanes;
	float planes[6][4];
	/* One texture projection per axis; a directional projector uses the first. */
	float texMat[3][2][4];
} fnWolfETDecalProjector_t;

typedef struct {
	float xyz[3], st[2];
	unsigned char modulate[4];
} fnWolfETDecalVert_t;

typedef struct {
	int inUse;
	int shader;
	int fadeStartTime, fadeEndTime;
	int numVerts;
	fnWolfETDecalVert_t verts[FN_WOLFET_MAX_DECAL_VERTS];
	/* The renderer's own: the map surface the decal lies on, whose visibility
	 * decides whether the decal is drawn, and that surface's fog. */
	const void *parent;
	int fogIndex;
} fnWolfETDecal_t;

static inline float FnWolfET_DecalDot( const float *a, const float *b ) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline float FnWolfET_DecalNormalize( const float in[3], float out[3] ) {
	const float length = (float)sqrt( in[0] * in[0] + in[1] * in[1] + in[2] * in[2] );
	if ( length > 0 ) {
		const float inverse = 1.0f / length;
		out[0] = in[0] * inverse; out[1] = in[1] * inverse; out[2] = in[2] * inverse;
	} else {
		out[0] = out[1] = out[2] = 0;
	}
	return length;
}

typedef struct { float xyz[3], st[2]; } fnWolfETDecalCorner_t;

/* A texture projection that maps a triangle's three corners to their texture
 * coordinates, once all three are flattened onto the projection plane. Fails
 * when the corners' coordinates do not span a triangle. */
static inline int FnWolfET_DecalTextureMatrix( float texMat[2][4], const float projection[4],
	const fnWolfETDecalCorner_t *a, const fnWolfETDecalCorner_t *b,
	const fnWolfETDecalCorner_t *c ) {
	const fnWolfETDecalCorner_t *corners[3];
	double flat[3][3], bary[3], origin[3], xyz[3], bb;
	float vecs[3][3], axis[3][3], lengths[3];
	int i, j, which;
	corners[0] = a; corners[1] = b; corners[2] = c;
	for ( i = 0; i < 3; i++ ) {
		const double d = FnWolfET_DecalDot( corners[i]->xyz, projection ) - projection[3];
		for ( j = 0; j < 3; j++ ) flat[i][j] = corners[i]->xyz[j] - d * projection[j];
	}
	bb = ( b->st[0] - a->st[0] ) * ( c->st[1] - a->st[1] ) -
		( c->st[0] - a->st[0] ) * ( b->st[1] - a->st[1] );
	if ( fabs( bb ) < 0.00000001f ) return 0;
	/* The point at texture origin, then one unit along s and one along t. */
	for ( which = 0; which < 3; which++ ) {
		const double s = which == 1 ? 1.0 : 0.0, t = which == 2 ? 1.0 : 0.0;
		bary[0] = ( ( b->st[0] - s ) * ( c->st[1] - t ) - ( c->st[0] - s ) * ( b->st[1] - t ) ) / bb;
		bary[1] = ( ( c->st[0] - s ) * ( a->st[1] - t ) - ( a->st[0] - s ) * ( c->st[1] - t ) ) / bb;
		bary[2] = ( ( a->st[0] - s ) * ( b->st[1] - t ) - ( b->st[0] - s ) * ( a->st[1] - t ) ) / bb;
		for ( j = 0; j < 3; j++ )
			xyz[j] = bary[0] * flat[0][j] + bary[1] * flat[1][j] + bary[2] * flat[2][j];
		if ( !which ) {
			for ( j = 0; j < 3; j++ ) origin[j] = xyz[j];
		} else {
			for ( j = 0; j < 3; j++ ) vecs[which - 1][j] = (float)( xyz[j] - origin[j] );
		}
	}
	for ( j = 0; j < 3; j++ ) vecs[2][j] = -projection[j];
	for ( i = 0; i < 3; i++ ) lengths[i] = FnWolfET_DecalNormalize( vecs[i], axis[i] );
	for ( i = 0; i < 2; i++ ) for ( j = 0; j < 3; j++ )
		texMat[i][j] = lengths[i] > 0.0f ? axis[i][j] / lengths[i] : 0.0f;
	{
		float pa[3];
		for ( j = 0; j < 3; j++ ) pa[j] = (float)flat[0][j];
		texMat[0][3] = a->st[0] - FnWolfET_DecalDot( pa, texMat[0] );
		texMat[1][3] = a->st[1] - FnWolfET_DecalDot( pa, texMat[1] );
	}
	for ( i = 0; i < 2; i++ ) for ( j = 0; j < 4; j++ ) if ( !isfinite( texMat[i][j] ) ) return 0;
	return 1;
}

/* What ET does with a request before it decides whether the projector is in
 * view: the early refusals, the times, the colour and the bounding volume. The
 * planes are built afterwards by FnWolfET_DecalProjectorPlanes, because ET only
 * builds them for a projector it has not culled. Returns 0 for a request ET
 * ignores. `corners` receives the projector's footprint for the second step. */
static inline int FnWolfET_DecalProjectorStart( int shader, int numPoints, const float points[][3],
	const float projection[4], const float color[4], int lifeTime, int fadeTime, int time,
	fnWolfETDecalProjector_t *out, fnWolfETDecalCorner_t corners[4], float direction[4],
	int *footprint ) {
	float xyz[3];
	int i, j;
	if ( !points || !projection || !color || !out || !corners || !direction || !footprint ) return 0;
	if ( numPoints != 1 && numPoints != 3 && numPoints != 4 ) return 0;
	if ( lifeTime == 0 ) return 0;
	if ( !( projection[3] > 0.0f ) ) return 0;
	for ( i = 0; i < 4; i++ ) if ( !isfinite( projection[i] ) || !isfinite( color[i] ) ) return 0;
	for ( i = 0; i < numPoints; i++ ) for ( j = 0; j < 3; j++ ) if ( !isfinite( points[i][j] ) ) return 0;
	/* A negative lifetime or fade asks for a mark that lasts this frame only. */
	if ( lifeTime < 0 || fadeTime < 0 ) lifeTime = fadeTime = 0;
	if ( time > INT_MAX - lifeTime ) return 0;
	memset( out, 0, sizeof( *out ) );
	out->shader = shader;
	/* ET converts a colour past one straight to a byte, which the language
	 * leaves undefined; it is held to the byte's range here. */
	for ( i = 0; i < 4; i++ ) {
		float channel = color[i] * 255;
		if ( !( channel >= 0 ) ) channel = 0;
		if ( channel > 255 ) channel = 255;
		out->color[i] = (unsigned char)channel;
	}
	out->numPlanes = numPoints + 2;
	out->fadeStartTime = time + lifeTime - fadeTime;
	out->fadeEndTime = out->fadeStartTime + fadeTime;
	corners[0].st[0] = 0; corners[0].st[1] = 0;
	corners[1].st[0] = 0; corners[1].st[1] = 1;
	corners[2].st[0] = 1; corners[2].st[1] = 1;
	corners[3].st[0] = 1; corners[3].st[1] = 0;
	for ( i = 0; i < 4; i++ ) direction[i] = projection[i];
	if ( numPoints == 1 ) {
		/* A single point is a sphere: four corners of a square above it, a
		 * projection straight down twice the radius, and a texture projection
		 * for each axis so any wall it touches can be textured flat. */
		const float radius = projection[3];
		const float inverse = 1.0f / ( radius * 2.0f );
		const float *p = points[0];
		numPoints = 4;
		out->numPlanes = 6;
		out->omnidirectional = 1;
		direction[0] = 0; direction[1] = 0; direction[2] = -1; direction[3] = radius * 2.0f;
		xyz[0] = p[0] - radius; xyz[1] = p[1] - radius; xyz[2] = p[2] + radius;
		out->texMat[0][0][1] = inverse; out->texMat[0][1][2] = inverse;
		out->texMat[1][0][0] = inverse; out->texMat[1][1][2] = inverse;
		out->texMat[2][0][0] = inverse; out->texMat[2][1][1] = inverse;
		for ( i = 0; i < 3; i++ ) for ( j = 0; j < 2; j++ )
			out->texMat[i][j][3] = -FnWolfET_DecalDot( out->texMat[i][j], xyz );
		corners[0].xyz[0] = p[0] - radius; corners[0].xyz[1] = p[1] - radius; corners[0].xyz[2] = p[2] + radius;
		corners[1].xyz[0] = p[0] - radius; corners[1].xyz[1] = p[1] + radius; corners[1].xyz[2] = p[2] + radius;
		corners[2].xyz[0] = p[0] + radius; corners[2].xyz[1] = p[1] + radius; corners[2].xyz[2] = p[2] + radius;
		corners[3].xyz[0] = p[0] + radius; corners[3].xyz[1] = p[1] - radius; corners[3].xyz[2] = p[2] + radius;
	} else {
		/* ET copies four corners whether it was given three or four; only the
		 * ones it was given are read here. */
		for ( i = 0; i < numPoints; i++ ) for ( j = 0; j < 3; j++ ) corners[i].xyz[j] = points[i][j];
		if ( !FnWolfET_DecalTextureMatrix( out->texMat[0], direction, &corners[0], &corners[1], &corners[2] ) )
			return 0;
	}
	for ( j = 0; j < 3; j++ ) { out->mins[j] = 99999; out->maxs[j] = -99999; }
	for ( i = 0; i < numPoints; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			const float near = corners[i].xyz[j];
			const float far = corners[i].xyz[j] + direction[3] * direction[j];
			if ( near < out->mins[j] ) out->mins[j] = near;
			if ( near > out->maxs[j] ) out->maxs[j] = near;
			if ( far < out->mins[j] ) out->mins[j] = far;
			if ( far > out->maxs[j] ) out->maxs[j] = far;
		}
	}
	for ( j = 0; j < 3; j++ ) {
		out->center[j] = ( out->mins[j] + out->maxs[j] ) * 0.5f;
		xyz[j] = out->maxs[j] - out->center[j];
	}
	out->radius = (float)sqrt( FnWolfET_DecalDot( xyz, xyz ) );
	out->radius2 = out->radius * out->radius;
	*footprint = numPoints;
	return isfinite( out->radius ) ? 1 : 0;
}

/* The front plane through the footprint, the back plane the projection's
 * length behind it, and one side plane per footprint edge. Returns 0 when any
 * of them is degenerate, which is where ET abandons the request. */
static inline int FnWolfET_DecalProjectorPlanes( fnWolfETDecalProjector_t *dp,
	const fnWolfETDecalCorner_t corners[4], const float direction[4], int footprint ) {
	float xyz[3];
	int i, j;
	if ( !dp || !corners || !direction || footprint < 3 || footprint > 4 ) return 0;
	if ( !PlaneFromPoints( dp->planes[0], corners[0].xyz, corners[1].xyz, corners[2].xyz ) ) return 0;
	for ( j = 0; j < 3; j++ ) {
		dp->planes[1][j] = -dp->planes[0][j];
		xyz[j] = corners[0].xyz[j] + direction[3] * direction[j];
	}
	dp->planes[1][3] = FnWolfET_DecalDot( xyz, dp->planes[1] );
	for ( i = 0; i < footprint; i++ ) {
		for ( j = 0; j < 3; j++ ) xyz[j] = corners[i].xyz[j] + direction[3] * direction[j];
		if ( !PlaneFromPoints( dp->planes[i + 2], corners[( i + 1 ) % footprint].xyz,
			corners[i].xyz, xyz ) ) return 0;
	}
	return 1;
}

/* A projector moved into a brush model's own space. ET translates the box but
 * does not rotate it, as its own comment admits, so the box is only good for a
 * model that has not turned; the sphere, the planes and the texture
 * projections are all transformed properly. */
static inline void FnWolfET_DecalTransform( const fnWolfETDecalProjector_t *in,
	const float axis[3][3], const float origin[3], fnWolfETDecalProjector_t *out ) {
	float center[3];
	int i, j, m;
	out->shader = in->shader;
	memcpy( out->color, in->color, sizeof( out->color ) );
	out->fadeStartTime = in->fadeStartTime;
	out->fadeEndTime = in->fadeEndTime;
	out->omnidirectional = in->omnidirectional;
	out->numPlanes = in->numPlanes;
	for ( j = 0; j < 3; j++ ) {
		out->mins[j] = in->mins[j] - origin[j];
		out->maxs[j] = in->maxs[j] - origin[j];
		center[j] = in->center[j] - origin[j];
	}
	for ( j = 0; j < 3; j++ ) out->center[j] = FnWolfET_DecalDot( center, axis[j] );
	out->radius = in->radius;
	out->radius2 = in->radius2;
	for ( i = 0; i < in->numPlanes; i++ ) {
		for ( j = 0; j < 3; j++ ) out->planes[i][j] = FnWolfET_DecalDot( in->planes[i], axis[j] );
		out->planes[i][3] = in->planes[i][3] - FnWolfET_DecalDot( in->planes[i], origin );
	}
	for ( m = 0; m < 3; m++ ) for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 3; j++ ) out->texMat[m][i][j] = FnWolfET_DecalDot( in->texMat[m][i], axis[j] );
		out->texMat[m][i][3] = in->texMat[m][i][3] + FnWolfET_DecalDot( in->texMat[m][i], origin );
	}
}

/* ET's tests are strict: a box or sphere that only touches the projector's
 * sphere does not count. */
static inline int FnWolfET_DecalTouchesBox( const fnWolfETDecalProjector_t *dp,
	const float mins[3], const float maxs[3] ) {
	int j;
	for ( j = 0; j < 3; j++ )
		if ( mins[j] >= dp->center[j] + dp->radius || maxs[j] <= dp->center[j] - dp->radius ) return 0;
	return 1;
}

static inline int FnWolfET_DecalTouchesSphere( const fnWolfETDecalProjector_t *dp,
	const float center[3], float radius2 ) {
	float delta[3];
	int j;
	for ( j = 0; j < 3; j++ ) delta[j] = center[j] - dp->center[j];
	return FnWolfET_DecalDot( delta, delta ) < radius2 + dp->radius2;
}

/* Keeps the part of a polygon behind a plane. A polygon entirely in front is
 * dropped and one entirely behind is kept whole; one near the vertex limit is
 * dropped rather than risking a split it has no room for. */
static inline void FnWolfET_DecalChop( int numIn, float in[][3], int *numOut,
	float out[][3], const float plane[4], float epsilon ) {
	enum { FRONT, BACK, ON };
	float dists[FN_WOLFET_MAX_DECAL_VERTS + 4];
	int sides[FN_WOLFET_MAX_DECAL_VERTS + 4], counts[3] = { 0, 0, 0 };
	int i, j;
	*numOut = 0;
	if ( numIn >= FN_WOLFET_MAX_DECAL_VERTS - 1 ) return;
	for ( i = 0; i < numIn; i++ ) {
		dists[i] = FnWolfET_DecalDot( in[i], plane ) - plane[3];
		sides[i] = dists[i] > epsilon ? FRONT : dists[i] < -epsilon ? BACK : ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	if ( !counts[BACK] ) return;
	if ( !counts[FRONT] ) {
		*numOut = numIn;
		memcpy( out, in, (size_t)numIn * sizeof( in[0] ) );
		return;
	}
	for ( i = 0; i < numIn; i++ ) {
		const float *p1 = in[i];
		if ( sides[i] == ON || sides[i] == BACK ) {
			for ( j = 0; j < 3; j++ ) out[*numOut][j] = p1[j];
			( *numOut )++;
		}
		if ( sides[i + 1] == ON || sides[i + 1] == sides[i] ) continue;
		{
			const float *p2 = in[( i + 1 ) % numIn];
			const float d = dists[i] - dists[i + 1];
			const float dot = d == 0 ? 0 : dists[i] / d;
			for ( j = 0; j < 3; j++ ) out[*numOut][j] = p1[j] + dot * ( p2[j] - p1[j] );
			( *numOut )++;
		}
	}
}

/* Clips one triangle of a surface to the projector and colours what is left.
 * Returns 1 with the result in `decal` when anything is left. The caller
 * chooses which of its slots receives it. */
static inline int FnWolfET_DecalWinding( const fnWolfETDecalProjector_t *dp, int numPoints,
	float points[2][FN_WOLFET_MAX_DECAL_VERTS][3], fnWolfETDecal_t *decal ) {
	float plane[4], alpha = 1.0f, pd, d, d2;
	int i, pingPong = 0, axis;
	if ( !dp || !decal || numPoints < 3 ) return 0;
	if ( !PlaneFromPoints( plane, points[0][0], points[0][1], points[0][2] ) ) return 0;
	if ( dp->omnidirectional ) {
		float absolute[3];
		pd = 1.0f;
		/* A sphere fades with the surface's distance from its centre. */
		d = FnWolfET_DecalDot( dp->center, plane ) - plane[3];
		alpha = 1.0f - ( (float)fabs( d ) / dp->radius );
		if ( alpha < 0.0f ) return 0;
		if ( alpha > 1.0f ) alpha = 1.0f;
		for ( i = 0; i < 3; i++ ) absolute[i] = (float)fabs( plane[i] );
		/* Flattened along whichever axis the surface faces most. */
		if ( absolute[2] >= absolute[0] && absolute[2] >= absolute[1] ) axis = 2;
		else if ( absolute[0] >= absolute[1] && absolute[0] >= absolute[2] ) axis = 0;
		else axis = 1;
	} else {
		/* A surface facing away from the projector takes nothing. */
		pd = FnWolfET_DecalDot( dp->planes[0], plane );
		if ( pd < -0.0001f ) return 0;
		axis = 0;
	}
	for ( i = 0; i < dp->numPlanes; i++ ) {
		FnWolfET_DecalChop( numPoints, points[pingPong], &numPoints, points[!pingPong], dp->planes[i], 0.0f );
		pingPong ^= 1;
		if ( numPoints < 3 ) return 0;
		if ( numPoints == FN_WOLFET_MAX_DECAL_VERTS ) break;
	}
	memset( decal, 0, sizeof( *decal ) );
	decal->inUse = 1;
	decal->shader = dp->shader;
	decal->fadeStartTime = dp->fadeStartTime;
	decal->fadeEndTime = dp->fadeEndTime;
	decal->numVerts = numPoints;
	for ( i = 0; i < numPoints; i++ ) {
		fnWolfETDecalVert_t *vert = &decal->verts[i];
		int c;
		memcpy( vert->xyz, points[pingPong][i], sizeof( vert->xyz ) );
		vert->st[0] = FnWolfET_DecalDot( vert->xyz, dp->texMat[axis][0] ) + dp->texMat[axis][0][3];
		vert->st[1] = FnWolfET_DecalDot( vert->xyz, dp->texMat[axis][1] ) + dp->texMat[axis][1][3];
		if ( !dp->omnidirectional ) {
			/* A directional projector fades out over the back half of its
			 * depth, measured between the front and back planes. */
			d = FnWolfET_DecalDot( vert->xyz, dp->planes[0] ) - dp->planes[0][3];
			d2 = FnWolfET_DecalDot( vert->xyz, dp->planes[1] ) - dp->planes[1][3];
			alpha = 2.0f * d2 / ( d + d2 );
			if ( !isfinite( alpha ) ) alpha = 0.0f;
			if ( alpha > 1.0f ) alpha = 1.0f;
			else if ( alpha < 0.0f ) alpha = 0.0f;
		}
		for ( c = 0; c < 3; c++ ) vert->modulate[c] = (unsigned char)(int)( pd * alpha * dp->color[c] );
		vert->modulate[3] = (unsigned char)(int)( alpha * dp->color[3] );
	}
	return 1;
}

/* Ages a decal after the caller has copied it for this frame's drawing, as ET
 * does: the fade is applied to the kept decal, not to the copy, so the copy
 * shows the previous frame's colour and each frame's fade multiplies into the
 * last. A decal therefore fades faster than its fade time says, and faster
 * still at a higher frame rate. Returns 1 when the decal has finished and its
 * slot is free again. */
static inline int FnWolfET_DecalAge( fnWolfETDecal_t *decal, int time ) {
	int i, c;
	if ( !decal || !decal->inUse ) return 0;
	if ( decal->fadeStartTime < time && decal->fadeStartTime < decal->fadeEndTime ) {
		float fade = (float)( decal->fadeEndTime - time ) /
			(float)( decal->fadeEndTime - decal->fadeStartTime );
		/* Past the end ET multiplies by a negative fade, which the language
		 * leaves undefined for a byte; the decal is freed this frame either way. */
		if ( fade < 0 ) fade = 0;
		if ( fade > 1 ) fade = 1;
		for ( i = 0; i < decal->numVerts; i++ ) for ( c = 0; c < 4; c++ )
			decal->verts[i].modulate[c] = (unsigned char)( decal->verts[i].modulate[c] * fade );
	}
	if ( decal->fadeEndTime <= time ) {
		decal->inUse = 0;
		return 1;
	}
	return 0;
}

/* Chooses where a new decal goes: the first free slot, or failing that the one
 * whose fade ends soonest. */
static inline int FnWolfET_DecalSlot( const fnWolfETDecal_t *decals, int count ) {
	int i, oldest = 0;
	if ( !decals || count <= 0 ) return -1;
	for ( i = 0; i < count; i++ ) {
		if ( !decals[i].inUse ) return i;
		if ( decals[i].fadeEndTime < decals[oldest].fadeEndTime ) oldest = i;
	}
	return oldest;
}

#endif
