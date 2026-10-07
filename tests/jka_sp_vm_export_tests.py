"""Run production VM_Call against a counted native SP cgame entry point."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <climits>
#define QDECL
#define MAX_VMMAIN_CALL_ARGS 16
#define NO_VM_COMPILED
#define id386 0
enum { ERR_DROP, ERR_FATAL, VM_GAME,
#ifndef USE_DEDICATED
    VM_CGAME, VM_UI,
#endif
    GAME_SNAPSHOT_CALLBACK=100, GAME_RETRIEVE_MOVESPEEDS_FROM_CLIENT,
    GAME_GETMODELINFO, AICAST_VISIBLEFROMPOS, AICAST_CHECKATTACKATPOS,
    CG_GET_TAG=200, CG_EVENT_HANDLING, UI_SET_ACTIVE_MENU, UI_GET_ACTIVE_MENU,
    CG_RAVEN_MAP_CHANGE };
using vmIndex_t=int;
using comGameId_t=int;
using qboolean=int;
using Entry=intptr_t (*)(int,...);
struct vm_t { vmIndex_t index; Entry entryPoint; void *dllExports;
    int callLevel, syscallCount, compiled; const char *name; };
struct Profile { comGameId_t id; const char *key; };
static Profile profile{1,"jka-sp"};
static int calls;
static char errorText[1024];
struct Drop {};
void Com_Error(int,const char *format,...) {
    va_list args;va_start(args,format);std::vsnprintf(errorText,sizeof(errorText),format,args);va_end(args);
    throw Drop{};
}
void Com_Printf(const char *,...) {}
const Profile *Com_GameProfile() { return &profile; }
bool Com_GameIsJKASP() { return profile.id==1; }
bool Com_GameIsJKAMP() { return profile.id==2; }
bool Com_GameIsRTCW() { return false; }
bool Com_GameIsWolfET() { return false; }
int FNTech3_JKACgameExportOrdinal(int command) { return command; }
int FNTech3_JKACgameExportArgumentCount(int) { return 0; }
int FNTech3_JKACgameEventToExternal(int command) { return command; }
int FNTech3_JKAUIExportOrdinal(int command) { return command; }
int FNTech3_JKAUIExportArgumentCount(int) { return 0; }
int FNTech3_JKAUIMenuToExternal(int command) { return command; }
int FNTech3_JKAUIMenuToInternal(int command) { return command; }
int FnTech3_WolfETMenuToInternal(intptr_t command) { return int(command); }
int VM_RTCWMenuFromExternal(comGameId_t,intptr_t command) { return int(command); }
qboolean VM_PrepareRTCWExport(comGameId_t,vmIndex_t,int,int,int32_t *,int *) { assert(false);return 0; }
qboolean VM_PrepareWolfETExport(vmIndex_t,int,int,int32_t *,int *) { assert(false);return 0; }
intptr_t VM_CallNativeExports(vm_t *,int,const intptr_t *) { assert(false);return 0; }
int32_t VM_CallInterpreted2(vm_t *,int,int32_t *) { assert(false);return 0; }
intptr_t Native(int command,...) { ++calls;return command+10; }
'''

CHECKS = r'''
#ifdef USE_DEDICATED
int main() {
    vm_t vm{VM_GAME,Native,nullptr,0,321,0,"jagame"};
    assert(VM_Call(&vm,0,8)==18 && calls==1 && vm.callLevel==0);
}
#else
int main() {
    vm_t vm{VM_CGAME,Native,nullptr,0,321,0,"jagame"};
    // Covers all known SP cgame exports, specifically the DLL's STL resize
    // exports7..11, plus unknown ordinals. No entry or counters may change.
    for(int command:{0,1,2,3,4,5,6,7,8,9,10,11,INT_MIN,INT_MAX}) {
        bool rejected=false;
        try { VM_Call(&vm,0,command); } catch(const Drop &) {
            rejected=true;assert(std::strstr(errorText,"single-player cgame export"));
        }
        assert(rejected && calls==0 && vm.callLevel==0 && vm.syscallCount==321);
    }
    vm.dllExports=&vm;
    try { VM_Call(&vm,0,8);assert(false); } catch(const Drop &) {}
    vm.dllExports=nullptr;
    for(int count:{-1,16}) {
        try { VM_Call(&vm,count,8);assert(false); } catch(const Drop &) {
            assert(std::strstr(errorText,"invalid argument count"));
        }
    }
    // The API-8 game host's own dispatcher and existing other profiles remain
    // callable. The test is not evidence for the unrelated mapping stubs.
    vm.index=VM_GAME;assert(VM_Call(&vm,0,8)==18 && calls==1);
    vm.index=VM_CGAME;
    for(int id:{2,3,4}) { profile.id=id;assert(VM_Call(&vm,0,0)==10); }
    assert(calls==4 && vm.callLevel==0);
}
#endif
'''


class SPExportTests(unittest.TestCase):
    def test_native_cgame_exports_refuse_before_module_entry(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8')
        start = source.index('intptr_t QDECL VM_Call( ')
        body = source[start:source.index('\n}', start) + 2]
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='jka-sp-vm-', dir=scratch) as folder:
            path = Path(folder)
            fixture = path / 'guard.cpp'
            fixture.write_text('#include <initializer_list>\n' + PRELUDE + body + CHECKS, encoding='utf-8')
            for dedicated in (False, True):
                with self.subTest(dedicated=dedicated):
                    # One executable per build: Windows can hold the last one run.
                    name = 'guard-dedicated' if dedicated else 'guard'
                    executable = path / (name + '.exe' if os.name == 'nt' else name)
                    flags = compiler_target_args(compiler)
                    if dedicated:
                        flags.append('-DUSE_DEDICATED')
                    build = subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        *flags, str(fixture), '-o', str(executable)], text=True, capture_output=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    result = subprocess.run([str(executable)], text=True, capture_output=True, timeout=15)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
