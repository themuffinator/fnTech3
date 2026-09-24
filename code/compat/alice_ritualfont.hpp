/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_RITUALFONT_HPP
#define FNTECH3_COMPAT_ALICE_RITUALFONT_HPP

/* American McGee's Alice's bitmap fonts: the `fonts/<name>.RitualFont` text
 * files its engine reads, their metrics, and the quads a string is laid out
 * into. Pure: no engine, renderer or filesystem is touched, so the parser can
 * be tested against authored fixtures and the retail files alike.
 *
 * Every rule here was read from retail alice.exe (image base 0x400000):
 *
 *   R_LoadFont        0x00498160  the grammar below, the 255-font cache and
 *                                 the "fonts/%s.RitualFont" path
 *   R_DrawString      0x00498660  glyph quads, tab, newline, carriage return,
 *                                 angle, character and line spacing, maxlen
 *   R_GetFontHeight   0x00498ad0  height * g1 * g2 (both globals are 1.0 at
 *                                 0x00528714/0x00528718 and never written by
 *                                 referenced code)
 *   UIFont::width     0x004c5700  the widest line, truncated
 *   UIFont::height    0x004c57e0  the truncated height, once per line
 *   UIFont::Print     0x004c54e0  the 640x480 rescale of position and scale
 *   UIFont::PrintJustified
 *                     0x004c55a0  horizontal and vertical justification
 *
 * The grammar (R_LoadFont): tokens as COM_Parse reads them. `RitFont` is
 * skipped; `height <f>` and `aspect <f>` store floats; `indirections {` takes
 * exactly 256 integers and a closing `}`; `locations {` takes exactly 256
 * `{ x y w h }` groups in image pixels and a closing `}`, and needs the aspect
 * already declared. Any other token is refused, as is a file that ends with a
 * zero height or aspect. Texture coordinates are stored normalized:
 * s = x/256, t = y*aspect/256, width w/256, height h*aspect/256 -- the atlas
 * is 256 texels wide and 256/aspect tall. */

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

namespace fntech3 {
namespace alice {
namespace ritualfont {

inline constexpr int GlyphCount = 256;
/* R_LoadFont refuses a 256th font (`cmp eax, 0xff` at 0x004981c2). */
inline constexpr int MaxFonts = 255;
/* UIFont::height answers 16 for a font that did not load (0x004c58d7). */
inline constexpr int MissingFontHeight = 16;

struct Glyph {
	float s = 0.0f, t = 0.0f;          /* top-left texture coordinate */
	float width = 0.0f, height = 0.0f; /* normalized extent */
};

struct Font {
	float height = 0.0f;
	float aspect = 0.0f;
	int indirection[GlyphCount];
	Glyph locations[GlyphCount];

	Font() {
		for ( int &value : indirection ) value = -1;
	}

	/* The glyph a byte draws with, or nullptr for a byte the font maps to
	 * nothing or out of range. */
	const Glyph *GlyphFor( unsigned char c ) const {
		const int index = indirection[c];
		return index >= 0 && index < GlyphCount ? &locations[index] : nullptr;
	}
};

namespace detail {

/* COM_Parse's tokens: whitespace is every byte at or below a space, `//` and
 * block comments are skipped, and a quoted token ends at its closing quote. */
class Tokenizer {
public:
	Tokenizer( const char *text, std::size_t length ) : cursor_( text ), end_( text + length ) {}

	/* The next token, or an empty string at the end of the text. */
	std::string Next() {
		for ( ;; ) {
			while ( cursor_ < end_ && static_cast<unsigned char>( *cursor_ ) <= ' ' ) ++cursor_;
			if ( cursor_ + 1 < end_ && cursor_[0] == '/' && cursor_[1] == '/' ) {
				while ( cursor_ < end_ && *cursor_ != '\n' ) ++cursor_;
				continue;
			}
			if ( cursor_ + 1 < end_ && cursor_[0] == '/' && cursor_[1] == '*' ) {
				cursor_ += 2;
				while ( cursor_ + 1 < end_ && !( cursor_[0] == '*' && cursor_[1] == '/' ) ) ++cursor_;
				cursor_ = cursor_ + 1 < end_ ? cursor_ + 2 : end_;
				continue;
			}
			break;
		}
		std::string token;
		if ( cursor_ >= end_ ) return token;
		if ( *cursor_ == '"' ) {
			++cursor_;
			while ( cursor_ < end_ && *cursor_ != '"' ) token += *cursor_++;
			if ( cursor_ < end_ ) ++cursor_;
			return token;
		}
		while ( cursor_ < end_ && static_cast<unsigned char>( *cursor_ ) > ' ' ) token += *cursor_++;
		return token;
	}

private:
	const char *cursor_;
	const char *end_;
};

inline bool EqualsNoCase( const std::string &a, const char *b ) {
	const std::size_t length = std::strlen( b );
	if ( a.size() != length ) return false;
	for ( std::size_t i = 0; i < length; ++i ) {
		char x = a[i], y = b[i];
		if ( x >= 'A' && x <= 'Z' ) x = static_cast<char>( x - 'A' + 'a' );
		if ( y >= 'A' && y <= 'Z' ) y = static_cast<char>( y - 'A' + 'a' );
		if ( x != y ) return false;
	}
	return true;
}

/* The retail loader converts with the C runtime's atoi and atof (0x004e6ff0,
 * 0x004e6f0e), which read a prefix and answer zero for none. */
inline int ToInt( const std::string &token ) { return std::atoi( token.c_str() ); }
inline float ToFloat( const std::string &token ) { return static_cast<float>( std::atof( token.c_str() ) ); }

} // namespace detail

/* Parses a font file. On failure `error` names the reason and the font must
 * not be used: retail answers NULL (0x0049855a) and keeps nothing. */
inline bool Parse( const char *text, std::size_t length, Font &font, std::string *error = nullptr ) {
	auto fail = [error]( const std::string &reason ) {
		if ( error ) *error = reason;
		return false;
	};
	font = Font();
	if ( !text ) return fail( "no text" );
	detail::Tokenizer tokens( text, length );
	for ( ;; ) {
		const std::string token = tokens.Next();
		if ( token.empty() ) break;
		if ( detail::EqualsNoCase( token, "RitFont" ) ) continue;
		if ( detail::EqualsNoCase( token, "indirections" ) ) {
			if ( tokens.Next() != "{" ) return fail( "indirections without '{'" );
			for ( int i = 0; i < GlyphCount; ++i ) {
				const std::string value = tokens.Next();
				if ( value.empty() ) return fail( "indirections end early" );
				font.indirection[i] = detail::ToInt( value );
			}
			if ( tokens.Next() != "}" ) return fail( "indirections without '}'" );
			continue;
		}
		if ( detail::EqualsNoCase( token, "locations" ) ) {
			if ( tokens.Next() != "{" ) return fail( "locations without '{'" );
			for ( int i = 0; i < GlyphCount; ++i ) {
				if ( tokens.Next() != "{" ) return fail( "location without '{'" );
				if ( font.aspect == 0.0f ) return fail( "aspect decl must be before locations" );
				Glyph &glyph = font.locations[i];
				glyph.s = detail::ToFloat( tokens.Next() ) * ( 1.0f / 256.0f );
				glyph.t = detail::ToFloat( tokens.Next() ) * font.aspect * ( 1.0f / 256.0f );
				glyph.width = detail::ToFloat( tokens.Next() ) * ( 1.0f / 256.0f );
				glyph.height = detail::ToFloat( tokens.Next() ) * font.aspect * ( 1.0f / 256.0f );
				if ( tokens.Next() != "}" ) return fail( "location without '}'" );
			}
			if ( tokens.Next() != "}" ) return fail( "locations without '}'" );
			continue;
		}
		if ( detail::EqualsNoCase( token, "height" ) ) {
			font.height = detail::ToFloat( tokens.Next() );
			continue;
		}
		if ( detail::EqualsNoCase( token, "aspect" ) ) {
			font.aspect = detail::ToFloat( tokens.Next() );
			continue;
		}
		return fail( "unknown token '" + token + "'" );
	}
	if ( font.height == 0.0f || font.aspect == 0.0f ) return fail( "zero height or aspect" );
	return true;
}

/* ---- metrics ------------------------------------------------------------- */

/* UIFont::height's per-line height: the font height truncated, or 16. */
inline int LineHeight( const Font *font ) {
	return font ? static_cast<int>( font->height ) : MissingFontHeight;
}

/* The width in unscaled pixels of the widest line (UIFont::width): tab is three
 * spaces, a byte outside 0..255 of the table adds nothing, character spacing is
 * not counted, and the sum is truncated. */
inline int StringWidth( const Font *font, const char *text, int maxLength = -1 ) {
	if ( !font || !text ) return 0;
	float line = 0.0f, widest = 0.0f;
	for ( int i = 0; text[i] && ( maxLength == -1 || i < maxLength ); ++i ) {
		const unsigned char c = static_cast<unsigned char>( text[i] );
		if ( c == '\n' ) {
			line = 0.0f;
			continue;
		}
		const int index = c == '\t' ? font->indirection[' '] : font->indirection[c];
		if ( index < 0 || index >= GlyphCount ) continue;
		line += font->locations[index].width * ( c == '\t' ? 3.0f : 1.0f );
		if ( line > widest ) widest = line;
	}
	return static_cast<int>( widest * 256.0f );
}

/* One character's width in unscaled pixels (UIFont::charWidth 0x004c5860):
 * a tab is three spaces, and a character the font maps to nothing is zero
 * wide. Retail indexes the table with a signed char, so a byte above 127
 * reads before it; here the byte is taken as unsigned. */
inline int CharWidth( const Font *font, char c ) {
	if ( !font ) return 0;
	const unsigned char byte = static_cast<unsigned char>( c );
	const int index = byte == '\t' ? font->indirection[' '] : font->indirection[byte];
	if ( index < 0 || index >= GlyphCount ) return 0;
	return static_cast<int>( font->locations[index].width * ( byte == '\t' ? 768.0f : 256.0f ) );
}

/* The height in unscaled pixels of every line (UIFont::height). */
inline int StringHeight( const Font *font, const char *text, int maxLength = -1 ) {
	if ( !font ) return 0;
	int lines = 1;
	for ( int i = 0; text && text[i] && ( maxLength == -1 || i < maxLength ); ++i ) {
		if ( text[i] == '\n' ) ++lines;
	}
	return LineHeight( font ) * lines;
}

/* ---- layout -------------------------------------------------------------- */

struct Quad {
	float x[4], y[4];   /* clockwise from the top left */
	float s0, t0, s1, t1;
};

/* Lays a string out as R_DrawString does and hands each glyph's quad to
 * `emit`. Positions and scales are in the caller's pixels. A newline moves down
 * a scaled font height plus the line spacing; a tab advances three unscaled
 * space widths and ignores the scale and spacing (0x00498a87); a byte with no
 * glyph advances nothing. A nonzero angle turns every corner about the string's
 * origin by that many degrees around the axis (0, 0, -1) (0x0049891a); on a
 * screen whose y grows downward that turns text counter-clockwise. */
template<typename Emit>
inline void Layout( const Font &font, const char *text, float x, float y, float angle,
	float charSpacing, float lineSpacing, float wScale, float hScale, int maxLength, Emit &&emit ) {
	if ( !text ) return;
	const float glyphHeight = font.height * hScale;
	const float radians = angle * 3.14159265358979323846f / 180.0f;
	const float c = std::cos( radians ), s = std::sin( radians );
	float penX = x, penY = y;
	for ( int i = 0; text[i] && ( maxLength == -1 || i < maxLength ); ++i ) {
		const unsigned char ch = static_cast<unsigned char>( text[i] );
		if ( ch == '\t' ) {
			const int space = font.indirection[' '];
			if ( space != -1 && space >= 0 && space < GlyphCount ) {
				penX += font.locations[space].width * 256.0f * 3.0f;
			}
			continue;
		}
		if ( ch == '\n' ) {
			penY += glyphHeight + lineSpacing;
			penX = x;
			continue;
		}
		if ( ch == '\r' ) {
			penX = x;
			continue;
		}
		const int index = font.indirection[ch];
		if ( index < 0 || index >= GlyphCount ) continue;
		const Glyph &glyph = font.locations[index];
		const float glyphWidth = glyph.width * 256.0f * wScale;
		Quad quad;
		quad.x[0] = penX;              quad.y[0] = penY;
		quad.x[1] = penX + glyphWidth; quad.y[1] = penY;
		quad.x[2] = penX + glyphWidth; quad.y[2] = penY + glyphHeight;
		quad.x[3] = penX;              quad.y[3] = penY + glyphHeight;
		quad.s0 = glyph.s;
		quad.t0 = glyph.t;
		quad.s1 = glyph.s + glyph.width;
		quad.t1 = glyph.t + glyph.height;
		if ( angle != 0.0f ) {
			for ( int v = 0; v < 4; ++v ) {
				const float dx = quad.x[v] - x, dy = quad.y[v] - y;
				quad.x[v] = x + dx * c + dy * s;
				quad.y[v] = y - dx * s + dy * c;
			}
		}
		emit( quad );
		penX += glyphWidth + charSpacing;
	}
}

/* ---- justification (UIFont::PrintJustified) ------------------------------ */

enum class Horizontal { Center = 0, Left = 1, Right = 2 };
enum class Vertical { Top = 0, Center = 1, Bottom = 2 };

struct Rect {
	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

/* Where a justified string starts. The measured width and height are the
 * truncations of width * wScale + charSpacing and height * hScale +
 * lineSpacing (0x004c560a, 0x004c562d); left-and-top skips measuring. A value
 * outside the three answers zero for that axis, as the retail switch does. */
inline void Justify( const Font *font, const Rect &rect, int horizontal, int vertical, const char *text,
	float charSpacing, float lineSpacing, float wScale, float hScale, float &x, float &y ) {
	int textWidth = 0;
	if ( horizontal != static_cast<int>( Horizontal::Left ) || vertical != static_cast<int>( Vertical::Top ) ) {
		if ( horizontal != static_cast<int>( Horizontal::Left ) ) {
			textWidth = static_cast<int>( StringWidth( font, text ) * wScale + charSpacing );
		}
	}
	int textHeight = 0;
	if ( vertical != static_cast<int>( Vertical::Top ) || horizontal != static_cast<int>( Horizontal::Left ) ) {
		textHeight = static_cast<int>( StringHeight( font, text ) * hScale + lineSpacing );
	}
	switch ( horizontal ) {
	case 0: x = rect.x + ( rect.w - textWidth ) * 0.5f; break;
	case 1: x = rect.x; break;
	case 2: x = rect.x + rect.w - textWidth; break;
	default: x = 0.0f; break;
	}
	switch ( vertical ) {
	case 0: y = rect.y; break;
	case 1: y = rect.y + ( rect.h - textHeight ) * 0.5f; break;
	case 2: y = rect.y + rect.h - textHeight; break;
	default: y = 0.0f; break;
	}
}

} // namespace ritualfont
} // namespace alice
} // namespace fntech3

#endif
