/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_GAME_IMPORTS_HPP
#define FNTECH3_STVEF_GAME_IMPORTS_HPP

#include "../qcommon/q_shared.h"

#include <cmath>
#include <cstdint>

/* Holomatch game imports that keep Quake III 1.32's contract, and the one
 * shared math routine whose Elite Force result differs. */
namespace fntech3::stvef::game {

// Imports 0-40, the math traps 100-111, 200-211, 303-318 and 500-572 keep
// Quake III 1.32's numbers and argument lists. 300-302 and 400-427 do not.
constexpr bool SharesQuake3Import( std::intptr_t ordinal ) noexcept {
	return ( ordinal >= 0 && ordinal <= 40 ) || ( ordinal >= 100 && ordinal <= 111 ) ||
		( ordinal >= 200 && ordinal <= 211 ) || ( ordinal >= 303 && ordinal <= 318 ) ||
		( ordinal >= 500 && ordinal <= 572 );
}

// Elite Force scans the axes from z down to x, so ties pick a different axis
// than Quake III's scan (retail game trap 109 at 0x4263e0).
inline void PerpendicularVector( float *destination, const float *source ) {
	int axis = 0;
	float smallest = 1.0f;
	for ( int i = 2; i >= 0; --i ) {
		if ( std::fabs( source[i] ) < smallest ) {
			axis = i;
			smallest = std::fabs( source[i] );
		}
	}
	vec3_t unit{};
	unit[axis] = 1.0f;
	ProjectPointOnPlane( destination, unit, source );
	VectorNormalize( destination );
}

} // namespace fntech3::stvef::game
#endif
