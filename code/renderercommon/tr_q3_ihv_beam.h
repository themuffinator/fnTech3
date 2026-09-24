/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_BEAM_H
#define FNTECH3_TR_Q3_IHV_BEAM_H

#include "tr_q3_ihv_entities.h"
#include <math.h>

/* Stored intermediates reproduce the original numeric helpers' precision-53
 * arithmetic. These functions operate only on validated, bounded vectors. */
static inline int Q3IHV_BeamNormalize( float vector[3] ) {
	volatile double x = (double)vector[0] * vector[0];
	volatile double y = (double)vector[1] * vector[1];
	volatile double z = (double)vector[2] * vector[2];
	volatile double length = x + y;
	volatile double inverse;
	int i;
	length = length + z;
	length = sqrt( length );
	if ( length == 0 ) return 0;
	inverse = 1 / length;
	for ( i = 0; i < 3; ++i ) {
		volatile double value = inverse * vector[i];
		vector[i] = (float)value;
	}
	return 1;
}

static inline void Q3IHV_BeamPerpendicular( const float direction[3], float output[3] ) {
	float unit[3] = { 0, 0, 0 }, minimum = 1;
	volatile double squares[3], denominator, inverse, projected, z, y, x;
	volatile float distance, normalXY[2];
	int i, selected = 0;
	for ( i = 0; i < 3; ++i ) if ( fabs( direction[i] ) < minimum ) {
		minimum = (float)fabs( direction[i] ); selected = i;
	}
	unit[selected] = 1;
	for ( i = 0; i < 3; ++i ) squares[i] = (double)direction[i] * direction[i];
	denominator = squares[0] + squares[1]; denominator = denominator + squares[2];
	inverse = 1 / denominator;
	z = (double)unit[2] * direction[2]; y = (double)unit[1] * direction[1]; x = (double)unit[0] * direction[0];
	projected = z + y; projected = projected + x; projected = projected * inverse;
	distance = (float)projected;
	/* The original spills x/y but retains the z product in its FP register. */
	normalXY[0] = (float)( inverse * direction[0] ); normalXY[1] = (float)( inverse * direction[1] );
	for ( i = 0; i < 3; ++i ) {
		volatile double normal = i < 2 ? normalXY[i] : inverse * direction[2];
		volatile double scaled = normal * distance;
		scaled = (double)unit[i] - scaled; output[i] = (float)scaled;
	}
	Q3IHV_BeamNormalize( output );
}

static inline void Q3IHV_BeamMatrix( const float a[3][3], const float b[3][3], float output[3][3] ) {
	int row, column;
	for ( row = 0; row < 3; ++row ) for ( column = 0; column < 3; ++column ) {
		volatile double x = (double)a[row][0] * b[0][column];
		volatile double y = (double)a[row][1] * b[1][column];
		volatile double z = (double)a[row][2] * b[2][column];
		volatile double sum;
		if ( column ) { sum = y + z; sum = sum + x; }
		else if ( row == 2 ) { sum = x + z; sum = sum + y; }
		else { sum = x + y; sum = sum + z; }
		output[row][column] = (float)sum;
	}
}

/* Original 0x442450: six segments, radius four, local origin zero. The
 * surrounding entity transform positions the beam. Twelve unique vertices
 * are enough; the draw path closes the strip with indices, exactly repeating
 * the first pair instead of evaluating a slightly different 360-degree pose.
 * Zero length and invalid/extreme endpoints leave output unchanged. */
static inline int Q3IHV_BeamVertices( const float origin[3], const float oldorigin[3], float output[12][3] ) {
	float direction[3], forward[3], right[3], up[3], point[3], basis[3][3], transpose[3][3], next[12][3];
	int i, row, column;
	if ( !origin || !oldorigin || !output ) return 0;
	for ( i = 0; i < 3; ++i ) {
		volatile double delta;
		if ( !Q3IHV_EntityFinite( origin + i ) || !Q3IHV_EntityFinite( oldorigin + i ) ) return 0;
		delta = (double)oldorigin[i] - origin[i];
		if ( delta > FLT_MAX || delta < -FLT_MAX ) return 0;
		direction[i] = forward[i] = (float)delta;
	}
	if ( !Q3IHV_BeamNormalize( forward ) ) return 0;
	Q3IHV_BeamPerpendicular( forward, right );
	for ( i = 0; i < 3; ++i ) {
		volatile double a = (double)right[(i + 1) % 3] * forward[(i + 2) % 3];
		volatile double b = (double)right[(i + 2) % 3] * forward[(i + 1) % 3];
		a = a - b; up[i] = (float)a;
		point[i] = (float)( (double)right[i] * 4 );
		basis[i][0] = right[i]; basis[i][1] = up[i]; basis[i][2] = forward[i];
	}
	for ( row = 0; row < 3; ++row ) for ( column = 0; column < 3; ++column ) transpose[row][column] = basis[column][row];
	for ( i = 0; i < 6; ++i ) {
		float rotation[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 1 } }, intermediate[3][3], matrix[3][3];
		volatile double radians = (double)( i * 60 ) * 3.141592653589793;
		volatile float sineAngle, cosine, sine;
		radians = radians * ( 1.0 / 180.0 );
		sineAngle = (float)radians;
		cosine = (float)cos( radians ); sine = (float)sin( (double)sineAngle );
		rotation[0][0] = rotation[1][1] = cosine;
		rotation[0][1] = sine; rotation[1][0] = -sine;
		Q3IHV_BeamMatrix( basis, rotation, intermediate );
		Q3IHV_BeamMatrix( intermediate, transpose, matrix );
		for ( row = 0; row < 3; ++row ) {
			volatile double x = (double)matrix[row][0] * point[0];
			volatile double y = (double)matrix[row][1] * point[1];
			volatile double z = (double)matrix[row][2] * point[2];
			volatile double value = x + z;
			value = value + y; next[i * 2][row] = (float)value;
			value = (double)next[i * 2][row] + direction[row];
			if ( value > FLT_MAX || value < -FLT_MAX ) return 0;
			next[i * 2 + 1][row] = (float)value;
		}
	}
	memcpy( output, next, sizeof( next ) );
	return 1;
}

#endif
