"""The Ritual server's world: how traces clip against entities, and the state
`solid` the link publishes.

Retail alice.exe asks SV_ClipHandleForEntity (0x004577b0) at every clip -- the
trace's entity pass at 0x0045821b and the single-entity clip at 0x00453709 --
for an inline model or a temporary box rebuilt from the entity's own bounds and
contents (CM_TempBoxModel 0x00430f50 takes the contents). The pass skips what is
not solid, the passing entity, its owner and their missiles, and contents the
mask does not take (0x00458130..0x004582e5). The link publishes the state's
solid (0x00457a78..0x00457bfc) packed by 0x0044a000. F.A.K.K.2 is served by the
same code [I: its executable is encrypted]. No retail file is read.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

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


SOLID_PROBE = r'''
#include "code/compat/ritual_solid.hpp"
#include <cstdio>
using namespace fntech3::ritual;
static int failures = 0;
static void Expect( bool ok, const char *what ) { if ( !ok ) { std::printf( "failed: %s\n", what ); ++failures; } }
static int Pack( int x, int y, int zd, int zu ) { return ( ( ( ( ( zu << 5 ) | zd ) << 8 ) | y ) << 9 ) | x; }
int main() {
	const float playerMins[3] = { -15, -15, 0 }, playerMaxs[3] = { 15, 15, 96 };
	Expect( PackBoundingBox( playerMins, playerMaxs ) == Pack( 15, 15, 16, 96 ), "Julie's box" );
	const float creeperMins[3] = { -32, -32, 0 }, creeperMaxs[3] = { 32, 32, 85 };
	Expect( PackBoundingBox( creeperMins, creeperMaxs ) == Pack( 32, 32, 16, 85 ), "a creeper's box" );
	// Each field is truncated, then clamped into its width.
	const float wideMins[3] = { -8, -8, -100 }, wideMaxs[3] = { 1000, 300, 700.9f };
	Expect( PackBoundingBox( wideMins, wideMaxs ) == Pack( 511, 255, 0, 511 ), "clamped high" );
	const float oddMins[3] = { 0, 0, 40 }, oddMaxs[3] = { -4, 7.9f, -1 };
	Expect( PackBoundingBox( oddMins, oddMaxs ) == Pack( 0, 7, 31, 0 ), "clamped low" );
	// The state's solid by type and contents.
	Expect( StateSolid( SolidTrigger, 0x2000000, playerMins, playerMaxs ) == 0, "a trigger" );
	Expect( StateSolid( SolidNot, 0x2000000, playerMins, playerMaxs ) == 0, "not solid" );
	Expect( StateSolid( 5, 0x2000000, playerMins, playerMaxs ) == 5, "another type is written as itself" );
	Expect( StateSolid( SolidBoundingBox, 0x2000000, playerMins, playerMaxs ) == Pack( 15, 15, 16, 96 ), "a body" );
	Expect( StateSolid( SolidBoundingBox, 0, playerMins, playerMaxs ) == 0, "no contents" );
	Expect( StateSolid( SolidBoundingBox, 0x100000, playerMins, playerMaxs ) == 0, "0x100000 alone" );
	Expect( StateSolid( SolidBoundingBox, 0x4000000, playerMins, playerMaxs ) == 0, "0x4000000 alone" );
	Expect( StateSolid( SolidBoundingBox, 0x4000001, playerMins, playerMaxs ) != 0, "0x4000000 with more" );
	Expect( StateSolid( SolidBsp, -1, playerMins, playerMaxs ) == SolidBrushModel, "a brush model, all contents" );
	Expect( StateSolid( SolidBsp, 1, playerMins, playerMaxs ) == SolidBrushModel, "a solid brush model" );
	Expect( StateSolid( SolidBsp, 0x4000000, playerMins, playerMaxs ) == 0, "a brush model of 0x4000000" );
	Expect( StateSolid( SolidBsp, 0x100001, playerMins, playerMaxs ) == 0, "a brush model of 0x100000" );
	return failures ? 1 : 0;
}
'''


class RitualSolidTests(unittest.TestCase):
    def test_the_state_solid_is_packed_as_retail_packs_it(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ritual-solid-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(SOLID_PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class RitualTraceSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.game = (ROOT / 'code/server/server_alice_game.hpp').read_text(encoding='utf-8')
        cls.cgame = (ROOT / 'code/client/client_alice_cgame.hpp').read_text(encoding='utf-8')

    def test_every_clip_builds_its_model_then(self):
        handle = function(self.game, 'clipHandle_t SV_AliceClipHandleForEntity( void *entity ) {')
        assert_order(self, handle, 'if ( SV_AliceClipsAsBrushes( entity ) ) {', 'CM_InlineModel( model )',
                     'CM_TempBoxModel(', 'CM_SetTempBoxModelContents(')
        clip = function(self.game, 'void SV_AliceClipEntity( trace_t *trace, const float *start, const float *end,')
        self.assertIn('const clipHandle_t handle = SV_AliceClipHandleForEntity( entity );', clip)
        self.assertNotIn('FNTECH3_RITUAL_ENT_CLIPHANDLE', clip)
        self.assertIn('SV_AliceClipsAsBrushes( entity ) ? angles : zero', clip)

    def test_the_entity_pass_skips_as_retail_does(self):
        trace = function(self.game, 'void QDECL SV_AliceTrace( void *results, const float *start, const float *mins,')
        assert_order(self, trace,
                     'const int ownerOffset = SV_RitualLayout().rCurrentAngles + 12;',
                     'for ( int i = 0; i < count && !trace.allsolid; i++ ) {',
                     'if ( solid == 0 || solid == 1 ) {',
                     'touched[i] == passEntityNum || touched[i] == passOwner',
                     'touchOwner == passEntityNum || touchOwner == passOwner',
                     '& contentMask ) ) {',
                     'SV_AliceClipEntity( &hit,',
                     'if ( hit.allsolid ) {', '} else if ( hit.startsolid ) {',
                     'if ( hit.fraction < trace.fraction ) {')

    def test_the_link_publishes_the_state_solid(self):
        link = function(self.game, 'void QDECL SV_AliceLinkEntity( void *entity ) {')
        self.assertIn('fntech3::ritual::StateSolid( solid,', link)
        self.assertNotIn('CM_TempBoxModel', link)

    def test_the_client_game_box_takes_its_contents(self):
        box = function(self.cgame, 'clipHandle_t QDECL CL_AliceTempBoxModel( const float *mins, const float *maxs, int contents ) {')
        assert_order(self, box, 'CM_TempBoxModel( mins, maxs, qfalse );', 'CM_SetTempBoxModelContents( contents );')


if __name__ == '__main__':
    unittest.main()
