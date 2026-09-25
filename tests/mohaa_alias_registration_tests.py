"""Medal of Honor: when a level's sound aliases exist, and for how long.

Both 1.00 modules look sounds up in one global alias list the engine keeps:
the game's Entity::Sound and LoopSound through GetRandomAlias (gamex86.dll
0x310cc0e0: the entity's model list, import 120, then the global list, import
126), the client game through import 121. The list is filled by
parsing ubersound/ubersound.scr and uberdialog.scr, each alias kept only when
a token of its `maps` is a case-insensitive prefix of the level's name:

- in single player by the client game alone, as it starts (CG_Init calls
  CM_LoadMap then CG_RegisterSounds, cgamex86.dll 0x300235e9, 0x300235f7),
  which is after the local server spawned the level;
- in multiplayer also by the game's RegisterSounds (export 20, 0x310d7c50),
  which 2015's engine calls once the spawn is done.

So a sound a level plays while it spawns finds no alias, and the game prints
"needs an alias in ubersound.scr or uberdialog.scr" for aliases the data does
define: `snd_pickup` from global/loadout.scr, a vehicle's start and idle.
Retail does the same: a 1.11 single-player log shows the `snd_pickup` miss in
m1l1's spawn. And loading a map empties the list (OpenMoHAA cm_load.c
CM_LoadMap), so one level's aliases never answer for the next; a restart, or
a load of the running level's own save, loads no map and keeps them."""
from pathlib import Path
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


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


class MohaaAliasRegistrationTests(unittest.TestCase):
    def test_the_game_registers_after_a_multiplayer_spawn_and_never_before_one(self):
        spawn = function(read('code/server/sv_init.cpp'), 'void SV_SpawnServer( const char *mapname, qboolean killBots ) {')
        self.assertLess(spawn.index('SV_MOHAASpawnLevel( mapname );'), spawn.index('SV_MOHAASettle();'))
        self.assertLess(spawn.index('SV_MOHAASettle();'), spawn.index('SV_MOHAASpawnDone();'))

        game = read('code/server/server_mohaa_game.hpp')
        self.assertEqual(game.count('Export<Void_t>( GE_RegisterSounds )();'), 1)
        done = function(game, 'void SV_MOHAASpawnDone( void ) {')
        self.assertIn('if ( exports && Cvar_VariableIntegerValue( "g_gametype" ) != 0 ) {\n'
                      '\t\tExport<Void_t>( GE_RegisterSounds )();', done)
        level = function(game, 'void SpawnLevel( const char *mapname ) {')
        self.assertNotIn('GE_RegisterSounds', level)

    def test_both_modules_share_the_engine_list(self):
        game = read('code/server/server_mohaa_game.hpp')
        self.assertIn('SERVE( GlobalAlias_Add, GlobalAliasAdd );', game)
        self.assertIn('SERVE( GlobalAlias_FindRandom, GlobalAliasFindRandom );', game)
        self.assertIn('return MOHAA_GlobalAliasAdd( alias, name, parameters );',
                      function(game, 'qboolean QDECL GlobalAliasAdd('))
        self.assertIn('return MOHAA_GlobalAliasFindRandom( alias, ret );',
                      function(game, 'const char *QDECL GlobalAliasFindRandom('))

        cgame = read('code/client/cl_mohaa_cgame.cpp')
        self.assertIn('SERVE( Alias_Add, AliasAdd );', cgame)
        self.assertIn('SERVE( Alias_FindRandom, AliasFindRandom );', cgame)
        self.assertIn('return MOHAA_GlobalAliasAdd( alias, name, parameters );',
                      function(cgame, 'qboolean QDECL AliasAdd('))
        self.assertIn('return MOHAA_GlobalAliasFindRandom( alias, ret );',
                      function(cgame, 'const char *QDECL AliasFindRandom('))

        api = read('code/mohaa/mohaa_tiki_api.cpp')
        self.assertIn('return Alias_Add( alias, name, parameters );', function(api, 'qboolean MOHAA_GlobalAliasAdd('))
        self.assertIn('return Alias_FindRandom( alias, ret );', function(api, 'const char *MOHAA_GlobalAliasFindRandom('))
        self.assertIn('Alias_Clear();', function(api, 'void MOHAA_GlobalAliasClear( void ) {'))

    def test_a_new_level_starts_with_no_alias_and_the_same_level_keeps_its_own(self):
        game = read('code/server/server_mohaa_game.hpp')
        level = function(game, 'void SpawnLevel( const char *mapname ) {')
        clear = 'if ( !sameLevel ) {\n\t\tMOHAA_GlobalAliasClear();\n\t}'
        # Asked before anything can return, so no later spawn inherits it.
        self.assertLess(level.index('const bool sameLevel = save::TakeSameLevel();'), level.index('if ( !exports ) {'))
        # Beside the model cache's reset, where the map is loaded, and before
        # anything the level registers or looks up.
        self.assertLess(level.index('TIKI_FreeAll();'), level.index(clear))
        for later in ('GE_SetMap', 'GE_Precache', 'save::ReadSavedLevel()', 'GE_SpawnEntities'):
            self.assertLess(level.index(clear), level.index(later), later)

        save = read('code/server/server_mohaa_save.hpp')
        take = function(save, 'bool TakeSameLevel() {')
        self.assertIn('sameLevel = false;', take)
        # A restart is the running level again; a level change never is, even
        # to the same map; a load is when its save is of the running map.
        spawn = 'SV_SpawnServer( map, qfalse );'
        restart = function(save, 'void Restart_f() {')
        self.assertLess(restart.index('sameLevel = true;'), restart.index(spawn))
        gamemap = function(save, 'void GameMap_f() {')
        self.assertLess(gamemap.index('sameLevel = false;'), gamemap.index(spawn))
        load = function(save, 'void LoadGame( const char *name ) {')
        rule = 'sameLevel = com_sv_running && com_sv_running->integer && !Q_stricmp( mapName, header.mapName );'
        self.assertLess(load.index(rule), load.index(spawn))

    def test_a_client_empties_the_list_unless_it_plays_on_its_own_server(self):
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        self.assertIn('SERVE( CM_LoadMap, LoadMap );', cgame)
        load = function(cgame, 'void QDECL LoadMap( const char *name ) {')
        self.assertIn('const bool ownServer = com_sv_running && com_sv_running->integer && !clc.demoplaying\n'
                      '\t\t&& clc.serverAddress.type == NA_LOOPBACK;', load)
        clear = 'if ( !ownServer ) {\n\t\tMOHAA_GlobalAliasClear();\n\t}'
        self.assertLess(load.index(clear), load.index('CM_LoadMap( name, qtrue, &checksum );'))

    def test_a_models_clear_import_dumps_as_2015s_engine_does(self):
        # OpenMoHAA's PF_Alias_Clear calls PF_Alias_Dump, and no 1.00 module
        # calls slot 122: keep 2015's wiring rather than invent a clear.
        game = read('code/server/server_mohaa_game.hpp')
        self.assertIn('SERVE( Alias_Clear, AliasDump );', game)
        self.assertIn('MOHAA_TIKI_AliasDump( tiki );', function(game, 'void QDECL AliasDump( dtiki_t *tiki ) {'))

    def test_the_list_finds_what_a_level_registered_and_forgets_it_with_the_level(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        alias = read('code/mohaa/qcommon/alias.c')
        alias = re.sub(r'#include "(q_shared|qcommon|alias)\.h"\n', '', alias)
        shared = read('code/mohaa/qcommon/q_shared.h')
        channels = shared[shared.index('enum {\n\tMOHAA_CHAN_AUTO,'):]
        channels = channels[:channels.index('};') + 2]
        tokenizer = read('code/mohaa/qcommon/mohaa_q_shared.c')
        tokenizer = function(tokenizer, 'static char com_token[MOHAA_MAX_STRING_CHARS + 1];')
        harness = r'''
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;
typedef enum { qfalse, qtrue } qboolean;
#define APP_MODULE 1
#define ARRAY_LEN( x ) ( sizeof( x ) / sizeof( *( x ) ) )
#define MOHAA_MAX_STRING_CHARS 2048
enum { TAG_TIKI = 1 };
static int blocks;
static void *Z_TagMalloc( size_t size, int tag ) { (void)tag; blocks++; return calloc( 1, size ); }
static void Z_Free( void *ptr ) { if ( ptr ) blocks--; free( ptr ); }
static void Com_Printf( const char *fmt, ... ) { (void)fmt; }
static void Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
static int Q_stricmp( const char *a, const char *b ) {
	for ( ;; a++, b++ ) {
		const int x = tolower( (unsigned char)*a ), y = tolower( (unsigned char)*b );
		if ( x != y ) return x - y;
		if ( !x ) return 0;
	}
}
''' + channels + '\n' + tokenizer + '\n#include "alias.h"\n' + alias + r'''

/* Retail's own lines (Pak0.pk3 ubersound/ubersound.scr), which a parse keeps
 * when a token of `maps` is a case-insensitive prefix of the level's name --
 * both 1.00 modules' rule (gamex86.dll 0x31044c50, cgamex86.dll 0x30016cc2),
 * which the client game applies as it starts. */
static const struct { const char *alias, *file, *parms, *maps; } uber[] = {
	{ "snd_pickup", "sound/weapons/foley/Pickup_01.wav", "soundparms 1.3 0.2 0.8 0.2 160 1000 item loaded ",
		"m dm moh obj train" },
	{ "jeep_snd_start", "sound/vehicle/veh_jeep_on.wav", "soundparms 1.0 0.0 1.0 0.0 800 4000 auto loaded ",
		"m1l2b m1l3a " },
	{ "jeep_snd_idle", "sound/vehicle/veh_jeep_idle.wav", "soundparms 0.5 0.0 1.0 0.0 800 4000 auto loaded ",
		"m1l2b m1l3a m1l3b " },
	{ "flak_snd_move", "sound/vehicle/veh_tank_turret1.wav", "soundparms 0.9 0.2 0.9 0.2 800 4000 auto loaded ",
		"m1l3 m3l2 m5l2 m5l3" },
	{ "king_snd_idle", "sound/vehicle/veh_tank_idle1.wav", "soundparms 0.5 0.0 1.0 0.0 800 4000 auto loaded ",
		"m5 " },
	{ "pickup_explosives1", "sound/items/Item_BangaloreAssemble_01.wav",
		"soundparms 1.2 0.0 1.0 0.0 100 2000 item loaded ", "m1l2a m1l2b m3l3 m4l3 training" },
	{ "pickup_explosives2", "sound/items/Item_BangaloreAssemble_02.wav",
		"soundparms 1.2 0.0 1.0 0.0 100 2000 item loaded ", "m1l2a m3l3 m4l3" },
	{ "pickup_explosives3", "sound/items/Item_BangaloreAssemble_03.wav",
		"soundparms 1.2 0.0 1.0 0.0 100 2000 item loaded ", "m1l2a m3l3 m4l3" },
	{ "m3l2_radio_pickup", "sound/items/Item_ToolBox_01.wav", "soundparms 1.5 0.0 1.0 0.0 100 2000 item loaded ",
		"m3l2 dm moh obj train" },
	{ "colt45_snd_pickup", "sound/weapons/foley/SilencerPickup1.wav",
		"soundparms 1.3 0.2 0.8 0.2 160 1000 item loaded ", "m dm moh obj train" },
	{ "colt45_snd_pickup_ammo", "sound/weapons/foley/AmmoPUPistol1.wav",
		"soundparms 1.3 0.2 0.8 0.2 160 1000 item loaded ", "m dm moh obj train" },
};

static int Covers( const char *maps, const char *level ) {
	char copy[128];
	strcpy( copy, maps );
	for ( char *token = strtok( copy, " " ); token; token = strtok( NULL, " " ) ) {
		size_t i = 0;
		while ( token[i] && tolower( (unsigned char)token[i] ) == tolower( (unsigned char)level[i] ) ) i++;
		if ( !token[i] ) return 1;
	}
	return 0;
}

static void Register( const char *level ) {
	for ( size_t i = 0; i < ARRAY_LEN( uber ); i++ ) {
		if ( Covers( uber[i].maps, level ) ) Alias_Add( uber[i].alias, uber[i].file, uber[i].parms );
	}
}

static int Finds( const char *name ) { return Alias_FindRandom( name, NULL ) != NULL; }

int main( void ) {
	AliasListNode_t *node = NULL;

	/* A level's spawn, before anything registered for it: nothing answers. */
	assert( !Finds( "snd_pickup" ) && !Finds( "jeep_snd_idle" ) );

	/* m1l3a, once its client game has registered: every miss its spawn
	 * printed now resolves, under any case, with the record the modules read. */
	Register( "m1l3a" );
	assert( Finds( "snd_pickup" ) && Finds( "jeep_snd_start" ) && Finds( "jeep_snd_idle" ) );
	assert( Finds( "flak_snd_move" ) && Finds( "SND_PICKUP" ) );
	assert( !strcmp( Alias_FindRandom( "snd_pickup", &node ), "sound/weapons/foley/Pickup_01.wav" ) );
	assert( node && node->channel == MOHAA_CHAN_ITEM && node->volume > 1.29f && node->volume < 1.31f );
	assert( node->maxDist > 999.0f && node->maxDist < 1001.0f && !node->streamed );
	/* The genuine gaps stay: no alias of those names covers m1l3a. */
	assert( !Finds( "king_snd_idle" ) && !Finds( "pickup_explosives" ) && !Finds( "m3l2_radio_pickup" ) );

	/* A restart keeps the list, and its client game's parse adds nothing twice. */
	const int entries = Alias_GetGlobalList()->num_in_list;
	Register( "m1l3a" );
	assert( Alias_GetGlobalList()->num_in_list == entries );

	/* The next level's map empties it: m1l3b's own aliases resolve once
	 * registered, m1l3a's alone do not. */
	Alias_Clear();
	assert( Alias_GetGlobalList()->num_in_list == 0 && blocks == 1 );
	assert( !Finds( "snd_pickup" ) );
	Register( "m1l3b" );
	assert( Finds( "snd_pickup" ) && Finds( "jeep_snd_idle" ) && Finds( "flak_snd_move" ) );
	assert( !Finds( "jeep_snd_start" ) );

	/* A name only numbered variants carry picks one of them where the level
	 * registered them, and is missing where it did not. */
	Alias_Clear();
	Register( "m3l3" );
	const char *explosives = Alias_FindRandom( "pickup_explosives", NULL );
	assert( explosives && !strncmp( explosives, "sound/items/Item_BangaloreAssemble_0", 36 ) );
	assert( !Finds( "m3l2_radio_pickup" ) );
	/* A longer name that goes on with '_' is another alias, never a variant:
	 * the pistol's pickup is not its ammunition's. */
	for ( int i = 0; i < 32; i++ ) {
		assert( !strcmp( Alias_FindRandom( "colt45_snd_pickup", NULL ), "sound/weapons/foley/SilencerPickup1.wav" ) );
	}
	Alias_Clear();
	Register( "m2l1" );
	assert( !Finds( "pickup_explosives" ) && !Finds( "m3l2_radio_pickup" ) && Finds( "snd_pickup" ) );
	Alias_Clear();
	Register( "m5l2a" );
	assert( Finds( "king_snd_idle" ) && Finds( "flak_snd_move" ) );

	/* Names past a record: an alias name holds 39 characters and a longer one
	 * is neither added nor looked up past the lookup's 40-byte buffer; a real
	 * name past 127 and a list's name past 39 are cut and terminated. */
	Alias_Clear();
	static char tooLong[1000];
	char fits[64], path[200];
	memset( tooLong, 'a', sizeof( tooLong ) - 1 );
	memset( fits, 'b', 39 );
	fits[39] = 0;
	assert( !Alias_Add( tooLong, "sound/too_long.wav", "" ) );
	assert( Alias_GetGlobalList()->num_in_list == 0 );
	assert( !Alias_Find( tooLong ) && !Alias_FindRandom( tooLong, NULL ) );
	assert( Alias_Add( fits, "sound/fits.wav", "" ) );
	assert( Alias_Find( fits ) && !strcmp( Alias_FindRandom( fits, NULL ), "sound/fits.wav" ) );
	memcpy( path, "sound/", 6 );
	memset( path + 6, 'c', sizeof( path ) - 7 );
	path[sizeof( path ) - 1] = 0;
	assert( Alias_Add( "long_path", path, "" ) );
	assert( strlen( Alias_FindRandom( "long_path", NULL ) ) == MAX_REAL_NAME_LENGTH - 1 );
	assert( strlen( AliasList_New( path )->name ) == MAX_ALIASLIST_NAME_LENGTH - 1 );
	assert( !Alias_Add( NULL, "sound/x.wav", "" ) && !Alias_Add( "no_file", NULL, "" ) );
	return 0;
}
'''
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='mohaa-alias-', dir=ROOT / '.tmp') as folder:
            source = Path(folder) / 'alias_probe.c'
            source.write_text(harness, encoding='utf-8')
            binary = Path(folder) / ('alias_probe.exe' if os.name == 'nt' else 'alias_probe')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c99',
                                    '-I', str(ROOT / 'code/mohaa/qcommon'), str(source), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
