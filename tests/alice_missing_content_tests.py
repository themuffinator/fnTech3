"""How Alice's engine answers for content its own paks do not ship.

Retail content names a few models that are not there: gvillage's and
pandemonium's `smoke1` entities name `smoke.tik`, which no pak holds, and the
client and server games register the sprites `targeted_outofrange.spr` and
`crosshair.spr`, which no shipped script or image defines. Retail alice.exe
answers each as follows, and these checks hold fnTech3 to it:

* A missing definition is a developer's note from the definition loader,
  "Tiki:LoadFile Couldn't load %s" (TIKI_LoadTIKIfile 0x0045bcf7 into the
  script loader's Com_DPrintf at 0x0046637d); the renderer that asked says
  nothing (R_RegisterModel 0x0048982e..0x00489ee7).
* A sprite looks its shader up without mipmaps, picmip or wrapping
  (0x004afb4d..0x004afb57). When neither a script nor an image has that name,
  R_FindShader hands back the default shader itself (FinishShader
  0x0048e3ed..0x0048e410), so the sprite takes the default image's size and
  registers; its "Could not find image for sprite" is an ERR_DROP retail never
  reaches that way (0x004afbc1).
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def read(relative):
    return (ROOT / relative).read_text(encoding='latin-1')


class AliceMissingContentTests(unittest.TestCase):
    def test_a_missing_sprite_shader_becomes_the_default_shader(self):
        gl = function(read('code/renderer/tr_alice.c'), 'qboolean R_LoadAliceSprite(')
        lookup = gl.index('shader = R_FindShader( shaderName, LIGHTMAP_NONE, qfalse );')
        fallback = gl.index('if ( shader->defaultShader ) {\n\t\tshader = tr.defaultShader;\n\t}')
        self.assertLess(lookup, fallback)
        # the default shader's image sizes the sprite
        self.assertLess(fallback, gl.index('image = shader->stages[stage]->bundle[0].image[0];'))
        self.assertNotIn('LIGHTMAP_NONE, qtrue', gl)
        for renderer in RENDERERS[1:]:
            with self.subTest(renderer=renderer):
                other = function(read(f'code/{renderer}/tr_alice.c'), 'qboolean R_LoadAliceSprite(')
                self.assertEqual(other, gl)

    def test_a_missing_definition_is_a_developer_note(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                tiki = function(read(f'code/{renderer}/tr_alice.c'), 'qboolean R_LoadAliceTiki(')
                self.assertIn('ri.Printf( PRINT_DEVELOPER, "R_LoadAliceTiki: %s did not load\\n", name );', tiki)
                self.assertNotIn('PRINT_WARNING', tiki)
        source = read('code/qcommon/alice_tiki.cpp')
        report = function(source, 'void TikiError( const char *fmt, ... ) {')
        self.assertIn('Com_DPrintf( "%s", text );', report)
        compile_text = function(source, 'bool CompileText( const char *name, Model &model ) {')
        missing = compile_text[:compile_text.index('std::string text( buffer.c, buffer.c + length );')]
        # only a file that is not there: an empty one is read and refused
        self.assertIn('if ( length < 0 ) {\n\t\t\tTikiError( "Tiki:LoadFile Couldn\'t load %s\\n", name );', missing)
        self.assertIn('FS_FreeFile( buffer.v );', missing)


if __name__ == '__main__':
    unittest.main()
