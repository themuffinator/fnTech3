"""Native module root lookup and file ownership, without loading game code."""
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
    return source[start:source.index("\n}", start) + 2]


PATH_CHECKS = r'''
#include "game_manager.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>
int main(int argc, char **argv) {
    assert(argc == 4 || argc == 5);
    const auto *sp = Com_GameProfileById(COM_GAME_RTCW_SP);
    const auto *profile = Com_GameManagerFindProfile(argv[3]);
    const bool unicode = argc == 5 && !std::strcmp(argv[4], "unicode");
    const bool outside = argc == 5 && !std::strcmp(argv[4], "outside");
    const bool inside = argc == 5 && !std::strcmp(argv[4], "inside");
    const std::string root = std::string(argv[1]) + (unicode ? "/caf\xc3\xa9" : ""), name = argv[2];
    char output[4096];
    std::memset(output, 0x5a, sizeof(output));
    const bool allowed = profile == sp && (name == "cgamex86.dll" || name == "cgame_sp_x86.dll");
    const auto base = std::filesystem::canonical(std::filesystem::u8path(root));
    std::error_code error;
    const auto path = std::filesystem::canonical(base / (inside ? "cgame_sp_x86.dll" : name), error);
    const bool exists = !error && path.parent_path() == base && std::filesystem::is_regular_file(path);
    // The fixture knows where each link points. MinGW canonical() must not
    // serve as the oracle for the very containment failure being tested.
    const bool expected = allowed && exists && !outside;
    const int result = Com_GameManagerResolveRootModule(profile, root.c_str(), name.c_str(),
        "x86", ".dll", output, sizeof(output));
    assert(result == expected);
    if (expected) {
        assert(std::filesystem::u8path(output) == path);
        const unsigned length = (unsigned)std::strlen(output);
        for (unsigned size : {0u, 1u, length}) {
            std::memset(output, 0x5a, sizeof(output));
            assert(!Com_GameManagerResolveRootModule(profile, root.c_str(), name.c_str(),
                "x86", ".dll", output, size));
            for (char byte : output) assert(byte == 0x5a);
        }
        assert(Com_GameManagerResolveRootModule(profile, root.c_str(), name.c_str(),
            "x86", ".dll", output, length + 1));
        assert(output[length + 1] == 0x5a);
    } else {
        for (char byte : output) assert(byte == 0x5a);
    }
    assert(!Com_GameManagerResolveRootModule(sp, nullptr, name.c_str(), "x86", ".dll", output, sizeof(output)));
    assert(!Com_GameManagerResolveRootModule(sp, "", name.c_str(), "x86", ".dll", output, sizeof(output)));
    assert(!Com_GameManagerResolveRootModule(sp, root.c_str(), nullptr, "x86", ".dll", output, sizeof(output)));
    assert(!Com_GameManagerResolveRootModule(sp, root.c_str(), name.c_str(), "x86", ".dll", nullptr, sizeof(output)));
    assert(!Com_GameManagerResolveRootModule(sp, root.c_str(), name.c_str(), "x86_64", ".dll", output, sizeof(output)));
    const std::string huge(40000, 'x');
    assert(!Com_GameManagerResolveRootModule(sp, huge.c_str(), name.c_str(), "x86", ".dll", output, sizeof(output)));
    const std::string invalid = root + "/\xff\xfe";
    assert(!Com_GameManagerResolveRootModule(sp, invalid.c_str(), name.c_str(), "x86", ".dll", output, sizeof(output)));
    comGameProfile_t forged = *sp;
    assert(!Com_GameManagerResolveRootModule(&forged, root.c_str(), name.c_str(), "x86", ".dll", output, sizeof(output)));
}
'''


READ_PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS
#include "game_manager.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_OSPATH 256
#define ARCH_STRING "x86"
#define DLL_EXT ".dll"
typedef int qboolean;
enum { qfalse, qtrue };
typedef unsigned char byte;
typedef struct { const char *string; } cvar_t;
static cvar_t base;
static cvar_t *fs_basepath = &base;
static int fs_loadCount, fs_loadStack, allocations, frees, opened, closed;
static int vfsPresent, rootAllowed = 1, shortRead, extraByte, closeError;
static long lengthOverride;
static const comGameProfile_t *selected;
static const char *rootModule;
static const comGameProfile_t *ActiveProfile(void) { return selected; }
#define Com_GameProfile ActiveProfile
static void *Hunk_AllocateTempMemory(int size) {
    assert(size > 0 && size < 100);
    ++allocations;
    return malloc((size_t)size);
}
static void Hunk_FreeTempMemory(void *data) { assert(data); ++frees; free(data); }
static int FS_ReadFile(const char *filename, void **buffer) {
    assert(filename && filename[0]);
    if (!vfsPresent) { if (buffer) *buffer = NULL; return -1; }
    if (buffer) {
        *buffer = Hunk_AllocateTempMemory(4);
        memcpy(*buffer, "pak", 4);
        ++fs_loadStack; ++fs_loadCount;
    }
    return 3;
}
static int Resolve(const comGameProfile_t *p, const char *root, const char *name,
    const char *arch, const char *ext, char *out, unsigned size) {
    assert(root == base.string);
    assert(!strcmp(arch, ARCH_STRING) && !strcmp(ext, DLL_EXT));
    if (!rootAllowed || p != Com_GameProfileById(COM_GAME_RTCW_SP) || strcmp(name, "cgamex86.dll")) return 0;
    assert(strlen(rootModule) < size);
    memcpy(out, rootModule, strlen(rootModule) + 1);
    return 1;
}
#define Com_GameManagerResolveRootModule Resolve
static FILE *Sys_FOpen(const char *path, const char *mode) {
    assert(!strcmp(mode, "rb"));
    FILE *file = fopen(path, mode);
    if (file) ++opened;
    return file;
}
static size_t Read(void *out, size_t width, size_t count, FILE *file) {
    return fread(out, width, count - (shortRead ? 1 : 0), file);
}
static int Getc(FILE *file) { return extraByte ? 'x' : fgetc(file); }
static long Tell(FILE *file) { return lengthOverride ? lengthOverride : ftell(file); }
static int Close(FILE *file) { ++closed; int result = fclose(file); return closeError ? EOF : result; }
#define fread Read
#define fgetc Getc
#define ftell Tell
#define fclose Close
'''

READ_CHECKS = r'''
static void Release(void *buffer) {
    assert(fs_loadStack == 1);
    --fs_loadStack;
    Hunk_FreeTempMemory(buffer);
}
static void Missing(const char *name) {
    void *buffer = (void *)1;
    const int count = fs_loadCount;
    assert(FS_ReadNativeModuleFile(name, &buffer, qtrue) == -1 && !buffer);
    assert(fs_loadCount == count && fs_loadStack == 0);
    assert(allocations == frees && opened == closed);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    rootModule = argv[1]; base.string = "selected-install";
    selected = Com_GameProfileById(COM_GAME_RTCW_SP);
    void *buffer;
    Missing(NULL); Missing("");
    assert(!opened);
    rootAllowed = 0;
    Missing("cgamex86.dll");
    rootAllowed = 1;
    for (int p = COM_GAME_QUAKE3; p < COM_GAME_COUNT; ++p) {
        selected = Com_GameProfileById((comGameId_t)p);
        vfsPresent = 1;
        assert(FS_ReadNativeModuleFile("cgamex86.dll", &buffer, qfalse) == 3);
        assert(!strcmp((char *)buffer, "pak") && !opened);
        Release(buffer);
        assert(FS_ReadNativeModuleFile("cgamex86.dll", NULL, qfalse) == 3);
        vfsPresent = 0;
        if (p != COM_GAME_RTCW_SP) Missing("cgamex86.dll");
    }
    selected = Com_GameProfileById(COM_GAME_RTCW_SP);
    fs_basepath = NULL;
    Missing("cgamex86.dll");
    fs_basepath = &base;
    Missing("../cgamex86.dll"); Missing("cgame_mp_x86.dll"); Missing("default.cfg");
    assert(FS_ReadNativeModuleFile("cgamex86.dll", &buffer, qtrue) == 6);
    assert(!memcmp(buffer, "retail\0", 7));
    Release(buffer);
    const int count = fs_loadCount;
    assert(FS_ReadNativeModuleFile("cgamex86.dll", NULL, qtrue) == 6);
    assert(fs_loadCount == count && !fs_loadStack);
    shortRead = 1; Missing("cgamex86.dll"); shortRead = 0;
    extraByte = 1; Missing("cgamex86.dll"); extraByte = 0;
    closeError = 1; Missing("cgamex86.dll"); closeError = 0;
    lengthOverride = INT_MAX; Missing("cgamex86.dll");
    lengthOverride = -1; Missing("cgamex86.dll"); lengthOverride = 0;
    rootModule = "file-does-not-exist"; Missing("cgamex86.dll");
    assert(allocations == frees && opened == closed);
}
'''


class NativeModuleFilesystemTests(unittest.TestCase):
    def compiler(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("requires a C++17 compiler")
        return compiler

    def build(self, compiler, folder, source):
        fixture = folder / "probe.cpp"
        executable = folder / ("probe.exe" if os.name == "nt" else "probe")
        fixture.write_text(source, encoding="utf-8")
        result = subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17",
            "-I", str(ROOT / "code/qcommon"), str(fixture),
            str(ROOT / "code/qcommon/game_profile.cpp"), "-o", str(executable)],
            capture_output=True, text=True, timeout=60, cwd=ROOT)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return executable

    def run_probe(self, executable, *args):
        result = subprocess.run([str(executable), *map(str, args)], capture_output=True,
                                text=True, timeout=15, cwd=ROOT)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_root_modules_are_profile_specific_and_contained(self):
        (ROOT / ".tmp").mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="native-root-", dir=ROOT / ".tmp") as temp:
            folder = Path(temp)
            executable = self.build(self.compiler(), folder, PATH_CHECKS)
            root = folder / "install"
            root.mkdir()
            for name in ("cgamex86.dll", "cgame_sp_x86.dll", "cgame_mp_x86.dll", "default.cfg"):
                (root / name).write_bytes(b"synthetic module fixture")
                for profile in ("q3", "ql", "rtcw-sp", "rtcw-mp"):
                    self.run_probe(executable, root, name, profile)
            for name in ("missing.dll", "../cgamex86.dll", "cgamex86.dll/extra"):
                self.run_probe(executable, root, name, "rtcw-sp")
            (root / "cgamex86.dll").unlink()
            (root / "cgamex86.dll").mkdir()
            self.run_probe(executable, root, "cgamex86.dll", "rtcw-sp")
            (root / "cgamex86.dll").rmdir()
            self.run_probe(executable, root, "cgamex86.dll", "rtcw-sp")
            unicode_root = root / "caf\u00e9"
            unicode_root.mkdir()
            (unicode_root / "cgamex86.dll").write_bytes(b"unicode fixture")
            self.run_probe(executable, root, "cgamex86.dll", "rtcw-sp", "unicode")
            # A module symlink must not escape the chosen installation.
            outside = folder / "outside.dll"
            outside.write_bytes(b"outside")
            link = root / "cgamex86.dll"
            try:
                link.symlink_to(outside)
            except OSError:
                pass  # Windows accounts without symlink rights; Linux covers this case.
            else:
                try:
                    self.run_probe(executable, root, "cgamex86.dll", "rtcw-sp", "outside")
                finally:
                    link.unlink()
                link.symlink_to(root / "cgame_sp_x86.dll")
                try:
                    self.run_probe(executable, root, "cgamex86.dll", "rtcw-sp", "inside")
                finally:
                    link.unlink()

    def test_module_reads_keep_vfs_precedence_and_balanced_ownership(self):
        source = (ROOT / "code/qcommon/files.c").read_text(encoding="utf-8")
        body = function(source, "int FS_ReadNativeModuleFile(")
        # The production TU is C; make its allocator assignment explicit for this C++ fixture.
        body = body.replace("data = Hunk_AllocateTempMemory", "data = (byte *)Hunk_AllocateTempMemory")
        (ROOT / ".tmp").mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="native-read-", dir=ROOT / ".tmp") as temp:
            folder = Path(temp)
            module = folder / "retail.fixture"
            module.write_bytes(b"retail")
            executable = self.build(self.compiler(), folder, READ_PRELUDE + body + READ_CHECKS)
            self.run_probe(executable, module)


if __name__ == "__main__":
    unittest.main()
