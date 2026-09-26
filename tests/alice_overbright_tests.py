"""American McGee's Alice draws with no overbright bits, as alice.exe does.

Retail alice.exe registers r_overBrightBits "0" (flags 0x21, 0x00475136) and
r_mapOverBrightBits "0" (flags 0x20, 0x0047529e), and never reads the second:
its lightmap loader, its surfaces' vertex colours and its light grid are
shifted by the overbright bits alone (0x0049ba40, 0x0049acb0), so at its
default a map's light is drawn as the map holds it. Under this engine's
usual one bit over two map bits, where 1,369 of Alice's 1,387 lightmap stages
say rgbGen identity, its levels drew two to three times as bright as with
none (docs/fntech3/2026-09-25-alice-overbright.md).

Each renderer's two registered defaults are compiled and evaluated for every
profile, so the check reads what the engine registers, however the
expression is spelled. Alice takes 0 and 0, as Elite Force's single player
does (its executable registers neither bit, docs/fntech3/2026-09-20-stvef-mdr.md),
Holomatch keeps one of each, and every other profile outside the Ritual
engines keeps one bit over two.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

PROGRAM = r'''
#include "code/qcommon/game_profile.h"
#include <stdio.h>

#ifndef COM_GAME_ID_IS_RITUAL
#define COM_GAME_ID_IS_RITUAL( id ) ( ( id ) == COM_GAME_ALICE )
#endif

static struct { comGameId_t gameId; } ri;

int main( void ) {
	int game;
	for ( game = COM_GAME_QUAKE3; game < COM_GAME_COUNT; game++ ) {
		ri.gameId = (comGameId_t)game;
		printf( "%d %d %d %d %d %s %s\n", game, game == COM_GAME_ALICE, COM_GAME_ID_IS_RITUAL( game ) ? 1 : 0,
			game == COM_GAME_STVEF_SP || game == COM_GAME_STVEF_MP, game == COM_GAME_STVEF_SP,
			( @OVERBRIGHT@ ), ( @MAP_OVERBRIGHT@ ) );
	}
	return 0;
}
'''


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def default_expression(source, name):
    """The default-value argument of the renderer's one latched registration of `name`."""
    pattern = re.compile(r'ri\.Cvar_Get\(\s*"' + name + r'",\s*(.*?),\s*CVAR_ARCHIVE_ND \| CVAR_LATCH\s*\);', re.S)
    found = pattern.findall(source)
    if len(found) != 1 or source.count(f'ri.Cvar_Get( "{name}"') != 1:
        raise AssertionError(f'{name}: expected exactly one archived, latched registration')
    return ' '.join(found[0].split())


def evaluate(renderer, folder):
    """Each profile's (id, is Alice, is Ritual, is Elite Force, is its single player,
    r_overBrightBits, r_mapOverBrightBits)."""
    compiler = shutil.which('clang')
    if not compiler:
        raise unittest.SkipTest('Clang unavailable')
    source = read(f'code/{renderer}/tr_init.c')
    program = (PROGRAM.replace('@OVERBRIGHT@', default_expression(source, 'r_overBrightBits'))
               .replace('@MAP_OVERBRIGHT@', default_expression(source, 'r_mapOverBrightBits')))
    c = folder / f'{renderer}_defaults.c'
    exe = folder / f'{renderer}_defaults.exe'
    c.write_text(program, encoding='utf-8')
    command = [compiler, *compiler_target_args(compiler), '-std=c11', '-w', '-I', str(ROOT), str(c), '-o', str(exe)]
    built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
    if built.returncode != 0:
        raise AssertionError(built.stdout + built.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    if run.returncode != 0:
        raise AssertionError(run.stdout + run.stderr)
    rows = []
    for line in run.stdout.splitlines():
        game, alice, ritual, elite_force, single_player, overbright, map_overbright = line.split()
        rows.append((int(game), alice == '1', ritual == '1', elite_force == '1', single_player == '1',
                     overbright, map_overbright))
    return rows


class AliceOverbrightTests(unittest.TestCase):
    def test_every_renderer_registers_alices_retail_defaults(self):
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    rows = evaluate(renderer, Path(temp))
                    self.assertGreater(len(rows), 12)
                    self.assertEqual([row[5:] for row in rows if row[1]], [('0', '0')])
                    for game, alice, ritual, elite_force, single_player, overbright, map_overbright in rows:
                        if ritual:
                            # Heavy Metal: F.A.K.K.2 keeps its own
                            # (tests/fakk2_overbright_tests.py).
                            continue
                        expected = ('0', '0') if single_player else ('1', '1' if elite_force else '2')
                        self.assertEqual((overbright, map_overbright), expected, f'profile {game}')


if __name__ == '__main__':
    unittest.main()
