"""Client large text is Quake Live's host text under Quake Live alone.

`SCR_DrawStringExt` draws the client's own large strings: the demo-recording
notice, the spatial-audio overlay and big edit fields. Quake Live draws them
through its retail host font. Once the renderer's host lane became Quake
Live's alone, every other profile reached the client's host-text fallback
instead. That fallback is Quake Live's: glyphs 0.6 of their height wide, drawn
above a baseline, with no shadow. Quake III, RTCW, Enemy Territory and Elite
Force draw square size x size charset cells from the top-left, over a black
shadow two virtual pixels right and down. The notice was also drawn where
Quake Live draws it, not where each original engine does.

These tests compile the client's own drawing functions with the renderer and
host text stubbed. They run them for every profile id at 1600x900, where a
virtual pixel is 2.5 wide and 1.875 high.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def block(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first) + len(end)] + '\n'


def optional(text, signature):
    # Helpers the previous source did not have. Skipping them lets that source
    # compile, so it fails on behaviour rather than on a missing name.
    return function(text, signature) if signature in text else ''


def fixture():
    screen = read('code/client/cl_scrn.cpp')
    keys = read('code/client/cl_keys.cpp')
    common = read('code/qcommon/qcommon.h')
    shared = read('code/qcommon/q_math.c')
    field = (re.search(r'#define\s+MAX_EDIT_LINE\s+\d+', common)[0] + '\n'
             + re.search(r'typedef struct \{\s*int\s+cursor;[^}]*\} field_t;', common)[0] + '\n')
    notice = (block(screen, 'struct DemoRecordingNotice {', '\n};')
              if 'struct DemoRecordingNotice {' in screen else '')
    code = ''.join((
        re.search(r'const vec4_t g_color_table\[ 64 \] = \{.*?\n\};', shared, re.S)[0] + '\n',
        function(shared, 'int ColorIndexFromChar( char ccode )'),
        function(read('code/client/client.h'), 'static inline float CL_CharsetCellWidth( void ) {'),
        function(screen, 'static int RoundToInt( float value )'),
        block(screen, 'class ScopedRenderColor {', '\n};'),
        function(screen, 'void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) {'),
        function(screen, 'void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {'),
        optional(screen, 'static void SCR_DrawChar( int x, int y, float size, int ch ) {'),
        optional(screen, 'static void SCR_DrawCharsetStringExt('),
        function(screen, 'void SCR_DrawStringExt('),
        function(screen, 'void SCR_DrawBigString('),
        notice,
        optional(screen, 'static DemoRecordingNotice SCR_DemoRecordingNotice( comGameId_t profile ) {'),
        function(screen, 'static void SCR_DrawDemoRecording( void ) {'),
        function(keys, 'static void Field_VariableSizeDraw('),
        function(keys, 'void Field_BigDraw('),
    ))
    graphics = read('code/client/cl_main.cpp')
    first = graphics.index('\tcls.charSetShader = Com_GameIsSTVEF()')
    last = graphics.index('\n\tcls.recordShader', first)
    code += 'static void RegisterCharset() {\nconst bool wolfet=Com_GameIsWolfET();\nconst bool rtcwMP=Com_GameIsRTCWMP();\n' + graphics[first:last] + '\n}\n'
    return HARNESS.replace('@FIELD@', field) + code + CHECKS


HARNESS = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/ql_font_text.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
@FIELD@
#define Com_sprintf std::snprintf
#define Com_Error( ... ) std::abort()
#define FS_FTell HarnessFTell

static comGameProfile_t profile;
static bool profileSelected;
const comGameProfile_t *Com_GameProfile( void ) { return profileSelected ? &profile : nullptr; }
int Com_GameIsQuakeLive( void ) { return profileSelected && profile.id == COM_GAME_QUAKELIVE; }
int Com_GameIsSTVEF( void ) {
	return profileSelected && ( profile.id == COM_GAME_STVEF_SP || profile.id == COM_GAME_STVEF_MP );
}

int Com_GameIsJKA( void ) { return profileSelected && ( profile.id == COM_GAME_JKA_SP || profile.id == COM_GAME_JKA_MP ); }
int Com_GameIsWolfET( void ) { return profileSelected && profile.id == COM_GAME_WOLFET; }
int Com_GameIsRTCWMP( void ) { return profileSelected && profile.id == COM_GAME_RTCW_MP; }
enum { kCharset = 7, kRecordIcon = 9 };
struct Quad { float x, y, w, h, s0, t0, s1, t1; qhandle_t shader; float rgba[4]; };
struct HostText { int x, y; std::string text; int font; float scale; int limit; bool clipped;
	qboolean forceColor; float rgba[4]; };
static std::vector<Quad> quads;
static std::vector<HostText> hostTexts;
static float drawColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static bool drawColorSet;

static void HarnessSetColor( const float *rgba ) {
	drawColorSet = rgba != nullptr;
	for ( int i = 0; i < 4; ++i ) drawColor[i] = rgba ? rgba[i] : 1.0f;
}
static void HarnessDrawStretchPic( float x, float y, float w, float h, float s0, float t0,
		float s1, float t1, qhandle_t shader ) {
	quads.push_back( { x, y, w, h, s0, t0, s1, t1, shader,
		{ drawColor[0], drawColor[1], drawColor[2], drawColor[3] } } );
}
static std::string registeredCharset;
static bool charsetNoMip;
static qhandle_t RegisterShader(const char *name){registeredCharset=name;charsetNoMip=false;return kCharset;}
static qhandle_t RegisterShaderNoMip(const char *name){registeredCharset=name;charsetNoMip=true;return kCharset;}
static struct {
	void ( *SetColor )( const float *rgba );
	void ( *DrawStretchPic )( float, float, float, float, float, float, float, float, qhandle_t );
	qhandle_t (*RegisterShader)(const char *);
	qhandle_t (*RegisterShaderNoMip)(const char *);
} re = { HarnessSetColor, HarnessDrawStretchPic, RegisterShader, RegisterShaderNoMip };

static struct {
	struct { int vidWidth, vidHeight; } glconfig;
	qhandle_t charSetShader;
	qhandle_t recordShader;
	int realtime;
} cls = { { 1600, 900 }, kCharset, kRecordIcon, 0 };
static struct {
	qboolean demorecording;
	qboolean spDemoRecording;
	fileHandle_t recordfile;
	char recordNameShort[64];
} clc;
static int messageMode;
static int CL_DemoRecordMessageMode( void ) { return messageMode; }
static int HarnessFTell( fileHandle_t ) { return 5 * 1024 + 7; }
// Enemy Territory's size cvar, which demo_recording_cvar_profile_tests.py covers.
static cvar_t *cl_demooffset;
static void Cvar_SetIntegerValue( const char *, int ) {}

// Quake Live's host lane, which only Quake Live may reach.
void RE_DrawScaledText( int x, int y, const char *text, int fontHandle, float scale, int limit,
		float *outMaxX, qboolean forceColor, const float *baseColor ) {
	HostText record = { x, y, text ? text : "", fontHandle, scale, limit, outMaxX != nullptr,
		forceColor, { 1.0f, 1.0f, 1.0f, 1.0f } };
	if ( baseColor ) std::copy_n( baseColor, 4, record.rgba );
	hostTexts.push_back( record );
}
void RE_MeasureScaledText( const char *, const char *, int, float, int, float *bounds ) {
	const float measured[5] = { 0.0f, -12.0f, 7.0f, 0.0f, 12.0f };
	std::copy_n( measured, 5, bounds );
}

static qboolean key_overstrikeMode;
static int smallchar_width = 6;
static int bigchar_width = 16;
static int smallDraws;
static void SCR_DrawSmallStringExt( int, int, const char *, const float *, qboolean, qboolean ) { ++smallDraws; }
static void SCR_DrawSmallChar( int, int, int ) { ++smallDraws; }
'''

CHECKS = r'''
static comGameId_t current;
static int failures;
#define CHECK( condition ) do { if ( !( condition ) ) { std::fprintf( stderr, \
	"line %d, profile %u: %s\n", __LINE__, static_cast<unsigned>( current ), #condition ); \
	++failures; } } while ( 0 )

static const float kOpaque[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static const float kOpaqueShadow[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static const float kWhite[4] = { 1.0f, 1.0f, 1.0f, 0.5f };
static const float kShadow[4] = { 0.0f, 0.0f, 0.0f, 0.5f };
static const float kRed[4] = { 1.0f, 0.0f, 0.0f, 0.5f };

static void Select( comGameId_t id, bool selected = true ) {
	current = id;
	profile = comGameProfile_t{};
	profile.id = id;
	profileSelected = selected;
}

static void Reset( void ) {
	quads.clear();
	hostTexts.clear();
	HarnessSetColor( nullptr );
}

// One Fn charset cell at 1600x900. Elite Force and Academy's installed
// charsgrid_med sample the left half of each cell; other profile atlases
// sample the whole 1/16 cell. Placement, colors and shadows remain Fn-owned.
static bool Glyph( const Quad &quad, int x, int y, float size, int ch, const float *rgba ) {
	const bool halfCell = current == COM_GAME_STVEF_SP || current == COM_GAME_STVEF_MP ||
		current == COM_GAME_JKA_SP || current == COM_GAME_JKA_MP;
	const float cell = halfCell ? 0.03125f : 0.0625f;
	const float s = ( ch & 15 ) * 0.0625f;
	const float t = ( ch >> 4 ) * 0.0625f;
	return quad.shader == kCharset && quad.x == x * 2.5f && quad.y == y * 1.875f
		&& quad.w == size * 2.5f && quad.h == size * 1.875f && quad.s0 == s && quad.t0 == t
		&& quad.s1 == s + cell && quad.t1 == t + 0.0625f && std::equal( rgba, rgba + 4, quad.rgba );
}

static bool Host( const HostText &host, int x, int y, const char *text, float scale, int limit,
		qboolean forceColor, const float *rgba ) {
	return host.x == x && host.y == y && host.text == text && host.font == 2 && host.scale == scale
		&& host.limit == limit && !host.clipped && host.forceColor == forceColor
		&& std::equal( rgba, rgba + 4, host.rgba );
}

static void Notice( int mode ) {
	Reset();
	messageMode = mode;
	SCR_DrawDemoRecording();
}

static void Cursor( bool overstrike ) {
	field_t field = {};
	std::strcpy( field.buffer, "ab" );
	field.cursor = 2;
	field.widthInChars = 10;
	key_overstrikeMode = overstrike ? qtrue : qfalse;
	cls.realtime = 0;
	Reset();
	Field_BigDraw( &field, 0, 100, 0, qtrue, qfalse );
}

// Quake III's large string from the top-left, over its shadow.
static void CheckCharsetString( void ) {
	Reset();
	SCR_DrawStringExt( 10, 30, 8, "A^1B C", kWhite, qfalse, qfalse );
	CHECK( hostTexts.empty() );
	CHECK( quads.size() == 6 );
	if ( quads.size() == 6 ) {
		// The escape is skipped and the space advances without a cell.
		CHECK( Glyph( quads[0], 12, 32, 8, 'A', kShadow ) );
		CHECK( Glyph( quads[1], 20, 32, 8, 'B', kShadow ) );
		CHECK( Glyph( quads[2], 36, 32, 8, 'C', kShadow ) );
		CHECK( Glyph( quads[3], 10, 30, 8, 'A', kWhite ) );
		CHECK( Glyph( quads[4], 18, 30, 8, 'B', kRed ) );
		CHECK( Glyph( quads[5], 34, 30, 8, 'C', kRed ) );
	}
	CHECK( !drawColorSet );

	// Literal escapes are drawn as cells and still color what follows.
	Reset();
	SCR_DrawStringExt( 10, 30, 8, "A^1B", kWhite, qfalse, qtrue );
	CHECK( hostTexts.empty() && quads.size() == 8 );
	if ( quads.size() == 8 ) {
		CHECK( Glyph( quads[1], 20, 32, 8, '^', kShadow ) );
		CHECK( Glyph( quads[2], 28, 32, 8, '1', kShadow ) );
		CHECK( Glyph( quads[4], 10, 30, 8, 'A', kWhite ) );
		CHECK( Glyph( quads[5], 18, 30, 8, '^', kRed ) );
		CHECK( Glyph( quads[6], 26, 30, 8, '1', kRed ) );
		CHECK( Glyph( quads[7], 34, 30, 8, 'B', kRed ) );
	}

	// A forced color ignores the escape.
	Reset();
	SCR_DrawStringExt( 10, 30, 8, "A^1B", kWhite, qtrue, qfalse );
	CHECK( hostTexts.empty() && quads.size() == 4 );
	if ( quads.size() == 4 ) {
		CHECK( Glyph( quads[2], 10, 30, 8, 'A', kWhite ) );
		CHECK( Glyph( quads[3], 18, 30, 8, 'B', kWhite ) );
	}

	// BIGCHAR_WIDTH cells, fading with the caller's alpha.
	Reset();
	SCR_DrawBigString( 100, 200, "Hi", 0.5f, qfalse );
	CHECK( hostTexts.empty() && quads.size() == 4 );
	if ( quads.size() == 4 ) {
		CHECK( Glyph( quads[0], 102, 202, 16, 'H', kShadow ) );
		CHECK( Glyph( quads[1], 118, 202, 16, 'i', kShadow ) );
		CHECK( Glyph( quads[2], 100, 200, 16, 'H', kWhite ) );
		CHECK( Glyph( quads[3], 116, 200, 16, 'i', kWhite ) );
	}

	// A cell wholly above the screen is culled; its shadow may not be.
	Reset();
	SCR_DrawStringExt( 0, -9, 8, "A", kWhite, qtrue, qfalse );
	CHECK( quads.size() == 1 && Glyph( quads[0], 2, -7, 8, 'A', kShadow ) );
}

static void CheckHostString( void ) {
	Reset();
	SCR_DrawStringExt( 10, 30, 8, "A^1B C", kWhite, qfalse, qfalse );
	CHECK( quads.empty() && hostTexts.size() == 1 );
	if ( hostTexts.size() == 1 ) {
		CHECK( Host( hostTexts[0], 25, 56, "A^1B C", 15.0f, -1, qfalse, kWhite ) );
	}

	// One scalar at a time, advancing by the measured extent.
	Reset();
	SCR_DrawStringExt( 10, 30, 8, "A^1B", kWhite, qfalse, qtrue );
	CHECK( quads.empty() && hostTexts.size() == 4 );
	if ( hostTexts.size() == 4 ) {
		CHECK( Host( hostTexts[0], 25, 56, "A", 15.0f, 1, qtrue, kWhite ) );
		CHECK( Host( hostTexts[1], 32, 56, "^", 15.0f, 1, qtrue, kWhite ) );
		CHECK( Host( hostTexts[2], 39, 56, "1", 15.0f, 1, qtrue, kWhite ) );
		CHECK( Host( hostTexts[3], 46, 56, "B", 15.0f, 1, qtrue, kWhite ) );
	}
}

// "RECORDING demos/probe: 5k" is 25 characters, two of them spaces.
static void CheckNotice( void ) {
	clc.demorecording = qtrue;
	clc.spDemoRecording = qfalse;
	std::strcpy( clc.recordNameShort, "demos/probe" );

	Notice( 1 );
	switch ( current ) {
	case COM_GAME_QUAKELIVE:
		// quakelive_steam.exe: ( 80 - 25 ) * 4, 420 on its host baseline.
		CHECK( quads.empty() && hostTexts.size() == 1 );
		if ( hostTexts.size() == 1 ) {
			CHECK( Host( hostTexts[0], 550, 787, "RECORDING demos/probe: 5k", 15.0f, -1, qtrue, kOpaque ) );
		}
		break;
	case COM_GAME_QUAKE3:
	case COM_GAME_RTCW_SP:
	case COM_GAME_STVEF_MP:
		// quake3.exe 1.32, WolfSP.exe and stvoyHM.exe: ( 80 - 25 ) * 4, 20.
		CHECK( hostTexts.empty() && quads.size() == 46 );
		if ( quads.size() == 46 ) {
			CHECK( Glyph( quads[0], 222, 22, 8, 'R', kOpaqueShadow ) );
			CHECK( Glyph( quads[23], 220, 20, 8, 'R', kOpaque ) );
			CHECK( Glyph( quads[45], 412, 20, 8, 'k', kOpaque ) );
		}
		break;
	case COM_GAME_RTCW_MP:
		// WolfMP.exe: 5, 470.
		CHECK( hostTexts.empty() && quads.size() == 46 );
		if ( quads.size() == 46 ) {
			CHECK( Glyph( quads[0], 7, 472, 8, 'R', kOpaqueShadow ) );
			CHECK( Glyph( quads[23], 5, 470, 8, 'R', kOpaque ) );
			CHECK( Glyph( quads[45], 197, 470, 8, 'k', kOpaque ) );
		}
		break;
	case COM_GAME_WOLFET:
	case COM_GAME_STVEF_SP:
		// et.exe leaves the notice to its cgame; stvoy.exe cannot record.
		CHECK( quads.empty() && hostTexts.empty() );
		break;
	default:
		// Whatever a later profile's own row says, it is not host text.
		CHECK( hostTexts.empty() );
		break;
	}

	if ( current == COM_GAME_QUAKELIVE ) {
		// The compact icon notice is Quake Live's alone.
		Notice( 2 );
		CHECK( quads.size() == 1 && hostTexts.size() == 1 );
		if ( quads.size() == 1 && hostTexts.size() == 1 ) {
			const Quad &icon = quads[0];
			CHECK( icon.shader == kRecordIcon && icon.x == 2.5f && icon.y == 881.25f
				&& icon.w == 27.5f && icon.h == 20.625f );
			CHECK( Host( hostTexts[0], 22, 894, "REC", 15.0f, -1, qtrue, kOpaque ) );
		}
	}

	Notice( 0 );
	CHECK( quads.empty() && hostTexts.empty() );
	clc.spDemoRecording = qtrue;
	Notice( 1 );
	CHECK( quads.empty() && hostTexts.empty() );
	clc.spDemoRecording = qfalse;
	clc.demorecording = qfalse;
	Notice( 1 );
	CHECK( quads.empty() && hostTexts.empty() );
}

// A big edit field's cursor: the charset's own cursor cells 10 and 11, or
// Quake Live's retail '|' and '_' shapes, which its host face has.
static void CheckCursor( void ) {
	for ( int overstrike = 0; overstrike < 2; ++overstrike ) {
		Cursor( overstrike != 0 );
		if ( current == COM_GAME_QUAKELIVE ) {
			CHECK( quads.empty() && hostTexts.size() == 2 );
			if ( hostTexts.size() == 2 ) {
				CHECK( Host( hostTexts[0], 0, 187, "ab", 30.0f, -1, qfalse, kOpaque ) );
				CHECK( Host( hostTexts[1], 80, 187, overstrike ? "_" : "|", 30.0f, -1, qfalse, kOpaque ) );
			}
		} else {
			CHECK( hostTexts.empty() && quads.size() == 6 );
			if ( quads.size() == 6 ) {
				CHECK( Glyph( quads[2], 0, 100, 16, 'a', kOpaque ) );
				CHECK( Glyph( quads[4], 34, 102, 16, overstrike ? 11 : 10, kOpaqueShadow ) );
				CHECK( Glyph( quads[5], 32, 100, 16, overstrike ? 11 : 10, kOpaque ) );
			}
		}
	}
}

int main() {
	int profiles = 0;
	int quakeLive = 0;
	for ( unsigned id = COM_GAME_QUAKE3; id < COM_GAME_COUNT; ++id ) {
		Select( static_cast<comGameId_t>( id ) );
		RegisterCharset();
		const bool halfCell = id == COM_GAME_STVEF_SP || id == COM_GAME_STVEF_MP || id == COM_GAME_JKA_SP || id == COM_GAME_JKA_MP;
		const char *expectedCharset = halfCell ? "gfx/2d/charsgrid_med" : id == COM_GAME_WOLFET ? "gfx/2d/consolechars" :
			id == COM_GAME_RTCW_MP ? "gfx/2d/hudchars" : "gfx/2d/bigchars";
		CHECK( registeredCharset == expectedCharset && charsetNoMip == halfCell && cls.charSetShader == kCharset );
		if ( id == COM_GAME_QUAKELIVE ) {
			CheckHostString();
			++quakeLive;
		} else {
			CheckCharsetString();
		}
		CheckNotice();
		CheckCursor();
		++profiles;
	}
	// Nothing to draw, or nothing to draw it in.
	Select( COM_GAME_QUAKE3 );
	Reset();
	SCR_DrawStringExt( 0, 0, 8, nullptr, kWhite, qfalse, qfalse );
	SCR_DrawStringExt( 0, 0, 8, "", kWhite, qfalse, qfalse );
	SCR_DrawStringExt( 0, 0, 8, "A", nullptr, qfalse, qfalse );
	CHECK( quads.empty() && hostTexts.empty() );
	// Before a profile is active the client draws Quake III's text and notice.
	Select( COM_GAME_AUTO, false );
	RegisterCharset();
	CHECK( registeredCharset == "gfx/2d/bigchars" && !charsetNoMip );
	CheckCharsetString();
	clc.demorecording = qtrue;
	Notice( 1 );
	CHECK( hostTexts.empty() && quads.size() == 46 && Glyph( quads[23], 220, 20, 8, 'R', kOpaque ) );
	CHECK( profiles == COM_GAME_COUNT - 1 && quakeLive == 1 && smallDraws == 0 );
	return failures ? 1 : 0;
}
'''


class LargeTextProfileTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='large-text-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_only_quake_live_draws_large_text_in_host_text(self):
        self.compile_and_run(fixture())

    def test_quake_live_host_lane_follows_the_profile_gate(self):
        screen = read('code/client/cl_scrn.cpp')
        draw = function(screen, 'void SCR_DrawStringExt(')
        gate = draw.index('if ( !Com_GameIsQuakeLive() ) {')
        # The guard comes first so a gated profile never dereferences nothing.
        self.assertLess(draw.index('if ( !string || !string[ 0 ] || !setColor ) {'), gate)
        self.assertLess(gate, draw.index('RE_DrawScaledText('))


if __name__ == '__main__':
    unittest.main()
