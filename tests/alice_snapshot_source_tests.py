"""American McGee's Alice's snapshot walk, and the load and spawn steps around it.

Retail alice.exe builds each client's snapshot from its game's own records
(SV_BuildClientSnapshot 0x00456f20, SV_AddEntitiesVisibleFromPoint 0x004571e0,
SV_AddEntToSnapshot 0x00457150): the client's own entity first, a child judged
by its parent, the sky portal's and every portal's view, and render flags
rewritten on the entities for each. A load of the level that is running keeps
the server's configstrings (SV_SpawnServer's reuse path, 0x004550a1..0x004550db);
the model import wrappers answer their own defaults for an index with no model
(0x00453960..0x004540f0); and `map`, `loadgame` and `loadbrokengame` stop a
playing cinematic before they spawn (0x00451118, 0x00451c53, 0x00451b09).

These checks pin that all of it is gated on the Alice profile and that every
other profile's snapshot path is the one it had; the walk itself is exercised by
tests/alice_snapshot_tests.cpp. See docs/fntech3/2026-09-22-alice-wire.md and
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


def ordered(test, text, parts):
    positions = [text.index(part) for part in parts]
    test.assertEqual(positions, sorted(positions), parts)


ALICE_BRANCH = (
    "\t// American McGee's Alice walks the game's own records and stores what\n"
    "\t// each client's walk chose (SV_BuildAliceClientSnapshot).\n"
    "\tif ( Com_GameIsAlice() ) {\n"
    "\t\tSV_BuildAliceClientSnapshot( client, frame );\n"
    "\t\treturn;\n"
    "\t}\n\n"
)


WRAPPER_PRELUDE = r'''
#include "code/qcommon/alice_tiki.h"
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
const vec3_t vec3_origin = { 0, 0, 0 };
static std::vector<std::string> calls;
static std::vector<std::string> prints;
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] ) {
	assert( angles[0] == 0 && angles[1] == 0 && angles[2] == 0 );
	for ( int i = 0; i < 3; ++i ) for ( int j = 0; j < 3; ++j ) axis[i][j] = i == j ? 1.0f : 0.0f;
}
void QDECL Com_DPrintf( const char *fmt, ... ) {
	char text[256];
	va_list list;
	va_start( list, fmt );
	vsnprintf( text, sizeof( text ), fmt, list );
	va_end( list );
	prints.push_back( text );
}
/* Index 0 has no definition, as nothing registers it; index n is handle n - 1. */
static int SV_AliceTikiForIndex( int index ) { return index - 1; }
#define CALLED calls.push_back( __func__ )
const char *AliceTiki_Name( int ) { CALLED; return "name"; }
int AliceTiki_NumAnims( int ) { CALLED; return 7; }
int AliceTiki_NumSurfaces( int ) { CALLED; return 7; }
int AliceTiki_NumTags( int ) { CALLED; return 7; }
void AliceTiki_CalculateBounds( int, float, vec3_t mins, vec3_t maxs ) { CALLED; mins[0] = maxs[0] = 7; }
qboolean AliceTiki_InitCommands( int, qboolean, fntech3_alice_tiki_cmd_t * ) { CALLED; return qtrue; }
const char *AliceTiki_AnimName( int, int ) { CALLED; return "anim"; }
int AliceTiki_AnimNumForName( int, const char * ) { CALLED; return 7; }
int AliceTiki_AnimRandom( int, const char * ) { CALLED; return 7; }
int AliceTiki_AnimNumFrames( int, int ) { CALLED; return 7; }
float AliceTiki_AnimTime( int, int ) { CALLED; return 7.0f; }
void AliceTiki_AnimDelta( int, int, vec3_t delta ) { CALLED; delta[0] = 7; }
void AliceTiki_AnimAbsoluteDelta( int, int, vec3_t delta ) { CALLED; delta[0] = 7; }
int AliceTiki_AnimFlags( int, int ) { CALLED; return 7; }
qboolean AliceTiki_AnimHasCommands( int, int, qboolean ) { CALLED; return qtrue; }
qboolean AliceTiki_FrameCommands( int, int, int, qboolean, fntech3_alice_tiki_cmd_t * ) { CALLED; return qtrue; }
void AliceTiki_FrameDelta( int, int, int, vec3_t delta ) { CALLED; delta[0] = 7; }
float AliceTiki_FrameTime( int, int, int ) { CALLED; return 7.0f; }
void AliceTiki_FrameBounds( int, int, int, float, vec3_t mins, vec3_t ) { CALLED; mins[0] = 7; }
int AliceTiki_SurfaceNameToNum( int, const char * ) { CALLED; return 7; }
const char *AliceTiki_SurfaceNumToName( int, int ) { CALLED; return "surface"; }
int AliceTiki_SurfaceFlags( int, int ) { CALLED; return 7; }
int AliceTiki_SurfaceNumSkins( int, int ) { CALLED; return 7; }
int AliceTiki_TagNumForName( int, const char * ) { CALLED; return 7; }
const char *AliceTiki_TagNameForNum( int, int ) { CALLED; return "tag"; }
void AliceTiki_TagOrientation( int, int, int, int, float, const fntech3_alice_bonectrl_t *,
	float origin[3], float axis[3][3] ) { CALLED; origin[0] = 7; axis[0][0] = 7; }
void AliceTiki_TagOrientationEx( int, int, int, int, float, const fntech3_alice_bonectrl_t *,
	int, int, float, qboolean, qboolean, int, int, int, int, float, float origin[3], float axis[3][3] ) {
	CALLED; origin[0] = 7; axis[0][0] = 7;
}
qboolean AliceAlias_Add( int, const char *, const char *, const char * ) { CALLED; return qtrue; }
const char *AliceAlias_FindRandom( int, const char * ) { CALLED; return "alias"; }
void AliceAlias_Dump( int ) { CALLED; }
const char *AliceAlias_FindDialog( int, const char *, int, int ) { CALLED; return "dialog"; }
void AliceAlias_UpdateDialog( int, const char *, int, int, int, int ) { CALLED; }
static fntech3::alice::alias::List *listStub;
fntech3::alice::alias::List *AliceAlias_GetList( int ) { CALLED; return listStub; }
'''

WRAPPER_CHECKS = r'''
int main() {
	fntech3_alice_tiki_cmd_t commands{};
	float mins[3] = { 5, 5, 5 }, maxs[3] = { 5, 5, 5 }, delta[3] = { 5, 5, 5 };
	/* Index 0: every wrapper answers its retail default and nothing reaches the TIKI code. */
	assert( SV_AliceNumAnims( 0 ) == 0 && SV_AliceNumSurfaces( 0 ) == 0 && SV_AliceNumTags( 0 ) == 0 );
	assert( !SV_AliceInitCommands( 0, &commands ) );
	SV_AliceCalculateBounds( 0, 1.0f, mins, maxs );
	assert( mins[0] == 5 && maxs[0] == 5 );
	assert( !SV_AliceAnimNameForNum( 0, 1 ) );
	assert( SV_AliceAnimNumForName( 0, "idle" ) == -1 && SV_AliceAnimRandom( 0, "idle" ) == -1 );
	assert( SV_AliceAnimNumFrames( 0, 1 ) == 0 && SV_AliceAnimTime( 0, 1 ) == 0.0f );
	SV_AliceAnimDelta( 0, 1, delta );
	assert( delta[0] == 0 && delta[1] == 0 && delta[2] == 0 );
	delta[0] = delta[1] = delta[2] = 5;
	SV_AliceAnimAbsoluteDelta( 0, 1, delta );
	assert( delta[0] == 0 && delta[1] == 0 && delta[2] == 0 );
	assert( SV_AliceAnimFlags( 0, 1 ) == 0 && !SV_AliceAnimHasCommands( 0, 1 ) );
	assert( !SV_AliceFrameCommands( 0, 1, 2, &commands ) );
	delta[0] = 5;
	SV_AliceFrameDelta( 0, 1, 2, delta );
	assert( delta[0] == 5 );
	assert( SV_AliceFrameTime( 0, 1, 2 ) == 0.0f );
	SV_AliceFrameBounds( 0, 1, 2, 1.0f, mins, maxs );
	assert( mins[0] == 5 );
	assert( SV_AliceSurfaceNameToNum( 0, "s" ) == -1 && !SV_AliceSurfaceNumToName( 0, 1 ) );
	assert( SV_AliceSurfaceFlags( 0, 1 ) == 0 && SV_AliceSurfaceNumSkins( 0, 1 ) == 0 );
	assert( SV_AliceTagNumForName( 0, "t" ) == -1 && !SV_AliceTagNameForNum( 0, 1 ) );
	AliceOrientation orientation;
	std::memset( &orientation, 0x7f, sizeof( orientation ) );
	assert( SV_AliceTagOrientation( &orientation, 0, 1, 2, 3, 1.0f, nullptr ) == &orientation );
	assert( orientation.origin[0] == 0 && orientation.origin[2] == 0 );
	assert( orientation.axis[0][0] == 1 && orientation.axis[1][1] == 1 && orientation.axis[2][2] == 1 );
	assert( orientation.axis[0][1] == 0 && orientation.axis[2][0] == 0 );
	std::memset( &orientation, 0x7f, sizeof( orientation ) );
	SV_AliceTagOrientationEx( &orientation, 0, 1, 2, 3, 1.0f, nullptr, 0, 0, 0.0f, 0, 0, 0, 0, 0, 0, 0.0f );
	assert( orientation.axis[1][1] == 1 && orientation.origin[1] == 0 );
	assert( ( prints == std::vector<std::string>{ "PF_Tag_Orientation: not a tiki model\n",
		"PF_Tag_OrientationEx: not a tiki model\n" } ) );
	assert( !SV_AliceAliasAdd( 0, "a", "b", "" ) && !SV_AliceAliasFindRandom( 0, "a" ) );
	SV_AliceAliasDump( 0 );
	assert( !SV_AliceAliasFindDialog( 0, "a", 0, 1 ) && !SV_AliceAliasGetList( 0 ) );
	SV_AliceAliasUpdateDialog( 0, "a", 1, 2, 3 );
	SV_AliceAliasAddActorDialog( 0, "a", 4, 1, 2, 3 );
	assert( !SV_AliceNameForNum( 0 ) );
	assert( calls.empty() );

	/* An index with a definition reaches each routine once. */
	assert( SV_AliceNumAnims( 3 ) == 7 && SV_AliceAnimNumForName( 3, "idle" ) == 7 );
	assert( SV_AliceAnimTime( 3, 1 ) == 7.0f && std::string( SV_AliceNameForNum( 3 ) ) == "name" );
	SV_AliceAnimDelta( 3, 1, delta );
	assert( delta[0] == 7 );
	SV_AliceTagOrientation( &orientation, 3, 1, 2, 3, 1.0f, nullptr );
	assert( orientation.origin[0] == 7 );
	assert( std::string( SV_AliceAliasFindDialog( 3, "a", 0, 1 ) ) == "dialog" );
	assert( calls.size() == 7 );
	return 0;
}
'''


class AliceSnapshotSourceTests(unittest.TestCase):
    def setUp(self):
        self.snapshot = read('code/server/sv_snapshot.cpp')
        self.walk = read('code/server/server_alice_snapshot.hpp')
        self.game = read('code/server/server_alice_game.hpp')
        self.save = read('code/server/server_alice_save.hpp')
        self.header = read('code/server/server.h')
        self.init = read('code/server/sv_init.cpp')

    def test_other_profiles_keep_their_snapshot_path(self):
        build = function(self.snapshot, 'static void SV_BuildClientSnapshot( client_t *client ) {')
        # One branch, after the entity check and before the common frame is built.
        self.assertEqual(build.count('Com_GameIsAlice()'), 1)
        ordered(self, build, ['if ( !client->gentity ) {', ALICE_BRANCH,
                              'SV_BuildCommonSnapshot();', 'SV_AddEntitiesVisibleFromPoint( org, frame, entityNumbers, false );'])
        rest = build.replace(ALICE_BRANCH, '')
        self.assertNotIn('lice', rest)
        for signature in ('static void SV_AddEntitiesVisibleFromPoint( const vec3_t origin',
                          'static void SV_BuildCommonSnapshot( void )',
                          'static void SV_SortEntityNumbers( SnapshotEntityNumbers &numbers ) {',
                          'static void SV_SnapshotViewOrigin( const client_t *client',
                          'static void SV_AddAudioReachableEntities( const vec3_t origin',
                          'static bool SV_EntityPassesSnapshotClientFlags( const sharedEntity_t &entity'):
            self.assertNotIn('lice', function(self.snapshot, signature), signature)
        numbers = self.snapshot[self.snapshot.index('struct SnapshotEntityNumbers {'):]
        numbers = numbers[:numbers.index('\n};') + 3]
        self.assertNotIn('lice', numbers)
        self.assertIn('const int limit = Com_GameIsWolfET() ? MAX_SERVER_SNAPSHOT_ENTITIES : MAX_SNAPSHOT_ENTITIES;',
                      numbers)
        # The module's records stay behind their accessors outside the Alice walk.
        self.assertNotIn('SV_GentityNum(', self.snapshot)

    def test_alice_snapshots_are_built_in_retail_order(self):
        build = function(self.snapshot, 'static void SV_BuildAliceClientSnapshot( client_t *client, clientSnapshot_t *frame ) {')
        ordered(self, build, ['sv.snapshotCounter++;', 'snapshot::ViewOrigin( frame->ps.pm_flags',
                              'world.Entity( self )', 'snapshot::AddEntToSnapshot( *ownEntity, own, numbers, sv.snapshotCounter, false, false );',
                              'snapshot::AddEntitiesVisibleFromPoint( world, origin, numbers, nullptr, false );',
                              'snapshot::SortNumbers( numbers )', 'areaWords[areaWord] ^= -1;',
                              'SV_StoreAliceSnapshotFrame( frame, numbers );'])
        self.assertIn('"SV_QsortEntityStates: duplicated entity"', build)
        self.assertIn('const int self = SV_ClientIndex( client );', build)
        world = self.snapshot[self.snapshot.index('struct AliceSnapshotWorld {'):]
        world = world[:world.index('\n};') + 3]
        for part in ('return SV_AliceGameEntity( number );', 'return sv.aliceFarPlane;',
                     'return sv.aliceSkyPortal != qfalse;', 'frame->areabytes = CM_WriteAreaBits( frame->areabits, area );',
                     'Com_Error( ERR_DROP, "%s", message );'):
            self.assertIn(part, world)
        store = function(self.snapshot, 'static void SV_StoreAliceSnapshotFrame( clientSnapshot_t *frame,')
        # The states are read after the walk, so they carry the flags it wrote.
        ordered(self, store, ['svs.snapshotFrame++;', 'SV_ReadGameEntityByNum( numbers.numbers[ i ], &entity )',
                              'svs.snapshotEntities[ index ] = entity.s;', 'frame->ents[ i ] = sf->ents[ i ];',
                              'frame->frameNum = sf->frameNum;'])

    def test_the_walk_is_retail_s(self):
        walk = self.walk
        self.assertIn('kEntityLimit = 0x400', walk)
        self.assertIn('kGlobalLoopSoundBits = 0x46ea6000u', walk)
        self.assertIn('kCameraViewFlag = 0x100u', walk)
        self.assertIn('"SV_AddEntitiesVisibleFromPoint: duplicate sky origin"', walk)
        self.assertIn('"SV_SvEntityForGentity: bad gEnt"', walk)
        body = walk[walk.index('void AddEntitiesVisibleFromPoint( World &world'):]
        ordered(self, body, ['ReadInt( entity, kInuse )', 'FNTECH3_ALICE_SVF_ENGINE_VISITED',
                             'ReadInt( entity, kLinked )', 'parentNumber = ReadInt( entity, kBindParent );',
                             'FNTECH3_ALICE_RF_DONTDRAW', 'FNTECH3_ALICE_RF_SKYORIGIN',
                             'FNTECH3_ALICE_SVF_BROADCAST | FNTECH3_ALICE_SVF_BROADCAST_ALT',
                             'parentEntity->snapshotCounter == counter', 'FNTECH3_ALICE_SVF_ALWAYS_RELEVANT',
                             'kGlobalLoopSoundBits', 'world.AreasConnected( area, visibility->areanum )',
                             'BeyondFarPlane(', 'PvsVisible(', 'MarkDual( entity );',
                             'FNTECH3_ALICE_SVF_PORTAL ) && svEntity != portal',
                             'ReadVector( skyEntity, kOrigin, origin );'])

    def test_far_plane_and_sky_portal_live_in_the_server_state(self):
        server = self.header[self.header.index('typedef struct {\n\tserverState_t\tstate;'):]
        server = server[:server.index('} server_t;')]
        self.assertIn('int\t\t\t\taliceFarPlane;', server)
        self.assertIn('qboolean\t\taliceSkyPortal;', server)
        self.assertIn('unsigned char *SV_AliceGameEntity( int number );', self.header)
        self.assertIn('sv.aliceFarPlane = fntech3::alice::snapshot::SquaredFarPlane( farPlane );', self.game)
        self.assertIn('sv.aliceSkyPortal = enabled ? qtrue : qfalse;', self.game)
        self.assertNotIn('float aliceFarPlane;', self.game)
        entity = function(self.game, 'unsigned char *SV_AliceGameEntity( int number ) {')
        self.assertIn('serverGameData.EntityRefAt( number, reference )', entity)
        self.assertIn('!Com_GameIsAlice()', entity)
        # Defined outside the file's anonymous namespace, so sv_snapshot.cpp links it.
        self.assertGreater(self.game.index('unsigned char *SV_AliceGameEntity( int number ) {'),
                           self.game.index('} // namespace\n'))

    def test_a_load_of_the_running_level_keeps_its_configstrings(self):
        prepare = function(self.save, 'const char *SV_AlicePrepareSpawn( const char *server, char *bsp, int size ) {')
        ordered(self, prepare, ['SV_AliceBspName( server, bsp, size );', 'aliceKeptConfigstrings.clear();',
                                'if ( aliceLoadPending && gvm && aliceSpawnedBsp[0] && !Q_stricmp( bsp, aliceSpawnedBsp ) ) {',
                                'aliceKeptConfigstrings.emplace_back(',
                                'Q_strncpyz( aliceSpawnedBsp, bsp, sizeof( aliceSpawnedBsp ) );'])
        restore = function(self.save, 'void SV_AliceRestoreConfigstrings( void ) {')
        ordered(self, restore, ['aliceModelTableKept = !aliceKeptConfigstrings.empty();',
                                'SV_ZFree( sv.configstrings[i] );', 'aliceKeptConfigstrings.clear();'])
        self.assertIn('void SV_AliceRestoreConfigstrings( void );', self.header)
        # After the spawn's own reset and before the module is loaded.
        ordered(self, self.init, ['SV_ClearServer();\n\tsv.maxclients = preservedMaxClients;',
                                  'sv.configstrings[index] = CopyString("");',
                                  'if ( Com_GameIsAlice() ) {\n\t\tSV_AliceRestoreConfigstrings();\n\t}',
                                  'SV_InitGameProgs();'])
        fill = function(self.game, 'void SV_AliceFillImports() {')
        ordered(self, fill, ['std::memset( aliceModelTiki, 0, sizeof( aliceModelTiki ) );',
                             'SV_AliceRegisterKeptModels();'])
        kept = function(self.game, 'void SV_AliceRegisterKeptModels() {')
        self.assertIn('aliceModelTiki[index] = AliceTiki_Register( name ) + 1;', kept)
        abort = function(self.save, 'void SV_AliceSaveAbort( void ) {')
        self.assertIn('aliceKeptConfigstrings.clear();', abort)
        self.assertIn('aliceModelTableKept = false;', abort)

    def test_map_and_loads_stop_a_cinematic_before_they_spawn(self):
        stop = function(self.save, 'void SV_AliceStopCinematic() {')
        self.assertIn('#ifndef DEDICATED\n\tif ( !com_dedicated->integer ) {\n\t\tCL_AliceStopCinematic();', stop)
        self.assertIn('#ifndef DEDICATED', self.save[:self.save.index('void CL_AliceStopCinematic( void );')][-400:])
        ordered(self, function(self.save, 'void SV_AliceMap_f() {'),
                ['SV_AliceWipeSaveGame( alicesave::CurrentName );', 'Cvar_Set( "nextmap", "" );',
                 'SV_AliceStopCinematic();', 'SV_SpawnServer( server, qfalse );'])
        ordered(self, function(self.save, 'void SV_AliceLoadGame_f() {'),
                ['Cvar_Set( "nextmap", "" );', 'SV_AliceStopCinematic();', 'SV_InitGameProgs();',
                 'SV_AliceCallLevelArchiveValid( level )', 'SV_AliceLoadArchive();'])
        ordered(self, function(self.save, 'void SV_AliceLoadBrokenGame_f() {'),
                ['SV_AliceArchiveServerFile( true, false )', 'SV_AliceCopyPersistant( true );',
                 'Cvar_Set( "nextmap", "" );', 'SV_AliceStopCinematic();', 'SV_SpawnServer( map, qfalse );'])
        # gamemap and restart have no stop in retail (callers of 0x00425770 are
        # 0x00429e69, 0x0042abf2 and the three above).
        for signature in ('void SV_AliceGameMap_f() {', 'void SV_AliceRestart_f() {'):
            self.assertNotIn('SV_AliceStopCinematic', function(self.save, signature))
        client = function(read('code/client/cl_cin.cpp'), 'void CL_AliceStopCinematic( void ) {')
        self.assertIn('if ( cls.state == CA_CINEMATIC ) {\n\t\tSCR_StopCinematic();', client)

    def test_model_wrappers_answer_retail_defaults(self):
        region = self.game[self.game.index('template<typename Result, typename Call>\nResult SV_AliceTikiCall'):]
        region = region[:region.index('\n}\n', region.index('const char *QDECL SV_AliceNameForNum( int index ) {')) + 3]
        self.assertNotIn('( SV_AliceTikiForIndex( index ),', region)
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        # Outside the x86 developer shell only the Q3-only diagnostic build is
        # valid on 64-bit Windows; these wrappers do not depend on pointer width.
        if os.name == 'nt' and not target and os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() != 'x86':
            target = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='alice-wrappers-', dir=ROOT / '.tmp') as folder:
            source = Path(folder) / 'probe.cpp'
            source.write_text(WRAPPER_PRELUDE + region + WRAPPER_CHECKS, encoding='utf-8')
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(source), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_every_alice_snapshot_ends_with_its_sound_block(self):
        write = function(self.snapshot, 'static void SV_WriteSnapshotToClient( const client_t *client, msg_t *msg ) {')
        zombie = write[write.index('if ( client->state <= CS_ZOMBIE ) {'):]
        zombie = zombie[:zombie.index('\t\treturn;\n\t}') + len('\t\treturn;\n\t}')]
        ordered(self, zombie, ['MSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );',
                               'if ( client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {\n\t\t\tSV_WriteAliceSnapshotTail( client, msg );',
                               'return;'])
        ordered(self, write, ['SV_EmitPacketEntities(', 'if ( client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {\n\t\tSV_WriteAliceSnapshotTail( client, msg );'])
        download = function(self.snapshot, 'void SV_WriteLegacyDownloadSnapshot( const client_t *client, msg_t *msg ) {')
        ordered(self, download, ['MSG_WriteBits( msg, MAX_GENTITIES - 1, GENTITYNUM_BITS );',
                                 'if ( client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {\n\t\tMSG_WriteAliceSounds( msg, nullptr, 0 );'])
        # Everything before the Alice block is the Holomatch message it was.
        self.assertNotIn('lice', download[:download.index("\t// Alice's client reads a sound block")])

    def test_alice_has_no_download(self):
        client = read('code/server/sv_client.cpp')
        find = function(client, 'static const ucmd_t *SV_FindClientCommand( const char *name )')
        ordered(self, find, ['!strcmp( ucmd.name, "wwwdl" ) && !Com_GameIsWolfET()',
                             'if ( Com_GameIsAlice() && ( ucmd.func == SV_BeginDownload_f || ucmd.func == SV_NextDownload_f',
                             'ucmd.func == SV_StopDownload_f || ucmd.func == SV_DoneDownload_f ) ) {\n\t\t\treturn nullptr;',
                             'return &ucmd;'])
        legacy = function(read('code/client/cl_main.cpp'), 'static void CL_InitLegacyDownloads( void ) {')
        ordered(self, legacy, ['if ( Com_GameIsAlice() ) {\n\t\tCL_DownloadsComplete();\n\t\treturn;\n\t}',
                               'if ( !(cl_allowDownload->integer & DLF_ENABLE) )', 'CL_NextDownload();'])

    def test_the_walk_is_registered_as_a_unit_test(self):
        meson = read('meson.build')
        self.assertIn("alice_snapshot_tests = executable('fntech3_alice_snapshot_tests',\n"
                      "  'tests/alice_snapshot_tests.cpp',", meson)
        self.assertIn("test('fntech3_alice_snapshot', alice_snapshot_tests)", meson)


if __name__ == '__main__':
    unittest.main()
