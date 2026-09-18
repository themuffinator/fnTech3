"""Native MDC ownership/LOD/tag tests and guarded renderer dispatch fixtures."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mdc_format_tests import FIXTURE

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/renderercommon/tr_types.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
'''

CALLBACKS = r'''
enum { LIGHTMAP_NONE = -1 };
struct shader_t { bool defaultShader; int index; } shader{false, 7};
static std::vector<std::unique_ptr<byte[]>> allocations;
static std::map<std::string, std::vector<byte>> files;
static std::vector<std::string> requested, registered;
static int frees, liveFiles, errors, flushes, overflows;
static bool failAllocation, failShader, failQueue;
static vec3_t tagAngles;
static mdcModel_t *Allocate(int bytes, ha_pref) {
    assert(bytes > 0); if (failAllocation) throw std::bad_alloc();
    allocations.emplace_back(new byte[bytes]{}); return (mdcModel_t *)allocations.back().get();
}
static int ReadFile(const char *name, void **out) {
    requested.emplace_back(name); *out = nullptr;
    const auto found = files.find(name); if (found == files.end()) return -1;
    *out = std::malloc(found->second.size()); assert(*out);
    std::memcpy(*out, found->second.data(), found->second.size()); ++liveFiles;
    return (int)found->second.size();
}
static void FreeFile(void *data) { assert(data); std::free(data); ++frees; --liveFiles; }
static void QDECL Print(int, const char *, ...) {}
static void QDECL Error(int, const char *, ...) { ++errors; throw std::runtime_error("drop"); }
static struct {
    decltype(&::Allocate) Hunk_Alloc;
    decltype(&::ReadFile) FS_ReadFile;
    decltype(&::FreeFile) FS_FreeFile;
    decltype(&::Print) Printf;
    decltype(&::Error) Error;
} ri{Allocate, ReadFile, FreeFile, Print, Error};
static shader_t *R_FindShader(const char *name, int lightmap, qboolean mip) {
    assert(lightmap == LIGHTMAP_NONE && mip); registered.emplace_back(name);
    if (failShader) throw std::runtime_error("shader failure");
    return &shader;
}
void Q_strncpyz(char *dest, const char *src, int size) {
    assert(size > 0); std::strncpy(dest, src, size - 1); dest[size - 1] = 0;
}
char *Q_strlwr(char *text) { for (char *p = text; *p; ++p) *p = (char)std::tolower((unsigned char)*p); return text; }
int QDECL Com_sprintf(char *dest, int size, const char *format, ...) {
    va_list args; va_start(args, format); const int count = std::vsnprintf(dest, size, format, args); va_end(args); return count;
}
void COM_StripExtension(const char *in, char *out, int size) {
    std::string name(in); const auto slash = name.find_last_of("/\\"), dot = name.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) name.resize(dot);
    Q_strncpyz(out, name.c_str(), size);
}
void AnglesToAxis(const vec3_t angles, vec3_t axis[3]) {
    std::memcpy(tagAngles, angles, sizeof(tagAngles)); std::memset(axis, 0, sizeof(vec3_t) * 3);
    axis[0][0] = axis[1][1] = axis[2][2] = 1;
}
#include "code/renderercommon/tr_mdc_model.h"
#include "code/renderercommon/tr_mesh_view.h"
static struct {
    unsigned guard1;
    int numVertexes, numIndexes, surfType;
    float xyz[12][4], normal[12][4], texCoords[2][12][2];
    unsigned indexes[12];
    unsigned guard2;
} tess;
static trRefEntity_t entity;
static struct { trRefEntity_t *currentEntity; } backEnd{&entity};
static void VBO_Flush() { ++flushes; }
static void CheckOverflow(int verts, int indexes) {
    ++overflows;
    if (failQueue) throw std::runtime_error("queue full");
    if (verts + tess.numVertexes > 12 || indexes + tess.numIndexes > 12) tess.numVertexes = tess.numIndexes = 0;
}
#define RB_CHECKOVERFLOW(v,i) CheckOverflow(v,i)
#define USE_VBO 1
#include "code/renderercommon/tr_mdc_surface.h"
static void Noop(void *) { assert(false); }
'''

CHECKS = r'''
int main() {
    Make(); FnMdc_InitNormals(&r_mdcNormals);
    const std::vector<byte> payload(original, original + BYTES);
    const char *names[] = {"models/v1.2/model.mdc", "models/v1.2/model_1.mdc", "models/v1.2/model_2.mdc"};
    for (int mask = 1; mask < 8; ++mask) {
        files.clear(); requested.clear(); registered.clear(); frees = liveFiles = 0;
        for (int lod = 0; lod < 3; ++lod) if (mask & (1 << lod)) files[names[lod]] = payload;
        model_t model{}; model.index = 11;
        md3Header_t stale{}; model.md3[0] = &stale; model.modelData = &stale;
        assert(R_RegisterMDC(names[0], &model) == 11 && model.type == MOD_MDC);
        assert(!model.md3[0] && !model.md3[1] && !model.md3[2] && !model.modelData && !model.bmodel);
        assert(!liveFiles && frees == (int)registered.size() && model.dataSize > 0);
        assert(requested.size() == 3 && requested[0] == names[0] && requested[1] == names[1] && requested[2] == names[2]);
        assert(model.numLods == ((mask & 4) ? 3 : (mask & 2) ? 2 : 1));
        for (int lod = 0; lod < 3; ++lod) {
            const auto *mdc = model.mdc[lod];
            assert(mdc && mdc->view.numFrames == 3 && mdc->view.numSurfaces == 1);
            assert(mdc->frames[2].radius == 34 && mdc->frames[2].bounds[0][0] == -12);
            assert(!std::strcmp(mdc->surfaces[0].name, "surface"));
            assert(mdc->surfaces[0].surfaceType == SF_MDC && mdc->surfaces[0].shaders[0] == 7);
            assert(mdc->view.data != original && !std::memcmp(mdc->view.data, payload.data(), payload.size()));
            assert(mdc->surfaces[0].view.data == mdc->view.data + SURFACE);
            assert(R_MeshFrame(&model, lod, 2) == mdc->frames + 2);
        }
        for (int lod = 0; lod < model.numLods - 1; ++lod)
            if (!(mask & (1 << lod))) assert(model.mdc[lod] == model.mdc[lod + 1]);
        for (int lod = model.numLods; lod < 3; ++lod) assert(model.mdc[lod] == model.mdc[model.numLods - 1]);
        assert(R_MeshFrameCount(&model) == 3 && R_MeshSurfaceCount(&model, 0) == 1);
        meshSurfaceView_t surface; R_MeshFirstSurface(&model, 0, &surface); R_MeshSurfaceDescribe(&surface);
        assert(surface.data == &model.mdc[0]->surfaces[0].surfaceType && !std::strcmp(surface.name, "surface"));
        for (int skin : {INT_MIN, -1, 0, 1, INT_MAX}) assert(R_MeshShaderIndex(&surface, skin) == 7);
        R_MeshNextSurface(&surface); // The end cursor must not dereference a fictitious next surface.
        md3Tag_t tag{};
        assert(R_GetMDCTag(model.mdc[0], -1, "tag_test", &tag) == &tag && tag.origin[0] == -1 && tagAngles[0] == -180);
        assert(R_GetMDCTag(model.mdc[0], INT_MAX, "tag_test", &tag) == &tag && tag.origin[0] == 1 && tagAngles[0] == 180);
        assert(!R_GetMDCTag(model.mdc[0], 0, "missing", &tag));
        for (int frame = 0; frame < 3; ++frame) {
            tess = {}; tess.guard1 = 0x12345678; tess.guard2 = 0xaabbccdd;
            tess.numVertexes = 3; tess.numIndexes = 3;
            entity.e.frame = frame; entity.e.oldframe = 0; entity.e.backlerp = 0.375f;
            rb_surfaceTable[SF_MDC](&model.mdc[0]->surfaces[0]);
            assert(tess.guard1 == 0x12345678 && tess.guard2 == 0xaabbccdd);
            assert(tess.numVertexes == 6 && tess.numIndexes == 6 && tess.surfType == SF_MDC);
            assert(tess.indexes[3] == 5 && tess.indexes[4] == 3 && tess.indexes[5] == 4);
            for (int vertex = 0; vertex < 3; ++vertex) {
                vec3_t xyz, normal;
                assert(FnMdc_AnimatedVertex(&model.mdc[0]->surfaces[0].view, &r_mdcNormals, 0, frame,
                    frame ? 0.375f : 0, vertex, xyz, normal));
                assert(!std::memcmp(xyz, tess.xyz[vertex + 3], sizeof(xyz)));
                assert(!std::memcmp(normal, tess.normal[vertex + 3], sizeof(normal)));
                assert(tess.texCoords[0][vertex + 3][0] == vertex * 0.25f);
                assert(tess.texCoords[0][vertex + 3][1] == vertex * -0.5f);
            }
        }
        tess.numVertexes = tess.numIndexes = 11;
        rb_surfaceTable[SF_MDC](&model.mdc[0]->surfaces[0]); assert(tess.numVertexes == 3 && tess.numIndexes == 3);
        entity.e.frame = 1; entity.e.oldframe = 0; entity.e.backlerp = NAN;
        try { rb_surfaceTable[SF_MDC](&model.mdc[0]->surfaces[0]); assert(false); } catch (const std::runtime_error &) {}
        assert(tess.numVertexes == 3 && tess.numIndexes == 3);
        failQueue = true;
        try { rb_surfaceTable[SF_MDC](&model.mdc[0]->surfaces[0]); assert(false); } catch (const std::runtime_error &) {}
        failQueue = false;
        assert(tess.numVertexes == 3 && tess.numIndexes == 3);
    }
    files.clear(); model_t missing{}; assert(!R_RegisterMDC(names[0], &missing)); assert(missing.type == MOD_BAD);
    files[names[0]] = payload; files[names[1]] = payload; Put32(files[names[1]].data() + 76, 2);
    files[names[2]] = payload; model_t partial{}; partial.index = 13;
    assert(R_RegisterMDC(names[0], &partial) == 13 && partial.mdc[1] == partial.mdc[2]);
    assert(!liveFiles);
    auto before = allocations.size(); model_t model{}; model.dataSize = INT_MAX;
    assert(!R_LoadMDC(&model, original, BYTES, "overflow", 0) && allocations.size() == before);
    model = {}; assert(!R_LoadMDC(&model, original, BYTES - 1, "truncated", 0));
    assert(!R_LoadMDC(&model, original, -1, "negative", 0));
    failAllocation = true;
    try { R_LoadMDC(&model, original, BYTES, "allocation failure", 0); assert(false); } catch (const std::bad_alloc &) {}
    failAllocation = false; assert(!model.dataSize);
    failShader = true;
    try { R_LoadMDC(&model, original, BYTES, "shader failure", 0); assert(false); } catch (const std::runtime_error &) {}
    failShader = false; assert(!model.dataSize);
    const auto *special = R_LoadMDC(&model, original, BYTES, "models/mg42/gun.mdc", 0);
    assert(special && special->frames[0].radius == 256 && special->frames[2].bounds[0][0] == 128 && special->frames[2].bounds[1][0] == -128);
    assert(FnMdc_Float(special->view.data + FRAMES + 36) == 32);
    // MD3 view access continues to use its own frame/surface/shader representation.
    struct { md3Header_t header; md3Frame_t frames[2]; md3Surface_t surface; md3Shader_t shaders[2]; } md3{};
    md3.header.numFrames = 2; md3.header.numSurfaces = 1;
    md3.header.ofsFrames = offsetof(decltype(md3), frames); md3.header.ofsSurfaces = offsetof(decltype(md3), surface);
    md3.surface.ident = SF_MD3; md3.surface.numShaders = 2; md3.surface.ofsShaders = sizeof(md3Surface_t);
    md3.surface.ofsEnd = sizeof(md3Surface_t) + sizeof(md3.shaders); std::strcpy(md3.surface.name, "legacy");
    md3.shaders[0].shaderIndex = 3; md3.shaders[1].shaderIndex = 9;
    model = {}; model.type = MOD_MESH; model.md3[0] = &md3.header;
    assert(R_MeshFrameCount(&model) == 2 && R_MeshFrame(&model, 0, 1) == &md3.frames[1]);
    meshSurfaceView_t view; R_MeshFirstSurface(&model, 0, &view); R_MeshSurfaceDescribe(&view);
    assert(view.data == (surfaceType_t *)&md3.surface && !std::strcmp(view.name, "legacy"));
    assert(R_MeshShaderIndex(&view, 3) == 9 && R_MeshShaderIndex(&view, -1) == 3);
    view.numShaders = 0; assert(R_MeshShaderIndex(&view, 0) == 0); R_MeshNextSurface(&view);
    assert(flushes && overflows && errors && !liveFiles);
}
'''


def renderer_fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    surface_enum = re.search(r'typedef enum\s*\{[^{}]*SF_BAD[^{}]*\}\s*surfaceType_t;', local)[0]
    model_enum = re.search(r'typedef enum\s*\{[^{}]*MOD_BAD[^{}]*\}\s*modtype_t;', local)[0]
    model_type = re.search(r'typedef struct model_s\s*\{[^{}]*\}\s*model_t;', local)[0]
    table_text = (ROOT / 'code' / renderer / 'tr_surface.c').read_text()
    table_start = table_text.index('void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])')
    table = table_text[table_start:table_text.index('\n};', table_start) + 3]
    # Whole entries only: one surface function's name can begin another's.
    table = re.sub(r'\)\)(RB_\w+)', lambda m: m[0] if m[1] == 'RB_SurfaceMDC' else '))Noop', table)
    source = PRELUDE + surface_enum + '\n' + model_enum + '\n'
    source += '#include "code/renderercommon/tr_mdc_model_types.h"\n#include "code/renderercommon/tr_mds_model_types.h"\n#include "code/renderercommon/tr_wolfet_model_types.h"\nstruct bmodel_t;\n' + model_type
    source += '\nstruct trRefEntity_t { refEntity_t e; };\n' + CALLBACKS
    return source + table + '\n' + FIXTURE[:FIXTURE.index('int main(void)')]


class MdcRendererTests(unittest.TestCase):
    def test_model_ownership_lods_tags_and_renderer_dispatch(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(prefix='mdc-render-', dir=ROOT / '.tmp') as temp:
                source = renderer_fixture(renderer) + CHECKS
                folder = Path(temp)
                fixture = folder / 'probe.cpp'
                executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                fixture.write_text(source, encoding='utf-8')
                build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(fixture),
                    '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=90)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
