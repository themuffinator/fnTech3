/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_DRAW_ADAPTER_HPP
#define FNTECH3_JKA_DRAW_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include <cmath>

/* Jedi Academy's turned pictures (cgame imports 214 and 215), in the module's
 * virtual 640x480 screen. Retail jamp.exe 1.01 queues both as 44-byte render
 * commands (RE_RotatePic 0x491760 id 3, RE_RotatePic2 0x491810 id 4) and its
 * backend draws them in that screen: RB_RotatePic 0x48b7c0 translates to the
 * picture's top-right corner (x + w, y), RB_RotatePic2 0x48b8b0 to its centre
 * (x, y); both then turn by the angle in degrees (glRotatef about +z, which is
 * clockwise in the y-down screen) and draw the four corners below. */
namespace fntech3::jka::draw {

enum class Pivot { Corner, Centre };
enum class Status { Ok, Empty, Invalid };

struct Rect {
	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

/* The rectangle the picture covers unturned, which the HUD rules place, and
 * its corners turned: top-left, top-right, bottom-right, bottom-left of the
 * unturned picture. A picture without area draws nothing. */
inline Status RotatedQuad( Pivot pivot, float x, float y, float w, float h, float s1, float t1,
	float s2, float t2, float degrees, qhandle_t shader, refDrawQuad_t &quad, Rect &unturned ) noexcept {
	const float values[] = { x, y, w, h, s1, t1, s2, t2, degrees };
	for ( float value : values ) {
		if ( !std::isfinite( value ) ) return Status::Invalid;
	}
	const double corner[4][2] = { { -w, 0 }, { 0, 0 }, { 0, h }, { -w, h } };
	const double centre[4][2] = { { -0.5 * w, -0.5 * h }, { 0.5 * w, -0.5 * h },
		{ 0.5 * w, 0.5 * h }, { -0.5 * w, 0.5 * h } };
	const auto &local = pivot == Pivot::Corner ? corner : centre;
	const double originX = pivot == Pivot::Corner ? static_cast<double>( x ) + w : x;
	const double radians = static_cast<double>( degrees ) * M_PI / 180.0;
	const double c = std::cos( radians ), s = std::sin( radians );
	const float st[4][2] = { { s1, t1 }, { s2, t1 }, { s2, t2 }, { s1, t2 } };
	refDrawQuad_t result{};
	for ( int i = 0; i < 4; ++i ) {
		result.xy[i][0] = static_cast<float>( originX + local[i][0] * c - local[i][1] * s );
		result.xy[i][1] = static_cast<float>( y + local[i][0] * s + local[i][1] * c );
		result.st[i][0] = st[i][0];
		result.st[i][1] = st[i][1];
		if ( !std::isfinite( result.xy[i][0] ) || !std::isfinite( result.xy[i][1] ) ) return Status::Invalid;
	}
	result.shader = shader;
	Rect area;
	area.x = pivot == Pivot::Corner ? x : static_cast<float>( x - 0.5 * w );
	area.y = pivot == Pivot::Corner ? y : static_cast<float>( y - 0.5 * h );
	area.w = w;
	area.h = h;
	if ( !std::isfinite( area.x ) || !std::isfinite( area.y ) ) return Status::Invalid;
	if ( w == 0.0f || h == 0.0f ) return Status::Empty;
	quad = result;
	unturned = area;
	return Status::Ok;
}

/* Carries a quad from the module's screen to wherever the HUD placed its
 * unturned rectangle: the same axis-aligned scale and offset as that
 * rectangle took. A stretched picture is therefore turned before it is
 * stretched, as retail's 640x480 screen was, and stays registered with the
 * stretched mask it belongs to; a uniformly placed one simply turns. */
inline bool PlaceQuad( refDrawQuad_t &quad, const Rect &from, const Rect &to ) noexcept {
	const float values[] = { from.x, from.y, from.w, from.h, to.x, to.y, to.w, to.h };
	for ( float value : values ) {
		if ( !std::isfinite( value ) ) return false;
	}
	if ( from.w == 0.0f || from.h == 0.0f ) return false;
	const double scaleX = static_cast<double>( to.w ) / from.w;
	const double scaleY = static_cast<double>( to.h ) / from.h;
	refDrawQuad_t placed = quad;
	for ( int i = 0; i < 4; ++i ) {
		placed.xy[i][0] = static_cast<float>( to.x + ( quad.xy[i][0] - static_cast<double>( from.x ) ) * scaleX );
		placed.xy[i][1] = static_cast<float>( to.y + ( quad.xy[i][1] - static_cast<double>( from.y ) ) * scaleY );
		if ( !std::isfinite( placed.xy[i][0] ) || !std::isfinite( placed.xy[i][1] ) ) return false;
	}
	quad = placed;
	return true;
}

} // namespace fntech3::jka::draw
#endif
