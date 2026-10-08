"""Elite Force's servers tell a kicked client and a quitting server's clients
what retail's engines tell them.

- Holomatch's `kick` drops a client with the reason `kicked` (stvoyHM.exe
  0x4cb1bc, pushed at 0x42e93f); fnTech3's other kick commands use the same
  reason there. Every other profile keeps Quake III's `was kicked`.
- When the server shuts down, Holomatch prints the message as it is and sends
  `disconnect server shut down` (stvoyHM.exe 0x4341b7, 0x4341c3, the string
  at 0x4cc054); single player sends a bare `disconnect` (stvoy.exe 0x425dc6,
  0x425dd5). Their messages end their own lines ("Server quit\n" at
  stvoyHM.exe 0x4c2138, stvoy.exe 0x48b5e0), so the print holds one newline.
  The retail Holomatch client shows the text after the word (CL_GetServerCommand,
  stvoyHM.exe 0x402b7a). Quake III 1.32's forms stay everywhere else, and no
  profile sends a disconnect to the local client.
- Holomatch's `devmap` removes the bots and its `map` from a single-player
  game keeps them (SV_Map_f 0x42e5a5..0x42e5d3); its SV_SpawnServer drops a
  bot, with an empty reason, for a level started with `killBots` or as single
  player (0x433b42..0x433b7b).

This compiles the production SV_KickedReason and SV_FinalMessage and runs
them for every profile in the catalog; each mutation below must fail it. The
map and spawn rules are checked in the production text.
"""
from pathlib import Path
import os
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
#include "code/server/server.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
server_t sv{};
serverStatic_t svs{};
static comGameId_t game;
int Com_GameIsQ3Test( void ) { return game == COM_GAME_Q3TEST; }
int Com_GameIsQuake3IHV( void ) { return game == COM_GAME_QUAKE3_IHV; }
int Com_GameIsSTVEF( void ) { return game == COM_GAME_STVEF_SP || game == COM_GAME_STVEF_MP; }
int Com_GameIsSTVEFMP( void ) { return game == COM_GAME_STVEF_MP; }
static std::vector<std::string> sent;
void SV_Q3TestFinalMessage( const char * ) { sent.emplace_back( "q3test" ); }
void SV_IHVNetFinalMessage( const char * ) { sent.emplace_back( "ihv" ); }
void NET_FlushPacketQueue( int ) {}
void QDECL SV_SendServerCommand( client_t *client, const char *format, ... ) {
	va_list args;
	va_start( args, format );
	char text[1024];
	const int length = vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	assert( client && length >= 0 && size_t( length ) < sizeof( text ) );
	sent.emplace_back( std::to_string( client - svs.clients ) + ": " + text );
}
void SV_SendClientSnapshot( client_t *client ) {
	sent.emplace_back( std::to_string( client - svs.clients ) + ": snapshot" );
}
'''

CHECKS = r'''
static std::vector<client_t> clients( 3 );

// Client 0 plays remotely, client 1 is the server's own, client 2 is still
// connecting.
static std::vector<std::string> quit( comGameId_t id, const char *message ) {
	game = id;
	for ( client_t &client : clients ) client = {};
	clients[0].state = CS_ACTIVE;
	clients[0].netchan.remoteAddress.type = NA_IP;
	clients[1].state = CS_ACTIVE;
	clients[1].netchan.remoteAddress.type = NA_LOOPBACK;
	clients[2].state = CS_CONNECTED;
	clients[2].netchan.remoteAddress.type = NA_IP;
	sent.clear();
	SV_FinalMessage( message );
	// Every client is a zombie for its last snapshot; Q3Test and the IHV
	// Test leave their own sessions to their own code.
	if ( !Com_GameIsQ3Test() && !Com_GameIsQuake3IHV() ) {
		for ( const client_t &client : clients ) assert( client.state == CS_ZOMBIE );
	}
	return sent;
}

int main() {
	svs.clients = clients.data();
	sv.maxclients = int( clients.size() );
	svs.time = 50000;
	for ( unsigned id = COM_GAME_QUAKE3; id < COM_GAME_COUNT; ++id ) {
		game = comGameId_t( id );
		const std::string reason = SV_KickedReason();
		assert( reason == ( id == COM_GAME_STVEF_MP ? "kicked" : "was kicked" ) );
		if ( id == COM_GAME_Q3TEST || id == COM_GAME_QUAKE3_IHV ) {
			assert( quit( comGameId_t( id ), "Server quit" ) == std::vector<std::string>{ id == COM_GAME_Q3TEST ? "q3test" : "ihv" } );
			continue;
		}
		for ( const char *message : { "Server quit", "Server quit\n", "killserver", "" } ) {
			const std::string text = message;
			const bool ownLine = !text.empty() && text.back() == '\n';
			// What a remote client is sent before its last snapshot.
			const auto commands = [&]( const std::string &n ) -> std::vector<std::string> {
				if ( id == COM_GAME_STVEF_MP || id == COM_GAME_STVEF_SP ) {
					return { n + ": print \"" + text + ( ownLine ? "" : "\n" ) + "\"",
						n + ( id == COM_GAME_STVEF_MP ? ": disconnect server shut down" : ": disconnect" ) };
				}
				return { n + ": print \"" + text + "\n\"\n", n + ": disconnect \"" + text + "\"" };
			};
			// The server's own client gets no command, only its snapshot. A
			// connecting client is offered both, which the command queue
			// takes only from a client in the game or downloading.
			std::vector<std::string> expected = commands( "0" );
			expected.insert( expected.end(), { "0: snapshot", "1: snapshot" } );
			for ( const std::string &line : commands( "2" ) ) expected.push_back( line );
			expected.push_back( "2: snapshot" );
			assert( quit( comGameId_t( id ), message ) == expected );
		}
	}
	svs.clients = nullptr;
	sv.maxclients = 0;
	return 0;
}
'''

# Each mutation replaces text in one production function and must fail.
MUTATIONS = {
    'Holomatch kicks with Quake III\'s words': (
        'static const char *SV_KickedReason(', 'Com_GameIsSTVEFMP() ? "kicked"', 'false ? "kicked"'),
    'every profile kicks with Holomatch\'s words': (
        'static const char *SV_KickedReason(', 'Com_GameIsSTVEFMP() ? "kicked"', 'true ? "kicked"'),
    'Elite Force quits with Quake III\'s commands': (
        'static void SV_FinalMessage(', 'if ( Com_GameIsSTVEF() ) {', 'if ( false ) {'),
    'Holomatch sends single player\'s bare command': (
        'static void SV_FinalMessage(', 'Com_GameIsSTVEFMP() ? "disconnect server shut down"',
        'false ? "disconnect server shut down"'),
    'Elite Force adds a second newline': (
        'static void SV_FinalMessage(', 'message[messageLength - 1] == \'\\n\' ) ? "" : "\\n"',
        'message[messageLength - 1] == \'\\n\' ) ? "\\n" : "\\n"'),
    'Elite Force sends the reason with its command': (
        'static void SV_FinalMessage(', '"disconnect server shut down" : "disconnect" );',
        '"disconnect server shut down" : "disconnect \\"%s\\"", message );'),
    'the local client is told to disconnect': (
        'static void SV_FinalMessage(', 'client.netchan.remoteAddress.type != NA_LOOPBACK',
        'client.netchan.remoteAddress.type != NA_BAD'),
}


def pieces():
    commands = (ROOT / 'code/server/sv_ccmds.cpp').read_text(encoding='utf-8')
    init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
    return {'static const char *SV_KickedReason(': function(commands, 'static const char *SV_KickedReason('),
            'static void SV_FinalMessage(': function(init, 'static void SV_FinalMessage(')}


def source(parts):
    return PRELUDE + '\n'.join(parts.values()) + '\n' + CHECKS


class STVEFServerMessageTests(unittest.TestCase):
    def run_probe(self, folder, name, text):
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(text, encoding='utf-8')
        built = subprocess.run([self.compiler, *self.flags, str(cpp), '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=180)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        return subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)

    def test_kicks_and_quits_use_each_engines_words(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(self.compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        self.flags = [*target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT)]
        production = pieces()
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-server-messages-', dir=ROOT / '.tmp') as directory:
            folder = Path(directory)
            ran = self.run_probe(folder, 'production', source(production))
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            for index, (description, (signature, old, new)) in enumerate(MUTATIONS.items()):
                with self.subTest(mutation=description):
                    self.assertEqual(production[signature].count(old), 1, old)
                    mutated = dict(production, **{signature: production[signature].replace(old, new)})
                    ran = self.run_probe(folder, f'mutation{index}', source(mutated))
                    self.assertNotEqual(ran.returncode, 0, description)

    def test_every_kick_command_uses_the_profiles_reason(self):
        commands = (ROOT / 'code/server/sv_ccmds.cpp').read_text(encoding='utf-8')
        for signature in ('static void SV_Kick_f(', 'static void SV_KickBots_f(',
                          'static void SV_KickAll_f(', 'static void SV_KickNum_f('):
            with self.subTest(command=signature):
                body = function(commands, signature)
                self.assertNotIn('"was kicked"', body)
                self.assertGreaterEqual(body.count('SV_DropClient('), 1)
                self.assertEqual(body.count('SV_DropClient('), body.count('SV_KickedReason()'))

    def test_holomatch_map_and_spawn_bot_rules(self):
        commands = (ROOT / 'code/server/sv_ccmds.cpp').read_text(encoding='utf-8')
        mapping = function(commands, 'static void SV_Map_f(')
        # The Holomatch rule follows Quake III's, which it replaces, and
        # comes before the level starts.
        generic = mapping.index('killBots = false;\n\t\t}')
        holomatch = mapping.index('if ( Com_GameIsSTVEFMP() ) {\n\t\t\tkillBots = cheat;\n\t\t}')
        self.assertLess(generic, holomatch)
        self.assertLess(holomatch, mapping.index('SV_SpawnServer( mapname.data(), SV_QBool( killBots ) );'))
        init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
        spawn = function(init, 'void SV_SpawnServer( const char *mapname, qboolean killBots ) {')
        rule = spawn[spawn.index('const bool holomatch = Com_GameIsSTVEFMP();'):]
        rule = rule[:rule.index('isBot = true;')]
        self.assertIn('( holomatch && Cvar_VariableValue( "g_gametype" ) == GT_SINGLE_PLAYER )', rule)
        self.assertIn('SV_DropClient( &slot.client, ( wolfET || holomatch ) ? "" : "was kicked" );', rule)
        # The game, which applies a latched game type, starts first.
        self.assertLess(spawn.index('SV_InitGameProgs'), spawn.index('const bool holomatch'))


if __name__ == '__main__':
    unittest.main()
