"""ET bone posing and weighted vertex placement, from its own skeletons."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
POSE = ROOT/'code/renderercommon/tr_wolfet_pose.h'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-pose.md'

# A skeleton of three bones in a chain, with two frames, built by hand so every
# posed value below is one that can be worked out on paper.
FIXTURE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <float.h>
const vec3_t vec3_origin = {0,0,0};
'''

BUILDER = r'''
#include "code/renderercommon/tr_wolfet_pose.h"

enum { BONES = 3, FRAMES = 2 };
enum { HEADER = FN_WOLFET_MDX_HEADER_BYTES,
       STRIDE = FN_WOLFET_MDX_FRAME_BYTES + BONES * FN_WOLFET_MDX_BONE_FRAME_BYTES,
       FRAMES_AT = HEADER,
       BONES_AT = HEADER + FRAMES * STRIDE,
       BYTES = BONES_AT + BONES * FN_WOLFET_MDX_BONE_BYTES };

static unsigned char skeleton[BYTES];

static void PutI(unsigned char *p, int32_t value) { memcpy(p, &value, 4); }
static void PutF(unsigned char *p, float value) { memcpy(p, &value, 4); }
static void PutS(unsigned char *p, int value) { short v = (short)value; memcpy(p, &v, 2); }

/* Bone n has parent n-1 and sits `dist` further along the direction its own
 * offset angles describe; every angle is zero unless a test sets one. */
static void Build(void) {
	int frame, bone;
	memset(skeleton, 0, sizeof(skeleton));
	PutI(skeleton, (int32_t)FN_WOLFET_MDX_IDENT);
	PutI(skeleton + 4, FN_WOLFET_MDX_VERSION);
	PutI(skeleton + 72, FRAMES);
	PutI(skeleton + 76, BONES);
	PutI(skeleton + 80, FRAMES_AT);
	PutI(skeleton + 84, BONES_AT);
	PutI(skeleton + 88, -1);
	PutI(skeleton + 92, BYTES);
	for (frame = 0; frame < FRAMES; ++frame) {
		unsigned char *f = skeleton + FRAMES_AT + frame * STRIDE;
		PutF(f + 40, 1); PutF(f + 44, 2); PutF(f + 48, 3);   /* parent offset */
	}
	for (bone = 0; bone < BONES; ++bone) {
		unsigned char *b = skeleton + BONES_AT + bone * FN_WOLFET_MDX_BONE_BYTES;
		b[0] = (unsigned char)('a' + bone); b[1] = 0;
		PutI(b + 64, bone - 1);          /* parent, -1 for the root */
		PutF(b + 68, 0);                 /* torso weight */
		PutF(b + 72, 10);                /* distance from the parent */
	}
}

static void Near(float actual, float expected) { assert(fabsf(actual - expected) < 0.0002f); }

static fnWolfETPoseParams_t Params(const fnWolfETSkeletonView_t *view) {
	fnWolfETPoseParams_t p;
	memset(&p, 0, sizeof(p));
	p.frameView = p.oldFrameView = p.torsoView = p.oldTorsoView = view;
	p.torsoAxis[0][0] = p.torsoAxis[1][1] = p.torsoAxis[2][2] = 1;
	return p;
}

static void Rejected(const fnWolfETPoseParams_t *params) {
	fnWolfETPose_t pose, saved;
	memset(&pose, 0xa5, sizeof(pose));
	saved = pose;
	/* A refused pose must leave the caller's storage exactly as it was. */
	assert(!FnWolfET_DecodePose(params, &pose) && !memcmp(&pose, &saved, sizeof(pose)));
}

int main(void) {
	fnWolfETSkeletonView_t view, other;
	fnWolfETPose_t pose;
	fnWolfETPoseParams_t params;
	int i, j, k;

	Build();
	assert(FnWolfET_Skeleton(skeleton, BYTES, &view));
	params = Params(&view);

	/* With every angle zero the chain runs along x, ten units a bone, starting
	 * at the frame's parent offset. */
	assert(FnWolfET_DecodePose(&params, &pose) && pose.numBones == BONES);
	for (i = 0; i < BONES; ++i) {
		Near(pose.bones[i].origin[0], 1 + i * 10.0f);
		Near(pose.bones[i].origin[1], 2);
		Near(pose.bones[i].origin[2], 3);
		for (j = 0; j < 3; ++j) for (k = 0; k < 3; ++k)
			Near(pose.bones[i].axis[j][k], j == k ? 1.0f : 0.0f);
	}

	/* A quarter turn of yaw on bone 1's offset sends its child up the y axis:
	 * 16384 sixteen-bit ticks is ninety degrees. */
	PutS(skeleton + FRAMES_AT + FN_WOLFET_MDX_FRAME_BYTES + FN_WOLFET_MDX_BONE_FRAME_BYTES + 10, 16384);
	assert(FnWolfET_Skeleton(skeleton, BYTES, &view));
	assert(FnWolfET_DecodePose(&params, &pose));
	Near(pose.bones[1].origin[0], 1);
	Near(pose.bones[1].origin[1], 12);
	Build();
	assert(FnWolfET_Skeleton(skeleton, BYTES, &view));

	/* One bone and its ancestry: bone 2 needs 0 and 1, and nothing else is
	 * written into the caller's pose. */
	memset(&pose, 0, sizeof(pose));
	assert(FnWolfET_DecodePoseSelected(&params, 2, &pose));
	Near(pose.bones[2].origin[0], 21);
	Near(pose.bones[0].origin[0], 1);
	assert(!FnWolfET_DecodePoseSelected(&params, BONES, &pose));

	/* A frame index outside either skeleton is refused. */
	params = Params(&view); params.frame = FRAMES; Rejected(&params);
	params = Params(&view); params.oldFrame = FRAMES; Rejected(&params);
	/* So is a non-finite blend or torso axis. */
	params = Params(&view); params.frame = 0; params.oldFrame = 1;
	params.backlerp = (float)(1.0 / 0.0) * 0.0f; Rejected(&params);
	params = Params(&view); params.torsoAxis[1][1] = (float)(1.0 / 0.0); Rejected(&params);
	/* And a missing skeleton. */
	params = Params(&view); params.frameView = NULL; Rejected(&params);
	params = Params(&view); params.oldFrameView = NULL; Rejected(&params);

	/* Two skeletons that do not describe the same bones must not be blended:
	 * a frame from one would be posed against the other's joint. */
	{
		static unsigned char shorter[BYTES];
		const int bones = BONES - 1;
		const int stride = FN_WOLFET_MDX_FRAME_BYTES + bones * FN_WOLFET_MDX_BONE_FRAME_BYTES;
		const int bonesAt = HEADER + FRAMES * stride;
		int bone;
		memset(shorter, 0, sizeof(shorter));
		memcpy(shorter, skeleton, HEADER);
		PutI(shorter + 76, bones);
		PutI(shorter + 84, bonesAt);
		for (bone = 0; bone < bones; ++bone) {
			unsigned char *b = shorter + bonesAt + bone * FN_WOLFET_MDX_BONE_BYTES;
			b[0] = (unsigned char)('a' + bone); b[1] = 0;
			PutI(b + 64, bone - 1);
			PutF(b + 72, 10);
		}
		assert(FnWolfET_Skeleton(shorter, BYTES, &other));
		params = Params(&view);
		params.oldFrameView = &other;
		Rejected(&params);
		/* A torso parent that differs is the same kind of disagreement. */
		memcpy(shorter, skeleton, BYTES);
		PutI(shorter + 88, 0);
		assert(FnWolfET_Skeleton(shorter, BYTES, &other));
		params = Params(&view);
		params.torsoView = &other;
		params.oldTorsoView = &other;
		{
			unsigned char *b = skeleton + BONES_AT + FN_WOLFET_MDX_BONE_BYTES;
			PutF(b + 68, 1.0f);
			assert(FnWolfET_Skeleton(skeleton, BYTES, &view));
			params.frameView = params.oldFrameView = &view;
			Rejected(&params);
			PutF(b + 68, 0);
			assert(FnWolfET_Skeleton(skeleton, BYTES, &view));
		}
	}

	/* A whole-skeleton pose always needs its torso frames, which is what ET's
	 * own bone calculation resolves before it starts. A single bone that
	 * carries no torso weight does not, so a tag lookup costs nothing extra. */
	params = Params(&view);
	params.torsoView = params.oldTorsoView = NULL;
	Rejected(&params);
	assert(FnWolfET_DecodePoseSelected(&params, 2, &pose));
	{
		unsigned char *b = skeleton + BONES_AT + FN_WOLFET_MDX_BONE_BYTES;
		PutF(b + 68, 1.0f);   /* bone 1 is fully driven by the torso */
		assert(FnWolfET_Skeleton(skeleton, BYTES, &view));
		params = Params(&view);
		params.torsoView = params.oldTorsoView = NULL;
		/* Bone 2 now depends on bone 1, so it needs the torso frames too. */
		assert(!FnWolfET_DecodePoseSelected(&params, 2, &pose));
		params = Params(&view);
		assert(FnWolfET_DecodePose(&params, &pose));
		PutF(b + 68, 0);
		assert(FnWolfET_Skeleton(skeleton, BYTES, &view));
	}

	/* Weighted placement: a vertex offset by (1,0,0) on the root and (2,0,0) on
	 * bone 1, half each, lands halfway between those two bone origins plus the
	 * offsets. The normal follows the first weight's bone only. */
	{
		unsigned char vertexBytes[FN_WOLFET_MDM_VERTEX_PREFIX_BYTES + 2 * FN_WOLFET_MDM_WEIGHT_BYTES];
		fnWolfETVertexView_t vertex;
		float xyz[3], normal[3];
		memset(vertexBytes, 0, sizeof(vertexBytes));
		PutF(vertexBytes + 0, 0); PutF(vertexBytes + 4, 1); PutF(vertexBytes + 8, 0);
		PutI(vertexBytes + 20, 2);
		PutI(vertexBytes + 24, 0); PutF(vertexBytes + 28, 0.5f); PutF(vertexBytes + 32, 1);
		PutI(vertexBytes + 44, 1); PutF(vertexBytes + 48, 0.5f); PutF(vertexBytes + 52, 2);
		memset(&vertex, 0, sizeof(vertex));
		vertex.numWeights = 2;
		vertex.weights = vertexBytes + FN_WOLFET_MDM_VERTEX_PREFIX_BYTES;
		vertex.normal[1] = 1;
		params = Params(&view);
		assert(FnWolfET_DecodePose(&params, &pose));
		assert(FnWolfET_SkinVertex(&vertex, &pose, xyz, normal));
		Near(xyz[0], 0.5f * (1 + 1) + 0.5f * (11 + 2));
		Near(xyz[1], 2); Near(xyz[2], 3);
		Near(normal[0], 0); Near(normal[1], 1); Near(normal[2], 0);
		/* A weight naming a bone the skeleton does not have is refused. */
		PutI(vertexBytes + 44, BONES);
		assert(!FnWolfET_SkinVertex(&vertex, &pose, xyz, normal));
		PutI(vertexBytes + 44, -1);
		assert(!FnWolfET_SkinVertex(&vertex, &pose, xyz, normal));
	}
	return 0;
}
'''


def fixture():
    source = FIXTURE
    math = (ROOT/'code/qcommon/q_math.c').read_text(encoding='utf-8')
    for signature in ('void AngleVectors(', 'void AnglesToAxis('):
        source += function(math, signature)
    return source + BUILDER


class WolfETPoseTests(unittest.TestCase):
    def test_pose(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-pose'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'pose.cpp'
            executable = folder/('pose.exe' if os.name == 'nt' else 'pose')
            source.write_text(fixture(), encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            built = subprocess.run([compiler, *target, '-std=c++17',
                '-I', str(ROOT), str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True,
                                 text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_each_frame_index_may_name_its_own_skeleton(self):
        """ET's entity carries a model handle beside every frame index."""
        text = POSE.read_text(encoding='utf-8')
        for field in ('frameView', 'oldFrameView', 'torsoView', 'oldTorsoView'):
            self.assertIn(field, text, field)
        self.assertIn('FnWolfET_SkeletonsAgree', text)

    def test_a_weight_names_a_global_bone(self):
        text = POSE.read_text(encoding='utf-8')
        self.assertIn('weight.bone >= pose->numBones', text)
        self.assertIn('not a position in the surface', text)

    def test_the_record_states_what_is_and_is_not_claimed(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('Nothing is drawn', record)
        self.assertIn('Four skeletons, not one', record)


if __name__ == '__main__':
    unittest.main()
