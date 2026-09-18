"""RTCW SP ends its campaign through G_ENDGAME, as original SP does."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


def block(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first)]


class RTCWEndgameTests(unittest.TestCase):
    def test_only_sp_has_the_ordinal(self):
        # Original SP g_public.h numbers G_ENDGAME 2; MP has no such import, so
        # an MP module can never reach the case below.
        row = re.search(r'FNTECH3_RTCW_MODULE_CALL\(GAME, IMPORT, G_ENDGAME, (-?\d+), (-?\d+)\)',
                        read('code/compat/rtcw_module_calls.def'))
        self.assertEqual(row.groups(), ('2', '-1'))
        # The SP UI numbers its endgame menu 4 and MP has none.
        self.assertIn('FNTECH3_RTCW_MENU(ENDGAME, 4, -1)', read('code/compat/rtcw_menus.def'))

    def test_game_import_ends_the_frame_with_endgame(self):
        game = read('code/server/sv_game.cpp')
        case = block(game, 'case Call::G_ENDGAME:', 'case Call::G_CVAR_REGISTER:')
        self.assertIn('Com_Error( ERR_ENDGAME, "endgame" );', case)
        # It sits in the RTCW dispatcher, before the unsupported-import default.
        rtcw = game[game.index('case Call::G_ENDGAME:'):]
        self.assertLess(rtcw.index('Com_Error( ERR_ENDGAME'), rtcw.index('Unsupported %s game import'))

    def test_error_path_matches_original_sp(self):
        self.assertRegex(read('code/qcommon/q_shared.h'),
                         r'ERR_NEED_CD,[^\n]*\n\s*ERR_ENDGAME\s*//[^\n]*\n\}\s*errorParm_t;')
        common = read('code/qcommon/common.c')
        error = block(common, 'QDECL Com_Error( errorParm_t code', '\n}\n')
        # Not an error to report, stop a build script for, or keep as a message.
        self.assertIn('code != ERR_DISCONNECT && code != ERR_NEED_CD && code != ERR_ENDGAME', error)
        self.assertIn('com_buildScript->integer && code != ERR_ENDGAME', error)
        branch = block(error, '} else if ( code == ERR_ENDGAME ) {', '} else {')
        order = ['VM_Forced_Unload_Start();', 'SV_Shutdown( "endgame" );', 'CL_Disconnect( qtrue );',
                 'CL_FlushMemory();', 'CL_EndgameMenu();', 'VM_Forced_Unload_Done();',
                 'com_errorEntered = qfalse;', 'Q_longjmp( abortframe, 1 );']
        positions = [branch.index(step) for step in order]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn('Com_Printf', branch)

    def test_client_opens_the_endgame_menu_next_frame(self):
        client = read('code/client/cl_main.cpp')
        menu = block(client, 'void CL_EndgameMenu( void ) {', '\n}')
        self.assertIn('cls.endgamemenu = ToQboolean( Com_GameIsRTCWSP() != 0 );', menu)
        frame = block(client, 'void CL_Frame( int msec, int realMsec ) {', '\n}\n')
        opened = frame.index('VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_ENDGAME );')
        self.assertLess(frame.index('} else if ( cls.endgamemenu && uivm ) {'), opened)
        self.assertLess(opened, frame.index('CL_RestoreMainMenuAfterDisconnect();'))
        self.assertIn('qboolean	endgamemenu;', read('code/client/client.h'))
        self.assertIn('void CL_EndgameMenu( void );', read('code/qcommon/qcommon.h'))


if __name__ == '__main__':
    unittest.main()
