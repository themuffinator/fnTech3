"""Execute the MDS ray transport with bounded geometry and material callbacks."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mds_renderer_tests import ROOT, renderer_fixture
from mdc_ray_geometry_tests import FIXTURE as MDC_FIXTURE

CALLBACKS = MDC_FIXTURE[MDC_FIXTURE.index('enum { RTX_RT_MATFLAG_MASKED'):MDC_FIXTURE.index('int main()')].replace('tr_mdc_ray_geometry.h', 'tr_mds_ray_geometry.h')

CHECKS = r'''
int main() {
    Init();
    entity.e.origin[0] = 128; entity.e.origin[2] = -32;
    for (auto game : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        ri.gameId = game;
        for (int frame : {-1, 0, 1, 2, INT_MAX}) {
            for (int wrap : {0, RF_WRAP_FRAMES}) {
                rtxRtCpuGeometry_t geometry{};
                entity.e.renderfx = wrap; entity.e.frame = frame;
                auto input = entity.e;
                assert(vk_rt_append_mds_entity_geometry(&geometry, &entity));
                assert(!std::memcmp(&input, &entity.e, sizeof(input)));
                assert(geometry.numVertices == 3 && geometry.numTriangles == 1);
                assert(geometry.sourceTriangleCount == 1 && geometry.sourceEntityCount == 1 && geometry.maskedTriangleCount == 1);
                assert(geometry.triangles[0][0] == 0 && geometry.triangles[0][1] == 2 && geometry.triangles[0][2] == 1);
                R_MdsSanitizeFrames(&testModel.mds->view, &input, game);
                fnMdsPose_t pose; assert(R_MdsEntityPose(&testModel.mds->view, &input, game, &pose));
                for (int vertex = 0; vertex < 3; ++vertex) {
                    vec3_t xyz, normal;
                    assert(FnMds_SkinVertex(&testModel.mds->surfaces[0].vertices[vertex], &pose, xyz, normal));
                    for (int axis = 0; axis < 3; ++axis) {
                        // Match the stored float, including x87's wider expression precision.
                        volatile float expected = xyz[axis] + entity.e.origin[axis];
                        assert(geometry.vertices[vertex].xyz[axis] == expected);
                        assert(geometry.vertices[vertex].normal[axis] == normal[axis]);
                    }
                    assert(geometry.vertices[vertex].uv[0] == vertex * 0.25f);
                    assert(geometry.vertices[vertex].uv[1] == vertex * -0.5f);
                }
            }
        }
    }
    entity.e.frame = 1; entity.e.oldframe = 0; entity.e.renderfx = RF_FIRST_PERSON;
    rtxRtCpuGeometry_t geometry{};
    assert(vk_rt_append_mds_entity_geometry(&geometry, &entity) && tuning == 1 && materialHandle == 7);
    entity.e.customSkin = 1; entity.e.rtcw.blink = qtrue; skin.numSurfaces = 2;
    skin.surfaces[0] = {"surface", &shaderTable[8]}; skin.surfaces[1] = {"surface_b", &shaderTable[9]};
    geometry = {}; assert(vk_rt_append_mds_entity_geometry(&geometry, &entity) && materialHandle == 9);
    ri.gameId = COM_GAME_RTCW_SP;
    geometry = {}; assert(vk_rt_append_mds_entity_geometry(&geometry, &entity) && materialHandle == 8);
    std::strcpy(testModel.mds->surfaces[0].name, "h_blink"); entity.e.rtcw.blink = qfalse;
    geometry = {}; assert(vk_rt_append_mds_entity_geometry(&geometry, &entity) && !geometry.numVertices && !geometry.sourceEntityCount);
    entity.e.rtcw.blink = qtrue; entity.e.customShader = 10;
    geometry = {}; assert(vk_rt_append_mds_entity_geometry(&geometry, &entity) && materialHandle == 10);
    for (failure = 1; failure <= 4; ++failure) {
        geometry = {}; assert(!vk_rt_append_mds_entity_geometry(&geometry, &entity));
        assert(!geometry.sourceTriangleCount && !geometry.sourceEntityCount);
    }
    failure = 0; entity.e.backlerp = NAN;
    geometry = {}; assert(!vk_rt_append_mds_entity_geometry(&geometry, &entity) && !geometry.numVertices);
    entity.e.backlerp = 0.5f; entity.e.rtcw.torsoAxis[0][0] = INFINITY;
    geometry = {}; assert(!vk_rt_append_mds_entity_geometry(&geometry, &entity) && !geometry.numVertices);
    testModel.type = MOD_MESH;
    assert(vk_rt_append_mds_entity_geometry(&geometry, &entity) && !geometry.numVertices);
}
'''


class MdsRayGeometryTests(unittest.TestCase):
    def test_animation_materials_and_failure_propagation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='mds-ray-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source, executable = folder / 'probe.cpp', folder / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(renderer_fixture('rendererrtx') + CALLBACKS + CHECKS, encoding='utf-8')
            build = subprocess.run([compiler, *target, '-std=c++17', '-fno-operator-names', '-I', str(ROOT), str(source),
                '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
