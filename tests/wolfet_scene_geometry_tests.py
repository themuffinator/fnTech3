"""ET's polygon buffer and its runtime shader definitions."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
RENDER = ROOT/'code/client/client_wolfet_render_compat.hpp'
DYNAMIC = ROOT/'code/renderercommon/tr_wolfet_dynamic_shader.h'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-scene-geometry.md'
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def _function(path, signature):
    lines = path.read_text(encoding='utf-8').split('\n')
    first = next(i for i, line in enumerate(lines) if line.startswith(signature))
    last = next(i for i in range(first + 1, len(lines)) if lines[i] == '}')
    return '\n'.join(lines[first:last + 1])


class WolfETSceneGeometryTests(unittest.TestCase):
    def test_a_polygon_buffer_never_retains_module_memory(self):
        """ET's own renderer holds the module's buffer until the frame draws;
        this engine copies the triangles out instead."""
        region = _function(RENDER, 'inline bool CL_WolfETRenderCall')
        block = region[region.index('CG_R_ADDPOLYBUFFERTOSCENE'):]
        block = block[:block.index('case Call::CG_R_LOADDYNAMICSHADER')]
        self.assertIn('Hunk_AllocateTempMemory', block)
        self.assertIn('re.AddPolyToScene( buffer->shader, 3, expanded, indices / 3 );', block)

    def test_a_polygon_buffer_is_bounded_and_whole(self):
        region = _function(RENDER, 'inline bool CL_WolfETRenderCall')
        block = region[region.index('CG_R_ADDPOLYBUFFERTOSCENE'):]
        block = block[:block.index('case Call::CG_R_LOADDYNAMICSHADER')]
        self.assertIn('vertices > FNTECH3_WOLFET_MAX_PB_VERTS', block)
        self.assertIn('indices > FNTECH3_WOLFET_MAX_PB_INDICIES', block)
        # Not whole triangles, or naming a vertex it does not carry, is refused.
        self.assertIn('if ( indices % 3 || !indices || !vertices ) break;', block)
        self.assertIn('ET polygon buffer index out of range', block)

    def test_runtime_shaders_are_searched_before_the_loaded_text(self):
        for renderer in RENDERERS:
            text = (ROOT/('code/%s/tr_shader.c' % renderer)).read_text(encoding='utf-8')
            lookup = text.index('static const char *FindShaderInShaderText')
            body = text[lookup:lookup + 900]
            self.assertIn('R_WolfETFindDynamicShader( shadername )', body, renderer)
            self.assertLess(body.index('R_WolfETFindDynamicShader'),
                            body.index('generateHashValue'), renderer)

    def test_a_runtime_definition_is_copied_not_referenced(self):
        text = DYNAMIC.read_text(encoding='utf-8')
        self.assertIn('Com_Memcpy( added->text, text, bytes );', text)
        self.assertIn('R_WolfETClearDynamicShaders', text)

    def test_the_three_argument_forms_behave_as_et_does(self):
        text = DYNAMIC.read_text(encoding='utf-8')
        # Text with no name is refused; neither empties the list; a name alone removes one.
        self.assertIn('shader text with no name', text)
        self.assertIn('R_WolfETClearDynamicShaders();\n\t\treturn qtrue;', text)
        self.assertIn('already exists', text)
        # A definition that does not declare its own name could never be found.
        self.assertIn('does not declare that name', text)

    def test_runtime_definitions_are_bounded(self):
        text = DYNAMIC.read_text(encoding='utf-8')
        self.assertIn('FNTECH3_WOLFET_MAX_DYNAMIC_SHADERS', text)
        self.assertIn('FNTECH3_WOLFET_MAX_DYNAMIC_SHADER_BYTES', text)
        self.assertIn('strlen( name ) >= MAX_QPATH', text)

    def test_every_renderer_registers_the_entry_point(self):
        for renderer in RENDERERS:
            init = (ROOT/('code/%s/tr_init.c' % renderer)).read_text(encoding='utf-8')
            self.assertIn('re.LoadWolfETDynamicShader = RE_LoadWolfETDynamicShader;', init, renderer)

    def test_the_lookup_answers_only_for_et(self):
        text = DYNAMIC.read_text(encoding='utf-8')
        self.assertIn('ri.gameId != COM_GAME_WOLFET', text)

    def test_the_record_states_the_coverage(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('142 of the 147', record)


if __name__ == '__main__':
    unittest.main()
