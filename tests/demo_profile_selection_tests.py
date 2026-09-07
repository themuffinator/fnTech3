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
#include <array>
#include <charconv>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
enum { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, OLD_PROTOCOL_VERSION = 68,
       QL_RETAIL_PROTOCOL_VERSION = 91 };
struct Profile { int id; const char *displayName = "fixture"; } profile;
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

#define MAX_OSPATH 256
#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))
#define S_COLOR_YELLOW ""
using qboolean = int;
constexpr int qfalse = 0;
int ToQboolean(bool b) { return b; }
int Q_stricmpn(const char *a, const char *b, int n) {
    for (int i=0; i<n; ++i) {
        int x=std::tolower(static_cast<unsigned char>(a[i]));
        int y=std::tolower(static_cast<unsigned char>(b[i]));
        if (x!=y || !x || !y) return x-y;
    }
    return 0;
}
int Q_stricmp(const char *a,const char *b) { return Q_stricmpn(a,b,256); }
void Q_strncpyz(char *out,const char *in,int size) { snprintf(out,size,"%s",in); }
const char *COM_GetExtension(const char *p) {
    auto dot=std::strrchr(p,'.'); return dot ? dot+1 : "";
}
std::string argument;
int transitions=0;
int Cmd_Argc() { return 2; }
const char *Cmd_Argv(int) { return argument.c_str(); }

''' + function("static bool CL_DemoProtocolSupported(") + "\n" + function(
            "static bool CL_TryDemoProtocol(") + "\n" + function(
            "static int CL_WalkDemoExt(") + "\n" + function(
            "static qboolean CL_IsLegacyDemoExt(") + "\n" + function(
            "static bool CL_ParseDemoProtocolExtension(") + "\n" + function(
            "static qboolean CL_DemoNameCallback_f(") + "\n" + source[
                source.index("static void CL_PlayDemo_f("):source.index(
                    "\n\thFile.reset();", source.index("static void CL_PlayDemo_f("))
            ] + "\n++transitions;\n}\n" + r'''
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
    // Explicit cross-profile files exist, with a same-name matching fallback.
    files = {"demos/mixed.dm_91", "demos/mixed.dm_68", "demos/mixed.dm3"};
    attempts.clear(); argument="mixed.dm_68";
    CL_PlayDemo_f(); assert(transitions==0 && attempts.empty());
    argument="mixed.dm3";
    CL_PlayDemo_f(); assert(transitions==0 && attempts.empty());
    profile.id=COM_GAME_QUAKE3; argument="mixed.dm_91";
    CL_PlayDemo_f(); assert(transitions==0 && attempts.empty());
    argument="mixed.dm_68";
    CL_PlayDemo_f(); assert(transitions==1 && attempts.size()==1);
    assert(CL_DemoNameCallback_f("x.DM3",5));
    assert(CL_DemoNameCallback_f("x.DM_68",7));
    assert(!CL_DemoNameCallback_f("x.dm_91",7));
    profile.id=COM_GAME_QUAKELIVE;
    assert(!CL_DemoNameCallback_f("x.DM3",5));
    assert(!CL_DemoNameCallback_f("x.dm_68",7));
    assert(CL_DemoNameCallback_f("x.DM_91",7));
    for (const char *bad : {"x.dm_91junk", "x.dm_-1", "x.dm_2147483648", "x.dm_"})
        assert(!CL_DemoNameCallback_f(bad, static_cast<int>(std::strlen(bad))));

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
