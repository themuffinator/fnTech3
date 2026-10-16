/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

The Jedi single-player games' menus

Neither Star Wars Jedi Knight: Jedi Academy's single player (jka-sp) nor Jedi
Outcast's (jk2-sp) ships a menu module. Their engines read `ui/menus.txt` and
the `.menu` files it names and run those menus themselves: jasp.exe 1.01's
menu code sits at 0x494000..0x4a8000 (UI_Init 0x494ae0, UI_MainMenu
0x49a080, UI_LoadMenus 0x498640, the console commands 0x494a20), jk2sp.exe
1.02a's beside it (UI_Init 0x4994b0, UI_Load 0x49aac0). This file is that
engine menu system, hosted as the client's UI module (VM_CreateHosted) as
Alice's and Medal of Honor's are, so the client's uivm call sites -- keys,
mouse, the active-menu requests, the full-screen query, console commands
and shutdown -- reach it unchanged.

code/compat/raven_menu.hpp reads the files with each engine's own keywords
and named values. The runtime here follows the engines: menus open over the
one that had focus and give it back when they close, a script's commands are
each engine's table and anything else is the game's own script (`uiScript`),
the mouse moves a 640x480 cursor, keys go to the focused item and then to
the menu. Raven's released sources (OpenJK code/ui, GPL-2.0) were read as
reference for that behaviour; none of their text is used.

Presentation (AGENTS.md invariant 11): the virtual screen reaches the window
as the Jedi multiplayer menus do (CL_UIVirtualToScreen: stretched, or with
cl_menuAspect 1 in the centred 4:3 area), text goes through the Raven font
path the multiplayer hosts use, and every menu sound is an interface sound.

Starting or loading a game reaches the single-player game and client
boundary, which fnTech3 does not serve yet; that stops with its own error.
===========================================================================
*/

extern "C" {
#include "client.h"
}

#include "client_cpp.h"
#include "../ui/ui_public.h"
#include "../compat/raven_menu.hpp"
#include "client_jka_font.hpp"
#include "client_jka_strings.hpp"
#include "client_jk2_font.hpp"
#include "client_jk2_strings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace rm = fntech3::raven::menu;

/* ---- constants ------------------------------------------------------------ */

constexpr float kScreenWidth = 640.0f, kScreenHeight = 480.0f;
constexpr float kScrollbarSize = 16.0f;
constexpr float kSliderWidth = 128.0f, kSliderHeight = 16.0f;
constexpr float kSliderThumbWidth = 12.0f, kSliderThumbHeight = 16.0f;
constexpr int kPulseDivisor = 75, kBlinkDivisor = 600;
constexpr int kMaxOpenMenus = 16;
constexpr int kDoubleClickDelay = 300;
constexpr float kCursorSize = 48.0f;

// Raven's text styles (menudef.h ITEM_TEXTSTYLE_*) and the font flags they take.
enum TextStyle { StyleNormal, StyleBlink, StylePulse, StyleShadowed, StyleOutlined, StyleOutlineShadowed,
	StyleShadowedMore };
constexpr std::uint32_t kFontBlink = 0x40000000u, kFontDropShadow = 0x80000000u;

// The feeders the single-player menus name (menudef.h FEEDER_*).
enum Feeder {
	FeederSaveGames = 0, FeederMods = 9, FeederPlayerSpecies = 16, FeederSkinHead = 17, FeederSkinTorso = 18,
	FeederSkinLegs = 19, FeederColorChoices = 20, FeederMoves = 21, FeederMovesTitles = 22, FeederLanguages = 23
};

// Their owner draws (menudef.h UI_*).
enum OwnerDraw {
	DrawDatapadMission = 203, DrawDatapadWeapons = 204, DrawDatapadInventory = 205, DrawDatapadForcePowers = 206,
	DrawSaveShot = 236, DrawGlInfo = 249, DrawKeyBindStatus = 250
};

// The string package flag jk2sp.exe's UI_Load registers its menus with (0x49aae1).
constexpr unsigned kOutcastMenuStrings = 4;

/* ---- state ---------------------------------------------------------------- */

struct Mod { std::string name, description; };

struct State {
	bool academy = false;
	bool loaded = false;
	rm::Assets assets;
	std::vector<std::unique_ptr<rm::Menu>> menus;
	std::vector<rm::Menu *> menuStack;          // what had focus under each open menu
	std::vector<int> fonts;                     // menu font number - 1 -> host font handle
	int mediumFont = 0;
	qhandle_t white = 0, cursor = 0, gradientBar = 0;
	qhandle_t scrollBar = 0, scrollUp = 0, scrollDown = 0, scrollLeft = 0, scrollRight = 0, scrollThumb = 0;
	qhandle_t sliderBar = 0, sliderThumb = 0;
	sfxHandle_t itemFocusSound = 0, nullSound = 0;
	float cursorX = 0.0f, cursorY = 0.0f;
	bool cursorShow = true;
	int realTime = 0, frameTime = 0;
	bool debugMode = false;
	// Binding, text editing and capture.
	bool waitingForKey = false;
	rm::Item *bindItem = nullptr;
	bool editingField = false;
	rm::Item *editItem = nullptr;
	rm::Item *itemCapture = nullptr;
	int lastListClick = 0;
	// The deferred part of a script (defer, rundeferred).
	std::string deferredScript;
	rm::Item *deferredItem = nullptr;
	// Key bindings for the bind items: each command's two keys.
	std::vector<std::array<int, 2>> bindKeys;
	// Lists the menus show.
	std::vector<Mod> mods;
	int modIndex = 0;
	std::vector<std::string> saves;
	int saveCursor = 0;
	bool savesRead = false;
	int languageModification = -1;
};

State ui;

bool Academy() { return ui.academy; }

/* ---- strings ------------------------------------------------------------- */

/* A `@PACKAGE_KEY` reference read from the game's own string packages:
 * Academy's StringEd tables for the selected language (SE_GetString),
 * Outcast's StripEd packages its menus registered (SP_GetStringTextString). */
std::string Localize( const std::string &text ) {
	if ( text.empty() || text[0] != '@' ) return text;
	const std::string key = text.substr( 1 );
	if ( Academy() ) {
		const auto value = fntech3::jka::strings::Lookup( fntech3::jka::hoststrings::Table(), key );
		return std::string( value );
	}
	namespace host = fntech3::jk2::hoststrings;
	const unsigned language = host::Language();
	for ( const auto &entry : host::Storage() ) {
		const auto value = fntech3::jk2::strings::Lookup( entry.second.package, key, language );
		if ( !value.empty() ) return std::string( value );
	}
	return std::string();
}

int LanguageModification() {
	const cvar_t *language = Cvar_Get( Academy() ? "se_language" : "sp_language", Academy() ? "english" : "0",
		CVAR_ARCHIVE | CVAR_NORESTART );
	return language ? language->modificationCount : 0;
}

/* ---- cvars and commands -------------------------------------------------- */

std::string CvarString( const std::string &name ) {
	char buffer[MAX_CVAR_VALUE_STRING];
	Cvar_VariableStringBuffer( name.c_str(), buffer, sizeof( buffer ) );
	return buffer;
}

float CvarValue( const std::string &name ) { return Cvar_VariableValue( name.c_str() ); }

void Execute( const std::string &text ) { Cbuf_ExecuteText( EXEC_APPEND, text.c_str() ); }

/* ---- sound --------------------------------------------------------------- */

sfxHandle_t RegisterSound( const std::string &name ) {
	return name.empty() ? 0 : S_RegisterSound( name.c_str(), qfalse );
}

void PlaySound( sfxHandle_t sfx, int channel ) {
	if ( sfx ) S_StartInterfaceSound( sfx, channel, 1.0f );
}

/* ---- drawing -------------------------------------------------------------- */

void SetColor( const float *rgba ) { re.SetColor( rgba ); }

void DrawPic( float x, float y, float w, float h, qhandle_t shader ) {
	if ( !shader ) return;
	CL_UIVirtualToScreen( &x, &y, &w, &h );
	re.DrawStretchPic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, shader );
}

void FillRect( float x, float y, float w, float h, const float *color ) {
	SetColor( color );
	DrawPic( x, y, w, h, ui.white );
	SetColor( nullptr );
}

void DrawSides( float x, float y, float w, float h, float size ) {
	DrawPic( x, y, size, h, ui.white );
	DrawPic( x + w - size, y, size, h, ui.white );
}

void DrawTopBottom( float x, float y, float w, float h, float size ) {
	DrawPic( x, y, w, size, ui.white );
	DrawPic( x, y + h - size, w, size, ui.white );
}

void DrawRect( float x, float y, float w, float h, float size, const float *color ) {
	SetColor( color );
	DrawTopBottom( x, y, w, h, size );
	DrawSides( x, y, w, h, size );
	SetColor( nullptr );
}

/* The menus' font numbers are the engines' font handles in the order the
 * assets registered them (a clean start's handles, the ones the files were
 * written against); any other number takes the medium font. */
int FontHandle( int number ) {
	if ( number >= 1 && number <= static_cast<int>( ui.fonts.size() ) ) return ui.fonts[number - 1];
	return ui.mediumFont;
}

int RegisterFont( const std::string &name ) {
	return Academy() ? CL_JKARegisterFont( name.c_str() ) : CL_JK2RegisterFont( name.c_str() );
}

int TextWidth( const std::string &text, float scale, int font ) {
	const int handle = FontHandle( font );
	if ( Academy() ) {
		const auto *found = CL_JKAFindFont( handle );
		return found ? fntech3::jka::font::Width( found->metrics, text.c_str(), scale ) : 0;
	}
	const auto *found = CL_JK2FindFont( handle );
	return found ? fntech3::jk2::font::Width( found->metrics, text.c_str(), scale ) : 0;
}

int TextHeight( float scale, int font ) {
	const int handle = FontHandle( font );
	float height = 0.0f;
	if ( Academy() ) {
		if ( const auto *found = CL_JKAFindFont( handle ) ) height = found->metrics.pointSize * scale;
	} else if ( const auto *found = CL_JK2FindFont( handle ) ) {
		height = found->metrics.pointSize * scale;
	}
	return std::isfinite( height ) ? static_cast<int>( height ) : 0;
}

/* Text_Paint (jasp.exe 0x4951a0, jk2sp.exe 0x4999d0): the font path takes
 * whole virtual pixels (both truncate x and y), font 0 is the default font,
 * and only styles 2 to 6 reach the font: 2 (pulse) blinks and 3 to 6 drop a
 * shadow (the style less 2 indexes a five-entry table, 0x495210 and 0x499a40).
 * Style 1, which the menus' buttons use, draws plain text. */
void DrawText( float x, float y, float scale, const float *color, const std::string &text, int maxPixelWidth,
	int style, int font ) {
	if ( text.empty() ) return;
	std::uint32_t flags = 0;
	switch ( style ) {
	case StylePulse: flags = kFontBlink; break;
	case StyleShadowed: case StyleOutlined: case StyleOutlineShadowed: case StyleShadowedMore:
		flags = kFontDropShadow; break;
	default: break;
	}
	const int limit = maxPixelWidth ? maxPixelWidth : -1;
	const float ox = static_cast<float>( static_cast<int>( x ) ), oy = static_cast<float>( static_cast<int>( y ) );
	const int handle = FontHandle( font );
	auto draw = []( float gx, float gy, float gw, float gh, const fntech3::raven::font::Glyph &glyph,
		qhandle_t shader ) {
		CL_UIVirtualToScreen( &gx, &gy, &gw, &gh );
		re.DrawStretchPic( gx, gy, gw, gh, glyph.s, glyph.t, glyph.s2, glyph.t2, shader );
	};
	if ( Academy() ) {
		if ( const auto *found = CL_JKAFindFont( handle ) )
			CL_JKADrawFont( *found, text.c_str(), ox, oy, color, flags, limit, scale, draw );
	} else if ( const auto *found = CL_JK2FindFont( handle ) ) {
		CL_JK2DrawFont( *found, text.c_str(), ox, oy, color, flags, limit, scale, draw );
	}
}

/* ---- menus ----------------------------------------------------------------- */

bool Same( const std::string &a, const std::string &b ) { return !Q_stricmp( a.c_str(), b.c_str() ); }

rm::Menu *FindMenu( const std::string &name ) {
	for ( auto &menu : ui.menus ) {
		if ( Same( menu->window.name, name ) ) return menu.get();
	}
	return nullptr;
}

rm::Menu *Focused() {
	for ( auto &menu : ui.menus ) {
		const auto flags = menu->window.flags;
		if ( ( flags & rm::flag::HasFocus ) && ( flags & rm::flag::Visible ) ) return menu.get();
	}
	return nullptr;
}

bool AnyFullScreenVisible() {
	for ( auto &menu : ui.menus ) {
		if ( ( menu->window.flags & rm::flag::Visible ) && menu->fullScreen ) return true;
	}
	return false;
}

rm::Item *FindItem( rm::Menu *menu, const std::string &name ) {
	if ( !menu ) return nullptr;
	for ( auto &item : menu->items ) {
		if ( Same( item->window.name, name ) ) return item.get();
	}
	return nullptr;
}

// Menu_ItemsMatchingGroup / Menu_GetMatchingItemByNumber: by name or group.
std::vector<rm::Item *> Matching( rm::Menu *menu, const std::string &name ) {
	std::vector<rm::Item *> found;
	if ( !menu ) return found;
	for ( auto &item : menu->items ) {
		if ( Same( item->window.name, name ) || ( !item->window.group.empty() && Same( item->window.group, name ) ) )
			found.push_back( item.get() );
	}
	return found;
}

void UpdatePosition( rm::Item &item ) {
	if ( !item.parent ) return;
	float x = item.parent->window.rect.x, y = item.parent->window.rect.y;
	if ( item.parent->window.border ) {
		x += item.parent->window.borderSize;
		y += item.parent->window.borderSize;
	}
	rm::Reader::SetScreenCoords( item, x, y );
}

void RunScript( rm::Item &item, const std::string &script );
void HandleMouseMove( rm::Menu *menu, float x, float y );

/* A menu's own script (onOpen, onClose, onESC, onAccept) runs on a stand-in
 * item that belongs to it, as the engines run it; a delay it reaches is the
 * menu's to resume. */
void RunMenuScript( rm::Menu &menu, const std::string &script ) {
	if ( script.empty() ) return;
	rm::Item stand;
	stand.parent = &menu;
	RunScript( stand, script );
	if ( stand.window.flags & rm::flag::ScriptWaiting ) {
		menu.window.flags |= rm::flag::ScriptWaiting;
		menu.window.delayedScript = stand.window.delayedScript;
		menu.window.delayTime = stand.window.delayTime;
	}
}

void Activate( rm::Menu &menu ) {
	menu.window.flags |= rm::flag::HasFocus | rm::flag::Visible;
	RunMenuScript( menu, menu.onOpen );
	menu.appearanceTime = 0;
	menu.appearanceCount = 0;
}

void LoadMenus( const std::string &file, bool reset );

/* Menus_ActivateByName: the named menu opens over the one with focus, which
 * waits on the stack. Academy loads its three mission-select menus only when
 * one is asked for (jasp.exe keeps them in ui/tier1..3.txt). */
rm::Menu *ActivateByName( const std::string &name ) {
	rm::Menu *focus = Focused();
	rm::Menu *found = nullptr;
	for ( auto &menu : ui.menus ) {
		if ( Same( menu->window.name, name ) ) {
			found = menu.get();
			Activate( *found );
			if ( static_cast<int>( ui.menuStack.size() ) < kMaxOpenMenus && focus ) ui.menuStack.push_back( focus );
		} else {
			menu->window.flags &= ~rm::flag::HasFocus;
		}
	}
	if ( !found ) {
		if ( Academy() && ( Same( name, "ingameMissionSelect1" ) || Same( name, "ingameMissionSelect2" ) ||
			Same( name, "ingameMissionSelect3" ) ) ) {
			LoadMenus( std::string( "ui/tier" ) + name.back() + ".txt", false );
			for ( auto &menu : ui.menus ) menu->window.flags &= ~( rm::flag::HasFocus | rm::flag::Visible );
			ui.menuStack.clear();
			return ActivateByName( name );
		}
		Com_Printf( S_COLOR_YELLOW "WARNING: Menus_ActivateByName: Unable to find menu \"%s\"\n", name.c_str() );
	}
	if ( Academy() && ( Same( name, "ingameForceSelect" ) || Same( name, "ingameWpnSelect" ) ) &&
		Cvar_VariableIntegerValue( "tier_storyinfo" ) == 1 ) {
		ActivateByName( Same( name, "ingameForceSelect" ) ? "ingameForceHelp" : "ingameWpnSelectHelp" );
	}
	HandleMouseMove( found, ui.cursorX, ui.cursorY );
	return found;
}

void RunCloseScript( rm::Menu &menu ) {
	if ( ( menu.window.flags & rm::flag::Visible ) && !menu.onClose.empty() ) {
		rm::Item stand;
		stand.parent = &menu;
		RunScript( stand, menu.onClose );
	}
}

void CloseByName( const std::string &name ) {
	rm::Menu *menu = FindMenu( name );
	if ( !menu ) return;
	RunCloseScript( *menu );
	if ( ( menu->window.flags & rm::flag::HasFocus ) && !ui.menuStack.empty() ) {
		ui.menuStack.back()->window.flags |= rm::flag::HasFocus;
		ui.menuStack.pop_back();
	}
	menu->window.flags &= ~( rm::flag::Visible | rm::flag::HasFocus );
}

void CloseAll() {
	for ( auto &menu : ui.menus ) {
		RunCloseScript( *menu );
		menu->window.flags &= ~( rm::flag::HasFocus | rm::flag::Visible );
	}
	ui.menuStack.clear();
}

void ShowItems( rm::Menu *menu, const std::string &name, bool show ) {
	const auto items = Matching( menu, name );
	if ( items.empty() ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: Menu_ShowItemByName - unable to locate any items named: \"%s\"\n",
			name.c_str() );
	}
	for ( rm::Item *item : items ) {
		if ( show ) item->window.flags |= rm::flag::Visible;
		else item->window.flags &= ~( rm::flag::Visible | rm::flag::HasFocus );
	}
}

// Menu_ClearFocus: every focused item loses it and runs its leaveFocus.
rm::Item *ClearFocus( rm::Menu *menu ) {
	rm::Item *previous = nullptr;
	if ( !menu ) return nullptr;
	for ( auto &item : menu->items ) {
		if ( item->window.flags & rm::flag::HasFocus ) {
			previous = item.get();
			item->window.flags &= ~rm::flag::HasFocus;
			if ( !item->leaveFocus.empty() ) RunScript( *item, item->leaveFocus );
		}
	}
	return previous;
}

/* ---- cvar rules ------------------------------------------------------------ */

/* Item_EnableShowViaCvar: whether the item's cvar test passes for the given
 * rule (enable or show). The list names values; Academy's substring rule
 * looks for one value inside the cvar instead. */
bool EnableShowViaCvar( const rm::Item &item, int rule ) {
	if ( item.enableCvar.empty() || item.cvarTest.empty() ) return true;
	std::string list = item.enableCvar, compared;
	if ( item.cvarFlags & rm::cvarRule::Substring ) {
		rm::Tokens first( item.enableCvar );
		const std::string value = first.Next( false );
		if ( value.empty() ) return !( item.cvarFlags & rule );
		compared = value;
		list = CvarString( item.cvarTest );
	} else {
		compared = CvarString( item.cvarTest );
	}
	rm::Tokens tokens( list );
	for ( ;; ) {
		const std::string value = tokens.Next( false );
		if ( value.empty() ) return !( item.cvarFlags & rule );
		if ( value == ";" ) continue;
		if ( Same( compared, value ) ) return ( item.cvarFlags & rule ) != 0;
	}
}

bool Enabled( const rm::Item &item ) {
	return !( item.cvarFlags & ( rm::cvarRule::Enable | rm::cvarRule::Disable ) ) ||
		EnableShowViaCvar( item, rm::cvarRule::Enable );
}

bool Shown( const rm::Item &item ) {
	return !( item.cvarFlags & ( rm::cvarRule::Show | rm::cvarRule::Hide ) ) ||
		EnableShowViaCvar( item, rm::cvarRule::Show );
}

/* ---- feeders ---------------------------------------------------------------- */

void ReadSaves();
void LoadMods();

int FeederCount( float feeder ) {
	switch ( static_cast<int>( feeder ) ) {
	case FeederSaveGames:
		if ( !ui.savesRead ) ReadSaves();
		return static_cast<int>( ui.saves.size() );
	case FeederMods:
		return static_cast<int>( ui.mods.size() );
	case FeederLanguages:
		return Academy() ? CL_JKAStringCount() : 0;
	default:
		return 0;
	}
}

std::string FeederItemText( float feeder, int index, int column, qhandle_t &image ) {
	image = -1;
	switch ( static_cast<int>( feeder ) ) {
	case FeederSaveGames:
		if ( index >= 0 && index < static_cast<int>( ui.saves.size() ) && column == 0 ) return ui.saves[index];
		return "";
	case FeederMods:
		if ( index >= 0 && index < static_cast<int>( ui.mods.size() ) )
			return ui.mods[index].description.empty() ? ui.mods[index].name : ui.mods[index].description;
		return "";
	default:
		return "";
	}
}

qhandle_t FeederItemImage( float, int ) { return 0; }

void FeederSelection( float feeder, int index, rm::Item * ) {
	switch ( static_cast<int>( feeder ) ) {
	case FeederSaveGames: ui.saveCursor = index; break;
	case FeederMods: ui.modIndex = index; break;
	default: break;
	}
}

/* ---- the game's own scripts (uiScript) ---------------------------------- */

void RunGameScript( rm::Tokens &args, rm::Item &item );

/* UI_DeferMenuScript: `defer VideoSetup <warning menu>` holds the rest of the
 * script while the video settings have changed, showing the warning. */
bool DeferScript( rm::Tokens &args ) {
	const std::string name = args.Next( false );
	if ( name.empty() ) return false;
	if ( Same( name, "VideoSetup" ) ) {
		const std::string warning = args.Next( false );
		if ( warning.empty() ) return false;
		const bool deferred = Cvar_VariableIntegerValue( "ui_r_modified" ) != 0;
		if ( deferred ) ActivateByName( warning );
		return deferred;
	}
	return false;
}

/* ---- scripts -------------------------------------------------------------- */

bool RectArgs( rm::Tokens &args, rm::Rect &rect ) {
	return args.Float( rect.x ) && args.Float( rect.y ) && args.Float( rect.w ) && args.Float( rect.h );
}

void Transition( rm::Menu *menu, const std::string &name, const rm::Rect *from, const rm::Rect &to, int time,
	float amount ) {
	for ( rm::Item *item : Matching( menu, name ) ) {
		if ( !from ) from = &item->window.rect;   // the first item's own rect, for every match
		item->window.flags |= rm::flag::InTransition | rm::flag::Visible;
		item->window.offsetTime = time;
		item->window.rectClient = *from;
		item->window.rectEffects = to;
		item->window.rectEffects2 = { std::fabs( to.x - from->x ) / amount, std::fabs( to.y - from->y ) / amount,
			std::fabs( to.w - from->w ) / amount, std::fabs( to.h - from->h ) / amount };
		UpdatePosition( *item );
	}
}

void SetItemColor( rm::Item &self, const std::string &name, const std::string &which, const rm::Color &color ) {
	for ( rm::Item *item : Matching( self.parent, name ) ) {
		rm::Color *out = nullptr;
		if ( Same( which, "backcolor" ) ) out = &item->window.backColor;
		else if ( Same( which, "forecolor" ) ) {
			out = &item->window.foreColor;
			item->window.flags |= rm::flag::ForeColorSet;
		} else if ( Same( which, "bordercolor" ) ) out = &item->window.borderColor;
		if ( out ) *out = color;
	}
}

void FeederStep( rm::Item &item, int step );

/* One command of a script; false stops the script (defer, delay). Academy and
 * Outcast each know only their own table's commands (raven_menu.hpp). */
bool Command( rm::Item &item, const std::string &command, rm::Tokens &args ) {
	rm::Menu *menu = item.parent;
	auto next = [&]() { return args.Next( false ); };
	if ( Same( command, "activate" ) ) {
		const std::string menuName = next(), itemName = next();
		if ( !itemName.empty() ) {
			if ( rm::Item *found = FindItem( FindMenu( menuName ), itemName ) )
				found->window.flags &= ~rm::flag::Inactive;
		}
		return true;
	}
	if ( Same( command, "close" ) ) {
		const std::string name = next();
		if ( Same( name, "all" ) ) CloseAll();
		else if ( !name.empty() ) CloseByName( name );
		return true;
	}
	if ( Same( command, "exec" ) ) {
		const std::string text = next();
		if ( !text.empty() ) Execute( text + " ; " );
		return true;
	}
	if ( Same( command, "fadein" ) || Same( command, "fadeout" ) ) {
		const std::string name = next();
		const bool out = Same( command, "fadeout" );
		for ( rm::Item *found : Matching( menu, name ) ) {
			if ( out ) {
				found->window.flags |= rm::flag::FadingOut | rm::flag::Visible;
				found->window.flags &= ~rm::flag::FadingIn;
			} else {
				found->window.flags |= rm::flag::Visible | rm::flag::FadingIn;
				found->window.flags &= ~rm::flag::FadingOut;
			}
		}
		return true;
	}
	if ( Same( command, "hide" ) || Same( command, "show" ) ) {
		const std::string name = next();
		if ( !name.empty() ) ShowItems( menu, name, Same( command, "show" ) );
		return true;
	}
	if ( Same( command, "open" ) ) {
		const std::string name = next();
		if ( !name.empty() ) ActivateByName( name );
		return true;
	}
	if ( Same( command, "openGoToMenu" ) ) return true;   // the engines never fill its menu name
	if ( Same( command, "orbit" ) ) {
		const std::string name = next();
		float x = 0, y = 0, cx = 0, cy = 0;
		int time = 0;
		if ( !name.empty() && args.Float( x ) && args.Float( y ) && args.Float( cx ) && args.Float( cy ) &&
			args.Int( time ) ) {
			for ( rm::Item *found : Matching( menu, name ) ) {
				found->window.flags |= rm::flag::Orbiting | rm::flag::Visible;
				found->window.offsetTime = time;
				found->window.rectEffects.x = cx;
				found->window.rectEffects.y = cy;
				found->window.rectClient.x = x;
				found->window.rectClient.y = y;
				UpdatePosition( *found );
			}
		}
		return true;
	}
	if ( Same( command, "play" ) ) {
		const std::string name = next();
		if ( !name.empty() ) PlaySound( RegisterSound( name ), CHAN_AUTO );
		return true;
	}
	if ( Same( command, "playVoice" ) ) {
		const std::string name = next();
		if ( !name.empty() ) PlaySound( RegisterSound( name ), CHAN_VOICE );
		return true;
	}
	if ( Same( command, "stopVoice" ) ) {
		PlaySound( ui.nullSound, CHAN_VOICE );
		return true;
	}
	if ( Same( command, "playlooped" ) ) {   // Outcast: the music the menu plays
		const std::string name = next();
		if ( !name.empty() ) {
			S_StopBackgroundTrack();
			S_StartBackgroundTrack( name.c_str(), name.c_str() );
		}
		return true;
	}
	if ( Same( command, "ffplay" ) ) {       // a force-feedback effect: fnTech3 drives no such device
		next();
		return true;
	}
	if ( Same( command, "setasset" ) ) {     // read and left as the engines leave it
		next();
		return true;
	}
	if ( Same( command, "setbackground" ) ) {
		const std::string name = next();
		if ( !name.empty() ) item.window.background = name;
		return true;
	}
	if ( Same( command, "setcolor" ) ) {
		const std::string which = next();
		rm::Color *out = nullptr;
		if ( Same( which, "backcolor" ) ) { out = &item.window.backColor; item.window.flags |= rm::flag::BackColorSet; }
		else if ( Same( which, "forecolor" ) ) { out = &item.window.foreColor; item.window.flags |= rm::flag::ForeColorSet; }
		else if ( Same( which, "bordercolor" ) ) out = &item.window.borderColor;
		if ( out ) {
			for ( float &channel : *out ) {
				float value = 0;
				if ( !args.Float( value ) ) return true;
				channel = value;
			}
		}
		return true;
	}
	if ( Same( command, "setcvar" ) ) {
		const std::string name = next(), value = next();
		if ( !name.empty() && !value.empty() ) Cvar_Set( name.c_str(), Same( value, "(NULL)" ) ? "" : value.c_str() );
		return true;
	}
	if ( Same( command, "setfocus" ) ) {
		const std::string name = next();
		rm::Item *found = FindItem( menu, name );
		if ( found && !( found->window.flags & rm::flag::Decoration ) && !( found->window.flags & rm::flag::HasFocus ) ) {
			ClearFocus( menu );
			found->window.flags |= rm::flag::HasFocus;
			if ( !found->onFocus.empty() ) RunScript( *found, found->onFocus );
			PlaySound( ui.itemFocusSound, CHAN_LOCAL_SOUND );
		}
		return true;
	}
	if ( Same( command, "setitemcolor" ) ) {
		const std::string name = next(), which = next();
		if ( !name.empty() && !which.empty() ) {
			rm::Color color{};
			for ( float &channel : color ) {
				if ( !args.Float( channel ) ) return true;
			}
			SetItemColor( item, name, which, color );
		}
		return true;
	}
	if ( Same( command, "setitemflag" ) ) {
		const std::string name = next(), number = next();
		if ( rm::Item *found = FindItem( menu, name ) ) {
			if ( !number.empty() ) found->window.flags |= static_cast<std::uint32_t>( std::atoi( number.c_str() ) );
		}
		return true;
	}
	if ( Same( command, "showMenu" ) ) {
		const std::string name = next();
		if ( rm::Menu *found = FindMenu( name ) ) {
			for ( auto &each : found->items ) each->window.flags |= rm::flag::Visible;
		}
		return true;
	}
	if ( Same( command, "transition" ) ) {
		const std::string name = next();
		rm::Rect from, to;
		int time = 0;
		float amount = 0;
		if ( !name.empty() ) {
			if ( RectArgs( args, from ) && RectArgs( args, to ) && args.Int( time ) && args.Float( amount ) )
				Transition( menu, name, &from, to, time, amount );
			else Com_Printf( S_COLOR_YELLOW "WARNING: Script_Transition: error parsing '%s'\n", name.c_str() );
		}
		return true;
	}
	if ( Same( command, "transition2" ) ) {
		const std::string name = next();
		rm::Rect to;
		int time = 0;
		float amount = 0;
		if ( !name.empty() ) {
			if ( RectArgs( args, to ) && args.Int( time ) && args.Float( amount ) )
				Transition( menu, name, nullptr, to, time, amount );
			else Com_Printf( S_COLOR_YELLOW "WARNING: Script_Transition2: error parsing '%s'\n", name.c_str() );
		}
		return true;
	}
	if ( Same( command, "transition3" ) ) {
		// A model view's bounds and field of view moved over time; read whole.
		const std::string name = next();
		for ( int n = 0; n < 10; ++n ) {
			if ( next().empty() ) break;
		}
		(void)name;
		return true;
	}
	if ( Same( command, "setitembackground" ) ) {
		const std::string name = next(), shader = next();
		if ( !name.empty() && !shader.empty() ) {
			for ( rm::Item *found : Matching( menu, name ) ) found->window.background = shader;
		}
		return true;
	}
	if ( Same( command, "setitemtext" ) ) {
		const std::string name = next(), text = next();
		if ( !name.empty() && !text.empty() ) {
			for ( rm::Item *found : Matching( menu, name ) ) {
				if ( text[0] == '*' ) {
					found->cvar = text.substr( 1 );
					if ( found->edit ) found->edit->minVal = found->edit->maxVal = found->edit->defVal = -1;
				} else {
					found->text = text;
					if ( found->scroll ) found->scroll->startPos = found->scroll->endPos = 0;
				}
			}
		}
		return true;
	}
	if ( Same( command, "setitemrect" ) ) {
		const std::string name = next();
		rm::Rect rect;
		if ( !name.empty() ) {
			const auto items = Matching( menu, name );
			if ( !RectArgs( args, rect ) ) return true;
			for ( rm::Item *found : items ) found->window.rect = rect;
		}
		return true;
	}
	if ( Same( command, "defer" ) ) {
		if ( DeferScript( args ) ) {
			ui.deferredItem = &item;
			ui.deferredScript = args.Rest();
			return false;
		}
		return true;
	}
	if ( Same( command, "rundeferred" ) ) {
		if ( !ui.deferredScript.empty() && ui.deferredItem ) RunScript( *ui.deferredItem, ui.deferredScript );
		return true;
	}
	if ( Same( command, "delay" ) ) {
		int time = 0;
		if ( args.Int( time ) ) {
			item.window.flags |= rm::flag::ScriptWaiting;
			item.window.delayTime = ui.realTime + time;
			item.window.delayedScript = args.Rest();
		} else {
			Com_Printf( S_COLOR_YELLOW "WARNING: Script_Delay: error parsing\n" );
		}
		return false;
	}
	if ( Same( command, "incrementfeeder" ) ) { FeederStep( item, 1 ); return true; }
	if ( Same( command, "decrementfeeder" ) ) { FeederStep( item, -1 ); return true; }
	return true;
}

/* Item_RunScript: commands up to `;`. A command the engine's table does not
 * have goes to the game's own scripts, which read the next word as the
 * script's name: that is how `uiScript <name>` reaches them. */
void RunScript( rm::Item &item, const std::string &script ) {
	if ( script.empty() ) return;
	const rm::Dialect dialect = Academy() ? rm::Dialect::Academy : rm::Dialect::Outcast;
	// Its own copy: a delay or defer rewrites the text a script may be read from.
	const std::string text = script;
	rm::Tokens args( text );
	for ( ;; ) {
		const std::string command = args.Next( false );
		if ( command.empty() ) break;
		if ( command == ";" ) continue;
		if ( rm::IsCommand( dialect, command ) ) {
			if ( !Command( item, command, args ) ) return;
			continue;
		}
		RunGameScript( args, item );
	}
}

/* ---- painting ------------------------------------------------------------- */

void LerpColor( const rm::Color &a, const rm::Color &b, rm::Color &c, float t ) {
	for ( int i = 0; i < 4; ++i ) {
		c[i] = a[i] + t * ( b[i] - a[i] );
		if ( c[i] < 0 ) c[i] = 0;
		else if ( c[i] > 1.0f ) c[i] = 1.0f;
	}
}

float Pulse() { return 0.5f + 0.5f * std::sin( static_cast<float>( ui.realTime / kPulseDivisor ) ); }

void FocusPulse( const rm::Menu &menu, rm::Color &out ) {
	const rm::Color low{ 0.8f * menu.focusColor[0], 0.8f * menu.focusColor[1], 0.8f * menu.focusColor[2],
		0.8f * menu.focusColor[3] };
	LerpColor( menu.focusColor, low, out, Pulse() );
}

void Fade( std::uint32_t &flags, float &value, float clamp, int &nextTime, int offsetTime, bool useFlags,
	float amount ) {
	if ( !( flags & ( rm::flag::FadingOut | rm::flag::FadingIn ) ) ) return;
	if ( ui.realTime <= nextTime ) return;
	nextTime = ui.realTime + offsetTime;
	if ( flags & rm::flag::FadingOut ) {
		value -= amount;
		if ( useFlags && value <= 0.0f ) flags &= ~( rm::flag::FadingOut | rm::flag::Visible );
	} else {
		value += amount;
		if ( value >= clamp ) {
			value = clamp;
			if ( useFlags ) flags &= ~rm::flag::FadingIn;
		}
	}
}

qhandle_t Shader( const std::string &name ) {
	return name.empty() ? 0 : re.RegisterShaderNoMip( name.c_str() );
}

void WindowPaint( rm::Window &w, float fadeAmount, float fadeClamp, int fadeCycle ) {
	if ( ui.debugMode ) {
		const float white[4] = { 1, 1, 1, 1 };
		DrawRect( w.rect.x, w.rect.y, w.rect.w, w.rect.h, 1, white );
	}
	if ( w.style == rm::StyleEmpty && w.border == 0 ) return;
	rm::Rect fill = w.rect;
	if ( w.border ) {
		fill.x += w.borderSize;
		fill.y += w.borderSize;
		fill.w -= w.borderSize + 1;
		fill.h -= w.borderSize + 1;
	}
	if ( w.style == rm::StyleFilled ) {
		if ( !w.background.empty() ) {
			Fade( w.flags, w.backColor[3], fadeClamp, w.nextTime, fadeCycle, true, fadeAmount );
			SetColor( w.backColor.data() );
			DrawPic( fill.x, fill.y, fill.w, fill.h, Shader( w.background ) );
			SetColor( nullptr );
		} else {
			FillRect( fill.x, fill.y, fill.w, fill.h, w.backColor.data() );
		}
	} else if ( w.style == rm::StyleGradient ) {
		SetColor( w.backColor.data() );
		DrawPic( fill.x, fill.y, fill.w, fill.h, ui.gradientBar );
		SetColor( nullptr );
	} else if ( w.style == rm::StyleShader ) {
		if ( w.flags & rm::flag::PlayerColor ) {
			const float color[4] = { Cvar_VariableIntegerValue( "ui_char_color_red" ) / 255.0f,
				Cvar_VariableIntegerValue( "ui_char_color_green" ) / 255.0f,
				Cvar_VariableIntegerValue( "ui_char_color_blue" ) / 255.0f, 1.0f };
			SetColor( color );
		} else if ( w.flags & rm::flag::ForeColorSet ) {
			SetColor( w.foreColor.data() );
		}
		DrawPic( fill.x, fill.y, fill.w, fill.h, Shader( w.background ) );
		SetColor( nullptr );
	}
	switch ( w.border ) {
	case 1: // full
		DrawRect( w.rect.x, w.rect.y, w.rect.w, w.rect.h, w.borderSize, w.borderColor.data() );
		break;
	case 2: // horizontal
		SetColor( w.borderColor.data() );
		DrawTopBottom( w.rect.x, w.rect.y, w.rect.w, w.rect.h, w.borderSize );
		SetColor( nullptr );
		break;
	case 3: // vertical
		SetColor( w.borderColor.data() );
		DrawSides( w.rect.x, w.rect.y, w.rect.w, w.rect.h, w.borderSize );
		SetColor( nullptr );
		break;
	case 4: { // two gradient bars along the edges
		rm::Rect r = w.rect;
		r.h = w.borderSize;
		SetColor( w.borderColor.data() );
		DrawPic( r.x, r.y, r.w, r.h, ui.gradientBar );
		r.y = w.rect.y + w.rect.h - 1;
		DrawPic( r.x, r.y, r.w, r.h, ui.gradientBar );
		SetColor( nullptr );
		break;
	}
	default:
		break;
	}
}

void ToWindowCoords( float &x, float &y, const rm::Window &w ) {
	if ( w.border ) { x += w.borderSize; y += w.borderSize; }
	x += w.rect.x;
	y += w.rect.y;
}

/* Item_SetTextExtents: the text's box, measured once until the item moves or
 * the language changes (a centred owner draw every time). */
void SetTextExtents( rm::Item &item, const std::string &text, int &width, int &height ) {
	width = static_cast<int>( item.textRect.w );
	height = static_cast<int>( item.textRect.h );
	const bool language = !item.text.empty() && item.text[0] == '@' && item.cursorPos != -1 &&
		ui.languageModification != LanguageModification();
	if ( width == 0 || ( item.type == rm::TypeOwnerDraw && item.textAlignment == rm::AlignCenter ) || language ) {
		int original = TextWidth( text, item.textScale, item.font );
		if ( item.type == rm::TypeEditField && item.textAlignment == rm::AlignCenter && !item.cvar.empty() )
			original += TextWidth( CvarString( item.cvar ), item.textScale, item.font );
		width = TextWidth( text, item.textScale, item.font );
		height = TextHeight( item.textScale, item.font );
		item.textRect.w = static_cast<float>( width );
		item.textRect.h = static_cast<float>( height );
		item.textRect.x = item.textAlignX;
		item.textRect.y = item.textAlignY;
		if ( item.textAlignment == rm::AlignRight ) item.textRect.x = item.textAlignX - original;
		else if ( item.textAlignment == rm::AlignCenter ) item.textRect.x = item.textAlignX - original / 2;
		ToWindowCoords( item.textRect.x, item.textRect.y, item.window );
	}
}

void TextColor( rm::Item &item, rm::Color &color ) {
	rm::Menu &menu = *item.parent;
	Fade( item.window.flags, item.window.foreColor[3], menu.fadeClamp, item.window.nextTime, menu.fadeCycle, true,
		menu.fadeAmount );
	if ( !( item.type == rm::TypeText && ( item.window.flags & rm::flag::AutoWrapped ) ) &&
		( item.window.flags & rm::flag::HasFocus ) ) {
		FocusPulse( menu, color );
	} else {
		color = item.window.foreColor;
	}
	if ( item.disabled ) color = menu.disableColor;
	if ( !item.enableCvar.empty() && !item.cvarTest.empty() &&
		( item.cvarFlags & ( rm::cvarRule::Enable | rm::cvarRule::Disable ) ) &&
		!EnableShowViaCvar( item, rm::cvarRule::Enable ) )
		color = menu.disableColor;
	if ( item.window.flags & rm::flag::Inactive ) color = { 0.5f, 0.5f, 0.5f, 1.0f };
}

std::string ItemText( const rm::Item &item ) {
	if ( !item.text.empty() ) return Localize( item.text );
	if ( !item.cvar.empty() ) return CvarString( item.cvar );
	return std::string();
}

void TextPaintWrapped( rm::Item &item ) {
	std::string text = item.text.empty() ? ( item.cvar.empty() ? std::string() : CvarString( item.cvar ) )
		: Localize( item.text );
	if ( text.empty() ) return;
	rm::Color color{};
	TextColor( item, color );
	int width = 0, height = 0;
	SetTextExtents( item, text, width, height );
	float x = item.textRect.x, y = item.textRect.y;
	std::size_t start = 0;
	for ( std::size_t at = text.find( '\r' ); at != std::string::npos; at = text.find( '\r', at + 1 ) ) {
		DrawText( x, y, item.textScale, color.data(), text.substr( start, at - start ), 0, item.textStyle, item.font );
		y += height + 5;
		start = at + 1;
	}
	DrawText( x, y, item.textScale, color.data(), text.substr( start ), 0, item.textStyle, item.font );
}

/* Item_Text_AutoWrapped_Paint: words laid out to the item's width. */
void TextPaintAutoWrapped( rm::Item &item ) {
	std::string text = item.text.empty() ? ( item.cvar.empty() ? std::string() : CvarString( item.cvar ) )
		: Localize( item.text );
	if ( text.empty() ) return;
	rm::Color color{};
	TextColor( item, color );
	const int height = TextHeight( item.textScale, item.font );
	float y = item.textAlignY;
	std::string line;
	std::size_t at = 0;
	auto flush = [&]( const std::string &out ) {
		float x = item.textAlignX, ly = y;
		const int width = TextWidth( out, item.textScale, item.font );
		if ( item.textAlignment == rm::AlignCenter ) x = item.textAlignX - width / 2;
		else if ( item.textAlignment == rm::AlignRight ) x = item.textAlignX - width;
		ToWindowCoords( x, ly, item.window );
		DrawText( x, ly, item.textScale, color.data(), out, 0, item.textStyle, item.font );
		y += height + 5;
	};
	while ( at < text.size() ) {
		std::size_t end = text.find_first_of( " \r\n", at );
		if ( end == std::string::npos ) end = text.size();
		const std::string word = text.substr( at, end - at );
		const std::string candidate = line.empty() ? word : line + " " + word;
		if ( !line.empty() && TextWidth( candidate, item.textScale, item.font ) > item.window.rect.w ) {
			flush( line );
			line = word;
		} else {
			line = candidate;
		}
		if ( end < text.size() && ( text[end] == '\r' || text[end] == '\n' ) ) {
			flush( line );
			line.clear();
		}
		at = end + 1;
	}
	if ( !line.empty() ) flush( line );
}

void TextPaint( rm::Item &item ) {
	if ( item.window.flags & rm::flag::Wrapped ) { TextPaintWrapped( item ); return; }
	if ( item.window.flags & rm::flag::AutoWrapped ) { TextPaintAutoWrapped( item ); return; }
	const std::string text = ItemText( item );
	int width = 0, height = 0;
	SetTextExtents( item, text, width, height );
	if ( text.empty() ) return;
	rm::Color color{};
	TextColor( item, color );
	DrawText( item.textRect.x, item.textRect.y, item.textScale, color.data(), text, 0, item.textStyle, item.font );
	if ( !item.text2.empty() ) {
		TextColor( item, color );
		DrawText( item.textRect.x + item.text2AlignX, item.textRect.y + item.text2AlignY, item.textScale,
			color.data(), Localize( item.text2 ), 0, item.textStyle, item.font );
	}
}

void TextFieldPaint( rm::Item &item ) {
	TextPaint( item );
	const std::string value = item.cvar.empty() ? std::string() : CvarString( item.cvar );
	rm::Color color{};
	if ( item.window.flags & rm::flag::HasFocus ) FocusPulse( *item.parent, color );
	else color = item.window.foreColor;
	const int offset = item.edit ? item.edit->paintOffset : 0;
	const std::string shown = offset < static_cast<int>( value.size() ) ? value.substr( offset ) : std::string();
	const float x = item.textRect.x + item.textRect.w + 8;
	if ( ( item.window.flags & rm::flag::HasFocus ) && ui.editingField && ui.editItem == &item ) {
		const int cursor = item.cursorPos - offset;
		std::string before = shown.substr( 0, std::max( 0, std::min<int>( cursor, static_cast<int>( shown.size() ) ) ) );
		DrawText( x, item.textRect.y, item.textScale, color.data(), shown, static_cast<int>( item.window.rect.w ),
			item.textStyle, item.font );
		if ( ( ui.realTime >> 8 ) & 1 ) {
			const float cx = x + TextWidth( before, item.textScale, item.font );
			DrawText( cx, item.textRect.y, item.textScale, color.data(), Key_GetOverstrikeMode() ? "_" : "|", 0,
				item.textStyle, item.font );
		}
	} else {
		DrawText( x, item.textRect.y, item.textScale, color.data(), shown, static_cast<int>( item.window.rect.w ),
			item.textStyle, item.font );
	}
}

/* ---- list boxes ----------------------------------------------------------- */

int ListMaxScroll( rm::Item &item ) {
	rm::ListBox &list = *item.listBox;
	const int count = FeederCount( item.special );
	int max;
	if ( item.window.flags & rm::flag::Horizontal )
		max = count - static_cast<int>( item.window.rect.w / std::max( 1.0f, list.elementWidth ) ) + 1;
	else
		max = count - static_cast<int>( item.window.rect.h / std::max( 1.0f, list.elementHeight ) ) + 1;
	return max < 0 ? 0 : max;
}

int ListThumbPosition( rm::Item &item ) {
	const float max = static_cast<float>( ListMaxScroll( item ) );
	rm::ListBox &list = *item.listBox;
	if ( item.window.flags & rm::flag::Horizontal ) {
		const float size = item.window.rect.w - kScrollbarSize * 2 - 2;
		const float pos = max > 0 ? ( size - kScrollbarSize ) / max : 0;
		return static_cast<int>( item.window.rect.x + 1 + kScrollbarSize + pos * list.startPos );
	}
	const float size = item.window.rect.h - kScrollbarSize * 2 - 2;
	const float pos = max > 0 ? ( size - kScrollbarSize ) / max : 0;
	return static_cast<int>( item.window.rect.y + 1 + kScrollbarSize + pos * list.startPos );
}

void ListBoxPaint( rm::Item &item ) {
	if ( !item.listBox ) return;
	rm::ListBox &list = *item.listBox;
	const int count = FeederCount( item.special );
	if ( list.startPos > ( count ? count - 1 : count ) ) list.startPos = 0;
	if ( item.cursorPos > ( count ? count - 1 : count ) ) item.cursorPos = 0;
	if ( item.window.flags & rm::flag::Horizontal ) {
		if ( !list.scrollHidden ) {
			if ( ListMaxScroll( item ) > 0 ) {
				float x = item.window.rect.x + 1;
				const float y = item.window.rect.y + item.window.rect.h - kScrollbarSize - 1;
				DrawPic( x, y, kScrollbarSize, kScrollbarSize, ui.scrollLeft );
				x += kScrollbarSize - 1;
				const float size = item.window.rect.w - kScrollbarSize * 2;
				DrawPic( x, y, size + 1, kScrollbarSize, ui.scrollBar );
				x += size - 1;
				DrawPic( x, y, kScrollbarSize, kScrollbarSize, ui.scrollRight );
				float thumb = static_cast<float>( ListThumbPosition( item ) );
				if ( thumb > x - kScrollbarSize - 1 ) thumb = x - kScrollbarSize - 1;
				DrawPic( thumb, y, kScrollbarSize, kScrollbarSize, ui.scrollThumb );
			} else if ( list.startPos > 0 ) {
				list.startPos = 0;
			}
		}
		list.endPos = list.startPos;
		float size = item.window.rect.w - 2;
		if ( list.elementStyle == 1 ) {
			float x = item.window.rect.x + 1;
			const float y = item.window.rect.y + 1;
			for ( int i = list.startPos; i < count; ++i ) {
				if ( const qhandle_t image = FeederItemImage( item.special, i ) )
					DrawPic( x + 1, y + 1, list.elementWidth - 2, list.elementHeight - 2, image );
				if ( i == item.cursorPos )
					DrawRect( x, y, list.elementWidth - 1, list.elementHeight - 1, item.window.borderSize,
						item.window.borderColor.data() );
				size -= list.elementWidth;
				if ( size < list.elementWidth ) { list.drawPadding = static_cast<int>( size ); break; }
				x += list.elementWidth;
				list.endPos++;
			}
		}
		return;
	}
	if ( !list.scrollHidden ) {
		const float x = item.window.rect.x + item.window.rect.w - kScrollbarSize - 1;
		float y = item.window.rect.y + 1;
		DrawPic( x, y, kScrollbarSize, kScrollbarSize, ui.scrollUp );
		y += kScrollbarSize - 1;
		list.endPos = list.startPos;
		const float size = item.window.rect.h - kScrollbarSize * 2;
		DrawPic( x, y, kScrollbarSize, size + 1, ui.scrollBar );
		y += size - 1;
		DrawPic( x, y, kScrollbarSize, kScrollbarSize, ui.scrollDown );
		float thumb = static_cast<float>( ListThumbPosition( item ) );
		if ( thumb > y - kScrollbarSize - 1 ) thumb = y - kScrollbarSize - 1;
		DrawPic( x, thumb, kScrollbarSize, kScrollbarSize, ui.scrollThumb );
	}
	float size = item.window.rect.h - 2;
	if ( list.elementStyle == 1 ) {
		const float x = item.window.rect.x + 1;
		float y = item.window.rect.y + 1;
		for ( int i = list.startPos; i < count; ++i ) {
			if ( const qhandle_t image = FeederItemImage( item.special, i ) )
				DrawPic( x + 1, y + 1, list.elementWidth - 2, list.elementHeight - 2, image );
			if ( i == item.cursorPos )
				DrawRect( x, y, list.elementWidth - 1, list.elementHeight - 1, item.window.borderSize,
					item.window.borderColor.data() );
			list.endPos++;
			size -= list.elementHeight;
			if ( size < list.elementHeight ) { list.drawPadding = static_cast<int>( list.elementHeight - size ); break; }
			y += list.elementHeight;
		}
		return;
	}
	const float x = item.window.rect.x + 1;
	float y = item.window.rect.y + 1 - list.elementHeight;
	for ( int i = list.startPos; i < count; ++i ) {
		qhandle_t image = -1;
		if ( list.numColumns > 0 ) {
			for ( int j = 0; j < list.numColumns; ++j ) {
				const std::string text = Localize( FeederItemText( item.special, i, j, image ) );
				const rm::ColumnInfo &column = list.columns[j];
				if ( image >= 0 ) {
					DrawPic( x + 4 + column.pos, y - 1 + list.elementHeight / 2, column.width, column.width, image );
				} else {
					const float *color = i == item.cursorPos ? item.parent->focusColor.data() : item.window.foreColor.data();
					DrawText( x + 4 + column.pos, y + list.elementHeight, item.textScale, color, text, column.maxChars,
						item.textStyle, item.font );
				}
			}
		} else {
			const std::string text = FeederItemText( item.special, i, 0, image );
			if ( image < 0 )
				DrawText( x + 4, y + list.elementHeight, item.textScale, item.window.foreColor.data(), text, 0,
					item.textStyle, item.font );
		}
		if ( i == item.cursorPos )
			FillRect( x + 2, y + list.elementHeight + 2, item.window.rect.w - kScrollbarSize - 4, list.elementHeight + 2,
				item.window.outlineColor.data() );
		size -= list.elementHeight;
		if ( size < list.elementHeight ) { list.drawPadding = static_cast<int>( list.elementHeight - size ); break; }
		list.endPos++;
		y += list.elementHeight;
	}
}

/* ---- text scroll (Academy) ------------------------------------------------- */

void TextScrollBuildLines( rm::Item &item ) {
	if ( !item.scroll ) return;
	rm::TextScroll &scroll = *item.scroll;
	scroll.lines.clear();
	const std::string text = item.cvar.empty() ? Localize( item.text ) : CvarString( item.cvar );
	const float width = item.window.rect.w - ( kScrollbarSize + 10 );
	std::string line;
	std::size_t at = 0;
	while ( at < text.size() ) {
		std::size_t end = text.find_first_of( " \r\n", at );
		if ( end == std::string::npos ) end = text.size();
		const std::string word = text.substr( at, end - at );
		const std::string candidate = line.empty() ? word : line + " " + word;
		if ( !line.empty() && TextWidth( candidate, item.textScale, item.font ) > width ) {
			scroll.lines.push_back( line );
			line = word;
		} else {
			line = candidate;
		}
		if ( end < text.size() && ( text[end] == '\r' || text[end] == '\n' ) ) {
			scroll.lines.push_back( line );
			line.clear();
		}
		at = end + 1;
	}
	if ( !line.empty() ) scroll.lines.push_back( line );
}

int TextScrollMax( rm::Item &item ) {
	const rm::TextScroll &scroll = *item.scroll;
	const int lineHeight = std::max( 1, scroll.lineHeight );
	const int max = static_cast<int>( scroll.lines.size() ) - static_cast<int>( item.window.rect.h / lineHeight ) + 1;
	return max < 0 ? 0 : max;
}

int TextScrollThumb( rm::Item &item ) {
	const float max = static_cast<float>( TextScrollMax( item ) );
	const float size = item.window.rect.h - kScrollbarSize * 2 - 2;
	const float pos = max > 0 ? ( size - kScrollbarSize ) / max : 0;
	return static_cast<int>( item.window.rect.y + 1 + kScrollbarSize + pos * item.scroll->startPos );
}

void TextScrollPaint( rm::Item &item ) {
	if ( !item.scroll ) return;
	rm::TextScroll &scroll = *item.scroll;
	TextScrollBuildLines( item );
	const int count = static_cast<int>( scroll.lines.size() );
	float size = item.window.rect.h - 2;
	if ( count * scroll.lineHeight > size ) {
		const float x = item.window.rect.x + item.window.rect.w - kScrollbarSize - 1;
		float y = item.window.rect.y + 1;
		DrawPic( x, y, kScrollbarSize, kScrollbarSize, ui.scrollUp );
		y += kScrollbarSize - 1;
		scroll.endPos = scroll.startPos;
		size = item.window.rect.h - kScrollbarSize * 2;
		DrawPic( x, y, kScrollbarSize, size + 1, ui.scrollBar );
		y += size - 1;
		DrawPic( x, y, kScrollbarSize, kScrollbarSize, ui.scrollDown );
		float thumb = static_cast<float>( TextScrollThumb( item ) );
		if ( thumb > y - kScrollbarSize - 1 ) thumb = y - kScrollbarSize - 1;
		DrawPic( x, thumb, kScrollbarSize, kScrollbarSize, ui.scrollThumb );
	}
	size = item.window.rect.h - 2;
	const float x = item.window.rect.x + item.textAlignX + 1;
	float y = item.window.rect.y + item.textAlignY + 1;
	for ( int i = scroll.startPos; i < count; ++i ) {
		DrawText( x + 4, y, item.textScale, item.window.foreColor.data(), scroll.lines[i], 0, item.textStyle, item.font );
		size -= scroll.lineHeight;
		if ( size < scroll.lineHeight ) { scroll.drawPadding = static_cast<int>( scroll.lineHeight - size ); break; }
		scroll.endPos++;
		y += scroll.lineHeight;
	}
}

/* ---- binds ------------------------------------------------------------------ */

/* The commands the bind items can name, in each engine's order (jasp.exe and
 * jk2sp.exe keep this list in their menu code; Raven's released menus agree). */
const std::vector<std::string> &BindCommands() {
	static const std::vector<std::string> academy = { "+altattack", "+attack", "+back", "+force_drain",
		"+force_grip", "+force_lightning", "+forward", "+left", "+lookdown", "+lookup", "+mlook", "+movedown",
		"+moveleft", "+moveright", "+moveup", "+right", "+speed", "+strafe", "+use", "+useforce", "centerview",
		"cg_thirdperson !", "datapad", "exitview", "force_absorb", "force_distract", "force_heal", "force_protect",
		"force_pull", "force_rage", "force_sight", "force_speed", "force_throw", "forcenext", "forceprev",
		"invnext", "invprev", "invuse", "load auto", "load quick", "saberAttackCycle", "save quick", "taunt",
		"uimenu ingameloadmenu", "uimenu ingamesavemenu", "use_bacta", "use_electrobinoculars",
		"use_lightamp_goggles", "use_seeker", "use_sentry", "weapnext", "weapon 0", "weapon 1", "weapon 10",
		"weapon 11", "weapon 12", "weapon 13", "weapon 2", "weapon 3", "weapon 4", "weapon 5", "weapon 6",
		"weapon 7", "weapon 8", "weapon 9", "weapprev", "zoom" };
	static const std::vector<std::string> outcast = { "+altattack", "+attack", "+back", "+force_grip",
		"+force_lightning", "+forward", "+left", "+lookdown", "+lookup", "+mlook", "+movedown", "+moveleft",
		"+moveright", "+moveup", "+right", "+speed", "+strafe", "+use", "+useforce", "centerview",
		"cg_thirdperson !", "datapad", "exitview", "force_distract", "force_heal", "force_pull", "force_speed",
		"force_throw", "forcenext", "forceprev", "invnext", "invprev", "invuse", "load auto", "load quik",
		"saberAttackCycle", "save quik*", "taunt", "uimenu ingameloadmenu", "uimenu ingamesavemenu", "use_bacta",
		"use_electrobinoculars", "use_lightamp_goggles", "use_seeker", "use_sentry", "weapnext", "weapon 0",
		"weapon 1", "weapon 10", "weapon 11", "weapon 12", "weapon 13", "weapon 2", "weapon 3", "weapon 4",
		"weapon 5", "weapon 6", "weapon 7", "weapon 8", "weapon 9", "weapprev", "zoom" };
	return Academy() ? academy : outcast;
}

int BindIndex( const std::string &command ) {
	const auto &commands = BindCommands();
	for ( std::size_t i = 0; i < commands.size(); ++i ) {
		if ( Same( commands[i], command ) ) return static_cast<int>( i );
	}
	return -1;
}

// Controls_GetConfig: each command's first two keys.
void GetBindConfig() {
	const auto &commands = BindCommands();
	ui.bindKeys.assign( commands.size(), { -1, -1 } );
	for ( std::size_t i = 0; i < commands.size(); ++i ) {
		int count = 0;
		for ( int key = 0; key < MAX_KEYS && count < 2; ++key ) {
			const char *binding = Key_GetBinding( key );
			if ( binding && *binding && Same( binding, commands[i] ) ) ui.bindKeys[i][count++] = key;
		}
	}
}

// Controls_SetConfig: the keys bound as the bind items left them.
void SetBindConfig() {
	const auto &commands = BindCommands();
	for ( std::size_t i = 0; i < commands.size() && i < ui.bindKeys.size(); ++i ) {
		for ( int key : ui.bindKeys[i] ) {
			if ( key != -1 ) Key_SetBinding( key, commands[i].c_str() );
		}
	}
}

std::string BindingName( const std::string &command ) {
	const int index = BindIndex( command );
	if ( index < 0 || index >= static_cast<int>( ui.bindKeys.size() ) || ui.bindKeys[index][0] == -1 ) return "???";
	const int first = ui.bindKeys[index][0], second = ui.bindKeys[index][1];
	std::string name = Key_KeynumToString( first );
	if ( second != -1 ) {
		const std::string orText = Localize( Academy() ? "@MENUS_KEYBIND_OR" : "@MENUS3_KEYBIND_OR" );
		name += " " + orText + " " + Key_KeynumToString( second );
	}
	return name;
}

void BindPaint( rm::Item &item ) {
	rm::Color color{};
	rm::Menu &menu = *item.parent;
	const int maxChars = item.edit ? item.edit->maxPaintChars : 0;
	if ( item.window.flags & rm::flag::HasFocus ) {
		if ( ui.bindItem == &item ) {
			const rm::Color low{ 0.8f, 0.0f, 0.0f, 0.8f };
			LerpColor( menu.focusColor, low, color, Pulse() );
		} else {
			FocusPulse( menu, color );
		}
	} else {
		TextColor( item, color );
	}
	if ( item.text.empty() ) {
		const float value = item.cvar.empty() ? 0.0f : CvarValue( item.cvar );
		DrawText( item.textRect.x, item.textRect.y, item.textScale, color.data(), value != 0 ? "FIXME 1" : "FIXME 0",
			maxChars, item.textStyle, item.font );
		return;
	}
	TextPaint( item );
	const std::string name = BindingName( item.cvar );
	float scale = item.textScale;
	int width = TextWidth( name, scale, 0 );
	const float start = item.textRect.x + item.textRect.w + 8;
	while ( start + width >= kScreenWidth && scale > 0.05f ) {
		scale -= 0.05f;
		width = TextWidth( name, scale, 0 );
	}
	int yAdjust = 0;
	if ( scale != item.textScale ) yAdjust = TextHeight( item.textScale, 0 ) - TextHeight( scale, 0 );
	DrawText( start, item.textRect.y + yAdjust, scale, color.data(), name, maxChars, item.textStyle, item.font );
}

/* ---- yes/no, multi, slider -------------------------------------------------- */

std::string MultiSetting( rm::Item &item ) {
	if ( !item.multi ) return "";
	rm::Multi &multi = *item.multi;
	if ( item.cvar.empty() ) {
		return item.value >= 0 && item.value < static_cast<int>( multi.cvarList.size() ) ? multi.cvarList[item.value] : "";
	}
	const std::string text = CvarString( item.cvar );
	const float value = CvarValue( item.cvar );
	for ( std::size_t i = 0; i < multi.cvarList.size(); ++i ) {
		if ( multi.strDef ) {
			if ( i < multi.cvarStr.size() && Same( text, multi.cvarStr[i] ) ) return multi.cvarList[i];
		} else if ( i < multi.cvarValue.size() && multi.cvarValue[i] == value ) {
			return multi.cvarList[i];
		}
	}
	return "";
}

void YesNoPaint( rm::Item &item ) {
	const float value = item.cvar.empty() ? 0.0f : CvarValue( item.cvar );
	const std::string yes = Localize( Academy() ? "@MENUS_YES" : "@MENUS0_YES" );
	const std::string no = Localize( Academy() ? "@MENUS_NO" : "@MENUS0_NO" );
	const std::string shown = item.invertYesNo ? ( value == 0 ? yes : no ) : ( value != 0 ? yes : no );
	rm::Color color{};
	TextColor( item, color );
	if ( !item.text.empty() ) {
		TextPaint( item );
		DrawText( item.textRect.x + item.textRect.w + 8, item.textRect.y, item.textScale, color.data(), shown, 0,
			item.textStyle, item.font );
	} else {
		DrawText( item.textRect.x, item.textRect.y, item.textScale, color.data(), shown, 0, item.textStyle, item.font );
	}
}

void MultiPaint( rm::Item &item ) {
	const std::string text = Localize( MultiSetting( item ) );
	rm::Color color{};
	TextColor( item, color );
	if ( !item.text.empty() ) {
		TextPaint( item );
		DrawText( item.textRect.x + item.textRect.w + 8, item.textRect.y, item.textScale, color.data(), text, 0,
			item.textStyle, item.font );
	} else {
		DrawText( item.textRect.x + item.xoffset, item.textRect.y, item.textScale, color.data(), text, 0,
			item.textStyle, item.font );
	}
}

float SliderThumbPosition( rm::Item &item ) {
	float x = item.text.empty() ? item.window.rect.x : item.textRect.x + item.textRect.w + 8;
	if ( !item.edit || item.cvar.empty() ) return x;
	float value = CvarValue( item.cvar );
	if ( value < item.edit->minVal ) value = item.edit->minVal;
	else if ( value > item.edit->maxVal ) value = item.edit->maxVal;
	const float range = item.edit->maxVal - item.edit->minVal;
	if ( range != 0 ) x += ( value - item.edit->minVal ) / range * kSliderWidth;
	return x;
}

void SliderPaint( rm::Item &item ) {
	rm::Color color{};
	if ( item.window.flags & rm::flag::HasFocus ) FocusPulse( *item.parent, color );
	else color = item.window.foreColor;
	const float y = item.window.rect.y;
	float x;
	if ( !item.text.empty() ) {
		TextPaint( item );
		x = item.textRect.x + item.textRect.w + 8;
	} else {
		x = item.window.rect.x;
	}
	SetColor( color.data() );
	DrawPic( x, y + 2, kSliderWidth, kSliderHeight, ui.sliderBar );
	x = SliderThumbPosition( item );
	DrawPic( x - kSliderThumbWidth / 2, y + 2, kSliderThumbWidth, kSliderThumbHeight, ui.sliderThumb );
	SetColor( nullptr );
}

/* ---- models ------------------------------------------------------------------ */

/* A menu's 3D view of a plain model (asset_model naming an .md3): its
 * bounds framed in the item's rect, turned by model_rotation. Ghoul2 player
 * models (.glm, Academy's character and saber menus) are not drawn yet. */
void ModelPaint( rm::Item &item ) {
	if ( !item.model || item.assetModel.empty() ) return;
	if ( item.assetModel.size() > 4 && !Q_stricmp( item.assetModel.c_str() + item.assetModel.size() - 4, ".glm" ) )
		return;
	const qhandle_t model = re.RegisterModel( item.assetModel.c_str() );
	if ( !model ) return;
	rm::Model &def = *item.model;
	float x = item.window.rect.x + 1, y = item.window.rect.y + 1, w = item.window.rect.w - 2, h = item.window.rect.h - 2;
	CL_UIVirtualToScreen( &x, &y, &w, &h );
	refdef_t view{};
	view.rdflags = RDF_NOWORLDMODEL;
	AxisClear( view.viewaxis );
	view.x = static_cast<int>( x );
	view.y = static_cast<int>( y );
	view.width = static_cast<int>( w );
	view.height = static_cast<int>( h );
	vec3_t mins, maxs, origin;
	re.ModelBounds( model, mins, maxs );
	origin[2] = -0.5f * ( mins[2] + maxs[2] );
	origin[1] = 0.5f * ( mins[1] + maxs[1] );
	const float len = 0.5f * ( maxs[2] - mins[2] );
	origin[0] = len / 0.268f;   // len / tan( fov / 2 ) at the engines' 30 degree default
	view.fov_x = def.fovX ? def.fovX : 30.0f;
	view.fov_y = def.fovY ? def.fovY : 30.0f;
	view.time = ui.realTime;
	re.ClearScene();
	refEntity_t entity{};
	vec3_t angles{ 0, 0, 0 };
	angles[YAW] = def.rotationSpeed ? static_cast<float>( ( ui.realTime / def.rotationSpeed ) % 360 )
		: static_cast<float>( def.angle );
	AnglesToAxis( angles, entity.axis );
	entity.hModel = model;
	VectorCopy( origin, entity.origin );
	VectorCopy( origin, entity.lightingOrigin );
	VectorCopy( origin, entity.oldorigin );
	entity.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
	re.AddRefEntityToScene( &entity, qfalse );
	re.RenderScene( &view );
}

/* ---- owner draws -------------------------------------------------------------- */

void OwnerDrawItem( rm::Item &item, float x, float y, float w, float h, float textX, float textY, const float *color ) {
	switch ( item.window.ownerDraw ) {
	case DrawGlInfo: {
		// The renderer's name and version, as the driver menu shows them.
		const float *c = color;
		const int height = TextHeight( item.textScale, item.font );
		const glconfig_t &gl = cls.glconfig;
		DrawText( x + textX, y + textY, item.textScale, c, va( "VENDOR: %s", gl.vendor_string ), 0, item.textStyle,
			item.font );
		DrawText( x + textX, y + textY + height + 2, item.textScale, c, va( "VERSION: %s: %s", gl.version_string,
			gl.renderer_string ), 0, item.textStyle, item.font );
		DrawText( x + textX, y + textY + 2 * ( height + 2 ), item.textScale, c, va( "PIXELFORMAT: color(%d-bits) "
			"Z(%d-bit) stencil(%d-bits)", gl.colorBits, gl.depthBits, gl.stencilBits ), 0, item.textStyle,
			item.font );
		break;
	}
	case DrawKeyBindStatus:
		if ( ui.waitingForKey ) {
			DrawText( x + textX, y + textY, item.textScale, color,
				Localize( Academy() ? "@MENUS_WAITINGFORKEY" : "@MENUS3_WAITINGFORKEY" ), 0, item.textStyle,
				item.font );
		}
		break;
	case DrawSaveShot:
		// A save's picture: the level's shot (saves are not read yet).
		break;
	default:
		break;
	}
	(void)w; (void)h;
}

void OwnerDrawPaint( rm::Item &item ) {
	rm::Menu &menu = *item.parent;
	Fade( item.window.flags, item.window.foreColor[3], menu.fadeClamp, item.window.nextTime, menu.fadeCycle, true,
		menu.fadeAmount );
	rm::Color color = item.window.foreColor;
	if ( item.window.flags & rm::flag::HasFocus ) {
		FocusPulse( menu, color );
	} else if ( item.textStyle == StyleBlink && !( ( ui.realTime / kBlinkDivisor ) & 1 ) ) {
		const rm::Color low{ 0.8f * item.window.foreColor[0], 0.8f * item.window.foreColor[1],
			0.8f * item.window.foreColor[2], 0.8f * item.window.foreColor[3] };
		LerpColor( item.window.foreColor, low, color, Pulse() );
	}
	if ( item.disabled ) color = menu.disableColor;
	if ( ( item.cvarFlags & ( rm::cvarRule::Enable | rm::cvarRule::Disable ) ) &&
		!EnableShowViaCvar( item, rm::cvarRule::Enable ) )
		color = menu.disableColor;
	if ( !item.text.empty() ) {
		TextPaint( item );
		OwnerDrawItem( item, item.textRect.x + item.textRect.w + 8, item.window.rect.y, item.window.rect.w,
			item.window.rect.h, 0, item.textAlignY, color.data() );
	} else {
		OwnerDrawItem( item, item.window.rect.x, item.window.rect.y, item.window.rect.w, item.window.rect.h,
			item.textAlignX, item.textAlignY, color.data() );
	}
}

/* ---- item painting ------------------------------------------------------------ */

void StepToward( float &value, float target, float step, int &done ) {
	if ( value == target ) { ++done; return; }
	if ( value < target ) {
		value += step;
		if ( value > target ) { value = target; ++done; }
	} else {
		value -= step;
		if ( value < target ) { value = target; ++done; }
	}
}

/* Item_Paint: effects first (a waiting script, an orbit, a transition), then
 * the visibility rules; bDraw false only asks whether it would draw. */
bool ItemPaint( rm::Item &item, bool draw ) {
	rm::Menu &menu = *item.parent;
	if ( item.window.flags & rm::flag::ScriptWaiting ) {
		if ( ui.realTime > item.window.delayTime ) {
			item.window.flags &= ~rm::flag::ScriptWaiting;
			RunScript( item, item.window.delayedScript );
		}
	}
	if ( item.window.flags & rm::flag::Orbiting ) {
		if ( ui.realTime > item.window.nextTime ) {
			item.window.nextTime = ui.realTime + item.window.offsetTime;
			const float w = item.window.rectClient.w / 2, h = item.window.rectClient.h / 2;
			const float rx = item.window.rectClient.x + w - item.window.rectEffects.x;
			const float ry = item.window.rectClient.y + h - item.window.rectEffects.y;
			const float a = static_cast<float>( 3 * M_PI / 180 ), c = std::cos( a ), s = std::sin( a );
			item.window.rectClient.x = ( rx * c - ry * s ) + item.window.rectEffects.x - w;
			item.window.rectClient.y = ( rx * s + ry * c ) + item.window.rectEffects.y - h;
			UpdatePosition( item );
		}
	}
	if ( item.window.flags & rm::flag::InTransition ) {
		if ( ui.realTime > item.window.nextTime ) {
			int done = 0;
			item.window.nextTime = ui.realTime + item.window.offsetTime;
			StepToward( item.window.rectClient.x, item.window.rectEffects.x, item.window.rectEffects2.x, done );
			StepToward( item.window.rectClient.y, item.window.rectEffects.y, item.window.rectEffects2.y, done );
			StepToward( item.window.rectClient.w, item.window.rectEffects.w, item.window.rectEffects2.w, done );
			StepToward( item.window.rectClient.h, item.window.rectEffects.h, item.window.rectEffects2.h, done );
			UpdatePosition( item );
			if ( done == 4 ) item.window.flags &= ~rm::flag::InTransition;
		}
	}
	if ( !Shown( item ) ) return false;
	if ( !( item.window.flags & rm::flag::Visible ) ) return false;
	if ( !draw ) return true;

	if ( ( item.window.flags & rm::flag::MouseOver ) && !item.descText.empty() && !ui.waitingForKey && Enabled( item ) ) {
		const std::string text = Localize( item.descText );
		rm::Color color{ 1, 1, 1, 1 };
		TextColor( item, color );
		const int descFont = Academy() ? 4 : 0;
		const float base = menu.descScale ? menu.descScale : 1.0f;
		float scale = base;
		for ( ;; ) {
			const int width = TextWidth( text, scale, descFont );
			int x = menu.descX;
			if ( menu.descAlignment == rm::AlignRight ) x = menu.descX - width;
			else if ( menu.descAlignment == rm::AlignCenter ) x = menu.descX - width / 2;
			if ( menu.descAlignment == rm::AlignCenter && x + width > kScreenWidth - 4 && scale > 0.001f ) {
				scale -= 0.001f;
				continue;
			}
			int yAdjust = 0;
			if ( scale != base ) yAdjust = TextHeight( base, 0 ) - TextHeight( scale, 0 );
			DrawText( static_cast<float>( x ), static_cast<float>( menu.descY + yAdjust ), scale, menu.descColor.data(),
				text, 0, menu.descTextStyle, descFont );
			break;
		}
	}

	WindowPaint( item.window, menu.fadeAmount, menu.fadeClamp, menu.fadeCycle );
	if ( ui.debugMode ) {
		const float green[4] = { 0, 1, 0, 1 };
		DrawRect( item.window.rect.x, item.window.rect.y, item.window.rect.w, item.window.rect.h, 1, green );
	}
	switch ( item.type ) {
	case rm::TypeOwnerDraw: OwnerDrawPaint( item ); break;
	case rm::TypeText: case rm::TypeButton: TextPaint( item ); break;
	case rm::TypeEditField: case rm::TypeNumericField: TextFieldPaint( item ); break;
	case rm::TypeListBox: ListBoxPaint( item ); break;
	case rm::TypeTextScroll: TextScrollPaint( item ); break;
	case rm::TypeModel: ModelPaint( item ); break;
	case rm::TypeYesNo: YesNoPaint( item ); break;
	case rm::TypeMulti: MultiPaint( item ); break;
	case rm::TypeBind: BindPaint( item ); break;
	case rm::TypeSlider: SliderPaint( item ); break;
	default: break;
	}
	return true;
}

void MenuPaint( rm::Menu &menu ) {
	if ( menu.window.flags & rm::flag::ScriptWaiting ) {
		if ( ui.realTime > menu.window.delayTime ) {
			menu.window.flags &= ~rm::flag::ScriptWaiting;
			RunMenuScript( menu, menu.window.delayedScript );
		}
	}
	if ( !( menu.window.flags & rm::flag::Visible ) ) return;
	if ( menu.fullScreen ) {
		SetColor( menu.window.backColor.data() );
		const qhandle_t background = menu.window.background.empty() ? ui.white : Shader( menu.window.background );
		DrawPic( 0, 0, kScreenWidth, kScreenHeight, background );
		SetColor( nullptr );
	}
	WindowPaint( menu.window, menu.fadeAmount, menu.fadeClamp, menu.fadeCycle );
	int slotsVisible = 0;
	for ( auto &item : menu.items ) {
		if ( !item->appearanceSlot ) {
			ItemPaint( *item, true );
		} else {
			if ( ItemPaint( *item, false ) ) ++slotsVisible;
			if ( item->appearanceSlot <= menu.appearanceCount ) ItemPaint( *item, true );
		}
	}
	if ( slotsVisible && menu.appearanceTime < ui.realTime &&
		menu.appearanceCount < static_cast<int>( menu.items.size() ) ) {
		menu.appearanceTime = ui.realTime + menu.appearanceIncrement;
		menu.appearanceCount++;
	}
	if ( ui.debugMode ) {
		const float magenta[4] = { 1, 0, 1, 1 };
		DrawRect( menu.window.rect.x, menu.window.rect.y, menu.window.rect.w, menu.window.rect.h, 1, magenta );
	}
}

/* ---- mouse ---------------------------------------------------------------------- */

bool Contains( const rm::Rect &rect, float x, float y ) {
	return x > rect.x && x < rect.x + rect.w && y > rect.y && y < rect.y + rect.h;
}

bool IsVisible( std::uint32_t flags ) { return ( flags & rm::flag::Visible ) && !( flags & rm::flag::FadingOut ); }

int ListOver( rm::Item &item, float x, float y ) {
	rm::Rect r;
	if ( item.window.flags & rm::flag::Horizontal ) {
		r = { item.window.rect.x, item.window.rect.y + item.window.rect.h - kScrollbarSize, kScrollbarSize, kScrollbarSize };
		if ( Contains( r, x, y ) ) return rm::flag::ListLeftArrow;
		r.x = item.window.rect.x + item.window.rect.w - kScrollbarSize;
		if ( Contains( r, x, y ) ) return rm::flag::ListRightArrow;
		const float thumb = static_cast<float>( ListThumbPosition( item ) );
		r.x = thumb;
		if ( Contains( r, x, y ) ) return rm::flag::ListThumb;
		r.x = item.window.rect.x + kScrollbarSize;
		r.w = thumb - r.x;
		if ( Contains( r, x, y ) ) return rm::flag::ListPageUp;
		r.x = thumb + kScrollbarSize;
		r.w = item.window.rect.x + item.window.rect.w - kScrollbarSize;
		if ( Contains( r, x, y ) ) return rm::flag::ListPageDown;
		return 0;
	}
	r = { item.window.rect.x + item.window.rect.w - kScrollbarSize, item.window.rect.y, kScrollbarSize, kScrollbarSize };
	if ( Contains( r, x, y ) ) return rm::flag::ListLeftArrow;
	r.y = item.window.rect.y + item.window.rect.h - kScrollbarSize;
	if ( Contains( r, x, y ) ) return rm::flag::ListRightArrow;
	const float thumb = static_cast<float>( ListThumbPosition( item ) );
	r.y = thumb;
	if ( Contains( r, x, y ) ) return rm::flag::ListThumb;
	r.y = item.window.rect.y + kScrollbarSize;
	r.h = thumb - r.y;
	if ( Contains( r, x, y ) ) return rm::flag::ListPageUp;
	r.y = thumb + kScrollbarSize;
	r.h = item.window.rect.y + item.window.rect.h - kScrollbarSize;
	if ( Contains( r, x, y ) ) return rm::flag::ListPageDown;
	return 0;
}

constexpr std::uint32_t kListParts = rm::flag::ListLeftArrow | rm::flag::ListRightArrow | rm::flag::ListThumb |
	rm::flag::ListPageUp | rm::flag::ListPageDown;

void ListMouseEnter( rm::Item &item, float x, float y ) {
	if ( !item.listBox ) return;
	rm::ListBox &list = *item.listBox;
	item.window.flags &= ~kListParts;
	item.window.flags |= ListOver( item, x, y );
	if ( item.window.flags & kListParts ) return;
	if ( item.window.flags & rm::flag::Horizontal ) {
		if ( list.elementStyle == 1 ) {
			const rm::Rect r{ item.window.rect.x, item.window.rect.y, item.window.rect.w - list.drawPadding,
				item.window.rect.h - kScrollbarSize };
			if ( Contains( r, x, y ) ) {
				list.cursorPos = static_cast<int>( ( x - r.x ) / std::max( 1.0f, list.elementWidth ) ) + list.startPos;
				if ( list.cursorPos > list.endPos ) list.cursorPos = list.endPos;
			}
		}
		return;
	}
	const rm::Rect r{ item.window.rect.x, item.window.rect.y, item.window.rect.w - kScrollbarSize,
		item.window.rect.h - list.drawPadding };
	if ( Contains( r, x, y ) ) {
		list.cursorPos = static_cast<int>( ( y - 2 - r.y ) / std::max( 1.0f, list.elementHeight ) ) + list.startPos;
		if ( list.cursorPos > list.endPos ) list.cursorPos = list.endPos;
	}
}

void MouseEnter( rm::Item &item, float x, float y ) {
	if ( item.disabled || !Enabled( item ) || !Shown( item ) ) return;
	if ( Contains( item.textRect, x, y ) ) {
		if ( !( item.window.flags & rm::flag::MouseOverText ) ) {
			RunScript( item, item.mouseEnterText );
			item.window.flags |= rm::flag::MouseOverText;
		}
		if ( !( item.window.flags & rm::flag::MouseOver ) ) {
			RunScript( item, item.mouseEnter );
			item.window.flags |= rm::flag::MouseOver;
		}
		return;
	}
	if ( item.window.flags & rm::flag::MouseOverText ) {
		RunScript( item, item.mouseExitText );
		item.window.flags &= ~rm::flag::MouseOverText;
	}
	if ( !( item.window.flags & rm::flag::MouseOver ) ) {
		RunScript( item, item.mouseEnter );
		item.window.flags |= rm::flag::MouseOver;
	}
	if ( item.type == rm::TypeListBox ) ListMouseEnter( item, x, y );
	else if ( item.type == rm::TypeTextScroll && item.scroll ) {
		item.window.flags &= ~kListParts;
	}
}

void MouseLeave( rm::Item &item ) {
	if ( item.window.flags & rm::flag::MouseOverText ) {
		RunScript( item, item.mouseExitText );
		item.window.flags &= ~rm::flag::MouseOverText;
	}
	RunScript( item, item.mouseExit );
	item.window.flags &= ~( rm::flag::ListRightArrow | rm::flag::ListLeftArrow );
}

/* Item_SetFocus: the item takes the focus and plays its focus sound (or the
 * menus' own); a text item only while the point is over its text. */
bool SetFocus( rm::Item &item, float x, float y ) {
	const std::uint32_t flags = item.window.flags;
	if ( ( flags & rm::flag::Decoration ) || ( flags & rm::flag::HasFocus ) || !( flags & rm::flag::Visible ) ||
		( flags & rm::flag::Inactive ) )
		return false;
	if ( item.disabled || !Enabled( item ) || !Shown( item ) ) return false;
	rm::Item *previous = ClearFocus( item.parent );
	sfxHandle_t sound = ui.itemFocusSound;
	bool play = false;
	if ( item.type == rm::TypeText ) {
		if ( Contains( item.textRect, x, y ) ) {
			item.window.flags |= rm::flag::HasFocus;
			if ( !item.focusSound.empty() ) sound = RegisterSound( item.focusSound );
			play = true;
		} else if ( previous ) {
			previous->window.flags |= rm::flag::HasFocus;
			if ( !previous->onFocus.empty() ) RunScript( *previous, previous->onFocus );
		}
	} else {
		item.window.flags |= rm::flag::HasFocus;
		if ( !item.onFocus.empty() ) RunScript( item, item.onFocus );
		if ( !item.focusSound.empty() ) sound = RegisterSound( item.focusSound );
		play = true;
	}
	if ( play ) PlaySound( sound, CHAN_LOCAL_SOUND );
	auto &items = item.parent->items;
	for ( std::size_t i = 0; i < items.size(); ++i ) {
		if ( items[i].get() == &item ) { item.parent->cursorItem = static_cast<int>( i ); break; }
	}
	return true;
}

void HandleMouseMove( rm::Menu *menu, float x, float y ) {
	if ( !menu ) return;
	if ( !( menu->window.flags & ( rm::flag::Visible | rm::flag::Forced ) ) ) return;
	if ( ui.itemCapture || ui.waitingForKey || ui.editingField ) return;
	bool focusSet = false;
	for ( int pass = 0; pass < 2; ++pass ) {
		for ( std::size_t i = 0; i < menu->items.size(); ++i ) {
			rm::Item &item = *menu->items[i];
			if ( !( item.window.flags & ( rm::flag::Visible | rm::flag::Forced ) ) ) continue;
			if ( item.window.flags & rm::flag::Inactive ) continue;
			if ( item.disabled || !Enabled( item ) || !Shown( item ) ) continue;
			if ( Contains( item.window.rect, x, y ) ) {
				if ( pass == 1 && IsVisible( item.window.flags ) ) {
					MouseEnter( item, x, y );
					if ( !focusSet ) focusSet = SetFocus( item, x, y );
				}
			} else if ( item.window.flags & rm::flag::MouseOver ) {
				MouseLeave( item );
				item.window.flags &= ~rm::flag::MouseOver;
			}
		}
	}
}

void DisplayMouseMove( float x, float y ) {
	rm::Menu *menu = Focused();
	if ( menu && ( menu->window.flags & rm::flag::Popup ) ) {
		HandleMouseMove( menu, x, y );
		return;
	}
	for ( auto &each : ui.menus ) HandleMouseMove( each.get(), x, y );
}

/* ---- keys ------------------------------------------------------------------------ */

void BindUngrey( rm::Item &item ) {
	for ( auto &each : item.parent->items ) {
		if ( each.get() != &item ) each->window.flags &= ~rm::flag::Inactive;
	}
}

void BindGrey( rm::Item &item ) {
	for ( auto &each : item.parent->items ) {
		if ( each.get() != &item ) each->window.flags |= rm::flag::Inactive;
	}
}

bool BindKey( rm::Item &item, int key, bool down ) {
	if ( key == K_MOUSE1 && Contains( item.window.rect, ui.cursorX, ui.cursorY ) && !ui.waitingForKey ) {
		if ( down ) { ui.waitingForKey = true; ui.bindItem = &item; BindGrey( item ); }
		return true;
	}
	if ( key == K_ENTER && !ui.waitingForKey ) {
		if ( down ) { ui.waitingForKey = true; ui.bindItem = &item; BindGrey( item ); }
		return true;
	}
	if ( !ui.waitingForKey || !ui.bindItem ) return false;
	if ( key & K_CHAR_FLAG ) return true;
	const int id = BindIndex( item.cvar );
	switch ( key ) {
	case K_ESCAPE:
		ui.waitingForKey = false;
		BindUngrey( item );
		return true;
	case K_BACKSPACE:
		if ( id != -1 && id < static_cast<int>( ui.bindKeys.size() ) ) {
			for ( int &bound : ui.bindKeys[id] ) {
				if ( bound != -1 ) Key_SetBinding( bound, "" );
				bound = -1;
			}
		}
		SetBindConfig();
		ui.waitingForKey = false;
		ui.bindItem = nullptr;
		BindUngrey( item );
		return true;
	case '`':
		return true;
	default:
		break;
	}
	if ( key != -1 ) {
		for ( auto &pair : ui.bindKeys ) {
			if ( pair[1] == key ) pair[1] = -1;
			if ( pair[0] == key ) { pair[0] = pair[1]; pair[1] = -1; }
		}
	}
	if ( id != -1 && id < static_cast<int>( ui.bindKeys.size() ) ) {
		auto &pair = ui.bindKeys[id];
		if ( key == -1 ) {
			for ( int &bound : pair ) {
				if ( bound != -1 ) { Key_SetBinding( bound, "" ); bound = -1; }
			}
		} else if ( pair[0] == -1 ) {
			pair[0] = key;
		} else if ( pair[0] != key && pair[1] == -1 ) {
			pair[1] = key;
		} else {
			Key_SetBinding( pair[0], "" );
			if ( pair[1] != -1 ) Key_SetBinding( pair[1], "" );
			pair[0] = key;
			pair[1] = -1;
		}
	}
	SetBindConfig();
	ui.waitingForKey = false;
	BindUngrey( item );
	return true;
}

bool YesNoKey( rm::Item &item, int key ) {
	if ( Contains( item.window.rect, ui.cursorX, ui.cursorY ) && ( item.window.flags & rm::flag::HasFocus ) &&
		!item.cvar.empty() ) {
		if ( key == K_MOUSE1 || key == K_ENTER || key == K_MOUSE2 || key == K_MOUSE3 ) {
			Cvar_Set( item.cvar.c_str(), va( "%i", CvarValue( item.cvar ) == 0 ) );
			return true;
		}
	}
	return false;
}

int MultiFindCvarByValue( rm::Item &item ) {
	if ( !item.multi ) return 0;
	rm::Multi &multi = *item.multi;
	const std::string text = CvarString( item.cvar );
	const float value = CvarValue( item.cvar );
	for ( std::size_t i = 0; i < multi.cvarList.size(); ++i ) {
		if ( multi.strDef ) {
			if ( i < multi.cvarStr.size() && Same( text, multi.cvarStr[i] ) ) return static_cast<int>( i );
		} else if ( i < multi.cvarValue.size() && multi.cvarValue[i] == value ) {
			return static_cast<int>( i );
		}
	}
	return 0;
}

bool MultiKey( rm::Item &item, int key ) {
	if ( !item.multi ) return false;
	if ( !Contains( item.window.rect, ui.cursorX, ui.cursorY ) || !( item.window.flags & rm::flag::HasFocus ) )
		return false;
	if ( key != K_MOUSE1 && key != K_ENTER && key != K_MOUSE2 && key != K_MOUSE3 && key != K_MWHEELDOWN &&
		key != K_MWHEELUP )
		return false;
	rm::Multi &multi = *item.multi;
	const int max = static_cast<int>( multi.cvarList.size() );
	const bool back = key == K_MOUSE2 || key == K_MWHEELDOWN;
	if ( !item.cvar.empty() ) {
		if ( max <= 0 ) return true;
		int current = MultiFindCvarByValue( item ) + ( back ? -1 : 1 );
		if ( current < 0 ) current = max - 1;
		if ( current >= max ) current = 0;
		if ( multi.strDef ) {
			if ( current < static_cast<int>( multi.cvarStr.size() ) ) Cvar_Set( item.cvar.c_str(), multi.cvarStr[current].c_str() );
		} else if ( current < static_cast<int>( multi.cvarValue.size() ) ) {
			const float value = multi.cvarValue[current];
			if ( static_cast<float>( static_cast<int>( value ) ) == value )
				Cvar_Set( item.cvar.c_str(), va( "%i", static_cast<int>( value ) ) );
			else
				Cvar_Set( item.cvar.c_str(), va( "%f", value ) );
		}
		if ( item.special ) FeederSelection( item.special, current, &item );
	} else {
		item.value += back ? -1 : 1;
		if ( item.value < 0 ) item.value = max;
		if ( item.value >= max ) item.value = 0;
		if ( item.special ) FeederSelection( item.special, item.value, &item );
	}
	return true;
}

bool SliderKey( rm::Item &item, int key ) {
	if ( !( item.window.flags & rm::flag::HasFocus ) || item.cvar.empty() ||
		!Contains( item.window.rect, ui.cursorX, ui.cursorY ) )
		return false;
	if ( key != K_MOUSE1 && key != K_ENTER && key != K_MOUSE2 && key != K_MOUSE3 ) return false;
	if ( !item.edit ) return false;
	const float x = item.text.empty() ? item.window.rect.x : item.textRect.x + item.textRect.w + 8;
	rm::Rect test = item.window.rect;
	test.x = x - kSliderThumbWidth / 2;
	test.w = kSliderWidth + kSliderThumbWidth / 2;
	if ( !Contains( test, ui.cursorX, ui.cursorY ) ) return false;
	const float value = ( ui.cursorX - x ) / kSliderWidth * ( item.edit->maxVal - item.edit->minVal ) + item.edit->minVal;
	Cvar_Set( item.cvar.c_str(), va( "%f", value ) );
	return true;
}

bool TextKey( rm::Item &item, int key ) {
	if ( !Contains( item.window.rect, ui.cursorX, ui.cursorY ) || !( item.window.flags & rm::flag::AutoWrapped ) )
		return false;
	if ( key != K_MOUSE1 && key != K_ENTER && key != K_MOUSE2 && key != K_MOUSE3 ) return false;
	if ( key == K_MOUSE2 ) {
		if ( --item.cursorPos < 0 ) item.cursorPos = 0;
	} else if ( item.special ) {
		if ( ++item.cursorPos >= item.value ) item.cursorPos = 0;
	}
	return true;
}

bool ListKey( rm::Item &item, int key, bool force ) {
	if ( !item.listBox ) return false;
	rm::ListBox &list = *item.listBox;
	const int count = FeederCount( item.special );
	if ( !force && !( Contains( item.window.rect, ui.cursorX, ui.cursorY ) && ( item.window.flags & rm::flag::HasFocus ) ) )
		return false;
	const int max = ListMaxScroll( item );
	const bool horizontal = ( item.window.flags & rm::flag::Horizontal ) != 0;
	const int view = horizontal ? static_cast<int>( item.window.rect.w / std::max( 1.0f, list.elementWidth ) )
		: static_cast<int>( item.window.rect.h / std::max( 1.0f, list.elementHeight ) );
	const int backKey = horizontal ? K_LEFTARROW : K_UPARROW, nextKey = horizontal ? K_RIGHTARROW : K_DOWNARROW;
	auto select = [&]( int index ) {
		item.cursorPos = index;
		list.cursorPos = index;
		FeederSelection( item.special, index, &item );
	};
	if ( key == backKey || key == ( horizontal ? K_KP_LEFTARROW : K_KP_UPARROW ) ) {
		if ( !list.notSelectable ) {
			if ( item.cursorPos > 0 ) {
				select( item.cursorPos - 1 );
				if ( item.cursorPos < list.startPos ) list.startPos = item.cursorPos;
			}
		} else if ( list.startPos > 0 ) {
			list.startPos--;
		}
		return true;
	}
	if ( key == nextKey || key == ( horizontal ? K_KP_RIGHTARROW : K_KP_DOWNARROW ) ) {
		if ( !list.notSelectable ) {
			if ( item.cursorPos < count - 1 ) {
				select( item.cursorPos + 1 );
				if ( item.cursorPos >= list.startPos + view ) list.startPos = item.cursorPos - view + 1;
			}
		} else if ( list.startPos < max ) {
			list.startPos++;
		}
		return true;
	}
	if ( key == K_MOUSE1 || key == K_MOUSE2 ) {
		if ( item.window.flags & rm::flag::ListLeftArrow ) {
			if ( --list.startPos < 0 ) list.startPos = 0;
		} else if ( item.window.flags & rm::flag::ListRightArrow ) {
			if ( ++list.startPos > max ) list.startPos = max;
		} else if ( item.window.flags & rm::flag::ListPageUp ) {
			list.startPos -= view;
			if ( list.startPos < 0 ) list.startPos = 0;
		} else if ( item.window.flags & rm::flag::ListPageDown ) {
			list.startPos += view;
			if ( list.startPos > max ) list.startPos = max;
		} else if ( !( item.window.flags & rm::flag::ListThumb ) ) {
			if ( ui.realTime < ui.lastListClick && !list.doubleClick.empty() ) RunScript( item, list.doubleClick );
			ui.lastListClick = ui.realTime + kDoubleClickDelay;
			if ( item.cursorPos != list.cursorPos ) select( list.cursorPos );
		}
		return true;
	}
	if ( key == K_MWHEELUP ) {
		if ( --list.startPos < 0 ) list.startPos = 0;
		return true;
	}
	if ( key == K_MWHEELDOWN ) {
		if ( ++list.startPos > max ) list.startPos = max;
		return true;
	}
	if ( key == K_HOME || key == K_KP_HOME ) { list.startPos = 0; return true; }
	if ( key == K_END || key == K_KP_END ) { list.startPos = max; return true; }
	if ( key == K_PGUP || key == K_KP_PGUP ) {
		list.startPos -= view;
		if ( list.startPos < 0 ) list.startPos = 0;
		return true;
	}
	if ( key == K_PGDN || key == K_KP_PGDN ) {
		list.startPos += view;
		if ( list.startPos > max ) list.startPos = max;
		return true;
	}
	return false;
}

bool TextScrollKey( rm::Item &item, int key ) {
	if ( !item.scroll ) return false;
	rm::TextScroll &scroll = *item.scroll;
	const int max = TextScrollMax( item );
	const int view = static_cast<int>( item.window.rect.h / std::max( 1, scroll.lineHeight ) );
	if ( !Contains( item.window.rect, ui.cursorX, ui.cursorY ) || !( item.window.flags & rm::flag::HasFocus ) )
		return false;
	if ( key == K_UPARROW || key == K_KP_UPARROW || key == K_MWHEELUP ) {
		if ( --scroll.startPos < 0 ) scroll.startPos = 0;
		return true;
	}
	if ( key == K_DOWNARROW || key == K_KP_DOWNARROW || key == K_MWHEELDOWN ) {
		if ( ++scroll.startPos > max ) scroll.startPos = max;
		return true;
	}
	if ( key == K_PGUP || key == K_KP_PGUP ) {
		scroll.startPos = std::max( 0, scroll.startPos - view );
		return true;
	}
	if ( key == K_PGDN || key == K_KP_PGDN ) {
		scroll.startPos = std::min( max, scroll.startPos + view );
		return true;
	}
	return false;
}

/* The text field's own keys while it is being edited; false ends editing. */
bool TextFieldKey( rm::Item &item, int key ) {
	if ( item.cvar.empty() || !item.edit ) return false;
	rm::EditField &edit = *item.edit;
	std::string value = CvarString( item.cvar );
	const int length = static_cast<int>( value.size() );
	const int maxChars = edit.maxChars ? edit.maxChars : MAX_CVAR_VALUE_STRING - 1;
	if ( key & K_CHAR_FLAG ) {
		key &= ~K_CHAR_FLAG;
		if ( key == 'h' - 'a' + 1 ) { // backspace
			if ( item.cursorPos > 0 ) {
				value.erase( item.cursorPos - 1, 1 );
				item.cursorPos--;
				if ( item.cursorPos < edit.paintOffset ) edit.paintOffset--;
			}
			Cvar_Set( item.cvar.c_str(), value.c_str() );
			return true;
		}
		if ( key < 32 || !item.cvar.size() ) return true;
		if ( item.type == rm::TypeNumericField && ( key < '0' || key > '9' ) ) return false;
		if ( !Key_GetOverstrikeMode() ) {
			if ( length >= maxChars ) return true;
			value.insert( item.cursorPos, 1, static_cast<char>( key ) );
		} else {
			if ( item.cursorPos >= maxChars ) return true;
			if ( item.cursorPos < length ) value[item.cursorPos] = static_cast<char>( key );
			else value.push_back( static_cast<char>( key ) );
		}
		if ( item.cursorPos < length + 1 ) {
			item.cursorPos++;
			if ( edit.maxPaintChars && item.cursorPos > edit.maxPaintChars ) edit.paintOffset++;
		}
		Cvar_Set( item.cvar.c_str(), value.c_str() );
		return true;
	}
	switch ( key ) {
	case K_DEL: case K_KP_DEL:
		if ( item.cursorPos < length ) {
			value.erase( item.cursorPos, 1 );
			Cvar_Set( item.cvar.c_str(), value.c_str() );
		}
		return true;
	case K_RIGHTARROW: case K_KP_RIGHTARROW:
		if ( edit.maxPaintChars && item.cursorPos >= edit.maxPaintChars && item.cursorPos < length ) {
			item.cursorPos++;
			edit.paintOffset++;
			return true;
		}
		if ( item.cursorPos < length ) item.cursorPos++;
		return true;
	case K_LEFTARROW: case K_KP_LEFTARROW:
		if ( item.cursorPos > 0 ) item.cursorPos--;
		if ( item.cursorPos < edit.paintOffset ) edit.paintOffset--;
		return true;
	case K_HOME: case K_KP_HOME:
		item.cursorPos = 0;
		edit.paintOffset = 0;
		return true;
	case K_END: case K_KP_END:
		item.cursorPos = length;
		if ( edit.maxPaintChars && item.cursorPos > edit.maxPaintChars ) edit.paintOffset = length - edit.maxPaintChars;
		return true;
	case K_INS: case K_KP_INS:
		Key_SetOverstrikeMode( Key_GetOverstrikeMode() ? qfalse : qtrue );
		return true;
	default:
		break;
	}
	return !( key == K_TAB || key == K_DOWNARROW || key == K_KP_DOWNARROW || key == K_UPARROW ||
		key == K_KP_UPARROW || key == K_ENTER || key == K_KP_ENTER || key == K_ESCAPE );
}

bool ItemKey( rm::Item &item, int key, bool down ) {
	if ( ui.itemCapture ) {
		ui.itemCapture = nullptr;
	} else if ( down && ( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) ) {
		if ( ( item.type == rm::TypeListBox && ( item.window.flags & ( rm::flag::ListThumb | rm::flag::ListLeftArrow |
			rm::flag::ListRightArrow ) ) ) )
			ui.itemCapture = &item;
	}
	if ( !down ) return false;
	switch ( item.type ) {
	case rm::TypeListBox: return ListKey( item, key, false );
	case rm::TypeTextScroll: return TextScrollKey( item, key );
	case rm::TypeYesNo: return YesNoKey( item, key );
	case rm::TypeMulti: return MultiKey( item, key );
	case rm::TypeBind: return BindKey( item, key, down );
	case rm::TypeSlider: return SliderKey( item, key );
	case rm::TypeText: return TextKey( item, key );
	default: return false;
	}
}

void Action( rm::Item &item ) { RunScript( item, item.action ); }

rm::Item *SetNextCursorItem( rm::Menu &menu ) {
	bool wrapped = false;
	const int old = menu.cursorItem;
	const int count = static_cast<int>( menu.items.size() );
	if ( menu.cursorItem == -1 ) { menu.cursorItem = 0; wrapped = true; }
	while ( menu.cursorItem < count ) {
		menu.cursorItem++;
		if ( menu.cursorItem >= count ) {
			if ( wrapped ) break;
			wrapped = true;
			menu.cursorItem = 0;
		}
		rm::Item &item = *menu.items[menu.cursorItem];
		if ( SetFocus( item, ui.cursorX, ui.cursorY ) ) {
			HandleMouseMove( &menu, item.window.rect.x + 1, item.window.rect.y + 1 );
			return &item;
		}
	}
	menu.cursorItem = old;
	return nullptr;
}

rm::Item *SetPrevCursorItem( rm::Menu &menu ) {
	bool wrapped = false;
	const int old = menu.cursorItem;
	const int count = static_cast<int>( menu.items.size() );
	if ( menu.cursorItem < 0 ) { menu.cursorItem = count - 1; wrapped = true; }
	while ( menu.cursorItem > -1 ) {
		menu.cursorItem--;
		if ( menu.cursorItem < 0 ) {
			if ( wrapped ) break;
			wrapped = true;
			menu.cursorItem = count - 1;
		}
		rm::Item &item = *menu.items[menu.cursorItem];
		if ( SetFocus( item, ui.cursorX, ui.cursorY ) ) {
			HandleMouseMove( &menu, item.window.rect.x + 1, item.window.rect.y + 1 );
			return &item;
		}
	}
	menu.cursorItem = old;
	return nullptr;
}

bool OverActiveItem( rm::Menu &menu, float x, float y ) {
	if ( !( menu.window.flags & ( rm::flag::Visible | rm::flag::Forced ) ) || !Contains( menu.window.rect, x, y ) )
		return false;
	for ( auto &item : menu.items ) {
		if ( !( item->window.flags & ( rm::flag::Visible | rm::flag::Forced ) ) ) continue;
		if ( item->window.flags & rm::flag::Decoration ) continue;
		if ( Contains( item->window.rect, x, y ) ) return true;
	}
	return false;
}

void HandleKey( rm::Menu *menu, int key, bool down );

// A click outside a non-popup menu: an out-of-bounds menu closes, and a menu
// whose active item is under the cursor takes the click.
void OutOfBoundsClick( rm::Menu &menu, int key, bool down ) {
	if ( down && ( menu.window.flags & rm::flag::OutOfBoundsClick ) ) {
		RunCloseScript( menu );
		menu.window.flags &= ~( rm::flag::HasFocus | rm::flag::Visible );
	}
	for ( auto &each : ui.menus ) {
		if ( OverActiveItem( *each, ui.cursorX, ui.cursorY ) ) {
			RunCloseScript( menu );
			menu.window.flags &= ~( rm::flag::HasFocus | rm::flag::Visible );
			HandleMouseMove( each.get(), ui.cursorX, ui.cursorY );
			HandleKey( each.get(), key, down );
		}
	}
}

void RunAccept( rm::Menu &menu ) {
	RunMenuScript( menu, menu.onAccept );
}

/* Menu_HandleKey: a bind waiting for its key or a field being edited takes
 * the key first, then the focused item, then the menu's own keys. */
void HandleKey( rm::Menu *menu, int key, bool down ) {
	static bool inHandler = false;
	if ( inHandler ) return;
	inHandler = true;
	struct Release { ~Release() { inHandler = false; } } release;
	if ( ui.waitingForKey && down ) {
		if ( ui.bindItem ) BindKey( *ui.bindItem, key, down );
		return;
	}
	if ( ui.editingField && down ) {
		if ( !ui.editItem || !TextFieldKey( *ui.editItem, key ) ) {
			ui.editingField = false;
			ui.editItem = nullptr;
			return;
		}
		if ( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) {
			ui.editingField = false;
			ui.editItem = nullptr;
			DisplayMouseMove( ui.cursorX, ui.cursorY );
		} else if ( key == K_TAB || key == K_UPARROW || key == K_DOWNARROW ) {
			return;
		}
	}
	if ( !menu ) return;
	if ( down && !( menu->window.flags & rm::flag::Popup ) && !Contains( menu->window.rect, ui.cursorX, ui.cursorY ) ) {
		static bool inOutOfBounds = false;
		if ( !inOutOfBounds && ( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) ) {
			inOutOfBounds = true;
			inHandler = false;
			OutOfBoundsClick( *menu, key, down );
			inOutOfBounds = false;
			return;
		}
	}
	rm::Item *item = nullptr;
	for ( auto &each : menu->items ) {
		if ( each->window.flags & rm::flag::HasFocus ) { item = each.get(); break; }
	}
	if ( item && item->disabled ) return;
	if ( item && ItemKey( *item, key, down ) ) {
		if ( !item->disabled ) Action( *item );
		return;
	}
	if ( !down ) return;
	if ( !( key & K_CHAR_FLAG ) ) {
		const char *binding = Key_GetBinding( key );
		if ( binding && Same( binding, "datapad" ) && ( Same( menu->window.name, "datapadMissionMenu" ) ||
			Same( menu->window.name, "datapadWeaponsMenu" ) || Same( menu->window.name, "datapadForcePowersMenu" ) ||
			Same( menu->window.name, "datapadInventoryMenu" ) ) )
			key = K_ESCAPE;
	}
	switch ( key ) {
	case K_F11:
		if ( Cvar_VariableIntegerValue( "developer" ) ) ui.debugMode = !ui.debugMode;
		break;
	case K_F12:
		if ( Cvar_VariableIntegerValue( "developer" ) ) Execute( "screenshot\n" );
		break;
	case K_KP_UPARROW: case K_UPARROW:
		SetPrevCursorItem( *menu );
		break;
	case K_ESCAPE:
		if ( !ui.waitingForKey && !menu->onESC.empty() ) RunMenuScript( *menu, menu->onESC );
		break;
	case K_TAB: case K_KP_DOWNARROW: case K_DOWNARROW:
		SetNextCursorItem( *menu );
		break;
	case K_MOUSE1: case K_MOUSE2:
		if ( item ) {
			if ( item->type == rm::TypeText ) {
				if ( Contains( item->window.rect, ui.cursorX, ui.cursorY ) ) {
					if ( !item->action.empty() ) Action( *item );
					else RunAccept( *menu );
				}
			} else if ( item->type == rm::TypeEditField || item->type == rm::TypeNumericField ) {
				if ( Contains( item->window.rect, ui.cursorX, ui.cursorY ) ) {
					item->cursorPos = 0;
					ui.editingField = true;
					ui.editItem = item;
				}
			} else if ( item->type == rm::TypeMulti || item->type == rm::TypeYesNo || item->type == rm::TypeSlider ) {
				if ( !item->accept.empty() ) RunScript( *item, item->accept );
				else RunAccept( *menu );
			} else if ( Contains( item->window.rect, ui.cursorX, ui.cursorY ) ) {
				Action( *item );
			}
		} else {
			RunAccept( *menu );
		}
		break;
	case K_KP_ENTER: case K_ENTER:
		if ( item ) {
			if ( item->type == rm::TypeEditField || item->type == rm::TypeNumericField ) {
				item->cursorPos = 0;
				ui.editingField = true;
				ui.editItem = item;
			} else {
				Action( *item );
			}
		}
		break;
	default:
		break;
	}
}

/* ---- the games' own scripts and lists ---------------------------------------- */

void ReadSaves() {
	/* The single-player save format is not served yet (the savegame gate);
	 * the list stays empty rather than offering games it cannot load. */
	ui.saves.clear();
	ui.saveCursor = 0;
	ui.savesRead = true;
}

void LoadMods() {
	ui.mods.clear();
	char list[16384];
	const int count = FS_GetFileList( "$modlist", "", list, sizeof( list ) );
	const char *name = list;
	for ( int i = 0; i < count && *name; ++i ) {
		const std::size_t nameLength = std::strlen( name );
		const char *description = name + nameLength + 1;
		ui.mods.push_back( { name, description } );
		name = description + std::strlen( description ) + 1;
	}
}

void FeederStep( rm::Item &item, int step ) {
	const int count = FeederCount( item.special );
	if ( count <= 0 ) return;
	int value = item.special == FeederMods ? ui.modIndex : item.special == FeederSaveGames ? ui.saveCursor : -1;
	value += step;
	if ( value >= count ) value = 0;
	if ( value < 0 ) value = count - 1;
	FeederSelection( item.special, value, &item );
}

void UpdateCvar( const std::string &name ) {
	const int value = static_cast<int>( CvarValue( name ) );
	if ( Same( name, "s_khz" ) ) { Execute( "snd_restart\n" ); return; }
	if ( Same( name, "ui_SetName" ) ) { Cvar_Set( "name", CvarString( "ui_Name" ).c_str() ); return; }
	if ( Same( name, "ui_GetName" ) ) { Cvar_Set( "ui_Name", CvarString( "name" ).c_str() ); return; }
	if ( Same( name, "ui_r_colorbits" ) ) {
		if ( value == 0 || value == 16 ) Cvar_SetValue( "ui_r_depthbits", static_cast<float>( value ) );
		else if ( value == 32 ) Cvar_SetValue( "ui_r_depthbits", 24 );
		return;
	}
	if ( Same( name, "ui_r_lodbias" ) ) {
		if ( value >= 0 && value <= 2 ) Cvar_SetValue( "ui_r_subdivisions", static_cast<float>( 4 + value * 8 ) );
		return;
	}
	if ( Same( name, "ui_r_glCustom" ) ) {
		/* The presets as the engines set them. They choose the menu's own
		 * copies of the video cvars; nothing changes until they are applied. */
		struct Preset { int subdivisions, lodbias, colorbits, depthbits, picmip, mode, texturebits, fastSky, inGameVideo;
			const char *textureMode; };
		static const Preset presets[] = {
			{ 4, 0, 32, 24, 0, 4, 32, 0, 1, "GL_LINEAR_MIPMAP_LINEAR" },
			{ 4, 0, 0, 24, 1, 3, 0, 0, 1, "GL_LINEAR_MIPMAP_LINEAR" },
			{ 12, 1, 0, 0, 2, 3, 0, 1, 0, "GL_LINEAR_MIPMAP_NEAREST" },
			{ 20, 2, 16, 16, 3, 3, 16, 1, 0, "GL_LINEAR_MIPMAP_NEAREST" },
		};
		if ( value < 0 || value > 3 ) return;
		const Preset &p = presets[value];
		Cvar_SetValue( "ui_r_fullScreen", 1 );
		Cvar_SetValue( "ui_r_subdivisions", static_cast<float>( p.subdivisions ) );
		Cvar_SetValue( "ui_r_lodbias", static_cast<float>( p.lodbias ) );
		Cvar_SetValue( "ui_r_colorbits", static_cast<float>( p.colorbits ) );
		Cvar_SetValue( "ui_r_depthbits", static_cast<float>( p.depthbits ) );
		Cvar_SetValue( "ui_r_picmip", static_cast<float>( p.picmip ) );
		Cvar_SetValue( "ui_r_mode", static_cast<float>( p.mode ) );
		Cvar_SetValue( "ui_r_texturebits", static_cast<float>( p.texturebits ) );
		Cvar_SetValue( "ui_r_fastSky", static_cast<float>( p.fastSky ) );
		Cvar_SetValue( "ui_r_inGameVideo", static_cast<float>( p.inGameVideo ) );
		Cvar_Set( "ui_r_texturemode", p.textureMode );
		return;
	}
	if ( Same( name, "ui_mousePitch" ) ) {
		Cvar_SetValue( "m_pitch", value == 0 ? 0.022f : -0.022f );
		return;
	}
	Com_Printf( "unknown UI script UPDATE %s\n", name.c_str() );
}

const char *const kVideoCvars[] = { "r_mode", "r_fullscreen", "r_colorbits", "r_lodbias", "r_picmip",
	"r_texturebits", "r_texturemode", "r_detailtextures", "r_ext_compress_textures", "r_depthbits",
	"r_subdivisions", "r_fastSky", "r_inGameVideo", "r_allowExtensions" };

void GetVideoSetup() {
	Cvar_Get( "ui_r_glCustom", "4", CVAR_ARCHIVE );
	for ( const char *name : kVideoCvars ) {
		const std::string copy = std::string( "ui_" ) + name;
		Cvar_Get( copy.c_str(), "0", CVAR_ROM );
		Cvar_Set( copy.c_str(), CvarString( name ).c_str() );
	}
	Cvar_Get( "ui_r_modified", "0", CVAR_ROM );
	Cvar_Set( "ui_r_modified", "0" );
}

void UpdateVideoSetup() {
	for ( const char *name : kVideoCvars ) Cvar_Set( name, CvarString( std::string( "ui_" ) + name ).c_str() );
	Cvar_Set( "ui_r_modified", "0" );
	Execute( "vid_restart;" );
}

/* UI_RunMenuScript: the scripts the menus reach by name. The ones the main
 * menus use are served here; a script that needs the running single-player
 * game (its saves, its player, its datapad) says so in the console. */
void RunGameScript( rm::Tokens &args, rm::Item &item ) {
	const std::string name = args.Next( false );
	if ( name.empty() ) return;
	if ( Same( name, "resetdefaults" ) ) {
		Execute( "cvar_restart\n" );
		Execute( "exec default.cfg\n" );
		Execute( "vid_restart\n" );
	} else if ( Same( name, "saveControls" ) ) {
		SetBindConfig();
	} else if ( Same( name, "loadControls" ) ) {
		GetBindConfig();
	} else if ( Same( name, "clearError" ) ) {
		Cvar_Set( "com_errorMessage", "" );
	} else if ( Same( name, "ReadSaveDirectory" ) ) {
		ui.savesRead = false;
	} else if ( Same( name, "loadAuto" ) ) {
		CloseAll();
		Execute( "load auto\n" );
	} else if ( Same( name, "loadgame" ) ) {
		if ( ui.saveCursor >= 0 && ui.saveCursor < static_cast<int>( ui.saves.size() ) ) {
			CloseAll();
			Execute( "load " + ui.saves[ui.saveCursor] + "\n" );
		}
		ui.saveCursor = 0;
	} else if ( Same( name, "deletegame" ) ) {
		ui.savesRead = false;
	} else if ( Same( name, "LoadMods" ) ) {
		LoadMods();
	} else if ( Same( name, "RunMod" ) ) {
		if ( ui.modIndex >= 0 && ui.modIndex < static_cast<int>( ui.mods.size() ) ) {
			Cvar_Set( "fs_game", ui.mods[ui.modIndex].name.c_str() );
			Execute( "vid_restart;" );
		}
	} else if ( Same( name, "Quit" ) ) {
		Cbuf_ExecuteText( EXEC_NOW, "quit" );
	} else if ( Same( name, "Leave" ) ) {
		Execute( "disconnect\n" );
		Key_SetCatcher( KEYCATCH_UI );
		CloseAll();
	} else if ( Same( name, "getvideosetup" ) ) {
		GetVideoSetup();
	} else if ( Same( name, "updatevideosetup" ) ) {
		UpdateVideoSetup();
	} else if ( Same( name, "glCustom" ) ) {
		Cvar_Set( "ui_r_glCustom", "4" );
	} else if ( Same( name, "update" ) ) {
		const std::string cvar = args.Next( false );
		if ( !cvar.empty() ) UpdateCvar( cvar );
	} else if ( Same( name, "startgame" ) ) {
		CloseAll();
		Execute( Academy() ? "map yavin1\n" : "map kejim_post\n" );
	} else if ( Same( name, "startmap" ) ) {
		const std::string map = args.Next( false );
		CloseAll();
		Execute( "maptransition " + map + "\n" );
	} else if ( Same( name, "closeingame" ) ) {
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Key_ClearStates();
		Cvar_Set( "cl_paused", "0" );
		CloseAll();
		if ( Cvar_VariableIntegerValue( "ui_missionfailed" ) == 1 ) {
			ActivateByName( "missionfailed_menu" );
			Key_SetCatcher( KEYCATCH_UI );
		} else {
			ActivateByName( "mainhud" );
		}
	} else if ( Same( name, "closedatapad" ) ) {
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Key_ClearStates();
		Cvar_Set( "cl_paused", "0" );
		CloseAll();
		ActivateByName( "mainhud" );
		for ( const char *cvar : { "cg_updatedDataPadForcePower1", "cg_updatedDataPadForcePower2",
			"cg_updatedDataPadForcePower3", "cg_updatedDataPadObjective" } )
			Cvar_Set( cvar, "0" );
	} else if ( Same( name, "clearmouseover" ) ) {
		const std::string itemName = args.Next( false );
		if ( rm::Item *found = FindItem( Focused(), itemName ) ) found->window.flags &= ~rm::flag::MouseOver;
	} else if ( Same( name, "checkvid1data" ) ) {
		const std::string to = args.Next( false ), warning = args.Next( false );
		rm::Menu *menu = Focused();
		rm::Item *apply = FindItem( menu, "applyChanges" );
		if ( !menu ) Com_Printf( S_COLOR_YELLOW "WARNING: No videoMenu was found. Video data could not be checked\n" );
		else if ( !apply ) ActivateByName( to );
		else if ( apply->window.flags & rm::flag::Visible ) ActivateByName( warning );
	} else if ( Same( name, "setvid1data" ) || Same( name, "getvid1data" ) ) {
		// Outcast's driver-page copies (vid_warning.menu): nothing to carry.
	} else {
		Com_DPrintf( S_COLOR_YELLOW "Jedi single-player menu script \"%s\" is not served yet\n", name.c_str() );
	}
	(void)item;
}

/* ---- loading ---------------------------------------------------------------------- */

std::string ReadText( const std::string &path ) {
	void *data = nullptr;
	const int length = FS_ReadFile( path.c_str(), &data );
	std::string text;
	if ( length > 0 && data ) text.assign( static_cast<const char *>( data ), static_cast<std::size_t>( length ) );
	if ( data ) FS_FreeFile( data );
	return text;
}

void Warn( const rm::Diagnostic &d ) {
	Com_Printf( S_COLOR_YELLOW "WARNING: %s Line #%d of File '%s'\n", d.message.c_str(), d.line, d.file.c_str() );
}

/* UI_LoadMenus: the list file (or ui/menus.txt), each menu file it names. */
void LoadMenus( const std::string &file, bool reset ) {
	const int start = Sys_Milliseconds();
	std::string list = ReadText( file );
	std::string listName = file;
	if ( list.empty() ) {
		Com_Printf( S_COLOR_YELLOW "menu file not found: %s, using default\n", file.c_str() );
		list = ReadText( "ui/menus.txt" );
		listName = "ui/menus.txt";
		if ( list.empty() ) Com_Error( ERR_FATAL, "default menu file not found: ui/menus.txt, unable to continue!" );
	}
	if ( reset ) {
		ui.menus.clear();
		ui.menuStack.clear();
	}
	rm::Reader reader( Academy() ? rm::Dialect::Academy : rm::Dialect::Outcast, Warn );
	for ( const std::string &name : rm::Reader::MenuList( list, Warn, listName ) ) {
		const std::string text = ReadText( name );
		if ( text.empty() ) {
			Com_Printf( "UI_ParseMenu: Unable to load menu %s\n", name.c_str() );
			continue;
		}
		reader.File( name, text, ui.assets, ui.menus );
	}
	// Fonts the assets named, each registered once, in order.
	for ( const auto &font : ui.assets.fonts ) {
		bool known = false;
		const int handle = RegisterFont( font.name );
		for ( const int existing : ui.fonts ) known = known || existing == handle;
		if ( !known && handle ) ui.fonts.push_back( handle );
		if ( Same( font.name, ui.assets.mediumFont ) && handle ) ui.mediumFont = handle;
	}
	ui.cursor = Shader( ui.assets.cursor );
	ui.gradientBar = Shader( ui.assets.gradientBar );
	ui.itemFocusSound = RegisterSound( ui.assets.itemFocusSound );
	for ( const auto &sound : ui.assets.precacheSounds ) RegisterSound( sound );
	Com_Printf( "UI menu load time = %d milli seconds\n", Sys_Milliseconds() - start );
}

void Init( bool inGame ) {
	ui = State{};
	ui.academy = Com_GameIsJKASP() != 0;
	if ( !ui.academy ) {
		// jk2sp.exe UI_Load (0x49aae1): menus0..9 until one is missing.
		for ( int i = 0; i < 10; ++i ) {
			char error[160]{};
			if ( !fntech3::jk2::hoststrings::Register( va( "menus%d", i ), kOutcastMenuStrings, error ) ) break;
		}
	}
	Cvar_Get( "ui_menuFiles", "ui/menus.txt", CVAR_ARCHIVE );
	Cvar_Get( "cg_hudFiles", ui.academy ? "ui/jahud.txt" : "ui/jk2hud.txt", CVAR_ARCHIVE );
	for ( const char *name : { "ui_char_model", "ui_char_skin_head", "ui_char_skin_torso", "ui_char_skin_legs",
		"ui_saber_type", "ui_saber", "ui_saber2", "ui_saber_color", "ui_saber2_color", "ui_char_color_red",
		"ui_char_color_green", "ui_char_color_blue" } )
		Cvar_Get( name, "", 0 );
	Cvar_Get( "ui_char_anim", "BOTH_WALK1", 0 );
	Cvar_Get( "ui_PrecacheModels", "1", CVAR_ARCHIVE );
	ui.white = re.RegisterShaderNoMip( "white" );
	std::string menuSet = CvarString( "ui_menuFiles" );
	if ( menuSet.empty() ) menuSet = "ui/menus.txt";
	LoadMenus( ui.academy && inGame ? "ui/ingame.txt" : menuSet, true );
	CloseAll();
	ui.scrollBar = Shader( "gfx/menus/scrollbar.tga" );
	ui.scrollDown = Shader( "gfx/menus/scrollbar_arrow_dwn_a.tga" );
	ui.scrollUp = Shader( "gfx/menus/scrollbar_arrow_up_a.tga" );
	ui.scrollLeft = Shader( "gfx/menus/scrollbar_arrow_left.tga" );
	ui.scrollRight = Shader( "gfx/menus/scrollbar_arrow_right.tga" );
	ui.scrollThumb = Shader( "gfx/menus/scrollbar_thumb.tga" );
	ui.sliderBar = Shader( "menu/new/slider" );
	ui.sliderThumb = Shader( "menu/new/sliderthumb" );
	Cvar_Set( "ui_mousePitch", CvarValue( "m_pitch" ) >= 0 ? "0" : "1" );
	Cvar_Set( "cg_endcredits", "0" );
	Cvar_Set( "ui_missionfailed", "0" );
	ui.nullSound = RegisterSound( "sound/null" );
	GetBindConfig();
	ui.languageModification = LanguageModification();
	ui.loaded = true;
}

void Shutdown() {
	ui.menus.clear();
	ui.menuStack.clear();
	ui.loaded = false;
	// Outcast's menu packages go with the menus; the next start registers them again.
	if ( !ui.academy ) CL_JK2StringReset( kOutcastMenuStrings );
}

/* ---- the client's calls ------------------------------------------------------------ */

void MainMenu() {
	Cvar_Set( "sv_killserver", "1" );
	Key_SetCatcher( KEYCATCH_UI );
	if ( !ActivateByName( "mainMenu" ) ) LoadMenus( "ui/menus.txt", false );
	if ( !CvarString( "com_errorMessage" ).empty() ) ActivateByName( "error_popmenu" );
}

void InGameMenu() {
	CloseByName( "mainhud" );
	if ( !FindMenu( "ingameMainMenu" ) && Academy() ) LoadMenus( "ui/ingame.txt", false );
	ActivateByName( "ingameMainMenu" );
	Key_SetCatcher( KEYCATCH_UI );
}

void SetActiveMenu( int menu ) {
	switch ( menu ) {
	case UIMENU_NONE:
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Key_ClearStates();
		Cvar_Set( "cl_paused", "0" );
		CloseAll();
		return;
	case UIMENU_MAIN:
		MainMenu();
		return;
	case UIMENU_INGAME:
		InGameMenu();
		return;
	default:
		return;
	}
}

void Refresh( int realTime ) {
	if ( !( Key_GetCatcher() & KEYCATCH_UI ) ) return;
	ui.frameTime = realTime - ui.realTime;
	ui.realTime = realTime;
	const int language = LanguageModification();
	if ( language != ui.languageModification ) {
		ui.languageModification = language;
		for ( auto &menu : ui.menus ) {
			for ( auto &item : menu->items ) item->textRect.w = 0;
		}
	}
	if ( ui.menus.empty() ) return;
	for ( auto &menu : ui.menus ) MenuPaint( *menu );
	if ( ui.debugMode ) {
		const float white[4] = { 1, 1, 1, 1 };
		DrawText( 5, 25, 0.75f, white, va( "(%d,%d)", static_cast<int>( ui.cursorX ), static_cast<int>( ui.cursorY ) ),
			0, 0, 0 );
	}
	SetColor( nullptr );
	if ( ui.cursorShow ) DrawPic( ui.cursorX, ui.cursorY, kCursorSize, kCursorSize, ui.cursor );
}

void MouseEvent( int dx, int dy ) {
	ui.cursorX = std::clamp( ui.cursorX + static_cast<float>( dx ), 0.0f, kScreenWidth );
	ui.cursorY = std::clamp( ui.cursorY + static_cast<float>( dy ), 0.0f, kScreenHeight );
	if ( !ui.menus.empty() ) DisplayMouseMove( ui.cursorX, ui.cursorY );
}

void KeyEvent( int key, bool down ) {
	if ( ui.menus.empty() ) return;
	rm::Menu *menu = Focused();
	if ( menu ) {
		if ( key == K_ESCAPE && down && !AnyFullScreenVisible() && !( menu->window.flags & rm::flag::IgnoreEscape ) )
			CloseAll();
		else
			HandleKey( menu, key, down );
	} else {
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Key_ClearStates();
		Cvar_Set( "cl_paused", "0" );
	}
}

/* The engines' console commands (jasp.exe 0x494a20), and fnTech3's
 * ui_openmenu, which the multiplayer menu modules have and the single-player
 * engines do not: it opens a menu by name, for diagnostics and probes. */
bool ConsoleCommand() {
	const char *command = Cmd_Argv( 0 );
	if ( !Q_stricmp( command, "ui_load" ) ) {
		rm::Menu *focus = Focused();
		const std::string last = focus ? focus->window.name : std::string();
		std::string menuSet = CvarString( "ui_menuFiles" );
		if ( menuSet.empty() ) menuSet = "ui/menus.txt";
		LoadMenus( menuSet, true );
		CloseAll();
		if ( !last.empty() ) ActivateByName( last );
		return true;
	}
	if ( !Q_stricmp( command, "ui_report" ) ) {
		std::size_t items = 0;
		for ( const auto &menu : ui.menus ) items += menu->items.size();
		Com_Printf( "%d menus, %d items\n", static_cast<int>( ui.menus.size() ), static_cast<int>( items ) );
		return true;
	}
	if ( !Q_stricmp( command, "ui_openmenu" ) ) {
		if ( Cmd_Argc() < 2 ) {
			Com_Printf( "usage: ui_openmenu <menu>\n" );
			return true;
		}
		CloseAll();
		Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
		ActivateByName( Cmd_Argv( 1 ) );
		return true;
	}
	return false;
}

intptr_t QDECL Entry( int command, ... ) {
	int arg[3] = {};
	va_list ap;
	va_start( ap, command );
	for ( int &value : arg ) value = va_arg( ap, int );
	va_end( ap );

	switch ( command ) {
	case UI_GETAPIVERSION:
		return 0;
	case UI_INIT:
		Init( arg[0] != 0 );
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
		Refresh( arg[0] );
		return 0;
	case UI_IS_FULLSCREEN:
		return AnyFullScreenVisible() ? qtrue : qfalse;
	case UI_SET_ACTIVE_MENU:
		SetActiveMenu( arg[0] );
		return 0;
	case UI_CONSOLE_COMMAND:
		return ConsoleCommand() ? qtrue : qfalse;
	case UI_MENUS_ANY_VISIBLE:
		for ( auto &menu : ui.menus ) {
			if ( menu->window.flags & rm::flag::Visible ) return qtrue;
		}
		return qfalse;
	default:
		return 0;
	}
}

} // namespace

/*
====================
CL_JediSPLoadUI

The Jedi single-player engines' own menus as the client's UI module.
====================
*/
extern "C" vm_t *CL_JediSPLoadUI( void ) {
	return VM_CreateHosted( VM_UI, Entry );
}
