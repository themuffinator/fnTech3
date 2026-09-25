"""American McGee's Alice's engine-owned menus: the source contracts that the
pure tests (alice_menu_tests.cpp, alice_urc_tests.cpp) cannot reach.

Retail alice.exe: UI_Init registers the UI's commands once (0x004105fc..
0x0041080b) and never shuts the UI down; UI_Update keeps a three-state record
of the pause a full-screen menu took (0x0053162c, 0x0040c92e..0x0040ca15);
UI_EndLoad forgets it and unpauses (0x0040ecf0). The load/save list's
`savegame` event runs the save from inside the UI (0x0041bec0).
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def assert_order(case, text, *fragments):
    cursor = -1
    for fragment in fragments:
        found = text.find(fragment, cursor + 1)
        case.assertNotEqual(found, -1, f'missing {fragment!r} after offset {cursor}')
        cursor = found


def string_table(source, declaration):
    start = source.index(declaration)
    body = source[source.index('{', start) + 1:source.index('};', start)]
    return body


class AliceMenuSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / 'code/client/cl_alice_ui.cpp').read_text(encoding='utf-8')
        cls.alice = (ROOT / 'code/client/cl_alice.cpp').read_text(encoding='utf-8')

    def test_commands_are_registered_once_and_outlive_the_ui(self):
        names = re.findall(r'"([^"]+)"', string_table(self.ui, 'const char *const kCommands[] = {'))
        handlers = re.findall(r'(\w+_f)\b', string_table(self.ui, 'const xcommand_t kCommandHandlers[] = {'))
        self.assertEqual(len(names), len(handlers))
        pairs = dict(zip(names, handlers))
        self.assertEqual(pairs, {
            'pushmenu': 'PushMenu_f', 'forcemenu': 'ForceMenu_f', 'popmenu': 'PopMenu_f',
            'showmenu': 'ShowMenu_f', 'widgetcommand': 'WidgetCommand_f', 'listmenus': 'ListMenus_f',
            'centerprint': 'CenterPrint_f', 'locationprint': 'LocationPrint_f',
            'ui_resetcvars': 'UiResetCvars_f', 'ui_restorecvars': 'UiRestoreCvars_f',
            'ui_setcvars': 'UiSetCvars_f', 'ui_showmouse': 'UiShowMouse_f', 'ui_hidemouse': 'UiHideMouse_f',
            'ui_checkrestart': 'UiCheckRestart_f',
        })
        self.assertIn('static_assert( std::size( kCommands ) == std::size( kCommandHandlers )', self.ui)

        register = function(self.ui, 'void RegisterCommands() {')
        assert_order(self, register,
                     'if ( !commandsRegistered ) {',
                     'commandsRegistered = true;',
                     'Cmd_AddCommand( kCommands[i], kCommandHandlers[i] );',
                     'if ( !introCommandsRegistered ) {')
        # Nothing forgets that the commands are there, and nothing takes one away:
        # a command queued just before a map load or a video restart must run.
        self.assertNotIn('commandsRegistered = false', self.ui)
        self.assertNotIn('Cmd_RemoveCommand', self.ui)
        self.assertNotIn('Cmd_RemoveCommand', function(self.ui, 'void Shutdown() {'))
        self.assertIn('RegisterCommands();', function(self.ui, 'void Init() {'))

    def test_menu_commands_wait_for_a_ui_that_is_down(self):
        # With the UI down there are no menus: the stack commands queue for the
        # restart instead of reaching a menu table that is empty.
        for handler, call in (('void PushMenu_f() {', 'PushMenu( Cmd_Argv( 1 ), false );'),
                              ('void ForceMenu_f() {', 'PushMenu( Cmd_Argv( 1 ), true );'),
                              ('void ShowMenu_f() {', 'PushMenu( Cmd_Argv( 1 ), false );')):
            assert_order(self, function(self.ui, handler),
                         'if ( !uiInitialized ) { savedStack.emplace_back( Cmd_Argv( 1 ) ); return; }', call)
        assert_order(self, function(self.ui, 'void PopMenu_f() {'),
                     'if ( !uiInitialized ) {', 'savedStack.pop_back();', 'return;',
                     'PopMenu( atoi( Cmd_Argv( 1 ) ) != 0 );')
        # The rest act on the current menu, which is null while the UI is down.
        for handler in ('void UiResetCvars_f() {', 'void UiRestoreCvars_f() {', 'void UiSetCvars_f() {'):
            self.assertIn('if ( Menu *menu = CurrentMenu() )', function(self.ui, handler))
        self.assertIn('if ( CurrentMenu() ) viewHasMouse = true;', self.ui)

    def test_a_menu_pause_leaves_the_players_pause_alone(self):
        self.assertIn('int uiPauseState;', self.ui)
        self.assertNotIn('uiPausedGame', self.ui)
        release = function(self.ui, 'void ReleaseMenuPause() {')
        assert_order(self, release,
                     'if ( uiPauseState == 1 && Cvar_VariableIntegerValue( "paused" ) ) TogglePause();',
                     'uiPauseState = 0;')
        frame = function(self.ui, 'void CL_AliceUIFrame( void ) {')
        assert_order(self, frame,
                     'if ( !CL_AliceLoadingActive() ) {',
                     'if ( CL_AliceUIFullscreenMenuUp() ) {',
                     'if ( uiPauseState == 0 && cls.state == CA_ACTIVE ) {',
                     'if ( !Cvar_VariableIntegerValue( "paused" ) ) {',
                     'TogglePause();',
                     'uiPauseState = 1;',
                     'uiPauseState = 2;',
                     '} else if ( cls.state == CA_ACTIVE || cls.state == CA_CINEMATIC ) {',
                     'ReleaseMenuPause();')
        # A video restart takes the UI down: only the menu's own pause goes.
        shutdown = function(self.ui, 'void Shutdown() {')
        self.assertIn('ReleaseMenuPause();', shutdown)
        self.assertNotIn('CL_AliceUIReleasePause', shutdown)
        # UI_EndLoad unpauses whoever paused, and forgets the state.
        end_load = function(self.ui, 'void CL_AliceUIReleasePause( void ) {')
        assert_order(self, end_load, 'uiPauseState = 0;',
                     'if ( Cvar_VariableIntegerValue( "paused" ) ) TogglePause();')
        self.assertIn('CL_AliceUIReleasePause();', function(self.alice, 'void CL_AliceFirstSnapshot( void ) {'))

    def test_saves_and_loads_go_through_the_command_buffer(self):
        # The save runs from the buffer's front, never inside the UI's own
        # event, and its result is read on the next UI frame.
        self.assertNotIn('EXEC_NOW', self.ui)
        start = function(self.ui, 'void StartSave( Menu &menu, int index ) {')
        assert_order(self, start, 'pendingSave.active = true;', 'Cbuf_ExecuteText( EXEC_INSERT, "savegame\\n" );')
        finished = function(self.ui, 'void SaveFinished() {')
        assert_order(self, finished,
                     'if ( !pendingSave.active ) return;',
                     'pendingSave.active = false;',
                     'const int error = SV_AliceSaveError( qfalse );',
                     'ClearMenus( true );',
                     'SetWidgetShown( menu, "DiskFull", true );',
                     'SV_AliceSaveError( qtrue );')
        assert_order(self, function(self.ui, 'void CL_AliceUIFrame( void ) {'), 'SaveFinished();',
                     'const bool wantCursor')
        self.assertIn('Cbuf_AddText( va( "loadgame %s\\n", list.load.rows[index - 1].name.c_str() ) );',
                      function(self.ui, 'void LoadChosenSave( Widget &list ) {'))

    def test_urc_diagnostics_past_the_cap_are_counted(self):
        load = function(self.ui, 'void LoadMenuFile( const char *path ) {')
        assert_order(self, load, 'for ( const urc::Diagnostic &diagnostic : layout.diagnostics ) {',
                     'if ( layout.diagnosticsDropped ) {')


if __name__ == '__main__':
    unittest.main()
