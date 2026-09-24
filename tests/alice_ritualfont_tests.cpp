/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's `.ritualfont` reader, metrics and string layout
 * (code/compat/alice_ritualfont.hpp). The fixtures are written here; the
 * fourteen fonts a retail installation ships are also read, read-only, when
 * one is present, and the test says so when it is not. */

#include "../code/compat/alice_ritualfont.hpp"
#include "alice_pak_reader.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::ritualfont;

namespace {

bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }

/* A font whose glyph n is at (10n, 2n) and 8 by 12 pixels, mapping 'A'..'D' to
 * glyphs 0..3, space to glyph 4 (6 wide) and newline-free text otherwise. */
std::string Fixture( float height = 12.0f, float aspect = 2.0f, bool aspectFirst = true,
	int indirectionCount = 256, int locationCount = 256 ) {
	std::ostringstream out;
	out << "RitFont\r\n";
	out << "// a comment\r\n";
	out << "height " << height << "\r\n";
	if ( aspectFirst ) out << "aspect " << aspect << "\r\n";
	out << "indirections {\r\n";
	for ( int i = 0; i < indirectionCount; ++i ) {
		int value = -1;
		if ( i >= 'A' && i <= 'D' ) value = i - 'A';
		if ( i == ' ' ) value = 4;
		out << value << ( i % 10 == 9 ? "\r\n" : " " );
	}
	out << "}\r\n";
	out << "locations {\r\n";
	for ( int i = 0; i < locationCount; ++i ) {
		const int w = i == 4 ? 6 : 8;
		out << "{ " << 10 * i << " " << 2 * i << " " << w << " 12 } ";
		if ( i % 3 == 2 ) out << "\r\n";
	}
	out << "}\r\n";
	// Declared after the locations it should have scaled.
	if ( !aspectFirst ) out << "aspect " << aspect << "\r\n";
	return out.str();
}

void TestParse() {
	const std::string text = Fixture();
	Font font;
	std::string error;
	CHECK( Parse( text.data(), text.size(), font, &error ) );
	CHECK( font.height == 12.0f );
	CHECK( font.aspect == 2.0f );
	CHECK( font.indirection['A'] == 0 && font.indirection['D'] == 3 && font.indirection['E'] == -1 );
	CHECK( font.indirection[' '] == 4 );
	// s = x/256, t = y*aspect/256, width w/256, height h*aspect/256.
	CHECK( Near( font.locations[3].s, 30.0f / 256.0f ) );
	CHECK( Near( font.locations[3].t, 6.0f * 2.0f / 256.0f ) );
	CHECK( Near( font.locations[3].width, 8.0f / 256.0f ) );
	CHECK( Near( font.locations[3].height, 12.0f * 2.0f / 256.0f ) );
	CHECK( font.GlyphFor( 'B' ) == &font.locations[1] );
	CHECK( font.GlyphFor( 'z' ) == nullptr );
}

void TestRefusals() {
	Font font;
	std::string error;
	// The aspect has to come before the locations it scales.
	std::string text = Fixture( 12.0f, 2.0f, false );
	CHECK( !Parse( text.data(), text.size(), font, &error ) );
	CHECK( error.find( "aspect" ) != std::string::npos );
	// Zero height or aspect.
	text = Fixture( 0.0f );
	CHECK( !Parse( text.data(), text.size(), font, &error ) );
	text = Fixture( 12.0f, 0.0f );
	CHECK( !Parse( text.data(), text.size(), font, &error ) );
	// Too few indirections: the next token is not a number but '}' is read as
	// one, and the block's end is then missing.
	text = Fixture( 12.0f, 2.0f, true, 255 );
	CHECK( !Parse( text.data(), text.size(), font, &error ) );
	// Too few locations.
	text = Fixture( 12.0f, 2.0f, true, 256, 255 );
	CHECK( !Parse( text.data(), text.size(), font, &error ) );
	// An unknown token.
	text = "height 10\naspect 1\nwidth 3\n";
	CHECK( !Parse( text.data(), text.size(), font, &error ) );
	CHECK( error.find( "width" ) != std::string::npos );
	// A block that ends early.
	text = "height 10 aspect 1 indirections { 1 2 3";
	CHECK( !Parse( text.data(), text.size(), font, &error ) );
	// Nothing at all.
	CHECK( !Parse( "", 0, font, &error ) );
	CHECK( !Parse( nullptr, 0, font, &error ) );
	// Keywords ignore case, as retail's compare does.
	text = Fixture();
	for ( const char *word : { "height", "aspect", "indirections", "locations" } ) {
		const std::size_t at = text.find( word );
		for ( std::size_t i = 0; i < std::strlen( word ); ++i ) {
			text[at + i] = static_cast<char>( text[at + i] - 'a' + 'A' );
		}
	}
	CHECK( Parse( text.data(), text.size(), font, &error ) );
}

void TestMetrics() {
	const std::string text = Fixture();
	Font font;
	CHECK( Parse( text.data(), text.size(), font ) );
	CHECK( LineHeight( &font ) == 12 );
	CHECK( LineHeight( nullptr ) == MissingFontHeight );
	// Widest line, no spacing; an unmapped byte adds nothing.
	CHECK( StringWidth( &font, "ABC" ) == 24 );
	CHECK( StringWidth( &font, "A xB" ) == 8 + 6 + 8 );
	CHECK( StringWidth( &font, "AB\nABCD" ) == 32 );
	CHECK( StringWidth( &font, "ABCD", 2 ) == 16 );
	// Tab is three spaces.
	CHECK( StringWidth( &font, "A\tB" ) == 8 + 18 + 8 );
	CHECK( StringHeight( &font, "A\nB\nC" ) == 36 );
	CHECK( CharWidth( &font, 'A' ) == 8 && CharWidth( &font, ' ' ) == 6 );
	CHECK( CharWidth( &font, '\t' ) == 18 && CharWidth( &font, 'z' ) == 0 );
	CHECK( CharWidth( nullptr, 'A' ) == 0 );
	CHECK( StringHeight( nullptr, "A" ) == 0 );
	CHECK( StringWidth( nullptr, "A" ) == 0 );
}

void TestLayout() {
	const std::string text = Fixture();
	Font font;
	CHECK( Parse( text.data(), text.size(), font ) );
	std::vector<Quad> quads;
	auto collect = [&quads]( const Quad &quad ) { quads.push_back( quad ); };

	// Character spacing after every glyph, scale on width and height.
	Layout( font, "AB", 100, 50, 0, 2, 0, 2, 3, -1, collect );
	CHECK( quads.size() == 2 );
	CHECK( Near( quads[0].x[0], 100 ) && Near( quads[0].y[0], 50 ) );
	CHECK( Near( quads[0].x[1], 116 ) && Near( quads[0].y[2], 50 + 36 ) );
	CHECK( Near( quads[1].x[0], 118 ) );
	CHECK( Near( quads[1].s0, 10.0f / 256.0f ) && Near( quads[1].s1, 18.0f / 256.0f ) );

	// Newline: down a scaled height plus the line spacing, back to the start.
	// Carriage return: back to the start. Tab: three unscaled spaces.
	quads.clear();
	Layout( font, "A\nB\rC\tD", 0, 0, 0, 0, 5, 2, 1, -1, collect );
	CHECK( quads.size() == 4 );
	CHECK( Near( quads[1].x[0], 0 ) && Near( quads[1].y[0], 17 ) );
	CHECK( Near( quads[2].x[0], 0 ) && Near( quads[2].y[0], 17 ) );
	CHECK( Near( quads[3].x[0], 16 + 18 ) );

	// An unmapped byte draws nothing and advances nothing; maxlen stops.
	quads.clear();
	Layout( font, "AzB", 0, 0, 0, 0, 0, 1, 1, 2, collect );
	CHECK( quads.size() == 1 );

	// A quarter turn about the string's origin.
	quads.clear();
	Layout( font, "A", 10, 10, 90, 0, 0, 1, 1, -1, collect );
	CHECK( quads.size() == 1 );
	CHECK( Near( quads[0].x[1], 10 ) && Near( quads[0].y[1], 2 ) );
}

void TestJustify() {
	const std::string text = Fixture();
	Font font;
	CHECK( Parse( text.data(), text.size(), font ) );
	const Rect rect{ 10, 20, 100, 40 };
	float x, y;
	Justify( &font, rect, 0, 1, "AB", 0, 0, 1, 1, x, y );
	CHECK( Near( x, 10 + ( 100 - 16 ) * 0.5f ) && Near( y, 20 + ( 40 - 12 ) * 0.5f ) );
	Justify( &font, rect, 1, 0, "AB", 0, 0, 1, 1, x, y );
	CHECK( Near( x, 10 ) && Near( y, 20 ) );
	Justify( &font, rect, 2, 2, "AB", 0, 0, 1, 1, x, y );
	CHECK( Near( x, 110 - 16 ) && Near( y, 60 - 12 ) );
	// The measured width takes the scale and one character spacing, truncated.
	Justify( &font, rect, 2, 0, "AB", 3, 0, 1.5f, 1, x, y );
	CHECK( Near( x, 110 - 27 ) );
	// An unknown justification answers zero on that axis.
	Justify( &font, rect, 7, 9, "AB", 0, 0, 1, 1, x, y );
	CHECK( x == 0.0f && y == 0.0f );
}

/* Every retail font parses; the two the engine draws with have the metrics
 * their files declare. */
void TestRetailFonts() {
	fntech3_tests::AlicePak pak;
	if ( !pak.Available() ) {
		std::cout << "retail fonts: skipped, no installation at " << pak.Path() << "\n";
		return;
	}
	int fonts = 0;
	for ( const fntech3_tests::PakEntry &entry : pak.Entries() ) {
		const std::string lower = fntech3_tests::Lower( entry.name );
		if ( !fntech3_tests::EndsWith( lower, ".ritualfont" ) ) continue;
		std::string text;
		CHECK( pak.Read( entry, text ) );
		Font font;
		std::string error;
		if ( !Parse( text.data(), text.size(), font, &error ) ) {
			std::cerr << entry.name << ": " << error << "\n";
			std::exit( 1 );
		}
		if ( lower == "fonts/courier-14.ritualfont" ) {
			CHECK( font.height == 13.0f && font.aspect == 2.0f );
			CHECK( font.indirection[' '] == 0 && font.indirection['A'] == 33 );
			CHECK( StringWidth( &font, "ABC" ) == 24 );
		}
		if ( lower == "fonts/asrafel.ritualfont" ) {
			CHECK( font.height == 30.0f && font.aspect == 1.0f );
		}
		++fonts;
	}
	CHECK( fonts == 14 );
	std::cout << "retail fonts: " << fonts << " parsed from " << pak.Path() << "\n";
}

} // namespace

int main() {
	TestParse();
	TestRefusals();
	TestMetrics();
	TestLayout();
	TestJustify();
	TestRetailFonts();
	std::cout << "alice ritualfont tests passed\n";
	return 0;
}
