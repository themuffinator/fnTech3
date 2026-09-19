"""Quake III's UI and cgame get Quake III's own key numbers.

Original Quake III numbers the mouse wheel 183 and 184, joystick buttons
185-216 and aux keys 217-232 (the key name table of quake3.exe 1.32 and
code/ui/keycodes.h of the GPL release). This engine numbers mouse buttons 6-9
before the wheel, as retail Quake Live does (quakelive_steam.exe's table), so
from the wheel upward its numbers are four higher. q3_ui's menus take joystick
buttons 1-4 as Enter, so the wheel reached them as a key press on the focused
item. The probes compile the adapter, the key event helpers and the key import
helpers, the last against the engine's own key name table; the source checks
pin where the helpers are called and that Quake Live's modules keep this
engine's numbers. scripts/probe_q3_key_numbers.py compares both retail tables.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

NUMBER_PROBE = r'''
#include "code/compat/q3_key_adapter.h"
#include "code/compat/stvef_key_adapter.h"
#include <cassert>
#include <initializer_list>
// Quake Live's numbers are this engine's own, as quakelive_steam.exe names them.
static_assert( K_MOUSE5 == 182 && K_MOUSE6 == 183 && K_MOUSE9 == 186, "Quake Live mouse buttons" );
static_assert( K_MWHEELDOWN == 187 && K_MWHEELUP == 188, "Quake Live mouse wheel" );
static_assert( K_JOY1 == 189 && K_JOY32 == 220 && K_AUX1 == 221 && K_AUX16 == 236, "Quake Live joystick and aux keys" );
static_assert( K_CHAR_FLAG == 1024 && FNTECH3_Q3_K_AUX16 == 232, "character flag and Quake III's last key" );
int main() {
	// Quake III's numbers, as quake3.exe 1.32 names them.
	assert( FnTech3_Q3KeyToInternal( 'a' ) == 'a' && FnTech3_Q3KeyToInternal( 127 ) == K_BACKSPACE );
	assert( FnTech3_Q3KeyToInternal( 128 ) == K_COMMAND && FnTech3_Q3KeyToInternal( 177 ) == K_KP_EQUALS );
	assert( FnTech3_Q3KeyToInternal( 178 ) == K_MOUSE1 && FnTech3_Q3KeyToInternal( 182 ) == K_MOUSE5 );
	assert( FnTech3_Q3KeyToInternal( 183 ) == K_MWHEELDOWN && FnTech3_Q3KeyToInternal( 184 ) == K_MWHEELUP );
	assert( FnTech3_Q3KeyToInternal( 185 ) == K_JOY1 && FnTech3_Q3KeyToInternal( 216 ) == K_JOY32 );
	assert( FnTech3_Q3KeyToInternal( 217 ) == K_AUX1 && FnTech3_Q3KeyToInternal( 232 ) == K_AUX16 );
	for ( int key = 233; key <= 2048; ++key ) assert( FnTech3_Q3KeyToInternal( key ) == -1 );
	for ( int key : { -1, -2, -1024 } ) assert( FnTech3_Q3KeyToInternal( key ) == -1 );
	// Every Quake III number reaches a distinct engine key and comes back.
	bool seen[MAX_KEYS] = {};
	for ( int key = 0; key <= FNTECH3_Q3_K_AUX16; ++key ) {
		const int internal = FnTech3_Q3KeyToInternal( key );
		assert( internal >= 0 && internal < MAX_KEYS && !seen[internal] );
		seen[internal] = true;
		assert( FnTech3_Q3KeyFromInternal( internal ) == key && FnTech3_Q3KeyEventFromInternal( internal ) == key );
	}
	// Events: the wheel is the wheel, never joystick button 3 or 4, which q3_ui
	// takes as Enter, and mouse buttons 8 and 9 are not joystick buttons 1 and 2.
	assert( FnTech3_Q3KeyEventFromInternal( K_MWHEELDOWN ) == 183 && FnTech3_Q3KeyEventFromInternal( K_MWHEELUP ) == 184 );
	assert( FnTech3_Q3KeyEventFromInternal( K_JOY1 ) == 185 && FnTech3_Q3KeyEventFromInternal( K_JOY4 ) == 188 );
	assert( FnTech3_Q3KeyEventFromInternal( K_AUX1 ) == 217 && FnTech3_Q3KeyEventFromInternal( K_AUX16 ) == 232 );
	// Keys Quake III has no number for are never delivered.
	for ( int key : { K_MOUSE6, K_MOUSE7, K_MOUSE8, K_MOUSE9 } ) assert( FnTech3_Q3KeyEventFromInternal( key ) == -1 );
	for ( int key = K_AUX16 + 1; key < MAX_KEYS; ++key ) assert( FnTech3_Q3KeyEventFromInternal( key ) == -1 );
	assert( FnTech3_Q3KeyEventFromInternal( -1 ) == -1 && FnTech3_Q3KeyEventFromInternal( MAX_KEYS ) == -1 );
	// Characters keep K_CHAR_FLAG, 1024 in both numberings.
	for ( int ch = 0; ch < 256; ++ch ) assert( FnTech3_Q3KeyEventFromInternal( K_CHAR_FLAG | ch ) == ( 1024 | ch ) );
	assert( FnTech3_Q3KeyEventFromInternal( K_ESCAPE ) == 27 && FnTech3_Q3KeyEventFromInternal( K_ENTER ) == 13 );
	// Elite Force numbers its keys the same way.
	for ( int key = -2; key <= MAX_KEYS; ++key ) {
		assert( FnTech3_Q3KeyEventFromInternal( key ) == FnTech3_STVEFKeyEventFromInternal( key ) );
		assert( FnTech3_Q3KeyToInternal( key ) == FnTech3_STVEFKeyToInternal( key ) );
	}
}
'''

EVENT_PROBE = r'''
#include "code/compat/q3_key_adapter.h"
#include "code/compat/stvef_key_adapter.h"
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <vector>
typedef int qboolean;
enum { qfalse, qtrue };
struct vm_t { int id; };
static vm_t ui{ 1 }, cgame{ 2 };
static vm_t *uivm = &ui, *cgvm = &cgame;
enum { UI_KEY_EVENT = 3, CG_KEY_EVENT = 6 };
enum Game { QUAKE3, QUAKELIVE, RTCW, WOLFET, STVEF_MP, STVEF_SP } game;
static bool Com_GameIsQuake3() { return game == QUAKE3; }
static bool Com_GameIsSTVEF() { return game == STVEF_MP || game == STVEF_SP; }
struct Call { vm_t *vm; int count, call; std::vector<int> args; };
static std::vector<Call> calls;
static intptr_t VM_Call( vm_t *vm, int count, int call, ... ) {
	Call recorded{ vm, count, call, {} };
	va_list list;
	va_start( list, call );
	for ( int i = 0; i < count; ++i ) recorded.args.push_back( va_arg( list, int ) );
	va_end( list );
	calls.push_back( recorded );
	return 0;
}
EVENTS
// Returns the number the module was handed, or -1 when it was handed nothing.
static int Ui( int key, bool down ) {
	calls.clear();
	if ( down ) CL_UIKeyDownEvent( key, 1234 );
	else CL_UIKeyUpEvent( key, 1234 );
	if ( calls.empty() ) return -1;
	assert( calls.size() == 1 && calls[0].vm == uivm && calls[0].count == 3 && calls[0].call == UI_KEY_EVENT );
	assert( calls[0].args[1] == ( down ? qtrue : qfalse ) && calls[0].args[2] == 1234 );
	return calls[0].args[0];
}
static int Cgame( int key, bool down ) {
	calls.clear();
	CL_CGameKeyEvent( key, down ? qtrue : qfalse );
	if ( calls.empty() ) return -1;
	assert( calls.size() == 1 && calls[0].vm == cgvm && calls[0].count == 2 && calls[0].call == CG_KEY_EVENT );
	assert( calls[0].args[1] == ( down ? qtrue : qfalse ) );
	return calls[0].args[0];
}
static int Every( int key, int expected ) {
	for ( bool down : { true, false } ) {
		assert( Ui( key, down ) == expected );
		assert( Cgame( key, down ) == expected );
	}
	return expected;
}
int main() {
	// Quake III: the wheel is the wheel, joystick and aux keys are Quake III's,
	// and keys Quake III has no number for reach neither module.
	game = QUAKE3;
	Every( K_MWHEELDOWN, 183 );
	Every( K_MWHEELUP, 184 );
	Every( K_JOY1, 185 );
	Every( K_JOY32, 216 );
	Every( K_AUX1, 217 );
	Every( K_AUX16, 232 );
	for ( int key : { K_MOUSE6, K_MOUSE7, K_MOUSE8, K_MOUSE9, K_SUPER, K_PAD0_SOUTH, K_CONSOLE } ) Every( key, -1 );
	for ( int key : { K_A, K_ESCAPE, K_ENTER, K_MOUSE1, K_MOUSE5, K_KP_EQUALS } ) Every( key, key );
	assert( Ui( K_CHAR_FLAG | 'q', true ) == ( 1024 | 'q' ) );
	// Quake Live's modules, and RTCW's and ET's (converted in vm.c), get this
	// engine's numbers.
	for ( Game other : { QUAKELIVE, RTCW, WOLFET } ) {
		game = other;
		for ( int key : { K_MOUSE6, K_MOUSE9, K_MWHEELDOWN, K_MWHEELUP, K_JOY1, K_AUX16, K_PAD0_SOUTH, K_ESCAPE } ) Every( key, key );
	}
	// Elite Force's numbers are Quake III's.
	for ( Game eliteForce : { STVEF_MP, STVEF_SP } ) {
		game = eliteForce;
		Every( K_MWHEELDOWN, 183 );
		Every( K_AUX16, 232 );
		Every( K_MOUSE6, -1 );
		Every( K_PAD0_SOUTH, -1 );
	}
}
'''

IMPORT_PROBE = r'''
#include "code/compat/q3_key_adapter.h"
#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
typedef int qboolean;
enum { qfalse, qtrue };
enum errorParm_t { ERR_FATAL, ERR_DROP };
struct ProbeError {};
static void Com_Error( errorParm_t, const char * ) { throw ProbeError{}; }
static void Q_strncpyz( char *dest, const char *src, int size ) {
	int i = 0;
	for ( ; i + 1 < size && src[i]; ++i ) dest[i] = src[i];
	dest[i] = '\0';
}
static int Com_sprintf( char *dest, int size, const char *format, ... ) {
	va_list list;
	va_start( list, format );
	const int length = std::vsnprintf( dest, size, format, list );
	va_end( list );
	return length;
}
static int Q_stricmp( const char *a, const char *b ) {
	for ( ;; ++a, ++b ) {
		const int x = std::toupper( static_cast<unsigned char>( *a ) ), y = std::toupper( static_cast<unsigned char>( *b ) );
		if ( x != y ) return x < y ? -1 : 1;
		if ( !x ) return 0;
	}
}
// The engine's binding table and key state, by engine key number.
static std::map<int, std::string> bindings;
static bool down[MAX_KEYS];
static void Key_SetBinding( int key, const char *binding ) {
	assert( key >= 0 && key < MAX_KEYS );
	bindings[key] = binding;
}
static const char *Key_GetBinding( int key ) {
	if ( key < 0 || key >= MAX_KEYS ) return "";
	const auto found = bindings.find( key );
	return found == bindings.end() ? nullptr : found->second.c_str();
}
static qboolean Key_IsDown( int key ) { return key >= 0 && key < MAX_KEYS && down[key] ? qtrue : qfalse; }
KEYNAMES
IMPORTS
static std::string Name( int key ) {
	char text[32];
	std::memset( text, '#', sizeof( text ) );
	CL_Q3KeyString( key, text, sizeof( text ), qfalse );
	return text;
}
static std::string Binding( int key ) {
	char text[32];
	std::memset( text, '#', sizeof( text ) );
	CL_Q3KeyString( key, text, sizeof( text ), qtrue );
	return text;
}
int main() {
	// Names of Quake III's numbers are the engine's names for the same keys.
	assert( Name( 183 ) == "MWHEELDOWN" && Name( 184 ) == "MWHEELUP" && Name( 182 ) == "MOUSE5" );
	assert( Name( 185 ) == "JOY1" && Name( 216 ) == "JOY32" && Name( 217 ) == "AUX1" && Name( 232 ) == "AUX16" );
	assert( Name( 'a' ) == "a" && Name( ';' ) == "SEMICOLON" && Name( 13 ) == "ENTER" && Name( 127 ) == "BACKSPACE" );
	// Numbers Quake III has no key for are named as Quake III names them.
	assert( Name( 233 ) == "0xe9" && Name( 255 ) == "0xff" && Name( '"' ) == "0x22" );
	assert( Name( -1 ) == "<KEY NOT FOUND>" && Name( -2 ) == "<OUT OF RANGE>" && Name( 256 ) == "<OUT OF RANGE>" );
	assert( Name( 1024 ) == "<OUT OF RANGE>" );

	// Bindings are made and read through Quake III's numbers.
	CL_Q3SetBinding( 183, "weapnext" );
	CL_Q3SetBinding( 184, "weapprev" );
	CL_Q3SetBinding( 185, "+button2" );
	assert( bindings.at( K_MWHEELDOWN ) == "weapnext" && bindings.at( K_MWHEELUP ) == "weapprev" );
	assert( bindings.at( K_JOY1 ) == "+button2" && !bindings.count( K_MOUSE6 ) && !bindings.count( K_MOUSE8 ) );
	assert( Binding( 183 ) == "weapnext" && Binding( 185 ) == "+button2" && Binding( 186 ) == "" );
	// A number outside Quake III's vocabulary never becomes an engine binding.
	const std::size_t before = bindings.size();
	for ( int key : { 233, 236, 255, 256, -1, -2 } ) CL_Q3SetBinding( key, "+attack" );
	CL_Q3SetBinding( 178, nullptr );
	assert( bindings.size() == before );
	bindings[K_AUX16 + 1] = "engine only";
	assert( Binding( 233 ) == "" && Binding( -1 ) == "" && Binding( 300 ) == "" );

	// Key state.
	down[K_MWHEELUP] = true;
	down[K_MOUSE6] = true;
	assert( CL_Q3KeyIsDown( 184 ) && !CL_Q3KeyIsDown( 183 ) && !CL_Q3KeyIsDown( 188 ) );
	down[K_JOY4] = true;
	assert( CL_Q3KeyIsDown( 188 ) && !CL_Q3KeyIsDown( 233 ) && !CL_Q3KeyIsDown( -1 ) && !CL_Q3KeyIsDown( 1024 ) );

	// The key bound to a command, in Quake III's numbering and order.
	assert( CL_Q3KeyForBinding( "WEAPPREV" ) == 184 && CL_Q3KeyForBinding( "+button2" ) == 185 );
	bindings[K_MOUSE6] = "+attack";
	assert( CL_Q3KeyForBinding( "+attack" ) == -1 );
	bindings[K_AUX1] = "+attack";
	bindings[K_MOUSE1] = "+attack";
	assert( CL_Q3KeyForBinding( "+attack" ) == 178 );
	bindings[K_PAD0_SOUTH] = "+moveup";
	bindings[K_SPACE] = "+moveup";
	assert( CL_Q3KeyForBinding( "+moveup" ) == ' ' && CL_Q3KeyForBinding( "engine only" ) == -1 );
	assert( CL_Q3KeyForBinding( nullptr ) == -1 && CL_Q3KeyForBinding( "unbound" ) == -1 );

	// Empty buffers are left alone; a missing one is refused.
	char untouched = 'x';
	CL_Q3KeyString( 183, &untouched, 0, qfalse );
	assert( untouched == 'x' );
	bool refused = false;
	try { CL_Q3KeyString( 183, nullptr, 8, qtrue ); } catch ( const ProbeError & ) { refused = true; }
	assert( refused );
	char small[4];
	CL_Q3KeyString( 183, small, sizeof( small ), qfalse );
	assert( !std::strcmp( small, "MWH" ) );
}
'''


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def build_and_run(test, source, prefix):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('compiler unavailable')
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target:
        target = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp') as folder:
        cpp = Path(folder) / 'probe.cpp'
        binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *target, '-std=c++17', '-Wall', '-Wextra', '-I', str(ROOT), str(cpp), '-o', str(binary)],
                               capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30, **options)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


def engine_key_names():
    """The engine's key name table and name function, from keys.c."""
    keys = read('code/qcommon/keys.c')
    table = keys[keys.index('typedef struct {\n\tconst char *name;\n\tint keynum;\n} keyname_t;'):]
    table = table[:table.index('\n};\n') + 4]
    return table + function(keys, 'const char *Key_KeynumToString( int keynum ) {')


class Q3KeyNumberTests(unittest.TestCase):
    def test_key_numbers(self):
        build_and_run(self, NUMBER_PROBE, 'q3-keys-')

    def test_key_events_use_each_games_numbers(self):
        keys = read('code/client/cl_keys.cpp')
        events = ''.join(function(keys, signature) for signature in (
            'static int CL_ModuleKeyEvent(', 'static void CL_UIKeyDownEvent(',
            'static void CL_UIKeyUpEvent(', 'static void CL_CGameKeyEvent('))
        build_and_run(self, EVENT_PROBE.replace('EVENTS', events), 'q3-key-events-')

    def test_key_imports_use_quake3_numbers(self):
        keys = read('code/client/cl_keys.cpp')
        imports = ''.join(function(keys, signature) for signature in (
            'qboolean CL_Q3KeyIsDown(', 'void CL_Q3SetBinding(', 'void CL_Q3KeyString(', 'int CL_Q3KeyForBinding('))
        source = IMPORT_PROBE.replace('KEYNAMES', engine_key_names()).replace('IMPORTS', imports)
        build_and_run(self, source, 'q3-key-imports-')

    def test_every_key_event_is_numbered_for_its_game(self):
        keys = read('code/client/cl_keys.cpp')
        # One send of each kind, inside the helpers; the rest are character
        # events, whose numbers are the same in every game.
        self.assertEqual(keys.count('UI_KEY_EVENT'), 3)
        self.assertEqual(keys.count('VM_Call( uivm, 3, UI_KEY_EVENT, key, qtrue, time );'), 1)
        self.assertEqual(keys.count('VM_Call( uivm, 3, UI_KEY_EVENT, key, qfalse, time );'), 1)
        self.assertEqual(keys.count('VM_Call( uivm, 3, UI_KEY_EVENT, utf8Byte | K_CHAR_FLAG,'), 1)
        self.assertEqual(keys.count('CG_KEY_EVENT'), 1)
        self.assertEqual(keys.count('VM_Call( cgvm, 2, CG_KEY_EVENT, key, down );'), 1)
        self.assertEqual(keys.count('CL_UIKeyDownEvent( key, time );'), 2)
        self.assertEqual(keys.count('CL_UIKeyUpEvent( key, time );'), 2)
        self.assertEqual(keys.count('CL_CGameKeyEvent( key, qtrue );'), 1)
        self.assertEqual(keys.count('CL_CGameKeyEvent( key, qfalse );'), 1)
        for helper in ('static void CL_UIKeyDownEvent(', 'static void CL_UIKeyUpEvent(', 'static void CL_CGameKeyEvent('):
            self.assertIn('key = CL_ModuleKeyEvent( key );', function(keys, helper))
        module = function(keys, 'static int CL_ModuleKeyEvent(')
        self.assertLess(module.index('if ( Com_GameIsQuake3() ) {\n\t\treturn FnTech3_Q3KeyEventFromInternal( key );'),
                        module.index('if ( Com_GameIsSTVEF() ) {\n\t\treturn FnTech3_STVEFKeyEventFromInternal( key );'))
        self.assertTrue(module.endswith('\treturn key;\n}\n'))

    def test_quake3_imports_convert_and_quake_live_imports_do_not(self):
        ui = read('code/client/cl_ui.cpp')
        shared = ui[ui.index('\tcase UI_KEY_KEYNUMTOSTRINGBUF:'):ui.index('\tcase UI_KEY_GETOVERSTRIKEMODE:')]
        self.assertIn('if ( Com_GameIsQuake3() ) {\n\t\t\tCL_Q3KeyString( args[1], VMA(2), args[3], qfalse );\n'
                      '\t\t} else {\n\t\t\tKey_KeynumToStringBuf( args[1], VMA(2), args[3] );', shared)
        self.assertIn('if ( Com_GameIsQuake3() ) {\n\t\t\tCL_Q3KeyString( args[1], VMA(2), args[3], qtrue );\n'
                      '\t\t} else {\n\t\t\tKey_GetBindingBuf( args[1], VMA(2), args[3] );', shared)
        self.assertIn('if ( Com_GameIsQuake3() ) {\n\t\t\tCL_Q3SetBinding( args[1], VMA(2) );\n'
                      '\t\t} else {\n\t\t\tKey_SetBinding( args[1], VMA(2) );', shared)
        self.assertIn('return Com_GameIsQuake3() ? CL_Q3KeyIsDown( args[1] ) : Key_IsDown( args[1] );', shared)
        # Quake Live's native imports reach the same cases with this engine's numbers.
        for trap in ('KEYNUMTOSTRINGBUF', 'GETBINDINGBUF', 'SETBINDING', 'ISDOWN'):
            self.assertIn(f'UI_Import_Call( UI_KEY_{trap}, {{ keynum', ui)
        cgame = read('code/client/cl_cgame.cpp')
        shared = cgame[cgame.index('\tcase CG_KEY_ISDOWN:'):cgame.index('\tcase CG_KEY_GETOVERSTRIKEMODE:')]
        self.assertIn('return Com_GameIsQuake3() ? CL_Q3KeyIsDown( args[1] ) : Key_IsDown( args[1] );', shared)
        self.assertIn('return Com_GameIsQuake3() ? CL_Q3KeyForBinding( VMA(1) ) : Key_GetKey( VMA(1) );', shared)
        self.assertIn('if ( Com_GameIsQuake3() ) {\n\t\t\tCL_Q3KeyString( args[1], VMA(2), args[3], qfalse );\n'
                      '\t\t} else {\n\t\t\tQ_strncpyz( VMA(2), Key_KeynumToString( args[1] ), args[3] );', shared)
        self.assertIn('if ( Com_GameIsQuake3() ) {\n\t\t\tCL_Q3KeyString( args[1], VMA(2), args[3], qtrue );\n'
                      '\t\t\treturn 0;\n\t\t}\n\t\tbinding = Key_GetBinding( args[1] );', shared)
        self.assertIn('if ( Com_GameIsQuake3() ) {\n\t\t\tCL_Q3SetBinding( args[1], VMA(2) );\n'
                      '\t\t} else {\n\t\t\tKey_SetBinding( args[1], VMA(2) );', shared)
        # The Quake III profile loads only QVMs and vmMain libraries, never
        # Quake Live's structured modules.
        profiles = read('code/qcommon/game_profile.cpp')
        quake3 = profiles[profiles.index('\tCOM_GAME_QUAKE3,\n\t"quake3",'):profiles.index('\tCOM_GAME_QUAKELIVE,\n\t"quakelive",')]
        self.assertIn('\tCOM_MODULE_API_QVM | COM_MODULE_API_LEGACY_NATIVE,\n\tCOM_MODULE_API_QVM,\n', quake3)
        self.assertNotIn('COM_MODULE_API_STRUCTURED_NATIVE', quake3)


if __name__ == '__main__':
    unittest.main()
