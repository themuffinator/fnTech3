"""Run the production cgame trap branches with distinct Q3 and QL argument packs."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


class CgameUsercmdAbiTests(unittest.TestCase):
    def test_legacy_and_structured_traps_consume_their_own_arguments(self) -> None:
        compiler = next((path for name in ("clang++", "g++", "c++")
                         if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler")
        source = (ROOT / "code/client/cl_cgame.cpp").read_text(encoding="utf-8")
        setter = function(source, "static void CL_SetUserCmdValue(")
        legacy_start = source.index("\tcase CG_SETUSERCMDVALUE:")
        legacy = source[legacy_start:source.index("\tcase CG_MEMORY_REMAINING:", legacy_start)]
        structured = function(source, "static intptr_t CG_Import_Dispatch(")
        fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
struct { int cgameUserCmdValue, cgameUserCmdPrimary, cgameUserCmdFov;
         float cgameSensitivity; } cl;
static float _vmf(intptr_t value) {
    int bits = static_cast<int>(value); float result;
    std::memcpy(&result, &bits, sizeof(result)); return result;
}
static intptr_t pack(float value) {
    int bits; std::memcpy(&bits, &value, sizeof(bits)); return bits;
}
enum { CG_SETUSERCMDVALUE, CG_GETGLCONFIG, CG_GETSNAPSHOT };
#define VMF(n) _vmf(args[n])
static void CL_CopyRetailGlconfig(void *) {}
static intptr_t CL_GetRetailNativeSnapshot(int, void *) { return 0; }
''' + setter + '\nstatic intptr_t CL_CgameSystemCalls(intptr_t *args) {\nswitch(args[0]) {\n' + legacy + r'''
default: return -1;
}}
''' + structured + r'''
int main() {
    // Poison the QL-only positions so accidental reads cannot look correct.
    intptr_t q3[] = {CG_SETUSERCMDVALUE, 7, pack(0.625f), pack(3.5f), 111};
    cl = {1, 9, 120, 2.0f};
    assert(CL_CgameSystemCalls(q3) == 0);
    assert(cl.cgameUserCmdValue == 7 && cl.cgameSensitivity == 0.625f);
    assert(cl.cgameUserCmdPrimary == 0 && cl.cgameUserCmdFov == 0);
    intptr_t ql[] = {CG_SETUSERCMDVALUE, 4, 6, pack(0.875f), 103};
    assert(CG_Import_Dispatch(ql) == 0);
    assert(cl.cgameUserCmdValue == 4 && cl.cgameUserCmdPrimary == 6);
    assert(cl.cgameSensitivity == 0.875f && cl.cgameUserCmdFov == 103);
    q3[1] = 2; q3[2] = pack(1.0f);
    assert(CL_CgameSystemCalls(q3) == 0);
    assert(cl.cgameUserCmdValue == 2 && cl.cgameSensitivity == 1.0f);
    assert(cl.cgameUserCmdPrimary == 0 && cl.cgameUserCmdFov == 0);
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="cgame-usercmd-", dir=scratch) as folder:
            path = Path(folder)
            cpp = path / "trap.cpp"
            executable = path / ("trap.exe" if os.name == "nt" else "trap")
            cpp.write_text(fixture, encoding="utf-8")
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror"]
            if os.name == "nt" and "clang" in Path(compiler).name:
                command.append("--target=i686-pc-windows-msvc")
            build = subprocess.run([*command, str(cpp), "-o", str(executable)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
