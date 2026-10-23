"""Jedi Academy's server serves its Ghoul2 surface and removal imports.

Game imports 609 (G_G2_SETSURFACEONOFF), 612 (G_G2_GETSURFACERENDERSTATUS),
603 and 604 (G_G2_REMOVEGHOUL2MODEL, G_G2_REMOVEGHOUL2MODELS) and a render
skin on 591 are served from the title's own store, measured against retail
jamp.exe 1.01 (docs/fntech3/2026-10-22-jka-g2-surfaces.md). The logic is
tests/jka_g2_surface_tests.cpp's; this checks the dispatcher's wiring: each
case names its retail handler and core, refuses what retail reads through,
and the trace and collision walk read the store's override lists.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / 'code/server/server_jka_game.hpp'


def case(source, name):
    start = source.index(f'case Call::{name}:')
    end = source.index('\n\tcase Call::', start + 1)
    # Grouped cases share the body after the last of them.
    while source[start:end].rstrip().endswith(':'):
        end = source.index('\n\tcase Call::', end + 1)
    return source[start:end]


class JKAServerG2SurfaceTests(unittest.TestCase):
    def setUp(self):
        self.game = GAME.read_text(encoding='utf-8')

    def test_each_import_names_its_retail_handler_and_core(self):
        facts = {
            'G_G2_SETSURFACEONOFF': ('0x45ced2', '0x4deb70', '0x4ee700', 'SetSurfaceOnOff'),
            'G_G2_GETSURFACERENDERSTATUS': ('0x45cf26', '0x4116e0', '0x4dec00', '0x4eee70',
                                            'SurfaceRenderStatus'),
            'G_G2_REMOVEGHOUL2MODEL': ('0x45cd9a', '0x4decc0', 'RemoveModel'),
            'G_G2_REMOVEGHOUL2MODELS': ('0x45cdb5', '0x4deea0', 'RemoveModels'),
            'G_G2_SETSKIN': ('0x45cbb2', '0x4deb50', '0x4ee7e0', 'ApplySkinVisibility'),
        }
        for name, expected in facts.items():
            body = case(self.game, name)
            for fact in expected:
                with self.subTest(name=name, fact=fact):
                    self.assertIn(fact, body)

    def test_what_retail_reads_through_is_refused(self):
        surfaces = case(self.game, 'G_G2_SETSURFACEONOFF')
        self.assertRegex(surfaces, r'if \( !args\[1\] \) return true;')
        self.assertIn('never handed out', surfaces)
        self.assertIn('handed no surface name', surfaces)
        status = case(self.game, 'G_G2_GETSURFACERENDERSTATUS')
        self.assertIn('whose records retail reads past', status)
        self.assertIn('result = -1;', status)
        removal = case(self.game, 'G_G2_REMOVEGHOUL2MODEL')
        self.assertIn('handed no model field', removal)
        self.assertIn("instance's records", removal)
        self.assertIn('Removal::NoInstance', removal)
        self.assertIn('result = removal == Removal::NoModel ? 0 : 1;', removal)
        skin = case(self.game, 'G_G2_SETSKIN')
        self.assertIn('never registered', skin)
        self.assertLess(skin.index('never registered'), skin.index('SetSkin('))

    def test_the_store_reuses_places_and_the_walk_reads_overrides(self):
        assets = (ROOT / 'code/server/server_raven_assets.hpp').read_text(encoding='utf-8')
        self.assertRegex(assets, r'ravenGhoul2Academy\( ravenGhoul2Files, true, true, true,\s*'
                                 r'false, fntech3::ghoul2::NameMatch::AsciiFolded, true \);')
        self.assertRegex(assets, r'ravenGhoul2Outcast\( ravenGhoul2Files, true, false, true, true \);')
        trace = (ROOT / 'code/server/server_jka_trace.hpp').read_text(encoding='utf-8')
        self.assertEqual(trace.count('collision::StoredOverrides( *instance )'), 2)
        self.assertNotIn('return nullptr; }, prepared', trace)


if __name__ == '__main__':
    unittest.main()
