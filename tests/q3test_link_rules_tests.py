"""Run Q3Test's two link rules from the real sources against stubs: a link
whose leafs give no area leaves the entity unlinked, and nothing the game has
not marked in use is linked, as every retail release does (1.02 SV_LinkEntity
0x41de66, 0x41e17b; 1.08 0x41f76b, 0x41fa80). No other profile changes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include "code/compat/q3test_game_abi.h"
#include "code/qcommon/game_profile.h"
struct svEntity_t { int areanum; };
struct sharedEntity_t { int linked; };
static bool q3test, numbered;
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile() { profile.id = q3test ? COM_GAME_Q3TEST : COM_GAME_QUAKE3; return &profile; }
static bool SV_Q3TestNumberedClients() { return numbered; }
static int SV_Q3TestEntityIndex( const q3tEnt *entity ) { return entity->s.number; }
static sharedEntity_t shared[4];
static sharedEntity_t *SV_Q3TestEntity( q3tEnt *entity ) { return &shared[entity->s.number]; }
static int links, unlinks;
void SV_LinkEntity( sharedEntity_t *entity ) { ++links; entity->linked = 1; }
void SV_UnlinkEntity( sharedEntity_t *entity ) { ++unlinks; entity->linked = 0; }
'''

CHECKS = r'''
int main() {
	svEntity_t ent{};
	for ( bool profile : { false, true } ) {
		q3test = profile;
		ent.areanum = -1;
		assert( SV_LinkLeavesUnlinked( 0, &ent ) );
		assert( SV_LinkLeavesUnlinked( 3, &ent ) == profile );
		ent.areanum = 2;
		assert( SV_LinkLeavesUnlinked( 0, &ent ) );
		assert( !SV_LinkLeavesUnlinked( 3, &ent ) );
	}
	q3test = true;
	for ( bool clientsNumbered : { false, true } ) {
		numbered = clientsNumbered;
		for ( int number : { 0, 1 } ) {
			q3tEnt entity{};
			entity.s.number = number;
			const bool world = !clientsNumbered && number == 0;
			links = unlinks = 0; shared[number].linked = 1;
			entity.inuse = 0;
			SV_Q3TestLinkEntity( &entity );
			assert( links == 0 && unlinks == ( world ? 0 : 1 ) );
			assert( shared[number].linked == ( world ? 1 : 0 ) );
			entity.inuse = 1;
			SV_Q3TestLinkEntity( &entity );
			assert( links == ( world ? 0 : 1 ) );
		}
	}
}
'''


class Q3TestLinkRulesTests(unittest.TestCase):
    def test_link_rules_follow_every_release(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        world = (ROOT / 'code/server/sv_world.cpp').read_text(encoding='utf-8')
        game = (ROOT / 'code/server/server_q3test_game.hpp').read_text(encoding='utf-8')
        source = PRELUDE + function(world, 'static bool SV_LinkLeavesUnlinked(') + '\n'
        source += function(game, 'void SV_Q3TestLinkEntity( q3tEnt *entity ) {') + '\n' + CHECKS
        link = world[world.index('void SV_LinkEntity( sharedEntity_t *gEnt ) {'):]
        self.assertIn('if ( SV_LinkLeavesUnlinked( num_leafs, ent ) ) {', link[:link.index('\n}')])
        with tempfile.TemporaryDirectory(prefix='q3test-link-rules-') as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
