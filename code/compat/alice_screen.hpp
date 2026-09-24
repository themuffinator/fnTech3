/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_SCREEN_HPP
#define FNTECH3_COMPAT_ALICE_SCREEN_HPP

/* American McGee's Alice's engine-drawn screen: the pure arithmetic and the
 * tables its engine keeps for everything drawn outside the client game's 3D
 * view -- the centre and located prints, the screen fade and letterbox, the
 * frame counter, the intro sequence and the world-map loading screen. The
 * engine side (cl_alice_screen.cpp, cl_alice_loading.cpp, cl_alice_ui.cpp)
 * draws with these; nothing here touches an engine.
 *
 * Every constant is read from retail alice.exe (image base 0x400000) at the
 * address beside it. */

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace fntech3 {
namespace alice {
namespace screen {

/* ---- centre and located prints (View3D 0x0041fe70, 0x00420180) ---------- */

/* Both setters arm a 4000 ms timer (SetCenterPrint 0x0041f560,
 * SetLocationPrint 0x0041f760). */
inline constexpr float PrintTimeMs = 4000.0f;
inline constexpr float PrintFadeMs = 750.0f;      /* 0x004ff9c4 */
inline constexpr float PrintHoldEndMs = 3250.0f;  /* 0x004ff9c8 */

/* The print's alpha while its timer reads `timer`: 750 ms in, a plateau at 1,
 * 750 ms out. The print's own alpha scales only the two ramps; the result is
 * clamped to 0..1 (0x004201ac-0x00420236). A timer at zero draws nothing. */
inline float PrintAlpha( float timer, float alpha ) {
	float a;
	if ( timer > PrintHoldEndMs ) {
		a = 1.0f - ( timer - PrintHoldEndMs ) * ( 1.0f / PrintFadeMs ) * alpha;
	} else if ( timer < PrintFadeMs ) {
		a = timer * ( 1.0f / PrintFadeMs ) * alpha;
	} else {
		a = 1.0f;
	}
	if ( a > 1.0f ) a = 1.0f;
	if ( a < 0.0f ) a = 0.0f;
	return a;
}

/* The timer counts down by the frame time each time the view's overlay is
 * drawn, and stops at zero. */
inline float AdvancePrintTimer( float timer, int frameMs ) {
	timer -= static_cast<float>( frameMs );
	return timer < 0.0f ? 0.0f : timer;
}

/* A located print's text origin in real pixels and its justification
 * (0x00420236-0x004202a3). x and y are 640x480 units; y measures up from the
 * bottom of the screen to the text's bottom edge. -1 on an axis centres the
 * text on that axis instead. The conversions truncate. */
struct LocatedText {
	int x = 0, y = 0;
	int horizontal = 1;  /* 0 centre, 1 left */
	int vertical = 0;    /* 0 top, 1 centre */
};

inline LocatedText LocatedTextOrigin( int x, int y, int fontHeight, float width, float height ) {
	LocatedText out;
	out.x = static_cast<int>( x * ( 1.0f / 640.0f ) * width );
	out.y = static_cast<int>( ( 480 - y - fontHeight ) * ( 1.0f / 480.0f ) * height );
	if ( x == -1 ) {
		out.horizontal = 0;
		out.x = 0;
	}
	if ( y == -1 ) {
		out.vertical = 1;
		out.y = 0;
	}
	return out;
}

/* ---- the view's fade and letterbox (0x00420620, 0x00420570) ------------- */

/* stats[6] selects how the player-state blend colour covers the view. */
enum class Fade { Blend = 0, Additive = 1, Drug = 2, Ice = 3, None = 4 };

inline Fade FadeKind( int stat ) {
	return stat >= 0 && stat <= 3 ? static_cast<Fade>( stat ) : Fade::None;
}

/* stats[7] is the letterbox fraction scaled by 32767 (0x004ff9cc). A fraction
 * at or below zero draws nothing; above one is not clamped. */
inline float LetterboxFraction( int stat ) {
	return stat * 3.05185094759971922e-05f;
}

/* ---- frame counter (0x0041fb20) ----------------------------------------- */

inline constexpr float FpsGoodAbove = 23.94f;  /* 0x004ff9c0: white */
inline constexpr float FpsFairAbove = 18.0f;   /* 0x004ff9bc: yellow, else red */

/* ---- the HUD's show delay (UI_Update 0x0040c800) ------------------------- */

inline constexpr int HudShowDelayMs = 550;

/* ---- subtitles: AliceDialogText (0x00416c60..0x00417da0) ----------------- */

/* The box (AliceDialogText::Init 0x00417020, built by UI_Init at 384 by 64):
 * centred across 640, 10 units above 480's bottom. It is a desktop child, so
 * these are real pixels whatever the video size (the rescale rule at
 * 0x004bd777). Its text starts 40 in from its left (0x004178c7) and wraps at
 * its width less 88 (0x004170b9); a line is the font's height less 10 tall
 * (0x004177dd), and as many lines fit as that height divides the box's
 * (0x004170f0). */
struct DialogLayout {
	float x = 0, y = 0, w = 0, h = 0;
	int wrapWidth = 0;
	int lineHeight = 0;
	int visibleLines = 0;
	float textX = 40.0f;
};

inline DialogLayout DialogLayoutFor( float w, float h, int fontHeight ) {
	DialogLayout layout;
	layout.w = w;
	layout.h = h;
	layout.x = static_cast<float>( static_cast<int>( ( 640.0f - w ) * 0.5f ) );
	layout.y = static_cast<float>( static_cast<int>( 480.0f - h - 10.0f ) );
	layout.wrapWidth = static_cast<int>( w - 88.0f );
	layout.lineHeight = fontHeight - 10;
	layout.visibleLines = layout.lineHeight > 0 ? static_cast<int>( h / layout.lineHeight ) : 0;
	return layout;
}

/* The text colour UI_Init hands the box (0x00410b45, set as its foreground
 * through its vtable's +0x28 at 0x004bc970); its background is clear. */
inline constexpr float DialogTextColor[3] = { 0.1242f, 0.0984f, 0.0164f };
inline constexpr float DialogWidth = 384.0f;
inline constexpr float DialogHeight = 64.0f;
inline constexpr const char *DialogFont = "asrafel";
inline constexpr const char *DialogLeftFrame = "ui/dialog/leftFrame.tga";
inline constexpr const char *DialogRightFrame = "ui/dialog/rightFrame.tga";
/* The start time *resetdialog stores, which no draw reaches (0x00417c84). */
inline constexpr int DialogResetTime = -99999;

/* The timing the box starts with (0x00416daa-0x00416dbe) and *time resets:
 * a 200 ms fade in, text held until 5000 ms, a 1000 ms fade out. */
struct DialogTiming {
	float fadeIn = 200.0f;
	float fadeTime = 5000.0f;
	float fadeSpeed = 1000.0f;
	float time = 0.0f;
	int position = 0;
};

/* A `*` command (0x00417a80). Each is matched on its own length, so trailing
 * text after a name still matches it; values are seconds, stored as
 * milliseconds, and ignored unless positive. `*position` keeps 1 to 10 and
 * nothing in the box reads it. Returns what the command did to the text:
 * 0 nothing, 1 clear the lines, 2 reset the start time. */
inline int DialogCommand( const char *text, DialogTiming &timing ) {
	if ( !text ) return 0;
	if ( !std::strncmp( text, "*fadetime", 9 ) ) {
		const float v = static_cast<float>( std::atof( text + 10 ) * 1000.0 );
		if ( v > 0.0f ) timing.fadeTime = v;
		return 0;
	}
	if ( !std::strncmp( text, "*fadespeed", 10 ) ) {
		const float v = static_cast<float>( std::atof( text + 11 ) * 1000.0 );
		if ( v > 0.0f ) timing.fadeSpeed = v;
		return 0;
	}
	if ( !std::strncmp( text, "*fadeinspeed", 12 ) ) {
		const float v = static_cast<float>( std::atof( text + 13 ) * 1000.0 );
		if ( v > 0.0f ) timing.fadeIn = v;
		return 0;
	}
	if ( !std::strncmp( text, "*position", 9 ) ) {
		const int v = std::atoi( text + 10 );
		if ( v > 0 && v < 11 ) timing.position = v;
		return 0;
	}
	if ( !std::strncmp( text, "*cleartext", 10 ) ) return 1;
	if ( !std::strncmp( text, "*resetdialog", 12 ) ) return 2;
	if ( !std::strncmp( text, "*time", 5 ) ) {
		const float v = static_cast<float>( std::atof( text + 6 ) * 1000.0 );
		timing.fadeIn = 200.0f;
		timing.fadeSpeed = 1000.0f;
		if ( v > 0.0f ) {
			timing.time = v;
			timing.fadeTime = v + 200.0f;
		} else {
			timing.time = 0.0f;
			timing.fadeTime = 5000.0f;
		}
		return 0;
	}
	return 0;
}

/* Plain text into lines (0x0041716c-0x00417614): a first pass turns the space
 * at which each line overflows the wrap width into a newline, backing up to
 * the line's last space when the overflow falls inside a word; a second pass
 * cuts the text at its newlines and starts a new line wherever a line still
 * overflows. Widths are the font's unscaled character widths. The second
 * pass starts from the width the first ended with (0x004173bd adds to a sum
 * the first pass left, and only a newline clears it); the game ends every
 * line it sends with a newline (the format at fgamex86 0x10121215 is "%s"
 * and a newline), which clears that sum and leaves an empty last line. Both kept. `charWidth` answers
 * UIFont::charWidth (0x004c5860). */
template<typename CharWidth>
inline std::vector<std::string> DialogWrap( const char *source, int wrapWidth, CharWidth &&charWidth ) {
	std::vector<std::string> lines( 1 );
	if ( !source ) return lines;
	std::string text( source );
	int width = 0;
	int lastSpace = -1;
	for ( int i = 0; i < static_cast<int>( text.size() ); ++i ) {
		const char c = text[i];
		if ( c == ' ' ) {
			lastSpace = i;
			if ( width == 0 ) continue;
		}
		width += charWidth( c );
		if ( width > wrapWidth ) {
			width = 0;
			if ( text[i] == ' ' ) {
				text[i] = '\n';
			} else if ( lastSpace > -1 ) {
				i = lastSpace;
				text[i] = '\n';
				lastSpace = -1;
			}
		} else if ( text[i] == '\n' ) {
			width = 0;
			lastSpace = -1;
		}
	}
	for ( const char c : text ) {
		const int w = charWidth( c );
		width += w;
		if ( width > wrapWidth ) {
			lines.emplace_back( 1, c );
			width = w;
		} else if ( c == '\n' ) {
			lines.emplace_back();
			width = 0;
		} else {
			lines.back() += c;
		}
	}
	return lines;
}

/* The box's alpha `elapsed` ms after its text arrived (0x004176c3); a negative
 * answer means the text has faded and the box closes. */
inline float DialogAlpha( float elapsed, const DialogTiming &timing ) {
	if ( elapsed < timing.fadeIn ) return elapsed / timing.fadeIn;
	if ( elapsed < timing.fadeTime ) return 1.0f;
	if ( timing.fadeTime + timing.fadeSpeed > elapsed ) {
		return 1.0f - ( elapsed - timing.fadeTime ) / timing.fadeSpeed;
	}
	return -1.0f;
}

/* How many of the lines past the first screenful have scrolled in
 * (0x004178f5): the hold time spread evenly over them, one at the start. */
inline int DialogLinesShown( float elapsed, float fadeTime, int lineCount, int visibleLines ) {
	const double spread = static_cast<double>( elapsed ) / fadeTime * ( lineCount - visibleLines );
	return static_cast<int>( std::floor( spread ) + 1.0 );
}

/* ---- intro (_intro 0x0040fc70, plate 0x0040c620) ------------------------ */

enum class IntroAction {
	Nothing,     /* no step (a default IntroStep) */
	Soundtrack,  /* state -1, a map loaded: the soundtrack and state 0 */
	Black,       /* state 0 */
	PlateIn,     /* state 1: fade the legal plate in */
	PlateHold,   /* state 2 */
	PlateOut,    /* state 3 */
	Video,       /* states 4 and 5 */
	MainMenu,    /* states 6 and 100 */
	Skip,        /* states 7 to 99, and any outside -1..100 */
};

struct IntroStep {
	IntroAction action = IntroAction::Nothing;
	int nextState = -1;       /* the state stored before any wait */
	int waitMs = -1;          /* -1: nothing is queued */
	const char *video = nullptr;
	const char *videoFlags = nullptr; /* its cinematic flag: "25" or "26" */
	int videoResumeState = 0; /* the state that flag resumes at */
	float plateFrom = 0.0f, plateTo = 0.0f, plateMs = 0.0f;
};

/* The legal plate is drawn 1.8 times its image's size (0x005315fc). */
inline constexpr float IntroPlateScale = 1.8f;
inline constexpr const char *IntroPlateImage = "gfx/2d/legalplate.tga";
inline constexpr const char *IntroSoundtrack = "sound/music/intro";

/* One `_intro` step: what the state does, the state it leaves behind, and the
 * `wait` it queues before the next step. The jump table (0x0040fe20, indexed
 * by state + 1 through the bytes at 0x0040fe44) sends -1 into the main menu's
 * case after its push (0x0040fdd2): the intro's soundtrack starts and the
 * state becomes 0. A state outside -1..100 takes the default (0x0040fc85):
 * up by one, then "wait 0; _intro". A video's state goes up by one after
 * CIN_Play returns, from whatever it then is (0x0040fd73). */
inline IntroStep Intro( int state ) {
	IntroStep step;
	switch ( state ) {
	case -1:
		step.action = IntroAction::Soundtrack;
		step.nextState = 0;
		return step;
	case 0:
		step.action = IntroAction::Black;
		step.nextState = 1;
		step.waitMs = 5000;
		return step;
	case 1:
		step.action = IntroAction::PlateIn;
		step.nextState = 2;
		step.waitMs = 1000;
		step.plateFrom = 0.0f; step.plateTo = 1.0f; step.plateMs = 1000.0f;
		return step;
	case 2:
		step.action = IntroAction::PlateHold;
		step.nextState = 3;
		step.waitMs = 5000;
		step.plateFrom = 1.0f; step.plateTo = 1.0f; step.plateMs = 0.0f;
		return step;
	case 3:
		step.action = IntroAction::PlateOut;
		step.nextState = 4;
		step.waitMs = 1000;
		step.plateFrom = 1.0f; step.plateTo = 0.0f; step.plateMs = 1000.0f;
		return step;
	case 4:
		step.action = IntroAction::Video;
		step.nextState = 5;
		step.video = "ealogo.roq";         /* 0x0050c2a8 */
		step.videoFlags = "25";            /* 0x0050c2b4 */
		step.videoResumeState = 5;
		return step;
	case 5:
		step.action = IntroAction::Video;
		step.nextState = 6;
		step.video = "roguelogo.roq";      /* 0x0050c280 */
		step.videoFlags = "26";            /* 0x0050c290 */
		step.videoResumeState = 6;
		return step;
	case 6:
	case 100:
		step.action = IntroAction::MainMenu;
		step.nextState = 0;
		return step;
	default:
		step.action = IntroAction::Skip;
		// Retail's increment wraps; this one must not overflow.
		step.nextState = static_cast<int>( static_cast<unsigned>( state ) + 1u );
		step.waitMs = 0;
		return step;
	}
}

/* The legal plate's alpha: from + (to - from) * t over the fade length, with t
 * clamped to 0..1, and 1 when the length is zero (0x0040c648). */
inline float IntroPlateAlpha( float from, float to, float lengthMs, float elapsedMs ) {
	if ( lengthMs == 0.0f ) return 1.0f;
	float t = elapsedMs / lengthMs;
	if ( t < 0.0f ) t = 0.0f;
	if ( t > 1.0f ) t = 1.0f;
	return from + ( to - from ) * t;
}

/* A cinematic's flags, the '2' that resumes the intro among them, are read by
 * the cinematic player: code/client/client_alice_cinematic.hpp. */

/* ---- loading screen (0x0040c8c4 and its helpers) ------------------------- */

inline constexpr int LoadingStageMax = 9;

struct Tile {
	float x, y, w, h;
	const char *image;
};

/* The world map, six tiles over 640x480 (0x0040b250). */
inline constexpr Tile WorldMapTiles[6] = {
	{ 0.0f, 0.0f, 256.0f, 256.0f, "ui/worldmap/worldmapA" },
	{ 256.0f, 0.0f, 256.0f, 256.0f, "ui/worldmap/worldmapB" },
	{ 512.0f, 0.0f, 128.0f, 256.0f, "ui/worldmap/worldmapC" },
	{ 0.0f, 256.0f, 256.0f, 224.0f, "ui/worldmap/worldmapD" },
	{ 256.0f, 256.0f, 256.0f, 224.0f, "ui/worldmap/worldmapE" },
	{ 512.0f, 256.0f, 128.0f, 224.0f, "ui/worldmap/worldmapF" },
};

struct Zone {
	int x, y;
	const char *image;
};

/* Zone art, 128x128 each (table 0x0050b9f8, count 9 at 0x0050ba64). Every zone
 * is drawn at alpha 0.5, the current one opaque (0x0040b9d0). */
inline constexpr Zone Zones[9] = {
	{ 20, 318, "ui/worldmap/zone_1.tga" },
	{ 237, 354, "ui/worldmap/zone_2.tga" },
	{ 69, 225, "ui/worldmap/zone_3.tga" },
	{ 179, 188, "ui/worldmap/zone_4.tga" },
	{ 23, 135, "ui/worldmap/zone_5.tga" },
	{ 76, 1, "ui/worldmap/zone_6.tga" },
	{ 285, 219, "ui/worldmap/zone_7.tga" },
	{ 495, 243, "ui/worldmap/zone_8.tga" },
	{ 430, 40, "ui/worldmap/zone_9.tga" },
};
inline constexpr float ZoneSize = 128.0f;
inline constexpr float ZoneDimAlpha = 0.5f;

struct Region {
	int x, y, w, h;
	const char *image;
	int zone;
};

/* Region art (table 0x0050b8b8, count 13 at 0x0050b9f0): a region is drawn
 * while its zone lies beyond the current map's (0x0040bcb9). */
inline constexpr Region Regions[13] = {
	{ 107, 210, 256, 256, "ui/worldmap/region2_1.tga", 2 },
	{ 363, 210, 256, 256, "ui/worldmap/region2_2.tga", 2 },
	{ 97, 223, 128, 128, "ui/worldmap/region3.tga", 3 },
	{ 14, 207, 64, 128, "ui/worldmap/region4_1.tga", 4 },
	{ 78, 116, 256, 256, "ui/worldmap/region4_2.tga", 4 },
	{ 13, 36, 256, 256, "ui/worldmap/region5.tga", 5 },
	{ 13, 16, 256, 128, "ui/worldmap/region6_1.tga", 6 },
	{ 269, 16, 64, 128, "ui/worldmap/region6_2.tga", 6 },
	{ 269, 158, 256, 256, "ui/worldmap/region7.tga", 7 },
	{ 372, 210, 256, 256, "ui/worldmap/region8_1.tga", 8 },
	{ 372, 10, 256, 200, "ui/worldmap/region8_2.tga", 8 },
	{ 349, 16, 256, 256, "ui/worldmap/region9_1.tga", 9 },
	{ 222, 16, 128, 256, "ui/worldmap/region9_2.tga", 9 },
};

/* The progress indicator and its dots (0x0040be20, called with the rectangle
 * 370, 401, 256, 64): one 16x16 dot a stage, clamped to nine. */
inline constexpr float IndicatorRect[4] = { 370.0f, 401.0f, 256.0f, 64.0f };
inline constexpr float LoadingDotX[LoadingStageMax] = {
	426.0f, 444.0f, 462.0f, 479.0f, 496.0f, 514.0f, 532.0f, 550.0f, 567.0f,
};
inline constexpr float LoadingDotY = 418.0f;   /* 0x004ff494 */
inline constexpr float LoadingDotSize = 16.0f; /* 0x004ff498 */
inline constexpr const char *IndicatorImage = "ui/worldmap/indicator";
inline constexpr const char *IndicatorGlowImage = "ui/worldmap/indicator_glow";

/* The map's title, in asrafel at (400, 440), scaled by the maplist scale in
 * both axes with a character spacing of 1 (0x0040c19b-0x0040c1eb). */
inline constexpr float LoadingTitleX = 400.0f;
inline constexpr float LoadingTitleY = 440.0f;
inline constexpr float LoadingTitleColor[4] = { 0.1f, 0.04f, 0.04f, 1.0f };
inline constexpr const char *LoadingTitleFont = "asrafel";

inline int ClampLoadingStage( int stage ) {
	return stage > LoadingStageMax ? LoadingStageMax : stage;
}

/* ---- ui/maplist.scr (UI_LoadMapList 0x0040b5a0) -------------------------- */

struct MapListEntry {
	int zone = 0;
	float scale = 0.0f;
	std::string map;
	std::string title;
};

struct MapList {
	std::vector<MapListEntry> entries;
	std::string error;   /* the reason reading stopped early, if it did */

	/* The entry whose map name matches exactly, or -1 (0x0040b8e0: a
	 * case-sensitive compare). */
	int Find( const char *map ) const {
		if ( !map ) return -1;
		for ( std::size_t i = 0; i < entries.size(); ++i ) {
			if ( entries[i].map == map ) return static_cast<int>( i );
		}
		return -1;
	}
};

namespace detail {

/* COM_ParseExt with line breaks allowed: whitespace at or below a space,
 * `//` and block comments, quoted tokens. */
inline std::string NextToken( const char *&cursor, const char *end ) {
	for ( ;; ) {
		while ( cursor < end && static_cast<unsigned char>( *cursor ) <= ' ' ) ++cursor;
		if ( cursor + 1 < end && cursor[0] == '/' && cursor[1] == '/' ) {
			while ( cursor < end && *cursor != '\n' ) ++cursor;
			continue;
		}
		if ( cursor + 1 < end && cursor[0] == '/' && cursor[1] == '*' ) {
			cursor += 2;
			while ( cursor + 1 < end && !( cursor[0] == '*' && cursor[1] == '/' ) ) ++cursor;
			cursor = cursor + 1 < end ? cursor + 2 : end;
			continue;
		}
		break;
	}
	std::string token;
	if ( cursor >= end ) return token;
	if ( *cursor == '"' ) {
		++cursor;
		while ( cursor < end && *cursor != '"' ) token += *cursor++;
		if ( cursor < end ) ++cursor;
		return token;
	}
	while ( cursor < end && static_cast<unsigned char>( *cursor ) > ' ' ) token += *cursor++;
	return token;
}

} // namespace detail

/* Rows of `zone scale mapname "Title"`. Retail stops the whole client with a
 * drop on a short row ("UI_LoadMapList: Missing scale param." and its
 * siblings, 0x0040b662); this reader keeps the rows before it and says why. */
inline MapList ParseMapList( const char *text, std::size_t length ) {
	MapList list;
	if ( !text ) return list;
	const char *cursor = text;
	const char *end = text + length;
	for ( ;; ) {
		const std::string zone = detail::NextToken( cursor, end );
		if ( zone.empty() ) break;
		MapListEntry entry;
		entry.zone = std::atoi( zone.c_str() );
		const std::string scale = detail::NextToken( cursor, end );
		if ( scale.empty() ) { list.error = "UI_LoadMapList: Missing scale param."; break; }
		entry.scale = static_cast<float>( std::atof( scale.c_str() ) );
		entry.map = detail::NextToken( cursor, end );
		if ( entry.map.empty() ) { list.error = "UI_LoadMapList: Missing map file name."; break; }
		entry.title = detail::NextToken( cursor, end );
		if ( entry.title.empty() ) { list.error = "UI_LoadMapList: Missing text name."; break; }
		list.entries.push_back( std::move( entry ) );
	}
	return list;
}

/* The map name the loading screen looks up: the server's name up to any `$`
 * spawn target (SV_SpawnServer 0x00454ff0 copies it into 0x00b59b80). */
inline std::string MapNameForLoading( const char *server ) {
	if ( !server ) return std::string();
	const char *dollar = std::strchr( server, '$' );
	return dollar ? std::string( server, dollar ) : std::string( server );
}

} // namespace screen
} // namespace alice
} // namespace fntech3

#endif
