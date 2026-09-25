/*
===========================================================================
Copyright (C) 2008 Leszek Godlewski
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3. It is adapted from OpenMoHAA's
code/qcommon/cm_terrain.c (https://github.com/openmoh/openmohaa), which is
distributed under the GNU General Public License, version 2 or (at your
option) any later version, and remains under that licence here.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

fnTech3 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.
===========================================================================
*/

/*
Medal of Honor's LOD terrain, for collision.

A terrain patch is 8 by 8 squares of 64 units with a 9 by 9 heightmap. Each
square is two triangles whose shared diagonal alternates across the patch and
flips with the patch's flag; the renderer's variance tree, read here only for
the two bits that take a square's halves out of the terrain
(code/qcommon/mohaa_terrain_squares.h, which the renderers' translated world
reads too), decides whether a square collides as one plane, two, a single
triangle or not at all. Point
traces walk the squares a segment crosses; a capsule trace tests each square's
triangles against the capsule's lower sphere; a box trace sends a point from
each lower (or upper) corner of the box.

Adaptation for fnTech3: OpenMoHAA's global capsule state is fnTech3's
per-trace sphere, the varnode index table is built once (in
mohaa_terrain_squares.h, shared with the renderers), and nothing here reads a
global map; the loader hands each patch in. The collision arithmetic
is OpenMoHAA's, including its correction that also stops a trace leaving the
terrain from below. See docs/fntech3/2026-09-24-mohaa-maps.md.
*/

#include "cm_local.h"
#include "cm_mohaa_terrain.h"
#include "mohaa_terrain_squares.h"

typedef struct pointtrace_s {
	traceWork_t					*tw;
	const mohaaTerrainCollide_t	*tc;
	vec3_t						vStart;
	vec3_t						vEnd;
	int							i;
	int							j;
	float						fSurfaceClipEpsilon;
} pointtrace_t;

static mohaaTerrainSquareTable_t	g_squareTable;
static qboolean						g_squareTableReady;

/* World units to squares: 1/64, written as OpenMoHAA writes it. */
#define TERRAIN_UNIT	( SURFACE_CLIP_EPSILON / 8 )

static cvar_t *cm_ter_usesphere;

/*
====================
CM_MOHAA_PrepareTerrainCollide
====================
*/
void CM_MOHAA_PrepareTerrainCollide( void ) {
	cm_ter_usesphere = Cvar_Get( "cm_ter_usesphere", "1", CVAR_CHEAT );

	if ( g_squareTableReady ) {
		return;
	}
	MOHAA_BuildTerrainSquareTable( &g_squareTable );
	g_squareTableReady = qtrue;
}

/* A triangle's plane, in single precision as the terrain's own code builds
 * it; the patch code's version is private to it and works in doubles. */
static void CM_MOHAA_PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c ) {
	vec3_t d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );
	VectorNormalize( plane );
	plane[3] = DotProduct( a, plane );
}

static void CM_MOHAA_PickTerrainSquareMode( mohaaTerrainCollideSquare_t *square, const vec3_t vTest,
	int i, int j, const dterrainPatchMOHAA_t *patch ) {
	square->eMode = MOHAA_TerrainSquareMode( &g_squareTable, patch, i, j );

	if ( square->eMode == 2 ) {
		if ( DotProduct( vTest, square->plane[0] ) < square->plane[0][3] ) {
			square->eMode = 1;
		}
	} else if ( square->eMode == 5 || square->eMode == 6 ) {
		VectorCopy( square->plane[1], square->plane[0] );
		square->plane[0][3] = square->plane[1][3];
	}
}

/*
====================
CM_MOHAA_GenerateTerrainCollide
====================
*/
void CM_MOHAA_GenerateTerrainCollide( const dterrainPatchMOHAA_t *patch, mohaaTerrainCollide_t *tc ) {
	int							i;
	int							j;
	int							x0, y0, z0;
	float						fMaxHeight;
	float						heightmap[9][9];
	mohaaTerrainCollideSquare_t	*square;
	vec3_t						v1;
	vec3_t						v2;
	vec3_t						v3;
	vec3_t						v4;

	x0 = patch->x * 64;
	y0 = patch->y * 64;
	z0 = (int16_t)LittleShort( patch->iBaseHeight );

	fMaxHeight = (float)z0;

	for ( j = 0; j < 9; j++ ) {
		for ( i = 0; i < 9; i++ ) {
			heightmap[i][j] = (float)( z0 + 2 * patch->heightmap[j * 9 + i] );
		}
	}

	for ( j = 0; j < 8; j++ ) {
		for ( i = 0; i < 8; i++ ) {
			v1[0] = (float)( ( i << 6 ) + x0 );
			v1[1] = (float)( ( j << 6 ) + y0 );
			v1[2] = heightmap[i][j];

			v2[0] = (float)( ( i << 6 ) + x0 + 64 );
			v2[1] = (float)( ( j << 6 ) + y0 );
			v2[2] = heightmap[i + 1][j];

			v3[0] = (float)( ( i << 6 ) + x0 + 64 );
			v3[1] = (float)( ( j << 6 ) + y0 + 64 );
			v3[2] = heightmap[i + 1][j + 1];

			v4[0] = (float)( ( i << 6 ) + x0 );
			v4[1] = (float)( ( j << 6 ) + y0 + 64 );
			v4[2] = heightmap[i][j + 1];

			if ( fMaxHeight < v1[2] ) {
				fMaxHeight = v1[2];
			}
			if ( fMaxHeight < v2[2] ) {
				fMaxHeight = v2[2];
			}
			if ( fMaxHeight < v3[2] ) {
				fMaxHeight = v3[2];
			}
			if ( fMaxHeight < v4[2] ) {
				fMaxHeight = v4[2];
			}

			square = &tc->squares[i][j];

			if ( ( i + j ) & 1 ) {
				if ( patch->flags & MOHAA_TERPATCH_FLIP ) {
					CM_MOHAA_PlaneFromPoints( square->plane[0], v4, v2, v3 );
					CM_MOHAA_PlaneFromPoints( square->plane[1], v2, v4, v1 );
				} else {
					CM_MOHAA_PlaneFromPoints( square->plane[0], v2, v4, v3 );
					CM_MOHAA_PlaneFromPoints( square->plane[1], v4, v2, v1 );
				}
				CM_MOHAA_PickTerrainSquareMode( square, v1, i, j, patch );
			} else {
				if ( patch->flags & MOHAA_TERPATCH_FLIP ) {
					CM_MOHAA_PlaneFromPoints( square->plane[0], v1, v3, v4 );
					CM_MOHAA_PlaneFromPoints( square->plane[1], v3, v1, v2 );
				} else {
					CM_MOHAA_PlaneFromPoints( square->plane[0], v3, v1, v4 );
					CM_MOHAA_PlaneFromPoints( square->plane[1], v1, v3, v2 );
				}
				CM_MOHAA_PickTerrainSquareMode( square, v2, i, j, patch );
			}
		}
	}

	tc->vBounds[0][0] = (float)x0;
	tc->vBounds[0][1] = (float)y0;
	tc->vBounds[0][2] = (float)z0;
	tc->vBounds[1][0] = (float)( x0 + 512 );
	tc->vBounds[1][1] = (float)( y0 + 512 );
	tc->vBounds[1][2] = fMaxHeight;
}

static float CM_MOHAA_CheckTerrainPlane( const pointtrace_t *pt, const float *plane ) {
	float d1, d2;
	float f;

	d1 = DotProduct( pt->vStart, plane ) - plane[3];
	d2 = DotProduct( pt->vEnd, plane ) - plane[3];

	// if completely in front of face, no intersection with the entire brush
	if ( d1 > 0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ) ) {
		return 1;
	}

	if ( d1 <= 0 && d2 <= 0 ) {
		if ( d1 >= -32 ) {
			return 0;
		}
		if ( d2 >= -32 ) {
			return 0;
		}
		return 1;
	}

	// crosses the face, entering or leaving
	if ( d1 > d2 ) {
		f = (float)( ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 ) );
		if ( f < 0 ) {
			f = 0;
		}
	} else {
		f = (float)( ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 ) );
		if ( f > 1 ) {
			f = 1;
		}
	}

	return f;
}

static float CM_MOHAA_CheckTerrainTriSpherePoint( const pointtrace_t *pt, const vec3_t v ) {
	vec3_t	vDelta, vDir;
	float	fLenSq;
	float	fRadSq;
	float	fA, fB;
	float	fDiscr;
	float	fFrac;
	float	fSq;
	float	f;
	const float radius = pt->tw->sphere.radius;

	VectorSubtract( pt->vStart, v, vDir );

	fRadSq = Square( radius );
	fLenSq = VectorLengthSquared( vDir );

	if ( fLenSq <= fRadSq ) {
		pt->tw->trace.startsolid = qtrue;
		pt->tw->trace.allsolid = qtrue;
		return 0;
	}

	VectorSubtract( pt->vEnd, pt->vStart, vDelta );

	fA = VectorLengthSquared( vDelta );
	fB = DotProduct( vDelta, vDir );
	fDiscr = fB * fB - ( fLenSq - fRadSq ) * fA;

	if ( fDiscr <= 0.0f ) {
		return pt->tw->trace.fraction;
	}

	fSq = sqrtf( fDiscr );

	if ( fA > 0 ) {
		fFrac = ( -fB - fSq ) / fA - pt->fSurfaceClipEpsilon;

		if ( fFrac >= 0.0f && fFrac <= pt->tw->trace.fraction ) {
			return fFrac;
		}

		fFrac = -fB + fSq;
	} else {
		fFrac = ( -fB + fSq ) / fA - pt->fSurfaceClipEpsilon;

		if ( fFrac >= 0.0f && fFrac <= pt->tw->trace.fraction ) {
			return fFrac;
		}

		fFrac = -fB - fSq;
	}

	f = fFrac / fA - pt->fSurfaceClipEpsilon;
	if ( f < 0 || f > pt->tw->trace.fraction ) {
		f = pt->tw->trace.fraction;
	}

	return f;
}

static float CM_MOHAA_CheckTerrainTriSphereCorner( const pointtrace_t *pt, const float *plane,
	float x0, float y0, int i, int j ) {
	vec3_t v;

	v[0] = ( i << 6 ) + x0;
	v[1] = ( j << 6 ) + y0;
	v[2] = ( plane[3] - ( v[1] * plane[1] + v[0] * plane[0] ) ) / plane[2];

	return CM_MOHAA_CheckTerrainTriSpherePoint( pt, v );
}

static float CM_MOHAA_CheckTerrainTriSphereEdge( const pointtrace_t *pt, const float *plane,
	float x0, float y0, int i0, int j0, int i1, int j1 ) {
	vec3_t	v0, v1;
	float	fScale;
	float	fRadSq;
	float	S, T;
	vec3_t	vDeltaStart;
	vec3_t	vDirEdge;
	vec3_t	vDirTrace;
	float	fSFromT_Const;
	float	fSFromT_Scale;
	vec3_t	vRFromT_Const;
	vec3_t	vRFromT_Scale;
	float	fLengthSq, fDot;
	float	fFrac, fFracClip;

	fScale = 1.0f / plane[2];

	v0[0] = ( i0 << 6 ) + x0;
	v0[1] = ( j0 << 6 ) + y0;
	v0[2] = ( plane[3] - ( v0[0] * plane[0] + v0[1] * plane[1] ) ) * fScale;

	v1[0] = ( i1 << 6 ) + x0;
	v1[1] = ( j1 << 6 ) + y0;
	v1[2] = ( plane[3] - ( v1[0] * plane[0] + v1[1] * plane[1] ) ) * fScale;

	VectorSubtract( pt->vStart, v0, vDirTrace );
	VectorSubtract( v1, v0, vDirEdge );
	VectorSubtract( pt->vEnd, pt->vStart, vDeltaStart );

	fScale = 1.0f / VectorLengthSquared( vDirEdge );
	S = DotProduct( vDirTrace, vDirEdge ) * fScale;
	T = DotProduct( vDeltaStart, vDirEdge ) * fScale;

	VectorMA( vDirTrace, -S, vDirEdge, vRFromT_Const );
	VectorMA( vDeltaStart, -T, vDirEdge, vRFromT_Scale );

	fRadSq = Square( pt->tw->sphere.radius );
	fLengthSq = VectorLengthSquared( vRFromT_Const );

	if ( fLengthSq <= fRadSq ) {
		if ( S < 0 || S > 1 ) {
			return CM_MOHAA_CheckTerrainTriSpherePoint( pt, v0 );
		}

		pt->tw->trace.startsolid = qtrue;
		pt->tw->trace.allsolid = qtrue;
		return 1;
	}

	fDot = DotProduct( vRFromT_Scale, vRFromT_Const );
	fSFromT_Scale = VectorLengthSquared( vRFromT_Scale );
	fSFromT_Const = Square( fDot ) - ( fLengthSq - fRadSq ) * fSFromT_Scale;

	if ( fSFromT_Const <= 0 ) {
		return pt->tw->trace.fraction;
	}

	if ( fSFromT_Scale > 0 ) {
		fFrac = ( -fDot - sqrtf( fSFromT_Const ) ) / fSFromT_Scale;
	} else {
		fFrac = ( -fDot + sqrtf( fSFromT_Const ) ) / fSFromT_Scale;
	}

	if ( fFrac <= 0 ) {
		return pt->tw->trace.fraction;
	}

	fFracClip = fFrac - pt->fSurfaceClipEpsilon;
	if ( fFracClip >= pt->tw->trace.fraction ) {
		return pt->tw->trace.fraction;
	}

	fFrac = fFrac * T + S;

	if ( fFrac < 0 ) {
		return CM_MOHAA_CheckTerrainTriSpherePoint( pt, v0 );
	}

	if ( fFrac > 1 ) {
		return CM_MOHAA_CheckTerrainTriSpherePoint( pt, v1 );
	}

	if ( fFracClip < 0 ) {
		fFracClip = 0;
	}
	return fFracClip;
}

static float CM_MOHAA_CheckTerrainTriSphere( pointtrace_t *pt, float x0, float y0, int iPlane ) {
	const float	*plane;
	float		fMaxFraction;
	float		d1, d2;
	float		fSpherePlane;
	int			eMode;
	qboolean	bFitsX, bFitsY;
	qboolean	bFitsDiag;
	int			iX[3];
	int			iY[3];
	const float	radius = pt->tw->sphere.radius;

	plane = pt->tc->squares[pt->i][pt->j].plane[iPlane];
	d1 = DotProduct( pt->vStart, plane ) - plane[3];
	d2 = DotProduct( pt->vEnd, plane ) - plane[3];

	// if completely in front of face, no intersection with the entire brush
	if ( d1 > radius && ( d2 >= radius + SURFACE_CLIP_EPSILON || d2 >= d1 ) ) {
		return pt->tw->trace.fraction;
	}

	// if it doesn't cross the plane, the plane isn't relevent
	if ( d1 <= -radius && d2 <= -radius ) {
		return pt->tw->trace.fraction;
	}

	fMaxFraction = (float)( SURFACE_CLIP_EPSILON / ( d1 - d2 ) );
	pt->fSurfaceClipEpsilon = fMaxFraction;

	if ( d1 > d2 ) {
		fSpherePlane = ( d1 - radius ) / ( d1 - d2 ) - fMaxFraction;
		if ( fSpherePlane < 0 ) {
			fSpherePlane = 0;
		}
	} else {
		fSpherePlane = ( d1 + radius ) / ( d1 - d2 ) - fMaxFraction;
		if ( fSpherePlane > 1 ) {
			fSpherePlane = 1;
		}
	}

	if ( fSpherePlane >= pt->tw->trace.fraction ) {
		return pt->tw->trace.fraction;
	}

	d1 = ( pt->vEnd[0] - pt->vStart[0] ) * fSpherePlane + pt->vEnd[0] - radius * plane[0] - x0;
	d2 = ( pt->vEnd[1] - pt->vStart[1] ) * fSpherePlane + pt->vEnd[1] - radius * plane[1] - y0;

	eMode = pt->tc->squares[pt->i][pt->j].eMode;

	if ( eMode == 1 || eMode == 2 ) {
		if ( ( pt->i + pt->j ) & 1 ) {
			eMode = iPlane ? 6 : 3;
		} else {
			eMode = iPlane ? 5 : 4;
		}
	}

	switch ( eMode ) {
	case 3:
		bFitsX = d1 <= 64;
		bFitsY = d2 <= 64;
		bFitsDiag = d1 >= 64 - d2;
		iX[0] = 1; iX[1] = 0; iX[2] = 1;
		iY[0] = 1; iY[1] = 1; iY[2] = 0;
		break;
	case 4:
		bFitsX = d1 >= 0;
		bFitsY = d2 <= 64;
		bFitsDiag = d1 <= d2;
		iX[0] = 0; iX[1] = 1; iX[2] = 0;
		iY[0] = 1; iY[1] = 1; iY[2] = 0;
		break;
	case 5:
		bFitsX = d1 <= 64;
		bFitsY = d2 >= 0;
		bFitsDiag = d1 >= d2;
		iX[0] = 1; iX[1] = 0; iX[2] = 1;
		iY[0] = 0; iY[1] = 0; iY[2] = 1;
		break;
	case 6:
		bFitsX = d1 >= 0;
		bFitsY = d2 >= 0;
		bFitsDiag = d1 <= 64 - d2;
		iX[0] = 0; iX[1] = 1; iX[2] = 0;
		iY[0] = 0; iY[1] = 0; iY[2] = 1;
		break;
	default:
		return 0;
	}

	if ( bFitsX && bFitsY ) {
		if ( bFitsDiag ) {
			return fSpherePlane;
		}
		return CM_MOHAA_CheckTerrainTriSphereEdge( pt, plane, x0, y0, iX[1], iY[1], iX[2], iY[2] );
	}

	if ( bFitsX && !bFitsY ) {
		if ( bFitsDiag ) {
			return CM_MOHAA_CheckTerrainTriSphereEdge( pt, plane, x0, y0, iX[0], iY[0], iX[1], iY[1] );
		}
		return CM_MOHAA_CheckTerrainTriSphereCorner( pt, plane, x0, y0, iX[1], iY[1] );
	}

	if ( !bFitsX && bFitsY ) {
		if ( bFitsDiag ) {
			return CM_MOHAA_CheckTerrainTriSphereEdge( pt, plane, x0, y0, iX[0], iY[0], iX[2], iY[2] );
		}
		return CM_MOHAA_CheckTerrainTriSphereCorner( pt, plane, x0, y0, iX[2], iY[2] );
	}

	if ( bFitsDiag ) {
		return CM_MOHAA_CheckTerrainTriSphereCorner( pt, plane, x0, y0, iX[0], iY[0] );
	}

	return pt->tw->trace.fraction;
}

static qboolean CM_MOHAA_ValidateTerrainCollidePointSquare( const pointtrace_t *pt, float frac ) {
	float f;

	f = pt->vStart[0] + frac * ( pt->vEnd[0] - pt->vStart[0] ) - ( ( pt->i << 6 ) + pt->tc->vBounds[0][0] );

	if ( f >= 0 && f <= 64 ) {
		f = pt->vStart[1] + frac * ( pt->vEnd[1] - pt->vStart[1] ) - ( ( pt->j << 6 ) + pt->tc->vBounds[0][1] );

		if ( f >= 0 && f <= 64 ) {
			return qtrue;
		}
	}

	return qfalse;
}

static qboolean CM_MOHAA_ValidateTerrainCollidePointTri( const pointtrace_t *pt, int eMode, float frac ) {
	float x0, y0;
	float x, y;
	float dx, dy;

	x0 = ( pt->i << 6 ) + pt->tc->vBounds[0][0];
	dx = pt->vStart[0] + ( pt->vEnd[0] - pt->vStart[0] ) * frac;
	x = x0 + 64;

	if ( x0 > dx || x < dx ) {
		return qfalse;
	}

	y0 = ( pt->j << 6 ) + pt->tc->vBounds[0][1];
	dy = pt->vStart[1] + ( pt->vEnd[1] - pt->vStart[1] ) * frac;
	y = y0 + 64;

	if ( y0 > dy || y < dy ) {
		return qfalse;
	}

	switch ( eMode ) {
	case 3:
		return ( dx - x0 ) >= ( 64 - ( dy - y0 ) );
	case 4:
		return ( dx - x0 ) <= ( dy - y0 );
	case 5:
		return ( dx - x0 ) >= ( dy - y0 );
	case 6:
		return ( dx - x0 ) <= ( 64 - ( dy - y0 ) );
	default:
		return qtrue;
	}
}

static qboolean CM_MOHAA_TestTerrainCollideSquare( const pointtrace_t *pt ) {
	const float	*plane;
	float		frac0;
	float		enterFrac;
	int			eMode;

	eMode = pt->tc->squares[pt->i][pt->j].eMode;

	if ( !eMode ) {
		return qfalse;
	}

	if ( eMode >= 0 && eMode <= 2 ) {
		enterFrac = CM_MOHAA_CheckTerrainPlane( pt, pt->tc->squares[pt->i][pt->j].plane[0] );

		plane = pt->tc->squares[pt->i][pt->j].plane[1];
		frac0 = CM_MOHAA_CheckTerrainPlane( pt, plane );

		if ( eMode == 2 ) {
			if ( enterFrac > frac0 ) {
				enterFrac = frac0;
			}
		} else {
			if ( enterFrac < frac0 ) {
				enterFrac = frac0;
			}
		}

		if ( enterFrac < pt->tw->trace.fraction && CM_MOHAA_ValidateTerrainCollidePointSquare( pt, enterFrac ) ) {
			pt->tw->trace.fraction = enterFrac;
			VectorCopy( plane, pt->tw->trace.plane.normal );
			pt->tw->trace.plane.dist = plane[3];
			return qtrue;
		}
	} else {
		plane = pt->tc->squares[pt->i][pt->j].plane[0];
		enterFrac = CM_MOHAA_CheckTerrainPlane( pt, plane );

		if ( enterFrac < pt->tw->trace.fraction
			&& CM_MOHAA_ValidateTerrainCollidePointTri( pt, pt->tc->squares[pt->i][pt->j].eMode, enterFrac ) ) {
			pt->tw->trace.fraction = enterFrac;
			VectorCopy( plane, pt->tw->trace.plane.normal );
			pt->tw->trace.plane.dist = plane[3];
			return qtrue;
		}
	}

	return qfalse;
}

static qboolean CM_MOHAA_CheckStartInsideTerrain( const pointtrace_t *pt, int i, int j, float fx, float fy ) {
	const float	*plane;
	float		fDot;

	if ( i < 0 || i > 7 || j < 0 || j > 7 ) {
		return qfalse;
	}

	if ( !pt->tc->squares[i][j].eMode ) {
		return qfalse;
	}

	if ( ( i + j ) & 1 ) {
		if ( fx + fy >= 1 ) {
			if ( pt->tc->squares[i][j].eMode == 6 ) {
				return qfalse;
			}
			plane = pt->tc->squares[i][j].plane[0];
		} else {
			if ( pt->tc->squares[i][j].eMode == 3 ) {
				return qfalse;
			}
			plane = pt->tc->squares[i][j].plane[1];
		}
	} else {
		if ( fy >= fx ) {
			if ( pt->tc->squares[i][j].eMode == 5 ) {
				return qfalse;
			}
			plane = pt->tc->squares[i][j].plane[0];
		} else {
			if ( pt->tc->squares[i][j].eMode == 4 ) {
				return qfalse;
			}
			plane = pt->tc->squares[i][j].plane[1];
		}
	}

	fDot = DotProduct( pt->vStart, plane );
	if ( fDot <= plane[3] && fDot + 32 >= plane[3] ) {
		return qtrue;
	}

	return qfalse;
}

static qboolean CM_MOHAA_PositionTestPointInTerrainCollide( const pointtrace_t *pt ) {
	int		i0, j0;
	float	fx, fy;

	fx = (float)( ( pt->vStart[0] - pt->tc->vBounds[0][0] ) * TERRAIN_UNIT );
	fy = (float)( ( pt->vStart[1] - pt->tc->vBounds[0][1] ) * TERRAIN_UNIT );

	i0 = (int)floor( fx );
	j0 = (int)floor( fy );

	return CM_MOHAA_CheckStartInsideTerrain( pt, i0, j0, fx - i0, fy - j0 );
}

/* Walks the squares a segment crosses. `sight` stops at the first hit and
 * reports whether the segment was clear; otherwise the trace keeps the hit. */
static qboolean CM_MOHAA_WalkTerrainSquares( pointtrace_t *pt, qboolean sight ) {
	int		i0, j0, i1, j1;
	int		di, dj;
	int		d1, d2;
	float	fx, fy;
	float	dx, dy, dx2, dy2;
	float	dfx, dfy;

	fx = (float)( ( pt->vStart[0] - pt->tc->vBounds[0][0] ) * TERRAIN_UNIT );
	fy = (float)( ( pt->vStart[1] - pt->tc->vBounds[0][1] ) * TERRAIN_UNIT );
	i0 = (int)floor( fx );
	j0 = (int)floor( fy );
	i1 = (int)floor( ( pt->vEnd[0] - pt->tc->vBounds[0][0] ) * TERRAIN_UNIT );
	j1 = (int)floor( ( pt->vEnd[1] - pt->tc->vBounds[0][1] ) * TERRAIN_UNIT );

	dfx = fx - i0;
	dfy = fy - j0;

	if ( CM_MOHAA_CheckStartInsideTerrain( pt, i0, j0, dfx, dfy ) ) {
		if ( sight ) {
			return qfalse;
		}
		pt->tw->trace.startsolid = qtrue;
		pt->tw->trace.allsolid = qtrue;
		pt->tw->trace.fraction = 0;
		return qfalse;
	}

	if ( i0 == i1 ) {
		if ( i0 < 0 || i0 > 7 ) {
			return qtrue;
		}

		if ( j0 == j1 ) {
			if ( j0 < 0 || j0 > 7 ) {
				return qtrue;
			}
			pt->i = i0;
			pt->j = j0;
			return !CM_MOHAA_TestTerrainCollideSquare( pt );
		}
		if ( j0 >= j1 ) {
			if ( j0 > 7 ) {
				j0 = 7;
			}
			if ( j1 < 0 ) {
				j1 = 0;
			}
			pt->i = i0;
			for ( pt->j = j0; pt->j >= j1; pt->j-- ) {
				if ( CM_MOHAA_TestTerrainCollideSquare( pt ) ) {
					return qfalse;
				}
			}
		} else {
			if ( j0 < 0 ) {
				j0 = 0;
			}
			if ( j1 > 7 ) {
				j1 = 7;
			}
			pt->i = i0;
			for ( pt->j = j0; pt->j <= j1; pt->j++ ) {
				if ( CM_MOHAA_TestTerrainCollideSquare( pt ) ) {
					return qfalse;
				}
			}
		}
		return qtrue;
	}

	if ( j0 == j1 ) {
		if ( j0 < 0 || j0 > 7 ) {
			return qtrue;
		}

		if ( i0 >= i1 ) {
			if ( i0 > 7 ) {
				i0 = 7;
			}
			if ( i1 < 0 ) {
				i1 = 0;
			}
			pt->j = j0;
			for ( pt->i = i0; pt->i >= i1; pt->i-- ) {
				if ( CM_MOHAA_TestTerrainCollideSquare( pt ) ) {
					return qfalse;
				}
			}
		} else {
			if ( i0 < 0 ) {
				i0 = 0;
			}
			if ( i1 > 7 ) {
				i1 = 7;
			}
			pt->j = j0;
			for ( pt->i = i0; pt->i <= i1; pt->i++ ) {
				if ( CM_MOHAA_TestTerrainCollideSquare( pt ) ) {
					return qfalse;
				}
			}
		}
		return qtrue;
	}

	dx = pt->vEnd[0] - pt->vStart[0];
	dy = pt->vEnd[1] - pt->vStart[1];

	/* The direction comes from the squares, not from a float's sign, so a
	 * NaN delta cannot turn the walk around (OpenMoHAA's correction). */
	if ( i1 > i0 ) {
		d1 = 1;
		di = i1 - i0;
		dx2 = ( i0 + 1 - fx ) * dy;
	} else {
		d1 = -1;
		di = i0 - i1;
		dx = -dx;
		dx2 = dfx * dy;
	}

	if ( j1 > j0 ) {
		d2 = 1;
		dj = di + j1 - j0 + 1;
		dy2 = ( j0 + 1 - fy ) * dx;
	} else {
		d2 = -1;
		dy = -dy;
		dj = di + j0 - j1 + 1;
		dy2 = dfy * dx;
		dx2 = -dx2;
	}

	pt->i = i0;
	pt->j = j0;

	for ( ;; ) {
		if ( pt->i >= 0 && pt->i <= 7 && pt->j >= 0 && pt->j <= 7 ) {
			if ( CM_MOHAA_TestTerrainCollideSquare( pt ) ) {
				return qfalse;
			}
		}

		dj--;
		if ( !dj ) {
			break;
		}

		if ( dx2 < dy2 ) {
			dy2 -= dx2;
			dx2 = dy;
			pt->i += d1;
		} else {
			dx2 -= dy2;
			dy2 = dx;
			pt->j += d2;
		}
	}

	return qtrue;
}

static void CM_MOHAA_TraceCylinderThroughTerrainCollide( pointtrace_t *pt, traceWork_t *tw, const mohaaTerrainCollide_t *tc ) {
	int		i0, j0, i1, j1;
	float	x0, y0;
	float	enterFrac;

	/* OpenMoHAA floors before it scales, as here. */
	i0 = (int)( floor( tw->bounds[0][0] - tc->vBounds[0][0] ) * TERRAIN_UNIT );
	i1 = (int)( floor( tw->bounds[1][0] - tc->vBounds[0][0] ) * TERRAIN_UNIT );
	j0 = (int)( floor( tw->bounds[0][1] - tc->vBounds[0][1] ) * TERRAIN_UNIT );
	j1 = (int)( floor( tw->bounds[1][1] - tc->vBounds[0][1] ) * TERRAIN_UNIT );

	if ( i0 < 0 ) {
		i0 = 0;
	}
	if ( j0 < 0 ) {
		j0 = 0;
	}
	if ( i1 > 7 ) {
		i1 = 7;
	}
	if ( j1 > 7 ) {
		j1 = 7;
	}

	y0 = ( j0 << 6 ) + tc->vBounds[0][1];
	for ( pt->j = j0; pt->j <= j1; pt->j++ ) {
		x0 = ( i0 << 6 ) + tc->vBounds[0][0];
		for ( pt->i = i0; pt->i <= i1; pt->i++ ) {
			const mohaaTerrainCollideSquare_t *square = &tc->squares[pt->i][pt->j];
			int plane;

			for ( plane = 0; plane < 2; plane++ ) {
				switch ( square->eMode ) {
				case 1:
				case 2:
					break;
				case 3:
				case 4:
					if ( plane != 0 ) {
						continue;
					}
					break;
				case 5:
				case 6:
					if ( plane != 1 ) {
						continue;
					}
					break;
				default:
					continue;
				}
				enterFrac = CM_MOHAA_CheckTerrainTriSphere( pt, x0, y0, plane );
				if ( enterFrac < 0 ) {
					enterFrac = 0;
				}
				if ( enterFrac < pt->tw->trace.fraction ) {
					pt->tw->trace.fraction = enterFrac;
					VectorCopy( square->plane[plane], pt->tw->trace.plane.normal );
					pt->tw->trace.plane.dist = square->plane[plane][3];
				}
			}
			x0 += 64;
		}

		y0 += 64;
	}
}

static qboolean CM_MOHAA_TerrainBoundsMiss( const traceWork_t *tw, const mohaaTerrainCollide_t *tc ) {
	return tw->bounds[0][0] >= tc->vBounds[1][0] || tw->bounds[1][0] <= tc->vBounds[0][0]
		|| tw->bounds[0][1] >= tc->vBounds[1][1] || tw->bounds[1][1] <= tc->vBounds[0][1]
		|| tw->bounds[0][2] >= tc->vBounds[1][2] || tw->bounds[1][2] <= tc->vBounds[0][2];
}

static qboolean CM_MOHAA_UseSphere( const traceWork_t *tw ) {
	return tw->sphere.use && cm_ter_usesphere && cm_ter_usesphere->integer;
}

/*
====================
CM_MOHAA_TraceThroughTerrainCollide
====================
*/
void CM_MOHAA_TraceThroughTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc ) {
	int				i;
	pointtrace_t	pt;

	if ( CM_MOHAA_TerrainBoundsMiss( tw, tc ) ) {
		return;
	}

	Com_Memset( &pt, 0, sizeof( pt ) );
	pt.tw = tw;
	pt.tc = tc;

	if ( CM_MOHAA_UseSphere( tw ) ) {
		VectorSubtract( tw->start, tw->sphere.offset, pt.vStart );
		VectorSubtract( tw->end, tw->sphere.offset, pt.vEnd );
		CM_MOHAA_TraceCylinderThroughTerrainCollide( &pt, tw, tc );
	} else if ( tw->isPoint ) {
		VectorCopy( tw->start, pt.vStart );
		VectorCopy( tw->end, pt.vEnd );
		CM_MOHAA_WalkTerrainSquares( &pt, qfalse );
	} else {
		/* Upward-facing terrain meets a box at its lower corners, the other
		 * kind at its upper ones. */
		const int first = tc->squares[0][0].plane[0][2] >= 0 ? 0 : 4;

		for ( i = first; i < first + 4; i++ ) {
			VectorAdd( tw->start, tw->offsets[i], pt.vStart );
			VectorAdd( tw->end, tw->offsets[i], pt.vEnd );

			CM_MOHAA_WalkTerrainSquares( &pt, qfalse );
			if ( tw->trace.allsolid ) {
				return;
			}
		}
	}
}

/*
====================
CM_MOHAA_PositionTestInTerrainCollide
====================
*/
qboolean CM_MOHAA_PositionTestInTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc ) {
	int				i;
	pointtrace_t	pt;

	if ( CM_MOHAA_TerrainBoundsMiss( tw, tc ) ) {
		return qfalse;
	}

	Com_Memset( &pt, 0, sizeof( pt ) );
	pt.tw = tw;
	pt.tc = tc;

	if ( CM_MOHAA_UseSphere( tw ) ) {
		VectorSubtract( tw->start, tw->sphere.offset, pt.vStart );
		VectorSubtract( tw->end, tw->sphere.offset, pt.vEnd );
		CM_MOHAA_TraceCylinderThroughTerrainCollide( &pt, tw, tc );
		return tw->trace.startsolid;
	}
	if ( tw->isPoint ) {
		VectorCopy( tw->start, pt.vStart );
		VectorCopy( tw->end, pt.vEnd );
		return CM_MOHAA_PositionTestPointInTerrainCollide( &pt );
	}

	{
		const int first = tc->squares[0][0].plane[0][2] >= 0 ? 0 : 4;

		for ( i = first; i < first + 4; i++ ) {
			VectorAdd( tw->start, tw->offsets[i], pt.vStart );
			VectorAdd( tw->end, tw->offsets[i], pt.vEnd );

			if ( CM_MOHAA_PositionTestPointInTerrainCollide( &pt ) ) {
				return qtrue;
			}
		}
	}

	return qfalse;
}

/*
====================
CM_MOHAA_SightTraceThroughTerrainCollide

True when the segment is clear of this patch. A sight trace neither
records a hit nor uses the capsule, as OpenMoHAA's does not.
====================
*/
qboolean CM_MOHAA_SightTraceThroughTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc ) {
	int				i;
	pointtrace_t	pt;

	/* OpenMoHAA reports a miss of the patch's bounds as blocked; a segment
	 * that never reaches the patch cannot be blocked by it. */
	if ( CM_MOHAA_TerrainBoundsMiss( tw, tc ) ) {
		return qtrue;
	}

	Com_Memset( &pt, 0, sizeof( pt ) );
	pt.tw = tw;
	pt.tc = tc;

	if ( tw->isPoint ) {
		VectorCopy( tw->start, pt.vStart );
		VectorCopy( tw->end, pt.vEnd );
		return CM_MOHAA_WalkTerrainSquares( &pt, qtrue );
	}

	{
		const int first = tc->squares[0][0].plane[0][2] >= 0 ? 0 : 4;

		for ( i = first; i < first + 4; i++ ) {
			VectorAdd( tw->start, tw->offsets[i], pt.vStart );
			VectorAdd( tw->end, tw->offsets[i], pt.vEnd );

			if ( !CM_MOHAA_WalkTerrainSquares( &pt, qtrue ) ) {
				return qfalse;
			}
		}
	}

	return qtrue;
}
