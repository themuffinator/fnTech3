/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_FONT_HPP
#define FNTECH3_CLIENT_JK2_FONT_HPP
#include "../compat/jk2_font_format.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {
struct JK2Font {
	std::string path;
	fntech3::jk2::font::Font metrics;
	qhandle_t shader = 0;
};
/* Each module has its own handles. Registration refreshes the renderer shader
 * after a restart; no renderer handle survives by being assumed current. */
std::vector<JK2Font> jk2Fonts;

const char *CL_JK2FontText( vm_t *vm, intptr_t address, unsigned limit = 16384 ) {
	if ( !address ) Com_Error( ERR_DROP, "Missing Jedi Outcast font string" );
	const char *text = static_cast<const char *>( VM_ExplicitArgPtr( vm, address ) );
	for ( unsigned i = 0; i < limit; ++i ) {
		VM_CHECKBOUNDS( vm, address + i, 1 );
		if ( !text[i] ) return text;
	}
	Com_Error( ERR_DROP, "Unterminated Jedi Outcast font string" );
	return "";
}
int CL_JK2RegisterFont( const char *name ) {
	if ( !Q_stricmpn( name, "fonts/", 6 ) ) name += 6;
	const std::string leaf = name;
	if ( leaf.empty() || leaf.size() > 48 || leaf.find_first_of( ":/\\" ) != std::string::npos ||
		leaf.find( ".." ) != std::string::npos ) Com_Error( ERR_DROP, "Invalid Jedi Outcast font path" );
	const std::string path = "fonts/" + leaf;
	for ( std::size_t i = 0; i < jk2Fonts.size(); ++i ) {
		if ( !Q_stricmp( jk2Fonts[i].path.c_str(), path.c_str() ) ) {
			jk2Fonts[i].shader = re.RegisterShaderNoMip( path.c_str() );
			return jk2Fonts[i].shader ? int( i + 1 ) : 0;
		}
	}
	if ( jk2Fonts.size() >= 32 ) Com_Error( ERR_DROP, "Jedi Outcast font limit exceeded" );
	void *bytes = nullptr;
	const int size = FS_ReadFile( ( path + ".fontdat" ).c_str(), &bytes );
	JK2Font font;
	const bool valid = size >= 0 && fntech3::jk2::font::Read( bytes, static_cast<std::size_t>( size ), font.metrics );
	if ( bytes ) FS_FreeFile( bytes );
	if ( !valid ) {
		Com_Printf( S_COLOR_YELLOW "Jedi Outcast font %s is missing or invalid\n", path.c_str() );
		return 0;
	}
	font.path = path;
	font.shader = re.RegisterShaderNoMip( path.c_str() );
	if ( !font.shader ) return 0;
	jk2Fonts.push_back( std::move( font ) );
	return static_cast<int>( jk2Fonts.size() );
}
const JK2Font *CL_JK2FindFont( intptr_t handle ) {
	const auto index = static_cast<std::uint32_t>( handle ) & 0x00ffffffu;
	return index && index <= jk2Fonts.size() ? &jk2Fonts[index - 1] : nullptr;
}

template<class Draw>
void CL_JK2DrawFont( const JK2Font &font, const char *text, float x, float y,
	const float *rgba, std::uint32_t style, int limit, float scale, Draw draw ) {
	if ( !fntech3::jk2::font::Finite( scale ) || scale < 0 || scale > 128 || limit < -1 )
		Com_Error( ERR_DROP, "Invalid Jedi Outcast font drawing arguments" );
	if ( style & 0x3f000000u ) Com_Error( ERR_DROP, "Unknown Jedi Outcast font style" );
	if ( ( style & 0x40000000u ) && ( ( Sys_Milliseconds() >> 7 ) & 1 ) ) return;
	const float start = x;
	/* Metrics are authored data. Layout and submission use this engine's
 * HUD/menu transform and its renderer's texture/colour pipeline. */
	const float baseline = y + ( font.metrics.height - font.metrics.descender / 2 ) * scale;
	y = baseline;
	std::array<float, 4> color{ 1, 1, 1, 1 };
	if ( rgba ) std::copy_n( rgba, 4, color.begin() );
	re.SetColor( color.data() );
	for ( ; *text; ++text ) {
		if ( fntech3::jk2::font::ColorEscape( text ) ) {
			std::copy_n( g_color_table[ColorIndex( *++text )], 3, color.begin() );
			re.SetColor( color.data() );
			continue;
		}
		if ( *text == '\r' ) continue;
		if ( *text == '\n' ) { x = start; y += font.metrics.pointSize * scale; continue; }
		const auto &glyph = font.metrics.glyphs[static_cast<unsigned char>( *text )];
		const float advance = glyph.advance * scale;
		if ( limit >= 0 && x + advance - start > limit ) break;
		if ( glyph.width && glyph.height ) {
			const float left = x + glyph.offset * scale, top = y - glyph.baseline * scale;
			if ( style & 0x80000000u ) {
				const float shadow[4] = { 0, 0, 0, color[3] };
				re.SetColor( shadow );
				draw( left + scale, top + scale, glyph.width * scale, glyph.height * scale, glyph, font.shader );
				re.SetColor( color.data() );
			}
			draw( left, top, glyph.width * scale, glyph.height * scale, glyph, font.shader );
		}
		x += advance;
	}
	re.SetColor( nullptr );
}

template<class Draw>
bool CL_JK2FontCall( vm_t *vm, intptr_t *args, intptr_t &result, int first, Draw draw ) {
	using namespace fntech3::jk2::font;
	const auto ordinal = args[0] - first;
	if ( ordinal < 0 || ordinal > 4 ) return false;
	result = 0;
	if ( ordinal == 0 ) { result = CL_JK2RegisterFont( CL_JK2FontText( vm, args[1], 64 ) ); return true; }
	if ( ordinal == 2 ) { result = Characters( CL_JK2FontText( vm, args[1] ) ); return true; }
	const auto *font = CL_JK2FindFont( ordinal == 3 ? args[1] : ordinal == 4 ? args[5] : args[2] );
	if ( !font ) return true;
	if ( ordinal == 1 ) { result = Width( font->metrics, CL_JK2FontText( vm, args[1] ), VMF(3) ); return true; }
	if ( ordinal == 3 ) {
		const float height = font->metrics.pointSize * VMF(2);
		if ( !fntech3::jk2::font::Finite( height ) || height < 0 ||
			static_cast<double>( height ) > std::numeric_limits<int>::max() )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast font scale" );
		result = static_cast<int>( height ); return true;
	}
	const float *rgba = nullptr;
	if ( args[4] ) { VM_CHECKBOUNDS( vm, args[4], sizeof( float ) * 4 ); rgba = static_cast<const float *>( VM_ExplicitArgPtr( vm, args[4] ) ); }
	CL_JK2DrawFont( *font, CL_JK2FontText( vm, args[3] ), static_cast<float>( args[1] ), static_cast<float>( args[2] ),
		rgba, static_cast<std::uint32_t>( args[5] ) & 0xff000000u, static_cast<int>( args[6] ), VMF(7), draw );
	return true;
}
} // namespace
#endif
