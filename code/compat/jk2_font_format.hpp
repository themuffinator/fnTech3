/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_FONT_FORMAT_HPP
#define FNTECH3_JK2_FONT_FORMAT_HPP
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

/* Format observation: retail JK2 assets0.pk3 has three 7180-byte fontdat
 * records. OpenJK qfiles.h at1a6a643427aa347553e9073dac5570b33337c4d9 names
 * the slots. This bounded reader is independently written; no implementation
 * text from OpenJK or a retail renderer is incorporated. */
namespace fntech3::jk2::font {
inline constexpr std::size_t FileBytes = 7180, GlyphBytes = 28, GlyphCount = 256;
inline bool Finite(float value) noexcept {
	std::uint32_t bits; std::memcpy(&bits, &value, sizeof(bits));
	/* The host builds with fast math. An observable integer load keeps its
	 * finite-input assumption from erasing validation of external bytes. */
	volatile std::uint32_t encoded = bits;
	return (encoded & 0x7f800000u) != 0x7f800000u;
}
struct Glyph {
	std::int16_t width = 0, height = 0, advance = 0, offset = 0;
	std::int32_t baseline = 0;
	float s = 0, t = 0, s2 = 0, t2 = 0;
};
struct Font {
	std::array<Glyph, GlyphCount> glyphs{};
	std::int16_t pointSize = 0, height = 0, ascender = 0, descender = 0;
};
inline std::uint32_t Unsigned( const std::uint8_t *bytes, unsigned width ) noexcept {
	std::uint32_t value = 0;
	for ( unsigned i = 0; i < width; ++i ) value |= std::uint32_t( bytes[i] ) << ( i * 8 );
	return value;
}
inline std::int16_t Short( const std::uint8_t *bytes ) noexcept {
	const auto value = static_cast<std::uint16_t>( Unsigned( bytes, 2 ) );
	std::int16_t result;
	std::memcpy( &result, &value, sizeof( result ) );
	return result;
}
inline bool Read( const void *data, std::size_t length, Font &destination ) noexcept {
	if ( !data || length != FileBytes ) return false;
	const auto *bytes = static_cast<const std::uint8_t *>( data );
	Font out;
	for ( std::size_t i = 0; i < GlyphCount; ++i ) {
		const auto *record = bytes + i * GlyphBytes;
		auto &glyph = out.glyphs[i];
		glyph.width = Short( record ); glyph.height = Short( record + 2 );
		glyph.advance = Short( record + 4 ); glyph.offset = Short( record + 6 );
		const auto baseline = Unsigned( record + 8, 4 );
		std::memcpy( &glyph.baseline, &baseline, sizeof( baseline ) );
		float *coordinates[] = { &glyph.s, &glyph.t, &glyph.s2, &glyph.t2 };
		for ( unsigned j = 0; j < 4; ++j ) {
			const auto bits = Unsigned( record + 12 + j * 4, 4 );
			std::memcpy( coordinates[j], &bits, sizeof( bits ) );
			if ( !Finite( *coordinates[j] ) || *coordinates[j] < 0 || *coordinates[j] > 1 ) return false;
		}
		if ( glyph.width < 0 || glyph.width > 4096 || glyph.height < 0 || glyph.height > 4096 ||
			glyph.advance < 0 || glyph.advance > 4096 || glyph.offset < -4096 || glyph.offset > 4096 ||
			glyph.baseline < -4096 || glyph.baseline > 4096 || glyph.s2 < glyph.s || glyph.t2 < glyph.t ) return false;
	}
	out.pointSize = Short( bytes + 7168 ); out.height = Short( bytes + 7170 );
	out.ascender = Short( bytes + 7172 ); out.descender = Short( bytes + 7174 );
	/* JKA's retail arialnb.fontdat has height/ascender/descender zero. The
	 * shared file schema permits that header; the title's layout adapter
	 * supplies its fallback metrics after decoding. */
	if ( out.pointSize <= 0 || out.pointSize > 4096 || out.height < 0 || out.height > 4096 ||
		out.ascender < 0 || out.ascender > 4096 || out.descender < 0 || out.descender > 4096 ) return false;
	destination = out;
	return true;
}
inline bool ColorEscape( const char *text ) noexcept { return text[0] == '^' && text[1] >= '0' && text[1] <= '9'; }
inline int Characters( const char *text ) noexcept {
	if ( !text ) return 0;
	int count = 0;
	for ( ; *text; ++text ) {
		if ( ColorEscape( text ) ) { ++text; continue; }
		if ( *text != '\n' && *text != '\r' ) ++count;
	}
	return count;
}
inline int Width( const Font &font, const char *text, float scale ) noexcept {
	if ( !text || !Finite( scale ) || scale < 0 ) return 0;
	double total = 0;
	for ( ; *text; ++text ) {
		if ( ColorEscape( text ) ) { ++text; continue; }
		total += font.glyphs[static_cast<unsigned char>( *text )].advance * double( scale );
	}
	if ( total >= std::numeric_limits<int>::max() ) return std::numeric_limits<int>::max();
	return static_cast<int>( total );
}
} // namespace fntech3::jk2::font
#endif
