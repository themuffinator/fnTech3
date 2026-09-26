"""Compare real renderer shadow geometry with retail SP/Holomatch observations."""
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


class STVEFShadowTests(unittest.TestCase):
    def test_native_projection_extrusion_and_other_profile_policy(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        target = compiler_target_args(compiler) if os.name == 'nt' else ['-m32']
        sanitize = [] if os.name == 'nt' else [
            '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        expected = json.loads((ROOT / 'tests/fixtures/stvef_shadow_retail.json').read_text())
        creation = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            source = (ROOT / f'code/{renderer}/tr_shadows.c').read_text()
            # Run the complete production setup/extrusion before its GL/Vulkan
            # dispatch, and the complete projection deformation function.
            stencil = function(source, 'void RB_ShadowTessEnd(')
            stencil = stencil[:stencil.index('// decide which triangles')] + '}\n'
            projection = function(source, 'void RB_ProjectionShadowDeform(')
            for per_model_light in (False, True):
                with self.subTest(renderer=renderer, per_model_light=per_model_light), \
                        tempfile.TemporaryDirectory(prefix='stvef-shadow-', dir=ROOT / '.tmp') as folder:
                    folder = Path(folder)
                    (folder / 'stvef_shadow_implementation.inc').write_text(stencil + projection)
                    binary = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                    flags = [] if per_model_light else ['-DPROBE_NO_PMLIGHT']
                    if renderer != 'renderer':
                        flags.append('-DUSE_VULKAN')
                    built = subprocess.run([
                        compiler, *target, *sanitize, *flags, '-std=c99', '-O1',
                        '-I', str(folder), str(ROOT / 'tests/stvef_shadow_fixture.c'),
                        '-o', str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    for profile in range(7):
                        for light_mode in ((0, 1, 2) if per_model_light else (0,)):
                            ran = subprocess.run([str(binary), str(profile), str(light_mode)],
                                                 cwd=ROOT, capture_output=True, text=True,
                                                 timeout=30, **creation)
                            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
                            baseline = expected['retail' if profile in (5, 6) and light_mode != 2 else 'legacy']
                            actual = json.loads(ran.stdout)
                            self.assertEqual(len(actual), len(baseline))
                            for observed, ours in zip(baseline, actual):
                                with self.subTest(profile=profile, light_mode=light_mode,
                                                  kind=observed['type'], variant=observed['variant']):
                                    self.assertEqual(ours['type'], observed['type'])
                                    self.assertEqual(ours['variant'], observed['variant'])
                                    self.assertEqual(len(ours['vertices']), len(observed['vertices']))
                                    for a, b in zip(observed['vertices'], ours['vertices']):
                                        for x, y in zip(a, b):
                                            self.assertAlmostEqual(x, y, delta=0.00003)
                    ran = subprocess.run([str(binary), 'empty'], cwd=ROOT, capture_output=True,
                                         text=True, timeout=30, **creation)
                    self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
