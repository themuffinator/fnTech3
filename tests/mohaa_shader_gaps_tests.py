"""Medal of Honor: the shader gaps a developer sweep of all 54 retail maps found.

Every map, loaded with `developer 1`, printed the Quake III client art the
engine looked up under every profile -- "console", "icons/record",
"menu/art/3_cursor2", "ui/assets/backscreen_smoke" -- and the renderer's
"sun", none of which 1.00 ships (its "console" script names an absent
gfx/2d/BattleCry.tga), and four hedgerow shaders' `cull front` was refused
as invalid. Record: docs/fntech3/2026-10-15-mohaa-shader-sweep.md."""
import os
import pathlib
import re
import unittest
import zipfile

import model_shader_tests

ROOT = pathlib.Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
MOHAA_ROOT = pathlib.Path(os.environ.get('FNTECH3_MOHAA_ROOT', r'E:\Games\Medal of Honor Allied Assault'))


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8', errors='replace')


def block(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin + len(start))]


# A TIKI surface's raw image through the real R_FindShaderInternal of each
# renderer (model_shader_tests.py's fixture); `rawNoPicMip` stands for
# r_picmipFilter's verdict on the name. A TIKI shader with a '.' is an image
# relative to the script's `path`, which 1.00 always puts under models/.
MOHAA_RAW_FLAG_CHECKS = r'''
#include <cstdlib>
int main() {
#ifdef _MSC_VER
    _set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
#endif
    const imgFlags_t both = (imgFlags_t)( IMGFLAG_MIPMAP | IMGFLAG_PICMIP );
    explicitShader = false; stageCount = 1;
    ri.gameId = COM_GAME_MOHAA;
    /* the filter refuses picmip: the surface's own choice loses it */
    rawNoPicMip = true;
    R_FindShaderInternal( "models/c/skin.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, both );
    assert( lastFlags == IMGFLAG_MIPMAP );
    R_FindShaderInternal( "models/c/nomipmaps.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, IMGFLAG_PICMIP );
    assert( lastFlags == IMGFLAG_NONE );
    /* the filter allows it: `nopicmip` and `nomipmaps` still decide */
    rawNoPicMip = false;
    R_FindShaderInternal( "textures/c/skin.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, both );
    assert( lastFlags == both );
    R_FindShaderInternal( "textures/c/nopicmip.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, IMGFLAG_MIPMAP );
    assert( lastFlags == IMGFLAG_MIPMAP );
    return 0;
}
'''


def mohaa_pak_texts(root):
    """Every .tik, models/ .txt (TIKI includes) and scripts/*.shader text."""
    texts = {}
    for pak in sorted((root / 'main').glob('*.pk3'), key=lambda p: p.name.lower()):
        with zipfile.ZipFile(pak) as archive:
            for member in archive.namelist():
                lower = member.lower()
                if (lower.endswith('.tik') or (lower.startswith('models/') and lower.endswith('.txt'))
                        or (lower.startswith('scripts/') and lower.endswith('.shader'))):
                    texts[lower] = archive.read(member).decode('latin-1')
    return texts


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
                # the Ritual profiles share the condition
                # (ritual_shader_gaps_tests.py)
                self.assertIn('tr.sunShader = ri.gameId == COM_GAME_WOLFET || ri.gameId == COM_GAME_MOHAA ||\n'
                              '\t\tCOM_GAME_ID_IS_RITUAL( ri.gameId ) ? NULL : R_FindShader( "sun", LIGHTMAP_NONE, qtrue );',
                              read(f'code/{renderer}/tr_shader.c'))

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

    def test_tiki_raw_images_obey_the_picmip_filter_under_mohaa(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                # tr_mohaa.c hands each surface's own mip and picmip choices
                # to the branch that keeps a caller's flags
                self.assertIn('? R_FindAliceSurfaceShader( info.shaders[skin],', read(f'code/{renderer}/tr_mohaa.c'))
                internal = model_shader_tests.function(read(f'code/{renderer}/tr_shader.c'),
                                                       'static shader_t *R_FindShaderInternal(')
                chosen = internal.index('else if ( rawImageFlags >= 0 )')
                branch = internal[chosen:internal.index('else if (mipRawImage)', chosen)]
                self.assertIn('( COM_GAME_ID_IS_RITUAL( ri.gameId ) || ri.gameId == COM_GAME_MOHAA ) && shader.noPicMip',
                              branch)
        model_shader_tests.ModelShaderTests.run_fixture(self, model_shader_tests.shader_fixture, MOHAA_RAW_FLAG_CHECKS)

    def test_no_retail_tiki_surface_names_an_image(self):
        """Why the change above alters no 1.00 image: every surface shader
        1.00's TIKI files name is a script's, or names nothing the paks hold,
        and none is an image file. Its 60 picmip-only "reused image" reports
        come from the filter judging a model script (`car`, `static_v2`) and a
        world script (`textures/...`) that share an image."""
        if not (MOHAA_ROOT / 'main' / 'Pak0.pk3').is_file():
            self.skipTest('needs the Medal of Honor 1.00 installation')
        texts = mohaa_pak_texts(MOHAA_ROOT)
        definitions = set()
        for name, text in texts.items():
            if name.startswith('scripts/'):
                depth = 0
                for token in re.findall(r'[{}]|[^\s{}]+', re.sub(r'//[^\n]*', '', text)):
                    depth += token == '{'
                    depth -= token == '}'
                    if depth == 0 and token not in '{}':
                        definitions.add(token.lower())
        images = set()
        for pak in (MOHAA_ROOT / 'main').glob('*.pk3'):
            with zipfile.ZipFile(pak) as archive:
                images.update(member.lower().rsplit('.', 1)[0] for member in archive.namelist()
                              if member.lower().endswith(('.tga', '.jpg')))
        shaders = []
        for name, text in texts.items():
            if not name.startswith('scripts/'):
                text = re.sub(r'//[^\n]*', '', re.sub(r'/\*.*?\*/', '', text, flags=re.S))
                shaders += re.findall(r'(?im)^[ \t]*surface[ \t]+\S+.*?[ \t]shader[ \t]+(\S+)', text)
        self.assertGreater(len(shaders), 2500)
        self.assertEqual([shader for shader in shaders if '.' in shader], [])
        unscripted = {shader.lower() for shader in shaders} - definitions
        self.assertLess(len(unscripted), 12)
        self.assertEqual(unscripted & images, set())


if __name__ == '__main__':
    unittest.main()
