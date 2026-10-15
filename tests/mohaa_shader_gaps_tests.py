"""Medal of Honor: the shader gaps a developer sweep of all 54 retail maps found.

Every map, loaded with `developer 1`, printed the Quake III client art the
engine looked up under every profile -- "console", "icons/record",
"menu/art/3_cursor2", "ui/assets/backscreen_smoke" -- and the renderer's
"sun", none of which 1.00 ships (its "console" script names an absent
gfx/2d/BattleCry.tga), and four hedgerow shaders' `cull front` was refused
as invalid. Record: docs/fntech3/2026-10-15-mohaa-shader-sweep.md."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8', errors='replace')


def block(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin + len(start))]


class MohaaShaderGapTests(unittest.TestCase):
    def test_cull_front_is_the_front_sided_default_under_mohaa_alone(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                cull = block(read(f'code/{renderer}/tr_shader.c'), '!Q_stricmp( token, "cull") )', 'continue;\n\t\t}')
                front = cull.index('ri.gameId == COM_GAME_MOHAA && !Q_stricmp( token, "front" )')
                self.assertLess(cull.index('shader.cullType = CT_BACK_SIDED;'), front)
                self.assertIn('shader.cullType = CT_FRONT_SIDED;', cull[front:cull.index('else\n', front)])
                # every other profile still warns, as Quake III does
                self.assertIn("invalid cull parm '%s'", cull[front:])

    def test_the_renderer_looks_up_no_quake3_sun_under_mohaa(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                self.assertIn('tr.sunShader = ri.gameId == COM_GAME_WOLFET || ri.gameId == COM_GAME_MOHAA ? NULL\n'
                              '\t\t: R_FindShader( "sun", LIGHTMAP_NONE, qtrue );', read(f'code/{renderer}/tr_shader.c'))

    def test_the_client_registers_only_art_mohaa_ships(self):
        main = read('code/client/cl_main.cpp')
        art = "} else if ( Com_GameIsMOHAA() ) {\n\t\t// Medal of Honor ships Quake III's character sheet"
        branch = block(main, art, '} else {')
        self.assertIn('cls.charSetShader = re.RegisterShader( "gfx/2d/bigchars" );', branch)
        self.assertIn('cls.consoleShader = cls.whiteShader;', branch)
        for handle in ('recordShader', 'cursorShader'):
            self.assertIn(f'cls.{handle} = 0;', branch)
        # no logo layer: the shared branch alone sets one, as for Q3Test
        self.assertNotIn('consoleLogoShader', branch)
        # the character sheet is the only name it looks up
        self.assertEqual(branch.count('RegisterShader'), 1)
        # the branch sits ahead of the shared one, which keeps Quake III's names
        self.assertLess(main.index(art), main.index('re.RegisterShaderNoMip( "icons/record" )'))
        scrn = block(read('code/client/cl_scrn.cpp'), 'void SCR_Init( void ) {', 'scr_initialized = true;')
        self.assertIn('Com_GameIsWolfET() || Com_GameIsRitual() || Com_GameIsMOHAA() ? 0', scrn)


if __name__ == '__main__':
    unittest.main()
