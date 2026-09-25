"""An rcon command reaches the engine whole, including one it forwards.

SVC_RemoteCommand finds the command after "rcon <password>" inside the
tokenizer's own buffer (Cmd_Cmd). Handing that pointer to Cmd_ExecuteString
let the tokenizer copy the command over the buffer it was reading, so a
command nothing on a listen server knows -- forwarded to the game by the text
it was given -- arrived short by the length of "rcon <password> ": `rcon
in_synthesize motion 400 0` reached the game as `ze motion 400 0`. Quake III
1.32 ran a copy, as SVC_RemoteCommand does again. These tests compile the
engine's own SVC_RemoteCommand and tokenizer against a harness whose
Cmd_ExecuteString tokenizes and then keeps the text it would forward.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

void Q_strncpyz( char *dest, const char *src, int destsize ) {
	// The copy the engine's makes when src lies inside dest: forward.
	const std::size_t length = std::min( std::strlen( src ), static_cast<std::size_t>( destsize - 1 ) );
	std::memmove( dest, src, length );
	dest[length] = '\0';
}
char *Q_stradd( char *dst, const char *src ) {
	while ( ( *dst = *src ) != '\0' ) { dst++; src++; }
	return dst;
}

static int cmd_argc;
static char *cmd_argv[MAX_STRING_TOKENS];
static int cmd_argoffsets[MAX_STRING_TOKENS];
static qboolean cmd_quoted[MAX_STRING_TOKENS];
static char cmd_tokenized[BIG_INFO_STRING + MAX_STRING_TOKENS];
static char cmd_cmd[BIG_INFO_STRING];
'''

HARNESS = r'''
struct netadr_t { int port; };
typedef struct rateLimit_s { int lastTime, burst; } rateLimit_t;
static bool SVC_RateLimitAddress( const netadr_t *, int, int ) { return false; }
static bool SVC_RateLimit( rateLimit_t *, int, int ) { return false; }
static cvar_t developer, rconPassword;
static cvar_t *com_developer = &developer, *sv_rconPassword = &rconPassword;
// Q3Test checks its release's own password cvar (1.02's rcon_password).
static bool q3test;
static char q3testPassword[] = "old";
static int Com_GameIsQ3Test( void ) { return q3test; }
static const char *SV_Q3TestRconPassword( void ) { return q3testPassword; }
static char rconPassword2[64];
static netadr_t redirectAddress;
static const char *NET_AdrToString( const netadr_t * ) { return "127.0.0.1"; }
static std::string printed;
void QDECL Com_Printf( const char *format, ... ) {
	char text[4096];
	va_list args;
	va_start( args, format );
	std::vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	printed += text;
}
void QDECL Com_DPrintf( const char *, ... ) {}
static void Com_BeginRedirect( char *, int, void ( * )( const char * ) ) {}
static void Com_EndRedirect( void ) {}
static void SV_FlushRedirect( const char * ) {}
template<typename T, std::size_t N>
static constexpr int SV_ArraySize( const std::array<T, N> & ) { return static_cast<int>( N ); }
template<typename F> struct ScopeExit { F f; ~ScopeExit() { f(); } };
template<typename F> static ScopeExit<F> SV_MakeScopeExit( F &&f ) { return { std::forward<F>( f ) }; }

/* The engine's Cmd_ExecuteString tokenizes first; a command nothing knows is
 * then forwarded to the server by the text it was given. */
static int executed;
static std::string forwarded, firstWord;
void Cmd_ExecuteString( const char *text ) {
	Cmd_TokenizeString( text );
	executed++;
	firstWord = Cmd_Argv( 0 );
	forwarded = text;
}

static void Rcon( const char *packet ) {
	netadr_t from{};
	Cmd_TokenizeString( packet );
	executed = 0;
	forwarded.clear();
	printed.clear();
	SVC_RemoteCommand( &from );
}

int main() {
	char password[] = "probe";
	rconPassword.string = password;

	Rcon( "rcon probe in_synthesize motion 400 0" );
	assert( executed == 1 && firstWord == "in_synthesize" );
	assert( forwarded == "in_synthesize motion 400 0" );
	assert( printed.find( "Rcon from 127.0.0.1: in_synthesize motion 400 0" ) != std::string::npos );

	// A quoted password, and the spaces the walk skips.
	char quoted[] = "pro be";
	rconPassword.string = quoted;
	Rcon( "rcon \"pro be\"   give   all" );
	assert( executed == 1 && forwarded == "give   all" && firstWord == "give" );

	// A wrong password runs nothing.
	Rcon( "rcon wrong give all" );
	assert( executed == 0 && printed.find( "Bad rconpassword." ) != std::string::npos );

	// Q3Test takes its release's password, not this engine's.
	q3test = true;
	Rcon( "rcon old give all" );
	assert( executed == 1 && forwarded == "give all" );
	Rcon( "rcon \"pro be\" give all" );
	assert( executed == 0 && printed.find( "Bad rconpassword." ) != std::string::npos );
	q3test = false;
	std::puts( "ok" );
	return 0;
}
'''


class RconCommandCopyTests(unittest.TestCase):
    def test_forwarded_rcon_command_arrives_whole(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        cmd = read('code/qcommon/cmd.c')
        server = read('code/server/sv_main.cpp')
        source = PRELUDE
        for signature in ('int Cmd_Argc( void ) {', 'const char *Cmd_Argv( int arg ) {',
                          'char *Cmd_ArgsFrom( int arg ) {', 'char *Cmd_Cmd( void )',
                          'static void Cmd_TokenizeString2( const char *text_in, qboolean ignoreQuotes ) {',
                          'void Cmd_TokenizeString( const char *text_in ) {'):
            source += function(cmd, signature)
        source += HARNESS.replace('static void Rcon(', function(
            server, 'static void SVC_RemoteCommand( const netadr_t *from ) {') + '\nstatic void Rcon(', 1)
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'rcon.cpp'
            exe = Path(directory) / 'rcon.exe'
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


if __name__ == '__main__':
    unittest.main()
