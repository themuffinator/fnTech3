"""Jedi Outcast's client-game Ghoul2, drawing and HUD: the import map read from
retail jk2mp.exe, the matrix column import run against a QVM data segment,
the render flags its renderer reads, the scene routing, the engine's string
packages and the HUD frame. See docs/fntech3/2026-10-16-jk2-cgame-ghoul2.md."""
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
GHOUL2 = ROOT / 'code/client/client_jk2_cgame_ghoul2.hpp'
SERVICES = ROOT / 'code/client/client_jk2_cgame_services.hpp'
RECORD = ROOT / 'docs/fntech3/2026-10-16-jk2-cgame-ghoul2.md'
COMPANION = RECORD.with_suffix('.json')


def read(path):
    return path.read_text(encoding='utf-8')


class Ghoul2ImportTests(unittest.TestCase):
    def test_client_imports_share_the_game_imports_cores(self):
        ghoul2 = read(GHOUL2)
        # client, game, core: the table the header keeps, from both dispatchers.
        for client, game, core in ((281, 584, '0x4a7890'), (283, 586, '0x4a7620'), (284, 587, '0x4a7620'),
                                   (285, 588, '0x4a6e90'), (286, 599, '0x4a6e00'), (287, 591, '0x4a7520'),
                                   (288, 592, '0x4a7440'), (289, 593, '0x4a7d80'), (291, 595, '0x4a7bf0'),
                                   (292, 596, '0x4a7ce0'), (295, 589, '0x4a75a0'), (296, 590, '0x4a75f0')):
            with self.subTest(client=client):
                self.assertRegex(ghoul2, r' \* .*\b%d\s+%d\s+%s' % (client, game, core))
                self.assertIn('case %d' % client, ghoul2)
        call = function(ghoul2, 'static bool CL_JK2CgameGhoul2Call( intptr_t *args, intptr_t &result ) {')
        self.assertIn('case 294: case 297: case 298: case 299:', call)
        self.assertIn('"model removal", "", "", "root surface", "surface visibility", "new origin"', call)
        # 278-279 and 290 have no call site in the retail client game.
        self.assertIn("Jedi Outcast's Ghoul2 boundary is open.", call)
        # The init's fourth word is the model's own skin (0x4a7051, +0x38).
        self.assertIn('store.SetSkin( fntech3::ghoul2::ReadField( slot, jk2CgameGhoul2FieldBytes ), index,', call)
        self.assertIn('NameMatch::AsciiFolded', ghoul2)

    def test_scene_routing_and_material_order(self):
        services = read(SERVICES)
        self.assertIn('if ( source.ghoul2 ) { // posed and drawn with its view', services)
        self.assertIn('else CL_JK2CgameGhoul2Flush( value.time );', services)
        clear = re.search(r'\tcase 200:.*?return JK2CgameService::Shared;', services, re.S).group(0)
        self.assertIn('CL_JK2CgameGhoul2DropScene();', clear)
        flush = function(read(GHOUL2), 'static void CL_JK2CgameGhoul2Flush( int time ) {')
        self.assertIn('AnglesToAxis( angles, canonical.axis );', flush)
        self.assertIn('store.InstancePalettes( *instance, time, scene.order, scene.palettes, failed );', flush)
        self.assertIn('if ( source.customShader ) entity.customShader = source.customShader;', flush)
        self.assertIn('else entity.customSkin = model.customSkin ? model.customSkin : source.customSkin;', flush)


FIXTURE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/jk2_render_adapter.hpp"
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
struct Drop {};
void QDECL Com_Error( errorParm_t, const char *, ... ) { throw Drop{}; }
static vm_t *cgvm;
'''


class ExecutedTests(unittest.TestCase):
    def test_matrix_columns_and_render_flags(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        vm = read(ROOT / 'code/qcommon/vm.c')
        source = (FIXTURE + function(vm, 'void *VM_ExplicitArgPtr(') + '\n' + function(vm, 'void VM_CheckBounds(') +
                  '\n' + function(read(SERVICES), 'static void *CL_JK2CgameSpan(') + '\n' +
                  function(read(GHOUL2), 'static void CL_JK2CgameGhoul2Vector( const intptr_t *args ) {') + r'''
template<class F> static bool Drops( F action ) { try { action(); } catch ( Drop ) { return true; } return false; }
int main() {
	alignas( 4 ) static byte memory[256];
	vm_t vm{}; vm.dataBase = memory; vm.dataMask = 255; cgvm = &vm;
	float matrix[12];
	for ( int n = 0; n < 12; ++n ) matrix[n] = float( n + 1 );
	std::memcpy( memory + 16, matrix, sizeof( matrix ) );
	// Raven's orientations: origin, x, z, y, then the three negated.
	const int columns[7] = { 3, 0, 2, 1, 0, 2, 1 };
	for ( intptr_t flag = 0; flag < 7; ++flag ) {
		std::memset( memory + 80, 0, 12 );
		const intptr_t args[4] = { 282, 16, flag, 80 };
		CL_JK2CgameGhoul2Vector( args );
		float out[3];
		std::memcpy( out, memory + 80, sizeof( out ) );
		const float sign = flag >= 4 ? -1.0f : 1.0f;
		for ( int row = 0; row < 3; ++row ) assert( out[row] == sign * matrix[row * 4 + columns[flag]] );
	}
	std::memset( memory + 80, 0x5a, 12 );
	for ( intptr_t flag : { -1, 7, 100 } ) {
		const intptr_t args[4] = { 282, 16, flag, 80 };
		CL_JK2CgameGhoul2Vector( args );
		for ( int n = 0; n < 12; ++n ) assert( memory[80 + n] == 0x5a );
	}
	assert( Drops( [] { const intptr_t args[4] = { 282, 220, 0, 80 }; CL_JK2CgameGhoul2Vector( args ); } ) );
	assert( Drops( [] { const intptr_t args[4] = { 282, 16, 0, 250 }; CL_JK2CgameGhoul2Vector( args ); } ) );

	// jk2mp.exe's stage path reads 0x400 as forced entity alpha; nothing in its
	// renderer reads 0x800; any other bit outside the model set is refused.
	fntech3_jk2_mp_refEntity_t entity{};
	for ( int n = 0; n < 3; ++n ) entity.axis[n][n] = 1;
	entity.hModel = 3;
	refEntity_t out{};
	entity.renderfx = 0x400;
	assert( fntech3::jk2::render::Entity( entity, out ) == fntech3::jk2::render::Status::Ok && out.jka.forceAlpha );
	entity.renderfx = 0x800;
	assert( fntech3::jk2::render::Entity( entity, out ) == fntech3::jk2::render::Status::Ok && !out.jka.forceAlpha );
	assert( !out.renderfx && !out.jka.rgbTint );
	entity.renderfx = 0x1000;
	assert( fntech3::jk2::render::Entity( entity, out ) == fntech3::jk2::render::Status::EntityFlags );
	entity.renderfx = 0; entity.ghoul2 = 1;
	assert( fntech3::jk2::render::Entity( entity, out ) == fntech3::jk2::render::Status::Ghoul2 );
	return 0;
}
''')
        parent = ROOT / '.tmp/tests/jk2-cgame-ghoul2'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as folder:
            path = Path(folder) / 'ghoul2.cpp'
            path.write_text(source)
            executable = Path(folder) / ('ghoul2.exe' if os.name == 'nt' else 'ghoul2')
            build = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(path), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True,
                                   timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class HudAndStringsTests(unittest.TestCase):
    def test_hud_frame_rules_and_engine_strings(self):
        host = read(ROOT / 'code/client/client_jk2_cgame.hpp')
        glconfig = host[host.index('case 219:'):host.index('case 220:')]
        self.assertLess(glconfig.index('CL_CopyQ3Glconfig( VMA(1), &cls.glconfig );'),
                        glconfig.index('CL_HudModuleScreen( &width, &height );'))
        supported = read(ROOT / 'code/client/cl_hud.cpp').split('\n\thudSupported = ', 1)[1].split(';', 1)[0]
        self.assertIn('Com_GameIsJK2MP()', supported)
        rules = json.loads(read(ROOT / 'pkg/games/jk2-mp/base/fntech3-hud.json'))
        names = [rule['name'] for rule in rules['rules']]
        # Text that sits inside a cluster's area is placed before the cluster's
        # own rule can claim it.
        self.assertLess(names.index('score'), names.index('right_hud'))
        self.assertLess(names.index('selection_names'), names.index('selection_bars'))
        init = function(read(ROOT / 'code/client/cl_main.cpp'), 'void CL_Init( void ) {')
        self.assertLess(init.index('Com_Printf( "----- Client Initialization -----\\n" );'),
                        init.index('if ( Com_GameIsJK2MP() ) CL_JK2StringRegisterEngine();'))
        strings = read(ROOT / 'code/client/client_jk2_strings.hpp')
        self.assertIn('{"con_text", "mp_ingame", "mp_svgame", "sp_ingame"}', strings)
        self.assertIn('inline constexpr unsigned jk2StringEngineOwner = 8;', strings)


class RecordTests(unittest.TestCase):
    def test_record_and_companion_agree(self):
        text = read(RECORD)
        data = json.loads(read(COMPANION))
        self.assertEqual(data['profile'], 'jk2-mp')
        for run in data['live']:
            with self.subTest(run=run['run']):
                self.assertTrue(run['installationUnchanged'])
                self.assertRegex(run['fntech3Sha256'], r'^[0-9a-f]{64}$')
                for capture in run['captures']:
                    self.assertRegex(capture['sha256'], r'^[0-9a-f]{64}$')
        self.assertIn('1680x720', text)


if __name__ == '__main__':
    unittest.main()
