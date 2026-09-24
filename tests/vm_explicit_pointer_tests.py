"""Link separate UI/cgame callers to the actual shared renderer pointer helpers."""
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


class VMExplicitPointerTests(unittest.TestCase):
    def test_linked_callers_use_the_supplied_vm_for_qvm_and_native_data(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        vm = (ROOT / 'code/qcommon/vm.c').read_text()
        render = (ROOT / 'code/client/client_rtcw_render_compat.hpp').read_text()
        common = r'''
#include "code/qcommon/vm_local.h"
#include "code/compat/render_record_adapter.hpp"
#include <array>
#include <cassert>
#include <cstring>
struct Result { int model, width; const char *name; };
Result CallerA(vm_t *, intptr_t, intptr_t, intptr_t);
Result CallerB(vm_t *, intptr_t, intptr_t, intptr_t);
void ImplicitA(vm_t *);
void ImplicitB(vm_t *);
'''
        caller = common + r'''
static vm_t *implicitVM;
// This intentionally differs between translation units. Shared inline helpers
// must never bind to this UI/cgame-local resolver when the linker folds them.
static void *VM_ArgPtr(intptr_t address) { return VM_ExplicitArgPtr(implicitVM, address); }
#include "code/client/client_render_compat.hpp"
''' + function(render, 'inline const char *CL_RTCWRenderName(') + r'''
void ImplicitSUFFIX(vm_t *vm) { implicitVM = vm; }
Result CallerSUFFIX(vm_t *vm, intptr_t entity, intptr_t view, intptr_t name) {
    return {CL_ModuleRefEntity(vm, entity).hModel, CL_ModuleRefdef(vm, view).width,
            CL_RTCWRenderName(vm, name)};
}
'''
        main = common + r'''
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile() { return &profile; }
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
''' + function(vm, 'void *VM_ExplicitArgPtr(') + '\n' + function(vm, 'void VM_CheckBounds(') + r'''
static void Fill(byte *data, int model, int width, const char *name) {
    fntech3_q3_refEntity_t entity{}; entity.hModel = model;
    fntech3_q3_refdef_t view{}; view.width = width;
    std::memcpy(data + 32, &entity, sizeof(entity));
    std::memcpy(data + 512, &view, sizeof(view)); std::strcpy(reinterpret_cast<char *>(data + 1024), name);
}
int main() {
    profile.id = COM_GAME_QUAKE3;
    vm_t a{}, b{}; std::array<byte, 2048> adata{}, bdata{};
    a.dataBase = adata.data(); b.dataBase = bdata.data(); a.dataMask = b.dataMask = 2047;
    Fill(a.dataBase, 11, 640, "cgame"); Fill(b.dataBase, 22, 800, "ui");
    ImplicitA(&a); ImplicitB(&b);
    for (auto caller : {CallerA, CallerB}) for (auto current : {&a, &b}) {
        auto result = caller(current, 32, 512, 1024);
        assert(result.model == (current == &a ? 11 : 22));
        assert(result.width == (current == &a ? 640 : 800));
        assert(!std::strcmp(result.name, current == &a ? "cgame" : "ui"));
    }
    assert(VM_ExplicitArgPtr(&a, 4096 + 32) == a.dataBase + 32);
    assert(VM_ExplicitArgPtr(&a, -1) == a.dataBase + a.dataMask);
    assert(!VM_ExplicitArgPtr(&a, 0) && !VM_ExplicitArgPtr(nullptr, 1));
    vm_t empty{}; assert(!VM_ExplicitArgPtr(&empty, 1));
    bool rejected = false; try { CallerB(&b, 2000, 512, 1024); } catch (int e) { rejected = e == 7; }
    assert(rejected);
    // UI can run while cgame is absent. Native pointers are never masked using
    // another module's data segment, for either native ABI style.
    ImplicitA(nullptr); ImplicitB(nullptr);
    for (bool structured : {false,true}) {
        a.entryPoint = structured ? nullptr : reinterpret_cast<vmMainFunc_t>(1);
        a.dllExports = structured ? &a : nullptr;
        for (auto caller : {CallerA, CallerB}) {
            const auto result = caller(&a, reinterpret_cast<intptr_t>(b.dataBase + 32),
                reinterpret_cast<intptr_t>(b.dataBase + 512), reinterpret_cast<intptr_t>(b.dataBase + 1024));
            assert(result.model == 22 && result.width == 800 && !std::strcmp(result.name, "ui"));
        }
        assert(VM_ExplicitArgPtr(&a, INTPTR_MIN) == reinterpret_cast<void *>(INTPTR_MIN));
        assert(VM_ExplicitArgPtr(&a, INTPTR_MAX) == reinterpret_cast<void *>(INTPTR_MAX));
        assert(!VM_ExplicitArgPtr(&a, 0));
    }
}
'''
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='vm-explicit-', dir=ROOT / '.tmp') as temporary:
            folder = Path(temporary)
            sources = [folder / name for name in ['a.cpp', 'b.cpp', 'main.cpp']]
            for path, text in zip(sources, [caller.replace('SUFFIX', 'A'), caller.replace('SUFFIX', 'B'), main]):
                path.write_text(text)
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            flags = compiler_target_args(compiler)
            if os.name == 'nt' and not flags:
                flags += ['-DFNTECH3_Q3_ONLY']
            build = subprocess.run([compiler, '-std=c++17', '-O0', '-fno-inline', *flags,
                '-I', str(ROOT), *map(str, sources), '-o', str(executable)], cwd=ROOT,
                capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
