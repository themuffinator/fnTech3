"""Heavy Metal: F.A.K.K.2 draws with no overbright bits, as retail does.

Retail fakk2.exe 1.02 registers r_overBrightBits with "0" (0x00460d74) and
r_mapOverBrightBits with "2" (0x00460ed4), so its lightmaps take the whole
shift and saturate where retail's do. Under this engine's usual one bit,
while the lightmap stages that name no rgbGen (338 of 365) took Quake III's
identity lighting, `fakkhouse`'s grass drew at 0.69 of retail's; with none,
the same demo frames match retail's region for region. Those stages now take
retail's identity (tests/alice_shader_parse_tests.py). Every renderer
registers F.A.K.K.2's default, which it shares with Alice
(tests/alice_overbright_tests.py), and every other profile keeps its own.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


class Fakk2OverbrightTests(unittest.TestCase):
    def test_every_renderer_registers_fakk2s_retail_default(self):
        for renderer in RENDERERS:
            source = read(f'code/{renderer}/tr_init.c')
            with self.subTest(renderer=renderer):
                self.assertIn('r_overBrightBits = ri.Cvar_Get( "r_overBrightBits",\n'
                              '\t\t( COM_GAME_ID_IS_RITUAL( ri.gameId ) || ri.gameId == COM_GAME_STVEF_SP ) ? "0" : "1",\n'
                              '\t\tCVAR_ARCHIVE_ND | CVAR_LATCH );', source)
                self.assertEqual(source.count('ri.Cvar_Get( "r_overBrightBits"'), 1)
                # The map's own bits stay retail's 2 (Holomatch has 1, and Alice
                # and Elite Force's single player none).
                self.assertIn('\t\t( ri.gameId == COM_GAME_ALICE || ri.gameId == COM_GAME_STVEF_SP ) ? "0" :\n'
                              '\t\tri.gameId == COM_GAME_STVEF_MP ? "1" : "2",', source)


if __name__ == '__main__':
    unittest.main()
