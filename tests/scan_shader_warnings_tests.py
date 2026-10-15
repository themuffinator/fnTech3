"""The shader-warning scanner groups a sweep's developer logs by kind and map."""
from pathlib import Path
import importlib.util
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('scan_shader_warnings', ROOT / 'scripts' / 'scan_shader_warnings.py')
scan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)


class ShaderWarningScanTests(unittest.TestCase):
    def test_each_kind_is_recognised(self):
        cases = {
            "WARNING: unknown general shader parameter 'foo' in 'bar'": 'keyword',
            "WARNING: unknown parameter 'alpahFunc' in shader 'seagull'": 'keyword',
            "WARNING: unknown deformVertexes subtype 'x' found in shader 'y'": 'keyword',
            "WARNING: R_FindImageFile could not find 'textures/a.tga' in shader 'a'": 'image',
            'Shader image unavailable for: textures/notexture': 'default',
            "WARNING: shader 'x' has lightmap but no lightmap stage!": 'lightmap',
            "WARNING: nextBundle without an image in shader 'bp_debris3'": 'malformed',
            "WARNING: no concluding '}' in shader x": 'malformed',
            "WARNING: invalid cull parm 'front' in shader 'hedge'": 'keyword',
            "WARNING: RE_RemapShader: shader x not found": 'other',
        }
        for line, kind in cases.items():
            with self.subTest(line=line):
                self.assertEqual(scan.classify(line), kind)

    def test_unrelated_lines_are_ignored(self):
        for line in ('WARNING: reused image x.tga with mixed flags (5 vs 1)',
                     "TIKI_InitTiki: could not find surface 'origin' in 'a.tik'",
                     'Initializing Shaders', ''):
            with self.subTest(line=line):
                self.assertIsNone(scan.classify(line))

    def test_logs_are_grouped_by_line_with_their_maps(self):
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as folder:
            logs = Path(folder)
            missing = "^3WARNING: R_FindImageFile could not find 'gfx/2d/x.tga' in shader 'console'\n"
            (logs / 'mohaa_m1l1-console.log').write_text(missing + '^5Shader image unavailable for: poopy\n',
                                                         encoding='latin-1')
            (logs / 'mohaa_DM_mohdm1-console.log').write_text(missing, encoding='latin-1')
            found = scan.scan(sorted(logs.glob('*-console.log')))
        line = "WARNING: R_FindImageFile could not find 'gfx/2d/x.tga' in shader 'console'"
        self.assertEqual(found['image'][line], {'mohaa_m1l1', 'mohaa_DM_mohdm1'})
        self.assertEqual(found['default']['Shader image unavailable for: poopy'], {'mohaa_m1l1'})


if __name__ == '__main__':
    unittest.main()
