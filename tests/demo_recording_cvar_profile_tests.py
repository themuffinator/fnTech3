"""Enemy Territory's recording notice comes from three engine cvars.

Retail et.exe 2.60b draws no demo-recording notice of its own. ET's cgame
draws ` demo %s: %ik ` from three read-only cvars the engine owns:
`cl_demorecording` and `cl_demofilename`, set as recording starts and stops,
and `cl_demooffset`, the size written so far, set every active frame after
the cgame has drawn. The client registered none of them, so the retail cgame
created its own at their defaults and ET showed no notice at all.

These tests compile the client's own registration, `record` and `stoprecord`
commands and screen hook with the engine around them stubbed. They run them
for every profile id. Only `wolfet` may create or write the three cvars.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('cl_demorecording', 'cl_demofilename', 'cl_demooffset')
REGISTER = 'static void CL_RegisterDemoRecordingCvars( void ) {'


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def block(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first) + len(end)] + '\n'


def optional(text, signature, fallback=''):
    # Helpers the previous source did not have. Standing in for them lets that
    # source compile, so it fails on behaviour rather than on a missing name.
    return function(text, signature) if signature in text else fallback


def fixture():
    main = read('code/client/cl_main.cpp')
    screen = read('code/client/cl_scrn.cpp')
    code = ''.join((
        function(main, 'static bool CL_WireProfileKeepsDm68Fallback('),
        optional(main, REGISTER, REGISTER + '}\n'),
        optional(main, 'static void CL_SetDemoRecordingCvars( const char *name ) {'),
        optional(main, 'static void CL_AliceRecordName( char *name, int nameSize ) {'),
        function(main, 'void CL_StopRecord_f( void ) {'),
        function(main, 'static void CL_Record_f( void ) {'),
        block(screen, 'struct DemoRecordingNotice {', '\n};'),
        function(screen, 'static DemoRecordingNotice SCR_DemoRecordingNotice( comGameId_t profile ) {'),
        function(screen, 'static void SCR_DrawDemoRecording( void ) {'),
    ))
    return HARNESS + code + CHECKS


HARNESS = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/game_profile.h"
#include "code/qcommon/protocol_contract.hpp"
#include "code/client/client_alice_demo.hpp"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

typedef int qboolean;
enum { qfalse, qtrue };
typedef int fileHandle_t;
typedef int qhandle_t;
enum { FS_INVALID_HANDLE = 0, CVAR_ROM = 0x40, CA_ACTIVE = 8, OLD_PROTOCOL_VERSION = 68 };
#define MAX_QPATH 64
#define MAX_OSPATH 256
#define DEMOEXT "dm_"
#define S_COLOR_YELLOW "^3"
#define COLOR_WHITE '7'
#define ColorIndex( c ) ( ( c ) - '0' )
#define Com_sprintf std::snprintf
constexpr const char *STVEF_DEMOEXT = "efdemo";

static comGameProfile_t profile;
static bool profileSelected;
const comGameProfile_t *Com_GameProfile( void ) { return profileSelected ? &profile : nullptr; }
static int Is( comGameId_t id ) { return profileSelected && profile.id == id; }
int Com_GameIsQuakeLive( void ) { return Is( COM_GAME_QUAKELIVE ); }
int Com_GameIsWolfET( void ) { return Is( COM_GAME_WOLFET ); }
int Com_GameIsRTCWSP( void ) { return Is( COM_GAME_RTCW_SP ); }
int Com_GameIsSTVEFMP( void ) { return Is( COM_GAME_STVEF_MP ); }
// Q3Test records with its own recorder (cl_q3test.cpp), which writes no cvar
// either; the shared recorder stands in for it here.
int Com_GameIsQ3Test( void ) { return 0; }
void CL_Q3TestRecord_f( void ) {}
int Com_GameIsQuake3IHV( void ) { return 0; }
void CL_Q3TestStopRecord_f( void ) {}
void CL_Q3IHVRecord_f( void ) {}
void CL_Q3IHVStopRecord_f( void ) {}

// The engine's cvars, by name. Like the engine's, a write creates a cvar that
// does not exist yet, so a stray write in another profile shows up as a cvar.
struct cvar_t { const char *name; };
struct Cvar { cvar_t cvar; std::string value, description; int flags; };
static std::map<std::string, Cvar> cvars;
static int writes;
static Cvar &Entry( const char *name, const char *value ) {
	auto found = cvars.find( name );
	if ( found == cvars.end() ) {
		found = cvars.emplace( name, Cvar{ { nullptr }, value, "", 0 } ).first;
		found->second.cvar.name = found->first.c_str();
	}
	return found->second;
}
static cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	Cvar &entry = Entry( name, value );
	entry.flags |= flags;
	return &entry.cvar;
}
static void Cvar_SetDescription( cvar_t *cv, const char *text ) { cvars.at( cv->name ).description = text; }
static void Cvar_Set( const char *name, const char *value ) { ++writes; Entry( name, value ).value = value; }
static void Cvar_SetIntegerValue( const char *name, int value ) { Cvar_Set( name, std::to_string( value ).c_str() ); }
static cvar_t *cl_demorecording;
static cvar_t *cl_demofilename;
static cvar_t *cl_demooffset;

static std::string printed;
static void Com_Printf( const char *format, ... ) {
	char text[1024];
	va_list args;
	va_start( args, format );
	std::vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	printed += text;
}

struct qtime_t { int tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec; };
struct netadr_t { int type; };
static struct { int state; qhandle_t recordShader; } cls;
static struct {
	netadr_t serverAddress;
	fileHandle_t recordfile;
	int demoLegacyProtocol;
	qboolean demorecording, spDemoRecording, explicitRecordName, demowaiting, dm68compat, demoplaying;
	char recordName[MAX_OSPATH];
	char recordNameShort[MAX_QPATH];
} clc;

static qboolean ToQboolean( bool value ) { return value ? qtrue : qfalse; }
static netchanWireProfile_t CL_ActiveWireProfile( void ) {
	switch ( profileSelected ? profile.id : COM_GAME_QUAKE3 ) {
	case COM_GAME_QUAKELIVE: return NETCHAN_WIRE_QL_RETAIL;
	case COM_GAME_RTCW_SP: return NETCHAN_WIRE_RTCW_SP;
	case COM_GAME_RTCW_MP: return NETCHAN_WIRE_RTCW_MP;
	case COM_GAME_WOLFET: return NETCHAN_WIRE_WOLFET;
	case COM_GAME_STVEF_MP: return NETCHAN_WIRE_STVEF_HM;
	case COM_GAME_ALICE: return NETCHAN_WIRE_ALICE;
	default: return NETCHAN_WIRE_LEGACY_Q3;
	}
}
// Both names this predicate has had. Alice shares Elite Force's framing.
static qboolean CL_LegacyFraming( void ) {
	return ToQboolean( CL_ActiveWireProfile() == NETCHAN_WIRE_STVEF_HM
		|| CL_ActiveWireProfile() == NETCHAN_WIRE_ALICE );
}
static qboolean CL_STVEFFraming( void ) { return CL_LegacyFraming(); }

// The demo file.
static int openResult = 4, demoSize, gamestates;
static fileHandle_t FS_FOpenFileWrite( const char * ) { return openResult; }
static int FS_FTell( fileHandle_t handle ) { return handle == FS_INVALID_HANDLE ? -1 : demoSize; }
static int LittleLong( int value ) { return value; }
static void FileWrite( fileHandle_t, const void *, int ) {}
static void CloseFile( fileHandle_t &handle ) { handle = FS_INVALID_HANDLE; }
static void FS_HomeRemove( const char * ) {}
static bool FS_FileExists( const char * ) { return false; }
static void FS_Rename( const char *, const char * ) {}
static void CL_WriteGamestate( qboolean ) { ++gamestates; }

// The record command's surroundings.
static std::string argument;
static int recordSPDemo;
static int Cmd_Argc( void ) { return argument.empty() ? 1 : 2; }
static const char *Cmd_Argv( int ) { return argument.c_str(); }
// Set under both spellings: alice.exe asks for "g_syncronousClients".
static float Cvar_VariableValue( const char *name ) {
	return std::strcmp( name, "g_synchronousClients" ) && std::strcmp( name, "g_syncronousClients" )
		? 0.0f : 1.0f;
}
struct ScopedFileHandle {};
static int OpenPureFileRead( const char *, ScopedFileHandle & ) { return -1; }
static int Cvar_VariableIntegerValue( const char *name ) {
	if ( !std::strcmp( name, "g_synchronousClients" ) ) return 1;
	return std::strcmp( name, "ui_recordSPDemo" ) ? 0 : recordSPDemo;
}
static bool NET_IsLocalAddress( const netadr_t * ) { return true; }
static void Q_strncpyz( char *out, const char *in, int size ) { std::snprintf( out, size, "%s", in ); }
static void Q_strcat( char *out, int size, const char *in ) {
	const std::size_t used = std::strlen( out );
	std::snprintf( out + used, static_cast<std::size_t>( size ) - used, "%s", in );
}
static const char *COM_GetExtension( const char *name ) {
	const char *dot = std::strrchr( name, '.' );
	return dot ? dot + 1 : "";
}
static qboolean CL_IsLegacyDemoExt( const char * ) { return qfalse; }
static bool CL_IsSTVEFDemoExt( const char * ) { return false; }
static bool CL_ParseDemoProtocolExtension( const char *, int * ) { return false; }
static bool CL_DemoProtocolSupported( int ) { return false; }
static void Com_RealTime( qtime_t *t ) { *t = { 126, 8, 23, 10, 11, 12 }; }
static void Com_TruncateLongString( char *out, const char *in ) { std::snprintf( out, MAX_QPATH, "%s", in ); }
static void CL_WebView_PublishGameDemo( const char *, const char * ) {}

// The engine's own notice, which only records what it would draw.
static int messageMode = 1;
static int CL_DemoRecordMessageMode( void ) { return messageMode; }
static std::vector<std::string> notices;
static const float g_color_table[8][4] = {};
static void SCR_DrawStringExt( int, int, float, const char *text, const float *, qboolean, qboolean ) {
	notices.push_back( text );
}
static void SCR_DrawPic( float, float, float, float, qhandle_t ) { notices.push_back( "(icon)" ); }
'''

CHECKS = r'''
static unsigned current;
static int failures;
#define CHECK( condition ) do { if ( !( condition ) ) { std::fprintf( stderr, \
	"line %d, profile %u: %s\n", __LINE__, current, #condition ); \
	++failures; } } while ( 0 )

static const char *const kNames[3] = { "cl_demorecording", "cl_demofilename", "cl_demooffset" };

static void Select( comGameId_t id, bool selected = true ) {
	current = id;
	profile = comGameProfile_t{};
	profile.id = id;
	profileSelected = selected;
	cvars.clear();
	writes = 0;
	cl_demorecording = cl_demofilename = cl_demooffset = nullptr;
	clc = {};
	cls = {};
	cls.state = CA_ACTIVE;
	openResult = 4;
	gamestates = 0;
	messageMode = 1;
	recordSPDemo = 0;
}

static bool Published( const char *recording, const std::string &name, const char *offset ) {
	const auto r = cvars.find( kNames[0] ), n = cvars.find( kNames[1] ), o = cvars.find( kNames[2] );
	return cvars.size() == 3 && r != cvars.end() && n != cvars.end() && o != cvars.end()
		&& r->second.value == recording && n->second.value == name && o->second.value == offset;
}

static void Record( const std::string &name ) {
	argument = name;
	printed.clear();
	CL_Record_f();
}

static void Frame( int size ) {
	demoSize = size;
	notices.clear();
	SCR_DrawDemoRecording();
}

static void Stop( void ) {
	printed.clear();
	CL_StopRecord_f();
}

static void CheckProfile( void ) {
	const bool et = current == COM_GAME_WOLFET;

	// et.exe CL_Init 0x40d7c7: Cvar_Get( name, "0" or "", 0x40 ) for each.
	CL_RegisterDemoRecordingCvars();
	if ( et ) {
		CHECK( Published( "0", "", "0" ) );
		for ( const char *name : kNames ) {
			const auto found = cvars.find( name );
			CHECK( found != cvars.end() && found->second.flags == CVAR_ROM
				&& !found->second.description.empty() );
		}
		CHECK( cl_demorecording && cl_demofilename && cl_demooffset );
	} else {
		CHECK( cvars.empty() );
		CHECK( !cl_demorecording && !cl_demofilename && !cl_demooffset );
	}
	CHECK( writes == 0 );

	// CL_Record 0x40f310: "1", the name, "0", once the file is open.
	Record( "probe" );
	CHECK( clc.demorecording && gamestates > 0 );
	if ( et ) CHECK( Published( "1", "probe", "0" ) );

	// SCR_DrawDemoRecording 0x413d40: the size written so far, every active
	// frame, whatever would hide the engine's own notice. ET's engine draws
	// none.
	Frame( 5 * 1024 + 7 );
	if ( et ) CHECK( Published( "1", "probe", "5127" ) && notices.empty() );
	// alice.exe has no "RECORDING" string to draw.
	if ( current == COM_GAME_ALICE ) CHECK( notices.empty() );
	messageMode = 0;
	Frame( 6000 );
	if ( et ) CHECK( Published( "1", "probe", "6000" ) && notices.empty() );
	messageMode = 1;

	// A second record is refused and publishes nothing new.
	Record( "other" );
	CHECK( printed == "Already recording.\n" );
	if ( et ) CHECK( Published( "1", "probe", "6000" ) );

	// CL_StopRecord_f 0x410f33: "0", "", "0".
	Stop();
	CHECK( !clc.demorecording && printed == "Stopped demo recording.\n" );
	if ( et ) CHECK( Published( "0", "", "0" ) );

	// Nothing is recording: a frame and a stop publish nothing.
	const int settled = writes;
	Frame( 8000 );
	Stop();
	CHECK( printed == "Not recording a demo.\n" && writes == settled );

	// Quake III hides its own notice while the UI records a single-player
	// demo. et.exe has no such demo and keeps the size current regardless.
	recordSPDemo = 1;
	Record( "sp" );
	CHECK( clc.spDemoRecording );
	Frame( 7000 );
	if ( et ) CHECK( Published( "1", "sp", "7000" ) );
	Stop();
	recordSPDemo = 0;

	// An automatic name is published without its folder as well.
	Record( "" );
	if ( et ) CHECK( Published( "1", "demo-20260923-101112", "0" ) );
	Stop();

	// A name is cut to the 63 characters et.exe's clc.demoName holds.
	Record( std::string( 100, 'n' ) );
	if ( et ) CHECK( Published( "1", std::string( 63, 'n' ), "0" ) );
	Stop();

	// A demo that cannot be opened is never published.
	openResult = FS_INVALID_HANDLE;
	Record( "unopened" );
	CHECK( !clc.demorecording );
	if ( et ) CHECK( Published( "0", "", "0" ) );

	// No other profile created or wrote any cvar at all.
	if ( !et ) CHECK( cvars.empty() && writes == 0 );
}

int main() {
	int profiles = 0;
	for ( unsigned id = COM_GAME_QUAKE3; id < COM_GAME_COUNT; ++id ) {
		Select( static_cast<comGameId_t>( id ) );
		CheckProfile();
		++profiles;
	}
	// Before a profile is selected there is no Enemy Territory to serve.
	Select( COM_GAME_AUTO, false );
	CheckProfile();
	CHECK( profiles == COM_GAME_COUNT - 1 );
	return failures ? 1 : 0;
}
'''


class DemoRecordingCvarProfileTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = next((path for name in ('clang++', 'g++', 'c++')
                         if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest('requires a C++17 compiler')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='demo-recording-cvars-', dir=ROOT / '.tmp') as temp:
            cpp = Path(temp) / 'probe.cpp'
            exe = Path(temp) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-Wall',
                                    '-Wextra', '-Werror', '-Wno-unused-function', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_only_enemy_territory_publishes_its_recording_cvars(self):
        self.compile_and_run(fixture())

    def test_the_client_registers_them_once_and_writes_them_by_pointer(self):
        main = read('code/client/cl_main.cpp')
        self.assertEqual(1, function(main, 'void CL_Init( void ) {').count(
            'CL_RegisterDemoRecordingCvars();'))
        # A write by name would create the cvar in whichever profile ran it.
        # Only the registration may spell the names.
        register = function(main, REGISTER)
        sources = sorted(path for path in (ROOT / 'code/client').rglob('*')
                         if path.suffix in ('.c', '.cpp', '.h', '.hpp'))
        for path in sources:
            text = path.read_text(encoding='utf-8', errors='replace')
            for name in NAMES:
                with self.subTest(path=path.name, name=name):
                    expected = register.count(f'"{name}"') if path.name == 'cl_main.cpp' else 0
                    self.assertEqual(expected, text.count(f'"{name}"'))

    def test_the_offset_is_published_after_the_cgame_draws(self):
        # et.exe SCR_DrawScreenField calls CL_CGameRendering (0x4011f0), then
        # SCR_DrawDemoRecording (0x413d40): the cgame draws last frame's size.
        screen = read('code/client/cl_scrn.cpp')
        self.assertIn('\t\t\tCL_CGameRendering( stereoFrame );\n\t\t\tSCR_DrawDemoRecording();\n', screen)


if __name__ == '__main__':
    unittest.main()
