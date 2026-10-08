"""Five engine fixes and findings from the Alice probes of 2026-09-24.

docs/fntech3/2026-09-24-engine-input-capture-fixes.md records the evidence.

1. `putawayweapon` from the console: Alice's game module refuses every client
   command whose event is not registered for the console, whatever the engine
   passes; retail alice.exe passes the entity alone, as fnTech3 does. Pinned
   against the retail binaries when they are present (read only).
2. A screenshot request never vanishes without a word: a second request for a
   type whose capture is waiting, and a capture the renderer shuts down on,
   are both named (code/renderercommon/tr_screenshot_pending.h, compiled).
3. cl_autoRecordDemo asks a refusing `record` once per level or connection;
   the real CL_Record_f, CL_ClearState and CL_Frame's automatic block are
   compiled and driven frame by frame for every profile it touches.
4. No engine input path invents a key that could stop a cinematic.
5. Alice's cinematic shuts the local server down without disconnecting the
   local client, as retail's SV_Shutdown does.
"""
from pathlib import Path
import os
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
ALICE_ROOT = Path(os.environ.get('FNTECH3_ALICE_ROOT', r'E:\Games\Alice'))


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def compiler(names):
    return next((path for name in names if (path := shutil.which(name))), None)


def build_and_run(test, source, suffix, standard, extra=()):
    cc = compiler(('clang++', 'g++', 'c++') if suffix == '.cpp' else ('clang', 'gcc', 'cc'))
    if not cc:
        test.skipTest('compiler unavailable')
    (ROOT / '.tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='engine-input-capture-', dir=ROOT / '.tmp') as temp:
        path = Path(temp) / ('fixture' + suffix)
        path.write_text(source, encoding='utf-8')
        exe = Path(temp) / ('fixture.exe' if os.name == 'nt' else 'fixture')
        build = subprocess.run([cc, *compiler_target_args(cc), standard, '-I', str(ROOT), *extra,
                                str(path), '-o', str(exe)], capture_output=True, text=True)
        test.assertEqual(build.returncode, 0, build.stdout + build.stderr)
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        return run.stdout


class RetailImage:
    """A PE file read as its mapped image (read only)."""

    def __init__(self, path):
        self.data = path.read_bytes()
        pe = struct.unpack_from('<I', self.data, 0x3c)[0]
        assert self.data[pe:pe + 4] == b'PE\0\0'
        count = struct.unpack_from('<H', self.data, pe + 6)[0]
        optional = struct.unpack_from('<H', self.data, pe + 20)[0]
        self.base = struct.unpack_from('<I', self.data, pe + 24 + 28)[0]
        self.sections = []
        for i in range(count):
            entry = pe + 24 + optional + 40 * i
            size, address, raw_size, raw = struct.unpack_from('<IIII', self.data, entry + 8)
            self.sections.append((self.base + address, max(size, raw_size), raw, raw_size))

    def read(self, address, size):
        for start, length, raw, raw_size in self.sections:
            if start <= address and address + size <= start + length:
                offset = address - start
                # Past the file's bytes a section is zero-filled when mapped.
                present = self.data[raw + offset:raw + min(offset + size, raw_size)] if offset < raw_size else b''
                return present + b'\0' * (size - len(present))
        raise AssertionError('0x%08x is outside every section' % address)

    def text(self, address):
        chunk = self.read(address, 96)
        return chunk[:chunk.index(b'\0')].decode('latin-1')

    def call_target(self, address):
        assert self.read(address, 1) == b'\xe8', hex(address)
        return (address + 5 + struct.unpack('<i', self.read(address + 1, 4))[0]) & 0xffffffff


def retail(test, name):
    path = ALICE_ROOT / name
    if not path.is_file():
        test.skipTest('retail %s not present' % name)
    return RetailImage(path)


class ConsoleCommandTests(unittest.TestCase):
    """Item 1: `putawayweapon` is refused by the retail game module itself."""

    def test_engine_hands_client_command_the_entity_alone(self):
        glue = read('code/server/server_alice_game.hpp')
        self.assertIn('typedef void ( QDECL *AliceClientEntity_t )( void *entity );', glue)
        case = glue[glue.index('case GAME_CLIENT_COMMAND: {'):]
        case = case[:case.index('return 0;')]
        self.assertIn('aliceGameExports[FNTECH3_ALICE_EXPORT_ClientCommand]', case)
        self.assertIn('clientCommand( entity );', case)

    def test_retail_engine_passes_only_the_entity(self):
        exe = retail(self, 'alice.exe')
        # SV_ExecuteClientCommand: push the client's entity (+0x20430), call
        # the export at +0x24 (ClientCommand, slot 9). Nothing else is pushed.
        self.assertEqual(exe.read(0x00452f11, 16).hex(' '),
                         '8b 85 30 04 02 00 8b 0d 08 0e ac 00 50 ff 51 24')
        self.assertEqual(exe.read(0x00452f21, 9).hex(' '), '8b 0d 08 0e ac 00 83 c4 04')
        # The engine has no special case for the command at all.
        self.assertNotIn(b'putawayweapon', exe.data)

    def test_retail_game_refuses_every_client_command_without_the_console_flag(self):
        game = retail(self, 'base/fgamex86.dll')
        # G_ProcessClientCommand marks every client command's event as coming
        # from the console: source bits (2..3) = 1.
        self.assertEqual(game.read(0x100af591, 21).hex(' '),
                         '8b 4d 10 8b 54 24 60 83 e1 f7 52 83 c9 04 89 7c 24 5c 89 4d 10')
        # Listener::ProcessEvent: from the console (source == 1) and neither
        # EV_CONSOLE (0x10) nor EV_CHEAT (0x20) -> the refusal.
        self.assertEqual(game.read(0x100d86b1, 18).hex(' '),
                         '8b d0 83 e2 0c 80 fa 04 0f 85 bd 00 00 00 a8 30 75 4e')
        self.assertEqual(game.read(0x100d86f3, 5).hex(' '), '68 94 a8 1c 10')
        self.assertEqual(game.text(0x101ca894), 'print "Command not available from console.\n"')
        # putawayweapon is registered with EV_DEFAULT (-1), which the event
        # constructor turns into no flags on first registration.
        self.assertEqual(game.read(0x100f322f, 7).hex(' '), '6a ff 68 fc e5 1c 10')
        self.assertEqual(game.text(0x101ce5fc), 'putawayweapon')
        self.assertEqual(game.data.count(b'putawayweapon\0'), 1)
        self.assertEqual(game.read(0x100d697e, 15).hex(' '), '83 ff ff 89 4e 04 8b f1 89 75 0c 75 02 33 ff')


SCREENSHOT_FIXTURE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
	va_list a; va_start( a, fmt ); vfprintf( stderr, fmt, a ); va_end( a ); (void)level; exit( 3 );
}
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
float Q_atof( const char *text ) { return (float)atof( text ); }

typedef enum {
	SCREENSHOT_PNG = 1<<0, SCREENSHOT_TGA = 1<<1, SCREENSHOT_JPG = 1<<2,
	SCREENSHOT_BMP = 1<<3, SCREENSHOT_BMP_CLIPBOARD = 1<<4, SCREENSHOT_AVI = 1<<5
} screenshotType_t;

static char printed[4096];
static int levels[16], lines;
static void QDECL TestPrintf( int level, const char *fmt, ... ) {
	size_t used = strlen( printed );
	va_list a; va_start( a, fmt ); vsnprintf( printed + used, sizeof( printed ) - used, fmt, a ); va_end( a );
	levels[lines++ & 15] = level;
}
static struct { void ( QDECL *Printf )( int level, const char *fmt, ... ); } ri = { TestPrintf };

#include "code/renderercommon/tr_screenshot_pending.h"

static void Reset( void ) { printed[0] = '\0'; lines = 0; }

int main( void ) {
	/* Nothing waiting, or only the AVI frame bit: nothing to say. */
	Reset();
	assert( R_ReportUnwrittenScreenshots( 0, "", "", "", "", qfalse, qfalse, R_SCREENSHOT_LOST_TO_SHUTDOWN ) == 0 );
	assert( R_ReportUnwrittenScreenshots( SCREENSHOT_AVI, "", "", "", "", qfalse, qfalse, "x" ) == 0 );
	assert( !printed[0] );

	/* Every waiting capture is named, once, with the reason. */
	Reset();
	assert( R_ReportUnwrittenScreenshots( SCREENSHOT_PNG | SCREENSHOT_JPG | SCREENSHOT_AVI,
		"screenshots/s3.png", "", "screenshots/j.jpg", "", qtrue, qtrue, R_SCREENSHOT_LOST_TO_SHUTDOWN ) == 4 );
	assert( !strcmp( printed,
		"WARNING: screenshots/s3.png was not written: the renderer shut down before a frame was captured.\n"
		"WARNING: screenshots/j.jpg was not written: the renderer shut down before a frame was captured.\n"
		"WARNING: the levelshot was not written: the renderer shut down before a frame was captured.\n"
		"WARNING: the screenshot cubemap was not written: the renderer shut down before a frame was captured.\n" ) );
	assert( lines == 4 && levels[0] == PRINT_WARNING && levels[3] == PRINT_WARNING );

	/* A name left over without its bit is not waiting. */
	Reset();
	assert( R_ReportUnwrittenScreenshots( SCREENSHOT_TGA, "screenshots/old.png", "screenshots/t.tga", "", "",
		qfalse, qfalse, R_SCREENSHOT_LOST_TO_MINIMIZE ) == 1 );
	assert( !strcmp( printed,
		"WARNING: screenshots/t.tga was not written: the window is minimized and cannot be captured.\n" ) );

	/* A clipboard BMP has no file name. */
	Reset();
	assert( R_ReportUnwrittenScreenshots( SCREENSHOT_BMP | SCREENSHOT_BMP_CLIPBOARD, "", "", "", "",
		qfalse, qfalse, "gone" ) == 1 );
	assert( !strcmp( printed, "WARNING: the clipboard screenshot was not written: gone.\n" ) );

	/* A request that names the waiting file is covered by it, silently,
	   whatever the case of its name. */
	Reset();
	assert( R_ReportScreenshotAlreadyPending( "screenshotPNG", "s1", "png", "screenshots/s1.png" ) );
	assert( R_ReportScreenshotAlreadyPending( "screenshotPNG", "S1", "png", "screenshots/s1.png" ) );
	assert( !printed[0] );

	/* Any other request says why it is not taken. */
	assert( !R_ReportScreenshotAlreadyPending( "screenshotPNG", "s2", "png", "screenshots/s1.png" ) );
	assert( !strcmp( printed,
		"WARNING: screenshotPNG s2 not taken: screenshots/s1.png is already waiting for the next frame.\n" ) );
	Reset();
	assert( !R_ReportScreenshotAlreadyPending( "screenshot", "", "png", "screenshots/shot0001.png" ) );
	assert( !strcmp( printed,
		"WARNING: screenshot not taken: screenshots/shot0001.png is already waiting for the next frame.\n" ) );
	Reset();
	assert( !R_ReportScreenshotAlreadyPending( "screenshotJPEG", "silent", "jpg", "screenshots/shot0001.jpg" ) );
	assert( strstr( printed, "screenshotJPEG silent not taken" ) );
	Reset();
	assert( !R_ReportScreenshotAlreadyPending( "screenshotBMP", "b", "bmp", "" ) );
	assert( !strcmp( printed,
		"WARNING: screenshotBMP b not taken: a clipboard screenshot is already waiting for the next frame.\n" ) );
	/* A request for the clipboard never names a file, so it is never covered. */
	Reset();
	assert( !R_ReportScreenshotAlreadyPending( "screenshotBMP", "clipboard", "bmp", "" ) );
	assert( strstr( printed, "screenshotBMP clipboard not taken" ) );
	puts( "screenshot reports ok" );
	return 0;
}
'''


class ScreenshotRequestTests(unittest.TestCase):
    """Item 2: every request writes a file or says why it did not."""

    RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

    def test_reports_compile_and_name_each_lost_request(self):
        output = build_and_run(self, SCREENSHOT_FIXTURE, '.c', '-std=c11')
        self.assertIn('screenshot reports ok', output)

    def test_every_renderer_shares_the_flag_values_the_header_assumes(self):
        for renderer in self.RENDERERS:
            with self.subTest(renderer=renderer):
                local = read('code/%s/tr_local.h' % renderer)
                for name, value in (('SCREENSHOT_PNG', '1<<0'), ('SCREENSHOT_TGA', '1<<1'),
                                    ('SCREENSHOT_JPG', '1<<2'), ('SCREENSHOT_BMP', '1<<3'),
                                    ('SCREENSHOT_BMP_CLIPBOARD', '1<<4')):
                    self.assertRegex(local, r'\b%s = %s' % (name, re.escape(value)))

    def test_every_renderer_refuses_a_second_request_aloud(self):
        for renderer in self.RENDERERS:
            with self.subTest(renderer=renderer):
                init = read('code/%s/tr_init.c' % renderer)
                self.assertIn('#include "../renderercommon/tr_screenshot_pending.h"', init)
                command = function(init, 'static void R_ScreenShot_f( void ) {')
                pending = command.index('if ( backEnd.screenshotMask & typeMask ) {')
                branch = command[pending:command.index('return;', pending)]
                self.assertIn('R_ReportScreenshotAlreadyPending( ri.Cmd_Argv( 0 ), ri.Cmd_Argv( 1 ), ext,', branch)
                for name in ('screenshotPNG', 'screenshotJPG', 'screenshotBMP', 'screenshotTGA'):
                    self.assertIn('backEnd.' + name, branch)
                # Refused before the request touches the waiting capture's state.
                self.assertLess(pending, command.index('SCREENSHOT_BMP_CLIPBOARD'))
                self.assertLess(pending, command.index('backEnd.screenshotMask |= typeMask;'))

    def test_every_renderer_names_what_its_shutdown_drops(self):
        for renderer in self.RENDERERS:
            with self.subTest(renderer=renderer):
                init = read('code/%s/tr_init.c' % renderer)
                shutdown = function(init, 'static void RE_Shutdown( refShutdownCode_t code ) {')
                report = shutdown.index('R_ReportUnwrittenScreenshots( backEnd.screenshotMask, backEnd.screenshotPNG,')
                self.assertLess(shutdown.index('"RE_Shutdown( %i )\\n"'), report)
                self.assertLess(report, shutdown.index('R_SCREENSHOT_LOST_TO_SHUTDOWN'))
                self.assertLess(report, shutdown.index('backEnd.screenshotMask = 0;'))
                self.assertLess(report, shutdown.index('ri.Cmd_RemoveCommand( "screenshotPNG" );'))

    def test_minimized_vulkan_renderers_name_what_they_drop(self):
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                cmds = read('code/%s/tr_cmds.c' % renderer)
                self.assertIn('#include "../renderercommon/tr_screenshot_pending.h"', cmds)
                issue = cmds[cmds.index('if ( ri.CL_IsMinimized() && !RE_CanMinimize() ) {'):]
                issue = issue[:issue.index('return;')]
                self.assertLess(issue.index('R_SCREENSHOT_LOST_TO_MINIMIZE'),
                                issue.index('backEnd.screenshotMask = 0;'))

    def test_a_written_screenshot_reports_as_before(self):
        for renderer in self.RENDERERS:
            with self.subTest(renderer=renderer):
                backend = read('code/%s/tr_backend.c' % renderer)
                for name in ('PNG', 'TGA', 'JPG', 'BMP'):
                    wrote = 'ri.Printf( PRINT_ALL, "Wrote %%s\\n", backEnd.screenshot%s );' % name
                    if renderer == 'rendererrtx':
                        # its writers say whether the frame was read back, and
                        # a frame that was not is reported by the writer alone
                        call = backend[:backend.index(wrote)]
                        call = ' '.join(call[call.rindex('if ( RB_TakeScreenshot'):].split())
                        self.assertTrue(call.endswith('&& !backEnd.screenShot%ssilent ) {' % name), call)
                    else:
                        self.assertIn('if ( !backEnd.screenShot%ssilent ) {\n\t\t\t\t' % name + wrote, backend)


AUTO_RECORD_PRELUDE = r'''
#include "code/qcommon/protocol_contract.hpp"
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include "code/client/client_alice_demo.hpp"
#include "code/client/demo_auto_record.hpp"
typedef int qboolean;
enum { qfalse, qtrue, FS_INVALID_HANDLE = -1, MAX_OSPATH = 128, MAX_QPATH = 64,
       CA_DISCONNECTED = 1, CA_CONNECTED = 5, CA_LOADING = 6, CA_ACTIVE = 8, EXEC_NOW = 0 };
enum { COM_GAME_QUAKE3, COM_GAME_QUAKE_LIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP, COM_GAME_WOLFET,
       COM_GAME_STVEF_MP, COM_GAME_ALICE, COM_GAME_MOHAA };
#define S_COLOR_YELLOW "^3"
#define Com_sprintf snprintf
#define Com_Memset std::memset
struct qtime_t { int tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec; };
struct netadr_t { int local; };
static qboolean ToQboolean(bool value) { return value ? qtrue : qfalse; }
static struct { int state; char servername[256]; } cls;
static struct {
    netadr_t serverAddress;
    int recordfile;
    qboolean demorecording, demoplaying, spDemoRecording, explicitRecordName, demowaiting, dm68compat;
    char recordName[MAX_OSPATH], recordNameShort[32];
} clc;
static struct { char mapname[MAX_QPATH]; int other; } cl;
static struct { int integer; } autoRecordCvar = { 1 };
static decltype(autoRecordCvar) *cl_autoRecordDemo = &autoRecordCvar;
static int game = COM_GAME_QUAKE3, opens;
static float synchronous;
static bool openFails;
static std::string argument, printed;
static int Com_GameIsRTCWSP() { return game == COM_GAME_RTCW_SP; }
static int Com_GameIsSTVEFMP() { return game == COM_GAME_STVEF_MP; }
static int Com_GameIsAlice() { return game == COM_GAME_ALICE; }
static int Com_GameIsRitual() { return game == COM_GAME_ALICE; }
static int Com_GameIsMOHAA() { return game == COM_GAME_MOHAA; }
// Q3Test records with its own recorder (cl_q3test.cpp).
static int Com_GameIsQ3Test() { return 0; }
static void CL_Q3TestRecord_f() {}
static int Com_GameIsQuake3IHV() { return 0; }
static void CL_Q3IHVRecord_f() {}
/* cl_mohaa.h: Medal of Honor's own snapshots, entities and baselines. */
static int mohaaClears;
static void CL_MOHAAClearState( void ) { ++mohaaClears; }
static netchanWireProfile_t CL_ActiveWireProfile() {
    return game == COM_GAME_RTCW_SP ? NETCHAN_WIRE_RTCW_SP :
        game == COM_GAME_RTCW_MP ? NETCHAN_WIRE_RTCW_MP :
        game == COM_GAME_STVEF_MP ? NETCHAN_WIRE_STVEF_HM :
        game == COM_GAME_ALICE ? NETCHAN_WIRE_ALICE :
        game == COM_GAME_MOHAA ? NETCHAN_WIRE_MOHAA : NETCHAN_WIRE_LEGACY_Q3;
}
static int Cmd_Argc() { return argument.empty() ? 1 : 2; }
static const char *Cmd_Argv(int) { return argument.c_str(); }
static float Cvar_VariableValue(const char *name) {
    const char *wanted = game == COM_GAME_ALICE ? "g_syncronousClients" : "g_synchronousClients";
    return !std::strcmp(name, wanted) ? synchronous : 0.0f;
}
static int Cvar_VariableIntegerValue(const char *name) {
    return !std::strcmp(name, "g_synchronousClients") ? static_cast<int>(synchronous) : 0;
}
static bool NET_IsLocalAddress(const netadr_t *address) { return address->local != 0; }
static void Com_Printf(const char *format, ...) {
    char text[512]; va_list args; va_start(args, format);
    vsnprintf(text, sizeof(text), format, args); va_end(args); printed += text;
}
static const char *va(const char *format, ...) {
    static char text[1024]; va_list args; va_start(args, format);
    vsnprintf(text, sizeof(text), format, args); va_end(args); return text;
}
static const char *COM_GetExtension(const char *name) {
    const char *dot = std::strrchr(name, '.'); return dot ? dot + 1 : "";
}
static const char *COM_SkipPath(const char *path) {
    const char *slash = std::strrchr(path, '/'); return slash ? slash + 1 : path;
}
static void COM_StripExtension(const char *in, char *out, int size) {
    std::string text(in); const std::size_t dot = text.rfind('.');
    if (dot != std::string::npos) text.resize(dot);
    snprintf(out, size, "%s", text.c_str());
}
static qboolean CL_IsLegacyDemoExt(const char *) { return qfalse; }
static bool CL_IsSTVEFDemoExt(const char *) { return false; }
static bool CL_ParseDemoProtocolExtension(const char *, int *) { return false; }
static bool CL_DemoProtocolSupported(int) { return false; }
static void Q_strncpyz(char *out, const char *in, int size) { snprintf(out, size, "%s", in); }
static void Q_strcat(char *out, int size, const char *in) {
    const std::size_t used = std::strlen(out);
    snprintf(out + used, static_cast<std::size_t>(size) - used, "%s", in);
}
static void Com_RealTime(qtime_t *t) { *t = {126, 8, 24, 9, 0, 0}; }
static int FS_FOpenFileWrite(const char *) { ++opens; return openFails ? FS_INVALID_HANDLE : 4; }
static void Com_TruncateLongString(char *out, const char *in) { snprintf(out, 32, "%s", in); }
static void CL_WebView_PublishGameDemo(const char *, const char *) {}
static void CL_SetDemoRecordingCvars(const char *) {}
static void CL_WriteGamestate(qboolean) {}
static void S_AliceMusicFreeAll() {}
static void S_AliceFadeSound(float) {}
struct ScopedFileHandle {};
static int OpenPureFileRead(const char *, ScopedFileHandle &) { return -1; }
static void CL_StopRecord_f() { clc.demorecording = qfalse; }
static void CL_Record_f();
/* `record <name>` as the command buffer would run it. */
static int recordCommands;
static void Cbuf_ExecuteText(int, const char *text) {
    assert(!std::strncmp(text, "record ", 7));
    ++recordCommands;
    argument = text + 7;
    CL_Record_f();
}
'''


class AutoRecordTests(unittest.TestCase):
    """Item 3: the automatic `record` refuses once per level or connection."""

    @classmethod
    def setUpClass(cls):
        cls.main = read('code/client/cl_main.cpp')

    def fixture(self):
        main = self.main
        block_start = main.index('\tif ( cl_autoRecordDemo->integer && !clc.demoplaying ) {')
        block = main[block_start:main.index('\t// decide the simulation time', block_start)]
        latch = 'static bool cl_autoRecordRefused = false;'
        self.assertIn(latch, main)
        source = AUTO_RECORD_PRELUDE
        source += function(main, 'static bool CL_WireProfileKeepsDm68Fallback(')
        source += function(main, 'static void CL_AliceRecordName( char *name, int nameSize ) {')
        source += function(main, 'static void CL_Record_f( void ) {')
        source += function(main, 'static bool CL_RecordRefusedForSynchronousClients( void ) {')
        source += latch + '\n'
        source += function(main, 'void CL_ClearState( void ) {')
        source += 'static void AutoRecordFrame() {\n' + block + '}\n'
        source += AUTO_RECORD_MAIN
        return source

    def test_automatic_record_refuses_once_per_level_and_connection(self):
        output = build_and_run(self, self.fixture(), '.cpp', '-std=c++17')
        self.assertIn('auto record ok', output)

    def test_manual_record_and_its_refusals_are_unchanged(self):
        record = function(self.main, 'static void CL_Record_f( void ) {')
        self.assertIn('Com_Printf( "The server must have \'g_synchronousClients 1\' set for demos\\n" );', record)
        self.assertIn('Com_Printf( "%s", fntech3::alice::demo::kSynchronousClientsRefusal );', record)
        self.assertNotIn('cl_autoRecordRefused', record)
        self.assertNotIn('AutoRecordAttempt', record)

    def test_latch_is_cleared_with_every_gamestate_and_disconnect(self):
        clear = function(self.main, 'void CL_ClearState( void ) {')
        self.assertIn('cl_autoRecordRefused = false;', clear)
        parse = read('code/client/cl_parse.cpp')
        gamestate = function(parse, 'void CL_ParseGamestate( msg_t *msg ) {')
        self.assertIn('CL_ClearState();', gamestate)
        disconnect = function(self.main, 'qboolean CL_Disconnect( qboolean showMainMenu ) {')
        self.assertIn('CL_ClearState();', disconnect)


AUTO_RECORD_MAIN = r'''
static const char *const kRefusal = "The server must have 'g_synchronousClients 1' set for demos\n";
static const char *const kAliceRefusal = "The server must have 'g_syncronousClients 1' set for demos\n";

static int Count(const std::string &text, const char *needle) {
    int count = 0;
    for (std::size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++count;
    return count;
}

/* A new level: play is active on a fresh gamestate. */
static void NewLevel(int profile, float sync) {
    game = profile; synchronous = sync;
    clc = {}; CL_ClearState();
    cls.state = CA_ACTIVE; std::snprintf(cls.servername, sizeof(cls.servername), "localhost");
    std::snprintf(cl.mapname, sizeof(cl.mapname), "maps/garden1.bsp");
    printed.clear(); recordCommands = 0; opens = 0; openFails = false;
}

static void Frames(int count) { for (int i = 0; i < count; ++i) AutoRecordFrame(); }

int main() {
    /* The predicate is CL_Record_f's own refusal, for every profile and value. */
    for (int profile : { COM_GAME_QUAKE3, COM_GAME_QUAKE_LIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP,
                         COM_GAME_WOLFET, COM_GAME_STVEF_MP, COM_GAME_ALICE, COM_GAME_MOHAA }) {
        /* Clearing the state clears Medal of Honor's own records, and only its. */
        mohaaClears = 0; game = profile; CL_ClearState();
        assert(mohaaClears == (profile == COM_GAME_MOHAA ? 1 : 0));
        for (float sync : { 0.0f, 0.5f, 1.0f }) {
            NewLevel(profile, sync);
            const bool predicted = CL_RecordRefusedForSynchronousClients();
            argument = "probe"; CL_Record_f();
            const bool refused = printed.find(profile == COM_GAME_ALICE ? kAliceRefusal : kRefusal) != std::string::npos;
            assert(predicted == refused);
            assert(refused == !clc.demorecording);
            const bool refuses = profile == COM_GAME_RTCW_SP || profile == COM_GAME_STVEF_MP || profile == COM_GAME_ALICE;
            assert(refused == (refuses && sync == 0.0f));
        }
    }

    /* The three refusing profiles are asked once in a level, not every frame. */
    for (int profile : { COM_GAME_RTCW_SP, COM_GAME_STVEF_MP, COM_GAME_ALICE }) {
        const char *refusal = profile == COM_GAME_ALICE ? kAliceRefusal : kRefusal;
        NewLevel(profile, 0.0f);
        Frames(120);
        assert(recordCommands == 1 && Count(printed, refusal) == 1 && !clc.demorecording);

        /* A new level (its gamestate clears the client state) asks again, once. */
        NewLevel(profile, 0.0f);
        Frames(120);
        assert(recordCommands == 1 && Count(printed, refusal) == 1);

        /* So does a new connection: disconnect clears it too. */
        CL_ClearState(); Frames(60);
        assert(recordCommands == 2 && Count(printed, refusal) == 2);

        /* Setting the cvar mid-level starts the recording on the next frame. */
        synchronous = 1.0f; Frames(1);
        assert(recordCommands == 3 && clc.demorecording && Count(printed, refusal) == 2);
        Frames(30);
        assert(recordCommands == 3);

        /* Leaving play stops it, as before. */
        cls.state = CA_LOADING; Frames(1);
        assert(!clc.demorecording);

        /* The manual command still refuses every time it is typed. */
        NewLevel(profile, 0.0f);
        for (int i = 0; i < 3; ++i) { argument = "probe"; CL_Record_f(); }
        assert(Count(printed, refusal) == 3);
        /* A manual refusal does not use up the automatic attempt. */
        Frames(10);
        assert(recordCommands == 1 && Count(printed, refusal) == 4);
    }

    /* Every other profile keeps trying every frame: an attempt that fails for
       any other reason is repeated, as it always was. */
    for (int profile : { COM_GAME_QUAKE3, COM_GAME_QUAKE_LIVE, COM_GAME_RTCW_MP, COM_GAME_WOLFET,
                         COM_GAME_MOHAA }) {
        NewLevel(profile, 0.0f);
        openFails = true;
        Frames(50);
        assert(recordCommands == 50 && opens == 50 && Count(printed, "ERROR: couldn't open.\n") == 50);
        assert(Count(printed, kRefusal) == 0);
        openFails = false;
        Frames(1);
        assert(recordCommands == 51 && clc.demorecording);
    }
    /* So do the refusing profiles once their cvar is set. */
    NewLevel(COM_GAME_ALICE, 1.0f);
    openFails = true;
    Frames(20);
    assert(recordCommands == 20 && Count(printed, "ERROR: couldn't open.\n") == 20);

    /* No automatic attempt while a demo plays or the cvar is off. */
    NewLevel(COM_GAME_ALICE, 0.0f);
    clc.demoplaying = qtrue; Frames(10);
    assert(recordCommands == 0);
    clc.demoplaying = qfalse; autoRecordCvar.integer = 0; Frames(10);
    assert(recordCommands == 0);
    autoRecordCvar.integer = 1;
    std::puts("auto record ok");
    return 0;
}
'''


class SyntheticInputTests(unittest.TestCase):
    """Item 4: no engine path invents a key that could stop a cinematic.

    A cinematic stops on Escape, or on any key below 128 or MOUSE1 that no
    catcher takes (CL_KeyDownEvent). The platform layers queue key-downs only
    from their devices' own messages; the one press they reconstruct without
    a message is a modifier still held when focus returns, and every modifier
    key number is 128 or above.
    """

    def test_held_modifier_rebuilds_queue_only_modifier_keys(self):
        sources = {
            'code/sdl/sdl_input.cpp': 'static void IN_QueueHeldModifiers( int eventTime )',
            'code/win32/win_wndproc.cpp': 'static void WIN_QueueHeldModifiers( int eventTime )',
        }
        allowed = {'K_CTRL', 'K_SHIFT', 'K_ALT', 'K_MODE', 'K_COMMAND', 'K_SUPER'}
        for path, signature in sources.items():
            with self.subTest(path=path):
                body = function(read(path), signature)
                keys = set(re.findall(r'SE_KEY, (K_[A-Z_0-9]+), qtrue', body))
                self.assertTrue(keys)
                self.assertLessEqual(keys, allowed)

    def test_modifier_keys_cannot_become_the_cinematic_escape(self):
        output = build_and_run(self, r'''
#include "code/client/keycodes.h"
#include <cstdio>
static_assert( K_CTRL >= 128 && K_SHIFT >= 128 && K_ALT >= 128, "modifier below 128" );
static_assert( K_MODE >= 128 && K_SUPER >= 128 && K_COMMAND >= 128, "modifier below 128" );
static_assert( K_CTRL != K_MOUSE1 && K_SHIFT != K_MOUSE1 && K_ALT != K_MOUSE1 && K_MODE != K_MOUSE1
	&& K_SUPER != K_MOUSE1 && K_COMMAND != K_MOUSE1, "modifier is MOUSE1" );
int main() { std::puts( "modifiers ok" ); return 0; }
''', '.cpp', '-std=c++17')
        self.assertIn('modifiers ok', output)
        keys = read('code/client/cl_keys.cpp')
        down = function(keys, 'static void CL_KeyDownEvent( int key, unsigned time )')
        self.assertIn('if ( ( key < 128 || key == K_MOUSE1 )', down)


class AliceCinematicServerShutdownTests(unittest.TestCase):
    """Item 5: the cinematic's server shutdown keeps the local client."""

    def test_binding_shuts_the_server_down_without_the_client(self):
        cin = read('code/client/cl_cin.cpp')
        host = cin[cin.index('struct AliceCinematicHost {'):cin.index('\n};', cin.index('struct AliceCinematicHost {'))]
        shutdown = host[host.index('void ShutdownServer() {'):]
        shutdown = shutdown[:shutdown.index('\n\t}') + 3]
        order = [shutdown.index('Cvar_Set( "sv_killserver", "2" );'), shutdown.index('SV_Shutdown( "" );'),
                 shutdown.index('Cvar_Set( "sv_killserver", "0" );')]
        self.assertEqual(order, sorted(order))
        # Only Alice's CIN_Play, or F.A.K.K.2's, reaches this binding.
        self.assertIn('if ( Com_GameIsRitual() ) {\n\t\tif ( cls.state != CA_CINEMATIC ) {', cin)

    def test_server_shutdown_honours_the_request_and_nothing_else_changes(self):
        server = read('code/server/sv_init.cpp')
        shutdown = function(server, 'void SV_Shutdown( const char *finalmsg ) {')
        self.assertIn('if ( sv_killserver->integer != 2 )\n\t\tCL_Disconnect( qfalse );', shutdown)
        # The map load that follows a movie still drops the stale connection.
        main = read('code/client/cl_main.cpp')
        loading = function(main, 'void CL_MapLoading( void ) {')
        stale = loading[loading.index('} else {'):]
        self.assertLess(stale.index('Cvar_Set( "nextmap", "" );'), stale.index('CL_Disconnect( qfalse );'))

    def test_retail_cinematic_keeps_the_client_through_the_server_shutdown(self):
        exe = retail(self, 'alice.exe')
        # CIN_Play: SV_Shutdown("") at 0x004273df.
        self.assertEqual(exe.read(0x004273da, 5).hex(' '), '68 c0 fe 52 00')
        self.assertEqual(exe.read(0x0052fec0, 1), b'\0')
        self.assertEqual(exe.call_target(0x004273df), 0x00455910)
        # SV_Shutdown's calls: Com_Printf, SV_FinalMessage, the operator
        # commands (an empty stub), the master shutdown, the game's shutdown,
        # Z_Free of the clients, Cvar_Set( "sv_running", "0" ), Com_Printf.
        calls = {0x0045592d: 0x00438850, 0x0045594c: 0x004558a0, 0x00455955: 0x00451680,
                 0x0045595a: 0x00455cc0, 0x0045595f: 0x00454430, 0x0045597c: 0x00444ed0,
                 0x004559a6: 0x0043ab60, 0x004559b0: 0x00438850}
        for site, target in calls.items():
            with self.subTest(site=hex(site)):
                self.assertEqual(exe.call_target(site), target)
        self.assertEqual(exe.read(0x004559b9, 1), b'\xc3')
        self.assertEqual(exe.text(0x0051186c), 'sv_running')
        self.assertEqual(exe.read(0x00451680, 1), b'\xc3')
        # CL_Disconnect (0x0042abb0, which sends "disconnect" at 0x0042ac00)
        # is not among them; CL_MapLoading calls it (0x0042ab26).
        self.assertEqual(exe.read(0x0042ac00, 5).hex(' '), '68 14 e1 50 00')
        self.assertEqual(exe.text(0x0050e114), 'disconnect')
        self.assertNotIn(0x0042abb0, calls.values())
        self.assertEqual(exe.call_target(0x0042ab26), 0x0042abb0)
        # SV_FinalMessage sends `print` and `disconnect` and nothing more.
        self.assertEqual(exe.read(0x004558a9, 5).hex(' '), '68 c4 ac 51 00')
        self.assertEqual(exe.read(0x004558b5, 5).hex(' '), '68 14 e1 50 00')
        # The movie then puts the client at CA_CINEMATIC, and its end at
        # CA_DISCONNECTED.
        self.assertEqual(exe.read(0x0042750d, 10).hex(' '), 'c7 05 40 39 c2 00 09 00 00 00')
        self.assertEqual(exe.read(0x004257fe, 10).hex(' '), 'c7 05 40 39 c2 00 01 00 00 00')


if __name__ == '__main__':
    unittest.main()
