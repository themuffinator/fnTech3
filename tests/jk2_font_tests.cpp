/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jk2_font_format.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace fntech3::jk2::font;

void Word( std::vector<unsigned char> &bytes, unsigned at, unsigned value, unsigned width ) {
	for ( unsigned i = 0; i < width; ++i ) bytes[at + i] = static_cast<unsigned char>( value >> ( i * 8 ) );
}

int main( int argc, char **argv ) {
	std::vector<unsigned char> bytes( FileBytes );
	Word( bytes, 7168, 20, 2 ); Word( bytes, 7170, 22, 2 );
	Word( bytes, 7172, 17, 2 ); Word( bytes, 7174, 5, 2 );
	Word( bytes, 65 * GlyphBytes, 12, 2 );
	Word( bytes, 65 * GlyphBytes + 2, 18, 2 );
	Word( bytes, 65 * GlyphBytes + 4, 10, 2 );
	Word( bytes, 65 * GlyphBytes + 6, 0xffff, 2 );
	Word( bytes, 65 * GlyphBytes + 8, 16, 4 );
	Word( bytes, 65 * GlyphBytes + 20, 0x3f000000, 4 );
	Word( bytes, 65 * GlyphBytes + 24, 0x3f800000, 4 );
	Font font;
	assert( Read( bytes.data(), bytes.size(), font ) );
	assert( font.glyphs['A'].offset == -1 && font.glyphs['A'].baseline == 16 );
	assert( font.glyphs['A'].s2 == .5f && font.glyphs['A'].t2 == 1.0f );
	assert( Width( font, "A^2AA", .5f ) == 15 && Characters( "A^2AA\r\n" ) == 3 );
	assert( Width( font, "AAA", std::numeric_limits<float>::infinity() ) == 0 );
	assert( Width( font, "AAA", std::numeric_limits<float>::max() ) == std::numeric_limits<int>::max() );
	assert( !Read( nullptr, bytes.size(), font ) );
	auto before = font;
	assert( !Read( bytes.data(), bytes.size() - 1, font ) );
	assert( !std::memcmp( &before, &font, sizeof( font ) ) );
	for ( unsigned malformed : { 0x7fc00000u, 0x7f800000u, 0xbf800000u, 0x3f800001u } ) {
		Word( bytes, 65 * GlyphBytes + 20, malformed, 4 );
		assert( !Read( bytes.data(), bytes.size(), font ) );
		assert( !std::memcmp( &before, &font, sizeof( font ) ) );
	}
	Word( bytes, 65 * GlyphBytes + 20, 0x3f000000, 4 );
	Word( bytes, 65 * GlyphBytes + 12, 0x3f400000, 4 );
	assert( !Read( bytes.data(), bytes.size(), font ) );
	Word( bytes, 65 * GlyphBytes + 12, 0, 4 );
	Word( bytes, 65 * GlyphBytes, 0xffff, 2 );
	assert( !Read( bytes.data(), bytes.size(), font ) );
	Word( bytes, 65 * GlyphBytes, 12, 2 );
	Word( bytes, 65 * GlyphBytes + 4, 0xffff, 2 );
	assert( !Read( bytes.data(), bytes.size(), font ) );
	Word( bytes, 65 * GlyphBytes + 4, 10, 2 );
	Word( bytes, 7168, 0, 2 );
	assert( !Read( bytes.data(), bytes.size(), font ) );
	for ( int i = 1; i < argc; ++i ) {
		std::ifstream input( argv[i], std::ios::binary );
		std::vector<unsigned char> retail( ( std::istreambuf_iterator<char>( input ) ), {} );
		assert( Read( retail.data(), retail.size(), font ) );
		std::cout << argv[i] << " decoded: pointSize=" << font.pointSize << " height=" << font.height << '\n';
	}
	std::cout << "Jedi Outcast font format and malformed input hold\n";
}
