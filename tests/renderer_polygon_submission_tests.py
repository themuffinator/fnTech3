"""Execute all three renderer polygon queues with guarded, bounded buffers."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_types.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <memory>
struct fog_t { vec3_t bounds[2]; };
struct world_t { int numfogs; const fog_t *fogs; };
struct srfPoly_t { int surfaceType, hShader, numVerts; polyVert_t *verts; int fogIndex; };
struct Storage { unsigned before; srfPoly_t polys[2]; polyVert_t polyVerts[6]; unsigned after; };
static Storage storage, *backEndData = &storage;
static struct { qboolean registered; const world_t *world; } tr;
static int r_numpolys, r_numpolyverts, max_polys, max_polyverts;
enum { SF_POLY = 37 };
static void QDECL Print(int, const char *, ...) {}
static struct { decltype(&Print) Printf; } ri{Print};
void AddPointToBounds(const vec3_t point, vec3_t mins, vec3_t maxs) {
    for (int i = 0; i < 3; ++i) { mins[i] = std::min(mins[i], point[i]); maxs[i] = std::max(maxs[i], point[i]); }
}
static void Reset() {
    std::memset(&storage, 0, sizeof(storage)); storage.before = 0x12345678; storage.after = 0xaabbccdd;
    r_numpolys = r_numpolyverts = 0; max_polys = 2; max_polyverts = 6;
    tr.registered = qtrue; tr.world = nullptr;
}
static void Guards() { assert(storage.before == 0x12345678 && storage.after == 0xaabbccdd); }
'''

CHECKS = r'''
int main() {
    auto vertices = std::make_unique<polyVert_t[]>(6);
    for (int i = 0; i < 6; ++i) {
        auto &v = vertices[i];
        for (int j = 0; j < 3; ++j) v.xyz[j] = i + j * 0.25f;
        v.st[0] = i * 0.125f; v.st[1] = i * 0.0625f; v.modulate.u32 = 0xaabbcc00 + i;
    }
    Reset();
    // There is room for exactly two triangles. A third must not read beyond
    // this six-vertex allocation, nor consume either array's guard.
    RE_AddPolyToScene(19, 3, vertices.get(), 3); Guards();
    assert(r_numpolys == 2 && r_numpolyverts == 6);
    assert(!std::memcmp(storage.polyVerts, vertices.get(), 6 * sizeof(polyVert_t)));
    for (int i = 0; i < 2; ++i) {
        const auto &p = storage.polys[i];
        assert(p.surfaceType == SF_POLY && p.hShader == 19 && p.numVerts == 3);
        assert(p.verts == storage.polyVerts + i * 3 && p.fogIndex == 0);
    }
    const auto full = storage;
    RE_AddPolyToScene(19, INT_MAX, vertices.get(), INT_MAX);
    assert(!std::memcmp(&storage, &full, sizeof(storage))); Guards();
    for (int capacity : {0, 1, 2, 3, 5, 6}) {
        Reset(); max_polyverts = capacity;
        RE_AddPolyToScene(7, 3, vertices.get(), 3); Guards();
        assert(r_numpolys == capacity / 3 && r_numpolyverts == capacity / 3 * 3);
    }
    for (int capacity : {0, 1, 2}) {
        Reset(); max_polys = capacity;
        RE_AddPolyToScene(7, 3, vertices.get(), 3); Guards();
        assert(r_numpolys == capacity && r_numpolyverts == capacity * 3);
    }
    Reset();
    RE_AddPolyToScene(7, 3, vertices.get(), 1);
    RE_AddPolyToScene(8, 3, vertices.get() + 3, 1); Guards();
    assert(r_numpolys == 2 && storage.polys[0].hShader == 7 && storage.polys[1].hShader == 8);
    assert(!std::memcmp(storage.polyVerts, vertices.get(), 6 * sizeof(polyVert_t)));
    for (int count : {0, -1, INT_MIN}) {
        Reset(); const auto initial = storage;
        RE_AddPolyToScene(7, count, vertices.get(), 1);
        RE_AddPolyToScene(7, 3, vertices.get(), count);
        assert(r_numpolys == 0 && r_numpolyverts == 0 && !std::memcmp(&storage, &initial, sizeof(storage)));
    }
    Reset(); RE_AddPolyToScene(7, 3, nullptr, 1); Guards(); assert(!r_numpolys && !r_numpolyverts);
    tr.registered = qfalse; RE_AddPolyToScene(7, 3, vertices.get(), 1); assert(!r_numpolys);
    // Exhausted/inconsistent capacity rejects before any pointer arithmetic.
    Reset(); r_numpolyverts = INT_MAX; max_polyverts = INT_MAX;
    RE_AddPolyToScene(7, INT_MAX, vertices.get(), 1); Guards(); assert(!r_numpolys);
    Reset(); max_polyverts = INT_MIN;
    RE_AddPolyToScene(7, 3, vertices.get(), 1); Guards(); assert(!r_numpolys);
    Reset(); r_numpolyverts = 6; max_polyverts = 5;
    RE_AddPolyToScene(7, 3, vertices.get(), 1); Guards(); assert(!r_numpolys);
    // Shader zero and one/two-vertex submissions retain the established
    // Q3/QL queue behavior; RTCW suppresses shader zero at its own boundary.
    for (int count : {1, 2}) {
        Reset(); RE_AddPolyToScene(0, count, vertices.get(), 1); Guards();
        assert(r_numpolys == 1 && r_numpolyverts == count && storage.polys[0].hShader == 0);
    }
    fog_t fogs[3]{};
    for (int j = 0; j < 3; ++j) {
        fogs[1].bounds[0][j] = -10; fogs[1].bounds[1][j] = 2.5f;
        fogs[2].bounds[0][j] = 3; fogs[2].bounds[1][j] = 10;
    }
    world_t world{3, fogs};
    Reset(); tr.world = &world; RE_AddPolyToScene(19, 3, vertices.get(), 2); Guards();
    assert(storage.polys[0].fogIndex == 1 && storage.polys[1].fogIndex == 2);
    world.numfogs = 1;
    Reset(); tr.world = &world; RE_AddPolyToScene(19, 3, vertices.get(), 2); Guards();
    assert(storage.polys[0].fogIndex == 0 && storage.polys[1].fogIndex == 0);
    world.numfogs = 2;
    Reset(); tr.world = &world; RE_AddPolyToScene(19, 3, vertices.get() + 3, 1); Guards();
    assert(storage.polys[0].fogIndex == 0);
}
'''


class RendererPolygonSubmissionTests(unittest.TestCase):
    def test_queue_capacity_guards_order_and_fog_in_all_renderers(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(prefix='render-polys-', dir=ROOT / '.tmp') as temp:
                source = (ROOT / 'code' / renderer / 'tr_scene.c').read_text(encoding='utf-8')
                start = source.index('void RE_AddPolyToScene(')
                body = source[start:source.index('\n}', start) + 2]
                folder = Path(temp)
                fixture = folder / 'probe.cpp'
                executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                fixture.write_text(PRELUDE + body + CHECKS, encoding='utf-8')
                build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(fixture),
                    '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
