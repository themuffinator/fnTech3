"""Medal of Honor: 2015's `cheats` switch, which the 1.00 game reads (with
`thereisnomonkey`) in place of Quake III's `sv_cheats`. The engine registers
it at 1 and sets it as a map starts, before the game spawns."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


class MohaaCheatsTests(unittest.TestCase):
    def test_the_server_registers_cheats_for_this_profile_alone(self):
        init = function((ROOT / 'code/server/sv_init.cpp').read_text(), 'void SV_Init( void )')
        block = init[init.index('if ( Com_GameIsMOHAA() ) {\n\t\t// Medal of Honor\'s game reads 2015\'s own cheat switch'):]
        block = block[:block.index('\t}\n') + 3]
        self.assertIn('Cvar_Get( "cheats", "1", CVAR_LATCH | CVAR_SYSTEMINFO );', block)

    def test_a_map_sets_cheats_before_the_game_spawns(self):
        map_f = function((ROOT / 'code/server/sv_ccmds.cpp').read_text(), 'static void SV_Map_f( void ) {')
        rule = ('if ( Com_GameIsMOHAA() && !( com_developer && com_developer->integer ) ) {\n'
                '\t\tCvar_Set( "cheats", ( cheat || sv_maxclients->integer == 1 ) ? "1" : "0" );')
        self.assertIn(rule, map_f)
        self.assertLess(map_f.index(rule), map_f.index('SV_SpawnServer( mapname.data(), SV_QBool( killBots ) );'))


if __name__ == '__main__':
    unittest.main()
