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
            'ui_checkrestart': 'UiCheckRestart_f', 'maplist': 'MapList_f',
            'viewspawnlist': 'ViewSpawnList_f', 'soundpicker': 'SoundPicker_f', 'loadmenu': 'LoadMenu_f',
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

    def test_a_scrollbar_holds_and_repeats_as_uivertscroll_does(self):
        # UIVertScroll (0x004d8e10 down, 0x004d88b0 the step, 0x004d9200 up,
        # 0x004d9240 dragged): every scrolling list's bar is one.
        press = function(self.ui, 'void ScrollPress( ScrollHold &hold, float local, float height, int top, int page, int count ) {')
        assert_order(self, press, 'hold = ScrollHold();', 'if ( count <= page ) return;',
                     'hold.part = 1;', 'hold.rate = -1;', 'height - ScrollbarWidth < local', 'hold.rate = 1;',
                     'hold.part = 5;', 'hold.rate = -page;', 'hold.part = 3;',
                     'hold.grab = top - ScrollItemAt( local, height, count );', 'hold.topAtPress = top;', 'return;',
                     'hold.part = 4;', 'hold.rate = page;', 'hold.dueAt = Now();')
        repeat = function(self.ui, 'void ScrollRepeat( ScrollHold &hold, int top, Move move ) {')
        assert_order(self, repeat, 'if ( hold.dueAt < 0 || Now() + 1 < hold.dueAt ) return;',
                     'hold.dueAt = move( top + hold.rate ) ? Now() + 200 : -1;')
        drag = function(self.ui, 'void ScrollDrag( const ScrollHold &hold, float lx, float local, float height, int count, Move move ) {')
        assert_order(self, drag, 'if ( hold.part != 3 ) return;',
                     'if ( lx >= -24.0f && lx <= ScrollbarWidth + 24.0f ) move( ScrollItemAt( local, height, count ) + hold.grab );',
                     'else move( hold.topAtPress );')
        item = function(self.ui, 'int ScrollItemAt( float local, float height, int count ) {')
        self.assertIn('static_cast<int>( static_cast<float>( count ) * ( local - ScrollbarWidth ) / track )', item)
        # All three lists take their bar's press the same way.
        self.assertIn('ScrollPress( bind.hold, my - y, widget.h, bind.top, bind.page,',
                      function(self.ui, 'void BindListDown( Menu &menu, int index, float mx, float my ) {'))
        self.assertIn('ScrollPress( edit.hold, my - y, widget.h, edit.top, EditPage( widget ),',
                      function(self.ui, 'void EditDown( Menu &menu, int index, float mx, float my ) {'))
        saves = function(self.ui, 'void SaveListDown( Menu &menu, Widget &list, float mx, float my ) {')
        assert_order(self, saves, 'if ( mx >= x + list.w - ScrollbarWidth ) {',
                     'ScrollPress( list.load.hold, my - y, list.h, list.load.top, page, count );', 'return;')
        # The steps run before the mouse is read; a release anywhere ends a
        # hold; hover stops while one holds the mouse.
        frame = function(self.ui, 'void CL_AliceUIFrame( void ) {')
        assert_order(self, frame, 'ScrollRepeats( *menu );', 'PickerRepeats();', 'const bool wantCursor',
                     'ScrollReleases( *menu );', 'PickerMouseUp();')
        up = function(self.ui, 'void MouseUp( Menu &menu ) {')
        assert_order(self, up, 'held = held || HoldsMouse( widget );', 'ScrollReleases( menu );', 'if ( held ) return;')
        self.assertNotIn('scrollPart', self.ui)
        self.assertNotIn('thumbDrag', self.ui)
        hover = function(self.ui, 'void UpdateHover() {')
        assert_order(self, hover, 'if ( HoldsMouse( widget ) ) return;', 'PickerUnderCursor()')
        self.assertIn('widget.edit.hold = widget.bind.hold = widget.load.hold = ScrollHold();',
                      function(self.ui, 'void ShowMenu( Menu &menu, int delayMs ) {'))
        bar = function(self.ui, 'void DrawScrollbar( float barX, float y, float height, int top, int page, int count, int held ) {')
        assert_order(self, bar, 'white, arrow, 1.0f, 1.0f, 0.0f, 0.0f );', 'if ( thumb ) {',
                     'Box3D( barX, y, ScrollbarWidth, ScrollbarWidth, held == 1, border, 1.0f );',
                     'held == 2, border, 1.0f );')

    def test_the_alice3d_head_follows_the_cursor(self):
        # UIAlice3D (0x004d31db..0x004d3296): Bip01 Head on controller 0.
        draw = function(self.ui, 'void DrawAlice3D( Widget &widget ) {')
        assert_order(self, draw, 'alice.scale = 1.0f;', 'CL_AliceGetMouseState( &mx, &my, nullptr );',
                     '( 1.0 - static_cast<double>( my ) / cls.glconfig.vidHeight - 0.5 ) * -45.0',
                     '( 1.0 - static_cast<double>( mx ) / cls.glconfig.vidWidth - 0.2 ) * -45.0',
                     'AliceTiki_TagNumForName( handles.tiki, "Bip01 Head" )',
                     'fntech3::alice::state::reader::EulerToQuat( head, control.quat );',
                     'control.scale = 1.0f;', 'alice.useControllers = qtrue;', 're.AddRefEntityToScene( &entity, qfalse );')

    def test_the_developer_pickers(self):
        # maplist, viewspawnlist and soundpicker (0x0040d710, 0x0040d9f0,
        # 0x0040d9a0) open FilePickerClass windows (0x00417fd0).
        self.assertIn('OpenPicker( picker::Kind::MapRunner, "maps", folder, ".bsp" );', function(self.ui, 'void MapList_f() {'))
        self.assertIn('OpenPicker( picker::Kind::ViewSpawner, "models", "models", ".tik" );',
                      function(self.ui, 'void ViewSpawnList_f() {'))
        sound = function(self.ui, 'void SoundPicker_f() {')
        assert_order(self, sound, 'UiCvarString( "ui_pickedsound" )',
                     'OpenPicker( picker::Kind::SoundPicker, "sound", picker::StartFolder( picked, "sound/" ), ".*" );')
        choose = function(self.ui, 'bool PickerChoose( FilePicker &p, int row ) {')
        assert_order(self, choose, 'picker::RowType::Parent', 'picker::ParentOf( p.current, p.root )',
                     'picker::RowType::Folder', 'p.current += chosen.name + "/";',
                     'Cbuf_AddText( picker::ViewSpawnCommand( full ).c_str() );',
                     'picker::MapCommand( full )', 'UiCvarSet( "ui_pickedsound", full.c_str() );', 'return true;')
        select = function(self.ui, 'void PickerSelect( FilePicker &p, int row ) {')
        assert_order(self, select, 'if ( row == p.currentRow ) return;',
                     'p.kind == picker::Kind::SoundPicker', 'PlayLocalSound( p.current + chosen.name );')
        down = function(self.ui, 'bool PickerMouseDown() {')
        assert_order(self, down, 'ClosePicker( static_cast<std::size_t>( index ) );', 'p.minimized = !p.minimized;',
                     'p.titleClickTime = -20000;', 'p.dragging = true;', 'ScrollPress( p.hold, iy, lh, p.top, PickerPage( p ), count );',
                     'p.sizing = c;', 'picker::SortRows( p.rows, c, p.currentRow );', 'PickerSelect( p, row );',
                     'picker::DoubleClick(', 'p.clickTime = 0;')
        keys = function(self.ui, 'void KeyEvent( int key, bool down ) {')
        assert_order(self, keys, 'if ( key == K_MOUSE1 && PickerMouseUp() ) return;', 'EscapeKey();',
                     'if ( PickerMouseDown() ) return;', 'if ( PickerKey( key ) ) return;')
        assert_order(self, function(self.ui, 'void CL_AliceUIDrawMenus( void ) {'), 'DrawMenu( menu );', 'DrawPickers();',
                     'DrawPopup();')
        assert_order(self, function(self.ui, 'void Init() {'), 'LoadMenus();', 'RebuildPickers();', 'uiInitialized = true;')

    def test_loadmenu_reads_one_named_menu_file(self):
        # loadmenu (alice.exe 0x0040e0c0, fakk2.exe 1.02 0x0040cac0): ui/<name>
        # in 128 bytes, its extension replaced by .urc, read as every menu
        # file is; the lookup keeps finding the first menu of a name.
        body = function(self.ui, 'void LoadMenu_f() {')
        assert_order(self, body, 'char path[128];',
                     'Com_sprintf( path, sizeof( path ), "ui/%s", Cmd_Argv( 1 ) );',
                     'COM_StripExtension( path, path, sizeof( path ) );',
                     'Q_strcat( path, sizeof( path ), ".urc" );', 'const std::size_t first = menus.size();',
                     'LoadMenuFile( path );', 'for ( std::size_t i = first; i < menus.size(); ++i ) {',
                     'if ( widget.hasInitData ) UiCvarSet( widget.linkCvar, widget.initData.c_str() );')
        find = function(self.ui, 'int FindMenu( const char *name ) {')
        assert_order(self, find, 'for ( std::size_t i = 0; i < menus.size(); ++i ) {',
                     'if ( !Q_stricmp( menus[i].name.c_str(), name ) ) return static_cast<int>( i );')

    def test_a_video_restart_leaves_ritual_menus_alone(self):
        # vid_restart (alice.exe 0x0042b590, fakk2.exe 1.02 0x004212f0) never
        # shuts the engine UI down and unpauses the game outright.
        cl_ui = (ROOT / 'code/client/cl_ui.cpp').read_text(encoding='utf-8')
        assert_order(self, function(cl_ui, 'int CL_UIMenuForRestart( void ) {'),
                     'if ( !uivm || !( Key_GetCatcher() & KEYCATCH_UI ) ) return UIMENU_NONE;',
                     'if ( Com_GameIsRitual() ) return UIMENU_NONE;', 'if ( Com_GameIsRTCW() )')
        main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        assert_order(self, function(main, 'static void CL_Vid_Restart( refShutdownCode_t shutdownCode ) {'),
                     'Cvar_Set( "cl_paused", "0" );',
                     'if ( Com_GameIsRitual() && Cvar_VariableIntegerValue( "paused" ) ) {',
                     'Cvar_Set( "paused", "0" );', 'CL_ClearMemory();')


if __name__ == '__main__':
    unittest.main()
