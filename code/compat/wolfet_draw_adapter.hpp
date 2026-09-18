/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_DRAW_ADAPTER_HPP
#define FNTECH3_WOLFET_DRAW_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// Enemy Territory's two extra 2D submissions, recorded from its own renderer.
// The gradient names the colour of the lower two corners only, and the rotated
// picture places four corners on a circle whose radius is the half-diagonal of
// the requested rectangle, with the angle given in turns. ET is kept apart from
// the other Wolfenstein profile deliberately: these are its own observations and
// must stay free to diverge from RTCW's.
namespace fntech3::wolfet::render {

inline bool FiniteQuad( const refDrawQuad_t &quad ) noexcept {
	for ( int i = 0; i < 4; ++i )
		for ( int j = 0; j < 2; ++j )
			if ( !std::isfinite( quad.xy[i][j] ) || !std::isfinite( quad.st[i][j] ) ) return false;
	return true;
}

inline refDrawQuad_t Rectangle( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t shader ) noexcept {
	refDrawQuad_t result{};
	const vec2_t xy[] = { {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h} };
	const vec2_t st[] = { {s1, t1}, {s2, t1}, {s2, t2}, {s1, t2} };
	std::memcpy( result.xy, xy, sizeof( xy ) );
	std::memcpy( result.st, st, sizeof( st ) );
	result.shader = shader;
	return result;
}

inline bool GradientQuad( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t shader,
	const float *gradientColor, int gradientType, refDrawQuad_t &destination ) noexcept {
	// ET records a gradient type in its command but its backend never reads it;
	// the only gradient it draws runs from the top two corners to the bottom two.
	(void)gradientType;
	auto result = Rectangle( x, y, w, h, s1, t1, s2, t2, shader );
	if ( !FiniteQuad( result ) ) return false;
	result.colorMask = 12;
	for ( int i = 0; i < 4; ++i ) {
		// A null colour is white in the original, which scales each component
		// by 255 without clamping; a module value outside 0..1 is refused here
		// rather than wrapped into an unrelated byte.
		const float value = gradientColor ? gradientColor[i] : 1.0f;
		if ( !std::isfinite( value ) ) return false;
		const auto component = static_cast<byte>( std::clamp( value, 0.0f, 1.0f ) * 255.0f );
		result.colors[2].rgba[i] = result.colors[3].rgba[i] = component;
	}
	destination = result;
	return true;
}

inline bool RotatedQuad( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t shader,
	float turns, refDrawQuad_t &destination ) noexcept {
	auto result = Rectangle( x, y, w, h, s1, t1, s2, t2, shader );
	if ( !FiniteQuad( result ) || !std::isfinite( turns ) ) return false;
	const float halfWidth = w / 2, halfHeight = h / 2;
	const float centerX = x + halfWidth, centerY = y + halfHeight;
	const float radius = static_cast<float>( std::sqrt(
		static_cast<double>( halfWidth * halfWidth + halfHeight * halfHeight ) ) );
	const float twoPi = static_cast<float>( 2.0 * M_PI );
	for ( int i = 0; i < 4; ++i ) {
		const float angle = static_cast<float>( turns * twoPi + ( i * 0.25 ) * twoPi );
		if ( !std::isfinite( angle ) ) return false;
		result.xy[i][0] = static_cast<float>( centerX + std::cos( static_cast<double>( angle ) ) * radius );
		result.xy[i][1] = static_cast<float>( centerY + std::sin( static_cast<double>( angle ) ) * radius );
	}
	if ( !FiniteQuad( result ) ) return false;
	destination = result;
	return true;
}

} // namespace fntech3::wolfet::render
#endif
