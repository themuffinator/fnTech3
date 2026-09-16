"""Execute brush-material lookup and shader-variant cache paths in all renderers."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/qcommon/qfiles.h"
#include "code/renderercommon/tr_types.h"
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
'''


def common(renderer):
    source = (ROOT / 'code' / renderer / 'tr_common.h').read_text()
    flags = re.search(r'typedef enum\s*\{[^{}]*IMGFLAG_NONE[^{}]*\}\s*imgFlags_t;', source)[0]
    limits = '\n'.join(re.findall(r'^#define LIGHTMAP_[^\n]+', source, re.M))
    return PRELUDE + flags + '\n' + limits + r'''
struct image_t { const char *imgName; image_t *next; imgFlags_t flags; };
static void QDECL Print(int, const char *, ...) {}
static struct { decltype(&Print) Printf; comGameId_t gameId; } ri{Print,COM_GAME_QUAKE3};
void Q_strncpyz(char *out, const char *in, int size) { assert(size > 0); std::strncpy(out, in, size-1); out[size-1] = 0; }
int Q_stricmp(const char *a, const char *b) {
    while (*a && *b && std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b)) { ++a; ++b; }
    return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}
void COM_StripExtension(const char *in, char *out, int size) {
    std::string name(in); const auto slash = name.find_last_of("/\\"), dot = name.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) name.resize(dot);
    Q_strncpyz(out, name.c_str(), size);
}
'''


def brush_fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    records = re.search(r'typedef enum\s*\{[^{}]*SF_BAD[^{}]*\}\s*surfaceType_t;', local)[0]
    records += re.search(r'typedef enum\s*\{[^{}]*MOD_BAD[^{}]*\}\s*modtype_t;', local)[0]
    end = local.index('} bmodel_t;')
    records += local[local.rindex('typedef struct', 0, end):end + len('} bmodel_t;')]
    records += '\n#include "code/renderercommon/tr_mdc_model_types.h"\n#include "code/renderercommon/tr_mds_model_types.h"\n'
    records += re.search(r'typedef struct model_s\s*\{[^{}]*\}\s*model_t;', local)[0]
    source = common(renderer) + r'''
struct shader_t { char name[MAX_QPATH]; int lightmapIndex, index; };
struct msurface_t { shader_t *shader; };
''' + records + r'''
static model_t models[3];
static struct { int numModels; model_t *models[3]; } tr{3, {models, models+1, models+2}};
static image_t *hashTable[1];
static int generateHashValue(const char *) { return 0; } // Deliberately collide every image.
static int shaderCalls;
static std::string shaderName;
static qboolean shaderMip;
static shader_t resultShader{{}, LIGHTMAP_NONE, 731};
static bool nullShader, failShader;
static shader_t *R_FindModelShader(const char *name, qboolean mip) {
    ++shaderCalls; shaderName = name; shaderMip = mip;
    if (failShader) throw 7;
    return nullShader ? nullptr : &resultShader;
}
'''
    source += function((ROOT / 'code' / renderer / 'tr_model.c').read_text(), 'model_t\t*R_GetModelByHandle(')
    return source + '#include "code/renderercommon/tr_model_shader.h"\n'


BRUSH_CHECKS = r'''
int main() {
    shader_t materials[3]{{"textures/brick", 0, 101}, {"textures/metal", LIGHTMAP_NONE, 102}, {"textures/wood", LIGHTMAP_BY_VERTEX, 103}};
    msurface_t surfaces[3]{{materials}, {materials+1}, {materials+2}};
    bmodel_t brush{}; brush.firstSurface = surfaces; brush.numSurfaces = 3;
    models[1].type = MOD_BRUSH; models[1].bmodel = &brush;
    models[2].type = MOD_MESH; models[2].bmodel = &brush; // A stale pointer must not turn a mesh into a brush.
    for (int invalid : {INT_MIN, -1, 0, 2, 3, INT_MAX}) assert(!RE_GetShaderFromModel(invalid, 0, 0));
    assert(!shaderCalls);
    for (int surface : {INT_MIN, -1, 0, 1, 2, 3, INT_MAX}) for (int lightmap : {INT_MIN, -1, 0, 1, INT_MAX}) {
        const auto saved = materials[0]; const int before = shaderCalls;
        const int result = RE_GetShaderFromModel(1, surface, lightmap);
        if (surface == 1 || surface == 2) assert(result == materials[surface].index && shaderCalls == before);
        else assert(result == 731 && shaderCalls == before+1 && shaderName == "textures/brick" && shaderMip);
        assert(!std::memcmp(&saved, materials, sizeof(saved)));
    }
    image_t matched{"textures/brick", nullptr, IMGFLAG_NONE};
    image_t collision{"textures/not-brick", &matched, IMGFLAG_MIPMAP}; hashTable[0] = &collision;
    assert(RE_GetShaderFromModel(1,0,0) == 731 && !shaderMip);
    matched.flags = IMGFLAG_MIPMAP;
    assert(RE_GetShaderFromModel(1,0,0) == 731 && shaderMip);
    matched.imgName = "TEXTURES/BRICK"; matched.flags = IMGFLAG_NONE;
    assert(RE_GetShaderFromModel(1,0,0) == 731 && shaderMip); // Image matching is case sensitive.
    nullShader = true; assert(!RE_GetShaderFromModel(1,0,0)); nullShader = false;
    failShader = true; bool threw = false; try { RE_GetShaderFromModel(1,0,0); } catch (int) { threw = true; } assert(threw); failShader = false;
    surfaces[0].shader = nullptr; assert(!RE_GetShaderFromModel(1,0,0)); surfaces[0].shader = materials;
    brush.numSurfaces = 0; assert(!RE_GetShaderFromModel(1,0,0)); brush.numSurfaces = -1; assert(!RE_GetShaderFromModel(1,0,0));
    brush.numSurfaces = 3; brush.firstSurface = nullptr; assert(!RE_GetShaderFromModel(1,0,0));
    models[1].bmodel = nullptr; assert(!RE_GetShaderFromModel(1,0,0));
    tr.models[0] = nullptr; assert(!RE_GetShaderFromModel(INT_MAX,0,0));
}
'''


def shader_fixture(renderer):
    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
    start = source.index('\t/* RTCW asks for a diffuse-lit copy')
    end = source.index('\n\tfor ( i = 0; i < stage; i++ )', start)
    adjustment = source[start:end]
    finish = function(source, 'static shader_t *FinishShader(')
    # The change must feed derived multitexture/GPU state after lightmap simplification.
    assert finish.index('VertexLightingCollapse();') < finish.index('shader.rtcwModelDiffuse') < finish.index('CollapseMultitexture(')
    color = 'rgbGen' if renderer == 'renderer' else 'bundle[0].rgbGen'
    result = common(renderer) + '#define RGB(x) ((x).' + color + ')\n' + r'''
enum { CGEN_IDENTITY, CGEN_LIGHTING_DIFFUSE, FILE_HASH_SIZE = 1 };
struct shaderStage_t { int rgbGen; struct { int rgbGen; } bundle[1]; };
struct shader_t {
    char name[MAX_QPATH]; int lightmapSearchIndex, lightmapIndex, index, rtcwCompression;
    qboolean rtcwModelDiffuse, rtcwLightMaterial, defaultShader, needsNormal, noPicMip;
    shader_t *next; shaderStage_t first;
};
static shader_t defaultShader{}, shader;
static shaderStage_t stages[1];
static struct { shader_t *defaultShader; int numLightmaps; } tr{&defaultShader, 8};
static shader_t *hashTable[FILE_HASH_SIZE];
static std::vector<std::unique_ptr<shader_t>> allocations;
static int parses, imageReads, finishes, stageCount = 1;
static bool explicitShader, parseSuccess = true, imageAvailable = true, rawNoPicMip;
static imgFlags_t lastFlags;
static image_t rawImage{"textures/fixture", nullptr, IMGFLAG_NONE};
static cvar_t printShaders{}; static cvar_t *r_printShaders = &printShaders;
static cvar_t compression{};static cvar_t *r_ext_compressed_textures=&compression;
#include "code/renderercommon/tr_rtcw_shader_material.h"
static unsigned long generateHashValue(const char *, int) { return 0; }
static imgFlags_t &operator|=(imgFlags_t &a, imgFlags_t b) { a = static_cast<imgFlags_t>((int)a | (int)b); return a; }
static void InitShader(const char *name, int lightmap) {
    shader = {}; stages[0] = {}; Q_strncpyz(shader.name, name, sizeof(shader.name));
    shader.lightmapIndex = shader.lightmapSearchIndex = lightmap; shader.noPicMip = rawNoPicMip ? qtrue : qfalse;
}
static const char *FindShaderInShaderText(const char *) { return explicitShader ? "{}" : nullptr; }
static qboolean ParseShader(const char **) { ++parses; return parseSuccess ? qtrue : qfalse; }
static image_t *R_FindImageFile(const char *, imgFlags_t flags) { ++imageReads; lastFlags = flags; return imageAvailable ? &rawImage : nullptr; }
static void R_CreateDefaultShading(image_t *) { RGB(stages[0]) = shader.lightmapIndex == LIGHTMAP_NONE ? CGEN_LIGHTING_DIFFUSE : CGEN_IDENTITY; }
static shader_t *FinishShader() {
    ++finishes; const int stage = stageCount;
''' + adjustment + r'''
    shader.first = stages[0]; shader.index = (int)allocations.size() + 1; shader.next = hashTable[0];
    allocations.push_back(std::make_unique<shader_t>(shader));
    hashTable[0] = allocations.back().get(); return hashTable[0];
}
'''
    for signature in ('static image_t *R_FindShaderImage(', 'shader_t *R_FindShaderByName(', 'static shader_t *R_FindShaderInternal(',
                      'shader_t *R_FindShader(', 'shader_t *R_FindModelShader(', 'shader_t *R_FindRTCWLightShader(',
                      'qhandle_t RE_RegisterShaderFromImage('):
        result += function(source, signature)
    return result


SHADER_CHECKS = r'''
int main() {
    explicitShader = true;
    auto *normal = R_FindShader("textures/brick.tga", LIGHTMAP_NONE, qtrue);
    assert(!normal->rtcwModelDiffuse && RGB(normal->first) == CGEN_IDENTITY);
    auto *diffuse = R_FindModelShader("textures/brick", qfalse);
    assert(diffuse != normal && diffuse->rtcwModelDiffuse && diffuse->needsNormal && RGB(diffuse->first) == CGEN_LIGHTING_DIFFUSE);
    assert(RGB(normal->first) == CGEN_IDENTITY && !normal->needsNormal);
    const int before = finishes;
    assert(R_FindModelShader("TEXTURES/BRICK",qtrue) == diffuse);
    assert(R_FindShader("TEXTURES/BRICK",LIGHTMAP_NONE,qtrue) == normal);
    assert(R_FindShaderByName("textures/brick.jpg") == normal && finishes == before);
    auto *light = R_FindRTCWLightShader("textures/brick");
    assert(light != normal && light != diffuse && light->rtcwLightMaterial && !light->rtcwModelDiffuse);
    assert(light->lightmapIndex == LIGHTMAP_2D && !light->needsNormal && RGB(light->first) == CGEN_IDENTITY);
    assert(R_FindRTCWLightShader("TEXTURES/BRICK.tga") == light);
    assert(R_FindShaderByName("textures/brick") == normal);
    auto *lightOnly = R_FindRTCWLightShader("light-only");
    assert(R_FindShaderByName("light-only") == &defaultShader);
    const int rawLight = RE_RegisterShaderFromImage("light-only",LIGHTMAP_2D,&rawImage,qtrue);
    assert(rawLight != lightOnly->index && !R_FindShaderByName("light-only")->rtcwLightMaterial);
    auto *modelOnly = R_FindModelShader("model-only",qtrue);
    assert(R_FindShaderByName("model-only") == &defaultShader);
    const int imageHandle = RE_RegisterShaderFromImage("model-only",LIGHTMAP_2D,&rawImage,qtrue);
    assert(imageHandle != modelOnly->index && !R_FindShaderByName("model-only")->rtcwModelDiffuse);
    assert(RE_RegisterShaderFromImage("model-only",LIGHTMAP_2D,&rawImage,qtrue) == imageHandle);
    parseSuccess = false;
    auto *badModel = R_FindModelShader("bad",qtrue); assert(badModel->defaultShader);
    auto *badNormal = R_FindShader("bad",LIGHTMAP_2D,qtrue); assert(badNormal->defaultShader && badModel != badNormal);
    assert(R_FindModelShader("bad",qfalse) == badModel && R_FindShader("bad",0,qtrue) == badNormal);
    parseSuccess = true; stageCount = 0;
    auto *fog = R_FindModelShader("no-stage",qtrue); assert(!fog->needsNormal && RGB(fog->first) == CGEN_IDENTITY);
    stageCount = 1; explicitShader = false;
    R_FindModelShader("raw-mip",qtrue); assert(lastFlags == (IMGFLAG_MIPMAP | IMGFLAG_PICMIP));
    R_FindModelShader("raw-nomip",qfalse); assert(lastFlags == IMGFLAG_CLAMPTOEDGE);
    rawNoPicMip = true; R_FindModelShader("raw-nopicmip",qtrue); assert(lastFlags == IMGFLAG_MIPMAP);
    imageAvailable = false; auto *missing = R_FindModelShader("missing",qtrue); assert(missing->defaultShader);
    const int oldReads = imageReads; assert(R_FindModelShader("missing",qfalse) == missing && imageReads == oldReads);
    assert(R_FindModelShader("",qtrue) == &defaultShader && R_FindShader("",LIGHTMAP_2D,qtrue) == &defaultShader);
}
'''


class ModelShaderTests(unittest.TestCase):
    def run_fixture(self, make, checks):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(prefix='model-shader-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp); source = folder / 'probe.cpp'
                executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(make(renderer) + checks, encoding='utf-8')
                build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(source), '-o', str(executable)],
                    cwd=ROOT, capture_output=True, text=True, timeout=90)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_brush_shader_surface_fallback_mipmap_and_bounds(self):
        self.run_fixture(brush_fixture, BRUSH_CHECKS)

    def test_diffuse_variant_cache_and_compiler_adjustment(self):
        self.run_fixture(shader_fixture, SHADER_CHECKS)


if __name__ == '__main__':
    unittest.main()
