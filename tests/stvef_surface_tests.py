"""Execute all renderers' Holomatch effect geometry against native retail vectors."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


class STVEFSurfaceTests(unittest.TestCase):
    def test_retail_geometry_appending_flush_and_invalid_numeric_inputs(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        target = compiler_target_args(compiler) if os.name == 'nt' else ['-m32']
        sanitize = [] if os.name == 'nt' else [
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all']
        math = (ROOT / 'code/qcommon/q_math.c').read_text()
        helpers = '\n'.join(function(math, signature) for signature in (
            'float Q_fabs(', 'void RotatePointAroundVector(', 'void ProjectPointOnPlane(',
            'void MakeNormalVectors(', 'vec_t VectorNormalize(', 'void MatrixMultiply(',
            'void PerpendicularVector('))
        expected = json.loads((ROOT / 'tests/fixtures/stvef_surface_retail.json').read_text())['cases']
        creation = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(
                    prefix='stvef-surface-', dir=ROOT / '.tmp') as folder:
                folder = Path(folder)
                surfaces = (ROOT / f'code/{renderer}/tr_surface.c').read_text()
                self.assertIn('#include "../renderercommon/tr_stvef_surface.h"', surfaces)
                quad = function(surfaces, 'void RB_AddQuadStampExt(')
                (folder / 'stvef_surface_implementation.inc').write_text(
                    helpers + '\n' + quad + '\n#include "code/renderercommon/tr_stvef_surface.h"\n')
                binary = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                built = subprocess.run([
                    compiler, *target, *sanitize, '-std=c99', '-O1', '-I', str(ROOT),
                    '-I', str(folder), str(ROOT / 'tests/stvef_surface_fixture.c'),
                    '-o', str(binary), *([] if os.name == 'nt' else ['-lm'])],
                    cwd=ROOT, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                for mode in ('0', '7', 'bounds'):
                    ran = subprocess.run([str(binary), mode], cwd=ROOT, capture_output=True,
                                         text=True, timeout=30, **creation)
                    self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
                    if mode == 'bounds':
                        self.assertIn('bounds passed', ran.stdout)
                        continue
                    actual = json.loads(ran.stdout)
                    self.assertEqual(len(actual), len(expected))
                    for retail, ours in zip(expected, actual):
                        with self.subTest(mode=mode, primitive=retail['type'], variant=retail['variant']):
                            self.assertEqual(ours['type'], retail['type'])
                            self.assertEqual(ours['variant'], retail['variant'])
                            self.assertEqual(ours['indexes'], retail['indexes'])
                            self.assertEqual(len(ours['vertices']), len(retail['vertices']))
                            for observed, vertex in zip(retail['vertices'], ours['vertices']):
                                self.assertEqual(vertex[3:], observed[3:])  # UV and color bytes.
                                for a, b in zip(observed[:3], vertex[:3]):
                                    self.assertAlmostEqual(a, b, delta=0.00002)


if __name__ == '__main__':
    unittest.main()
