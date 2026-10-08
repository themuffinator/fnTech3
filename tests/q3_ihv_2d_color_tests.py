"""The IHV Test's 2D art takes the 2D colour in every renderer (tr_q3_ihv_2d.h).

quake3.exe's Draw_StretchPic (0x43fd60) draws a shader's first image times the
current colour, whatever its stages ask, and the build's nomipmaps shaders are
its 2D art. fnTech3 keeps its own shader pipeline and lets those stages take
the vertex colour instead of an identity one, for that profile alone.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from model_shader_tests import common

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

CHECKS = r'''
#ifndef MAX_SHADER_STAGES
#define MAX_SHADER_STAGES 8
#endif
#ifdef USE_VULKAN
struct textureBundle_t { colorGen_t rgbGen; alphaGen_t alphaGen; };
struct shaderStage_t { qboolean active; textureBundle_t bundle[2]; };
#define GEN(s) ((s).bundle[0])
#else
struct shaderStage_t { qboolean active; colorGen_t rgbGen; alphaGen_t alphaGen; };
#define GEN(s) (s)
#endif
static struct { unsigned noMipMaps:1; } shader;
static shaderStage_t stages[MAX_SHADER_STAGES];
#include "code/renderercommon/tr_q3_ihv_2d.h"
static void Set(int count, colorGen_t rgb, alphaGen_t alpha) {
 for (auto &s : stages) s = {};
 for (int i = 0; i < count; ++i) { stages[i].active = qtrue; GEN(stages[i]).rgbGen = rgb; GEN(stages[i]).alphaGen = alpha; }
}
int main() {
 // An IHV nomipmaps stage with an identity colour takes the vertex colour.
 ri.gameId = COM_GAME_QUAKE3_IHV; shader.noMipMaps = 1;
 for (colorGen_t rgb : {CGEN_IDENTITY, CGEN_IDENTITY_LIGHTING}) for (alphaGen_t alpha : {AGEN_IDENTITY, AGEN_SKIP}) {
  Set(2, rgb, alpha); R_Q3IHV2DStageColors();
  for (int i = 0; i < 2; ++i) assert(GEN(stages[i]).rgbGen == CGEN_VERTEX && GEN(stages[i]).alphaGen == AGEN_VERTEX);
  assert(!stages[2].active && GEN(stages[2]).rgbGen == colorGen_t(0));
 }
 // A stage that already colours itself keeps its rule.
 for (colorGen_t rgb : {CGEN_ENTITY, CGEN_CONST, CGEN_WAVEFORM, CGEN_VERTEX, CGEN_EXACT_VERTEX}) {
  Set(1, rgb, AGEN_ENTITY); R_Q3IHV2DStageColors();
  assert(GEN(stages[0]).rgbGen == rgb && GEN(stages[0]).alphaGen == AGEN_ENTITY);
 }
 // A mipmapped shader, and every other profile, is untouched.
 shader.noMipMaps = 0; Set(1, CGEN_IDENTITY, AGEN_IDENTITY); R_Q3IHV2DStageColors();
 assert(GEN(stages[0]).rgbGen == CGEN_IDENTITY && GEN(stages[0]).alphaGen == AGEN_IDENTITY);
 shader.noMipMaps = 1;
 for (comGameId_t profile : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_Q3TEST, COM_GAME_RTCW_MP, COM_GAME_WOLFET}) {
  ri.gameId = profile; Set(1, CGEN_IDENTITY, AGEN_IDENTITY); R_Q3IHV2DStageColors();
  assert(GEN(stages[0]).rgbGen == CGEN_IDENTITY && GEN(stages[0]).alphaGen == AGEN_IDENTITY);
 }
 return 0;
}
'''


class Q3IHV2DColorTests(unittest.TestCase):
    def test_nomipmaps_stages_take_the_2d_colour(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='q3-ihv-2d-', dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
                    records = ''
                    for name in ('colorGen_t', 'alphaGen_t'):
                        end = local.index('} ' + name + ';') + len('} ' + name + ';')
                        records += local[local.rindex('typedef ', 0, end):end] + '\n'
                    vulkan = '#define USE_VULKAN\n' if renderer != 'renderer' else ''
                    cpp = Path(temp) / (renderer + '.cpp')
                    exe = Path(temp) / (renderer + ('.exe' if os.name == 'nt' else ''))
                    cpp.write_text(common(renderer) + records + vulkan + '#include <cassert>\n' + CHECKS)
                    build = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    # Each renderer calls it once per shader, before its needs are read.
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    self.assertEqual(source.count('R_Q3IHV2DStageColors();'), 1)
                    self.assertLess(source.index('R_Q3IHV2DStageColors();'), source.index('\tDetectNeeds();'))


if __name__ == '__main__':
    unittest.main()
