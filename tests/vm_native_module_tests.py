"""Exercise the actual native loader with controlled symbol and failure shapes.

The small extracted C translation unit never loads a DLL or launches a game.
It catches dangling native pointers that could corrupt a subsequent QVM load.
"""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


PRELUDE = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "code/qcommon/game_manager.h"
#define QDECL
#define ARCH_STRING "test"
#define DLL_EXT ".so"
#define MAX_QPATH 64
#define idx64 0
#define Com_sprintf snprintf
#define Com_Memcpy memcpy
#define Com_Memset memset
typedef int qboolean;
enum { qfalse, qtrue };
typedef int vmIndex_t;
typedef intptr_t (*vmMainFunc_t)(int, ...);
typedef intptr_t (*dllSyscall_t)(intptr_t, ...);
typedef void (*dllEntry_t)(dllSyscall_t);
typedef void (*dllEntryNative_t)(void **, void *, int *);
typedef enum {
    VM_DLL_ENTRY_INVALID, VM_DLL_ENTRY_LEGACY, VM_DLL_ENTRY_STRUCTURED
} vmDllEntryAbi_t;
static comGameProfile_t profile;
static int library, exports, imports;
static int absent, missingEntry, hasMain, publishes;
static int legacyCalls, structuredCalls, unloads;
static int opens, selectedCandidate, pinnedExpected;
static const char *candidates[] = { "qagametest.so", NULL, NULL, NULL, NULL };
static vmMainFunc_t *observedMain;
static void **observedExports;
static int *observedApi;
const comGameProfile_t *Com_GameProfile(void) { return &profile; }
int Com_GameManagerNativeModuleCandidate(const comGameProfile_t *p, const char *module,
    const char *arch, const char *ext, unsigned candidate, char *out, unsigned size, int *root) {
    assert(p == &profile && !strcmp(module, "qagame"));
    assert(!strcmp(arch, ARCH_STRING) && !strcmp(ext, DLL_EXT));
    if (candidate >= 5 || !candidates[candidate]) return 0;
    assert(strlen(candidates[candidate]) < size);
    memcpy(out, candidates[candidate], strlen(candidates[candidate]) + 1);
    *root = candidate >= 2;
    return 1;
}
int Com_GameProfileAllowsModuleApi(const comGameProfile_t *p, unsigned api) {
    return p && api && (p->allowedModuleApis & api) == api;
}
static void Com_Printf(const char *format, ...) { (void)format; }
static intptr_t Main(int command, ...) { return command; }
static intptr_t Syscall(intptr_t command, ...) { return command; }
static void CheckUnpublished(void) {
    assert(*observedMain == NULL);
    assert(*observedExports == NULL);
    assert(*observedApi == 0);
}
static void Legacy(dllSyscall_t syscall) {
    CheckUnpublished();
    assert(syscall == Syscall);
    ++legacyCalls;
}
static void Structured(void **out, void *in, int *api) {
    CheckUnpublished();
    assert(in == &imports);
    ++structuredCalls;
    *out = publishes ? &exports : NULL;
    *api = 10;
    CheckUnpublished();
}
static void *VM_OpenNativeModule(vmIndex_t index, const char *name, qboolean pinned, qboolean root) {
    int candidate;
    assert(index == 0 && pinned == pinnedExpected);
    assert(opens < 5 && candidates[opens] && !strcmp(name, candidates[opens]));
    assert(root == (opens >= 2));
    candidate = opens++;
    return absent || candidate != selectedCandidate ? NULL : &library;
}
static void Sys_UnloadLibrary(void *handle) {
    assert(handle == &library);
    ++unloads;
}
static void *Sys_LoadFunction(void *handle, const char *name) {
    void *result = NULL;
    assert(handle == &library);
    if (!strcmp(name, "vmMain") && hasMain) {
        vmMainFunc_t symbol = Main;
        assert(sizeof(symbol) == sizeof(result));
        memcpy(&result, &symbol, sizeof(result));
    }
    if (!strcmp(name, "dllEntry") && !missingEntry) {
        if (hasMain) {
            dllEntry_t symbol = Legacy;
            assert(sizeof(symbol) == sizeof(result));
            memcpy(&result, &symbol, sizeof(result));
        } else {
            dllEntryNative_t symbol = Structured;
            assert(sizeof(symbol) == sizeof(result));
            memcpy(&result, &symbol, sizeof(result));
        }
    }
    return result;
}
'''


CHECKS = r'''
static void Scenario(unsigned allowed, int expectSuccess, int expectLegacy,
    int expectStructured, int expectUnload, int provideImports) {
    vmMainFunc_t main = Main; /* Deliberately seed stale caller state. */
    void *table = &exports;
    int api = 999;
    void *handle;
    observedMain = &main; observedExports = &table; observedApi = &api;
    legacyCalls = structuredCalls = unloads = 0;
    opens = 0;
    profile.allowedModuleApis = allowed;
    profile.displayName = "test";
    handle = VM_LoadDll(0, "qagame", &main, Syscall, &table,
        provideImports ? &imports : NULL, &api, pinnedExpected);
    assert((handle != NULL) == expectSuccess);
    assert(legacyCalls == expectLegacy);
    assert(structuredCalls == expectStructured);
    assert(unloads == expectUnload);
    if (!expectSuccess) {
        CheckUnpublished();
    } else if (expectLegacy) {
        assert(main == Main && table == NULL && api == 0);
        assert(main(42) == 42);
    } else {
        assert(main == NULL && table == &exports && api == 10);
    }
}
int main(void) {
    const unsigned q3 = COM_MODULE_API_QVM | COM_MODULE_API_LEGACY_NATIVE;
    const unsigned ql = COM_MODULE_API_STRUCTURED_NATIVE | COM_MODULE_API_LEGACY_NATIVE;
    absent = 1;
    Scenario(q3, 0, 0, 0, 0, 1); /* Missing module clears old state. */
    absent = 0; missingEntry = 1; hasMain = 1;
    Scenario(q3, 0, 0, 0, 1, 1); /* vmMain with no dllEntry must not leak. */
    missingEntry = 0;
    Scenario(0, 0, 0, 0, 1, 1); /* Profile rejection with a valid vmMain. */
    Scenario(q3, 1, 1, 0, 0, 1);
    Scenario(ql, 1, 1, 0, 0, 1);
    hasMain = 0; publishes = 1;
    Scenario(q3, 0, 0, 0, 1, 1); /* Q3 rejects structured before calling it. */
    Scenario(ql, 0, 0, 0, 1, 0); /* No imports means no structured ABI. */
    Scenario(ql, 1, 0, 1, 0, 1);
    publishes = 0;
    Scenario(ql, 0, 0, 1, 1, 1); /* Partial initialization clears API too. */
    hasMain = 1;
    candidates[0] = "qagame_sp_x86.dll"; candidates[1] = "qagamex86.dll";
    candidates[2] = candidates[0]; candidates[3] = candidates[1];
    selectedCandidate = 1;
    Scenario(COM_MODULE_API_LEGACY_NATIVE, 1, 1, 0, 0, 1);
    assert(opens == 2); /* Both ordinary and pinned loading follow manager order. */
    pinnedExpected = 1;
    Scenario(COM_MODULE_API_LEGACY_NATIVE, 1, 1, 0, 0, 1);
    assert(opens == 2);
    selectedCandidate = 0;
    Scenario(COM_MODULE_API_LEGACY_NATIVE, 1, 1, 0, 0, 1);
    assert(opens == 1);
    selectedCandidate = 2;
    Scenario(COM_MODULE_API_LEGACY_NATIVE, 1, 1, 0, 0, 1);
    assert(opens == 3); /* Both mod spellings precede the installation root. */
    selectedCandidate = 0;
    candidates[0] = "qagame_mp_x86.dll"; candidates[1] = NULL;
    Scenario(COM_MODULE_API_LEGACY_NATIVE, 1, 1, 0, 0, 1);
    candidates[0] = NULL;
    Scenario(COM_MODULE_API_LEGACY_NATIVE, 0, 0, 0, 0, 1);
    assert(opens == 0); /* Missing profile candidate cannot reach the OS loader. */
    return 0;
}
'''


class NativeModuleLoadingTests(unittest.TestCase):
    def test_native_failures_cannot_publish_dangling_vm_entry_points(self) -> None:
        compiler = next((path for name in ("clang", "gcc", "cc")
            if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C11 compiler (clang, gcc, or cc)")
        source = (ROOT / "code/qcommon/vm.c").read_text(encoding="utf-8")
        extracted = "\n".join(function(source, signature) for signature in (
            "static vmDllEntryAbi_t VM_SelectDllEntryAbi",
            "static unsigned VM_ModuleApiForDllEntryAbi",
            "static qboolean VM_AssignDllFunction",
            "static void * QDECL VM_LoadDll",
        ))
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="native-loader-", dir=scratch) as folder:
            path = Path(folder)
            fixture = path / "loader.c"
            executable = path / ("loader.exe" if os.name == "nt" else "loader")
            fixture.write_text(PRELUDE + extracted + CHECKS, encoding="utf-8")
            command = [compiler]
            if (os.name == "nt" and "clang" in Path(compiler).name.lower()
                    and os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() == "x86"):
                command.append("--target=i686-pc-windows-msvc")
            build = subprocess.run([*command, "-std=c11", "-Wall", "-Wextra",
                "-Werror", "-I", str(ROOT), str(fixture), "-o", str(executable)],
                text=True, capture_output=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(executable)], text=True,
                capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
