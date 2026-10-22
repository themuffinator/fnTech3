"""Elite Force SP's game host: the function-table boundary, its records and
the gentity view.

Retail stvoy.exe loads efgamex86.dll, passes GetGameAPI 42 imports in a fixed
order (SV_InitGameProgs 0x425490), attaches the cgame half of the same
library before the game's Init, and requires API version 6. The game's
gentity has an entityState, a client pointer, the engine's link fields and an
owner pointer, and no linkcount. The probes compile the record adapter and the
server's gentity view; the source checks pin the host's wiring.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

RECORDS = r'''
#include "code/qcommon/q_shared.h"
#include "code/compat/stvef_state_adapter.hpp"
#include <cassert>
#include <cstring>
#include <initializer_list>
using namespace fntech3::stvef::state;
template<class T> static void Fill( T &record ) {
	auto *bytes = reinterpret_cast<unsigned char *>( &record );
	for ( std::size_t i = 0; i < sizeof( record ); ++i ) bytes[i] = static_cast<unsigned char>( i * 7 + 3 );
}
int main() {
	// Every entity field survives the engine's state, the SP extras included.
	fntech3_stvef_sp_entityState_t entity;
	Fill( entity );
	entity.pos.trType = 2;
	entity.apos.trType = 5;
	entityState_t internalEntity{};
	assert( ToInternal( entity, internalEntity ) );
	assert( internalEntity.stvef.modelindex3 == entity.modelindex3 );
	assert( internalEntity.stvef.torsoAnimTimer == entity.torsoAnimTimer );
	fntech3_stvef_sp_entityState_t entityBack{};
	assert( FromInternal( internalEntity, entityBack ) );
	assert( !std::memcmp( &entity, &entityBack, sizeof( entity ) ) );
	entity.pos.trType = 6;
	assert( !ToInternal( entity, internalEntity ) );

	// Every player field, the narrow ones through the engine's ints.
	fntech3_stvef_sp_playerState_t player;
	Fill( player );
	player.useTimePad = 0;
	player.leanofsPad = 0;
	std::memset( player.tailPad, 0, sizeof( player.tailPad ) );
	player.useTime = -2;
	player.leanofs = -5;
	player.friction = 300;
	player.leanStopDebounceTime = 200;
	playerState_t internalPlayer{};
	assert( ToInternal( player, internalPlayer ) );
	assert( internalPlayer.stvef.leanofs == -5 && internalPlayer.stvef.friction == 300 );
	assert( internalPlayer.stvef.leanStopDebounceTime == 200 && internalPlayer.stvef.useTime == -2 );
	assert( internalPlayer.legsTimer == player.legsAnimTimer && internalPlayer.ammo[3] == player.ammo[3] );
	assert( internalPlayer.stvef.borgAdaptHits[31] == player.borgAdaptHits[31] );
	fntech3_stvef_sp_playerState_t playerBack{};
	assert( FromInternal( internalPlayer, playerBack ) );
	assert( !std::memcmp( &player, &playerBack, sizeof( player ) ) );
	// An engine value the record cannot hold is refused, not truncated.
	for ( int bad : { 128, -129 } ) {
		playerState_t wide = internalPlayer; wide.stvef.leanofs = bad;
		assert( !FromInternal( wide, playerBack ) );
	}
	for ( int bad : { 40000, -40000 } ) {
		playerState_t wide = internalPlayer; wide.stvef.friction = bad;
		assert( !FromInternal( wide, playerBack ) );
		wide = internalPlayer; wide.stvef.useTime = bad;
		assert( !FromInternal( wide, playerBack ) );
	}
	for ( int bad : { -1, 256 } ) {
		playerState_t wide = internalPlayer; wide.stvef.leanStopDebounceTime = bad;
		assert( !FromInternal( wide, playerBack ) );
	}

	// Commands: a full-width buttons field.
	fntech3_stvef_sp_usercmd_t command;
	Fill( command );
	std::memset( command.weaponPad, 0, sizeof( command.weaponPad ) );
	command.tailPad = 0;
	command.buttons = 0x12345678;
	usercmd_t internalCommand{};
	assert( ToInternal( command, internalCommand ) && internalCommand.buttons == 0x12345678 );
	fntech3_stvef_sp_usercmd_t commandBack{};
	assert( FromInternal( internalCommand, commandBack ) );
	assert( !std::memcmp( &command, &commandBack, sizeof( command ) ) );
}
'''

VIEW = r'''
#include "code/server/server_game_data.h"
#include "code/compat/stvef_game_abi.h"
#include <cassert>
#include <cstring>
#include <vector>
using namespace fntech3::server;
int main() {
	// The retail game's gentity is 1056 bytes; the engine sees its prefix.
	constexpr std::size_t stride = 1056;
	static_assert( stride >= sizeof( fntech3_stvef_sp_gentity_t ) );
	std::vector<std::uint32_t> storage( stride * 1024 / 4 );
	auto *arena = reinterpret_cast<unsigned char *>( storage.data() );
	auto entity = [&]( int n ) { return reinterpret_cast<fntech3_stvef_sp_gentity_t *>( arena + n * stride ); };
	NativeGameDataBinding binding;
	binding.entityBase = arena;
	binding.entityCount = 5;
	binding.entityCapacity = 1024;
	binding.entityStride = stride;
	binding.entitySpanBytes = stride * 1024;
	binding.clientCount = 1;
	ServerGameDataView view;
	assert( view.BindNative( COM_GAME_STVEF_SP, binding ) == GameDataBindError::None );
	assert( view.EntityCount() == 5 );

	// The whole g_entities array is addressable, beyond the high-water mark.
	GameEntityRef reference;
	assert( view.EntityRefAt( 900, reference ) && view.EntityRefForPointer( entity( 900 ), reference ) );
	assert( !view.EntityRefAt( 1024, reference ) );
	assert( view.EntityRefAt( 3, reference ) );

	// Owners are gentity pointers.
	std::int32_t owner = 0;
	entity( 3 )->owner = entity( 2 );
	assert( view.GetOwnerNum( reference, owner ) && owner == 2 );
	entity( 3 )->owner = nullptr;
	assert( view.GetOwnerNum( reference, owner ) && owner == ENTITYNUM_NONE );
	assert( view.SetOwnerNum( reference, 4 ) && entity( 3 )->owner == entity( 4 ) );
	assert( view.SetOwnerNum( reference, ENTITYNUM_NONE ) && entity( 3 )->owner == nullptr );
	assert( !view.SetOwnerNum( reference, 2000 ) );
	// A pointer that names no gentity reads as no owner and survives a write-back.
	auto *foreign = reinterpret_cast<fntech3_stvef_sp_gentity_t *>( arena + 8 );
	entity( 3 )->owner = foreign;
	assert( view.GetOwnerNum( reference, owner ) && owner == ENTITYNUM_NONE );
	assert( view.SetOwnerNum( reference, owner ) && entity( 3 )->owner == foreign );

	// No linkcount or singleClient in the record: the engine keeps its own.
	std::vector<unsigned char> before( arena, arena + stride * 5 );
	std::int32_t value = -1;
	assert( view.SetLinkcount( reference, 7 ) && view.GetLinkcount( reference, value ) && value == 7 );
	assert( view.SetSingleClient( reference, 3 ) && view.GetSingleClient( reference, value ) && value == 0 );
	assert( std::equal( before.begin(), before.end(), arena ) );

	// Fields at their record offsets.
	entity( 3 )->svFlags = 0x21;
	entity( 3 )->contents = 1;
	assert( view.GetSvFlags( reference, value ) && value == 0x21 );
	assert( view.GetContents( reference, value ) && value == 1 );
	entity( 3 )->s.number = 3;
	entity( 3 )->s.modelindex3 = 44;
	entity( 3 )->s.origin2[1] = 2.5f;
	entityState_t state{};
	assert( view.ReadEntityState( reference, state ) && state.number == 3 && state.stvef.modelindex3 == 44 );
	float origin2[3] = {};
	assert( view.GetSharedStateOrigin2( reference, origin2 ) && origin2[1] == 2.5f );
	state.stvef.modelindex3 = 45;
	assert( view.WriteEntityState( reference, state ) && entity( 3 )->s.modelindex3 == 45 );

	// Player state behind gentity 0's client pointer.
	playerState_t player{};
	assert( !view.ReadPlayerState( 0, player ) );
	fntech3_stvef_sp_playerState_t client{};
	client.origin[2] = 64.0f;
	client.ping = 77;
	client.borgAdaptHits[5] = 9;
	entity( 0 )->client = &client;
	assert( view.ReadPlayerState( 0, player ) && player.origin[2] == 64.0f && player.ping == 77 );
	assert( player.stvef.borgAdaptHits[5] == 9 );
	assert( view.SetPlayerPing( 0, 12 ) && client.ping == 12 );
	assert( !view.ReadPlayerState( 1, player ) );

	// One client at most.
	binding.clientCount = 2;
	assert( view.BindNative( COM_GAME_STVEF_SP, binding ) == GameDataBindError::InvalidClientCount );
}
'''


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def build_and_run(test, source, extra=()):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler unavailable')
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target:
        target = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='stvef-sp-host-', dir=ROOT / '.tmp') as folder:
        cpp = Path(folder) / 'probe.cpp'
        binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT),
                                str(cpp), *extra, '-o', str(binary)], cwd=ROOT, capture_output=True, text=True,
                               timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


class STVEFSPHostTests(unittest.TestCase):
    def test_initial_clock_allows_delayed_reference_tags(self):
        spawn = read('code/server/sv_init.cpp')
        start = spawn.index('\tsv.state = SS_LOADING;')
        end = spawn.index('\tSV_InitGameProgs();', start) + len('\tSV_InitGameProgs();')
        # Compile the actual pre-Init phase. A reference tag that schedules a
        # fractional millisecond delay must not become the zero/no-think value.
        probe = r'''
#include <cassert>
#include <cstring>
#include <initializer_list>
enum { Q3, QL, RTCW_SP, RTCW_MP, ET, EF_SP, EF_MP, SS_LOADING, ERR_DROP, CS_SERVERINFO, CVAR_SERVERINFO };
static int profile, initializedAt, restoreTime, restartMarkers;
static bool restored;
static struct { int time, state; } sv;
static bool Com_GameIsSTVEFSP() { return profile == EF_SP; }
// Of these profiles only Elite Force SP starts each level at 1000
// (FRAME_LEVEL_TIME_1000); tests/jedi_level_time_tests.py pins the table.
static bool SV_LevelTimeStartsAt1000() { return profile == EF_SP; }
static bool Com_GameIsWolfET() { return profile == ET; }
// The IHV test, Q3Test and the Ritual engines start their levels their own
// way, which their own tests cover; none of the profiles here is one.
static bool Com_GameIsQuake3IHV() { return false; }
static bool Com_GameIsQ3Test() { return false; }
static bool Com_GameIsRitual() { return false; }
struct Q3TestRelease { int protocol; };
static const Q3TestRelease *Com_Q3TestRelease() { static const Q3TestRelease release{ 40 }; return &release; }
static void SV_UpdateSysteminfoConfigstring() { assert(false); }
static void SV_SetConfigstring(int, const char *) { assert(false); }
static const char *Cvar_InfoString(int, void *) { assert(false); return ""; }
static void SV_AliceRestoreLevelTime() { assert(false); }
static bool SV_RTCWRestoreSaveTime() {
    restored = true;
    if (profile == RTCW_SP && restoreTime >= 0) sv.time = restoreTime;
    return true;
}
static void Com_Error(int, const char*) { assert(false); }
static void Cmd_TokenizeString(const char*) { assert(restored); }
static void Cvar_Set(const char* name, const char* value) {
    assert(profile == ET && !std::strcmp(name, "sv_serverRestarting") && !std::strcmp(value, "1"));
    ++restartMarkers;
}
static void SV_InitGameProgs() { assert(restored); initializedAt = sv.time; }
static void Spawn() {
    const char* spawnCommand = "devmap probe";
SPAWN_PHASE
}
int main() {
    for (profile = Q3; profile <= EF_MP; ++profile) {
        for (int previous : {0, 73000}) {
            sv.time = previous; restoreTime = -1; restored = false; restartMarkers = 0;
            Spawn();
            assert(sv.state == SS_LOADING && restored && restartMarkers == (profile == ET));
            if (profile == EF_SP) {
                assert(initializedAt == 1000); // stvoy.exe 1.20, 0x425ac9
                int deadline = static_cast<int>(initializedAt + 0.1);
                bool registered = false;
                for (int frame = 1; frame <= 3; ++frame) {
                    if (deadline > 0 && deadline <= initializedAt + frame * 100) registered = true;
                }
                assert(registered);
            } else {
                assert(initializedAt == previous);
            }
        }
    }
    profile = RTCW_SP; sv.time = 0; restoreTime = 42150; restored = false;
    Spawn();
    assert(initializedAt == 42150);
}
'''.replace('SPAWN_PHASE', spawn[start:end])
        build_and_run(self, probe)

    def test_records_cross_the_engine_state_whole(self):
        build_and_run(self, RECORDS)

    def test_gentity_view(self):
        build_and_run(self, VIEW, [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
                                   str(ROOT / 'code/compat/game_abi_adapter.c')])

    def test_host_follows_retail(self):
        host = read('code/server/server_stvef_sp_game.hpp')
        imports = function(host, 'void SV_STVEFSPFillImports(')
        api = read('code/compat/stvef_sp_game_api.h')
        table = api[api.index('typedef struct fntech3_stvef_sp_game_import_s'):api.index('} fntech3_stvef_sp_game_import_t;')]
        fields = re.findall(r'\(\*(\w+)\)\(|\*(S_Override);', table)
        names = [a or b for a, b in fields]
        self.assertEqual(len(names), 42)
        for name in names:
            self.assertIn(f'import.{name} = ', imports)
        load = function(host, 'vm_t *SV_STVEFSPLoadGame()')
        # The cgame half is attached before the version check and Init.
        self.assertLess(load.index('getGameAPI( &stvefSpImport )'), load.index('( entry )( SV_STVEFSPCgameSyscall )'))
        self.assertLess(load.index('( entry )( SV_STVEFSPCgameSyscall )'),
                        load.index('game->apiversion != FNTECH3_STVEF_SP_GAME_API_VERSION'))
        self.assertIn('"game is version %i, not %i"', load)
        entry = function(host, 'intptr_t QDECL SV_STVEFSPGameEntry(')
        self.assertIn('Cvar_VariableIntegerValue( "sv_mapChecksum" )', entry)
        self.assertIn('stvefSpSavedGameJustLoaded, stvefSpLoadTransition', entry)
        game = read('code/server/sv_game.cpp')
        self.assertIn('#include "server_stvef_sp_game.hpp"', game)
        self.assertIn('gvm = Com_GameIsSTVEFSP() ? SV_STVEFSPLoadGame()', game)
        vm = read('code/qcommon/vm.c')
        self.assertIn('vm_t *VM_CreateTable( vmIndex_t index, const char *symbolName, vmMainFunc_t dispatcher, void **symbol )', vm)

    def test_engine_reads_four_sp_server_flags(self):
        game = read('code/server/sv_game.cpp')
        table = game[game.index('constexpr ServerFlagMapping stvefSpServerFlags[]'):]
        table = table[:table.index('};')]
        self.assertEqual(re.findall(r'\{ FNTECH3_STVEF_SP_SVF_(\w+), SVF_(\w+) \}', table),
                         [('NOCLIENT', 'NOCLIENT'), ('BOT', 'BOT'), ('BROADCAST', 'BROADCAST'), ('PORTAL', 'PORTAL')])


if __name__ == '__main__':
    unittest.main()
