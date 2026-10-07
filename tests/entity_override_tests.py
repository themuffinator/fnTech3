"""Entity overrides and the in-game entity editor.

The editor's reader (code/qcommon/entity_text.hpp) must split entity text
exactly as the engine's COM_ParseExt hands it to game modules a token at a
time, so the first test compiles both -- the engine's own tokenizer sliced
out of code/qcommon/q_shared.c, and the editor's -- and compares them over
seeded random entity-like text: quotes, comments, braces, bytes from 0x80 up,
line breaks, NULs and overlong tokens.

The rest pin the wiring: which file the server reads in place of the BSP's
entities, and that every profile's game module, the botlib and the client
game read the text the server chose; that every profile's scene adapter
hands its view to the editor; that the editor's keys, characters and
pointer reach it on every platform layer; that what it draws keeps to its
share of the renderers' command buffer; and that its interface, Dear ImGui,
comes from a pinned Meson wrap.
"""
from pathlib import Path
import math
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def engine_tokenizer():
    shared = read('code/qcommon/q_shared.c')
    return (function(shared, 'static const char *SkipWhitespace( const char *data, qboolean *hasNewLines ) {')
            + function(shared, 'const char *COM_ParseExt( const char **data_p, qboolean allowLineBreaks )'))


HARNESS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include "code/qcommon/entity_text.hpp"

typedef enum { qfalse, qtrue } qboolean;
#define MAX_TOKEN_CHARS 1024
#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))
static char com_token[MAX_TOKEN_CHARS];
static int com_lines;
static int com_tokenline;
@ENGINE@

/* The engine's trap: tokens until COM_ParseExt leaves the pointer NULL with
 * an empty token, as SV_GameSystemCalls' entity-token case stops. */
static std::vector<std::string> Engine( const std::string &text ) {
	std::vector<std::string> out;
	const char *cursor = text.c_str();
	for ( int guard = 0; guard < 100000; guard++ ) {
		const char *token = COM_ParseExt( &cursor, qtrue );
		if ( !cursor && !token[0] ) return out;
		out.push_back( token );
	}
	assert( !"runaway" );
	return out;
}

static std::vector<std::string> Editor( const std::string &text ) {
	std::vector<std::string> out;
	fntech3::entities::Tokenizer tokenizer( text );
	fntech3::entities::Token token;
	while ( tokenizer.Next( token ) ) out.push_back( token.text );
	return out;
}

int main() {
	static const char *pieces[] = {
		"{", "}", "\"", " ", "\t", "\n", "\r\n", "//", "/*", "*/", "classname", "origin", "0 0 64",
		"\"worldspawn\"", "\"\"", "{\"a\"", "}x", "\xc3\xa9", "\x80", "\xff", "a b", "//c\n", "/* d */",
		"\"//q\"", "\"/*q*/\"", "x", "1", "-8.5",
	};
	std::mt19937 random( 20261005u );
	int compared = 0;
	for ( int round = 0; round < 6000; round++ ) {
		std::string text;
		const int count = static_cast<int>( random() % 40 );
		for ( int i = 0; i < count; i++ ) {
			text += pieces[random() % ( sizeof( pieces ) / sizeof( pieces[0] ) )];
		}
		if ( round % 50 == 0 ) {
			text += "\"" + std::string( 900 + random() % 400, 'v' ) + "\" tail";
		}
		if ( round % 97 == 0 ) {
			text += std::string( 1, '\0' ) + "{ \"after\" \"nul\" }";
		}
		/* the engine reads to the first NUL, as the editor does */
		const std::string upToNul = text.substr( 0, text.find( '\0' ) );
		const auto engine = Engine( upToNul );
		const auto editor = Editor( text );
		if ( engine != editor ) {
			std::printf( "round %d differs: %zu engine tokens, %zu editor tokens\n", round, engine.size(), editor.size() );
			for ( size_t i = 0; i < engine.size() || i < editor.size(); i++ ) {
				std::printf( "  [%zu] engine '%s' editor '%s'\n", i, i < engine.size() ? engine[i].c_str() : "-",
					i < editor.size() ? editor[i].c_str() : "-" );
			}
			return 1;
		}
		compared += static_cast<int>( engine.size() );
	}
	std::printf( "%d tokens compared\n", compared );
	return compared > 10000 ? 0 : 2;
}
'''


class TokenizerParityTests(unittest.TestCase):
    def test_editor_splits_entity_text_as_the_engine_hands_it_to_game_modules(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        source = HARNESS.replace('@ENGINE@', engine_tokenizer())
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='entity-tokens-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O1', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class ServerOverrideWiringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.game = read('code/server/sv_game.cpp')
        cls.init = read('code/server/sv_init.cpp')
        cls.commands = read('code/server/sv_ccmds.cpp')

    def test_every_profile_registers_the_override_switch(self):
        quake_live_block = self.init[self.init.index('if ( quakeLive ) {'):self.init.index('// systeminfo')]
        registration = 'sv_entityOverrides = Cvar_Get( "sv_entityOverrides", "1", CVAR_ARCHIVE_ND );'
        self.assertIn(registration, self.init)
        # outside the Quake Live block's two branches, so every profile has it
        self.assertLess(quake_live_block.index('sv_vac = nullptr;'), quake_live_block.index(registration))

    def test_quake_live_directory_first_then_maps_ent_parsed_and_rewritten(self):
        select = function(self.game, 'const char *SelectServerEntityString()')
        self.assertLess(select.index('sv_altEntDir'), select.index('sv_entityOverrides'))
        self.assertIn('serverEntityOverridePath.empty() && sv_entityOverrides && sv_entityOverrides->integer', select)
        self.assertIn('serverEntityTextGeneration++', select)
        loader = function(self.game, 'bool LoadMapEntityOverride( const std::string &path )')
        self.assertIn('fntech3::entities::Parse(', loader)
        self.assertIn('"worldspawn"', loader)
        self.assertIn('std::memchr( bytes, \'\\0\', length )', loader)
        self.assertIn('CM_CopyEntityStringForGame( Com_GameProfile()->id', loader)
        path = function(self.game, 'std::optional<std::string> EntityOverrideFileFor( const std::string &map )')
        self.assertIn('BuildEntityFilePath( directory, base, MAX_QPATH )', path)
        map_path = function(self.game, 'std::string CurrentEntityMapPath()')
        self.assertIn("Com_GameIsRitual()", map_path)
        self.assertIn("map.find( '$' )", map_path)

    def test_every_consumer_reads_the_chosen_text(self):
        for path, needle in (('code/server/server_alice_game.hpp', 'const char *source = SV_EntityText();'),
                             ('code/server/server_mohaa_game.hpp', 'entityText = SV_EntityText();'),
                             ('code/server/server_q3test_game.hpp', 'const char *entities = SV_EntityText();'),
                             ('code/server/sv_bot.cpp', 'return const_cast<char *>( SV_EntityText() );')):
            self.assertIn(needle, read(path), path)
        self.assertIn('sv.entityParsePoint = SelectServerEntityString();', self.game)
        shutdown = function(self.game, 'void SV_ShutdownGameProgs( void ) {')
        self.assertEqual(2, shutdown.count('ForgetServerEntityText();'))

    def test_saveentfile_writes_the_running_text_to_the_override_path(self):
        command = function(self.commands, 'static void SV_SaveEntFile_f( void ) {')
        self.assertIn('SV_EntityOverrideFile( path, sizeof( path ) )', command)
        # written and checked, since FS_WriteFile reports no failure
        self.assertNotIn('FS_WriteFile(', command)
        self.assertLess(command.index('FS_FOpenFileWrite( path )'), command.index('FS_Write( text, length, file )'))
        self.assertLess(command.index('if ( written != length ) {'), command.index('Com_Printf( "Wrote %s'))
        self.assertIn('{ "sv_saveentfile", SV_SaveEntFile_f },', self.commands)

    def test_the_editor_reloads_a_map_whose_entities_spawn_differently(self):
        # map_restart keeps the client game's state for each entity number;
        # RTCW's single player spins in its animation code when another list
        # spawns, so only lists that spawn alike take it.
        editor = read('code/client/cl_entity_editor.cpp')
        apply = function(editor, 'void Apply()')
        # a box still being typed in counts, as a save's does
        self.assertLess(apply.index('CommitField();'), apply.index('ee::SpawnsAlike('))
        self.assertIn('StartLevelChange( ReloadCommand(),', apply)
        self.assertIn('StartLevelChange( RestartCommand(),', apply)
        keeps = function(editor, 'bool RestartKeepsClientGame()')
        self.assertIn('return !Com_GameIsRitual() && !Com_GameIsMOHAA() && !Com_GameIsSTVEFSP();', keeps)
        # retail Q3Test's engine has map_restart from 1.06 only
        self.assertIn('return release && release->id >= Q3TEST_RELEASE_1_06;', keeps)
        self.assertIn('if ( !RestartKeepsClientGame() ) {\n\t\treturn ReloadCommand();',
                      function(editor, 'std::string RestartCommand()').replace('\r\n', '\n'))
        self.assertIn('SV_ReloadMapCommand( command, sizeof( command ) )', function(editor, 'std::string ReloadCommand()'))
        self.assertIn('StartLevelChange( ReloadCommand(), "reloading the map" );', function(editor, 'void ReloadMap()'))
        # the reload keeps the game type: a plain map turns single player into
        # a free-for-all where game type 2 is single player
        reload = function(self.commands, 'qboolean SV_ReloadMapCommand( char *command, int size ) {')
        self.assertIn('sv_gametype->integer == GT_SINGLE_PLAYER', reload)
        self.assertIn('singlePlayer ? "sp" : ""', reload)
        self.assertIn('cheats ? "devmap" : "map"', reload)
        self.assertIn('qboolean SV_ReloadMapCommand( char *command, int size );', read('code/qcommon/qcommon.h'))

    def test_the_editor_suspends_the_game_menus_while_open(self):
        screen = function(read('code/client/cl_scrn.cpp'), 'static void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {')
        self.assertIn('const bool editorView = cls.state == CA_ACTIVE && CL_EntityEditorActive();', screen)
        self.assertIn('if ( editorView ) {\n\t\tbrowserOverlayAllowed = false;', screen.replace('\r\n', '\n'))
        self.assertIn('if ( editorView ) {\n\t\tbrowserSuppressUiRefresh = true;', screen.replace('\r\n', '\n'))
        alice = function(read('code/client/cl_alice_screen.cpp'), 'void CL_AliceUpdateScreenField( stereoFrame_t stereo ) {')
        self.assertIn('if ( !editorView ) {\n\t\tCL_AliceUIDrawMenus();', alice.replace('\r\n', '\n'))
        self.assertIn('if ( !editorView ) {\n\t\tCL_AliceUIDrawCursor();', alice.replace('\r\n', '\n'))
        hud = function(read('code/client/cl_mohaa_ui.cpp'), 'void CL_MOHAAUIDrawHud( void ) {')
        self.assertLess(hud.index('UpdatePause();'), hud.index('if ( CL_EntityEditorActive() ) {'))
        self.assertLess(hud.index('if ( CL_EntityEditorActive() ) {'), hud.index('DrawSubtitles();'))

    def test_apply_on_another_server_goes_over_rcon(self):
        # Another server cannot run the editor's map command; with its rcon
        # password it is asked to load the map again, and its reply, which
        # rcon sends back instead of to the server's log, says whether it
        # read the saved file.
        editor = read('code/client/cl_entity_editor.cpp')
        change = function(editor, 'void StartLevelChange( const std::string &command, const char *what )')
        self.assertLess(change.index('if ( !editor.local ) {'), change.index('StartRemoteLevelChange();'))
        remote = function(editor, 'void StartRemoteLevelChange()')
        self.assertIn('RemoteApplyAvailable()', remote)
        self.assertLess(remote.index('if ( !Save() ) {'),
                        remote.index('Cbuf_ExecuteText( EXEC_INSERT, ( "rcon " + command + "\\n" ).c_str() );'))
        available = function(editor, 'bool RemoteApplyAvailable()')
        self.assertIn('!clc.demoplaying', available)
        self.assertIn('Cvar_VariableString( "rconPassword" )', available)
        command = function(editor, 'std::string RemoteReloadCommand()')
        self.assertIn('return "restart";', command)
        self.assertIn('return "devmap " + map;', command)
        # the map name goes as one argument: no separator can follow it
        allowed = command[command.index('find_first_not_of( "') + len('find_first_not_of( "'):]
        allowed = allowed[:allowed.index('"')]
        self.assertFalse(set(allowed) & set(';"\n\r\t $'), allowed)
        heard = function(editor, 'void CL_EntityEditorServerPrint( const char *text )')
        self.assertIn('"Loaded entity override " + editor.remotePath', heard)
        self.assertIn('"Ignored " + editor.remotePath', heard)
        packet = function(read('code/client/cl_main.cpp'), 'static bool CL_ConnectionlessPacket(')
        self.assertLess(packet.index('Com_Printf( "%s", s );'), packet.index('CL_EntityEditorServerPrint( s );'))
        stub = editor[editor.index('#else /* FNTECH3_ENTITY_EDITOR_UI */'):]
        self.assertIn('void CL_EntityEditorServerPrint( const char * ) {}', stub)

    def test_the_editor_hides_the_q3test_hud(self):
        # Q3Test's cgame draws through its own adapter, not the Quake III
        # syscalls that drop the HUD: its 2D draws and its status bar head's
        # worldless scene wait while the editor is open.
        cgame = read('code/client/cl_q3test_cgame.cpp')
        scene = function(cgame, 'void QDECL Q3CG_RenderScene( const void *native ) {')
        hidden = 'if ( ( fd.rdflags & RDF_NOWORLDMODEL ) && CL_EntityEditorActive() ) {'
        self.assertLess(scene.index(hidden), scene.index('CL_EntityEditorAdjustView( &fd );'))
        for signature in ('void QDECL Q3CG_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, '
                          'float s2, float t2, qhandle_t shader ) {',
                          'void QDECL Q3CG_FillRect( float x, float y, float w, float h, const float *color ) {',
                          'void QDECL Q3CG_DrawPic( float x, float y, float w, float h, qhandle_t shader ) {',
                          'void Q3TestDrawBigChar( float x, float y, int ch ) {'):
            body = function(cgame, signature)
            self.assertLess(body.index('CL_EntityEditorActive()'), body.index('re.DrawStretchPic('), signature)


class ClientEditorWiringTests(unittest.TestCase):
    def test_every_scene_adapter_hands_the_editor_its_view(self):
        sites = {
            'code/client/cl_cgame.cpp': ('CL_HudAdjustRefdef( &moduleRefdef );\n\t\tCL_EntityEditorAdjustView( &moduleRefdef );',),
            'code/client/client_rtcw_render_compat.hpp': ('CL_HudAdjustRefdef( &view );\n\t\t\tCL_EntityEditorAdjustView( &view );',),
            'code/client/client_wolfet_render_compat.hpp': ('CL_HudAdjustRefdef( &view );\n\t\t\tCL_EntityEditorAdjustView( &view );',),
            'code/client/cl_mohaa_cgame.cpp': ('CL_EntityEditorAdjustView( &view );',),
            'code/client/client_alice_cgame.hpp': ('CL_EntityEditorAdjustView( &view );\n\tre.RenderScene( &view );',),
            'code/client/cl_q3test_cgame.cpp': ('CL_EntityEditorAdjustView( &fd );\n\tre.RenderScene( &fd );',),
        }
        for path, needles in sites.items():
            text = read(path).replace('\r\n', '\n')
            for needle in needles:
                self.assertIn(needle, text, path)

    def test_the_editor_draws_the_view_its_markers_are_projected_with(self):
        # The renderers widen a game's 4:3 field of view on other screens
        # (r_fovCorrection) after the client hands them its view, so markers
        # projected with the game's field drifted from what they marked, a
        # third farther from the centre at 16:9. The editor hands the renderer
        # a field of its own, flagged to be drawn as given, and projects with
        # that same view.
        editor = read('code/client/cl_entity_editor.cpp').replace('\r\n', '\n')
        adjust = function(editor, 'void CL_EntityEditorAdjustView( refdef_t *refdef )')
        fov = adjust.index('EditorFov( screenWidth, screenHeight, refdef->fov_x, refdef->fov_y );')
        flag = adjust.index('refdef->rdflags |= RDF_NOFOVCORRECTION;')
        self.assertLess(fov, flag)
        # every view drawn while it is open, its sky portals' and the first
        # before its camera is placed, takes it
        self.assertLess(flag, adjust.index('if ( !editor.cameraSet ) {'))
        self.assertLess(flag, adjust.index('if ( portal ) {'))
        self.assertLess(flag, adjust.rindex('ViewFromRefdef( *refdef, editor.view );'))
        self.assertIn('ee::FitFieldOfView( horizontal, aspect, fovX, fovY );',
                      function(editor, 'void EditorFov( int width, int height, float &fovX, float &fovY )'))
        self.assertIn('( fd->rdflags & ( RDF_NOFOVCORRECTION | RDF_NOWORLDMODEL ) )',
                      read('code/renderercommon/tr_q3_fov.h'))
        # a brush entity's box is its brushes', without the unit the
        # collision map spreads each brush model's bounds by
        self.assertIn('out->mins[j] = LittleFloat (in->mins[j]) - 1;', read('code/qcommon/cm_load.c'))
        markers = function(editor, 'void RebuildMarkers()')
        self.assertIn('mins[axis] += 1.0f;', markers)
        self.assertIn('maxs[axis] -= 1.0f;', markers)
        # a point entity's box is its game's (ee::PointExtent)
        self.assertIn('MarkerExtent( entity, mins, maxs );', markers)
        self.assertIn('ee::PointExtent( GameLineage(), entity )', editor)

    def test_client_game_entity_tokens_follow_the_local_override(self):
        for path in ('code/client/cl_cgame.cpp', 'code/client/client_rtcw_render_compat.hpp',
                     'code/client/client_wolfet_render_compat.hpp', 'code/client/client_jka_cgame_host.hpp'):
            text = read(path)
            self.assertIn('CL_GetEntityToken(', text, path)
            self.assertNotIn('re.GetEntityToken', text, path)
        cgame = read('code/client/cl_cgame.cpp')
        self.assertIn('CL_EntityOverrideCGameInit();', function(cgame, 'void CL_InitCGame( void ) {'))
        self.assertIn('CL_EntityOverrideCGameShutdown();', function(cgame, 'void CL_ShutdownCGame( void ) {'))
        editor = read('code/client/cl_entity_editor.cpp')
        tokens = function(editor, 'qboolean CL_GetEntityToken( char *buffer, int size )')
        self.assertIn('return re.GetEntityToken( buffer, size );', tokens)
        self.assertIn('Com_GameIsJKAMP()', tokens)

    def test_input_reaches_the_editor_and_modules_cannot_clear_its_catcher(self):
        keys = read('code/client/cl_keys.cpp')
        set_catcher = function(keys, 'void Key_SetCatcher( int catcher )')
        self.assertIn('( keyCatchers & KEYCATCH_ENTITYEDITOR )', set_catcher)
        self.assertIn('CL_EntityEditorKeyEvent( key, qtrue, time )', function(keys, 'static void CL_KeyDownEvent( int key, unsigned time )'))
        self.assertIn('CL_EntityEditorKeyEvent( key, qfalse, time )', function(keys, 'static void CL_KeyUpEvent( int key, unsigned time )'))
        self.assertIn('CL_EntityEditorCharEvent(', function(keys, 'void CL_CharEvent( int key )'))
        mouse = read('code/client/cl_input.cpp')
        self.assertIn('CL_EntityEditorMouseEvent( dx, dy )', function(mouse, 'void CL_MouseEvent( int dx, int dy'))
        self.assertIn('CL_EntityEditorMouseAbsoluteEvent( x, y )', function(mouse, 'void CL_MouseAbsoluteEvent( int x, int y ) {'))
        self.assertRegex(read('code/qcommon/q_shared.h'), r'#define KEYCATCH_ENTITYEDITOR 0x0040')
        for path in ('code/sdl/sdl_input.cpp', 'code/unix/linux_glimp.cpp', 'code/win32/win_input.cpp'):
            self.assertIn('CL_EntityEditorPointerMask()', read(path), path)
        for path in ('code/sdl/sdl_input.cpp', 'code/unix/linux_glimp.cpp'):
            self.assertRegex(read(path), r'kTextInputCatcherMask =[^;]*KEYCATCH_ENTITYEDITOR;', path)

    def test_the_editor_draws_under_the_console_and_hides_the_game_hud(self):
        screen = read('code/client/cl_scrn.cpp').replace('\r\n', '\n')
        self.assertIn('CL_EntityEditorDraw();\n\t\tCon_DrawConsole ();', screen)
        alice = read('code/client/cl_alice_screen.cpp').replace('\r\n', '\n')
        self.assertLess(alice.index('CL_EntityEditorDraw();'), alice.index('Con_DrawConsole();'))
        self.assertIn('if ( !CL_EntityEditorActive() ) {\n\t\tCL_AliceHudDraw();', alice)
        self.assertIn('|| CL_EntityEditorActive()', function(read('code/client/cl_cgame.cpp'), 'static qboolean CL_CaptureHidesHud( void ) {'))
        self.assertIn('if ( CL_EntityEditorActive() ) return qfalse;', read('code/client/cl_hud.cpp'))
        self.assertIn('!CL_EntityEditorActive()', function(read('code/client/cl_console.cpp'), 'void Con_DrawConsole( void ) {'))

    def test_the_overlay_keeps_to_its_share_of_the_render_command_buffer(self):
        # Every renderer drops a command that no longer fits its buffer, so
        # an overlay that filled it would take the panels and the console.
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            local = read(f'code/{renderer}/tr_local.h')
            self.assertRegex(local, r'#define\s+MAX_RENDER_COMMANDS\s+0x80000', renderer)
            self.assertRegex(local, r'typedef struct \{\s*int\s+commandId;\s*shader_t\s+\*shader;\s*float\s+x, y;\s*'
                                    r'float\s+w, h;\s*float\s+s1, t1;\s*float\s+s2, t2;\s*\} stretchPicCommand_t;', renderer)
            self.assertRegex(local, r'stretchPicCommand_t base;\s*refDrawQuad_t quad;\s*\} drawQuadCommand_t;', renderer)
        self.assertRegex(read('code/renderercommon/tr_types.h'), r'vec2_t xy\[4\], st\[4\];\s*color4ub_t colors\[4\];\s*'
                                                                 r'unsigned int colorMask;\s*qhandle_t shader;\s*\} refDrawQuad_t;')
        editor = read('code/client/cl_entity_editor.cpp')
        value = {name: math.prod(int(factor) for factor in number.split('*')) for name, number in re.findall(
            r'constexpr (?:int|std::size_t) (k\w+) = ([0-9 *]+);', editor)}
        # the 64-bit sizes: a picture is an id, a pointer and eight floats
        picture, quad, colour = 4 + 4 + 8 + 8 * 4, 48 + 2 * 4 * 2 * 4 + 4 * 4 + 4 + 4, 24
        self.assertGreaterEqual(value['kPicBytes'], picture)
        self.assertGreaterEqual(value['kQuadBytes'], quad)
        self.assertGreaterEqual(value['kColorBytes'], colour)
        line, fill = value['kQuadBytes'], value['kPicBytes'] + value['kColorBytes']
        worst = ((value['kMaxBoxes'] + 2) * 15 * line          # boxes and facing arrows, selected and hovered too
                 + value['kMaxDots'] * fill
                 + value['kMaxLabels'] * (fill + value['kColorBytes'] + value['kLabelChars'] * value['kPicBytes'])
                 + value['kMaxLinks'] * 3 * line
                 + 9 * line + 5 * fill + 2 * line)              # the gizmo and the crosshair
        buffer = 0x80000
        self.assertLessEqual(worst, buffer * 45 // 100)
        self.assertLessEqual(value['kFrameBytes'], buffer * 65 // 100)
        frame = function(editor, 'void Frame()')
        # the overlay is ImGui's background list, drawn first and trimmed first
        self.assertIn('ui::AppendDrawList( *list, list == editor.overlay ? editor.viewDraw : editor.panelDraw );', frame)
        self.assertLess(frame.index('UpdateTextures( *data );'), frame.index('ui::AppendDrawList('))
        self.assertLess(frame.index('Render( editor.viewDraw,'), frame.index('Render( editor.panelDraw );'))
        self.assertIn('editor.frameBytes + bytes > kFrameBytes', function(editor, 'void Render( const ui::DrawList &list, int skip = 0 )'))
        # a release clears the editor's held keys and buttons wherever it is routed
        key = function(editor, 'qboolean CL_EntityEditorKeyEvent( int key, qboolean down, unsigned time )')
        self.assertLess(key.index('if ( !down && key >= 0 && key < MAX_KEYS ) {'), key.index('if ( !editor.active ||'))

    def test_build_lists_the_editor_and_its_tests(self):
        meson = read('meson.build')
        self.assertIn("'code/client/cl_entity_editor.cpp',", meson)
        self.assertIn("'tests/entity_editor_tests.cpp',", meson)
        self.assertIn('$(B)/client/cl_entity_editor.o', read('Makefile'))

    def test_dear_imgui_comes_from_its_pinned_wrap(self):
        # Dear ImGui is a Meson wrap, not a copy in the tree: upstream at a
        # pinned commit, built from our own minimal meson.build
        wrap = read('subprojects/imgui.wrap')
        self.assertIn('url = https://github.com/ocornut/imgui.git', wrap)
        self.assertRegex(wrap, r'revision = [0-9a-f]{40}')
        self.assertIn('patch_directory = imgui', wrap)
        build = read('subprojects/packagefiles/imgui/meson.build')
        for source in ('imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp',
                       'misc/cpp/imgui_stdlib.cpp'):
            self.assertIn(f"'{source}'", build)
        self.assertNotIn('backends/', build)
        self.assertIn("meson.override_dependency('imgui', imgui_dep)", build)
        meson = read('meson.build')
        self.assertRegex(meson, r"imgui_dep = dependency\('imgui',[^)]*fallback: 'imgui'")
        self.assertIn("client_cpp_args += ['-DFNTECH3_ENTITY_EDITOR_UI']", meson)
        makefile = read('Makefile')
        self.assertIn('IMGUIDIR ?= subprojects/imgui', makefile)
        self.assertIn('-DFNTECH3_ENTITY_EDITOR_UI', makefile)
        self.assertIn('$(B)/client/imgui_stdlib.o', makefile)
        # without it the client keeps overrides and the client game's tokens
        editor = read('code/client/cl_entity_editor.cpp')
        stub = editor[editor.index('#else /* FNTECH3_ENTITY_EDITOR_UI */'):]
        self.assertIn('void CL_EntityEditorDraw( void ) {}', stub)
        self.assertIn('qboolean CL_GetEntityToken( char *buffer, int size )', stub)
        self.assertIn('CL_EntityEditorRendererStarted();', function(read('code/client/cl_main.cpp'),
                                                                   'static void CL_InitRenderer( void ) {'))
        credits = read('CREDITS.md') + read('THIRD_PARTY_NOTICES.md')
        self.assertIn('https://github.com/ocornut/imgui', credits)
        # built copies carry ImGui's MIT notice, in the notices the release ships
        notices = read('THIRD_PARTY_NOTICES.md')
        section = notices[notices.index('## Dear ImGui'):]
        self.assertIn('Copyright (c) 2014-2026 Omar Cornut', section)
        self.assertIn('The above copyright notice and this permission notice shall be included in all', section)
        self.assertIn('(ROOT / "THIRD_PARTY_NOTICES.md", Path("THIRD_PARTY_NOTICES.md"))', read('scripts/release.py'))


if __name__ == '__main__':
    unittest.main()
