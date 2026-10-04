"""Execute Academy import 605 with the real model store and VM bounds checks."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def source():
    host = (ROOT / 'code/server/server_jka_game.hpp').read_text()
    start = host.index('\tcase Call::G_G2_CLEANMODELS:')
    end = host.index('\n\t/* --- navigation', start)
    vm = (ROOT / 'code/qcommon/vm.c').read_text()
    return r'''
#define main RavenStoreFixtureMain
#include "tests/raven_ghoul2_server_tests.cpp"
#undef main
#include "code/compat/jka_module_abi.h"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <thread>
using Call = fntech3::jka::module::Call;
using fntech3::jka::module::Lookup;
using fntech3::jka::module::Module;
using fntech3::jka::module::Direction;
struct vm_t { void *entryPoint; void *dllExports; unsigned char *dataBase; unsigned dataMask; };
constexpr int ERR_DROP = 1;
static void RaiseDrop(const char *message) { throw std::runtime_error(message); }
static void Com_Error(int, const char *format, ...) {
 char message[256]; va_list arguments; va_start(arguments, format);
 std::vsnprintf(message, sizeof(message), format, arguments); va_end(arguments);
 void (*volatile drop)(const char *) = RaiseDrop; drop(message);
}
#define VM_CHECKBOUNDS VM_CheckBounds
''' + function(vm, 'void *VM_ExplicitArgPtr(') + '\n' + function(
        vm, 'void VM_CheckBounds(') + r'''
static TestFiles modelFiles = Installed();
static ghoul2::Store ravenGhoul2(modelFiles);
static vm_t *gvm;
static bool dispatch(comGameId_t profile, const intptr_t *args, intptr_t &result) {
 const Call call = Lookup(profile, Module::GAME, Direction::IMPORT, args[0]);
 result = 0;
 auto pointer = [&](int index) { return VM_ExplicitArgPtr(gvm, args[index]); };
 switch(call) {
''' + host[start:end] + r'''
 default: return false;
 }
}
template<class Action> static void rejected(Action action, const char *reason) {
 bool dropped = false;
 try { action(); } catch(const std::runtime_error &error) {
  dropped = true; CHECK(std::strstr(error.what(), reason));
 }
 CHECK(dropped);
}
static void checkDispatch(int mode) {
 ravenGhoul2.Clear();
 std::array<unsigned char,256> memory; memory.fill(0xa5);
 vm_t vm{}; vm.dataBase=memory.data(); vm.dataMask=255;
 if(mode==1) vm.entryPoint=reinterpret_cast<void *>(std::uintptr_t(1));
 if(mode==2) vm.dllExports=reinterpret_cast<void *>(std::uintptr_t(1));
 gvm=&vm;
 auto address = [&](int at) -> intptr_t {
  return mode ? reinterpret_cast<intptr_t>(memory.data()+at) : at;
 };
 auto invoke = [&](intptr_t selected, comGameId_t profile=COM_GAME_JKA_MP) {
  intptr_t args[]={605,selected}; intptr_t result=123;
  bool served=dispatch(profile,args,result); CHECK(result==0); return served;
 };
 constexpr int slotAt=3;
 void *handle=nullptr, *sibling=nullptr;
 LoadError error=LoadError::None;
 CHECK(ravenGhoul2.AddModel(&handle,"models/players/test/model.glm",error)==0);
 CHECK(ravenGhoul2.SetSkin(handle,0,11) && ravenGhoul2.AddBolt(handle,0,"root")==0);
 CHECK(ravenGhoul2.Duplicate(handle,&sibling));
 const ghoul2::Model *held=ravenGhoul2.Resolve(sibling)->At(0);
 const Bytes mesh=*held->meshBytes, skeleton=*held->skeletonBytes;
 CHECK(ravenGhoul2.Attach(handle,4) && ravenGhoul2.Attach(handle,8));
 CHECK(ravenGhoul2.Attach(sibling,9));
 std::memcpy(memory.data()+slotAt,&handle,sizeof(handle));
 auto before=memory;
 // Other profiles never resolve this operation, including the SP table ABI.
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_JKA_SP,COM_GAME_JK2_MP}) {
  CHECK(!invoke(address(slotAt),profile)); CHECK(memory==before);
 }
 CHECK(invoke(address(slotAt)));
 void *cleared=handle; std::memcpy(&cleared,memory.data()+slotAt,sizeof(cleared));
 CHECK(!cleared); std::memcpy(before.data()+slotAt,&cleared,sizeof(cleared));
 CHECK(memory==before && !ravenGhoul2.Resolve(handle));
 CHECK(!ravenGhoul2.Attached(4) && !ravenGhoul2.Attached(8));
 CHECK(ravenGhoul2.AttachmentCount()==1 && ravenGhoul2.Attached(9)==ravenGhoul2.Resolve(sibling));
 CHECK(held==ravenGhoul2.Resolve(sibling)->At(0) && held->customSkin==11 && held->bolts.size()==1);
 CHECK(*held->meshBytes==mesh && *held->skeletonBytes==skeleton);
 CHECK(invoke(address(slotAt)) && memory==before); // already empty, no model allocation
 CHECK(ravenGhoul2.InstanceCount()==1);
 auto unchanged = [&](auto attempt,const char *reason) {
  auto canaries=memory; auto count=ravenGhoul2.InstanceCount();
  auto attachments=ravenGhoul2.AttachmentCount(); rejected(attempt,reason);
  CHECK(canaries==memory && count==ravenGhoul2.InstanceCount());
  CHECK(attachments==ravenGhoul2.AttachmentCount());
 };
 unchanged([&]{invoke(0);},"G_G2_CLEANMODELS: null model pointer slot");
 std::memcpy(memory.data()+slotAt,&handle,sizeof(handle));
 unchanged([&]{invoke(address(slotAt));},"G_G2_CLEANMODELS: stale or foreign model handle");
 ghoul2::Store other(modelFiles); void *foreign=nullptr;
 CHECK(other.AddModel(&foreign,"models/players/test/model.glm",error)==0);
 std::memcpy(memory.data()+slotAt,&foreign,sizeof(foreign));
 unchanged([&]{invoke(address(slotAt));},"G_G2_CLEANMODELS: stale or foreign model handle");
 CHECK(other.Resolve(foreign));
 if(!mode) {
  for(int bad:{-1,252,255,256,INT_MAX})
   unchanged([&]{invoke(bad);},"data segment bounds");
  // Last slot allowed by the real VM bounds contract; preserve its last-byte canary.
  const int last=int(vm.dataMask-sizeof(void *));
  std::memcpy(memory.data()+last,&sibling,sizeof(sibling)); auto canaries=memory;
  CHECK(invoke(address(last))); void *empty=nullptr;
  std::memcpy(canaries.data()+last,&empty,sizeof(empty)); CHECK(canaries==memory);
 } else {
  std::memcpy(memory.data()+slotAt,&sibling,sizeof(sibling)); CHECK(invoke(address(slotAt)));
 }
 CHECK(!ravenGhoul2.InstanceCount() && !ravenGhoul2.AttachmentCount());
 ravenGhoul2.Clear(); ravenGhoul2.Clear();
 std::memcpy(memory.data()+slotAt,&sibling,sizeof(sibling));
 unchanged([&]{invoke(address(slotAt));},"G_G2_CLEANMODELS: stale or foreign model handle");
}
static void checkConcurrentIdentities() {
 // Only identity allocation is shared between concurrently owned Stores.
 constexpr int threads=4, count=1000;
 std::array<void *,threads*count> identities{};
 std::array<std::thread,threads> workers;
 for(int worker=0;worker<threads;++worker) workers[worker]=std::thread([&,worker]{
  for(int i=0;i<count;++i) identities[worker*count+i]=ghoul2::ModelHandleIdentities.Allocate();
 });
 for(auto &worker:workers) worker.join();
 std::set<void *> unique(identities.begin(),identities.end());
 CHECK(unique.size()==identities.size() && !unique.count(nullptr));
 ghoul2::OpaqueHandleSequence last(std::numeric_limits<std::uintptr_t>::max());
 for(int worker=0;worker<threads;++worker) workers[worker]=std::thread([&,worker]{
  identities[worker]=last.Allocate();
 });
 for(auto &worker:workers) worker.join();
 int issued=0; for(int worker=0;worker<threads;++worker) issued+=identities[worker]!=nullptr;
 CHECK(issued==1 && !last.Allocate());
}
int main() {
 CHECK(RavenStoreFixtureMain()==0);
 static_assert(FNTECH3_JKA_G_G2_CLEANMODELS==605,"retail cleanup ordinal");
 checkDispatch(0); checkDispatch(1); checkDispatch(2); checkConcurrentIdentities();
 std::cout << "Jedi Academy cleanup import 605 tests passed\n";
}
'''


class JKAGhoul2CleanupTests(unittest.TestCase):
    def test_owned_slot_bounds_attachments_identity_lifetime_and_profile_isolation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/jka-compat/cleanup-tests'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary).resolve()
            cpp = folder / 'probe.cpp'
            cpp.write_text(source(), encoding='utf-8')
            for fast_math in (False, True):
                with self.subTest(fast_math=fast_math):
                    exe = folder / ('fast.exe' if fast_math else 'default.exe')
                    flags = ['-ffast-math'] if fast_math else []
                    if os.name != 'nt':
                        flags += ['-pthread']
                    built = subprocess.run([
                        compiler, *compiler_target_args(compiler), '-std=c++17', '-O2',
                        '-Wall', '-Wextra', '-Werror', *flags, '-I', str(ROOT),
                        str(cpp), '-o', str(exe)], cwd=folder, capture_output=True,
                        text=True, timeout=90)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    run = subprocess.run([str(exe)], cwd=folder, capture_output=True,
                                         text=True, timeout=20)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    self.assertIn('cleanup import 605 tests passed', run.stdout)


if __name__ == '__main__':
    unittest.main()
