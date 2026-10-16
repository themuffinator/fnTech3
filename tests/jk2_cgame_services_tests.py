"""Jedi Outcast's client-game services and effects: the import map read from
retail jk2mp.exe's handlers, the records it hands over, and the effects engine
Outcast shares with Academy. See docs/fntech3/2026-10-16-jk2-cgame-services.md."""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]
SERVICES = ROOT / 'code/client/client_jk2_cgame_services.hpp'
EFFECTS = ROOT / 'code/client/client_jk2_cgame_effects.hpp'
ACADEMY_EFFECTS = ROOT / 'code/client/client_jka_cgame_effects.hpp'
HOST = ROOT / 'code/client/client_jk2_cgame.hpp'
RECORD = ROOT / 'docs/fntech3/2026-10-16-jk2-cgame-services.md'
COMPANION = RECORD.with_suffix('.json')


def read(path):
    return path.read_text(encoding='utf-8')


class ImportMapTests(unittest.TestCase):
    def test_shared_operations_are_the_measured_ones(self):
        services = read(SERVICES)
        # Each Outcast ordinal that reaches a shared operation, as its retail
        # handler shows (.tmp/jk2-cgame/handlers-*.txt in the record).
        expected = {
            33: 'CG_S_STARTLOCALSOUND', 34: 'CG_S_CLEARLOOPINGSOUNDS', 35: 'CG_S_ADDLOOPINGSOUND',
            36: 'CG_S_UPDATEENTITYPOSITION', 37: 'CG_S_ADDREALLOOPINGSOUND', 38: 'CG_S_STOPLOOPINGSOUND',
            39: 'CG_S_RESPATIALIZE', 42: 'CG_R_LOADWORLDMAP', 43: 'CG_R_REGISTERMODEL', 44: 'CG_R_REGISTERSKIN',
            45: 'CG_R_REGISTERSHADER', 46: 'CG_R_REGISTERSHADERNOMIP', 200: 'CG_R_CLEARSCENE',
            208: 'CG_R_SETCOLOR', 214: 'CG_R_REMAP_SHADER', 234: 'CG_KEY_GETCATCHER', 235: 'CG_KEY_SETCATCHER',
            239: 'CG_PC_FREE_SOURCE', 246: 'CG_SNAPVECTOR', 247: 'CG_CIN_PLAYCINEMATIC',
            248: 'CG_CIN_STOPCINEMATIC', 249: 'CG_CIN_RUNCINEMATIC', 250: 'CG_CIN_DRAWCINEMATIC',
            251: 'CG_CIN_SETEXTENTS',
        }
        for ordinal, operation in expected.items():
            with self.subTest(ordinal=ordinal):
                case = re.search(r'\tcase %d:.*?(?=\n\tcase |\n\tdefault)' % ordinal, services, re.S)
                self.assertIsNotNone(case)
                self.assertIn(operation, case.group(0))
        # Raven records cross through their own adapters before the shared
        # scene path reads them again.
        self.assertIn('fntech3::jk2::render::Entity( source, value )', services)
        self.assertIn('fntech3::jk2::render::View( source, value )', services)
        adapter = read(ROOT / 'code/compat/render_record_adapter.hpp')
        self.assertIn('case COM_GAME_JK2_MP: return sizeof( fntech3_jk2_mp_refEntity_t );', adapter)
        self.assertIn('case COM_GAME_JK2_MP: return sizeof( fntech3_jk2_mp_refdef_t );', adapter)

    def test_unserved_services_are_refused_by_name(self):
        services = read(SERVICES)
        for ordinal, words in ((212, 'rotated pictures'), (217, 'brush-model vertices'), (218, 'effects boundary'),
                               (227, 'forced view'), (276, 'client ROFF playback')):
            with self.subTest(ordinal=ordinal):
                start = services.index('\tcase %d:' % ordinal)
                self.assertIn(words, services[start:services.index('return JK2CgameService::Handled;', start)])
        effects = read(EFFECTS)
        self.assertIn('"polygon", "Bezier", "primitive", "sprite", "electricity"', effects)
        self.assertIn('Jedi Outcast entity effect bolt word %d is not served yet', effects)

    def test_the_hook_runs_before_any_older_case(self):
        host = read(HOST)
        call = host.index('static bool CL_JK2CgameCall(')
        hook = host.index('CL_JK2CgameServiceCall( args, operation, result )', call)
        self.assertLess(hook, host.index('CL_JK2FontCall( cgvm, args, result, 47,', call))
        cgame = read(ROOT / 'code/client/cl_cgame.cpp')
        self.assertLess(cgame.index('#include "client_jk2_cgame_services.hpp"'),
                        cgame.index('#include "client_jk2_cgame.hpp"'))
        self.assertLess(cgame.index('#include "client_jka_cgame_host.hpp"'),
                        cgame.index('#include "client_jk2_cgame_effects.hpp"'))

    def test_input_publishes_both_selections_for_outcast(self):
        finish = function(read(ROOT / 'code/client/cl_input.cpp'), 'static void CL_FinishMove( usercmd_t *cmd ) {')
        self.assertIn('} else if ( Com_GameIsJKAMP() || Com_GameIsJK2MP() ) {', finish)
        services = read(SERVICES)
        self.assertIn('fntech3::jka::command::selections.Set( static_cast<int>( args[3] ), static_cast<int>( args[4] ) );',
                      services)
        mute = function(read(ROOT / 'code/client/audio/snd_main.cpp'), 'extern "C" void S_MuteJK2Sound(')
        self.assertIn('if ( channel < 1 ) return;', mute)


class EffectsAdapterTests(unittest.TestCase):
    def test_outcast_reaches_its_own_module_and_view(self):
        effects = read(EFFECTS)
        trace = function(effects, 'static fntech3_jka_trace_t CL_JK2CgameEffectsTrace(')
        self.assertIn('request.skipEntity = -1;', trace)
        self.assertIn('request.mask = 1;', trace)
        self.assertIn('VM_Call( module, 0, FNTECH3_JK2_MP_CG_TRACE );', trace)
        decal = function(effects, 'static int CL_JK2CgameEffectsDecal(')
        self.assertIn('VM_Call( module, 0, FNTECH3_JK2_MP_CG_IMPACT_MARK );', decal)
        # Retail's camera-shake helper discards what it measures.
        self.assertIn('static void CL_JK2CgameEffectsCameraShake( const fntech3::jka::effectplay::Spawn & ) {}',
                      effects)
        init = function(effects, 'static void CL_JK2CgameEffectsInit(')
        self.assertLess(init.index('CL_JKACgameEffectsFree();'), init.index('CL_JKACgameEffectsInit( 0 );'))
        play = function(effects, 'static void CL_JK2CgameEffectsPlay(')
        self.assertIn('if ( state.frameTime < 1 ) return;', play)
        abi = read(ROOT / 'code/compat/jk2_client_abi.h')
        self.assertIn('FNTECH3_JK2_MP_CG_TRACE = 16,', abi)
        self.assertIn('FNTECH3_JK2_MP_CG_IMPACT_MARK = 23', abi)

    def test_academy_keeps_its_own_operations_when_no_host_is_set(self):
        academy = read(ACADEMY_EFFECTS)
        self.assertIn('const RavenEffectsHost *ravenEffectsHost{};', academy)
        # The only writer of the host is Outcast's dispatcher.
        users = [path.relative_to(ROOT).as_posix() for path in (ROOT / 'code').rglob('*.hpp')
                 if 'ravenEffectsHost =' in read(path)]
        self.assertEqual(users, ['code/client/client_jk2_cgame_effects.hpp'])
        create = function(academy, 'static void CL_JKACgameEffectCreate(')
        self.assertIn('else CL_JKACgameEffectCameraShake(spawn);', create)
        self.assertIn(':CL_JKACgameDecalAdd(spawn.shader,spawn.origin,spawn.axis[0],spawn.rotation,color,spawn.size);', create)
        self.assertIn('S_StartJKASound(at,effectplay::SoundEntity,effectplay::SoundChannel,static_cast<sfxHandle_t>(spawn.sound));',
                      create)


FIXTURE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/jk2_client_abi.h"
#include <array>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstring>
#include <limits>
struct Drop {};
void QDECL Com_Error( errorParm_t, const char *, ... ) { throw Drop{}; }
static vm_t *cgvm;
namespace fntech3::jk2::hoststrings {
static int languageCalls;
inline unsigned Language() { ++languageCalls; return 0; }
}
'''


class ExecutedRecordTests(unittest.TestCase):
    def test_trace_record_read_character_and_names(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        vm = read(ROOT / 'code/qcommon/vm.c')
        services = read(SERVICES)
        source = FIXTURE + function(vm, 'void *VM_ExplicitArgPtr(') + '\n' + function(vm, 'void VM_CheckBounds(') + '\n'
        for signature in ('static void *CL_JK2CgameSpan(', 'static const char *CL_JK2CgameString(',
                          'static std::array<char, MAX_QPATH> CL_JK2CgameName(',
                          'static intptr_t CL_JK2CgameReadChar(',
                          'static fntech3_jk2_mp_trace_t CL_JK2CgameTraceRecord('):
            source += function(services, signature) + '\n'
        source += r'''
template<class F> static bool Drops( F action ) { try { action(); } catch ( Drop ) { return true; } return false; }
int main() {
	static_assert( sizeof( fntech3_jk2_mp_trace_t ) == 1080 );
	trace_t traced{};
	traced.allsolid = qtrue; traced.fraction = 0.25f; traced.endpos[2] = -7.5f; traced.plane.normal[1] = 1;
	traced.plane.dist = 12; traced.plane.type = 1; traced.plane.signbits = 2; traced.surfaceFlags = 0x40;
	traced.contents = 0x10001; traced.entityNum = 1022;
	const auto record = CL_JK2CgameTraceRecord( traced );
	assert( record.allsolid == 1 && !record.startsolid && record.fraction == 0.25f && record.endpos[2] == -7.5f );
	assert( record.normal[1] == 1 && record.dist == 12 && record.type == 1 && record.signbits == 2 );
	assert( record.surfaceFlags == 0x40 && record.contents == 0x10001 && record.entityNum == 1022 );
	for ( auto byteValue : record.ghoul2Collisions ) assert( !byteValue );

	alignas( 4 ) static byte memory[256];
	vm_t vm{}; vm.dataBase = memory; vm.dataMask = 255; cgvm = &vm;
	std::memcpy( memory + 16, "Ab", 3 );
	const std::int32_t cursor = 16;
	std::memcpy( memory + 8, &cursor, 4 );
	assert( CL_JK2CgameReadChar( 8 ) == 'A' && CL_JK2CgameReadChar( 8 ) == 'b' && CL_JK2CgameReadChar( 8 ) == 0 );
	std::int32_t moved; std::memcpy( &moved, memory + 8, 4 ); assert( moved == 19 );
	assert( fntech3::jk2::hoststrings::languageCalls == 3 );
	const std::int32_t outside = 300;
	std::memcpy( memory + 8, &outside, 4 );
	assert( Drops( [] { CL_JK2CgameReadChar( 8 ); } ) );
	assert( Drops( [] { CL_JK2CgameReadChar( 0 ); } ) );
	assert( Drops( [] { CL_JK2CgameReadChar( 254 ); } ) );

	assert( !CL_JK2CgameName( 0 )[0] );
	std::memset( memory + 100, 'x', 100 ); memory[200] = 0;
	const auto name = CL_JK2CgameName( 100 );
	assert( std::strlen( name.data() ) == MAX_QPATH - 1 );
	std::memcpy( memory + 40, "music/x.mp3", 12 );
	assert( !std::strcmp( CL_JK2CgameName( 40 ).data(), "music/x.mp3" ) );
	assert( !std::strcmp( CL_JK2CgameString( 40 ), "music/x.mp3" ) );
	assert( Drops( [] { CL_JK2CgameString( 100, 64 ); } ) );
	std::memset( memory + 240, 'y', 16 );
	assert( Drops( [] { CL_JK2CgameName( 240 ); } ) && Drops( [] { CL_JK2CgameString( 240 ); } ) );
	return 0;
}
'''
        parent = ROOT / '.tmp/tests/jk2-cgame-services'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as folder:
            path = Path(folder) / 'services.cpp'
            path.write_text(source)
            executable = Path(folder) / ('services.exe' if os.name == 'nt' else 'services')
            build = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(path), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True,
                                   timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class RecordTests(unittest.TestCase):
    def test_record_and_companion_agree(self):
        text = read(RECORD)
        data = json.loads(read(COMPANION))
        self.assertEqual(data['profile'], 'jk2-mp')
        self.assertIn('import 285', text)
        for row in data['imports']:
            with self.subTest(ordinal=row['ordinal']):
                self.assertIn(row['status'], {'served', 'shared', 'refused'})
                self.assertRegex(row['handler'], r'^0x[0-9a-f]{6}$')
                self.assertTrue(row['evidence'].strip())


if __name__ == '__main__':
    unittest.main()
