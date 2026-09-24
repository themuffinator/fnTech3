"""Exercise SP's opaque, game-owned animation pointer through the native VM boundary."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_animation_handoff_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/vm_local.h"
#include "code/game/g_public.h"
#include "code/ui/ui_public.h"
#include "code/cgame/cg_public.h"
#include "code/renderercommon/tr_types.h"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/wolfet_module_abi.h"
#include "code/compat/rtcw_game_abi.h"
#include "code/compat/rtcw_key_adapter.h"
#include "code/compat/wolfet_key_adapter.h"
#include "code/compat/wolfet_menu_adapter.h"
#include "code/server/server_rtcw_args.hpp"
#include <cassert>
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile() { return &profile; }
int Com_GameIsWolfET(void) { return profile.id == COM_GAME_WOLFET; }
int Com_GameIsRTCW() { return profile.id == COM_GAME_RTCW_SP || profile.id == COM_GAME_RTCW_MP; }
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
static vm_t game, client;
static vm_t *gvm = &game;
struct GameOwned { const char *name; GameOwned *next; int value; };
static GameOwned records[2] = {{"first", &records[1], 17}, {"second", &records[0], 23}};
static int calls, expectedClient, resultCode = 1;
static const char *expectedName;
static void *expectedBefore;
static const void *sourceName, *sourceOutput;
static bool nested, preserveOutput;
static intptr_t QDECL Native(int call, ...) {
    assert(call == 14 && game.callLevel == (nested ? 2 : 1));
    va_list ap; va_start(ap, call);
    const int number = va_arg(ap, int32_t);
    auto *name = reinterpret_cast<char *>(static_cast<uintptr_t>(static_cast<uint32_t>(va_arg(ap, int32_t))));
    auto **output = reinterpret_cast<void **>(static_cast<uintptr_t>(static_cast<uint32_t>(va_arg(ap, int32_t))));
    for (int n = 3; n < 15; ++n) assert(va_arg(ap, int32_t) == 0);
    va_end(ap);
    assert(number == expectedClient && name != sourceName && output != sourceOutput);
    assert(!std::strcmp(name, expectedName) && *output == expectedBefore);
    assert(reinterpret_cast<uintptr_t>(output) % alignof(void *) == 0);
    ++calls; ++game.syscallCount;
    if (!nested) {
        const char *savedName = expectedName;
        void *savedBefore = expectedBefore;
        void *inner = &records[1];
        expectedName = "nested"; expectedBefore = inner; nested = true;
        assert(SV_RTCWGetModelInfo(&client, number, reinterpret_cast<intptr_t>(expectedName),
                                 reinterpret_cast<intptr_t>(&inner)) == resultCode);
        assert(inner == (preserveOutput ? &records[1] : &records[0]));
        nested = false; expectedName = savedName; expectedBefore = savedBefore;
        assert(game.callLevel == 1 && game.syscallCount == 2);
        assert(!std::strcmp(name, expectedName) && *output == expectedBefore);
    }
    name[0] = '!'; // Callback writes cannot alter cgame's name buffer.
    if (!preserveOutput) *output = &records[0];
    return resultCode;
}
static intptr_t VM_CallNativeExports(vm_t *, int, intptr_t *) { assert(false); return 0; }
int32_t VM_CallCompiled(vm_t *, int, int32_t *) { assert(false); return 0; }
int32_t VM_CallInterpreted2(vm_t *, int, int32_t *) { assert(false); return 0; }
'''

CHECKS = r'''
template<class F> static void Reject(F &&action) {
    const int before = calls, level = game.callLevel, counter = game.syscallCount;
    bool failed = false;
    try { action(); } catch (int n) { assert(n == 7); failed = true; }
    assert(failed && calls == before && game.callLevel == level && game.syscallCount == counter);
}
int main() {
    profile.id = COM_GAME_RTCW_SP; profile.key = "test";
    game.name = "test"; game.index = VM_GAME; game.entryPoint = Native;
    client.name = "test"; client.index = VM_CGAME; client.entryPoint = Native;
    std::array<char, 64> name{}; std::memcpy(name.data(), "bj2", 4);
    std::array<byte, sizeof(void *) + 2> output{};
    void *initial = &records[1];
    auto resetOutput = [&] { output.fill(0xa5); std::memcpy(output.data()+1, &initial, sizeof(initial)); };
    auto readOutput = [&] { void *p; std::memcpy(&p, output.data()+1, sizeof(p)); return p; };
    resetOutput();
    const intptr_t input = reinterpret_cast<intptr_t>(name.data());
    const intptr_t address = reinterpret_cast<intptr_t>(output.data()+1);
    sourceName = name.data(); sourceOutput = output.data()+1;
    expectedName = name.data(); expectedBefore = initial;
    if (sizeof(void *) == 4) {
        for (int number : {0, 1, 127}) for (int value : {0, 1, INT_MIN, INT_MAX}) {
            expectedClient = number; resultCode = value;
            for (bool preserve : {false, true}) {
                preserveOutput = preserve; resetOutput(); int before = calls;
                assert(SV_RTCWGetModelInfo(&client, number, input, address) == value);
                assert(calls == before+2 && game.callLevel == 0 && game.syscallCount == 2);
                assert(readOutput() == (preserve ? &records[1] : &records[0]));
                auto *record = static_cast<GameOwned *>(readOutput());
                assert(record->next->next == record && record->value == (preserve ? 23 : 17));
                assert(name[0] == 'b' && output.front() == 0xa5 && output.back() == 0xa5);
            }
        }
        preserveOutput = false; resultCode = 1; expectedClient = 0;
        name.fill('x'); name.back() = 0; resetOutput();
        SV_RTCWGetModelInfo(&client, 0, input, address); assert(name[0] == 'x');
        name[0] = 0; resetOutput(); SV_RTCWGetModelInfo(&client, 0, input, address); assert(name[0] == 0);
        // All invalid requests preserve both buffers and avoid calling the game.
        resetOutput(); auto saved = output;
        for (intptr_t bad : {intptr_t(-1), intptr_t(128), intptr_t(INT_MIN), INTPTR_MAX})
            Reject([&] { SV_RTCWGetModelInfo(&client, bad, input, address); });
        for (intptr_t bad : {intptr_t(0), intptr_t(-1)}) {
            Reject([&] { SV_RTCWGetModelInfo(&client, 0, bad, address); });
            Reject([&] { SV_RTCWGetModelInfo(&client, 0, input, bad); });
        }
        name.fill('x'); Reject([&] { SV_RTCWGetModelInfo(&client, 0, input, address); });
        assert(name.front() == 'x' && name.back() == 'x' && saved == output);
        name.back() = 0;
    } else {
        // Original pointer slots are explicitly x86; no truncation on x64.
        Reject([&] { SV_RTCWGetModelInfo(&client, 0, input, address); });
    }
    for (auto id : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_MP}) {
        profile.id = id; Reject([&] { SV_RTCWGetModelInfo(&client, 0, input, address); });
    }
    profile.id = COM_GAME_RTCW_SP;
    for (vm_t *vm : {&game, &client}) {
        vm->entryPoint = nullptr; Reject([&] { SV_RTCWGetModelInfo(&client, 0, input, address); });
        vm->entryPoint = Native; vm->dllExports = vm;
        Reject([&] { SV_RTCWGetModelInfo(&client, 0, input, address); }); vm->dllExports = nullptr;
        auto index = vm->index; vm->index = VM_UI;
        Reject([&] { SV_RTCWGetModelInfo(&client, 0, input, address); }); vm->index = index;
    }
    Reject([&] { SV_RTCWGetModelInfo(nullptr, 0, input, address); });
    // An absent local game is a false result without touching caller memory.
    gvm = nullptr; auto saved = output;
    assert(!SV_RTCWGetModelInfo(nullptr, INTPTR_MAX, 0, 0) && output == saved);
}
'''


class RTCWModelInfoTests(unittest.TestCase):
    def test_native_pointer_ownership_recursion_bounds_and_profile_isolation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        vm = (ROOT / 'code/qcommon/vm.c').read_text()
        start = vm.index('#ifndef USE_DEDICATED\nstatic int VM_RTCWMenu(')
        end = vm.index('\n}\n', vm.index('intptr_t QDECL VM_Call(', start)) + 3
        game = (ROOT / 'code/server/sv_game.cpp').read_text()
        data = (ROOT / 'code/server/server_game_data.cpp').read_text()
        helpers = '\nnamespace fntech3::server {\n' + function(data, 'bool AddressRangeFits(')
        helpers += '\n' + function(data, 'bool ResolveQvmGameDataPointer(') + '\n}\n'
        body = PRELUDE + helpers + vm[start:end] + function(game, 'intptr_t SV_RTCWGetModelInfo(') + CHECKS
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-model-info-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); fixture = folder / 'probe.cpp'
            fixture.write_text(body, encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT),
                '-I', str(ROOT / 'code/qcommon'), str(fixture), '-o', str(executable)],
                capture_output=True, text=True, timeout=60, cwd=ROOT)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15, cwd=ROOT)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
