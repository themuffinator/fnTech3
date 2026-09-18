"""Executable checks for UI ABI selection without loading a module or game."""

from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest


from compiler_support import compiler_target_args


ROOT = Path(__file__).resolve().parents[1]


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


PRELUDE = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
typedef int qboolean;
enum { qfalse, qtrue };
struct vm_t { void *dllExports; };
static vm_t legacy{};
static vm_t structured{&legacy};
static vm_t *uivm;
static int catcher, vmCalls, vmResult, translationCalls, observedFlags;
static bool q3;
static bool rtcw;
static bool stvef;
enum { FNTECH3_RTCW_UI_API_VERSION = 4, FNTECH3_STVEF_MP_UI_API_VERSION = 4 };
static bool Com_GameIsQuake3() { return q3; }
static bool Com_GameIsRTCW() { return rtcw; }
static bool Com_GameIsSTVEFMP() { return stvef; }
static int Key_GetCatcher() { return catcher; }
static int VM_Call(vm_t *vm, int count, int ordinal) {
    assert(vm == &structured && count == 0 && ordinal == UI_MENUS_ANY_VISIBLE);
    ++vmCalls;
    return vmResult;
}
static uint32_t FnTech3_Q3CvarFlagsToInternal(uint32_t flags) {
    /* The complete translator has independent ABI adapter unit coverage. */
    assert(flags == 0x2000);
    ++translationCalls;
    return 0x800;
}
static void Cvar_InfoStringBuffer(int flags, char *buffer, int length) {
    assert(buffer != nullptr && length == 4);
    observedFlags = flags;
    std::memcpy(buffer, "ok", 3);
}
#define VM_CHECKBOUNDS(vm,address,length) ((void)0)
#define VMA(index) reinterpret_cast<char *>(args[index])
'''


CHECKS = r'''
int main() {
    uivm = nullptr; catcher = KEYCATCH_UI;
    assert(CL_UIMenusAreVisible() == qfalse && vmCalls == 0);
    uivm = &legacy; catcher = 0;
    assert(CL_UIMenusAreVisible() == qfalse && vmCalls == 0);
    catcher = KEYCATCH_UI;
    assert(CL_UIMenusAreVisible() == qtrue && vmCalls == 0);
    catcher = KEYCATCH_UI << 1;
    assert(CL_UIMenusAreVisible() == qfalse && vmCalls == 0);
    uivm = &structured; vmResult = 0; catcher = KEYCATCH_UI;
    assert(CL_UIMenusAreVisible() == qfalse && vmCalls == 1);
    vmResult = 7; catcher = 0;
    assert(CL_UIMenusAreVisible() == qtrue && vmCalls == 2);

    q3 = true;
    for (int version = 0; version <= 10; ++version) {
        assert(CL_UIApiVersionSupported(version, false) == (version == 4 || version == 6));
        assert(!CL_UIApiVersionSupported(version, true));
    }
    assert(std::strstr(CL_UIModuleLoadFailureMessage(), "Quake III Arena"));
    assert(std::strstr(CL_UIModuleLoadFailureMessage(), "vm/ui.qvm"));
    assert(!std::strstr(CL_UIModuleLoadFailureMessage(), "Quake Live"));
    q3 = false;
    for (int version = 0; version <= 10; ++version) {
        assert(CL_UIApiVersionSupported(version, false)
            == (version == 4 || version == 6 || version == 7 || version == 8));
        assert(CL_UIApiVersionSupported(version, true) == (version == 8));
    }
    assert(std::strstr(CL_UIModuleLoadFailureMessage(), "Quake Live"));
    assert(std::strstr(CL_UIModuleLoadFailureMessage(), "fnTech3"));

    rtcw = true;
    for (int version = -1; version <= 10; ++version) {
        assert(CL_UIApiVersionSupported(version, false) == (version == 4));
        assert(!CL_UIApiVersionSupported(version, true));
    }
    assert(std::strstr(CL_UIModuleLoadFailureMessage(), "RTCW"));
    assert(std::strstr(CL_UIModuleLoadFailureMessage(), "SP/MP"));
    assert(!std::strstr(CL_UIModuleLoadFailureMessage(), "Quake Live"));
    rtcw = false;

    // Holomatch takes only its own legacy UI version.
    stvef = true;
    for (int version = -1; version <= 10; ++version) {
        assert(CL_UIApiVersionSupported(version, false) == (version == 4));
        assert(!CL_UIApiVersionSupported(version, true));
    }
    assert(std::strstr(CL_UIModuleLoadFailureMessage(), "Elite Force Holomatch"));
    assert(std::strstr(CL_UIModuleLoadFailureMessage(), "vm/ui.qvm"));
    assert(!std::strstr(CL_UIModuleLoadFailureMessage(), "Quake Live"));
    stvef = false;

    char buffer[4]{};
    intptr_t args[] = {0, 0x2000, reinterpret_cast<intptr_t>(buffer), sizeof(buffer)};
    uivm = &legacy;
    assert(InfoStringTrap(args) == 0 && observedFlags == 0x800 && translationCalls == 1);
    assert(!std::strcmp(buffer, "ok"));
    uivm = &structured;
    assert(InfoStringTrap(args) == 0 && observedFlags == 0x2000 && translationCalls == 1);
    return 0;
}
'''


class UiModuleAbiTests(unittest.TestCase):
    def test_profile_and_legacy_ui_boundaries(self) -> None:
        compiler = next((path for name in ("clang++", "g++", "c++")
            if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler (clang++, g++, or c++)")
        source = (ROOT / "code/client/cl_ui.cpp").read_text(encoding="utf-8")
        public = (ROOT / "code/ui/ui_public.h").read_text(encoding="utf-8")
        shared = (ROOT / "code/qcommon/q_shared.h").read_text(encoding="utf-8")
        constants = "\n".join(re.findall(
            r"^#define\s+UI_(?:Q3_|QL_|OLD_)?API_VERSION\s+\d+", public + source, re.M))
        constants += "\n" + re.search(r"^#define\s+KEYCATCH_UI\s+\S+", shared, re.M)[0]
        constants += "\n" + re.search(r"typedef enum\s*{[^{}]*}\s*uiExport_t;", public, re.S)[0]
        extracted = "\n".join(function(source, signature) for signature in (
            "qboolean CL_UIMenusAreVisible",
            "static bool CL_UIApiVersionSupported",
            "static const char *CL_UIModuleLoadFailureMessage",
        ))
        info_case = source.split("\tcase UI_CVAR_INFOSTRINGBUFFER:", 1)[1].split("\tcase UI_ARGC:", 1)[0]
        extracted += "\nstatic int InfoStringTrap(intptr_t *args) {\n" + info_case + "\n}\n"
        init = function(source, "void CL_InitUI( void )")
        self.assertIn("CL_UIApiVersionSupported( v, uivm->dllExports != nullptr )", init)
        self.assertIn("CL_UIModuleLoadFailureMessage()", init)
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="ui-module-abi-", dir=scratch) as folder:
            path = Path(folder)
            fixture = path / "ui.cpp"
            executable = path / ("ui.exe" if os.name == "nt" else "ui")
            fixture.write_text(constants + PRELUDE + extracted + CHECKS, encoding="utf-8")
            build = subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17", "-Wall", "-Wextra",
                "-Werror", str(fixture), "-o", str(executable)],
                text=True, capture_output=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(executable)], text=True,
                capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
