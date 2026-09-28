"""A stack overflow on Windows is reported as an ERR_DROP instead of ending the process.

The exception filter used to report every fault on the stack that raised it.
After a stack overflow only a small reserve is left there, and the report's
Com_sprintf alone takes 32 KB: its stack probe faulted again and that second
fault ended the process. The filter now leaves the frame first, through
Com_ErrorAfterUnwind, and reports from the frame's abort point, where it also
puts the stack's guard page back.

The probe compiles the production filter, report and unwind code (sliced from
win_main.cpp, common.c and q_shared.c, Com_sprintf with its 32,000-byte buffer
included) into a small x86 process whose abort point is Com_Frame's, and
overflows its stack on purpose, one process per scenario: plain frames, three
overflows in a row, C++ frames whose destructors run in the unwind (one of them
taking 16 KB of the reserve), a frame printing through the redirect into a
buffer of its own, as rcon's does, whose text must outlive the frame, and
frames of a stand-in native module whose exception registrations must be
unlinked, not run. An access violation and a breakpoint still take their old
paths, and an overflow on another thread ends the process with the overflow's
own code instead of faulting again. Every scenario runs for the client and the
dedicated server.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import time
import unittest

from compiler_support import compiler_target_args
from process_relaunch_tests import function

ROOT = Path(__file__).resolve().parents[1]

# A stand-in native module: each level of its recursion links an exception
# registration whose handler is inside this image, as a module's frames that
# use exceptions do. Built without a SafeSEH table so the handler is valid.
MODULE = r'''
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>

typedef struct record_s { struct record_s *next; void *handler; } record_t;
static volatile LONG searched, unwound;

static EXCEPTION_DISPOSITION __cdecl ModuleHandler( EXCEPTION_RECORD *record, void *frame, CONTEXT *context, void *dispatcher ) {
	(void)frame; (void)context; (void)dispatcher;
	if ( record->ExceptionFlags & ( EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND ) ) {
		InterlockedIncrement( &unwound );
	} else {
		InterlockedIncrement( &searched );
	}
	return ExceptionContinueSearch;
}

__declspec(dllexport) int ModuleRecurse( int depth ) {
	NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
	volatile record_t record;
	volatile char frame[32];
	int result;

	record.next = (record_t *)tib->ExceptionList;
	record.handler = (void *)ModuleHandler;
	tib->ExceptionList = (struct _EXCEPTION_REGISTRATION_RECORD *)&record;
	frame[0] = (char)depth;
	if ( depth == INT_MAX ) {
		result = frame[0];
	} else {
		frame[1] = (char)ModuleRecurse( depth + 1 );
		result = frame[1];
	}
	tib->ExceptionList = (struct _EXCEPTION_REGISTRATION_RECORD *)record.next;
	return result;
}

__declspec(dllexport) LONG ModuleSearched( void ) { return searched; }
__declspec(dllexport) LONG ModuleUnwound( void ) { return unwound; }
'''

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include <windows.h>
#include <malloc.h>
#include <setjmp.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "code/qcommon/native_seh.h"
#include "code/qcommon/native_seh.c"

#define CHECK( x ) do { if ( !( x ) ) { std::printf( "FAILED line %d: %s\n", __LINE__, #x ); std::exit( 70 ); } } while ( 0 )

static jmp_buf abortframe;
static cvar_t dedicated, clientRunning;
cvar_t *com_dedicated = &dedicated;
cvar_t *com_cl_running = &clientRunning;
static int gammaRestores, windowsHidden, warnings, reports;
static bool announceDesktop;
static char reported[4][256];
static int destroyedAtReport = -1;
static int destroyed;

void GLW_RestoreGamma( void ) {
	++gammaRestores;
	if ( announceDesktop ) std::printf( "GAMMA %d\n", gammaRestores );
}
void GLW_HideFullscreenWindow( void ) { ++windowsHidden; }

int Q_vsnprintf( char *str, size_t size, const char *format, va_list ap ) {
	return std::vsnprintf( str, size, format, ap );
}
'''

# Com_Printf with common.c's print redirect in front, and Com_Error as far as a
# fault's report reaches it: its ERR_DROP printed, the redirect ended, then
# back to the abort point. Their buffers take what the engine's take.
PRINTS = r'''
void QDECL Com_Printf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list args;
	va_start( args, fmt );
	const int len = std::vsnprintf( text, sizeof( text ), fmt, args );
	va_end( args );
	if ( rd_buffer && !rd_flushing ) {
		if ( len + (int)std::strlen( rd_buffer ) > ( rd_buffersize - 1 ) ) {
			rd_flushing = qtrue;
			rd_flush( rd_buffer );
			rd_flushing = qfalse;
			*rd_buffer = '\0';
		}
		std::strncat( rd_buffer, text, rd_buffersize - 1 - std::strlen( rd_buffer ) );
		return;
	}
	if ( std::strstr( text, "WARNING" ) ) ++warnings;
	std::printf( "PRINT %s", text );
}

void NORETURN QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list args;
	va_start( args, fmt );
	std::vsnprintf( text, sizeof( text ), fmt, args );
	va_end( args );
	if ( level != ERR_DROP || reports >= 4 || std::strlen( text ) >= sizeof( reported[0] ) ) std::exit( 71 );
	std::strcpy( reported[reports++], text );
	destroyedAtReport = destroyed;
	Com_Printf( "********************\nERROR: %s\n********************\n", text );
	Com_EndRedirect();
	Q_longjmp( abortframe, 1 );
}

// The loaded native modules Com_Error unlinks; the stand-in, once loaded.
static void *nativeModule;
static int unlinkedRecords = -1;
int VM_UnlinkNativeExceptionRecords( void ) {
	unlinkedRecords = nativeModule ? Sys_UnlinkModuleExceptionRecords( &nativeModule, 1 ) : 0;
	return unlinkedRecords;
}
'''

HARNESS = r'''
#if defined( _MSC_VER )
#define TEST_NOINLINE __declspec( noinline )
#else
#define TEST_NOINLINE __attribute__(( noinline ))
#endif

// Com_Frame's abort point, taken from common.c.
static void Frame( void (*work)( void ) ) {
	LANDING
	work();
}

static void Overflow( void ) { Com_OverflowStack( 0 ); }
static void AccessViolation( void ) { *(volatile int *)nullptr = 0x12345678; }
static void Breakpoint( void ) { __debugbreak(); }

// How far down the stack's reservation is committed: an overflow commits all
// of it but the last page.
static unsigned CommittedFromBottom( void ) {
	MEMORY_BASIC_INFORMATION region;
	volatile char here = 0;
	CHECK( VirtualQuery( (const void *)&here, &region, sizeof( region ) ) );
	char *const base = (char *)region.AllocationBase;
	for ( char *at = base; VirtualQuery( at, &region, sizeof( region ) ) && region.AllocationBase == base;
		at = (char *)region.BaseAddress + region.RegionSize ) {
		if ( region.State == MEM_COMMIT ) return (unsigned)( at - base );
	}
	return ~0u;
}

static unsigned PageSize( void ) {
	SYSTEM_INFO system;
	GetSystemInfo( &system );
	return system.dwPageSize;
}

// Where a report says the fault was raised: its module and the offset in it.
static void ExpectAddress( const char *report, void *address, const char *module ) {
	HMODULE owner = NULL;
	char expected[160];
	CHECK( GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)address, &owner ) );
	std::snprintf( expected, sizeof( expected ), "\nException Address: %s@%x", module,
		(unsigned)( (byte *)address - (byte *)owner ) );
	const size_t length = std::strlen( report ), tail = std::strlen( expected );
	CHECK( length > tail && !std::strcmp( report + length - tail, expected ) );
}

static void ExpectOverflowReport( int index, const char *module ) {
	CHECK( reports == index + 1 );
	std::printf( "REPORT %d %s\n", index + 1, reported[index] );
	CHECK( !std::strncmp( reported[index], "Unhandled exception caught\nException Code: STACK_OVERFLOW\n",
		std::strlen( "Unhandled exception caught\nException Code: STACK_OVERFLOW\n" ) ) );
	ExpectAddress( reported[index], sys_overflowAddress, module );
	CHECK( com_unwoundError == NULL );
	CHECK( warnings == 0 );
	CHECK( gammaRestores == DESKTOP_RESTORES * ( index + 1 ) && windowsHidden == 0 );
	CHECK( CommittedFromBottom() <= PageSize() );
}

static void OverflowAndReport( int index ) {
	Frame( Overflow );
	ExpectOverflowReport( index, "probe.exe" );
	// The fault was raised in the recursion, and the report names it.
	CHECK( (byte *)sys_overflowAddress >= (byte *)&Com_OverflowStack );
	CHECK( (byte *)sys_overflowAddress < (byte *)&Com_OverflowStack + 0x100 );
}

// Engine frames between the fault and the abort point, with objects whose
// destructors the unwind runs, as it runs a scope exit's; the outermost one
// takes 16 KB of stack to clean up.
static int constructed;
struct Tracked {
	Tracked() { ++constructed; }
	~Tracked() { ++destroyed; }
};
static bool heavyCleanupRan;
struct HeavyCleanup {
	~HeavyCleanup() {
		volatile char scratch[16 * 1024];
		scratch[0] = 1;
		scratch[sizeof( scratch ) - 1] = 1;
		heavyCleanupRan = scratch[0] == 1;
	}
};
static TEST_NOINLINE int CleanupRecurse( int depth ) {
	Tracked tracked;
	volatile char frame[48];
	frame[0] = (char)depth;
	if ( depth == INT_MAX ) return frame[0];
	frame[1] = (char)CleanupRecurse( depth + 1 );
	return frame[1];
}
static void Cleanup( void ) {
	HeavyCleanup heavy;
	CleanupRecurse( 0 );
}

// A frame that prints into a buffer of its own through the redirect, as the
// one running an rcon command does, then overflows. The report's calls reuse
// that frame's stack.
static char flushed[4096];
static int flushes;
static void CollectFlush( const char *text ) {
	++flushes;
	std::strncat( flushed, text, sizeof( flushed ) - 1 - std::strlen( flushed ) );
}
static void RedirectedOverflow( void ) {
	char output[1024 - 16];
	Com_BeginRedirect( output, sizeof( output ), CollectFlush );
	Com_Printf( "before the overflow\n" );
	Com_OverflowStack( 0 );
}

typedef int ( *moduleRecurse_t )( int depth );
typedef LONG ( *moduleCount_t )( void );
static moduleRecurse_t moduleRecurse;
static void NativeModule( void ) { moduleRecurse( 0 ); }

// Only a stack overflow off the main thread comes back from the production
// filter; end the process with what it decided, before Windows Error Reporting
// would.
static LPTOP_LEVEL_EXCEPTION_FILTER productionFilter;
static LONG WINAPI ThreadScenarioFilter( EXCEPTION_POINTERS *info ) {
	const LONG verdict = productionFilter( info );
	const bool expected = verdict == EXCEPTION_CONTINUE_SEARCH
		&& info->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW
		&& GetCurrentThreadId() != sys_mainThread && reports == 0;
	TerminateProcess( GetCurrentProcess(), expected ? 90 + gammaRestores : 99 );
	return verdict;
}
static DWORD WINAPI OverflowThread( void * ) {
	Com_OverflowStack( 0 );
	return 0;
}

int main( int argc, char **argv ) {
	const char *scenario = argc > 1 ? argv[1] : "";
	ULONG guarantee = 0;

	std::setvbuf( stdout, NULL, _IONBF, 0 );
	clientRunning.integer = 1;
	InstallExceptionFilter();
	CHECK( sys_mainThread == GetCurrentThreadId() );
	CHECK( SetThreadStackGuarantee( &guarantee ) && guarantee == SYS_OVERFLOW_RESERVE );

	if ( !std::strcmp( scenario, "overflow" ) ) {
		OverflowAndReport( 0 );
	} else if ( !std::strcmp( scenario, "repeat" ) ) {
		// Each overflow uses up the guard page; each report puts it back.
		for ( int i = 0; i < 3; i++ ) {
			OverflowAndReport( i );
		}
	} else if ( !std::strcmp( scenario, "cleanup" ) ) {
		Frame( Cleanup );
		ExpectOverflowReport( 0, "probe.exe" );
		std::printf( "FRAMES %d\n", constructed );
		CHECK( constructed > 1000 && destroyed == constructed && destroyedAtReport == constructed );
		CHECK( heavyCleanupRan );
	} else if ( !std::strcmp( scenario, "redirect" ) ) {
		Frame( RedirectedOverflow );
		ExpectOverflowReport( 0, "probe.exe" );
		std::printf( "FLUSHED %d\n%s", flushes, flushed );
		// What the frame printed and the report reach the redirect's owner
		// together, as an ERR_DROP's prints do, and nothing after.
		char expected[512];
		std::snprintf( expected, sizeof( expected ),
			"before the overflow\n********************\nERROR: %s\n********************\n", reported[0] );
		CHECK( flushes == 1 && !std::strcmp( flushed, expected ) && rd_buffer == NULL );
	} else if ( !std::strcmp( scenario, "native" ) ) {
		HMODULE module = LoadLibraryA( argc > 2 ? argv[2] : "" );
		CHECK( module != NULL );
		moduleRecurse = (moduleRecurse_t)GetProcAddress( module, "ModuleRecurse" );
		const moduleCount_t searched = (moduleCount_t)GetProcAddress( module, "ModuleSearched" );
		const moduleCount_t unwound = (moduleCount_t)GetProcAddress( module, "ModuleUnwound" );
		CHECK( moduleRecurse && searched && unwound );
		nativeModule = module;
		Frame( NativeModule );
		ExpectOverflowReport( 0, "fixture_so.dll" );
		std::printf( "MODULE searched %ld unwound %ld unlinked %d\n", searched(), unwound(), unlinkedRecords );
		// The module's registrations were live when the overflow was dispatched,
		// and were unlinked before the unwind, as Com_Error unlinks them.
		CHECK( searched() > 1000 && unlinkedRecords == searched() && unwound() == 0 );
	} else if ( !std::strcmp( scenario, "access-violation" ) ) {
		Frame( AccessViolation );
		CHECK( reports == 1 );
		std::printf( "REPORT 1 %s\n", reported[0] );
		CHECK( !std::strncmp( reported[0], "Unhandled exception caught\nException Code: ACCESS_VIOLATION\nException Address: probe.exe@",
			std::strlen( "Unhandled exception caught\nException Code: ACCESS_VIOLATION\nException Address: probe.exe@" ) ) );
		CHECK( com_unwoundError == NULL && sys_overflowAddress == NULL );
		CHECK( gammaRestores == DESKTOP_RESTORES && warnings == 0 );
	} else if ( !std::strcmp( scenario, "breakpoint" ) ) {
		announceDesktop = true;
		Frame( Breakpoint );
		return 72;
	} else if ( !std::strcmp( scenario, "thread" ) ) {
		productionFilter = SetUnhandledExceptionFilter( ThreadScenarioFilter );
		HANDLE thread = CreateThread( NULL, 256 * 1024, OverflowThread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL );
		CHECK( thread != NULL );
		WaitForSingleObject( thread, 60000 );
		return 73;
	} else {
		return 74;
	}

	std::printf( "DONE %s\n", scenario );
	return 0;
}
'''

SCENARIOS = ('overflow', 'repeat', 'cleanup', 'redirect', 'native', 'access-violation', 'breakpoint', 'thread')


def production_sources():
    main = (ROOT / 'code/win32/win_main.cpp').read_text(encoding='utf-8')
    start = main.index('static const char *GetExceptionName( DWORD code )')
    end = main.index('\n}', main.index('static void InstallExceptionFilter( void )')) + 2
    filter_code = main[start:end]
    common = (ROOT / 'code/qcommon/common.c').read_text(encoding='utf-8')
    start = re.search(r'static char\s+\*rd_buffer;', common).start()
    end = common.index('\n}', common.index('void Com_EndRedirect( void )')) + 2
    redirect = common[start:end]
    start = common.index('static void (*com_unwoundError)( void );')
    end = common.index('\n}', common.index('static void Com_RaiseUnwoundError( void ) {')) + 2
    unwind = common[start:end]
    overflow = function(common, 'static int Com_OverflowStack( int depth ) {')
    frame = function(common, 'void Com_Frame( qboolean noDelay ) {')
    landing = frame[frame.index('if ( Q_setjmp( abortframe ) ) {'):]
    landing = landing[:landing.index('\n\t}') + 3]
    shared = (ROOT / 'code/qcommon/q_shared.c').read_text(encoding='utf-8')
    strings = '\n'.join(function(shared, signature) for signature in (
        'void Q_strncpyz( char *dest, const char *src, int destsize )',
        'int QDECL Com_sprintf( char *dest, int size, const char *fmt, ...)'))
    return dict(filter=filter_code, unwind=unwind, overflow=overflow, landing=landing, strings=strings,
                redirect=redirect)


def remove_tree(folder):
    for attempt in range(20):
        try:
            shutil.rmtree(folder)
            return
        except FileNotFoundError:
            return
        except PermissionError:
            time.sleep(0.25)
    shutil.rmtree(folder, ignore_errors=True)


class WindowsStackOverflowTests(unittest.TestCase):
    def test_every_abort_point_raises_an_unwound_error_and_winmain_reserves_stack(self):
        common = (ROOT / 'code/qcommon/common.c').read_text(encoding='utf-8')
        for signature in ('void Com_Init( char *commandLine ) {', 'void Com_Frame( qboolean noDelay ) {'):
            body = function(common, signature)
            landing = body[body.index('if ( Q_setjmp( abortframe ) ) {'):]
            landing = landing[:landing.index('\n\t}')]
            # First, before anything reports or returns.
            self.assertTrue(landing.split('{', 1)[1].lstrip().startswith('Com_RaiseUnwoundError();'), signature)
        self.assertEqual(common.count('Q_setjmp( abortframe )'), 2)
        main = (ROOT / 'code/win32/win_main.cpp').read_text(encoding='utf-8')
        startup = function(main, 'int WINAPI WinMain(')
        self.assertLess(startup.index('InstallExceptionFilter();'), startup.index('Com_Init( sys_cmdline );'))
        self.assertEqual(main.count('SetUnhandledExceptionFilter( ExceptionFilter );'), 1)
        install = function(main, 'static void InstallExceptionFilter( void )')
        self.assertLess(install.index('SetThreadStackGuarantee( &reserve );'),
                        install.index('SetUnhandledExceptionFilter( ExceptionFilter );'))

    def compilers(self):
        if os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() == 'x86' and shutil.which('cl'):
            return 'msvc', shutil.which('cl')
        clang = shutil.which('clang++')
        if clang and compiler_target_args(clang) == ['--target=i686-pc-windows-msvc']:
            return 'clang', clang
        self.skipTest('needs an x86 Windows compiler (the Visual Studio x86 developer shell)')

    def build(self, kind, compiler, folder, dedicated):
        parts = production_sources()
        desktop = '0' if dedicated else '1'
        source = '\n'.join(('#define DESKTOP_RESTORES ' + desktop, PRELUDE, parts['redirect'], PRINTS,
                            parts['strings'], parts['unwind'], parts['overflow'], parts['filter'],
                            HARNESS.replace('LANDING', parts['landing'])))
        name = 'dedicated' if dedicated else 'client'
        work = folder / name
        work.mkdir()
        (work / 'probe.cpp').write_text(source, encoding='utf-8')
        (work / 'fixture_so.c').write_text(MODULE, encoding='utf-8')
        defines = ['/DDEDICATED'] if dedicated and kind == 'msvc' else (['-DDEDICATED'] if dedicated else [])
        if kind == 'msvc':
            commands = [
                [compiler, '/nologo', '/O2', '/MT', '/LD', 'fixture_so.c', '/Fe:fixture_so.dll', '/link', '/SAFESEH:NO'],
                [compiler, '/nologo', '/O2', '/MT', '/EHsc', '/std:c++17', '/I' + str(ROOT), *defines, 'probe.cpp',
                 '/Fe:probe.exe', '/link', '/STACK:1048576'],
            ]
        else:
            target = compiler_target_args(compiler)
            clang_c = str(Path(compiler).with_name('clang.exe'))
            commands = [
                [clang_c, *target, '-O2', '-shared', 'fixture_so.c', '-o', 'fixture_so.dll', '-Wl,/SAFESEH:NO'],
                [compiler, *target, '-O2', '-std=c++17', '-I', str(ROOT), *defines, 'probe.cpp', '-o', 'probe.exe',
                 '-Wl,/STACK:1048576'],
            ]
        for command in commands:
            built = subprocess.run(command, cwd=work, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, ' '.join(command) + '\n' + built.stdout + built.stderr)
        return work

    def run_scenario(self, work, scenario):
        command = [str(work / 'probe.exe'), scenario]
        if scenario == 'native':
            command.append(str(work / 'fixture_so.dll'))
        return subprocess.run(command, cwd=work, capture_output=True, text=True, timeout=120,
                              creationflags=subprocess.CREATE_NO_WINDOW)

    @unittest.skipUnless(os.name == 'nt', 'Windows structured exceptions')
    def test_stack_overflow_is_reported_and_other_faults_unchanged(self):
        kind, compiler = self.compilers()
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        folder = Path(tempfile.mkdtemp(prefix='stack-overflow-', dir=ROOT / '.tmp'))
        try:
            for dedicated in (False, True):
                work = self.build(kind, compiler, folder, dedicated)
                restores = 0 if dedicated else 1
                for scenario in SCENARIOS:
                    with self.subTest(dedicated=dedicated, scenario=scenario):
                        ran = self.run_scenario(work, scenario)
                        output = ran.stdout + ran.stderr
                        if scenario == 'breakpoint':
                            # Still no report: the process ends with the breakpoint's
                            # own code once the desktop is put back.
                            self.assertEqual(ran.returncode, 0x80000003, output)
                            self.assertEqual(output.count('GAMMA'), restores, output)
                            continue
                        if scenario == 'thread':
                            # No abort point to unwind to off the main thread: the
                            # filter puts the desktop back and passes the overflow on.
                            self.assertEqual(ran.returncode, 90 + restores, output)
                            continue
                        self.assertEqual(ran.returncode, 0, output)
                        self.assertIn(f'DONE {scenario}', output)
                        codes = re.findall(r'^REPORT \d+ Unhandled exception caught$\s^Exception Code: (\w+)$',
                                           output, re.M)
                        expected = {'repeat': ['STACK_OVERFLOW'] * 3,
                                    'access-violation': ['ACCESS_VIOLATION']}.get(scenario, ['STACK_OVERFLOW'])
                        self.assertEqual(codes, expected, output)
        finally:
            remove_tree(folder)


if __name__ == '__main__':
    unittest.main()
