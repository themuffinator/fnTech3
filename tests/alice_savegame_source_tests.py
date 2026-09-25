"""American McGee's Alice's savegames and the server's side of its level flow.

Retail alice.exe keeps a save as four files, save/<config>_<name>.sav, .spv,
.ssv and .tga: the game module's level and persistent archives, the engine's
200-byte header with the area portals behind it, and a grey 256-pixel picture
of the frame. Its map, devmap, gamemap, restart, savegame, autosavegame,
loadgame, loadbrokengame and pause commands replace the Quake III map set, and
its SV_SpawnServer splits a `$` spawn target off the BSP name, reads a level
back from a save or spawns it fresh with three 100 ms settle frames. These
checks pin that the whole of it is gated on the Alice profile, that the other
profiles keep their own paths, and that the pieces reach each other; the byte
rules themselves are tested by tests/alice_savegame_tests.cpp. See
docs/fntech3/2026-09-23-alice-savegames.md.
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
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


RETAIL_MESSAGES = (
    'You must be in a game to save.\\n',
    "Can't savegame in a deathmatch\\n",
    "Can't savegame when dead\\n",
    'Not enough free space to save\\n',
    'Bad savedir.\\n',
    'Saving game...%s\\n',
    'Savegame not found.\\n',
    'USAGE: loadgame <name>\\n',
    'USAGE: loadbrokengame <name>\\n',
    'USAGE: map <map>\\n',
    'USAGE: gamemap <map>\\n',
    'SV_GameMap(%s)\\n',
    "Couldn't write %s\\n",
    "Couldn't read %s\\n",
    'copy %s to %s\\n',
    'numareaportals differs from save game',
    'Server is not running.\\n',
    'saveshot %s 256 256\\n',
)


class AliceSavegameSourceTests(unittest.TestCase):
    def setUp(self):
        self.save = read('code/server/server_alice_save.hpp')
        self.game = read('code/server/server_alice_game.hpp')
        self.init = read('code/server/sv_init.cpp')

    def test_retail_messages_are_kept(self):
        for message in RETAIL_MESSAGES:
            self.assertIn(message, self.save, message)
        compat = read('code/compat/alice_savegame.hpp')
        self.assertIn('"wait 1500; pause ; autosavegame ; pause\\n"', compat)
        self.assertIn('"%s/%s_%s.%s", Directory', compat)

    def test_alice_takes_the_place_of_the_quake_iii_map_set(self):
        commands = read('code/server/sv_ccmds.cpp')
        add = function(commands, 'void SV_AddOperatorCommands( void ) {')
        branch = add[add.index('if ( Com_GameIsRitual() ) {'):]
        self.assertLess(branch.index('SV_AliceAddCommands();'), branch.index('return;'))
        # The Quake III set is added after the Alice branch has returned.
        self.assertLess(add.index('if ( Com_GameIsRitual() ) {'), add.index('SV_AddCommandSet( mapCommands );'))
        registered = function(self.save, 'void SV_AliceAddCommands( void ) {')
        for name in ('map', 'devmap', 'gamemap', 'restart', 'savegame', 'autosavegame',
                     'loadgame', 'loadbrokengame', 'pause'):
            self.assertIn(f'Cmd_AddCommand( "{name}", SV_Alice', registered)
        self.assertIn('Cmd_RemoveCommand( "map_restart" );', registered)
        self.assertIn('Cvar_Get( "config", "", 0 );', registered)

    def test_the_spawn_is_alice_only_where_it_differs(self):
        spawn = function(self.init, 'void SV_SpawnServer( const char *mapname, qboolean killBots ) {')
        self.assertIn('if ( Com_GameIsRitual() ) {\n\t\tbspName = SV_AlicePrepareSpawn( mapname, aliceBsp, sizeof( aliceBsp ) );', spawn)
        self.assertIn('CM_LoadMap( va( "maps/%s.bsp", bspName ), qfalse, &checksum );', spawn)
        self.assertIn('Cvar_Set( "mapname", mapname );', spawn)
        self.assertIn('if ( !Com_GameIsRitual() ) {\n\t\tCvar_Set( "nextmap", "map_restart 0" );', spawn)
        self.assertIn('if ( Com_GameIsRitual() ) {\n\t\tSV_AliceFlushModels();', spawn)
        self.assertIn('const qboolean aliceRestored = Com_GameIsRitual() ? SV_AliceSpawnLevel() : qfalse;', spawn)
        self.assertIn('SV_AliceSettle( aliceRestored );', spawn)
        # Every other profile keeps its three frames and its fourth.
        self.assertIn('SV_Indices( 3 ) ) {\n\t\t\t(void)ignored;\n\t\t\tCbuf_Wait();\n'
                      '\t\t\tVM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );', spawn)
        # (Medal of Honor settles its own way and runs no fourth either)
        last = spawn[spawn.index('} else if ( !Com_GameIsQuake3IHV() && !Com_GameIsRitual() && !Com_GameIsMOHAA() ) {'):]
        self.assertLess(last.index('VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );'), last.index('\n\t}\n'))
        # The flush follows the client's shutdown and precedes the new game.
        self.assertLess(spawn.index('CL_ShutdownAll();'), spawn.index('SV_AliceFlushModels();'))
        self.assertLess(spawn.index('SV_AliceFlushModels();'), spawn.index('SV_InitGameProgs();'))
        self.assertLess(spawn.index('SV_AliceRestoreLevelTime();'), spawn.index('SV_InitGameProgs();'))

    def test_a_level_is_read_back_or_spawned_fresh(self):
        level = function(self.save, 'qboolean SV_AliceSpawnLevel( void ) {')
        self.assertLess(level.index('SV_AliceCallCleanup();'), level.index('SV_AliceArchiveLevelFile( true )'))
        self.assertIn('SV_SetConfigstring( 8, "none" );', level)
        fresh = level[level.index('sv.time = 0;'):]
        self.assertLess(fresh.index('SV_AliceCallSpawnEntities();'), fresh.index('SV_AliceArchivePersistantFile( true );'))
        settle = function(self.save, 'void SV_AliceSettle( qboolean restored ) {')
        self.assertIn('SV_AliceSettleFrame( sv.time );\n\t\tsv.time += 100;', settle)
        frame = function(self.game, 'void SV_AliceSettleFrame( int levelTime ) {')
        self.assertIn('runFrame( levelTime, 100 );', frame)
        # Init alone answers GAME_INIT; the level is not spawned there.
        entry = function(self.game, 'intptr_t QDECL SV_AliceGameEntry( int command, ... ) {')
        init = entry[entry.index('case GAME_INIT:'):entry.index('case GAME_SHUTDOWN:')]
        self.assertNotIn('SpawnEntities', init.replace('SV_AliceSpawnLevel', ''))

    def test_the_level_archive_order(self):
        level = function(self.save, 'qboolean SV_AliceArchiveLevelFile( bool loading ) {')
        self.assertLess(level.index('SV_AliceCallReadLevel( path )'), level.index('SV_AliceRestorePortalState( file );'))
        save = function(self.save, 'void SV_AliceSaveGame( const char *requested, bool autosave, bool requirePlayer = true ) {')
        order = [save.index(step) for step in ('SV_AliceCopyPersistant( false );',
                                                'SV_AliceArchiveLevelFile( false );',
                                                'SV_AliceArchiveServerFile( false, autosave );')]
        self.assertEqual(order, sorted(order))
        game = function(self.save, 'void SV_AliceGameMap_f() {')
        self.assertLess(game.index('SV_AliceArchivePersistantFile( false );'), game.index('SV_SpawnServer( server, qfalse );'))
        self.assertIn('Cbuf_AddText( alicesave::AutosaveCommand );', game)

    def test_the_archive_imports_and_level_exports_are_served(self):
        self.assertIn('SERVE( GetArchiveFileName, SV_AliceGetArchiveFileName );', self.game)
        self.assertIn('SERVE( CalcCRC, SV_AliceCalcCRC );', self.game)
        for export in ('Cleanup', 'WritePersistant', 'ReadPersistant', 'WriteLevel', 'ReadLevel',
                       'LevelArchiveValid'):
            self.assertIn(f'FNTECH3_ALICE_EXPORT_{export}', self.game)
        self.assertIn('( file, qfalse )', function(self.game, 'void SV_AliceCallWriteLevel( const char *file ) {'))

    def test_linked_entities_reach_the_snapshot(self):
        link = function(self.game, 'void QDECL SV_AliceLinkEntity( void *entity ) {')
        self.assertIn('SV_LinkEntityClusters( &sv.svEntities[number], absmin, absmax, number );', link)
        unlink = function(self.game, 'void QDECL SV_AliceUnlinkEntity( void *entity ) {')
        self.assertIn('record.numClusters = 0;', unlink)
        self.assertIn('record.areanum = -1;', unlink)
        portal = function(self.game, 'void QDECL SV_AliceAdjustAreaPortalState( void *entity, int open ) {')
        self.assertIn('CM_AdjustAreaPortalState( record.areanum, record.areanum2, open ? qtrue : qfalse );', portal)
        world = read('code/server/sv_world.cpp')
        helper = function(world, 'int SV_LinkEntityClusters( svEntity_t *ent, const vec3_t absmin, const vec3_t absmax, int number ) {')
        for step in ('CM_BoxLeafnumsTopnode', 'CM_BoxLeafnums', 'CM_LeafArea', 'SV_LinkQuake3IHVClusters',
                     'ent->lastCluster = CM_LeafCluster( lastLeaf );'):
            self.assertIn(step, helper)
        linker = function(world, 'void SV_LinkEntity( sharedEntity_t *gEnt ) {')
        self.assertIn('num_leafs = SV_LinkEntityClusters( ent, entity.r.absmin, entity.r.absmax, entity.s.number );', linker)
        self.assertLess(linker.index('ent->wolfetOriginCluster'), linker.index('SV_LinkEntityClusters'))

    def test_the_error_unwind_drops_a_pending_load(self):
        common = read('code/qcommon/common.c')
        self.assertIn('if ( Com_GameIsRitual() ) {\n\t\tSV_AliceSaveAbort();', common)

    def test_the_renderers_take_the_saveshot_from_the_frame(self):
        self.assertIn('void (*CaptureAliceSaveshot)( byte *rgb, int width, int height, qboolean *done );',
                      read('code/renderercommon/tr_public.h'))
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            init = read(f'code/{renderer}/tr_init.c')
            self.assertIn('#include "../compat/alice_saveshot.h"', init)
            self.assertIn('re.CaptureAliceSaveshot = RE_CaptureAliceSaveshot;', init)
            take = function(init, 'void RB_TakeSTVEFThumbnail( void ) {')
            self.assertIn('FNTECH3_AliceSaveshot( source, gls.captureWidth, gls.captureHeight,', take)
            # Elite Force's own picture is untouched, gamma and all.
            self.assertIn('R_STVEFThumbnail( source, gls.captureWidth, gls.captureHeight, padlen, rgba, side );', take)
            self.assertIn('R_GammaCorrect( rgba + i * 4, 3 );', take)
            self.assertIn('int\t\taliceSaveshotHeight;', read(f'code/{renderer}/tr_local.h'))
        client = read('code/client/cl_alice_saveshot.cpp')
        self.assertIn('re.CaptureAliceSaveshot( file.data() + alicesave::TgaHeaderBytes, width, height, &taken );', client)
        self.assertIn('FNTECH3_AliceTgaHeader( file.data(), width, height );', client)

    def test_the_saveshot_header_is_c(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        target = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='alice-saveshot-', dir=ROOT / '.tmp') as folder:
            source = Path(folder) / 'probe.c'
            source.write_text(
                '#include <stddef.h>\n'
                '#include "code/compat/alice_saveshot.h"\n'
                'int main( void ) {\n'
                '\tunsigned char frame[8 * 6 * 3] = { 0 };\n'
                '\tunsigned char out[2 * 2 * 3];\n'
                '\tunsigned char header[FNTECH3_ALICE_TGA_HEADER_BYTES];\n'
                '\tframe[0] = 120;\n'
                '\tFNTECH3_AliceSaveshot( frame, 8, 6, 8 * 3, out, 2, 2 );\n'
                '\tFNTECH3_AliceTgaHeader( header, 2, 2 );\n'
                '\treturn out[0] == 25 && header[2] == 2 && header[16] == 24 ? 0 : 1;\n'
                '}\n', encoding='utf-8')
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            built = subprocess.run([compiler, *target, '-std=c99', '-Wall', '-Werror', '-I', str(ROOT),
                                    str(source), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
