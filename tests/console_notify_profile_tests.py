"""Each profile but Quake Live draws its own engine's notify lines and chat line.

The console is FnQL's. Its Con_DrawNotify drew Quake Live 1.40.6's live chat
overlay for every profile: a translucent strip 413 lines down, a "say:" prompt
on Quake Live's 12x24 cell, a 73-character field, and no notify lines, because
Quake Live registers no con_notifytime. Quake III, RTCW, Enemy Territory and
Elite Force draw the last four console lines at the top of the screen for
con_notifytime seconds, then a chat prompt and field under them. Each engine
does that its own way; docs/fntech3/2026-09-23-profile-notify-lines.md has
the retail reads. Medal of Honor keeps Quake III's chat line but starts its
lines switched off, with a con_notifytime of 0 that hides every one.

These tests compile the console's own notify, chat-mode, registration and
draw-gate code with the screen, field and renderer calls recorded. They run
every profile id at 1280x960, where a virtual pixel is two, with the console
on a 16x32 cell. The fixture still compiles against the previous source, so
its behaviour can be seen to fail there.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def block(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first) + len(end)] + '\n'


def optional(text, signature):
    # Pieces the previous source did not have. Leaving them out lets that
    # source compile, so it fails on behaviour rather than on a missing name.
    return function(text, signature) if signature in text else ''


def optional_block(text, start, end):
    return block(text, start, end) if start in text else ''


def between(text, start, end):
    first = text.index(start) + len(start)
    return text[first:text.index(end, first)]


def fixture(console=None, keys=None):
    console = console if console is not None else read('code/client/cl_console.cpp')
    keys = keys if keys is not None else read('code/client/cl_keys.cpp')
    common = read('code/qcommon/qcommon.h')
    shared = read('code/qcommon/q_math.c')
    game = read('code/game/bg_public.h')
    types = ''.join((
        re.search(r'#define\s+MAX_EDIT_LINE\s+\d+', common)[0] + '\n',
        re.search(r'typedef struct \{\s*int\s+cursor;[^}]*\} field_t;', common)[0] + '\n',
        re.search(r'typedef void \(\*xcommand_t\) \(void\);', common)[0] + '\n',
        re.search(r'typedef enum \{\s*PM_NORMAL,[^}]*\} pmtype_t;', game)[0] + '\n',
        re.search(r'const vec4_t g_color_table\[ 64 \] = \{.*?\n\};', shared, re.S)[0] + '\n',
    ))
    constants = '\n'.join(line for line in console.splitlines() if line.startswith('constexpr '))
    colors = '\n'.join(re.findall(r'static const vec4_t con_chat\w+ = \{[^}]*\};', console))
    init = console[console.index('void Con_Init( void )'):]
    init = init[:init.index('\n}\n') + 3]
    shutdown = console[console.index('void Con_Shutdown( void )'):]
    shutdown = shutdown[:shutdown.index('\n}\n') + 3]
    registration = r'''
static void RegisterNotify( void ) {
''' + between(init, 'const char *speedValue;\n', '\tspeedValue = Cvar_VariableString( "con_speed" );') + r'''
}
static void RegisterChat( void ) {
''' + between(init, '\tCmd_AddCommand( "toggleconsole", Con_ToggleConsole_f );\n', '\n}\n') + r'''
}
static void UnregisterChat( void ) {
''' + between(shutdown, '\tCmd_RemoveCommand( "toggleconsole" );\n', '\n}\n') + r'''
}
'''
    code = ''.join((
        function(console, 'static int RoundToInt( float value )'),
        function(read('code/client/client.h'), 'static inline float CL_CharsetCellWidth( void ) {'),
        function(console, 'static void Con_SetScaledColor( const vec4_t color, float alphaScale ) {'),
        function(console, 'static float Con_GetTtfScale( void ) {'),
        function(console, 'static void Con_UpdateTtfFontAvailability( void ) {'),
        function(console, 'static bool Con_DrawHostText( float x, float y, const char *text,'),
        function(console, 'static void Con_DrawSmallCharFloat( float x, float y, int ch ) {'),
        optional_block(console, 'enum class ChatMode {', '\n};'),
        optional_block(console, 'struct NotifyContract {', '\n};'),
        optional_block(console, 'struct ChatCommand {', '\n};'),
        optional_block(console, 'struct ChatCommandSet {', '\n};'),
        optional(console, 'static comGameId_t Con_ProfileId( void ) {'),
        optional(console, 'static NotifyContract Con_NotifyContract( comGameId_t profile ) {'),
        function(console, 'static void Con_ToggleMessageCatcher( void ) {'),
        function(console, 'static int Con_GetChatFieldY( void ) {'),
        function(console, 'static int Con_GetChatFieldPixelWidth( void ) {'),
        function(console, 'static int Con_GetChatFieldWidthInChars( qboolean teamChat ) {'),
        optional(console, 'static int Con_ChatFieldWidth( ChatMode mode ) {'),
        function(console, 'static void Con_ResetChatField('),
        function(console, 'static void Con_MessageMode_f( void ) {'),
        function(console, 'static void Con_MessageMode2_f( void ) {'),
        function(console, 'static void Con_MessageMode3_f( void ) {'),
        function(console, 'static void Con_MessageMode4_f( void ) {'),
        optional(console, 'static void Con_WolfETMessageMode_f( void ) {'),
        optional(console, 'static void Con_WolfETMessageMode2_f( void ) {'),
        optional(console, 'static void Con_WolfETMessageMode3_f( void ) {'),
        optional(console, 'static void Con_STVEFSPMessageMode_f( void ) {'),
        optional_block(console, 'static const ChatCommand con_chatCommands[] = {', '\n};'),
        optional_block(console, 'static const ChatCommand con_wolfetChatCommands[] = {', '\n};'),
        optional_block(console, 'static const ChatCommand con_stvefSPChatCommands[] = {', '\n};'),
        optional(console, 'static ChatCommandSet Con_ChatCommands( comGameId_t profile ) {'),
        optional(console, 'void Con_OpenChat( qboolean team ) {'),
        optional(console, 'void Con_CloseChat( void ) {'),
        registration,
        optional(console, 'static int Con_NotifyLine( void )'),
        block(console, 'class Con_ScopedChatCell {', '\n};'),
        optional_block(console, 'class Con_ScopedSmallCharCell {', '\n};'),
        optional(console, 'static void Con_DrawClassicNotify( void )'),
        function(console, 'static void Con_DrawNotify( void )'),
        optional(console, 'static bool Con_NotifyDrawn( void ) {'),
        function(console, 'void Con_DrawConsole( void ) {'),
        function(keys, 'static void Message_Key( int key ) {'),
        function(shared, 'int ColorIndexFromChar( char ccode )'),
        function(console, 'static void Con_NewLine( void )'),
        function(console, 'static void Con_Linefeed( bool skipnotify )'),
        function(console, 'void CL_ConsolePrint( const char *txt ) {'),
    ))
    return (HARNESS.replace('@TYPES@', types).replace('@CONSTANTS@', constants)
            .replace('@COLORS@', colors) + code + CHECKS)


HARNESS = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/client/keycodes.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
@TYPES@
namespace {
@CONSTANTS@
}
@COLORS@
#define Com_sprintf std::snprintf

static comGameProfile_t selectedProfile;
static bool profileSelected;
const comGameProfile_t *Com_GameProfile( void ) { return profileSelected ? &selectedProfile : nullptr; }
static bool Is( comGameId_t id ) { return profileSelected && selectedProfile.id == id; }
int Com_GameIsQuakeLive( void ) { return Is( COM_GAME_QUAKELIVE ); }
int Com_GameIsWolfET( void ) { return Is( COM_GAME_WOLFET ); }
int Com_GameIsRTCW( void ) { return Is( COM_GAME_RTCW_SP ) || Is( COM_GAME_RTCW_MP ); }
int Com_GameIsSTVEF( void ) { return Is( COM_GAME_STVEF_SP ) || Is( COM_GAME_STVEF_MP ); }
int Com_GameIsMOHAA( void ) { return Is( COM_GAME_MOHAA ); }

// The client, its connection and its console.
static struct {
	int realtime;
	int state;
	float con_factor;
	qhandle_t charSetShader;
	struct { int vidWidth, vidHeight; } glconfig;
} cls = { 0, CA_ACTIVE, 1.0f, 7, { 1280, 960 } };
static struct {
	struct { struct { int pm_type; } ps; } snap;
	qboolean rtcwLimboMode;
} cl;
static struct {
	bool initialized;
	bool newline;
	int current;
	int x;
	int display;
	int linewidth;
	int totallines;
	int viswidth;
	float xadjust;
	float displayFrac;
	vec4_t color;
	std::array<int, NUM_CON_TIMES> times;
	std::array<short, CON_TEXTSIZE> text;
} con;
static cvar_t *con_scale;
static cvar_t *cl_noprint;
int Q_strncmp( const char *a, const char *b, int count ) { return std::strncmp( a, b, count ); }
static void Con_Fixup( void ) {}
static void Con_WriteTimestampPrefix( bool, int ) {}
static int console_char_width = 16, console_char_height = 32;
static int smallchar_width = 8, smallchar_height = 16;
static float con_ttfCellWidth = 16.0f;
static bool con_ttfFontAvailable;
static vec4_t con_drawColor = { 1.0f, 1.0f, 1.0f, 1.0f };

// Cvars and commands.
struct Registered { std::string name, value; int flags; };
static std::vector<Registered> registered;
static std::map<std::string, int> integers;
static cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	registered.push_back( { name, value, flags } );
	cvar_t *cvar = new cvar_t{};
	cvar->value = static_cast<float>( std::atof( value ) );
	cvar->integer = std::atoi( value );
	cvar->flags = flags;
	return cvar;
}
static void Cvar_SetDescription( cvar_t *, const char * ) {}
static int Cvar_VariableIntegerValue( const char *name ) {
	const auto found = integers.find( name );
	return found == integers.end() ? 0 : found->second;
}
static cvar_t *con_notifytime;
static cvar_t *con_drawnotify;
static cvar_t conXOffset;
static cvar_t *cl_conXOffset = &conXOffset;
static std::map<std::string, xcommand_t> commands;
static void Cmd_AddCommand( const char *name, xcommand_t command ) { commands[ name ] = command; }
static void Cmd_RemoveCommand( const char *name ) { commands.erase( name ); }

// Keys, the cgame and chat.
static int catcher;
static int Key_GetCatcher( void ) { return catcher; }
static void Key_SetCatcher( int value ) { catcher = value; }
// The entity editor, closed: notify lines draw as they always have.
static qboolean CL_EntityEditorActive( void ) { return qfalse; }
enum { CG_CROSSHAIR_PLAYER = 1000, CG_LAST_ATTACKER, CG_CHAT_UP, CG_CHAT_DOWN,
	CG_GET_CHAT_FIELD_Y, CG_GET_CHAT_FIELD_PIXEL_WIDTH, CG_GET_CHAT_FIELD_WIDTH_IN_CHARS };
struct HarnessVM { bool dllExports; };
static HarnessVM cgameVM;
static HarnessVM *cgvm = &cgameVM;
static int chatNotifications;
static int VM_Call( HarnessVM *, int, int call ) {
	switch ( call ) {
	case CG_CROSSHAIR_PLAYER: return 3;
	case CG_LAST_ATTACKER: return 4;
	case CG_GET_CHAT_FIELD_Y: return 410;
	case CG_GET_CHAT_FIELD_PIXEL_WIDTH: return 400;
	case CG_GET_CHAT_FIELD_WIDTH_IN_CHARS: return 45;
	default: ++chatNotifications; return 0;
	}
}
static field_t chatField;
static qboolean chat_team;
static qboolean chat_buddy;
static int chat_playerNum;
static int chat_dmTarget;
static void Field_Clear( field_t *edit ) { edit->buffer[0] = '\0'; edit->cursor = 0; edit->scroll = 0; }
static void Field_KeyDownEvent( field_t *, int ) {}
static std::string sent;
static void CL_AddReliableCommand( const char *text, qboolean ) { sent = text; }

// A catalogue with French prompts, for the profiles that translate them.
static bool french;
static int translations;
static void CL_TranslateClientString( const char *text, char *buffer, int size ) {
	++translations;
	const char *result = text;
	if ( french && !std::strcmp( text, "say:" ) ) result = "parler:";
	if ( french && !std::strcmp( text, "say_team:" ) ) result = "equipe:";
	if ( french && !std::strcmp( text, "say_fireteam:" ) ) result = "escouade:";
	std::snprintf( buffer, size, "%s", result );
}

// Every draw, in order, with the colour and small character cell it had.
enum class Kind { SmallChar, SmallString, BigString, Field, BigField, FillRect, Input, Host, Quad };
struct Call {
	Kind kind;
	float x, y, w, h;
	std::string text;
	int ch;
	int cellWidth, cellHeight;
	float rgba[4];
	bool colored;
	const field_t *field;
	int width;
	bool flag, flag2;
};
static std::vector<Call> calls;
static float drawColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static bool drawColored;
static int colorSets;
static Call Record( Kind kind, float x, float y ) {
	Call call = {};
	call.kind = kind;
	call.x = x;
	call.y = y;
	call.cellWidth = smallchar_width;
	call.cellHeight = smallchar_height;
	std::copy_n( drawColor, 4, call.rgba );
	call.colored = drawColored;
	return call;
}
static void HarnessSetColor( const float *rgba ) {
	++colorSets;
	drawColored = rgba != nullptr;
	for ( int i = 0; i < 4; ++i ) drawColor[i] = rgba ? rgba[i] : 1.0f;
}
static void HarnessDrawStretchPic( float x, float y, float w, float h, float, float, float, float, qhandle_t ) {
	Call call = Record( Kind::Quad, x, y );
	call.w = w;
	call.h = h;
	calls.push_back( call );
}
static void HarnessDrawScaledText( int x, int y, const char *text, int font, float scale, int, float *,
		qboolean forceColor ) {
	Call call = Record( Kind::Host, static_cast<float>( x ), static_cast<float>( y ) );
	call.text = text;
	call.ch = font;
	call.w = scale;
	call.flag = forceColor != qfalse;
	calls.push_back( call );
}
static bool hostFont;
static qboolean HarnessGetScaledFontMetrics( int, float, float *, float *, float *lineHeight ) {
	if ( lineHeight ) *lineHeight = 24.0f;
	return hostFont ? qtrue : qfalse;
}
static struct {
	void ( *SetColor )( const float *rgba );
	void ( *DrawStretchPic )( float, float, float, float, float, float, float, float, qhandle_t );
	void ( *DrawScaledText )( int, int, const char *, int, float, int, float *, qboolean );
	qboolean ( *GetScaledFontMetrics )( int, float, float *, float *, float * );
} re = { HarnessSetColor, HarnessDrawStretchPic, HarnessDrawScaledText, HarnessGetScaledFontMetrics };

static void SCR_DrawSmallChar( int x, int y, int ch ) {
	Call call = Record( Kind::SmallChar, static_cast<float>( x ), static_cast<float>( y ) );
	call.ch = ch;
	calls.push_back( call );
}
static void SCR_DrawSmallStringExt( int x, int y, const char *text, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	Call call = Record( Kind::SmallString, static_cast<float>( x ), static_cast<float>( y ) );
	call.text = text;
	std::copy_n( setColor, 4, call.rgba );
	call.flag = forceColor != qfalse;
	call.flag2 = noColorEscape != qfalse;
	calls.push_back( call );
}
static void SCR_DrawBigString( int x, int y, const char *text, float alpha, qboolean noColorEscape ) {
	Call call = Record( Kind::BigString, static_cast<float>( x ), static_cast<float>( y ) );
	call.text = text;
	call.w = alpha;
	call.flag2 = noColorEscape != qfalse;
	calls.push_back( call );
}
static void FieldCall( Kind kind, field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape ) {
	Call call = Record( kind, static_cast<float>( x ), static_cast<float>( y ) );
	call.field = edit;
	call.width = width;
	call.flag = showCursor != qfalse;
	call.flag2 = noColorEscape != qfalse;
	calls.push_back( call );
}
static void Field_Draw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape ) {
	FieldCall( Kind::Field, edit, x, y, width, showCursor, noColorEscape );
}
static void Field_BigDraw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape ) {
	FieldCall( Kind::BigField, edit, x, y, width, showCursor, noColorEscape );
}
static void SCR_FillRect( float x, float y, float w, float h, const float *color ) {
	Call call = Record( Kind::FillRect, x, y );
	call.w = w;
	call.h = h;
	std::copy_n( color, 4, call.rgba );
	calls.push_back( call );
}
static void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	if ( x ) *x *= cls.glconfig.vidWidth / 640.0f;
	if ( y ) *y *= cls.glconfig.vidHeight / 480.0f;
	if ( w ) *w *= cls.glconfig.vidWidth / 640.0f;
	if ( h ) *h *= cls.glconfig.vidHeight / 480.0f;
}
static void Con_DrawInputText( field_t *edit, float x, float y, float alphaScale, bool drawSelection,
		const vec4_t baseColor ) {
	Call call = Record( Kind::Input, x, y );
	call.field = edit;
	call.w = alphaScale;
	call.flag = drawSelection;
	std::copy_n( baseColor, 4, call.rgba );
	calls.push_back( call );
}
static void Con_CheckResize( void ) {}
static int solidConsoles;
static void Con_DrawSolidConsole( float ) { ++solidConsoles; }
'''

CHECKS = r'''
static comGameId_t current;
static int failures;
#define CHECK( condition ) do { if ( !( condition ) ) { std::fprintf( stderr, \
	"line %d, profile %u: %s\n", __LINE__, static_cast<unsigned>( current ), #condition ); \
	++failures; } } while ( 0 )

static const float kWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static const float kRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
static const float kGreen[4] = { 0.0f, 1.0f, 0.0f, 1.0f };

static void Select( comGameId_t id, bool selected = true ) {
	current = id;
	selectedProfile = comGameProfile_t{};
	selectedProfile.id = id;
	profileSelected = selected;
	registered.clear();
	commands.clear();
	con_notifytime = nullptr;
	con_drawnotify = nullptr;
	RegisterNotify();
	RegisterChat();
}

static void Reset( void ) {
	calls.clear();
	colorSets = 0;
	translations = 0;
	HarnessSetColor( nullptr );
	colorSets = 0;
}

static std::vector<Call> Only( Kind kind ) {
	std::vector<Call> found;
	for ( const Call &call : calls ) if ( call.kind == kind ) found.push_back( call );
	return found;
}

static bool Same( const float *a, const float *b ) { return std::equal( a, a + 4, b ); }

static bool Invoke( const char *name ) {
	const auto found = commands.find( name );
	if ( found == commands.end() ) return false;
	found->second();
	return true;
}

// Five console lines, four of them in the notify ring. At realtime 4200 the
// line printed at 1000 is 3.2 seconds old, the one at 2000 is 2.2, the one at
// 3500 is 0.7, and a [skipnotify] line has no time at all. The last line is
// still being printed, so the ring ends on it.
static void Lines( void ) {
	con.initialized = true;
	con.newline = false;
	con.linewidth = 6;
	con.totallines = 20;
	con.current = 4;
	con.x = 2;
	con.display = 4;
	con.xadjust = 0.0f;
	con.displayFrac = 0.0f;
	con.text.fill( static_cast<short>( ( ColorIndex( COLOR_WHITE ) << 8 ) | ' ' ) );
	const auto put = []( int line, const char *text, int color ) {
		for ( int x = 0; text[x]; ++x ) {
			con.text[ line * con.linewidth + x ] = static_cast<short>( ( color << 8 ) | text[x] );
		}
	};
	put( 0, "old", 7 );
	put( 1, "one", 7 );
	put( 2, "t o", 1 );
	put( 3, "skip", 7 );
	put( 4, "go", 2 );
	con.times = { 3500, 1000, 2000, 0 };
	cls.realtime = 4200;
	cls.state = CA_ACTIVE;
	cl.snap.ps.pm_type = PM_NORMAL;
	console_char_width = 16;
	console_char_height = 32;
	smallchar_width = 8;
	smallchar_height = 16;
	conXOffset.integer = 3;
	integers.clear();
	french = false;
	catcher = 0;
	chat_team = qfalse;
	chat_buddy = qfalse;
	chat_playerNum = -1;
	cgameVM.dllExports = false;
	hostFont = false;
}

struct Profile {
	int lines;			// notify lines drawn at realtime 4200
	int xOffset;		// cl_conXOffset's pixels, where the engine has it
	bool catchers;		// UI and cgame catchers hide the overlay
	bool small;			// Holomatch's small chat line
	bool team;			// a say_team: prompt exists
	bool buddy;			// a say_fireteam: prompt exists
	bool translated;
	const char *notifyTime;
};

static Profile Expected( comGameId_t id ) {
	switch ( id ) {
	case COM_GAME_RTCW_MP: return { 3, 3, true, false, true, false, true, "7" };
	case COM_GAME_WOLFET: return { 3, 3, true, false, true, true, true, "7" };
	case COM_GAME_STVEF_SP: return { 2, 0, false, false, false, false, false, "3" };
	case COM_GAME_STVEF_MP: return { 3, 0, false, true, true, false, false, "5" };
	// Medal of Honor's lines start switched off (CheckMOHAA).
	case COM_GAME_MOHAA: return { 0, 3, true, false, true, false, false, "0" };
	default: return { 2, 3, true, false, true, false, false, "3" };
	}
}

// The notify lines: each non-space character in the console's own 16x32 cell,
// from its left edge, one cell right of it, a cell height apart.
static void CheckLines( const Profile &expected, int drawnLines ) {
	const std::vector<Call> chars = Only( Kind::SmallChar );
	const bool threeLines = drawnLines == 3;
	const size_t count = threeLines ? 7 : 4;
	CHECK( chars.size() == ( drawnLines ? count : 0 ) );
	if ( !drawnLines || chars.size() != count ) return;
	const float x0 = static_cast<float>( expected.xOffset ) + con.xadjust;
	size_t n = 0;
	if ( threeLines ) {
		for ( int i = 0; i < 3; ++i, ++n ) {
			CHECK( chars[n].x == x0 + ( i + 1 ) * 16 && chars[n].y == 0 && chars[n].ch == "one"[i] );
			CHECK( Same( chars[n].rgba, kWhite ) );
		}
	}
	const float y = threeLines ? 32.0f : 0.0f;
	// "t o": the space is skipped but still takes its cell.
	CHECK( chars[n].x == x0 + 16 && chars[n].y == y && chars[n].ch == 't' && Same( chars[n].rgba, kRed ) );
	CHECK( chars[n + 1].x == x0 + 48 && chars[n + 1].y == y && chars[n + 1].ch == 'o' );
	CHECK( chars[n + 2].x == x0 + 16 && chars[n + 2].y == y + 32 && chars[n + 2].ch == 'g' );
	CHECK( Same( chars[n + 2].rgba, kGreen ) && chars[n + 3].ch == 'o' && chars[n + 3].x == x0 + 32 );
	for ( const Call &call : chars ) {
		CHECK( call.cellWidth == 16 && call.cellHeight == 32 && call.colored );
	}
}

static void Draw( void ) {
	Reset();
	Con_DrawNotify();
	CHECK( smallchar_width == 8 && smallchar_height == 16 );
	// The classic path leaves no colour behind. Quake Live's clears it in
	// Con_DrawInputText, which is stubbed here.
	CHECK( !drawColored || current == COM_GAME_QUAKELIVE );
}

static void CheckClassic( void ) {
	const Profile expected = Expected( current );
	const int drawn = expected.lines;

	// The lines alone, from a registered con_notifytime with the engine's default.
	CHECK( con_notifytime && con_notifytime->value == static_cast<float>( std::atof( expected.notifyTime ) ) );
	Lines();
	Draw();
	CheckLines( expected, drawn );
	CHECK( calls.size() == Only( Kind::SmallChar ).size() );
	CHECK( translations == 0 );

	// Centred 4:3 extents move the lines with the console.
	Lines();
	con.xadjust = 160.0f;
	Draw();
	CheckLines( expected, drawn );

	// A line exactly as old as con_notifytime is still drawn; one tick more is not.
	const int age = static_cast<int>( std::atof( expected.notifyTime ) * 1000 );
	Lines();
	con.times = { 4200 - age, 0, 0, 0 };
	Draw();
	CHECK( Only( Kind::SmallChar ).size() == 2 );
	cls.realtime += 1;
	Draw();
	CHECK( Only( Kind::SmallChar ).empty() );

	// The chat line under them.
	const int pixels = drawn * 32;
	const int virtualY = pixels / 2;
	for ( int mode = 0; mode < 3; ++mode ) {
		Lines();
		catcher = KEYCATCH_MESSAGE;
		chat_team = mode == 1 ? qtrue : qfalse;
		chat_buddy = mode == 2 ? qtrue : qfalse;
		Draw();
		CheckLines( expected, drawn );
		const char *prompt = "say:";
		int skip = 5;
		if ( mode == 1 && expected.team ) {
			prompt = "say_team:";
			skip = 11;
		} else if ( mode == 2 && expected.buddy ) {
			prompt = "say_fireteam:";
			skip = 15;
		}
		CHECK( translations == ( expected.translated ? 1 : 0 ) );
		if ( expected.small ) {
			// stvoyHM.exe: its small string, forced white, and Field_Draw.
			const std::vector<Call> strings = Only( Kind::SmallString );
			const std::vector<Call> fields = Only( Kind::Field );
			CHECK( strings.size() == 1 && fields.size() == 1 && Only( Kind::BigString ).empty() );
			if ( strings.size() == 1 && fields.size() == 1 ) {
				CHECK( strings[0].x == 16 && strings[0].y == pixels && strings[0].text == prompt );
				CHECK( Same( strings[0].rgba, kWhite ) && strings[0].flag && !strings[0].flag2 );
				CHECK( strings[0].cellWidth == 16 && strings[0].cellHeight == 32 );
				CHECK( fields[0].field == &chatField && fields[0].x == skip * 16 && fields[0].y == pixels );
				CHECK( fields[0].width == ( 639 - skip ) * 16 && fields[0].flag && fields[0].flag2 );
				CHECK( fields[0].cellWidth == 16 && fields[0].cellHeight == 32 );
			}
		} else {
			// Quake III's big string and big field, on the virtual screen.
			const std::vector<Call> strings = Only( Kind::BigString );
			const std::vector<Call> fields = Only( Kind::BigField );
			CHECK( strings.size() == 1 && fields.size() == 1 && Only( Kind::SmallString ).empty() );
			if ( strings.size() == 1 && fields.size() == 1 ) {
				CHECK( strings[0].x == 8 && strings[0].y == virtualY && strings[0].text == prompt );
				CHECK( strings[0].w == 1.0f && !strings[0].flag2 );
				CHECK( fields[0].field == &chatField && fields[0].x == skip * 16 && fields[0].y == virtualY );
				CHECK( fields[0].width == 640 - ( skip + 1 ) * 16 && fields[0].flag && fields[0].flag2 );
			}
		}
		CHECK( Only( Kind::FillRect ).empty() && Only( Kind::Input ).empty() && Only( Kind::Host ).empty() );
	}

	// Team outranks fireteam.
	Lines();
	catcher = KEYCATCH_MESSAGE;
	chat_team = qtrue;
	chat_buddy = qtrue;
	Draw();
	for ( const Call &call : calls ) {
		if ( call.kind == Kind::BigString || call.kind == Kind::SmallString ) {
			CHECK( call.text == ( expected.team ? "say_team:" : "say:" ) );
		}
	}

	// WolfMP.exe and et.exe count the cells from the translated prompt.
	if ( expected.translated ) {
		const char *prompts[] = { "parler:", "equipe:", "escouade:" };
		const int skips[] = { 8, 9, 11 };
		for ( int mode = 0; mode < 3; ++mode ) {
			if ( mode == 2 && !expected.buddy ) continue;
			Lines();
			french = true;
			catcher = KEYCATCH_MESSAGE;
			chat_team = mode == 1 ? qtrue : qfalse;
			chat_buddy = mode == 2 ? qtrue : qfalse;
			Draw();
			const std::vector<Call> strings = Only( Kind::BigString );
			const std::vector<Call> fields = Only( Kind::BigField );
			CHECK( strings.size() == 1 && fields.size() == 1 );
			if ( strings.size() == 1 && fields.size() == 1 ) {
				CHECK( strings[0].text == prompts[mode] && fields[0].x == skips[mode] * 16 );
				CHECK( fields[0].width == 640 - ( skips[mode] + 1 ) * 16 );
			}
		}
	}

	// Menus and the cgame's own input.
	for ( int hider : { KEYCATCH_UI, KEYCATCH_CGAME } ) {
		Lines();
		catcher = hider | KEYCATCH_MESSAGE;
		Draw();
		if ( expected.catchers ) {
			CHECK( calls.empty() );
			// At intermission the lines show through, but not the chat line.
			cl.snap.ps.pm_type = PM_INTERMISSION;
			Draw();
			CheckLines( expected, drawn );
			CHECK( calls.size() == Only( Kind::SmallChar ).size() );
		} else {
			// Elite Force tests no catcher at all.
			CheckLines( expected, drawn );
			CHECK( Only( expected.small ? Kind::Field : Kind::BigField ).size() == 1 );
		}
	}

	// RTCW MP draws nothing at all in limbo; no other engine asks.
	Lines();
	integers[ "ui_limboMode" ] = 1;
	catcher = KEYCATCH_MESSAGE;
	Draw();
	if ( current == COM_GAME_RTCW_MP ) {
		CHECK( calls.empty() && colorSets == 0 );
	} else {
		CheckLines( expected, drawn );
	}

	// A chat line with nothing above it sits at the top.
	Lines();
	con.times = { 0, 0, 0, 0 };
	catcher = KEYCATCH_MESSAGE;
	Draw();
	CHECK( Only( Kind::SmallChar ).empty() );
	for ( const Call &call : calls ) {
		if ( call.kind != Kind::Quad ) CHECK( call.y == 0 );
	}
}

// The ring, through the console's own print path. Retail moves to an empty
// line at each line feed and stamps that line at the end of the print, so it
// shows three lines and an empty row under them. [skipnotify] clears the line
// it printed, not the one before.
static void Print( int time, const char *text ) {
	cls.realtime = time;
	CL_ConsolePrint( text );
}

static void CheckRing( void ) {
	const Profile expected = Expected( current );
	const Kind prompt = expected.small ? Kind::SmallString : Kind::BigString;
	Lines();
	con.linewidth = 12;
	con.current = 0;
	con.newline = true;
	con.x = 0;
	con.display = 0;
	con.times = { 0, 0, 0, 0 };
	con.text.fill( static_cast<short>( ( ColorIndex( COLOR_WHITE ) << 8 ) | ' ' ) );
	Print( 1000, "one\n" );
	Print( 2000, "two\n" );
	Print( 3000, "[skipnotify]skip\n" );
	Print( 3500, "three\n" );
	cls.realtime = 4200;
	catcher = KEYCATCH_MESSAGE;
	Draw();
	// "two" and "three", then the empty line: "one" has left the ring,
	// whatever con_notifytime is.
	std::vector<Call> chars = Only( Kind::SmallChar );
	CHECK( chars.size() == 8 );
	if ( chars.size() == 8 ) {
		CHECK( chars[0].ch == 't' && chars[0].y == 0 && chars[2].ch == 'o' );
		CHECK( chars[3].ch == 't' && chars[3].y == 32 && chars[7].ch == 'e' );
	}
	std::vector<Call> prompts = Only( prompt );
	CHECK( prompts.size() == 1 && prompts[0].y == ( expected.small ? 96 : 48 ) );

	// A print that ends mid-line fills that row instead.
	Print( 4000, "part" );
	cls.realtime = 4200;
	Draw();
	chars = Only( Kind::SmallChar );
	CHECK( chars.size() == 12 && chars.back().ch == 't' && chars.back().y == 64 );
	prompts = Only( prompt );
	CHECK( prompts.size() == 1 && prompts[0].y == ( expected.small ? 96 : 48 ) );

	// Once every line is old, the chat line is at the top.
	cls.realtime = 20000;
	Draw();
	prompts = Only( prompt );
	CHECK( Only( Kind::SmallChar ).empty() && prompts.size() == 1 && prompts[0].y == 0 );
}

// Quake Live's overlay: no notify lines, a strip 413 lines down, its prompt on
// retail's 12x24 cell and the field re-measured from the cgame every frame.
static void CheckQuakeLive( void ) {
	CHECK( !con_notifytime && !con_drawnotify );
	for ( int team = 0; team < 2; ++team ) {
		for ( int host = 0; host < 2; ++host ) {
			Lines();
			catcher = KEYCATCH_MESSAGE | KEYCATCH_CGAME;
			chat_team = team ? qtrue : qfalse;
			hostFont = host != 0;
			Draw();
			CHECK( Only( Kind::SmallChar ).empty() && Only( Kind::BigString ).empty() );
			CHECK( Only( Kind::BigField ).empty() && Only( Kind::Field ).empty() );
			const std::vector<Call> strips = Only( Kind::FillRect );
			CHECK( strips.size() == 1 );
			if ( strips.size() == 1 ) {
				CHECK( strips[0].x == 6.0f && strips[0].y == 410.0f && strips[0].w == 628.0f && strips[0].h == 22.0f );
				volatile float expectedAlpha = 0.6f;
				CHECK( strips[0].rgba[3] == expectedAlpha );
			}
			const char *prompt = team ? "say team:" : "say:";
			if ( host ) {
				const std::vector<Call> hosts = Only( Kind::Host );
				CHECK( hosts.size() == 1 && Only( Kind::Quad ).empty() );
				if ( hosts.size() == 1 ) {
					CHECK( hosts[0].x == 16 && hosts[0].y == 850 && hosts[0].text == prompt && hosts[0].ch == 2 );
					CHECK( hosts[0].flag );
				}
			} else {
				const std::vector<Call> quads = Only( Kind::Quad );
				CHECK( quads.size() == std::strlen( prompt ) - ( team ? 1 : 0 ) );
				if ( !quads.empty() ) {
					CHECK( quads[0].x == 16.0f && quads[0].y == 826.0f && quads[0].w == 12.0f && quads[0].h == 24.0f );
				}
			}
			const std::vector<Call> inputs = Only( Kind::Input );
			CHECK( inputs.size() == 1 );
			if ( inputs.size() == 1 ) {
				CHECK( inputs[0].field == &chatField && inputs[0].x == ( team ? 132.0f : 72.0f ) );
				CHECK( inputs[0].y == 826.0f && inputs[0].w == 1.0f && !inputs[0].flag );
				volatile float expectedGreen = 0.8f;
				CHECK( inputs[0].rgba[0] == 1.0f && inputs[0].rgba[1] == expectedGreen && inputs[0].rgba[2] == 0.0f );
			}
			CHECK( chatField.widthInChars == ( team ? 68 : 73 ) );
			CHECK( console_char_width == 16 && console_char_height == 32 );
		}
	}
	// The cgame places and measures it when it can.
	Lines();
	catcher = KEYCATCH_MESSAGE;
	cgameVM.dllExports = true;
	Draw();
	const std::vector<Call> strips = Only( Kind::FillRect );
	CHECK( strips.size() == 1 && strips[0].y == 407.0f && strips[0].w == 388.0f );
	CHECK( chatField.widthInChars == 45 );
	// Only a menu hides it.
	Lines();
	catcher = KEYCATCH_MESSAGE | KEYCATCH_UI;
	Draw();
	CHECK( calls.empty() );
	// No chat, nothing drawn.
	Lines();
	Draw();
	CHECK( calls.empty() );
}

static void CheckRegistration( void ) {
	const bool quakeLive = current == COM_GAME_QUAKELIVE;
	const bool wolfet = current == COM_GAME_WOLFET;
	CHECK( registered.size() == ( quakeLive ? 0u : wolfet ? 2u : 1u ) );
	if ( !quakeLive && !registered.empty() ) {
		CHECK( registered[0].name == "con_notifytime" && registered[0].flags == 0 );
		CHECK( registered[0].value == Expected( current ).notifyTime );
	}
	if ( wolfet && registered.size() == 2 ) {
		CHECK( registered[1].name == "con_drawnotify" && registered[1].value == "0" );
		CHECK( registered[1].flags == CVAR_CHEAT && con_drawnotify );
	}

	std::vector<std::string> names;
	for ( const auto &command : commands ) names.push_back( command.first );
	if ( wolfet ) {
		CHECK( ( names == std::vector<std::string>{ "clMessageMode", "clMessageMode2", "clMessageMode3" } ) );
	} else if ( current == COM_GAME_STVEF_SP ) {
		CHECK( ( names == std::vector<std::string>{ "messagemode" } ) );
	} else if ( current == COM_GAME_MOHAA ) {
		// Medal of Honor's client game registers its own chat commands.
		CHECK( names.empty() );
	} else {
		CHECK( ( names == std::vector<std::string>{ "messagemode", "messagemode2", "messagemode3", "messagemode4" } ) );
	}
	UnregisterChat();
	CHECK( commands.empty() );
	RegisterChat();
}

// What each chat command leaves in the field and the chat state.
static void Open( const char *name, int width, int playerNum, bool team, bool buddy ) {
	catcher = 0;
	std::strcpy( chatField.buffer, "stale" );
	CHECK( Invoke( name ) );
	CHECK( catcher == KEYCATCH_MESSAGE && !chatField.buffer[0] && chatField.widthInChars == width );
	CHECK( chat_playerNum == playerNum && ( chat_team != qfalse ) == team && ( chat_buddy != qfalse ) == buddy );
}

static void CheckChatModes( void ) {
	Lines();
	chat_playerNum = 0;
	switch ( current ) {
	case COM_GAME_WOLFET:
		Open( "clMessageMode", 30, -1, false, false );
		Open( "clMessageMode2", 25, -1, true, false );
		Open( "clMessageMode3", 26, -1, false, true );
		// et.exe never clears chat_buddy, so say keeps the fireteam prompt.
		Open( "clMessageMode", 30, -1, false, true );
		Open( "clMessageMode2", 25, -1, true, true );
		CHECK( Invoke( "clMessageMode2" ) && catcher == 0 );
		break;
	case COM_GAME_STVEF_SP:
		// stvoy.exe's messagemode only readies the field.
		chat_playerNum = -1;
		std::strcpy( chatField.buffer, "stale" );
		CHECK( Invoke( "messagemode" ) && catcher == 0 && !chatField.buffer[0] && chatField.widthInChars == 30 );
		CHECK( !chat_team && !chat_buddy );
		break;
	case COM_GAME_MOHAA: {
		// Its client game opens the chat line (cl_mohaa_cgame.cpp), to all or
		// to the team, and closes it.
		catcher = 0;
		std::strcpy( chatField.buffer, "stale" );
		chat_buddy = qtrue;
		Con_OpenChat( qfalse );
		CHECK( catcher == KEYCATCH_MESSAGE && !chatField.buffer[0] && chatField.widthInChars == 30 );
		CHECK( chat_playerNum == -1 && !chat_team && !chat_buddy );
		Con_OpenChat( qtrue );
		CHECK( catcher == KEYCATCH_MESSAGE && chatField.widthInChars == 25 && chat_team );
		std::strcpy( chatField.buffer, "words" );
		Con_CloseChat();
		CHECK( catcher == 0 && !chatField.buffer[0] );
		break;
	}
	case COM_GAME_QUAKELIVE:
		Open( "messagemode", 73, -1, false, false );
		Open( "messagemode2", 68, -1, true, false );
		Open( "messagemode3", 73, 3, false, false );
		Open( "messagemode4", 73, 4, false, false );
		cgameVM.dllExports = true;
		Open( "messagemode", 45, -1, false, false );
		Open( "messagemode2", 40, -1, true, false );
		break;
	default: {
		const bool holomatch = current == COM_GAME_STVEF_MP;
		Open( "messagemode", holomatch ? 55 : 30, -1, false, false );
		CHECK( Invoke( "messagemode" ) && catcher == 0 );
		Open( "messagemode2", holomatch ? 50 : 25, -1, true, false );
		Open( "messagemode3", 30, 3, false, false );
		Open( "messagemode4", 30, 4, false, false );
		break;
	}
	}

	// Message_Key sends what the prompt says. Only ET has a fireteam, and
	// Medal of Honor's game reads only its own dmmessage, to its target.
	for ( int buddy = 0; buddy < 2; ++buddy ) {
		cls.state = CA_ACTIVE;
		catcher = KEYCATCH_MESSAGE;
		chat_playerNum = -1;
		chat_team = qfalse;
		chat_buddy = buddy ? qtrue : qfalse;
		chat_dmTarget = buddy ? -1 : 0;
		std::strcpy( chatField.buffer, "hi" );
		sent.clear();
		Message_Key( K_ENTER );
		if ( current == COM_GAME_MOHAA ) {
			CHECK( sent == ( buddy ? "dmmessage -1 hi\n" : "dmmessage 0 hi\n" ) );
		} else {
			CHECK( sent == ( buddy && current == COM_GAME_WOLFET ? "say_buddy \"hi\"\n" : "say \"hi\"\n" ) );
		}
		CHECK( !( catcher & KEYCATCH_MESSAGE ) );
	}
}

// Medal of Honor starts its lines switched off, as its engine UI shows the
// game's messages in message boxes of its own: con_notifytime 0 hides every
// line, even one printed this frame, which Quake III's 0 would still draw.
// Its chat line is Quake III's, two thirds of the way down where 2015's
// one-message chat window sat, and a raised con_notifytime draws Quake III's
// lines.
static void CheckMOHAA( void ) {
	CHECK( con_notifytime && con_notifytime->value == 0.0f );
	if ( !con_notifytime ) return;
	for ( int chat = 0; chat < 2; ++chat ) {
		Lines();
		con.times = { cls.realtime, cls.realtime, cls.realtime, cls.realtime };
		catcher = chat ? KEYCATCH_MESSAGE : 0;
		Draw();
		CHECK( Only( Kind::SmallChar ).empty() && calls.size() == ( chat ? 2u : 0u ) );
		if ( !chat ) continue;
		const std::vector<Call> strings = Only( Kind::BigString );
		const std::vector<Call> fields = Only( Kind::BigField );
		CHECK( strings.size() == 1 && fields.size() == 1 );
		if ( strings.size() == 1 && fields.size() == 1 ) {
			CHECK( strings[0].x == 8 && strings[0].y == 316 && strings[0].text == "say:" );
			CHECK( fields[0].field == &chatField && fields[0].x == 5 * 16 && fields[0].y == 316 );
		}
	}
	con_notifytime->value = 3.0f;
	Lines();
	Draw();
	CheckLines( Expected( COM_GAME_QUAKE3 ), 2 );
	con_notifytime->value = 0.0f;
}

// Whether a shut console leaves the overlay to draw, by connection state.
static void CheckDrawGate( void ) {
	// Medal of Honor draws nothing at its default (CheckMOHAA), so its gate is
	// seen with its lines raised.
	const float notifyTime = con_notifytime ? con_notifytime->value : 0.0f;
	if ( current == COM_GAME_MOHAA && con_notifytime ) con_notifytime->value = 3.0f;
	const int states[] = { CA_DISCONNECTED, CA_LOADING, CA_ACTIVE, CA_CINEMATIC };
	for ( int drawNotify = 0; drawNotify < 2; ++drawNotify ) {
		if ( con_drawnotify ) con_drawnotify->integer = drawNotify;
		for ( int state : states ) {
			Lines();
			cls.state = state;
			// A menu, or for Quake Live the cgame, holds the keys, so a
			// disconnected console is not drawn full screen. At intermission the
			// lines show through a menu wherever the engine draws them at all.
			catcher = current == COM_GAME_QUAKELIVE ? KEYCATCH_MESSAGE | KEYCATCH_CGAME
				: KEYCATCH_UI | KEYCATCH_MESSAGE;
			cl.snap.ps.pm_type = PM_INTERMISSION;
			solidConsoles = 0;
			Reset();
			Con_DrawConsole();
			bool expected = state == CA_ACTIVE;
			if ( current == COM_GAME_RTCW_SP ) expected = state != CA_LOADING;
			if ( current == COM_GAME_WOLFET ) expected = expected && drawNotify;
			// alice.exe has no overlay at all: no con_notifytime and no chat
			// prompt; its console text shows only in the console itself and
			// the mini console (UI_PrintConsole 0x0040c220). F.A.K.K.2's SDK
			// documents the same console window and mini console.
			if ( current == COM_GAME_ALICE || current == COM_GAME_FAKK2 ) expected = false;
			CHECK( solidConsoles == 0 );
			CHECK( !calls.empty() == expected );
		}
	}
	if ( con_drawnotify ) con_drawnotify->integer = 0;
	// An open console is drawn instead.
	Lines();
	con.displayFrac = 0.5f;
	solidConsoles = 0;
	Reset();
	Con_DrawConsole();
	CHECK( solidConsoles == 1 && calls.empty() );
	if ( con_notifytime ) con_notifytime->value = notifyTime;
}

int main() {
	int profiles = 0;
	for ( unsigned id = COM_GAME_QUAKE3; id < COM_GAME_COUNT; ++id ) {
		Select( static_cast<comGameId_t>( id ) );
		CheckRegistration();
		if ( id == COM_GAME_QUAKELIVE ) {
			CheckQuakeLive();
		} else if ( id <= COM_GAME_STVEF_MP ) {
			CheckClassic();
			CheckRing();
		} else if ( id == COM_GAME_MOHAA ) {
			CheckMOHAA();
		} else {
			// A later profile with no row of its own draws Quake III's lines.
			Lines();
			Draw();
			CHECK( Only( Kind::SmallChar ).size() == 4 );
		}
		CheckChatModes();
		CheckDrawGate();
		++profiles;
	}
	// Before a profile is active the console draws Quake III's.
	Select( COM_GAME_AUTO, false );
	current = COM_GAME_QUAKE3;
	CHECK( registered.size() == 1 && registered[0].value == "3" );
	Lines();
	catcher = KEYCATCH_MESSAGE;
	Draw();
	CheckLines( Expected( COM_GAME_QUAKE3 ), 2 );
	CHECK( Only( Kind::BigString ).size() == 1 && Only( Kind::BigField ).size() == 1 );
	CHECK( profiles == COM_GAME_COUNT - 1 );
	return failures ? 1 : 0;
}
'''


class ConsoleNotifyProfileTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = engine_fixture_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='console-notify-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_each_profile_draws_its_own_engines_notify_lines_and_chat_line(self):
        self.compile_and_run(fixture())

    def test_quake_live_keeps_its_overlay_ahead_of_the_classic_path(self):
        console = read('code/client/cl_console.cpp')
        notify = function(console, 'static void Con_DrawNotify( void )')
        # The classic path returns before any of Quake Live's overlay runs.
        dispatch = notify.index('if ( !Com_GameIsQuakeLive() ) {')
        self.assertLess(dispatch, notify.index('Con_DrawClassicNotify();'))
        self.assertLess(notify.index('return;'), notify.index('const Con_ScopedChatCell chatCell;'))
        # Quake Live registers neither cvar.
        init = console[console.index('void Con_Init( void )'):]
        self.assertLess(init.index('if ( !Com_GameIsQuakeLive() ) {'),
                        init.index('Cvar_Get( "con_notifytime"'))


if __name__ == '__main__':
    unittest.main()
