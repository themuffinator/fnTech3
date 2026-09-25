/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor: Allied Assault's menus and HUD

1.00 has no menu module. Its menus, loading screens, scoreboard and HUD are
the engine's own, built at start-up from the `.urc` resources under ui/ (179 menus
in Pak0.pk3) and driven by the player state the server sends. This file is
that engine UI. MOHAA.exe cannot be read (SafeDisc encrypts it), so its
behaviour follows OpenMoHAA's reconstruction (code/uilib and
code/client/cl_ui*.cpp, GPL-2.0-or-later), read as reference: the draw order,
the menu stacks, the HUD's selection of menus, the label's stat bars. None of
its text is used. The resource grammar is Ritual's, the one Alice's engine
reads, so code/compat/alice_urc.hpp parses the files.

Presentation (AGENTS.md invariant 11): a menu with `virtualres 1` covers the
screen as retail's does, its 640x480 stretched to the screen. Retail draws
the rest -- the HUD, the crosshair -- at their authored size in screen pixels,
so they shrink as the resolution grows. Here they scale with the screen's
height from 480 lines, as every other profile's HUD does, times ui_hudScale;
ui_hudScale 0 keeps retail's pixel sizes.

The saved-game list (FakkLoadGameClass) reads the save folder's headers
through code/compat/mohaa_savegame.hpp and shows each save's picture, which
`saveshot` takes from one frame of the view alone as the server saves. Its
delete asks first through `dialog`, over the retail question's art.

The LAN list (LANGameClass) shows the local servers that answered
`localservers`, whose answers the client keeps.

A label's 3D model: the one a cvar names (`rendermodel`), or the item in a
hand (`invmodelhand`), as global/inventory.txt shows it.
===========================================================================
*/

extern "C" {
#include "client.h"
}

#include "client_cpp.h"
#include "cl_mohaa.h"
#include "../ui/ui_public.h"
#include "../compat/alice_urc.hpp"
#include "../compat/mohaa_savegame.hpp"
#include "../compat/mohaa_load_record.hpp"
#include "../mohaa/mohaa_tiki.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace urc = fntech3::alice::urc;
namespace ssv = fntech3::mohaa::savegame;

/* ---- 1.00's player state, as the HUD reads it --------------------------- */

/* The stats (OpenMoHAA bg_public.h playerstat_t; the HUD menus' own comments
 * agree: `playerstat 0 // health`, `16 // north direction`). */
constexpr int kStatWeapons = 2;
constexpr int kStatEquippedWeapon = 3;
constexpr int kStatCrosshair = 9;
constexpr int kStatCompassNorth = 16;

/* activeItems slot 1 is the weapon in hand. Its configstring, past the
 * players', names the weapon, and "hud_ammo_<name>" is its ammo HUD. */
constexpr int kItemWeapon = 1;
constexpr int kCsWeapons = FNTECH3_MOHAA_CS_PLAYERS + FNTECH3_MOHAA_MAX_CLIENTS;

/* AA's pm_flags sit two bits above the later games' for bits 1..12, and are
 * theirs from bit 15 up (OpenMoHAA bg_compat.cpp
 * CPT_NormalizePlayerStateFlags_ver_6): intermission is bit 8, no-HUD bit 15.
 * AA has no weapon-bar flag. */
constexpr int kPmfIntermission = 1 << 8;
constexpr int kPmfNoHud = 1 << 15;

/* ---- colours ---------------------------------------------------------- */

struct Color {
	float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};

/* uilib's defaults: text in the HUD's gold, a dark blue background. */
constexpr Color kHudColor = { 0.7f, 0.6f, 0.05f, 1.0f };
constexpr Color kDefaultBackground = { 0.075f, 0.097f, 0.139f, 1.0f };
constexpr Color kDefaultBorder = { 0.15f, 0.195f, 0.278f, 1.0f };
constexpr Color kBlack = { 0.0f, 0.0f, 0.0f, 1.0f };
constexpr Color kWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

struct BorderColor {
	Color dark, reallyDark, light, original;
};

/* A 3D border's shades of one colour: 0.6 and 0.3 of it for the dark edges,
 * and for the light one three quarters of its saturation and 1.3 times its
 * value (uilib UBorderColor). */
BorderColor MakeBorder( const Color &c ) {
	BorderColor out;
	out.original = c;
	out.dark = { c.r * 0.6f, c.g * 0.6f, c.b * 0.6f, c.a };
	out.reallyDark = { c.r * 0.3f, c.g * 0.3f, c.b * 0.3f, c.a };

	const float high = std::max( { c.r, c.g, c.b } );
	const float low = std::min( { c.r, c.g, c.b } );
	float hue = 0.0f;
	if ( high > low ) {
		const float d = high - low;
		if ( high == c.r ) hue = ( c.g - c.b ) / d;
		else if ( high == c.g ) hue = 2.0f + ( c.b - c.r ) / d;
		else hue = 4.0f + ( c.r - c.g ) / d;
		hue *= 60.0f;
		if ( hue < 0.0f ) hue += 360.0f;
	}
	const float saturation = ( high > 0.0f ? ( high - low ) / high : 0.0f ) * 0.75f;
	const float value = std::min( high * 1.3f, 1.0f );
	const float sector = hue / 60.0f;
	const int i = static_cast<int>( std::floor( sector ) ) % 6;
	const float f = sector - std::floor( sector );
	const float p = value * ( 1.0f - saturation );
	const float q = value * ( 1.0f - saturation * f );
	const float t = value * ( 1.0f - saturation * ( 1.0f - f ) );
	float r = value, g = t, b = p;
	switch ( i ) {
	case 1: r = q; g = value; b = p; break;
	case 2: r = p; g = value; b = t; break;
	case 3: r = p; g = q; b = value; break;
	case 4: r = t; g = p; b = value; break;
	case 5: r = value; g = p; b = q; break;
	default: break;
	}
	out.light = { r, g, b, c.a };
	return out;
}

/* ---- pictures and fonts, registered when first drawn -------------------- */

struct Material {
	std::string name;
	qhandle_t handle = 0;
	bool looked = false;

	void Set( const std::string &shader ) {
		name = shader;
		handle = 0;
		looked = false;
	}
	/* A picture registered elsewhere, under a label of its own. */
	void SetHandle( const std::string &label, qhandle_t registered ) {
		name = label;
		handle = registered;
		looked = true;
	}
	bool Named() const { return !name.empty(); }
	qhandle_t Get() {
		if ( !looked ) {
			looked = true;
			handle = name.empty() ? 0 : re.RegisterShaderNoMip( name.c_str() );
		}
		return handle;
	}
};

void ShaderSize( qhandle_t shader, float &width, float &height ) {
	int w = 0, h = 0;
	if ( shader && re.GetShaderImageSize ) {
		re.GetShaderImageSize( shader, &w, &h );
	}
	width = w > 0 ? static_cast<float>( w ) : 64.0f;
	height = h > 0 ? static_cast<float>( h ) : 64.0f;
}

struct FontRef {
	std::string name = "verdana-12";
	void *font = nullptr;
	bool looked = false;

	void Set( const std::string &fontName ) {
		name = fontName;
		font = nullptr;
		looked = false;
	}
	void *Get() {
		if ( !looked ) {
			looked = true;
			font = CL_MOHAAFontLoad( name.c_str() );
		}
		return font;
	}
};

/* ---- scales --------------------------------------------------------------- */

struct Scale {
	float x = 1.0f, y = 1.0f;
};

cvar_t *ui_hudScale;
cvar_t *ui_crosshair;
cvar_t *ui_compass;
cvar_t *ui_weaponsbar;
cvar_t *ui_weaponsbartime;
cvar_t *ui_itemsbar;

float VidWidth() { return static_cast<float>( cls.glconfig.vidWidth ); }
float VidHeight() { return static_cast<float>( cls.glconfig.vidHeight ); }

Scale VirtualScale() { return { VidWidth() / 640.0f, VidHeight() / 480.0f }; }

Scale HudScale() {
	const float factor = ui_hudScale ? ui_hudScale->value : 1.0f;
	if ( !( factor > 0.0f ) ) {
		return {};
	}
	const float s = VidHeight() / 480.0f * factor;
	return { s, s };
}

/* ---- drawing -------------------------------------------------------------- */

void SetColor( const Color &c, float alpha ) {
	const vec4_t rgba = { c.r, c.g, c.b, c.a * alpha };
	re.SetColor( rgba );
}

void Fill( float x, float y, float w, float h ) {
	if ( w > 0.0f && h > 0.0f ) {
		re.DrawStretchPic( x, y, w, h, 0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader );
	}
}

void DrawBox( float x, float y, float w, float h, const Color &c, float alpha ) {
	SetColor( c, alpha );
	Fill( x, y, w, h );
}

/* Draw3DBox: an indented box is dark along its top and left, light along its
 * bottom and right; a raised one the other way round. */
void Draw3DBox( float x, float y, float w, float h, bool indent, const BorderColor &c, float alpha ) {
	if ( indent ) {
		DrawBox( x, y, 1.0f, h - 1.0f, c.dark, alpha );
		DrawBox( x + 1.0f, y + 1.0f, 1.0f, h - 3.0f, c.reallyDark, alpha );
		DrawBox( x, y, w - 1.0f, 1.0f, c.dark, alpha );
		DrawBox( x + 1.0f, y + 1.0f, w - 1.0f, 1.0f, c.reallyDark, alpha );
		DrawBox( x + w - 1.0f, y, 1.0f, h, c.light, alpha );
		DrawBox( x, y + h - 1.0f, w, 1.0f, c.light, alpha );
	} else {
		DrawBox( x + w - 2.0f, y + 1.0f, 1.0f, h - 2.0f, c.dark, alpha );
		DrawBox( x + w - 1.0f, y, 1.0f, h, c.reallyDark, alpha );
		DrawBox( x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f, c.dark, alpha );
		DrawBox( x, y + h - 1.0f, w, 1.0f, c.reallyDark, alpha );
		DrawBox( x, y, 1.0f, h - 1.0f, c.light, alpha );
		DrawBox( x, y, w - 1.0f, 1.0f, c.light, alpha );
	}
}

/* A frame drawn in `outside` around a box filled with `inside`. */
void DrawSolidBorderBox( float x, float y, float w, float h, const Color &inside, const Color &outside, float size,
	float alpha ) {
	DrawBox( x + size, y + size, w - size * 2.0f, h - size * 2.0f, inside, alpha );
	DrawBox( x, y, size, h - size, outside, alpha );
	DrawBox( x + size, y, w - size, size, outside, alpha );
	DrawBox( x + w - size, y + size, size, h - size, outside, alpha );
	DrawBox( x, y + h - size, w - size, size, outside, alpha );
}

void DrawPic( float x, float y, float w, float h, float s0, float t0, float s1, float t1, qhandle_t shader ) {
	if ( shader && w > 0.0f && h > 0.0f ) {
		re.DrawStretchPic( x, y, w, h, s0, t0, s1, t1, shader );
	}
}

/* Four corners, clockwise from the top left, in the current colour. */
void DrawQuadPic( const float xy[4][2], const float st[4][2], qhandle_t shader ) {
	if ( !shader || !re.DrawQuad ) {
		return;
	}
	refDrawQuad_t quad{};
	for ( int i = 0; i < 4; i++ ) {
		quad.xy[i][0] = xy[i][0];
		quad.xy[i][1] = xy[i][1];
		quad.st[i][0] = st[i][0];
		quad.st[i][1] = st[i][1];
	}
	quad.shader = shader;
	re.DrawQuad( &quad );
}

void DrawTrianglePic( const float xy[3][2], const float st[3][2], qhandle_t shader ) {
	const float quadXy[4][2] = { { xy[0][0], xy[0][1] }, { xy[1][0], xy[1][1] }, { xy[2][0], xy[2][1] },
		{ xy[2][0], xy[2][1] } };
	const float quadSt[4][2] = { { st[0][0], st[0][1] }, { st[1][0], st[1][1] }, { st[2][0], st[2][1] },
		{ st[2][0], st[2][1] } };
	DrawQuadPic( quadXy, quadSt, shader );
}

/* ---- text ----------------------------------------------------------------- */

enum { JustLeft, JustCenter, JustRight };
enum { JustTop, JustMiddle, JustBottom };

float FontHeight( void *font ) {
	return font ? static_cast<float>( CL_MOHAAFontHeight( font ) ) : 0.0f;
}

float TextWidth( void *font, const char *text ) {
	return font && text ? static_cast<float>( CL_MOHAAFontWidth( font, text, -1 ) ) : 0.0f;
}

/* Text at (x, y) in the font's own units, `scale` screen pixels to the unit. */
void Print( void *font, float x, float y, const char *text, const Scale &scale ) {
	if ( !font || !text || !text[0] ) {
		return;
	}
	const float factors[2] = { scale.x, scale.y };
	CL_MOHAAFontDraw( font, text, x, y, -1, factors );
}

/* UIFont::PrintJustified: the frame in screen pixels, each line placed on its
 * own, blank lines skipped. */
void PrintJustified( void *font, const float frame[4], int horizontal, int vertical, const std::string &text,
	const Scale &scale ) {
	if ( !font || text.empty() ) {
		return;
	}
	const float x = frame[0] / scale.x, y = frame[1] / scale.y;
	const float w = frame[2] / scale.x, h = frame[3] / scale.y;
	if ( horizontal == JustLeft && vertical == JustTop ) {
		Print( font, x, y, text.c_str(), scale );
		return;
	}
	const float lineHeight = FontHeight( font );
	const float textHeight = lineHeight * static_cast<float>( 1 + std::count( text.begin(), text.end(), '\n' ) );
	float lineY = y;
	if ( vertical == JustMiddle ) lineY = y + ( h - textHeight ) * 0.5f;
	else if ( vertical == JustBottom ) lineY = y + h - textHeight;

	std::size_t start = 0;
	while ( start < text.size() ) {
		while ( start < text.size() && text[start] == '\n' ) start++;
		if ( start >= text.size() ) break;
		std::size_t end = text.find( '\n', start );
		if ( end == std::string::npos ) end = text.size();
		const std::string line = text.substr( start, end - start );
		float lineX = x;
		if ( horizontal == JustCenter ) lineX = x + ( w - TextWidth( font, line.c_str() ) ) * 0.5f;
		else if ( horizontal == JustRight ) lineX = x + w - TextWidth( font, line.c_str() );
		Print( font, lineX, lineY, line.c_str(), scale );
		lineY += lineHeight;
		start = end;
	}
}

/* ---- the player's state --------------------------------------------------- */

const fntech3::mohaa::PlayerState *PlayerState() {
	return cls.state == CA_ACTIVE ? CL_MOHAAPlayerState() : nullptr;
}

int Stat( int index ) {
	const fntech3::mohaa::PlayerState *ps = PlayerState();
	if ( !ps || index < 0 || index >= FNTECH3_MOHAA_MAX_STATS ) {
		return 0;
	}
	return ps->stats[index];
}

int ActiveItem( int index ) {
	const fntech3::mohaa::PlayerState *ps = PlayerState();
	if ( !ps || index < 0 || index >= FNTECH3_MOHAA_MAX_ACTIVE_ITEMS ) {
		return -1;
	}
	return ps->activeItems[index];
}

const char *ConfigString( int index ) {
	if ( index < 0 || index >= MAX_GAME_CONFIGSTRINGS ) {
		return "";
	}
	const int offset = cl.gameState.stringOffsets[index];
	if ( offset <= 0 || offset >= static_cast<int>( sizeof( cl.gameState.stringData ) ) ) {
		return "";
	}
	return cl.gameState.stringData + offset;
}

std::string CvarString( const std::string &name ) {
	char buffer[MAX_CVAR_VALUE_STRING];
	Cvar_VariableStringBuffer( name.c_str(), buffer, sizeof( buffer ) );
	return buffer;
}

std::string LatchedCvarString( const std::string &name ) {
	char buffer[MAX_CVAR_VALUE_STRING];
	Cvar_LatchedVariableStringBufferSafe( name.c_str(), buffer, sizeof( buffer ), 0 );
	return buffer;
}

void PlaySound( const std::string &name ) {
	if ( name.empty() ) {
		return;
	}
	const sfxHandle_t sfx = S_RegisterSound( name.c_str(), qfalse );
	if ( sfx ) {
		S_StartLocalSound( sfx, CHAN_LOCAL_SOUND );
	}
}

void Stuff( const std::string &command ) {
	if ( !command.empty() ) {
		Cbuf_AddText( ( command + "\n" ).c_str() );
	}
}

/* ---- the widgets ------------------------------------------------------------ */

enum class Kind { Label, Button, CheckBox, Slider, Field, List, ListBox, Pulldown, BindList, LoadGame, LanGame };

/* A resource's class: "UI" before its token, Label translated to FakkLabel
 * (OpenMoHAA UILayout::ProcessCommands). */
bool KindFromClass( const std::string &token, Kind &kind ) {
	static const struct {
		const char *name;
		Kind kind;
	} kKinds[] = {
		{ "Label", Kind::Label }, { "Button", Kind::Button }, { "CheckBox", Kind::CheckBox },
		{ "Slider", Kind::Slider }, { "Field", Kind::Field }, { "List", Kind::List },
		{ "ListBox", Kind::ListBox }, { "PulldownMenuContainer", Kind::Pulldown },
		{ "FakkBindList", Kind::BindList }, { "FakkLoadGameClass", Kind::LoadGame },
		{ "LANGameClass", Kind::LanGame },
	};
	for ( const auto &entry : kKinds ) {
		if ( urc::EqualsNoCase( token, entry.name ) ) {
			kind = entry.kind;
			return true;
		}
	}
	return false;
}

enum class Bar {
	None, Horizontal, Vertical, VerticalReverse, StaggerEven, StaggerOdd, Circular, Needle, Rotator, Compass,
	Spinner, HeadingSpinner
};

enum { BorderNone = 0, Border3D = 1, BorderIndent = 2 };
enum { AlignLeft = 1, AlignRight = 2, AlignTop = 4, AlignBottom = 8, AlignCenterX = 16, AlignCenterY = 32 };

/* One row of a FakkBindList: what the player reads and the command bound. */
struct BindItem {
	std::string name, command;
};

struct BindLayout {
	std::string headers[3] = { "Action", "Primary", "Alternate" };
	float commandWidth = 160.0f, keyWidth = 64.0f, rowHeight = 16.0f;
	Color titleForeground = { 0.4f, 1.0f, 0.25f, 1.0f }, titleBackground = kBlack;
	Color activeBorder = { 1.0f, 0.2f, 0.2f, 1.0f };
	Color inactiveForeground = kWhite, inactiveBackground = { 0.1f, 0.1f, 0.1f, 1.0f };
	Color highlightForeground = { 0.5f, 1.0f, 0.5f, 1.0f }, highlightBackground = kBlack;
	Color selectForeground = { 1.0f, 0.5f, 0.5f, 1.0f }, selectBackground = kBlack;
	std::string activeSound, enterSound;
	std::vector<BindItem> items;
};

struct Popup {
	std::string title, type, data;
};

/* One row of a FakkLoadGameClass: a save's name on disk and what the list
 * shows of it. */
struct SaveRow {
	std::string name, comment, elapsed, date;
	std::uint32_t saveTime = 0;
};

struct Widget {
	Kind kind = Kind::Label;
	std::string name, title;
	/* The layout's widgets start at 0,0 128x64 (UILayout InitFrame). */
	float rect[4] = { 0.0f, 0.0f, 128.0f, 64.0f };
	Color foreground = kHudColor, background = kDefaultBackground;
	BorderColor border = MakeBorder( kDefaultBorder );
	int borderStyle = BorderIndent;
	Material material, hoverMaterial, pressedMaterial;
	bool tile = false;
	FontRef font;
	int horizontal = JustCenter, vertical = JustMiddle;
	std::string cvar, command, clickSound, hoverSound, hoverCommand, awayCommand, enabledCvar;
	std::vector<std::string> showCommands, hideCommands;
	float fadeTime = 0.0f;
	urc::Direction direction = urc::Direction::None;
	float motionTime = 0.0f;
	int align = 0;
	bool virtualRes = false, screenPlaced = false, stretchVertical = false, stretchHorizontal = false;
	int order = 0;

	/* UILabel */
	std::vector<std::pair<std::string, std::string>> linkStrings;
	bool linkCvarToShader = false;
	std::string currentShader;

	/* FakkLabel */
	int stat = -1, statAlpha = -1, statConfigstring = -1, maxStat = -1, itemStat = -1, invModelHand = -1;
	bool renderModel = false;
	std::string modelName;
	/* A `rendermodel` label's placing and pose, and where its animation is. */
	float modelOffset[3] = {}, modelRotateOffset[3] = {}, modelAngles[3] = {};
	float modelScale = 1.0f;
	std::string modelAnim, poseAnim;
	int poseIndex = -1, poseUpdated = 0;
	float poseTime = 0.0f;
	Bar bar = Bar::None;
	float barMin = -1.0f, barMax = -1.0f;
	Material barMaterial, barFlash, barMarker;
	float angles[3] = {};
	float rotatorScale = 0.0f;
	float lastFrac = -1.0f;
	int flashTime = 0;

	/* UICheckBox */
	Material checkedMaterial, uncheckedMaterial;
	std::string checkedCommand, uncheckedCommand;

	/* UISlider */
	float sliderMin = 0.0f, sliderMax = 100.0f, step = 1.0f, oldValue = 0.0f;
	bool sliderFloat = false, thumbDown = false, prevDown = false, nextDown = false;
	int repeatAt = 0;

	/* UIList, UIListBox */
	std::vector<std::pair<std::string, std::string>> items;
	int current = 0;
	int top = 0;

	/* UIPulldownMenuContainer: one menu of popups, its title "MENU". */
	std::vector<Popup> popups;
	Material menuMaterial, selectedMenuMaterial;
	bool open = false;
	int popupHover = -1;

	/* UIField */
	int cursor = 0;

	/* FakkBindList */
	BindLayout bind;
	int bindRow = -1, bindColumn = 0;
	bool binding = false;

	/* FakkLoadGameClass: the saves, newest first; `current` and `top` as a
	 * list's. */
	std::vector<SaveRow> saves;
	bool removePending = false;
	int clickTime = 0, clickRow = 0;

	/* What the menu's showing and the pointer did to it. */
	bool enabled = true, shown = false, hovered = false, pressed = false;
	float frame[4] = {};
	float alpha = 1.0f;
	Scale scale;
};

struct Menu {
	std::string name, file;
	float width = 0.0f, height = 0.0f;
	urc::Direction direction = urc::Direction::None;
	float motionTime = 0.0f;
	Color background = kDefaultBackground, foreground = kHudColor;
	BorderColor border = MakeBorder( kDefaultBorder );
	int borderStyle = BorderIndent;
	Color fill = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool fullscreen = false, virtualRes = false;
	int align = 0;
	float fadeTime = 0.0f;
	std::vector<Widget> widgets;

	bool visible = false, motionOut = false;
	int motionStart = 0;
	int hideAt = 0, showAt = 0;
	float frame[4] = {};
	float alpha = 1.0f;
	std::vector<std::string> savedCvars;
};

std::vector<Menu> menus;

int FindMenu( const char *name ) {
	if ( !name ) {
		return -1;
	}
	for ( std::size_t i = 0; i < menus.size(); i++ ) {
		if ( !Q_stricmp( menus[i].name.c_str(), name ) ) {
			return static_cast<int>( i );
		}
	}
	return -1;
}

/* ---- reading the resources ---------------------------------------------- */

float Arg( const std::vector<std::string> &args, std::size_t i, float fallback = 0.0f ) {
	return i < args.size() ? static_cast<float>( std::atof( args[i].c_str() ) ) : fallback;
}

int IntArg( const std::vector<std::string> &args, std::size_t i, int fallback = 0 ) {
	return i < args.size() ? std::atoi( args[i].c_str() ) : fallback;
}

std::string StringArg( const std::vector<std::string> &args, std::size_t i ) {
	return i < args.size() ? args[i] : std::string();
}

bool Is( const std::string &a, const char *b ) {
	return urc::EqualsNoCase( a, b );
}

Color ColorArgs( const std::vector<std::string> &args, std::size_t first = 0 ) {
	return { Arg( args, first ), Arg( args, first + 1 ), Arg( args, first + 2 ), Arg( args, first + 3 ) };
}

/* A vector written as one quoted string, "60 12 0", or as three words. */
void VectorArgs( const std::vector<std::string> &args, float out[3] ) {
	out[0] = out[1] = out[2] = 0.0f;
	if ( args.size() >= 3 ) {
		for ( int i = 0; i < 3; i++ ) out[i] = Arg( args, static_cast<std::size_t>( i ) );
	} else if ( !args.empty() ) {
		std::sscanf( args[0].c_str(), "%f %f %f", &out[0], &out[1], &out[2] );
	}
}

int AlignArgs( const std::vector<std::string> &args ) {
	int align = 0;
	for ( const std::string &word : args ) {
		if ( Is( word, "left" ) ) align |= AlignLeft;
		else if ( Is( word, "right" ) ) align |= AlignRight;
		else if ( Is( word, "top" ) ) align |= AlignTop;
		else if ( Is( word, "bottom" ) ) align |= AlignBottom;
		else if ( Is( word, "centerx" ) ) align |= AlignCenterX;
		else if ( Is( word, "centery" ) ) align |= AlignCenterY;
	}
	return align;
}

bool ReadText( const std::string &path, std::string &text ) {
	void *buffer = nullptr;
	const int length = FS_ReadFile( path.c_str(), &buffer );
	if ( length < 0 || !buffer ) {
		if ( buffer ) {
			FS_FreeFile( buffer );
		}
		return false;
	}
	text.assign( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ) );
	FS_FreeFile( buffer );
	return true;
}

/* FakkBindList's `filename`: its headers, sizes, colours, sounds and rows. */
void LoadBindFile( BindLayout &bind, const std::string &path ) {
	std::string text;
	if ( !ReadText( path, text ) ) {
		Com_Printf( "UI: couldn't load bind file %s\n", path.c_str() );
		return;
	}
	/* The file is lines of keyword and values, a menu's events in all but name. */
	const std::string wrapped = "menu \"bind\" 0 0 NONE\n" + text;
	const urc::Layout parsed = urc::Parse( wrapped.data(), wrapped.size() );
	if ( parsed.menus.empty() ) {
		return;
	}
	for ( const urc::Property &p : parsed.menus.front().events ) {
		if ( Is( p.name, "header" ) ) {
			const int column = IntArg( p.args, 0 ) - 1;
			if ( column >= 0 && column < 3 ) bind.headers[column] = StringArg( p.args, 1 );
		} else if ( Is( p.name, "bind_commandwidth" ) ) bind.commandWidth = Arg( p.args, 0, bind.commandWidth );
		else if ( Is( p.name, "bind_commandheight" ) ) bind.rowHeight = Arg( p.args, 0, bind.rowHeight );
		else if ( Is( p.name, "bind_primarykeywidth" ) ) bind.keyWidth = Arg( p.args, 0, bind.keyWidth );
		else if ( Is( p.name, "titleforegroundcolor" ) ) bind.titleForeground = ColorArgs( p.args );
		else if ( Is( p.name, "titlebackgroundcolor" ) ) bind.titleBackground = ColorArgs( p.args );
		else if ( Is( p.name, "activebordercolor" ) ) bind.activeBorder = ColorArgs( p.args );
		else if ( Is( p.name, "inactiveforegroundcolor" ) ) bind.inactiveForeground = ColorArgs( p.args );
		else if ( Is( p.name, "inactivebackgroundcolor" ) ) bind.inactiveBackground = ColorArgs( p.args );
		else if ( Is( p.name, "highlightforegroundcolor" ) ) bind.highlightForeground = ColorArgs( p.args );
		else if ( Is( p.name, "highlightbackgroundcolor" ) ) bind.highlightBackground = ColorArgs( p.args );
		else if ( Is( p.name, "selectforegroundcolor" ) ) bind.selectForeground = ColorArgs( p.args );
		else if ( Is( p.name, "selectbackgroundcolor" ) ) bind.selectBackground = ColorArgs( p.args );
		else if ( Is( p.name, "activesound" ) ) bind.activeSound = StringArg( p.args, 0 );
		else if ( Is( p.name, "entersound" ) ) bind.enterSound = StringArg( p.args, 0 );
		else if ( Is( p.name, "binditem" ) ) bind.items.push_back( { StringArg( p.args, 0 ), StringArg( p.args, 1 ) } );
	}
}

/* FakkLoadGameClass's and LANGameClass's own events, below the drawing. */
bool LoadGameEvent( Widget &w, const std::string &event );
bool LanGameEvent( Widget &w, const std::string &event );

/* One widget keyword. These are the layout events of UIWidget, UILabel,
 * FakkLabel, UIButton, UICheckBox, UISlider, UIList and
 * UIPulldownMenuContainer that 1.00's menus use; the rest (aliascache,
 * dontlocalize, noparentclip) change nothing drawn. */
void ApplyProperty( Widget &w, const std::string &name, const std::vector<std::string> &args ) {
	if ( w.kind == Kind::LoadGame && LoadGameEvent( w, name ) ) {
		return;
	}
	if ( w.kind == Kind::LanGame && LanGameEvent( w, name ) ) {
		return;
	}
	if ( Is( name, "name" ) ) w.name = StringArg( args, 0 );
	else if ( Is( name, "rect" ) || Is( name, "size" ) ) {
		if ( args.size() == 4 ) {
			for ( int i = 0; i < 4; i++ ) w.rect[i] = Arg( args, static_cast<std::size_t>( i ) );
		}
	} else if ( Is( name, "title" ) ) w.title = StringArg( args, 0 );
	else if ( Is( name, "font" ) ) w.font.Set( StringArg( args, 0 ) );
	else if ( Is( name, "fgcolor" ) ) w.foreground = ColorArgs( args );
	else if ( Is( name, "bgcolor" ) ) {
		/* A background colour is its border's too (setBackgroundColor). */
		w.background = ColorArgs( args );
		w.border = MakeBorder( w.background );
	} else if ( Is( name, "bordercolor" ) ) w.border = MakeBorder( ColorArgs( args ) );
	else if ( Is( name, "borderstyle" ) ) {
		const std::string style = StringArg( args, 0 );
		if ( Is( style, "NONE" ) ) w.borderStyle = BorderNone;
		else if ( Is( style, "3D_BORDER" ) ) w.borderStyle = Border3D;
		else if ( Is( style, "INDENT_BORDER" ) ) w.borderStyle = BorderIndent;
	} else if ( Is( name, "shader" ) ) {
		w.currentShader = StringArg( args, 0 );
		w.material.Set( w.currentShader );
		w.tile = false;
	} else if ( Is( name, "tileshader" ) ) {
		w.currentShader = StringArg( args, 0 );
		w.material.Set( w.currentShader );
		w.tile = true;
	} else if ( Is( name, "hovershader" ) ) w.hoverMaterial.Set( StringArg( args, 0 ) );
	else if ( Is( name, "pressedshader" ) ) w.pressedMaterial.Set( StringArg( args, 0 ) );
	else if ( Is( name, "fadein" ) ) w.fadeTime = Arg( args, 0 );
	else if ( Is( name, "clicksound" ) ) w.clickSound = StringArg( args, 0 );
	else if ( Is( name, "hoversound" ) ) w.hoverSound = StringArg( args, 0 );
	else if ( Is( name, "hovercommand" ) ) w.hoverCommand = StringArg( args, 0 );
	else if ( Is( name, "mouseawaycommand" ) ) w.awayCommand = StringArg( args, 0 );
	else if ( Is( name, "stretch" ) ) {
		const std::string how = StringArg( args, 0 );
		if ( Is( how, "vertical" ) || Is( how, "both" ) ) w.stretchVertical = true;
		if ( Is( how, "horizontal" ) || Is( how, "both" ) ) w.stretchHorizontal = true;
		w.screenPlaced = true;
	} else if ( Is( name, "virtualres" ) ) {
		if ( IntArg( args, 0 ) ) w.virtualRes = true;
	} else if ( Is( name, "initdata" ) ) {
		/* Sets the linked cvar as the menu loads. */
		if ( !w.cvar.empty() ) Cvar_Set( w.cvar.c_str(), StringArg( args, 0 ).c_str() );
	} else if ( Is( name, "direction" ) ) {
		w.direction = urc::DirectionFromName( StringArg( args, 0 ), false );
		if ( args.size() > 1 ) w.motionTime = Arg( args, 1 );
	} else if ( Is( name, "align" ) ) {
		w.align = AlignArgs( args );
		w.screenPlaced = true;
	} else if ( Is( name, "stuffcommand" ) ) w.command = StringArg( args, 0 );
	else if ( Is( name, "linkcvar" ) ) w.cvar = StringArg( args, 0 );
	else if ( Is( name, "ordernumber" ) ) w.order = IntArg( args, 0 );
	else if ( Is( name, "textalign" ) ) {
		for ( const std::string &word : args ) {
			if ( Is( word, "left" ) ) w.horizontal = JustLeft;
			else if ( Is( word, "right" ) ) w.horizontal = JustRight;
			else if ( Is( word, "centerx" ) ) w.horizontal = JustCenter;
			else if ( Is( word, "top" ) ) w.vertical = JustTop;
			else if ( Is( word, "bottom" ) ) w.vertical = JustBottom;
			else if ( Is( word, "centery" ) ) w.vertical = JustMiddle;
		}
	} else if ( Is( name, "enabledcvar" ) ) w.enabledCvar = StringArg( args, 0 );
	else if ( Is( name, "showcommand" ) ) w.showCommands.push_back( StringArg( args, 0 ) );
	else if ( Is( name, "hidecommand" ) ) w.hideCommands.push_back( StringArg( args, 0 ) );
	/* UILabel */
	else if ( Is( name, "linkstring" ) ) w.linkStrings.push_back( { StringArg( args, 0 ), StringArg( args, 1 ) } );
	else if ( Is( name, "linkcvartoshader" ) ) w.linkCvarToShader = true;
	/* FakkLabel */
	else if ( Is( name, "playerstat" ) ) w.stat = IntArg( args, 0 );
	else if ( Is( name, "playerstatalpha" ) ) w.statAlpha = IntArg( args, 0 );
	else if ( Is( name, "playerstatconfigstring" ) ) w.statConfigstring = IntArg( args, 0 );
	else if ( Is( name, "maxplayerstat" ) ) w.maxStat = IntArg( args, 0 );
	else if ( Is( name, "itemstat" ) ) w.itemStat = IntArg( args, 0 );
	else if ( Is( name, "invmodelhand" ) ) w.invModelHand = IntArg( args, 0 );
	else if ( Is( name, "invmodelname" ) ) w.modelName = StringArg( args, 0 );
	else if ( Is( name, "rendermodel" ) ) w.renderModel = IntArg( args, 0 ) != 0;
	else if ( Is( name, "modeloffset" ) ) VectorArgs( args, w.modelOffset );
	else if ( Is( name, "modelrotateoffset" ) ) VectorArgs( args, w.modelRotateOffset );
	else if ( Is( name, "modelangles" ) ) VectorArgs( args, w.modelAngles );
	else if ( Is( name, "modelscale" ) ) w.modelScale = Arg( args, 0, 1.0f );
	else if ( Is( name, "modelanim" ) ) w.modelAnim = StringArg( args, 0 );
	else if ( Is( name, "statbar" ) ) {
		static const struct {
			const char *name;
			Bar bar;
			bool round;
		} kBars[] = {
			{ "horizontal", Bar::Horizontal, false }, { "vertical_stagger_even", Bar::StaggerEven, false },
			{ "vertical_stagger_odd", Bar::StaggerOdd, false }, { "vertical", Bar::Vertical, false },
			{ "vertical_reverse", Bar::VerticalReverse, false }, { "circular", Bar::Circular, true },
			{ "needle", Bar::Needle, true }, { "rotator", Bar::Rotator, true }, { "compass", Bar::Compass, true },
			{ "spinner", Bar::Spinner, true }, { "headingspinner", Bar::HeadingSpinner, true },
		};
		if ( args.size() > 1 ) {
			w.barMin = Arg( args, 1 );
			w.barMax = Arg( args, 2 );
		}
		const std::string type = StringArg( args, 0 );
		bool known = false;
		for ( const auto &entry : kBars ) {
			if ( Is( type, entry.name ) ) {
				w.bar = entry.bar;
				/* The round bars draw "blank" until given a shader. */
				if ( entry.round && !w.barMaterial.Named() ) w.barMaterial.Set( "blank" );
				known = true;
				break;
			}
		}
		if ( !known ) Com_DPrintf( "UI: invalid statbar orientation \"%s\"\n", type.c_str() );
	} else if ( Is( name, "statbar_shader" ) ) w.barMaterial.Set( StringArg( args, 0 ) );
	else if ( Is( name, "statbar_tileshader" ) ) {
		w.barMaterial.Set( StringArg( args, 0 ) );
		w.tile = true;
	} else if ( Is( name, "statbar_shader_flash" ) ) w.barFlash.Set( StringArg( args, 0 ) );
	else if ( Is( name, "statbar_tileshader_flash" ) ) {
		w.barFlash.Set( StringArg( args, 0 ) );
		w.tile = true;
	} else if ( Is( name, "statbar_shader_marker" ) ) w.barMarker.Set( StringArg( args, 0 ) );
	else if ( Is( name, "statbar_endangles" ) ) {
		w.angles[0] = Arg( args, 0 );
		w.angles[1] = Arg( args, 1 );
	} else if ( Is( name, "statbar_needlewidth" ) ) w.angles[2] = Arg( args, 0 );
	else if ( Is( name, "statbar_rotatorsize" ) ) {
		w.angles[2] = Arg( args, 0 );
		w.rotatorScale = args.size() > 1 ? Arg( args, 1 ) : w.angles[2];
	}
	/* UICheckBox */
	else if ( Is( name, "checked_shader" ) ) w.checkedMaterial.Set( StringArg( args, 0 ) );
	else if ( Is( name, "unchecked_shader" ) ) w.uncheckedMaterial.Set( StringArg( args, 0 ) );
	else if ( Is( name, "checked_command" ) ) w.checkedCommand = StringArg( args, 0 );
	else if ( Is( name, "unchecked_command" ) ) w.uncheckedCommand = StringArg( args, 0 );
	/* UISlider */
	else if ( Is( name, "setrange" ) ) {
		w.sliderMin = Arg( args, 0 );
		w.sliderMax = Arg( args, 1 );
	} else if ( Is( name, "slidertype" ) ) {
		if ( Is( StringArg( args, 0 ), "float" ) ) w.sliderFloat = true;
		else if ( Is( StringArg( args, 0 ), "integer" ) ) w.sliderFloat = false;
	} else if ( Is( name, "stepsize" ) ) w.step = Arg( args, 0 );
	/* UIList, UIListBox */
	else if ( Is( name, "additem" ) ) {
		w.items.push_back( { StringArg( args, 0 ), StringArg( args, 1 ) } );
		w.current = 1;
	}
	/* UIPulldownMenuContainer */
	else if ( Is( name, "addpopup" ) ) w.popups.push_back( { StringArg( args, 1 ), StringArg( args, 2 ), StringArg( args, 3 ) } );
	else if ( Is( name, "menushader" ) ) w.menuMaterial.Set( StringArg( args, 1 ) );
	else if ( Is( name, "selmenushader" ) ) w.selectedMenuMaterial.Set( StringArg( args, 1 ) );
	/* FakkBindList */
	else if ( Is( name, "filename" ) ) LoadBindFile( w.bind, StringArg( args, 0 ) );
	/* Widget events a menu or the widgetcommand console command sends. */
	else if ( Is( name, "enable" ) ) {
		w.enabled = true;
		w.shown = true;
	} else if ( Is( name, "disable" ) ) {
		w.enabled = false;
		w.shown = false;
	} else if ( Is( name, "hide" ) ) w.shown = false;
}

/* The properties of a menu's own container. */
void ApplyMenuProperty( Menu &m, const urc::Property &p ) {
	if ( Is( p.name, "bgcolor" ) ) {
		m.background = ColorArgs( p.args );
		m.border = MakeBorder( m.background );
	} else if ( Is( p.name, "fgcolor" ) ) m.foreground = ColorArgs( p.args );
	else if ( Is( p.name, "bordercolor" ) ) m.border = MakeBorder( ColorArgs( p.args ) );
	else if ( Is( p.name, "borderstyle" ) ) {
		const std::string style = StringArg( p.args, 0 );
		if ( Is( style, "NONE" ) ) m.borderStyle = BorderNone;
		else if ( Is( style, "3D_BORDER" ) ) m.borderStyle = Border3D;
		else if ( Is( style, "INDENT_BORDER" ) ) m.borderStyle = BorderIndent;
	} else if ( Is( p.name, "bgfill" ) ) m.fill = ColorArgs( p.args );
	else if ( Is( p.name, "fullscreen" ) ) m.fullscreen = IntArg( p.args, 0 ) != 0;
	else if ( Is( p.name, "virtualres" ) ) {
		if ( IntArg( p.args, 0 ) ) m.virtualRes = true;
	} else if ( Is( p.name, "align" ) ) m.align = AlignArgs( p.args );
	else if ( Is( p.name, "fadein" ) ) m.fadeTime = Arg( p.args, 0 );
}

bool BuildWidget( const urc::Widget &parsed, Widget &w, const std::string &file ) {
	if ( !KindFromClass( parsed.classToken, w.kind ) ) {
		Com_DPrintf( "UI: %s:%d: Failed to find resource type '%s'\n", file.c_str(), parsed.line,
			parsed.classToken.c_str() );
		return false;
	}
	if ( w.kind == Kind::Button ) {
		w.borderStyle = BorderNone;
	}
	for ( const urc::Property &p : parsed.properties ) {
		ApplyProperty( w, p.name, p.args );
	}
	return true;
}

void AppendWidgets( Menu &m, const urc::Menu &parsed, const std::string &file ) {
	for ( const urc::Widget &pw : parsed.widgets ) {
		Widget w;
		if ( BuildWidget( pw, w, file ) ) {
			m.widgets.push_back( std::move( w ) );
		}
	}
}

/* A file's menus. A `menu` line without a name adds its resources to the
 * menu before it, which is how ui/loadingbar.txt joins the loading screens;
 * `postinclude` reads its file into the file's last menu once the file is
 * done (UILayout::ProcessCommands). */
void LoadMenuFile( const std::string &path ) {
	std::string text;
	if ( !ReadText( path, text ) ) {
		Com_Printf( "UI: couldn't load %s\n", path.c_str() );
		return;
	}
	const urc::IncludeReader reader = []( const std::string &name, std::string &out ) {
		return ReadText( name, out );
	};
	const urc::Layout layout = urc::Parse( text.data(), text.size(), reader );
	for ( const urc::Diagnostic &d : layout.diagnostics ) {
		/* The parser knows Alice's classes; the ones only this UI serves
		 * (LANGameClass) are not missing here. */
		static const std::string kUnknownClass = "Tried to instantiate a widget that doesn't exist: ";
		Kind kind;
		if ( !d.message.compare( 0, kUnknownClass.size(), kUnknownClass )
			&& KindFromClass( d.message.substr( kUnknownClass.size() ), kind ) ) {
			continue;
		}
		Com_DPrintf( "UI: %s:%d: %s\n", path.c_str(), d.line, d.message.c_str() );
	}

	int target = -1;
	std::string postInclude;
	for ( const urc::Menu &parsed : layout.menus ) {
		urc::MenuHeader header;
		const bool named = urc::ReadMenuHeader( parsed.header, header ) && !header.name.empty();
		if ( !named ) {
			if ( target >= 0 ) {
				for ( const urc::Property &p : parsed.events ) ApplyMenuProperty( menus[target], p );
				AppendWidgets( menus[target], parsed, path );
			}
			continue;
		}
		Menu m;
		m.name = header.name;
		m.file = path;
		m.width = header.width;
		m.height = header.height;
		m.direction = header.direction;
		m.motionTime = header.motionTime;
		for ( const urc::Property &p : parsed.events ) {
			if ( Is( p.name, "postinclude" ) ) postInclude = StringArg( p.args, 0 );
			else ApplyMenuProperty( m, p );
		}
		AppendWidgets( m, parsed, path );
		menus.push_back( std::move( m ) );
		target = static_cast<int>( menus.size() ) - 1;
	}

	if ( !postInclude.empty() && target >= 0 ) {
		std::string included;
		if ( !ReadText( postInclude, included ) ) {
			Com_Printf( "UI: couldn't load %s\n", postInclude.c_str() );
			return;
		}
		const urc::Layout extra = urc::Parse( included.data(), included.size(), reader );
		for ( const urc::Menu &parsed : extra.menus ) {
			AppendWidgets( menus[target], parsed, postInclude );
		}
	}
}

/* A menu's widgets take its virtual resolution (UILayout gives each widget
 * the container's `virtualres` as it is made). */
void FinishMenu( Menu &m ) {
	for ( Widget &w : m.widgets ) {
		if ( m.virtualRes ) w.virtualRes = true;
	}
}

void LoadAllMenus() {
	int count = 0;
	char **files = FS_ListFiles( "ui", ".urc", &count );
	for ( int i = 0; i < count; i++ ) {
		const std::size_t before = menus.size();
		LoadMenuFile( std::string( "ui/" ) + files[i] );
		for ( std::size_t m = before; m < menus.size(); m++ ) FinishMenu( menus[m] );
	}
	FS_FreeFileList( files );
	Com_Printf( "Medal of Honor: %i menus from %i resource files\n", static_cast<int>( menus.size() ), count );
}

/* ---- showing and hiding --------------------------------------------------- */

int Now() { return cls.realtime; }

bool WidgetEnabled( const Widget &w ) {
	if ( !w.enabled ) return false;
	return w.enabledCvar.empty() || Cvar_VariableIntegerValue( w.enabledCvar.c_str() ) != 0;
}

/* UIWidget::Display draws a widget with an `enabledcvar` whether shown or not. */
bool WidgetDrawn( const Widget &w ) {
	return WidgetEnabled( w ) && ( !w.enabledCvar.empty() || w.shown );
}

float MaxMotionTime( const Menu &m ) {
	float longest = m.motionTime;
	for ( const Widget &w : m.widgets ) longest = std::max( longest, w.motionTime );
	return longest;
}

void SaveCvars( Menu &m ) {
	m.savedCvars.clear();
	for ( const Widget &w : m.widgets ) {
		m.savedCvars.push_back( w.cvar.empty() ? std::string() : CvarString( w.cvar ) );
	}
}

void RestoreCvars( Menu &m ) {
	for ( std::size_t i = 0; i < m.widgets.size() && i < m.savedCvars.size(); i++ ) {
		if ( !m.widgets[i].cvar.empty() ) Cvar_Set( m.widgets[i].cvar.c_str(), m.savedCvars[i].c_str() );
	}
}

/* UIFAKKLoadGameClass::SetupFiles, below the drawing. */
void SetupSaves( Menu &m, Widget &w );

/* Menu::ShowMenu: every enabled widget shows, its motion starts again and its
 * show commands run. A saved-game list reads the folder again
 * (UpdateUIElement). */
void ShowMenu( Menu &m ) {
	m.visible = true;
	m.motionOut = false;
	m.motionStart = Now();
	m.hideAt = 0;
	m.showAt = 0;
	for ( Widget &w : m.widgets ) {
		if ( WidgetEnabled( w ) ) {
			w.shown = true;
			for ( const std::string &c : w.showCommands ) Stuff( c );
		}
		w.pressed = w.hovered = w.open = false;
		w.lastFrac = -1.0f;
		if ( w.kind == Kind::LoadGame ) SetupSaves( m, w );
	}
}

/* ForceShow: shown at once, no motion, no commands. */
void ForceShow( Menu &m ) {
	const bool wasShown = m.visible && !m.motionOut;
	if ( !wasShown ) {
		m.motionStart = Now() - 100000;
	}
	m.visible = true;
	m.motionOut = false;
	m.hideAt = 0;
	for ( Widget &w : m.widgets ) {
		if ( WidgetEnabled( w ) ) w.shown = true;
		if ( w.kind == Kind::LoadGame && !wasShown ) SetupSaves( m, w );
	}
}

void ForceHide( Menu &m ) {
	for ( Widget &w : m.widgets ) {
		if ( w.shown ) {
			for ( const std::string &c : w.hideCommands ) Stuff( c );
		}
		w.shown = false;
		w.pressed = w.hovered = w.open = false;
	}
	m.visible = false;
	m.motionOut = false;
	m.hideAt = 0;
	m.showAt = 0;
}

/* Menu::HideMenu: the widgets move out, and hide once the longest motion is
 * done -- at once when forced or when nothing moves. */
void HideMenu( Menu &m, bool force ) {
	const float longest = MaxMotionTime( m );
	if ( force || longest <= 0.0f ) {
		ForceHide( m );
		return;
	}
	if ( !m.motionOut ) {
		m.motionOut = true;
		m.motionStart = Now();
	}
	m.showAt = 0;
	m.hideAt = Now() + static_cast<int>( longest * 1000.0f );
}

/* ---- the menu stacks -------------------------------------------------------- */

std::vector<int> stack;      /* pushmenu: the menu on top takes the input */
std::vector<int> hudList;    /* ui_addhud */
int lockedUntil = 0;         /* the manager's lock while menus move */
bool cursorShown = true;
float cursorX, cursorY;
bool pausedByMenu = false;

int hudHealth = -1, hudAmmo = -1, hudCompass = -1, hudWeapons = -1, hudItems = -1, crosshairMenu = -1;
int mainMenu = -1;
bool hudOn = false;
int weaponsHudTime = 0, itemsHudTime = 0;
int lastOwned = 0, lastEquipped = 0;

/* The scoreboard the client game fills (UIListCtrl "scoreboardlist"). */
struct ScoreRow {
	std::string cells[8];
	bool colored = false, header = false;
	Color text, back;
};
struct Scoreboard {
	std::string menuName;
	int menu = -1;
	bool shown = false, made = false, drawHeader = true;
	float rect[4] = {};
	Color back = kBlack, text = kWhite;
	std::vector<std::pair<std::string, float>> columns;
	std::vector<ScoreRow> rows;
} scoreboard;

/* The loading screens: the map's own menu or loading_default, up from the
 * connection to the first frame, then either gone or, on a local
 * single-player map whose menu has one, waiting for its Continue button. */
struct Loading {
	bool active = false, waiting = false;
	int menu = -1;
	std::string mapName;
} loading;

bool Locked() { return lockedUntil && Now() < lockedUntil; }

void Lock( float seconds ) {
	lockedUntil = seconds > 0.0f ? Now() + static_cast<int>( seconds * 1000.0f ) : 0;
}

int CurrentMenu() {
	return stack.empty() ? -1 : stack.back();
}

void SetPause( bool pause ) {
	if ( pause == pausedByMenu ) {
		return;
	}
	pausedByMenu = pause;
	Cvar_Set( "cl_paused", pause ? "1" : "0" );
}

/* A fullscreen menu over a local game stops it (Com_FakePause in UI_Update). */
void UpdatePause() {
	const int current = CurrentMenu();
	SetPause( current >= 0 && menus[current].fullscreen && cls.state == CA_ACTIVE && com_sv_running
		&& com_sv_running->integer );
}

/* Whether a `dialog` box or a picker is up (below). */
bool DialogOpen();
bool PickerOpen();

/* UI_FocusMenuIfExists: a menu takes the keys and the mouse; without one the
 * view has them back. A dialog box or a picker keeps them while it is up. */
void FocusMenu() {
	if ( CurrentMenu() >= 0 || DialogOpen() || PickerOpen() ) {
		Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
	} else {
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	}
	UpdatePause();
}

void PassToWidget( int menu, const char *widgetName, const char *event ) {
	if ( menu < 0 ) {
		return;
	}
	for ( Widget &w : menus[menu].widgets ) {
		if ( !Q_stricmp( w.name.c_str(), widgetName ) ) {
			ApplyProperty( w, event, {} );
			return;
		}
	}
}

/* UI_MainMenuWidgetsUpdate and UI_MultiplayerMenuWidgetsUpdate: "backtogame"
 * and "disconnect" only once a game is up; a new server, or a map change for
 * a local multiplayer one; joining only while neither serving nor playing. */
void UpdateMenuWidgets( int menu ) {
	if ( menu < 0 ) {
		return;
	}
	const bool inGame = cls.state > CA_PRIMED;
	if ( !Q_stricmp( menus[menu].name.c_str(), "main" ) ) {
		PassToWidget( menu, "backtogame", inGame ? "enable" : "disable" );
	} else if ( !Q_stricmp( menus[menu].name.c_str(), "multiplayer" ) ) {
		const bool serving = com_sv_running && com_sv_running->integer;
		PassToWidget( menu, "startnew", serving ? "disable" : "enable" );
		PassToWidget( menu, "changemap",
			serving && !( inGame && !Cvar_VariableIntegerValue( "cg_gametype" ) ) ? "enable" : "disable" );
		PassToWidget( menu, "disconnect", inGame ? "enable" : "disable" );
		PassToWidget( menu, "joinlan", serving || inGame ? "disable" : "enable" );
		PassToWidget( menu, "joininternet", serving || inGame ? "disable" : "enable" );
	}
}

/* MenuManager::PushMenu: the head moves out, the new menu in after it, and
 * the same menu is never pushed twice in a row. */
bool PushMenu( const char *name ) {
	if ( Locked() ) {
		return false;
	}
	const int index = FindMenu( name );
	if ( index < 0 ) {
		Com_Printf( "Couldn't find menu %s\n", name );
		return false;
	}
	if ( CurrentMenu() == index ) {
		return false;
	}
	Menu &m = menus[index];
	SaveCvars( m );
	float outTime = 0.0f;
	if ( !stack.empty() ) {
		Menu &head = menus[stack.back()];
		outTime = MaxMotionTime( head );
		HideMenu( head, false );
	}
	if ( outTime <= 0.0f ) {
		ShowMenu( m );
	} else {
		m.showAt = Now() + static_cast<int>( outTime * 1000.0f );
	}
	Lock( MaxMotionTime( m ) + outTime );
	stack.push_back( index );
	UpdateMenuWidgets( index );
	return true;
}

void PopMenu( bool restoreCvars ) {
	if ( Locked() || stack.empty() ) {
		return;
	}
	Menu &top = menus[stack.back()];
	stack.pop_back();
	if ( restoreCvars ) {
		RestoreCvars( top );
	}
	const float outTime = MaxMotionTime( top );
	HideMenu( top, false );
	if ( !stack.empty() ) {
		Menu &head = menus[stack.back()];
		if ( outTime <= 0.0f ) ShowMenu( head );
		else head.showAt = Now() + static_cast<int>( outTime * 1000.0f );
		UpdateMenuWidgets( stack.back() );
	}
	Lock( outTime );
}

/* MenuManager::ForceMenu: the head hides at once and the menu is pushed. */
bool ForceMenu( int index ) {
	if ( index < 0 || CurrentMenu() == index ) {
		return false;
	}
	SaveCvars( menus[index] );
	if ( !stack.empty() ) {
		ForceHide( menus[stack.back()] );
	}
	ShowMenu( menus[index] );
	stack.push_back( index );
	return true;
}

/* MenuManager::ClearMenus: the stacks. A menu `showmenu` or the client game
 * showed is on neither and stays up: a briefing shows its first slide as its
 * level spawns, before the level's loading screen is done with. Those end
 * with their level (HideLevelMenus). */
void ClearMenus( bool force ) {
	if ( Locked() && !force ) {
		return;
	}
	for ( int index : stack ) HideMenu( menus[index], force );
	stack.clear();
	lockedUntil = 0;
}

void ForceMenuOff( bool force ) {
	ClearMenus( force );
	FocusMenu();
}

/* UI_MenuEscape. */
void MenuEscape( const char *name ) {
	if ( loading.active ) {
		return;
	}
	const int current = CurrentMenu();
	if ( current >= 0 && current == mainMenu && cls.state == CA_DISCONNECTED ) {
		if ( Cvar_VariableIntegerValue( "developer" ) ) {
			Con_ToggleConsole_f();
		}
		return;
	}
	if ( current >= 0 ) {
		PopMenu( true );
	} else if ( !Q_stricmp( name, "main" ) && cls.state > CA_PRIMED && Cvar_VariableIntegerValue( "cg_gametype" ) > 0 ) {
		PushMenu( "dm_main" );
	} else {
		PushMenu( name );
	}
	FocusMenu();
}

/* ---- where things are ----------------------------------------------------- */

float MotionFraction( int start, float seconds ) {
	if ( !( seconds > 0.0f ) ) {
		return 1.0f;
	}
	const float frac = static_cast<float>( Now() - start ) / ( seconds * 1000.0f );
	return frac < 0.0f ? 0.0f : ( frac > 1.0f ? 1.0f : frac );
}

/* A frame sliding in along `direction` towards (x, y), `frac` of the way. */
void Slide( urc::Direction direction, float frac, float frame[4] ) {
	switch ( direction ) {
	case urc::Direction::FromLeft: frame[0] = ( frame[0] + frame[2] ) * frac - frame[2]; break;
	case urc::Direction::FromRight: frame[0] = VidWidth() - ( VidWidth() - frame[0] ) * frac; break;
	case urc::Direction::FromBottom: frame[1] = VidHeight() - ( VidHeight() - frame[1] ) * frac; break;
	case urc::Direction::FromTop: frame[1] = ( frame[1] + frame[3] ) * frac - frame[3]; break;
	default: break;
	}
}

/* The container's frame: its size in the menu's scale, placed by `align`,
 * centred on an axis it does not align (UIWidgetContainer::AlignPosition). */
void PlaceMenu( Menu &m ) {
	const Scale s = m.virtualRes ? VirtualScale() : HudScale();
	float *f = m.frame;
	f[2] = m.width * s.x;
	f[3] = m.height * s.y;
	if ( m.align & AlignLeft ) f[0] = 0.0f;
	else if ( m.align & AlignRight ) f[0] = VidWidth() - f[2];
	else f[0] = ( VidWidth() - f[2] ) * 0.5f;
	if ( m.align & AlignBottom ) f[1] = VidHeight() - f[3];
	else if ( m.align & AlignTop ) f[1] = 0.0f;
	else f[1] = ( VidHeight() - f[3] ) * 0.5f;

	float frac = MotionFraction( m.motionStart, m.motionTime );
	if ( m.motionOut ) frac = 1.0f - frac;
	if ( m.direction != urc::Direction::None ) Slide( m.direction, frac, f );

	m.alpha = 1.0f;
	if ( m.fadeTime > 0.0f ) {
		const float fade = MotionFraction( m.motionStart, m.fadeTime );
		m.alpha = m.motionOut ? 1.0f - fade : fade;
	}
}

void PlaceWidget( const Menu &m, Widget &w ) {
	w.scale = w.virtualRes ? VirtualScale() : HudScale();
	float *f = w.frame;
	f[0] = w.rect[0] * w.scale.x;
	f[1] = w.rect[1] * w.scale.y;
	f[2] = w.rect[2] * w.scale.x;
	f[3] = w.rect[3] * w.scale.y;
	if ( w.stretchVertical ) {
		f[1] = 0.0f;
		f[3] = VidHeight();
	}
	if ( w.stretchHorizontal ) {
		f[0] = 0.0f;
		f[2] = VidWidth();
	}
	if ( w.align & AlignCenterX ) f[0] = ( VidWidth() - f[2] ) * 0.5f;
	if ( w.align & AlignRight ) f[0] = VidWidth() - f[2];
	else if ( w.align & AlignLeft ) f[0] = 0.0f;
	if ( w.align & AlignCenterY ) f[1] = ( VidHeight() - f[3] ) * 0.5f;
	if ( w.align & AlignBottom ) f[1] = VidHeight() - f[3];
	else if ( w.align & AlignTop ) f[1] = 0.0f;
	if ( !w.screenPlaced ) {
		f[0] += m.frame[0];
		f[1] += m.frame[1];
	}

	float frac = MotionFraction( m.motionStart, w.motionTime );
	if ( m.motionOut ) frac = 1.0f - frac;
	if ( w.direction != urc::Direction::None ) Slide( w.direction, frac, f );

	w.alpha = 1.0f;
	if ( w.fadeTime > 0.0f ) {
		const float fade = MotionFraction( m.motionStart, w.fadeTime );
		w.alpha = m.motionOut ? 1.0f - fade : fade;
	}
}

bool Inside( const float f[4], float x, float y ) {
	return x >= f[0] && y >= f[1] && x < f[0] + f[2] && y < f[1] + f[3];
}

/* ---- FakkLabel's bars ------------------------------------------------------ */

/* The flash a bar gives when its value changes, fading over a second and a half. */
float BarFlash( Widget &w, float frac ) {
	if ( frac != w.lastFrac ) {
		w.flashTime = Now();
	}
	w.lastFrac = frac;
	const float alpha = 1.0f - static_cast<float>( Now() - w.flashTime ) / 1500.0f;
	return alpha < 0.0f ? 0.0f : ( alpha > 1.0f ? 1.0f : alpha );
}

void DrawBarPicture( Widget &w, Material &material, float x, float y, float width, float height, float s0, float t0,
	float s1, float t1 ) {
	DrawPic( w.frame[0] + x, w.frame[1] + y, width, height, s0, t0, s1, t1, material.Get() );
}

/* The straight bars (UIFakkLabel::DrawStatbar). A tiled bar repeats its
 * picture at the picture's size in the widget's scale and is cut at the
 * frame; a stretched one shows the part of its picture the value covers. */
void DrawStraightBar( Widget &w, float frac, float widgetAlpha ) {
	if ( frac <= 0.0f ) {
		return;
	}
	const float flash = BarFlash( w, frac );
	const float fw = w.frame[2], fh = w.frame[3];
	const Color flashColor = { 1.0f, 1.0f, 1.0f, flash };

	if ( !w.barMaterial.Named() ) {
		SetColor( w.foreground, widgetAlpha );
		if ( w.bar == Bar::Horizontal ) Fill( w.frame[0], w.frame[1], frac * fw, fh );
		else if ( w.bar != Bar::VerticalReverse ) Fill( w.frame[0], w.frame[1] + fh * ( 1.0f - frac ), fw, fh * frac );
		return;
	}

	const auto draw = [&]( Material &material ) {
		float sw, sh;
		ShaderSize( material.Get(), sw, sh );
		const float tilesX = fw / w.scale.x / sw, tilesY = fh / w.scale.y / sh;
		if ( w.bar == Bar::Horizontal ) {
			if ( w.tile ) DrawBarPicture( w, material, 0.0f, 0.0f, frac * fw, fh, 0.0f, 0.0f, tilesX * frac, tilesY );
			else DrawBarPicture( w, material, 0.0f, 0.0f, frac * fw, fh, 0.0f, 0.0f, 1.0f, 1.0f );
		} else if ( w.bar == Bar::VerticalReverse && !w.tile ) {
			DrawBarPicture( w, material, 0.0f, 0.0f, fw, fh * frac, 0.0f, 0.0f, 1.0f, frac );
		} else if ( w.tile ) {
			DrawBarPicture( w, material, 0.0f, fh * ( 1.0f - frac ), fw, fh * frac, 0.0f, 0.0f, tilesX, tilesY * frac );
		} else {
			DrawBarPicture( w, material, 0.0f, fh * ( 1.0f - frac ), fw, fh * frac, 0.0f, 1.0f - frac, 1.0f, 1.0f );
		}
	};

	SetColor( kWhite, widgetAlpha );
	draw( w.barMaterial );
	if ( flash > 0.0f && w.barFlash.Named() ) {
		SetColor( flashColor, widgetAlpha );
		draw( w.barFlash );
	}
	if ( w.barMarker.Named() && w.bar != Bar::VerticalReverse ) {
		float mw, mh;
		ShaderSize( w.barMarker.Get(), mw, mh );
		SetColor( kWhite, widgetAlpha );
		DrawBarPicture( w, w.barMarker, fw * frac - mw * 0.5f, fh * 0.5f - mh * 0.5f, mw, mh, 0.0f, 0.0f, 1.0f, 1.0f );
	}
}

/* A picture turned `degrees` clockwise about the frame's centre
 * (UIFakkLabel::DrawStatSpinner and DrawStatCompass). */
void DrawTurned( Widget &w, Material &material, float degrees ) {
	const float radians = DEG2RAD( degrees );
	const float s = std::sin( radians ), c = std::cos( radians );
	const float cx = w.frame[2] * 0.5f, cy = w.frame[3] * 0.5f;
	const float ox = w.frame[0], oy = w.frame[1];
	const float xy[4][2] = {
		{ ox + cx + s * cx - c * cy, oy + cy - c * cy - s * cx },
		{ ox + cx + s * cx + c * cy, oy + cy - c * cy + s * cx },
		{ ox + cx - s * cx + c * cy, oy + cy + c * cy + s * cx },
		{ ox + cx - s * cx - c * cy, oy + cy + c * cy - s * cx },
	};
	const float st[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
	DrawQuadPic( xy, st, material.Get() );
}

/* The compass face turns against the view's heading from north, with a
 * needle's lag and overshoot (UIFakkLabel::DrawStatCompass). */
struct CompassMotion {
	int lastTime = -9999;
	float lastYaw = 0.0f, lastYawDelta = 0.0f;
	float needleOffset = 0.0f, needleSpeed = 0.0f;
} compassMotion;

float CompassHeading() {
	float view[3];
	CL_MOHAACGameEyeAngles( view );
	const float yaw = view[YAW] - static_cast<float>( Stat( kStatCompassNorth ) ) / 182.0f;
	CompassMotion &cm = compassMotion;
	int elapsed = Now() - cm.lastTime;
	if ( elapsed <= 1000 && elapsed > 0 ) {
		const float delta = AngleSubtract( yaw, cm.lastYaw );
		if ( std::fabs( delta ) > 0.1f ) {
			cm.needleOffset -= delta;
		}
		while ( elapsed > 0 ) {
			const int step = std::min( elapsed, 15 );
			elapsed -= step;
			const float dt = static_cast<float>( step );
			if ( std::fabs( cm.needleOffset ) < 0.1f && std::fabs( cm.needleSpeed ) < 0.01f ) {
				cm.needleOffset = cm.needleSpeed = 0.0f;
				break;
			}
			cm.needleOffset += dt * cm.needleSpeed;
			if ( cm.needleOffset > 180.0f ) cm.needleOffset -= 360.0f;
			else if ( cm.needleOffset < -180.0f ) cm.needleOffset += 360.0f;
			if ( cm.needleOffset > 0.0f ) cm.needleSpeed -= dt * 0.00175f;
			else if ( cm.needleOffset < 0.0f ) cm.needleSpeed += dt * 0.00175f;
			cm.needleSpeed -= cm.needleSpeed * 0.0025f * dt;
			if ( cm.needleSpeed > 0.0f ) cm.needleSpeed = std::max( 0.0f, cm.needleSpeed - dt * 0.00035f );
			else cm.needleSpeed = std::min( 0.0f, cm.needleSpeed + dt * 0.00035f );
		}
		cm.lastYawDelta = delta;
	} else if ( elapsed > 1000 ) {
		cm.needleOffset = cm.needleSpeed = cm.lastYawDelta = 0.0f;
	}
	cm.lastYaw = yaw;
	cm.lastTime = Now();
	return AngleMod( cm.needleOffset + yaw );
}

/* The texture coordinate where a ray from the centre at `degrees` leaves the
 * unit square, measured as the needle's and the rotator's are: clockwise from
 * straight up. No 1.00 menu uses a circular bar; OpenMoHAA's
 * StatCircleTexCoord jumps from edge to edge at each corner, so the ray is
 * followed instead. */
void CircleTexCoord( float degrees, float out[2] ) {
	const float dx = std::sin( DEG2RAD( degrees ) ), dy = -std::cos( DEG2RAD( degrees ) );
	const float reach = std::max( std::fabs( dx ), std::fabs( dy ) );
	out[0] = 0.5f + dx * 0.5f / reach;
	out[1] = 0.5f + dy * 0.5f / reach;
}

/* A pie of the picture from the start angle to `frac` of the way to the end
 * one, cut at each quarter so every piece is a triangle from the centre. */
void DrawCircleBar( Widget &w, float frac, float widgetAlpha ) {
	const float flash = BarFlash( w, frac );
	const float start = w.angles[0];
	const float end = frac * ( w.angles[1] - start ) + start;
	const float step = end >= start ? 90.0f : -90.0f;
	float current = start;
	int guard = 0;
	while ( ( step > 0.0f ? current < end : current > end ) && guard++ < 16 ) {
		float next = step > 0.0f ? std::floor( ( current + 45.0f ) / 90.0f ) * 90.0f + 45.0f
			: std::ceil( ( current - 45.0f ) / 90.0f ) * 90.0f - 45.0f;
		if ( next == current ) next += step;
		if ( step > 0.0f ? next > end : next < end ) next = end;
		float a[2], b[2];
		CircleTexCoord( current, a );
		CircleTexCoord( next, b );
		const float st[3][2] = { { 0.5f, 0.5f }, { a[0], a[1] }, { b[0], b[1] } };
		const float xy[3][2] = {
			{ w.frame[0] + w.frame[2] * 0.5f, w.frame[1] + w.frame[3] * 0.5f },
			{ w.frame[0] + w.frame[2] * a[0], w.frame[1] + w.frame[3] * a[1] },
			{ w.frame[0] + w.frame[2] * b[0], w.frame[1] + w.frame[3] * b[1] },
		};
		SetColor( flash > 0.0f && w.barFlash.Named() ? w.foreground : kWhite, widgetAlpha );
		DrawTrianglePic( xy, st, w.barMaterial.Get() );
		if ( flash > 0.0f && w.barFlash.Named() ) {
			SetColor( { 1.0f, 1.0f, 1.0f, flash }, widgetAlpha );
			DrawTrianglePic( xy, st, w.barFlash.Get() );
		}
		current = next;
	}
}

/* A small picture carried round the frame's edge at the value's angle
 * (UIFakkLabel::DrawStatRotator). */
void DrawRotatorBar( Widget &w, float frac, float widgetAlpha ) {
	const float flash = BarFlash( w, frac );
	const float radians = DEG2RAD( frac * ( w.angles[1] - w.angles[0] ) + w.angles[0] );
	const float dirX = std::sin( radians ), dirY = -std::cos( radians );
	const float fw = w.frame[2], fh = w.frame[3];
	const float sizeX = ( fw + fh ) / fw * w.angles[2] * w.scale.x;
	const float sizeY = ( fw + fh ) / fh * w.rotatorScale * w.scale.y;
	const float cx = w.frame[0] + ( fw * 0.5f - sizeX ) * dirX + fw * 0.5f;
	const float cy = w.frame[1] + ( fh * 0.5f - sizeY ) * dirY + fh * 0.5f;
	const float xy[4][2] = { { cx - sizeX, cy - sizeY }, { cx + sizeX, cy - sizeY }, { cx + sizeX, cy + sizeY },
		{ cx - sizeX, cy + sizeY } };
	const float st[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
	SetColor( flash > 0.0f && w.barFlash.Named() ? w.foreground : kWhite, widgetAlpha );
	DrawQuadPic( xy, st, w.barMaterial.Get() );
	if ( flash > 0.0f && w.barFlash.Named() ) {
		DrawQuadPic( xy, st, w.barFlash.Get() );
	}
}

/* A needle from the centre to the edge at the value's angle
 * (UIFakkLabel::DrawStatNeedle), drawn as a quad `angles[2]` wide. */
void DrawNeedleBar( Widget &w, float frac, float widgetAlpha ) {
	const float flash = BarFlash( w, frac );
	const float radians = DEG2RAD( frac * ( w.angles[1] - w.angles[0] ) + w.angles[0] );
	const float s = std::sin( radians ), c = std::cos( radians );
	const float cx = w.frame[0] + w.frame[2] * 0.5f, cy = w.frame[1] + w.frame[3] * 0.5f;
	const float tipX = cx + s * w.frame[2] * 0.5f, tipY = cy - c * w.frame[3] * 0.5f;
	const float halfX = w.angles[2] * c * 0.5f, halfY = w.angles[2] * s * 0.5f;
	const float xy[4][2] = { { tipX - halfX, tipY - halfY }, { tipX + halfX, tipY + halfY },
		{ cx + halfX, cy + halfY }, { cx - halfX, cy - halfY } };
	const float st[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
	SetColor( flash > 0.0f ? w.foreground : kWhite, widgetAlpha );
	DrawQuadPic( xy, st, w.barMaterial.Get() );
	if ( flash > 0.0f && w.barFlash.Named() ) {
		SetColor( { 1.0f, 1.0f, 1.0f, flash }, widgetAlpha );
		DrawQuadPic( xy, st, w.barFlash.Get() );
	}
}

void DrawBar( Widget &w, float frac, float widgetAlpha ) {
	switch ( w.bar ) {
	case Bar::Circular: DrawCircleBar( w, frac, widgetAlpha ); return;
	case Bar::Needle: DrawNeedleBar( w, frac, widgetAlpha ); return;
	case Bar::Rotator: DrawRotatorBar( w, frac, widgetAlpha ); return;
	case Bar::Compass:
		SetColor( kWhite, widgetAlpha );
		DrawTurned( w, w.barMaterial, CompassHeading() );
		return;
	case Bar::Spinner:
	case Bar::HeadingSpinner: {
		const float flash = BarFlash( w, frac );
		if ( w.bar == Bar::HeadingSpinner ) {
			/* The value is a heading; the spinner shows it from the view's. */
			float view[3];
			CL_MOHAACGameEyeAngles( view );
			frac = ( AngleSubtract( view[YAW], frac * 360.0f + 180.0f ) + 180.0f ) / 360.0f;
			if ( frac < 0.0f || frac > 1.0f ) frac = 0.0f;
		}
		const float degrees = frac * ( w.angles[1] - w.angles[0] ) + w.angles[0];
		SetColor( flash > 0.0f && w.barFlash.Named() ? w.foreground : kWhite, widgetAlpha );
		DrawTurned( w, w.barMaterial, degrees );
		if ( flash > 0.0f && w.barFlash.Named() ) {
			SetColor( { 1.0f, 1.0f, 1.0f, flash }, widgetAlpha );
			DrawTurned( w, w.barFlash, degrees );
		}
		return;
	}
	default:
		DrawStraightBar( w, frac, widgetAlpha );
		return;
	}
}

/* ---- each widget's own drawing ---------------------------------------------- */

void PrintWidgetText( Widget &w, const std::string &text, float alpha ) {
	if ( text.empty() ) {
		return;
	}
	SetColor( w.foreground, alpha );
	PrintJustified( w.font.Get(), w.frame, w.horizontal, w.vertical, text, w.scale );
}

/* UILabel::Draw: its cvar's value, mapped through `linkstring`, or its title;
 * `linkcvartoshader` shows the picture the cvar names instead. */
void DrawLabelText( Widget &w, float alpha ) {
	std::string text;
	if ( !w.cvar.empty() ) {
		text = CvarString( w.cvar );
		if ( !w.linkStrings.empty() ) {
			const std::string value = LatchedCvarString( w.cvar );
			for ( const auto &link : w.linkStrings ) {
				if ( link.first == value ) {
					text = link.second;
					break;
				}
			}
		}
		if ( w.linkCvarToShader ) {
			if ( Q_stricmp( w.currentShader.c_str(), text.c_str() ) ) {
				w.currentShader = text;
				w.material.Set( text );
			}
			text = w.title;
		}
	} else {
		text = w.title;
	}
	PrintWidgetText( w, text, alpha );
}

/* A `rendermodel` label (UIFakkLabel::DrawModel and CL_Draw3DModel,
 * reference only): the model its linked cvar names, in a 30-degree view of
 * no world over the label's frame. The model stands as a 96-unit figure
 * would, as far off as its height and the label's scale say, moved by the
 * label's offset and by its rotate offset turned with it, and plays the
 * label's animation, from its start again whenever it ends. */
/* ---- the inventory's models ------------------------------------------------ */

/* An item as global/inventory.txt shows it in the HUD (OpenMoHAA cl_inv.cpp,
 * reference only): its model, the angles it is held at, how it bobs or spins
 * through its angle deltas, and where it sits. Only the members a label
 * draws with are kept. */
struct InventoryItem {
	std::string name, model;
	bool bob = false, spin = false;
	float angles[3] = {}, angleDeltas[3] = {}, offset[3] = {}, rotateOffset[3] = {};
	float scale = 1.0f;
};

std::vector<InventoryItem> inventory;
bool inventoryRead;

void ReadVector( const char **text, float out[3] ) {
	const char *token = COM_ParseExt( text, qfalse );
	if ( std::sscanf( token, "%f %f %f", &out[0], &out[1], &out[2] ) != 3 ) {
		out[0] = out[1] = out[2] = 0.0f;
	}
}

void ReadInventory() {
	inventoryRead = true;
	inventory.clear();
	char *buffer = nullptr;
	if ( FS_ReadFile( "global/inventory.txt", reinterpret_cast<void **>( &buffer ) ) <= 0 || !buffer ) {
		return;
	}
	const char *text = buffer;
	InventoryItem *item = nullptr;
	for ( ;; ) {
		const char *token = COM_ParseExt( &text, qtrue );
		if ( !token[0] ) {
			break;
		}
		if ( !Q_stricmp( token, "item" ) ) {
			inventory.emplace_back();
			item = &inventory.back();
			item->name = COM_ParseExt( &text, qfalse );
		} else if ( !Q_stricmp( token, "}" ) ) {
			item = nullptr;
		} else if ( item && !Q_stricmp( token, "hudmodel" ) ) {
			item->model = COM_ParseExt( &text, qfalse );
		} else if ( item && !Q_stricmp( token, "hudmove" ) ) {
			const char *move = COM_ParseExt( &text, qfalse );
			item->bob = !Q_stricmp( move, "bob" );
			item->spin = !Q_stricmp( move, "spin" );
		} else if ( item && !Q_stricmp( token, "hudangles" ) ) {
			ReadVector( &text, item->angles );
		} else if ( item && !Q_stricmp( token, "hudangledeltas" ) ) {
			ReadVector( &text, item->angleDeltas );
		} else if ( item && !Q_stricmp( token, "hudoffset" ) ) {
			ReadVector( &text, item->offset );
		} else if ( item && !Q_stricmp( token, "hudrotateoffset" ) ) {
			ReadVector( &text, item->rotateOffset );
		} else if ( item && !Q_stricmp( token, "hudscale" ) ) {
			item->scale = static_cast<float>( std::atof( COM_ParseExt( &text, qfalse ) ) );
		}
		SkipRestOfLine( &text );
	}
	FS_FreeFile( buffer );
}

const InventoryItem *FindInventoryItem( const char *name ) {
	if ( !inventoryRead ) {
		ReadInventory();
	}
	for ( const InventoryItem &item : inventory ) {
		if ( name && !Q_stricmp( item.name.c_str(), name ) ) {
			return &item;
		}
	}
	return nullptr;
}

/* A model in a label's frame, as UIFakkLabel::DrawModel places it for
 * CL_Draw3DModel: centred on its box, as far back as its largest side times
 * its scale needs in the 30-degree view, then moved by its offset and turned
 * about its rotate offset. A player model's box is the standing figure's. */
void DrawModelInFrame( Widget &w, const char *name, const float angles[3], const float offset[3],
	const float rotateOffset[3], float scale, bool standingBox ) {
	qhandle_t renderer = 0;
	void *definition = nullptr;
	if ( !name || !name[0] || !CL_MOHAAUIModel( name, &renderer, &definition ) ) {
		return;
	}
	dtiki_t *tiki = static_cast<dtiki_t *>( definition );
	vec3_t mins = { -16.0f, -16.0f, 0.0f }, maxs = { 16.0f, 16.0f, 96.0f };
	if ( !standingBox ) {
		if ( tiki ) {
			TIKI_CalculateBounds( tiki, 1.0f, mins, maxs );
		} else {
			re.ModelBounds( renderer, mins, maxs );
		}
	}
	float size = maxs[2] - mins[2];
	size = std::max( size, maxs[1] - mins[1] );
	size = std::max( size, maxs[0] - mins[0] );

	refdef_t view{};
	view.x = static_cast<int>( w.frame[0] );
	view.y = static_cast<int>( w.frame[1] );
	view.width = static_cast<int>( w.frame[2] );
	view.height = static_cast<int>( w.frame[3] );
	if ( view.width <= 0 || view.height <= 0 ) {
		return;
	}
	view.fov_x = 30.0f;
	view.fov_y = 30.0f;
	view.rdflags = RDF_NOWORLDMODEL;
	view.time = cls.realtime;
	AxisClear( view.viewaxis );

	refEntity_t ent{};
	ent.reType = RT_MODEL;
	ent.hModel = renderer;
	vec3_t axis[3];
	AnglesToAxis( angles, axis );
	for ( int i = 0; i < 3; i++ ) {
		VectorCopy( axis[i], ent.axis[i] );
	}
	ent.origin[0] = size * scale * 0.5f / 0.268f + offset[0];
	ent.origin[1] = ( mins[1] + maxs[1] ) * 0.5f + offset[1];
	ent.origin[2] = ( mins[2] + maxs[2] ) * -0.5f + offset[2];
	for ( int i = 0; i < 3; i++ ) {
		ent.origin[i] += rotateOffset[0] * axis[0][i] + rotateOffset[1] * axis[1][i]
			+ rotateOffset[2] * axis[2][i];
	}
	VectorCopy( ent.origin, ent.oldorigin );
	VectorCopy( ent.origin, ent.lightingOrigin );
	ent.shader.rgba[0] = ent.shader.rgba[1] = ent.shader.rgba[2] = ent.shader.rgba[3] = 255;

	ent.mohaa.pose.tiki = tiki;
	ent.mohaa.pose.entityNumber = ENTITYNUM_NONE;
	for ( int i = 0; i < 5; i++ ) {
		ent.mohaa.pose.boneTag[i] = -1;
		ent.mohaa.pose.boneQuat[i][3] = 1.0f;
	}
	ent.mohaa.scale = 1.0f;
	ent.mohaa.lod = -1.0f;
	if ( tiki && !w.modelAnim.empty() ) {
		if ( w.poseIndex < 0 || Q_stricmp( w.poseAnim.c_str(), w.modelAnim.c_str() ) ) {
			w.poseAnim = w.modelAnim;
			w.poseIndex = TIKI_Anim_NumForName( tiki, w.modelAnim.c_str() );
			w.poseTime = 0.0f;
			w.poseUpdated = Now();
		} else {
			w.poseTime += static_cast<float>( Now() - w.poseUpdated ) * 0.001f;
			w.poseUpdated = Now();
			if ( w.poseIndex >= 0 && TIKI_Anim_Time( tiki, w.poseIndex ) < w.poseTime ) {
				w.poseTime = 0.0f;
			}
		}
		if ( w.poseIndex >= 0 ) {
			ent.mohaa.pose.frameInfo[0].index = w.poseIndex;
			ent.mohaa.pose.frameInfo[0].time = w.poseTime;
			ent.mohaa.pose.frameInfo[0].weight = 1.0f;
			ent.mohaa.pose.actionWeight = 1.0f;
		}
	}

	re.ClearScene();
	re.AddRefEntityToScene( &ent, qfalse );
	re.RenderScene( &view );
}

/* A `rendermodel` label: the model its cvar names, a standing figure. */
void DrawLabelModel( Widget &w ) {
	if ( w.cvar.empty() ) {
		return;
	}
	const std::string name = CvarString( w.cvar );
	DrawModelInFrame( w, name.c_str(), w.modelAngles, w.modelOffset, w.modelRotateOffset, w.modelScale, true );
}

/* An `invmodelhand` label: the item in that hand (the player state's active
 * item of that index, named by its configstring), held at its angles and
 * bobbing or spinning through its deltas over 600-millisecond steps. The
 * base is the item's own `hudangles` [I]: OpenMoHAA starts from the deltas. */
void DrawInventoryModel( Widget &w ) {
	const int item = ActiveItem( w.invModelHand );
	if ( item <= 0 || item >= 64 ) {
		return;
	}
	const InventoryItem *entry = FindInventoryItem( ConfigString( kCsWeapons + item ) );
	if ( !entry || entry->model.empty() ) {
		return;
	}
	const float steps = static_cast<float>( Now() ) / 600.0f;
	float angles[3];
	for ( int i = 0; i < 3; i++ ) {
		angles[i] = entry->angles[i];
		if ( entry->bob ) angles[i] += std::sin( steps ) * entry->angleDeltas[i];
		else if ( entry->spin ) angles[i] += steps * entry->angleDeltas[i];
	}
	DrawModelInFrame( w, entry->model.c_str(), angles, entry->offset, entry->rotateOffset, entry->scale, false );
}

/* UIFakkLabel::Draw. */
void DrawFakkLabel( Widget &w, float alpha ) {
	if ( w.statAlpha != -1 ) {
		const float frac = static_cast<float>( Stat( w.statAlpha ) + 1 ) / 100.0f;
		w.alpha = frac < 0.0f ? 0.0f : ( frac > 1.0f ? 1.0f : frac );
	}
	if ( w.stat == -1 && w.itemStat == -1 && w.invModelHand == -1 && w.statConfigstring == -1 && !w.renderModel
		&& w.bar == Bar::None && w.modelName.empty() ) {
		DrawLabelText( w, alpha );
		return;
	}
	if ( w.statConfigstring != -1 ) {
		PrintWidgetText( w, ConfigString( Stat( w.statConfigstring ) ), alpha );
		return;
	}
	if ( w.stat != -1 ) {
		int value = Stat( w.stat );
		if ( w.bar == Bar::None ) {
			PrintWidgetText( w, va( "%d", value ), alpha );
			return;
		}
		if ( w.bar == Bar::StaggerEven || w.bar == Bar::StaggerOdd ) {
			/* A clip of two-round rows shows whole rows only. */
			const int full = w.maxStat < 0 ? static_cast<int>( w.barMax ) : Stat( w.maxStat );
			int missing = full - value;
			if ( missing & 1 ) missing += w.bar == Bar::StaggerEven ? -1 : 1;
			value = full - missing;
			if ( w.maxStat >= 0 || w.barMin <= static_cast<float>( value ) ) value = std::max( value, 0 );
			else value = static_cast<int>( w.barMin );
		}
		float frac;
		if ( w.maxStat >= 0 ) {
			const int max = Stat( w.maxStat );
			frac = max ? static_cast<float>( value ) / static_cast<float>( max ) : 0.0f;
		} else {
			const float range = w.barMax - w.barMin;
			frac = range != 0.0f ? static_cast<float>( value ) / range : 0.0f;
		}
		DrawBar( w, frac < 0.0f ? 0.0f : ( frac > 1.0f ? 1.0f : frac ), alpha );
		return;
	}
	if ( w.itemStat != -1 ) {
		const int item = ActiveItem( w.itemStat );
		if ( item >= 0 && item < 64 ) {
			PrintWidgetText( w, ConfigString( kCsWeapons + item ), alpha );
		}
		return;
	}
	if ( w.invModelHand == -1 && w.modelName.empty() && !w.renderModel ) {
		if ( w.bar == Bar::None ) {
			return;
		}
		float frac = 0.0f;
		const float range = w.barMax - w.barMin;
		if ( !w.cvar.empty() && range != 0.0f ) {
			frac = Cvar_VariableValue( w.cvar.c_str() ) / range;
		}
		DrawBar( w, frac < 0.0f ? 0.0f : ( frac > 1.0f ? 1.0f : frac ), alpha );
	}
	if ( w.renderModel ) {
		DrawLabelModel( w );
	} else if ( w.invModelHand != -1 ) {
		DrawInventoryModel( w );
	}
}

void DrawButton( Widget &w, float alpha ) {
	const bool down = w.pressed && Inside( w.frame, cursorX, cursorY );
	float frame[4] = { w.frame[0], w.frame[1], w.frame[2], w.frame[3] };
	if ( down ) {
		if ( w.borderStyle != BorderNone ) Draw3DBox( frame[0], frame[1], frame[2], frame[3], true, w.border, alpha );
		frame[0] += 1.0f;
		frame[1] += 1.0f;
	}
	if ( w.title.empty() ) {
		return;
	}
	SetColor( w.foreground, alpha );
	PrintJustified( w.font.Get(), frame, w.horizontal, w.vertical, w.title, w.scale );
}

bool Checked( const Widget &w ) {
	return !w.cvar.empty() && Cvar_VariableIntegerValue( w.cvar.c_str() ) != 0;
}

/* UICheckBox::Draw: the box 14 units square, eight in, the title after it. */
void DrawCheckBox( Widget &w, float alpha ) {
	const bool checked = Checked( w );
	if ( !w.pressed ) Draw3DBox( w.frame[0], w.frame[1], w.frame[2], w.frame[3], false, w.border, alpha );
	const float bw = 14.0f * w.scale.x, bh = 14.0f * w.scale.y;
	const float x = w.frame[0] + 8.0f * w.scale.x, y = w.frame[1] + ( w.frame[3] - bh ) * 0.5f;
	Material &picture = checked ? w.checkedMaterial : w.uncheckedMaterial;
	SetColor( w.foreground, alpha );
	if ( picture.Named() && picture.Get() ) {
		DrawPic( x, y, bw, bh, 0.0f, 0.0f, 1.0f, 1.0f, picture.Get() );
	} else {
		DrawBox( x, y, bw, bh, checked ? Color{ 0.0f, 0.0f, 1.0f, 1.0f } : kBlack, alpha );
	}
	void *font = w.font.Get();
	if ( !w.title.empty() && font ) {
		SetColor( w.foreground, alpha );
		const float titleX = w.frame[0] / w.scale.x + 8.0f * 1.5f + 14.0f;
		const float titleY = w.frame[1] / w.scale.y + ( w.frame[3] / w.scale.y - FontHeight( font ) ) * 0.5f - 1.0f;
		Print( font, titleX, titleY, w.title.c_str(), w.scale );
	}
}

/* UISlider: arrows fifteen units wide at each end, a thumb ten wide between. */
struct SliderGeometry {
	float arrow, thumb, minX, maxX;
};

SliderGeometry SliderShape( const Widget &w ) {
	SliderGeometry g;
	g.arrow = 15.0f * w.scale.x;
	g.thumb = 10.0f * w.scale.x;
	g.minX = w.frame[0] + g.arrow + g.thumb * 0.5f;
	g.maxX = w.frame[0] + w.frame[2] - g.arrow - g.thumb * 0.5f;
	return g;
}

Material sliderLeft, sliderLeftDown, sliderRight, sliderRightDown, sliderThumb, sliderThumbDown;
Material listLeft, listRight, cursorMaterial;

void DrawSlider( Widget &w, float alpha ) {
	const float value = w.cvar.empty() ? w.sliderMin : Cvar_VariableValue( w.cvar.c_str() );
	const float range = w.sliderMax - w.sliderMin;
	float pos = range != 0.0f ? ( value - w.sliderMin ) / range : 0.0f;
	pos = pos < 0.0f ? 0.0f : ( pos > 1.0f ? 1.0f : pos );
	const SliderGeometry g = SliderShape( w );
	const float x = w.frame[0] + ( g.maxX - g.minX ) * pos + g.arrow;
	SetColor( kWhite, alpha );
	DrawPic( x, w.frame[1], g.thumb, w.frame[3], 0.0f, 0.0f, 1.0f, 1.0f,
		( w.thumbDown ? sliderThumbDown : sliderThumb ).Get() );
	DrawPic( w.frame[0], w.frame[1], g.arrow, w.frame[3], 0.0f, 0.0f, 1.0f, 1.0f,
		( w.prevDown ? sliderLeftDown : sliderLeft ).Get() );
	DrawPic( w.frame[0] + w.frame[2] - g.arrow, w.frame[1], g.arrow, w.frame[3], 0.0f, 0.0f, 1.0f, 1.0f,
		( w.nextDown ? sliderRightDown : sliderRight ).Get() );
}

/* UIField: the cvar's text, and a blinking cursor while it has the keys. */
int activeMenu = -1, activeWidget = -1;

bool IsActive( int menu, int widget ) {
	return menu == activeMenu && widget == activeWidget;
}

void DrawField( Widget &w, float alpha, bool active ) {
	void *font = w.font.Get();
	if ( !font ) {
		return;
	}
	const std::string text = w.cvar.empty() ? std::string() : CvarString( w.cvar );
	w.cursor = std::min( w.cursor, static_cast<int>( text.size() ) );
	const float x = w.frame[0] / w.scale.x + 2.0f;
	const float y = w.frame[1] / w.scale.y + ( w.frame[3] / w.scale.y - FontHeight( font ) ) * 0.5f;
	const float room = w.frame[2] / w.scale.x - 4.0f;
	/* Keep the cursor in view: drop characters from the left as needed. */
	std::size_t first = 0;
	while ( first < static_cast<std::size_t>( w.cursor )
		&& TextWidth( font, text.substr( first, static_cast<std::size_t>( w.cursor ) - first ).c_str() ) > room ) {
		first++;
	}
	std::string shown = text.substr( first );
	while ( !shown.empty() && TextWidth( font, shown.c_str() ) > room ) shown.pop_back();
	SetColor( w.foreground, alpha );
	Print( font, x, y, shown.c_str(), w.scale );
	if ( active && ( ( Now() / 250 ) & 1 ) ) {
		const float at = x + TextWidth( font, text.substr( first, static_cast<std::size_t>( w.cursor ) - first ).c_str() );
		Print( font, at, y, "_", w.scale );
	}
}

/* UIList: the current item between two arrows. */
void DrawList( Widget &w, float alpha ) {
	void *font = w.font.Get();
	if ( !w.cvar.empty() ) {
		const std::string value = CvarString( w.cvar );
		for ( std::size_t i = 0; i < w.items.size(); i++ ) {
			if ( w.items[i].first == value ) w.current = static_cast<int>( i ) + 1;
		}
	}
	if ( font && w.current >= 1 && w.current <= static_cast<int>( w.items.size() ) ) {
		const auto &item = w.items[static_cast<std::size_t>( w.current - 1 )];
		const std::string &text = item.second.empty() ? item.first : item.second;
		SetColor( w.foreground, alpha );
		const float frame[4] = { w.frame[0], w.frame[1], w.frame[2], w.frame[3] };
		PrintJustified( font, frame, JustCenter, JustMiddle, text, w.scale );
	}
	const float arrow = 15.0f * w.scale.x;
	Draw3DBox( w.frame[0], w.frame[1], arrow, w.frame[3], w.prevDown, w.border, alpha );
	Draw3DBox( w.frame[0] + w.frame[2] - arrow, w.frame[1], arrow, w.frame[3], w.nextDown, w.border, alpha );
	SetColor( kWhite, alpha );
	DrawPic( w.frame[0] + 2.0f, w.frame[1] + 2.0f, arrow - 4.0f, w.frame[3] - 4.0f, 0.0f, 0.0f, 1.0f, 1.0f, listLeft.Get() );
	DrawPic( w.frame[0] + w.frame[2] - arrow + 2.0f, w.frame[1] + 2.0f, arrow - 4.0f, w.frame[3] - 4.0f, 0.0f, 0.0f,
		1.0f, 1.0f, listRight.Get() );
}

/* UIListBox: the items, one a line, the selected one lit. */
float ListRowHeight( Widget &w ) {
	return FontHeight( w.font.Get() ) * w.scale.y;
}

void DrawListBox( Widget &w, float alpha ) {
	void *font = w.font.Get();
	const float row = ListRowHeight( w );
	if ( !font || row <= 0.0f ) {
		return;
	}
	float y = w.frame[1];
	for ( int i = w.top; i < static_cast<int>( w.items.size() ) && y + row <= w.frame[1] + w.frame[3]; i++ ) {
		if ( i + 1 == w.current ) DrawBox( w.frame[0], y, w.frame[2], row, { 0.21f, 0.18f, 0.015f, 1.0f }, alpha );
		SetColor( i + 1 == w.current ? Color{ 0.9f, 0.8f, 0.6f, 1.0f } : w.foreground, alpha );
		const auto &item = w.items[static_cast<std::size_t>( i )];
		Print( font, w.frame[0] / w.scale.x + 1.0f, y / w.scale.y,
			( item.second.empty() ? item.first : item.second ).c_str(), w.scale );
		y += row;
	}
}

/* UIPulldownMenuContainer: the menu's picture, and while open its popups
 * below it, one a line. */
float PopupRowHeight( Widget &w ) {
	return ( FontHeight( w.font.Get() ) + 4.0f ) * w.scale.y;
}

float PopupWidth( Widget &w ) {
	float widest = 0.0f;
	for ( const Popup &p : w.popups ) widest = std::max( widest, TextWidth( w.font.Get(), p.title.c_str() ) );
	return ( widest + 16.0f ) * w.scale.x;
}

void DrawPulldown( Widget &w, float alpha ) {
	Material &picture = w.open && w.selectedMenuMaterial.Named() ? w.selectedMenuMaterial : w.menuMaterial;
	SetColor( kWhite, alpha );
	float pw, ph;
	ShaderSize( picture.Get(), pw, ph );
	DrawPic( w.frame[0], w.frame[1], pw * w.scale.x, ph * w.scale.y, 0.0f, 0.0f, 1.0f, 1.0f, picture.Get() );
}

void DrawPopups( Widget &w, float alpha ) {
	void *font = w.font.Get();
	if ( !w.open || !font ) {
		return;
	}
	const float row = PopupRowHeight( w ), width = PopupWidth( w );
	const float x = w.frame[0], y0 = w.frame[1] + w.frame[3];
	DrawBox( x, y0, width, row * static_cast<float>( w.popups.size() ), kBlack, alpha * 0.9f );
	for ( std::size_t i = 0; i < w.popups.size(); i++ ) {
		const float y = y0 + row * static_cast<float>( i );
		const bool lit = static_cast<int>( i ) == w.popupHover;
		if ( lit ) DrawBox( x, y, width, row, { 0.21f, 0.18f, 0.015f, 1.0f }, alpha );
		SetColor( lit ? Color{ 0.9f, 0.8f, 0.6f, 1.0f } : kHudColor, alpha );
		Print( font, x / w.scale.x + 8.0f, y / w.scale.y + 2.0f, w.popups[i].title.c_str(), w.scale );
	}
	Draw3DBox( x, y0, width, row * static_cast<float>( w.popups.size() ), false, MakeBorder( kHudColor ), alpha );
}

/* FakkBindList: a header of three columns, then the actions with the first
 * two keys bound to each. */
void KeysFor( const std::string &command, int out[2] ) {
	out[0] = out[1] = -1;
	int found = 0;
	for ( int key = 0; key < MAX_KEYS && found < 2; key++ ) {
		const char *binding = Key_GetBinding( key );
		if ( binding && binding[0] && !Q_stricmp( binding, command.c_str() ) ) out[found++] = key;
	}
}

float BindRowHeight( const Widget &w ) {
	return w.bind.rowHeight * w.scale.y;
}

int BindVisibleRows( const Widget &w ) {
	const float row = BindRowHeight( w );
	return row > 0.0f ? std::max( 0, static_cast<int>( w.frame[3] / row ) - 1 ) : 0;
}

void DrawBindList( Widget &w, float alpha ) {
	void *font = w.font.Get();
	if ( !font ) {
		return;
	}
	const BindLayout &b = w.bind;
	const float row = BindRowHeight( w );
	const float widths[3] = { b.commandWidth * w.scale.x, b.keyWidth * w.scale.x, b.keyWidth * w.scale.x };
	float x = w.frame[0];
	for ( int c = 0; c < 3; c++ ) {
		DrawBox( x, w.frame[1], widths[c], row, b.titleBackground, alpha );
		SetColor( b.titleForeground, alpha );
		Print( font, x / w.scale.x + 2.0f, w.frame[1] / w.scale.y + 1.0f, b.headers[c].c_str(), w.scale );
		x += widths[c];
	}
	const int visible = BindVisibleRows( w );
	for ( int i = 0; i < visible && w.top + i < static_cast<int>( b.items.size() ); i++ ) {
		const int index = w.top + i;
		const BindItem &item = b.items[static_cast<std::size_t>( index )];
		const float y = w.frame[1] + row * static_cast<float>( i + 1 );
		int bound[2];
		KeysFor( item.command, bound );
		x = w.frame[0];
		for ( int c = 0; c < 3; c++ ) {
			const bool selected = index == w.bindRow && ( c == 0 || c == w.bindColumn + 1 );
			const Color &back = selected ? ( w.binding ? b.selectBackground : b.highlightBackground ) : b.inactiveBackground;
			const Color &fore = selected ? ( w.binding ? b.selectForeground : b.highlightForeground ) : b.inactiveForeground;
			DrawBox( x, y, widths[c], row, back, alpha );
			SetColor( fore, alpha );
			const char *text = c == 0 ? item.name.c_str()
				: ( selected && w.binding ? "???" : ( bound[c - 1] >= 0 ? Key_KeynumToString( bound[c - 1] ) : "" ) );
			Print( font, x / w.scale.x + 2.0f, y / w.scale.y + 1.0f, text, w.scale );
			if ( selected && c > 0 && w.binding ) {
				DrawSolidBorderBox( x, y, widths[c], row, { 0.0f, 0.0f, 0.0f, 0.0f }, b.activeBorder, 1.0f, alpha );
			}
			x += widths[c];
		}
	}
}

/* ---- the saved games (FakkLoadGameClass) -------------------------------------- */

std::string SaveFolder() {
	char folder[ssv::PathBytes];
	ssv::FolderPath( CvarString( "config" ).c_str(), folder );
	return folder;
}

std::string SavePath( const std::string &name, const char *extension ) {
	char path[ssv::PathBytes];
	return ssv::ArchivePath( CvarString( "config" ).c_str(), name.c_str(), extension, path ) ? path : std::string();
}

/* A save's picture as the renderer takes pixels, top row first, from an
 * uncompressed true-colour TGA, the kind `saveshot` writes. */
bool DecodeShot( const std::uint8_t *data, std::size_t size, std::vector<std::uint8_t> &rgba, int &width, int &height ) {
	if ( !data || size < 18 || data[1] != 0 || data[2] != 2 ) {
		return false;
	}
	width = data[12] | data[13] << 8;
	height = data[14] | data[15] << 8;
	const int bits = data[16];
	if ( width <= 0 || height <= 0 || width > 1024 || height > 1024 || ( bits != 24 && bits != 32 ) ) {
		return false;
	}
	const std::size_t bytes = static_cast<std::size_t>( bits / 8 );
	const std::size_t start = 18u + data[0];
	const std::size_t rowBytes = static_cast<std::size_t>( width ) * bytes;
	if ( size < start + rowBytes * static_cast<std::size_t>( height ) ) {
		return false;
	}
	const bool topFirst = ( data[17] & 0x20 ) != 0;
	rgba.resize( static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4u );
	for ( int y = 0; y < height; y++ ) {
		const std::uint8_t *row = data + start + rowBytes * static_cast<std::size_t>( topFirst ? y : height - 1 - y );
		std::uint8_t *out = rgba.data() + static_cast<std::size_t>( y ) * static_cast<std::size_t>( width ) * 4u;
		for ( int x = 0; x < width; x++, row += bytes, out += 4 ) {
			out[0] = row[2];
			out[1] = row[1];
			out[2] = row[0];
			out[3] = bytes == 4 ? row[3] : 255;
		}
	}
	return true;
}

/* The pictures registered so far, one for each time a save was made, so a
 * save made again under its old name shows its new picture. */
struct ShotCache {
	std::string key;
	qhandle_t handle = 0;
};
std::vector<ShotCache> shots;

/* A save's picture, or 0 while it has none: an autosave's comes a frame
 * after the save, so a missing one is looked for again next time. */
qhandle_t SaveShot( const SaveRow &row ) {
	const std::string key = row.name + "/" + std::to_string( row.saveTime );
	for ( const ShotCache &cached : shots ) {
		if ( cached.key == key ) return cached.handle;
	}
	const std::string path = SavePath( row.name, ssv::ShotExtension );
	void *buffer = nullptr;
	const int length = path.empty() ? -1 : FS_ReadFile( path.c_str(), &buffer );
	qhandle_t handle = 0;
	if ( length > 0 && buffer && re.RegisterShaderFromRGBA ) {
		std::vector<std::uint8_t> rgba;
		int width = 0, height = 0;
		if ( DecodeShot( static_cast<const std::uint8_t *>( buffer ), static_cast<std::size_t>( length ), rgba, width,
				height ) ) {
			handle = re.RegisterShaderFromRGBA( va( "*fntech3/mohaa/saveshot/%s/%u", row.name.c_str(), row.saveTime ),
				rgba.data(), width, height );
		}
	}
	if ( buffer ) {
		FS_FreeFile( buffer );
	}
	if ( handle ) {
		shots.push_back( { key, handle } );
	}
	return handle;
}

const SaveRow *CurrentSave( const Widget &w ) {
	return w.current >= 1 && w.current <= static_cast<int>( w.saves.size() )
		? &w.saves[static_cast<std::size_t>( w.current - 1 )] : nullptr;
}

/* SelectGame: the chosen save's picture in the menu's LoadSaveShot, or the
 * menu's own "no saved games" picture. */
void SelectSave( Menu &m, Widget &w ) {
	const SaveRow *row = CurrentSave( w );
	const qhandle_t shot = row ? SaveShot( *row ) : 0;
	for ( Widget &picture : m.widgets ) {
		if ( Q_stricmp( picture.name.c_str(), "LoadSaveShot" ) ) continue;
		if ( shot ) picture.material.SetHandle( "*saveshot", shot );
		else picture.material.Set( ssv::NoShot );
		picture.tile = false;
	}
}

/* The header in facfont-20 two units clear above and below, the rows in the
 * list's own font (UIListCtrl). */
float SaveHeaderHeight( Widget &w ) {
	void *header = CL_MOHAAFontLoad( "facfont-20" );
	return ( FontHeight( header ? header : w.font.Get() ) + 4.0f ) * w.scale.y;
}

float SaveRowHeight( Widget &w ) {
	return FontHeight( w.font.Get() ) * w.scale.y;
}

int SaveRowsShown( Widget &w ) {
	const float row = SaveRowHeight( w );
	return row > 0.0f ? std::max( 1, static_cast<int>( ( w.frame[3] - SaveHeaderHeight( w ) ) / row ) ) : 1;
}

/* The chosen row kept in view. */
void FollowSave( Widget &w ) {
	const int shown = SaveRowsShown( w );
	if ( w.current - 1 < w.top ) w.top = std::max( 0, w.current - 1 );
	else if ( w.current - 1 >= w.top + shown ) w.top = w.current - shown;
}

/* UIFAKKLoadGameClass::SetupFiles: every .ssv of this engine's layout in the
 * save folder, the newest first and the first chosen, and the list takes the
 * keys. A save is known by its file's name. */
void SetupSaves( Menu &m, Widget &w ) {
	w.saves.clear();
	w.removePending = false;
	const std::string folder = SaveFolder();
	int count = 0;
	char **files = FS_ListFiles( folder.c_str(), ".ssv", &count );
	for ( int i = 0; files && i < count; i++ ) {
		std::string name = files[i];
		if ( name.size() <= 4 ) continue;
		name.resize( name.size() - 4 );
		if ( !ssv::NameAllowed( name.c_str() ) ) continue;
		void *buffer = nullptr;
		const int length = FS_ReadFile( ( folder + "/" + files[i] ).c_str(), &buffer );
		ssv::Header header;
		const bool read = length > 0 && buffer
			&& ssv::Decode( static_cast<const std::uint8_t *>( buffer ), static_cast<std::size_t>( length ), header );
		if ( buffer ) FS_FreeFile( buffer );
		if ( !read ) continue;

		SaveRow row;
		row.name = name;
		row.comment = header.comment;
		char elapsed[32];
		ssv::ElapsedText( header.mapTime, elapsed );
		row.elapsed = elapsed;
		row.saveTime = static_cast<std::uint32_t>( header.saveTime );
		const std::time_t when = static_cast<std::time_t>( row.saveTime );
		char date[64] = "";
		if ( const std::tm *local = std::localtime( &when ) ) {
			std::strftime( date, sizeof( date ), ssv::DateFormat, local );
		}
		row.date = date;
		w.saves.push_back( std::move( row ) );
	}
	if ( files ) FS_FreeFileList( files );
	std::stable_sort( w.saves.begin(), w.saves.end(),
		[]( const SaveRow &a, const SaveRow &b ) { return a.saveTime > b.saveTime; } );
	w.current = w.saves.empty() ? 0 : 1;
	w.top = 0;
	SelectSave( m, w );
	activeMenu = static_cast<int>( &m - menus.data() );
	activeWidget = static_cast<int>( &w - m.widgets.data() );
}

/* Text cut to fit a column. */
std::string FitText( void *font, const std::string &text, float width ) {
	std::string fitted = text;
	while ( !fitted.empty() && TextWidth( font, fitted.c_str() ) > width ) fitted.pop_back();
	return fitted;
}

/* The three columns FakkLoadGameClass makes of its width. */
constexpr float kSaveColumns[3] = { 0.555f, 0.17f, 0.275f };
const char *const kSaveHeaders[3] = { "Mission", "Elapsed Time", "Date & Time Logged" };

/* UIListCtrl::DrawColumns and DrawContent: the headers in the HUD's gold on
 * dark boxes, then a row a save on the list's background, the chosen one
 * lit. */
void DrawLoadGame( Widget &w, float alpha ) {
	void *font = w.font.Get();
	void *header = CL_MOHAAFontLoad( "facfont-20" );
	if ( !font ) {
		return;
	}
	if ( !header ) header = font;
	const float x0 = w.frame[0], y0 = w.frame[1], width = w.frame[2];
	const float headerHeight = SaveHeaderHeight( w );
	const Color column = { 0.07f, 0.06f, 0.005f, 1.0f };
	float left = 0.0f;
	for ( int c = 0; c < 3; c++ ) {
		const float cw = width * kSaveColumns[c];
		DrawSolidBorderBox( x0 + left, y0, cw + w.scale.y, headerHeight, column, kHudColor, 1.0f, alpha );
		SetColor( kHudColor, alpha );
		Print( header, ( x0 + left ) / w.scale.x + 2.0f, y0 / w.scale.y + 2.0f,
			FitText( header, kSaveHeaders[c], cw / w.scale.x - 4.0f ).c_str(), w.scale );
		left += cw;
	}
	const float row = SaveRowHeight( w );
	const int shown = SaveRowsShown( w );
	float top = y0 + headerHeight;
	for ( int i = w.top; i < static_cast<int>( w.saves.size() ) && i < w.top + shown; i++ ) {
		const SaveRow &save = w.saves[static_cast<std::size_t>( i )];
		const bool chosen = i + 1 == w.current;
		const Color back = chosen ? Color{ 0.21f, 0.18f, 0.015f, 1.0f }
			: Color{ w.background.r, w.background.g, w.background.b, 1.0f };
		const Color text = chosen ? Color{ 0.9f, 0.8f, 0.6f, 1.0f } : w.foreground;
		const std::string *cells[3] = { &save.comment, &save.elapsed, &save.date };
		left = 0.0f;
		for ( int c = 0; c < 3; c++ ) {
			const float cw = width * kSaveColumns[c];
			DrawBox( x0 + left, top, cw, row, back, alpha );
			SetColor( text, alpha );
			Print( font, ( x0 + left ) / w.scale.x + 1.0f, top / w.scale.y,
				FitText( font, *cells[c], cw / w.scale.x - 2.0f ).c_str(), w.scale );
			left += cw;
		}
		top += row;
	}
}

/* ---- the LAN game list (LANGameClass) -------------------------------------------- */

/* The columns 1.00's list makes of its width less sixteen for the scroll bar
 * (OpenMoHAA UILANGameClass::UpdateUIElement keeps them as comments beside
 * its own), headed in facfont-20. */
constexpr float kLanColumns[5] = { 0.4f, 0.15f, 0.165f, 0.22f, 0.065f };
const char *const kLanHeaders[5] = { "Server Name", "Map", "Players", "GameType", "Ping" };

/* g_gametype's names, as 1.00's game module sets g_gametypestring
 * (gamex86 0x1d6c80..0x1d6da4); the list's answers carry the number. */
const char *LanGameTypeName( int gameType ) {
	switch ( gameType ) {
	case 0: return "Single-Player";
	case 1: return "Free-For-All";
	case 2: return "Team-Match";
	case 3: return "Round-Based-Match";
	case 4: return "Objective-Match";
	default: return "";
	}
}

int LanServerCount() {
	return std::max( 0, std::min( cls.numlocalservers, MAX_OTHER_SERVERS ) );
}

/* When the list last saw a scan begin, so that its status can say Ready. */
int lanScanSeenAt;

void LanScanStatus() {
	const std::string status = CvarString( "cl_langamerefreshstatus" );
	if ( status.compare( 0, 8, "Scanning" ) ) {
		lanScanSeenAt = 0;
		return;
	}
	if ( !lanScanSeenAt ) {
		lanScanSeenAt = Now();
	} else if ( Now() - lanScanSeenAt > 2000 ) {
		/* [I] the scan's answers have come by now; retail's wording for the
		 * settled list is OpenMoHAA's "Ready". */
		Cvar_Set( "cl_langamerefreshstatus", "Ready" );
		lanScanSeenAt = 0;
	}
}

/* JoinServer: the chosen server, by its address. */
void JoinChosenServer( const Widget &w ) {
	if ( w.current < 1 || w.current > LanServerCount() ) {
		return;
	}
	const serverInfo_t &server = cls.localServers[w.current - 1];
	Stuff( std::string( "connect " ) + NET_AdrToStringwPort( &server.adr ) );
}

/* UIListCtrl as LANGameClass sets it up, the rows the local servers that
 * answered, or the menu's "no servers" picture when none has. */
void DrawLanGame( Widget &w, float alpha ) {
	LanScanStatus();
	void *font = w.font.Get();
	void *header = CL_MOHAAFontLoad( "facfont-20" );
	if ( !font ) {
		return;
	}
	if ( !header ) header = font;
	const int count = LanServerCount();
	/* The first server to answer is chosen until another is, so Join and
	 * Enter work without a click. */
	if ( w.current > count || ( w.current < 1 && count > 0 ) ) w.current = count > 0 ? 1 : 0;
	const float x0 = w.frame[0], y0 = w.frame[1];
	const float width = std::max( 0.0f, w.frame[2] - 16.0f * w.scale.x );
	const float headerHeight = ( FontHeight( header ) + 4.0f ) * w.scale.y;
	const Color column = { 0.07f, 0.06f, 0.005f, 1.0f };
	float left = 0.0f;
	for ( int c = 0; c < 5; c++ ) {
		const float cw = width * kLanColumns[c];
		DrawSolidBorderBox( x0 + left, y0, cw + w.scale.y, headerHeight, column, kHudColor, 1.0f, alpha );
		SetColor( kHudColor, alpha );
		Print( header, ( x0 + left ) / w.scale.x + 2.0f, y0 / w.scale.y + 2.0f,
			FitText( header, kLanHeaders[c], cw / w.scale.x - 4.0f ).c_str(), w.scale );
		left += cw;
	}
	if ( !count ) {
		static Material noServers;
		if ( !noServers.Named() ) noServers.Set( "textures/menu/noservers" );
		float pw, ph;
		ShaderSize( noServers.Get(), pw, ph );
		SetColor( kWhite, alpha );
		DrawPic( x0 + ( w.frame[2] - pw * w.scale.x ) * 0.5f, y0 + ( w.frame[3] - ph * w.scale.y ) * 0.5f,
			pw * w.scale.x, ph * w.scale.y, 0.0f, 0.0f, 1.0f, 1.0f, noServers.Get() );
		return;
	}
	const float row = FontHeight( font ) * w.scale.y;
	float top = y0 + headerHeight;
	for ( int i = w.top; i < count && top + row <= y0 + w.frame[3]; i++ ) {
		const serverInfo_t &server = cls.localServers[i];
		const bool chosen = i + 1 == w.current;
		const Color back = chosen ? Color{ 0.21f, 0.18f, 0.015f, 1.0f }
			: Color{ w.background.r, w.background.g, w.background.b, 1.0f };
		const Color text = chosen ? Color{ 0.9f, 0.8f, 0.6f, 1.0f } : w.foreground;
		const std::string cells[5] = { server.hostName, server.mapName,
			va( "%02i/%02i", server.clients, server.maxClients ), LanGameTypeName( server.gameType ),
			va( "%i", server.ping ) };
		left = 0.0f;
		for ( int c = 0; c < 5; c++ ) {
			const float cw = width * kLanColumns[c];
			DrawBox( x0 + left, top, cw, row, back, alpha );
			SetColor( text, alpha );
			Print( font, ( x0 + left ) / w.scale.x + 1.0f, top / w.scale.y,
				FitText( font, cells[c], cw / w.scale.x - 2.0f ).c_str(), w.scale );
			left += cw;
		}
		top += row;
	}
}

/* The list's events: `joingame` joins the chosen server, and so does the
 * `connecttoselected` the menu's Join button names first. */
bool LanGameEvent( Widget &w, const std::string &event ) {
	if ( Is( event, "joingame" ) || Is( event, "connecttoselected" ) ) {
		JoinChosenServer( w );
		return true;
	}
	return false;
}

/* ---- the dialog box ------------------------------------------------------------ */

/* `dialog title cvar command [cancelcommand] [width] [height] [shader]
 * [okshader] [cancelshader]` (OpenMoHAA CL_Dialog_f and UIDialog, reference
 * only): a box over everything that keeps every key and click until one of
 * its two buttons runs its command. Its text is the cvar's, each `$` a new
 * line; a shader takes the text's place, and a button's shader its title.
 * The load list's delete asks through it over the retail question's art,
 * confirm_delete (Pak1, 256 by 64), whose YES and NO the buttons' places
 * cover. Units are the screen's height over 480, so the art keeps its
 * shape. Escape answers Cancel and Enter Ok; a lit button shows which. */
struct Dialog {
	bool open = false;
	std::string title, text, okCommand, cancelCommand;
	float size[2] = { 300.0f, 200.0f };
	Material label, ok, cancel;
	int pressed = 0, hovered = 0;
	float frame[4] = {}, child[4] = {}, okFrame[4] = {}, cancelFrame[4] = {};
} dialog;

bool DialogOpen() {
	return dialog.open;
}

void OpenDialog( const char *title, const char *cvar, const char *command, const char *cancelCommand, int width,
	int height, const char *shader, const char *okShader, const char *cancelShader ) {
	Dialog d;
	d.open = true;
	d.title = title ? title : "";
	d.okCommand = command ? command : "";
	d.cancelCommand = cancelCommand ? cancelCommand : "";
	/* Room for both buttons apart, and no larger than the screen. */
	d.size[0] = static_cast<float>( std::max( 240, std::min( 640, width ) ) );
	d.size[1] = static_cast<float>( std::max( 64, std::min( 480, height ) ) );
	if ( cvar && cvar[0] ) {
		d.text = CvarString( cvar );
		std::replace( d.text.begin(), d.text.end(), '$', '\n' );
	}
	if ( shader && shader[0] ) d.label.Set( shader );
	if ( okShader && okShader[0] ) d.ok.Set( okShader );
	if ( cancelShader && cancelShader[0] ) d.cancel.Set( cancelShader );
	dialog = d;
}

/* The window centred, a title bar twenty units high when it has a title, and
 * the buttons 60 by 24 at 48 from the left and 108 from the right, 30 up
 * from the bottom of the space under the title (UIDialog). */
void PlaceDialog() {
	Dialog &d = dialog;
	const float s = VidHeight() / 480.0f;
	d.frame[2] = d.size[0] * s;
	d.frame[3] = d.size[1] * s;
	d.frame[0] = ( VidWidth() - d.frame[2] ) * 0.5f;
	d.frame[1] = ( VidHeight() - d.frame[3] ) * 0.5f;
	const bool titled = !d.title.empty();
	d.child[0] = d.frame[0] + ( titled ? 2.0f * s : 0.0f );
	d.child[1] = d.frame[1] + ( titled ? 20.0f * s : 0.0f );
	d.child[2] = d.frame[2] - ( titled ? 4.0f * s : 0.0f );
	d.child[3] = d.frame[3] - ( titled ? 22.0f * s : 0.0f );
	const float buttonY = d.child[1] + d.child[3] - 30.0f * s;
	const float okBox[4] = { d.child[0] + 48.0f * s, buttonY, 60.0f * s, 24.0f * s };
	const float cancelBox[4] = { d.child[0] + d.child[2] - 108.0f * s, buttonY, 60.0f * s, 24.0f * s };
	std::copy( okBox, okBox + 4, d.okFrame );
	std::copy( cancelBox, cancelBox + 4, d.cancelFrame );
}

int DialogButtonAt( float x, float y ) {
	PlaceDialog();
	if ( Inside( dialog.okFrame, x, y ) ) return 1;
	if ( Inside( dialog.cancelFrame, x, y ) ) return 2;
	return 0;
}

void DrawDialogButton( const float f[4], Material &material, const char *title, int which, const Scale &scale ) {
	const bool lit = dialog.hovered == which;
	const bool down = lit && dialog.pressed == which;
	if ( material.Named() && material.Get() ) {
		SetColor( kWhite, 1.0f );
		DrawPic( f[0], f[1], f[2], f[3], 0.0f, 0.0f, 1.0f, 1.0f, material.Get() );
	} else {
		Draw3DBox( f[0], f[1], f[2], f[3], down, MakeBorder( kDefaultBorder ), 1.0f );
		SetColor( kHudColor, 1.0f );
		PrintJustified( CL_MOHAAFontLoad( "verdana-12" ), f, JustCenter, JustMiddle, title, scale );
	}
	if ( lit ) {
		DrawBox( f[0], f[1], f[2], f[3], kHudColor, down ? 0.35f : 0.2f );
	}
}

void DrawDialog() {
	if ( !dialog.open ) {
		return;
	}
	PlaceDialog();
	Dialog &d = dialog;
	const float s = VidHeight() / 480.0f;
	const Scale scale = { s, s };
	DrawSolidBorderBox( d.frame[0], d.frame[1], d.frame[2], d.frame[3], kDefaultBackground, kDefaultBorder, 2.0f, 1.0f );
	if ( !d.title.empty() ) {
		DrawBox( d.frame[0], d.frame[1], d.frame[2], 20.0f * s, kDefaultBorder, 1.0f );
		SetColor( kHudColor, 1.0f );
		Print( CL_MOHAAFontLoad( "verdana-14" ), d.frame[0] / s + 3.0f, d.frame[1] / s + 3.0f, d.title.c_str(), scale );
	}
	if ( d.label.Named() && d.label.Get() ) {
		SetColor( kWhite, 1.0f );
		DrawPic( d.child[0], d.child[1], d.child[2], d.child[3], 0.0f, 0.0f, 1.0f, 1.0f, d.label.Get() );
	} else if ( !d.text.empty() ) {
		SetColor( kHudColor, 1.0f );
		const float textArea[4] = { d.child[0], d.child[1], d.child[2], d.child[3] - 30.0f * s };
		PrintJustified( CL_MOHAAFontLoad( "verdana-12" ), textArea, JustCenter, JustMiddle, d.text, scale );
	}
	DrawDialogButton( d.okFrame, d.ok, "Ok", 1, scale );
	DrawDialogButton( d.cancelFrame, d.cancel, "Cancel", 2, scale );
	re.SetColor( nullptr );
}

/* A button's answer: the box goes, then its command runs. */
void CloseDialog( bool ok ) {
	const std::string command = ok ? dialog.okCommand : dialog.cancelCommand;
	dialog = Dialog();
	Stuff( command );
	FocusMenu();
}

/* Every key and click while the box is up is the box's. */
bool DialogKey( int key, bool down ) {
	if ( !dialog.open ) {
		return false;
	}
	if ( key & K_CHAR_FLAG ) {
		return true;
	}
	if ( key == K_MOUSE1 ) {
		const int at = DialogButtonAt( cursorX, cursorY );
		if ( down ) {
			dialog.pressed = at;
		} else {
			const int pressed = dialog.pressed;
			dialog.pressed = 0;
			if ( at && at == pressed ) CloseDialog( at == 1 );
		}
		return true;
	}
	if ( !down ) {
		return true;
	}
	if ( key == K_ESCAPE ) CloseDialog( false );
	else if ( key == K_ENTER || key == K_KP_ENTER ) CloseDialog( true );
	return true;
}

/* ---- the pickers --------------------------------------------------------------- */

/* A window with a title over a list, as the map pickers and the player model
 * picker are (OpenMoHAA MpMapPickerClass, MapRunnerClass and
 * PlayerModelPickerClass on UIFloatingWindow, reference only): 300 by 200,
 * its items in facfont-20 and sorted, a click choosing one and running its
 * command, and the window closing on a choice, on Escape or on a click
 * outside it. Units are the screen's height over 480. */
struct PickerItem {
	std::string label, command;
};

struct Picker {
	bool open = false;
	std::string title;
	std::vector<PickerItem> items;
	int current = 0, top = 0, hovered = -1;
	float frame[4] = {}, list[4] = {};
} picker;

bool PickerOpen() {
	return picker.open;
}

void OpenPicker( const std::string &title, std::vector<PickerItem> items ) {
	std::sort( items.begin(), items.end(), []( const PickerItem &a, const PickerItem &b ) {
		return Q_stricmp( a.label.c_str(), b.label.c_str() ) < 0;
	} );
	picker = Picker();
	picker.open = true;
	picker.title = title;
	picker.items = std::move( items );
	cursorX = VidWidth() * 0.5f;
	cursorY = VidHeight() * 0.5f;
	FocusMenu();
}

void ClosePicker() {
	picker = Picker();
	FocusMenu();
}

void PlacePicker() {
	const float s = VidHeight() / 480.0f;
	picker.frame[2] = 300.0f * s;
	picker.frame[3] = 200.0f * s;
	picker.frame[0] = ( VidWidth() - picker.frame[2] ) * 0.5f;
	picker.frame[1] = ( VidHeight() - picker.frame[3] ) * 0.5f;
	picker.list[0] = picker.frame[0] + 2.0f * s;
	picker.list[1] = picker.frame[1] + 20.0f * s;
	picker.list[2] = picker.frame[2] - 4.0f * s;
	picker.list[3] = picker.frame[3] - 22.0f * s;
}

float PickerRowHeight() {
	return FontHeight( CL_MOHAAFontLoad( "facfont-20" ) ) * ( VidHeight() / 480.0f );
}

int PickerRowsShown() {
	const float row = PickerRowHeight();
	return row > 0.0f ? std::max( 1, static_cast<int>( picker.list[3] / row ) ) : 1;
}

int PickerItemAt( float x, float y ) {
	PlacePicker();
	const float row = PickerRowHeight();
	if ( row <= 0.0f || !Inside( picker.list, x, y ) ) {
		return -1;
	}
	const int item = picker.top + static_cast<int>( ( y - picker.list[1] ) / row );
	return item >= 0 && item < static_cast<int>( picker.items.size() ) ? item : -1;
}

void DrawPicker() {
	if ( !picker.open ) {
		return;
	}
	PlacePicker();
	const float s = VidHeight() / 480.0f;
	const Scale scale = { s, s };
	void *font = CL_MOHAAFontLoad( "facfont-20" );
	const float *f = picker.frame;
	DrawSolidBorderBox( f[0], f[1], f[2], f[3], kDefaultBackground, kDefaultBorder, 2.0f, 1.0f );
	DrawBox( f[0], f[1], f[2], 20.0f * s, kDefaultBorder, 1.0f );
	SetColor( kHudColor, 1.0f );
	Print( font, f[0] / s + 3.0f, f[1] / s + 1.0f, FitText( font, picker.title, f[2] / s - 6.0f ).c_str(), scale );
	const float row = PickerRowHeight();
	const int shown = PickerRowsShown();
	for ( int i = picker.top; i < static_cast<int>( picker.items.size() ) && i < picker.top + shown; i++ ) {
		const float y = picker.list[1] + row * static_cast<float>( i - picker.top );
		const bool lit = i == picker.hovered;
		if ( lit ) DrawBox( picker.list[0], y, picker.list[2], row, { 0.21f, 0.18f, 0.015f, 1.0f }, 1.0f );
		SetColor( lit ? Color{ 0.9f, 0.8f, 0.6f, 1.0f } : kHudColor, 1.0f );
		Print( font, picker.list[0] / s + 2.0f, y / s,
			FitText( font, picker.items[static_cast<std::size_t>( i )].label, picker.list[2] / s - 4.0f ).c_str(), scale );
	}
	re.SetColor( nullptr );
}

void ChoosePickerItem( int item ) {
	if ( item < 0 || item >= static_cast<int>( picker.items.size() ) ) {
		return;
	}
	const std::string command = picker.items[static_cast<std::size_t>( item )].command;
	PlaySound( "sound/menu/apply.wav" );
	ClosePicker();
	Stuff( command );
}

/* Every key and click while the window is up is the window's. */
bool PickerKey( int key, bool down ) {
	if ( !picker.open ) {
		return false;
	}
	if ( ( key & K_CHAR_FLAG ) || !down ) {
		return true;
	}
	const int count = static_cast<int>( picker.items.size() );
	if ( key == K_MOUSE1 ) {
		PlacePicker();
		if ( !Inside( picker.frame, cursorX, cursorY ) ) {
			ClosePicker();
		} else {
			ChoosePickerItem( PickerItemAt( cursorX, cursorY ) );
		}
	} else if ( key == K_ESCAPE ) {
		ClosePicker();
	} else if ( key == K_ENTER || key == K_KP_ENTER ) {
		ChoosePickerItem( picker.hovered );
	} else if ( key == K_UPARROW || key == K_DOWNARROW ) {
		picker.hovered = std::max( 0, std::min( count - 1, picker.hovered + ( key == K_UPARROW ? -1 : 1 ) ) );
		if ( picker.hovered < picker.top ) picker.top = picker.hovered;
		else if ( picker.hovered >= picker.top + PickerRowsShown() ) picker.top = picker.hovered - PickerRowsShown() + 1;
	} else if ( key == K_MWHEELUP ) {
		picker.top = std::max( 0, picker.top - 1 );
	} else if ( key == K_MWHEELDOWN ) {
		picker.top = std::max( 0, std::min( count - PickerRowsShown(), picker.top + 1 ) );
	}
	return true;
}

/* The maps of a folder, each a command with its path under maps/, and a
 * folder's own subfolders as entries that open the picker there. */
std::vector<PickerItem> MapItems( const std::string &folder, const std::string &command, const char *reopen ) {
	std::vector<PickerItem> items;
	const std::string under = folder.size() > 5 ? folder.substr( 5 ) + "/" : std::string();
	int count = 0;
	char **files = FS_ListFiles( folder.c_str(), ".bsp", &count );
	for ( int i = 0; files && i < count; i++ ) {
		std::string map = files[i];
		if ( map.size() <= 4 ) continue;
		map.resize( map.size() - 4 );
		items.push_back( { map, command + " \"" + under + map + "\"" } );
	}
	if ( files ) FS_FreeFileList( files );
	if ( reopen ) {
		files = FS_ListFiles( folder.c_str(), "/", &count );
		for ( int i = 0; files && i < count; i++ ) {
			const std::string sub = files[i];
			if ( sub.empty() || sub[0] == '.' ) continue;
			items.push_back( { sub + "/", std::string( reopen ) + " \"" + under + sub + "\"" } );
		}
		if ( files ) FS_FreeFileList( files );
	}
	return items;
}

/* A folder's files of the given extensions, each running a command on its
 * full name, then the folder's parent (short of the picker's root) and its
 * subfolders as entries that open the same picker there: the engine's
 * developer file pickers (OpenMoHAA FilePickerClass, reference only). The
 * filesystem's listing reaches one folder down into the paks and names a
 * folder once for the paks and once for the disk, so only the folder's own
 * files are kept and each subfolder once. */
std::vector<PickerItem> FileItems( const std::string &root, const std::string &folder,
	std::initializer_list<const char *> extensions, const std::string &command, const std::string &reopen ) {
	std::vector<PickerItem> items;
	for ( const char *extension : extensions ) {
		int count = 0;
		char **files = FS_ListFiles( folder.c_str(), extension, &count );
		for ( int i = 0; files && i < count; i++ ) {
			const std::string name = files[i];
			if ( name.empty() || name.find_first_of( "/\\" ) != std::string::npos ) continue;
			items.push_back( { name, command + " \"" + folder + "/" + name + "\"" } );
		}
		if ( files ) FS_FreeFileList( files );
	}
	const std::size_t slash = folder.rfind( '/' );
	if ( folder != root && slash != std::string::npos ) {
		items.push_back( { "../", reopen + " \"" + folder.substr( 0, slash ) + "\"" } );
	}
	std::vector<std::string> subfolders;
	int count = 0;
	char **folders = FS_ListFiles( folder.c_str(), "/", &count );
	for ( int i = 0; folders && i < count; i++ ) {
		std::string sub = folders[i];
		while ( !sub.empty() && ( sub.back() == '/' || sub.back() == '\\' ) ) sub.pop_back();
		if ( sub.empty() || sub[0] == '.' || sub.find_first_of( "/\\" ) != std::string::npos ) continue;
		const bool known = std::any_of( subfolders.begin(), subfolders.end(),
			[&]( const std::string &s ) { return !Q_stricmp( s.c_str(), sub.c_str() ); } );
		if ( known ) continue;
		subfolders.push_back( sub );
		items.push_back( { sub + "/", reopen + " \"" + folder + "/" + sub + "\"" } );
	}
	if ( folders ) FS_FreeFileList( folders );
	return items;
}

/* The folder a developer picker opens: its argument when it names one under
 * the root, else the root. */
std::string PickerFolder( const std::string &root, const std::string &fallback ) {
	std::string folder = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : fallback;
	while ( !folder.empty() && folder.back() == '/' ) folder.pop_back();
	if ( Q_stricmpn( folder.c_str(), root.c_str(), static_cast<int>( root.size() ) )
		|| ( folder.size() > root.size() && folder[root.size()] != '/' ) || strstr( folder.c_str(), ".." ) ) {
		return root;
	}
	return folder;
}

/* ---- the saved games' actions ------------------------------------------------ */

Menu *MenuOf( const Widget &w ) {
	for ( Menu &m : menus ) {
		if ( !m.widgets.empty() && &w >= m.widgets.data() && &w < m.widgets.data() + m.widgets.size() ) return &m;
	}
	return nullptr;
}

/* LoadGame: the chosen save through the server's own command. */
void LoadChosenSave( const Widget &w ) {
	if ( const SaveRow *row = CurrentSave( w ) ) {
		Stuff( "loadgame \"" + row->name + "\"" );
	}
}

/* RemoveGame: a question first, whose answer comes back as deletegame or
 * nodeletegame. */
void RemoveChosenSave( Widget &w ) {
	if ( w.removePending || !CurrentSave( w ) ) {
		return;
	}
	const std::string list = "widgetcommand \"" + w.name + "\" ";
	OpenDialog( "", "", ( list + "deletegame" ).c_str(), ( list + "nodeletegame" ).c_str(), 256, 64, "confirm_delete",
		"menu_button_trans", "menu_button_trans" );
	w.removePending = true;
	FocusMenu();
}

/* DeleteGame: the save's files go (Com_WipeSavegame), g_lastsave with them
 * when it named this save, and the list is read again. */
void DeleteChosenSave( Menu &m, Widget &w ) {
	w.removePending = false;
	const SaveRow *row = CurrentSave( w );
	if ( !row ) {
		return;
	}
	if ( CvarString( "g_lastsave" ) == row->name ) {
		Cvar_Set( "g_lastsave", "" );
	}
	for ( const char *extension : ssv::WipeOrder ) {
		const std::string path = SavePath( row->name, extension );
		if ( !path.empty() ) FS_HomeRemove( path.c_str() );
	}
	SetupSaves( m, w );
}

/* The widget's events (widgetcommand <list> <event>). False for any other
 * keyword, which the rest of ApplyProperty takes. */
bool LoadGameEvent( Widget &w, const std::string &event ) {
	const bool known = Is( event, "loadgame" ) || Is( event, "savegame" ) || Is( event, "removegame" )
		|| Is( event, "deletegame" ) || Is( event, "nodeletegame" );
	Menu *m = known ? MenuOf( w ) : nullptr;
	if ( !m ) {
		return known;
	}
	if ( Is( event, "loadgame" ) ) {
		LoadChosenSave( w );
	} else if ( Is( event, "savegame" ) ) {
		/* The save and its picture are written before the list reads the
		 * folder again. */
		Cbuf_ExecuteText( EXEC_NOW, "savegame\n" );
		SetupSaves( *m, w );
	} else if ( Is( event, "removegame" ) ) {
		RemoveChosenSave( w );
	} else if ( Is( event, "deletegame" ) ) {
		DeleteChosenSave( *m, w );
	} else {
		w.removePending = false;
	}
	return true;
}

/* UIWidget::Display, then the widget's own Draw. */
void DrawWidget( Menu &m, int menuIndex, int widgetIndex, float parentAlpha ) {
	Widget &w = m.widgets[static_cast<std::size_t>( widgetIndex )];
	if ( !WidgetDrawn( w ) ) {
		return;
	}
	PlaceWidget( m, w );
	const float alpha = w.alpha * parentAlpha;
	const float *f = w.frame;

	if ( w.background.a != 0.0f ) {
		DrawBox( f[0], f[1], f[2], f[3], w.background, w.background.a * alpha );
	}
	if ( w.borderStyle != BorderNone ) {
		Draw3DBox( f[0], f[1], f[2], f[3], w.borderStyle == BorderIndent, w.border, alpha );
	}
	if ( w.material.Named() && w.material.Get() ) {
		SetColor( w.foreground, alpha );
		if ( w.tile ) {
			float sw, sh;
			ShaderSize( w.material.Get(), sw, sh );
			DrawPic( f[0], f[1], f[2], f[3], 0.0f, 0.0f, f[2] / w.scale.x / sw, f[3] / w.scale.y / sh, w.material.Get() );
		} else {
			DrawPic( f[0], f[1], f[2], f[3], 0.0f, 0.0f, 1.0f, 1.0f, w.material.Get() );
		}
	} else {
		SetColor( kWhite, alpha );
	}
	if ( w.pressed && w.pressedMaterial.Named() ) {
		DrawPic( f[0], f[1], f[2], f[3], 0.0f, 0.0f, 1.0f, 1.0f, w.pressedMaterial.Get() );
	} else if ( w.hovered && w.hoverMaterial.Named() ) {
		DrawPic( f[0], f[1], f[2], f[3], 0.0f, 0.0f, 1.0f, 1.0f, w.hoverMaterial.Get() );
	}

	switch ( w.kind ) {
	case Kind::Label: DrawFakkLabel( w, alpha ); break;
	case Kind::Button: DrawButton( w, alpha ); break;
	case Kind::CheckBox: DrawCheckBox( w, alpha ); break;
	case Kind::Slider: DrawSlider( w, alpha ); break;
	case Kind::Field: DrawField( w, alpha, IsActive( menuIndex, widgetIndex ) ); break;
	case Kind::List: DrawList( w, alpha ); break;
	case Kind::ListBox: DrawListBox( w, alpha ); break;
	case Kind::Pulldown: DrawPulldown( w, alpha ); break;
	case Kind::BindList: DrawBindList( w, alpha ); break;
	case Kind::LoadGame: DrawLoadGame( w, alpha ); break;
	case Kind::LanGame: DrawLanGame( w, alpha ); break;
	}
}

/* A menu's container, then its widgets in order, then any open popup over
 * them. The container fills the whole screen with `bgfill` first
 * (UIWidgetContainer::Draw). */
/* `fade` scales the menu's own alpha, for a HUD menu something else covers. */
void DrawMenu( int index, float fade = 1.0f ) {
	Menu &m = menus[static_cast<std::size_t>( index )];
	if ( !m.visible ) {
		return;
	}
	PlaceMenu( m );
	const float alpha = m.alpha * fade;
	const float *f = m.frame;
	if ( m.background.a != 0.0f ) {
		DrawBox( f[0], f[1], f[2], f[3], m.background, m.background.a * alpha );
	}
	if ( m.borderStyle != BorderNone ) {
		Draw3DBox( f[0], f[1], f[2], f[3], m.borderStyle == BorderIndent, m.border, alpha );
	}
	if ( m.fill.a != 0.0f ) {
		DrawBox( 0.0f, 0.0f, VidWidth(), VidHeight(), m.fill, alpha );
	}
	for ( std::size_t i = 0; i < m.widgets.size(); i++ ) {
		DrawWidget( m, index, static_cast<int>( i ), alpha );
	}
	for ( Widget &w : m.widgets ) {
		if ( w.kind == Kind::Pulldown && w.open && WidgetDrawn( w ) ) DrawPopups( w, alpha );
	}
	re.SetColor( nullptr );
}

/* ---- timers ------------------------------------------------------------------- */

void ServiceTimers() {
	const int now = Now();
	for ( Menu &m : menus ) {
		if ( m.showAt && now >= m.showAt ) {
			ShowMenu( m );
		}
		if ( m.hideAt && now >= m.hideAt ) {
			ForceHide( m );
		}
	}
	if ( lockedUntil && now >= lockedUntil ) {
		lockedUntil = 0;
	}
}

/* ---- the HUD ------------------------------------------------------------------- */

void HideHud() {
	for ( int index : { hudHealth, hudAmmo, hudCompass, hudWeapons, hudItems, crosshairMenu } ) {
		if ( index >= 0 && menus[index].visible ) ForceHide( menus[index] );
	}
	for ( int index : hudList ) {
		if ( menus[index].visible ) ForceHide( menus[index] );
	}
	weaponsHudTime = itemsHudTime = 0;
}

void ShowHud( int index ) {
	if ( index >= 0 && !menus[index].visible ) ForceShow( menus[index] );
}

void SetHudWidget( int menu, const char *name, bool enable ) {
	PassToWidget( menu, name, enable ? "enable" : "disable" );
}

/* The weapons bar: slots the player has lose their "empty" cover, the one in
 * hand is lit, and any change shows the bar for ui_weaponsbartime
 * (UI_Update). */
void UpdateWeaponsBar() {
	if ( hudWeapons < 0 ) {
		return;
	}
	Menu &bar = menus[hudWeapons];
	if ( !ui_weaponsbar || !ui_weaponsbar->integer ) {
		ForceHide( bar );
		weaponsHudTime = 0;
		return;
	}
	const int owned = Stat( kStatWeapons ) & 0x3f;
	const int equipped = Stat( kStatEquippedWeapon ) & 0x3f;
	if ( ui_weaponsbar->integer == 2 ) {
		weaponsHudTime = Now() + static_cast<int>( ui_weaponsbartime->value );
	}
	const int ownedDiff = owned ^ lastOwned;
	const int equippedDiff = equipped ^ lastEquipped;
	if ( ownedDiff || equippedDiff ) {
		weaponsHudTime = Now() + ui_weaponsbartime->integer;
	}
	static const char *const kSlots[6] = { "pistol", "rifle", "smg", "mg", "grenade", "heavy" };
	for ( int i = 0; i < 6; i++ ) {
		if ( ownedDiff & ( 1 << i ) ) SetHudWidget( hudWeapons, va( "%s_empty", kSlots[i] ), !( owned & ( 1 << i ) ) );
		if ( equippedDiff & ( 1 << i ) ) {
			SetHudWidget( hudWeapons, va( "%s_equipped", kSlots[i] ), ( equipped & ( 1 << i ) ) != 0 );
		}
	}
	lastOwned = owned;
	lastEquipped = equipped;

	if ( !weaponsHudTime || weaponsHudTime < Now() ) {
		weaponsHudTime = 0;
		if ( bar.visible && !bar.motionOut ) HideMenu( bar, false );
	} else if ( !bar.visible || bar.motionOut ) {
		ShowMenu( bar );
		if ( hudItems >= 0 && menus[hudItems].visible ) HideMenu( menus[hudItems], false );
	}
}

/* The items bar: with ui_itemsbar on and ui_weaponsbar 3 it stays up, but
 * for while the weapons bar shows (OpenMoHAA's UI_Update [O]). Retail's menus
 * set neither cvar that way, so by default it never shows. Each slot's
 * `invmodelhand` label draws the item held in that hand. */
void UpdateItemsBar() {
	if ( hudItems < 0 ) {
		return;
	}
	Menu &bar = menus[hudItems];
	if ( !( ui_weaponsbar->integer && ui_itemsbar->integer ) ) {
		if ( bar.visible ) ForceHide( bar );
		return;
	}
	const bool weaponsUp = weaponsHudTime && hudWeapons >= 0 && menus[hudWeapons].visible;
	if ( ui_weaponsbar->integer == 3 && !weaponsUp ) {
		if ( !bar.visible || bar.motionOut ) ShowMenu( bar );
	} else if ( bar.visible && !bar.motionOut ) {
		HideMenu( bar, false );
	}
}

/* The HUD UI_Update keeps: all of it hidden while a fullscreen menu is up,
 * the game loads, the server says so, intermission runs or a cinematic's
 * letterbox is up; otherwise health,
 * the ammo for the weapon in hand, the compass and the crosshair, and the
 * menus ui_addhud named. */
void UpdateHud() {
	const fntech3::mohaa::PlayerState *ps = PlayerState();
	const int current = CurrentMenu();
	const bool hidden = !hudOn || cls.state != CA_ACTIVE || !ps || loading.active
		|| ( current >= 0 && menus[current].fullscreen ) || ( ps->pm_flags & ( kPmfNoHud | kPmfIntermission ) )
		|| CL_MOHAALetterboxActive();
	if ( hidden ) {
		HideHud();
		return;
	}
	if ( crosshairMenu >= 0 ) {
		if ( ui_crosshair->integer && Stat( kStatCrosshair ) ) ShowHud( crosshairMenu );
		else if ( menus[crosshairMenu].visible ) ForceHide( menus[crosshairMenu] );
	}
	UpdateWeaponsBar();
	UpdateItemsBar();
	ShowHud( hudHealth );

	const int item = ActiveItem( kItemWeapon );
	const std::string ammoName = std::string( "hud_ammo_" ) + ( item >= 0 && item < 64 ? ConfigString( kCsWeapons + item ) : "" );
	if ( hudAmmo < 0 || Q_stricmp( menus[hudAmmo].name.c_str(), ammoName.c_str() ) ) {
		int ammo = FindMenu( ammoName.c_str() );
		if ( ammo < 0 ) ammo = FindMenu( "hud_ammo_" );
		if ( ammo >= 0 && ammo != hudAmmo ) {
			if ( hudAmmo >= 0 ) ForceHide( menus[hudAmmo] );
			hudAmmo = ammo;
		}
	}
	ShowHud( hudAmmo );

	if ( hudCompass >= 0 ) {
		if ( ui_compass->integer ) ShowHud( hudCompass );
		else if ( menus[hudCompass].visible ) ForceHide( menus[hudCompass] );
	}
	for ( int index : hudList ) ShowHud( index );
}

bool IsHudMenu( int index ) {
	if ( index == hudHealth || index == hudAmmo || index == hudCompass || index == hudWeapons || index == hudItems
		|| index == crosshairMenu ) {
		return true;
	}
	return std::find( hudList.begin(), hudList.end(), index ) != hudList.end();
}

bool OnStack( int index ) {
	return std::find( stack.begin(), stack.end(), index ) != stack.end();
}

/* ---- the scoreboard ---------------------------------------------------------- */

void MakeScoreboard() {
	float rect[4], back[4], text[4];
	bool header = true;
	if ( !CL_MOHAACGameScoreboardLayout( rect, back, text, &header ) ) {
		return;
	}
	Scoreboard &sb = scoreboard;
	if ( sb.made && std::equal( rect, rect + 4, sb.rect ) && header == sb.drawHeader ) {
		return;
	}
	std::copy( rect, rect + 4, sb.rect );
	sb.back = { back[0], back[1], back[2], back[3] };
	sb.text = { text[0], text[1], text[2], text[3] };
	sb.drawHeader = header;
	sb.columns.clear();
	for ( int i = 0; i < 8; i++ ) {
		int width = 0;
		const char *name = CL_MOHAACGameScoreboardColumn( i, &width );
		if ( !name ) break;
		sb.columns.push_back( { name, static_cast<float>( width ) } );
	}
	sb.made = true;
}

/* UIListCtrl: a header row of the columns in facfont-20, then a row a player,
 * each in its own colours once the client game gives them. */
void DrawScoreboard() {
	Scoreboard &sb = scoreboard;
	if ( !sb.shown || !sb.made ) {
		return;
	}
	const Scale vs = VirtualScale(), fs = HudScale();
	void *font = CL_MOHAAFontLoad( "verdana-12" );
	void *headerFont = CL_MOHAAFontLoad( "facfont-20" );
	if ( !font ) {
		return;
	}
	const float x0 = sb.rect[0] * vs.x, y0 = sb.rect[1] * vs.y, width = sb.rect[2] * vs.x, height = sb.rect[3] * vs.y;
	const float pad = 2.0f;
	float top = 0.0f;
	if ( sb.drawHeader ) {
		void *hf = headerFont ? headerFont : font;
		const float rowHeight = FontHeight( hf ) * fs.y + pad * 2.0f * fs.y;
		float left = 0.0f;
		const Color column = { 0.07f, 0.06f, 0.005f, 1.0f };
		for ( const auto &c : sb.columns ) {
			const float cw = c.second * vs.x;
			DrawSolidBorderBox( x0 + left, y0, cw + fs.y, rowHeight, column, kHudColor, 1.0f, 1.0f );
			SetColor( kHudColor, 1.0f );
			Print( hf, ( x0 + left ) / fs.x + pad, y0 / fs.y + pad, c.first.c_str(), fs );
			left += cw;
		}
		if ( width > left ) DrawSolidBorderBox( x0 + left, y0, width - left + 1.0f, rowHeight, column, kHudColor, 1.0f, 1.0f );
		top = rowHeight;
	}
	for ( const ScoreRow &row : sb.rows ) {
		void *rf = row.header && headerFont ? headerFont : font;
		const float rowHeight = FontHeight( rf ) * fs.y;
		if ( top + rowHeight > height ) break;
		float left = 0.0f;
		const Color back = row.colored ? row.back : sb.back;
		const Color text = row.colored ? row.text : sb.text;
		for ( std::size_t c = 0; c < sb.columns.size(); c++ ) {
			const float cw = sb.columns[c].second * vs.x;
			DrawBox( x0 + left, y0 + top, cw, rowHeight, back, 1.0f );
			SetColor( text, 1.0f );
			Print( rf, ( x0 + left + 1.0f ) / fs.x, ( y0 + top ) / fs.y, row.cells[c].c_str(), fs );
			if ( row.header && row.colored ) DrawBox( x0 + left, y0 + top + rowHeight - 2.0f, cw, 2.0f, back, 1.0f );
			left += cw;
		}
		if ( width > left ) DrawBox( x0 + left, y0 + top, width - left, rowHeight, back, 1.0f );
		top += rowHeight;
	}
	re.SetColor( nullptr );
}

/* ---- the message boxes ------------------------------------------------------- */

/* The codes a printed line may start with, which 1.00's game module writes
 * (`print "\x01%s"`, `\x02` before a chat line, `\x03`, `\x04`; OpenMoHAA
 * q_shared.h MESSAGE_*): a game message, chat, a bold game message, and a
 * death in red or green. UI_PrintConsole sends the game messages to the game
 * message box under the compass and the rest to the deathmatch box above. */
enum { MessageYellow = 1, MessageChatWhite, MessageWhite, MessageChatRed, MessageChatGreen, MessageMax };

struct BoxLine {
	std::string text;   /* broken into lines that fit the box */
	Color color;
	float seconds = 5.0f;
};

struct TextBox {
	std::vector<BoxLine> lines;
	std::size_t limit = 5;
	int decayStart = 0, decayEnd = 0;
	float frame[4] = {};
} gameBox, deathmatchBox;

/* UIGMBox::CalculateBreaks: words carried to a new line past the width. */
std::string BreakLines( void *font, const std::string &text, float width ) {
	std::string out;
	float x = 0.0f;
	std::size_t at = 0;
	while ( at < text.size() ) {
		std::size_t end = text.find( ' ', at );
		end = end == std::string::npos ? text.size() : end + 1;
		const std::string word = text.substr( at, end - at );
		const float w = TextWidth( font, word.c_str() );
		if ( x + w > width && x > 0.0f ) {
			out += '\n';
			x = 0.0f;
		}
		out += word;
		x += w;
		at = end;
	}
	return out;
}

int LineCount( const std::string &text ) {
	return 1 + static_cast<int>( std::count( text.begin(), text.end(), '\n' ) );
}

/* The oldest line's time to go: its lines times five seconds, or the longer
 * time its kind keeps (UIGMBox and UIDMBox PostDecayEvent). */
void StartDecay( TextBox &box ) {
	if ( box.lines.empty() ) {
		box.decayEnd = 0;
		return;
	}
	const BoxLine &top = box.lines.front();
	box.decayStart = Now();
	box.decayEnd = Now() + static_cast<int>( static_cast<float>( LineCount( top.text ) ) * top.seconds * 1000.0f );
}

void AddBoxLine( TextBox &box, BoxLine line ) {
	if ( box.lines.size() > box.limit - 1 ) {
		box.lines.erase( box.lines.begin() );
		StartDecay( box );
	}
	box.lines.push_back( std::move( line ) );
	if ( !box.decayEnd ) {
		StartDecay( box );
	}
}

void ServiceBox( TextBox &box ) {
	if ( box.decayEnd && Now() >= box.decayEnd ) {
		box.lines.erase( box.lines.begin() );
		StartDecay( box );
	}
}

/* Where the boxes go: the game box under the compass, the deathmatch box to
 * its right along the top, clear of the score at the top right. */
void PlaceBoxes() {
	const Scale s = HudScale();
	gameBox.frame[0] = 20.0f * s.x;
	gameBox.frame[1] = 136.0f * s.y;
	gameBox.frame[2] = VidWidth() - 40.0f * s.x;
	gameBox.frame[3] = 128.0f * s.y;
	deathmatchBox.frame[0] = 136.0f * s.x;
	deathmatchBox.frame[1] = 0.0f;
	deathmatchBox.frame[2] = std::max( 64.0f * s.x, VidWidth() - deathmatchBox.frame[0] - 192.0f * s.x );
	deathmatchBox.frame[3] = 120.0f * s.y;
}

/* UIGMBox::Draw: the oldest line fades over the last quarter of its time;
 * lines past the box's bottom send the oldest away at once. */
void DrawTextBox( TextBox &box, float alphaScale ) {
	ServiceBox( box );
	if ( box.lines.empty() ) {
		return;
	}
	void *font = CL_MOHAAFontLoad( "facfont-20" );
	if ( !font ) {
		return;
	}
	const Scale s = HudScale();
	const float lineHeight = FontHeight( font );
	float frac = box.decayEnd > box.decayStart
		? static_cast<float>( Now() - box.decayStart ) / static_cast<float>( box.decayEnd - box.decayStart ) : 1.0f;
	float alpha = std::min( 1.0f, ( 1.0f - std::min( frac, 1.0f ) ) * 4.0f );
	float y = 0.0f;
	for ( std::size_t i = 0; i < box.lines.size(); i++ ) {
		const BoxLine &line = box.lines[i];
		const float lineAlpha = i == 0 ? alpha * alphaScale : alphaScale;
		SetColor( line.color, lineAlpha );
		std::size_t start = 0;
		float height = 0.0f;
		while ( start <= line.text.size() ) {
			std::size_t end = line.text.find( '\n', start );
			if ( end == std::string::npos ) end = line.text.size();
			const std::string part = line.text.substr( start, end - start );
			Print( font, box.frame[0] / s.x + 3.0f, ( box.frame[1] + y ) / s.y + height, part.c_str(), s );
			height += lineHeight;
			start = end + 1;
		}
		/* A fading oldest line gives up its place early. */
		if ( !( i == 0 && alpha <= 0.2f ) ) y += height * s.y;
		if ( y > box.frame[3] && i + 1 < box.lines.size() ) {
			box.decayEnd = Now();
			break;
		}
	}
	re.SetColor( nullptr );
}

/* ---- the loading screens ------------------------------------------------------ */

std::string LoadingMapName() {
	std::string map;
	if ( com_sv_running && com_sv_running->integer ) {
		map = CvarString( "mapname" );
	} else if ( cls.state >= CA_LOADING ) {
		map = Info_ValueForKey( ConfigString( CS_SERVERINFO ), "mapname" );
	}
	return map;
}

/* The menus a level showed with `showmenu` or through its client game -- off
 * the stacks and not the HUD's or the scoreboard -- go with the level, when
 * the next load or the main menu begins; the last slide of a briefing does
 * not stay over the mission it hands on to. */
void HideLevelMenus() {
	for ( std::size_t i = 0; i < menus.size(); i++ ) {
		const int index = static_cast<int>( i );
		if ( menus[i].visible && !IsHudMenu( index ) && !OnStack( index ) && index != scoreboard.menu ) {
			ForceHide( menus[i] );
		}
	}
}

/* ---- the loading bar ------------------------------------------------------ */

/* A map's load record, maps/<map>.min (code/compat/mohaa_load_record.hpp):
 * as the load reaches each resource the record names, the loading screen's
 * bar advances by the time retail took over it, out of the time of every
 * resource this load will reach. Ones already in the caches are left out,
 * as are the kinds this engine does not report -- the renderers' images (n),
 * the collision's fence masks (f), the engine's own steps (*) and the rest --
 * so that the bar fills. Without a record the loading screen shows its
 * flasher instead, as retail's does.
 *
 * A listen server spawns its level with the client shut down, this UI with
 * it, where 2015's engine keeps its UI up and shows the bar moving through
 * the spawn (OpenMoHAA's CL_MapLoading and CL_FlushMemory [O]). The record
 * and its hook therefore outlive the UI: the game's models, animations,
 * sound scripts and entities count while it is down, and the loading screen
 * the restarted UI shows takes the bar up from there. */
namespace loadrecord = fntech3::mohaa::loadrecord;

struct LoadRecord {
	/* The map and the server's spawn the record is being spent on. */
	std::string key;
	loadrecord::Record record;
} loadRecord;

/* The kinds a load reports here: a model's definition and its parts (a..e, h,
 * l), a skeletal animation (g), the game's entities (i), a sound (k) and the
 * two sound scripts (m, o). */
bool CountedKind( char kind ) {
	return kind && std::strchr( "abcdeghiklmo", kind ) != nullptr;
}

std::string LoadKey( const std::string &map ) {
	const int spawn = com_sv_running && com_sv_running->integer ? Cvar_VariableIntegerValue( "sv_serverid" )
		: cl.serverId;
	return map + "#" + std::to_string( spawn );
}

void ReadLoadRecord( const std::string &map, const std::string &key ) {
	loadRecord = LoadRecord();
	loadRecord.key = key;
	char *text = nullptr;
	const int length = FS_ReadFile( va( "maps/%s.min", map.c_str() ), reinterpret_cast<void **>( &text ) );
	if ( length <= 0 || !text ) {
		if ( text ) FS_FreeFile( text );
		return;
	}
	loadRecord.record = loadrecord::Parse( text, length, []( const std::string &name ) {
		return CountedKind( name[0] ) && !MOHAA_ResourceCached( name.c_str() );
	} );
	FS_FreeFile( text );
	if ( !loadRecord.record.valid ) {
		Com_DPrintf( "Medal of Honor: maps/%s.min is no load record this engine can count\n", map.c_str() );
	}
}

/* The record the current load spends, read when a new load begins. */
loadrecord::Record *CurrentLoadRecord() {
	const std::string map = LoadingMapName();
	if ( map.empty() ) {
		return nullptr;
	}
	const std::string key = LoadKey( map );
	if ( loadRecord.key != key ) {
		ReadLoadRecord( map, key );
	}
	return loadRecord.record.valid ? &loadRecord.record : nullptr;
}

float LoadFraction() {
	const loadrecord::Record *record = CurrentLoadRecord();
	return record ? loadrecord::Fraction( *record ) : 0.0f;
}

/* Inside the engine menus' own drawing, when a load must not draw again. */
bool uiDrawing;

struct DrawingScope {
	bool outer;
	DrawingScope() : outer( !uiDrawing ) { uiDrawing = true; }
	~DrawingScope() { if ( outer ) uiDrawing = false; }
};

/* MOHAA_LoadResource's hook: a resource of the level being loaded. The
 * loading screen is drawn again at most every 33 ms (UI_TestUpdateScreen),
 * and only between frames, with the renderer up. */
void LoadResourceHook( const char *name ) {
	if ( cls.state == CA_ACTIVE || !name || !name[0] ) {
		return;
	}
	loadrecord::Record *record = CurrentLoadRecord();
	if ( !record || !loadrecord::Reach( *record, name ) ) {
		return;
	}
	Cvar_SetValue( "loadingbar", loadrecord::Fraction( *record ) );
	static int lastDrawn;
	const int now = Sys_Milliseconds();
	if ( !uiDrawing && cls.rendererStarted && now - lastDrawn >= 33 ) {
		lastDrawn = now;
		SCR_UpdateScreen();
	}
}

/* UI_BeginLoad: the map's own menu, else loading_default, forced on its own.
 * With a load record its bar shows how far the load has come; without one
 * its flasher shows instead, as retail's does. */
void BeginLoad( const std::string &map ) {
	if ( !loading.active || Q_stricmp( map.c_str(), loading.mapName.c_str() ) ) {
		HideLevelMenus();
	}
	int index = FindMenu( map.c_str() );
	if ( index < 0 ) {
		index = FindMenu( "loading_default" );
	}
	loading.active = true;
	loading.waiting = false;
	loading.mapName = map;
	if ( index != loading.menu || CurrentMenu() != index ) {
		ForceMenuOff( true );
		loading.menu = index;
		if ( index >= 0 ) {
			ForceMenu( index );
			const bool progress = CurrentLoadRecord() != nullptr;
			Cvar_SetValue( "loadingbar", LoadFraction() );
			PassToWidget( index, "continuebutton", "disable" );
			PassToWidget( index, "loadingflasher", progress ? "disable" : "enable" );
			PassToWidget( index, "loadingbar", progress ? "enable" : "disable" );
			PassToWidget( index, "loadingbar_border", progress ? "enable" : "disable" );
		}
		FocusMenu();
	}
}

void FinishLoadingScreen() {
	loading = Loading();
	ForceMenuOff( true );
}

/* UI_ServerLoaded: a local single-player map whose loading menu has a
 * Continue button waits on it, paused; any other load is simply done. */
void ServerLoaded() {
	if ( !loading.active || loading.waiting ) {
		return;
	}
	const int index = loading.menu;
	bool hasContinue = false;
	if ( index >= 0 ) {
		for ( const Widget &w : menus[index].widgets ) {
			if ( !Q_stricmp( w.name.c_str(), "continuebutton" ) ) hasContinue = true;
		}
	}
	const bool local = com_sv_running && com_sv_running->integer && Cvar_VariableIntegerValue( "sv_maxclients" ) <= 1;
	if ( index < 0 || !hasContinue || !local || !Q_stricmp( menus[index].name.c_str(), "loading_default" )
		|| CurrentMenu() != index ) {
		FinishLoadingScreen();
		return;
	}
	loading.waiting = true;
	PassToWidget( index, "continuebutton", "enable" );
	PassToWidget( index, "loadingflasher", "disable" );
	PassToWidget( index, "loadingbar", "disable" );
	PassToWidget( index, "loadingbar_border", "disable" );
	FocusMenu();
}

/* Before the first frame: the loading screen once a map is known, the
 * "connecting" menu while a remote server answers. */
void UpdateConnectScreen() {
	const std::string map = LoadingMapName();
	if ( !map.empty() || ( com_sv_running && com_sv_running->integer ) ) {
		if ( !loading.active || Q_stricmp( map.c_str(), loading.mapName.c_str() ) || CurrentMenu() != loading.menu ) {
			BeginLoad( map );
		}
		return;
	}
	const int connecting = FindMenu( "connecting" );
	if ( connecting >= 0 && CurrentMenu() != connecting ) {
		HideLevelMenus();
		ForceMenuOff( true );
		ForceMenu( connecting );
		FocusMenu();
	}
}

/* ---- the startup intro ------------------------------------------------------------- */

/*
1.00 starts with EA's logo movie, then the title picture and the legal one,
each faded in from black, held and faded out, then 2015's logo movie and the
intro movie, and then the main menu (OpenMoHAA cl_ui.cpp
CL_FinishedStartStage, UI_DrawIntro, UI_StartStageKeyEvent and
CL_TryStartIntro, reference only). The game's autoexec.cfg sets it up each
run: `cl_playintro 1`, no skipping the logo movie, the title or the legal
picture, and half-second fades around a three-second hold; the defaults
below are OpenMoHAA's. A key ends either of the last two movies, and a
skippable picture's stages. The profile's intro command starts it when the
engine starts with nothing on its command line to do
(code/qcommon/game_profile.cpp), unless `developer` is set or cl_playintro
is 0; starting clears cl_playintro.

The pictures cover the screen over black, their 640x480 stretched to it as a
`virtualres` menu's are, lit by the fade's level. The intro holds the keys
throughout, so none reaches a binding or the cinematic's own stop.
*/

struct Intro {
	/* 0 while none plays; otherwise the stage now playing, counted as
	 * CL_FinishedStartStage counts (IntroNext). */
	int stage = 0;
	/* A movie of the intro ended, or never started; the next stage begins on
	 * the next refresh, not inside the cinematic's own shutdown. */
	bool movieEnded = false;
	Material picture;
	float from = 1.0f, to = 1.0f;
	int start = 0, end = 0, fade = 0;
};
Intro intro;

cvar_t *cl_playintro;
cvar_t *ui_skip_eamovie, *ui_skip_titlescreen, *ui_skip_legalscreen;
cvar_t *ui_titlescreen_fadein, *ui_titlescreen_stay, *ui_titlescreen_fadeout;
cvar_t *ui_legalscreen_fadein, *ui_legalscreen_stay, *ui_legalscreen_fadeout;

void SetActiveMenu( int menu );

void IntroHoldKeys() {
	Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
}

/* At once, as CL_FinishedStartStage runs it: appended, it would wait behind
 * whatever the buffer still holds. A movie that cannot start ends there,
 * and the next stage begins on the next refresh. */
void IntroMovie( const char *name ) {
	Cbuf_ExecuteText( EXEC_NOW, va( "cinematic %s\n", name ) );
}

/* A picture's stage: `shader` the picture, or null for the one showing; its
 * level going from `from` to `to` over `seconds`, which is how long it
 * lasts. */
void IntroPicture( const char *shader, float from, float to, const cvar_t *seconds ) {
	if ( shader ) {
		intro.picture.Set( shader );
	}
	const float length = seconds ? seconds->value * 1000.0f : 0.0f;
	const int ms = length > 0.0f ? static_cast<int>( length ) : 0;
	intro.from = from;
	intro.to = to;
	intro.start = Now();
	intro.end = intro.start + ms;
	intro.fade = from != to ? ms : 0;
}

void IntroEnd() {
	intro = Intro();
	SetActiveMenu( UIMENU_MAIN );
}

/* CL_FinishedStartStage: the stage after the one that ended. The title's
 * hold is its own cvar's; OpenMoHAA's reads the legal picture's there, which
 * 1.00's autoexec.cfg sets to the same three seconds. */
void IntroNext() {
	switch ( intro.stage++ ) {
	case 1: IntroMovie( "EAlogo.RoQ" ); break;
	case 2: IntroPicture( "mohaa_title", 0.0f, 1.0f, ui_titlescreen_fadein ); break;
	case 3: IntroPicture( nullptr, 1.0f, 1.0f, ui_titlescreen_stay ); break;
	case 4: IntroPicture( nullptr, 1.0f, 0.0f, ui_titlescreen_fadeout ); break;
	case 5: IntroPicture( "legal", 0.0f, 1.0f, ui_legalscreen_fadein ); break;
	case 6: IntroPicture( nullptr, 1.0f, 1.0f, ui_legalscreen_stay ); break;
	case 7: IntroPicture( nullptr, 1.0f, 0.0f, ui_legalscreen_fadeout ); break;
	case 8: IntroMovie( "2015intro.RoQ" ); break;
	case 9: IntroMovie( "intro.RoQ" ); break;
	default: IntroEnd(); break;
	}
}

void IntroStart() {
	ClearMenus( true );
	CL_MOHAAMusicStopTriggered();
	intro = Intro();
	intro.stage = 1;
	IntroHoldKeys();
	IntroNext();
}

void IntroStopMovie() {
	if ( cls.state == CA_CINEMATIC ) {
		SCR_StopCinematic();
	}
}

/* UI_StartStageKeyEvent: stage 2 is EA's movie, 3 to 5 the title's stages,
 * 6 to 8 the legal picture's, 9 and 10 the last two movies. */
void IntroKey() {
	switch ( intro.stage ) {
	case 2:
		if ( ui_skip_eamovie->integer ) IntroStopMovie();
		break;
	case 3: case 4: case 5:
		if ( ui_skip_titlescreen->integer ) {
			intro.stage = 5;
			IntroNext();
		}
		break;
	case 6: case 7: case 8:
		if ( ui_skip_legalscreen->integer ) {
			intro.stage = 8;
			IntroNext();
		}
		break;
	case 9: case 10:
		IntroStopMovie();
		break;
	default:
		break;
	}
}

/* UI_DrawIntro, each refresh while the intro plays: nothing over a movie;
 * otherwise black, and in a picture's stages the picture, going on to the
 * next stage once this one's time is up. */
void IntroRefresh() {
	IntroHoldKeys();
	if ( cls.state == CA_CINEMATIC ) {
		return;
	}
	if ( intro.movieEnded ) {
		intro.movieEnded = false;
		IntroNext();
		if ( !intro.stage ) {
			return;
		}
	}
	SetColor( kBlack, 1.0f );
	Fill( 0.0f, 0.0f, VidWidth(), VidHeight() );
	if ( intro.stage >= 3 && intro.stage <= 8 ) {
		const qhandle_t shader = intro.picture.Get();
		if ( shader ) {
			float level = intro.to;
			if ( intro.fade > 0 ) {
				const float f = std::min( std::max( static_cast<float>( Now() - intro.start ) / intro.fade, 0.0f ), 1.0f );
				level = intro.from + f * ( intro.to - intro.from );
			}
			const vec4_t color = { level, level, level, 1.0f };
			re.SetColor( color );
			DrawPic( 0.0f, 0.0f, VidWidth(), VidHeight(), 0.0f, 0.0f, 1.0f, 1.0f, shader );
		}
		if ( Now() >= intro.end ) {
			IntroNext();
		}
	}
	re.SetColor( nullptr );
}

/* The profile's intro command: CL_TryStartIntro. */
void StartIntro_f() {
	if ( cls.state != CA_DISCONNECTED ) {
		Com_Printf( "The intro plays only before a game.\n" );
		return;
	}
	if ( Cvar_VariableIntegerValue( "developer" ) || !cl_playintro || !cl_playintro->integer ) {
		return;
	}
	Cvar_Set( "cl_playintro", "0" );
	IntroStart();
}

/* ---- the pointer and the keys ---------------------------------------------------- */

int hoverMenu = -1, hoverWidget = -1;
int pressMenu = -1, pressWidget = -1;

bool TakesPointer( const Widget &w ) {
	return w.kind != Kind::Label || w.hoverMaterial.Named();
}

bool CanActivate( const Widget &w ) {
	return w.kind != Kind::Label;
}

/* The widget of the current menu under the cursor, the one drawn last first. */
int WidgetAt( int menu, float x, float y ) {
	if ( menu < 0 || !menus[menu].visible ) {
		return -1;
	}
	Menu &m = menus[menu];
	for ( int i = static_cast<int>( m.widgets.size() ) - 1; i >= 0; i-- ) {
		Widget &w = m.widgets[static_cast<std::size_t>( i )];
		if ( WidgetDrawn( w ) && TakesPointer( w ) && Inside( w.frame, x, y ) ) {
			return i;
		}
	}
	return -1;
}

Widget *WidgetPtr( int menu, int widget ) {
	if ( menu < 0 || menu >= static_cast<int>( menus.size() ) || widget < 0
		|| widget >= static_cast<int>( menus[menu].widgets.size() ) ) {
		return nullptr;
	}
	return &menus[menu].widgets[static_cast<std::size_t>( widget )];
}

void Activate( int menu, int widget ) {
	activeMenu = menu;
	activeWidget = widget;
}

/* MouseEntered and MouseExited: the hover picture, sound and commands. */
void UpdateHover() {
	const int menu = CurrentMenu();
	const int widget = WidgetAt( menu, cursorX, cursorY );
	if ( menu == hoverMenu && widget == hoverWidget ) {
		return;
	}
	if ( Widget *old = WidgetPtr( hoverMenu, hoverWidget ) ) {
		old->hovered = false;
		if ( old->kind != Kind::Button ) old->pressed = false;
		Stuff( old->awayCommand );
	}
	hoverMenu = menu;
	hoverWidget = widget;
	if ( Widget *now = WidgetPtr( menu, widget ) ) {
		now->hovered = true;
		if ( now->kind == Kind::Button ) {
			Activate( menu, widget );
			PlaySound( now->hoverSound );
			Stuff( now->hoverCommand );
		}
	}
}

/* UIButtonBase::Action. */
void ButtonAction( Widget &w ) {
	PlaySound( w.clickSound );
	Stuff( w.command );
}

void SetSliderValue( Widget &w, float value ) {
	value = std::max( w.sliderMin, std::min( w.sliderMax, value ) );
	if ( w.cvar.empty() ) {
		return;
	}
	if ( w.sliderFloat ) Cvar_Set( w.cvar.c_str(), va( "%f", value ) );
	else Cvar_Set( w.cvar.c_str(), va( "%i", static_cast<int>( value ) ) );
}

void SliderStep( Widget &w, float direction ) {
	const float value = w.cvar.empty() ? w.sliderMin : Cvar_VariableValue( w.cvar.c_str() );
	SetSliderValue( w, value + w.step * direction );
}

void SliderDrag( Widget &w, float x ) {
	const SliderGeometry g = SliderShape( w );
	float value;
	if ( x <= g.minX ) value = w.sliderMin;
	else if ( x >= g.maxX ) value = w.sliderMax;
	else value = ( x - g.minX ) / ( g.maxX - g.minX ) * ( w.sliderMax - w.sliderMin ) + w.sliderMin;
	SetSliderValue( w, value );
}

void ListStep( Widget &w, int direction ) {
	if ( w.items.empty() ) {
		return;
	}
	w.current = std::max( 1, std::min( static_cast<int>( w.items.size() ), w.current + direction ) );
	const auto &item = w.items[static_cast<std::size_t>( w.current - 1 )];
	if ( !w.cvar.empty() ) Cvar_Set( w.cvar.c_str(), item.first.c_str() );
	if ( !w.command.empty() ) Stuff( w.command + " " + item.first );
}

void MouseDown() {
	const int menu = CurrentMenu();
	/* An open popup takes the click first. */
	if ( menu >= 0 ) {
		for ( Widget &w : menus[menu].widgets ) {
			if ( w.kind != Kind::Pulldown || !w.open ) continue;
			const float row = PopupRowHeight( w ), width = PopupWidth( w );
			const float y0 = w.frame[1] + w.frame[3];
			if ( cursorX >= w.frame[0] && cursorX < w.frame[0] + width && cursorY >= y0 ) {
				const int item = static_cast<int>( ( cursorY - y0 ) / row );
				if ( item >= 0 && item < static_cast<int>( w.popups.size() ) ) {
					const Popup &p = w.popups[static_cast<std::size_t>( item )];
					if ( Is( p.type, "command" ) ) Stuff( p.data );
					else if ( Is( p.type, "cvar" ) ) Stuff( "set " + p.data );
				}
			}
			w.open = false;
			return;
		}
	}
	const int widget = WidgetAt( menu, cursorX, cursorY );
	Widget *w = WidgetPtr( menu, widget );
	if ( !w || !CanActivate( *w ) ) {
		return;
	}
	Activate( menu, widget );
	pressMenu = menu;
	pressWidget = widget;
	w->pressed = true;
	switch ( w->kind ) {
	case Kind::Slider: {
		const SliderGeometry g = SliderShape( *w );
		if ( cursorX < w->frame[0] + g.arrow ) {
			SliderStep( *w, -1.0f );
			w->prevDown = true;
			w->repeatAt = Now() + 200;
		} else if ( cursorX >= w->frame[0] + w->frame[2] - g.arrow ) {
			SliderStep( *w, 1.0f );
			w->nextDown = true;
			w->repeatAt = Now() + 200;
		} else {
			w->oldValue = w->cvar.empty() ? 0.0f : Cvar_VariableValue( w->cvar.c_str() );
			w->thumbDown = true;
			SliderDrag( *w, cursorX );
		}
		break;
	}
	case Kind::List: {
		const float arrow = 15.0f * w->scale.x;
		if ( cursorX < w->frame[0] + arrow ) {
			w->prevDown = true;
			ListStep( *w, -1 );
		} else if ( cursorX >= w->frame[0] + w->frame[2] - arrow ) {
			w->nextDown = true;
			ListStep( *w, 1 );
		}
		break;
	}
	case Kind::ListBox: {
		const float row = ListRowHeight( *w );
		if ( row > 0.0f ) {
			const int item = w->top + static_cast<int>( ( cursorY - w->frame[1] ) / row );
			if ( item >= 0 && item < static_cast<int>( w->items.size() ) ) {
				w->current = item + 1;
				if ( !w->cvar.empty() ) Cvar_Set( w->cvar.c_str(), w->items[static_cast<std::size_t>( item )].first.c_str() );
			}
		}
		break;
	}
	case Kind::Pulldown:
		w->open = !w->popups.empty();
		w->popupHover = -1;
		break;
	case Kind::Field: {
		void *font = w->font.Get();
		const std::string text = w->cvar.empty() ? std::string() : CvarString( w->cvar );
		const float point = ( cursorX - w->frame[0] ) / w->scale.x - 2.0f;
		int at = 0;
		while ( at < static_cast<int>( text.size() )
			&& TextWidth( font, text.substr( 0, static_cast<std::size_t>( at ) + 1 ).c_str() ) <= point ) {
			at++;
		}
		w->cursor = at;
		break;
	}
	case Kind::LoadGame: {
		/* A row chosen; the same row again within half a second loads it
		 * (UIListCtrl's double click). */
		const float row = SaveRowHeight( *w );
		const float y = cursorY - w->frame[1] - SaveHeaderHeight( *w );
		if ( row <= 0.0f || y < 0.0f ) break;
		const int item = w->top + static_cast<int>( y / row ) + 1;
		if ( item < 1 || item > static_cast<int>( w->saves.size() ) ) break;
		const bool again = item == w->current && item == w->clickRow && Now() - w->clickTime < 500;
		w->current = item;
		SelectSave( menus[menu], *w );
		if ( again ) {
			w->clickTime = 0;
			LoadChosenSave( *w );
		} else {
			w->clickTime = Now();
			w->clickRow = item;
		}
		break;
	}
	case Kind::LanGame: {
		/* A server chosen; the same one again within half a second joins. */
		void *font = w->font.Get();
		void *header = CL_MOHAAFontLoad( "facfont-20" );
		const float row = FontHeight( font ) * w->scale.y;
		const float y = cursorY - w->frame[1] - ( FontHeight( header ? header : font ) + 4.0f ) * w->scale.y;
		if ( row <= 0.0f || y < 0.0f ) break;
		const int item = w->top + static_cast<int>( y / row ) + 1;
		if ( item < 1 || item > LanServerCount() ) break;
		const bool again = item == w->current && item == w->clickRow && Now() - w->clickTime < 500;
		w->current = item;
		if ( again ) {
			w->clickTime = 0;
			JoinChosenServer( *w );
		} else {
			w->clickTime = Now();
			w->clickRow = item;
		}
		break;
	}
	case Kind::BindList: {
		const float row = BindRowHeight( *w );
		if ( row <= 0.0f ) break;
		const int line = static_cast<int>( ( cursorY - w->frame[1] ) / row ) - 1;
		const int index = w->top + line;
		if ( line < 0 || index >= static_cast<int>( w->bind.items.size() ) ) break;
		const float keyStart = w->frame[0] + w->bind.commandWidth * w->scale.x;
		const int column = cursorX >= keyStart + w->bind.keyWidth * w->scale.x ? 1 : 0;
		if ( index == w->bindRow && column == w->bindColumn && !w->binding ) {
			/* A second click on the selected cell waits for its key. */
			w->binding = true;
			PlaySound( w->bind.enterSound );
		} else {
			w->bindRow = index;
			w->bindColumn = column;
			w->binding = false;
			PlaySound( w->bind.activeSound );
		}
		break;
	}
	default:
		break;
	}
}

void MouseUp() {
	Widget *w = WidgetPtr( pressMenu, pressWidget );
	pressMenu = pressWidget = -1;
	if ( !w ) {
		return;
	}
	const bool inside = Inside( w->frame, cursorX, cursorY );
	w->pressed = false;
	w->prevDown = w->nextDown = w->thumbDown = false;
	if ( !inside ) {
		return;
	}
	if ( w->kind == Kind::Button ) {
		ButtonAction( *w );
	} else if ( w->kind == Kind::CheckBox ) {
		/* UICheckBox::Released: flip, run the matching command, set the cvar. */
		const bool checked = !Checked( *w );
		Stuff( checked ? w->checkedCommand : w->uncheckedCommand );
		if ( !w->cvar.empty() ) Cvar_Set( w->cvar.c_str(), checked ? "1" : "0" );
		PlaySound( w->clickSound );
	}
}

void MouseMoved() {
	if ( dialog.open ) {
		dialog.hovered = DialogButtonAt( cursorX, cursorY );
		return;
	}
	if ( picker.open ) {
		picker.hovered = PickerItemAt( cursorX, cursorY );
		return;
	}
	UpdateHover();
	if ( Widget *w = WidgetPtr( pressMenu, pressWidget ) ) {
		if ( w->kind == Kind::Slider && w->thumbDown ) SliderDrag( *w, cursorX );
	}
	const int menu = CurrentMenu();
	if ( menu >= 0 ) {
		for ( Widget &w : menus[menu].widgets ) {
			if ( w.kind != Kind::Pulldown || !w.open ) continue;
			const float row = PopupRowHeight( w ), y0 = w.frame[1] + w.frame[3];
			w.popupHover = cursorX >= w.frame[0] && cursorX < w.frame[0] + PopupWidth( w ) && cursorY >= y0
				? static_cast<int>( ( cursorY - y0 ) / row ) : -1;
		}
	}
}

/* The slider's arrows repeat while held (UISlider::AutoIncrement). */
void ServiceRepeat() {
	Widget *w = WidgetPtr( pressMenu, pressWidget );
	if ( !w || w->kind != Kind::Slider || !( w->prevDown || w->nextDown ) || Now() < w->repeatAt ) {
		return;
	}
	if ( !Inside( w->frame, cursorX, cursorY ) ) {
		w->prevDown = w->nextDown = false;
		return;
	}
	SliderStep( *w, w->nextDown ? 1.0f : -1.0f );
	w->repeatAt = Now() + 100;
}

void FieldKey( Widget &w, int key ) {
	std::string text = w.cvar.empty() ? std::string() : CvarString( w.cvar );
	const int length = static_cast<int>( text.size() );
	switch ( key ) {
	case K_ENTER:
	case K_KP_ENTER:
		if ( !w.command.empty() ) Stuff( w.command + " " + text );
		return;
	case K_DEL:
		if ( w.cursor < length ) text.erase( static_cast<std::size_t>( w.cursor ), 1 );
		break;
	case K_LEFTARROW:
		if ( w.cursor > 0 ) w.cursor--;
		return;
	case K_RIGHTARROW:
		if ( w.cursor < length ) w.cursor++;
		return;
	case K_HOME:
		w.cursor = 0;
		return;
	case K_END:
		w.cursor = length;
		return;
	default:
		return;
	}
	if ( !w.cvar.empty() ) Cvar_Set( w.cvar.c_str(), text.c_str() );
}

void FieldChar( Widget &w, int ch ) {
	std::string text = w.cvar.empty() ? std::string() : CvarString( w.cvar );
	w.cursor = std::min( w.cursor, static_cast<int>( text.size() ) );
	if ( ch == '\b' ) {
		if ( w.cursor <= 0 ) return;
		text.erase( static_cast<std::size_t>( w.cursor ) - 1, 1 );
		w.cursor--;
	} else if ( ch >= ' ' && ch < 127 ) {
		if ( text.size() >= MAX_CVAR_VALUE_STRING - 1 ) return;
		text.insert( text.begin() + w.cursor, static_cast<char>( ch ) );
		w.cursor++;
	} else {
		return;
	}
	if ( !w.cvar.empty() ) Cvar_Set( w.cvar.c_str(), text.c_str() );
}

/* The key a FakkBindList waits for: Escape gives up, Backspace clears the
 * cell, anything else takes it. */
bool BindKey( int key ) {
	Widget *w = WidgetPtr( activeMenu, activeWidget );
	if ( !w || w->kind != Kind::BindList || !w->binding ) {
		return false;
	}
	w->binding = false;
	if ( w->bindRow < 0 || w->bindRow >= static_cast<int>( w->bind.items.size() ) || key == K_ESCAPE ) {
		return true;
	}
	const std::string &command = w->bind.items[static_cast<std::size_t>( w->bindRow )].command;
	int bound[2];
	KeysFor( command, bound );
	const int old = bound[w->bindColumn];
	if ( key == K_BACKSPACE ) {
		if ( old >= 0 ) Key_SetBinding( old, "" );
		return true;
	}
	if ( old >= 0 ) Key_SetBinding( old, "" );
	Key_SetBinding( key, command.c_str() );
	return true;
}

void KeyEvent( int key, bool down ) {
	if ( intro.stage ) {
		if ( down && !( key & K_CHAR_FLAG ) ) {
			IntroKey();
		}
		return;
	}
	if ( DialogKey( key, down ) || PickerKey( key, down ) ) {
		return;
	}
	if ( key & K_CHAR_FLAG ) {
		Widget *w = WidgetPtr( activeMenu, activeWidget );
		if ( w && activeMenu == CurrentMenu() ) {
			if ( w->kind == Kind::Field ) FieldChar( *w, key & ~K_CHAR_FLAG );
			else if ( w->kind == Kind::CheckBox && ( key & ~K_CHAR_FLAG ) == ' ' ) {
				Cvar_Set( w->cvar.c_str(), Checked( *w ) ? "0" : "1" );
			}
		}
		return;
	}
	if ( down && BindKey( key ) ) {
		return;
	}
	if ( key == K_MOUSE1 ) {
		if ( down ) MouseDown();
		else MouseUp();
		return;
	}
	if ( !down ) {
		return;
	}
	if ( key == K_ESCAPE ) {
		const int menu = CurrentMenu();
		if ( menu >= 0 ) {
			for ( Widget &w : menus[menu].widgets ) {
				if ( w.kind == Kind::Pulldown && w.open ) {
					w.open = false;
					return;
				}
			}
		}
		MenuEscape( "main" );
		return;
	}
	Widget *w = WidgetPtr( activeMenu, activeWidget );
	if ( !w || activeMenu != CurrentMenu() ) {
		return;
	}
	switch ( w->kind ) {
	case Kind::Button:
		if ( key == K_ENTER || key == K_KP_ENTER ) ButtonAction( *w );
		break;
	case Kind::Slider:
		if ( key == K_LEFTARROW ) SliderStep( *w, -1.0f );
		else if ( key == K_RIGHTARROW ) SliderStep( *w, 1.0f );
		break;
	case Kind::List:
		if ( key == K_LEFTARROW ) ListStep( *w, -1 );
		else if ( key == K_RIGHTARROW ) ListStep( *w, 1 );
		break;
	case Kind::Field:
		FieldKey( *w, key );
		break;
	case Kind::LoadGame: {
		/* UIFAKKLoadGameClass::KeyEvent: Delete asks to remove, Enter loads,
		 * the arrows choose and the wheel scrolls. */
		const int count = static_cast<int>( w->saves.size() );
		Menu &m = menus[activeMenu];
		if ( key == K_DEL ) RemoveChosenSave( *w );
		else if ( key == K_ENTER || key == K_KP_ENTER ) LoadChosenSave( *w );
		else if ( ( key == K_UPARROW && w->current > 1 ) || ( key == K_DOWNARROW && w->current < count ) ) {
			w->current += key == K_UPARROW ? -1 : 1;
			FollowSave( *w );
			SelectSave( m, *w );
		} else if ( key == K_MWHEELUP ) w->top = std::max( 0, w->top - 1 );
		else if ( key == K_MWHEELDOWN ) w->top = std::max( 0, std::min( count - SaveRowsShown( *w ), w->top + 1 ) );
		break;
	}
	case Kind::LanGame: {
		/* UILANGameClass::KeyEvent: Enter joins, the arrows choose. */
		const int count = LanServerCount();
		if ( key == K_ENTER || key == K_KP_ENTER ) JoinChosenServer( *w );
		else if ( key == K_UPARROW && w->current > 1 ) w->current--;
		else if ( key == K_DOWNARROW && w->current < count ) w->current++;
		else if ( key == K_MWHEELUP ) w->top = std::max( 0, w->top - 1 );
		else if ( key == K_MWHEELDOWN ) w->top = std::max( 0, std::min( count - 1, w->top + 1 ) );
		break;
	}
	case Kind::BindList:
	case Kind::ListBox: {
		const int count = w->kind == Kind::BindList ? static_cast<int>( w->bind.items.size() )
			: static_cast<int>( w->items.size() );
		const int visible = w->kind == Kind::BindList ? BindVisibleRows( *w )
			: ( ListRowHeight( *w ) > 0.0f ? static_cast<int>( w->frame[3] / ListRowHeight( *w ) ) : 0 );
		if ( key == K_MWHEELUP || key == K_UPARROW ) w->top = std::max( 0, w->top - 1 );
		else if ( key == K_MWHEELDOWN || key == K_DOWNARROW ) w->top = std::max( 0, std::min( count - visible, w->top + 1 ) );
		else if ( w->kind == Kind::BindList && ( key == K_ENTER || key == K_KP_ENTER ) && w->bindRow >= 0 ) {
			w->binding = true;
			PlaySound( w->bind.enterSound );
		}
		break;
	}
	default:
		break;
	}
}

void MouseEvent( int dx, int dy ) {
	cursorX = std::max( 0.0f, std::min( VidWidth() - 1.0f, cursorX + static_cast<float>( dx ) ) );
	cursorY = std::max( 0.0f, std::min( VidHeight() - 1.0f, cursorY + static_cast<float>( dy ) ) );
	MouseMoved();
}

void DrawCursor() {
	if ( !cursorShown || ( CurrentMenu() < 0 && !dialog.open && !picker.open ) || ( loading.active && !loading.waiting ) ) {
		return;
	}
	const qhandle_t shader = cursorMaterial.Get();
	if ( !shader ) {
		return;
	}
	float w, h;
	ShaderSize( shader, w, h );
	const Scale s = HudScale();
	re.SetColor( nullptr );
	DrawPic( cursorX, cursorY, w * s.x, h * s.y, 0.0f, 0.0f, 1.0f, 1.0f, shader );
}

/* ---- the console commands -------------------------------------------------------- */

void PushMenu_f() {
	if ( Cmd_Argc() <= 1 ) {
		Com_Printf( "Usage: pushmenu <menuname>\n" );
		return;
	}
	PushMenu( Cmd_Argv( 1 ) );
	FocusMenu();
}

bool SinglePlayer() {
	const bool clientIdle = cls.state == CA_DISCONNECTED || !Cvar_VariableIntegerValue( "cg_gametype" );
	const bool serverSingle = !( com_sv_running && com_sv_running->integer ) || !Cvar_VariableIntegerValue( "g_gametype" );
	return clientIdle && serverSingle;
}

void PushMenuSP_f() {
	if ( Cmd_Argc() <= 1 ) {
		Com_Printf( "Usage: pushmenu_sp <menuname>\n" );
		return;
	}
	if ( SinglePlayer() ) {
		PushMenu( Cmd_Argv( 1 ) );
		FocusMenu();
	}
}

/* pushmenu_dm: only in a multiplayer game; SelectTeam is the game type's own
 * team menu. */
void PushMenuDM_f() {
	if ( Cmd_Argc() <= 1 ) {
		Com_Printf( "Usage: pushmenu_dm <menuname>\n" );
		return;
	}
	if ( cls.state == CA_DISCONNECTED || !Cvar_VariableIntegerValue( "cg_gametype" ) ) {
		return;
	}
	const char *name = Cmd_Argv( 1 );
	if ( !Q_stricmp( name, "SelectTeam" ) ) {
		switch ( Cvar_VariableIntegerValue( "cg_gametype" ) ) {
		case 1: name = "SelectFFAModel"; break;
		case 4: name = "ObjSelectTeam"; break;
		default: break;
		}
	}
	PushMenu( name );
	FocusMenu();
}

void ForceMenu_f() {
	if ( Cmd_Argc() <= 1 ) {
		Com_Printf( "Usage: forcemenu <menuname>\n" );
		return;
	}
	const int index = FindMenu( Cmd_Argv( 1 ) );
	if ( index < 0 ) {
		Com_Printf( "Couldn't find menu %s\n", Cmd_Argv( 1 ) );
		return;
	}
	ForceMenu( index );
	FocusMenu();
}

void PopMenu_f() {
	if ( Cmd_Argc() <= 1 ) {
		Com_Printf( "Usage: popmenu <restore_cvars>\n" );
		return;
	}
	PopMenu( std::atoi( Cmd_Argv( 1 ) ) != 0 );
	FocusMenu();
}

/* showmenu and hidemenu, and the client game's calls: a menu shown over the
 * others, off the stacks (UI_ShowMenu, UI_HideMenu). */
void ShowNamedMenu( const char *name, bool force ) {
	const int index = FindMenu( name );
	if ( index < 0 ) {
		return;
	}
	if ( force ) ForceShow( menus[index] );
	else if ( !menus[index].visible || menus[index].motionOut ) ShowMenu( menus[index] );
}

void HideNamedMenu( const char *name, bool force ) {
	const int index = FindMenu( name );
	if ( index < 0 ) {
		return;
	}
	if ( force ) ForceHide( menus[index] );
	else if ( menus[index].visible ) HideMenu( menus[index], false );
}

void ShowMenu_f() {
	if ( Cmd_Argc() <= 1 ) {
		Com_Printf( "Usage: showmenu <menuname>\n" );
		return;
	}
	ShowNamedMenu( Cmd_Argv( 1 ), Cmd_Argc() > 2 && std::atoi( Cmd_Argv( 2 ) ) );
}

void HideMenu_f() {
	if ( Cmd_Argc() <= 1 ) {
		Com_Printf( "Usage: hidemenu <menuname>\n" );
		return;
	}
	HideNamedMenu( Cmd_Argv( 1 ), Cmd_Argc() > 2 && std::atoi( Cmd_Argv( 2 ) ) );
}

void WidgetCommandIn( int menu, const char *widgetName ) {
	std::vector<std::string> args;
	for ( int i = 3; i < Cmd_Argc(); i++ ) args.push_back( Cmd_Argv( i ) );
	for ( Widget &w : menus[menu].widgets ) {
		if ( !Q_stricmp( w.name.c_str(), widgetName ) ) {
			ApplyProperty( w, Cmd_Argv( 2 ), args );
			return;
		}
	}
}

/* widgetcommand: an event for a widget of the current menu. */
void WidgetCommand_f() {
	if ( Cmd_Argc() <= 2 ) {
		Com_Printf( "Usage: widgetcommand <name> <args1...argsN>\n" );
		return;
	}
	if ( CurrentMenu() >= 0 ) {
		WidgetCommandIn( CurrentMenu(), Cmd_Argv( 1 ) );
	}
}

/* globalwidgetcommand: the same for a widget of any menu. */
void GlobalWidgetCommand_f() {
	if ( Cmd_Argc() <= 2 ) {
		Com_Printf( "Usage: globalwidgetcommand <name> <args1...argsN>\n" );
		return;
	}
	for ( std::size_t i = 0; i < menus.size(); i++ ) WidgetCommandIn( static_cast<int>( i ), Cmd_Argv( 1 ) );
}

/* listmenus [visible]: every menu, or only those showing, with how each is up
 * -- on the stack, the HUD's, or shown by `showmenu` or the client game --
 * and, for one showing, how many of its widgets show. */
void ListMenus_f() {
	const bool visibleOnly = Cmd_Argc() > 1 && !Q_stricmp( Cmd_Argv( 1 ), "visible" );
	for ( std::size_t i = 0; i < menus.size(); i++ ) {
		const Menu &m = menus[i];
		const int index = static_cast<int>( i );
		if ( visibleOnly && !m.visible ) {
			continue;
		}
		if ( !m.visible ) {
			Com_Printf( "%s\n", m.name.c_str() );
			continue;
		}
		int widgetsShown = 0;
		for ( const Widget &w : m.widgets ) widgetsShown += w.shown ? 1 : 0;
		Com_Printf( "%s: showing%s, %s, %i of %i widgets\n", m.name.c_str(), m.motionOut ? " (leaving)" : "",
			OnStack( index ) ? "on the stack" : IsHudMenu( index ) ? "HUD" : "shown", widgetsShown,
			static_cast<int>( m.widgets.size() ) );
	}
}

void LoadMenu_f() {
	if ( Cmd_Argc() <= 1 ) {
		Com_Printf( "Usage: loadmenu <filename>\n" );
		return;
	}
	const std::size_t before = menus.size();
	LoadMenuFile( Cmd_Argv( 1 ) );
	for ( std::size_t i = before; i < menus.size(); i++ ) FinishMenu( menus[i] );
}

/* ui_hud: the HUD on or off, and cg_hud with it. */
void Hud_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: ui_hud [1|0]\n" );
		return;
	}
	hudOn = std::atoi( Cmd_Argv( 1 ) ) != 0;
	if ( !hudOn ) {
		HideHud();
	}
	Cvar_Set( "cg_hud", hudOn ? "1" : "0" );
}

void AddHud_f() {
	const int index = FindMenu( Cmd_Argv( 1 ) );
	if ( index < 0 ) {
		Com_DPrintf( "Hud %s not found in menu system.\n", Cmd_Argv( 1 ) );
		return;
	}
	if ( std::find( hudList.begin(), hudList.end(), index ) == hudList.end() ) {
		hudList.push_back( index );
	}
	ShowMenu( menus[index] );
}

void RemoveHud_f() {
	const int index = FindMenu( Cmd_Argv( 1 ) );
	const auto at = std::find( hudList.begin(), hudList.end(), index );
	if ( index >= 0 && at != hudList.end() ) {
		ForceHide( menus[index] );
		hudList.erase( at );
	}
}

/* ui_resetcvars: the current menu's cvars back to their defaults. */
void ResetCvars_f() {
	const int menu = CurrentMenu();
	if ( menu < 0 ) {
		return;
	}
	for ( const Widget &w : menus[menu].widgets ) {
		if ( !w.cvar.empty() ) Cvar_Reset( w.cvar.c_str() );
	}
}

/* ui_checkrestart: a video restart once a latched cvar of the current menu
 * has a new value waiting. */
void CheckRestart_f() {
	const int menu = CurrentMenu();
	if ( menu < 0 ) {
		return;
	}
	for ( const Widget &w : menus[menu].widgets ) {
		if ( !w.cvar.empty() && Q_stricmp( LatchedCvarString( w.cvar ).c_str(), CvarString( w.cvar ).c_str() ) ) {
			Cbuf_AddText( "vid_restart\n" );
			return;
		}
	}
}

void FinishLoadingScreen_f() {
	FinishLoadingScreen();
}

void ShowMouse_f() {
	cursorShown = true;
}

void HideMouse_f() {
	cursorShown = false;
}

void Dialog_f() {
	if ( Cmd_Argc() <= 3 ) {
		Com_Printf( "usage: dialog title cvar_name command [width] [height]\n" );
		return;
	}
	const auto argument = []( int i ) { return std::string( Cmd_Argv( i ) ); };
	const std::string title = argument( 1 ), cvar = argument( 2 ), command = argument( 3 ), cancel = argument( 4 );
	const std::string shader = argument( 7 ), okShader = argument( 8 ), cancelShader = argument( 9 );
	OpenDialog( title.c_str(), cvar.c_str(), command.c_str(), cancel.c_str(),
		Cmd_Argc() > 5 ? std::atoi( Cmd_Argv( 5 ) ) : 300, Cmd_Argc() > 6 ? std::atoi( Cmd_Argv( 6 ) ) : 200,
		shader.c_str(), okShader.c_str(), cancelShader.c_str() );
	FocusMenu();
}

/* The frame `saveshot` draws: the view alone. */
bool viewOnly = false;

/* saveshot <path> <width> <height>: one frame of the view alone -- no menu,
 * HUD, message or console -- reduced to a width by height picture and
 * written to <path> (OpenMoHAA CL_SaveShot_f, reference only). The server
 * asks for it as it saves. A name without a slash goes under screenshots/;
 * anything but a TGA inside the game's folders is refused, as a server may
 * send the command too. */
void SaveShot_f() {
	if ( Cmd_Argc() < 4 ) {
		Com_Printf( "usage: saveshot <path> <width> <height>\n" );
		return;
	}
	char path[MAX_OSPATH];
	const char *name = Cmd_Argv( 1 );
	if ( std::strchr( name, '/' ) ) {
		Q_strncpyz( path, name, sizeof( path ) );
	} else {
		Com_sprintf( path, sizeof( path ), "screenshots/%s.tga", name );
	}
	const std::size_t length = std::strlen( path );
	if ( length < 5 || Q_stricmp( path + length - 4, ".tga" ) || std::strstr( path, ".." ) || std::strchr( path, ':' )
		|| std::strchr( path, '\\' ) || path[0] == '/' ) {
		Com_Printf( "saveshot: %s is not a picture this engine writes\n", path );
		return;
	}
	if ( cls.state != CA_ACTIVE ) {
		return;
	}
	viewOnly = true;
	CL_AliceWriteSaveshot( path, std::atoi( Cmd_Argv( 2 ) ), std::atoi( Cmd_Argv( 3 ) ) );
	viewOnly = false;
}

/* dmmapselect <folder> [...]: the maps of maps/<folder>, one of which to host;
 * choosing sets ui_dmmap (OpenMoHAA UI_DMMapSelect_f, MpMapPickerClass). */
void DMMapSelect_f() {
	std::string folder = "maps";
	if ( Cmd_Argc() > 1 && std::strcmp( Cmd_Argv( 1 ), "." ) ) {
		folder += std::string( "/" ) + Cmd_Argv( 1 );
	}
	OpenPicker( "Select a Map", MapItems( folder, "set ui_dmmap", nullptr ) );
}

/* maplist [folder]: every map of a folder, started with spmap when chosen,
 * its subfolders opening the list there (UI_MapList_f, MapRunnerClass). The
 * main menu offers it to developers. */
void MapList_f() {
	std::string folder = "maps";
	if ( Cmd_Argc() == 2 ) {
		folder += std::string( "/" ) + Cmd_Argv( 1 );
	}
	OpenPicker( "Select a Map", MapItems( folder, "spmap", "maplist" ) );
}

/* viewspawnlist [folder]: the model viewer's list of TIKI files under
 * models/; choosing one spawns it with the game's viewspawn (OpenMoHAA
 * UI_ViewSpawnList_f, ViewSpawnerClass). The animate2 menu runs it. */
void ViewSpawnList_f() {
	OpenPicker( "Select a Model", FileItems( "models", PickerFolder( "models", "models" ), { ".tik" }, "viewspawn",
		"viewspawnlist" ) );
}

/* lod_spawnlist [folder]: the same list for the LOD tool, choosing with
 * lod_spawn (UI_LODSpawnList_f, LODSpawnerClass). */
void LODSpawnList_f() {
	OpenPicker( "Select a Model", FileItems( "models", PickerFolder( "models", "models" ), { ".tik" }, "lod_spawn",
		"lod_spawnlist" ) );
}

/* soundpicker [folder]: the sounds under sound/, opening where the last
 * picked one lies; choosing one sets ui_pickedsound, which the ZOUND menu
 * reads, and plays it, as selecting one does in the retail window
 * (UI_SoundPicker_f, SoundPickerClass). */
void SoundPicker_f() {
	std::string last = Cvar_VariableString( "ui_pickedsound" );
	const std::size_t slash = last.rfind( '/' );
	last = slash == std::string::npos ? std::string( "sound" ) : last.substr( 0, slash );
	const std::string folder = PickerFolder( "sound", last );
	std::vector<PickerItem> items = FileItems( "sound", folder, { ".wav", ".mp3" }, "set ui_pickedsound", "soundpicker" );
	for ( PickerItem &item : items ) {
		if ( item.label.empty() || item.label.back() == '/' ) continue;
		const std::string name = item.command.substr( item.command.find( '"' ) );
		item.command += "; play " + name;
	}
	OpenPicker( "Select a Sound", std::move( items ) );
}

/* The side a player model dresses: the Axis models are named german, axis,
 * it or sc (PlayerModelPickerClass::SetupFiles). */
bool AxisModel( const std::string &name ) {
	return !Q_stricmpn( name.c_str(), "german", 6 ) || !Q_stricmpn( name.c_str(), "axis", 4 )
		|| !Q_stricmpn( name.c_str(), "it", 2 ) || !Q_stricmpn( name.c_str(), "sc", 2 );
}

/* playermodel <0|1> [folder]: the Allied or the Axis player models under
 * models/player, view models (_fps) and hidden ones (_) left out; choosing
 * one sets the menu's name, choice and displayed model for that side. 1.00's
 * list is taken to show the files' own names [I]; OpenMoHAA's display names
 * are the later games'. */
void PlayerModel_f() {
	const bool axis = Cmd_Argc() > 1 && std::atoi( Cmd_Argv( 1 ) ) != 0;
	std::string folder = "models/player";
	if ( Cmd_Argc() == 3 ) {
		folder += std::string( "/" ) + Cmd_Argv( 2 );
	}
	const std::string prefix = axis ? "ui_dm_playergermanmodel" : "ui_dm_playermodel";
	const std::string display = axis ? "ui_disp_playergermanmodel" : "ui_disp_playermodel";
	std::vector<PickerItem> items;
	int count = 0;
	char **files = FS_ListFiles( folder.c_str(), ".tik", &count );
	for ( int i = 0; files && i < count; i++ ) {
		std::string model = files[i];
		if ( model.size() <= 4 || model[0] == '_' || std::strstr( model.c_str(), "_fps" ) || AxisModel( model ) != axis ) {
			continue;
		}
		model.resize( model.size() - 4 );
		items.push_back( { model, "set " + prefix + " \"" + model + "\"; set " + prefix + "_set \"" + model
			+ "\"; set " + display + " \"" + folder + "/" + model + ".tik\"" } );
	}
	if ( files ) FS_FreeFileList( files );
	OpenPicker( axis ? "Select Your Axis Player Model" : "Select Your Allied Player Model", std::move( items ) );
}

/* ui_getplayermodel: the menu's Allied and Axis choices and displayed models
 * from dm_playermodel and dm_playergermanmodel, and its name from name
 * (UI_GetPlayerModel_f). The client game asks for it too. */
void GetPlayerModel_f() {
	std::string allied = CvarString( "dm_playermodel" );
	if ( allied.empty() ) allied = "american_army";
	Cvar_Set( "ui_dm_playermodel", allied.c_str() );
	Cvar_Set( "ui_dm_playermodel_set", allied.c_str() );
	Cvar_Set( "ui_disp_playermodel", va( "models/player/%s.tik", allied.c_str() ) );
	std::string axis = CvarString( "dm_playergermanmodel" );
	if ( axis.empty() ) axis = "german_wehrmacht_soldier";
	Cvar_Set( "ui_dm_playergermanmodel", axis.c_str() );
	Cvar_Set( "ui_dm_playergermanmodel_set", axis.c_str() );
	Cvar_Set( "ui_disp_playergermanmodel", va( "models/player/%s.tik", axis.c_str() ) );
	const std::string name = CvarString( "name" );
	Cvar_Set( "ui_name", name.empty() ? "UnnamedSoldier" : name.c_str() );
}

/* ui_applyplayermodel: the menu's choices become the player's
 * (UI_ApplyPlayerModel_f). */
void ApplyPlayerModel_f() {
	const std::string allied = CvarString( "ui_dm_playermodel_set" );
	Cvar_Set( "dm_playermodel", allied.empty() ? "american_army" : allied.c_str() );
	const std::string axis = CvarString( "ui_dm_playergermanmodel_set" );
	Cvar_Set( "dm_playergermanmodel", axis.empty() ? "german_wehrmacht_soldier" : axis.c_str() );
	const std::string name = CvarString( "ui_name" );
	Cvar_Set( "name", name.empty() ? "UnnamedSoldier" : name.c_str() );
}

int ClampedCvar( const char *name, int low, int high ) {
	const int value = std::max( low, std::min( high, Cvar_VariableIntegerValue( name ) ) );
	Cvar_Set( name, va( "%i", value ) );
	return value;
}

/* ui_startdmmap [gametype [name [maplist cvar]]]: a multiplayer server from
 * the Start New Server menu's settings, on its chosen map (UI_StartDMMap_f).
 * The client cannot become a dedicated server here, as fntech3.ded is its own
 * program, so the Dedicated box starts a listen server and says so; nor is
 * there GameSpy to announce to. */
void StartDMMap_f() {
	const int gameType = Cmd_Argc() > 1 ? std::atoi( Cmd_Argv( 1 ) ) : 1;
	const std::string gameTypeName = Cmd_Argc() > 2 ? Cmd_Argv( 2 ) : "Free-For-All";
	const std::string mapListCvar = Cmd_Argc() > 3 ? Cmd_Argv( 3 ) : "sv_maplist";
	const std::string map = CvarString( "ui_dmmap" );
	if ( map.empty() ) {
		return;
	}
	if ( ClampedCvar( "ui_dedicated", 0, 1 ) ) {
		Com_Printf( "This client starts a listen server; run fntech3.ded for a dedicated one.\n" );
	}
	ClampedCvar( "ui_gamespy", 0, 1 );
	const int maxClients = ClampedCvar( "ui_maxclients", 1, FNTECH3_MOHAA_MAX_CLIENTS );
	const int fragLimit = ClampedCvar( "ui_fraglimit", 0, 0x7fffffff );
	const int timeLimit = ClampedCvar( "ui_timelimit", 0, 0x7fffffff );
	const int teamDamage = ClampedCvar( "ui_teamdamage", 0, 1 );
	const int inactiveSpectate = ClampedCvar( "ui_inactivespectate", 0, 0x7fffffff );
	const int inactiveKick = ClampedCvar( "ui_inactivekick", 0, 0x7fffffff );
	const auto safe = []( std::string text, std::size_t limit ) {
		if ( text.size() > limit ) text.resize( limit );
		for ( char &c : text ) {
			if ( c == ';' || c == '"' || c == '\\' ) c = '_';
		}
		return text;
	};
	std::string hostName = CvarString( "ui_hostname" );
	if ( hostName.empty() ) hostName = "Nameless Battle";
	hostName = safe( hostName, 31 );
	Cvar_Set( "ui_hostname", hostName.c_str() );
	Stuff( va( "set sv_maxclients %i; set g_gametype %i; set g_gametypestring \"%s\"; set fraglimit %i; "
		"set timelimit %i; set g_teamdamage %i; set g_inactivespectate %i; set g_inactivekick %i; "
		"set sv_maplist \"%s\"; set sv_hostname \"%s\"; set cheats 0; wait; map \"%s\"",
		maxClients, gameType, safe( gameTypeName, 63 ).c_str(), fragLimit, timeLimit, teamDamage, inactiveSpectate,
		inactiveKick, safe( CvarString( mapListCvar ), 255 ).c_str(), hostName.c_str(), safe( map, 63 ).c_str() ) );
}

/* The briefing room's difficulty buttons: easy, medium and hard before the
 * mission's briefing map starts. 1.00's engine serves these; OpenMoHAA, from
 * the later games, has none, so they are taken to set the skill the game
 * module reads, 0 to 2 [I]. */
void DifficultyEasy_f() { Cvar_Set( "skill", "0" ); }
void DifficultyMedium_f() { Cvar_Set( "skill", "1" ); }
void DifficultyHard_f() { Cvar_Set( "skill", "2" ); }

/* menuconnect <cvar>: connect to the address a menu field wrote into a cvar
 * (CL_MenuConnect_f). */
void MenuConnect_f() {
	if ( Cmd_Argc() != 2 ) {
		return;
	}
	const std::string address = CvarString( Cmd_Argv( 1 ) );
	if ( !address.empty() ) {
		Stuff( "connect \"" + address + "\"" );
	}
}

/* launchgamespy: the internet server list was GameSpy's, whose masters are
 * gone; nothing is pretended. */
void LaunchGameSpy_f() {
	Com_Printf( "GameSpy's server list is no longer available. Join a server by its address, or find one on "
		"your network with Join LAN Game.\n" );
}

const struct {
	const char *name;
	xcommand_t function;
} kCommands[] = {
	{ "pushmenu", PushMenu_f }, { "pushmenu_sp", PushMenuSP_f }, { "pushmenu_dm", PushMenuDM_f },
	{ "forcemenu", ForceMenu_f }, { "popmenu", PopMenu_f }, { "showmenu", ShowMenu_f }, { "hidemenu", HideMenu_f },
	{ "widgetcommand", WidgetCommand_f }, { "globalwidgetcommand", GlobalWidgetCommand_f },
	{ "listmenus", ListMenus_f }, { "loadmenu", LoadMenu_f }, { "ui_hud", Hud_f }, { "ui_addhud", AddHud_f },
	{ "ui_removehud", RemoveHud_f }, { "ui_resetcvars", ResetCvars_f }, { "ui_checkrestart", CheckRestart_f },
	{ "finishloadingscreen", FinishLoadingScreen_f }, { "ui_showmouse", ShowMouse_f },
	{ "ui_hidemouse", HideMouse_f }, { "dialog", Dialog_f }, { "saveshot", SaveShot_f },
	{ "dmmapselect", DMMapSelect_f }, { "maplist", MapList_f }, { "playermodel", PlayerModel_f },
	{ "ui_getplayermodel", GetPlayerModel_f }, { "ui_applyplayermodel", ApplyPlayerModel_f },
	{ "ui_startdmmap", StartDMMap_f }, { "difficultyeasy", DifficultyEasy_f },
	{ "difficultymedium", DifficultyMedium_f }, { "difficultyhard", DifficultyHard_f },
	{ "menuconnect", MenuConnect_f }, { "launchgamespy", LaunchGameSpy_f },
	{ "viewspawnlist", ViewSpawnList_f }, { "lod_spawnlist", LODSpawnList_f }, { "soundpicker", SoundPicker_f },
	{ "ui_startintro", StartIntro_f },
};

/* ---- the hosted module ------------------------------------------------------------ */

/* The subtitles' cvars (below). */
void InitSubtitles();

void Init() {
	menus.clear();
	stack.clear();
	hudList.clear();
	lockedUntil = 0;
	loading = Loading();
	MOHAA_SetLoadResourceHook( LoadResourceHook );
	scoreboard = Scoreboard();
	gameBox = TextBox();
	deathmatchBox = TextBox();
	deathmatchBox.limit = 6;
	hoverMenu = hoverWidget = pressMenu = pressWidget = activeMenu = activeWidget = -1;
	weaponsHudTime = itemsHudTime = lastOwned = lastEquipped = 0;
	cursorShown = true;
	cursorX = VidWidth() * 0.5f;
	cursorY = VidHeight() * 0.5f;
	hudOn = false;
	shots.clear();
	dialog = Dialog();
	picker = Picker();
	viewOnly = false;
	lanScanSeenAt = 0;
	Cvar_Get( "cl_langamerefreshstatus", "", 0 );

	ui_hudScale = Cvar_Get( "ui_hudScale", "1", CVAR_ARCHIVE );
	InitSubtitles();
	Cvar_SetDescription( ui_hudScale, "Medal of Honor: 0 draws the HUD at retail's pixel sizes; otherwise the HUD scales "
		"with the screen height from 480 lines, times this value." );
	ui_crosshair = Cvar_Get( "ui_crosshair", "1", CVAR_ARCHIVE );
	ui_compass = Cvar_Get( "ui_compass", "1", 0 );
	ui_weaponsbar = Cvar_Get( "ui_weaponsbar", "1", CVAR_ARCHIVE );
	ui_weaponsbartime = Cvar_Get( "ui_weaponsbartime", "2500", CVAR_ARCHIVE );
	ui_itemsbar = Cvar_Get( "ui_itemsbar", "0", CVAR_ARCHIVE );
	Cvar_Get( "ui_signshader", "", 0 );
	Cvar_Get( "loadingbar", "0", 0 );
	Cvar_Set( "cg_hud", "0" );

	/* The intro's, with OpenMoHAA's defaults (cl_ui.cpp CL_InitializeUI);
	 * autoexec.cfg sets them before the menus start. */
	cl_playintro = Cvar_Get( "cl_playintro", "1", 0 );
	ui_skip_eamovie = Cvar_Get( "ui_skip_eamovie", "1", 0 );
	ui_skip_titlescreen = Cvar_Get( "ui_skip_titlescreen", "1", 0 );
	ui_skip_legalscreen = Cvar_Get( "ui_skip_legalscreen", "1", 0 );
	ui_titlescreen_fadein = Cvar_Get( "ui_titlescreen_fadein", "1", 0 );
	ui_titlescreen_stay = Cvar_Get( "ui_titlescreen_stay", "3", 0 );
	ui_titlescreen_fadeout = Cvar_Get( "ui_titlescreen_fadeout", "1", 0 );
	ui_legalscreen_fadein = Cvar_Get( "ui_legalscreen_fadein", "1", 0 );
	ui_legalscreen_stay = Cvar_Get( "ui_legalscreen_stay", "3", 0 );
	ui_legalscreen_fadeout = Cvar_Get( "ui_legalscreen_fadeout", "1", 0 );
	Cvar_Get( "cl_movieaudio", "1", 0 );

	sliderLeft.Set( "textures/menu/arrow_left" );
	sliderLeftDown.Set( "textures/menu/arrow_left_sel" );
	sliderRight.Set( "textures/menu/arrow_right" );
	sliderRightDown.Set( "textures/menu/arrow_right_sel" );
	sliderThumb.Set( "textures/menu/slider_thumb" );
	sliderThumbDown.Set( "textures/menu/slider_thumb_sel" );
	listLeft.Set( "gfx/2d/arrow_left.tga" );
	listRight.Set( "gfx/2d/arrow_right.tga" );
	cursorMaterial.Set( "gfx/2d/mouse_cursor.tga" );

	LoadAllMenus();
	mainMenu = FindMenu( "main" );
	hudHealth = FindMenu( "hud_health" );
	hudAmmo = -1;
	hudCompass = FindMenu( "hud_compass" );
	hudWeapons = FindMenu( "hud_weapons" );
	hudItems = FindMenu( "hud_items" );
	crosshairMenu = FindMenu( "crosshair" );

	for ( const auto &command : kCommands ) {
		Cmd_AddCommand( command.name, command.function );
	}
	/* The team and model menus show the models these name from their first
	 * showing [I]; the menus and the client game fill them again later. */
	GetPlayerModel_f();
	CL_MOHAAMusicInit();
}

/* The load record's hook stays (see the loading bar). An intro still playing
 * ends: a map loads, or the menus start again. */
void Shutdown() {
	for ( const auto &command : kCommands ) {
		Cmd_RemoveCommand( command.name );
	}
	intro = Intro();
	CL_MOHAAMusicShutdown();
	SetPause( false );
	menus.clear();
	stack.clear();
	hudList.clear();
	shots.clear();
	dialog = Dialog();
	picker = Picker();
}

/* Menus up in an order a later one draws over: overlays the client game or a
 * command showed, then the stack from the bottom. */
void DrawMenus( bool includeOverlays ) {
	if ( includeOverlays ) {
		for ( std::size_t i = 0; i < menus.size(); i++ ) {
			const int index = static_cast<int>( i );
			if ( menus[i].visible && !IsHudMenu( index ) && !OnStack( index ) && index != scoreboard.menu ) {
				DrawMenu( index );
			}
		}
	}
	for ( int index : stack ) {
		DrawMenu( index );
	}
}

/* UI_REFRESH, every frame a menu has the keys. The connection and loading
 * screens draw from UI_DRAW_CONNECT_SCREEN instead. */
void Refresh() {
	if ( viewOnly ) {
		return;
	}
	const DrawingScope drawing;
	ServiceTimers();
	ServiceRepeat();
	CL_MOHAAMusicUpdate();
	if ( intro.stage ) {
		IntroRefresh();
		if ( intro.stage ) {
			return;
		}
	}
	if ( cls.state >= CA_CONNECTING && cls.state <= CA_PRIMED ) {
		return;
	}
	if ( loading.active && cls.state == CA_ACTIVE ) {
		ServerLoaded();
	}
	if ( CurrentMenu() < 0 && !loading.active && !dialog.open && !picker.open ) {
		/* Nothing to show: give the keys back. */
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		UpdatePause();
		return;
	}
	/* UI_Update keeps the main menus' widgets current every frame. */
	UpdateMenuWidgets( CurrentMenu() );
	if ( !dialog.open && !picker.open ) {
		UpdateHover();
	}
	UpdatePause();
	DrawMenus( cls.state != CA_ACTIVE );
	DrawPicker();
	DrawDialog();
	DrawCursor();
}

void DrawConnectScreen() {
	if ( viewOnly ) {
		return;
	}
	const DrawingScope drawing;
	ServiceTimers();
	CL_MOHAAMusicUpdate();
	UpdateConnectScreen();
	DrawMenus( false );
}

void SetActiveMenu( int menu ) {
	/* The intro's own movies start and end here, and it keeps the keys. */
	if ( intro.stage && ( menu == UIMENU_NONE || menu == UIMENU_MAIN ) ) {
		ClearMenus( true );
		IntroHoldKeys();
		return;
	}
	switch ( menu ) {
	case UIMENU_NONE:
		if ( !loading.active ) {
			ForceMenuOff( true );
		}
		break;
	case UIMENU_MAIN:
		HideLevelMenus();
		if ( CurrentMenu() < 0 ) {
			/* The menu coming up with nothing before a game starts the main
			 * theme over (CL_Frame, S_TriggeredMusic_PlayIntroMusic). */
			if ( PushMenu( "main" ) && cls.state == CA_DISCONNECTED
				&& !( com_sv_running && com_sv_running->integer ) ) {
				CL_MOHAAMusicMainTheme();
			}
		}
		FocusMenu();
		break;
	case UIMENU_INGAME:
		MenuEscape( "main" );
		break;
	default:
		break;
	}
}

intptr_t QDECL Entry( int command, ... ) {
	int arg[3] = {};
	va_list ap;
	va_start( ap, command );
	for ( int &value : arg ) {
		value = va_arg( ap, int );
	}
	va_end( ap );

	switch ( command ) {
	case UI_INIT:
		Init();
		return 0;
	case UI_SHUTDOWN:
		Shutdown();
		return 0;
	case UI_KEY_EVENT:
		KeyEvent( arg[0], arg[1] != 0 );
		return 0;
	case UI_MOUSE_EVENT:
		MouseEvent( arg[0], arg[1] );
		return 0;
	case UI_REFRESH:
		Refresh();
		return 0;
	case UI_IS_FULLSCREEN: {
		const int current = CurrentMenu();
		return current >= 0 && menus[current].fullscreen ? qtrue : qfalse;
	}
	case UI_SET_ACTIVE_MENU:
		SetActiveMenu( arg[0] );
		return 0;
	case UI_DRAW_CONNECT_SCREEN:
		DrawConnectScreen();
		return 0;
	case UI_MENUS_ANY_VISIBLE:
		return CurrentMenu() >= 0 ? qtrue : qfalse;
	default:
		return 0;
	}
}

/* ---- subtitles ---------------------------------------------------------- */

/* An alias's subtitle, which the game or the client game leaves in
 * subtitle0..3 when g_subtitle is on or the alias forces one (both 1.00
 * libraries name "subtitle%d", "curSubtitle" and "g_subtitle"; the advanced
 * options menu's "subtitles" box sets g_subtitle). 2015's view draws them,
 * not the client game: white over a black shadow from ten lines above the
 * foot of the screen, broken at four fifths of its width, each for 2.5
 * seconds a 68 characters and 1.5 more, fading in and out over 0.75 seconds,
 * at `subAlpha`; then it clears the cvar (OpenMoHAA View3D::InitSubtitle and
 * DrawSubtitleOverlay, reference only). They scale with the HUD. */
constexpr int kSubtitles = 4;

struct Subtitle {
	cvar_t *cvar = nullptr;
	std::string text;
	float life = 0.0f;
	float left = 0.0f;
};

Subtitle subtitles[kSubtitles];
cvar_t *subAlpha;

void InitSubtitles() {
	for ( int i = 0; i < kSubtitles; i++ ) {
		Subtitle &sub = subtitles[i];
		sub.cvar = Cvar_Get( va( "subtitle%d", i ), "", 0 );
		sub.text = sub.cvar->string;
		sub.life = sub.left = 4000.0f;
	}
	subAlpha = Cvar_Get( "subAlpha", "0.5", 0 );
}

float SubtitleAlpha( const Subtitle &sub ) {
	if ( sub.left > sub.life - 750.0f ) {
		return 1.0f - ( sub.left - ( sub.life - 750.0f ) ) / 750.0f;
	}
	if ( sub.left < 750.0f ) {
		return sub.left / 750.0f;
	}
	return 1.0f;
}

void DrawSubtitles() {
	for ( int i = 0; i < kSubtitles; i++ ) {
		Subtitle &sub = subtitles[i];
		if ( !sub.cvar ) {
			return;
		}
		if ( sub.text != sub.cvar->string ) {
			sub.text = sub.cvar->string;
			sub.life = sub.left = 2500.0f * static_cast<float>( sub.text.size() / 68 + 1 ) + 1500.0f;
		}
	}
	void *font = CL_MOHAAFontLoad( "facfont-20" );
	const Scale s = HudScale();
	const float lineHeight = FontHeight( font ) * s.y;
	const float top = VidHeight() - lineHeight * 10.0f;
	const float width = VidWidth() * 0.8f / s.x - 20.0f;
	const float shade = subAlpha ? subAlpha->value : 0.5f;
	int line = 0;
	for ( int i = 0; i < kSubtitles; i++ ) {
		Subtitle &sub = subtitles[i];
		const float alpha = SubtitleAlpha( sub ) * shade;
		if ( font && sub.left > 0.0f && !sub.text.empty() ) {
			const std::string lines = BreakLines( font, sub.text, width );
			std::size_t start = 0;
			while ( start <= lines.size() ) {
				std::size_t end = lines.find( '\n', start );
				if ( end == std::string::npos ) end = lines.size();
				const std::string part = lines.substr( start, end - start );
				const float y = top + lineHeight * static_cast<float>( line++ );
				SetColor( kBlack, alpha );
				Print( font, 18.0f, ( y + 1.0f ) / s.y, part.c_str(), s );
				SetColor( kWhite, alpha );
				Print( font, 20.0f, y / s.y, part.c_str(), s );
				start = end + 1;
			}
		}
		sub.left -= static_cast<float>( cls.frametime );
		if ( sub.left < 0.0f ) {
			sub.left = 0.0f;
			sub.text.clear();
			if ( sub.cvar->string[0] ) {
				Cvar_Set( sub.cvar->name, "" );
			}
		}
	}
	re.SetColor( nullptr );
}

} // namespace

vm_t *CL_MOHAALoadUI( void ) {
	return VM_CreateHosted( VM_UI, Entry );
}

bool CL_MOHAAViewOnlyFrame( void ) {
	return viewOnly;
}

bool CL_MOHAAIntroMovieEnded( void ) {
	if ( !intro.stage ) {
		return false;
	}
	intro.movieEnded = true;
	return true;
}

/* Each call uploads a new picture: the renderer keeps the first shader of a
 * name and drops the rest, but not their images. CL_InitRenderer registers
 * it once a renderer start, as cls.whiteShader, and draws use that. */
qhandle_t CL_MOHAAWhite( void ) {
	byte white[4] = { 255, 255, 255, 255 };
	const qhandle_t shader = re.RegisterShaderFromRGBA
		? re.RegisterShaderFromRGBA( "*fntech3/mohaa/white", white, 1, 1 ) : 0;
	/* The renderer's own untextured picture, as a last resort. */
	return shader ? shader : re.RegisterShaderNoMip( "*white" );
}

/* The HUD, the menus the client game showed and the scoreboard, over the
 * view, each frame the game draws. */
void CL_MOHAAUIDrawHud( void ) {
	if ( menus.empty() || viewOnly ) {
		return;
	}
	const DrawingScope drawing;
	ServiceTimers();
	CL_MOHAAMusicUpdate();
	if ( loading.active && cls.state == CA_ACTIVE ) {
		ServerLoaded();
	}
	UpdateHud();
	UpdatePause();
	/* The view's subtitles, after the prints and before the HUD. */
	DrawSubtitles();
	/* The client game draws the objectives overlay at a place of its own in
	 * screen pixels, its backdrop's top at 125 (cgamex86.dll 0x3001f3f5), where
	 * retail's 128-pixel compass ends. A compass scaled larger covers the
	 * overlay's heading, so it fades out as the overlay comes up, as the
	 * message boxes do; one at retail's size is left as retail leaves it. Its
	 * ring's shader ignores the colour's alpha (`alphaGen oneMinusVertex`,
	 * which Ritual's back end has no case for), so the compass goes once the
	 * overlay is half up. */
	constexpr float kRetailCompassBottom = 128.0f;
	const float objectiveAlpha = CL_MOHAACGameObjectiveAlpha();
	for ( int index : { hudHealth, hudAmmo, hudCompass, hudWeapons, hudItems } ) {
		if ( index < 0 ) {
			continue;
		}
		float fade = 1.0f;
		if ( index == hudCompass && objectiveAlpha > 0.0f && menus[index].visible ) {
			PlaceMenu( menus[index] );
			const float bottom = menus[index].frame[1] + menus[index].frame[3];
			if ( bottom > kRetailCompassBottom ) {
				fade = 1.0f - objectiveAlpha;
			}
		}
		if ( fade > 0.5f ) {
			DrawMenu( index, fade );
		}
	}
	for ( int index : hudList ) {
		DrawMenu( index );
	}
	if ( crosshairMenu >= 0 ) {
		DrawMenu( crosshairMenu );
	}
	for ( std::size_t i = 0; i < menus.size(); i++ ) {
		const int index = static_cast<int>( i );
		if ( menus[i].visible && !IsHudMenu( index ) && !OnStack( index ) && index != scoreboard.menu ) {
			DrawMenu( index );
		}
	}
	/* The objectives overlay fades the message boxes out as it comes up. */
	PlaceBoxes();
	const float boxAlpha = 1.0f - CL_MOHAACGameObjectiveAlpha();
	DrawTextBox( gameBox, boxAlpha );
	DrawTextBox( deathmatchBox, boxAlpha );
	if ( scoreboard.menu >= 0 ) {
		DrawMenu( scoreboard.menu );
	}
	DrawScoreboard();
}

/* UI_PrintConsole's routing of a coded line: game messages to the game box,
 * chat and deaths to the deathmatch box, each in its kind's colour and for
 * its kind's time. */
void CL_MOHAAUIPrint( const char *text ) {
	if ( !text || text[0] < MessageYellow || text[0] >= MessageMax ) {
		return;
	}
	const int code = text[0];
	std::string body = text + 1;
	while ( !body.empty() && ( body.back() == '\n' || body.back() == '\r' ) ) {
		body.pop_back();
	}
	if ( body.empty() ) {
		return;
	}
	void *font = CL_MOHAAFontLoad( "facfont-20" );
	PlaceBoxes();
	const Scale s = HudScale();
	BoxLine line;
	if ( code == MessageYellow || code == MessageWhite ) {
		line.color = code == MessageWhite ? kWhite : kHudColor;
		line.seconds = code == MessageWhite ? 10.0f : 5.0f;
		line.text = BreakLines( font, body, gameBox.frame[2] / s.x );
		AddBoxLine( gameBox, std::move( line ) );
		return;
	}
	if ( code == MessageChatWhite ) {
		line.color = { 0.75f, 0.75f, 0.75f, 1.0f };
		line.seconds = 8.0f;
	} else {
		line.color = code == MessageChatRed ? Color{ 1.0f, 0.333f, 0.333f, 1.0f } : Color{ 0.0f, 1.0f, 0.333f, 1.0f };
		line.seconds = 6.0f;
	}
	line.text = BreakLines( font, body, deathmatchBox.frame[2] / s.x );
	AddBoxLine( deathmatchBox, std::move( line ) );
}

void CL_MOHAAUIShowMenu( const char *name, bool force ) {
	ShowNamedMenu( name, force );
}

void CL_MOHAAUIHideMenu( const char *name, bool force ) {
	HideNamedMenu( name, force );
}

/* UI_ShowScoreboard_f: the named menu and the list, unless a menu is up. */
void CL_MOHAAUIShowScoreboard( const char *menuName ) {
	Scoreboard &sb = scoreboard;
	if ( menuName ) {
		if ( !sb.menuName.empty() && Q_stricmp( sb.menuName.c_str(), menuName ) && sb.menu >= 0 ) {
			ForceHide( menus[sb.menu] );
		}
		sb.menuName = menuName;
	}
	sb.menu = sb.menuName.empty() ? -1 : FindMenu( sb.menuName.c_str() );
	if ( CurrentMenu() >= 0 ) {
		if ( sb.menu >= 0 ) ForceHide( menus[sb.menu] );
		sb.shown = false;
		return;
	}
	if ( sb.menu >= 0 ) ForceShow( menus[sb.menu] );
	sb.shown = true;
}

void CL_MOHAAUIHideScoreboard( void ) {
	scoreboard.shown = false;
	if ( scoreboard.menu >= 0 ) {
		ForceHide( menus[scoreboard.menu] );
	}
}

void CL_MOHAAUISetScoreboardItem( int item, const char *const data[8], const float *textColor,
	const float *backColor, bool header ) {
	if ( item < 0 || item >= 64 ) {
		return;
	}
	MakeScoreboard();
	if ( !scoreboard.made ) {
		return;
	}
	if ( static_cast<std::size_t>( item ) >= scoreboard.rows.size() ) {
		scoreboard.rows.resize( static_cast<std::size_t>( item ) + 1 );
	}
	ScoreRow &row = scoreboard.rows[static_cast<std::size_t>( item )];
	for ( int i = 0; i < 8; i++ ) {
		row.cells[i] = data && data[i] ? data[i] : "";
	}
	row.colored = textColor && backColor;
	if ( row.colored ) {
		row.text = { textColor[0], textColor[1], textColor[2], textColor[3] };
		row.back = { backColor[0], backColor[1], backColor[2], backColor[3] };
	}
	row.header = header;
}

void CL_MOHAAUIDeleteScoreboardItems( int maxIndex ) {
	if ( maxIndex >= 0 && static_cast<std::size_t>( maxIndex ) < scoreboard.rows.size() ) {
		scoreboard.rows.resize( static_cast<std::size_t>( maxIndex ) );
	}
}
