"""Medal of Honor's deathmatch chat.

1.00's game module reads no say, say_team or tell: its only chat command is
its own `dmmessage <target> <words>`, 0 to all, -1 to the team, else to a
client (gamex86.dll's command list). Its client game registers messagemode,
say and their kin itself and opens the chat through import 159 with 300 (the
kind last opened), 100, 200, a client's number, or minus one of them for a
single message (0x30046490..0x300466db). The engine therefore registers no
chat command of its own for this profile (tests/console_notify_profile_tests.py
checks that and the chat line's dmmessage), and import 159 opens and closes
the chat line. The fixture compiles the import sliced from
code/client/cl_mohaa_cgame.cpp."""
import unittest
from pathlib import Path

from mohaa_permanent_decal_tests import build_and_run, function, read

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include <cassert>
#include <cstdio>
#define QDECL
#define MAX_CLIENTS 64
#define KEYCATCH_MESSAGE 0x0004
typedef int qboolean;
enum { qfalse, qtrue };
static int catcher, opens, closes;
static qboolean openedTeam;
int chat_dmTarget = 12345;
static int Key_GetCatcher( void ) { return catcher; }
static void Con_OpenChat( qboolean team ) { catcher |= KEYCATCH_MESSAGE; openedTeam = team; opens++; }
static void Con_CloseChat( void ) { catcher &= ~KEYCATCH_MESSAGE; closes++; }
'''

CHECKS = r'''
static void Expect( int mode, bool open, int target, bool team ) {
	ToggleDMMessageConsole( mode );
	assert( ( ( catcher & KEYCATCH_MESSAGE ) != 0 ) == open );
	if ( open ) {
		assert( chat_dmTarget == target && ( openedTeam != 0 ) == team );
	}
}

int main() {
	/* all, team, a client; one message the same */
	Expect( 100, true, 0, false );
	Expect( 100, false, 0, false );		/* the same kind again closes it */
	Expect( 200, true, -1, true );
	Expect( 100, true, 0, false );		/* another kind switches */
	Expect( -200, true, -1, true );
	Expect( 7, true, 7, false );
	Expect( -64, true, 64, false );		/* 1 to 64, as the client game checks */
	/* 300 opens the kind last opened, and closes an open chat */
	Expect( 300, false, 0, false );
	Expect( 300, true, 64, false );
	/* what is refused leaves it as it was */
	const int before = opens + closes;
	Expect( 0, true, 64, false );
	Expect( 65, true, 64, false );
	Expect( 150, true, 64, false );
	Expect( -1000, true, 64, false );
	assert( opens + closes == before );
	puts( "chat ok" );
	return 0;
}
'''


class MohaaChatTests(unittest.TestCase):
    def test_the_client_games_chat_import_opens_the_chat_line(self):
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        source = HOST + 'int chatKind = 100;\n' + function(cgame, 'void QDECL ToggleDMMessageConsole( int mode ) {')
        output = build_and_run(self, 'chat', [source + CHECKS], cpp=True)
        self.assertIn('chat ok', output)

    def test_the_chat_line_speaks_the_games_own_command(self):
        keys = read('code/client/cl_keys.cpp')
        message = function(keys, 'static void Message_Key( int key ) {')
        self.assertIn('if ( Com_GameIsMOHAA() )\n\t\t\t\tCom_sprintf( buffer.data(), static_cast<int>( '
                      'buffer.size() ), "dmmessage %i %s\\n", chat_dmTarget, chatField.buffer );', message)
        console = read('code/client/cl_console.cpp')
        chat = function(console, 'static ChatCommandSet Con_ChatCommands( comGameId_t profile ) {')
        self.assertIn('case COM_GAME_MOHAA:\n\t\treturn { nullptr, 0 };', chat)
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        self.assertIn('SERVE( UI_ToggleDMMessageConsole, ToggleDMMessageConsole );', cgame)
        imports = read('code/compat/mohaa_cgame_imports.def')
        self.assertRegex(imports, r'FNTECH3_MOHAA_CGAME_IMPORT\(\s+159, UI_ToggleDMMessageConsole,\s+1,')


if __name__ == '__main__':
    unittest.main()
