"""The static world buffer (`r_vbo`) is on by default in every renderer.

GL drew every world surface from the CPU each frame until 2026-10-26; the
per-profile frames and timings that moved its default are in
docs/fntech3/2026-10-26-glx-world-vbo-default.md.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
REGISTER = re.compile(r'r_vbo = ri\.Cvar_Get\( "r_vbo", "(\d)", ([A-Z_| ]+) \);')


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


class WorldVBODefaultTests(unittest.TestCase):
    def test_every_renderer_registers_it_on_and_latched(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                found = REGISTER.findall(read(f'code/{renderer}/tr_init.c'))
                self.assertEqual(len(found), 1, found)
                default, flags = found[0]
                self.assertEqual(default, '1')
                self.assertIn('CVAR_LATCH', flags)

    def test_gl_saves_only_a_changed_value(self):
        # GL's default was 0 and never written to a config, so players who
        # left it alone take the new default.
        _, flags = REGISTER.findall(read('code/renderer/tr_init.c'))[0]
        self.assertIn('CVAR_ARCHIVE_ND', flags)

    def test_the_guides_name_it(self):
        display = read('docs/DISPLAY.md')
        restart = display[display.index('## When To Use `vid_restart`'):display.index('## Related Guides')]
        self.assertIn('`r_vbo`', restart)
        self.assertIn('r_vbo 0', read('docs/GLX.md'))


if __name__ == '__main__':
    unittest.main()
