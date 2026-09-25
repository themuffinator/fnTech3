/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Q3Test 1.05's menus.
 *
 * 1.05 ships no menu module: its menus are engine code, a Quake II-style
 * stack of full-screen and overlay menus hooked where 1.06 later called its
 * module. This is this engine's equivalent behind the same calls. What it must
 * keep is what each menu drives -- the cvars it writes, the commands it runs,
 * the key catcher bit and the connection screen's text -- and it draws them
 * in the 640x480 space with the release's own font and pictures
 * (invariant 11: the presentation is this engine's). */
extern "C" {
#include "client.h"
}
#include "client_q3test.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace q3cl = fntech3::q3test::client;

namespace {

constexpr float kWhite[4] = { 1, 1, 1, 1 };
constexpr float kGreen[4] = { 0, 1, 0, 1 };
constexpr float kYellow[4] = { 1, 1, 0, 1 };
constexpr float kGrey[4] = { 0.5f, 0.5f, 0.5f, 1 };
constexpr float kShade[4] = { 0, 0, 0, 0.5f };

enum class ItemType { Action, Spin, Slider, YesNo, Field, Separator, Binding };

struct Item {
	ItemType type = ItemType::Action;
	std::string label;
	std::vector<std::string> options;  // spin
	int current = 0;                   // spin, yes/no
	float minimum = 0, maximum = 0, value = 0;  // slider
	std::string text;                  // field
	int maxChars = 0;                  // field
	std::string command;               // binding
	bool greyed = false;
	std::function<void()> activate;    // Enter
	std::function<void( Item & )> changed;  // left and right
};

struct Menu {
	std::string title;
	std::vector<Item> items;
	int cursor = 0;
	bool fullscreen = true;
	std::function<void()> escape;      // runs before the menu pops
	std::function<void()> draw;        // anything beside the items
	bool lanList = false;
};

struct Menus {
	std::vector<Menu> stack;
	bool ingame = false;
	bool grabbing = false;             // a binding row waits for its key
	std::string connectCount, serverMessage;
	bool enterSound = false;
};
Menus m;

void PlaySound( const char *name ) {
	S_StartLocalSound( S_RegisterSound( name, qfalse ), 1 );
}

int Catcher() { return Key_GetCatcher(); }
void SetMenuCatcher( bool exact ) {
	Key_SetCatcher( exact ? ( ( Catcher() & ~7 ) | KEYCATCH_UI ) : ( Catcher() | KEYCATCH_UI ) );
}

/* M_ForceMenuOff (0x420d50): the stack is cleared, the menu bit dropped and
 * held keys released; retail also writes the unused cvar `paused` and, when
 * disconnected, `d1`, which runs nothing in a stock installation. */
void ForceMenuOff() {
	m.stack.clear();
	m.grabbing = false;
	Key_SetCatcher( Catcher() & ~KEYCATCH_UI );
	Key_ClearStates();
	Cvar_Set( "paused", "0" );
}

/* M_PushMenu (0x420cb0): depth 8, the menu bit or-ed in. */
void Push( Menu menu ) {
	if ( m.stack.size() >= 8 ) Com_Error( ERR_FATAL, "M_PushMenu: MAX_MENU_DEPTH" );
	m.stack.push_back( std::move( menu ) );
	SetMenuCatcher( false );
	m.enterSound = true;
}

void Pop() {
	if ( m.stack.empty() ) Com_Error( ERR_FATAL, "M_PopMenu: depth < 1" );
	PlaySound( "sound/misc/menu3.wav" );
	m.stack.pop_back();
	if ( m.stack.empty() ) ForceMenuOff();
}

void Execute( const char *text ) {
	Cbuf_AddText( text );
	Cbuf_Execute();
}

Item Action( const char *label, std::function<void()> activate ) {
	Item item;
	item.type = ItemType::Action;
	item.label = label;
	item.activate = std::move( activate );
	return item;
}

Item Spin( const char *label, std::vector<std::string> options, int current, std::function<void( Item & )> changed ) {
	Item item;
	item.type = ItemType::Spin;
	item.label = label;
	item.options = std::move( options );
	item.current = std::max( 0, std::min( current, static_cast<int>( item.options.size() ) - 1 ) );
	item.changed = std::move( changed );
	return item;
}

Item YesNo( const char *label, bool on, std::function<void( Item & )> changed ) {
	Item item;
	item.type = ItemType::YesNo;
	item.label = label;
	item.current = on ? 1 : 0;
	item.changed = std::move( changed );
	return item;
}

Item Slider( const char *label, float minimum, float maximum, float value, std::function<void( Item & )> changed ) {
	Item item;
	item.type = ItemType::Slider;
	item.label = label;
	item.minimum = minimum;
	item.maximum = maximum;
	item.value = std::max( minimum, std::min( value, maximum ) );
	item.changed = std::move( changed );
	return item;
}

Item Field( const char *label, const char *text, int maxChars ) {
	Item item;
	item.type = ItemType::Field;
	item.label = label;
	item.text = text;
	item.maxChars = maxChars;
	return item;
}

Item Separator( const char *label ) {
	Item item;
	item.type = ItemType::Separator;
	item.label = label;
	return item;
}

bool Selectable( const Item &item ) { return item.type != ItemType::Separator && !item.greyed; }

/* ---- the menus ---------------------------------------------------------------- */

void OpenMain();
void OpenPlayer();
void OpenControls();
void OpenSystem();
void OpenQuit();
void OpenStartServer();
void OpenLan();
void OpenInternet();

// Customize Player (0x4220a0): applied on Escape.
void OpenPlayer() {
	Menu menu;
	menu.title = "PLAYER";
	char skin[MAX_CVAR_VALUE_STRING];
	Cvar_VariableStringBuffer( "model", skin, sizeof( skin ) );
	menu.items.push_back( Field( "name", Cvar_VariableString( "name" ), 16 ) );
	menu.items.push_back( Spin( "skin", { "red", "blue" }, strstr( skin, "blue" ) ? 1 : 0, nullptr ) );
	menu.items.push_back( Spin( "effects color", { "blue", "green", "cyan", "red", "magenta", "yellow", "white" },
		Cvar_VariableIntegerValue( "color" ) - 1, nullptr ) );
	const int handicap = Cvar_VariableIntegerValue( "handicap" );
	menu.items.push_back( Spin( "handicap", { "25", "50", "75", "100" }, handicap <= 25 ? 0 : handicap <= 50 ? 1 : handicap <= 75 ? 2 : 3, nullptr ) );
	menu.escape = [] {
		const Menu &self = m.stack.back();
		Cvar_Set( "name", self.items[0].text.c_str() );
		Cvar_Set( "model", va( "visor/%s", self.items[1].options[static_cast<std::size_t>( self.items[1].current )].c_str() ) );
		Cvar_Set( "color", va( "%i", self.items[2].current + 1 ) );
		Cvar_Set( "handicap", self.items[3].options[static_cast<std::size_t>( self.items[3].current )].c_str() );
	};
	menu.fullscreen = !m.ingame;
	Push( std::move( menu ) );
}

// The binding rows of Customize Controls (0x46bdd4).
const char *const kBindings[][2] = {
	{ "show scores", "+scores" }, { "run / walk", "+speed" }, { "walk forward", "+forward" },
	{ "backpedal", "+back" }, { "step left", "+moveleft" }, { "step right", "+moveright" },
	{ "up / jump", "+moveup" }, { "down / crouch", "+movedown" }, { "turn left", "+left" },
	{ "turn right", "+right" }, { "sidestep / turn", "+strafe" }, { "look up", "+lookup" },
	{ "look down", "+lookdown" }, { "mouse look", "+mlook" }, { "center view", "centerview" },
	{ "zoom view", "+zoom" }, { "gauntlet", "weapon 1" }, { "machinegun", "weapon 2" },
	{ "shotgun", "weapon 3" }, { "grenade launcher", "weapon 4" }, { "rocket launcher", "weapon 5" },
	{ "lightning", "weapon 6" }, { "railgun", "weapon 7" }, { "plasma gun", "weapon 8" },
	{ "BFG", "weapon 9" }, { "weapon 10", "weapon 10" }, { "weapon 11", "weapon 11" },
	{ "attack", "+attack" }, { "prev weapon", "weapprev" }, { "next weapon", "weapnext" },
	{ "use item", "+button2" },
};

std::string BoundKeys( const char *command ) {
	std::string out;
	int found = 0;
	for ( int key = 0; key < MAX_KEYS && found < 2; ++key ) {
		const char *binding = Key_GetBinding( key );
		if ( binding && !Q_stricmp( binding, command ) ) {
			if ( found++ ) out += " or ";
			out += Key_KeynumToString( key );
		}
	}
	return found ? out : "???";
}

void OpenControls() {
	Menu menu;
	menu.title = "CONTROLS";
	menu.items.push_back( Separator( "MOVEMENT" ) );
	for ( const auto &row : kBindings ) {
		Item item;
		item.type = ItemType::Binding;
		item.label = row[0];
		item.command = row[1];
		menu.items.push_back( item );
	}
	menu.items.push_back( Separator( "MOUSE" ) );
	menu.items.push_back( YesNo( "always run", Cvar_VariableIntegerValue( "cl_run" ) != 0,
		[]( Item &item ) { Cvar_SetValue( "cl_run", static_cast<float>( item.current ) ); } ) );
	menu.items.push_back( YesNo( "free look", Cvar_VariableIntegerValue( "freelook" ) != 0,
		[]( Item &item ) { Cvar_SetValue( "freelook", static_cast<float>( item.current ) ); } ) );
	menu.items.push_back( YesNo( "invert mouse", Cvar_VariableValue( "m_pitch" ) < 0,
		[]( Item &item ) {
			const float pitch = std::fabs( Cvar_VariableValue( "m_pitch" ) );
			Cvar_SetValue( "m_pitch", item.current ? -pitch : pitch );
		} ) );
	menu.items.push_back( Slider( "mouse speed", 2, 30, Cvar_VariableValue( "sensitivity" ),
		[]( Item &item ) { Cvar_SetValue( "sensitivity", item.value ); } ) );
	menu.items.push_back( Separator( "WEAPONS" ) );
	menu.items.push_back( YesNo( "autoswitch weapons", Cvar_VariableIntegerValue( "cg_autoswitch" ) != 0,
		[]( Item &item ) { Cvar_SetValue( "cg_autoswitch", static_cast<float>( item.current ) ); } ) );
	menu.cursor = 1;
	menu.fullscreen = !m.ingame;
	Push( std::move( menu ) );
}

void OpenPreferences() {
	Menu menu;
	menu.title = "PREFERENCES";
	auto toggle = []( const char *label, const char *cvar ) {
		return Spin( label, { "disabled", "enabled" }, Cvar_VariableIntegerValue( cvar ) ? 1 : 0,
			[cvar]( Item &item ) { Cvar_SetValue( cvar, static_cast<float>( item.current ) ); } );
	};
	menu.items.push_back( toggle( "marks on walls", "cg_marks" ) );
	menu.items.push_back( toggle( "dynamic lights", "r_dynamiclight" ) );
	menu.items.push_back( toggle( "light flares", "r_flares" ) );
	menu.items.push_back( toggle( "identify target", "cg_drawCrosshairNames" ) );
	menu.items.push_back( toggle( "sync every frame", "r_finish" ) );
	menu.items.push_back( Spin( "sky", { "high quality", "high speed" }, Cvar_VariableIntegerValue( "r_fastsky" ) ? 1 : 0,
		[]( Item &item ) { Cvar_SetValue( "r_fastsky", static_cast<float>( item.current ) ); } ) );
	menu.fullscreen = !m.ingame;
	Push( std::move( menu ) );
}

// The video modes the renderer numbers 0-11 (0x424720).
const std::vector<std::string> kModes = { "320 240", "400 300", "512 384", "640 480", "800 600", "960 720",
	"1024 768", "1152 864", "1280 1024", "1600 1200", "2048 1536", "856 480 W" };

void OpenGraphics() {
	Menu menu;
	menu.title = "GRAPHICS";
	menu.items.push_back( Spin( "OpenGL extensions", { "off", "on" }, Cvar_VariableIntegerValue( "r_allowExtensions" ) ? 1 : 0, nullptr ) );
	menu.items.push_back( Spin( "video mode", kModes, Cvar_VariableIntegerValue( "r_mode" ), nullptr ) );
	const int bits = Cvar_VariableIntegerValue( "r_colorbits" );
	menu.items.push_back( Spin( "color depth", { "default", "16 bit", "32 bit" }, bits == 16 ? 1 : bits == 32 ? 2 : 0, nullptr ) );
	menu.items.push_back( Spin( "fullscreen", { "off", "on" }, Cvar_VariableIntegerValue( "r_fullscreen" ) ? 1 : 0, nullptr ) );
	menu.items.push_back( Spin( "lighting", { "lightmap", "vertex" }, Cvar_VariableIntegerValue( "r_vertexLight" ) ? 1 : 0, nullptr ) );
	menu.items.push_back( Spin( "geometric detail", { "high", "low" }, Cvar_VariableIntegerValue( "r_lodBias" ) > 0 ? 1 : 0, nullptr ) );
	menu.items.push_back( Slider( "texture detail", 0, 3, 3.0f - Cvar_VariableValue( "r_picmip" ), nullptr ) );
	const int texbits = Cvar_VariableIntegerValue( "r_texturebits" );
	menu.items.push_back( Spin( "texture quality", { "compressed", "16 bit", "32 bit" },
		Cvar_VariableIntegerValue( "r_ext_compress_textures" ) ? 0 : texbits == 16 ? 1 : 2, nullptr ) );
	char filter[MAX_CVAR_VALUE_STRING];
	Cvar_VariableStringBuffer( "r_textureMode", filter, sizeof( filter ) );
	menu.items.push_back( Spin( "texture filter", { "bilinear", "trilinear" }, Q_stricmp( filter, "GL_LINEAR_MIPMAP_NEAREST" ) ? 1 : 0, nullptr ) );
	menu.items.push_back( Action( "apply", [] {
		const Menu &self = m.stack.back();
		Cvar_SetValue( "r_allowExtensions", static_cast<float>( self.items[0].current ) );
		Cvar_SetValue( "r_mode", static_cast<float>( self.items[1].current ) );
		static const int color[3][3] = { { 0, 0, 0 }, { 16, 16, 0 }, { 32, 24, -1 } };
		const int *depth = color[self.items[2].current];
		Cvar_SetValue( "r_colorbits", static_cast<float>( depth[0] ) );
		Cvar_SetValue( "r_depthbits", static_cast<float>( depth[1] ) );
		if ( depth[2] >= 0 ) Cvar_SetValue( "r_stencilbits", static_cast<float>( depth[2] ) );
		Cvar_SetValue( "r_fullscreen", static_cast<float>( self.items[3].current ) );
		Cvar_SetValue( "r_vertexLight", static_cast<float>( self.items[4].current ) );
		Cvar_SetValue( "r_lodBias", static_cast<float>( self.items[5].current ) );
		Cvar_SetValue( "r_subdivisions", self.items[5].current ? 999.0f : 4.0f );
		Cvar_SetValue( "r_picmip", 3.0f - self.items[6].value );
		const int quality = self.items[7].current;
		Cvar_SetValue( "r_ext_compress_textures", quality == 0 ? 1.0f : 0.0f );
		Cvar_SetValue( "r_texturebits", quality == 1 ? 16.0f : quality == 2 ? 32.0f : 0.0f );
		Cvar_Set( "r_textureMode", self.items[8].current ? "GL_LINEAR_MIPMAP_LINEAR" : "GL_LINEAR_MIPMAP_NEAREST" );
		ForceMenuOff();
		Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
	} ) );
	menu.fullscreen = !m.ingame;
	Push( std::move( menu ) );
}

void OpenSystem() {
	Menu menu;
	menu.title = "SYSTEM";
	menu.items.push_back( Slider( "effects volume", 0, 10, Cvar_VariableValue( "s_volume" ) * 10,
		[]( Item &item ) { Cvar_SetValue( "s_volume", item.value / 10 ); } ) );
	Item brightness = Slider( "brightness", 5, 30, Cvar_VariableValue( "r_gamma" ) * 10,
		[]( Item &item ) { Cvar_SetValue( "r_gamma", item.value / 10 ); } );
	brightness.greyed = !cls.glconfig.deviceSupportsGamma;
	menu.items.push_back( brightness );
	menu.items.push_back( Slider( "screen size", 3, 10, Cvar_VariableValue( "viewsize" ) / 10,
		[]( Item &item ) { Cvar_SetValue( "viewsize", item.value * 10 ); } ) );
	menu.items.push_back( YesNo( "high quality sound", Cvar_VariableIntegerValue( "s_khz" ) == 22,
		[]( Item &item ) {
			Cvar_SetValue( "s_khz", item.current ? 22.0f : 11.0f );
			Cvar_SetValue( "s_loadas8bit", item.current ? 0.0f : 1.0f );
			ForceMenuOff();
			Cbuf_ExecuteText( EXEC_APPEND, "snd_restart\n" );
		} ) );
	menu.items.push_back( Action( "preferences", OpenPreferences ) );
	menu.items.push_back( Action( "graphics options", OpenGraphics ) );
	menu.items.push_back( Action( "reset defaults", [] {
		Execute( "exec default.cfg\n" );
		Cbuf_ExecuteText( EXEC_APPEND, "cvar_restart\n" );
		ForceMenuOff();
	} ) );
	menu.fullscreen = !m.ingame;
	Push( std::move( menu ) );
}

void OpenQuit() {
	Menu menu;
	menu.title = "QUIT";
	menu.draw = [] {
		const float x = 320 - q3cl::BigStringWidth( "Quit Q3Test? (y/n)" ) / 2.0f;
		q3cl::DrawBigString( static_cast<int>( x ), 232, "Quit Q3Test? (y/n)", kWhite, true );
	};
	menu.fullscreen = !m.ingame;
	Push( std::move( menu ) );
}

// Start Game Server (0x4241a0).
void OpenStartServer() {
	Menu menu;
	menu.title = "START SERVER";
	menu.items.push_back( Action( "START", [] {
		const Menu &self = m.stack.back();
		Cvar_Set( "dedicated", va( "%i", self.items[2].current ) );
		Cvar_Set( "timelimit", self.items[3].text.c_str() );
		Cvar_Set( "fraglimit", self.items[4].text.c_str() );
		Cvar_Set( "sv_maxclients", va( "%i", std::max( 0, std::min( atoi( self.items[5].text.c_str() ), 16 ) ) ) );
		Cvar_Set( "sv_hostname", self.items[6].text.c_str() );
		const std::string map = self.items[1].options[static_cast<std::size_t>( self.items[1].current )];
		ForceMenuOff();
		Execute( va( "wait ; wait ; map %s\n", map.c_str() ) );
	} ) );
	menu.items.push_back( Spin( "initial map", { "Q3TEST1", "Q3TEST2" }, 0, nullptr ) );
	menu.items.push_back( Spin( "dedicated server", { "no", "LAN", "Internet" }, 0, nullptr ) );
	menu.items.push_back( Field( "time limit", Cvar_VariableString( "timelimit" ), 3 ) );
	menu.items.push_back( Field( "frag limit", Cvar_VariableString( "fraglimit" ), 3 ) );
	menu.items.push_back( Field( "max players", Cvar_VariableString( "sv_maxclients" ), 3 ) );
	menu.items.push_back( Field( "hostname", Cvar_VariableString( "sv_hostname" ), 12 ) );
	Push( std::move( menu ) );
}

// Search for LAN Game (0x423bc0): ENTER connects, SPACE scans again.
void OpenLan() {
	m.stack.clear();
	Menu menu;
	menu.title = "LAN GAMES";
	menu.lanList = true;
	q3cl::LocalServersScan();
	Push( std::move( menu ) );
}

// Connect to Internet Game (0x423f50).
void OpenInternet() {
	Menu menu;
	menu.title = "INTERNET GAME";
	menu.items.push_back( Field( "server", "", 29 ) );
	const int rate = Cvar_VariableIntegerValue( "rate" );
	menu.items.push_back( Spin( "connection", { "<= 28.8K", "33.6K", "56K", "ISDN", "LAN/cable/xDSL" },
		rate <= 2500 ? 0 : rate <= 3000 ? 1 : rate <= 4000 ? 2 : rate <= 5000 ? 3 : 4,
		[]( Item &item ) {
			static const char *const rates[] = { "2500", "3000", "4000", "5000", "25000" };
			Cvar_Set( "rate", rates[item.current] );
		} ) );
	menu.items[0].activate = [] {
		const std::string server = m.stack.back().items[0].text;
		ForceMenuOff();
		if ( !server.empty() ) Cbuf_AddText( va( "connect %s\n", server.c_str() ) );
	};
	Push( std::move( menu ) );
}

void OpenMain() {
	Menu menu;
	menu.title = "";
	menu.items.push_back( Action( "Search for LAN Game", OpenLan ) );
	menu.items.push_back( Action( "Connect to Internet Game", OpenInternet ) );
	menu.items.push_back( Action( "Start Game Server", OpenStartServer ) );
	menu.items.push_back( Action( "Customize Player", OpenPlayer ) );
	menu.items.push_back( Action( "Customize Controls", OpenControls ) );
	menu.items.push_back( Action( "System Configuration", OpenSystem ) );
	menu.items.push_back( Action( "Quit", OpenQuit ) );
	menu.draw = [] {
		q3cl::DrawPic( 64, 0, 512, 128, re.RegisterShaderNoMip( "gfx/2d/menuinfo" ) );
		q3cl::DrawPic( 64, 350, 512, 128, re.RegisterShaderNoMip( "gfx/2d/menuinfo2" ) );
	};
	Push( std::move( menu ) );
}

/* M_Menu_Main_f (0x421330): the local server is stopped and the stack begins
 * again with the main menu. */
void MenuMain() {
	Cvar_Set( "sv_killserver", "1" );
	m.ingame = false;
	m.stack.clear();
	Key_SetCatcher( ( Catcher() & ~7 ) | KEYCATCH_UI );
	OpenMain();
}

// M_Menu_InGame (0x422610).
void MenuInGame() {
	m.ingame = true;
	m.stack.clear();
	Key_SetCatcher( ( Catcher() & ~7 ) | KEYCATCH_UI );
	Menu menu;
	menu.title = "";
	menu.fullscreen = false;
	menu.items.push_back( Action( "customize player", OpenPlayer ) );
	menu.items.push_back( Action( "customize controls", OpenControls ) );
	menu.items.push_back( Action( "system configuration", OpenSystem ) );
	menu.items.push_back( Action( "screenshot", [] {
		ForceMenuOff();
		Execute( "wait; wait; wait; wait; screenshot\n" );
	} ) );
	menu.items.push_back( Action( "leave arena", [] {
		ForceMenuOff();
		Execute( "disconnect\n" );
	} ) );
	menu.items.push_back( Action( "quit game", OpenQuit ) );
	Push( std::move( menu ) );
}

/* ---- keys ----------------------------------------------------------------- */

void MoveCursor( Menu &menu, int step ) {
	if ( menu.items.empty() ) return;
	const int count = static_cast<int>( menu.items.size() );
	for ( int tries = 0; tries < count; ++tries ) {
		menu.cursor = ( menu.cursor + step + count ) % count;
		if ( Selectable( menu.items[static_cast<std::size_t>( menu.cursor )] ) ) break;
	}
	PlaySound( "sound/misc/menu2.wav" );
}

void Slide( Item &item, int step ) {
	switch ( item.type ) {
	case ItemType::Spin:
		item.current = std::max( 0, std::min( item.current + step, static_cast<int>( item.options.size() ) - 1 ) );
		break;
	case ItemType::YesNo:
		item.current = !item.current;
		break;
	case ItemType::Slider:
		item.value = std::max( item.minimum, std::min( item.value + step, item.maximum ) );
		break;
	default:
		return;
	}
	PlaySound( "sound/misc/menu2.wav" );
	if ( item.changed ) item.changed( item );
}

void LanKey( int key ) {
	const int count = q3cl::LocalServerCount();
	Menu &menu = m.stack.back();
	switch ( key ) {
	case K_UPARROW: case K_KP_UPARROW:
		if ( count > 0 ) menu.cursor = ( menu.cursor - 1 + count ) % count;
		break;
	case K_DOWNARROW: case K_KP_DOWNARROW: case K_TAB:
		if ( count > 0 ) menu.cursor = ( menu.cursor + 1 ) % count;
		break;
	case K_SPACE:
		q3cl::LocalServersScan();
		break;
	case K_ENTER: case K_KP_ENTER: case K_MOUSE1:
		if ( menu.cursor < count ) {
			const std::string address = q3cl::LocalServerAddress( menu.cursor );
			ForceMenuOff();
			Execute( va( "connect %s\n", address.c_str() ) );
		}
		break;
	default:
		break;
	}
}

bool FieldKey( Item &item, int key ) {
	if ( key & K_CHAR_FLAG ) {
		const int ch = key & ~K_CHAR_FLAG;
		if ( ch == 8 ) {
			if ( !item.text.empty() ) item.text.pop_back();
		} else if ( ch >= 32 && ch < 127 && static_cast<int>( item.text.size() ) < item.maxChars ) {
			item.text.push_back( static_cast<char>( ch ) );
		}
		return true;
	}
	if ( key == K_BACKSPACE ) {
		if ( !item.text.empty() ) item.text.pop_back();
		return true;
	}
	return false;
}

/* M_Keydown (0x4225b0) with the menu's key function (Default_MenuKey 0x4238a0). */
void KeyDown( int key ) {
	if ( m.stack.empty() ) {
		if ( key == K_ESCAPE ) {
			if ( cls.state == CA_ACTIVE && !clc.demoplaying ) MenuInGame();
			else MenuMain();
		}
		return;
	}
	Menu &menu = m.stack.back();
	if ( m.grabbing ) {
		if ( !( key & K_CHAR_FLAG ) ) {
			Item &item = menu.items[static_cast<std::size_t>( menu.cursor )];
			if ( key != K_ESCAPE ) Cbuf_AddText( va( "bind \"%s\" \"%s\"\n", Key_KeynumToString( key ), item.command.c_str() ) );
			m.grabbing = false;
		}
		return;
	}
	if ( menu.title == "QUIT" ) {
		const int ch = key & ~K_CHAR_FLAG;
		if ( ch == 'y' || ch == 'Y' ) Cbuf_AddText( "quit\n" );
		else if ( key == K_ESCAPE || ch == 'n' || ch == 'N' ) Pop();
		return;
	}
	if ( key == K_ESCAPE ) {
		if ( menu.escape ) menu.escape();
		Pop();
		return;
	}
	if ( menu.lanList ) {
		if ( !( key & K_CHAR_FLAG ) ) LanKey( key );
		return;
	}
	if ( menu.items.empty() ) return;
	Item &item = menu.items[static_cast<std::size_t>( menu.cursor )];
	if ( item.type == ItemType::Field && FieldKey( item, key ) ) return;
	if ( key & K_CHAR_FLAG ) return;
	switch ( key ) {
	case K_UPARROW: case K_KP_UPARROW:
		MoveCursor( menu, -1 );
		break;
	case K_DOWNARROW: case K_KP_DOWNARROW: case K_TAB:
		MoveCursor( menu, 1 );
		break;
	case K_LEFTARROW: case K_KP_LEFTARROW:
		Slide( item, -1 );
		break;
	case K_RIGHTARROW: case K_KP_RIGHTARROW:
		Slide( item, 1 );
		break;
	case K_BACKSPACE: case K_DEL:
		if ( item.type == ItemType::Binding ) {
			for ( int k = 0; k < MAX_KEYS; ++k ) {
				const char *binding = Key_GetBinding( k );
				if ( binding && !Q_stricmp( binding, item.command.c_str() ) ) Key_SetBinding( k, "" );
			}
		}
		break;
	case K_ENTER: case K_KP_ENTER: case K_MOUSE1: case K_MOUSE2: case K_MOUSE3:
		PlaySound( "sound/misc/menu2.wav" );
		if ( item.type == ItemType::Binding ) {
			m.grabbing = true;
		} else if ( item.type == ItemType::Field && !item.activate ) {
			MoveCursor( menu, 1 );
		} else if ( item.activate ) {
			std::function<void()> activate = item.activate;
			activate();
		}
		break;
	default:
		break;
	}
}

/* ---- drawing ---------------------------------------------------------------- */

void DrawItem( const Item &item, int y, bool selected ) {
	const float *labelColor = item.greyed ? kGrey : item.type == ItemType::Separator ? kWhite : selected ? kYellow : kGreen;
	if ( item.type == ItemType::Action ) {
		q3cl::DrawBigString( 320 - q3cl::BigStringWidth( item.label.c_str() ) / 2, y, item.label.c_str(), labelColor, true );
		return;
	}
	q3cl::DrawBigString( 296 - q3cl::BigStringWidth( item.label.c_str() ), y, item.label.c_str(), labelColor, true );
	std::string value;
	switch ( item.type ) {
	case ItemType::Spin:
		value = item.options.empty() ? "" : item.options[static_cast<std::size_t>( item.current )];
		break;
	case ItemType::YesNo:
		value = item.current ? "yes" : "no";
		break;
	case ItemType::Slider: {
		const float fraction = item.maximum > item.minimum ? ( item.value - item.minimum ) / ( item.maximum - item.minimum ) : 0;
		q3cl::DrawBigChar( 336, static_cast<float>( y ), 0x80 );
		for ( int i = 0; i < 10; ++i ) q3cl::DrawBigChar( 352.0f + i * 16, static_cast<float>( y ), 0x81 );
		q3cl::DrawBigChar( 512, static_cast<float>( y ), 0x82 );
		re.SetColor( kGreen );
		q3cl::DrawBigChar( 352 + fraction * 144, static_cast<float>( y ), 0x83 );
		re.SetColor( nullptr );
		return;
	}
	case ItemType::Field:
		value = item.text + ( selected && ( cls.realtime & 256 ) ? "_" : "" );
		break;
	case ItemType::Binding:
		value = m.grabbing && selected ? "press a key" : BoundKeys( item.command.c_str() );
		break;
	default:
		return;
	}
	q3cl::DrawBigString( 336, y, value.c_str(), kWhite, true );
}

void DrawMenu( const Menu &menu ) {
	if ( menu.lanList ) {
		q3cl::DrawBigString( 320 - q3cl::BigStringWidth( "Server selection screen" ) / 2, 60, "Server selection screen", kWhite, true );
		const int count = q3cl::LocalServerCount();
		for ( int i = 0; i < count; ++i ) {
			const std::string info = q3cl::LocalServerInfo( i );
			// Info_ValueForKey rotates two buffers, so each value is copied out
			// before the next lookup.
			const std::string hostname = Info_ValueForKey( info.c_str(), "hostname" );
			const std::string mapname = Info_ValueForKey( info.c_str(), "mapname" );
			const int clients = atoi( Info_ValueForKey( info.c_str(), "clients" ) );
			const int maxClients = atoi( Info_ValueForKey( info.c_str(), "sv_maxclients" ) );
			const std::string row = va( "%-19.19s %-11.11s %2i/%2i", hostname.c_str(), mapname.c_str(), clients, maxClients );
			q3cl::DrawBigString( 16, 100 + 18 * i, row.c_str(), i == menu.cursor ? kYellow : kGreen, true );
		}
		if ( !count ) q3cl::DrawBigString( 320 - q3cl::BigStringWidth( "no servers found" ) / 2, 100, "no servers found", kGrey, true );
		return;
	}
	if ( menu.draw ) menu.draw();
	if ( menu.items.empty() ) return;
	const int rows = 21;
	const int count = static_cast<int>( menu.items.size() );
	int top = 0;
	if ( count > rows ) top = std::max( 0, std::min( menu.cursor - rows / 2, count - rows ) );
	const int shown = std::min( count, rows );
	int y = 240 - shown * 18 / 2;
	if ( !menu.title.empty() ) q3cl::DrawBigString( 320 - q3cl::BigStringWidth( menu.title.c_str() ) / 2, y - 36, menu.title.c_str(), kWhite, true );
	for ( int i = top; i < top + shown; ++i, y += 18 ) {
		DrawItem( menu.items[static_cast<std::size_t>( i )], y, i == menu.cursor );
		if ( i == menu.cursor ) {
			re.SetColor( kWhite );
			q3cl::DrawBigChar( 8, static_cast<float>( y ), 12 + ( ( cls.realtime / 250 ) & 1 ) );
			re.SetColor( nullptr );
		}
	}
}

/* M_Draw (0x4224c0): full-screen menus over conback with the version tag,
 * overlay menus over a 50% shade. */
void Draw() {
	if ( !( Catcher() & KEYCATCH_UI ) || m.stack.empty() ) return;
	const Menu &menu = m.stack.back();
	if ( menu.fullscreen && !m.ingame ) {
		q3cl::DrawPic( 0, 0, 640, 480, re.RegisterShaderNoMip( "gfx/2d/conback" ) );
		const char *tag = "Q3T 1.05";
		q3cl::DrawBigString( 632 - q3cl::BigStringWidth( tag ), 456, tag, kYellow, true );
	} else {
		q3cl::FillRect( 0, 0, 640, 480, kShade );
	}
	DrawMenu( menu );
	if ( m.enterSound ) {
		PlaySound( "sound/misc/menu1.wav" );
		m.enterSound = false;
	}
}

/* The connection screen (0x41f170 and its overlay 0x41ef70). */
void DrawConnect( bool overlay ) {
	if ( !overlay ) q3cl::DrawPic( 0, 0, 640, 480, re.RegisterShaderNoMip( "gfx/2d/conback" ) );
	auto centred = []( int y, const char *text ) {
		q3cl::DrawBigString( 320 - q3cl::BigStringWidth( text ) / 2, y, text, kWhite, true );
	};
	centred( 16, va( "Connecting to %s", cls.servername ) );
	centred( 64, "Press Esc to abort" );
	const char *motd = Info_ValueForKey( cls.updateInfoString, "motd" );
	if ( motd[0] ) centred( 100, motd );
	if ( cls.state < CA_CONNECTED && !m.serverMessage.empty() ) centred( 360, m.serverMessage.c_str() );
	const char *line = "";
	switch ( cls.state ) {
	case CA_CONNECTING: line = va( "Awaiting challenge...%s", m.connectCount.c_str() ); break;
	case CA_CHALLENGING: line = va( "Awaiting connection...%s", m.connectCount.c_str() ); break;
	case CA_CONNECTED: line = "Awaiting gamestate..."; break;
	case CA_LOADING: line = "Loading..."; break;
	case CA_PRIMED: line = "Awaiting snapshot..."; break;
	default: break;
	}
	centred( 432, line );
}

} // namespace

void CL_Q3TestMenuReset( void ) {
	m = Menus{};
}

void CL_Q3TestMenuConnectText( const char *text, bool message ) {
	std::string value = text ? text : "";
	if ( message ) {
		const std::size_t newline = value.find( '\n' );
		if ( newline != std::string::npos ) value.erase( newline );
		m.serverMessage = value.substr( 0, 1023 );
	} else {
		m.connectCount = value.substr( 0, 1023 );
	}
	SCR_UpdateScreen();
}

/* The engine's calls, in its own numbering, as 1.06's module took them. */
intptr_t CL_Q3TestMenuEntry( int command, intptr_t a0, intptr_t a1 ) {
	switch ( command ) {
	case UI_INIT:
		m = Menus{};
		return 0;
	case UI_SHUTDOWN:
		m = Menus{};
		return 0;
	case UI_KEY_EVENT:
		if ( a1 ) KeyDown( static_cast<int>( a0 ) );
		return 0;
	case UI_REFRESH:
		Draw();
		return 0;
	// M_IsFullscreen (0x4224a0)
	case UI_IS_FULLSCREEN:
		return ( Catcher() & KEYCATCH_UI ) && !m.ingame && !m.stack.empty() ? 1 : 0;
	case UI_SET_ACTIVE_MENU:
		if ( a0 == UIMENU_NONE ) ForceMenuOff();
		else if ( a0 == UIMENU_MAIN ) MenuMain();
		else if ( a0 == UIMENU_INGAME ) MenuInGame();
		return 0;
	case UI_DRAW_CONNECT_SCREEN:
		DrawConnect( a0 != 0 );
		return 0;
	default:
		return 0;
	}
}
