"""Execute RTCW pak/byte verification before a pinned native module is loaded."""
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
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using byte = unsigned char;
using qboolean = int;
using fileHandle_t = int;
enum { qfalse, qtrue };
enum { VM_GAME, VM_CGAME, VM_UI, VM_COUNT };
using vmIndex_t = int;
#define MAX_QPATH 64
#define MAX_OSPATH 256
#define ARCH_STRING "x86"
#define DLL_EXT ".dll"
#define FS_INVALID_HANDLE 0
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile() { return &profile; }
static int Com_GameManagerPureModuleFilename(const comGameProfile_t *, unsigned slot,
    const char *, const char *, char *out, unsigned bytes) {
    assert(slot < 2 && bytes >= 17);
    const char *name = slot ? "ui_mp_x86.dll" : "cgame_mp_x86.dll";
    std::memcpy(out, name, std::strlen(name) + 1); return 1;
}
static int FS_FilenameCompare(const char *a, const char *b) {
    while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) == std::tolower(static_cast<unsigned char>(*b))) { ++a; ++b; }
    return *a != *b;
}
static long FS_HashFileName(const char *, unsigned) { return 0; }
struct fileInPack_t { const char *name; unsigned long size; const byte *bytes; fileInPack_t *next; };
struct pack_t { int referenced, exclude, hashSize = 1; bool pure = true; fileInPack_t *hashTable[1]; };
struct searchpath_t { pack_t *pack; searchpath_t *next; };
static byte payload[9000], pinnedBytes[9000];
static fileInPack_t entry, other;
static pack_t pak, lower;
static searchpath_t paths[3];
static searchpath_t *fs_searchpaths = paths;
static struct { bool zipFile; struct { struct { int z; } file; } handleFiles; } fsh[2];
static int opens, closes, reads, crcChecks, readPosition, caches, writes, loads;
static bool failOpen, failRead, failCrc;
static const fileInPack_t *opened;
static int FS_ModuleReferenceFlag(const char *);
static qboolean FS_PakIsPure(const pack_t *p) { return p->pure; }
static int FS_OpenFileInPak(fileHandle_t *file, pack_t *p, fileInPack_t *e, qboolean unique) {
    assert(unique); ++opens; p->referenced |= FS_GENERAL_REF | FS_ModuleReferenceFlag(e->name);
    if (failOpen) return -1;
    *file = 1; opened = e; readPosition = 0; fsh[1].zipFile = true; fsh[1].handleFiles.file.z = 42;
    return e->size;
}
static int FS_Read(void *out, int count, fileHandle_t file) {
    assert(file == 1 && count > 0 && count <= 4096 && readPosition + count <= opened->size);
    ++reads;
    if (failRead) return count - 1;
    std::memcpy(out, opened->bytes + readPosition, count); readPosition += count; return count;
}
static qboolean FS_ZipPayloadMatches(int archive, const byte *bytes, int length) {
    assert(archive == 42 && bytes == pinnedBytes && length == sizeof(pinnedBytes)); ++crcChecks; return !failCrc;
}
static void FS_FCloseFile(fileHandle_t file) { assert(file == 1); ++closes; }
static void Com_Printf(const char *, ...) {}
struct vmPinnedNative_t { const char *filename; const byte *bytes; int length; };
static vmPinnedNative_t vmPinnedNative[VM_COUNT];
static qboolean VM_HasPinnedNativeModule(vmIndex_t index) { return vmPinnedNative[index].bytes != nullptr; }
static const char *VM_NativeCachePath(const char *, char *out, size_t) { ++caches; out[0] = 0; return out; }
static qboolean VM_WriteNativeCacheFile(const char *, const void *bytes, int length) {
    assert(bytes == pinnedBytes && length == sizeof(pinnedBytes)); ++writes; return true;
}
static void *Sys_LoadLibrary(const char *) { ++loads; return &pak; }
static int Q_stricmp(const char *a, const char *b) { return FS_FilenameCompare(a,b); }
'''

CHECKS = r'''
static void Reset() {
    profile.pureProof = COM_PURE_PROOF_RTCW_NATIVE;
    for (unsigned i = 0; i < sizeof(payload); ++i) payload[i] = byte(i * 37);
    std::memcpy(pinnedBytes, payload, sizeof(payload));
    entry = {"cgame_mp_x86.dll", sizeof(payload), payload, nullptr};
    other = entry;
    pak = {}; pak.hashSize = 1; pak.pure = true; pak.hashTable[0] = &entry;
    lower = pak; lower.hashTable[0] = &other;
    // A directory between paks can never satisfy this proof.
    paths[0] = {&pak, paths+1}; paths[1] = {nullptr, paths+2}; paths[2] = {&lower, nullptr};
    for (auto &p : vmPinnedNative) p = {entry.name, pinnedBytes, int(sizeof(pinnedBytes))};
    opens = closes = reads = crcChecks = caches = writes = loads = 0;
    failOpen = failRead = failCrc = false;
}
static qboolean Verify(const char *name = "cgame_mp_x86.dll") {
    return FS_VerifyNativeModulePak(name, pinnedBytes, sizeof(pinnedBytes));
}
int main() {
    Reset();
    assert(Verify() && opens == 1 && closes == 1 && reads == 3 && crcChecks == 1);
    assert(pak.referenced == (FS_GENERAL_REF | FS_CGAME_REF) && lower.referenced == 0);
    for (int slot : {VM_CGAME, VM_UI}) {
        Reset();
        entry.name = slot == VM_CGAME ? "cgame_mp_x86.dll" : "ui_mp_x86.dll";
        vmPinnedNative[slot].filename = entry.name;
        assert(VM_LoadPinnedDll(slot, entry.name) == &pak);
        assert(opens == 1 && closes == 1 && caches == 1 && writes == 1 && loads == 1);
        assert(pak.referenced == (FS_GENERAL_REF | (slot == VM_CGAME ? FS_CGAME_REF : FS_UI_REF)));
        pak.referenced = 0;
        assert(VM_LoadPinnedDll(slot, entry.name) == &pak && pak.referenced != 0);
    }
    Reset(); assert(Verify("CGAME_MP_X86.DLL"));
    for (int fault = 0; fault < 9; ++fault) {
        Reset(); pak.referenced = 0x80;
        if (fault == 0) failOpen = true;
        if (fault == 1) failRead = true;
        if (fault == 2) failCrc = true;
        if (fault == 3) --entry.size;
        if (fault == 4) pinnedBytes[8192] ^= 1;
        if (fault == 5) { pak.pure = lower.pure = false; }
        if (fault == 6) { pak.exclude = lower.exclude = true; }
        if (fault == 7) { pak.hashTable[0] = lower.hashTable[0] = nullptr; }
        if (fault == 8) entry.size = ~0UL;
        assert(!VM_LoadPinnedDll(VM_CGAME, "cgame_mp_x86.dll"));
        assert(!caches && !writes && !loads && pak.referenced == 0x80 && lower.referenced == 0);
        assert(closes == (fault == 1 || fault == 2 || fault == 4 ? 1 : 0));
    }
    Reset(); pak.pure = false; assert(Verify() && !pak.referenced && lower.referenced);
    Reset(); pak.exclude = true; assert(Verify() && !pak.referenced && lower.referenced);
    Reset();
    fileInPack_t unrelated = {"other", 1, payload, &entry}; pak.hashTable[0] = &unrelated;
    assert(Verify());
    Reset();
    assert(!FS_VerifyNativeModulePak(nullptr,pinnedBytes,sizeof(pinnedBytes)));
    assert(!FS_VerifyNativeModulePak(entry.name,nullptr,sizeof(pinnedBytes)));
    assert(!FS_VerifyNativeModulePak(entry.name,pinnedBytes,0));
    assert(!FS_VerifyNativeModulePak(entry.name,pinnedBytes,-1));
    assert(!FS_VerifyNativeModulePak(entry.name,reinterpret_cast<void *>(UINTPTR_MAX),9));
    for (const char *name : {"vm/cgame.qvm", "cgamex86.dll", "qagame_mp_x86.dll", "other/cgame_mp_x86.dll", "../cgame_mp_x86.dll"})
        assert(!Verify(name));
    assert(!opens && !pak.referenced);
    assert(!VM_LoadPinnedDll(VM_CGAME, "different.dll") && !caches);
    vmPinnedNative[VM_CGAME].bytes = nullptr;
    assert(!VM_LoadPinnedDll(VM_CGAME, entry.name) && !caches);
    for (auto policy : {COM_PURE_PROOF_QVM, COM_PURE_PROOF_QL_NATIVE, COM_PURE_PROOF_UNAVAILABLE}) {
        Reset(); profile.pureProof = policy; failRead = true;
        assert(!Verify() && !opens);
        // Other profiles retain their existing pinned-module behavior.
        assert(VM_LoadPinnedDll(VM_CGAME, entry.name) == &pak && !opens && loads == 1);
        assert(FS_ModuleReferenceFlag("vm/cgame.qvm") == FS_CGAME_REF);
        assert(FS_ModuleReferenceFlag("vm/ui.qvm") == FS_UI_REF);
        assert(FS_ModuleReferenceFlag("cgame_mp_x86.dll") == 0);
    }
}
'''


class RTCWPureModuleTests(unittest.TestCase):
    def test_pak_precedence_bytes_crc_references_and_pinned_load_order(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        files = (ROOT / 'code/qcommon/files.c').read_text()
        vm = (ROOT / 'code/qcommon/vm.c').read_text()
        header = (ROOT / 'code/qcommon/qcommon.h').read_text()
        constants = '\n'.join(line for line in header.splitlines() if line.startswith('#define')
                              and any('FS_' + flag + '_REF' in line for flag in ('GENERAL', 'UI', 'CGAME')))
        body = constants + PRELUDE + function(files, 'static int FS_ModuleReferenceFlag(')
        body += function(files, 'qboolean FS_VerifyNativeModulePak(')
        body += function(vm, 'static void *VM_LoadPinnedDll(') + CHECKS
        with tempfile.TemporaryDirectory(prefix='rtcw-pure-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            fixture = folder / 'probe.cpp'
            fixture.write_text(body, encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                str(fixture), '-o', str(executable)], capture_output=True, text=True, timeout=60, cwd=ROOT)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15, cwd=ROOT)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
