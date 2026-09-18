"""Exercise production entity-history parsing across signed counter wrap."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class EntityHistoryWrapTests(unittest.TestCase):
    def test_production_entity_parser_wrap(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        source = (ROOT / "code/client/cl_parse.cpp").read_text(encoding="utf-8")
        start = source.index("static void CL_DeltaEntity(")
        end = source.index("\n\n/*", source.index("static void CL_ParsePacketEntities("))
        harness = r'''
#include <cassert>
#include <climits>
#include "code/qcommon/netchan_safety.hpp"
constexpr int MAX_PARSE_ENTITIES = 8, MAX_GENTITIES = 16, ERR_DROP = 1;
using qboolean = int;
constexpr int qtrue = 1, qfalse = 0;
struct entityState_t { int number, value; };
struct clSnapshot_t { int parseEntitiesNum, numEntities, serverTime; };
struct msg_t { int readcount = 0; int numbers[4] = {}; bool remove = false; };
struct { int parseEntitiesNum; entityState_t parseEntities[8], entityBaselines[16]; } cl;
struct cvar_t { int integer; } shownet;
cvar_t *cl_shownet = &shownet;
void Com_Error(int, const char *, ...) { throw 1; }
void Com_Printf(const char *, ...) {}
// RTCW MP's server-requested culling is profile-gated and covered separately.
void CL_RTCWApplyOnlyVisibleClients(entityState_t *, int) {}
int MSG_ReadEntitynum(msg_t *msg) { return msg->numbers[msg->readcount++]; }
void CL_ReadDeltaEntity(msg_t *msg, const entityState_t *, entityState_t *out, int number) {
    *out = {msg->remove ? MAX_GENTITIES - 1 : number, 99};
}
''' + source[start:end] + r'''
int main() {
    cl = {};
    msg_t msg;
    clSnapshot_t frame{};
    entityState_t old{1, 42};
    cl.parseEntitiesNum = INT_MAX;
    CL_DeltaEntity(&msg, &frame, 1, &old, qtrue);
    assert(cl.parseEntitiesNum == INT_MIN && frame.numEntities == 1);
    assert(cl.parseEntities[7].number == 1 && cl.parseEntities[7].value == 42);
    msg.remove = true;
    CL_DeltaEntity(&msg, &frame, 1, &old, qfalse);
    assert(cl.parseEntitiesNum == INT_MIN && frame.numEntities == 1);

    // The second old entity is across INT_MAX; unchanged and delta paths
    // must both use its proper ring slot. New output occupies separate slots.
    cl = {};
    clSnapshot_t previous{INT_MAX, 2}, next{};
    cl.parseEntitiesNum = INT_MIN + 1;
    cl.parseEntities[7] = {1, 41};
    cl.parseEntities[0] = {2, 42};
    msg = {}; msg.numbers[0] = 2; msg.numbers[1] = MAX_GENTITIES - 1;
    CL_ParsePacketEntities(&msg, &previous, &next);
    assert(next.parseEntitiesNum == INT_MIN + 1 && next.numEntities == 2);
    assert(cl.parseEntitiesNum == INT_MIN + 3);
    assert(cl.parseEntities[1].number == 1 && cl.parseEntities[1].value == 41);
    assert(cl.parseEntities[2].number == 2 && cl.parseEntities[2].value == 99);

    // An empty delta preserves all old entities through the final copy loop.
    cl.parseEntitiesNum = INT_MIN + 3;
    msg = {}; msg.numbers[0] = MAX_GENTITIES - 1;
    CL_ParsePacketEntities(&msg, &previous, &next);
    assert(next.numEntities == 2 && cl.parseEntitiesNum == INT_MIN + 5);
    assert(cl.parseEntities[3].value == 41 && cl.parseEntities[4].value == 42);
}
'''
        demo = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        packet_index = re.search(r"newent = &cl\.parseEntities\[(.*?)\];", demo).group(1)
        saved_index = re.search(r"saved_ents\[ i \] = cl\.parseEntities\[(.*?)\];", demo).group(1)
        checks = """
    clSnapshot_t demoFrame{INT_MAX, 0};
    auto *to = &demoFrame;
    auto *snap = &demoFrame;
    int newindex = 1, i = 1;
"""
        checks += f"assert(({packet_index}) == 0);\nassert(({saved_index}) == 0);\n"
        harness = harness.replace("int main() {", "int main() {" + checks)
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / "probe.cpp"
            exe = Path(tmp) / "probe.exe"
            cpp.write_text(harness, encoding="utf-8")
            subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17",
                            "-I", str(ROOT), str(cpp), "-o", str(exe)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
