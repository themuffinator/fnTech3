"""Execute production demo format selection with mixed-profile filenames."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class DemoProfileSelectionTests(unittest.TestCase):
    def test_lookup_never_opens_the_other_profiles_demo(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        source = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        def function(signature):
            start = source.index(signature)
            return source[start:source.index("\n}", start)+2]
        harness = r'''
#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
enum { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, OLD_PROTOCOL_VERSION = 68,
       QL_RETAIL_PROTOCOL_VERSION = 91 };
struct Profile { int id; } profile;
Profile *Com_GameProfile() { return &profile; }
const int demo_protocols[] = {66, 67, 68, 71, 91, 0};
bool Com_DemoProtocolSupported(int p) {
    for (int value : demo_protocols) if (value && value == p) return true;
    return false;
}
struct ScopedFileHandle {
    bool open = false;
    void reset() { open = false; }
    explicit operator bool() const { return open; }
};
std::set<std::string> files;
std::vector<std::string> attempts;
void OpenPureFileRead(const char *name, ScopedFileHandle &h) {
    attempts.emplace_back(name); h.open = files.count(name) != 0;
}
void Com_Printf(const char *, ...) {}
#define Com_sprintf snprintf
#define DEMOEXT "dm_"
#define LEGACY_DEMOEXT "dm3"
''' + function("static bool CL_DemoProtocolSupported(") + "\n" + function(
            "static bool CL_TryDemoProtocol(") + "\n" + function(
            "static int CL_WalkDemoExt(") + r'''
int main() {
    char name[256] = {};
    ScopedFileHandle handle;
    profile.id = COM_GAME_QUAKE3;
    files = {"demos/mixed.dm_91"};
    assert(CL_WalkDemoExt("mixed", name, sizeof(name), handle) == -1);
    for (const auto &p : attempts) assert(p != "demos/mixed.dm_91");
    files.insert("demos/mixed.dm_68"); files.insert("demos/mixed.dm_66");
    assert(CL_WalkDemoExt("mixed", name, sizeof(name), handle) == 66);
    assert(!CL_DemoProtocolSupported(91) && !CL_DemoProtocolSupported(999));
    profile.id = COM_GAME_QUAKELIVE;
    assert(CL_WalkDemoExt("mixed", name, sizeof(name), handle) == 91);
    files = {"demos/mixed.dm_68", "demos/mixed.dm3"}; attempts.clear();
    assert(CL_WalkDemoExt("mixed", name, sizeof(name), handle) == -1);
    assert(attempts == std::vector<std::string>{"demos/mixed.dm_91"});
    assert(!handle && !CL_DemoProtocolSupported(68));
    assert(CL_DemoProtocolSupported(91));
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp, exe = Path(tmp)/"probe.cpp", Path(tmp)/"probe.exe"
            cpp.write_text(harness, encoding="utf-8")
            subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17",
                            str(cpp), "-o", str(exe)], check=True,
                           capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
