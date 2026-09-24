/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* American McGee's Alice's menus and intro, which its engine owns: the game
 * ships no menu module, only twelve `ui/*.urc` resources its engine reads at
 * start-up and turns into menus (UI_Init 0x004104d0).
 *
 * The engine's UI is hosted here as the client's UI module (VM_CreateHosted),
 * the way Elite Force's single-player client game is hosted, so the client's
 * existing uivm call sites -- keys, mouse, the active-menu requests, the
 * full-screen query and shutdown -- reach it unchanged. Every export it does
 * not serve answers zero.
 *
 * Retail, all alice.exe:
 *
 *   menu manager     0x00aa6a18   CurrentMenu 0x004c3740, push 0x004c3b20,
 *                                 pop 0x004c3f20, clear 0x004c34b0
 *   pushmenu         0x0040d050   popmenu 0x0040d0b0, forcemenu 0x0040d080,
 *                                 showmenu 0x0040d0e0, togglemenu 0x0040cbb0,
 *                                 widgetcommand 0x0040d110 (current menu only)
 *   menu escape      0x0040d470   refused while loading, at `main` while
 *                                 disconnected or dead; pops with restore, or
 *                                 pushes `main`
 *   widget draw      0x004bd740   waits, hover commands, motion and fade, the
 *                                 background, border, shaders, class draw
 *   menu show        0x004c4d50   enabled widgets shown, motion in, commands
 *   activategroup    0x004b7a30
 *   motion           0x004bb990, 0x004bbb10
 *   Label draw       0x004d5910   the linked cvar, else the title
 *   _intro           0x0040fc70   the intro's states (alice_screen.hpp)
 *   intro plate      0x0040c620
 *   UI_Update        0x0040c800   the pause a full-screen menu holds
 *
 * Every widget class the twelve menus use is served:
 *
 *   Label, Button      draw 0x004d5910; a button holds the mouse (0x004cf8c0)
 *                      and clicks when let go over itself (0x004cfa30)
 *   CheckBox           draw 0x004e0af0, release 0x004e0780, cvar 0x004e05e0
 *   Slider             draw 0x004db440, press 0x004daea0, drag 0x004dac70,
 *                      arrows 0x004da8b0 and 0x004da940, cvar 0x004db120
 *   Field              draw 0x004df7c0, keys 0x004dfc00, characters 0x004dfaa0
 *   List               draw 0x004df190, press 0x004deee0, cvar 0x004decf0
 *   FakkBindList       bind file 0x004e3990, rows 0x004e3bd0, draw 0x004e4960,
 *                      keys 0x004e4330, key buttons 0x004d1650 and 0x004d1700,
 *                      the window manager's bindings 0x004b4da0 and 0x004b52d0
 *   FakkLoadGameClass  list 0x0041a210, pictures 0x0041af20, load 0x0041bcb0,
 *                      save 0x0041bec0, remove 0x0041bbe0, errors 0x00419ff0
 *   AutoScroll         draw 0x004c0a40, resetscroll 0x004c0490
 *   Alice3D            draw 0x004d3310, animation 0x004d27c0
 *
 * with a menu's cvars saved when it is pushed and put back by Escape or
 * ui_restorecvars (0x004c4540, 0x004c4830), ui_setcvars' restart check
 * (0x004c4910), border colours made from the background (0x004bc700) and the
 * window manager's mouse events (0x004b3a50). Left out: the scrollbar's arrows
 * do not repeat while held, UIWidget3D does not turn the model's head after
 * the mouse (bone "Bip01 Head", 0x004d31ed), and the developer menus' own
 * commands (soundpicker, snd, cam, view*) belong to parts of retail this
 * client does not have. See docs/fntech3/2026-09-24-alice-menus.md. */

#include "client_alice_ui.hpp"
#include "../compat/alice_menu.hpp"
#include "../compat/alice_noise.hpp"
#include "../compat/alice_savegame.hpp"
#include "../compat/alice_urc.hpp"
#include "../qcommon/alice_tiki.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <ctime>
#include <iterator>
#include <string>
#include <vector>

namespace {

namespace urc = fntech3::alice::urc;
namespace screen = fntech3::alice::screen;
namespace font = fntech3::alice::ritualfont;
namespace mw = fntech3::alice::menu;
namespace noise = fntech3::alice::noise;
namespace alicesave = fntech3::alice::savegame;

enum class WidgetKind { Label, Button, CheckBox, Slider, Field, List, BindList, LoadGame, AutoScroll, Alice3D };

enum class Motion { None, In, Out };

struct KeyCommand {
	int key = -1;
	std::string command;
};

struct Wait {
	std::string command;
	int delayMs = 0;
	int fireAt = -1;
};

/* UIFakkBindList (constructor 0x004e37f0): the rows its bind file lists
 * (built at 0x004e3bd0), its selection and its scrollbar (UIVertScroll,
 * 0x004d80b0, attached at 0x004d9370). */
struct BindState {
	std::string file;
	mw::BindLayout layout;
	bool built = false;
	/* The window manager's key table for each row (0x004b4bb0): the first two
	 * keys found bound to the row's command, -1 for none. */
	std::vector<int> key1, key2;
	int row = 0;          // the selected row (+0x1ec), from 0
	int column = 1;       // 1 the name, 2 the key (+0x1f0)
	int top = 0;          // the scrollbar's position (+0x1d8)
	int page = 0;         // rows below the header (the scrollbar's +0x1d4)
	bool capturing = false;   // the row's key button waits for a key ([0xaa47a0])
	int animWait = -1;    // ms since the row changed (+0x1f8), -1 once played
	int animRow = -1;     // the row that wait is for (+0x1fc)
	int scrollPart = 0;   // UIVertScroll's pressed part (+0x1f8): 1 up, 2 down
	bool thumbDrag = false;
	float thumbGrab = 0.0f;
	int model = -1;       // the menu's `alice3d` widget (+0x200)
};

/* UIAlice3D (constructor 0x004d2ca0) over UIWidget3D (0x004d22d0). */
struct ModelState {
	std::string model;             // +0x1d4
	std::string anim, previous;    // +0x1d8, +0x1dc
	float frame = 0.0f;            // +0x21c, advanced 0.02 a millisecond
	int numFrames = 0;             // +0x210
	bool loop = false;             // +0x220
	float yaw = 0.0f, previousYaw = 0.0f;   // +0x224, +0x228
	float blend = 1.0f;            // +0x22c
	int previousFrame = 0;         // +0x234
	float scale = 1.0f;            // +0x1f0
	int thunderWait = 0;           // +0x26c
	int thunder = 0;               // +0x270
};

struct Widget {
	WidgetKind kind = WidgetKind::Label;
	std::string name;
	/* InitFrame's defaults (0x004cc2a1): 10, 10, 100 by 64, courier-14. */
	float x = 10, y = 10, w = 100, h = 64;
	/* UIWidget's colours (0x004bb01e): light grey behind, white in front. */
	float fg[4] = { 1, 1, 1, 1 };
	float bg[4] = { 0.75f, 0.75f, 0.75f, 1 };
	int border = 2;
	std::string title;
	std::string fontName = "courier-14";
	int font = -1;
	int justifyH = 0, justifyV = 1;
	float fontScale = 1.0f, charSpacing = 0.0f, lineSpacing = 0.0f, fontAngle = 0.0f;
	float fontPosX = -1.0f, fontPosY = -1.0f;
	std::string shader, hoverShader, staticImage;
	float fadeIn = 0.0f;
	bool inverseAlpha = false;
	urc::Direction direction = urc::Direction::None;
	float motionTime = 0.0f;
	float pointX = 0.0f, pointY = 0.0f;
	std::vector<Wait> waits;
	std::string stuffCommand, mouseEnterCommand, mouseExitCommand, clickSound, hoverSound, linkCvar, groupId;
	int orderNumber = 0;
	std::vector<KeyCommand> keyCommands;
	bool enabled = true, neverDraw = false, noInput = false;
	/* forcescissor (0x004b94a0): drawn clipped to its own rectangle (0x004bd560). */
	bool forceScissor = false;
	/* initdata (0x004b85c0): written to the linked cvar when the file is read. */
	bool hasInitData = false;
	std::string initData;
	/* The border colours bgcolor makes (setBackgroundColor 0x004bc700); the
	 * constructor makes them from its own light grey (0x004bb0ff). */
	mw::BorderColor border3d = mw::MakeBorderColor( { 0.75f, 0.75f, 0.75f, 1.0f } );
	/* The linked cvar as the menu's push saved it (SaveCvars 0x004c4540). */
	std::string savedCvar;

	/* UICheckBox (0x004e0200). */
	struct {
		std::string checkedCommand, uncheckedCommand, checkedShader, uncheckedShader;
		bool checked = false, depressed = false;
	} check;
	/* UISlider (0x004d9880). */
	struct {
		mw::Slider s;
		bool leftPressed = false, rightPressed = false, dragging = false;
		float dragFrom = 0.0f;   // +0x1ec, put back when a drag leaves the widget
		int repeatAt = -1;       // when autoinc/autodec (0x004daa10, 0x004daae0) runs
	} slider;
	/* UIField (0x004df730). */
	mw::EditLine field;
	/* UIList (0x004de400). */
	struct {
		std::vector<mw::ListItem> items;
		int current = 0;         // 1-based, 0 for none (+0x1dc)
		bool previousPressed = false, nextPressed = false;
	} list;
	BindState bind;
	/* UIFAKKLoadGameClass (0x00419dc0). */
	struct {
		mw::SavePage page;
		std::vector<mw::SaveRow> rows;
	} load;
	/* UIAutoScroll (0x004c0340). */
	struct {
		std::vector<std::string> lines;
		std::vector<mw::ScrollImage> images;
		float speed = mw::DefaultScrollSpeed;
		float position = 0.0f;
	} scroll;
	ModelState model;

	/* Runtime. */
	bool shown = false;
	Motion motion = Motion::None;
	int motionStart = 0;
};

struct Menu {
	std::string name, file;
	float w = 640, h = 480;
	urc::Direction direction = urc::Direction::None;
	float motionTime = 0.0f;
	float bg[4] = { 0.75f, 0.75f, 0.75f, 1 };
	float bgFill[4] = { 0, 0, 0, 0 };
	bool hasBgFill = false;
	int border = 2;
	mw::BorderColor border3d = mw::MakeBorderColor( { 0.75f, 0.75f, 0.75f, 1.0f } );
	bool fullscreen = false;
	float fadeOverlay = 0.0f;
	int align = 0;
	std::vector<std::string> showCommands, hideCommands;
	std::vector<KeyCommand> keyCommands;
	std::vector<Widget> widgets;

	/* Runtime. */
	bool visible = false;
	bool hiding = false;
	int showStart = 0;
	int hideStart = 0;
	int hideEnd = 0;
	int focus = -1;
	int hovered = -1;
	int pressed = -1;
};

std::vector<Menu> menus;
std::vector<int> menuStack;
/* The names on the stack when the module was shut down for a video restart. */
std::vector<std::string> savedStack;
int inputLockUntil;
float cursorX = -1.0f, cursorY = -1.0f;
qhandle_t cursorShader;
/* Cursor mode (0x00427b00 on, 0x00427b50 off) as the menus last set it, and
 * the cursor's buttons as last read (0x004282b0). */
bool cursorModeOn;
int cursorButtons;
bool uiInitialized;
bool firstInitDone;
/* 0x0053162c: what a full-screen menu did about the pause. 0: nothing yet;
 * 1: it paused the game; 2: it found the game already paused, and leaves that
 * pause to whoever took it (UI_Update 0x0040c92e..0x0040c95e). */
int uiPauseState;

/* The intro: its state (0x005315f8), the scheduled next step that retail
 * queues as "wait N; _intro", and the legal plate (0x005315fc-0x00531610).
 * Its logos continue it from their own end (cl_cin.cpp). */
int introState;
int introNextStep = -1;
bool introQueued;
struct {
	std::string image;
	float scale = 0, from = 0, to = 0, lengthMs = 0;
	int start = 0;
} introPlate;

cvar_t *ui_hud, *ui_subtitles, *ui_console;

/* keybind.scr's translations (UI_LoadTransTable 0x004d1810) and maplist.scr's
 * titles (UI_LoadMapList 0x0040b5a0), read as the UI starts. */
std::vector<mw::KeyAlias> keyAliases;
screen::MapList mapTitles;
/* initdata writes its cvar once a process (LoadMenus). */
bool initDataApplied;
/* ui_hidemouse (0x0040f2e0): the view has the mouse while a menu stays up,
 * until ui_showmouse or the next push or pop. */
bool viewHasMouse;
/* The load/save list's `savegame` (0x0041bec0), finished on the next frame. */
struct {
	bool active = false;
	int menu = -1, widget = -1;
} pendingSave;
/* The cursor in menu units as the last frame saw it, for the drag events
 * the window manager sends (0x004b3bab). */
float lastVirtualX = -1.0f, lastVirtualY = -1.0f;

/* ---- small helpers -------------------------------------------------------- */

float ToFloat( const std::string &text ) { return static_cast<float>( atof( text.c_str() ) ); }

bool ReadColor( const std::vector<std::string> &args, float *out ) {
	if ( args.size() < 4 ) return false;
	for ( int i = 0; i < 4; ++i ) out[i] = ToFloat( args[i] );
	return true;
}

mw::Rgba ToRgba( const float *c ) { return { c[0], c[1], c[2], c[3] }; }

int KeyFromName( const std::string &name ) {
	if ( name.size() == 1 ) {
		char c = name[0];
		if ( c >= 'A' && c <= 'Z' ) c = static_cast<char>( c - 'A' + 'a' );
		return static_cast<unsigned char>( c );
	}
	return Key_StringToKeynum( name.c_str() );
}

void ExecuteLine( const std::string &command ) {
	if ( command.empty() ) return;
	Cbuf_AddText( command.c_str() );
	if ( command.back() != '\n' ) Cbuf_AddText( "\n" );
}

void PlayLocalSound( const std::string &name ) {
	if ( name.empty() ) return;
	S_StartLocalSound( S_RegisterSound( name.c_str(), qfalse ), CHAN_LOCAL_SOUND );
}

int Now() { return cls.realtime; }

Menu *CurrentMenu() {
	return menuStack.empty() ? nullptr : &menus[menuStack.back()];
}

int FindMenu( const char *name ) {
	for ( std::size_t i = 0; i < menus.size(); ++i ) {
		if ( !Q_stricmp( menus[i].name.c_str(), name ) ) return static_cast<int>( i );
	}
	return -1;
}

int MenuIndex( const Menu &menu ) {
	return static_cast<int>( &menu - menus.data() );
}

/* A widget by name in one menu, the first whose name matches exactly (the
 * child lookup 0x004bc330 and widgetcommand's 0x004c50f0). */
int FindWidgetIndex( const Menu &menu, const char *name ) {
	for ( std::size_t i = 0; i < menu.widgets.size(); ++i ) {
		if ( menu.widgets[i].name == name ) return static_cast<int>( i );
	}
	return -1;
}

Widget *FindWidget( Menu &menu, const char *name ) {
	const int index = FindWidgetIndex( menu, name );
	return index >= 0 ? &menu.widgets[index] : nullptr;
}

/* ---- the UI's cvars (0x0040e1a0, 0x0040e2d0, 0x004c1860..0x004c1930) ---- */

bool CvarExists( const char *name ) {
	return name && name[0] && !( Cvar_Flags( name ) & CVAR_NONEXISTENT );
}

/* The value a latched cvar waits to take, when it differs from its value. */
bool CvarLatched( const char *name, std::string &latched ) {
	char buffer[MAX_CVAR_VALUE_STRING];
	Cvar_LatchedVariableStringBufferSafe( name, buffer, sizeof( buffer ), 0 );
	if ( !strcmp( buffer, Cvar_VariableString( name ) ) ) return false;
	latched = buffer;
	return true;
}

/* The UI reads a latched cvar's waiting value rather than its current one
 * (0x0040e1a0), and reads a latched number as a whole number (0x004c1890
 * converts it with atoi). */
std::string UiCvarString( const char *name ) {
	std::string latched;
	if ( CvarLatched( name, latched ) && !latched.empty() ) return latched;
	return Cvar_VariableString( name );
}

int UiCvarInt( const char *name, int fallback ) {
	if ( !CvarExists( name ) ) return fallback;
	std::string latched;
	if ( CvarLatched( name, latched ) ) return atoi( latched.c_str() );
	return Cvar_VariableIntegerValue( name );
}

float UiCvarFloat( const char *name, float fallback ) {
	if ( !CvarExists( name ) ) return fallback;
	std::string latched;
	if ( CvarLatched( name, latched ) ) return static_cast<float>( atoi( latched.c_str() ) );
	return Cvar_VariableValue( name );
}

/* UI_SetCvar (0x0040e2d0) is Cvar_Set2 without force, so a latched cvar
 * latches until its subsystem restarts. */
void UiCvarSet( const std::string &name, const char *value ) {
	if ( !name.empty() ) Cvar_Set2( name.c_str(), value, qfalse );
}

/* ---- drawing in the menus' 640 by 480 units ------------------------------- */

float ScaleX() { return cls.glconfig.vidWidth / 640.0f; }
float ScaleY() { return cls.glconfig.vidHeight / 480.0f; }

/* DrawBox (0x004b5a40): scaled to the screen and cut to whole pixels. */
void VirtualFill( float x, float y, float w, float h, const float *rgba ) {
	const float sx = ScaleX(), sy = ScaleY();
	CL_AliceFill( static_cast<float>( static_cast<int>( x * sx ) ), static_cast<float>( static_cast<int>( y * sy ) ),
		static_cast<float>( static_cast<int>( w * sx ) ), static_cast<float>( static_cast<int>( h * sy ) ), rgba );
}

/* The UI's DrawStretchPic (0x0040e2f0). */
void VirtualPic( float x, float y, float w, float h, const float *rgba, qhandle_t shader,
	float s0 = 0.0f, float t0 = 0.0f, float s1 = 1.0f, float t1 = 1.0f ) {
	if ( !shader ) return;
	const float sx = ScaleX(), sy = ScaleY();
	re.SetColor( rgba );
	re.DrawStretchPic( x * sx, y * sy, w * sx, h * sy, s0, t0, s1, t1, shader );
	re.SetColor( nullptr );
}

/* A picture cut to a rectangle with its texture coordinates cut alike, which
 * is what forcescissor's scissor (0x004bd560) leaves of it. */
void ClippedPic( float x, float y, float w, float h, const float *rgba, qhandle_t shader, const float *clip ) {
	if ( !clip ) {
		VirtualPic( x, y, w, h, rgba, shader );
		return;
	}
	if ( w <= 0.0f || h <= 0.0f ) return;
	float x0 = x, y0 = y, x1 = x + w, y1 = y + h;
	float s0 = 0.0f, t0 = 0.0f, s1 = 1.0f, t1 = 1.0f;
	if ( x0 < clip[0] ) { s0 = ( clip[0] - x ) / w; x0 = clip[0]; }
	if ( y0 < clip[1] ) { t0 = ( clip[1] - y ) / h; y0 = clip[1]; }
	if ( x1 > clip[0] + clip[2] ) { s1 = ( clip[0] + clip[2] - x ) / w; x1 = clip[0] + clip[2]; }
	if ( y1 > clip[1] + clip[3] ) { t1 = ( clip[1] + clip[3] - y ) / h; y1 = clip[1] + clip[3]; }
	if ( x1 <= x0 || y1 <= y0 ) return;
	VirtualPic( x0, y0, x1 - x0, y1 - y0, rgba, shader, s0, t0, s1, t1 );
}

/* Draw3DBox (0x004b5b00) in a widget's border colours at its alpha. */
void Box3D( float x, float y, float w, float h, bool indent, const mw::BorderColor &border, float alpha ) {
	mw::BoxEdge edges[6];
	mw::Box3DEdges( x, y, w, h, indent, edges );
	for ( const mw::BoxEdge &edge : edges ) {
		const mw::Rgba &c = mw::ShadeColor( border, edge.shade );
		const float rgba[4] = { c.r, c.g, c.b, c.a * alpha };
		VirtualFill( edge.x, edge.y, edge.w, edge.h, rgba );
	}
}

/* A widget's font at its scale (the widget prints 0x004bd3c0, 0x004bd480
 * multiply both scales by fontscale). */
void WidgetPrint( const Widget &widget, const float *rgba, float x, float y, const char *text,
	float angle = 0.0f, float charSpacing = 0.0f, float lineSpacing = 0.0f, float scale = 1.0f ) {
	CL_AliceFontPrint( widget.font, rgba, false, x, y, text, -1, angle, charSpacing, lineSpacing,
		scale * widget.fontScale, scale * widget.fontScale );
}

void WidgetPrintJustified( const Widget &widget, const float *rgba, float x, float y, float w, float h,
	const char *text, float angle = 0.0f, float charSpacing = 0.0f, float lineSpacing = 0.0f, float scale = 1.0f ) {
	const font::Rect rect{ x, y, w, h };
	CL_AliceFontPrintJustified( widget.font, rgba, false, rect, widget.justifyH, widget.justifyV, text, angle,
		charSpacing, lineSpacing, scale * widget.fontScale, scale * widget.fontScale );
}

/* The font's height as a whole number (0x004c58d0), 16 without a font. */
int FontHeight( const Widget &widget ) {
	return font::LineHeight( CL_AliceFontMetrics( widget.font ) );
}

/* ---- reading the .urc files ---------------------------------------------- */

WidgetKind KindFor( const std::string &canonical ) {
	if ( canonical == "UIButton" ) return WidgetKind::Button;
	if ( canonical == "UICheckBox" ) return WidgetKind::CheckBox;
	if ( canonical == "UISlider" ) return WidgetKind::Slider;
	if ( canonical == "UIField" ) return WidgetKind::Field;
	if ( canonical == "UIList" ) return WidgetKind::List;
	if ( canonical == "UIFakkBindList" ) return WidgetKind::BindList;
	if ( canonical == "UIFAKKLoadGameClass" ) return WidgetKind::LoadGame;
	if ( canonical == "UIAutoScroll" ) return WidgetKind::AutoScroll;
	if ( canonical == "UIAlice3D" ) return WidgetKind::Alice3D;
	return WidgetKind::Label;
}

/* Keywords whose events change nothing this client draws; accepted quietly so
 * only a keyword no class knows is reported. */
bool IsQuietKeyword( const std::string &name ) {
	static const char *const kKeywords[] = {
		"allowactivate", "thumbshader", "leftshader", "rightshader", "loadfile", "stretch", "tileshader",
		"aliascache", "norescale", "noparentclip", "noparentadjust", "motiondelay", "stopsound",
		"bordercolor", "size", "align", "showcvar",
	};
	for ( const char *keyword : kKeywords ) {
		if ( urc::EqualsNoCase( name, keyword ) ) return true;
	}
	return false;
}

void ApplyWidgetProperty( Widget &widget, const urc::Property &property, const char *file ) {
	const std::string &key = property.name;
	const std::vector<std::string> &args = property.args;
	auto is = [&key]( const char *name ) { return urc::EqualsNoCase( key, name ); };
	const std::string first = args.empty() ? std::string() : args[0];

	if ( is( "name" ) ) { if ( !args.empty() ) widget.name = first; }
	else if ( is( "rect" ) ) {
		// Exactly four numbers (0x004b9220).
		if ( args.size() == 4 ) {
			widget.x = ToFloat( args[0] ); widget.y = ToFloat( args[1] );
			widget.w = ToFloat( args[2] ); widget.h = ToFloat( args[3] );
		}
	}
	else if ( is( "fgcolor" ) ) ReadColor( args, widget.fg );
	else if ( is( "bgcolor" ) ) {
		// setBackgroundColor( color, true ) (0x004b9fb0 -> 0x004bc700).
		if ( ReadColor( args, widget.bg ) ) widget.border3d = mw::MakeBorderColor( ToRgba( widget.bg ) );
	}
	else if ( is( "borderstyle" ) ) { if ( !args.empty() ) urc::ReadBorderStyle( first, widget.border ); }
	else if ( is( "title" ) ) widget.title = urc::Join( args );
	else if ( is( "font" ) ) { if ( !args.empty() ) widget.fontName = first; }
	else if ( is( "fontjustify" ) ) urc::ReadJustify( args, widget.justifyH, widget.justifyV );
	else if ( is( "fontscale" ) ) { if ( !args.empty() ) widget.fontScale = ToFloat( first ); }
	else if ( is( "fontspacing" ) ) {
		if ( args.size() > 0 ) widget.charSpacing = ToFloat( args[0] );
		if ( args.size() > 1 ) widget.lineSpacing = ToFloat( args[1] );
	}
	else if ( is( "fontangle" ) ) { if ( !args.empty() ) widget.fontAngle = ToFloat( first ); }
	else if ( is( "fontpos" ) ) {
		if ( args.size() >= 2 ) { widget.fontPosX = ToFloat( args[0] ); widget.fontPosY = ToFloat( args[1] ); }
	}
	else if ( is( "shader" ) ) { if ( !args.empty() ) widget.shader = first; }
	else if ( is( "hovershader" ) ) { if ( !args.empty() ) widget.hoverShader = first; }
	else if ( is( "staticshader" ) ) { if ( args.size() >= 2 ) widget.staticImage = args[1]; }
	else if ( is( "fadein" ) ) {
		if ( !args.empty() ) {
			widget.fadeIn = ToFloat( first );
			// A fade also times the widget's motion when nothing else does.
			if ( widget.motionTime == 0.0f ) widget.motionTime = widget.fadeIn;
		}
	}
	else if ( is( "inversealpha" ) ) widget.inverseAlpha = true;
	else if ( is( "direction" ) ) {
		if ( !args.empty() ) widget.direction = urc::DirectionFromName( first, true );
		if ( args.size() > 1 ) widget.motionTime = ToFloat( args[1] );
		if ( args.size() > 3 ) { widget.pointX = ToFloat( args[2] ); widget.pointY = ToFloat( args[3] ); }
	}
	else if ( is( "waitcommand" ) ) {
		if ( args.size() >= 2 ) widget.waits.push_back( { args[0], atoi( args[1].c_str() ), -1 } );
	}
	else if ( is( "stuffcommand" ) ) widget.stuffCommand = urc::Join( args );
	else if ( is( "mouseEnterCmd" ) ) widget.mouseEnterCommand = urc::Join( args );
	else if ( is( "mouseExitCmd" ) ) widget.mouseExitCommand = urc::Join( args );
	else if ( is( "clicksound" ) ) { if ( !args.empty() ) widget.clickSound = first; }
	else if ( is( "hoversound" ) ) { if ( !args.empty() ) widget.hoverSound = first; }
	else if ( is( "linkcvar" ) ) { if ( !args.empty() ) widget.linkCvar = first; }
	else if ( is( "groupid" ) ) { if ( !args.empty() ) widget.groupId = first; }
	else if ( is( "ordernumber" ) ) { if ( !args.empty() ) widget.orderNumber = atoi( first.c_str() ); }
	else if ( is( "keycommand" ) ) {
		if ( args.size() >= 2 ) widget.keyCommands.push_back( { KeyFromName( args[0] ), urc::Join( args, 1 ) } );
	}
	else if ( is( "disable" ) ) widget.enabled = false;
	else if ( is( "neverdraw" ) ) widget.neverDraw = true;
	else if ( is( "noinput" ) ) widget.noInput = true;
	else if ( is( "forcescissor" ) ) widget.forceScissor = true;
	else if ( is( "initdata" ) ) {
		// Written through the linked cvar as it stands (0x004b85c0).
		if ( !widget.linkCvar.empty() ) { widget.hasInitData = true; widget.initData = first; }
	}
	/* UICheckBox (0x004e03a0, 0x004e04c0, 0x004e0850, 0x004e09a0). */
	else if ( is( "checked_command" ) ) widget.check.checkedCommand = first;
	else if ( is( "unchecked_command" ) ) widget.check.uncheckedCommand = first;
	else if ( is( "checked_shader" ) ) widget.check.checkedShader = first;
	else if ( is( "unchecked_shader" ) ) widget.check.uncheckedShader = first;
	/* UISlider (0x004da530, 0x004da660, 0x004da5f0). */
	else if ( is( "setrange" ) ) {
		widget.slider.s.min = args.size() > 0 ? ToFloat( args[0] ) : 0.0f;
		widget.slider.s.max = args.size() > 1 ? ToFloat( args[1] ) : 0.0f;
	}
	else if ( is( "slidertype" ) ) mw::SetSliderType( widget.slider.s, first );
	else if ( is( "stepsize" ) ) widget.slider.s.step = args.empty() ? 0.0f : ToFloat( first );
	/* UIList (0x004de170). */
	else if ( is( "additem" ) ) {
		if ( !args.empty() ) widget.list.items.push_back( { first, args.size() > 1 ? args[1] : std::string() } );
	}
	/* UIFakkBindList (0x004e3990): read once the menu is read. */
	else if ( is( "filename" ) ) widget.bind.file = first;
	/* UIAutoScroll (0x004c0540, 0x004c0700, 0x004c0e80, 0x004c0a30). */
	else if ( is( "addline" ) ) widget.scroll.lines.push_back( first );
	else if ( is( "addimage" ) ) {
		if ( args.size() >= 5 ) {
			mw::ScrollImage image;
			image.shader = args[0];
			image.x = ToFloat( args[1] );
			image.line = static_cast<float>( widget.scroll.lines.size() ) + ToFloat( args[2] );
			image.w = ToFloat( args[3] );
			image.h = ToFloat( args[4] );
			image.fade = !widget.scroll.lines.empty();
			widget.scroll.images.push_back( std::move( image ) );
		}
	}
	else if ( is( "scrollspeed" ) ) { if ( !args.empty() ) widget.scroll.speed = ToFloat( first ); }
	else if ( is( "clearlist" ) ) widget.scroll.lines.clear();
	else if ( !IsQuietKeyword( key ) ) {
		Com_DPrintf( "UI: Invalid token '%s' on line %d of %s\n", key.c_str(), property.line, file );
	}
}

/* Align flags (agentB): left 1, right 2, top 8, bottom 0x10, centre x 0x20,
 * centre y 0x40. */
int ReadAlign( const std::vector<std::string> &args ) {
	int flags = 0;
	for ( const std::string &word : args ) {
		if ( urc::EqualsNoCase( word, "left" ) ) flags |= 1;
		else if ( urc::EqualsNoCase( word, "right" ) ) flags |= 2;
		else if ( urc::EqualsNoCase( word, "top" ) ) flags |= 8;
		else if ( urc::EqualsNoCase( word, "bottom" ) ) flags |= 0x10;
		else if ( urc::EqualsNoCase( word, "centerx" ) ) flags |= 0x20;
		else if ( urc::EqualsNoCase( word, "centery" ) ) flags |= 0x40;
	}
	return flags;
}

void ApplyMenuEvent( Menu &menu, const urc::Property &event ) {
	const std::vector<std::string> &args = event.args;
	auto is = [&event]( const char *name ) { return urc::EqualsNoCase( event.name, name ); };
	if ( is( "bgcolor" ) ) {
		if ( ReadColor( args, menu.bg ) ) menu.border3d = mw::MakeBorderColor( ToRgba( menu.bg ) );
	}
	else if ( is( "borderstyle" ) ) { if ( !args.empty() ) urc::ReadBorderStyle( args[0], menu.border ); }
	else if ( is( "bgfill" ) ) menu.hasBgFill = ReadColor( args, menu.bgFill );
	else if ( is( "fullscreen" ) ) menu.fullscreen = !args.empty() && atoi( args[0].c_str() ) != 0;
	else if ( is( "fadeoverlay" ) ) { if ( !args.empty() ) menu.fadeOverlay = ToFloat( args[0] ); }
	else if ( is( "align" ) ) menu.align = ReadAlign( args );
	else if ( is( "showcommand" ) ) menu.showCommands.push_back( urc::Join( args ) );
	else if ( is( "hidecommand" ) ) menu.hideCommands.push_back( urc::Join( args ) );
	else if ( is( "keycommand" ) ) {
		if ( args.size() >= 2 ) menu.keyCommands.push_back( { KeyFromName( args[0] ), urc::Join( args, 1 ) } );
	}
	// Other layout events (allowactivate, virtualres) change nothing drawn.
}

void BuildBindList( Menu &menu, int index );

void LoadMenuFile( const char *path ) {
	void *buffer = nullptr;
	const int length = FS_ReadFile( path, &buffer );
	if ( length < 0 || !buffer ) {
		Com_Printf( "UI: couldn't read %s\n", path );
		return;
	}
	const urc::IncludeReader include = []( const std::string &name, std::string &text ) {
		void *data = nullptr;
		const int bytes = FS_ReadFile( name.c_str(), &data );
		if ( bytes < 0 || !data ) return false;
		text.assign( static_cast<const char *>( data ), static_cast<std::size_t>( bytes ) );
		FS_FreeFile( data );
		return true;
	};
	const urc::Layout layout = urc::Parse( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ),
		include );
	FS_FreeFile( buffer );
	for ( const urc::Diagnostic &diagnostic : layout.diagnostics ) {
		Com_DPrintf( "UI: %s on line %d of %s\n", diagnostic.message.c_str(), diagnostic.line, path );
	}
	if ( layout.diagnosticsDropped ) {
		Com_DPrintf( "UI: %d more problems in %s\n", static_cast<int>( layout.diagnosticsDropped ), path );
	}
	for ( const urc::Menu &source : layout.menus ) {
		urc::MenuHeader header;
		if ( !urc::ReadMenuHeader( source.header, header ) ) {
			Com_Printf( "UI: a menu in %s names no menu\n", path );
			continue;
		}
		Menu menu;
		menu.name = header.name;
		menu.file = path;
		menu.w = header.width;
		menu.h = header.height;
		menu.direction = header.direction;
		menu.motionTime = header.hasMotionTime ? header.motionTime : 0.0f;
		for ( const urc::Property &event : source.events ) ApplyMenuEvent( menu, event );
		for ( const urc::Widget &sourceWidget : source.widgets ) {
			const std::string canonical = urc::CanonicalClass( sourceWidget.classToken );
			if ( canonical.empty() ) {
				Com_Printf( "Tried to instantiate a widget that doesn't exist: %s\n", sourceWidget.classToken.c_str() );
				continue;
			}
			Widget widget;
			widget.kind = KindFor( canonical );
			for ( const urc::Property &property : sourceWidget.properties ) {
				ApplyWidgetProperty( widget, property, path );
			}
			widget.font = CL_AliceFontRegister( widget.fontName.c_str() );
			menu.widgets.push_back( std::move( widget ) );
		}
		menus.push_back( std::move( menu ) );
		/* The bind file is read where its `filename` line is (0x004e3990),
		 * after the `alice3d` widget controls.urc declares above it. */
		Menu &added = menus.back();
		for ( std::size_t i = 0; i < added.widgets.size(); ++i ) {
			if ( added.widgets[i].kind == WidgetKind::BindList ) BuildBindList( added, static_cast<int>( i ) );
		}
	}
}

void LoadMenus() {
	int count = 0;
	char **files = FS_ListFiles( "ui", ".urc", &count );
	std::vector<std::string> names;
	for ( int i = 0; i < count; ++i ) names.emplace_back( files[i] );
	FS_FreeFileList( files );
	std::sort( names.begin(), names.end(), []( const std::string &a, const std::string &b ) {
		return Q_stricmp( a.c_str(), b.c_str() ) < 0;
	} );
	for ( const std::string &name : names ) {
		LoadMenuFile( va( "ui/%s", name.c_str() ) );
	}
	/* initdata writes its cvar as retail reads the file, which it does once;
	 * this client reads the files again whenever its UI restarts. */
	if ( !initDataApplied ) {
		initDataApplied = true;
		for ( const Menu &menu : menus ) {
			for ( const Widget &widget : menu.widgets ) {
				if ( widget.hasInitData ) UiCvarSet( widget.linkCvar, widget.initData.c_str() );
			}
		}
	}
	Com_DPrintf( "Alice UI: %i menus from %i files\n", static_cast<int>( menus.size() ), static_cast<int>( names.size() ) );
}

/* keybind.scr (UI_LoadTransTable 0x004d1810) and maplist.scr's titles for the
 * load/save menu (0x0040b950 over UI_LoadMapList 0x0040b5a0), read as the UI
 * starts (UI_Init 0x00410940..0x0041094f). */
void LoadTables() {
	void *buffer = nullptr;
	int length = FS_ReadFile( "ui/keybind.scr", &buffer );
	keyAliases.clear();
	if ( length >= 0 && buffer ) {
		std::string error;
		keyAliases = mw::ParseKeyAliases( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ), &error );
		if ( !error.empty() ) Com_Printf( "%s\n", error.c_str() );
		FS_FreeFile( buffer );
	}
	buffer = nullptr;
	length = FS_ReadFile( "ui/maplist.scr", &buffer );
	mapTitles = screen::MapList();
	if ( length >= 0 && buffer ) {
		mapTitles = screen::ParseMapList( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ) );
		FS_FreeFile( buffer );
	}
}

/* ---- motion and fade (0x004bb990, 0x004bbb10) ----------------------------- */

float MenuOriginX( const Menu &menu ) {
	if ( menu.align & 2 ) return 640.0f - menu.w;
	if ( menu.align & 0x20 ) return ( 640.0f - menu.w ) * 0.5f;
	return 0.0f;
}

float MenuOriginY( const Menu &menu ) {
	if ( menu.align & 0x10 ) return 480.0f - menu.h;
	if ( menu.align & 0x40 ) return ( 480.0f - menu.h ) * 0.5f;
	return 0.0f;
}

int MotionMs( float seconds ) { return static_cast<int>( seconds * 1000.0f ); }

int LongestMotionMs( const Menu &menu ) {
	int longest = MotionMs( menu.motionTime );
	for ( const Widget &widget : menu.widgets ) longest = std::max( longest, MotionMs( widget.motionTime ) );
	return longest;
}

/* How far into its motion a widget is, 0 at its start, 1 at its end. */
float MotionFraction( const Widget &widget, float seconds ) {
	if ( seconds <= 0.0f ) return 1.0f;
	float frac = ( Now() - widget.motionStart ) / ( seconds * 1000.0f );
	return frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac;
}

/* The widget's position: home, or on its way between home and the edge it
 * enters from. Retail measures the far edges in the real screen's size while
 * the widget is in 640x480 units; kept. */
void WidgetPosition( const Menu &menu, const Widget &widget, float &x, float &y ) {
	const float originX = MenuOriginX( menu ), originY = MenuOriginY( menu );
	x = originX + widget.x;
	y = originY + widget.y;
	if ( widget.direction == urc::Direction::None || widget.motion == Motion::None || widget.motionTime <= 0.0f ) {
		return;
	}
	float offX = x, offY = y;
	switch ( widget.direction ) {
	case urc::Direction::FromLeft: offX = -( widget.w + originX ); break;
	case urc::Direction::FromRight: offX = static_cast<float>( cls.glconfig.vidWidth ); break;
	case urc::Direction::FromBottom: offY = static_cast<float>( cls.glconfig.vidHeight ); break;
	case urc::Direction::FromTop: offY = -( widget.h + originY ); break;
	case urc::Direction::FromPoint: offX = x + widget.pointX; offY = y + widget.pointY; break;
	default: break;
	}
	float frac = MotionFraction( widget, widget.motionTime );
	if ( widget.motion == Motion::Out ) frac = 1.0f - frac;
	x = offX + ( x - offX ) * frac;
	y = offY + ( y - offY ) * frac;
}

float WidgetFade( const Widget &widget ) {
	float alpha = 1.0f;
	if ( widget.fadeIn > 0.0f && widget.motion != Motion::None ) {
		alpha = MotionFraction( widget, widget.fadeIn );
		if ( widget.motion == Motion::Out ) alpha = 1.0f - alpha;
	}
	if ( widget.inverseAlpha ) alpha = 1.0f - alpha;
	return alpha;
}

/* ---- widget classes ------------------------------------------------------ */

/* Each class's two refreshes: UpdateUIElement (vtable slot 15) reads what the
 * widget shows from its cvar, UpdateData (slot 14) writes it -- except that a
 * checkbox's and a field's UpdateData read. A menu's show runs both, in that
 * order, on each enabled widget (0x004c4d97..0x004c4dae), and several classes
 * run one of them from their own draw. */

/* UICheckBox::UpdateCvar (0x004e05e0): the cvar as 0 or 1, then the stuff
 * command with " 1\n" or " 0\n" after it. */
void CheckBoxWrite( Widget &widget ) {
	if ( !widget.linkCvar.empty() ) UiCvarSet( widget.linkCvar, widget.check.checked ? "1" : "0" );
	if ( !widget.stuffCommand.empty() ) {
		Cbuf_AddText( ( widget.stuffCommand + mw::CheckCommandSuffix( widget.check.checked ) ).c_str() );
	}
}

/* UISlider::UpdateData (0x004db120): the cvar, then the stuff command with
 * the value after a space. */
void SliderWrite( Widget &widget ) {
	if ( !widget.linkCvar.empty() ) UiCvarSet( widget.linkCvar, mw::SliderCvarText( widget.slider.s ).c_str() );
	if ( !widget.stuffCommand.empty() ) {
		Cbuf_AddText( ( widget.stuffCommand + " " + mw::SliderCommandValue( widget.slider.s ) + "\n" ).c_str() );
	}
}

/* UIList::UpdateData (0x004decf0): the item's value to the cvar and to the
 * stuff command. */
void ListWrite( Widget &widget ) {
	const int current = widget.list.current;
	if ( current <= 0 || current > static_cast<int>( widget.list.items.size() ) ) return;
	const std::string &value = widget.list.items[current - 1].value;
	if ( !widget.linkCvar.empty() ) UiCvarSet( widget.linkCvar, value.c_str() );
	if ( !widget.stuffCommand.empty() ) Cbuf_AddText( ( widget.stuffCommand + " " + value + "\n" ).c_str() );
}

void RefreshSaves( Menu &menu, Widget &widget );

void UpdateUIElement( Menu &menu, int index ) {
	Widget &widget = menu.widgets[index];
	switch ( widget.kind ) {
	case WidgetKind::Slider:
		// 0x004db3f0
		if ( !widget.linkCvar.empty() ) widget.slider.s.value = UiCvarFloat( widget.linkCvar.c_str(), 0.0f );
		break;
	case WidgetKind::List:
		// 0x004df0e0: a cvar that does not exist yet takes the first item.
		if ( !widget.linkCvar.empty() ) {
			if ( !CvarExists( widget.linkCvar.c_str() ) ) {
				widget.list.current = 1;
				ListWrite( widget );
			} else {
				const int found = mw::ListFind( widget.list.items, UiCvarString( widget.linkCvar.c_str() ).c_str() );
				if ( found ) widget.list.current = found;
			}
		}
		break;
	case WidgetKind::LoadGame:
		// 0x0041a150: the list takes the keys and reads the saves again.
		menu.focus = index;
		RefreshSaves( menu, widget );
		break;
	default:
		break;
	}
}

void UpdateData( Menu &menu, int index ) {
	Widget &widget = menu.widgets[index];
	switch ( widget.kind ) {
	case WidgetKind::CheckBox:
		// 0x004e0cc0
		if ( !widget.linkCvar.empty() ) widget.check.checked = UiCvarInt( widget.linkCvar.c_str(), 0 ) != 0;
		break;
	case WidgetKind::Slider:
		SliderWrite( widget );
		break;
	case WidgetKind::Field:
		// 0x004dff30
		if ( !widget.linkCvar.empty() ) {
			widget.field.text = UiCvarString( widget.linkCvar.c_str() ).substr( 0, mw::FieldMaxLength );
			widget.field.Clamp();
		}
		break;
	case WidgetKind::List:
		ListWrite( widget );
		break;
	default:
		break;
	}
}

/* ---- UIAlice3D ----------------------------------------------------------- */

struct ModelHandles {
	qhandle_t handle = 0;
	int tiki = -1;
};

ModelHandles RegisterAliceModel( const std::string &name ) {
	ModelHandles out;
	if ( name.empty() ) return out;
	out.handle = re.RegisterModel( name.c_str() );
	out.tiki = out.handle && re.GetAliceTikiHandle ? re.GetAliceTikiHandle( out.handle ) : -1;
	return out;
}

/* UIWidget3D's animation change (0x004d27c0): the old frame is kept for the
 * cross-blend, the turn starts over from where it was, and the new
 * animation's length is looked up in the model. */
void SetModelAnim( ModelState &model, const std::string &anim, bool loop, float yaw ) {
	model.blend = 0.0f;
	model.previousFrame = static_cast<int>( model.frame - 1.0f > 0.0f ? model.frame - 1.0f : 0.0f );
	model.frame = 0.0f;
	model.loop = loop;
	model.previousYaw = model.yaw;
	model.yaw = yaw;
	model.previous = model.anim;
	model.anim = anim;
	const ModelHandles handles = RegisterAliceModel( model.model );
	const int animNum = handles.tiki >= 0 ? AliceTiki_AnimNumForName( handles.tiki, anim.c_str() ) : -1;
	model.numFrames = animNum >= 0 ? AliceTiki_AnimNumFrames( handles.tiki, animNum ) : 0;
}

/* The idle UIAlice3D goes back to when an animation that does not loop ends
 * (0x0052bfb8, one entry). */
const char *const kAliceIdles[] = { "idle_stand_rocktoes" };
int aliceIdle;
/* The model's eyes close for 4 frames in every 100 (0x004d32a9, 0x00aa79bc). */
int aliceBlink;

const noise::Table &NoiseTable() {
	static const noise::Table table = noise::Build();
	return table;
}

/* UIAlice3D::Draw (0x004d3310): the model in a view of the whole screen with a
 * 30 degree field of view, turned towards its bind's angle over 250 ms, lit
 * by a steady light and a flickering one whose peaks bring thunder. */
void DrawAlice3D( Widget &widget ) {
	ModelState &model = widget.model;
	if ( model.model.empty() ) return;
	const ModelHandles handles = RegisterAliceModel( model.model );
	if ( !handles.handle ) return;
	const int now = Now();
	const float yaw = ( 1.0f - model.blend ) * model.previousYaw + model.yaw * model.blend + 180.0f;

	vec3_t mins, maxs;
	re.ModelBounds( handles.handle, mins, maxs );
	const float centreY = ( maxs[1] + mins[1] ) * 0.5f;
	const float centreZ = ( maxs[2] + mins[2] ) * -0.5f;
	// The largest extent, taking the later axis on a tie (0x004d33d8..0x004d3416).
	float extent = maxs[1] - mins[1];
	if ( !( extent > maxs[2] - mins[2] ) ) extent = maxs[2] - mins[2];
	if ( !( extent > maxs[0] - mins[0] ) ) extent = maxs[0] - mins[0];
	const float distance = extent * model.scale * 1.8656716f;   // 0x004ff45c

	// The animation runs at 20 frames a second (0.02 a millisecond, 0x00500a2c).
	model.frame += cls.frametime * 0.02f;
	if ( !( model.frame < static_cast<float>( model.numFrames ) ) ) {
		if ( model.loop ) {
			model.frame = 0.0f;
		} else {
			aliceIdle = ( aliceIdle + 1 ) % static_cast<int>( std::size( kAliceIdles ) );
			SetModelAnim( model, kAliceIdles[aliceIdle], true, model.yaw );
		}
	}

	refdef_t view{};
	view.width = cls.glconfig.vidWidth;
	view.height = cls.glconfig.vidHeight;
	view.fov_x = view.fov_y = 30.0f;
	AxisClear( view.viewaxis );
	view.time = now;
	view.rdflags = RDF_NOWORLDMODEL;
	/* Unlike UI_Render3DModel (0x004083e0), this view does not read
	 * ui_skipDepthClear: its refdef is RDF_NOWORLDMODEL alone (0x004d3869). */
	view.alice.keepDepth = qfalse;

	re.ClearScene();
	// The steady light (0x004d34bd..0x004d353a).
	const vec3_t steady = { distance + 250.0f, centreY - 300.0f, centreZ - 200.0f };
	re.AddLightToScene( steady, static_cast<float>( std::sin( now * 0.00314159 ) * 300.0 + 1000.0 ),
		0.972f, 0.972f, 0.824f );
	// The lightning: renderer noise over time, with thunder on its peaks at
	// most every five seconds (0x004d3540..0x004d3732).
	const float lightning = noise::Get4f( NoiseTable(), 0.0f, 0.0f, 0.0f,
		static_cast<float>( now * 0.001 * 3.0 ) ) * 4000.0f;
	if ( lightning > 2000.0f && model.thunderWait <= 0 ) {
		PlayLocalSound( va( "sound/ui/thunder_%d.wav", model.thunder + 1 ) );
		model.thunderWait = 5000;
		model.thunder = ( model.thunder + 1 ) % 3;
	}
	model.thunderWait -= cls.frametime;
	const vec3_t flicker = { distance - 250.0f, centreY + 300.0f, centreZ - 200.0f };
	if ( lightning > 0.0f ) re.AddLightToScene( flicker, lightning, 0.972f, 0.972f, 0.824f );

	// The entity (0x004d2fc0): the origin moved by (0, -29, -43).
	refEntity_t entity{};
	entity.reType = RT_MODEL;
	entity.hModel = handles.handle;
	const vec3_t angles = { 0.0f, yaw, 0.0f };
	AnglesToAxis( angles, entity.axis );
	entity.origin[0] = distance;
	entity.origin[1] = centreY - 29.0f;
	entity.origin[2] = centreZ - 43.0f;
	VectorCopy( entity.origin, entity.oldorigin );
	VectorCopy( entity.origin, entity.lightingOrigin );
	aliceRenderEntity_t &alice = entity.alice;
	const int animNum = handles.tiki >= 0 ? AliceTiki_AnimNumForName( handles.tiki, model.anim.c_str() ) : -1;
	alice.anim = alice.oldanim = animNum > 0 ? animNum : 0;
	alice.frame = static_cast<int>( model.frame );
	alice.oldframe = static_cast<int>( model.frame - 1.0f > 0.0f ? model.frame - 1.0f : 0.0f );
	alice.backlerp = 1.0f - ( model.frame - static_cast<float>( static_cast<int>( model.frame ) ) );
	alice.crossblendLerp = -1.0f;
	if ( !model.previous.empty() && handles.tiki >= 0 && model.blend < 1.0f ) {
		const int previous = AliceTiki_AnimNumForName( handles.tiki, model.previous.c_str() );
		if ( previous >= 0 && model.previousFrame < AliceTiki_AnimNumFrames( handles.tiki, previous ) ) {
			alice.crossblendAnim = previous;
			alice.crossblendFrame = model.previousFrame;
			alice.crossblendLerp = model.blend;
		}
	}
	alice.torsoCrossblendLerp = 0.0f;
	alice.useLegs = qtrue;
	alice.scale = 1.0f;
	if ( handles.tiki >= 0 ) {
		const int eyes = AliceTiki_SurfaceNameToNum( handles.tiki, "material1" );
		aliceBlink = aliceBlink + 1 > 100 ? 0 : aliceBlink + 1;
		if ( eyes >= 0 && eyes < static_cast<int>( sizeof( alice.surfaces ) ) && aliceBlink % 100 < 4 ) {
			alice.surfaces[eyes] |= 1;
		}
	}
	re.AddRefEntityToScene( &entity, qfalse );
	re.RenderScene( &view );

	// The turn finishes over 250 ms (0.02 * 0.2 a millisecond).
	if ( model.blend < 1.0f ) {
		model.blend += cls.frametime * 0.02f * 0.2f;
		if ( model.blend > 1.0f ) model.blend = 1.0f;
	}
}

/* ---- UIFakkBindList ------------------------------------------------------ */

/* The first two keys bound to a command, compared without regard to case
 * (Key_GetKeysForCommand 0x00429450, which looks at the 256 keys). */
void KeysForCommand( const std::string &command, int &first, int &second ) {
	first = second = -1;
	for ( int key = 0; key < 256; ++key ) {
		const char *binding = Key_GetBinding( key );
		if ( !binding || !binding[0] || Q_stricmp( binding, command.c_str() ) ) continue;
		if ( first < 0 ) {
			first = key;
		} else {
			second = key;
			return;
		}
	}
}

bool BoundTo( int key, const std::string &command ) {
	const char *binding = key >= 0 ? Key_GetBinding( key ) : nullptr;
	return binding && !Q_stricmp( binding, command.c_str() );
}

/* The window manager's key table for a row, refreshed and checked as its key
 * name is made (0x004b4da0): looked up again while either key is missing, and
 * a key no longer bound to the command dropped. */
void RowKeys( BindState &bind, int row ) {
	const std::string &command = bind.layout.items[row].command;
	int &first = bind.key1[row];
	int &second = bind.key2[row];
	if ( first < 0 || second < 0 ) KeysForCommand( command, first, second );
	if ( second >= 0 && !BoundTo( second, command ) ) second = -1;
	if ( first >= 0 && !BoundTo( first, command ) ) {
		first = second;
		second = -1;
	}
}

/* UIWindowManager::BindKeyToCommand for the first key (0x004b52d0): the old
 * key loses the command, and so does the second key when it is the new one. */
void BindRowKey( BindState &bind, int row, int key ) {
	RowKeys( bind, row );
	const std::string &command = bind.layout.items[row].command;
	int &first = bind.key1[row];
	int &second = bind.key2[row];
	if ( first == key ) return;
	if ( first >= 0 ) Key_SetBinding( first, "" );
	if ( second == key ) {
		Key_SetBinding( second, "" );
		second = -1;
	}
	first = key;
	Key_SetBinding( key, command.c_str() );
}

/* A key button's clear (0x004d1700): the first key loses the command. */
void ClearRowKey( BindState &bind, int row ) {
	RowKeys( bind, row );
	if ( bind.key1[row] > 0 ) Key_SetBinding( bind.key1[row], "" );
}

void BuildBindList( Menu &menu, int index ) {
	Widget &widget = menu.widgets[index];
	BindState &bind = widget.bind;
	if ( bind.file.empty() ) return;
	void *buffer = nullptr;
	const int length = FS_ReadFile( bind.file.c_str(), &buffer );
	if ( length < 0 || !buffer ) {
		Com_Printf( "UI: couldn't read %s\n", bind.file.c_str() );
		return;
	}
	bind.layout = mw::ParseBindScript( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ) );
	FS_FreeFile( buffer );
	const std::size_t rows = bind.layout.items.size();
	bind.key1.assign( rows, -1 );
	bind.key2.assign( rows, -1 );
	bind.page = mw::BindPageSize( widget.h, bind.layout.commandHeight );
	bind.built = true;
	/* The menu's model, which the rows' animations play on (0x004e3c60..
	 * 0x004e3d47): Alice, at half scale, standing idle. */
	bind.model = FindWidgetIndex( menu, "alice3d" );
	if ( bind.model >= 0 ) {
		ModelState &model = menu.widgets[bind.model].model;
		model.scale = 0.5f;
		model.model = "models/alice.tik";
		SetModelAnim( model, "idle_base_01", false, 0.0f );
	}
}

/* The row's animation on the model, once a row has been chosen for 200 ms
 * (0x004e42c0). */
void PlayRowAnim( Menu &menu, Widget &widget ) {
	const BindState &bind = widget.bind;
	if ( bind.model < 0 || bind.row < 0 || bind.row >= static_cast<int>( bind.layout.items.size() ) ) return;
	const mw::BindItem &item = bind.layout.items[bind.row];
	if ( item.anim.empty() ) return;
	SetModelAnim( menu.widgets[bind.model].model, item.anim, item.loop, item.rotation );
}

/* The press-any-key picture while a key is awaited, centred on the screen at
 * its own size (0x004e4690). */
void DrawPressAnyKey( const Widget &widget ) {
	if ( !widget.bind.capturing ) return;
	const qhandle_t shader = CL_AliceShader( "ui/control/press_any_key.tga" );
	int w = 0, h = 0;
	CL_AliceShaderSize( shader, &w, &h );
	static const float white[4] = { 1, 1, 1, 1 };
	VirtualPic( ( 640.0f - w ) * 0.5f, ( 480.0f - h ) * 0.5f, static_cast<float>( w ), static_cast<float>( h ), white,
		shader );
}

int captureMenu = -1, captureWidget = -1;

void StartCapture( Menu &menu, int index ) {
	Widget &widget = menu.widgets[index];
	widget.bind.capturing = true;
	captureMenu = MenuIndex( menu );
	captureWidget = index;
	PlayLocalSound( widget.bind.layout.activeSound );
}

void EndCapture() {
	if ( captureMenu >= 0 && captureMenu < static_cast<int>( menus.size() )
		&& captureWidget >= 0 && captureWidget < static_cast<int>( menus[captureMenu].widgets.size() ) ) {
		menus[captureMenu].widgets[captureWidget].bind.capturing = false;
	}
	captureMenu = captureWidget = -1;
}

bool Capturing() {
	return captureMenu >= 0 && captureMenu < static_cast<int>( menus.size() ) && captureWidget >= 0
		&& captureWidget < static_cast<int>( menus[captureMenu].widgets.size() )
		&& menus[captureMenu].widgets[captureWidget].bind.capturing;
}

/* The key the chosen row's button waits for (UIBindButton::KeyEvent
 * 0x004d1650): any key but Escape takes the command; either way the wait
 * ends with the enter sound. */
void CaptureKey( int key ) {
	Widget &widget = menus[captureMenu].widgets[captureWidget];
	BindState &bind = widget.bind;
	if ( key != K_ESCAPE && key != 0 && bind.row >= 0 && bind.row < static_cast<int>( bind.layout.items.size() ) ) {
		BindRowKey( bind, bind.row, key );
	}
	EndCapture();
	PlayLocalSound( bind.layout.enterSound );
}

/* UIVertScroll's position (0x004d8b70); false when it did not move. */
bool ScrollTo( BindState &bind, int value ) {
	const int clamped = mw::ClampScroll( value, static_cast<int>( bind.layout.items.size() ), bind.page );
	if ( clamped == bind.top ) return false;
	bind.top = clamped;
	return true;
}

/* UIFakkBindList::KeyEvent (0x004e4330). */
bool BindListKey( Menu &menu, int index, int key ) {
	Widget &widget = menu.widgets[index];
	BindState &bind = widget.bind;
	const int count = static_cast<int>( bind.layout.items.size() );
	if ( !bind.built || !count ) return false;
	const std::string &change = bind.layout.changeSound;
	switch ( key ) {
	case K_ENTER:
		// The chosen cell's key button, or the name's, waits for a key.
		StartCapture( menu, index );
		return true;
	case K_BACKSPACE:
	case K_DEL:
		if ( bind.column == 2 ) {
			ClearRowKey( bind, bind.row );
			PlayLocalSound( bind.layout.activeSound );
		}
		return true;
	case K_MWHEELUP:
		ScrollTo( bind, bind.top - 1 );
		PlayLocalSound( change );
		return true;
	case K_MWHEELDOWN:
		ScrollTo( bind, bind.top + 1 );
		PlayLocalSound( change );
		return true;
	case K_RIGHTARROW:
		bind.column = bind.column + 1 > 2 ? 2 : bind.column + 1;
		PlayLocalSound( change );
		return true;
	case K_LEFTARROW:
		bind.column = bind.column - 1 < 1 ? 1 : bind.column - 1;
		PlayLocalSound( change );
		return true;
	case K_UPARROW:
		if ( bind.row == 0 ) return true;
		if ( bind.row == bind.top ) {
			ScrollTo( bind, bind.top - 1 );
			bind.row = bind.top;
		} else {
			--bind.row;
		}
		PlayLocalSound( change );
		return true;
	case K_DOWNARROW:
		if ( bind.row + 1 >= count ) return true;
		if ( bind.row - bind.top + 1 >= bind.page ) {
			if ( ScrollTo( bind, bind.row + 1 ) ) ++bind.row;
			return true;
		}
		++bind.row;
		PlayLocalSound( change );
		return true;
	default:
		return false;
	}
}

/* ---- UIFAKKLoadGameClass ------------------------------------------------- */

/* The archive path of one of a save's files, or empty. */
std::string SavePath( const std::string &name, const char *extension ) {
	char path[alicesave::PathBytes];
	return SV_AliceArchiveFileName( name.c_str(), extension, path, sizeof( path ) ) ? path : std::string();
}

/* The map's title as maplist.scr gives it, else its name (0x0040b950). */
std::string MapTitle( const std::string &map ) {
	const int found = mapTitles.Find( map.c_str() );
	return found >= 0 ? mapTitles.entries[found].title : map;
}

std::string SaveDate( int32_t saveTime ) {
	const std::time_t when = static_cast<std::time_t>( saveTime );
	const std::tm *local = std::localtime( &when );
	return local ? mw::CTimeText( *local ) : std::string();
}

void SetWidgetShown( Menu &menu, const char *name, bool enable ) {
	if ( Widget *widget = FindWidget( menu, name ) ) widget->enabled = widget->shown = enable;
}

/* The select handler (0x0041af20): the page's six pictures and buttons, the
 * page number, the chosen save's picture, map and date, and the disk-full
 * message put away when no save has failed. */
void ShowSaves( Menu &menu, Widget &list ) {
	mw::SavePage &page = list.load.page;
	const int count = static_cast<int>( list.load.rows.size() );
	page.Refreshed( count );
	if ( Widget *pageNum = FindWidget( menu, "PageNum" ) ) pageNum->title = va( "%i", page.page + 1 );
	for ( int slot = 1; slot <= mw::ShotsPerPage; ++slot ) {
		const int index = page.IndexAt( slot );
		const bool used = count > 0 && index <= count;
		std::string shot = "ui/load/bigDefault.tga";   // 0x0050d6ec
		if ( used ) {
			const std::string path = SavePath( list.load.rows[index - 1].name, alicesave::ShotExtension );
			/* A save whose picture is not there yet -- this client takes it a
			 * frame after the save -- shows the default. */
			if ( !path.empty() && FS_ReadFile( path.c_str(), nullptr ) > 0 ) shot = path;
		}
		if ( Widget *picture = FindWidget( menu, va( "LoadSaveShot%i", slot ) ) ) picture->staticImage = shot;
		if ( Widget *button = FindWidget( menu, va( "LoadButton%i", slot ) ) ) {
			button->shader = slot == page.tab ? "ui/load/pushbtn_med_lit" : "ui/load/pushbtn_med";
		}
		if ( slot != page.tab ) continue;
		if ( Widget *big = FindWidget( menu, "BigSaveShot" ) ) big->staticImage = shot;
		if ( Widget *map = FindWidget( menu, "MapName" ) ) map->title = used ? MapTitle( list.load.rows[index - 1].map ) : "";
		if ( Widget *date = FindWidget( menu, "DateTime" ) ) {
			date->title = used ? SaveDate( list.load.rows[index - 1].saveTime ) : "";
		}
	}
	if ( SV_AliceSaveError( qfalse ) == 0 ) {
		SetWidgetShown( menu, "MsgBackground", false );
		SetWidgetShown( menu, "DiskFull", false );
	}
}

/* The refresh (0x0041a210): every save's header, `quicksave` left out, oldest
 * first. */
void RefreshSaves( Menu &menu, Widget &list ) {
	list.load.rows.clear();
	const std::string pattern = SavePath( "*", alicesave::ServerExtension );
	const std::size_t star = pattern.find( '*' );
	const std::size_t slash = pattern.rfind( '/' );
	if ( star != std::string::npos && slash != std::string::npos && slash < star ) {
		const std::string directory = pattern.substr( 0, slash );
		const std::string prefix = pattern.substr( slash + 1, star - slash - 1 );
		int count = 0;
		char **files = FS_ListFiles( directory.c_str(), va( ".%s", alicesave::ServerExtension ), &count );
		for ( int i = 0; i < count; ++i ) {
			if ( Q_stricmpn( files[i], prefix.c_str(), static_cast<int>( prefix.size() ) ) ) continue;
			fileHandle_t file = 0;
			if ( FS_FOpenFileRead( va( "%s/%s", directory.c_str(), files[i] ), &file, qfalse ) < 0 || !file ) continue;
			uint8_t bytes[alicesave::HeaderBytes];
			const int read = FS_Read( bytes, sizeof( bytes ), file );
			FS_FCloseFile( file );
			if ( read != static_cast<int>( sizeof( bytes ) ) ) continue;
			alicesave::ServerHeader header;
			alicesave::Decode( bytes, header );
			if ( !mw::ListsSave( header.saveName ) ) continue;
			mw::SaveRow row;
			row.map = header.mapName;
			row.name = header.saveName;
			row.saveTime = header.saveTime;
			row.levelTime = header.levelTime;
			list.load.rows.push_back( std::move( row ) );
		}
		FS_FreeFileList( files );
	}
	mw::SortSaves( list.load.rows );
	ShowSaves( menu, list );
}

/* CheckErrorState (0x00419ff0): a full disk is shown, then forgotten. */
void CheckSaveError( Menu &menu ) {
	if ( SV_AliceSaveError( qfalse ) == static_cast<int>( alicesave::SaveError::DiskFull ) ) {
		SetWidgetShown( menu, "MsgBackground", true );
		SetWidgetShown( menu, "DiskFull", true );
	}
	SV_AliceSaveError( qtrue );
}

/* loadgame (0x0041bcb0) through the command buffer. */
void LoadChosenSave( Widget &list ) {
	const int count = static_cast<int>( list.load.rows.size() );
	const int index = list.load.page.Selected();
	if ( count <= 0 || index <= 0 || index > count ) return;
	Cbuf_AddText( va( "loadgame %s\n", list.load.rows[index - 1].name.c_str() ) );
}

/* removegame (0x0041bbe0): the save's four files (Com_WipeSaveGame
 * 0x0043a400, fs_homepath only), then the list again. */
void RemoveChosenSave( Menu &menu, Widget &list ) {
	const int count = static_cast<int>( list.load.rows.size() );
	const int index = list.load.page.Selected();
	if ( count <= 0 || index <= 0 || index > count ) return;
	for ( const char *extension : alicesave::WipeOrder ) {
		const std::string path = SavePath( list.load.rows[index - 1].name, extension );
		if ( !path.empty() ) FS_HomeRemove( path.c_str() );
	}
	RefreshSaves( menu, list );
}

/* The `savegame` event (0x0041bec0) runs `savegame` at once; here it goes to
 * the front of the command buffer and its result is taken on the next frame
 * (SaveFinished). */
void StartSave( Menu &menu, int index ) {
	pendingSave.active = true;
	pendingSave.menu = MenuIndex( menu );
	pendingSave.widget = index;
	Cbuf_ExecuteText( EXEC_INSERT, "savegame\n" );
}

void ClearMenus( bool force );

/* The rest of the `savegame` event: a save that worked lists the saves again,
 * shows the newest and closes every menu; a full disk is shown; the error is
 * forgotten either way. */
void SaveFinished() {
	if ( !pendingSave.active ) return;
	pendingSave.active = false;
	if ( pendingSave.menu < 0 || pendingSave.menu >= static_cast<int>( menus.size() ) ) return;
	Menu &menu = menus[pendingSave.menu];
	if ( pendingSave.widget < 0 || pendingSave.widget >= static_cast<int>( menu.widgets.size() ) ) return;
	Widget &list = menu.widgets[pendingSave.widget];
	const int error = SV_AliceSaveError( qfalse );
	if ( error == 0 ) {
		RefreshSaves( menu, list );
		const int count = static_cast<int>( list.load.rows.size() );
		if ( count > 0 ) {
			list.load.page.page = ( count - 1 ) / mw::ShotsPerPage;
			list.load.page.tab = ( count - 1 ) % mw::ShotsPerPage + 1;
		}
		ClearMenus( true );
	} else if ( error == static_cast<int>( alicesave::SaveError::DiskFull ) ) {
		SetWidgetShown( menu, "MsgBackground", true );
		SetWidgetShown( menu, "DiskFull", true );
	}
	SV_AliceSaveError( qtrue );
}

/* UIFAKKLoadGameClass::KeyEvent (0x0041c090). Up and Down move the list
 * control's own selection, which the pictures do not follow. */
bool LoadGameKey( Menu &menu, Widget &list, int key ) {
	switch ( key ) {
	case K_DEL:
		RemoveChosenSave( menu, list );
		return true;
	case K_ENTER:
		LoadChosenSave( list );
		return true;
	case K_UPARROW:
	case K_DOWNARROW:
		ShowSaves( menu, list );
		return true;
	default:
		return false;
	}
}

/* ---- the classes' keys --------------------------------------------------- */

/* UIField::KeyEvent (0x004dfc00) and CharEvent (0x004dfaa0): every edit
 * writes the cvar; Enter writes it and runs the stuff command with the text. */
void FieldWrite( Widget &widget ) {
	if ( !widget.linkCvar.empty() ) UiCvarSet( widget.linkCvar, widget.field.text.c_str() );
}

bool FieldKey( Widget &widget, int key ) {
	mw::EditLine &line = widget.field;
	const bool ctrl = keys[K_CTRL].down != qfalse;
	const bool shift = keys[K_SHIFT].down != qfalse;
	if ( key == 'l' && ctrl ) return true;
	if ( key == K_ENTER || key == K_KP_ENTER ) {
		FieldWrite( widget );
		if ( !widget.stuffCommand.empty() ) Cbuf_AddText( ( widget.stuffCommand + " " + line.text + "\n" ).c_str() );
		return true;
	}
	if ( ( key == K_INS || key == K_KP_INS ) && shift ) return true;
	if ( key == K_DEL ) {
		if ( line.Delete() ) FieldWrite( widget );
		return true;
	}
	if ( key == K_RIGHTARROW ) { line.Right(); return true; }
	if ( key == K_LEFTARROW ) { line.Left(); return true; }
	if ( key == K_HOME || ( tolower( key ) == 'a' && ctrl ) ) { line.Home(); return true; }
	if ( key == K_END || ( tolower( key ) == 'e' && ctrl ) ) { line.End(); return true; }
	return key == K_INS;
}

void FieldChar( Widget &widget, int ch ) {
	mw::EditLine &line = widget.field;
	if ( ch == 0x16 || ch == 3 ) return;   // paste and copy are refused
	if ( ch == 8 ) {
		if ( line.Backspace() ) FieldWrite( widget );
		return;
	}
	if ( ch == 1 ) { line.Home(); return; }
	if ( ch == 5 ) { line.End(); return; }
	if ( ch >= 0x20 && line.Insert( static_cast<char>( ch ) ) ) FieldWrite( widget );
}

/* UISlider's arrows (0x004da8b0, 0x004da940): a step and the write. */
void SliderStep( Widget &widget, int direction ) {
	widget.slider.s.value = mw::Stepped( widget.slider.s, direction );
	SliderWrite( widget );
}

/* The keys a widget's class takes before the menu's own (the window
 * manager's KeyEvent 0x004b3382 asks the active control first). */
bool WidgetKey( Menu &menu, int index, int key ) {
	Widget &widget = menu.widgets[index];
	switch ( widget.kind ) {
	case WidgetKind::Slider:
		// 0x004da9d0
		if ( key == K_RIGHTARROW ) { SliderStep( widget, 1 ); return true; }
		if ( key == K_LEFTARROW ) { SliderStep( widget, -1 ); return true; }
		return false;
	case WidgetKind::List:
		// 0x004deb90
		if ( key == K_RIGHTARROW ) {
			widget.list.current = mw::ListNext( widget.list.current, static_cast<int>( widget.list.items.size() ) );
			ListWrite( widget );
			return true;
		}
		if ( key == K_LEFTARROW ) {
			widget.list.current = mw::ListPrevious( widget.list.current );
			ListWrite( widget );
			return true;
		}
		return false;
	case WidgetKind::Field:
		return FieldKey( widget, key );
	case WidgetKind::BindList:
		return BindListKey( menu, index, key );
	case WidgetKind::LoadGame:
		return LoadGameKey( menu, widget, key );
	default:
		return false;
	}
}

void WidgetChar( Menu &menu, int index, int ch ) {
	Widget &widget = menu.widgets[index];
	switch ( widget.kind ) {
	case WidgetKind::Field:
		FieldChar( widget, ch );
		break;
	case WidgetKind::CheckBox:
		// 0x004e0c90: the space bar ticks it, without the checked commands.
		if ( ch == ' ' ) {
			widget.check.checked = !widget.check.checked;
			CheckBoxWrite( widget );
		}
		break;
	default:
		break;
	}
}

/* ---- menu manager --------------------------------------------------------- */

/* A menu's linked cvars, for the widgets of one group or of all (Menu,
 * 0x004c4540 save, 0x004c4830 restore, 0x004c4a00 reset). The push saves them
 * all; Escape's pop and ui_restorecvars put them back; ui_setcvars saves them
 * again once they are applied. */
bool InGroup( const Widget &widget, const char *group ) {
	return !group || !group[0] || widget.groupId == group;
}

void SaveCvars( Menu &menu, const char *group ) {
	for ( Widget &widget : menu.widgets ) {
		if ( !widget.linkCvar.empty() && InGroup( widget, group ) ) {
			widget.savedCvar = UiCvarString( widget.linkCvar.c_str() );
		}
	}
}

void RestoreCvars( Menu &menu, const char *group ) {
	for ( const Widget &widget : menu.widgets ) {
		if ( !widget.linkCvar.empty() && InGroup( widget, group ) ) UiCvarSet( widget.linkCvar, widget.savedCvar.c_str() );
	}
}

void ResetCvars( Menu &menu, const char *group ) {
	for ( const Widget &widget : menu.widgets ) {
		if ( !widget.linkCvar.empty() && InGroup( widget, group ) ) UiCvarSet( widget.linkCvar, nullptr );
	}
}

/* ui_setcvars' restart check (0x004c4910), over every widget of the menu: a
 * cvar whose waiting value differs from the one saved asks for its system's
 * restart. Retail tells the two apart by the cvar's flags, 0x20 for the video
 * cvars and 0x1000 for the sound system's (all `s_`, registered at
 * 0x0044c738..0x0044c75b and 0x0046e4dd); this engine latches both kinds with
 * one flag, so the name decides [inferred]. */
void CheckRestart( Menu &menu ) {
	bool video = false, sound = false;
	for ( const Widget &widget : menu.widgets ) {
		if ( widget.linkCvar.empty() || !CvarExists( widget.linkCvar.c_str() ) ) continue;
		std::string latched;
		if ( !CvarLatched( widget.linkCvar.c_str(), latched ) ) continue;
		if ( !Q_stricmp( widget.savedCvar.c_str(), latched.c_str() ) ) continue;
		if ( !Q_stricmpn( widget.linkCvar.c_str(), "s_", 2 ) ) sound = true;
		else video = true;
	}
	if ( video ) Cbuf_AddText( "vid_restart; in_restart\n" );   // 0x0052b5cc
	if ( sound ) Cbuf_AddText( "snd_restart; in_restart\n" );   // 0x0052b5b0
}

void ShowMenu( Menu &menu, int delayMs ) {
	menu.visible = true;
	menu.hiding = false;
	menu.showStart = Now() + delayMs;
	menu.hovered = menu.pressed = -1;
	menu.focus = -1;
	for ( Widget &widget : menu.widgets ) {
		widget.shown = widget.enabled;
		widget.motion = Motion::In;
		widget.motionStart = menu.showStart;
		for ( Wait &wait : widget.waits ) wait.fireAt = -1;
		widget.check.depressed = false;
		widget.slider.leftPressed = widget.slider.rightPressed = widget.slider.dragging = false;
		widget.slider.repeatAt = -1;
		widget.list.previousPressed = widget.list.nextPressed = false;
	}
	for ( const std::string &command : menu.showCommands ) ExecuteLine( command );
	/* Each enabled widget reads its cvar and writes what it holds (the menu's
	 * show, 0x004c4d97..0x004c4dae). */
	for ( std::size_t i = 0; i < menu.widgets.size(); ++i ) {
		if ( !menu.widgets[i].enabled ) continue;
		UpdateUIElement( menu, static_cast<int>( i ) );
		UpdateData( menu, static_cast<int>( i ) );
	}
}

void FinishHide( Menu &menu ) {
	menu.visible = false;
	menu.hiding = false;
	for ( const std::string &command : menu.hideCommands ) ExecuteLine( command );
}

void HideMenu( Menu &menu, bool force ) {
	if ( captureMenu == MenuIndex( menu ) ) EndCapture();
	if ( !menu.visible ) return;
	if ( force ) {
		FinishHide( menu );
		return;
	}
	menu.hiding = true;
	menu.hideStart = Now();
	menu.hideEnd = Now() + LongestMotionMs( menu );
	for ( Widget &widget : menu.widgets ) {
		widget.motion = Motion::Out;
		widget.motionStart = Now();
	}
	menu.hovered = menu.pressed = -1;
}

bool PushMenu( const char *name, bool force ) {
	const int index = FindMenu( name );
	if ( index < 0 ) {
		Com_Printf( "Couldn't find menu %s\n", name );
		return false;
	}
	Menu *current = CurrentMenu();
	if ( current && current == &menus[index] ) {
		return true;
	}
	if ( !force && Now() < inputLockUntil ) {
		return false;
	}
	int delay = 0;
	if ( current ) {
		HideMenu( *current, false );
		delay = LongestMotionMs( *current );
	}
	menuStack.push_back( index );
	// The pushed menu's cvars as they stand (0x004c3bf6).
	SaveCvars( menus[index], nullptr );
	viewHasMouse = false;
	ShowMenu( menus[index], delay );
	inputLockUntil = Now() + delay + LongestMotionMs( menus[index] );
	return true;
}

/* MenuManager's pop (0x004c3f20): with `restore`, the menu's cvars go back to
 * what the push saved. */
void PopMenu( bool restore ) {
	if ( menuStack.empty() ) return;
	Menu &popped = menus[menuStack.back()];
	menuStack.pop_back();
	if ( restore ) RestoreCvars( popped, nullptr );
	HideMenu( popped, false );
	viewHasMouse = false;
	const int delay = LongestMotionMs( popped );
	int lock = delay;
	if ( Menu *next = CurrentMenu() ) {
		ShowMenu( *next, delay );
		lock += LongestMotionMs( *next );
	}
	inputLockUntil = Now() + lock;
}

void ClearMenus( bool force ) {
	while ( !menuStack.empty() ) {
		Menu &menu = menus[menuStack.back()];
		menuStack.pop_back();
		HideMenu( menu, force );
	}
	viewHasMouse = false;
	if ( force ) {
		for ( Menu &menu : menus ) {
			if ( menu.visible ) FinishHide( menu );
		}
		inputLockUntil = 0;
	}
}

bool PlayerDead() {
	return cls.state == CA_ACTIVE && cl.snap.ps.alice.block[0][0] <= 0;
}

/* UI_MenuEscape (0x0040d470): refused while a key is awaited (0x0040d460). */
void MenuEscape( const char *name ) {
	if ( CL_AliceLoadingActive() || Capturing() ) return;
	Menu *current = CurrentMenu();
	if ( current && !Q_stricmp( current->name.c_str(), name ) ) {
		if ( cls.state <= CA_DISCONNECTED || PlayerDead() ) return;
	}
	if ( current ) {
		if ( Now() >= inputLockUntil ) PopMenu( true );
	} else {
		PushMenu( name, false );
	}
}

/* The retail key handler ignores Escape while the intro has not given way to
 * a map and developer is off (CL_KeyEvent 0x00429dbe). */
bool EscapeBlockedByIntro() {
	return !Cvar_VariableIntegerValue( "developer" ) && introState != -1;
}

/* The toggle retail's `pause` command makes (0x00439800): unpause if paused,
 * otherwise pause a local single-player game. This engine pauses its local
 * server through cl_paused; `paused` is kept beside it for the client game,
 * which draws its pause watch from it. */
void TogglePause() {
	if ( Cvar_VariableIntegerValue( "paused" ) ) {
		Cvar_Set( "paused", "0" );
		Cvar_Set( "cl_paused", "0" );
	} else if ( !Cvar_VariableIntegerValue( "deathmatch" ) && com_sv_running && com_sv_running->integer ) {
		Cvar_Set( "paused", "1" );
		Cvar_Set( "cl_paused", "1" );
	}
}

/* Play is back on screen: undo the pause only if a menu took it and the game
 * is still paused, so a pause the player took before opening the menu stays
 * (UI_Update 0x0040c9b3..0x0040c9dd, and 0x0040c9fb..0x0040ca15 for a
 * cinematic). */
void ReleaseMenuPause() {
	if ( uiPauseState == 1 && Cvar_VariableIntegerValue( "paused" ) ) TogglePause();
	uiPauseState = 0;
}

/* ---- the intro ------------------------------------------------------------ */

void ScheduleIntro( int delayMs ) {
	introNextStep = Now() + delayMs;
}

void RunIntroStep() {
	introNextStep = -1;
	const screen::IntroStep step = screen::Intro( introState );
	Com_DPrintf( "Alice intro: state %d\n", introState );
	switch ( step.action ) {
	case screen::IntroAction::Nothing:
		return;
	case screen::IntroAction::Black:
	case screen::IntroAction::Skip:
		introState = step.nextState;
		ScheduleIntro( step.waitMs );
		return;
	case screen::IntroAction::PlateIn:
	case screen::IntroAction::PlateHold:
	case screen::IntroAction::PlateOut:
		introPlate.image = screen::IntroPlateImage;
		introPlate.scale = screen::IntroPlateScale;
		introPlate.from = step.plateFrom;
		introPlate.to = step.plateTo;
		introPlate.lengthMs = step.plateMs;
		introPlate.start = Now();
		introState = step.nextState;
		ScheduleIntro( step.waitMs );
		return;
	case screen::IntroAction::Video:
		/* The logo is played directly, with the flag that resumes the intro
		 * at the next state when it ends (0x0040fd50..0x0040fd69); its end
		 * calls CL_AliceUISetIntroState and appends `_intro` (CIN_Finish
		 * 0x0042585b..0x0042586a). No wait is queued. The state then goes
		 * up by one from whatever it is once the call returns (0x0040fd73).
		 * A logo that does not open leaves the intro there, as retail's
		 * does: CIN_Play returns without a word for a missing file
		 * (0x00427382) and its end does nothing for a bad header (status 0,
		 * 0x004257b8), so nothing appends another `_intro`. */
		Cvar_Set( "nextmap", "" );
		CL_AliceCinematicPlay( step.video, step.videoFlags );
		++introState;
		return;
	case screen::IntroAction::MainMenu:
		if ( uiInitialized ) {
			PushMenu( "main", true );
		} else {
			// Between this client's UI restarts: opened when it is back.
			savedStack.assign( 1, "main" );
		}
		S_AliceMusicNewSoundtrack( screen::IntroSoundtrack );
		introPlate = {};
		introState = step.nextState;
		return;
	case screen::IntroAction::Soundtrack:
		/* State -1, a map loaded: the main menu's case without its push
		 * (0x0040fdd2). The movie that ends into a map load appends
		 * `_intro` after the map, so this runs then. */
		S_AliceMusicNewSoundtrack( screen::IntroSoundtrack );
		introPlate = {};
		introState = step.nextState;
		return;
	}
}

void DoIntro_f() {
	introQueued = false;
	introState = 0;
	RunIntroStep();
}

void Intro_f() {
	RunIntroStep();
}

bool IntroRunning() {
	return introQueued || introState > 0 || introNextStep >= 0;
}

/* ---- widget commands ------------------------------------------------------ */

void SetGroup( Menu &menu, const Widget &target, const std::string &group, bool activate, bool enable ) {
	bool found = false;
	for ( Widget &widget : menu.widgets ) {
		if ( widget.groupId.empty() ) continue;
		const bool member = widget.groupId == group;
		found = found || member;
		if ( activate ) {
			widget.enabled = widget.shown = member;
			widget.motion = member ? Motion::In : Motion::Out;
			widget.motionStart = Now();
		} else if ( member ) {
			widget.enabled = widget.shown = enable;
		}
	}
	(void)target;
	if ( !found ) Com_Printf( "Invalid group %s\n", group.c_str() );
}

/* The events only a class answers: UIFAKKLoadGameClass (its table at
 * 0x004199f0), UIAutoScroll (0x004c0490..0x004c0e80) and UIWidget3D's
 * setanim (0x004d2600). */
bool ClassCommand( Menu &menu, int index, const char *event ) {
	Widget &widget = menu.widgets[index];
	const auto is = [event]( const char *name ) { return !Q_stricmp( event, name ); };
	if ( widget.kind == WidgetKind::LoadGame ) {
		mw::SavePage &page = widget.load.page;
		const int count = static_cast<int>( widget.load.rows.size() );
		if ( is( "resetpage" ) || is( "refreshtab" ) ) ShowSaves( menu, widget );
		else if ( is( "CheckErrorState" ) ) CheckSaveError( menu );
		else if ( is( "selecttab" ) ) { if ( page.Select( atoi( Cmd_Argv( 3 ) ), count ) ) ShowSaves( menu, widget ); }
		else if ( is( "nextpage" ) ) { if ( page.Next( count ) ) ShowSaves( menu, widget ); }
		else if ( is( "prevpage" ) ) { if ( page.Previous() ) ShowSaves( menu, widget ); }
		else if ( is( "loadgame" ) ) LoadChosenSave( widget );
		else if ( is( "savegame" ) ) StartSave( menu, index );
		else if ( is( "removegame" ) ) RemoveChosenSave( menu, widget );
		else return false;
		return true;
	}
	if ( widget.kind == WidgetKind::AutoScroll ) {
		if ( is( "resetscroll" ) ) {
			const float lineHeight = static_cast<float>( FontHeight( widget ) ) + widget.lineSpacing;
			widget.scroll.position = Cmd_Argc() > 3 ? mw::ResetScrollPosition( ToFloat( Cmd_Argv( 3 ) ), lineHeight ) : 0.0f;
			return true;
		}
	}
	if ( widget.kind == WidgetKind::Alice3D && is( "setanim" ) ) {
		// setanim <anim>: played once (0x004d2637 passes no loop and no turn).
		SetModelAnim( widget.model, Cmd_Argv( 3 ), false, 0.0f );
		return true;
	}
	return false;
}

void WidgetCommand_f() {
	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: widgetcommand <widget> <command> [args]\n" );
		return;
	}
	Menu *menu = CurrentMenu();
	if ( !menu ) return;
	const int index = FindWidgetIndex( *menu, Cmd_Argv( 1 ) );
	if ( index < 0 ) {
		Com_DPrintf( "widgetcommand: no widget %s in %s\n", Cmd_Argv( 1 ), menu->name.c_str() );
		return;
	}
	Widget *widget = &menu->widgets[index];
	const char *event = Cmd_Argv( 2 );
	if ( !Q_stricmp( event, "hide" ) ) {
		widget->shown = false;
	} else if ( !Q_stricmp( event, "enable" ) ) {
		widget->enabled = widget->shown = true;
	} else if ( !Q_stricmp( event, "disable" ) ) {
		widget->enabled = widget->shown = false;
	} else if ( !Q_stricmp( event, "showifalive" ) ) {
		// Shown only in the game with health left (0x0040d520).
		const bool alive = cls.state == CA_ACTIVE && cl.snap.ps.alice.block[0][0] > 0;
		widget->enabled = widget->shown = alive;
	} else if ( !Q_stricmp( event, "triggerwait" ) ) {
		for ( Wait &wait : widget->waits ) wait.fireAt = Now() + wait.delayMs;
	} else if ( !Q_stricmp( event, "title" ) ) {
		widget->title = Cmd_ArgsFrom( 3 );
	} else if ( !Q_stricmp( event, "shader" ) ) {
		widget->shader = Cmd_Argv( 3 );
	} else if ( !Q_stricmp( event, "resetmotion" ) ) {
		const char *how = Cmd_Argv( 3 );
		widget->motion = !Q_stricmp( how, "out" ) || !Q_stricmp( how, "outin" ) ? Motion::Out
			: !Q_stricmp( how, "none" ) ? Motion::None : Motion::In;
		widget->motionStart = Now();
	} else if ( !Q_stricmp( event, "activategroup" ) ) {
		SetGroup( *menu, *widget, Cmd_Argv( 3 ), true, true );
	} else if ( !Q_stricmp( event, "enablegroup" ) ) {
		SetGroup( *menu, *widget, Cmd_Argv( 3 ), false, true );
	} else if ( !Q_stricmp( event, "disablegroup" ) ) {
		SetGroup( *menu, *widget, Cmd_Argv( 3 ), false, false );
	} else if ( !ClassCommand( *menu, index, event ) ) {
		/* Any other event goes to the widget as the .urc line would. */
		urc::Property property;
		property.name = event;
		for ( int i = 3; i < Cmd_Argc(); ++i ) property.args.emplace_back( Cmd_Argv( i ) );
		ApplyWidgetProperty( *widget, property, "widgetcommand" );
	}
}

/* ---- commands ------------------------------------------------------------- */

/* A menu pushed while this client has its UI down (a map load or a video
 * restart) opens when the UI is back, as the menu retail's never-stopped UI
 * would have pushed. */
void PushMenu_f() {
	if ( Cmd_Argc() < 2 ) { Com_Printf( "Usage: pushmenu <menuname>\n" ); return; }
	if ( !uiInitialized ) { savedStack.emplace_back( Cmd_Argv( 1 ) ); return; }
	PushMenu( Cmd_Argv( 1 ), false );
}

void ForceMenu_f() {
	if ( Cmd_Argc() < 2 ) { Com_Printf( "Usage: forcemenu <menuname>\n" ); return; }
	if ( !uiInitialized ) { savedStack.emplace_back( Cmd_Argv( 1 ) ); return; }
	PushMenu( Cmd_Argv( 1 ), true );
}

/* popmenu prints its usage without an argument and pops anyway (0x0040d0b0);
 * the argument, read with atoi, asks for the menu's cvars to be restored. */
void PopMenu_f() {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: popmenu <restore_cvars - set to 1 if you want all cvars restored to their original values>\n" );
	}
	if ( !uiInitialized ) {
		if ( !savedStack.empty() ) savedStack.pop_back();
		return;
	}
	PopMenu( atoi( Cmd_Argv( 1 ) ) != 0 );
}

void ShowMenu_f() {
	if ( Cmd_Argc() < 2 ) { Com_Printf( "Usage: showmenu <menuname>\n" ); return; }
	if ( !uiInitialized ) { savedStack.emplace_back( Cmd_Argv( 1 ) ); return; }
	PushMenu( Cmd_Argv( 1 ), false );
}

void ListMenus_f() {
	for ( const Menu &menu : menus ) {
		Com_Printf( "%s (%s)%s\n", menu.name.c_str(), menu.file.c_str(), menu.visible ? " visible" : "" );
	}
}

void CenterPrint_f() {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: centerprint string [alpha]\n" );
		return;
	}
	CL_AliceSetCenterPrint( Cmd_Argv( 1 ), Cmd_Argc() > 2 ? static_cast<float>( atof( Cmd_Argv( 2 ) ) ) : 1.0f );
}

/* locationprint x y string [alpha] [w h] (0x0040edf0): the quicksave camera the
 * game stuffs is `locationprint 0 0 @ui/quicksavecam/quicksavecam 1 64 64`. */
void LocationPrint_f() {
	if ( Cmd_Argc() < 4 ) {
		Com_Printf( "Usage: locationprint x y string [alpha]\n" );
		return;
	}
	const float alpha = Cmd_Argc() > 4 ? static_cast<float>( atof( Cmd_Argv( 4 ) ) ) : 1.0f;
	const int w = Cmd_Argc() > 6 ? atoi( Cmd_Argv( 5 ) ) : -1;
	const int h = Cmd_Argc() > 6 ? atoi( Cmd_Argv( 6 ) ) : -1;
	CL_AliceSetLocationPrint( atoi( Cmd_Argv( 1 ) ), atoi( Cmd_Argv( 2 ) ), Cmd_Argv( 3 ), alpha, w, h );
}

/* ui_resetcvars, ui_restorecvars and ui_setcvars [group] (0x0040f230,
 * 0x0040f260, 0x0040f290) on the current menu. Retail's ui_restorecvars
 * resets instead when the current menu is one showmenu put up (0x004c52c4);
 * this client's showmenu pushes, so that case does not arise. */
void UiResetCvars_f() {
	const std::string group = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "";
	if ( Menu *menu = CurrentMenu() ) ResetCvars( *menu, group.c_str() );
}

void UiRestoreCvars_f() {
	const std::string group = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "";
	if ( Menu *menu = CurrentMenu() ) RestoreCvars( *menu, group.c_str() );
}

void UiSetCvars_f() {
	const std::string group = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "";
	if ( Menu *menu = CurrentMenu() ) {
		CheckRestart( *menu );
		SaveCvars( *menu, group.c_str() );
	}
}

/* ui_hidemouse (0x0040f2e0) gives the mouse to the view while the menu stays;
 * ui_showmouse (0x0040f2c0) takes it back. */
void UiHideMouse_f() { if ( CurrentMenu() ) viewHasMouse = true; }
void UiShowMouse_f() { viewHasMouse = false; }

/* The engine UI's commands (UI_Init 0x004105fc..0x0041080b). Retail registers
 * them once and never shuts that UI down. This client restarts the hosted
 * module with every map load and video restart, so every one of them is
 * registered once and outlives those restarts: a command queued just before a
 * load -- a `widgetcommand`, a `pushmenu`, the `_intro` a movie appends behind
 * the map it starts (CIN_Finish 0x00425865) -- must not find itself unknown.
 * Each works on the menus the UI has, and while it is down there are none. */
const char *const kCommands[] = {
	"pushmenu", "forcemenu", "popmenu", "showmenu", "widgetcommand", "listmenus",
	"centerprint", "locationprint", "ui_resetcvars", "ui_restorecvars", "ui_setcvars",
	"ui_showmouse", "ui_hidemouse",
};
const xcommand_t kCommandHandlers[] = {
	PushMenu_f, ForceMenu_f, PopMenu_f, ShowMenu_f, WidgetCommand_f, ListMenus_f,
	CenterPrint_f, LocationPrint_f, UiResetCvars_f, UiRestoreCvars_f, UiSetCvars_f,
	UiShowMouse_f, UiHideMouse_f,
};
static_assert( std::size( kCommands ) == std::size( kCommandHandlers ), "one handler for each command" );

bool commandsRegistered;
bool introCommandsRegistered;

void RegisterCommands() {
	if ( !commandsRegistered ) {
		commandsRegistered = true;
		for ( std::size_t i = 0; i < std::size( kCommands ); ++i ) Cmd_AddCommand( kCommands[i], kCommandHandlers[i] );
	}
	if ( !introCommandsRegistered ) {
		introCommandsRegistered = true;
		Cmd_AddCommand( "dointro", DoIntro_f );
		Cmd_AddCommand( "_intro", Intro_f );
	}
}

/* The engine UI's cvars (UI_Init 0x004104e4-0x004105d5). */
void RegisterCvars() {
	Cvar_Get( "ui_drawcoords", "0", 0 );
	Cvar_Get( "ui_minicon", "0", CVAR_ARCHIVE );
	Cvar_Get( "ui_consoleposition", "", CVAR_ARCHIVE );
	Cvar_Get( "ui_inventoryfile", "global/inventory.scr", 0 );
	ui_console = Cvar_Get( "ui_console", "0", CVAR_ARCHIVE );
	Cvar_Get( "ui_newvidmode", "-1", 0 );
	ui_hud = Cvar_Get( "ui_hud", "1", CVAR_ARCHIVE );
	ui_subtitles = Cvar_Get( "ui_subtitles", "1", CVAR_ARCHIVE );
	Cvar_Get( "ui_skipDepthClear", "1", 0 );
	Cvar_Get( "ui_crosshair", "1", CVAR_ARCHIVE );
	Cvar_Get( "ui_jumpreticle", "1", CVAR_ARCHIVE );
	Cvar_Get( "dlg_badsave", "This save game is invalid", 0 );
}

/* ---- input ---------------------------------------------------------------- */

float VirtualCursorX() { return cursorX * 640.0f / cls.glconfig.vidWidth; }
float VirtualCursorY() { return cursorY * 480.0f / cls.glconfig.vidHeight; }

/* A widget the cursor can rest on. Labels with nothing to do on hover pass
 * the mouse through, so a decoration drawn over a button does not hide it; so
 * do the classes with no mouse of their own. */
bool Hoverable( const Widget &widget ) {
	if ( !widget.shown || !widget.enabled || widget.noInput ) return false;
	switch ( widget.kind ) {
	case WidgetKind::Label:
		return !widget.hoverShader.empty() || !widget.mouseEnterCommand.empty();
	case WidgetKind::AutoScroll:
	case WidgetKind::Alice3D:
	case WidgetKind::LoadGame:
		return false;
	default:
		return true;
	}
}

int WidgetAt( const Menu &menu, float vx, float vy ) {
	// The last-declared widget under the cursor is on top.
	for ( int i = static_cast<int>( menu.widgets.size() ) - 1; i >= 0; --i ) {
		const Widget &widget = menu.widgets[i];
		if ( !Hoverable( widget ) ) continue;
		float x, y;
		WidgetPosition( menu, widget, x, y );
		if ( vx >= x && vx < x + widget.w && vy >= y && vy < y + widget.h ) return i;
	}
	return -1;
}

bool Inside( float px, float py, float x, float y, float w, float h ) {
	return px >= x && px < x + w && py >= y && py < y + h;
}

/* ---- UISlider's mouse (0x004daea0 down, 0x004dac70 dragged, 0x004db420 up,
 * 0x004dabb0 exited, 0x004dabf0 entered, 0x004daa10/0x004daae0 repeat) --- */

void SliderDown( const Menu &menu, Widget &widget, float mx, float my ) {
	float x, y;
	WidgetPosition( menu, widget, x, y );
	mw::Slider &s = widget.slider.s;
	const float arrow = static_cast<float>( s.arrowWidth );
	if ( Inside( mx, my, x, y, arrow, widget.h ) ) {
		SliderStep( widget, -1 );
		widget.slider.leftPressed = true;
		widget.slider.repeatAt = Now() + 200;   // 0.2 s (0x3e4ccccd)
		return;
	}
	if ( Inside( mx, my, x + widget.w - arrow, y, arrow, widget.h ) ) {
		SliderStep( widget, 1 );
		widget.slider.rightPressed = true;
		widget.slider.repeatAt = Now() + 200;
		return;
	}
	widget.slider.dragFrom = s.value;
	widget.slider.dragging = true;
	s.value = mw::ValueAt( s, mw::TrackFor( s, x, widget.w ), mx );
	SliderWrite( widget );
}

void SliderDragged( const Menu &menu, Widget &widget, float mx, float my ) {
	float x, y;
	WidgetPosition( menu, widget, x, y );
	mw::Slider &s = widget.slider.s;
	if ( widget.slider.dragging ) {
		s.value = mw::ValueAt( s, mw::TrackFor( s, x, widget.w ), mx );
		SliderWrite( widget );
		return;
	}
	const float arrow = static_cast<float>( s.arrowWidth );
	if ( ( !widget.slider.leftPressed && Inside( mx, my, x, y, arrow, widget.h ) )
		|| ( !widget.slider.rightPressed && Inside( mx, my, x + widget.w - arrow, y, arrow, widget.h ) ) ) {
		SliderDown( menu, widget, mx, my );
	}
}

void SliderRelease( Widget &widget ) {
	widget.slider.leftPressed = widget.slider.rightPressed = widget.slider.dragging = false;
	widget.slider.repeatAt = -1;
}

/* An arrow held repeats every 0.1 s while the cursor stays on it. */
void SliderRepeat( const Menu &menu, Widget &widget ) {
	if ( widget.slider.repeatAt < 0 || Now() < widget.slider.repeatAt ) return;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	const float arrow = static_cast<float>( widget.slider.s.arrowWidth );
	const float mx = VirtualCursorX(), my = VirtualCursorY();
	const bool left = widget.slider.leftPressed;
	if ( left ? Inside( mx, my, x, y, arrow, widget.h ) : Inside( mx, my, x + widget.w - arrow, y, arrow, widget.h ) ) {
		SliderStep( widget, left ? -1 : 1 );
		widget.slider.repeatAt = Now() + 100;   // 0.1 s (0x3dcccccd)
	} else {
		SliderRelease( widget );
	}
}

/* ---- UIList's mouse (0x004deee0 down, 0x004df0c0 up) --------------------- */

void ListDown( const Menu &menu, Widget &widget, float mx, float my ) {
	float x, y;
	WidgetPosition( menu, widget, x, y );
	const int count = static_cast<int>( widget.list.items.size() );
	if ( Inside( mx, my, x, y, mw::ListArrowWidth, widget.h ) ) {
		widget.list.previousPressed = true;
		widget.list.current = mw::ListPrevious( widget.list.current );
	} else if ( Inside( mx, my, x + widget.w - mw::ListArrowWidth, y, mw::ListArrowWidth, widget.h ) ) {
		widget.list.nextPressed = true;
		widget.list.current = mw::ListNext( widget.list.current, count );
	} else {
		widget.list.nextPressed = true;
		widget.list.current = mw::ListCycle( widget.list.current, count );
	}
	ListWrite( widget );
}

/* ---- UIFakkBindList's mouse: its rows' labels (UIFakkBindListLabel
 * 0x004e5200), their key buttons (UIBindButton 0x004d1540) and its scrollbar
 * (UIVertScroll 0x004d8e10) ------------------------------------------------ */

inline constexpr float ScrollbarWidth = 16.0f;   // 0x004ff498

/* The row and column under the cursor, false for none. */
bool BindCellAt( const Menu &menu, const Widget &widget, float mx, float my, int &row, int &column ) {
	const BindState &bind = widget.bind;
	const mw::BindLayout &layout = bind.layout;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	for ( int r = 0; r < static_cast<int>( layout.items.size() ); ++r ) {
		if ( !mw::RowVisible( r, bind.top, layout.commandHeight, widget.h ) ) continue;
		const float ry = y + static_cast<float>( mw::VisibleRowTop( r, bind.top, layout.commandHeight ) );
		// The key button is drawn over the right end of the row's label.
		if ( Inside( mx, my, x + layout.commandWidth, ry, static_cast<float>( layout.primaryKeyWidth ),
			static_cast<float>( layout.primaryKeyHeight ) ) ) {
			row = r;
			column = 2;
			return true;
		}
		if ( Inside( mx, my, x, ry, static_cast<float>( layout.commandWidth + layout.primaryKeyWidth ),
			static_cast<float>( layout.commandHeight ) ) ) {
			row = r;
			column = 1;
			return true;
		}
	}
	return false;
}

/* The scrollbar's thumb: its length for the page, at least 6 units, and its
 * place for the position, below the 16-unit arrow (0x004d8670). */
void ThumbFor( const Widget &widget, float &offset, float &length ) {
	const BindState &bind = widget.bind;
	const float track = widget.h - 32.0f;
	const float count = static_cast<float>( bind.layout.items.size() );
	length = count > 0.0f ? static_cast<float>( bind.page ) * track / count : track;
	if ( length < 6.0f ) length = 6.0f;
	offset = count > 0.0f ? static_cast<float>( bind.top ) * track / count : 0.0f;
	if ( offset + length - track > 0.0f ) offset -= offset + length - track;
	offset += ScrollbarWidth;
}

void BindListDown( Menu &menu, int index, float mx, float my ) {
	Widget &widget = menu.widgets[index];
	BindState &bind = widget.bind;
	if ( !bind.built ) return;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	const float barX = x + widget.w - ScrollbarWidth;
	if ( mx >= barX ) {
		const float local = my - y;
		if ( local < ScrollbarWidth ) {
			bind.scrollPart = 1;
			ScrollTo( bind, bind.top - 1 );
		} else if ( local >= widget.h - ScrollbarWidth ) {
			bind.scrollPart = 2;
			ScrollTo( bind, bind.top + 1 );
		} else {
			float offset, length;
			ThumbFor( widget, offset, length );
			if ( local < offset ) ScrollTo( bind, bind.top - bind.page );
			else if ( local >= offset + length ) ScrollTo( bind, bind.top + bind.page );
			else { bind.thumbDrag = true; bind.thumbGrab = local - offset; }
		}
		return;
	}
	int row = 0, column = 1;
	if ( !BindCellAt( menu, widget, mx, my, row, column ) ) return;
	/* A cell clicked again waits for a key; either way it is the chosen one
	 * (0x004e47f0, 0x004e4740). */
	if ( row == bind.row && column == bind.column ) StartCapture( menu, index );
	bind.row = row;
	bind.column = column;
	PlayLocalSound( bind.layout.changeSound );
}

void BindListDragged( const Menu &menu, Widget &widget, float my ) {
	BindState &bind = widget.bind;
	if ( !bind.thumbDrag ) return;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	const float track = widget.h - 32.0f;
	const float count = static_cast<float>( bind.layout.items.size() );
	if ( track <= 0.0f ) return;
	const float offset = my - y - bind.thumbGrab - ScrollbarWidth;
	ScrollTo( bind, static_cast<int>( offset * count / track + 0.5f ) );
}

/* ---- the window manager's mouse (0x004b3a50) ------------------------------ */

void MouseEnter( Menu &menu, int index ) {
	Widget &widget = menu.widgets[index];
	if ( !widget.mouseEnterCommand.empty() ) ExecuteLine( widget.mouseEnterCommand );
	switch ( widget.kind ) {
	case WidgetKind::Button:
		PlayLocalSound( widget.hoverSound );
		menu.focus = index;
		break;
	case WidgetKind::CheckBox:
		// 0x004e07e0: the control takes the keys; a held button presses it.
		menu.focus = index;
		if ( cursorButtons & 1 ) widget.check.depressed = true;
		break;
	case WidgetKind::Slider:
		menu.focus = index;
		if ( cursorButtons & 1 ) SliderDown( menu, widget, VirtualCursorX(), VirtualCursorY() );
		break;
	default:
		break;
	}
}

void MouseExit( Menu &menu, int index ) {
	Widget &widget = menu.widgets[index];
	if ( !widget.mouseExitCommand.empty() ) ExecuteLine( widget.mouseExitCommand );
	switch ( widget.kind ) {
	case WidgetKind::CheckBox:
		widget.check.depressed = false;
		break;
	case WidgetKind::Slider:
		// A drag that leaves the slider puts the value back.
		if ( widget.slider.dragging ) {
			widget.slider.s.value = widget.slider.dragFrom;
			SliderWrite( widget );
		}
		SliderRelease( widget );
		break;
	default:
		break;
	}
}

void UpdateHover() {
	Menu *menu = CurrentMenu();
	if ( !menu ) return;
	const int hovered = Now() < menu->showStart || cursorX < 0 || viewHasMouse
		? -1 : WidgetAt( *menu, VirtualCursorX(), VirtualCursorY() );
	if ( hovered == menu->hovered ) return;
	if ( menu->hovered >= 0 ) MouseExit( *menu, menu->hovered );
	menu->hovered = hovered;
	if ( hovered >= 0 ) MouseEnter( *menu, hovered );
}

/* Left button down: the widget under the cursor takes the keys (0x004b3c4f)
 * and the press; a button holds the mouse until it is let go. */
void MouseDown( Menu &menu ) {
	UpdateHover();
	const int index = menu.hovered;
	if ( index < 0 ) return;
	Widget &widget = menu.widgets[index];
	menu.focus = index;
	const float mx = VirtualCursorX(), my = VirtualCursorY();
	switch ( widget.kind ) {
	case WidgetKind::Button:
		menu.pressed = index;
		break;
	case WidgetKind::CheckBox:
		widget.check.depressed = true;   // 0x004e0770
		break;
	case WidgetKind::Slider:
		SliderDown( menu, widget, mx, my );
		break;
	case WidgetKind::List:
		ListDown( menu, widget, mx, my );
		break;
	case WidgetKind::BindList:
		BindListDown( menu, index, mx, my );
		break;
	default:
		break;
	}
}

void Click( Widget &widget ) {
	PlayLocalSound( widget.clickSound );
	ExecuteLine( widget.stuffCommand );
}

void MouseUp( Menu &menu ) {
	if ( menu.pressed >= 0 ) {
		Widget &widget = menu.widgets[menu.pressed];
		const int under = WidgetAt( menu, VirtualCursorX(), VirtualCursorY() );
		const int pressed = menu.pressed;
		menu.pressed = -1;
		if ( under == pressed ) Click( widget );
		return;
	}
	const int index = menu.hovered;
	if ( index < 0 ) return;
	Widget &widget = menu.widgets[index];
	switch ( widget.kind ) {
	case WidgetKind::CheckBox:
		/* 0x004e0780: the tick changes, its checked or unchecked command runs,
		 * and the cvar and stuff command follow. */
		widget.check.checked = !widget.check.checked;
		ExecuteLine( widget.check.checked ? widget.check.checkedCommand : widget.check.uncheckedCommand );
		widget.check.depressed = false;
		CheckBoxWrite( widget );
		break;
	case WidgetKind::Slider:
		SliderRelease( widget );
		break;
	case WidgetKind::List:
		widget.list.previousPressed = widget.list.nextPressed = false;
		break;
	case WidgetKind::BindList:
		widget.bind.scrollPart = 0;
		widget.bind.thumbDrag = false;
		break;
	default:
		break;
	}
}

void MouseDragged( Menu &menu ) {
	const int index = menu.pressed >= 0 ? menu.pressed : menu.hovered;
	if ( index < 0 ) return;
	Widget &widget = menu.widgets[index];
	if ( widget.kind == WidgetKind::Slider ) SliderDragged( menu, widget, VirtualCursorX(), VirtualCursorY() );
	else if ( widget.kind == WidgetKind::BindList ) BindListDragged( menu, widget, VirtualCursorY() );
}

/* A mouse button while a key is awaited binds that button when the cursor is
 * on the waiting key button (UIBindButton any_mouse_down 0x004d1591..
 * 0x004d15c2). */
bool CaptureMouse( int key ) {
	Menu &menu = menus[captureMenu];
	const Widget &widget = menu.widgets[captureWidget];
	int row = 0, column = 1;
	if ( !BindCellAt( menu, widget, VirtualCursorX(), VirtualCursorY(), row, column ) || row != widget.bind.row
		|| column != 2 ) {
		return false;
	}
	CaptureKey( key );
	return true;
}

/* Escape, in retail's order (CL_KeyEvent 0x00429d9d..0x00429e4c): ignored
 * while the intro has not given way to a map and developer is off; a second
 * press within a second (outside a cinematic) is a menu escape; otherwise a
 * menu up takes it, a scripted in-game cinematic is skipped, the menus take
 * it while disconnected or in the game, and a cinematic with no mouse button
 * held is stopped. The modal test (0x0040c7e0) is read here as "a menu is
 * up" [inferred]. */
int lastEscapeTime = -100000;

void EscapeKey() {
	if ( EscapeBlockedByIntro() ) return;
	if ( Now() - lastEscapeTime < 1000 && cls.state != CA_CINEMATIC ) {
		MenuEscape( "main" );
		return;
	}
	lastEscapeTime = Now();
	if ( !menuStack.empty() ) {
		MenuEscape( "main" );
		return;
	}
	if ( cls.state == CA_ACTIVE && ( cl.snap.ps.alice.block[0][5] & 1 ) ) {
		Cbuf_AddText( "skipcinematic\n" );
		return;
	}
	if ( cls.state <= CA_DISCONNECTED || cls.state == CA_ACTIVE ) {
		MenuEscape( "main" );
		return;
	}
	if ( cls.state == CA_CINEMATIC ) {
		for ( int mouse = K_MOUSE1; mouse <= K_MOUSE5; ++mouse ) {
			if ( keys[mouse].down ) return;
		}
		/* CIN_Stop (0x00425770): a logo's resume state is cleared, so its end
		 * sends the intro to the main menu (state 100), and a movie with a
		 * nextmap starts that map. */
		SCR_StopCinematic();
	}
}

bool RunKeyCommands( const std::vector<KeyCommand> &commands, int key ) {
	for ( const KeyCommand &command : commands ) {
		if ( command.key == key ) {
			ExecuteLine( command.command );
			return true;
		}
	}
	return false;
}

/* Up and Down step the focus through the widgets by their order numbers. */
void MoveFocus( Menu &menu, int step ) {
	std::vector<int> ordered;
	for ( std::size_t i = 0; i < menu.widgets.size(); ++i ) {
		const Widget &widget = menu.widgets[i];
		if ( widget.orderNumber > 0 && widget.shown && widget.enabled ) ordered.push_back( static_cast<int>( i ) );
	}
	if ( ordered.empty() ) return;
	std::stable_sort( ordered.begin(), ordered.end(), [&menu]( int a, int b ) {
		return menu.widgets[a].orderNumber < menu.widgets[b].orderNumber;
	} );
	auto at = std::find( ordered.begin(), ordered.end(), menu.focus );
	int position = at == ordered.end() ? ( step > 0 ? -1 : 0 ) : static_cast<int>( at - ordered.begin() );
	position = ( position + step + static_cast<int>( ordered.size() ) ) % static_cast<int>( ordered.size() );
	menu.focus = ordered[position];
}

void KeyEvent( int key, bool down ) {
	Menu *menu = CurrentMenu();
	if ( key & K_CHAR_FLAG ) {
		// Characters go to the control that has the keys (CharEvent, slot 13).
		if ( down && menu && Now() >= inputLockUntil && !Capturing() && menu->focus >= 0 ) {
			WidgetChar( *menu, menu->focus, key & ~K_CHAR_FLAG );
		}
		return;
	}
	if ( !down ) {
		if ( key == K_MOUSE1 && menu ) MouseUp( *menu );
		return;
	}
	// A key button waiting for a key takes the next one, Escape included.
	if ( Capturing() ) {
		if ( key >= K_MOUSE1 && key <= K_MOUSE5 ) {
			if ( CaptureMouse( key ) ) return;
		} else {
			CaptureKey( key );
			return;
		}
	}
	if ( key == K_ESCAPE ) {
		EscapeKey();
		return;
	}
	if ( !menu || Now() < inputLockUntil ) return;
	if ( key == K_MOUSE1 ) {
		MouseDown( *menu );
		return;
	}
	if ( menu->focus >= 0 && WidgetKey( *menu, menu->focus, key ) ) return;
	if ( menu->focus >= 0 && RunKeyCommands( menu->widgets[menu->focus].keyCommands, key ) ) return;
	if ( RunKeyCommands( menu->keyCommands, key ) ) return;
	switch ( key ) {
	case K_UPARROW:
	case K_KP_UPARROW:
		MoveFocus( *menu, -1 );
		break;
	case K_DOWNARROW:
	case K_KP_DOWNARROW:
		MoveFocus( *menu, 1 );
		break;
	case K_ENTER:
	case K_KP_ENTER:
		if ( menu->focus >= 0 && menu->widgets[menu->focus].kind == WidgetKind::Button ) {
			Click( menu->widgets[menu->focus] );
		}
		break;
	default:
		break;
	}
}

void MouseEvent( int dx, int dy ) {
	if ( cursorX < 0 ) {
		cursorX = cls.glconfig.vidWidth * 0.5f;
		cursorY = cls.glconfig.vidHeight * 0.5f;
	}
	cursorX = std::min( std::max( cursorX + dx, 0.0f ), cls.glconfig.vidWidth - 1.0f );
	cursorY = std::min( std::max( cursorY + dy, 0.0f ), cls.glconfig.vidHeight - 1.0f );
}

/* ---- drawing -------------------------------------------------------------- */

/* A widget's or a menu's border (Display 0x004bd923..0x004bd998): 3D_BORDER
 * raised, INDENT_BORDER sunk, in the border colours its background made. */
void DrawBorder( float x, float y, float w, float h, int style, const mw::BorderColor &border, float alpha ) {
	if ( style != 1 && style != 2 ) return;
	Box3D( x, y, w, h, style == 2, border, alpha );
}

void DrawWidgetText( const Menu &menu, const Widget &widget, float x, float y, float alpha, bool pressed ) {
	std::string text;
	if ( widget.kind == WidgetKind::Label && !widget.linkCvar.empty() ) {
		text = Cvar_VariableString( widget.linkCvar.c_str() );
	} else {
		text = widget.title;
	}
	if ( text.empty() ) return;
	const float color[4] = { widget.fg[0], widget.fg[1], widget.fg[2], widget.fg[3] * alpha };
	const float offset = pressed ? 1.0f : 0.0f;
	if ( widget.kind == WidgetKind::Button && widget.fontPosX != -1.0f && widget.fontPosY != -1.0f ) {
		// A button with a text position ignores its justification.
		CL_AliceFontPrint( widget.font, color, false, x + widget.fontPosX + offset, y + widget.fontPosY + offset,
			text.c_str(), -1, widget.fontAngle, widget.charSpacing, widget.lineSpacing, widget.fontScale,
			widget.fontScale );
		return;
	}
	font::Rect rect{ x + offset, y + offset, widget.w, widget.h };
	CL_AliceFontPrintJustified( widget.font, color, false, rect, widget.justifyH, widget.justifyV, text.c_str(),
		widget.fontAngle, widget.charSpacing, widget.lineSpacing, widget.fontScale, widget.fontScale );
	(void)menu;
}

/* The class draws, each at the widget's place in menu units and its alpha. */

/* UICheckBox::Draw (0x004e0af0): a raised box unless pressed, the checked or
 * unchecked picture (blue or black without one) at the left, centred, and the
 * title after it. */
void DrawCheckBox( Menu &menu, int index, float x, float y, float alpha ) {
	Widget &widget = menu.widgets[index];
	UpdateData( menu, index );
	if ( !widget.check.depressed ) Box3D( x, y, widget.w, widget.h, false, widget.border3d, alpha );
	const float color[4] = { widget.fg[0], widget.fg[1], widget.fg[2], widget.fg[3] * alpha };
	const float boxY = y + ( widget.h - mw::CheckSize ) * 0.5f;
	const std::string &shader = widget.check.checked ? widget.check.checkedShader : widget.check.uncheckedShader;
	if ( !shader.empty() ) {
		VirtualPic( x, boxY, mw::CheckSize, mw::CheckSize, color, CL_AliceShader( shader.c_str() ) );
	} else {
		// UBlue (0x00aa6fd0) or UBlack (0x00aa7010).
		const float fill[4] = { 0.0f, 0.0f, widget.check.checked ? 1.0f : 0.0f, alpha };
		VirtualFill( x, boxY, mw::CheckSize, mw::CheckSize, fill );
	}
	if ( !widget.title.empty() ) {
		const float titleY = ( widget.h - static_cast<float>( FontHeight( widget ) ) ) * 0.5f - 1.0f;
		WidgetPrint( widget, widget.fg, x + mw::CheckSize, y + titleY, widget.title.c_str(), widget.fontAngle,
			widget.charSpacing, widget.lineSpacing );
	}
}

/* UISlider::Draw (0x004db440), always opaque: the bar between the arrows, the
 * thumb on it, and the two arrows, pressed or not. */
void DrawSlider( Menu &menu, int index, float x, float y ) {
	Widget &widget = menu.widgets[index];
	UpdateUIElement( menu, index );
	static const float white[4] = { 1, 1, 1, 1 };
	const mw::Slider &s = widget.slider.s;
	const float arrow = static_cast<float>( s.arrowWidth );
	const float centre = mw::ThumbCenter( s, mw::TrackFor( s, x, widget.w ) );
	VirtualPic( x + arrow, y, widget.w - 2.0f * arrow, widget.h, white, CL_AliceShader( "ui/control/hslider_bar" ) );
	VirtualPic( x + centre - s.thumbWidth * 0.5f, y, s.thumbWidth, widget.h, white,
		CL_AliceShader( "ui/control/hslider_indicator" ) );
	VirtualPic( x, y, arrow, widget.h, white, CL_AliceShader( widget.slider.leftPressed
		? "ui/control/hslider_prev_pressed" : "ui/control/hslider_prev" ) );
	VirtualPic( x + widget.w - arrow, y, arrow, widget.h, white, CL_AliceShader( widget.slider.rightPressed
		? "ui/control/hslider_next_pressed" : "ui/control/hslider_next" ) );
}

/* UIList::Draw (0x004df190): the item justified across the widget and the
 * arrows inset by 2 at its ends. */
void DrawList( Menu &menu, int index, float x, float y ) {
	Widget &widget = menu.widgets[index];
	UpdateUIElement( menu, index );
	const int current = widget.list.current;
	if ( current > 0 && current <= static_cast<int>( widget.list.items.size() ) ) {
		WidgetPrintJustified( widget, widget.fg, x, y, widget.w, widget.h,
			mw::ListShows( widget.list.items[current - 1] ).c_str() );
	}
	static const float white[4] = { 1, 1, 1, 1 };
	const float arrow = mw::ListArrowWidth;
	VirtualPic( x + 2.0f, y + 2.0f, arrow - 4.0f, widget.h - 4.0f, white, CL_AliceShader( widget.list.previousPressed
		? "ui/control/hslider_prev_pressed" : "ui/control/hslider_prev" ) );
	VirtualPic( x + widget.w - arrow + 2.0f, y + 2.0f, arrow - 4.0f, widget.h - 4.0f, white,
		CL_AliceShader( widget.list.nextPressed ? "ui/control/hslider_next_pressed" : "ui/control/hslider_next" ) );
}

/* UIField::Draw (0x004df7c0): the text from the first character that keeps
 * the cursor in view, 10 units in, and a cursor blinking every 250 ms while
 * the field has the keys. */
void DrawField( Menu &menu, int index, float x, float y ) {
	Widget &widget = menu.widgets[index];
	UpdateData( menu, index );
	const font::Font *metrics = CL_AliceFontMetrics( widget.font );
	const std::string &text = widget.field.text;
	const int cursor = widget.field.cursor;
	const float indent = 10.0f;      // +0x138 (0x004bb1b6)
	const float vertical = 5.0f;     // +0x13c (0x004bb1c0)
	const int start = mw::FieldFirstVisible( text, cursor, indent, widget.w,
		static_cast<float>( font::CharWidth( metrics, '|' ) ),
		[metrics]( const char *part ) { return font::StringWidth( metrics, part ); } );
	float caretX = indent - 1.0f;
	for ( int i = start; i < cursor; ++i ) caretX += static_cast<float>( font::CharWidth( metrics, text[i] ) );
	if ( caretX > widget.w ) caretX = widget.w;
	const float room = widget.h - static_cast<float>( font::LineHeight( metrics ) );
	const float textY = static_cast<float>( static_cast<int>( 2.0f * vertical > room ? room * 0.5f : room - vertical ) );
	WidgetPrint( widget, widget.fg, x + indent, y + textY, text.c_str() + start );
	if ( menu.focus == index && ( Now() / 250 ) & 1 ) WidgetPrint( widget, widget.fg, x + caretX, y + textY, "|" );
}

/* UIAutoScroll::Draw (0x004c0a40): lines a font height plus the line spacing
 * apart moving up at scrollspeed lines a second, pictures among them, and
 * both faded over 40 units at the edges; forcescissor clips the rest. */
void DrawAutoScroll( Widget &widget, float x, float y ) {
	const float lineHeight = static_cast<float>( FontHeight( widget ) ) + widget.lineSpacing;
	widget.scroll.position -= cls.frametime * 0.001f * widget.scroll.speed * lineHeight;
	const float clip[4] = { x, y, widget.w, widget.h };
	const float *scissor = widget.forceScissor ? clip : nullptr;
	for ( const mw::ScrollImage &image : widget.scroll.images ) {
		const float top = lineHeight * image.line + widget.scroll.position;
		const float color[4] = { 1, 1, 1, image.fade ? mw::EdgeAlpha( top, top + image.h, widget.h ) : 1.0f };
		ClippedPic( x + image.x, y + top, image.w, image.h, color, CL_AliceShader( image.shader.c_str() ), scissor );
	}
	float top = widget.scroll.position;
	for ( std::size_t i = 0; i < widget.scroll.lines.size() && top < widget.h; ++i, top += lineHeight ) {
		if ( !( top >= -lineHeight ) ) continue;
		// The edge fade replaces the text colour's alpha (0x004c0cac, 0x004c0d02).
		widget.fg[3] = mw::EdgeAlpha( top, top + lineHeight, widget.h );
		WidgetPrintJustified( widget, widget.fg, x, y + top, widget.w, lineHeight, widget.scroll.lines[i].c_str(),
			widget.fontAngle, widget.charSpacing, widget.lineSpacing );
	}
}

/* One bind list cell (a UILabel or UIBindButton the list made): its
 * background and its text in its colours. */
void DrawBindCell( const Widget &list, float x, float y, float w, float h, const mw::Rgba &fg, const mw::Rgba &bg,
	const char *text, bool nameColumn, float scale, float alpha ) {
	if ( bg.a != 0.0f ) {
		const float fill[4] = { bg.r, bg.g, bg.b, bg.a * bg.a * alpha };
		VirtualFill( x, y, w, h, fill );
	}
	if ( !text || !text[0] ) return;
	const float color[4] = { fg.r, fg.g, fg.b, fg.a };
	const font::Rect rect{ x, y, w, h };
	if ( nameColumn ) {
		// The row's label: left and centred, spacing 0.15, scale 0.9 (0x004e3f68..0x004e3f94).
		CL_AliceFontPrintJustified( list.font, color, false, rect, 1, 1, text, 0.0f, 0.15f, 0.0f, 0.9f, 0.9f );
	} else {
		// The key button: centred, at 0.85 times the key's own scale, 1 unit in (0x004d1f4f..0x004d20db).
		const font::Rect inset{ x + 1.0f, y + 1.0f, w, h };
		CL_AliceFontPrintJustified( list.font, color, false, inset, 0, 1, text, 0.0f, 0.0f, 0.0f, 0.85f * scale,
			0.85f * scale );
	}
}

/* UIFakkBindList::Draw (0x004e4960) with its cells and its scrollbar
 * (UIVertScroll::Draw 0x004d87b0). */
void DrawBindList( Menu &menu, Widget &widget, float x, float y, float alpha ) {
	BindState &bind = widget.bind;
	if ( !bind.built ) return;
	const mw::BindLayout &layout = bind.layout;
	// The chosen row's animation plays once it has stood 200 ms.
	if ( bind.row != bind.animRow ) {
		bind.animWait = 0;
		bind.animRow = bind.row;
	}
	if ( bind.animWait != -1 ) {
		if ( bind.animWait < 200 ) {
			bind.animWait += cls.frametime;
		} else {
			PlayRowAnim( menu, widget );
			bind.animWait = -1;
		}
	}
	const float cw = static_cast<float>( layout.commandWidth ), ch = static_cast<float>( layout.commandHeight );
	const float kw = static_cast<float>( layout.primaryKeyWidth ), kh = static_cast<float>( layout.primaryKeyHeight );
	// The two headers (0x004e3c0c..0x004e3e92): raised, courier-14, the title colours.
	const mw::BorderColor titleBorder = mw::MakeBorderColor( layout.titleBackground );
	DrawBindCell( widget, x, y, cw, ch, layout.titleForeground, layout.titleBackground, layout.headers[0].c_str(),
		true, 1.0f, alpha );
	Box3D( x, y, cw, ch, false, titleBorder, alpha );
	DrawBindCell( widget, x + cw, y, kw, ch, layout.titleForeground, layout.titleBackground, layout.headers[1].c_str(),
		true, 1.0f, alpha );
	Box3D( x + cw, y, kw, ch, false, titleBorder, alpha );
	for ( int row = 0; row < static_cast<int>( layout.items.size() ); ++row ) {
		if ( !mw::RowVisible( row, bind.top, layout.commandHeight, widget.h ) ) continue;
		const float rowY = y + static_cast<float>( mw::VisibleRowTop( row, bind.top, layout.commandHeight ) );
		for ( int column = 1; column <= 2; ++column ) {
			mw::Rgba fg = layout.inactiveForeground, bg = layout.inactiveBackground;
			if ( row == bind.row ) {
				fg = layout.highlightForeground;
				bg = layout.highlightBackground;
				if ( column == bind.column ) {
					fg = bind.capturing ? layout.activeForeground : layout.selectForeground;
					bg = bind.capturing ? layout.activeBackground : layout.selectBackground;
				}
			}
			if ( column == 1 ) {
				DrawBindCell( widget, x, rowY, cw + kw, ch, fg, bg, layout.items[row].name.c_str(), true, 1.0f, alpha );
				continue;
			}
			RowKeys( bind, row );
			const int key = bind.key1[row];
			const char *name = key >= 0 ? Key_KeynumToString( key ) : "None";
			const mw::KeyAlias *alias = mw::FindKeyAlias( keyAliases, name );
			const float scale = alias && alias->scale != -1.0f ? alias->scale : 1.0f;
			DrawBindCell( widget, x + cw, rowY, kw, kh, fg, bg, alias ? alias->name.c_str() : name, false, scale, alpha );
			if ( bind.capturing && row == bind.row ) {
				// The waiting button's 2-unit solid border (0x004d1b9d), in the active border colour.
				const mw::Rgba &edge = layout.activeBorder;
				const float rgba[4] = { edge.r, edge.g, edge.b, edge.a };
				VirtualFill( x + cw, rowY, kw, 2.0f, rgba );
				VirtualFill( x + cw, rowY + kh - 2.0f, kw, 2.0f, rgba );
				VirtualFill( x + cw, rowY, 2.0f, kh, rgba );
				VirtualFill( x + cw + kw - 2.0f, rowY, 2.0f, kh, rgba );
			}
		}
	}
	// The scrollbar: its bar, its arrows (the lower one flipped) and its thumb.
	static const float white[4] = { 1, 1, 1, 1 };
	const float barX = x + widget.w - ScrollbarWidth;
	VirtualPic( barX, y + ScrollbarWidth, ScrollbarWidth, widget.h - 32.0f, white,
		CL_AliceShader( "ui/control/slider2_bar.tga" ) );
	const qhandle_t arrow = CL_AliceShader( "ui/control/slider2_top.tga" );
	VirtualPic( barX, y, ScrollbarWidth, ScrollbarWidth, white, arrow );
	VirtualPic( barX, y + widget.h - ScrollbarWidth, ScrollbarWidth, ScrollbarWidth, white, arrow, 0.0f, 1.0f, 1.0f, 0.0f );
	if ( static_cast<int>( layout.items.size() ) > bind.page ) {
		float offset, length;
		ThumbFor( widget, offset, length );
		VirtualPic( barX, y + offset, ScrollbarWidth, length, white, CL_AliceShader( "ui/control/slider2_indicator.tga" ) );
	}
}

void DrawWidget( Menu &menu, int index, float parentAlpha ) {
	Widget &widget = menu.widgets[index];
	if ( !widget.shown ) return;
	// Waits fire only while their widget is drawn (0x004bd740).
	for ( Wait &wait : widget.waits ) {
		if ( wait.fireAt >= 0 && Now() >= wait.fireAt ) {
			wait.fireAt = -1;
			ExecuteLine( wait.command );
		}
	}
	if ( widget.neverDraw ) return;
	const float alpha = parentAlpha * WidgetFade( widget );
	if ( alpha <= 0.0f ) return;
	float vx, vy;
	WidgetPosition( menu, widget, vx, vy );
	const bool hovered = menu.hovered == index;
	const bool pressed = menu.pressed == index && hovered;

	// The background fill takes its alpha twice (0x004bd8ea, then DrawBox's 0x004b59a0).
	if ( widget.bg[3] != 0.0f ) {
		const float bg[4] = { widget.bg[0], widget.bg[1], widget.bg[2], widget.bg[3] * widget.bg[3] * alpha };
		VirtualFill( vx, vy, widget.w, widget.h, bg );
	}
	// A pressed button's border sinks; one drawn with none stays without.
	DrawBorder( vx, vy, widget.w, widget.h, pressed && widget.border != 0 ? 2 : widget.border, widget.border3d, alpha );
	const float shade[4] = { 1, 1, 1, widget.fg[3] * alpha };
	if ( !widget.shader.empty() ) VirtualPic( vx, vy, widget.w, widget.h, shade, CL_AliceShader( widget.shader.c_str() ) );
	if ( !widget.staticImage.empty() ) {
		VirtualPic( vx, vy, widget.w, widget.h, shade, CL_AliceShader( widget.staticImage.c_str() ) );
	}
	if ( hovered && !widget.hoverShader.empty() ) {
		VirtualPic( vx, vy, widget.w, widget.h, shade, CL_AliceShader( widget.hoverShader.c_str() ) );
	}
	switch ( widget.kind ) {
	case WidgetKind::Label:
	case WidgetKind::Button:
		DrawWidgetText( menu, widget, vx, vy, alpha, pressed );
		break;
	case WidgetKind::CheckBox:
		DrawCheckBox( menu, index, vx, vy, alpha );
		break;
	case WidgetKind::Slider:
		DrawSlider( menu, index, vx, vy );
		break;
	case WidgetKind::List:
		DrawList( menu, index, vx, vy );
		break;
	case WidgetKind::Field:
		DrawField( menu, index, vx, vy );
		break;
	case WidgetKind::AutoScroll:
		DrawAutoScroll( widget, vx, vy );
		break;
	case WidgetKind::BindList:
		DrawBindList( menu, widget, vx, vy, alpha );
		break;
	case WidgetKind::Alice3D:
		DrawAlice3D( widget );
		break;
	case WidgetKind::LoadGame:
		break;
	}
}

void DrawMenu( Menu &menu ) {
	if ( !menu.visible || Now() < menu.showStart ) return;
	if ( menu.hasBgFill && menu.bgFill[3] != 0.0f ) {
		CL_AliceFill( 0, 0, static_cast<float>( cls.glconfig.vidWidth ), static_cast<float>( cls.glconfig.vidHeight ),
			menu.bgFill );
	}
	const float x = MenuOriginX( menu ), y = MenuOriginY( menu );
	if ( menu.bg[3] != 0.0f ) {
		const float bg[4] = { menu.bg[0], menu.bg[1], menu.bg[2], menu.bg[3] * menu.bg[3] };
		VirtualFill( x, y, menu.w, menu.h, bg );
	}
	DrawBorder( x, y, menu.w, menu.h, menu.border, menu.border3d, 1.0f );
	for ( std::size_t i = 0; i < menu.widgets.size(); ++i ) {
		DrawWidget( menu, static_cast<int>( i ), 1.0f );
	}
	if ( menu.fadeOverlay > 0.0f ) {
		// The menu's colour over it, fading from opaque to clear as it opens
		// and back as it closes (0x004bdb0c..0x004bdb9c).
		const int start = menu.hiding ? menu.hideStart : menu.showStart;
		float t = ( Now() - start ) / ( menu.fadeOverlay * 1000.0f );
		t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
		const float a = menu.hiding ? t : 1.0f - t;
		if ( a > 0.0f ) {
			const float overlay[4] = { menu.bg[0], menu.bg[1], menu.bg[2], a * menu.bg[3] };
			VirtualFill( x, y, menu.w, menu.h, overlay );
		}
	}
	// The bind list's press-any-key picture is a desktop widget above the menu.
	for ( const Widget &widget : menu.widgets ) {
		if ( widget.kind == WidgetKind::BindList && widget.shown ) DrawPressAnyKey( widget );
	}
}

/* ---- the hosted module ---------------------------------------------------- */

void Init() {
	RegisterCvars();
	RegisterCommands();
	CL_AliceScreenInit();
	CL_AliceLoadingInit();
	CL_AliceHudInit();
	cursorShader = CL_AliceShader( "gfx/2d/mouse_arrow" );
	menus.clear();
	menuStack.clear();
	captureMenu = captureWidget = -1;
	pendingSave.active = false;
	LoadTables();
	LoadMenus();
	uiInitialized = true;

	if ( !firstInitDone ) {
		firstInitDone = true;
		/* UI_Init's last act (0x00410ced): the intro, or with developer set
		 * the console instead. Retail's UI starts inside Com_Init, before the
		 * command line's commands are queued, so its `dointro` runs ahead of
		 * them; this client starts its UI on the first frame, after they are
		 * queued, so the intro goes to the front to keep that order. A map the
		 * command line already started gives the intro up (intro state -1),
		 * as the map would have in retail. */
		if ( !Cvar_VariableIntegerValue( "developer" ) ) {
			if ( introState != -1 && !CL_AliceLoadingActive() ) {
				introQueued = true;
				Cbuf_ExecuteText( EXEC_INSERT, "dointro\n" );
			}
		} else if ( !( Key_GetCatcher() & KEYCATCH_CONSOLE ) && !CL_AliceLoadingActive() ) {
			Con_ToggleConsole_f();
		}
		return;
	}
	// This client restarts its modules with the renderer; reopen what was open.
	if ( !CL_AliceLoadingActive() ) {
		for ( const std::string &name : savedStack ) PushMenu( name.c_str(), true );
	}
	savedStack.clear();
}

void Shutdown() {
	savedStack.clear();
	for ( int index : menuStack ) savedStack.push_back( menus[index].name );
	// The commands stay registered; see RegisterCommands.
	EndCapture();
	pendingSave.active = false;
	viewHasMouse = false;
	CL_AliceScreenShutdown();
	CL_AliceHudShutdown();
	/* Retail's UI never goes down with vid_restart and nothing but UI_Update
	 * and UI_EndLoad touches its pause state. This UI restarts with the
	 * renderer, so it gives back only the pause its menu took, by the same
	 * rule; the menu it restores pauses again once the game is back in play. */
	ReleaseMenuPause();
	if ( cursorModeOn ) {
		CL_AliceSetCursorMode( qfalse );
		cursorModeOn = false;
	}
	cursorButtons = 0;
	menus.clear();
	menuStack.clear();
	uiInitialized = false;
}

/* UI_SET_ACTIVE_MENU from the client's shared paths. */
void SetActiveMenu( int menu ) {
	switch ( menu ) {
	case UIMENU_NONE:
		/* Retail closes menus only through its own commands; a cinematic
		 * starting leaves them (New Game's `cinematic opening.roq;
		 * togglemenu newgame` closes them itself). */
		return;
	case UIMENU_MAIN:
		// A logo stopped on the way here has appended `_intro` at state 100
		// (CIN_Stop 0x00425770); that brings the main menu.
		if ( IntroRunning() ) return;
		// The drop path (0x0042ac50): loading ends, then the console for a
		// developer or the main menu.
		if ( cls.state <= CA_DISCONNECTED ) CL_AliceLoadingEnd();
		if ( Cvar_VariableIntegerValue( "developer" ) ) {
			if ( !( Key_GetCatcher() & KEYCATCH_CONSOLE ) && cls.state <= CA_DISCONNECTED ) Con_ToggleConsole_f();
			return;
		}
		if ( !CurrentMenu() ) PushMenu( "main", false );
		return;
	case UIMENU_INGAME:
		// Escape reaches the menus through CL_AliceEscape; this request comes
		// from paths that want a menu up, such as a video restart reopening
		// what was open, so it only ever opens `main`.
		if ( !CurrentMenu() && !EscapeBlockedByIntro() ) PushMenu( "main", false );
		return;
	default:
		return;
	}
}

intptr_t QDECL CL_AliceUIEntry( int command, ... ) {
	int arg[3] = {};
	va_list ap;
	va_start( ap, command );
	for ( int &value : arg ) value = va_arg( ap, int );
	va_end( ap );

	switch ( command ) {
	case UI_GETAPIVERSION:
		// No module speaks this boundary; the client does not check it.
		return 0;
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
	case UI_IS_FULLSCREEN:
		return CL_AliceUIFullscreenMenuUp() ? qtrue : qfalse;
	case UI_SET_ACTIVE_MENU:
		SetActiveMenu( arg[0] );
		return 0;
	default:
		return 0;
	}
}

} // namespace

/*
====================
CL_AliceLoadUI

The engine's own UI as the client's UI module.
====================
*/
vm_t *CL_AliceLoadUI( void ) {
	return VM_CreateHosted( VM_UI, CL_AliceUIEntry );
}

/*
====================
CL_AliceUIFrame
====================
*/
void CL_AliceUIFrame( void ) {
	/* The intro's queued step and the logo it waits on. Retail queues its
	 * steps as "wait N; _intro" at the end of the command buffer (0x0040fd85),
	 * where the wait holds every command behind it; here the intro keeps its
	 * own time and puts the step at the front, so a console command is not
	 * held up behind the intro's five-second waits. The step itself still
	 * runs from the command buffer, outside the frame being drawn. */
	if ( introNextStep >= 0 && Now() >= introNextStep ) {
		introNextStep = -1;
		Cbuf_ExecuteText( EXEC_INSERT, "_intro\n" );
	}
	/* A logo continues the intro itself: its end sets the state its flag
	 * named and appends `_intro` (cl_cin.cpp, CIN_Finish 0x004257a0). */

	for ( Menu &menu : menus ) {
		if ( menu.hiding && Now() >= menu.hideEnd ) FinishHide( menu );
	}

	SaveFinished();

	/* Cursor mode: on while a menu is up (the push path's 0x00427b00), off
	 * once none remains or ui_hidemouse gives the view the mouse (0x00427b50).
	 * While it is on the pointer moves only the cursor and the mouse buttons
	 * stop at its mask, so the menus read both from there (0x004282b0) and
	 * the window manager's events follow from them (0x004b3a50). */
	const bool wantCursor = !menuStack.empty() && !viewHasMouse;
	if ( wantCursor != cursorModeOn ) {
		CL_AliceSetCursorMode( wantCursor ? qtrue : qfalse );
		cursorModeOn = wantCursor;
		cursorButtons = 0;
	}
	if ( cursorModeOn ) {
		int x = 0, y = 0, buttons = 0;
		CL_AliceGetMouseState( &x, &y, &buttons );
		cursorX = static_cast<float>( x );
		cursorY = static_cast<float>( y );
		UpdateHover();
		const float vx = VirtualCursorX(), vy = VirtualCursorY();
		Menu *menu = CurrentMenu();
		if ( menu && ( buttons & cursorButtons & 1 ) && ( vx != lastVirtualX || vy != lastVirtualY ) ) {
			MouseDragged( *menu );
		}
		lastVirtualX = vx;
		lastVirtualY = vy;
		if ( ( buttons ^ cursorButtons ) & 1 ) {
			KeyEvent( K_MOUSE1, ( buttons & 1 ) != 0 );
		}
		// The other buttons matter only to a key button waiting for a key.
		for ( int button = 1; button < 5; ++button ) {
			if ( ( buttons & ~cursorButtons & ( 1 << button ) ) && Capturing() ) CaptureMouse( K_MOUSE1 + button );
		}
		cursorButtons = buttons;
	} else {
		UpdateHover();
	}
	if ( Menu *menu = CurrentMenu() ) {
		for ( Widget &widget : menu->widgets ) {
			if ( widget.kind == WidgetKind::Slider ) SliderRepeat( *menu, widget );
		}
	}

	/* A full-screen menu pauses the game once and remembers whether it took
	 * the pause (1) or found the game paused already (2); with play or a
	 * cinematic back on screen it gives back only its own pause. None of it
	 * runs while the loading screen is up (UI_Update 0x0040c8cb, 0x0040c92e..
	 * 0x0040c95e, 0x0040c9b3..0x0040c9dd, 0x0040c9fb..0x0040ca15). Retail
	 * decides with no connection state; this engine takes the pause only in
	 * play, because its pause stops the local server's snapshots
	 * (SV_CheckPaused) and one taken before the first would never be given
	 * back. */
	if ( !CL_AliceLoadingActive() ) {
		if ( CL_AliceUIFullscreenMenuUp() ) {
			if ( uiPauseState == 0 && cls.state == CA_ACTIVE ) {
				if ( !Cvar_VariableIntegerValue( "paused" ) ) {
					TogglePause();
					uiPauseState = 1;
				} else {
					uiPauseState = 2;
				}
			}
		} else if ( cls.state == CA_ACTIVE || cls.state == CA_CINEMATIC ) {
			ReleaseMenuPause();
		}
	}

	/* UI_Update gives the intro up on every frame that a full-screen menu is
	 * current (0x0040c933) or, with none, the game is in play (0x0040c9b8),
	 * but not while the loading screen is up: it draws that and returns
	 * first (0x0040c8cb). So Escape with developer off is refused only while
	 * the intro itself runs (CL_KeyEvent 0x00429dbe) -- not at the main menu,
	 * and not in New Game's opening movie, which any key then stops. */
	if ( !CL_AliceLoadingActive() && ( CL_AliceUIFullscreenMenuUp() || cls.state == CA_ACTIVE ) ) {
		introState = -1;
	}

	// The menus own the keys while one is up, and while disconnected, where
	// the intro and the menus are all there is. A movie with no menu up
	// leaves them free, so any key becomes Escape (CL_KeyEvent 0x00429d66),
	// whose rules then refuse it during the intro's logos with developer off.
	const bool wantKeys = ( !menuStack.empty() && !viewHasMouse ) || cls.state == CA_DISCONNECTED;
	const int catcher = Key_GetCatcher();
	if ( wantKeys && !( catcher & KEYCATCH_UI ) ) {
		Key_SetCatcher( catcher | KEYCATCH_UI );
	} else if ( !wantKeys && ( catcher & KEYCATCH_UI ) ) {
		Key_SetCatcher( catcher & ~KEYCATCH_UI );
	}
}

bool CL_AliceUIFullscreenMenuUp( void ) {
	const Menu *menu = uiInitialized ? CurrentMenu() : nullptr;
	return menu && menu->fullscreen;
}

bool CL_AliceUIMenuIsUp( void ) {
	return uiInitialized && !menuStack.empty();
}

qboolean CL_AliceUIMenuUp( void ) {
	return CL_AliceUIMenuIsUp() ? qtrue : qfalse;
}

void CL_AliceUIDrawMenus( void ) {
	for ( Menu &menu : menus ) DrawMenu( menu );
}

void CL_AliceUIDrawIntroPlate( void ) {
	if ( introState <= 0 || introPlate.image.empty() ) return;
	const qhandle_t shader = CL_AliceShader( introPlate.image.c_str() );
	if ( !shader ) return;
	int w = 0, h = 0;
	CL_AliceShaderSize( shader, &w, &h );
	const float scale = introPlate.scale != 0.0f ? introPlate.scale : 1.0f;
	const float width = w * scale, height = h * scale;
	const float alpha = screen::IntroPlateAlpha( introPlate.from, introPlate.to, introPlate.lengthMs,
		static_cast<float>( Now() - introPlate.start ) );
	const float color[4] = { 1, 1, 1, alpha };
	CL_AliceDrawPic( ( cls.glconfig.vidWidth - width ) * 0.5f, ( cls.glconfig.vidHeight - height ) * 0.5f,
		width, height, color, shader );
}

void CL_AliceUIDrawCursor( void ) {
	if ( menuStack.empty() || !cursorShader || cursorX < 0 ) return;
	int w = 0, h = 0;
	CL_AliceShaderSize( cursorShader, &w, &h );
	static const float white[4] = { 1, 1, 1, 1 };
	CL_AliceDrawPic( cursorX, cursorY, static_cast<float>( w ), static_cast<float>( h ), white, cursorShader );
}

/* UI_EndLoad (0x0040ecf0): the load is over, so the menu's pause state is
 * forgotten and the game unpaused whoever paused it. */
void CL_AliceUIReleasePause( void ) {
	uiPauseState = 0;
	if ( Cvar_VariableIntegerValue( "paused" ) ) TogglePause();
}

void CL_AliceUIClearMenus( bool force ) {
	ClearMenus( force );
}

int CL_AliceUIIntroState( void ) {
	return introState;
}

void CL_AliceUISetIntroStateInternal( int state ) {
	introState = state;
	if ( state == -1 ) {
		introNextStep = -1;
		introQueued = false;
		introPlate = {};
	}
}

/* UI_SetIntroState (0x0040fc50), from the end of a cinematic (CIN_Finish
 * 0x00425860): the state its flags named, or 100. The `_intro` that follows
 * is the cinematic's to append. */
void CL_AliceUISetIntroState( int state ) {
	introState = state;
}

int CL_AliceUIGetIntroState( void ) {
	return introState;
}

/* Escape with no menu holding the keys, from the client's key handler. */
void CL_AliceEscape( void ) {
	EscapeKey();
}

/* togglemenu <name> (0x0040cbb0): close the menus if it is the current one,
 * otherwise close them and open it. */
void CL_AliceToggleMenu_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: togglemenu <menuname>\n" );
		return;
	}
	const char *name = Cmd_Argv( 1 );
	const Menu *current = CurrentMenu();
	if ( current && !Q_stricmp( current->name.c_str(), name ) ) {
		ClearMenus( false );
		return;
	}
	if ( current ) {
		ClearMenus( false );
		PushMenu( name, true );
		return;
	}
	PushMenu( name, false );
}
