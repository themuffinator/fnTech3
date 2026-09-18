"""Execute production demo format selection with mixed-profile filenames."""
from pathlib import Path
import re
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
        # Use the production table so its order and membership cannot drift.
        common = (ROOT / "code/qcommon/common.c").read_text(encoding="utf-8")
        table = re.search(r"const int demo_protocols\[\] = \{([^}]*)\};", common, re.S)
        demo_protocols = ", ".join(entry.strip() for entry in
            re.sub(r"/\*.*?\*/", "", table.group(1), flags=re.S).split(",") if entry.strip())
        self.assertEqual(demo_protocols, "66, 67, OLD_PROTOCOL_VERSION, NEW_PROTOCOL_VERSION, "
            "QL_RETAIL_PROTOCOL_VERSION, NETCHAN_RTCW_SP_PROTOCOL_VERSION, "
            "NETCHAN_RTCW_MP_PROTOCOL_VERSION, 0")
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
enum { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP,
       COM_GAME_WOLFET, OLD_PROTOCOL_VERSION = 68, NEW_PROTOCOL_VERSION = 71,
       QL_RETAIL_PROTOCOL_VERSION = 91, NETCHAN_RTCW_SP_PROTOCOL_VERSION = 49,
       NETCHAN_RTCW_MP_PROTOCOL_VERSION = 60 };
struct Profile { int id; const char *displayName = "fixture"; } profile;
Profile *Com_GameProfile() { return &profile; }
int Com_GameIsRTCWSP() { return profile.id == COM_GAME_RTCW_SP; }
const int demo_protocols[] = {''' + demo_protocols + r'''};
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

    // RTCW SP and MP each own one protocol and never reach a Q3/QL codec or
    // the Q3-only dm3 fallback; Q3 never opens an RTCW demo.
    const std::vector<std::string> everything = {"demos/mixed.dm_66", "demos/mixed.dm_67",
        "demos/mixed.dm_68", "demos/mixed.dm_71", "demos/mixed.dm_91",
        "demos/mixed.dm_49", "demos/mixed.dm_60", "demos/mixed.dm3"};
    for (int id : {COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        profile.id = id;
        const int own = id == COM_GAME_RTCW_SP ? 49 : 60, other = id == COM_GAME_RTCW_SP ? 60 : 49;
        files = std::set<std::string>(everything.begin(), everything.end()); attempts.clear();
        assert(CL_WalkDemoExt("mixed", name, sizeof(name), handle) == own);
        assert(attempts == std::vector<std::string>{"demos/mixed.dm_" + std::to_string(own)});
        files.erase("demos/mixed.dm_" + std::to_string(own)); attempts.clear();
        assert(CL_WalkDemoExt("mixed", name, sizeof(name), handle) == -1 && !handle);
        assert(attempts == std::vector<std::string>{"demos/mixed.dm_" + std::to_string(own)});
        for (int protocol : {66, 67, 68, 71, 91, other})
            assert(!CL_DemoProtocolSupported(protocol));
        assert(CL_DemoProtocolSupported(own));
        assert(!CL_DemoNameCallback_f("x.dm3", 5) && !CL_DemoNameCallback_f("x.dm_68", 7));
        assert(CL_DemoNameCallback_f(own == 49 ? "x.DM_49" : "x.DM_60", 7));
        for (const char *cross : {"mixed.dm_68", "mixed.dm_91", "mixed.dm3",
                                  own == 49 ? "mixed.dm_60" : "mixed.dm_49"}) {
            argument = cross; attempts.clear();
            CL_PlayDemo_f(); assert(transitions == 1 && attempts.empty());
        }
        argument = own == 49 ? "mixed.dm_49" : "mixed.dm_60"; files.insert("demos/" + argument);
        attempts.clear(); CL_PlayDemo_f();
        assert(attempts == std::vector<std::string>{"demos/" + argument});
        // Retail SP cannot replay SP demos (its cgame needs a running game
        // module), so the SP profile refuses after finding the file; MP plays.
        assert(transitions == (id == COM_GAME_RTCW_SP ? 1 : 2));
        transitions = 1;
    }
    profile.id = COM_GAME_QUAKE3;
    files = std::set<std::string>(everything.begin(), everything.end());
    files.erase("demos/mixed.dm_66"); files.erase("demos/mixed.dm_67");
    files.erase("demos/mixed.dm_68"); files.erase("demos/mixed.dm_71"); attempts.clear();
    assert(CL_WalkDemoExt("mixed", name, sizeof(name), handle) == 68);
    for (const auto &p : attempts) assert(p != "demos/mixed.dm_49" && p != "demos/mixed.dm_60");
    assert(!CL_DemoProtocolSupported(49) && !CL_DemoProtocolSupported(60));
    for (const char *cross : {"mixed.dm_49", "mixed.dm_60"}) {
        argument = cross; attempts.clear();
        CL_PlayDemo_f(); assert(transitions == 1 && attempts.empty());
    }
    // ET demo playback is unimplemented: nothing is admitted.
    profile.id = COM_GAME_WOLFET;
    for (int protocol : {49, 60, 66, 67, 68, 71, 91}) assert(!CL_DemoProtocolSupported(protocol));
    files = std::set<std::string>(everything.begin(), everything.end()); attempts.clear();
    assert(CL_WalkDemoExt("mixed", name, sizeof(name), handle) == -1 && attempts.empty());
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
