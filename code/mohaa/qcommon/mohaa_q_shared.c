/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) the OpenMoHAA team
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3. Its routines are taken from OpenMoHAA's
code/qcommon/q_math.c and code/qcommon/q_shared.c
(https://github.com/openmoh/openmohaa), which carry Quake III Arena's
source with OpenMoHAA's additions and are distributed under the GNU General
Public License, version 2 or (at your option) any later version. They
remain under that licence here.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

fnTech3 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.
===========================================================================
*/

/* The math and parsing the vendored skeletor, TIKI and alias sources expect
 * beyond fnTech3's q_math.c and q_shared.c, unchanged except for layout; see
 * q_shared.h beside this file. */

#include "q_shared.h"

#include <float.h>
#include <string.h>

enum { QX = 0, QY = 1, QZ = 2, QW = 3 };

short ShortNoSwapPtr( const void *l ) {
	short out;
	memcpy( &out, l, sizeof( out ) );
	return out;
}

int LongNoSwapPtr( const void *l ) {
	int out;
	memcpy( &out, l, sizeof( out ) );
	return out;
}

float FloatNoSwapPtr( const void *l ) {
	float out;
	memcpy( &out, l, sizeof( out ) );
	return out;
}

void QuatToMat( const float q[4], float m[3][3] ) {
	float wx, wy, wz;
	float xx, yy, yz;
	float xy, xz, zz;
	float x2, y2, z2;

	x2 = q[QX] + q[QX];
	y2 = q[QY] + q[QY];
	z2 = q[QZ] + q[QZ];

	xx = q[QX] * x2;
	xy = q[QX] * y2;
	xz = q[QX] * z2;

	yy = q[QY] * y2;
	yz = q[QY] * z2;
	zz = q[QZ] * z2;

	wx = q[QW] * x2;
	wy = q[QW] * y2;
	wz = q[QW] * z2;

	m[0][0] = 1.0f - ( yy + zz );
	m[0][1] = xy - wz;
	m[0][2] = xz + wy;

	m[1][0] = xy + wz;
	m[1][1] = 1.0f - ( xx + zz );
	m[1][2] = yz - wx;

	m[2][0] = xz - wy;
	m[2][1] = yz + wx;
	m[2][2] = 1.0f - ( xx + yy );
}

void MatToQuat( float srcMatrix[3][3], float destQuat[4] ) {
	double trace, s;
	int i, j, k;
	static const int next[3] = { QY, QZ, QX };

	trace = srcMatrix[QX][QX] + srcMatrix[QY][QY] + srcMatrix[QZ][QZ];

	if ( trace > 0.0 ) {
		s = sqrtf( (float)( trace + 1.0 ) );
		destQuat[QW] = (float)( s * 0.5 );
		s = 0.5 / s;

		destQuat[QX] = (float)( ( srcMatrix[QZ][QY] - srcMatrix[QY][QZ] ) * s );
		destQuat[QY] = (float)( ( srcMatrix[QX][QZ] - srcMatrix[QZ][QX] ) * s );
		destQuat[QZ] = (float)( ( srcMatrix[QY][QX] - srcMatrix[QX][QY] ) * s );
	} else {
		i = QX;
		if ( srcMatrix[QY][QY] > srcMatrix[QX][QX] ) {
			i = QY;
		}
		if ( srcMatrix[QZ][QZ] > srcMatrix[i][i] ) {
			i = QZ;
		}
		j = next[i];
		k = next[j];

		s = sqrt( ( srcMatrix[i][i] - ( srcMatrix[j][j] + srcMatrix[k][k] ) ) + 1.0 );
		destQuat[i] = (float)( s * 0.5 );

		s = 0.5 / s;

		destQuat[QW] = (float)( ( srcMatrix[k][j] - srcMatrix[j][k] ) * s );
		destQuat[j] = (float)( ( srcMatrix[j][i] + srcMatrix[i][j] ) * s );
		destQuat[k] = (float)( ( srcMatrix[k][i] + srcMatrix[i][k] ) * s );
	}
}

#define SLERP_DELTA 1e-6

void SlerpQuaternion( float from[4], float to[4], float t, float res[4] ) {
	float to1[4];
	double omega, cosom, sinom, scale0, scale1;

	cosom = from[QX] * to[QX] + from[QY] * to[QY] + from[QZ] * to[QZ] + from[QW] * to[QW];
	if ( cosom < 0.0 ) {
		cosom = -cosom;
		to1[QX] = -to[QX];
		to1[QY] = -to[QY];
		to1[QZ] = -to[QZ];
		to1[QW] = -to[QW];
	} else if ( from[QX] == to[QX] && from[QY] == to[QY] && from[QZ] == to[QZ] && from[QW] == to[QW] ) {
		// equal case, early exit
		res[QX] = to[QX];
		res[QY] = to[QY];
		res[QZ] = to[QZ];
		res[QW] = to[QW];
		return;
	} else {
		to1[QX] = to[QX];
		to1[QY] = to[QY];
		to1[QZ] = to[QZ];
		to1[QW] = to[QW];
	}

	if ( ( 1.0 - cosom ) > SLERP_DELTA ) {
		omega = acos( cosom );
		sinom = sin( omega );
		scale0 = sin( ( 1.0 - t ) * omega ) / sinom;
		scale1 = sin( t * omega ) / sinom;
	} else {
		scale0 = 1.0 - t;
		scale1 = t;
	}

	res[QX] = (float)( scale0 * from[QX] + scale1 * to1[QX] );
	res[QY] = (float)( scale0 * from[QY] + scale1 * to1[QY] );
	res[QZ] = (float)( scale0 * from[QZ] + scale1 * to1[QZ] );
	res[QW] = (float)( scale0 * from[QW] + scale1 * to1[QW] );
}

void EulerToQuat( float ang[3], float q[4] ) {
	float mat[3][3];
	int bits[3];

	memcpy( bits, ang, sizeof( bits ) );
	if ( !bits[0] && !bits[1] && !bits[2] ) {
		q[0] = 0;
		q[1] = 0;
		q[2] = 0;
		q[3] = 1.0f;
	} else {
		AnglesToAxis( ang, mat );
		MatToQuat( mat, q );
	}
}

void MatrixTransformVector( const vec3_t in, const float mat[3][3], vec3_t out ) {
	out[0] = ( in[0] * mat[0][0] ) + ( in[1] * mat[1][0] ) + ( in[2] * mat[2][0] );
	out[1] = ( in[0] * mat[0][1] ) + ( in[1] * mat[1][1] ) + ( in[2] * mat[2][1] );
	out[2] = ( in[0] * mat[0][2] ) + ( in[1] * mat[1][2] ) + ( in[2] * mat[2][2] );
}

void VectorMatrixInverse( void *DstMatrix, const void *SrcMatrix ) {
	typedef float Float4x4[4][4];
	Float4x4 M;
	Float4x4 Result;
	float Det[4];
	Float4x4 Tmp;
	float Determinant, RDet;

	memcpy( M, SrcMatrix, sizeof( Float4x4 ) );

	Tmp[0][0] = M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[0][1] = M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[0][2] = M[1][2] * M[2][3] - M[1][3] * M[2][2];

	Tmp[1][0] = M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[1][1] = M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[1][2] = M[0][2] * M[2][3] - M[0][3] * M[2][2];

	Tmp[2][0] = M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[2][1] = M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[2][2] = M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Tmp[3][0] = M[1][2] * M[2][3] - M[1][3] * M[2][2];
	Tmp[3][1] = M[0][2] * M[2][3] - M[0][3] * M[2][2];
	Tmp[3][2] = M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Det[0] = M[1][1] * Tmp[0][0] - M[2][1] * Tmp[0][1] + M[3][1] * Tmp[0][2];
	Det[1] = M[0][1] * Tmp[1][0] - M[2][1] * Tmp[1][1] + M[3][1] * Tmp[1][2];
	Det[2] = M[0][1] * Tmp[2][0] - M[1][1] * Tmp[2][1] + M[3][1] * Tmp[2][2];
	Det[3] = M[0][1] * Tmp[3][0] - M[1][1] * Tmp[3][1] + M[2][1] * Tmp[3][2];

	Determinant = M[0][0] * Det[0] - M[1][0] * Det[1] + M[2][0] * Det[2] - M[3][0] * Det[3];
	RDet = 1.0f / Determinant;

	Result[0][0] = RDet * Det[0];
	Result[0][1] = -RDet * Det[1];
	Result[0][2] = RDet * Det[2];
	Result[0][3] = -RDet * Det[3];
	Result[1][0] = -RDet * ( M[1][0] * Tmp[0][0] - M[2][0] * Tmp[0][1] + M[3][0] * Tmp[0][2] );
	Result[1][1] = RDet * ( M[0][0] * Tmp[1][0] - M[2][0] * Tmp[1][1] + M[3][0] * Tmp[1][2] );
	Result[1][2] = -RDet * ( M[0][0] * Tmp[2][0] - M[1][0] * Tmp[2][1] + M[3][0] * Tmp[2][2] );
	Result[1][3] = RDet * ( M[0][0] * Tmp[3][0] - M[1][0] * Tmp[3][1] + M[2][0] * Tmp[3][2] );
	Result[2][0] = RDet * (
		M[1][0] * ( M[2][1] * M[3][3] - M[2][3] * M[3][1] ) -
		M[2][0] * ( M[1][1] * M[3][3] - M[1][3] * M[3][1] ) +
		M[3][0] * ( M[1][1] * M[2][3] - M[1][3] * M[2][1] ) );
	Result[2][1] = -RDet * (
		M[0][0] * ( M[2][1] * M[3][3] - M[2][3] * M[3][1] ) -
		M[2][0] * ( M[0][1] * M[3][3] - M[0][3] * M[3][1] ) +
		M[3][0] * ( M[0][1] * M[2][3] - M[0][3] * M[2][1] ) );
	Result[2][2] = RDet * (
		M[0][0] * ( M[1][1] * M[3][3] - M[1][3] * M[3][1] ) -
		M[1][0] * ( M[0][1] * M[3][3] - M[0][3] * M[3][1] ) +
		M[3][0] * ( M[0][1] * M[1][3] - M[0][3] * M[1][1] ) );
	Result[2][3] = -RDet * (
		M[0][0] * ( M[1][1] * M[2][3] - M[1][3] * M[2][1] ) -
		M[1][0] * ( M[0][1] * M[2][3] - M[0][3] * M[2][1] ) +
		M[2][0] * ( M[0][1] * M[1][3] - M[0][3] * M[1][1] ) );
	Result[3][0] = -RDet * (
		M[1][0] * ( M[2][1] * M[3][2] - M[2][2] * M[3][1] ) -
		M[2][0] * ( M[1][1] * M[3][2] - M[1][2] * M[3][1] ) +
		M[3][0] * ( M[1][1] * M[2][2] - M[1][2] * M[2][1] ) );
	Result[3][1] = RDet * (
		M[0][0] * ( M[2][1] * M[3][2] - M[2][2] * M[3][1] ) -
		M[2][0] * ( M[0][1] * M[3][2] - M[0][2] * M[3][1] ) +
		M[3][0] * ( M[0][1] * M[2][2] - M[0][2] * M[2][1] ) );
	Result[3][2] = -RDet * (
		M[0][0] * ( M[1][1] * M[3][2] - M[1][2] * M[3][1] ) -
		M[1][0] * ( M[0][1] * M[3][2] - M[0][2] * M[3][1] ) +
		M[3][0] * ( M[0][1] * M[1][2] - M[0][2] * M[1][1] ) );
	Result[3][3] = RDet * (
		M[0][0] * ( M[1][1] * M[2][2] - M[1][2] * M[2][1] ) -
		M[1][0] * ( M[0][1] * M[2][2] - M[0][2] * M[2][1] ) +
		M[2][0] * ( M[0][1] * M[1][2] - M[0][2] * M[1][1] ) );

	memcpy( DstMatrix, Result, 16 * sizeof( float ) );
}

void QuatToAngles( const float q[4], vec3_t angles ) {
	float q2[4];

	q2[0] = q[0] * q[0];
	q2[1] = q[1] * q[1];
	q2[2] = q[2] * q[2];
	q2[3] = q[3] * q[3];

	angles[PITCH] = RAD2DEG( asin( -2 * ( q[2] * q[0] - q[3] * q[1] ) ) );
	angles[YAW] = RAD2DEG( atan2( 2 * ( q[2] * q[3] + q[0] * q[1] ), ( q2[2] - q2[3] - q2[0] + q2[1] ) ) );
	angles[ROLL] = RAD2DEG( atan2( 2 * ( q[3] * q[0] + q[2] * q[1] ), ( -q2[2] - q2[3] + q2[0] + q2[1] ) ) );
}

void MatrixToEulerAngles( const float mat[3][3], vec3_t ang ) {
	double theta;
	double cp;
	double sp;

	sp = mat[0][2];

	// cap off our sin value so that we don't get any NANs
	if ( sp > 1.0 ) {
		sp = 1.0;
	}
	if ( sp < -1.0 ) {
		sp = -1.0;
	}

	theta = -asin( sp );
	cp = cos( theta );

	if ( cp > 8192 * FLT_EPSILON ) {
		ang[0] = (float)( theta * 180 / M_PI );
		ang[1] = (float)( atan2( mat[0][1], mat[0][0] ) * 180 / M_PI );
		ang[2] = (float)( atan2( mat[1][2], mat[2][2] ) * 180 / M_PI );
	} else {
		ang[0] = (float)( theta * 180 / M_PI );
		ang[1] = (float)( -atan2( mat[1][0], mat[1][1] ) * 180 / M_PI );
		ang[2] = 0;
	}
}

void TransposeMatrix( float in[3][3], float out[3][3] ) {
	out[0][0] = in[0][0];
	out[0][1] = in[1][0];
	out[0][2] = in[2][0];
	out[1][0] = in[0][1];
	out[1][1] = in[1][1];
	out[1][2] = in[2][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
	out[2][2] = in[2][2];
}

/* Ritual's tokenizer, which Medal of Honor's alias parameters are read with.
 * The token buffer holds the longest token the loop can store; upstream's is
 * smaller than its own bound. */
static char com_token[MOHAA_MAX_STRING_CHARS + 1];

const char *COM_GetToken( char **data_p, qboolean crossline ) {
	int c;
	int len;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if ( !data ) {
		*data_p = NULL;
		return "";
	}

	// skip whitespace
skipwhite:
	while ( ( c = *data ) <= ' ' ) {
		if ( c == '\n' && !crossline ) {
			*data_p = data;
			return "";
		}
		if ( !c ) {
			*data_p = NULL;
			return "";
		}
		data++;
	}

	// skip // comments
	if ( c == '/' && data[1] == '/' ) {
		while ( *data && *data != '\n' ) {
			data++;
		}
		goto skipwhite;
	}

	// skip /* comments
	if ( c == '/' && data[1] == '*' ) {
		data++;
		while ( *data ) {
			if ( ( *( data - 1 ) == '*' ) && ( *data == '/' ) ) {
				break;
			}
			data++;
		}
		while ( *data && *data != '\n' ) {
			data++;
		}
		goto skipwhite;
	}

	// handle quoted strings specially
	if ( c == '"' ) {
		data++;
		while ( 1 ) {
			c = *data++;
			if ( c == '\\' && *data == '"' ) {
				if ( len < MOHAA_MAX_STRING_CHARS ) {
					com_token[len] = '"';
					len++;
				}
				data++;
			} else if ( c == '"' || !c ) {
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			} else if ( len < MOHAA_MAX_STRING_CHARS ) {
				if ( c == '\\' && *data == 'n' ) {
					com_token[len] = '\n';
					data++;
				} else {
					com_token[len] = (char)c;
				}
				len++;
			}
		}
	}

	// parse a regular word
	do {
		if ( len < MOHAA_MAX_STRING_CHARS ) {
			com_token[len] = (char)c;
			len++;
		}
		data++;
		c = *data;
	} while ( c > 32 );

	if ( len == MOHAA_MAX_STRING_CHARS ) {
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}
