"""American McGee's Alice and Heavy Metal: F.A.K.K.2: the shader gaps a
developer sweep of all 66 retail maps found.

Every Alice map printed "Shader image unavailable for: sun": the renderer
looked up Quake III's sun, which neither Ritual engine does (alice.exe and
fakk2.exe 1.02 name "sun" only as a lens-flare section) and which Alice ships
no script or image for. F.A.K.K.2's maps printed hundreds of "reused image
... with mixed flags": a TIKI surface's raw image took the flags its model
chose and skipped r_picmipFilter, which a script naming the same image obeyed,
so whichever loaded first decided the image's picmip.
Record: docs/fntech3/2026-10-15-ritual-shader-sweep.md."""
import os
import pathlib
import re
import unittest
import zipfile

import model_shader_tests

ROOT = pathlib.Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
ALICE_ROOT = pathlib.Path(os.environ.get('FNTECH3_ALICE_ROOT', r'E:\Games\Alice'))
FAKK2_ROOT = pathlib.Path(os.environ.get('FNTECH3_FAKK2_ROOT', str(ROOT / '.tmp' / 'fakk2-install' / 'game')))


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8', errors='replace')


# A model's raw image, through the real R_FindShaderInternal of each renderer
# (model_shader_tests.py's fixture). `rawNoPicMip` stands for InitShader's
# r_picmipFilter verdict on the image's path: the default filter allows picmip
# under textures/ alone, so a models/ skin is refused it.
RAW_FLAG_CHECKS = r'''
#include <cstdlib>
int main() {
#ifdef _MSC_VER
    _set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
#endif
    const imgFlags_t both = (imgFlags_t)( IMGFLAG_MIPMAP | IMGFLAG_PICMIP );
    explicitShader = false; stageCount = 1;
    /* the filter refuses picmip: Ritual's raw images lose it, as a script's would */
    rawNoPicMip = true;
    ri.gameId = COM_GAME_FAKK2;
    R_FindShaderInternal( "models/weapons/sword/sword1.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, both );
    assert( lastFlags == IMGFLAG_MIPMAP );
    ri.gameId = COM_GAME_ALICE;
    R_FindShaderInternal( "models/a/nomipmaps.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, IMGFLAG_PICMIP );
    assert( lastFlags == IMGFLAG_NONE );
    R_FindShaderInternal( "models/a/mip-only.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, IMGFLAG_MIPMAP );
    assert( lastFlags == IMGFLAG_MIPMAP );
    /* the filter allows it: the model's own choice stands */
    rawNoPicMip = false;
    R_FindShaderInternal( "textures/a/wall.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, both );
    assert( lastFlags == both );
    R_FindShaderInternal( "textures/a/nopicmip.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, IMGFLAG_MIPMAP );
    assert( lastFlags == IMGFLAG_MIPMAP );
    /* Medal of Honor, the other caller, follows the filter too
       (mohaa_shader_gaps_tests.py) */
    rawNoPicMip = true;
    ri.gameId = COM_GAME_MOHAA;
    R_FindShaderInternal( "models/b/skin.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, both );
    assert( lastFlags == IMGFLAG_MIPMAP );
    /* and an unchosen raw image still follows the filter everywhere */
    ri.gameId = COM_GAME_QUAKE3;
    R_FindShaderInternal( "models/c/skin.tga", LIGHTMAP_NONE, qtrue, qfalse, qfalse, -1 );
    assert( lastFlags == IMGFLAG_MIPMAP );
    return 0;
}
'''


def shader_names(root, game):
    """Every top-level definition name of every scripts/*.shader in the paks."""
    names = {}
    for pak in sorted((root / game).glob('*.pk3'), key=lambda p: p.name.lower()):
        with zipfile.ZipFile(pak) as archive:
            for member in archive.namelist():
                lower = member.lower()
                if not (lower.startswith('scripts/') and lower.endswith('.shader')):
                    continue
                text = re.sub(r'//[^\n]*', '', archive.read(member).decode('latin-1'))
                depth = 0
                for token in re.findall(r'[{}]|[^\s{}]+', text):
                    if token == '{':
                        depth += 1
                    elif token == '}':
                        depth -= 1
                    elif depth == 0:
                        names.setdefault(token.lower(), []).append(member)
    return names


class RitualShaderGapTests(unittest.TestCase):
    def test_the_renderer_looks_up_no_quake3_sun_under_either_ritual_profile(self):
        statements = set()
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                shader = read(f'code/{renderer}/tr_shader.c')
                start = shader.index('\ttr.sunShader = ')
                statement = shader[start:shader.index(';', start) + 1]
                condition, found = statement.split('? NULL :')
                for profile in ('ri.gameId == COM_GAME_WOLFET', 'ri.gameId == COM_GAME_MOHAA',
                                'COM_GAME_ID_IS_RITUAL( ri.gameId )'):
                    self.assertIn(profile, condition)
                self.assertEqual(found.strip(), 'R_FindShader( "sun", LIGHTMAP_NONE, qtrue );')
                self.assertEqual(shader.count('R_FindShader( "sun"'), 1)
                statements.add(statement)
        self.assertEqual(len(statements), 1)

    def test_raw_model_images_obey_the_picmip_filter_under_ritual_profiles(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                internal = model_shader_tests.function(read(f'code/{renderer}/tr_shader.c'),
                                                       'static shader_t *R_FindShaderInternal(')
                chosen = internal.index('else if ( rawImageFlags >= 0 )')
                branch = internal[chosen:internal.index('else if (mipRawImage)', chosen)]
                self.assertIn('flags = (imgFlags_t)rawImageFlags;', branch)
                self.assertIn('if ( ( COM_GAME_ID_IS_RITUAL( ri.gameId ) || ri.gameId == COM_GAME_MOHAA ) '
                              '&& shader.noPicMip ) {', branch)
                # the filter's verdict comes from InitShader, ahead of the lookup
                self.assertIn('R_ApplyShaderPicMipFilter();',
                              model_shader_tests.function(read(f'code/{renderer}/tr_shader.c'), 'static void InitShader('))
        model_shader_tests.ModelShaderTests.run_fixture(self, model_shader_tests.shader_fixture, RAW_FLAG_CHECKS)

    def test_neither_ritual_title_ships_a_sun_the_engine_should_draw(self):
        """The data the sun change rests on: Alice defines no `sun`, and
        F.A.K.K.2's one script is its edensky model's `sun` surface."""
        alice = ALICE_ROOT / 'base' / 'pak0.pk3'
        fakk2 = FAKK2_ROOT / 'fakk' / 'pak0.pk3'
        if not alice.is_file() or not fakk2.is_file():
            self.skipTest('needs the Alice and F.A.K.K.2 installations')
        self.assertNotIn('sun', shader_names(ALICE_ROOT, 'base'))
        self.assertEqual(shader_names(FAKK2_ROOT, 'fakk')['sun'], ['scripts/beau.shader'])
        with zipfile.ZipFile(fakk2) as archive:
            tiki = archive.read('models/fx_edensky2.tik').decode('latin-1')
        self.assertRegex(tiki, r'(?m)^\s*surface\s+sun\s+shader\s+sun\s*$')


if __name__ == '__main__':
    unittest.main()
