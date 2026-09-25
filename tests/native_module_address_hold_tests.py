"""Keep a 32-bit client's native module reloadable across a restart.

Enemy Territory's cgame image is 57 MB. A renderer restart unloads it, then
allocates (the renderer, the UI module) before loading it again; in a 32-bit
process the hole it left was often the only one large enough, and once the
restart's allocations split it the reload failed ("VM_Create on cgame
failed"). The engine now keeps the range reserved from the unload until the
slot loads again (Sys_UnloadLibraryHeld, Sys_ReleaseLibraryHold, vm.c).

The Windows test loads a real DLL with a 48 MB image; the bookkeeping test
compiles vm.c's own VM_Free and VM_ReleaseLibraryHold against a stub platform.
"""
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


WINDOWS_HARNESS = r'''
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
'''

WINDOWS_CHECKS = r'''
static SIZE_T ImageSize(HMODULE module) {
 auto base = reinterpret_cast<const BYTE *>(module);
 auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
 auto nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
 return nt->OptionalHeader.SizeOfImage;
}
static bool Loaded(const wchar_t *name) { return GetModuleHandleW(name) != nullptr; }
int wmain(int argc, wchar_t **argv) {
 assert(argc == 2);
 const wchar_t *dll = argv[1];
 const wchar_t *name = wcsrchr(dll, L'\\') + 1;
 assert(!Sys_UnloadLibraryHeld(nullptr));
 Sys_ReleaseLibraryHold(nullptr);

 HMODULE module = LoadLibraryW(dll);
 assert(module);
 const SIZE_T size = ImageSize(module);
 assert(size >= (48u << 20));
 void *hold = Sys_UnloadLibraryHeld(module);
 assert(!Loaded(name));
 if (sizeof(void *) == 8) {
  // a 64-bit process has the space and holds nothing
  assert(!hold);
 } else {
  // the whole image range stays reserved, and nothing else can take it
  assert(hold == static_cast<void *>(module));
  MEMORY_BASIC_INFORMATION info{};
  assert(VirtualQuery(hold, &info, sizeof(info)) == sizeof(info));
  assert(info.State == MEM_RESERVE && info.AllocationBase == hold && info.RegionSize >= size);
  assert(!VirtualAlloc(static_cast<BYTE *>(hold) + (size / 2 & ~static_cast<SIZE_T>(0xffff)),
                       0x10000, MEM_RESERVE, PAGE_NOACCESS));
  Sys_ReleaseLibraryHold(hold);
  assert(VirtualQuery(hold, &info, sizeof(info)) == sizeof(info) && info.State == MEM_FREE);
 }

 // the slot's module loads again once the range is released
 module = LoadLibraryW(dll);
 assert(module && GetProcAddress(module, "FnTech3HeldProbe"));

 // an image another reference still maps is not unloaded, so nothing is held
 HMODULE second = LoadLibraryW(dll);
 assert(second == module);
 assert(!Sys_UnloadLibraryHeld(second));
 assert(Loaded(name));
 FreeLibrary(module);
 assert(!Loaded(name));
 std::puts("held");
 return 0;
}
'''

FIXTURE_DLL = r'''
// a retail cgame's image is mostly zero-filled data
static char image[48u << 20];
extern "C" __declspec(dllexport) int FnTech3HeldProbe(int index) {
 image[index] = 1;
 return image[index];
}
'''

BOOKKEEPING = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define VM_COUNT 3
#define ERR_FATAL 0
#define Com_Memset memset
typedef int vmIndex_t;
typedef struct vm_s vm_t;
struct vm_s {
 int callLevel;
 void (*destroy)(vm_t *vm);
 void *dllHandle;
 vmIndex_t index;
 const char *name;
};
static struct vm_s vmTable[VM_COUNT];
static int forced_unload;
static void *released[8], *plainUnloads[8];
static int releases, unloadsHeld, unloads, holdFails;
static void Com_Error(int level, const char *format, ...) { (void)level; (void)format; assert(0); }
static void Com_Printf(const char *format, ...) { (void)format; }
static void *Sys_UnloadLibraryHeld(void *handle) { ++unloadsHeld; return holdFails ? NULL : handle; }
static void Sys_ReleaseLibraryHold(void *hold) { assert(hold); released[releases++] = hold; }
static void Sys_UnloadLibrary(void *handle) { plainUnloads[unloads++] = handle; }
'''

BOOKKEEPING_CHECKS = r'''
int main(void) {
 int first, second, third;
 vm_t *cgame = &vmTable[1];

 // an unloaded module's range is held for its slot
 cgame->dllHandle = &first; cgame->index = 1; cgame->name = "cgame";
 VM_Free(cgame);
 assert(unloadsHeld == 1 && vmLibraryHold[1] == &first && !cgame->dllHandle && !releases);

 // a slot never holds two ranges: the older one is released first
 cgame->dllHandle = &second; cgame->index = 1;
 VM_Free(cgame);
 assert(releases == 1 && released[0] == &first && vmLibraryHold[1] == &second);

 // loading into the slot releases what it holds, once
 VM_ReleaseLibraryHold(1);
 assert(releases == 2 && released[1] == &second && !vmLibraryHold[1]);
 VM_ReleaseLibraryHold(1);
 VM_ReleaseLibraryHold(-1);
 VM_ReleaseLibraryHold(VM_COUNT);
 assert(releases == 2);

 // a hold that could not be taken leaves the slot empty
 holdFails = 1;
 cgame->dllHandle = &third; cgame->index = 1;
 VM_Free(cgame);
 assert(unloadsHeld == 3 && !vmLibraryHold[1] && releases == 2);
 holdFails = 0;

 // a VM without a library holds nothing; nor does one outside the table's slots
 vmTable[2].index = 2;
 VM_Free(&vmTable[2]);
 assert(!vmLibraryHold[2] && unloadsHeld == 3);
 vmTable[0].dllHandle = &first; vmTable[0].index = VM_COUNT;
 VM_Free(&vmTable[0]);
 assert(unloads == 1 && plainUnloads[0] == &first);
 return 0;
}
'''


class NativeModuleAddressHoldTests(unittest.TestCase):
    def test_every_native_load_releases_its_slots_hold_first(self):
        vm = (ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8')
        native = function(vm, 'vm_t *VM_CreateNative( vmIndex_t index')
        release = native.index('VM_ReleaseLibraryHold( index );')
        self.assertLess(native.index('return vm;\n\t}'), release)  # a VM already loaded keeps its hold
        self.assertLess(release, native.index('Loading dll file %s.'))
        table = function(vm, 'vm_t *VM_CreateTable( vmIndex_t index')
        self.assertLess(table.index('VM_ReleaseLibraryHold( index );'),
                        table.index('VM_OpenNativeModule( index, filename'))
        unload = function(vm, 'void VM_Free( vm_t *vm )')
        self.assertIn('vmLibraryHold[ vm->index ] = Sys_UnloadLibraryHeld( vm->dllHandle );', unload)
        # dlopen has no counterpart; the other platform holds nothing
        unix = (ROOT / 'code/unix/unix_shared.cpp').read_text(encoding='utf-8')
        self.assertIn('Sys_UnloadLibrary( handle );\n\treturn NULL;',
                      function(unix, 'void *Sys_UnloadLibraryHeld( void *handle )'))

    def test_vm_free_holds_one_range_per_slot(self):
        compiler = next((path for name in ('clang', 'gcc', 'cc') if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest('requires a C compiler (clang, gcc, or cc)')
        vm = (ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8')
        start = vm.index('static void *vmLibraryHold[ VM_COUNT ];')
        holds = vm[start:vm.index('\n}', start) + 2]
        source = BOOKKEEPING + holds + '\n' + function(vm, 'void VM_Free( vm_t *vm )') + BOOKKEEPING_CHECKS
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='module-hold-', dir=scratch) as folder:
            path = Path(folder)
            fixture = path / 'holds.c'
            executable = path / ('holds.exe' if os.name == 'nt' else 'holds')
            fixture.write_text(source, encoding='utf-8')
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c11', '-Wall', '-Wextra',
                                    '-Werror', str(fixture), '-o', str(executable)],
                                   text=True, capture_output=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(executable)], text=True, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    @unittest.skipUnless(os.name == 'nt', 'Win32 address-space reservation')
    def test_an_unloaded_modules_image_range_is_held_until_released(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        main = (ROOT / 'code/win32/win_main.cpp').read_text(encoding='utf-8')
        source = WINDOWS_HARNESS
        for signature in ('void Sys_UnloadLibrary( void *handle )', 'void *Sys_UnloadLibraryHeld( void *handle )',
                          'void Sys_ReleaseLibraryHold( void *hold )'):
            source += '\n' + function(main, signature)
        source += WINDOWS_CHECKS
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='module-hold-', dir=scratch) as folder:
            root = Path(folder)
            (root / 'held.cpp').write_text(source, encoding='utf-8')
            (root / 'fixture.cpp').write_text(FIXTURE_DLL, encoding='utf-8')
            target = compiler_target_args(compiler)
            for command in ([compiler, *target, '-shared', 'fixture.cpp', '-o', 'fixture_held.dll'],
                            [compiler, *target, '-std=c++17', 'held.cpp', '-o', 'held.exe']):
                built = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(root / 'held.exe'), str(root / 'fixture_held.dll')], cwd=root,
                                 capture_output=True, text=True, timeout=30,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertIn('held', ran.stdout)


if __name__ == '__main__':
    unittest.main()
