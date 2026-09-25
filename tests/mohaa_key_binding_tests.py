"""Medal of Honor: the local player's key bindings, which a listen server's
game reads through its imports 153..155 (a level's `getboundkey1 "+scores"`
names the key its hint tells the player to press).

The three functions are production's, compiled from
code/client/cl_mohaa_cgame.cpp against stub bindings; the server's imports
must hand them on only while a client runs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


class MohaaKeyBindingTests(unittest.TestCase):
    def test_bindings_are_found_and_named_as_openmohaa_names_them(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_mohaa_cgame.cpp').read_text()
        functions = ''.join(function(source, signature) for signature in (
            'int CL_MOHAAKeyStringToKeynum( const char *name ) {',
            'const char *CL_MOHAAKeyBindString( int keynum ) {',
            'void CL_MOHAAKeysForCommand( const char *command, int *key1, int *key2 ) {',
        ))
        harness = r'''
#include <cassert>
#include <cctype>
#include <cstring>
#include <map>
#include <string>
constexpr int MAX_KEYS = 1024;
std::map<int, std::string> bindings;
std::map<std::string, int> names{ { "TAB", 9 }, { "MOUSE1", 178 }, { "f", 'f' } };
const char *Key_GetBinding( int keynum ) {
	auto found = bindings.find( keynum );
	return found == bindings.end() ? nullptr : found->second.c_str();
}
int Key_StringToKeynum( const char *name ) {
	auto found = names.find( name );
	return found == names.end() ? -1 : found->second;
}
const char *Key_KeynumToString( int keynum ) {
	static char text[16];
	for ( const auto &entry : names ) if ( entry.second == keynum ) return entry.first.c_str();
	std::strcpy( text, "0x??" );
	return text;
}
int Q_stricmp( const char *a, const char *b ) {
	for ( ;; a++, b++ ) {
		const int x = std::tolower( static_cast<unsigned char>( *a ) );
		const int y = std::tolower( static_cast<unsigned char>( *b ) );
		if ( x != y ) return x - y;
		if ( !x ) return 0;
	}
}
''' + functions + r'''
int main() {
	int first = 0, second = 0;
	/* Unbound: both keys -1, which read back as "Not Bound". */
	CL_MOHAAKeysForCommand( "+scores", &first, &second );
	assert( first == -1 && second == -1 );
	assert( !std::strcmp( CL_MOHAAKeyBindString( first ), "Not Bound" ) );
	/* One key, then two: the lower number first, and a third ignored; the
	 * command is matched without regard to case. */
	bindings[9] = "+scores";
	CL_MOHAAKeysForCommand( "+SCORES", &first, &second );
	assert( first == 9 && second == -1 );
	assert( !std::strcmp( CL_MOHAAKeyBindString( first ), "TAB" ) );
	bindings[178] = "+scores";
	bindings[500] = "+scores";
	bindings['f'] = "+attackprimary";
	CL_MOHAAKeysForCommand( "+scores", &first, &second );
	assert( first == 9 && second == 178 );
	assert( !std::strcmp( CL_MOHAAKeyBindString( second ), "MOUSE1" ) );
	/* Out of range, a null command and null outputs. */
	assert( !std::strcmp( CL_MOHAAKeyBindString( MAX_KEYS ), "Out of Range" ) );
	assert( !std::strcmp( CL_MOHAAKeyBindString( -2 ), "Out of Range" ) );
	CL_MOHAAKeysForCommand( nullptr, &first, &second );
	assert( first == -1 && second == -1 );
	CL_MOHAAKeysForCommand( "+scores", nullptr, nullptr );
	/* Names go through the engine's own table. */
	assert( CL_MOHAAKeyStringToKeynum( "TAB" ) == 9 );
	assert( CL_MOHAAKeyStringToKeynum( "NOSUCHKEY" ) == -1 );
	assert( CL_MOHAAKeyStringToKeynum( nullptr ) == -1 );
	return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'keys.cpp'
            exe = Path(directory) / 'keys.exe'
            cpp.write_text(harness)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_server_hands_them_on_only_beside_a_client(self):
        game = (ROOT / 'code/server/server_mohaa_game.hpp').read_text()
        for signature, call in (
                ('int QDECL KeyStringToKeynum( const char *str ) {', 'CL_MOHAAKeyStringToKeynum( str )'),
                ('const char *QDECL KeyKeynumToBindString( int keynum ) {', 'CL_MOHAAKeyBindString( keynum )'),
                ('void QDECL KeyGetKeysForCommand( const char *command, int *key1, int *key2 ) {',
                 'CL_MOHAAKeysForCommand( command, key1, key2 )')):
            body = function(game, signature)
            self.assertIn('#ifndef DEDICATED', body)
            self.assertIn('if ( LocalClient() ) {', body)
            self.assertIn(call, body)
        local = function(game, 'bool LocalClient() {')
        self.assertIn('return com_cl_running && com_cl_running->integer;', local)


if __name__ == '__main__':
    unittest.main()
