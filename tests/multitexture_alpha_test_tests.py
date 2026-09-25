"""The Vulkan-family renderers' multitexture fragment shaders alpha-test.

A stage that draws two or three bundles in one pass keeps its alpha function:
Medal of Honor's `nextBundle` stages alpha-test a foliage card or a fence and
multiply the lightmap in the same stage, as GL's multitexture path does. The
USE_TX variants were compiled without USE_ATEST, so the pipeline's alpha-test
specialization constants had nothing to specialize and each card drew its
whole quad. Splitting the stage in two instead fogged the texture before the
lightmap multiplied it, darkening every fogged card.

gen_frag.tmpl tests the first bundle's alpha after combining the bundles, and
a pipeline without alpha-test bits specializes the test away (vk.c sets
constant 0 to 0), so a stage that never alpha-tested is unchanged."""
import pathlib
import re
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
MULTITEXTURE = ('frag_tx1', 'frag_tx1_ident1', 'frag_tx1_fixed', 'frag_tx1_cl', 'frag_tx2', 'frag_tx2_cl')
OP_DECORATE = 71
DECORATION_SPEC_ID = 1


def spirv_arrays(path):
    text = path.read_text()
    arrays = {}
    for match in re.finditer(r'const unsigned char (\w+)\[(\d+)\] = \{(.*?)\};', text, re.S):
        data = bytes(int(value, 16) for value in re.findall(r'0x([0-9A-Fa-f]{2})', match.group(3)))
        arrays[match.group(1)] = data
    return arrays


def spec_ids(module):
    words = struct.unpack(f'<{len(module) // 4}I', module)
    assert words[0] == 0x07230203, 'not SPIR-V'
    ids = set()
    index = 5
    while index < len(words):
        count, opcode = words[index] >> 16, words[index] & 0xFFFF
        if count == 0:
            break
        if opcode == OP_DECORATE and words[index + 2] == DECORATION_SPEC_ID:
            ids.add(words[index + 3])
        index += count
    return ids


class MultitextureAlphaTestTests(unittest.TestCase):
    def test_every_multitexture_variant_is_compiled_with_the_alpha_test(self):
        compile_bat = (ROOT / 'code/renderervk/shaders/compile.bat').read_text()
        lines = [line for line in compile_bat.splitlines()
                 if 'gen_frag.tmpl' in line and '-DUSE_TX' in line and not line.lstrip().startswith('@rem')]
        self.assertEqual(len(lines), 12)
        for line in lines:
            self.assertIn('-DUSE_ATEST', line)
        build = (ROOT / 'code/rendererrtx/shaders/build_shaders.py').read_text()
        for symbol in MULTITEXTURE:
            match = re.search(r'append_pair_variants\(variants, "gen_frag.tmpl", "frag", "' + symbol + r'", \(([^)]*)\)\)', build)
            self.assertIsNotNone(match, symbol)
            self.assertIn('"USE_ATEST"', match.group(1), symbol)

    def test_the_embedded_modules_declare_the_alpha_test_constants(self):
        for renderer in ('renderervk', 'rendererrtx'):
            arrays = spirv_arrays(ROOT / 'code' / renderer / 'shaders/spirv/shader_data.c')
            for symbol in MULTITEXTURE:
                for name in (symbol, symbol + '_fog'):
                    ids = spec_ids(arrays[name])
                    # 0: alpha-test function, 1: reference, 6: multitexture mode
                    self.assertTrue({0, 1, 6} <= ids, f'{renderer} {name}: {sorted(ids)}')

    def test_the_template_tests_the_first_bundle_after_combining(self):
        for renderer in ('renderervk', 'rendererrtx'):
            template = (ROOT / 'code' / renderer / 'shaders/gen_frag.tmpl').read_text()
            combine = template.index('#elif defined(USE_TX1)')
            test = template.index('#ifdef USE_ATEST\n\tif (alpha_to_coverage != 0) {')
            self.assertLess(combine, test, renderer)
            self.assertIn('if (color0.a < alpha_test_value) discard;', template[test:])

    def test_the_pipeline_specializes_the_test_from_the_stage_bits_for_every_type(self):
        for renderer in ('renderervk', 'rendererrtx'):
            vk = (ROOT / 'code' / renderer / 'vk.c').read_text()
            start = vk.index('atest_bits = state_bits & GLS_ATEST_BITS;')
            switch = vk[start:vk.index('};', start)]
            self.assertIn('frag_spec_data[0].i = 3; // greater or equal', switch)
            self.assertIn('frag_spec_data[0].i = 0;', switch)
            # the specialization is not limited to single-texture pipelines
            self.assertNotIn('shader_type', switch)
        for renderer in ('renderervk', 'rendererrtx'):
            self.assertNotIn('R_MOHAASplitAlphaTestedBundles',
                             (ROOT / 'code' / renderer / 'tr_shader.c').read_text())


if __name__ == '__main__':
    unittest.main()
