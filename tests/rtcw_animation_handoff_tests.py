"""Execute the cgame-to-game text boundary and its actual legacy VM export."""
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
#include "code/qcommon/vm_local.h"
#include "code/game/g_public.h"
#include "code/ui/ui_public.h"
#include "code/cgame/cg_public.h"
#include "code/renderercommon/tr_types.h"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/wolfet_module_abi.h"
#include "code/compat/rtcw_key_adapter.h"
#include "code/compat/wolfet_key_adapter.h"
#include "code/compat/wolfet_menu_adapter.h"
#include "code/compat/jka_cgame_exports.h"
#include "code/compat/jka_ui_exports.h"
#include "code/server/server_rtcw_args.hpp"
#include <cassert>
#include <cstdio>
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile() { return &profile; }
int Com_GameIsWolfET(void) { return profile.id == COM_GAME_WOLFET; }
int Com_GameIsJKAMP(void) { return profile.id == COM_GAME_JKA_MP; }
int Com_GameIsJKASP(void) { return profile.id == COM_GAME_JKA_SP; }
int Com_GameIsRTCW() { return profile.id == COM_GAME_RTCW_SP || profile.id == COM_GAME_RTCW_MP; }
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
static vm_t game, client;
static vm_t *gvm = &game;
static int calls, expectedEntity;
static const char *expectedText;
static const void *sourcePointer;
static bool nested;
static intptr_t QDECL Native(int call, ...) {
    assert(call == 13 && game.callLevel == (nested ? 2 : 1));
    va_list ap; va_start(ap, call);
    const auto entity = va_arg(ap, int32_t);
    auto *text = reinterpret_cast<char *>(static_cast<uintptr_t>(static_cast<uint32_t>(va_arg(ap, int32_t))));
    for (int n = 2; n < 15; ++n) assert(va_arg(ap, int32_t) == 0);
    va_end(ap);
    assert(entity == expectedEntity && text != sourcePointer && !std::strcmp(text, expectedText));
    ++calls; ++game.syscallCount;
    if (!nested) {
        const auto saved = expectedText;
        expectedText = "nested walk 117";
        nested = true;
        SV_RTCWSendMoveSpeeds(&client, expectedEntity, reinterpret_cast<intptr_t>(expectedText));
        nested = false; expectedText = saved;
        assert(game.callLevel == 1 && game.syscallCount == 2);
        assert(!std::strcmp(text, expectedText)); // Inner call has its own copy.
    }
    text[0] = '!'; // The callee may write its copy without changing cgame memory.
    return -13579;
}
static intptr_t VM_CallNativeExports(vm_t *, int, intptr_t *) { assert(false); return 0; }
int32_t VM_CallCompiled(vm_t *, int, int32_t *) { assert(false); return 0; }
int32_t VM_CallInterpreted2(vm_t *, int, int32_t *) { assert(false); return 0; }
'''

CHECKS = r'''
template<class F> static void RejectAt(F &&action, int line) {
    const int before = calls, level = game.callLevel, counter = game.syscallCount;
    bool failed = false;
    try { action(); } catch (int n) { assert(n == 7); failed = true; }
    if (!failed || calls != before || game.callLevel != level || game.syscallCount != counter) {
        std::fprintf(stderr, "Rejection failed at fixture line %d: threw=%d calls=%d/%d level=%d/%d counter=%d/%d\n",
            line, int(failed), calls, before, game.callLevel, level, game.syscallCount, counter);
        std::abort();
    }
}
#define Reject(action) RejectAt(action, __LINE__)
static void Run(comGameId_t id) {
    profile.id = id; profile.key = "test";
    game = {}; game.name = "test"; game.index = VM_GAME; game.entryPoint = Native;
    client = {}; client.index = VM_CGAME; client.entryPoint = Native;
    gvm = &game; calls = 0;
    std::array<char, 10000> text{};
    std::memcpy(text.data(), "soldier walk 171 run 249", 24);
    expectedText = text.data(); expectedEntity = INT_MIN; sourcePointer = text.data();
    const intptr_t address = reinterpret_cast<intptr_t>(text.data());
    if (sizeof(void *) == 4) {
        SV_RTCWSendMoveSpeeds(&client, expectedEntity, address);
        assert(calls == 2 && game.callLevel == 0 && game.syscallCount == 2);
        assert(text[0] == 's');
        // Maximum terminated payload and signed entity bits are preserved.
        text.fill('x'); text.back() = 0; expectedEntity = INT_MAX;
        SV_RTCWSendMoveSpeeds(&client, expectedEntity, address);
        assert(calls == 4 && text[0] == 'x');
        text[0] = 0; SV_RTCWSendMoveSpeeds(&client, expectedEntity, address);
        assert(calls == 6 && text[0] == 0); // Parsing semantics belong to the game.
    } else {
        Reject([&] { SV_RTCWSendMoveSpeeds(&client, 0, address); });
        Reject([&] { SV_RTCWSendMoveSpeeds(&client, INTPTR_MAX, address); });
    }
    // The bounded copier itself executes on every host architecture.
    text.fill('x');
    Reject([&] { SV_RTCWString<10000>(&client, address); });
    Reject([&] { SV_RTCWString<10000>(&client, 0); });
    Reject([&] { SV_RTCWString<10000>(nullptr, address); });
    Reject([&] { SV_RTCWString<10000>(&client, -1); });
    const char one[] = "";
    assert(SV_RTCWString<10000>(&client, reinterpret_cast<intptr_t>(one))[0] == 0);
    text.back() = 0;
    assert(SV_RTCWString<10000>(&client, address) == text);
    client.dllExports = &client;
    assert(SV_RTCWString<10000>(&client, address) == text);
    client.dllExports = nullptr;
    if (sizeof(void *) == 4) {
        Reject([&] { SV_RTCWSendMoveSpeeds(nullptr, 0, address); });
        Reject([&] { SV_RTCWSendMoveSpeeds(&client, 0, 0); });
        text.back() = 'x'; Reject([&] { SV_RTCWSendMoveSpeeds(&client, 0, address); });
        text.back() = 0;
    }
    // Exact VM extents, rather than an address mask, bound a source string.
    vm_t qvm{};
    std::array<byte, 10002> data{};
    qvm.dataBase = data.data(); qvm.exactDataLength = 10001;
    std::memcpy(data.data() + 1, text.data(), text.size()); data.back() = 0xa5;
    assert(SV_RTCWString<10000>(&qvm, 1) == text && data.back() == 0xa5);
    if (sizeof(void *) == 4) {
        const int before = calls;
        SV_RTCWSendMoveSpeeds(&qvm, expectedEntity, 1);
        assert(calls == before + 2 && data[1] == 'x' && data.back() == 0xa5);
    }
    --qvm.exactDataLength; Reject([&] { SV_RTCWString<10000>(&qvm, 1); });
    qvm.exactDataLength = 2; data[1] = 0;
    assert(SV_RTCWString<10000>(&qvm, 1)[0] == 0);
    for (intptr_t bad : {intptr_t(0), intptr_t(-1), intptr_t(2), INTPTR_MAX})
        Reject([&] { SV_RTCWString<10000>(&qvm, bad); });
    qvm.exactDataLength = 0; Reject([&] { SV_RTCWString<10000>(&qvm, 1); });
    qvm.exactDataLength = 2; qvm.dataBase = nullptr;
    Reject([&] { SV_RTCWString<10000>(&qvm, 1); });
    // Never send an engine pointer to a QVM or a different module API.
    game.entryPoint = nullptr; Reject([&] { SV_RTCWSendMoveSpeeds(&client, 0, address); });
    game.entryPoint = Native; game.dllExports = &game;
    Reject([&] { SV_RTCWSendMoveSpeeds(&client, 0, address); });
    game.dllExports = nullptr; game.index = VM_CGAME;
    Reject([&] { SV_RTCWSendMoveSpeeds(&client, 0, address); });
    game.index = VM_GAME;
    // No server: even absent payload memory is not examined.
    gvm = nullptr; SV_RTCWSendMoveSpeeds(nullptr, INTPTR_MAX, 0); gvm = &game;
    for (auto other : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE}) {
        profile.id = other;
        Reject([&] { SV_RTCWSendMoveSpeeds(&client, 0, address); });
    }
}
int main() { Run(COM_GAME_RTCW_SP); Run(COM_GAME_RTCW_MP); }
'''


class RTCWAnimationHandoffTests(unittest.TestCase):
    def test_bounded_copy_native_export_recursion_and_profile_isolation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        source = (ROOT / 'code/qcommon/vm.c').read_text()
        start = source.index('#ifndef USE_DEDICATED\nstatic int VM_RTCWMenu(')
        end = source.index('\n}\n', source.index('intptr_t QDECL VM_Call(', start)) + 3
        game = (ROOT / 'code/server/sv_game.cpp').read_text()
        data = (ROOT / 'code/server/server_game_data.cpp').read_text()
        helpers = '\nnamespace fntech3::server {\n' + function(data, 'bool AddressRangeFits(')
        helpers += '\n' + function(data, 'bool ResolveQvmGameDataPointer(') + '\n}\n'
        body = PRELUDE + helpers + source[start:end] + function(game, 'void SV_RTCWSendMoveSpeeds(') + CHECKS
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-animation-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            fixture = folder / 'probe.cpp'
            fixture.write_text(body, encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT),
                '-I', str(ROOT / 'code/qcommon'),
                str(fixture), '-o', str(executable)], capture_output=True, text=True, timeout=60, cwd=ROOT)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15, cwd=ROOT)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
