"""The IHV Test's overlay square draws its shader's first image in the overlay colour.

quake3.exe 4.01's R_BlendBlobs (0x442700) binds the overlay shader's first
stage image and draws it with glColor4fv of the overlay's four words, alpha
blending and no depth test, reading none of the stage's colour rules. Its
client game draws the damage blood splat that way with a shader whose stage
asks for rgbGen entity. RE_RegisterIHVOverlayShader (tr_q3_ihv_overlay_shader.h)
gives each renderer a 2D shader holding that image with the vertex colour.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from model_shader_tests import PRELUDE

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

HOST = r'''
#ifndef MAX_SHADER_STAGES
#define MAX_SHADER_STAGES 8
#endif
struct image_t { int id; };
struct textureBundle_t { image_t *image[1]; texCoordGen_t tcGen;
#ifdef USE_VULKAN
 colorGen_t rgbGen; alphaGen_t alphaGen;
#endif
};
struct shaderStage_t { qboolean active; textureBundle_t bundle[2]; unsigned stateBits;
#ifndef USE_VULKAN
 colorGen_t rgbGen; alphaGen_t alphaGen;
#endif
};
struct shader_t { char name[MAX_QPATH]; int index, lightmapIndex; qboolean defaultShader;
 cullType_t cullType; int sort; shaderStage_t *stages[MAX_SHADER_STAGES]; };
static shader_t shader;
static shaderStage_t stages[MAX_SHADER_STAGES];
static std::vector<std::unique_ptr<shader_t>> made;
static std::vector<std::unique_ptr<shaderStage_t>> madeStages;
static shader_t source{"models/weaphits/blood201", 41}, fallback{"<default>", 0};
static shaderStage_t sourceStage;
static image_t blood{201};
static int inits, finishes;
static struct { comGameId_t gameId; } ri{COM_GAME_QUAKE3_IHV};
static struct { qboolean registered; shader_t *defaultShader; } tr{qtrue, &fallback};
int QDECL Com_sprintf(char *out, int size, const char *format, ...) {
 va_list args; va_start(args, format); const int n = std::vsnprintf(out, size_t(size), format, args); va_end(args); return n;
}
static shader_t *R_GetShaderByHandle(qhandle_t handle) {
 if (handle == source.index) return &source;
 for (auto &sh : made) if (sh->index == handle) return sh.get();
 return &fallback;
}
static shader_t *R_FindShaderByName(const char *name) {
 for (auto &sh : made) if (!std::strcmp(sh->name, name)) return sh.get();
 return tr.defaultShader;
}
static void InitShader(const char *name, int lightmap) {
 ++inits; shader = {}; for (auto &s : stages) s = {};
 std::snprintf(shader.name, sizeof shader.name, "%s", name); shader.lightmapIndex = lightmap;
}
static shader_t *FinishShader() {
 ++finishes; madeStages.push_back(std::make_unique<shaderStage_t>(stages[0]));
 made.push_back(std::make_unique<shader_t>(shader));
 made.back()->index = 100 + int(made.size()); made.back()->stages[0] = madeStages.back().get();
 return made.back().get();
}
#include "code/renderercommon/tr_q3_ihv_overlay_shader.h"
#ifdef USE_VULKAN
#define GEN(s) ((s).bundle[0])
#else
#define GEN(s) (s)
#endif
int main() {
 sourceStage.active = qtrue; sourceStage.bundle[0].image[0] = &blood; source.stages[0] = &sourceStage;
 // The splat's first image, in the vertex colour and alpha, blended, on top.
 const qhandle_t overlay = RE_RegisterIHVOverlayShader(41);
 assert(overlay == 101 && inits == 1 && finishes == 1);
 const shader_t *made0 = made[0].get();
 assert(!std::strcmp(made0->name, "models/weaphits/blood201#ihvoverlay") && made0->lightmapIndex == LIGHTMAP_2D);
 const shaderStage_t &stage = *made0->stages[0];
 assert(stage.active && stage.bundle[0].image[0] == &blood && stage.bundle[0].tcGen == TCGEN_TEXTURE);
 assert(GEN(stage).rgbGen == CGEN_VERTEX && GEN(stage).alphaGen == AGEN_VERTEX);
 assert(stage.stateBits == (GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHTEST_DISABLE));
 assert(made0->cullType == CT_TWO_SIDED);
 // Asked again, the same shader is found, not made twice.
 assert(RE_RegisterIHVOverlayShader(41) == overlay && inits == 1);
 // No active first stage, no image or the default shader: nothing to draw.
 sourceStage.bundle[0].image[0] = nullptr; assert(!RE_RegisterIHVOverlayShader(41));
 sourceStage.bundle[0].image[0] = &blood; sourceStage.active = qfalse; assert(!RE_RegisterIHVOverlayShader(41));
 sourceStage.active = qtrue; source.stages[0] = nullptr; assert(!RE_RegisterIHVOverlayShader(41));
 source.stages[0] = &sourceStage; source.defaultShader = qtrue; assert(!RE_RegisterIHVOverlayShader(41));
 source.defaultShader = qfalse; assert(!RE_RegisterIHVOverlayShader(0));
 tr.registered = qfalse; assert(!RE_RegisterIHVOverlayShader(41)); tr.registered = qtrue;
 // Every other profile makes none.
 for (comGameId_t profile : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_Q3TEST, COM_GAME_RTCW_MP, COM_GAME_WOLFET}) {
  ri.gameId = profile; assert(!RE_RegisterIHVOverlayShader(41));
 }
 assert(inits == 1 && finishes == 1);
 return 0;
}
'''


def records(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    text = ''
    for name in ('colorGen_t', 'alphaGen_t', 'texCoordGen_t'):
        end = local.index('} ' + name + ';') + len('} ' + name + ';')
        text += local[local.rindex('typedef ', 0, end):end] + '\n'
    for name in ('GLS_SRCBLEND_SRC_ALPHA', 'GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA', 'GLS_DEPTHTEST_DISABLE'):
        text += re.search(r'^#define\s+' + name + r'\s+\S+', local, re.M)[0] + '\n'
    sort = re.search(r'SS_BLEND0\s*,?', local)
    assert sort, renderer
    text += '#ifndef SS_BLEND0\n#define SS_BLEND0 9\n#endif\n'
    common = (ROOT / 'code' / renderer / 'tr_common.h').read_text()
    end = common.index('} cullType_t;') + len('} cullType_t;')
    text += common[common.rindex('typedef ', 0, end):end] + '\n'
    text += '\n'.join(re.findall(r'^#define LIGHTMAP_[^\n]+', common, re.M)) + '\n'
    return text


class Q3IHVOverlayShaderTests(unittest.TestCase):
    def test_overlay_shader_is_the_first_image_in_the_vertex_colour(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='q3-ihv-overlay-', dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    vulkan = '#define USE_VULKAN\n' if renderer != 'renderer' else ''
                    cpp = Path(temp) / (renderer + '.cpp')
                    exe = Path(temp) / (renderer + ('.exe' if os.name == 'nt' else ''))
                    cpp.write_text(PRELUDE + vulkan + records(renderer) + HOST)
                    build = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout[-3000:] + build.stderr[-3000:])
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_every_renderer_exports_it_and_the_client_draws_with_it(self):
        public = (ROOT / 'code/renderercommon/tr_public.h').read_text()
        self.assertIn('qhandle_t (*RegisterIHVOverlayShader)( qhandle_t shader );', public)
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                folder = ROOT / 'code' / renderer
                self.assertIn('qhandle_t RE_RegisterIHVOverlayShader( qhandle_t hShader );',
                              (folder / 'tr_local.h').read_text())
                self.assertIn('re.RegisterIHVOverlayShader = RE_RegisterIHVOverlayShader;',
                              (folder / 'tr_init.c').read_text())
                self.assertEqual((folder / 'tr_shader.c').read_text().count(
                    '#include "../renderercommon/tr_q3_ihv_overlay_shader.h"'), 1)
        client = (ROOT / 'code/client/cl_q3ihv_cgame.cpp').read_text()
        draw = client[client.index('if ( polyblend->value != 0.0f && scene.view.OverlayRect('):]
        draw = draw[:draw.index('re.SetColor( nullptr );')]
        self.assertIn('re.RegisterIHVOverlayShader( scene.view.overlayShader )', draw)
        self.assertIn('re.SetColor( scene.view.overlayColor );', draw)


if __name__ == '__main__':
    unittest.main()
