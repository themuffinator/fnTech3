"""American McGee's Alice's `wait`, which retail alice.exe counts in milliseconds.

Retail's Cbuf_Execute (0x00437b80) takes a frame time and counts a pending
`wait N` down by it before every line; Com_Frame passes 0 to its first call
(0x0043a0b6) and the modified frame time only to the client's call after
SV_Frame (0x0043a183). Cmd_Wait_f stores atoi() unchanged (0x00437a12..
0x00437a1a), and Cbuf_ExecuteText's EXEC_NOW runs the one line and nothing
else (0x00437b6b). These tests compile cmd.c's own functions against a small
harness and run them under the Alice profile and under one other profile,
which must keep the frame-counted wait it always had.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define MAX_CMD_BUFFER 4096
#define MAX_CMD_LINE 1024
typedef struct { byte *data; int maxsize; int cursize; } cmd_t;
static int cmd_wait, cmd_frameMsec, cmd_aliasExpansions, nestedCmdOffset;
static cmd_t cmd_text;
static byte cmd_text_buf[MAX_CMD_BUFFER];
static bool alice;
static std::vector<std::string> ran;
static std::vector<std::string> argv;

static int Com_GameIsAlice( void ) { return alice ? 1 : 0; }
static void Com_DPrintf( const char *, ... ) {}
void QDECL Com_Error( errorParm_t, const char *, ... ) { std::abort(); }
static int Cmd_Argc( void ) { return static_cast<int>( argv.size() ); }
static const char *Cmd_Argv( int i ) { return i < Cmd_Argc() ? argv[i].c_str() : ""; }
static void Cbuf_AddText( const char *text ) {
    const int length = static_cast<int>( std::strlen( text ) );
    assert( cmd_text.cursize + length <= cmd_text.maxsize );
    std::memcpy( cmd_text.data + cmd_text.cursize, text, length );
    cmd_text.cursize += length;
}
static void Cbuf_InsertText( const char *text ) { (void)text; std::abort(); }
static void Cmd_Wait_f( void );
static void Cmd_ExecuteString( const char *text ) {
    argv.clear();
    std::string word;
    for ( const char *p = text; ; ++p ) {
        if ( !*p || *p == ' ' ) {
            if ( !word.empty() ) argv.push_back( word );
            word.clear();
            if ( !*p ) break;
        } else {
            word += *p;
        }
    }
    if ( argv.empty() ) return;
    if ( argv[0] == "wait" ) { Cmd_Wait_f(); return; }
    ran.push_back( argv[0] );
}
'''

HARNESS = r'''
static void Reset( bool profile ) {
    alice = profile;
    cmd_text.data = cmd_text_buf;
    cmd_text.maxsize = MAX_CMD_BUFFER;
    cmd_text.cursize = 0;
    cmd_wait = cmd_frameMsec = 0;
    ran.clear();
}

static void Frame( int msec ) {
    // What Com_Frame does: a pass with no time, then the client's pass with
    // the frame time.
    Cbuf_Execute();
    Cbuf_SetFrameMsec( msec );
    Cbuf_Execute();
}

int main() {
    // Alice: `wait 100` holds the rest until 100 ms of client frames pass.
    Reset( true );
    Cbuf_AddText( "a; wait 100; b\n" );
    Cbuf_Execute();
    assert( ran.size() == 1 && ran[0] == "a" );
    Frame( 40 );
    Frame( 40 );
    assert( ran.size() == 1 );
    Frame( 40 );
    assert( ran.size() == 2 && ran[1] == "b" );

    // A pass with no frame time counts nothing down.
    Reset( true );
    Cbuf_AddText( "wait 10; c\n" );
    for ( int i = 0; i < 50; ++i ) Cbuf_Execute();
    assert( ran.empty() );
    Frame( 10 );
    assert( ran.size() == 1 && ran[0] == "c" );

    // `wait 1` is one frame's wait, as retail's 0x00437bb0 makes it.
    Reset( true );
    Cbuf_AddText( "wait; d\n" );
    Cbuf_Execute();
    assert( ran.empty() );
    Cbuf_Execute();
    assert( ran.size() == 1 && ran[0] == "d" );

    // A negative count is no wait at all.
    Reset( true );
    Cbuf_AddText( "wait -5; e\n" );
    Cbuf_Execute();
    assert( ran.size() == 1 && ran[0] == "e" );

    // EXEC_NOW runs its line and leaves the pending wait and the buffer alone.
    Reset( true );
    Cbuf_AddText( "wait 2000; f\n" );
    Cbuf_Execute();
    Cbuf_ExecuteText( EXEC_NOW, "g" );
    assert( ran.size() == 1 && ran[0] == "g" );
    Cbuf_ExecuteText( EXEC_NOW, "" );
    assert( ran.size() == 1 && cmd_wait == 2000 );
    Frame( 1999 );
    assert( ran.size() == 1 );
    Frame( 1 );
    assert( ran.size() == 2 && ran[1] == "f" );

    // Any other profile keeps the frame-counted wait: a negative count is a
    // single frame, and EXEC_NOW discards the wait.
    Reset( false );
    Cbuf_AddText( "wait -5; h\n" );
    Cbuf_Execute();
    assert( ran.empty() && cmd_wait == 1 );
    Cbuf_Wait();
    Cbuf_Execute();
    assert( ran.size() == 1 && ran[0] == "h" );
    Reset( false );
    Cbuf_AddText( "wait 3; i\n" );
    Cbuf_Execute();
    Cbuf_ExecuteText( EXEC_NOW, "j" );
    assert( cmd_wait == 0 && ran.size() == 1 && ran[0] == "j" );
    Cbuf_Execute();
    assert( ran.size() == 2 && ran[1] == "i" );
    std::puts( "ok" );
    return 0;
}
'''


class AliceCommandWaitTests(unittest.TestCase):
    def test_wait_counts_client_frame_milliseconds_only_under_alice(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        cmd = (ROOT / 'code/qcommon/cmd.c').read_text(encoding='utf-8')
        source = PRELUDE
        for signature in ['static void Cmd_Wait_f( void ) {', 'void Cbuf_SetFrameMsec( int msec )',
                          'static qboolean Cbuf_AliceWaiting( int msec )', 'void Cbuf_Execute( void )',
                          'void Cbuf_Wait( void )', 'void Cbuf_ExecuteText( cbufExec_t exec_when, const char *text )']:
            source += '\n' + function(cmd, signature)
        source += HARNESS
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'wait.cpp'
            exe = Path(directory) / 'wait.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler, *target, *abi, '-std=c++17', '-I', str(ROOT), str(cpp),
                                    '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=20,
                                 **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_frame_hands_the_time_only_to_the_client_pass(self):
        common = (ROOT / 'code/qcommon/common.c').read_text(encoding='utf-8')
        frame = common[common.index('lastTime = com_frameTime;'):]
        server = frame.index('SV_Frame( gameMsec, realMsec );')
        # The first pass, before the server's frame, gets no time.
        self.assertNotIn('Cbuf_SetFrameMsec', frame[:server])
        self.assertIn('Cbuf_Execute();', frame[:server])
        # The client's pass gets the frame time the server ran, just before it
        # executes, and only in the client block.
        client = frame.index('if ( !com_dedicated->integer ) {', frame.index('#else', server))
        handed = frame.index('Cbuf_SetFrameMsec( gameMsec );', client)
        self.assertLess(handed, frame.index('Cbuf_Execute();', handed))
        self.assertLess(handed, frame.index('CL_Frame( gameMsec, realMsec );', client))
        self.assertEqual(len(re.findall(r'Cbuf_SetFrameMsec\(', frame)), 1)


if __name__ == '__main__':
    unittest.main()
