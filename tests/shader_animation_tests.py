"""Execute animMap parsing with each renderer's limits and the real tokenizer."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function
from model_shader_tests import common
from rtcw_shader_text_tests import tokenizer

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

HOST = r'''
// Preserve C's enum bitwise assignment when compiling the parser fixture as C++.
static imgFlags_t &operator|=(imgFlags_t &a, imgFlags_t b) {
    return a = imgFlags_t(int(a) | int(b));
}
static char com_token[MAX_TOKEN_CHARS], com_parsename[MAX_TOKEN_CHARS];
static int com_lines, com_tokenline;
int QDECL Com_sprintf(char *out, int size, const char *format, ...) {
    va_list args; va_start(args, format);
    int n = std::vsnprintf(out, size, format, args); va_end(args); return n;
}
float Q_atof(const char *text) { return float(std::atof(text)); }
static std::vector<std::string> warnings, loaded;
static void QDECL Warn(int level, const char *format, ...) {
    assert(level == PRINT_WARNING);
    char text[1024]; va_list args; va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args); va_end(args);
    warnings.emplace_back(text);
}
static int s_extendedShader;
static qboolean R_AliceShaderDialect() { return ri.gameId == COM_GAME_ALICE ? qtrue : qfalse; }
static cvar_t compression{};
static cvar_t *r_ext_compressed_textures = &compression;
static struct {
    const char *name;
    int noMipMaps, noPicMip, noLightScale;
} shader{"synthetic/fire", 0, 0, 0};
static image_t images[MAX_IMAGE_ANIMATIONS];
static int missing = -1;
static image_t *R_FindShaderImage(const char *name, imgFlags_t flags) {
    int i = int(loaded.size());
    assert(i < MAX_IMAGE_ANIMATIONS);
    loaded.emplace_back(name);
    images[i].flags = flags;
    return i == missing ? nullptr : &images[i];
}
'''

CHECKS = r'''
static void Reset() {
    loaded.clear(); warnings.clear(); missing = -1;
    shader.noMipMaps = shader.noPicMip = shader.noLightScale = 0;
    COM_BeginParseSession("synthetic animation");
}
static std::string Animation(int count, const char *word = "AnImMaP") {
    std::string text = std::string(word) + " 10";
    for (int i = 0; i < count; ++i) text += " frame" + std::to_string(i);
    return text + " // keep the next directive\n rgbGen identity\n }\n";
}
int main() {
    ri.Printf = Warn;
    for (auto profile : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP, COM_GAME_ALICE,
                         COM_GAME_JKA_SP, COM_GAME_JKA_MP, COM_GAME_JK2_SP, COM_GAME_JK2_MP}) {
        ri.gameId = profile;
        const bool rtcw = profile == COM_GAME_RTCW_SP || profile == COM_GAME_RTCW_MP;
        const bool academy = profile == COM_GAME_JKA_SP || profile == COM_GAME_JKA_MP;
        const bool jedi = academy || profile == COM_GAME_JK2_SP || profile == COM_GAME_JK2_MP;
        // The Jedi engines' oneShotAnimMap keeps animMap's bound (tr_jka_shader.h).
        const std::vector<const char *> words = jedi ? std::vector<const char *>{"AnImMaP", "oNeShOtAnImMaP"} :
            std::vector<const char *>{"AnImMaP"};
        for (int extended : {0, 1}) {
            s_extendedShader = extended;
            // Ritual's dialect keeps retail's 64 frames (alice.exe 0x0048c5bc,
            // fakk2.exe 1.02 0x00476e00; F.A.K.K.2's caustics use 47) in a
            // renderer that carries it, as do extended shaders.
            // Jedi Academy keeps 32 (jamp.exe 0x4b42e4, jasp.exe 0x47a497),
            // Jedi Outcast Quake III's 8 (jk2mp.exe 0x48d9ed, jk2sp.exe 0x47d81d).
            const int limit = extended ? 64 : rtcw ? 16 : profile == COM_GAME_ALICE ? ALICE_ANIMATION_LIMIT :
                academy ? 32 : 8;
            for (const char *word : words)
            for (int count : {0, 1, 8, 9, 15, 16, 17, 24, 25, 31, 32, 33, 47, 64, 65}) {
                Reset(); shaderStage_t stage{};
                auto input = Animation(count, word); const char *text = input.c_str();
                assert(ParseAnimation(&stage, &text));
                const int accepted = count < limit ? count : limit;
                assert(stage.bundle[0].numImageAnimations == accepted);
                assert(stage.bundle[0].imageAnimationSpeed == 10);
                assert(int(loaded.size()) == accepted);
                // This checks retained frames, including the second half of RTCW fire.
                for (int i = 0; i < MAX_IMAGE_ANIMATIONS; ++i) {
                    assert(stage.bundle[0].image[i] == (i < accepted ? &images[i] : nullptr));
                    if (i < accepted) {
                        assert(loaded[i] == "frame" + std::to_string(i));
                        assert(images[i].flags == (IMGFLAG_MIPMAP | IMGFLAG_PICMIP));
                    }
                }
                assert(warnings.size() == (count > limit ? 1u : 0u));
                if (count > limit) {
                    const auto expected = "(found " + std::to_string(count) + ", max is " + std::to_string(limit) + ")";
                    assert(warnings[0].find(expected) != std::string::npos);
                }
                assert(!strcmp(COM_ParseExt(&text, qtrue), "rgbGen"));
                assert(!strcmp(COM_ParseExt(&text, qtrue), "identity"));
                assert(!strcmp(COM_ParseExt(&text, qtrue), "}"));
            }
            // A missing accepted frame must still fail; excess names are not loaded.
            for (int absent : {limit - 1, limit}) {
                Reset(); missing = absent; shaderStage_t stage{};
                auto input = Animation(limit + 1); const char *text = input.c_str();
                assert(bool(ParseAnimation(&stage, &text)) == (absent == limit));
                assert(warnings.size() == 1);
                assert((warnings[0].find("could not find") != std::string::npos) == (absent < limit));
            }
            for (const char *input : {"animMap", "animMap\n rgbGen identity"}) {
                Reset(); shaderStage_t stage{}; const char *text = input;
                assert(!ParseAnimation(&stage, &text));
                assert(loaded.empty() && warnings.size() == 1);
            }
            Reset(); shaderStage_t stage{};
            shader.noMipMaps = shader.noPicMip = shader.noLightScale = 1;
            auto input = Animation(limit); const char *text = input.c_str();
            assert(ParseAnimation(&stage, &text));
            for (int i = 0; i < limit; ++i) assert(images[i].flags == IMGFLAG_NOLIGHTSCALE);
        }
    }
}
'''


class ShaderAnimationTests(unittest.TestCase):
    def test_profile_limits_and_parser_bounds(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='shader-animation-', dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
                    defines = '\n'.join(re.findall(r'^#define MAX_IMAGE_ANIMATIONS[^\n]*', local, re.M))
                    # Use the production bundle's image storage/count/speed fields.
                    fields = re.search(r'image_t\s+\*image\[MAX_IMAGE_ANIMATIONS\];.*?double\s+imageAnimationSpeed;', local, re.S)[0]
                    # Academy's oneShotAnimMap bit (tr_jka_shader.h) is written by the same branch.
                    records = 'struct textureBundle_t {' + fields + ' unsigned int jkaOneShotAnimMap : 1;};\nstruct shaderStage_t {textureBundle_t bundle[1];};\n'
                    stage = function(source, 'static qboolean ParseStage(')
                    start = stage.index('else if ( !Q_stricmp( token, "animMap" )')
                    end = stage.index('else if ( !Q_stricmp( token, "videoMap" ) )', start)
                    branch = stage[start:end].replace('else if', 'if', 1)
                    parser = '''static qboolean ParseAnimation(shaderStage_t *stage, const char **text) {
                        const char *token = COM_ParseExt(text, qtrue);
                        // The Ritual dialect's nextBundle selects bundle 1; no stage here takes it.
                        const struct { int bundle; } aliceStage{};
                        (void)aliceStage;
                    ''' + branch + 'else return qfalse; return qtrue;}\n'
                    defines += '\n#define ALICE_ANIMATION_LIMIT %d\n' % (64 if 'R_AliceShaderDialect()' in branch else 8)
                    fixture = common(renderer) + '\n#include <cstdlib>\n' + defines + '\n' + HOST + tokenizer()
                    fixture += '\n#include "code/renderercommon/tr_rtcw_shader_material.h"\n#include "code/renderercommon/tr_jka_shader.h"\n' + records + parser + CHECKS
                    cpp = Path(temp) / f'{renderer}.cpp'
                    exe = Path(temp) / (renderer + ('.exe' if os.name == 'nt' else ''))
                    cpp.write_text(fixture, encoding='utf-8')
                    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
