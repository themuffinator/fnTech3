/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* American McGee's Alice's Ritual fonts and the engine's own 2D art.
 *
 * Retail loads a font once by name into a cache of 255 (R_LoadFont
 * 0x00498160), reads `fonts/<name>.RitualFont` for the glyph table, and
 * registers `gfx/fonts/<name>` as the picture (0x00498580). That registration
 * forces every stage of the font's shader to take the colour the caller set
 * (stage +0x298 = 0xf, +0x2b0 = 0x10 at 0x00498614), whatever the shader
 * script says: `scripts/fonts.shader` gives every font `rgbgen identity`, yet
 * retail text is drawn in its widgets' colours. This engine's renderers honour
 * `rgbgen identity`, so the font's image is registered here as a plain 2D
 * picture instead, which takes the vertex colour and blends by alpha exactly as
 * the font stage does. The image is the one the script maps,
 * `gfx/fonts/<name>.tga`, and like the script it is neither mipmapped nor
 * reduced by r_picmip.
 *
 * The engine's fills are untextured boxes in retail (DrawBox, renderer +0x68).
 * Alice ships no `white` shader, so the white here is the engine's own one
 * texel picture. */

#include "client_alice_ui.hpp"

#include <unordered_map>
#include <vector>

namespace {

namespace font = fntech3::alice::ritualfont;

struct CachedFont {
	std::string name;
	font::Font metrics;
	qhandle_t shader = 0;
	int epoch = 0;
};

std::vector<CachedFont> aliceFonts;
std::unordered_map<std::string, qhandle_t> aliceShaders;
qhandle_t aliceWhite = 0;
/* Bumped whenever the renderer starts; a handle from an older epoch names a
 * shader that no longer exists. */
int aliceArtEpoch = 1;
int aliceWhiteEpoch = 0;

std::string LowerName( const char *name ) {
	std::string lowered( name ? name : "" );
	for ( char &c : lowered ) {
		if ( c >= 'A' && c <= 'Z' ) c = static_cast<char>( c - 'A' + 'a' );
	}
	return lowered;
}

float VidWidth() { return static_cast<float>( cls.glconfig.vidWidth ); }
float VidHeight() { return static_cast<float>( cls.glconfig.vidHeight ); }

/* The rectangle, in pixels, the 2D draws are cut to while one is set. */
bool clipOn = false;
float clipRect[4];

/* Whether the draws are F.A.K.K.2's HUD menus, which the HUD's aspect rules
 * place (cl_hud.cpp). */
bool hudLayout = false;

/* The name a HUD rule knows a picture by: the art's own, `white` for a fill,
 * and the picture its script maps for a font's glyphs. */
std::string HudShaderName( qhandle_t shader ) {
	if ( !shader ) return std::string();
	if ( shader == aliceWhite ) return "white";
	for ( const auto &entry : aliceShaders ) {
		if ( entry.second == shader ) return entry.first;
	}
	for ( const CachedFont &cached : aliceFonts ) {
		if ( cached.shader == shader ) return "gfx/fonts/" + cached.name;
	}
	return std::string();
}

/* A quad and its texture coordinates cut to the clip, as a scissor leaves it;
 * false when nothing of it is left. */
bool ClipQuad( float &x, float &y, float &w, float &h, float &s0, float &t0, float &s1, float &t1 ) {
	if ( !( w > 0.0f ) || !( h > 0.0f ) ) return false;
	const float left = clipRect[0], top = clipRect[1];
	const float right = left + clipRect[2], bottom = top + clipRect[3];
	float x0 = x, y0 = y, x1 = x + w, y1 = y + h;
	float ns0 = s0, nt0 = t0, ns1 = s1, nt1 = t1;
	if ( x0 < left ) { ns0 = s0 + ( left - x ) / w * ( s1 - s0 ); x0 = left; }
	if ( x1 > right ) { ns1 = s0 + ( right - x ) / w * ( s1 - s0 ); x1 = right; }
	if ( y0 < top ) { nt0 = t0 + ( top - y ) / h * ( t1 - t0 ); y0 = top; }
	if ( y1 > bottom ) { nt1 = t0 + ( bottom - y ) / h * ( t1 - t0 ); y1 = bottom; }
	if ( !( x1 > x0 ) || !( y1 > y0 ) ) return false;
	x = x0;
	y = y0;
	w = x1 - x0;
	h = y1 - y0;
	s0 = ns0;
	t0 = nt0;
	s1 = ns1;
	t1 = nt1;
	return true;
}

/* The font's picture, registered again after a renderer restart the way
 * retail re-registers it when the renderer's sequence moves (0x004986ac). */
qhandle_t FontShader( CachedFont &cached ) {
	if ( cached.epoch == aliceArtEpoch ) {
		return cached.shader;
	}
	cached.epoch = aliceArtEpoch;
	cached.shader = 0;
	if ( !re.RegisterShaderFromRGBA || !re.LoadImagePixels ) {
		cached.shader = re.RegisterShaderNoMip( va( "gfx/fonts/%s", cached.name.c_str() ) );
		return cached.shader;
	}
	int width = 0, height = 0;
	byte *pixels = re.LoadImagePixels( va( "gfx/fonts/%s.tga", cached.name.c_str() ), &width, &height );
	if ( pixels && width > 0 && height > 0 ) {
		cached.shader = re.RegisterShaderFromRGBA( va( "*fntech3/alice/font/%s", cached.name.c_str() ),
			pixels, width, height );
	}
	if ( pixels ) {
		Z_Free( pixels );
	}
	if ( !cached.shader ) {
		// Retail's own message for a font whose picture failed (0x004985e1).
		Com_Printf( "Could not load font shader for %s\n", cached.name.c_str() );
	}
	return cached.shader;
}

void DrawQuad( const font::Quad &quad, qhandle_t shader, bool rotated ) {
	if ( !rotated ) {
		CL_RitualStretchPic( quad.x[0], quad.y[0], quad.x[1] - quad.x[0], quad.y[2] - quad.y[0],
			quad.s0, quad.t0, quad.s1, quad.t1, shader );
		return;
	}
	// A turned glyph is drawn whole; no retail menu turns its text.
	if ( !re.DrawQuad ) {
		return;
	}
	refDrawQuad_t out{};
	const float st[4][2] = { { quad.s0, quad.t0 }, { quad.s1, quad.t0 }, { quad.s1, quad.t1 }, { quad.s0, quad.t1 } };
	for ( int i = 0; i < 4; ++i ) {
		out.xy[i][0] = quad.x[i];
		out.xy[i][1] = quad.y[i];
		out.st[i][0] = st[i][0];
		out.st[i][1] = st[i][1];
	}
	out.shader = shader;
	re.DrawQuad( &out );
}

} // namespace

/*
====================
CL_AliceFontRegister
====================
*/
int CL_AliceFontRegister( const char *name ) {
	if ( !name || !name[0] ) {
		return -1;
	}
	const std::string key = LowerName( name );
	for ( std::size_t i = 0; i < aliceFonts.size(); ++i ) {
		if ( aliceFonts[i].name == key ) {
			return static_cast<int>( i );
		}
	}
	if ( static_cast<int>( aliceFonts.size() ) >= font::MaxFonts ) {
		Com_Printf( "LoadFont: Too many fonts loaded!  Couldn't load %s\n", name );
		return -1;
	}
	void *buffer = nullptr;
	const int length = FS_ReadFile( va( "fonts/%s.RitualFont", name ), &buffer );
	if ( length < 0 || !buffer ) {
		Com_Printf( "LoadFont: Couldn't load font %s\n", name );
		return -1;
	}
	CachedFont cached;
	cached.name = key;
	std::string error;
	const bool parsed = font::Parse( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ),
		cached.metrics, &error );
	FS_FreeFile( buffer );
	if ( !parsed ) {
		Com_DPrintf( "font %s: %s\n", name, error.c_str() );
		Com_Printf( "WARNING: Error parsing font %s.\n", name );
		return -1;
	}
	aliceFonts.push_back( std::move( cached ) );
	return static_cast<int>( aliceFonts.size() ) - 1;
}

const fntech3::alice::ritualfont::Font *CL_AliceFontMetrics( int index ) {
	return index >= 0 && index < static_cast<int>( aliceFonts.size() ) ? &aliceFonts[index].metrics : nullptr;
}

int CL_AliceFontLineHeight( int index ) {
	return font::LineHeight( CL_AliceFontMetrics( index ) );
}

/*
====================
CL_AliceFontPrint
====================
*/
void CL_AliceFontPrint( int index, const float *rgba, bool noRescale, float x, float y,
	const char *text, int maxLength, float angle, float charSpacing, float lineSpacing,
	float wScale, float hScale ) {
	if ( !text ) {
		return;
	}
	if ( index < 0 || index >= static_cast<int>( aliceFonts.size() ) ) {
		Com_DPrintf( "No font loaded to print \"%s\"!\n", text );
		return;
	}
	if ( !noRescale ) {
		x *= VidWidth() * ( 1.0f / 640.0f );
		wScale *= VidWidth() * ( 1.0f / 640.0f );
		y *= VidHeight() * ( 1.0f / 480.0f );
		hScale *= VidHeight() * ( 1.0f / 480.0f );
	}
	CachedFont &cached = aliceFonts[index];
	const qhandle_t shader = FontShader( cached );
	if ( !shader ) {
		return;
	}
	re.SetColor( rgba );
	const bool rotated = angle != 0.0f;
	font::Layout( cached.metrics, text, x, y, angle, charSpacing, lineSpacing, wScale, hScale, maxLength,
		[shader, rotated]( const font::Quad &quad ) { DrawQuad( quad, shader, rotated ); } );
	re.SetColor( nullptr );
}

void CL_AliceFontPrintJustified( int index, const float *rgba, bool noRescale,
	const fntech3::alice::ritualfont::Rect &rect, int horizontal, int vertical, const char *text,
	float angle, float charSpacing, float lineSpacing, float wScale, float hScale ) {
	float x = rect.x, y = rect.y;
	font::Justify( CL_AliceFontMetrics( index ), rect, horizontal, vertical, text, charSpacing, lineSpacing,
		wScale, hScale, x, y );
	CL_AliceFontPrint( index, rgba, noRescale, x, y, text, -1, angle, charSpacing, lineSpacing, wScale, hScale );
}

/*
====================
CL_AliceShader

Menu and loading-screen art. Retail's static-shader slots (0x0040ad70) load an
image into one of twenty renderer slots on every draw; a picture registered by
name and kept draws the same image.
====================
*/
qhandle_t CL_AliceShader( const char *name ) {
	if ( !name || !name[0] ) {
		return 0;
	}
	const std::string key = LowerName( name );
	const auto found = aliceShaders.find( key );
	if ( found != aliceShaders.end() ) {
		return found->second;
	}
	const qhandle_t shader = re.RegisterShaderNoMip( name );
	aliceShaders.emplace( key, shader );
	return shader;
}

const char *CL_RitualDefaultFont( void ) {
	/* The font a .urc widget starts with: the menu loader's InitFrame gives
	 * F.A.K.K.2's verdana-12 (fakk2.exe 1.02 0x004aad60) where Alice's gives
	 * courier-14. A bare UIFont is verdana-14 there (0x004b0253), but no .urc
	 * widget keeps a bare one. */
	return Com_GameIsFAKK2() ? "verdana-12" : "courier-14";
}

const char *CL_RitualMenuSoundtrack( void ) {
	return Com_GameIsFAKK2() ? "music/intro.mus" : fntech3::alice::screen::IntroSoundtrack;
}

qhandle_t CL_AliceWhite( void ) {
	if ( aliceWhiteEpoch == aliceArtEpoch ) {
		return aliceWhite;
	}
	aliceWhiteEpoch = aliceArtEpoch;
	byte white[4] = { 255, 255, 255, 255 };
	aliceWhite = re.RegisterShaderFromRGBA
		? re.RegisterShaderFromRGBA( "*fntech3/alice/white", white, 1, 1 ) : 0;
	if ( !aliceWhite ) {
		// The renderer's own untextured picture, as a last resort.
		aliceWhite = re.RegisterShaderNoMip( "*white" );
	}
	return aliceWhite;
}

void CL_AliceFill( float x, float y, float w, float h, const float *rgba ) {
	re.SetColor( rgba );
	CL_RitualStretchPic( x, y, w, h, 0, 0, 1, 1, CL_AliceWhite() );
	re.SetColor( nullptr );
}

void CL_AliceDrawPic( float x, float y, float w, float h, const float *rgba, qhandle_t shader ) {
	if ( !shader ) {
		return;
	}
	re.SetColor( rgba );
	CL_RitualStretchPic( x, y, w, h, 0, 0, 1, 1, shader );
	re.SetColor( nullptr );
}

void CL_RitualSetClip( const float *rect ) {
	clipOn = rect != nullptr;
	if ( rect ) {
		for ( int i = 0; i < 4; ++i ) clipRect[i] = rect[i];
	}
}

void CL_RitualStretchPic( float x, float y, float w, float h, float s0, float t0, float s1, float t1,
	qhandle_t shader ) {
	if ( clipOn && !ClipQuad( x, y, w, h, s0, t0, s1, t1 ) ) {
		return;
	}
	/* A HUD menu's quad is cut in the stretched screen, as every menu's is,
	 * then placed by the rule its name and place match. */
	if ( hudLayout ) {
		CL_HudRegisterShaderName( shader, HudShaderName( shader ).c_str() );
		if ( !CL_HudAdjustPic( &x, &y, &w, &h, shader ) ) return;
	}
	re.DrawStretchPic( x, y, w, h, s0, t0, s1, t1, shader );
}

void CL_RitualSetHudLayout( bool on ) {
	hudLayout = on;
}

bool CL_RitualHudLayout( void ) {
	return hudLayout;
}

void CL_AliceShaderSize( qhandle_t shader, int *width, int *height ) {
	*width = *height = 0;
	if ( shader && re.GetShaderImageSize ) {
		re.GetShaderImageSize( shader, width, height );
	}
}

void CL_AliceClearBlack( void ) {
	static const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	CL_AliceFill( 0, 0, VidWidth(), VidHeight(), black );
}

/*
====================
CL_AliceConsoleCharset

This engine's console and its text fields draw from a 16 by 16 character
sheet. Alice ships none; its console draws in courier-14 (UIFloatingConsole
0x004c5b00 takes the widget default font). So the sheet is built here from
courier-14's own glyphs: each glyph the font maps is stretched into its
character's cell, and a character the font has no glyph for stays empty.
====================
*/
qhandle_t CL_AliceConsoleCharset( void ) {
	const int index = CL_AliceFontRegister( "courier-14" );
	const font::Font *metrics = CL_AliceFontMetrics( index );
	if ( !metrics || !re.LoadImagePixels || !re.RegisterShaderFromRGBA ) {
		return 0;
	}
	int width = 0, height = 0;
	byte *source = re.LoadImagePixels( "gfx/fonts/courier-14.tga", &width, &height );
	if ( !source || width <= 0 || height <= 0 ) {
		if ( source ) {
			Z_Free( source );
		}
		return 0;
	}
	constexpr int kCell = 16;
	constexpr int kSheet = kCell * 16;
	std::vector<byte> sheet( kSheet * kSheet * 4, 0 );
	for ( int ch = 0; ch < 256; ++ch ) {
		const font::Glyph *glyph = metrics->GlyphFor( static_cast<unsigned char>( ch ) );
		if ( !glyph ) {
			continue;
		}
		const float gx = glyph->s * width, gy = glyph->t * height;
		const float gw = glyph->width * width, gh = glyph->height * height;
		if ( gw <= 0.0f || gh <= 0.0f ) {
			continue;
		}
		const int cellX = ( ch & 15 ) * kCell, cellY = ( ch >> 4 ) * kCell;
		for ( int py = 0; py < kCell; ++py ) {
			int sy = static_cast<int>( gy + ( py + 0.5f ) * gh / kCell );
			sy = sy < 0 ? 0 : sy >= height ? height - 1 : sy;
			for ( int px = 0; px < kCell; ++px ) {
				int sx = static_cast<int>( gx + ( px + 0.5f ) * gw / kCell );
				sx = sx < 0 ? 0 : sx >= width ? width - 1 : sx;
				const byte *from = source + ( sy * width + sx ) * 4;
				byte *to = sheet.data() + ( ( cellY + py ) * kSheet + cellX + px ) * 4;
				to[0] = from[0];
				to[1] = from[1];
				to[2] = from[2];
				to[3] = from[3];
			}
		}
	}
	Z_Free( source );
	return re.RegisterShaderFromRGBA( "*fntech3/alice/charset", sheet.data(), kSheet, kSheet );
}

void CL_AliceArtRendererStarted( void ) {
	++aliceArtEpoch;
	aliceShaders.clear();
	aliceWhite = 0;
}
