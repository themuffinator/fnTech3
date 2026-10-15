"""Medal of Honor: the untextured picture the engine draws fades, the
letterbox and its menus with is registered once a renderer start.

CL_MOHAAWhite uploads a new 1x1 picture on every call: the renderer answers a
repeated name with the shader it already has, but R_CreateImage has run by
then, so each call spends one of the renderer's 2,048 image slots. The fades
and letterbox had called it every frame they drew, and `m3l1a`'s landing,
whose explosions flash the screen, used up the slots within three minutes
(R_CreateImage: MAX_DRAWIMAGES hit, which drops the game). CL_InitRenderer
registers it once as cls.whiteShader, and every draw uses that."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


class MohaaWhiteShaderTests(unittest.TestCase):
    def test_only_the_renderer_start_registers_the_white_picture(self):
        callers = []
        for path in sorted((ROOT / 'code/client').glob('*.cpp')):
            text = path.read_text(encoding='utf-8', errors='replace')
            for match in re.finditer(r'\bCL_MOHAAWhite\(\s*\)', text):
                line = text.count('\n', 0, match.start()) + 1
                definition = text[match.start() - 30:match.start()].rstrip().endswith('qhandle_t')
                if not definition:
                    callers.append(f'{path.name}:{line}')
        self.assertEqual(len(callers), 1, callers)
        self.assertTrue(callers[0].startswith('cl_main.cpp:'), callers)
        main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8', errors='replace')
        branch = main[main.index("// Medal of Honor ships Quake III's character sheet"):]
        branch = branch[:branch.index('} else {')]
        self.assertIn('cls.whiteShader = CL_MOHAAWhite();', branch)

    def test_fades_and_letterbox_draw_with_the_registered_picture(self):
        text = (ROOT / 'code/client/cl_mohaa.cpp').read_text(encoding='utf-8', errors='replace')
        fades = function(text, 'void CL_MOHAADrawFades( void ) {')
        self.assertNotIn('CL_MOHAAWhite', fades)
        self.assertNotIn('RegisterShaderFromRGBA', fades)
        self.assertEqual(fades.count('cls.whiteShader'), 3)


if __name__ == '__main__':
    unittest.main()
