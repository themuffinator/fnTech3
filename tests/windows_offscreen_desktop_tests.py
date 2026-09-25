"""A Windows client's offscreen audio state follows the desktop it is on.

WIN_UpdateOffscreen mutes a client whose desktop is not the input desktop,
such as a probe's private desktop. While the input desktop cannot be read,
as during a session lock, a state it once determined stands. A client that
has not determined one yet, having started during a lock, counts as on
screen only on the desktop named Default. The probe compiles the production
function against scripted desktop stubs, one process per scenario, since the
function keeps its state in statics.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from process_relaunch_tests import function

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'code/win32/win_main.cpp'
PRELUDE = r'''
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
typedef enum { qfalse, qtrue } qboolean;
typedef wchar_t WCHAR;
typedef int BOOL;
typedef unsigned long DWORD;
struct Desktop { const wchar_t *name; };
typedef Desktop *HDESK;
#define FALSE 0
#define UOI_NAME 2
#define DESKTOP_READOBJECTS 0x0001L
qboolean gw_offscreen = qfalse;
static int clockMs, opened, closed;
static bool locked, ownMissing;
static Desktop own = { L"Default" }, input = { L"Default" };
static void Fail( const char *what ) { std::printf( "%s\n", what ); std::exit( 90 ); }
static int Sys_Milliseconds( void ) { return clockMs; }
static DWORD GetCurrentThreadId( void ) { return 7; }
static HDESK GetThreadDesktop( DWORD thread ) {
 if ( thread != 7 ) Fail( "asked for another thread's desktop" );
 return ownMissing ? NULL : &own;
}
// A locked session's input desktop is Winlogon, which a user's process
// cannot open.
static HDESK OpenInputDesktop( DWORD flags, BOOL inherit, long access ) {
 if ( flags != 0 || inherit != FALSE || access != DESKTOP_READOBJECTS ) Fail( "input desktop access" );
 if ( locked ) return NULL;
 ++opened;
 return &input;
}
static BOOL GetUserObjectInformationW( HDESK desktop, int index, void *info, std::size_t length, DWORD *needed ) {
 if ( index != UOI_NAME || needed != NULL ) Fail( "desktop query" );
 const std::size_t bytes = ( std::wcslen( desktop->name ) + 1 ) * sizeof( WCHAR );
 if ( length < bytes ) return FALSE;
 std::memcpy( info, desktop->name, bytes );
 return 1;
}
static BOOL CloseDesktop( HDESK desktop ) {
 if ( desktop != &input ) Fail( "closed the thread's own desktop" );
 ++closed;
 return 1;
}
static int TestWcsicmp( const wchar_t *a, const wchar_t *b ) {
 for ( ;; ++a, ++b ) {
  const std::wint_t x = std::towlower( *a ), y = std::towlower( *b );
  if ( x != y ) return x < y ? -1 : 1;
  if ( !x ) return 0;
 }
}
#define _wcsicmp TestWcsicmp
'''
DRIVER = r'''
static const wchar_t probe[] = L"fntech3_probe_0123456789abcdef0123456789abcdef";
// A desktop a desktop-switching tool shows in place of Default.
static const wchar_t other[] = L"Desktop 2";
static wchar_t overlong[300];
static void Poll( int elapsed, qboolean expected, const char *step ) {
 clockMs += elapsed;
 WIN_UpdateOffscreen();
 if ( gw_offscreen != expected ) {
  std::printf( "%s: offscreen %d, expected %d\n", step, (int)gw_offscreen, (int)expected );
  std::exit( 1 );
 }
}
int main( int argc, char **argv ) {
 const char *s = argc > 1 ? argv[1] : "";
 for ( int i = 0; i < 299; ++i ) overlong[i] = L'x';
 if ( !std::strcmp( s, "unlocked-private" ) ) {
  own.name = probe;
  Poll( 0, qtrue, "first poll" );
  locked = true;
  Poll( 500, qtrue, "locked later" );
 } else if ( !std::strcmp( s, "locked-start-private" ) ) {
  own.name = probe; locked = true;
  Poll( 0, qtrue, "first poll during the lock" );
  Poll( 500, qtrue, "still locked" );
  locked = false;
  Poll( 500, qtrue, "unlocked" );
  locked = true;
  Poll( 500, qtrue, "locked again" );
 } else if ( !std::strcmp( s, "locked-start-default" ) ) {
  locked = true;
  Poll( 0, qfalse, "first poll during the lock" );
  Poll( 500, qfalse, "still locked" );
  locked = false;
  Poll( 500, qfalse, "unlocked" );
  input.name = other;
  Poll( 500, qtrue, "switched to another desktop" );
  locked = true;
  Poll( 500, qtrue, "locked away from its desktop" );
 } else if ( !std::strcmp( s, "determined-on-screen-stands" ) ) {
  own.name = other; input.name = other;
  Poll( 0, qfalse, "shown desktop" );
  locked = true;
  Poll( 500, qfalse, "locked" );
 } else if ( !std::strcmp( s, "locked-start-other" ) ) {
  own.name = other; locked = true;
  Poll( 0, qtrue, "first poll during the lock" );
  locked = false; input.name = other;
  Poll( 500, qfalse, "unlocked onto its desktop" );
 } else if ( !std::strcmp( s, "unreadable-input" ) ) {
  own.name = probe; input.name = overlong;
  Poll( 0, qtrue, "input desktop without a name" );
  input.name = probe;
  Poll( 500, qfalse, "shown desktop" );
  input.name = overlong;
  Poll( 500, qfalse, "input desktop without a name again" );
 } else if ( !std::strcmp( s, "unreadable-own" ) ) {
  ownMissing = true; input.name = other;
  Poll( 0, qfalse, "no thread desktop" );
  ownMissing = false; own.name = overlong;
  Poll( 500, qfalse, "thread desktop without a name" );
 } else if ( !std::strcmp( s, "interval" ) ) {
  own.name = probe; locked = true;
  Poll( 0, qtrue, "first poll during the lock" );
  locked = false; input.name = probe;
  Poll( 499, qtrue, "within 500 ms" );
  Poll( 1, qfalse, "after 500 ms" );
 } else if ( !std::strcmp( s, "case" ) ) {
  own.name = L"default"; locked = true;
  Poll( 0, qfalse, "first poll during the lock" );
  locked = false; input.name = L"DEFAULT";
  Poll( 500, qfalse, "unlocked" );
 } else {
  return 2;
 }
 if ( opened != closed ) Fail( "input desktop handle leaked" );
 return 0;
}
'''
SCENARIOS = (
    'unlocked-private', 'locked-start-private', 'locked-start-default',
    'determined-on-screen-stands', 'locked-start-other', 'unreadable-input',
    'unreadable-own', 'interval', 'case',
)


class WindowsOffscreenDesktopTests(unittest.TestCase):
    def test_offscreen_state_follows_the_desktop(self):
        cc = shutil.which('clang++') or shutil.which('g++')
        if not cc:
            self.skipTest('C++ compiler unavailable')
        source = SOURCE.read_text(encoding='utf-8')
        probe = PRELUDE + function(source, 'static void WIN_UpdateOffscreen( void )') + DRIVER
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'offscreen.cpp'
            cpp.write_text(probe, encoding='utf-8')
            exe = Path(folder) / 'offscreen.exe'
            build = subprocess.run([cc, *compiler_target_args(cc), '-std=c++17', str(cpp), '-o', str(exe)],
                                   capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            for scenario in SCENARIOS:
                with self.subTest(scenario=scenario):
                    run = subprocess.run([str(exe), scenario], capture_output=True, text=True, timeout=10)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_state_is_read_before_startup_and_every_client_frame(self):
        source = SOURCE.read_text(encoding='utf-8')
        startup = function(source, 'int WINAPI WinMain(')
        self.assertLess(startup.index('WIN_UpdateOffscreen();'), startup.index('Com_Init( sys_cmdline );'))
        events = function(source, 'void Sys_SendKeyEvents( void )')
        self.assertLess(events.index('WIN_UpdateOffscreen();'), events.index('HandleEvents();'))


if __name__ == '__main__':
    unittest.main()
