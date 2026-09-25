/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_MENU_HPP
#define FNTECH3_COMPAT_ALICE_MENU_HPP

/* American McGee's Alice's menu widgets: the rules the engine UI's widget
 * classes follow, kept apart from the engine so they can be tested. The
 * client's host (code/client/cl_alice_ui.cpp) draws the widgets and feeds
 * them input; everything here is arithmetic, text and file formats.
 *
 * Every rule is read from the retail executable, alice.exe (image base
 * 0x00400000); the address each one comes from is given with it. See
 * docs/fntech3/2026-09-24-alice-menus.md. */

#include "alice_urc.hpp"
#include "alice_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace fntech3 {
namespace alice {
namespace menu {

/* ---- colours: UColorHSV (0x004cdad0, 0x004cd950), UBorderColor (0x004bc700) */

struct Rgba {
	float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
};

struct Hsv {
	float h = 0.0f, s = 0.0f, v = 0.0f, a = 0.0f;
};

/* Both conversions treat a saturation within this of zero as grey (the double
 * at 0x005007f8). */
inline constexpr double GreySaturation = 0.035;

/* RGB to HSV (0x004cdad0): hue in degrees, -1 for a grey. */
inline Hsv RgbToHsv( const Rgba &c ) {
	Hsv out;
	out.a = c.a;
	const float maxValue = c.r > c.g ? ( c.r > c.b ? c.r : c.b ) : ( c.g > c.b ? c.g : c.b );
	const float minValue = c.r < c.g ? ( c.r < c.b ? c.r : c.b ) : ( c.g < c.b ? c.g : c.b );
	out.v = maxValue;
	out.s = maxValue != 0.0f ? ( maxValue - minValue ) / maxValue : 0.0f;
	if ( std::fabs( out.s ) <= GreySaturation ) {
		out.h = -1.0f;
		return out;
	}
	const float delta = maxValue - minValue;
	if ( c.r == maxValue ) out.h = ( c.g - c.b ) / delta;
	else if ( c.g == maxValue ) out.h = ( c.b - c.r ) / delta + 2.0f;
	else out.h = ( c.r - c.g ) / delta + 4.0f;
	out.h *= 60.0f;
	if ( out.h < 0.0f ) out.h += 360.0f;
	return out;
}

/* HSV to RGB (0x004cd950): six sectors of sixty degrees. */
inline Rgba HsvToRgb( const Hsv &c ) {
	Rgba out;
	out.a = c.a;
	if ( std::fabs( c.s ) <= GreySaturation ) {
		out.r = out.g = out.b = c.v;
		return out;
	}
	const float h = ( c.h == 360.0f ? 0.0f : c.h ) * ( 1.0f / 60.0f );
	const int sector = static_cast<int>( h );
	const float f = h - static_cast<float>( sector );
	const float p = ( 1.0f - c.s ) * c.v;
	const float q = ( 1.0f - f * c.s ) * c.v;
	const float t = ( 1.0f - ( 1.0f - f ) * c.s ) * c.v;
	switch ( sector ) {
	case 0: out.r = c.v; out.g = t; out.b = p; break;
	case 1: out.r = q; out.g = c.v; out.b = p; break;
	case 2: out.r = p; out.g = c.v; out.b = t; break;
	case 3: out.r = p; out.g = q; out.b = c.v; break;
	case 4: out.r = t; out.g = p; out.b = c.v; break;
	case 5: out.r = c.v; out.g = p; out.b = q; break;
	default: break;   // retail leaves the colour as it was
	}
	return out;
}

/* A widget's border colours, made from its background colour whenever
 * `bgcolor` sets it (setBackgroundColor 0x004bc700): two darker shades and a
 * lighter, less saturated one. */
struct BorderColor {
	Rgba dark, reallyDark, light, original;
};

inline BorderColor MakeBorderColor( const Rgba &c ) {
	BorderColor out;
	const float twoThirds = 0.6666667f;   // 0x00500598
	const float oneThird = 0.3333333f;    // 0x00500594
	out.dark = { c.r * twoThirds, c.g * twoThirds, c.b * twoThirds, c.a };
	out.reallyDark = { c.r * oneThird, c.g * oneThird, c.b * oneThird, c.a };
	Hsv hsv = RgbToHsv( c );
	hsv.s *= 0.75f;          // 0x0050036c
	hsv.v *= 1.3333334f;     // 0x00500368
	out.light = HsvToRgb( hsv );
	out.light.a = c.a;
	out.original = c;
	return out;
}

enum class Shade { Dark, ReallyDark, Light };

struct BoxEdge {
	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
	Shade shade = Shade::Dark;
};

/* Draw3DBox (0x004b5b00): six one-unit strips, in the widget's own units. An
 * indented box is dark along the top and left with a really dark inner line
 * and light along the bottom and right; a raised box is light along the top
 * and left and dark, then really dark, along the bottom and right. */
inline void Box3DEdges( float x, float y, float w, float h, bool indent, BoxEdge ( &out )[6] ) {
	if ( indent ) {
		out[0] = { x, y, 1.0f, h - 1.0f, Shade::Dark };
		out[1] = { x + 1.0f, y + 1.0f, 1.0f, h - 3.0f, Shade::ReallyDark };
		out[2] = { x, y, w - 1.0f, 1.0f, Shade::Dark };
		out[3] = { x + 1.0f, y + 1.0f, w - 3.0f, 1.0f, Shade::ReallyDark };
		out[4] = { x + w - 1.0f, y, 1.0f, h, Shade::Light };
		out[5] = { x, y + h - 1.0f, w, 1.0f, Shade::Light };
	} else {
		out[0] = { x + w - 2.0f, y + 1.0f, 1.0f, h - 2.0f, Shade::Dark };
		out[1] = { x + w - 1.0f, y, 1.0f, h, Shade::ReallyDark };
		out[2] = { x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f, Shade::Dark };
		out[3] = { x, y + h - 1.0f, w, 1.0f, Shade::ReallyDark };
		out[4] = { x, y, 1.0f, h - 1.0f, Shade::Light };
		out[5] = { x, y, w - 1.0f, 1.0f, Shade::Light };
	}
}

inline const Rgba &ShadeColor( const BorderColor &border, Shade shade ) {
	return shade == Shade::Dark ? border.dark : shade == Shade::ReallyDark ? border.reallyDark : border.light;
}

/* ---- UICheckBox (constructor 0x004e0200) --------------------------------- */

/* The box is 16 units square, centred vertically, at an indent the
 * constructor sets, and the title follows it at twice that indent past the
 * box (Draw 0x004e0af0). Alice's indent is 0 (0x004e0290); F.A.K.K.2's is 16
 * (fakk2.exe 1.02 0x004bae69..0x004bae7a, Draw 0x004bb6e0..0x004bb84b), so
 * its box sits 16 units in and its title 48. */
inline constexpr float CheckSize = 16.0f;
inline constexpr float Fakk2CheckIndent = 16.0f;
inline constexpr float CheckTitleX( float indent ) { return indent * 2.0f + CheckSize; }

/* The cvar a checkbox writes (0x004e05e0 through 0x004c1900) and the text its
 * stuff command gets after it (" 1\n" 0x0052cf4c, " 0\n" 0x0052cf48). */
inline const char *CheckCommandSuffix( bool checked ) { return checked ? " 1\n" : " 0\n"; }

/* ---- UISlider (constructor 0x004d9880) ----------------------------------- */

enum class SliderType { Float = 0, Integer = 1 };

struct Slider {
	float min = 0.0f, max = 100.0f;   // setrange 0x004da530
	float value = 0.0f;
	float step = 1.0f;                // stepsize 0x004da5f0
	SliderType type = SliderType::Integer;   // slidertype 0x004da660
	int arrowWidth = 12;
	float thumbWidth = 12.0f;
};

/* `slidertype`: "float" and "integer" (case aside); anything else, such as the
 * "int" zound.urc writes, leaves the type as it was. */
inline void SetSliderType( Slider &slider, const std::string &word ) {
	if ( urc::EqualsNoCase( word, "float" ) ) slider.type = SliderType::Float;
	else if ( urc::EqualsNoCase( word, "integer" ) ) slider.type = SliderType::Integer;
}

/* The thumb's travel (FrameInitialized 0x004da800): whole units from the
 * first arrow plus half a thumb to the last arrow less half a thumb, measured
 * from the widget's left edge in the menu's units and truncated. */
struct SliderTrack {
	int start = 0, end = 0;
};

inline SliderTrack TrackFor( const Slider &slider, float frameX, float width ) {
	const float inset = static_cast<float>( slider.arrowWidth ) + slider.thumbWidth * 0.5f;
	SliderTrack track;
	track.start = static_cast<int>( frameX + inset );
	track.end = static_cast<int>( width + frameX - inset );
	return track;
}

/* How far along the value is, 0 to 1 (Draw 0x004db483..0x004db4c9). */
inline float SliderFraction( const Slider &slider ) {
	const float range = slider.max - slider.min;
	const float offset = slider.value - slider.min;
	if ( range == 0.0f ) return offset > 0.0f ? 1.0f : 0.0f;
	const float frac = offset / range;
	return frac > 1.0f ? 1.0f : frac < 0.0f ? 0.0f : frac;
}

/* The thumb's centre from the widget's left edge (Draw 0x004db4c9..0x004db4fa). */
inline float ThumbCenter( const Slider &slider, const SliderTrack &track ) {
	return static_cast<float>( track.end - track.start ) * SliderFraction( slider )
		+ slider.thumbWidth * 0.5f + static_cast<float>( slider.arrowWidth );
}

/* The value a click or drag at `x` sets (0x004db051..0x004db101). */
inline float ValueAt( const Slider &slider, const SliderTrack &track, float x ) {
	if ( !( x > static_cast<float>( track.start ) ) ) return slider.min;
	if ( !( static_cast<float>( track.end ) > x ) ) return slider.max;
	return ( x - static_cast<float>( track.start ) ) / static_cast<float>( track.end - track.start )
		* ( slider.max - slider.min ) + slider.min;
}

/* One arrow press (0x004da8b0 right, 0x004da940 left): the step is added or
 * taken away and the value held to the range -- from min up to max for a
 * positive step, and for a zero or negative step, which a reversed range such
 * as r_picmip's `setrange 3 0` uses, from max up to min. A negative step on an
 * ordinary range therefore always lands on max: controls.urc's camera
 * distance slider (`setrange 76 192`, `stepsize -1`) does that in retail. */
inline float Stepped( const Slider &slider, int direction ) {
	float value = direction > 0 ? slider.value + slider.step : slider.value - slider.step;
	if ( slider.step > 0.0f ) {
		if ( value < slider.min ) value = slider.min;
		if ( value > slider.max ) value = slider.max;
	} else {
		if ( value > slider.min ) value = slider.min;
		if ( value < slider.max ) value = slider.max;
	}
	return value;
}

/* Heavy Metal: F.A.K.K.2's arrows (fakk2.exe 1.02 0x004b5a20 up, 0x004b5a70
 * down) hold the value to min and then to max whatever the step's sign, so a
 * reversed range always lands on max. */
inline float SteppedFakk2( const Slider &slider, int direction ) {
	float value = direction > 0 ? slider.value + slider.step : slider.value - slider.step;
	if ( value < slider.min ) value = slider.min;
	if ( value > slider.max ) value = slider.max;
	return value;
}

/* The cvar's text (UpdateData 0x004db120: 0x004c1900 "%d", 0x004c1930 "%f"). */
inline std::string SliderCvarText( const Slider &slider ) {
	char text[64];
	if ( slider.type == SliderType::Integer ) {
		std::snprintf( text, sizeof( text ), "%d", static_cast<int>( slider.value ) );
	} else {
		std::snprintf( text, sizeof( text ), "%f", static_cast<double>( slider.value ) );
	}
	return text;
}

/* The value the stuff command is given, "%d" or "%.3f" (0x004db1e5, 0x004db282). */
inline std::string SliderCommandValue( const Slider &slider ) {
	char text[64];
	if ( slider.type == SliderType::Integer ) {
		std::snprintf( text, sizeof( text ), "%d", static_cast<int>( slider.value ) );
	} else {
		std::snprintf( text, sizeof( text ), "%.3f", static_cast<double>( slider.value ) );
	}
	return text;
}

/* ---- UIList (constructor 0x004de400) -------------------------------------- */

/* `additem <value> [text]` (0x004de170): the text is shown, the value written. */
struct ListItem {
	std::string value, text;
};

inline constexpr float ListArrowWidth = 15.0f;   // 0x004de455

/* The item whose value is the cvar's text exactly, 1-based, or 0
 * (UpdateUIElement 0x004df0e0 compares with strcmp, 0x0044fcb0). */
inline int ListFind( const std::vector<ListItem> &items, const char *value ) {
	if ( !value ) return 0;
	for ( std::size_t i = 0; i < items.size(); ++i ) {
		if ( items[i].value == value ) return static_cast<int>( i ) + 1;
	}
	return 0;
}

/* Draw 0x004df217..0x004df367: the text if there is one, else the value. */
inline const std::string &ListShows( const ListItem &item ) {
	return item.text.empty() ? item.value : item.text;
}

/* The arrows stop at the ends (0x004dec80, 0x004decd0); a click between them
 * goes round (0x004deca0). */
inline int ListNext( int current, int count ) { return current + 1 > count ? count : current + 1; }
inline int ListPrevious( int current ) { return current - 1 > 0 ? current - 1 : 1; }
inline int ListCycle( int current, int count ) { return current + 1 > count ? 1 : current + 1; }

/* ---- UIField (constructor 0x004df730) ------------------------------------- */

inline constexpr int FieldMaxLength = 255;   // 0x004dfb76: a full buffer refuses more

struct EditLine {
	std::string text;
	int cursor = 0;

	void Clamp() {
		if ( cursor < 0 ) cursor = 0;
		if ( cursor > static_cast<int>( text.size() ) ) cursor = static_cast<int>( text.size() );
	}
	/* A printable character at the cursor (CharEvent 0x004dfb5f). */
	bool Insert( char c ) {
		Clamp();
		if ( static_cast<unsigned char>( c ) < 0x20 || static_cast<int>( text.size() ) >= FieldMaxLength ) return false;
		text.insert( text.begin() + cursor, c );
		++cursor;
		return true;
	}
	/* Backspace (0x004dface). */
	bool Backspace() {
		Clamp();
		if ( cursor <= 0 ) return false;
		text.erase( text.begin() + ( cursor - 1 ) );
		--cursor;
		return true;
	}
	/* Delete (KeyEvent 0x004dfc8b). */
	bool Delete() {
		Clamp();
		if ( cursor >= static_cast<int>( text.size() ) ) return false;
		text.erase( text.begin() + cursor );
		return true;
	}
	void Left() { if ( cursor > 0 ) --cursor; }
	void Right() { if ( cursor < static_cast<int>( text.size() ) ) ++cursor; }
	void Home() { cursor = 0; }
	void End() { cursor = static_cast<int>( text.size() ); }
};

/* The first character a field draws (Draw 0x004df7c0..0x004df903), given the
 * font's widths: all of it when it fits within the widget less the indent;
 * otherwise from the cursor when what is before the cursor is narrower than the
 * overflow; otherwise the fewest leading characters that, dropped, let the rest
 * and a cursor at its end fit. */
template <typename StringWidth>
inline int FieldFirstVisible( const std::string &text, int cursor, float indent, float width,
	float cursorWidth, StringWidth stringWidth ) {
	const float full = static_cast<float>( stringWidth( text.c_str() ) ) + indent;
	if ( !( full > width ) ) return 0;
	const float beforeCursor = static_cast<float>( stringWidth( text.substr( 0, static_cast<std::size_t>( cursor ) ).c_str() ) );
	if ( full - width > beforeCursor ) return cursor;
	const float extra = cursor == static_cast<int>( text.size() ) ? cursorWidth : 0.0f;
	int start = 0;
	while ( start < static_cast<int>( text.size() )
		&& static_cast<float>( stringWidth( text.c_str() + start ) ) + extra + indent > width ) {
		++start;
	}
	return start;
}

/* ---- UIFakkBindList (bind.scr through bindlistener 0x00412f10) ----------- */

struct BindItem {
	std::string name, command, anim, shader;
	bool loop = false;
	float rotation = 0.0f;
	/* Heavy Metal: F.A.K.K.2 reads the third word as the row's picture
	 * (fakk2.exe 1.02 binditem 0x00411c90), where Alice reads an animation. */
	std::string material;
};

struct BindLayout {
	/* The bind_* numbers (0x00415960..0x00415cd0), in retail's order. */
	int width = 0, height = 0, fillWidth = 0;
	int commandWidth = 0, commandHeight = 0;
	int primaryKeyWidth = 0, primaryKeyHeight = 0;
	int alternateKeyWidth = 0, alternateKeyHeight = 0;
	Rgba titleForeground, titleBackground;
	Rgba activeForeground, activeBackground, activeBorder;
	Rgba inactiveForeground, inactiveBackground;
	Rgba highlightForeground, highlightBackground;
	Rgba selectForeground, selectBackground;
	std::string changeSound, activeSound, enterSound;
	std::string headers[3];
	/* F.A.K.K.2's pictures: each header's third word (0x00410580) and the fill
	 * strips' second (bind_fillwidth 0x00411f50). */
	std::string headerMaterials[3];
	std::string fillMaterial;
	std::vector<BindItem> items;
};

namespace detail {

inline Rgba ReadRgba( const std::vector<std::string> &args ) {
	Rgba c;
	float *out[4] = { &c.r, &c.g, &c.b, &c.a };
	for ( std::size_t i = 0; i < 4 && i < args.size(); ++i ) *out[i] = static_cast<float>( std::atof( args[i].c_str() ) );
	return c;
}

} // namespace detail

/* bind.scr is read with the same tokenizer as the .urc files, one keyword and
 * its line per event (0x004160a0); an unknown keyword is passed over. */
inline BindLayout ParseBindScript( const char *text, std::size_t length ) {
	BindLayout layout;
	if ( !text ) return layout;
	urc::detail::Tokenizer tokens( text, length );
	urc::Token keyword;
	while ( tokens.Next( keyword ) ) {
		std::vector<std::string> args;
		urc::detail::ReadRestOfLine( tokens, args );
		const auto is = [&keyword]( const char *name ) { return urc::EqualsNoCase( keyword.text, name ); };
		const auto number = [&args]() { return args.empty() ? 0 : std::atoi( args[0].c_str() ); };
		if ( is( "bind_width" ) ) layout.width = number();
		else if ( is( "bind_height" ) ) layout.height = number();
		else if ( is( "bind_fillwidth" ) ) {
			layout.fillWidth = number();
			if ( args.size() > 1 ) layout.fillMaterial = args[1];
		}
		else if ( is( "bind_commandwidth" ) ) layout.commandWidth = number();
		else if ( is( "bind_commandheight" ) ) layout.commandHeight = number();
		else if ( is( "bind_primarykeywidth" ) ) layout.primaryKeyWidth = number();
		else if ( is( "bind_primarykeyheight" ) ) layout.primaryKeyHeight = number();
		else if ( is( "bind_alternatekeywidth" ) ) layout.alternateKeyWidth = number();
		else if ( is( "bind_alternatekeyheight" ) ) layout.alternateKeyHeight = number();
		else if ( is( "titleforegroundcolor" ) ) layout.titleForeground = detail::ReadRgba( args );
		else if ( is( "titlebackgroundcolor" ) ) layout.titleBackground = detail::ReadRgba( args );
		else if ( is( "activeforegroundcolor" ) ) layout.activeForeground = detail::ReadRgba( args );
		else if ( is( "activebackgroundcolor" ) ) layout.activeBackground = detail::ReadRgba( args );
		else if ( is( "activebordercolor" ) ) layout.activeBorder = detail::ReadRgba( args );
		else if ( is( "inactiveforegroundcolor" ) ) layout.inactiveForeground = detail::ReadRgba( args );
		else if ( is( "inactivebackgroundcolor" ) ) layout.inactiveBackground = detail::ReadRgba( args );
		else if ( is( "highlightforegroundcolor" ) ) layout.highlightForeground = detail::ReadRgba( args );
		else if ( is( "highlightbackgroundcolor" ) ) layout.highlightBackground = detail::ReadRgba( args );
		else if ( is( "selectforegroundcolor" ) ) layout.selectForeground = detail::ReadRgba( args );
		else if ( is( "selectbackgroundcolor" ) ) layout.selectBackground = detail::ReadRgba( args );
		else if ( is( "changesound" ) ) { if ( !args.empty() ) layout.changeSound = args[0]; }
		else if ( is( "activesound" ) ) { if ( !args.empty() ) layout.activeSound = args[0]; }
		else if ( is( "entersound" ) ) { if ( !args.empty() ) layout.enterSound = args[0]; }
		else if ( is( "header" ) ) {
			// header <column 1..3> "<text>" (0x00413e50); retail's file has them commented out.
			if ( args.size() >= 2 ) {
				const int column = std::atoi( args[0].c_str() );
				if ( column >= 1 && column <= 3 ) {
					layout.headers[column - 1] = args[1];
					if ( args.size() > 2 ) layout.headerMaterials[column - 1] = args[2];
				}
			}
		} else if ( is( "binditem" ) ) {
			// binditem "<name>" <command> [anim [looped [rotation [shader]]]] (0x004155f0).
			if ( args.size() < 2 ) continue;
			BindItem item;
			item.name = args[0];
			item.command = args[1];
			if ( args.size() > 2 ) item.anim = item.material = args[2];
			if ( args.size() > 3 ) item.loop = urc::EqualsNoCase( args[3], "true" );
			if ( args.size() > 4 ) item.rotation = static_cast<float>( std::atof( args[4].c_str() ) );
			if ( args.size() > 5 ) item.shader = args[5];
			layout.items.push_back( std::move( item ) );
		}
	}
	return layout;
}

/* The rows the list shows below its header (Draw 0x004e4960..0x004e5098): the
 * scrollbar's position is the first, and a row whose top would reach the list's
 * bottom is hidden. */
inline int VisibleRowTop( int row, int top, int commandHeight ) {
	return ( row - top ) * commandHeight + commandHeight;
}

inline bool RowVisible( int row, int top, int commandHeight, float listHeight ) {
	return row >= top && static_cast<float>( VisibleRowTop( row, top, commandHeight ) ) < listHeight;
}

/* The scrollbar's page (filename 0x004e3a59..0x004e3a8c): whole rows below
 * the header. */
inline int BindPageSize( float listHeight, int commandHeight ) {
	if ( commandHeight <= 0 ) return 0;
	return static_cast<int>( ( listHeight - static_cast<float>( commandHeight ) ) / static_cast<float>( commandHeight ) );
}

/* UIVertScroll's position is held within 0 and the rows past a page
 * (0x004d8b70). */
inline int ClampScroll( int value, int count, int page ) {
	const int most = count - page > 0 ? count - page : 0;
	return value < 0 ? 0 : value > most ? most : value;
}

/* keybind.scr (UI_LoadTransTable 0x004d1810): `"KEY" "Display" scale`
 * triples, at most 128, read with COM_Parse. A scale of -1 keeps the button's
 * own. */
struct KeyAlias {
	std::string key, name;
	float scale = -1.0f;
};

inline constexpr int MaxKeyAliases = 128;

inline std::vector<KeyAlias> ParseKeyAliases( const char *text, std::size_t length, std::string *error = nullptr ) {
	std::vector<KeyAlias> aliases;
	if ( !text ) return aliases;
	const char *cursor = text;
	const char *end = text + length;
	for ( ;; ) {
		const std::string key = screen::detail::NextToken( cursor, end );
		if ( key.empty() ) break;
		if ( static_cast<int>( aliases.size() ) >= MaxKeyAliases ) {
			if ( error ) *error = "UI_LoadTransTable: Too many keybind translations.";
			break;
		}
		KeyAlias alias;
		alias.key = key;
		alias.name = screen::detail::NextToken( cursor, end );
		if ( alias.name.empty() ) {
			if ( error ) *error = "UI_LoadTransTable: Missing translated name.";
			break;
		}
		const std::string scale = screen::detail::NextToken( cursor, end );
		if ( scale.empty() ) {
			if ( error ) *error = "UI_LoadTransTable: Missing scale param.";
			break;
		}
		alias.scale = static_cast<float>( std::atof( scale.c_str() ) );
		aliases.push_back( std::move( alias ) );
	}
	return aliases;
}

/* The alias for a key's name, compared without regard to case (0x004d1f1d,
 * _stricmp 0x004e8760), or null. */
inline const KeyAlias *FindKeyAlias( const std::vector<KeyAlias> &aliases, const char *keyName ) {
	if ( !keyName ) return nullptr;
	for ( const KeyAlias &alias : aliases ) {
		if ( urc::EqualsNoCase( alias.key, keyName ) ) return &alias;
	}
	return nullptr;
}

/* ---- UIFAKKLoadGameClass (constructor 0x00419dc0) ------------------------ */

/* One row of the list the refresh builds (0x0041a210) from a `.ssv` header. */
struct SaveRow {
	std::string map;       // header +0x48, the list's first column in Alice
	std::string comment;   // header +0x08, F.A.K.K.2's first column (the level's title)
	std::string name;      // header +0x88, the archive name loadgame is given
	int32_t saveTime = 0;  // header +0x00, time_t, the column the list sorts by
	int32_t levelTime = 0; // header +0x04, milliseconds
};

/* `quicksave` is left out, compared without regard to case (0x0041a68f,
 * _stricmp 0x004e8760). */
inline bool ListsSave( const char *name ) {
	return name && !urc::EqualsNoCase( name, "quicksave" );
}

/* The list sorts by the save's time, as a number, oldest first (0x0041a7ac,
 * the item's key 0x0041a980 is atol of the column; the column was added
 * numeric and not reversed at 0x0041a1cd). */
inline void SortSaves( std::vector<SaveRow> &rows ) {
	std::stable_sort( rows.begin(), rows.end(), []( const SaveRow &a, const SaveRow &b ) {
		return a.saveTime < b.saveTime;
	} );
}

inline constexpr int ShotsPerPage = 6;

/* The page and the selected picture on it (0x0041b0c2..0x0041b10a and the
 * page commands 0x0041be30, 0x0041be70, 0x0041bde0). */
struct SavePage {
	int page = 0;
	int tab = -1;
	int lastCount = -1;

	/* A list whose length changed shows its last save. */
	void Refreshed( int count ) {
		if ( lastCount == count ) return;
		if ( count > 0 ) {
			page = ( count - 1 ) / ShotsPerPage;
			tab = ( count - 1 ) % ShotsPerPage + 1;
		}
		lastCount = count;
	}
	/* The save shown at picture `slot` (1..6), 1-based. */
	int IndexAt( int slot ) const { return page * ShotsPerPage + slot; }
	int Selected() const { return IndexAt( tab ); }
	bool Next( int count ) {
		if ( page * ShotsPerPage + ShotsPerPage + 1 > count ) return false;
		++page;
		return true;
	}
	bool Previous() {
		if ( page <= 0 ) return false;
		--page;
		return true;
	}
	bool Select( int slot, int count ) {
		if ( count <= 0 || IndexAt( slot ) > count ) return false;
		tab = slot;
		return true;
	}
};

/* ctime() without its newline (the list's date column, 0x0041adc6), from the
 * broken-down local time the caller made. */
inline std::string CTimeText( const std::tm &t ) {
	static const char *const days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char *const months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
		"Nov", "Dec" };
	char text[64];
	std::snprintf( text, sizeof( text ), "%.3s %.3s%3d %.2d:%.2d:%.2d %d", days[( t.tm_wday % 7 + 7 ) % 7],
		months[( t.tm_mon % 12 + 12 ) % 12], t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, 1900 + t.tm_year );
	return text;
}

/* The list control's own drawing of the saves (UIListCtrl::Draw 0x004ce3a0:
 * the headers 0x004cde00, the rows 0x004ce010), which F.A.K.K.2's load menu
 * shows and Alice's hides behind its pictures (`neverdraw`). Its columns
 * (0x0041a150) are the map, the level's time and the save's date, over 40,
 * 20 and 40 per cent of the list's width (0x004ff884, 0x004ff880), each cut
 * to a whole unit. The second title is retail's own spelling. */
inline constexpr int SaveColumns = 3;
inline constexpr const char *SaveColumnTitles[SaveColumns] = { "Location", "Elapased Time", "Date & Time" };

inline int SaveColumnWidth( int column, float listWidth ) {
	return static_cast<int>( listWidth * ( column == 1 ? 0.2f : 0.4f ) );
}

/* The header: a line with 2 units above and below it (0x004cde20..0x004cde2e,
 * the padding at 0x004cdd3a). A row is one line. */
inline int SaveHeaderHeight( int lineHeight ) { return lineHeight + 4; }

/* The level's time (0x0041aa3a): whole seconds -- the refresh divides the
 * header's milliseconds by 1,000 (0x0041a5b0) -- as hours, then minutes and
 * seconds of two digits. */
inline std::string ElapsedText( int32_t levelTime ) {
	const int seconds = levelTime > 0 ? static_cast<int>( levelTime / 1000 ) : 0;
	char text[32];
	std::snprintf( text, sizeof( text ), "%d:%02d:%02d", seconds / 3600, seconds % 3600 / 60, seconds % 60 );
	return text;
}

/* The rows below the header that fit whole: a page for the scrollbar and the
 * page keys. Retail draws one more while its top is inside the list
 * (0x004ce179), cut by the list's edge. */
inline int SaveRowsPerPage( float listHeight, int lineHeight ) {
	if ( lineHeight <= 0 ) return 1;
	const int rows = static_cast<int>( ( listHeight - static_cast<float>( SaveHeaderHeight( lineHeight ) ) )
		/ static_cast<float>( lineHeight ) );
	return rows > 1 ? rows : 1;
}

/* The first row shown, moved only as far as keeps the chosen row (1-based)
 * on the page. */
inline int SaveTopFor( int current, int top, int page, int count ) {
	const int maxTop = count > page ? count - page : 0;
	if ( current > 0 ) {
		if ( current - 1 < top ) top = current - 1;
		if ( current - 1 >= top + page ) top = current - page;
	}
	if ( top > maxTop ) top = maxTop;
	return top < 0 ? 0 : top;
}

/* The page and picture a list row stands for, so the list's choice is the
 * one `loadgame` and `removegame` read (0x0041bcb0 takes the page's). */
inline void SelectSaveRow( SavePage &page, int row ) {
	if ( row <= 0 ) return;
	page.page = ( row - 1 ) / ShotsPerPage;
	page.tab = ( row - 1 ) % ShotsPerPage + 1;
}

/* ---- UIAutoScroll (constructor 0x004c0340) -------------------------------- */

inline constexpr float DefaultScrollSpeed = 0.2f;   // 0x004c0393

/* `addimage <shader> <x> <line> <w> <h>` (0x004c0700): placed a number of
 * lines below the lines already added, and faded at the edges only when some
 * line was added before it. */
struct ScrollImage {
	std::string shader;
	float x = 0.0f, line = 0.0f, w = 0.0f, h = 0.0f;
	bool fade = false;
};

/* The fade at the widget's edges over 40 units (Draw 0x004c0b23..0x004c0bbd,
 * 0x004c0c6d..0x004c0d1c; 0.025 is 0x00500634): a line or picture wholly
 * inside shows at full strength, one crossing the top or the bottom fades,
 * and the bottom's rule overrides the top's. */
inline float EdgeAlpha( float top, float bottom, float height ) {
	float alpha = 1.0f;
	const float fromTop = top * 0.025f;
	if ( fromTop < 0.0f ) alpha = 0.0f;
	else if ( fromTop < 1.0f ) alpha = fromTop;
	const float fromBottom = ( height - bottom ) * 0.025f;
	if ( fromBottom < 0.0f ) alpha = 0.0f;
	else if ( fromBottom < 1.0f ) alpha = fromBottom;
	return alpha;
}

/* `resetscroll [line]` (0x004c0490): the given line at the top, or the first. */
inline float ResetScrollPosition( float line, float lineHeight ) {
	return -( lineHeight * line );
}

/* ---- an uncleared screen, as shares -------------------------------------- */

/* Heavy Metal: F.A.K.K.2's screen is not cleared under its full-screen menus
 * (cl_alice_ui.cpp's MenuCarry), so what it shows is kept as shares of the
 * layers drawn into it: a draw at alpha a leaves every share 1 - a of itself
 * and adds a to its own layer's. `kind` and `menu` name a layer; the caller
 * gives them meaning. */
struct KeptLayer {
	int kind = 0;
	int menu = -1;
	float share = 0.0f;
};

inline void KeepDraw( std::vector<KeptLayer> &layers, int kind, int menu, float a ) {
	if ( !( a > 0.0f ) ) return;
	if ( a > 1.0f ) a = 1.0f;
	bool found = false;
	for ( KeptLayer &layer : layers ) {
		layer.share *= 1.0f - a;
		if ( layer.kind == kind && layer.menu == menu ) {
			layer.share += a;
			found = true;
		}
	}
	if ( !found ) {
		KeptLayer layer;
		layer.kind = kind;
		layer.menu = menu;
		layer.share = a;
		layers.push_back( layer );
	}
}

/* Shares too faint to see (under 1/512) are dropped. */
inline void DropFaintLayers( std::vector<KeptLayer> &layers ) {
	layers.erase( std::remove_if( layers.begin(), layers.end(),
		[]( const KeptLayer &layer ) { return layer.share < 1.0f / 512.0f; } ), layers.end() );
}

/* The alpha each layer is drawn at, from the first up, to leave it its share:
 * from the last down, its share of what the layers after it let through. */
inline std::vector<float> KeptAlphas( const std::vector<KeptLayer> &layers ) {
	std::vector<float> alpha( layers.size(), 0.0f );
	float through = 1.0f;
	for ( std::size_t i = layers.size(); i-- > 0; ) {
		const float share = layers[i].share;
		alpha[i] = through > 0.0f ? std::min( share / through, 1.0f ) : 0.0f;
		through -= share;
	}
	return alpha;
}

} // namespace menu
} // namespace alice
} // namespace fntech3

#endif
