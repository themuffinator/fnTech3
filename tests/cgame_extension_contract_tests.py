"""Execute cgame extension discovery and dispatch for the Q3 and QL profiles."""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from spawn_view_angle_source_tests import function_body


ROOT = Path(__file__).resolve().parents[1]


FIXTURE = r'''
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "q3_game_abi.h"

using qboolean = int;
constexpr int qtrue = 1, qfalse = 0, ERR_DROP = 1;
static bool quake3Profile = true;
static int lastCall, advertisements, errors, boundsChecks;
static const char *descriptionName, *descriptionText;
static int Com_GameIsQuake3() { return quake3Profile ? 1 : 0; }
static int Q_stricmp(const char *left, const char *right) {
    while (*left && *right && std::tolower(static_cast<unsigned char>(*left)) ==
        std::tolower(static_cast<unsigned char>(*right))) { ++left; ++right; }
    return std::tolower(static_cast<unsigned char>(*left)) -
        std::tolower(static_cast<unsigned char>(*right));
}
static void Com_sprintf(char *value, int size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::vsnprintf(value, static_cast<std::size_t>(size), format, args);
    va_end(args);
}
static void Com_Error(int, const char *, ...) { ++errors; }
static void CL_AddEnemyHighlightRefEntity(void *, int) { lastCall = 1; }
static void CL_ForceFixedDlights() { lastCall = 2; }
static void AddLinearLight(void *, void *, float, float, float, float) { lastCall = 3; }
static struct { decltype(&AddLinearLight) AddLinearLightToScene = AddLinearLight; } re;
static struct { int demorecording = 77; } clc;
static void Cvar_SetDescription2(const char *name, const char *description) {
    lastCall = 5; descriptionName = name; descriptionText = description;
}
static void CL_AdvertisementBridge_InitCGame() { ++advertisements; }
static void CL_AdvertisementBridge_ShutdownCGame() { ++advertisements; }
static void CL_AdvertisementBridge_UpdateLoadingViewParameters() { ++advertisements; }
static void CL_AdvertisementBridge_SetFrameTime(intptr_t) { ++advertisements; }
static void *VM_ArgPtr(intptr_t arg) { return reinterpret_cast<void *>(arg); }
static float VM_Float(intptr_t arg) {
    const int bits = static_cast<int>(arg);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
#define VMF(n) VM_Float(args[n])
#define VM_CHECKBOUNDS(vm, address, length) (++boundsChecks)
@IMPORT_ENUM@
@VM_ARG_PTR@
@VMA_MACRO@
@TRAP_REMAP@
@GET_VALUE@
@DISPATCH@

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
int main() {
    // Independent expected ordinals from the pinned FnQuake3 cgameImport_t.
    constexpr int q3Ordinals[] = {112, 113, 114, 115, 116};
    constexpr int qlOrdinals[] = {116, 117, 118, 119, 120};
    const char *keys[] = {"trap_R_AddRefEntityToScene2", "trap_R_ForceFixedDLights",
        "trap_R_AddLinearLightToScene_Q3E", "trap_IsRecordingDemo",
        "trap_Cvar_SetDescription_Q3E"};
    for (const bool quake3 : {true, false}) {
        quake3Profile = quake3;
        for (int index = 0; index < 5; ++index) {
            char value[32] = "unchanged";
            intptr_t query[] = {CG_TRAP_GETVALUE, reinterpret_cast<intptr_t>(value),
                sizeof(value), reinterpret_cast<intptr_t>(keys[index])};
            CHECK(CL_CgameSystemCalls(query) == qtrue);
            const int fixedOrdinal = quake3 ? q3Ordinals[index] : qlOrdinals[index];
            CHECK(std::atoi(value) == fixedOrdinal);
            for (const int trap : {fixedOrdinal, std::atoi(value)}) {
                intptr_t args[] = {trap, reinterpret_cast<intptr_t>("test_cvar"),
                    reinterpret_cast<intptr_t>("test description"), 0, 0, 0, 0};
                lastCall = 0; advertisements = 0; errors = 0;
                const intptr_t result = CL_CgameSystemCalls(args);
                CHECK(errors == 0 && advertisements == 0);
                CHECK(lastCall == (index == 3 ? 0 : index + 1));
                CHECK(result == (index == 3 ? 77 : 0));
                CHECK(args[0] == trap);
                if (index == 4) {
                    CHECK(std::strcmp(descriptionName, "test_cvar") == 0);
                    CHECK(std::strcmp(descriptionText, "test description") == 0);
                }
            }
        }
        char value[32] = "unchanged";
        CHECK(CL_GetValue(value, sizeof(value), "unsupported_extension") == qfalse);
        CHECK(std::strcmp(value, "unchanged") == 0);
        re.AddLinearLightToScene = nullptr;
        CHECK(CL_GetValue(value, sizeof(value), keys[2]) == qfalse);
        CHECK(std::strcmp(value, "unchanged") == 0);
        re.AddLinearLightToScene = AddLinearLight;
        CHECK(CL_CGameTrapForProfile(42) == 42);
        CHECK(CL_CGameTrapForProfile(CG_TRAP_GETVALUE) == CG_TRAP_GETVALUE);
    }

    quake3Profile = false;
    advertisements = 0;
    for (int trap = 112; trap <= 115; ++trap) {
        intptr_t args[] = {trap, 10};
        CHECK(CL_CgameSystemCalls(args) == 0);
    }
    CHECK(advertisements == 4);

    quake3Profile = true;
    advertisements = 0; lastCall = 0; errors = 0;
    for (int trap = 117; trap <= 120; ++trap) {
        intptr_t args[] = {trap, 0, 0, 0, 0, 0, 0};
        CL_CgameSystemCalls(args);
    }
    CHECK(errors == 4 && advertisements == 0 && lastCall == 0);
    CHECK(boundsChecks == 10);
    return 0;
}
'''


class CgameExtensionContractTests(unittest.TestCase):
    def test_fixed_and_discovered_ordinals_reach_the_same_profile_handler(self) -> None:
        compiler = shutil.which("clang++") or shutil.which("g++") or shutil.which("cl")
        if compiler is None:
            self.skipTest("A C++20 compiler is required for the cgame extension harness")
        source = (ROOT / "code/client/cl_cgame.cpp").read_text(encoding="utf-8")
        public = (ROOT / "code/cgame/cg_public.h").read_text(encoding="utf-8")
        common = (ROOT / "code/qcommon/qcommon.h").read_text(encoding="utf-8")
        end = public.index("} cgameImport_t;") + len("} cgameImport_t;")
        imports = public[public.rfind("typedef enum {", 0, end):end]
        get_value_macro = re.search(r"(?m)^#define COM_TRAP_GETVALUE .*$", common).group()
        class_start = source.index("class VmArgPtr {")
        vm_arg_ptr = source[class_start:source.index("\n};", class_start) + 3]
        vma = re.search(r"(?m)^#define VMA\(x\).*$", source).group()
        calls = function_body(source, "CL_CgameSystemCalls")
        # Compile the original function entry/switch expression and all of
        # its extension and advertisement branches. Unrelated traps are
        # excluded so this test needs no renderer, filesystem, or live VM.
        dispatch = calls[:calls.index("\tcase CG_PRINT:")]
        dispatch += calls[calls.index("\t// engine extensions"):]
        fixture = FIXTURE.replace("@IMPORT_ENUM@", get_value_macro + "\n" + imports)
        fixture = fixture.replace("@VM_ARG_PTR@", vm_arg_ptr).replace("@VMA_MACRO@", vma)
        fixture = fixture.replace("@TRAP_REMAP@", function_body(source, "CL_CGameTrapForProfile"))
        fixture = fixture.replace("@GET_VALUE@", function_body(source, "CL_GetValue"))
        fixture = fixture.replace("@DISPATCH@", dispatch)
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="cgame-extension-", dir=scratch) as folder:
            folder = Path(folder)
            cpp = folder / "extension.cpp"
            executable = folder / ("extension.exe" if os.name == "nt" else "extension")
            cpp.write_text(fixture, encoding="utf-8")
            include = str(ROOT / "code/compat")
            if Path(compiler).stem.lower() == "cl":
                command = [compiler, "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX",
                           "/I" + include, str(cpp), "/Fe:" + str(executable)]
            else:
                command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                           "-I" + include, str(cpp), "-o", str(executable)]
                if os.name == "nt" and "clang" in Path(compiler).name:
                    command.append("--target=i686-pc-windows-msvc")
            build = subprocess.run(command, cwd=folder, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
