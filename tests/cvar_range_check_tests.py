"""Run the real cvar system through Cvar_CheckRange's initial range check.

Configuration files run before most engine cvars are registered, so a value
they set reaches Cvar_CheckRange as the cvar's current value. Cvar_Set2 skips
a value equal to the current one before validating it (a config replaying a
startup-only cvar must stay quiet), so the range check has to validate the
current value itself.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.c"
#include "code/qcommon/q_math.c"
#include "code/qcommon/cvar.c"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cvar_t *com_cl_running, *com_sv_running;
qboolean com_errorEntered;
static char printed[65536];
static int argc;
static const char *argv[4];
static int failures;

static void Append( const char *fmt, va_list args ) {
	size_t used = strlen( printed );
	vsnprintf( printed + used, sizeof( printed ) - used, fmt, args );
}
void QDECL Com_Printf( const char *fmt, ... ) { va_list a; va_start( a, fmt ); Append( fmt, a ); va_end( a ); }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
	va_list a; va_start( a, fmt ); vfprintf( stderr, fmt, a ); va_end( a );
	(void)level; exit( 3 );
}
char *CopyString( const char *in ) { char *out = (char *)malloc( strlen( in ) + 1 ); strcpy( out, in ); return out; }
void Z_Free( void *ptr ) { free( ptr ); }
void Cmd_AddCommand( const char *name, xcommand_t function ) { (void)name; (void)function; }
void Cmd_SetCommandCompletionFunc( const char *name, completionFunc_t complete ) { (void)name; (void)complete; }
int Cmd_Argc( void ) { return argc; }
const char *Cmd_Argv( int arg ) { return arg < argc ? argv[arg] : ""; }
char *Cmd_ArgsFrom( int arg ) { return (char *)Cmd_Argv( arg ); }
void Field_CompleteCommand( const char *cmd, qboolean doCommands, qboolean doCvars ) { (void)cmd; (void)doCommands; (void)doCvars; }
int Com_Filter( const char *filter, const char *name ) { (void)filter; (void)name; return 0; }
qboolean FS_InvalidGameDir( const char *gamedir ) {
	return !strcmp( gamedir, "." ) || !strcmp( gamedir, ".." )
		|| strchr( gamedir, '/' ) || strchr( gamedir, '\\' ) ? qtrue : qfalse;
}
void QDECL FS_Printf( fileHandle_t f, const char *fmt, ... ) { (void)f; (void)fmt; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)f; return len; }
int Com_GameIsRTCW( void ) { return 0; }
int Com_GameIsWolfET( void ) { return 0; }
const comGameProfile_t *Com_GameProfile( void ) { return NULL; }
uint32_t FnTech3_Q3CvarFlagsToInternal( uint32_t flags ) { return flags; }
uint32_t FnTech3_RTCWCvarFlagsToInternal( comGameId_t profile, uint32_t flags ) { (void)profile; return flags; }
void CL_WebView_PublishCvarChange( const char *name, const char *value, qboolean replicate ) { (void)name; (void)value; (void)replicate; }

/* One configuration line, the way Cbuf hands it to the seta command. */
static void Seta( const char *name, const char *value ) {
	argc = 3; argv[0] = "seta"; argv[1] = name; argv[2] = value;
	Cvar_Set_f();
}

#define CHECK( condition ) do { if ( !( condition ) ) { \
	fprintf( stderr, "line %d: %s\nconsole: %s\n", __LINE__, #condition, printed ); failures++; } } while ( 0 )

int main( void ) {
	cvar_t *cv;
	int count;

	Cvar_Init();

	/* A latched cvar the config set below its minimum is clamped at once, so
	   the value used right after registration is already in range. */
	Seta( "com_hunkMegs", "56" );
	cv = Cvar_Get( "com_hunkMegs", "256", CVAR_LATCH | CVAR_ARCHIVE );
	printed[0] = '\0';
	Cvar_CheckRange( cv, "64", NULL, CV_INTEGER );
	CHECK( cv->integer == 64 && !strcmp( cv->string, "64" ) && !cv->latchedString );
	CHECK( ( cv->flags & CVAR_ARCHIVE ) && !strcmp( cv->resetString, "256" ) );
	CHECK( strstr( printed, "com_hunkMegs" ) && strstr( printed, "out of range" ) );

	/* Above the maximum, and for a float range. */
	Seta( "cl_viewAccel", "12.5" );
	cv = Cvar_Get( "cl_viewAccel", "1.7", CVAR_ARCHIVE );
	Cvar_CheckRange( cv, "0", "8", CV_FLOAT );
	CHECK( !strcmp( cv->string, "8" ) && cv->value == 8.0f );

	/* A value in range is left alone: no write, no modification. */
	Seta( "cl_packetdup", "3" );
	cv = Cvar_Get( "cl_packetdup", "1", CVAR_ARCHIVE );
	count = cv->modificationCount;
	printed[0] = '\0';
	Cvar_CheckRange( cv, "0", "5", CV_INTEGER );
	CHECK( !strcmp( cv->string, "3" ) && cv->modificationCount == count && !printed[0] );

	/* An integer range stores an integer; a non-number takes the default. */
	Seta( "r_picmip", "3.981250" );
	cv = Cvar_Get( "r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_CheckRange( cv, "0", "16", CV_INTEGER );
	CHECK( !strcmp( cv->string, "3" ) && cv->integer == 3 );
	Seta( "cl_maxpackets", "fast" );
	cv = Cvar_Get( "cl_maxpackets", "125", CVAR_ARCHIVE );
	Cvar_CheckRange( cv, "15", "125", CV_INTEGER );
	CHECK( !strcmp( cv->string, "125" ) );

	/* A mod directory set before the filesystem starts gets its path check. */
	Seta( "fs_game", "../outside" );
	cv = Cvar_Get( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO );
	Cvar_CheckRange( cv, NULL, NULL, CV_FSPATH );
	CHECK( !strcmp( cv->string, "" ) );

	/* A numeric check without bounds keeps any number. */
	Seta( "sensitivity", "30" );
	cv = Cvar_Get( "sensitivity", "2", CVAR_ARCHIVE );
	Cvar_CheckRange( cv, NULL, NULL, CV_FLOAT );
	CHECK( !strcmp( cv->string, "30" ) );

	/* A config replaying a write-protected cvar's value stays quiet; a
	   different value is still refused. */
	cv = Cvar_Get( "journal", "0", CVAR_INIT | CVAR_PROTECTED );
	Cvar_CheckRange( cv, "0", "2", CV_INTEGER );
	printed[0] = '\0';
	Seta( "journal", "0" );
	CHECK( !printed[0] );
	Seta( "journal", "1" );
	CHECK( strstr( printed, "write protected" ) && !strcmp( cv->string, "0" ) );

	/* Checking a valid current value leaves a pending latched change alone. */
	cv = Cvar_Get( "r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_CheckRange( cv, "-2", "24", CV_INTEGER );
	Seta( "r_mode", "5" );
	CHECK( cv->latchedString && !strcmp( cv->latchedString, "5" ) );
	Cvar_CheckRange( cv, "-2", "24", CV_INTEGER );
	CHECK( !strcmp( cv->string, "3" ) && cv->latchedString && !strcmp( cv->latchedString, "5" ) );

	if ( !failures )
		printf( "cvar range check ok\n" );
	return failures ? 1 : 0;
}
'''


class CvarRangeCheckTests(unittest.TestCase):
    def test_initial_range_check_validates_the_current_value(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="cvar-range-", dir=scratch) as directory:
            path = Path(directory)
            fixture = path / "cvar_range.c"
            fixture.write_text(HARNESS, encoding="utf-8")
            executable = path / ("cvar_range.exe" if os.name == "nt" else "cvar_range")
            target = compiler_target_args(compiler)
            if os.name == "nt" and not target:
                target = ["-DFNTECH3_Q3_ONLY"]
            elif os.name != "nt":
                target = ["-m32"]
            build = subprocess.run([compiler, *target, "-std=c11", "-I", str(ROOT),
                str(fixture), "-o", str(executable), *([] if os.name == "nt" else ["-lm"])],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("cvar range check ok", result.stdout)


if __name__ == "__main__":
    unittest.main()
