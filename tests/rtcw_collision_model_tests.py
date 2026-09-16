"""Real model-loader/resolver checks for Q3/QL and RTCW handle ranges."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/qcommon/cm_local.h"
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static const comGameProfile_t *selected;
const comGameProfile_t *FnFixture_GameProfile(void) { return selected; }
static jmp_buf errors;
static int allocations, used;
static union { double alignment; byte bytes[128 * 1024]; } arena;
static dmodel_t records[513];
static byte *cmod_base;
static cmodel_t box_model;
clipMap_t cm;
void QDECL Com_Error(errorParm_t level, const char *format, ...) {
    (void)level; (void)format; longjmp(errors, 1);
}
void *Hunk_Alloc(int size, ha_pref preference) {
    void *result; (void)preference;
    assert(size >= 0 && size <= (int)sizeof(arena) - used);
    result = arena.bytes + used; used += (size + 15) & ~15; ++allocations;
    return result;
}
'''


CHECKS = r'''
static void LoadCount(int count) {
    const int fits = count > 0 && count <= selected->collisionModelHandleCount - 2;
    const lump_t lump = {0, count * (int)sizeof(dmodel_t)};
    int i;
    memset(&cm, 0, sizeof(cm)); memset(&arena, 0, sizeof(arena));
    cm.leafbrushes = (int *)arena.bytes; cm.leafsurfaces = (int *)arena.bytes;
    used = 16; allocations = 0;
    if (setjmp(errors) == 0) {
        CMod_LoadSubmodels(&lump); assert(fits);
    } else {
        assert(!fits && !allocations && !cm.cmodels && !cm.numSubModels); return;
    }
    assert(cm.numSubModels == count);
    for (i = 0; i < count; ++i) {
        const cmodel_t *model = CM_ClipHandleToModel(i);
        assert(model == &cm.cmodels[i]);
        assert(model->mins[0] == -1 && model->maxs[0] == 11);
    }
    assert(CM_ClipHandleToModel(CM_TemporaryModelHandle(qfalse)) == &box_model);
    assert(CM_ClipHandleToModel(CM_TemporaryModelHandle(qtrue)) == &box_model);
    for (i = 0; i < 3; ++i) {
        const int invalid[] = {-1, INT_MIN, INT_MAX};
        if (setjmp(errors) == 0) { CM_ClipHandleToModel(invalid[i]); assert(0); }
    }
    if (!CM_IsTemporaryModelHandle(count)) {
        if (setjmp(errors) == 0) { CM_ClipHandleToModel(count); assert(0); }
    }
}
void TestModelLimits(const comGameProfile_t *profile) {
    const int counts[] = {-1,0,1,254,255,256,257,400,509,510,511,512,513};
    unsigned i;
    selected = profile; cmod_base = (byte *)records;
    for (i = 0; i < 513; ++i) {
        records[i].maxs[0] = 10; records[i].maxs[1] = 20; records[i].maxs[2] = 30;
    }
    for (i = 0; i < sizeof(counts)/sizeof(counts[0]); ++i) LoadCount(counts[i]);
    {
        const lump_t malformed = {0, sizeof(dmodel_t) - 1};
        allocations = 0;
        if (setjmp(errors) == 0) { CMod_LoadSubmodels(&malformed); assert(0); }
        assert(!allocations);
    }
}
'''


class RTCWCollisionModelTests(unittest.TestCase):
    def test_inline_models_and_temporary_hulls_never_share_a_handle(self):
        cc = shutil.which('clang')
        cxx = shutil.which('clang++')
        if not cc or not cxx:
            self.skipTest('Clang C/C++ compilers unavailable')
        body = (ROOT / 'code/qcommon/cm_load.c').read_text()
        source = PRELUDE + '\n' + function(body, 'static void CMod_LoadSubmodels(')
        source += '\n' + function(body, 'cmodel_t *CM_ClipHandleToModel(') + '\n' + CHECKS
        driver = r'''
#include "code/qcommon/game_manager.h"
extern "C" void TestModelLimits(const comGameProfile_t *);
int main() {
    for (unsigned i = 0; i < Com_GameManagerProfileCount(); ++i)
        TestModelLimits(Com_GameManagerProfileAt(i));
}
'''
        target = compiler_target_args(cxx)
        abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
        with tempfile.TemporaryDirectory() as directory:
            cfile, cpp, obj, exe = [Path(directory) / name for name in ['models.c', 'main.cpp', 'models.o', 'models.exe']]
            cfile.write_text(source, encoding='utf-8'); cpp.write_text(driver, encoding='utf-8')
            commands = [
                [cc, *target, *abi, '-std=c11', '-I', str(ROOT), '-DCom_GameProfile=FnFixture_GameProfile',
                 '-c', str(cfile), '-o', str(obj)],
                [cxx, *target, *abi, '-std=c++17', '-I', str(ROOT), str(cpp), str(obj),
                 str(ROOT / 'code/qcommon/game_profile.cpp'), '-o', str(exe)],
            ]
            for command in commands:
                run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=15, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
