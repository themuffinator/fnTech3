"""Run MDC's RTX transport against bounded geometry/material callbacks."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mdc_renderer_tests import ROOT, renderer_fixture

FIXTURE = r'''
static model_t rayModel;
struct skin_t { int numSurfaces; struct { const char *name; shader_t *shader; } surfaces[2]; } skin;
static shader_t shaderTable[12];
static struct { int numSkins; shader_t *defaultShader; } tr{2, shaderTable};
static const model_t *R_GetModelByHandle(int handle) { assert(handle == 11); return &rayModel; }
static shader_t *R_GetShaderByHandle(int handle) { assert(handle >= 0 && handle < 12); return &shaderTable[handle]; }
static const skin_t *R_GetSkinByHandle(int handle) { assert(handle == 1); return &skin; }
int Q_stricmp(const char *a,const char *b) {
    while(*a && *b && std::tolower((unsigned char)*a)==std::tolower((unsigned char)*b)) { ++a; ++b; }
    return std::tolower((unsigned char)*a)-std::tolower((unsigned char)*b);
}
enum { RTX_RT_MATFLAG_MASKED = 1 };
struct rtxRtCpuGeometry_t {
    unsigned numVertices, numTriangles, sourceTriangleCount, sourceEntityCount, maskedTriangleCount;
    struct { int flags; } materials[1];
    struct { float xyz[3], normal[3], uv[2]; } vertices[3];
    unsigned triangles[1][3];
};
static int failure, tuning, materialHandle;
static qboolean vk_rt_checked_u32_multiply(uint32_t a, uint32_t b, uint32_t *out) {
    if (a > UINT32_MAX / b) return qfalse; *out = a * b; return qtrue;
}
static qboolean vk_rt_cpu_geometry_find_or_add_material(rtxRtCpuGeometry_t *geometry, const shader_t *shader, uint32_t *out) {
    materialHandle = shader->index; *out = 0; geometry->materials[0].flags = materialHandle == 7 ? RTX_RT_MATFLAG_MASKED : 0;
    return failure == 1 ? qfalse : qtrue;
}
static qboolean vk_rt_cpu_geometry_reserve(rtxRtCpuGeometry_t *, uint32_t verts, uint32_t indexes) {
    assert(verts == 3 && indexes == 3); return failure == 2 ? qfalse : qtrue;
}
static void vk_rt_apply_effect_material_tuning(rtxRtCpuGeometry_t *, uint32_t material, qboolean a, qboolean b) {
    assert(material == 0 && !a && !b); ++tuning;
}
static void vk_rt_transform_point(const trRefEntity_t *ent, const vec3_t local, vec3_t world) {
    for (int i = 0; i < 3; ++i) world[i] = local[i] + ent->e.origin[i];
}
static void vk_rt_transform_direction(const trRefEntity_t *, const vec3_t local, vec3_t world) { std::memcpy(world, local, sizeof(vec3_t)); }
static qboolean vk_rt_cpu_geometry_add_vertex(rtxRtCpuGeometry_t *geometry, const vec3_t xyz, const vec3_t normal,
    const vec4_t tangent, const vec2_t uv, uint32_t color, uint32_t material, void *unused) {
    assert(geometry->numVertices < 3 && material == 0 && color == UINT32_MAX && !unused);
    assert(tangent[0] == 1 && tangent[3] == 1);
    if (failure == 3) return qfalse;
    auto &vertex = geometry->vertices[geometry->numVertices++];
    std::memcpy(vertex.xyz, xyz, sizeof(vec3_t)); std::memcpy(vertex.normal, normal, sizeof(vec3_t)); std::memcpy(vertex.uv, uv, sizeof(vec2_t));
    return qtrue;
}
static qboolean vk_rt_cpu_geometry_add_triangle(rtxRtCpuGeometry_t *geometry, uint32_t a, uint32_t b, uint32_t c) {
    assert(!geometry->numTriangles && a < 3 && b < 3 && c < 3);
    if (failure == 4) return qfalse;
    geometry->triangles[0][0] = a; geometry->triangles[0][1] = b; geometry->triangles[0][2] = c; ++geometry->numTriangles;
    return qtrue;
}
#include "code/renderercommon/tr_mdc_ray_geometry.h"
int main() {
    Make(); FnMdc_InitNormals(&r_mdcNormals);
    for (int i = 0; i < 12; ++i) shaderTable[i].index = i;
    files["model.mdc"] = std::vector<byte>(original, original + BYTES);
    rayModel.index = 11; assert(R_RegisterMDC("model.mdc", &rayModel) == 11);
    entity.e.hModel = 11; entity.e.origin[0] = 128; entity.e.origin[2] = -32;
    for (int frame : {-1, 0, 1, 2, 3, INT_MAX}) {
        for (int wrap : {0, RF_WRAP_FRAMES}) {
            rtxRtCpuGeometry_t geometry{};
            entity.e.renderfx = wrap; entity.e.frame = frame; entity.e.oldframe = 1; entity.e.backlerp = 0.375f;
            assert(vk_rt_append_mdc_entity_geometry(&geometry, &entity));
            assert(geometry.numVertices == 3 && geometry.numTriangles == 1);
            assert(geometry.sourceTriangleCount == 1 && geometry.sourceEntityCount == 1 && geometry.maskedTriangleCount == 1);
            assert(geometry.triangles[0][0] == 2 && geometry.triangles[0][1] == 0 && geometry.triangles[0][2] == 1);
            int current = wrap ? frame % 3 : frame, old = 1;
            if (current < 0 || current >= 3) current = old = 0;
            vec3_t xyz, normal;
            assert(FnMdc_AnimatedVertex(&rayModel.mdc[0]->surfaces[0].view, &r_mdcNormals, old, current,
                old == current ? 0 : 0.375f, 0, xyz, normal));
            for (int i = 0; i < 3; ++i) {
                assert(geometry.vertices[0].xyz[i] == xyz[i] + entity.e.origin[i]);
                assert(geometry.vertices[0].normal[i] == normal[i]);
            }
            assert(geometry.vertices[2].uv[0] == 0.5f && geometry.vertices[2].uv[1] == -1);
        }
    }
    entity.e.frame = 2; entity.e.oldframe = 0; entity.e.renderfx = RF_FIRST_PERSON;
    entity.e.skinNum = -1;
    rtxRtCpuGeometry_t geometry{};
    assert(vk_rt_append_mdc_entity_geometry(&geometry, &entity) && tuning == 1 && materialHandle == 7);
    skin.numSurfaces = 1; skin.surfaces[0] = {"surface", &shaderTable[8]}; entity.e.customSkin = 1;
    geometry = {}; assert(vk_rt_append_mdc_entity_geometry(&geometry, &entity) && materialHandle == 8 && !geometry.maskedTriangleCount);
    skin.surfaces[0].name = "absent";
    geometry = {}; assert(vk_rt_append_mdc_entity_geometry(&geometry, &entity) && materialHandle == 0);
    entity.e.customShader = 9;
    geometry = {}; assert(vk_rt_append_mdc_entity_geometry(&geometry, &entity) && materialHandle == 9);
    entity.e.customShader=0; entity.e.rtcw.blink=qtrue; ri.gameId=COM_GAME_RTCW_MP;
    skin.numSurfaces=2; skin.surfaces[0]={"surface",&shaderTable[8]}; skin.surfaces[1]={"surface_b",&shaderTable[9]};
    geometry={}; assert(vk_rt_append_mdc_entity_geometry(&geometry,&entity) && materialHandle==9);
    skin.surfaces[1].shader=tr.defaultShader;
    geometry={}; assert(vk_rt_append_mdc_entity_geometry(&geometry,&entity) && materialHandle==8);
    ri.gameId=COM_GAME_RTCW_SP; entity.e.rtcw.blink=qfalse;
    std::strcpy(rayModel.mdc[0]->surfaces[0].name,"h_blink");
    geometry={}; assert(vk_rt_append_mdc_entity_geometry(&geometry,&entity) && !geometry.numVertices);
    entity.e.rtcw.blink=qtrue; entity.e.customShader=9;
    geometry={}; assert(vk_rt_append_mdc_entity_geometry(&geometry,&entity) && geometry.numVertices==3);
    for (failure = 1; failure <= 4; ++failure) {
        geometry = {}; assert(!vk_rt_append_mdc_entity_geometry(&geometry, &entity));
        assert(!geometry.sourceTriangleCount && !geometry.sourceEntityCount);
    }
    failure = 0; entity.e.backlerp = NAN; geometry = {};
    assert(!vk_rt_append_mdc_entity_geometry(&geometry, &entity) && !geometry.numVertices);
    entity.e.backlerp = 0.25f; rayModel.mdc[0]->view.numSurfaces = 0;
    assert(vk_rt_append_mdc_entity_geometry(&geometry, &entity) && !geometry.numVertices);
    rayModel.type = MOD_BAD; assert(vk_rt_append_mdc_entity_geometry(&geometry, &entity));
}
'''


class MdcRayGeometryTests(unittest.TestCase):
    def test_animation_materials_indices_and_failure_propagation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='mdc-ray-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            fixture = '#include "code/qcommon/game_profile.h"\n' + renderer_fixture('rendererrtx')
            fixture = fixture.replace('decltype(&::Error) Error;', 'decltype(&::Error) Error; comGameId_t gameId;')
            fixture = fixture.replace('FreeFile, Print, Error};', 'FreeFile, Print, Error, COM_GAME_QUAKE3};')
            source.write_text(fixture + FIXTURE, encoding='utf-8')
            build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(source),
                '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
