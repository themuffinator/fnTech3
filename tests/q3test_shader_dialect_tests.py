"""Q3Test's shader dialect, its gfx.shader repair and its 2D pictures, compiled
from the renderer headers with the engine tokenizer."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_shader_text_tests import PRELUDE, tokenizer

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def record(local, name):
    end = local.index('} ' + name + ';') + len('} ' + name + ';')
    return local[local.rindex('typedef ', 0, end):end] + '\n'


DIALECT_CHECKS = r'''
#include <cctype>
int Q_stricmp(const char *a, const char *b) {
    while (*a && *b && std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b)) { ++a; ++b; }
    return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}
static float Q_atof(const char *s) { return float(std::atof(s)); }
struct shader_t { char name[MAX_QPATH]; fogParms_t fogParms; int contentFlags; int surfaceFlags; qboolean isSky; };
static shader_t shader;
#include "code/renderercommon/tr_q3test_shader_dialect.h"
#include "code/renderercommon/tr_q3test_shader_text.h"
static void Reset() { shader = shader_t{}; std::strcpy(shader.name, "test/q3test"); }
static bool Near(float a, float b) { return std::fabs(a - b) < 1e-5f; }
int main(int argc, char **argv) {
    for (auto profile : {COM_GAME_QUAKE3, COM_GAME_QUAKE3_IHV, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_Q3TEST}) {
        ri.gameId = profile;
        const bool q3test = profile == COM_GAME_Q3TEST;
        // fogParms r g b depth type: five bare values
        Reset();
        const char *text = " 0.7 0.5 0.25 128 -1\nnext";
        const char *start = text;
        assert(R_Q3TestFogParms(&text) == (q3test ? 1 : 0));
        if (q3test) {
            assert(Near(shader.fogParms.color[0], 0.7f) && Near(shader.fogParms.color[2], 0.25f));
            assert(Near(shader.fogParms.depthForOpaque, 128) && shader.fogParms.q3testType == -1);
            assert(!std::strcmp(COM_ParseExt(&text, qtrue), "next"));
        } else {
            assert(text == start && shader.fogParms.q3testType == 0);
        }
        // Quake III's parenthesised form is left to the standard parser
        Reset();
        text = " ( 1 1 1 ) 64";
        start = text;
        assert(R_Q3TestFogParms(&text) == 0 && text == start);
        // a missing value is warned about and the shader kept, unchanged
        Reset();
        text = " 1 1\n";
        if (q3test) { assert(R_Q3TestFogParms(&text) == 1 && shader.fogParms.depthForOpaque == 0); }
        // fogonly and foggen
        Reset();
        text = " rest";
        assert(R_Q3TestFogDirective("fogonly", &text) == (q3test ? 1 : 0));
        assert(bool(shader.contentFlags & CONTENTS_FOG) == q3test);
        text = " sin .1 .2 .3 .4\nnext";
        assert(R_Q3TestFogDirective("foggen", &text) == (q3test ? 1 : 0));
        if (q3test) assert(!std::strcmp(COM_ParseExt(&text, qtrue), "next"));
        text = " x";
        assert(R_Q3TestFogDirective("fogParms", &text) == 0);
        // stageless skies
        assert(bool(R_Q3TestStagelessSky(SURF_SKY, qfalse)) == q3test);
        assert(!R_Q3TestStagelessSky(0, qfalse) && !R_Q3TestStagelessSky(SURF_SKY, qtrue));
        if (q3test) {
            const char *black = R_Q3TestBlackStage;
            assert(!std::strcmp(COM_ParseExt(&black, qtrue), "map"));
            assert(!std::strcmp(COM_ParseExt(&black, qtrue), "*white"));
        }
    }
    // clampTexCoords anywhere in the stage, not in a comment or a nested block
    assert(R_Q3TestStageClamps("\n map sprites/splash.tga\n blendFunc GL_ONE GL_ONE\n clampTexCoords\n}\n}"));
    assert(R_Q3TestStageClamps("clamptexcoords\n map a.tga\n}"));
    assert(!R_Q3TestStageClamps("\n map a.tga\n//clampTexCoords\n}\n"));
    assert(!R_Q3TestStageClamps("\n map a.tga\n/* clampTexCoords */\n}\n"));
    assert(!R_Q3TestStageClamps("\n map a.tga\n}\n{ clampTexCoords }\n"));
    assert(!R_Q3TestStageClamps("\n { clampTexCoords }\n map a.tga\n}\n"));
    assert(!R_Q3TestStageClamps("\n map \"clampTexCoords\"\n}\n"));
    assert(!R_Q3TestStageClamps("clampTexCoordsX\n}"));
    assert(!R_Q3TestStageClamps(""));
    // the value after map
    assert(R_Q3TestImageFollows(" textures/sfx/swirl_r1.tga\n"));
    assert(!R_Q3TestImageFollows(" $lightmap\n") && !R_Q3TestImageFollows(" *white") && !R_Q3TestImageFollows("\n x"));
    // the gfx.shader repair touches nothing but its exact files
    ri.gameId = COM_GAME_Q3TEST;
    std::string other(20263, 'x');
    const std::string before = other;
    assert(!R_Q3TestRepairShaderText(other.data(), long(other.size()), "scripts/gfx.shader") && other == before);
    std::string wrongName = other;
    assert(!R_Q3TestRepairShaderText(wrongName.data(), long(wrongName.size()), "scripts/sfx.shader") && wrongName == before);
    // FNTECH3_Q3TEST_GFX_SHADERS names the retail files (1.06, 1.07, 1.08):
    // each is repaired, and then every shader in it closes.
    for (int i = 1; i < argc; ++i) {
        FILE *file = std::fopen(argv[i], "rb");
        assert(file);
        std::string source;
        char chunk[4096];
        for (std::size_t n; (n = std::fread(chunk, 1, sizeof(chunk), file)) > 0;) source.append(chunk, n);
        std::fclose(file);
        for (auto profile : {COM_GAME_QUAKE3, COM_GAME_Q3TEST}) {
            ri.gameId = profile;
            std::string copy = source;
            const bool repaired = R_Q3TestRepairShaderText(copy.data(), long(copy.size()), "scripts/gfx.shader");
            assert(repaired == (profile == COM_GAME_Q3TEST));
            if (!repaired) { assert(copy == source); continue; }
            const char *p = copy.c_str();
            int shaders = 0;
            bool crosshairc = false;
            for (;;) {
                const char *token = COM_ParseExt(&p, qtrue);
                if (!*token) break;
                crosshairc |= !std::strcmp(token, "gfx/2d/crosshairc");
                token = COM_ParseExt(&p, qtrue);
                assert(!std::strcmp(token, "{"));
                assert(SkipBracedSection(&p, 1));
                ++shaders;
            }
            assert(crosshairc && shaders > 60);
            // a second pass finds nothing left to repair
            assert(!R_Q3TestRepairShaderText(copy.data(), long(copy.size()), "scripts/gfx.shader"));
        }
    }
    std::puts("q3test shader dialect ok");
    return 0;
}
'''

PICTURE_CHECKS = r'''
#define LIGHTMAP_2D -4
struct image_t { char imgName[MAX_QPATH]; };
struct textureBundle_t { image_t *image[8]; int numImageAnimations; };
struct shaderStage_t { qboolean active; textureBundle_t bundle[2]; };
struct shader_t { int index; qboolean defaultShader; shaderStage_t *stages[8]; int q3test2D; };
static shader_t shaders[64];
static shaderStage_t stagePool[64];
static int numShaders = 1, registrations = 0, lastLightmap = 0;
static shader_t *R_GetShaderByHandle(qhandle_t h) { assert(h >= 0 && h < numShaders); return &shaders[h]; }
static shader_t *Make(image_t *image, int animations) {
    shader_t &sh = shaders[numShaders];
    sh.index = numShaders;
    if (image) {
        stagePool[numShaders].active = qtrue;
        stagePool[numShaders].bundle[0].image[0] = image;
        stagePool[numShaders].bundle[0].numImageAnimations = animations;
        sh.stages[0] = &stagePool[numShaders];
    }
    ++numShaders;
    return &sh;
}
static qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean) {
    assert(!std::strncmp(name, "*q3test2d/", 10));
    ++registrations;
    lastLightmap = lightmapIndex;
    return Make(image, 1)->index;
}
#include "code/renderercommon/tr_q3test_2d.h"
int main() {
    image_t art{"gfx/2d/menuinfo2"}, cursor{"menu/arena/3_cursor2"};
    shader_t *menu = Make(&art, 1), *menuAgain = Make(&art, 1), *animated = Make(&cursor, 4), *empty = Make(nullptr, 0);
    shader_t *fallback = Make(&art, 1);
    fallback->defaultShader = qtrue;
    ri.gameId = COM_GAME_QUAKE3;
    assert(R_Q3Test2DShader(menu) == menu && registrations == 0 && !menu->q3test2D);
    ri.gameId = COM_GAME_Q3TEST;
    shader_t *picture = R_Q3Test2DShader(menu);
    assert(picture != menu && registrations == 1 && lastLightmap == LIGHTMAP_2D);
    assert(picture->stages[0]->bundle[0].image[0] == &art);
    assert(R_Q3Test2DShader(menu) == picture && registrations == 1);
    // another shader of the same image gets the same picture shader's image
    assert(R_Q3Test2DShader(menuAgain)->stages[0]->bundle[0].image[0] == &art);
    assert(R_Q3Test2DShader(animated) == animated && animated->q3test2D == animated->index);
    assert(R_Q3Test2DShader(empty) == empty);
    assert(R_Q3Test2DShader(fallback) == fallback);
    assert(R_Q3Test2DShader(nullptr) == nullptr);
    std::puts("q3test pictures ok");
    return 0;
}
'''


def compile_and_run(test, source, name, args=()):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++ compiler unavailable')
    flags = compiler_target_args(compiler)
    if os.name == 'nt' and not flags:
        flags = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='q3test-shader-', dir=ROOT / '.tmp') as temp:
        cpp = Path(temp) / (name + '.cpp')
        exe = Path(temp) / (name + ('.exe' if os.name == 'nt' else ''))
        cpp.write_text(source)
        build = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(build.returncode, 0, build.stdout + build.stderr)
        run = subprocess.run([str(exe), *args], cwd=ROOT, capture_output=True, text=True, timeout=60)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class Q3TestShaderDialectTests(unittest.TestCase):
    def test_dialect_and_gfx_repair(self):
        shaders = [path for path in os.environ.get('FNTECH3_Q3TEST_GFX_SHADERS', '').split(os.pathsep) if path]
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
                source = (PRELUDE + '#include <cmath>\n#include "code/qcommon/surfaceflags.h"\n'
                          '#include "code/renderercommon/tr_q3_ihv_fog.h"\n' + tokenizer() +
                          record(local, 'fogParms_t') + DIALECT_CHECKS)
                compile_and_run(self, source, 'dialect-' + renderer, shaders)

    def test_pictures_use_their_first_image(self):
        compile_and_run(self, PRELUDE + PICTURE_CHECKS, 'pictures')

    def test_every_renderer_reads_the_dialect(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                shader = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                self.assertIn('#include "../renderercommon/tr_q3test_shader_dialect.h"', shader)
                self.assertIn('R_Q3TestRepairShaderText( buffers[i], summand, filename );', shader)
                self.assertIn('if ( R_Q3TestFogParms( text ) ) continue;', shader)
                self.assertIn('if ( R_Q3TestFogDirective( token, text ) ) continue;', shader)
                self.assertIn('R_Q3TestStagelessSky( shader.surfaceFlags, shader.isSky )', shader)
                self.assertIn('q3testClamp = R_Q3TestShaderDialect() && R_Q3TestStageClamps( *text )', shader)
                commands = (ROOT / 'code' / renderer / 'tr_cmds.c').read_text()
                self.assertIn('cmd->shader = R_Q3Test2DShader( R_GetShaderByHandle( hShader ) );', commands)
                bsp = (ROOT / 'code' / renderer / 'tr_bsp.c').read_text()
                self.assertIn('shader->fogParms.q3testType > 0 ? 5 : shader->fogParms.q3testType < 0 ? 4 : -1', bsp)
                self.assertIn('if ( l->filelen == 0 && ri.gameId == COM_GAME_Q3TEST ) {', bsp)


if __name__ == '__main__':
    unittest.main()
