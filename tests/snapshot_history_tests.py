"""Execute the production snapshot lookup with hostile and wrapped counters."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class SnapshotHistoryTests(unittest.TestCase):
    def test_production_snapshot_lookup(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        source = (ROOT / "code/client/cl_cgame.cpp").read_text(encoding="utf-8")
        start = source.index("static clSnapshot_t *CL_FindSnapshot(")
        stop = source.index("static qboolean CL_GetSnapshot(", start)
        harness = r'''
#include <cassert>
#include <climits>
#include <cstring>
#include "code/qcommon/netchan_safety.hpp"
constexpr int PACKET_BACKUP = 32, PACKET_MASK = 31, MAX_PARSE_ENTITIES = 2048;
constexpr int ERR_DROP = 1;
struct clSnapshot_t { bool valid; int messageNum, parseEntitiesNum, numEntities; };
struct { clSnapshot_t snap, snapshots[PACKET_BACKUP]; int parseEntitiesNum; } cl;
void Com_Error(int, const char *, ...) { throw 1; }
''' + source[start:stop] + r'''
void reset(int number = 100) {
    cl = {};
    cl.snap.messageNum = number;
    cl.parseEntitiesNum = 12;
    cl.snapshots[number & PACKET_MASK] = {true, number, 10, 2};
}
int main() {
    reset();
    assert(CL_FindSnapshot(100) == &cl.snapshots[100 & PACKET_MASK]);
    assert(!CL_FindSnapshot(INT_MIN));
    assert(!CL_FindSnapshot(-1));
    assert(!CL_FindSnapshot(100 - PACKET_BACKUP));
    bool dropped = false;
    try { CL_FindSnapshot(101); } catch (int) { dropped = true; }
    assert(dropped);
    auto &slot = cl.snapshots[100 & PACKET_MASK];
    slot.messageNum = 68; assert(!CL_FindSnapshot(100));
    reset(); slot.valid = false; assert(!CL_FindSnapshot(100));
    reset(); slot.numEntities = -1; assert(!CL_FindSnapshot(100));
    reset(); slot.numEntities = 3; assert(!CL_FindSnapshot(100));
    reset(); slot.parseEntitiesNum = 13; assert(!CL_FindSnapshot(100));
    reset(); cl.parseEntitiesNum = 10 + MAX_PARSE_ENTITIES;
    assert(!CL_FindSnapshot(100));
    reset(); cl.parseEntitiesNum = 10 + MAX_PARSE_ENTITIES - 1;
    assert(CL_FindSnapshot(100));
    reset(); slot.parseEntitiesNum = INT_MAX;
    cl.parseEntitiesNum = fnql::net::CounterAdd(INT_MAX, 2);
    assert(CL_FindSnapshot(100));
    reset(0);
    cl.snapshots[INT_MAX & PACKET_MASK] = {true, INT_MAX, 10, 2};
    assert(CL_FindSnapshot(INT_MAX));
    reset(INT_MAX);
    dropped = false;
    try { CL_FindSnapshot(0); } catch (int) { dropped = true; }
    assert(dropped);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / "probe.cpp"
            exe = Path(tmp) / "probe.exe"
            cpp.write_text(harness, encoding="utf-8")
            subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17",
                            "-I", str(ROOT), str(cpp), "-o", str(exe)], check=True,
                           capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
