"""ET's detail reduction, its collapse map and its tag placement."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
LOD = ROOT/'code/renderercommon/tr_wolfet_lod.h'
POSE = ROOT/'code/renderercommon/tr_wolfet_pose.h'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-lod.md'

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <float.h>
const vec3_t vec3_origin = {0,0,0};
'''

BODY = r'''
#include "code/renderercommon/tr_wolfet_lod.h"
#include "code/renderercommon/tr_wolfet_pose.h"

static void Near(float actual, float expected) { assert(fabsf(actual - expected) < 0.0002f); }

static unsigned char collapseBytes[16 * 4];
static fnWolfETMeshSurfaceView_t Surface(uint32_t verts, uint32_t minLod) {
	fnWolfETMeshSurfaceView_t s;
	memset(&s, 0, sizeof(s));
	s.data = collapseBytes;
	s.bytes = sizeof(collapseBytes);
	s.numVerts = verts;
	s.minLod = minLod;
	s.collapseMap = 0;
	return s;
}
static void PutU(unsigned char *p, uint32_t v) { memcpy(p, &v, 4); }

int main(void) {
	uint32_t map[16];
	fnWolfETMeshSurfaceView_t surface;
	uint32_t i;

	/* A model that projects to nothing is drawn whole. */
	Near(FnWolfET_LodFraction(0, 0, 1, 0, 1, 0), 1);
	/* Otherwise detail follows the projected size, both scales and the bias. */
	Near(FnWolfET_LodFraction(0, 0.5f, 1, 0, 1, 0), 0.5f);
	Near(FnWolfET_LodFraction(0, 0.5f, 2, 0, 1, 0), 1);
	Near(FnWolfET_LodFraction(0, 0.5f, 1, 0, 0.5f, 0), 0.25f);
	Near(FnWolfET_LodFraction(0, 0.8f, 1, 0.4f, 1, 0), 0.7f);
	Near(FnWolfET_LodFraction(0, 0.8f, 1, 0, 1, 0.3f), 0.5f);
	/* The two flags ET carries: a forced low detail halves it, and a corpse
	 * takes four fifths. */
	Near(FnWolfET_LodFraction(FN_WOLFET_REFLAG_FORCE_LOD, 0.8f, 1, 0, 1, 0), 0.4f);
	Near(FnWolfET_LodFraction(FN_WOLFET_REFLAG_DEAD_LOD, 0.5f, 1, 0, 1, 0), 0.4f);
	Near(FnWolfET_LodFraction(FN_WOLFET_REFLAG_FORCE_LOD | FN_WOLFET_REFLAG_DEAD_LOD,
		1.0f, 1, 0, 1, 0), 0.4f);
	/* Flags this engine knows but ET does not attach to detail change nothing. */
	Near(FnWolfET_LodFraction(1u | 16u, 0.5f, 1, 0, 1, 0), 0.5f);
	/* Results stay inside zero and one, and a non-finite input draws whole. */
	Near(FnWolfET_LodFraction(0, 2.0f, 4, 0, 1, 0), 1);
	Near(FnWolfET_LodFraction(0, 0.1f, 1, 4, 1, 0), 0);
	Near(FnWolfET_LodFraction(0, (float)(1.0/0.0), 1, 0, 1, 0), 1);
	Near(FnWolfET_LodFraction(0, 0.5f, (float)(1.0/0.0), 0, 1, 0), 1);

	/* The surface's own minimum is honoured, unless the mesh is a corpse. */
	surface = Surface(100, 40);
	assert(FnWolfET_LodVertexCount(&surface, 0.1f, 0) == 40);
	assert(FnWolfET_LodVertexCount(&surface, 0.6f, 0) == 60);
	assert(FnWolfET_LodVertexCount(&surface, 1.0f, 0) == 100);
	/* A corpse may fall below it, but never below ET's 35% floor. */
	assert(FnWolfET_LodVertexCount(&surface, 0.1f, 1) == 35);
	assert(FnWolfET_LodVertexCount(&surface, 0.5f, 1) == 50);
	/* Nonsense never returns more vertices than the surface holds. */
	assert(FnWolfET_LodVertexCount(&surface, 4.0f, 0) == 100);
	assert(FnWolfET_LodVertexCount(&surface, (float)(1.0/0.0), 0) == 100);
	assert(FnWolfET_LodVertexCount(NULL, 1.0f, 0) == 0);

	/* Every vertex past the retained count folds onto an earlier one, following
	 * the chain all the way back. */
	memset(collapseBytes, 0, sizeof(collapseBytes));
	for (i = 0; i < 8; ++i) PutU(collapseBytes + i * 4, i ? i - 1 : 0);
	surface = Surface(8, 0);
	assert(FnWolfET_CollapseVertices(&surface, 8, map, 16));
	for (i = 0; i < 8; ++i) assert(map[i] == i);
	assert(FnWolfET_CollapseVertices(&surface, 3, map, 16));
	for (i = 0; i < 3; ++i) assert(map[i] == i);
	for (i = 3; i < 8; ++i) assert(map[i] == 2);
	/* A map entry that does not point backwards is refused. */
	PutU(collapseBytes + 5 * 4, 5);
	assert(!FnWolfET_CollapseVertices(&surface, 3, map, 16));
	PutU(collapseBytes + 5 * 4, 6);
	assert(!FnWolfET_CollapseVertices(&surface, 3, map, 16));
	PutU(collapseBytes + 5 * 4, 4);
	/* So are a zero retained count, one past the surface, and a small buffer. */
	assert(!FnWolfET_CollapseVertices(&surface, 0, map, 16));
	assert(!FnWolfET_CollapseVertices(&surface, 9, map, 16));
	assert(!FnWolfET_CollapseVertices(&surface, 3, map, 4));

	/* A tag hangs off a bone with its own axis and offset, so both compose. */
	{
		unsigned char meshBytes[FN_WOLFET_MDM_HEADER_BYTES + FN_WOLFET_MDM_TAG_BYTES];
		fnWolfETMeshView_t mesh;
		fnWolfETPose_t pose;
		float origin[3], axis[3][3];
		int row, column;
		unsigned char *tag = meshBytes + FN_WOLFET_MDM_HEADER_BYTES;
		memset(meshBytes, 0, sizeof(meshBytes));
		PutU(meshBytes, (uint32_t)FN_WOLFET_MDM_IDENT);
		PutU(meshBytes + 4, FN_WOLFET_MDM_VERSION);
		PutU(meshBytes + 88, 1);
		PutU(meshBytes + 92, FN_WOLFET_MDM_HEADER_BYTES);
		PutU(meshBytes + 96, (uint32_t)sizeof(meshBytes));
		memcpy(tag, "muzzle", 7);
		/* The tag's own axis is a quarter turn about z. */
		{ float v = 0; memcpy(tag + 64, &v, 4); v = 1; memcpy(tag + 68, &v, 4); }
		{ float v = -1; memcpy(tag + 76, &v, 4); v = 0; memcpy(tag + 80, &v, 4); }
		{ float v = 1; memcpy(tag + 96, &v, 4); }
		PutU(tag + 100, 0);                       /* bone 0 */
		{ float v = 5; memcpy(tag + 104, &v, 4); }  /* offset (5,0,0) */
		PutU(tag + 124, FN_WOLFET_MDM_TAG_BYTES);
		assert(FnWolfET_Mesh(meshBytes, sizeof(meshBytes), &mesh));
		assert(FnWolfET_FindTag(&mesh, "muzzle", 0) == 0);
		assert(FnWolfET_FindTag(&mesh, "muzzle", 1) == -1);
		assert(FnWolfET_FindTag(&mesh, "hand", 0) == -1);

		memset(&pose, 0, sizeof(pose));
		pose.numBones = 1;
		pose.bones[0].origin[0] = 10;
		pose.bones[0].origin[1] = 20;
		for (row = 0; row < 3; ++row) pose.bones[0].axis[row][row] = 1;
		assert(FnWolfET_TagOrientation(&mesh, 0, &pose, origin, axis));
		Near(origin[0], 15); Near(origin[1], 20); Near(origin[2], 0);
		Near(axis[0][1], 1); Near(axis[1][0], -1); Near(axis[2][2], 1);
		for (row = 0; row < 3; ++row) for (column = 0; column < 3; ++column)
			assert(isfinite(axis[row][column]));
		/* A tag naming a bone the pose does not hold is refused. */
		pose.numBones = 0;
		assert(!FnWolfET_TagOrientation(&mesh, 0, &pose, origin, axis));
		pose.numBones = 1;
		assert(!FnWolfET_TagOrientation(&mesh, 1, &pose, origin, axis));
	}
	return 0;
}
'''


def fixture():
    source = PRELUDE
    math = (ROOT/'code/qcommon/q_math.c').read_text(encoding='utf-8')
    for signature in ('void AngleVectors(', 'void AnglesToAxis('):
        source += function(math, signature)
    return source + BODY


class WolfETLodTests(unittest.TestCase):
    def test_detail_collapse_and_tags(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-lod'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'lod.cpp'
            executable = folder/('lod.exe' if os.name == 'nt' else 'lod')
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

    def test_et_reads_its_own_two_flags(self):
        """ET has one reading of the forced-detail bit; the other Wolfenstein
        profile has two, and neither belongs here."""
        text = LOD.read_text(encoding='utf-8')
        self.assertIn('FN_WOLFET_REFLAG_FORCE_LOD 8u', text)
        self.assertIn('FN_WOLFET_REFLAG_DEAD_LOD 32u', text)
        self.assertNotIn('COM_GAME_RTCW', text)

    def test_a_tag_composes_with_its_bone(self):
        text = POSE.read_text(encoding='utf-8')
        self.assertIn('FnWolfET_TagOrientation', text)
        self.assertIn('tag.bone >= pose->numBones', text)

    def test_the_record_states_what_is_not_claimed(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('Nothing is drawn', record)


if __name__ == '__main__':
    unittest.main()
