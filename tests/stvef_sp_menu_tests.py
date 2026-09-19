"""Elite Force SP's menus: efuix86.dll's function tables and the client's host.

Retail stvoy.exe calls GetUIAPI, copies the twelve exports, fills 59 imports
on its stack in a fixed order with the scissor slot left empty (CL_InitUI
0x40a4a0) and initializes the menus with API version 2. It opens menus by
name, asks the game whether a menu may open (SG_GameAllowedToSaveHere
0x4272e0), sends key presses only, and refreshes the menus every frame after
the connection screen (SCR_DrawScreenField 0x40a130). The checks pin the
tables, the menu names and the host's wiring; the ABI layout and render
record tests cover the records.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

IMPORTS = [
    'Printf', 'Error', 'Cvar_Set', 'Cvar_VariableValue', 'Cvar_VariableStringBuffer', 'Cvar_SetValue',
    'Cvar_Reset', 'Cvar_Create', 'Cvar_InfoStringBuffer', 'Argc', 'Argv', 'Cmd_ExecuteText',
    'Cmd_TokenizeString', 'FS_FOpenFile', 'FS_Read', 'FS_Write', 'FS_FCloseFile', 'FS_GetFileList',
    'FS_ReadFile', 'FS_FreeFile', 'R_RegisterModel', 'R_RegisterSkin', 'R_RegisterShader',
    'R_RegisterShaderNoMip', 'R_ClearScene', 'R_AddRefEntityToScene', 'R_AddPolyToScene',
    'R_AddLightToScene', 'R_RenderScene', 'R_SetColor', 'R_DrawStretchPic', 'R_ScissorPic', 'UpdateScreen',
    'PrecacheScreenshot', 'R_LerpTag', 'S_StartLocalSound', 'S_RegisterSound', 'S_StartLocalLoopingSound',
    'DrawStretchRaw', 'SG_GetSaveImage', 'SG_GetSaveGameComment', 'SG_ValidateForLoadSaveScreen',
    'SG_GameAllowedToSaveHere', 'SG_StoreSaveGameComment', 'SCR_GetScreenshot', 'Key_KeynumToStringBuf',
    'Key_GetBindingBuf', 'Key_SetBinding', 'Key_IsDown', 'Key_GetOverstrikeMode', 'Key_SetOverstrikeMode',
    'Key_ClearStates', 'Key_GetCatcher', 'Key_SetCatcher', 'GetClipboardData', 'GetGlconfig',
    'GetClientState', 'GetConfigString', 'Milliseconds',
]

EXPORTS = [
    'UI_Init', 'UI_Shutdown', 'UI_KeyEvent', 'UI_MouseEvent', 'UI_Refresh', 'UI_GetActiveMenu',
    'UI_SetActiveMenu', 'UI_ConsoleCommand', 'UI_DrawConnect', 'UI_DrawConnectText',
    'UI_UpdateConnectionString', 'UI_UpdateConnectionMessageString',
]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def table(source, name):
    body = source[source.index(f'typedef struct {name}_s'):source.index(f'}} {name}_t;')]
    return re.findall(r'\(\s*\*\s*(\w+)\s*\)\s*\(', body)


class STVEFSPMenuTests(unittest.TestCase):
    def test_tables_follow_retail_order(self):
        api = read('code/compat/stvef_sp_ui_api.h')
        self.assertEqual(table(api, 'fntech3_stvef_sp_ui_import'), IMPORTS)
        self.assertEqual(table(api, 'fntech3_stvef_sp_ui_export'), EXPORTS)
        self.assertIn('typedef fntech3_stvef_sp_ui_export_t *(*fntech3_stvef_sp_GetUIAPI_t)( void );', api)
        # Single player has no authorizing state: active is 7, cinematic 8.
        self.assertIn('FNTECH3_STVEF_SP_CA_ACTIVE = 7', api)
        self.assertIn('FNTECH3_STVEF_SP_CA_CINEMATIC = 8', api)

    def test_every_import_but_the_scissor_is_filled(self):
        host = read('code/client/client_stvef_sp_ui.hpp')
        fill = function(host, 'void CL_STVEFSPUIFillImports(')
        filled = re.findall(r'import\.(\w+) = ', fill)
        self.assertEqual(filled, IMPORTS)
        self.assertIn('import.R_ScissorPic = nullptr;', fill)
        # The menus ask the game before opening any menu.
        self.assertIn('return SV_STVEFSPGameAllowedToSaveHere( inCamera ? qtrue : qfalse );',
                      function(host, 'std::int32_t CL_STVEFSPUIAllowedToSave('))

    def test_menus_open_by_name(self):
        host = read('code/client/client_stvef_sp_ui.hpp')
        entry = function(host, 'intptr_t QDECL CL_STVEFSPUIEntry(')
        self.assertIn('return FNTECH3_STVEF_SP_UI_API_VERSION;', entry)
        self.assertIn('ui->UI_Init( FNTECH3_STVEF_SP_UI_API_VERSION, &stvefSpUIImport );', entry)
        self.assertIn('case UIMENU_NONE: ui->UI_SetActiveMenu( nullptr, nullptr ); break;', entry)
        self.assertIn('case UIMENU_INGAME: ui->UI_SetActiveMenu( "ingame", CL_STVEFSPOnHolodeck() ? "HOLO" : nullptr ); break;',
                      entry)
        self.assertIn('case UIMENU_NEED_CD: ui->UI_SetActiveMenu( "needcd", nullptr ); break;', entry)
        self.assertIn('default: ui->UI_SetActiveMenu( "main", nullptr ); break;', entry)
        # Full screen is the active menu's flag; the overlay is the text-only screen.
        self.assertIn('ui->UI_GetActiveMenu( nullptr, &fullscreen );', entry)
        self.assertIn('if ( arg[0] ) ui->UI_DrawConnectText( cls.servername, cls.updateInfoString );', entry)
        self.assertIn('else ui->UI_DrawConnect( cls.servername, cls.updateInfoString );', entry)
        self.assertIn('return !Q_stricmpn( Info_ValueForKey( serverinfo, "mapname" ), "_holo", 5 );',
                      function(host, 'bool CL_STVEFSPOnHolodeck()'))
        self.assertIn('"genericholomenu" : "genericmenu", Cmd_Argv( 1 )',
                      function(host, 'void CL_STVEFSPGenericMenu_f()'))
        self.assertIn('UI_SetActiveMenu( "endholomenu", Cmd_Argv( 1 ) )',
                      function(host, 'void CL_STVEFSPEndHolodeckMenu_f()'))
        load = function(host, 'vm_t *CL_STVEFSPLoadUI()')
        self.assertIn('VM_CreateTable( VM_UI, "GetUIAPI", CL_STVEFSPUIEntry, &symbol )', load)

    def test_client_states_skip_authorizing(self):
        host = read('code/client/client_stvef_sp_ui.hpp')
        states = function(host, 'std::int32_t CL_STVEFSPUIClientState()')
        pairs = re.findall(r'case (CA_\w+):(?: return FNTECH3_STVEF_SP_(CA_\w+);)?', states)
        self.assertEqual(pairs, [
            ('CA_UNINITIALIZED', 'CA_UNINITIALIZED'), ('CA_DISCONNECTED', 'CA_DISCONNECTED'),
            ('CA_AUTHORIZING', ''), ('CA_CONNECTING', 'CA_CONNECTING'), ('CA_CHALLENGING', 'CA_CHALLENGING'),
            ('CA_CONNECTED', 'CA_CONNECTED'), ('CA_LOADING', 'CA_LOADING'), ('CA_PRIMED', 'CA_PRIMED'),
            ('CA_ACTIVE', 'CA_ACTIVE'), ('CA_CINEMATIC', 'CA_CINEMATIC'),
        ])

    def test_menus_open_only_where_the_game_allows(self):
        game = read('code/server/sv_game.cpp')
        allowed = function(game, 'qboolean SV_STVEFSPGameAllowedToSaveHere( qboolean inCamera )')
        # Without inCamera: a running level, not a '_' map, a living player.
        order = ['if ( !inCamera )', 'sv.state != SS_GAME', "mapname[0] == '_'", 'player.stats[0] <= 0',
                 'if ( !stvefSpGame )', 'return inCamera;', 'stvefSpGame->GameAllowedToSaveHere()']
        positions = [allowed.index(text) for text in order]
        self.assertEqual(positions, sorted(positions))
        self.assertIn('qboolean SV_STVEFSPGameAllowedToSaveHere( qboolean inCamera );',
                      read('code/qcommon/qcommon.h'))

    def test_client_hosts_the_menus(self):
        ui = read('code/client/cl_ui.cpp')
        self.assertIn('#include "client_stvef_sp_ui.hpp"', ui)
        init = function(ui, 'void CL_InitUI(')
        self.assertIn('uivm = CL_STVEFSPLoadUI();', init)
        self.assertIn('Cmd_AddCommand( "genericmenu", CL_STVEFSPGenericMenu_f );', init)
        self.assertIn('Cmd_AddCommand( "endholodeckmenu", CL_STVEFSPEndHolodeckMenu_f );', init)
        shutdown = function(ui, 'void CL_ShutdownUI( void )')
        self.assertLess(shutdown.index('Cmd_RemoveCommand( "genericmenu" );'), shutdown.index('UI_SHUTDOWN'))
        self.assertIn('return !structured && version == FNTECH3_STVEF_SP_UI_API_VERSION;', ui)
        # Presses only, for both Elite Force modes.
        keys = read('code/client/cl_keys.cpp')
        self.assertIn('if ( sendKeyUp && !Com_GameIsSTVEF() )', keys)
        self.assertIn('if ( uivm && !Com_GameIsSTVEF() )', keys)
        self.assertNotIn('!Com_GameIsSTVEFMP()', keys)

    def test_connection_screen_gets_its_text(self):
        host = read('code/client/client_stvef_sp_ui.hpp')
        text = function(host, 'void CL_STVEFSPUIConnectionText( const char *text, bool message )')
        self.assertIn('stvefSpUI->UI_UpdateConnectionMessageString( copy );', text)
        self.assertIn('stvefSpUI->UI_UpdateConnectionString( copy );', text)
        self.assertIn('if ( uivm && Com_GameIsSTVEFSP() ) {',
                      function(read('code/client/cl_ui.cpp'), 'void CL_STVEFSPConnectionText('))
        main = read('code/client/cl_main.cpp')
        # The attempt count on every retransmission, and the server's message.
        self.assertIn('clc.connectPacketCount++;\n\tCL_STVEFSPConnectionText( va( "(%i)", clc.connectPacketCount ), qfalse );',
                      main.replace('\r\n', '\n'))
        self.assertIn('CL_STVEFSPConnectionText( s, qtrue );', function(main, 'static bool CL_ConnectionlessPacket('))
        # Cleared when the gamestate arrives.
        gamestate = function(read('code/client/cl_parse.cpp'), 'static void CL_ParseGamestate( msg_t *msg )')
        self.assertLess(gamestate.index('CL_STVEFSPConnectionText( "", qfalse );'), gamestate.index('CL_ClearState();'))

    def test_menus_refresh_every_frame_after_the_connection_screen(self):
        screen = function(read('code/client/cl_scrn.cpp'), 'static void SCR_DrawScreenField(')
        self.assertIn('const bool refreshEveryFrame = Com_GameIsSTVEFSP();', screen)
        for overlay in ('qfalse', 'qtrue'):
            draw = screen.index(f'VM_Call( uivm, 1, UI_DRAW_CONNECT_SCREEN, {overlay} );')
            guard = screen.rindex('if ( !refreshEveryFrame ) {', 0, draw)
            self.assertLess(draw - guard, 120)
        self.assertIn('if ( ( uiVisible || ( refreshEveryFrame && uivm ) ) && !browserSuppressUiRefresh )', screen)


if __name__ == '__main__':
    unittest.main()
