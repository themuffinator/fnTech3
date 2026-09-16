/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDC_DECODE_H
#define FNTECH3_TR_MDC_DECODE_H

#include "tr_mdc_format.h"

typedef struct {
	float directions[256][3];
	float sine[1024];
} fnMdcNormals_t;

/* Construct the direction dictionary geometrically. The six-decimal encoding
 * and the handful of rounding residues below are format facts observed in the
 * original RTCW SP/MP and iortcw dictionaries, not imported implementation.
 * The poles have four directions; each successive latitude ring adds four.
 */
static inline void FnMdc_InitNormals( fnMdcNormals_t *table ) {
	static const struct { unsigned char index, component; float value; } residues[] = {
		{21, 1, -0.831469f}, {22, 0, -0.382684f}, {26, 0, 0.382684f}, {26, 1, -0.923879f},
		{75, 0, -0.653282f}, {81, 1, -0.653282f}, {83, 1, -0.239117f},
		{97, 0, -0.488725f}, {101, 0, 0.488725f}, {109, 1, 0.653282f}, {119, 0, 0.653282f}
	};
	const double pi = 3.14159265358979323846;
	int hemisphere, level, column, index = 0, i;
	for ( hemisphere = 0; hemisphere < 2; ++hemisphere ) {
		for ( level = hemisphere; level < 8; ++level ) {
			const int count = 32 - 4 * level;
			const double radius = cos( level * pi / 16 );
			const double z = level ? sin( level * pi / 16 ) * ( hemisphere ? 1 : -1 ) : 0;
			for ( column = 0; column < count; ++column, ++index ) {
				double direction[3];
				direction[0] = radius * cos( column * 2 * pi / count );
				direction[1] = radius * sin( column * 2 * pi / count );
				direction[2] = z;
				for ( i = 0; i < 3; ++i ) table->directions[index][i] =
					(float)copysign( floor( fabs( direction[i] ) * 1000000 + 0.5 ) / 1000000, direction[i] );
				/* Preserve the signs of the dictionary's cardinal zeroes. */
				if ( column == count / 4 ) table->directions[index][0] = -0.0f;
				if ( column == count / 2 ) table->directions[index][1] = -0.0f;
				if ( column == count * 3 / 4 ) table->directions[index][0] = 0.0f;
			}
		}
	}
	for ( i = 0; i < (int)( sizeof( residues ) / sizeof( residues[0] ) ); ++i ) {
		table->directions[residues[i].index][residues[i].component] = residues[i].value;
		if ( residues[i].index >= 32 )
			table->directions[residues[i].index + 112][residues[i].component] = residues[i].value;
	}
	/* RTCW's packed base normals use a 1023-period sine table. fnTech3's Q3/QL
 * waveform table has a different period and bias, so it cannot substitute. */
	for ( i = 0; i < 1024; ++i ) {
		float angle = i * 360.0f / 1023.0f;
		angle = ( angle * 3.14159265358979323846f ) / 180.0f;
		table->sine[i] = (float)sin( angle );
	}
}

static inline void FnMdc_DecodeNormal( const fnMdcNormals_t *table, const fnMdcVertex_t *vertex,
	float destination[3] ) {
	if ( vertex->compressedNormal ) {
		memcpy( destination, table->directions[vertex->normal & 255], 3 * sizeof( float ) );
	} else {
		const unsigned lat = ( vertex->normal >> 8 ) * 4, lng = ( vertex->normal & 255 ) * 4;
		destination[0] = table->sine[( lat + 256 ) & 1023] * table->sine[lng];
		destination[1] = table->sine[lat] * table->sine[lng];
		destination[2] = table->sine[( lng + 256 ) & 1023];
	}
}

/* Validated immutable surface, with transactional output. Finite extrapolation
 * is supported, as it is by the original module contract. */
static inline int FnMdc_AnimatedVertex( const fnMdcSurfaceView_t *surface, const fnMdcNormals_t *table,
	uint32_t oldFrame, uint32_t frame, float backlerp, uint32_t vertex, float xyz[3], float normal[3] ) {
	fnMdcVertex_t current, previous;
	float position[3], direction[3], oldNormal[3];
	int i;
	if ( !table || !xyz || !normal || !isfinite( backlerp ) ||
		!FnMdc_Vertex( surface, frame, vertex, &current ) ) return 0;
	FnMdc_DecodeNormal( table, &current, direction );
	memcpy( position, current.xyz, sizeof( position ) );
	if ( backlerp != 0 ) {
		const float frontlerp = 1.0f - backlerp;
		float length;
		if ( !FnMdc_Vertex( surface, oldFrame, vertex, &previous ) ) return 0;
		FnMdc_DecodeNormal( table, &previous, oldNormal );
		for ( i = 0; i < 3; ++i ) {
			position[i] = previous.base[i] * backlerp + current.base[i] * frontlerp;
			if ( current.compressedNormal ) position[i] = (float)( position[i] + (1.0 - backlerp) * current.offset[i] );
			if ( previous.compressedNormal ) position[i] += backlerp * previous.offset[i];
			direction[i] = oldNormal[i] * backlerp + direction[i] * frontlerp;
		}
		length = (float)sqrt( direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2] );
		if ( length > 0 ) {
			const float inverse = 1.0f / length;
			for ( i = 0; i < 3; ++i ) direction[i] *= inverse;
		}
	}
	for ( i = 0; i < 3; ++i ) if ( !isfinite( position[i] ) || !isfinite( direction[i] ) ) return 0;
	memcpy( xyz, position, sizeof( position ) );
	memcpy( normal, direction, sizeof( direction ) );
	return 1;
}

#endif
