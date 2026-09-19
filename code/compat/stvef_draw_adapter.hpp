/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_DRAW_ADAPTER_HPP
#define FNTECH3_STVEF_DRAW_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include <cmath>

namespace fntech3::stvef::render {

/* Elite Force SP's rotated picture: retail's backend translates to the
 * picture's top-right corner (x + w, y), rotates by the angle in degrees and
 * draws the rectangle to the left of that corner (stvoy.exe RB_RotatePic
 * 0x450fc0). Corners follow the unrotated picture's order: top-left,
 * top-right, bottom-right, bottom-left. */
inline bool RotatedQuad( float x, float y, float w, float h, float s1, float t1,
	float s2, float t2, float degrees, qhandle_t shader, refDrawQuad_t &destination ) noexcept {
	const float values[] = { x, y, w, h, s1, t1, s2, t2, degrees };
	for ( float value : values ) {
		if ( !std::isfinite( value ) ) return false;
	}
	const double radians = static_cast<double>( degrees ) * M_PI / 180.0;
	const double c = std::cos( radians ), s = std::sin( radians );
	const double local[4][2] = { { -w, 0 }, { 0, 0 }, { 0, h }, { -w, h } };
	const float st[4][2] = { { s1, t1 }, { s2, t1 }, { s2, t2 }, { s1, t2 } };
	refDrawQuad_t result{};
	for ( int i = 0; i < 4; ++i ) {
		result.xy[i][0] = static_cast<float>( x + w + local[i][0] * c - local[i][1] * s );
		result.xy[i][1] = static_cast<float>( y + local[i][0] * s + local[i][1] * c );
		result.st[i][0] = st[i][0];
		result.st[i][1] = st[i][1];
		if ( !std::isfinite( result.xy[i][0] ) || !std::isfinite( result.xy[i][1] ) ) return false;
	}
	result.shader = shader;
	destination = result;
	return true;
}

} // namespace fntech3::stvef::render
#endif
