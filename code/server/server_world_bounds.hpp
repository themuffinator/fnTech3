/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_WORLD_BOUNDS_HPP
#define FNTECH3_SERVER_WORLD_BOUNDS_HPP
#include <cstdint>
#include <cstring>

inline bool SV_FiniteWorldVector( const float *value ) {
	if ( !value ) return false;
	for ( int axis = 0; axis < 3; ++axis ) {
		std::uint32_t bits;
		std::memcpy( &bits, value + axis, sizeof( bits ) );
		if ( ( bits & 0x7f800000u ) == 0x7f800000u ) return false;
	}
	return true;
}

inline bool SV_ValidWorldBounds( const float *mins, const float *maxs ) {
	if ( !SV_FiniteWorldVector( mins ) || !SV_FiniteWorldVector( maxs ) ) return false;
	for ( int axis = 0; axis < 3; ++axis ) if ( mins[axis] > maxs[axis] ) return false;
	return true;
}
#endif
