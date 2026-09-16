"""Indexed RTCW attachments and legacy boolean tags across all renderer models."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    source = '''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/renderercommon/tr_types.h"
#include "code/qcommon/game_profile.h"
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
'''
    for pattern in (r'typedef enum\s*\{[^{}]*SF_BAD[^{}]*\}\s*surfaceType_t;',
                    r'typedef enum\s*\{[^{}]*MOD_BAD[^{}]*\}\s*modtype_t;'):
        source += re.search(pattern, local)[0] + '\n'
    source += '#include "code/renderercommon/tr_mdc_model_types.h"\n#include "code/renderercommon/tr_mds_model_types.h"\ntypedef struct bmodel_s bmodel_t;\n'
    source += re.search(r'typedef struct model_s\s*\{[^{}]*\}\s*model_t;', local)[0] + '\n'
    source += '''
static struct { comGameId_t gameId; } ri = {COM_GAME_QUAKE3};
static model_t models[3];
static int missingModel;
static model_t *R_GetModelByHandle(qhandle_t handle) {
    return missingModel ? NULL : &models[handle > 0 && handle < 3 ? handle : 0];
}
void Q_strncpyz(char *destination, const char *source, int bytes) {
    assert(bytes > 0); strncpy(destination, source, bytes - 1); destination[bytes - 1] = 0;
}
const vec3_t vec3_origin = {0, 0, 0};
'''
    math = (ROOT / 'code/qcommon/q_math.c').read_text()
    for signature in ('void AngleVectors(', 'void AnglesToAxis(', 'void AxisClear(', 'vec_t VectorNormalize('):
        source += function(math, signature)
    source += '#include "code/renderercommon/tr_mesh_tags.h"\n'
    legacy = (ROOT / 'code' / renderer / 'tr_model.c').read_text()
    source += function(legacy, 'static md3Tag_t *R_GetTag(')
    source += function(legacy, 'static md3Tag_t *R_GetAnimTag(')
    source += function((ROOT / 'code/renderercommon/tr_mdc_model.h').read_text(), 'static md3Tag_t *R_GetMDCTag(')
    source += '''
static int R_IQMLerpTag(orientation_t *tag, void *data, int a, int b, float f, const char *name) {
    assert(0); return 0;
}
'''
    return source + function(legacy, 'int R_LerpTag(')


CHECKS = r'''
typedef struct { md3Header_t header; md3Tag_t tags[2][4]; } md3Fixture_t;
static md3Fixture_t md3;
static unsigned char mdcBytes[577];
static mdcModel_t mdc;
static void Put32(unsigned char *p, uint32_t value) {
    int i; for (i = 0; i < 4; ++i) p[i] = (unsigned char)(value >> (i * 8));
}
static void Put16(unsigned char *p, int value) {
    p[0] = (unsigned char)value; p[1] = (unsigned char)((unsigned)value >> 8);
}
static void Make(void) {
    int frame, index, component;
    unsigned char *data = mdcBytes + 1;
    const char *names[4] = {"tag_repeat", "tag_other", "tag_repeat", "tag_final"};
    memset(&md3, 0, sizeof(md3)); memset(&mdc, 0, sizeof(mdc)); memset(mdcBytes, 0, sizeof(mdcBytes));
    md3.header.numFrames = 2; md3.header.numTags = 4; md3.header.ofsTags = offsetof(md3Fixture_t, tags);
    Put32(data, 0x43504449); Put32(data + 4, 2); Put32(data + 76, 2); Put32(data + 80, 4);
    Put32(data + 92, 112); Put32(data + 96, 224); Put32(data + 100, 480);
    Put32(data + 104, 576); Put32(data + 108, 576);
    for (index = 0; index < 4; ++index) {
        strcpy((char *)data + 224 + index * 64, names[index]);
        for (frame = 0; frame < 2; ++frame) {
            md3Tag_t *tag = &md3.tags[frame][index];
            unsigned char *compressed = data + 480 + (frame * 4 + index) * 12;
            strcpy(tag->name, names[index]); AxisClear(tag->axis);
            for (component = 0; component < 3; ++component) {
                tag->origin[component] = frame * 10.0f + index * 3 + component;
                Put16(compressed + component * 2, (int)(tag->origin[component] * 64));
            }
        }
    }
    assert(FnMdc_Open(data, 576, &mdc.view));
    models[1].type = MOD_MESH; models[1].md3[0] = &md3.header;
    models[2].type = MOD_MDC; models[2].mdc[0] = &mdc;
}
static void Identity(const orientation_t *tag) {
    int row, column;
    for (row = 0; row < 3; ++row) {
        assert(tag->origin[row] == 0);
        for (column = 0; column < 3; ++column) assert(tag->axis[row][column] == (row == column ? 1.0f : 0));
    }
}
static void Near(float actual, float expected) { assert(fabsf(actual - expected) < 0.000001f); }
int main(void) {
    struct { unsigned before; orientation_t value; unsigned after; } out;
    refEntity_t entity;
    orientation_t expected;
    int handle, first, component;
    memset(&entity, 0, sizeof(entity)); out.before = 0x11223344; out.after = 0x55667788;
    Make(); entity.oldframe = 0; entity.frame = 1; entity.backlerp = 0.25f;
    for (handle = 1; handle <= 2; ++handle) {
        entity.hModel = handle;
        for (first = -2; first <= 4; ++first) {
            const int index = first <= 0 ? 0 : first <= 2 ? 2 : -1;
            assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", first) == index);
            if (index < 0) Identity(&out.value);
            else for (component = 0; component < 3; ++component) Near(out.value.origin[component], index * 3 + component + 7.5f);
        }
        for (first = 0; first < 2; ++first) {
            const int extreme = first ? INT_MAX : INT_MIN;
            assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", extreme) == (first ? -1 : 0));
            entity.frame = entity.oldframe = extreme;
            assert(R_LerpTagEntity(&out.value, &entity, "tag_final", 0) == 3);
            Near(out.value.origin[0], first ? 19.0f : 9.0f);
            entity.oldframe = 0; entity.frame = 1;
        }
        assert(R_LerpTagEntity(&out.value, &entity, "tag_missing", 0) == -1); Identity(&out.value);
        assert(R_LerpTagEntity(&out.value, &entity, "Tag_repeat", 0) == -1); Identity(&out.value);
        assert(R_LerpTagEntity(&out.value, &entity, "", 0) == -1); Identity(&out.value);
        entity.backlerp = -0.5f;
        assert(R_LerpTagEntity(&out.value, &entity, "tag_final", 0) == 3); Near(out.value.origin[0], 24);
        entity.backlerp = 1.5f;
        assert(R_LerpTagEntity(&out.value, &entity, "tag_final", 0) == 3); Near(out.value.origin[0], 4);
        entity.backlerp = 0x1p-26f;
        assert(R_LerpTagEntity(&out.value, &entity, "tag_final", 0) == 3);
        assert(out.value.origin[0] == 19.0f); // Both rounded fractions matter.
        entity.backlerp = 0.25f;
        assert(R_LerpTag(&out.value, handle, 0, 1, 0.75f, "tag_final") == qtrue);
        Near(out.value.origin[0], 16.5f);
        assert(R_LerpTag(&out.value, handle, 0, 1, 0.75f, "absent") == qfalse); Identity(&out.value);
    }
    // MD3 names belong to each frame; returning the current match need not
    // return the index that supplied the old orientation.
    entity.hModel = 1;
    strcpy(md3.tags[1][0].name, "different");
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == 2); Near(out.value.origin[0], 12);
    strcpy(md3.tags[0][0].name, "different"); strcpy(md3.tags[0][2].name, "different");
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    Make(); strcpy(md3.tags[1][0].name, "different"); strcpy(md3.tags[1][2].name, "different");
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    Make();
    {
        char longName[100]; memset(longName, 'x', sizeof(longName)); longName[99] = 0;
        memset(md3.tags[0][3].name, 'x', 63); md3.tags[0][3].name[63] = 0;
        memcpy(md3.tags[1][3].name, md3.tags[0][3].name, 64);
        assert(R_LerpTagEntity(&out.value, &entity, longName, 0) == 3);
    }
    Make();
    md3.tags[1][0].axis[0][0] = 0; md3.tags[1][0].axis[0][1] = 1;
    md3.tags[1][0].axis[1][0] = -1; md3.tags[1][0].axis[1][1] = 0;
    Put16(mdcBytes + 1 + 480 + 4 * 12 + 8, 8175); // 90-degree yaw.
    entity.backlerp = 0.5f;
    for (handle = 1; handle <= 2; ++handle) {
        entity.hModel = handle;
        assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == 0);
        Near(out.value.axis[0][0], 0.70710678f); Near(out.value.axis[0][1], 0.70710678f);
        Near(out.value.axis[1][0], -0.70710678f); Near(out.value.axis[1][1], 0.70710678f);
        expected = out.value;
        {
            union { char name[64]; orientation_t tag; } alias;
            strcpy(alias.name, "tag_repeat");
            assert(R_LerpTagEntity(&alias.tag, &entity, alias.name, 0) == 0);
            assert(!memcmp(&alias.tag, &expected, sizeof(expected)));
        }
        {
            refEntity_t alias = entity;
            assert(R_LerpTagEntity((orientation_t *)&alias, &alias, "tag_repeat", 0) == 0);
            assert(!memcmp(&alias, &expected, sizeof(expected)));
        }
    }
    {
        const float invalid[] = {NAN, INFINITY, -INFINITY, FLT_MAX};
        for (first = 0; first < 4; ++first) {
            entity.backlerp = invalid[first];
            assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
        }
    }
    entity.backlerp = 0.5f; entity.hModel = 1;
    md3.tags[1][0].origin[0] = INFINITY;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    Make(); md3.tags[1][0].axis[0][0] = FLT_MAX;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    Make(); memset(md3.tags[0][0].axis, 0, sizeof(md3.tags[0][0].axis)); memset(md3.tags[1][0].axis, 0, sizeof(md3.tags[1][0].axis));
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == 0 && out.value.axis[0][0] == 0);
    md3.header.numFrames = 0;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    Make(); md3.header.numTags = 0;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    Make(); models[1].md3[0] = NULL;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    entity.hModel = 2; mdc.view.numFrames = 0;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    models[2].mdc[0] = NULL;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    entity.hModel = INT_MAX;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    missingModel = 1;
    assert(R_LerpTagEntity(&out.value, &entity, "tag_repeat", 0) == -1); Identity(&out.value);
    assert(R_LerpTagEntity(&out.value, NULL, "tag_repeat", 0) == -1); Identity(&out.value);
    assert(R_LerpTagEntity(&out.value, &entity, NULL, 0) == -1); Identity(&out.value);
    assert(R_LerpTagEntity(NULL, &entity, "tag_repeat", 0) == -1);
    assert(out.before == 0x11223344 && out.after == 0x55667788);
    return 0;
}
'''


class MeshTagTests(unittest.TestCase):
    def test_indexed_and_boolean_attachments(self):
        for language in ('c', 'c++'):
            compiler = shutil.which('clang' if language == 'c' else 'clang++')
            if not compiler:
                self.skipTest('Clang C/C++ compiler unavailable')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            for renderer in ('renderer', 'renderervk', 'rendererrtx'):
                with self.subTest(language=language, renderer=renderer), tempfile.TemporaryDirectory(prefix='mesh-tags-', dir=ROOT / '.tmp') as temp:
                    path = Path(temp)
                    source = path / ('probe.c' if language == 'c' else 'probe.cpp')
                    executable = path / ('probe.exe' if os.name == 'nt' else 'probe')
                    source.write_text(fixture(renderer) + CHECKS)
                    command = [compiler, *target, '-std=' + ('c11' if language == 'c' else 'c++17'),
                               '-I', str(ROOT), str(source), '-o', str(executable)]
                    if os.name != 'nt':
                        command.append('-lm')
                    build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
