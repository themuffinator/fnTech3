/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_DRAW_ADAPTER_HPP
#define FNTECH3_RTCW_DRAW_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fntech3::rtcw::render {

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
	(void)gradientType; // The original SP/MP backend always uses a vertical gradient.
	auto result = Rectangle( x, y, w, h, s1, t1, s2, t2, shader );
	if ( !FiniteQuad( result ) ) return false;
	result.colorMask = 12;
	for ( int i = 0; i < 4; ++i ) {
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
	const float radius = static_cast<float>( std::sqrt( static_cast<double>( halfWidth * halfWidth + halfHeight * halfHeight ) ) );
	const float twoPi = static_cast<float>( 2.0 * M_PI );
	// RTCW MP places four corners on a circle of half-diagonal radius, even
	// for a nonsquare input rectangle. Its angle is in turns, not degrees.
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

} // namespace fntech3::rtcw::render
#endif
