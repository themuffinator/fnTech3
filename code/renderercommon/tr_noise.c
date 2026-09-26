/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

#define NOISE_SIZE 256
#define NOISE_MASK ( NOISE_SIZE - 1 )

#define VAL( a ) s_noise_perm[ ( a ) & ( NOISE_MASK )]
#define INDEX( x, y, z, t ) VAL( x + VAL( y + VAL( z + VAL( t ) ) ) )

static float s_noise_table[NOISE_SIZE];
static int s_noise_perm[NOISE_SIZE];

static float GetNoiseValue( int x, int y, int z, int t )
{
	int index = INDEX( ( int ) x, ( int ) y, ( int ) z, ( int ) t );

	return s_noise_table[index];
}

void R_NoiseInit( void )
{
	int i;

	if ( ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP ) {
		// Retail SP/MP seed at 1001 and use the same 15-bit sequence
		// (stvoy.exe 0x458ad0, stvoyHM.exe 0x46bf40). Keep it local so the
		// table is portable and does not reseed the engine's other randomness.
		uint32_t state = 1001;
		// Exact values of retail's float scale words 0x38000100/0x3bff01fe.
		const double valueScale = 0x1.0002p-15;
		const double permutationScale = 0x1.fe03fcp-8;
		for ( i = 0; i < NOISE_SIZE; i++ ) {
			state = state * 214013u + 2531011u;
			s_noise_table[i] = (float)( ( ( state >> 16 ) & 32767u ) * valueScale * 2.0 - 1.0 );
			state = state * 214013u + 2531011u;
			s_noise_perm[i] = (int)( ( ( state >> 16 ) & 32767u ) * permutationScale ) & NOISE_MASK;
		}
		return;
	}

	for ( i = 0; i < NOISE_SIZE; i++ )
	{
		s_noise_table[i] = ( float ) ( ( ( rand() / ( float ) RAND_MAX ) * 2.0 - 1.0 ) );
		s_noise_perm[i] = ( unsigned char ) ( rand() / ( float ) RAND_MAX * 255 );
	}
}

/* Elite Force's random waveform reads one noise value, moved to 0..2, chosen
 * by the low byte of its clock (stvoyHM.exe 0x46bf20). */
float R_NoiseRandom( int index )
{
	return s_noise_table[ s_noise_perm[ index & NOISE_MASK ] ] + 1.0f;
}

float R_NoiseGet4f( float x, float y, float z, double t )
{
	int i;
	int ix, iy, iz, it;
	float fx, fy, fz, ft;
	float front[4];
	float back[4];
	float fvalue, bvalue, value[2], finalvalue;

	ix = ( int ) floor( x );
	fx = x - ix;
	iy = ( int ) floor( y );
	fy = y - iy;
	iz = ( int ) floor( z );
	fz = z - iz;
	it = ( int ) floor( t );
	ft = t - it;

	for ( i = 0; i < 2; i++ )
	{
		front[0] = GetNoiseValue( ix, iy, iz, it + i );
		front[1] = GetNoiseValue( ix+1, iy, iz, it + i );
		front[2] = GetNoiseValue( ix, iy+1, iz, it + i );
		front[3] = GetNoiseValue( ix+1, iy+1, iz, it + i );

		back[0] = GetNoiseValue( ix, iy, iz + 1, it + i );
		back[1] = GetNoiseValue( ix+1, iy, iz + 1, it + i );
		back[2] = GetNoiseValue( ix, iy+1, iz + 1, it + i );
		back[3] = GetNoiseValue( ix+1, iy+1, iz + 1, it + i );

		fvalue = LERP( LERP( front[0], front[1], fx ), LERP( front[2], front[3], fx ), fy );
		bvalue = LERP( LERP( back[0], back[1], fx ), LERP( back[2], back[3], fx ), fy );

		value[i] = LERP( fvalue, bvalue, fz );
	}

	finalvalue = LERP( value[0], value[1], ft );

	return finalvalue;
}
