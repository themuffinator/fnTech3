"""Exercise independent MDS pose evaluation and weighted vertex deformation."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mds_format_tests import FIXTURE
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    source = '#define _CRT_SECURE_NO_WARNINGS 1\n#include "code/qcommon/q_shared.h"\n#include <float.h>\n'
    source += 'const vec3_t vec3_origin = {0,0,0};\n'
    math = (ROOT / 'code/qcommon/q_math.c').read_text()
    for signature in ('void AngleVectors(', 'void AnglesToAxis('):
        source += function(math, signature)
    source += FIXTURE[:FIXTURE.index('int main(void)')]
    return source + '#include "code/renderercommon/tr_mds_pose.h"\n'


CHECKS = r'''
static void Near(float actual, float expected) { assert(fabsf(actual - expected) < 0.00002f); }
static fnMdsPoseParams_t Params(void) {
    fnMdsPoseParams_t result;
    memset(&result, 0, sizeof(result)); result.torsoAxis[0][0] = result.torsoAxis[1][1] = result.torsoAxis[2][2] = 1;
    return result;
}
static void ZeroAngles(void) {
    int frame; for (frame = 0; frame < 2; ++frame) memset(original + FRAMES + frame * 88 + 52, 0, 36);
}
static void Rejected(const fnMdsView_t *view, const fnMdsPoseParams_t *params) {
    fnMdsPose_t result, saved;
    memset(&result, 0xa5, sizeof(result)); saved = result;
    assert(!FnMds_DecodePose(view, params, &result) && !memcmp(&result, &saved, sizeof(result)));
}
int main(void) {
    fnMdsView_t view;
    fnMdsPose_t pose, saved;
    fnMdsPoseParams_t params = Params();
    fnMdsSurfaceView_t surface;
    fnMdsVertexView_t vertex;
    float xyz[3], normal[3];
    int i, j, k;
    Make(); ZeroAngles(); assert(FnMds_Open(original, BYTES, &view));
    assert(FnMds_DecodePose(&view, &params, &pose) && pose.numBones == 3);
    for (i = 0; i < 3; ++i) {
        Near(pose.bones[i].origin[0], i == 2 ? 30.0f : i * 10.0f);
        Near(pose.bones[i].origin[1], 1); Near(pose.bones[i].origin[2], 2);
        for (j = 0; j < 3; ++j) for (k = 0; k < 3; ++k) Near(pose.bones[i].axis[j][k], j == k ? 1.0f : 0);
    }
    assert(FnMds_Surface(&view, SURFACE, &surface));
    assert(FnMds_Vertex(&surface, VERTS + 52, &vertex));
    assert(FnMds_SkinVertex(&vertex, &pose, xyz, normal));
    Near(xyz[0], 23.625f); Near(xyz[1], -8.75f); Near(xyz[2], 1002);
    Near(normal[0], 0); Near(normal[1], 1); Near(normal[2], 0);
    saved = pose;
    pose.bones[2].axis[1][1] = 100;
    assert(FnMds_SkinVertex(&vertex, &pose, xyz, normal));
    Near(normal[1], 1); // First bone alone transforms the normal.
    pose = saved;
    {
        const float backs[] = {-0.5f, 0, 0.25f, 0.5f, 1, 1.5f};
        params.frame = params.torsoFrame = 1;
        for (i = 0; i < 6; ++i) {
            params.backlerp = backs[i]; params.torsoBacklerp = 1 - backs[i];
            assert(FnMds_DecodePose(&view, &params, &pose));
            for(uint32_t bone=0;bone<view.numBones;++bone){
                fnMdsTransform_t selected;assert(FnMds_DecodeBone(&view,&params,bone,&selected));
                for(int row=0;row<3;++row){Near(selected.origin[row],pose.bones[bone].origin[row]);
                    for(int col=0;col<3;++col)Near(selected.axis[row][col],pose.bones[bone].axis[row][col]);}
            }
            Near(pose.bones[0].origin[0], 10 * (1 - backs[i]));
            Near(pose.bones[2].origin[0], 30 + 10 * (1 - backs[i]));
        }
    }
    params = Params(); params.oldTorsoFrame = params.torsoFrame = 1;
    for (i = 1; i < 3; ++i) {
        Put16(original + FRAMES + 88 + 52 + i * 12 + 2, 16384); // Torso yaw 90 degrees.
        Put16(original + FRAMES + 88 + 52 + i * 12 + 10, 16384);
    }
    assert(FnMds_DecodePose(&view, &params, &pose));
    Near(pose.bones[1].axis[0][0], 0.70710678f); Near(pose.bones[1].axis[0][1], 0.70710678f);
    Near(pose.bones[1].origin[0], 7.0710678f); Near(pose.bones[1].origin[1], 8.0710678f);
    Near(pose.bones[2].origin[0], 7.0710678f); Near(pose.bones[2].origin[1], 28.0710678f);
    params.torsoAxis[0][0] = params.torsoAxis[1][1] = 0;
    params.torsoAxis[0][1] = 1; params.torsoAxis[1][0] = -1;
    assert(FnMds_DecodePose(&view, &params, &pose));
    Near(pose.bones[1].axis[0][0], 0.70710678f); Near(pose.bones[1].axis[0][1], 0);
    Near(pose.bones[1].axis[1][0], 0); Near(pose.bones[1].axis[1][1], 0.70710678f);
    Near(pose.bones[2].axis[0][0], -1); Near(pose.bones[2].axis[0][1], 0);
    Near(pose.bones[2].origin[0], -12.9289322f); Near(pose.bones[2].origin[1], 8.0710678f);
    // A serialized full-torso tag must not leak its selection into an unrelated
    // partial-torso mesh bone (the femzombie reference-order regression).
    Make(); ZeroAngles(); params = Params(); params.oldTorsoFrame = params.torsoFrame = 1;
    PutFloat(original + BONES + 80 + 68, 1); Put32(original + BONES + 80 + 76, 1);
    Put32(original + BONES + 160 + 64, 0); PutFloat(original + BONES + 160 + 68, 0.5f);
    Put32(original + BONES + 160 + 76, 0);
    for (i = 1; i < 3; ++i) Put16(original + FRAMES + 88 + 52 + i * 12 + 2, 16384);
    assert(FnMds_DecodePose(&view, &params, &pose)); Near(pose.bones[2].axis[0][0], 0.70710678f);
    saved = pose;
    PutFloat(original + BONES + 80 + 68, 0);
    assert(FnMds_DecodePose(&view, &params, &pose));
    assert(!memcmp(pose.bones[2].axis, saved.bones[2].axis, sizeof(pose.bones[2].axis)));
    // Cross the signed-angle seam by the shortest route, not through zero.
    Make(); ZeroAngles(); params = Params(); params.frame = 1; params.backlerp = 0.5f;
    Put16(original + FRAMES + 52 + 2, 30948); Put16(original + FRAMES + 88 + 52 + 2, -30948);
    assert(FnMds_DecodePose(&view, &params, &pose)); Near(pose.bones[0].axis[0][0], -1);
    {
        float wrapped = 123;
        assert(FnMds_WrapAngle(-180, &wrapped) && wrapped == 180);
        assert(FnMds_WrapAngle(360, &wrapped) && wrapped == 0);
        assert(!FnMds_WrapAngle(FLT_MAX, &wrapped) && wrapped == 0);
        assert(!FnMds_WrapAngle(NAN, &wrapped) && wrapped == 0);
        const float a[3] = {1,0,0}, b[3] = {-1,0,0}; float out[3];
        assert(FnMds_BlendDirection(a, b, 0.5f, out) && out[0] == 0);
    }
    Make(); ZeroAngles(); params = Params(); params.backlerp = params.torsoBacklerp = NAN;
    assert(FnMds_DecodePose(&view, &params, &pose)); // Equal frames ignore the unused fractions.
    {
        const fnMdsPoseParams_t good=params;fnMdsTransform_t selected,expected,before;
        assert(FnMds_DecodeBone(&view,&params,0,&expected));
        params.torsoFrame=UINT32_MAX;params.oldTorsoFrame=INT_MAX;params.torsoBacklerp=NAN;
        for(int row=0;row<3;++row)for(int col=0;col<3;++col)params.torsoAxis[row][col]=NAN;
        assert(FnMds_DecodeBone(&view,&params,0,&selected)&&!memcmp(&selected,&expected,sizeof(selected)));
        before=selected;assert(!FnMds_DecodeBone(&view,&params,2,&selected)&&!memcmp(&selected,&before,sizeof(selected)));
        assert(!FnMds_DecodeBone(&view,&params,UINT32_MAX,&selected));
        assert(!FnMds_DecodeBone(NULL,&params,0,&selected)&&!FnMds_DecodeBone(&view,NULL,0,&selected));
        assert(!FnMds_DecodeBone(&view,&params,0,NULL));
        // A needed ancestor's animation matters even if the child has no torso weight.
        PutFloat(original+BONES+2*80+68,0);assert(!FnMds_DecodeBone(&view,&params,2,&selected));
        params.torsoFrame=params.oldTorsoFrame=0;
        assert(FnMds_DecodeBone(&view,&params,2,&selected)); // Unused torso axis is still ignored.
        assert(!FnMds_DecodeBone(&view,&params,1,&selected)); // This output does rotate with the torso.
        params=good;Make();ZeroAngles();
        Put32(original+BONES+64,2);assert(!FnMds_DecodeBone(&view,&params,2,&selected));
        Make();ZeroAngles();
    }
    params.frame = 1; Rejected(&view, &params); params = Params(); params.torsoFrame = 1; params.torsoBacklerp = INFINITY; Rejected(&view, &params);
    params = Params(); params.frame = UINT32_MAX; Rejected(&view, &params);
    params = Params(); params.oldFrame = 2; Rejected(&view, &params);
    params = Params(); params.torsoFrame = 2; Rejected(&view, &params);
    params = Params(); params.oldTorsoFrame = 2; Rejected(&view, &params);
    params = Params(); params.torsoAxis[0][1] = INFINITY; Rejected(&view, &params);
    params = Params(); params.frame = 1; params.backlerp = FLT_MAX; Rejected(&view, &params);
    params = Params(); Rejected(NULL, &params); Rejected(&view, NULL); assert(!FnMds_DecodePose(&view, &params, NULL));
    Put32(original + BONES + 64, 2); Rejected(&view, &params); // A cycle cannot recurse or publish.
    Make(); ZeroAngles(); Put32(original + BONES + 80 + 64, UINT32_MAX); Put32(original + BONES + 64, 2);
    assert(FnMds_Open(original, BYTES, &view) && FnMds_DecodePose(&view, &params, &pose)); // Forward ancestry and two roots.
    Make(); ZeroAngles(); assert(FnMds_Open(original, BYTES, &view) && FnMds_DecodePose(&view, &params, &pose));
    {
        float output[6], before[6];
        for (i = 0; i < 6; ++i) output[i] = 100.0f + i;
        memcpy(before, output, sizeof(output));
        pose.bones[2].origin[0] = INFINITY;
        assert(!FnMds_SkinVertex(&vertex, &pose, output, output + 3) && !memcmp(output, before, sizeof(output)));
        pose.numBones = 2;
        assert(!FnMds_SkinVertex(&vertex, &pose, output, output + 3) && !memcmp(output, before, sizeof(output)));
        assert(!FnMds_SkinVertex(NULL, &pose, output, output + 3));
        assert(!FnMds_SkinVertex(&vertex, NULL, output, output + 3));
        assert(!FnMds_SkinVertex(&vertex, &pose, NULL, output + 3));
        assert(!FnMds_SkinVertex(&vertex, &pose, output, NULL));
        assert(FnMds_DecodePose(&view, &params, &pose));
        assert(FnMds_SkinVertex(&vertex, &pose, output, output)); Near(output[0], 0); Near(output[1], 1); Near(output[2], 0);
    }
    return 0;
}
'''


class MdsPoseTests(unittest.TestCase):
    def test_pose_and_weighted_vertices(self):
        for language in ('c', 'c++'):
            compiler = shutil.which('clang' if language == 'c' else 'clang++')
            if not compiler:
                self.skipTest('Clang C/C++ compiler unavailable')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            with self.subTest(language=language), tempfile.TemporaryDirectory(prefix='mds-pose-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp)
                source = folder / ('probe.c' if language == 'c' else 'probe.cpp')
                executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(fixture() + CHECKS)
                command = [compiler, *target, '-std=' + ('c11' if language == 'c' else 'c++17'),
                           '-I', str(ROOT), str(source), '-o', str(executable)]
                if os.name != 'nt':
                    command.append('-lm')
                build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
