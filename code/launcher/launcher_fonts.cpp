/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_fonts.hpp"

#include "launcher_fs.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace fntech3::launcher::gui {

namespace {

constexpr std::size_t kMaxFontBytes = 64u * 1024u * 1024u;

// TrueType, OpenType (CFF) and collections; anything else is not handed to
// the rasterizer, which trusts what it is given.
bool LooksLikeFont( const std::string &data )
{
	if ( data.size() < 12 ) return false;
	const unsigned char *bytes = reinterpret_cast<const unsigned char *>( data.data() );
	return ( bytes[0] == 0 && bytes[1] == 1 && bytes[2] == 0 && bytes[3] == 0 )
		|| !std::memcmp( bytes, "OTTO", 4 ) || !std::memcmp( bytes, "true", 4 ) || !std::memcmp( bytes, "ttcf", 4 );
}

bool ReadFont( const std::string &path, std::string &data )
{
	return !path.empty() && fs::ReadFile( fs::FromUtf8( path ), data, kMaxFontBytes ) && LooksLikeFont( data );
}

ImFont *AddFont( const std::string &path )
{
	std::string data;
	if ( !ReadFont( path, data ) ) return nullptr;
	void *copy = IM_ALLOC( data.size() );
	std::memcpy( copy, data.data(), data.size() );
	ImFontConfig config;
	const std::string name = fs::ToUtf8( fs::FromUtf8( path ).filename() );
	std::snprintf( config.Name, sizeof( config.Name ), "%s", name.c_str() );
	// The atlas owns the copy from here, and frees it with IM_FREE.
	return ImGui::GetIO().Fonts->AddFontFromMemoryTTF( copy, int( data.size() ), 0.0f, &config );
}

std::string FirstExisting( const std::vector<std::string> &candidates )
{
	for ( const std::string &candidate : candidates ) {
		if ( fs::IsRegularFile( fs::FromUtf8( candidate ) ) ) return candidate;
	}
	return {};
}

#if !defined( _WIN32 ) && !defined( __APPLE__ )
// The desktop's own choice, as fontconfig resolves it.
std::string FontConfigMatch( const char *pattern )
{
	const std::string command = std::string( "fc-match --format=%{file} '" ) + pattern + "' 2>/dev/null";
	FILE *pipe = popen( command.c_str(), "r" );
	if ( !pipe ) return {};
	std::string output;
	char buffer[1024];
	std::size_t count;
	while ( ( count = std::fread( buffer, 1, sizeof( buffer ), pipe ) ) > 0 && output.size() < 8192 ) {
		output.append( buffer, count );
	}
	pclose( pipe );
	while ( !output.empty() && ( output.back() == '\n' || output.back() == '\r' || output.back() == ' ' ) ) output.pop_back();
	return !output.empty() && output[0] == '/' && fs::IsRegularFile( fs::FromUtf8( output ) ) ? output : std::string();
}
#endif

struct FontFiles {
	std::string regular, medium, bold, fallback;
};

FontFiles SystemFontFiles()
{
	FontFiles files;
#if defined( _WIN32 )
	std::string folder = fs::Environment( "WINDIR" );
	if ( folder.empty() ) folder = "C:\\Windows";
	folder += "\\Fonts\\";
	files.regular = FirstExisting( { folder + "segoeui.ttf", folder + "tahoma.ttf", folder + "arial.ttf" } );
	files.medium = FirstExisting( { folder + "seguisb.ttf", folder + "segoeuib.ttf" } );
	files.bold = FirstExisting( { folder + "segoeuib.ttf", folder + "seguisb.ttf", folder + "arialbd.ttf" } );
	files.fallback = FirstExisting( { folder + "msyh.ttc", folder + "YuGothM.ttc", folder + "meiryo.ttc",
		folder + "malgun.ttf", folder + "simsun.ttc" } );
#elif defined( __APPLE__ )
	files.regular = FirstExisting( { "/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/Helvetica.ttc",
		"/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Arial.ttf" } );
	files.fallback = FirstExisting( { "/System/Library/Fonts/Hiragino Sans GB.ttc",
		"/System/Library/Fonts/Supplemental/Arial Unicode.ttf", "/Library/Fonts/Arial Unicode.ttf" } );
#else
	files.regular = FontConfigMatch( "sans-serif:style=Regular" );
	files.medium = FontConfigMatch( "sans-serif:weight=semibold" );
	files.bold = FontConfigMatch( "sans-serif:weight=bold" );
	files.fallback = FontConfigMatch( "sans-serif:lang=zh-cn" );
	if ( files.regular.empty() ) {
		files.regular = FirstExisting( {
			"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", "/usr/share/fonts/noto/NotoSans-Regular.ttf",
			"/usr/share/fonts/google-noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
			"/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/dejavu/DejaVuSans.ttf",
			"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
			"/usr/share/fonts/liberation/LiberationSans-Regular.ttf" } );
	}
	if ( files.bold.empty() ) {
		files.bold = FirstExisting( {
			"/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf", "/usr/share/fonts/noto/NotoSans-Bold.ttf",
			"/usr/share/fonts/google-noto/NotoSans-Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
			"/usr/share/fonts/TTF/DejaVuSans-Bold.ttf", "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
			"/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
			"/usr/share/fonts/liberation/LiberationSans-Bold.ttf" } );
	}
	if ( files.fallback.empty() ) {
		files.fallback = FirstExisting( {
			"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
			"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc" } );
	}
	// fontconfig answers every question with its nearest match, which is the
	// regular face itself when there is no heavier one.
	if ( files.medium == files.regular ) files.medium.clear();
	if ( files.bold == files.regular ) files.bold.clear();
#endif
	return files;
}

// Kept alive for the atlas, which reads merged sources without owning them,
// so one copy of a large collection serves every weight.
std::unique_ptr<std::string> fallbackData;
bool fallbackTried = false;

bool NeedsFallback( const std::string &text )
{
	for ( std::size_t i = 0; i < text.size(); ++i ) {
		const unsigned char c = static_cast<unsigned char>( text[i] );
		// Three- and four-byte UTF-8 sequences from U+2E80 up: CJK, kana, Hangul.
		if ( c >= 0xf0 ) return true;
		if ( c >= 0xe2 && i + 1 < text.size() ) {
			const unsigned codepoint = ( unsigned( c & 0x0f ) << 12 )
				| ( unsigned( static_cast<unsigned char>( text[i + 1] ) & 0x3f ) << 6 );
			if ( codepoint >= 0x2e80 ) return true;
		}
	}
	return false;
}

} // namespace

Fonts LoadFonts()
{
	Fonts fonts;
	ImGuiIO &io = ImGui::GetIO();
	const FontFiles files = SystemFontFiles();
	fonts.regular = AddFont( files.regular );
	if ( fonts.regular ) {
		fonts.systemFont = true;
		fonts.description = fs::ToUtf8( fs::FromUtf8( files.regular ).filename() );
		fonts.medium = AddFont( files.medium );
		fonts.bold = AddFont( files.bold );
	} else {
		fonts.regular = io.Fonts->AddFontDefaultVector();
		fonts.description = "Dear ImGui's built-in font";
	}
	if ( !fonts.bold ) {
		fonts.bold = fonts.medium ? fonts.medium : fonts.regular;
		fonts.syntheticBold = !fonts.medium;
	}
	if ( !fonts.medium ) fonts.medium = fonts.bold != fonts.regular ? fonts.bold : fonts.regular;
	io.FontDefault = fonts.regular;
	return fonts;
}

void EnsureFallbackFor( Fonts &fonts, const std::string &text )
{
	if ( fallbackTried || !fonts.systemFont || !NeedsFallback( text ) ) return;
	fallbackTried = true;
	auto data = std::make_unique<std::string>();
	if ( !ReadFont( SystemFontFiles().fallback, *data ) ) return;
	fallbackData = std::move( data );
	for ( ImFont *font : { fonts.regular, fonts.medium, fonts.bold } ) {
		bool merged = false;
		if ( font == fonts.medium && fonts.medium == fonts.regular ) merged = true;
		if ( font == fonts.bold && ( fonts.bold == fonts.regular || fonts.bold == fonts.medium ) ) merged = true;
		if ( merged ) continue;
		ImFontConfig config;
		config.MergeMode = true;
		config.DstFont = font;
		config.FontDataOwnedByAtlas = false;
		std::snprintf( config.Name, sizeof( config.Name ), "fallback" );
		ImGui::GetIO().Fonts->AddFontFromMemoryTTF( fallbackData->data(), int( fallbackData->size() ), 0.0f, &config );
	}
}

} // namespace fntech3::launcher::gui
