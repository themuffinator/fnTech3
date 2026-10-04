"""Retail JK2 1.02a function-table validation and partial-startup ownership."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

CONTRACT = r'''
#include "code/compat/jk2_sp_game_api.h"
#include "code/compat/jk2_sp_memory.hpp"
#include <cassert>
#include <cstring>
#include <initializer_list>
using namespace fntech3::jk2::sp;
static void Noop() {}
static void* Fail(std::size_t) { return nullptr; }
int main() {
    assert(ValidateExports(nullptr) == ExportError::MissingTable);
    fntech3_jk2_sp_game_export_t g{};
    g.apiversion = 10;
    assert(ValidateExports(&g) == ExportError::Version); // OpenJK is a different API.
    g.apiversion = 6;
    assert(ValidateExports(&g) == ExportError::MissingFunction);
    // Populate every function slot, leaving the data fields zero.
    void (*entry)() = Noop;
    for (std::size_t n = 0; n < 15; ++n)
        std::memcpy(reinterpret_cast<char*>(&g.Init) + n * sizeof(entry), &entry, sizeof(entry));
    g.gentitySize = 1288;
    assert(ValidateExports(&g) == ExportError::None);
    for (std::size_t n = 0; n < 15; ++n) {
        auto missing = g;
        void (*nullEntry)() = nullptr;
        std::memcpy(reinterpret_cast<char*>(&missing.Init) + n * sizeof(entry), &nullEntry, sizeof(entry));
        assert(ValidateExports(&missing) == ExportError::MissingFunction);
    }
    g.gentitySize = 828; // MP's stride never validates SP.
    assert(ValidateExports(&g) == ExportError::EntityStride);
    g.gentitySize = 1288; g.num_entities = 1;
    assert(ValidateExports(&g) == ExportError::EntityCount);
    g.gentities = &g;
    for (int bad : {-1, 1025}) {
        g.num_entities = bad;
        assert(ValidateExports(&g) == ExportError::EntityCount);
    }
    g.num_entities = 1024;
    assert(ValidateExports(&g) == ExportError::None);

    AllocationPool pool;
    assert(!pool.Allocate(-1, 11, true));
    auto* zero = static_cast<unsigned char*>(pool.Allocate(71, 11, true));
    assert(zero && pool.Count() == 1);
    for (int n = 0; n < 71; ++n) assert(!zero[n]);
    assert(reinterpret_cast<std::uintptr_t>(zero) % alignof(std::max_align_t) == 0);
    void* other = pool.Allocate(19, 32, false);
    assert(other && pool.Count() == 2);
    assert(!pool.Free(zero + 1) && pool.Count() == 2); // interior pointer
    int foreign = 0;
    assert(!pool.Free(&foreign) && pool.Count() == 2);
    assert(pool.Free(zero) && pool.Count() == 1);
    assert(!pool.Free(zero) && pool.Count() == 1); // repeated free
    assert(pool.Free(nullptr) && pool.Count() == 1);
    pool.Reset(); pool.Reset(); assert(pool.Count() == 0);
    assert(pool.Allocate(0, 11, true) && pool.Count() == 1);
    AllocationPool failed(Fail, std::free);
    assert(!failed.Allocate(4, 11, true) && failed.Count() == 0);
}
'''

HOST = r'''
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#define QDECL
enum { ERR_DROP, ERR_DISCONNECT, ERR_NEED_CD, ERR_FATAL, H_QAGAME,
    GAME_SHUTDOWN, GAME_INIT, GAME_RUN_FRAME, VM_GAME };
enum fsMode_t { FS_READ };
using fileHandle_t = int;
using vmMainFunc_t = intptr_t(*)(int,...);
using dllEntry_t = void(*)(intptr_t(*)(intptr_t,...));
struct vm_t { void* dllHandle; };
static vm_t *gvm;
static bool com_errorEntered;
static constexpr int MAXPRINTMSG=4096, MAX_STRING_CHARS=1024, MAX_QPATH=64;
struct cvar_t {
    char *name, *string, *resetString, *latchedString;
    int flags, modified, modificationCount; float value; int integer; cvar_t* next;
};
struct { const char* entityParsePoint; } sv;
struct Drop { std::string text; };
static int filesClosed, initCalls, shutdownCalls, attached, vmFreed;
static std::string printed;
static void Com_Error(int,const char* fmt,...) {
    char buffer[4096]; va_list a; va_start(a,fmt); vsnprintf(buffer,sizeof(buffer),fmt,a); va_end(a);
    throw Drop{buffer};
}
static void Com_Printf(const char* fmt,...) {
    char buffer[4096]; va_list a; va_start(a,fmt); vsnprintf(buffer,sizeof(buffer),fmt,a); va_end(a);
    printed += buffer;
}
static void Q_vsnprintf(char* b, std::size_t n, const char* f, va_list a) { vsnprintf(b,n,f,a); }
static void FS_VM_CloseFiles(int) { ++filesClosed; }
static int Sys_Milliseconds() { return 777; }
static int Com_Milliseconds() { return 888; }
static cvar_t* Cvar_Get(const char*,const char*,int) { static cvar_t c{}; return &c; }
static void Cvar_Set(const char*,const char*) {}
static int Cvar_VariableIntegerValue(const char*) { return 123; }
static void Cvar_VariableStringBuffer(const char* name,char* b,int n) {
    std::snprintf(b,n,"%s", !std::strcmp(name,"mapname") ? "kejim_post" : "");
}
static int Cmd_Argc() { return 0; }
static const char* Cmd_Argv(int) { return ""; }
static int FS_VM_OpenFile(const char*,fileHandle_t* f,fsMode_t,int) { if(f)*f=0; return -1; }
static int FS_VM_ReadFile(void*,int,int,int) { return -1; }
static int FS_VM_WriteFile(void*,int,int,int) { return -1; }
static void FS_VM_CloseFile(int,int) {}
static int FS_ReadFile(const char*,void** b) { if(b)*b=nullptr; return -1; }
static void FS_FreeFile(void*) {}
static int FS_GetFileList(const char*,const char*,char*,int) { return 0; }
static void Cbuf_AddText(const char*) {}
static void SV_GameDropClient(int,const char*) {}
static void SV_GameSendServerCommand(int,const char*) {}
static void SV_SetConfigstring(int,const char*) {}
static void SV_GetConfigstring(int,char*,int) {}
static void SV_GetUserinfo(int,char*,int) {}
static void SV_SetUserinfo(int,const char*) {}
static void SV_GetServerinfo(char*,int) {}
static int SV_inPVS(const float*,const float*) { return 1; }
static int SV_inPVSIgnorePortals(const float*,const float*) { return 1; }
static int CM_AreasConnected(int,int) { return 1; }
static vm_t* VM_CreateTable(int,const char*,vmMainFunc_t,void**);
static void VM_Free(vm_t*) { ++vmFreed; }
static void* Sys_LoadFunction(void*,const char*);
#include "code/server/server_jk2_sp_game.hpp"
static void FakeInit(const char* map,const char* target,int checksum,const char* entities,
    int time,int seed,int global,int saved,int transition) {
    assert(!std::strcmp(map,"kejim_post") && !*target && checksum==123);
    assert(!std::strcmp(entities,"{}") && time==1000 && seed==53 && global==888);
    assert(!saved && !transition); ++initCalls;
    assert(SV_JK2SPMalloc(17,11,1));
    // Exercise refusal with a malformed native pointer without dereferencing it.
    auto callback = reinterpret_cast<void(*)(void*,const char*)>(jk2SpImports.SetBrushModel);
    callback(reinterpret_cast<void*>(1),"*1");
}
static void FakeShutdown() { ++shutdownCalls; }
static fntech3_jk2_sp_game_export_t* FakeGetAPI(fntech3_jk2_sp_game_import_t* imports) {
    assert(imports && imports->Malloc && imports->Free && imports->VoiceVolume);
    assert(imports->FS_ReadFile && imports->FS_FreeFile);
    for(auto entry : imports->unserved) assert(entry);
    static fntech3_jk2_sp_game_export_t game{};
    void (*function)() = FakeShutdown;
    for(std::size_t n=0;n<15;++n)
        std::memcpy(reinterpret_cast<char*>(&game.Init)+n*sizeof(function),&function,sizeof(function));
    game.apiversion=6; game.Init=FakeInit; game.Shutdown=FakeShutdown; game.gentitySize=1288;
    return &game;
}
static vm_t* VM_CreateTable(int,const char* name,vmMainFunc_t,void** symbol) {
    assert(!std::strcmp(name,"GetGameAPI"));
    static vm_t vm{}; *symbol=reinterpret_cast<void*>(FakeGetAPI); return &vm;
}
static void FakeDllEntry(intptr_t(*)(intptr_t,...)) { ++attached; }
static void* Sys_LoadFunction(void*,const char* name) {
    return !std::strcmp(name,"dllEntry") ? reinterpret_cast<void*>(FakeDllEntry) : reinterpret_cast<void*>(FakeShutdown);
}
int main() {
    SV_JK2SPFillImports();
    SV_JK2SPPrintf("literal %s", "%n"); assert(printed=="literal %n");
    assert(SV_JK2SPMalloc(17,11,1));
    assert(jk2SpMemory.Count()==1);
    try { jk2SpImports.unserved[0](); assert(false); }
    catch(const Drop& drop) { assert(drop.text.find("42 (+0xa8)")!=std::string::npos); }
    assert(jk2SpMemory.Count()==0 && !jk2SpGame && !jk2SpInitComplete && filesClosed==1);
    // Repeated cleanup is safe; unowned frees cannot poison the allocation list.
    SV_JK2SPReset();
    assert(SV_JK2SPMalloc(3,32,0));
    int foreign;
    try { SV_JK2SPFree(&foreign); assert(false); } catch(const Drop&) {}
    assert(jk2SpMemory.Count()==0);
    try { SV_JK2SPMalloc(-1,11,0); assert(false); } catch(const Drop&) {}
    assert(jk2SpMemory.Count()==0);
    SV_JK2SPGameEntry(GAME_SHUTDOWN); assert(shutdownCalls==0);
    jk2SpGame=FakeGetAPI(&jk2SpImports); sv.entityParsePoint="{}";
    try { SV_JK2SPGameEntry(GAME_INIT,1000,53,0); assert(false); }
    catch(const Drop& drop) { assert(drop.text.find("28 (+0x70)")!=std::string::npos); }
    assert(initCalls==1 && jk2SpMemory.Count()==0 && !jk2SpGame && !jk2SpInitComplete);
    com_errorEntered=true;
    SV_JK2SPGameEntry(GAME_SHUTDOWN); assert(shutdownCalls==0);
    com_errorEntered=false;
#if defined(_WIN32) && UINTPTR_MAX == UINT32_MAX
    assert(SV_JK2SPLoadGame() && attached==1 && gvm);
    assert(jk2SpGame && printed.find("imports 412 bytes")!=std::string::npos);
    SV_JK2SPGameEntry(GAME_SHUTDOWN); assert(shutdownCalls==0);
#else
    try { SV_JK2SPLoadGame(); assert(false); }
    catch(const Drop& drop) { assert(drop.text.find("Windows x86")!=std::string::npos); }
#endif
}
'''


def build_and_run(test, source):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler unavailable')
    target = compiler_target_args(compiler)
    temp_root = ROOT / '.tmp' / 'jk2-sp-host-tests'
    temp_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=temp_root) as folder:
        cpp = Path(folder) / 'probe.cpp'
        binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                                '-I', str(ROOT), str(cpp), '-o', str(binary)],
                               cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True,
                             timeout=30, **options)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


class JK2SPHostTests(unittest.TestCase):
    def test_frozen_contract_and_memory_ownership(self):
        build_and_run(self, CONTRACT)

    def test_partial_startup_and_unserved_boundaries(self):
        build_and_run(self, HOST)

    def test_sp_state_roundtrip_and_owner_containment(self):
        source = '#include "' + (ROOT / 'tests/jk2_sp_state_tests.cpp').as_posix() + '"\n'
        compiler = shutil.which('clang++') or shutil.which('g++')
        if os.name == 'nt' and compiler and not compiler_target_args(compiler):
            source = '#define FNTECH3_Q3_ONLY\n' + source
        build_and_run(self, source)


if __name__ == '__main__':
    unittest.main()
