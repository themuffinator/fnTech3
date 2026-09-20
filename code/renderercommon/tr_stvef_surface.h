/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_SURFACE_H
#define FNTECH3_TR_STVEF_SURFACE_H

/*
 * Elite Force's procedural entity surfaces, tessellated as stvoyHM.exe 1.20
 * does (RB_SurfaceEntity 0x476c30 and the routines it calls). Each renderer's
 * tr_surface.c includes this after RB_AddQuadStampExt; it supplies tess,
 * backEnd, RB_CHECKOVERFLOW and R_ViewPassIsMirror.
 *
 * Retail writes only the red, green and blue bytes of these vertices and
 * leaves alpha as the buffer held it; here alpha is the entity's own.
 */

#define STVEF_BEZIER_STEP 0.0625f
#define STVEF_BEZIER_SEGMENTS 17
#define STVEF_ELECTRICITY_SEGMENTS 16

static void RB_STVEFVertex( int index, const vec3_t xyz, float s, float t,
	const byte *rgb, float colorScale, byte alpha ) {
	vec3_t normal;
	VectorCopy( xyz, tess.xyz[index] );
	VectorSubtract( vec3_origin, backEnd.viewParms.or.axis[0], normal );
	VectorCopy( normal, tess.normal[index] );
	tess.texCoords[0][index][0] = tess.texCoords[1][index][0] = s;
	tess.texCoords[0][index][1] = tess.texCoords[1][index][1] = t;
	// Retail truncates the scaled color toward zero.
	tess.vertexColors[index].rgba[0] = (byte)( rgb[0] * colorScale );
	tess.vertexColors[index].rgba[1] = (byte)( rgb[1] * colorScale );
	tess.vertexColors[index].rgba[2] = (byte)( rgb[2] * colorScale );
	tess.vertexColors[index].rgba[3] = alpha;
}

static void RB_STVEFTriangle( int a, int b, int c ) {
	tess.indexes[tess.numIndexes++] = a;
	tess.indexes[tess.numIndexes++] = b;
	tess.indexes[tess.numIndexes++] = c;
}

/* Sprite corners from two axes, rotated by degrees (RB_SurfaceSprite's rule). */
static void RB_STVEFSpriteAxes( const vec3_t leftAxis, const vec3_t upAxis,
	float radius, float rotation, vec3_t left, vec3_t up ) {
	if ( rotation == 0.0f ) {
		VectorScale( leftAxis, radius, left );
		VectorScale( upAxis, radius, up );
	} else {
		const float angle = M_PI * rotation / 180.0f;
		const float s = sin( angle ), c = cos( angle );
		VectorScale( leftAxis, c * radius, left );
		VectorMA( left, -s * radius, upAxis, left );
		VectorScale( upAxis, c * radius, up );
		VectorMA( up, s * radius, leftAxis, up );
	}
}

/* 0x476fd0: the entity's own left and up axes. Retail negates left in every
 * view except a mirror, the opposite of a view-facing sprite. */
static void RB_SurfaceSTVEFOrientedSprite( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t left, up;
	RB_STVEFSpriteAxes( e->axis[1], e->axis[2], e->radius, e->rotation, left, up );
	if ( !R_ViewPassIsMirror( &backEnd.viewParms ) ) {
		VectorSubtract( vec3_origin, left, left );
	}
	RB_AddQuadStampExt( e->origin, left, up, e->shader, 0, 0, 1, 1 );
}

/* 0x477150: a view-facing sprite whose four corners take their own colors. */
static void RB_SurfaceSTVEFAlphaVertPoly( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t left, up;
	int first, i;
	RB_STVEFSpriteAxes( backEnd.viewParms.or.axis[1], backEnd.viewParms.or.axis[2],
		e->radius, e->rotation, left, up );
	if ( R_ViewPassIsMirror( &backEnd.viewParms ) ) {
		VectorSubtract( vec3_origin, left, left );
	}
	RB_CHECKOVERFLOW( 4, 6 );
	first = tess.numVertexes;
	RB_AddQuadStampExt( e->origin, left, up, e->shader, 0, 0, 1, 1 );
	for ( i = 0; i < 4; ++i ) {
		tess.vertexColors[first + i] = e->stvef.vertexColors[i];
	}
}

/* 0x4744a0: one quad from start to end, halfWidth along up. The start's
 * first corner is drawn at a quarter of the color. */
static void RB_STVEFLineQuad( const vec3_t start, const vec3_t end, const vec3_t up,
	float halfWidth, float endT ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	const byte *rgb = e->shader.rgba;
	const byte alpha = e->shader.rgba[3];
	vec3_t corner;
	int ndx;
	RB_CHECKOVERFLOW( 4, 6 );
	ndx = tess.numVertexes;
	VectorMA( start, halfWidth, up, corner );
	RB_STVEFVertex( ndx, corner, 0, 0, rgb, 0.25f, alpha );
	VectorMA( start, -halfWidth, up, corner );
	RB_STVEFVertex( ndx + 1, corner, 1, 0, rgb, 1.0f, alpha );
	VectorMA( end, halfWidth, up, corner );
	RB_STVEFVertex( ndx + 2, corner, 0, endT, rgb, 1.0f, alpha );
	VectorMA( end, -halfWidth, up, corner );
	RB_STVEFVertex( ndx + 3, corner, 1, endT, rgb, 1.0f, alpha );
	tess.numVertexes += 4;
	RB_STVEFTriangle( ndx, ndx + 1, ndx + 2 );
	RB_STVEFTriangle( ndx + 2, ndx + 1, ndx + 3 );
}

/* The width direction of a view-facing line: normal to the plane through the
 * eye and both ends (0x4743b0, 0x474800). */
static void RB_STVEFLineAxis( const vec3_t start, const vec3_t end, vec3_t up ) {
	vec3_t toStart, toEnd;
	VectorSubtract( start, backEnd.viewParms.or.origin, toStart );
	VectorNormalize( toStart );
	VectorSubtract( end, backEnd.viewParms.or.origin, toEnd );
	VectorNormalize( toEnd );
	CrossProduct( toStart, toEnd, up );
	VectorNormalize( up );
}

static void RB_SurfaceSTVEFLine( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t up;
	RB_STVEFLineAxis( e->origin, e->oldorigin, up );
	RB_STVEFLineQuad( e->origin, e->oldorigin, up, e->stvef.width * 0.5f, e->stvef.stscale );
}

/* The entity's second axis sets the width direction. Retail normalizes the
 * entity's own axis; a copy is normalized here. */
static void RB_SurfaceSTVEFOrientedLine( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t up;
	VectorCopy( e->axis[1], up );
	VectorNormalize( up );
	RB_STVEFLineQuad( e->origin, e->oldorigin, up, e->stvef.width * 0.5f, e->stvef.stscale );
}

/* 0x474900: a trapezoid from width at the start to width2 at the end, fanned
 * from the start's midpoint, whose side corners are at half color. */
static void RB_SurfaceSTVEFLine2( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	const byte *rgb = e->shader.rgba;
	const byte alpha = e->shader.rgba[3];
	const float startHalf = e->stvef.width * 0.5f;
	const float endHalf = e->stvef.width2 * 0.5f;
	vec3_t up, corner;
	int ndx;
	RB_STVEFLineAxis( e->origin, e->oldorigin, up );
	RB_CHECKOVERFLOW( 5, 9 );
	ndx = tess.numVertexes;
	RB_STVEFVertex( ndx, e->origin, 0.5f, 0, rgb, 1.0f, alpha );
	VectorMA( e->origin, startHalf, up, corner );
	RB_STVEFVertex( ndx + 1, corner, 0, 0, rgb, 0.5f, alpha );
	VectorMA( e->origin, -startHalf, up, corner );
	RB_STVEFVertex( ndx + 2, corner, 1, 0, rgb, 0.5f, alpha );
	VectorMA( e->oldorigin, endHalf, up, corner );
	RB_STVEFVertex( ndx + 3, corner, 0, e->stvef.stscale, rgb, 1.0f, alpha );
	VectorMA( e->oldorigin, -endHalf, up, corner );
	RB_STVEFVertex( ndx + 4, corner, 1, e->stvef.stscale, rgb, 1.0f, alpha );
	tess.numVertexes += 5;
	RB_STVEFTriangle( ndx + 1, ndx, ndx + 3 );
	RB_STVEFTriangle( ndx, ndx + 2, ndx + 4 );
	RB_STVEFTriangle( ndx, ndx + 4, ndx + 3 );
}

/* A view-facing ribbon segment that continues from the previous one's end
 * corners (bezier 0x474ea0, electricity 0x475910). The two routines keep
 * their sides differently: the bezier's first quad starts with +width at
 * texture s 0, the electricity's with -width. */
static void RB_STVEFRibbonSegment( const vec3_t from, const vec3_t to, float halfWidth,
	float fromT, float toT, qboolean *continued, vec3_t corners[2], qboolean bezier ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	const byte *rgb = e->shader.rgba;
	const byte alpha = e->shader.rgba[3];
	const float firstSide = bezier ? halfWidth : -halfWidth;
	vec3_t dir, view, right, corner;
	const int ndx = tess.numVertexes;
	VectorSubtract( to, from, dir );
	VectorSubtract( to, backEnd.viewParms.or.origin, view );
	CrossProduct( dir, view, right );
	VectorNormalize( right );
	if ( *continued ) VectorCopy( corners[0], corner );
	else VectorMA( from, firstSide, right, corner );
	RB_STVEFVertex( ndx, corner, 0, fromT, rgb, 1.0f, alpha );
	if ( *continued ) VectorCopy( corners[1], corner );
	else VectorMA( from, -firstSide, right, corner );
	RB_STVEFVertex( ndx + 1, corner, 1, fromT, rgb, 1.0f, alpha );
	VectorMA( to, halfWidth, right, corner );
	RB_STVEFVertex( ndx + 2, corner, 1, toT, rgb, 1.0f, alpha );
	VectorCopy( corner, corners[1] );
	VectorMA( to, -halfWidth, right, corner );
	RB_STVEFVertex( ndx + 3, corner, 0, toT, rgb, 1.0f, alpha );
	VectorCopy( corner, corners[0] );
	tess.numVertexes += 4;
	RB_STVEFTriangle( ndx, ndx + 1, ndx + 2 );
	RB_STVEFTriangle( ndx + 2, ndx + 3, ndx );
	*continued = qtrue;
}

/* 0x474d70: a cubic curve from origin through control1 and control2 to
 * oldorigin, sampled every sixteenth from t = 0 (a first, empty segment)
 * to t = 1; the texture runs half as fast as t. */
static void RB_SurfaceSTVEFBezier( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t corners[2], previous, point;
	qboolean continued = qfalse;
	double t = 0.0;
	int axis;
	RB_CHECKOVERFLOW( STVEF_BEZIER_SEGMENTS * 4, STVEF_BEZIER_SEGMENTS * 6 );
	VectorCopy( e->origin, previous );
	do {
		const double u = 1.0 - t;
		const double next = t + STVEF_BEZIER_STEP;
		for ( axis = 0; axis < 3; ++axis ) {
			point[axis] = (float)( e->stvef.control2[axis] * ( 3.0 * u * t * t )
				+ e->oldorigin[axis] * ( t * t * t ) + e->origin[axis] * ( u * u * u )
				+ e->stvef.control1[axis] * ( 3.0 * u * u * t ) );
		}
		RB_STVEFRibbonSegment( previous, point, e->stvef.width * 0.5f,
			(float)( t * 0.5 ), (float)( next * 0.5 ), &continued, corners, qtrue );
		VectorCopy( point, previous );
		t = next;
	} while ( t <= 1.0 );
}

/* 0x475360: a frustum along axis[0] from width at the origin to width2 at
 * height, with no caps. Its face count falls with distance, scaled by bias:
 * (1 - distance / 1024) * bias * 32, held to 7..32. Texture t is 1 at the
 * base and 0 at the top; s spans each face, or wraps once around. */
static void RB_SurfaceSTVEFCylinder( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	const byte *rgb = e->shader.rgba;
	vec3_t middle, right, up, bottomRadius, topRadius, top;
	vec3_t bottomRing[32], topRing[32];
	float faceStep, sStep = 0.0f;
	float detail;
	int faces, i;
	VectorMA( e->origin, e->stvef.height * 0.5f, e->axis[0], middle );
	VectorSubtract( middle, backEnd.viewParms.or.origin, middle );
	detail = ( 1.0f - VectorNormalize( middle ) * ( 1.0f / 1024.0f ) ) * e->stvef.bias * 32.0f;
	// Clamp before converting: a NaN or oversized module-supplied bias must
	// never reach an undefined float-to-int conversion.
	if ( !( detail >= 7.0f ) ) faces = 7;
	else if ( detail >= 32.0f ) faces = 32;
	else faces = (int)detail;
	MakeNormalVectors( e->axis[0], right, up );
	VectorScale( up, e->stvef.width * 0.5f, bottomRadius );
	VectorScale( up, e->stvef.width2 * 0.5f, topRadius );
	VectorMA( e->origin, e->stvef.height, e->axis[0], top );
	faceStep = 360.0f / (float)faces;
	for ( i = 0; i < faces; ++i ) {
		const float degrees = (float)i * faceStep;
		RotatePointAroundVector( bottomRing[i], e->axis[0], bottomRadius, degrees );
		VectorAdd( e->origin, bottomRing[i], bottomRing[i] );
		RotatePointAroundVector( topRing[i], e->axis[0], topRadius, degrees );
		VectorAdd( top, topRing[i], topRing[i] );
	}
	if ( e->stvef.wrap ) {
		sStep = e->stvef.stscale / (float)faces;
	}
	for ( i = 0; i < faces; ++i ) {
		const int next = i + 1 < faces ? i + 1 : 0;
		const float s0 = e->stvef.wrap ? (float)i * sStep : 0.0f;
		const float s1 = e->stvef.wrap ? (float)( i + 1 ) * sStep : e->stvef.stscale;
		int ndx;
		RB_CHECKOVERFLOW( 4, 6 );
		ndx = tess.numVertexes;
		RB_STVEFVertex( ndx, bottomRing[i], s0, 1.0f, rgb, 1.0f, 255 );
		RB_STVEFVertex( ndx + 1, topRing[i], s0, 0.0f, rgb, 1.0f, 255 );
		RB_STVEFVertex( ndx + 2, topRing[next], s1, 0.0f, rgb, 1.0f, 255 );
		RB_STVEFVertex( ndx + 3, bottomRing[next], s1, 1.0f, rgb, 1.0f, 255 );
		tess.numVertexes += 4;
		RB_STVEFTriangle( ndx, ndx + 1, ndx + 2 );
		RB_STVEFTriangle( ndx + 2, ndx + 3, ndx );
	}
}

/* 0x475d90: sixteen jittered steps from origin to oldorigin, each a
 * sixteenth of the whole length; the fifteenth aims at the midpoint of what
 * remains and the last ends exactly at oldorigin. With taper each step is a
 * sixteenth narrower; with wrap the texture advances a sixteenth per step. */
static void RB_STVEFElectricityStep( const vec3_t direction, float deviation,
	float stepLength, vec3_t point ) {
	vec3_t offset;
	offset[0] = crandom() * deviation + direction[0];
	offset[1] = crandom() * deviation + direction[1];
	offset[2] = crandom() * deviation * 0.5f + direction[2];
	VectorMA( point, stepLength, offset, point );
}

static void RB_SurfaceSTVEFElectricity( void ) {
	const refEntity_t *e = &backEnd.currentEntity->e;
	const float widthStep = e->stvef.taper ? e->stvef.width * ( 1.0f / 16.0f ) : 0.0f;
	const float tStep = e->stvef.wrap ? e->stvef.stscale * ( 1.0f / 16.0f ) : 0.0f;
	float fromT = 0.0f, toT = e->stvef.stscale, width = e->stvef.width, stepLength;
	vec3_t direction, point, previous, corners[2];
	qboolean continued = qfalse;
	int i;
	VectorSubtract( e->oldorigin, e->origin, direction );
	// Retail truncates the length to whole units before dividing it.
	stepLength = VectorNormalize( direction );
	// 2^31 is the first non-negative float outside a signed 32-bit integer.
	// Reject invalid lengths before touching the batch or truncating them.
	if ( !( stepLength >= 0.0f ) || stepLength >= 2147483648.0f ) return;
	stepLength = (float)(int)stepLength * ( 1.0f / 16.0f );
	RB_CHECKOVERFLOW( STVEF_ELECTRICITY_SEGMENTS * 4, STVEF_ELECTRICITY_SEGMENTS * 6 );
	VectorCopy( e->origin, point );
	for ( i = 0; i < STVEF_ELECTRICITY_SEGMENTS; ++i ) {
		VectorCopy( point, previous );
		if ( i == STVEF_ELECTRICITY_SEGMENTS - 1 ) {
			VectorCopy( e->oldorigin, point );
		} else {
			if ( i == STVEF_ELECTRICITY_SEGMENTS - 2 ) {
				VectorAdd( point, e->oldorigin, point );
				VectorScale( point, 0.5f, point );
			}
			RB_STVEFElectricityStep( direction, e->stvef.deviation, stepLength, point );
		}
		if ( e->stvef.wrap ) {
			fromT = (float)i * tStep;
			toT = (float)( i + 1 ) * tStep;
		}
		width -= widthStep;
		RB_STVEFRibbonSegment( previous, point, width * 0.5f, fromT, toT,
			&continued, corners, qfalse );
	}
}

/* Returns qtrue when the current entity is one of Elite Force's primitives. */
static qboolean RB_SurfaceSTVEF( void ) {
	switch ( backEnd.currentEntity->e.reType ) {
	case RT_STVEF_ORIENTEDSPRITE: RB_SurfaceSTVEFOrientedSprite(); return qtrue;
	case RT_STVEF_ALPHAVERTPOLY: RB_SurfaceSTVEFAlphaVertPoly(); return qtrue;
	case RT_STVEF_LINE: RB_SurfaceSTVEFLine(); return qtrue;
	case RT_STVEF_ORIENTEDLINE: RB_SurfaceSTVEFOrientedLine(); return qtrue;
	case RT_STVEF_LINE2: RB_SurfaceSTVEFLine2(); return qtrue;
	case RT_STVEF_BEZIER: RB_SurfaceSTVEFBezier(); return qtrue;
	case RT_STVEF_CYLINDER: RB_SurfaceSTVEFCylinder(); return qtrue;
	case RT_STVEF_ELECTRICITY: RB_SurfaceSTVEFElectricity(); return qtrue;
	default: return qfalse;
	}
}

#endif /* FNTECH3_TR_STVEF_SURFACE_H */
