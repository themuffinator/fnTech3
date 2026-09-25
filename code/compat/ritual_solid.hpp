/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_RITUAL_SOLID_HPP
#define FNTECH3_COMPAT_RITUAL_SOLID_HPP

/* The `solid` word of a Ritual entity's state, as the server's link writes it
 * and the client game reads it back to clip against the entity.
 *
 * Retail alice.exe's link (0x00457a78..0x00457bfc) writes, by solid type:
 *   - a trigger (1): zero;
 *   - any type but the three named here: the type itself (0x00457ace), which
 *     for nothing solid (0) is zero;
 *   - a bounding box (2): zero when its contents are exactly 0, 0x100000 or
 *     0x4000000, else the box packed by 0x0044a000 below;
 *   - a brush model (3): 0xffffff, unless its contents are not -1 and hold
 *     one of the bits 0x4100000, when it is zero.
 *
 * The packing is Ritual's own, not Quake III's: nine bits of the high x bound,
 * eight of the high y bound, five of the low z bound plus 16, and nine of the
 * high z bound, each truncated as retail's ftol does (0x004e6d40) and clamped
 * into its field.
 *
 * Read from alice.exe; F.A.K.K.2's executable is encrypted, and its link is
 * taken to be the same [I]. No implementation text is incorporated. */

namespace fntech3 {
namespace ritual {

enum SolidType { SolidNot = 0, SolidTrigger = 1, SolidBoundingBox = 2, SolidBsp = 3 };

/* The whole-brush-model marker. */
inline constexpr int SolidBrushModel = 0xffffff;

inline int ClampField( float value, int limit ) {
	const int truncated = static_cast<int>( value );
	if ( truncated < 0 ) return 0;
	return truncated > limit ? limit : truncated;
}

/* BoundingBoxToInteger (0x0044a000). */
inline int PackBoundingBox( const float mins[3], const float maxs[3] ) {
	const int x = ClampField( maxs[0], 0x1ff );
	const int y = ClampField( maxs[1], 0xff );
	const int zd = ClampField( mins[2] + 16.0f, 0x1f );
	const int zu = ClampField( maxs[2], 0x1ff );
	return ( ( ( ( ( zu << 5 ) | zd ) << 8 ) | y ) << 9 ) | x;
}

inline int StateSolid( int solidType, int contents, const float mins[3], const float maxs[3] ) {
	if ( solidType == SolidTrigger ) return 0;
	if ( solidType == SolidBoundingBox ) {
		if ( contents == 0 || contents == 0x100000 || contents == 0x4000000 ) return 0;
		return PackBoundingBox( mins, maxs );
	}
	if ( solidType == SolidBsp ) {
		return contents == -1 || !( contents & 0x4100000 ) ? SolidBrushModel : 0;
	}
	return solidType;
}

} // namespace ritual
} // namespace fntech3

#endif
