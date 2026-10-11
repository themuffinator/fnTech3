"""Heavy Metal: F.A.K.K.2's HUD menus, inventory and weapon groups, and the
menu widgets its .urc files add: the source contracts the pure test
(fakk2_hud_tests.cpp) cannot reach.

Retail alice.exe, F.A.K.K.2's descendant, where fakk2.exe 1.02 was not read
(the patch's plain executable is cited by its own addresses where it was):
every `Label` is a UIFakkLabel (0x0040e240, Draw 0x0041ef00);
UI_Init reads the inventory script and registers its commands (0x004105f2,
0x004106cc..0x004107f2); `saveweaps` keeps a weapon group as a console alias
(0x0040ffe0), which the configuration writes back (0x004380a0); the open
inventory takes the keys (0x004091b0) and Escape closes it first
(0x0040d4e1). F.A.K.K.2's game module keeps its cinematic flags, additive fade
and letterbox in stats 14, 15 and 16 (fakk/gamex86.dll 0x100d135f,
0x100cc3a4, 0x100d1502), where Alice's are 5, 6 and 7.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def assert_order(case, text, *fragments):
    cursor = -1
    for fragment in fragments:
        found = text.find(fragment, cursor + 1)
        case.assertNotEqual(found, -1, f'missing {fragment!r} after offset {cursor}')
        cursor = found


class Fakk2UiSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = read('code/client/cl_alice_ui.cpp')
        cls.inventory = read('code/client/cl_fakk2_inventory.cpp')
        cls.header = read('code/client/client_alice_ui.hpp')
        cls.screen = read('code/client/cl_alice_screen.cpp')
        cls.hud = read('code/client/cl_alice_hud.cpp')

    def test_the_engine_reads_each_titles_own_stats(self):
        self.assertIn('return Com_GameIsFAKK2() ? fntech3::fakk2::hud::StatAddFade : 6;', self.header)
        self.assertIn('return Com_GameIsFAKK2() ? fntech3::fakk2::hud::StatLetterbox : 7;', self.header)
        self.assertIn('screen::FadeKind( Stats()[CL_RitualFadeStat()] )', self.screen)
        self.assertIn('screen::LetterboxFraction( Stats()[CL_RitualLetterboxStat()] )', self.screen)
        self.assertNotRegex(self.screen, r'Stats\(\)\[[0-9]+\]')
        self.assertNotIn('block[0][5]', self.ui + self.hud)

    def test_the_hud_is_fakk2s_menus_behind_its_own_gate(self):
        # fakk2.exe 1.02's UI update (0x0040b29b..0x0040b326): ui_hud, in the
        # game, no letterbox or menu -- not Alice's cinematic flag, and no delay.
        frame = function(self.hud, 'void CL_AliceHudFrame( bool allowed ) {')
        assert_order(self, frame,
                     'allowed = allowed && hudEnabled && hudEnabled->integer;',
                     'if ( Com_GameIsFAKK2() ) {',
                     'CL_RitualHudMenusFrame( allowed );',
                     'allowed = false;',
                     'allowed = allowed && !CL_RitualInCinematic();')
        menus = function(self.ui, 'void CL_RitualHudMenusFrame( bool allowed ) {')
        assert_order(self, menus,
                     'if ( !allowed ) {',
                     'set( fakkhud::HealthHud, false, true );',
                     'const fakkhud::Selection shown = fakkhud::Select( cl.snap.ps.alice.block[0], cl.snap.ps.alice.block[1] );',
                     'set( fakkhud::BossHud, shown.boss, false );')
        self.assertNotIn('HudShowDelayMs', menus)
        self.assertNotIn('CL_RitualInCinematic', menus)
        # HUD menus are shown, never pushed: the stack and its input stay the menus'.
        self.assertNotIn('PushMenu', menus)
        draw = function(self.ui, 'void CL_AliceUIDrawMenus( void ) {')
        assert_order(self, draw, 'if ( menu.hud ) DrawMenu( menu );', 'CL_Fakk2InventoryDraw();',
                     'if ( menu.hud ) continue;', 'DrawMenu( menu );', 'DrawPopup();')

    def test_only_the_hud_menus_take_the_hud_aspect_rules(self):
        # FnQuake3's HUD correction places the four HUD menus (AGENTS.md
        # invariant 11); every other menu and the inventory stay stretched.
        draw = function(self.ui, 'void CL_AliceUIDrawMenus( void ) {')
        assert_order(self, draw, 'CL_HudBeginFrame();', 'CL_RitualSetHudLayout( true );',
                     'if ( menu.hud ) DrawMenu( menu );', 'CL_RitualSetHudLayout( false );', 'CL_HudEndFrame();',
                     'CL_Fakk2InventoryDraw();', 'if ( menu.hud ) continue;')
        self.assertEqual(self.ui.count('CL_RitualSetHudLayout( true );'), 1)
        # A quad is cut in the stretched screen first, then placed.
        stretch = function(read('code/client/cl_alice_font.cpp'),
                           'void CL_RitualStretchPic( float x, float y, float w, float h, float s0, float t0, float s1, float t1,')
        assert_order(self, stretch, 'if ( clipOn && !ClipQuad( x, y, w, h, s0, t0, s1, t1 ) ) {', 'if ( hudLayout ) {',
                     'CL_HudRegisterShaderName( shader, HudShaderName( shader ).c_str() );',
                     'if ( !CL_HudAdjustPic( &x, &y, &w, &h, shader ) ) return;',
                     're.DrawStretchPic( x, y, w, h, s0, t0, s1, t1, shader );')
        # A hand's model view follows its ring; the inventory's does not.
        model = function(self.inventory, 'void CL_Fakk2RenderModel( float x, float y, float w, float h, const char *model, const char *anim,')
        assert_order(self, model, 'refdef.rdflags = RDF_NOWORLDMODEL;',
                     'if ( CL_RitualHudLayout() ) CL_HudAdjustRefdef( &refdef );', 're.RenderScene( &refdef );')
        hud = read('code/client/cl_hud.cpp')
        supported = ' '.join(hud.split('\n\thudSupported = ', 1)[1].split(';', 1)[0].split())
        self.assertIn('Com_GameIsRitual()', supported)
        allowed = ' '.join(read('code/qcommon/files.c').split('static qboolean FS_RootArchiveAllowsFile(', 1)[1]
                           .split('\n}', 1)[0].split())
        self.assertIn('( ( Com_GameIsQuake3() || Com_GameIsRTCW() || Com_GameIsWolfET() || Com_GameIsSTVEF() '
                      '|| Com_GameIsRitual() ) &&', allowed)

    def test_every_label_is_a_fakk_label(self):
        widget = function(self.ui, 'void DrawWidget( Menu &menu, int index, float parentAlpha ) {')
        self.assertIn('if ( !DrawFakkLabel( widget, vx, vy ) ) DrawWidgetText( menu, widget, vx, vy, alpha, pressed );',
                      widget)
        label = function(self.ui, 'bool DrawFakkLabel( Widget &widget, float x, float y ) {')
        assert_order(self, label, 'if ( label.stat != -1 ) {', 'if ( label.item != -1 ) {',
                     'if ( label.model != -1 ) {', 'return false;')
        # Its keywords are a Label's alone, as UIFakkLabel's responses are.
        for keyword in ('playerstat', 'maxplayerstat', 'itemstat', 'invmodelrender', 'statbar',
                        'statbar_shader', 'statbar_tileshader', 'statbar_shader_flash',
                        'statbar_tileshader_flash', 'linkstring'):
            self.assertIn(f'widget.kind == WidgetKind::Label && is( "{keyword}" )', self.ui)

    def test_inventory_commands_are_ritual_and_registered_once(self):
        # Both engines' UI_Init load the script and register these (alice.exe
        # 0x004105f2, 0x004106cc..0x00410715, 0x004107f7; fakk2.exe 1.02
        # 0x0040ea82); only fakk2.exe registers ui_hud as a command.
        init = function(self.inventory, 'void CL_Fakk2InventoryInit( void ) {')
        assert_order(self, init, 'if ( !Com_GameIsRitual() ) return;', 'if ( !commandsRegistered ) {',
                     'Cmd_AddCommand( "inventory", Inventory_f );', 'Cmd_AddCommand( "warpinv", WarpInv_f );',
                     'Cmd_AddCommand( "nextinv", NextInv_f );', 'Cmd_AddCommand( "previnv", PrevInv_f );',
                     'Cmd_AddCommand( "inv_restart", InvRestart_f );', 'Cmd_AddCommand( "saveweaps", SaveWeaps_f );',
                     'if ( Com_GameIsFAKK2() ) Cmd_AddCommand( "ui_hud", UiHud_f );', 'LoadInventory();')
        self.assertNotIn('Cmd_RemoveCommand', self.inventory)
        self.assertIn('CL_Fakk2InventoryInit();', function(self.ui, 'void Init() {'))
        # The alias the group becomes, and the centre print.
        save = function(self.inventory, 'void SaveWeaps_f() {')
        assert_order(self, save, 'Cmd_SetAlias( va( "weapgroup%s", Cmd_Argv( 1 ) ), command.c_str() );',
                     'CL_AliceSetCenterPrint( va( "Saved Weapon Group %d", atoi( Cmd_Argv( 1 ) ) ), 1.0f );')

    def test_each_cell_draws_its_gauge_under_the_art_and_the_model(self):
        # FakkItemList::Draw (fakk2.exe 1.02 0x00409950): the gauge, the cell
        # art, a selected picture that is not on top, the model, then one that is.
        draw = function(self.inventory, 'void CL_Fakk2InventoryDraw( void ) {')
        assert_order(self, draw, 'DrawGauge( item, list.x, y, w, h, rowY );',
                     'DrawPic( list.x, y, w, h, item.bgShader );',
                     '!item.selectedOnTop ) DrawPic( list.x, y, w, h, item.selectedShader );',
                     'CL_Fakk2RenderModel(', 'item.selectedOnTop ) DrawPic( list.x, y, w, h, item.selectedShader );')
        gauge = function(self.inventory,
                         'void DrawGauge( const inv::Item &item, float x, float y, float w, float h, float rowY ) {')
        assert_order(self, gauge, 'AmmoCount( item.ammo, amount, max );', 're.GetShaderImageSize( handle, &imageW, &imageH );',
                     'inv::AmmoGauge( item, cell, rowY, amount, max,', 'if ( !gauge.draw ) return;',
                     'gauge.s0, gauge.t0, gauge.s1, gauge.t1, handle );')

    def test_a_shown_save_list_draws_and_chooses_the_save(self):
        # F.A.K.K.2's load menu shows its list; Alice's is neverdraw.
        self.assertIn('case WidgetKind::LoadGame:\n\t\tDrawSaveList( widget, vx, vy, alpha );', self.ui)
        self.assertIn('case WidgetKind::LoadGame:\n\t\t// Only a list the menu shows (F.A.K.K.2\'s) takes the mouse.\n'
                      '\t\treturn !widget.neverDraw;', self.ui)
        draw = function(self.ui, 'void DrawSaveList( Widget &list, float x, float y, float alpha ) {')
        assert_order(self, draw, 'mw::SaveColumnTitles[column]', 'const bool chosen = row + 1 == list.load.current;',
                     'mw::ElapsedText( save.levelTime )', 'SaveDate( save.saveTime )')
        choose = function(self.ui, 'void ChooseSaveRow( Widget &list, int row ) {')
        self.assertIn('mw::SelectSaveRow( list.load.page, row );', choose)
        refresh = function(self.ui, 'void RefreshSaves( Menu &menu, Widget &list ) {')
        assert_order(self, refresh, 'mw::SortSaves( list.load.rows );', 'ShowSaves( menu, list );',
                     'FollowSavePage( list );')
        self.assertIn('FindWidget( menu, "LoadSaveShot" )', function(self.ui, 'void ShowSaves( Menu &menu, Widget &list ) {'))

    def test_the_open_inventory_takes_the_keys_and_escape_closes_it_first(self):
        key = function(self.ui, 'void KeyEvent( int key, bool down ) {')
        assert_order(self, key, 'if ( menuStack.empty() && CL_Fakk2InventoryIsOpen() ) {',
                     'CL_Fakk2InventoryKey( key );', 'Menu *menu = CurrentMenu();')
        escape = function(self.ui, 'void MenuEscape( const char *name ) {')
        assert_order(self, escape, 'if ( CL_Fakk2InventoryClose() ) return;', 'Menu *current = CurrentMenu();')
        frame = function(self.ui, 'void CL_AliceUIFrame( void ) {')
        want = frame[frame.index('const bool wantKeys'):frame.index(';', frame.index('const bool wantKeys'))]
        self.assertIn('CL_Fakk2InventoryIsOpen()', want)

    def test_fakk2s_intro_escape_and_back_to_game(self):
        # fakk2.exe 1.02: its own intro table (_intro 0x0040e0b0), BACK TO GAME
        # disabled outside a game by the intro's last step (0x0040e295) and by
        # the escape (0x0040c032..0x0040c0e9), and an escape with no intro block.
        step = function(self.ui, 'void RunIntroStep() {')
        self.assertIn('Com_GameIsFAKK2() ? screen::Fakk2Intro( introState ) : screen::Intro( introState )', step)
        self.assertNotIn('introState = 100', step)
        assert_order(self, step, 'case screen::IntroAction::MainMenu:', 'PushMenu( "main", true );',
                     'if ( Com_GameIsFAKK2() && cls.state < CA_ACTIVE && CurrentMenu() ) {',
                     'SetWidgetShown( *CurrentMenu(), "backtogame", false );',
                     'if ( step.soundtrack ) S_AliceMusicNewSoundtrack( CL_RitualMenuSoundtrack() );')
        assert_order(self, step, 'introPlate.image = step.plateImage;', 'introPlate.scale = step.plateScale;')
        escape = function(self.ui, 'void MenuEscape( const char *name ) {')
        assert_order(self, escape, 'PushMenu( name, false );',
                     'if ( Com_GameIsFAKK2() && CurrentMenu() && !Q_stricmp( name, "main" ) ) {',
                     'SetWidgetShown( *CurrentMenu(), "backtogame", cls.state >= CA_ACTIVE );')
        key = function(self.ui, 'void EscapeKey() {')
        assert_order(self, key, 'if ( !fakk2 && EscapeBlockedByIntro() ) return;',
                     'if ( Now() - lastEscapeTime < 1000 && ( fakk2 || cls.state != CA_CINEMATIC ) ) {',
                     'if ( cls.state == CA_CINEMATIC && !fakk2 ) {')

    def test_fakk2s_buttons_stay_lit_while_they_have_the_keys(self):
        # fakk2.exe 1.02: Display draws the hover picture while the widget's
        # flag is set (0x0049f103..0x0049f135); UIButtonBase's mouse exit is
        # empty (0x00404200); the window manager moves the flag with the keys
        # (0x00498c10); each change of menus gives the keys to the menu's
        # current order (0x004a40d0 -> 0x0049f8d0 -> 0x0049f900).
        lights = function(self.ui, 'bool LightsFollowKeys( const Widget &widget ) {')
        self.assertIn('return Com_GameIsFAKK2() && widget.kind == WidgetKind::Button;', lights)
        activate = function(self.ui, 'void Activate( Menu &menu, int index ) {')
        assert_order(self, activate, 'if ( menu.focus == index ) return;',
                     'if ( menu.focus >= 0 ) menu.widgets[menu.focus].lit = false;', 'menu.focus = index;',
                     'menu.widgets[index].lit = true;', 'menu.currentOrder = menu.widgets[index].orderNumber;')
        order = function(self.ui, 'void ActivateCurrentOrder( Menu &menu ) {')
        assert_order(self, order, 'if ( !menu.currentOrder ) menu.currentOrder = 1;',
                     'if ( widget.orderNumber == order && widget.shown && widget.enabled ) {',
                     'Activate( menu, static_cast<int>( i ) );', 'widget.lit = false;')
        show = function(self.ui, 'void ShowMenu( Menu &menu, int delayMs ) {')
        assert_order(self, show, 'menu.focus = -1;', 'UpdateUIElement( menu, static_cast<int>( i ) );',
                     'if ( Com_GameIsFAKK2() ) ActivateCurrentOrder( menu );')
        draw = function(self.ui, 'void DrawWidget( Menu &menu, int index, float parentAlpha ) {')
        self.assertIn('if ( ( LightsFollowKeys( widget ) ? widget.lit : hovered ) && !widget.hoverShader.empty() ) {',
                      draw)
        # A button is lit when the mouse comes onto it or presses it, and
        # stays lit when the mouse goes; letting go off it puts it out.
        enter = function(self.ui, 'void MouseEnter( Menu &menu, int index ) {')
        assert_order(self, enter, 'case WidgetKind::Button:', 'widget.lit = true;', 'Activate( menu, index );')
        self.assertNotIn('.lit', function(self.ui, 'void MouseExit( Menu &menu, int index ) {'))
        down = function(self.ui, 'void MouseDown( Menu &menu ) {')
        assert_order(self, down, 'Activate( menu, index );', 'menu.pressed = index;', 'widget.lit = true;')
        up = function(self.ui, 'void MouseUp( Menu &menu ) {')
        assert_order(self, up, 'if ( under == pressed ) {', 'Click( widget );',
                     '} else if ( LightsFollowKeys( widget ) ) {', 'widget.lit = false;')
        self.assertIn('Activate( menu, ordered[position] );', function(self.ui, 'void MoveFocus( Menu &menu, int step ) {'))

    def test_fakk2s_bind_list(self):
        # UIFakkBindList (fakk2.exe 1.02 children 0x004c0390, Draw 0x004c1360,
        # KeyEvent 0x004c0de0): three columns of pictures, two key cells.
        columns = function(self.ui, 'void Fakk2BindColumns( const mw::BindLayout &layout, float xs[4], float ws[4], float hs[4] ) {')
        assert_order(self, columns, 'xs[2] = cw + fw;', 'xs[3] = cw + 2.0f * fw + pw;')
        draw = function(self.ui, 'void DrawFakk2BindList( Widget &widget, float x, float y, float alpha ) {')
        assert_order(self, draw, 'CL_AliceFontRegister( "verdana-12" )', 'layout.headerMaterials[c - 1]',
                     'Fakk2BindPicture( fx, y, fw, widget.h, white, grey, std::string(), alpha );',
                     'const float rowY = ch * static_cast<float>( row - bind.top + 1 );',
                     'fg = bind.capturing ? layout.activeForeground : layout.selectForeground;',
                     'const char *name = key >= 0 ? Key_KeynumToString( key ) : "None";',
                     'std::string( "textures/bind/" ) + name : std::string( "textures/bind/emptykey" )',
                     'DrawScrollbar( x + widget.w - ScrollbarWidth, y, widget.h, bind.top, bind.page, count, bind.hold.part );')
        self.assertIn('if ( Com_GameIsFAKK2() ) DrawFakk2BindList( widget, vx, vy, alpha );', self.ui)
        keys = function(self.ui, 'bool BindListKey( Menu &menu, int index, int key ) {')
        assert_order(self, keys, 'if ( !fakk2 || bind.column >= 2 ) StartCapture( menu, index );',
                     'ClearRowKey( bind, bind.row, bind.column == 3 );',
                     'bind.column + 1 > ( fakk2 ? 3 : 2 )', 'bind.column - 1 < ( fakk2 ? 2 : 1 )')
        capture = function(self.ui, 'void CaptureKey( int key ) {')
        assert_order(self, capture, 'if ( Com_GameIsFAKK2() && bind.column == 3 ) BindRowAlternate( bind, bind.row, key );',
                     'else BindRowKey( bind, bind.row, key );')
        alternate = function(self.ui, 'void BindRowAlternate( BindState &bind, int row, int key ) {')
        assert_order(self, alternate, 'if ( second == key ) return;', 'if ( second >= 0 ) Key_SetBinding( second, "" );',
                     'if ( first == key ) {', 'second = key;', 'Key_SetBinding( key, command.c_str() );')
        down = function(self.ui, 'void BindListDown( Menu &menu, int index, float mx, float my ) {')
        assert_order(self, down, 'if ( Com_GameIsFAKK2() ) {', 'if ( column == 1 ) {',
                     'PlayLocalSound( bind.layout.changeSound );', '} else if ( row == bind.row && column == bind.column ) {',
                     'StartCapture( menu, index );')
        self.assertIn('if ( Com_GameIsFAKK2() ) bind.column = 2;', function(self.ui, 'void BuildBindList( Menu &menu, int index ) {'))
        self.assertIn('"textures/menu/presskey"', function(self.ui, 'void DrawPressAnyKey( const Widget &widget ) {'))

    def test_fakk2s_scrollbar_and_save_list(self):
        # UIVertScroll (fakk2.exe 1.02 0x004b3ab0): black-edged light grey, the
        # arrows bevelled and white only while there is something to scroll.
        bar = function(self.ui, 'void DrawScrollbar( float barX, float y, float height, int top, int page, int count, int held ) {')
        assert_order(self, bar, 'if ( bar && arrow ) {', 'return;',
                     'EdgedBox( barX, y, ScrollbarWidth, height, grey );',
                     'if ( thumb ) Bevel( barX + 1.0f, ay + 1.0f, ScrollbarWidth - 2.0f, ScrollbarWidth - 2.0f, bevel );',
                     'thumb ? white : greyed',
                     'EdgedBox( barX, y + offset, ScrollbarWidth, length, thumbGrey );')
        edged = function(self.ui, 'void EdgedBox( float x, float y, float w, float h, const float *fill, const float *edge = nullptr ) {')
        assert_order(self, edged, 'VirtualFill( x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fill );',
                     'VirtualFill( x, y, 1.0f, h - 1.0f, line );', 'VirtualFill( x, y + h - 1.0f, w - 1.0f, 1.0f, line );')
        # The list's headers 0.3 grey edged white with white titles (0x004ac8e0);
        # its scrollbar always drawn; the shot or no_saved_games (0x00415390).
        saves = function(self.ui, 'void DrawSaveList( Widget &list, float x, float y, float alpha ) {')
        assert_order(self, saves, 'const float headerFill[4] = { 0.3f, 0.3f, 0.3f, alpha };',
                     'EdgedBox( x + cx, y, w, header, headerFill, white );',
                     'DrawScrollbar( x + list.w - ScrollbarWidth, y, list.h, list.load.top, page, count, list.load.hold.part );')
        self.assertNotIn('if ( count > page ) DrawScrollbar', saves)
        shown = function(self.ui, 'void ShowSaves( Menu &menu, Widget &list ) {')
        self.assertIn('std::string shot = Com_GameIsFAKK2() ? "textures/menu/no_saved_games.tga" : "";', shown)

    def test_fakk2s_slider(self):
        # UISlider (fakk2.exe 1.02 0x004b4aa0): arrows 15 wide, a thumb 10 wide,
        # and its six pictures, which the shader commands replace.
        setup = function(self.ui, 'void LoadMenuFile( const char *path ) {')
        assert_order(self, setup, 'if ( widget.kind == WidgetKind::Slider && Com_GameIsFAKK2() ) {',
                     'widget.slider.s.arrowWidth = 15;', 'widget.slider.s.thumbWidth = 10.0f;',
                     'widget.slider.thumb = "textures/menu/slider_thumb";',
                     'widget.slider.rightSel = "textures/menu/arrow_right_sel";')
        self.assertIn('lit = args.size() > 1 ? args[1] : first;', self.ui)
        # Draw (0x004b64e0): the thumb, then each arrow, in the colour the
        # widget's own draw left; one name for both pictures makes a button.
        draw = function(self.ui, 'void DrawSlider( Menu &menu, int index, float x, float y, const float *inherited, float alpha ) {')
        assert_order(self, draw, 'if ( Com_GameIsFAKK2() ) {',
                     'float pen[4] = { inherited[0], inherited[1], inherited[2], inherited[3] };',
                     'widget.slider.dragging ? widget.slider.thumbSel : widget.slider.thumb',
                     'if ( normal != lit ) {', 'held ? lit : normal',
                     'Box3D( ax, y, arrow, widget.h, held, widget.border3d, alpha );',
                     'VirtualPic( ax + 2.0f, y + 2.0f, arrow - 4.0f, widget.h - 4.0f, pen, CL_AliceShader( normal.c_str() ) );',
                     'drawArrow( x, widget.slider.leftPressed, widget.slider.left, widget.slider.leftSel );',
                     'drawArrow( x + widget.w - arrow, widget.slider.rightPressed, widget.slider.right, widget.slider.rightSel );')
        # UIWidget::Display (0x0049ef30): white, the fill's, the border's light
        # strip, then the foreground under a shader.
        widget = function(self.ui, 'void DrawWidget( Menu &menu, int index, float parentAlpha ) {')
        assert_order(self, widget, 'float inherited[4] = { 1.0f, 1.0f, 1.0f, alpha };',
                     'std::copy( bg, bg + 4, inherited );', 'if ( borderStyle == 1 || borderStyle == 2 ) {',
                     'inherited[3] = light.a * alpha;',
                     'if ( !widget.shader.empty() ) std::copy( shade, shade + 4, inherited );',
                     'DrawSlider( menu, index, vx, vy, inherited, alpha );')
        # The arrows hold the value to min, then max (0x004b5a20, 0x004b5a70).
        step = function(self.ui, 'void SliderStep( Widget &widget, int direction ) {')
        self.assertIn('Com_GameIsFAKK2() ? mw::SteppedFakk2( widget.slider.s, direction )', step)

    def test_fakk2_cuts_each_widget_to_its_menu(self):
        # Each widget's 2D window is scissored to it cut to its menu
        # (0x0049ee80; 0x0049ec21..0x0049eceb), and the UI's quads are cut alike.
        widget = function(self.ui, 'void DrawWidget( Menu &menu, int index, float parentAlpha ) {')
        assert_order(self, widget, 'WidgetPosition( menu, widget, vx, vy );',
                     '~ClipScope() { if ( on ) CL_RitualSetClip( nullptr ); }', 'if ( Com_GameIsFAKK2() ) {',
                     'MenuPosition( menu, mx, my );',
                     'const float right = std::min( { vx + widget.w, mx + menu.w, 640.0f } );',
                     'CL_RitualSetClip( clip );', 'switch ( widget.kind ) {')
        self.assertIn('CL_RitualStretchPic( x * sx, y * sy, w * sx, h * sy, s0, t0, s1, t1, shader );',
                      function(self.ui, 'void VirtualPic( float x, float y, float w, float h, const float *rgba, qhandle_t shader,'))
        font = read('code/client/cl_alice_font.cpp')
        stretch = function(font, 'void CL_RitualStretchPic( float x, float y, float w, float h, float s0, float t0, float s1, float t1,')
        assert_order(self, stretch, 'if ( clipOn && !ClipQuad( x, y, w, h, s0, t0, s1, t1 ) ) {', 'return;',
                     're.DrawStretchPic( x, y, w, h, s0, t0, s1, t1, shader );')
        for helper in ('void CL_AliceFill( float x, float y, float w, float h, const float *rgba ) {',
                       'void CL_AliceDrawPic( float x, float y, float w, float h, const float *rgba, qhandle_t shader ) {',
                       'void DrawQuad( const font::Quad &quad, qhandle_t shader, bool rotated ) {'):
            with self.subTest(helper=helper):
                self.assertIn('CL_RitualStretchPic(', function(font, helper))
        self.assertNotIn('re.DrawStretchPic', self.ui)
        # The popups and the press-a-key picture are windows of their own.
        menu = function(self.ui, 'void DrawMenu( Menu &menu, float keptAlpha = -1.0f, bool kept = false ) {')
        assert_order(self, menu, 'DrawWidget( menu, static_cast<int>( i ), fade );', 'DrawPressAnyKey( widget );')

    def test_fakk2s_screen_keeps_its_image_under_full_screen_menus(self):
        # fakk2.exe 1.02's UI update clears nothing while a full-screen menu is
        # up (0x0040b0cf..0x0040b296): the image is kept as shares, started from
        # the game's view or black, and only F.A.K.K.2 keeps one.
        update = function(self.screen, 'void CL_AliceUpdateScreenField( stereoFrame_t stereo ) {')
        assert_order(self, update, 'if ( CL_AliceLoadingActive() ) {', 'CL_RitualEndMenuCarry();',
                     'if ( CL_AliceUIFullscreenMenuUp() ) {', 'CL_AliceClearBlack();',
                     'if ( CL_RitualMenuCarry( viewShownLast ) ) {', 'DrawView( stereo, false );',
                     '} else {', 'CL_RitualEndMenuCarry();', 'viewShownLast = showView;')
        # That is retail's presentation, so it is an option, off by default
        # (AGENTS.md invariant 11): fnTech3 fades the menus over black.
        carry = function(self.ui, 'bool CL_RitualMenuCarry( bool viewShown ) {')
        assert_order(self, carry,
                     'if ( !Com_GameIsFAKK2() || !uiInitialized || !ui_menuDissolve || !ui_menuDissolve->integer ) {',
                     'carry.active = false;', 'return false;',
                     'PlanCarry( viewShown );', 'if ( layer.kind == KeptView ) return true;')
        self.assertIn('ui_menuDissolve = Cvar_Get( "ui_menuDissolve", "0", CVAR_ARCHIVE );',
                      function(self.ui, 'void RegisterCvars() {'))
        plan = function(self.ui, 'void PlanCarry( bool viewShown ) {')
        assert_order(self, plan, 'start.kind = viewShown ? KeptView : KeptBlack;',
                     'if ( carry.frame == cls.framecount ) return;',
                     'mw::KeepDraw( carry.layers, KeptFill, index, menu.bg[3] * menu.bg[3] * fade );',
                     'mw::KeepDraw( carry.layers, KeptArt, index, fade );', 'mw::DropFaintLayers( carry.layers );')
        draws = function(self.ui, 'void CL_AliceUIDrawMenus( void ) {')
        assert_order(self, draws, 'if ( carry.active ) DrawKept();', 'if ( carry.active && menu.fullscreen ) {',
                     'DrawPressAnyKey( widget );', 'continue;', 'DrawMenu( menu );')
        kept = function(self.ui, 'void DrawKept() {')
        assert_order(self, kept, 'mw::KeptAlphas( carry.layers )',
                     'if ( layer.kind == KeptBlack || layer.kind == KeptView ) continue;',
                     'VirtualFill( x, y, menu.w, menu.h, fill );', 'DrawMenu( menu, alpha[i], true );')
        # A kept menu's pictures alone, whether it shows or not; its fill is its own layer.
        menu = function(self.ui, 'void DrawMenu( Menu &menu, float keptAlpha = -1.0f, bool kept = false ) {')
        assert_order(self, menu, 'if ( !kept && ( !menu.visible || Now() < menu.showStart ) ) return;',
                     'const float fade = kept ? keptAlpha : MenuFade( menu );',
                     'if ( !kept && menu.bg[3] != 0.0f ) {', 'if ( kept ) return;')
        # A gone menu's kept pictures fire none of its waits.
        widget = function(self.ui, 'void DrawWidget( Menu &menu, int index, float parentAlpha ) {')
        self.assertIn('if ( showing && wait.fireAt >= 0 && Now() >= wait.fireAt ) {', widget)

    def test_fakk2s_save_list_keys_and_double_click(self):
        # UIListCtrl (fakk2.exe 1.02): Page Up and Page Down go to the shown
        # page's edge first (0x004afa57, 0x004afa83); the wheel scrolls two rows
        # (0x004af9e3, 0x004afa05); the same row twice within 500 ms and two
        # units loads it (0x004ad497..0x004ad4fe -> loadgame 0x00415660).
        keys = function(self.ui, 'bool LoadGameKey( Menu &menu, Widget &list, int key ) {')
        assert_order(self, keys, 'case K_LEFTARROW:', 'case K_PGUP:',
                     'else if ( key == K_PGUP ) row = row - 1 != top ? top + 1 : row - page;',
                     'else if ( key == K_PGDN ) row = row != top + page ? top + page : row + page;',
                     'case K_MWHEELUP:', 'list.load.top + ( key == K_MWHEELUP ? -2 : 2 )')
        down = function(self.ui, 'void SaveListDown( Menu &menu, Widget &list, float mx, float my ) {')
        assert_order(self, down, 'ChooseSaveRow( list, row );', 'ShowSaves( menu, list );',
                     'Now() < list.load.clickTime + 500 && list.load.current == list.load.clickRow',
                     'std::fabs( list.load.clickX - mx ) <= 2.0f && std::fabs( list.load.clickY - my ) <= 2.0f',
                     'list.load.clickTime = Now();', 'if ( twice ) LoadChosenSave( list );')
        self.assertNotIn('if ( row > count ) return;', down)

    def test_fakk2s_own_ui_cvars(self):
        # fakk2.exe 1.02's UI registers no ui_subtitles, ui_skipDepthClear,
        # ui_crosshair or ui_jumpreticle (0x0040e9df..0x0040ea63), and its ui_hud
        # is a flag of the process (0x004df7b8) that nothing saves.
        cvars = function(self.ui, 'void RegisterCvars() {')
        assert_order(self, cvars, 'ui_hud = Cvar_Get( "ui_hud", "1", CL_RitualUiHudFlags() );',
                     'if ( !Com_GameIsFAKK2() ) {', 'Cvar_Get( "ui_jumpreticle", "1", CVAR_ARCHIVE );', '}',
                     'Cvar_Get( "dlg_badsave", "This save game is invalid", 0 );')
        self.assertIn('return Com_GameIsFAKK2() ? 0 : CVAR_ARCHIVE;', self.header)
        hud = function(self.hud, 'void CL_AliceHudInit( void ) {')
        self.assertIn('hudEnabled = Cvar_Get( "ui_hud", "1", CL_RitualUiHudFlags() );', hud)
        self.assertIn('subtitlesEnabled = Com_GameIsFAKK2() ? nullptr : Cvar_Get( "ui_subtitles", "1", CVAR_ARCHIVE );', hud)
        # The report's lines are printed whole (0x004b76df); the widget's cut ends them.
        edit = function(self.ui, 'void DrawMultiLineEdit( Menu &menu, int index, float x, float y ) {')
        self.assertIn('const std::string &text = edit.lines[line];', edit)
        self.assertNotIn('substr( 0, fit )', edit)

    def test_the_ritual_clients_menu_cvars(self):
        # Both clients register these (fakk2.exe 1.02 0x00422806..0x0042284e,
        # 0x0045768a; alice.exe 0x0042cafd..0x0042cb39, 0x0046adba). They sit
        # ahead of the userinfo cvars tests/rtcw_player_interaction_tests.py
        # compiles from "password" on.
        init = read('code/client/cl_main.cpp')
        assert_order(self, init, 'if ( Com_GameIsRitual() ) {\n\t\t/* Both Ritual clients register these',
                     'Cvar_Get( "com_blood", "1", CVAR_ARCHIVE | CVAR_USERINFO | CVAR_SERVERINFO );',
                     'Cvar_Get( "s_milesdriver", "auto", CVAR_ARCHIVE );',
                     'Cvar_Get( "s_speaker_type", "0", CVAR_ARCHIVE );',
                     'Cvar_Get( "s_khz", "22", CVAR_ARCHIVE_ND | CVAR_LATCH );',
                     'Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );',
                     # Both renderers' (fakk2.exe 0x004616fc, alice.exe 0x00475b06);
                     # F.A.K.K.2's advanced options menu shows it as checked.
                     'Cvar_Get( "r_useglfog", "1", CVAR_ARCHIVE )',
                     'Cvar_Get ("password", "", CVAR_USERINFO | CVAR_NORESTART);')

    def test_fakk2s_checkbox_is_indented(self):
        # UICheckBox: fakk2.exe 1.02's constructor sets the box's indent to 16
        # (0x004bae69..0x004bae7a) where alice.exe's sets 0 (0x004e0290); Draw
        # puts the box at the indent and the title at twice it past the box
        # (0x004bb6e0..0x004bb84b), 48 units in.
        draw = function(self.ui, 'void DrawCheckBox( Menu &menu, int index, float x, float y, float alpha ) {')
        assert_order(self, draw, 'const float indent = Com_GameIsFAKK2() ? mw::Fakk2CheckIndent : 0.0f;',
                     'VirtualPic( x + indent, boxY, mw::CheckSize, mw::CheckSize, color,',
                     'VirtualFill( x + indent, boxY, mw::CheckSize, mw::CheckSize, fill );',
                     'WidgetPrint( widget, widget.fg, x + mw::CheckTitleX( indent ), y + titleY,')

    def test_the_views_font_and_the_loading_picture(self):
        # fakk2.exe 1.02 builds View3D with facfont-20 (0x0040ecb2); alice.exe
        # with courier-14 (0x00410840). The frame counter and both prints use it.
        name = function(self.screen, 'const char *ViewFontName() {')
        self.assertIn('return Com_GameIsFAKK2() ? "facfont-20" : "courier-14";', name)
        self.assertIn('viewFont = CL_AliceFontRegister( ViewFontName() );', function(self.screen, 'int ViewFont() {'))
        self.assertIn('viewFont = CL_AliceFontRegister( ViewFontName() );',
                      function(self.screen, 'void CL_AliceScreenInit( void ) {'))
        # The loading picture at its own size, centred on black (0x0040b14c..0x0040b222).
        loading = function(read('code/client/cl_alice_loading.cpp'), 'void CL_AliceDrawLoadingScreen( void ) {')
        assert_order(self, loading, 'if ( Com_GameIsFAKK2() ) {',
                     're.GetShaderImageSize( loading, &width, &height );', 'CL_AliceClearBlack();',
                     'CL_AliceDrawPic( ( cls.glconfig.vidWidth - w ) * 0.5f, ( cls.glconfig.vidHeight - h ) * 0.5f, w, h, white,')

    def test_ritual_aliases_keep_quoted_words_and_are_saved(self):
        cmd = read('code/qcommon/cmd.c')
        alias = function(cmd, 'static void Cmd_Alias_f( void ) {')
        assert_order(self, alias, 'if ( Com_GameIsRitual() && Cmd_Argc() > 3 ) {',
                     'Cmd_SetAlias( Cmd_Argv( 1 ), Cmd_Cmd() + start );', 'Cmd_SetAlias( Cmd_Argv( 1 ), Cmd_Argv( 2 ) );')
        write = function(cmd, 'void Cmd_WriteAliases( fileHandle_t file ) {')
        self.assertIn("if ( Com_GameIsRitual() && strchr( command, '\"' ) && !strchr( command, ';' ) ) {", write)
        common = read('code/qcommon/common.c')
        config = function(common, 'static void Com_WriteConfigToFile( const char *filename ) {')
        assert_order(self, config, 'Cvar_WriteVariables( f );', 'if ( Com_GameIsRitual() ) {',
                     'Cmd_WriteAliases( f );', 'FS_FCloseFile( f );')

    def test_fakk2s_no_world_scenes_keep_the_flat_light(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            light = read(f'code/{renderer}/tr_light.c')
            body = function(light, 'void R_SetupAliceEntityLighting(')
            with self.subTest(renderer=renderer):
                self.assertIn('} else if ( ( refdef->rdflags & RDF_NOWORLDMODEL ) && ri.gameId != COM_GAME_FAKK2 ) {',
                              body)

    def test_menu_classes_and_commands(self):
        urc = read('code/compat/alice_urc.hpp')
        self.assertIn('{ "PulldownMenuContainer", "UIPulldownMenuContainer" },', urc)
        self.assertIn('{ "MultiLineEdit", "UIMultiLineEdit" },', urc)
        # Popup commands run as console text, `cvar` as `command` does (0x004e5c6c).
        release = function(self.ui, 'void ReleasePulldown( float mx, float my ) {')
        assert_order(self, release, 'ClosePulldown();',
                     'if ( item.type == PopupCommand || item.type == PopupCvar ) ExecuteLine( item.data );')
        # The graphics report is written before the menus are read, so the
        # text box's one-time updatecvar finds it.
        assert_order(self, function(self.ui, 'void Init() {'), 'SetGfxInfo();', 'LoadMenus();')
        self.assertIn('"ui_checkrestart"', self.ui)


if __name__ == '__main__':
    unittest.main()
