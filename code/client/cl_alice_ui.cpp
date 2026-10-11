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
 * and those Heavy Metal: F.A.K.K.2's menus add, on the same engine UI:
 *
 *   FakkLabel          what every Label is (the loader's remap 0x0040e240): a
 *                      stat as a number or a bar with its flash, or the item
 *                      in a hand as its name or its model (Draw 0x0041ef00,
 *                      the bar 0x0041eb90) -- F.A.K.K.2's HUD menus, shown by
 *                      its engine's rule (fakk2.exe 1.02 0x0040b29b..0x0040b326;
 *                      code/compat/fakk2_hud.hpp)
 *   PulldownMenuContainer  an arrow whose list opens on the press and runs the
 *                      row let go over (0x004e2940, 0x004e2ad0, 0x004e2960;
 *                      the list's draw 0x004e58c0 and choice 0x004e5b40)
 *   MultiLineEdit      its lines, selection and keys (draw 0x004dc4f0, keys
 *                      0x004dd180, characters 0x004dd810)
 *   linkstring         a linked label's value shown as words (0x004d5810)
 *   aliascache         a hover sound's aliases (0x004baa30)
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
#include "../compat/alice_picker.hpp"
#include "../compat/alice_savegame.hpp"
#include "../compat/alice_state_adapter.hpp"
#include "../compat/alice_urc.hpp"
#include "../compat/fakk2_hud.hpp"
#include "../compat/fakk2_inventory.hpp"
#include "../qcommon/alice_tiki.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <ctime>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {

namespace urc = fntech3::alice::urc;
namespace screen = fntech3::alice::screen;
namespace font = fntech3::alice::ritualfont;
namespace mw = fntech3::alice::menu;
namespace noise = fntech3::alice::noise;
namespace picker = fntech3::alice::picker;
namespace alicesave = fntech3::alice::savegame;
namespace fakkhud = fntech3::fakk2::hud;
namespace fakkinv = fntech3::fakk2::inventory;

enum class WidgetKind {
	Label, Button, CheckBox, Slider, Field, List, BindList, LoadGame, AutoScroll, Alice3D, Pulldown, MultiLineEdit
};

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

/* UIVertScroll (0x004d80b0) as a press holds it: the part pressed (+0x1f8) --
 * 1 the up arrow, 2 the down arrow, 3 the thumb, 4 below the thumb, 5 above
 * it (0x004d8f49..0x004d9006) -- the scrollrate its posted _scrollbar_scroll
 * carries (0x004d90bc) and the cls.realtime that event is due at, -1 for
 * none; and for the thumb its grab in items (+0x218) and the position at the
 * press (+0x21c). The bind list's, the text box's and the save list's
 * scrollbars are all one (0x004e3856, 0x004dc379, 0x004d3e98). */
struct ScrollHold {
	int part = 0;
	int rate = 0;
	int dueAt = -1;
	int grab = 0;
	int topAtPress = 0;
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
	ScrollHold hold;      // its scrollbar's press
	int model = -1;       // the menu's `alice3d` widget (+0x200)
};

/* UIPulldownMenuContainer (0x004d62a0) and the UIPulldownMenu it makes
 * (0x004e18c0). Each entry is a uipull_describe (0x004e1d70), made on the
 * first mention of its name, with its list's colours, its arrows and its
 * items (uipopup_describe 0x004cb8f0), which keep the .urc's order. */
struct PopupItem {
	std::string title;
	int type = 0;          // UI_PopupTypeStringToInt (0x004e6350)
	std::string data;
};

struct PulldownEntry {
	std::string name;
	float highlightFg[4] = { 1, 1, 1, 1 };          // +0x04, white (0x004e1eab..0x004e1f3a)
	float highlightBg[4] = { 0, 0, 1, 1 };          // +0x14, blue
	float fg[4] = { 0, 0, 0, 1 };                   // +0x24, black
	float bg[4] = { 0.75f, 0.75f, 0.75f, 1 };       // +0x34, grey
	std::string shader, selectedShader;             // +0x44, +0x48
	std::vector<PopupItem> items;                   // +0x4c
};

struct PulldownState {
	std::vector<PulldownEntry> entries;
	float highlightBg[4] = { 0, 0, 1, 1 };   // the closed menu's +0x1e8
	float highlightFg[4] = { 1, 1, 1, 1 };   // +0x1f8
	int open = -1;                           // the open entry (+0x1e0), from 0
};

/* UIMultiLineEdit (0x004db980): its lines, the selection's two ends, its
 * scrollbar's top line, and whether it takes typing (+0x205). */
struct EditPoint {
	int line = 0, column = 0;
};

struct EditState {
	std::vector<std::string> lines = { std::string() };
	EditPoint begin, end;     // +0x1d0..+0x1dc
	int top = 0;              // the scrollbar's top line
	bool editable = true;     // `edit`
	bool changed = false;     // +0x206
	bool dragging = false;    // +0x200
	int dragRepeatAt = -1;    // the drag timer's next scroll (0x004dd080)
	ScrollHold hold;          // its scrollbar's press
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
	/* InitFrame's defaults (0x004cc2a1): 10, 10, 100 by 64, courier-14 --
	 * verdana-12 under F.A.K.K.2 (CL_RitualDefaultFont). */
	float x = 10, y = 10, w = 100, h = 64;
	/* UIWidget's colours (0x004bb01e): light grey behind, white in front. */
	float fg[4] = { 1, 1, 1, 1 };
	float bg[4] = { 0.75f, 0.75f, 0.75f, 1 };
	int border = 2;
	std::string title;
	std::string fontName = CL_RitualDefaultFont();
	int font = -1;
	int justifyH = 0, justifyV = 1;
	float fontScale = 1.0f, charSpacing = 0.0f, lineSpacing = 0.0f, fontAngle = 0.0f;
	float fontPosX = -1.0f, fontPosY = -1.0f;
	std::string shader, hoverShader, staticImage;
	/* `tileshader` sets the shader and the flag (8) that draws it tiled
	 * (0x004ba610; Display 0x004bd9e7 -> 0x004bcfe0) rather than stretched;
	 * `shader` sets it and clears the flag (0x004ba4b0). */
	bool tileShader = false;
	float fadeIn = 0.0f;
	bool inverseAlpha = false;
	urc::Direction direction = urc::Direction::None;
	float motionTime = 0.0f;
	float pointX = 0.0f, pointY = 0.0f;
	std::vector<Wait> waits;
	std::string stuffCommand, mouseEnterCommand, mouseExitCommand, clickSound, hoverSound, linkCvar, groupId;
	int orderNumber = 0;
	/* The flag F.A.K.K.2 draws a widget's hover picture by (UIWidget +0x120,
	 * Display 0x0049f103..0x0049f135); see LightsFollowKeys. */
	bool lit = false;
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
		/* F.A.K.K.2's pictures and their lit forms (fakk2.exe 1.02
		 * 0x004b4aa0), which `thumbshader`, `leftshader` and `rightshader`
		 * replace (0x004b4ec0, 0x004b5160, 0x004b5400). */
		std::string thumb, thumbSel, left, leftSel, right, rightSel;
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
		int current = 0;   // the drawn list's chosen row, 1-based, 0 for none
		int top = 0;       // its first row shown
		/* The last click (UIListCtrl +0x188..+0x194): its time, row and place. */
		int clickTime = -100000, clickRow = 0;
		float clickX = 0.0f, clickY = 0.0f;
		ScrollHold hold;   // its scrollbar's press (0x004d3e98)
	} load;
	/* UIAutoScroll (0x004c0340). */
	struct {
		std::vector<std::string> lines;
		std::vector<mw::ScrollImage> images;
		float speed = mw::DefaultScrollSpeed;
		float position = 0.0f;
	} scroll;
	ModelState model;
	/* UIFakkLabel (0x0041e0c0): the nine keywords, the bar's two materials
	 * (+0x214, +0x218), its flash, and the item a model label last found (a
	 * safe pointer in retail, +0x1e0; its name here). */
	struct {
		fakkhud::Label label;
		std::string barShader, flashShader;
		fakkhud::Flash flash;
		std::string item;
	} fakk;
	/* UILabel's linkstrings (0x004d54a0): a value and the words it shows. */
	std::vector<std::pair<std::string, std::string>> linkStrings;
	/* aliascache (0x004baa30): each alias, its sound and its parameters. */
	struct AliasCache {
		std::string name, path, parameters;
	};
	std::vector<AliasCache> aliases;
	PulldownState pulldown;
	EditState edit;

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
	/* `fadein` on the menu's own container (0x004b8120): the whole menu fades
	 * in over it, each widget taking its alpha (Display 0x004bd8b0). */
	float fadeIn = 0.0f;
	int align = 0;
	std::vector<std::string> showCommands, hideCommands;
	std::vector<KeyCommand> keyCommands;
	std::vector<Widget> widgets;

	/* Runtime. */
	bool visible = false;
	/* Shown as one of F.A.K.K.2's HUD menus rather than pushed. */
	bool hud = false;
	bool hiding = false;
	int showStart = 0;
	int hideStart = 0;
	int hideEnd = 0;
	int focus = -1;
	/* The order number of the widget that last took the keys, which the menu
	 * gives them back to when it comes up again (UIWidget +0x178). */
	int currentOrder = 0;
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
/* F.A.K.K.2's retail-exact menu dissolve (MenuCarry), off by default. */
cvar_t *ui_menuDissolve;

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
/* A pulldown's open list (UIPopupMenu 0x004e53f0): a top-level window over
 * every menu. Its owner, its rectangle in the menus' units and the row under
 * the cursor (+0x1d4), from 0. The pulldown holds the mouse while it is open
 * (the window manager's first responder, 0x004b3e70). */
struct {
	int menu = -1, widget = -1;
	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
	int selected = -1;
} popup;

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

/* The UI's local sound (0x0044ce10): a name the global alias list holds plays
 * one of its sounds, chosen as the list chooses (Alias_FindRandom
 * 0x004236c0); any other name is a sound's path. */
void PlayLocalSound( const std::string &name ) {
	if ( name.empty() ) return;
	const char *alias = AliceAlias_FindRandom( -1, name.c_str() );
	S_StartLocalSound( S_RegisterSound( alias ? alias : name.c_str(), qfalse ), CHAN_LOCAL_SOUND );
}

/* aliascache (0x004baa30): the alias into the global list, its sound cached
 * when its path names a .wav or an .mp3 (0x004bacd6..0x004bad0b). The level's
 * modules clear that list and this client's menus outlive a level, so each
 * menu adds its aliases again as it shows; adding one twice keeps the first. */
void AddAlias( const Widget::AliasCache &alias ) {
	AliceAlias_Add( -1, alias.name.c_str(), alias.path.c_str(), alias.parameters.c_str() );
	if ( strstr( alias.path.c_str(), ".wav" ) || strstr( alias.path.c_str(), ".mp3" ) ) {
		S_RegisterSound( alias.path.c_str(), qfalse );
	}
}

void AddAliases( const Widget &widget ) {
	for ( const Widget::AliasCache &alias : widget.aliases ) AddAlias( alias );
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

/* The value a latched cvar waits to take, when it differs from its value. The
 * buffer holds one character less than its size, so a longer value -- the
 * graphics report in r_gfxinfo -- that begins with it is the value itself. */
bool CvarLatched( const char *name, std::string &latched ) {
	char buffer[MAX_CVAR_VALUE_STRING];
	Cvar_LatchedVariableStringBufferSafe( name, buffer, sizeof( buffer ), 0 );
	if ( !strncmp( buffer, Cvar_VariableString( name ), sizeof( buffer ) - 1 ) ) return false;
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
	CL_RitualStretchPic( x * sx, y * sy, w * sx, h * sy, s0, t0, s1, t1, shader );
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

/* F.A.K.K.2's DrawBox with a one-unit edge (fakk2.exe 1.02 0x0049a650, flags
 * 3): the inside filled, then the left, top, right and bottom strips, here in
 * the black its scrollbars and list edges use (0x009b2a98). */
void EdgedBox( float x, float y, float w, float h, const float *fill, const float *edge = nullptr ) {
	static const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const float *line = edge ? edge : black;
	VirtualFill( x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fill );
	VirtualFill( x, y, 1.0f, h - 1.0f, line );
	VirtualFill( x + 1.0f, y, w - 1.0f, 1.0f, line );
	VirtualFill( x + w - 1.0f, y + 1.0f, 1.0f, h - 1.0f, line );
	VirtualFill( x, y + h - 1.0f, w - 1.0f, 1.0f, line );
}

/* The one-unit bevel F.A.K.K.2 draws inside a raised scrollbar part
 * (fakk2.exe 1.02 0x0049a4b0): light along the top and left, then dark along
 * the right and bottom. */
void Bevel( float x, float y, float w, float h, const mw::BorderColor &border ) {
	const float light[4] = { border.light.r, border.light.g, border.light.b, border.light.a };
	const float dark[4] = { border.dark.r, border.dark.g, border.dark.b, border.dark.a };
	VirtualFill( x, y, w, 1.0f, light );
	VirtualFill( x, y, 1.0f, h - 1.0f, light );
	VirtualFill( x + w - 1.0f, y + 1.0f, 1.0f, h - 1.0f, dark );
	VirtualFill( x, y + h - 1.0f, w, 1.0f, dark );
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
	if ( canonical == "UIPulldownMenuContainer" ) return WidgetKind::Pulldown;
	if ( canonical == "UIMultiLineEdit" ) return WidgetKind::MultiLineEdit;
	return WidgetKind::Label;
}

/* Keywords whose events change nothing this client draws; accepted quietly so
 * only a keyword no class knows is reported. */
bool IsQuietKeyword( const std::string &name ) {
	static const char *const kKeywords[] = {
		"allowactivate", "thumbshader", "leftshader", "rightshader", "loadfile", "stretch",
		"norescale", "noparentclip", "noparentadjust", "motiondelay", "stopsound",
		"bordercolor", "size", "align", "showcvar",
	};
	for ( const char *keyword : kKeywords ) {
		if ( urc::EqualsNoCase( name, keyword ) ) return true;
	}
	return false;
}

/* UI_PopupTypeStringToInt (0x004e6350): exact words; anything else is 0, and
 * a static event (1) or a submenu (3) cannot come from a .urc. */
enum PopupType { PopupNone = 0, PopupEventString = 2, PopupCommand = 4, PopupCvar = 5, PopupSeparator = 6 };

int PopupTypeFor( const std::string &word ) {
	if ( word == "event_string" ) return PopupEventString;
	if ( word == "command" ) return PopupCommand;
	if ( word == "cvar" ) return PopupCvar;
	if ( word == "separator" ) return PopupSeparator;
	return PopupNone;
}

/* A pulldown's entry by its exact name (getPulldown 0x004e1c50), made on its
 * first mention by addpopup, menushader or selmenushader (0x004e1d70,
 * 0x004e2070, 0x004e2370); the colour keywords name only an existing one
 * (0x004e31c8). */
PulldownEntry *FindPulldown( PulldownState &state, const std::string &name, bool create ) {
	for ( PulldownEntry &entry : state.entries ) {
		if ( entry.name == name ) return &entry;
	}
	if ( !create ) return nullptr;
	state.entries.emplace_back();
	state.entries.back().name = name;
	return &state.entries.back();
}

/* The words after the first as a colour, missing ones read as 0 (Event's
 * accessors); `from` is the first colour word. */
void ReadColorFrom( const std::vector<std::string> &args, std::size_t from, float *out ) {
	for ( std::size_t i = 0; i < 4; ++i ) out[i] = from + i < args.size() ? ToFloat( args[from + i] ) : 0.0f;
}

/* UIMultiLineEdit::setData (0x004dbd70): a line for each '\n', '\r' dropped,
 * no wrapping; the selection, the scrollbar and the changed flag start over. */
void EditSetText( EditState &edit, const char *text ) {
	edit.lines.assign( 1, std::string() );
	for ( const char *c = text ? text : ""; *c; ++c ) {
		if ( *c == '\n' ) edit.lines.emplace_back();
		else if ( *c != '\r' ) edit.lines.back().push_back( *c );
	}
	edit.begin = edit.end = EditPoint();
	edit.top = 0;
	edit.changed = false;
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
	else if ( is( "shader" ) ) { if ( !args.empty() ) { widget.shader = first; widget.tileShader = false; } }
	else if ( is( "tileshader" ) ) { if ( !args.empty() ) { widget.shader = first; widget.tileShader = true; } }
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
	/* F.A.K.K.2's picture and its lit form; a lone name serves as both. */
	else if ( Com_GameIsFAKK2() && ( is( "thumbshader" ) || is( "leftshader" ) || is( "rightshader" ) ) ) {
		if ( !args.empty() ) {
			std::string &normal = is( "thumbshader" ) ? widget.slider.thumb : is( "leftshader" ) ? widget.slider.left
				: widget.slider.right;
			std::string &lit = is( "thumbshader" ) ? widget.slider.thumbSel : is( "leftshader" ) ? widget.slider.leftSel
				: widget.slider.rightSel;
			normal = first;
			lit = args.size() > 1 ? args[1] : first;
		}
	}
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
	/* aliascache alias realPath [parameters] (0x004baa30), a UIWidget's. */
	else if ( is( "aliascache" ) ) {
		if ( args.size() >= 2 ) {
			widget.aliases.push_back( { args[0], args[1], urc::Join( args, 2 ) } );
			AddAlias( widget.aliases.back() );
		}
	}
	/* UIFakkLabel's (responses 0x0050d820), which only a Label is. A missing
	 * number reads as 0 and a missing shader as "", as Event's accessors give
	 * them (0x0041e243). */
	else if ( widget.kind == WidgetKind::Label && is( "playerstat" ) ) widget.fakk.label.stat = atoi( first.c_str() );
	else if ( widget.kind == WidgetKind::Label && is( "maxplayerstat" ) ) widget.fakk.label.maxStat = atoi( first.c_str() );
	else if ( widget.kind == WidgetKind::Label && is( "itemstat" ) ) widget.fakk.label.item = atoi( first.c_str() );
	else if ( widget.kind == WidgetKind::Label && is( "invmodelrender" ) ) widget.fakk.label.model = atoi( first.c_str() );
	else if ( widget.kind == WidgetKind::Label && is( "statbar" ) ) {
		// 0x0041e830: the range is kept before the orientation is read, and an
		// orientation it does not know leaves the one it had.
		if ( args.size() > 1 ) {
			widget.fakk.label.min = ToFloat( args[1] );
			widget.fakk.label.max = args.size() > 2 ? ToFloat( args[2] ) : 0.0f;
		}
		if ( urc::EqualsNoCase( first, "horizontal" ) ) widget.fakk.label.orientation = fakkhud::Orientation::Horizontal;
		else if ( urc::EqualsNoCase( first, "vertical" ) ) widget.fakk.label.orientation = fakkhud::Orientation::Vertical;
		else Com_DPrintf( "UIFakkLabel::LayoutPlayerStat: Invalid statbar orientation: \"%s\"\n", first.c_str() );
	}
	/* The tile variants set the widget's own tile flag, which `shader` clears
	 * (0x0041e3fe, 0x0041e6ae). */
	else if ( widget.kind == WidgetKind::Label && is( "statbar_shader" ) ) widget.fakk.barShader = first;
	else if ( widget.kind == WidgetKind::Label && is( "statbar_tileshader" ) ) {
		widget.fakk.barShader = first;
		widget.tileShader = true;
	}
	else if ( widget.kind == WidgetKind::Label && is( "statbar_shader_flash" ) ) widget.fakk.flashShader = first;
	else if ( widget.kind == WidgetKind::Label && is( "statbar_tileshader_flash" ) ) {
		widget.fakk.flashShader = first;
		widget.tileShader = true;
	}
	/* UILabel's linkstring value words (0x004d54a0): both read as text. */
	else if ( widget.kind == WidgetKind::Label && is( "linkstring" ) ) {
		widget.linkStrings.emplace_back( first, args.size() > 1 ? args[1] : std::string() );
	}
	/* UIPulldownMenuContainer's (responses 0x0052c228). */
	else if ( widget.kind == WidgetKind::Pulldown && is( "addpopup" ) ) {
		// menuname title type string (0x004d6630)
		PopupItem item;
		item.title = args.size() > 1 ? args[1] : std::string();
		item.type = PopupTypeFor( args.size() > 2 ? args[2] : std::string() );
		item.data = args.size() > 3 ? args[3] : std::string();
		FindPulldown( widget.pulldown, first, true )->items.push_back( std::move( item ) );
	}
	else if ( widget.kind == WidgetKind::Pulldown && is( "menushader" ) ) {
		FindPulldown( widget.pulldown, first, true )->shader = args.size() > 1 ? args[1] : std::string();
	}
	else if ( widget.kind == WidgetKind::Pulldown && is( "selmenushader" ) ) {
		FindPulldown( widget.pulldown, first, true )->selectedShader = args.size() > 1 ? args[1] : std::string();
	}
	else if ( widget.kind == WidgetKind::Pulldown && ( is( "popup_highlight_fgcolor" ) || is( "popup_highlight_bgcolor" )
		|| is( "popup_fgcolor" ) || is( "popup_bgcolor" ) ) ) {
		/* menu r g b a, by the handlers: the binary's documentation for the
		 * four is crossed (0x0052c4ec). */
		if ( PulldownEntry *entry = FindPulldown( widget.pulldown, first, false ) ) {
			float *color = is( "popup_highlight_fgcolor" ) ? entry->highlightFg
				: is( "popup_highlight_bgcolor" ) ? entry->highlightBg : is( "popup_fgcolor" ) ? entry->fg : entry->bg;
			ReadColorFrom( args, 1, color );
		}
	}
	else if ( widget.kind == WidgetKind::Pulldown && ( is( "highlight_fgcolor" ) || is( "highlight_bgcolor" ) ) ) {
		/* Declared `menu r g b a` but read from the first word
		 * (0x004d7446..0x004d755d), so the menu's name is the red. */
		ReadColorFrom( args, 0, is( "highlight_fgcolor" ) ? widget.pulldown.highlightFg : widget.pulldown.highlightBg );
	}
	/* UIMultiLineEdit's (responses 0x0052cc68). */
	else if ( widget.kind == WidgetKind::MultiLineEdit && is( "edit" ) ) widget.edit.editable = atoi( first.c_str() ) != 0;
	else if ( widget.kind == WidgetKind::MultiLineEdit && is( "updatecvar" ) ) {
		// 0x004dc4b0: the linked cvar's text, once, as the line is read.
		if ( !widget.linkCvar.empty() ) EditSetText( widget.edit, UiCvarString( widget.linkCvar.c_str() ).c_str() );
	}
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
	else if ( is( "fadein" ) ) { if ( !args.empty() ) menu.fadeIn = ToFloat( args[0] ); }
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
			if ( widget.kind == WidgetKind::Slider && Com_GameIsFAKK2() ) {
				// UISlider's constructor (fakk2.exe 1.02 0x004b4b3f, 0x004b4b5d, 0x004b4ac9..0x004b4e73).
				widget.slider.s.arrowWidth = 15;
				widget.slider.s.thumbWidth = 10.0f;
				widget.slider.thumb = "textures/menu/slider_thumb";
				widget.slider.thumbSel = "textures/menu/slider_thumb_sel";
				widget.slider.left = "textures/menu/arrow_left";
				widget.slider.leftSel = "textures/menu/arrow_left_sel";
				widget.slider.right = "textures/menu/arrow_right";
				widget.slider.rightSel = "textures/menu/arrow_right_sel";
			}
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

/* loadmenu (alice.exe 0x0040e180 over 0x0040e0c0, fakk2.exe 1.02 0x0040cb80
 * over 0x0040cac0): ui/<name> in a 128-byte buffer with its extension
 * replaced by .urc, read as every menu file is, its initdata written as it
 * is read. A menu whose name the list already holds stays behind the one
 * loaded first, which the lookup finds (0x004c40f0). */
void LoadMenu_f() {
	char path[128];
	Com_sprintf( path, sizeof( path ), "ui/%s", Cmd_Argv( 1 ) );
	COM_StripExtension( path, path, sizeof( path ) );
	Q_strcat( path, sizeof( path ), ".urc" );
	const std::size_t first = menus.size();
	LoadMenuFile( path );
	for ( std::size_t i = first; i < menus.size(); ++i ) {
		for ( const Widget &widget : menus[i].widgets ) {
			if ( widget.hasInitData ) UiCvarSet( widget.linkCvar, widget.initData.c_str() );
		}
	}
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
	int longest = std::max( MotionMs( menu.motionTime ), MotionMs( menu.fadeIn ) );
	for ( const Widget &widget : menu.widgets ) longest = std::max( longest, MotionMs( widget.motionTime ) );
	return longest;
}

/* The menu's own fade (a menu-level `fadein`), which its widgets multiply
 * into theirs: in as it shows and back out as it hides, as a widget's fade
 * runs. fakk2.exe 1.02's hide moves every item of the menu out, its
 * container among them (0x004a4120), and a fade runs out as it runs in
 * (0x0049de30). */
float MenuFade( const Menu &menu ) {
	if ( menu.fadeIn <= 0.0f ) return 1.0f;
	const int start = menu.hiding ? menu.hideStart : menu.showStart;
	float t = ( Now() - start ) / ( menu.fadeIn * 1000.0f );
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	return menu.hiding ? 1.0f - t : t;
}

/* How far into its motion a widget is, 0 at its start, 1 at its end. */
float MotionFraction( const Widget &widget, float seconds ) {
	if ( seconds <= 0.0f ) return 1.0f;
	float frac = ( Now() - widget.motionStart ) / ( seconds * 1000.0f );
	return frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac;
}

/* The menu's place: home, or on its way in from the edge its header names as
 * it shows and back as it hides, carrying its widgets (the container's own
 * motion, 0x004bb990) [I: as a widget's]. Only F.A.K.K.2's menus name one --
 * bosshud comes in from the left over a second. */
void MenuPosition( const Menu &menu, float &x, float &y ) {
	x = MenuOriginX( menu );
	y = MenuOriginY( menu );
	if ( menu.motionTime <= 0.0f || !menu.visible ) return;
	float offX = x, offY = y;
	switch ( menu.direction ) {
	case urc::Direction::FromLeft: offX = -menu.w; break;
	case urc::Direction::FromRight: offX = static_cast<float>( cls.glconfig.vidWidth ); break;
	case urc::Direction::FromBottom: offY = static_cast<float>( cls.glconfig.vidHeight ); break;
	case urc::Direction::FromTop: offY = -menu.h; break;
	default: return;
	}
	const int start = menu.hiding ? menu.hideStart : menu.showStart;
	float frac = ( Now() - start ) / ( menu.motionTime * 1000.0f );
	frac = frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac;
	if ( menu.hiding ) frac = 1.0f - frac;
	x = offX + ( x - offX ) * frac;
	y = offY + ( y - offY ) * frac;
}

/* The widget's position: home, or on its way between home and the edge it
 * enters from. Retail measures the far edges in the real screen's size while
 * the widget is in 640x480 units; kept. */
void WidgetPosition( const Menu &menu, const Widget &widget, float &x, float &y ) {
	float originX, originY;
	MenuPosition( menu, originX, originY );
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
	/* The head follows the cursor (0x004d31db..0x004d3296): the cursor's place
	 * in pixels over the whole screen, read every frame whatever the cursor
	 * mode (0x0040e400), gives Bip01 Head a pitch and a yaw -- -45 times
	 * (1 - y / height - 0.5) and (1 - x / width - 0.2), so straight ahead
	 * with the cursor at 0.8 of the width -- made a quaternion through their
	 * axis (0x0044a810) on controller 0 at scale 1, with the entity's
	 * useAngles set (0x004d320e). Nothing is kept between frames, smoothed or
	 * clamped. Retail leaves the other four controllers zeroed, which its
	 * renderer applies to bone 0, the origin no vertex follows; here they
	 * name no bone. */
	if ( handles.tiki >= 0 && cls.glconfig.vidWidth > 0 && cls.glconfig.vidHeight > 0 ) {
		int mx = 0, my = 0;
		CL_AliceGetMouseState( &mx, &my, nullptr );
		const float head[3] = {
			static_cast<float>( ( 1.0 - static_cast<double>( my ) / cls.glconfig.vidHeight - 0.5 ) * -45.0 ),
			static_cast<float>( ( 1.0 - static_cast<double>( mx ) / cls.glconfig.vidWidth - 0.2 ) * -45.0 ),
			0.0f };
		aliceBoneController_t &control = alice.controllers[0];
		control.bone = AliceTiki_TagNumForName( handles.tiki, "Bip01 Head" );
		fntech3::alice::state::reader::EulerToQuat( head, control.quat );
		control.scale = 1.0f;
		for ( int c = 1; c < 5; ++c ) alice.controllers[c].bone = -1;
		alice.useControllers = qtrue;
	}
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
	if ( !binding ) return false;
	/* F.A.K.K.2 checks a kept key case-sensitively (fakk2.exe 1.02 0x004997a0),
	 * so a binding in another case shows as None there. */
	return Com_GameIsFAKK2() ? !strcmp( binding, command.c_str() ) : !Q_stricmp( binding, command.c_str() );
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

/* F.A.K.K.2's alternate key (fakk2.exe 1.02 BindKeyToCommand 0x00499cc0 for a
 * cell marked alternate, 0x004bbf00): the second key takes the command, unless
 * it is already the first key, which only clears the second. */
void BindRowAlternate( BindState &bind, int row, int key ) {
	RowKeys( bind, row );
	const std::string &command = bind.layout.items[row].command;
	int &first = bind.key1[row];
	int &second = bind.key2[row];
	if ( second == key ) return;
	if ( second >= 0 ) Key_SetBinding( second, "" );
	if ( first == key ) {
		second = -1;
		return;
	}
	second = key;
	Key_SetBinding( key, command.c_str() );
}

/* A key button's clear (0x004d1700): the first key loses the command, or in
 * F.A.K.K.2's alternate cell the second (0x004bc300). */
void ClearRowKey( BindState &bind, int row, bool alternate = false ) {
	RowKeys( bind, row );
	const int key = alternate ? bind.key2[row] : bind.key1[row];
	if ( key > 0 ) Key_SetBinding( key, "" );
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
	// F.A.K.K.2's list starts on the first key cell (fakk2.exe 1.02 0x004c00e0).
	if ( Com_GameIsFAKK2() ) bind.column = 2;
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
	// F.A.K.K.2's is its own 128 by 64 picture (fakk2.exe 1.02 0x004c10e0).
	const qhandle_t shader = CL_AliceShader( Com_GameIsFAKK2() ? "textures/menu/presskey" : "ui/control/press_any_key.tga" );
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
		if ( Com_GameIsFAKK2() && bind.column == 3 ) BindRowAlternate( bind, bind.row, key );
		else BindRowKey( bind, bind.row, key );
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
	/* F.A.K.K.2's list (fakk2.exe 1.02 KeyEvent 0x004c0de0) has the name in
	 * column 1 and two key cells in columns 2 and 3; only a key cell waits for
	 * a key or clears, and the arrows move between the key cells. */
	const bool fakk2 = Com_GameIsFAKK2() != 0;
	switch ( key ) {
	case K_ENTER:
		// The chosen cell's key button, or the name's, waits for a key.
		if ( !fakk2 || bind.column >= 2 ) StartCapture( menu, index );
		return true;
	case K_BACKSPACE:
	case K_DEL:
		if ( bind.column == 2 || ( fakk2 && bind.column == 3 ) ) {
			ClearRowKey( bind, bind.row, bind.column == 3 );
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
		bind.column = bind.column + 1 > ( fakk2 ? 3 : 2 ) ? ( fakk2 ? 3 : 2 ) : bind.column + 1;
		PlayLocalSound( change );
		return true;
	case K_LEFTARROW:
		bind.column = bind.column - 1 < ( fakk2 ? 2 : 1 ) ? ( fakk2 ? 2 : 1 ) : bind.column - 1;
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

void FollowSavePage( Widget &list );
void ChooseSaveRow( Widget &list, int row );
int SaveRowsPage( const Widget &list );
void SaveListDown( Menu &menu, Widget &list, float mx, float my );

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
	/* F.A.K.K.2's load menu has one picture, `LoadSaveShot`, beside its list:
	 * the chosen save's shot, or its localized pak's no_saved_games picture
	 * while none is chosen (fakk2.exe 1.02 0x00415390..0x00415431). A save
	 * whose shot this client has not written yet shows nothing. */
	if ( Widget *picture = FindWidget( menu, "LoadSaveShot" ) ) {
		const int chosen = page.Selected();
		std::string shot = Com_GameIsFAKK2() ? "textures/menu/no_saved_games.tga" : "";
		if ( count > 0 && chosen >= 1 && chosen <= count ) {
			const std::string path = SavePath( list.load.rows[chosen - 1].name, alicesave::ShotExtension );
			shot = !path.empty() && FS_ReadFile( path.c_str(), nullptr ) > 0 ? path : std::string();
		}
		picture->staticImage = shot;
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
			row.comment = header.comment;
			row.name = header.saveName;
			row.saveTime = header.saveTime;
			row.levelTime = header.levelTime;
			list.load.rows.push_back( std::move( row ) );
		}
		FS_FreeFileList( files );
	}
	mw::SortSaves( list.load.rows );
	if ( Com_GameIsFAKK2() ) {
		/* fakk2.exe 1.02 sorts by the date column, which is numeric and
		 * reversed, and then chooses the first row, the newest save
		 * (0x004147a0, 0x00414d01..0x00414d0e). */
		std::reverse( list.load.rows.begin(), list.load.rows.end() );
		list.load.page = mw::SavePage();
		list.load.page.lastCount = static_cast<int>( list.load.rows.size() );   // ShowSaves keeps the choice
		list.load.page.Select( 1, static_cast<int>( list.load.rows.size() ) );
	}
	ShowSaves( menu, list );
	FollowSavePage( list );
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
		FollowSavePage( list );
		ClearMenus( true );
	} else if ( error == static_cast<int>( alicesave::SaveError::DiskFull ) ) {
		SetWidgetShown( menu, "MsgBackground", true );
		SetWidgetShown( menu, "DiskFull", true );
	}
	SV_AliceSaveError( qtrue );
}

/* UIFAKKLoadGameClass::KeyEvent (0x0041c090). Up and Down move the list
 * control's own selection, which Alice's pictures do not follow. F.A.K.K.2's
 * (fakk2.exe 1.02 0x00415710) is the same: Delete removes the chosen save,
 * Enter loads it, and Up and Down step the choice, whose every change shows
 * its save (listbase_item_selected -> 0x00415390). There the choice is the
 * save the buttons take (0x004155b0, 0x00415660), and the other keys are the
 * list control's. */
bool LoadGameKey( Menu &menu, Widget &list, int key ) {
	const int page = SaveRowsPage( list );
	const int count = static_cast<int>( list.load.rows.size() );
	switch ( key ) {
	case K_DEL:
		RemoveChosenSave( menu, list );
		return true;
	case K_ENTER:
		LoadChosenSave( list );
		return true;
	case K_UPARROW:
	case K_DOWNARROW:
		if ( !list.neverDraw ) ChooseSaveRow( list, list.load.current + ( key == K_UPARROW ? -1 : 1 ) );
		ShowSaves( menu, list );
		return true;
	case K_LEFTARROW:
	case K_RIGHTARROW:
	case K_PGUP:
	case K_PGDN:
	case K_HOME:
	case K_END: {
		if ( list.neverDraw ) return false;
		/* The rest are the list control's (fakk2.exe 1.02 UIListCtrl::KeyEvent
		 * 0x004af9a0): Left and Right step as Up and Down do; Page Up and
		 * Page Down go to the shown page's first or last row, and a page on
		 * from there; Home and End to the first and last. */
		const int top = list.load.top;
		int row = list.load.current;
		if ( key == K_LEFTARROW ) row -= 1;
		else if ( key == K_RIGHTARROW ) row += 1;
		else if ( key == K_PGUP ) row = row - 1 != top ? top + 1 : row - page;
		else if ( key == K_PGDN ) row = row != top + page ? top + page : row + page;
		else row = key == K_HOME ? 1 : count;
		ChooseSaveRow( list, row );
		ShowSaves( menu, list );
		return true;
	}
	case K_MWHEELUP:
	case K_MWHEELDOWN:
		// The wheel scrolls two rows and leaves the choice (0x004af9e3, 0x004afa05).
		if ( list.neverDraw ) return false;
		list.load.top = std::clamp( list.load.top + ( key == K_MWHEELUP ? -2 : 2 ), 0, std::max( count - page, 0 ) );
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
	widget.slider.s.value = Com_GameIsFAKK2() ? mw::SteppedFakk2( widget.slider.s, direction )
		: mw::Stepped( widget.slider.s, direction );
	SliderWrite( widget );
}

/* The keys a widget's class takes before the menu's own (the window
 * manager's KeyEvent 0x004b3382 asks the active control first). */
/* ---- UIMultiLineEdit ------------------------------------------------------- */

int EditPage( const Widget &widget ) {
	const int h = FontHeight( widget );
	return h > 0 ? static_cast<int>( widget.h / static_cast<float>( h ) ) : 0;
}

/* Bound (0x004dd120): the line into the text, the column into its line. */
void EditBound( const EditState &edit, EditPoint &p ) {
	const int count = static_cast<int>( edit.lines.size() );
	if ( p.line < 0 ) p.line = 0;
	if ( p.line > count - 1 ) p.line = count - 1;
	const int length = static_cast<int>( edit.lines[p.line].size() );
	if ( p.column < 0 ) p.column = 0;
	if ( p.column > length ) p.column = length;
}

/* EnsureSelectionPointVisible (0x004dd0e0). Returns whether it scrolled. */
bool EditEnsureVisible( const Widget &widget, EditState &edit, const EditPoint &p ) {
	const int page = EditPage( widget );
	if ( p.line >= edit.top && p.line < edit.top + page ) return false;
	edit.top = p.line < edit.top ? p.line : std::max( 0, p.line - page + 1 );
	return true;
}

/* The selection's ends in order (0x004dc450). */
void EditOrdered( const EditState &edit, EditPoint &from, EditPoint &to ) {
	from = edit.begin;
	to = edit.end;
	if ( to.line < from.line || ( to.line == from.line && to.column < from.column ) ) std::swap( from, to );
}

bool EditHasSelection( const EditState &edit ) {
	return edit.begin.line != edit.end.line || edit.begin.column != edit.end.column;
}

/* PointToSelectionPoint (0x004dce60), from a point in the widget. */
EditPoint EditPointAt( const Widget &widget, const EditState &edit, float px, float py ) {
	EditPoint p;
	const int fontH = FontHeight( widget );
	const int line = fontH > 0 ? static_cast<int>( py / static_cast<float>( fontH ) + static_cast<float>( edit.top ) ) : 0;
	if ( line < 0 ) return p;
	p.line = std::min( line, static_cast<int>( edit.lines.size() ) - 1 );
	const std::string &text = edit.lines[p.line];
	const font::Font *metrics = CL_AliceFontMetrics( widget.font );
	float sum = 0.0f;
	int column = 0;
	float width = 0.0f;
	while ( column < static_cast<int>( text.size() ) && sum < px ) {
		width = static_cast<float>( font::CharWidth( metrics, text[column] ) );
		sum += width;
		++column;
	}
	if ( column < static_cast<int>( text.size() ) && column > 0 && sum - width * 0.5f >= px ) --column;
	p.column = column;
	return p;
}

void EditDeleteSelection( EditState &edit ) {
	if ( !EditHasSelection( edit ) ) return;
	EditPoint from, to;
	EditOrdered( edit, from, to );
	std::string &first = edit.lines[from.line];
	const std::string tail = edit.lines[to.line].substr( std::min<std::size_t>( to.column, edit.lines[to.line].size() ) );
	first = first.substr( 0, from.column ) + tail;
	edit.lines.erase( edit.lines.begin() + from.line + 1, edit.lines.begin() + to.line + 1 );
	edit.begin = edit.end = from;
	edit.changed = true;
}

std::string EditSelectedText( const EditState &edit ) {
	EditPoint from, to;
	EditOrdered( edit, from, to );
	std::string text;
	for ( int line = from.line; line <= to.line; ++line ) {
		const std::string &source = edit.lines[line];
		const int length = static_cast<int>( source.size() );
		const int start = std::min( line == from.line ? from.column : 0, length );
		const int end = std::min( line == to.line ? to.column : length, length );
		if ( end > start ) text += source.substr( start, end - start );
		if ( line != to.line ) text += '\n';
	}
	return text;
}

/* AttemptScrollTo (0x004d8b70): the top line held to the text. */
bool EditScrollTo( const Widget &widget, EditState &edit, int top ) {
	const int last = std::max( 0, static_cast<int>( edit.lines.size() ) - EditPage( widget ) );
	const int clamped = std::min( std::max( top, 0 ), last );
	const bool moved = clamped != edit.top;
	edit.top = clamped;
	return moved;
}

/* Copy (0x004ddc00) to the clipboard. */
void EditCopy( const EditState &edit ) {
	const std::string text = EditSelectedText( edit );
	Sys_SetClipboardData( text.c_str() );
}

bool EditChar( Widget &widget, int ch );

/* Paste (0x004ddeb0): the selection goes, then the clipboard's bytes are
 * typed, a newline as a return -- none of them when the box takes no typing,
 * though the selection has gone all the same. */
void EditPaste( Widget &widget ) {
	char *clip = Sys_GetClipboardData();
	EditDeleteSelection( widget.edit );
	if ( !clip ) return;
	for ( const char *c = clip; *c; ++c ) EditChar( widget, *c == '\n' ? 13 : static_cast<unsigned char>( *c ) );
	Z_Free( clip );
}

/* UIMultiLineEdit::KeyEvent (0x004dd180): the selection's end moves and is
 * held to the text; the start follows unless Shift is down; the end is
 * brought into view. It answers only the keys it knows. */
bool EditKey( Widget &widget, int key ) {
	EditState &edit = widget.edit;
	const int page = EditPage( widget );
	const int count = static_cast<int>( edit.lines.size() );
	const bool ctrl = keys[K_CTRL].down != qfalse;
	bool shift = keys[K_SHIFT].down != qfalse;
	EditPoint &end = edit.end;
	switch ( key ) {
	case K_UPARROW:
		--end.line;
		break;
	case K_DOWNARROW:
		++end.line;
		break;
	case K_LEFTARROW:
		if ( --end.column < 0 ) {
			--end.line;
			end.column = end.line < 0 ? 0 : 999999;
		}
		break;
	case K_RIGHTARROW: {
		const EditPoint before = end;
		++end.column;
		EditBound( edit, end );
		if ( end.line == before.line && end.column == before.column ) {
			if ( end.line < count - 1 ) {
				++end.line;
				end.column = 0;
			} else {
				end.column = 999999;
			}
		}
		break;
	}
	case K_HOME:
		end.column = 0;
		if ( ctrl ) end.line = 0;
		break;
	case K_END:
		end.column = 999999;
		if ( ctrl ) end.line = count;
		break;
	case K_PGUP:
		end.line = end.line == edit.top ? end.line - page : edit.top;
		break;
	case K_PGDN:
		end.line = end.line == edit.top + page - 1 ? end.line + page : edit.top + page - 1;
		break;
	case K_MWHEELDOWN:
		EditScrollTo( widget, edit, edit.top + 2 );
		return true;
	case K_MWHEELUP:
		EditScrollTo( widget, edit, edit.top - 2 );
		return true;
	case K_INS:
		// Only with a selection: Ctrl copies, Shift pastes.
		if ( !EditHasSelection( edit ) ) return true;
		if ( ctrl ) EditCopy( edit );
		else if ( shift ) EditPaste( widget );
		return true;
	case K_DEL:
		if ( !EditHasSelection( edit ) ) {
			// A forced-shift Right: the selection grows by one and nothing goes.
			shift = true;
			++end.column;
			EditBound( edit, end );
			break;
		}
		if ( shift ) EditCopy( edit );
		EditDeleteSelection( edit );
		EditEnsureVisible( widget, edit, edit.end );
		return true;
	default:
		return false;
	}
	EditBound( edit, end );
	if ( !shift ) edit.begin = end;
	EditEnsureVisible( widget, edit, end );
	return true;
}

/* UIMultiLineEdit::CharEvent (0x004dd810), the only thing `edit 0` stops:
 * a printable character or a tab replaces the selection, Backspace takes the
 * selection or the character before, and Return splits the line. */
bool EditChar( Widget &widget, int ch ) {
	EditState &edit = widget.edit;
	if ( !edit.editable ) return false;
	if ( ( ch >= 0x20 && ch <= 0x7e ) || ch == '\t' ) {
		EditDeleteSelection( edit );
		std::string &line = edit.lines[edit.begin.line];
		line.insert( line.begin() + std::min<std::size_t>( edit.begin.column, line.size() ), static_cast<char>( ch ) );
		++edit.begin.column;
		edit.end = edit.begin;
		edit.changed = true;
	} else if ( ch == 8 ) {
		if ( !EditHasSelection( edit ) ) {
			if ( --edit.end.column < 0 ) {
				--edit.end.line;
				edit.end.column = edit.end.line < 0 ? 0 : 999999;
			}
			EditBound( edit, edit.end );
		}
		EditDeleteSelection( edit );
	} else if ( ch == 13 ) {
		EditDeleteSelection( edit );
		std::string &line = edit.lines[edit.begin.line];
		const std::size_t at = std::min<std::size_t>( edit.begin.column, line.size() );
		const std::string rest = line.substr( at );
		line.erase( at );
		edit.lines.insert( edit.lines.begin() + edit.begin.line + 1, rest );
		++edit.begin.line;
		edit.begin.column = 0;
		edit.end = edit.begin;
		edit.changed = true;
	} else {
		return false;
	}
	EditEnsureVisible( widget, edit, edit.end );
	return true;
}

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
	case WidgetKind::MultiLineEdit:
		return EditKey( widget, key );
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
	case WidgetKind::MultiLineEdit:
		EditChar( widget, ch );
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

void ClosePulldown();
bool PulldownOpen();
void OpenPulldown( Menu &menu, int index, float mx, float my );
void PulldownDragged( float mx, float my );
void ReleasePulldown( float mx, float my );

/* F.A.K.K.2's buttons light their hover picture when the mouse comes onto
 * them or presses them, and keep it when the mouse goes: UIButtonBase's
 * mouse-exit response is empty (fakk2.exe 1.02 0x004fd438 -> 0x00404200).
 * The window manager puts it out on the control it takes the keys from
 * (0x00498c7a..0x00498c90), so the picture stays with the button that has
 * the keys. Alice's buttons put it out when the mouse leaves (alice.exe
 * 0x004cfc60), and this UI draws Alice's under the mouse. */
bool LightsFollowKeys( const Widget &widget ) {
	return Com_GameIsFAKK2() && widget.kind == WidgetKind::Button;
}

/* The window manager's ActivateControl (0x00498c10): the widget takes the
 * keys and is lit, the one before is put out, and its menu remembers its
 * order number (0x00498cb0 -> 0x0049f5f0 -> 0x0049f970). */
void Activate( Menu &menu, int index ) {
	if ( menu.focus == index ) return;
	if ( menu.focus >= 0 ) menu.widgets[menu.focus].lit = false;
	menu.focus = index;
	if ( index < 0 ) return;
	menu.widgets[index].lit = true;
	menu.currentOrder = menu.widgets[index].orderNumber;
}

/* After every change of menus F.A.K.K.2 gives the keys to the top menu's
 * current order, 1 until a widget with an order number has had them
 * (0x0040abcb -> 0x004a40d0 -> 0x0049f8d0), and puts out every other widget
 * (0x0049f900). The main menu opens with NEW GAME lit. */
void ActivateCurrentOrder( Menu &menu ) {
	if ( !menu.currentOrder ) menu.currentOrder = 1;
	const int order = menu.currentOrder;
	for ( std::size_t i = 0; i < menu.widgets.size(); ++i ) {
		Widget &widget = menu.widgets[i];
		if ( widget.orderNumber == order && widget.shown && widget.enabled ) {
			Activate( menu, static_cast<int>( i ) );
		} else {
			widget.lit = false;
		}
	}
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
		widget.pulldown.open = -1;
		widget.edit.dragging = false;
		widget.edit.hold = widget.bind.hold = widget.load.hold = ScrollHold();
		AddAliases( widget );
	}
	for ( const std::string &command : menu.showCommands ) ExecuteLine( command );
	/* Each enabled widget reads its cvar and writes what it holds (the menu's
	 * show, 0x004c4d97..0x004c4dae). */
	for ( std::size_t i = 0; i < menu.widgets.size(); ++i ) {
		if ( !menu.widgets[i].enabled ) continue;
		UpdateUIElement( menu, static_cast<int>( i ) );
		UpdateData( menu, static_cast<int>( i ) );
	}
	if ( Com_GameIsFAKK2() ) ActivateCurrentOrder( menu );
}

void FinishHide( Menu &menu ) {
	menu.visible = false;
	menu.hiding = false;
	menu.hud = false;
	if ( popup.menu == MenuIndex( menu ) ) ClosePulldown();
	for ( const std::string &command : menu.hideCommands ) ExecuteLine( command );
}

void HideMenu( Menu &menu, bool force ) {
	if ( captureMenu == MenuIndex( menu ) ) EndCapture();
	if ( popup.menu == MenuIndex( menu ) ) ClosePulldown();
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
	// An open inventory is closed first, and that is all (0x0040d4e1..0x0040d4e8).
	if ( CL_Fakk2InventoryClose() ) return;
	if ( PulldownOpen() ) {
		ClosePulldown();
		return;
	}
	Menu *current = CurrentMenu();
	if ( current && !Q_stricmp( current->name.c_str(), name ) ) {
		if ( cls.state <= CA_DISCONNECTED || PlayerDead() ) return;
	}
	if ( current ) {
		if ( Now() >= inputLockUntil ) PopMenu( true );
	} else {
		PushMenu( name, false );
	}
	/* F.A.K.K.2's escape then offers BACK TO GAME only in a game: its main
	 * menu's `backtogame` is disabled outside one and enabled in one
	 * (fakk2.exe 1.02 0x0040c032..0x0040c0e9). Alice's main menu sees to its
	 * own, from its show command. */
	if ( Com_GameIsFAKK2() && CurrentMenu() && !Q_stricmp( name, "main" ) ) {
		SetWidgetShown( *CurrentMenu(), "backtogame", cls.state >= CA_ACTIVE );
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
	// Heavy Metal: F.A.K.K.2's own steps: two plates and no movie (alice_screen.hpp).
	const screen::IntroStep step = Com_GameIsFAKK2() ? screen::Fakk2Intro( introState ) : screen::Intro( introState );
	Com_DPrintf( "Ritual intro: state %d\n", introState );
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
		introPlate.image = step.plateImage;
		introPlate.scale = step.plateScale;
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
			/* F.A.K.K.2's last step disables BACK TO GAME outside a game
			 * (0x0040e295..0x0040e2eb). */
			if ( Com_GameIsFAKK2() && cls.state < CA_ACTIVE && CurrentMenu() ) {
				SetWidgetShown( *CurrentMenu(), "backtogame", false );
			}
		} else {
			// Between this client's UI restarts: opened when it is back.
			savedStack.assign( 1, "main" );
		}
		if ( step.soundtrack ) S_AliceMusicNewSoundtrack( CL_RitualMenuSoundtrack() );
		introPlate = {};
		introState = step.nextState;
		return;
	case screen::IntroAction::Soundtrack:
		/* Alice's state -1, a map loaded: the main menu's case without its
		 * push (0x0040fdd2). The movie that ends into a map load appends
		 * `_intro` after the map, so this runs then. F.A.K.K.2's state 0
		 * starts the soundtrack and waits before its first plate. */
		S_AliceMusicNewSoundtrack( CL_RitualMenuSoundtrack() );
		introPlate = {};
		introState = step.nextState;
		if ( step.waitMs >= 0 ) ScheduleIntro( step.waitMs );
		return;
	case screen::IntroAction::End:
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

/* ui_checkrestart (0x0040eee0), F.A.K.K.2's Apply buttons: the current menu's
 * restart check (0x004c5340 -> 0x004c4910). Retail first opens or closes its
 * floating and mini consoles by ui_console and ui_minicon; this client's
 * console is its own. */
void UiCheckRestart_f() {
	if ( Menu *menu = CurrentMenu() ) CheckRestart( *menu );
}

/* The engine UI's commands (UI_Init 0x004105fc..0x0041080b). Retail registers
 * them once and never shuts that UI down. This client restarts the hosted
 * module with every map load and video restart, so every one of them is
 * registered once and outlives those restarts: a command queued just before a
 * load -- a `widgetcommand`, a `pushmenu`, the `_intro` a movie appends behind
 * the map it starts (CIN_Finish 0x00425865) -- must not find itself unknown.
 * Each works on the menus the UI has, and while it is down there are none. */
/* The developer menus' pickers (UI_Init 0x00410672..0x00410690). */
void MapList_f();
void ViewSpawnList_f();
void SoundPicker_f();

const char *const kCommands[] = {
	"pushmenu", "forcemenu", "popmenu", "showmenu", "widgetcommand", "listmenus",
	"centerprint", "locationprint", "ui_resetcvars", "ui_restorecvars", "ui_setcvars",
	"ui_showmouse", "ui_hidemouse", "ui_checkrestart", "maplist", "viewspawnlist", "soundpicker",
	"loadmenu",
};
const xcommand_t kCommandHandlers[] = {
	PushMenu_f, ForceMenu_f, PopMenu_f, ShowMenu_f, WidgetCommand_f, ListMenus_f,
	CenterPrint_f, LocationPrint_f, UiResetCvars_f, UiRestoreCvars_f, UiSetCvars_f,
	UiShowMouse_f, UiHideMouse_f, UiCheckRestart_f, MapList_f, ViewSpawnList_f, SoundPicker_f,
	LoadMenu_f,
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
	ui_hud = Cvar_Get( "ui_hud", "1", CL_RitualUiHudFlags() );
	if ( !Com_GameIsFAKK2() ) {
		// fakk2.exe 1.02 has none of these (UI_Init 0x0040e9df..0x0040ea63).
		ui_subtitles = Cvar_Get( "ui_subtitles", "1", CVAR_ARCHIVE );
		Cvar_Get( "ui_skipDepthClear", "1", 0 );
		Cvar_Get( "ui_crosshair", "1", CVAR_ARCHIVE );
		Cvar_Get( "ui_jumpreticle", "1", CVAR_ARCHIVE );
	} else {
		// fnTech3's own: an option, not retail's.
		ui_menuDissolve = Cvar_Get( "ui_menuDissolve", "0", CVAR_ARCHIVE );
		Cvar_SetDescription( ui_menuDissolve,
			"1 dissolves the full-screen menus over the screen's last image, as retail F.A.K.K.2's uncleared "
			"screen leaves them; 0 fades them over black." );
	}
	Cvar_Get( "dlg_badsave", "This save game is invalid", 0 );
}

/* r_gfxinfo: the renderer's report, which Ritual's renderer writes at its
 * start and at each `gfxinfo` (0x00474b40, 0x00475d77, 0x00476028) and
 * F.A.K.K.2's graphics menu shows. This client's renderers print their own
 * report and write no cvar, so the UI writes it as it starts -- after the
 * renderer, as retail's does -- in retail's lines, those this engine has an
 * answer for. */
void SetGfxInfo() {
	const glconfig_t &gl = cls.glconfig;
	std::string report;
	report += va( "\nGL_VENDOR: %s\n", gl.vendor_string );
	report += va( "GL_RENDERER: %s\n", gl.renderer_string );
	report += va( "GL_VERSION: %s\n", gl.version_string );
	/* One line an extension, as F.A.K.K.2's renderer writes the report
	 * (fakk2.exe 1.02 0x00460813..0x00460868): the string cut at its spaces,
	 * each word longer than one character. */
	for ( const char *word = gl.extensions_string; *word; ) {
		const char *end = strchr( word, ' ' );
		const std::size_t length = end ? static_cast<std::size_t>( end - word ) : strlen( word );
		if ( length > 1 ) report += "GL_EXTENSIONS: " + std::string( word, length ) + "\n";
		word += length;
		while ( *word == ' ' ) ++word;
	}
	report += va( "GL_MAX_TEXTURE_SIZE: %d\n", gl.maxTextureSize );
	report += va( "GL_MAX_ACTIVE_TEXTURES_ARB: %d\n", gl.numTextureUnits );
	report += va( "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", gl.colorBits, gl.depthBits, gl.stencilBits );
	report += va( "MODE: %d, %d x %d %s hz:", Cvar_VariableIntegerValue( "r_mode" ), gl.vidWidth, gl.vidHeight,
		gl.isFullscreen ? "fullscreen" : "windowed" );
	report += gl.displayFrequency ? va( "%d\n", gl.displayFrequency ) : "N/A\n";
	report += va( gl.deviceSupportsGamma ? "GAMMA: hardware w/ %d overbright bits\n" : "GAMMA: software w/ %d overbright bits\n",
		Cvar_VariableIntegerValue( "r_overBrightBits" ) );
	report += va( "CPU: %s\n", Cvar_VariableString( "sys_cpustring" ) );
	report += va( "texturemode: %s\n", Cvar_VariableString( "r_textureMode" ) );
	report += va( "picmip: %d\n", Cvar_VariableIntegerValue( "r_picmip" ) );
	report += va( "texture bits: %d\n", Cvar_VariableIntegerValue( "r_texturebits" ) );
	report += va( "multitexture: %s\n", gl.numTextureUnits > 1 ? "enabled" : "disabled" );
	report += va( "texenv add: %s\n", gl.textureEnvAddAvailable ? "enabled" : "disabled" );
	report += va( "compressed textures: %s\n", gl.textureCompression != TC_NONE ? "enabled" : "disabled" );
	Cvar_Get( "r_gfxinfo", "", 0 );
	Cvar_Set( "r_gfxinfo", report.c_str() );
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
		return false;
	case WidgetKind::LoadGame:
		// Only a list the menu shows (F.A.K.K.2's) takes the mouse.
		return !widget.neverDraw;
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
/* F.A.K.K.2's columns (fakk2.exe 1.02 0x004c0390): the name, then after a
 * fill strip the primary key, then after another the alternate key. Index 1
 * to 3; each column's x in the list, width and height. */
void Fakk2BindColumns( const mw::BindLayout &layout, float xs[4], float ws[4], float hs[4] ) {
	const float cw = static_cast<float>( layout.commandWidth ), fw = static_cast<float>( layout.fillWidth );
	const float pw = static_cast<float>( layout.primaryKeyWidth );
	xs[0] = ws[0] = hs[0] = 0.0f;
	xs[1] = 0.0f;
	ws[1] = cw;
	hs[1] = static_cast<float>( layout.commandHeight );
	xs[2] = cw + fw;
	ws[2] = pw;
	hs[2] = static_cast<float>( layout.primaryKeyHeight );
	xs[3] = cw + 2.0f * fw + pw;
	ws[3] = static_cast<float>( layout.alternateKeyWidth );
	hs[3] = static_cast<float>( layout.alternateKeyHeight );
}

bool BindCellAt( const Menu &menu, const Widget &widget, float mx, float my, int &row, int &column ) {
	const BindState &bind = widget.bind;
	const mw::BindLayout &layout = bind.layout;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	for ( int r = 0; r < static_cast<int>( layout.items.size() ); ++r ) {
		if ( !mw::RowVisible( r, bind.top, layout.commandHeight, widget.h ) ) continue;
		const float ry = y + static_cast<float>( mw::VisibleRowTop( r, bind.top, layout.commandHeight ) );
		if ( Com_GameIsFAKK2() ) {
			float xs[4], ws[4], hs[4];
			Fakk2BindColumns( layout, xs, ws, hs );
			for ( int c = 1; c <= 3; ++c ) {
				if ( Inside( mx, my, x + xs[c], ry, ws[c], hs[c] ) ) {
					row = r;
					column = c;
					return true;
				}
			}
			continue;
		}
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
void ScrollThumb( float height, int top, int page, int count, float &offset, float &length ) {
	const float track = height - 32.0f;
	const float n = static_cast<float>( count );
	length = n > 0.0f ? static_cast<float>( page ) * track / n : track;
	if ( length < 6.0f ) length = 6.0f;
	offset = n > 0.0f ? static_cast<float>( top ) * track / n : 0.0f;
	if ( offset + length - track > 0.0f ) offset -= offset + length - track;
	offset += ScrollbarWidth;
}

void ThumbFor( const Widget &widget, float &offset, float &length ) {
	ScrollThumb( widget.h, widget.bind.top, widget.bind.page, static_cast<int>( widget.bind.layout.items.size() ),
		offset, length );
}

/* The item a point on the track names (0x004d91d0): its share of the count,
 * truncated (0x004e6d40). */
int ScrollItemAt( float local, float height, int count ) {
	const float track = height - 32.0f;
	if ( track <= 0.0f ) return 0;
	return static_cast<int>( static_cast<float>( count ) * ( local - ScrollbarWidth ) / track );
}

/* UIVertScroll::left_mouse_down (0x004d8e10). `local` is the press below the
 * scrollbar's top. A list that fits takes no press at all (0x004d8f35..
 * 0x004d8f43). An arrow or the track posts _scrollbar_scroll with no delay,
 * which the next frame's event pass runs (0x004d9199..0x004d91a0); the
 * thumb is taken in items (0x004d8fd6..0x004d9025). Either way the bar holds
 * the mouse until it is let go (0x004d91a5). */
void ScrollPress( ScrollHold &hold, float local, float height, int top, int page, int count ) {
	hold = ScrollHold();
	if ( count <= page ) return;
	if ( local < ScrollbarWidth ) {
		hold.part = 1;
		hold.rate = -1;
	} else if ( height - ScrollbarWidth < local ) {
		hold.part = 2;
		hold.rate = 1;
	} else {
		float offset, length;
		ScrollThumb( height, top, page, count, offset, length );
		if ( local < offset ) {
			hold.part = 5;
			hold.rate = -page;
		} else if ( local < offset + length ) {
			hold.part = 3;
			hold.grab = top - ScrollItemAt( local, height, count );
			hold.topAtPress = top;
			return;
		} else {
			hold.part = 4;
			hold.rate = page;
		}
		if ( !hold.rate ) {   // 0x004d8fa4
			hold = ScrollHold();
			return;
		}
	}
	hold.dueAt = Now();
}

/* UIVertScroll's _scrollbar_scroll (0x004d88b0): one step, and while it still
 * moves the list the same step again 0.2 s on (0x004d896c). Its 0.1 s form
 * needs `autorepeat`, which retail never posts (0x004d8fc6, 0x004d898e). The
 * event pass fires an event up to 1 ms early (0x00443bda), and runs before
 * the mouse is read (0x0042c2aa, 0x0040c82b). */
template <typename Move>
void ScrollRepeat( ScrollHold &hold, int top, Move move ) {
	if ( hold.dueAt < 0 || Now() + 1 < hold.dueAt ) return;
	hold.dueAt = move( top + hold.rate ) ? Now() + 200 : -1;
}

/* left_mousedragged (0x004d9240): only the thumb follows, while the cursor
 * is within 24 units either side of the bar; past that it goes back where it
 * was pressed (0x004d9302..0x004d9355). `lx` is across the bar, `local` down
 * it. */
template <typename Move>
void ScrollDrag( const ScrollHold &hold, float lx, float local, float height, int count, Move move ) {
	if ( hold.part != 3 ) return;
	if ( lx >= -24.0f && lx <= ScrollbarWidth + 24.0f ) move( ScrollItemAt( local, height, count ) + hold.grab );
	else move( hold.topAtPress );
}

/* left_mouse_up (0x004d9200): the step waiting goes and the bar lets go. */
void ScrollRelease( ScrollHold &hold ) {
	hold = ScrollHold();
}

/* Whether a widget's scrollbar holds the mouse, as the window manager's
 * first responder does (0x004b3990): no other widget hovers meanwhile. */
bool HoldsMouse( const Widget &widget ) {
	return widget.bind.hold.part || widget.edit.hold.part || widget.load.hold.part;
}

void BindListDown( Menu &menu, int index, float mx, float my ) {
	Widget &widget = menu.widgets[index];
	BindState &bind = widget.bind;
	if ( !bind.built ) return;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	const float barX = x + widget.w - ScrollbarWidth;
	if ( mx >= barX ) {
		ScrollPress( bind.hold, my - y, widget.h, bind.top, bind.page, static_cast<int>( bind.layout.items.size() ) );
		return;
	}
	int row = 0, column = 1;
	if ( !BindCellAt( menu, widget, mx, my, row, column ) ) return;
	if ( Com_GameIsFAKK2() ) {
		/* A name chooses its row with the change sound (fakk2.exe 1.02
		 * 0x004c1b50 -> 0x004c11c0); a key cell is chosen silently, and waits
		 * for a key only if it was the chosen one already (0x004bc140). */
		if ( column == 1 ) {
			bind.row = row;
			bind.column = 1;
			PlayLocalSound( bind.layout.changeSound );
		} else if ( row == bind.row && column == bind.column ) {
			StartCapture( menu, index );
		} else {
			bind.row = row;
			bind.column = column;
		}
		return;
	}
	/* A cell clicked again waits for a key; either way it is the chosen one
	 * (0x004e47f0, 0x004e4740). */
	if ( row == bind.row && column == bind.column ) StartCapture( menu, index );
	bind.row = row;
	bind.column = column;
	PlayLocalSound( bind.layout.changeSound );
}

void BindListDragged( const Menu &menu, Widget &widget, float mx, float my ) {
	BindState &bind = widget.bind;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	ScrollDrag( bind.hold, mx - ( x + widget.w - ScrollbarWidth ), my - y, widget.h,
		static_cast<int>( bind.layout.items.size() ), [&bind]( int value ) { return ScrollTo( bind, value ); } );
}

/* ---- UIMultiLineEdit's mouse (0x004dcf60 down, 0x004dd020 dragged,
 * 0x004dd080 the drag timer, 0x004dcfd0 up) and its scrollbar -------------- */

void EditDown( Menu &menu, int index, float mx, float my ) {
	Widget &widget = menu.widgets[index];
	EditState &edit = widget.edit;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	if ( mx >= x + widget.w - ScrollbarWidth ) {
		ScrollPress( edit.hold, my - y, widget.h, edit.top, EditPage( widget ), static_cast<int>( edit.lines.size() ) );
		return;
	}
	// The selection starts and ends at the point, which is brought into view.
	edit.begin = edit.end = EditPointAt( widget, edit, mx - x, my - y );
	EditEnsureVisible( widget, edit, edit.end );
	edit.dragging = true;
	edit.dragRepeatAt = -1;
}

void EditDragged( const Menu &menu, Widget &widget, float mx, float my ) {
	EditState &edit = widget.edit;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	if ( edit.hold.part ) {
		ScrollDrag( edit.hold, mx - ( x + widget.w - ScrollbarWidth ), my - y, widget.h,
			static_cast<int>( edit.lines.size() ),
			[&widget, &edit]( int value ) { return EditScrollTo( widget, edit, value ); } );
		return;
	}
	if ( !edit.dragging ) return;
	edit.end = EditPointAt( widget, edit, mx - x, my - y );
	// A drag past an edge scrolls, and scrolls again every 0.25 s (0x3e800000).
	if ( EditEnsureVisible( widget, edit, edit.end ) && edit.dragRepeatAt < 0 ) edit.dragRepeatAt = Now() + 250;
}

void EditRepeat( const Menu &menu, Widget &widget ) {
	EditState &edit = widget.edit;
	if ( !edit.dragging || edit.dragRepeatAt < 0 || Now() < edit.dragRepeatAt ) return;
	float x, y;
	WidgetPosition( menu, widget, x, y );
	edit.end = EditPointAt( widget, edit, VirtualCursorX() - x, VirtualCursorY() - y );
	edit.dragRepeatAt = EditEnsureVisible( widget, edit, edit.end ) ? Now() + 250 : -1;
}

void EditUp( Widget &widget ) {
	widget.edit.dragging = false;
	widget.edit.dragRepeatAt = -1;
	ScrollRelease( widget.edit.hold );
}

int SaveRowsPage( const Widget &list );
void SaveListDragged( const Menu &menu, Widget &list, float mx, float my );

/* Every scrollbar's step that is due, before the mouse is read, as retail's
 * event pass runs ahead of its UI update (0x0042c2aa, 0x0040c82b). */
void ScrollRepeats( Menu &menu ) {
	for ( Widget &widget : menu.widgets ) {
		BindState &bind = widget.bind;
		ScrollRepeat( bind.hold, bind.top, [&bind]( int value ) { return ScrollTo( bind, value ); } );
		EditState &edit = widget.edit;
		ScrollRepeat( edit.hold, edit.top,
			[&widget, &edit]( int value ) { return EditScrollTo( widget, edit, value ); } );
		const int count = static_cast<int>( widget.load.rows.size() ), page = SaveRowsPage( widget );
		ScrollRepeat( widget.load.hold, widget.load.top, [&widget, count, page]( int value ) {
			const int top = std::clamp( value, 0, std::max( 0, count - page ) );
			const bool moved = top != widget.load.top;
			widget.load.top = top;
			return moved;
		} );
	}
}

/* The bar lets go wherever the button comes up (0x004d9200). */
void ScrollReleases( Menu &menu ) {
	for ( Widget &widget : menu.widgets ) {
		ScrollRelease( widget.bind.hold );
		ScrollRelease( widget.edit.hold );
		ScrollRelease( widget.load.hold );
	}
}

/* ---- the window manager's mouse (0x004b3a50) ------------------------------ */

void MouseEnter( Menu &menu, int index ) {
	Widget &widget = menu.widgets[index];
	if ( !widget.mouseEnterCommand.empty() ) ExecuteLine( widget.mouseEnterCommand );
	switch ( widget.kind ) {
	case WidgetKind::Button:
		// Lit even if it has the keys already (0x004ae409).
		widget.lit = true;
		PlayLocalSound( widget.hoverSound );
		Activate( menu, index );
		break;
	case WidgetKind::CheckBox:
		// 0x004e07e0: the control takes the keys; a held button presses it.
		Activate( menu, index );
		if ( cursorButtons & 1 ) widget.check.depressed = true;
		break;
	case WidgetKind::Slider:
		Activate( menu, index );
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

/* The pickers' windows, defined with their drawing below. */
bool PickerUnderCursor();
bool PickerMouseDown();
bool PickerMouseUp();
bool PickerMouseDragged();
bool PickerKey( int key );
void PickerHover();
void PickerRepeats();
void RebuildPickers();
void DrawPickers();

void UpdateHover() {
	Menu *menu = CurrentMenu();
	if ( !menu ) return;
	for ( const Widget &widget : menu->widgets ) {
		if ( HoldsMouse( widget ) ) return;
	}
	// A picker's window over the menu is what the cursor is on (0x004b3990).
	const int hovered = Now() < menu->showStart || cursorX < 0 || viewHasMouse || PickerUnderCursor()
		? -1 : WidgetAt( *menu, VirtualCursorX(), VirtualCursorY() );
	// A held F.A.K.K.2 button is lit while the cursor is on it (0x004ae0c0).
	if ( menu->pressed >= 0 && LightsFollowKeys( menu->widgets[menu->pressed] ) ) {
		Widget &held = menu->widgets[menu->pressed];
		float x, y;
		WidgetPosition( *menu, held, x, y );
		held.lit = Inside( VirtualCursorX(), VirtualCursorY(), x, y, held.w, held.h );
	}
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
	Activate( menu, index );
	const float mx = VirtualCursorX(), my = VirtualCursorY();
	switch ( widget.kind ) {
	case WidgetKind::Button:
		menu.pressed = index;
		widget.lit = true;   // 0x004ae0a0
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
	case WidgetKind::LoadGame:
		SaveListDown( menu, widget, mx, my );
		break;
	case WidgetKind::Pulldown:
		// The press opens the entry under it at once (0x004e2940 -> 0x004e2ad0).
		OpenPulldown( menu, index, mx, my );
		break;
	case WidgetKind::MultiLineEdit:
		EditDown( menu, index, mx, my );
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
	// A text box lets the mouse go wherever the button comes up (0x004dcfd0),
	// and so does every scrollbar (0x004d9200).
	bool held = false;
	for ( Widget &widget : menu.widgets ) {
		held = held || HoldsMouse( widget );
		if ( widget.kind == WidgetKind::MultiLineEdit ) EditUp( widget );
	}
	ScrollReleases( menu );
	if ( held ) return;
	if ( menu.pressed >= 0 ) {
		Widget &widget = menu.widgets[menu.pressed];
		const int under = WidgetAt( menu, VirtualCursorX(), VirtualCursorY() );
		const int pressed = menu.pressed;
		menu.pressed = -1;
		if ( under == pressed ) {
			Click( widget );
		} else if ( LightsFollowKeys( widget ) ) {
			widget.lit = false;   // let go off it (0x004ae2d3)
		}
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
	default:
		break;
	}
}

void MouseDragged( Menu &menu ) {
	// A text box being dragged across keeps the mouse, as its first responder does.
	for ( Widget &widget : menu.widgets ) {
		if ( widget.kind == WidgetKind::MultiLineEdit && ( widget.edit.dragging || widget.edit.hold.part ) ) {
			EditDragged( menu, widget, VirtualCursorX(), VirtualCursorY() );
			return;
		}
		if ( widget.kind == WidgetKind::BindList && widget.bind.hold.part ) {
			BindListDragged( menu, widget, VirtualCursorX(), VirtualCursorY() );
			return;
		}
		if ( widget.kind == WidgetKind::LoadGame && widget.load.hold.part ) {
			SaveListDragged( menu, widget, VirtualCursorX(), VirtualCursorY() );
			return;
		}
	}
	const int index = menu.pressed >= 0 ? menu.pressed : menu.hovered;
	if ( index < 0 ) return;
	Widget &widget = menu.widgets[index];
	if ( widget.kind == WidgetKind::Slider ) SliderDragged( menu, widget, VirtualCursorX(), VirtualCursorY() );
}

/* A mouse button while a key is awaited binds that button when the cursor is
 * on the waiting key button (UIBindButton any_mouse_down 0x004d1591..
 * 0x004d15c2). */
bool CaptureMouse( int key ) {
	Menu &menu = menus[captureMenu];
	const Widget &widget = menu.widgets[captureWidget];
	/* F.A.K.K.2's window manager gives a waiting cell every mouse event, so a
	 * button binds wherever the cursor is (fakk2.exe 1.02 0x0049873e). */
	if ( Com_GameIsFAKK2() ) {
		CaptureKey( key );
		return true;
	}
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
 * up" [inferred]. F.A.K.K.2's (fakk2.exe 1.02 0x0041fb3d..0x0041fbf0) tests
 * "a menu is up" (0x0040ab40) in that order too, but blocks nothing during
 * its intro, takes a second press even in a movie, and stops no movie. */
int lastEscapeTime = -100000;

void EscapeKey() {
	const bool fakk2 = Com_GameIsFAKK2() != 0;
	if ( !fakk2 && EscapeBlockedByIntro() ) return;
	if ( Now() - lastEscapeTime < 1000 && ( fakk2 || cls.state != CA_CINEMATIC ) ) {
		MenuEscape( "main" );
		return;
	}
	lastEscapeTime = Now();
	if ( !menuStack.empty() ) {
		MenuEscape( "main" );
		return;
	}
	if ( cls.state == CA_ACTIVE && CL_RitualInCinematic() ) {
		Cbuf_AddText( "skipcinematic\n" );
		return;
	}
	if ( cls.state <= CA_DISCONNECTED || cls.state == CA_ACTIVE ) {
		MenuEscape( "main" );
		return;
	}
	if ( cls.state == CA_CINEMATIC && !fakk2 ) {
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
	Activate( menu, ordered[position] );
}

void KeyEvent( int key, bool down ) {
	/* F.A.K.K.2's open inventory takes the presses while no menu is up (its
	 * KeyEvent 0x004091b0, as the window manager's active control); with the
	 * cursor on, its buttons come through the cursor instead. */
	if ( menuStack.empty() && CL_Fakk2InventoryIsOpen() ) {
		if ( !down || ( key & K_CHAR_FLAG ) ) return;
		if ( key == K_ESCAPE ) {
			EscapeKey();
			return;
		}
		if ( key >= K_MOUSE1 && key <= K_MOUSE5 && CL_Fakk2InventoryWantsCursor() ) return;
		CL_Fakk2InventoryKey( key );
		return;
	}
	Menu *menu = CurrentMenu();
	if ( key & K_CHAR_FLAG ) {
		// Characters go to the control that has the keys (CharEvent, slot 13).
		if ( down && menu && Now() >= inputLockUntil && !Capturing() && menu->focus >= 0 ) {
			WidgetChar( *menu, menu->focus, key & ~K_CHAR_FLAG );
		}
		return;
	}
	if ( !down ) {
		if ( key == K_MOUSE1 && PickerMouseUp() ) return;
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
		if ( PickerMouseDown() ) return;
		MouseDown( *menu );
		return;
	}
	// A picker's list that has the keys takes them before the menu does.
	if ( PickerKey( key ) ) return;
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
		/* UILabel::Draw (0x004d5910): the cvar's waiting value when it has one
		 * (0x0040e1a0), shown as the words of the first linkstring whose value
		 * it is, compared without regard to case (0x004d5810), else as it is. */
		text = UiCvarString( widget.linkCvar.c_str() );
		for ( const auto &link : widget.linkStrings ) {
			if ( !Q_stricmp( link.first.c_str(), text.c_str() ) ) {
				text = link.second;
				break;
			}
		}
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
 * unchecked picture (blue or black without one) at its indent, centred, and
 * the title after it. F.A.K.K.2's box is indented and Alice's is not. */
void DrawCheckBox( Menu &menu, int index, float x, float y, float alpha ) {
	Widget &widget = menu.widgets[index];
	UpdateData( menu, index );
	if ( !widget.check.depressed ) Box3D( x, y, widget.w, widget.h, false, widget.border3d, alpha );
	const float color[4] = { widget.fg[0], widget.fg[1], widget.fg[2], widget.fg[3] * alpha };
	const float indent = Com_GameIsFAKK2() ? mw::Fakk2CheckIndent : 0.0f;
	const float boxY = y + ( widget.h - mw::CheckSize ) * 0.5f;
	const std::string &shader = widget.check.checked ? widget.check.checkedShader : widget.check.uncheckedShader;
	if ( !shader.empty() ) {
		VirtualPic( x + indent, boxY, mw::CheckSize, mw::CheckSize, color, CL_AliceShader( shader.c_str() ) );
	} else {
		// UBlue (0x00aa6fd0) or UBlack (0x00aa7010).
		const float fill[4] = { 0.0f, 0.0f, widget.check.checked ? 1.0f : 0.0f, alpha };
		VirtualFill( x + indent, boxY, mw::CheckSize, mw::CheckSize, fill );
	}
	if ( !widget.title.empty() ) {
		const float titleY = ( widget.h - static_cast<float>( FontHeight( widget ) ) ) * 0.5f - 1.0f;
		WidgetPrint( widget, widget.fg, x + mw::CheckTitleX( indent ), y + titleY, widget.title.c_str(), widget.fontAngle,
			widget.charSpacing, widget.lineSpacing );
	}
}

/* UISlider::Draw (0x004db440), always opaque: the bar between the arrows, the
 * thumb on it, and the two arrows, pressed or not. */
void DrawSlider( Menu &menu, int index, float x, float y, const float *inherited, float alpha ) {
	Widget &widget = menu.widgets[index];
	UpdateUIElement( menu, index );
	static const float white[4] = { 1, 1, 1, 1 };
	const mw::Slider &s = widget.slider.s;
	const float arrow = static_cast<float>( s.arrowWidth );
	const float centre = mw::ThumbCenter( s, mw::TrackFor( s, x, widget.w ) );
	if ( Com_GameIsFAKK2() ) {
		/* fakk2.exe 1.02 0x004b64e0: the track is the widget's shader. The
		 * thumb, lit while dragged, then each arrow, lit while held, take the
		 * colour the widget's own draw left set. An arrow whose picture and
		 * lit picture are one name is a button: a 3D box, sunk while held,
		 * and the picture 2 units inside it in the box's last, light shade
		 * (0x004b660b, 0x004b66df). */
		float pen[4] = { inherited[0], inherited[1], inherited[2], inherited[3] };
		VirtualPic( x + centre - s.thumbWidth * 0.5f, y, s.thumbWidth, widget.h, pen,
			CL_AliceShader( ( widget.slider.dragging ? widget.slider.thumbSel : widget.slider.thumb ).c_str() ) );
		const auto drawArrow = [&]( float ax, bool held, const std::string &normal, const std::string &lit ) {
			if ( normal != lit ) {
				VirtualPic( ax, y, arrow, widget.h, pen, CL_AliceShader( ( held ? lit : normal ).c_str() ) );
				return;
			}
			Box3D( ax, y, arrow, widget.h, held, widget.border3d, alpha );
			const mw::Rgba &light = widget.border3d.light;
			pen[0] = light.r;
			pen[1] = light.g;
			pen[2] = light.b;
			pen[3] = light.a * alpha;
			VirtualPic( ax + 2.0f, y + 2.0f, arrow - 4.0f, widget.h - 4.0f, pen, CL_AliceShader( normal.c_str() ) );
		};
		drawArrow( x, widget.slider.leftPressed, widget.slider.left, widget.slider.leftSel );
		drawArrow( x + widget.w - arrow, widget.slider.rightPressed, widget.slider.right, widget.slider.rightSel );
		return;
	}
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

/* UIVertScroll::Draw (alice.exe 0x004d87b0): its bar, its arrows (the lower
 * one flipped) and, with more rows than a page, its thumb, in Alice's art.
 * F.A.K.K.2 ships none of that art and draws its own below. */
void DrawScrollbar( float barX, float y, float height, int top, int page, int count, int held ) {
	static const float white[4] = { 1, 1, 1, 1 };
	const qhandle_t bar = CL_AliceShader( "ui/control/slider2_bar.tga" );
	const qhandle_t arrow = CL_AliceShader( "ui/control/slider2_top.tga" );
	float offset = 0.0f, length = 0.0f;
	const bool thumb = count > page;
	if ( thumb ) ScrollThumb( height, top, page, count, offset, length );
	if ( bar && arrow ) {
		/* Alice's arrows (0x004d8430): the lower one is the upper picture turned
		 * half a turn, all four texture coordinates swapped (0x004d8613..
		 * 0x004d863b); while there is something to scroll each is boxed, the
		 * held one sunken (0x004d85d2..0x004d85fa, the box 0x004b5d80) in the
		 * light grey a widget draws its borders from (0x004bb01e). */
		static const mw::BorderColor border = mw::MakeBorderColor( { 0.75f, 0.75f, 0.75f, 1.0f } );
		VirtualPic( barX, y + ScrollbarWidth, ScrollbarWidth, height - 32.0f, white, bar );
		VirtualPic( barX, y, ScrollbarWidth, ScrollbarWidth, white, arrow );
		VirtualPic( barX, y + height - ScrollbarWidth, ScrollbarWidth, ScrollbarWidth, white, arrow, 1.0f, 1.0f, 0.0f, 0.0f );
		if ( thumb ) {
			Box3D( barX, y, ScrollbarWidth, ScrollbarWidth, held == 1, border, 1.0f );
			Box3D( barX, y + height - ScrollbarWidth, ScrollbarWidth, ScrollbarWidth, held == 2, border, 1.0f );
		}
		if ( thumb ) {
			VirtualPic( barX, y + offset, ScrollbarWidth, length, white, CL_AliceShader( "ui/control/slider2_indicator.tga" ) );
		}
		return;
	}
	/* F.A.K.K.2's UIVertScroll (fakk2.exe 1.02 Draw 0x004b3ab0), in its
	 * widget's light grey with a black edge (0x009b2a98, constructor
	 * 0x004b341d). Its two 16-unit arrow buttons are bevelled only while there
	 * is something to scroll (0x004b366c), their marlett glyphs white then and
	 * in the grey's dark shade otherwise (0x004b355f..0x004b3588). The thumb
	 * is 0.5 grey (0x009b2a78), bevelled in its own shades (0x004b36b0). */
	static const float grey[4] = { 0.75f, 0.75f, 0.75f, 1.0f };
	static const float thumbGrey[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	static const mw::BorderColor bevel = mw::MakeBorderColor( { 0.75f, 0.75f, 0.75f, 1.0f } );
	static const mw::BorderColor thumbBevel = mw::MakeBorderColor( { 0.5f, 0.5f, 0.5f, 1.0f } );
	static const int marlett = CL_AliceFontRegister( "marlett" );
	const float greyed[4] = { bevel.dark.r, bevel.dark.g, bevel.dark.b, bevel.dark.a };
	EdgedBox( barX, y, ScrollbarWidth, height, grey );
	for ( int end = 0; end < 2; ++end ) {
		const float ay = end ? y + height - ScrollbarWidth : y;
		EdgedBox( barX, ay, ScrollbarWidth, ScrollbarWidth, grey );
		if ( thumb ) Bevel( barX + 1.0f, ay + 1.0f, ScrollbarWidth - 2.0f, ScrollbarWidth - 2.0f, bevel );
		const font::Rect rect{ barX, ay, ScrollbarWidth, ScrollbarWidth };
		CL_AliceFontPrintJustified( marlett, thumb ? white : greyed, false, rect, 0, 1, end ? "6" : "5", 0.0f, 0.0f,
			0.0f, 1.0f, 1.0f );
	}
	if ( thumb ) {
		EdgedBox( barX, y + offset, ScrollbarWidth, length, thumbGrey );
		Bevel( barX + 1.0f, y + offset + 1.0f, ScrollbarWidth - 2.0f, length - 2.0f, thumbBevel );
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
	DrawScrollbar( x + widget.w - ScrollbarWidth, y, widget.h, bind.top, bind.page,
		static_cast<int>( layout.items.size() ), bind.hold.part );
}

/* Whether the paks hold textures/bind/<name>: F.A.K.K.2's key cell registers
 * that picture for its key and falls back to textures/bind/emptykey with the
 * name printed over it (fakk2.exe 1.02 0x004bc56d..0x004bc6a2). */
bool BindKeyPicture( const char *name ) {
	static std::map<std::string, bool> known;
	std::string key = name;
	for ( char &c : key ) c = static_cast<char>( tolower( static_cast<unsigned char>( c ) ) );
	const auto found = known.find( key );
	if ( found != known.end() ) return found->second;
	bool exists = false;
	for ( const char *extension : { "ftx", "tga", "jpg" } ) {
		if ( FS_ReadFile( va( "textures/bind/%s.%s", key.c_str(), extension ), nullptr ) > 0 ) {
			exists = true;
			break;
		}
	}
	known[key] = exists;
	return exists;
}

/* One of F.A.K.K.2's list cells as UIWidget::Display draws it (0x0049ef30):
 * its background, then its picture in its foreground colour. */
void Fakk2BindPicture( float x, float y, float w, float h, const mw::Rgba &fg, const mw::Rgba &bg,
	const std::string &picture, float alpha ) {
	if ( bg.a != 0.0f ) {
		const float fill[4] = { bg.r, bg.g, bg.b, bg.a * bg.a * alpha };
		VirtualFill( x, y, w, h, fill );
	}
	if ( !picture.empty() ) {
		const float tint[4] = { fg.r, fg.g, fg.b, fg.a * alpha };
		VirtualPic( x, y, w, h, tint, CL_AliceShader( picture.c_str() ) );
	}
}

/* A cell's text, centred, as far as it fits: a name wider than the cell is
 * cut at the right, as retail's clip shows `MWHEELDOW` (0x004bc6ac..0x004bc767). */
void Fakk2BindText( int font, float x, float y, float w, float h, const mw::Rgba &fg, const char *text, float alpha ) {
	const fntech3::alice::ritualfont::Font *metrics = CL_AliceFontMetrics( font );
	std::string shown = text;
	if ( metrics ) {
		while ( shown.size() > 1 && fntech3::alice::ritualfont::StringWidth( metrics, shown.c_str() ) > w ) shown.pop_back();
	}
	const float color[4] = { fg.r, fg.g, fg.b, fg.a * alpha };
	const font::Rect rect{ x, y, w, h };
	CL_AliceFontPrintJustified( font, color, false, rect, 0, 1, shown.c_str(), 0.0f, 0.0f, 0.0f, 1.0f, 1.0f );
}

/* UIFakkBindList as fakk2.exe 1.02 builds and draws it (children 0x004c0390,
 * Draw 0x004c1360): three header labels in the title colours; fill strips the
 * list's height between and after the columns, 0.75 grey behind the tiled
 * fill picture; a row an item from the scrollbar's position, its name picture,
 * or name, then its first and second key's picture or name over emptykey, in
 * the highlight colours for the chosen row and the select (or, waiting, the
 * active) colours for the chosen cell; a waiting cell's 2-unit active border;
 * and the scrollbar at the list's right edge. Every cell is in verdana-12. */
void DrawFakk2BindList( Widget &widget, float x, float y, float alpha ) {
	BindState &bind = widget.bind;
	if ( !bind.built ) return;
	const mw::BindLayout &layout = bind.layout;
	static const int cellFont = CL_AliceFontRegister( "verdana-12" );
	float xs[4], ws[4], hs[4];
	Fakk2BindColumns( layout, xs, ws, hs );
	const float ch = static_cast<float>( layout.commandHeight );
	for ( int c = 1; c <= 3; ++c ) {
		const std::string &picture = layout.headerMaterials[c - 1];
		Fakk2BindPicture( x + xs[c], y, ws[c], ch, layout.titleForeground, layout.titleBackground, picture, alpha );
		if ( picture.empty() && !layout.headers[c - 1].empty() ) {
			Fakk2BindText( cellFont, x + xs[c], y, ws[c], ch, layout.titleForeground, layout.headers[c - 1].c_str(), alpha );
		}
	}
	if ( layout.fillWidth > 0 ) {
		static const mw::Rgba grey{ 0.75f, 0.75f, 0.75f, 1.0f };
		static const mw::Rgba white{ 1.0f, 1.0f, 1.0f, 1.0f };
		const float fw = static_cast<float>( layout.fillWidth );
		const qhandle_t fill = layout.fillMaterial.empty() ? 0 : CL_AliceShader( layout.fillMaterial.c_str() );
		int imageW = 0, imageH = 0;
		if ( fill && re.GetShaderImageSize ) re.GetShaderImageSize( fill, &imageW, &imageH );
		for ( int c = 1; c <= 3; ++c ) {
			const float fx = x + xs[c] + ws[c];
			Fakk2BindPicture( fx, y, fw, widget.h, white, grey, std::string(), alpha );
			if ( fill && imageW > 0 && imageH > 0 ) {
				// Tiled from its corner (the label's tile flag 8, Display 0x0049f0cb).
				const float tint[4] = { 1.0f, 1.0f, 1.0f, alpha };
				VirtualPic( fx, y, fw, widget.h, tint, fill, 0.0f, 0.0f, fw / imageW, widget.h / imageH );
			}
		}
	}
	const int count = static_cast<int>( layout.items.size() );
	for ( int row = bind.top; row < count; ++row ) {
		const float rowY = ch * static_cast<float>( row - bind.top + 1 );
		if ( rowY >= widget.h ) break;
		RowKeys( bind, row );
		const mw::BindItem &item = layout.items[row];
		for ( int c = 1; c <= 3; ++c ) {
			mw::Rgba fg = layout.inactiveForeground, bg = layout.inactiveBackground;
			if ( row == bind.row ) {
				fg = layout.highlightForeground;
				bg = layout.highlightBackground;
				if ( c == bind.column ) {
					fg = bind.capturing ? layout.activeForeground : layout.selectForeground;
					bg = bind.capturing ? layout.activeBackground : layout.selectBackground;
				}
			}
			const float cx = x + xs[c], cy = y + rowY, cw = ws[c], cellH = hs[c];
			if ( c == 1 ) {
				Fakk2BindPicture( cx, cy, cw, cellH, fg, bg, item.material, alpha );
				if ( item.material.empty() ) Fakk2BindText( cellFont, cx, cy, cw, cellH, fg, item.name.c_str(), alpha );
				continue;
			}
			const int key = c == 2 ? bind.key1[row] : bind.key2[row];
			const char *name = key >= 0 ? Key_KeynumToString( key ) : "None";
			const bool picture = BindKeyPicture( name );
			Fakk2BindPicture( cx, cy, cw, cellH, fg, bg,
				picture ? std::string( "textures/bind/" ) + name : std::string( "textures/bind/emptykey" ), alpha );
			if ( !picture ) Fakk2BindText( cellFont, cx + 1.0f, cy + 1.0f, cw, cellH, fg, name, alpha );
			if ( bind.capturing && row == bind.row && c == bind.column ) {
				// The waiting cell's 2-unit border, at full alpha (0x004bc47d..0x004bc496).
				const float edge[4] = { layout.activeBorder.r, layout.activeBorder.g, layout.activeBorder.b,
					layout.activeBorder.a };
				VirtualFill( cx, cy, cw, 2.0f, edge );
				VirtualFill( cx, cy + cellH - 2.0f, cw, 2.0f, edge );
				VirtualFill( cx, cy, 2.0f, cellH, edge );
				VirtualFill( cx + cw - 2.0f, cy, 2.0f, cellH, edge );
			}
		}
	}
	DrawScrollbar( x + widget.w - ScrollbarWidth, y, widget.h, bind.top, bind.page, count, bind.hold.part );
}

/* ---- UIFAKKLoadGameClass drawn: the list control ------------------------- */

int SaveRowsPage( const Widget &list ) {
	return mw::SaveRowsPerPage( list.h, FontHeight( list ) );
}

/* UIListCtrl::Draw over the saves, as F.A.K.K.2's load menu shows them;
 * Alice's list is `neverdraw`. fakk2.exe 1.02 (0x004ac8e0) boxes each
 * column's header 0.3 grey with a one-unit white edge through its DrawBox
 * (0x004ac941..0x004aca0f), prints its title in white two units in (0x009b2ad8,
 * 0x009b2ae8), and boxes the width the columns leave the same; alice.exe's
 * headers are raised grey with black titles (0x004cde00). Then a line a save
 * in the widget's own colours, the chosen one yellow on blue (alice.exe
 * 0x004cd920, 0x004cd8e0), each cell filled and its text at its corner, from
 * the first row shown until a row's top passes the list (0x004ce010). The
 * list's scrollbar is always there, its arrows greyed while the saves fit a
 * page. */
void DrawSaveList( Widget &list, float x, float y, float alpha ) {
	const int line = FontHeight( list );
	const float lineF = static_cast<float>( line );
	const float header = static_cast<float>( mw::SaveHeaderHeight( line ) );
	const float headerFill[4] = { 0.3f, 0.3f, 0.3f, alpha };
	const float white[4] = { 1.0f, 1.0f, 1.0f, alpha };
	float cx = 0.0f;
	for ( int column = 0; column <= mw::SaveColumns; ++column ) {
		const float w = column < mw::SaveColumns ? static_cast<float>( mw::SaveColumnWidth( column, list.w ) )
			: list.w - cx;
		if ( w <= 0.0f ) break;
		EdgedBox( x + cx, y, w, header, headerFill, white );
		if ( column < mw::SaveColumns ) {
			CL_AliceFontPrint( list.font, white, false, x + cx + 2.0f, y + 2.0f, mw::SaveColumnTitles[column], -1,
				0.0f, 0.0f, 0.0f, 1.0f, 1.0f );
		}
		cx += w;
	}
	const int count = static_cast<int>( list.load.rows.size() );
	for ( int row = list.load.top; row < count; ++row ) {
		const float rowY = header + static_cast<float>( row - list.load.top ) * lineF;
		if ( rowY > list.h ) break;
		const float cellH = std::min( lineF, list.h - rowY );
		/* fakk2.exe 1.02's rows (0x004acb04..0x004acbfe): the chosen one black
		 * on white, the rest in the widget's colours, each at the widget's own
		 * alpha rather than its colours' (alice.exe's chosen row is yellow on
		 * blue, 0x004cd920, 0x004cd8e0). The first column is the header's
		 * comment, the level's title. */
		const bool chosen = row + 1 == list.load.current;
		static const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f }, whiteRow[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		const float *fg = chosen ? black : list.fg;
		const float *bg = chosen ? whiteRow : list.bg;
		const float fill[4] = { bg[0], bg[1], bg[2], alpha };
		const float text[4] = { fg[0], fg[1], fg[2], alpha };
		const mw::SaveRow &save = list.load.rows[row];
		const std::string cells[mw::SaveColumns] = { save.comment, mw::ElapsedText( save.levelTime ),
			SaveDate( save.saveTime ) };
		cx = 0.0f;
		for ( int column = 0; column <= mw::SaveColumns; ++column ) {
			const float w = column < mw::SaveColumns ? static_cast<float>( mw::SaveColumnWidth( column, list.w ) )
				: list.w - cx;
			if ( w <= 0.0f ) break;
			if ( fill[3] > 0.0f && cellH > 0.0f ) VirtualFill( x + cx, y + rowY, w, cellH, fill );
			// A row the list's edge cuts keeps its fill; its text would overrun the list.
			if ( column < mw::SaveColumns && cellH >= lineF ) {
				CL_AliceFontPrint( list.font, text, false, x + cx, y + rowY, cells[column].c_str(), -1, 0.0f, 0.0f,
					0.0f, 1.0f, 1.0f );
			}
			cx += w;
		}
	}
	const int page = SaveRowsPage( list );
	DrawScrollbar( x + list.w - ScrollbarWidth, y, list.h, list.load.top, page, count, list.load.hold.part );
}

/* A drawn list follows the page's choice -- the newest save after a refresh
 * whose count changed (SavePage::Refreshed) -- and keeps it in view. */
void FollowSavePage( Widget &list ) {
	if ( list.neverDraw ) return;
	const int count = static_cast<int>( list.load.rows.size() );
	const int chosen = list.load.page.Selected();
	list.load.current = count > 0 && chosen >= 1 && chosen <= count ? chosen : 0;
	list.load.top = mw::SaveTopFor( list.load.current, list.load.top, SaveRowsPage( list ), count );
}

/* A row chosen in a drawn list is the save the buttons take: the page and
 * picture `loadgame` and `removegame` read (0x0041bcb0) follow it, as
 * fakk2.exe 1.02's load and delete read the list's chosen row (+0x160,
 * 0x00415660 and 0x004155b0). */
void ChooseSaveRow( Widget &list, int row ) {
	const int count = static_cast<int>( list.load.rows.size() );
	if ( count <= 0 ) return;
	row = std::clamp( row, 1, count );
	list.load.current = row;
	mw::SelectSaveRow( list.load.page, row );
	list.load.top = mw::SaveTopFor( row, list.load.top, SaveRowsPage( list ), count );
}

/* A press in a drawn list: a row chooses it, one below the last choosing the
 * last as the list's setter holds it (fakk2.exe 1.02 0x004af7a8); on the
 * scrollbar the arrows step and the track pages. The same row pressed again
 * within half a second and two units of the last press is a double click,
 * which loads its save (UIListCtrl 0x004ad497..0x004ad4fe, 2.0 at
 * 0x004d61d8; listbase_item_doubleclicked -> loadgame 0x00415660), and every
 * press is kept for the next (0x004ad729..0x004ad749). */
void SaveListDown( Menu &menu, Widget &list, float mx, float my ) {
	if ( list.neverDraw ) return;
	float x, y;
	WidgetPosition( menu, list, x, y );
	const int line = FontHeight( list );
	const int count = static_cast<int>( list.load.rows.size() );
	const int page = SaveRowsPage( list );
	/* Its scrollbar is a child over its right edge (0x004d3e98), which takes
	 * the press whether or not there is anything to scroll. */
	if ( mx >= x + list.w - ScrollbarWidth ) {
		ScrollPress( list.load.hold, my - y, list.h, list.load.top, page, count );
		return;
	}
	const float local = my - y - static_cast<float>( mw::SaveHeaderHeight( line ) );
	if ( local < 0.0f || line <= 0 ) return;
	const int row = list.load.top + static_cast<int>( local / static_cast<float>( line ) ) + 1;
	if ( count <= 0 ) return;
	ChooseSaveRow( list, row );
	ShowSaves( menu, list );
	const bool twice = Now() < list.load.clickTime + 500 && list.load.current == list.load.clickRow
		&& std::fabs( list.load.clickX - mx ) <= 2.0f && std::fabs( list.load.clickY - my ) <= 2.0f;
	list.load.clickTime = Now();
	list.load.clickRow = list.load.current;
	list.load.clickX = mx;
	list.load.clickY = my;
	if ( twice ) LoadChosenSave( list );
}

void SaveListDragged( const Menu &menu, Widget &list, float mx, float my ) {
	float x, y;
	WidgetPosition( menu, list, x, y );
	const int count = static_cast<int>( list.load.rows.size() ), page = SaveRowsPage( list );
	ScrollDrag( list.load.hold, mx - ( x + list.w - ScrollbarWidth ), my - y, list.h, count,
		[&list, count, page]( int value ) {
			const int top = std::clamp( value, 0, std::max( 0, count - page ) );
			const bool moved = top != list.load.top;
			list.load.top = top;
			return moved;
		} );
}

/* ---- UIFakkLabel (Draw 0x0041ef00) ----------------------------------------- */

/* The label's number or item name (0x0041efcb..0x0041f041): centred both ways
 * in its rectangle, in its font at the foreground colour as it stands -- no
 * justification, scale, spacing or fade of the widget's (0x0041eff3). */
void FakkLabelPrint( const Widget &widget, float x, float y, const char *text ) {
	const font::Rect rect{ x, y, widget.w, widget.h };
	CL_AliceFontPrintJustified( widget.font, widget.fg, false, rect, 0, 1, text, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f );
}

/* One of the bar's two pictures over the quad the orientation, the tile
 * flag and the fraction make, its texture coordinates from its own image. */
void FakkBarPicture( const Widget &widget, float x, float y, float frac, qhandle_t shader, const float *rgba ) {
	if ( !shader ) return;
	int imageW = 0, imageH = 0;
	if ( re.GetShaderImageSize ) re.GetShaderImageSize( shader, &imageW, &imageH );
	const fakkhud::Quad q = fakkhud::BarQuad( widget.fakk.label.orientation, widget.tileShader, frac, widget.w, widget.h,
		static_cast<float>( imageW ), static_cast<float>( imageH ) );
	if ( q.draw ) VirtualPic( x + q.x, y + q.y, q.w, q.h, rgba, shader, q.s0, q.t0, q.s1, q.t1 );
}

/* DrawStatbar (0x0041eb90): an empty bar draws nothing; otherwise the bar, in
 * whatever colour the widget's draw left (the shipped bars ignore it), then
 * the flash over it at its strength. Without a bar picture, a box in the
 * foreground colour and no flash. */
void FakkStatBar( Widget &widget, float x, float y, float frac ) {
	if ( !( frac > 0.0f ) ) return;
	const float a = fakkhud::StepFlash( widget.fakk.flash, frac, static_cast<float>( Now() ) );
	if ( widget.fakk.barShader.empty() ) {
		const fakkhud::Quad box = fakkhud::FallbackBox( frac, widget.w, widget.h );
		if ( box.draw ) VirtualFill( x + box.x, y + box.y, box.w, box.h, widget.fg );
		return;
	}
	static const float white[4] = { 1, 1, 1, 1 };
	FakkBarPicture( widget, x, y, frac, CL_AliceShader( widget.fakk.barShader.c_str() ), white );
	if ( a == 0.0f || widget.fakk.flashShader.empty() ) return;
	const float flash[4] = { a, a, a, a };
	FakkBarPicture( widget, x, y, frac, CL_AliceShader( widget.fakk.flashShader.c_str() ), flash );
}

/* A model label: the item in its hand slot, looked up again only in the
 * second half of each second (0x0041f0d5..0x0041f111), drawn by its hud*
 * placement, always moving, at its first animation's first frame. */
void FakkItemModel( Widget &widget, float x, float y ) {
	if ( fakkhud::ItemLookupWindow( cls.realtime ) ) {
		const int slot = fakkhud::ReadSlot( cl.snap.ps.alice.block[1], widget.fakk.label.model );
		const char *name = CL_RitualItemName( slot );
		const fakkinv::Item *found = name ? CL_Fakk2FindInventoryItem( name ) : nullptr;
		widget.fakk.item = found ? found->name : std::string();
	}
	if ( widget.fakk.item.empty() ) return;
	const fakkinv::Item *item = CL_Fakk2FindInventoryItem( widget.fakk.item.c_str() );
	if ( item ) CL_Fakk2RenderModel( x, y, widget.w, widget.h, item->hudModel.c_str(), "", item->hud, true );
}

/* Whether the label is one of UIFakkLabel's, drawn so: a stat, else an item's
 * name, else its model; the title is then never drawn. */
bool DrawFakkLabel( Widget &widget, float x, float y ) {
	const fakkhud::Label &label = widget.fakk.label;
	if ( label.stat != -1 ) {
		if ( label.orientation != fakkhud::Orientation::None ) {
			FakkStatBar( widget, x, y, fakkhud::StatFraction( label, cl.snap.ps.alice.block[0] ) );
		} else {
			FakkLabelPrint( widget, x, y, va( "%d", fakkhud::ReadSlot( cl.snap.ps.alice.block[0], label.stat ) ) );
		}
		return true;
	}
	if ( label.item != -1 ) {
		// A slot below -1 draws nothing (0x0041f04e).
		if ( label.item >= 0 ) {
			const char *name = CL_RitualItemName( fakkhud::ReadSlot( cl.snap.ps.alice.block[1], label.item ) );
			if ( name ) FakkLabelPrint( widget, x, y, name );
		}
		return true;
	}
	if ( label.model != -1 ) {
		FakkItemModel( widget, x, y );
		return true;
	}
	return false;
}

/* ---- UIPulldownMenuContainer ------------------------------------------------ */

/* The closed menu's frame (0x004e1b60): the container's width by the tallest
 * of the font and the entries' pictures, plus 4. */
float PulldownHeight( const Widget &widget ) {
	float height = static_cast<float>( FontHeight( widget ) );
	for ( const PulldownEntry &entry : widget.pulldown.entries ) {
		if ( entry.shader.empty() ) continue;
		int w = 0, h = 0;
		CL_AliceShaderSize( CL_AliceShader( entry.shader.c_str() ), &w, &h );
		height = std::max( height, static_cast<float>( h ) );
	}
	return height + 4.0f;
}

/* An entry's width (0x004e2e80): its picture's, else its title's plus 8. */
float PulldownEntryWidth( const Widget &widget, const PulldownEntry &entry ) {
	if ( !entry.shader.empty() ) {
		int w = 0, h = 0;
		CL_AliceShaderSize( CL_AliceShader( entry.shader.c_str() ), &w, &h );
		return static_cast<float>( w );
	}
	return static_cast<float>( font::StringWidth( CL_AliceFontMetrics( widget.font ), entry.name.c_str() ) ) + 8.0f;
}

float PulldownEntryHeight( const Widget &widget, const PulldownEntry &entry ) {
	if ( !entry.shader.empty() ) {
		int w = 0, h = 0;
		CL_AliceShaderSize( CL_AliceShader( entry.shader.c_str() ), &w, &h );
		return static_cast<float>( h );
	}
	return static_cast<float>( FontHeight( widget ) );
}

/* UIPulldownMenu::Draw (0x004e2770): the entries left to right; the open one
 * on its highlight box, or its selected picture instead of its own. */
void DrawPulldown( const Widget &widget, float x, float y, float alpha ) {
	const PulldownState &state = widget.pulldown;
	const float frameH = PulldownHeight( widget );
	// The menu is its own widget, with the container's colours (0x004d6590).
	if ( widget.bg[3] != 0.0f ) {
		const float bg[4] = { widget.bg[0], widget.bg[1], widget.bg[2], widget.bg[3] * widget.bg[3] * alpha };
		VirtualFill( x, y, widget.w, frameH, bg );
	}
	const float shade[4] = { 1.0f, 1.0f, 1.0f, alpha };
	float atx = 0.0f;
	for ( std::size_t i = 0; i < state.entries.size(); ++i ) {
		const PulldownEntry &entry = state.entries[i];
		const float w = PulldownEntryWidth( widget, entry ), h = PulldownEntryHeight( widget, entry );
		const bool open = static_cast<int>( i ) == state.open;
		const float *color = open ? state.highlightFg : widget.fg;
		if ( open ) {
			if ( !entry.selectedShader.empty() ) {
				VirtualPic( x + atx, y, w, h, shade, CL_AliceShader( entry.selectedShader.c_str() ) );
			} else {
				VirtualFill( x + atx, y, w, frameH, state.highlightBg );
			}
		}
		if ( !entry.shader.empty() ) {
			if ( !open ) VirtualPic( x + atx, y, w, h, shade, CL_AliceShader( entry.shader.c_str() ) );
		} else {
			WidgetPrint( widget, color, x + atx + 4.0f, y + 2.0f, entry.name.c_str() );
		}
		atx += w;
	}
}

/* The open list's rows (UIPopupMenu's heights and widths, 0x004e5f20,
 * 0x004e5f50): 8 units for a separator, a font height otherwise. */
float PopupRowHeight( const Widget &widget, const PopupItem &item ) {
	return item.type == PopupSeparator ? 8.0f : static_cast<float>( FontHeight( widget ) );
}

const PulldownEntry *PopupEntry() {
	if ( popup.menu < 0 || popup.menu >= static_cast<int>( menus.size() ) ) return nullptr;
	const Menu &menu = menus[popup.menu];
	if ( popup.widget < 0 || popup.widget >= static_cast<int>( menu.widgets.size() ) ) return nullptr;
	const Widget &widget = menu.widgets[popup.widget];
	const int open = widget.pulldown.open;
	return open >= 0 && open < static_cast<int>( widget.pulldown.entries.size() ) ? &widget.pulldown.entries[open] : nullptr;
}

/* The row under a point (0x004e5ce0), from 0: none above its first row or
 * in its side margins, and its last row for anything below the rows. */
int PopupRowAt( float px, float py ) {
	const PulldownEntry *entry = PopupEntry();
	if ( !entry || entry->items.empty() ) return -1;
	const Widget &widget = menus[popup.menu].widgets[popup.widget];
	const float lx = px - popup.x, ly = py - popup.y;
	if ( ly < 4.0f || lx < 2.0f || lx >= popup.w - 2.0f ) return -1;
	float top = 4.0f;
	for ( std::size_t i = 0; i < entry->items.size(); ++i ) {
		const float h = PopupRowHeight( widget, entry->items[i] );
		if ( ly >= top && ly < top + h ) return static_cast<int>( i );
		top += h;
	}
	return static_cast<int>( entry->items.size() ) - 1;
}

/* A press opens the entry under it at once (the drag handler 0x004e2ad0):
 * the list below it, 24 wider than its widest title and 8 taller than its
 * rows, held on the screen (UIPopupMenu::Create 0x004e5600). */
void OpenPulldown( Menu &menu, int index, float mx, float my ) {
	Widget &widget = menu.widgets[index];
	float x, y;
	WidgetPosition( menu, widget, x, y );
	const float frameH = PulldownHeight( widget );
	const float px = mx - x, py = my - y;
	if ( !( px >= 0.0f && px < widget.w && py >= 0.0f && py < frameH ) ) return;
	float atx = 2.0f;   // hit-testing starts 2 units in (0x004e2b94)
	for ( std::size_t i = 0; i < widget.pulldown.entries.size(); ++i ) {
		const PulldownEntry &entry = widget.pulldown.entries[i];
		const float w = PulldownEntryWidth( widget, entry );
		if ( px >= atx && px < atx + w ) {
			if ( static_cast<int>( i ) == widget.pulldown.open ) return;
			widget.pulldown.open = static_cast<int>( i );
			float listW = 0.0f, listH = 0.0f;
			for ( const PopupItem &item : entry.items ) {
				listH += PopupRowHeight( widget, item );
				if ( item.type != PopupSeparator ) {
					listW = std::max( listW, static_cast<float>( font::StringWidth( CL_AliceFontMetrics( widget.font ),
						item.title.c_str() ) ) );
				}
			}
			if ( listW == 0.0f || listH == 0.0f ) {
				popup = {};
				popup.menu = MenuIndex( menu );
				popup.widget = index;
				return;
			}
			listW += 24.0f;
			listH += 8.0f;
			float lx = x + atx, ly = y + frameH;
			if ( lx + listW > 640.0f ) lx = x + atx + w < listW ? std::max( 0.0f, 640.0f - listW ) : x + atx + w - listW;
			if ( ly + listH > 480.0f ) ly = y < listH ? std::max( 0.0f, 480.0f - listH ) : y - listH;
			popup.menu = MenuIndex( menu );
			popup.widget = index;
			popup.x = lx;
			popup.y = ly;
			popup.w = listW;
			popup.h = listH;
			popup.selected = -1;
			return;
		}
		atx += w;
	}
}

void ClosePulldown() {
	if ( popup.menu >= 0 && popup.menu < static_cast<int>( menus.size() ) ) {
		Menu &menu = menus[popup.menu];
		if ( popup.widget >= 0 && popup.widget < static_cast<int>( menu.widgets.size() ) ) {
			menu.widgets[popup.widget].pulldown.open = -1;
		}
	}
	popup = {};
}

bool PulldownOpen() {
	return popup.menu >= 0;
}

/* A drag while the list is open (0x004e2ad0): another entry under the cursor
 * opens instead, and the list's row under it is picked -- none beside the
 * list or above its rows, its last row anywhere below them (0x004e5e90). */
void PulldownDragged( float mx, float my ) {
	if ( !PulldownOpen() ) return;
	OpenPulldown( menus[popup.menu], popup.widget, mx, my );
	popup.selected = PopupRowAt( mx, my );
}

/* Let go (0x004e2960, then the list's 0x004e5b40): over the list, the list
 * goes first and then the picked row's command runs -- `command` and `cvar`
 * both as a line of console text; elsewhere the list just closes. */
void ReleasePulldown( float mx, float my ) {
	const PulldownEntry *entry = PopupEntry();
	const bool over = entry && Inside( mx, my, popup.x, popup.y, popup.w, popup.h );
	const int row = over ? popup.selected : -1;
	const PopupItem item = row >= 0 && row < static_cast<int>( entry->items.size() ) ? entry->items[row] : PopupItem();
	ClosePulldown();
	if ( item.type == PopupCommand || item.type == PopupCvar ) ExecuteLine( item.data );
}

/* UIPopupMenu::Draw (0x004e58c0), over every menu: its background and raised
 * border in the entry's list colours, each row's title 4 units in, the row
 * under the cursor on the highlight, a separator as a line. */
void DrawPopup() {
	const PulldownEntry *entry = PopupEntry();
	if ( !entry || popup.w <= 0.0f ) return;
	const Widget &widget = menus[popup.menu].widgets[popup.widget];
	const float bg[4] = { entry->bg[0], entry->bg[1], entry->bg[2], entry->bg[3] * entry->bg[3] };
	VirtualFill( popup.x, popup.y, popup.w, popup.h, bg );
	Box3D( popup.x, popup.y, popup.w, popup.h, false, mw::MakeBorderColor( ToRgba( entry->bg ) ), 1.0f );
	float top = 4.0f;
	for ( std::size_t i = 0; i < entry->items.size(); ++i ) {
		const PopupItem &item = entry->items[i];
		const float h = PopupRowHeight( widget, item );
		if ( item.type == PopupSeparator ) {
			VirtualFill( popup.x + 8.0f, popup.y + top + 4.0f, popup.w - 16.0f, 1.0f, entry->fg );
		} else {
			const float *color = entry->fg;
			if ( static_cast<int>( i ) == popup.selected ) {
				VirtualFill( popup.x + 2.0f, popup.y + top, popup.w - 4.0f, static_cast<float>( FontHeight( widget ) ),
					entry->highlightBg );
				color = entry->highlightFg;
			}
			WidgetPrint( widget, color, popup.x + 4.0f, popup.y + top, item.title.c_str() );
		}
		top += h;
	}
}

/* UIMultiLineEdit::Draw (0x004dc4f0): the lines from the scrollbar's top in the
 * foreground colour, the selection white on a dark blue box, and a black
 * caret on the selection's end line every other 375 ms while it has the keys. */
/* ---- the developer menus' file pickers -------------------------------------
 *
 * FilePickerClass (0x00417fd0) in its UIFloatingWindow (0x004c1dd0) of a
 * UIListCtrl (0x004cdc50), and the three subclasses the developer menus open:
 * MapRunnerClass (`maplist`, 0x0041c1c0), ViewSpawnerClass (`viewspawnlist`,
 * 0x0041c930) and SoundPickerClass (`soundpicker`, 0x0041d830); fakk2.exe 1.02
 * has the same five (0x00412880..0x00416f90). Each command opens another
 * window at the same place, as retail tracks none (0x0040d9a0..0x0040daca).
 * The rules are code/compat/alice_picker.hpp's. What differs here: the rows
 * come from this engine's file listing, because retail's skips a folder's
 * own pak files and garbles deeper names (0x0043cc90), so on a retail
 * installation its pickers listed no model or map; and a picker lives across
 * the hosted UI's restarts, its rows read again, where retail's UI never
 * restarts. */

struct FilePicker {
	picker::Kind kind = picker::Kind::MapRunner;
	std::string root, current, extension;     // +0x24, +0x20, +0x28
	std::vector<picker::Row> rows;
	int currentRow = 0;   // 1-based, 0 for none (UIListBase +0x1d0)
	int top = 0;          // the first row shown (its scrollbar's position)
	ScrollHold hold;
	int clickTime = 0, clickRow = 0;
	float clickX = 0.0f, clickY = 0.0f;
	float width[3] = { picker::ColumnWidths[0], picker::ColumnWidths[1], picker::ColumnWidths[2] };
	int sizing = -1;      // the column a header edge resizes (+0x1f0)
	float sizingLeft = 0.0f;
	float x = picker::WindowX, y = picker::WindowY, w = picker::WindowW, h = picker::WindowH;
	float restoredH = picker::WindowH;
	bool minimized = false, dragging = false;
	float grabX = 0.0f, grabY = 0.0f;
	int titleClickTime = -20000;
	float titleClickX = 0.0f, titleClickY = 0.0f;
};

std::vector<FilePicker> pickers;   // drawn in order; the last is on top
int pickerKeys = -1;               // the picker whose list has the keys

int PickerFont() {
	static std::string loaded;
	static int font = -1;
	const char *name = CL_RitualDefaultFont();
	if ( font < 0 || loaded != name ) {
		font = CL_AliceFontRegister( name );
		loaded = name;
	}
	return font;
}

float PickerLine() {
	const int line = CL_AliceFontLineHeight( PickerFont() );
	return static_cast<float>( line > 0 ? line : 14 );
}

float PickerHeader() {
	return PickerLine() + 2.0f * picker::HeaderPad;
}

void PickerList( const FilePicker &p, float &lx, float &ly, float &lw, float &lh ) {
	lx = p.x + picker::ClientX;
	ly = p.y + picker::ClientY;
	lw = p.w - picker::ClientShrinkW;
	lh = p.h - picker::ClientShrinkH;
}

int PickerPage( const FilePicker &p ) {
	float lx, ly, lw, lh;
	PickerList( p, lx, ly, lw, lh );
	return std::max( 0, static_cast<int>( ( lh - PickerHeader() ) / PickerLine() ) );
}

/* A loose file's date and size from the directory that holds it; a file in a
 * pak has neither (FileInfo 0x0043ec00, whose _findfirst finds only files on
 * disk). */
void PickerFileInfo( const std::string &path, std::string &date, std::string &size ) {
	date.clear();
	size.clear();
	const char *bases[] = { Cvar_VariableString( "fs_homepath" ), Cvar_VariableString( "fs_basepath" ) };
	for ( const char *base : bases ) {
		if ( !base || !base[0] ) continue;
		fileOffset_t bytes = 0;
		fileTime_t mtime = 0, ctime = 0;
		const char *os = FS_BuildOSPath( base, FS_GetCurrentGameDir(), path.c_str() );
		if ( os && Sys_GetFileStats( os, &bytes, &mtime, &ctime ) ) {
			const time_t when = static_cast<time_t>( mtime );
			if ( const std::tm *local = std::localtime( &when ) ) date = picker::DateText( *local );
			size = va( "%d", static_cast<int>( bytes ) );
			return;
		}
	}
}

std::vector<std::string> PickerListing( const std::string &folder, const char *extension ) {
	std::vector<std::string> names;
	std::string path = folder;
	while ( path.size() > 1 && path.back() == '/' ) path.pop_back();
	int count = 0;
	char **list = FS_ListFiles( path.c_str(), extension, &count );
	for ( int i = 0; list && i < count; ++i ) names.emplace_back( list[i] );
	if ( list ) FS_FreeFileList( list );
	return names;
}

/* SetupFiles (0x00418b10): the rows again, the selection and scroll gone. */
void PickerSetupFiles( FilePicker &p ) {
	const std::vector<std::string> folders = picker::CleanNames( PickerListing( p.current, "/" ), true );
	/* SoundPickerClass lists every file (".*", 0x0041db96); this listing's
	 * empty extension is that. */
	const char *extension = p.extension == ".*" ? "" : p.extension.c_str();
	const std::vector<std::string> files = picker::CleanNames( PickerListing( p.current, extension ), false );
	std::vector<std::string> dates( files.size() ), sizes( files.size() );
	for ( std::size_t i = 0; i < files.size(); ++i ) PickerFileInfo( p.current + files[i], dates[i], sizes[i] );
	p.rows = picker::BuildRows( p.current, p.root, folders, files, dates, sizes );
	p.currentRow = 0;
	p.top = 0;
	p.hold = ScrollHold();
}

/* Setup (0x004185d0): the root and the current folder each end in '/'. */
void OpenPicker( picker::Kind kind, const std::string &root, const std::string &current, const char *extension ) {
	FilePicker p;
	p.kind = kind;
	p.root = picker::WithSlash( root );
	p.current = picker::WithSlash( current );
	p.extension = extension;
	PickerSetupFiles( p );
	pickers.push_back( std::move( p ) );
}

void RebuildPickers() {
	for ( FilePicker &p : pickers ) PickerSetupFiles( p );
}

/* `maplist [folder]` (0x0040d710): maps/<folder>, "." meaning the root. */
void MapList_f() {
	if ( !uiInitialized ) return;
	std::string folder = "maps";
	if ( Cmd_Argc() == 2 && std::strcmp( Cmd_Argv( 1 ), "." ) ) folder += std::string( "/" ) + Cmd_Argv( 1 );
	OpenPicker( picker::Kind::MapRunner, "maps", folder, ".bsp" );
}

/* `viewspawnlist` (0x0040d9f0): models/, .tik, no arguments. */
void ViewSpawnList_f() {
	if ( !uiInitialized ) return;
	OpenPicker( picker::Kind::ViewSpawner, "models", "models", ".tik" );
}

/* `soundpicker` (0x0040d9a0): sound/, every file, opening in the folder of
 * the sound last picked (0x0041d9cc reads ui_pickedsound as the UI does,
 * latched value first). */
void SoundPicker_f() {
	if ( !uiInitialized ) return;
	const std::string picked = UiCvarString( "ui_pickedsound" );
	OpenPicker( picker::Kind::SoundPicker, "sound", picker::StartFolder( picked, "sound/" ), ".*" );
}

/* TrySelectItem (0x004d4180): clamped to the rows, `listbase_item_selected`
 * only on a change, kept on the page. A sound picker plays a file selected
 * (0x0041dc30, the alias-aware local sound 0x0044ce10); a folder or the
 * parent row selects silently (0x004195ad, 0x004195d5). */
void PickerSelect( FilePicker &p, int row ) {
	const int count = static_cast<int>( p.rows.size() );
	if ( count <= 0 ) return;
	row = std::clamp( row, 1, count );
	const int page = std::max( 1, PickerPage( p ) );
	if ( row <= p.top ) p.top = row - 1;
	else if ( row > p.top + page ) p.top = row - page;
	p.top = std::clamp( p.top, 0, std::max( 0, count - page ) );
	if ( row == p.currentRow ) return;
	p.currentRow = row;
	const picker::Row &chosen = p.rows[row - 1];
	if ( chosen.type == picker::RowType::File && p.kind == picker::Kind::SoundPicker ) {
		PlayLocalSound( p.current + chosen.name );
	}
}

/* FileChosen (0x00419740): a folder or the parent opens there; a file goes to
 * the subclass, which then closes its window (0x0041c42f, 0x0041cb3c,
 * 0x0041dc6c). True when the picker is to close. */
bool PickerChoose( FilePicker &p, int row ) {
	if ( row < 1 || row > static_cast<int>( p.rows.size() ) ) return false;
	const picker::Row chosen = p.rows[row - 1];
	if ( chosen.type == picker::RowType::Parent ) {
		const std::string parent = picker::ParentOf( p.current, p.root );
		if ( parent != p.current ) {
			p.current = parent;
			PickerSetupFiles( p );
		}
		return false;
	}
	if ( chosen.type == picker::RowType::Folder ) {
		p.current += chosen.name + "/";
		PickerSetupFiles( p );
		return false;
	}
	const std::string full = p.current + chosen.name;
	switch ( p.kind ) {
	case picker::Kind::ViewSpawner:
		Cbuf_AddText( picker::ViewSpawnCommand( full ).c_str() );
		break;
	case picker::Kind::MapRunner: {
		const std::string command = picker::MapCommand( full );
		if ( !command.empty() ) Cbuf_AddText( command.c_str() );
		break;
	}
	case picker::Kind::SoundPicker:
		// Cvar_Set2 without force (0x0040e2d0), and no sound.
		UiCvarSet( "ui_pickedsound", full.c_str() );
		break;
	}
	return true;
}

void ClosePicker( std::size_t index ) {
	if ( index >= pickers.size() ) return;
	pickers.erase( pickers.begin() + static_cast<std::ptrdiff_t>( index ) );
	if ( pickerKeys == static_cast<int>( index ) ) pickerKeys = -1;
	else if ( pickerKeys > static_cast<int>( index ) ) --pickerKeys;
}

bool PickerContains( const FilePicker &p, float vx, float vy ) {
	return Inside( vx, vy, p.x, p.y, p.w, p.minimized ? picker::TitleHeight : p.h );
}

int PickerAt( float vx, float vy ) {
	for ( int i = static_cast<int>( pickers.size() ) - 1; i >= 0; --i ) {
		if ( PickerContains( pickers[static_cast<std::size_t>( i )], vx, vy ) ) return i;
	}
	return -1;
}

bool PickerUnderCursor() {
	return cursorX >= 0 && PickerAt( VirtualCursorX(), VirtualCursorY() ) >= 0;
}

/* The list's `mouse_entered` makes it the active control (0x004ceef0), which
 * is what gives it the keys. */
void PickerHover() {
	const int at = cursorX >= 0 ? PickerAt( VirtualCursorX(), VirtualCursorY() ) : -1;
	if ( at < 0 ) return;
	const FilePicker &p = pickers[static_cast<std::size_t>( at )];
	float lx, ly, lw, lh;
	PickerList( p, lx, ly, lw, lh );
	if ( !p.minimized && Inside( VirtualCursorX(), VirtualCursorY(), lx, ly, lw, lh ) ) pickerKeys = at;
}

/* A press: the window under the cursor takes it (the window manager's hit
 * test, 0x004b3990) and comes to the top. */
bool PickerMouseDown() {
	if ( cursorX < 0 ) return false;
	const float vx = VirtualCursorX(), vy = VirtualCursorY();
	int index = PickerAt( vx, vy );
	if ( index < 0 ) {
		pickerKeys = -1;
		return false;
	}
	if ( index != static_cast<int>( pickers.size() ) - 1 ) {
		FilePicker raised = std::move( pickers[static_cast<std::size_t>( index )] );
		pickers.erase( pickers.begin() + index );
		pickers.push_back( std::move( raised ) );
		if ( pickerKeys == index ) pickerKeys = static_cast<int>( pickers.size() ) - 1;
		else if ( pickerKeys > index ) --pickerKeys;
		index = static_cast<int>( pickers.size() ) - 1;
	}
	FilePicker &p = pickers[static_cast<std::size_t>( index )];
	const float localX = vx - p.x, localY = vy - p.y;
	const int now = Now();
	// The title bar's two buttons (FrameInitialized 0x004c2170).
	const float buttonY = 2.0f;
	if ( localY >= buttonY && localY < buttonY + picker::ButtonSize ) {
		if ( localX >= p.w - picker::CloseRight && localX < p.w - picker::CloseRight + picker::ButtonSize ) {
			// floatingwindow_close_pressed (0x004c2ad0): the window goes.
			ClosePicker( static_cast<std::size_t>( index ) );
			return true;
		}
		if ( localX >= p.w - picker::MinimizeRight && localX < p.w - picker::MinimizeRight + picker::ButtonSize ) {
			// floatingwindow_minimize_pressed (0x004c2af0).
			p.minimized = !p.minimized;
			if ( p.minimized ) p.restoredH = p.h;
			return true;
		}
	}
	// left_mouse_down (0x004c2840): twice quickly minimises; the title drags.
	if ( p.titleClickTime + picker::DoubleClickMs > now && std::fabs( vx - p.titleClickX ) <= picker::DoubleClickSlop
		&& std::fabs( vy - p.titleClickY ) <= picker::DoubleClickSlop ) {
		p.minimized = !p.minimized;
		p.titleClickTime = -20000;   // 0x004c291b
		return true;
	}
	p.titleClickTime = now;
	p.titleClickX = vx;
	p.titleClickY = vy;
	if ( localY <= picker::TitleGrab ) {
		p.dragging = true;
		p.grabX = localX;
		p.grabY = localY;
		return true;
	}
	if ( p.minimized ) return true;
	float lx, ly, lw, lh;
	PickerList( p, lx, ly, lw, lh );
	if ( !Inside( vx, vy, lx, ly, lw, lh ) ) return true;
	pickerKeys = index;
	const float ix = vx - lx, iy = vy - ly;
	const int count = static_cast<int>( p.rows.size() );
	// The scrollbar, 16 wide at the list's right edge (0x004ce8ed).
	if ( ix >= lw - ScrollbarWidth ) {
		ScrollPress( p.hold, iy, lh, p.top, PickerPage( p ), count );
		return true;
	}
	const float header = PickerHeader();
	if ( iy < header ) {
		// A column edge resizes it, inside a column sorts by it (0x004ce9b0..0x004ceb25).
		float left = 0.0f;
		for ( int c = 0; c < 3; ++c ) {
			const float right = left + p.width[c];
			if ( std::fabs( ix - right ) <= picker::EdgeGrab ) {
				p.sizing = c;
				p.sizingLeft = left;
				return true;
			}
			if ( ix >= left && ix < right ) {
				picker::SortRows( p.rows, c, p.currentRow );
				return true;
			}
			left = right;
		}
		return true;
	}
	const int row = picker::RowAt( iy, header, PickerLine(), p.top );
	PickerSelect( p, row );
	if ( picker::DoubleClick( now, p.clickTime, p.currentRow, p.clickRow, vx, vy, p.clickX, p.clickY ) ) {
		p.clickTime = 0;   // 0x004cedff
		if ( PickerChoose( p, p.currentRow ) ) ClosePicker( static_cast<std::size_t>( index ) );
		return true;
	}
	p.clickTime = now;
	p.clickRow = p.currentRow;
	p.clickX = vx;
	p.clickY = vy;
	return true;
}

/* Drags go to the picker that holds the mouse. */
bool PickerMouseDragged() {
	if ( cursorX < 0 ) return false;
	const float vx = VirtualCursorX(), vy = VirtualCursorY();
	for ( FilePicker &p : pickers ) {
		if ( p.dragging ) {
			// left_mousedragged (0x004c2990): the window follows.
			p.x = vx - p.grabX;
			p.y = vy - p.grabY;
			return true;
		}
		if ( p.sizing >= 0 ) {
			float lx, ly, lw, lh;
			PickerList( p, lx, ly, lw, lh );
			p.width[p.sizing] = std::clamp( vx - lx - p.sizingLeft, 0.0f, picker::MaxColumnWidth );
			return true;
		}
		if ( p.hold.part ) {
			float lx, ly, lw, lh;
			PickerList( p, lx, ly, lw, lh );
			const int count = static_cast<int>( p.rows.size() ), page = PickerPage( p );
			ScrollDrag( p.hold, vx - ( lx + lw - ScrollbarWidth ), vy - ly, lh, count, [&p, count, page]( int value ) {
				const int top = std::clamp( value, 0, std::max( 0, count - page ) );
				const bool moved = top != p.top;
				p.top = top;
				return moved;
			} );
			return true;
		}
	}
	return false;
}

/* Letting go ends a drag, a resize and a scrollbar's hold (0x004c2a40,
 * 0x004cef00, 0x004d9200). True when a picker held the mouse. */
bool PickerMouseUp() {
	bool held = false;
	for ( FilePicker &p : pickers ) {
		held = held || p.dragging || p.sizing >= 0 || p.hold.part;
		p.dragging = false;
		p.sizing = -1;
		ScrollRelease( p.hold );
	}
	return held;
}

void PickerRepeats() {
	for ( FilePicker &p : pickers ) {
		const int count = static_cast<int>( p.rows.size() ), page = PickerPage( p );
		ScrollRepeat( p.hold, p.top, [&p, count, page]( int value ) {
			const int top = std::clamp( value, 0, std::max( 0, count - page ) );
			const bool moved = top != p.top;
			p.top = top;
			return moved;
		} );
	}
}

/* UIListBase::KeyEvent (0x004d43d0) for the list that has the keys: the
 * arrows step, the page keys page from the page's edge, Home and End go to
 * the ends and the wheel scrolls two rows. Enter and Escape are not its. */
bool PickerKey( int key ) {
	if ( pickerKeys < 0 || pickerKeys >= static_cast<int>( pickers.size() ) ) return false;
	FilePicker &p = pickers[static_cast<std::size_t>( pickerKeys )];
	if ( p.minimized ) return false;
	const int count = static_cast<int>( p.rows.size() ), page = std::max( 1, PickerPage( p ) );
	const int row = p.currentRow;
	switch ( key ) {
	case K_UPARROW:
	case K_LEFTARROW:
		PickerSelect( p, row - 1 );
		return true;
	case K_DOWNARROW:
	case K_RIGHTARROW:
		PickerSelect( p, row + 1 );
		return true;
	case K_PGUP:
		PickerSelect( p, row - 1 != p.top ? p.top + 1 : row - page );
		return true;
	case K_PGDN:
		PickerSelect( p, row != p.top + page ? p.top + page : row + page );
		return true;
	case K_HOME:
		PickerSelect( p, 1 );
		return true;
	case K_END:
		PickerSelect( p, count );
		return true;
	case K_MWHEELUP:
	case K_MWHEELDOWN:
		p.top = std::clamp( p.top + ( key == K_MWHEELUP ? -2 : 2 ), 0, std::max( 0, count - page ) );
		return true;
	default:
		return false;
	}
}

/* The window (0x004c2430) and its list (0x004ce3a0): a blue title bar with
 * white text while it is the one on top, grey with black otherwise; its two
 * marlett buttons; the list white with black text, its header grey boxes
 * with black titles, its chosen row yellow on blue; the scrollbar over its
 * right edge. Cells are cut at the list's edge, as retail's scissor cuts
 * its 400 units of columns in a 396-unit list. */
void DrawPicker( const FilePicker &p, bool active ) {
	static const float blue[4] = { 0, 0, 1, 1 };
	static const float white[4] = { 1, 1, 1, 1 };
	static const float black[4] = { 0, 0, 0, 1 };
	static const float yellow[4] = { 1, 1, 0, 1 };
	static const float grey[4] = { 0.75f, 0.75f, 0.75f, 1 };
	static const float inactive[4] = { 0.5f, 0.5f, 0.5f, 1 };
	static const mw::BorderColor border = mw::MakeBorderColor( { 0.75f, 0.75f, 0.75f, 1.0f } );
	static const int marlett = CL_AliceFontRegister( "marlett" );
	const int font = PickerFont();
	const float title = std::min( p.minimized ? picker::TitleHeight : p.h, picker::TitleHeight );
	if ( !p.minimized ) {
		VirtualFill( p.x, p.y, p.w, p.h, grey );
		Box3D( p.x, p.y + picker::TitleHeight, p.w, p.h - picker::TitleHeight, false, border, 1.0f );
	}
	VirtualFill( p.x, p.y, p.w, title, active ? blue : inactive );
	CL_AliceFontPrint( font, active ? white : black, false, p.x + 3.0f, p.y + 3.0f, p.current.c_str(), -1, 0.0f,
		0.0f, 0.0f, 1.0f, 1.0f );
	const struct { float right; const char *glyph; } buttons[] = {
		{ picker::CloseRight, "r" }, { picker::MinimizeRight, p.minimized ? "1" : "0" } };
	for ( const auto &button : buttons ) {
		const float bx = p.x + p.w - button.right, by = p.y + 2.0f;
		VirtualFill( bx, by, picker::ButtonSize, picker::ButtonSize, grey );
		Box3D( bx, by, picker::ButtonSize, picker::ButtonSize, false, border, 1.0f );
		const font::Rect rect{ bx, by, picker::ButtonSize, picker::ButtonSize };
		CL_AliceFontPrintJustified( marlett, black, false, rect, 0, 1, button.glyph, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f );
	}
	if ( p.minimized ) return;

	float lx, ly, lw, lh;
	PickerList( p, lx, ly, lw, lh );
	VirtualFill( lx, ly, lw, lh, white );
	const float header = PickerHeader(), line = PickerLine();
	const float sx = ScaleX(), sy = ScaleY();
	const float clip[4] = { lx * sx, ly * sy, std::max( lw - ScrollbarWidth, 0.0f ) * sx, lh * sy };
	CL_RitualSetClip( clip );
	float cx = lx;
	for ( int c = 0; c < 3; ++c ) {
		VirtualFill( cx, ly, p.width[c], header, grey );
		Box3D( cx, ly, p.width[c], header, false, border, 1.0f );
		CL_AliceFontPrint( font, black, false, cx + picker::HeaderPad, ly + picker::HeaderPad, picker::ColumnTitles[c],
			-1, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f );
		cx += p.width[c];
	}
	if ( cx < lx + lw ) {
		VirtualFill( cx, ly, lx + lw - cx, header, grey );
		Box3D( cx, ly, lx + lw - cx, header, false, border, 1.0f );
	}
	const int count = static_cast<int>( p.rows.size() );
	float ry = ly + header;
	for ( int row = p.top + 1; row <= count && ry < ly + lh; ++row, ry += line ) {
		const picker::Row &r = p.rows[static_cast<std::size_t>( row - 1 )];
		const bool chosen = row == p.currentRow;
		float rx = lx;
		for ( int c = 0; c < 3; ++c ) {
			if ( chosen ) VirtualFill( rx, ry, p.width[c], line, blue );
			CL_AliceFontPrint( font, chosen ? yellow : black, false, rx, ry, r.cells[c].c_str(), -1, 0.0f, 0.0f, 0.0f,
				1.0f, 1.0f );
			rx += p.width[c];
		}
	}
	CL_RitualSetClip( nullptr );
	DrawScrollbar( lx + lw - ScrollbarWidth, ly, lh, p.top, PickerPage( p ), count, p.hold.part );
}

/* UI_Update draws every window the window manager holds, menu up or not
 * (UpdateViews 0x004b3440, at 0x0040ca97). */
void DrawPickers() {
	for ( std::size_t i = 0; i < pickers.size(); ++i ) DrawPicker( pickers[i], i + 1 == pickers.size() );
}

void DrawMultiLineEdit( Menu &menu, int index, float x, float y ) {
	Widget &widget = menu.widgets[index];
	EditState &edit = widget.edit;
	const font::Font *metrics = CL_AliceFontMetrics( widget.font );
	const float fontH = static_cast<float>( FontHeight( widget ) );
	static const float white[4] = { 1, 1, 1, 1 };
	static const float selection[4] = { 0.0f, 0.0f, 0.5f, 1.0f };
	static const float black[4] = { 0, 0, 0, 1 };
	EditPoint from, to;
	EditOrdered( edit, from, to );
	const bool selecting = EditHasSelection( edit );
	float aty = 0.0f;
	for ( int line = edit.top; line < static_cast<int>( edit.lines.size() ) && aty < widget.h; ++line, aty += fontH ) {
		/* Each line whole, from any line whose top is in the box (fakk2.exe
		 * 1.02 0x004b760b, 0x004b76df); the widget's clip cuts what runs past
		 * the box (DrawWidget). */
		const std::string &text = edit.lines[line];
		if ( !selecting || line < from.line || line > to.line ) {
			WidgetPrint( widget, widget.fg, x, y + aty, text.c_str() );
		} else {
			const int length = static_cast<int>( text.size() );
			const int start = std::min( line == from.line ? from.column : 0, length );
			const int end = std::min( line == to.line ? to.column : length, length );
			const std::string before = text.substr( 0, start );
			const std::string inside = text.substr( start, std::max( 0, end - start ) );
			const std::string after = text.substr( std::max( start, end ) );
			const float beforeW = static_cast<float>( font::StringWidth( metrics, before.c_str() ) );
			const float insideW = static_cast<float>( font::StringWidth( metrics, inside.c_str() ) );
			WidgetPrint( widget, widget.fg, x, y + aty, before.c_str() );
			VirtualFill( x + beforeW, y + aty, insideW, fontH, selection );
			WidgetPrint( widget, white, x + beforeW, y + aty, inside.c_str() );
			WidgetPrint( widget, widget.fg, x + beforeW + insideW, y + aty, after.c_str() );
		}
		if ( line == edit.end.line && menu.focus == index && Now() % 750 >= 375 ) {
			const std::string upTo = text.substr( 0, std::min<std::size_t>( edit.end.column, text.size() ) );
			VirtualFill( x + static_cast<float>( font::StringWidth( metrics, upTo.c_str() ) ), y + aty, 2.0f, fontH, black );
		}
	}
	// Its scrollbar, 16 units at its right edge, over the text (0x004d9370).
	DrawScrollbar( x + widget.w - ScrollbarWidth, y, widget.h, edit.top, EditPage( widget ),
		static_cast<int>( edit.lines.size() ), edit.hold.part );
}

void DrawWidget( Menu &menu, int index, float parentAlpha ) {
	Widget &widget = menu.widgets[index];
	if ( !widget.shown ) return;
	// Waits fire only while their widget is drawn (0x004bd740), and a gone
	// menu's kept image is not drawn by its menu (MenuCarry).
	const bool showing = menu.visible && Now() >= menu.showStart;
	for ( Wait &wait : widget.waits ) {
		if ( showing && wait.fireAt >= 0 && Now() >= wait.fireAt ) {
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
	/* Heavy Metal: F.A.K.K.2 draws each widget in a 2D window of its own and
	 * scissors it to the widget cut to its menu and the screen (fakk2.exe 1.02
	 * 0x0049ee80; the cut 0x0049ec21..0x0049eceb), so nothing a widget draws
	 * runs past it: a report's last line stops at its box. */
	struct ClipScope {
		bool on = false;
		~ClipScope() { if ( on ) CL_RitualSetClip( nullptr ); }
	} clipScope;
	if ( Com_GameIsFAKK2() ) {
		float mx, my;
		MenuPosition( menu, mx, my );
		const float left = std::max( { vx, mx, 0.0f } ), top = std::max( { vy, my, 0.0f } );
		const float right = std::min( { vx + widget.w, mx + menu.w, 640.0f } );
		const float bottom = std::min( { vy + widget.h, my + menu.h, 480.0f } );
		const float clip[4] = { left * ScaleX(), top * ScaleY(), std::max( right - left, 0.0f ) * ScaleX(),
			std::max( bottom - top, 0.0f ) * ScaleY() };
		CL_RitualSetClip( clip );
		clipScope.on = true;
	}

	/* F.A.K.K.2's slider draws in the colour this draw last set (fakk2.exe
	 * 1.02 UIWidget::Display 0x0049ef30): white at the widget's alpha
	 * (0x0049efb0), the fill's, a 3D border's last strip, which is its light
	 * shade (0x0049a260), then the foreground under a shader (0x0049f0a2). */
	float inherited[4] = { 1.0f, 1.0f, 1.0f, alpha };
	// The background fill takes its alpha twice (0x004bd8ea, then DrawBox's 0x004b59a0).
	if ( widget.bg[3] != 0.0f ) {
		const float bg[4] = { widget.bg[0], widget.bg[1], widget.bg[2], widget.bg[3] * widget.bg[3] * alpha };
		VirtualFill( vx, vy, widget.w, widget.h, bg );
		std::copy( bg, bg + 4, inherited );
	}
	// A pressed button's border sinks; one drawn with none stays without.
	const int borderStyle = pressed && widget.border != 0 ? 2 : widget.border;
	DrawBorder( vx, vy, widget.w, widget.h, borderStyle, widget.border3d, alpha );
	if ( borderStyle == 1 || borderStyle == 2 ) {
		const mw::Rgba &light = widget.border3d.light;
		inherited[0] = light.r;
		inherited[1] = light.g;
		inherited[2] = light.b;
		inherited[3] = light.a * alpha;
	}
	/* Alice draws a widget's pictures white at its foreground alpha
	 * (0x004bd9ab..0x004bd9d8). Heavy Metal: F.A.K.K.2 draws them in the
	 * whole foreground colour (fakk2.exe 1.02 0x0049f078..0x0049f0a2): its
	 * menus' cloud layer is two additive `rgbGen global` passes under a 0.2
	 * grey foreground, which white would wash across the whole menu. */
	const bool tinted = Com_GameIsFAKK2() != 0;
	const float shade[4] = { tinted ? widget.fg[0] : 1.0f, tinted ? widget.fg[1] : 1.0f, tinted ? widget.fg[2] : 1.0f,
		widget.fg[3] * alpha };
	if ( !widget.shader.empty() && widget.tileShader ) {
		/* Retail hands the rectangle to the renderer's tile draw (0x004bcfe0),
		 * whose texture coordinates are position over the image's size
		 * (0x00498de0..0x00498e7a). Alice draws its menus over the whole screen
		 * in pixels, so its tiles are a texel a pixel from the screen's corner.
		 * F.A.K.K.2's UI draws each widget in a 2D window of its own, its corner
		 * the origin (fakk2.exe 1.02 0x0049ee80), and tiles from (0, 0) there
		 * (0x0049f0b5..0x0049f0cb), a texel a unit of its 640 by 480:
		 * what its frame edges are drawn to, a 512 by 16 strip of a 32 by 16
		 * image once across its height. */
		const qhandle_t handle = CL_AliceShader( widget.shader.c_str() );
		int imageW = 0, imageH = 0;
		if ( re.GetShaderImageSize ) re.GetShaderImageSize( handle, &imageW, &imageH );
		const float iw = imageW > 0 ? static_cast<float>( imageW ) : 0.0f;
		const float ih = imageH > 0 ? static_cast<float>( imageH ) : 0.0f;
		if ( Com_GameIsFAKK2() ) {
			VirtualPic( vx, vy, widget.w, widget.h, shade, handle, 0.0f, 0.0f, iw > 0.0f ? widget.w / iw : 1.0f,
				ih > 0.0f ? widget.h / ih : 1.0f );
		} else {
			const float px = vx * ScaleX(), py = vy * ScaleY(), pw = widget.w * ScaleX(), ph = widget.h * ScaleY();
			VirtualPic( vx, vy, widget.w, widget.h, shade, handle, iw > 0.0f ? px / iw : 0.0f, ih > 0.0f ? py / ih : 0.0f,
				iw > 0.0f ? ( px + pw ) / iw : 1.0f, ih > 0.0f ? ( py + ph ) / ih : 1.0f );
		}
	} else if ( !widget.shader.empty() ) {
		VirtualPic( vx, vy, widget.w, widget.h, shade, CL_AliceShader( widget.shader.c_str() ) );
	}
	if ( !widget.shader.empty() ) std::copy( shade, shade + 4, inherited );
	if ( !widget.staticImage.empty() ) {
		VirtualPic( vx, vy, widget.w, widget.h, shade, CL_AliceShader( widget.staticImage.c_str() ) );
	}
	if ( ( LightsFollowKeys( widget ) ? widget.lit : hovered ) && !widget.hoverShader.empty() ) {
		VirtualPic( vx, vy, widget.w, widget.h, shade, CL_AliceShader( widget.hoverShader.c_str() ) );
	}
	switch ( widget.kind ) {
	case WidgetKind::Label:
		// Every Label is a UIFakkLabel, whose draw is UILabel's without its keywords.
		if ( !DrawFakkLabel( widget, vx, vy ) ) DrawWidgetText( menu, widget, vx, vy, alpha, pressed );
		break;
	case WidgetKind::Button:
		DrawWidgetText( menu, widget, vx, vy, alpha, pressed );
		break;
	case WidgetKind::Pulldown:
		DrawPulldown( widget, vx, vy, alpha );
		break;
	case WidgetKind::MultiLineEdit:
		DrawMultiLineEdit( menu, index, vx, vy );
		break;
	case WidgetKind::CheckBox:
		DrawCheckBox( menu, index, vx, vy, alpha );
		break;
	case WidgetKind::Slider:
		DrawSlider( menu, index, vx, vy, inherited, alpha );
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
		if ( Com_GameIsFAKK2() ) DrawFakk2BindList( widget, vx, vy, alpha );
		else DrawBindList( menu, widget, vx, vy, alpha );
		break;
	case WidgetKind::Alice3D:
		DrawAlice3D( widget );
		break;
	case WidgetKind::LoadGame:
		DrawSaveList( widget, vx, vy, alpha );
		break;
	}
}

/* A menu at its own fade; or, for the image a full-screen menu leaves
 * (MenuCarry), its pictures alone at `keptAlpha`, whether it shows or not. */
void DrawMenu( Menu &menu, float keptAlpha = -1.0f, bool kept = false ) {
	if ( !kept && ( !menu.visible || Now() < menu.showStart ) ) return;
	if ( !kept && menu.hasBgFill && menu.bgFill[3] != 0.0f ) {
		CL_AliceFill( 0, 0, static_cast<float>( cls.glconfig.vidWidth ), static_cast<float>( cls.glconfig.vidHeight ),
			menu.bgFill );
	}
	float x, y;
	MenuPosition( menu, x, y );
	const float fade = kept ? keptAlpha : MenuFade( menu );
	if ( !kept && menu.bg[3] != 0.0f ) {
		const float bg[4] = { menu.bg[0], menu.bg[1], menu.bg[2], menu.bg[3] * menu.bg[3] * fade };
		VirtualFill( x, y, menu.w, menu.h, bg );
	}
	DrawBorder( x, y, menu.w, menu.h, menu.border, menu.border3d, fade );
	for ( std::size_t i = 0; i < menu.widgets.size(); ++i ) {
		DrawWidget( menu, static_cast<int>( i ), fade );
	}
	if ( kept ) return;
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

/* ---- what F.A.K.K.2's screen keeps under its full-screen menus -------------- */

/* fakk2.exe 1.02's UI update clears nothing while a full-screen menu is up
 * (0x0040b0cf..0x0040b296 blacks the screen only for loading, before a game
 * and after one), so each frame's menus are drawn over the last frame's
 * image. Every full-screen menu fades (`fadein 0.5`) and fills itself white
 * (`bgcolor 1 1 1 1`) under its pictures, both at the fade's alpha, so a
 * menu going hazes to white over its own image, and the next comes through
 * that haze. A retail capture of the main menu giving way to the options
 * shows both, where this engine had faded them through black.
 *
 * That is the retail engine's presentation, so here it is an option,
 * `ui_menuDissolve 1` (AGENTS.md invariant 11); by default this engine
 * blacks the screen under the menus, and they fade over black.
 *
 * This engine's frames keep no image, so the kept image is held as shares of
 * what was drawn into it: black or the game's view it started from, and each
 * menu's fill and pictures. Drawing a layer at alpha a leaves every share
 * 1 - a of itself and adds a to the layer's own, as drawing over the kept
 * image does. The shares are drawn back each frame, the lowest first, each at
 * the alpha that leaves it its share. A menu keeps its shares once it has
 * gone, until others cover them. */
enum Kept { KeptBlack, KeptView, KeptFill, KeptArt };

struct MenuCarry {
	bool active = false;
	std::vector<mw::KeptLayer> layers;   // the lowest first
	int frame = -1;                      // the cls.framecount the shares last moved in
};
MenuCarry carry;

bool CarryDraws( const Menu &menu ) {
	return !menu.hud && menu.fullscreen && menu.visible && Now() >= menu.showStart;
}

/* The frame's draws into the kept image, once a frame whatever the stereo
 * passes, in the menus' draw order: each full-screen menu's fill, then its
 * pictures (DrawMenu). */
void PlanCarry( bool viewShown ) {
	if ( !carry.active ) {
		carry = MenuCarry();
		carry.active = true;
		mw::KeptLayer start;
		start.kind = viewShown ? KeptView : KeptBlack;
		start.share = 1.0f;
		carry.layers.push_back( start );
	}
	if ( carry.frame == cls.framecount ) return;
	carry.frame = cls.framecount;
	for ( std::size_t i = 0; i < menus.size(); ++i ) {
		const Menu &menu = menus[i];
		if ( !CarryDraws( menu ) ) continue;
		const int index = static_cast<int>( i );
		const float fade = MenuFade( menu );
		if ( menu.bg[3] != 0.0f ) mw::KeepDraw( carry.layers, KeptFill, index, menu.bg[3] * menu.bg[3] * fade );
		mw::KeepDraw( carry.layers, KeptArt, index, fade );
	}
	mw::DropFaintLayers( carry.layers );
}

/* The kept image drawn back over the black or the view the screen code has
 * drawn, which is always its lowest layer. */
void DrawKept() {
	const std::vector<float> alpha = mw::KeptAlphas( carry.layers );
	for ( std::size_t i = 0; i < carry.layers.size(); ++i ) {
		const mw::KeptLayer &layer = carry.layers[i];
		if ( !( alpha[i] > 0.0f ) ) continue;
		if ( layer.kind == KeptBlack || layer.kind == KeptView ) continue;
		if ( layer.menu < 0 || layer.menu >= static_cast<int>( menus.size() ) ) continue;
		Menu &menu = menus[layer.menu];
		if ( layer.kind == KeptFill ) {
			float x, y;
			MenuPosition( menu, x, y );
			const float fill[4] = { menu.bg[0], menu.bg[1], menu.bg[2], alpha[i] };
			VirtualFill( x, y, menu.w, menu.h, fill );
		} else {
			DrawMenu( menu, alpha[i], true );
		}
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
	carry = MenuCarry();
	captureMenu = captureWidget = -1;
	pendingSave.active = false;
	popup = {};
	SetGfxInfo();
	// F.A.K.K.2's inventory script, read as its UI starts (0x004105f2).
	CL_Fakk2InventoryInit();
	LoadTables();
	LoadMenus();
	// An open picker outlives the restart; its folder is read again.
	RebuildPickers();
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
	popup = {};
	CL_Fakk2InventoryShutdown();
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
	carry = MenuCarry();
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

	// F.A.K.K.2's inventory's timeout, and its closing in a cinematic.
	CL_Fakk2InventoryFrame();

	// Scrollbar steps that are due run ahead of the mouse (0x0042c2aa).
	if ( Menu *menu = CurrentMenu() ) ScrollRepeats( *menu );
	PickerRepeats();

	/* Cursor mode: on while a menu is up (the push path's 0x00427b00), off
	 * once none remains or ui_hidemouse gives the view the mouse (0x00427b50).
	 * While it is on the pointer moves only the cursor and the mouse buttons
	 * stop at its mask, so the menus read both from there (0x004282b0) and
	 * the window manager's events follow from them (0x004b3a50). F.A.K.K.2's
	 * inventory opened by `inventory` turns it on too (0x00408570). */
	const bool inventoryCursor = menuStack.empty() && CL_Fakk2InventoryWantsCursor();
	const bool wantCursor = ( !menuStack.empty() && !viewHasMouse ) || inventoryCursor;
	if ( wantCursor != cursorModeOn ) {
		CL_AliceSetCursorMode( wantCursor ? qtrue : qfalse );
		cursorModeOn = wantCursor;
		cursorButtons = 0;
		// The release this loses would never come: no bar keeps the mouse.
		if ( Menu *menu = CurrentMenu() ) ScrollReleases( *menu );
		PickerMouseUp();
	}
	if ( cursorModeOn ) {
		int x = 0, y = 0, buttons = 0;
		CL_AliceGetMouseState( &x, &y, &buttons );
		cursorX = static_cast<float>( x );
		cursorY = static_cast<float>( y );
		const float vx = VirtualCursorX(), vy = VirtualCursorY();
		const bool moved = vx != lastVirtualX || vy != lastVirtualY;
		if ( inventoryCursor ) {
			if ( moved ) CL_Fakk2InventoryMouseMoved( vx, vy );
			for ( int button = 0; button < 2; ++button ) {
				if ( buttons & ~cursorButtons & ( 1 << button ) ) CL_Fakk2InventoryMouseDown( K_MOUSE1 + button, vx, vy );
			}
		} else if ( PulldownOpen() ) {
			/* An open pulldown holds the mouse (its first responder): the
			 * motion tracks the list, and letting go chooses (0x004e2960). */
			if ( moved ) PulldownDragged( vx, vy );
			if ( ( cursorButtons & 1 ) && !( buttons & 1 ) ) ReleasePulldown( vx, vy );
			if ( !PulldownOpen() ) UpdateHover();
		} else {
			UpdateHover();
			PickerHover();
			Menu *menu = CurrentMenu();
			if ( menu && ( buttons & cursorButtons & 1 ) && moved && !PickerMouseDragged() ) {
				MouseDragged( *menu );
			}
			if ( ( buttons ^ cursorButtons ) & 1 ) {
				KeyEvent( K_MOUSE1, ( buttons & 1 ) != 0 );
			}
			// The other buttons matter only to a key button waiting for a key.
			for ( int button = 1; button < 5; ++button ) {
				if ( ( buttons & ~cursorButtons & ( 1 << button ) ) && Capturing() ) CaptureMouse( K_MOUSE1 + button );
			}
		}
		lastVirtualX = vx;
		lastVirtualY = vy;
		cursorButtons = buttons;
	} else {
		UpdateHover();
	}
	if ( Menu *menu = CurrentMenu() ) {
		for ( Widget &widget : menu->widgets ) {
			if ( widget.kind == WidgetKind::Slider ) SliderRepeat( *menu, widget );
			else if ( widget.kind == WidgetKind::MultiLineEdit ) EditRepeat( *menu, widget );
		}
	}

	/* ui_newvidmode follows r_mode while a menu is up, for the video menu's
	 * pending resolution: -1 with no menu, r_mode once one shows (0x0040c3f0,
	 * after each change of the menus). */
	if ( menuStack.empty() ) {
		if ( Cvar_VariableIntegerValue( "ui_newvidmode" ) != -1 ) Cvar_Set( "ui_newvidmode", "-1" );
	} else if ( Cvar_VariableIntegerValue( "ui_newvidmode" ) == -1 && Cvar_VariableIntegerValue( "r_mode" ) != -1 ) {
		Cvar_Set( "ui_newvidmode", Cvar_VariableString( "r_mode" ) );
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
	const bool wantKeys = ( !menuStack.empty() && !viewHasMouse ) || cls.state == CA_DISCONNECTED
		|| CL_Fakk2InventoryIsOpen();
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

/* The desktop's windows in their order (0x004bdac1..0x004bdb0a, later ones on
 * top): F.A.K.K.2's HUD menus, which show only with no menu up, under its
 * inventory, under the menus, under a pulldown's open list.
 *
 * Every menu is drawn in its 640 by 480 units stretched to the screen, but the
 * HUD menus are the game's HUD: with cl_hudAspect on, the HUD's rules keep
 * each uniform and anchored where its fntech3-hud.json rule says, as they do
 * Quake III's and RTCW's HUDs (AGENTS.md invariant 11). */
void CL_AliceUIDrawMenus( void ) {
	CL_HudBeginFrame();
	CL_RitualSetHudLayout( true );
	for ( Menu &menu : menus ) {
		if ( menu.hud ) DrawMenu( menu );
	}
	CL_RitualSetHudLayout( false );
	CL_HudEndFrame();
	CL_Fakk2InventoryDraw();
	// F.A.K.K.2's full-screen menus are in the image its screen keeps.
	if ( carry.active ) DrawKept();
	for ( Menu &menu : menus ) {
		if ( menu.hud ) continue;
		if ( carry.active && menu.fullscreen ) {
			if ( CarryDraws( menu ) ) {
				for ( const Widget &widget : menu.widgets ) {
					if ( widget.kind == WidgetKind::BindList && widget.shown ) DrawPressAnyKey( widget );
				}
			}
			continue;
		}
		DrawMenu( menu );
	}
	DrawPickers();
	DrawPopup();
}

bool CL_RitualMenuCarry( bool viewShown ) {
	if ( !Com_GameIsFAKK2() || !uiInitialized || !ui_menuDissolve || !ui_menuDissolve->integer ) {
		carry.active = false;
		return false;
	}
	PlanCarry( viewShown );
	for ( const mw::KeptLayer &layer : carry.layers ) {
		if ( layer.kind == KeptView ) return true;
	}
	return false;
}

void CL_RitualEndMenuCarry( void ) {
	carry.active = false;
}

/*
====================
CL_RitualHudMenusFrame

Heavy Metal: F.A.K.K.2's HUD is four of its menus, shown rather than pushed,
by its engine's rule (fntech3::fakk2::hud::Select) whenever its gate is open
(CL_AliceHudFrame: ui_hud, in the game, no letterbox or menu), at once:
fakk2.exe 1.02's UI update (0x0040b29b..0x0040b326) has no delay, where
Alice's HUD waits 550 ms. A menu the rule drops moves out as a menu hides;
the gate's closing takes them all at once (0x0040b59c).
====================
*/
void CL_RitualHudMenusFrame( bool allowed ) {
	if ( !uiInitialized ) return;
	auto set = []( const char *name, bool show, bool force ) {
		const int index = FindMenu( name );
		if ( index < 0 ) return;
		Menu &menu = menus[index];
		if ( show ) {
			if ( !menu.hud || !menu.visible || menu.hiding ) {
				menu.hud = true;
				ShowMenu( menu, 0 );
			}
		} else if ( menu.hud && menu.visible && ( force || !menu.hiding ) ) {
			HideMenu( menu, force );
		}
	};
	if ( !allowed ) {
		set( fakkhud::HealthHud, false, true );
		set( fakkhud::WeaponHud, false, true );
		set( fakkhud::DualWeaponHud, false, true );
		set( fakkhud::BossHud, false, true );
		return;
	}
	const fakkhud::Selection shown = fakkhud::Select( cl.snap.ps.alice.block[0], cl.snap.ps.alice.block[1] );
	set( fakkhud::HealthHud, shown.health, false );
	set( fakkhud::WeaponHud, shown.weapon, false );
	set( fakkhud::DualWeaponHud, shown.dualWeapon, false );
	set( fakkhud::BossHud, shown.boss, false );
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
	const bool inventoryCursor = menuStack.empty() && CL_Fakk2InventoryWantsCursor();
	if ( ( menuStack.empty() && !inventoryCursor ) || !cursorShader || cursorX < 0 ) return;
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
