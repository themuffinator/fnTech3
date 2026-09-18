"""Owned MDS models, profile-specific materials/LOD, skeletal tags and dispatch."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mdc_renderer_tests import PRELUDE, CALLBACKS
from mds_format_tests import FIXTURE
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def renderer_fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    source = '#define _CRT_SECURE_NO_WARNINGS 1\n' + PRELUDE + '#include "code/qcommon/game_profile.h"\n#include <float.h>\n'
    for pattern in (r'typedef enum\s*\{[^{}]*SF_BAD[^{}]*\}\s*surfaceType_t;',
                    r'typedef enum\s*\{[^{}]*MOD_BAD[^{}]*\}\s*modtype_t;'):
        source += re.search(pattern, local)[0] + '\n'
    source += '#include "code/renderercommon/tr_mdc_model_types.h"\n#include "code/renderercommon/tr_mds_model_types.h"\n#include "code/renderercommon/tr_wolfet_model_types.h"\nstruct bmodel_t;\n'
    source += re.search(r'typedef struct model_s\s*\{[^{}]*\}\s*model_t;', local)[0] + '\n'
    source += 'struct trRefEntity_t { refEntity_t e; };\n'
    callbacks = CALLBACKS.replace('static mdcModel_t *Allocate', 'static mdsModel_t *Allocate').replace('(mdcModel_t *)allocations', '(mdsModel_t *)allocations')
    callbacks = callbacks.replace('tr_mdc_model.h', 'tr_mds_model.h').replace('tr_mdc_surface.h', 'tr_mds_surface.h')
    callbacks = callbacks.replace('decltype(&::Error) Error;', 'decltype(&::Error) Error;\n    comGameId_t gameId;')
    callbacks = callbacks.replace('FreeFile, Print, Error};', 'FreeFile, Print, Error, COM_GAME_QUAKE3};')
    start = callbacks.index('void AnglesToAxis(')
    end = callbacks.index('#include "code/renderercommon/tr_mds_model.h"', start)
    math = (ROOT / 'code/qcommon/q_math.c').read_text()
    callbacks = callbacks[:start] + 'const vec3_t vec3_origin = {0,0,0};\n' + ''.join(
        function(math, signature) for signature in ('void AngleVectors(', 'void AnglesToAxis(', 'vec_t VectorNormalize(')
    ) + callbacks[end:]
    callbacks = callbacks.replace('static struct { trRefEntity_t *currentEntity; } backEnd{&entity};', '''
#define USE_PMLIGHT
enum { VPF_DLIGHT_SHADOW=0x20 };
struct viewParms_t { struct { vec3_t axis[3], origin; } or; float projectionMatrix[16]; int passFlags; };
static struct { trRefEntity_t *currentEntity; viewParms_t viewParms; } backEnd{&entity, {}};
static cvar_t lodScale, lodBias;
static cvar_t *r_lodscale = &lodScale, *r_lodbias = &lodBias;
''')
    source += callbacks
    text = (ROOT / 'code' / renderer / 'tr_surface.c').read_text()
    start = text.index('void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])')
    table = text[start:text.index('\n};', start) + 3]
    # Whole entries only: one surface function's name can begin another's.
    table = re.sub(r'\)\)(RB_\w+)', lambda m: m[0] if m[1] == 'RB_SurfaceMDS' else '))Noop', table)
    source += table + '\n' + FIXTURE[:FIXTURE.index('int main(void)')]
    return source + r'''
static model_t testModel;
struct skin_t { int numSurfaces; struct { const char *name; shader_t *shader; } surfaces[3]; } skin;
static shader_t shaderTable[12];
static struct { int numSkins; shader_t *defaultShader; } tr{2, shaderTable};
static model_t *R_GetModelByHandle(int handle) { assert(handle == 11); return &testModel; }
static shader_t *R_GetShaderByHandle(int handle) { assert(handle >= 0 && handle < 12); return &shaderTable[handle]; }
static const skin_t *R_GetSkinByHandle(int handle) { assert(handle == 1); return &skin; }
int Q_stricmp(const char *a, const char *b) {
    while (*a && *b && std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b)) { ++a; ++b; }
    return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}
#include "code/renderercommon/tr_mds_material.h"
#include "code/renderercommon/tr_mesh_tags.h"
static void Init() {
    Make(); files.clear(); registered.clear(); frees = liveFiles = 0; entity = {}; testModel = {};
    files["model.mds"] = std::vector<byte>(original, original + BYTES);
    testModel.index = 11; assert(R_RegisterMDS("model.mds", &testModel) == 11);
    for (int i = 0; i < 12; ++i) shaderTable[i].index = i;
    entity.e.hModel = 11; entity.e.frame = 1; entity.e.backlerp = 0.375f;
    entity.e.rtcw.torsoFrame = 0; entity.e.rtcw.oldTorsoFrame = 1; entity.e.rtcw.torsoBacklerp = 0.75f;
    const vec3_t angles = {20, 45, -30}; AnglesToAxis(angles, entity.e.rtcw.torsoAxis);
    backEnd.viewParms = {}; backEnd.viewParms.or.axis[0][0] = 1;
    backEnd.viewParms.projectionMatrix[5] = 1; backEnd.viewParms.projectionMatrix[11] = -1;
    lodScale.value = 5; lodBias.value = 0;
}
'''


CHECKS = r'''
static void Near(float actual, float expected) { assert(fabsf(actual - expected) < 0.00003f); }
int main() {
    Init();
    auto *mds = testModel.mds;
    assert(testModel.type == MOD_MDS && testModel.numLods == 1 && testModel.dataSize > BYTES);
    assert(frees == 1 && !liveFiles && registered.size() == 1 && registered[0] == "textures/test");
    assert(mds->view.data != original && !std::memcmp(mds->view.data, original, BYTES));
    assert(mds->surfaces[0].view.data == mds->view.data + SURFACE && mds->surfaces[0].model == mds);
    assert(mds->frames[1].radius == 100 && mds->frames[0].bounds[0][0] == -64);
    assert(mds->surfaces[0].surfaceType == SF_MDS && mds->surfaces[0].shader == 7);
    assert(mds->surfaces[0].vertices[1].weights == mds->surfaces[0].view.data + VERTS + 52 + 32);
    assert(R_MeshFrameCount(&testModel) == 2 && R_MeshFrame(&testModel, 2, 1) == &mds->frames[1]);
    assert(R_MeshSurfaceCount(&testModel, 2) == 1);
    meshSurfaceView_t cursor; R_MeshFirstSurface(&testModel, 0, &cursor); R_MeshSurfaceDescribe(&cursor);
    assert(cursor.data == &mds->surfaces[0].surfaceType && !cursor.md3 && !cursor.mdc);
    for (int skinNum : {INT_MIN, -1, 0, 1, INT_MAX}) assert(R_MeshShaderIndex(&cursor, skinNum) == 7);
    R_MeshNextSurface(&cursor);
    for (auto game : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        ri.gameId = game;
        fnMdsPose_t pose; assert(R_MdsEntityPose(&mds->view, &entity.e, game, &pose));
        tess = {}; tess.guard1 = 0x12345678; tess.guard2 = 0xaabbccdd;
        tess.numVertexes = tess.numIndexes = 3;
        rb_surfaceTable[SF_MDS](&mds->surfaces[0]);
        assert(tess.guard1 == 0x12345678 && tess.guard2 == 0xaabbccdd);
        assert(tess.numVertexes == 6 && tess.numIndexes == 6 && tess.surfType == SF_MDS);
        assert(tess.indexes[3] == 3 && tess.indexes[4] == 5 && tess.indexes[5] == 4);
        for (int vertex = 0; vertex < 3; ++vertex) {
            vec3_t xyz, normal;
            assert(FnMds_SkinVertex(&mds->surfaces[0].vertices[vertex], &pose, xyz, normal));
            assert(!std::memcmp(tess.xyz[3+vertex], xyz, sizeof(xyz)));
            assert(!std::memcmp(tess.normal[3+vertex], normal, sizeof(normal)));
            assert(tess.texCoords[0][3+vertex][0] == vertex * 0.25f);
            assert(tess.texCoords[0][3+vertex][1] == vertex * -0.5f);
        }
        orientation_t tag;
        assert(R_LerpTagEntity(&tag, &entity.e, "tag_head", -1) == 0);
        assert(!std::memcmp(tag.origin, pose.bones[2].origin, sizeof(tag.origin)));
        assert(!std::memcmp(tag.axis, pose.bones[2].axis, sizeof(tag.axis)));
        for (int first : {1, 2, INT_MAX}) {
            std::memset(&tag, 0xff, sizeof(tag)); assert(R_LerpTagEntity(&tag, &entity.e, "tag_head", first) == -1);
            orientation_t zero{}; assert(!std::memcmp(&tag, &zero, sizeof(tag)));
        }
        char aliasedName[sizeof(tag)] = "tag_head";
        assert(R_LerpTagEntity((orientation_t *)aliasedName, &entity.e, aliasedName, 0) == 0);
        refEntity_t aliasedEntity = entity.e;
        assert(R_LerpTagEntity((orientation_t *)&aliasedEntity, &aliasedEntity, "tag_head", 0) == 0);
        assert(!std::memcmp(&aliasedEntity, aliasedName, sizeof(tag)));
        refEntity_t invalid = entity.e; invalid.frame = 2;
        assert(R_LerpTagEntity(&tag, &invalid, "tag_head", 0) == -1);
        invalid = entity.e; invalid.oldframe = invalid.frame; invalid.backlerp = NAN;
        assert(R_LerpTagEntity(&tag, &invalid, "tag_head", 0) == 0); // Unused blend is ignored.
        invalid = entity.e; invalid.backlerp = NAN;
        assert(R_LerpTagEntity(&tag, &invalid, "tag_head", 0) == -1);
        for (int frame : {-1, 0, 1, 2, INT_MAX}) {
            auto e = entity.e; e.frame = frame; e.rtcw.torsoFrame = frame;
            e.renderfx = RF_WRAP_FRAMES; R_MdsSanitizeFrames(&mds->view, &e, game);
            assert((unsigned)e.frame < 2 && (unsigned)e.oldframe < 2);
            if (game == COM_GAME_RTCW_SP || game == COM_GAME_RTCW_MP) assert((unsigned)e.rtcw.torsoFrame < 2);
        }
    }
    // Frame/pose and queue failures must not publish partial tessellation counts.
    ri.gameId = COM_GAME_RTCW_SP;
    entity.e.frame = 1; entity.e.oldframe = 0; entity.e.backlerp = NAN;
    const int oldVerts = tess.numVertexes, oldIndices = tess.numIndexes;
    try { rb_surfaceTable[SF_MDS](&mds->surfaces[0]); assert(false); } catch (const std::runtime_error &) {}
    assert(tess.numVertexes == oldVerts && tess.numIndexes == oldIndices);
    entity.e.backlerp = 0.5f; failQueue = true;
    try { rb_surfaceTable[SF_MDS](&mds->surfaces[0]); assert(false); } catch (const std::runtime_error &) {}
    failQueue = false; assert(tess.numVertexes == oldVerts && tess.numIndexes == oldIndices);
    // Projection uses the backend's active view, and SP/MP resolve aliased flags.
    entity.e.origin[0] = 1000; mds->view.lodBias = 0; mds->view.lodScale = 1;
    lodScale.value = 1; entity.e.rtcw.reFlags = 8;
    Near(RB_MdsLodFraction(mds, &entity.e), 1);
    ri.gameId = COM_GAME_RTCW_MP; Near(RB_MdsLodFraction(mds, &entity.e), 0.05f);
    backEnd.viewParms.passFlags=VPF_DLIGHT_SHADOW;
    Near(RB_MdsLodFraction(mds, &entity.e), 1);
    backEnd.viewParms.passFlags=0;
    entity.e.rtcw.reFlags = 0;
    tess = {}; rb_surfaceTable[SF_MDS](&mds->surfaces[0]);
    assert(tess.numVertexes == 1 && !tess.numIndexes); // Collapsed triangle is degenerate.
    mds->view.lodBias = -1; tess = {}; tess.numVertexes = tess.numIndexes = 11;
    rb_surfaceTable[SF_MDS](&mds->surfaces[0]); assert(tess.numVertexes == 3 && tess.numIndexes == 3);
    // Model loading preserves authored surface names and discards disk shader indices.
    auto &payload = files["model.mds"];
    std::strcpy((char *)payload.data() + SURFACE + 4, "Upper_1"); payload[SURFACE + 68] = 0;
    auto *emptyShader = R_LoadMDS(&testModel, payload.data(), (int)payload.size(), "empty");
    assert(emptyShader && !std::strcmp(emptyShader->surfaces[0].name, "Upper_1") && !emptyShader->surfaces[0].shader);
    model_t stale{}; stale.index = 11; stale.md3[0] = (md3Header_t *)mds; stale.mdc[2] = (mdcModel_t *)mds;
    stale.modelData = mds; stale.bmodel = (bmodel_t *)mds;
    assert(R_RegisterMDS("model.mds", &stale) == 11);
    assert(!stale.md3[0] && !stale.mdc[2] && !stale.modelData && !stale.bmodel);
    model_t rejected{}; const auto before = allocations.size();
    assert(!R_LoadMDS(&rejected, original, -1, "negative"));
    assert(!R_LoadMDS(&rejected, original, BYTES - 1, "short"));
    rejected.dataSize = INT_MAX; assert(!R_LoadMDS(&rejected, original, BYTES, "overflow"));
    rejected.dataSize = -1; assert(!R_LoadMDS(&rejected, original, BYTES, "negative total"));
    assert(allocations.size() == before);
    rejected = {}; failAllocation = true;
    try { R_LoadMDS(&rejected, original, BYTES, "allocation"); assert(false); } catch (const std::bad_alloc &) {}
    failAllocation = false; assert(!rejected.dataSize);
    failShader = true;
    try { R_LoadMDS(&rejected, original, BYTES, "shader"); assert(false); } catch (const std::runtime_error &) {}
    failShader = false; assert(!rejected.dataSize && !rejected.mds);
    assert(!R_RegisterMDS("missing.mds", &rejected) && !liveFiles);
    // Duplicate tags retain inclusive indices. Skeletal names are compared in
    // full, unlike the legacy MD3 path's truncation to MAX_QPATH - 1.
    {
        std::vector<byte> duplicated(BYTES + 72);
        std::memcpy(duplicated.data(), original, SURFACE);
        std::memcpy(duplicated.data() + SURFACE, original + TAGS, 72);
        std::memcpy(duplicated.data() + SURFACE + 72, original + SURFACE, SURFBYTES);
        Put32(duplicated.data() + 104, SURFACE + 72); Put32(duplicated.data() + 108, 2);
        Put32(duplicated.data() + 116, BYTES + 72); Put32(duplicated.data() + SURFACE + 72 + 140, (uint32_t)-(SURFACE + 72));
        model_t holder{}; auto *duplicates = R_LoadMDS(&holder, duplicated.data(), (int)duplicated.size(), "duplicate tags");
        assert(duplicates);
        testModel.mds = duplicates; orientation_t tag;
        assert(R_LerpTagEntity(&tag, &entity.e, "tag_head", 0) == 0);
        assert(R_LerpTagEntity(&tag, &entity.e, "tag_head", 1) == 1);
        assert(R_LerpTagEntity(&tag, &entity.e, "tag_head", 2) == -1);
        std::memset(duplicated.data() + TAGS, 't', 63); duplicated[TAGS + 63] = 0;
        duplicates = R_LoadMDS(&holder, duplicated.data(), (int)duplicated.size(), "long tag");
        assert(duplicates); testModel.mds = duplicates;
        std::string longName(63, 't');
        assert(R_LerpTagEntity(&tag, &entity.e, longName.c_str(), 0) == 0);
        longName += 'x'; assert(R_LerpTagEntity(&tag, &entity.e, longName.c_str(), 0) == -1);
        testModel.mds = mds;
    }
    // Material precedence and the two original blink contracts.
    auto &surface = mds->surfaces[0]; entity.e.rtcw.blink = qtrue;
    entity.e.customSkin = 1; skin.numSurfaces = 2;
    skin.surfaces[0] = {"surface", &shaderTable[8]}; skin.surfaces[1] = {"surface_b", &shaderTable[9]};
    ri.gameId = COM_GAME_RTCW_MP; assert(R_MdsSurfaceShader(&entity.e, &surface)->index == 9);
    skin.surfaces[1].shader = tr.defaultShader; assert(R_MdsSurfaceShader(&entity.e, &surface)->index == 8);
    ri.gameId = COM_GAME_RTCW_SP; assert(R_MdsSurfaceShader(&entity.e, &surface)->index == 8);
    std::strcpy(surface.name, "H_bLiNk"); entity.e.rtcw.blink = qfalse;
    assert(R_MeshSurfaceHidden(&entity.e, surface.name)); entity.e.rtcw.blink = qtrue;
    assert(!R_MeshSurfaceHidden(&entity.e, surface.name));
    for (auto game : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_MP}) {
        ri.gameId = game; entity.e.rtcw.blink = qfalse; assert(!R_MeshSurfaceHidden(&entity.e, surface.name));
    }
    entity.e.customShader = 10; assert(R_MdsSurfaceShader(&entity.e, &surface)->index == 10);
    entity.e.customShader = 0; assert(R_MdsSurfaceShader(&entity.e, &surface) == tr.defaultShader);
    entity.e.customSkin = 0; assert(R_MdsSurfaceShader(&entity.e, &surface)->index == 7);
    assert(flushes && overflows && errors && !liveFiles);
}
'''


class MdsRendererTests(unittest.TestCase):
    def test_ownership_dispatch_tags_materials_and_continuous_lod(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(prefix='mds-render-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp)
                source, executable = folder / 'probe.cpp', folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(renderer_fixture(renderer) + CHECKS, encoding='utf-8')
                build = subprocess.run([compiler, *target, '-std=c++17', '-fno-operator-names', '-I', str(ROOT), str(source),
                    '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=90)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
