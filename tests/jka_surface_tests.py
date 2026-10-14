"""Draw Jedi Academy's saber glow, line and oriented line through every
renderer and compare them with the vertices retail jamp.exe 1.01's own
routines drew from the same synthetic cases (tests/jka_surface_cases.h,
tests/fixtures/jka_surface_retail.json)."""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


class JKASurfaceTests(unittest.TestCase):
    def test_every_renderer_draws_retail_s_geometry(self):
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
        retail = json.loads((ROOT / 'tests/fixtures/jka_surface_retail.json').read_text())['cases']
        self.assertEqual([case['case'] for case in retail], list(range(len(retail))))
        creation = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(
                    prefix='jka-surface-', dir=ROOT / '.tmp') as folder:
                folder = Path(folder)
                surfaces = (ROOT / f'code/{renderer}/tr_surface.c').read_text()
                self.assertIn('#include "../renderercommon/tr_jka_surface.h"', surfaces)
                self.assertIn('if ( !RB_SurfaceSTVEF() && !RB_SurfaceJKA() ) {', surfaces)
                quad = function(surfaces, 'void RB_AddQuadStampExt(')
                (folder / 'jka_surface_implementation.inc').write_text(
                    helpers + '\n' + quad + '\n#include "code/renderercommon/tr_stvef_surface.h"\n'
                    '#include "code/renderercommon/tr_jka_surface.h"\n')
                binary = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                built = subprocess.run([
                    compiler, *target, *sanitize, '-std=c99', '-O1', '-I', str(ROOT),
                    '-I', str(folder), str(ROOT / 'tests/jka_surface_fixture.c'),
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
                    ours = json.loads(ran.stdout)
                    self.assertEqual(len(ours), len(retail))
                    for expected, actual in zip(retail, ours):
                        with self.subTest(mode=mode, case=expected['case'], type=expected['type']):
                            self.assertEqual(actual['type'], expected['type'])
                            self.assertEqual(actual['randomCalls'], expected['randomCalls'])
                            self.assertEqual(actual['indexes'], expected['indexes'])
                            # A blade takes retail's number of sprites.
                            self.assertEqual(len(actual['vertices']), len(expected['vertices']))
                            for observed, vertex in zip(expected['vertices'], actual['vertices']):
                                self.assertEqual(vertex[3:], observed[3:])  # UV and colour bytes.
                                for a, b in zip(observed[:3], vertex[:3]):
                                    self.assertAlmostEqual(a, b, delta=0.00002)

    def test_the_front_end_takes_them_as_retail_s_takes_its_sprites(self):
        # Retail R_AddEntitySurfaces (jamp.exe 0x4a6442, table 0x4a65c0) sends
        # types 2-6 and 8-10 to one branch: the third-person skip, the custom
        # shader and the sprite fog test. The adapter's own types enter Fn's.
        adapter = (ROOT / 'code/compat/jka_render_adapter.hpp').read_text()
        produced = set(re.findall(r'case \d+: type = (RT_\w+); break;', adapter))
        self.assertEqual(produced, {'RT_MODEL', 'RT_SPRITE', 'RT_BEAM', 'RT_JKA_SABER_GLOW', 'RT_PORTALSURFACE',
                                    'RT_JKA_LINE', 'RT_JKA_ORIENTEDLINE'})
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                body = function((ROOT / f'code/{renderer}/tr_main.c').read_text(),
                                'static void R_AddEntitySurfaces( void ) {')
                generated = body[body.index('case RT_SPRITE:'):]
                generated = generated[:generated.index('break;')]
                for entity_type in ('RT_JKA_SABER_GLOW', 'RT_JKA_LINE', 'RT_JKA_ORIENTEDLINE'):
                    self.assertIn(f'case {entity_type}:', generated)
                self.assertIn('RF_THIRD_PERSON', generated)
                self.assertIn('R_GetShaderByHandle( ent->e.customShader )', generated)
                self.assertIn('R_SpriteFogNum( ent )', generated)
        # GLx batches the two lines as beams, as it batches Elite Force's.
        compat = (ROOT / 'code/renderer/tr_glx_compat.h').read_text()
        beams = compat[compat.index('switch ( entity->reType ) {'):compat.index('GLX_DYNAMIC_CATEGORY_MASK_BEAM;')]
        for entity_type in ('RT_JKA_LINE', 'RT_JKA_ORIENTEDLINE'):
            self.assertIn(f'case {entity_type}:', beams)

    def test_retail_fixture_names_its_origin(self):
        fixture = json.loads((ROOT / 'tests/fixtures/jka_surface_retail.json').read_text())
        provenance = fixture['provenance']
        self.assertEqual(provenance['retail'], 'jamp.exe 1.01')
        self.assertEqual(provenance['sha256'],
                         'e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b')
        self.assertEqual(provenance['entries'], {'saberGlow': '0x4ba780', 'line': '0x4bb3c0',
                                                 'orientedLine': '0x4bb4c0', 'dispatch': '0x4bdcd0'})
        # The cases include a blade, its mirror, a hilt alone and the lines'
        # degenerate eye and axis; each type's count is fixed.
        types = [case['type'] for case in fixture['cases']]
        self.assertEqual((types.count(5), types.count(8), types.count(9)), (10, 6, 6))


if __name__ == '__main__':
    unittest.main()
