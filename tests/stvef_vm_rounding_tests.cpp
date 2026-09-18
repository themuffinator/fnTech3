/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

// Elite Force QVMs convert floats to integers by rounding to nearest, ties to
// even, as retail's x87 fistp does in its default mode; Quake III truncates.
// The interpreter's conversion must equal the x86 cvtss2si the compiler emits.

extern "C" {
#include "vm_local.h"
}

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#if defined( _M_IX86 ) || defined( _M_X64 ) || defined( __SSE__ )
#include <xmmintrin.h>
#define STVEF_HAVE_SSE_CONVERSION 1
#endif

#define CHECK( expression ) do { if ( !( expression ) ) return __LINE__; } while ( false )

int main() {
	CHECK( VM_RoundFloatToInt( 0.5f ) == 0 );
	CHECK( VM_RoundFloatToInt( 1.5f ) == 2 );
	CHECK( VM_RoundFloatToInt( 2.5f ) == 2 );
	CHECK( VM_RoundFloatToInt( 2.4999998f ) == 2 );
	CHECK( VM_RoundFloatToInt( 2.5000002f ) == 3 );
	CHECK( VM_RoundFloatToInt( -0.5f ) == 0 );
	CHECK( VM_RoundFloatToInt( -1.5f ) == -2 );
	CHECK( VM_RoundFloatToInt( -2.5f ) == -2 );
	CHECK( VM_RoundFloatToInt( -2.6f ) == -3 );
	CHECK( VM_RoundFloatToInt( 8388607.5f ) == 8388608 );
	CHECK( VM_RoundFloatToInt( 2147483520.0f ) == 2147483520 );
	CHECK( VM_RoundFloatToInt( -2147483648.0f ) == INT32_MIN );
	CHECK( VM_RoundFloatToInt( 2147483648.0f ) == INT32_MIN );
	CHECK( VM_RoundFloatToInt( std::numeric_limits<float>::infinity() ) == INT32_MIN );
	CHECK( VM_RoundFloatToInt( std::numeric_limits<float>::quiet_NaN() ) == INT32_MIN );

#ifdef STVEF_HAVE_SSE_CONVERSION
	// Every exponent and many mantissas, both signs, against the instruction.
	std::uint32_t state = 0x2545f491u;
	for ( int i = 0; i < 2000000; ++i ) {
		state = state * 1664525u + 1013904223u;
		std::uint32_t bits = state;
		if ( ( i & 3 ) == 0 ) bits = ( bits & 0x807fffffu ) | ( ( 118u + ( i >> 2 ) % 40u ) << 23 );
		float value;
		std::memcpy( &value, &bits, sizeof( value ) );
		CHECK( VM_RoundFloatToInt( value ) == _mm_cvtss_si32( _mm_set_ss( value ) ) );
	}
#endif
	return 0;
}
